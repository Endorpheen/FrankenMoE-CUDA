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
static uint8_t gs_os_read_file(gg_slice path,gg_reference out) {
    char *p=gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t gs_os_write_file(gg_slice path,gg_slice data) {
    char *p=gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t gs_os_file_exists(gg_slice path) { char*p=gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
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
typedef uint64_t gg_unaligned_3 GG_ALIGN1;
typedef int64_t gg_unaligned_4 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { int64_t data[256]; } gg_type_7;
#pragma pack(pop)
typedef gg_slice gg_unaligned_8 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { uint8_t len; int64_t data[144]; } gg_type_9;
#pragma pack(pop)
_Static_assert(sizeof(gg_type_9) == 1153, "limited array layout mismatch");
#pragma pack(push, 1)
typedef struct { int64_t data[1]; } gg_type_10;
#pragma pack(pop)
typedef gg_type_9 *gg_type_11;
typedef gg_type_11 gg_unaligned_11 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_4 *gg_type_12;
typedef gg_type_12 gg_unaligned_12 GG_ALIGN1;
typedef float gg_unaligned_13 GG_ALIGN1;
typedef int8_t gg_unaligned_14 GG_ALIGN1;
typedef int16_t gg_unaligned_15 GG_ALIGN1;
typedef int32_t gg_unaligned_16 GG_ALIGN1;
typedef uint8_t gg_unaligned_17 GG_ALIGN1;
typedef uint16_t gg_unaligned_18 GG_ALIGN1;
typedef uint32_t gg_unaligned_19 GG_ALIGN1;
typedef gg_slice gg_unaligned_20 GG_ALIGN1;
typedef struct gg_type_21 gg_type_21;
#pragma pack(push, 1)
struct gg_type_21 {
  uint64_t f0;
};
#pragma pack(pop)
typedef gg_type_21 *gg_type_22;
typedef gg_type_22 gg_unaligned_22 GG_ALIGN1;
typedef struct gg_type_23 gg_type_23;
#pragma pack(push, 1)
typedef struct { uint8_t data[256]; } gg_type_24;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[3]; } gg_type_25;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[16]; } gg_type_26;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_23 {
  gg_type_24 f1;
  uint8_t f2;
  uint8_t f3;
  gg_type_25 f4;
  gg_type_26 f5;
  gg_type_26 f6;
};
#pragma pack(pop)
typedef gg_type_24 *gg_type_27;
typedef gg_type_27 gg_unaligned_27 GG_ALIGN1;
typedef const gg_type_23 *gg_type_28;
typedef gg_type_28 gg_unaligned_28 GG_ALIGN1;
typedef gg_slice gg_unaligned_29 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_17 *gg_type_30;
typedef gg_type_30 gg_unaligned_30 GG_ALIGN1;
typedef gg_reference gg_unaligned_32 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { uint8_t len; uint8_t data[64]; } gg_type_33;
#pragma pack(pop)
_Static_assert(sizeof(gg_type_33) == 65, "limited array layout mismatch");
#pragma pack(push, 1)
typedef struct { uint8_t data[1]; } gg_type_34;
#pragma pack(pop)
typedef gg_type_33 *gg_type_35;
typedef gg_type_35 gg_unaligned_35 GG_ALIGN1;
#pragma pack(push, 1)
typedef struct { gg_slice data[1]; } gg_type_37;
#pragma pack(pop)
typedef GG_UNALIGNED gg_unaligned_29 *gg_type_38;
typedef gg_type_38 gg_unaligned_38 GG_ALIGN1;
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(double a, double b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, double value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint64_t value);
static uint8_t gg_eq_4(int64_t a, int64_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, int64_t value);
static uint8_t gg_eq_5(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, uint8_t value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_8(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_slice value);
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, gg_type_9 value);
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, gg_type_10 value);
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, gg_type_11 value);
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_type_12 value);
static uint8_t gg_eq_13(float a, float b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, float value);
static uint8_t gg_eq_14(int8_t a, int8_t b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, int8_t value);
static uint8_t gg_eq_15(int16_t a, int16_t b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, int16_t value);
static uint8_t gg_eq_16(int32_t a, int32_t b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, int32_t value);
static uint8_t gg_eq_17(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, uint8_t value);
static uint8_t gg_eq_18(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, uint16_t value);
static uint8_t gg_eq_19(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, uint32_t value);
static uint8_t gg_eq_20(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_20(gg_slice a, gg_slice b);
static void gg_render_20(gg_reference out, gg_slice value);
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
static uint8_t gg_eq_29(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_29(gg_slice a, gg_slice b);
static void gg_render_29(gg_reference out, gg_slice value);
static uint8_t gg_eq_30(gg_type_30 a, gg_type_30 b);
static uint8_t gg_equal_elements_30(gg_slice a, gg_slice b);
static void gg_render_30(gg_reference out, gg_type_30 value);
static uint8_t gg_eq_31(gg_header a, gg_header b);
static uint8_t gg_equal_elements_31(gg_slice a, gg_slice b);
static void gg_render_31(gg_reference out, gg_header value);
static uint8_t gg_eq_32(gg_reference a, gg_reference b);
static uint8_t gg_equal_elements_32(gg_slice a, gg_slice b);
static void gg_render_32(gg_reference out, gg_reference value);
static uint8_t gg_eq_33(gg_type_33 a, gg_type_33 b);
static uint8_t gg_equal_elements_33(gg_slice a, gg_slice b);
static void gg_render_33(gg_reference out, gg_type_33 value);
static uint8_t gg_eq_34(gg_type_34 a, gg_type_34 b);
static uint8_t gg_equal_elements_34(gg_slice a, gg_slice b);
static void gg_render_34(gg_reference out, gg_type_34 value);
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b);
static uint8_t gg_equal_elements_35(gg_slice a, gg_slice b);
static void gg_render_35(gg_reference out, gg_type_35 value);
static uint8_t gg_eq_36(gg_header a, gg_header b);
static uint8_t gg_equal_elements_36(gg_slice a, gg_slice b);
static void gg_render_36(gg_reference out, gg_header value);
static uint8_t gg_eq_37(gg_type_37 a, gg_type_37 b);
static uint8_t gg_equal_elements_37(gg_slice a, gg_slice b);
static void gg_render_37(gg_reference out, gg_type_37 value);
static uint8_t gg_eq_38(gg_type_38 a, gg_type_38 b);
static uint8_t gg_equal_elements_38(gg_slice a, gg_slice b);
static void gg_render_38(gg_reference out, gg_type_38 value);
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
static uint8_t gg_eq_3(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((GG_UNALIGNED gg_unaligned_4*)a.data)[i],((GG_UNALIGNED gg_unaligned_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_4(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_4(out, ((GG_UNALIGNED gg_unaligned_4*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((GG_UNALIGNED gg_unaligned_8*)a.data)[i],((GG_UNALIGNED gg_unaligned_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_slice a, gg_slice b) {
return gg_equal_elements_4(a,b);
}
static void gg_render_8(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_4(out, ((GG_UNALIGNED gg_unaligned_4*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((gg_type_9*)a.data)[i],((gg_type_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b) {
  if ((uint64_t)(a).len > UINT64_C(144)) gg_fail("invalid limited array length");
  if ((uint64_t)(b).len > UINT64_C(144)) gg_fail("invalid limited array length");
return gg_equal_elements_4(gg_view((a).data, (a).len),gg_view((b).data, (b).len));
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
  if ((uint64_t)(value).len > UINT64_C(144)) gg_fail("invalid limited array length");
int64_t i; gg_slice view=gg_view((value).data, (value).len);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_4(out, ((GG_UNALIGNED gg_unaligned_4*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
return gg_equal_elements_4(gg_view((a).data, 0),gg_view((b).data, 0));
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
int64_t i; gg_slice view=gg_view((value).data, 0);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_4(out, ((GG_UNALIGNED gg_unaligned_4*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((GG_UNALIGNED gg_unaligned_11*)a.data)[i],((GG_UNALIGNED gg_unaligned_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b) {
return a == b;
}
static void gg_render_11(gg_reference out, gg_type_11 value) {
gg_render_9(out,*value);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((GG_UNALIGNED gg_unaligned_12*)a.data)[i],((GG_UNALIGNED gg_unaligned_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return a == b;
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
gg_render_4(out,*value);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((GG_UNALIGNED gg_unaligned_13*)a.data)[i],((GG_UNALIGNED gg_unaligned_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(float a, float b) {
return a == b;
}
static void gg_render_13(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((GG_UNALIGNED gg_unaligned_14*)a.data)[i],((GG_UNALIGNED gg_unaligned_14*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_14(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((GG_UNALIGNED gg_unaligned_15*)a.data)[i],((GG_UNALIGNED gg_unaligned_15*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_15(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((GG_UNALIGNED gg_unaligned_16*)a.data)[i],((GG_UNALIGNED gg_unaligned_16*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_16(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((GG_UNALIGNED gg_unaligned_17*)a.data)[i],((GG_UNALIGNED gg_unaligned_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_17(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((GG_UNALIGNED gg_unaligned_18*)a.data)[i],((GG_UNALIGNED gg_unaligned_18*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_18(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((GG_UNALIGNED gg_unaligned_19*)a.data)[i],((GG_UNALIGNED gg_unaligned_19*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_19(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((GG_UNALIGNED gg_unaligned_20*)a.data)[i],((GG_UNALIGNED gg_unaligned_20*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_slice a, gg_slice b) {
return gg_equal_elements_17(a,b);
}
static void gg_render_20(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((gg_type_21*)a.data)[i],((gg_type_21*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b) {
if(!gg_eq_3(a.f0,b.f0))return 0;
return 1;
}
static void gg_render_21(gg_reference out, gg_type_21 value) {
gg_append(out,"\162\156\147\040\173\040",6,1);
gg_render_3(out, value.f0);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((GG_UNALIGNED gg_unaligned_22*)a.data)[i],((GG_UNALIGNED gg_unaligned_22*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b) {
return a == b;
}
static void gg_render_22(gg_reference out, gg_type_22 value) {
gg_render_21(out,*value);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((gg_type_23*)a.data)[i],((gg_type_23*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b) {
if(!gg_eq_24(a.f1,b.f1))return 0;
if(!gg_eq_17(a.f2,b.f2))return 0;
if(!gg_eq_5(a.f3,b.f3))return 0;
if(!gg_eq_25(a.f4,b.f4))return 0;
if(!gg_eq_26(a.f5,b.f5))return 0;
if(!gg_eq_26(a.f6,b.f6))return 0;
return 1;
}
static void gg_render_23(gg_reference out, gg_type_23 value) {
gg_append(out,"\102\171\164\145\123\145\164\040\173\040",10,1);
gg_render_24(out, value.f1);
gg_append(out,", ",2,1);
gg_render_17(out, value.f2);
gg_append(out,", ",2,1);
gg_render_5(out, value.f3);
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f4).data, 3));
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f5).data, 16));
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f6).data, 16));
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((gg_type_24*)a.data)[i],((gg_type_24*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b) {
return gg_equal_elements_5(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_24(gg_reference out, gg_type_24 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((uint8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((gg_type_25*)a.data)[i],((gg_type_25*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b) {
return gg_equal_elements_17(gg_view((a).data, 3),gg_view((b).data, 3));
}
static void gg_render_25(gg_reference out, gg_type_25 value) {
gg_text(out,gg_view((value).data, 3));
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((gg_type_26*)a.data)[i],((gg_type_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b) {
return gg_equal_elements_17(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_26(gg_reference out, gg_type_26 value) {
gg_text(out,gg_view((value).data, 16));
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((GG_UNALIGNED gg_unaligned_27*)a.data)[i],((GG_UNALIGNED gg_unaligned_27*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b) {
return a == b;
}
static void gg_render_27(gg_reference out, gg_type_27 value) {
gg_render_24(out,*value);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((GG_UNALIGNED gg_unaligned_28*)a.data)[i],((GG_UNALIGNED gg_unaligned_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
return a == b;
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
gg_render_23(out,*value);
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((GG_UNALIGNED gg_unaligned_29*)a.data)[i],((GG_UNALIGNED gg_unaligned_29*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(gg_slice a, gg_slice b) {
return gg_equal_elements_17(a,b);
}
static void gg_render_29(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_30(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_30(((GG_UNALIGNED gg_unaligned_30*)a.data)[i],((GG_UNALIGNED gg_unaligned_30*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_30(gg_type_30 a, gg_type_30 b) {
return a == b;
}
static void gg_render_30(gg_reference out, gg_type_30 value) {
gg_render_17(out,*value);
}
static uint8_t gg_equal_elements_31(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_31(((gg_header*)a.data)[i],((gg_header*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_31(gg_header a, gg_header b) {
return gg_equal_elements_17(gg_view((a).base, (a).len),gg_view((b).base, (b).len));
}
static void gg_render_31(gg_reference out, gg_header value) {
gg_text(out,gg_view((value).base, (value).len));
}
static uint8_t gg_equal_elements_32(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_32(((GG_UNALIGNED gg_unaligned_32*)a.data)[i],((GG_UNALIGNED gg_unaligned_32*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_32(gg_reference a, gg_reference b) {
return a.header == b.header;
}
static void gg_render_32(gg_reference out, gg_reference value) {
gg_render_31(out,*value.header);
}
static uint8_t gg_equal_elements_33(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_33(((gg_type_33*)a.data)[i],((gg_type_33*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_33(gg_type_33 a, gg_type_33 b) {
  if ((uint64_t)(a).len > UINT64_C(64)) gg_fail("invalid limited array length");
  if ((uint64_t)(b).len > UINT64_C(64)) gg_fail("invalid limited array length");
return gg_equal_elements_17(gg_view((a).data, (a).len),gg_view((b).data, (b).len));
}
static void gg_render_33(gg_reference out, gg_type_33 value) {
  if ((uint64_t)(value).len > UINT64_C(64)) gg_fail("invalid limited array length");
gg_text(out,gg_view((value).data, (value).len));
}
static uint8_t gg_equal_elements_34(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_34(((gg_type_34*)a.data)[i],((gg_type_34*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_34(gg_type_34 a, gg_type_34 b) {
return gg_equal_elements_17(gg_view((a).data, 0),gg_view((b).data, 0));
}
static void gg_render_34(gg_reference out, gg_type_34 value) {
gg_text(out,gg_view((value).data, 0));
}
static uint8_t gg_equal_elements_35(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_35(((GG_UNALIGNED gg_unaligned_35*)a.data)[i],((GG_UNALIGNED gg_unaligned_35*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b) {
return a == b;
}
static void gg_render_35(gg_reference out, gg_type_35 value) {
gg_render_33(out,*value);
}
static uint8_t gg_equal_elements_36(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_36(((gg_header*)a.data)[i],((gg_header*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_36(gg_header a, gg_header b) {
return gg_equal_elements_29(gg_view((a).base, (a).len),gg_view((b).base, (b).len));
}
static void gg_render_36(gg_reference out, gg_header value) {
int64_t i; gg_slice view=gg_view((value).base, (value).len);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, ((GG_UNALIGNED gg_unaligned_29*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_37(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_37(((gg_type_37*)a.data)[i],((gg_type_37*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_37(gg_type_37 a, gg_type_37 b) {
return gg_equal_elements_29(gg_view((a).data, 0),gg_view((b).data, 0));
}
static void gg_render_37(gg_reference out, gg_type_37 value) {
int64_t i; gg_slice view=gg_view((value).data, 0);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, ((GG_UNALIGNED gg_unaligned_29*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_38(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_38(((GG_UNALIGNED gg_unaligned_38*)a.data)[i],((GG_UNALIGNED gg_unaligned_38*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_38(gg_type_38 a, gg_type_38 b) {
return a == b;
}
static void gg_render_38(gg_reference out, gg_type_38 value) {
gg_render_29(out,*value);
}
struct gg_frame_24;
struct gg_frame_25;
struct gg_frame_26;
struct gg_frame_27;
struct gg_frame_31;
struct gg_frame_32;
struct gg_frame_33;
struct gg_frame_34;
struct gg_frame_35;
struct gg_frame_36;
struct gg_frame_38;
struct gg_frame_39;
struct gg_frame_40;
struct gg_frame_41;
struct gg_frame_42;
struct gg_frame_43;
static struct {
  unsigned char dummy;
  _Alignas(double) gg_type_2 v136;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(uint64_t p0, int64_t p1, int64_t base);
static void gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static int64_t gg_function_5(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static int64_t gg_function_6(int64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_10(int64_t p0, int64_t base);
static int64_t gg_function_11(int64_t p0, int64_t base);
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_13(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_14(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_15(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_16(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_17(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_18(uint64_t p0, int64_t p1, uint64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_19(uint64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_20(uint64_t p0, int64_t p1, int64_t base);
static void gg_function_21(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_23(uint64_t p0, int64_t p1, uint64_t p2, int64_t p3, uint64_t p4, int64_t p5, int64_t base);
struct gg_frame_24 {
  int64_t base;
  uint64_t v243;
  int64_t v244;
  _Alignas(int64_t) gg_type_7 v245;
  _Alignas(int64_t) gg_type_7 v247;
  int64_t v249;
  _Alignas(int64_t) gg_type_7 v606;
  _Alignas(int64_t) gg_type_7 v608;
  int64_t v610;
  int64_t v613;
  int64_t v614;
  int64_t v615;
  int64_t v616;
  int64_t v617;
};
static int64_t gg_function_24(uint64_t p243, int64_t p244, int64_t base);
struct gg_frame_25 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v257;
  int64_t v258;
  int64_t v275;
  int64_t v311;
  gg_type_9 v313;
  int64_t v315;
  int64_t v316;
  gg_slice v317;
  int64_t v478;
  int64_t v479;
  int64_t v480;
  int64_t v498;
};
static void gg_function_25(gg_slice p257, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_26 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v264;
  int64_t v265;
  int64_t v266;
  int64_t v267;
  int64_t v268;
  int64_t v269;
  int64_t v270;
  int64_t v271;
  int64_t v272;
};
static void gg_function_26(gg_slice p264, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_27 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v281;
  int64_t v282;
  int64_t v283;
  int64_t v284;
  int64_t v285;
  int64_t v286;
  int64_t v287;
  int64_t v288;
  int64_t v289;
  int64_t v290;
  int64_t v306;
  int64_t v307;
  int64_t v308;
  int64_t v309;
};
static int64_t gg_function_27(gg_slice p281, struct gg_frame_24 *fv_frame_24, int64_t base);
static void gg_function_28(gg_slice p0, int64_t base);
static void gg_function_29(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
struct gg_frame_31 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v325;
  int64_t v326;
  int64_t v327;
  int64_t v328;
  int64_t v329;
  int64_t v358;
  int64_t v359;
  int64_t v388;
  int64_t v417;
};
static int64_t gg_function_31(gg_slice p325, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_32 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v335;
  int64_t v336;
  int64_t v337;
  int64_t v338;
  uint8_t v339;
  int64_t v340;
  int64_t v341;
  int64_t v342;
  int64_t v343;
  int64_t v344;
  int64_t v345;
  int64_t v346;
  int64_t v347;
  int64_t v348;
  int64_t v349;
  int64_t v350;
  int64_t v351;
  int64_t v352;
  int64_t v353;
  int64_t v354;
  int64_t v355;
  int64_t v356;
  int64_t v357;
};
static int64_t gg_function_32(gg_slice p335, int64_t p336, int64_t p337, int64_t p338, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_33 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v365;
  int64_t v366;
  int64_t v367;
  int64_t v368;
  uint8_t v369;
  int64_t v370;
  int64_t v371;
  int64_t v372;
  int64_t v373;
  int64_t v374;
  int64_t v375;
  int64_t v376;
  int64_t v377;
  int64_t v378;
  int64_t v379;
  int64_t v380;
  int64_t v381;
  int64_t v382;
  int64_t v383;
  int64_t v384;
  int64_t v385;
  int64_t v386;
  int64_t v387;
};
static int64_t gg_function_33(gg_slice p365, int64_t p366, int64_t p367, int64_t p368, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_34 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v394;
  int64_t v395;
  int64_t v396;
  int64_t v397;
  uint8_t v398;
  int64_t v399;
  int64_t v400;
  int64_t v401;
  int64_t v402;
  int64_t v403;
  int64_t v404;
  int64_t v405;
  int64_t v406;
  int64_t v407;
  int64_t v408;
  int64_t v409;
  int64_t v410;
  int64_t v411;
  int64_t v412;
  int64_t v413;
  int64_t v414;
  int64_t v415;
  int64_t v416;
};
static int64_t gg_function_34(gg_slice p394, int64_t p395, int64_t p396, int64_t p397, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_35 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v423;
  int64_t v424;
  int64_t v425;
  int64_t v426;
  uint8_t v427;
  int64_t v428;
  int64_t v429;
  int64_t v430;
  int64_t v431;
  int64_t v432;
  int64_t v433;
  int64_t v434;
  int64_t v435;
  int64_t v436;
  int64_t v437;
  int64_t v438;
  int64_t v439;
  int64_t v440;
  int64_t v441;
  int64_t v442;
  int64_t v443;
  int64_t v444;
  int64_t v445;
};
static int64_t gg_function_35(gg_slice p423, int64_t p424, int64_t p425, int64_t p426, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_36 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v451;
  int64_t v452;
  int64_t v453;
  int64_t v454;
  uint8_t v455;
  int64_t v456;
  int64_t v457;
  int64_t v458;
  int64_t v459;
  int64_t v460;
  int64_t v461;
  int64_t v462;
  int64_t v463;
  int64_t v464;
  int64_t v465;
  int64_t v466;
  int64_t v467;
  int64_t v468;
  int64_t v469;
  int64_t v470;
  int64_t v471;
  int64_t v472;
  int64_t v473;
};
static int64_t gg_function_36(gg_slice p451, int64_t p452, int64_t p453, int64_t p454, struct gg_frame_24 *fv_frame_24, int64_t base);
static void gg_function_37(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
struct gg_frame_38 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v486;
  int64_t v487;
  int64_t v488;
  int64_t v489;
  int64_t v490;
  int64_t v491;
  int64_t v492;
  int64_t v493;
  int64_t v494;
};
static int64_t gg_function_38(gg_slice p486, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_39 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v504;
  int64_t v505;
  int64_t v506;
  int64_t v507;
  int64_t v508;
  int64_t v509;
  int64_t v510;
  int64_t v511;
  int64_t v512;
};
static int64_t gg_function_39(gg_slice p504, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_40 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v527;
  int64_t v528;
  int64_t v556;
};
static void gg_function_40(gg_slice p527, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_41 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v534;
  int64_t v535;
  int64_t v536;
  int64_t v537;
  int64_t v538;
  int64_t v539;
  int64_t v540;
  int64_t v541;
  int64_t v542;
  int64_t v543;
  int64_t v544;
  int64_t v545;
  int64_t v546;
  int64_t v547;
  int64_t v548;
  int64_t v549;
  int64_t v550;
  int64_t v551;
  int64_t v552;
  int64_t v553;
};
static void gg_function_41(gg_slice p534, int64_t p535, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_42 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v564;
  int64_t v565;
  int64_t v566;
  int64_t v567;
  int64_t v568;
  int64_t v569;
  int64_t v570;
  int64_t v571;
  int64_t v572;
  int64_t v573;
  int64_t v574;
  int64_t v575;
  int64_t v576;
  int64_t v577;
  int64_t v578;
  int64_t v579;
  int64_t v580;
  int64_t v581;
  int64_t v582;
  int64_t v583;
};
static void gg_function_42(gg_slice p564, int64_t p565, struct gg_frame_24 *fv_frame_24, int64_t base);
struct gg_frame_43 {
  int64_t base;
  struct gg_frame_24 *fv_frame_24;
  gg_slice v591;
  int64_t v592;
  int64_t v593;
  int64_t v594;
  int64_t v595;
  int64_t v596;
  int64_t v597;
  int64_t v598;
  int64_t v599;
};
static void gg_function_43(gg_slice p591, struct gg_frame_24 *fv_frame_24, int64_t base);
static void gg_function_43(gg_slice p591, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_43 frame;
  int64_t t0;
  int64_t t1;
  int64_t t2;
  gg_slice t3;
  int64_t t4;
  GG_UNALIGNED gg_unaligned_4 * t5;
  int64_t t6;
  int64_t t7;
  gg_slice t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  GG_UNALIGNED gg_unaligned_4 * t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  gg_slice t19;
  int64_t t20;
  GG_UNALIGNED gg_unaligned_4 * t21;
  int64_t t22;
  gg_slice t23;
  int64_t t24;
  GG_UNALIGNED gg_unaligned_4 * t25;
  int64_t t26;
  uint8_t t27;
  uint8_t t28;
  gg_slice t29;
  int64_t t30;
  GG_UNALIGNED gg_unaligned_4 * t31;
  int64_t t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  GG_UNALIGNED gg_unaligned_4 * t36;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v591 = p591;
  {
  t1 = ((int64_t)UINT64_C(1));
  t2 = ((&frame)->v591).len;
  t0 = t1;
label0:;
  if (t0 >= t2) goto label2;
  (&frame)->v592 = t0;
  {
  t3 = (&frame)->v591;
  t4 = (&frame)->v592;
  t5 = ((GG_UNALIGNED gg_unaligned_4 *)t3.data) + gg_index(t4, t3.len);
  (&frame)->v593 = ((int64_t)((*t5)));
  (&frame)->v594 = ((int64_t)((&frame)->v592));
label4:;
  t6 = ((int64_t)((&frame)->v594));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t6) > (t7)))) goto label5;
  {
  t8 = (&frame)->v591;
  t9 = ((int64_t)((&frame)->v594));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((int64_t)((uint64_t)(t9) - (uint64_t)(t10)));
  t12 = ((GG_UNALIGNED gg_unaligned_4 *)t8.data) + gg_index(t11, t8.len);
  (&frame)->v595 = ((int64_t)((*t12)));
  t13 = ((int64_t)((&frame)->v593));
  t14 = ((int64_t)((&frame)->v595));
  (&frame)->v596 = t13;
  (&frame)->v597 = t14;
  {
  t16 = ((int64_t)((&frame)->v596));
  t17 = ((int64_t)((&frame)->v597));
  (&frame)->v598 = t16;
  (&frame)->v599 = t17;
  {
  t19 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t20 = (&frame)->v598;
  t21 = ((GG_UNALIGNED gg_unaligned_4 *)t19.data) + gg_index(t20, t19.len);
  t22 = ((int64_t)((*t21)));
  t23 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t24 = (&frame)->v599;
  t25 = ((GG_UNALIGNED gg_unaligned_4 *)t23.data) + gg_index(t24, t23.len);
  t26 = ((int64_t)((*t25)));
  t18 = ((t22) < (t26));
  }
  t15 = t18;
  }
  t27 = t15;
  t28 = (!(t27));
  if (t28) {
  {
  goto label5;
  }
  }
  t29 = (&frame)->v591;
  t30 = (&frame)->v594;
  t31 = ((GG_UNALIGNED gg_unaligned_4 *)t29.data) + gg_index(t30, t29.len);
  (*t31) = ((int64_t)((&frame)->v595));
  t32 = (&frame)->v594;
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v594 = ((int64_t)((uint64_t)(t32) - (uint64_t)(t33)));
  }
  goto label4;
label5:;
  t34 = (&frame)->v591;
  t35 = (&frame)->v594;
  t36 = ((GG_UNALIGNED gg_unaligned_4 *)t34.data) + gg_index(t35, t34.len);
  (*t36) = ((int64_t)((&frame)->v593));
  }
label1:;
  t0++; goto label0;
label2:;
  }
gg_done:;
}
static void gg_function_42(gg_slice p564, int64_t p565, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_42 frame;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  gg_slice t44;
  int64_t t45;
  GG_UNALIGNED gg_unaligned_4 * t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  uint8_t t59;
  gg_slice t60;
  int64_t t61;
  GG_UNALIGNED gg_unaligned_4 * t62;
  int64_t t63;
  gg_slice t64;
  int64_t t65;
  GG_UNALIGNED gg_unaligned_4 * t66;
  int64_t t67;
  uint8_t t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
  int64_t t72;
  int64_t t73;
  uint8_t t74;
  gg_slice t75;
  int64_t t76;
  GG_UNALIGNED gg_unaligned_4 * t77;
  int64_t t78;
  gg_slice t79;
  int64_t t80;
  GG_UNALIGNED gg_unaligned_4 * t81;
  int64_t t82;
  uint8_t t83;
  gg_slice t84;
  int64_t t85;
  GG_UNALIGNED gg_unaligned_4 * t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
  int64_t t93;
  int64_t t94;
  uint8_t t95;
  gg_slice t96;
  int64_t t97;
  GG_UNALIGNED gg_unaligned_4 * t98;
  int64_t t99;
  gg_slice t100;
  int64_t t101;
  GG_UNALIGNED gg_unaligned_4 * t102;
  int64_t t103;
  uint8_t t104;
  uint8_t t105;
  gg_slice t106;
  int64_t t107;
  GG_UNALIGNED gg_unaligned_4 * t108;
  gg_slice t109;
  int64_t t110;
  GG_UNALIGNED gg_unaligned_4 * t111;
  gg_slice t112;
  int64_t t113;
  GG_UNALIGNED gg_unaligned_4 * t114;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v564 = p564;
  (&frame)->v565 = p565;
  {
  (&frame)->v566 = ((int64_t)(((&frame)->v564).len));
  (&frame)->v567 = ((int64_t)((&frame)->v565));
  t37 = ((int64_t)(((int64_t)UINT64_C(2))));
  t38 = ((int64_t)((&frame)->v567));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  t42 = ((int64_t)((&frame)->v566));
  t43 = ((t41) >= (t42));
  if (t43) {
  {
  goto gg_done;
  }
  }
  t44 = (&frame)->v564;
  t45 = (&frame)->v567;
  t46 = ((GG_UNALIGNED gg_unaligned_4 *)t44.data) + gg_index(t45, t44.len);
  (&frame)->v568 = ((int64_t)((*t46)));
label7:;
  {
  t47 = ((int64_t)(((int64_t)UINT64_C(2))));
  t48 = ((int64_t)((&frame)->v567));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v569 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t50)))));
  t51 = ((int64_t)((&frame)->v569));
  t52 = ((int64_t)((&frame)->v566));
  t53 = ((t51) >= (t52));
  if (t53) {
  {
  goto label8;
  }
  }
  (&frame)->v570 = ((int64_t)((&frame)->v569));
  t54 = ((int64_t)((&frame)->v569));
  t55 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v571 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t55)))));
  t56 = ((int64_t)((&frame)->v571));
  t57 = ((int64_t)((&frame)->v566));
  t58 = ((t56) < (t57));
  t59 = t58;
  if (t59) {
  t60 = (&frame)->v564;
  t61 = (&frame)->v571;
  t62 = ((GG_UNALIGNED gg_unaligned_4 *)t60.data) + gg_index(t61, t60.len);
  t63 = ((int64_t)((*t62)));
  t64 = (&frame)->v564;
  t65 = (&frame)->v569;
  t66 = ((GG_UNALIGNED gg_unaligned_4 *)t64.data) + gg_index(t65, t64.len);
  t67 = ((int64_t)((*t66)));
  (&frame)->v572 = t63;
  (&frame)->v573 = t67;
  {
  t69 = ((int64_t)((&frame)->v573));
  t70 = ((int64_t)((&frame)->v572));
  (&frame)->v574 = t69;
  (&frame)->v575 = t70;
  {
  t72 = ((int64_t)((&frame)->v574));
  t73 = ((int64_t)((&frame)->v575));
  (&frame)->v576 = t72;
  (&frame)->v577 = t73;
  {
  t75 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t76 = (&frame)->v576;
  t77 = ((GG_UNALIGNED gg_unaligned_4 *)t75.data) + gg_index(t76, t75.len);
  t78 = ((int64_t)((*t77)));
  t79 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t80 = (&frame)->v577;
  t81 = ((GG_UNALIGNED gg_unaligned_4 *)t79.data) + gg_index(t80, t79.len);
  t82 = ((int64_t)((*t81)));
  t74 = ((t78) < (t82));
  }
  t71 = t74;
  }
  t68 = t71;
  }
  t59 = t68;
  }
  t83 = t59;
  if (t83) {
  {
  (&frame)->v570 = ((int64_t)((&frame)->v571));
  }
  }
  t84 = (&frame)->v564;
  t85 = (&frame)->v570;
  t86 = ((GG_UNALIGNED gg_unaligned_4 *)t84.data) + gg_index(t85, t84.len);
  t87 = ((int64_t)((*t86)));
  t88 = ((int64_t)((&frame)->v568));
  (&frame)->v578 = t87;
  (&frame)->v579 = t88;
  {
  t90 = ((int64_t)((&frame)->v579));
  t91 = ((int64_t)((&frame)->v578));
  (&frame)->v580 = t90;
  (&frame)->v581 = t91;
  {
  t93 = ((int64_t)((&frame)->v580));
  t94 = ((int64_t)((&frame)->v581));
  (&frame)->v582 = t93;
  (&frame)->v583 = t94;
  {
  t96 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t97 = (&frame)->v582;
  t98 = ((GG_UNALIGNED gg_unaligned_4 *)t96.data) + gg_index(t97, t96.len);
  t99 = ((int64_t)((*t98)));
  t100 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t101 = (&frame)->v583;
  t102 = ((GG_UNALIGNED gg_unaligned_4 *)t100.data) + gg_index(t101, t100.len);
  t103 = ((int64_t)((*t102)));
  t95 = ((t99) < (t103));
  }
  t92 = t95;
  }
  t89 = t92;
  }
  t104 = t89;
  t105 = (!(t104));
  if (t105) {
  {
  goto label8;
  }
  }
  t106 = (&frame)->v564;
  t107 = (&frame)->v567;
  t108 = ((GG_UNALIGNED gg_unaligned_4 *)t106.data) + gg_index(t107, t106.len);
  t109 = (&frame)->v564;
  t110 = (&frame)->v570;
  t111 = ((GG_UNALIGNED gg_unaligned_4 *)t109.data) + gg_index(t110, t109.len);
  (*t108) = ((int64_t)((*t111)));
  (&frame)->v567 = ((int64_t)((&frame)->v570));
  }
  goto label7;
label8:;
  t112 = (&frame)->v564;
  t113 = (&frame)->v567;
  t114 = ((GG_UNALIGNED gg_unaligned_4 *)t112.data) + gg_index(t113, t112.len);
  (*t114) = ((int64_t)((&frame)->v568));
  }
gg_done:;
}
static void gg_function_37(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  GG_UNALIGNED gg_unaligned_4 * t1;
  int64_t t2;
  GG_UNALIGNED gg_unaligned_4 * t3;
  int64_t t4;
  GG_UNALIGNED gg_unaligned_4 * t5;
  int64_t t6;
  GG_UNALIGNED gg_unaligned_4 * t7;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t0 = v1;
  t1 = ((GG_UNALIGNED gg_unaligned_4 *)(v0).data) + gg_index(t0, (v0).len);
  v3 = ((int64_t)((*t1)));
  /* FG_DIRECT_SLICE_INDEX */
  t2 = v1;
  t3 = ((GG_UNALIGNED gg_unaligned_4 *)(v0).data) + gg_index(t2, (v0).len);
  /* FG_DIRECT_SLICE_INDEX */
  t4 = v2;
  t5 = ((GG_UNALIGNED gg_unaligned_4 *)(v0).data) + gg_index(t4, (v0).len);
  (*t3) = ((int64_t)((*t5)));
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((GG_UNALIGNED gg_unaligned_4 *)(v0).data) + gg_index(t6, (v0).len);
  (*t7) = ((int64_t)(v3));
  }
gg_done:;
}
static void gg_function_41(gg_slice p534, int64_t p535, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_41 frame;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  gg_slice t15;
  int64_t t16;
  GG_UNALIGNED gg_unaligned_4 * t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  uint8_t t30;
  gg_slice t31;
  int64_t t32;
  GG_UNALIGNED gg_unaligned_4 * t33;
  int64_t t34;
  gg_slice t35;
  int64_t t36;
  GG_UNALIGNED gg_unaligned_4 * t37;
  int64_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  gg_slice t46;
  int64_t t47;
  GG_UNALIGNED gg_unaligned_4 * t48;
  int64_t t49;
  gg_slice t50;
  int64_t t51;
  GG_UNALIGNED gg_unaligned_4 * t52;
  int64_t t53;
  uint8_t t54;
  gg_slice t55;
  int64_t t56;
  GG_UNALIGNED gg_unaligned_4 * t57;
  int64_t t58;
  int64_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  gg_slice t67;
  int64_t t68;
  GG_UNALIGNED gg_unaligned_4 * t69;
  int64_t t70;
  gg_slice t71;
  int64_t t72;
  GG_UNALIGNED gg_unaligned_4 * t73;
  int64_t t74;
  uint8_t t75;
  uint8_t t76;
  gg_slice t77;
  int64_t t78;
  GG_UNALIGNED gg_unaligned_4 * t79;
  gg_slice t80;
  int64_t t81;
  GG_UNALIGNED gg_unaligned_4 * t82;
  gg_slice t83;
  int64_t t84;
  GG_UNALIGNED gg_unaligned_4 * t85;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v534 = p534;
  (&frame)->v535 = p535;
  {
  (&frame)->v536 = ((int64_t)(((&frame)->v534).len));
  (&frame)->v537 = ((int64_t)((&frame)->v535));
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  t9 = ((int64_t)((&frame)->v537));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)((&frame)->v536));
  t14 = ((t12) >= (t13));
  if (t14) {
  {
  goto gg_done;
  }
  }
  t15 = (&frame)->v534;
  t16 = (&frame)->v537;
  t17 = ((GG_UNALIGNED gg_unaligned_4 *)t15.data) + gg_index(t16, t15.len);
  (&frame)->v538 = ((int64_t)((*t17)));
label1:;
  {
  t18 = ((int64_t)(((int64_t)UINT64_C(2))));
  t19 = ((int64_t)((&frame)->v537));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v539 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t21)))));
  t22 = ((int64_t)((&frame)->v539));
  t23 = ((int64_t)((&frame)->v536));
  t24 = ((t22) >= (t23));
  if (t24) {
  {
  goto label2;
  }
  }
  (&frame)->v540 = ((int64_t)((&frame)->v539));
  t25 = ((int64_t)((&frame)->v539));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v541 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  t27 = ((int64_t)((&frame)->v541));
  t28 = ((int64_t)((&frame)->v536));
  t29 = ((t27) < (t28));
  t30 = t29;
  if (t30) {
  t31 = (&frame)->v534;
  t32 = (&frame)->v541;
  t33 = ((GG_UNALIGNED gg_unaligned_4 *)t31.data) + gg_index(t32, t31.len);
  t34 = ((int64_t)((*t33)));
  t35 = (&frame)->v534;
  t36 = (&frame)->v539;
  t37 = ((GG_UNALIGNED gg_unaligned_4 *)t35.data) + gg_index(t36, t35.len);
  t38 = ((int64_t)((*t37)));
  (&frame)->v542 = t34;
  (&frame)->v543 = t38;
  {
  t40 = ((int64_t)((&frame)->v543));
  t41 = ((int64_t)((&frame)->v542));
  (&frame)->v544 = t40;
  (&frame)->v545 = t41;
  {
  t43 = ((int64_t)((&frame)->v544));
  t44 = ((int64_t)((&frame)->v545));
  (&frame)->v546 = t43;
  (&frame)->v547 = t44;
  {
  t46 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t47 = (&frame)->v546;
  t48 = ((GG_UNALIGNED gg_unaligned_4 *)t46.data) + gg_index(t47, t46.len);
  t49 = ((int64_t)((*t48)));
  t50 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t51 = (&frame)->v547;
  t52 = ((GG_UNALIGNED gg_unaligned_4 *)t50.data) + gg_index(t51, t50.len);
  t53 = ((int64_t)((*t52)));
  t45 = ((t49) < (t53));
  }
  t42 = t45;
  }
  t39 = t42;
  }
  t30 = t39;
  }
  t54 = t30;
  if (t54) {
  {
  (&frame)->v540 = ((int64_t)((&frame)->v541));
  }
  }
  t55 = (&frame)->v534;
  t56 = (&frame)->v540;
  t57 = ((GG_UNALIGNED gg_unaligned_4 *)t55.data) + gg_index(t56, t55.len);
  t58 = ((int64_t)((*t57)));
  t59 = ((int64_t)((&frame)->v538));
  (&frame)->v548 = t58;
  (&frame)->v549 = t59;
  {
  t61 = ((int64_t)((&frame)->v549));
  t62 = ((int64_t)((&frame)->v548));
  (&frame)->v550 = t61;
  (&frame)->v551 = t62;
  {
  t64 = ((int64_t)((&frame)->v550));
  t65 = ((int64_t)((&frame)->v551));
  (&frame)->v552 = t64;
  (&frame)->v553 = t65;
  {
  t67 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t68 = (&frame)->v552;
  t69 = ((GG_UNALIGNED gg_unaligned_4 *)t67.data) + gg_index(t68, t67.len);
  t70 = ((int64_t)((*t69)));
  t71 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t72 = (&frame)->v553;
  t73 = ((GG_UNALIGNED gg_unaligned_4 *)t71.data) + gg_index(t72, t71.len);
  t74 = ((int64_t)((*t73)));
  t66 = ((t70) < (t74));
  }
  t63 = t66;
  }
  t60 = t63;
  }
  t75 = t60;
  t76 = (!(t75));
  if (t76) {
  {
  goto label2;
  }
  }
  t77 = (&frame)->v534;
  t78 = (&frame)->v537;
  t79 = ((GG_UNALIGNED gg_unaligned_4 *)t77.data) + gg_index(t78, t77.len);
  t80 = (&frame)->v534;
  t81 = (&frame)->v540;
  t82 = ((GG_UNALIGNED gg_unaligned_4 *)t80.data) + gg_index(t81, t80.len);
  (*t79) = ((int64_t)((*t82)));
  (&frame)->v537 = ((int64_t)((&frame)->v540));
  }
  goto label1;
label2:;
  t83 = (&frame)->v534;
  t84 = (&frame)->v537;
  t85 = ((GG_UNALIGNED gg_unaligned_4 *)t83.data) + gg_index(t84, t83.len);
  (*t85) = ((int64_t)((&frame)->v538));
  }
gg_done:;
}
static void gg_function_40(gg_slice p527, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_40 frame;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  gg_slice t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  gg_slice t98;
  int64_t t99;
  int64_t t100;
  gg_slice t101;
  int64_t t102;
  gg_slice t103;
  int64_t t104;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v527 = p527;
  {
  t86 = ((int64_t)(((&frame)->v527).len));
  t87 = ((int64_t)(((int64_t)UINT64_C(2))));
  (&frame)->v528 = ((int64_t)(((int64_t)gg_div(t86, t87))));
label4:;
  t88 = ((int64_t)((&frame)->v528));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t88) > (t89)))) goto label5;
  {
  t90 = (&frame)->v528;
  t91 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v528 = ((int64_t)((uint64_t)(t90) - (uint64_t)(t91)));
  t92 = (&frame)->v527;
  t93 = ((int64_t)((&frame)->v528));
  gg_function_41(t92, t93, (&frame)->fv_frame_24, frame.base + 0);
  }
  goto label4;
label5:;
  (&frame)->v556 = ((int64_t)(((&frame)->v527).len));
label7:;
  t94 = ((int64_t)((&frame)->v556));
  t95 = ((int64_t)(((int64_t)UINT64_C(1))));
  if (!(((t94) > (t95)))) goto label8;
  {
  t96 = (&frame)->v556;
  t97 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v556 = ((int64_t)((uint64_t)(t96) - (uint64_t)(t97)));
  t98 = (&frame)->v527;
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = ((int64_t)((&frame)->v556));
  gg_function_37(t98, t99, t100, frame.base + 0);
  t101 = (&frame)->v527;
  t102 = (&frame)->v556;
  t103 = gg_sub(t101, 0, t102, sizeof(int64_t));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_42(t103, t104, (&frame)->fv_frame_24, frame.base + 0);
  }
  goto label7;
label8:;
  }
gg_done:;
}
static int64_t gg_function_39(gg_slice p504, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_39 frame;
  int64_t t105;
  gg_slice t106;
  int64_t t107;
  GG_UNALIGNED gg_unaligned_4 * t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  gg_slice t112;
  int64_t t113;
  GG_UNALIGNED gg_unaligned_4 * t114;
  gg_slice t115;
  int64_t t116;
  GG_UNALIGNED gg_unaligned_4 * t117;
  gg_slice t118;
  int64_t t119;
  GG_UNALIGNED gg_unaligned_4 * t120;
  gg_slice t121;
  int64_t t122;
  GG_UNALIGNED gg_unaligned_4 * t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint8_t t128;
  int64_t t129;
  int64_t t130;
  uint8_t t131;
  gg_slice t132;
  int64_t t133;
  GG_UNALIGNED gg_unaligned_4 * t134;
  int64_t t135;
  gg_slice t136;
  int64_t t137;
  GG_UNALIGNED gg_unaligned_4 * t138;
  int64_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v504 = p504;
  {
  t106 = (&frame)->v504;
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((GG_UNALIGNED gg_unaligned_4 *)t106.data) + gg_index(t107, t106.len);
  (&frame)->v505 = ((int64_t)((*t108)));
  (&frame)->v506 = ((int64_t)(((int64_t)UINT64_C(1))));
  t110 = ((int64_t)UINT64_C(1));
  t111 = ((&frame)->v504).len;
  t109 = t110;
label9:;
  if (t109 >= t111) goto label11;
  (&frame)->v507 = t109;
  {
  t112 = (&frame)->v504;
  t113 = (&frame)->v507;
  t114 = ((GG_UNALIGNED gg_unaligned_4 *)t112.data) + gg_index(t113, t112.len);
  (&frame)->v508 = ((int64_t)((*t114)));
  t115 = (&frame)->v504;
  t116 = (&frame)->v507;
  t117 = ((GG_UNALIGNED gg_unaligned_4 *)t115.data) + gg_index(t116, t115.len);
  t118 = (&frame)->v504;
  t119 = (&frame)->v506;
  t120 = ((GG_UNALIGNED gg_unaligned_4 *)t118.data) + gg_index(t119, t118.len);
  (*t117) = ((int64_t)((*t120)));
  t121 = (&frame)->v504;
  t122 = (&frame)->v506;
  t123 = ((GG_UNALIGNED gg_unaligned_4 *)t121.data) + gg_index(t122, t121.len);
  (*t123) = ((int64_t)((&frame)->v508));
  t124 = (&frame)->v506;
  t126 = ((int64_t)((&frame)->v508));
  t127 = ((int64_t)((&frame)->v505));
  (&frame)->v509 = t126;
  (&frame)->v510 = t127;
  {
  t129 = ((int64_t)((&frame)->v509));
  t130 = ((int64_t)((&frame)->v510));
  (&frame)->v511 = t129;
  (&frame)->v512 = t130;
  {
  t132 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t133 = (&frame)->v511;
  t134 = ((GG_UNALIGNED gg_unaligned_4 *)t132.data) + gg_index(t133, t132.len);
  t135 = ((int64_t)((*t134)));
  t136 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t137 = (&frame)->v512;
  t138 = ((GG_UNALIGNED gg_unaligned_4 *)t136.data) + gg_index(t137, t136.len);
  t139 = ((int64_t)((*t138)));
  t131 = ((t135) < (t139));
  }
  t128 = t131;
  }
  t140 = t128;
  if (t140) {
  {
  t125 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t141 = ((int64_t)(t125));
  (&frame)->v506 = ((int64_t)((uint64_t)(t124) + (uint64_t)(t141)));
  }
label10:;
  t109++; goto label9;
label11:;
  t142 = ((int64_t)((&frame)->v506));
  t143 = ((int64_t)(((int64_t)UINT64_C(1))));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t142) - (uint64_t)(t143)))));
  goto gg_done;
  }
gg_done:;
  return t105;
}
static int64_t gg_function_38(gg_slice p486, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_38 frame;
  int64_t t144;
  gg_slice t145;
  int64_t t146;
  GG_UNALIGNED gg_unaligned_4 * t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  gg_slice t151;
  int64_t t152;
  GG_UNALIGNED gg_unaligned_4 * t153;
  gg_slice t154;
  int64_t t155;
  GG_UNALIGNED gg_unaligned_4 * t156;
  gg_slice t157;
  int64_t t158;
  GG_UNALIGNED gg_unaligned_4 * t159;
  gg_slice t160;
  int64_t t161;
  GG_UNALIGNED gg_unaligned_4 * t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  uint8_t t167;
  int64_t t168;
  int64_t t169;
  uint8_t t170;
  gg_slice t171;
  int64_t t172;
  GG_UNALIGNED gg_unaligned_4 * t173;
  int64_t t174;
  gg_slice t175;
  int64_t t176;
  GG_UNALIGNED gg_unaligned_4 * t177;
  int64_t t178;
  uint8_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v486 = p486;
  {
  t145 = (&frame)->v486;
  t146 = ((int64_t)UINT64_C(0));
  t147 = ((GG_UNALIGNED gg_unaligned_4 *)t145.data) + gg_index(t146, t145.len);
  (&frame)->v487 = ((int64_t)((*t147)));
  (&frame)->v488 = ((int64_t)(((int64_t)UINT64_C(1))));
  t149 = ((int64_t)UINT64_C(1));
  t150 = ((&frame)->v486).len;
  t148 = t149;
label12:;
  if (t148 >= t150) goto label14;
  (&frame)->v489 = t148;
  {
  t151 = (&frame)->v486;
  t152 = (&frame)->v489;
  t153 = ((GG_UNALIGNED gg_unaligned_4 *)t151.data) + gg_index(t152, t151.len);
  (&frame)->v490 = ((int64_t)((*t153)));
  t154 = (&frame)->v486;
  t155 = (&frame)->v489;
  t156 = ((GG_UNALIGNED gg_unaligned_4 *)t154.data) + gg_index(t155, t154.len);
  t157 = (&frame)->v486;
  t158 = (&frame)->v488;
  t159 = ((GG_UNALIGNED gg_unaligned_4 *)t157.data) + gg_index(t158, t157.len);
  (*t156) = ((int64_t)((*t159)));
  t160 = (&frame)->v486;
  t161 = (&frame)->v488;
  t162 = ((GG_UNALIGNED gg_unaligned_4 *)t160.data) + gg_index(t161, t160.len);
  (*t162) = ((int64_t)((&frame)->v490));
  t163 = (&frame)->v488;
  t165 = ((int64_t)((&frame)->v490));
  t166 = ((int64_t)((&frame)->v487));
  (&frame)->v491 = t165;
  (&frame)->v492 = t166;
  {
  t168 = ((int64_t)((&frame)->v492));
  t169 = ((int64_t)((&frame)->v491));
  (&frame)->v493 = t168;
  (&frame)->v494 = t169;
  {
  t171 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t172 = (&frame)->v493;
  t173 = ((GG_UNALIGNED gg_unaligned_4 *)t171.data) + gg_index(t172, t171.len);
  t174 = ((int64_t)((*t173)));
  t175 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t176 = (&frame)->v494;
  t177 = ((GG_UNALIGNED gg_unaligned_4 *)t175.data) + gg_index(t176, t175.len);
  t178 = ((int64_t)((*t177)));
  t170 = ((t174) < (t178));
  }
  t179 = t170;
  t167 = (!(t179));
  }
  t180 = t167;
  if (t180) {
  {
  t164 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t164 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t181 = ((int64_t)(t164));
  (&frame)->v488 = ((int64_t)((uint64_t)(t163) + (uint64_t)(t181)));
  }
label13:;
  t148++; goto label12;
label14:;
  t182 = ((int64_t)((&frame)->v488));
  t183 = ((int64_t)(((int64_t)UINT64_C(1))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t182) - (uint64_t)(t183)))));
  goto gg_done;
  }
gg_done:;
  return t144;
}
static int64_t gg_function_36(gg_slice p451, int64_t p452, int64_t p453, int64_t p454, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_36 frame;
  int64_t t184;
  gg_slice t185;
  int64_t t186;
  GG_UNALIGNED gg_unaligned_4 * t187;
  int64_t t188;
  gg_slice t189;
  int64_t t190;
  GG_UNALIGNED gg_unaligned_4 * t191;
  int64_t t192;
  uint8_t t193;
  int64_t t194;
  int64_t t195;
  uint8_t t196;
  int64_t t197;
  int64_t t198;
  uint8_t t199;
  gg_slice t200;
  int64_t t201;
  GG_UNALIGNED gg_unaligned_4 * t202;
  int64_t t203;
  gg_slice t204;
  int64_t t205;
  GG_UNALIGNED gg_unaligned_4 * t206;
  int64_t t207;
  uint8_t t208;
  gg_slice t209;
  int64_t t210;
  GG_UNALIGNED gg_unaligned_4 * t211;
  int64_t t212;
  gg_slice t213;
  int64_t t214;
  GG_UNALIGNED gg_unaligned_4 * t215;
  int64_t t216;
  uint8_t t217;
  int64_t t218;
  int64_t t219;
  uint8_t t220;
  int64_t t221;
  int64_t t222;
  uint8_t t223;
  gg_slice t224;
  int64_t t225;
  GG_UNALIGNED gg_unaligned_4 * t226;
  int64_t t227;
  gg_slice t228;
  int64_t t229;
  GG_UNALIGNED gg_unaligned_4 * t230;
  int64_t t231;
  uint8_t t232;
  uint8_t t233;
  gg_slice t234;
  int64_t t235;
  GG_UNALIGNED gg_unaligned_4 * t236;
  int64_t t237;
  gg_slice t238;
  int64_t t239;
  GG_UNALIGNED gg_unaligned_4 * t240;
  int64_t t241;
  uint8_t t242;
  int64_t t243;
  int64_t t244;
  uint8_t t245;
  int64_t t246;
  int64_t t247;
  uint8_t t248;
  gg_slice t249;
  int64_t t250;
  GG_UNALIGNED gg_unaligned_4 * t251;
  int64_t t252;
  gg_slice t253;
  int64_t t254;
  GG_UNALIGNED gg_unaligned_4 * t255;
  int64_t t256;
  uint8_t t257;
  uint8_t t258;
  uint8_t t259;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v451 = p451;
  (&frame)->v452 = p452;
  (&frame)->v453 = p453;
  (&frame)->v454 = p454;
  {
  t185 = (&frame)->v451;
  t186 = (&frame)->v452;
  t187 = ((GG_UNALIGNED gg_unaligned_4 *)t185.data) + gg_index(t186, t185.len);
  t188 = ((int64_t)((*t187)));
  t189 = (&frame)->v451;
  t190 = (&frame)->v453;
  t191 = ((GG_UNALIGNED gg_unaligned_4 *)t189.data) + gg_index(t190, t189.len);
  t192 = ((int64_t)((*t191)));
  (&frame)->v456 = t188;
  (&frame)->v457 = t192;
  {
  t194 = ((int64_t)((&frame)->v456));
  t195 = ((int64_t)((&frame)->v457));
  (&frame)->v458 = t194;
  (&frame)->v459 = t195;
  {
  t197 = ((int64_t)((&frame)->v458));
  t198 = ((int64_t)((&frame)->v459));
  (&frame)->v460 = t197;
  (&frame)->v461 = t198;
  {
  t200 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t201 = (&frame)->v460;
  t202 = ((GG_UNALIGNED gg_unaligned_4 *)t200.data) + gg_index(t201, t200.len);
  t203 = ((int64_t)((*t202)));
  t204 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t205 = (&frame)->v461;
  t206 = ((GG_UNALIGNED gg_unaligned_4 *)t204.data) + gg_index(t205, t204.len);
  t207 = ((int64_t)((*t206)));
  t199 = ((t203) < (t207));
  }
  t196 = t199;
  }
  t193 = t196;
  }
  (&frame)->v455 = t193;
  t208 = (&frame)->v455;
  t209 = (&frame)->v451;
  t210 = (&frame)->v452;
  t211 = ((GG_UNALIGNED gg_unaligned_4 *)t209.data) + gg_index(t210, t209.len);
  t212 = ((int64_t)((*t211)));
  t213 = (&frame)->v451;
  t214 = (&frame)->v454;
  t215 = ((GG_UNALIGNED gg_unaligned_4 *)t213.data) + gg_index(t214, t213.len);
  t216 = ((int64_t)((*t215)));
  (&frame)->v462 = t212;
  (&frame)->v463 = t216;
  {
  t218 = ((int64_t)((&frame)->v462));
  t219 = ((int64_t)((&frame)->v463));
  (&frame)->v464 = t218;
  (&frame)->v465 = t219;
  {
  t221 = ((int64_t)((&frame)->v464));
  t222 = ((int64_t)((&frame)->v465));
  (&frame)->v466 = t221;
  (&frame)->v467 = t222;
  {
  t224 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t225 = (&frame)->v466;
  t226 = ((GG_UNALIGNED gg_unaligned_4 *)t224.data) + gg_index(t225, t224.len);
  t227 = ((int64_t)((*t226)));
  t228 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t229 = (&frame)->v467;
  t230 = ((GG_UNALIGNED gg_unaligned_4 *)t228.data) + gg_index(t229, t228.len);
  t231 = ((int64_t)((*t230)));
  t223 = ((t227) < (t231));
  }
  t220 = t223;
  }
  t217 = t220;
  }
  t232 = t217;
  t233 = ((t208) != (t232));
  if (t233) {
  {
  t184 = ((int64_t)((&frame)->v452));
  goto gg_done;
  }
  }
  t234 = (&frame)->v451;
  t235 = (&frame)->v453;
  t236 = ((GG_UNALIGNED gg_unaligned_4 *)t234.data) + gg_index(t235, t234.len);
  t237 = ((int64_t)((*t236)));
  t238 = (&frame)->v451;
  t239 = (&frame)->v454;
  t240 = ((GG_UNALIGNED gg_unaligned_4 *)t238.data) + gg_index(t239, t238.len);
  t241 = ((int64_t)((*t240)));
  (&frame)->v468 = t237;
  (&frame)->v469 = t241;
  {
  t243 = ((int64_t)((&frame)->v468));
  t244 = ((int64_t)((&frame)->v469));
  (&frame)->v470 = t243;
  (&frame)->v471 = t244;
  {
  t246 = ((int64_t)((&frame)->v470));
  t247 = ((int64_t)((&frame)->v471));
  (&frame)->v472 = t246;
  (&frame)->v473 = t247;
  {
  t249 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t250 = (&frame)->v472;
  t251 = ((GG_UNALIGNED gg_unaligned_4 *)t249.data) + gg_index(t250, t249.len);
  t252 = ((int64_t)((*t251)));
  t253 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t254 = (&frame)->v473;
  t255 = ((GG_UNALIGNED gg_unaligned_4 *)t253.data) + gg_index(t254, t253.len);
  t256 = ((int64_t)((*t255)));
  t248 = ((t252) < (t256));
  }
  t245 = t248;
  }
  t242 = t245;
  }
  t257 = t242;
  t258 = (&frame)->v455;
  t259 = ((t257) != (t258));
  if (t259) {
  {
  t184 = ((int64_t)((&frame)->v454));
  goto gg_done;
  }
  }
  t184 = ((int64_t)((&frame)->v453));
  goto gg_done;
  }
gg_done:;
  return t184;
}
static int64_t gg_function_35(gg_slice p423, int64_t p424, int64_t p425, int64_t p426, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_35 frame;
  int64_t t260;
  gg_slice t261;
  int64_t t262;
  GG_UNALIGNED gg_unaligned_4 * t263;
  int64_t t264;
  gg_slice t265;
  int64_t t266;
  GG_UNALIGNED gg_unaligned_4 * t267;
  int64_t t268;
  uint8_t t269;
  int64_t t270;
  int64_t t271;
  uint8_t t272;
  int64_t t273;
  int64_t t274;
  uint8_t t275;
  gg_slice t276;
  int64_t t277;
  GG_UNALIGNED gg_unaligned_4 * t278;
  int64_t t279;
  gg_slice t280;
  int64_t t281;
  GG_UNALIGNED gg_unaligned_4 * t282;
  int64_t t283;
  uint8_t t284;
  gg_slice t285;
  int64_t t286;
  GG_UNALIGNED gg_unaligned_4 * t287;
  int64_t t288;
  gg_slice t289;
  int64_t t290;
  GG_UNALIGNED gg_unaligned_4 * t291;
  int64_t t292;
  uint8_t t293;
  int64_t t294;
  int64_t t295;
  uint8_t t296;
  int64_t t297;
  int64_t t298;
  uint8_t t299;
  gg_slice t300;
  int64_t t301;
  GG_UNALIGNED gg_unaligned_4 * t302;
  int64_t t303;
  gg_slice t304;
  int64_t t305;
  GG_UNALIGNED gg_unaligned_4 * t306;
  int64_t t307;
  uint8_t t308;
  uint8_t t309;
  gg_slice t310;
  int64_t t311;
  GG_UNALIGNED gg_unaligned_4 * t312;
  int64_t t313;
  gg_slice t314;
  int64_t t315;
  GG_UNALIGNED gg_unaligned_4 * t316;
  int64_t t317;
  uint8_t t318;
  int64_t t319;
  int64_t t320;
  uint8_t t321;
  int64_t t322;
  int64_t t323;
  uint8_t t324;
  gg_slice t325;
  int64_t t326;
  GG_UNALIGNED gg_unaligned_4 * t327;
  int64_t t328;
  gg_slice t329;
  int64_t t330;
  GG_UNALIGNED gg_unaligned_4 * t331;
  int64_t t332;
  uint8_t t333;
  uint8_t t334;
  uint8_t t335;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v423 = p423;
  (&frame)->v424 = p424;
  (&frame)->v425 = p425;
  (&frame)->v426 = p426;
  {
  t261 = (&frame)->v423;
  t262 = (&frame)->v424;
  t263 = ((GG_UNALIGNED gg_unaligned_4 *)t261.data) + gg_index(t262, t261.len);
  t264 = ((int64_t)((*t263)));
  t265 = (&frame)->v423;
  t266 = (&frame)->v425;
  t267 = ((GG_UNALIGNED gg_unaligned_4 *)t265.data) + gg_index(t266, t265.len);
  t268 = ((int64_t)((*t267)));
  (&frame)->v428 = t264;
  (&frame)->v429 = t268;
  {
  t270 = ((int64_t)((&frame)->v428));
  t271 = ((int64_t)((&frame)->v429));
  (&frame)->v430 = t270;
  (&frame)->v431 = t271;
  {
  t273 = ((int64_t)((&frame)->v430));
  t274 = ((int64_t)((&frame)->v431));
  (&frame)->v432 = t273;
  (&frame)->v433 = t274;
  {
  t276 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t277 = (&frame)->v432;
  t278 = ((GG_UNALIGNED gg_unaligned_4 *)t276.data) + gg_index(t277, t276.len);
  t279 = ((int64_t)((*t278)));
  t280 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t281 = (&frame)->v433;
  t282 = ((GG_UNALIGNED gg_unaligned_4 *)t280.data) + gg_index(t281, t280.len);
  t283 = ((int64_t)((*t282)));
  t275 = ((t279) < (t283));
  }
  t272 = t275;
  }
  t269 = t272;
  }
  (&frame)->v427 = t269;
  t284 = (&frame)->v427;
  t285 = (&frame)->v423;
  t286 = (&frame)->v424;
  t287 = ((GG_UNALIGNED gg_unaligned_4 *)t285.data) + gg_index(t286, t285.len);
  t288 = ((int64_t)((*t287)));
  t289 = (&frame)->v423;
  t290 = (&frame)->v426;
  t291 = ((GG_UNALIGNED gg_unaligned_4 *)t289.data) + gg_index(t290, t289.len);
  t292 = ((int64_t)((*t291)));
  (&frame)->v434 = t288;
  (&frame)->v435 = t292;
  {
  t294 = ((int64_t)((&frame)->v434));
  t295 = ((int64_t)((&frame)->v435));
  (&frame)->v436 = t294;
  (&frame)->v437 = t295;
  {
  t297 = ((int64_t)((&frame)->v436));
  t298 = ((int64_t)((&frame)->v437));
  (&frame)->v438 = t297;
  (&frame)->v439 = t298;
  {
  t300 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t301 = (&frame)->v438;
  t302 = ((GG_UNALIGNED gg_unaligned_4 *)t300.data) + gg_index(t301, t300.len);
  t303 = ((int64_t)((*t302)));
  t304 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t305 = (&frame)->v439;
  t306 = ((GG_UNALIGNED gg_unaligned_4 *)t304.data) + gg_index(t305, t304.len);
  t307 = ((int64_t)((*t306)));
  t299 = ((t303) < (t307));
  }
  t296 = t299;
  }
  t293 = t296;
  }
  t308 = t293;
  t309 = ((t284) != (t308));
  if (t309) {
  {
  t260 = ((int64_t)((&frame)->v424));
  goto gg_done;
  }
  }
  t310 = (&frame)->v423;
  t311 = (&frame)->v425;
  t312 = ((GG_UNALIGNED gg_unaligned_4 *)t310.data) + gg_index(t311, t310.len);
  t313 = ((int64_t)((*t312)));
  t314 = (&frame)->v423;
  t315 = (&frame)->v426;
  t316 = ((GG_UNALIGNED gg_unaligned_4 *)t314.data) + gg_index(t315, t314.len);
  t317 = ((int64_t)((*t316)));
  (&frame)->v440 = t313;
  (&frame)->v441 = t317;
  {
  t319 = ((int64_t)((&frame)->v440));
  t320 = ((int64_t)((&frame)->v441));
  (&frame)->v442 = t319;
  (&frame)->v443 = t320;
  {
  t322 = ((int64_t)((&frame)->v442));
  t323 = ((int64_t)((&frame)->v443));
  (&frame)->v444 = t322;
  (&frame)->v445 = t323;
  {
  t325 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t326 = (&frame)->v444;
  t327 = ((GG_UNALIGNED gg_unaligned_4 *)t325.data) + gg_index(t326, t325.len);
  t328 = ((int64_t)((*t327)));
  t329 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t330 = (&frame)->v445;
  t331 = ((GG_UNALIGNED gg_unaligned_4 *)t329.data) + gg_index(t330, t329.len);
  t332 = ((int64_t)((*t331)));
  t324 = ((t328) < (t332));
  }
  t321 = t324;
  }
  t318 = t321;
  }
  t333 = t318;
  t334 = (&frame)->v427;
  t335 = ((t333) != (t334));
  if (t335) {
  {
  t260 = ((int64_t)((&frame)->v426));
  goto gg_done;
  }
  }
  t260 = ((int64_t)((&frame)->v425));
  goto gg_done;
  }
gg_done:;
  return t260;
}
static int64_t gg_function_34(gg_slice p394, int64_t p395, int64_t p396, int64_t p397, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_34 frame;
  int64_t t336;
  gg_slice t337;
  int64_t t338;
  GG_UNALIGNED gg_unaligned_4 * t339;
  int64_t t340;
  gg_slice t341;
  int64_t t342;
  GG_UNALIGNED gg_unaligned_4 * t343;
  int64_t t344;
  uint8_t t345;
  int64_t t346;
  int64_t t347;
  uint8_t t348;
  int64_t t349;
  int64_t t350;
  uint8_t t351;
  gg_slice t352;
  int64_t t353;
  GG_UNALIGNED gg_unaligned_4 * t354;
  int64_t t355;
  gg_slice t356;
  int64_t t357;
  GG_UNALIGNED gg_unaligned_4 * t358;
  int64_t t359;
  uint8_t t360;
  gg_slice t361;
  int64_t t362;
  GG_UNALIGNED gg_unaligned_4 * t363;
  int64_t t364;
  gg_slice t365;
  int64_t t366;
  GG_UNALIGNED gg_unaligned_4 * t367;
  int64_t t368;
  uint8_t t369;
  int64_t t370;
  int64_t t371;
  uint8_t t372;
  int64_t t373;
  int64_t t374;
  uint8_t t375;
  gg_slice t376;
  int64_t t377;
  GG_UNALIGNED gg_unaligned_4 * t378;
  int64_t t379;
  gg_slice t380;
  int64_t t381;
  GG_UNALIGNED gg_unaligned_4 * t382;
  int64_t t383;
  uint8_t t384;
  uint8_t t385;
  gg_slice t386;
  int64_t t387;
  GG_UNALIGNED gg_unaligned_4 * t388;
  int64_t t389;
  gg_slice t390;
  int64_t t391;
  GG_UNALIGNED gg_unaligned_4 * t392;
  int64_t t393;
  uint8_t t394;
  int64_t t395;
  int64_t t396;
  uint8_t t397;
  int64_t t398;
  int64_t t399;
  uint8_t t400;
  gg_slice t401;
  int64_t t402;
  GG_UNALIGNED gg_unaligned_4 * t403;
  int64_t t404;
  gg_slice t405;
  int64_t t406;
  GG_UNALIGNED gg_unaligned_4 * t407;
  int64_t t408;
  uint8_t t409;
  uint8_t t410;
  uint8_t t411;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v394 = p394;
  (&frame)->v395 = p395;
  (&frame)->v396 = p396;
  (&frame)->v397 = p397;
  {
  t337 = (&frame)->v394;
  t338 = (&frame)->v395;
  t339 = ((GG_UNALIGNED gg_unaligned_4 *)t337.data) + gg_index(t338, t337.len);
  t340 = ((int64_t)((*t339)));
  t341 = (&frame)->v394;
  t342 = (&frame)->v396;
  t343 = ((GG_UNALIGNED gg_unaligned_4 *)t341.data) + gg_index(t342, t341.len);
  t344 = ((int64_t)((*t343)));
  (&frame)->v399 = t340;
  (&frame)->v400 = t344;
  {
  t346 = ((int64_t)((&frame)->v399));
  t347 = ((int64_t)((&frame)->v400));
  (&frame)->v401 = t346;
  (&frame)->v402 = t347;
  {
  t349 = ((int64_t)((&frame)->v401));
  t350 = ((int64_t)((&frame)->v402));
  (&frame)->v403 = t349;
  (&frame)->v404 = t350;
  {
  t352 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t353 = (&frame)->v403;
  t354 = ((GG_UNALIGNED gg_unaligned_4 *)t352.data) + gg_index(t353, t352.len);
  t355 = ((int64_t)((*t354)));
  t356 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t357 = (&frame)->v404;
  t358 = ((GG_UNALIGNED gg_unaligned_4 *)t356.data) + gg_index(t357, t356.len);
  t359 = ((int64_t)((*t358)));
  t351 = ((t355) < (t359));
  }
  t348 = t351;
  }
  t345 = t348;
  }
  (&frame)->v398 = t345;
  t360 = (&frame)->v398;
  t361 = (&frame)->v394;
  t362 = (&frame)->v395;
  t363 = ((GG_UNALIGNED gg_unaligned_4 *)t361.data) + gg_index(t362, t361.len);
  t364 = ((int64_t)((*t363)));
  t365 = (&frame)->v394;
  t366 = (&frame)->v397;
  t367 = ((GG_UNALIGNED gg_unaligned_4 *)t365.data) + gg_index(t366, t365.len);
  t368 = ((int64_t)((*t367)));
  (&frame)->v405 = t364;
  (&frame)->v406 = t368;
  {
  t370 = ((int64_t)((&frame)->v405));
  t371 = ((int64_t)((&frame)->v406));
  (&frame)->v407 = t370;
  (&frame)->v408 = t371;
  {
  t373 = ((int64_t)((&frame)->v407));
  t374 = ((int64_t)((&frame)->v408));
  (&frame)->v409 = t373;
  (&frame)->v410 = t374;
  {
  t376 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t377 = (&frame)->v409;
  t378 = ((GG_UNALIGNED gg_unaligned_4 *)t376.data) + gg_index(t377, t376.len);
  t379 = ((int64_t)((*t378)));
  t380 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t381 = (&frame)->v410;
  t382 = ((GG_UNALIGNED gg_unaligned_4 *)t380.data) + gg_index(t381, t380.len);
  t383 = ((int64_t)((*t382)));
  t375 = ((t379) < (t383));
  }
  t372 = t375;
  }
  t369 = t372;
  }
  t384 = t369;
  t385 = ((t360) != (t384));
  if (t385) {
  {
  t336 = ((int64_t)((&frame)->v395));
  goto gg_done;
  }
  }
  t386 = (&frame)->v394;
  t387 = (&frame)->v396;
  t388 = ((GG_UNALIGNED gg_unaligned_4 *)t386.data) + gg_index(t387, t386.len);
  t389 = ((int64_t)((*t388)));
  t390 = (&frame)->v394;
  t391 = (&frame)->v397;
  t392 = ((GG_UNALIGNED gg_unaligned_4 *)t390.data) + gg_index(t391, t390.len);
  t393 = ((int64_t)((*t392)));
  (&frame)->v411 = t389;
  (&frame)->v412 = t393;
  {
  t395 = ((int64_t)((&frame)->v411));
  t396 = ((int64_t)((&frame)->v412));
  (&frame)->v413 = t395;
  (&frame)->v414 = t396;
  {
  t398 = ((int64_t)((&frame)->v413));
  t399 = ((int64_t)((&frame)->v414));
  (&frame)->v415 = t398;
  (&frame)->v416 = t399;
  {
  t401 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t402 = (&frame)->v415;
  t403 = ((GG_UNALIGNED gg_unaligned_4 *)t401.data) + gg_index(t402, t401.len);
  t404 = ((int64_t)((*t403)));
  t405 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t406 = (&frame)->v416;
  t407 = ((GG_UNALIGNED gg_unaligned_4 *)t405.data) + gg_index(t406, t405.len);
  t408 = ((int64_t)((*t407)));
  t400 = ((t404) < (t408));
  }
  t397 = t400;
  }
  t394 = t397;
  }
  t409 = t394;
  t410 = (&frame)->v398;
  t411 = ((t409) != (t410));
  if (t411) {
  {
  t336 = ((int64_t)((&frame)->v397));
  goto gg_done;
  }
  }
  t336 = ((int64_t)((&frame)->v396));
  goto gg_done;
  }
gg_done:;
  return t336;
}
static int64_t gg_function_33(gg_slice p365, int64_t p366, int64_t p367, int64_t p368, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_33 frame;
  int64_t t412;
  gg_slice t413;
  int64_t t414;
  GG_UNALIGNED gg_unaligned_4 * t415;
  int64_t t416;
  gg_slice t417;
  int64_t t418;
  GG_UNALIGNED gg_unaligned_4 * t419;
  int64_t t420;
  uint8_t t421;
  int64_t t422;
  int64_t t423;
  uint8_t t424;
  int64_t t425;
  int64_t t426;
  uint8_t t427;
  gg_slice t428;
  int64_t t429;
  GG_UNALIGNED gg_unaligned_4 * t430;
  int64_t t431;
  gg_slice t432;
  int64_t t433;
  GG_UNALIGNED gg_unaligned_4 * t434;
  int64_t t435;
  uint8_t t436;
  gg_slice t437;
  int64_t t438;
  GG_UNALIGNED gg_unaligned_4 * t439;
  int64_t t440;
  gg_slice t441;
  int64_t t442;
  GG_UNALIGNED gg_unaligned_4 * t443;
  int64_t t444;
  uint8_t t445;
  int64_t t446;
  int64_t t447;
  uint8_t t448;
  int64_t t449;
  int64_t t450;
  uint8_t t451;
  gg_slice t452;
  int64_t t453;
  GG_UNALIGNED gg_unaligned_4 * t454;
  int64_t t455;
  gg_slice t456;
  int64_t t457;
  GG_UNALIGNED gg_unaligned_4 * t458;
  int64_t t459;
  uint8_t t460;
  uint8_t t461;
  gg_slice t462;
  int64_t t463;
  GG_UNALIGNED gg_unaligned_4 * t464;
  int64_t t465;
  gg_slice t466;
  int64_t t467;
  GG_UNALIGNED gg_unaligned_4 * t468;
  int64_t t469;
  uint8_t t470;
  int64_t t471;
  int64_t t472;
  uint8_t t473;
  int64_t t474;
  int64_t t475;
  uint8_t t476;
  gg_slice t477;
  int64_t t478;
  GG_UNALIGNED gg_unaligned_4 * t479;
  int64_t t480;
  gg_slice t481;
  int64_t t482;
  GG_UNALIGNED gg_unaligned_4 * t483;
  int64_t t484;
  uint8_t t485;
  uint8_t t486;
  uint8_t t487;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v365 = p365;
  (&frame)->v366 = p366;
  (&frame)->v367 = p367;
  (&frame)->v368 = p368;
  {
  t413 = (&frame)->v365;
  t414 = (&frame)->v366;
  t415 = ((GG_UNALIGNED gg_unaligned_4 *)t413.data) + gg_index(t414, t413.len);
  t416 = ((int64_t)((*t415)));
  t417 = (&frame)->v365;
  t418 = (&frame)->v367;
  t419 = ((GG_UNALIGNED gg_unaligned_4 *)t417.data) + gg_index(t418, t417.len);
  t420 = ((int64_t)((*t419)));
  (&frame)->v370 = t416;
  (&frame)->v371 = t420;
  {
  t422 = ((int64_t)((&frame)->v370));
  t423 = ((int64_t)((&frame)->v371));
  (&frame)->v372 = t422;
  (&frame)->v373 = t423;
  {
  t425 = ((int64_t)((&frame)->v372));
  t426 = ((int64_t)((&frame)->v373));
  (&frame)->v374 = t425;
  (&frame)->v375 = t426;
  {
  t428 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t429 = (&frame)->v374;
  t430 = ((GG_UNALIGNED gg_unaligned_4 *)t428.data) + gg_index(t429, t428.len);
  t431 = ((int64_t)((*t430)));
  t432 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t433 = (&frame)->v375;
  t434 = ((GG_UNALIGNED gg_unaligned_4 *)t432.data) + gg_index(t433, t432.len);
  t435 = ((int64_t)((*t434)));
  t427 = ((t431) < (t435));
  }
  t424 = t427;
  }
  t421 = t424;
  }
  (&frame)->v369 = t421;
  t436 = (&frame)->v369;
  t437 = (&frame)->v365;
  t438 = (&frame)->v366;
  t439 = ((GG_UNALIGNED gg_unaligned_4 *)t437.data) + gg_index(t438, t437.len);
  t440 = ((int64_t)((*t439)));
  t441 = (&frame)->v365;
  t442 = (&frame)->v368;
  t443 = ((GG_UNALIGNED gg_unaligned_4 *)t441.data) + gg_index(t442, t441.len);
  t444 = ((int64_t)((*t443)));
  (&frame)->v376 = t440;
  (&frame)->v377 = t444;
  {
  t446 = ((int64_t)((&frame)->v376));
  t447 = ((int64_t)((&frame)->v377));
  (&frame)->v378 = t446;
  (&frame)->v379 = t447;
  {
  t449 = ((int64_t)((&frame)->v378));
  t450 = ((int64_t)((&frame)->v379));
  (&frame)->v380 = t449;
  (&frame)->v381 = t450;
  {
  t452 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t453 = (&frame)->v380;
  t454 = ((GG_UNALIGNED gg_unaligned_4 *)t452.data) + gg_index(t453, t452.len);
  t455 = ((int64_t)((*t454)));
  t456 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t457 = (&frame)->v381;
  t458 = ((GG_UNALIGNED gg_unaligned_4 *)t456.data) + gg_index(t457, t456.len);
  t459 = ((int64_t)((*t458)));
  t451 = ((t455) < (t459));
  }
  t448 = t451;
  }
  t445 = t448;
  }
  t460 = t445;
  t461 = ((t436) != (t460));
  if (t461) {
  {
  t412 = ((int64_t)((&frame)->v366));
  goto gg_done;
  }
  }
  t462 = (&frame)->v365;
  t463 = (&frame)->v367;
  t464 = ((GG_UNALIGNED gg_unaligned_4 *)t462.data) + gg_index(t463, t462.len);
  t465 = ((int64_t)((*t464)));
  t466 = (&frame)->v365;
  t467 = (&frame)->v368;
  t468 = ((GG_UNALIGNED gg_unaligned_4 *)t466.data) + gg_index(t467, t466.len);
  t469 = ((int64_t)((*t468)));
  (&frame)->v382 = t465;
  (&frame)->v383 = t469;
  {
  t471 = ((int64_t)((&frame)->v382));
  t472 = ((int64_t)((&frame)->v383));
  (&frame)->v384 = t471;
  (&frame)->v385 = t472;
  {
  t474 = ((int64_t)((&frame)->v384));
  t475 = ((int64_t)((&frame)->v385));
  (&frame)->v386 = t474;
  (&frame)->v387 = t475;
  {
  t477 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t478 = (&frame)->v386;
  t479 = ((GG_UNALIGNED gg_unaligned_4 *)t477.data) + gg_index(t478, t477.len);
  t480 = ((int64_t)((*t479)));
  t481 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t482 = (&frame)->v387;
  t483 = ((GG_UNALIGNED gg_unaligned_4 *)t481.data) + gg_index(t482, t481.len);
  t484 = ((int64_t)((*t483)));
  t476 = ((t480) < (t484));
  }
  t473 = t476;
  }
  t470 = t473;
  }
  t485 = t470;
  t486 = (&frame)->v369;
  t487 = ((t485) != (t486));
  if (t487) {
  {
  t412 = ((int64_t)((&frame)->v368));
  goto gg_done;
  }
  }
  t412 = ((int64_t)((&frame)->v367));
  goto gg_done;
  }
gg_done:;
  return t412;
}
static int64_t gg_function_32(gg_slice p335, int64_t p336, int64_t p337, int64_t p338, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_32 frame;
  int64_t t488;
  gg_slice t489;
  int64_t t490;
  GG_UNALIGNED gg_unaligned_4 * t491;
  int64_t t492;
  gg_slice t493;
  int64_t t494;
  GG_UNALIGNED gg_unaligned_4 * t495;
  int64_t t496;
  uint8_t t497;
  int64_t t498;
  int64_t t499;
  uint8_t t500;
  int64_t t501;
  int64_t t502;
  uint8_t t503;
  gg_slice t504;
  int64_t t505;
  GG_UNALIGNED gg_unaligned_4 * t506;
  int64_t t507;
  gg_slice t508;
  int64_t t509;
  GG_UNALIGNED gg_unaligned_4 * t510;
  int64_t t511;
  uint8_t t512;
  gg_slice t513;
  int64_t t514;
  GG_UNALIGNED gg_unaligned_4 * t515;
  int64_t t516;
  gg_slice t517;
  int64_t t518;
  GG_UNALIGNED gg_unaligned_4 * t519;
  int64_t t520;
  uint8_t t521;
  int64_t t522;
  int64_t t523;
  uint8_t t524;
  int64_t t525;
  int64_t t526;
  uint8_t t527;
  gg_slice t528;
  int64_t t529;
  GG_UNALIGNED gg_unaligned_4 * t530;
  int64_t t531;
  gg_slice t532;
  int64_t t533;
  GG_UNALIGNED gg_unaligned_4 * t534;
  int64_t t535;
  uint8_t t536;
  uint8_t t537;
  gg_slice t538;
  int64_t t539;
  GG_UNALIGNED gg_unaligned_4 * t540;
  int64_t t541;
  gg_slice t542;
  int64_t t543;
  GG_UNALIGNED gg_unaligned_4 * t544;
  int64_t t545;
  uint8_t t546;
  int64_t t547;
  int64_t t548;
  uint8_t t549;
  int64_t t550;
  int64_t t551;
  uint8_t t552;
  gg_slice t553;
  int64_t t554;
  GG_UNALIGNED gg_unaligned_4 * t555;
  int64_t t556;
  gg_slice t557;
  int64_t t558;
  GG_UNALIGNED gg_unaligned_4 * t559;
  int64_t t560;
  uint8_t t561;
  uint8_t t562;
  uint8_t t563;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v335 = p335;
  (&frame)->v336 = p336;
  (&frame)->v337 = p337;
  (&frame)->v338 = p338;
  {
  t489 = (&frame)->v335;
  t490 = (&frame)->v336;
  t491 = ((GG_UNALIGNED gg_unaligned_4 *)t489.data) + gg_index(t490, t489.len);
  t492 = ((int64_t)((*t491)));
  t493 = (&frame)->v335;
  t494 = (&frame)->v337;
  t495 = ((GG_UNALIGNED gg_unaligned_4 *)t493.data) + gg_index(t494, t493.len);
  t496 = ((int64_t)((*t495)));
  (&frame)->v340 = t492;
  (&frame)->v341 = t496;
  {
  t498 = ((int64_t)((&frame)->v340));
  t499 = ((int64_t)((&frame)->v341));
  (&frame)->v342 = t498;
  (&frame)->v343 = t499;
  {
  t501 = ((int64_t)((&frame)->v342));
  t502 = ((int64_t)((&frame)->v343));
  (&frame)->v344 = t501;
  (&frame)->v345 = t502;
  {
  t504 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t505 = (&frame)->v344;
  t506 = ((GG_UNALIGNED gg_unaligned_4 *)t504.data) + gg_index(t505, t504.len);
  t507 = ((int64_t)((*t506)));
  t508 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t509 = (&frame)->v345;
  t510 = ((GG_UNALIGNED gg_unaligned_4 *)t508.data) + gg_index(t509, t508.len);
  t511 = ((int64_t)((*t510)));
  t503 = ((t507) < (t511));
  }
  t500 = t503;
  }
  t497 = t500;
  }
  (&frame)->v339 = t497;
  t512 = (&frame)->v339;
  t513 = (&frame)->v335;
  t514 = (&frame)->v336;
  t515 = ((GG_UNALIGNED gg_unaligned_4 *)t513.data) + gg_index(t514, t513.len);
  t516 = ((int64_t)((*t515)));
  t517 = (&frame)->v335;
  t518 = (&frame)->v338;
  t519 = ((GG_UNALIGNED gg_unaligned_4 *)t517.data) + gg_index(t518, t517.len);
  t520 = ((int64_t)((*t519)));
  (&frame)->v346 = t516;
  (&frame)->v347 = t520;
  {
  t522 = ((int64_t)((&frame)->v346));
  t523 = ((int64_t)((&frame)->v347));
  (&frame)->v348 = t522;
  (&frame)->v349 = t523;
  {
  t525 = ((int64_t)((&frame)->v348));
  t526 = ((int64_t)((&frame)->v349));
  (&frame)->v350 = t525;
  (&frame)->v351 = t526;
  {
  t528 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t529 = (&frame)->v350;
  t530 = ((GG_UNALIGNED gg_unaligned_4 *)t528.data) + gg_index(t529, t528.len);
  t531 = ((int64_t)((*t530)));
  t532 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t533 = (&frame)->v351;
  t534 = ((GG_UNALIGNED gg_unaligned_4 *)t532.data) + gg_index(t533, t532.len);
  t535 = ((int64_t)((*t534)));
  t527 = ((t531) < (t535));
  }
  t524 = t527;
  }
  t521 = t524;
  }
  t536 = t521;
  t537 = ((t512) != (t536));
  if (t537) {
  {
  t488 = ((int64_t)((&frame)->v336));
  goto gg_done;
  }
  }
  t538 = (&frame)->v335;
  t539 = (&frame)->v337;
  t540 = ((GG_UNALIGNED gg_unaligned_4 *)t538.data) + gg_index(t539, t538.len);
  t541 = ((int64_t)((*t540)));
  t542 = (&frame)->v335;
  t543 = (&frame)->v338;
  t544 = ((GG_UNALIGNED gg_unaligned_4 *)t542.data) + gg_index(t543, t542.len);
  t545 = ((int64_t)((*t544)));
  (&frame)->v352 = t541;
  (&frame)->v353 = t545;
  {
  t547 = ((int64_t)((&frame)->v352));
  t548 = ((int64_t)((&frame)->v353));
  (&frame)->v354 = t547;
  (&frame)->v355 = t548;
  {
  t550 = ((int64_t)((&frame)->v354));
  t551 = ((int64_t)((&frame)->v355));
  (&frame)->v356 = t550;
  (&frame)->v357 = t551;
  {
  t553 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t554 = (&frame)->v356;
  t555 = ((GG_UNALIGNED gg_unaligned_4 *)t553.data) + gg_index(t554, t553.len);
  t556 = ((int64_t)((*t555)));
  t557 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t558 = (&frame)->v357;
  t559 = ((GG_UNALIGNED gg_unaligned_4 *)t557.data) + gg_index(t558, t557.len);
  t560 = ((int64_t)((*t559)));
  t552 = ((t556) < (t560));
  }
  t549 = t552;
  }
  t546 = t549;
  }
  t561 = t546;
  t562 = (&frame)->v339;
  t563 = ((t561) != (t562));
  if (t563) {
  {
  t488 = ((int64_t)((&frame)->v338));
  goto gg_done;
  }
  }
  t488 = ((int64_t)((&frame)->v337));
  goto gg_done;
  }
gg_done:;
  return t488;
}
static int64_t gg_function_31(gg_slice p325, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_31 frame;
  int64_t t564;
  int64_t t565;
  int64_t t566;
  int64_t t567;
  int64_t t568;
  int64_t t569;
  int64_t t570;
  int64_t t571;
  int64_t t572;
  uint8_t t573;
  gg_slice t574;
  int64_t t575;
  int64_t t576;
  int64_t t577;
  int64_t t578;
  int64_t t579;
  gg_slice t580;
  int64_t t581;
  int64_t t582;
  int64_t t583;
  int64_t t584;
  int64_t t585;
  int64_t t586;
  int64_t t587;
  int64_t t588;
  int64_t t589;
  int64_t t590;
  int64_t t591;
  gg_slice t592;
  int64_t t593;
  int64_t t594;
  int64_t t595;
  int64_t t596;
  int64_t t597;
  int64_t t598;
  int64_t t599;
  int64_t t600;
  int64_t t601;
  int64_t t602;
  int64_t t603;
  gg_slice t604;
  int64_t t605;
  int64_t t606;
  int64_t t607;
  int64_t t608;
  int64_t t609;
  int64_t t610;
  int64_t t611;
  int64_t t612;
  int64_t t613;
  int64_t t614;
  int64_t t615;
  gg_slice t616;
  int64_t t617;
  int64_t t618;
  int64_t t619;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v325 = p325;
  {
  t565 = ((int64_t)(((&frame)->v325).len));
  t566 = ((int64_t)(((int64_t)UINT64_C(8))));
  (&frame)->v326 = ((int64_t)(((int64_t)gg_div(t565, t566))));
  (&frame)->v327 = ((int64_t)(((int64_t)UINT64_C(0))));
  t567 = ((int64_t)((&frame)->v326));
  t568 = ((int64_t)(((int64_t)UINT64_C(4))));
  (&frame)->v328 = ((int64_t)(((int64_t)((uint64_t)(t567) * (uint64_t)(t568)))));
  t569 = ((int64_t)((&frame)->v326));
  t570 = ((int64_t)(((int64_t)UINT64_C(7))));
  (&frame)->v329 = ((int64_t)(((int64_t)((uint64_t)(t569) * (uint64_t)(t570)))));
  t571 = ((int64_t)(((&frame)->v325).len));
  t572 = ((int64_t)(((int64_t)UINT64_C(128))));
  t573 = ((t571) < (t572));
  if (t573) {
  {
  t574 = (&frame)->v325;
  t575 = ((int64_t)((&frame)->v327));
  t576 = ((int64_t)((&frame)->v328));
  t577 = ((int64_t)((&frame)->v329));
  t564 = gg_function_32(t574, t575, t576, t577, (&frame)->fv_frame_24, frame.base + 0);
  goto gg_done;
  }
  }
  t578 = ((int64_t)((&frame)->v326));
  t579 = ((int64_t)(((int64_t)UINT64_C(8))));
  (&frame)->v358 = ((int64_t)(((int64_t)gg_div(t578, t579))));
  t580 = (&frame)->v325;
  t581 = ((int64_t)((&frame)->v327));
  t582 = ((int64_t)((&frame)->v327));
  t583 = ((int64_t)((&frame)->v358));
  t584 = ((int64_t)(((int64_t)UINT64_C(4))));
  t585 = ((int64_t)(((int64_t)((uint64_t)(t583) * (uint64_t)(t584)))));
  t586 = ((int64_t)(((int64_t)((uint64_t)(t582) + (uint64_t)(t585)))));
  t587 = ((int64_t)((&frame)->v327));
  t588 = ((int64_t)((&frame)->v358));
  t589 = ((int64_t)(((int64_t)UINT64_C(7))));
  t590 = ((int64_t)(((int64_t)((uint64_t)(t588) * (uint64_t)(t589)))));
  t591 = ((int64_t)(((int64_t)((uint64_t)(t587) + (uint64_t)(t590)))));
  (&frame)->v359 = gg_function_33(t580, t581, t586, t591, (&frame)->fv_frame_24, frame.base + 0);
  t592 = (&frame)->v325;
  t593 = ((int64_t)((&frame)->v328));
  t594 = ((int64_t)((&frame)->v328));
  t595 = ((int64_t)((&frame)->v358));
  t596 = ((int64_t)(((int64_t)UINT64_C(4))));
  t597 = ((int64_t)(((int64_t)((uint64_t)(t595) * (uint64_t)(t596)))));
  t598 = ((int64_t)(((int64_t)((uint64_t)(t594) + (uint64_t)(t597)))));
  t599 = ((int64_t)((&frame)->v328));
  t600 = ((int64_t)((&frame)->v358));
  t601 = ((int64_t)(((int64_t)UINT64_C(7))));
  t602 = ((int64_t)(((int64_t)((uint64_t)(t600) * (uint64_t)(t601)))));
  t603 = ((int64_t)(((int64_t)((uint64_t)(t599) + (uint64_t)(t602)))));
  (&frame)->v388 = gg_function_34(t592, t593, t598, t603, (&frame)->fv_frame_24, frame.base + 0);
  t604 = (&frame)->v325;
  t605 = ((int64_t)((&frame)->v329));
  t606 = ((int64_t)((&frame)->v329));
  t607 = ((int64_t)((&frame)->v358));
  t608 = ((int64_t)(((int64_t)UINT64_C(4))));
  t609 = ((int64_t)(((int64_t)((uint64_t)(t607) * (uint64_t)(t608)))));
  t610 = ((int64_t)(((int64_t)((uint64_t)(t606) + (uint64_t)(t609)))));
  t611 = ((int64_t)((&frame)->v329));
  t612 = ((int64_t)((&frame)->v358));
  t613 = ((int64_t)(((int64_t)UINT64_C(7))));
  t614 = ((int64_t)(((int64_t)((uint64_t)(t612) * (uint64_t)(t613)))));
  t615 = ((int64_t)(((int64_t)((uint64_t)(t611) + (uint64_t)(t614)))));
  (&frame)->v417 = gg_function_35(t604, t605, t610, t615, (&frame)->fv_frame_24, frame.base + 0);
  t616 = (&frame)->v325;
  t617 = ((int64_t)((&frame)->v359));
  t618 = ((int64_t)((&frame)->v388));
  t619 = ((int64_t)((&frame)->v417));
  t564 = gg_function_36(t616, t617, t618, t619, (&frame)->fv_frame_24, frame.base + 0);
  goto gg_done;
  }
gg_done:;
  return t564;
}
static void gg_function_29(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
  /* FG_SHARED_BODY:37 */
  gg_function_37(p0, p1, p2, base);
}
static void gg_function_28(gg_slice p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  gg_slice t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = ((int64_t)((v0).len));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t0) - (uint64_t)(t1)))));
label1:;
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  if (!(((t2) < (t3)))) goto label2;
  {
  t4 = v0;
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v2));
  gg_function_37(t4, t5, t6, base + 0);
  t7 = v1;
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t7) + (uint64_t)(t8)));
  t9 = v2;
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t9) - (uint64_t)(t10)));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_27(gg_slice p281, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_27 frame;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  gg_slice t15;
  int64_t t16;
  GG_UNALIGNED gg_unaligned_4 * t17;
  int64_t t18;
  gg_slice t19;
  int64_t t20;
  GG_UNALIGNED gg_unaligned_4 * t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  gg_slice t27;
  int64_t t28;
  GG_UNALIGNED gg_unaligned_4 * t29;
  int64_t t30;
  gg_slice t31;
  int64_t t32;
  GG_UNALIGNED gg_unaligned_4 * t33;
  int64_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  gg_slice t38;
  int64_t t39;
  GG_UNALIGNED gg_unaligned_4 * t40;
  int64_t t41;
  gg_slice t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  GG_UNALIGNED gg_unaligned_4 * t46;
  int64_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  gg_slice t52;
  int64_t t53;
  GG_UNALIGNED gg_unaligned_4 * t54;
  int64_t t55;
  gg_slice t56;
  int64_t t57;
  GG_UNALIGNED gg_unaligned_4 * t58;
  int64_t t59;
  uint8_t t60;
  uint8_t t61;
  int64_t t62;
  int64_t t63;
  gg_slice t64;
  int64_t t65;
  gg_slice t66;
  int64_t t67;
  int64_t t68;
  gg_slice t69;
  int64_t t70;
  GG_UNALIGNED gg_unaligned_4 * t71;
  int64_t t72;
  gg_slice t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  GG_UNALIGNED gg_unaligned_4 * t77;
  int64_t t78;
  uint8_t t79;
  int64_t t80;
  int64_t t81;
  uint8_t t82;
  gg_slice t83;
  int64_t t84;
  GG_UNALIGNED gg_unaligned_4 * t85;
  int64_t t86;
  gg_slice t87;
  int64_t t88;
  GG_UNALIGNED gg_unaligned_4 * t89;
  int64_t t90;
  uint8_t t91;
  int64_t t92;
  int64_t t93;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v281 = p281;
  {
  t12 = ((int64_t)(((&frame)->v281).len));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t11 = ((int64_t)(((&frame)->v281).len));
  goto gg_done;
  }
  }
  (&frame)->v282 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = (&frame)->v281;
  t16 = ((int64_t)UINT64_C(1));
  t17 = ((GG_UNALIGNED gg_unaligned_4 *)t15.data) + gg_index(t16, t15.len);
  t18 = ((int64_t)((*t17)));
  t19 = (&frame)->v281;
  t20 = ((int64_t)UINT64_C(0));
  t21 = ((GG_UNALIGNED gg_unaligned_4 *)t19.data) + gg_index(t20, t19.len);
  t22 = ((int64_t)((*t21)));
  (&frame)->v283 = t18;
  (&frame)->v284 = t22;
  {
  t24 = ((int64_t)((&frame)->v283));
  t25 = ((int64_t)((&frame)->v284));
  (&frame)->v285 = t24;
  (&frame)->v286 = t25;
  {
  t27 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t28 = (&frame)->v285;
  t29 = ((GG_UNALIGNED gg_unaligned_4 *)t27.data) + gg_index(t28, t27.len);
  t30 = ((int64_t)((*t29)));
  t31 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t32 = (&frame)->v286;
  t33 = ((GG_UNALIGNED gg_unaligned_4 *)t31.data) + gg_index(t32, t31.len);
  t34 = ((int64_t)((*t33)));
  t26 = ((t30) < (t34));
  }
  t23 = t26;
  }
  t35 = t23;
  if (t35) {
  {
label4:;
  t36 = ((int64_t)((&frame)->v282));
  t37 = ((int64_t)(((&frame)->v281).len));
  if (!(((t36) < (t37)))) goto label5;
  {
  t38 = (&frame)->v281;
  t39 = (&frame)->v282;
  t40 = ((GG_UNALIGNED gg_unaligned_4 *)t38.data) + gg_index(t39, t38.len);
  t41 = ((int64_t)((*t40)));
  t42 = (&frame)->v281;
  t43 = ((int64_t)((&frame)->v282));
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  t45 = ((int64_t)((uint64_t)(t43) - (uint64_t)(t44)));
  t46 = ((GG_UNALIGNED gg_unaligned_4 *)t42.data) + gg_index(t45, t42.len);
  t47 = ((int64_t)((*t46)));
  (&frame)->v287 = t41;
  (&frame)->v288 = t47;
  {
  t49 = ((int64_t)((&frame)->v287));
  t50 = ((int64_t)((&frame)->v288));
  (&frame)->v289 = t49;
  (&frame)->v290 = t50;
  {
  t52 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t53 = (&frame)->v289;
  t54 = ((GG_UNALIGNED gg_unaligned_4 *)t52.data) + gg_index(t53, t52.len);
  t55 = ((int64_t)((*t54)));
  t56 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t57 = (&frame)->v290;
  t58 = ((GG_UNALIGNED gg_unaligned_4 *)t56.data) + gg_index(t57, t56.len);
  t59 = ((int64_t)((*t58)));
  t51 = ((t55) < (t59));
  }
  t48 = t51;
  }
  t60 = t48;
  t61 = (!(t60));
  if (t61) {
  {
  goto label5;
  }
  }
  t62 = (&frame)->v282;
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v282 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  }
  goto label4;
label5:;
  t64 = (&frame)->v281;
  t65 = (&frame)->v282;
  t66 = gg_sub(t64, 0, t65, sizeof(int64_t));
  gg_function_28(t66, frame.base + 0);
  }
  } else {
  {
label7:;
  t67 = ((int64_t)((&frame)->v282));
  t68 = ((int64_t)(((&frame)->v281).len));
  if (!(((t67) < (t68)))) goto label8;
  {
  t69 = (&frame)->v281;
  t70 = (&frame)->v282;
  t71 = ((GG_UNALIGNED gg_unaligned_4 *)t69.data) + gg_index(t70, t69.len);
  t72 = ((int64_t)((*t71)));
  t73 = (&frame)->v281;
  t74 = ((int64_t)((&frame)->v282));
  t75 = ((int64_t)(((int64_t)UINT64_C(1))));
  t76 = ((int64_t)((uint64_t)(t74) - (uint64_t)(t75)));
  t77 = ((GG_UNALIGNED gg_unaligned_4 *)t73.data) + gg_index(t76, t73.len);
  t78 = ((int64_t)((*t77)));
  (&frame)->v306 = t72;
  (&frame)->v307 = t78;
  {
  t80 = ((int64_t)((&frame)->v306));
  t81 = ((int64_t)((&frame)->v307));
  (&frame)->v308 = t80;
  (&frame)->v309 = t81;
  {
  t83 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t84 = (&frame)->v308;
  t85 = ((GG_UNALIGNED gg_unaligned_4 *)t83.data) + gg_index(t84, t83.len);
  t86 = ((int64_t)((*t85)));
  t87 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t88 = (&frame)->v309;
  t89 = ((GG_UNALIGNED gg_unaligned_4 *)t87.data) + gg_index(t88, t87.len);
  t90 = ((int64_t)((*t89)));
  t82 = ((t86) < (t90));
  }
  t79 = t82;
  }
  t91 = t79;
  if (t91) {
  {
  goto label8;
  }
  }
  t92 = (&frame)->v282;
  t93 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v282 = ((int64_t)((uint64_t)(t92) + (uint64_t)(t93)));
  }
  goto label7;
label8:;
  }
  }
  t11 = ((int64_t)((&frame)->v282));
  goto gg_done;
  }
gg_done:;
  return t11;
}
static void gg_function_26(gg_slice p264, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_26 frame;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  gg_slice t97;
  int64_t t98;
  GG_UNALIGNED gg_unaligned_4 * t99;
  int64_t t100;
  int64_t t101;
  gg_slice t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  GG_UNALIGNED gg_unaligned_4 * t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  uint8_t t112;
  gg_slice t113;
  int64_t t114;
  GG_UNALIGNED gg_unaligned_4 * t115;
  int64_t t116;
  gg_slice t117;
  int64_t t118;
  GG_UNALIGNED gg_unaligned_4 * t119;
  int64_t t120;
  uint8_t t121;
  uint8_t t122;
  gg_slice t123;
  int64_t t124;
  GG_UNALIGNED gg_unaligned_4 * t125;
  int64_t t126;
  int64_t t127;
  gg_slice t128;
  int64_t t129;
  GG_UNALIGNED gg_unaligned_4 * t130;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v264 = p264;
  {
  t95 = ((int64_t)UINT64_C(1));
  t96 = ((&frame)->v264).len;
  t94 = t95;
label9:;
  if (t94 >= t96) goto label11;
  (&frame)->v265 = t94;
  {
  t97 = (&frame)->v264;
  t98 = (&frame)->v265;
  t99 = ((GG_UNALIGNED gg_unaligned_4 *)t97.data) + gg_index(t98, t97.len);
  (&frame)->v266 = ((int64_t)((*t99)));
  (&frame)->v267 = ((int64_t)((&frame)->v265));
label13:;
  t100 = ((int64_t)((&frame)->v267));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t100) > (t101)))) goto label14;
  {
  t102 = (&frame)->v264;
  t103 = ((int64_t)((&frame)->v267));
  t104 = ((int64_t)(((int64_t)UINT64_C(1))));
  t105 = ((int64_t)((uint64_t)(t103) - (uint64_t)(t104)));
  t106 = ((GG_UNALIGNED gg_unaligned_4 *)t102.data) + gg_index(t105, t102.len);
  (&frame)->v268 = ((int64_t)((*t106)));
  t107 = ((int64_t)((&frame)->v266));
  t108 = ((int64_t)((&frame)->v268));
  (&frame)->v269 = t107;
  (&frame)->v270 = t108;
  {
  t110 = ((int64_t)((&frame)->v269));
  t111 = ((int64_t)((&frame)->v270));
  (&frame)->v271 = t110;
  (&frame)->v272 = t111;
  {
  t113 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t114 = (&frame)->v271;
  t115 = ((GG_UNALIGNED gg_unaligned_4 *)t113.data) + gg_index(t114, t113.len);
  t116 = ((int64_t)((*t115)));
  t117 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t118 = (&frame)->v272;
  t119 = ((GG_UNALIGNED gg_unaligned_4 *)t117.data) + gg_index(t118, t117.len);
  t120 = ((int64_t)((*t119)));
  t112 = ((t116) < (t120));
  }
  t109 = t112;
  }
  t121 = t109;
  t122 = (!(t121));
  if (t122) {
  {
  goto label14;
  }
  }
  t123 = (&frame)->v264;
  t124 = (&frame)->v267;
  t125 = ((GG_UNALIGNED gg_unaligned_4 *)t123.data) + gg_index(t124, t123.len);
  (*t125) = ((int64_t)((&frame)->v268));
  t126 = (&frame)->v267;
  t127 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v267 = ((int64_t)((uint64_t)(t126) - (uint64_t)(t127)));
  }
  goto label13;
label14:;
  t128 = (&frame)->v264;
  t129 = (&frame)->v267;
  t130 = ((GG_UNALIGNED gg_unaligned_4 *)t128.data) + gg_index(t129, t128.len);
  (*t130) = ((int64_t)((&frame)->v266));
  }
label10:;
  t94++; goto label9;
label11:;
  }
gg_done:;
}
static void gg_function_25(gg_slice p257, struct gg_frame_24 *fv_frame_24, int64_t base) {
  struct gg_frame_25 frame;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  gg_slice t134;
  gg_slice t135;
  int64_t t136;
  int64_t t137;
  uint8_t t138;
  int64_t t139;
  int64_t t140;
  uint64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  gg_slice t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  uint8_t t150;
  uint8_t t151;
  int64_t t152;
  int64_t t153;
  uint8_t t154;
  int64_t t155;
  int64_t t156;
  gg_slice t157;
  int64_t t158;
  gg_slice t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  uint8_t t163;
  uint8_t t164;
  gg_slice t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  GG_UNALIGNED gg_unaligned_4 * t169;
  int64_t t170;
  gg_slice t171;
  int64_t t172;
  GG_UNALIGNED gg_unaligned_4 * t173;
  int64_t t174;
  uint8_t t175;
  gg_slice t176;
  int64_t t177;
  GG_UNALIGNED gg_unaligned_4 * t178;
  int64_t t179;
  gg_slice t180;
  int64_t t181;
  GG_UNALIGNED gg_unaligned_4 * t182;
  int64_t t183;
  uint8_t t184;
  uint8_t t185;
  gg_slice t186;
  gg_slice t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  gg_slice t194;
  gg_slice t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  uint8_t t204;
  gg_type_11 t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  gg_type_11 t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  gg_type_11 t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  gg_type_11 t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  gg_type_11 t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  gg_type_11 t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  uint8_t t243;
  gg_slice t244;
  gg_slice t245;
  int64_t t246;
  int64_t t247;
  uint8_t t248;
  gg_type_11 t249;
  int64_t t250;
  gg_type_11 t251;
  int64_t t252;
  gg_type_11 t253;
  int64_t t254;
  frame.base = base;
  frame.fv_frame_24 = fv_frame_24;
  (&frame)->v257 = p257;
  {
  (&frame)->v258 = ((int64_t)(((&frame)->v257).len));
  t131 = ((int64_t)((&frame)->v258));
  t132 = ((int64_t)(((int64_t)UINT64_C(10))));
  t133 = ((t131) <= (t132));
  if (t133) {
  {
  t134 = (&frame)->v257;
  gg_function_26(t134, (&frame)->fv_frame_24, frame.base + 0);
  goto gg_done;
  }
  }
  t135 = (&frame)->v257;
  (&frame)->v275 = gg_function_27(t135, (&frame)->fv_frame_24, frame.base + 0);
  t136 = ((int64_t)((&frame)->v275));
  t137 = ((int64_t)((&frame)->v258));
  t138 = ((t136) == (t137));
  if (t138) {
  {
  goto gg_done;
  }
  }
  t139 = ((int64_t)(((int64_t)UINT64_C(2))));
  t140 = ((int64_t)(((int64_t)UINT64_C(63))));
  t141 = ((uint64_t)(((uint64_t)((&frame)->v258))));
#ifndef __cplusplus
  extern int64_t (gs_clz)(uint64_t);
#endif
  t142 = gs_clz(t141);
  t143 = ((int64_t)(t142));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t140) - (uint64_t)(t143)))));
  (&frame)->v311 = ((int64_t)(((int64_t)((uint64_t)(t139) * (uint64_t)(t144)))));
  ((&frame)->v313).len = 0;
  (&frame)->v315 = ((int64_t)(((int64_t)UINT64_C(0))));
  (&frame)->v316 = ((int64_t)((&frame)->v258));
label16:;
  {
  t145 = (&frame)->v257;
  t146 = (&frame)->v315;
  t147 = (&frame)->v316;
  (&frame)->v317 = gg_sub(t145, t146, t147, sizeof(int64_t));
  t148 = ((int64_t)(((&frame)->v317).len));
  t149 = ((int64_t)(((int64_t)UINT64_C(10))));
  t150 = ((t148) > (t149));
  t151 = t150;
  if (t151) {
  t152 = ((int64_t)((&frame)->v311));
  t153 = ((int64_t)(((int64_t)UINT64_C(0))));
  t151 = ((t152) > (t153));
  }
  t154 = t151;
  if (t154) {
  {
  t155 = (&frame)->v311;
  t156 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v311 = ((int64_t)((uint64_t)(t155) - (uint64_t)(t156)));
  t157 = (&frame)->v317;
  t158 = ((int64_t)(((int64_t)UINT64_C(0))));
  t159 = (&frame)->v317;
  t160 = gg_function_31(t159, (&frame)->fv_frame_24, frame.base + 0);
  gg_function_37(t157, t158, t160, frame.base + 0);
  t161 = ((int64_t)((&frame)->v315));
  t162 = ((int64_t)(((int64_t)UINT64_C(0))));
  t163 = ((t161) > (t162));
  t164 = t163;
  if (t164) {
  t165 = (&frame)->v257;
  t166 = ((int64_t)((&frame)->v315));
  t167 = ((int64_t)(((int64_t)UINT64_C(1))));
  t168 = ((int64_t)((uint64_t)(t166) - (uint64_t)(t167)));
  t169 = ((GG_UNALIGNED gg_unaligned_4 *)t165.data) + gg_index(t168, t165.len);
  t170 = ((int64_t)((*t169)));
  t171 = (&frame)->v317;
  t172 = ((int64_t)UINT64_C(0));
  t173 = ((GG_UNALIGNED gg_unaligned_4 *)t171.data) + gg_index(t172, t171.len);
  t174 = ((int64_t)((*t173)));
  (&frame)->v478 = t170;
  (&frame)->v479 = t174;
  {
  t176 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t177 = (&frame)->v478;
  t178 = ((GG_UNALIGNED gg_unaligned_4 *)t176.data) + gg_index(t177, t176.len);
  t179 = ((int64_t)((*t178)));
  t180 = gg_view(((&frame)->fv_frame_24->v245).data, 256);
  t181 = (&frame)->v479;
  t182 = ((GG_UNALIGNED gg_unaligned_4 *)t180.data) + gg_index(t181, t180.len);
  t183 = ((int64_t)((*t182)));
  t175 = ((t179) < (t183));
  }
  t184 = t175;
  t164 = (!(t184));
  }
  t185 = t164;
  if (t185) {
  {
  t186 = (&frame)->v317;
  (&frame)->v480 = gg_function_38(t186, (&frame)->fv_frame_24, frame.base + 0);
  t187 = (&frame)->v317;
  t188 = ((int64_t)(((int64_t)UINT64_C(0))));
  t189 = ((int64_t)((&frame)->v480));
  gg_function_37(t187, t188, t189, frame.base + 0);
  t190 = (&frame)->v315;
  t191 = ((int64_t)((&frame)->v480));
  t192 = ((int64_t)(((int64_t)UINT64_C(1))));
  t193 = ((int64_t)(((int64_t)((uint64_t)(t191) + (uint64_t)(t192)))));
  (&frame)->v315 = ((int64_t)((uint64_t)(t190) + (uint64_t)(t193)));
  goto label16;
  }
  }
  t194 = (&frame)->v317;
  (&frame)->v498 = gg_function_39(t194, (&frame)->fv_frame_24, frame.base + 0);
  t195 = (&frame)->v317;
  t196 = ((int64_t)(((int64_t)UINT64_C(0))));
  t197 = ((int64_t)((&frame)->v498));
  gg_function_37(t195, t196, t197, frame.base + 0);
  t198 = ((int64_t)((&frame)->v498));
  t199 = ((int64_t)(((&frame)->v317).len));
  t200 = ((int64_t)(((int64_t)UINT64_C(1))));
  t201 = ((int64_t)(((int64_t)((uint64_t)(t199) - (uint64_t)(t200)))));
  t202 = ((int64_t)((&frame)->v498));
  t203 = ((int64_t)(((int64_t)((uint64_t)(t201) - (uint64_t)(t202)))));
  t204 = ((t198) < (t203));
  if (t204) {
  {
  t205 = &((&frame)->v313);
  t206 = ((int64_t)((&frame)->v315));
  t207 = ((int64_t)((&frame)->v498));
  t208 = ((int64_t)(((int64_t)((uint64_t)(t206) + (uint64_t)(t207)))));
  t209 = ((int64_t)(((int64_t)UINT64_C(1))));
  t210 = ((int64_t)(((int64_t)((uint64_t)(t208) + (uint64_t)(t209)))));
  if ((uint64_t)((*(t205))).len > UINT64_C(144)) gg_fail("invalid limited array length");
  t211 = (int64_t)((*(t205))).len;
  t212 = 1;
  if (t212 < 0 || t212 > 144 - (int64_t)t211) gg_fail("limited array capacity exceeded");
  if (t212) memmove((unsigned char *)((*(t205))).data + (size_t)t211 * sizeof(int64_t), (const void *)(&t210), (size_t)t212 * sizeof(int64_t));
  ((*(t205))).len = t211 + t212;
  (void)(((GG_UNALIGNED gg_unaligned_4 *)((*(t205))).data) + ((*(t205))).len - 1);
  t213 = &((&frame)->v313);
  t214 = ((int64_t)((&frame)->v316));
  if ((uint64_t)((*(t213))).len > UINT64_C(144)) gg_fail("invalid limited array length");
  t215 = (int64_t)((*(t213))).len;
  t216 = 1;
  if (t216 < 0 || t216 > 144 - (int64_t)t215) gg_fail("limited array capacity exceeded");
  if (t216) memmove((unsigned char *)((*(t213))).data + (size_t)t215 * sizeof(int64_t), (const void *)(&t214), (size_t)t216 * sizeof(int64_t));
  ((*(t213))).len = t215 + t216;
  (void)(((GG_UNALIGNED gg_unaligned_4 *)((*(t213))).data) + ((*(t213))).len - 1);
  t217 = &((&frame)->v313);
  t218 = ((int64_t)((&frame)->v311));
  if ((uint64_t)((*(t217))).len > UINT64_C(144)) gg_fail("invalid limited array length");
  t219 = (int64_t)((*(t217))).len;
  t220 = 1;
  if (t220 < 0 || t220 > 144 - (int64_t)t219) gg_fail("limited array capacity exceeded");
  if (t220) memmove((unsigned char *)((*(t217))).data + (size_t)t219 * sizeof(int64_t), (const void *)(&t218), (size_t)t220 * sizeof(int64_t));
  ((*(t217))).len = t219 + t220;
  (void)(((GG_UNALIGNED gg_unaligned_4 *)((*(t217))).data) + ((*(t217))).len - 1);
  t221 = ((int64_t)((&frame)->v315));
  t222 = ((int64_t)((&frame)->v498));
  (&frame)->v316 = ((int64_t)(((int64_t)((uint64_t)(t221) + (uint64_t)(t222)))));
  }
  } else {
  {
  t223 = &((&frame)->v313);
  t224 = ((int64_t)((&frame)->v315));
  if ((uint64_t)((*(t223))).len > UINT64_C(144)) gg_fail("invalid limited array length");
  t225 = (int64_t)((*(t223))).len;
  t226 = 1;
  if (t226 < 0 || t226 > 144 - (int64_t)t225) gg_fail("limited array capacity exceeded");
  if (t226) memmove((unsigned char *)((*(t223))).data + (size_t)t225 * sizeof(int64_t), (const void *)(&t224), (size_t)t226 * sizeof(int64_t));
  ((*(t223))).len = t225 + t226;
  (void)(((GG_UNALIGNED gg_unaligned_4 *)((*(t223))).data) + ((*(t223))).len - 1);
  t227 = &((&frame)->v313);
  t228 = ((int64_t)((&frame)->v315));
  t229 = ((int64_t)((&frame)->v498));
  t230 = ((int64_t)(((int64_t)((uint64_t)(t228) + (uint64_t)(t229)))));
  if ((uint64_t)((*(t227))).len > UINT64_C(144)) gg_fail("invalid limited array length");
  t231 = (int64_t)((*(t227))).len;
  t232 = 1;
  if (t232 < 0 || t232 > 144 - (int64_t)t231) gg_fail("limited array capacity exceeded");
  if (t232) memmove((unsigned char *)((*(t227))).data + (size_t)t231 * sizeof(int64_t), (const void *)(&t230), (size_t)t232 * sizeof(int64_t));
  ((*(t227))).len = t231 + t232;
  (void)(((GG_UNALIGNED gg_unaligned_4 *)((*(t227))).data) + ((*(t227))).len - 1);
  t233 = &((&frame)->v313);
  t234 = ((int64_t)((&frame)->v311));
  if ((uint64_t)((*(t233))).len > UINT64_C(144)) gg_fail("invalid limited array length");
  t235 = (int64_t)((*(t233))).len;
  t236 = 1;
  if (t236 < 0 || t236 > 144 - (int64_t)t235) gg_fail("limited array capacity exceeded");
  if (t236) memmove((unsigned char *)((*(t233))).data + (size_t)t235 * sizeof(int64_t), (const void *)(&t234), (size_t)t236 * sizeof(int64_t));
  ((*(t233))).len = t235 + t236;
  (void)(((GG_UNALIGNED gg_unaligned_4 *)((*(t233))).data) + ((*(t233))).len - 1);
  t237 = (&frame)->v315;
  t238 = ((int64_t)((&frame)->v498));
  t239 = ((int64_t)(((int64_t)UINT64_C(1))));
  t240 = ((int64_t)(((int64_t)((uint64_t)(t238) + (uint64_t)(t239)))));
  (&frame)->v315 = ((int64_t)((uint64_t)(t237) + (uint64_t)(t240)));
  }
  }
  goto label16;
  }
  }
  t241 = ((int64_t)(((&frame)->v317).len));
  t242 = ((int64_t)(((int64_t)UINT64_C(10))));
  t243 = ((t241) > (t242));
  if (t243) {
  {
  t244 = (&frame)->v317;
  gg_function_40(t244, (&frame)->fv_frame_24, frame.base + 0);
  }
  } else {
  {
  t245 = (&frame)->v317;
  gg_function_43(t245, (&frame)->fv_frame_24, frame.base + 0);
  }
  }
  if ((uint64_t)((&frame)->v313).len > UINT64_C(144)) gg_fail("invalid limited array length");
  t246 = ((int64_t)((gg_view(((&frame)->v313).data, ((&frame)->v313).len)).len));
  t247 = ((int64_t)(((int64_t)UINT64_C(0))));
  t248 = ((t246) == (t247));
  if (t248) {
  {
  goto gg_done;
  }
  }
  t249 = &((&frame)->v313);
  if ((uint64_t)((*(t249))).len > UINT64_C(144)) gg_fail("invalid limited array length");
  t250 = ((GG_UNALIGNED gg_unaligned_4 *)((*(t249))).data)[gg_index((int64_t)((*(t249))).len-1, ((*(t249))).len)];
  ((*(t249))).len--;
  (&frame)->v311 = ((int64_t)(t250));
  t251 = &((&frame)->v313);
  if ((uint64_t)((*(t251))).len > UINT64_C(144)) gg_fail("invalid limited array length");
  t252 = ((GG_UNALIGNED gg_unaligned_4 *)((*(t251))).data)[gg_index((int64_t)((*(t251))).len-1, ((*(t251))).len)];
  ((*(t251))).len--;
  (&frame)->v316 = ((int64_t)(t252));
  t253 = &((&frame)->v313);
  if ((uint64_t)((*(t253))).len > UINT64_C(144)) gg_fail("invalid limited array length");
  t254 = ((GG_UNALIGNED gg_unaligned_4 *)((*(t253))).data)[gg_index((int64_t)((*(t253))).len-1, ((*(t253))).len)];
  ((*(t253))).len--;
  (&frame)->v315 = ((int64_t)(t254));
  }
  goto label16;
label17:;
  }
gg_done:;
}
static void gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  uint8_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(v1));
#ifndef __cplusplus
  extern void (fg201l_fail)(int64_t);
#endif
  fg201l_fail(t2);
  }
  }
  }
gg_done:;
}
static int64_t gg_function_24(uint64_t p243, int64_t p244, int64_t base) {
  struct gg_frame_24 frame;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  gg_slice t20;
  int64_t t21;
  GG_UNALIGNED gg_unaligned_4 * t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  gg_slice t28;
  int64_t t29;
  GG_UNALIGNED gg_unaligned_4 * t30;
  int64_t t31;
  int64_t t32;
  gg_slice t33;
  int64_t t34;
  int64_t t35;
  gg_slice t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  gg_slice t43;
  int64_t t44;
  GG_UNALIGNED gg_unaligned_4 * t45;
  gg_slice t46;
  int64_t t47;
  GG_UNALIGNED gg_unaligned_4 * t48;
  uint64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  gg_slice t54;
  int64_t t55;
  GG_UNALIGNED gg_unaligned_4 * t56;
  uint64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  gg_slice t72;
  int64_t t73;
  GG_UNALIGNED gg_unaligned_4 * t74;
  int64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  gg_slice t82;
  int64_t t83;
  GG_UNALIGNED gg_unaligned_4 * t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  uint64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  uint64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  uint64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  frame.base = base;
  (&frame)->v243 = p243;
  (&frame)->v244 = p244;
  {
  t4 = ((int64_t)((&frame)->v244));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) >= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(80))));
  gg_function_2(t6, t7, frame.base + 0);
  t8 = ((int64_t)((&frame)->v244));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t10 = ((t8) <= (t9));
  if (t10) {
  {
  t3 = ((int64_t)((&frame)->v244));
  goto gg_done;
  }
  }
  t11 = ((int64_t)((&frame)->v244));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t13 = ((t11) <= (t12));
  if (t13) {
  {
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  for (t15=0; t15<256; t15++) ((&frame)->v245).data[t15] = t14;
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  for (t17=0; t17<256; t17++) ((&frame)->v247).data[t17] = t16;
  (&frame)->v249 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t18 = ((int64_t)((&frame)->v249));
  t19 = ((int64_t)((&frame)->v244));
  if (!(((t18) < (t19)))) goto label2;
  {
  t20 = gg_view(((&frame)->v245).data, 256);
  t21 = (&frame)->v249;
  t22 = ((GG_UNALIGNED gg_unaligned_4 *)t20.data) + gg_index(t21, t20.len);
  t23 = ((uint64_t)((&frame)->v243));
  t24 = ((int64_t)((&frame)->v249));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
#ifndef __cplusplus
  extern int64_t (fg201l_ld_ne)(uint64_t, int64_t);
#endif
  t27 = fg201l_ld_ne(t23, t26);
  (*t22) = ((int64_t)(t27));
  t28 = gg_view(((&frame)->v247).data, 256);
  t29 = (&frame)->v249;
  t30 = ((GG_UNALIGNED gg_unaligned_4 *)t28.data) + gg_index(t29, t28.len);
  (*t30) = ((int64_t)((&frame)->v249));
  t31 = (&frame)->v249;
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v249 = ((int64_t)((uint64_t)(t31) + (uint64_t)(t32)));
  }
  goto label1;
label2:;
  t33 = gg_view(((&frame)->v247).data, 256);
  t34 = ((int64_t)UINT64_C(0));
  t35 = (&frame)->v244;
  t36 = gg_sub(t33, t34, t35, sizeof(int64_t));
  gg_function_25(t36, (&frame), frame.base + 0);
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  for (t38=0; t38<256; t38++) ((&frame)->v606).data[t38] = t37;
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  for (t40=0; t40<256; t40++) ((&frame)->v608).data[t40] = t39;
  (&frame)->v249 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t41 = ((int64_t)((&frame)->v249));
  t42 = ((int64_t)((&frame)->v244));
  if (!(((t41) < (t42)))) goto label5;
  {
  t43 = gg_view(((&frame)->v247).data, 256);
  t44 = (&frame)->v249;
  t45 = ((GG_UNALIGNED gg_unaligned_4 *)t43.data) + gg_index(t44, t43.len);
  (&frame)->v610 = ((int64_t)((*t45)));
  t46 = gg_view(((&frame)->v606).data, 256);
  t47 = (&frame)->v249;
  t48 = ((GG_UNALIGNED gg_unaligned_4 *)t46.data) + gg_index(t47, t46.len);
  t49 = ((uint64_t)((&frame)->v243));
  t50 = ((int64_t)((&frame)->v610));
  t51 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
#ifndef __cplusplus
  extern int64_t (fg201l_ld_ne)(uint64_t, int64_t);
#endif
  t53 = fg201l_ld_ne(t49, t52);
  (*t48) = ((int64_t)(t53));
  t54 = gg_view(((&frame)->v608).data, 256);
  t55 = (&frame)->v249;
  t56 = ((GG_UNALIGNED gg_unaligned_4 *)t54.data) + gg_index(t55, t54.len);
  t57 = ((uint64_t)((&frame)->v243));
  t58 = ((int64_t)((&frame)->v610));
  t59 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
#ifndef __cplusplus
  extern int64_t (fg201l_ld_ne)(uint64_t, int64_t);
#endif
  t63 = fg201l_ld_ne(t57, t62);
  (*t56) = ((int64_t)(t63));
  t64 = (&frame)->v249;
  t65 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v249 = ((int64_t)((uint64_t)(t64) + (uint64_t)(t65)));
  }
  goto label4;
label5:;
  (&frame)->v249 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t66 = ((int64_t)((&frame)->v249));
  t67 = ((int64_t)((&frame)->v244));
  if (!(((t66) < (t67)))) goto label8;
  {
  t68 = ((uint64_t)((&frame)->v243));
  t69 = ((int64_t)((&frame)->v249));
  t70 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = gg_view(((&frame)->v606).data, 256);
  t73 = (&frame)->v249;
  t74 = ((GG_UNALIGNED gg_unaligned_4 *)t72.data) + gg_index(t73, t72.len);
  t75 = ((int64_t)((*t74)));
#ifndef __cplusplus
  extern void (fg201l_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg201l_st_i64(t68, t71, t75);
  t76 = ((uint64_t)((&frame)->v243));
  t77 = ((int64_t)((&frame)->v249));
  t78 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t78)))));
  t80 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  t82 = gg_view(((&frame)->v608).data, 256);
  t83 = (&frame)->v249;
  t84 = ((GG_UNALIGNED gg_unaligned_4 *)t82.data) + gg_index(t83, t82.len);
  t85 = ((int64_t)((*t84)));
#ifndef __cplusplus
  extern void (fg201l_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg201l_st_i64(t76, t81, t85);
  t86 = (&frame)->v249;
  t87 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v249 = ((int64_t)((uint64_t)(t86) + (uint64_t)(t87)));
  }
  goto label7;
label8:;
  t3 = ((int64_t)((&frame)->v244));
  goto gg_done;
  }
  }
  (&frame)->v613 = ((int64_t)(((int64_t)UINT64_C(1))));
label10:;
  t88 = ((int64_t)((&frame)->v613));
  t89 = ((int64_t)((&frame)->v244));
  if (!(((t88) < (t89)))) goto label11;
  {
  t90 = ((uint64_t)((&frame)->v243));
  t91 = ((int64_t)((&frame)->v613));
  t92 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t92)))));
#ifndef __cplusplus
  extern int64_t (fg201l_ld_ne)(uint64_t, int64_t);
#endif
  t94 = fg201l_ld_ne(t90, t93);
  (&frame)->v614 = ((int64_t)(t94));
  t95 = ((uint64_t)((&frame)->v243));
  t96 = ((int64_t)((&frame)->v613));
  t97 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) * (uint64_t)(t97)))));
  t99 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) + (uint64_t)(t99)))));
#ifndef __cplusplus
  extern int64_t (fg201l_ld_ne)(uint64_t, int64_t);
#endif
  t101 = fg201l_ld_ne(t95, t100);
  (&frame)->v615 = ((int64_t)(t101));
  t102 = ((int64_t)((&frame)->v613));
  t103 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  (&frame)->v616 = ((int64_t)(((int64_t)((uint64_t)(t102) - (uint64_t)(t103)))));
label13:;
  t104 = ((int64_t)((&frame)->v616));
  t105 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  if (!(((t104) >= (t105)))) goto label14;
  {
  t106 = ((uint64_t)((&frame)->v243));
  t107 = ((int64_t)((&frame)->v616));
  t108 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t107) * (uint64_t)(t108)))));
#ifndef __cplusplus
  extern int64_t (fg201l_ld_ne)(uint64_t, int64_t);
#endif
  t110 = fg201l_ld_ne(t106, t109);
  (&frame)->v617 = ((int64_t)(t110));
  t111 = ((int64_t)((&frame)->v617));
  t112 = ((int64_t)((&frame)->v614));
  t113 = ((t111) <= (t112));
  if (t113) {
  {
  goto label14;
  }
  }
  t114 = ((uint64_t)((&frame)->v243));
  t115 = ((int64_t)((&frame)->v616));
  t116 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) * (uint64_t)(t118)))));
  t120 = ((int64_t)((&frame)->v617));
#ifndef __cplusplus
  extern void (fg201l_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg201l_st_i64(t114, t119, t120);
  t121 = ((uint64_t)((&frame)->v243));
  t122 = ((int64_t)((&frame)->v616));
  t123 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t122) + (uint64_t)(t123)))));
  t125 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t124) * (uint64_t)(t125)))));
  t127 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t128 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t127)))));
  t129 = ((uint64_t)((&frame)->v243));
  t130 = ((int64_t)((&frame)->v616));
  t131 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t132 = ((int64_t)(((int64_t)((uint64_t)(t130) * (uint64_t)(t131)))));
  t133 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t134 = ((int64_t)(((int64_t)((uint64_t)(t132) + (uint64_t)(t133)))));
#ifndef __cplusplus
  extern int64_t (fg201l_ld_ne)(uint64_t, int64_t);
#endif
  t135 = fg201l_ld_ne(t129, t134);
  t136 = ((int64_t)(t135));
#ifndef __cplusplus
  extern void (fg201l_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg201l_st_i64(t121, t128, t136);
  t137 = (&frame)->v616;
  t138 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v616 = ((int64_t)((uint64_t)(t137) - (uint64_t)(t138)));
  }
  goto label13;
label14:;
  t139 = ((uint64_t)((&frame)->v243));
  t140 = ((int64_t)((&frame)->v616));
  t141 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t142 = ((int64_t)(((int64_t)((uint64_t)(t140) + (uint64_t)(t141)))));
  t143 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t142) * (uint64_t)(t143)))));
  t145 = ((int64_t)((&frame)->v614));
#ifndef __cplusplus
  extern void (fg201l_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg201l_st_i64(t139, t144, t145);
  t146 = ((uint64_t)((&frame)->v243));
  t147 = ((int64_t)((&frame)->v616));
  t148 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t149 = ((int64_t)(((int64_t)((uint64_t)(t147) + (uint64_t)(t148)))));
  t150 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t151 = ((int64_t)(((int64_t)((uint64_t)(t149) * (uint64_t)(t150)))));
  t152 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t153 = ((int64_t)(((int64_t)((uint64_t)(t151) + (uint64_t)(t152)))));
  t154 = ((int64_t)((&frame)->v615));
#ifndef __cplusplus
  extern void (fg201l_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg201l_st_i64(t146, t153, t154);
  t155 = (&frame)->v613;
  t156 = ((int64_t)(((int64_t)UINT64_C(1))));
  (&frame)->v613 = ((int64_t)((uint64_t)(t155) + (uint64_t)(t156)));
  }
  goto label10;
label11:;
  t3 = ((int64_t)((&frame)->v244));
  goto gg_done;
  }
gg_done:;
  return t3;
}
static void gg_function_21(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v2));
#ifndef __cplusplus
  extern void (fg201l_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg201l_st_i64(t0, t3, t4);
  t5 = ((uint64_t)(v0));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = ((int64_t)(v3));
#ifndef __cplusplus
  extern void (fg201l_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg201l_st_i64(t5, t10, t11);
  }
gg_done:;
}
static int64_t gg_function_17(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_16(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((int64_t)(v0));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_15(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v0));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_14(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
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
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) >= (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t13) >= (t14));
  }
  t15 = t12;
  t16 = ((int64_t)(((int64_t)UINT64_C(51))));
  gg_function_2(t15, t16, base + 0);
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(v2));
  t19 = ((t17) <= (t18));
  if (t19) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(v3));
  t22 = ((t20) >= (t21));
  if (t22) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_20(uint64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
#ifndef __cplusplus
  extern int64_t (fg201l_ld_ne)(uint64_t, int64_t);
#endif
  t7 = fg201l_ld_ne(t1, t6);
  t0 = ((int64_t)(t7));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_19(uint64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
#ifndef __cplusplus
  extern int64_t (fg201l_ld_ne)(uint64_t, int64_t);
#endif
  t5 = fg201l_ld_ne(t1, t4);
  t0 = ((int64_t)(t5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_23(uint64_t p0, int64_t p1, uint64_t p2, int64_t p3, uint64_t p4, int64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  int64_t t16;
  uint64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  int64_t t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  int64_t t42;
  uint64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  int64_t t62;
  uint64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  uint64_t v0;
  int64_t v1;
  uint64_t v2;
  int64_t v3;
  uint64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) >= (t10));
  }
  t11 = t8;
  t12 = ((int64_t)(((int64_t)UINT64_C(70))));
  gg_function_2(t11, t12, base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t13 = ((int64_t)(v7));
  t14 = ((int64_t)(v1));
  if (!(((t13) < (t14)))) goto label2;
  {
  t15 = ((uint64_t)(v0));
  t16 = ((int64_t)(v7));
  v8 = gg_function_19(t15, t16, base + 0);
  t17 = ((uint64_t)(v0));
  t18 = ((int64_t)(v7));
  v9 = gg_function_20(t17, t18, base + 0);
  v10 = ((int64_t)(v8));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t19 = ((int64_t)(v11));
  t20 = ((int64_t)(v3));
  if (!(((t19) < (t20)))) goto label5;
  {
  t21 = ((uint64_t)(v2));
  t22 = ((int64_t)(v11));
  v12 = gg_function_19(t21, t22, base + 0);
  t23 = ((uint64_t)(v2));
  t24 = ((int64_t)(v11));
  v13 = gg_function_20(t23, t24, base + 0);
  t25 = ((int64_t)(v12));
  t26 = ((int64_t)(v13));
  t27 = ((int64_t)(v10));
  t28 = ((int64_t)(v9));
  t29 = gg_function_14(t25, t26, t27, t28, base + 0);
  t30 = ((int64_t)(t29));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t32 = ((t30) == (t31));
  if (t32) {
  {
  t33 = ((int64_t)(v10));
  t34 = ((int64_t)(v12));
  t35 = gg_function_15(t33, t34, base + 0);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) != (t37));
  if (t38) {
  {
  t39 = ((int64_t)(v6));
  t40 = ((int64_t)(v5));
  t41 = ((t39) < (t40));
  t42 = ((int64_t)(((int64_t)UINT64_C(71))));
  gg_function_2(t41, t42, base + 0);
  t43 = ((uint64_t)(v4));
  t44 = ((int64_t)(v6));
  t45 = ((int64_t)(v10));
  t46 = ((int64_t)(v12));
  gg_function_21(t43, t44, t45, t46, base + 0);
  t47 = v6;
  t48 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t47) + (uint64_t)(t48)));
  }
  }
  t49 = ((int64_t)(v10));
  t50 = ((int64_t)(v13));
  v10 = gg_function_16(t49, t50, base + 0);
  }
  }
  t51 = v11;
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t51) + (uint64_t)(t52)));
  }
  goto label4;
label5:;
  t53 = ((int64_t)(v10));
  t54 = ((int64_t)(v9));
  t55 = gg_function_17(t53, t54, base + 0);
  t56 = ((int64_t)(t55));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((t56) != (t57));
  if (t58) {
  {
  t59 = ((int64_t)(v6));
  t60 = ((int64_t)(v5));
  t61 = ((t59) < (t60));
  t62 = ((int64_t)(((int64_t)UINT64_C(72))));
  gg_function_2(t61, t62, base + 0);
  t63 = ((uint64_t)(v4));
  t64 = ((int64_t)(v6));
  t65 = ((int64_t)(v10));
  t66 = ((int64_t)(v9));
  gg_function_21(t63, t64, t65, t66, base + 0);
  t67 = v6;
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t67) + (uint64_t)(t68)));
  }
  }
  t69 = v7;
  t70 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t69) + (uint64_t)(t70)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_13(int64_t p0, int64_t p1, int64_t base) {
  /* FG_SHARED_BODY:16 */
  return gg_function_16(p0, p1, base);
}
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(50))));
  gg_function_2(t7, t8, base + 0);
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v0));
  t11 = ((t9) <= (t10));
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_18(uint64_t p0, int64_t p1, uint64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  int64_t t12;
  uint64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint8_t t48;
  int64_t t49;
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  uint64_t v0;
  int64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(60))));
  gg_function_2(t7, t8, base + 0);
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(v1));
  if (!(((t9) < (t10)))) goto label2;
  {
  t11 = ((uint64_t)(v0));
  t12 = ((int64_t)(v5));
  v6 = gg_function_19(t11, t12, base + 0);
  t13 = ((uint64_t)(v0));
  t14 = ((int64_t)(v5));
  v7 = gg_function_20(t13, t14, base + 0);
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(v7));
  t17 = ((t15) <= (t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(61))));
  gg_function_2(t17, t18, base + 0);
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t21 = ((t19) > (t20));
  if (t21) {
  {
  t22 = ((uint64_t)(v2));
  t23 = ((int64_t)(v4));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) - (uint64_t)(t24)))));
  v8 = gg_function_20(t22, t25, base + 0);
  t26 = ((int64_t)(v8));
  t27 = ((int64_t)(v6));
  t28 = gg_function_12(t26, t27, base + 0);
  t29 = ((int64_t)(t28));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t29) != (t30));
  if (t31) {
  {
  t32 = ((uint64_t)(v2));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) - (uint64_t)(t34)))));
  t36 = ((uint64_t)(v2));
  t37 = ((int64_t)(v4));
  t38 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) - (uint64_t)(t38)))));
  t40 = gg_function_19(t36, t39, base + 0);
  t41 = ((int64_t)(v8));
  t42 = ((int64_t)(v7));
  t43 = gg_function_16(t41, t42, base + 0);
  gg_function_21(t32, t35, t40, t43, base + 0);
  t44 = v5;
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  goto label1;
  }
  }
  }
  }
  t46 = ((int64_t)(v4));
  t47 = ((int64_t)(v3));
  t48 = ((t46) < (t47));
  t49 = ((int64_t)(((int64_t)UINT64_C(62))));
  gg_function_2(t48, t49, base + 0);
  t50 = ((uint64_t)(v2));
  t51 = ((int64_t)(v4));
  t52 = ((int64_t)(v6));
  t53 = ((int64_t)(v7));
  gg_function_21(t50, t51, t52, t53, base + 0);
  t54 = v4;
  t55 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t54) + (uint64_t)(t55)));
  t56 = v5;
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t56) + (uint64_t)(t57)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_11(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) - (uint64_t)(t3)))));
  t5 = ((t1) == (t4));
  if (t5) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t8 = ((t6) == (t7));
  if (t8) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(41))));
  gg_function_2(t7, t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(v1));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(40))));
  gg_function_2(t7, t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(v1));
  t11 = ((t9) > (t10));
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_5(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(20))));
  gg_function_2(t7, t8, base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  if (!(((t9) < (t10)))) goto label2;
  {
  t11 = ((int64_t)(v6));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t13 = ((t11) == (t12));
  if (t13) {
  {
  v7 = ((int64_t)(v2));
  }
  } else {
  {
  t14 = ((int64_t)(v6));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  v7 = ((int64_t)(v3));
  }
  } else {
  {
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  v7 = ((int64_t)(v4));
  }
  } else {
  {
  v7 = ((int64_t)(v5));
  }
  }
  }
  }
  }
  }
  t20 = ((uint64_t)(v0));
  t21 = ((int64_t)(v6));
#ifndef __cplusplus
  extern int64_t (fg201l_ld_ne)(uint64_t, int64_t);
#endif
  t22 = fg201l_ld_ne(t20, t21);
  v8 = ((int64_t)(t22));
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(v1));
  t25 = ((t23) < (t24));
  if (t25) {
  {
  v9 = ((int64_t)(v7));
  }
  } else {
  {
  v9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t26 = ((int64_t)(v8));
  t27 = ((int64_t)(v9));
  t28 = ((t26) != (t27));
  if (t28) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t29 = v6;
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t29) + (uint64_t)(t30)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(uint64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_2(t7, t8, base + 0);
  v2 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(v1));
  if (!(((t9) < (t10)))) goto label2;
  {
  t11 = ((uint64_t)(v0));
  t12 = ((int64_t)(v3));
#ifndef __cplusplus
  extern int64_t (fg201l_ld_ne)(uint64_t, int64_t);
#endif
  t13 = fg201l_ld_ne(t11, t12);
  v4 = ((int64_t)(t13));
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) >= (t15));
  t17 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_2(t16, t17, base + 0);
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(v4));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t20 = v3;
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t20) + (uint64_t)(t21)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_6(int64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(30))));
  gg_function_2(t3, t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) != (t6));
  if (t7) {
  {
  t8 = ((uint64_t)(v1));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t10 = gg_function_1(t8, t9, base + 0);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(v3));
  t13 = ((t11) == (t12));
  if (t13) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t14 = ((uint64_t)(v1));
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)(v6));
  t19 = ((int64_t)(v7));
  t0 = gg_function_5(t14, t15, t16, t17, t18, t19, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint64_t t52;
  int64_t t53;
  uint64_t t54;
  int64_t t55;
  int64_t t56;
  uint64_t t57;
  int64_t t58;
  uint64_t t59;
  int64_t t60;
  uint64_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
  int64_t t65;
  int64_t t66;
  uint64_t t67;
  int64_t t68;
  int64_t t69;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t2 = gg_function_1(t0, t1, base + 0);
  (void)(t2);
  t3 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t9 = gg_function_5(t3, t4, t5, t6, t7, t8, base + 0);
  (void)(t9);
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t18 = gg_function_6(t10, t11, t12, t13, t14, t15, t16, t17, base + 0);
  (void)(t18);
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t21 = gg_function_7(t19, t20, base + 0);
  (void)(t21);
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t24 = gg_function_8(t22, t23, base + 0);
  (void)(t24);
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t27 = gg_function_9(t25, t26, base + 0);
  (void)(t27);
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t29 = gg_function_10(t28, base + 0);
  (void)(t29);
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t31 = gg_function_11(t30, base + 0);
  (void)(t31);
  t32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t34 = gg_function_12(t32, t33, base + 0);
  (void)(t34);
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t37 = gg_function_16(t35, t36, base + 0);
  (void)(t37);
  t38 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t42 = gg_function_14(t38, t39, t40, t41, base + 0);
  (void)(t42);
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t45 = gg_function_15(t43, t44, base + 0);
  (void)(t45);
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t47 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t48 = gg_function_16(t46, t47, base + 0);
  (void)(t48);
  t49 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t50 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t51 = gg_function_17(t49, t50, base + 0);
  (void)(t51);
  t52 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t53 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t54 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t56 = gg_function_18(t52, t53, t54, t55, base + 0);
  (void)(t56);
  t57 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t59 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t60 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t61 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t62 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t63 = gg_function_23(t57, t58, t59, t60, t61, t62, base + 0);
  (void)(t63);
  t64 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t65 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t66 = gg_function_24(t64, t65, base + 0);
  (void)(t66);
  t67 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(16))))));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t69 = gg_function_24(t67, t68, base + 0);
  (void)(t69);
  }
gg_done:;
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
  (gg_globals.v136).data[0] = ((double)(gg_double_bits(UINT64_C(4607182418800017408))));
  (gg_globals.v136).data[1] = ((double)(gg_double_bits(UINT64_C(4621819117588971520))));
  (gg_globals.v136).data[2] = ((double)(gg_double_bits(UINT64_C(4636737291354636288))));
  (gg_globals.v136).data[3] = ((double)(gg_double_bits(UINT64_C(4652007308841189376))));
  (gg_globals.v136).data[4] = ((double)(gg_double_bits(UINT64_C(4666723172467343360))));
  (gg_globals.v136).data[5] = ((double)(gg_double_bits(UINT64_C(4681608360884174848))));
  (gg_globals.v136).data[6] = ((double)(gg_double_bits(UINT64_C(4696837146684686336))));
  (gg_globals.v136).data[7] = ((double)(gg_double_bits(UINT64_C(4711630319722168320))));
  (gg_globals.v136).data[8] = ((double)(gg_double_bits(UINT64_C(4726483295884279808))));
  (gg_globals.v136).data[9] = ((double)(gg_double_bits(UINT64_C(4741671816366391296))));
  (gg_globals.v136).data[10] = ((double)(gg_double_bits(UINT64_C(4756540486875873280))));
  (gg_globals.v136).data[11] = ((double)(gg_double_bits(UINT64_C(4771362005757984768))));
  (gg_globals.v136).data[12] = ((double)(gg_double_bits(UINT64_C(4786511204640096256))));
  (gg_globals.v136).data[13] = ((double)(gg_double_bits(UINT64_C(4801453603149578240))));
  (gg_globals.v136).data[14] = ((double)(gg_double_bits(UINT64_C(4816244402031689728))));
  (gg_globals.v136).data[15] = ((double)(gg_double_bits(UINT64_C(4831355200913801216))));
  (gg_globals.v136).data[16] = ((double)(gg_double_bits(UINT64_C(4846369599423283200))));
  (gg_globals.v136).data[17] = ((double)(gg_double_bits(UINT64_C(4861130398305394688))));
  (gg_globals.v136).data[18] = ((double)(gg_double_bits(UINT64_C(4876203697187506176))));
  (gg_globals.v136).data[19] = ((double)(gg_double_bits(UINT64_C(4891288408196988160))));
  (gg_globals.v136).data[20] = ((double)(gg_double_bits(UINT64_C(4906019910204099648))));
  (gg_globals.v136).data[21] = ((double)(gg_double_bits(UINT64_C(4921056587992461136))));
  (gg_globals.v136).data[22] = ((double)(gg_double_bits(UINT64_C(4936209963552724370))));
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
