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
static int64_t gg_function_5(uint64_t p0, int64_t p1, int64_t base);
static void gg_function_7(int64_t p0, uint64_t p1, int64_t p2, int64_t base);
static void gg_function_9(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static void gg_function_10(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_10(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  extern void (fg346k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg346k_stw(t0, t3, t4);
  }
gg_done:;
}
static int64_t gg_function_5(uint64_t p0, int64_t p1, int64_t base) {
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
  extern int64_t (fg346k_ld_i64)(uint64_t, int64_t);
#endif
  t5 = fg346k_ld_i64(t1, t4);
  t0 = ((int64_t)(t5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_9(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  uint64_t t0;
  int64_t t1;
  uint64_t t2;
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
  uint64_t t14;
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
  uint64_t v0;
  uint64_t v1;
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
  t0 = ((uint64_t)(v1));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v5 = gg_function_5(t0, t1, base + 0);
  t2 = ((uint64_t)(v0));
  t3 = ((int64_t)(v5));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((int64_t)(v2));
  gg_function_10(t2, t5, t6, base + 0);
  t7 = ((uint64_t)(v0));
  t8 = ((int64_t)(v5));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)(v3));
  gg_function_10(t7, t12, t13, base + 0);
  t14 = ((uint64_t)(v0));
  t15 = ((int64_t)(v5));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = ((int64_t)(v4));
  gg_function_10(t14, t19, t20, base + 0);
  t21 = ((uint64_t)(v1));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t23 = ((int64_t)(v5));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))));
  gg_function_10(t21, t22, t25, base + 0);
  }
gg_done:;
}
static void gg_function_7(int64_t p0, uint64_t p1, int64_t p2, int64_t base) {
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
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(2))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern void (fg346k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg346k_put(t0, t1, t2, t3);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(2))));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((int64_t)(((int64_t)(v2))));
  t9 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t7) - (uint64_t)(t8)))))));
#ifndef __cplusplus
  extern void (fg346k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg346k_put(t4, t5, t6, t9);
  }
gg_done:;
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
  extern void (fg346k_fail)(int64_t);
#endif
  fg346k_fail(t2);
  }
  }
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
  uint8_t t67;
  uint8_t t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint8_t t75;
  int64_t t76;
  int64_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  uint64_t t86;
  int64_t t87;
  uint64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint8_t t103;
  uint64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  uint8_t t108;
  int64_t t109;
  int64_t t110;
  uint8_t t111;
  int64_t t112;
  uint64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint64_t t126;
  int64_t t127;
  uint64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint8_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  uint8_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
  int64_t t147;
  int64_t t148;
  uint8_t t149;
  int64_t t150;
  int64_t t151;
  uint8_t t152;
  int64_t t153;
  int64_t t154;
  uint8_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  uint8_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint8_t t169;
  uint64_t t170;
  uint64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  uint64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  uint64_t t182;
  int64_t t183;
  uint64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  uint8_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  uint8_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  uint8_t t202;
  int64_t t203;
  int64_t t204;
  uint8_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  uint8_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  uint8_t t215;
  uint64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  uint8_t t226;
  uint64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  uint8_t t231;
  int64_t t232;
  int64_t t233;
  uint8_t t234;
  int64_t t235;
  uint64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  uint64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  uint8_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  uint8_t t251;
  int64_t t252;
  int64_t t253;
  uint8_t t254;
  uint64_t t255;
  uint64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  uint8_t t266;
  uint64_t t267;
  uint64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  uint64_t t274;
  int64_t t275;
  uint64_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  uint8_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  uint8_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  uint8_t t294;
  int64_t t295;
  int64_t t296;
  uint8_t t297;
  int64_t t298;
  int64_t t299;
  uint8_t t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  int64_t t304;
  uint8_t t305;
  uint64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  uint8_t t316;
  uint64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  uint8_t t321;
  int64_t t322;
  int64_t t323;
  uint8_t t324;
  int64_t t325;
  uint64_t t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  uint64_t t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  uint8_t t334;
  int64_t t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  int64_t t340;
  uint8_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  uint8_t t346;
  int64_t t347;
  int64_t t348;
  uint64_t t349;
  uint64_t t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t t359;
  uint8_t t360;
  uint64_t t361;
  uint64_t t362;
  int64_t t363;
  int64_t t364;
  int64_t t365;
  uint64_t t366;
  uint64_t t367;
  int64_t t368;
  int64_t t369;
  int64_t t370;
  uint64_t t371;
  uint64_t t372;
  int64_t t373;
  int64_t t374;
  int64_t t375;
  uint64_t t376;
  uint64_t t377;
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
  uint64_t t388;
  int64_t t389;
  int64_t t390;
  uint8_t t391;
  uint64_t t392;
  uint64_t t393;
  int64_t t394;
  int64_t t395;
  int64_t t396;
  uint64_t t397;
  uint64_t t398;
  int64_t t399;
  int64_t t400;
  int64_t t401;
  int64_t t402;
  int64_t t403;
  uint8_t t404;
  int64_t t405;
  int64_t t406;
  uint8_t t407;
  int64_t t408;
  int64_t t409;
  uint8_t t410;
  uint64_t t411;
  uint64_t t412;
  int64_t t413;
  int64_t t414;
  int64_t t415;
  int64_t t416;
  uint64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  uint8_t t421;
  uint64_t t422;
  uint64_t t423;
  int64_t t424;
  int64_t t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  int64_t t429;
  uint64_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  uint64_t t434;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
  int64_t v8;
  uint64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  uint64_t v15;
  uint64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
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
  int64_t v50;
  int64_t v51;
  int64_t v52;
  int64_t v53;
  int64_t v54;
  int64_t v55;
  int64_t v56;
  int64_t v57;
  int64_t v58;
  int64_t v59;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t4 = fg346k_get(t1, t2, t3);
  v1 = ((int64_t)(((int64_t)(t4))));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t8 = fg346k_get(t5, t6, t7);
  v2 = ((int64_t)(((int64_t)(t8))));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t12 = fg346k_get(t9, t10, t11);
  v3 = ((int64_t)(((int64_t)(t12))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t16 = fg346k_get(t13, t14, t15);
  v4 = ((int64_t)(((int64_t)(t16))));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t20 = fg346k_get(t17, t18, t19);
  v5 = ((uint64_t)(t20));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(5))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t24 = fg346k_get(t21, t22, t23);
  v6 = ((uint64_t)(t24));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((int64_t)(((int64_t)UINT64_C(6))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t28 = fg346k_get(t25, t26, t27);
  v7 = ((uint64_t)(t28));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(((int64_t)UINT64_C(7))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t32 = fg346k_get(t29, t30, t31);
  v8 = ((int64_t)(((int64_t)(t32))));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t36 = fg346k_get(t33, t34, t35);
  v9 = ((uint64_t)(t36));
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((int64_t)(((int64_t)UINT64_C(9))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t40 = fg346k_get(t37, t38, t39);
  v10 = ((int64_t)(((int64_t)(t40))));
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((int64_t)(((int64_t)UINT64_C(10))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t44 = fg346k_get(t41, t42, t43);
  v11 = ((int64_t)(((int64_t)(t44))));
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((int64_t)(((int64_t)UINT64_C(11))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t48 = fg346k_get(t45, t46, t47);
  v12 = ((int64_t)(((int64_t)(t48))));
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((int64_t)(((int64_t)UINT64_C(12))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t52 = fg346k_get(t49, t50, t51);
  v13 = ((int64_t)(((int64_t)(t52))));
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((int64_t)(((int64_t)UINT64_C(13))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t56 = fg346k_get(t53, t54, t55);
  v14 = ((int64_t)(((int64_t)(t56))));
  t57 = ((int64_t)(v0));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t60 = fg346k_get(t57, t58, t59);
  v15 = ((uint64_t)(t60));
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg346k_get)(int64_t, int64_t, int64_t);
#endif
  t64 = fg346k_get(t61, t62, t63);
  v16 = ((uint64_t)(t64));
  t65 = ((int64_t)(v3));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t67 = ((t65) >= (t66));
  t68 = t67;
  if (t68) {
  t69 = ((int64_t)(v4));
  t70 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t68 = ((t69) >= (t70));
  }
  t71 = t68;
  t72 = ((int64_t)(((int64_t)UINT64_C(60))));
  gg_function_3(t71, t72, base + 0);
  t73 = ((int64_t)(v1));
  t74 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t75 = ((t73) == (t74));
  if (t75) {
  {
  t76 = ((int64_t)(v8));
  t77 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t78 = ((t76) != (t77));
  if (t78) {
  {
  t79 = ((int64_t)(v2));
  t80 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t81 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) - (uint64_t)(t81)))));
  t83 = ((t79) == (t82));
  if (t83) {
  {
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t84 = ((int64_t)(v17));
  t85 = ((int64_t)(v4));
  if (!(((t84) < (t85)))) goto label2;
  {
  t86 = ((uint64_t)(v6));
  t87 = ((int64_t)(v17));
  v18 = gg_function_5(t86, t87, base + 0);
  t88 = ((uint64_t)(v5));
  t89 = ((int64_t)(v17));
  v19 = gg_function_5(t88, t89, base + 0);
  t90 = ((int64_t)(v19));
  t91 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t92 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) - (uint64_t)(t92)))));
  t94 = ((t90) != (t93));
  if (t94) {
  {
  v20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v22 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t95 = ((int64_t)(v22));
  t96 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  if (!(((t95) < (t96)))) goto label5;
  {
  t97 = ((int64_t)(v18));
  t98 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t99 = ((int64_t)(v22));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) << ((uint64_t)(t99) & 63)))));
  t101 = ((int64_t)(((t97) & (t100))));
  t102 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t103 = ((t101) != (t102));
  if (t103) {
  {
  t104 = ((uint64_t)(v9));
  t105 = ((int64_t)(v22));
  v23 = gg_function_5(t104, t105, base + 0);
  t106 = ((int64_t)(v20));
  t107 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t108 = ((t106) != (t107));
  if (t108) {
  {
  v21 = ((int64_t)(v23));
  v20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t109 = ((int64_t)(v21));
  t110 = ((int64_t)(v23));
  t111 = ((t109) != (t110));
  if (t111) {
  {
  t112 = ((int64_t)(v0));
  t113 = ((uint64_t)(v16));
  t114 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  gg_function_7(t112, t113, t114, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t115 = ((int64_t)(v22));
  t116 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  }
  goto label4;
label5:;
  }
  }
  t117 = ((int64_t)(v17));
  t118 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t117) + (uint64_t)(t118)))));
  }
  goto label1;
label2:;
  }
  }
  }
  }
  t119 = ((int64_t)(v10));
  t120 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t121 = ((t119) != (t120));
  if (t121) {
  {
  v24 = ((int64_t)(((int64_t)UINT64_C(0))));
  v25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t123 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t122) - (uint64_t)(t123)))));
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t124 = ((int64_t)(v27));
  t125 = ((int64_t)(v4));
  if (!(((t124) < (t125)))) goto label8;
  {
  t126 = ((uint64_t)(v5));
  t127 = ((int64_t)(v27));
  v28 = gg_function_5(t126, t127, base + 0);
  t128 = ((uint64_t)(v6));
  t129 = ((int64_t)(v27));
  v29 = gg_function_5(t128, t129, base + 0);
  v30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t130 = ((int64_t)(v2));
  t131 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t132 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t133 = ((int64_t)(((int64_t)((uint64_t)(t131) - (uint64_t)(t132)))));
  t134 = ((t130) == (t133));
  if (t134) {
  {
  t135 = ((int64_t)(v28));
  t136 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t137 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t136) - (uint64_t)(t137)))));
  t139 = ((t135) != (t138));
  if (t139) {
  {
  v30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  } else {
  {
  t140 = ((int64_t)(v29));
  t141 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t142 = ((int64_t)(v2));
  t143 = ((int64_t)(((int64_t)((uint64_t)(t141) << ((uint64_t)(t142) & 63)))));
  t144 = ((int64_t)(((t140) & (t143))));
  t145 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t146 = ((t144) != (t145));
  if (t146) {
  {
  v30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  }
  t147 = ((int64_t)(v30));
  t148 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t149 = ((t147) != (t148));
  if (t149) {
  {
  t150 = ((int64_t)(v26));
  t151 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t152 = ((t150) < (t151));
  if (t152) {
  {
  v26 = ((int64_t)(v27));
  }
  }
  }
  } else {
  {
  t153 = ((int64_t)(v26));
  t154 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t155 = ((t153) >= (t154));
  if (t155) {
  {
  t156 = ((int64_t)(v24));
  t157 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t156) + (uint64_t)(t157)))));
  t158 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t159 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t158) - (uint64_t)(t159)))));
  }
  }
  }
  }
  t160 = ((int64_t)(v27));
  t161 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t160) + (uint64_t)(t161)))));
  }
  goto label7;
label8:;
  t162 = ((int64_t)(v26));
  t163 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t164 = ((t162) >= (t163));
  if (t164) {
  {
  t165 = ((int64_t)(v24));
  t166 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t165) + (uint64_t)(t166)))));
  }
  }
  t167 = ((int64_t)(v24));
  t168 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t169 = ((t167) > (t168));
  if (t169) {
  {
  t170 = ((uint64_t)(v15));
  t171 = ((uint64_t)(v16));
  t172 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t173 = ((int64_t)(v24));
  t174 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_9(t170, t171, t172, t173, t174, base + 0);
  t175 = ((int64_t)(v0));
  t176 = ((uint64_t)(v16));
  t177 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  gg_function_7(t175, t176, t177, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  }
  }
  v31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t178 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t179 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t178) - (uint64_t)(t179)))));
  v33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v34 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v36 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t180 = ((int64_t)(v36));
  t181 = ((int64_t)(v4));
  if (!(((t180) < (t181)))) goto label11;
  {
  t182 = ((uint64_t)(v5));
  t183 = ((int64_t)(v36));
  v37 = gg_function_5(t182, t183, base + 0);
  t184 = ((uint64_t)(v6));
  t185 = ((int64_t)(v36));
  v38 = gg_function_5(t184, t185, base + 0);
  v39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t186 = ((int64_t)(v2));
  t187 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t188 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t189 = ((int64_t)(((int64_t)((uint64_t)(t187) - (uint64_t)(t188)))));
  t190 = ((t186) == (t189));
  if (t190) {
  {
  t191 = ((int64_t)(v37));
  t192 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t193 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t194 = ((int64_t)(((int64_t)((uint64_t)(t192) - (uint64_t)(t193)))));
  t195 = ((t191) != (t194));
  if (t195) {
  {
  v39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  } else {
  {
  t196 = ((int64_t)(v38));
  t197 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t198 = ((int64_t)(v2));
  t199 = ((int64_t)(((int64_t)((uint64_t)(t197) << ((uint64_t)(t198) & 63)))));
  t200 = ((int64_t)(((t196) & (t199))));
  t201 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t202 = ((t200) != (t201));
  if (t202) {
  {
  v39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  }
  t203 = ((int64_t)(v39));
  t204 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t205 = ((t203) != (t204));
  if (t205) {
  {
  t206 = ((int64_t)(v31));
  t207 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t206) + (uint64_t)(t207)))));
  v40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t208 = ((int64_t)(v8));
  t209 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t210 = ((t208) != (t209));
  if (t210) {
  {
  t211 = ((int64_t)(v2));
  t212 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t213 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t214 = ((int64_t)(((int64_t)((uint64_t)(t212) - (uint64_t)(t213)))));
  t215 = ((t211) != (t214));
  if (t215) {
  {
  t216 = ((uint64_t)(v9));
  t217 = ((int64_t)(v2));
  v40 = gg_function_5(t216, t217, base + 0);
  }
  } else {
  {
  v41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v42 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t218 = ((int64_t)(v42));
  t219 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  if (!(((t218) < (t219)))) goto label14;
  {
  t220 = ((int64_t)(v38));
  t221 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t222 = ((int64_t)(v42));
  t223 = ((int64_t)(((int64_t)((uint64_t)(t221) << ((uint64_t)(t222) & 63)))));
  t224 = ((int64_t)(((t220) & (t223))));
  t225 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t226 = ((t224) != (t225));
  if (t226) {
  {
  t227 = ((uint64_t)(v9));
  t228 = ((int64_t)(v42));
  v43 = gg_function_5(t227, t228, base + 0);
  t229 = ((int64_t)(v41));
  t230 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t231 = ((t229) != (t230));
  if (t231) {
  {
  v40 = ((int64_t)(v43));
  v41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t232 = ((int64_t)(v40));
  t233 = ((int64_t)(v43));
  t234 = ((t232) != (t233));
  if (t234) {
  {
  t235 = ((int64_t)(v0));
  t236 = ((uint64_t)(v16));
  t237 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  gg_function_7(t235, t236, t237, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t238 = ((int64_t)(v42));
  t239 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v42 = ((int64_t)(((int64_t)((uint64_t)(t238) + (uint64_t)(t239)))));
  }
  goto label13;
label14:;
  }
  }
  }
  }
  t240 = ((uint64_t)(v7));
  t241 = ((int64_t)(v36));
  v44 = gg_function_5(t240, t241, base + 0);
  v45 = ((int64_t)(v36));
  t242 = ((int64_t)(v44));
  t243 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t244 = ((t242) >= (t243));
  if (t244) {
  {
  v45 = ((int64_t)(v44));
  }
  }
  t245 = ((int64_t)(v40));
  t246 = ((int64_t)(v3));
  t247 = ((int64_t)(((int64_t)((uint64_t)(t245) * (uint64_t)(t246)))));
  t248 = ((int64_t)(v45));
  v46 = ((int64_t)(((int64_t)((uint64_t)(t247) + (uint64_t)(t248)))));
  t249 = ((int64_t)(v32));
  t250 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t251 = ((t249) < (t250));
  if (t251) {
  {
  v32 = ((int64_t)(v36));
  }
  }
  }
  } else {
  {
  t252 = ((int64_t)(v32));
  t253 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t254 = ((t252) >= (t253));
  if (t254) {
  {
  t255 = ((uint64_t)(v15));
  t256 = ((uint64_t)(v16));
  t257 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t258 = ((int64_t)(v32));
  t259 = ((int64_t)(v36));
  gg_function_9(t255, t256, t257, t258, t259, base + 0);
  t260 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t261 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t260) - (uint64_t)(t261)))));
  }
  }
  }
  }
  t262 = ((int64_t)(v36));
  t263 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t262) + (uint64_t)(t263)))));
  }
  goto label10;
label11:;
  t264 = ((int64_t)(v32));
  t265 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t266 = ((t264) >= (t265));
  if (t266) {
  {
  t267 = ((uint64_t)(v15));
  t268 = ((uint64_t)(v16));
  t269 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t270 = ((int64_t)(v32));
  t271 = ((int64_t)(v4));
  gg_function_9(t267, t268, t269, t270, t271, base + 0);
  }
  }
  v47 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t272 = ((int64_t)(v47));
  t273 = ((int64_t)(v4));
  if (!(((t272) < (t273)))) goto label17;
  {
  t274 = ((uint64_t)(v5));
  t275 = ((int64_t)(v47));
  v48 = gg_function_5(t274, t275, base + 0);
  t276 = ((uint64_t)(v6));
  t277 = ((int64_t)(v47));
  v49 = gg_function_5(t276, t277, base + 0);
  v50 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t278 = ((int64_t)(v2));
  t279 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t280 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t281 = ((int64_t)(((int64_t)((uint64_t)(t279) - (uint64_t)(t280)))));
  t282 = ((t278) == (t281));
  if (t282) {
  {
  t283 = ((int64_t)(v48));
  t284 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t285 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t286 = ((int64_t)(((int64_t)((uint64_t)(t284) - (uint64_t)(t285)))));
  t287 = ((t283) != (t286));
  if (t287) {
  {
  v50 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  } else {
  {
  t288 = ((int64_t)(v49));
  t289 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t290 = ((int64_t)(v2));
  t291 = ((int64_t)(((int64_t)((uint64_t)(t289) << ((uint64_t)(t290) & 63)))));
  t292 = ((int64_t)(((t288) & (t291))));
  t293 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t294 = ((t292) != (t293));
  if (t294) {
  {
  v50 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  }
  t295 = ((int64_t)(v50));
  t296 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t297 = ((t295) != (t296));
  if (t297) {
  {
  v51 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t298 = ((int64_t)(v8));
  t299 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t300 = ((t298) != (t299));
  if (t300) {
  {
  t301 = ((int64_t)(v2));
  t302 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t303 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t304 = ((int64_t)(((int64_t)((uint64_t)(t302) - (uint64_t)(t303)))));
  t305 = ((t301) != (t304));
  if (t305) {
  {
  t306 = ((uint64_t)(v9));
  t307 = ((int64_t)(v2));
  v51 = gg_function_5(t306, t307, base + 0);
  }
  } else {
  {
  v52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v53 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t308 = ((int64_t)(v53));
  t309 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  if (!(((t308) < (t309)))) goto label20;
  {
  t310 = ((int64_t)(v49));
  t311 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t312 = ((int64_t)(v53));
  t313 = ((int64_t)(((int64_t)((uint64_t)(t311) << ((uint64_t)(t312) & 63)))));
  t314 = ((int64_t)(((t310) & (t313))));
  t315 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t316 = ((t314) != (t315));
  if (t316) {
  {
  t317 = ((uint64_t)(v9));
  t318 = ((int64_t)(v53));
  v54 = gg_function_5(t317, t318, base + 0);
  t319 = ((int64_t)(v52));
  t320 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t321 = ((t319) != (t320));
  if (t321) {
  {
  v51 = ((int64_t)(v54));
  v52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t322 = ((int64_t)(v51));
  t323 = ((int64_t)(v54));
  t324 = ((t322) != (t323));
  if (t324) {
  {
  t325 = ((int64_t)(v0));
  t326 = ((uint64_t)(v16));
  t327 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  gg_function_7(t325, t326, t327, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t328 = ((int64_t)(v53));
  t329 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v53 = ((int64_t)(((int64_t)((uint64_t)(t328) + (uint64_t)(t329)))));
  }
  goto label19;
label20:;
  }
  }
  }
  }
  t330 = ((uint64_t)(v7));
  t331 = ((int64_t)(v47));
  v55 = gg_function_5(t330, t331, base + 0);
  v56 = ((int64_t)(v47));
  t332 = ((int64_t)(v55));
  t333 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t334 = ((t332) >= (t333));
  if (t334) {
  {
  v56 = ((int64_t)(v55));
  }
  }
  t335 = ((int64_t)(v51));
  t336 = ((int64_t)(v3));
  t337 = ((int64_t)(((int64_t)((uint64_t)(t335) * (uint64_t)(t336)))));
  t338 = ((int64_t)(v56));
  v57 = ((int64_t)(((int64_t)((uint64_t)(t337) + (uint64_t)(t338)))));
  t339 = ((int64_t)(v35));
  t340 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t341 = ((t339) == (t340));
  if (t341) {
  {
  v33 = ((int64_t)(v57));
  t342 = ((int64_t)(v57));
  t343 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t342) + (uint64_t)(t343)))));
  v35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t344 = ((int64_t)(v34));
  t345 = ((int64_t)(v57));
  t346 = ((t344) == (t345));
  if (t346) {
  {
  t347 = ((int64_t)(v57));
  t348 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t347) + (uint64_t)(t348)))));
  }
  } else {
  {
  t349 = ((uint64_t)(v15));
  t350 = ((uint64_t)(v16));
  t351 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t352 = ((int64_t)(v33));
  t353 = ((int64_t)(v34));
  gg_function_9(t349, t350, t351, t352, t353, base + 0);
  v33 = ((int64_t)(v57));
  t354 = ((int64_t)(v57));
  t355 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t354) + (uint64_t)(t355)))));
  }
  }
  }
  }
  }
  }
  t356 = ((int64_t)(v47));
  t357 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v47 = ((int64_t)(((int64_t)((uint64_t)(t356) + (uint64_t)(t357)))));
  }
  goto label16;
label17:;
  t358 = ((int64_t)(v35));
  t359 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t360 = ((t358) != (t359));
  if (t360) {
  {
  t361 = ((uint64_t)(v15));
  t362 = ((uint64_t)(v16));
  t363 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t364 = ((int64_t)(v33));
  t365 = ((int64_t)(v34));
  gg_function_9(t361, t362, t363, t364, t365, base + 0);
  }
  }
  t366 = ((uint64_t)(v15));
  t367 = ((uint64_t)(v16));
  t368 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t369 = ((int64_t)(v31));
  t370 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_9(t366, t367, t368, t369, t370, base + 0);
  t371 = ((uint64_t)(v15));
  t372 = ((uint64_t)(v16));
  t373 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t374 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t375 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_9(t371, t372, t373, t374, t375, base + 0);
  t376 = ((uint64_t)(v15));
  t377 = ((uint64_t)(v16));
  t378 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t379 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t380 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_9(t376, t377, t378, t379, t380, base + 0);
  t381 = ((int64_t)(v0));
  t382 = ((int64_t)(((int64_t)UINT64_C(2))));
  t383 = ((int64_t)(((int64_t)UINT64_C(0))));
  t384 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern void (fg346k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg346k_put(t381, t382, t383, t384);
  t385 = ((int64_t)(v0));
  t386 = ((int64_t)(((int64_t)UINT64_C(2))));
  t387 = ((int64_t)(((int64_t)UINT64_C(1))));
  t388 = ((uint64_t)(((uint64_t)(((int64_t)(((int64_t)UINT64_C(0))))))));
#ifndef __cplusplus
  extern void (fg346k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg346k_put(t385, t386, t387, t388);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  } else {
  {
  v58 = ((int64_t)(v11));
  t389 = ((int64_t)(v58));
  t390 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t391 = ((t389) != (t390));
  if (t391) {
  {
  t392 = ((uint64_t)(v15));
  t393 = ((uint64_t)(v16));
  t394 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t395 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t396 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_9(t392, t393, t394, t395, t396, base + 0);
  }
  }
  t397 = ((uint64_t)(v15));
  t398 = ((uint64_t)(v16));
  t399 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t400 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t401 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_9(t397, t398, t399, t400, t401, base + 0);
  t402 = ((int64_t)(v12));
  t403 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t404 = ((t402) != (t403));
  if (t404) {
  {
  v58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t405 = ((int64_t)(v58));
  t406 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t407 = ((t405) == (t406));
  if (t407) {
  {
  v59 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t408 = ((int64_t)(v11));
  t409 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t410 = ((t408) != (t409));
  if (t410) {
  {
  v59 = ((int64_t)(v14));
  }
  }
  t411 = ((uint64_t)(v15));
  t412 = ((uint64_t)(v16));
  t413 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t414 = ((int64_t)(v13));
  t415 = ((int64_t)(v59));
  gg_function_9(t411, t412, t413, t414, t415, base + 0);
  t416 = ((int64_t)(v0));
  t417 = ((uint64_t)(v16));
  t418 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  gg_function_7(t416, t417, t418, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t419 = ((int64_t)(v8));
  t420 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t421 = ((t419) != (t420));
  if (t421) {
  {
  t422 = ((uint64_t)(v15));
  t423 = ((uint64_t)(v16));
  t424 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  t425 = ((int64_t)(v2));
  t426 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  gg_function_9(t422, t423, t424, t425, t426, base + 0);
  }
  }
  t427 = ((int64_t)(v0));
  t428 = ((int64_t)(((int64_t)UINT64_C(2))));
  t429 = ((int64_t)(((int64_t)UINT64_C(0))));
  t430 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern void (fg346k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg346k_put(t427, t428, t429, t430);
  t431 = ((int64_t)(v0));
  t432 = ((int64_t)(((int64_t)UINT64_C(2))));
  t433 = ((int64_t)(((int64_t)UINT64_C(1))));
  t434 = ((uint64_t)(((uint64_t)(((int64_t)(((int64_t)UINT64_C(0))))))));
#ifndef __cplusplus
  extern void (fg346k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg346k_put(t431, t432, t433, t434);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
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
