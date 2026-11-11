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
typedef struct { void *data; int64_t len; } fg298ds_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg298ds_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg298ds_gg_stack;
typedef struct { fg298ds_gg_header *header; fg298ds_gg_stack *stack; } fg298ds_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg298ds_gg_stack fg298ds_gg_stacks[GG_STACK_COUNT];
static int fg298ds_gg_argc;
static char **fg298ds_gg_argv;
static void fg298ds_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg298ds_gg_stack *fg298ds_gg_get_stack(int64_t id) {
    fg298ds_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg298ds_gg_fail("data stack limit exceeded");
    s = &fg298ds_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg298ds_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg298ds_gg_allocate(fg298ds_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg298ds_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg298ds_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg298ds_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg298ds_gg_reference fg298ds_gg_ref(fg298ds_gg_header *h, fg298ds_gg_stack *s) { fg298ds_gg_reference r; r.header=h; r.stack=s; return r; }
static fg298ds_gg_slice fg298ds_gg_view(void *p, int64_t n) { fg298ds_gg_slice v; v.data=p; v.len=n; return v; }
static fg298ds_gg_slice fg298ds_gg_byte_view(fg298ds_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg298ds_gg_fail("byte view size overflow");
    return fg298ds_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg298ds_gg_slice fg298ds_gg_sub(fg298ds_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg298ds_gg_fail("slice out of bounds");
    return fg298ds_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg298ds_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg298ds_gg_fail("array index out of bounds"); return i; }
static void fg298ds_gg_append(fg298ds_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg298ds_gg_fail("invalid array size");
    to=fg298ds_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg298ds_gg_resize(fg298ds_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg298ds_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg298ds_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg298ds_gg_fail("invalid array size");
        p=fg298ds_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg298ds_gg_div(int64_t a, int64_t b) { if (!b) fg298ds_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg298ds_gg_fail("division overflow"); return a/b; }
static int64_t fg298ds_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg298ds_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg298ds_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg298ds_gg_fail("division by zero");return a/b; }
static uint64_t fg298ds_gg_umod(uint64_t a,uint64_t b) { if(!b)fg298ds_gg_fail("division by zero");return a%b; }
static int fg298ds_gg_equal(fg298ds_gg_slice a, fg298ds_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg298ds_gg_text(fg298ds_gg_reference out, fg298ds_gg_slice v) { fg298ds_gg_append(out,v.data,v.len,1); }
static void fg298ds_gg_integer(fg298ds_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg298ds_gg_append(out,b,k,1); }
static void fg298ds_gg_unsigned(fg298ds_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg298ds_gg_append(out,b,k,1); }
static double fg298ds_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t ggf_fmt_f64(uint8_t *out,double n);
static int64_t ggf_fmt_f32(uint8_t *out,float n);
/* Only the in-range fast path uses a C float-to-integer cast. The tail
   projects the finite IEEE significand modulo 2^64 without UB or libm.
   Nonfinite unchecked inputs produce zero; checked inputs reject them. */
static uint64_t fg298ds_gg_float_integer_wrap(double d) {
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
static void fg298ds_gg_float_integer_fail(double d,int f32,const char *type,const char *file,int line,const char *why) {
    uint8_t text[64];int64_t n=f32?ggf_fmt_f32(text,(float)d):ggf_fmt_f64(text,d);
    fprintf(stderr,"goose runtime error: conversion %s to %s (%.*s) at %s:%d\\n",why,type,(int)n,(const char*)text,file,line);
    exit(1);
}
#endif
static uint64_t fg298ds_gg_float_integer(double d,int width,int unsign,int checked,int f32,const char *type,const char *file,int line) {
#if defined(GS_DEBUG) && GS_DEBUG
    if(checked) {
        int64_t value,lo,hi;
        if(unsign && width==64) {
            uint64_t u;
            if(!(d>=0.0 && d<18446744073709551616.0))
                fg298ds_gg_float_integer_fail(d,f32,type,file,line,"out of range");
            u=(uint64_t)d;
            if((double)u!=d)fg298ds_gg_float_integer_fail(d,f32,type,file,line,"changes the value");
            return u;
        }
        if(!(d>=-9223372036854775808.0 && d<9223372036854775808.0))
            fg298ds_gg_float_integer_fail(d,f32,type,file,line,"out of range");
        value=(int64_t)d;
        hi=(int64_t)((UINT64_C(1)<<(width-(unsign?0:1)))-1);
        lo=unsign?0:-1-hi;
        if(value<lo || value>hi)fg298ds_gg_float_integer_fail(d,f32,type,file,line,"out of range");
        if((double)value!=d)fg298ds_gg_float_integer_fail(d,f32,type,file,line,"changes the value");
        return (uint64_t)value;
    }
#else
    (void)width;(void)unsign;(void)checked;(void)f32;(void)type;(void)file;(void)line;
#endif
    return fg298ds_gg_float_integer_wrap(d);
}
static void fg298ds_gg_float(fg298ds_gg_reference out,double n) { uint8_t b[64];int64_t k=ggf_fmt_f64(b,n);fg298ds_gg_append(out,b,k,1); }
static void fg298ds_gg_float32(fg298ds_gg_reference out,float n) { uint8_t b[64];int64_t k=ggf_fmt_f32(b,n);fg298ds_gg_append(out,b,k,1); }
static void fg298ds_gg_bool(fg298ds_gg_reference out,uint8_t n) { fg298ds_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg298ds_gg_write(FILE *file,fg298ds_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg298ds_gg_fail("write failed"); }
static char *fg298ds_gg_cstring(fg298ds_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg298ds_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t gs_os_read_file(fg298ds_gg_slice path,fg298ds_gg_reference out) {
    char *p=fg298ds_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg298ds_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t gs_os_write_file(fg298ds_gg_slice path,fg298ds_gg_slice data) {
    char *p=fg298ds_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t gs_os_file_exists(fg298ds_gg_slice path) { char*p=fg298ds_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void gs_os_write_stdout(fg298ds_gg_slice s) { fg298ds_gg_write(stdout,s); }
static void gs_os_write_stderr(fg298ds_gg_slice s) { fg298ds_gg_write(stderr,s); }
static void gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t gs_os_arg_count(void) { return fg298ds_gg_argc; }
static void gs_os_arg(int64_t i,fg298ds_gg_reference out) { if(i>=0 && i<fg298ds_gg_argc)fg298ds_gg_append(out,fg298ds_gg_argv[i],(int64_t)strlen(fg298ds_gg_argv[i]),1); }
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


static void fg298ds_gg_quote(fg298ds_gg_reference out,fg298ds_gg_slice text) { int64_t i; unsigned char c; fg298ds_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg298ds_gg_append(out,"\\",1,1);fg298ds_gg_append(out,&c,1,1);} else if(c=='\n')fg298ds_gg_append(out,"\\n",2,1);else if(c=='\r')fg298ds_gg_append(out,"\\r",2,1);else if(c=='\t')fg298ds_gg_append(out,"\\t",2,1);else fg298ds_gg_append(out,&c,1,1);}fg298ds_gg_append(out,"\"",1,1);}
static uint8_t fg298ds_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg298ds_gg_equal_elements_1(fg298ds_gg_slice a, fg298ds_gg_slice b);
static void fg298ds_gg_render_1(fg298ds_gg_reference out, int64_t value);
static uint8_t fg298ds_gg_eq_2(uint8_t a, uint8_t b);
static uint8_t fg298ds_gg_equal_elements_2(fg298ds_gg_slice a, fg298ds_gg_slice b);
static void fg298ds_gg_render_2(fg298ds_gg_reference out, uint8_t value);
static uint8_t fg298ds_gg_eq_3(uint64_t a, uint64_t b);
static uint8_t fg298ds_gg_equal_elements_3(fg298ds_gg_slice a, fg298ds_gg_slice b);
static void fg298ds_gg_render_3(fg298ds_gg_reference out, uint64_t value);
static uint8_t fg298ds_gg_eq_5(uint8_t a, uint8_t b);
static uint8_t fg298ds_gg_equal_elements_5(fg298ds_gg_slice a, fg298ds_gg_slice b);
static void fg298ds_gg_render_5(fg298ds_gg_reference out, uint8_t value);
static uint8_t fg298ds_gg_equal_elements_1(fg298ds_gg_slice a,fg298ds_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg298ds_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg298ds_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg298ds_gg_render_1(fg298ds_gg_reference out, int64_t value) {
fg298ds_gg_integer(out,value);
}
static uint8_t fg298ds_gg_equal_elements_2(fg298ds_gg_slice a,fg298ds_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg298ds_gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg298ds_gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void fg298ds_gg_render_2(fg298ds_gg_reference out, uint8_t value) {
fg298ds_gg_bool(out,value);
}
static uint8_t fg298ds_gg_equal_elements_3(fg298ds_gg_slice a,fg298ds_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg298ds_gg_eq_3(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg298ds_gg_eq_3(uint64_t a, uint64_t b) {
return a == b;
}
static void fg298ds_gg_render_3(fg298ds_gg_reference out, uint64_t value) {
fg298ds_gg_unsigned(out,value);
}
static uint8_t fg298ds_gg_equal_elements_5(fg298ds_gg_slice a,fg298ds_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg298ds_gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg298ds_gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void fg298ds_gg_render_5(fg298ds_gg_reference out, uint8_t value) {
fg298ds_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg298ds_gg_globals;
static void fg298ds_gg_function_0(int64_t base);
static uint8_t fg298ds_gg_function_1(int64_t p0, int64_t base);
static void fg298ds_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static int64_t fg298ds_gg_function_5(int64_t p0, int64_t base);
static int64_t fg298ds_gg_function_6(uint64_t p0, int64_t p1, int64_t base);
static void fg298ds_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg298k_fail)(int64_t);
#endif
  fg298k_fail(t2);
  }
  }
  }
fg298ds_gg_done:;
}
static int64_t fg298ds_gg_function_6(uint64_t p0, int64_t p1, int64_t base) {
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
  extern int64_t (fg298k_ld_i64)(uint64_t, int64_t);
#endif
  t5 = fg298k_ld_i64(t1, t4);
  t0 = ((int64_t)(t5));
  goto fg298ds_gg_done;
  }
fg298ds_gg_done:;
  return t0;
}
static int64_t fg298ds_gg_function_5(int64_t p0, int64_t base) {
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(255))))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  v1 = ((int64_t)(((int64_t)fg298ds_gg_div(t3, t4))));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  goto fg298ds_gg_done;
  }
fg298ds_gg_done:;
  return t0;
}
static uint8_t fg298ds_gg_function_1(int64_t p0, int64_t base) {
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
  uint64_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  uint64_t t35;
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
  uint64_t t49;
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  uint64_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  uint8_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  uint64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  uint8_t t104;
  uint8_t t105;
  uint8_t t106;
  uint8_t t107;
  uint64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  uint64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint8_t t128;
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
  uint8_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  uint8_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  uint64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  uint64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  uint64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  uint64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  uint64_t t166;
  int64_t t167;
  int64_t t168;
  uint64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  uint64_t t175;
  int64_t t176;
  int64_t t177;
  uint64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  uint64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  uint64_t t193;
  int64_t t194;
  uint64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  uint64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
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
  uint64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  uint64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  uint64_t t243;
  int64_t t244;
  int64_t t245;
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
  int64_t t256;
  int64_t t257;
  uint64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  uint64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  uint8_t t278;
  uint8_t t279;
  uint8_t t280;
  uint64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  uint8_t t290;
  uint64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  uint64_t t300;
  int64_t t301;
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
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  uint64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
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
  int64_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  uint8_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  uint64_t t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  uint64_t t353;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  uint8_t v5;
  int64_t v6;
  int64_t v7;
  uint8_t v8;
  int64_t v9;
  uint64_t v10;
  uint8_t v11;
  uint64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  uint8_t v20;
  uint8_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  uint64_t v27;
  uint64_t v28;
  uint64_t v29;
  uint64_t v30;
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
  uint8_t v43;
  int64_t v44;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t4 = fg298k_get(t1, t2, t3);
  v1 = ((int64_t)(((int64_t)(t4))));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t8 = fg298k_get(t5, t6, t7);
  v2 = ((uint64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t12 = fg298k_get(t9, t10, t11);
  v3 = ((int64_t)(((int64_t)(t12))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t16 = fg298k_get(t13, t14, t15);
  v4 = ((int64_t)(((int64_t)(t16))));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t20 = fg298k_get(t17, t18, t19);
  t21 = ((uint64_t)(t20));
  t22 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  v5 = ((t21) != (t22));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(5))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t26 = fg298k_get(t23, t24, t25);
  v6 = ((int64_t)(((int64_t)(t26))));
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((int64_t)(((int64_t)UINT64_C(6))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t30 = fg298k_get(t27, t28, t29);
  v7 = ((int64_t)(((int64_t)(t30))));
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((int64_t)(((int64_t)UINT64_C(7))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t34 = fg298k_get(t31, t32, t33);
  t35 = ((uint64_t)(t34));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  v8 = ((t35) != (t36));
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((int64_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t40 = fg298k_get(t37, t38, t39);
  v9 = ((int64_t)(((int64_t)(t40))));
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((int64_t)(((int64_t)UINT64_C(9))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t44 = fg298k_get(t41, t42, t43);
  v10 = ((uint64_t)(t44));
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((int64_t)(((int64_t)UINT64_C(10))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t48 = fg298k_get(t45, t46, t47);
  t49 = ((uint64_t)(t48));
  t50 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  v11 = ((t49) != (t50));
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((int64_t)(((int64_t)UINT64_C(11))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t54 = fg298k_get(t51, t52, t53);
  v12 = ((uint64_t)(t54));
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(3))));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t58 = fg298k_get(t55, t56, t57);
  v13 = ((int64_t)(((int64_t)(t58))));
  t59 = ((int64_t)(v4));
  t60 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t61 = ((t59) >= (t60));
  t62 = t61;
  if (t62) {
  t63 = ((int64_t)(v6));
  t64 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t62 = ((t63) >= (t64));
  }
  t65 = t62;
  t66 = t65;
  if (t66) {
  t67 = ((int64_t)(v7));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t66 = ((t67) >= (t68));
  }
  t69 = t66;
  t70 = ((int64_t)(((int64_t)UINT64_C(60))));
  fg298ds_gg_function_3(t69, t70, base + 0);
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((int64_t)(v1));
  t72 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t73 = ((t71) == (t72));
  if (t73) {
  {
  v15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v16 = ((int64_t)(v15));
  t74 = v5;
  if (t74) {
  {
  v16 = ((int64_t)(v6));
  }
  }
  t75 = ((int64_t)(v4));
  t76 = ((int64_t)(v16));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t75) * (uint64_t)(t76)))));
  t78 = ((int64_t)(v7));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  v18 = ((int64_t)(v3));
  t79 = ((int64_t)(v17));
  t80 = ((int64_t)(v3));
  t81 = ((t79) < (t80));
  if (t81) {
  {
  v18 = ((int64_t)(v17));
  }
  }
  t82 = ((int64_t)(v18));
  v18 = fg298ds_gg_function_5(t82, base + 0);
  t83 = v8;
  if (t83) {
  {
  v18 = ((int64_t)(v3));
  }
  }
  v14 = ((int64_t)(v18));
  }
  } else {
  {
  t84 = ((int64_t)(v1));
  t85 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t86 = ((t84) == (t85));
  if (t86) {
  {
  t87 = ((int64_t)(v9));
  t88 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t89 = ((t87) >= (t88));
  t90 = ((int64_t)(((int64_t)UINT64_C(61))));
  fg298ds_gg_function_3(t89, t90, base + 0);
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t91 = ((int64_t)(v19));
  t92 = ((int64_t)(v9));
  if (!(((t91) < (t92)))) goto label2;
  {
  v20 = ((uint8_t)UINT64_C(1));
  t93 = v11;
  if (t93) {
  {
  t94 = ((uint64_t)(v12));
  t95 = ((int64_t)(v19));
  t96 = fg298ds_gg_function_6(t94, t95, base + 0);
  t97 = ((int64_t)(t96));
  t98 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v20 = ((t97) != (t98));
  }
  }
  t99 = ((uint64_t)(v10));
  t100 = ((int64_t)(v19));
  t101 = fg298ds_gg_function_6(t99, t100, base + 0);
  t102 = ((int64_t)(t101));
  t103 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v21 = ((t102) != (t103));
  v22 = ((int64_t)(((int64_t)UINT64_C(0))));
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t104 = v20;
  if (t104) {
  {
  t105 = v21;
  t106 = (!(t105));
  if (t106) {
  {
  v22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t107 = v21;
  if (t107) {
  {
  v23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  }
  t108 = ((uint64_t)(v2));
  t109 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t110 = ((int64_t)(v19));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t109) * (uint64_t)(t110)))));
  t112 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t111) * (uint64_t)(t112)))));
  t114 = ((int64_t)(v22));
#ifndef __cplusplus
  extern void (fg298k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg298k_st_i64(t108, t113, t114);
  t115 = ((uint64_t)(v2));
  t116 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t117 = ((int64_t)(v19));
  t118 = ((int64_t)(((int64_t)((uint64_t)(t116) * (uint64_t)(t117)))));
  t119 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t118) + (uint64_t)(t119)))));
  t121 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t122 = ((int64_t)(((int64_t)((uint64_t)(t120) * (uint64_t)(t121)))));
  t123 = ((int64_t)(v23));
#ifndef __cplusplus
  extern void (fg298k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg298k_st_i64(t115, t122, t123);
  t124 = ((int64_t)(v19));
  t125 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t124) + (uint64_t)(t125)))));
  }
  goto label1;
label2:;
  v14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t126 = ((int64_t)(v1));
  t127 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t128 = ((t126) == (t127));
  if (t128) {
  {
  t129 = ((int64_t)(v0));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = ((int64_t)(((int64_t)UINT64_C(12))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t132 = fg298k_get(t129, t130, t131);
  v24 = ((int64_t)(((int64_t)(t132))));
  t133 = ((int64_t)(v0));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((int64_t)(((int64_t)UINT64_C(13))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t136 = fg298k_get(t133, t134, t135);
  v25 = ((int64_t)(((int64_t)(t136))));
  t137 = ((int64_t)(v24));
  t138 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t139 = ((t137) != (t138));
  t140 = t139;
  if (t140) {
  t141 = ((int64_t)(v25));
  t142 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t140 = ((t141) != (t142));
  }
  t143 = t140;
  if (t143) {
  {
  v14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  } else {
  {
  t144 = ((int64_t)(v1));
  t145 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t146 = ((t144) == (t145));
  if (t146) {
  {
  t147 = ((int64_t)(v0));
  t148 = ((int64_t)(((int64_t)UINT64_C(0))));
  t149 = ((int64_t)(((int64_t)UINT64_C(14))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t150 = fg298k_get(t147, t148, t149);
  v26 = ((int64_t)(((int64_t)(t150))));
  t151 = ((int64_t)(v0));
  t152 = ((int64_t)(((int64_t)UINT64_C(0))));
  t153 = ((int64_t)(((int64_t)UINT64_C(12))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t154 = fg298k_get(t151, t152, t153);
  v27 = ((uint64_t)(t154));
  t155 = ((int64_t)(v0));
  t156 = ((int64_t)(((int64_t)UINT64_C(0))));
  t157 = ((int64_t)(((int64_t)UINT64_C(13))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t158 = fg298k_get(t155, t156, t157);
  v28 = ((uint64_t)(t158));
  t159 = ((int64_t)(v0));
  t160 = ((int64_t)(((int64_t)UINT64_C(0))));
  t161 = ((int64_t)(((int64_t)UINT64_C(11))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t162 = fg298k_get(t159, t160, t161);
  v29 = ((uint64_t)(t162));
  t163 = ((int64_t)(v0));
  t164 = ((int64_t)(((int64_t)UINT64_C(0))));
  t165 = ((int64_t)(((int64_t)UINT64_C(10))));
#ifndef __cplusplus
  extern uint64_t (fg298k_get)(int64_t, int64_t, int64_t);
#endif
  t166 = fg298k_get(t163, t164, t165);
  v30 = ((uint64_t)(t166));
  v31 = ((int64_t)(v13));
  v32 = ((int64_t)(v26));
  v33 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t167 = ((int64_t)(v33));
  t168 = ((int64_t)(v26));
  if (!(((t167) < (t168)))) goto label5;
  {
  t169 = ((uint64_t)(v2));
  t170 = ((int64_t)(v33));
  t171 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t172 = ((int64_t)(((int64_t)((uint64_t)(t170) * (uint64_t)(t171)))));
  t173 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t172) * (uint64_t)(t173)))));
  t175 = ((uint64_t)(v27));
  t176 = ((int64_t)(v33));
  t177 = fg298ds_gg_function_6(t175, t176, base + 0);
#ifndef __cplusplus
  extern void (fg298k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg298k_st_i64(t169, t174, t177);
  t178 = ((uint64_t)(v2));
  t179 = ((int64_t)(v33));
  t180 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t181 = ((int64_t)(((int64_t)((uint64_t)(t179) * (uint64_t)(t180)))));
  t182 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t183 = ((int64_t)(((int64_t)((uint64_t)(t181) + (uint64_t)(t182)))));
  t184 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t185 = ((int64_t)(((int64_t)((uint64_t)(t183) * (uint64_t)(t184)))));
  t186 = ((uint64_t)(v28));
  t187 = ((int64_t)(v33));
  t188 = fg298ds_gg_function_6(t186, t187, base + 0);
#ifndef __cplusplus
  extern void (fg298k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg298k_st_i64(t178, t185, t188);
  t189 = ((int64_t)(v33));
  t190 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t189) + (uint64_t)(t190)))));
  }
  goto label4;
label5:;
  v34 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t191 = ((int64_t)(v34));
  t192 = ((int64_t)(v31));
  if (!(((t191) < (t192)))) goto label8;
  {
  t193 = ((uint64_t)(v29));
  t194 = ((int64_t)(v34));
  v35 = fg298ds_gg_function_6(t193, t194, base + 0);
  t195 = ((uint64_t)(v30));
  t196 = ((int64_t)(v34));
  v36 = fg298ds_gg_function_6(t195, t196, base + 0);
  t197 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t198 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v37 = ((int64_t)(((int64_t)((uint64_t)(t197) - (uint64_t)(t198)))));
  v38 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t199 = ((int64_t)(v38));
  t200 = ((int64_t)(v32));
  if (!(((t199) < (t200)))) goto label11;
  {
  t201 = ((uint64_t)(v2));
  t202 = ((int64_t)(v38));
  t203 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t204 = ((int64_t)(((int64_t)((uint64_t)(t202) * (uint64_t)(t203)))));
  t205 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t206 = ((int64_t)(((int64_t)((uint64_t)(t204) * (uint64_t)(t205)))));
#ifndef __cplusplus
  extern int64_t (fg298k_ld_i64)(uint64_t, int64_t);
#endif
  t207 = fg298k_ld_i64(t201, t206);
  t208 = ((int64_t)(t207));
  t209 = ((int64_t)(v35));
  t210 = ((t208) == (t209));
  if (t210) {
  {
  v37 = ((int64_t)(v38));
  }
  }
  t211 = ((int64_t)(v38));
  t212 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t211) + (uint64_t)(t212)))));
  }
  goto label10;
label11:;
  t213 = ((int64_t)(v37));
  t214 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t215 = ((t213) >= (t214));
  if (t215) {
  {
  t216 = ((uint64_t)(v2));
  t217 = ((int64_t)(v37));
  t218 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t219 = ((int64_t)(((int64_t)((uint64_t)(t217) * (uint64_t)(t218)))));
  t220 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t221 = ((int64_t)(((int64_t)((uint64_t)(t219) + (uint64_t)(t220)))));
  t222 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t223 = ((int64_t)(((int64_t)((uint64_t)(t221) * (uint64_t)(t222)))));
#ifndef __cplusplus
  extern int64_t (fg298k_ld_i64)(uint64_t, int64_t);
#endif
  t224 = fg298k_ld_i64(t216, t223);
  v39 = ((int64_t)(t224));
  t225 = ((uint64_t)(v2));
  t226 = ((int64_t)(v37));
  t227 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t228 = ((int64_t)(((int64_t)((uint64_t)(t226) * (uint64_t)(t227)))));
  t229 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t230 = ((int64_t)(((int64_t)((uint64_t)(t228) + (uint64_t)(t229)))));
  t231 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t232 = ((int64_t)(((int64_t)((uint64_t)(t230) * (uint64_t)(t231)))));
  t233 = ((int64_t)(v39));
  t234 = ((int64_t)(v36));
  t235 = ((int64_t)(((int64_t)((uint64_t)(t233) + (uint64_t)(t234)))));
#ifndef __cplusplus
  extern void (fg298k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg298k_st_i64(t225, t232, t235);
  }
  } else {
  {
  t236 = ((uint64_t)(v2));
  t237 = ((int64_t)(v32));
  t238 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t239 = ((int64_t)(((int64_t)((uint64_t)(t237) * (uint64_t)(t238)))));
  t240 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t241 = ((int64_t)(((int64_t)((uint64_t)(t239) * (uint64_t)(t240)))));
  t242 = ((int64_t)(v35));
#ifndef __cplusplus
  extern void (fg298k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg298k_st_i64(t236, t241, t242);
  t243 = ((uint64_t)(v2));
  t244 = ((int64_t)(v32));
  t245 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t246 = ((int64_t)(((int64_t)((uint64_t)(t244) * (uint64_t)(t245)))));
  t247 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t248 = ((int64_t)(((int64_t)((uint64_t)(t246) + (uint64_t)(t247)))));
  t249 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t250 = ((int64_t)(((int64_t)((uint64_t)(t248) * (uint64_t)(t249)))));
  t251 = ((int64_t)(v36));
#ifndef __cplusplus
  extern void (fg298k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg298k_st_i64(t243, t250, t251);
  t252 = ((int64_t)(v32));
  t253 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t252) + (uint64_t)(t253)))));
  }
  }
  t254 = ((int64_t)(v34));
  t255 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t254) + (uint64_t)(t255)))));
  }
  goto label7;
label8:;
  v33 = ((int64_t)(((int64_t)UINT64_C(1))));
label13:;
  t256 = ((int64_t)(v33));
  t257 = ((int64_t)(v32));
  if (!(((t256) < (t257)))) goto label14;
  {
  t258 = ((uint64_t)(v2));
  t259 = ((int64_t)(v33));
  t260 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t261 = ((int64_t)(((int64_t)((uint64_t)(t259) * (uint64_t)(t260)))));
  t262 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t263 = ((int64_t)(((int64_t)((uint64_t)(t261) * (uint64_t)(t262)))));
#ifndef __cplusplus
  extern int64_t (fg298k_ld_i64)(uint64_t, int64_t);
#endif
  t264 = fg298k_ld_i64(t258, t263);
  v40 = ((int64_t)(t264));
  t265 = ((uint64_t)(v2));
  t266 = ((int64_t)(v33));
  t267 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t266) * (uint64_t)(t267)))));
  t269 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t268) + (uint64_t)(t269)))));
  t271 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t272 = ((int64_t)(((int64_t)((uint64_t)(t270) * (uint64_t)(t271)))));
#ifndef __cplusplus
  extern int64_t (fg298k_ld_i64)(uint64_t, int64_t);
#endif
  t273 = fg298k_ld_i64(t265, t272);
  v41 = ((int64_t)(t273));
  t274 = ((int64_t)(v33));
  t275 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v42 = ((int64_t)(((int64_t)((uint64_t)(t274) - (uint64_t)(t275)))));
  v43 = ((uint8_t)UINT64_C(0));
label16:;
  t276 = ((int64_t)(v42));
  t277 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t278 = ((t276) >= (t277));
  t279 = t278;
  if (t279) {
  t280 = v43;
  t279 = (!(t280));
  }
  if (!(t279)) goto label17;
  {
  t281 = ((uint64_t)(v2));
  t282 = ((int64_t)(v42));
  t283 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t284 = ((int64_t)(((int64_t)((uint64_t)(t282) * (uint64_t)(t283)))));
  t285 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t286 = ((int64_t)(((int64_t)((uint64_t)(t284) * (uint64_t)(t285)))));
#ifndef __cplusplus
  extern int64_t (fg298k_ld_i64)(uint64_t, int64_t);
#endif
  t287 = fg298k_ld_i64(t281, t286);
  v44 = ((int64_t)(t287));
  t288 = ((int64_t)(v44));
  t289 = ((int64_t)(v40));
  t290 = ((t288) <= (t289));
  if (t290) {
  {
  v43 = ((uint8_t)UINT64_C(1));
  }
  } else {
  {
  t291 = ((uint64_t)(v2));
  t292 = ((int64_t)(v42));
  t293 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t294 = ((int64_t)(((int64_t)((uint64_t)(t292) + (uint64_t)(t293)))));
  t295 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t296 = ((int64_t)(((int64_t)((uint64_t)(t294) * (uint64_t)(t295)))));
  t297 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t298 = ((int64_t)(((int64_t)((uint64_t)(t296) * (uint64_t)(t297)))));
  t299 = ((int64_t)(v44));
#ifndef __cplusplus
  extern void (fg298k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg298k_st_i64(t291, t298, t299);
  t300 = ((uint64_t)(v2));
  t301 = ((int64_t)(v42));
  t302 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t303 = ((int64_t)(((int64_t)((uint64_t)(t301) + (uint64_t)(t302)))));
  t304 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t305 = ((int64_t)(((int64_t)((uint64_t)(t303) * (uint64_t)(t304)))));
  t306 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t307 = ((int64_t)(((int64_t)((uint64_t)(t305) + (uint64_t)(t306)))));
  t308 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t309 = ((int64_t)(((int64_t)((uint64_t)(t307) * (uint64_t)(t308)))));
  t310 = ((uint64_t)(v2));
  t311 = ((int64_t)(v42));
  t312 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t313 = ((int64_t)(((int64_t)((uint64_t)(t311) * (uint64_t)(t312)))));
  t314 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t315 = ((int64_t)(((int64_t)((uint64_t)(t313) + (uint64_t)(t314)))));
  t316 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t317 = ((int64_t)(((int64_t)((uint64_t)(t315) * (uint64_t)(t316)))));
#ifndef __cplusplus
  extern int64_t (fg298k_ld_i64)(uint64_t, int64_t);
#endif
  t318 = fg298k_ld_i64(t310, t317);
  t319 = ((int64_t)(t318));
#ifndef __cplusplus
  extern void (fg298k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg298k_st_i64(t300, t309, t319);
  t320 = ((int64_t)(v42));
  t321 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v42 = ((int64_t)(((int64_t)((uint64_t)(t320) - (uint64_t)(t321)))));
  }
  }
  }
  goto label16;
label17:;
  t322 = ((uint64_t)(v2));
  t323 = ((int64_t)(v42));
  t324 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t325 = ((int64_t)(((int64_t)((uint64_t)(t323) + (uint64_t)(t324)))));
  t326 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t327 = ((int64_t)(((int64_t)((uint64_t)(t325) * (uint64_t)(t326)))));
  t328 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t329 = ((int64_t)(((int64_t)((uint64_t)(t327) * (uint64_t)(t328)))));
  t330 = ((int64_t)(v40));
#ifndef __cplusplus
  extern void (fg298k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg298k_st_i64(t322, t329, t330);
  t331 = ((uint64_t)(v2));
  t332 = ((int64_t)(v42));
  t333 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t334 = ((int64_t)(((int64_t)((uint64_t)(t332) + (uint64_t)(t333)))));
  t335 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t336 = ((int64_t)(((int64_t)((uint64_t)(t334) * (uint64_t)(t335)))));
  t337 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t338 = ((int64_t)(((int64_t)((uint64_t)(t336) + (uint64_t)(t337)))));
  t339 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t340 = ((int64_t)(((int64_t)((uint64_t)(t338) * (uint64_t)(t339)))));
  t341 = ((int64_t)(v41));
#ifndef __cplusplus
  extern void (fg298k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg298k_st_i64(t331, t340, t341);
  t342 = ((int64_t)(v33));
  t343 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t342) + (uint64_t)(t343)))));
  }
  goto label13;
label14:;
  v14 = ((int64_t)(v32));
  }
  } else {
  {
  t344 = ((uint8_t)UINT64_C(0));
  t345 = ((int64_t)(((int64_t)UINT64_C(62))));
  fg298ds_gg_function_3(t344, t345, base + 0);
  }
  }
  }
  }
  }
  }
  }
  }
  t346 = ((int64_t)(v0));
  t347 = ((int64_t)(((int64_t)UINT64_C(3))));
  t348 = ((int64_t)(((int64_t)UINT64_C(0))));
  t349 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern void (fg298k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg298k_put(t346, t347, t348, t349);
  t350 = ((int64_t)(v0));
  t351 = ((int64_t)(((int64_t)UINT64_C(3))));
  t352 = ((int64_t)(((int64_t)UINT64_C(1))));
  t353 = ((uint64_t)(((uint64_t)(v14))));
#ifndef __cplusplus
  extern void (fg298k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg298k_put(t350, t351, t352, t353);
  t0 = ((uint8_t)UINT64_C(1));
  goto fg298ds_gg_done;
  }
fg298ds_gg_done:;
  return t0;
}
static void fg298ds_gg_function_0(int64_t base) {
  int64_t t0;
  uint8_t t1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = fg298ds_gg_function_1(t0, base + 0);
  (void)(t1);
  }
fg298ds_gg_done:;
}
static int fg298ds_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg298ds_gg_argc = argc; fg298ds_gg_argv = argv;
  fg298ds_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg298ds_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg298ds_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg298ds_gg_program(argc, argv);
#endif
}
