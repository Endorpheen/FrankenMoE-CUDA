#include <stdint.h>
#include <string.h>
#ifndef FGOOSE_PACKED
#define FGOOSE_PACKED 1
#endif
#if !defined(_MSC_VER) && (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
#define GG_PACKED_X86 1
#include <immintrin.h>
#ifndef FGOOSE_SIMD_HOOK
#define FGOOSE_SIMD_HOOK(id) ((void)0)
#endif
static int gg_packed_available(int need_fma) {
#ifdef FGOOSE_SIMD_CPU_DISABLE
    return 0;
#endif
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") && (!need_fma || __builtin_cpu_supports("fma"));
}
// Q4_K x8 helpers: the packed kernel reads the same little-endian words and
// converts f16 with the same bit arithmetic as the Goose source.
static float gg_q4k_bits_f32(uint32_t bits) {
    float value;
    memcpy(&value, &bits, 4);
    return value;
}
static uint32_t gg_q4k_u32(const uint8_t *d, int64_t off) {
    return (uint32_t)d[off] | ((uint32_t)d[off + 1] << 8) | ((uint32_t)d[off + 2] << 16) |
           ((uint32_t)d[off + 3] << 24);
}
static float gg_q4k_f32(const uint8_t *d) {
    return gg_q4k_bits_f32(gg_q4k_u32(d, 0));
}
static float gg_q4k_f16(const uint8_t *d, int off) {
    uint32_t half = (uint32_t)d[off] | ((uint32_t)d[off + 1] << 8);
    uint32_t sign = (half & 32768u) << 16;
    uint32_t exponent = (half >> 10) & 31u;
    uint32_t fraction = half & 1023u;
    if (exponent == 31u) {
        return gg_q4k_bits_f32(sign | 2139095040u | (fraction << 13));
    }
    if (exponent == 0u) {
        if (fraction == 0u) {
            return gg_q4k_bits_f32(sign);
        }
        int32_t power = -14;
        while ((fraction & 1024u) == 0u) {
            fraction <<= 1;
            power -= 1;
        }
        fraction &= 1023u;
        return gg_q4k_bits_f32(sign | (((uint32_t)(power + 127)) << 23) | (fraction << 13));
    }
    return gg_q4k_bits_f32(sign | ((exponent + 112u) << 23) | (fraction << 13));
}
#else
#define GG_PACKED_X86 0
#endif
static float gs_simd_f32_from_bits(uint32_t bits) { float value; memcpy(&value, &bits, 4); return value; }
static double gs_simd_f64_from_bits(uint64_t bits) { double value; memcpy(&value, &bits, 8); return value; }
static uint32_t gs_simd_f32_to_bits(float value) { uint32_t bits; memcpy(&bits, &value, 4); return bits; }
static uint64_t gs_simd_f64_to_bits(double value) { uint64_t bits; memcpy(&bits, &value, 8); return bits; }
static uint64_t gs_simd_cpu_features(void) {
    uint64_t features = 1;
#if GG_PACKED_X86
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx2")) features |= 2;
    if (__builtin_cpu_supports("fma")) features |= 4;
    if (__builtin_cpu_supports("avx512f")) features |= 8;
    if (__builtin_cpu_supports("avx512vnni")) features |= 16;
#endif
    return features;
}
#if defined(_MSC_VER)
#define GG_ALIGN1
#if defined(_M_IX86)
#define GG_UNALIGNED
#else
#define GG_UNALIGNED __unaligned
#endif
#else
#define GG_UNALIGNED
#define GG_ALIGN1 __attribute__((aligned(1)))
#endif
#if defined(__OPTIMIZE__) && (defined(__GNUC__) || defined(__clang__))
#define GG_INLINE __attribute__((always_inline)) inline
#else
#define GG_INLINE inline
#endif
/* Fried Goose self-hosted C backend. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#ifdef _MSC_VER
#include <malloc.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
typedef struct { void *data; int64_t len; } gg_slice;
// A language aggregate may place a grow header at any byte offset.
#pragma pack(push, 1)
typedef struct { unsigned char *base; int64_t len; } gg_header;
#pragma pack(pop)
typedef struct { unsigned char *base; size_t top, committed; } gg_stack;
typedef struct { gg_header *header; gg_stack *stack; } gg_reference;
typedef struct { void *header; gg_stack *stack; } gg_record_result;
#ifndef GG_STACK_RESERVE
#define GG_STACK_RESERVE 268435456ULL
#endif
#if GG_STACK_RESERVE < 65536 || GG_STACK_RESERVE % 65536 != 0
#error GG_STACK_RESERVE_must_be_a_positive_multiple_of_65536
#endif
typedef char gg_reservation_size_fits[(GG_STACK_RESERVE <= SIZE_MAX - 65536) ? 1 : -1];
#define GG_RESERVATION ((size_t)GG_STACK_RESERVE)
#define GG_STACK_COUNT 8192
static gg_stack gg_stacks[GG_STACK_COUNT];
static int gg_argc;
static char **gg_argv;
static void gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static gg_stack *gg_get_stack(int64_t id) {
    gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) gg_fail("data stack limit exceeded");
    s = &gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) gg_fail("cannot reserve data stack");
    }
    return s;
}
/* The stack owning an array region, found from the header's data base.
   The header may be an unaligned field of a packed struct, so the base is
   read by memcpy and never through a gg_header lvalue. */
static gg_stack *gg_stack_of(const void *header) {
    unsigned char *base; uintptr_t q; int64_t i;
    memcpy(&base, header, sizeof base); q = (uintptr_t)base;
    for (i = 0; i < GG_STACK_COUNT; i++) {
        gg_stack *s = &gg_stacks[i];
        if (s->base && q >= (uintptr_t)s->base && q < (uintptr_t)s->base + GG_RESERVATION) return s;
    }
    gg_fail("array data is not in a data stack"); return 0;
}
/* A pool-relative reference narrowed to a smaller offset width: refuse a
   value that would not survive the store instead of truncating it. */
static uint64_t gg_ref_offset_fits(uint64_t v, uint64_t max) {
    if (v > max) gg_fail("relative reference offset overflow");
    return v;
}
/* ULEB128 length prefix of a serialization image (spec 3.6). */
static int64_t gg_uleb(uint8_t *b, uint64_t n) {
    int64_t k = 0;
    do { uint8_t c = (uint8_t)(n & 0x7f); n >>= 7; if (n) c |= 0x80; b[k++] = c; } while (n);
    return k;
}
/* Bounded ULEB128 read (spec 3.6): 1 and the value, or 0 when the bytes
   are truncated or past 64 bits. */
/* Owned images have stable addresses. Regions reclaim their allocations
   at exit; publishing into an older owner promotes the same block, so
   legal interior borrows do not become stale. Unscoped callers retain
   the former process-lifetime behavior through the permanent root. */
typedef struct gg_owned_scope gg_owned_scope;
typedef struct gg_owned_block gg_owned_block;
typedef struct gg_owned_root gg_owned_root;
struct gg_owned_root {
    const void *address; size_t bytes; gg_owned_scope *owner;
    gg_owned_root *previous, *next, *region_previous, *region_next;
    gg_owned_scope *lifetime; unsigned bound;
};
struct gg_owned_scope {
    gg_owned_scope *parent; gg_owned_root *roots;
    gg_owned_block *blocks; const void *thread; unsigned used, kind;
};
struct gg_owned_block {
    void *data; size_t bytes; gg_owned_scope *owner;
    gg_owned_block *previous, *next, *region_previous, *region_next;
};
static gg_owned_scope gg_owned_permanent;
static gg_owned_block *gg_owned_blocks;
static size_t gg_owned_count;
static int gg_owned_registered;
#ifdef _MSC_VER
static __declspec(thread) gg_owned_scope *gg_owned_current;
static __declspec(thread) gg_owned_root *gg_owned_roots;
static volatile LONG gg_owned_guard;
static void gg_owned_lock(void) {
    while (InterlockedCompareExchange(&gg_owned_guard, 1, 0)) Sleep(0);
}
static void gg_owned_unlock(void) { InterlockedExchange(&gg_owned_guard, 0); }
#else
#include <stdatomic.h>
static _Thread_local gg_owned_scope *gg_owned_current;
static _Thread_local gg_owned_root *gg_owned_roots;
static atomic_flag gg_owned_guard = ATOMIC_FLAG_INIT;
static void gg_owned_lock(void) {
    while (atomic_flag_test_and_set_explicit(&gg_owned_guard, memory_order_acquire)) {}
}
static void gg_owned_unlock(void) {
    atomic_flag_clear_explicit(&gg_owned_guard, memory_order_release);
}
#endif
static gg_owned_scope *gg_owned_active(void) {
    return gg_owned_current ? gg_owned_current : &gg_owned_permanent;
}
static void gg_owned_enter(gg_owned_scope *scope) {
    scope->parent = gg_owned_active(); scope->roots = NULL;
    scope->blocks = NULL; scope->thread = &gg_owned_current; scope->used = 0; scope->kind = 0;
    gg_owned_current = scope;
}
static int gg_owned_ancestor(gg_owned_scope *ancestor, gg_owned_scope *scope);
static void gg_owned_enter_kind(gg_owned_scope *scope, unsigned kind) {
    gg_owned_enter(scope); scope->kind = kind;
}
static gg_owned_scope *gg_owned_binding_owner(void) {
    gg_owned_scope *owner = gg_owned_active();
    while (owner->kind == 2) owner = owner->parent;
    return owner;
}
static void gg_owned_root_region_remove(gg_owned_root *root) {
    if (root->region_previous) root->region_previous->region_next = root->region_next;
    else root->lifetime->roots = root->region_next;
    if (root->region_next) root->region_next->region_previous = root->region_previous;
}
static void gg_owned_root_region_add(gg_owned_root *root, gg_owned_scope *lifetime) {
    root->lifetime = lifetime; root->region_previous = NULL;
    root->region_next = lifetime->roots;
    if (lifetime->roots) lifetime->roots->region_previous = root;
    lifetime->roots = root;
}
static void gg_owned_bind_lifetime(gg_owned_root *root, const void *address,
                                   size_t bytes, gg_owned_scope *owner,
                                   gg_owned_scope *lifetime) {
    if (!lifetime) lifetime = gg_owned_active();
    if (!gg_owned_ancestor(lifetime, gg_owned_active())) gg_fail("invalid owned root lifetime");
    root->address = address; root->bytes = bytes;
    root->owner = owner ? owner : &gg_owned_permanent;
    if (!root->bound) {
        root->previous = gg_owned_roots; root->next = NULL;
        if (gg_owned_roots) gg_owned_roots->next = root;
        gg_owned_roots = root; root->bound = 1;
        gg_owned_root_region_add(root, lifetime);
    } else if (root->lifetime != lifetime) {
        gg_owned_root_region_remove(root); gg_owned_root_region_add(root, lifetime);
    }
}
static void gg_owned_bind(gg_owned_root *root, const void *address,
                          size_t bytes, gg_owned_scope *owner) {
    gg_owned_bind_lifetime(root, address, bytes, owner, gg_owned_active());
}
static gg_owned_scope *gg_owned_owner(const void *address) {
    uintptr_t p = (uintptr_t)address; gg_owned_root *root; gg_owned_block *b;
    gg_owned_scope *owner = &gg_owned_permanent;
    for (root = gg_owned_roots; root; root = root->previous) {
        uintptr_t start = (uintptr_t)root->address;
        if (p >= start && p - start < root->bytes) return root->owner;
    }
    gg_owned_lock();
    for (b = gg_owned_blocks; b; b = b->next) {
        uintptr_t start = (uintptr_t)b->data;
        if (p >= start && p - start < b->bytes) { owner = b->owner; break; }
    }
    gg_owned_unlock(); return owner;
}
static void gg_owned_region_remove(gg_owned_block *b) {
    if (b->region_previous) b->region_previous->region_next = b->region_next;
    else b->owner->blocks = b->region_next;
    if (b->region_next) b->region_next->region_previous = b->region_previous;
}
static void gg_owned_region_add(gg_owned_block *b, gg_owned_scope *owner) {
    owner->used = 1;
    b->owner = owner; b->region_previous = NULL; b->region_next = owner->blocks;
    if (owner->blocks) owner->blocks->region_previous = b;
    owner->blocks = b;
}
static int gg_owned_ancestor(gg_owned_scope *ancestor, gg_owned_scope *scope) {
    for (; scope; scope = scope->parent) if (scope == ancestor) return 1;
    return 0;
}
static void gg_owned_keep(const void *data, gg_owned_scope *destination) {
    gg_owned_block *b; gg_owned_scope *owner;
    if (!data) return;
    if (!destination) destination = &gg_owned_permanent;
    if (destination != &gg_owned_permanent && destination->thread != &gg_owned_current)
        destination = &gg_owned_permanent;
    gg_owned_lock();
    for (b = gg_owned_blocks; b; b = b->next) if (b->data == data) break;
    if (b) {
        owner = b->owner;
        while (owner && !gg_owned_ancestor(owner, destination)) owner = owner->parent;
        if (!owner) owner = &gg_owned_permanent;
        if (owner != b->owner) { gg_owned_region_remove(b); gg_owned_region_add(b, owner); }
    }
    gg_owned_unlock();
}
static void gg_owned_leave(gg_owned_scope *scope) {
    gg_owned_block *b, *next;
    if (gg_owned_current != scope) gg_fail("unbalanced owned scope exit");
    if (scope->used) {
        gg_owned_lock();
        for (b = scope->blocks; b; b = next) {
            next = b->region_next;
            if (b->previous) b->previous->next = b->next; else gg_owned_blocks = b->next;
            if (b->next) b->next->previous = b->previous;
            free(b->data); free(b); gg_owned_count--;
        }
        scope->blocks = NULL; gg_owned_unlock();
    }
    while (scope->roots) {
        gg_owned_root *root = scope->roots;
        gg_owned_root_region_remove(root);
        if (root->next) root->next->previous = root->previous;
        else gg_owned_roots = root->previous;
        if (root->previous) root->previous->next = root->next;
        root->bound = 0;
    }
    gg_owned_current = scope->parent == &gg_owned_permanent ? NULL : scope->parent;
}
static void gg_owned_unwind_to(gg_owned_scope *scope) {
    if (!scope || !gg_owned_ancestor(scope, gg_owned_active())) gg_fail("invalid owned unwind target");
    while (gg_owned_active() != scope) gg_owned_leave(gg_owned_current);
}
static void gg_owned_release(void) {
    gg_owned_block *b, *next;
    gg_owned_lock();
    for (b = gg_owned_blocks; b; b = next) { next = b->next; free(b->data); free(b); }
    gg_owned_blocks = NULL; gg_owned_count = 0; gg_owned_permanent.blocks = NULL;
    gg_owned_unlock();
}
static gg_slice gg_array_length(gg_slice value, uint64_t maximum) {
    if ((uint64_t)value.len > maximum) gg_fail("array length exceeds storage range");
    return value;
}
static gg_slice gg_owned_image(const void *p, int64_t n, size_t element) {
    gg_slice v; gg_owned_block *b;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) gg_fail("invalid owned array size");
    v.len = n; v.data = NULL;
    if (!n) return v;
    v.data = malloc((size_t)n * element);
    if (!v.data) gg_fail("out of memory in owned array");
    b = malloc(sizeof *b);
    if (!b) { free(v.data); gg_fail("out of memory in owned image metadata"); }
    if (p) memcpy(v.data, p, (size_t)n * element);
    else memset(v.data, 0, (size_t)n * element);
    b->data = v.data; b->bytes = (size_t)n * element;
    gg_owned_lock();
    if (!gg_owned_registered) {
        if (atexit(gg_owned_release)) {
            gg_owned_unlock(); free(b); free(v.data); gg_fail("cannot register owned cleanup");
        }
        gg_owned_registered = 1;
    }
    b->previous = NULL; b->next = gg_owned_blocks;
    if (gg_owned_blocks) gg_owned_blocks->previous = b;
    gg_owned_blocks = b; gg_owned_region_add(b, gg_owned_active()); gg_owned_count++;
    gg_owned_unlock(); return v;
}
/* A variable enum contains exactly its tag and selected packed payload.
   The descriptor is a backend carrier, not a language serialization image. */
static gg_header gg_enum_new(uint32_t tag, size_t width, size_t payload) {
    gg_slice image; gg_header out;
    if ((width != 1 && width != 2) || payload > GG_RESERVATION - width ||
        tag > (width == 1 ? UINT8_MAX : UINT16_MAX)) gg_fail("invalid enum size or tag");
    image = gg_owned_image(NULL, (int64_t)(width + payload), 1);
    out.base = image.data; out.len = image.len;
    if (width == 1) { uint8_t v = (uint8_t)tag; memcpy(out.base, &v, 1); }
    else { uint16_t v = (uint16_t)tag; memcpy(out.base, &v, 2); }
    return out;
}
static uint32_t gg_enum_tag(gg_header value, size_t width) {
    if ((width != 1 && width != 2) || !value.base || value.len < (int64_t)width ||
        (uint64_t)value.len > GG_RESERVATION) gg_fail("invalid enum image");
    if (width == 1) { uint8_t v; memcpy(&v, value.base, 1); return v; }
    else { uint16_t v; memcpy(&v, value.base, 2); return v; }
}
static void *gg_enum_payload(gg_header value, size_t width, size_t payload) {
    (void)gg_enum_tag(value, width);
    if (payload > GG_RESERVATION - width || value.len != (int64_t)(width + payload))
        gg_fail("invalid enum payload size");
    return value.base + width;
}
static gg_header gg_enum_clone(gg_header value) {
    gg_slice image; gg_header out;
    if (!value.base || value.len < 1) gg_fail("invalid enum copy");
    image = gg_owned_image(value.base, value.len, 1);
    out.base = image.data; out.len = image.len; return out;
}
static int gg_uleb_read(const uint8_t *p, int64_t n, int64_t *at, uint64_t *out) {
    uint64_t v = 0; int shift = 0, i;
    for (i = 0; i < 10; i++) {
        uint8_t c;
        if (*at < 0 || *at >= n) return 0;
        c = p[(*at)++];
        if (shift > 63 || (shift == 63 && (c & 0x7e) != 0)) return 0;
        v |= (uint64_t)(c & 0x7f) << shift;
        if (!(c & 0x80)) { *out = v; return 1; }
        shift += 7;
    }
    return 0;
}
static void *gg_allocate(gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static gg_reference gg_ref(gg_header *h, gg_stack *s) { gg_reference r; r.header=h; r.stack=s; return r; }
static gg_slice gg_view(void *p, int64_t n) { gg_slice v; v.data=p; v.len=n; return v; }
static gg_slice gg_byte_view(gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) gg_fail("byte view size overflow");
    return gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static gg_slice gg_sub(gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) gg_fail("slice out of bounds");
    return gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) gg_fail("array index out of bounds"); return i; }
static void gg_append(gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) gg_fail("invalid array size");
    to=gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void gg_resize(gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) gg_fail("invalid array size");
        p=gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t gg_div(int64_t a, int64_t b) { if (!b) gg_fail("division by zero"); if (a==INT64_MIN && b==-1) gg_fail("division overflow"); return a/b; }
static int64_t gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t gg_udiv(uint64_t a,uint64_t b) { if(!b)gg_fail("division by zero");return a/b; }
static uint64_t gg_umod(uint64_t a,uint64_t b) { if(!b)gg_fail("division by zero");return a%b; }
static int gg_equal(gg_slice a, gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void gg_text(gg_reference out, gg_slice v) { gg_append(out,v.data,v.len,1); }
/* std's find_any and find_pair over a slice; a set points at std's packed
   ByteSet (members[256], kind, invert, bytes[3], lo[16], hi[16]). This is
   the scalar path of the pinned runtime, kept for compatibility. */
#pragma pack(push, 1)
typedef struct { uint8_t members[256]; uint8_t kind, invert, bytes[3], lo[16], hi[16]; } gs_abi_byteset;
#pragma pack(pop)
static int64_t gs_scan_any(const uint8_t *p, int64_t n, const gs_abi_byteset *s) {
    if (n <= 0) return -1;
    if (s->kind == 0) return s->invert ? 0 : -1;
    if (s->kind == 1 && !s->invert) {
        const uint8_t *q = (const uint8_t *)memchr(p, s->bytes[0], (size_t)n);
        return q ? (int64_t)(q - p) : -1;
    }
    for (int64_t i = 0; i < n; i++) if (s->members[p[i]]) return i;
    return -1;
}
static int64_t gs_scan_pair(const uint8_t *p, int64_t n, const gs_abi_byteset *a, int64_t d,
                            const gs_abi_byteset *b) {
    int64_t np;
    if (d < 0 || d >= n) return -1;
    np = n - d;
    if (a->kind == 0) return a->invert ? gs_scan_any(p + d, np, b) : -1;
    if (b->kind == 0) return b->invert ? gs_scan_any(p, np, a) : -1;
    for (int64_t i = 0; i < np; i++) if (a->members[p[i]] & b->members[p[i + d]]) return i;
    return -1;
}
/* The emitted extern declaration carries the module's own reference type,
   so these stay macros rather than a conflicting C prototype. */
#define gs_find_any(s, set) gs_scan_any((const uint8_t *)(s).data, (s).len, (const gs_abi_byteset *)(set))
#define gs_find_pair(s, a, d, b) gs_scan_pair((const uint8_t *)(s).data, (s).len, (const gs_abi_byteset *)(a), (d), (const gs_abi_byteset *)(b))

static void gg_integer(gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);gg_append(out,b,k,1); }
static void gg_unsigned(gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);gg_append(out,b,k,1); }
static double gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t ggf_fmt_f64(uint8_t *out,double n);
static int64_t ggf_fmt_f32(uint8_t *out,float n);
/* Only the in-range fast path uses a C float-to-integer cast. The tail
   projects the finite IEEE significand modulo 2^64 without UB or libm.
   Nonfinite unchecked inputs produce zero; checked inputs reject them. */
static uint64_t gg_float_integer_wrap(double d) {
    uint64_t bits, magnitude; int exponent;
    if(d>=-9223372036854775808.0 && d<9223372036854775808.0)
        return (uint64_t)(int64_t)d;
    memcpy(&bits,&d,sizeof bits);
    exponent=(int)((bits>>52)&2047)-1023;
    if(exponent<0 || exponent>=116) return 0;
    magnitude=(bits&UINT64_C(0xFFFFFFFFFFFFF))|UINT64_C(0x10000000000000);
    if(exponent>=52) magnitude<<=exponent-52;
    else magnitude>>=52-exponent;
    return bits>>63 ? (uint64_t)0-magnitude : magnitude;
}
#if defined(GS_DEBUG) && GS_DEBUG
static void gg_float_integer_fail(double d,int f32,const char *type,const char *file,int line,const char *why) {
    uint8_t text[64];int64_t n=f32?ggf_fmt_f32(text,(float)d):ggf_fmt_f64(text,d);
    fprintf(stderr,"goose runtime error: conversion %s to %s (%.*s) at %s:%d\\n",why,type,(int)n,(const char*)text,file,line);
    exit(1);
}
#endif
static uint64_t gg_float_integer(double d,int width,int unsign,int checked,int f32,const char *type,const char *file,int line) {
#if defined(GS_DEBUG) && GS_DEBUG
    if(checked) {
        int64_t value,lo,hi;
        if(unsign && width==64) {
            uint64_t u;
            if(!(d>=0.0 && d<18446744073709551616.0))
                gg_float_integer_fail(d,f32,type,file,line,"out of range");
            u=(uint64_t)d;
            if((double)u!=d)gg_float_integer_fail(d,f32,type,file,line,"changes the value");
            return u;
        }
        if(!(d>=-9223372036854775808.0 && d<9223372036854775808.0))
            gg_float_integer_fail(d,f32,type,file,line,"out of range");
        value=(int64_t)d;
        hi=(int64_t)((UINT64_C(1)<<(width-(unsign?0:1)))-1);
        lo=unsign?0:-1-hi;
        if(value<lo || value>hi)gg_float_integer_fail(d,f32,type,file,line,"out of range");
        if((double)value!=d)gg_float_integer_fail(d,f32,type,file,line,"changes the value");
        return (uint64_t)value;
    }
#else
    (void)width;(void)unsign;(void)checked;(void)f32;(void)type;(void)file;(void)line;
#endif
    return gg_float_integer_wrap(d);
}
static void gg_float(gg_reference out,double n) { uint8_t b[64];int64_t k=ggf_fmt_f64(b,n);gg_append(out,b,k,1); }
static void gg_float32(gg_reference out,float n) { uint8_t b[64];int64_t k=ggf_fmt_f32(b,n);gg_append(out,b,k,1); }
static void gg_bool(gg_reference out,uint8_t n) { gg_append(out,n?"true":"false",n?4:5,1); }
static void gg_write(FILE *file,gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)gg_fail("write failed"); }
static char *gg_cstring(gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static int gg_os_path_ok(gg_slice p) { int64_t i; if(p.len<0||p.len>=4096)return 0; for(i=0;i<p.len;i++)if(((const unsigned char*)p.data)[i]==0)return 0; return 1; }
static uint8_t gs_os_read_file(gg_slice path,gg_reference out) {
    if(!gg_os_path_ok(path))return 0;
    char *p=gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t gs_os_write_file(gg_slice path,gg_slice data) {
    if(!gg_os_path_ok(path))return 0;
    char *p=gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t gs_os_file_exists(gg_slice path) { if(!gg_os_path_ok(path))return 0; char*p=gg_cstring(path);FILE*f=fopen(p,"rb");int bad;free(p);if(!f)return 0;{char b;if(fread(&b,1,1,f)==0){}bad=ferror(f);}fclose(f);return bad?0:1; }
static void gs_os_write_stdout(gg_slice s) { gg_write(stdout,s); }
static void gs_os_write_stderr(gg_slice s) { gg_write(stderr,s); }
static void gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t gs_os_arg_count(void) { return gg_argc; }
static void gs_os_arg(int64_t i,gg_reference out) { if(i>=0 && i<gg_argc)gg_append(out,gg_argv[i],(int64_t)strlen(gg_argv[i]),1); }
/* math's sqrt (stdlib/math.goose). Goose has no errno, and C's sqrt has to
   set it for a negative argument: clang and gcc then guard the square root
   instruction with a test and a library call at every use, and will not
   vectorize a loop around one. A compiler that has a square root without
   errno uses that instead; the value is the same correctly rounded root
   either way, and NaN for a negative argument. */
#if defined(__has_builtin)
#if __has_builtin(__builtin_elementwise_sqrt)
#define GS_SQRT_NO_ERRNO 1
#endif
#endif
#ifdef GS_SQRT_NO_ERRNO
static double gs_sqrt(double x) { return __builtin_elementwise_sqrt(x); }
static float gs_sqrtf(float x) { return __builtin_elementwise_sqrt(x); }
#else
static double gs_sqrt(double x) { return sqrt(x); }
static float gs_sqrtf(float x) { return sqrtf(x); }
#endif

/* math's other libm functions (stdlib/math.goose). C has them set errno on a
   domain or range error, so a C compiler takes each call for a write to
   memory: a loop around one loads again after it whatever it had hoisted
   out or kept in registers. Goose has no errno, so where the compiler has
   asm labels and the const attribute each is libm's own function declared
   under a name of its own as depending on its arguments alone; elsewhere
   the name forwards to libm. */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
#define GS_LIBM_SYM2(p, f) #p #f
#define GS_LIBM_SYM(p, f) GS_LIBM_SYM2(p, f)
#define GS_LIBM
#define GS_LIBM1(f) __asm__(GS_LIBM_SYM(__USER_LABEL_PREFIX__, f)) __attribute__((const));
#define GS_LIBM2(f) GS_LIBM1(f)
#else
#define GS_LIBM static
#define GS_LIBM1(f) { return f(x); }
#define GS_LIBM2(f) { return f(x, y); }
#endif
GS_LIBM double gs_sin(double x) GS_LIBM1(sin)
GS_LIBM double gs_cos(double x) GS_LIBM1(cos)
GS_LIBM double gs_tan(double x) GS_LIBM1(tan)
GS_LIBM double gs_asin(double x) GS_LIBM1(asin)
GS_LIBM double gs_acos(double x) GS_LIBM1(acos)
GS_LIBM double gs_atan(double x) GS_LIBM1(atan)
GS_LIBM double gs_atan2(double x, double y) GS_LIBM2(atan2)
GS_LIBM double gs_exp(double x) GS_LIBM1(exp)
GS_LIBM double gs_log(double x) GS_LIBM1(log)
GS_LIBM double gs_log2(double x) GS_LIBM1(log2)
GS_LIBM double gs_log10(double x) GS_LIBM1(log10)
GS_LIBM double gs_pow(double x, double y) GS_LIBM2(pow)
GS_LIBM float gs_sinf(float x) GS_LIBM1(sinf)
GS_LIBM float gs_cosf(float x) GS_LIBM1(cosf)
GS_LIBM float gs_tanf(float x) GS_LIBM1(tanf)
GS_LIBM float gs_asinf(float x) GS_LIBM1(asinf)
GS_LIBM float gs_acosf(float x) GS_LIBM1(acosf)
GS_LIBM float gs_atanf(float x) GS_LIBM1(atanf)
GS_LIBM float gs_atan2f(float x, float y) GS_LIBM2(atan2f)
GS_LIBM float gs_expf(float x) GS_LIBM1(expf)
GS_LIBM float gs_logf(float x) GS_LIBM1(logf)
GS_LIBM float gs_log2f(float x) GS_LIBM1(log2f)
GS_LIBM float gs_log10f(float x) GS_LIBM1(log10f)
GS_LIBM float gs_powf(float x, float y) GS_LIBM2(powf)
#if defined(__CUDACC__)
static __host__ __device__ double gs_exp_cuda(double x) { return exp(x); }
static __host__ __device__ float gs_expf_cuda(float x) { return expf(x); }
#define gs_exp(x) gs_exp_cuda(x)
#define gs_expf(x) gs_expf_cuda(x)
#endif
/* std's popcount, clz and ctz (64 for zero): the instruction where the C
   compiler has one, and counting under TinyCC. MSVC's popcount is an
   instruction past x86-64's baseline, which /arch:AVX and up promise. */
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif
static int64_t gs_popcount(uint64_t x) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
    return __builtin_popcountll(x);
#elif defined(_MSC_VER) && defined(_M_X64) && defined(__AVX__)
    return (int64_t)__popcnt64(x);
#else
    int64_t n = 0;
    while (x) {
        x &= x - 1;
        n++;
    }
    return n;
#endif
}
static int64_t gs_clz(uint64_t x) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
    return x ? __builtin_clzll(x) : 64;
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    unsigned long i;
    return _BitScanReverse64(&i, x) ? 63 - (int64_t)i : 64;
#else
    int64_t n = 0;
    if (!x) return 64;
    while (!(x >> 63)) {
        x <<= 1;
        n++;
    }
    return n;
#endif
}
static int64_t gs_ctz(uint64_t x) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
    return x ? __builtin_ctzll(x) : 64;
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    unsigned long i;
    return _BitScanForward64(&i, x) ? (int64_t)i : 64;
#else
    int64_t n = 0;
    if (!x) return 64;
    while (!(x & 1)) {
        x >>= 1;
        n++;
    }
    return n;
#endif
}
/* A float's text is the fewest significant digits that read back as the
   same value of its own type and, of those, the ones nearest it, a tie going
   to the even digit (as Python's repr and Ryu choose). They are found in
   integers, by Burger and Dybvig's free-format algorithm, not through the C
   library: msvcrt, which tcc uses on Windows, rounds a printf tie away from
   zero, and below a power of two the gap to the next float is half the gap
   above, so the shortest text can lie above the value while the correctly
   rounded spelling of that length, below it, does not read back.

   The numbers are naturals in 32-bit limbs, least significant first. The
   denominator s stays below 2^1079 (ten times 2^1075, for the smallest
   doubles), and the others below eleven times s. */
typedef struct { int n; uint32_t d[36]; } ggf_big;

/* a = v << sh, for v > 0. */
static void ggf_big_set(ggf_big *a, uint64_t v, int sh) {
    int w = sh >> 5, b = sh & 31, i;
    uint64_t lo = v << b, hi = b ? v >> (64 - b) : 0;
    for (i = 0; i < w; i++) a->d[i] = 0;
    a->d[w] = (uint32_t)lo;
    a->d[w + 1] = (uint32_t)(lo >> 32);
    a->d[w + 2] = (uint32_t)hi;
    a->n = w + 3;
    while (!a->d[a->n - 1]) a->n--;
}

static void ggf_big_mul(ggf_big *a, uint32_t m) {
    uint64_t c = 0;
    for (int i = 0; i < a->n; i++) {
        c += (uint64_t)a->d[i] * m;
        a->d[i] = (uint32_t)c;
        c >>= 32;
    }
    if (c) a->d[a->n++] = (uint32_t)c;
}

static void ggf_big_mul_pow10(ggf_big *a, int k) {
    for (; k >= 9; k -= 9) ggf_big_mul(a, 1000000000u);
    for (; k > 0; k--) ggf_big_mul(a, 10);
}

static int ggf_big_cmp(const ggf_big *a, const ggf_big *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

/* t = a + b */
static void ggf_big_add(ggf_big *t, const ggf_big *a, const ggf_big *b) {
    int n = a->n > b->n ? a->n : b->n;
    uint64_t c = 0;
    for (int i = 0; i < n; i++) {
        c += (uint64_t)(i < a->n ? a->d[i] : 0) + (i < b->n ? b->d[i] : 0);
        t->d[i] = (uint32_t)c;
        c >>= 32;
    }
    t->n = n;
    if (c) t->d[t->n++] = (uint32_t)c;
}

/* a -= b, for a >= b. */
static void ggf_big_sub(ggf_big *a, const ggf_big *b) {
    uint64_t borrow = 0;
    for (int i = 0; i < a->n; i++) {
        uint64_t x = (uint64_t)a->d[i] - (i < b->n ? b->d[i] : 0) - borrow;
        a->d[i] = (uint32_t)x;
        borrow = x >> 63;
    }
    while (a->n && !a->d[a->n - 1]) a->n--;
}

/* The shortest digits of f * 2^e (f > 0) as a float of `bits` significant
   bits and least exponent emin, into dig; returns their count and sets *k
   so that the value they spell is 0.d1d2... * 10^*k. */
static int ggf_float_digits(uint64_t f, int e, int bits, int emin, char *dig, int *k) {
    ggf_big r, s, mp, mlo, t;
    /* Reading rounds a tie to the even significand, so the interval that
       reads back as f * 2^e includes its ends when f is even. */
    int even = !(f & 1);
    /* Below a power of two, the gap to the next float down is half the gap up. */
    int asym = f == (uint64_t)1 << (bits - 1) && e > emin;
    int up = e > 0 ? e : 0, lg = e - 1, n = 0, c;
    /* The value is r / s, and what reads back as it reaches from (r - *mm) / s
       to (r + mp) / s: half the gap to each neighbour. */
    ggf_big *mm = asym ? &mlo : &mp;
    ggf_big_set(&r, f, up + 1 + asym);
    ggf_big_set(&s, 1, up - e + 1 + asym);
    ggf_big_set(&mp, 1, up + asym);
    ggf_big_set(&mlo, 1, up);
    /* Scale to 10^*k, the least power of ten above the interval: with
       lg = floor(log2 value), ceil(lg log10 2) is it or one short, and
       78913 / 2^18 is log10 2 closely enough to compute that for |lg| < 1650. */
    for (uint64_t g = f; g; g >>= 1) lg++;
    *k = lg > 0 ? ((lg * 78913) >> 18) + 1 : -((-lg * 78913) >> 18);
    if (*k >= 0) ggf_big_mul_pow10(&s, *k);
    else {
        ggf_big_mul_pow10(&r, -*k);
        ggf_big_mul_pow10(&mp, -*k);
        if (asym) ggf_big_mul_pow10(&mlo, -*k);
    }
    ggf_big_add(&t, &r, &mp);
    c = ggf_big_cmp(&t, &s);
    if (even ? c >= 0 : c > 0) {
        ggf_big_mul(&s, 10);
        ++*k;
    }
    /* Each digit d leaves the remainder r: stop where the digits so far
       followed by d (r within *mm) or by d + 1 (r + mp past s) read back. */
    for (;;) {
        int d = 0, low, high;
        ggf_big_mul(&r, 10);
        ggf_big_mul(&mp, 10);
        if (asym) ggf_big_mul(&mlo, 10);
        while (ggf_big_cmp(&r, &s) >= 0) {
            ggf_big_sub(&r, &s);
            d++;
        }
        c = ggf_big_cmp(&r, mm);
        low = even ? c <= 0 : c < 0;
        ggf_big_add(&t, &r, &mp);
        c = ggf_big_cmp(&t, &s);
        high = even ? c >= 0 : c > 0;
        if (!low && !high) {
            dig[n++] = (char)('0' + d);
            continue;
        }
        if (low && high) {
            /* Both read back: the nearer, d + 1 when r is past half of s,
               and of two as near, the even one. */
            ggf_big_add(&t, &r, &r);
            c = ggf_big_cmp(&t, &s);
            high = c > 0 || (c == 0 && (d & 1));
        }
        dig[n++] = (char)('0' + d + high);
        return n;
    }
}

/* The text of a finite float from its fields: the sign, the significand
   without its implicit bit and the biased exponent, for a type of `bits`
   significant bits and least exponent emin. The digits are laid out as C's
   %g lays them out at a precision of max(15, digits): in exponent form, with
   at least two exponent digits, below 1e-4 or from that power of ten up. */
static int64_t ggf_fmt_float(uint8_t *dst, int neg, uint64_t frac, int bexp, int bits,
                            int emin) {
    char dig[20], *p = (char *)dst;
    int k, n, x, i;
    if (neg) *p++ = '-';
    if (!frac && !bexp) {
        memcpy(p, "0.0", 3);
        return p + 3 - (char *)dst;
    }
    n = ggf_float_digits(bexp ? frac | (uint64_t)1 << (bits - 1) : frac,
                        (bexp ? bexp - 1 : 0) + emin, bits, emin, dig, &k);
    x = k - 1;
    if (x < -4 || x >= (n > 15 ? n : 15)) {
        *p++ = dig[0];
        if (n > 1) *p++ = '.';
        for (i = 1; i < n; i++) *p++ = dig[i];
        *p++ = 'e';
        *p++ = x < 0 ? '-' : '+';
        if (x < 0) x = -x;
        if (x >= 100) *p++ = (char)('0' + x / 100);
        *p++ = (char)('0' + x / 10 % 10);
        *p++ = (char)('0' + x % 10);
    } else if (x < 0) {
        *p++ = '0';
        *p++ = '.';
        for (i = -1; i > x; i--) *p++ = '0';
        for (i = 0; i < n; i++) *p++ = dig[i];
    } else {
        for (i = 0; i < n || i <= x; i++) {
            if (i == x + 1) *p++ = '.';
            *p++ = i < n ? dig[i] : '0';
        }
        /* A whole number still reads as a float: 1.0, not 1. */
        if (n <= x + 1) {
            *p++ = '.';
            *p++ = '0';
        }
    }
    return p - (char *)dst;
}

static int64_t ggf_fmt_f64(uint8_t *dst, double v) {
    uint64_t b;
    /* C libraries disagree here (msvcrt, which tcc uses on Windows, writes
       1.#INF and -1.#IND; others give a NaN's sign bit, which depends on
       the CPU that made it), so these are spelled by the runtime. */
    if (v != v) { memcpy(dst, "nan", 3); return 3; }
    if (isinf(v)) {
        if (v > 0) { memcpy(dst, "inf", 3); return 3; }
        memcpy(dst, "-inf", 4);
        return 4;
    }
    memcpy(&b, &v, sizeof b);
    return ggf_fmt_float(dst, (int)(b >> 63), b & (((uint64_t)1 << 52) - 1),
                        (int)(b >> 52) & 0x7FF, 53, -1074);
}

/* The digits are the f32's own, laid out as an f64's are, so 0.1 as an f32
   prints as 0.1, not as the digits of the f64 it widens to. */
static int64_t ggf_fmt_f32(uint8_t *dst, float v) {
    double d = v;
    uint32_t b;
    if (d != d || isinf(d)) return ggf_fmt_f64(dst, d);
    memcpy(&b, &v, sizeof b);
    return ggf_fmt_float(dst, (int)(b >> 31), b & 0x7FFFFF, (int)(b >> 23) & 0xFF, 24, -149);
}


typedef int64_t gg_unaligned_1 GG_ALIGN1;
typedef float gg_unaligned_2 GG_ALIGN1;
typedef uint8_t gg_unaligned_3 GG_ALIGN1;
typedef gg_slice gg_unaligned_4 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_5;
#pragma pack(pop)
typedef double gg_unaligned_6 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { int64_t data[4]; } gg_type_7;
#pragma pack(pop)
typedef gg_type_5 *gg_type_8;
typedef gg_type_8 gg_unaligned_8 GG_ALIGN1;
typedef gg_type_7 *gg_type_9;
typedef gg_type_9 gg_unaligned_9 GG_ALIGN1;
typedef gg_slice gg_unaligned_10 GG_ALIGN1;
typedef gg_slice gg_unaligned_11 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_1 *gg_type_12;
typedef gg_type_12 gg_unaligned_12 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_2 *gg_type_13;
typedef gg_type_13 gg_unaligned_13 GG_ALIGN1;
typedef gg_slice gg_unaligned_16 GG_ALIGN1;
typedef gg_slice gg_unaligned_17 GG_ALIGN1;
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(int64_t a, int64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, int64_t value);
static uint8_t gg_eq_2(float a, float b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, float value);
static uint8_t gg_eq_3(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint8_t value);
static uint8_t gg_eq_4(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_slice value);
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_type_5 value);
static uint8_t gg_eq_6(double a, double b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, double value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, gg_type_9 value);
static uint8_t gg_eq_10(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, gg_slice value);
static uint8_t gg_eq_11(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, gg_slice value);
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_type_12 value);
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_type_13 value);
static uint8_t gg_eq_14(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, uint8_t value);
static uint8_t gg_eq_16(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, gg_slice value);
static uint8_t gg_eq_17(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_slice value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((GG_UNALIGNED gg_unaligned_1*)a.data)[i],((GG_UNALIGNED gg_unaligned_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((GG_UNALIGNED gg_unaligned_2*)a.data)[i],((GG_UNALIGNED gg_unaligned_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(float a, float b) {
return a == b;
}
static void gg_render_2(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;if(a.data==b.data)return 1;for(i=0;i<a.len;i++)if(!gg_eq_3(((GG_UNALIGNED gg_unaligned_3*)a.data)[i],((GG_UNALIGNED gg_unaligned_3*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((GG_UNALIGNED gg_unaligned_4*)a.data)[i],((GG_UNALIGNED gg_unaligned_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_4(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_type_5*)a.data)[i],((gg_type_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
return gg_equal_elements_2(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_2(out, ((GG_UNALIGNED gg_unaligned_2*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((GG_UNALIGNED gg_unaligned_6*)a.data)[i],((GG_UNALIGNED gg_unaligned_6*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(double a, double b) {
return a == b;
}
static void gg_render_6(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_1(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((GG_UNALIGNED gg_unaligned_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((GG_UNALIGNED gg_unaligned_8*)a.data)[i],((GG_UNALIGNED gg_unaligned_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
return a == b;
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
gg_render_5(out,*value);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((GG_UNALIGNED gg_unaligned_9*)a.data)[i],((GG_UNALIGNED gg_unaligned_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b) {
return a == b;
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
gg_render_7(out,*value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((GG_UNALIGNED gg_unaligned_10*)a.data)[i],((GG_UNALIGNED gg_unaligned_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_slice a, gg_slice b) {
return gg_equal_elements_2(a,b);
}
static void gg_render_10(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_2(out, ((GG_UNALIGNED gg_unaligned_2*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((GG_UNALIGNED gg_unaligned_11*)a.data)[i],((GG_UNALIGNED gg_unaligned_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_11(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((GG_UNALIGNED gg_unaligned_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((GG_UNALIGNED gg_unaligned_12*)a.data)[i],((GG_UNALIGNED gg_unaligned_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return a == b;
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((GG_UNALIGNED gg_unaligned_13*)a.data)[i],((GG_UNALIGNED gg_unaligned_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
return a == b;
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
gg_render_2(out,*value);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_14(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((GG_UNALIGNED gg_unaligned_16*)a.data)[i],((GG_UNALIGNED gg_unaligned_16*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_16(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((GG_UNALIGNED gg_unaligned_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((GG_UNALIGNED gg_unaligned_17*)a.data)[i],((GG_UNALIGNED gg_unaligned_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_slice a, gg_slice b) {
return gg_equal_elements_2(a,b);
}
static void gg_render_17(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_2(out, ((GG_UNALIGNED gg_unaligned_2*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base);
static int64_t gg_function_2(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base);
static void gg_function_3(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t base);
static void gg_function_4(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t base);
static double gg_function_5(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_8(gg_slice p0, gg_slice p1, int64_t p2, int64_t base);
static int64_t gg_function_9(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_10(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_11(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, float p5, int64_t p6, int64_t base);
static int64_t gg_function_12(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, float p5, int64_t p6, int64_t base);
static void gg_function_13(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static void gg_function_14(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_15(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base);
static int64_t gg_function_16(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base);
static int64_t gg_function_17(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t p4, float p5, int64_t p6, float p7, float p8, int64_t base);
static int64_t gg_function_18(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, float p5, int64_t p6, float p7, int64_t base);
static int64_t gg_function_19(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t base);
static int64_t gg_function_20(float p0, float p1, float p2, gg_slice p3, int64_t base);
static int64_t gg_function_21(gg_slice p0, int64_t p1, float p2, int64_t p3, float p4, int64_t base);
static float gg_function_24(float p0, float p1, float p2, float p3, int64_t base);
static int64_t gg_function_25(gg_slice p0, int64_t base);
static void gg_function_26(gg_slice p0, int64_t base);
static int64_t gg_function_27(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, float p5, float p6, float p7, int64_t base);
static double gg_function_28(gg_slice p0, int64_t base);
static int64_t gg_function_29(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, float p5, float p6, int64_t base);
static int64_t gg_function_31(gg_slice p0, int64_t p1, gg_slice p2, int64_t base);
static int64_t gg_function_32(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base);
static int64_t gg_function_33(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_34(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t p5, int64_t p6, int64_t base);
static int64_t gg_function_35(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t p5, int64_t p6, int64_t base);
static int64_t gg_function_36(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_37(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_38(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, float p10, float p11, int64_t base);
static int64_t gg_function_39(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, float p10, float p11, int64_t base);
static int64_t gg_function_40(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t base);
static int64_t gg_function_41(gg_slice p0, int64_t p1, float p2, int64_t p3, float p4, int64_t base);
static int64_t gg_function_42(gg_slice p0, int64_t p1, gg_slice p2, int64_t base);
static int64_t gg_function_12(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, float p5, int64_t p6, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint8_t t8;
  gg_slice t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  GG_UNALIGNED gg_unaligned_2 * t13;
  _Alignas(8) float t14;
  _Alignas(8) float t15;
  _Alignas(8) uint8_t t16;
  uint8_t t17;
  gg_slice t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  GG_UNALIGNED gg_unaligned_2 * t22;
  gg_slice t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  GG_UNALIGNED gg_unaligned_2 * t27;
  gg_slice t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  GG_UNALIGNED gg_unaligned_1 * t32;
  gg_slice t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  GG_UNALIGNED gg_unaligned_1 * t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  gg_slice t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  GG_UNALIGNED gg_unaligned_2 * t46;
  gg_slice t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  GG_UNALIGNED gg_unaligned_1 * t51;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) float v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  v7 = ((int64_t)(v3));
  t1 = ((int64_t)(v7));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) - (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  v8 = ((int64_t)(((int64_t)gg_div(t3, t4))));
label1:;
  t5 = ((int64_t)(v7));
  t6 = ((int64_t)(v4));
  t7 = ((t5) > (t6));
  t8 = t7;
  if (t8) {
  t9 = v0;
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(v8));
  t12 = ((int64_t)((uint64_t)(t10) + (uint64_t)(t11)));
  t13 = ((GG_UNALIGNED gg_unaligned_2 *)t9.data) + gg_index(t12, t9.len);
  t14 = ((float)((*t13)));
  t15 = ((float)(v5));
  t8 = ((t14) > (t15));
  }
  t16 = t8;
  t17 = t16;
  if(!t17) goto label2;
  {
  t18 = v0;
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(v7));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((GG_UNALIGNED gg_unaligned_2 *)t18.data) + gg_index(t21, t18.len);
  t23 = v0;
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(v8));
  t26 = ((int64_t)((uint64_t)(t24) + (uint64_t)(t25)));
  t27 = ((GG_UNALIGNED gg_unaligned_2 *)t23.data) + gg_index(t26, t23.len);
  (*t22) = ((float)((*t27)));
  t28 = v1;
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(v7));
  t31 = ((int64_t)((uint64_t)(t29) + (uint64_t)(t30)));
  t32 = ((GG_UNALIGNED gg_unaligned_1 *)t28.data) + gg_index(t31, t28.len);
  t33 = v1;
  t34 = ((int64_t)(v2));
  t35 = ((int64_t)(v8));
  t36 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  t37 = ((GG_UNALIGNED gg_unaligned_1 *)t33.data) + gg_index(t36, t33.len);
  (*t32) = ((int64_t)((*t37)));
  v7 = ((int64_t)(v8));
  t38 = ((int64_t)(v7));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) - (uint64_t)(t39)))));
  t41 = ((int64_t)(((int64_t)UINT64_C(2))));
  v8 = ((int64_t)(((int64_t)gg_div(t40, t41))));
  }
  goto label1;
label2:;
  t42 = v0;
  t43 = ((int64_t)(v2));
  t44 = ((int64_t)(v7));
  t45 = ((int64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  t46 = ((GG_UNALIGNED gg_unaligned_2 *)t42.data) + gg_index(t45, t42.len);
  (*t46) = ((float)(v5));
  t47 = v1;
  t48 = ((int64_t)(v2));
  t49 = ((int64_t)(v7));
  t50 = ((int64_t)((uint64_t)(t48) + (uint64_t)(t49)));
  t51 = ((GG_UNALIGNED gg_unaligned_1 *)t47.data) + gg_index(t50, t47.len);
  (*t51) = ((int64_t)(v6));
  t0 = ((int64_t)(v7));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_11(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, float p5, int64_t p6, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint8_t t6;
  uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  gg_slice t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  GG_UNALIGNED gg_unaligned_2 * t16;
  _Alignas(8) float t17;
  gg_slice t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  GG_UNALIGNED gg_unaligned_2 * t24;
  _Alignas(8) float t25;
  _Alignas(8) uint8_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  gg_slice t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  GG_UNALIGNED gg_unaligned_2 * t33;
  gg_slice t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  GG_UNALIGNED gg_unaligned_2 * t38;
  gg_slice t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  GG_UNALIGNED gg_unaligned_1 * t43;
  gg_slice t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  GG_UNALIGNED gg_unaligned_1 * t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) uint8_t t53;
  _Alignas(8) uint8_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) uint8_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) int64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  gg_slice t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  GG_UNALIGNED gg_unaligned_2 * t70;
  gg_slice t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) int64_t t75;
  _Alignas(8) int64_t t76;
  GG_UNALIGNED gg_unaligned_2 * t77;
  gg_slice t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) int64_t t80;
  _Alignas(8) int64_t t81;
  GG_UNALIGNED gg_unaligned_1 * t82;
  gg_slice t83;
  _Alignas(8) int64_t t84;
  _Alignas(8) int64_t t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) int64_t t87;
  _Alignas(8) int64_t t88;
  GG_UNALIGNED gg_unaligned_1 * t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  _Alignas(8) gg_slice t92;
  _Alignas(8) gg_slice t93;
  _Alignas(8) int64_t t94;
  _Alignas(8) int64_t t95;
  _Alignas(8) int64_t t96;
  _Alignas(8) float t97;
  _Alignas(8) int64_t t98;
  _Alignas(8) int64_t t99;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) float v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  v7 = ((int64_t)(v3));
  v8 = ((int64_t)(v7));
label1:;
  t0 = ((int64_t)(v8));
  t1 = ((int64_t)(v4));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) - (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  t5 = ((int64_t)(((int64_t)gg_div(t3, t4))));
  t6 = ((t0) < (t5));
  t7 = t6;
  if(!t7) goto label2;
  {
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  t9 = ((int64_t)(v8));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t11)))));
  t12 = v0;
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(v8));
  t15 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  t16 = ((GG_UNALIGNED gg_unaligned_2 *)t12.data) + gg_index(t15, t12.len);
  t17 = ((float)((*t16)));
  t18 = v0;
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(v8));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  t23 = ((int64_t)((uint64_t)(t21) - (uint64_t)(t22)));
  t24 = ((GG_UNALIGNED gg_unaligned_2 *)t18.data) + gg_index(t23, t18.len);
  t25 = ((float)((*t24)));
  t26 = ((t17) > (t25));
  if (t26) {
  {
  t27 = ((int64_t)(v8));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t27) - (uint64_t)(t28)))));
  }
  }
  t29 = v0;
  t30 = ((int64_t)(v2));
  t31 = ((int64_t)(v7));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((GG_UNALIGNED gg_unaligned_2 *)t29.data) + gg_index(t32, t29.len);
  t34 = v0;
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)(v8));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((GG_UNALIGNED gg_unaligned_2 *)t34.data) + gg_index(t37, t34.len);
  (*t33) = ((float)((*t38)));
  t39 = v1;
  t40 = ((int64_t)(v2));
  t41 = ((int64_t)(v7));
  t42 = ((int64_t)((uint64_t)(t40) + (uint64_t)(t41)));
  t43 = ((GG_UNALIGNED gg_unaligned_1 *)t39.data) + gg_index(t42, t39.len);
  t44 = v1;
  t45 = ((int64_t)(v2));
  t46 = ((int64_t)(v8));
  t47 = ((int64_t)((uint64_t)(t45) + (uint64_t)(t46)));
  t48 = ((GG_UNALIGNED gg_unaligned_1 *)t44.data) + gg_index(t47, t44.len);
  (*t43) = ((int64_t)((*t48)));
  v7 = ((int64_t)(v8));
  }
  goto label1;
label2:;
  t49 = ((int64_t)(v4));
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  t51 = ((int64_t)(((t49) & (t50))));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((t51) == (t52));
  t54 = t53;
  if (t54) {
  t55 = ((int64_t)(v8));
  t56 = ((int64_t)(v4));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) - (uint64_t)(t57)))));
  t59 = ((int64_t)(((int64_t)UINT64_C(2))));
  t60 = ((int64_t)(((int64_t)gg_div(t58, t59))));
  t54 = ((t55) == (t60));
  }
  t61 = t54;
  if (t61) {
  {
  t62 = ((int64_t)(((int64_t)UINT64_C(2))));
  t63 = ((int64_t)(v8));
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t64)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t65)))));
  t66 = v0;
  t67 = ((int64_t)(v2));
  t68 = ((int64_t)(v7));
  t69 = ((int64_t)((uint64_t)(t67) + (uint64_t)(t68)));
  t70 = ((GG_UNALIGNED gg_unaligned_2 *)t66.data) + gg_index(t69, t66.len);
  t71 = v0;
  t72 = ((int64_t)(v2));
  t73 = ((int64_t)(v8));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  t75 = ((int64_t)(((int64_t)UINT64_C(1))));
  t76 = ((int64_t)((uint64_t)(t74) - (uint64_t)(t75)));
  t77 = ((GG_UNALIGNED gg_unaligned_2 *)t71.data) + gg_index(t76, t71.len);
  (*t70) = ((float)((*t77)));
  t78 = v1;
  t79 = ((int64_t)(v2));
  t80 = ((int64_t)(v7));
  t81 = ((int64_t)((uint64_t)(t79) + (uint64_t)(t80)));
  t82 = ((GG_UNALIGNED gg_unaligned_1 *)t78.data) + gg_index(t81, t78.len);
  t83 = v1;
  t84 = ((int64_t)(v2));
  t85 = ((int64_t)(v8));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t85)))));
  t87 = ((int64_t)(((int64_t)UINT64_C(1))));
  t88 = ((int64_t)((uint64_t)(t86) - (uint64_t)(t87)));
  t89 = ((GG_UNALIGNED gg_unaligned_1 *)t83.data) + gg_index(t88, t83.len);
  (*t82) = ((int64_t)((*t89)));
  t90 = ((int64_t)(v8));
  t91 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t90) - (uint64_t)(t91)))));
  }
  }
  t92 = v0;
  t93 = v1;
  t94 = ((int64_t)(v2));
  t95 = ((int64_t)(v7));
  t96 = ((int64_t)(v3));
  t97 = ((float)(v5));
  t98 = ((int64_t)(v6));
  t99 = gg_function_12(t92, t93, t94, t95, t96, t97, t98, base + 0);
  (void)(t99);
  }
}
static void gg_function_13(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  _Alignas(8) int64_t t0;
  GG_UNALIGNED gg_unaligned_2 * t1;
  _Alignas(8) int64_t t2;
  GG_UNALIGNED gg_unaligned_1 * t3;
  _Alignas(8) int64_t t4;
  GG_UNALIGNED gg_unaligned_2 * t5;
  _Alignas(8) int64_t t6;
  GG_UNALIGNED gg_unaligned_2 * t7;
  _Alignas(8) int64_t t8;
  GG_UNALIGNED gg_unaligned_1 * t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_1 * t11;
  _Alignas(8) gg_slice t12;
  _Alignas(8) gg_slice t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) float t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) float v5;
  _Alignas(8) int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t0 = v4;
  t1 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t0, (v0).len);
  v5 = ((float)((*t1)));
  /* FG_DIRECT_SLICE_INDEX */
  t2 = v4;
  t3 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t2, (v1).len);
  v6 = ((int64_t)((*t3)));
  /* FG_DIRECT_SLICE_INDEX */
  t4 = v4;
  t5 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t4, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t6, (v0).len);
  (*t5) = ((float)((*t7)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v4;
  t9 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t8, (v1).len);
  /* FG_DIRECT_SLICE_INDEX */
  t10 = v2;
  t11 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t10, (v1).len);
  (*t9) = ((int64_t)((*t11)));
  t12 = v0;
  t13 = v1;
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) - (uint64_t)(t17)))));
  t19 = ((float)(v5));
  t20 = ((int64_t)(v6));
  gg_function_11(t12, t13, t14, t15, t18, t19, t20, base + 0);
  }
}
static void gg_function_14(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) uint8_t t2;
  uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) gg_slice t6;
  _Alignas(8) gg_slice t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  v4 = ((int64_t)(v3));
label1:;
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if(!t3) goto label2;
  {
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t4) - (uint64_t)(t5)))));
  t6 = v0;
  t7 = v1;
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(v4));
  gg_function_13(t6, t7, t8, t9, t10, base + 0);
  }
  goto label1;
label2:;
  }
}
static void gg_function_10(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) uint8_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  uint8_t t8;
  gg_slice t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  GG_UNALIGNED gg_unaligned_2 * t13;
  gg_slice t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  GG_UNALIGNED gg_unaligned_1 * t18;
  _Alignas(8) gg_slice t19;
  _Alignas(8) gg_slice t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) float t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) uint8_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) float v5;
  _Alignas(8) int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(2))));
  t2 = ((t0) < (t1));
  if (t2) {
  {
  goto gg_done;
  }
  }
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  v4 = ((int64_t)(((int64_t)gg_div(t5, t6))));
label1:;
  t7 = ((uint8_t)UINT64_C(1));
  t8 = t7;
  if(!t8) goto label2;
  {
  t9 = v0;
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(v4));
  t12 = ((int64_t)((uint64_t)(t10) + (uint64_t)(t11)));
  t13 = ((GG_UNALIGNED gg_unaligned_2 *)t9.data) + gg_index(t12, t9.len);
  v5 = ((float)((*t13)));
  t14 = v1;
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)((uint64_t)(t15) + (uint64_t)(t16)));
  t18 = ((GG_UNALIGNED gg_unaligned_1 *)t14.data) + gg_index(t17, t14.len);
  v6 = ((int64_t)((*t18)));
  t19 = v0;
  t20 = v1;
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(v3));
  t24 = ((float)(v5));
  t25 = ((int64_t)(v6));
  gg_function_11(t19, t20, t21, t22, t23, t24, t25, base + 0);
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v4));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t29) - (uint64_t)(t30)))));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_9(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) uint8_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) uint8_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) gg_slice t21;
  _Alignas(8) gg_slice t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) uint8_t t27;
  uint8_t t28;
  _Alignas(8) int64_t t29;
  GG_UNALIGNED gg_unaligned_2 * t30;
  _Alignas(8) float t31;
  _Alignas(8) int64_t t32;
  GG_UNALIGNED gg_unaligned_2 * t33;
  _Alignas(8) float t34;
  _Alignas(8) uint8_t t35;
  _Alignas(8) gg_slice t36;
  _Alignas(8) gg_slice t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) gg_slice t43;
  _Alignas(8) gg_slice t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(v2));
  t8 = ((t9) > (t10));
  }
  t11 = t8;
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)((v0).len));
  t14 = ((int64_t)(v2));
  t12 = ((t13) < (t14));
  }
  t15 = t12;
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)((v1).len));
  t18 = ((int64_t)(v2));
  t16 = ((t17) < (t18));
  }
  t19 = t16;
  if (t19) {
  {
  t20 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t20)))));
  goto gg_done;
  }
  }
  t21 = v0;
  t22 = v1;
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(v3));
  gg_function_10(t21, t22, t23, t24, base + 0);
  v4 = ((int64_t)(v3));
label1:;
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(v2));
  t27 = ((t25) < (t26));
  t28 = t27;
  if(!t28) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t29 = v4;
  t30 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t29, (v0).len);
  t31 = ((float)((*t30)));
  /* FG_DIRECT_SLICE_INDEX */
  t32 = ((int64_t)UINT64_C(0));
  t33 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t32, (v0).len);
  t34 = ((float)((*t33)));
  t35 = ((t31) > (t34));
  if (t35) {
  {
  t36 = v0;
  t37 = v1;
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((int64_t)(v3));
  t40 = ((int64_t)(v4));
  gg_function_13(t36, t37, t38, t39, t40, base + 0);
  }
  }
  t41 = ((int64_t)(v4));
  t42 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t42)))));
  }
  goto label1;
label2:;
  t43 = v0;
  t44 = v1;
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((int64_t)(v3));
  gg_function_14(t43, t44, t45, t46, base + 0);
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_33(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) uint8_t t10;
  _Alignas(8) gg_slice t11;
  _Alignas(8) gg_slice t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) uint8_t t20;
  uint8_t t21;
  _Alignas(8) int64_t t22;
  GG_UNALIGNED gg_unaligned_2 * t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v4));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  v5 = ((int64_t)(v4));
  t8 = ((int64_t)(v5));
  t9 = ((int64_t)(v3));
  t10 = ((t8) > (t9));
  if (t10) {
  {
  v5 = ((int64_t)(v3));
  }
  }
  t11 = v0;
  t12 = v2;
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(v3));
  v6 = gg_function_9(t11, t12, t13, t14, base + 0);
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) < (t16));
  if (t17) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t18 = ((int64_t)(v7));
  t19 = ((int64_t)(v5));
  t20 = ((t18) < (t19));
  t21 = t20;
  if(!t21) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t22 = v7;
  t23 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t22, (v1).len);
  (*t23) = ((float)(gg_double_bits(UINT64_C(0))));
  t24 = ((int64_t)(v7));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_26(gg_slice p0, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) uint8_t t2;
  uint8_t t3;
  _Alignas(8) int64_t t4;
  GG_UNALIGNED gg_unaligned_1 * t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  gg_slice t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  GG_UNALIGNED gg_unaligned_1 * t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) uint8_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  GG_UNALIGNED gg_unaligned_1 * t29;
  gg_slice t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  GG_UNALIGNED gg_unaligned_1 * t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  GG_UNALIGNED gg_unaligned_1 * t42;
  _Alignas(8) gg_slice v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(gg_globals.v5));
  t2 = ((t0) < (t1));
  t3 = t2;
  if(!t3) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t4 = v1;
  t5 = ((GG_UNALIGNED gg_unaligned_1 *)(v0).data) + gg_index(t4, (v0).len);
  t6 = ((int64_t)((*t5)));
  t7 = ((int64_t)(gg_globals.v8));
  t8 = ((int64_t)(((t6) & (t7))));
  t9 = v0;
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)(gg_globals.v5));
  t14 = ((int64_t)gg_mod(t12, t13));
  t15 = ((GG_UNALIGNED gg_unaligned_1 *)t9.data) + gg_index(t14, t9.len);
  t16 = ((int64_t)((*t15)));
  t17 = ((int64_t)(gg_globals.v9));
  t18 = ((int64_t)(((t16) & (t17))));
  v2 = ((int64_t)(((t8) | (t18))));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)(((t19) >> ((uint64_t)(t20) & 63))));
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  t23 = ((int64_t)(((t21) & (t22))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) != (t24));
  if (t25) {
  {
  t26 = ((int64_t)(v3));
  t27 = ((int64_t)(gg_globals.v7));
  v3 = ((int64_t)(((t26) ^ (t27))));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t28 = v1;
  t29 = ((GG_UNALIGNED gg_unaligned_1 *)(v0).data) + gg_index(t28, (v0).len);
  t30 = v0;
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(gg_globals.v6));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = ((int64_t)(gg_globals.v5));
  t35 = ((int64_t)gg_mod(t33, t34));
  t36 = ((GG_UNALIGNED gg_unaligned_1 *)t30.data) + gg_index(t35, t30.len);
  t37 = ((int64_t)((*t36)));
  t38 = ((int64_t)(v3));
  (*t29) = ((int64_t)(((t37) ^ (t38))));
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  }
  goto label1;
label2:;
  /* FG_DIRECT_SLICE_INDEX */
  t41 = ((int64_t)UINT64_C(624));
  t42 = ((GG_UNALIGNED gg_unaligned_1 *)(v0).data) + gg_index(t41, (v0).len);
  (*t42) = ((int64_t)(((int64_t)UINT64_C(0))));
  }
}
static int64_t gg_function_25(gg_slice p0, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  GG_UNALIGNED gg_unaligned_1 * t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) gg_slice t9;
  gg_slice t10;
  _Alignas(8) int64_t t11;
  GG_UNALIGNED gg_unaligned_1 * t12;
  _Alignas(8) int64_t t13;
  GG_UNALIGNED gg_unaligned_1 * t14;
  _Alignas(8) int64_t t15;
  GG_UNALIGNED gg_unaligned_1 * t16;
  _Alignas(8) int64_t t17;
  GG_UNALIGNED gg_unaligned_1 * t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) gg_slice v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)((v0).len));
  t2 = ((int64_t)(((int64_t)UINT64_C(625))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t4 = ((int64_t)UINT64_C(624));
  t5 = ((GG_UNALIGNED gg_unaligned_1 *)(v0).data) + gg_index(t4, (v0).len);
  t6 = ((int64_t)((*t5)));
  t7 = ((int64_t)(gg_globals.v5));
  t8 = ((t6) >= (t7));
  if (t8) {
  {
  t9 = v0;
  gg_function_26(t9, base + 0);
  }
  }
  t10 = v0;
  /* FG_DIRECT_SLICE_INDEX */
  t11 = ((int64_t)UINT64_C(624));
  t12 = ((GG_UNALIGNED gg_unaligned_1 *)(v0).data) + gg_index(t11, (v0).len);
  t13 = (*t12);
  t14 = ((GG_UNALIGNED gg_unaligned_1 *)t10.data) + gg_index(t13, t10.len);
  v1 = ((int64_t)((*t14)));
  /* FG_DIRECT_SLICE_INDEX */
  t15 = ((int64_t)UINT64_C(624));
  t16 = ((GG_UNALIGNED gg_unaligned_1 *)(v0).data) + gg_index(t15, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t17 = ((int64_t)UINT64_C(624));
  t18 = ((GG_UNALIGNED gg_unaligned_1 *)(v0).data) + gg_index(t17, (v0).len);
  t19 = ((int64_t)((*t18)));
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  (*t16) = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)UINT64_C(11))));
  t24 = ((int64_t)(((t22) >> ((uint64_t)(t23) & 63))));
  v1 = ((int64_t)(((t21) ^ (t24))));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(((int64_t)UINT64_C(7))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) << ((uint64_t)(t27) & 63)))));
  t29 = ((int64_t)(((int64_t)UINT64_C(2636928640))));
  t30 = ((int64_t)(((t28) & (t29))));
  v1 = ((int64_t)(((t25) ^ (t30))));
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(((int64_t)UINT64_C(15))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) << ((uint64_t)(t33) & 63)))));
  t35 = ((int64_t)(((int64_t)UINT64_C(4022730752))));
  t36 = ((int64_t)(((t34) & (t35))));
  v1 = ((int64_t)(((t31) ^ (t36))));
  t37 = ((int64_t)(v1));
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(((int64_t)UINT64_C(18))));
  t40 = ((int64_t)(((t38) >> ((uint64_t)(t39) & 63))));
  v1 = ((int64_t)(((t37) ^ (t40))));
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(gg_globals.v10));
  t0 = ((int64_t)(((t41) & (t42))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static double gg_function_28(gg_slice p0, int64_t base) {
  _Alignas(8) double t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) gg_slice t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) double t5;
  _Alignas(8) double t6;
  _Alignas(8) double t7;
  _Alignas(8) double t8;
  _Alignas(8) double t9;
  _Alignas(8) double t10;
  _Alignas(8) gg_slice v0;
  _Alignas(8) double v1;
  _Alignas(8) double v2;
  v0 = p0;
  {
  t1 = v0;
  t2 = gg_function_25(t1, base + 0);
  v1 = ((double)(((double)(t2))));
  t3 = v0;
  t4 = gg_function_25(t3, base + 0);
  v2 = ((double)(((double)(t4))));
  t5 = ((double)(v1));
  t6 = ((double)(v2));
  t7 = ((double)(gg_double_bits(UINT64_C(4751297606875873280))));
  t8 = ((double)(((t6) * (t7))));
  t9 = ((double)(((t5) + (t8))));
  t10 = ((double)(gg_double_bits(UINT64_C(4895412794951729152))));
  t0 = ((double)(((t9) / (t10))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_31(gg_slice p0, int64_t p1, gg_slice p2, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  uint8_t t4;
  _Alignas(8) double t5;
  _Alignas(8) int64_t t6;
  GG_UNALIGNED gg_unaligned_2 * t7;
  _Alignas(8) double t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) gg_slice t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) uint8_t t14;
  uint8_t t15;
  _Alignas(8) double t16;
  _Alignas(8) int64_t t17;
  GG_UNALIGNED gg_unaligned_2 * t18;
  _Alignas(8) double t19;
  _Alignas(8) double t20;
  _Alignas(8) double t21;
  _Alignas(8) double t22;
  _Alignas(8) double t23;
  _Alignas(8) uint8_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) gg_slice v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) double v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) double v5;
  _Alignas(8) double v6;
  _Alignas(8) double v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((double)(gg_double_bits(UINT64_C(0))));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v4));
  t2 = ((int64_t)(v1));
  t3 = ((t1) < (t2));
  t4 = t3;
  if(!t4) goto label2;
  {
  t5 = ((double)(v3));
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v4;
  t7 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t6, (v0).len);
  t8 = ((double)(((double)((*t7)))));
  v3 = ((double)(((t5) + (t8))));
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  }
  goto label1;
label2:;
  t11 = v2;
  v5 = gg_function_28(t11, base + 0);
  v6 = ((double)(gg_double_bits(UINT64_C(0))));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(v1));
  t14 = ((t12) < (t13));
  t15 = t14;
  if(!t15) goto label5;
  {
  t16 = ((double)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t17 = v4;
  t18 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t17, (v0).len);
  t19 = ((double)(((double)((*t18)))));
  v6 = ((double)(((t16) + (t19))));
  t20 = ((double)(v6));
  t21 = ((double)(v3));
  v7 = ((double)(((t20) / (t21))));
  t22 = ((double)(v7));
  t23 = ((double)(v5));
  t24 = ((t22) >= (t23));
  if (t24) {
  {
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  }
  goto label4;
label5:;
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t27) - (uint64_t)(t28)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_42(gg_slice p0, int64_t p1, gg_slice p2, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) gg_slice t3;
  _Alignas(8) gg_slice v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) gg_slice v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  t3 = v2;
  t0 = gg_function_31(t1, t2, t3, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_32(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) int64_t t8;
  GG_UNALIGNED gg_unaligned_2 * t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) uint8_t t12;
  uint8_t t13;
  _Alignas(8) int64_t t14;
  GG_UNALIGNED gg_unaligned_2 * t15;
  _Alignas(8) float t16;
  _Alignas(8) float t17;
  _Alignas(8) uint8_t t18;
  _Alignas(8) int64_t t19;
  GG_UNALIGNED gg_unaligned_2 * t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) float t23;
  _Alignas(8) float t24;
  _Alignas(8) float t25;
  _Alignas(8) float t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) uint8_t t29;
  uint8_t t30;
  _Alignas(8) int64_t t31;
  GG_UNALIGNED gg_unaligned_2 * t32;
  _Alignas(8) float t33;
  _Alignas(8) float t34;
  _Alignas(8) uint8_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) uint8_t t42;
  _Alignas(8) uint8_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) uint8_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) uint8_t t49;
  uint8_t t50;
  _Alignas(8) int64_t t51;
  GG_UNALIGNED gg_unaligned_2 * t52;
  _Alignas(8) float t53;
  _Alignas(8) float t54;
  _Alignas(8) uint8_t t55;
  _Alignas(8) int64_t t56;
  GG_UNALIGNED gg_unaligned_2 * t57;
  _Alignas(8) int64_t t58;
  GG_UNALIGNED gg_unaligned_2 * t59;
  _Alignas(8) int64_t t60;
  GG_UNALIGNED gg_unaligned_1 * t61;
  _Alignas(8) int64_t t62;
  GG_UNALIGNED gg_unaligned_1 * t63;
  _Alignas(8) int64_t t64;
  GG_UNALIGNED gg_unaligned_2 * t65;
  _Alignas(8) int64_t t66;
  GG_UNALIGNED gg_unaligned_2 * t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) float v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) float v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((float)(v4));
  t2 = ((float)(gg_double_bits(UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t8, (v0).len);
  v6 = ((float)((*t9)));
  v7 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t10 = ((int64_t)(v7));
  t11 = ((int64_t)(v3));
  t12 = ((t10) < (t11));
  t13 = t12;
  if(!t13) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t14 = v7;
  t15 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t14, (v0).len);
  t16 = ((float)((*t15)));
  t17 = ((float)(v6));
  t18 = ((t16) > (t17));
  if (t18) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t19 = v7;
  t20 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t19, (v0).len);
  v6 = ((float)((*t20)));
  }
  }
  t21 = ((int64_t)(v7));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  }
  goto label1;
label2:;
  t23 = ((float)(v6));
  t24 = ((float)(v4));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t25 = logf(t24);
  t26 = ((float)(t25));
  v8 = ((float)(((t23) + (t26))));
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t27 = ((int64_t)(v7));
  t28 = ((int64_t)(v3));
  t29 = ((t27) < (t28));
  t30 = t29;
  if(!t30) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = v7;
  t32 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t31, (v0).len);
  t33 = ((float)((*t32)));
  t34 = ((float)(v8));
  t35 = ((t33) >= (t34));
  if (t35) {
  {
  t36 = ((int64_t)(v9));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t37)))));
  }
  }
  t38 = ((int64_t)(v7));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  }
  goto label4;
label5:;
  t40 = ((int64_t)(v9));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t40) == (t41));
  t43 = t42;
  if (!t43) {
  t44 = ((int64_t)(v9));
  t45 = ((int64_t)(v5));
  t43 = ((t44) < (t45));
  }
  t46 = t43;
  if (t46) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t47 = ((int64_t)(v7));
  t48 = ((int64_t)(v3));
  t49 = ((t47) < (t48));
  t50 = t49;
  if(!t50) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t51 = v7;
  t52 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t51, (v0).len);
  t53 = ((float)((*t52)));
  t54 = ((float)(v8));
  t55 = ((t53) >= (t54));
  if (t55) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t56 = v10;
  t57 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t56, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t58 = v7;
  t59 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t58, (v0).len);
  (*t57) = ((float)((*t59)));
  /* FG_DIRECT_SLICE_INDEX */
  t60 = v10;
  t61 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t60, (v2).len);
  /* FG_DIRECT_SLICE_INDEX */
  t62 = v7;
  t63 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t62, (v2).len);
  (*t61) = ((int64_t)((*t63)));
  /* FG_DIRECT_SLICE_INDEX */
  t64 = v10;
  t65 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t64, (v1).len);
  /* FG_DIRECT_SLICE_INDEX */
  t66 = v7;
  t67 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t66, (v1).len);
  (*t65) = ((float)((*t67)));
  t68 = ((int64_t)(v10));
  t69 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t69)))));
  }
  }
  t70 = ((int64_t)(v7));
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  }
  goto label7;
label8:;
  t0 = ((int64_t)(v9));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_21(gg_slice p0, int64_t p1, float p2, int64_t p3, float p4, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) uint8_t t12;
  uint8_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) float t16;
  _Alignas(8) float t17;
  _Alignas(8) float t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) float t21;
  _Alignas(8) float t22;
  _Alignas(8) float t23;
  _Alignas(8) int64_t t24;
  GG_UNALIGNED gg_unaligned_2 * t25;
  _Alignas(8) float t26;
  gg_slice t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  GG_UNALIGNED gg_unaligned_2 * t31;
  _Alignas(8) float t32;
  _Alignas(8) float t33;
  _Alignas(8) float t34;
  _Alignas(8) float t35;
  _Alignas(8) float t36;
  _Alignas(8) float t37;
  _Alignas(8) float t38;
  _Alignas(8) float t39;
  _Alignas(8) float t40;
  _Alignas(8) float t41;
  _Alignas(8) float t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) float t45;
  _Alignas(8) float t46;
  _Alignas(8) float t47;
  _Alignas(8) float t48;
  _Alignas(8) float t49;
  _Alignas(8) float t50;
  _Alignas(8) float t51;
  _Alignas(8) float t52;
  _Alignas(8) float t53;
  _Alignas(8) float t54;
  _Alignas(8) float t55;
  _Alignas(8) float t56;
  _Alignas(8) float t57;
  _Alignas(8) float t58;
  _Alignas(8) float t59;
  _Alignas(8) float t60;
  _Alignas(8) float t61;
  _Alignas(8) float t62;
  _Alignas(8) float t63;
  _Alignas(8) float t64;
  _Alignas(8) float t65;
  _Alignas(8) float t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) float t69;
  _Alignas(8) float t70;
  _Alignas(8) uint8_t t71;
  _Alignas(8) uint8_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) uint8_t t75;
  uint8_t t76;
  _Alignas(8) int64_t t77;
  _Alignas(8) int64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) int64_t t80;
  _Alignas(8) uint8_t t81;
  _Alignas(8) gg_slice v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) float v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  _Alignas(8) float v5;
  _Alignas(8) float v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) float v9;
  _Alignas(8) float v10;
  _Alignas(8) float v11;
  _Alignas(8) float v12;
  _Alignas(8) float v13;
  _Alignas(8) int64_t v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  v5 = ((float)(gg_double_bits(UINT64_C(0))));
  v6 = ((float)(gg_double_bits(UINT64_C(0))));
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t1) - (uint64_t)(t2)))));
  t3 = ((int64_t)(v7));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) - (uint64_t)(t5)))));
  t7 = ((t3) > (t6));
  if (t7) {
  {
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t8) - (uint64_t)(t9)))));
  }
  }
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t10 = ((int64_t)(v8));
  t11 = ((int64_t)(v7));
  t12 = ((t10) < (t11));
  t13 = t12;
  if(!t13) goto label2;
  {
  t14 = ((int64_t)(v8));
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  t16 = ((float)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t17 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t18 = ((float)(((t16) * (t17))));
  t19 = ((int64_t)(v8));
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  t21 = ((float)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t22 = ((float)(((t18) / (t21))));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t23 = logf(t22);
  v9 = ((float)(t23));
  /* FG_DIRECT_SLICE_INDEX */
  t24 = v8;
  t25 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t24, (v0).len);
  t26 = ((float)((*t25)));
  t27 = v0;
  t28 = ((int64_t)(v8));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((int64_t)((uint64_t)(t28) + (uint64_t)(t29)));
  t31 = ((GG_UNALIGNED gg_unaligned_2 *)t27.data) + gg_index(t30, t27.len);
  t32 = ((float)((*t31)));
  t33 = ((float)(((t26) / (t32))));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t34 = logf(t33);
  v10 = ((float)(t34));
  t35 = ((float)(v5));
  t36 = ((float)(v9));
  t37 = ((float)(v10));
  t38 = ((float)(((t36) * (t37))));
  v5 = ((float)(((t35) + (t38))));
  t39 = ((float)(v6));
  t40 = ((float)(v9));
  t41 = ((float)(v9));
  t42 = ((float)(((t40) * (t41))));
  v6 = ((float)(((t39) + (t42))));
  t43 = ((int64_t)(v8));
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t44)))));
  }
  goto label1;
label2:;
  t45 = ((float)(v5));
  t46 = ((float)(v6));
  v11 = ((float)(((t45) / (t46))));
  t47 = ((float)(v11));
  t48 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  v12 = ((float)(((t47) - (t48))));
  t49 = ((float)(v12));
  t50 = ((float)(gg_double_bits(UINT64_C(4611686018427387904))));
  t51 = ((float)(v2));
#ifndef __cplusplus
  extern float (powf)(float, float);
#endif
  t52 = powf(t50, t51);
  t53 = ((float)(t52));
  t54 = ((float)(((t49) * (t53))));
  t55 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t56 = ((float)(v4));
  t57 = v12;
  t58 = ((float)((-(t57))));
#ifndef __cplusplus
  extern float (powf)(float, float);
#endif
  t59 = powf(t56, t58);
  t60 = ((float)(t59));
  t61 = ((float)(((t55) - (t60))));
  t62 = ((float)(((t54) / (t61))));
  t63 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t64 = ((float)(v11));
  t65 = ((float)(((t63) / (t64))));
#ifndef __cplusplus
  extern float (powf)(float, float);
#endif
  t66 = powf(t62, t65);
  v13 = ((float)(t66));
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t67 = ((int64_t)(v14));
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  t69 = ((float)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  t70 = ((float)(v13));
  t71 = ((t69) <= (t70));
  t72 = t71;
  if (t72) {
  t73 = ((int64_t)(v14));
  t74 = ((int64_t)(((int64_t)UINT64_C(1000000))));
  t72 = ((t73) < (t74));
  }
  t75 = t72;
  t76 = t75;
  if(!t76) goto label5;
  {
  t77 = ((int64_t)(v14));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  }
  goto label4;
label5:;
  t79 = ((int64_t)(v14));
  t80 = ((int64_t)(((int64_t)UINT64_C(1))));
  t81 = ((t79) < (t80));
  if (t81) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t0 = ((int64_t)(v14));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_41(gg_slice p0, int64_t p1, float p2, int64_t p3, float p4, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) float t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) float t5;
  _Alignas(8) gg_slice v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) float v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  t3 = ((float)(v2));
  t4 = ((int64_t)(v3));
  t5 = ((float)(v4));
  t0 = gg_function_21(t1, t2, t3, t4, t5, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_20(float p0, float p1, float p2, gg_slice p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) float t4;
  _Alignas(8) float t5;
  _Alignas(8) uint8_t t6;
  _Alignas(8) float t7;
  _Alignas(8) float t8;
  _Alignas(8) uint8_t t9;
  _Alignas(8) float t10;
  _Alignas(8) float t11;
  _Alignas(8) float t12;
  _Alignas(8) float t13;
  _Alignas(8) float t14;
  _Alignas(8) float t15;
  _Alignas(8) float t16;
  _Alignas(8) float t17;
  _Alignas(8) uint8_t t18;
  _Alignas(8) float t19;
  _Alignas(8) float t20;
  _Alignas(8) uint8_t t21;
  _Alignas(8) int64_t t22;
  GG_UNALIGNED gg_unaligned_2 * t23;
  _Alignas(8) float v0;
  _Alignas(8) float v1;
  _Alignas(8) float v2;
  _Alignas(8) gg_slice v3;
  _Alignas(8) float v4;
  _Alignas(8) float v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  v4 = ((float)(v0));
  t1 = ((float)(v4));
  t2 = ((float)(gg_double_bits(UINT64_C(0))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  v4 = ((float)(gg_double_bits(UINT64_C(0))));
  }
  }
  t4 = ((float)(v4));
  t5 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t6 = ((t4) > (t5));
  if (t6) {
  {
  v4 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  }
  }
  v5 = ((float)(v4));
  t7 = ((float)(v2));
  t8 = ((float)(gg_double_bits(UINT64_C(0))));
  t9 = ((t7) != (t8));
  if (t9) {
  {
  t10 = ((float)(gg_double_bits(UINT64_C(4611686018427387904))));
  t11 = ((float)(v4));
  t12 = ((float)(((t10) * (t11))));
  t13 = ((float)(v1));
  t14 = ((float)(v2));
  t15 = ((float)(((t13) / (t14))));
  v5 = ((float)(((t12) - (t15))));
  }
  }
  t16 = ((float)(v5));
  t17 = ((float)(gg_double_bits(UINT64_C(0))));
  t18 = ((t16) < (t17));
  if (t18) {
  {
  v5 = ((float)(gg_double_bits(UINT64_C(0))));
  }
  }
  t19 = ((float)(v5));
  t20 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t21 = ((t19) > (t20));
  if (t21) {
  {
  v5 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t22 = ((int64_t)UINT64_C(0));
  t23 = ((GG_UNALIGNED gg_unaligned_2 *)(v3).data) + gg_index(t22, (v3).len);
  (*t23) = ((float)(v5));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static double gg_function_5(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base) {
  _Alignas(8) double t0;
  _Alignas(8) int64_t t1;
  GG_UNALIGNED gg_unaligned_2 * t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) uint8_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint8_t t8;
  uint8_t t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_2 * t11;
  _Alignas(8) float t12;
  _Alignas(8) float t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) int64_t t15;
  GG_UNALIGNED gg_unaligned_2 * t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) uint8_t t21;
  uint8_t t22;
  _Alignas(8) int64_t t23;
  GG_UNALIGNED gg_unaligned_2 * t24;
  _Alignas(8) float t25;
  _Alignas(8) float t26;
  _Alignas(8) float t27;
  _Alignas(8) float t28;
  _Alignas(8) int64_t t29;
  GG_UNALIGNED gg_unaligned_2 * t30;
  _Alignas(8) float t31;
  _Alignas(8) float t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) uint8_t t37;
  uint8_t t38;
  _Alignas(8) int64_t t39;
  GG_UNALIGNED gg_unaligned_2 * t40;
  _Alignas(8) int64_t t41;
  GG_UNALIGNED gg_unaligned_2 * t42;
  _Alignas(8) float t43;
  _Alignas(8) float t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) float v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) float v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1 = ((int64_t)UINT64_C(0));
  t2 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t1, (v0).len);
  v4 = ((float)((*t2)));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t6 = ((int64_t)(v5));
  t7 = ((int64_t)(v2));
  t8 = ((t6) < (t7));
  t9 = t8;
  if(!t9) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t10 = v5;
  t11 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t10, (v0).len);
  t12 = ((float)((*t11)));
  t13 = ((float)(v4));
  t14 = ((t12) > (t13));
  if (t14) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t15 = v5;
  t16 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t15, (v0).len);
  v4 = ((float)((*t16)));
  }
  }
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  }
  goto label1;
label2:;
  }
  }
  v6 = ((float)(gg_double_bits(UINT64_C(0))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t19 = ((int64_t)(v7));
  t20 = ((int64_t)(v2));
  t21 = ((t19) < (t20));
  t22 = t21;
  if(!t22) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t23 = v7;
  t24 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t23, (v0).len);
  t25 = ((float)((*t24)));
  t26 = ((float)(v4));
  t27 = ((float)(((t25) - (t26))));
#ifndef __cplusplus
  extern float (expf)(float);
#endif
  t28 = expf(t27);
  v8 = ((float)(t28));
  /* FG_DIRECT_SLICE_INDEX */
  t29 = v7;
  t30 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t29, (v1).len);
  (*t30) = ((float)(v8));
  t31 = ((float)(v6));
  t32 = ((float)(v8));
  v6 = ((float)(((t31) + (t32))));
  t33 = ((int64_t)(v7));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
  }
  goto label4;
label5:;
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t35 = ((int64_t)(v7));
  t36 = ((int64_t)(v2));
  t37 = ((t35) < (t36));
  t38 = t37;
  if(!t38) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t39 = v7;
  t40 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t39, (v1).len);
  /* FG_DIRECT_SLICE_INDEX */
  t41 = v7;
  t42 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t41, (v1).len);
  t43 = ((float)((*t42)));
  t44 = ((float)(v6));
  (*t40) = ((float)(((t43) / (t44))));
  t45 = ((int64_t)(v7));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  }
  goto label7;
label8:;
  t0 = ((double)(((double)(v6))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_19(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) double t1;
  _Alignas(8) double t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) uint8_t t5;
  uint8_t t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_2 * t8;
  _Alignas(8) float t9;
  _Alignas(8) double t10;
  _Alignas(8) float t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  GG_UNALIGNED gg_unaligned_2 * t16;
  _Alignas(8) float t17;
  _Alignas(8) float t18;
  _Alignas(8) float t19;
  _Alignas(8) float t20;
  _Alignas(8) float t21;
  _Alignas(8) float t22;
  _Alignas(8) uint8_t t23;
  _Alignas(8) float t24;
  _Alignas(8) int64_t t25;
  GG_UNALIGNED gg_unaligned_2 * t26;
  _Alignas(8) float t27;
  _Alignas(8) float t28;
  _Alignas(8) float t29;
  _Alignas(8) float t30;
  _Alignas(8) float t31;
  _Alignas(8) float t32;
  _Alignas(8) float t33;
  _Alignas(8) float t34;
  _Alignas(8) float t35;
  _Alignas(8) float t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) gg_slice t39;
  _Alignas(8) gg_slice t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) double t43;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  _Alignas(8) float v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) float v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((double)(gg_double_bits(UINT64_C(4607182418800017408))));
  t2 = ((double)(gg_double_bits(UINT64_C(4599075939470750515))));
  v5 = ((float)(((double)gg_double_bits(UINT64_C(4614688418178968235)))));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v6));
  t4 = ((int64_t)(v3));
  t5 = ((t3) < (t4));
  t6 = t5;
  if(!t6) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t7 = v6;
  t8 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t7, (v0).len);
  t9 = ((float)((*t8)));
  t10 = gg_double_bits(UINT64_C(5175426328141668785));
  t11 = ((float)((-(t10))));
  t12 = ((t9) < (t11));
  if (t12) {
  {
  t13 = ((int64_t)(v6));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  goto label1;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t15 = v6;
  t16 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t15, (v1).len);
  t17 = ((float)((*t16)));
  t18 = ((float)(v4));
  t19 = ((float)(((t17) - (t18))));
  t20 = ((float)(v5));
  v7 = ((float)(((t19) * (t20))));
  t21 = ((float)(v7));
  t22 = ((float)(gg_double_bits(UINT64_C(0))));
  t23 = ((t21) < (t22));
  if (t23) {
  {
  t24 = v7;
  v7 = ((float)((-(t24))));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v6;
  t26 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t25, (v0).len);
  t27 = ((float)(gg_double_bits(UINT64_C(4617315517961601024))));
  t28 = ((float)(gg_double_bits(UINT64_C(4621819117588971520))));
  t29 = ((float)(v7));
  t30 = ((float)(((t28) * (t29))));
  t31 = ((float)(v7));
  t32 = ((float)(((t30) * (t31))));
  t33 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t34 = ((float)(v7));
  t35 = ((float)(((t33) + (t34))));
  t36 = ((float)(((t32) / (t35))));
  (*t26) = ((float)(((t27) - (t36))));
  t37 = ((int64_t)(v6));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  }
  goto label1;
label2:;
  t39 = v0;
  t40 = v1;
  t41 = ((int64_t)(v3));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = gg_function_5(t39, t40, t41, t42, base + 0);
  (void)(t43);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_40(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) gg_slice t2;
  _Alignas(8) gg_slice t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) float t5;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = v0;
  t2 = v1;
  t3 = v2;
  t4 = ((int64_t)(v3));
  t5 = ((float)(v4));
  t0 = gg_function_19(t1, t2, t3, t4, t5, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_39(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, float p10, float p11, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) float t4;
  _Alignas(8) float t5;
  _Alignas(8) float t6;
  _Alignas(8) float t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) uint8_t t10;
  uint8_t t11;
  _Alignas(8) int64_t t12;
  GG_UNALIGNED gg_unaligned_1 * t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) uint8_t t19;
  uint8_t t20;
  _Alignas(8) int64_t t21;
  GG_UNALIGNED gg_unaligned_1 * t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) uint8_t t25;
  gg_slice t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  GG_UNALIGNED gg_unaligned_1 * t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) uint8_t t35;
  _Alignas(8) int64_t t36;
  GG_UNALIGNED gg_unaligned_1 * t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) uint8_t t40;
  _Alignas(8) int64_t t41;
  GG_UNALIGNED gg_unaligned_1 * t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) uint8_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) uint8_t t50;
  uint8_t t51;
  _Alignas(8) int64_t t52;
  GG_UNALIGNED gg_unaligned_1 * t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) uint8_t t56;
  _Alignas(8) uint8_t t57;
  _Alignas(8) int64_t t58;
  GG_UNALIGNED gg_unaligned_1 * t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) uint8_t t62;
  _Alignas(8) int64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) uint8_t t65;
  _Alignas(8) uint8_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) uint8_t t71;
  uint8_t t72;
  _Alignas(8) float t73;
  _Alignas(8) float t74;
  _Alignas(8) uint8_t t75;
  _Alignas(8) uint8_t t76;
  _Alignas(8) float t77;
  _Alignas(8) float t78;
  _Alignas(8) float t79;
  _Alignas(8) float t80;
  _Alignas(8) uint8_t t81;
  _Alignas(8) float t82;
  _Alignas(8) float t83;
  _Alignas(8) int64_t t84;
  _Alignas(8) int64_t t85;
  _Alignas(8) float t86;
  _Alignas(8) float t87;
  _Alignas(8) float t88;
  _Alignas(8) float t89;
  _Alignas(8) float t90;
  _Alignas(8) int64_t t91;
  GG_UNALIGNED gg_unaligned_2 * t92;
  _Alignas(8) int64_t t93;
  GG_UNALIGNED gg_unaligned_2 * t94;
  _Alignas(8) float t95;
  _Alignas(8) float t96;
  _Alignas(8) int64_t t97;
  _Alignas(8) int64_t t98;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) gg_slice v3;
  _Alignas(8) gg_slice v4;
  _Alignas(8) gg_slice v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) float v10;
  _Alignas(8) float v11;
  _Alignas(8) float v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) int64_t v14;
  _Alignas(8) int64_t v15;
  _Alignas(8) int64_t v16;
  _Alignas(8) uint8_t v17;
  _Alignas(8) int64_t v18;
  _Alignas(8) float v19;
  _Alignas(8) int64_t v20;
  _Alignas(8) float v21;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  v9 = p9;
  v10 = p10;
  v11 = p11;
  {
  v12 = ((float)(gg_double_bits(UINT64_C(0))));
  t1 = ((float)(v11));
  t2 = ((float)(gg_double_bits(UINT64_C(4607182423303617035))));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t4 = ((float)(gg_double_bits(UINT64_C(4635943731704213833))));
  t5 = ((float)(v11));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t6 = logf(t5);
  t7 = ((float)(t6));
  v12 = ((float)(((t4) / (t7))));
  }
  }
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t8 = ((int64_t)(v13));
  t9 = ((int64_t)(v7));
  t10 = ((t8) < (t9));
  t11 = t10;
  if(!t11) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t12 = v13;
  t13 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t12, (v3).len);
  v14 = ((int64_t)((*t13)));
  t14 = ((int64_t)UINT64_C(1));
  v15 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t14)))));
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t15 = ((int64_t)(v16));
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) - (uint64_t)(t17)))));
  t19 = ((t15) < (t18));
  t20 = t19;
  if(!t20) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t21 = v16;
  t22 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t21, (v1).len);
  t23 = ((int64_t)((*t22)));
  t24 = ((int64_t)(v8));
  t25 = ((t23) >= (t24));
  if (t25) {
  {
  t26 = v0;
  t27 = ((int64_t)(v6));
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) - (uint64_t)(t28)))));
  t30 = ((int64_t)(v16));
  t31 = ((int64_t)((uint64_t)(t29) - (uint64_t)(t30)));
  t32 = ((GG_UNALIGNED gg_unaligned_1 *)t26.data) + gg_index(t31, t26.len);
  t33 = ((int64_t)((*t32)));
  t34 = ((int64_t)(v14));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t36 = v16;
  t37 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t36, (v1).len);
  t38 = ((int64_t)((*t37)));
  t39 = ((int64_t)(v15));
  t40 = ((t38) > (t39));
  if (t40) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t41 = v16;
  t42 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t41, (v1).len);
  v15 = ((int64_t)((*t42)));
  }
  }
  }
  }
  }
  }
  t43 = ((int64_t)(v16));
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t44)))));
  }
  goto label4;
label5:;
  t45 = ((int64_t)(v15));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((t45) >= (t46));
  if (t47) {
  {
  v17 = ((uint8_t)UINT64_C(0));
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t48 = ((int64_t)(v18));
  t49 = ((int64_t)(v9));
  t50 = ((t48) < (t49));
  t51 = t50;
  if(!t51) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t52 = v18;
  t53 = ((GG_UNALIGNED gg_unaligned_1 *)(v4).data) + gg_index(t52, (v4).len);
  t54 = ((int64_t)((*t53)));
  t55 = ((int64_t)(v14));
  t56 = ((t54) == (t55));
  t57 = t56;
  if (t57) {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = v18;
  t59 = ((GG_UNALIGNED gg_unaligned_1 *)(v5).data) + gg_index(t58, (v5).len);
  t60 = ((int64_t)((*t59)));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t60) == (t61));
  }
  t62 = t57;
  if (t62) {
  {
  v17 = ((uint8_t)UINT64_C(1));
  goto label8;
  }
  }
  t63 = ((int64_t)(v18));
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t64)))));
  }
  goto label7;
label8:;
  t65 = v17;
  t66 = (!(t65));
  if (t66) {
  {
  v19 = ((float)(gg_double_bits(UINT64_C(0))));
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t67 = ((int64_t)(v20));
  t68 = ((int64_t)(v15));
  t69 = ((int64_t)(v8));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t68) - (uint64_t)(t69)))));
  t71 = ((t67) < (t70));
  t72 = t71;
  if(!t72) goto label11;
  {
  t73 = ((float)(v12));
  t74 = ((float)(gg_double_bits(UINT64_C(0))));
  t75 = ((t73) > (t74));
  t76 = t75;
  if (t76) {
  t77 = ((float)(v19));
  t78 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t79 = ((float)(((t77) + (t78))));
  t80 = ((float)(v12));
  t76 = ((t79) > (t80));
  }
  t81 = t76;
  if (t81) {
  {
  goto label11;
  }
  }
  t82 = ((float)(v19));
  t83 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  v19 = ((float)(((t82) + (t83))));
  t84 = ((int64_t)(v20));
  t85 = ((int64_t)(((int64_t)UINT64_C(1))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t85)))));
  }
  goto label10;
label11:;
  t86 = ((float)(v10));
  t87 = ((float)(v11));
  t88 = ((float)(v19));
#ifndef __cplusplus
  extern float (powf)(float, float);
#endif
  t89 = powf(t87, t88);
  t90 = ((float)(t89));
  v21 = ((float)(((t86) * (t90))));
  /* FG_DIRECT_SLICE_INDEX */
  t91 = v13;
  t92 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t91, (v2).len);
  /* FG_DIRECT_SLICE_INDEX */
  t93 = v13;
  t94 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t93, (v2).len);
  t95 = ((float)((*t94)));
  t96 = ((float)(v21));
  (*t92) = ((float)(((t95) - (t96))));
  }
  }
  }
  }
  t97 = ((int64_t)(v13));
  t98 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t97) + (uint64_t)(t98)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_38(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, float p10, float p11, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) gg_slice t2;
  _Alignas(8) gg_slice t3;
  _Alignas(8) gg_slice t4;
  _Alignas(8) gg_slice t5;
  _Alignas(8) gg_slice t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) float t11;
  _Alignas(8) float t12;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) gg_slice v3;
  _Alignas(8) gg_slice v4;
  _Alignas(8) gg_slice v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) float v10;
  _Alignas(8) float v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  v9 = p9;
  v10 = p10;
  v11 = p11;
  {
  t1 = v0;
  t2 = v1;
  t3 = v2;
  t4 = v3;
  t5 = v4;
  t6 = v5;
  t7 = ((int64_t)(v6));
  t8 = ((int64_t)(v7));
  t9 = ((int64_t)(v8));
  t10 = ((int64_t)(v9));
  t11 = ((float)(v10));
  t12 = ((float)(v11));
  t0 = gg_function_39(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_37(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) uint8_t t5;
  uint8_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) uint8_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) uint8_t t15;
  gg_slice t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  GG_UNALIGNED gg_unaligned_1 * t22;
  _Alignas(8) int64_t t23;
  gg_slice t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  GG_UNALIGNED gg_unaligned_1 * t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) uint8_t t34;
  uint8_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) uint8_t t40;
  gg_slice t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  GG_UNALIGNED gg_unaligned_1 * t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) uint8_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  gg_slice t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) int64_t t62;
  GG_UNALIGNED gg_unaligned_1 * t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) uint8_t t66;
  gg_slice t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  GG_UNALIGNED gg_unaligned_1 * t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) uint8_t t74;
  gg_slice t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) int64_t t77;
  _Alignas(8) int64_t t78;
  GG_UNALIGNED gg_unaligned_1 * t79;
  _Alignas(8) int64_t t80;
  _Alignas(8) int64_t t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) uint8_t t84;
  _Alignas(8) uint8_t t85;
  gg_slice t86;
  _Alignas(8) int64_t t87;
  _Alignas(8) int64_t t88;
  _Alignas(8) int64_t t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  GG_UNALIGNED gg_unaligned_1 * t92;
  _Alignas(8) int64_t t93;
  gg_slice t94;
  _Alignas(8) int64_t t95;
  _Alignas(8) int64_t t96;
  _Alignas(8) int64_t t97;
  _Alignas(8) int64_t t98;
  _Alignas(8) int64_t t99;
  _Alignas(8) int64_t t100;
  _Alignas(8) int64_t t101;
  GG_UNALIGNED gg_unaligned_1 * t102;
  _Alignas(8) int64_t t103;
  _Alignas(8) uint8_t t104;
  uint8_t t105;
  _Alignas(8) int64_t t106;
  _Alignas(8) int64_t t107;
  _Alignas(8) int64_t t108;
  _Alignas(8) int64_t t109;
  _Alignas(8) int64_t t110;
  _Alignas(8) int64_t t111;
  _Alignas(8) uint8_t t112;
  gg_slice t113;
  _Alignas(8) int64_t t114;
  _Alignas(8) int64_t t115;
  _Alignas(8) int64_t t116;
  GG_UNALIGNED gg_unaligned_1 * t117;
  _Alignas(8) int64_t t118;
  _Alignas(8) int64_t t119;
  _Alignas(8) int64_t t120;
  _Alignas(8) int64_t t121;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) int64_t v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t1) - (uint64_t)(t2)))));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t3 = ((int64_t)(v7));
  t4 = ((int64_t)(v2));
  t5 = ((t3) < (t4));
  t6 = t5;
  if(!t6) goto label2;
  {
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v5));
  t9 = ((t7) > (t8));
  if (t9) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t10 = ((int64_t)(v8));
  t11 = ((int64_t)(v7));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)(v2));
  t14 = ((t12) < (t13));
  t15 = t14;
  if (t15) {
  t16 = v0;
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) - (uint64_t)(t18)))));
  t20 = ((int64_t)(v8));
  t21 = ((int64_t)((uint64_t)(t19) - (uint64_t)(t20)));
  t22 = ((GG_UNALIGNED gg_unaligned_1 *)t16.data) + gg_index(t21, t16.len);
  t23 = ((int64_t)((*t22)));
  t24 = v0;
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) - (uint64_t)(t26)))));
  t28 = ((int64_t)(v8));
  t29 = ((int64_t)(v7));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)((uint64_t)(t27) - (uint64_t)(t30)));
  t32 = ((GG_UNALIGNED gg_unaligned_1 *)t24.data) + gg_index(t31, t24.len);
  t33 = ((int64_t)((*t32)));
  t15 = ((t23) == (t33));
  }
  t34 = t15;
  t35 = t34;
  if(!t35) goto label5;
  {
  t36 = ((int64_t)(v8));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t37)))));
  }
  goto label4;
label5:;
  v9 = ((int64_t)(v8));
  t38 = ((int64_t)(v9));
  t39 = ((int64_t)(v3));
  t40 = ((t38) > (t39));
  if (t40) {
  {
  v9 = ((int64_t)(v3));
  }
  }
  t41 = v1;
  t42 = ((int64_t)(v4));
  t43 = ((int64_t)(v7));
  t44 = ((int64_t)((uint64_t)(t42) - (uint64_t)(t43)));
  t45 = ((GG_UNALIGNED gg_unaligned_1 *)t41.data) + gg_index(t44, t41.len);
  (*t45) = ((int64_t)(v9));
  t46 = ((int64_t)(v8));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((t46) > (t47));
  if (t48) {
  {
  v6 = ((int64_t)(v7));
  t49 = ((int64_t)(v7));
  t50 = ((int64_t)(v8));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t50)))));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t51) - (uint64_t)(t52)))));
  }
  }
  }
  } else {
  {
  t53 = ((int64_t)(v7));
  t54 = ((int64_t)(v6));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t53) - (uint64_t)(t54)))));
  t55 = ((int64_t)(v5));
  t56 = ((int64_t)(v7));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) - (uint64_t)(t56)))));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t59 = v1;
  t60 = ((int64_t)(v4));
  t61 = ((int64_t)(v10));
  t62 = ((int64_t)((uint64_t)(t60) - (uint64_t)(t61)));
  t63 = ((GG_UNALIGNED gg_unaligned_1 *)t59.data) + gg_index(t62, t59.len);
  t64 = ((int64_t)((*t63)));
  t65 = ((int64_t)(v11));
  t66 = ((t64) < (t65));
  if (t66) {
  {
  t67 = v1;
  t68 = ((int64_t)(v4));
  t69 = ((int64_t)(v10));
  t70 = ((int64_t)((uint64_t)(t68) - (uint64_t)(t69)));
  t71 = ((GG_UNALIGNED gg_unaligned_1 *)t67.data) + gg_index(t70, t67.len);
  v12 = ((int64_t)((*t71)));
  t72 = ((int64_t)(v12));
  t73 = ((int64_t)(v3));
  t74 = ((t72) > (t73));
  if (t74) {
  {
  v12 = ((int64_t)(v3));
  }
  }
  t75 = v1;
  t76 = ((int64_t)(v4));
  t77 = ((int64_t)(v7));
  t78 = ((int64_t)((uint64_t)(t76) - (uint64_t)(t77)));
  t79 = ((GG_UNALIGNED gg_unaligned_1 *)t75.data) + gg_index(t78, t75.len);
  (*t79) = ((int64_t)(v12));
  }
  } else {
  {
  t80 = ((int64_t)(v5));
  t81 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t81)))));
label7:;
  t82 = ((int64_t)(v13));
  t83 = ((int64_t)(v2));
  t84 = ((t82) < (t83));
  t85 = t84;
  if (t85) {
  t86 = v0;
  t87 = ((int64_t)(v2));
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t87) - (uint64_t)(t88)))));
  t90 = ((int64_t)(v13));
  t91 = ((int64_t)((uint64_t)(t89) - (uint64_t)(t90)));
  t92 = ((GG_UNALIGNED gg_unaligned_1 *)t86.data) + gg_index(t91, t86.len);
  t93 = ((int64_t)((*t92)));
  t94 = v0;
  t95 = ((int64_t)(v2));
  t96 = ((int64_t)(((int64_t)UINT64_C(1))));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t95) - (uint64_t)(t96)))));
  t98 = ((int64_t)(v13));
  t99 = ((int64_t)(v7));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) - (uint64_t)(t99)))));
  t101 = ((int64_t)((uint64_t)(t97) - (uint64_t)(t100)));
  t102 = ((GG_UNALIGNED gg_unaligned_1 *)t94.data) + gg_index(t101, t94.len);
  t103 = ((int64_t)((*t102)));
  t85 = ((t93) == (t103));
  }
  t104 = t85;
  t105 = t104;
  if(!t105) goto label8;
  {
  t106 = ((int64_t)(v13));
  t107 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t106) + (uint64_t)(t107)))));
  }
  goto label7;
label8:;
  t108 = ((int64_t)(v13));
  t109 = ((int64_t)(v7));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t108) - (uint64_t)(t109)))));
  t110 = ((int64_t)(v14));
  t111 = ((int64_t)(v3));
  t112 = ((t110) > (t111));
  if (t112) {
  {
  v14 = ((int64_t)(v3));
  }
  }
  t113 = v1;
  t114 = ((int64_t)(v4));
  t115 = ((int64_t)(v7));
  t116 = ((int64_t)((uint64_t)(t114) - (uint64_t)(t115)));
  t117 = ((GG_UNALIGNED gg_unaligned_1 *)t113.data) + gg_index(t116, t113.len);
  (*t117) = ((int64_t)(v14));
  v6 = ((int64_t)(v7));
  t118 = ((int64_t)(v13));
  t119 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t118) - (uint64_t)(t119)))));
  }
  }
  }
  }
  t120 = ((int64_t)(v7));
  t121 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t120) + (uint64_t)(t121)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_36(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) gg_slice t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = v0;
  t2 = v1;
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v3));
  t0 = gg_function_37(t1, t2, t3, t4, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_35(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t p5, int64_t p6, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  uint8_t t4;
  gg_slice t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_1 * t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint8_t t15;
  uint8_t t16;
  _Alignas(8) int64_t t17;
  GG_UNALIGNED gg_unaligned_1 * t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) uint8_t t21;
  _Alignas(8) int64_t t22;
  GG_UNALIGNED gg_unaligned_1 * t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) uint8_t t26;
  _Alignas(8) uint8_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) uint8_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) uint8_t t33;
  uint8_t t34;
  gg_slice t35;
  _Alignas(8) int64_t t36;
  GG_UNALIGNED gg_unaligned_1 * t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  GG_UNALIGNED gg_unaligned_1 * t41;
  _Alignas(8) int64_t t42;
  gg_slice t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t t52;
  GG_UNALIGNED gg_unaligned_1 * t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) uint8_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) uint8_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) uint8_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) gg_slice v3;
  _Alignas(8) gg_slice v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) uint8_t v13;
  _Alignas(8) int64_t v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  v7 = ((int64_t)(v5));
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v8));
  t2 = ((int64_t)(v5));
  t3 = ((t1) < (t2));
  t4 = t3;
  if(!t4) goto label2;
  {
  t5 = v0;
  t6 = ((int64_t)(v5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  t9 = ((int64_t)(v8));
  t10 = ((int64_t)((uint64_t)(t8) - (uint64_t)(t9)));
  t11 = ((GG_UNALIGNED gg_unaligned_1 *)t5.data) + gg_index(t10, t5.len);
  v9 = ((int64_t)((*t11)));
  t12 = ((int64_t)UINT64_C(1));
  v10 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t12)))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t13 = ((int64_t)(v11));
  t14 = ((int64_t)(v6));
  t15 = ((t13) < (t14));
  t16 = t15;
  if(!t16) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t17 = v11;
  t18 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t17, (v1).len);
  t19 = ((int64_t)((*t18)));
  t20 = ((int64_t)(v9));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t22 = v11;
  t23 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t22, (v3).len);
  v12 = ((int64_t)((*t23)));
  t24 = ((int64_t)(v12));
  t25 = ((int64_t)(v10));
  t26 = ((t24) > (t25));
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(v12));
  t29 = ((int64_t)(v8));
  t27 = ((t28) <= (t29));
  }
  t30 = t27;
  if (t30) {
  {
  v13 = ((uint8_t)UINT64_C(1));
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t31 = ((int64_t)(v14));
  t32 = ((int64_t)(v12));
  t33 = ((t31) < (t32));
  t34 = t33;
  if(!t34) goto label8;
  {
  t35 = v4;
  /* FG_DIRECT_SLICE_INDEX */
  t36 = v11;
  t37 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t36, (v2).len);
  t38 = ((int64_t)((*t37)));
  t39 = ((int64_t)(v14));
  t40 = ((int64_t)((uint64_t)(t38) + (uint64_t)(t39)));
  t41 = ((GG_UNALIGNED gg_unaligned_1 *)t35.data) + gg_index(t40, t35.len);
  t42 = ((int64_t)((*t41)));
  t43 = v0;
  t44 = ((int64_t)(v5));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) - (uint64_t)(t45)))));
  t47 = ((int64_t)(v8));
  t48 = ((int64_t)(v14));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) - (uint64_t)(t48)))));
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) - (uint64_t)(t50)))));
  t52 = ((int64_t)((uint64_t)(t46) - (uint64_t)(t51)));
  t53 = ((GG_UNALIGNED gg_unaligned_1 *)t43.data) + gg_index(t52, t43.len);
  t54 = ((int64_t)((*t53)));
  t55 = ((t42) != (t54));
  if (t55) {
  {
  v13 = ((uint8_t)UINT64_C(0));
  goto label8;
  }
  }
  t56 = ((int64_t)(v14));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  }
  goto label7;
label8:;
  t58 = v13;
  if (t58) {
  {
  v10 = ((int64_t)(v12));
  }
  }
  }
  }
  }
  }
  t59 = ((int64_t)(v11));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t60)))));
  }
  goto label4;
label5:;
  t61 = ((int64_t)(v10));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((t61) >= (t62));
  if (t63) {
  {
  t64 = ((int64_t)(v8));
  t65 = ((int64_t)(v10));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t64) - (uint64_t)(t65)))));
  goto label2;
  }
  }
  t66 = ((int64_t)(v8));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v7));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_34(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t p5, int64_t p6, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) gg_slice t2;
  _Alignas(8) gg_slice t3;
  _Alignas(8) gg_slice t4;
  _Alignas(8) gg_slice t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) gg_slice v3;
  _Alignas(8) gg_slice v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = v0;
  t2 = v1;
  t3 = v2;
  t4 = v3;
  t5 = v4;
  t6 = ((int64_t)(v5));
  t7 = ((int64_t)(v6));
  t0 = gg_function_35(t1, t2, t3, t4, t5, t6, t7, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_29(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, float p5, float p6, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) float t8;
  _Alignas(8) float t9;
  _Alignas(8) float t10;
  _Alignas(8) float t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) float t13;
  _Alignas(8) float t14;
  _Alignas(8) gg_slice t15;
  _Alignas(8) gg_slice t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) double t19;
  _Alignas(8) float t20;
  _Alignas(8) float t21;
  _Alignas(8) float t22;
  _Alignas(8) float t23;
  _Alignas(8) float t24;
  _Alignas(8) float t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) uint8_t t28;
  uint8_t t29;
  _Alignas(8) int64_t t30;
  GG_UNALIGNED gg_unaligned_2 * t31;
  _Alignas(8) float t32;
  _Alignas(8) float t33;
  _Alignas(8) uint8_t t34;
  _Alignas(8) float t35;
  _Alignas(8) float t36;
  _Alignas(8) float t37;
  _Alignas(8) float t38;
  _Alignas(8) float t39;
  _Alignas(8) float t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) float t43;
  _Alignas(8) float t44;
  _Alignas(8) float t45;
  _Alignas(8) float t46;
  _Alignas(8) float t47;
  _Alignas(8) float t48;
  _Alignas(8) float t49;
  _Alignas(8) float t50;
  _Alignas(8) float t51;
  _Alignas(8) float t52;
  _Alignas(8) float t53;
  _Alignas(8) float t54;
  _Alignas(8) float t55;
  _Alignas(8) uint8_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) uint8_t t59;
  uint8_t t60;
  _Alignas(8) int64_t t61;
  GG_UNALIGNED gg_unaligned_2 * t62;
  _Alignas(8) double t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  GG_UNALIGNED gg_unaligned_2 * t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) uint8_t t70;
  uint8_t t71;
  _Alignas(8) int64_t t72;
  GG_UNALIGNED gg_unaligned_2 * t73;
  _Alignas(8) int64_t t74;
  GG_UNALIGNED gg_unaligned_2 * t75;
  _Alignas(8) float t76;
  _Alignas(8) float t77;
  _Alignas(8) int64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) int64_t t80;
  GG_UNALIGNED gg_unaligned_2 * t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) uint8_t t84;
  uint8_t t85;
  _Alignas(8) int64_t t86;
  GG_UNALIGNED gg_unaligned_2 * t87;
  _Alignas(8) double t88;
  _Alignas(8) double t89;
  _Alignas(8) double t90;
  _Alignas(8) double t91;
  _Alignas(8) int64_t t92;
  GG_UNALIGNED gg_unaligned_2 * t93;
  _Alignas(8) double t94;
  _Alignas(8) double t95;
  _Alignas(8) int64_t t96;
  _Alignas(8) int64_t t97;
  _Alignas(8) int64_t t98;
  _Alignas(8) int64_t t99;
  _Alignas(8) uint8_t t100;
  uint8_t t101;
  _Alignas(8) int64_t t102;
  GG_UNALIGNED gg_unaligned_2 * t103;
  _Alignas(8) int64_t t104;
  GG_UNALIGNED gg_unaligned_2 * t105;
  _Alignas(8) double t106;
  _Alignas(8) double t107;
  _Alignas(8) int64_t t108;
  _Alignas(8) int64_t t109;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  _Alignas(8) float v5;
  _Alignas(8) float v6;
  _Alignas(8) float v7;
  _Alignas(8) float v8;
  _Alignas(8) float v9;
  _Alignas(8) float v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) float v12;
  _Alignas(8) float v13;
  _Alignas(8) float v14;
  _Alignas(8) double v15;
  _Alignas(8) double v16;
  _Alignas(8) double v17;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((float)(v5));
  t2 = ((float)(gg_double_bits(UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  t8 = ((float)(v4));
  t9 = ((float)(v5));
  v7 = ((float)(((t8) - (t9))));
  t10 = ((float)(v7));
  t11 = ((float)(gg_double_bits(UINT64_C(0))));
  t12 = ((t10) < (t11));
  if (t12) {
  {
  v7 = ((float)(gg_double_bits(UINT64_C(0))));
  }
  }
  t13 = ((float)(v4));
  t14 = ((float)(v5));
  v8 = ((float)(((t13) + (t14))));
  t15 = v0;
  t16 = v1;
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = gg_function_5(t15, t16, t17, t18, base + 0);
  (void)(t19);
  t20 = ((float)(gg_double_bits(UINT64_C(0))));
  t21 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t22 = ((float)(((float)(v3))));
  t23 = ((float)(((t21) / (t22))));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t24 = logf(t23);
  t25 = ((float)(t24));
  v9 = ((float)(((t20) - (t25))));
  v10 = ((float)(gg_double_bits(UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t26 = ((int64_t)(v11));
  t27 = ((int64_t)(v3));
  t28 = ((t26) < (t27));
  t29 = t28;
  if(!t29) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t30 = v11;
  t31 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t30, (v1).len);
  v12 = ((float)((*t31)));
  t32 = ((float)(v12));
  t33 = ((float)(gg_double_bits(UINT64_C(0))));
  t34 = ((t32) > (t33));
  if (t34) {
  {
  t35 = ((float)(v10));
  t36 = ((float)(v12));
  t37 = ((float)(v12));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t38 = logf(t37);
  t39 = ((float)(t38));
  t40 = ((float)(((t36) * (t39))));
  v10 = ((float)(((t35) - (t40))));
  }
  }
  t41 = ((int64_t)(v11));
  t42 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t42)))));
  }
  goto label1;
label2:;
  t43 = ((float)(v10));
  t44 = ((float)(v9));
  v13 = ((float)(((t43) / (t44))));
  t45 = ((float)(v7));
  t46 = ((float)(v8));
  t47 = ((float)(v7));
  t48 = ((float)(((t46) - (t47))));
  t49 = ((float)(v13));
  t50 = ((float)(v6));
#ifndef __cplusplus
  extern float (powf)(float, float);
#endif
  t51 = powf(t49, t50);
  t52 = ((float)(t51));
  t53 = ((float)(((t48) * (t52))));
  v14 = ((float)(((t45) + (t53))));
  t54 = ((float)(v14));
  t55 = ((float)(gg_double_bits(UINT64_C(0))));
  t56 = ((t54) <= (t55));
  if (t56) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t57 = ((int64_t)(v11));
  t58 = ((int64_t)(v3));
  t59 = ((t57) < (t58));
  t60 = t59;
  if(!t60) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t61 = v11;
  t62 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t61, (v0).len);
  t63 = gg_double_bits(UINT64_C(5183643170655547384));
  (*t62) = ((float)((-(t63))));
  t64 = ((int64_t)(v11));
  t65 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
  }
  goto label4;
label5:;
  /* FG_DIRECT_SLICE_INDEX */
  t66 = ((int64_t)UINT64_C(0));
  t67 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t66, (v0).len);
  (*t67) = ((float)(gg_double_bits(UINT64_C(0))));
  }
  } else {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t68 = ((int64_t)(v11));
  t69 = ((int64_t)(v3));
  t70 = ((t68) < (t69));
  t71 = t70;
  if(!t71) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t72 = v11;
  t73 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t72, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t74 = v11;
  t75 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t74, (v0).len);
  t76 = ((float)((*t75)));
  t77 = ((float)(v14));
  (*t73) = ((float)(((t76) / (t77))));
  t78 = ((int64_t)(v11));
  t79 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t79)))));
  }
  goto label7;
label8:;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t80 = ((int64_t)UINT64_C(0));
  t81 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t80, (v0).len);
  v15 = ((double)(((double)((*t81)))));
  v16 = ((double)(gg_double_bits(UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t82 = ((int64_t)(v11));
  t83 = ((int64_t)(v3));
  t84 = ((t82) < (t83));
  t85 = t84;
  if(!t85) goto label11;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t86 = v11;
  t87 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t86, (v0).len);
  t88 = ((double)(((double)((*t87)))));
  t89 = ((double)(v15));
  t90 = ((double)(((t88) - (t89))));
#ifndef __cplusplus
  extern double (exp)(double);
#endif
  t91 = exp(t90);
  v17 = ((double)(t91));
  /* FG_DIRECT_SLICE_INDEX */
  t92 = v11;
  t93 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t92, (v1).len);
  (*t93) = ((float)(((float)(v17))));
  t94 = ((double)(v16));
  t95 = ((double)(v17));
  v16 = ((double)(((t94) + (t95))));
  t96 = ((int64_t)(v11));
  t97 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  }
  goto label10;
label11:;
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t98 = ((int64_t)(v11));
  t99 = ((int64_t)(v3));
  t100 = ((t98) < (t99));
  t101 = t100;
  if(!t101) goto label14;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t102 = v11;
  t103 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t102, (v1).len);
  /* FG_DIRECT_SLICE_INDEX */
  t104 = v11;
  t105 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t104, (v1).len);
  t106 = ((double)(((double)((*t105)))));
  t107 = ((double)(v16));
  (*t103) = ((float)(((float)(((t106) / (t107))))));
  t108 = ((int64_t)(v11));
  t109 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t108) + (uint64_t)(t109)))));
  }
  goto label13;
label14:;
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_27(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, float p5, float p6, float p7, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) float t5;
  _Alignas(8) float t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) float t9;
  _Alignas(8) float t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) float t13;
  _Alignas(8) float t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) uint8_t t18;
  uint8_t t19;
  _Alignas(8) int64_t t20;
  GG_UNALIGNED gg_unaligned_1 * t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) uint8_t t24;
  uint8_t t25;
  _Alignas(8) int64_t t26;
  GG_UNALIGNED gg_unaligned_2 * t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) uint8_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) uint8_t t37;
  _Alignas(8) int64_t t38;
  GG_UNALIGNED gg_unaligned_2 * t39;
  _Alignas(8) float t40;
  _Alignas(8) float t41;
  _Alignas(8) uint8_t t42;
  _Alignas(8) int64_t t43;
  GG_UNALIGNED gg_unaligned_2 * t44;
  _Alignas(8) int64_t t45;
  GG_UNALIGNED gg_unaligned_2 * t46;
  _Alignas(8) float t47;
  _Alignas(8) float t48;
  _Alignas(8) int64_t t49;
  GG_UNALIGNED gg_unaligned_2 * t50;
  _Alignas(8) int64_t t51;
  GG_UNALIGNED gg_unaligned_2 * t52;
  _Alignas(8) float t53;
  _Alignas(8) float t54;
  _Alignas(8) int64_t t55;
  GG_UNALIGNED gg_unaligned_2 * t56;
  _Alignas(8) int64_t t57;
  GG_UNALIGNED gg_unaligned_2 * t58;
  _Alignas(8) float t59;
  _Alignas(8) float t60;
  _Alignas(8) float t61;
  _Alignas(8) float t62;
  _Alignas(8) float t63;
  _Alignas(8) float t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) float v5;
  _Alignas(8) float v6;
  _Alignas(8) float v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t1 = ((int64_t)(v4));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((float)(v5));
  t6 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t7 = ((t5) == (t6));
  t8 = t7;
  if (t8) {
  t9 = ((float)(v6));
  t10 = ((float)(gg_double_bits(UINT64_C(0))));
  t8 = ((t9) == (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((float)(v7));
  t14 = ((float)(gg_double_bits(UINT64_C(0))));
  t12 = ((t13) == (t14));
  }
  t4 = t12;
  }
  t15 = t4;
  if (t15) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t16 = ((int64_t)(v8));
  t17 = ((int64_t)(v3));
  t18 = ((t16) < (t17));
  t19 = t18;
  if(!t19) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t20 = v8;
  t21 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t20, (v1).len);
  v9 = ((int64_t)((*t21)));
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t22 = ((int64_t)(v11));
  t23 = ((int64_t)(v4));
  t24 = ((t22) < (t23));
  t25 = t24;
  if(!t25) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t26 = v11;
  t27 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t26, (v2).len);
  t28 = ((int64_t)(((int64_t)gg_float_integer((*t27), 64, 0, 1, 1, "\151\066\064", "\163\162\143\057\143\150\141\151\156\056\147\157\157\163\145", 1226))));
  t29 = ((int64_t)(v9));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t31 = ((int64_t)(v10));
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  }
  }
  t33 = ((int64_t)(v11));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
  }
  goto label4;
label5:;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t35) > (t36));
  if (t37) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t38 = v8;
  t39 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t38, (v0).len);
  t40 = ((float)((*t39)));
  t41 = ((float)(gg_double_bits(UINT64_C(0))));
  t42 = ((t40) <= (t41));
  if (t42) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t43 = v8;
  t44 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t43, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t45 = v8;
  t46 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t45, (v0).len);
  t47 = ((float)((*t46)));
  t48 = ((float)(v5));
  (*t44) = ((float)(((t47) * (t48))));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t49 = v8;
  t50 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t49, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t51 = v8;
  t52 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t51, (v0).len);
  t53 = ((float)((*t52)));
  t54 = ((float)(v5));
  (*t50) = ((float)(((t53) / (t54))));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t55 = v8;
  t56 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t55, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t57 = v8;
  t58 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t57, (v0).len);
  t59 = ((float)((*t58)));
  t60 = ((float)(((float)(v10))));
  t61 = ((float)(v6));
  t62 = ((float)(((t60) * (t61))));
  t63 = ((float)(v7));
  t64 = ((float)(((t62) + (t63))));
  (*t56) = ((float)(((t59) - (t64))));
  }
  }
  t65 = ((int64_t)(v8));
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_4(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint8_t t6;
  uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) uint8_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) int64_t t18;
  GG_UNALIGNED gg_unaligned_2 * t19;
  _Alignas(8) float t20;
  gg_slice t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  GG_UNALIGNED gg_unaligned_2 * t25;
  _Alignas(8) float t26;
  _Alignas(8) uint8_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  GG_UNALIGNED gg_unaligned_2 * t31;
  _Alignas(8) float t32;
  _Alignas(8) int64_t t33;
  GG_UNALIGNED gg_unaligned_2 * t34;
  _Alignas(8) float t35;
  _Alignas(8) uint8_t t36;
  _Alignas(8) int64_t t37;
  GG_UNALIGNED gg_unaligned_2 * t38;
  _Alignas(8) int64_t t39;
  GG_UNALIGNED gg_unaligned_2 * t40;
  _Alignas(8) int64_t t41;
  GG_UNALIGNED gg_unaligned_2 * t42;
  _Alignas(8) int64_t t43;
  GG_UNALIGNED gg_unaligned_2 * t44;
  _Alignas(8) int64_t t45;
  GG_UNALIGNED gg_unaligned_1 * t46;
  _Alignas(8) int64_t t47;
  GG_UNALIGNED gg_unaligned_1 * t48;
  _Alignas(8) int64_t t49;
  GG_UNALIGNED gg_unaligned_1 * t50;
  _Alignas(8) int64_t t51;
  GG_UNALIGNED gg_unaligned_1 * t52;
  _Alignas(8) int64_t t53;
  GG_UNALIGNED gg_unaligned_2 * t54;
  _Alignas(8) int64_t t55;
  GG_UNALIGNED gg_unaligned_2 * t56;
  _Alignas(8) int64_t t57;
  GG_UNALIGNED gg_unaligned_2 * t58;
  _Alignas(8) int64_t t59;
  GG_UNALIGNED gg_unaligned_2 * t60;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) float v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) float v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  v5 = ((int64_t)(v3));
label1:;
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1 = ((int64_t)(v5));
  t2 = ((int64_t)(((int64_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = ((int64_t)(v4));
  t6 = ((t4) < (t5));
  t7 = t6;
  if(!t7) goto label2;
  {
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t12 = ((int64_t)(v6));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  t15 = ((int64_t)(v4));
  t16 = ((t14) < (t15));
  t17 = t16;
  if (t17) {
  /* FG_DIRECT_SLICE_INDEX */
  t18 = v6;
  t19 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t18, (v0).len);
  t20 = ((float)((*t19)));
  t21 = v0;
  t22 = ((int64_t)(v6));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((GG_UNALIGNED gg_unaligned_2 *)t21.data) + gg_index(t24, t21.len);
  t26 = ((float)((*t25)));
  t17 = ((t20) < (t26));
  }
  t27 = t17;
  if (t27) {
  {
  t28 = ((int64_t)(v6));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t30 = v5;
  t31 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t30, (v0).len);
  t32 = ((float)((*t31)));
  /* FG_DIRECT_SLICE_INDEX */
  t33 = v6;
  t34 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t33, (v0).len);
  t35 = ((float)((*t34)));
  t36 = ((t32) >= (t35));
  if (t36) {
  {
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t37 = v5;
  t38 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t37, (v0).len);
  v7 = ((float)((*t38)));
  /* FG_DIRECT_SLICE_INDEX */
  t39 = v5;
  t40 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t39, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t41 = v6;
  t42 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t41, (v0).len);
  (*t40) = ((float)((*t42)));
  /* FG_DIRECT_SLICE_INDEX */
  t43 = v6;
  t44 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t43, (v0).len);
  (*t44) = ((float)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t45 = v5;
  t46 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t45, (v1).len);
  v8 = ((int64_t)((*t46)));
  /* FG_DIRECT_SLICE_INDEX */
  t47 = v5;
  t48 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t47, (v1).len);
  /* FG_DIRECT_SLICE_INDEX */
  t49 = v6;
  t50 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t49, (v1).len);
  (*t48) = ((int64_t)((*t50)));
  /* FG_DIRECT_SLICE_INDEX */
  t51 = v6;
  t52 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t51, (v1).len);
  (*t52) = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t53 = v5;
  t54 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t53, (v2).len);
  v9 = ((float)((*t54)));
  /* FG_DIRECT_SLICE_INDEX */
  t55 = v5;
  t56 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t55, (v2).len);
  /* FG_DIRECT_SLICE_INDEX */
  t57 = v6;
  t58 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t57, (v2).len);
  (*t56) = ((float)((*t58)));
  /* FG_DIRECT_SLICE_INDEX */
  t59 = v6;
  t60 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t59, (v2).len);
  (*t60) = ((float)(v9));
  v5 = ((int64_t)(v6));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_3(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint8_t t6;
  uint8_t t7;
  _Alignas(8) gg_slice t8;
  _Alignas(8) gg_slice t9;
  _Alignas(8) gg_slice t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) uint8_t t19;
  uint8_t t20;
  _Alignas(8) int64_t t21;
  GG_UNALIGNED gg_unaligned_2 * t22;
  _Alignas(8) int64_t t23;
  GG_UNALIGNED gg_unaligned_2 * t24;
  _Alignas(8) int64_t t25;
  GG_UNALIGNED gg_unaligned_2 * t26;
  _Alignas(8) int64_t t27;
  GG_UNALIGNED gg_unaligned_2 * t28;
  _Alignas(8) int64_t t29;
  GG_UNALIGNED gg_unaligned_1 * t30;
  _Alignas(8) int64_t t31;
  GG_UNALIGNED gg_unaligned_1 * t32;
  _Alignas(8) int64_t t33;
  GG_UNALIGNED gg_unaligned_1 * t34;
  _Alignas(8) int64_t t35;
  GG_UNALIGNED gg_unaligned_1 * t36;
  _Alignas(8) int64_t t37;
  GG_UNALIGNED gg_unaligned_2 * t38;
  _Alignas(8) int64_t t39;
  GG_UNALIGNED gg_unaligned_2 * t40;
  _Alignas(8) int64_t t41;
  GG_UNALIGNED gg_unaligned_2 * t42;
  _Alignas(8) int64_t t43;
  GG_UNALIGNED gg_unaligned_2 * t44;
  _Alignas(8) gg_slice t45;
  _Alignas(8) gg_slice t46;
  _Alignas(8) gg_slice t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) float v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) float v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(2))));
  t2 = ((int64_t)(((int64_t)((uint64_t)(t0) - (uint64_t)(t1)))));
  t3 = ((int64_t)(((int64_t)UINT64_C(2))));
  v4 = ((int64_t)(((int64_t)gg_div(t2, t3))));
label1:;
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) >= (t5));
  t7 = t6;
  if(!t7) goto label2;
  {
  t8 = v0;
  t9 = v1;
  t10 = v2;
  t11 = ((int64_t)(v4));
  t12 = ((int64_t)(v3));
  gg_function_4(t8, t9, t10, t11, t12, base + 0);
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t13) - (uint64_t)(t14)))));
  }
  goto label1;
label2:;
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t15) - (uint64_t)(t16)))));
label4:;
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) > (t18));
  t20 = t19;
  if(!t20) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t21 = ((int64_t)UINT64_C(0));
  t22 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t21, (v0).len);
  v6 = ((float)((*t22)));
  /* FG_DIRECT_SLICE_INDEX */
  t23 = ((int64_t)UINT64_C(0));
  t24 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t23, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v5;
  t26 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t25, (v0).len);
  (*t24) = ((float)((*t26)));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = v5;
  t28 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t27, (v0).len);
  (*t28) = ((float)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t29 = ((int64_t)UINT64_C(0));
  t30 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t29, (v1).len);
  v7 = ((int64_t)((*t30)));
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t31, (v1).len);
  /* FG_DIRECT_SLICE_INDEX */
  t33 = v5;
  t34 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t33, (v1).len);
  (*t32) = ((int64_t)((*t34)));
  /* FG_DIRECT_SLICE_INDEX */
  t35 = v5;
  t36 = ((GG_UNALIGNED gg_unaligned_1 *)(v1).data) + gg_index(t35, (v1).len);
  (*t36) = ((int64_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t37 = ((int64_t)UINT64_C(0));
  t38 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t37, (v2).len);
  v8 = ((float)((*t38)));
  /* FG_DIRECT_SLICE_INDEX */
  t39 = ((int64_t)UINT64_C(0));
  t40 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t39, (v2).len);
  /* FG_DIRECT_SLICE_INDEX */
  t41 = v5;
  t42 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t41, (v2).len);
  (*t40) = ((float)((*t42)));
  /* FG_DIRECT_SLICE_INDEX */
  t43 = v5;
  t44 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t43, (v2).len);
  (*t44) = ((float)(v8));
  t45 = v0;
  t46 = v1;
  t47 = v2;
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(v5));
  gg_function_4(t45, t46, t47, t48, t49, base + 0);
  t50 = ((int64_t)(v5));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t50) - (uint64_t)(t51)))));
  }
  goto label4;
label5:;
  }
}
static float gg_function_24(float p0, float p1, float p2, float p3, int64_t base) {
  _Alignas(8) float t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  _Alignas(8) float t3;
  _Alignas(8) float t4;
  _Alignas(8) float t5;
  _Alignas(8) float t6;
  _Alignas(8) float v0;
  _Alignas(8) float v1;
  _Alignas(8) float v2;
  _Alignas(8) float v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((float)(v3));
  t4 = ((float)(v2));
  t5 = ((float)(((t3) - (t4))));
  t6 = ((float)(((t2) * (t5))));
  t0 = ((float)(((t1) - (t6))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_18(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, float p5, int64_t p6, float p7, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) float t5;
  _Alignas(8) float t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) float t12;
  _Alignas(8) float t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) gg_slice t15;
  _Alignas(8) gg_slice t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) double t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) uint8_t t22;
  uint8_t t23;
  _Alignas(8) int64_t t24;
  GG_UNALIGNED gg_unaligned_2 * t25;
  _Alignas(8) float t26;
  _Alignas(8) float t27;
  _Alignas(8) uint8_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) uint8_t t35;
  _Alignas(8) uint8_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) uint8_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) uint8_t t44;
  uint8_t t45;
  _Alignas(8) int64_t t46;
  GG_UNALIGNED gg_unaligned_2 * t47;
  gg_slice t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  GG_UNALIGNED gg_unaligned_2 * t52;
  _Alignas(8) int64_t t53;
  GG_UNALIGNED gg_unaligned_1 * t54;
  gg_slice t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  GG_UNALIGNED gg_unaligned_1 * t59;
  _Alignas(8) int64_t t60;
  GG_UNALIGNED gg_unaligned_2 * t61;
  gg_slice t62;
  _Alignas(8) int64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  GG_UNALIGNED gg_unaligned_2 * t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  _Alignas(8) float v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) float v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t1 = ((float)(v4));
  t2 = ((float)(gg_double_bits(UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((float)(v5));
  t6 = ((float)(gg_double_bits(UINT64_C(4602678819172646912))));
  t4 = ((t5) > (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((t9) < (t10));
  }
  t11 = t8;
  if (t11) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  t12 = ((float)(v7));
  t13 = ((float)(v4));
  t14 = ((t12) > (t13));
  if (t14) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  t15 = v0;
  t16 = v1;
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = gg_function_5(t15, t16, t17, t18, base + 0);
  (void)(t19);
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t20 = ((int64_t)(v9));
  t21 = ((int64_t)(v3));
  t22 = ((t20) < (t21));
  t23 = t22;
  if(!t23) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t24 = v9;
  t25 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t24, (v1).len);
  t26 = ((float)((*t25)));
  t27 = ((float)(v5));
  t28 = ((t26) >= (t27));
  if (t28) {
  {
  v8 = ((int64_t)(v9));
  }
  } else {
  {
  goto label2;
  }
  }
  t29 = ((int64_t)(v9));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  }
  goto label1;
label2:;
  t31 = ((int64_t)(v3));
  t32 = ((int64_t)(v8));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) - (uint64_t)(t32)))));
  t34 = ((int64_t)(v6));
  t35 = ((t33) >= (t34));
  t36 = t35;
  if (t36) {
  t37 = ((int64_t)(v8));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t37) > (t38));
  }
  t39 = t36;
  if (t39) {
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t40 = ((int64_t)(v9));
  t41 = ((int64_t)(v3));
  t42 = ((int64_t)(v8));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) - (uint64_t)(t42)))));
  t44 = ((t40) < (t43));
  t45 = t44;
  if(!t45) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t46 = v9;
  t47 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t46, (v0).len);
  t48 = v0;
  t49 = ((int64_t)(v9));
  t50 = ((int64_t)(v8));
  t51 = ((int64_t)((uint64_t)(t49) + (uint64_t)(t50)));
  t52 = ((GG_UNALIGNED gg_unaligned_2 *)t48.data) + gg_index(t51, t48.len);
  (*t47) = ((float)((*t52)));
  /* FG_DIRECT_SLICE_INDEX */
  t53 = v9;
  t54 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t53, (v2).len);
  t55 = v2;
  t56 = ((int64_t)(v9));
  t57 = ((int64_t)(v8));
  t58 = ((int64_t)((uint64_t)(t56) + (uint64_t)(t57)));
  t59 = ((GG_UNALIGNED gg_unaligned_1 *)t55.data) + gg_index(t58, t55.len);
  (*t54) = ((int64_t)((*t59)));
  /* FG_DIRECT_SLICE_INDEX */
  t60 = v9;
  t61 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t60, (v1).len);
  t62 = v1;
  t63 = ((int64_t)(v9));
  t64 = ((int64_t)(v8));
  t65 = ((int64_t)((uint64_t)(t63) + (uint64_t)(t64)));
  t66 = ((GG_UNALIGNED gg_unaligned_2 *)t62.data) + gg_index(t65, t62.len);
  (*t61) = ((float)((*t66)));
  t67 = ((int64_t)(v9));
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  }
  goto label4;
label5:;
  t69 = ((int64_t)(v3));
  t70 = ((int64_t)(v8));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t69) - (uint64_t)(t70)))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_16(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) float t17;
  _Alignas(8) float t18;
  _Alignas(8) uint8_t t19;
  _Alignas(8) int64_t t20;
  GG_UNALIGNED gg_unaligned_2 * t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) uint8_t t24;
  uint8_t t25;
  _Alignas(8) int64_t t26;
  GG_UNALIGNED gg_unaligned_2 * t27;
  _Alignas(8) float t28;
  _Alignas(8) float t29;
  _Alignas(8) uint8_t t30;
  _Alignas(8) int64_t t31;
  GG_UNALIGNED gg_unaligned_2 * t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) uint8_t t37;
  uint8_t t38;
  _Alignas(8) float t39;
  _Alignas(8) int64_t t40;
  GG_UNALIGNED gg_unaligned_2 * t41;
  _Alignas(8) float t42;
  _Alignas(8) float t43;
  _Alignas(8) float t44;
  _Alignas(8) float t45;
  _Alignas(8) float t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) gg_slice t49;
  _Alignas(8) gg_slice t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) uint8_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) uint8_t t59;
  uint8_t t60;
  _Alignas(8) int64_t t61;
  GG_UNALIGNED gg_unaligned_2 * t62;
  _Alignas(8) float t63;
  _Alignas(8) float t64;
  _Alignas(8) float t65;
  _Alignas(8) float t66;
  _Alignas(8) float t67;
  _Alignas(8) float t68;
  _Alignas(8) float t69;
  _Alignas(8) float t70;
  _Alignas(8) float t71;
  _Alignas(8) float t72;
  _Alignas(8) uint8_t t73;
  _Alignas(8) uint8_t t74;
  _Alignas(8) int64_t t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) int64_t t77;
  _Alignas(8) int64_t t78;
  _Alignas(8) uint8_t t79;
  _Alignas(8) int64_t t80;
  _Alignas(8) int64_t t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) float v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) float v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) float v10;
  _Alignas(8) float v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)((v0).len));
  t6 = ((int64_t)(v3));
  t4 = ((t5) < (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)((v1).len));
  t10 = ((int64_t)(v3));
  t8 = ((t9) < (t10));
  }
  t11 = t8;
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)((v2).len));
  t14 = ((int64_t)(v3));
  t12 = ((t13) < (t14));
  }
  t15 = t12;
  if (t15) {
  {
  t16 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t16)))));
  goto gg_done;
  }
  }
  t17 = ((float)(v4));
  t18 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t19 = ((t17) >= (t18));
  if (t19) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t20 = ((int64_t)UINT64_C(0));
  t21 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t20, (v0).len);
  v6 = ((float)((*t21)));
  v7 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t22 = ((int64_t)(v7));
  t23 = ((int64_t)(v3));
  t24 = ((t22) < (t23));
  t25 = t24;
  if(!t25) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t26 = v7;
  t27 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t26, (v0).len);
  t28 = ((float)((*t27)));
  t29 = ((float)(v6));
  t30 = ((t28) > (t29));
  if (t30) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = v7;
  t32 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t31, (v0).len);
  v6 = ((float)((*t32)));
  }
  }
  t33 = ((int64_t)(v7));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
  }
  goto label1;
label2:;
  v8 = ((float)(gg_double_bits(UINT64_C(0))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t35 = ((int64_t)(v7));
  t36 = ((int64_t)(v3));
  t37 = ((t35) < (t36));
  t38 = t37;
  if(!t38) goto label5;
  {
  t39 = ((float)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t40 = v7;
  t41 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t40, (v0).len);
  t42 = ((float)((*t41)));
  t43 = ((float)(v6));
  t44 = ((float)(((t42) - (t43))));
#ifndef __cplusplus
  extern float (expf)(float);
#endif
  t45 = expf(t44);
  t46 = ((float)(t45));
  v8 = ((float)(((t39) + (t46))));
  t47 = ((int64_t)(v7));
  t48 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  }
  goto label4;
label5:;
  t49 = v0;
  t50 = v2;
  t51 = ((int64_t)(v3));
  t52 = ((int64_t)(v3));
  v9 = gg_function_9(t49, t50, t51, t52, base + 0);
  t53 = ((int64_t)(v9));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((t53) < (t54));
  if (t55) {
  {
  t56 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t56)))));
  goto gg_done;
  }
  }
  v10 = ((float)(gg_double_bits(UINT64_C(0))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t57 = ((int64_t)(v7));
  t58 = ((int64_t)(v3));
  t59 = ((t57) < (t58));
  t60 = t59;
  if(!t60) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t61 = v7;
  t62 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t61, (v0).len);
  t63 = ((float)((*t62)));
  t64 = ((float)(v6));
  t65 = ((float)(((t63) - (t64))));
#ifndef __cplusplus
  extern float (expf)(float);
#endif
  t66 = expf(t65);
  t67 = ((float)(t66));
  t68 = ((float)(v8));
  v11 = ((float)(((t67) / (t68))));
  t69 = ((float)(v10));
  t70 = ((float)(v11));
  v10 = ((float)(((t69) + (t70))));
  t71 = ((float)(v10));
  t72 = ((float)(v4));
  t73 = ((t71) >= (t72));
  t74 = t73;
  if (t74) {
  t75 = ((int64_t)(v7));
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t76)))));
  t78 = ((int64_t)(v5));
  t74 = ((t77) >= (t78));
  }
  t79 = t74;
  if (t79) {
  {
  t80 = ((int64_t)(v7));
  t81 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t81)))));
  goto gg_done;
  }
  }
  t82 = ((int64_t)(v7));
  t83 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  }
  goto label7;
label8:;
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_2(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) float t17;
  _Alignas(8) float t18;
  _Alignas(8) uint8_t t19;
  _Alignas(8) gg_slice t20;
  _Alignas(8) gg_slice t21;
  _Alignas(8) gg_slice t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) uint8_t t28;
  uint8_t t29;
  _Alignas(8) int64_t t30;
  GG_UNALIGNED gg_unaligned_2 * t31;
  _Alignas(8) int64_t t32;
  GG_UNALIGNED gg_unaligned_2 * t33;
  _Alignas(8) int64_t t34;
  GG_UNALIGNED gg_unaligned_2 * t35;
  _Alignas(8) int64_t t36;
  GG_UNALIGNED gg_unaligned_2 * t37;
  _Alignas(8) int64_t t38;
  GG_UNALIGNED gg_unaligned_1 * t39;
  _Alignas(8) int64_t t40;
  GG_UNALIGNED gg_unaligned_1 * t41;
  _Alignas(8) int64_t t42;
  GG_UNALIGNED gg_unaligned_1 * t43;
  _Alignas(8) int64_t t44;
  GG_UNALIGNED gg_unaligned_1 * t45;
  _Alignas(8) int64_t t46;
  GG_UNALIGNED gg_unaligned_2 * t47;
  _Alignas(8) int64_t t48;
  GG_UNALIGNED gg_unaligned_2 * t49;
  _Alignas(8) int64_t t50;
  GG_UNALIGNED gg_unaligned_2 * t51;
  _Alignas(8) int64_t t52;
  GG_UNALIGNED gg_unaligned_2 * t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) gg_slice t58;
  _Alignas(8) gg_slice t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) double t62;
  _Alignas(8) double t63;
  _Alignas(8) uint8_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) uint8_t t68;
  uint8_t t69;
  _Alignas(8) float t70;
  _Alignas(8) int64_t t71;
  GG_UNALIGNED gg_unaligned_2 * t72;
  _Alignas(8) float t73;
  _Alignas(8) int64_t t74;
  GG_UNALIGNED gg_unaligned_2 * t75;
  _Alignas(8) float t76;
  _Alignas(8) float t77;
  _Alignas(8) float t78;
  _Alignas(8) float t79;
  _Alignas(8) int64_t t80;
  _Alignas(8) int64_t t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) uint8_t t84;
  uint8_t t85;
  _Alignas(8) int64_t t86;
  GG_UNALIGNED gg_unaligned_2 * t87;
  _Alignas(8) float t88;
  _Alignas(8) float t89;
  _Alignas(8) float t90;
  _Alignas(8) float t91;
  _Alignas(8) float t92;
  _Alignas(8) int64_t t93;
  GG_UNALIGNED gg_unaligned_2 * t94;
  _Alignas(8) float t95;
  _Alignas(8) float t96;
  _Alignas(8) uint8_t t97;
  _Alignas(8) float t98;
  _Alignas(8) int64_t t99;
  _Alignas(8) int64_t t100;
  _Alignas(8) gg_slice t101;
  _Alignas(8) gg_slice t102;
  _Alignas(8) gg_slice t103;
  _Alignas(8) int64_t t104;
  _Alignas(8) int64_t t105;
  _Alignas(8) int64_t t106;
  _Alignas(8) uint8_t t107;
  uint8_t t108;
  _Alignas(8) float t109;
  _Alignas(8) int64_t t110;
  GG_UNALIGNED gg_unaligned_2 * t111;
  _Alignas(8) float t112;
  _Alignas(8) float t113;
  _Alignas(8) float t114;
  _Alignas(8) uint8_t t115;
  _Alignas(8) uint8_t t116;
  _Alignas(8) int64_t t117;
  _Alignas(8) int64_t t118;
  _Alignas(8) uint8_t t119;
  _Alignas(8) uint8_t t120;
  _Alignas(8) int64_t t121;
  _Alignas(8) int64_t t122;
  _Alignas(8) int64_t t123;
  _Alignas(8) int64_t t124;
  _Alignas(8) uint8_t t125;
  _Alignas(8) int64_t t126;
  _Alignas(8) int64_t t127;
  _Alignas(8) int64_t t128;
  _Alignas(8) int64_t t129;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) float v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) float v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) double v12;
  _Alignas(8) float v13;
  _Alignas(8) float v14;
  _Alignas(8) float v15;
  _Alignas(8) int64_t v16;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)((v0).len));
  t6 = ((int64_t)(v3));
  t4 = ((t5) < (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)((v1).len));
  t10 = ((int64_t)(v3));
  t8 = ((t9) < (t10));
  }
  t11 = t8;
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)((v2).len));
  t14 = ((int64_t)(v3));
  t12 = ((t13) < (t14));
  }
  t15 = t12;
  if (t15) {
  {
  t16 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t16)))));
  goto gg_done;
  }
  }
  t17 = ((float)(v4));
  t18 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t19 = ((t17) >= (t18));
  if (t19) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  t20 = v0;
  t21 = v2;
  t22 = v1;
  t23 = ((int64_t)(v3));
  gg_function_3(t20, t21, t22, t23, base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t24) - (uint64_t)(t25)))));
label1:;
  t26 = ((int64_t)(v6));
  t27 = ((int64_t)(v7));
  t28 = ((t26) < (t27));
  t29 = t28;
  if(!t29) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t30 = v6;
  t31 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t30, (v0).len);
  v8 = ((float)((*t31)));
  /* FG_DIRECT_SLICE_INDEX */
  t32 = v6;
  t33 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t32, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t34 = v7;
  t35 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t34, (v0).len);
  (*t33) = ((float)((*t35)));
  /* FG_DIRECT_SLICE_INDEX */
  t36 = v7;
  t37 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t36, (v0).len);
  (*t37) = ((float)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t38 = v6;
  t39 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t38, (v2).len);
  v9 = ((int64_t)((*t39)));
  /* FG_DIRECT_SLICE_INDEX */
  t40 = v6;
  t41 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t40, (v2).len);
  /* FG_DIRECT_SLICE_INDEX */
  t42 = v7;
  t43 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t42, (v2).len);
  (*t41) = ((int64_t)((*t43)));
  /* FG_DIRECT_SLICE_INDEX */
  t44 = v7;
  t45 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t44, (v2).len);
  (*t45) = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t46 = v6;
  t47 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t46, (v1).len);
  v10 = ((float)((*t47)));
  /* FG_DIRECT_SLICE_INDEX */
  t48 = v6;
  t49 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t48, (v1).len);
  /* FG_DIRECT_SLICE_INDEX */
  t50 = v7;
  t51 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t50, (v1).len);
  (*t49) = ((float)((*t51)));
  /* FG_DIRECT_SLICE_INDEX */
  t52 = v7;
  t53 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t52, (v1).len);
  (*t53) = ((float)(v10));
  t54 = ((int64_t)(v6));
  t55 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t55)))));
  t56 = ((int64_t)(v7));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t56) - (uint64_t)(t57)))));
  }
  goto label1;
label2:;
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = v0;
  t59 = v1;
  t60 = ((int64_t)(v3));
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = gg_function_5(t58, t59, t60, t61, base + 0);
  t62 = ((double)(v12));
  t63 = ((double)(gg_double_bits(UINT64_C(0))));
  t64 = ((t62) < (t63));
  if (t64) {
  {
  t65 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t65)))));
  goto gg_done;
  }
  }
  v13 = ((float)(gg_double_bits(UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t66 = ((int64_t)(v11));
  t67 = ((int64_t)(v3));
  t68 = ((t66) < (t67));
  t69 = t68;
  if(!t69) goto label5;
  {
  t70 = ((float)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t71 = v11;
  t72 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t71, (v1).len);
  t73 = ((float)((*t72)));
  /* FG_DIRECT_SLICE_INDEX */
  t74 = v11;
  t75 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t74, (v1).len);
  t76 = ((float)((*t75)));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t77 = logf(t76);
  t78 = ((float)(t77));
  t79 = ((float)(((t73) * (t78))));
  v13 = ((float)(((t70) - (t79))));
  t80 = ((int64_t)(v11));
  t81 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t81)))));
  }
  goto label4;
label5:;
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t82 = ((int64_t)(v11));
  t83 = ((int64_t)(v3));
  t84 = ((t82) < (t83));
  t85 = t84;
  if(!t85) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t86 = v11;
  t87 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t86, (v1).len);
  t88 = ((float)((*t87)));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t89 = logf(t88);
  t90 = t89;
  t91 = ((float)((-(t90))));
  t92 = ((float)(v13));
  v14 = ((float)(((t91) - (t92))));
  /* FG_DIRECT_SLICE_INDEX */
  t93 = v11;
  t94 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t93, (v0).len);
  t95 = ((float)(v14));
  t96 = ((float)(gg_double_bits(UINT64_C(0))));
  t97 = ((t95) < (t96));
  if (t97) {
  {
  t98 = v14;
  (*t94) = ((float)((-(t98))));
  }
  } else {
  {
  (*t94) = ((float)(v14));
  }
  }
  t99 = ((int64_t)(v11));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t99) + (uint64_t)(t100)))));
  }
  goto label7;
label8:;
  t101 = v0;
  t102 = v2;
  t103 = v1;
  t104 = ((int64_t)(v3));
  gg_function_3(t101, t102, t103, t104, base + 0);
  v15 = ((float)(gg_double_bits(UINT64_C(0))));
  v16 = ((int64_t)(v3));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t105 = ((int64_t)(v11));
  t106 = ((int64_t)(v3));
  t107 = ((t105) < (t106));
  t108 = t107;
  if(!t108) goto label11;
  {
  t109 = ((float)(v15));
  /* FG_DIRECT_SLICE_INDEX */
  t110 = v11;
  t111 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t110, (v1).len);
  t112 = ((float)((*t111)));
  v15 = ((float)(((t109) + (t112))));
  t113 = ((float)(v15));
  t114 = ((float)(v4));
  t115 = ((t113) > (t114));
  t116 = t115;
  if (t116) {
  t117 = ((int64_t)(v5));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((t117) <= (t118));
  t120 = t119;
  if (!t120) {
  t121 = ((int64_t)(v11));
  t122 = ((int64_t)(((int64_t)UINT64_C(1))));
  t123 = ((int64_t)(((int64_t)((uint64_t)(t121) + (uint64_t)(t122)))));
  t124 = ((int64_t)(v5));
  t120 = ((t123) >= (t124));
  }
  t116 = t120;
  }
  t125 = t116;
  if (t125) {
  {
  t126 = ((int64_t)(v11));
  t127 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t127)))));
  goto label11;
  }
  }
  t128 = ((int64_t)(v11));
  t129 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t128) + (uint64_t)(t129)))));
  }
  goto label10;
label11:;
  t0 = ((int64_t)(v16));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_17(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t p4, float p5, int64_t p6, float p7, float p8, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) gg_slice t4;
  _Alignas(8) gg_slice t5;
  _Alignas(8) gg_slice t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) float t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) gg_slice t13;
  _Alignas(8) gg_slice t14;
  _Alignas(8) gg_slice t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) float t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) uint8_t t21;
  _Alignas(8) gg_slice t22;
  _Alignas(8) gg_slice t23;
  _Alignas(8) gg_slice t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) float t26;
  _Alignas(8) float t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) float t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) uint8_t t32;
  _Alignas(8) gg_slice t33;
  _Alignas(8) gg_slice t34;
  _Alignas(8) gg_slice t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) float t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) uint8_t t40;
  _Alignas(8) float t41;
  _Alignas(8) float t42;
  _Alignas(8) float t43;
  _Alignas(8) gg_slice t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) uint8_t t47;
  _Alignas(8) gg_slice t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) float t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) float t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) uint8_t t55;
  _Alignas(8) int64_t t56;
  GG_UNALIGNED gg_unaligned_2 * t57;
  _Alignas(8) float t58;
  _Alignas(8) float t59;
  _Alignas(8) float t60;
  _Alignas(8) int64_t t61;
  GG_UNALIGNED gg_unaligned_2 * t62;
  _Alignas(8) float t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) uint8_t t66;
  _Alignas(8) int64_t t67;
  GG_UNALIGNED gg_unaligned_1 * t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) uint8_t t73;
  uint8_t t74;
  gg_slice t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) int64_t t77;
  _Alignas(8) int64_t t78;
  GG_UNALIGNED gg_unaligned_1 * t79;
  _Alignas(8) int64_t t80;
  GG_UNALIGNED gg_unaligned_1 * t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) int64_t t84;
  _Alignas(8) int64_t t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) int64_t t87;
  _Alignas(8) int64_t t88;
  _Alignas(8) int64_t t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  _Alignas(8) int64_t t92;
  _Alignas(8) int64_t t93;
  _Alignas(8) int64_t t94;
  GG_UNALIGNED gg_unaligned_1 * t95;
  _Alignas(8) int64_t t96;
  _Alignas(8) int64_t t97;
  _Alignas(8) uint8_t t98;
  _Alignas(8) gg_slice t99;
  _Alignas(8) int64_t t100;
  _Alignas(8) int64_t t101;
  _Alignas(8) uint8_t t102;
  _Alignas(8) int64_t t103;
  GG_UNALIGNED gg_unaligned_1 * t104;
  _Alignas(8) int64_t t105;
  _Alignas(8) int64_t t106;
  _Alignas(8) uint8_t t107;
  _Alignas(8) int64_t t108;
  _Alignas(8) int64_t t109;
  _Alignas(8) uint8_t t110;
  _Alignas(8) gg_slice t111;
  _Alignas(8) gg_slice t112;
  _Alignas(8) gg_slice t113;
  _Alignas(8) int64_t t114;
  _Alignas(8) int64_t t115;
  _Alignas(8) int64_t t116;
  _Alignas(8) int64_t t117;
  _Alignas(8) int64_t t118;
  _Alignas(8) uint8_t t119;
  uint8_t t120;
  _Alignas(8) int64_t t121;
  GG_UNALIGNED gg_unaligned_2 * t122;
  _Alignas(8) int64_t t123;
  GG_UNALIGNED gg_unaligned_2 * t124;
  _Alignas(8) int64_t t125;
  GG_UNALIGNED gg_unaligned_2 * t126;
  _Alignas(8) int64_t t127;
  GG_UNALIGNED gg_unaligned_2 * t128;
  _Alignas(8) int64_t t129;
  GG_UNALIGNED gg_unaligned_1 * t130;
  _Alignas(8) int64_t t131;
  GG_UNALIGNED gg_unaligned_1 * t132;
  _Alignas(8) int64_t t133;
  GG_UNALIGNED gg_unaligned_1 * t134;
  _Alignas(8) int64_t t135;
  GG_UNALIGNED gg_unaligned_1 * t136;
  _Alignas(8) int64_t t137;
  _Alignas(8) int64_t t138;
  _Alignas(8) int64_t t139;
  _Alignas(8) int64_t t140;
  _Alignas(8) int64_t t141;
  _Alignas(8) int64_t t142;
  _Alignas(8) uint8_t t143;
  _Alignas(8) gg_slice t144;
  _Alignas(8) gg_slice t145;
  _Alignas(8) gg_slice t146;
  _Alignas(8) int64_t t147;
  _Alignas(8) int64_t t148;
  _Alignas(8) float t149;
  _Alignas(8) float t150;
  _Alignas(8) float t151;
  _Alignas(8) int64_t t152;
  _Alignas(8) int64_t t153;
  _Alignas(8) uint8_t t154;
  _Alignas(8) gg_slice t155;
  _Alignas(8) int64_t t156;
  GG_UNALIGNED gg_unaligned_2 * t157;
  _Alignas(8) gg_slice t158;
  _Alignas(8) double t159;
  _Alignas(8) double t160;
  _Alignas(8) double t161;
  _Alignas(8) int64_t t162;
  GG_UNALIGNED gg_unaligned_2 * t163;
  _Alignas(8) gg_slice t164;
  _Alignas(8) double t165;
  _Alignas(8) double t166;
  _Alignas(8) double t167;
  _Alignas(8) int64_t t168;
  GG_UNALIGNED gg_unaligned_2 * t169;
  _Alignas(8) int64_t t170;
  _Alignas(8) int64_t t171;
  _Alignas(8) uint8_t t172;
  _Alignas(8) int64_t t173;
  GG_UNALIGNED gg_unaligned_2 * t174;
  _Alignas(8) int64_t t175;
  GG_UNALIGNED gg_unaligned_2 * t176;
  _Alignas(8) double t177;
  _Alignas(8) double t178;
  _Alignas(8) int64_t t179;
  GG_UNALIGNED gg_unaligned_2 * t180;
  _Alignas(8) int64_t t181;
  _Alignas(8) int64_t t182;
  _Alignas(8) uint8_t t183;
  uint8_t t184;
  _Alignas(8) double t185;
  _Alignas(8) int64_t t186;
  GG_UNALIGNED gg_unaligned_2 * t187;
  _Alignas(8) double t188;
  _Alignas(8) int64_t t189;
  _Alignas(8) int64_t t190;
  _Alignas(8) int64_t t191;
  GG_UNALIGNED gg_unaligned_2 * t192;
  _Alignas(8) int64_t t193;
  _Alignas(8) int64_t t194;
  _Alignas(8) uint8_t t195;
  _Alignas(8) int64_t t196;
  GG_UNALIGNED gg_unaligned_2 * t197;
  _Alignas(8) int64_t t198;
  _Alignas(8) int64_t t199;
  _Alignas(8) uint8_t t200;
  uint8_t t201;
  _Alignas(8) int64_t t202;
  GG_UNALIGNED gg_unaligned_2 * t203;
  _Alignas(8) float t204;
  _Alignas(8) float t205;
  _Alignas(8) uint8_t t206;
  _Alignas(8) int64_t t207;
  GG_UNALIGNED gg_unaligned_2 * t208;
  _Alignas(8) int64_t t209;
  _Alignas(8) int64_t t210;
  _Alignas(8) float t211;
  _Alignas(8) float t212;
  _Alignas(8) float t213;
  _Alignas(8) float t214;
  _Alignas(8) float t215;
  _Alignas(8) float t216;
  _Alignas(8) gg_slice t217;
  _Alignas(8) gg_slice t218;
  _Alignas(8) int64_t t219;
  _Alignas(8) int64_t t220;
  _Alignas(8) int64_t t221;
  _Alignas(8) int64_t t222;
  _Alignas(8) uint8_t t223;
  uint8_t t224;
  _Alignas(8) int64_t t225;
  GG_UNALIGNED gg_unaligned_2 * t226;
  _Alignas(8) float t227;
  _Alignas(8) float t228;
  _Alignas(8) uint8_t t229;
  _Alignas(8) float t230;
  _Alignas(8) int64_t t231;
  GG_UNALIGNED gg_unaligned_2 * t232;
  _Alignas(8) float t233;
  _Alignas(8) int64_t t234;
  GG_UNALIGNED gg_unaligned_2 * t235;
  _Alignas(8) float t236;
  _Alignas(8) float t237;
  _Alignas(8) float t238;
  _Alignas(8) float t239;
  _Alignas(8) int64_t t240;
  _Alignas(8) int64_t t241;
  _Alignas(8) float t242;
  _Alignas(8) float t243;
  _Alignas(8) float t244;
  _Alignas(8) float t245;
  _Alignas(8) float t246;
  _Alignas(8) float t247;
  _Alignas(8) uint8_t t248;
  _Alignas(8) float t249;
  _Alignas(8) float t250;
  _Alignas(8) float t251;
  _Alignas(8) float t252;
  _Alignas(8) float t253;
  _Alignas(8) float t254;
  _Alignas(8) float t255;
  _Alignas(8) float t256;
  _Alignas(8) float t257;
  _Alignas(8) float t258;
  _Alignas(8) float t259;
  _Alignas(8) int64_t t260;
  GG_UNALIGNED gg_unaligned_2 * t261;
  _Alignas(8) int64_t t262;
  GG_UNALIGNED gg_unaligned_2 * t263;
  _Alignas(8) int64_t t264;
  GG_UNALIGNED gg_unaligned_2 * t265;
  _Alignas(8) int64_t t266;
  GG_UNALIGNED gg_unaligned_2 * t267;
  _Alignas(8) int64_t t268;
  _Alignas(8) int64_t t269;
  _Alignas(8) uint8_t t270;
  _Alignas(8) gg_slice t271;
  _Alignas(8) gg_slice t272;
  _Alignas(8) int64_t t273;
  _Alignas(8) int64_t t274;
  _Alignas(8) double t275;
  _Alignas(8) double t276;
  _Alignas(8) uint8_t t277;
  _Alignas(8) int64_t t278;
  _Alignas(8) int64_t t279;
  _Alignas(8) int64_t t280;
  _Alignas(8) uint8_t t281;
  _Alignas(8) gg_slice t282;
  _Alignas(8) gg_slice t283;
  _Alignas(8) gg_slice t284;
  _Alignas(8) int64_t t285;
  _Alignas(8) float t286;
  _Alignas(8) float t287;
  _Alignas(8) float t288;
  _Alignas(8) int64_t t289;
  _Alignas(8) int64_t t290;
  _Alignas(8) uint8_t t291;
  _Alignas(8) int64_t t292;
  _Alignas(8) int64_t t293;
  _Alignas(8) uint8_t t294;
  uint8_t t295;
  _Alignas(8) double t296;
  _Alignas(8) int64_t t297;
  GG_UNALIGNED gg_unaligned_2 * t298;
  _Alignas(8) double t299;
  _Alignas(8) int64_t t300;
  _Alignas(8) int64_t t301;
  _Alignas(8) gg_slice t302;
  _Alignas(8) gg_slice t303;
  _Alignas(8) int64_t t304;
  _Alignas(8) int64_t t305;
  _Alignas(8) int64_t t306;
  _Alignas(8) uint8_t t307;
  uint8_t t308;
  _Alignas(8) double t309;
  _Alignas(8) int64_t t310;
  GG_UNALIGNED gg_unaligned_2 * t311;
  _Alignas(8) double t312;
  _Alignas(8) int64_t t313;
  _Alignas(8) int64_t t314;
  _Alignas(8) uint8_t t315;
  _Alignas(8) uint8_t t316;
  _Alignas(8) double t317;
  _Alignas(8) double t318;
  _Alignas(8) double t319;
  _Alignas(8) double t320;
  _Alignas(8) double t321;
  _Alignas(8) double t322;
  _Alignas(8) double t323;
  _Alignas(8) double t324;
  _Alignas(8) double t325;
  _Alignas(8) double t326;
  _Alignas(8) uint8_t t327;
  _Alignas(8) int64_t t328;
  _Alignas(8) int64_t t329;
  _Alignas(8) int64_t t330;
  _Alignas(8) int64_t t331;
  _Alignas(8) uint8_t t332;
  _Alignas(8) int64_t t333;
  _Alignas(8) int64_t t334;
  _Alignas(8) int64_t t335;
  _Alignas(8) int64_t t336;
  _Alignas(8) uint8_t t337;
  _Alignas(8) int64_t t338;
  _Alignas(8) int64_t t339;
  _Alignas(8) uint8_t t340;
  uint8_t t341;
  _Alignas(8) double t342;
  _Alignas(8) int64_t t343;
  GG_UNALIGNED gg_unaligned_2 * t344;
  _Alignas(8) double t345;
  _Alignas(8) int64_t t346;
  _Alignas(8) int64_t t347;
  _Alignas(8) gg_slice t348;
  _Alignas(8) int64_t t349;
  _Alignas(8) int64_t t350;
  _Alignas(8) int64_t t351;
  _Alignas(8) int64_t t352;
  _Alignas(8) uint8_t t353;
  uint8_t t354;
  _Alignas(8) double t355;
  _Alignas(8) int64_t t356;
  GG_UNALIGNED gg_unaligned_2 * t357;
  _Alignas(8) double t358;
  _Alignas(8) double t359;
  _Alignas(8) double t360;
  _Alignas(8) int64_t t361;
  GG_UNALIGNED gg_unaligned_2 * t362;
  _Alignas(8) double t363;
  _Alignas(8) double t364;
  _Alignas(8) uint8_t t365;
  _Alignas(8) uint8_t t366;
  _Alignas(8) int64_t t367;
  _Alignas(8) int64_t t368;
  _Alignas(8) int64_t t369;
  _Alignas(8) int64_t t370;
  _Alignas(8) uint8_t t371;
  _Alignas(8) uint8_t t372;
  _Alignas(8) int64_t t373;
  _Alignas(8) int64_t t374;
  _Alignas(8) uint8_t t375;
  _Alignas(8) int64_t t376;
  _Alignas(8) int64_t t377;
  _Alignas(8) int64_t t378;
  GG_UNALIGNED gg_unaligned_2 * t379;
  _Alignas(8) int64_t t380;
  _Alignas(8) int64_t t381;
  _Alignas(8) uint8_t t382;
  _Alignas(8) gg_slice t383;
  _Alignas(8) double t384;
  _Alignas(8) double t385;
  _Alignas(8) double t386;
  _Alignas(8) int64_t t387;
  _Alignas(8) int64_t t388;
  _Alignas(8) uint8_t t389;
  _Alignas(8) gg_slice t390;
  _Alignas(8) int64_t t391;
  _Alignas(8) gg_slice t392;
  _Alignas(8) int64_t t393;
  _Alignas(8) int64_t t394;
  _Alignas(8) uint8_t t395;
  _Alignas(8) gg_slice t396;
  _Alignas(8) gg_slice t397;
  _Alignas(8) gg_slice t398;
  _Alignas(8) int64_t t399;
  _Alignas(8) float t400;
  _Alignas(8) int64_t t401;
  _Alignas(8) int64_t t402;
  _Alignas(8) int64_t t403;
  _Alignas(8) uint8_t t404;
  _Alignas(8) gg_slice t405;
  _Alignas(8) gg_slice t406;
  _Alignas(8) gg_slice t407;
  _Alignas(8) int64_t t408;
  _Alignas(8) int64_t t409;
  _Alignas(8) int64_t t410;
  _Alignas(8) int64_t t411;
  _Alignas(8) uint8_t t412;
  _Alignas(8) int64_t t413;
  GG_UNALIGNED gg_unaligned_1 * t414;
  _Alignas(8) int64_t t415;
  GG_UNALIGNED gg_unaligned_1 * t416;
  _Alignas(8) int64_t t417;
  _Alignas(8) int64_t t418;
  _Alignas(8) int64_t t419;
  GG_UNALIGNED gg_unaligned_1 * t420;
  _Alignas(8) int64_t t421;
  _Alignas(8) int64_t v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) gg_slice v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) float v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) float v7;
  _Alignas(8) float v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) float v13;
  _Alignas(8) int64_t v14;
  _Alignas(8) double v15;
  _Alignas(8) double v16;
  _Alignas(8) double v17;
  _Alignas(8) double v18;
  _Alignas(8) double v19;
  _Alignas(8) double v20;
  _Alignas(8) int64_t v21;
  _Alignas(8) float v22;
  _Alignas(8) int64_t v23;
  _Alignas(8) float v24;
  _Alignas(8) double v25;
  _Alignas(8) float v26;
  _Alignas(8) int64_t v27;
  _Alignas(8) float v28;
  _Alignas(8) float v29;
  _Alignas(8) float v30;
  _Alignas(8) float v31;
  _Alignas(8) double v32;
  _Alignas(8) double v33;
  _Alignas(8) int64_t v34;
  _Alignas(8) int64_t v35;
  _Alignas(8) int64_t v36;
  _Alignas(8) double v37;
  _Alignas(8) int64_t v38;
  _Alignas(8) double v39;
  _Alignas(8) int64_t v40;
  _Alignas(8) double v41;
  _Alignas(8) double v42;
  _Alignas(8) double v43;
  _Alignas(8) int64_t v44;
  _Alignas(8) double v45;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = v1;
  t5 = v2;
  t6 = v3;
  t7 = ((int64_t)(v4));
  t8 = ((float)(v5));
  t9 = ((int64_t)(v6));
  t0 = gg_function_2(t4, t5, t6, t7, t8, t9, base + 0);
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t13 = v1;
  t14 = v2;
  t15 = v3;
  t16 = ((int64_t)(v4));
  t17 = ((float)(v5));
  t18 = ((int64_t)(v6));
  t0 = gg_function_16(t13, t14, t15, t16, t17, t18, base + 0);
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = v1;
  t23 = v2;
  t24 = v3;
  t25 = ((int64_t)(v4));
  t26 = ((float)(v5));
  t27 = ((float)(v7));
  t28 = ((int64_t)(v6));
  t29 = ((float)(v8));
  t0 = gg_function_18(t22, t23, t24, t25, t26, t27, t28, t29, base + 0);
  goto gg_done;
  }
  }
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(3))));
  t32 = ((t30) == (t31));
  if (t32) {
  {
  t33 = v1;
  t34 = v2;
  t35 = v3;
  t36 = ((int64_t)(v4));
  t37 = ((float)(v5));
  t0 = gg_function_19(t33, t34, t35, t36, t37, base + 0);
  goto gg_done;
  }
  }
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(4))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  t41 = ((float)(v5));
  t42 = ((float)(v7));
  t43 = ((float)(v8));
  t44 = v2;
  t0 = gg_function_20(t41, t42, t43, t44, base + 0);
  goto gg_done;
  }
  }
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(5))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  t48 = v1;
  t49 = ((int64_t)(v4));
  t50 = ((float)(v5));
  t51 = ((int64_t)(v6));
  t52 = ((float)(v7));
  t0 = gg_function_21(t48, t49, t50, t51, t52, base + 0);
  goto gg_done;
  }
  }
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(6))));
  t55 = ((t53) == (t54));
  if (t55) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t56 = ((int64_t)UINT64_C(0));
  t57 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t56, (v2).len);
  t58 = ((float)(v5));
  t59 = ((float)(v7));
  t60 = ((float)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t61 = ((int64_t)UINT64_C(0));
  t62 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t61, (v1).len);
  t63 = ((float)((*t62)));
  (*t57) = gg_function_24(t58, t59, t60, t63, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(((int64_t)UINT64_C(7))));
  t66 = ((t64) == (t65));
  if (t66) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t67 = ((int64_t)UINT64_C(0));
  t68 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t67, (v3).len);
  t69 = ((int64_t)(v6));
  t70 = ((int64_t)(gg_globals.v10));
  (*t68) = ((int64_t)(((t69) & (t70))));
  v9 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t71 = ((int64_t)(v9));
  t72 = ((int64_t)(((int64_t)UINT64_C(624))));
  t73 = ((t71) < (t72));
  t74 = t73;
  if(!t74) goto label2;
  {
  t75 = v3;
  t76 = ((int64_t)(v9));
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  t78 = ((int64_t)((uint64_t)(t76) - (uint64_t)(t77)));
  t79 = ((GG_UNALIGNED gg_unaligned_1 *)t75.data) + gg_index(t78, t75.len);
  v10 = ((int64_t)((*t79)));
  /* FG_DIRECT_SLICE_INDEX */
  t80 = v9;
  t81 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t80, (v3).len);
  t82 = ((int64_t)(((int64_t)UINT64_C(1812433253))));
  t83 = ((int64_t)(v10));
  t84 = ((int64_t)(v10));
  t85 = ((int64_t)(((int64_t)UINT64_C(30))));
  t86 = ((int64_t)(((t84) >> ((uint64_t)(t85) & 63))));
  t87 = ((int64_t)(((t83) ^ (t86))));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t82) * (uint64_t)(t87)))));
  t89 = ((int64_t)(v9));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))));
  t91 = ((int64_t)(gg_globals.v10));
  (*t81) = ((int64_t)(((t90) & (t91))));
  t92 = ((int64_t)(v9));
  t93 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t92) + (uint64_t)(t93)))));
  }
  goto label1;
label2:;
  /* FG_DIRECT_SLICE_INDEX */
  t94 = ((int64_t)UINT64_C(624));
  t95 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t94, (v3).len);
  (*t95) = ((int64_t)(((int64_t)UINT64_C(624))));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t96 = ((int64_t)(v0));
  t97 = ((int64_t)(((int64_t)UINT64_C(8))));
  t98 = ((t96) == (t97));
  if (t98) {
  {
  t99 = v3;
  t0 = gg_function_25(t99, base + 0);
  goto gg_done;
  }
  }
  t100 = ((int64_t)(v0));
  t101 = ((int64_t)(((int64_t)UINT64_C(9))));
  t102 = ((t100) == (t101));
  if (t102) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t103 = ((int64_t)UINT64_C(0));
  t104 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t103, (v3).len);
  (*t104) = ((int64_t)(((int64_t)UINT64_C(12345))));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t105 = ((int64_t)(v0));
  t106 = ((int64_t)(((int64_t)UINT64_C(10))));
  t107 = ((t105) == (t106));
  if (t107) {
  {
  t0 = ((int64_t)(v6));
  goto gg_done;
  }
  }
  t108 = ((int64_t)(v0));
  t109 = ((int64_t)(((int64_t)UINT64_C(24))));
  t110 = ((t108) == (t109));
  if (t110) {
  {
  t111 = v1;
  t112 = v3;
  t113 = v2;
  t114 = ((int64_t)(v4));
  gg_function_3(t111, t112, t113, t114, base + 0);
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t115 = ((int64_t)(v4));
  t116 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t115) - (uint64_t)(t116)))));
label4:;
  t117 = ((int64_t)(v11));
  t118 = ((int64_t)(v12));
  t119 = ((t117) < (t118));
  t120 = t119;
  if(!t120) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t121 = v11;
  t122 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t121, (v1).len);
  v13 = ((float)((*t122)));
  /* FG_DIRECT_SLICE_INDEX */
  t123 = v11;
  t124 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t123, (v1).len);
  /* FG_DIRECT_SLICE_INDEX */
  t125 = v12;
  t126 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t125, (v1).len);
  (*t124) = ((float)((*t126)));
  /* FG_DIRECT_SLICE_INDEX */
  t127 = v12;
  t128 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t127, (v1).len);
  (*t128) = ((float)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t129 = v11;
  t130 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t129, (v3).len);
  v14 = ((int64_t)((*t130)));
  /* FG_DIRECT_SLICE_INDEX */
  t131 = v11;
  t132 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t131, (v3).len);
  /* FG_DIRECT_SLICE_INDEX */
  t133 = v12;
  t134 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t133, (v3).len);
  (*t132) = ((int64_t)((*t134)));
  /* FG_DIRECT_SLICE_INDEX */
  t135 = v12;
  t136 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t135, (v3).len);
  (*t136) = ((int64_t)(v14));
  t137 = ((int64_t)(v11));
  t138 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t137) + (uint64_t)(t138)))));
  t139 = ((int64_t)(v12));
  t140 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t139) - (uint64_t)(t140)))));
  }
  goto label4;
label5:;
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
  }
  t141 = ((int64_t)(v0));
  t142 = ((int64_t)(((int64_t)UINT64_C(23))));
  t143 = ((t141) == (t142));
  if (t143) {
  {
  t144 = v1;
  t145 = v3;
  t146 = v2;
  t147 = ((int64_t)(v4));
  t148 = ((int64_t)(v6));
  t149 = ((float)(v5));
  t150 = ((float)(v7));
  t151 = ((float)(v8));
  t0 = gg_function_27(t144, t145, t146, t147, t148, t149, t150, t151, base + 0);
  goto gg_done;
  }
  }
  t152 = ((int64_t)(v0));
  t153 = ((int64_t)(((int64_t)UINT64_C(22))));
  t154 = ((t152) == (t153));
  if (t154) {
  {
  t155 = v3;
  v15 = gg_function_28(t155, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t156 = ((int64_t)UINT64_C(0));
  t157 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t156, (v2).len);
  (*t157) = ((float)(((float)(v15))));
  t158 = v3;
  t159 = gg_function_28(t158, base + 0);
  t160 = ((double)(t159));
  t161 = ((double)(gg_double_bits(UINT64_C(4607182418800017408))));
  v16 = ((double)(((t160) * (t161))));
  /* FG_DIRECT_SLICE_INDEX */
  t162 = ((int64_t)UINT64_C(1));
  t163 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t162, (v2).len);
  (*t163) = ((float)(((float)(v16))));
  t164 = v3;
  t165 = gg_function_28(t164, base + 0);
  t166 = ((double)(t165));
  t167 = ((double)(gg_double_bits(UINT64_C(0))));
  v17 = ((double)(((t166) + (t167))));
  /* FG_DIRECT_SLICE_INDEX */
  t168 = ((int64_t)UINT64_C(2));
  t169 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t168, (v2).len);
  (*t169) = ((float)(((float)(v17))));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t170 = ((int64_t)(v0));
  t171 = ((int64_t)(((int64_t)UINT64_C(21))));
  t172 = ((t170) == (t171));
  if (t172) {
  {
  v18 = ((double)(gg_double_bits(UINT64_C(4609434218613702656))));
  /* FG_DIRECT_SLICE_INDEX */
  t173 = ((int64_t)UINT64_C(0));
  t174 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t173, (v2).len);
  (*t174) = ((float)(((float)(v18))));
  /* FG_DIRECT_SLICE_INDEX */
  t175 = ((int64_t)UINT64_C(0));
  t176 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t175, (v1).len);
  t177 = ((double)(((double)((*t176)))));
  t178 = ((double)(gg_double_bits(UINT64_C(4611686018427387904))));
  v19 = ((double)(((t177) * (t178))));
  /* FG_DIRECT_SLICE_INDEX */
  t179 = ((int64_t)UINT64_C(1));
  t180 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t179, (v2).len);
  (*t180) = ((float)(((float)(v19))));
  v20 = ((double)(gg_double_bits(UINT64_C(0))));
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t181 = ((int64_t)(v21));
  t182 = ((int64_t)(((int64_t)UINT64_C(3))));
  t183 = ((t181) < (t182));
  t184 = t183;
  if(!t184) goto label8;
  {
  t185 = ((double)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t186 = v21;
  t187 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t186, (v1).len);
  t188 = ((double)(((double)((*t187)))));
  v20 = ((double)(((t185) + (t188))));
  t189 = ((int64_t)(v21));
  t190 = ((int64_t)(((int64_t)UINT64_C(1))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t189) + (uint64_t)(t190)))));
  }
  goto label7;
label8:;
  /* FG_DIRECT_SLICE_INDEX */
  t191 = ((int64_t)UINT64_C(2));
  t192 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t191, (v2).len);
  (*t192) = ((float)(((float)(v20))));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t193 = ((int64_t)(v0));
  t194 = ((int64_t)(((int64_t)UINT64_C(20))));
  t195 = ((t193) == (t194));
  if (t195) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t196 = ((int64_t)UINT64_C(0));
  t197 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t196, (v1).len);
  v22 = ((float)((*t197)));
  v23 = ((int64_t)(((int64_t)UINT64_C(1))));
label10:;
  t198 = ((int64_t)(v23));
  t199 = ((int64_t)(v4));
  t200 = ((t198) < (t199));
  t201 = t200;
  if(!t201) goto label11;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t202 = v23;
  t203 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t202, (v1).len);
  t204 = ((float)((*t203)));
  t205 = ((float)(v22));
  t206 = ((t204) > (t205));
  if (t206) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t207 = v23;
  t208 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t207, (v1).len);
  v22 = ((float)((*t208)));
  }
  }
  t209 = ((int64_t)(v23));
  t210 = ((int64_t)(((int64_t)UINT64_C(1))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t209) + (uint64_t)(t210)))));
  }
  goto label10;
label11:;
  t211 = ((float)(gg_double_bits(UINT64_C(0))));
  t212 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t213 = ((float)(((float)(v4))));
  t214 = ((float)(((t212) / (t213))));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t215 = logf(t214);
  t216 = ((float)(t215));
  v24 = ((float)(((t211) - (t216))));
  t217 = v1;
  t218 = v2;
  t219 = ((int64_t)(v4));
  t220 = ((int64_t)(((int64_t)UINT64_C(1))));
  v25 = gg_function_5(t217, t218, t219, t220, base + 0);
  v26 = ((float)(gg_double_bits(UINT64_C(0))));
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t221 = ((int64_t)(v27));
  t222 = ((int64_t)(v4));
  t223 = ((t221) < (t222));
  t224 = t223;
  if(!t224) goto label14;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t225 = v27;
  t226 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t225, (v2).len);
  t227 = ((float)((*t226)));
  t228 = ((float)(gg_double_bits(UINT64_C(0))));
  t229 = ((t227) > (t228));
  if (t229) {
  {
  t230 = ((float)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t231 = v27;
  t232 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t231, (v2).len);
  t233 = ((float)((*t232)));
  /* FG_DIRECT_SLICE_INDEX */
  t234 = v27;
  t235 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t234, (v2).len);
  t236 = ((float)((*t235)));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t237 = logf(t236);
  t238 = ((float)(t237));
  t239 = ((float)(((t233) * (t238))));
  v26 = ((float)(((t230) - (t239))));
  }
  }
  t240 = ((int64_t)(v27));
  t241 = ((int64_t)(((int64_t)UINT64_C(1))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t240) + (uint64_t)(t241)))));
  }
  goto label13;
label14:;
  t242 = ((float)(v26));
  t243 = ((float)(v24));
  v28 = ((float)(((t242) / (t243))));
  t244 = ((float)(v5));
  t245 = ((float)(v7));
  v29 = ((float)(((t244) - (t245))));
  t246 = ((float)(v29));
  t247 = ((float)(gg_double_bits(UINT64_C(0))));
  t248 = ((t246) < (t247));
  if (t248) {
  {
  v29 = ((float)(gg_double_bits(UINT64_C(0))));
  }
  }
  t249 = ((float)(v5));
  t250 = ((float)(v7));
  v30 = ((float)(((t249) + (t250))));
  t251 = ((float)(v29));
  t252 = ((float)(v30));
  t253 = ((float)(v29));
  t254 = ((float)(((t252) - (t253))));
  t255 = ((float)(v28));
  t256 = ((float)(v8));
#ifndef __cplusplus
  extern float (powf)(float, float);
#endif
  t257 = powf(t255, t256);
  t258 = ((float)(t257));
  t259 = ((float)(((t254) * (t258))));
  v31 = ((float)(((t251) + (t259))));
  /* FG_DIRECT_SLICE_INDEX */
  t260 = ((int64_t)UINT64_C(0));
  t261 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t260, (v2).len);
  (*t261) = ((float)(v24));
  /* FG_DIRECT_SLICE_INDEX */
  t262 = ((int64_t)UINT64_C(1));
  t263 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t262, (v2).len);
  (*t263) = ((float)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t264 = ((int64_t)UINT64_C(2));
  t265 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t264, (v2).len);
  (*t265) = ((float)(v28));
  /* FG_DIRECT_SLICE_INDEX */
  t266 = ((int64_t)UINT64_C(3));
  t267 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t266, (v2).len);
  (*t267) = ((float)(v31));
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
  }
  t268 = ((int64_t)(v0));
  t269 = ((int64_t)(((int64_t)UINT64_C(19))));
  t270 = ((t268) == (t269));
  if (t270) {
  {
  t271 = v1;
  t272 = v2;
  t273 = ((int64_t)(v4));
  t274 = ((int64_t)(((int64_t)UINT64_C(1))));
  v32 = gg_function_5(t271, t272, t273, t274, base + 0);
  t275 = ((double)(v32));
  t276 = ((double)(gg_double_bits(UINT64_C(0))));
  t277 = ((t275) < (t276));
  if (t277) {
  {
  t278 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t278)))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
  }
  t279 = ((int64_t)(v0));
  t280 = ((int64_t)(((int64_t)UINT64_C(18))));
  t281 = ((t279) == (t280));
  if (t281) {
  {
  t282 = v1;
  t283 = v2;
  t284 = v3;
  t285 = ((int64_t)(v4));
  t286 = ((float)(v5));
  t287 = ((float)(v7));
  t288 = ((float)(v8));
  t0 = gg_function_29(t282, t283, t284, t285, t286, t287, t288, base + 0);
  goto gg_done;
  }
  }
  t289 = ((int64_t)(v0));
  t290 = ((int64_t)(((int64_t)UINT64_C(17))));
  t291 = ((t289) == (t290));
  if (t291) {
  {
  v33 = ((double)(gg_double_bits(UINT64_C(0))));
  v34 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t292 = ((int64_t)(v34));
  t293 = ((int64_t)(v4));
  t294 = ((t292) < (t293));
  t295 = t294;
  if(!t295) goto label17;
  {
  t296 = ((double)(v33));
  /* FG_DIRECT_SLICE_INDEX */
  t297 = v34;
  t298 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t297, (v1).len);
  t299 = ((double)(((double)((*t298)))));
  v33 = ((double)(((t296) + (t299))));
  t300 = ((int64_t)(v34));
  t301 = ((int64_t)(((int64_t)UINT64_C(1))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t300) + (uint64_t)(t301)))));
  }
  goto label16;
label17:;
  t302 = v3;
  v35 = gg_function_25(t302, base + 0);
  t303 = v3;
  v36 = gg_function_25(t303, base + 0);
  v37 = ((double)(gg_double_bits(UINT64_C(0))));
  t304 = ((int64_t)UINT64_C(1));
  v38 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t304)))));
  v34 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t305 = ((int64_t)(v34));
  t306 = ((int64_t)(v4));
  t307 = ((t305) < (t306));
  t308 = t307;
  if(!t308) goto label20;
  {
  t309 = ((double)(v37));
  /* FG_DIRECT_SLICE_INDEX */
  t310 = v34;
  t311 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t310, (v1).len);
  t312 = ((double)(((double)((*t311)))));
  v37 = ((double)(((t309) + (t312))));
  t313 = ((int64_t)(v38));
  t314 = ((int64_t)(((int64_t)UINT64_C(0))));
  t315 = ((t313) < (t314));
  t316 = t315;
  if (t316) {
  t317 = ((double)(v37));
  t318 = ((double)(v33));
  t319 = ((double)(((t317) / (t318))));
  t320 = ((double)(gg_double_bits(UINT64_C(4895412794951729152))));
  t321 = ((double)(((t319) * (t320))));
  t322 = ((double)(((double)(v35))));
  t323 = ((double)(((double)(v36))));
  t324 = ((double)(gg_double_bits(UINT64_C(4751297606875873280))));
  t325 = ((double)(((t323) * (t324))));
  t326 = ((double)(((t322) + (t325))));
  t316 = ((t321) >= (t326));
  }
  t327 = t316;
  if (t327) {
  {
  v38 = ((int64_t)(v34));
  }
  }
  t328 = ((int64_t)(v34));
  t329 = ((int64_t)(((int64_t)UINT64_C(1))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t328) + (uint64_t)(t329)))));
  }
  goto label19;
label20:;
  t330 = ((int64_t)(v38));
  t331 = ((int64_t)(((int64_t)UINT64_C(0))));
  t332 = ((t330) < (t331));
  if (t332) {
  {
  t333 = ((int64_t)(v4));
  t334 = ((int64_t)(((int64_t)UINT64_C(1))));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t333) - (uint64_t)(t334)))));
  }
  }
  t0 = ((int64_t)(v38));
  goto gg_done;
  }
  }
  t335 = ((int64_t)(v0));
  t336 = ((int64_t)(((int64_t)UINT64_C(16))));
  t337 = ((t335) == (t336));
  if (t337) {
  {
  v39 = ((double)(gg_double_bits(UINT64_C(0))));
  v40 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t338 = ((int64_t)(v40));
  t339 = ((int64_t)(v4));
  t340 = ((t338) < (t339));
  t341 = t340;
  if(!t341) goto label23;
  {
  t342 = ((double)(v39));
  /* FG_DIRECT_SLICE_INDEX */
  t343 = v40;
  t344 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t343, (v1).len);
  t345 = ((double)(((double)((*t344)))));
  v39 = ((double)(((t342) + (t345))));
  t346 = ((int64_t)(v40));
  t347 = ((int64_t)(((int64_t)UINT64_C(1))));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t346) + (uint64_t)(t347)))));
  }
  goto label22;
label23:;
  t348 = v3;
  v41 = gg_function_28(t348, base + 0);
  v42 = ((double)(gg_double_bits(UINT64_C(0))));
  v43 = ((double)(gg_double_bits(UINT64_C(0))));
  t349 = ((int64_t)(v4));
  t350 = ((int64_t)(((int64_t)UINT64_C(1))));
  v44 = ((int64_t)(((int64_t)((uint64_t)(t349) - (uint64_t)(t350)))));
  v40 = ((int64_t)(((int64_t)UINT64_C(0))));
label25:;
  t351 = ((int64_t)(v40));
  t352 = ((int64_t)(v4));
  t353 = ((t351) < (t352));
  t354 = t353;
  if(!t354) goto label26;
  {
  t355 = ((double)(v42));
  /* FG_DIRECT_SLICE_INDEX */
  t356 = v40;
  t357 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t356, (v1).len);
  t358 = ((double)(((double)((*t357)))));
  v42 = ((double)(((t355) + (t358))));
  t359 = ((double)(v42));
  t360 = ((double)(v39));
  v45 = ((double)(((t359) / (t360))));
  /* FG_DIRECT_SLICE_INDEX */
  t361 = v40;
  t362 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t361, (v2).len);
  (*t362) = ((float)(((float)(v45))));
  t363 = ((double)(v45));
  t364 = ((double)(v41));
  t365 = ((t363) >= (t364));
  t366 = t365;
  if (t366) {
  t367 = ((int64_t)(v44));
  t368 = ((int64_t)(v4));
  t369 = ((int64_t)(((int64_t)UINT64_C(1))));
  t370 = ((int64_t)(((int64_t)((uint64_t)(t368) - (uint64_t)(t369)))));
  t366 = ((t367) == (t370));
  }
  t371 = t366;
  t372 = t371;
  if (t372) {
  t373 = ((int64_t)(v40));
  t374 = ((int64_t)(v4));
  t372 = ((t373) < (t374));
  }
  t375 = t372;
  if (t375) {
  {
  v44 = ((int64_t)(v40));
  }
  }
  t376 = ((int64_t)(v40));
  t377 = ((int64_t)(((int64_t)UINT64_C(1))));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t376) + (uint64_t)(t377)))));
  }
  goto label25;
label26:;
  /* FG_DIRECT_SLICE_INDEX */
  t378 = ((int64_t)UINT64_C(0));
  t379 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t378, (v2).len);
  (*t379) = ((float)(((float)(v41))));
  t0 = ((int64_t)(v44));
  goto gg_done;
  }
  }
  t380 = ((int64_t)(v0));
  t381 = ((int64_t)(((int64_t)UINT64_C(15))));
  t382 = ((t380) == (t381));
  if (t382) {
  {
  t383 = v3;
  t384 = gg_function_28(t383, base + 0);
  t385 = ((double)(t384));
  t386 = ((double)(gg_double_bits(UINT64_C(4751297606875873280))));
  t0 = ((int64_t)(((int64_t)gg_float_integer(((t385) * (t386)), 64, 0, 1, 0, "\151\066\064", "\163\162\143\057\143\150\141\151\156\056\147\157\157\163\145", 798))));
  goto gg_done;
  }
  }
  t387 = ((int64_t)(v0));
  t388 = ((int64_t)(((int64_t)UINT64_C(14))));
  t389 = ((t387) == (t388));
  if (t389) {
  {
  t390 = v1;
  t391 = ((int64_t)(v4));
  t392 = v3;
  t0 = gg_function_31(t390, t391, t392, base + 0);
  goto gg_done;
  }
  }
  t393 = ((int64_t)(v0));
  t394 = ((int64_t)(((int64_t)UINT64_C(12))));
  t395 = ((t393) == (t394));
  if (t395) {
  {
  t396 = v1;
  t397 = v2;
  t398 = v3;
  t399 = ((int64_t)(v4));
  t400 = ((float)(v5));
  t401 = ((int64_t)(v6));
  t0 = gg_function_32(t396, t397, t398, t399, t400, t401, base + 0);
  goto gg_done;
  }
  }
  t402 = ((int64_t)(v0));
  t403 = ((int64_t)(((int64_t)UINT64_C(13))));
  t404 = ((t402) == (t403));
  if (t404) {
  {
  t405 = v1;
  t406 = v2;
  t407 = v3;
  t408 = ((int64_t)(v4));
  t409 = ((int64_t)(v6));
  t0 = gg_function_33(t405, t406, t407, t408, t409, base + 0);
  goto gg_done;
  }
  }
  t410 = ((int64_t)(v0));
  t411 = ((int64_t)(((int64_t)UINT64_C(11))));
  t412 = ((t410) == (t411));
  if (t412) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t413 = ((int64_t)UINT64_C(0));
  t414 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t413, (v3).len);
  (*t414) = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t415 = ((int64_t)UINT64_C(1));
  t416 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t415, (v3).len);
  t417 = ((int64_t)(v6));
  t418 = ((int64_t)(gg_globals.v10));
  (*t416) = ((int64_t)(((t417) & (t418))));
  /* FG_DIRECT_SLICE_INDEX */
  t419 = ((int64_t)UINT64_C(2));
  t420 = ((GG_UNALIGNED gg_unaligned_1 *)(v3).data) + gg_index(t419, (v3).len);
  (*t420) = ((int64_t)(((int64_t)UINT64_C(7))));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t421 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t421)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_15(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) gg_slice t2;
  _Alignas(8) gg_slice t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) float t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  _Alignas(8) int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = v0;
  t2 = v1;
  t3 = v2;
  t4 = ((int64_t)(v3));
  t5 = ((float)(v4));
  t6 = ((int64_t)(v5));
  t0 = gg_function_16(t1, t2, t3, t4, t5, t6, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_8(gg_slice p0, gg_slice p1, int64_t p2, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) gg_slice t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = v0;
  t2 = v1;
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v2));
  t0 = gg_function_9(t1, t2, t3, t4, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) gg_slice t2;
  _Alignas(8) gg_slice t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) float t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) gg_slice v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) float v4;
  _Alignas(8) int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = v0;
  t2 = v1;
  t3 = v2;
  t4 = ((int64_t)(v3));
  t5 = ((float)(v4));
  t6 = ((int64_t)(v5));
  t0 = gg_function_2(t1, t2, t3, t4, t5, t6, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  _Alignas(8) gg_header t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) gg_slice t2;
  _Alignas(8) gg_slice t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) float t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) gg_slice t7;
  _Alignas(8) gg_slice t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) gg_slice t10;
  _Alignas(8) gg_slice t11;
  _Alignas(8) gg_slice t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) float t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) gg_slice t17;
  _Alignas(8) gg_slice t18;
  _Alignas(8) gg_slice t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) float t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) float t23;
  _Alignas(8) float t24;
  _Alignas(8) gg_slice t25;
  _Alignas(8) gg_slice t26;
  _Alignas(8) gg_slice t27;
  _Alignas(8) gg_slice t28;
  _Alignas(8) gg_slice t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) gg_slice t32;
  _Alignas(8) gg_slice t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) gg_slice t36;
  _Alignas(8) gg_slice t37;
  _Alignas(8) gg_slice t38;
  _Alignas(8) gg_slice t39;
  _Alignas(8) gg_slice t40;
  _Alignas(8) gg_slice t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) float t46;
  _Alignas(8) float t47;
  _Alignas(8) gg_slice t48;
  _Alignas(8) gg_slice t49;
  _Alignas(8) gg_slice t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) float t52;
  _Alignas(8) float t53;
  _Alignas(8) float t54;
  _Alignas(8) float t55;
  _Alignas(8) gg_slice t56;
  _Alignas(8) gg_slice t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) float t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) float t61;
  _Alignas(8) gg_slice t62;
  _Alignas(8) gg_slice t63;
  _Alignas(8) gg_slice t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) float t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) gg_slice t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) gg_slice t70;
  _Alignas(8) gg_slice t71;
  _Alignas(8) gg_slice t72;
  _Alignas(8) gg_slice t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) int64_t t75;
  int64_t t76;
  _Alignas(8) gg_type_5 v0;
  _Alignas(8) gg_type_5 v1;
  _Alignas(8) gg_type_7 v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) int64_t v14;
  _Alignas(8) int64_t v15;
  {
  (t0).base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; (t0).len = 0;
  gg_text(gg_ref(&t0, gg_get_stack(base + 0)), gg_view((void *)"\160\066\055\061\060\040\164\171\160\151\143\141\154\040\147\141\164\145", 18));
  gg_append(gg_ref(&t0, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t0).base, (t0).len));
  (v0).data[0] = ((float)(gg_double_bits(UINT64_C(0))));
  (v0).data[1] = ((float)(gg_double_bits(UINT64_C(0))));
  (v0).data[2] = ((float)(gg_double_bits(UINT64_C(0))));
  (v0).data[3] = ((float)(gg_double_bits(UINT64_C(0))));
  (v1).data[0] = ((float)(gg_double_bits(UINT64_C(0))));
  (v1).data[1] = ((float)(gg_double_bits(UINT64_C(0))));
  (v1).data[2] = ((float)(gg_double_bits(UINT64_C(0))));
  (v1).data[3] = ((float)(gg_double_bits(UINT64_C(0))));
  (v2).data[0] = ((int64_t)(((int64_t)UINT64_C(0))));
  (v2).data[1] = ((int64_t)(((int64_t)UINT64_C(1))));
  (v2).data[2] = ((int64_t)(((int64_t)UINT64_C(2))));
  (v2).data[3] = ((int64_t)(((int64_t)UINT64_C(3))));
  t1 = gg_view((v0).data, 4);
  t2 = gg_view((v1).data, 4);
  t3 = gg_view((v2).data, 4);
  t4 = ((int64_t)(((int64_t)UINT64_C(4))));
  t5 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  v3 = gg_function_1(t1, t2, t3, t4, t5, t6, base + 1);
  t7 = gg_view((v0).data, 4);
  t8 = gg_view((v2).data, 4);
  t9 = ((int64_t)(((int64_t)UINT64_C(4))));
  v4 = gg_function_8(t7, t8, t9, base + 1);
  t10 = gg_view((v0).data, 4);
  t11 = gg_view((v1).data, 4);
  t12 = gg_view((v2).data, 4);
  t13 = ((int64_t)(((int64_t)UINT64_C(4))));
  t14 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = gg_function_15(t10, t11, t12, t13, t14, t15, base + 1);
  t16 = ((int64_t)(((int64_t)UINT64_C(9))));
  t17 = gg_view((v0).data, 4);
  t18 = gg_view((v1).data, 4);
  t19 = gg_view((v2).data, 4);
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((float)(gg_double_bits(UINT64_C(0))));
  t24 = ((float)(gg_double_bits(UINT64_C(0))));
  v6 = gg_function_17(t16, t17, t18, t19, t20, t21, t22, t23, t24, base + 1);
  t25 = gg_view((v2).data, 4);
  t26 = gg_view((v2).data, 4);
  t27 = gg_view((v2).data, 4);
  t28 = gg_view((v2).data, 4);
  t29 = gg_view((v2).data, 4);
  t30 = ((int64_t)(((int64_t)UINT64_C(4))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = gg_function_34(t25, t26, t27, t28, t29, t30, t31, base + 1);
  t32 = gg_view((v2).data, 4);
  t33 = gg_view((v2).data, 4);
  t34 = ((int64_t)(((int64_t)UINT64_C(4))));
  t35 = ((int64_t)(((int64_t)UINT64_C(4))));
  v8 = gg_function_36(t32, t33, t34, t35, base + 1);
  t36 = gg_view((v2).data, 4);
  t37 = gg_view((v2).data, 4);
  t38 = gg_view((v0).data, 4);
  t39 = gg_view((v2).data, 4);
  t40 = gg_view((v2).data, 4);
  t41 = gg_view((v2).data, 4);
  t42 = ((int64_t)(((int64_t)UINT64_C(4))));
  t43 = ((int64_t)(((int64_t)UINT64_C(4))));
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t47 = ((float)(gg_double_bits(UINT64_C(4609434218613702656))));
  v9 = gg_function_38(t36, t37, t38, t39, t40, t41, t42, t43, t44, t45, t46, t47, base + 1);
  t48 = gg_view((v0).data, 4);
  t49 = gg_view((v1).data, 4);
  t50 = gg_view((v2).data, 4);
  t51 = ((int64_t)(((int64_t)UINT64_C(4))));
  t52 = ((float)(gg_double_bits(UINT64_C(4602678819172646912))));
  v10 = gg_function_40(t48, t49, t50, t51, t52, base + 1);
  t53 = ((float)(gg_double_bits(UINT64_C(4602678819172646912))));
  t54 = ((float)(gg_double_bits(UINT64_C(0))));
  t55 = ((float)(gg_double_bits(UINT64_C(0))));
  t56 = gg_view((v1).data, 4);
  v11 = gg_function_20(t53, t54, t55, t56, base + 1);
  t57 = gg_view((v0).data, 4);
  t58 = ((int64_t)(((int64_t)UINT64_C(4))));
  t59 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t60 = ((int64_t)(((int64_t)UINT64_C(3))));
  t61 = ((float)(gg_double_bits(UINT64_C(4636737291354636288))));
  v12 = gg_function_41(t57, t58, t59, t60, t61, base + 1);
  t62 = gg_view((v0).data, 4);
  t63 = gg_view((v1).data, 4);
  t64 = gg_view((v2).data, 4);
  t65 = ((int64_t)(((int64_t)UINT64_C(4))));
  t66 = ((float)(gg_double_bits(UINT64_C(4602678819172646912))));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = gg_function_32(t62, t63, t64, t65, t66, t67, base + 1);
  t68 = gg_view((v0).data, 4);
  t69 = ((int64_t)(((int64_t)UINT64_C(4))));
  t70 = gg_view((v2).data, 4);
  v14 = gg_function_42(t68, t69, t70, base + 1);
  t71 = gg_view((v0).data, 4);
  t72 = gg_view((v1).data, 4);
  t73 = gg_view((v2).data, 4);
  t74 = ((int64_t)(((int64_t)UINT64_C(4))));
  t75 = ((int64_t)(((int64_t)UINT64_C(2))));
  v15 = gg_function_33(t71, t72, t73, t74, t75, base + 1);
  }
  for (t76 = 0; t76 < 1; t76++) gg_stacks[base + t76].top = 0;
}
#ifdef _MSC_VER
static volatile LONG gg_globals_state;
static __declspec(thread) int gg_globals_owner;
static int gg_globals_ready(void) { return InterlockedCompareExchange(&gg_globals_state, 0, 0) == 2; }
static int gg_globals_claim(void) { return InterlockedCompareExchange(&gg_globals_state, 1, 0) == 0; }
static void gg_globals_publish(void) { InterlockedExchange(&gg_globals_state, 2); }
#else
#include <stdatomic.h>
static atomic_int gg_globals_state;
static _Thread_local int gg_globals_owner;
static int gg_globals_ready(void) { return atomic_load_explicit(&gg_globals_state, memory_order_acquire) == 2; }
static int gg_globals_claim(void) { int expected = 0; return atomic_compare_exchange_strong_explicit(&gg_globals_state, &expected, 1, memory_order_acq_rel, memory_order_acquire); }
static void gg_globals_publish(void) { atomic_store_explicit(&gg_globals_state, 2, memory_order_release); }
#endif
#ifndef _WIN32
#include <sched.h>
#endif
int64_t gg_init_globals(int argc, char **argv) {  if (argc < 0 || (argc && !argv)) gg_fail("invalid module arguments");
  if (gg_globals_ready()) return 0;
  if (gg_globals_owner) gg_fail("recursive global initialization");
  if (!gg_globals_claim()) {
    while (!gg_globals_ready()) {
#ifdef _WIN32
      SwitchToThread();
#else
      sched_yield();
#endif
    }
    return 0;
  }
  gg_globals_owner = 1;
  struct { int64_t base; } frame;
  frame.base = 0;
  gg_argc = argc; gg_argv = argv;
  gg_globals.v5 = ((int64_t)(((int64_t)UINT64_C(624))));
  gg_globals.v6 = ((int64_t)(((int64_t)UINT64_C(397))));
  gg_globals.v7 = ((int64_t)(((int64_t)UINT64_C(2567483615))));
  gg_globals.v8 = ((int64_t)(((int64_t)UINT64_C(2147483648))));
  gg_globals.v9 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  gg_globals.v10 = ((int64_t)(((int64_t)UINT64_C(4294967295))));
  gg_globals_owner = 0;
  gg_globals_publish();
  return 0;
}
static int gg_program(int argc, char **argv) {
  int64_t base = gg_init_globals(argc, argv);
  gg_function_0(base);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); gg_fail("native call stack overflow"); return 1;
  }
#else
  return gg_program(argc, argv);
#endif
}
