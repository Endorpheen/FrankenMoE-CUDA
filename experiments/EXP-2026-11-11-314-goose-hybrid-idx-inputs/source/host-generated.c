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
static void gg_function_5(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_7(uint64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_8(uint64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_10(uint64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_11(uint64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_12(uint64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_13(uint64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_14(int64_t base);
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
  extern void (fg314k_fail)(int64_t);
#endif
  fg314k_fail(t2);
  }
  }
  }
gg_done:;
}
static void gg_function_5(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v2));
#ifndef __cplusplus
  extern void (fg314k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg314k_st_i64(t0, t3, t4);
  }
gg_done:;
}
static int64_t gg_function_14(int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  {
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16777216))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t1) - (uint64_t)(t2)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_8(uint64_t p0, int64_t p1, int64_t base) {
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
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
#ifndef __cplusplus
  extern int64_t (fg314k_ld_i64)(uint64_t, int64_t);
#endif
  t5 = fg314k_ld_i64(t1, t4);
  t0 = ((int64_t)(t5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_11(uint64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t0 = gg_function_8(t1, t6, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_13(uint64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t0 = gg_function_8(t1, t6, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(uint64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t0 = gg_function_8(t1, t6, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_12(uint64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t0 = gg_function_8(t1, t6, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_7(uint64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t0 = gg_function_8(t1, t4, base + 0);
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
  uint64_t t37;
  uint64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint64_t t50;
  uint64_t t51;
  uint64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  uint64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  uint64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  uint64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  uint64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint64_t t96;
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
  int64_t t107;
  uint64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  uint64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  uint64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  uint64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  uint64_t t140;
  int64_t t141;
  int64_t t142;
  uint8_t t143;
  uint8_t t144;
  int64_t t145;
  int64_t t146;
  uint8_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  uint8_t t151;
  uint8_t t152;
  int64_t t153;
  int64_t t154;
  uint8_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  uint8_t t159;
  int64_t t160;
  int64_t t161;
  uint64_t t162;
  int64_t t163;
  int64_t t164;
  uint64_t t165;
  int64_t t166;
  uint64_t t167;
  int64_t t168;
  uint64_t t169;
  int64_t t170;
  uint64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  uint64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  uint8_t t181;
  int64_t t182;
  int64_t t183;
  uint64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  uint8_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  uint64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  uint64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  uint8_t t211;
  uint8_t t212;
  uint64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  uint64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  uint64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  uint64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  uint64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  uint64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  uint64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  uint64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  uint64_t t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  uint64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  int64_t t287;
  uint64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  uint64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  uint64_t t301;
  int64_t t302;
  uint64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  uint64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  uint8_t t323;
  int64_t t324;
  int64_t t325;
  uint8_t t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  uint64_t t331;
  int64_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  uint64_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  uint64_t t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  uint64_t t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t t364;
  int64_t t365;
  int64_t t366;
  uint8_t t367;
  int64_t t368;
  int64_t t369;
  uint64_t t370;
  int64_t t371;
  int64_t t372;
  int64_t t373;
  uint64_t t374;
  int64_t t375;
  uint64_t t376;
  int64_t t377;
  uint64_t t378;
  int64_t t379;
  uint64_t t380;
  int64_t t381;
  uint64_t t382;
  int64_t t383;
  int64_t t384;
  int64_t t385;
  uint8_t t386;
  uint64_t t387;
  int64_t t388;
  int64_t t389;
  int64_t t390;
  int64_t t391;
  int64_t t392;
  int64_t t393;
  int64_t t394;
  int64_t t395;
  int64_t t396;
  uint8_t t397;
  int64_t t398;
  int64_t t399;
  int64_t t400;
  int64_t t401;
  int64_t t402;
  int64_t t403;
  int64_t t404;
  int64_t t405;
  uint64_t t406;
  int64_t t407;
  int64_t t408;
  int64_t t409;
  int64_t t410;
  uint8_t t411;
  int64_t t412;
  int64_t t413;
  int64_t t414;
  int64_t t415;
  int64_t t416;
  int64_t t417;
  uint64_t t418;
  int64_t t419;
  int64_t t420;
  int64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  int64_t t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  uint64_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  int64_t t434;
  int64_t t435;
  int64_t t436;
  int64_t t437;
  int64_t t438;
  int64_t t439;
  uint8_t t440;
  int64_t t441;
  int64_t t442;
  uint8_t t443;
  int64_t t444;
  int64_t t445;
  int64_t t446;
  int64_t t447;
  uint64_t t448;
  int64_t t449;
  int64_t t450;
  int64_t t451;
  int64_t t452;
  int64_t t453;
  int64_t t454;
  int64_t t455;
  int64_t t456;
  int64_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  uint64_t t461;
  int64_t t462;
  int64_t t463;
  int64_t t464;
  int64_t t465;
  int64_t t466;
  int64_t t467;
  int64_t t468;
  int64_t t469;
  uint8_t t470;
  uint64_t t471;
  int64_t t472;
  int64_t t473;
  int64_t t474;
  int64_t t475;
  int64_t t476;
  int64_t t477;
  int64_t t478;
  uint8_t t479;
  uint64_t t480;
  int64_t t481;
  int64_t t482;
  int64_t t483;
  int64_t t484;
  int64_t t485;
  int64_t t486;
  int64_t t487;
  int64_t t488;
  int64_t t489;
  int64_t t490;
  int64_t t491;
  int64_t t492;
  int64_t t493;
  uint8_t t494;
  uint8_t t495;
  uint64_t t496;
  int64_t t497;
  uint64_t t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  int64_t t502;
  int64_t t503;
  int64_t t504;
  int64_t t505;
  int64_t t506;
  int64_t t507;
  int64_t t508;
  int64_t t509;
  int64_t t510;
  int64_t t511;
  uint64_t t512;
  int64_t t513;
  int64_t t514;
  int64_t t515;
  int64_t t516;
  int64_t t517;
  int64_t t518;
  uint8_t t519;
  int64_t t520;
  int64_t t521;
  int64_t t522;
  int64_t t523;
  uint8_t t524;
  uint64_t t525;
  int64_t t526;
  int64_t t527;
  int64_t t528;
  int64_t t529;
  int64_t t530;
  int64_t t531;
  uint8_t t532;
  uint64_t t533;
  int64_t t534;
  int64_t t535;
  int64_t t536;
  uint64_t t537;
  int64_t t538;
  int64_t t539;
  int64_t t540;
  int64_t t541;
  int64_t t542;
  int64_t t543;
  int64_t t544;
  int64_t t545;
  uint8_t t546;
  int64_t t547;
  int64_t t548;
  int64_t t549;
  int64_t t550;
  uint8_t t551;
  uint8_t t552;
  uint64_t t553;
  int64_t t554;
  int64_t t555;
  int64_t t556;
  int64_t t557;
  int64_t t558;
  int64_t t559;
  int64_t t560;
  int64_t t561;
  int64_t t562;
  int64_t t563;
  int64_t t564;
  int64_t t565;
  int64_t t566;
  int64_t t567;
  uint8_t t568;
  int64_t t569;
  int64_t t570;
  int64_t t571;
  int64_t t572;
  uint64_t t573;
  int64_t t574;
  int64_t t575;
  int64_t t576;
  uint64_t t577;
  int64_t v0;
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
  uint8_t v12;
  uint64_t v13;
  uint64_t v14;
  uint64_t v15;
  uint64_t v16;
  int64_t v17;
  uint64_t v18;
  uint64_t v19;
  uint64_t v20;
  int64_t v21;
  uint64_t v22;
  int64_t v23;
  uint64_t v24;
  uint64_t v25;
  uint64_t v26;
  uint64_t v27;
  uint64_t v28;
  uint64_t v29;
  uint64_t v30;
  uint64_t v31;
  uint64_t v32;
  uint64_t v33;
  uint64_t v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
  int64_t v42;
  int64_t v43;
  uint8_t v44;
  int64_t v45;
  int64_t v46;
  int64_t v47;
  int64_t v48;
  int64_t v49;
  int64_t v50;
  int64_t v51;
  int64_t v52;
  int64_t v53;
  uint8_t v54;
  int64_t v55;
  int64_t v56;
  int64_t v57;
  int64_t v58;
  int64_t v59;
  int64_t v60;
  int64_t v61;
  int64_t v62;
  int64_t v63;
  int64_t v64;
  int64_t v65;
  int64_t v66;
  int64_t v67;
  int64_t v68;
  int64_t v69;
  int64_t v70;
  int64_t v71;
  int64_t v72;
  int64_t v73;
  int64_t v74;
  int64_t v75;
  int64_t v76;
  int64_t v77;
  int64_t v78;
  int64_t v79;
  int64_t v80;
  int64_t v81;
  uint8_t v82;
  int64_t v83;
  int64_t v84;
  int64_t v85;
  int64_t v86;
  int64_t v87;
  int64_t v88;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t4 = fg314k_get(t1, t2, t3);
  v1 = ((int64_t)(((int64_t)(t4))));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t8 = fg314k_get(t5, t6, t7);
  v2 = ((int64_t)(((int64_t)(t8))));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t12 = fg314k_get(t9, t10, t11);
  v3 = ((int64_t)(((int64_t)(t12))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t16 = fg314k_get(t13, t14, t15);
  v4 = ((int64_t)(((int64_t)(t16))));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t20 = fg314k_get(t17, t18, t19);
  v5 = ((int64_t)(((int64_t)(t20))));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(5))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t24 = fg314k_get(t21, t22, t23);
  v6 = ((int64_t)(((int64_t)(t24))));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((int64_t)(((int64_t)UINT64_C(6))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t28 = fg314k_get(t25, t26, t27);
  v7 = ((int64_t)(((int64_t)(t28))));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(((int64_t)UINT64_C(7))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t32 = fg314k_get(t29, t30, t31);
  v8 = ((int64_t)(((int64_t)(t32))));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t36 = fg314k_get(t33, t34, t35);
  t37 = ((uint64_t)(t36));
  t38 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  v9 = ((t37) != (t38));
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((int64_t)(((int64_t)UINT64_C(9))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t42 = fg314k_get(t39, t40, t41);
  v10 = ((int64_t)(((int64_t)(t42))));
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = ((int64_t)(((int64_t)UINT64_C(10))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t46 = fg314k_get(t43, t44, t45);
  v11 = ((int64_t)(((int64_t)(t46))));
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(((int64_t)UINT64_C(11))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t50 = fg314k_get(t47, t48, t49);
  t51 = ((uint64_t)(t50));
  t52 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  v12 = ((t51) != (t52));
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t56 = fg314k_get(t53, t54, t55);
  v13 = ((uint64_t)(t56));
  t57 = ((int64_t)(v0));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t60 = fg314k_get(t57, t58, t59);
  v14 = ((uint64_t)(t60));
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t63 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t64 = fg314k_get(t61, t62, t63);
  v15 = ((uint64_t)(t64));
  t65 = ((int64_t)(v0));
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  t67 = ((int64_t)(((int64_t)UINT64_C(3))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t68 = fg314k_get(t65, t66, t67);
  v16 = ((uint64_t)(t68));
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(((int64_t)UINT64_C(1))));
  t71 = ((int64_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t72 = fg314k_get(t69, t70, t71);
  v17 = ((int64_t)(((int64_t)(t72))));
  t73 = ((int64_t)(v0));
  t74 = ((int64_t)(((int64_t)UINT64_C(1))));
  t75 = ((int64_t)(((int64_t)UINT64_C(5))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t76 = fg314k_get(t73, t74, t75);
  v18 = ((uint64_t)(t76));
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  t79 = ((int64_t)(((int64_t)UINT64_C(6))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t80 = fg314k_get(t77, t78, t79);
  v19 = ((uint64_t)(t80));
  t81 = ((int64_t)(v0));
  t82 = ((int64_t)(((int64_t)UINT64_C(1))));
  t83 = ((int64_t)(((int64_t)UINT64_C(7))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t84 = fg314k_get(t81, t82, t83);
  v20 = ((uint64_t)(t84));
  t85 = ((int64_t)(v0));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((int64_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t88 = fg314k_get(t85, t86, t87);
  v21 = ((int64_t)(((int64_t)(t88))));
  t89 = ((int64_t)(v0));
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  t91 = ((int64_t)(((int64_t)UINT64_C(9))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t92 = fg314k_get(t89, t90, t91);
  v22 = ((uint64_t)(t92));
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(((int64_t)UINT64_C(1))));
  t95 = ((int64_t)(((int64_t)UINT64_C(10))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t96 = fg314k_get(t93, t94, t95);
  v23 = ((int64_t)(((int64_t)(t96))));
  t97 = ((int64_t)(v0));
  t98 = ((int64_t)(((int64_t)UINT64_C(1))));
  t99 = ((int64_t)(((int64_t)UINT64_C(11))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t100 = fg314k_get(t97, t98, t99);
  v24 = ((uint64_t)(t100));
  t101 = ((int64_t)(v0));
  t102 = ((int64_t)(((int64_t)UINT64_C(2))));
  t103 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t104 = fg314k_get(t101, t102, t103);
  v25 = ((uint64_t)(t104));
  t105 = ((int64_t)(v0));
  t106 = ((int64_t)(((int64_t)UINT64_C(2))));
  t107 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t108 = fg314k_get(t105, t106, t107);
  v26 = ((uint64_t)(t108));
  t109 = ((int64_t)(v0));
  t110 = ((int64_t)(((int64_t)UINT64_C(2))));
  t111 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t112 = fg314k_get(t109, t110, t111);
  v27 = ((uint64_t)(t112));
  t113 = ((int64_t)(v0));
  t114 = ((int64_t)(((int64_t)UINT64_C(2))));
  t115 = ((int64_t)(((int64_t)UINT64_C(3))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t116 = fg314k_get(t113, t114, t115);
  v28 = ((uint64_t)(t116));
  t117 = ((int64_t)(v0));
  t118 = ((int64_t)(((int64_t)UINT64_C(2))));
  t119 = ((int64_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t120 = fg314k_get(t117, t118, t119);
  v29 = ((uint64_t)(t120));
  t121 = ((int64_t)(v0));
  t122 = ((int64_t)(((int64_t)UINT64_C(2))));
  t123 = ((int64_t)(((int64_t)UINT64_C(5))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t124 = fg314k_get(t121, t122, t123);
  v30 = ((uint64_t)(t124));
  t125 = ((int64_t)(v0));
  t126 = ((int64_t)(((int64_t)UINT64_C(2))));
  t127 = ((int64_t)(((int64_t)UINT64_C(6))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t128 = fg314k_get(t125, t126, t127);
  v31 = ((uint64_t)(t128));
  t129 = ((int64_t)(v0));
  t130 = ((int64_t)(((int64_t)UINT64_C(2))));
  t131 = ((int64_t)(((int64_t)UINT64_C(7))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t132 = fg314k_get(t129, t130, t131);
  v32 = ((uint64_t)(t132));
  t133 = ((int64_t)(v0));
  t134 = ((int64_t)(((int64_t)UINT64_C(2))));
  t135 = ((int64_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t136 = fg314k_get(t133, t134, t135);
  v33 = ((uint64_t)(t136));
  t137 = ((int64_t)(v0));
  t138 = ((int64_t)(((int64_t)UINT64_C(2))));
  t139 = ((int64_t)(((int64_t)UINT64_C(9))));
#ifndef __cplusplus
  extern uint64_t (fg314k_get)(int64_t, int64_t, int64_t);
#endif
  t140 = fg314k_get(t137, t138, t139);
  v34 = ((uint64_t)(t140));
  t141 = ((int64_t)(v2));
  t142 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t143 = ((t141) >= (t142));
  t144 = t143;
  if (t144) {
  t145 = ((int64_t)(v3));
  t146 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t144 = ((t145) > (t146));
  }
  t147 = t144;
  t148 = ((int64_t)(((int64_t)UINT64_C(60))));
  gg_function_3(t147, t148, base + 0);
  t149 = ((int64_t)(v21));
  t150 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t151 = ((t149) >= (t150));
  t152 = t151;
  if (t152) {
  t153 = ((int64_t)(v8));
  t154 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t152 = ((t153) >= (t154));
  }
  t155 = t152;
  t156 = ((int64_t)(((int64_t)UINT64_C(61))));
  gg_function_3(t155, t156, base + 0);
  v35 = ((int64_t)(v5));
  v36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t157 = ((int64_t)(v1));
  t158 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t159 = ((t157) == (t158));
  if (t159) {
  {
  v37 = ((int64_t)(((int64_t)UINT64_C(0))));
  v38 = ((int64_t)(((int64_t)UINT64_C(0))));
  v39 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t160 = ((int64_t)(v39));
  t161 = ((int64_t)(v8));
  if (!(((t160) < (t161)))) goto label2;
  {
  t162 = ((uint64_t)(v33));
  t163 = ((int64_t)(v39));
  t164 = ((int64_t)(v37));
  gg_function_5(t162, t163, t164, base + 0);
  t165 = ((uint64_t)(v13));
  t166 = ((int64_t)(v39));
  v40 = gg_function_7(t165, t166, base + 0);
  t167 = ((uint64_t)(v13));
  t168 = ((int64_t)(v39));
  v41 = gg_function_10(t167, t168, base + 0);
  t169 = ((uint64_t)(v13));
  t170 = ((int64_t)(v39));
  v42 = gg_function_11(t169, t170, base + 0);
  t171 = ((uint64_t)(v13));
  t172 = ((int64_t)(v39));
  v43 = gg_function_12(t171, t172, base + 0);
  v44 = ((uint8_t)UINT64_C(0));
  t173 = ((int64_t)(v6));
  t174 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t175 = ((t173) > (t174));
  if (t175) {
  {
  t176 = ((uint64_t)(v18));
  t177 = ((int64_t)(v39));
  t178 = gg_function_8(t176, t177, base + 0);
  t179 = ((int64_t)(t178));
  t180 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t181 = ((t179) == (t180));
  if (t181) {
  {
  v44 = ((uint8_t)UINT64_C(1));
  }
  }
  }
  }
  v45 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t182 = ((int64_t)(v45));
  t183 = ((int64_t)(v42));
  if (!(((t182) < (t183)))) goto label5;
  {
  t184 = ((uint64_t)(v24));
  t185 = ((int64_t)(v43));
  t186 = ((int64_t)(v45));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t185) + (uint64_t)(t186)))));
  v46 = gg_function_8(t184, t187, base + 0);
  t188 = ((int64_t)(v37));
  t189 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t188) + (uint64_t)(t189)))));
  t191 = ((int64_t)(v3));
  t192 = ((t190) < (t191));
  t193 = ((int64_t)(((int64_t)UINT64_C(62))));
  gg_function_3(t192, t193, base + 0);
  t194 = ((int64_t)(v46));
  t195 = ((int64_t)(v2));
  t196 = ((int64_t)(((int64_t)((uint64_t)(t194) + (uint64_t)(t195)))));
  t197 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v47 = ((int64_t)(((int64_t)((uint64_t)(t196) - (uint64_t)(t197)))));
  t198 = ((uint64_t)(v15));
  t199 = ((int64_t)(v41));
  t200 = ((int64_t)(v47));
  t201 = ((int64_t)(((int64_t)((uint64_t)(t199) + (uint64_t)(t200)))));
  v48 = gg_function_8(t198, t201, base + 0);
  t202 = ((uint64_t)(v25));
  t203 = ((int64_t)(v37));
  t204 = ((int64_t)(v40));
  t205 = ((int64_t)(v5));
  t206 = ((int64_t)(((int64_t)((uint64_t)(t204) * (uint64_t)(t205)))));
  t207 = ((int64_t)(v48));
  t208 = ((int64_t)(((int64_t)((uint64_t)(t206) + (uint64_t)(t207)))));
  gg_function_5(t202, t203, t208, base + 0);
  v49 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t209 = ((int64_t)(v49));
  t210 = ((int64_t)(v2));
  if (!(((t209) < (t210)))) goto label8;
  {
  v50 = ((int64_t)(v35));
  t211 = v44;
  t212 = (!(t211));
  if (t212) {
  {
  t213 = ((uint64_t)(v15));
  t214 = ((int64_t)(v41));
  t215 = ((int64_t)(v46));
  t216 = ((int64_t)(((int64_t)((uint64_t)(t214) + (uint64_t)(t215)))));
  t217 = ((int64_t)(v49));
  t218 = ((int64_t)(((int64_t)((uint64_t)(t216) + (uint64_t)(t217)))));
  v50 = gg_function_8(t213, t218, base + 0);
  }
  }
  t219 = ((uint64_t)(v26));
  t220 = ((int64_t)(v37));
  t221 = ((int64_t)(v2));
  t222 = ((int64_t)(((int64_t)((uint64_t)(t220) * (uint64_t)(t221)))));
  t223 = ((int64_t)(v49));
  t224 = ((int64_t)(((int64_t)((uint64_t)(t222) + (uint64_t)(t223)))));
  t225 = ((int64_t)(v50));
  gg_function_5(t219, t224, t225, base + 0);
  t226 = ((int64_t)(v49));
  t227 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v49 = ((int64_t)(((int64_t)((uint64_t)(t226) + (uint64_t)(t227)))));
  }
  goto label7;
label8:;
  t228 = ((uint64_t)(v16));
  t229 = ((int64_t)(v37));
  t230 = gg_function_8(t228, t229, base + 0);
  t231 = ((int64_t)(t230));
  t232 = ((int64_t)(v17));
  t233 = ((t231) == (t232));
  if (t233) {
  {
  v51 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t234 = ((int64_t)(v51));
  t235 = ((int64_t)(v2));
  if (!(((t234) < (t235)))) goto label11;
  {
  t236 = ((uint64_t)(v27));
  t237 = ((int64_t)(v38));
  t238 = ((int64_t)(v2));
  t239 = ((int64_t)(((int64_t)((uint64_t)(t237) * (uint64_t)(t238)))));
  t240 = ((int64_t)(v51));
  t241 = ((int64_t)(((int64_t)((uint64_t)(t239) + (uint64_t)(t240)))));
  t242 = ((int64_t)(v40));
  t243 = ((int64_t)(v5));
  t244 = ((int64_t)(((int64_t)((uint64_t)(t242) * (uint64_t)(t243)))));
  t245 = ((uint64_t)(v15));
  t246 = ((int64_t)(v41));
  t247 = ((int64_t)(v46));
  t248 = ((int64_t)(((int64_t)((uint64_t)(t246) + (uint64_t)(t247)))));
  t249 = ((int64_t)(v51));
  t250 = ((int64_t)(((int64_t)((uint64_t)(t248) + (uint64_t)(t249)))));
  t251 = gg_function_8(t245, t250, base + 0);
  t252 = ((int64_t)(t251));
  t253 = ((int64_t)(((int64_t)((uint64_t)(t244) + (uint64_t)(t252)))));
  gg_function_5(t236, t241, t253, base + 0);
  t254 = ((int64_t)(v51));
  t255 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t254) + (uint64_t)(t255)))));
  }
  goto label10;
label11:;
  t256 = ((uint64_t)(v28));
  t257 = ((int64_t)(v38));
  t258 = ((int64_t)(v40));
  t259 = ((int64_t)(v5));
  t260 = ((int64_t)(((int64_t)((uint64_t)(t258) * (uint64_t)(t259)))));
  t261 = ((int64_t)(v48));
  t262 = ((int64_t)(((int64_t)((uint64_t)(t260) + (uint64_t)(t261)))));
  gg_function_5(t256, t257, t262, base + 0);
  t263 = ((uint64_t)(v29));
  t264 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t265 = ((int64_t)(v4));
  t266 = ((int64_t)(((int64_t)((uint64_t)(t264) * (uint64_t)(t265)))));
  t267 = ((int64_t)(v38));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t266) + (uint64_t)(t267)))));
  t269 = ((uint64_t)(v14));
  t270 = ((int64_t)(v41));
  t271 = ((int64_t)(v46));
  t272 = ((int64_t)(((int64_t)((uint64_t)(t270) + (uint64_t)(t271)))));
  t273 = gg_function_8(t269, t272, base + 0);
  gg_function_5(t263, t268, t273, base + 0);
  t274 = ((uint64_t)(v29));
  t275 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t276 = ((int64_t)(v4));
  t277 = ((int64_t)(((int64_t)((uint64_t)(t275) * (uint64_t)(t276)))));
  t278 = ((int64_t)(v38));
  t279 = ((int64_t)(((int64_t)((uint64_t)(t277) + (uint64_t)(t278)))));
  t280 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_5(t274, t279, t280, base + 0);
  t281 = ((uint64_t)(v29));
  t282 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t283 = ((int64_t)(v4));
  t284 = ((int64_t)(((int64_t)((uint64_t)(t282) * (uint64_t)(t283)))));
  t285 = ((int64_t)(v38));
  t286 = ((int64_t)(((int64_t)((uint64_t)(t284) + (uint64_t)(t285)))));
  t287 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_5(t281, t286, t287, base + 0);
  t288 = ((uint64_t)(v29));
  t289 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t290 = ((int64_t)(v4));
  t291 = ((int64_t)(((int64_t)((uint64_t)(t289) * (uint64_t)(t290)))));
  t292 = ((int64_t)(v38));
  t293 = ((int64_t)(((int64_t)((uint64_t)(t291) + (uint64_t)(t292)))));
  t294 = ((uint64_t)(v14));
  t295 = ((int64_t)(v41));
  t296 = ((int64_t)(v46));
  t297 = ((int64_t)(((int64_t)((uint64_t)(t295) + (uint64_t)(t296)))));
  t298 = gg_function_8(t294, t297, base + 0);
  gg_function_5(t288, t293, t298, base + 0);
  t299 = ((int64_t)(v38));
  t300 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t299) + (uint64_t)(t300)))));
  }
  }
  t301 = ((uint64_t)(v34));
  t302 = ((int64_t)(v37));
  t303 = ((uint64_t)(v14));
  t304 = ((int64_t)(v41));
  t305 = ((int64_t)(v47));
  t306 = ((int64_t)(((int64_t)((uint64_t)(t304) + (uint64_t)(t305)))));
  t307 = gg_function_8(t303, t306, base + 0);
  gg_function_5(t301, t302, t307, base + 0);
  t308 = ((int64_t)(v37));
  t309 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v37 = ((int64_t)(((int64_t)((uint64_t)(t308) + (uint64_t)(t309)))));
  t310 = ((int64_t)(v45));
  t311 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v45 = ((int64_t)(((int64_t)((uint64_t)(t310) + (uint64_t)(t311)))));
  }
  goto label4;
label5:;
  t312 = ((int64_t)(v39));
  t313 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t312) + (uint64_t)(t313)))));
  }
  goto label1;
label2:;
  v52 = ((int64_t)(((int64_t)UINT64_C(0))));
  v53 = ((int64_t)(v4));
label13:;
  t314 = ((int64_t)(v53));
  t315 = ((int64_t)(v3));
  if (!(((t314) < (t315)))) goto label14;
  {
  v54 = ((uint8_t)UINT64_C(1));
label16:;
  if (!(v54)) goto label17;
  {
  v54 = ((uint8_t)UINT64_C(0));
  v55 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t316 = ((int64_t)(v55));
  t317 = ((int64_t)(v37));
  if (!(((t316) < (t317)))) goto label20;
  {
  t318 = ((uint64_t)(v25));
  t319 = ((int64_t)(v55));
  t320 = gg_function_8(t318, t319, base + 0);
  t321 = ((int64_t)(t320));
  t322 = ((int64_t)(v52));
  t323 = ((t321) == (t322));
  if (t323) {
  {
  v54 = ((uint8_t)UINT64_C(1));
  }
  }
  t324 = ((int64_t)(v55));
  t325 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v55 = ((int64_t)(((int64_t)((uint64_t)(t324) + (uint64_t)(t325)))));
  }
  goto label19;
label20:;
  t326 = v54;
  if (t326) {
  {
  t327 = ((int64_t)(v52));
  t328 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v52 = ((int64_t)(((int64_t)((uint64_t)(t327) + (uint64_t)(t328)))));
  }
  }
  }
  goto label16;
label17:;
  v56 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t329 = ((int64_t)(v56));
  t330 = ((int64_t)(v2));
  if (!(((t329) < (t330)))) goto label23;
  {
  t331 = ((uint64_t)(v27));
  t332 = ((int64_t)(v53));
  t333 = ((int64_t)(v2));
  t334 = ((int64_t)(((int64_t)((uint64_t)(t332) * (uint64_t)(t333)))));
  t335 = ((int64_t)(v56));
  t336 = ((int64_t)(((int64_t)((uint64_t)(t334) + (uint64_t)(t335)))));
  t337 = ((int64_t)(v52));
  gg_function_5(t331, t336, t337, base + 0);
  t338 = ((int64_t)(v56));
  t339 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v56 = ((int64_t)(((int64_t)((uint64_t)(t338) + (uint64_t)(t339)))));
  }
  goto label22;
label23:;
  t340 = ((uint64_t)(v28));
  t341 = ((int64_t)(v53));
  t342 = ((int64_t)(v52));
  gg_function_5(t340, t341, t342, base + 0);
  t343 = ((int64_t)(v52));
  t344 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v52 = ((int64_t)(((int64_t)((uint64_t)(t343) + (uint64_t)(t344)))));
  t345 = ((int64_t)(v53));
  t346 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v53 = ((int64_t)(((int64_t)((uint64_t)(t345) + (uint64_t)(t346)))));
  }
  goto label13;
label14:;
  v57 = ((int64_t)(v4));
label25:;
  t347 = ((int64_t)(v57));
  t348 = ((int64_t)(v3));
  if (!(((t347) < (t348)))) goto label26;
  {
  t349 = ((uint64_t)(v25));
  t350 = ((int64_t)(v57));
  t351 = ((int64_t)(v23));
  gg_function_5(t349, t350, t351, base + 0);
  v58 = ((int64_t)(((int64_t)UINT64_C(0))));
label28:;
  t352 = ((int64_t)(v58));
  t353 = ((int64_t)(v2));
  if (!(((t352) < (t353)))) goto label29;
  {
  t354 = ((uint64_t)(v26));
  t355 = ((int64_t)(v57));
  t356 = ((int64_t)(v2));
  t357 = ((int64_t)(((int64_t)((uint64_t)(t355) * (uint64_t)(t356)))));
  t358 = ((int64_t)(v58));
  t359 = ((int64_t)(((int64_t)((uint64_t)(t357) + (uint64_t)(t358)))));
  t360 = ((int64_t)(v35));
  gg_function_5(t354, t359, t360, base + 0);
  t361 = ((int64_t)(v58));
  t362 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v58 = ((int64_t)(((int64_t)((uint64_t)(t361) + (uint64_t)(t362)))));
  }
  goto label28;
label29:;
  t363 = ((int64_t)(v57));
  t364 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v57 = ((int64_t)(((int64_t)((uint64_t)(t363) + (uint64_t)(t364)))));
  }
  goto label25;
label26:;
  v36 = ((int64_t)(v38));
  }
  } else {
  {
  t365 = ((int64_t)(v1));
  t366 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t367 = ((t365) == (t366));
  if (t367) {
  {
  v59 = ((int64_t)(((int64_t)UINT64_C(0))));
label31:;
  t368 = ((int64_t)(v59));
  t369 = ((int64_t)(v7));
  if (!(((t368) < (t369)))) goto label32;
  {
  t370 = ((uint64_t)(v20));
  t371 = ((int64_t)(v59));
  t372 = ((int64_t)(v21));
  t373 = ((int64_t)(((int64_t)((uint64_t)(t371) * (uint64_t)(t372)))));
  v60 = gg_function_8(t370, t373, base + 0);
  t374 = ((uint64_t)(v19));
  t375 = ((int64_t)(v59));
  v61 = gg_function_8(t374, t375, base + 0);
  t376 = ((uint64_t)(v13));
  t377 = ((int64_t)(v60));
  v62 = gg_function_10(t376, t377, base + 0);
  t378 = ((uint64_t)(v13));
  t379 = ((int64_t)(v60));
  v63 = gg_function_13(t378, t379, base + 0);
  t380 = ((uint64_t)(v13));
  t381 = ((int64_t)(v60));
  v64 = gg_function_11(t380, t381, base + 0);
  t382 = ((uint64_t)(v33));
  t383 = ((int64_t)(v60));
  v65 = gg_function_8(t382, t383, base + 0);
  t384 = ((int64_t)(v65));
  t385 = ((int64_t)(v64));
  v66 = ((int64_t)(((int64_t)((uint64_t)(t384) + (uint64_t)(t385)))));
  v67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t386 = v9;
  if (t386) {
  {
  t387 = ((uint64_t)(v22));
  t388 = ((int64_t)(v59));
  v68 = gg_function_8(t387, t388, base + 0);
  t389 = ((int64_t)(v68));
  t390 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t391 = ((int64_t)(((int64_t)((uint64_t)(t389) + (uint64_t)(t390)))));
  t392 = ((int64_t)(v2));
  v67 = ((int64_t)(((int64_t)gg_div(t391, t392))));
  t393 = ((int64_t)(v66));
  t394 = ((int64_t)(v65));
  t395 = ((int64_t)(((int64_t)((uint64_t)(t393) - (uint64_t)(t394)))));
  t396 = ((int64_t)(v67));
  t397 = ((t395) < (t396));
  if (t397) {
  {
  t398 = ((int64_t)(v66));
  t399 = ((int64_t)(v65));
  v67 = ((int64_t)(((int64_t)((uint64_t)(t398) - (uint64_t)(t399)))));
  }
  }
  }
  } else {
  {
  v69 = ((int64_t)(v65));
  v70 = ((int64_t)(v66));
label34:;
  t400 = ((int64_t)(v69));
  t401 = ((int64_t)(v70));
  if (!(((t400) < (t401)))) goto label35;
  {
  t402 = ((int64_t)(v69));
  t403 = ((int64_t)(v70));
  t404 = ((int64_t)(((int64_t)((uint64_t)(t402) + (uint64_t)(t403)))));
  t405 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v71 = ((int64_t)(((int64_t)gg_div(t404, t405))));
  t406 = ((uint64_t)(v34));
  t407 = ((int64_t)(v71));
  t408 = gg_function_8(t406, t407, base + 0);
  t409 = ((int64_t)(t408));
  t410 = ((int64_t)(v61));
  t411 = ((t409) <= (t410));
  if (t411) {
  {
  t412 = ((int64_t)(v71));
  t413 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v69 = ((int64_t)(((int64_t)((uint64_t)(t412) + (uint64_t)(t413)))));
  }
  } else {
  {
  v70 = ((int64_t)(v71));
  }
  }
  }
  goto label34;
label35:;
  t414 = ((int64_t)(v69));
  t415 = ((int64_t)(v65));
  v67 = ((int64_t)(((int64_t)((uint64_t)(t414) - (uint64_t)(t415)))));
  }
  }
  v72 = ((int64_t)(((int64_t)UINT64_C(0))));
label37:;
  t416 = ((int64_t)(v72));
  t417 = ((int64_t)(v3));
  if (!(((t416) < (t417)))) goto label38;
  {
  t418 = ((uint64_t)(v30));
  t419 = ((int64_t)(v59));
  t420 = ((int64_t)(v3));
  t421 = ((int64_t)(((int64_t)((uint64_t)(t419) * (uint64_t)(t420)))));
  t422 = ((int64_t)(v72));
  t423 = ((int64_t)(((int64_t)((uint64_t)(t421) + (uint64_t)(t422)))));
  t424 = gg_function_14(base + 0);
  gg_function_5(t418, t423, t424, base + 0);
  t425 = ((int64_t)(v72));
  t426 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v72 = ((int64_t)(((int64_t)((uint64_t)(t425) + (uint64_t)(t426)))));
  }
  goto label37;
label38:;
  v73 = ((int64_t)(((int64_t)UINT64_C(0))));
label40:;
  t427 = ((int64_t)(v73));
  t428 = ((int64_t)(v67));
  if (!(((t427) < (t428)))) goto label41;
  {
  t429 = ((uint64_t)(v30));
  t430 = ((int64_t)(v59));
  t431 = ((int64_t)(v3));
  t432 = ((int64_t)(((int64_t)((uint64_t)(t430) * (uint64_t)(t431)))));
  t433 = ((int64_t)(v65));
  t434 = ((int64_t)(((int64_t)((uint64_t)(t432) + (uint64_t)(t433)))));
  t435 = ((int64_t)(v73));
  t436 = ((int64_t)(((int64_t)((uint64_t)(t434) + (uint64_t)(t435)))));
  t437 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_5(t429, t436, t437, base + 0);
  t438 = ((int64_t)(v73));
  t439 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v73 = ((int64_t)(((int64_t)((uint64_t)(t438) + (uint64_t)(t439)))));
  }
  goto label40;
label41:;
  t440 = v12;
  if (t440) {
  {
  v74 = ((int64_t)(v67));
  t441 = ((int64_t)(v11));
  t442 = ((int64_t)(v74));
  t443 = ((t441) < (t442));
  if (t443) {
  {
  v74 = ((int64_t)(v11));
  }
  }
  v75 = ((int64_t)(((int64_t)UINT64_C(0))));
label43:;
  t444 = ((int64_t)(v75));
  t445 = ((int64_t)(v11));
  t446 = ((int64_t)(v2));
  t447 = ((int64_t)(((int64_t)((uint64_t)(t445) * (uint64_t)(t446)))));
  if (!(((t444) < (t447)))) goto label44;
  {
  t448 = ((uint64_t)(v31));
  t449 = ((int64_t)(v59));
  t450 = ((int64_t)(v10));
  t451 = ((int64_t)(((int64_t)((uint64_t)(t449) * (uint64_t)(t450)))));
  t452 = ((int64_t)(v75));
  t453 = ((int64_t)(((int64_t)((uint64_t)(t451) + (uint64_t)(t452)))));
  t454 = gg_function_14(base + 0);
  gg_function_5(t448, t453, t454, base + 0);
  t455 = ((int64_t)(v75));
  t456 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v75 = ((int64_t)(((int64_t)((uint64_t)(t455) + (uint64_t)(t456)))));
  }
  goto label43;
label44:;
  v76 = ((int64_t)(((int64_t)UINT64_C(0))));
label46:;
  t457 = ((int64_t)(v76));
  t458 = ((int64_t)(v74));
  t459 = ((int64_t)(v2));
  t460 = ((int64_t)(((int64_t)((uint64_t)(t458) * (uint64_t)(t459)))));
  if (!(((t457) < (t460)))) goto label47;
  {
  t461 = ((uint64_t)(v31));
  t462 = ((int64_t)(v59));
  t463 = ((int64_t)(v10));
  t464 = ((int64_t)(((int64_t)((uint64_t)(t462) * (uint64_t)(t463)))));
  t465 = ((int64_t)(v76));
  t466 = ((int64_t)(((int64_t)((uint64_t)(t464) + (uint64_t)(t465)))));
  t467 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_5(t461, t466, t467, base + 0);
  t468 = ((int64_t)(v76));
  t469 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v76 = ((int64_t)(((int64_t)((uint64_t)(t468) + (uint64_t)(t469)))));
  }
  goto label46;
label47:;
  }
  }
  v77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t470 = v9;
  if (t470) {
  {
  t471 = ((uint64_t)(v22));
  t472 = ((int64_t)(v59));
  v78 = gg_function_8(t471, t472, base + 0);
  t473 = ((int64_t)(v78));
  t474 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t475 = ((int64_t)(((int64_t)((uint64_t)(t473) + (uint64_t)(t474)))));
  t476 = ((int64_t)(v2));
  v77 = ((int64_t)(((int64_t)gg_mod(t475, t476))));
  }
  } else {
  {
  v79 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t477 = ((int64_t)(v63));
  t478 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t479 = ((t477) > (t478));
  if (t479) {
  {
  t480 = ((uint64_t)(v14));
  t481 = ((int64_t)(v62));
  v79 = gg_function_8(t480, t481, base + 0);
  }
  }
  t482 = ((int64_t)(v61));
  t483 = ((int64_t)(v79));
  t484 = ((int64_t)(((int64_t)((uint64_t)(t482) - (uint64_t)(t483)))));
  t485 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t486 = ((int64_t)(((int64_t)((uint64_t)(t484) + (uint64_t)(t485)))));
  t487 = ((int64_t)(v2));
  v77 = ((int64_t)(((int64_t)gg_mod(t486, t487))));
  }
  }
  v80 = ((int64_t)(((int64_t)UINT64_C(0))));
label49:;
  t488 = ((int64_t)(v80));
  t489 = ((int64_t)(v2));
  t490 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t491 = ((int64_t)(((int64_t)((uint64_t)(t489) - (uint64_t)(t490)))));
  if (!(((t488) < (t491)))) goto label50;
  {
  v81 = ((int64_t)(v35));
  v82 = ((uint8_t)UINT64_C(0));
  t492 = ((int64_t)(v80));
  t493 = ((int64_t)(v77));
  t494 = ((t492) < (t493));
  if (t494) {
  {
  t495 = v9;
  if (t495) {
  {
  t496 = ((uint64_t)(v22));
  t497 = ((int64_t)(v59));
  v83 = gg_function_8(t496, t497, base + 0);
  t498 = ((uint64_t)(v15));
  t499 = ((int64_t)(v62));
  t500 = ((int64_t)(v83));
  t501 = ((int64_t)(((int64_t)((uint64_t)(t499) + (uint64_t)(t500)))));
  t502 = ((int64_t)(v80));
  t503 = ((int64_t)(((int64_t)((uint64_t)(t501) - (uint64_t)(t502)))));
  v81 = gg_function_8(t498, t503, base + 0);
  v82 = ((uint8_t)UINT64_C(1));
  }
  } else {
  {
  t504 = ((int64_t)(v61));
  t505 = ((int64_t)(v80));
  v84 = ((int64_t)(((int64_t)((uint64_t)(t504) - (uint64_t)(t505)))));
  v85 = ((int64_t)(((int64_t)UINT64_C(0))));
  v86 = ((int64_t)(v63));
label52:;
  t506 = ((int64_t)(v85));
  t507 = ((int64_t)(v86));
  if (!(((t506) < (t507)))) goto label53;
  {
  t508 = ((int64_t)(v85));
  t509 = ((int64_t)(v86));
  t510 = ((int64_t)(((int64_t)((uint64_t)(t508) + (uint64_t)(t509)))));
  t511 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v87 = ((int64_t)(((int64_t)gg_div(t510, t511))));
  t512 = ((uint64_t)(v14));
  t513 = ((int64_t)(v62));
  t514 = ((int64_t)(v87));
  t515 = ((int64_t)(((int64_t)((uint64_t)(t513) + (uint64_t)(t514)))));
  t516 = gg_function_8(t512, t515, base + 0);
  t517 = ((int64_t)(t516));
  t518 = ((int64_t)(v84));
  t519 = ((t517) < (t518));
  if (t519) {
  {
  t520 = ((int64_t)(v87));
  t521 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v85 = ((int64_t)(((int64_t)((uint64_t)(t520) + (uint64_t)(t521)))));
  }
  } else {
  {
  v86 = ((int64_t)(v87));
  }
  }
  }
  goto label52;
label53:;
  t522 = ((int64_t)(v85));
  t523 = ((int64_t)(v63));
  t524 = ((t522) < (t523));
  if (t524) {
  {
  t525 = ((uint64_t)(v14));
  t526 = ((int64_t)(v62));
  t527 = ((int64_t)(v85));
  t528 = ((int64_t)(((int64_t)((uint64_t)(t526) + (uint64_t)(t527)))));
  t529 = gg_function_8(t525, t528, base + 0);
  t530 = ((int64_t)(t529));
  t531 = ((int64_t)(v84));
  t532 = ((t530) == (t531));
  if (t532) {
  {
  t533 = ((uint64_t)(v15));
  t534 = ((int64_t)(v62));
  t535 = ((int64_t)(v85));
  t536 = ((int64_t)(((int64_t)((uint64_t)(t534) + (uint64_t)(t535)))));
  v81 = gg_function_8(t533, t536, base + 0);
  v82 = ((uint8_t)UINT64_C(1));
  }
  }
  }
  }
  }
  }
  }
  }
  t537 = ((uint64_t)(v32));
  t538 = ((int64_t)(v59));
  t539 = ((int64_t)(v2));
  t540 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t541 = ((int64_t)(((int64_t)((uint64_t)(t539) - (uint64_t)(t540)))));
  t542 = ((int64_t)(((int64_t)((uint64_t)(t538) * (uint64_t)(t541)))));
  t543 = ((int64_t)(v80));
  t544 = ((int64_t)(((int64_t)((uint64_t)(t542) + (uint64_t)(t543)))));
  t545 = ((int64_t)(v81));
  gg_function_5(t537, t544, t545, base + 0);
  t546 = v12;
  if (t546) {
  {
  t547 = ((int64_t)(v10));
  t548 = ((int64_t)(v2));
  t549 = ((int64_t)(((int64_t)gg_mod(t547, t548))));
  t550 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t551 = ((t549) != (t550));
  if (t551) {
  {
  v88 = gg_function_14(base + 0);
  t552 = v82;
  if (t552) {
  {
  v88 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t553 = ((uint64_t)(v31));
  t554 = ((int64_t)(v59));
  t555 = ((int64_t)(v10));
  t556 = ((int64_t)(((int64_t)((uint64_t)(t554) * (uint64_t)(t555)))));
  t557 = ((int64_t)(v11));
  t558 = ((int64_t)(v2));
  t559 = ((int64_t)(((int64_t)((uint64_t)(t557) * (uint64_t)(t558)))));
  t560 = ((int64_t)(((int64_t)((uint64_t)(t556) + (uint64_t)(t559)))));
  t561 = ((int64_t)(v80));
  t562 = ((int64_t)(((int64_t)((uint64_t)(t560) + (uint64_t)(t561)))));
  t563 = ((int64_t)(v88));
  gg_function_5(t553, t562, t563, base + 0);
  }
  }
  }
  }
  t564 = ((int64_t)(v80));
  t565 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v80 = ((int64_t)(((int64_t)((uint64_t)(t564) + (uint64_t)(t565)))));
  }
  goto label49;
label50:;
  t566 = ((int64_t)(v59));
  t567 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v59 = ((int64_t)(((int64_t)((uint64_t)(t566) + (uint64_t)(t567)))));
  }
  goto label31;
label32:;
  v36 = ((int64_t)(v7));
  }
  } else {
  {
  t568 = ((uint8_t)UINT64_C(0));
  t569 = ((int64_t)(((int64_t)UINT64_C(63))));
  gg_function_3(t568, t569, base + 0);
  }
  }
  }
  }
  t570 = ((int64_t)(v0));
  t571 = ((int64_t)(((int64_t)UINT64_C(3))));
  t572 = ((int64_t)(((int64_t)UINT64_C(0))));
  t573 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern void (fg314k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg314k_put(t570, t571, t572, t573);
  t574 = ((int64_t)(v0));
  t575 = ((int64_t)(((int64_t)UINT64_C(3))));
  t576 = ((int64_t)(((int64_t)UINT64_C(1))));
  t577 = ((uint64_t)(((uint64_t)(v36))));
#ifndef __cplusplus
  extern void (fg314k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg314k_put(t574, t575, t576, t577);
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
