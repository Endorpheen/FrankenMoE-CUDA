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
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base);
static int64_t gg_function_2(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t base);
static double gg_function_3(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base);
static double gg_function_3(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base) {
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
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) uint8_t t22;
  uint8_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) uint8_t t26;
  uint8_t t27;
  gg_slice t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  GG_UNALIGNED gg_unaligned_2 * t32;
  _Alignas(8) float t33;
  _Alignas(8) int64_t t34;
  GG_UNALIGNED gg_unaligned_2 * t35;
  _Alignas(8) float t36;
  _Alignas(8) uint8_t t37;
  gg_slice t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  GG_UNALIGNED gg_unaligned_2 * t42;
  gg_slice t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  GG_UNALIGNED gg_unaligned_2 * t47;
  _Alignas(8) int64_t t48;
  GG_UNALIGNED gg_unaligned_2 * t49;
  _Alignas(8) int64_t t50;
  GG_UNALIGNED gg_unaligned_2 * t51;
  gg_slice t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  GG_UNALIGNED gg_unaligned_1 * t56;
  gg_slice t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) int64_t t60;
  GG_UNALIGNED gg_unaligned_1 * t61;
  _Alignas(8) int64_t t62;
  GG_UNALIGNED gg_unaligned_1 * t63;
  _Alignas(8) int64_t t64;
  GG_UNALIGNED gg_unaligned_1 * t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) gg_slice t70;
  _Alignas(8) gg_slice t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) double t74;
  _Alignas(8) int64_t t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) uint8_t t77;
  uint8_t t78;
  _Alignas(8) float t79;
  _Alignas(8) int64_t t80;
  GG_UNALIGNED gg_unaligned_2 * t81;
  _Alignas(8) float t82;
  _Alignas(8) int64_t t83;
  GG_UNALIGNED gg_unaligned_2 * t84;
  _Alignas(8) float t85;
  _Alignas(8) float t86;
  _Alignas(8) float t87;
  _Alignas(8) float t88;
  _Alignas(8) int64_t t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  _Alignas(8) int64_t t92;
  _Alignas(8) uint8_t t93;
  uint8_t t94;
  _Alignas(8) int64_t t95;
  GG_UNALIGNED gg_unaligned_2 * t96;
  _Alignas(8) float t97;
  _Alignas(8) float t98;
  _Alignas(8) float t99;
  _Alignas(8) float t100;
  _Alignas(8) float t101;
  _Alignas(8) int64_t t102;
  GG_UNALIGNED gg_unaligned_2 * t103;
  _Alignas(8) float t104;
  _Alignas(8) float t105;
  _Alignas(8) uint8_t t106;
  _Alignas(8) float t107;
  _Alignas(8) int64_t t108;
  _Alignas(8) int64_t t109;
  _Alignas(8) int64_t t110;
  _Alignas(8) int64_t t111;
  _Alignas(8) uint8_t t112;
  uint8_t t113;
  _Alignas(8) int64_t t114;
  _Alignas(8) int64_t t115;
  _Alignas(8) int64_t t116;
  _Alignas(8) int64_t t117;
  _Alignas(8) uint8_t t118;
  uint8_t t119;
  _Alignas(8) int64_t t120;
  GG_UNALIGNED gg_unaligned_2 * t121;
  _Alignas(8) float t122;
  _Alignas(8) int64_t t123;
  GG_UNALIGNED gg_unaligned_2 * t124;
  _Alignas(8) float t125;
  _Alignas(8) uint8_t t126;
  _Alignas(8) int64_t t127;
  _Alignas(8) int64_t t128;
  _Alignas(8) int64_t t129;
  _Alignas(8) int64_t t130;
  _Alignas(8) uint8_t t131;
  _Alignas(8) int64_t t132;
  GG_UNALIGNED gg_unaligned_2 * t133;
  _Alignas(8) int64_t t134;
  GG_UNALIGNED gg_unaligned_2 * t135;
  _Alignas(8) int64_t t136;
  GG_UNALIGNED gg_unaligned_2 * t137;
  _Alignas(8) int64_t t138;
  GG_UNALIGNED gg_unaligned_2 * t139;
  _Alignas(8) int64_t t140;
  GG_UNALIGNED gg_unaligned_1 * t141;
  _Alignas(8) int64_t t142;
  GG_UNALIGNED gg_unaligned_1 * t143;
  _Alignas(8) int64_t t144;
  GG_UNALIGNED gg_unaligned_1 * t145;
  _Alignas(8) int64_t t146;
  GG_UNALIGNED gg_unaligned_1 * t147;
  _Alignas(8) int64_t t148;
  GG_UNALIGNED gg_unaligned_2 * t149;
  _Alignas(8) int64_t t150;
  GG_UNALIGNED gg_unaligned_2 * t151;
  _Alignas(8) int64_t t152;
  GG_UNALIGNED gg_unaligned_2 * t153;
  _Alignas(8) int64_t t154;
  GG_UNALIGNED gg_unaligned_2 * t155;
  _Alignas(8) int64_t t156;
  _Alignas(8) int64_t t157;
  _Alignas(8) int64_t t158;
  _Alignas(8) int64_t t159;
  _Alignas(8) uint8_t t160;
  uint8_t t161;
  _Alignas(8) float t162;
  _Alignas(8) int64_t t163;
  GG_UNALIGNED gg_unaligned_2 * t164;
  _Alignas(8) float t165;
  _Alignas(8) float t166;
  _Alignas(8) float t167;
  _Alignas(8) uint8_t t168;
  _Alignas(8) uint8_t t169;
  _Alignas(8) int64_t t170;
  _Alignas(8) int64_t t171;
  _Alignas(8) uint8_t t172;
  _Alignas(8) uint8_t t173;
  _Alignas(8) int64_t t174;
  _Alignas(8) int64_t t175;
  _Alignas(8) int64_t t176;
  _Alignas(8) int64_t t177;
  _Alignas(8) uint8_t t178;
  _Alignas(8) int64_t t179;
  _Alignas(8) int64_t t180;
  _Alignas(8) int64_t t181;
  _Alignas(8) int64_t t182;
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
  _Alignas(8) float v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) float v14;
  _Alignas(8) int64_t v15;
  _Alignas(8) float v16;
  _Alignas(8) float v17;
  _Alignas(8) int64_t v18;
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
  v6 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(v3));
  t22 = ((t20) < (t21));
  t23 = t22;
  if(!t23) goto label2;
  {
  v7 = ((int64_t)(v6));
label4:;
  t24 = ((int64_t)(v7));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) > (t25));
  t27 = t26;
  if(!t27) goto label5;
  {
  t28 = v0;
  t29 = ((int64_t)(v7));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((int64_t)((uint64_t)(t29) - (uint64_t)(t30)));
  t32 = ((GG_UNALIGNED gg_unaligned_2 *)t28.data) + gg_index(t31, t28.len);
  t33 = ((float)((*t32)));
  /* FG_DIRECT_SLICE_INDEX */
  t34 = v7;
  t35 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t34, (v0).len);
  t36 = ((float)((*t35)));
  t37 = ((t33) >= (t36));
  if (t37) {
  {
  goto label5;
  }
  }
  t38 = v0;
  t39 = ((int64_t)(v7));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  t41 = ((int64_t)((uint64_t)(t39) - (uint64_t)(t40)));
  t42 = ((GG_UNALIGNED gg_unaligned_2 *)t38.data) + gg_index(t41, t38.len);
  v8 = ((float)((*t42)));
  t43 = v0;
  t44 = ((int64_t)(v7));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  t46 = ((int64_t)((uint64_t)(t44) - (uint64_t)(t45)));
  t47 = ((GG_UNALIGNED gg_unaligned_2 *)t43.data) + gg_index(t46, t43.len);
  /* FG_DIRECT_SLICE_INDEX */
  t48 = v7;
  t49 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t48, (v0).len);
  (*t47) = ((float)((*t49)));
  /* FG_DIRECT_SLICE_INDEX */
  t50 = v7;
  t51 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t50, (v0).len);
  (*t51) = ((float)(v8));
  t52 = v2;
  t53 = ((int64_t)(v7));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((int64_t)((uint64_t)(t53) - (uint64_t)(t54)));
  t56 = ((GG_UNALIGNED gg_unaligned_1 *)t52.data) + gg_index(t55, t52.len);
  v9 = ((int64_t)((*t56)));
  t57 = v2;
  t58 = ((int64_t)(v7));
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((int64_t)((uint64_t)(t58) - (uint64_t)(t59)));
  t61 = ((GG_UNALIGNED gg_unaligned_1 *)t57.data) + gg_index(t60, t57.len);
  /* FG_DIRECT_SLICE_INDEX */
  t62 = v7;
  t63 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t62, (v2).len);
  (*t61) = ((int64_t)((*t63)));
  /* FG_DIRECT_SLICE_INDEX */
  t64 = v7;
  t65 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t64, (v2).len);
  (*t65) = ((int64_t)(v9));
  t66 = ((int64_t)(v7));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t66) - (uint64_t)(t67)))));
  }
  goto label4;
label5:;
  t68 = ((int64_t)(v6));
  t69 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t69)))));
  }
  goto label1;
label2:;
  t70 = v0;
  t71 = v1;
  t72 = ((int64_t)(v3));
  t73 = ((int64_t)(((int64_t)UINT64_C(1))));
  t74 = gg_function_3(t70, t71, t72, t73, base + 0);
  (void)(t74);
  v10 = ((float)(gg_double_bits(UINT64_C(0))));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t75 = ((int64_t)(v6));
  t76 = ((int64_t)(v3));
  t77 = ((t75) < (t76));
  t78 = t77;
  if(!t78) goto label8;
  {
  t79 = ((float)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t80 = v6;
  t81 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t80, (v1).len);
  t82 = ((float)((*t81)));
  /* FG_DIRECT_SLICE_INDEX */
  t83 = v6;
  t84 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t83, (v1).len);
  t85 = ((float)((*t84)));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t86 = logf(t85);
  t87 = ((float)(t86));
  t88 = ((float)(((t82) * (t87))));
  v10 = ((float)(((t79) - (t88))));
  t89 = ((int64_t)(v6));
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t89) + (uint64_t)(t90)))));
  }
  goto label7;
label8:;
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t91 = ((int64_t)(v6));
  t92 = ((int64_t)(v3));
  t93 = ((t91) < (t92));
  t94 = t93;
  if(!t94) goto label11;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t95 = v6;
  t96 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t95, (v1).len);
  t97 = ((float)((*t96)));
#ifndef __cplusplus
  extern float (logf)(float);
#endif
  t98 = logf(t97);
  t99 = t98;
  t100 = ((float)((-(t99))));
  t101 = ((float)(v10));
  v11 = ((float)(((t100) - (t101))));
  /* FG_DIRECT_SLICE_INDEX */
  t102 = v6;
  t103 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t102, (v0).len);
  t104 = ((float)(v11));
  t105 = ((float)(gg_double_bits(UINT64_C(0))));
  t106 = ((t104) < (t105));
  if (t106) {
  {
  t107 = v11;
  (*t103) = ((float)((-(t107))));
  }
  } else {
  {
  (*t103) = ((float)(v11));
  }
  }
  t108 = ((int64_t)(v6));
  t109 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t108) + (uint64_t)(t109)))));
  }
  goto label10;
label11:;
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t110 = ((int64_t)(v6));
  t111 = ((int64_t)(v3));
  t112 = ((t110) < (t111));
  t113 = t112;
  if(!t113) goto label14;
  {
  v12 = ((int64_t)(v6));
  t114 = ((int64_t)(v6));
  t115 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t115)))));
label16:;
  t116 = ((int64_t)(v13));
  t117 = ((int64_t)(v3));
  t118 = ((t116) < (t117));
  t119 = t118;
  if(!t119) goto label17;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t120 = v13;
  t121 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t120, (v0).len);
  t122 = ((float)((*t121)));
  /* FG_DIRECT_SLICE_INDEX */
  t123 = v12;
  t124 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t123, (v0).len);
  t125 = ((float)((*t124)));
  t126 = ((t122) < (t125));
  if (t126) {
  {
  v12 = ((int64_t)(v13));
  }
  }
  t127 = ((int64_t)(v13));
  t128 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t127) + (uint64_t)(t128)))));
  }
  goto label16;
label17:;
  t129 = ((int64_t)(v12));
  t130 = ((int64_t)(v6));
  t131 = ((t129) != (t130));
  if (t131) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t132 = v6;
  t133 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t132, (v0).len);
  v14 = ((float)((*t133)));
  /* FG_DIRECT_SLICE_INDEX */
  t134 = v6;
  t135 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t134, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t136 = v12;
  t137 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t136, (v0).len);
  (*t135) = ((float)((*t137)));
  /* FG_DIRECT_SLICE_INDEX */
  t138 = v12;
  t139 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t138, (v0).len);
  (*t139) = ((float)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t140 = v6;
  t141 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t140, (v2).len);
  v15 = ((int64_t)((*t141)));
  /* FG_DIRECT_SLICE_INDEX */
  t142 = v6;
  t143 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t142, (v2).len);
  /* FG_DIRECT_SLICE_INDEX */
  t144 = v12;
  t145 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t144, (v2).len);
  (*t143) = ((int64_t)((*t145)));
  /* FG_DIRECT_SLICE_INDEX */
  t146 = v12;
  t147 = ((GG_UNALIGNED gg_unaligned_1 *)(v2).data) + gg_index(t146, (v2).len);
  (*t147) = ((int64_t)(v15));
  /* FG_DIRECT_SLICE_INDEX */
  t148 = v6;
  t149 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t148, (v1).len);
  v16 = ((float)((*t149)));
  /* FG_DIRECT_SLICE_INDEX */
  t150 = v6;
  t151 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t150, (v1).len);
  /* FG_DIRECT_SLICE_INDEX */
  t152 = v12;
  t153 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t152, (v1).len);
  (*t151) = ((float)((*t153)));
  /* FG_DIRECT_SLICE_INDEX */
  t154 = v12;
  t155 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t154, (v1).len);
  (*t155) = ((float)(v16));
  }
  }
  t156 = ((int64_t)(v6));
  t157 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t156) + (uint64_t)(t157)))));
  }
  goto label13;
label14:;
  v17 = ((float)(gg_double_bits(UINT64_C(0))));
  v18 = ((int64_t)(v3));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t158 = ((int64_t)(v6));
  t159 = ((int64_t)(v3));
  t160 = ((t158) < (t159));
  t161 = t160;
  if(!t161) goto label20;
  {
  t162 = ((float)(v17));
  /* FG_DIRECT_SLICE_INDEX */
  t163 = v6;
  t164 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t163, (v1).len);
  t165 = ((float)((*t164)));
  v17 = ((float)(((t162) + (t165))));
  t166 = ((float)(v17));
  t167 = ((float)(v4));
  t168 = ((t166) > (t167));
  t169 = t168;
  if (t169) {
  t170 = ((int64_t)(v5));
  t171 = ((int64_t)(((int64_t)UINT64_C(0))));
  t172 = ((t170) <= (t171));
  t173 = t172;
  if (!t173) {
  t174 = ((int64_t)(v6));
  t175 = ((int64_t)(((int64_t)UINT64_C(1))));
  t176 = ((int64_t)(((int64_t)((uint64_t)(t174) + (uint64_t)(t175)))));
  t177 = ((int64_t)(v5));
  t173 = ((t176) >= (t177));
  }
  t169 = t173;
  }
  t178 = t169;
  if (t178) {
  {
  t179 = ((int64_t)(v6));
  t180 = ((int64_t)(((int64_t)UINT64_C(1))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t179) + (uint64_t)(t180)))));
  goto label20;
  }
  }
  t181 = ((int64_t)(v6));
  t182 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t181) + (uint64_t)(t182)))));
  }
  goto label19;
label20:;
  t0 = ((int64_t)(v18));
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
  int64_t t7;
  _Alignas(8) gg_type_5 v0;
  _Alignas(8) gg_type_5 v1;
  _Alignas(8) gg_type_7 v2;
  _Alignas(8) int64_t v3;
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
  }
  for (t7 = 0; t7 < 1; t7++) gg_stacks[base + t7].top = 0;
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
  gg_globals.v2 = ((int64_t)(((int64_t)UINT64_C(624))));
  gg_globals.v3 = ((int64_t)(((int64_t)UINT64_C(397))));
  gg_globals.v4 = ((int64_t)(((int64_t)UINT64_C(2567483615))));
  gg_globals.v5 = ((int64_t)(((int64_t)UINT64_C(2147483648))));
  gg_globals.v6 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  gg_globals.v7 = ((int64_t)(((int64_t)UINT64_C(4294967295))));
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
