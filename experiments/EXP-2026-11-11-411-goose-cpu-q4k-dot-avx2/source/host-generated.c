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


#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_6;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_7;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int64_t data[8]; } gg_type_8;
#pragma pack(pop)
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
static uint8_t gg_eq_5(double a, double b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, double value);
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, gg_type_6 value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_10(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, uint8_t value);
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
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(float a, float b) {
return a == b;
}
static void gg_render_4(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(double a, double b) {
return a == b;
}
static void gg_render_5(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_type_6*)a.data)[i],((gg_type_6*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b) {
return gg_equal_elements_4(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_6(gg_reference out, gg_type_6 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_4(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_4(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_4(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
return gg_equal_elements_1(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
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
static uint8_t gg_function_1(int64_t p0, int64_t base);
static void gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static void gg_function_9(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_9(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)UINT64_C(4));
  t0 = 0;
label0:;
  if (t0 >= t1) goto label2;
  v3 = t0;
  {
  t2 = ((uint64_t)(v0));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)(v3))));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t4)))));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((int64_t)UINT64_C(8))));
  t9 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))))));
  t10 = ((int64_t)(((t6) >> ((uint64_t)(t9) & 63))));
  t11 = ((int64_t)(((int64_t)UINT64_C(255))));
  t12 = ((int64_t)(((t10) & (t11))));
#ifndef __cplusplus
  extern void (fg410k_stb)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stb(t2, t5, t12);
  }
label1:;
  t0++; goto label0;
label2:;
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
  extern void (fg410k_fail)(int64_t);
#endif
  fg410k_fail(t2);
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
  uint8_t t19;
  int64_t t20;
  float t21;
  float t22;
  int64_t t23;
  float t24;
  float t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  float * t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  float * t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  int64_t t42;
  int64_t t43;
  uint64_t t44;
  uint64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  float t49;
  float t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  uint64_t t55;
  int64_t t56;
  float t57;
  float t58;
  uint64_t t59;
  int64_t t60;
  int64_t t61;
  uint64_t t62;
  uint64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  float t67;
  float t68;
  uint64_t t69;
  int64_t t70;
  int64_t t71;
  uint64_t t72;
  uint64_t t73;
  int64_t t74;
  float t75;
  float t76;
  uint64_t t77;
  int64_t t78;
  int64_t t79;
  uint64_t t80;
  uint64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  int64_t t85;
  int64_t t86;
  uint64_t t87;
  uint64_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  int64_t t92;
  int64_t t93;
  uint64_t t94;
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
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint64_t t124;
  int64_t t125;
  int64_t t126;
  uint64_t t127;
  int64_t t128;
  int64_t t129;
  uint64_t t130;
  int64_t t131;
  int64_t t132;
  uint64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  uint64_t t138;
  int64_t t139;
  int64_t t140;
  uint64_t t141;
  uint64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  uint64_t t149;
  int64_t t150;
  int64_t t151;
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
  uint8_t t162;
  uint64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  uint64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  uint64_t t183;
  int64_t t184;
  int64_t t185;
  uint64_t t186;
  uint64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
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
  int64_t t204;
  int64_t t205;
  int64_t t206;
  uint64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
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
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t * t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  uint64_t t249;
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
  uint64_t t261;
  int64_t t262;
  int64_t t263;
  uint64_t t264;
  uint64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t * t273;
  int64_t t274;
  int64_t * t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  float * t289;
  float t290;
  int64_t t291;
  int64_t * t292;
  float t293;
  int64_t t294;
  float * t295;
  float t296;
  float t297;
  int64_t t298;
  int64_t t299;
  uint8_t t300;
  uint64_t t301;
  int64_t t302;
  int64_t t303;
  int64_t * t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  float t308;
  float t309;
  int64_t t310;
  int64_t t311;
  uint64_t t312;
  int64_t t313;
  int64_t t314;
  uint64_t t315;
  uint64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  uint64_t t323;
  int64_t t324;
  int64_t t325;
  uint64_t t326;
  uint64_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  uint64_t t334;
  int64_t t335;
  int64_t t336;
  uint64_t t337;
  uint64_t t338;
  int64_t t339;
  int64_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  uint64_t t345;
  int64_t t346;
  int64_t t347;
  uint64_t t348;
  uint64_t t349;
  int64_t t350;
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
  int64_t t370;
  int64_t t371;
  int64_t t372;
  int64_t t373;
  int64_t t374;
  int64_t t375;
  int64_t t376;
  int64_t t377;
  int64_t t378;
  int64_t t379;
  int64_t t380;
  float * t381;
  float t382;
  float t383;
  int64_t t384;
  float * t385;
  float t386;
  float t387;
  int64_t t388;
  int64_t t389;
  uint64_t t390;
  int64_t t391;
  int64_t t392;
  int64_t t393;
  int64_t t394;
  int64_t t395;
  float t396;
  int64_t t397;
  int64_t t398;
  uint64_t t399;
  int64_t t400;
  int64_t t401;
  int64_t t402;
  int64_t t403;
  int64_t t404;
  float t405;
  int64_t t406;
  int64_t t407;
  uint64_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  int64_t t413;
  int64_t t414;
  uint64_t t415;
  int64_t t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  int64_t t421;
  float * t422;
  float t423;
  int64_t t424;
  int64_t t425;
  uint64_t t426;
  int64_t t427;
  float t428;
  int64_t t429;
  int64_t t430;
  uint64_t t431;
  int64_t t432;
  int64_t t433;
  uint64_t t434;
  int64_t t435;
  int64_t t436;
  float * t437;
  float t438;
  int64_t t439;
  int64_t t440;
  int64_t t441;
  int64_t t442;
  int64_t t443;
  float * t444;
  float t445;
  int64_t t446;
  float * t447;
  float t448;
  int64_t t449;
  float * t450;
  float t451;
  int64_t t452;
  float * t453;
  float t454;
  int64_t t455;
  float * t456;
  float t457;
  int64_t t458;
  float * t459;
  float t460;
  int64_t t461;
  float * t462;
  float t463;
  int64_t t464;
  float * t465;
  float t466;
  float t467;
  float t468;
  float t469;
  float t470;
  float t471;
  float t472;
  int64_t t473;
  float * t474;
  float t475;
  int64_t t476;
  float * t477;
  float t478;
  float t479;
  int64_t t480;
  float * t481;
  float t482;
  int64_t t483;
  float * t484;
  float t485;
  float t486;
  uint64_t t487;
  int64_t t488;
  float t489;
  int64_t t490;
  int64_t t491;
  uint64_t t492;
  int64_t t493;
  float t494;
  int64_t t495;
  int64_t t496;
  float t497;
  float t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  float t502;
  int64_t t503;
  uint64_t t504;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  float v5;
  _Alignas(float) gg_type_6 v6;
  _Alignas(float) gg_type_7 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  float v12;
  float v13;
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
  int64_t v25;
  _Alignas(int64_t) gg_type_8 v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  float v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
  float v42;
  float v43;
  float v44;
  float v45;
  float v46;
  float v47;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg410k_get)(int64_t, int64_t, int64_t);
#endif
  t4 = fg410k_get(t1, t2, t3);
  v1 = ((int64_t)(((int64_t)(t4))));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg410k_get)(int64_t, int64_t, int64_t);
#endif
  t8 = fg410k_get(t5, t6, t7);
  v2 = ((uint64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg410k_get)(int64_t, int64_t, int64_t);
#endif
  t12 = fg410k_get(t9, t10, t11);
  v3 = ((uint64_t)(t12));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg410k_get)(int64_t, int64_t, int64_t);
#endif
  t16 = fg410k_get(t13, t14, t15);
  v4 = ((uint64_t)(t16));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) > (t18));
  t20 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_3(t19, t20, base + 0);
  v5 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  memset(&t21, 0, sizeof(t21));
  t22 = ((float)(t21));
  for (t23=0; t23<8; t23++) (v6).data[t23] = t22;
  memset(&t24, 0, sizeof(t24));
  t25 = ((float)(t24));
  for (t26=0; t26<4; t26++) (v7).data[t26] = t25;
  v8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t27 = ((int64_t)(v8));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t27) < (t28)))) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t29 = v8;
  t30 = ((float *)(v7).data) + gg_index(t29, ((int64_t)UINT64_C(4)));
  (*t30) = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t31 = ((int64_t)(v8));
  t32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  }
  goto label1;
label2:;
  v9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label4:;
  t33 = ((int64_t)(v10));
  t34 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t33) < (t34)))) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t35 = v10;
  t36 = ((float *)(v6).data) + gg_index(t35, ((int64_t)UINT64_C(8)));
  (*t36) = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t37 = ((int64_t)(v10));
  t38 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  }
  goto label4;
label5:;
  v11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label7:;
  t39 = ((int64_t)(v11));
  t40 = ((int64_t)(v1));
  if (!(((t39) < (t40)))) goto label8;
  {
  t41 = ((uint64_t)(v2));
  t42 = ((int64_t)(v11));
  t43 = ((int64_t)(((int64_t)UINT64_C(144))));
  t44 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))))));
  t45 = ((uint64_t)(((uint64_t)((uint64_t)(t41) + (uint64_t)(t44)))));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern int64_t (fg410k_ld_i64)(uint64_t, int64_t);
#endif
  t47 = fg410k_ld_i64(t45, t46);
  t48 = ((int64_t)(t47));
#ifndef __cplusplus
  extern float (fg410k_f16f32)(int64_t);
#endif
  t49 = fg410k_f16f32(t48);
  t50 = ((float)(t49));
  t51 = ((uint64_t)(v3));
  t52 = ((int64_t)(v11));
  t53 = ((int64_t)(((int64_t)UINT64_C(292))));
  t54 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))))));
  t55 = ((uint64_t)(((uint64_t)((uint64_t)(t51) + (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern float (fg410k_ldf)(uint64_t, int64_t);
#endif
  t57 = fg410k_ldf(t55, t56);
  t58 = ((float)(t57));
  v12 = ((float)(((t50) * (t58))));
  t59 = ((uint64_t)(v2));
  t60 = ((int64_t)(v11));
  t61 = ((int64_t)(((int64_t)UINT64_C(144))));
  t62 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))))));
  t63 = ((uint64_t)(((uint64_t)((uint64_t)(t59) + (uint64_t)(t62)))));
  t64 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
#ifndef __cplusplus
  extern int64_t (fg410k_ld_i64)(uint64_t, int64_t);
#endif
  t65 = fg410k_ld_i64(t63, t64);
  t66 = ((int64_t)(t65));
#ifndef __cplusplus
  extern float (fg410k_f16f32)(int64_t);
#endif
  t67 = fg410k_f16f32(t66);
  t68 = ((float)(t67));
  t69 = ((uint64_t)(v3));
  t70 = ((int64_t)(v11));
  t71 = ((int64_t)(((int64_t)UINT64_C(292))));
  t72 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))))));
  t73 = ((uint64_t)(((uint64_t)((uint64_t)(t69) + (uint64_t)(t72)))));
  t74 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern float (fg410k_ldf)(uint64_t, int64_t);
#endif
  t75 = fg410k_ldf(t73, t74);
  t76 = ((float)(t75));
  v13 = ((float)(((t68) * (t76))));
  t77 = ((uint64_t)(v2));
  t78 = ((int64_t)(v11));
  t79 = ((int64_t)(((int64_t)UINT64_C(144))));
  t80 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))))));
  t81 = ((uint64_t)(((uint64_t)((uint64_t)(t77) + (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldu32)(uint64_t, int64_t);
#endif
  t83 = fg410k_ldu32(t81, t82);
  v14 = ((int64_t)(t83));
  t84 = ((uint64_t)(v2));
  t85 = ((int64_t)(v11));
  t86 = ((int64_t)(((int64_t)UINT64_C(144))));
  t87 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t86)))))));
  t88 = ((uint64_t)(((uint64_t)((uint64_t)(t84) + (uint64_t)(t87)))));
  t89 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldu32)(uint64_t, int64_t);
#endif
  t90 = fg410k_ldu32(t88, t89);
  v15 = ((int64_t)(t90));
  t91 = ((uint64_t)(v2));
  t92 = ((int64_t)(v11));
  t93 = ((int64_t)(((int64_t)UINT64_C(144))));
  t94 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t92) * (uint64_t)(t93)))))));
  t95 = ((uint64_t)(((uint64_t)((uint64_t)(t91) + (uint64_t)(t94)))));
  t96 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldu32)(uint64_t, int64_t);
#endif
  t97 = fg410k_ldu32(t95, t96);
  v16 = ((int64_t)(t97));
  t98 = ((int64_t)(v16));
  t99 = ((int64_t)(((int64_t)UINT64_C(4))));
  t100 = ((int64_t)(((t98) >> ((uint64_t)(t99) & 63))));
  t101 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(252645135))))));
  t102 = ((int64_t)(((t100) & (t101))));
  t103 = ((int64_t)(v15));
  t104 = ((int64_t)(((int64_t)UINT64_C(6))));
  t105 = ((int64_t)(((t103) >> ((uint64_t)(t104) & 63))));
  t106 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(50529027))))));
  t107 = ((int64_t)(((t105) & (t106))));
  t108 = ((int64_t)(((int64_t)UINT64_C(4))));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t107) << ((uint64_t)(t108) & 63)))));
  v17 = ((int64_t)(((t102) | (t109))));
  t110 = ((int64_t)(v15));
  t111 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1061109567))))));
  v18 = ((int64_t)(((t110) & (t111))));
  t112 = ((int64_t)(v16));
  t113 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(252645135))))));
  t114 = ((int64_t)(((t112) & (t113))));
  t115 = ((int64_t)(v14));
  t116 = ((int64_t)(((int64_t)UINT64_C(6))));
  t117 = ((int64_t)(((t115) >> ((uint64_t)(t116) & 63))));
  t118 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(50529027))))));
  t119 = ((int64_t)(((t117) & (t118))));
  t120 = ((int64_t)(((int64_t)UINT64_C(4))));
  t121 = ((int64_t)(((int64_t)((uint64_t)(t119) << ((uint64_t)(t120) & 63)))));
  v15 = ((int64_t)(((t114) | (t121))));
  v16 = ((int64_t)(v18));
  t122 = ((int64_t)(v14));
  t123 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1061109567))))));
  v14 = ((int64_t)(((t122) & (t123))));
  t124 = ((uint64_t)(v4));
  t125 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t126 = ((int64_t)(v14));
  gg_function_9(t124, t125, t126, base + 0);
  t127 = ((uint64_t)(v4));
  t128 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t129 = ((int64_t)(v15));
  gg_function_9(t127, t128, t129, base + 0);
  t130 = ((uint64_t)(v4));
  t131 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t132 = ((int64_t)(v16));
  gg_function_9(t130, t131, t132, base + 0);
  t133 = ((uint64_t)(v4));
  t134 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  t135 = ((int64_t)(v17));
  gg_function_9(t133, t134, t135, base + 0);
  v19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label10:;
  t136 = ((int64_t)(v20));
  t137 = ((int64_t)(((int64_t)UINT64_C(16))));
  if (!(((t136) < (t137)))) goto label11;
  {
  t138 = ((uint64_t)(v3));
  t139 = ((int64_t)(v11));
  t140 = ((int64_t)(((int64_t)UINT64_C(292))));
  t141 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t139) * (uint64_t)(t140)))))));
  t142 = ((uint64_t)(((uint64_t)((uint64_t)(t138) + (uint64_t)(t141)))));
  t143 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(260))))));
  t144 = ((int64_t)(v20));
  t145 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t145)))));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t143) + (uint64_t)(t146)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldi16)(uint64_t, int64_t);
#endif
  t148 = fg410k_ldi16(t142, t147);
  v21 = ((int64_t)(t148));
  t149 = ((uint64_t)(v4));
  t150 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t151 = ((int64_t)(v20));
  t152 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t153 = ((int64_t)(((int64_t)gg_div(t151, t152))));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t150) + (uint64_t)(t153)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldu8)(uint64_t, int64_t);
#endif
  t155 = fg410k_ldu8(t149, t154);
  v22 = ((int64_t)(((int64_t)(t155))));
  t156 = ((int64_t)(v19));
  t157 = ((int64_t)(v21));
  t158 = ((int64_t)(v22));
  t159 = ((int64_t)(((int64_t)((uint64_t)(t157) * (uint64_t)(t158)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t156) + (uint64_t)(t159)))));
  t160 = ((int64_t)(v20));
  t161 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t162 = ((t160) < (t161));
  if (t162) {
  {
  t163 = ((uint64_t)(v4));
  t164 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1400))))));
  t165 = ((int64_t)(v20));
  t166 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t165) * (uint64_t)(t166)))));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t167)))));
  t169 = ((int64_t)(v21));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t163, t168, t169);
  t170 = ((uint64_t)(v4));
  t171 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1408))))));
  t172 = ((int64_t)(v20));
  t173 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t172) * (uint64_t)(t173)))));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t171) + (uint64_t)(t174)))));
  t176 = ((int64_t)(v22));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t170, t175, t176);
  }
  }
  t177 = ((int64_t)(v20));
  t178 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t177) + (uint64_t)(t178)))));
  }
  goto label10;
label11:;
  v23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label13:;
  t179 = ((int64_t)(v23));
  t180 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t179) < (t180)))) goto label14;
  {
  v24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label16:;
  t181 = ((int64_t)(v24));
  t182 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t181) < (t182)))) goto label17;
  {
  t183 = ((uint64_t)(v2));
  t184 = ((int64_t)(v11));
  t185 = ((int64_t)(((int64_t)UINT64_C(144))));
  t186 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t184) * (uint64_t)(t185)))))));
  t187 = ((uint64_t)(((uint64_t)((uint64_t)(t183) + (uint64_t)(t186)))));
  t188 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t189 = ((int64_t)(v23));
  t190 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t191 = ((int64_t)(((int64_t)((uint64_t)(t189) * (uint64_t)(t190)))));
  t192 = ((int64_t)(((int64_t)((uint64_t)(t188) + (uint64_t)(t191)))));
  t193 = ((int64_t)(((int64_t)(v24))));
  t194 = ((int64_t)(((int64_t)((uint64_t)(t192) + (uint64_t)(t193)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldu8)(uint64_t, int64_t);
#endif
  t195 = fg410k_ldu8(t187, t194);
  v25 = ((int64_t)(t195));
  t196 = ((uint64_t)(v4));
  t197 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t198 = ((int64_t)(v23));
  t199 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t200 = ((int64_t)(((int64_t)((uint64_t)(t198) * (uint64_t)(t199)))));
  t201 = ((int64_t)(((int64_t)((uint64_t)(t197) + (uint64_t)(t200)))));
  t202 = ((int64_t)(((int64_t)(v24))));
  t203 = ((int64_t)(((int64_t)((uint64_t)(t201) + (uint64_t)(t202)))));
  t204 = ((int64_t)(v25));
  t205 = ((int64_t)(((int64_t)UINT64_C(15))));
  t206 = ((int64_t)(((t204) & (t205))));
#ifndef __cplusplus
  extern void (fg410k_stb)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stb(t196, t203, t206);
  t207 = ((uint64_t)(v4));
  t208 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t209 = ((int64_t)(v23));
  t210 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t211 = ((int64_t)(((int64_t)((uint64_t)(t209) * (uint64_t)(t210)))));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t208) + (uint64_t)(t211)))));
  t213 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t214 = ((int64_t)(((int64_t)((uint64_t)(t212) + (uint64_t)(t213)))));
  t215 = ((int64_t)(((int64_t)(v24))));
  t216 = ((int64_t)(((int64_t)((uint64_t)(t214) + (uint64_t)(t215)))));
  t217 = ((int64_t)(v25));
  t218 = ((int64_t)(((int64_t)UINT64_C(4))));
  t219 = ((int64_t)(((t217) >> ((uint64_t)(t218) & 63))));
  t220 = ((int64_t)(((int64_t)UINT64_C(15))));
  t221 = ((int64_t)(((t219) & (t220))));
#ifndef __cplusplus
  extern void (fg410k_stb)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stb(t207, t216, t221);
  t222 = ((int64_t)(v24));
  t223 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t222) + (uint64_t)(t223)))));
  }
  goto label16;
label17:;
  t224 = ((int64_t)(v23));
  t225 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t224) + (uint64_t)(t225)))));
  }
  goto label13;
label14:;
  memset(&t226, 0, sizeof(t226));
  t227 = ((int64_t)(t226));
  for (t228=0; t228<8; t228++) (v26).data[t228] = t227;
label19:;
  t229 = ((int64_t)(v9));
  t230 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t229) < (t230)))) goto label20;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t231 = v9;
  t232 = ((int64_t *)(v26).data) + gg_index(t231, ((int64_t)UINT64_C(8)));
  (*t232) = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t233 = ((int64_t)(v9));
  t234 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t233) + (uint64_t)(t234)))));
  }
  goto label19;
label20:;
  v20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label22:;
  t235 = ((int64_t)(v20));
  t236 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t235) < (t236)))) goto label23;
  {
  v9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label25:;
  t237 = ((int64_t)(v9));
  t238 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t237) < (t238)))) goto label26;
  {
  t239 = ((int64_t)(v20));
  t240 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t241 = ((int64_t)(((int64_t)((uint64_t)(t239) * (uint64_t)(t240)))));
  t242 = ((int64_t)(v9));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t241) + (uint64_t)(t242)))));
  t243 = ((int64_t)(v27));
  t244 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  v28 = ((int64_t)(((int64_t)gg_div(t243, t244))));
  t245 = ((int64_t)(v27));
  t246 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t247 = ((int64_t)(((int64_t)gg_mod(t245, t246))));
  t248 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  v29 = ((int64_t)(((int64_t)gg_div(t247, t248))));
  t249 = ((uint64_t)(v4));
  t250 = ((int64_t)(v28));
  t251 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t252 = ((int64_t)(((int64_t)((uint64_t)(t250) * (uint64_t)(t251)))));
  t253 = ((int64_t)(v29));
  t254 = ((int64_t)(((int64_t)((uint64_t)(t252) + (uint64_t)(t253)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldu8)(uint64_t, int64_t);
#endif
  t255 = fg410k_ldu8(t249, t254);
  v30 = ((int64_t)(((int64_t)(t255))));
  t256 = ((uint64_t)(v4));
  t257 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t258 = ((int64_t)(v27));
  t259 = ((int64_t)(((int64_t)((uint64_t)(t257) + (uint64_t)(t258)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldu8)(uint64_t, int64_t);
#endif
  t260 = fg410k_ldu8(t256, t259);
  v31 = ((int64_t)(t260));
  t261 = ((uint64_t)(v3));
  t262 = ((int64_t)(v11));
  t263 = ((int64_t)(((int64_t)UINT64_C(292))));
  t264 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t262) * (uint64_t)(t263)))))));
  t265 = ((uint64_t)(((uint64_t)((uint64_t)(t261) + (uint64_t)(t264)))));
  t266 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t267 = ((int64_t)(v27));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t266) + (uint64_t)(t267)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldi8)(uint64_t, int64_t);
#endif
  t269 = fg410k_ldi8(t265, t268);
  v32 = ((int64_t)(((int64_t)(t269))));
  t270 = ((int64_t)(v9));
  t271 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v33 = ((int64_t)(((int64_t)gg_div(t270, t271))));
  /* FG_DIRECT_SLICE_INDEX */
  t272 = v33;
  t273 = ((int64_t *)(v26).data) + gg_index(t272, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t274 = v33;
  t275 = ((int64_t *)(v26).data) + gg_index(t274, ((int64_t)UINT64_C(8)));
  t276 = ((int64_t)((*t275)));
  t277 = ((int64_t)(v30));
  t278 = ((int64_t)(v31));
  t279 = ((int64_t)(((int64_t)((uint64_t)(t277) * (uint64_t)(t278)))));
  t280 = ((int64_t)(v32));
  t281 = ((int64_t)(((int64_t)((uint64_t)(t279) * (uint64_t)(t280)))));
  (*t273) = ((int64_t)(((int64_t)((uint64_t)(t276) + (uint64_t)(t281)))));
  t282 = ((int64_t)(v9));
  t283 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t282) + (uint64_t)(t283)))));
  }
  goto label25;
label26:;
  t284 = ((int64_t)(v20));
  t285 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t284) + (uint64_t)(t285)))));
  }
  goto label22;
label23:;
  v9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label28:;
  t286 = ((int64_t)(v9));
  t287 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t286) < (t287)))) goto label29;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t288 = v9;
  t289 = ((float *)(v6).data) + gg_index(t288, ((int64_t)UINT64_C(8)));
  t290 = ((float)(v12));
  /* FG_DIRECT_SLICE_INDEX */
  t291 = v9;
  t292 = ((int64_t *)(v26).data) + gg_index(t291, ((int64_t)UINT64_C(8)));
  t293 = ((float)(((float)((*t292)))));
  /* FG_DIRECT_SLICE_INDEX */
  t294 = v9;
  t295 = ((float *)(v6).data) + gg_index(t294, ((int64_t)UINT64_C(8)));
  t296 = ((float)((*t295)));
#ifndef __cplusplus
  extern float (fg410k_fma)(float, float, float);
#endif
  t297 = fg410k_fma(t290, t293, t296);
  (*t289) = ((float)(t297));
  t298 = ((int64_t)(v9));
  t299 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t300 = ((t298) == (t299));
  if (t300) {
  {
  t301 = ((uint64_t)(v4));
  t302 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1500))))));
  /* FG_DIRECT_SLICE_INDEX */
  t303 = ((int64_t)UINT64_C(0));
  t304 = ((int64_t *)(v26).data) + gg_index(t303, ((int64_t)UINT64_C(8)));
  t305 = ((int64_t)((*t304)));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t301, t302, t305);
  }
  }
  t306 = ((int64_t)(v9));
  t307 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t306) + (uint64_t)(t307)))));
  }
  goto label28;
label29:;
  t308 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t309 = ((float)(v13));
  v34 = ((float)(((t308) - (t309))));
  v8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label31:;
  t310 = ((int64_t)(v8));
  t311 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t310) < (t311)))) goto label32;
  {
  t312 = ((uint64_t)(v3));
  t313 = ((int64_t)(v11));
  t314 = ((int64_t)(((int64_t)UINT64_C(292))));
  t315 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t313) * (uint64_t)(t314)))))));
  t316 = ((uint64_t)(((uint64_t)((uint64_t)(t312) + (uint64_t)(t315)))));
  t317 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(260))))));
  t318 = ((int64_t)(v8));
  t319 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t320 = ((int64_t)(((int64_t)((uint64_t)(t318) * (uint64_t)(t319)))));
  t321 = ((int64_t)(((int64_t)((uint64_t)(t317) + (uint64_t)(t320)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldi16)(uint64_t, int64_t);
#endif
  t322 = fg410k_ldi16(t316, t321);
  v35 = ((int64_t)(t322));
  t323 = ((uint64_t)(v3));
  t324 = ((int64_t)(v11));
  t325 = ((int64_t)(((int64_t)UINT64_C(292))));
  t326 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t324) * (uint64_t)(t325)))))));
  t327 = ((uint64_t)(((uint64_t)((uint64_t)(t323) + (uint64_t)(t326)))));
  t328 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(262))))));
  t329 = ((int64_t)(v8));
  t330 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t331 = ((int64_t)(((int64_t)((uint64_t)(t329) * (uint64_t)(t330)))));
  t332 = ((int64_t)(((int64_t)((uint64_t)(t328) + (uint64_t)(t331)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldi16)(uint64_t, int64_t);
#endif
  t333 = fg410k_ldi16(t327, t332);
  v36 = ((int64_t)(t333));
  t334 = ((uint64_t)(v3));
  t335 = ((int64_t)(v11));
  t336 = ((int64_t)(((int64_t)UINT64_C(292))));
  t337 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t335) * (uint64_t)(t336)))))));
  t338 = ((uint64_t)(((uint64_t)((uint64_t)(t334) + (uint64_t)(t337)))));
  t339 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(264))))));
  t340 = ((int64_t)(v8));
  t341 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t342 = ((int64_t)(((int64_t)((uint64_t)(t340) * (uint64_t)(t341)))));
  t343 = ((int64_t)(((int64_t)((uint64_t)(t339) + (uint64_t)(t342)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldi16)(uint64_t, int64_t);
#endif
  t344 = fg410k_ldi16(t338, t343);
  v37 = ((int64_t)(t344));
  t345 = ((uint64_t)(v3));
  t346 = ((int64_t)(v11));
  t347 = ((int64_t)(((int64_t)UINT64_C(292))));
  t348 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t346) * (uint64_t)(t347)))))));
  t349 = ((uint64_t)(((uint64_t)((uint64_t)(t345) + (uint64_t)(t348)))));
  t350 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(266))))));
  t351 = ((int64_t)(v8));
  t352 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t353 = ((int64_t)(((int64_t)((uint64_t)(t351) * (uint64_t)(t352)))));
  t354 = ((int64_t)(((int64_t)((uint64_t)(t350) + (uint64_t)(t353)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldi16)(uint64_t, int64_t);
#endif
  t355 = fg410k_ldi16(t349, t354);
  v38 = ((int64_t)(t355));
  t356 = ((uint64_t)(v4));
  t357 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t358 = ((int64_t)(v8));
  t359 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t360 = ((int64_t)(((int64_t)((uint64_t)(t358) * (uint64_t)(t359)))));
  t361 = ((int64_t)(((int64_t)((uint64_t)(t357) + (uint64_t)(t360)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldu8)(uint64_t, int64_t);
#endif
  t362 = fg410k_ldu8(t356, t361);
  v39 = ((int64_t)(((int64_t)(t362))));
  t363 = ((uint64_t)(v4));
  t364 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(9))))));
  t365 = ((int64_t)(v8));
  t366 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t367 = ((int64_t)(((int64_t)((uint64_t)(t365) * (uint64_t)(t366)))));
  t368 = ((int64_t)(((int64_t)((uint64_t)(t364) + (uint64_t)(t367)))));
#ifndef __cplusplus
  extern int64_t (fg410k_ldu8)(uint64_t, int64_t);
#endif
  t369 = fg410k_ldu8(t363, t368);
  v40 = ((int64_t)(((int64_t)(t369))));
  t370 = ((int64_t)(v39));
  t371 = ((int64_t)(v35));
  t372 = ((int64_t)(v36));
  t373 = ((int64_t)(((int64_t)((uint64_t)(t371) + (uint64_t)(t372)))));
  t374 = ((int64_t)(((int64_t)((uint64_t)(t370) * (uint64_t)(t373)))));
  t375 = ((int64_t)(v40));
  t376 = ((int64_t)(v37));
  t377 = ((int64_t)(v38));
  t378 = ((int64_t)(((int64_t)((uint64_t)(t376) + (uint64_t)(t377)))));
  t379 = ((int64_t)(((int64_t)((uint64_t)(t375) * (uint64_t)(t378)))));
  v41 = ((int64_t)(((int64_t)((uint64_t)(t374) + (uint64_t)(t379)))));
  /* FG_DIRECT_SLICE_INDEX */
  t380 = v8;
  t381 = ((float *)(v7).data) + gg_index(t380, ((int64_t)UINT64_C(4)));
  t382 = ((float)(v34));
  t383 = ((float)(((float)(v41))));
  /* FG_DIRECT_SLICE_INDEX */
  t384 = v8;
  t385 = ((float *)(v7).data) + gg_index(t384, ((int64_t)UINT64_C(4)));
  t386 = ((float)((*t385)));
#ifndef __cplusplus
  extern float (fg410k_fma)(float, float, float);
#endif
  t387 = fg410k_fma(t382, t383, t386);
  (*t381) = ((float)(t387));
  t388 = ((int64_t)(v8));
  t389 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t388) + (uint64_t)(t389)))));
  }
  goto label31;
label32:;
  t390 = ((uint64_t)(v4));
  t391 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2000))))));
  t392 = ((int64_t)(v11));
  t393 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t394 = ((int64_t)(((int64_t)((uint64_t)(t392) * (uint64_t)(t393)))));
  t395 = ((int64_t)(((int64_t)((uint64_t)(t391) + (uint64_t)(t394)))));
  t396 = ((float)(v12));
#ifndef __cplusplus
  extern int64_t (fg410k_f32bits)(float);
#endif
  t397 = fg410k_f32bits(t396);
  t398 = ((int64_t)(t397));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t390, t395, t398);
  t399 = ((uint64_t)(v4));
  t400 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2008))))));
  t401 = ((int64_t)(v11));
  t402 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t403 = ((int64_t)(((int64_t)((uint64_t)(t401) * (uint64_t)(t402)))));
  t404 = ((int64_t)(((int64_t)((uint64_t)(t400) + (uint64_t)(t403)))));
  t405 = ((float)(v13));
#ifndef __cplusplus
  extern int64_t (fg410k_f32bits)(float);
#endif
  t406 = fg410k_f32bits(t405);
  t407 = ((int64_t)(t406));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t399, t404, t407);
  t408 = ((uint64_t)(v4));
  t409 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2016))))));
  t410 = ((int64_t)(v11));
  t411 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t412 = ((int64_t)(((int64_t)((uint64_t)(t410) * (uint64_t)(t411)))));
  t413 = ((int64_t)(((int64_t)((uint64_t)(t409) + (uint64_t)(t412)))));
  t414 = ((int64_t)(v19));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t408, t413, t414);
  t415 = ((uint64_t)(v4));
  t416 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2024))))));
  t417 = ((int64_t)(v11));
  t418 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t419 = ((int64_t)(((int64_t)((uint64_t)(t417) * (uint64_t)(t418)))));
  t420 = ((int64_t)(((int64_t)((uint64_t)(t416) + (uint64_t)(t419)))));
  /* FG_DIRECT_SLICE_INDEX */
  t421 = ((int64_t)UINT64_C(0));
  t422 = ((float *)(v6).data) + gg_index(t421, ((int64_t)UINT64_C(8)));
  t423 = ((float)((*t422)));
#ifndef __cplusplus
  extern int64_t (fg410k_f32bits)(float);
#endif
  t424 = fg410k_f32bits(t423);
  t425 = ((int64_t)(t424));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t415, t420, t425);
  t426 = ((uint64_t)(v4));
  t427 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1300))))));
  t428 = ((float)(v12));
#ifndef __cplusplus
  extern int64_t (fg410k_f32bits)(float);
#endif
  t429 = fg410k_f32bits(t428);
  t430 = ((int64_t)(t429));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t426, t427, t430);
  t431 = ((uint64_t)(v4));
  t432 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1308))))));
  t433 = ((int64_t)(v19));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t431, t432, t433);
  t434 = ((uint64_t)(v4));
  t435 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1316))))));
  /* FG_DIRECT_SLICE_INDEX */
  t436 = ((int64_t)UINT64_C(0));
  t437 = ((float *)(v6).data) + gg_index(t436, ((int64_t)UINT64_C(8)));
  t438 = ((float)((*t437)));
#ifndef __cplusplus
  extern int64_t (fg410k_f32bits)(float);
#endif
  t439 = fg410k_f32bits(t438);
  t440 = ((int64_t)(t439));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t434, t435, t440);
  t441 = ((int64_t)(v11));
  t442 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t441) + (uint64_t)(t442)))));
  }
  goto label7;
label8:;
  /* FG_DIRECT_SLICE_INDEX */
  t443 = ((int64_t)UINT64_C(4));
  t444 = ((float *)(v6).data) + gg_index(t443, ((int64_t)UINT64_C(8)));
  t445 = ((float)((*t444)));
  /* FG_DIRECT_SLICE_INDEX */
  t446 = ((int64_t)UINT64_C(0));
  t447 = ((float *)(v6).data) + gg_index(t446, ((int64_t)UINT64_C(8)));
  t448 = ((float)((*t447)));
  v42 = ((float)(((t445) + (t448))));
  /* FG_DIRECT_SLICE_INDEX */
  t449 = ((int64_t)UINT64_C(5));
  t450 = ((float *)(v6).data) + gg_index(t449, ((int64_t)UINT64_C(8)));
  t451 = ((float)((*t450)));
  /* FG_DIRECT_SLICE_INDEX */
  t452 = ((int64_t)UINT64_C(1));
  t453 = ((float *)(v6).data) + gg_index(t452, ((int64_t)UINT64_C(8)));
  t454 = ((float)((*t453)));
  v43 = ((float)(((t451) + (t454))));
  /* FG_DIRECT_SLICE_INDEX */
  t455 = ((int64_t)UINT64_C(6));
  t456 = ((float *)(v6).data) + gg_index(t455, ((int64_t)UINT64_C(8)));
  t457 = ((float)((*t456)));
  /* FG_DIRECT_SLICE_INDEX */
  t458 = ((int64_t)UINT64_C(2));
  t459 = ((float *)(v6).data) + gg_index(t458, ((int64_t)UINT64_C(8)));
  t460 = ((float)((*t459)));
  v44 = ((float)(((t457) + (t460))));
  /* FG_DIRECT_SLICE_INDEX */
  t461 = ((int64_t)UINT64_C(7));
  t462 = ((float *)(v6).data) + gg_index(t461, ((int64_t)UINT64_C(8)));
  t463 = ((float)((*t462)));
  /* FG_DIRECT_SLICE_INDEX */
  t464 = ((int64_t)UINT64_C(3));
  t465 = ((float *)(v6).data) + gg_index(t464, ((int64_t)UINT64_C(8)));
  t466 = ((float)((*t465)));
  v45 = ((float)(((t463) + (t466))));
  t467 = ((float)(v42));
  t468 = ((float)(v44));
  t469 = ((float)(((t467) + (t468))));
  t470 = ((float)(v43));
  t471 = ((float)(v45));
  t472 = ((float)(((t470) + (t471))));
  v46 = ((float)(((t469) + (t472))));
  /* FG_DIRECT_SLICE_INDEX */
  t473 = ((int64_t)UINT64_C(0));
  t474 = ((float *)(v7).data) + gg_index(t473, ((int64_t)UINT64_C(4)));
  t475 = ((float)((*t474)));
  /* FG_DIRECT_SLICE_INDEX */
  t476 = ((int64_t)UINT64_C(2));
  t477 = ((float *)(v7).data) + gg_index(t476, ((int64_t)UINT64_C(4)));
  t478 = ((float)((*t477)));
  t479 = ((float)(((t475) + (t478))));
  /* FG_DIRECT_SLICE_INDEX */
  t480 = ((int64_t)UINT64_C(1));
  t481 = ((float *)(v7).data) + gg_index(t480, ((int64_t)UINT64_C(4)));
  t482 = ((float)((*t481)));
  /* FG_DIRECT_SLICE_INDEX */
  t483 = ((int64_t)UINT64_C(3));
  t484 = ((float *)(v7).data) + gg_index(t483, ((int64_t)UINT64_C(4)));
  t485 = ((float)((*t484)));
  t486 = ((float)(((t482) + (t485))));
  v47 = ((float)(((t479) + (t486))));
  t487 = ((uint64_t)(v4));
  t488 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1700))))));
  t489 = ((float)(v46));
#ifndef __cplusplus
  extern int64_t (fg410k_f32bits)(float);
#endif
  t490 = fg410k_f32bits(t489);
  t491 = ((int64_t)(t490));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t487, t488, t491);
  t492 = ((uint64_t)(v4));
  t493 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1708))))));
  t494 = ((float)(v47));
#ifndef __cplusplus
  extern int64_t (fg410k_f32bits)(float);
#endif
  t495 = fg410k_f32bits(t494);
  t496 = ((int64_t)(t495));
#ifndef __cplusplus
  extern void (fg410k_stw)(uint64_t, int64_t, int64_t);
#endif
  fg410k_stw(t492, t493, t496);
  t497 = ((float)(v46));
  t498 = ((float)(v47));
  v5 = ((float)(((t497) + (t498))));
  t499 = ((int64_t)(v0));
  t500 = ((int64_t)(((int64_t)UINT64_C(2))));
  t501 = ((int64_t)(((int64_t)UINT64_C(0))));
  t502 = ((float)(v5));
#ifndef __cplusplus
  extern int64_t (fg410k_f32bits)(float);
#endif
  t503 = fg410k_f32bits(t502);
  t504 = ((uint64_t)(((uint64_t)(t503))));
#ifndef __cplusplus
  extern void (fg410k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg410k_put(t499, t500, t501, t504);
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
