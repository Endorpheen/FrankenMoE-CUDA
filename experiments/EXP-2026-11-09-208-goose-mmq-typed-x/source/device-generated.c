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
#define GG_RESERVATION ((size_t)268435456)
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


typedef uint64_t gg_unaligned_1 GG_ALIGN1;
typedef int64_t gg_unaligned_2 GG_ALIGN1;
typedef float gg_unaligned_3 GG_ALIGN1;
typedef double gg_unaligned_4 GG_ALIGN1;
typedef const GG_UNALIGNED gg_unaligned_3 *gg_type_5;
typedef gg_type_5 gg_unaligned_5 GG_ALIGN1;
typedef int32_t gg_unaligned_6 GG_ALIGN1;
typedef uint32_t gg_unaligned_9 GG_ALIGN1;
typedef uint8_t gg_unaligned_10 GG_ALIGN1;
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, uint64_t value);
static uint8_t gg_eq_2(int64_t a, int64_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, int64_t value);
static uint8_t gg_eq_3(float a, float b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, float value);
static uint8_t gg_eq_4(double a, double b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, double value);
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_type_5 value);
static uint8_t gg_eq_6(int32_t a, int32_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, int32_t value);
static uint8_t gg_eq_7(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, uint8_t value);
static uint8_t gg_eq_9(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, uint32_t value);
static uint8_t gg_eq_10(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((GG_UNALIGNED gg_unaligned_1*)a.data)[i],((GG_UNALIGNED gg_unaligned_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((GG_UNALIGNED gg_unaligned_2*)a.data)[i],((GG_UNALIGNED gg_unaligned_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((GG_UNALIGNED gg_unaligned_3*)a.data)[i],((GG_UNALIGNED gg_unaligned_3*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(float a, float b) {
return a == b;
}
static void gg_render_3(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((GG_UNALIGNED gg_unaligned_4*)a.data)[i],((GG_UNALIGNED gg_unaligned_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(double a, double b) {
return a == b;
}
static void gg_render_4(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((GG_UNALIGNED gg_unaligned_5*)a.data)[i],((GG_UNALIGNED gg_unaligned_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
return a == b;
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
gg_render_3(out,*value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((GG_UNALIGNED gg_unaligned_6*)a.data)[i],((GG_UNALIGNED gg_unaligned_6*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_7(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((GG_UNALIGNED gg_unaligned_9*)a.data)[i],((GG_UNALIGNED gg_unaligned_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_9(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((GG_UNALIGNED gg_unaligned_10*)a.data)[i],((GG_UNALIGNED gg_unaligned_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_10(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static void gg_function_2(gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base);
static void gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static int64_t gg_function_5(uint64_t p0, uint64_t p1, int64_t base);
static float gg_function_12(float p0, float p1, int64_t base);
static float gg_function_13(float p0, int32_t p1, int64_t base);
static float gg_function_15(float p0, int32_t p1, int64_t base);
static void gg_function_22(gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static void gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg180q_fail)(int64_t);
#endif
  fg180q_fail(t2);
  }
  }
  }
gg_done:;
}
static int64_t gg_function_5(uint64_t p0, uint64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
#ifndef __cplusplus
  extern int64_t (fg180q_ld_i32)(uint64_t, uint64_t);
#endif
  t3 = fg180q_ld_i32(t1, t2);
  t0 = ((int64_t)(t3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_15(float p0, int32_t p1, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  uint8_t t3;
  float t4;
  float t5;
  uint32_t t6;
  int32_t t7;
  float t8;
  float t9;
  float t10;
  float t11;
  uint32_t t12;
  int32_t t13;
  float t14;
  float t15;
  float t16;
  float t17;
  uint32_t t18;
  int32_t t19;
  float t20;
  float t21;
  float t22;
  float t23;
  uint32_t t24;
  int32_t t25;
  float t26;
  float t27;
  float t28;
  float t29;
  uint32_t t30;
  int32_t t31;
  float t32;
  float t33;
  float t34;
  float t35;
  uint32_t t36;
  int32_t t37;
  float t38;
  float t39;
  float t40;
  float t41;
  uint32_t t42;
  int32_t t43;
  float t44;
  float t45;
  float v0;
  int32_t v1;
  float v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v2));
  t5 = ((float)(v2));
  t6 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t7 = ((int32_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t8 = fg180q_shfl_xor(t5, t6, t7);
  t9 = ((float)(t8));
  v2 = ((float)(((t4) + (t9))));
  t10 = ((float)(v2));
  t11 = ((float)(v2));
  t12 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t14 = fg180q_shfl_xor(t11, t12, t13);
  t15 = ((float)(t14));
  v2 = ((float)(((t10) + (t15))));
  t16 = ((float)(v2));
  t17 = ((float)(v2));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t19 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t20 = fg180q_shfl_xor(t17, t18, t19);
  t21 = ((float)(t20));
  v2 = ((float)(((t16) + (t21))));
  t22 = ((float)(v2));
  t23 = ((float)(v2));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t25 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t26 = fg180q_shfl_xor(t23, t24, t25);
  t27 = ((float)(t26));
  v2 = ((float)(((t22) + (t27))));
  }
  } else {
  {
  t28 = ((float)(v2));
  t29 = ((float)(v2));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t31 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t32 = fg180q_shfl_xor(t29, t30, t31);
  t33 = ((float)(t32));
  v2 = ((float)(((t28) + (t33))));
  t34 = ((float)(v2));
  t35 = ((float)(v2));
  t36 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t37 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t38 = fg180q_shfl_xor(t35, t36, t37);
  t39 = ((float)(t38));
  v2 = ((float)(((t34) + (t39))));
  t40 = ((float)(v2));
  t41 = ((float)(v2));
  t42 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t44 = fg180q_shfl_xor(t41, t42, t43);
  t45 = ((float)(t44));
  v2 = ((float)(((t40) + (t45))));
  }
  }
  t0 = ((float)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_12(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((float)(v1));
  }
  } else {
  {
  t0 = ((float)(v0));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_13(float p0, int32_t p1, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  uint8_t t3;
  float t4;
  float t5;
  uint32_t t6;
  int32_t t7;
  float t8;
  float t9;
  float t10;
  float t11;
  uint32_t t12;
  int32_t t13;
  float t14;
  float t15;
  float t16;
  float t17;
  uint32_t t18;
  int32_t t19;
  float t20;
  float t21;
  float t22;
  float t23;
  uint32_t t24;
  int32_t t25;
  float t26;
  float t27;
  float t28;
  float t29;
  uint32_t t30;
  int32_t t31;
  float t32;
  float t33;
  float t34;
  float t35;
  uint32_t t36;
  int32_t t37;
  float t38;
  float t39;
  float t40;
  float t41;
  uint32_t t42;
  int32_t t43;
  float t44;
  float t45;
  float v0;
  int32_t v1;
  float v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v2));
  t5 = ((float)(v2));
  t6 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t7 = ((int32_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t8 = fg180q_shfl_xor(t5, t6, t7);
  t9 = ((float)(t8));
  v2 = gg_function_12(t4, t9, base + 0);
  t10 = ((float)(v2));
  t11 = ((float)(v2));
  t12 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t14 = fg180q_shfl_xor(t11, t12, t13);
  t15 = ((float)(t14));
  v2 = gg_function_12(t10, t15, base + 0);
  t16 = ((float)(v2));
  t17 = ((float)(v2));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t19 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t20 = fg180q_shfl_xor(t17, t18, t19);
  t21 = ((float)(t20));
  v2 = gg_function_12(t16, t21, base + 0);
  t22 = ((float)(v2));
  t23 = ((float)(v2));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t25 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t26 = fg180q_shfl_xor(t23, t24, t25);
  t27 = ((float)(t26));
  v2 = gg_function_12(t22, t27, base + 0);
  }
  } else {
  {
  t28 = ((float)(v2));
  t29 = ((float)(v2));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t31 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t32 = fg180q_shfl_xor(t29, t30, t31);
  t33 = ((float)(t32));
  v2 = gg_function_12(t28, t33, base + 0);
  t34 = ((float)(v2));
  t35 = ((float)(v2));
  t36 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t37 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t38 = fg180q_shfl_xor(t35, t36, t37);
  t39 = ((float)(t38));
  v2 = gg_function_12(t34, t39, base + 0);
  t40 = ((float)(v2));
  t41 = ((float)(v2));
  t42 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t44 = fg180q_shfl_xor(t41, t42, t43);
  t45 = ((float)(t44));
  v2 = gg_function_12(t40, t45, base + 0);
  }
  }
  t0 = ((float)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_2(gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint8_t t2;
  int32_t t3;
  int32_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int32_t t31;
  int32_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  int32_t t36;
  int32_t t37;
  uint8_t t38;
  int32_t t39;
  int32_t t40;
  int32_t t41;
  int32_t t42;
  uint64_t t43;
  uint64_t t44;
  uint8_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  gg_type_5 t71;
  uint64_t t72;
  float t73;
  gg_type_5 t74;
  uint64_t t75;
  float t76;
  gg_type_5 t77;
  uint64_t t78;
  float t79;
  gg_type_5 t80;
  uint64_t t81;
  float t82;
  float t83;
  float t84;
  float t85;
  float t86;
  float t87;
  float t88;
  float t89;
  float t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  float t97;
  int32_t t98;
  int32_t t99;
  int32_t t100;
  uint8_t t101;
  float t102;
  float t103;
  float t104;
  float t105;
  float t106;
  float t107;
  float t108;
  int32_t t109;
  float t110;
  float t111;
  float t112;
  float t113;
  float t114;
  float t115;
  float t116;
  int32_t t117;
  float t118;
  float t119;
  float t120;
  float t121;
  float t122;
  int32_t t123;
  float t124;
  float t125;
  float t126;
  float t127;
  float t128;
  int32_t t129;
  float t130;
  float t131;
  float t132;
  float t133;
  float t134;
  int32_t t135;
  int32_t t136;
  int32_t t137;
  int32_t t138;
  int32_t t139;
  int32_t t140;
  int32_t t141;
  int32_t t142;
  int32_t t143;
  int32_t t144;
  int32_t t145;
  int32_t t146;
  int32_t t147;
  int32_t t148;
  int32_t t149;
  int32_t t150;
  int32_t t151;
  int32_t t152;
  int32_t t153;
  int32_t t154;
  int32_t t155;
  float t156;
  float t157;
  int32_t t158;
  int32_t t159;
  uint8_t t160;
  int32_t t161;
  int32_t t162;
  int32_t t163;
  int32_t t164;
  uint8_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  uint64_t t170;
  int64_t t171;
  int64_t t172;
  uint64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int32_t t178;
  int32_t t179;
  int32_t t180;
  int32_t t181;
  int32_t t182;
  int32_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  uint8_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  uint64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  uint64_t t203;
  int32_t t204;
  int32_t t205;
  int32_t t206;
  uint8_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  uint8_t t212;
  uint8_t t213;
  int64_t t214;
  int64_t t215;
  uint8_t t216;
  uint64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  uint64_t t228;
  float t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  uint8_t t234;
  uint64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  uint64_t t244;
  float t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  uint8_t t250;
  int32_t t251;
  int32_t t252;
  uint8_t t253;
  uint64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  uint64_t t261;
  float t262;
  float t263;
  uint64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  uint64_t t271;
  float t272;
  int32_t t273;
  int32_t t274;
  gg_type_5 v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
  int32_t v18;
  int32_t v19;
  int32_t v20;
  int32_t v21;
  int64_t v22;
  int64_t v23;
  int32_t v24;
  int32_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int32_t v29;
  int64_t v30;
  int64_t v31;
  float v32;
  float v33;
  float v34;
  float v35;
  int64_t v36;
  uint64_t v37;
  float v38;
  float v39;
  float v40;
  int32_t v41;
  int32_t v42;
  int32_t v43;
  int32_t v44;
  int32_t v45;
  int32_t v46;
  float v47;
  int32_t v48;
  int32_t v49;
  int64_t v50;
  int64_t v51;
  int64_t v52;
  int32_t v53;
  int64_t v54;
  int64_t v55;
  int64_t v56;
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
  v12 = p12;
  v13 = p13;
  v14 = p14;
  v15 = p15;
  v16 = p16;
  v17 = p17;
  v18 = p18;
  v19 = p19;
  {
  t0 = ((int32_t)(v11));
  t1 = ((int32_t)(((int64_t)UINT64_C(2))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v20 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(64))))));
  }
  } else {
  {
  v20 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  }
  }
  t3 = ((int32_t)(v11));
  t4 = ((int32_t)(((int64_t)UINT64_C(2))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  v21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  }
  } else {
  {
  v21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  }
  }
  t6 = ((int64_t)(((int64_t)(v17))));
  t7 = ((int64_t)(((int64_t)(v14))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = ((int64_t)(((int64_t)(v16))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t12 = ((int64_t)(v22));
  t13 = ((int64_t)(v7));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) > (t16));
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(v8));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t19) > (t20));
  }
  t21 = t18;
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(v9));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t23) > (t24));
  }
  t25 = t22;
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t27) > (t28));
  }
  t29 = t26;
  t30 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_3(t29, t30, base + 0);
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int32_t)(v12));
  t32 = ((int32_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  if (t33) {
  {
  t34 = ((int64_t)(((int64_t)(v13))));
  t35 = ((int64_t)(v5));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  }
  } else {
  {
  v24 = ((int32_t)(v15));
  v25 = ((int32_t)(((int32_t)(v9))));
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((int32_t)(v25));
  t37 = ((int32_t)(((int64_t)UINT64_C(1))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
  v27 = ((int64_t)(((int64_t)(v24))));
  }
  } else {
  {
  t39 = ((int32_t)(v24));
  t40 = ((int32_t)(v25));
  v26 = ((int64_t)(((int64_t)(((int32_t)gg_mod(t39, t40))))));
  t41 = ((int32_t)(v24));
  t42 = ((int32_t)(v25));
  v27 = ((int64_t)(((int64_t)(((int32_t)gg_div(t41, t42))))));
  }
  }
  v28 = ((int64_t)(((int64_t)(v13))));
  t43 = ((uint64_t)(v1));
  t44 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t45 = ((t43) != (t44));
  if (t45) {
  {
  t46 = ((uint64_t)(v1));
  t47 = ((int64_t)(((int64_t)(v13))));
  t48 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t49 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))))));
  v28 = gg_function_5(t46, t49, base + 0);
  }
  }
  t50 = ((int64_t)(v27));
  t51 = ((int64_t)(v6));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(v26));
  t54 = ((int64_t)(v5));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t57 = ((int64_t)(v28));
  t58 = ((int64_t)(v4));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t59)))));
  }
  }
  t60 = ((int64_t)(v22));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  v29 = ((int32_t)(((int32_t)(((t60) >> ((uint64_t)(t61) & 63))))));
  v30 = ((int64_t)(((int64_t)(v29))));
  t62 = ((int64_t)(v22));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(127))))));
  v31 = ((int64_t)(((t62) & (t63))));
  v32 = ((float)(gg_double_bits(UINT64_C(0))));
  v33 = ((float)(gg_double_bits(UINT64_C(0))));
  v34 = ((float)(gg_double_bits(UINT64_C(0))));
  v35 = ((float)(gg_double_bits(UINT64_C(0))));
  t64 = ((int64_t)(v22));
  t65 = ((int64_t)(v3));
  t66 = ((t64) < (t65));
  if (t66) {
  {
  t67 = ((int64_t)(v23));
  t68 = ((int64_t)(v22));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  t69 = ((int64_t)(v36));
  t70 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))))));
  t71 = v0;
  if ((uintptr_t)t71 % _Alignof(float)) gg_fail("unaligned foreign reference");
  t72 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4x)(gg_type_5, uint64_t);
#endif
  t73 = fg180q_ld_f4x(t71, t72);
  v32 = ((float)(t73));
  t74 = v0;
  if ((uintptr_t)t74 % _Alignof(float)) gg_fail("unaligned foreign reference");
  t75 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4y)(gg_type_5, uint64_t);
#endif
  t76 = fg180q_ld_f4y(t74, t75);
  v33 = ((float)(t76));
  t77 = v0;
  if ((uintptr_t)t77 % _Alignof(float)) gg_fail("unaligned foreign reference");
  t78 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4z)(gg_type_5, uint64_t);
#endif
  t79 = fg180q_ld_f4z(t77, t78);
  v34 = ((float)(t79));
  t80 = v0;
  if ((uintptr_t)t80 % _Alignof(float)) gg_fail("unaligned foreign reference");
  t81 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4w)(gg_type_5, uint64_t);
#endif
  t82 = fg180q_ld_f4w(t80, t81);
  v35 = ((float)(t82));
  }
  }
  t83 = ((float)(v32));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t84 = fg180q_fabs(t83);
  v38 = ((float)(t84));
  t85 = ((float)(v38));
  t86 = ((float)(v33));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t87 = fg180q_fabs(t86);
  t88 = ((float)(t87));
  v38 = gg_function_12(t85, t88, base + 0);
  t89 = ((float)(v38));
  t90 = ((float)(v34));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t91 = fg180q_fabs(t90);
  t92 = ((float)(t91));
  v38 = gg_function_12(t89, t92, base + 0);
  t93 = ((float)(v38));
  t94 = ((float)(v35));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t95 = fg180q_fabs(t94);
  t96 = ((float)(t95));
  v38 = gg_function_12(t93, t96, base + 0);
  t97 = ((float)(v38));
  t98 = ((int32_t)(v11));
  v38 = gg_function_13(t97, t98, base + 0);
  v39 = ((float)(gg_double_bits(UINT64_C(0))));
  t99 = ((int32_t)(v11));
  t100 = ((int32_t)(((int64_t)UINT64_C(0))));
  t101 = ((t99) != (t100));
  if (t101) {
  {
  t102 = ((float)(v32));
  t103 = ((float)(v33));
  v39 = ((float)(((t102) + (t103))));
  t104 = ((float)(v39));
  t105 = ((float)(v34));
  v39 = ((float)(((t104) + (t105))));
  t106 = ((float)(v39));
  t107 = ((float)(v35));
  v39 = ((float)(((t106) + (t107))));
  t108 = ((float)(v39));
  t109 = ((int32_t)(v11));
  v39 = gg_function_15(t108, t109, base + 0);
  }
  }
  t110 = ((float)(gg_double_bits(UINT64_C(4638637247447433216))));
  t111 = ((float)(v38));
  v40 = ((float)(((t110) / (t111))));
#ifndef __cplusplus
  extern void (fg180q_pdl_sync)(void);
#endif
  fg180q_pdl_sync();
  t112 = ((float)(v32));
  t113 = ((float)(v40));
  t114 = ((float)(((t112) * (t113))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t115 = fg180q_round(t114);
  t116 = ((float)(t115));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t117 = fg180q_i32(t116);
  v41 = ((int32_t)(t117));
  t118 = ((float)(v33));
  t119 = ((float)(v40));
  t120 = ((float)(((t118) * (t119))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t121 = fg180q_round(t120);
  t122 = ((float)(t121));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t123 = fg180q_i32(t122);
  v42 = ((int32_t)(t123));
  t124 = ((float)(v34));
  t125 = ((float)(v40));
  t126 = ((float)(((t124) * (t125))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t127 = fg180q_round(t126);
  t128 = ((float)(t127));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t129 = fg180q_i32(t128);
  v43 = ((int32_t)(t129));
  t130 = ((float)(v35));
  t131 = ((float)(v40));
  t132 = ((float)(((t130) * (t131))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t133 = fg180q_round(t132);
  t134 = ((float)(t133));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t135 = fg180q_i32(t134);
  v44 = ((int32_t)(t135));
  v45 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(255))))));
  t136 = ((int32_t)(v41));
  t137 = ((int32_t)(v45));
  t138 = ((int32_t)(((t136) & (t137))));
  t139 = ((int32_t)(v42));
  t140 = ((int32_t)(v45));
  t141 = ((int32_t)(((t139) & (t140))));
  t142 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  t143 = ((int32_t)(((int32_t)((uint64_t)(t141) << ((uint64_t)(t142) & 31)))));
  t144 = ((int32_t)(((t138) | (t143))));
  t145 = ((int32_t)(v43));
  t146 = ((int32_t)(v45));
  t147 = ((int32_t)(((t145) & (t146))));
  t148 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  t149 = ((int32_t)(((int32_t)((uint64_t)(t147) << ((uint64_t)(t148) & 31)))));
  t150 = ((int32_t)(((t144) | (t149))));
  t151 = ((int32_t)(v44));
  t152 = ((int32_t)(v45));
  t153 = ((int32_t)(((t151) & (t152))));
  t154 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(24))))));
  t155 = ((int32_t)(((int32_t)((uint64_t)(t153) << ((uint64_t)(t154) & 31)))));
  v46 = ((int32_t)(((t150) | (t155))));
  t156 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t157 = ((float)(v40));
  v47 = ((float)(((t156) / (t157))));
  t158 = ((int32_t)(v12));
  t159 = ((int32_t)(((int64_t)UINT64_C(0))));
  t160 = ((t158) != (t159));
  if (t160) {
  {
  v48 = ((int32_t)(v10));
  }
  } else {
  {
  v48 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  }
  }
  v49 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t161 = ((int32_t)(v49));
  t162 = ((int32_t)(v48));
  if (!(((t161) < (t162)))) goto label2;
  {
  v50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t163 = ((int32_t)(v12));
  t164 = ((int32_t)(((int64_t)UINT64_C(0))));
  t165 = ((t163) != (t164));
  if (t165) {
  {
  t166 = ((int64_t)(((int64_t)(v13))));
  t167 = ((int64_t)(((int64_t)(v10))));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))));
  t169 = ((int64_t)(((int64_t)(v49))));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t168) + (uint64_t)(t169)))));
  t170 = ((uint64_t)(v1));
  t171 = ((int64_t)(v51));
  t172 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t173 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t171) * (uint64_t)(t172)))))));
  v52 = gg_function_5(t170, t173, base + 0);
  t174 = ((int64_t)(v30));
  t175 = ((int64_t)(v8));
  t176 = ((int64_t)(((int64_t)((uint64_t)(t174) * (uint64_t)(t175)))));
  t177 = ((int64_t)(v52));
  v50 = ((int64_t)(((int64_t)((uint64_t)(t176) + (uint64_t)(t177)))));
  }
  } else {
  {
  t178 = ((int32_t)(v18));
  t179 = ((int32_t)(v19));
  t180 = ((int32_t)(((int32_t)((uint64_t)(t178) * (uint64_t)(t179)))));
  t181 = ((int32_t)(v17));
  t182 = ((int32_t)(((int32_t)((uint64_t)(t180) * (uint64_t)(t181)))));
  t183 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(7))))));
  v53 = ((int32_t)(((t182) >> ((uint64_t)(t183) & 31))));
  v54 = ((int64_t)(((int64_t)(v53))));
  t184 = ((int64_t)(((int64_t)(v15))));
  t185 = ((int64_t)(v54));
  v55 = ((int64_t)(((int64_t)((uint64_t)(t184) * (uint64_t)(t185)))));
  t186 = ((int64_t)(v55));
  t187 = ((int64_t)(v30));
  t188 = ((int64_t)(v8));
  t189 = ((int64_t)(((int64_t)((uint64_t)(t187) * (uint64_t)(t188)))));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t186) + (uint64_t)(t189)))));
  t191 = ((int64_t)(((int64_t)(v13))));
  v50 = ((int64_t)(((int64_t)((uint64_t)(t190) + (uint64_t)(t191)))));
  }
  }
  t192 = ((int64_t)(v50));
  t193 = ((int64_t)(((int64_t)UINT64_C(0))));
  t194 = ((t192) >= (t193));
  t195 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_3(t194, t195, base + 0);
  t196 = ((int64_t)(v50));
  t197 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(144))))));
  v56 = ((int64_t)(((int64_t)((uint64_t)(t196) * (uint64_t)(t197)))));
  t198 = ((uint64_t)(v2));
  t199 = ((int64_t)(v56));
  t200 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t201 = ((int64_t)(((int64_t)((uint64_t)(t199) + (uint64_t)(t200)))));
  t202 = ((int64_t)(v31));
  t203 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t201) + (uint64_t)(t202)))))));
  t204 = ((int32_t)(v46));
#ifndef __cplusplus
  extern void (fg180q_st_i32)(uint64_t, uint64_t, int32_t);
#endif
  fg180q_st_i32(t198, t203, t204);
  t205 = ((int32_t)(v11));
  t206 = ((int32_t)(((int64_t)UINT64_C(2))));
  t207 = ((t205) == (t206));
  if (t207) {
  {
  t208 = ((int64_t)(v31));
  t209 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(15))))));
  t210 = ((int64_t)(((t208) & (t209))));
  t211 = ((int64_t)(((int64_t)UINT64_C(0))));
  t212 = ((t210) == (t211));
  t213 = t212;
  if (t213) {
  t214 = ((int64_t)(((int64_t)UINT64_C(96))));
  t215 = ((int64_t)(v31));
  t213 = ((t214) > (t215));
  }
  t216 = t213;
  if (t216) {
  {
  t217 = ((uint64_t)(v2));
  t218 = ((int64_t)(v56));
  t219 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t220 = ((int64_t)(((int64_t)((uint64_t)(t218) + (uint64_t)(t219)))));
  t221 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t222 = ((int64_t)(v31));
  t223 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t224 = ((int64_t)(((t222) >> ((uint64_t)(t223) & 63))));
  t225 = ((int64_t)(((int64_t)((uint64_t)(t221) + (uint64_t)(t224)))));
  t226 = ((int64_t)(((int64_t)UINT64_C(2))));
  t227 = ((int64_t)(((int64_t)((uint64_t)(t225) * (uint64_t)(t226)))));
  t228 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t220) + (uint64_t)(t227)))))));
  t229 = ((float)(v39));
#ifndef __cplusplus
  extern void (fg180q_st_f32)(uint64_t, uint64_t, float);
#endif
  fg180q_st_f32(t217, t228, t229);
  t230 = ((int64_t)(v31));
  t231 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(63))))));
  t232 = ((int64_t)(((t230) & (t231))));
  t233 = ((int64_t)(((int64_t)UINT64_C(0))));
  t234 = ((t232) == (t233));
  if (t234) {
  {
  t235 = ((uint64_t)(v2));
  t236 = ((int64_t)(v56));
  t237 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t238 = ((int64_t)(((int64_t)((uint64_t)(t236) + (uint64_t)(t237)))));
  t239 = ((int64_t)(v31));
  t240 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t241 = ((int64_t)(((t239) >> ((uint64_t)(t240) & 63))));
  t242 = ((int64_t)(((int64_t)UINT64_C(2))));
  t243 = ((int64_t)(((int64_t)((uint64_t)(t241) * (uint64_t)(t242)))));
  t244 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t238) + (uint64_t)(t243)))))));
  t245 = ((float)(v47));
#ifndef __cplusplus
  extern void (fg180q_st_f32)(uint64_t, uint64_t, float);
#endif
  fg180q_st_f32(t235, t244, t245);
  }
  }
  }
  }
  }
  } else {
  {
  t246 = ((int64_t)(v31));
  t247 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(31))))));
  t248 = ((int64_t)(((t246) & (t247))));
  t249 = ((int64_t)(((int64_t)UINT64_C(0))));
  t250 = ((t248) == (t249));
  if (t250) {
  {
  t251 = ((int32_t)(v11));
  t252 = ((int32_t)(((int64_t)UINT64_C(1))));
  t253 = ((t251) == (t252));
  if (t253) {
  {
  t254 = ((uint64_t)(v2));
  t255 = ((int64_t)(v56));
  t256 = ((int64_t)(v31));
  t257 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t258 = ((int64_t)(((t256) >> ((uint64_t)(t257) & 63))));
  t259 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t260 = ((int64_t)(((int64_t)((uint64_t)(t258) * (uint64_t)(t259)))));
  t261 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t255) + (uint64_t)(t260)))))));
  t262 = ((float)(v47));
  t263 = ((float)(v39));
#ifndef __cplusplus
  extern void (fg180q_st_half2)(uint64_t, uint64_t, float, float);
#endif
  fg180q_st_half2(t254, t261, t262, t263);
  }
  } else {
  {
  t264 = ((uint64_t)(v2));
  t265 = ((int64_t)(v56));
  t266 = ((int64_t)(v31));
  t267 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t268 = ((int64_t)(((t266) >> ((uint64_t)(t267) & 63))));
  t269 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t268) * (uint64_t)(t269)))));
  t271 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t265) + (uint64_t)(t270)))))));
  t272 = ((float)(v47));
#ifndef __cplusplus
  extern void (fg180q_st_f32)(uint64_t, uint64_t, float);
#endif
  fg180q_st_f32(t264, t271, t272);
  }
  }
  }
  }
  }
  }
  t273 = v49;
  t274 = ((int32_t)(((int64_t)UINT64_C(1))));
  v49 = ((int32_t)((uint64_t)(t273) + (uint64_t)(t274)));
  }
  goto label1;
label2:;
  }
  }
  }
gg_done:;
}
static void gg_function_22(gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  gg_type_5 t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  int32_t t19;
  gg_type_5 v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
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
  v12 = p12;
  v13 = p13;
  v14 = p14;
  v15 = p15;
  v16 = p16;
  v17 = p17;
  {
  t0 = v0;
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
gg_done:;
}
static void gg_function_1(gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  gg_type_5 t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  int32_t t19;
  gg_type_5 v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
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
  v12 = p12;
  v13 = p13;
  v14 = p14;
  v15 = p15;
  v16 = p16;
  v17 = p17;
  {
  t0 = v0;
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  gg_type_5 t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  gg_type_5 t18;
  uint64_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int32_t t28;
  int32_t t29;
  int32_t t30;
  int32_t t31;
  int32_t t32;
  int32_t t33;
  int32_t t34;
  int32_t t35;
  uint64_t v0;
  float v1;
  {
  v0 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v1 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t0 = &(v1);
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v0));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t10 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t11 = ((int32_t)(((int64_t)UINT64_C(0))));
  t12 = ((int32_t)(((int64_t)UINT64_C(0))));
  t13 = ((int32_t)(((int64_t)UINT64_C(0))));
  t14 = ((int32_t)(((int64_t)UINT64_C(0))));
  t15 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t16 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t17 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  gg_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, base + 0);
  t18 = &(v1);
  t19 = ((uint64_t)(v0));
  t20 = ((uint64_t)(v0));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t28 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t29 = ((int32_t)(((int64_t)UINT64_C(0))));
  t30 = ((int32_t)(((int64_t)UINT64_C(0))));
  t31 = ((int32_t)(((int64_t)UINT64_C(0))));
  t32 = ((int32_t)(((int64_t)UINT64_C(0))));
  t33 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t34 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t35 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  gg_function_22(t18, t19, t20, t21, t22, t23, t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, t34, t35, base + 0);
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
