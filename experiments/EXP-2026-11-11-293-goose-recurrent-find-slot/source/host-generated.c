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
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t base);
static void gg_function_10(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_12(int64_t p0, int64_t base);
static uint8_t gg_function_14(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_15(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static uint8_t gg_function_16(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_17(int64_t p0, int64_t base);
static int64_t gg_function_18(int64_t p0, int64_t p1, int64_t base);
static void gg_function_19(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static void gg_function_20(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_21(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(4))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t6)))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  goto gg_done;
  }
gg_done:;
  return t0;
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
static int64_t gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(4))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t6)))));
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_16(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t7 = ((int64_t)(((int64_t)gg_div(t5, t6))));
  t8 = gg_function_11(t2, t3, t4, t7, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t9 = fg293k_ld_i64(t1, t8);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(v4));
  t12 = gg_function_12(t11, base + 0);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(((t10) & (t13))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t0 = ((t14) != (t15));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_15(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
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
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v5));
  t2 = ((int64_t)(v3));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  if (!(((t1) < (t4)))) goto label2;
  {
  t5 = ((uint64_t)(v0));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(v5));
  t10 = gg_function_16(t5, t6, t7, t8, t9, base + 0);
  t11 = t10;
  if (t11) {
  {
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  }
  }
  t14 = ((int64_t)(v5));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v4));
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_14(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  uint8_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
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
  t5 = gg_function_15(t1, t2, t3, t4, base + 0);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t0 = ((t6) == (t7));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(4))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t6)))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t base) {
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
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(4))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t6)))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_18(int64_t p0, int64_t p1, int64_t base) {
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
static int64_t gg_function_21(int64_t p0, int64_t p1, int64_t base) {
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
  extern void (fg293k_fail)(int64_t);
#endif
  fg293k_fail(t2);
  }
  }
  }
gg_done:;
}
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t base) {
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
  t1 = ((int64_t)(((int64_t)UINT64_C(3))));
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
static void gg_function_19(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
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
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  v5 = ((int64_t)(((int64_t)gg_div(t0, t1))));
  t2 = ((uint64_t)(v0));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(v5));
  t7 = gg_function_11(t3, t4, t5, t6, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(v5));
  t13 = gg_function_11(t9, t10, t11, t12, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t14 = fg293k_ld_i64(t8, t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(v4));
  t17 = gg_function_12(t16, base + 0);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((t15) | (t18))));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t2, t7, t19);
  }
gg_done:;
}
static void gg_function_20(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
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
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(v3));
  if (!(((t0) < (t1)))) goto label2;
  {
  t2 = ((uint64_t)(v0));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(v4));
  t7 = gg_function_11(t3, t4, t5, t6, base + 0);
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t2, t7, t8);
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_17(int64_t p0, int64_t base) {
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
static void gg_function_10(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
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
  int64_t t21;
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
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  v5 = ((int64_t)(((int64_t)gg_div(t0, t1))));
  t2 = ((uint64_t)(v0));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(v5));
  t7 = gg_function_11(t3, t4, t5, t6, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t8 = fg293k_ld_i64(t2, t7);
  v6 = ((int64_t)(t8));
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(v5));
  t14 = gg_function_11(t10, t11, t12, t13, base + 0);
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(v4));
  t18 = gg_function_12(t17, base + 0);
  t19 = ((int64_t)(t18));
  t20 = ((int64_t)(((t16) & (t19))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t15) - (uint64_t)(t20)))));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t9, t14, t21);
  }
gg_done:;
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
  uint8_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  int64_t t56;
  uint64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  uint64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  uint64_t t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  uint64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  uint64_t t115;
  int64_t t116;
  int64_t t117;
  uint8_t t118;
  uint64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint8_t t126;
  uint64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  uint64_t t132;
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
  uint8_t t143;
  uint8_t t144;
  uint64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  uint64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint8_t t168;
  int64_t t169;
  int64_t t170;
  uint64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  uint8_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  uint64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  uint64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  uint8_t t201;
  uint64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  uint8_t t207;
  int64_t t208;
  uint64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  uint8_t t216;
  uint8_t t217;
  uint8_t t218;
  uint64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  uint8_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  uint8_t t227;
  uint64_t t228;
  int64_t t229;
  int64_t t230;
  uint64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  uint64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  uint64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  uint64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  uint64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  uint64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  uint8_t t260;
  uint8_t t261;
  uint8_t t262;
  int64_t t263;
  uint64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  uint8_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  uint8_t t280;
  int64_t t281;
  int64_t t282;
  uint64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  uint8_t t287;
  uint8_t t288;
  int64_t t289;
  int64_t t290;
  uint64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  uint8_t t298;
  int64_t t299;
  int64_t t300;
  uint8_t t301;
  int64_t t302;
  int64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  uint64_t t310;
  int64_t t311;
  uint64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  uint8_t t324;
  uint64_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  uint64_t t329;
  int64_t t330;
  int64_t t331;
  uint64_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  int64_t t336;
  uint64_t t337;
  int64_t t338;
  int64_t t339;
  int64_t t340;
  uint64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  uint64_t t346;
  int64_t t347;
  int64_t t348;
  int64_t t349;
  uint64_t t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  uint64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  uint64_t t363;
  int64_t t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  uint64_t t370;
  int64_t t371;
  int64_t t372;
  int64_t t373;
  int64_t t374;
  int64_t t375;
  uint64_t t376;
  int64_t t377;
  int64_t t378;
  int64_t t379;
  int64_t t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  uint64_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  int64_t t388;
  int64_t t389;
  int64_t t390;
  int64_t t391;
  int64_t t392;
  int64_t t393;
  int64_t t394;
  uint64_t t395;
  int64_t t396;
  int64_t t397;
  int64_t t398;
  int64_t t399;
  int64_t t400;
  int64_t t401;
  uint8_t t402;
  uint64_t t403;
  int64_t t404;
  int64_t t405;
  int64_t t406;
  int64_t t407;
  int64_t t408;
  int64_t t409;
  uint8_t t410;
  int64_t t411;
  int64_t t412;
  uint8_t t413;
  uint64_t t414;
  int64_t t415;
  int64_t t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  int64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  int64_t t425;
  int64_t t426;
  uint64_t t427;
  int64_t t428;
  int64_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  int64_t t434;
  int64_t t435;
  int64_t t436;
  int64_t t437;
  uint64_t t438;
  int64_t t439;
  int64_t t440;
  int64_t t441;
  uint64_t t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  uint64_t t446;
  int64_t t447;
  int64_t t448;
  int64_t t449;
  int64_t t450;
  int64_t t451;
  int64_t t452;
  uint64_t t453;
  int64_t t454;
  int64_t t455;
  int64_t t456;
  int64_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  int64_t t461;
  uint64_t t462;
  int64_t t463;
  int64_t t464;
  int64_t t465;
  int64_t t466;
  uint64_t t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
  int64_t t472;
  int64_t t473;
  int64_t t474;
  int64_t t475;
  int64_t t476;
  int64_t t477;
  int64_t t478;
  int64_t t479;
  int64_t t480;
  int64_t t481;
  uint64_t t482;
  int64_t t483;
  int64_t t484;
  int64_t t485;
  int64_t t486;
  int64_t t487;
  int64_t t488;
  uint8_t t489;
  int64_t t490;
  int64_t t491;
  int64_t t492;
  int64_t t493;
  int64_t t494;
  int64_t t495;
  uint8_t t496;
  int64_t t497;
  int64_t t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  int64_t t502;
  uint64_t t503;
  int64_t t504;
  int64_t t505;
  int64_t t506;
  int64_t t507;
  int64_t t508;
  int64_t t509;
  uint8_t t510;
  int64_t t511;
  int64_t t512;
  uint8_t t513;
  int64_t t514;
  uint64_t t515;
  int64_t t516;
  int64_t t517;
  int64_t t518;
  int64_t t519;
  uint64_t t520;
  int64_t t521;
  int64_t t522;
  int64_t t523;
  uint64_t t524;
  int64_t t525;
  int64_t t526;
  int64_t t527;
  int64_t t528;
  int64_t t529;
  uint64_t t530;
  int64_t t531;
  int64_t t532;
  int64_t t533;
  int64_t t534;
  int64_t t535;
  int64_t t536;
  uint64_t t537;
  int64_t t538;
  int64_t t539;
  int64_t t540;
  int64_t t541;
  uint64_t t542;
  int64_t t543;
  int64_t t544;
  int64_t t545;
  int64_t t546;
  int64_t t547;
  int64_t t548;
  int64_t t549;
  int64_t t550;
  int64_t t551;
  int64_t t552;
  uint64_t t553;
  int64_t t554;
  int64_t t555;
  int64_t t556;
  uint8_t t557;
  uint8_t t558;
  uint8_t t559;
  int64_t t560;
  int64_t t561;
  int64_t t562;
  int64_t t563;
  uint64_t t564;
  int64_t t565;
  int64_t t566;
  int64_t t567;
  int64_t t568;
  int64_t t569;
  int64_t t570;
  int64_t t571;
  int64_t t572;
  int64_t t573;
  int64_t t574;
  int64_t t575;
  uint64_t t576;
  int64_t t577;
  int64_t t578;
  uint8_t t579;
  int64_t t580;
  int64_t t581;
  int64_t t582;
  uint64_t t583;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  uint64_t v5;
  uint64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  uint8_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
  int64_t v42;
  int64_t v43;
  int64_t v44;
  int64_t v45;
  int64_t v46;
  int64_t v47;
  int64_t v48;
  int64_t v49;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg293k_get)(int64_t, int64_t, int64_t);
#endif
  t4 = fg293k_get(t1, t2, t3);
  v1 = ((uint64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg293k_get)(int64_t, int64_t, int64_t);
#endif
  t8 = fg293k_get(t5, t6, t7);
  v2 = ((int64_t)(((int64_t)(t8))));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg293k_get)(int64_t, int64_t, int64_t);
#endif
  t12 = fg293k_get(t9, t10, t11);
  v3 = ((int64_t)(((int64_t)(t12))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
#ifndef __cplusplus
  extern uint64_t (fg293k_get)(int64_t, int64_t, int64_t);
#endif
  t16 = fg293k_get(t13, t14, t15);
  v4 = ((uint64_t)(t16));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern uint64_t (fg293k_get)(int64_t, int64_t, int64_t);
#endif
  t20 = fg293k_get(t17, t18, t19);
  v5 = ((uint64_t)(t20));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(5))));
#ifndef __cplusplus
  extern uint64_t (fg293k_get)(int64_t, int64_t, int64_t);
#endif
  t24 = fg293k_get(t21, t22, t23);
  v6 = ((uint64_t)(t24));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((int64_t)(((int64_t)UINT64_C(6))));
#ifndef __cplusplus
  extern uint64_t (fg293k_get)(int64_t, int64_t, int64_t);
#endif
  t28 = fg293k_get(t25, t26, t27);
  v7 = ((int64_t)(((int64_t)(t28))));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(((int64_t)UINT64_C(7))));
#ifndef __cplusplus
  extern uint64_t (fg293k_get)(int64_t, int64_t, int64_t);
#endif
  t32 = fg293k_get(t29, t30, t31);
  v8 = ((int64_t)(((int64_t)(t32))));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern uint64_t (fg293k_get)(int64_t, int64_t, int64_t);
#endif
  t36 = fg293k_get(t33, t34, t35);
  v9 = ((int64_t)(((int64_t)(t36))));
  t37 = ((int64_t)(v2));
  t38 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t39 = ((t37) > (t38));
  t40 = t39;
  if (t40) {
  t41 = ((int64_t)(v3));
  t42 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t40 = ((t41) > (t42));
  }
  t43 = t40;
  t44 = t43;
  if (t44) {
  t45 = ((int64_t)(v7));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t44 = ((t45) > (t46));
  }
  t47 = t44;
  t48 = ((int64_t)(((int64_t)UINT64_C(60))));
  gg_function_3(t47, t48, base + 0);
  t49 = ((int64_t)(v8));
  t50 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t51 = ((t49) > (t50));
  t52 = t51;
  if (t52) {
  t53 = ((int64_t)(v9));
  t54 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t52 = ((t53) > (t54));
  }
  t55 = t52;
  t56 = ((int64_t)(((int64_t)UINT64_C(61))));
  gg_function_3(t55, t56, base + 0);
  t57 = ((uint64_t)(v1));
  t58 = ((int64_t)(v2));
  t59 = ((int64_t)(v3));
  t60 = gg_function_5(t58, t59, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t61 = fg293k_ld_i64(t57, t60);
  v10 = ((int64_t)(t61));
  t62 = ((uint64_t)(v1));
  t63 = ((int64_t)(v2));
  t64 = ((int64_t)(v3));
  t65 = gg_function_7(t63, t64, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t66 = fg293k_ld_i64(t62, t65);
  v11 = ((int64_t)(t66));
  t67 = ((int64_t)(v10));
  t68 = ((int64_t)(v11));
  t69 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t70 = ((int64_t)(v9));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t71)))));
  t73 = ((t67) > (t72));
  if (t73) {
  {
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t74 = ((int64_t)(v12));
  t75 = ((int64_t)(v9));
  if (!(((t74) < (t75)))) goto label2;
  {
  t76 = ((int64_t)(v12));
  t77 = ((int64_t)(v8));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t76) * (uint64_t)(t77)))));
  t78 = ((uint64_t)(v6));
  t79 = ((int64_t)(v13));
  t80 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t82 = fg293k_ld_i64(t78, t81);
  v14 = ((int64_t)(t82));
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t83 = ((int64_t)(v15));
  t84 = ((int64_t)(v14));
  if (!(((t83) < (t84)))) goto label5;
  {
  t85 = ((uint64_t)(v5));
  t86 = ((int64_t)(v13));
  t87 = ((int64_t)(v7));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t87)))));
  t89 = ((int64_t)(v15));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))));
  t91 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t93 = fg293k_ld_i64(t85, t92);
  v16 = ((int64_t)(t93));
  t94 = ((int64_t)(v16));
  t95 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t96 = ((t94) < (t95));
  if (t96) {
  {
  t97 = ((int64_t)(v0));
  t98 = ((int64_t)(((int64_t)UINT64_C(3))));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern void (fg293k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg293k_put(t97, t98, t99, t100);
  t101 = ((int64_t)(v0));
  t102 = ((int64_t)(((int64_t)UINT64_C(3))));
  t103 = ((int64_t)(((int64_t)UINT64_C(1))));
  t104 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg293k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg293k_put(t101, t102, t103, t104);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t105 = ((int64_t)(v16));
  t106 = ((int64_t)(v2));
  t107 = ((t105) >= (t106));
  if (t107) {
  {
  t108 = ((int64_t)(v0));
  t109 = ((int64_t)(((int64_t)UINT64_C(3))));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern void (fg293k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg293k_put(t108, t109, t110, t111);
  t112 = ((int64_t)(v0));
  t113 = ((int64_t)(((int64_t)UINT64_C(3))));
  t114 = ((int64_t)(((int64_t)UINT64_C(1))));
  t115 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg293k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg293k_put(t112, t113, t114, t115);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t116 = ((int64_t)(v15));
  t117 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t118 = ((t116) > (t117));
  if (t118) {
  {
  t119 = ((uint64_t)(v1));
  t120 = ((int64_t)(v2));
  t121 = ((int64_t)(v16));
  t122 = gg_function_9(t120, t121, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t123 = fg293k_ld_i64(t119, t122);
  v17 = ((int64_t)(t123));
  t124 = ((int64_t)(v17));
  t125 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t126 = ((t124) >= (t125));
  if (t126) {
  {
  t127 = ((uint64_t)(v1));
  t128 = ((int64_t)(v2));
  t129 = ((int64_t)(v17));
  t130 = ((int64_t)(v3));
  t131 = ((int64_t)(v16));
  gg_function_10(t127, t128, t129, t130, t131, base + 0);
  t132 = ((uint64_t)(v1));
  t133 = ((int64_t)(v2));
  t134 = ((int64_t)(v16));
  t135 = gg_function_9(t133, t134, base + 0);
  t136 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t137 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t136) - (uint64_t)(t137)))));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t132, t135, t138);
  t139 = ((uint64_t)(v1));
  t140 = ((int64_t)(v2));
  t141 = ((int64_t)(v17));
  t142 = ((int64_t)(v3));
  t143 = gg_function_14(t139, t140, t141, t142, base + 0);
  t144 = t143;
  if (t144) {
  {
  t145 = ((uint64_t)(v1));
  t146 = ((int64_t)(v17));
  t147 = gg_function_17(t146, base + 0);
  t148 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t149 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) - (uint64_t)(t149)))));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t145, t147, t150);
  t151 = ((uint64_t)(v1));
  t152 = ((int64_t)(v2));
  t153 = ((int64_t)(v17));
  t154 = gg_function_18(t152, t153, base + 0);
  t155 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t156 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) - (uint64_t)(t156)))));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t151, t154, t157);
  t158 = ((int64_t)(v11));
  t159 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t158) - (uint64_t)(t159)))));
  }
  }
  }
  }
  }
  }
  t160 = ((int64_t)(v15));
  t161 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t160) + (uint64_t)(t161)))));
  }
  goto label4;
label5:;
  t162 = ((int64_t)(v12));
  t163 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t162) + (uint64_t)(t163)))));
  }
  goto label1;
label2:;
  v18 = ((int64_t)(v10));
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t164 = ((int64_t)(v19));
  t165 = ((int64_t)(v2));
  if (!(((t164) < (t165)))) goto label8;
  {
  t166 = ((int64_t)(v18));
  t167 = ((int64_t)(v2));
  t168 = ((t166) >= (t167));
  if (t168) {
  {
  t169 = ((int64_t)(v18));
  t170 = ((int64_t)(v2));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t169) - (uint64_t)(t170)))));
  }
  }
  t171 = ((uint64_t)(v1));
  t172 = ((int64_t)(v2));
  t173 = ((int64_t)(v18));
  t174 = ((int64_t)(v3));
  t175 = gg_function_14(t171, t172, t173, t174, base + 0);
  t176 = t175;
  if (t176) {
  {
  goto label8;
  }
  }
  t177 = ((int64_t)(v18));
  t178 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t177) + (uint64_t)(t178)))));
  t179 = ((int64_t)(v19));
  t180 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t179) + (uint64_t)(t180)))));
  }
  goto label7;
label8:;
  t181 = ((int64_t)(v2));
  t182 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t181) - (uint64_t)(t182)))));
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t183 = ((int64_t)(v12));
  t184 = ((int64_t)(v9));
  if (!(((t183) < (t184)))) goto label11;
  {
  t185 = ((int64_t)(v12));
  t186 = ((int64_t)(v8));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t185) * (uint64_t)(t186)))));
  t187 = ((uint64_t)(v5));
  t188 = ((int64_t)(v22));
  t189 = ((int64_t)(v7));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t188) * (uint64_t)(t189)))));
  t191 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t192 = ((int64_t)(((int64_t)((uint64_t)(t190) * (uint64_t)(t191)))));
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t193 = fg293k_ld_i64(t187, t192);
  v23 = ((int64_t)(t193));
  t194 = ((uint64_t)(v1));
  t195 = ((int64_t)(v2));
  t196 = ((int64_t)(v23));
  t197 = gg_function_9(t195, t196, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t198 = fg293k_ld_i64(t194, t197);
  v24 = ((int64_t)(t198));
  v25 = ((uint8_t)UINT64_C(0));
  t199 = ((int64_t)(v24));
  t200 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t201 = ((t199) >= (t200));
  if (t201) {
  {
  t202 = ((uint64_t)(v1));
  t203 = ((int64_t)(v2));
  t204 = ((int64_t)(v24));
  t205 = ((int64_t)(v3));
  t206 = ((int64_t)(v23));
  t207 = gg_function_16(t202, t203, t204, t205, t206, base + 0);
  t208 = ((int64_t)(((int64_t)UINT64_C(63))));
  gg_function_3(t207, t208, base + 0);
  t209 = ((uint64_t)(v1));
  t210 = ((int64_t)(v2));
  t211 = ((int64_t)(v24));
  t212 = ((int64_t)(v3));
  t213 = gg_function_15(t209, t210, t211, t212, base + 0);
  t214 = ((int64_t)(t213));
  t215 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t216 = ((t214) == (t215));
  if (t216) {
  {
  v25 = ((uint8_t)UINT64_C(1));
  }
  }
  }
  }
  t217 = v25;
  t218 = (!(t217));
  if (t218) {
  {
  t219 = ((uint64_t)(v1));
  t220 = ((int64_t)(v2));
  t221 = ((int64_t)(v18));
  t222 = ((int64_t)(v3));
  t223 = gg_function_14(t219, t220, t221, t222, base + 0);
  t224 = ((int64_t)(((int64_t)UINT64_C(64))));
  gg_function_3(t223, t224, base + 0);
  t225 = ((int64_t)(v24));
  t226 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t227 = ((t225) >= (t226));
  if (t227) {
  {
  t228 = ((uint64_t)(v1));
  t229 = ((int64_t)(v18));
  t230 = gg_function_17(t229, base + 0);
  t231 = ((uint64_t)(v1));
  t232 = ((int64_t)(v24));
  t233 = gg_function_17(t232, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t234 = fg293k_ld_i64(t231, t233);
  t235 = ((int64_t)(t234));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t228, t230, t235);
  t236 = ((uint64_t)(v1));
  t237 = ((int64_t)(v2));
  t238 = ((int64_t)(v18));
  t239 = gg_function_18(t237, t238, base + 0);
  t240 = ((uint64_t)(v1));
  t241 = ((int64_t)(v2));
  t242 = ((int64_t)(v24));
  t243 = gg_function_18(t241, t242, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t244 = fg293k_ld_i64(t240, t243);
  t245 = ((int64_t)(t244));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t236, t239, t245);
  t246 = ((uint64_t)(v1));
  t247 = ((int64_t)(v2));
  t248 = ((int64_t)(v24));
  t249 = ((int64_t)(v3));
  t250 = ((int64_t)(v23));
  gg_function_10(t246, t247, t248, t249, t250, base + 0);
  t251 = ((uint64_t)(v1));
  t252 = ((int64_t)(v2));
  t253 = ((int64_t)(v18));
  t254 = ((int64_t)(v3));
  t255 = ((int64_t)(v23));
  gg_function_19(t251, t252, t253, t254, t255, base + 0);
  t256 = ((uint64_t)(v1));
  t257 = ((int64_t)(v2));
  t258 = ((int64_t)(v24));
  t259 = ((int64_t)(v3));
  t260 = gg_function_14(t256, t257, t258, t259, base + 0);
  t261 = t260;
  t262 = (!(t261));
  t263 = ((int64_t)(((int64_t)UINT64_C(65))));
  gg_function_3(t262, t263, base + 0);
  }
  }
  t264 = ((uint64_t)(v1));
  t265 = ((int64_t)(v2));
  t266 = ((int64_t)(v23));
  t267 = gg_function_9(t265, t266, base + 0);
  t268 = ((int64_t)(v18));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t264, t267, t268);
  t269 = ((int64_t)(v12));
  t270 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t271 = ((int64_t)(((int64_t)((uint64_t)(t269) + (uint64_t)(t270)))));
  t272 = ((int64_t)(v9));
  t273 = ((t271) < (t272));
  if (t273) {
  {
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t274 = ((int64_t)(v26));
  t275 = ((int64_t)(v2));
  if (!(((t274) < (t275)))) goto label14;
  {
  t276 = ((int64_t)(v18));
  t277 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t276) + (uint64_t)(t277)))));
  t278 = ((int64_t)(v18));
  t279 = ((int64_t)(v2));
  t280 = ((t278) >= (t279));
  if (t280) {
  {
  t281 = ((int64_t)(v18));
  t282 = ((int64_t)(v2));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t281) - (uint64_t)(t282)))));
  }
  }
  t283 = ((uint64_t)(v1));
  t284 = ((int64_t)(v2));
  t285 = ((int64_t)(v18));
  t286 = ((int64_t)(v3));
  t287 = gg_function_14(t283, t284, t285, t286, base + 0);
  t288 = t287;
  if (t288) {
  {
  goto label14;
  }
  }
  t289 = ((int64_t)(v26));
  t290 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t289) + (uint64_t)(t290)))));
  }
  goto label13;
label14:;
  }
  }
  }
  }
  t291 = ((uint64_t)(v1));
  t292 = ((int64_t)(v2));
  t293 = ((int64_t)(v23));
  t294 = gg_function_9(t292, t293, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t295 = fg293k_ld_i64(t291, t294);
  v27 = ((int64_t)(t295));
  t296 = ((int64_t)(v20));
  t297 = ((int64_t)(v27));
  t298 = ((t296) > (t297));
  if (t298) {
  {
  v20 = ((int64_t)(v27));
  }
  }
  t299 = ((int64_t)(v21));
  t300 = ((int64_t)(v27));
  t301 = ((t299) < (t300));
  if (t301) {
  {
  v21 = ((int64_t)(v27));
  }
  }
  t302 = ((int64_t)(v12));
  t303 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t302) + (uint64_t)(t303)))));
  }
  goto label10;
label11:;
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t304 = ((int64_t)(v12));
  t305 = ((int64_t)(v9));
  if (!(((t304) < (t305)))) goto label17;
  {
  t306 = ((int64_t)(v12));
  t307 = ((int64_t)(v8));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t306) * (uint64_t)(t307)))));
  t308 = ((int64_t)(v12));
  t309 = ((int64_t)(v20));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t308) + (uint64_t)(t309)))));
  t310 = ((uint64_t)(v1));
  t311 = ((int64_t)(v2));
  t312 = ((uint64_t)(v5));
  t313 = ((int64_t)(v28));
  t314 = ((int64_t)(v7));
  t315 = ((int64_t)(((int64_t)((uint64_t)(t313) * (uint64_t)(t314)))));
  t316 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t317 = ((int64_t)(((int64_t)((uint64_t)(t315) * (uint64_t)(t316)))));
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t318 = fg293k_ld_i64(t312, t317);
  t319 = ((int64_t)(t318));
  t320 = gg_function_9(t311, t319, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t321 = fg293k_ld_i64(t310, t320);
  v30 = ((int64_t)(t321));
  t322 = ((int64_t)(v29));
  t323 = ((int64_t)(v30));
  t324 = ((t322) != (t323));
  if (t324) {
  {
  t325 = ((uint64_t)(v1));
  t326 = ((int64_t)(v29));
  t327 = gg_function_17(t326, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t328 = fg293k_ld_i64(t325, t327);
  v31 = ((int64_t)(t328));
  t329 = ((uint64_t)(v1));
  t330 = ((int64_t)(v29));
  t331 = gg_function_17(t330, base + 0);
  t332 = ((uint64_t)(v1));
  t333 = ((int64_t)(v30));
  t334 = gg_function_17(t333, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t335 = fg293k_ld_i64(t332, t334);
  t336 = ((int64_t)(t335));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t329, t331, t336);
  t337 = ((uint64_t)(v1));
  t338 = ((int64_t)(v30));
  t339 = gg_function_17(t338, base + 0);
  t340 = ((int64_t)(v31));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t337, t339, t340);
  t341 = ((uint64_t)(v1));
  t342 = ((int64_t)(v2));
  t343 = ((int64_t)(v29));
  t344 = gg_function_18(t342, t343, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t345 = fg293k_ld_i64(t341, t344);
  v32 = ((int64_t)(t345));
  t346 = ((uint64_t)(v1));
  t347 = ((int64_t)(v2));
  t348 = ((int64_t)(v29));
  t349 = gg_function_18(t347, t348, base + 0);
  t350 = ((uint64_t)(v1));
  t351 = ((int64_t)(v2));
  t352 = ((int64_t)(v30));
  t353 = gg_function_18(t351, t352, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t354 = fg293k_ld_i64(t350, t353);
  t355 = ((int64_t)(t354));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t346, t349, t355);
  t356 = ((uint64_t)(v1));
  t357 = ((int64_t)(v2));
  t358 = ((int64_t)(v30));
  t359 = gg_function_18(t357, t358, base + 0);
  t360 = ((int64_t)(v32));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t356, t359, t360);
  v33 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t361 = ((int64_t)(v33));
  t362 = ((int64_t)(v3));
  if (!(((t361) < (t362)))) goto label20;
  {
  t363 = ((uint64_t)(v1));
  t364 = ((int64_t)(v2));
  t365 = ((int64_t)(v29));
  t366 = ((int64_t)(v3));
  t367 = ((int64_t)(v33));
  t368 = gg_function_11(t364, t365, t366, t367, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t369 = fg293k_ld_i64(t363, t368);
  v34 = ((int64_t)(t369));
  t370 = ((uint64_t)(v1));
  t371 = ((int64_t)(v2));
  t372 = ((int64_t)(v29));
  t373 = ((int64_t)(v3));
  t374 = ((int64_t)(v33));
  t375 = gg_function_11(t371, t372, t373, t374, base + 0);
  t376 = ((uint64_t)(v1));
  t377 = ((int64_t)(v2));
  t378 = ((int64_t)(v30));
  t379 = ((int64_t)(v3));
  t380 = ((int64_t)(v33));
  t381 = gg_function_11(t377, t378, t379, t380, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t382 = fg293k_ld_i64(t376, t381);
  t383 = ((int64_t)(t382));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t370, t375, t383);
  t384 = ((uint64_t)(v1));
  t385 = ((int64_t)(v2));
  t386 = ((int64_t)(v30));
  t387 = ((int64_t)(v3));
  t388 = ((int64_t)(v33));
  t389 = gg_function_11(t385, t386, t387, t388, base + 0);
  t390 = ((int64_t)(v34));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t384, t389, t390);
  t391 = ((int64_t)(v33));
  t392 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t391) + (uint64_t)(t392)))));
  }
  goto label19;
label20:;
  v35 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t393 = ((int64_t)(v35));
  t394 = ((int64_t)(v2));
  if (!(((t393) < (t394)))) goto label23;
  {
  t395 = ((uint64_t)(v1));
  t396 = ((int64_t)(v2));
  t397 = ((int64_t)(v35));
  t398 = gg_function_9(t396, t397, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t399 = fg293k_ld_i64(t395, t398);
  v36 = ((int64_t)(t399));
  t400 = ((int64_t)(v36));
  t401 = ((int64_t)(v30));
  t402 = ((t400) == (t401));
  if (t402) {
  {
  t403 = ((uint64_t)(v1));
  t404 = ((int64_t)(v2));
  t405 = ((int64_t)(v35));
  t406 = gg_function_9(t404, t405, base + 0);
  t407 = ((int64_t)(v29));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t403, t406, t407);
  }
  }
  t408 = ((int64_t)(v36));
  t409 = ((int64_t)(v29));
  t410 = ((t408) == (t409));
  if (t410) {
  {
  t411 = ((int64_t)(v36));
  t412 = ((int64_t)(v30));
  t413 = ((t411) != (t412));
  if (t413) {
  {
  t414 = ((uint64_t)(v1));
  t415 = ((int64_t)(v2));
  t416 = ((int64_t)(v35));
  t417 = gg_function_9(t415, t416, base + 0);
  t418 = ((int64_t)(v30));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t414, t417, t418);
  }
  }
  }
  }
  t419 = ((int64_t)(v35));
  t420 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t419) + (uint64_t)(t420)))));
  }
  goto label22;
label23:;
  }
  }
  t421 = ((int64_t)(v12));
  t422 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t421) + (uint64_t)(t422)))));
  }
  goto label16;
label17:;
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label25:;
  t423 = ((int64_t)(v12));
  t424 = ((int64_t)(v9));
  if (!(((t423) < (t424)))) goto label26;
  {
  t425 = ((int64_t)(v12));
  t426 = ((int64_t)(v8));
  v37 = ((int64_t)(((int64_t)((uint64_t)(t425) * (uint64_t)(t426)))));
  t427 = ((uint64_t)(v4));
  t428 = ((int64_t)(v37));
  t429 = ((int64_t)(v8));
  t430 = ((int64_t)(((int64_t)((uint64_t)(t428) + (uint64_t)(t429)))));
  t431 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t432 = ((int64_t)(((int64_t)((uint64_t)(t430) - (uint64_t)(t431)))));
  t433 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t434 = ((int64_t)(((int64_t)((uint64_t)(t432) * (uint64_t)(t433)))));
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t435 = fg293k_ld_i64(t427, t434);
  v38 = ((int64_t)(t435));
  t436 = ((int64_t)(v12));
  t437 = ((int64_t)(v20));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t436) + (uint64_t)(t437)))));
  t438 = ((uint64_t)(v1));
  t439 = ((int64_t)(v39));
  t440 = gg_function_17(t439, base + 0);
  t441 = ((int64_t)(v38));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t438, t440, t441);
  t442 = ((uint64_t)(v1));
  t443 = ((int64_t)(v2));
  t444 = ((int64_t)(v39));
  t445 = ((int64_t)(v3));
  gg_function_20(t442, t443, t444, t445, base + 0);
  t446 = ((uint64_t)(v6));
  t447 = ((int64_t)(v37));
  t448 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t449 = ((int64_t)(((int64_t)((uint64_t)(t447) * (uint64_t)(t448)))));
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t450 = fg293k_ld_i64(t446, t449);
  v40 = ((int64_t)(t450));
  v41 = ((int64_t)(((int64_t)UINT64_C(0))));
label28:;
  t451 = ((int64_t)(v41));
  t452 = ((int64_t)(v40));
  if (!(((t451) < (t452)))) goto label29;
  {
  t453 = ((uint64_t)(v5));
  t454 = ((int64_t)(v37));
  t455 = ((int64_t)(v7));
  t456 = ((int64_t)(((int64_t)((uint64_t)(t454) * (uint64_t)(t455)))));
  t457 = ((int64_t)(v41));
  t458 = ((int64_t)(((int64_t)((uint64_t)(t456) + (uint64_t)(t457)))));
  t459 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t460 = ((int64_t)(((int64_t)((uint64_t)(t458) * (uint64_t)(t459)))));
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t461 = fg293k_ld_i64(t453, t460);
  v42 = ((int64_t)(t461));
  t462 = ((uint64_t)(v1));
  t463 = ((int64_t)(v2));
  t464 = ((int64_t)(v39));
  t465 = ((int64_t)(v3));
  t466 = ((int64_t)(v42));
  gg_function_19(t462, t463, t464, t465, t466, base + 0);
  t467 = ((uint64_t)(v1));
  t468 = ((int64_t)(v2));
  t469 = ((int64_t)(v42));
  t470 = gg_function_9(t468, t469, base + 0);
  t471 = ((int64_t)(v39));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t467, t470, t471);
  t472 = ((int64_t)(v41));
  t473 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v41 = ((int64_t)(((int64_t)((uint64_t)(t472) + (uint64_t)(t473)))));
  }
  goto label28;
label29:;
  t474 = ((int64_t)(v12));
  t475 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t474) + (uint64_t)(t475)))));
  }
  goto label25;
label26:;
  t476 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t477 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v43 = ((int64_t)(((int64_t)((uint64_t)(t476) - (uint64_t)(t477)))));
  v44 = ((int64_t)(v20));
label31:;
  t478 = ((int64_t)(v44));
  t479 = ((int64_t)(v21));
  if (!(((t478) <= (t479)))) goto label32;
  {
  v45 = ((int64_t)(((int64_t)UINT64_C(0))));
  v46 = ((int64_t)(((int64_t)UINT64_C(0))));
label34:;
  t480 = ((int64_t)(v46));
  t481 = ((int64_t)(v2));
  if (!(((t480) < (t481)))) goto label35;
  {
  t482 = ((uint64_t)(v1));
  t483 = ((int64_t)(v2));
  t484 = ((int64_t)(v46));
  t485 = gg_function_18(t483, t484, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t486 = fg293k_ld_i64(t482, t485);
  t487 = ((int64_t)(t486));
  t488 = ((int64_t)(v44));
  t489 = ((t487) == (t488));
  if (t489) {
  {
  t490 = ((int64_t)(v45));
  t491 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v45 = ((int64_t)(((int64_t)((uint64_t)(t490) + (uint64_t)(t491)))));
  }
  }
  t492 = ((int64_t)(v46));
  t493 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v46 = ((int64_t)(((int64_t)((uint64_t)(t492) + (uint64_t)(t493)))));
  }
  goto label34;
label35:;
  t494 = ((int64_t)(v45));
  t495 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t496 = ((t494) == (t495));
  if (t496) {
  {
  v43 = ((int64_t)(v44));
  t497 = ((int64_t)(v21));
  t498 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v44 = ((int64_t)(((int64_t)((uint64_t)(t497) + (uint64_t)(t498)))));
  }
  } else {
  {
  t499 = ((int64_t)(v44));
  t500 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v44 = ((int64_t)(((int64_t)((uint64_t)(t499) + (uint64_t)(t500)))));
  }
  }
  }
  goto label31;
label32:;
  v44 = ((int64_t)(v20));
label37:;
  t501 = ((int64_t)(v44));
  t502 = ((int64_t)(v21));
  if (!(((t501) <= (t502)))) goto label38;
  {
  t503 = ((uint64_t)(v1));
  t504 = ((int64_t)(v2));
  t505 = ((int64_t)(v44));
  t506 = gg_function_18(t504, t505, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t507 = fg293k_ld_i64(t503, t506);
  t508 = ((int64_t)(t507));
  t509 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t510 = ((t508) < (t509));
  if (t510) {
  {
  t511 = ((int64_t)(v43));
  t512 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t513 = ((t511) >= (t512));
  t514 = ((int64_t)(((int64_t)UINT64_C(66))));
  gg_function_3(t513, t514, base + 0);
  t515 = ((uint64_t)(v1));
  t516 = ((int64_t)(v2));
  t517 = ((int64_t)(v44));
  t518 = gg_function_21(t516, t517, base + 0);
  t519 = ((int64_t)(v43));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t515, t518, t519);
  }
  } else {
  {
  t520 = ((uint64_t)(v1));
  t521 = ((int64_t)(v2));
  t522 = ((int64_t)(v44));
  t523 = gg_function_21(t521, t522, base + 0);
  t524 = ((uint64_t)(v1));
  t525 = ((int64_t)(v2));
  t526 = ((int64_t)(v44));
  t527 = gg_function_18(t525, t526, base + 0);
#ifndef __cplusplus
  extern int64_t (fg293k_ld_i64)(uint64_t, int64_t);
#endif
  t528 = fg293k_ld_i64(t524, t527);
  t529 = ((int64_t)(t528));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t520, t523, t529);
  }
  }
  t530 = ((uint64_t)(v1));
  t531 = ((int64_t)(v2));
  t532 = ((int64_t)(v44));
  t533 = gg_function_18(t531, t532, base + 0);
  t534 = ((int64_t)(v44));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t530, t533, t534);
  t535 = ((int64_t)(v44));
  t536 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v44 = ((int64_t)(((int64_t)((uint64_t)(t535) + (uint64_t)(t536)))));
  }
  goto label37;
label38:;
  t537 = ((uint64_t)(v1));
  t538 = ((int64_t)(v2));
  t539 = ((int64_t)(v3));
  t540 = gg_function_5(t538, t539, base + 0);
  t541 = ((int64_t)(v20));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t537, t540, t541);
  t542 = ((uint64_t)(v1));
  t543 = ((int64_t)(v2));
  t544 = ((int64_t)(v3));
  t545 = gg_function_22(t543, t544, base + 0);
  t546 = ((int64_t)(v21));
  t547 = ((int64_t)(v20));
  t548 = ((int64_t)(((int64_t)((uint64_t)(t546) - (uint64_t)(t547)))));
  t549 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t550 = ((int64_t)(((int64_t)((uint64_t)(t548) + (uint64_t)(t549)))));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t542, t545, t550);
  v47 = ((int64_t)(((int64_t)UINT64_C(0))));
  v44 = ((int64_t)(((int64_t)UINT64_C(0))));
label40:;
  t551 = ((int64_t)(v44));
  t552 = ((int64_t)(v2));
  if (!(((t551) < (t552)))) goto label41;
  {
  t553 = ((uint64_t)(v1));
  t554 = ((int64_t)(v2));
  t555 = ((int64_t)(v44));
  t556 = ((int64_t)(v3));
  t557 = gg_function_14(t553, t554, t555, t556, base + 0);
  t558 = t557;
  t559 = (!(t558));
  if (t559) {
  {
  t560 = ((int64_t)(v47));
  t561 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v47 = ((int64_t)(((int64_t)((uint64_t)(t560) + (uint64_t)(t561)))));
  }
  }
  t562 = ((int64_t)(v44));
  t563 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v44 = ((int64_t)(((int64_t)((uint64_t)(t562) + (uint64_t)(t563)))));
  }
  goto label40;
label41:;
  t564 = ((uint64_t)(v1));
  t565 = ((int64_t)(v2));
  t566 = ((int64_t)(v3));
  t567 = gg_function_7(t565, t566, base + 0);
  t568 = ((int64_t)(v47));
#ifndef __cplusplus
  extern void (fg293k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg293k_st_i64(t564, t567, t568);
  t569 = ((int64_t)(v21));
  t570 = ((int64_t)(v20));
  t571 = ((int64_t)(((int64_t)((uint64_t)(t569) - (uint64_t)(t570)))));
  t572 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v48 = ((int64_t)(((int64_t)((uint64_t)(t571) + (uint64_t)(t572)))));
  t573 = ((int64_t)(v0));
  t574 = ((int64_t)(((int64_t)UINT64_C(3))));
  t575 = ((int64_t)(((int64_t)UINT64_C(0))));
  t576 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern void (fg293k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg293k_put(t573, t574, t575, t576);
  v49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t577 = ((int64_t)(v48));
  t578 = ((int64_t)(v9));
  t579 = ((t577) >= (t578));
  if (t579) {
  {
  v49 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t580 = ((int64_t)(v0));
  t581 = ((int64_t)(((int64_t)UINT64_C(3))));
  t582 = ((int64_t)(((int64_t)UINT64_C(1))));
  t583 = ((uint64_t)(((uint64_t)(v49))));
#ifndef __cplusplus
  extern void (fg293k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg293k_put(t580, t581, t582, t583);
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
