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


typedef double gg_unaligned_1 GG_ALIGN1;
typedef uint8_t gg_unaligned_2 GG_ALIGN1;
typedef float gg_unaligned_3 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { uint8_t data[1152]; } gg_type_4;
#pragma pack(pop)
typedef int64_t gg_unaligned_5 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { uint8_t data[292]; } gg_type_6;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_7;
#pragma pack(pop)
typedef gg_type_4 *gg_type_8;
typedef gg_type_8 gg_unaligned_8 GG_ALIGN1;
typedef gg_type_6 *gg_type_9;
typedef gg_type_9 gg_unaligned_9 GG_ALIGN1;
typedef gg_type_7 *gg_type_10;
typedef gg_type_10 gg_unaligned_10 GG_ALIGN1;
typedef gg_slice gg_unaligned_11 GG_ALIGN1;
typedef gg_slice gg_unaligned_12 GG_ALIGN1;
typedef uint32_t gg_unaligned_14 GG_ALIGN1;
typedef uint16_t gg_unaligned_15 GG_ALIGN1;
typedef int16_t gg_unaligned_16 GG_ALIGN1;
typedef int32_t gg_unaligned_17 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { uint8_t data[32]; } gg_type_18;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[16]; } gg_type_20;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_21;
#pragma pack(pop)
typedef gg_type_20 *gg_type_22;
typedef gg_type_22 gg_unaligned_22 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_14 *gg_type_23;
typedef gg_type_23 gg_unaligned_23 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_5 *gg_type_24;
typedef gg_type_24 gg_unaligned_24 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { int16_t data[16]; } gg_type_25;
#pragma pack(pop)
typedef gg_type_25 *gg_type_26;
typedef gg_type_26 gg_unaligned_26 GG_ALIGN1;
typedef gg_type_18 *gg_type_27;
typedef gg_type_27 gg_unaligned_27 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_28;
#pragma pack(pop)
typedef gg_type_28 *gg_type_29;
typedef gg_type_29 gg_unaligned_29 GG_ALIGN1;
typedef int8_t gg_unaligned_30 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { int8_t data[32]; } gg_type_31;
#pragma pack(pop)
typedef gg_type_31 *gg_type_32;
typedef gg_type_32 gg_unaligned_32 GG_ALIGN1;
typedef gg_type_21 *gg_type_33;
typedef gg_type_33 gg_unaligned_33 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_3 *gg_type_34;
typedef gg_type_34 gg_unaligned_34 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { double data[4]; } gg_type_35;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int64_t data[4]; } gg_type_36;
#pragma pack(pop)
typedef GG_UNALIGNED gg_unaligned_1 *gg_type_37;
typedef gg_type_37 gg_unaligned_37 GG_ALIGN1;
typedef gg_slice gg_unaligned_38 GG_ALIGN1;
typedef uint64_t gg_unaligned_39 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_39 *gg_type_40;
typedef gg_type_40 gg_unaligned_40 GG_ALIGN1;
typedef gg_type_35 *gg_type_41;
typedef gg_type_41 gg_unaligned_41 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_42;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(double a, double b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, double value);
static uint8_t gg_eq_2(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint8_t value);
static uint8_t gg_eq_3(float a, float b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, float value);
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_type_4 value);
static uint8_t gg_eq_5(int64_t a, int64_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, int64_t value);
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, gg_type_6 value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, gg_type_9 value);
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, gg_type_10 value);
static uint8_t gg_eq_11(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, gg_slice value);
static uint8_t gg_eq_12(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_slice value);
static uint8_t gg_eq_13(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, uint8_t value);
static uint8_t gg_eq_14(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, uint32_t value);
static uint8_t gg_eq_15(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, uint16_t value);
static uint8_t gg_eq_16(int16_t a, int16_t b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, int16_t value);
static uint8_t gg_eq_17(int32_t a, int32_t b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, int32_t value);
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, gg_type_18 value);
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b);
static uint8_t gg_equal_elements_20(gg_slice a, gg_slice b);
static void gg_render_20(gg_reference out, gg_type_20 value);
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b);
static uint8_t gg_equal_elements_21(gg_slice a, gg_slice b);
static void gg_render_21(gg_reference out, gg_type_21 value);
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b);
static uint8_t gg_equal_elements_22(gg_slice a, gg_slice b);
static void gg_render_22(gg_reference out, gg_type_22 value);
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b);
static uint8_t gg_equal_elements_23(gg_slice a, gg_slice b);
static void gg_render_23(gg_reference out, gg_type_23 value);
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b);
static uint8_t gg_equal_elements_24(gg_slice a, gg_slice b);
static void gg_render_24(gg_reference out, gg_type_24 value);
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b);
static uint8_t gg_equal_elements_25(gg_slice a, gg_slice b);
static void gg_render_25(gg_reference out, gg_type_25 value);
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b);
static uint8_t gg_equal_elements_26(gg_slice a, gg_slice b);
static void gg_render_26(gg_reference out, gg_type_26 value);
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b);
static uint8_t gg_equal_elements_27(gg_slice a, gg_slice b);
static void gg_render_27(gg_reference out, gg_type_27 value);
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b);
static uint8_t gg_equal_elements_28(gg_slice a, gg_slice b);
static void gg_render_28(gg_reference out, gg_type_28 value);
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b);
static uint8_t gg_equal_elements_29(gg_slice a, gg_slice b);
static void gg_render_29(gg_reference out, gg_type_29 value);
static uint8_t gg_eq_30(int8_t a, int8_t b);
static uint8_t gg_equal_elements_30(gg_slice a, gg_slice b);
static void gg_render_30(gg_reference out, int8_t value);
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b);
static uint8_t gg_equal_elements_31(gg_slice a, gg_slice b);
static void gg_render_31(gg_reference out, gg_type_31 value);
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b);
static uint8_t gg_equal_elements_32(gg_slice a, gg_slice b);
static void gg_render_32(gg_reference out, gg_type_32 value);
static uint8_t gg_eq_33(gg_type_33 a, gg_type_33 b);
static uint8_t gg_equal_elements_33(gg_slice a, gg_slice b);
static void gg_render_33(gg_reference out, gg_type_33 value);
static uint8_t gg_eq_34(gg_type_34 a, gg_type_34 b);
static uint8_t gg_equal_elements_34(gg_slice a, gg_slice b);
static void gg_render_34(gg_reference out, gg_type_34 value);
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b);
static uint8_t gg_equal_elements_35(gg_slice a, gg_slice b);
static void gg_render_35(gg_reference out, gg_type_35 value);
static uint8_t gg_eq_36(gg_type_36 a, gg_type_36 b);
static uint8_t gg_equal_elements_36(gg_slice a, gg_slice b);
static void gg_render_36(gg_reference out, gg_type_36 value);
static uint8_t gg_eq_37(gg_type_37 a, gg_type_37 b);
static uint8_t gg_equal_elements_37(gg_slice a, gg_slice b);
static void gg_render_37(gg_reference out, gg_type_37 value);
static uint8_t gg_eq_38(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_38(gg_slice a, gg_slice b);
static void gg_render_38(gg_reference out, gg_slice value);
static uint8_t gg_eq_39(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_39(gg_slice a, gg_slice b);
static void gg_render_39(gg_reference out, uint64_t value);
static uint8_t gg_eq_40(gg_type_40 a, gg_type_40 b);
static uint8_t gg_equal_elements_40(gg_slice a, gg_slice b);
static void gg_render_40(gg_reference out, gg_type_40 value);
static uint8_t gg_eq_41(gg_type_41 a, gg_type_41 b);
static uint8_t gg_equal_elements_41(gg_slice a, gg_slice b);
static void gg_render_41(gg_reference out, gg_type_41 value);
static uint8_t gg_eq_42(gg_type_42 a, gg_type_42 b);
static uint8_t gg_equal_elements_42(gg_slice a, gg_slice b);
static void gg_render_42(gg_reference out, gg_type_42 value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((GG_UNALIGNED gg_unaligned_1*)a.data)[i],((GG_UNALIGNED gg_unaligned_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(double a, double b) {
return a == b;
}
static void gg_render_1(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;if(a.data==b.data)return 1;for(i=0;i<a.len;i++)if(!gg_eq_2(((GG_UNALIGNED gg_unaligned_2*)a.data)[i],((GG_UNALIGNED gg_unaligned_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((GG_UNALIGNED gg_unaligned_3*)a.data)[i],((GG_UNALIGNED gg_unaligned_3*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(float a, float b) {
return a == b;
}
static void gg_render_3(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_type_4*)a.data)[i],((gg_type_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b) {
return gg_equal_elements_2(gg_view((a).data, 1152),gg_view((b).data, 1152));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
gg_text(out,gg_view((value).data, 1152));
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((GG_UNALIGNED gg_unaligned_5*)a.data)[i],((GG_UNALIGNED gg_unaligned_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_type_6*)a.data)[i],((gg_type_6*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b) {
return gg_equal_elements_2(gg_view((a).data, 292),gg_view((b).data, 292));
}
static void gg_render_6(gg_reference out, gg_type_6 value) {
gg_text(out,gg_view((value).data, 292));
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_3(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((GG_UNALIGNED gg_unaligned_3*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((GG_UNALIGNED gg_unaligned_8*)a.data)[i],((GG_UNALIGNED gg_unaligned_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
return a == b;
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
gg_render_4(out,*value);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((GG_UNALIGNED gg_unaligned_9*)a.data)[i],((GG_UNALIGNED gg_unaligned_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b) {
return a == b;
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
gg_render_6(out,*value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((GG_UNALIGNED gg_unaligned_10*)a.data)[i],((GG_UNALIGNED gg_unaligned_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
return a == b;
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
gg_render_7(out,*value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((GG_UNALIGNED gg_unaligned_11*)a.data)[i],((GG_UNALIGNED gg_unaligned_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_slice a, gg_slice b) {
return gg_equal_elements_2(a,b);
}
static void gg_render_11(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((GG_UNALIGNED gg_unaligned_12*)a.data)[i],((GG_UNALIGNED gg_unaligned_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_12(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((GG_UNALIGNED gg_unaligned_3*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_13(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((GG_UNALIGNED gg_unaligned_14*)a.data)[i],((GG_UNALIGNED gg_unaligned_14*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_14(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((GG_UNALIGNED gg_unaligned_15*)a.data)[i],((GG_UNALIGNED gg_unaligned_15*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_15(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((GG_UNALIGNED gg_unaligned_16*)a.data)[i],((GG_UNALIGNED gg_unaligned_16*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_16(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((GG_UNALIGNED gg_unaligned_17*)a.data)[i],((GG_UNALIGNED gg_unaligned_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_17(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_type_18*)a.data)[i],((gg_type_18*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b) {
return gg_equal_elements_2(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_18(gg_reference out, gg_type_18 value) {
gg_text(out,gg_view((value).data, 32));
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((gg_type_20*)a.data)[i],((gg_type_20*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b) {
return gg_equal_elements_2(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_20(gg_reference out, gg_type_20 value) {
gg_text(out,gg_view((value).data, 16));
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((gg_type_21*)a.data)[i],((gg_type_21*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b) {
return gg_equal_elements_17(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_21(gg_reference out, gg_type_21 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_17(out, ((GG_UNALIGNED gg_unaligned_17*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((GG_UNALIGNED gg_unaligned_22*)a.data)[i],((GG_UNALIGNED gg_unaligned_22*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b) {
return a == b;
}
static void gg_render_22(gg_reference out, gg_type_22 value) {
gg_render_20(out,*value);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((GG_UNALIGNED gg_unaligned_23*)a.data)[i],((GG_UNALIGNED gg_unaligned_23*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b) {
return a == b;
}
static void gg_render_23(gg_reference out, gg_type_23 value) {
gg_render_14(out,*value);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((GG_UNALIGNED gg_unaligned_24*)a.data)[i],((GG_UNALIGNED gg_unaligned_24*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b) {
return a == b;
}
static void gg_render_24(gg_reference out, gg_type_24 value) {
gg_render_5(out,*value);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((gg_type_25*)a.data)[i],((gg_type_25*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b) {
return gg_equal_elements_16(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_25(gg_reference out, gg_type_25 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((GG_UNALIGNED gg_unaligned_16*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((GG_UNALIGNED gg_unaligned_26*)a.data)[i],((GG_UNALIGNED gg_unaligned_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b) {
return a == b;
}
static void gg_render_26(gg_reference out, gg_type_26 value) {
gg_render_25(out,*value);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((GG_UNALIGNED gg_unaligned_27*)a.data)[i],((GG_UNALIGNED gg_unaligned_27*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b) {
return a == b;
}
static void gg_render_27(gg_reference out, gg_type_27 value) {
gg_render_18(out,*value);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_type_28*)a.data)[i],((gg_type_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
return gg_equal_elements_17(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_17(out, ((GG_UNALIGNED gg_unaligned_17*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((GG_UNALIGNED gg_unaligned_29*)a.data)[i],((GG_UNALIGNED gg_unaligned_29*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b) {
return a == b;
}
static void gg_render_29(gg_reference out, gg_type_29 value) {
gg_render_28(out,*value);
}
static uint8_t gg_equal_elements_30(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_30(((GG_UNALIGNED gg_unaligned_30*)a.data)[i],((GG_UNALIGNED gg_unaligned_30*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_30(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_30(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_31(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_31(((gg_type_31*)a.data)[i],((gg_type_31*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b) {
return gg_equal_elements_30(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_31(gg_reference out, gg_type_31 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_30(out, ((GG_UNALIGNED gg_unaligned_30*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_32(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_32(((GG_UNALIGNED gg_unaligned_32*)a.data)[i],((GG_UNALIGNED gg_unaligned_32*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b) {
return a == b;
}
static void gg_render_32(gg_reference out, gg_type_32 value) {
gg_render_31(out,*value);
}
static uint8_t gg_equal_elements_33(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_33(((GG_UNALIGNED gg_unaligned_33*)a.data)[i],((GG_UNALIGNED gg_unaligned_33*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_33(gg_type_33 a, gg_type_33 b) {
return a == b;
}
static void gg_render_33(gg_reference out, gg_type_33 value) {
gg_render_21(out,*value);
}
static uint8_t gg_equal_elements_34(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_34(((GG_UNALIGNED gg_unaligned_34*)a.data)[i],((GG_UNALIGNED gg_unaligned_34*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_34(gg_type_34 a, gg_type_34 b) {
return a == b;
}
static void gg_render_34(gg_reference out, gg_type_34 value) {
gg_render_3(out,*value);
}
static uint8_t gg_equal_elements_35(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_35(((gg_type_35*)a.data)[i],((gg_type_35*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b) {
return gg_equal_elements_1(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_35(gg_reference out, gg_type_35 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((GG_UNALIGNED gg_unaligned_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_36(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_36(((gg_type_36*)a.data)[i],((gg_type_36*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_36(gg_type_36 a, gg_type_36 b) {
return gg_equal_elements_5(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_36(gg_reference out, gg_type_36 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((GG_UNALIGNED gg_unaligned_5*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_37(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_37(((GG_UNALIGNED gg_unaligned_37*)a.data)[i],((GG_UNALIGNED gg_unaligned_37*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_37(gg_type_37 a, gg_type_37 b) {
return a == b;
}
static void gg_render_37(gg_reference out, gg_type_37 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_38(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_38(((GG_UNALIGNED gg_unaligned_38*)a.data)[i],((GG_UNALIGNED gg_unaligned_38*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_38(gg_slice a, gg_slice b) {
return gg_equal_elements_2(a,b);
}
static void gg_render_38(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_39(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_39(((GG_UNALIGNED gg_unaligned_39*)a.data)[i],((GG_UNALIGNED gg_unaligned_39*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_39(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_39(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_40(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_40(((GG_UNALIGNED gg_unaligned_40*)a.data)[i],((GG_UNALIGNED gg_unaligned_40*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_40(gg_type_40 a, gg_type_40 b) {
return a == b;
}
static void gg_render_40(gg_reference out, gg_type_40 value) {
gg_render_39(out,*value);
}
static uint8_t gg_equal_elements_41(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_41(((GG_UNALIGNED gg_unaligned_41*)a.data)[i],((GG_UNALIGNED gg_unaligned_41*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_41(gg_type_41 a, gg_type_41 b) {
return a == b;
}
static void gg_render_41(gg_reference out, gg_type_41 value) {
gg_render_35(out,*value);
}
static uint8_t gg_equal_elements_42(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_42(((gg_type_42*)a.data)[i],((gg_type_42*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_42(gg_type_42 a, gg_type_42 b) {
return gg_equal_elements_3(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_42(gg_reference out, gg_type_42 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((GG_UNALIGNED gg_unaligned_3*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
  _Alignas(8) double v244;
  _Alignas(8) double v245;
} gg_globals;
static void gg_function_0(int64_t base);
static uint8_t gg_function_1(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, gg_slice p4, int64_t base);
static GG_INLINE gg_type_18 gg_function_2(uint8_t p265, int64_t base);
static GG_INLINE gg_type_20 gg_function_3(gg_type_20 p0, gg_type_20 p1, int64_t base);
static float gg_function_4(gg_type_20 p0, int64_t p1, int64_t base);
static GG_INLINE float gg_function_6(gg_slice p0, int64_t p1, int64_t base);
static GG_INLINE uint32_t gg_function_7(gg_slice p0, int64_t p1, int64_t base);
static GG_INLINE gg_type_25 gg_function_8(gg_type_25 p0, gg_type_25 p1, int64_t base);
static GG_INLINE gg_type_18 gg_function_9(gg_slice p0, int64_t p1, int64_t base);
static GG_INLINE gg_type_18 gg_function_10(gg_type_18 p367, gg_type_18 p368, int64_t base);
static GG_INLINE gg_type_18 gg_function_11(gg_type_18 p412, uint8_t p413, int64_t base);
static GG_INLINE uint32_t gg_function_12(gg_slice p0, int64_t p1, int64_t base);
static GG_INLINE gg_type_20 gg_function_13(gg_type_28 p0, int64_t base);
static GG_INLINE gg_type_25 gg_function_14(gg_type_20 p0, int64_t base);
static GG_INLINE gg_type_28 gg_function_15(gg_type_28 p0, int64_t p1, int64_t base);
static GG_INLINE gg_type_20 gg_function_16(gg_type_20 p0, gg_type_20 p1, int64_t base);
static GG_INLINE gg_type_31 gg_function_17(gg_slice p0, int64_t p1, int64_t base);
static GG_INLINE gg_type_31 gg_function_18(gg_type_31 p0, int64_t base);
static GG_INLINE gg_type_18 gg_function_19(gg_type_31 p0, int64_t base);
static GG_INLINE gg_type_18 gg_function_20(gg_type_18 p0, int64_t p1, int64_t base);
static GG_INLINE gg_type_31 gg_function_21(gg_type_18 p0, int64_t base);
static GG_INLINE gg_type_18 gg_function_22(gg_type_18 p0, gg_type_18 p1, int64_t p2, int64_t base);
static GG_INLINE gg_type_25 gg_function_23(gg_type_18 p0, gg_type_31 p1, int64_t base);
static GG_INLINE gg_type_25 gg_function_24(gg_type_18 p0, gg_type_31 p1, int64_t base);
static GG_INLINE gg_type_25 gg_function_25(gg_type_25 p713, gg_type_25 p714, int64_t base);
static GG_INLINE gg_type_21 gg_function_26(gg_type_25 p0, gg_type_25 p1, int64_t base);
static GG_INLINE gg_type_21 gg_function_27(gg_type_25 p858, gg_type_25 p859, int64_t base);
static GG_INLINE gg_type_21 gg_function_28(gg_type_21 p874, gg_type_21 p875, int64_t base);
static GG_INLINE gg_type_25 gg_function_29(gg_type_25 p0, int64_t base);
static GG_INLINE gg_type_25 gg_function_30(gg_type_25 p0, int64_t base);
static GG_INLINE gg_type_7 gg_function_31(float p912, int64_t base);
static GG_INLINE gg_type_7 gg_function_32(gg_type_7 p918, gg_type_7 p919, int64_t base);
static GG_INLINE gg_type_7 gg_function_33(gg_type_21 p931, int64_t base);
static GG_INLINE gg_type_7 gg_function_34(gg_type_7 p939, gg_type_7 p940, gg_type_7 p941, int64_t base);
static GG_INLINE gg_type_7 gg_function_36(gg_type_7 p962, gg_type_21 p963, int64_t base);
/* FG_PACKED_MATCH:simd_permute_f32 */
#if FGOOSE_PACKED && GG_PACKED_X86
__attribute__((target("avx2"))) static gg_type_7 gg_packed_36_avx2(gg_type_7 p962, gg_type_21 p963, int64_t base) {
  gg_type_7 packed_result;
  _mm256_storeu_ps(packed_result.data, _mm256_permutevar8x32_ps(_mm256_loadu_ps(p962.data), _mm256_loadu_si256((const __m256i*)p963.data)));
  return packed_result;
}
#endif
static GG_INLINE gg_type_7 gg_function_36(gg_type_7 p962, gg_type_21 p963, int64_t base) {
#if FGOOSE_PACKED && GG_PACKED_X86
  if(gg_packed_available(0)) { FGOOSE_SIMD_HOOK(36); return gg_packed_36_avx2(p962, p963, base); }
#endif
  _Alignas(8) gg_type_7 t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_3 * t8;
  gg_slice t9;
  gg_slice t10;
  _Alignas(8) int64_t t11;
  GG_UNALIGNED gg_unaligned_17 * t12;
  _Alignas(8) uint32_t t13;
  _Alignas(8) uint32_t t14;
  _Alignas(8) int64_t t15;
  GG_UNALIGNED gg_unaligned_3 * t16;
  _Alignas(8) gg_type_7 v962;
  _Alignas(8) gg_type_21 v963;
  _Alignas(8) gg_type_7 v964;
  _Alignas(8) int64_t v966;
  v962 = p962;
  v963 = p963;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v964).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v966 = t4;
  {
  t6 = gg_view((v964).data, 8);
  t7 = v966;
  t8 = ((GG_UNALIGNED gg_unaligned_3 *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v962).data, 8);
  t10 = gg_view((v963).data, 8);
  t11 = v966;
  t12 = ((GG_UNALIGNED gg_unaligned_17 *)t10.data) + gg_index(t11, t10.len);
  t13 = ((uint32_t)(((uint32_t)((*t12)))));
  t14 = ((uint32_t)(((int64_t)UINT64_C(7))));
  t15 = ((int64_t)(((t13) & (t14))));
  t16 = ((GG_UNALIGNED gg_unaligned_3 *)t9.data) + gg_index(t15, t9.len);
  (*t8) = ((float)((*t16)));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v964;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:simd_fma_f32 */
#if FGOOSE_PACKED && GG_PACKED_X86
__attribute__((target("avx2,fma"))) static gg_type_7 gg_packed_34_avx2(gg_type_7 p939, gg_type_7 p940, gg_type_7 p941, int64_t base) {
  gg_type_7 packed_result;
  _mm256_storeu_ps(packed_result.data, _mm256_fmadd_ps(_mm256_loadu_ps(p939.data), _mm256_loadu_ps(p940.data), _mm256_loadu_ps(p941.data)));
  return packed_result;
}
#endif
static GG_INLINE gg_type_7 gg_function_34(gg_type_7 p939, gg_type_7 p940, gg_type_7 p941, int64_t base) {
#if FGOOSE_PACKED && GG_PACKED_X86
  if(gg_packed_available(1)) { FGOOSE_SIMD_HOOK(34); return gg_packed_34_avx2(p939, p940, p941, base); }
#endif
  _Alignas(8) gg_type_7 t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_3 * t8;
  gg_slice t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_3 * t11;
  _Alignas(8) float t12;
  gg_slice t13;
  _Alignas(8) int64_t t14;
  GG_UNALIGNED gg_unaligned_3 * t15;
  _Alignas(8) float t16;
  gg_slice t17;
  _Alignas(8) int64_t t18;
  GG_UNALIGNED gg_unaligned_3 * t19;
  _Alignas(8) float t20;
  _Alignas(8) float t21;
  _Alignas(8) gg_type_7 v939;
  _Alignas(8) gg_type_7 v940;
  _Alignas(8) gg_type_7 v941;
  _Alignas(8) gg_type_7 v942;
  _Alignas(8) int64_t v944;
  v939 = p939;
  v940 = p940;
  v941 = p941;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v942).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v944 = t4;
  {
  t6 = gg_view((v942).data, 8);
  t7 = v944;
  t8 = ((GG_UNALIGNED gg_unaligned_3 *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v939).data, 8);
  t10 = v944;
  t11 = ((GG_UNALIGNED gg_unaligned_3 *)t9.data) + gg_index(t10, t9.len);
  t12 = ((float)((*t11)));
  t13 = gg_view((v940).data, 8);
  t14 = v944;
  t15 = ((GG_UNALIGNED gg_unaligned_3 *)t13.data) + gg_index(t14, t13.len);
  t16 = ((float)((*t15)));
  t17 = gg_view((v941).data, 8);
  t18 = v944;
  t19 = ((GG_UNALIGNED gg_unaligned_3 *)t17.data) + gg_index(t18, t17.len);
  t20 = ((float)((*t19)));
#ifndef __cplusplus
  extern float (fmaf)(float, float, float);
#endif
  t21 = fmaf(t12, t16, t20);
  (*t8) = ((float)(t21));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v942;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:simd_cvt_i32_f32 */
#if FGOOSE_PACKED && GG_PACKED_X86
__attribute__((target("avx2"))) static gg_type_7 gg_packed_33_avx2(gg_type_21 p931, int64_t base) {
  gg_type_7 packed_result;
  _mm256_storeu_ps(packed_result.data, _mm256_cvtepi32_ps(_mm256_loadu_si256((const __m256i*)p931.data)));
  return packed_result;
}
#endif
static GG_INLINE gg_type_7 gg_function_33(gg_type_21 p931, int64_t base) {
#if FGOOSE_PACKED && GG_PACKED_X86
  if(gg_packed_available(0)) { FGOOSE_SIMD_HOOK(33); return gg_packed_33_avx2(p931, base); }
#endif
  _Alignas(8) gg_type_7 t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_3 * t8;
  gg_slice t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_17 * t11;
  _Alignas(8) gg_type_21 v931;
  _Alignas(8) gg_type_7 v932;
  _Alignas(8) int64_t v934;
  v931 = p931;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v932).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v934 = t4;
  {
  t6 = gg_view((v932).data, 8);
  t7 = v934;
  t8 = ((GG_UNALIGNED gg_unaligned_3 *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v931).data, 8);
  t10 = v934;
  t11 = ((GG_UNALIGNED gg_unaligned_17 *)t9.data) + gg_index(t10, t9.len);
  (*t8) = ((float)(((float)((*t11)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v932;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:simd_mul_f32 */
#if FGOOSE_PACKED && GG_PACKED_X86
__attribute__((target("avx2"))) static gg_type_7 gg_packed_32_avx2(gg_type_7 p918, gg_type_7 p919, int64_t base) {
  gg_type_7 packed_result;
  _mm256_storeu_ps(packed_result.data, _mm256_mul_ps(_mm256_loadu_ps(p918.data), _mm256_loadu_ps(p919.data)));
  return packed_result;
}
#endif
static GG_INLINE gg_type_7 gg_function_32(gg_type_7 p918, gg_type_7 p919, int64_t base) {
#if FGOOSE_PACKED && GG_PACKED_X86
  if(gg_packed_available(0)) { FGOOSE_SIMD_HOOK(32); return gg_packed_32_avx2(p918, p919, base); }
#endif
  _Alignas(8) gg_type_7 t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_3 * t8;
  gg_slice t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_3 * t11;
  _Alignas(8) float t12;
  gg_slice t13;
  _Alignas(8) int64_t t14;
  GG_UNALIGNED gg_unaligned_3 * t15;
  _Alignas(8) float t16;
  _Alignas(8) gg_type_7 v918;
  _Alignas(8) gg_type_7 v919;
  _Alignas(8) gg_type_7 v920;
  _Alignas(8) int64_t v922;
  v918 = p918;
  v919 = p919;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v920).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v922 = t4;
  {
  t6 = gg_view((v920).data, 8);
  t7 = v922;
  t8 = ((GG_UNALIGNED gg_unaligned_3 *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v918).data, 8);
  t10 = v922;
  t11 = ((GG_UNALIGNED gg_unaligned_3 *)t9.data) + gg_index(t10, t9.len);
  t12 = ((float)((*t11)));
  t13 = gg_view((v919).data, 8);
  t14 = v922;
  t15 = ((GG_UNALIGNED gg_unaligned_3 *)t13.data) + gg_index(t14, t13.len);
  t16 = ((float)((*t15)));
  (*t8) = ((float)(((t12) * (t16))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v920;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:simd_splat_f32 */
#if FGOOSE_PACKED && GG_PACKED_X86
__attribute__((target("avx2"))) static gg_type_7 gg_packed_31_avx2(float p912, int64_t base) {
  gg_type_7 packed_result;
  _mm256_storeu_ps(packed_result.data, _mm256_set1_ps(p912));
  return packed_result;
}
#endif
static GG_INLINE gg_type_7 gg_function_31(float p912, int64_t base) {
#if FGOOSE_PACKED && GG_PACKED_X86
  if(gg_packed_available(0)) { FGOOSE_SIMD_HOOK(31); return gg_packed_31_avx2(p912, base); }
#endif
  _Alignas(8) gg_type_7 t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_3 * t8;
  _Alignas(8) float v912;
  _Alignas(8) gg_type_7 v913;
  _Alignas(8) int64_t v915;
  v912 = p912;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v913).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v915 = t4;
  {
  t6 = gg_view((v913).data, 8);
  t7 = v915;
  t8 = ((GG_UNALIGNED gg_unaligned_3 *)t6.data) + gg_index(t7, t6.len);
  (*t8) = ((float)(v912));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v913;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:simd_add_i32 */
#if FGOOSE_PACKED && GG_PACKED_X86
__attribute__((target("avx2"))) static gg_type_21 gg_packed_28_avx2(gg_type_21 p874, gg_type_21 p875, int64_t base) {
  gg_type_21 packed_result;
  _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)p874.data), _mm256_loadu_si256((const __m256i*)p875.data)));
  return packed_result;
}
#endif
static GG_INLINE gg_type_21 gg_function_28(gg_type_21 p874, gg_type_21 p875, int64_t base) {
#if FGOOSE_PACKED && GG_PACKED_X86
  if(gg_packed_available(0)) { FGOOSE_SIMD_HOOK(28); return gg_packed_28_avx2(p874, p875, base); }
#endif
  _Alignas(8) gg_type_21 t0;
  _Alignas(8) int32_t t1;
  _Alignas(8) int32_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_17 * t8;
  gg_slice t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_17 * t11;
  _Alignas(8) int64_t t12;
  gg_slice t13;
  _Alignas(8) int64_t t14;
  GG_UNALIGNED gg_unaligned_17 * t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) gg_type_21 v874;
  _Alignas(8) gg_type_21 v875;
  _Alignas(8) gg_type_21 v876;
  _Alignas(8) int64_t v878;
  v874 = p874;
  v875 = p875;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v876).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v878 = t4;
  {
  t6 = gg_view((v876).data, 8);
  t7 = v878;
  t8 = ((GG_UNALIGNED gg_unaligned_17 *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v874).data, 8);
  t10 = v878;
  t11 = ((GG_UNALIGNED gg_unaligned_17 *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int64_t)(((int64_t)((*t11)))));
  t13 = gg_view((v875).data, 8);
  t14 = v878;
  t15 = ((GG_UNALIGNED gg_unaligned_17 *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int64_t)(((int64_t)((*t15)))));
  (*t8) = ((int32_t)(((int32_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t16)))))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v876;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_25 gg_function_30(gg_type_25 p0, int64_t base) {
  _Alignas(8) gg_type_25 t0;
  _Alignas(8) int16_t t1;
  _Alignas(8) int16_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_16 * t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) uint8_t t14;
  gg_slice t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  GG_UNALIGNED gg_unaligned_16 * t23;
  _Alignas(8) gg_type_25 v0;
  _Alignas(8) gg_type_25 v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  v3 = ((int64_t)(((int64_t)gg_div(t6, t7))));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)gg_mod(t8, t9))));
  /* FG_DIRECT_SLICE_INDEX */
  t10 = v2;
  t11 = ((GG_UNALIGNED gg_unaligned_16 *)(v1).data) + gg_index(t10, ((int64_t)UINT64_C(16)));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(6))));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t15 = gg_view((v0).data, 16);
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)UINT64_C(8))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
  t21 = ((int64_t)(((int64_t)UINT64_C(2))));
  t22 = ((int64_t)((uint64_t)(t20) + (uint64_t)(t21)));
  t23 = ((GG_UNALIGNED gg_unaligned_16 *)t15.data) + gg_index(t22, t15.len);
  (*t11) = ((int16_t)((*t23)));
  }
  } else {
  {
  (*t11) = ((int16_t)(((int16_t)(((int64_t)UINT64_C(0))))));
  }
  }
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v1;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:madd16 */
#if FGOOSE_PACKED && GG_PACKED_X86
__attribute__((target("avx2"))) static gg_type_21 gg_packed_27_avx2(gg_type_25 p858, gg_type_25 p859, int64_t base) {
  gg_type_21 packed_result;
  _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_madd_epi16(_mm256_loadu_si256((const __m256i*)p858.data), _mm256_loadu_si256((const __m256i*)p859.data)));
  return packed_result;
}
#endif
static GG_INLINE gg_type_21 gg_function_27(gg_type_25 p858, gg_type_25 p859, int64_t base) {
#if FGOOSE_PACKED && GG_PACKED_X86
  if(gg_packed_available(0)) { FGOOSE_SIMD_HOOK(27); return gg_packed_27_avx2(p858, p859, base); }
#endif
  _Alignas(8) gg_type_21 t0;
  _Alignas(8) int32_t t1;
  _Alignas(8) int32_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  GG_UNALIGNED gg_unaligned_16 * t10;
  _Alignas(8) int64_t t11;
  gg_slice t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  GG_UNALIGNED gg_unaligned_16 * t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  gg_slice t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  GG_UNALIGNED gg_unaligned_16 * t25;
  _Alignas(8) int64_t t26;
  gg_slice t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  GG_UNALIGNED gg_unaligned_16 * t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  gg_slice t36;
  _Alignas(8) int64_t t37;
  GG_UNALIGNED gg_unaligned_17 * t38;
  _Alignas(8) gg_type_25 v858;
  _Alignas(8) gg_type_25 v859;
  _Alignas(8) gg_type_21 v860;
  _Alignas(8) int64_t v862;
  _Alignas(8) int64_t v863;
  v858 = p858;
  v859 = p859;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v860).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v862 = t4;
  {
  t6 = gg_view((v858).data, 16);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v862));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((GG_UNALIGNED gg_unaligned_16 *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int64_t)(((int64_t)((*t10)))));
  t12 = gg_view((v859).data, 16);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v862));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((GG_UNALIGNED gg_unaligned_16 *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int64_t)(((int64_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v858).data, 16);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v862));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((GG_UNALIGNED gg_unaligned_16 *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int64_t)(((int64_t)((*t25)))));
  t27 = gg_view((v859).data, 16);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v862));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((GG_UNALIGNED gg_unaligned_16 *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int64_t)(((int64_t)((*t33)))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v863 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  t36 = gg_view((v860).data, 8);
  t37 = v862;
  t38 = ((GG_UNALIGNED gg_unaligned_17 *)t36.data) + gg_index(t37, t36.len);
  (*t38) = ((int32_t)(((int32_t)(v863))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v860;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_21 gg_function_26(gg_type_25 p0, gg_type_25 p1, int64_t base) {
  _Alignas(8) gg_type_21 t0;
  _Alignas(8) gg_type_25 t1;
  _Alignas(8) gg_type_25 t2;
  _Alignas(8) gg_type_25 v0;
  _Alignas(8) gg_type_25 v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = v0;
  t2 = v1;
  t0 = gg_function_27(t1, t2, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_25 gg_function_29(gg_type_25 p0, int64_t base) {
  _Alignas(8) gg_type_25 t0;
  _Alignas(8) int16_t t1;
  _Alignas(8) int16_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  GG_UNALIGNED gg_unaligned_16 * t7;
  gg_slice t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  GG_UNALIGNED gg_unaligned_16 * t12;
  _Alignas(8) gg_type_25 v0;
  _Alignas(8) gg_type_25 v1;
  _Alignas(8) int64_t v2;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((GG_UNALIGNED gg_unaligned_16 *)(v1).data) + gg_index(t6, ((int64_t)UINT64_C(16)));
  t8 = gg_view((v0).data, 16);
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)gg_mod(t9, t10));
  t12 = ((GG_UNALIGNED gg_unaligned_16 *)t8.data) + gg_index(t11, t8.len);
  (*t7) = ((int16_t)((*t12)));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v1;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:simd_add_i16 */
#if FGOOSE_PACKED && GG_PACKED_X86
__attribute__((target("avx2"))) static gg_type_25 gg_packed_25_avx2(gg_type_25 p713, gg_type_25 p714, int64_t base) {
  gg_type_25 packed_result;
  _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_add_epi16(_mm256_loadu_si256((const __m256i*)p713.data), _mm256_loadu_si256((const __m256i*)p714.data)));
  return packed_result;
}
#endif
static GG_INLINE gg_type_25 gg_function_25(gg_type_25 p713, gg_type_25 p714, int64_t base) {
#if FGOOSE_PACKED && GG_PACKED_X86
  if(gg_packed_available(0)) { FGOOSE_SIMD_HOOK(25); return gg_packed_25_avx2(p713, p714, base); }
#endif
  _Alignas(8) gg_type_25 t0;
  _Alignas(8) int16_t t1;
  _Alignas(8) int16_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_16 * t8;
  gg_slice t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_16 * t11;
  _Alignas(8) int64_t t12;
  gg_slice t13;
  _Alignas(8) int64_t t14;
  GG_UNALIGNED gg_unaligned_16 * t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) gg_type_25 v713;
  _Alignas(8) gg_type_25 v714;
  _Alignas(8) gg_type_25 v715;
  _Alignas(8) int64_t v717;
  v713 = p713;
  v714 = p714;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v715).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v717 = t4;
  {
  t6 = gg_view((v715).data, 16);
  t7 = v717;
  t8 = ((GG_UNALIGNED gg_unaligned_16 *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v713).data, 16);
  t10 = v717;
  t11 = ((GG_UNALIGNED gg_unaligned_16 *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int64_t)(((int64_t)((*t11)))));
  t13 = gg_view((v714).data, 16);
  t14 = v717;
  t15 = ((GG_UNALIGNED gg_unaligned_16 *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int64_t)(((int64_t)((*t15)))));
  (*t8) = ((int16_t)(((int16_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t16)))))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v715;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_25 gg_function_24(gg_type_18 p0, gg_type_31 p1, int64_t base) {
  _Alignas(8) gg_type_25 t0;
  _Alignas(8) int16_t t1;
  _Alignas(8) int16_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  GG_UNALIGNED gg_unaligned_2 * t10;
  _Alignas(8) int32_t t11;
  gg_slice t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  GG_UNALIGNED gg_unaligned_30 * t16;
  _Alignas(8) int32_t t17;
  _Alignas(8) int32_t t18;
  gg_slice t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  GG_UNALIGNED gg_unaligned_2 * t25;
  _Alignas(8) int32_t t26;
  gg_slice t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  GG_UNALIGNED gg_unaligned_30 * t33;
  _Alignas(8) int32_t t34;
  _Alignas(8) int32_t t35;
  _Alignas(8) int64_t t36;
  GG_UNALIGNED gg_unaligned_16 * t37;
  _Alignas(8) int32_t t38;
  _Alignas(8) int32_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int32_t t41;
  _Alignas(8) uint8_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int32_t t44;
  _Alignas(8) int32_t t45;
  _Alignas(8) uint8_t t46;
  _Alignas(8) gg_type_18 v0;
  _Alignas(8) gg_type_31 v1;
  _Alignas(8) gg_type_25 v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int32_t v4;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  t6 = gg_view((v0).data, 32);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((GG_UNALIGNED gg_unaligned_2 *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int32_t)(((int32_t)((*t10)))));
  t12 = gg_view((v1).data, 32);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((GG_UNALIGNED gg_unaligned_30 *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int32_t)(((int32_t)((*t16)))));
  t18 = ((int32_t)(((int32_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v0).data, 32);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((GG_UNALIGNED gg_unaligned_2 *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int32_t)(((int32_t)((*t25)))));
  t27 = gg_view((v1).data, 32);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v3));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((GG_UNALIGNED gg_unaligned_30 *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int32_t)(((int32_t)((*t33)))));
  t35 = ((int32_t)(((int32_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v4 = ((int32_t)(((int32_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  /* FG_DIRECT_SLICE_INDEX */
  t36 = v3;
  t37 = ((GG_UNALIGNED gg_unaligned_16 *)(v2).data) + gg_index(t36, ((int64_t)UINT64_C(16)));
  t39 = ((int32_t)(v4));
  t40 = ((int64_t)UINT64_C(32768));
  t41 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t40)))));
  t42 = ((t39) < (t41));
  if (t42) {
  {
  t43 = ((int64_t)UINT64_C(32768));
  t38 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t43)))));
  }
  } else {
  t44 = ((int32_t)(v4));
  t45 = ((int32_t)(((int64_t)UINT64_C(32767))));
  t46 = ((t44) > (t45));
  if (t46) {
  {
  t38 = ((int32_t)(((int64_t)UINT64_C(32767))));
  }
  } else {
  {
  t38 = ((int32_t)(v4));
  }
  }
  }
  (*t37) = ((int16_t)(((int16_t)(t38))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v2;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_25 gg_function_23(gg_type_18 p0, gg_type_31 p1, int64_t base) {
  _Alignas(8) gg_type_25 t0;
  _Alignas(8) gg_type_18 t1;
  _Alignas(8) gg_type_31 t2;
  _Alignas(8) gg_type_18 v0;
  _Alignas(8) gg_type_31 v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = v0;
  t2 = v1;
  t0 = gg_function_24(t1, t2, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_18 gg_function_22(gg_type_18 p0, gg_type_18 p1, int64_t p2, int64_t base) {
  _Alignas(8) gg_type_18 t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) uint8_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_2 * t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) uint8_t t18;
  gg_slice t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  GG_UNALIGNED gg_unaligned_2 * t25;
  gg_slice t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  GG_UNALIGNED gg_unaligned_2 * t32;
  _Alignas(8) gg_type_18 v0;
  _Alignas(8) gg_type_18 v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) gg_type_18 v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v3).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v4 = t4;
  {
  t6 = ((int64_t)(v4));
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  v5 = ((int64_t)(((int64_t)gg_div(t6, t7))));
  t8 = ((int64_t)(v4));
  t9 = ((int64_t)(((int64_t)UINT64_C(4))));
  v6 = ((int64_t)(((int64_t)gg_mod(t8, t9))));
  /* FG_DIRECT_SLICE_INDEX */
  t10 = v4;
  t11 = ((GG_UNALIGNED gg_unaligned_2 *)(v3).data) + gg_index(t10, ((int64_t)UINT64_C(32)));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(((t12) >> ((uint64_t)(t13) & 63))));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((int64_t)(((t14) & (t15))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  if (t18) {
  {
  t19 = gg_view((v1).data, 32);
  t20 = ((int64_t)(v5));
  t21 = ((int64_t)(((int64_t)UINT64_C(4))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((GG_UNALIGNED gg_unaligned_2 *)t19.data) + gg_index(t24, t19.len);
  (*t11) = ((uint8_t)((*t25)));
  }
  } else {
  {
  t26 = gg_view((v0).data, 32);
  t27 = ((int64_t)(v5));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(v6));
  t31 = ((int64_t)((uint64_t)(t29) + (uint64_t)(t30)));
  t32 = ((GG_UNALIGNED gg_unaligned_2 *)t26.data) + gg_index(t31, t26.len);
  (*t11) = ((uint8_t)((*t32)));
  }
  }
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v3;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_18 gg_function_20(gg_type_18 p0, int64_t p1, int64_t base) {
  _Alignas(8) gg_type_18 t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) uint8_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  GG_UNALIGNED gg_unaligned_2 * t21;
  gg_slice t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  GG_UNALIGNED gg_unaligned_2 * t32;
  _Alignas(8) gg_type_18 v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) gg_type_18 v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(((int64_t)UINT64_C(16))));
  v4 = ((int64_t)(((int64_t)gg_div(t6, t7))));
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(16))));
  t10 = ((int64_t)(((int64_t)gg_mod(t8, t9))));
  t11 = ((int64_t)(((int64_t)UINT64_C(4))));
  t12 = ((int64_t)(((int64_t)gg_div(t10, t11))));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)UINT64_C(4))));
  v6 = ((int64_t)(((int64_t)gg_mod(t14, t15))));
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)(((t16) >> ((uint64_t)(t17) & 63))));
  t19 = ((int64_t)(((int64_t)UINT64_C(3))));
  v7 = ((int64_t)(((t18) & (t19))));
  /* FG_DIRECT_SLICE_INDEX */
  t20 = v3;
  t21 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t20, ((int64_t)UINT64_C(32)));
  t22 = gg_view((v0).data, 32);
  t23 = ((int64_t)(v4));
  t24 = ((int64_t)(((int64_t)UINT64_C(16))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t26 = ((int64_t)(v7));
  t27 = ((int64_t)(((int64_t)UINT64_C(4))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t28)))));
  t30 = ((int64_t)(v6));
  t31 = ((int64_t)((uint64_t)(t29) + (uint64_t)(t30)));
  t32 = ((GG_UNALIGNED gg_unaligned_2 *)t22.data) + gg_index(t31, t22.len);
  (*t21) = ((uint8_t)((*t32)));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v2;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_31 gg_function_21(gg_type_18 p0, int64_t base) {
  _Alignas(8) gg_type_31 t0;
  _Alignas(8) int8_t t1;
  _Alignas(8) int8_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  GG_UNALIGNED gg_unaligned_30 * t7;
  _Alignas(8) int64_t t8;
  GG_UNALIGNED gg_unaligned_2 * t9;
  _Alignas(8) gg_type_18 v0;
  _Alignas(8) gg_type_31 v1;
  _Alignas(8) int64_t v2;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int8_t)(t1));
  for (t3=0; t3<32; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((GG_UNALIGNED gg_unaligned_30 *)(v1).data) + gg_index(t6, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v2;
  t9 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(32)));
  (*t7) = ((int8_t)(((int8_t)((*t9)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v1;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_18 gg_function_19(gg_type_31 p0, int64_t base) {
  _Alignas(8) gg_type_18 t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) uint8_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  GG_UNALIGNED gg_unaligned_2 * t7;
  _Alignas(8) int64_t t8;
  GG_UNALIGNED gg_unaligned_30 * t9;
  _Alignas(8) gg_type_31 v0;
  _Alignas(8) gg_type_18 v1;
  _Alignas(8) int64_t v2;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t6, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v2;
  t9 = ((GG_UNALIGNED gg_unaligned_30 *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(32)));
  (*t7) = ((uint8_t)(((uint8_t)((*t9)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v1;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_31 gg_function_18(gg_type_31 p0, int64_t base) {
  _Alignas(8) gg_type_31 t0;
  _Alignas(8) int8_t t1;
  _Alignas(8) int8_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  GG_UNALIGNED gg_unaligned_30 * t7;
  gg_slice t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  GG_UNALIGNED gg_unaligned_30 * t12;
  _Alignas(8) gg_type_31 v0;
  _Alignas(8) gg_type_31 v1;
  _Alignas(8) int64_t v2;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int8_t)(t1));
  for (t3=0; t3<32; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((GG_UNALIGNED gg_unaligned_30 *)(v1).data) + gg_index(t6, ((int64_t)UINT64_C(32)));
  t8 = gg_view((v0).data, 32);
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(16))));
  t11 = ((int64_t)gg_mod(t9, t10));
  t12 = ((GG_UNALIGNED gg_unaligned_30 *)t8.data) + gg_index(t11, t8.len);
  (*t7) = ((int8_t)((*t12)));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v1;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_31 gg_function_17(gg_slice p0, int64_t p1, int64_t base) {
  _Alignas(8) gg_type_31 t0;
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
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int8_t t13;
  _Alignas(8) int8_t t14;
  int64_t t15;
  int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  GG_UNALIGNED gg_unaligned_30 * t19;
  gg_slice t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  const GG_UNALIGNED gg_unaligned_2 * t24;
  int64_t t25;
  _Alignas(8) gg_slice v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) gg_type_31 v2;
  _Alignas(8) int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)((v0).len));
  t2 = ((int64_t)(((int64_t)UINT64_C(32))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)((v0).len));
  t11 = ((int64_t)(((int64_t)UINT64_C(32))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) - (uint64_t)(t11)))));
  t8 = ((t9) <= (t12));
  }
  if (!(t8)) gg_fail("assertion failed");
  memset(&t13, 0, sizeof(t13));
  t14 = ((int8_t)(t13));
  for (t15=0; t15<32; t15++) (v2).data[t15] = t14;
  t17 = ((int64_t)UINT64_C(32));
  t16 = 0;
label0:;
  if (t16 >= t17) goto label2;
  v3 = t16;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t18 = v3;
  t19 = ((GG_UNALIGNED gg_unaligned_30 *)(v2).data) + gg_index(t18, ((int64_t)UINT64_C(32)));
  t20 = v0;
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)((uint64_t)(t21) + (uint64_t)(t22)));
  t24 = ((const GG_UNALIGNED gg_unaligned_2 *)t20.data) + gg_index(t23, t20.len);
  (*t19) = ((int8_t)(((int8_t)((*t24)))));
  }
label1:;
  t16++; goto label0;
label2:;
  t0 = v2;
  goto gg_done;
  }
gg_done:;
  for (t25 = 0; t25 < 1; t25++) gg_stacks[base + t25].top = 0;
  return t0;
}
static GG_INLINE gg_type_25 gg_function_14(gg_type_20 p0, int64_t base) {
  _Alignas(8) gg_type_25 t0;
  _Alignas(8) int16_t t1;
  _Alignas(8) int16_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  GG_UNALIGNED gg_unaligned_16 * t7;
  _Alignas(8) int64_t t8;
  GG_UNALIGNED gg_unaligned_2 * t9;
  _Alignas(8) gg_type_20 v0;
  _Alignas(8) gg_type_25 v1;
  _Alignas(8) int64_t v2;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((GG_UNALIGNED gg_unaligned_16 *)(v1).data) + gg_index(t6, ((int64_t)UINT64_C(16)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v2;
  t9 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(16)));
  (*t7) = ((int16_t)(((int16_t)((*t9)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v1;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_20 gg_function_16(gg_type_20 p0, gg_type_20 p1, int64_t base) {
  _Alignas(8) gg_type_20 t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) uint8_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  GG_UNALIGNED gg_unaligned_2 * t10;
  _Alignas(8) int64_t t11;
  GG_UNALIGNED gg_unaligned_2 * t12;
  gg_slice t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  GG_UNALIGNED gg_unaligned_2 * t19;
  _Alignas(8) int64_t t20;
  GG_UNALIGNED gg_unaligned_2 * t21;
  _Alignas(8) gg_type_20 v0;
  _Alignas(8) gg_type_20 v1;
  _Alignas(8) gg_type_20 v2;
  _Alignas(8) int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<16; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  t6 = gg_view((v2).data, 16);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((GG_UNALIGNED gg_unaligned_2 *)t6.data) + gg_index(t9, t6.len);
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v3;
  t12 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t11, ((int64_t)UINT64_C(16)));
  (*t10) = ((uint8_t)((*t12)));
  t13 = gg_view((v2).data, 16);
  t14 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = ((int64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  t19 = ((GG_UNALIGNED gg_unaligned_2 *)t13.data) + gg_index(t18, t13.len);
  /* FG_DIRECT_SLICE_INDEX */
  t20 = v3;
  t21 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t20, ((int64_t)UINT64_C(16)));
  (*t19) = ((uint8_t)((*t21)));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v2;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_20 gg_function_13(gg_type_28 p0, int64_t base) {
  _Alignas(8) gg_type_20 t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) uint8_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  GG_UNALIGNED gg_unaligned_17 * t7;
  int64_t t8;
  _Alignas(8) int64_t t9;
  gg_slice t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  GG_UNALIGNED gg_unaligned_2 * t16;
  _Alignas(8) uint32_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) uint8_t t20;
  _Alignas(8) uint32_t t21;
  _Alignas(8) uint32_t t22;
  _Alignas(8) gg_type_28 v0;
  _Alignas(8) gg_type_20 v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) uint32_t v3;
  _Alignas(8) int64_t v4;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<16; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(4));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((GG_UNALIGNED gg_unaligned_17 *)(v0).data) + gg_index(t6, ((int64_t)UINT64_C(4)));
  v3 = ((uint32_t)(((uint32_t)((*t7)))));
  t9 = ((int64_t)UINT64_C(4));
  t8 = 0;
label3:;
  if (t8 >= t9) goto label5;
  v4 = t8;
  {
  t10 = gg_view((v1).data, 16);
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(4))));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  t16 = ((GG_UNALIGNED gg_unaligned_2 *)t10.data) + gg_index(t15, t10.len);
  t17 = ((uint32_t)(v3));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((int64_t)(v4));
  t20 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))))));
  t21 = ((uint32_t)(((t17) >> ((uint64_t)(t20) & 31))));
  t22 = ((uint32_t)(((int64_t)UINT64_C(255))));
  (*t16) = ((uint8_t)(((uint8_t)(((t21) & (t22))))));
  }
label4:;
  t8++; goto label3;
label5:;
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v1;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_28 gg_function_15(gg_type_28 p0, int64_t p1, int64_t base) {
  _Alignas(8) gg_type_28 t0;
  _Alignas(8) int32_t t1;
  _Alignas(8) int32_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  GG_UNALIGNED gg_unaligned_17 * t7;
  gg_slice t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  GG_UNALIGNED gg_unaligned_17 * t16;
  _Alignas(8) gg_type_28 v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) gg_type_28 v2;
  _Alignas(8) int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<4; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(4));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((GG_UNALIGNED gg_unaligned_17 *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(4)));
  t8 = gg_view((v0).data, 4);
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((int64_t)(((t9) >> ((uint64_t)(t12) & 63))));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = ((t13) & (t14));
  t16 = ((GG_UNALIGNED gg_unaligned_17 *)t8.data) + gg_index(t15, t8.len);
  (*t7) = ((int32_t)((*t16)));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v2;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_20 gg_function_3(gg_type_20 p0, gg_type_20 p1, int64_t base) {
  _Alignas(8) gg_type_20 t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) uint8_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  GG_UNALIGNED gg_unaligned_2 * t7;
  gg_slice t8;
  _Alignas(8) int64_t t9;
  GG_UNALIGNED gg_unaligned_2 * t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) int64_t t13;
  GG_UNALIGNED gg_unaligned_2 * t14;
  _Alignas(8) gg_type_20 v0;
  _Alignas(8) gg_type_20 v1;
  _Alignas(8) gg_type_20 v2;
  _Alignas(8) int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<16; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(16)));
  t8 = gg_view((v0).data, 16);
  /* FG_DIRECT_SLICE_INDEX */
  t9 = v3;
  t10 = ((GG_UNALIGNED gg_unaligned_2 *)(v1).data) + gg_index(t9, ((int64_t)UINT64_C(16)));
  t11 = ((uint8_t)((*t10)));
  t12 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t13 = ((int64_t)(((t11) & (t12))));
  t14 = ((GG_UNALIGNED gg_unaligned_2 *)t8.data) + gg_index(t13, t8.len);
  (*t7) = ((uint8_t)((*t14)));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v2;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE uint32_t gg_function_12(gg_slice p0, int64_t p1, int64_t base) {
  _Alignas(8) uint32_t t0;
  _Alignas(8) int64_t t1;
  const GG_UNALIGNED gg_unaligned_2 * t2;
  _Alignas(8) uint32_t t3;
  gg_slice t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  const GG_UNALIGNED gg_unaligned_2 * t8;
  _Alignas(8) uint32_t t9;
  _Alignas(8) uint8_t t10;
  _Alignas(8) uint32_t t11;
  _Alignas(8) uint32_t t12;
  gg_slice t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  const GG_UNALIGNED gg_unaligned_2 * t17;
  _Alignas(8) uint32_t t18;
  _Alignas(8) uint8_t t19;
  _Alignas(8) uint32_t t20;
  _Alignas(8) uint32_t t21;
  gg_slice t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  const GG_UNALIGNED gg_unaligned_2 * t26;
  _Alignas(8) uint32_t t27;
  _Alignas(8) uint8_t t28;
  _Alignas(8) uint32_t t29;
  int64_t t30;
  _Alignas(8) gg_slice v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1 = v1;
  t2 = ((const GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t1, (v0).len);
  t3 = ((uint32_t)(((uint32_t)((*t2)))));
  t4 = v0;
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)((uint64_t)(t5) + (uint64_t)(t6)));
  t8 = ((const GG_UNALIGNED gg_unaligned_2 *)t4.data) + gg_index(t7, t4.len);
  t9 = ((uint32_t)(((uint32_t)((*t8)))));
  t10 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(8))))));
  t11 = ((uint32_t)(((uint32_t)((uint64_t)(t9) << ((uint64_t)(t10) & 31)))));
  t12 = ((uint32_t)(((t3) | (t11))));
  t13 = v0;
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  t16 = ((int64_t)((uint64_t)(t14) + (uint64_t)(t15)));
  t17 = ((const GG_UNALIGNED gg_unaligned_2 *)t13.data) + gg_index(t16, t13.len);
  t18 = ((uint32_t)(((uint32_t)((*t17)))));
  t19 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(16))))));
  t20 = ((uint32_t)(((uint32_t)((uint64_t)(t18) << ((uint64_t)(t19) & 31)))));
  t21 = ((uint32_t)(((t12) | (t20))));
  t22 = v0;
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(3))));
  t25 = ((int64_t)((uint64_t)(t23) + (uint64_t)(t24)));
  t26 = ((const GG_UNALIGNED gg_unaligned_2 *)t22.data) + gg_index(t25, t22.len);
  t27 = ((uint32_t)(((uint32_t)((*t26)))));
  t28 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(24))))));
  t29 = ((uint32_t)(((uint32_t)((uint64_t)(t27) << ((uint64_t)(t28) & 31)))));
  t0 = ((uint32_t)(((t21) | (t29))));
  goto gg_done;
  }
gg_done:;
  for (t30 = 0; t30 < 1; t30++) gg_stacks[base + t30].top = 0;
  return t0;
}
/* FG_PACKED_MATCH:simd_and_u8 */
#if FGOOSE_PACKED && GG_PACKED_X86
__attribute__((target("avx2"))) static gg_type_18 gg_packed_10_avx2(gg_type_18 p367, gg_type_18 p368, int64_t base) {
  gg_type_18 packed_result;
  _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_loadu_si256((const __m256i*)p367.data), _mm256_loadu_si256((const __m256i*)p368.data)));
  return packed_result;
}
#endif
static GG_INLINE gg_type_18 gg_function_10(gg_type_18 p367, gg_type_18 p368, int64_t base) {
#if FGOOSE_PACKED && GG_PACKED_X86
  if(gg_packed_available(0)) { FGOOSE_SIMD_HOOK(10); return gg_packed_10_avx2(p367, p368, base); }
#endif
  _Alignas(8) gg_type_18 t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) uint8_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_2 * t8;
  gg_slice t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_2 * t11;
  _Alignas(8) uint8_t t12;
  gg_slice t13;
  _Alignas(8) int64_t t14;
  GG_UNALIGNED gg_unaligned_2 * t15;
  _Alignas(8) uint8_t t16;
  _Alignas(8) gg_type_18 v367;
  _Alignas(8) gg_type_18 v368;
  _Alignas(8) gg_type_18 v369;
  _Alignas(8) int64_t v371;
  v367 = p367;
  v368 = p368;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v369).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v371 = t4;
  {
  t6 = gg_view((v369).data, 32);
  t7 = v371;
  t8 = ((GG_UNALIGNED gg_unaligned_2 *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v367).data, 32);
  t10 = v371;
  t11 = ((GG_UNALIGNED gg_unaligned_2 *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = gg_view((v368).data, 32);
  t14 = v371;
  t15 = ((GG_UNALIGNED gg_unaligned_2 *)t13.data) + gg_index(t14, t13.len);
  t16 = ((uint8_t)((*t15)));
  (*t8) = ((uint8_t)(((t12) & (t16))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v369;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:simd_shr_u8 */
#if FGOOSE_PACKED && GG_PACKED_X86
__attribute__((target("avx2"))) static gg_type_18 gg_packed_11_avx2(gg_type_18 p412, uint8_t p413, int64_t base) {
  gg_type_18 packed_result;
  _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)p412.data), _mm_cvtsi32_si128(p413 & 7)), _mm256_set1_epi8((char)(255u >> (p413 & 7)))));
  return packed_result;
}
#endif
static GG_INLINE gg_type_18 gg_function_11(gg_type_18 p412, uint8_t p413, int64_t base) {
#if FGOOSE_PACKED && GG_PACKED_X86
  if(gg_packed_available(0)) { FGOOSE_SIMD_HOOK(11); return gg_packed_11_avx2(p412, p413, base); }
#endif
  _Alignas(8) gg_type_18 t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) uint8_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_2 * t8;
  gg_slice t9;
  _Alignas(8) int64_t t10;
  GG_UNALIGNED gg_unaligned_2 * t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) uint8_t t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) gg_type_18 v412;
  _Alignas(8) uint8_t v413;
  _Alignas(8) gg_type_18 v414;
  _Alignas(8) int64_t v416;
  v412 = p412;
  v413 = p413;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v414).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v416 = t4;
  {
  t6 = gg_view((v414).data, 32);
  t7 = v416;
  t8 = ((GG_UNALIGNED gg_unaligned_2 *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v412).data, 32);
  t10 = v416;
  t11 = ((GG_UNALIGNED gg_unaligned_2 *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v413));
  t14 = ((uint8_t)(((int64_t)UINT64_C(7))));
  t15 = ((uint8_t)(((t13) & (t14))));
  (*t8) = ((uint8_t)(((t12) >> ((uint64_t)(t15) & 7))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v414;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE gg_type_18 gg_function_9(gg_slice p0, int64_t p1, int64_t base) {
  _Alignas(8) gg_type_18 t0;
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
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) uint8_t t13;
  _Alignas(8) uint8_t t14;
  int64_t t15;
  int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  GG_UNALIGNED gg_unaligned_2 * t19;
  gg_slice t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  const GG_UNALIGNED gg_unaligned_2 * t24;
  int64_t t25;
  _Alignas(8) gg_slice v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) gg_type_18 v2;
  _Alignas(8) int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)((v0).len));
  t2 = ((int64_t)(((int64_t)UINT64_C(32))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)((v0).len));
  t11 = ((int64_t)(((int64_t)UINT64_C(32))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) - (uint64_t)(t11)))));
  t8 = ((t9) <= (t12));
  }
  if (!(t8)) gg_fail("assertion failed");
  memset(&t13, 0, sizeof(t13));
  t14 = ((uint8_t)(t13));
  for (t15=0; t15<32; t15++) (v2).data[t15] = t14;
  t17 = ((int64_t)UINT64_C(32));
  t16 = 0;
label0:;
  if (t16 >= t17) goto label2;
  v3 = t16;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t18 = v3;
  t19 = ((GG_UNALIGNED gg_unaligned_2 *)(v2).data) + gg_index(t18, ((int64_t)UINT64_C(32)));
  t20 = v0;
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)((uint64_t)(t21) + (uint64_t)(t22)));
  t24 = ((const GG_UNALIGNED gg_unaligned_2 *)t20.data) + gg_index(t23, t20.len);
  (*t19) = ((uint8_t)(((uint8_t)((*t24)))));
  }
label1:;
  t16++; goto label0;
label2:;
  t0 = v2;
  goto gg_done;
  }
gg_done:;
  for (t25 = 0; t25 < 1; t25++) gg_stacks[base + t25].top = 0;
  return t0;
}
static GG_INLINE gg_type_25 gg_function_8(gg_type_25 p0, gg_type_25 p1, int64_t base) {
  _Alignas(8) gg_type_25 t0;
  _Alignas(8) int16_t t1;
  _Alignas(8) int16_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  GG_UNALIGNED gg_unaligned_16 * t7;
  gg_slice t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  GG_UNALIGNED gg_unaligned_16 * t12;
  _Alignas(8) int64_t t13;
  gg_slice t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  GG_UNALIGNED gg_unaligned_16 * t20;
  _Alignas(8) int64_t t21;
  int64_t t22;
  _Alignas(8) int64_t t23;
  gg_slice t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  GG_UNALIGNED gg_unaligned_16 * t28;
  gg_slice t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  GG_UNALIGNED gg_unaligned_16 * t33;
  _Alignas(8) int64_t t34;
  gg_slice t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  GG_UNALIGNED gg_unaligned_16 * t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) gg_type_25 v0;
  _Alignas(8) gg_type_25 v1;
  _Alignas(8) gg_type_25 v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((GG_UNALIGNED gg_unaligned_16 *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(16)));
  t8 = gg_view((v0).data, 16);
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)((uint64_t)(t9) * (uint64_t)(t10)));
  t12 = ((GG_UNALIGNED gg_unaligned_16 *)t8.data) + gg_index(t11, t8.len);
  t13 = ((int64_t)(((int64_t)((*t12)))));
  t14 = gg_view((v0).data, 16);
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)((uint64_t)(t17) + (uint64_t)(t18)));
  t20 = ((GG_UNALIGNED gg_unaligned_16 *)t14.data) + gg_index(t19, t14.len);
  t21 = ((int64_t)(((int64_t)((*t20)))));
  (*t7) = ((int16_t)(((int16_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t21)))))));
  }
label1:;
  t4++; goto label0;
label2:;
  t23 = ((int64_t)UINT64_C(8));
  t22 = 0;
label3:;
  if (t22 >= t23) goto label5;
  v4 = t22;
  {
  t24 = gg_view((v2).data, 16);
  t25 = ((int64_t)(((int64_t)UINT64_C(8))));
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  t28 = ((GG_UNALIGNED gg_unaligned_16 *)t24.data) + gg_index(t27, t24.len);
  t29 = gg_view((v1).data, 16);
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = ((int64_t)(v4));
  t32 = ((int64_t)((uint64_t)(t30) * (uint64_t)(t31)));
  t33 = ((GG_UNALIGNED gg_unaligned_16 *)t29.data) + gg_index(t32, t29.len);
  t34 = ((int64_t)(((int64_t)((*t33)))));
  t35 = gg_view((v1).data, 16);
  t36 = ((int64_t)(((int64_t)UINT64_C(2))));
  t37 = ((int64_t)(v4));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = ((int64_t)((uint64_t)(t38) + (uint64_t)(t39)));
  t41 = ((GG_UNALIGNED gg_unaligned_16 *)t35.data) + gg_index(t40, t35.len);
  t42 = ((int64_t)(((int64_t)((*t41)))));
  (*t28) = ((int16_t)(((int16_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t42)))))));
  }
label4:;
  t22++; goto label3;
label5:;
  t0 = v2;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static GG_INLINE uint32_t gg_function_7(gg_slice p0, int64_t p1, int64_t base) {
  /* FG_SHARED_BODY:12 */
  return gg_function_12(p0, p1, base);
}
static GG_INLINE float gg_function_6(gg_slice p0, int64_t p1, int64_t base) {
  _Alignas(8) float t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint32_t t3;
  _Alignas(8) float t4;
  int64_t t5;
  _Alignas(8) gg_slice v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  t3 = gg_function_12(t1, t2, base + 1);
#ifndef __cplusplus
  extern float (gs_simd_f32_from_bits)(uint32_t);
#endif
  t4 = gs_simd_f32_from_bits(t3);
  t0 = ((float)(t4));
  goto gg_done;
  }
gg_done:;
  for (t5 = 0; t5 < 1; t5++) gg_stacks[base + t5].top = 0;
  return t0;
}
static float gg_function_4(gg_type_20 p0, int64_t p1, int64_t base) {
  _Alignas(8) float t0;
  _Alignas(8) int64_t t1;
  GG_UNALIGNED gg_unaligned_2 * t2;
  _Alignas(8) uint32_t t3;
  gg_slice t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_2 * t8;
  _Alignas(8) uint32_t t9;
  _Alignas(8) uint8_t t10;
  _Alignas(8) uint32_t t11;
  _Alignas(8) uint32_t t12;
  _Alignas(8) uint32_t t13;
  _Alignas(8) uint32_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) uint32_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) uint32_t t18;
  _Alignas(8) uint32_t t19;
  _Alignas(8) uint32_t t20;
  _Alignas(8) uint32_t t21;
  _Alignas(8) uint32_t t22;
  _Alignas(8) uint32_t t23;
  _Alignas(8) uint8_t t24;
  _Alignas(8) uint32_t t25;
  _Alignas(8) uint32_t t26;
  _Alignas(8) uint32_t t27;
  _Alignas(8) uint32_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) uint32_t t30;
  _Alignas(8) uint32_t t31;
  _Alignas(8) float t32;
  _Alignas(8) uint32_t t33;
  _Alignas(8) uint32_t t34;
  _Alignas(8) uint8_t t35;
  _Alignas(8) uint32_t t36;
  _Alignas(8) uint32_t t37;
  _Alignas(8) uint8_t t38;
  _Alignas(8) uint32_t t39;
  _Alignas(8) float t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) uint32_t t42;
  _Alignas(8) uint32_t t43;
  _Alignas(8) uint32_t t44;
  _Alignas(8) uint32_t t45;
  _Alignas(8) uint8_t t46;
  uint8_t t47;
  _Alignas(8) uint32_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int32_t t50;
  _Alignas(8) int32_t t51;
  _Alignas(8) uint32_t t52;
  _Alignas(8) uint32_t t53;
  _Alignas(8) uint32_t t54;
  _Alignas(8) int32_t t55;
  _Alignas(8) int32_t t56;
  _Alignas(8) uint32_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) uint32_t t59;
  _Alignas(8) uint32_t t60;
  _Alignas(8) uint32_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) uint32_t t63;
  _Alignas(8) uint32_t t64;
  _Alignas(8) float t65;
  _Alignas(8) uint32_t t66;
  _Alignas(8) uint32_t t67;
  _Alignas(8) uint32_t t68;
  _Alignas(8) uint32_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) uint32_t t71;
  _Alignas(8) uint32_t t72;
  _Alignas(8) uint32_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) uint32_t t75;
  _Alignas(8) uint32_t t76;
  _Alignas(8) float t77;
  _Alignas(8) gg_type_20 v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) uint32_t v2;
  _Alignas(8) uint32_t v3;
  _Alignas(8) uint32_t v4;
  _Alignas(8) uint32_t v5;
  _Alignas(8) int32_t v6;
  v0 = p0;
  v1 = p1;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1 = v1;
  t2 = ((GG_UNALIGNED gg_unaligned_2 *)(v0).data) + gg_index(t1, ((int64_t)UINT64_C(16)));
  t3 = ((uint32_t)(((uint32_t)((*t2)))));
  t4 = gg_view((v0).data, 16);
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)((uint64_t)(t5) + (uint64_t)(t6)));
  t8 = ((GG_UNALIGNED gg_unaligned_2 *)t4.data) + gg_index(t7, t4.len);
  t9 = ((uint32_t)(((uint32_t)((*t8)))));
  t10 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(8))))));
  t11 = ((uint32_t)(((uint32_t)((uint64_t)(t9) << ((uint64_t)(t10) & 31)))));
  v2 = ((uint32_t)(((t3) | (t11))));
  t12 = ((uint32_t)(v2));
  t13 = ((uint32_t)(((int64_t)UINT64_C(32768))));
  t14 = ((uint32_t)(((t12) & (t13))));
  t15 = ((int64_t)(((int64_t)UINT64_C(16))));
  v3 = ((uint32_t)(((uint32_t)((uint64_t)(t14) << ((uint64_t)(t15) & 31)))));
  t16 = ((uint32_t)(v2));
  t17 = ((int64_t)(((int64_t)UINT64_C(10))));
  t18 = ((uint32_t)(((t16) >> ((uint64_t)(t17) & 31))));
  t19 = ((uint32_t)(((int64_t)UINT64_C(31))));
  v4 = ((uint32_t)(((t18) & (t19))));
  t20 = ((uint32_t)(v2));
  t21 = ((uint32_t)(((int64_t)UINT64_C(1023))));
  v5 = ((uint32_t)(((t20) & (t21))));
  t22 = ((uint32_t)(v4));
  t23 = ((uint32_t)(((int64_t)UINT64_C(31))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t25 = ((uint32_t)(v3));
  t26 = ((uint32_t)(((int64_t)UINT64_C(2139095040))));
  t27 = ((uint32_t)(((t25) | (t26))));
  t28 = ((uint32_t)(v5));
  t29 = ((int64_t)(((int64_t)UINT64_C(13))));
  t30 = ((uint32_t)(((uint32_t)((uint64_t)(t28) << ((uint64_t)(t29) & 31)))));
  t31 = ((uint32_t)(((t27) | (t30))));
#ifndef __cplusplus
  extern float (gs_simd_f32_from_bits)(uint32_t);
#endif
  t32 = gs_simd_f32_from_bits(t31);
  t0 = ((float)(t32));
  goto gg_done;
  }
  }
  t33 = ((uint32_t)(v4));
  t34 = ((uint32_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t36 = ((uint32_t)(v5));
  t37 = ((uint32_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  t39 = ((uint32_t)(v3));
#ifndef __cplusplus
  extern float (gs_simd_f32_from_bits)(uint32_t);
#endif
  t40 = gs_simd_f32_from_bits(t39);
  t0 = ((float)(t40));
  goto gg_done;
  }
  }
  t41 = ((int64_t)UINT64_C(14));
  v6 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t41)))));
label1:;
  t42 = ((uint32_t)(v5));
  t43 = ((uint32_t)(((int64_t)UINT64_C(1024))));
  t44 = ((uint32_t)(((t42) & (t43))));
  t45 = ((uint32_t)(((int64_t)UINT64_C(0))));
  t46 = ((t44) == (t45));
  t47 = t46;
  if(!t47) goto label2;
  {
  t48 = v5;
  t49 = ((int64_t)UINT64_C(1));
  v5 = ((uint32_t)((uint64_t)(t48) << ((uint64_t)(t49) & 31)));
  t50 = v6;
  t51 = ((int32_t)(((int64_t)UINT64_C(1))));
  v6 = ((int32_t)((uint64_t)(t50) - (uint64_t)(t51)));
  }
  goto label1;
label2:;
  t52 = v5;
  t53 = ((uint32_t)(((int64_t)UINT64_C(1023))));
  v5 = ((t52) & (t53));
  t54 = ((uint32_t)(v3));
  t55 = ((int32_t)(v6));
  t56 = ((int32_t)(((int64_t)UINT64_C(127))));
  t57 = ((uint32_t)(((uint32_t)(((int32_t)((uint64_t)(t55) + (uint64_t)(t56)))))));
  t58 = ((int64_t)(((int64_t)UINT64_C(23))));
  t59 = ((uint32_t)(((uint32_t)((uint64_t)(t57) << ((uint64_t)(t58) & 31)))));
  t60 = ((uint32_t)(((t54) | (t59))));
  t61 = ((uint32_t)(v5));
  t62 = ((int64_t)(((int64_t)UINT64_C(13))));
  t63 = ((uint32_t)(((uint32_t)((uint64_t)(t61) << ((uint64_t)(t62) & 31)))));
  t64 = ((uint32_t)(((t60) | (t63))));
#ifndef __cplusplus
  extern float (gs_simd_f32_from_bits)(uint32_t);
#endif
  t65 = gs_simd_f32_from_bits(t64);
  t0 = ((float)(t65));
  goto gg_done;
  }
  }
  t66 = ((uint32_t)(v3));
  t67 = ((uint32_t)(v4));
  t68 = ((uint32_t)(((int64_t)UINT64_C(112))));
  t69 = ((uint32_t)(((uint32_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)UINT64_C(23))));
  t71 = ((uint32_t)(((uint32_t)((uint64_t)(t69) << ((uint64_t)(t70) & 31)))));
  t72 = ((uint32_t)(((t66) | (t71))));
  t73 = ((uint32_t)(v5));
  t74 = ((int64_t)(((int64_t)UINT64_C(13))));
  t75 = ((uint32_t)(((uint32_t)((uint64_t)(t73) << ((uint64_t)(t74) & 31)))));
  t76 = ((uint32_t)(((t72) | (t75))));
#ifndef __cplusplus
  extern float (gs_simd_f32_from_bits)(uint32_t);
#endif
  t77 = gs_simd_f32_from_bits(t76);
  t0 = ((float)(t77));
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:simd_splat_u8 */
#if FGOOSE_PACKED && GG_PACKED_X86
__attribute__((target("avx2"))) static gg_type_18 gg_packed_2_avx2(uint8_t p265, int64_t base) {
  gg_type_18 packed_result;
  _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_set1_epi8((char)p265));
  return packed_result;
}
#endif
static GG_INLINE gg_type_18 gg_function_2(uint8_t p265, int64_t base) {
#if FGOOSE_PACKED && GG_PACKED_X86
  if(gg_packed_available(0)) { FGOOSE_SIMD_HOOK(2); return gg_packed_2_avx2(p265, base); }
#endif
  _Alignas(8) gg_type_18 t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) uint8_t t2;
  int64_t t3;
  int64_t t4;
  _Alignas(8) int64_t t5;
  gg_slice t6;
  _Alignas(8) int64_t t7;
  GG_UNALIGNED gg_unaligned_2 * t8;
  _Alignas(8) uint8_t v265;
  _Alignas(8) gg_type_18 v266;
  _Alignas(8) int64_t v268;
  v265 = p265;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v266).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v268 = t4;
  {
  t6 = gg_view((v266).data, 32);
  t7 = v268;
  t8 = ((GG_UNALIGNED gg_unaligned_2 *)t6.data) + gg_index(t7, t6.len);
  (*t8) = ((uint8_t)(v265));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v266;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_1(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, gg_slice p4, int64_t base) {
  _Alignas(8) uint8_t t0;
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
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
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
  int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) float t30;
  _Alignas(8) float t31;
  int64_t t32;
  _Alignas(8) float t33;
  _Alignas(8) float t34;
  int64_t t35;
  int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) uint8_t t46;
  _Alignas(8) uint8_t t47;
  int64_t t48;
  _Alignas(8) uint8_t t49;
  _Alignas(8) uint8_t t50;
  int64_t t51;
  int64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  GG_UNALIGNED gg_unaligned_2 * t55;
  gg_slice t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  const GG_UNALIGNED gg_unaligned_2 * t60;
  _Alignas(8) int64_t t61;
  GG_UNALIGNED gg_unaligned_2 * t62;
  gg_slice t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  const GG_UNALIGNED gg_unaligned_2 * t69;
  _Alignas(8) gg_type_20 t70;
  _Alignas(8) gg_type_20 t71;
  _Alignas(8) float t72;
  _Alignas(8) float t73;
  int64_t t74;
  _Alignas(8) float t75;
  _Alignas(8) float t76;
  int64_t t77;
  int64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) int64_t t80;
  GG_UNALIGNED gg_unaligned_3 * t81;
  _Alignas(8) gg_type_20 t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) int64_t t84;
  _Alignas(8) int64_t t85;
  _Alignas(8) int64_t t86;
  GG_UNALIGNED gg_unaligned_3 * t87;
  _Alignas(8) gg_type_20 t88;
  _Alignas(8) int64_t t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  _Alignas(8) gg_slice t92;
  _Alignas(8) int64_t t93;
  _Alignas(8) int32_t t94;
  _Alignas(8) int32_t t95;
  int64_t t96;
  _Alignas(8) int32_t t97;
  _Alignas(8) int32_t t98;
  int64_t t99;
  _Alignas(8) int16_t t100;
  _Alignas(8) int16_t t101;
  int64_t t102;
  int64_t t103;
  _Alignas(8) int64_t t104;
  _Alignas(8) int64_t t105;
  GG_UNALIGNED gg_unaligned_16 * t106;
  gg_slice t107;
  _Alignas(8) int64_t t108;
  _Alignas(8) int64_t t109;
  _Alignas(8) int64_t t110;
  _Alignas(8) int64_t t111;
  _Alignas(8) int64_t t112;
  _Alignas(8) int64_t t113;
  _Alignas(8) int64_t t114;
  const GG_UNALIGNED gg_unaligned_2 * t115;
  _Alignas(8) uint16_t t116;
  gg_slice t117;
  _Alignas(8) int64_t t118;
  _Alignas(8) int64_t t119;
  _Alignas(8) int64_t t120;
  _Alignas(8) int64_t t121;
  _Alignas(8) int64_t t122;
  _Alignas(8) int64_t t123;
  _Alignas(8) int64_t t124;
  const GG_UNALIGNED gg_unaligned_2 * t125;
  _Alignas(8) uint16_t t126;
  _Alignas(8) uint8_t t127;
  _Alignas(8) uint16_t t128;
  _Alignas(8) gg_type_25 t129;
  _Alignas(8) gg_type_25 t130;
  _Alignas(8) int16_t t131;
  _Alignas(8) int16_t t132;
  int64_t t133;
  int64_t t134;
  _Alignas(8) int64_t t135;
  _Alignas(8) int64_t t136;
  GG_UNALIGNED gg_unaligned_16 * t137;
  gg_slice t138;
  _Alignas(8) int64_t t139;
  _Alignas(8) int64_t t140;
  _Alignas(8) int64_t t141;
  GG_UNALIGNED gg_unaligned_16 * t142;
  int64_t t143;
  _Alignas(8) int64_t t144;
  _Alignas(8) int64_t t145;
  _Alignas(8) int64_t t146;
  _Alignas(8) int64_t t147;
  _Alignas(8) int64_t t148;
  _Alignas(8) int64_t t149;
  _Alignas(8) int64_t t150;
  _Alignas(8) gg_slice t151;
  _Alignas(8) int64_t t152;
  _Alignas(8) int64_t t153;
  _Alignas(8) int64_t t154;
  _Alignas(8) gg_type_18 t155;
  _Alignas(8) gg_type_18 t156;
  _Alignas(8) gg_slice t157;
  _Alignas(8) int64_t t158;
  _Alignas(8) int64_t t159;
  _Alignas(8) int64_t t160;
  _Alignas(8) gg_type_18 t161;
  _Alignas(8) gg_type_18 t162;
  _Alignas(8) gg_slice t163;
  _Alignas(8) int64_t t164;
  _Alignas(8) int64_t t165;
  _Alignas(8) int64_t t166;
  _Alignas(8) gg_type_18 t167;
  _Alignas(8) gg_type_18 t168;
  _Alignas(8) gg_slice t169;
  _Alignas(8) int64_t t170;
  _Alignas(8) int64_t t171;
  _Alignas(8) int64_t t172;
  _Alignas(8) gg_type_18 t173;
  _Alignas(8) gg_type_18 t174;
  _Alignas(8) gg_slice t175;
  _Alignas(8) int64_t t176;
  _Alignas(8) int64_t t177;
  _Alignas(8) int64_t t178;
  _Alignas(8) gg_type_18 t179;
  _Alignas(8) gg_type_18 t180;
  _Alignas(8) gg_slice t181;
  _Alignas(8) int64_t t182;
  _Alignas(8) int64_t t183;
  _Alignas(8) int64_t t184;
  _Alignas(8) gg_type_18 t185;
  _Alignas(8) gg_type_18 t186;
  _Alignas(8) gg_slice t187;
  _Alignas(8) int64_t t188;
  _Alignas(8) int64_t t189;
  _Alignas(8) int64_t t190;
  _Alignas(8) gg_type_18 t191;
  _Alignas(8) gg_type_18 t192;
  _Alignas(8) gg_slice t193;
  _Alignas(8) int64_t t194;
  _Alignas(8) int64_t t195;
  _Alignas(8) int64_t t196;
  _Alignas(8) gg_type_18 t197;
  _Alignas(8) gg_type_18 t198;
  _Alignas(8) gg_slice t199;
  _Alignas(8) int64_t t200;
  _Alignas(8) int64_t t201;
  _Alignas(8) int64_t t202;
  _Alignas(8) gg_type_18 t203;
  _Alignas(8) uint8_t t204;
  _Alignas(8) gg_type_18 t205;
  _Alignas(8) gg_type_18 t206;
  _Alignas(8) gg_slice t207;
  _Alignas(8) int64_t t208;
  _Alignas(8) int64_t t209;
  _Alignas(8) int64_t t210;
  _Alignas(8) gg_type_18 t211;
  _Alignas(8) uint8_t t212;
  _Alignas(8) gg_type_18 t213;
  _Alignas(8) gg_type_18 t214;
  _Alignas(8) gg_slice t215;
  _Alignas(8) int64_t t216;
  _Alignas(8) int64_t t217;
  _Alignas(8) int64_t t218;
  _Alignas(8) gg_type_18 t219;
  _Alignas(8) uint8_t t220;
  _Alignas(8) gg_type_18 t221;
  _Alignas(8) gg_type_18 t222;
  _Alignas(8) gg_slice t223;
  _Alignas(8) int64_t t224;
  _Alignas(8) int64_t t225;
  _Alignas(8) int64_t t226;
  _Alignas(8) gg_type_18 t227;
  _Alignas(8) uint8_t t228;
  _Alignas(8) gg_type_18 t229;
  _Alignas(8) gg_type_18 t230;
  _Alignas(8) gg_slice t231;
  _Alignas(8) int64_t t232;
  _Alignas(8) int64_t t233;
  _Alignas(8) int64_t t234;
  _Alignas(8) gg_type_18 t235;
  _Alignas(8) uint8_t t236;
  _Alignas(8) gg_type_18 t237;
  _Alignas(8) gg_type_18 t238;
  _Alignas(8) gg_slice t239;
  _Alignas(8) int64_t t240;
  _Alignas(8) int64_t t241;
  _Alignas(8) int64_t t242;
  _Alignas(8) gg_type_18 t243;
  _Alignas(8) uint8_t t244;
  _Alignas(8) gg_type_18 t245;
  _Alignas(8) gg_type_18 t246;
  _Alignas(8) gg_slice t247;
  _Alignas(8) int64_t t248;
  _Alignas(8) int64_t t249;
  _Alignas(8) int64_t t250;
  _Alignas(8) gg_type_18 t251;
  _Alignas(8) uint8_t t252;
  _Alignas(8) gg_type_18 t253;
  _Alignas(8) gg_type_18 t254;
  _Alignas(8) gg_slice t255;
  _Alignas(8) int64_t t256;
  _Alignas(8) int64_t t257;
  _Alignas(8) int64_t t258;
  _Alignas(8) gg_type_18 t259;
  _Alignas(8) uint8_t t260;
  _Alignas(8) gg_type_18 t261;
  _Alignas(8) gg_type_18 t262;
  _Alignas(8) gg_slice t263;
  _Alignas(8) int64_t t264;
  _Alignas(8) int64_t t265;
  _Alignas(8) int64_t t266;
  _Alignas(8) int64_t t267;
  _Alignas(8) int64_t t268;
  _Alignas(8) int64_t t269;
  _Alignas(8) int64_t t270;
  _Alignas(8) gg_slice t271;
  _Alignas(8) int64_t t272;
  _Alignas(8) int64_t t273;
  _Alignas(8) int64_t t274;
  _Alignas(8) int64_t t275;
  _Alignas(8) int64_t t276;
  _Alignas(8) int64_t t277;
  _Alignas(8) int64_t t278;
  _Alignas(8) gg_slice t279;
  _Alignas(8) int64_t t280;
  _Alignas(8) int64_t t281;
  _Alignas(8) int64_t t282;
  _Alignas(8) int64_t t283;
  _Alignas(8) int64_t t284;
  _Alignas(8) int64_t t285;
  _Alignas(8) int64_t t286;
  _Alignas(8) int32_t t287;
  _Alignas(8) int32_t t288;
  int64_t t289;
  _Alignas(8) int64_t t290;
  GG_UNALIGNED gg_unaligned_17 * t291;
  _Alignas(8) uint32_t t292;
  _Alignas(8) int64_t t293;
  _Alignas(8) uint32_t t294;
  _Alignas(8) uint32_t t295;
  _Alignas(8) uint32_t t296;
  _Alignas(8) uint32_t t297;
  _Alignas(8) int64_t t298;
  _Alignas(8) uint32_t t299;
  _Alignas(8) uint32_t t300;
  _Alignas(8) uint32_t t301;
  _Alignas(8) int64_t t302;
  _Alignas(8) uint32_t t303;
  _Alignas(8) uint32_t t304;
  _Alignas(8) uint32_t t305;
  _Alignas(8) int64_t t306;
  GG_UNALIGNED gg_unaligned_17 * t307;
  _Alignas(8) uint32_t t308;
  _Alignas(8) uint32_t t309;
  _Alignas(8) uint32_t t310;
  _Alignas(8) uint32_t t311;
  _Alignas(8) int64_t t312;
  _Alignas(8) uint32_t t313;
  _Alignas(8) uint32_t t314;
  _Alignas(8) uint32_t t315;
  _Alignas(8) int64_t t316;
  _Alignas(8) uint32_t t317;
  _Alignas(8) int64_t t318;
  GG_UNALIGNED gg_unaligned_17 * t319;
  _Alignas(8) int64_t t320;
  GG_UNALIGNED gg_unaligned_17 * t321;
  _Alignas(8) uint32_t t322;
  _Alignas(8) uint32_t t323;
  _Alignas(8) gg_slice t324;
  _Alignas(8) int64_t t325;
  _Alignas(8) int64_t t326;
  _Alignas(8) int64_t t327;
  _Alignas(8) int64_t t328;
  _Alignas(8) int64_t t329;
  _Alignas(8) int64_t t330;
  _Alignas(8) int64_t t331;
  _Alignas(8) gg_slice t332;
  _Alignas(8) int64_t t333;
  _Alignas(8) int64_t t334;
  _Alignas(8) int64_t t335;
  _Alignas(8) int64_t t336;
  _Alignas(8) int64_t t337;
  _Alignas(8) int64_t t338;
  _Alignas(8) int64_t t339;
  _Alignas(8) gg_slice t340;
  _Alignas(8) int64_t t341;
  _Alignas(8) int64_t t342;
  _Alignas(8) int64_t t343;
  _Alignas(8) int64_t t344;
  _Alignas(8) int64_t t345;
  _Alignas(8) int64_t t346;
  _Alignas(8) int64_t t347;
  _Alignas(8) int32_t t348;
  _Alignas(8) int32_t t349;
  int64_t t350;
  _Alignas(8) int64_t t351;
  GG_UNALIGNED gg_unaligned_17 * t352;
  _Alignas(8) uint32_t t353;
  _Alignas(8) int64_t t354;
  _Alignas(8) uint32_t t355;
  _Alignas(8) uint32_t t356;
  _Alignas(8) uint32_t t357;
  _Alignas(8) uint32_t t358;
  _Alignas(8) int64_t t359;
  _Alignas(8) uint32_t t360;
  _Alignas(8) uint32_t t361;
  _Alignas(8) uint32_t t362;
  _Alignas(8) int64_t t363;
  _Alignas(8) uint32_t t364;
  _Alignas(8) uint32_t t365;
  _Alignas(8) uint32_t t366;
  _Alignas(8) int64_t t367;
  GG_UNALIGNED gg_unaligned_17 * t368;
  _Alignas(8) uint32_t t369;
  _Alignas(8) uint32_t t370;
  _Alignas(8) uint32_t t371;
  _Alignas(8) uint32_t t372;
  _Alignas(8) int64_t t373;
  _Alignas(8) uint32_t t374;
  _Alignas(8) uint32_t t375;
  _Alignas(8) uint32_t t376;
  _Alignas(8) int64_t t377;
  _Alignas(8) uint32_t t378;
  _Alignas(8) int64_t t379;
  GG_UNALIGNED gg_unaligned_17 * t380;
  _Alignas(8) int64_t t381;
  GG_UNALIGNED gg_unaligned_17 * t382;
  _Alignas(8) uint32_t t383;
  _Alignas(8) uint32_t t384;
  _Alignas(8) gg_type_28 t385;
  _Alignas(8) gg_type_28 t386;
  _Alignas(8) gg_type_20 t387;
  _Alignas(8) gg_type_20 t388;
  _Alignas(8) gg_type_20 t389;
  _Alignas(8) gg_type_20 t390;
  _Alignas(8) gg_type_20 t391;
  _Alignas(8) gg_type_20 t392;
  _Alignas(8) gg_type_28 t393;
  _Alignas(8) int64_t t394;
  _Alignas(8) gg_type_28 t395;
  _Alignas(8) gg_type_20 t396;
  _Alignas(8) gg_type_28 t397;
  _Alignas(8) int64_t t398;
  _Alignas(8) gg_type_28 t399;
  _Alignas(8) gg_type_20 t400;
  _Alignas(8) gg_type_20 t401;
  _Alignas(8) gg_slice t402;
  _Alignas(8) int64_t t403;
  _Alignas(8) int64_t t404;
  _Alignas(8) int64_t t405;
  _Alignas(8) int64_t t406;
  _Alignas(8) int64_t t407;
  _Alignas(8) int64_t t408;
  _Alignas(8) int64_t t409;
  _Alignas(8) gg_type_31 t410;
  _Alignas(8) gg_slice t411;
  _Alignas(8) int64_t t412;
  _Alignas(8) int64_t t413;
  _Alignas(8) int64_t t414;
  _Alignas(8) int64_t t415;
  _Alignas(8) int64_t t416;
  _Alignas(8) int64_t t417;
  _Alignas(8) int64_t t418;
  _Alignas(8) gg_type_31 t419;
  _Alignas(8) gg_slice t420;
  _Alignas(8) int64_t t421;
  _Alignas(8) int64_t t422;
  _Alignas(8) int64_t t423;
  _Alignas(8) int64_t t424;
  _Alignas(8) int64_t t425;
  _Alignas(8) int64_t t426;
  _Alignas(8) int64_t t427;
  _Alignas(8) gg_type_31 t428;
  _Alignas(8) gg_slice t429;
  _Alignas(8) int64_t t430;
  _Alignas(8) int64_t t431;
  _Alignas(8) int64_t t432;
  _Alignas(8) int64_t t433;
  _Alignas(8) int64_t t434;
  _Alignas(8) int64_t t435;
  _Alignas(8) int64_t t436;
  _Alignas(8) gg_type_31 t437;
  _Alignas(8) gg_type_31 t438;
  _Alignas(8) gg_type_18 t439;
  _Alignas(8) int64_t t440;
  _Alignas(8) gg_type_18 t441;
  _Alignas(8) gg_type_31 t442;
  _Alignas(8) gg_type_18 t443;
  _Alignas(8) int64_t t444;
  _Alignas(8) gg_type_18 t445;
  _Alignas(8) gg_type_31 t446;
  _Alignas(8) gg_type_18 t447;
  _Alignas(8) int64_t t448;
  _Alignas(8) gg_type_18 t449;
  _Alignas(8) gg_type_31 t450;
  _Alignas(8) gg_type_18 t451;
  _Alignas(8) int64_t t452;
  _Alignas(8) gg_type_18 t453;
  _Alignas(8) gg_type_31 t454;
  _Alignas(8) gg_type_18 t455;
  _Alignas(8) int64_t t456;
  _Alignas(8) gg_type_18 t457;
  _Alignas(8) gg_type_31 t458;
  _Alignas(8) gg_type_18 t459;
  _Alignas(8) int64_t t460;
  _Alignas(8) gg_type_18 t461;
  _Alignas(8) gg_type_31 t462;
  _Alignas(8) gg_type_18 t463;
  _Alignas(8) int64_t t464;
  _Alignas(8) gg_type_18 t465;
  _Alignas(8) gg_type_31 t466;
  _Alignas(8) gg_type_18 t467;
  _Alignas(8) int64_t t468;
  _Alignas(8) gg_type_18 t469;
  _Alignas(8) gg_type_31 t470;
  _Alignas(8) gg_type_18 t471;
  _Alignas(8) int64_t t472;
  _Alignas(8) gg_type_18 t473;
  _Alignas(8) gg_type_31 t474;
  _Alignas(8) gg_type_18 t475;
  _Alignas(8) int64_t t476;
  _Alignas(8) gg_type_18 t477;
  _Alignas(8) gg_type_31 t478;
  _Alignas(8) gg_type_18 t479;
  _Alignas(8) int64_t t480;
  _Alignas(8) gg_type_18 t481;
  _Alignas(8) gg_type_31 t482;
  _Alignas(8) gg_type_18 t483;
  _Alignas(8) int64_t t484;
  _Alignas(8) gg_type_18 t485;
  _Alignas(8) gg_type_31 t486;
  _Alignas(8) gg_type_18 t487;
  _Alignas(8) int64_t t488;
  _Alignas(8) gg_type_18 t489;
  _Alignas(8) gg_type_31 t490;
  _Alignas(8) gg_type_18 t491;
  _Alignas(8) int64_t t492;
  _Alignas(8) gg_type_18 t493;
  _Alignas(8) gg_type_31 t494;
  _Alignas(8) gg_type_18 t495;
  _Alignas(8) int64_t t496;
  _Alignas(8) gg_type_18 t497;
  _Alignas(8) gg_type_31 t498;
  _Alignas(8) gg_type_18 t499;
  _Alignas(8) int64_t t500;
  _Alignas(8) gg_type_18 t501;
  _Alignas(8) gg_type_18 t502;
  _Alignas(8) int64_t t503;
  _Alignas(8) gg_type_18 t504;
  _Alignas(8) int64_t t505;
  _Alignas(8) gg_type_18 t506;
  _Alignas(8) int64_t t507;
  _Alignas(8) gg_type_18 t508;
  _Alignas(8) int64_t t509;
  _Alignas(8) gg_type_18 t510;
  _Alignas(8) int64_t t511;
  _Alignas(8) gg_type_18 t512;
  _Alignas(8) int64_t t513;
  _Alignas(8) gg_type_18 t514;
  _Alignas(8) int64_t t515;
  _Alignas(8) gg_type_18 t516;
  _Alignas(8) int64_t t517;
  _Alignas(8) int16_t t518;
  _Alignas(8) int16_t t519;
  int64_t t520;
  _Alignas(8) gg_type_25 t521;
  _Alignas(8) gg_type_18 t522;
  _Alignas(8) gg_type_18 t523;
  _Alignas(8) int64_t t524;
  _Alignas(8) gg_type_18 t525;
  _Alignas(8) gg_type_31 t526;
  _Alignas(8) gg_type_25 t527;
  _Alignas(8) gg_type_25 t528;
  _Alignas(8) gg_type_18 t529;
  _Alignas(8) gg_type_18 t530;
  _Alignas(8) int64_t t531;
  _Alignas(8) gg_type_18 t532;
  _Alignas(8) gg_type_31 t533;
  _Alignas(8) gg_type_25 t534;
  _Alignas(8) gg_type_25 t535;
  _Alignas(8) gg_type_18 t536;
  _Alignas(8) gg_type_18 t537;
  _Alignas(8) int64_t t538;
  _Alignas(8) gg_type_18 t539;
  _Alignas(8) gg_type_31 t540;
  _Alignas(8) gg_type_25 t541;
  _Alignas(8) gg_type_25 t542;
  _Alignas(8) gg_type_18 t543;
  _Alignas(8) gg_type_18 t544;
  _Alignas(8) int64_t t545;
  _Alignas(8) gg_type_18 t546;
  _Alignas(8) gg_type_31 t547;
  _Alignas(8) gg_type_25 t548;
  _Alignas(8) gg_type_25 t549;
  _Alignas(8) gg_type_18 t550;
  _Alignas(8) gg_type_18 t551;
  _Alignas(8) int64_t t552;
  _Alignas(8) gg_type_18 t553;
  _Alignas(8) gg_type_31 t554;
  _Alignas(8) gg_type_25 t555;
  _Alignas(8) gg_type_25 t556;
  _Alignas(8) gg_type_18 t557;
  _Alignas(8) gg_type_18 t558;
  _Alignas(8) int64_t t559;
  _Alignas(8) gg_type_18 t560;
  _Alignas(8) gg_type_31 t561;
  _Alignas(8) gg_type_25 t562;
  _Alignas(8) gg_type_25 t563;
  _Alignas(8) gg_type_18 t564;
  _Alignas(8) gg_type_18 t565;
  _Alignas(8) int64_t t566;
  _Alignas(8) gg_type_18 t567;
  _Alignas(8) gg_type_31 t568;
  _Alignas(8) gg_type_25 t569;
  _Alignas(8) gg_type_25 t570;
  _Alignas(8) gg_type_18 t571;
  _Alignas(8) gg_type_18 t572;
  _Alignas(8) int64_t t573;
  _Alignas(8) gg_type_18 t574;
  _Alignas(8) gg_type_31 t575;
  _Alignas(8) gg_type_25 t576;
  _Alignas(8) gg_type_18 t577;
  _Alignas(8) int64_t t578;
  _Alignas(8) gg_type_18 t579;
  _Alignas(8) int64_t t580;
  _Alignas(8) gg_type_18 t581;
  _Alignas(8) int64_t t582;
  _Alignas(8) gg_type_18 t583;
  _Alignas(8) int64_t t584;
  _Alignas(8) gg_type_18 t585;
  _Alignas(8) int64_t t586;
  _Alignas(8) gg_type_18 t587;
  _Alignas(8) int64_t t588;
  _Alignas(8) gg_type_18 t589;
  _Alignas(8) int64_t t590;
  _Alignas(8) gg_type_18 t591;
  _Alignas(8) int64_t t592;
  _Alignas(8) int16_t t593;
  _Alignas(8) int16_t t594;
  int64_t t595;
  _Alignas(8) gg_type_25 t596;
  _Alignas(8) gg_type_18 t597;
  _Alignas(8) gg_type_18 t598;
  _Alignas(8) int64_t t599;
  _Alignas(8) gg_type_18 t600;
  _Alignas(8) gg_type_31 t601;
  _Alignas(8) gg_type_25 t602;
  _Alignas(8) gg_type_25 t603;
  _Alignas(8) gg_type_18 t604;
  _Alignas(8) gg_type_18 t605;
  _Alignas(8) int64_t t606;
  _Alignas(8) gg_type_18 t607;
  _Alignas(8) gg_type_31 t608;
  _Alignas(8) gg_type_25 t609;
  _Alignas(8) gg_type_25 t610;
  _Alignas(8) gg_type_18 t611;
  _Alignas(8) gg_type_18 t612;
  _Alignas(8) int64_t t613;
  _Alignas(8) gg_type_18 t614;
  _Alignas(8) gg_type_31 t615;
  _Alignas(8) gg_type_25 t616;
  _Alignas(8) gg_type_25 t617;
  _Alignas(8) gg_type_18 t618;
  _Alignas(8) gg_type_18 t619;
  _Alignas(8) int64_t t620;
  _Alignas(8) gg_type_18 t621;
  _Alignas(8) gg_type_31 t622;
  _Alignas(8) gg_type_25 t623;
  _Alignas(8) gg_type_25 t624;
  _Alignas(8) gg_type_18 t625;
  _Alignas(8) gg_type_18 t626;
  _Alignas(8) int64_t t627;
  _Alignas(8) gg_type_18 t628;
  _Alignas(8) gg_type_31 t629;
  _Alignas(8) gg_type_25 t630;
  _Alignas(8) gg_type_25 t631;
  _Alignas(8) gg_type_18 t632;
  _Alignas(8) gg_type_18 t633;
  _Alignas(8) int64_t t634;
  _Alignas(8) gg_type_18 t635;
  _Alignas(8) gg_type_31 t636;
  _Alignas(8) gg_type_25 t637;
  _Alignas(8) gg_type_25 t638;
  _Alignas(8) gg_type_18 t639;
  _Alignas(8) gg_type_18 t640;
  _Alignas(8) int64_t t641;
  _Alignas(8) gg_type_18 t642;
  _Alignas(8) gg_type_31 t643;
  _Alignas(8) gg_type_25 t644;
  _Alignas(8) gg_type_25 t645;
  _Alignas(8) gg_type_18 t646;
  _Alignas(8) gg_type_18 t647;
  _Alignas(8) int64_t t648;
  _Alignas(8) gg_type_18 t649;
  _Alignas(8) gg_type_31 t650;
  _Alignas(8) gg_type_25 t651;
  _Alignas(8) gg_type_25 t652;
  _Alignas(8) gg_type_25 t653;
  _Alignas(8) gg_type_25 t654;
  _Alignas(8) gg_type_25 t655;
  _Alignas(8) gg_type_21 t656;
  _Alignas(8) gg_type_21 t657;
  _Alignas(8) gg_type_25 t658;
  _Alignas(8) gg_type_25 t659;
  _Alignas(8) gg_type_25 t660;
  _Alignas(8) gg_type_25 t661;
  _Alignas(8) gg_type_21 t662;
  _Alignas(8) gg_type_21 t663;
  _Alignas(8) gg_type_21 t664;
  _Alignas(8) gg_type_21 t665;
  _Alignas(8) gg_type_7 t666;
  _Alignas(8) float t667;
  _Alignas(8) gg_type_7 t668;
  _Alignas(8) gg_type_7 t669;
  _Alignas(8) float t670;
  _Alignas(8) gg_type_7 t671;
  _Alignas(8) gg_type_21 t672;
  _Alignas(8) gg_type_7 t673;
  _Alignas(8) gg_type_7 t674;
  _Alignas(8) gg_type_7 t675;
  _Alignas(8) gg_type_21 t676;
  _Alignas(8) gg_type_7 t677;
  _Alignas(8) gg_type_7 t678;
  _Alignas(8) gg_type_7 t679;
  _Alignas(8) gg_type_7 t680;
  _Alignas(8) gg_type_21 t681;
  int64_t t682;
  _Alignas(8) int64_t t683;
  gg_slice t684;
  _Alignas(8) int64_t t685;
  _Alignas(8) int64_t t686;
  _Alignas(8) int64_t t687;
  _Alignas(8) int64_t t688;
  _Alignas(8) int64_t t689;
  GG_UNALIGNED gg_unaligned_3 * t690;
  _Alignas(8) int64_t t691;
  GG_UNALIGNED gg_unaligned_3 * t692;
  _Alignas(8) float t693;
  _Alignas(8) int64_t t694;
  GG_UNALIGNED gg_unaligned_3 * t695;
  _Alignas(8) float t696;
  int64_t t697;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) gg_slice v4;
  _Alignas(8) gg_type_18 v5;
  _Alignas(8) uint32_t v6;
  _Alignas(8) uint32_t v7;
  _Alignas(8) uint32_t v8;
  _Alignas(8) gg_type_20 v9;
  _Alignas(8) gg_type_20 v10;
  _Alignas(8) gg_type_21 v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) gg_type_7 v13;
  _Alignas(8) gg_type_7 v14;
  _Alignas(8) int64_t v15;
  _Alignas(8) int64_t v16;
  _Alignas(8) int64_t v17;
  _Alignas(8) gg_type_20 v18;
  _Alignas(8) gg_type_20 v19;
  _Alignas(8) int64_t v20;
  _Alignas(8) gg_type_20 v21;
  _Alignas(8) gg_type_7 v22;
  _Alignas(8) gg_type_7 v23;
  _Alignas(8) int64_t v24;
  _Alignas(8) float v25;
  _Alignas(8) gg_type_21 v26;
  _Alignas(8) gg_type_21 v27;
  _Alignas(8) gg_type_25 v28;
  _Alignas(8) int64_t v29;
  _Alignas(8) gg_type_25 v30;
  _Alignas(8) gg_type_25 v31;
  _Alignas(8) int64_t v32;
  _Alignas(8) int64_t v33;
  _Alignas(8) int64_t v34;
  _Alignas(8) gg_type_18 v35;
  _Alignas(8) gg_type_18 v36;
  _Alignas(8) gg_type_18 v37;
  _Alignas(8) gg_type_18 v38;
  _Alignas(8) gg_type_18 v39;
  _Alignas(8) gg_type_18 v40;
  _Alignas(8) gg_type_18 v41;
  _Alignas(8) gg_type_18 v42;
  _Alignas(8) gg_type_18 v43;
  _Alignas(8) gg_type_18 v44;
  _Alignas(8) gg_type_18 v45;
  _Alignas(8) gg_type_18 v46;
  _Alignas(8) gg_type_18 v47;
  _Alignas(8) gg_type_18 v48;
  _Alignas(8) gg_type_18 v49;
  _Alignas(8) gg_type_18 v50;
  _Alignas(8) uint32_t v51;
  _Alignas(8) uint32_t v52;
  _Alignas(8) uint32_t v53;
  _Alignas(8) gg_type_28 v54;
  _Alignas(8) uint32_t v55;
  _Alignas(8) uint32_t v56;
  _Alignas(8) uint32_t v57;
  _Alignas(8) uint32_t v58;
  _Alignas(8) gg_type_28 v59;
  _Alignas(8) uint32_t v60;
  _Alignas(8) gg_type_20 v61;
  _Alignas(8) gg_type_20 v62;
  _Alignas(8) gg_type_25 v63;
  _Alignas(8) gg_type_25 v64;
  _Alignas(8) gg_type_25 v65;
  _Alignas(8) gg_type_31 v66;
  _Alignas(8) gg_type_31 v67;
  _Alignas(8) gg_type_31 v68;
  _Alignas(8) gg_type_31 v69;
  _Alignas(8) gg_type_31 v70;
  _Alignas(8) gg_type_31 v71;
  _Alignas(8) gg_type_31 v72;
  _Alignas(8) gg_type_31 v73;
  _Alignas(8) gg_type_31 v74;
  _Alignas(8) gg_type_31 v75;
  _Alignas(8) gg_type_31 v76;
  _Alignas(8) gg_type_31 v77;
  _Alignas(8) gg_type_31 v78;
  _Alignas(8) gg_type_31 v79;
  _Alignas(8) gg_type_31 v80;
  _Alignas(8) gg_type_31 v81;
  _Alignas(8) gg_type_31 v82;
  _Alignas(8) gg_type_31 v83;
  _Alignas(8) gg_type_31 v84;
  _Alignas(8) gg_type_31 v85;
  _Alignas(8) gg_type_18 v86;
  _Alignas(8) gg_type_18 v87;
  _Alignas(8) gg_type_18 v88;
  _Alignas(8) gg_type_18 v89;
  _Alignas(8) gg_type_18 v90;
  _Alignas(8) gg_type_18 v91;
  _Alignas(8) gg_type_18 v92;
  _Alignas(8) gg_type_18 v93;
  _Alignas(8) gg_type_25 v94;
  _Alignas(8) gg_type_18 v95;
  _Alignas(8) gg_type_18 v96;
  _Alignas(8) gg_type_18 v97;
  _Alignas(8) gg_type_18 v98;
  _Alignas(8) gg_type_18 v99;
  _Alignas(8) gg_type_18 v100;
  _Alignas(8) gg_type_18 v101;
  _Alignas(8) gg_type_18 v102;
  _Alignas(8) gg_type_25 v103;
  _Alignas(8) gg_type_21 v104;
  _Alignas(8) gg_type_21 v105;
  _Alignas(8) gg_type_21 v106;
  _Alignas(8) gg_type_25 v107;
  _Alignas(8) gg_type_21 v108;
  _Alignas(8) gg_type_7 v109;
  _Alignas(8) gg_type_7 v110;
  _Alignas(8) gg_type_7 v111;
  _Alignas(8) int64_t v112;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) > (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(8))));
  t11 = ((int64_t)(((int64_t)gg_mod(t9, t10))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t11) == (t12));
  }
  if (!(t8)) gg_fail("assertion failed");
  t13 = ((int64_t)((v0).len));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)UINT64_C(144))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  if (!(((t13) == (t18)))) gg_fail("assertion failed");
  t19 = ((int64_t)((v1).len));
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(((int64_t)UINT64_C(292))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  if (!(((t19) == (t22)))) gg_fail("assertion failed");
  t23 = ((int64_t)((v4).len));
  t24 = ((int64_t)(v3));
  if (!(((t23) == (t24)))) gg_fail("assertion failed");
  t25 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v5 = gg_function_2(t25, base + 2);
  v6 = ((uint32_t)(((int64_t)UINT64_C(1061109567))));
  v7 = ((uint32_t)(((int64_t)UINT64_C(252645135))));
  v8 = ((uint32_t)(((int64_t)UINT64_C(50529027))));
  (v9).data[0] = ((uint8_t)(((int64_t)UINT64_C(0))));
  (v9).data[1] = ((uint8_t)(((int64_t)UINT64_C(1))));
  (v9).data[2] = ((uint8_t)(((int64_t)UINT64_C(8))));
  (v9).data[3] = ((uint8_t)(((int64_t)UINT64_C(9))));
  (v9).data[4] = ((uint8_t)(((int64_t)UINT64_C(2))));
  (v9).data[5] = ((uint8_t)(((int64_t)UINT64_C(3))));
  (v9).data[6] = ((uint8_t)(((int64_t)UINT64_C(10))));
  (v9).data[7] = ((uint8_t)(((int64_t)UINT64_C(11))));
  (v9).data[8] = ((uint8_t)(((int64_t)UINT64_C(4))));
  (v9).data[9] = ((uint8_t)(((int64_t)UINT64_C(5))));
  (v9).data[10] = ((uint8_t)(((int64_t)UINT64_C(12))));
  (v9).data[11] = ((uint8_t)(((int64_t)UINT64_C(13))));
  (v9).data[12] = ((uint8_t)(((int64_t)UINT64_C(6))));
  (v9).data[13] = ((uint8_t)(((int64_t)UINT64_C(7))));
  (v9).data[14] = ((uint8_t)(((int64_t)UINT64_C(14))));
  (v9).data[15] = ((uint8_t)(((int64_t)UINT64_C(15))));
  (v10).data[0] = ((uint8_t)(((int64_t)UINT64_C(0))));
  (v10).data[1] = ((uint8_t)(((int64_t)UINT64_C(0))));
  (v10).data[2] = ((uint8_t)(((int64_t)UINT64_C(4))));
  (v10).data[3] = ((uint8_t)(((int64_t)UINT64_C(4))));
  (v10).data[4] = ((uint8_t)(((int64_t)UINT64_C(1))));
  (v10).data[5] = ((uint8_t)(((int64_t)UINT64_C(1))));
  (v10).data[6] = ((uint8_t)(((int64_t)UINT64_C(5))));
  (v10).data[7] = ((uint8_t)(((int64_t)UINT64_C(5))));
  (v10).data[8] = ((uint8_t)(((int64_t)UINT64_C(2))));
  (v10).data[9] = ((uint8_t)(((int64_t)UINT64_C(2))));
  (v10).data[10] = ((uint8_t)(((int64_t)UINT64_C(6))));
  (v10).data[11] = ((uint8_t)(((int64_t)UINT64_C(6))));
  (v10).data[12] = ((uint8_t)(((int64_t)UINT64_C(3))));
  (v10).data[13] = ((uint8_t)(((int64_t)UINT64_C(3))));
  (v10).data[14] = ((uint8_t)(((int64_t)UINT64_C(7))));
  (v10).data[15] = ((uint8_t)(((int64_t)UINT64_C(7))));
  (v11).data[0] = ((int32_t)(((int64_t)UINT64_C(0))));
  (v11).data[1] = ((int32_t)(((int64_t)UINT64_C(2))));
  (v11).data[2] = ((int32_t)(((int64_t)UINT64_C(4))));
  (v11).data[3] = ((int32_t)(((int64_t)UINT64_C(6))));
  (v11).data[4] = ((int32_t)(((int64_t)UINT64_C(1))));
  (v11).data[5] = ((int32_t)(((int64_t)UINT64_C(3))));
  (v11).data[6] = ((int32_t)(((int64_t)UINT64_C(5))));
  (v11).data[7] = ((int32_t)(((int64_t)UINT64_C(7))));
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(8))));
  t29 = ((int64_t)gg_div(t27, t28));
  t26 = 0;
label0:;
  if (t26 >= t29) goto label2;
  v12 = t26;
  {
  memset(&t30, 0, sizeof(t30));
  t31 = ((float)(t30));
  for (t32=0; t32<8; t32++) (v13).data[t32] = t31;
  memset(&t33, 0, sizeof(t33));
  t34 = ((float)(t33));
  for (t35=0; t35<8; t35++) (v14).data[t35] = t34;
  t37 = v2;
  t36 = 0;
label3:;
  if (t36 >= t37) goto label5;
  v15 = t36;
  {
  t38 = ((int64_t)(v12));
  t39 = ((int64_t)(v2));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  t41 = ((int64_t)(v15));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))));
  t43 = ((int64_t)(((int64_t)UINT64_C(1152))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t44 = ((int64_t)(v15));
  t45 = ((int64_t)(((int64_t)UINT64_C(292))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  memset(&t46, 0, sizeof(t46));
  t47 = ((uint8_t)(t46));
  for (t48=0; t48<16; t48++) (v18).data[t48] = t47;
  memset(&t49, 0, sizeof(t49));
  t50 = ((uint8_t)(t49));
  for (t51=0; t51<16; t51++) (v19).data[t51] = t50;
  t53 = ((int64_t)UINT64_C(16));
  t52 = 0;
label6:;
  if (t52 >= t53) goto label8;
  v20 = t52;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t54 = v20;
  t55 = ((GG_UNALIGNED gg_unaligned_2 *)(v18).data) + gg_index(t54, ((int64_t)UINT64_C(16)));
  t56 = v0;
  t57 = ((int64_t)(v16));
  t58 = ((int64_t)(v20));
  t59 = ((int64_t)((uint64_t)(t57) + (uint64_t)(t58)));
  t60 = ((const GG_UNALIGNED gg_unaligned_2 *)t56.data) + gg_index(t59, t56.len);
  (*t55) = ((uint8_t)(((uint8_t)((*t60)))));
  /* FG_DIRECT_SLICE_INDEX */
  t61 = v20;
  t62 = ((GG_UNALIGNED gg_unaligned_2 *)(v19).data) + gg_index(t61, ((int64_t)UINT64_C(16)));
  t63 = v0;
  t64 = ((int64_t)(v16));
  t65 = ((int64_t)(((int64_t)UINT64_C(16))));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
  t67 = ((int64_t)(v20));
  t68 = ((int64_t)((uint64_t)(t66) + (uint64_t)(t67)));
  t69 = ((const GG_UNALIGNED gg_unaligned_2 *)t63.data) + gg_index(t68, t63.len);
  (*t62) = ((uint8_t)(((uint8_t)((*t69)))));
  }
label7:;
  t52++; goto label6;
label8:;
  t70 = v18;
  t71 = v9;
  v21 = gg_function_3(t70, t71, base + 2);
  memset(&t72, 0, sizeof(t72));
  t73 = ((float)(t72));
  for (t74=0; t74<8; t74++) (v22).data[t74] = t73;
  memset(&t75, 0, sizeof(t75));
  t76 = ((float)(t75));
  for (t77=0; t77<8; t77++) (v23).data[t77] = t76;
  t79 = ((int64_t)UINT64_C(8));
  t78 = 0;
label9:;
  if (t78 >= t79) goto label11;
  v24 = t78;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t80 = v24;
  t81 = ((GG_UNALIGNED gg_unaligned_3 *)(v22).data) + gg_index(t80, ((int64_t)UINT64_C(8)));
  t82 = v21;
  t83 = ((int64_t)(v24));
  t84 = ((int64_t)(((int64_t)UINT64_C(2))));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t83) * (uint64_t)(t84)))));
  (*t81) = gg_function_4(t82, t85, base + 2);
  /* FG_DIRECT_SLICE_INDEX */
  t86 = v24;
  t87 = ((GG_UNALIGNED gg_unaligned_3 *)(v23).data) + gg_index(t86, ((int64_t)UINT64_C(8)));
  t88 = v19;
  t89 = ((int64_t)(v24));
  t90 = ((int64_t)(((int64_t)UINT64_C(2))));
  t91 = ((int64_t)(((int64_t)((uint64_t)(t89) * (uint64_t)(t90)))));
  (*t87) = gg_function_4(t88, t91, base + 2);
  }
label10:;
  t78++; goto label9;
label11:;
  t92 = v1;
  t93 = ((int64_t)(v17));
  v25 = gg_function_6(t92, t93, base + 2);
  memset(&t94, 0, sizeof(t94));
  t95 = ((int32_t)(t94));
  for (t96=0; t96<8; t96++) (v26).data[t96] = t95;
  memset(&t97, 0, sizeof(t97));
  t98 = ((int32_t)(t97));
  for (t99=0; t99<8; t99++) (v27).data[t99] = t98;
  memset(&t100, 0, sizeof(t100));
  t101 = ((int16_t)(t100));
  for (t102=0; t102<16; t102++) (v28).data[t102] = t101;
  t104 = ((int64_t)UINT64_C(16));
  t103 = 0;
label12:;
  if (t103 >= t104) goto label14;
  v29 = t103;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t105 = v29;
  t106 = ((GG_UNALIGNED gg_unaligned_16 *)(v28).data) + gg_index(t105, ((int64_t)UINT64_C(16)));
  t107 = v1;
  t108 = ((int64_t)(v17));
  t109 = ((int64_t)(((int64_t)UINT64_C(260))));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) + (uint64_t)(t109)))));
  t111 = ((int64_t)(v29));
  t112 = ((int64_t)(((int64_t)UINT64_C(2))));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t111) * (uint64_t)(t112)))));
  t114 = ((int64_t)((uint64_t)(t110) + (uint64_t)(t113)));
  t115 = ((const GG_UNALIGNED gg_unaligned_2 *)t107.data) + gg_index(t114, t107.len);
  t116 = ((uint16_t)(((uint16_t)((*t115)))));
  t117 = v1;
  t118 = ((int64_t)(v17));
  t119 = ((int64_t)(((int64_t)UINT64_C(261))));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t118) + (uint64_t)(t119)))));
  t121 = ((int64_t)(v29));
  t122 = ((int64_t)(((int64_t)UINT64_C(2))));
  t123 = ((int64_t)(((int64_t)((uint64_t)(t121) * (uint64_t)(t122)))));
  t124 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t123)));
  t125 = ((const GG_UNALIGNED gg_unaligned_2 *)t117.data) + gg_index(t124, t117.len);
  t126 = ((uint16_t)(((uint16_t)((*t125)))));
  t127 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(8))))));
  t128 = ((uint16_t)(((uint16_t)((uint64_t)(t126) << ((uint64_t)(t127) & 15)))));
  (*t106) = ((int16_t)(((int16_t)(((t116) | (t128))))));
  }
label13:;
  t103++; goto label12;
label14:;
  t129 = v28;
  t130 = v28;
  v30 = gg_function_8(t129, t130, base + 2);
  memset(&t131, 0, sizeof(t131));
  t132 = ((int16_t)(t131));
  for (t133=0; t133<16; t133++) (v31).data[t133] = t132;
  t135 = ((int64_t)UINT64_C(16));
  t134 = 0;
label15:;
  if (t134 >= t135) goto label17;
  v32 = t134;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t136 = v32;
  t137 = ((GG_UNALIGNED gg_unaligned_16 *)(v31).data) + gg_index(t136, ((int64_t)UINT64_C(16)));
  t138 = gg_view((v30).data, 16);
  t139 = ((int64_t)(v32));
  t140 = ((int64_t)(((int64_t)UINT64_C(8))));
  t141 = ((int64_t)gg_mod(t139, t140));
  t142 = ((GG_UNALIGNED gg_unaligned_16 *)t138.data) + gg_index(t141, t138.len);
  (*t137) = ((int16_t)((*t142)));
  }
label16:;
  t134++; goto label15;
label17:;
  t144 = ((int64_t)UINT64_C(4));
  t143 = 0;
label18:;
  if (t143 >= t144) goto label20;
  v33 = t143;
  {
  t145 = ((int64_t)(v16));
  t146 = ((int64_t)(((int64_t)UINT64_C(128))));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t145) + (uint64_t)(t146)))));
  t148 = ((int64_t)(v33));
  t149 = ((int64_t)(((int64_t)UINT64_C(256))));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) * (uint64_t)(t149)))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t147) + (uint64_t)(t150)))));
  t151 = v0;
  t152 = ((int64_t)(v34));
  t153 = ((int64_t)(((int64_t)UINT64_C(0))));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t152) + (uint64_t)(t153)))));
  t155 = gg_function_9(t151, t154, base + 2);
  t156 = v5;
  v35 = gg_function_10(t155, t156, base + 2);
  t157 = v0;
  t158 = ((int64_t)(v34));
  t159 = ((int64_t)(((int64_t)UINT64_C(32))));
  t160 = ((int64_t)(((int64_t)((uint64_t)(t158) + (uint64_t)(t159)))));
  t161 = gg_function_9(t157, t160, base + 2);
  t162 = v5;
  v36 = gg_function_10(t161, t162, base + 2);
  t163 = v0;
  t164 = ((int64_t)(v34));
  t165 = ((int64_t)(((int64_t)UINT64_C(64))));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t165)))));
  t167 = gg_function_9(t163, t166, base + 2);
  t168 = v5;
  v37 = gg_function_10(t167, t168, base + 2);
  t169 = v0;
  t170 = ((int64_t)(v34));
  t171 = ((int64_t)(((int64_t)UINT64_C(96))));
  t172 = ((int64_t)(((int64_t)((uint64_t)(t170) + (uint64_t)(t171)))));
  t173 = gg_function_9(t169, t172, base + 2);
  t174 = v5;
  v38 = gg_function_10(t173, t174, base + 2);
  t175 = v0;
  t176 = ((int64_t)(v34));
  t177 = ((int64_t)(((int64_t)UINT64_C(128))));
  t178 = ((int64_t)(((int64_t)((uint64_t)(t176) + (uint64_t)(t177)))));
  t179 = gg_function_9(t175, t178, base + 2);
  t180 = v5;
  v39 = gg_function_10(t179, t180, base + 2);
  t181 = v0;
  t182 = ((int64_t)(v34));
  t183 = ((int64_t)(((int64_t)UINT64_C(160))));
  t184 = ((int64_t)(((int64_t)((uint64_t)(t182) + (uint64_t)(t183)))));
  t185 = gg_function_9(t181, t184, base + 2);
  t186 = v5;
  v40 = gg_function_10(t185, t186, base + 2);
  t187 = v0;
  t188 = ((int64_t)(v34));
  t189 = ((int64_t)(((int64_t)UINT64_C(192))));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t188) + (uint64_t)(t189)))));
  t191 = gg_function_9(t187, t190, base + 2);
  t192 = v5;
  v41 = gg_function_10(t191, t192, base + 2);
  t193 = v0;
  t194 = ((int64_t)(v34));
  t195 = ((int64_t)(((int64_t)UINT64_C(224))));
  t196 = ((int64_t)(((int64_t)((uint64_t)(t194) + (uint64_t)(t195)))));
  t197 = gg_function_9(t193, t196, base + 2);
  t198 = v5;
  v42 = gg_function_10(t197, t198, base + 2);
  t199 = v0;
  t200 = ((int64_t)(v34));
  t201 = ((int64_t)(((int64_t)UINT64_C(0))));
  t202 = ((int64_t)(((int64_t)((uint64_t)(t200) + (uint64_t)(t201)))));
  t203 = gg_function_9(t199, t202, base + 2);
  t204 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t205 = gg_function_11(t203, t204, base + 2);
  t206 = v5;
  v43 = gg_function_10(t205, t206, base + 2);
  t207 = v0;
  t208 = ((int64_t)(v34));
  t209 = ((int64_t)(((int64_t)UINT64_C(32))));
  t210 = ((int64_t)(((int64_t)((uint64_t)(t208) + (uint64_t)(t209)))));
  t211 = gg_function_9(t207, t210, base + 2);
  t212 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t213 = gg_function_11(t211, t212, base + 2);
  t214 = v5;
  v44 = gg_function_10(t213, t214, base + 2);
  t215 = v0;
  t216 = ((int64_t)(v34));
  t217 = ((int64_t)(((int64_t)UINT64_C(64))));
  t218 = ((int64_t)(((int64_t)((uint64_t)(t216) + (uint64_t)(t217)))));
  t219 = gg_function_9(t215, t218, base + 2);
  t220 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t221 = gg_function_11(t219, t220, base + 2);
  t222 = v5;
  v45 = gg_function_10(t221, t222, base + 2);
  t223 = v0;
  t224 = ((int64_t)(v34));
  t225 = ((int64_t)(((int64_t)UINT64_C(96))));
  t226 = ((int64_t)(((int64_t)((uint64_t)(t224) + (uint64_t)(t225)))));
  t227 = gg_function_9(t223, t226, base + 2);
  t228 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t229 = gg_function_11(t227, t228, base + 2);
  t230 = v5;
  v46 = gg_function_10(t229, t230, base + 2);
  t231 = v0;
  t232 = ((int64_t)(v34));
  t233 = ((int64_t)(((int64_t)UINT64_C(128))));
  t234 = ((int64_t)(((int64_t)((uint64_t)(t232) + (uint64_t)(t233)))));
  t235 = gg_function_9(t231, t234, base + 2);
  t236 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t237 = gg_function_11(t235, t236, base + 2);
  t238 = v5;
  v47 = gg_function_10(t237, t238, base + 2);
  t239 = v0;
  t240 = ((int64_t)(v34));
  t241 = ((int64_t)(((int64_t)UINT64_C(160))));
  t242 = ((int64_t)(((int64_t)((uint64_t)(t240) + (uint64_t)(t241)))));
  t243 = gg_function_9(t239, t242, base + 2);
  t244 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t245 = gg_function_11(t243, t244, base + 2);
  t246 = v5;
  v48 = gg_function_10(t245, t246, base + 2);
  t247 = v0;
  t248 = ((int64_t)(v34));
  t249 = ((int64_t)(((int64_t)UINT64_C(192))));
  t250 = ((int64_t)(((int64_t)((uint64_t)(t248) + (uint64_t)(t249)))));
  t251 = gg_function_9(t247, t250, base + 2);
  t252 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t253 = gg_function_11(t251, t252, base + 2);
  t254 = v5;
  v49 = gg_function_10(t253, t254, base + 2);
  t255 = v0;
  t256 = ((int64_t)(v34));
  t257 = ((int64_t)(((int64_t)UINT64_C(224))));
  t258 = ((int64_t)(((int64_t)((uint64_t)(t256) + (uint64_t)(t257)))));
  t259 = gg_function_9(t255, t258, base + 2);
  t260 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t261 = gg_function_11(t259, t260, base + 2);
  t262 = v5;
  v50 = gg_function_10(t261, t262, base + 2);
  t263 = v0;
  t264 = ((int64_t)(v16));
  t265 = ((int64_t)(((int64_t)UINT64_C(32))));
  t266 = ((int64_t)(((int64_t)((uint64_t)(t264) + (uint64_t)(t265)))));
  t267 = ((int64_t)(((int64_t)UINT64_C(24))));
  t268 = ((int64_t)(v33));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t267) * (uint64_t)(t268)))));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t266) + (uint64_t)(t269)))));
  v51 = gg_function_12(t263, t270, base + 2);
  t271 = v0;
  t272 = ((int64_t)(v16));
  t273 = ((int64_t)(((int64_t)UINT64_C(36))));
  t274 = ((int64_t)(((int64_t)((uint64_t)(t272) + (uint64_t)(t273)))));
  t275 = ((int64_t)(((int64_t)UINT64_C(24))));
  t276 = ((int64_t)(v33));
  t277 = ((int64_t)(((int64_t)((uint64_t)(t275) * (uint64_t)(t276)))));
  t278 = ((int64_t)(((int64_t)((uint64_t)(t274) + (uint64_t)(t277)))));
  v52 = gg_function_12(t271, t278, base + 2);
  t279 = v0;
  t280 = ((int64_t)(v16));
  t281 = ((int64_t)(((int64_t)UINT64_C(40))));
  t282 = ((int64_t)(((int64_t)((uint64_t)(t280) + (uint64_t)(t281)))));
  t283 = ((int64_t)(((int64_t)UINT64_C(24))));
  t284 = ((int64_t)(v33));
  t285 = ((int64_t)(((int64_t)((uint64_t)(t283) * (uint64_t)(t284)))));
  t286 = ((int64_t)(((int64_t)((uint64_t)(t282) + (uint64_t)(t285)))));
  v53 = gg_function_12(t279, t286, base + 2);
  memset(&t287, 0, sizeof(t287));
  t288 = ((int32_t)(t287));
  for (t289=0; t289<4; t289++) (v54).data[t289] = t288;
  /* FG_DIRECT_SLICE_INDEX */
  t290 = ((int64_t)UINT64_C(3));
  t291 = ((GG_UNALIGNED gg_unaligned_17 *)(v54).data) + gg_index(t290, ((int64_t)UINT64_C(4)));
  t292 = ((uint32_t)(v53));
  t293 = ((int64_t)(((int64_t)UINT64_C(4))));
  t294 = ((uint32_t)(((t292) >> ((uint64_t)(t293) & 31))));
  t295 = ((uint32_t)(v7));
  t296 = ((uint32_t)(((t294) & (t295))));
  t297 = ((uint32_t)(v52));
  t298 = ((int64_t)(((int64_t)UINT64_C(6))));
  t299 = ((uint32_t)(((t297) >> ((uint64_t)(t298) & 31))));
  t300 = ((uint32_t)(v8));
  t301 = ((uint32_t)(((t299) & (t300))));
  t302 = ((int64_t)(((int64_t)UINT64_C(4))));
  t303 = ((uint32_t)(((uint32_t)((uint64_t)(t301) << ((uint64_t)(t302) & 31)))));
  (*t291) = ((int32_t)(((int32_t)(((t296) | (t303))))));
  t304 = ((uint32_t)(v52));
  t305 = ((uint32_t)(v6));
  v55 = ((uint32_t)(((t304) & (t305))));
  /* FG_DIRECT_SLICE_INDEX */
  t306 = ((int64_t)UINT64_C(1));
  t307 = ((GG_UNALIGNED gg_unaligned_17 *)(v54).data) + gg_index(t306, ((int64_t)UINT64_C(4)));
  t308 = ((uint32_t)(v53));
  t309 = ((uint32_t)(v7));
  t310 = ((uint32_t)(((t308) & (t309))));
  t311 = ((uint32_t)(v51));
  t312 = ((int64_t)(((int64_t)UINT64_C(6))));
  t313 = ((uint32_t)(((t311) >> ((uint64_t)(t312) & 31))));
  t314 = ((uint32_t)(v8));
  t315 = ((uint32_t)(((t313) & (t314))));
  t316 = ((int64_t)(((int64_t)UINT64_C(4))));
  t317 = ((uint32_t)(((uint32_t)((uint64_t)(t315) << ((uint64_t)(t316) & 31)))));
  (*t307) = ((int32_t)(((int32_t)(((t310) | (t317))))));
  /* FG_DIRECT_SLICE_INDEX */
  t318 = ((int64_t)UINT64_C(2));
  t319 = ((GG_UNALIGNED gg_unaligned_17 *)(v54).data) + gg_index(t318, ((int64_t)UINT64_C(4)));
  (*t319) = ((int32_t)(((int32_t)(v55))));
  /* FG_DIRECT_SLICE_INDEX */
  t320 = ((int64_t)UINT64_C(0));
  t321 = ((GG_UNALIGNED gg_unaligned_17 *)(v54).data) + gg_index(t320, ((int64_t)UINT64_C(4)));
  t322 = ((uint32_t)(v51));
  t323 = ((uint32_t)(v6));
  (*t321) = ((int32_t)(((int32_t)(((t322) & (t323))))));
  t324 = v0;
  t325 = ((int64_t)(v16));
  t326 = ((int64_t)(((int64_t)UINT64_C(44))));
  t327 = ((int64_t)(((int64_t)((uint64_t)(t325) + (uint64_t)(t326)))));
  t328 = ((int64_t)(((int64_t)UINT64_C(24))));
  t329 = ((int64_t)(v33));
  t330 = ((int64_t)(((int64_t)((uint64_t)(t328) * (uint64_t)(t329)))));
  t331 = ((int64_t)(((int64_t)((uint64_t)(t327) + (uint64_t)(t330)))));
  v56 = gg_function_12(t324, t331, base + 2);
  t332 = v0;
  t333 = ((int64_t)(v16));
  t334 = ((int64_t)(((int64_t)UINT64_C(48))));
  t335 = ((int64_t)(((int64_t)((uint64_t)(t333) + (uint64_t)(t334)))));
  t336 = ((int64_t)(((int64_t)UINT64_C(24))));
  t337 = ((int64_t)(v33));
  t338 = ((int64_t)(((int64_t)((uint64_t)(t336) * (uint64_t)(t337)))));
  t339 = ((int64_t)(((int64_t)((uint64_t)(t335) + (uint64_t)(t338)))));
  v57 = gg_function_12(t332, t339, base + 2);
  t340 = v0;
  t341 = ((int64_t)(v16));
  t342 = ((int64_t)(((int64_t)UINT64_C(52))));
  t343 = ((int64_t)(((int64_t)((uint64_t)(t341) + (uint64_t)(t342)))));
  t344 = ((int64_t)(((int64_t)UINT64_C(24))));
  t345 = ((int64_t)(v33));
  t346 = ((int64_t)(((int64_t)((uint64_t)(t344) * (uint64_t)(t345)))));
  t347 = ((int64_t)(((int64_t)((uint64_t)(t343) + (uint64_t)(t346)))));
  v58 = gg_function_12(t340, t347, base + 2);
  memset(&t348, 0, sizeof(t348));
  t349 = ((int32_t)(t348));
  for (t350=0; t350<4; t350++) (v59).data[t350] = t349;
  /* FG_DIRECT_SLICE_INDEX */
  t351 = ((int64_t)UINT64_C(3));
  t352 = ((GG_UNALIGNED gg_unaligned_17 *)(v59).data) + gg_index(t351, ((int64_t)UINT64_C(4)));
  t353 = ((uint32_t)(v58));
  t354 = ((int64_t)(((int64_t)UINT64_C(4))));
  t355 = ((uint32_t)(((t353) >> ((uint64_t)(t354) & 31))));
  t356 = ((uint32_t)(v7));
  t357 = ((uint32_t)(((t355) & (t356))));
  t358 = ((uint32_t)(v57));
  t359 = ((int64_t)(((int64_t)UINT64_C(6))));
  t360 = ((uint32_t)(((t358) >> ((uint64_t)(t359) & 31))));
  t361 = ((uint32_t)(v8));
  t362 = ((uint32_t)(((t360) & (t361))));
  t363 = ((int64_t)(((int64_t)UINT64_C(4))));
  t364 = ((uint32_t)(((uint32_t)((uint64_t)(t362) << ((uint64_t)(t363) & 31)))));
  (*t352) = ((int32_t)(((int32_t)(((t357) | (t364))))));
  t365 = ((uint32_t)(v57));
  t366 = ((uint32_t)(v6));
  v60 = ((uint32_t)(((t365) & (t366))));
  /* FG_DIRECT_SLICE_INDEX */
  t367 = ((int64_t)UINT64_C(1));
  t368 = ((GG_UNALIGNED gg_unaligned_17 *)(v59).data) + gg_index(t367, ((int64_t)UINT64_C(4)));
  t369 = ((uint32_t)(v58));
  t370 = ((uint32_t)(v7));
  t371 = ((uint32_t)(((t369) & (t370))));
  t372 = ((uint32_t)(v56));
  t373 = ((int64_t)(((int64_t)UINT64_C(6))));
  t374 = ((uint32_t)(((t372) >> ((uint64_t)(t373) & 31))));
  t375 = ((uint32_t)(v8));
  t376 = ((uint32_t)(((t374) & (t375))));
  t377 = ((int64_t)(((int64_t)UINT64_C(4))));
  t378 = ((uint32_t)(((uint32_t)((uint64_t)(t376) << ((uint64_t)(t377) & 31)))));
  (*t368) = ((int32_t)(((int32_t)(((t371) | (t378))))));
  /* FG_DIRECT_SLICE_INDEX */
  t379 = ((int64_t)UINT64_C(2));
  t380 = ((GG_UNALIGNED gg_unaligned_17 *)(v59).data) + gg_index(t379, ((int64_t)UINT64_C(4)));
  (*t380) = ((int32_t)(((int32_t)(v60))));
  /* FG_DIRECT_SLICE_INDEX */
  t381 = ((int64_t)UINT64_C(0));
  t382 = ((GG_UNALIGNED gg_unaligned_17 *)(v59).data) + gg_index(t381, ((int64_t)UINT64_C(4)));
  t383 = ((uint32_t)(v56));
  t384 = ((uint32_t)(v6));
  (*t382) = ((int32_t)(((int32_t)(((t383) & (t384))))));
  t385 = v54;
  v61 = gg_function_13(t385, base + 2);
  t386 = v59;
  v62 = gg_function_13(t386, base + 2);
  t387 = v61;
  t388 = v10;
  t389 = gg_function_3(t387, t388, base + 2);
  v63 = gg_function_14(t389, base + 2);
  t390 = v62;
  t391 = v10;
  t392 = gg_function_3(t390, t391, base + 2);
  v64 = gg_function_14(t392, base + 2);
  t393 = v54;
  t394 = ((int64_t)(((int64_t)UINT64_C(78))));
  t395 = gg_function_15(t393, t394, base + 2);
  t396 = gg_function_13(t395, base + 2);
  t397 = v59;
  t398 = ((int64_t)(((int64_t)UINT64_C(78))));
  t399 = gg_function_15(t397, t398, base + 2);
  t400 = gg_function_13(t399, base + 2);
  t401 = gg_function_16(t396, t400, base + 2);
  v65 = gg_function_14(t401, base + 2);
  t402 = v1;
  t403 = ((int64_t)(v17));
  t404 = ((int64_t)(((int64_t)UINT64_C(4))));
  t405 = ((int64_t)(((int64_t)((uint64_t)(t403) + (uint64_t)(t404)))));
  t406 = ((int64_t)(v33));
  t407 = ((int64_t)(((int64_t)UINT64_C(64))));
  t408 = ((int64_t)(((int64_t)((uint64_t)(t406) * (uint64_t)(t407)))));
  t409 = ((int64_t)(((int64_t)((uint64_t)(t405) + (uint64_t)(t408)))));
  t410 = gg_function_17(t402, t409, base + 2);
  v66 = gg_function_18(t410, base + 2);
  t411 = v1;
  t412 = ((int64_t)(v17));
  t413 = ((int64_t)(((int64_t)UINT64_C(20))));
  t414 = ((int64_t)(((int64_t)((uint64_t)(t412) + (uint64_t)(t413)))));
  t415 = ((int64_t)(v33));
  t416 = ((int64_t)(((int64_t)UINT64_C(64))));
  t417 = ((int64_t)(((int64_t)((uint64_t)(t415) * (uint64_t)(t416)))));
  t418 = ((int64_t)(((int64_t)((uint64_t)(t414) + (uint64_t)(t417)))));
  t419 = gg_function_17(t411, t418, base + 2);
  v67 = gg_function_18(t419, base + 2);
  t420 = v1;
  t421 = ((int64_t)(v17));
  t422 = ((int64_t)(((int64_t)UINT64_C(36))));
  t423 = ((int64_t)(((int64_t)((uint64_t)(t421) + (uint64_t)(t422)))));
  t424 = ((int64_t)(v33));
  t425 = ((int64_t)(((int64_t)UINT64_C(64))));
  t426 = ((int64_t)(((int64_t)((uint64_t)(t424) * (uint64_t)(t425)))));
  t427 = ((int64_t)(((int64_t)((uint64_t)(t423) + (uint64_t)(t426)))));
  t428 = gg_function_17(t420, t427, base + 2);
  v68 = gg_function_18(t428, base + 2);
  t429 = v1;
  t430 = ((int64_t)(v17));
  t431 = ((int64_t)(((int64_t)UINT64_C(52))));
  t432 = ((int64_t)(((int64_t)((uint64_t)(t430) + (uint64_t)(t431)))));
  t433 = ((int64_t)(v33));
  t434 = ((int64_t)(((int64_t)UINT64_C(64))));
  t435 = ((int64_t)(((int64_t)((uint64_t)(t433) * (uint64_t)(t434)))));
  t436 = ((int64_t)(((int64_t)((uint64_t)(t432) + (uint64_t)(t435)))));
  t437 = gg_function_17(t429, t436, base + 2);
  v69 = gg_function_18(t437, base + 2);
  t438 = v66;
  t439 = gg_function_19(t438, base + 2);
  t440 = ((int64_t)(((int64_t)UINT64_C(0))));
  t441 = gg_function_20(t439, t440, base + 2);
  v70 = gg_function_21(t441, base + 2);
  t442 = v66;
  t443 = gg_function_19(t442, base + 2);
  t444 = ((int64_t)(((int64_t)UINT64_C(85))));
  t445 = gg_function_20(t443, t444, base + 2);
  v71 = gg_function_21(t445, base + 2);
  t446 = v66;
  t447 = gg_function_19(t446, base + 2);
  t448 = ((int64_t)(((int64_t)UINT64_C(170))));
  t449 = gg_function_20(t447, t448, base + 2);
  v72 = gg_function_21(t449, base + 2);
  t450 = v66;
  t451 = gg_function_19(t450, base + 2);
  t452 = ((int64_t)(((int64_t)UINT64_C(255))));
  t453 = gg_function_20(t451, t452, base + 2);
  v73 = gg_function_21(t453, base + 2);
  t454 = v67;
  t455 = gg_function_19(t454, base + 2);
  t456 = ((int64_t)(((int64_t)UINT64_C(0))));
  t457 = gg_function_20(t455, t456, base + 2);
  v74 = gg_function_21(t457, base + 2);
  t458 = v67;
  t459 = gg_function_19(t458, base + 2);
  t460 = ((int64_t)(((int64_t)UINT64_C(85))));
  t461 = gg_function_20(t459, t460, base + 2);
  v75 = gg_function_21(t461, base + 2);
  t462 = v67;
  t463 = gg_function_19(t462, base + 2);
  t464 = ((int64_t)(((int64_t)UINT64_C(170))));
  t465 = gg_function_20(t463, t464, base + 2);
  v76 = gg_function_21(t465, base + 2);
  t466 = v67;
  t467 = gg_function_19(t466, base + 2);
  t468 = ((int64_t)(((int64_t)UINT64_C(255))));
  t469 = gg_function_20(t467, t468, base + 2);
  v77 = gg_function_21(t469, base + 2);
  t470 = v68;
  t471 = gg_function_19(t470, base + 2);
  t472 = ((int64_t)(((int64_t)UINT64_C(0))));
  t473 = gg_function_20(t471, t472, base + 2);
  v78 = gg_function_21(t473, base + 2);
  t474 = v68;
  t475 = gg_function_19(t474, base + 2);
  t476 = ((int64_t)(((int64_t)UINT64_C(85))));
  t477 = gg_function_20(t475, t476, base + 2);
  v79 = gg_function_21(t477, base + 2);
  t478 = v68;
  t479 = gg_function_19(t478, base + 2);
  t480 = ((int64_t)(((int64_t)UINT64_C(170))));
  t481 = gg_function_20(t479, t480, base + 2);
  v80 = gg_function_21(t481, base + 2);
  t482 = v68;
  t483 = gg_function_19(t482, base + 2);
  t484 = ((int64_t)(((int64_t)UINT64_C(255))));
  t485 = gg_function_20(t483, t484, base + 2);
  v81 = gg_function_21(t485, base + 2);
  t486 = v69;
  t487 = gg_function_19(t486, base + 2);
  t488 = ((int64_t)(((int64_t)UINT64_C(0))));
  t489 = gg_function_20(t487, t488, base + 2);
  v82 = gg_function_21(t489, base + 2);
  t490 = v69;
  t491 = gg_function_19(t490, base + 2);
  t492 = ((int64_t)(((int64_t)UINT64_C(85))));
  t493 = gg_function_20(t491, t492, base + 2);
  v83 = gg_function_21(t493, base + 2);
  t494 = v69;
  t495 = gg_function_19(t494, base + 2);
  t496 = ((int64_t)(((int64_t)UINT64_C(170))));
  t497 = gg_function_20(t495, t496, base + 2);
  v84 = gg_function_21(t497, base + 2);
  t498 = v69;
  t499 = gg_function_19(t498, base + 2);
  t500 = ((int64_t)(((int64_t)UINT64_C(255))));
  t501 = gg_function_20(t499, t500, base + 2);
  v85 = gg_function_21(t501, base + 2);
  t502 = v35;
  t503 = ((int64_t)(((int64_t)UINT64_C(177))));
  v86 = gg_function_20(t502, t503, base + 2);
  t504 = v36;
  t505 = ((int64_t)(((int64_t)UINT64_C(177))));
  v87 = gg_function_20(t504, t505, base + 2);
  t506 = v37;
  t507 = ((int64_t)(((int64_t)UINT64_C(177))));
  v88 = gg_function_20(t506, t507, base + 2);
  t508 = v38;
  t509 = ((int64_t)(((int64_t)UINT64_C(177))));
  v89 = gg_function_20(t508, t509, base + 2);
  t510 = v39;
  t511 = ((int64_t)(((int64_t)UINT64_C(177))));
  v90 = gg_function_20(t510, t511, base + 2);
  t512 = v40;
  t513 = ((int64_t)(((int64_t)UINT64_C(177))));
  v91 = gg_function_20(t512, t513, base + 2);
  t514 = v41;
  t515 = ((int64_t)(((int64_t)UINT64_C(177))));
  v92 = gg_function_20(t514, t515, base + 2);
  t516 = v42;
  t517 = ((int64_t)(((int64_t)UINT64_C(177))));
  v93 = gg_function_20(t516, t517, base + 2);
  memset(&t518, 0, sizeof(t518));
  t519 = ((int16_t)(t518));
  for (t520=0; t520<16; t520++) (v94).data[t520] = t519;
  t521 = v94;
  t522 = v35;
  t523 = v87;
  t524 = ((int64_t)(((int64_t)UINT64_C(170))));
  t525 = gg_function_22(t522, t523, t524, base + 2);
  t526 = v70;
  t527 = gg_function_23(t525, t526, base + 2);
  v94 = gg_function_25(t521, t527, base + 2);
  t528 = v94;
  t529 = v86;
  t530 = v36;
  t531 = ((int64_t)(((int64_t)UINT64_C(170))));
  t532 = gg_function_22(t529, t530, t531, base + 2);
  t533 = v71;
  t534 = gg_function_23(t532, t533, base + 2);
  v94 = gg_function_25(t528, t534, base + 2);
  t535 = v94;
  t536 = v37;
  t537 = v89;
  t538 = ((int64_t)(((int64_t)UINT64_C(170))));
  t539 = gg_function_22(t536, t537, t538, base + 2);
  t540 = v72;
  t541 = gg_function_23(t539, t540, base + 2);
  v94 = gg_function_25(t535, t541, base + 2);
  t542 = v94;
  t543 = v88;
  t544 = v38;
  t545 = ((int64_t)(((int64_t)UINT64_C(170))));
  t546 = gg_function_22(t543, t544, t545, base + 2);
  t547 = v73;
  t548 = gg_function_23(t546, t547, base + 2);
  v94 = gg_function_25(t542, t548, base + 2);
  t549 = v94;
  t550 = v39;
  t551 = v91;
  t552 = ((int64_t)(((int64_t)UINT64_C(170))));
  t553 = gg_function_22(t550, t551, t552, base + 2);
  t554 = v74;
  t555 = gg_function_23(t553, t554, base + 2);
  v94 = gg_function_25(t549, t555, base + 2);
  t556 = v94;
  t557 = v90;
  t558 = v40;
  t559 = ((int64_t)(((int64_t)UINT64_C(170))));
  t560 = gg_function_22(t557, t558, t559, base + 2);
  t561 = v75;
  t562 = gg_function_23(t560, t561, base + 2);
  v94 = gg_function_25(t556, t562, base + 2);
  t563 = v94;
  t564 = v41;
  t565 = v93;
  t566 = ((int64_t)(((int64_t)UINT64_C(170))));
  t567 = gg_function_22(t564, t565, t566, base + 2);
  t568 = v76;
  t569 = gg_function_23(t567, t568, base + 2);
  v94 = gg_function_25(t563, t569, base + 2);
  t570 = v94;
  t571 = v92;
  t572 = v42;
  t573 = ((int64_t)(((int64_t)UINT64_C(170))));
  t574 = gg_function_22(t571, t572, t573, base + 2);
  t575 = v77;
  t576 = gg_function_23(t574, t575, base + 2);
  v94 = gg_function_25(t570, t576, base + 2);
  t577 = v43;
  t578 = ((int64_t)(((int64_t)UINT64_C(177))));
  v95 = gg_function_20(t577, t578, base + 2);
  t579 = v44;
  t580 = ((int64_t)(((int64_t)UINT64_C(177))));
  v96 = gg_function_20(t579, t580, base + 2);
  t581 = v45;
  t582 = ((int64_t)(((int64_t)UINT64_C(177))));
  v97 = gg_function_20(t581, t582, base + 2);
  t583 = v46;
  t584 = ((int64_t)(((int64_t)UINT64_C(177))));
  v98 = gg_function_20(t583, t584, base + 2);
  t585 = v47;
  t586 = ((int64_t)(((int64_t)UINT64_C(177))));
  v99 = gg_function_20(t585, t586, base + 2);
  t587 = v48;
  t588 = ((int64_t)(((int64_t)UINT64_C(177))));
  v100 = gg_function_20(t587, t588, base + 2);
  t589 = v49;
  t590 = ((int64_t)(((int64_t)UINT64_C(177))));
  v101 = gg_function_20(t589, t590, base + 2);
  t591 = v50;
  t592 = ((int64_t)(((int64_t)UINT64_C(177))));
  v102 = gg_function_20(t591, t592, base + 2);
  memset(&t593, 0, sizeof(t593));
  t594 = ((int16_t)(t593));
  for (t595=0; t595<16; t595++) (v103).data[t595] = t594;
  t596 = v103;
  t597 = v43;
  t598 = v96;
  t599 = ((int64_t)(((int64_t)UINT64_C(170))));
  t600 = gg_function_22(t597, t598, t599, base + 2);
  t601 = v78;
  t602 = gg_function_23(t600, t601, base + 2);
  v103 = gg_function_25(t596, t602, base + 2);
  t603 = v103;
  t604 = v95;
  t605 = v44;
  t606 = ((int64_t)(((int64_t)UINT64_C(170))));
  t607 = gg_function_22(t604, t605, t606, base + 2);
  t608 = v79;
  t609 = gg_function_23(t607, t608, base + 2);
  v103 = gg_function_25(t603, t609, base + 2);
  t610 = v103;
  t611 = v45;
  t612 = v98;
  t613 = ((int64_t)(((int64_t)UINT64_C(170))));
  t614 = gg_function_22(t611, t612, t613, base + 2);
  t615 = v80;
  t616 = gg_function_23(t614, t615, base + 2);
  v103 = gg_function_25(t610, t616, base + 2);
  t617 = v103;
  t618 = v97;
  t619 = v46;
  t620 = ((int64_t)(((int64_t)UINT64_C(170))));
  t621 = gg_function_22(t618, t619, t620, base + 2);
  t622 = v81;
  t623 = gg_function_23(t621, t622, base + 2);
  v103 = gg_function_25(t617, t623, base + 2);
  t624 = v103;
  t625 = v47;
  t626 = v100;
  t627 = ((int64_t)(((int64_t)UINT64_C(170))));
  t628 = gg_function_22(t625, t626, t627, base + 2);
  t629 = v82;
  t630 = gg_function_23(t628, t629, base + 2);
  v103 = gg_function_25(t624, t630, base + 2);
  t631 = v103;
  t632 = v99;
  t633 = v48;
  t634 = ((int64_t)(((int64_t)UINT64_C(170))));
  t635 = gg_function_22(t632, t633, t634, base + 2);
  t636 = v83;
  t637 = gg_function_23(t635, t636, base + 2);
  v103 = gg_function_25(t631, t637, base + 2);
  t638 = v103;
  t639 = v49;
  t640 = v102;
  t641 = ((int64_t)(((int64_t)UINT64_C(170))));
  t642 = gg_function_22(t639, t640, t641, base + 2);
  t643 = v84;
  t644 = gg_function_23(t642, t643, base + 2);
  v103 = gg_function_25(t638, t644, base + 2);
  t645 = v103;
  t646 = v101;
  t647 = v50;
  t648 = ((int64_t)(((int64_t)UINT64_C(170))));
  t649 = gg_function_22(t646, t647, t648, base + 2);
  t650 = v85;
  t651 = gg_function_23(t649, t650, base + 2);
  v103 = gg_function_25(t645, t651, base + 2);
  t652 = v94;
  t653 = v63;
  v104 = gg_function_26(t652, t653, base + 2);
  t654 = v103;
  t655 = v64;
  v105 = gg_function_26(t654, t655, base + 2);
  t656 = v104;
  t657 = v105;
  v106 = gg_function_28(t656, t657, base + 2);
  t658 = v31;
  v107 = gg_function_29(t658, base + 2);
  t659 = v107;
  t660 = v65;
  v108 = gg_function_26(t659, t660, base + 2);
  t661 = v31;
  v31 = gg_function_30(t661, base + 2);
  t662 = v26;
  t663 = v106;
  v26 = gg_function_28(t662, t663, base + 2);
  t664 = v27;
  t665 = v108;
  v27 = gg_function_28(t664, t665, base + 2);
  }
label19:;
  t143++; goto label18;
label20:;
  t666 = v22;
  t667 = ((float)(v25));
  t668 = gg_function_31(t667, base + 2);
  v109 = gg_function_32(t666, t668, base + 2);
  t669 = v23;
  t670 = ((float)(v25));
  t671 = gg_function_31(t670, base + 2);
  v110 = gg_function_32(t669, t671, base + 2);
  t672 = v26;
  t673 = gg_function_33(t672, base + 2);
  t674 = v109;
  t675 = v13;
  v13 = gg_function_34(t673, t674, t675, base + 2);
  t676 = v27;
  t677 = gg_function_33(t676, base + 2);
  t678 = v110;
  t679 = v14;
  v14 = gg_function_34(t677, t678, t679, base + 2);
  }
label4:;
  t36++; goto label3;
label5:;
  t680 = v13;
  t681 = v11;
  v111 = gg_function_36(t680, t681, base + 2);
  t683 = ((int64_t)UINT64_C(8));
  t682 = 0;
label21:;
  if (t682 >= t683) goto label23;
  v112 = t682;
  {
  t684 = v4;
  t685 = ((int64_t)(v12));
  t686 = ((int64_t)(((int64_t)UINT64_C(8))));
  t687 = ((int64_t)(((int64_t)((uint64_t)(t685) * (uint64_t)(t686)))));
  t688 = ((int64_t)(v112));
  t689 = ((int64_t)((uint64_t)(t687) + (uint64_t)(t688)));
  t690 = ((GG_UNALIGNED gg_unaligned_3 *)t684.data) + gg_index(t689, t684.len);
  /* FG_DIRECT_SLICE_INDEX */
  t691 = v112;
  t692 = ((GG_UNALIGNED gg_unaligned_3 *)(v111).data) + gg_index(t691, ((int64_t)UINT64_C(8)));
  t693 = ((float)((*t692)));
  /* FG_DIRECT_SLICE_INDEX */
  t694 = v112;
  t695 = ((GG_UNALIGNED gg_unaligned_3 *)(v14).data) + gg_index(t694, ((int64_t)UINT64_C(8)));
  t696 = ((float)((*t695)));
  (*t690) = ((float)(((t693) - (t696))));
  }
label22:;
  t682++; goto label21;
label23:;
  }
label1:;
  t26++; goto label0;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  for (t697 = 0; t697 < 2; t697++) gg_stacks[base + t697].top = 0;
  return t0;
}
static void gg_function_0(int64_t base) {
  _Alignas(8) uint8_t t0;
  int64_t t1;
  _Alignas(8) uint8_t t2;
  int64_t t3;
  _Alignas(8) float t4;
  _Alignas(8) float t5;
  int64_t t6;
  _Alignas(8) gg_slice t7;
  _Alignas(8) gg_slice t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) gg_slice t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) gg_header t13;
  int64_t t14;
  _Alignas(8) gg_type_4 v0;
  _Alignas(8) gg_type_6 v1;
  _Alignas(8) gg_type_7 v2;
  {
  t0 = ((uint8_t)(((int64_t)UINT64_C(0))));
  for (t1=0; t1<1152; t1++) (v0).data[t1] = t0;
  t2 = ((uint8_t)(((int64_t)UINT64_C(0))));
  for (t3=0; t3<292; t3++) (v1).data[t3] = t2;
  memset(&t4, 0, sizeof(t4));
  t5 = ((float)(t4));
  for (t6=0; t6<8; t6++) (v2).data[t6] = t5;
  t7 = gg_view((v0).data, 1152);
  t8 = gg_view((v1).data, 292);
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((int64_t)(((int64_t)UINT64_C(8))));
  t11 = gg_view((v2).data, 8);
  t12 = gg_function_1(t7, t8, t9, t10, t11, base + 0);
  if (!(t12)) gg_fail("assertion failed");
  (t13).base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; (t13).len = 0;
  gg_text(gg_ref(&t13, gg_get_stack(base + 0)), gg_view((void *)"\147\145\155\166\040\163\155\157\153\145\040\157\153", 13));
  gg_append(gg_ref(&t13, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t13).base, (t13).len));
  }
  for (t14 = 0; t14 < 1; t14++) gg_stacks[base + t14].top = 0;
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
  gg_globals.v244 = ((double)(gg_double_bits(UINT64_C(4614256656552045848))));
  gg_globals.v245 = ((double)(gg_double_bits(UINT64_C(4618760256179416344))));
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
