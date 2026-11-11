#ifndef FGOOSE_PACKED
#define FGOOSE_PACKED 1
#endif
#if defined(__AVX2__)
#include <immintrin.h>
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


static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, uint64_t value);
static uint8_t gg_eq_2(int64_t a, int64_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, int64_t value);
static uint8_t gg_eq_3(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint8_t value);
static uint8_t gg_eq_4(float a, float b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, float value);
static uint8_t gg_eq_5(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, uint32_t value);
static uint8_t gg_eq_7(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(float a, float b) {
return a == b;
}
static void gg_render_4(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_7(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static int64_t gg_function_5(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t base);
static void gg_function_9(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
gg_done:;
  return t0;
}
static int64_t gg_function_5(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint32_t t8;
  uint32_t t9;
  uint32_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  uint32_t t17;
  uint32_t t18;
  uint32_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  uint32_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint32_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  uint32_t t38;
  uint32_t t39;
  uint32_t t40;
  int64_t t41;
  uint32_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint32_t t47;
  int64_t t48;
  int64_t t49;
  uint32_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint32_t v5;
  uint32_t v6;
  uint32_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t4 = ((uint64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v2));
  t7 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t8 = fg265d_ld_u8(t4, t7);
  t9 = ((uint32_t)(t8));
  t10 = ((uint32_t)(((int64_t)UINT64_C(63))));
  v3 = ((int64_t)(((int64_t)(((t9) & (t10))))));
  t11 = ((uint64_t)(v0));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t16 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t17 = fg265d_ld_u8(t11, t16);
  t18 = ((uint32_t)(t17));
  t19 = ((uint32_t)(((int64_t)UINT64_C(63))));
  v4 = ((int64_t)(((int64_t)(((t18) & (t19))))));
  }
  } else {
  {
  t20 = ((uint64_t)(v0));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t25 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t26 = fg265d_ld_u8(t20, t25);
  v5 = ((uint32_t)(t26));
  t27 = ((uint64_t)(v0));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t30) - (uint64_t)(t31)))))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t33 = fg265d_ld_u8(t27, t32);
  v6 = ((uint32_t)(t33));
  t34 = ((uint64_t)(v0));
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(v2));
  t37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t38 = fg265d_ld_u8(t34, t37);
  v7 = ((uint32_t)(t38));
  t39 = ((uint32_t)(v5));
  t40 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t41 = ((int64_t)(((int64_t)(((t39) & (t40))))));
  t42 = ((uint32_t)(v6));
  t43 = ((int64_t)(((int64_t)UINT64_C(6))));
  t44 = ((int64_t)(((int64_t)(((t42) >> ((uint64_t)(t43) & 31))))));
  t45 = ((int64_t)(((int64_t)UINT64_C(4))));
  t46 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t44) << ((uint64_t)(t45) & 63)))))));
  v3 = ((int64_t)(((t41) | (t46))));
  t47 = ((uint32_t)(v5));
  t48 = ((int64_t)(((int64_t)UINT64_C(4))));
  t49 = ((int64_t)(((int64_t)(((t47) >> ((uint64_t)(t48) & 31))))));
  t50 = ((uint32_t)(v7));
  t51 = ((int64_t)(((int64_t)UINT64_C(6))));
  t52 = ((int64_t)(((int64_t)(((t50) >> ((uint64_t)(t51) & 31))))));
  t53 = ((int64_t)(((int64_t)UINT64_C(4))));
  t54 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t52) << ((uint64_t)(t53) & 63)))))));
  v4 = ((int64_t)(((t49) | (t54))));
  }
  }
  t55 = ((int64_t)(v3));
  t56 = ((int64_t)(((int64_t)UINT64_C(8))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) << ((uint64_t)(t56) & 63)))));
  t58 = ((int64_t)(v4));
  t0 = ((int64_t)(((t57) | (t58))));
  }
gg_done:;
  return t0;
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
  extern void (fg265d_fail)(int64_t);
#endif
  fg265d_fail(t2);
  }
  }
  }
gg_done:;
}
static void gg_function_9(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
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
  uint64_t t26;
  uint64_t t27;
  float t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  float t33;
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
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  float t62;
  int64_t t63;
  int64_t t64;
  float t65;
  float t66;
  int64_t t67;
  int64_t t68;
  float t69;
  float t70;
  int64_t t71;
  int64_t t72;
  float t73;
  float t74;
  int64_t t75;
  int64_t t76;
  float t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint32_t t86;
  uint64_t t87;
  int64_t t88;
  int64_t t89;
  uint64_t t90;
  uint32_t t91;
  uint64_t t92;
  uint64_t t93;
  uint32_t t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  uint64_t t98;
  uint32_t t99;
  uint32_t t100;
  uint32_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  uint32_t t108;
  uint32_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint32_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint32_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  uint64_t t132;
  int64_t t133;
  int64_t t134;
  uint64_t t135;
  float t136;
  float t137;
  float t138;
  float t139;
  float t140;
  uint64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint64_t t146;
  float t147;
  float t148;
  float t149;
  float t150;
  float t151;
  uint64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  uint64_t t157;
  float t158;
  float t159;
  float t160;
  float t161;
  float t162;
  uint64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint64_t t168;
  float t169;
  float t170;
  float t171;
  float t172;
  float t173;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  float v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  float v15;
  float v16;
  float v17;
  float v18;
  int64_t v19;
  int64_t v20;
  uint32_t v21;
  uint32_t v22;
  uint32_t v23;
  uint32_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(16))));
  v4 = ((int64_t)(((int64_t)gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(16))));
  v5 = ((int64_t)(((int64_t)gg_mod(t10, t11))));
  t12 = ((int64_t)(((int64_t)UINT64_C(2))));
  t13 = ((int64_t)(v4));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(176))))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)UINT64_C(64))));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  t24 = ((int64_t)(v5));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t25)))));
  t26 = ((uint64_t)(v0));
  t27 = ((uint64_t)(((uint64_t)(v7))));
#ifndef __cplusplus
  extern float (fg265d_ld_half)(uint64_t, uint64_t);
#endif
  t28 = fg265d_ld_half(t26, t27);
  v9 = ((float)(t28));
  t29 = ((uint64_t)(v0));
  t30 = ((int64_t)(v7));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))))));
#ifndef __cplusplus
  extern float (fg265d_ld_half)(uint64_t, uint64_t);
#endif
  t33 = fg265d_ld_half(t29, t32);
  v10 = ((float)(t33));
  t34 = ((int64_t)(v7));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(48))))));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)UINT64_C(32))));
  t38 = ((int64_t)(v4));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t39)))));
  t41 = ((int64_t)(((int64_t)UINT64_C(2))));
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))));
  t44 = ((int64_t)(v7));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)UINT64_C(2))));
  t48 = ((int64_t)(v5));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t49)))));
  t50 = ((uint64_t)(v0));
  t51 = ((int64_t)(v7));
  t52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) + (uint64_t)(t52)))));
  t54 = ((int64_t)(v6));
  v13 = gg_function_5(t50, t53, t54, base + 0);
  t55 = ((uint64_t)(v0));
  t56 = ((int64_t)(v7));
  t57 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  t59 = ((int64_t)(v6));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t60)))));
  v14 = gg_function_5(t55, t58, t61, base + 0);
  t62 = ((float)(v9));
  t63 = ((int64_t)(v13));
  t64 = ((int64_t)(((int64_t)UINT64_C(8))));
  t65 = ((float)(((float)(((t63) >> ((uint64_t)(t64) & 63))))));
  v15 = ((float)(((t62) * (t65))));
  t66 = ((float)(v10));
  t67 = ((int64_t)(v13));
  t68 = ((int64_t)(((int64_t)UINT64_C(255))));
  t69 = ((float)(((float)(((t67) & (t68))))));
  v16 = ((float)(((t66) * (t69))));
  t70 = ((float)(v9));
  t71 = ((int64_t)(v14));
  t72 = ((int64_t)(((int64_t)UINT64_C(8))));
  t73 = ((float)(((float)(((t71) >> ((uint64_t)(t72) & 63))))));
  v17 = ((float)(((t70) * (t73))));
  t74 = ((float)(v10));
  t75 = ((int64_t)(v14));
  t76 = ((int64_t)(((int64_t)UINT64_C(255))));
  t77 = ((float)(((float)(((t75) & (t76))))));
  v18 = ((float)(((t74) * (t77))));
  t78 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((int64_t)(v4));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t78) << ((uint64_t)(t81) & 63)))));
  t82 = ((int64_t)(v19));
  t83 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t82) << ((uint64_t)(t83) & 63)))));
  t84 = ((uint64_t)(v0));
  t85 = ((uint64_t)(((uint64_t)(v11))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t86 = fg265d_ld_u8(t84, t85);
  v21 = ((uint32_t)(t86));
  t87 = ((uint64_t)(v0));
  t88 = ((int64_t)(v11));
  t89 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t90 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t91 = fg265d_ld_u8(t87, t90);
  v22 = ((uint32_t)(t91));
  t92 = ((uint64_t)(v0));
  t93 = ((uint64_t)(((uint64_t)(v12))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t94 = fg265d_ld_u8(t92, t93);
  v23 = ((uint32_t)(t94));
  t95 = ((uint64_t)(v0));
  t96 = ((int64_t)(v12));
  t97 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t98 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t99 = fg265d_ld_u8(t95, t98);
  v24 = ((uint32_t)(t99));
  t100 = ((uint32_t)(v21));
  t101 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t102 = ((int64_t)(((int64_t)(((t100) & (t101))))));
  t103 = ((int64_t)(((int64_t)(v23))));
  t104 = ((int64_t)(v19));
  t105 = ((int64_t)(((t103) & (t104))));
  t106 = gg_function_7(t105, base + 0);
  t107 = ((int64_t)(t106));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t107)))));
  t108 = ((uint32_t)(v22));
  t109 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t110 = ((int64_t)(((int64_t)(((t108) & (t109))))));
  t111 = ((int64_t)(((int64_t)(v24))));
  t112 = ((int64_t)(v19));
  t113 = ((int64_t)(((t111) & (t112))));
  t114 = gg_function_7(t113, base + 0);
  t115 = ((int64_t)(t114));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t115)))));
  t116 = ((uint32_t)(v21));
  t117 = ((int64_t)(((int64_t)UINT64_C(4))));
  t118 = ((int64_t)(((int64_t)(((t116) >> ((uint64_t)(t117) & 31))))));
  t119 = ((int64_t)(((int64_t)(v23))));
  t120 = ((int64_t)(v20));
  t121 = ((int64_t)(((t119) & (t120))));
  t122 = gg_function_7(t121, base + 0);
  t123 = ((int64_t)(t122));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t118) + (uint64_t)(t123)))));
  t124 = ((uint32_t)(v22));
  t125 = ((int64_t)(((int64_t)UINT64_C(4))));
  t126 = ((int64_t)(((int64_t)(((t124) >> ((uint64_t)(t125) & 31))))));
  t127 = ((int64_t)(((int64_t)(v24))));
  t128 = ((int64_t)(v20));
  t129 = ((int64_t)(((t127) & (t128))));
  t130 = gg_function_7(t129, base + 0);
  t131 = ((int64_t)(t130));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t131)))));
  t132 = ((uint64_t)(v1));
  t133 = ((int64_t)(v8));
  t134 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t135 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t133) * (uint64_t)(t134)))))));
  t136 = ((float)(v15));
  t137 = ((float)(((float)(v25))));
  t138 = ((float)(((t136) * (t137))));
  t139 = ((float)(v16));
  t140 = ((float)(((t138) - (t139))));
#ifndef __cplusplus
  extern void (fg265d_st_bf16)(uint64_t, uint64_t, float);
#endif
  fg265d_st_bf16(t132, t135, t140);
  t141 = ((uint64_t)(v1));
  t142 = ((int64_t)(v8));
  t143 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t142) + (uint64_t)(t143)))));
  t145 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t146 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t145)))))));
  t147 = ((float)(v15));
  t148 = ((float)(((float)(v26))));
  t149 = ((float)(((t147) * (t148))));
  t150 = ((float)(v16));
  t151 = ((float)(((t149) - (t150))));
#ifndef __cplusplus
  extern void (fg265d_st_bf16)(uint64_t, uint64_t, float);
#endif
  fg265d_st_bf16(t141, t146, t151);
  t152 = ((uint64_t)(v1));
  t153 = ((int64_t)(v8));
  t154 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t154)))));
  t156 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t157 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t155) * (uint64_t)(t156)))))));
  t158 = ((float)(v17));
  t159 = ((float)(((float)(v27))));
  t160 = ((float)(((t158) * (t159))));
  t161 = ((float)(v18));
  t162 = ((float)(((t160) - (t161))));
#ifndef __cplusplus
  extern void (fg265d_st_bf16)(uint64_t, uint64_t, float);
#endif
  fg265d_st_bf16(t152, t157, t162);
  t163 = ((uint64_t)(v1));
  t164 = ((int64_t)(v8));
  t165 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(33))))));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t165)))));
  t167 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t168 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))))));
  t169 = ((float)(v17));
  t170 = ((float)(((float)(v28))));
  t171 = ((float)(((t169) * (t170))));
  t172 = ((float)(v18));
  t173 = ((float)(((t171) - (t172))));
#ifndef __cplusplus
  extern void (fg265d_st_bf16)(uint64_t, uint64_t, float);
#endif
  fg265d_st_bf16(t163, t168, t173);
  }
gg_done:;
}
static void gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
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
  uint64_t t26;
  uint64_t t27;
  float t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  float t33;
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
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  float t62;
  int64_t t63;
  int64_t t64;
  float t65;
  float t66;
  int64_t t67;
  int64_t t68;
  float t69;
  float t70;
  int64_t t71;
  int64_t t72;
  float t73;
  float t74;
  int64_t t75;
  int64_t t76;
  float t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint32_t t86;
  uint64_t t87;
  int64_t t88;
  int64_t t89;
  uint64_t t90;
  uint32_t t91;
  uint64_t t92;
  uint64_t t93;
  uint32_t t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  uint64_t t98;
  uint32_t t99;
  uint32_t t100;
  uint32_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  uint32_t t108;
  uint32_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint32_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint32_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  uint64_t t132;
  int64_t t133;
  int64_t t134;
  uint64_t t135;
  float t136;
  float t137;
  float t138;
  float t139;
  float t140;
  uint64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint64_t t146;
  float t147;
  float t148;
  float t149;
  float t150;
  float t151;
  uint64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  uint64_t t157;
  float t158;
  float t159;
  float t160;
  float t161;
  float t162;
  uint64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint64_t t168;
  float t169;
  float t170;
  float t171;
  float t172;
  float t173;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  float v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  float v15;
  float v16;
  float v17;
  float v18;
  int64_t v19;
  int64_t v20;
  uint32_t v21;
  uint32_t v22;
  uint32_t v23;
  uint32_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(16))));
  v4 = ((int64_t)(((int64_t)gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(16))));
  v5 = ((int64_t)(((int64_t)gg_mod(t10, t11))));
  t12 = ((int64_t)(((int64_t)UINT64_C(2))));
  t13 = ((int64_t)(v4));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(176))))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)UINT64_C(64))));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  t24 = ((int64_t)(v5));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t25)))));
  t26 = ((uint64_t)(v0));
  t27 = ((uint64_t)(((uint64_t)(v7))));
#ifndef __cplusplus
  extern float (fg265d_ld_half)(uint64_t, uint64_t);
#endif
  t28 = fg265d_ld_half(t26, t27);
  v9 = ((float)(t28));
  t29 = ((uint64_t)(v0));
  t30 = ((int64_t)(v7));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))))));
#ifndef __cplusplus
  extern float (fg265d_ld_half)(uint64_t, uint64_t);
#endif
  t33 = fg265d_ld_half(t29, t32);
  v10 = ((float)(t33));
  t34 = ((int64_t)(v7));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(48))))));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)UINT64_C(32))));
  t38 = ((int64_t)(v4));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t39)))));
  t41 = ((int64_t)(((int64_t)UINT64_C(2))));
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))));
  t44 = ((int64_t)(v7));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)UINT64_C(2))));
  t48 = ((int64_t)(v5));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t49)))));
  t50 = ((uint64_t)(v0));
  t51 = ((int64_t)(v7));
  t52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) + (uint64_t)(t52)))));
  t54 = ((int64_t)(v6));
  v13 = gg_function_5(t50, t53, t54, base + 0);
  t55 = ((uint64_t)(v0));
  t56 = ((int64_t)(v7));
  t57 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  t59 = ((int64_t)(v6));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t60)))));
  v14 = gg_function_5(t55, t58, t61, base + 0);
  t62 = ((float)(v9));
  t63 = ((int64_t)(v13));
  t64 = ((int64_t)(((int64_t)UINT64_C(8))));
  t65 = ((float)(((float)(((t63) >> ((uint64_t)(t64) & 63))))));
  v15 = ((float)(((t62) * (t65))));
  t66 = ((float)(v10));
  t67 = ((int64_t)(v13));
  t68 = ((int64_t)(((int64_t)UINT64_C(255))));
  t69 = ((float)(((float)(((t67) & (t68))))));
  v16 = ((float)(((t66) * (t69))));
  t70 = ((float)(v9));
  t71 = ((int64_t)(v14));
  t72 = ((int64_t)(((int64_t)UINT64_C(8))));
  t73 = ((float)(((float)(((t71) >> ((uint64_t)(t72) & 63))))));
  v17 = ((float)(((t70) * (t73))));
  t74 = ((float)(v10));
  t75 = ((int64_t)(v14));
  t76 = ((int64_t)(((int64_t)UINT64_C(255))));
  t77 = ((float)(((float)(((t75) & (t76))))));
  v18 = ((float)(((t74) * (t77))));
  t78 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((int64_t)(v4));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t78) << ((uint64_t)(t81) & 63)))));
  t82 = ((int64_t)(v19));
  t83 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t82) << ((uint64_t)(t83) & 63)))));
  t84 = ((uint64_t)(v0));
  t85 = ((uint64_t)(((uint64_t)(v11))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t86 = fg265d_ld_u8(t84, t85);
  v21 = ((uint32_t)(t86));
  t87 = ((uint64_t)(v0));
  t88 = ((int64_t)(v11));
  t89 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t90 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t91 = fg265d_ld_u8(t87, t90);
  v22 = ((uint32_t)(t91));
  t92 = ((uint64_t)(v0));
  t93 = ((uint64_t)(((uint64_t)(v12))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t94 = fg265d_ld_u8(t92, t93);
  v23 = ((uint32_t)(t94));
  t95 = ((uint64_t)(v0));
  t96 = ((int64_t)(v12));
  t97 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t98 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))))));
#ifndef __cplusplus
  extern uint32_t (fg265d_ld_u8)(uint64_t, uint64_t);
#endif
  t99 = fg265d_ld_u8(t95, t98);
  v24 = ((uint32_t)(t99));
  t100 = ((uint32_t)(v21));
  t101 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t102 = ((int64_t)(((int64_t)(((t100) & (t101))))));
  t103 = ((int64_t)(((int64_t)(v23))));
  t104 = ((int64_t)(v19));
  t105 = ((int64_t)(((t103) & (t104))));
  t106 = gg_function_7(t105, base + 0);
  t107 = ((int64_t)(t106));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t107)))));
  t108 = ((uint32_t)(v22));
  t109 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t110 = ((int64_t)(((int64_t)(((t108) & (t109))))));
  t111 = ((int64_t)(((int64_t)(v24))));
  t112 = ((int64_t)(v19));
  t113 = ((int64_t)(((t111) & (t112))));
  t114 = gg_function_7(t113, base + 0);
  t115 = ((int64_t)(t114));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t115)))));
  t116 = ((uint32_t)(v21));
  t117 = ((int64_t)(((int64_t)UINT64_C(4))));
  t118 = ((int64_t)(((int64_t)(((t116) >> ((uint64_t)(t117) & 31))))));
  t119 = ((int64_t)(((int64_t)(v23))));
  t120 = ((int64_t)(v20));
  t121 = ((int64_t)(((t119) & (t120))));
  t122 = gg_function_7(t121, base + 0);
  t123 = ((int64_t)(t122));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t118) + (uint64_t)(t123)))));
  t124 = ((uint32_t)(v22));
  t125 = ((int64_t)(((int64_t)UINT64_C(4))));
  t126 = ((int64_t)(((int64_t)(((t124) >> ((uint64_t)(t125) & 31))))));
  t127 = ((int64_t)(((int64_t)(v24))));
  t128 = ((int64_t)(v20));
  t129 = ((int64_t)(((t127) & (t128))));
  t130 = gg_function_7(t129, base + 0);
  t131 = ((int64_t)(t130));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t131)))));
  t132 = ((uint64_t)(v1));
  t133 = ((int64_t)(v8));
  t134 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t135 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t133) * (uint64_t)(t134)))))));
  t136 = ((float)(v15));
  t137 = ((float)(((float)(v25))));
  t138 = ((float)(((t136) * (t137))));
  t139 = ((float)(v16));
  t140 = ((float)(((t138) - (t139))));
#ifndef __cplusplus
  extern void (fg265d_st_half)(uint64_t, uint64_t, float);
#endif
  fg265d_st_half(t132, t135, t140);
  t141 = ((uint64_t)(v1));
  t142 = ((int64_t)(v8));
  t143 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t142) + (uint64_t)(t143)))));
  t145 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t146 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t145)))))));
  t147 = ((float)(v15));
  t148 = ((float)(((float)(v26))));
  t149 = ((float)(((t147) * (t148))));
  t150 = ((float)(v16));
  t151 = ((float)(((t149) - (t150))));
#ifndef __cplusplus
  extern void (fg265d_st_half)(uint64_t, uint64_t, float);
#endif
  fg265d_st_half(t141, t146, t151);
  t152 = ((uint64_t)(v1));
  t153 = ((int64_t)(v8));
  t154 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t154)))));
  t156 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t157 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t155) * (uint64_t)(t156)))))));
  t158 = ((float)(v17));
  t159 = ((float)(((float)(v27))));
  t160 = ((float)(((t158) * (t159))));
  t161 = ((float)(v18));
  t162 = ((float)(((t160) - (t161))));
#ifndef __cplusplus
  extern void (fg265d_st_half)(uint64_t, uint64_t, float);
#endif
  fg265d_st_half(t152, t157, t162);
  t163 = ((uint64_t)(v1));
  t164 = ((int64_t)(v8));
  t165 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(33))))));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t165)))));
  t167 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t168 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))))));
  t169 = ((float)(v17));
  t170 = ((float)(((float)(v28))));
  t171 = ((float)(((t169) * (t170))));
  t172 = ((float)(v18));
  t173 = ((float)(((t171) - (t172))));
#ifndef __cplusplus
  extern void (fg265d_st_half)(uint64_t, uint64_t, float);
#endif
  fg265d_st_half(t163, t168, t173);
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  uint64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  {
  t0 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t1 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_1(t0, t1, t2, t3, base + 0);
  t4 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t5 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_9(t4, t5, t6, t7, base + 0);
  }
gg_done:;
}
static int gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  gg_argc = argc; gg_argv = argv;
  gg_function_0(frame.base + 0);
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
