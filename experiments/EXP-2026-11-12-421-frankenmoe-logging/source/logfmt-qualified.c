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
typedef struct { void *data; int64_t len; } fglog_slice;
typedef struct { unsigned char *base; int64_t len; } fglog_header;
typedef struct { unsigned char *base; size_t top, committed; } fglog_stack;
typedef struct { fglog_header *header; fglog_stack *stack; } fglog_reference;
#ifndef GG_STACK_RESERVE
#define GG_STACK_RESERVE 268435456ULL
#endif
#if GG_STACK_RESERVE < 65536 || GG_STACK_RESERVE % 65536 != 0
#error GG_STACK_RESERVE_must_be_a_positive_multiple_of_65536
#endif
typedef char fglog_reservation_size_fits[(GG_STACK_RESERVE <= SIZE_MAX - 65536) ? 1 : -1];
#define GG_RESERVATION ((size_t)GG_STACK_RESERVE)
#define GG_STACK_COUNT 8192
static fglog_stack fglog_stacks[GG_STACK_COUNT];
static int fglog_argc;
static char **fglog_argv;
static void fglog_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fglog_stack *fglog_get_stack(int64_t id) {
    fglog_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fglog_fail("data stack limit exceeded");
    s = &fglog_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fglog_fail("cannot reserve data stack");
    }
    return s;
}
/* The stack owning an array region, found from the header's data base.
   The header may be an unaligned field of a packed struct, so the base is
   read by memcpy and never through a fglog_header lvalue. */
static fglog_stack *fglog_stack_of(const void *header) {
    unsigned char *base; uintptr_t q; int64_t i;
    memcpy(&base, header, sizeof base); q = (uintptr_t)base;
    for (i = 0; i < GG_STACK_COUNT; i++) {
        fglog_stack *s = &fglog_stacks[i];
        if (s->base && q >= (uintptr_t)s->base && q < (uintptr_t)s->base + GG_RESERVATION) return s;
    }
    fglog_fail("array data is not in a data stack"); return 0;
}
/* A pool-relative reference narrowed to a smaller offset width: refuse a
   value that would not survive the store instead of truncating it. */
static uint64_t fglog_ref_offset_fits(uint64_t v, uint64_t max) {
    if (v > max) fglog_fail("relative reference offset overflow");
    return v;
}
/* ULEB128 length prefix of a serialization image (spec 3.6). */
static int64_t fglog_uleb(uint8_t *b, uint64_t n) {
    int64_t k = 0;
    do { uint8_t c = (uint8_t)(n & 0x7f); n >>= 7; if (n) c |= 0x80; b[k++] = c; } while (n);
    return k;
}
/* Bounded ULEB128 read (spec 3.6): 1 and the value, or 0 when the bytes
   are truncated or past 64 bits. */
static int fglog_uleb_read(const uint8_t *p, int64_t n, int64_t *at, uint64_t *out) {
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
static void *fglog_allocate(fglog_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fglog_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fglog_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fglog_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fglog_reference fglog_ref(fglog_header *h, fglog_stack *s) { fglog_reference r; r.header=h; r.stack=s; return r; }
static fglog_slice fglog_view(void *p, int64_t n) { fglog_slice v; v.data=p; v.len=n; return v; }
static fglog_slice fglog_byte_view(fglog_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fglog_fail("byte view size overflow");
    return fglog_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fglog_slice fglog_sub(fglog_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fglog_fail("slice out of bounds");
    return fglog_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fglog_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fglog_fail("array index out of bounds"); return i; }
static void fglog_append(fglog_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fglog_fail("invalid array size");
    to=fglog_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fglog_resize(fglog_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fglog_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fglog_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fglog_fail("invalid array size");
        p=fglog_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fglog_div(int64_t a, int64_t b) { if (!b) fglog_fail("division by zero"); if (a==INT64_MIN && b==-1) fglog_fail("division overflow"); return a/b; }
static int64_t fglog_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fglog_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fglog_udiv(uint64_t a,uint64_t b) { if(!b)fglog_fail("division by zero");return a/b; }
static uint64_t fglog_umod(uint64_t a,uint64_t b) { if(!b)fglog_fail("division by zero");return a%b; }
static int fglog_equal(fglog_slice a, fglog_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fglog_text(fglog_reference out, fglog_slice v) { fglog_append(out,v.data,v.len,1); }
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

static void fglog_integer(fglog_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fglog_append(out,b,k,1); }
static void fglog_unsigned(fglog_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fglog_append(out,b,k,1); }
static double fglog_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t ggf_fmt_f64(uint8_t *out,double n);
static int64_t ggf_fmt_f32(uint8_t *out,float n);
/* Only the in-range fast path uses a C float-to-integer cast. The tail
   projects the finite IEEE significand modulo 2^64 without UB or libm.
   Nonfinite unchecked inputs produce zero; checked inputs reject them. */
static uint64_t fglog_float_integer_wrap(double d) {
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
static void fglog_float_integer_fail(double d,int f32,const char *type,const char *file,int line,const char *why) {
    uint8_t text[64];int64_t n=f32?ggf_fmt_f32(text,(float)d):ggf_fmt_f64(text,d);
    fprintf(stderr,"goose runtime error: conversion %s to %s (%.*s) at %s:%d\\n",why,type,(int)n,(const char*)text,file,line);
    exit(1);
}
#endif
static uint64_t fglog_float_integer(double d,int width,int unsign,int checked,int f32,const char *type,const char *file,int line) {
#if defined(GS_DEBUG) && GS_DEBUG
    if(checked) {
        int64_t value,lo,hi;
        if(unsign && width==64) {
            uint64_t u;
            if(!(d>=0.0 && d<18446744073709551616.0))
                fglog_float_integer_fail(d,f32,type,file,line,"out of range");
            u=(uint64_t)d;
            if((double)u!=d)fglog_float_integer_fail(d,f32,type,file,line,"changes the value");
            return u;
        }
        if(!(d>=-9223372036854775808.0 && d<9223372036854775808.0))
            fglog_float_integer_fail(d,f32,type,file,line,"out of range");
        value=(int64_t)d;
        hi=(int64_t)((UINT64_C(1)<<(width-(unsign?0:1)))-1);
        lo=unsign?0:-1-hi;
        if(value<lo || value>hi)fglog_float_integer_fail(d,f32,type,file,line,"out of range");
        if((double)value!=d)fglog_float_integer_fail(d,f32,type,file,line,"changes the value");
        return (uint64_t)value;
    }
#else
    (void)width;(void)unsign;(void)checked;(void)f32;(void)type;(void)file;(void)line;
#endif
    return fglog_float_integer_wrap(d);
}
static void fglog_float(fglog_reference out,double n) { uint8_t b[64];int64_t k=ggf_fmt_f64(b,n);fglog_append(out,b,k,1); }
static void fglog_float32(fglog_reference out,float n) { uint8_t b[64];int64_t k=ggf_fmt_f32(b,n);fglog_append(out,b,k,1); }
static void fglog_bool(fglog_reference out,uint8_t n) { fglog_append(out,n?"true":"false",n?4:5,1); }
static void fglog_write(FILE *file,fglog_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fglog_fail("write failed"); }
static char *fglog_cstring(fglog_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fglog_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static int fglog_os_path_ok(fglog_slice p) { int64_t i; if(p.len<0||p.len>=4096)return 0; for(i=0;i<p.len;i++)if(((const unsigned char*)p.data)[i]==0)return 0; return 1; }
static uint8_t gs_os_read_file(fglog_slice path,fglog_reference out) {
    if(!fglog_os_path_ok(path))return 0;
    char *p=fglog_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fglog_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t gs_os_write_file(fglog_slice path,fglog_slice data) {
    if(!fglog_os_path_ok(path))return 0;
    char *p=fglog_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t gs_os_file_exists(fglog_slice path) { if(!fglog_os_path_ok(path))return 0; char*p=fglog_cstring(path);FILE*f=fopen(p,"rb");int bad;free(p);if(!f)return 0;{char b;if(fread(&b,1,1,f)==0){}bad=ferror(f);}fclose(f);return bad?0:1; }
static void gs_os_write_stdout(fglog_slice s) { fglog_write(stdout,s); }
static void gs_os_write_stderr(fglog_slice s) { fglog_write(stderr,s); }
static void gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t gs_os_arg_count(void) { return fglog_argc; }
static void gs_os_arg(int64_t i,fglog_reference out) { if(i>=0 && i<fglog_argc)fglog_append(out,fglog_argv[i],(int64_t)strlen(fglog_argv[i]),1); }
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


typedef uint64_t fglog_unaligned_1 GG_ALIGN1;
typedef int64_t fglog_unaligned_2 GG_ALIGN1;
typedef uint8_t fglog_unaligned_3 GG_ALIGN1;
typedef fglog_slice fglog_unaligned_6 GG_ALIGN1;
typedef fglog_slice fglog_unaligned_7 GG_ALIGN1;
static void fglog_quote(fglog_reference out,fglog_slice text) { int64_t i; unsigned char c; fglog_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fglog_append(out,"\\",1,1);fglog_append(out,&c,1,1);} else if(c=='\n')fglog_append(out,"\\n",2,1);else if(c=='\r')fglog_append(out,"\\r",2,1);else if(c=='\t')fglog_append(out,"\\t",2,1);else fglog_append(out,&c,1,1);}fglog_append(out,"\"",1,1);}
static uint8_t fglog_eq_1(uint64_t a, uint64_t b);
static uint8_t fglog_equal_elements_1(fglog_slice a, fglog_slice b);
static void fglog_render_1(fglog_reference out, uint64_t value);
static uint8_t fglog_eq_2(int64_t a, int64_t b);
static uint8_t fglog_equal_elements_2(fglog_slice a, fglog_slice b);
static void fglog_render_2(fglog_reference out, int64_t value);
static uint8_t fglog_eq_3(uint8_t a, uint8_t b);
static uint8_t fglog_equal_elements_3(fglog_slice a, fglog_slice b);
static void fglog_render_3(fglog_reference out, uint8_t value);
static uint8_t fglog_eq_4(uint8_t a, uint8_t b);
static uint8_t fglog_equal_elements_4(fglog_slice a, fglog_slice b);
static void fglog_render_4(fglog_reference out, uint8_t value);
static uint8_t fglog_eq_6(fglog_slice a, fglog_slice b);
static uint8_t fglog_equal_elements_6(fglog_slice a, fglog_slice b);
static void fglog_render_6(fglog_reference out, fglog_slice value);
static uint8_t fglog_eq_7(fglog_slice a, fglog_slice b);
static uint8_t fglog_equal_elements_7(fglog_slice a, fglog_slice b);
static void fglog_render_7(fglog_reference out, fglog_slice value);
static uint8_t fglog_equal_elements_1(fglog_slice a,fglog_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fglog_eq_1(((GG_UNALIGNED fglog_unaligned_1*)a.data)[i],((GG_UNALIGNED fglog_unaligned_1*)b.data)[i]))return 0;return 1;}
static uint8_t fglog_eq_1(uint64_t a, uint64_t b) {
return a == b;
}
static void fglog_render_1(fglog_reference out, uint64_t value) {
fglog_unsigned(out,value);
}
static uint8_t fglog_equal_elements_2(fglog_slice a,fglog_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fglog_eq_2(((GG_UNALIGNED fglog_unaligned_2*)a.data)[i],((GG_UNALIGNED fglog_unaligned_2*)b.data)[i]))return 0;return 1;}
static uint8_t fglog_eq_2(int64_t a, int64_t b) {
return a == b;
}
static void fglog_render_2(fglog_reference out, int64_t value) {
fglog_integer(out,value);
}
static uint8_t fglog_equal_elements_3(fglog_slice a,fglog_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fglog_eq_3(((GG_UNALIGNED fglog_unaligned_3*)a.data)[i],((GG_UNALIGNED fglog_unaligned_3*)b.data)[i]))return 0;return 1;}
static uint8_t fglog_eq_3(uint8_t a, uint8_t b) {
return a == b;
}
static void fglog_render_3(fglog_reference out, uint8_t value) {
fglog_unsigned(out,value);
}
static uint8_t fglog_equal_elements_4(fglog_slice a,fglog_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fglog_eq_4(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fglog_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void fglog_render_4(fglog_reference out, uint8_t value) {
fglog_bool(out,value);
}
static uint8_t fglog_equal_elements_6(fglog_slice a,fglog_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fglog_eq_6(((GG_UNALIGNED fglog_unaligned_6*)a.data)[i],((GG_UNALIGNED fglog_unaligned_6*)b.data)[i]))return 0;return 1;}
static uint8_t fglog_eq_6(fglog_slice a, fglog_slice b) {
return fglog_equal_elements_3(a,b);
}
static void fglog_render_6(fglog_reference out, fglog_slice value) {
fglog_text(out,value);
}
static uint8_t fglog_equal_elements_7(fglog_slice a,fglog_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fglog_eq_7(((GG_UNALIGNED fglog_unaligned_7*)a.data)[i],((GG_UNALIGNED fglog_unaligned_7*)b.data)[i]))return 0;return 1;}
static uint8_t fglog_eq_7(fglog_slice a, fglog_slice b) {
return fglog_equal_elements_3(a,b);
}
static void fglog_render_7(fglog_reference out, fglog_slice value) {
fglog_text(out,value);
}
static struct {
  unsigned char dummy;
} fglog_globals;
static void fglog_function_0(int64_t base);
static int64_t fglog_function_1(uint64_t p0, int64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, uint64_t p6, int64_t p7, uint64_t p8, int64_t p9, int64_t base);
static int64_t fglog_function_2(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint8_t fglog_function_3(uint64_t p0, int64_t p1, fglog_slice p2, int64_t base);
static uint8_t fglog_function_4(uint64_t p0, int64_t p1, int64_t p2, fglog_slice p3, int64_t base);
static int64_t fglog_function_7(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t p4, int64_t base);
static int64_t fglog_function_8(uint64_t p0, int64_t p1, int64_t p2, uint8_t p3, int64_t base);
static int64_t fglog_function_10(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t fglog_function_11(uint64_t p0, int64_t p1, int64_t p2, fglog_slice p3, int64_t base);
static fglog_slice fglog_function_12(int64_t p0, int64_t base);
static fglog_slice fglog_function_13(int64_t p0, int64_t base);
static fglog_slice fglog_function_14(int64_t p0, int64_t base);
static int64_t fglog_function_15(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t p4, int64_t base);
static int64_t fglog_function_8(uint64_t p0, int64_t p1, int64_t p2, uint8_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint64_t t4;
  _Alignas(8) uint64_t t5;
  _Alignas(8) uint8_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) uint8_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t4 = ((uint64_t)(v0));
  t5 = ((uint64_t)(((uint64_t)(v2))));
  t6 = ((uint8_t)(v3));
#ifndef __cplusplus
  extern void (fglog_st_u8)(uint64_t, uint64_t, uint8_t);
#endif
  fglog_st_u8(t4, t5, t6);
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  goto fglog_done;
  }
  }
  t0 = ((int64_t)(v2));
  goto fglog_done;
  }
fglog_done:;
  return t0;
}
static int64_t fglog_function_11(uint64_t p0, int64_t p1, int64_t p2, fglog_slice p3, int64_t base) {
  _Alignas(8) int64_t t0;
  int64_t t1;
  _Alignas(8) uint64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) uint8_t t5;
  int64_t t6;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) fglog_slice v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) uint8_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  v4 = ((int64_t)(v2));
  t1 = 0;
label0:;
  if (t1 >= (v3).len) goto label2;
  v5 = ((const GG_UNALIGNED fglog_unaligned_3 *)(v3).data)[t1];
  {
  t2 = ((uint64_t)(v0));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v4));
  t5 = ((uint8_t)(v5));
  v4 = fglog_function_8(t2, t3, t4, t5, base + 1);
  }
label1:;
  t1++; goto label0;
label2:;
  t0 = ((int64_t)(v4));
  goto fglog_done;
  }
fglog_done:;
  for (t6 = 0; t6 < 1; t6++) fglog_stacks[base + t6].top = 0;
  return t0;
}
static int64_t fglog_function_15(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t p4, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint64_t t4;
  _Alignas(8) uint64_t t5;
  _Alignas(8) uint8_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) uint8_t t9;
  _Alignas(8) uint64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) fglog_slice t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) uint8_t t16;
  _Alignas(8) uint64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) fglog_slice t20;
  _Alignas(8) uint8_t t21;
  _Alignas(8) uint8_t t22;
  _Alignas(8) uint8_t t23;
  _Alignas(8) uint64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) fglog_slice t27;
  _Alignas(8) uint8_t t28;
  _Alignas(8) uint8_t t29;
  _Alignas(8) uint8_t t30;
  _Alignas(8) uint64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) fglog_slice t34;
  _Alignas(8) uint8_t t35;
  _Alignas(8) uint8_t t36;
  _Alignas(8) uint8_t t37;
  _Alignas(8) uint64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) fglog_slice t41;
  _Alignas(8) uint64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) uint8_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) uint64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) uint8_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  v5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v6 = ((int64_t)(v2));
label1:;
  t1 = ((int64_t)(v5));
  t2 = ((int64_t)(v4));
  t3 = ((t1) < (t2));
  if (!(t3)) goto label2;
  {
  t4 = ((uint64_t)(v3));
  t5 = ((uint64_t)(((uint64_t)(v5))));
#ifndef __cplusplus
  extern uint8_t (fglog_ld_u8)(uint64_t, uint64_t);
#endif
  t6 = fglog_ld_u8(t4, t5);
  v7 = ((uint8_t)(t6));
  t7 = ((uint8_t)(v7));
  t8 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(34))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v0));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(v6));
  t13 = fglog_view((void *)"\134\042", 2);
  v6 = fglog_function_11(t10, t11, t12, t13, base + 0);
  }
  } else {
  {
  t14 = ((uint8_t)(v7));
  t15 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(92))))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = ((uint64_t)(v0));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(v6));
  t20 = fglog_view((void *)"\134\134", 2);
  v6 = fglog_function_11(t17, t18, t19, t20, base + 0);
  }
  } else {
  {
  t21 = ((uint8_t)(v7));
  t22 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(10))))));
  t23 = ((t21) == (t22));
  if (t23) {
  {
  t24 = ((uint64_t)(v0));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(v6));
  t27 = fglog_view((void *)"\134\156", 2);
  v6 = fglog_function_11(t24, t25, t26, t27, base + 0);
  }
  } else {
  {
  t28 = ((uint8_t)(v7));
  t29 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(13))))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t31 = ((uint64_t)(v0));
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(v6));
  t34 = fglog_view((void *)"\134\162", 2);
  v6 = fglog_function_11(t31, t32, t33, t34, base + 0);
  }
  } else {
  {
  t35 = ((uint8_t)(v7));
  t36 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(9))))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  t38 = ((uint64_t)(v0));
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(v6));
  t41 = fglog_view((void *)"\134\164", 2);
  v6 = fglog_function_11(t38, t39, t40, t41, base + 0);
  }
  } else {
  {
  t42 = ((uint64_t)(v0));
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(v6));
  t45 = ((uint8_t)(v7));
  v6 = fglog_function_8(t42, t43, t44, t45, base + 0);
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  t46 = ((int64_t)(v5));
  t47 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v6));
  goto fglog_done;
  }
fglog_done:;
  return t0;
}
static fglog_slice fglog_function_14(int64_t p0, int64_t base) {
  _Alignas(8) fglog_slice t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint8_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) uint8_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) uint8_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) uint8_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) uint8_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) uint8_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) uint8_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) uint8_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) uint8_t t36;
  _Alignas(8) int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = fglog_view((void *)"\163\145\162\166\145\162\137\163\164\141\162\164", 12);
  goto fglog_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = fglog_view((void *)"\147\160\165\137\144\145\164\145\143\164\145\144", 12);
  goto fglog_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = fglog_view((void *)"\155\157\144\145\154\137\154\157\141\144\151\156\147", 13);
  goto fglog_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = fglog_view((void *)"\155\157\144\145\154\137\154\157\141\144\145\144", 12);
  goto fglog_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t0 = fglog_view((void *)"\155\145\155\157\162\171\137\143\141\143\150\145", 12);
  goto fglog_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t0 = fglog_view((void *)"\162\145\141\144\171", 5);
  goto fglog_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t0 = fglog_view((void *)"\160\145\162\146\157\162\155\141\156\143\145", 11);
  goto fglog_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t0 = fglog_view((void *)"\145\162\162\157\162", 5);
  goto fglog_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(9))))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t0 = fglog_view((void *)"\163\150\165\164\144\157\167\156", 8);
  goto fglog_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t0 = fglog_view((void *)"\167\141\162\156\151\156\147", 7);
  goto fglog_done;
  }
  }
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(11))))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t0 = fglog_view((void *)"\150\164\164\160", 4);
  goto fglog_done;
  }
  }
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  t0 = fglog_view((void *)"\155\157\145", 3);
  goto fglog_done;
  }
  }
  t0 = fglog_view((void *)"\154\157\147", 3);
  goto fglog_done;
  }
fglog_done:;
  return t0;
}
static fglog_slice fglog_function_13(int64_t p0, int64_t base) {
  _Alignas(8) fglog_slice t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint8_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) uint8_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = fglog_view((void *)"\145\162\162\157\162", 5);
  goto fglog_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = fglog_view((void *)"\167\141\162\156", 4);
  goto fglog_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = fglog_view((void *)"\144\145\142\165\147", 5);
  goto fglog_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = fglog_view((void *)"\143\157\156\164", 4);
  goto fglog_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t0 = fglog_view((void *)"\156\157\156\145", 4);
  goto fglog_done;
  }
  }
  t0 = fglog_view((void *)"\151\156\146\157", 4);
  goto fglog_done;
  }
fglog_done:;
  return t0;
}
static int64_t fglog_function_7(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t p4, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint64_t t7;
  _Alignas(8) uint64_t t8;
  _Alignas(8) uint8_t t9;
  _Alignas(8) uint8_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) uint64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  v5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v6 = ((int64_t)(v2));
label1:;
  t1 = ((int64_t)(v5));
  t2 = ((int64_t)(v4));
  t3 = ((t1) < (t2));
  if (!(t3)) goto label2;
  {
  t4 = ((uint64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v6));
  t7 = ((uint64_t)(v3));
  t8 = ((uint64_t)(((uint64_t)(v5))));
#ifndef __cplusplus
  extern uint8_t (fglog_ld_u8)(uint64_t, uint64_t);
#endif
  t9 = fglog_ld_u8(t7, t8);
  t10 = ((uint8_t)(t9));
  v6 = fglog_function_8(t4, t5, t6, t10, base + 0);
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v6));
  goto fglog_done;
  }
fglog_done:;
  return t0;
}
static fglog_slice fglog_function_12(int64_t p0, int64_t base) {
  _Alignas(8) fglog_slice t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint8_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) uint8_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = fglog_view((void *)"\133\105\122\122\117\122\135\040", 8);
  goto fglog_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = fglog_view((void *)"\133\127\101\122\116\135\040", 7);
  goto fglog_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = fglog_view((void *)"\133\104\105\102\125\107\135\040", 8);
  goto fglog_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = fglog_view((void *)"", 0);
  goto fglog_done;
  }
  }
  t0 = fglog_view((void *)"\133\111\116\106\117\135\040", 7);
  goto fglog_done;
  }
fglog_done:;
  return t0;
}
static int64_t fglog_function_10(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) fglog_slice t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) uint8_t t10;
  _Alignas(8) uint64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) fglog_slice t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) uint64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) fglog_slice t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) uint8_t t24;
  _Alignas(8) uint64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) fglog_slice t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) uint8_t t31;
  _Alignas(8) uint64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) fglog_slice t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) uint8_t t38;
  _Alignas(8) uint64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) fglog_slice t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) uint8_t t45;
  _Alignas(8) uint64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) fglog_slice t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) uint8_t t52;
  _Alignas(8) uint64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) fglog_slice t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) uint8_t t59;
  _Alignas(8) uint64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) fglog_slice t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) uint8_t t66;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((uint64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v2));
  t7 = fglog_view((void *)"\360\237\224\245\040\106\122\101\116\113\105\116\115\117\105\040\111\123\040\101\114\111\126\105\041\012", 26);
  t0 = fglog_function_11(t4, t5, t6, t7, base + 0);
  goto fglog_done;
  }
  }
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t11 = ((uint64_t)(v0));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v2));
  t14 = fglog_view((void *)"\342\232\241\040\107\120\125\040\104\105\124\105\103\124\105\104\040\342\200\224\040\122\101\127\040\120\117\127\105\122\040\117\116\114\111\116\105\041\012", 39);
  t0 = fglog_function_11(t11, t12, t13, t14, base + 0);
  goto fglog_done;
  }
  }
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t18 = ((uint64_t)(v0));
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(v2));
  t21 = fglog_view((void *)"\342\232\241\040\125\116\114\105\101\123\110\040\124\110\105\040\115\117\116\123\124\105\122\041\012", 25);
  t0 = fglog_function_11(t18, t19, t20, t21, base + 0);
  goto fglog_done;
  }
  }
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t25 = ((uint64_t)(v0));
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(v2));
  t28 = fglog_view((void *)"\360\237\224\245\040\124\110\105\040\102\105\101\123\124\040\111\123\040\114\117\101\104\105\104\041\012", 26);
  t0 = fglog_function_11(t25, t26, t27, t28, base + 0);
  goto fglog_done;
  }
  }
  t29 = ((int64_t)(v3));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((uint64_t)(v0));
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(v2));
  t35 = fglog_view((void *)"\360\237\247\240\040\103\101\103\110\105\040\101\122\115\105\104\041\012", 18);
  t0 = fglog_function_11(t32, t33, t34, t35, base + 0);
  goto fglog_done;
  }
  }
  t36 = ((int64_t)(v3));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  t39 = ((uint64_t)(v0));
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(v2));
  t42 = fglog_view((void *)"\360\237\224\245\040\101\114\114\040\123\131\123\124\105\115\123\040\102\125\122\116\111\116\107\041\012", 26);
  t0 = fglog_function_11(t39, t40, t41, t42, base + 0);
  goto fglog_done;
  }
  }
  t43 = ((int64_t)(v3));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  t45 = ((t43) == (t44));
  if (t45) {
  {
  t46 = ((uint64_t)(v0));
  t47 = ((int64_t)(v1));
  t48 = ((int64_t)(v2));
  t49 = fglog_view((void *)"\360\237\232\200\040\106\125\114\114\040\124\110\122\117\124\124\114\105\041\012", 20);
  t0 = fglog_function_11(t46, t47, t48, t49, base + 0);
  goto fglog_done;
  }
  }
  t50 = ((int64_t)(v3));
  t51 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t52 = ((t50) == (t51));
  if (t52) {
  {
  t53 = ((uint64_t)(v0));
  t54 = ((int64_t)(v1));
  t55 = ((int64_t)(v2));
  t56 = fglog_view((void *)"\360\237\222\245\040\124\110\105\040\115\117\116\123\124\105\122\040\110\111\124\040\101\040\127\101\114\114\041\012", 29);
  t0 = fglog_function_11(t53, t54, t55, t56, base + 0);
  goto fglog_done;
  }
  }
  t57 = ((int64_t)(v3));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(9))))));
  t59 = ((t57) == (t58));
  if (t59) {
  {
  t60 = ((uint64_t)(v0));
  t61 = ((int64_t)(v1));
  t62 = ((int64_t)(v2));
  t63 = fglog_view((void *)"\360\237\233\221\040\106\122\101\116\113\105\116\115\117\105\040\120\117\127\105\122\111\116\107\040\104\117\127\116\056\012", 31);
  t0 = fglog_function_11(t60, t61, t62, t63, base + 0);
  goto fglog_done;
  }
  }
  t64 = ((int64_t)(v3));
  t65 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  t66 = ((t64) == (t65));
  if (t66) {
  {
  t0 = ((int64_t)(v2));
  goto fglog_done;
  }
  }
  t0 = ((int64_t)(v2));
  goto fglog_done;
  }
fglog_done:;
  return t0;
}
static uint8_t fglog_function_4(uint64_t p0, int64_t p1, int64_t p2, fglog_slice p3, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint8_t t8;
  int64_t t9;
  _Alignas(8) uint64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) uint64_t t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) uint8_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  int64_t t20;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) fglog_slice v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) uint8_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fglog_done;
  }
  }
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)((v3).len))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = ((int64_t)(v1));
  t8 = ((t6) > (t7));
  if (t8) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fglog_done;
  }
  }
  v4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t9 = 0;
label0:;
  if (t9 >= (v3).len) goto label2;
  v5 = ((const GG_UNALIGNED fglog_unaligned_3 *)(v3).data)[t9];
  {
  t10 = ((uint64_t)(v0));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(v4));
  t13 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))))));
#ifndef __cplusplus
  extern uint8_t (fglog_ld_u8)(uint64_t, uint64_t);
#endif
  t14 = fglog_ld_u8(t10, t13);
  t15 = ((uint8_t)(t14));
  t16 = ((uint8_t)(v5));
  t17 = ((t15) != (t16));
  if (t17) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fglog_done;
  }
  }
  t18 = ((int64_t)(v4));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
  }
label1:;
  t9++; goto label0;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto fglog_done;
  }
fglog_done:;
  for (t20 = 0; t20 < 1; t20++) fglog_stacks[base + t20].top = 0;
  return t0;
}
static uint8_t fglog_function_3(uint64_t p0, int64_t p1, fglog_slice p2, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) uint8_t t5;
  _Alignas(8) uint64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) fglog_slice t9;
  _Alignas(8) uint8_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  int64_t t14;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) fglog_slice v2;
  _Alignas(8) int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)((v2).len))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t4 = ((int64_t)(v1));
  t5 = ((t3) <= (t4));
  if (!(t5)) goto label2;
  {
  t6 = ((uint64_t)(v0));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v3));
  t9 = v2;
  t10 = fglog_function_4(t6, t7, t8, t9, base + 1);
  t11 = t10;
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fglog_done;
  }
  }
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(0));
  goto fglog_done;
  }
fglog_done:;
  for (t14 = 0; t14 < 1; t14++) fglog_stacks[base + t14].top = 0;
  return t0;
}
static int64_t fglog_function_2(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint8_t t6;
  _Alignas(8) uint64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) fglog_slice t9;
  _Alignas(8) uint8_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) uint64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) fglog_slice t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) uint8_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) uint64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) fglog_slice t20;
  _Alignas(8) uint8_t t21;
  _Alignas(8) uint8_t t22;
  _Alignas(8) uint8_t t23;
  _Alignas(8) uint64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) fglog_slice t26;
  _Alignas(8) uint8_t t27;
  _Alignas(8) uint8_t t28;
  _Alignas(8) uint8_t t29;
  _Alignas(8) uint64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) fglog_slice t32;
  _Alignas(8) uint8_t t33;
  _Alignas(8) uint8_t t34;
  _Alignas(8) uint64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) fglog_slice t37;
  _Alignas(8) uint8_t t38;
  _Alignas(8) uint8_t t39;
  _Alignas(8) uint8_t t40;
  _Alignas(8) uint64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) fglog_slice t43;
  _Alignas(8) uint8_t t44;
  _Alignas(8) uint8_t t45;
  _Alignas(8) uint8_t t46;
  _Alignas(8) uint64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) fglog_slice t49;
  _Alignas(8) uint8_t t50;
  _Alignas(8) uint8_t t51;
  _Alignas(8) uint64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) fglog_slice t54;
  _Alignas(8) uint8_t t55;
  _Alignas(8) uint8_t t56;
  _Alignas(8) uint8_t t57;
  _Alignas(8) uint64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) fglog_slice t60;
  _Alignas(8) uint8_t t61;
  _Alignas(8) uint8_t t62;
  _Alignas(8) uint64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) fglog_slice t65;
  _Alignas(8) uint8_t t66;
  _Alignas(8) uint8_t t67;
  _Alignas(8) uint8_t t68;
  _Alignas(8) uint64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) fglog_slice t71;
  _Alignas(8) uint8_t t72;
  _Alignas(8) uint8_t t73;
  _Alignas(8) uint8_t t74;
  _Alignas(8) uint64_t t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) fglog_slice t77;
  _Alignas(8) uint8_t t78;
  _Alignas(8) uint8_t t79;
  _Alignas(8) uint8_t t80;
  _Alignas(8) uint64_t t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) fglog_slice t83;
  _Alignas(8) uint8_t t84;
  _Alignas(8) uint8_t t85;
  _Alignas(8) uint8_t t86;
  _Alignas(8) uint64_t t87;
  _Alignas(8) int64_t t88;
  _Alignas(8) fglog_slice t89;
  _Alignas(8) uint8_t t90;
  _Alignas(8) uint8_t t91;
  _Alignas(8) uint8_t t92;
  _Alignas(8) uint64_t t93;
  _Alignas(8) int64_t t94;
  _Alignas(8) fglog_slice t95;
  _Alignas(8) uint8_t t96;
  _Alignas(8) uint8_t t97;
  _Alignas(8) uint64_t t98;
  _Alignas(8) int64_t t99;
  _Alignas(8) fglog_slice t100;
  _Alignas(8) uint8_t t101;
  _Alignas(8) uint8_t t102;
  _Alignas(8) uint8_t t103;
  _Alignas(8) uint64_t t104;
  _Alignas(8) int64_t t105;
  _Alignas(8) fglog_slice t106;
  _Alignas(8) uint8_t t107;
  _Alignas(8) uint8_t t108;
  _Alignas(8) uint64_t t109;
  _Alignas(8) int64_t t110;
  _Alignas(8) fglog_slice t111;
  _Alignas(8) uint8_t t112;
  _Alignas(8) uint8_t t113;
  _Alignas(8) uint8_t t114;
  _Alignas(8) uint64_t t115;
  _Alignas(8) int64_t t116;
  _Alignas(8) fglog_slice t117;
  _Alignas(8) uint8_t t118;
  _Alignas(8) uint8_t t119;
  _Alignas(8) uint8_t t120;
  _Alignas(8) uint64_t t121;
  _Alignas(8) int64_t t122;
  _Alignas(8) fglog_slice t123;
  _Alignas(8) uint8_t t124;
  _Alignas(8) uint8_t t125;
  _Alignas(8) uint64_t t126;
  _Alignas(8) int64_t t127;
  _Alignas(8) fglog_slice t128;
  _Alignas(8) uint8_t t129;
  _Alignas(8) uint8_t t130;
  _Alignas(8) uint8_t t131;
  _Alignas(8) uint64_t t132;
  _Alignas(8) int64_t t133;
  _Alignas(8) fglog_slice t134;
  _Alignas(8) uint8_t t135;
  _Alignas(8) uint8_t t136;
  _Alignas(8) uint64_t t137;
  _Alignas(8) int64_t t138;
  _Alignas(8) fglog_slice t139;
  _Alignas(8) uint8_t t140;
  _Alignas(8) uint8_t t141;
  _Alignas(8) uint8_t t142;
  _Alignas(8) uint64_t t143;
  _Alignas(8) int64_t t144;
  _Alignas(8) fglog_slice t145;
  _Alignas(8) uint8_t t146;
  _Alignas(8) uint8_t t147;
  _Alignas(8) uint8_t t148;
  _Alignas(8) uint64_t t149;
  _Alignas(8) int64_t t150;
  _Alignas(8) fglog_slice t151;
  _Alignas(8) uint8_t t152;
  _Alignas(8) uint8_t t153;
  _Alignas(8) uint8_t t154;
  _Alignas(8) uint64_t t155;
  _Alignas(8) int64_t t156;
  _Alignas(8) fglog_slice t157;
  _Alignas(8) uint8_t t158;
  _Alignas(8) uint8_t t159;
  _Alignas(8) uint8_t t160;
  _Alignas(8) uint64_t t161;
  _Alignas(8) int64_t t162;
  _Alignas(8) fglog_slice t163;
  _Alignas(8) uint8_t t164;
  _Alignas(8) uint8_t t165;
  _Alignas(8) uint64_t t166;
  _Alignas(8) int64_t t167;
  _Alignas(8) fglog_slice t168;
  _Alignas(8) uint8_t t169;
  _Alignas(8) uint8_t t170;
  _Alignas(8) uint8_t t171;
  _Alignas(8) uint64_t t172;
  _Alignas(8) int64_t t173;
  _Alignas(8) fglog_slice t174;
  _Alignas(8) uint8_t t175;
  _Alignas(8) uint8_t t176;
  _Alignas(8) uint8_t t177;
  _Alignas(8) uint64_t t178;
  _Alignas(8) int64_t t179;
  _Alignas(8) fglog_slice t180;
  _Alignas(8) uint8_t t181;
  _Alignas(8) uint8_t t182;
  _Alignas(8) uint64_t t183;
  _Alignas(8) int64_t t184;
  _Alignas(8) fglog_slice t185;
  _Alignas(8) uint8_t t186;
  _Alignas(8) uint8_t t187;
  _Alignas(8) uint8_t t188;
  _Alignas(8) uint64_t t189;
  _Alignas(8) int64_t t190;
  _Alignas(8) fglog_slice t191;
  _Alignas(8) uint8_t t192;
  _Alignas(8) uint8_t t193;
  _Alignas(8) uint8_t t194;
  _Alignas(8) uint64_t t195;
  _Alignas(8) int64_t t196;
  _Alignas(8) fglog_slice t197;
  _Alignas(8) uint8_t t198;
  _Alignas(8) uint8_t t199;
  _Alignas(8) uint8_t t200;
  _Alignas(8) uint64_t t201;
  _Alignas(8) int64_t t202;
  _Alignas(8) fglog_slice t203;
  _Alignas(8) uint8_t t204;
  _Alignas(8) uint8_t t205;
  _Alignas(8) uint64_t t206;
  _Alignas(8) int64_t t207;
  _Alignas(8) fglog_slice t208;
  _Alignas(8) uint8_t t209;
  _Alignas(8) uint8_t t210;
  _Alignas(8) uint8_t t211;
  _Alignas(8) uint64_t t212;
  _Alignas(8) int64_t t213;
  _Alignas(8) fglog_slice t214;
  _Alignas(8) uint8_t t215;
  _Alignas(8) uint8_t t216;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  goto fglog_done;
  }
  }
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  goto fglog_done;
  }
  }
  t7 = ((uint64_t)(v0));
  t8 = ((int64_t)(v1));
  t9 = fglog_view((void *)"\154\157\141\144\137\155\157\144\145\154\072\040\151\156\151\164\151\141\154\151\172\151\156\147", 24);
  t10 = fglog_function_3(t7, t8, t9, base + 0);
  t11 = t10;
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  goto fglog_done;
  }
  }
  t12 = ((uint64_t)(v0));
  t13 = ((int64_t)(v1));
  t14 = fglog_view((void *)"\154\157\141\144\151\156\147\040\155\157\144\145\154", 13);
  t15 = fglog_function_3(t12, t13, t14, base + 0);
  t16 = t15;
  t17 = t16;
  if (!t17) {
  t18 = ((uint64_t)(v0));
  t19 = ((int64_t)(v1));
  t20 = fglog_view((void *)"\154\157\141\144\137\155\157\144\145\154", 10);
  t21 = fglog_function_3(t18, t19, t20, base + 0);
  t17 = t21;
  }
  t22 = t17;
  t23 = t22;
  if (!t23) {
  t24 = ((uint64_t)(v0));
  t25 = ((int64_t)(v1));
  t26 = fglog_view((void *)"\154\154\141\155\141\137\155\157\144\145\154\137\154\157\141\144\145\162", 18);
  t27 = fglog_function_3(t24, t25, t26, base + 0);
  t23 = t27;
  }
  t28 = t23;
  t29 = t28;
  if (!t29) {
  t30 = ((uint64_t)(v0));
  t31 = ((int64_t)(v1));
  t32 = fglog_view((void *)"\154\157\141\144\137\164\145\156\163\157\162\163", 12);
  t33 = fglog_function_3(t30, t31, t32, base + 0);
  t29 = t33;
  }
  t34 = t29;
  if (t34) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  goto fglog_done;
  }
  }
  t35 = ((uint64_t)(v0));
  t36 = ((int64_t)(v1));
  t37 = fglog_view((void *)"\154\154\141\155\141\137\163\145\162\166\145\162\072\040\151\156\151\164\151\141\154\151\172\151\156\147", 26);
  t38 = fglog_function_3(t35, t36, t37, base + 0);
  t39 = t38;
  t40 = t39;
  if (!t40) {
  t41 = ((uint64_t)(v0));
  t42 = ((int64_t)(v1));
  t43 = fglog_view((void *)"\163\164\141\162\164\151\156\147\040\163\145\162\166\145\162", 15);
  t44 = fglog_function_3(t41, t42, t43, base + 0);
  t40 = t44;
  }
  t45 = t40;
  t46 = t45;
  if (!t46) {
  t47 = ((uint64_t)(v0));
  t48 = ((int64_t)(v1));
  t49 = fglog_view((void *)"\163\145\162\166\145\162\040\151\163\040\163\164\141\162\164\151\156\147", 18);
  t50 = fglog_function_3(t47, t48, t49, base + 0);
  t46 = t50;
  }
  t51 = t46;
  if (t51) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fglog_done;
  }
  }
  t52 = ((uint64_t)(v0));
  t53 = ((int64_t)(v1));
  t54 = fglog_view((void *)"\154\151\163\164\145\156\151\156\147", 9);
  t55 = fglog_function_3(t52, t53, t54, base + 0);
  t56 = t55;
  t57 = t56;
  if (!t57) {
  t58 = ((uint64_t)(v0));
  t59 = ((int64_t)(v1));
  t60 = fglog_view((void *)"\163\145\162\166\145\162\040\151\163\040\162\145\141\144\171", 15);
  t61 = fglog_function_3(t58, t59, t60, base + 0);
  t57 = t61;
  }
  t62 = t57;
  if (t62) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  goto fglog_done;
  }
  }
  t63 = ((uint64_t)(v0));
  t64 = ((int64_t)(v1));
  t65 = fglog_view((void *)"\103\125\104\101", 4);
  t66 = fglog_function_3(t63, t64, t65, base + 0);
  t67 = t66;
  t68 = t67;
  if (!t68) {
  t69 = ((uint64_t)(v0));
  t70 = ((int64_t)(v1));
  t71 = fglog_view((void *)"\143\165\144\141", 4);
  t72 = fglog_function_3(t69, t70, t71, base + 0);
  t68 = t72;
  }
  t73 = t68;
  t74 = t73;
  if (!t74) {
  t75 = ((uint64_t)(v0));
  t76 = ((int64_t)(v1));
  t77 = fglog_view((void *)"\116\126\111\104\111\101", 6);
  t78 = fglog_function_3(t75, t76, t77, base + 0);
  t74 = t78;
  }
  t79 = t74;
  t80 = t79;
  if (t80) {
  t81 = ((uint64_t)(v0));
  t82 = ((int64_t)(v1));
  t83 = fglog_view((void *)"\144\145\166\151\143\145", 6);
  t84 = fglog_function_3(t81, t82, t83, base + 0);
  t85 = t84;
  t86 = t85;
  if (!t86) {
  t87 = ((uint64_t)(v0));
  t88 = ((int64_t)(v1));
  t89 = fglog_view((void *)"\107\120\125", 3);
  t90 = fglog_function_3(t87, t88, t89, base + 0);
  t86 = t90;
  }
  t91 = t86;
  t92 = t91;
  if (!t92) {
  t93 = ((uint64_t)(v0));
  t94 = ((int64_t)(v1));
  t95 = fglog_view((void *)"\142\141\143\153\145\156\144", 7);
  t96 = fglog_function_3(t93, t94, t95, base + 0);
  t92 = t96;
  }
  t80 = t92;
  }
  t97 = t80;
  if (t97) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  goto fglog_done;
  }
  }
  t98 = ((uint64_t)(v0));
  t99 = ((int64_t)(v1));
  t100 = fglog_view((void *)"\155\157\144\145\154\040\154\157\141\144\145\144", 12);
  t101 = fglog_function_3(t98, t99, t100, base + 0);
  t102 = t101;
  t103 = t102;
  if (!t103) {
  t104 = ((uint64_t)(v0));
  t105 = ((int64_t)(v1));
  t106 = fglog_view((void *)"\154\157\141\144\040\164\151\155\145", 9);
  t107 = fglog_function_3(t104, t105, t106, base + 0);
  t103 = t107;
  }
  t108 = t103;
  if (t108) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  goto fglog_done;
  }
  }
  t109 = ((uint64_t)(v0));
  t110 = ((int64_t)(v1));
  t111 = fglog_view((void *)"\113\126", 2);
  t112 = fglog_function_3(t109, t110, t111, base + 0);
  t113 = t112;
  t114 = t113;
  if (!t114) {
  t115 = ((uint64_t)(v0));
  t116 = ((int64_t)(v1));
  t117 = fglog_view((void *)"\143\141\143\150\145", 5);
  t118 = fglog_function_3(t115, t116, t117, base + 0);
  t114 = t118;
  }
  t119 = t114;
  t120 = t119;
  if (!t120) {
  t121 = ((uint64_t)(v0));
  t122 = ((int64_t)(v1));
  t123 = fglog_view((void *)"\142\165\146\146\145\162\040\163\151\172\145", 11);
  t124 = fglog_function_3(t121, t122, t123, base + 0);
  t120 = t124;
  }
  t125 = t120;
  if (t125) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  goto fglog_done;
  }
  }
  t126 = ((uint64_t)(v0));
  t127 = ((int64_t)(v1));
  t128 = fglog_view((void *)"\141\154\154\040\163\171\163\164\145\155\163", 11);
  t129 = fglog_function_3(t126, t127, t128, base + 0);
  t130 = t129;
  t131 = t130;
  if (!t131) {
  t132 = ((uint64_t)(v0));
  t133 = ((int64_t)(v1));
  t134 = fglog_view((void *)"\155\157\144\145\154\040\154\157\141\144\145\144\040\163\165\143\143\145\163\163\146\165\154\154\171", 25);
  t135 = fglog_function_3(t132, t133, t134, base + 0);
  t131 = t135;
  }
  t136 = t131;
  if (t136) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  goto fglog_done;
  }
  }
  t137 = ((uint64_t)(v0));
  t138 = ((int64_t)(v1));
  t139 = fglog_view((void *)"\160\162\157\155\160\164\040\145\166\141\154\040\164\151\155\145", 16);
  t140 = fglog_function_3(t137, t138, t139, base + 0);
  t141 = t140;
  t142 = t141;
  if (!t142) {
  t143 = ((uint64_t)(v0));
  t144 = ((int64_t)(v1));
  t145 = fglog_view((void *)"\145\166\141\154\040\164\151\155\145", 9);
  t146 = fglog_function_3(t143, t144, t145, base + 0);
  t142 = t146;
  }
  t147 = t142;
  t148 = t147;
  if (!t148) {
  t149 = ((uint64_t)(v0));
  t150 = ((int64_t)(v1));
  t151 = fglog_view((void *)"\160\162\151\156\164\137\164\151\155\151\156\147", 12);
  t152 = fglog_function_3(t149, t150, t151, base + 0);
  t148 = t152;
  }
  t153 = t148;
  t154 = t153;
  if (!t154) {
  t155 = ((uint64_t)(v0));
  t156 = ((int64_t)(v1));
  t157 = fglog_view((void *)"\164\157\153\145\156\163\040\160\145\162\040\163\145\143\157\156\144", 17);
  t158 = fglog_function_3(t155, t156, t157, base + 0);
  t154 = t158;
  }
  t159 = t154;
  t160 = t159;
  if (!t160) {
  t161 = ((uint64_t)(v0));
  t162 = ((int64_t)(v1));
  t163 = fglog_view((void *)"\164\157\153\057\163", 5);
  t164 = fglog_function_3(t161, t162, t163, base + 0);
  t160 = t164;
  }
  t165 = t160;
  if (t165) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  goto fglog_done;
  }
  }
  t166 = ((uint64_t)(v0));
  t167 = ((int64_t)(v1));
  t168 = fglog_view((void *)"\163\150\165\164\164\151\156\147\040\144\157\167\156", 13);
  t169 = fglog_function_3(t166, t167, t168, base + 0);
  t170 = t169;
  t171 = t170;
  if (!t171) {
  t172 = ((uint64_t)(v0));
  t173 = ((int64_t)(v1));
  t174 = fglog_view((void *)"\143\154\145\141\156\151\156\147\040\165\160", 11);
  t175 = fglog_function_3(t172, t173, t174, base + 0);
  t171 = t175;
  }
  t176 = t171;
  t177 = t176;
  if (!t177) {
  t178 = ((uint64_t)(v0));
  t179 = ((int64_t)(v1));
  t180 = fglog_view((void *)"\163\145\162\166\145\162\040\163\164\157\160\160\145\144", 14);
  t181 = fglog_function_3(t178, t179, t180, base + 0);
  t177 = t181;
  }
  t182 = t177;
  if (t182) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(9))))));
  goto fglog_done;
  }
  }
  t183 = ((uint64_t)(v0));
  t184 = ((int64_t)(v1));
  t185 = fglog_view((void *)"\110\124\124\120", 4);
  t186 = fglog_function_3(t183, t184, t185, base + 0);
  t187 = t186;
  t188 = t187;
  if (!t188) {
  t189 = ((uint64_t)(v0));
  t190 = ((int64_t)(v1));
  t191 = fglog_view((void *)"\162\145\161\165\145\163\164", 7);
  t192 = fglog_function_3(t189, t190, t191, base + 0);
  t188 = t192;
  }
  t193 = t188;
  t194 = t193;
  if (!t194) {
  t195 = ((uint64_t)(v0));
  t196 = ((int64_t)(v1));
  t197 = fglog_view((void *)"\120\117\123\124", 4);
  t198 = fglog_function_3(t195, t196, t197, base + 0);
  t194 = t198;
  }
  t199 = t194;
  t200 = t199;
  if (!t200) {
  t201 = ((uint64_t)(v0));
  t202 = ((int64_t)(v1));
  t203 = fglog_view((void *)"\107\105\124\040", 4);
  t204 = fglog_function_3(t201, t202, t203, base + 0);
  t200 = t204;
  }
  t205 = t200;
  if (t205) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(11))))));
  goto fglog_done;
  }
  }
  t206 = ((uint64_t)(v0));
  t207 = ((int64_t)(v1));
  t208 = fglog_view((void *)"\115\157\105", 3);
  t209 = fglog_function_3(t206, t207, t208, base + 0);
  t210 = t209;
  t211 = t210;
  if (!t211) {
  t212 = ((uint64_t)(v0));
  t213 = ((int64_t)(v1));
  t214 = fglog_view((void *)"\145\170\160\145\162\164", 6);
  t215 = fglog_function_3(t212, t213, t214, base + 0);
  t211 = t215;
  }
  t216 = t211;
  if (t216) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  goto fglog_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fglog_done;
  }
fglog_done:;
  return t0;
}
static int64_t fglog_function_1(uint64_t p0, int64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, uint64_t p6, int64_t p7, uint64_t p8, int64_t p9, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) uint64_t t4;
  _Alignas(8) uint64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) uint8_t t9;
  _Alignas(8) uint64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) uint64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) uint8_t t20;
  _Alignas(8) uint64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) uint64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) fglog_slice t29;
  _Alignas(8) uint64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) uint64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) uint8_t t37;
  _Alignas(8) uint64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) uint64_t t41;
  _Alignas(8) uint8_t t42;
  _Alignas(8) uint8_t t43;
  _Alignas(8) uint8_t t44;
  _Alignas(8) uint8_t t45;
  _Alignas(8) uint64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) uint8_t t49;
  _Alignas(8) uint64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) fglog_slice t53;
  _Alignas(8) uint64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) uint64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) uint64_t t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) fglog_slice t62;
  _Alignas(8) uint64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) fglog_slice t67;
  _Alignas(8) uint64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) fglog_slice t71;
  _Alignas(8) uint64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) int64_t t75;
  _Alignas(8) fglog_slice t76;
  _Alignas(8) uint64_t t77;
  _Alignas(8) int64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) fglog_slice t80;
  _Alignas(8) int64_t t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) uint8_t t83;
  _Alignas(8) uint64_t t84;
  _Alignas(8) int64_t t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) uint64_t t87;
  _Alignas(8) uint8_t t88;
  _Alignas(8) uint8_t t89;
  _Alignas(8) uint8_t t90;
  _Alignas(8) uint8_t t91;
  _Alignas(8) int64_t t92;
  _Alignas(8) int64_t t93;
  _Alignas(8) uint64_t t94;
  _Alignas(8) int64_t t95;
  _Alignas(8) int64_t t96;
  _Alignas(8) uint64_t t97;
  _Alignas(8) int64_t t98;
  _Alignas(8) uint64_t t99;
  _Alignas(8) int64_t t100;
  _Alignas(8) int64_t t101;
  _Alignas(8) fglog_slice t102;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) uint64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) uint64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
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
  {
  t1 = ((uint64_t)(v6));
  t2 = ((int64_t)(v7));
  t3 = ((int64_t)(v3));
  v10 = fglog_function_2(t1, t2, t3, base + 0);
  t4 = ((uint64_t)(v2));
  t5 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t6 = ((int64_t)(v10));
#ifndef __cplusplus
  extern void (fglog_st_i64)(uint64_t, uint64_t, int64_t);
#endif
  fglog_st_i64(t4, t5, t6);
  t7 = ((int64_t)(v4));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v0));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t13 = ((uint64_t)(v6));
  t14 = ((int64_t)(v7));
  t0 = fglog_function_7(t10, t11, t12, t13, t14, base + 0);
  goto fglog_done;
  }
  }
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  v11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t18 = ((int64_t)(v10));
  t19 = ((int64_t)(v5));
  t20 = ((t18) != (t19));
  if (t20) {
  {
  t21 = ((uint64_t)(v0));
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(v11));
  t24 = ((int64_t)(v10));
  v11 = fglog_function_10(t21, t22, t23, t24, base + 0);
  }
  }
  t25 = ((uint64_t)(v0));
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(v11));
  t28 = ((int64_t)(v3));
  t29 = fglog_function_12(t28, base + 0);
  v11 = fglog_function_11(t25, t26, t27, t29, base + 0);
  t30 = ((uint64_t)(v0));
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(v11));
  t33 = ((uint64_t)(v6));
  t34 = ((int64_t)(v7));
  v11 = fglog_function_7(t30, t31, t32, t33, t34, base + 0);
  t35 = ((int64_t)(v7));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t37 = ((t35) > (t36));
  if (t37) {
  {
  t38 = ((uint64_t)(v6));
  t39 = ((int64_t)(v7));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t41 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t39) - (uint64_t)(t40)))))));
#ifndef __cplusplus
  extern uint8_t (fglog_ld_u8)(uint64_t, uint64_t);
#endif
  t42 = fglog_ld_u8(t38, t41);
  t43 = ((uint8_t)(t42));
  t44 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(10))))));
  t45 = ((t43) != (t44));
  if (t45) {
  {
  t46 = ((uint64_t)(v0));
  t47 = ((int64_t)(v1));
  t48 = ((int64_t)(v11));
  t49 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(10))))));
  v11 = fglog_function_8(t46, t47, t48, t49, base + 0);
  }
  }
  }
  }
  t0 = ((int64_t)(v11));
  goto fglog_done;
  }
  }
  v12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t50 = ((uint64_t)(v0));
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(v12));
  t53 = fglog_view((void *)"\173\042\164\151\155\145\042\072\042", 9);
  v12 = fglog_function_11(t50, t51, t52, t53, base + 0);
  t54 = ((uint64_t)(v0));
  t55 = ((int64_t)(v1));
  t56 = ((int64_t)(v12));
  t57 = ((uint64_t)(v8));
  t58 = ((int64_t)(v9));
  v12 = fglog_function_7(t54, t55, t56, t57, t58, base + 0);
  t59 = ((uint64_t)(v0));
  t60 = ((int64_t)(v1));
  t61 = ((int64_t)(v12));
  t62 = fglog_view((void *)"\042\054\042\154\145\166\145\154\042\072\042", 11);
  v12 = fglog_function_11(t59, t60, t61, t62, base + 0);
  t63 = ((uint64_t)(v0));
  t64 = ((int64_t)(v1));
  t65 = ((int64_t)(v12));
  t66 = ((int64_t)(v3));
  t67 = fglog_function_13(t66, base + 0);
  v12 = fglog_function_11(t63, t64, t65, t67, base + 0);
  t68 = ((uint64_t)(v0));
  t69 = ((int64_t)(v1));
  t70 = ((int64_t)(v12));
  t71 = fglog_view((void *)"\042\054\042\145\166\145\156\164\042\072\042", 11);
  v12 = fglog_function_11(t68, t69, t70, t71, base + 0);
  t72 = ((uint64_t)(v0));
  t73 = ((int64_t)(v1));
  t74 = ((int64_t)(v12));
  t75 = ((int64_t)(v10));
  t76 = fglog_function_14(t75, base + 0);
  v12 = fglog_function_11(t72, t73, t74, t76, base + 0);
  t77 = ((uint64_t)(v0));
  t78 = ((int64_t)(v1));
  t79 = ((int64_t)(v12));
  t80 = fglog_view((void *)"\042\054\042\155\163\147\042\072\042", 9);
  v12 = fglog_function_11(t77, t78, t79, t80, base + 0);
  v13 = ((int64_t)(v7));
  t81 = ((int64_t)(v13));
  t82 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t83 = ((t81) > (t82));
  if (t83) {
  {
  t84 = ((uint64_t)(v6));
  t85 = ((int64_t)(v13));
  t86 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t87 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t85) - (uint64_t)(t86)))))));
#ifndef __cplusplus
  extern uint8_t (fglog_ld_u8)(uint64_t, uint64_t);
#endif
  t88 = fglog_ld_u8(t84, t87);
  t89 = ((uint8_t)(t88));
  t90 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(10))))));
  t91 = ((t89) == (t90));
  if (t91) {
  {
  t92 = ((int64_t)(v13));
  t93 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t92) - (uint64_t)(t93)))));
  }
  }
  }
  }
  t94 = ((uint64_t)(v0));
  t95 = ((int64_t)(v1));
  t96 = ((int64_t)(v12));
  t97 = ((uint64_t)(v6));
  t98 = ((int64_t)(v13));
  v12 = fglog_function_15(t94, t95, t96, t97, t98, base + 0);
  t99 = ((uint64_t)(v0));
  t100 = ((int64_t)(v1));
  t101 = ((int64_t)(v12));
  t102 = fglog_view((void *)"\042\175\012", 3);
  v12 = fglog_function_11(t99, t100, t101, t102, base + 0);
  t0 = ((int64_t)(v12));
  goto fglog_done;
  }
fglog_done:;
  return t0;
}
static void fglog_function_0(int64_t base) {
  _Alignas(8) uint64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) uint64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t6 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t10 = fglog_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, base + 0);
  (void)(t10);
  }
}
#ifdef _MSC_VER
static volatile LONG fglog_globals_state;
static __declspec(thread) int fglog_globals_owner;
static int fglog_globals_ready(void) { return InterlockedCompareExchange(&fglog_globals_state, 0, 0) == 2; }
static int fglog_globals_claim(void) { return InterlockedCompareExchange(&fglog_globals_state, 1, 0) == 0; }
static void fglog_globals_publish(void) { InterlockedExchange(&fglog_globals_state, 2); }
#else
#include <stdatomic.h>
static atomic_int fglog_globals_state;
static _Thread_local int fglog_globals_owner;
static int fglog_globals_ready(void) { return atomic_load_explicit(&fglog_globals_state, memory_order_acquire) == 2; }
static int fglog_globals_claim(void) { int expected = 0; return atomic_compare_exchange_strong_explicit(&fglog_globals_state, &expected, 1, memory_order_acq_rel, memory_order_acquire); }
static void fglog_globals_publish(void) { atomic_store_explicit(&fglog_globals_state, 2, memory_order_release); }
#endif
#ifndef _WIN32
#include <sched.h>
#endif
int64_t fglog_init_globals(int argc, char **argv) {  if (argc < 0 || (argc && !argv)) fglog_fail("invalid module arguments");
  if (fglog_globals_ready()) return 0;
  if (fglog_globals_owner) fglog_fail("recursive global initialization");
  if (!fglog_globals_claim()) {
    while (!fglog_globals_ready()) {
#ifdef _WIN32
      SwitchToThread();
#else
      sched_yield();
#endif
    }
    return 0;
  }
  fglog_globals_owner = 1;
  struct { int64_t base; } frame;
  frame.base = 0;
  fglog_argc = argc; fglog_argv = argv;
  fglog_globals_owner = 0;
  fglog_globals_publish();
  return 0;
}
static int fglog_program(int argc, char **argv) {
  int64_t base = fglog_init_globals(argc, argv);
  fglog_function_0(base);
  return 0;
}
int fglog_generated_main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fglog_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fglog_fail("native call stack overflow"); return 1;
  }
#else
  return fglog_program(argc, argv);
#endif
}
