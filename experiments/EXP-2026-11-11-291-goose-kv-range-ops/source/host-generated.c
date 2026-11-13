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
static uint8_t gg_eq_1(int64_t a, int64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, int64_t value);
static uint8_t gg_eq_2(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint8_t value);
static uint8_t gg_eq_3(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint64_t value);
static uint8_t gg_eq_5(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
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
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static uint8_t gg_function_1(int64_t p0, int64_t base);
static void gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static int64_t gg_function_5(int64_t base);
static uint8_t gg_function_7(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t base);
static uint8_t gg_function_9(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_11(int64_t base);
static int64_t gg_function_12(int64_t p0, int64_t base);
static int64_t gg_function_13(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_15(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_16(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_17(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_18(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_19(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_20(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_21(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_23(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static void gg_function_24(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static void gg_function_25(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static uint8_t gg_function_26(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_27(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_28(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_30(int64_t p0, int64_t p1, int64_t base);
static void gg_function_31(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_32(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_33(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_34(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static void gg_function_35(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static void gg_function_36(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static void gg_function_37(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
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
  extern void (fg291k_fail)(int64_t);
#endif
  fg291k_fail(t2);
  }
  }
  }
gg_done:;
}
static int64_t gg_function_11(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_17(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(6))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v0));
  t5 = gg_function_11(base + 0);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t6)))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t7)))));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t13)))));
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_25(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = gg_function_17(t1, t2, t3, base + 0);
  t5 = ((int64_t)(v4));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t0, t4, t5);
  }
gg_done:;
}
static int64_t gg_function_19(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(6))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v0));
  t5 = gg_function_11(base + 0);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t6)))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t7)))));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t11)))));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_24(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = gg_function_19(t1, t2, t3, base + 0);
  t5 = ((int64_t)(v4));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t0, t4, t5);
  }
gg_done:;
}
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(6))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v0));
  t5 = gg_function_11(base + 0);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t6)))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t7)))));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_23(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = gg_function_22(t1, t2, t3, base + 0);
  t5 = ((int64_t)(v4));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t0, t4, t5);
  }
gg_done:;
}
static int64_t gg_function_18(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v3));
  t5 = gg_function_19(t2, t3, t4, base + 0);
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t6 = fg291k_ld_i64(t1, t5);
  t0 = ((int64_t)(t6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_21(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v3));
  t5 = gg_function_22(t2, t3, t4, base + 0);
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t6 = fg291k_ld_i64(t1, t5);
  t0 = ((int64_t)(t6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_16(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v3));
  t5 = gg_function_17(t2, t3, t4, base + 0);
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t6 = fg291k_ld_i64(t1, t5);
  t0 = ((int64_t)(t6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_36(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  uint8_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
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
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint64_t t45;
  int64_t t46;
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
  uint64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  uint8_t v9;
  int64_t v10;
  int64_t v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(v3));
  v6 = gg_function_16(t0, t1, t2, t3, base + 0);
  t4 = ((int64_t)(v6));
  t5 = ((int64_t)(v1));
  t6 = ((t4) < (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(51))));
  gg_function_3(t6, t7, base + 0);
  v7 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  v9 = ((uint8_t)UINT64_C(0));
label1:;
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v6));
  if (!(((t8) < (t9)))) goto label2;
  {
  t10 = v9;
  t11 = (!(t10));
  if (t11) {
  {
  t12 = ((uint64_t)(v0));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(v8));
  v10 = gg_function_21(t12, t13, t14, t15, base + 0);
  t16 = ((int64_t)(v10));
  t17 = ((int64_t)(v4));
  t18 = ((t16) > (t17));
  if (t18) {
  {
  v7 = ((int64_t)(v8));
  v9 = ((uint8_t)UINT64_C(1));
  }
  } else {
  {
  t19 = ((int64_t)(v10));
  t20 = ((int64_t)(v4));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = ((uint64_t)(v0));
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(v8));
  t26 = gg_function_18(t22, t23, t24, t25, base + 0);
  t27 = ((int64_t)(t26));
  t28 = ((int64_t)(v5));
  t29 = ((t27) > (t28));
  if (t29) {
  {
  v7 = ((int64_t)(v8));
  v9 = ((uint8_t)UINT64_C(1));
  }
  }
  }
  }
  }
  }
  }
  }
  t30 = ((int64_t)(v8));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  }
  goto label1;
label2:;
  v11 = ((int64_t)(v6));
label4:;
  t32 = ((int64_t)(v11));
  t33 = ((int64_t)(v7));
  if (!(((t32) > (t33)))) goto label5;
  {
  t34 = ((uint64_t)(v0));
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(v3));
  t37 = ((int64_t)(v11));
  t38 = ((uint64_t)(v0));
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(v3));
  t41 = ((int64_t)(v11));
  t42 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) - (uint64_t)(t42)))));
  t44 = gg_function_21(t38, t39, t40, t43, base + 0);
  gg_function_23(t34, t35, t36, t37, t44, base + 0);
  t45 = ((uint64_t)(v0));
  t46 = ((int64_t)(v1));
  t47 = ((int64_t)(v3));
  t48 = ((int64_t)(v11));
  t49 = ((uint64_t)(v0));
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(v3));
  t52 = ((int64_t)(v11));
  t53 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) - (uint64_t)(t53)))));
  t55 = gg_function_18(t49, t50, t51, t54, base + 0);
  gg_function_24(t45, t46, t47, t48, t55, base + 0);
  t56 = ((int64_t)(v11));
  t57 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t56) - (uint64_t)(t57)))));
  }
  goto label4;
label5:;
  t58 = ((uint64_t)(v0));
  t59 = ((int64_t)(v1));
  t60 = ((int64_t)(v3));
  t61 = ((int64_t)(v7));
  t62 = ((int64_t)(v4));
  gg_function_23(t58, t59, t60, t61, t62, base + 0);
  t63 = ((uint64_t)(v0));
  t64 = ((int64_t)(v1));
  t65 = ((int64_t)(v3));
  t66 = ((int64_t)(v7));
  t67 = ((int64_t)(v5));
  gg_function_24(t63, t64, t65, t66, t67, base + 0);
  t68 = ((uint64_t)(v0));
  t69 = ((int64_t)(v1));
  t70 = ((int64_t)(v2));
  t71 = ((int64_t)(v3));
  t72 = ((int64_t)(v6));
  t73 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  gg_function_25(t68, t69, t70, t71, t74, base + 0);
  }
gg_done:;
}
static int64_t gg_function_8(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_35(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((uint64_t)(v0));
  t5 = ((int64_t)(v4));
  t6 = gg_function_8(t5, base + 0);
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t7 = fg291k_ld_i64(t4, t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(v4));
  gg_function_36(t0, t1, t2, t3, t8, t9, base + 0);
  }
gg_done:;
}
static int64_t gg_function_12(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t4 = ((int64_t)(((int64_t)gg_mod(t2, t3))));
  if (!(((t1) < (t4)))) goto label2;
  {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v1 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v1));
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(3))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v1));
  t5 = gg_function_11(base + 0);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t6)))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t7)))));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_9(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  uint8_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
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
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t8 = ((int64_t)(((int64_t)gg_div(t6, t7))));
  t9 = gg_function_10(t2, t3, t8, base + 0);
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t10 = fg291k_ld_i64(t1, t9);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(v4));
  t13 = gg_function_12(t12, base + 0);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((t11) & (t14))));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t0 = ((t15) != (t16));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_37(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
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
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v3));
  t3 = gg_function_11(base + 0);
  t4 = ((int64_t)(v4));
  t5 = gg_function_9(t0, t1, t2, t3, t4, base + 0);
  t6 = t5;
  t7 = (!(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(68))));
  gg_function_3(t7, t8, base + 0);
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  v5 = ((int64_t)(((int64_t)gg_div(t9, t10))));
  t11 = ((uint64_t)(v0));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(v5));
  t15 = gg_function_10(t12, t13, t14, base + 0);
  t16 = ((uint64_t)(v0));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(v5));
  t20 = gg_function_10(t17, t18, t19, base + 0);
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t21 = fg291k_ld_i64(t16, t20);
  t22 = ((int64_t)(t21));
  t23 = ((int64_t)(v4));
  t24 = gg_function_12(t23, base + 0);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(((t22) | (t25))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t11, t15, t26);
  t27 = ((uint64_t)(v0));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(v3));
  gg_function_35(t27, t28, t29, t30, t31, base + 0);
  }
gg_done:;
}
static uint8_t gg_function_7(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  uint8_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = gg_function_8(t2, base + 0);
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t4 = fg291k_ld_i64(t1, t3);
  v4 = ((int64_t)(t4));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(v2));
  t7 = ((t5) >= (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(v3));
  t8 = ((t9) < (t10));
  }
  t0 = t8;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_31(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(v3));
  t2 = ((t0) != (t1));
  if (t2) {
  {
  t3 = ((uint64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t7 = fg291k_ld_i64(t3, t6);
  v4 = ((int64_t)(t7));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(v4));
  t10 = ((t8) < (t9));
  if (t10) {
  {
  t11 = ((uint64_t)(v0));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((int64_t)(v2));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t11, t14, t15);
  }
  }
  }
  }
  }
gg_done:;
}
static int64_t gg_function_30(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(2))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t4)))));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_28(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(3))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v0));
  t5 = gg_function_11(base + 0);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t6)))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t7)))));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t11)))));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_27(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = gg_function_28(t1, t2, t3, base + 0);
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t0, t4, t5);
  t6 = ((uint64_t)(v0));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t10 = gg_function_28(t7, t8, t9, base + 0);
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t6, t10, t11);
  t12 = ((uint64_t)(v0));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t16 = gg_function_28(t13, t14, t15, base + 0);
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t12, t16, t17);
  }
gg_done:;
}
static void gg_function_33(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
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
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((int64_t)(v3));
  t1 = gg_function_11(base + 0);
  t2 = ((int64_t)(t1));
  if (!(((t0) < (t2)))) goto label2;
  {
  t3 = ((uint64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v3));
  t7 = gg_function_10(t4, t5, t6, base + 0);
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t3, t7, t8);
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_20(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(v3));
  v5 = gg_function_16(t0, t1, t2, t3, base + 0);
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t6 = ((t4) >= (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v4));
  t9 = ((int64_t)(v5));
  t7 = ((t8) < (t9));
  }
  t10 = t7;
  t11 = ((int64_t)(((int64_t)UINT64_C(50))));
  gg_function_3(t10, t11, base + 0);
  v6 = ((int64_t)(v4));
label1:;
  t12 = ((int64_t)(v6));
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) - (uint64_t)(t14)))));
  if (!(((t12) < (t15)))) goto label2;
  {
  t16 = ((uint64_t)(v0));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(v6));
  t20 = ((uint64_t)(v0));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))));
  t26 = gg_function_21(t20, t21, t22, t25, base + 0);
  gg_function_23(t16, t17, t18, t19, t26, base + 0);
  t27 = ((uint64_t)(v0));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v3));
  t30 = ((int64_t)(v6));
  t31 = ((uint64_t)(v0));
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(v6));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  t37 = gg_function_18(t31, t32, t33, t36, base + 0);
  gg_function_24(t27, t28, t29, t30, t37, base + 0);
  t38 = ((int64_t)(v6));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  }
  goto label1;
label2:;
  t40 = ((uint64_t)(v0));
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(v2));
  t43 = ((int64_t)(v3));
  t44 = ((int64_t)(v5));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) - (uint64_t)(t45)))));
  gg_function_25(t40, t41, t42, t43, t46, base + 0);
  }
gg_done:;
}
static int64_t gg_function_15(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v3));
  v5 = gg_function_16(t1, t2, t3, t4, base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t5 = ((int64_t)(v6));
  t6 = ((int64_t)(v5));
  if (!(((t5) < (t6)))) goto label2;
  {
  t7 = ((uint64_t)(v0));
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(v6));
  t11 = gg_function_18(t7, t8, t9, t10, base + 0);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(v4));
  t14 = ((t12) == (t13));
  if (t14) {
  {
  t0 = ((int64_t)(v6));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
  }
  goto label1;
label2:;
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t17) - (uint64_t)(t18)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_32(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint8_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(v2));
  if (!(((t0) < (t1)))) goto label2;
  {
  t2 = ((uint64_t)(v0));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v3));
  t5 = gg_function_11(base + 0);
  t6 = ((int64_t)(v4));
  t7 = gg_function_9(t2, t3, t4, t5, t6, base + 0);
  t8 = t7;
  if (t8) {
  {
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(v3));
  v5 = gg_function_15(t9, t10, t11, t12, t13, base + 0);
  t14 = ((int64_t)(v5));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t16 = ((t14) >= (t15));
  t17 = ((int64_t)(((int64_t)UINT64_C(52))));
  gg_function_3(t16, t17, base + 0);
  t18 = ((uint64_t)(v0));
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(v5));
  gg_function_20(t18, t19, t20, t21, t22, base + 0);
  }
  }
  t23 = ((int64_t)(v4));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static uint8_t gg_function_26(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  uint8_t t0;
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
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v4));
  t2 = ((int64_t)(v3));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((uint64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v4));
  t7 = gg_function_10(t4, t5, t6, base + 0);
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t8 = fg291k_ld_i64(t3, t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((t9) != (t10));
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_34(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  uint8_t t34;
  uint64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint64_t t48;
  int64_t t49;
  int64_t t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  int64_t t70;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v3));
  t4 = gg_function_11(base + 0);
  t5 = ((int64_t)(v4));
  t6 = gg_function_9(t1, t2, t3, t4, t5, base + 0);
  t7 = t6;
  if (t7) {
  {
  t8 = ((uint64_t)(v0));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(v3));
  gg_function_32(t8, t9, t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v3));
  gg_function_33(t12, t13, t14, base + 0);
  t15 = ((uint64_t)(v0));
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(v4));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t20 = ((int64_t)(((int64_t)gg_div(t18, t19))));
  t21 = gg_function_10(t16, t17, t20, base + 0);
  t22 = ((int64_t)(v4));
  t23 = gg_function_12(t22, base + 0);
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t15, t21, t23);
  t24 = ((uint64_t)(v0));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)(v4));
  t28 = ((int64_t)(v3));
  gg_function_35(t24, t25, t26, t27, t28, base + 0);
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t29 = ((uint64_t)(v0));
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(v3));
  t32 = gg_function_11(base + 0);
  t33 = gg_function_26(t29, t30, t31, t32, base + 0);
  t34 = t33;
  if (t34) {
  {
  t35 = ((uint64_t)(v0));
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(v2));
  t38 = ((int64_t)(v3));
  gg_function_32(t35, t36, t37, t38, base + 0);
  t39 = ((uint64_t)(v0));
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(v3));
  gg_function_33(t39, t40, t41, base + 0);
  t42 = ((uint64_t)(v0));
  t43 = ((int64_t)(v3));
  t44 = gg_function_8(t43, base + 0);
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) - (uint64_t)(t46)))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t42, t44, t47);
  t48 = ((uint64_t)(v0));
  t49 = ((int64_t)(v1));
  t50 = ((int64_t)(v3));
  gg_function_27(t48, t49, t50, base + 0);
  t51 = ((uint64_t)(v0));
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(v3));
  t54 = gg_function_29(t52, t53, base + 0);
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t51, t54, t55);
  t56 = ((uint64_t)(v0));
  t57 = ((int64_t)(v1));
  t58 = ((int64_t)(v3));
  t59 = gg_function_30(t57, t58, base + 0);
  t60 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t56, t59, t60);
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t61 = ((uint64_t)(v0));
  t62 = ((int64_t)(v3));
  t63 = gg_function_8(t62, base + 0);
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t64 = fg291k_ld_i64(t61, t63);
  t65 = ((int64_t)(t64));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t67 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) - (uint64_t)(t67)))));
  t69 = ((t65) == (t68));
  t70 = ((int64_t)(((int64_t)UINT64_C(54))));
  gg_function_3(t69, t70, base + 0);
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_13(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
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
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
  uint8_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint64_t t48;
  int64_t t49;
  int64_t t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint64_t v0;
  int64_t v1;
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
  {
  t1 = ((int64_t)(v4));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  v5 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t3 = ((uint64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(v5));
  t7 = gg_function_10(t4, t5, t6, base + 0);
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t8 = fg291k_ld_i64(t3, t7);
  v6 = ((int64_t)(t8));
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(v5));
  t13 = gg_function_10(t10, t11, t12, base + 0);
  t14 = ((int64_t)(v6));
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(v4));
  t17 = gg_function_12(t16, base + 0);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((t15) & (t18))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t14) - (uint64_t)(t19)))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t9, t13, t20);
  t21 = ((uint64_t)(v0));
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(v3));
  v7 = gg_function_15(t21, t22, t23, t24, t25, base + 0);
  t26 = ((int64_t)(v7));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t28 = ((t26) >= (t27));
  t29 = ((int64_t)(((int64_t)UINT64_C(53))));
  gg_function_3(t28, t29, base + 0);
  t30 = ((uint64_t)(v0));
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(v2));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(v7));
  gg_function_20(t30, t31, t32, t33, t34, base + 0);
  t35 = ((uint64_t)(v0));
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(v3));
  t38 = gg_function_11(base + 0);
  t39 = gg_function_26(t35, t36, t37, t38, base + 0);
  t40 = t39;
  t41 = (!(t40));
  if (t41) {
  {
  t42 = ((uint64_t)(v0));
  t43 = ((int64_t)(v3));
  t44 = gg_function_8(t43, base + 0);
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) - (uint64_t)(t46)))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t42, t44, t47);
  t48 = ((uint64_t)(v0));
  t49 = ((int64_t)(v1));
  t50 = ((int64_t)(v3));
  gg_function_27(t48, t49, t50, base + 0);
  t51 = ((uint64_t)(v0));
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(v3));
  t54 = gg_function_29(t52, t53, base + 0);
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t51, t54, t55);
  t56 = ((uint64_t)(v0));
  t57 = ((int64_t)(v1));
  t58 = ((int64_t)(v3));
  t59 = gg_function_30(t57, t58, base + 0);
  t60 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t56, t59, t60);
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
static int64_t gg_function_5(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2147483647))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_1(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  uint64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint64_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  uint8_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  int64_t t74;
  int64_t t75;
  uint8_t t76;
  int64_t t77;
  uint64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  uint64_t t83;
  int64_t t84;
  int64_t t85;
  uint64_t t86;
  int64_t t87;
  int64_t t88;
  uint64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  uint8_t t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t t100;
  uint8_t t101;
  uint64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  uint8_t t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  int64_t t114;
  int64_t t115;
  uint64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  uint8_t t122;
  int64_t t123;
  int64_t t124;
  uint8_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  uint64_t t129;
  int64_t t130;
  int64_t t131;
  uint64_t t132;
  int64_t t133;
  int64_t t134;
  uint64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  uint8_t t139;
  uint8_t t140;
  uint64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  uint64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  uint64_t t151;
  int64_t t152;
  int64_t t153;
  uint64_t t154;
  int64_t t155;
  int64_t t156;
  uint64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  uint64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint8_t t169;
  int64_t t170;
  int64_t t171;
  uint64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  uint8_t t183;
  int64_t t184;
  uint64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  uint64_t t190;
  int64_t t191;
  int64_t t192;
  uint64_t t193;
  int64_t t194;
  int64_t t195;
  uint64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  uint8_t t204;
  int64_t t205;
  int64_t t206;
  uint8_t t207;
  int64_t t208;
  int64_t t209;
  uint64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  uint8_t t216;
  int64_t t217;
  int64_t t218;
  uint8_t t219;
  uint8_t t220;
  int64_t t221;
  int64_t t222;
  uint8_t t223;
  int64_t t224;
  uint64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  uint64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  uint8_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  uint8_t t241;
  uint64_t t242;
  int64_t t243;
  int64_t t244;
  uint64_t t245;
  int64_t t246;
  int64_t t247;
  uint64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  uint8_t t252;
  uint8_t t253;
  uint64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  uint8_t t259;
  uint8_t t260;
  uint64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  uint8_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  uint64_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  uint64_t t277;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  int64_t v3;
  uint64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  uint64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  uint64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  uint64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  uint64_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  uint64_t v29;
  int64_t v30;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t4 = fg291k_get(t1, t2, t3);
  v1 = ((int64_t)(((int64_t)(t4))));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t8 = fg291k_get(t5, t6, t7);
  v2 = ((uint64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t12 = fg291k_get(t9, t10, t11);
  v3 = ((int64_t)(((int64_t)(t12))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t16 = fg291k_get(t13, t14, t15);
  v4 = ((uint64_t)(t16));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t20 = fg291k_get(t17, t18, t19);
  v5 = ((int64_t)(((int64_t)(t20))));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(5))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t24 = fg291k_get(t21, t22, t23);
  v6 = ((int64_t)(((int64_t)(t24))));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((int64_t)(((int64_t)UINT64_C(6))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t28 = fg291k_get(t25, t26, t27);
  v7 = ((int64_t)(((int64_t)(t28))));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(((int64_t)UINT64_C(7))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t32 = fg291k_get(t29, t30, t31);
  v8 = ((uint64_t)(t32));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t36 = fg291k_get(t33, t34, t35);
  v9 = ((int64_t)(((int64_t)(t36))));
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t40 = fg291k_get(t37, t38, t39);
  v10 = ((int64_t)(((int64_t)(t40))));
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(1))));
  t43 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t44 = fg291k_get(t41, t42, t43);
  v11 = ((int64_t)(((int64_t)(t44))));
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t47 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t48 = fg291k_get(t45, t46, t47);
  v12 = ((int64_t)(((int64_t)(t48))));
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  t51 = ((int64_t)(((int64_t)UINT64_C(3))));
#ifndef __cplusplus
  extern uint64_t (fg291k_get)(int64_t, int64_t, int64_t);
#endif
  t52 = fg291k_get(t49, t50, t51);
  v13 = ((int64_t)(((int64_t)(t52))));
  t53 = ((int64_t)(v5));
  t54 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t55 = ((t53) > (t54));
  t56 = t55;
  if (t56) {
  t57 = ((int64_t)(v6));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t56 = ((t57) > (t58));
  }
  t59 = t56;
  t60 = ((int64_t)(((int64_t)UINT64_C(60))));
  gg_function_3(t59, t60, base + 0);
  t61 = ((int64_t)(v3));
  t62 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t63 = ((t61) > (t62));
  t64 = ((int64_t)(((int64_t)UINT64_C(61))));
  gg_function_3(t63, t64, base + 0);
  t65 = ((int64_t)(v12));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t67 = ((t65) < (t66));
  if (t67) {
  {
  v12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t68 = ((int64_t)(v13));
  t69 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t70 = ((t68) < (t69));
  if (t70) {
  {
  v13 = gg_function_5(base + 0);
  }
  }
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((int64_t)(v1));
  t72 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t73 = ((t71) == (t72));
  if (t73) {
  {
  t74 = ((int64_t)(v10));
  t75 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t76 = ((t74) >= (t75));
  t77 = ((int64_t)(((int64_t)UINT64_C(62))));
  gg_function_3(t76, t77, base + 0);
  t78 = ((uint64_t)(v8));
  t79 = ((int64_t)(v10));
  t80 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t82 = fg291k_ld_i64(t78, t81);
  v15 = ((int64_t)(t82));
  t83 = ((uint64_t)(v2));
  t84 = ((int64_t)(v15));
  t85 = ((int64_t)(v3));
  t86 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))))));
  v16 = ((uint64_t)(((uint64_t)((uint64_t)(t83) + (uint64_t)(t86)))));
  v17 = ((int64_t)(v5));
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t87 = ((int64_t)(v18));
  t88 = ((int64_t)(v5));
  if (!(((t87) < (t88)))) goto label2;
  {
  t89 = ((uint64_t)(v16));
  t90 = ((int64_t)(v18));
  t91 = ((int64_t)(v12));
  t92 = ((int64_t)(v13));
  t93 = gg_function_7(t89, t90, t91, t92, base + 0);
  t94 = t93;
  if (t94) {
  {
  t95 = ((uint64_t)(v16));
  t96 = ((int64_t)(v5));
  t97 = ((int64_t)(v18));
  t98 = ((int64_t)(v6));
  t99 = ((int64_t)(v10));
  t100 = gg_function_9(t95, t96, t97, t98, t99, base + 0);
  t101 = t100;
  if (t101) {
  {
  t102 = ((uint64_t)(v16));
  t103 = ((int64_t)(v5));
  t104 = ((int64_t)(v7));
  t105 = ((int64_t)(v18));
  t106 = ((int64_t)(v10));
  t107 = gg_function_13(t102, t103, t104, t105, t106, base + 0);
  t108 = ((int64_t)(t107));
  t109 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t110 = ((t108) == (t109));
  if (t110) {
  {
  t111 = ((int64_t)(v17));
  t112 = ((int64_t)(v5));
  t113 = ((t111) == (t112));
  if (t113) {
  {
  v17 = ((int64_t)(v18));
  }
  }
  }
  }
  }
  }
  }
  }
  t114 = ((int64_t)(v18));
  t115 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t115)))));
  }
  goto label1;
label2:;
  t116 = ((uint64_t)(v4));
  t117 = ((int64_t)(v15));
  t118 = ((int64_t)(v17));
  t119 = ((int64_t)(v5));
  gg_function_31(t116, t117, t118, t119, base + 0);
  }
  } else {
  {
  t120 = ((int64_t)(v1));
  t121 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t122 = ((t120) == (t121));
  if (t122) {
  {
  t123 = ((int64_t)(v9));
  t124 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t125 = ((t123) > (t124));
  t126 = ((int64_t)(((int64_t)UINT64_C(63))));
  gg_function_3(t125, t126, base + 0);
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t127 = ((int64_t)(v19));
  t128 = ((int64_t)(v9));
  if (!(((t127) < (t128)))) goto label5;
  {
  t129 = ((uint64_t)(v2));
  t130 = ((int64_t)(v19));
  t131 = ((int64_t)(v3));
  t132 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t130) * (uint64_t)(t131)))))));
  v20 = ((uint64_t)(((uint64_t)((uint64_t)(t129) + (uint64_t)(t132)))));
  v21 = ((int64_t)(v5));
  v22 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t133 = ((int64_t)(v22));
  t134 = ((int64_t)(v5));
  if (!(((t133) < (t134)))) goto label8;
  {
  t135 = ((uint64_t)(v20));
  t136 = ((int64_t)(v22));
  t137 = ((int64_t)(v12));
  t138 = ((int64_t)(v13));
  t139 = gg_function_7(t135, t136, t137, t138, base + 0);
  t140 = t139;
  if (t140) {
  {
  t141 = ((uint64_t)(v20));
  t142 = ((int64_t)(v5));
  t143 = ((int64_t)(v7));
  t144 = ((int64_t)(v22));
  gg_function_32(t141, t142, t143, t144, base + 0);
  t145 = ((uint64_t)(v20));
  t146 = ((int64_t)(v22));
  t147 = gg_function_8(t146, base + 0);
  t148 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t149 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) - (uint64_t)(t149)))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t145, t147, t150);
  t151 = ((uint64_t)(v20));
  t152 = ((int64_t)(v5));
  t153 = ((int64_t)(v22));
  gg_function_33(t151, t152, t153, base + 0);
  t154 = ((uint64_t)(v20));
  t155 = ((int64_t)(v5));
  t156 = ((int64_t)(v22));
  gg_function_27(t154, t155, t156, base + 0);
  t157 = ((uint64_t)(v20));
  t158 = ((int64_t)(v5));
  t159 = ((int64_t)(v22));
  t160 = gg_function_29(t158, t159, base + 0);
  t161 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t157, t160, t161);
  t162 = ((uint64_t)(v20));
  t163 = ((int64_t)(v5));
  t164 = ((int64_t)(v22));
  t165 = gg_function_30(t163, t164, base + 0);
  t166 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg291k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291k_st_i64(t162, t165, t166);
  t167 = ((int64_t)(v21));
  t168 = ((int64_t)(v5));
  t169 = ((t167) == (t168));
  if (t169) {
  {
  v21 = ((int64_t)(v22));
  }
  }
  }
  }
  t170 = ((int64_t)(v22));
  t171 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t170) + (uint64_t)(t171)))));
  }
  goto label7;
label8:;
  t172 = ((uint64_t)(v4));
  t173 = ((int64_t)(v19));
  t174 = ((int64_t)(v21));
  t175 = ((int64_t)(v5));
  gg_function_31(t172, t173, t174, t175, base + 0);
  t176 = ((int64_t)(v19));
  t177 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t176) + (uint64_t)(t177)))));
  }
  goto label4;
label5:;
  }
  } else {
  {
  t178 = ((int64_t)(v1));
  t179 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t180 = ((t178) == (t179));
  if (t180) {
  {
  t181 = ((int64_t)(v10));
  t182 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t183 = ((t181) >= (t182));
  t184 = ((int64_t)(((int64_t)UINT64_C(64))));
  gg_function_3(t183, t184, base + 0);
  t185 = ((uint64_t)(v8));
  t186 = ((int64_t)(v10));
  t187 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t188 = ((int64_t)(((int64_t)((uint64_t)(t186) * (uint64_t)(t187)))));
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t189 = fg291k_ld_i64(t185, t188);
  v23 = ((int64_t)(t189));
  t190 = ((uint64_t)(v2));
  t191 = ((int64_t)(v23));
  t192 = ((int64_t)(v3));
  t193 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t191) * (uint64_t)(t192)))))));
  v24 = ((uint64_t)(((uint64_t)((uint64_t)(t190) + (uint64_t)(t193)))));
  v25 = ((int64_t)(v5));
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t194 = ((int64_t)(v26));
  t195 = ((int64_t)(v5));
  if (!(((t194) < (t195)))) goto label11;
  {
  t196 = ((uint64_t)(v24));
  t197 = ((int64_t)(v5));
  t198 = ((int64_t)(v7));
  t199 = ((int64_t)(v26));
  t200 = ((int64_t)(v10));
  t201 = gg_function_34(t196, t197, t198, t199, t200, base + 0);
  t202 = ((int64_t)(t201));
  t203 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t204 = ((t202) == (t203));
  if (t204) {
  {
  t205 = ((int64_t)(v25));
  t206 = ((int64_t)(v5));
  t207 = ((t205) == (t206));
  if (t207) {
  {
  v25 = ((int64_t)(v26));
  }
  }
  }
  }
  t208 = ((int64_t)(v26));
  t209 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t208) + (uint64_t)(t209)))));
  }
  goto label10;
label11:;
  t210 = ((uint64_t)(v4));
  t211 = ((int64_t)(v23));
  t212 = ((int64_t)(v25));
  t213 = ((int64_t)(v5));
  gg_function_31(t210, t211, t212, t213, base + 0);
  }
  } else {
  {
  t214 = ((int64_t)(v1));
  t215 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t216 = ((t214) == (t215));
  if (t216) {
  {
  t217 = ((int64_t)(v10));
  t218 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t219 = ((t217) >= (t218));
  t220 = t219;
  if (t220) {
  t221 = ((int64_t)(v11));
  t222 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t220 = ((t221) >= (t222));
  }
  t223 = t220;
  t224 = ((int64_t)(((int64_t)UINT64_C(65))));
  gg_function_3(t223, t224, base + 0);
  t225 = ((uint64_t)(v8));
  t226 = ((int64_t)(v10));
  t227 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t228 = ((int64_t)(((int64_t)((uint64_t)(t226) * (uint64_t)(t227)))));
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t229 = fg291k_ld_i64(t225, t228);
  v27 = ((int64_t)(t229));
  t230 = ((uint64_t)(v8));
  t231 = ((int64_t)(v11));
  t232 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t233 = ((int64_t)(((int64_t)((uint64_t)(t231) * (uint64_t)(t232)))));
#ifndef __cplusplus
  extern int64_t (fg291k_ld_i64)(uint64_t, int64_t);
#endif
  t234 = fg291k_ld_i64(t230, t233);
  v28 = ((int64_t)(t234));
  t235 = ((int64_t)(v27));
  t236 = ((int64_t)(v28));
  t237 = ((t235) == (t236));
  t238 = ((int64_t)(((int64_t)UINT64_C(66))));
  gg_function_3(t237, t238, base + 0);
  t239 = ((int64_t)(v10));
  t240 = ((int64_t)(v11));
  t241 = ((t239) != (t240));
  if (t241) {
  {
  t242 = ((uint64_t)(v2));
  t243 = ((int64_t)(v27));
  t244 = ((int64_t)(v3));
  t245 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t243) * (uint64_t)(t244)))))));
  v29 = ((uint64_t)(((uint64_t)((uint64_t)(t242) + (uint64_t)(t245)))));
  v30 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t246 = ((int64_t)(v30));
  t247 = ((int64_t)(v5));
  if (!(((t246) < (t247)))) goto label14;
  {
  t248 = ((uint64_t)(v29));
  t249 = ((int64_t)(v30));
  t250 = ((int64_t)(v12));
  t251 = ((int64_t)(v13));
  t252 = gg_function_7(t248, t249, t250, t251, base + 0);
  t253 = t252;
  if (t253) {
  {
  t254 = ((uint64_t)(v29));
  t255 = ((int64_t)(v5));
  t256 = ((int64_t)(v30));
  t257 = ((int64_t)(v6));
  t258 = ((int64_t)(v10));
  t259 = gg_function_9(t254, t255, t256, t257, t258, base + 0);
  t260 = t259;
  if (t260) {
  {
  t261 = ((uint64_t)(v29));
  t262 = ((int64_t)(v5));
  t263 = ((int64_t)(v7));
  t264 = ((int64_t)(v30));
  t265 = ((int64_t)(v11));
  gg_function_37(t261, t262, t263, t264, t265, base + 0);
  }
  }
  }
  }
  t266 = ((int64_t)(v30));
  t267 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v30 = ((int64_t)(((int64_t)((uint64_t)(t266) + (uint64_t)(t267)))));
  }
  goto label13;
label14:;
  }
  }
  }
  } else {
  {
  t268 = ((uint8_t)UINT64_C(0));
  t269 = ((int64_t)(((int64_t)UINT64_C(67))));
  gg_function_3(t268, t269, base + 0);
  }
  }
  }
  }
  }
  }
  }
  }
  t270 = ((int64_t)(v0));
  t271 = ((int64_t)(((int64_t)UINT64_C(3))));
  t272 = ((int64_t)(((int64_t)UINT64_C(0))));
  t273 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern void (fg291k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg291k_put(t270, t271, t272, t273);
  t274 = ((int64_t)(v0));
  t275 = ((int64_t)(((int64_t)UINT64_C(3))));
  t276 = ((int64_t)(((int64_t)UINT64_C(1))));
  t277 = ((uint64_t)(((uint64_t)(v14))));
#ifndef __cplusplus
  extern void (fg291k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg291k_put(t274, t275, t276, t277);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  int64_t t0;
  uint8_t t1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = gg_function_1(t0, base + 0);
  (void)(t1);
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
