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


typedef int64_t gg_unaligned_1 GG_ALIGN1;
typedef uint64_t gg_unaligned_3 GG_ALIGN1;
typedef float gg_unaligned_4 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_5;
#pragma pack(pop)
typedef double gg_unaligned_6 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { int64_t data[8]; } gg_type_7;
#pragma pack(pop)
typedef uint8_t gg_unaligned_9 GG_ALIGN1;
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(int64_t a, int64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, int64_t value);
static uint8_t gg_eq_2(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint8_t value);
static uint8_t gg_eq_3(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint64_t value);
static uint8_t gg_eq_4(float a, float b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, float value);
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_type_5 value);
static uint8_t gg_eq_6(double a, double b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, double value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_9(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((GG_UNALIGNED gg_unaligned_1*)a.data)[i],((GG_UNALIGNED gg_unaligned_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((GG_UNALIGNED gg_unaligned_3*)a.data)[i],((GG_UNALIGNED gg_unaligned_3*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((GG_UNALIGNED gg_unaligned_4*)a.data)[i],((GG_UNALIGNED gg_unaligned_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(float a, float b) {
return a == b;
}
static void gg_render_4(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_type_5*)a.data)[i],((gg_type_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
return gg_equal_elements_4(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_4(out, ((GG_UNALIGNED gg_unaligned_4*)view.data)[i]);}gg_append(out,"]",1,1);
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
return gg_equal_elements_1(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((GG_UNALIGNED gg_unaligned_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((GG_UNALIGNED gg_unaligned_9*)a.data)[i],((GG_UNALIGNED gg_unaligned_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_9(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static uint8_t gg_function_1(int64_t p0, int64_t base);
static void gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static void gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(v1));
#ifndef __cplusplus
  extern void (fg412k_fail)(int64_t);
#endif
  fg412k_fail(t2);
  }
  }
  }
}
static uint8_t gg_function_1(int64_t p0, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) uint64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) uint64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) float t17;
  _Alignas(8) float t18;
  int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) uint8_t t22;
  _Alignas(8) int64_t t23;
  GG_UNALIGNED gg_unaligned_4 * t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) uint8_t t29;
  _Alignas(8) uint64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) uint64_t t33;
  _Alignas(8) uint64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) uint64_t t37;
  _Alignas(8) uint64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) uint64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) float t45;
  _Alignas(8) float t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) float t48;
  _Alignas(8) float t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  int64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) uint8_t t55;
  _Alignas(8) uint64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) uint64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) uint64_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) int64_t t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) uint64_t t77;
  _Alignas(8) int64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) int64_t t80;
  _Alignas(8) int64_t t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) int64_t t84;
  GG_UNALIGNED gg_unaligned_1 * t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) int64_t t87;
  _Alignas(8) int64_t t88;
  _Alignas(8) int64_t t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  _Alignas(8) int64_t t92;
  _Alignas(8) int64_t t93;
  _Alignas(8) int64_t t94;
  _Alignas(8) int64_t t95;
  _Alignas(8) int64_t t96;
  _Alignas(8) int64_t t97;
  int64_t t98;
  _Alignas(8) int64_t t99;
  _Alignas(8) int64_t t100;
  _Alignas(8) uint8_t t101;
  _Alignas(8) int64_t t102;
  GG_UNALIGNED gg_unaligned_1 * t103;
  _Alignas(8) int64_t t104;
  _Alignas(8) int64_t t105;
  _Alignas(8) int64_t t106;
  _Alignas(8) int64_t t107;
  _Alignas(8) uint8_t t108;
  _Alignas(8) uint64_t t109;
  _Alignas(8) int64_t t110;
  _Alignas(8) int64_t t111;
  _Alignas(8) uint64_t t112;
  _Alignas(8) uint64_t t113;
  _Alignas(8) uint64_t t114;
  _Alignas(8) uint64_t t115;
  _Alignas(8) int64_t t116;
  _Alignas(8) int64_t t117;
  _Alignas(8) int64_t t118;
  _Alignas(8) int64_t t119;
  _Alignas(8) uint64_t t120;
  _Alignas(8) uint64_t t121;
  _Alignas(8) int64_t t122;
  _Alignas(8) int64_t t123;
  _Alignas(8) int64_t t124;
  _Alignas(8) int64_t t125;
  _Alignas(8) uint64_t t126;
  _Alignas(8) int64_t t127;
  _Alignas(8) int64_t t128;
  _Alignas(8) uint8_t t129;
  _Alignas(8) uint64_t t130;
  _Alignas(8) int64_t t131;
  _Alignas(8) int64_t t132;
  _Alignas(8) uint64_t t133;
  _Alignas(8) int64_t t134;
  _Alignas(8) int64_t t135;
  _Alignas(8) uint64_t t136;
  _Alignas(8) int64_t t137;
  _Alignas(8) int64_t t138;
  _Alignas(8) int64_t t139;
  _Alignas(8) int64_t t140;
  _Alignas(8) int64_t t141;
  _Alignas(8) int64_t t142;
  _Alignas(8) int64_t t143;
  _Alignas(8) int64_t t144;
  _Alignas(8) int64_t t145;
  _Alignas(8) int64_t t146;
  _Alignas(8) int64_t t147;
  _Alignas(8) int64_t t148;
  _Alignas(8) int64_t t149;
  _Alignas(8) int64_t t150;
  _Alignas(8) int64_t t151;
  _Alignas(8) int64_t t152;
  _Alignas(8) int64_t t153;
  _Alignas(8) int64_t t154;
  _Alignas(8) int64_t t155;
  _Alignas(8) int64_t t156;
  _Alignas(8) int64_t t157;
  _Alignas(8) int64_t t158;
  _Alignas(8) int64_t t159;
  _Alignas(8) int64_t t160;
  _Alignas(8) int64_t t161;
  _Alignas(8) int64_t t162;
  _Alignas(8) int64_t t163;
  _Alignas(8) int64_t t164;
  _Alignas(8) int64_t t165;
  _Alignas(8) int64_t t166;
  _Alignas(8) int64_t t167;
  _Alignas(8) int64_t t168;
  _Alignas(8) int64_t t169;
  _Alignas(8) int64_t t170;
  _Alignas(8) int64_t t171;
  _Alignas(8) int64_t t172;
  _Alignas(8) int64_t t173;
  _Alignas(8) int64_t t174;
  _Alignas(8) int64_t t175;
  _Alignas(8) int64_t t176;
  _Alignas(8) uint8_t t177;
  _Alignas(8) uint64_t t178;
  _Alignas(8) int64_t t179;
  _Alignas(8) int64_t t180;
  _Alignas(8) int64_t t181;
  _Alignas(8) int64_t t182;
  _Alignas(8) int64_t t183;
  _Alignas(8) int64_t t184;
  _Alignas(8) int64_t t185;
  _Alignas(8) int64_t t186;
  _Alignas(8) int64_t t187;
  _Alignas(8) int64_t t188;
  _Alignas(8) int64_t t189;
  _Alignas(8) int64_t t190;
  _Alignas(8) uint64_t t191;
  _Alignas(8) int64_t t192;
  _Alignas(8) int64_t t193;
  _Alignas(8) int64_t t194;
  _Alignas(8) int64_t t195;
  _Alignas(8) int64_t t196;
  _Alignas(8) int64_t t197;
  _Alignas(8) int64_t t198;
  _Alignas(8) int64_t t199;
  _Alignas(8) int64_t t200;
  _Alignas(8) int64_t t201;
  _Alignas(8) int64_t t202;
  _Alignas(8) int64_t t203;
  _Alignas(8) uint64_t t204;
  _Alignas(8) int64_t t205;
  _Alignas(8) int64_t t206;
  _Alignas(8) int64_t t207;
  _Alignas(8) int64_t t208;
  _Alignas(8) int64_t t209;
  _Alignas(8) int64_t t210;
  _Alignas(8) int64_t t211;
  _Alignas(8) int64_t t212;
  _Alignas(8) int64_t t213;
  _Alignas(8) int64_t t214;
  _Alignas(8) int64_t t215;
  _Alignas(8) int64_t t216;
  _Alignas(8) uint64_t t217;
  _Alignas(8) int64_t t218;
  _Alignas(8) int64_t t219;
  _Alignas(8) int64_t t220;
  _Alignas(8) int64_t t221;
  _Alignas(8) int64_t t222;
  _Alignas(8) int64_t t223;
  _Alignas(8) int64_t t224;
  _Alignas(8) int64_t t225;
  _Alignas(8) int64_t t226;
  _Alignas(8) int64_t t227;
  _Alignas(8) int64_t t228;
  _Alignas(8) int64_t t229;
  _Alignas(8) uint64_t t230;
  _Alignas(8) int64_t t231;
  _Alignas(8) int64_t t232;
  _Alignas(8) uint64_t t233;
  _Alignas(8) int64_t t234;
  _Alignas(8) int64_t t235;
  _Alignas(8) int64_t t236;
  _Alignas(8) int64_t t237;
  _Alignas(8) uint64_t t238;
  _Alignas(8) int64_t t239;
  _Alignas(8) int64_t t240;
  _Alignas(8) int64_t t241;
  _Alignas(8) int64_t t242;
  _Alignas(8) uint64_t t243;
  _Alignas(8) int64_t t244;
  _Alignas(8) int64_t t245;
  _Alignas(8) int64_t t246;
  _Alignas(8) int64_t t247;
  _Alignas(8) int64_t t248;
  GG_UNALIGNED gg_unaligned_1 * t249;
  _Alignas(8) int64_t t250;
  GG_UNALIGNED gg_unaligned_1 * t251;
  _Alignas(8) int64_t t252;
  _Alignas(8) int64_t t253;
  _Alignas(8) int64_t t254;
  _Alignas(8) int64_t t255;
  _Alignas(8) int64_t t256;
  _Alignas(8) int64_t t257;
  _Alignas(8) int64_t t258;
  _Alignas(8) int64_t t259;
  _Alignas(8) int64_t t260;
  _Alignas(8) int64_t t261;
  _Alignas(8) int64_t t262;
  _Alignas(8) int64_t t263;
  _Alignas(8) int64_t t264;
  _Alignas(8) int64_t t265;
  _Alignas(8) int64_t t266;
  _Alignas(8) int64_t t267;
  _Alignas(8) int64_t t268;
  _Alignas(8) int64_t t269;
  _Alignas(8) int64_t t270;
  _Alignas(8) int64_t t271;
  _Alignas(8) int64_t t272;
  _Alignas(8) int64_t t273;
  _Alignas(8) int64_t t274;
  _Alignas(8) int64_t t275;
  _Alignas(8) int64_t t276;
  _Alignas(8) int64_t t277;
  _Alignas(8) int64_t t278;
  _Alignas(8) int64_t t279;
  _Alignas(8) int64_t t280;
  _Alignas(8) int64_t t281;
  _Alignas(8) uint8_t t282;
  _Alignas(8) int64_t t283;
  GG_UNALIGNED gg_unaligned_4 * t284;
  _Alignas(8) float t285;
  _Alignas(8) int64_t t286;
  GG_UNALIGNED gg_unaligned_1 * t287;
  _Alignas(8) int64_t t288;
  _Alignas(8) int64_t t289;
  GG_UNALIGNED gg_unaligned_1 * t290;
  _Alignas(8) int64_t t291;
  _Alignas(8) float t292;
  _Alignas(8) int64_t t293;
  GG_UNALIGNED gg_unaligned_4 * t294;
  _Alignas(8) float t295;
  _Alignas(8) float t296;
  _Alignas(8) int64_t t297;
  _Alignas(8) int64_t t298;
  _Alignas(8) int64_t t299;
  _Alignas(8) int64_t t300;
  _Alignas(8) int64_t t301;
  GG_UNALIGNED gg_unaligned_4 * t302;
  _Alignas(8) float t303;
  _Alignas(8) int64_t t304;
  GG_UNALIGNED gg_unaligned_4 * t305;
  _Alignas(8) float t306;
  _Alignas(8) int64_t t307;
  GG_UNALIGNED gg_unaligned_4 * t308;
  _Alignas(8) float t309;
  _Alignas(8) int64_t t310;
  GG_UNALIGNED gg_unaligned_4 * t311;
  _Alignas(8) float t312;
  _Alignas(8) int64_t t313;
  GG_UNALIGNED gg_unaligned_4 * t314;
  _Alignas(8) float t315;
  _Alignas(8) int64_t t316;
  GG_UNALIGNED gg_unaligned_4 * t317;
  _Alignas(8) float t318;
  _Alignas(8) int64_t t319;
  GG_UNALIGNED gg_unaligned_4 * t320;
  _Alignas(8) float t321;
  _Alignas(8) int64_t t322;
  GG_UNALIGNED gg_unaligned_4 * t323;
  _Alignas(8) float t324;
  _Alignas(8) float t325;
  _Alignas(8) float t326;
  _Alignas(8) float t327;
  _Alignas(8) float t328;
  _Alignas(8) float t329;
  _Alignas(8) float t330;
  _Alignas(8) int64_t t331;
  _Alignas(8) int64_t t332;
  _Alignas(8) int64_t t333;
  _Alignas(8) float t334;
  _Alignas(8) int64_t t335;
  _Alignas(8) uint64_t t336;
  _Alignas(8) int64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) uint64_t v3;
  _Alignas(8) gg_type_5 v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) uint64_t v7;
  _Alignas(8) uint64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) float v10;
  _Alignas(8) gg_type_7 v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) int64_t v14;
  _Alignas(8) int64_t v15;
  _Alignas(8) int64_t v16;
  _Alignas(8) gg_type_7 v17;
  _Alignas(8) int64_t v18;
  _Alignas(8) uint64_t v19;
  _Alignas(8) uint64_t v20;
  _Alignas(8) uint64_t v21;
  _Alignas(8) uint64_t v22;
  _Alignas(8) int64_t v23;
  _Alignas(8) int64_t v24;
  _Alignas(8) int64_t v25;
  _Alignas(8) int64_t v26;
  _Alignas(8) int64_t v27;
  _Alignas(8) int64_t v28;
  _Alignas(8) int64_t v29;
  _Alignas(8) int64_t v30;
  _Alignas(8) int64_t v31;
  _Alignas(8) int64_t v32;
  _Alignas(8) int64_t v33;
  _Alignas(8) int64_t v34;
  _Alignas(8) int64_t v35;
  _Alignas(8) int64_t v36;
  _Alignas(8) int64_t v37;
  _Alignas(8) int64_t v38;
  _Alignas(8) int64_t v39;
  _Alignas(8) int64_t v40;
  _Alignas(8) float v41;
  _Alignas(8) float v42;
  _Alignas(8) float v43;
  _Alignas(8) float v44;
  _Alignas(8) float v45;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg412k_get)(int64_t, int64_t, int64_t);
#endif
  t4 = fg412k_get(t1, t2, t3);
  v1 = ((int64_t)(((int64_t)(t4))));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg412k_get)(int64_t, int64_t, int64_t);
#endif
  t8 = fg412k_get(t5, t6, t7);
  v2 = ((uint64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg412k_get)(int64_t, int64_t, int64_t);
#endif
  t12 = fg412k_get(t9, t10, t11);
  v3 = ((uint64_t)(t12));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) > (t14));
  t16 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_3(t15, t16, base + 0);
  memset(&t17, 0, sizeof(t17));
  t18 = ((float)(t17));
  for (t19=0; t19<8; t19++) (v4).data[t19] = t18;
  v5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t20 = ((int64_t)(v5));
  t21 = ((int64_t)(((int64_t)UINT64_C(8))));
  t22 = ((t20) < (t21));
  if (!(t22)) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t23 = v5;
  t24 = ((GG_UNALIGNED gg_unaligned_4 *)(v4).data) + gg_index(t23, ((int64_t)UINT64_C(8)));
  (*t24) = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  }
  goto label1;
label2:;
  v6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label4:;
  t27 = ((int64_t)(v6));
  t28 = ((int64_t)(v1));
  t29 = ((t27) < (t28));
  if (!(t29)) goto label5;
  {
  t30 = ((uint64_t)(v2));
  t31 = ((int64_t)(v6));
  t32 = ((int64_t)(((int64_t)UINT64_C(210))));
  t33 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))))));
  v7 = ((uint64_t)(((uint64_t)((uint64_t)(t30) + (uint64_t)(t33)))));
  t34 = ((uint64_t)(v3));
  t35 = ((int64_t)(v6));
  t36 = ((int64_t)(((int64_t)UINT64_C(292))));
  t37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))))));
  v8 = ((uint64_t)(((uint64_t)((uint64_t)(t34) + (uint64_t)(t37)))));
  t38 = ((uint64_t)(v7));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(208))))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi16)(uint64_t, int64_t);
#endif
  t40 = fg412k_ldi16(t38, t39);
  t41 = ((int64_t)(t40));
  t42 = ((int64_t)(((int64_t)UINT64_C(65535))));
  v9 = ((int64_t)(((t41) & (t42))));
  t43 = ((uint64_t)(v8));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern float (fg412k_ldf)(uint64_t, int64_t);
#endif
  t45 = fg412k_ldf(t43, t44);
  t46 = ((float)(t45));
  t47 = ((int64_t)(v9));
#ifndef __cplusplus
  extern float (fg412k_f16f32)(int64_t);
#endif
  t48 = fg412k_f16f32(t47);
  t49 = ((float)(t48));
  v10 = ((float)(((t46) * (t49))));
  memset(&t50, 0, sizeof(t50));
  t51 = ((int64_t)(t50));
  for (t52=0; t52<8; t52++) (v11).data[t52] = t51;
  v12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label7:;
  t53 = ((int64_t)(v12));
  t54 = ((int64_t)(((int64_t)UINT64_C(8))));
  t55 = ((t53) < (t54));
  if (!(t55)) goto label8;
  {
  t56 = ((uint64_t)(v8));
  t57 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(260))))));
  t58 = ((int64_t)(v12));
  t59 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t60)))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi16)(uint64_t, int64_t);
#endif
  t62 = fg412k_ldi16(t56, t61);
  v13 = ((int64_t)(t62));
  t63 = ((uint64_t)(v8));
  t64 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(262))))));
  t65 = ((int64_t)(v12));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t67)))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi16)(uint64_t, int64_t);
#endif
  t69 = fg412k_ldi16(t63, t68);
  v14 = ((int64_t)(t69));
  t70 = ((uint64_t)(v7));
  t71 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(192))))));
  t72 = ((int64_t)(v12));
  t73 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t74)))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi8)(uint64_t, int64_t);
#endif
  t76 = fg412k_ldi8(t70, t75);
  v15 = ((int64_t)(t76));
  t77 = ((uint64_t)(v7));
  t78 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(193))))));
  t79 = ((int64_t)(v12));
  t80 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t81)))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi8)(uint64_t, int64_t);
#endif
  t83 = fg412k_ldi8(t77, t82);
  v16 = ((int64_t)(t83));
  /* FG_DIRECT_SLICE_INDEX */
  t84 = v12;
  t85 = ((GG_UNALIGNED gg_unaligned_1 *)(v11).data) + gg_index(t84, ((int64_t)UINT64_C(8)));
  t86 = ((int64_t)(v13));
  t87 = ((int64_t)(v15));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t87)))));
  t89 = ((int64_t)(v14));
  t90 = ((int64_t)(v16));
  t91 = ((int64_t)(((int64_t)((uint64_t)(t89) * (uint64_t)(t90)))));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t91)))));
  t93 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  (*t85) = ((int64_t)(((int64_t)((uint64_t)(t92) << ((uint64_t)(t93) & 63)))));
  t94 = ((int64_t)(v12));
  t95 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t94) + (uint64_t)(t95)))));
  }
  goto label7;
label8:;
  memset(&t96, 0, sizeof(t96));
  t97 = ((int64_t)(t96));
  for (t98=0; t98<8; t98++) (v17).data[t98] = t97;
  v12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label10:;
  t99 = ((int64_t)(v12));
  t100 = ((int64_t)(((int64_t)UINT64_C(8))));
  t101 = ((t99) < (t100));
  if (!(t101)) goto label11;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t102 = v12;
  t103 = ((GG_UNALIGNED gg_unaligned_1 *)(v17).data) + gg_index(t102, ((int64_t)UINT64_C(8)));
  (*t103) = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t104 = ((int64_t)(v12));
  t105 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t104) + (uint64_t)(t105)))));
  }
  goto label10;
label11:;
  v18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label13:;
  t106 = ((int64_t)(v18));
  t107 = ((int64_t)(((int64_t)UINT64_C(2))));
  t108 = ((t106) < (t107));
  if (!(t108)) goto label14;
  {
  t109 = ((uint64_t)(v7));
  t110 = ((int64_t)(v18));
  t111 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t112 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t110) * (uint64_t)(t111)))))));
  v19 = ((uint64_t)(((uint64_t)((uint64_t)(t109) + (uint64_t)(t112)))));
  t113 = ((uint64_t)(v19));
  t114 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32))))));
  v20 = ((uint64_t)(((uint64_t)((uint64_t)(t113) + (uint64_t)(t114)))));
  t115 = ((uint64_t)(v7));
  t116 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(128))))));
  t117 = ((int64_t)(v18));
  t118 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) * (uint64_t)(t118)))));
  t120 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t116) + (uint64_t)(t119)))))));
  v21 = ((uint64_t)(((uint64_t)((uint64_t)(t115) + (uint64_t)(t120)))));
  t121 = ((uint64_t)(v8));
  t122 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t123 = ((int64_t)(v18));
  t124 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(128))))));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) * (uint64_t)(t124)))));
  t126 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t122) + (uint64_t)(t125)))))));
  v22 = ((uint64_t)(((uint64_t)((uint64_t)(t121) + (uint64_t)(t126)))));
  v23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label16:;
  t127 = ((int64_t)(v23));
  t128 = ((int64_t)(((int64_t)UINT64_C(32))));
  t129 = ((t127) < (t128));
  if (!(t129)) goto label17;
  {
  t130 = ((uint64_t)(v19));
  t131 = ((int64_t)(v23));
#ifndef __cplusplus
  extern int64_t (fg412k_ldu8)(uint64_t, int64_t);
#endif
  t132 = fg412k_ldu8(t130, t131);
  v24 = ((int64_t)(t132));
  t133 = ((uint64_t)(v20));
  t134 = ((int64_t)(v23));
#ifndef __cplusplus
  extern int64_t (fg412k_ldu8)(uint64_t, int64_t);
#endif
  t135 = fg412k_ldu8(t133, t134);
  v25 = ((int64_t)(t135));
  t136 = ((uint64_t)(v21));
  t137 = ((int64_t)(v23));
#ifndef __cplusplus
  extern int64_t (fg412k_ldu8)(uint64_t, int64_t);
#endif
  t138 = fg412k_ldu8(t136, t137);
  v26 = ((int64_t)(t138));
  t139 = ((int64_t)(v24));
  t140 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(15))))));
  t141 = ((int64_t)(((t139) & (t140))));
  t142 = ((int64_t)(v26));
  t143 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t144 = ((int64_t)(((t142) & (t143))));
  t145 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t144) << ((uint64_t)(t145) & 63)))));
  v27 = ((int64_t)(((t141) | (t146))));
  t147 = ((int64_t)(v25));
  t148 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(15))))));
  t149 = ((int64_t)(((t147) & (t148))));
  t150 = ((int64_t)(v26));
  t151 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  t152 = ((int64_t)(((t150) & (t151))));
  t153 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t152) << ((uint64_t)(t153) & 63)))));
  v28 = ((int64_t)(((t149) | (t154))));
  t155 = ((int64_t)(v24));
  t156 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t157 = ((int64_t)(((t155) >> ((uint64_t)(t156) & 63))));
  t158 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(15))))));
  t159 = ((int64_t)(((t157) & (t158))));
  t160 = ((int64_t)(v26));
  t161 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(48))))));
  t162 = ((int64_t)(((t160) & (t161))));
  v29 = ((int64_t)(((t159) | (t162))));
  t163 = ((int64_t)(v25));
  t164 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t165 = ((int64_t)(((t163) >> ((uint64_t)(t164) & 63))));
  t166 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(15))))));
  t167 = ((int64_t)(((t165) & (t166))));
  t168 = ((int64_t)(v26));
  t169 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(192))))));
  t170 = ((int64_t)(((t168) & (t169))));
  t171 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t172 = ((int64_t)(((t170) >> ((uint64_t)(t171) & 63))));
  v30 = ((int64_t)(((t167) | (t172))));
  t173 = ((int64_t)(v23));
  t174 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v31 = ((int64_t)(((int64_t)gg_div(t173, t174))));
  t175 = ((int64_t)(v31));
  t176 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t177 = ((t175) >= (t176));
  if (t177) {
  {
  v32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  v32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t178 = ((uint64_t)(v7));
  t179 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(192))))));
  t180 = ((int64_t)(v18));
  t181 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t182 = ((int64_t)(((int64_t)((uint64_t)(t180) * (uint64_t)(t181)))));
  t183 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t184 = ((int64_t)(((int64_t)((uint64_t)(t182) + (uint64_t)(t183)))));
  t185 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t186 = ((int64_t)(((int64_t)((uint64_t)(t184) * (uint64_t)(t185)))));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t179) + (uint64_t)(t186)))));
  t188 = ((int64_t)(v32));
  t189 = ((int64_t)(((int64_t)((uint64_t)(t187) + (uint64_t)(t188)))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi8)(uint64_t, int64_t);
#endif
  t190 = fg412k_ldi8(t178, t189);
  v33 = ((int64_t)(t190));
  t191 = ((uint64_t)(v7));
  t192 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(192))))));
  t193 = ((int64_t)(v18));
  t194 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t195 = ((int64_t)(((int64_t)((uint64_t)(t193) * (uint64_t)(t194)))));
  t196 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t197 = ((int64_t)(((int64_t)((uint64_t)(t195) + (uint64_t)(t196)))));
  t198 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t199 = ((int64_t)(((int64_t)((uint64_t)(t197) * (uint64_t)(t198)))));
  t200 = ((int64_t)(((int64_t)((uint64_t)(t192) + (uint64_t)(t199)))));
  t201 = ((int64_t)(v32));
  t202 = ((int64_t)(((int64_t)((uint64_t)(t200) + (uint64_t)(t201)))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi8)(uint64_t, int64_t);
#endif
  t203 = fg412k_ldi8(t191, t202);
  v34 = ((int64_t)(t203));
  t204 = ((uint64_t)(v7));
  t205 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(192))))));
  t206 = ((int64_t)(v18));
  t207 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t208 = ((int64_t)(((int64_t)((uint64_t)(t206) * (uint64_t)(t207)))));
  t209 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t210 = ((int64_t)(((int64_t)((uint64_t)(t208) + (uint64_t)(t209)))));
  t211 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t210) * (uint64_t)(t211)))));
  t213 = ((int64_t)(((int64_t)((uint64_t)(t205) + (uint64_t)(t212)))));
  t214 = ((int64_t)(v32));
  t215 = ((int64_t)(((int64_t)((uint64_t)(t213) + (uint64_t)(t214)))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi8)(uint64_t, int64_t);
#endif
  t216 = fg412k_ldi8(t204, t215);
  v35 = ((int64_t)(t216));
  t217 = ((uint64_t)(v7));
  t218 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(192))))));
  t219 = ((int64_t)(v18));
  t220 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t221 = ((int64_t)(((int64_t)((uint64_t)(t219) * (uint64_t)(t220)))));
  t222 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t223 = ((int64_t)(((int64_t)((uint64_t)(t221) + (uint64_t)(t222)))));
  t224 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t225 = ((int64_t)(((int64_t)((uint64_t)(t223) * (uint64_t)(t224)))));
  t226 = ((int64_t)(((int64_t)((uint64_t)(t218) + (uint64_t)(t225)))));
  t227 = ((int64_t)(v32));
  t228 = ((int64_t)(((int64_t)((uint64_t)(t226) + (uint64_t)(t227)))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi8)(uint64_t, int64_t);
#endif
  t229 = fg412k_ldi8(t217, t228);
  v36 = ((int64_t)(t229));
  t230 = ((uint64_t)(v22));
  t231 = ((int64_t)(v23));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi8)(uint64_t, int64_t);
#endif
  t232 = fg412k_ldi8(t230, t231);
  v37 = ((int64_t)(((int64_t)(t232))));
  t233 = ((uint64_t)(v22));
  t234 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t235 = ((int64_t)(v23));
  t236 = ((int64_t)(((int64_t)((uint64_t)(t234) + (uint64_t)(t235)))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi8)(uint64_t, int64_t);
#endif
  t237 = fg412k_ldi8(t233, t236);
  v38 = ((int64_t)(((int64_t)(t237))));
  t238 = ((uint64_t)(v22));
  t239 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t240 = ((int64_t)(v23));
  t241 = ((int64_t)(((int64_t)((uint64_t)(t239) + (uint64_t)(t240)))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi8)(uint64_t, int64_t);
#endif
  t242 = fg412k_ldi8(t238, t241);
  v39 = ((int64_t)(((int64_t)(t242))));
  t243 = ((uint64_t)(v22));
  t244 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(96))))));
  t245 = ((int64_t)(v23));
  t246 = ((int64_t)(((int64_t)((uint64_t)(t244) + (uint64_t)(t245)))));
#ifndef __cplusplus
  extern int64_t (fg412k_ldi8)(uint64_t, int64_t);
#endif
  t247 = fg412k_ldi8(t243, t246);
  v40 = ((int64_t)(((int64_t)(t247))));
  /* FG_DIRECT_SLICE_INDEX */
  t248 = v31;
  t249 = ((GG_UNALIGNED gg_unaligned_1 *)(v17).data) + gg_index(t248, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t250 = v31;
  t251 = ((GG_UNALIGNED gg_unaligned_1 *)(v17).data) + gg_index(t250, ((int64_t)UINT64_C(8)));
  t252 = ((int64_t)((*t251)));
  t253 = ((int64_t)(v33));
  t254 = ((int64_t)(v27));
  t255 = ((int64_t)(((int64_t)((uint64_t)(t253) * (uint64_t)(t254)))));
  t256 = ((int64_t)(v37));
  t257 = ((int64_t)(((int64_t)((uint64_t)(t255) * (uint64_t)(t256)))));
  t258 = ((int64_t)(((int64_t)((uint64_t)(t252) + (uint64_t)(t257)))));
  t259 = ((int64_t)(v34));
  t260 = ((int64_t)(v28));
  t261 = ((int64_t)(((int64_t)((uint64_t)(t259) * (uint64_t)(t260)))));
  t262 = ((int64_t)(v38));
  t263 = ((int64_t)(((int64_t)((uint64_t)(t261) * (uint64_t)(t262)))));
  t264 = ((int64_t)(((int64_t)((uint64_t)(t258) + (uint64_t)(t263)))));
  t265 = ((int64_t)(v35));
  t266 = ((int64_t)(v29));
  t267 = ((int64_t)(((int64_t)((uint64_t)(t265) * (uint64_t)(t266)))));
  t268 = ((int64_t)(v39));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t267) * (uint64_t)(t268)))));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t264) + (uint64_t)(t269)))));
  t271 = ((int64_t)(v36));
  t272 = ((int64_t)(v30));
  t273 = ((int64_t)(((int64_t)((uint64_t)(t271) * (uint64_t)(t272)))));
  t274 = ((int64_t)(v40));
  t275 = ((int64_t)(((int64_t)((uint64_t)(t273) * (uint64_t)(t274)))));
  (*t249) = ((int64_t)(((int64_t)((uint64_t)(t270) + (uint64_t)(t275)))));
  t276 = ((int64_t)(v23));
  t277 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t276) + (uint64_t)(t277)))));
  }
  goto label16;
label17:;
  t278 = ((int64_t)(v18));
  t279 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t278) + (uint64_t)(t279)))));
  }
  goto label13;
label14:;
  v12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label19:;
  t280 = ((int64_t)(v12));
  t281 = ((int64_t)(((int64_t)UINT64_C(8))));
  t282 = ((t280) < (t281));
  if (!(t282)) goto label20;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t283 = v12;
  t284 = ((GG_UNALIGNED gg_unaligned_4 *)(v4).data) + gg_index(t283, ((int64_t)UINT64_C(8)));
  t285 = ((float)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t286 = v12;
  t287 = ((GG_UNALIGNED gg_unaligned_1 *)(v17).data) + gg_index(t286, ((int64_t)UINT64_C(8)));
  t288 = ((int64_t)((*t287)));
  /* FG_DIRECT_SLICE_INDEX */
  t289 = v12;
  t290 = ((GG_UNALIGNED gg_unaligned_1 *)(v11).data) + gg_index(t289, ((int64_t)UINT64_C(8)));
  t291 = ((int64_t)((*t290)));
  t292 = ((float)(((float)(((int64_t)((uint64_t)(t288) - (uint64_t)(t291)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t293 = v12;
  t294 = ((GG_UNALIGNED gg_unaligned_4 *)(v4).data) + gg_index(t293, ((int64_t)UINT64_C(8)));
  t295 = ((float)((*t294)));
#ifndef __cplusplus
  extern float (fg412k_fma)(float, float, float);
#endif
  t296 = fg412k_fma(t285, t292, t295);
  (*t284) = ((float)(t296));
  t297 = ((int64_t)(v12));
  t298 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t297) + (uint64_t)(t298)))));
  }
  goto label19;
label20:;
  t299 = ((int64_t)(v6));
  t300 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t299) + (uint64_t)(t300)))));
  }
  goto label4;
label5:;
  /* FG_DIRECT_SLICE_INDEX */
  t301 = ((int64_t)UINT64_C(4));
  t302 = ((GG_UNALIGNED gg_unaligned_4 *)(v4).data) + gg_index(t301, ((int64_t)UINT64_C(8)));
  t303 = ((float)((*t302)));
  /* FG_DIRECT_SLICE_INDEX */
  t304 = ((int64_t)UINT64_C(0));
  t305 = ((GG_UNALIGNED gg_unaligned_4 *)(v4).data) + gg_index(t304, ((int64_t)UINT64_C(8)));
  t306 = ((float)((*t305)));
  v41 = ((float)(((t303) + (t306))));
  /* FG_DIRECT_SLICE_INDEX */
  t307 = ((int64_t)UINT64_C(5));
  t308 = ((GG_UNALIGNED gg_unaligned_4 *)(v4).data) + gg_index(t307, ((int64_t)UINT64_C(8)));
  t309 = ((float)((*t308)));
  /* FG_DIRECT_SLICE_INDEX */
  t310 = ((int64_t)UINT64_C(1));
  t311 = ((GG_UNALIGNED gg_unaligned_4 *)(v4).data) + gg_index(t310, ((int64_t)UINT64_C(8)));
  t312 = ((float)((*t311)));
  v42 = ((float)(((t309) + (t312))));
  /* FG_DIRECT_SLICE_INDEX */
  t313 = ((int64_t)UINT64_C(6));
  t314 = ((GG_UNALIGNED gg_unaligned_4 *)(v4).data) + gg_index(t313, ((int64_t)UINT64_C(8)));
  t315 = ((float)((*t314)));
  /* FG_DIRECT_SLICE_INDEX */
  t316 = ((int64_t)UINT64_C(2));
  t317 = ((GG_UNALIGNED gg_unaligned_4 *)(v4).data) + gg_index(t316, ((int64_t)UINT64_C(8)));
  t318 = ((float)((*t317)));
  v43 = ((float)(((t315) + (t318))));
  /* FG_DIRECT_SLICE_INDEX */
  t319 = ((int64_t)UINT64_C(7));
  t320 = ((GG_UNALIGNED gg_unaligned_4 *)(v4).data) + gg_index(t319, ((int64_t)UINT64_C(8)));
  t321 = ((float)((*t320)));
  /* FG_DIRECT_SLICE_INDEX */
  t322 = ((int64_t)UINT64_C(3));
  t323 = ((GG_UNALIGNED gg_unaligned_4 *)(v4).data) + gg_index(t322, ((int64_t)UINT64_C(8)));
  t324 = ((float)((*t323)));
  v44 = ((float)(((t321) + (t324))));
  t325 = ((float)(v41));
  t326 = ((float)(v43));
  t327 = ((float)(((t325) + (t326))));
  t328 = ((float)(v42));
  t329 = ((float)(v44));
  t330 = ((float)(((t328) + (t329))));
  v45 = ((float)(((t327) + (t330))));
  t331 = ((int64_t)(v0));
  t332 = ((int64_t)(((int64_t)UINT64_C(2))));
  t333 = ((int64_t)(((int64_t)UINT64_C(0))));
  t334 = ((float)(v45));
#ifndef __cplusplus
  extern int64_t (fg412k_f32bits)(float);
#endif
  t335 = fg412k_f32bits(t334);
  t336 = ((uint64_t)(((uint64_t)(t335))));
#ifndef __cplusplus
  extern void (fg412k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg412k_put(t331, t332, t333, t336);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) uint8_t t1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = gg_function_1(t0, base + 0);
  (void)(t1);
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
