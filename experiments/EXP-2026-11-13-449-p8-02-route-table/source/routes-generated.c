#ifndef FGOOSE_PACKED
#define FGOOSE_PACKED 1
#endif
#if defined(__AVX2__)
#include <immintrin.h>
#endif
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
typedef struct { unsigned char *base; int64_t len; } gg_header;
typedef struct { unsigned char *base; size_t top, committed; } gg_stack;
typedef struct { gg_header *header; gg_stack *stack; } gg_reference;
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
#pragma pack(push, 1)
typedef struct { double data[23]; } gg_type_2;
#pragma pack(pop)
typedef int64_t gg_unaligned_3 GG_ALIGN1;
typedef uint8_t gg_unaligned_4 GG_ALIGN1;
typedef gg_slice gg_unaligned_5 GG_ALIGN1;
typedef gg_slice gg_unaligned_8 GG_ALIGN1;
typedef float gg_unaligned_9 GG_ALIGN1;
typedef uint64_t gg_unaligned_10 GG_ALIGN1;
typedef int8_t gg_unaligned_11 GG_ALIGN1;
typedef int16_t gg_unaligned_12 GG_ALIGN1;
typedef int32_t gg_unaligned_13 GG_ALIGN1;
typedef uint16_t gg_unaligned_14 GG_ALIGN1;
typedef uint32_t gg_unaligned_15 GG_ALIGN1;
typedef struct gg_type_16 gg_type_16;
#pragma pack(push, 1)
struct gg_type_16 {
  uint64_t f0;
};
#pragma pack(pop)
typedef gg_type_16 *gg_type_17;
typedef gg_type_17 gg_unaligned_17 GG_ALIGN1;
typedef struct gg_type_18 gg_type_18;
#pragma pack(push, 1)
typedef struct { uint8_t data[256]; } gg_type_19;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[3]; } gg_type_20;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[16]; } gg_type_21;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_18 {
  gg_type_19 f1;
  uint8_t f2;
  uint8_t f3;
  gg_type_20 f4;
  gg_type_21 f5;
  gg_type_21 f6;
};
#pragma pack(pop)
typedef gg_type_19 *gg_type_22;
typedef gg_type_22 gg_unaligned_22 GG_ALIGN1;
typedef const gg_type_18 *gg_type_23;
typedef gg_type_23 gg_unaligned_23 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_4 *gg_type_24;
typedef gg_type_24 gg_unaligned_24 GG_ALIGN1;
typedef gg_reference gg_unaligned_26 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { uint8_t len; uint8_t data[64]; } gg_type_27;
#pragma pack(pop)
_Static_assert(sizeof(gg_type_27) == 65, "limited array layout mismatch");
#pragma pack(push, 1)
typedef struct { uint8_t data[1]; } gg_type_28;
#pragma pack(pop)
typedef gg_type_27 *gg_type_29;
typedef gg_type_29 gg_unaligned_29 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { gg_slice data[1]; } gg_type_31;
#pragma pack(pop)
typedef GG_UNALIGNED gg_unaligned_5 *gg_type_32;
typedef gg_type_32 gg_unaligned_32 GG_ALIGN1;
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(double a, double b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, double value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(int64_t a, int64_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, int64_t value);
static uint8_t gg_eq_4(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint8_t value);
static uint8_t gg_eq_5(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_slice value);
static uint8_t gg_eq_6(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint8_t value);
static uint8_t gg_eq_8(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_slice value);
static uint8_t gg_eq_9(float a, float b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, float value);
static uint8_t gg_eq_10(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, uint64_t value);
static uint8_t gg_eq_11(int8_t a, int8_t b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, int8_t value);
static uint8_t gg_eq_12(int16_t a, int16_t b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, int16_t value);
static uint8_t gg_eq_13(int32_t a, int32_t b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, int32_t value);
static uint8_t gg_eq_14(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, uint16_t value);
static uint8_t gg_eq_15(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, uint32_t value);
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, gg_type_16 value);
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_type_17 value);
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, gg_type_18 value);
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, gg_type_19 value);
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
static uint8_t gg_eq_25(gg_header a, gg_header b);
static uint8_t gg_equal_elements_25(gg_slice a, gg_slice b);
static void gg_render_25(gg_reference out, gg_header value);
static uint8_t gg_eq_26(gg_reference a, gg_reference b);
static uint8_t gg_equal_elements_26(gg_slice a, gg_slice b);
static void gg_render_26(gg_reference out, gg_reference value);
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b);
static uint8_t gg_equal_elements_27(gg_slice a, gg_slice b);
static void gg_render_27(gg_reference out, gg_type_27 value);
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b);
static uint8_t gg_equal_elements_28(gg_slice a, gg_slice b);
static void gg_render_28(gg_reference out, gg_type_28 value);
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b);
static uint8_t gg_equal_elements_29(gg_slice a, gg_slice b);
static void gg_render_29(gg_reference out, gg_type_29 value);
static uint8_t gg_eq_30(gg_header a, gg_header b);
static uint8_t gg_equal_elements_30(gg_slice a, gg_slice b);
static void gg_render_30(gg_reference out, gg_header value);
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b);
static uint8_t gg_equal_elements_31(gg_slice a, gg_slice b);
static void gg_render_31(gg_reference out, gg_type_31 value);
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b);
static uint8_t gg_equal_elements_32(gg_slice a, gg_slice b);
static void gg_render_32(gg_reference out, gg_type_32 value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((GG_UNALIGNED gg_unaligned_1*)a.data)[i],((GG_UNALIGNED gg_unaligned_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(double a, double b) {
return a == b;
}
static void gg_render_1(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
return gg_equal_elements_1(gg_view((a).data, 23),gg_view((b).data, 23));
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
int64_t i; gg_slice view=gg_view((value).data, 23);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((GG_UNALIGNED gg_unaligned_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((GG_UNALIGNED gg_unaligned_3*)a.data)[i],((GG_UNALIGNED gg_unaligned_3*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((GG_UNALIGNED gg_unaligned_4*)a.data)[i],((GG_UNALIGNED gg_unaligned_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((GG_UNALIGNED gg_unaligned_5*)a.data)[i],((GG_UNALIGNED gg_unaligned_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_slice a, gg_slice b) {
return gg_equal_elements_4(a,b);
}
static void gg_render_5(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((GG_UNALIGNED gg_unaligned_8*)a.data)[i],((GG_UNALIGNED gg_unaligned_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_slice a, gg_slice b) {
return gg_equal_elements_4(a,b);
}
static void gg_render_8(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((GG_UNALIGNED gg_unaligned_9*)a.data)[i],((GG_UNALIGNED gg_unaligned_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(float a, float b) {
return a == b;
}
static void gg_render_9(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((GG_UNALIGNED gg_unaligned_10*)a.data)[i],((GG_UNALIGNED gg_unaligned_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_10(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((GG_UNALIGNED gg_unaligned_11*)a.data)[i],((GG_UNALIGNED gg_unaligned_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_11(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((GG_UNALIGNED gg_unaligned_12*)a.data)[i],((GG_UNALIGNED gg_unaligned_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_12(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((GG_UNALIGNED gg_unaligned_13*)a.data)[i],((GG_UNALIGNED gg_unaligned_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_13(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((GG_UNALIGNED gg_unaligned_14*)a.data)[i],((GG_UNALIGNED gg_unaligned_14*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_14(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((GG_UNALIGNED gg_unaligned_15*)a.data)[i],((GG_UNALIGNED gg_unaligned_15*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_15(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((gg_type_16*)a.data)[i],((gg_type_16*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b) {
if(!gg_eq_10(a.f0,b.f0))return 0;
return 1;
}
static void gg_render_16(gg_reference out, gg_type_16 value) {
gg_append(out,"\162\156\147\040\173\040",6,1);
gg_render_10(out, value.f0);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((GG_UNALIGNED gg_unaligned_17*)a.data)[i],((GG_UNALIGNED gg_unaligned_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b) {
return a == b;
}
static void gg_render_17(gg_reference out, gg_type_17 value) {
gg_render_16(out,*value);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_type_18*)a.data)[i],((gg_type_18*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b) {
if(!gg_eq_19(a.f1,b.f1))return 0;
if(!gg_eq_4(a.f2,b.f2))return 0;
if(!gg_eq_6(a.f3,b.f3))return 0;
if(!gg_eq_20(a.f4,b.f4))return 0;
if(!gg_eq_21(a.f5,b.f5))return 0;
if(!gg_eq_21(a.f6,b.f6))return 0;
return 1;
}
static void gg_render_18(gg_reference out, gg_type_18 value) {
gg_append(out,"\102\171\164\145\123\145\164\040\173\040",10,1);
gg_render_19(out, value.f1);
gg_append(out,", ",2,1);
gg_render_4(out, value.f2);
gg_append(out,", ",2,1);
gg_render_6(out, value.f3);
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f4).data, 3));
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f5).data, 16));
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f6).data, 16));
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((gg_type_19*)a.data)[i],((gg_type_19*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b) {
return gg_equal_elements_6(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_19(gg_reference out, gg_type_19 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((uint8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((gg_type_20*)a.data)[i],((gg_type_20*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b) {
return gg_equal_elements_4(gg_view((a).data, 3),gg_view((b).data, 3));
}
static void gg_render_20(gg_reference out, gg_type_20 value) {
gg_text(out,gg_view((value).data, 3));
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((gg_type_21*)a.data)[i],((gg_type_21*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b) {
return gg_equal_elements_4(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_21(gg_reference out, gg_type_21 value) {
gg_text(out,gg_view((value).data, 16));
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((GG_UNALIGNED gg_unaligned_22*)a.data)[i],((GG_UNALIGNED gg_unaligned_22*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b) {
return a == b;
}
static void gg_render_22(gg_reference out, gg_type_22 value) {
gg_render_19(out,*value);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((GG_UNALIGNED gg_unaligned_23*)a.data)[i],((GG_UNALIGNED gg_unaligned_23*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b) {
return a == b;
}
static void gg_render_23(gg_reference out, gg_type_23 value) {
gg_render_18(out,*value);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((GG_UNALIGNED gg_unaligned_24*)a.data)[i],((GG_UNALIGNED gg_unaligned_24*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b) {
return a == b;
}
static void gg_render_24(gg_reference out, gg_type_24 value) {
gg_render_4(out,*value);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((gg_header*)a.data)[i],((gg_header*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(gg_header a, gg_header b) {
return gg_equal_elements_4(gg_view((a).base, (a).len),gg_view((b).base, (b).len));
}
static void gg_render_25(gg_reference out, gg_header value) {
gg_text(out,gg_view((value).base, (value).len));
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((GG_UNALIGNED gg_unaligned_26*)a.data)[i],((GG_UNALIGNED gg_unaligned_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_reference a, gg_reference b) {
return a.header == b.header;
}
static void gg_render_26(gg_reference out, gg_reference value) {
gg_render_25(out,*value.header);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((gg_type_27*)a.data)[i],((gg_type_27*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b) {
  if ((uint64_t)(a).len > UINT64_C(64)) gg_fail("invalid limited array length");
  if ((uint64_t)(b).len > UINT64_C(64)) gg_fail("invalid limited array length");
return gg_equal_elements_4(gg_view((a).data, (a).len),gg_view((b).data, (b).len));
}
static void gg_render_27(gg_reference out, gg_type_27 value) {
  if ((uint64_t)(value).len > UINT64_C(64)) gg_fail("invalid limited array length");
gg_text(out,gg_view((value).data, (value).len));
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_type_28*)a.data)[i],((gg_type_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
return gg_equal_elements_4(gg_view((a).data, 0),gg_view((b).data, 0));
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
gg_text(out,gg_view((value).data, 0));
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((GG_UNALIGNED gg_unaligned_29*)a.data)[i],((GG_UNALIGNED gg_unaligned_29*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b) {
return a == b;
}
static void gg_render_29(gg_reference out, gg_type_29 value) {
gg_render_27(out,*value);
}
static uint8_t gg_equal_elements_30(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_30(((gg_header*)a.data)[i],((gg_header*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_30(gg_header a, gg_header b) {
return gg_equal_elements_5(gg_view((a).base, (a).len),gg_view((b).base, (b).len));
}
static void gg_render_30(gg_reference out, gg_header value) {
int64_t i; gg_slice view=gg_view((value).base, (value).len);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, ((GG_UNALIGNED gg_unaligned_5*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_31(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_31(((gg_type_31*)a.data)[i],((gg_type_31*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b) {
return gg_equal_elements_5(gg_view((a).data, 0),gg_view((b).data, 0));
}
static void gg_render_31(gg_reference out, gg_type_31 value) {
int64_t i; gg_slice view=gg_view((value).data, 0);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, ((GG_UNALIGNED gg_unaligned_5*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_32(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_32(((GG_UNALIGNED gg_unaligned_32*)a.data)[i],((GG_UNALIGNED gg_unaligned_32*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b) {
return a == b;
}
static void gg_render_32(gg_reference out, gg_type_32 value) {
gg_render_5(out,*value);
}
static struct {
  unsigned char dummy;
  _Alignas(8) gg_type_2 v120;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(int64_t p0, gg_slice p1, int64_t base);
static uint8_t gg_function_2(gg_slice p0, gg_slice p1, int64_t base);
static int64_t gg_function_3(gg_slice p0, gg_slice p1, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint8_t t6;
  _Alignas(8) int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_3(gg_slice p0, gg_slice p1, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) int64_t t9;
  const GG_UNALIGNED gg_unaligned_4 * t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) int64_t t12;
  const GG_UNALIGNED gg_unaligned_4 * t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) uint8_t t16;
  gg_slice t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) gg_slice t22;
  _Alignas(8) gg_slice t23;
  _Alignas(8) uint8_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  int64_t t28;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  _Alignas(8) int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)((v1).len));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)((v1).len));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = ((int64_t)((v0).len));
  t8 = ((t6) <= (t7));
  if (!(t8)) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t9 = v2;
  t10 = ((const GG_UNALIGNED gg_unaligned_4 *)(v0).data) + gg_index(t9, (v0).len);
  t11 = ((uint8_t)((*t10)));
  /* FG_DIRECT_SLICE_INDEX */
  t12 = ((int64_t)UINT64_C(0));
  t13 = ((const GG_UNALIGNED gg_unaligned_4 *)(v1).data) + gg_index(t12, (v1).len);
  t14 = ((uint8_t)((*t13)));
  t15 = ((t11) == (t14));
  t16 = t15;
  if (t16) {
  t17 = v0;
  t18 = v2;
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)((v1).len));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = gg_sub(t17, t18, t21, sizeof(uint8_t));
  t23 = v1;
  t16 = gg_equal_elements_4(t22, t23);
  }
  t24 = t16;
  if (t24) {
  {
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
  }
  t25 = v2;
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  }
  goto label1;
label2:;
  t27 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t27)))));
  goto gg_done;
  }
gg_done:;
  for (t28 = 0; t28 < 2; t28++) gg_stacks[base + t28].top = 0;
  return t0;
}
static uint8_t gg_function_2(gg_slice p0, gg_slice p1, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  gg_slice t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) gg_slice t7;
  _Alignas(8) gg_slice t8;
  int64_t t9;
  _Alignas(8) gg_slice v0;
  _Alignas(8) gg_slice v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)((v1).len));
  t2 = ((int64_t)((v0).len));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (t4) {
  t5 = v0;
  t6 = (v1).len;
  t7 = gg_sub(t5, 0, t6, sizeof(uint8_t));
  t8 = v1;
  t4 = gg_equal_elements_4(t7, t8);
  }
  t0 = t4;
  }
  for (t9 = 0; t9 < 2; t9++) gg_stacks[base + t9].top = 0;
  return t0;
}
static int64_t gg_function_1(int64_t p0, gg_slice p1, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) gg_slice t5;
  _Alignas(8) gg_slice t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) uint8_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) gg_slice t12;
  _Alignas(8) gg_slice t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) uint8_t t18;
  _Alignas(8) gg_slice t19;
  _Alignas(8) gg_slice t20;
  _Alignas(8) uint8_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) uint8_t t24;
  _Alignas(8) uint8_t t25;
  _Alignas(8) gg_slice t26;
  _Alignas(8) gg_slice t27;
  _Alignas(8) uint8_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) uint8_t t31;
  _Alignas(8) uint8_t t32;
  _Alignas(8) gg_slice t33;
  _Alignas(8) gg_slice t34;
  _Alignas(8) uint8_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) uint8_t t38;
  _Alignas(8) uint8_t t39;
  _Alignas(8) gg_slice t40;
  _Alignas(8) gg_slice t41;
  _Alignas(8) uint8_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) uint8_t t45;
  _Alignas(8) uint8_t t46;
  _Alignas(8) gg_slice t47;
  _Alignas(8) gg_slice t48;
  _Alignas(8) uint8_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) uint8_t t52;
  _Alignas(8) uint8_t t53;
  _Alignas(8) gg_slice t54;
  _Alignas(8) gg_slice t55;
  _Alignas(8) uint8_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) uint8_t t59;
  _Alignas(8) uint8_t t60;
  _Alignas(8) gg_slice t61;
  _Alignas(8) gg_slice t62;
  _Alignas(8) uint8_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) uint8_t t66;
  _Alignas(8) uint8_t t67;
  _Alignas(8) gg_slice t68;
  _Alignas(8) gg_slice t69;
  _Alignas(8) uint8_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) uint8_t t73;
  _Alignas(8) uint8_t t74;
  _Alignas(8) gg_slice t75;
  _Alignas(8) gg_slice t76;
  _Alignas(8) uint8_t t77;
  _Alignas(8) int64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) uint8_t t80;
  _Alignas(8) uint8_t t81;
  _Alignas(8) gg_slice t82;
  _Alignas(8) gg_slice t83;
  _Alignas(8) uint8_t t84;
  _Alignas(8) int64_t t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) uint8_t t87;
  _Alignas(8) uint8_t t88;
  _Alignas(8) gg_slice t89;
  _Alignas(8) gg_slice t90;
  _Alignas(8) uint8_t t91;
  _Alignas(8) int64_t t92;
  _Alignas(8) int64_t t93;
  _Alignas(8) uint8_t t94;
  _Alignas(8) uint8_t t95;
  _Alignas(8) gg_slice t96;
  _Alignas(8) gg_slice t97;
  _Alignas(8) uint8_t t98;
  _Alignas(8) int64_t t99;
  _Alignas(8) int64_t t100;
  _Alignas(8) uint8_t t101;
  _Alignas(8) uint8_t t102;
  _Alignas(8) gg_slice t103;
  _Alignas(8) gg_slice t104;
  _Alignas(8) uint8_t t105;
  _Alignas(8) int64_t t106;
  _Alignas(8) int64_t t107;
  _Alignas(8) uint8_t t108;
  _Alignas(8) uint8_t t109;
  _Alignas(8) gg_slice t110;
  _Alignas(8) gg_slice t111;
  _Alignas(8) uint8_t t112;
  _Alignas(8) int64_t t113;
  _Alignas(8) int64_t t114;
  _Alignas(8) uint8_t t115;
  _Alignas(8) uint8_t t116;
  _Alignas(8) gg_slice t117;
  _Alignas(8) gg_slice t118;
  _Alignas(8) uint8_t t119;
  _Alignas(8) int64_t t120;
  _Alignas(8) int64_t t121;
  _Alignas(8) uint8_t t122;
  _Alignas(8) uint8_t t123;
  _Alignas(8) gg_slice t124;
  _Alignas(8) gg_slice t125;
  _Alignas(8) uint8_t t126;
  _Alignas(8) int64_t t127;
  _Alignas(8) int64_t t128;
  _Alignas(8) uint8_t t129;
  _Alignas(8) uint8_t t130;
  _Alignas(8) gg_slice t131;
  _Alignas(8) gg_slice t132;
  _Alignas(8) uint8_t t133;
  _Alignas(8) int64_t t134;
  _Alignas(8) int64_t t135;
  _Alignas(8) uint8_t t136;
  _Alignas(8) uint8_t t137;
  _Alignas(8) gg_slice t138;
  _Alignas(8) gg_slice t139;
  _Alignas(8) uint8_t t140;
  _Alignas(8) int64_t t141;
  _Alignas(8) int64_t t142;
  _Alignas(8) uint8_t t143;
  _Alignas(8) uint8_t t144;
  _Alignas(8) gg_slice t145;
  _Alignas(8) gg_slice t146;
  _Alignas(8) uint8_t t147;
  _Alignas(8) int64_t t148;
  _Alignas(8) int64_t t149;
  _Alignas(8) uint8_t t150;
  _Alignas(8) uint8_t t151;
  _Alignas(8) gg_slice t152;
  _Alignas(8) gg_slice t153;
  _Alignas(8) uint8_t t154;
  _Alignas(8) int64_t t155;
  _Alignas(8) int64_t t156;
  _Alignas(8) uint8_t t157;
  _Alignas(8) uint8_t t158;
  _Alignas(8) gg_slice t159;
  _Alignas(8) gg_slice t160;
  _Alignas(8) uint8_t t161;
  _Alignas(8) int64_t t162;
  _Alignas(8) int64_t t163;
  _Alignas(8) uint8_t t164;
  _Alignas(8) uint8_t t165;
  _Alignas(8) gg_slice t166;
  _Alignas(8) gg_slice t167;
  _Alignas(8) uint8_t t168;
  _Alignas(8) int64_t t169;
  _Alignas(8) int64_t t170;
  _Alignas(8) uint8_t t171;
  _Alignas(8) uint8_t t172;
  _Alignas(8) gg_slice t173;
  _Alignas(8) gg_slice t174;
  _Alignas(8) uint8_t t175;
  _Alignas(8) int64_t t176;
  _Alignas(8) int64_t t177;
  _Alignas(8) uint8_t t178;
  _Alignas(8) uint8_t t179;
  _Alignas(8) gg_slice t180;
  _Alignas(8) gg_slice t181;
  _Alignas(8) uint8_t t182;
  _Alignas(8) int64_t t183;
  _Alignas(8) int64_t t184;
  _Alignas(8) uint8_t t185;
  _Alignas(8) uint8_t t186;
  _Alignas(8) gg_slice t187;
  _Alignas(8) gg_slice t188;
  _Alignas(8) uint8_t t189;
  _Alignas(8) int64_t t190;
  _Alignas(8) int64_t t191;
  _Alignas(8) uint8_t t192;
  _Alignas(8) uint8_t t193;
  _Alignas(8) gg_slice t194;
  _Alignas(8) gg_slice t195;
  _Alignas(8) uint8_t t196;
  _Alignas(8) int64_t t197;
  _Alignas(8) int64_t t198;
  _Alignas(8) uint8_t t199;
  _Alignas(8) uint8_t t200;
  _Alignas(8) gg_slice t201;
  _Alignas(8) gg_slice t202;
  _Alignas(8) uint8_t t203;
  _Alignas(8) int64_t t204;
  _Alignas(8) int64_t t205;
  _Alignas(8) uint8_t t206;
  _Alignas(8) uint8_t t207;
  _Alignas(8) gg_slice t208;
  _Alignas(8) gg_slice t209;
  _Alignas(8) uint8_t t210;
  _Alignas(8) int64_t t211;
  _Alignas(8) int64_t t212;
  _Alignas(8) uint8_t t213;
  _Alignas(8) uint8_t t214;
  _Alignas(8) gg_slice t215;
  _Alignas(8) gg_slice t216;
  _Alignas(8) uint8_t t217;
  _Alignas(8) int64_t t218;
  _Alignas(8) int64_t t219;
  _Alignas(8) uint8_t t220;
  _Alignas(8) uint8_t t221;
  _Alignas(8) gg_slice t222;
  _Alignas(8) gg_slice t223;
  _Alignas(8) uint8_t t224;
  _Alignas(8) int64_t t225;
  _Alignas(8) int64_t t226;
  _Alignas(8) uint8_t t227;
  _Alignas(8) uint8_t t228;
  _Alignas(8) gg_slice t229;
  _Alignas(8) gg_slice t230;
  _Alignas(8) uint8_t t231;
  _Alignas(8) int64_t t232;
  _Alignas(8) int64_t t233;
  _Alignas(8) uint8_t t234;
  _Alignas(8) uint8_t t235;
  _Alignas(8) gg_slice t236;
  _Alignas(8) gg_slice t237;
  _Alignas(8) uint8_t t238;
  _Alignas(8) int64_t t239;
  _Alignas(8) int64_t t240;
  _Alignas(8) uint8_t t241;
  _Alignas(8) uint8_t t242;
  _Alignas(8) gg_slice t243;
  _Alignas(8) gg_slice t244;
  _Alignas(8) uint8_t t245;
  _Alignas(8) int64_t t246;
  _Alignas(8) int64_t t247;
  _Alignas(8) uint8_t t248;
  _Alignas(8) uint8_t t249;
  _Alignas(8) gg_slice t250;
  _Alignas(8) gg_slice t251;
  _Alignas(8) uint8_t t252;
  _Alignas(8) int64_t t253;
  _Alignas(8) int64_t t254;
  _Alignas(8) uint8_t t255;
  _Alignas(8) uint8_t t256;
  _Alignas(8) gg_slice t257;
  _Alignas(8) gg_slice t258;
  _Alignas(8) uint8_t t259;
  _Alignas(8) int64_t t260;
  _Alignas(8) int64_t t261;
  _Alignas(8) uint8_t t262;
  _Alignas(8) uint8_t t263;
  _Alignas(8) gg_slice t264;
  _Alignas(8) gg_slice t265;
  _Alignas(8) uint8_t t266;
  _Alignas(8) int64_t t267;
  _Alignas(8) int64_t t268;
  _Alignas(8) uint8_t t269;
  _Alignas(8) uint8_t t270;
  _Alignas(8) gg_slice t271;
  _Alignas(8) gg_slice t272;
  _Alignas(8) uint8_t t273;
  _Alignas(8) int64_t t274;
  _Alignas(8) int64_t t275;
  _Alignas(8) uint8_t t276;
  _Alignas(8) uint8_t t277;
  _Alignas(8) gg_slice t278;
  _Alignas(8) gg_slice t279;
  _Alignas(8) uint8_t t280;
  _Alignas(8) int64_t t281;
  _Alignas(8) int64_t t282;
  _Alignas(8) uint8_t t283;
  _Alignas(8) uint8_t t284;
  _Alignas(8) gg_slice t285;
  _Alignas(8) gg_slice t286;
  _Alignas(8) uint8_t t287;
  _Alignas(8) int64_t t288;
  _Alignas(8) int64_t t289;
  _Alignas(8) uint8_t t290;
  _Alignas(8) uint8_t t291;
  _Alignas(8) gg_slice t292;
  _Alignas(8) gg_slice t293;
  _Alignas(8) uint8_t t294;
  _Alignas(8) int64_t t295;
  _Alignas(8) int64_t t296;
  _Alignas(8) uint8_t t297;
  _Alignas(8) uint8_t t298;
  _Alignas(8) gg_slice t299;
  _Alignas(8) gg_slice t300;
  _Alignas(8) uint8_t t301;
  _Alignas(8) int64_t t302;
  _Alignas(8) int64_t t303;
  _Alignas(8) uint8_t t304;
  _Alignas(8) uint8_t t305;
  _Alignas(8) gg_slice t306;
  _Alignas(8) gg_slice t307;
  _Alignas(8) uint8_t t308;
  _Alignas(8) int64_t t309;
  _Alignas(8) int64_t t310;
  _Alignas(8) uint8_t t311;
  _Alignas(8) uint8_t t312;
  _Alignas(8) gg_slice t313;
  _Alignas(8) gg_slice t314;
  _Alignas(8) uint8_t t315;
  _Alignas(8) int64_t t316;
  _Alignas(8) int64_t t317;
  _Alignas(8) uint8_t t318;
  _Alignas(8) uint8_t t319;
  _Alignas(8) gg_slice t320;
  _Alignas(8) gg_slice t321;
  _Alignas(8) uint8_t t322;
  _Alignas(8) int64_t t323;
  _Alignas(8) int64_t t324;
  _Alignas(8) uint8_t t325;
  _Alignas(8) uint8_t t326;
  _Alignas(8) gg_slice t327;
  _Alignas(8) gg_slice t328;
  _Alignas(8) uint8_t t329;
  _Alignas(8) int64_t t330;
  _Alignas(8) int64_t t331;
  _Alignas(8) uint8_t t332;
  _Alignas(8) uint8_t t333;
  _Alignas(8) gg_slice t334;
  _Alignas(8) gg_slice t335;
  _Alignas(8) uint8_t t336;
  _Alignas(8) int64_t t337;
  _Alignas(8) int64_t t338;
  _Alignas(8) uint8_t t339;
  _Alignas(8) uint8_t t340;
  _Alignas(8) gg_slice t341;
  _Alignas(8) gg_slice t342;
  _Alignas(8) uint8_t t343;
  _Alignas(8) int64_t t344;
  _Alignas(8) int64_t t345;
  _Alignas(8) uint8_t t346;
  _Alignas(8) uint8_t t347;
  _Alignas(8) gg_slice t348;
  _Alignas(8) gg_slice t349;
  _Alignas(8) uint8_t t350;
  _Alignas(8) int64_t t351;
  _Alignas(8) int64_t t352;
  _Alignas(8) uint8_t t353;
  _Alignas(8) uint8_t t354;
  _Alignas(8) gg_slice t355;
  _Alignas(8) gg_slice t356;
  _Alignas(8) uint8_t t357;
  _Alignas(8) int64_t t358;
  _Alignas(8) int64_t t359;
  _Alignas(8) uint8_t t360;
  _Alignas(8) uint8_t t361;
  _Alignas(8) gg_slice t362;
  _Alignas(8) gg_slice t363;
  _Alignas(8) uint8_t t364;
  _Alignas(8) int64_t t365;
  _Alignas(8) int64_t t366;
  _Alignas(8) uint8_t t367;
  _Alignas(8) uint8_t t368;
  _Alignas(8) gg_slice t369;
  _Alignas(8) gg_slice t370;
  _Alignas(8) uint8_t t371;
  _Alignas(8) int64_t t372;
  _Alignas(8) int64_t t373;
  _Alignas(8) uint8_t t374;
  _Alignas(8) gg_slice t375;
  _Alignas(8) gg_slice t376;
  _Alignas(8) uint8_t t377;
  _Alignas(8) uint8_t t378;
  _Alignas(8) int64_t t379;
  _Alignas(8) int64_t t380;
  _Alignas(8) uint8_t t381;
  gg_slice t382;
  _Alignas(8) int64_t t383;
  _Alignas(8) gg_slice t384;
  _Alignas(8) gg_slice t385;
  _Alignas(8) int64_t t386;
  _Alignas(8) int64_t t387;
  _Alignas(8) int64_t t388;
  _Alignas(8) uint8_t t389;
  int64_t t390;
  _Alignas(8) int64_t v0;
  _Alignas(8) gg_slice v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (t4) {
  t5 = v1;
  t6 = gg_view((void *)"\057\150\145\141\154\164\150", 7);
  t4 = gg_equal_elements_4(t5, t6);
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t10 = ((t8) == (t9));
  t11 = t10;
  if (t11) {
  t12 = v1;
  t13 = gg_view((void *)"\057\166\061\057\150\145\141\154\164\150", 10);
  t11 = gg_equal_elements_4(t12, t13);
  }
  t14 = t11;
  if (t14) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t17 = ((t15) == (t16));
  t18 = t17;
  if (t18) {
  t19 = v1;
  t20 = gg_view((void *)"\057\155\145\164\162\151\143\163", 8);
  t18 = gg_equal_elements_4(t19, t20);
  }
  t21 = t18;
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t24 = ((t22) == (t23));
  t25 = t24;
  if (t25) {
  t26 = v1;
  t27 = gg_view((void *)"\057\160\162\157\160\163", 6);
  t25 = gg_equal_elements_4(t26, t27);
  }
  t28 = t25;
  if (t28) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t31 = ((t29) == (t30));
  t32 = t31;
  if (t32) {
  t33 = v1;
  t34 = gg_view((void *)"\057\160\162\157\160\163", 6);
  t32 = gg_equal_elements_4(t33, t34);
  }
  t35 = t32;
  if (t35) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  goto gg_done;
  }
  }
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t38 = ((t36) == (t37));
  t39 = t38;
  if (t39) {
  t40 = v1;
  t41 = gg_view((void *)"\057\155\157\144\145\154\163", 7);
  t39 = gg_equal_elements_4(t40, t41);
  }
  t42 = t39;
  if (t42) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  goto gg_done;
  }
  }
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t45 = ((t43) == (t44));
  t46 = t45;
  if (t46) {
  t47 = v1;
  t48 = gg_view((void *)"\057\166\061\057\155\157\144\145\154\163", 10);
  t46 = gg_equal_elements_4(t47, t48);
  }
  t49 = t46;
  if (t49) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  goto gg_done;
  }
  }
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = v1;
  t55 = gg_view((void *)"\057\155\157\144\145\154\163", 7);
  t53 = gg_equal_elements_4(t54, t55);
  }
  t56 = t53;
  if (t56) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  goto gg_done;
  }
  }
  t57 = ((int64_t)(v0));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t59 = ((t57) == (t58));
  t60 = t59;
  if (t60) {
  t61 = v1;
  t62 = gg_view((void *)"\057\155\157\144\145\154\163\057\154\157\141\144", 12);
  t60 = gg_equal_elements_4(t61, t62);
  }
  t63 = t60;
  if (t63) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  goto gg_done;
  }
  }
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t66 = ((t64) == (t65));
  t67 = t66;
  if (t67) {
  t68 = v1;
  t69 = gg_view((void *)"\057\155\157\144\145\154\163\057\165\156\154\157\141\144", 14);
  t67 = gg_equal_elements_4(t68, t69);
  }
  t70 = t67;
  if (t70) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  goto gg_done;
  }
  }
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t73 = ((t71) == (t72));
  t74 = t73;
  if (t74) {
  t75 = v1;
  t76 = gg_view((void *)"\057\155\157\144\145\154\163\057\163\163\145", 11);
  t74 = gg_equal_elements_4(t75, t76);
  }
  t77 = t74;
  if (t77) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(9))))));
  goto gg_done;
  }
  }
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t80 = ((t78) == (t79));
  t81 = t80;
  if (t81) {
  t82 = v1;
  t83 = gg_view((void *)"\057\155\157\144\145\154\163", 7);
  t81 = gg_equal_elements_4(t82, t83);
  }
  t84 = t81;
  if (t84) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  goto gg_done;
  }
  }
  t85 = ((int64_t)(v0));
  t86 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t87 = ((t85) == (t86));
  t88 = t87;
  if (t88) {
  t89 = v1;
  t90 = gg_view((void *)"\057\143\157\155\160\154\145\164\151\157\156", 11);
  t88 = gg_equal_elements_4(t89, t90);
  }
  t91 = t88;
  if (t91) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(11))))));
  goto gg_done;
  }
  }
  t92 = ((int64_t)(v0));
  t93 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t94 = ((t92) == (t93));
  t95 = t94;
  if (t95) {
  t96 = v1;
  t97 = gg_view((void *)"\057\143\157\155\160\154\145\164\151\157\156\163", 12);
  t95 = gg_equal_elements_4(t96, t97);
  }
  t98 = t95;
  if (t98) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(11))))));
  goto gg_done;
  }
  }
  t99 = ((int64_t)(v0));
  t100 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = v1;
  t104 = gg_view((void *)"\057\166\061\057\143\157\155\160\154\145\164\151\157\156\163", 15);
  t102 = gg_equal_elements_4(t103, t104);
  }
  t105 = t102;
  if (t105) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  goto gg_done;
  }
  }
  t106 = ((int64_t)(v0));
  t107 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t108 = ((t106) == (t107));
  t109 = t108;
  if (t109) {
  t110 = v1;
  t111 = gg_view((void *)"\057\143\150\141\164\057\143\157\155\160\154\145\164\151\157\156\163", 17);
  t109 = gg_equal_elements_4(t110, t111);
  }
  t112 = t109;
  if (t112) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(13))))));
  goto gg_done;
  }
  }
  t113 = ((int64_t)(v0));
  t114 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t115 = ((t113) == (t114));
  t116 = t115;
  if (t116) {
  t117 = v1;
  t118 = gg_view((void *)"\057\166\061\057\143\150\141\164\057\143\157\155\160\154\145\164\151\157\156\163", 20);
  t116 = gg_equal_elements_4(t117, t118);
  }
  t119 = t116;
  if (t119) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(13))))));
  goto gg_done;
  }
  }
  t120 = ((int64_t)(v0));
  t121 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t122 = ((t120) == (t121));
  t123 = t122;
  if (t123) {
  t124 = v1;
  t125 = gg_view((void *)"\057\166\061\057\143\150\141\164\057\143\157\155\160\154\145\164\151\157\156\163\057\143\157\156\164\162\157\154", 28);
  t123 = gg_equal_elements_4(t124, t125);
  }
  t126 = t123;
  if (t126) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(14))))));
  goto gg_done;
  }
  }
  t127 = ((int64_t)(v0));
  t128 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t129 = ((t127) == (t128));
  t130 = t129;
  if (t130) {
  t131 = v1;
  t132 = gg_view((void *)"\057\162\145\163\160\157\156\163\145\163", 10);
  t130 = gg_equal_elements_4(t131, t132);
  }
  t133 = t130;
  if (t133) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(15))))));
  goto gg_done;
  }
  }
  t134 = ((int64_t)(v0));
  t135 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t136 = ((t134) == (t135));
  t137 = t136;
  if (t137) {
  t138 = v1;
  t139 = gg_view((void *)"\057\166\061\057\162\145\163\160\157\156\163\145\163", 13);
  t137 = gg_equal_elements_4(t138, t139);
  }
  t140 = t137;
  if (t140) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(15))))));
  goto gg_done;
  }
  }
  t141 = ((int64_t)(v0));
  t142 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t143 = ((t141) == (t142));
  t144 = t143;
  if (t144) {
  t145 = v1;
  t146 = gg_view((void *)"\057\141\165\144\151\157\057\164\162\141\156\163\143\162\151\160\164\151\157\156\163", 21);
  t144 = gg_equal_elements_4(t145, t146);
  }
  t147 = t144;
  if (t147) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  goto gg_done;
  }
  }
  t148 = ((int64_t)(v0));
  t149 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t150 = ((t148) == (t149));
  t151 = t150;
  if (t151) {
  t152 = v1;
  t153 = gg_view((void *)"\057\166\061\057\141\165\144\151\157\057\164\162\141\156\163\143\162\151\160\164\151\157\156\163", 24);
  t151 = gg_equal_elements_4(t152, t153);
  }
  t154 = t151;
  if (t154) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  goto gg_done;
  }
  }
  t155 = ((int64_t)(v0));
  t156 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t157 = ((t155) == (t156));
  t158 = t157;
  if (t158) {
  t159 = v1;
  t160 = gg_view((void *)"\057\164\164\163", 4);
  t158 = gg_equal_elements_4(t159, t160);
  }
  t161 = t158;
  if (t161) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(17))))));
  goto gg_done;
  }
  }
  t162 = ((int64_t)(v0));
  t163 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t164 = ((t162) == (t163));
  t165 = t164;
  if (t165) {
  t166 = v1;
  t167 = gg_view((void *)"\057\166\061\057\162\145\141\154\164\151\155\145", 12);
  t165 = gg_equal_elements_4(t166, t167);
  }
  t168 = t165;
  if (t168) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(18))))));
  goto gg_done;
  }
  }
  t169 = ((int64_t)(v0));
  t170 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t171 = ((t169) == (t170));
  t172 = t171;
  if (t172) {
  t173 = v1;
  t174 = gg_view((void *)"\057\166\061\057\155\145\163\163\141\147\145\163", 12);
  t172 = gg_equal_elements_4(t173, t174);
  }
  t175 = t172;
  if (t175) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(19))))));
  goto gg_done;
  }
  }
  t176 = ((int64_t)(v0));
  t177 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t178 = ((t176) == (t177));
  t179 = t178;
  if (t179) {
  t180 = v1;
  t181 = gg_view((void *)"\057\151\156\146\151\154\154", 7);
  t179 = gg_equal_elements_4(t180, t181);
  }
  t182 = t179;
  if (t182) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(20))))));
  goto gg_done;
  }
  }
  t183 = ((int64_t)(v0));
  t184 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t185 = ((t183) == (t184));
  t186 = t185;
  if (t186) {
  t187 = v1;
  t188 = gg_view((void *)"\057\145\155\142\145\144\144\151\156\147", 10);
  t186 = gg_equal_elements_4(t187, t188);
  }
  t189 = t186;
  if (t189) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(21))))));
  goto gg_done;
  }
  }
  t190 = ((int64_t)(v0));
  t191 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t192 = ((t190) == (t191));
  t193 = t192;
  if (t193) {
  t194 = v1;
  t195 = gg_view((void *)"\057\145\155\142\145\144\144\151\156\147\163", 11);
  t193 = gg_equal_elements_4(t194, t195);
  }
  t196 = t193;
  if (t196) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(21))))));
  goto gg_done;
  }
  }
  t197 = ((int64_t)(v0));
  t198 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t199 = ((t197) == (t198));
  t200 = t199;
  if (t200) {
  t201 = v1;
  t202 = gg_view((void *)"\057\166\061\057\145\155\142\145\144\144\151\156\147\163", 14);
  t200 = gg_equal_elements_4(t201, t202);
  }
  t203 = t200;
  if (t203) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(22))))));
  goto gg_done;
  }
  }
  t204 = ((int64_t)(v0));
  t205 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t206 = ((t204) == (t205));
  t207 = t206;
  if (t207) {
  t208 = v1;
  t209 = gg_view((void *)"\057\162\145\162\141\156\153", 7);
  t207 = gg_equal_elements_4(t208, t209);
  }
  t210 = t207;
  if (t210) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(23))))));
  goto gg_done;
  }
  }
  t211 = ((int64_t)(v0));
  t212 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t213 = ((t211) == (t212));
  t214 = t213;
  if (t214) {
  t215 = v1;
  t216 = gg_view((void *)"\057\162\145\162\141\156\153\151\156\147", 10);
  t214 = gg_equal_elements_4(t215, t216);
  }
  t217 = t214;
  if (t217) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(23))))));
  goto gg_done;
  }
  }
  t218 = ((int64_t)(v0));
  t219 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t220 = ((t218) == (t219));
  t221 = t220;
  if (t221) {
  t222 = v1;
  t223 = gg_view((void *)"\057\166\061\057\162\145\162\141\156\153", 10);
  t221 = gg_equal_elements_4(t222, t223);
  }
  t224 = t221;
  if (t224) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(23))))));
  goto gg_done;
  }
  }
  t225 = ((int64_t)(v0));
  t226 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t227 = ((t225) == (t226));
  t228 = t227;
  if (t228) {
  t229 = v1;
  t230 = gg_view((void *)"\057\166\061\057\162\145\162\141\156\153\151\156\147", 13);
  t228 = gg_equal_elements_4(t229, t230);
  }
  t231 = t228;
  if (t231) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(23))))));
  goto gg_done;
  }
  }
  t232 = ((int64_t)(v0));
  t233 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t234 = ((t232) == (t233));
  t235 = t234;
  if (t235) {
  t236 = v1;
  t237 = gg_view((void *)"\057\166\061\057\163\171\163\164\145\155\157\156\145", 13);
  t235 = gg_equal_elements_4(t236, t237);
  }
  t238 = t235;
  if (t238) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(24))))));
  goto gg_done;
  }
  }
  t239 = ((int64_t)(v0));
  t240 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t241 = ((t239) == (t240));
  t242 = t241;
  if (t242) {
  t243 = v1;
  t244 = gg_view((void *)"\057\164\157\153\145\156\151\172\145", 9);
  t242 = gg_equal_elements_4(t243, t244);
  }
  t245 = t242;
  if (t245) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(25))))));
  goto gg_done;
  }
  }
  t246 = ((int64_t)(v0));
  t247 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t248 = ((t246) == (t247));
  t249 = t248;
  if (t249) {
  t250 = v1;
  t251 = gg_view((void *)"\057\144\145\164\157\153\145\156\151\172\145", 11);
  t249 = gg_equal_elements_4(t250, t251);
  }
  t252 = t249;
  if (t252) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(26))))));
  goto gg_done;
  }
  }
  t253 = ((int64_t)(v0));
  t254 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t255 = ((t253) == (t254));
  t256 = t255;
  if (t256) {
  t257 = v1;
  t258 = gg_view((void *)"\057\141\160\160\154\171\055\164\145\155\160\154\141\164\145", 15);
  t256 = gg_equal_elements_4(t257, t258);
  }
  t259 = t256;
  if (t259) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(27))))));
  goto gg_done;
  }
  }
  t260 = ((int64_t)(v0));
  t261 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t262 = ((t260) == (t261));
  t263 = t262;
  if (t263) {
  t264 = v1;
  t265 = gg_view((void *)"\057\143\150\141\164\057\143\157\155\160\154\145\164\151\157\156\163\057\151\156\160\165\164\137\164\157\153\145\156\163", 30);
  t263 = gg_equal_elements_4(t264, t265);
  }
  t266 = t263;
  if (t266) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(28))))));
  goto gg_done;
  }
  }
  t267 = ((int64_t)(v0));
  t268 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t269 = ((t267) == (t268));
  t270 = t269;
  if (t270) {
  t271 = v1;
  t272 = gg_view((void *)"\057\166\061\057\143\150\141\164\057\143\157\155\160\154\145\164\151\157\156\163\057\151\156\160\165\164\137\164\157\153\145\156\163", 33);
  t270 = gg_equal_elements_4(t271, t272);
  }
  t273 = t270;
  if (t273) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(28))))));
  goto gg_done;
  }
  }
  t274 = ((int64_t)(v0));
  t275 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t276 = ((t274) == (t275));
  t277 = t276;
  if (t277) {
  t278 = v1;
  t279 = gg_view((void *)"\057\162\145\163\160\157\156\163\145\163\057\151\156\160\165\164\137\164\157\153\145\156\163", 23);
  t277 = gg_equal_elements_4(t278, t279);
  }
  t280 = t277;
  if (t280) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(29))))));
  goto gg_done;
  }
  }
  t281 = ((int64_t)(v0));
  t282 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t283 = ((t281) == (t282));
  t284 = t283;
  if (t284) {
  t285 = v1;
  t286 = gg_view((void *)"\057\166\061\057\162\145\163\160\157\156\163\145\163\057\151\156\160\165\164\137\164\157\153\145\156\163", 26);
  t284 = gg_equal_elements_4(t285, t286);
  }
  t287 = t284;
  if (t287) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(29))))));
  goto gg_done;
  }
  }
  t288 = ((int64_t)(v0));
  t289 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t290 = ((t288) == (t289));
  t291 = t290;
  if (t291) {
  t292 = v1;
  t293 = gg_view((void *)"\057\166\061\057\155\145\163\163\141\147\145\163\057\143\157\165\156\164\137\164\157\153\145\156\163", 25);
  t291 = gg_equal_elements_4(t292, t293);
  }
  t294 = t291;
  if (t294) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(30))))));
  goto gg_done;
  }
  }
  t295 = ((int64_t)(v0));
  t296 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t297 = ((t295) == (t296));
  t298 = t297;
  if (t298) {
  t299 = v1;
  t300 = gg_view((void *)"\057\154\157\162\141\055\141\144\141\160\164\145\162\163", 14);
  t298 = gg_equal_elements_4(t299, t300);
  }
  t301 = t298;
  if (t301) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(31))))));
  goto gg_done;
  }
  }
  t302 = ((int64_t)(v0));
  t303 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t304 = ((t302) == (t303));
  t305 = t304;
  if (t305) {
  t306 = v1;
  t307 = gg_view((void *)"\057\154\157\162\141\055\141\144\141\160\164\145\162\163", 14);
  t305 = gg_equal_elements_4(t306, t307);
  }
  t308 = t305;
  if (t308) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  goto gg_done;
  }
  }
  t309 = ((int64_t)(v0));
  t310 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t311 = ((t309) == (t310));
  t312 = t311;
  if (t312) {
  t313 = v1;
  t314 = gg_view((void *)"\057\163\154\157\164\163", 6);
  t312 = gg_equal_elements_4(t313, t314);
  }
  t315 = t312;
  if (t315) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(33))))));
  goto gg_done;
  }
  }
  t316 = ((int64_t)(v0));
  t317 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t318 = ((t316) == (t317));
  t319 = t318;
  if (t319) {
  t320 = v1;
  t321 = gg_view((void *)"\057\163\154\157\164\163\057\072\151\144\137\163\154\157\164", 15);
  t319 = gg_equal_elements_4(t320, t321);
  }
  t322 = t319;
  if (t322) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(34))))));
  goto gg_done;
  }
  }
  t323 = ((int64_t)(v0));
  t324 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t325 = ((t323) == (t324));
  t326 = t325;
  if (t326) {
  t327 = v1;
  t328 = gg_view((void *)"\057\166\061\057\163\164\162\145\141\155", 10);
  t326 = gg_equal_elements_4(t327, t328);
  }
  t329 = t326;
  if (t329) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(35))))));
  goto gg_done;
  }
  }
  t330 = ((int64_t)(v0));
  t331 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t332 = ((t330) == (t331));
  t333 = t332;
  if (t333) {
  t334 = v1;
  t335 = gg_view((void *)"\057\166\061\057\163\164\162\145\141\155\163\057\154\157\157\153\165\160", 18);
  t333 = gg_equal_elements_4(t334, t335);
  }
  t336 = t333;
  if (t336) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(36))))));
  goto gg_done;
  }
  }
  t337 = ((int64_t)(v0));
  t338 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t339 = ((t337) == (t338));
  t340 = t339;
  if (t340) {
  t341 = v1;
  t342 = gg_view((void *)"\057\166\061\057\163\164\162\145\141\155", 10);
  t340 = gg_equal_elements_4(t341, t342);
  }
  t343 = t340;
  if (t343) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(37))))));
  goto gg_done;
  }
  }
  t344 = ((int64_t)(v0));
  t345 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t346 = ((t344) == (t345));
  t347 = t346;
  if (t347) {
  t348 = v1;
  t349 = gg_view((void *)"\057\143\157\162\163\055\160\162\157\170\171", 11);
  t347 = gg_equal_elements_4(t348, t349);
  }
  t350 = t347;
  if (t350) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(38))))));
  goto gg_done;
  }
  }
  t351 = ((int64_t)(v0));
  t352 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t353 = ((t351) == (t352));
  t354 = t353;
  if (t354) {
  t355 = v1;
  t356 = gg_view((void *)"\057\143\157\162\163\055\160\162\157\170\171", 11);
  t354 = gg_equal_elements_4(t355, t356);
  }
  t357 = t354;
  if (t357) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(38))))));
  goto gg_done;
  }
  }
  t358 = ((int64_t)(v0));
  t359 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t360 = ((t358) == (t359));
  t361 = t360;
  if (t361) {
  t362 = v1;
  t363 = gg_view((void *)"\057\164\157\157\154\163", 6);
  t361 = gg_equal_elements_4(t362, t363);
  }
  t364 = t361;
  if (t364) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(39))))));
  goto gg_done;
  }
  }
  t365 = ((int64_t)(v0));
  t366 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t367 = ((t365) == (t366));
  t368 = t367;
  if (t368) {
  t369 = v1;
  t370 = gg_view((void *)"\057\164\157\157\154\163", 6);
  t368 = gg_equal_elements_4(t369, t370);
  }
  t371 = t368;
  if (t371) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(39))))));
  goto gg_done;
  }
  }
  t372 = ((int64_t)(v0));
  t373 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t374 = ((t372) == (t373));
  if (t374) {
  {
  t375 = v1;
  t376 = gg_view((void *)"\057\163\154\157\164\163\057", 7);
  t377 = gg_function_2(t375, t376, base + 1);
  t378 = t377;
  if (t378) {
  {
  t379 = ((int64_t)((v1).len));
  t380 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  t381 = ((t379) > (t380));
  if (t381) {
  {
  t382 = v1;
  t383 = ((int64_t)UINT64_C(7));
  t384 = gg_sub(t382, t383, t382.len, sizeof(uint8_t));
  t385 = gg_view((void *)"\057", 1);
  t386 = gg_function_3(t384, t385, base + 1);
  t387 = ((int64_t)(t386));
  t388 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t389 = ((t387) < (t388));
  if (t389) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(34))))));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  for (t390 = 0; t390 < 1; t390++) gg_stacks[base + t390].top = 0;
  return t0;
}
static void gg_function_0(int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) gg_slice t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t1 = gg_view((void *)"\057\150\145\141\154\164\150", 7);
  t2 = gg_function_1(t0, t1, base + 0);
  (void)(t2);
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = gg_function_4(t3, base + 0);
  (void)(t4);
  }
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
  (gg_globals.v120).data[0] = ((double)(gg_double_bits(UINT64_C(4607182418800017408))));
  (gg_globals.v120).data[1] = ((double)(gg_double_bits(UINT64_C(4621819117588971520))));
  (gg_globals.v120).data[2] = ((double)(gg_double_bits(UINT64_C(4636737291354636288))));
  (gg_globals.v120).data[3] = ((double)(gg_double_bits(UINT64_C(4652007308841189376))));
  (gg_globals.v120).data[4] = ((double)(gg_double_bits(UINT64_C(4666723172467343360))));
  (gg_globals.v120).data[5] = ((double)(gg_double_bits(UINT64_C(4681608360884174848))));
  (gg_globals.v120).data[6] = ((double)(gg_double_bits(UINT64_C(4696837146684686336))));
  (gg_globals.v120).data[7] = ((double)(gg_double_bits(UINT64_C(4711630319722168320))));
  (gg_globals.v120).data[8] = ((double)(gg_double_bits(UINT64_C(4726483295884279808))));
  (gg_globals.v120).data[9] = ((double)(gg_double_bits(UINT64_C(4741671816366391296))));
  (gg_globals.v120).data[10] = ((double)(gg_double_bits(UINT64_C(4756540486875873280))));
  (gg_globals.v120).data[11] = ((double)(gg_double_bits(UINT64_C(4771362005757984768))));
  (gg_globals.v120).data[12] = ((double)(gg_double_bits(UINT64_C(4786511204640096256))));
  (gg_globals.v120).data[13] = ((double)(gg_double_bits(UINT64_C(4801453603149578240))));
  (gg_globals.v120).data[14] = ((double)(gg_double_bits(UINT64_C(4816244402031689728))));
  (gg_globals.v120).data[15] = ((double)(gg_double_bits(UINT64_C(4831355200913801216))));
  (gg_globals.v120).data[16] = ((double)(gg_double_bits(UINT64_C(4846369599423283200))));
  (gg_globals.v120).data[17] = ((double)(gg_double_bits(UINT64_C(4861130398305394688))));
  (gg_globals.v120).data[18] = ((double)(gg_double_bits(UINT64_C(4876203697187506176))));
  (gg_globals.v120).data[19] = ((double)(gg_double_bits(UINT64_C(4891288408196988160))));
  (gg_globals.v120).data[20] = ((double)(gg_double_bits(UINT64_C(4906019910204099648))));
  (gg_globals.v120).data[21] = ((double)(gg_double_bits(UINT64_C(4921056587992461136))));
  (gg_globals.v120).data[22] = ((double)(gg_double_bits(UINT64_C(4936209963552724370))));
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
