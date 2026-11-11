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
typedef struct { void *data; int64_t len; } fg302ds_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg302ds_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg302ds_gg_stack;
typedef struct { fg302ds_gg_header *header; fg302ds_gg_stack *stack; } fg302ds_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg302ds_gg_stack fg302ds_gg_stacks[GG_STACK_COUNT];
static int fg302ds_gg_argc;
static char **fg302ds_gg_argv;
static void fg302ds_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg302ds_gg_stack *fg302ds_gg_get_stack(int64_t id) {
    fg302ds_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg302ds_gg_fail("data stack limit exceeded");
    s = &fg302ds_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg302ds_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg302ds_gg_allocate(fg302ds_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg302ds_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg302ds_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg302ds_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg302ds_gg_reference fg302ds_gg_ref(fg302ds_gg_header *h, fg302ds_gg_stack *s) { fg302ds_gg_reference r; r.header=h; r.stack=s; return r; }
static fg302ds_gg_slice fg302ds_gg_view(void *p, int64_t n) { fg302ds_gg_slice v; v.data=p; v.len=n; return v; }
static fg302ds_gg_slice fg302ds_gg_byte_view(fg302ds_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg302ds_gg_fail("byte view size overflow");
    return fg302ds_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg302ds_gg_slice fg302ds_gg_sub(fg302ds_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg302ds_gg_fail("slice out of bounds");
    return fg302ds_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg302ds_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg302ds_gg_fail("array index out of bounds"); return i; }
static void fg302ds_gg_append(fg302ds_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg302ds_gg_fail("invalid array size");
    to=fg302ds_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg302ds_gg_resize(fg302ds_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg302ds_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg302ds_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg302ds_gg_fail("invalid array size");
        p=fg302ds_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg302ds_gg_div(int64_t a, int64_t b) { if (!b) fg302ds_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg302ds_gg_fail("division overflow"); return a/b; }
static int64_t fg302ds_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg302ds_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg302ds_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg302ds_gg_fail("division by zero");return a/b; }
static uint64_t fg302ds_gg_umod(uint64_t a,uint64_t b) { if(!b)fg302ds_gg_fail("division by zero");return a%b; }
static int fg302ds_gg_equal(fg302ds_gg_slice a, fg302ds_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg302ds_gg_text(fg302ds_gg_reference out, fg302ds_gg_slice v) { fg302ds_gg_append(out,v.data,v.len,1); }
static void fg302ds_gg_integer(fg302ds_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg302ds_gg_append(out,b,k,1); }
static void fg302ds_gg_unsigned(fg302ds_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg302ds_gg_append(out,b,k,1); }
static double fg302ds_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t ggf_fmt_f64(uint8_t *out,double n);
static int64_t ggf_fmt_f32(uint8_t *out,float n);
/* Only the in-range fast path uses a C float-to-integer cast. The tail
   projects the finite IEEE significand modulo 2^64 without UB or libm.
   Nonfinite unchecked inputs produce zero; checked inputs reject them. */
static uint64_t fg302ds_gg_float_integer_wrap(double d) {
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
static void fg302ds_gg_float_integer_fail(double d,int f32,const char *type,const char *file,int line,const char *why) {
    uint8_t text[64];int64_t n=f32?ggf_fmt_f32(text,(float)d):ggf_fmt_f64(text,d);
    fprintf(stderr,"goose runtime error: conversion %s to %s (%.*s) at %s:%d\\n",why,type,(int)n,(const char*)text,file,line);
    exit(1);
}
#endif
static uint64_t fg302ds_gg_float_integer(double d,int width,int unsign,int checked,int f32,const char *type,const char *file,int line) {
#if defined(GS_DEBUG) && GS_DEBUG
    if(checked) {
        int64_t value,lo,hi;
        if(unsign && width==64) {
            uint64_t u;
            if(!(d>=0.0 && d<18446744073709551616.0))
                fg302ds_gg_float_integer_fail(d,f32,type,file,line,"out of range");
            u=(uint64_t)d;
            if((double)u!=d)fg302ds_gg_float_integer_fail(d,f32,type,file,line,"changes the value");
            return u;
        }
        if(!(d>=-9223372036854775808.0 && d<9223372036854775808.0))
            fg302ds_gg_float_integer_fail(d,f32,type,file,line,"out of range");
        value=(int64_t)d;
        hi=(int64_t)((UINT64_C(1)<<(width-(unsign?0:1)))-1);
        lo=unsign?0:-1-hi;
        if(value<lo || value>hi)fg302ds_gg_float_integer_fail(d,f32,type,file,line,"out of range");
        if((double)value!=d)fg302ds_gg_float_integer_fail(d,f32,type,file,line,"changes the value");
        return (uint64_t)value;
    }
#else
    (void)width;(void)unsign;(void)checked;(void)f32;(void)type;(void)file;(void)line;
#endif
    return fg302ds_gg_float_integer_wrap(d);
}
static void fg302ds_gg_float(fg302ds_gg_reference out,double n) { uint8_t b[64];int64_t k=ggf_fmt_f64(b,n);fg302ds_gg_append(out,b,k,1); }
static void fg302ds_gg_float32(fg302ds_gg_reference out,float n) { uint8_t b[64];int64_t k=ggf_fmt_f32(b,n);fg302ds_gg_append(out,b,k,1); }
static void fg302ds_gg_bool(fg302ds_gg_reference out,uint8_t n) { fg302ds_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg302ds_gg_write(FILE *file,fg302ds_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg302ds_gg_fail("write failed"); }
static char *fg302ds_gg_cstring(fg302ds_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg302ds_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t gs_os_read_file(fg302ds_gg_slice path,fg302ds_gg_reference out) {
    char *p=fg302ds_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg302ds_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t gs_os_write_file(fg302ds_gg_slice path,fg302ds_gg_slice data) {
    char *p=fg302ds_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t gs_os_file_exists(fg302ds_gg_slice path) { char*p=fg302ds_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void gs_os_write_stdout(fg302ds_gg_slice s) { fg302ds_gg_write(stdout,s); }
static void gs_os_write_stderr(fg302ds_gg_slice s) { fg302ds_gg_write(stderr,s); }
static void gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t gs_os_arg_count(void) { return fg302ds_gg_argc; }
static void gs_os_arg(int64_t i,fg302ds_gg_reference out) { if(i>=0 && i<fg302ds_gg_argc)fg302ds_gg_append(out,fg302ds_gg_argv[i],(int64_t)strlen(fg302ds_gg_argv[i]),1); }
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


static void fg302ds_gg_quote(fg302ds_gg_reference out,fg302ds_gg_slice text) { int64_t i; unsigned char c; fg302ds_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg302ds_gg_append(out,"\\",1,1);fg302ds_gg_append(out,&c,1,1);} else if(c=='\n')fg302ds_gg_append(out,"\\n",2,1);else if(c=='\r')fg302ds_gg_append(out,"\\r",2,1);else if(c=='\t')fg302ds_gg_append(out,"\\t",2,1);else fg302ds_gg_append(out,&c,1,1);}fg302ds_gg_append(out,"\"",1,1);}
static uint8_t fg302ds_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg302ds_gg_equal_elements_1(fg302ds_gg_slice a, fg302ds_gg_slice b);
static void fg302ds_gg_render_1(fg302ds_gg_reference out, int64_t value);
static uint8_t fg302ds_gg_eq_2(uint8_t a, uint8_t b);
static uint8_t fg302ds_gg_equal_elements_2(fg302ds_gg_slice a, fg302ds_gg_slice b);
static void fg302ds_gg_render_2(fg302ds_gg_reference out, uint8_t value);
static uint8_t fg302ds_gg_eq_3(uint64_t a, uint64_t b);
static uint8_t fg302ds_gg_equal_elements_3(fg302ds_gg_slice a, fg302ds_gg_slice b);
static void fg302ds_gg_render_3(fg302ds_gg_reference out, uint64_t value);
static uint8_t fg302ds_gg_eq_5(uint8_t a, uint8_t b);
static uint8_t fg302ds_gg_equal_elements_5(fg302ds_gg_slice a, fg302ds_gg_slice b);
static void fg302ds_gg_render_5(fg302ds_gg_reference out, uint8_t value);
static uint8_t fg302ds_gg_equal_elements_1(fg302ds_gg_slice a,fg302ds_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg302ds_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg302ds_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg302ds_gg_render_1(fg302ds_gg_reference out, int64_t value) {
fg302ds_gg_integer(out,value);
}
static uint8_t fg302ds_gg_equal_elements_2(fg302ds_gg_slice a,fg302ds_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg302ds_gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg302ds_gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void fg302ds_gg_render_2(fg302ds_gg_reference out, uint8_t value) {
fg302ds_gg_bool(out,value);
}
static uint8_t fg302ds_gg_equal_elements_3(fg302ds_gg_slice a,fg302ds_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg302ds_gg_eq_3(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg302ds_gg_eq_3(uint64_t a, uint64_t b) {
return a == b;
}
static void fg302ds_gg_render_3(fg302ds_gg_reference out, uint64_t value) {
fg302ds_gg_unsigned(out,value);
}
static uint8_t fg302ds_gg_equal_elements_5(fg302ds_gg_slice a,fg302ds_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg302ds_gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg302ds_gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void fg302ds_gg_render_5(fg302ds_gg_reference out, uint8_t value) {
fg302ds_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg302ds_gg_globals;
static void fg302ds_gg_function_0(int64_t base);
static uint8_t fg302ds_gg_function_1(int64_t p0, int64_t base);
static void fg302ds_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static uint8_t fg302ds_gg_function_5(int64_t p0, uint64_t p1, int64_t base);
static int64_t fg302ds_gg_function_6(uint64_t p0, int64_t p1, int64_t base);
static void fg302ds_gg_function_8(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t fg302ds_gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t fg302ds_gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, int64_t p5, uint64_t p6, uint64_t p7, int64_t p8, int64_t p9, int64_t base);
static uint8_t fg302ds_gg_function_12(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, uint64_t p4, int64_t base);
static int64_t fg302ds_gg_function_13(int64_t p0, int64_t base);
static int64_t fg302ds_gg_function_14(int64_t p0, int64_t p1, uint8_t p2, int64_t base);
static void fg302ds_gg_function_8(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  extern void (fg302k_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg302k_st_i64(t0, t3, t4);
  }
fg302ds_gg_done:;
}
static int64_t fg302ds_gg_function_14(int64_t p0, int64_t p1, uint8_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t v0;
  int64_t v1;
  uint8_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg302ds_gg_done;
  }
  }
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t6 = ((t4) <= (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg302ds_gg_done;
  }
  }
  t7 = v2;
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg302ds_gg_done;
  }
  }
  t0 = ((int64_t)(v1));
  goto fg302ds_gg_done;
  }
fg302ds_gg_done:;
  return t0;
}
static int64_t fg302ds_gg_function_6(uint64_t p0, int64_t p1, int64_t base) {
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
  extern int64_t (fg302k_ld_i64)(uint64_t, int64_t);
#endif
  t5 = fg302k_ld_i64(t1, t4);
  t0 = ((int64_t)(t5));
  goto fg302ds_gg_done;
  }
fg302ds_gg_done:;
  return t0;
}
static int64_t fg302ds_gg_function_13(int64_t p0, int64_t base) {
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
  v1 = ((int64_t)(((int64_t)fg302ds_gg_div(t3, t4))));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  goto fg302ds_gg_done;
  }
fg302ds_gg_done:;
  return t0;
}
static int64_t fg302ds_gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
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
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg302ds_gg_done;
  }
  }
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t6 = ((t4) < (t5));
  if (t6) {
  {
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t7) - (uint64_t)(t8)))));
  goto fg302ds_gg_done;
  }
  }
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v0));
  t11 = ((t9) >= (t10));
  if (t11) {
  {
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t12) - (uint64_t)(t13)))));
  goto fg302ds_gg_done;
  }
  }
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(v2));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  goto fg302ds_gg_done;
  }
fg302ds_gg_done:;
  return t0;
}
static uint8_t fg302ds_gg_function_12(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, uint64_t p4, int64_t base) {
  uint8_t t0;
  uint64_t t1;
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
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  uint64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((uint64_t)(v4));
  t2 = ((int64_t)(v0));
  v5 = fg302ds_gg_function_6(t1, t2, base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v6));
  t4 = ((int64_t)(v5));
  if (!(((t3) < (t4)))) goto label2;
  {
  t5 = ((uint64_t)(v3));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = fg302ds_gg_function_6(t5, t10, base + 0);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(v1));
  t14 = ((t12) == (t13));
  if (t14) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg302ds_gg_done;
  }
  }
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(0));
  goto fg302ds_gg_done;
  }
fg302ds_gg_done:;
  return t0;
}
static int64_t fg302ds_gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, int64_t p5, uint64_t p6, uint64_t p7, int64_t p8, int64_t p9, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  int64_t v5;
  uint64_t v6;
  uint64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
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
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v9));
  t5 = ((int64_t)(v3));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  goto fg302ds_gg_done;
  }
  }
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t8 = ((int64_t)(v11));
  t9 = ((int64_t)(v3));
  if (!(((t8) < (t9)))) goto label2;
  {
  t10 = ((uint64_t)(v4));
  t11 = ((int64_t)(v11));
  t12 = fg302ds_gg_function_6(t10, t11, base + 0);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(v0));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v11));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(v5));
  t19 = ((uint64_t)(v6));
  t20 = ((uint64_t)(v7));
  t21 = fg302ds_gg_function_12(t16, t17, t18, t19, t20, base + 0);
  t22 = t21;
  if (t22) {
  {
  v10 = ((int64_t)(v11));
  }
  }
  }
  }
  t23 = ((int64_t)(v11));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))));
  }
  goto label1;
label2:;
  t25 = ((int64_t)(v10));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t27 = ((t25) >= (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v9));
  t29 = ((int64_t)(v10));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  goto fg302ds_gg_done;
  }
  }
  t30 = ((int64_t)(v8));
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(v2));
  v12 = fg302ds_gg_function_10(t30, t31, t32, base + 0);
  t33 = ((int64_t)(v12));
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)(((int64_t)fg302ds_gg_mod(t34, t35))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t36)))));
  goto fg302ds_gg_done;
  }
fg302ds_gg_done:;
  return t0;
}
static void fg302ds_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg302k_fail)(int64_t);
#endif
  fg302k_fail(t2);
  }
  }
  }
fg302ds_gg_done:;
}
static uint8_t fg302ds_gg_function_5(int64_t p0, uint64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(v0));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((uint64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = fg302ds_gg_function_6(t3, t4, base + 0);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t8 = ((t6) > (t7));
  if (t8) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg302ds_gg_done;
  }
  }
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(0));
  goto fg302ds_gg_done;
  }
fg302ds_gg_done:;
  return t0;
}
static uint8_t fg302ds_gg_function_1(int64_t p0, int64_t base) {
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
  uint8_t t63;
  uint8_t t64;
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
  uint8_t t76;
  int64_t t77;
  int64_t t78;
  uint8_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint64_t t107;
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
  int64_t t118;
  uint64_t t119;
  int64_t t120;
  int64_t t121;
  uint64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  uint64_t t131;
  int64_t t132;
  uint64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  uint8_t t138;
  uint64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  uint64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint8_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  uint64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  uint8_t t164;
  int64_t t165;
  int64_t t166;
  uint64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  uint64_t t171;
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
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  uint64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  uint8_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  uint8_t t200;
  uint64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  uint64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  uint64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  uint64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  uint8_t t231;
  uint64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  uint64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  uint8_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  uint64_t t262;
  int64_t t263;
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
  uint64_t t278;
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
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  uint64_t t294;
  int64_t t295;
  uint64_t t296;
  uint64_t t297;
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
  int64_t t310;
  int64_t t311;
  int64_t t312;
  uint8_t t313;
  int64_t t314;
  int64_t t315;
  uint8_t t316;
  uint8_t t317;
  int64_t t318;
  int64_t t319;
  uint8_t t320;
  int64_t t321;
  int64_t t322;
  uint8_t t323;
  uint8_t t324;
  uint8_t t325;
  uint64_t t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  uint8_t t331;
  int64_t t332;
  int64_t t333;
  uint8_t t334;
  int64_t t335;
  uint64_t t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  int64_t t340;
  int64_t t341;
  int64_t t342;
  uint64_t t343;
  int64_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  uint64_t t349;
  int64_t t350;
  uint64_t t351;
  uint64_t t352;
  int64_t t353;
  int64_t t354;
  uint64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t t359;
  int64_t t360;
  int64_t t361;
  uint64_t t362;
  int64_t t363;
  int64_t t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  uint64_t t369;
  int64_t t370;
  int64_t t371;
  int64_t t372;
  int64_t t373;
  int64_t t374;
  int64_t t375;
  int64_t t376;
  int64_t t377;
  uint8_t t378;
  int64_t t379;
  int64_t t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  int64_t t384;
  int64_t t385;
  int64_t t386;
  uint64_t t387;
  int64_t t388;
  uint64_t t389;
  int64_t t390;
  int64_t t391;
  int64_t t392;
  int64_t t393;
  int64_t t394;
  uint8_t t395;
  uint8_t t396;
  int64_t t397;
  int64_t t398;
  int64_t t399;
  int64_t t400;
  uint8_t t401;
  int64_t t402;
  int64_t t403;
  int64_t t404;
  uint8_t t405;
  uint8_t t406;
  uint8_t t407;
  uint64_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  uint8_t t413;
  int64_t t414;
  int64_t t415;
  int64_t t416;
  uint64_t t417;
  uint64_t t418;
  uint8_t t419;
  uint8_t t420;
  int64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  uint8_t t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  int64_t t429;
  uint64_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  int64_t t434;
  int64_t t435;
  uint64_t t436;
  int64_t t437;
  uint64_t t438;
  uint64_t t439;
  int64_t t440;
  int64_t t441;
  uint64_t t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  int64_t t446;
  int64_t t447;
  int64_t t448;
  uint64_t t449;
  int64_t t450;
  int64_t t451;
  int64_t t452;
  int64_t t453;
  int64_t t454;
  int64_t t455;
  uint64_t t456;
  int64_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  int64_t t461;
  int64_t t462;
  int64_t t463;
  int64_t t464;
  int64_t t465;
  int64_t t466;
  uint8_t t467;
  int64_t t468;
  int64_t t469;
  uint8_t t470;
  int64_t t471;
  int64_t t472;
  uint8_t t473;
  int64_t t474;
  int64_t t475;
  uint8_t t476;
  int64_t t477;
  int64_t t478;
  int64_t t479;
  uint8_t t480;
  uint8_t t481;
  uint8_t t482;
  uint64_t t483;
  int64_t t484;
  int64_t t485;
  int64_t t486;
  int64_t t487;
  uint8_t t488;
  int64_t t489;
  int64_t t490;
  uint8_t t491;
  int64_t t492;
  uint64_t t493;
  int64_t t494;
  int64_t t495;
  int64_t t496;
  int64_t t497;
  int64_t t498;
  int64_t t499;
  uint64_t t500;
  int64_t t501;
  int64_t t502;
  int64_t t503;
  int64_t t504;
  int64_t t505;
  uint64_t t506;
  int64_t t507;
  uint64_t t508;
  uint64_t t509;
  int64_t t510;
  int64_t t511;
  uint64_t t512;
  int64_t t513;
  int64_t t514;
  int64_t t515;
  int64_t t516;
  int64_t t517;
  int64_t t518;
  uint64_t t519;
  int64_t t520;
  int64_t t521;
  int64_t t522;
  int64_t t523;
  int64_t t524;
  int64_t t525;
  uint64_t t526;
  int64_t t527;
  int64_t t528;
  int64_t t529;
  int64_t t530;
  int64_t t531;
  int64_t t532;
  int64_t t533;
  int64_t t534;
  int64_t t535;
  uint8_t t536;
  int64_t t537;
  int64_t t538;
  uint64_t t539;
  int64_t t540;
  int64_t t541;
  int64_t t542;
  uint64_t t543;
  int64_t t544;
  int64_t t545;
  int64_t t546;
  int64_t t547;
  int64_t t548;
  uint64_t t549;
  int64_t t550;
  int64_t t551;
  int64_t t552;
  int64_t t553;
  int64_t t554;
  int64_t t555;
  int64_t t556;
  int64_t t557;
  int64_t t558;
  uint8_t t559;
  uint8_t t560;
  uint8_t t561;
  uint64_t t562;
  int64_t t563;
  int64_t t564;
  int64_t t565;
  int64_t t566;
  int64_t t567;
  uint8_t t568;
  uint64_t t569;
  int64_t t570;
  int64_t t571;
  int64_t t572;
  int64_t t573;
  int64_t t574;
  int64_t t575;
  uint64_t t576;
  int64_t t577;
  int64_t t578;
  int64_t t579;
  int64_t t580;
  int64_t t581;
  int64_t t582;
  int64_t t583;
  uint64_t t584;
  int64_t t585;
  int64_t t586;
  int64_t t587;
  int64_t t588;
  int64_t t589;
  int64_t t590;
  uint64_t t591;
  int64_t t592;
  int64_t t593;
  int64_t t594;
  int64_t t595;
  int64_t t596;
  int64_t t597;
  int64_t t598;
  uint64_t t599;
  int64_t t600;
  int64_t t601;
  int64_t t602;
  int64_t t603;
  int64_t t604;
  int64_t t605;
  int64_t t606;
  int64_t t607;
  uint64_t t608;
  int64_t t609;
  int64_t t610;
  int64_t t611;
  int64_t t612;
  int64_t t613;
  int64_t t614;
  uint64_t t615;
  int64_t t616;
  int64_t t617;
  int64_t t618;
  int64_t t619;
  int64_t t620;
  int64_t t621;
  int64_t t622;
  int64_t t623;
  uint64_t t624;
  int64_t t625;
  int64_t t626;
  int64_t t627;
  int64_t t628;
  int64_t t629;
  int64_t t630;
  int64_t t631;
  int64_t t632;
  int64_t t633;
  int64_t t634;
  int64_t t635;
  int64_t t636;
  uint64_t t637;
  int64_t t638;
  uint64_t t639;
  int64_t t640;
  int64_t t641;
  int64_t t642;
  int64_t t643;
  int64_t t644;
  int64_t t645;
  uint64_t t646;
  int64_t t647;
  uint64_t t648;
  int64_t t649;
  int64_t t650;
  int64_t t651;
  int64_t t652;
  int64_t t653;
  int64_t t654;
  uint64_t t655;
  int64_t t656;
  int64_t t657;
  uint64_t t658;
  int64_t t659;
  int64_t t660;
  uint64_t t661;
  int64_t t662;
  int64_t t663;
  uint64_t t664;
  int64_t t665;
  int64_t t666;
  uint64_t t667;
  int64_t t668;
  int64_t t669;
  uint64_t t670;
  int64_t t671;
  int64_t t672;
  int64_t t673;
  uint8_t t674;
  int64_t t675;
  int64_t t676;
  int64_t t677;
  int64_t t678;
  uint64_t t679;
  int64_t t680;
  int64_t t681;
  int64_t t682;
  uint64_t t683;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  uint64_t v7;
  uint64_t v8;
  uint64_t v9;
  int64_t v10;
  int64_t v11;
  uint64_t v12;
  uint64_t v13;
  uint64_t v14;
  int64_t v15;
  int64_t v16;
  uint8_t v17;
  uint64_t v18;
  uint64_t v19;
  uint64_t v20;
  uint64_t v21;
  uint64_t v22;
  uint64_t v23;
  uint64_t v24;
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
  uint8_t v47;
  int64_t v48;
  int64_t v49;
  int64_t v50;
  int64_t v51;
  int64_t v52;
  int64_t v53;
  int64_t v54;
  int64_t v55;
  int64_t v56;
  uint8_t v57;
  int64_t v58;
  uint8_t v59;
  int64_t v60;
  int64_t v61;
  int64_t v62;
  int64_t v63;
  uint8_t v64;
  int64_t v65;
  int64_t v66;
  int64_t v67;
  int64_t v68;
  int64_t v69;
  int64_t v70;
  int64_t v71;
  int64_t v72;
  int64_t v73;
  uint8_t v74;
  int64_t v75;
  int64_t v76;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t4 = fg302k_get(t1, t2, t3);
  v1 = ((uint64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t8 = fg302k_get(t5, t6, t7);
  v2 = ((int64_t)(((int64_t)(t8))));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(3))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t12 = fg302k_get(t9, t10, t11);
  v3 = ((int64_t)(((int64_t)(t12))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t16 = fg302k_get(t13, t14, t15);
  v4 = ((int64_t)(((int64_t)(t16))));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(5))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t20 = fg302k_get(t17, t18, t19);
  v5 = ((int64_t)(((int64_t)(t20))));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(6))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t24 = fg302k_get(t21, t22, t23);
  v6 = ((int64_t)(((int64_t)(t24))));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((int64_t)(((int64_t)UINT64_C(7))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t28 = fg302k_get(t25, t26, t27);
  v7 = ((uint64_t)(t28));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t32 = fg302k_get(t29, t30, t31);
  v8 = ((uint64_t)(t32));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(9))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t36 = fg302k_get(t33, t34, t35);
  v9 = ((uint64_t)(t36));
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((int64_t)(((int64_t)UINT64_C(10))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t40 = fg302k_get(t37, t38, t39);
  v10 = ((int64_t)(((int64_t)(t40))));
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((int64_t)(((int64_t)UINT64_C(11))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t44 = fg302k_get(t41, t42, t43);
  v11 = ((int64_t)(((int64_t)(t44))));
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((int64_t)(((int64_t)UINT64_C(12))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t48 = fg302k_get(t45, t46, t47);
  v12 = ((uint64_t)(t48));
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((int64_t)(((int64_t)UINT64_C(13))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t52 = fg302k_get(t49, t50, t51);
  v13 = ((uint64_t)(t52));
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((int64_t)(((int64_t)UINT64_C(14))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t56 = fg302k_get(t53, t54, t55);
  v14 = ((uint64_t)(t56));
  t57 = ((int64_t)(v0));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(((int64_t)UINT64_C(15))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t60 = fg302k_get(t57, t58, t59);
  v15 = ((int64_t)(((int64_t)(t60))));
  t61 = ((int64_t)(v2));
  t62 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t63 = ((t61) > (t62));
  t64 = t63;
  if (t64) {
  t65 = ((int64_t)(v3));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t64 = ((t65) > (t66));
  }
  t67 = t64;
  t68 = t67;
  if (t68) {
  t69 = ((int64_t)(v4));
  t70 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t68 = ((t69) > (t70));
  }
  t71 = t68;
  t72 = ((int64_t)(((int64_t)UINT64_C(60))));
  fg302ds_gg_function_3(t71, t72, base + 0);
  t73 = ((int64_t)(v5));
  t74 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t75 = ((t73) > (t74));
  t76 = t75;
  if (t76) {
  t77 = ((int64_t)(v10));
  t78 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t76 = ((t77) > (t78));
  }
  t79 = t76;
  t80 = ((int64_t)(((int64_t)UINT64_C(61))));
  fg302ds_gg_function_3(t79, t80, base + 0);
  t81 = ((int64_t)(v6));
  t82 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t83 = ((t81) <= (t82));
  if (t83) {
  {
  t84 = ((int64_t)(v11));
  t85 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t86 = ((t84) > (t85));
  if (t86) {
  {
  t87 = ((int64_t)(((int64_t)UINT64_C(62))));
#ifndef __cplusplus
  extern void (fg302k_fail)(int64_t);
#endif
  fg302k_fail(t87);
  }
  }
  }
  }
  t88 = ((int64_t)(v4));
  t89 = ((int64_t)(v6));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t88) * (uint64_t)(t89)))));
  t90 = ((int64_t)(v2));
  t91 = ((uint64_t)(v8));
  v17 = fg302ds_gg_function_5(t90, t91, base + 0);
  t92 = ((int64_t)(v0));
  t93 = ((int64_t)(((int64_t)UINT64_C(1))));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t95 = fg302k_get(t92, t93, t94);
  v18 = ((uint64_t)(t95));
  t96 = ((int64_t)(v0));
  t97 = ((int64_t)(((int64_t)UINT64_C(1))));
  t98 = ((int64_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t99 = fg302k_get(t96, t97, t98);
  v19 = ((uint64_t)(t99));
  t100 = ((int64_t)(v0));
  t101 = ((int64_t)(((int64_t)UINT64_C(1))));
  t102 = ((int64_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t103 = fg302k_get(t100, t101, t102);
  v20 = ((uint64_t)(t103));
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(((int64_t)UINT64_C(1))));
  t106 = ((int64_t)(((int64_t)UINT64_C(3))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t107 = fg302k_get(t104, t105, t106);
  v21 = ((uint64_t)(t107));
  t108 = ((int64_t)(v0));
  t109 = ((int64_t)(((int64_t)UINT64_C(1))));
  t110 = ((int64_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t111 = fg302k_get(t108, t109, t110);
  v22 = ((uint64_t)(t111));
  t112 = ((int64_t)(v0));
  t113 = ((int64_t)(((int64_t)UINT64_C(1))));
  t114 = ((int64_t)(((int64_t)UINT64_C(5))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t115 = fg302k_get(t112, t113, t114);
  v23 = ((uint64_t)(t115));
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(((int64_t)UINT64_C(1))));
  t118 = ((int64_t)(((int64_t)UINT64_C(6))));
#ifndef __cplusplus
  extern uint64_t (fg302k_get)(int64_t, int64_t, int64_t);
#endif
  t119 = fg302k_get(t116, t117, t118);
  v24 = ((uint64_t)(t119));
  v25 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t120 = ((int64_t)(v25));
  t121 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  if (!(((t120) < (t121)))) goto label2;
  {
  t122 = ((uint64_t)(v13));
  t123 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(512))))));
  t124 = ((int64_t)(v25));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) + (uint64_t)(t124)))));
  t126 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  fg302ds_gg_function_8(t122, t125, t126, base + 0);
  t127 = ((int64_t)(v25));
  t128 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t127) + (uint64_t)(t128)))));
  }
  goto label1;
label2:;
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
  v28 = ((int64_t)(((int64_t)UINT64_C(0))));
  v29 = ((int64_t)(((int64_t)UINT64_C(0))));
  v30 = ((int64_t)(((int64_t)UINT64_C(0))));
  v31 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t129 = ((int64_t)(v31));
  t130 = ((int64_t)(v2));
  if (!(((t129) < (t130)))) goto label5;
  {
  t131 = ((uint64_t)(v7));
  t132 = ((int64_t)(v31));
  v32 = fg302ds_gg_function_6(t131, t132, base + 0);
  t133 = ((uint64_t)(v18));
  t134 = ((int64_t)(v31));
  t135 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  fg302ds_gg_function_8(t133, t134, t135, base + 0);
  t136 = ((int64_t)(v32));
  t137 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t138 = ((t136) >= (t137));
  if (t138) {
  {
  t139 = ((uint64_t)(v19));
  t140 = ((int64_t)(v26));
  t141 = ((int64_t)(v32));
  t142 = ((int64_t)(v3));
  t143 = ((int64_t)(((int64_t)fg302ds_gg_mod(t141, t142))));
  fg302ds_gg_function_8(t139, t140, t143, base + 0);
  t144 = ((int64_t)(v26));
  t145 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t144) + (uint64_t)(t145)))));
  t146 = ((int64_t)(v32));
  t147 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t146) + (uint64_t)(t147)))));
  t149 = ((int64_t)(v3));
  v33 = ((int64_t)(((int64_t)fg302ds_gg_div(t148, t149))));
  t150 = ((uint64_t)(v18));
  t151 = ((int64_t)(v31));
  t152 = ((int64_t)(v33));
  fg302ds_gg_function_8(t150, t151, t152, base + 0);
  t153 = ((int64_t)(v33));
  t154 = ((int64_t)(v30));
  t155 = ((t153) > (t154));
  if (t155) {
  {
  v30 = ((int64_t)(v33));
  }
  }
  }
  }
  t156 = ((int64_t)(v31));
  t157 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t156) + (uint64_t)(t157)))));
  }
  goto label4;
label5:;
  v31 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t158 = ((int64_t)(v31));
  t159 = ((int64_t)(v2));
  if (!(((t158) < (t159)))) goto label8;
  {
  t160 = ((uint64_t)(v7));
  t161 = ((int64_t)(v31));
  v34 = fg302ds_gg_function_6(t160, t161, base + 0);
  t162 = ((int64_t)(v34));
  t163 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t164 = ((t162) < (t163));
  if (t164) {
  {
  t165 = ((int64_t)(v31));
  t166 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t165) + (uint64_t)(t166)))));
  }
  } else {
  {
  t167 = ((uint64_t)(v8));
  t168 = ((int64_t)(v31));
  v35 = fg302ds_gg_function_6(t167, t168, base + 0);
  v36 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t169 = ((int64_t)(v36));
  t170 = ((int64_t)(v35));
  if (!(((t169) < (t170)))) goto label11;
  {
  t171 = ((uint64_t)(v9));
  t172 = ((int64_t)(v31));
  t173 = ((int64_t)(v10));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t172) * (uint64_t)(t173)))));
  t175 = ((int64_t)(v36));
  t176 = ((int64_t)(((int64_t)((uint64_t)(t174) + (uint64_t)(t175)))));
  v37 = fg302ds_gg_function_6(t171, t176, base + 0);
  t177 = ((int64_t)(v6));
  t178 = ((int64_t)(v37));
  t179 = ((int64_t)(v4));
  v38 = fg302ds_gg_function_10(t177, t178, t179, base + 0);
  t180 = ((int64_t)(v38));
  t181 = ((int64_t)(v34));
  t182 = ((int64_t)(v4));
  t183 = ((int64_t)(((int64_t)fg302ds_gg_mod(t181, t182))));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t180) + (uint64_t)(t183)))));
  t184 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t185 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t184) - (uint64_t)(t185)))));
  v41 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t186 = ((int64_t)(v41));
  t187 = ((int64_t)(v29));
  if (!(((t186) < (t187)))) goto label14;
  {
  t188 = ((uint64_t)(v13));
  t189 = ((int64_t)(v41));
  t190 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t191 = ((int64_t)(((int64_t)((uint64_t)(t189) * (uint64_t)(t190)))));
  t192 = fg302ds_gg_function_6(t188, t191, base + 0);
  t193 = ((int64_t)(t192));
  t194 = ((int64_t)(v39));
  t195 = ((t193) == (t194));
  if (t195) {
  {
  v40 = ((int64_t)(v41));
  }
  }
  t196 = ((int64_t)(v41));
  t197 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v41 = ((int64_t)(((int64_t)((uint64_t)(t196) + (uint64_t)(t197)))));
  }
  goto label13;
label14:;
  t198 = ((int64_t)(v40));
  t199 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t200 = ((t198) < (t199));
  if (t200) {
  {
  t201 = ((uint64_t)(v13));
  t202 = ((int64_t)(v29));
  t203 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t204 = ((int64_t)(((int64_t)((uint64_t)(t202) * (uint64_t)(t203)))));
  t205 = ((int64_t)(v39));
  fg302ds_gg_function_8(t201, t204, t205, base + 0);
  t206 = ((uint64_t)(v13));
  t207 = ((int64_t)(v29));
  t208 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t209 = ((int64_t)(((int64_t)((uint64_t)(t207) * (uint64_t)(t208)))));
  t210 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t211 = ((int64_t)(((int64_t)((uint64_t)(t209) + (uint64_t)(t210)))));
  t212 = ((int64_t)(v31));
  fg302ds_gg_function_8(t206, t211, t212, base + 0);
  t213 = ((uint64_t)(v13));
  t214 = ((int64_t)(v29));
  t215 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t216 = ((int64_t)(((int64_t)((uint64_t)(t214) * (uint64_t)(t215)))));
  t217 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t218 = ((int64_t)(((int64_t)((uint64_t)(t216) + (uint64_t)(t217)))));
  t219 = ((int64_t)(v34));
  fg302ds_gg_function_8(t213, t218, t219, base + 0);
  t220 = ((int64_t)(v29));
  t221 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t220) + (uint64_t)(t221)))));
  }
  } else {
  {
  t222 = ((int64_t)(v34));
  t223 = ((uint64_t)(v13));
  t224 = ((int64_t)(v40));
  t225 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t226 = ((int64_t)(((int64_t)((uint64_t)(t224) * (uint64_t)(t225)))));
  t227 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t228 = ((int64_t)(((int64_t)((uint64_t)(t226) + (uint64_t)(t227)))));
  t229 = fg302ds_gg_function_6(t223, t228, base + 0);
  t230 = ((int64_t)(t229));
  t231 = ((t222) > (t230));
  if (t231) {
  {
  t232 = ((uint64_t)(v13));
  t233 = ((int64_t)(v40));
  t234 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t235 = ((int64_t)(((int64_t)((uint64_t)(t233) * (uint64_t)(t234)))));
  t236 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t237 = ((int64_t)(((int64_t)((uint64_t)(t235) + (uint64_t)(t236)))));
  t238 = ((int64_t)(v31));
  fg302ds_gg_function_8(t232, t237, t238, base + 0);
  t239 = ((uint64_t)(v13));
  t240 = ((int64_t)(v40));
  t241 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t242 = ((int64_t)(((int64_t)((uint64_t)(t240) * (uint64_t)(t241)))));
  t243 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t244 = ((int64_t)(((int64_t)((uint64_t)(t242) + (uint64_t)(t243)))));
  t245 = ((int64_t)(v34));
  fg302ds_gg_function_8(t239, t244, t245, base + 0);
  }
  }
  }
  }
  t246 = ((int64_t)(v34));
  t247 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t248 = ((int64_t)(((int64_t)((uint64_t)(t246) + (uint64_t)(t247)))));
  t249 = ((int64_t)(v3));
  t250 = ((int64_t)(((int64_t)fg302ds_gg_mod(t248, t249))));
  t251 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t252 = ((t250) != (t251));
  if (t252) {
  {
  t253 = ((int64_t)(v36));
  t254 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t253) + (uint64_t)(t254)))));
  }
  } else {
  {
  t255 = ((int64_t)(v34));
  t256 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t257 = ((int64_t)(((int64_t)((uint64_t)(t255) + (uint64_t)(t256)))));
  t258 = ((int64_t)(v3));
  v42 = ((int64_t)(((int64_t)((uint64_t)(t257) - (uint64_t)(t258)))));
  t259 = ((int64_t)(v6));
  t260 = ((int64_t)(v37));
  t261 = ((int64_t)(v5));
  v43 = fg302ds_gg_function_10(t259, t260, t261, base + 0);
  t262 = ((uint64_t)(v20));
  t263 = ((int64_t)(v27));
  t264 = ((int64_t)(v43));
  t265 = ((int64_t)(v34));
  t266 = ((int64_t)(v3));
  t267 = ((int64_t)(((int64_t)fg302ds_gg_div(t265, t266))));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t264) + (uint64_t)(t267)))));
  fg302ds_gg_function_8(t262, t263, t268, base + 0);
  t269 = ((uint64_t)(v21));
  t270 = ((int64_t)(v27));
  t271 = ((int64_t)(v42));
  fg302ds_gg_function_8(t269, t270, t271, base + 0);
  t272 = ((int64_t)(v27));
  t273 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t272) + (uint64_t)(t273)))));
  t274 = ((uint64_t)(v13));
  t275 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(512))))));
  t276 = ((int64_t)(v37));
  t277 = ((int64_t)(((int64_t)((uint64_t)(t275) + (uint64_t)(t276)))));
  t278 = ((uint64_t)(v13));
  t279 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(512))))));
  t280 = ((int64_t)(v37));
  t281 = ((int64_t)(((int64_t)((uint64_t)(t279) + (uint64_t)(t280)))));
  t282 = fg302ds_gg_function_6(t278, t281, base + 0);
  t283 = ((int64_t)(t282));
  t284 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t285 = ((int64_t)(((int64_t)((uint64_t)(t283) + (uint64_t)(t284)))));
  fg302ds_gg_function_8(t274, t277, t285, base + 0);
  v44 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t286 = ((int64_t)(v44));
  t287 = ((int64_t)(v3));
  if (!(((t286) < (t287)))) goto label17;
  {
  t288 = ((int64_t)(v42));
  t289 = ((int64_t)(v44));
  t290 = ((int64_t)(((int64_t)((uint64_t)(t288) + (uint64_t)(t289)))));
  t291 = ((int64_t)(v37));
  t292 = ((int64_t)(v4));
  t293 = ((int64_t)(v2));
  t294 = ((uint64_t)(v7));
  t295 = ((int64_t)(v10));
  t296 = ((uint64_t)(v9));
  t297 = ((uint64_t)(v8));
  t298 = ((int64_t)(v6));
  t299 = ((int64_t)(v16));
  v45 = fg302ds_gg_function_11(t290, t291, t292, t293, t294, t295, t296, t297, t298, t299, base + 0);
  t300 = ((uint64_t)(v22));
  t301 = ((int64_t)(v28));
  t302 = ((int64_t)(v45));
  fg302ds_gg_function_8(t300, t301, t302, base + 0);
  t303 = ((int64_t)(v28));
  t304 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t303) + (uint64_t)(t304)))));
  t305 = ((int64_t)(v44));
  t306 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v44 = ((int64_t)(((int64_t)((uint64_t)(t305) + (uint64_t)(t306)))));
  }
  goto label16;
label17:;
  t307 = ((int64_t)(v36));
  t308 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t307) + (uint64_t)(t308)))));
  }
  }
  }
  goto label10;
label11:;
  t309 = ((int64_t)(v31));
  t310 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t309) + (uint64_t)(t310)))));
  }
  }
  }
  goto label7;
label8:;
  t311 = ((int64_t)(v3));
  t312 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t313 = ((t311) == (t312));
  if (t313) {
  {
  t314 = ((int64_t)(v26));
  t315 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t316 = ((t314) > (t315));
  if (t316) {
  {
  t317 = v17;
  if (t317) {
  {
  t318 = ((int64_t)(v27));
  t319 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t320 = ((t318) == (t319));
  if (t320) {
  {
  v46 = ((int64_t)(((int64_t)UINT64_C(0))));
  v47 = ((uint8_t)UINT64_C(0));
label19:;
  t321 = ((int64_t)(v46));
  t322 = ((int64_t)(v2));
  t323 = ((t321) < (t322));
  t324 = t323;
  if (t324) {
  t325 = v47;
  t324 = (!(t325));
  }
  if (!(t324)) goto label20;
  {
  t326 = ((uint64_t)(v7));
  t327 = ((int64_t)(v46));
  t328 = fg302ds_gg_function_6(t326, t327, base + 0);
  t329 = ((int64_t)(t328));
  t330 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t331 = ((t329) >= (t330));
  if (t331) {
  {
  v47 = ((uint8_t)UINT64_C(1));
  }
  } else {
  {
  t332 = ((int64_t)(v46));
  t333 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v46 = ((int64_t)(((int64_t)((uint64_t)(t332) + (uint64_t)(t333)))));
  }
  }
  }
  goto label19;
label20:;
  t334 = v47;
  t335 = ((int64_t)(((int64_t)UINT64_C(63))));
  fg302ds_gg_function_3(t334, t335, base + 0);
  t336 = ((uint64_t)(v9));
  t337 = ((int64_t)(v46));
  t338 = ((int64_t)(v10));
  t339 = ((int64_t)(((int64_t)((uint64_t)(t337) * (uint64_t)(t338)))));
  v48 = fg302ds_gg_function_6(t336, t339, base + 0);
  t340 = ((int64_t)(v6));
  t341 = ((int64_t)(v48));
  t342 = ((int64_t)(v5));
  v49 = fg302ds_gg_function_10(t340, t341, t342, base + 0);
  t343 = ((uint64_t)(v7));
  t344 = ((int64_t)(v46));
  t345 = fg302ds_gg_function_6(t343, t344, base + 0);
  t346 = ((int64_t)(v48));
  t347 = ((int64_t)(v4));
  t348 = ((int64_t)(v2));
  t349 = ((uint64_t)(v7));
  t350 = ((int64_t)(v10));
  t351 = ((uint64_t)(v9));
  t352 = ((uint64_t)(v8));
  t353 = ((int64_t)(v6));
  t354 = ((int64_t)(v16));
  v50 = fg302ds_gg_function_11(t345, t346, t347, t348, t349, t350, t351, t352, t353, t354, base + 0);
  t355 = ((uint64_t)(v20));
  t356 = ((int64_t)(v27));
  t357 = ((int64_t)(v49));
  t358 = ((int64_t)(v5));
  t359 = ((int64_t)(((int64_t)((uint64_t)(t357) + (uint64_t)(t358)))));
  t360 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t361 = ((int64_t)(((int64_t)((uint64_t)(t359) - (uint64_t)(t360)))));
  fg302ds_gg_function_8(t355, t356, t361, base + 0);
  t362 = ((uint64_t)(v21));
  t363 = ((int64_t)(v27));
  t364 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  fg302ds_gg_function_8(t362, t363, t364, base + 0);
  t365 = ((int64_t)(v27));
  t366 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t365) + (uint64_t)(t366)))));
  v51 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t367 = ((int64_t)(v51));
  t368 = ((int64_t)(v3));
  if (!(((t367) < (t368)))) goto label23;
  {
  t369 = ((uint64_t)(v22));
  t370 = ((int64_t)(v28));
  t371 = ((int64_t)(v50));
  fg302ds_gg_function_8(t369, t370, t371, base + 0);
  t372 = ((int64_t)(v28));
  t373 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t372) + (uint64_t)(t373)))));
  t374 = ((int64_t)(v51));
  t375 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t374) + (uint64_t)(t375)))));
  }
  goto label22;
label23:;
  }
  }
  }
  } else {
  {
  v52 = ((int64_t)(v15));
  t376 = ((int64_t)(v52));
  t377 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t378 = ((t376) < (t377));
  if (t378) {
  {
  v52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t379 = ((int64_t)(v52));
  t380 = ((int64_t)(v3));
  t381 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t382 = ((int64_t)(((int64_t)((uint64_t)(t380) - (uint64_t)(t381)))));
  t383 = ((int64_t)(((int64_t)((uint64_t)(t379) + (uint64_t)(t382)))));
  t384 = ((int64_t)(v3));
  v53 = ((int64_t)(((int64_t)fg302ds_gg_div(t383, t384))));
  v54 = ((int64_t)(((int64_t)UINT64_C(0))));
label25:;
  t385 = ((int64_t)(v54));
  t386 = ((int64_t)(v11));
  if (!(((t385) < (t386)))) goto label26;
  {
  t387 = ((uint64_t)(v12));
  t388 = ((int64_t)(v54));
  v55 = fg302ds_gg_function_6(t387, t388, base + 0);
  t389 = ((uint64_t)(v13));
  t390 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(512))))));
  t391 = ((int64_t)(v55));
  t392 = ((int64_t)(((int64_t)((uint64_t)(t390) + (uint64_t)(t391)))));
  v56 = fg302ds_gg_function_6(t389, t392, base + 0);
  v57 = ((uint8_t)UINT64_C(1));
  t393 = ((int64_t)(v56));
  t394 = ((int64_t)(v53));
  t395 = ((t393) >= (t394));
  if (t395) {
  {
  v57 = ((uint8_t)UINT64_C(0));
  }
  }
  t396 = v57;
  if (t396) {
  {
  t397 = ((int64_t)(v56));
  t398 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t399 = ((int64_t)(((int64_t)((uint64_t)(t397) + (uint64_t)(t398)))));
  t400 = ((int64_t)(v53));
  t401 = ((t399) != (t400));
  if (t401) {
  {
  t402 = ((int64_t)(((int64_t)UINT64_C(64))));
#ifndef __cplusplus
  extern void (fg302k_fail)(int64_t);
#endif
  fg302k_fail(t402);
  }
  }
  v58 = ((int64_t)(((int64_t)UINT64_C(0))));
  v59 = ((uint8_t)UINT64_C(0));
label28:;
  t403 = ((int64_t)(v58));
  t404 = ((int64_t)(v2));
  t405 = ((t403) < (t404));
  t406 = t405;
  if (t406) {
  t407 = v59;
  t406 = (!(t407));
  }
  if (!(t406)) goto label29;
  {
  t408 = ((uint64_t)(v7));
  t409 = ((int64_t)(v58));
  t410 = fg302ds_gg_function_6(t408, t409, base + 0);
  t411 = ((int64_t)(t410));
  t412 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t413 = ((t411) >= (t412));
  if (t413) {
  {
  t414 = ((int64_t)(v58));
  t415 = ((int64_t)(v55));
  t416 = ((int64_t)(v10));
  t417 = ((uint64_t)(v9));
  t418 = ((uint64_t)(v8));
  t419 = fg302ds_gg_function_12(t414, t415, t416, t417, t418, base + 0);
  t420 = t419;
  if (t420) {
  {
  v59 = ((uint8_t)UINT64_C(1));
  }
  } else {
  {
  t421 = ((int64_t)(v58));
  t422 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v58 = ((int64_t)(((int64_t)((uint64_t)(t421) + (uint64_t)(t422)))));
  }
  }
  }
  } else {
  {
  t423 = ((int64_t)(v58));
  t424 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v58 = ((int64_t)(((int64_t)((uint64_t)(t423) + (uint64_t)(t424)))));
  }
  }
  }
  goto label28;
label29:;
  t425 = v59;
  t426 = ((int64_t)(((int64_t)UINT64_C(65))));
  fg302ds_gg_function_3(t425, t426, base + 0);
  t427 = ((int64_t)(v6));
  t428 = ((int64_t)(v55));
  t429 = ((int64_t)(v5));
  v60 = fg302ds_gg_function_10(t427, t428, t429, base + 0);
  t430 = ((uint64_t)(v7));
  t431 = ((int64_t)(v58));
  t432 = fg302ds_gg_function_6(t430, t431, base + 0);
  t433 = ((int64_t)(v55));
  t434 = ((int64_t)(v4));
  t435 = ((int64_t)(v2));
  t436 = ((uint64_t)(v7));
  t437 = ((int64_t)(v10));
  t438 = ((uint64_t)(v9));
  t439 = ((uint64_t)(v8));
  t440 = ((int64_t)(v6));
  t441 = ((int64_t)(v16));
  v61 = fg302ds_gg_function_11(t432, t433, t434, t435, t436, t437, t438, t439, t440, t441, base + 0);
  t442 = ((uint64_t)(v20));
  t443 = ((int64_t)(v27));
  t444 = ((int64_t)(v60));
  t445 = ((int64_t)(v5));
  t446 = ((int64_t)(((int64_t)((uint64_t)(t444) + (uint64_t)(t445)))));
  t447 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t448 = ((int64_t)(((int64_t)((uint64_t)(t446) - (uint64_t)(t447)))));
  fg302ds_gg_function_8(t442, t443, t448, base + 0);
  t449 = ((uint64_t)(v21));
  t450 = ((int64_t)(v27));
  t451 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  fg302ds_gg_function_8(t449, t450, t451, base + 0);
  t452 = ((int64_t)(v27));
  t453 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t452) + (uint64_t)(t453)))));
  v62 = ((int64_t)(((int64_t)UINT64_C(0))));
label31:;
  t454 = ((int64_t)(v62));
  t455 = ((int64_t)(v3));
  if (!(((t454) < (t455)))) goto label32;
  {
  t456 = ((uint64_t)(v22));
  t457 = ((int64_t)(v28));
  t458 = ((int64_t)(v61));
  fg302ds_gg_function_8(t456, t457, t458, base + 0);
  t459 = ((int64_t)(v28));
  t460 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t459) + (uint64_t)(t460)))));
  t461 = ((int64_t)(v62));
  t462 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v62 = ((int64_t)(((int64_t)((uint64_t)(t461) + (uint64_t)(t462)))));
  }
  goto label31;
label32:;
  }
  }
  t463 = ((int64_t)(v54));
  t464 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v54 = ((int64_t)(((int64_t)((uint64_t)(t463) + (uint64_t)(t464)))));
  }
  goto label25;
label26:;
  }
  }
  }
  }
  }
  }
  t465 = ((int64_t)(v3));
  t466 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(128))))));
  t467 = ((t465) == (t466));
  if (t467) {
  {
  t468 = ((int64_t)(v26));
  t469 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t470 = ((t468) > (t469));
  if (t470) {
  {
  t471 = ((int64_t)(v27));
  t472 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t473 = ((t471) == (t472));
  if (t473) {
  {
  t474 = ((int64_t)(v30));
  t475 = ((int64_t)(v5));
  t476 = ((t474) < (t475));
  t477 = ((int64_t)(((int64_t)UINT64_C(66))));
  fg302ds_gg_function_3(t476, t477, base + 0);
  v63 = ((int64_t)(((int64_t)UINT64_C(0))));
  v64 = ((uint8_t)UINT64_C(0));
label34:;
  t478 = ((int64_t)(v63));
  t479 = ((int64_t)(v2));
  t480 = ((t478) < (t479));
  t481 = t480;
  if (t481) {
  t482 = v64;
  t481 = (!(t482));
  }
  if (!(t481)) goto label35;
  {
  t483 = ((uint64_t)(v7));
  t484 = ((int64_t)(v63));
  t485 = fg302ds_gg_function_6(t483, t484, base + 0);
  t486 = ((int64_t)(t485));
  t487 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t488 = ((t486) >= (t487));
  if (t488) {
  {
  v64 = ((uint8_t)UINT64_C(1));
  }
  } else {
  {
  t489 = ((int64_t)(v63));
  t490 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v63 = ((int64_t)(((int64_t)((uint64_t)(t489) + (uint64_t)(t490)))));
  }
  }
  }
  goto label34;
label35:;
  t491 = v64;
  t492 = ((int64_t)(((int64_t)UINT64_C(67))));
  fg302ds_gg_function_3(t491, t492, base + 0);
  t493 = ((uint64_t)(v9));
  t494 = ((int64_t)(v63));
  t495 = ((int64_t)(v10));
  t496 = ((int64_t)(((int64_t)((uint64_t)(t494) * (uint64_t)(t495)))));
  v65 = fg302ds_gg_function_6(t493, t496, base + 0);
  t497 = ((int64_t)(v6));
  t498 = ((int64_t)(v65));
  t499 = ((int64_t)(v5));
  v66 = fg302ds_gg_function_10(t497, t498, t499, base + 0);
  t500 = ((uint64_t)(v7));
  t501 = ((int64_t)(v63));
  t502 = fg302ds_gg_function_6(t500, t501, base + 0);
  t503 = ((int64_t)(v65));
  t504 = ((int64_t)(v4));
  t505 = ((int64_t)(v2));
  t506 = ((uint64_t)(v7));
  t507 = ((int64_t)(v10));
  t508 = ((uint64_t)(v9));
  t509 = ((uint64_t)(v8));
  t510 = ((int64_t)(v6));
  t511 = ((int64_t)(v16));
  v67 = fg302ds_gg_function_11(t502, t503, t504, t505, t506, t507, t508, t509, t510, t511, base + 0);
  t512 = ((uint64_t)(v20));
  t513 = ((int64_t)(v27));
  t514 = ((int64_t)(v66));
  t515 = ((int64_t)(v5));
  t516 = ((int64_t)(((int64_t)((uint64_t)(t514) + (uint64_t)(t515)))));
  t517 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t518 = ((int64_t)(((int64_t)((uint64_t)(t516) - (uint64_t)(t517)))));
  fg302ds_gg_function_8(t512, t513, t518, base + 0);
  t519 = ((uint64_t)(v21));
  t520 = ((int64_t)(v27));
  t521 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  fg302ds_gg_function_8(t519, t520, t521, base + 0);
  t522 = ((int64_t)(v27));
  t523 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t522) + (uint64_t)(t523)))));
  v68 = ((int64_t)(((int64_t)UINT64_C(0))));
label37:;
  t524 = ((int64_t)(v68));
  t525 = ((int64_t)(v3));
  if (!(((t524) < (t525)))) goto label38;
  {
  t526 = ((uint64_t)(v22));
  t527 = ((int64_t)(v28));
  t528 = ((int64_t)(v67));
  fg302ds_gg_function_8(t526, t527, t528, base + 0);
  t529 = ((int64_t)(v28));
  t530 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t529) + (uint64_t)(t530)))));
  t531 = ((int64_t)(v68));
  t532 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v68 = ((int64_t)(((int64_t)((uint64_t)(t531) + (uint64_t)(t532)))));
  }
  goto label37;
label38:;
  }
  }
  }
  }
  }
  }
  t533 = ((int64_t)(v30));
  v30 = fg302ds_gg_function_13(t533, base + 0);
  t534 = ((int64_t)(v30));
  t535 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t536 = ((t534) < (t535));
  if (t536) {
  {
  v30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  }
  }
  v69 = ((int64_t)(((int64_t)UINT64_C(1))));
label40:;
  t537 = ((int64_t)(v69));
  t538 = ((int64_t)(v29));
  if (!(((t537) < (t538)))) goto label41;
  {
  t539 = ((uint64_t)(v13));
  t540 = ((int64_t)(v69));
  t541 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t542 = ((int64_t)(((int64_t)((uint64_t)(t540) * (uint64_t)(t541)))));
  v70 = fg302ds_gg_function_6(t539, t542, base + 0);
  t543 = ((uint64_t)(v13));
  t544 = ((int64_t)(v69));
  t545 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t546 = ((int64_t)(((int64_t)((uint64_t)(t544) * (uint64_t)(t545)))));
  t547 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t548 = ((int64_t)(((int64_t)((uint64_t)(t546) + (uint64_t)(t547)))));
  v71 = fg302ds_gg_function_6(t543, t548, base + 0);
  t549 = ((uint64_t)(v13));
  t550 = ((int64_t)(v69));
  t551 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t552 = ((int64_t)(((int64_t)((uint64_t)(t550) * (uint64_t)(t551)))));
  t553 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t554 = ((int64_t)(((int64_t)((uint64_t)(t552) + (uint64_t)(t553)))));
  v72 = fg302ds_gg_function_6(t549, t554, base + 0);
  t555 = ((int64_t)(v69));
  t556 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v73 = ((int64_t)(((int64_t)((uint64_t)(t555) - (uint64_t)(t556)))));
  v74 = ((uint8_t)UINT64_C(0));
label43:;
  t557 = ((int64_t)(v73));
  t558 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t559 = ((t557) >= (t558));
  t560 = t559;
  if (t560) {
  t561 = v74;
  t560 = (!(t561));
  }
  if (!(t560)) goto label44;
  {
  t562 = ((uint64_t)(v13));
  t563 = ((int64_t)(v73));
  t564 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t565 = ((int64_t)(((int64_t)((uint64_t)(t563) * (uint64_t)(t564)))));
  v75 = fg302ds_gg_function_6(t562, t565, base + 0);
  t566 = ((int64_t)(v75));
  t567 = ((int64_t)(v70));
  t568 = ((t566) <= (t567));
  if (t568) {
  {
  v74 = ((uint8_t)UINT64_C(1));
  }
  } else {
  {
  t569 = ((uint64_t)(v13));
  t570 = ((int64_t)(v73));
  t571 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t572 = ((int64_t)(((int64_t)((uint64_t)(t570) + (uint64_t)(t571)))));
  t573 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t574 = ((int64_t)(((int64_t)((uint64_t)(t572) * (uint64_t)(t573)))));
  t575 = ((int64_t)(v75));
  fg302ds_gg_function_8(t569, t574, t575, base + 0);
  t576 = ((uint64_t)(v13));
  t577 = ((int64_t)(v73));
  t578 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t579 = ((int64_t)(((int64_t)((uint64_t)(t577) + (uint64_t)(t578)))));
  t580 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t581 = ((int64_t)(((int64_t)((uint64_t)(t579) * (uint64_t)(t580)))));
  t582 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t583 = ((int64_t)(((int64_t)((uint64_t)(t581) + (uint64_t)(t582)))));
  t584 = ((uint64_t)(v13));
  t585 = ((int64_t)(v73));
  t586 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t587 = ((int64_t)(((int64_t)((uint64_t)(t585) * (uint64_t)(t586)))));
  t588 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t589 = ((int64_t)(((int64_t)((uint64_t)(t587) + (uint64_t)(t588)))));
  t590 = fg302ds_gg_function_6(t584, t589, base + 0);
  fg302ds_gg_function_8(t576, t583, t590, base + 0);
  t591 = ((uint64_t)(v13));
  t592 = ((int64_t)(v73));
  t593 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t594 = ((int64_t)(((int64_t)((uint64_t)(t592) + (uint64_t)(t593)))));
  t595 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t596 = ((int64_t)(((int64_t)((uint64_t)(t594) * (uint64_t)(t595)))));
  t597 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t598 = ((int64_t)(((int64_t)((uint64_t)(t596) + (uint64_t)(t597)))));
  t599 = ((uint64_t)(v13));
  t600 = ((int64_t)(v73));
  t601 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t602 = ((int64_t)(((int64_t)((uint64_t)(t600) * (uint64_t)(t601)))));
  t603 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t604 = ((int64_t)(((int64_t)((uint64_t)(t602) + (uint64_t)(t603)))));
  t605 = fg302ds_gg_function_6(t599, t604, base + 0);
  fg302ds_gg_function_8(t591, t598, t605, base + 0);
  t606 = ((int64_t)(v73));
  t607 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v73 = ((int64_t)(((int64_t)((uint64_t)(t606) - (uint64_t)(t607)))));
  }
  }
  }
  goto label43;
label44:;
  t608 = ((uint64_t)(v13));
  t609 = ((int64_t)(v73));
  t610 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t611 = ((int64_t)(((int64_t)((uint64_t)(t609) + (uint64_t)(t610)))));
  t612 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t613 = ((int64_t)(((int64_t)((uint64_t)(t611) * (uint64_t)(t612)))));
  t614 = ((int64_t)(v70));
  fg302ds_gg_function_8(t608, t613, t614, base + 0);
  t615 = ((uint64_t)(v13));
  t616 = ((int64_t)(v73));
  t617 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t618 = ((int64_t)(((int64_t)((uint64_t)(t616) + (uint64_t)(t617)))));
  t619 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t620 = ((int64_t)(((int64_t)((uint64_t)(t618) * (uint64_t)(t619)))));
  t621 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t622 = ((int64_t)(((int64_t)((uint64_t)(t620) + (uint64_t)(t621)))));
  t623 = ((int64_t)(v71));
  fg302ds_gg_function_8(t615, t622, t623, base + 0);
  t624 = ((uint64_t)(v13));
  t625 = ((int64_t)(v73));
  t626 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t627 = ((int64_t)(((int64_t)((uint64_t)(t625) + (uint64_t)(t626)))));
  t628 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t629 = ((int64_t)(((int64_t)((uint64_t)(t627) * (uint64_t)(t628)))));
  t630 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t631 = ((int64_t)(((int64_t)((uint64_t)(t629) + (uint64_t)(t630)))));
  t632 = ((int64_t)(v72));
  fg302ds_gg_function_8(t624, t631, t632, base + 0);
  t633 = ((int64_t)(v69));
  t634 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v69 = ((int64_t)(((int64_t)((uint64_t)(t633) + (uint64_t)(t634)))));
  }
  goto label40;
label41:;
  v76 = ((int64_t)(((int64_t)UINT64_C(0))));
label46:;
  t635 = ((int64_t)(v76));
  t636 = ((int64_t)(v29));
  if (!(((t635) < (t636)))) goto label47;
  {
  t637 = ((uint64_t)(v23));
  t638 = ((int64_t)(v76));
  t639 = ((uint64_t)(v13));
  t640 = ((int64_t)(v76));
  t641 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t642 = ((int64_t)(((int64_t)((uint64_t)(t640) * (uint64_t)(t641)))));
  t643 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t644 = ((int64_t)(((int64_t)((uint64_t)(t642) + (uint64_t)(t643)))));
  t645 = fg302ds_gg_function_6(t639, t644, base + 0);
  fg302ds_gg_function_8(t637, t638, t645, base + 0);
  t646 = ((uint64_t)(v24));
  t647 = ((int64_t)(v76));
  t648 = ((uint64_t)(v13));
  t649 = ((int64_t)(v76));
  t650 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t651 = ((int64_t)(((int64_t)((uint64_t)(t649) * (uint64_t)(t650)))));
  t652 = fg302ds_gg_function_6(t648, t651, base + 0);
  fg302ds_gg_function_8(t646, t647, t652, base + 0);
  t653 = ((int64_t)(v76));
  t654 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v76 = ((int64_t)(((int64_t)((uint64_t)(t653) + (uint64_t)(t654)))));
  }
  goto label46;
label47:;
  t655 = ((uint64_t)(v14));
  t656 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t657 = ((int64_t)(v26));
  fg302ds_gg_function_8(t655, t656, t657, base + 0);
  t658 = ((uint64_t)(v14));
  t659 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t660 = ((int64_t)(v27));
  fg302ds_gg_function_8(t658, t659, t660, base + 0);
  t661 = ((uint64_t)(v14));
  t662 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t663 = ((int64_t)(v28));
  fg302ds_gg_function_8(t661, t662, t663, base + 0);
  t664 = ((uint64_t)(v14));
  t665 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t666 = ((int64_t)(v29));
  fg302ds_gg_function_8(t664, t665, t666, base + 0);
  t667 = ((uint64_t)(v14));
  t668 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t669 = ((int64_t)(v30));
  fg302ds_gg_function_8(t667, t668, t669, base + 0);
  t670 = ((uint64_t)(v14));
  t671 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t672 = ((int64_t)(v6));
  t673 = ((int64_t)(v11));
  t674 = v17;
  t675 = fg302ds_gg_function_14(t672, t673, t674, base + 0);
  fg302ds_gg_function_8(t670, t671, t675, base + 0);
  t676 = ((int64_t)(v0));
  t677 = ((int64_t)(((int64_t)UINT64_C(3))));
  t678 = ((int64_t)(((int64_t)UINT64_C(0))));
  t679 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern void (fg302k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg302k_put(t676, t677, t678, t679);
  t680 = ((int64_t)(v0));
  t681 = ((int64_t)(((int64_t)UINT64_C(3))));
  t682 = ((int64_t)(((int64_t)UINT64_C(1))));
  t683 = ((uint64_t)(((uint64_t)(v30))));
#ifndef __cplusplus
  extern void (fg302k_put)(int64_t, int64_t, int64_t, uint64_t);
#endif
  fg302k_put(t680, t681, t682, t683);
  t0 = ((uint8_t)UINT64_C(1));
  goto fg302ds_gg_done;
  }
fg302ds_gg_done:;
  return t0;
}
static void fg302ds_gg_function_0(int64_t base) {
  int64_t t0;
  uint8_t t1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = fg302ds_gg_function_1(t0, base + 0);
  (void)(t1);
  }
fg302ds_gg_done:;
}
static int fg302ds_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg302ds_gg_argc = argc; fg302ds_gg_argv = argv;
  fg302ds_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg302ds_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg302ds_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg302ds_gg_program(argc, argv);
#endif
}
