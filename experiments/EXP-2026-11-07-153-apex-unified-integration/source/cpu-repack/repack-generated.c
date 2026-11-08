#ifndef FGOOSE_PACKED
#define FGOOSE_PACKED 1
#endif
#if defined(__AVX2__)
#include <immintrin.h>
#endif
/* Goose-in-Goose experimental C backend. Generated from test/goose_in_goose/. */
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
typedef struct { uint8_t data[1]; } gg_type_2;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[1]; } gg_type_4;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[8]; } gg_type_10;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[1024]; } gg_type_12;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_13;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_19;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[128]; } gg_type_20;
#pragma pack(pop)
typedef struct gg_type_21 gg_type_21;
#pragma pack(push, 1)
struct gg_type_21 {
  gg_type_10 f0;
  gg_type_10 f1;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_22;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_23;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[32]; } gg_type_26;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int8_t data[256]; } gg_type_27;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, uint8_t value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(float a, float b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, float value);
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_type_4 value);
static uint8_t gg_eq_5(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_slice value);
static uint8_t gg_eq_6(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, gg_slice value);
static uint8_t gg_eq_7(int64_t a, int64_t b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, int64_t value);
static uint8_t gg_eq_8(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, uint8_t value);
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, gg_type_10 value);
static uint8_t gg_eq_11(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, gg_slice value);
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_type_12 value);
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_type_13 value);
static uint8_t gg_eq_14(double a, double b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, double value);
static uint8_t gg_eq_15(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, uint32_t value);
static uint8_t gg_eq_16(int32_t a, int32_t b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, int32_t value);
static uint8_t gg_eq_17(int8_t a, int8_t b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, int8_t value);
static uint8_t gg_eq_18(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, gg_slice value);
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, gg_type_19 value);
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b);
static uint8_t gg_equal_elements_20(gg_slice a, gg_slice b);
static void gg_render_20(gg_reference out, gg_type_20 value);
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b);
static uint8_t gg_equal_elements_21(gg_slice a, gg_slice b);
static void gg_render_21(gg_reference out, gg_type_21 value);
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b);
static uint8_t gg_equal_elements_22(gg_slice a, gg_slice b);
static void gg_render_22(gg_reference out, gg_type_22 value);
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b);
static uint8_t gg_equal_elements_23(gg_slice a, gg_slice b);
static void gg_render_23(gg_reference out, gg_type_23 value);
static uint8_t gg_eq_24(int16_t a, int16_t b);
static uint8_t gg_equal_elements_24(gg_slice a, gg_slice b);
static void gg_render_24(gg_reference out, int16_t value);
static uint8_t gg_eq_25(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_25(gg_slice a, gg_slice b);
static void gg_render_25(gg_reference out, uint16_t value);
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b);
static uint8_t gg_equal_elements_26(gg_slice a, gg_slice b);
static void gg_render_26(gg_reference out, gg_type_26 value);
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b);
static uint8_t gg_equal_elements_27(gg_slice a, gg_slice b);
static void gg_render_27(gg_reference out, gg_type_27 value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
return gg_equal_elements_1(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
gg_text(out,gg_view((value).data, 1));
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(float a, float b) {
return a == b;
}
static void gg_render_3(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_type_4*)a.data)[i],((gg_type_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b) {
return gg_equal_elements_3(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_5(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_6(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_7(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_8(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
return gg_equal_elements_1(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
gg_text(out,gg_view((value).data, 8));
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_11(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return gg_equal_elements_3(gg_view((a).data, 1024),gg_view((b).data, 1024));
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
int64_t i; gg_slice view=gg_view((value).data, 1024);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
return gg_equal_elements_3(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(double a, double b) {
return a == b;
}
static void gg_render_14(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_15(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_16(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_17(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_18(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((gg_type_19*)a.data)[i],((gg_type_19*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b) {
return gg_equal_elements_3(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_19(gg_reference out, gg_type_19 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((gg_type_20*)a.data)[i],((gg_type_20*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b) {
return gg_equal_elements_16(gg_view((a).data, 128),gg_view((b).data, 128));
}
static void gg_render_20(gg_reference out, gg_type_20 value) {
int64_t i; gg_slice view=gg_view((value).data, 128);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((gg_type_21*)a.data)[i],((gg_type_21*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b) {
if(!gg_eq_10(a.f0,b.f0))return 0;
if(!gg_eq_10(a.f1,b.f1))return 0;
return 1;
}
static void gg_render_21(gg_reference out, gg_type_21 value) {
gg_append(out,"\121\064\123\143\141\154\145\163\040\173\040",11,1);
gg_quote(out, gg_view((value.f0).data, 8));
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f1).data, 8));
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((gg_type_22*)a.data)[i],((gg_type_22*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b) {
return gg_equal_elements_16(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_22(gg_reference out, gg_type_22 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((gg_type_23*)a.data)[i],((gg_type_23*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b) {
return gg_equal_elements_16(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_23(gg_reference out, gg_type_23 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_24(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_25(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((gg_type_26*)a.data)[i],((gg_type_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b) {
return gg_equal_elements_3(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_26(gg_reference out, gg_type_26 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((gg_type_27*)a.data)[i],((gg_type_27*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b) {
return gg_equal_elements_17(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_27(gg_reference out, gg_type_27 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_17(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_2(gg_slice p0, int64_t p1, int64_t base);
static void gg_function_3(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_5(gg_slice p0, gg_slice p1, int64_t p2, int64_t base);
static void gg_function_7(gg_slice p0, int64_t p1, uint32_t p2, int64_t base);
static int32_t gg_function_8(float p0, int64_t base);
static void gg_function_9(int64_t p0, gg_slice p1, int64_t p2, gg_slice p3, gg_slice p4, int64_t p5, int64_t p6, int64_t base);
static int64_t gg_function_10(gg_slice p0, int64_t p1, int64_t base);
static gg_type_20 gg_function_12(gg_slice p0, int64_t p1, int64_t base);
static gg_type_21 gg_function_13(gg_slice p0, int64_t p1, int64_t base);
static void gg_function_16(int64_t p0, gg_slice p1, int64_t p2, gg_slice p3, gg_slice p4, int64_t p5, int64_t p6, int64_t base);
static int64_t gg_function_17(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_17(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_2(gg_slice p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t * t10;
  int64_t t11;
  gg_slice t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t * t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)((v0).len));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  t4 = ((t5) <= (t8));
  }
  if (!(t4)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t9 = v1;
  t10 = ((uint8_t *)(v0).data) + gg_index(t9, (v0).len);
  t11 = ((int64_t)(((int64_t)((*t10)))));
  t12 = v0;
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  t16 = ((uint8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int64_t)(((int64_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) << ((uint64_t)(t18) & 63)))));
  t0 = ((int64_t)(((t11) | (t19))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(gg_slice p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t * t10;
  int64_t t11;
  gg_slice t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t * t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  gg_slice t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t * t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  gg_slice t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t * t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)((v0).len));
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  t4 = ((t5) <= (t8));
  }
  if (!(t4)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t9 = v1;
  t10 = ((uint8_t *)(v0).data) + gg_index(t9, (v0).len);
  t11 = ((int64_t)(((int64_t)((*t10)))));
  t12 = v0;
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  t16 = ((uint8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int64_t)(((int64_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) << ((uint64_t)(t18) & 63)))));
  t20 = ((int64_t)(((t11) | (t19))));
  t21 = v0;
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((uint8_t *)t21.data) + gg_index(t24, t21.len);
  t26 = ((int64_t)(((int64_t)((*t25)))));
  t27 = ((int64_t)(((int64_t)UINT64_C(16))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) << ((uint64_t)(t27) & 63)))));
  t29 = ((int64_t)(((t20) | (t28))));
  t30 = v0;
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  t33 = ((int64_t)((uint64_t)(t31) + (uint64_t)(t32)));
  t34 = ((uint8_t *)t30.data) + gg_index(t33, t30.len);
  t35 = ((int64_t)(((int64_t)((*t34)))));
  t36 = ((int64_t)(((int64_t)UINT64_C(24))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) << ((uint64_t)(t36) & 63)))));
  t0 = ((int64_t)(((t29) | (t37))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_21 gg_function_13(gg_slice p0, int64_t p1, int64_t base) {
  gg_type_21 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int32_t * t7;
  gg_slice t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int32_t * t16;
  int64_t t17;
  int32_t * t18;
  int32_t t19;
  int64_t t20;
  int32_t t21;
  int32_t t22;
  int32_t t23;
  int64_t t24;
  int32_t * t25;
  int32_t t26;
  int64_t t27;
  int32_t t28;
  int32_t t29;
  int32_t t30;
  int64_t t31;
  int32_t t32;
  int64_t t33;
  int32_t * t34;
  int32_t t35;
  int32_t t36;
  int64_t t37;
  int32_t * t38;
  int64_t t39;
  int32_t * t40;
  int32_t t41;
  int32_t t42;
  int32_t t43;
  int64_t t44;
  int32_t * t45;
  int32_t t46;
  int64_t t47;
  int32_t t48;
  int32_t t49;
  int32_t t50;
  int64_t t51;
  int32_t t52;
  int64_t t53;
  int32_t * t54;
  int64_t t55;
  int32_t * t56;
  int64_t t57;
  int32_t * t58;
  int32_t t59;
  int32_t t60;
  uint8_t t61;
  uint8_t t62;
  int64_t t63;
  uint8_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t * t70;
  int64_t t71;
  int32_t * t72;
  int32_t t73;
  int64_t t74;
  int64_t t75;
  uint32_t t76;
  gg_slice t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t * t81;
  int64_t t82;
  int32_t * t83;
  int32_t t84;
  int64_t t85;
  int64_t t86;
  uint32_t t87;
  int64_t t88;
  uint8_t * t89;
  int64_t t90;
  int32_t * t91;
  int32_t t92;
  int64_t t93;
  int64_t t94;
  uint32_t t95;
  gg_slice t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t * t100;
  int64_t t101;
  int32_t * t102;
  int32_t t103;
  int64_t t104;
  int64_t t105;
  uint32_t t106;
  gg_slice v0;
  int64_t v1;
  gg_type_22 v2;
  int64_t v3;
  int32_t v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  gg_type_21 v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<4; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(4));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((int32_t *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(4)));
  t8 = v0;
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t12)))));
  t14 = gg_function_10(t8, t13, base + 0);
  (*t7) = ((int32_t)(((int32_t)(t14))));
  }
label1:;
  t4++; goto label0;
label2:;
  v4 = ((int32_t)(((int64_t)UINT64_C(1061109567))));
  v5 = ((int32_t)(((int64_t)UINT64_C(252645135))));
  v6 = ((int32_t)(((int64_t)UINT64_C(50529027))));
  /* FG_DIRECT_SLICE_INDEX */
  t15 = ((int64_t)UINT64_C(3));
  t16 = ((int32_t *)(v2).data) + gg_index(t15, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t17 = ((int64_t)UINT64_C(2));
  t18 = ((int32_t *)(v2).data) + gg_index(t17, ((int64_t)UINT64_C(4)));
  t19 = ((int32_t)((*t18)));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = ((int32_t)(((t19) >> ((uint64_t)(t20) & 31))));
  t22 = ((int32_t)(v5));
  t23 = ((int32_t)(((t21) & (t22))));
  /* FG_DIRECT_SLICE_INDEX */
  t24 = ((int64_t)UINT64_C(1));
  t25 = ((int32_t *)(v2).data) + gg_index(t24, ((int64_t)UINT64_C(4)));
  t26 = ((int32_t)((*t25)));
  t27 = ((int64_t)(((int64_t)UINT64_C(6))));
  t28 = ((int32_t)(((t26) >> ((uint64_t)(t27) & 31))));
  t29 = ((int32_t)(v6));
  t30 = ((int32_t)(((t28) & (t29))));
  t31 = ((int64_t)(((int64_t)UINT64_C(4))));
  t32 = ((int32_t)(((int32_t)((uint64_t)(t30) << ((uint64_t)(t31) & 31)))));
  (*t16) = ((int32_t)(((t23) | (t32))));
  /* FG_DIRECT_SLICE_INDEX */
  t33 = ((int64_t)UINT64_C(1));
  t34 = ((int32_t *)(v2).data) + gg_index(t33, ((int64_t)UINT64_C(4)));
  t35 = ((int32_t)((*t34)));
  t36 = ((int32_t)(v4));
  v7 = ((int32_t)(((t35) & (t36))));
  /* FG_DIRECT_SLICE_INDEX */
  t37 = ((int64_t)UINT64_C(1));
  t38 = ((int32_t *)(v2).data) + gg_index(t37, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t39 = ((int64_t)UINT64_C(2));
  t40 = ((int32_t *)(v2).data) + gg_index(t39, ((int64_t)UINT64_C(4)));
  t41 = ((int32_t)((*t40)));
  t42 = ((int32_t)(v5));
  t43 = ((int32_t)(((t41) & (t42))));
  /* FG_DIRECT_SLICE_INDEX */
  t44 = ((int64_t)UINT64_C(0));
  t45 = ((int32_t *)(v2).data) + gg_index(t44, ((int64_t)UINT64_C(4)));
  t46 = ((int32_t)((*t45)));
  t47 = ((int64_t)(((int64_t)UINT64_C(6))));
  t48 = ((int32_t)(((t46) >> ((uint64_t)(t47) & 31))));
  t49 = ((int32_t)(v6));
  t50 = ((int32_t)(((t48) & (t49))));
  t51 = ((int64_t)(((int64_t)UINT64_C(4))));
  t52 = ((int32_t)(((int32_t)((uint64_t)(t50) << ((uint64_t)(t51) & 31)))));
  (*t38) = ((int32_t)(((t43) | (t52))));
  /* FG_DIRECT_SLICE_INDEX */
  t53 = ((int64_t)UINT64_C(2));
  t54 = ((int32_t *)(v2).data) + gg_index(t53, ((int64_t)UINT64_C(4)));
  (*t54) = ((int32_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t55 = ((int64_t)UINT64_C(0));
  t56 = ((int32_t *)(v2).data) + gg_index(t55, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t57 = ((int64_t)UINT64_C(0));
  t58 = ((int32_t *)(v2).data) + gg_index(t57, ((int64_t)UINT64_C(4)));
  t59 = ((int32_t)((*t58)));
  t60 = ((int32_t)(v4));
  (*t56) = ((int32_t)(((t59) & (t60))));
  memset(&(v8), 0, sizeof(v8));
  memset(&t61, 0, sizeof(t61));
  t62 = ((uint8_t)(t61));
  for (t63=0; t63<8; t63++) ((v8).f0).data[t63] = t62;
  memset(&t64, 0, sizeof(t64));
  t65 = ((uint8_t)(t64));
  for (t66=0; t66<8; t66++) ((v8).f1).data[t66] = t65;
  t68 = ((int64_t)UINT64_C(4));
  t67 = 0;
label3:;
  if (t67 >= t68) goto label5;
  v9 = t67;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t69 = v9;
  t70 = ((uint8_t *)((v8).f0).data) + gg_index(t69, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t71 = ((int64_t)UINT64_C(0));
  t72 = ((int32_t *)(v2).data) + gg_index(t71, ((int64_t)UINT64_C(4)));
  t73 = ((int32_t)((*t72)));
  t74 = ((int64_t)(((int64_t)UINT64_C(8))));
  t75 = ((int64_t)(v9));
  t76 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t74) * (uint64_t)(t75)))))));
  (*t70) = ((uint8_t)(((uint8_t)(((t73) >> ((uint64_t)(t76) & 31))))));
  t77 = gg_view(((v8).f0).data, 8);
  t78 = ((int64_t)(((int64_t)UINT64_C(4))));
  t79 = ((int64_t)(v9));
  t80 = ((int64_t)((uint64_t)(t78) + (uint64_t)(t79)));
  t81 = ((uint8_t *)t77.data) + gg_index(t80, t77.len);
  /* FG_DIRECT_SLICE_INDEX */
  t82 = ((int64_t)UINT64_C(1));
  t83 = ((int32_t *)(v2).data) + gg_index(t82, ((int64_t)UINT64_C(4)));
  t84 = ((int32_t)((*t83)));
  t85 = ((int64_t)(((int64_t)UINT64_C(8))));
  t86 = ((int64_t)(v9));
  t87 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t86)))))));
  (*t81) = ((uint8_t)(((uint8_t)(((t84) >> ((uint64_t)(t87) & 31))))));
  /* FG_DIRECT_SLICE_INDEX */
  t88 = v9;
  t89 = ((uint8_t *)((v8).f1).data) + gg_index(t88, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t90 = ((int64_t)UINT64_C(2));
  t91 = ((int32_t *)(v2).data) + gg_index(t90, ((int64_t)UINT64_C(4)));
  t92 = ((int32_t)((*t91)));
  t93 = ((int64_t)(((int64_t)UINT64_C(8))));
  t94 = ((int64_t)(v9));
  t95 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t93) * (uint64_t)(t94)))))));
  (*t89) = ((uint8_t)(((uint8_t)(((t92) >> ((uint64_t)(t95) & 31))))));
  t96 = gg_view(((v8).f1).data, 8);
  t97 = ((int64_t)(((int64_t)UINT64_C(4))));
  t98 = ((int64_t)(v9));
  t99 = ((int64_t)((uint64_t)(t97) + (uint64_t)(t98)));
  t100 = ((uint8_t *)t96.data) + gg_index(t99, t96.len);
  /* FG_DIRECT_SLICE_INDEX */
  t101 = ((int64_t)UINT64_C(3));
  t102 = ((int32_t *)(v2).data) + gg_index(t101, ((int64_t)UINT64_C(4)));
  t103 = ((int32_t)((*t102)));
  t104 = ((int64_t)(((int64_t)UINT64_C(8))));
  t105 = ((int64_t)(v9));
  t106 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t104) * (uint64_t)(t105)))))));
  (*t100) = ((uint8_t)(((uint8_t)(((t103) >> ((uint64_t)(t106) & 31))))));
  }
label4:;
  t67++; goto label3;
label5:;
  t0 = v8;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_20 gg_function_12(gg_slice p0, int64_t p1, int64_t base) {
  gg_type_20 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  gg_slice t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int32_t * t22;
  int64_t t23;
  uint8_t * t24;
  gg_slice t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int32_t * t33;
  int64_t t34;
  uint8_t * t35;
  gg_slice v0;
  int64_t v1;
  gg_type_20 v2;
  int64_t v3;
  gg_type_21 v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<128; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  t6 = v0;
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)UINT64_C(32))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)UINT64_C(12))));
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t12)))));
  v4 = gg_function_13(t6, t13, base + 0);
  t15 = ((int64_t)UINT64_C(8));
  t14 = 0;
label3:;
  if (t14 >= t15) goto label5;
  v5 = t14;
  {
  t16 = gg_view((v2).data, 128);
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(v5));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((int32_t *)t16.data) + gg_index(t21, t16.len);
  /* FG_DIRECT_SLICE_INDEX */
  t23 = v5;
  t24 = ((uint8_t *)((v4).f0).data) + gg_index(t23, ((int64_t)UINT64_C(8)));
  (*t22) = ((int32_t)(((int32_t)((*t24)))));
  t25 = gg_view((v2).data, 128);
  t26 = ((int64_t)(((int64_t)UINT64_C(64))));
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(8))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t29)))));
  t31 = ((int64_t)(v5));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((int32_t *)t25.data) + gg_index(t32, t25.len);
  /* FG_DIRECT_SLICE_INDEX */
  t34 = v5;
  t35 = ((uint8_t *)((v4).f1).data) + gg_index(t34, ((int64_t)UINT64_C(8)));
  (*t33) = ((int32_t)(((int32_t)((*t35)))));
  }
label4:;
  t14++; goto label3;
label5:;
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v2;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_16(int64_t p0, gg_slice p1, int64_t p2, gg_slice p3, gg_slice p4, int64_t p5, int64_t p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  uint8_t t23;
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
  int64_t t52;
  int64_t t53;
  float t54;
  float t55;
  int64_t t56;
  float t57;
  float t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
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
  int64_t t73;
  gg_slice t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  gg_slice t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint32_t t85;
  float t86;
  int8_t t87;
  int8_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int8_t * t97;
  gg_slice t98;
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
  uint8_t * t112;
  int32_t t113;
  int32_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int32_t t120;
  gg_slice t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int8_t * t127;
  int32_t t128;
  int64_t t129;
  int32_t * t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  gg_slice t139;
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
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  uint8_t * t157;
  uint8_t t158;
  uint8_t t159;
  uint8_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int32_t t170;
  int32_t t171;
  int64_t t172;
  int8_t * t173;
  int32_t t174;
  int32_t t175;
  gg_slice t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int32_t * t184;
  int32_t t185;
  int32_t t186;
  int32_t t187;
  int32_t t188;
  gg_slice t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int8_t * t193;
  int32_t t194;
  int32_t t195;
  gg_slice t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int32_t * t206;
  int32_t t207;
  int32_t t208;
  gg_slice t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int32_t * t219;
  int32_t t220;
  gg_slice t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int32_t * t225;
  int32_t t226;
  int32_t t227;
  gg_slice t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int32_t * t240;
  int32_t t241;
  gg_slice t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int32_t * t248;
  int32_t t249;
  int32_t t250;
  gg_slice t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  float * t257;
  float t258;
  gg_slice t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  uint16_t t266;
  float t267;
  float t268;
  float t269;
  float t270;
  gg_slice t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  float * t277;
  float t278;
  float t279;
  gg_slice t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  float * t286;
  float t287;
  gg_slice t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  uint16_t t297;
  float t298;
  float t299;
  float t300;
  float t301;
  gg_slice t302;
  int64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  float * t308;
  float t309;
  float t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  gg_slice t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  float * t329;
  gg_slice t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  float * t336;
  float t337;
  gg_slice t338;
  int64_t t339;
  int64_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  float * t344;
  float t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  int64_t t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  gg_slice v3;
  gg_slice v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  gg_type_26 v10;
  gg_type_26 v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  gg_type_20 v15;
  int64_t v16;
  float v17;
  gg_type_27 v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  gg_type_23 v22;
  int64_t v23;
  int32_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int32_t v28;
  int64_t v29;
  int64_t v30;
  uint8_t v31;
  int32_t v32;
  int32_t v33;
  int64_t v34;
  int32_t v35;
  int64_t v36;
  int64_t v37;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(256))));
  t6 = ((int64_t)(((int64_t)gg_mod(t4, t5))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t6) == (t7));
  }
  t8 = t3;
  t9 = t8;
  if (t9) {
  t10 = ((int64_t)(v5));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t10) > (t11));
  }
  t12 = t9;
  t13 = t12;
  if (t13) {
  t14 = ((int64_t)(v5));
  t15 = ((int64_t)(((int64_t)UINT64_C(4))));
  t16 = ((int64_t)(((int64_t)gg_mod(t14, t15))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t16) == (t17));
  }
  t18 = t13;
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t20) > (t21));
  }
  t22 = t19;
  t23 = t22;
  if (t23) {
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)UINT64_C(8))));
  t26 = ((int64_t)(((int64_t)gg_mod(t24, t25))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t26) == (t27));
  }
  if (!(t23)) gg_fail("assertion failed");
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(256))));
  v7 = ((int64_t)(((int64_t)gg_div(t28, t29))));
  t30 = ((int64_t)((v3).len));
  t31 = ((int64_t)(v6));
  t32 = ((int64_t)(((int64_t)UINT64_C(8))));
  t33 = ((int64_t)(((int64_t)gg_div(t31, t32))));
  t34 = ((int64_t)(v7));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((int64_t)(((int64_t)UINT64_C(1152))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  if (!(((t30) == (t37)))) gg_fail("assertion failed");
  t38 = ((int64_t)((v4).len));
  t39 = ((int64_t)(v5));
  t40 = ((int64_t)(((int64_t)UINT64_C(4))));
  t41 = ((int64_t)(((int64_t)gg_div(t39, t40))));
  t42 = ((int64_t)(v7));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t44 = ((int64_t)(((int64_t)UINT64_C(1168))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  if (!(((t38) == (t45)))) gg_fail("assertion failed");
  t47 = ((int64_t)(v5));
  t48 = ((int64_t)(((int64_t)UINT64_C(4))));
  t49 = ((int64_t)gg_div(t47, t48));
  t46 = 0;
label0:;
  if (t46 >= t49) goto label2;
  v8 = t46;
  {
  t51 = ((int64_t)(v6));
  t52 = ((int64_t)(((int64_t)UINT64_C(8))));
  t53 = ((int64_t)gg_div(t51, t52));
  t50 = 0;
label3:;
  if (t50 >= t53) goto label5;
  v9 = t50;
  {
  memset(&t54, 0, sizeof(t54));
  t55 = ((float)(t54));
  for (t56=0; t56<32; t56++) (v10).data[t56] = t55;
  memset(&t57, 0, sizeof(t57));
  t58 = ((float)(t57));
  for (t59=0; t59<32; t59++) (v11).data[t59] = t58;
  t61 = v7;
  t60 = 0;
label6:;
  if (t60 >= t61) goto label8;
  v12 = t60;
  {
  t62 = ((int64_t)(v8));
  t63 = ((int64_t)(v7));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t65 = ((int64_t)(v12));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
  t67 = ((int64_t)(((int64_t)UINT64_C(1168))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  t68 = ((int64_t)(v9));
  t69 = ((int64_t)(v7));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t69)))));
  t71 = ((int64_t)(v12));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)UINT64_C(1152))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  t74 = v3;
  t75 = ((int64_t)(v14));
  v15 = gg_function_12(t74, t75, base + 0);
  t77 = ((int64_t)UINT64_C(4));
  t76 = 0;
label9:;
  if (t76 >= t77) goto label11;
  v16 = t76;
  {
  t78 = v4;
  t79 = ((int64_t)(v13));
  t80 = ((int64_t)(((int64_t)UINT64_C(4))));
  t81 = ((int64_t)(v16));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t82)))));
  t84 = gg_function_10(t78, t83, base + 0);
  t85 = ((uint32_t)(((uint32_t)(t84))));
  t86 = fg143_from_bits(t85);
  v17 = ((float)(t86));
  memset(&t87, 0, sizeof(t87));
  t88 = ((int8_t)(t87));
  for (t89=0; t89<256; t89++) (v18).data[t89] = t88;
  t91 = ((int64_t)UINT64_C(256));
  t90 = 0;
label12:;
  if (t90 >= t91) goto label14;
  v19 = t90;
  {
  t92 = ((int64_t)(v19));
  t93 = ((int64_t)(((int64_t)UINT64_C(8))));
  v20 = ((int64_t)(((int64_t)gg_div(t92, t93))));
  t94 = ((int64_t)(v19));
  t95 = ((int64_t)(((int64_t)UINT64_C(8))));
  v21 = ((int64_t)(((int64_t)gg_mod(t94, t95))));
  /* FG_DIRECT_SLICE_INDEX */
  t96 = v19;
  t97 = ((int8_t *)(v18).data) + gg_index(t96, ((int64_t)UINT64_C(256)));
  t98 = v4;
  t99 = ((int64_t)(v13));
  t100 = ((int64_t)(((int64_t)UINT64_C(16))));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) + (uint64_t)(t100)))));
  t102 = ((int64_t)(v20));
  t103 = ((int64_t)(((int64_t)UINT64_C(32))));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t102) * (uint64_t)(t103)))));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t101) + (uint64_t)(t104)))));
  t106 = ((int64_t)(v16));
  t107 = ((int64_t)(((int64_t)UINT64_C(8))));
  t108 = ((int64_t)(((int64_t)((uint64_t)(t106) * (uint64_t)(t107)))));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t105) + (uint64_t)(t108)))));
  t110 = ((int64_t)(v21));
  t111 = ((int64_t)((uint64_t)(t109) + (uint64_t)(t110)));
  t112 = ((uint8_t *)t98.data) + gg_index(t111, t98.len);
  (*t97) = ((int8_t)(((int8_t)((*t112)))));
  }
label13:;
  t90++; goto label12;
label14:;
  memset(&t113, 0, sizeof(t113));
  t114 = ((int32_t)(t113));
  for (t115=0; t115<8; t115++) (v22).data[t115] = t114;
  t117 = ((int64_t)UINT64_C(8));
  t116 = 0;
label15:;
  if (t116 >= t117) goto label17;
  v23 = t116;
  {
  v24 = ((int32_t)(((int64_t)UINT64_C(0))));
  t119 = ((int64_t)UINT64_C(32));
  t118 = 0;
label18:;
  if (t118 >= t119) goto label20;
  v25 = t118;
  {
  t120 = v24;
  t121 = gg_view((v18).data, 256);
  t122 = ((int64_t)(v23));
  t123 = ((int64_t)(((int64_t)UINT64_C(32))));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t122) * (uint64_t)(t123)))));
  t125 = ((int64_t)(v25));
  t126 = ((int64_t)((uint64_t)(t124) + (uint64_t)(t125)));
  t127 = ((int8_t *)t121.data) + gg_index(t126, t121.len);
  t128 = ((int32_t)(((int32_t)((*t127)))));
  v24 = ((int32_t)((uint64_t)(t120) + (uint64_t)(t128)));
  }
label19:;
  t118++; goto label18;
label20:;
  /* FG_DIRECT_SLICE_INDEX */
  t129 = v23;
  t130 = ((int32_t *)(v22).data) + gg_index(t129, ((int64_t)UINT64_C(8)));
  (*t130) = ((int32_t)(v24));
  }
label16:;
  t116++; goto label15;
label17:;
  t132 = ((int64_t)UINT64_C(4));
  t131 = 0;
label21:;
  if (t131 >= t132) goto label23;
  v26 = t131;
  {
  t134 = ((int64_t)UINT64_C(8));
  t133 = 0;
label24:;
  if (t133 >= t134) goto label26;
  v27 = t133;
  {
  v28 = ((int32_t)(((int64_t)UINT64_C(0))));
  t136 = ((int64_t)UINT64_C(4));
  t135 = 0;
label27:;
  if (t135 >= t136) goto label29;
  v29 = t135;
  {
  t138 = ((int64_t)UINT64_C(8));
  t137 = 0;
label30:;
  if (t137 >= t138) goto label32;
  v30 = t137;
  {
  t139 = v3;
  t140 = ((int64_t)(v14));
  t141 = ((int64_t)(((int64_t)UINT64_C(128))));
  t142 = ((int64_t)(((int64_t)((uint64_t)(t140) + (uint64_t)(t141)))));
  t143 = ((int64_t)(((int64_t)UINT64_C(4))));
  t144 = ((int64_t)(v26));
  t145 = ((int64_t)(((int64_t)((uint64_t)(t143) * (uint64_t)(t144)))));
  t146 = ((int64_t)(v29));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t145) + (uint64_t)(t146)))));
  t148 = ((int64_t)(((int64_t)UINT64_C(64))));
  t149 = ((int64_t)(((int64_t)((uint64_t)(t147) * (uint64_t)(t148)))));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t142) + (uint64_t)(t149)))));
  t151 = ((int64_t)(v27));
  t152 = ((int64_t)(((int64_t)UINT64_C(8))));
  t153 = ((int64_t)(((int64_t)((uint64_t)(t151) * (uint64_t)(t152)))));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t150) + (uint64_t)(t153)))));
  t155 = ((int64_t)(v30));
  t156 = ((int64_t)((uint64_t)(t154) + (uint64_t)(t155)));
  t157 = ((uint8_t *)t139.data) + gg_index(t156, t139.len);
  v31 = ((uint8_t)((*t157)));
  t158 = ((uint8_t)(v31));
  t159 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v32 = ((int32_t)(((int32_t)(((t158) & (t159))))));
  t160 = ((uint8_t)(v31));
  t161 = ((int64_t)(((int64_t)UINT64_C(4))));
  v33 = ((int32_t)(((int32_t)(((t160) >> ((uint64_t)(t161) & 7))))));
  t162 = ((int64_t)(v26));
  t163 = ((int64_t)(((int64_t)UINT64_C(64))));
  t164 = ((int64_t)(((int64_t)((uint64_t)(t162) * (uint64_t)(t163)))));
  t165 = ((int64_t)(v29));
  t166 = ((int64_t)(((int64_t)UINT64_C(8))));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t165) * (uint64_t)(t166)))));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t167)))));
  t169 = ((int64_t)(v30));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t168) + (uint64_t)(t169)))));
  t170 = v28;
  t171 = ((int32_t)(v32));
  /* FG_DIRECT_SLICE_INDEX */
  t172 = v34;
  t173 = ((int8_t *)(v18).data) + gg_index(t172, ((int64_t)UINT64_C(256)));
  t174 = ((int32_t)(((int32_t)((*t173)))));
  t175 = ((int32_t)(((int32_t)((uint64_t)(t171) * (uint64_t)(t174)))));
  t176 = gg_view((v15).data, 128);
  t177 = ((int64_t)(((int64_t)UINT64_C(2))));
  t178 = ((int64_t)(v26));
  t179 = ((int64_t)(((int64_t)((uint64_t)(t177) * (uint64_t)(t178)))));
  t180 = ((int64_t)(((int64_t)UINT64_C(8))));
  t181 = ((int64_t)(((int64_t)((uint64_t)(t179) * (uint64_t)(t180)))));
  t182 = ((int64_t)(v27));
  t183 = ((int64_t)((uint64_t)(t181) + (uint64_t)(t182)));
  t184 = ((int32_t *)t176.data) + gg_index(t183, t176.len);
  t185 = ((int32_t)((*t184)));
  t186 = ((int32_t)(((int32_t)((uint64_t)(t175) * (uint64_t)(t185)))));
  v28 = ((int32_t)((uint64_t)(t170) + (uint64_t)(t186)));
  t187 = v28;
  t188 = ((int32_t)(v33));
  t189 = gg_view((v18).data, 256);
  t190 = ((int64_t)(v34));
  t191 = ((int64_t)(((int64_t)UINT64_C(32))));
  t192 = ((int64_t)((uint64_t)(t190) + (uint64_t)(t191)));
  t193 = ((int8_t *)t189.data) + gg_index(t192, t189.len);
  t194 = ((int32_t)(((int32_t)((*t193)))));
  t195 = ((int32_t)(((int32_t)((uint64_t)(t188) * (uint64_t)(t194)))));
  t196 = gg_view((v15).data, 128);
  t197 = ((int64_t)(((int64_t)UINT64_C(2))));
  t198 = ((int64_t)(v26));
  t199 = ((int64_t)(((int64_t)((uint64_t)(t197) * (uint64_t)(t198)))));
  t200 = ((int64_t)(((int64_t)UINT64_C(1))));
  t201 = ((int64_t)(((int64_t)((uint64_t)(t199) + (uint64_t)(t200)))));
  t202 = ((int64_t)(((int64_t)UINT64_C(8))));
  t203 = ((int64_t)(((int64_t)((uint64_t)(t201) * (uint64_t)(t202)))));
  t204 = ((int64_t)(v27));
  t205 = ((int64_t)((uint64_t)(t203) + (uint64_t)(t204)));
  t206 = ((int32_t *)t196.data) + gg_index(t205, t196.len);
  t207 = ((int32_t)((*t206)));
  t208 = ((int32_t)(((int32_t)((uint64_t)(t195) * (uint64_t)(t207)))));
  v28 = ((int32_t)((uint64_t)(t187) + (uint64_t)(t208)));
  }
label31:;
  t137++; goto label30;
label32:;
  }
label28:;
  t135++; goto label27;
label29:;
  t209 = gg_view((v15).data, 128);
  t210 = ((int64_t)(((int64_t)UINT64_C(64))));
  t211 = ((int64_t)(((int64_t)UINT64_C(2))));
  t212 = ((int64_t)(v26));
  t213 = ((int64_t)(((int64_t)((uint64_t)(t211) * (uint64_t)(t212)))));
  t214 = ((int64_t)(((int64_t)UINT64_C(8))));
  t215 = ((int64_t)(((int64_t)((uint64_t)(t213) * (uint64_t)(t214)))));
  t216 = ((int64_t)(((int64_t)((uint64_t)(t210) + (uint64_t)(t215)))));
  t217 = ((int64_t)(v27));
  t218 = ((int64_t)((uint64_t)(t216) + (uint64_t)(t217)));
  t219 = ((int32_t *)t209.data) + gg_index(t218, t209.len);
  t220 = ((int32_t)((*t219)));
  t221 = gg_view((v22).data, 8);
  t222 = ((int64_t)(((int64_t)UINT64_C(2))));
  t223 = ((int64_t)(v26));
  t224 = ((int64_t)((uint64_t)(t222) * (uint64_t)(t223)));
  t225 = ((int32_t *)t221.data) + gg_index(t224, t221.len);
  t226 = ((int32_t)((*t225)));
  t227 = ((int32_t)(((int32_t)((uint64_t)(t220) * (uint64_t)(t226)))));
  t228 = gg_view((v15).data, 128);
  t229 = ((int64_t)(((int64_t)UINT64_C(64))));
  t230 = ((int64_t)(((int64_t)UINT64_C(2))));
  t231 = ((int64_t)(v26));
  t232 = ((int64_t)(((int64_t)((uint64_t)(t230) * (uint64_t)(t231)))));
  t233 = ((int64_t)(((int64_t)UINT64_C(1))));
  t234 = ((int64_t)(((int64_t)((uint64_t)(t232) + (uint64_t)(t233)))));
  t235 = ((int64_t)(((int64_t)UINT64_C(8))));
  t236 = ((int64_t)(((int64_t)((uint64_t)(t234) * (uint64_t)(t235)))));
  t237 = ((int64_t)(((int64_t)((uint64_t)(t229) + (uint64_t)(t236)))));
  t238 = ((int64_t)(v27));
  t239 = ((int64_t)((uint64_t)(t237) + (uint64_t)(t238)));
  t240 = ((int32_t *)t228.data) + gg_index(t239, t228.len);
  t241 = ((int32_t)((*t240)));
  t242 = gg_view((v22).data, 8);
  t243 = ((int64_t)(((int64_t)UINT64_C(2))));
  t244 = ((int64_t)(v26));
  t245 = ((int64_t)(((int64_t)((uint64_t)(t243) * (uint64_t)(t244)))));
  t246 = ((int64_t)(((int64_t)UINT64_C(1))));
  t247 = ((int64_t)((uint64_t)(t245) + (uint64_t)(t246)));
  t248 = ((int32_t *)t242.data) + gg_index(t247, t242.len);
  t249 = ((int32_t)((*t248)));
  t250 = ((int32_t)(((int32_t)((uint64_t)(t241) * (uint64_t)(t249)))));
  v35 = ((int32_t)(((int32_t)((uint64_t)(t227) + (uint64_t)(t250)))));
  t251 = gg_view((v10).data, 32);
  t252 = ((int64_t)(v16));
  t253 = ((int64_t)(((int64_t)UINT64_C(8))));
  t254 = ((int64_t)(((int64_t)((uint64_t)(t252) * (uint64_t)(t253)))));
  t255 = ((int64_t)(v27));
  t256 = ((int64_t)((uint64_t)(t254) + (uint64_t)(t255)));
  t257 = ((float *)t251.data) + gg_index(t256, t251.len);
  t258 = ((float)(((float)(v28))));
  t259 = v3;
  t260 = ((int64_t)(v14));
  t261 = ((int64_t)(((int64_t)UINT64_C(2))));
  t262 = ((int64_t)(v27));
  t263 = ((int64_t)(((int64_t)((uint64_t)(t261) * (uint64_t)(t262)))));
  t264 = ((int64_t)(((int64_t)((uint64_t)(t260) + (uint64_t)(t263)))));
  t265 = gg_function_2(t259, t264, base + 0);
  t266 = ((uint16_t)(((uint16_t)(t265))));
  t267 = fg143_half(t266);
  t268 = ((float)(t267));
  t269 = ((float)(v17));
  t270 = ((float)(((t268) * (t269))));
  t271 = gg_view((v10).data, 32);
  t272 = ((int64_t)(v16));
  t273 = ((int64_t)(((int64_t)UINT64_C(8))));
  t274 = ((int64_t)(((int64_t)((uint64_t)(t272) * (uint64_t)(t273)))));
  t275 = ((int64_t)(v27));
  t276 = ((int64_t)((uint64_t)(t274) + (uint64_t)(t275)));
  t277 = ((float *)t271.data) + gg_index(t276, t271.len);
  t278 = ((float)((*t277)));
  t279 = fmaf(t258, t270, t278);
  (*t257) = ((float)(t279));
  t280 = gg_view((v11).data, 32);
  t281 = ((int64_t)(v16));
  t282 = ((int64_t)(((int64_t)UINT64_C(8))));
  t283 = ((int64_t)(((int64_t)((uint64_t)(t281) * (uint64_t)(t282)))));
  t284 = ((int64_t)(v27));
  t285 = ((int64_t)((uint64_t)(t283) + (uint64_t)(t284)));
  t286 = ((float *)t280.data) + gg_index(t285, t280.len);
  t287 = ((float)(((float)(v35))));
  t288 = v3;
  t289 = ((int64_t)(v14));
  t290 = ((int64_t)(((int64_t)UINT64_C(16))));
  t291 = ((int64_t)(((int64_t)((uint64_t)(t289) + (uint64_t)(t290)))));
  t292 = ((int64_t)(((int64_t)UINT64_C(2))));
  t293 = ((int64_t)(v27));
  t294 = ((int64_t)(((int64_t)((uint64_t)(t292) * (uint64_t)(t293)))));
  t295 = ((int64_t)(((int64_t)((uint64_t)(t291) + (uint64_t)(t294)))));
  t296 = gg_function_2(t288, t295, base + 0);
  t297 = ((uint16_t)(((uint16_t)(t296))));
  t298 = fg143_half(t297);
  t299 = ((float)(t298));
  t300 = ((float)(v17));
  t301 = ((float)(((t299) * (t300))));
  t302 = gg_view((v11).data, 32);
  t303 = ((int64_t)(v16));
  t304 = ((int64_t)(((int64_t)UINT64_C(8))));
  t305 = ((int64_t)(((int64_t)((uint64_t)(t303) * (uint64_t)(t304)))));
  t306 = ((int64_t)(v27));
  t307 = ((int64_t)((uint64_t)(t305) + (uint64_t)(t306)));
  t308 = ((float *)t302.data) + gg_index(t307, t302.len);
  t309 = ((float)((*t308)));
  t310 = fmaf(t287, t301, t309);
  (*t286) = ((float)(t310));
  }
label25:;
  t133++; goto label24;
label26:;
  }
label22:;
  t131++; goto label21;
label23:;
  }
label10:;
  t76++; goto label9;
label11:;
  }
label7:;
  t60++; goto label6;
label8:;
  t312 = ((int64_t)UINT64_C(4));
  t311 = 0;
label33:;
  if (t311 >= t312) goto label35;
  v36 = t311;
  {
  t314 = ((int64_t)UINT64_C(8));
  t313 = 0;
label36:;
  if (t313 >= t314) goto label38;
  v37 = t313;
  {
  t315 = v1;
  t316 = ((int64_t)(v8));
  t317 = ((int64_t)(((int64_t)UINT64_C(4))));
  t318 = ((int64_t)(((int64_t)((uint64_t)(t316) * (uint64_t)(t317)))));
  t319 = ((int64_t)(v36));
  t320 = ((int64_t)(((int64_t)((uint64_t)(t318) + (uint64_t)(t319)))));
  t321 = ((int64_t)(v2));
  t322 = ((int64_t)(((int64_t)((uint64_t)(t320) * (uint64_t)(t321)))));
  t323 = ((int64_t)(v9));
  t324 = ((int64_t)(((int64_t)UINT64_C(8))));
  t325 = ((int64_t)(((int64_t)((uint64_t)(t323) * (uint64_t)(t324)))));
  t326 = ((int64_t)(((int64_t)((uint64_t)(t322) + (uint64_t)(t325)))));
  t327 = ((int64_t)(v37));
  t328 = ((int64_t)((uint64_t)(t326) + (uint64_t)(t327)));
  t329 = ((float *)t315.data) + gg_index(t328, t315.len);
  t330 = gg_view((v10).data, 32);
  t331 = ((int64_t)(v36));
  t332 = ((int64_t)(((int64_t)UINT64_C(8))));
  t333 = ((int64_t)(((int64_t)((uint64_t)(t331) * (uint64_t)(t332)))));
  t334 = ((int64_t)(v37));
  t335 = ((int64_t)((uint64_t)(t333) + (uint64_t)(t334)));
  t336 = ((float *)t330.data) + gg_index(t335, t330.len);
  t337 = ((float)((*t336)));
  t338 = gg_view((v11).data, 32);
  t339 = ((int64_t)(v36));
  t340 = ((int64_t)(((int64_t)UINT64_C(8))));
  t341 = ((int64_t)(((int64_t)((uint64_t)(t339) * (uint64_t)(t340)))));
  t342 = ((int64_t)(v37));
  t343 = ((int64_t)((uint64_t)(t341) + (uint64_t)(t342)));
  t344 = ((float *)t338.data) + gg_index(t343, t338.len);
  t345 = ((float)((*t344)));
  (*t329) = ((float)(((t337) - (t345))));
  }
label37:;
  t313++; goto label36;
label38:;
  }
label34:;
  t311++; goto label33;
label35:;
  }
label4:;
  t50++; goto label3;
label5:;
  }
label1:;
  t46++; goto label0;
label2:;
  t346 = ((int64_t)(((int64_t)UINT64_C(3))));
  t347 = ((int64_t)(v5));
  t348 = ((int64_t)(((int64_t)UINT64_C(4))));
  t349 = ((int64_t)(((int64_t)gg_div(t347, t348))));
  t350 = ((int64_t)(v7));
  t351 = ((int64_t)(((int64_t)((uint64_t)(t349) * (uint64_t)(t350)))));
  t352 = ((int64_t)(v6));
  t353 = ((int64_t)(((int64_t)UINT64_C(8))));
  t354 = ((int64_t)(((int64_t)gg_div(t352, t353))));
  t355 = ((int64_t)(((int64_t)((uint64_t)(t351) * (uint64_t)(t354)))));
  fg006_repack_witness(t346, t355);
  }
gg_done:;
}
static void gg_function_9(int64_t p0, gg_slice p1, int64_t p2, gg_slice p3, gg_slice p4, int64_t p5, int64_t p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  uint8_t t17;
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
  float t44;
  float t45;
  int64_t t46;
  float t47;
  float t48;
  int64_t t49;
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
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  uint32_t t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int32_t t71;
  int32_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  gg_slice t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  gg_slice t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int32_t * t95;
  int32_t t96;
  int32_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  gg_slice t106;
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
  uint8_t * t124;
  uint8_t t125;
  uint8_t t126;
  uint8_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int32_t t137;
  int32_t t138;
  gg_slice t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  uint8_t * t145;
  int32_t t146;
  int32_t t147;
  gg_slice t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int32_t * t156;
  int32_t t157;
  int32_t t158;
  int32_t t159;
  int32_t t160;
  gg_slice t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint8_t * t169;
  int32_t t170;
  int32_t t171;
  gg_slice t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int32_t * t182;
  int32_t t183;
  int32_t t184;
  int64_t t185;
  int64_t t186;
  int32_t t187;
  gg_slice t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int32_t * t196;
  int32_t t197;
  int64_t t198;
  int32_t * t199;
  int32_t t200;
  int32_t t201;
  int64_t t202;
  float * t203;
  float t204;
  gg_slice t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  uint16_t t212;
  float t213;
  float t214;
  float t215;
  float t216;
  int64_t t217;
  float * t218;
  float t219;
  float t220;
  int64_t t221;
  float * t222;
  float t223;
  gg_slice t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  uint16_t t233;
  float t234;
  float t235;
  float t236;
  float t237;
  int64_t t238;
  float * t239;
  float t240;
  float t241;
  int64_t t242;
  int64_t t243;
  gg_slice t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  float * t254;
  int64_t t255;
  float * t256;
  float t257;
  int64_t t258;
  float * t259;
  float t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  gg_slice v3;
  gg_slice v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  gg_type_19 v10;
  gg_type_19 v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  float v15;
  gg_type_20 v16;
  gg_type_23 v17;
  int64_t v18;
  int32_t v19;
  int32_t v20;
  int64_t v21;
  int32_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  uint8_t v26;
  int32_t v27;
  int32_t v28;
  int64_t v29;
  int32_t v30;
  int64_t v31;
  int64_t v32;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(256))));
  t6 = ((int64_t)(((int64_t)gg_mod(t4, t5))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t6) == (t7));
  }
  t8 = t3;
  t9 = t8;
  if (t9) {
  t10 = ((int64_t)(v5));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t10) > (t11));
  }
  t12 = t9;
  t13 = t12;
  if (t13) {
  t14 = ((int64_t)(v6));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t14) > (t15));
  }
  t16 = t13;
  t17 = t16;
  if (t17) {
  t18 = ((int64_t)(v6));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((int64_t)(((int64_t)gg_mod(t18, t19))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t20) == (t21));
  }
  if (!(t17)) gg_fail("assertion failed");
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(256))));
  v7 = ((int64_t)(((int64_t)gg_div(t22, t23))));
  t24 = ((int64_t)((v3).len));
  t25 = ((int64_t)(v6));
  t26 = ((int64_t)(((int64_t)UINT64_C(8))));
  t27 = ((int64_t)(((int64_t)gg_div(t25, t26))));
  t28 = ((int64_t)(v7));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(((int64_t)UINT64_C(1152))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  if (!(((t24) == (t31)))) gg_fail("assertion failed");
  t32 = ((int64_t)((v4).len));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(v7));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((int64_t)(((int64_t)UINT64_C(292))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  if (!(((t32) == (t37)))) gg_fail("assertion failed");
  t39 = v5;
  t38 = 0;
label0:;
  if (t38 >= t39) goto label2;
  v8 = t38;
  {
  t41 = ((int64_t)(v6));
  t42 = ((int64_t)(((int64_t)UINT64_C(8))));
  t43 = ((int64_t)gg_div(t41, t42));
  t40 = 0;
label3:;
  if (t40 >= t43) goto label5;
  v9 = t40;
  {
  memset(&t44, 0, sizeof(t44));
  t45 = ((float)(t44));
  for (t46=0; t46<8; t46++) (v10).data[t46] = t45;
  memset(&t47, 0, sizeof(t47));
  t48 = ((float)(t47));
  for (t49=0; t49<8; t49++) (v11).data[t49] = t48;
  t51 = v7;
  t50 = 0;
label6:;
  if (t50 >= t51) goto label8;
  v12 = t50;
  {
  t52 = ((int64_t)(v8));
  t53 = ((int64_t)(v7));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  t55 = ((int64_t)(v12));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t55)))));
  t57 = ((int64_t)(((int64_t)UINT64_C(292))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  t58 = ((int64_t)(v9));
  t59 = ((int64_t)(v7));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(v12));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  t63 = ((int64_t)(((int64_t)UINT64_C(1152))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t64 = v4;
  t65 = ((int64_t)(v13));
  t66 = gg_function_10(t64, t65, base + 0);
  t67 = ((uint32_t)(((uint32_t)(t66))));
  t68 = fg143_from_bits(t67);
  v15 = ((float)(t68));
  t69 = v3;
  t70 = ((int64_t)(v14));
  v16 = gg_function_12(t69, t70, base + 0);
  memset(&t71, 0, sizeof(t71));
  t72 = ((int32_t)(t71));
  for (t73=0; t73<8; t73++) (v17).data[t73] = t72;
  t75 = ((int64_t)UINT64_C(8));
  t74 = 0;
label9:;
  if (t74 >= t75) goto label11;
  v18 = t74;
  {
  t76 = v4;
  t77 = ((int64_t)(v13));
  t78 = ((int64_t)(((int64_t)UINT64_C(260))));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  t80 = ((int64_t)(((int64_t)UINT64_C(4))));
  t81 = ((int64_t)(v18));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t82)))));
  t84 = gg_function_2(t76, t83, base + 0);
  v19 = ((int32_t)(((int32_t)(((int16_t)(t84))))));
  t85 = v4;
  t86 = ((int64_t)(v13));
  t87 = ((int64_t)(((int64_t)UINT64_C(262))));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))));
  t89 = ((int64_t)(((int64_t)UINT64_C(4))));
  t90 = ((int64_t)(v18));
  t91 = ((int64_t)(((int64_t)((uint64_t)(t89) * (uint64_t)(t90)))));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t91)))));
  t93 = gg_function_2(t85, t92, base + 0);
  v20 = ((int32_t)(((int32_t)(((int16_t)(t93))))));
  /* FG_DIRECT_SLICE_INDEX */
  t94 = v18;
  t95 = ((int32_t *)(v17).data) + gg_index(t94, ((int64_t)UINT64_C(8)));
  t96 = ((int32_t)(v19));
  t97 = ((int32_t)(v20));
  (*t95) = ((int32_t)(((int32_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  }
label10:;
  t74++; goto label9;
label11:;
  t99 = ((int64_t)UINT64_C(8));
  t98 = 0;
label12:;
  if (t98 >= t99) goto label14;
  v21 = t98;
  {
  v22 = ((int32_t)(((int64_t)UINT64_C(0))));
  t101 = ((int64_t)UINT64_C(4));
  t100 = 0;
label15:;
  if (t100 >= t101) goto label17;
  v23 = t100;
  {
  t103 = ((int64_t)UINT64_C(4));
  t102 = 0;
label18:;
  if (t102 >= t103) goto label20;
  v24 = t102;
  {
  t105 = ((int64_t)UINT64_C(8));
  t104 = 0;
label21:;
  if (t104 >= t105) goto label23;
  v25 = t104;
  {
  t106 = v3;
  t107 = ((int64_t)(v14));
  t108 = ((int64_t)(((int64_t)UINT64_C(128))));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t107) + (uint64_t)(t108)))));
  t110 = ((int64_t)(((int64_t)UINT64_C(4))));
  t111 = ((int64_t)(v23));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) * (uint64_t)(t111)))));
  t113 = ((int64_t)(v24));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) + (uint64_t)(t113)))));
  t115 = ((int64_t)(((int64_t)UINT64_C(64))));
  t116 = ((int64_t)(((int64_t)((uint64_t)(t114) * (uint64_t)(t115)))));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t109) + (uint64_t)(t116)))));
  t118 = ((int64_t)(v21));
  t119 = ((int64_t)(((int64_t)UINT64_C(8))));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t118) * (uint64_t)(t119)))));
  t121 = ((int64_t)(((int64_t)((uint64_t)(t117) + (uint64_t)(t120)))));
  t122 = ((int64_t)(v25));
  t123 = ((int64_t)((uint64_t)(t121) + (uint64_t)(t122)));
  t124 = ((uint8_t *)t106.data) + gg_index(t123, t106.len);
  v26 = ((uint8_t)((*t124)));
  t125 = ((uint8_t)(v26));
  t126 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v27 = ((int32_t)(((int32_t)(((t125) & (t126))))));
  t127 = ((uint8_t)(v26));
  t128 = ((int64_t)(((int64_t)UINT64_C(4))));
  v28 = ((int32_t)(((int32_t)(((t127) >> ((uint64_t)(t128) & 7))))));
  t129 = ((int64_t)(v23));
  t130 = ((int64_t)(((int64_t)UINT64_C(64))));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t129) * (uint64_t)(t130)))));
  t132 = ((int64_t)(v24));
  t133 = ((int64_t)(((int64_t)UINT64_C(8))));
  t134 = ((int64_t)(((int64_t)((uint64_t)(t132) * (uint64_t)(t133)))));
  t135 = ((int64_t)(((int64_t)((uint64_t)(t131) + (uint64_t)(t134)))));
  t136 = ((int64_t)(v25));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t135) + (uint64_t)(t136)))));
  t137 = v22;
  t138 = ((int32_t)(v27));
  t139 = v4;
  t140 = ((int64_t)(v13));
  t141 = ((int64_t)(((int64_t)UINT64_C(4))));
  t142 = ((int64_t)(((int64_t)((uint64_t)(t140) + (uint64_t)(t141)))));
  t143 = ((int64_t)(v29));
  t144 = ((int64_t)((uint64_t)(t142) + (uint64_t)(t143)));
  t145 = ((uint8_t *)t139.data) + gg_index(t144, t139.len);
  t146 = ((int32_t)(((int32_t)(((int8_t)((*t145)))))));
  t147 = ((int32_t)(((int32_t)((uint64_t)(t138) * (uint64_t)(t146)))));
  t148 = gg_view((v16).data, 128);
  t149 = ((int64_t)(((int64_t)UINT64_C(2))));
  t150 = ((int64_t)(v23));
  t151 = ((int64_t)(((int64_t)((uint64_t)(t149) * (uint64_t)(t150)))));
  t152 = ((int64_t)(((int64_t)UINT64_C(8))));
  t153 = ((int64_t)(((int64_t)((uint64_t)(t151) * (uint64_t)(t152)))));
  t154 = ((int64_t)(v21));
  t155 = ((int64_t)((uint64_t)(t153) + (uint64_t)(t154)));
  t156 = ((int32_t *)t148.data) + gg_index(t155, t148.len);
  t157 = ((int32_t)((*t156)));
  t158 = ((int32_t)(((int32_t)((uint64_t)(t147) * (uint64_t)(t157)))));
  v22 = ((int32_t)((uint64_t)(t137) + (uint64_t)(t158)));
  t159 = v22;
  t160 = ((int32_t)(v28));
  t161 = v4;
  t162 = ((int64_t)(v13));
  t163 = ((int64_t)(((int64_t)UINT64_C(4))));
  t164 = ((int64_t)(((int64_t)((uint64_t)(t162) + (uint64_t)(t163)))));
  t165 = ((int64_t)(v29));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t165)))));
  t167 = ((int64_t)(((int64_t)UINT64_C(32))));
  t168 = ((int64_t)((uint64_t)(t166) + (uint64_t)(t167)));
  t169 = ((uint8_t *)t161.data) + gg_index(t168, t161.len);
  t170 = ((int32_t)(((int32_t)(((int8_t)((*t169)))))));
  t171 = ((int32_t)(((int32_t)((uint64_t)(t160) * (uint64_t)(t170)))));
  t172 = gg_view((v16).data, 128);
  t173 = ((int64_t)(((int64_t)UINT64_C(2))));
  t174 = ((int64_t)(v23));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) * (uint64_t)(t174)))));
  t176 = ((int64_t)(((int64_t)UINT64_C(1))));
  t177 = ((int64_t)(((int64_t)((uint64_t)(t175) + (uint64_t)(t176)))));
  t178 = ((int64_t)(((int64_t)UINT64_C(8))));
  t179 = ((int64_t)(((int64_t)((uint64_t)(t177) * (uint64_t)(t178)))));
  t180 = ((int64_t)(v21));
  t181 = ((int64_t)((uint64_t)(t179) + (uint64_t)(t180)));
  t182 = ((int32_t *)t172.data) + gg_index(t181, t172.len);
  t183 = ((int32_t)((*t182)));
  t184 = ((int32_t)(((int32_t)((uint64_t)(t171) * (uint64_t)(t183)))));
  v22 = ((int32_t)((uint64_t)(t159) + (uint64_t)(t184)));
  }
label22:;
  t104++; goto label21;
label23:;
  }
label19:;
  t102++; goto label18;
label20:;
  }
label16:;
  t100++; goto label15;
label17:;
  v30 = ((int32_t)(((int64_t)UINT64_C(0))));
  t186 = ((int64_t)UINT64_C(8));
  t185 = 0;
label24:;
  if (t185 >= t186) goto label26;
  v31 = t185;
  {
  t187 = v30;
  t188 = gg_view((v16).data, 128);
  t189 = ((int64_t)(((int64_t)UINT64_C(64))));
  t190 = ((int64_t)(v31));
  t191 = ((int64_t)(((int64_t)UINT64_C(8))));
  t192 = ((int64_t)(((int64_t)((uint64_t)(t190) * (uint64_t)(t191)))));
  t193 = ((int64_t)(((int64_t)((uint64_t)(t189) + (uint64_t)(t192)))));
  t194 = ((int64_t)(v21));
  t195 = ((int64_t)((uint64_t)(t193) + (uint64_t)(t194)));
  t196 = ((int32_t *)t188.data) + gg_index(t195, t188.len);
  t197 = ((int32_t)((*t196)));
  /* FG_DIRECT_SLICE_INDEX */
  t198 = v31;
  t199 = ((int32_t *)(v17).data) + gg_index(t198, ((int64_t)UINT64_C(8)));
  t200 = ((int32_t)((*t199)));
  t201 = ((int32_t)(((int32_t)((uint64_t)(t197) * (uint64_t)(t200)))));
  v30 = ((int32_t)((uint64_t)(t187) + (uint64_t)(t201)));
  }
label25:;
  t185++; goto label24;
label26:;
  /* FG_DIRECT_SLICE_INDEX */
  t202 = v21;
  t203 = ((float *)(v10).data) + gg_index(t202, ((int64_t)UINT64_C(8)));
  t204 = ((float)(((float)(v22))));
  t205 = v3;
  t206 = ((int64_t)(v14));
  t207 = ((int64_t)(((int64_t)UINT64_C(2))));
  t208 = ((int64_t)(v21));
  t209 = ((int64_t)(((int64_t)((uint64_t)(t207) * (uint64_t)(t208)))));
  t210 = ((int64_t)(((int64_t)((uint64_t)(t206) + (uint64_t)(t209)))));
  t211 = gg_function_2(t205, t210, base + 0);
  t212 = ((uint16_t)(((uint16_t)(t211))));
  t213 = fg143_half(t212);
  t214 = ((float)(t213));
  t215 = ((float)(v15));
  t216 = ((float)(((t214) * (t215))));
  /* FG_DIRECT_SLICE_INDEX */
  t217 = v21;
  t218 = ((float *)(v10).data) + gg_index(t217, ((int64_t)UINT64_C(8)));
  t219 = ((float)((*t218)));
  t220 = fmaf(t204, t216, t219);
  (*t203) = ((float)(t220));
  /* FG_DIRECT_SLICE_INDEX */
  t221 = v21;
  t222 = ((float *)(v11).data) + gg_index(t221, ((int64_t)UINT64_C(8)));
  t223 = ((float)(((float)(v30))));
  t224 = v3;
  t225 = ((int64_t)(v14));
  t226 = ((int64_t)(((int64_t)UINT64_C(16))));
  t227 = ((int64_t)(((int64_t)((uint64_t)(t225) + (uint64_t)(t226)))));
  t228 = ((int64_t)(((int64_t)UINT64_C(2))));
  t229 = ((int64_t)(v21));
  t230 = ((int64_t)(((int64_t)((uint64_t)(t228) * (uint64_t)(t229)))));
  t231 = ((int64_t)(((int64_t)((uint64_t)(t227) + (uint64_t)(t230)))));
  t232 = gg_function_2(t224, t231, base + 0);
  t233 = ((uint16_t)(((uint16_t)(t232))));
  t234 = fg143_half(t233);
  t235 = ((float)(t234));
  t236 = ((float)(v15));
  t237 = ((float)(((t235) * (t236))));
  /* FG_DIRECT_SLICE_INDEX */
  t238 = v21;
  t239 = ((float *)(v11).data) + gg_index(t238, ((int64_t)UINT64_C(8)));
  t240 = ((float)((*t239)));
  t241 = fmaf(t223, t237, t240);
  (*t222) = ((float)(t241));
  }
label13:;
  t98++; goto label12;
label14:;
  }
label7:;
  t50++; goto label6;
label8:;
  t243 = ((int64_t)UINT64_C(8));
  t242 = 0;
label27:;
  if (t242 >= t243) goto label29;
  v32 = t242;
  {
  t244 = v1;
  t245 = ((int64_t)(v8));
  t246 = ((int64_t)(v5));
  t247 = ((int64_t)(((int64_t)((uint64_t)(t245) * (uint64_t)(t246)))));
  t248 = ((int64_t)(v9));
  t249 = ((int64_t)(((int64_t)UINT64_C(8))));
  t250 = ((int64_t)(((int64_t)((uint64_t)(t248) * (uint64_t)(t249)))));
  t251 = ((int64_t)(((int64_t)((uint64_t)(t247) + (uint64_t)(t250)))));
  t252 = ((int64_t)(v32));
  t253 = ((int64_t)((uint64_t)(t251) + (uint64_t)(t252)));
  t254 = ((float *)t244.data) + gg_index(t253, t244.len);
  /* FG_DIRECT_SLICE_INDEX */
  t255 = v32;
  t256 = ((float *)(v10).data) + gg_index(t255, ((int64_t)UINT64_C(8)));
  t257 = ((float)((*t256)));
  /* FG_DIRECT_SLICE_INDEX */
  t258 = v32;
  t259 = ((float *)(v11).data) + gg_index(t258, ((int64_t)UINT64_C(8)));
  t260 = ((float)((*t259)));
  (*t254) = ((float)(((t257) - (t260))));
  }
label28:;
  t242++; goto label27;
label29:;
  }
label4:;
  t40++; goto label3;
label5:;
  }
label1:;
  t38++; goto label0;
label2:;
  t261 = ((int64_t)(((int64_t)UINT64_C(2))));
  t262 = ((int64_t)(v5));
  t263 = ((int64_t)(v7));
  t264 = ((int64_t)(((int64_t)((uint64_t)(t262) * (uint64_t)(t263)))));
  t265 = ((int64_t)(v6));
  t266 = ((int64_t)(((int64_t)UINT64_C(8))));
  t267 = ((int64_t)(((int64_t)gg_div(t265, t266))));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t264) * (uint64_t)(t267)))));
  fg006_repack_witness(t261, t268);
  }
gg_done:;
}
static void gg_function_3(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t * t9;
  int64_t t10;
  int64_t t11;
  gg_slice t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t * t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)((v0).len));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) - (uint64_t)(t6)))));
  t3 = ((t4) <= (t7));
  }
  if (!(t3)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v1;
  t9 = ((uint8_t *)(v0).data) + gg_index(t8, (v0).len);
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(((int64_t)UINT64_C(255))));
  (*t9) = ((uint8_t)(((uint8_t)(((t10) & (t11))))));
  t12 = v0;
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  t16 = ((uint8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((int64_t)(((t17) >> ((uint64_t)(t18) & 63))));
  t20 = ((int64_t)(((int64_t)UINT64_C(255))));
  (*t16) = ((uint8_t)(((uint8_t)(((t19) & (t20))))));
  }
gg_done:;
}
static int32_t gg_function_8(float p0, int64_t base) {
  int32_t t0;
  float t1;
  float t2;
  float t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
  int32_t t7;
  int32_t t8;
  float v0;
  float v1;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = ((float)(((float)(gg_double_bits(UINT64_C(4713017010043224064))))));
  v1 = ((float)(((t1) + (t2))));
  t3 = ((float)(v1));
  t4 = fg_q8k_float_bits(t3);
  t5 = ((uint32_t)(t4));
  t6 = ((uint32_t)(((int64_t)UINT64_C(8388607))));
  t7 = ((int32_t)(((int32_t)(((t5) & (t6))))));
  t8 = ((int32_t)(((int64_t)UINT64_C(4194304))));
  t0 = ((int32_t)(((int32_t)((uint64_t)(t7) - (uint64_t)(t8)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_7(gg_slice p0, int64_t p1, uint32_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t * t9;
  uint32_t t10;
  uint32_t t11;
  gg_slice t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t * t16;
  uint32_t t17;
  int64_t t18;
  uint32_t t19;
  uint32_t t20;
  gg_slice t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t * t25;
  uint32_t t26;
  int64_t t27;
  uint32_t t28;
  uint32_t t29;
  gg_slice t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t * t34;
  uint32_t t35;
  int64_t t36;
  uint32_t t37;
  uint32_t t38;
  gg_slice v0;
  int64_t v1;
  uint32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)((v0).len));
  t6 = ((int64_t)(((int64_t)UINT64_C(4))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) - (uint64_t)(t6)))));
  t3 = ((t4) <= (t7));
  }
  if (!(t3)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v1;
  t9 = ((uint8_t *)(v0).data) + gg_index(t8, (v0).len);
  t10 = ((uint32_t)(v2));
  t11 = ((uint32_t)(((int64_t)UINT64_C(255))));
  (*t9) = ((uint8_t)(((uint8_t)(((t10) & (t11))))));
  t12 = v0;
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  t16 = ((uint8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((uint32_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((uint32_t)(((t17) >> ((uint64_t)(t18) & 31))));
  t20 = ((uint32_t)(((int64_t)UINT64_C(255))));
  (*t16) = ((uint8_t)(((uint8_t)(((t19) & (t20))))));
  t21 = v0;
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((uint8_t *)t21.data) + gg_index(t24, t21.len);
  t26 = ((uint32_t)(v2));
  t27 = ((int64_t)(((int64_t)UINT64_C(16))));
  t28 = ((uint32_t)(((t26) >> ((uint64_t)(t27) & 31))));
  t29 = ((uint32_t)(((int64_t)UINT64_C(255))));
  (*t25) = ((uint8_t)(((uint8_t)(((t28) & (t29))))));
  t30 = v0;
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  t33 = ((int64_t)((uint64_t)(t31) + (uint64_t)(t32)));
  t34 = ((uint8_t *)t30.data) + gg_index(t33, t30.len);
  t35 = ((uint32_t)(v2));
  t36 = ((int64_t)(((int64_t)UINT64_C(24))));
  t37 = ((uint32_t)(((t35) >> ((uint64_t)(t36) & 31))));
  t38 = ((uint32_t)(((int64_t)UINT64_C(255))));
  (*t34) = ((uint8_t)(((uint8_t)(((t37) & (t38))))));
  }
gg_done:;
}
static void gg_function_5(gg_slice p0, gg_slice p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
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
  float t18;
  float t19;
  int64_t t20;
  float t21;
  float t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  gg_slice t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  float * t40;
  gg_slice t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  float * t47;
  float t48;
  float t49;
  uint8_t t50;
  float t51;
  float t52;
  float t53;
  uint8_t t54;
  float t55;
  float t56;
  uint8_t t57;
  int64_t t58;
  int64_t t59;
  gg_slice t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  float * t66;
  float t67;
  float t68;
  uint8_t t69;
  float t70;
  float t71;
  uint8_t t72;
  uint8_t t73;
  float t74;
  float t75;
  float t76;
  float t77;
  float t78;
  int64_t t79;
  float * t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  float t87;
  float t88;
  uint8_t t89;
  float t90;
  float t91;
  gg_slice t92;
  int64_t t93;
  float t94;
  uint32_t t95;
  uint32_t t96;
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
  gg_slice t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  float * t119;
  float t120;
  int64_t t121;
  float * t122;
  float t123;
  float t124;
  gg_slice t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  uint8_t * t133;
  int32_t t134;
  int32_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int32_t t142;
  gg_slice t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
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
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  uint8_t * t161;
  int32_t t162;
  int64_t t163;
  int64_t t164;
  int32_t t165;
  gg_slice t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
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
  int64_t t183;
  int64_t t184;
  int64_t t185;
  uint8_t * t186;
  int32_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  gg_slice t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  gg_slice v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  gg_type_12 v4;
  gg_type_13 v5;
  int64_t v6;
  int64_t v7;
  float v8;
  uint8_t v9;
  int64_t v10;
  float v11;
  float v12;
  int64_t v13;
  float v14;
  int64_t v15;
  float v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int32_t v20;
  int64_t v21;
  int64_t v22;
  int32_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(256))));
  t6 = ((int64_t)(((int64_t)gg_mod(t4, t5))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t6) == (t7));
  }
  if (!(t3)) gg_fail("assertion failed");
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(256))));
  v3 = ((int64_t)(((int64_t)gg_div(t8, t9))));
  t10 = ((int64_t)((v0).len));
  t11 = ((int64_t)(((int64_t)UINT64_C(4))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  if (!(((t10) == (t13)))) gg_fail("assertion failed");
  t14 = ((int64_t)((v1).len));
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(((int64_t)UINT64_C(1168))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  if (!(((t14) == (t17)))) gg_fail("assertion failed");
  memset(&t18, 0, sizeof(t18));
  t19 = ((float)(t18));
  for (t20=0; t20<1024; t20++) (v4).data[t20] = t19;
  memset(&t21, 0, sizeof(t21));
  t22 = ((float)(t21));
  for (t23=0; t23<4; t23++) (v5).data[t23] = t22;
  t25 = v3;
  t24 = 0;
label0:;
  if (t24 >= t25) goto label2;
  v6 = t24;
  {
  t27 = ((int64_t)UINT64_C(4));
  t26 = 0;
label3:;
  if (t26 >= t27) goto label5;
  v7 = t26;
  {
  v8 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  v9 = ((uint8_t)UINT64_C(0));
  t29 = ((int64_t)UINT64_C(256));
  t28 = 0;
label6:;
  if (t28 >= t29) goto label8;
  v10 = t28;
  {
  t30 = v0;
  t31 = ((int64_t)(v7));
  t32 = ((int64_t)(v2));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t34 = ((int64_t)(v6));
  t35 = ((int64_t)(((int64_t)UINT64_C(256))));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t36)))));
  t38 = ((int64_t)(v10));
  t39 = ((int64_t)((uint64_t)(t37) + (uint64_t)(t38)));
  t40 = ((float *)t30.data) + gg_index(t39, t30.len);
  v11 = ((float)((*t40)));
  t41 = gg_view((v4).data, 1024);
  t42 = ((int64_t)(v7));
  t43 = ((int64_t)(((int64_t)UINT64_C(256))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(v10));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((float *)t41.data) + gg_index(t46, t41.len);
  (*t47) = ((float)(v11));
  t48 = ((float)(v11));
  t49 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t50 = ((t48) < (t49));
  if (t50) {
  {
  t51 = v11;
  v12 = ((float)((-(t51))));
  }
  } else {
  {
  v12 = ((float)(v11));
  }
  }
  t52 = ((float)(v12));
  t53 = ((float)(v8));
  t54 = ((t52) > (t53));
  if (t54) {
  {
  v8 = ((float)(v12));
  }
  }
  }
label7:;
  t28++; goto label6;
label8:;
  t55 = ((float)(v8));
  t56 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t57 = ((t55) > (t56));
  if (t57) {
  {
  t59 = ((int64_t)UINT64_C(256));
  t58 = 0;
label9:;
  if (t58 >= t59) goto label11;
  v13 = t58;
  {
  t60 = gg_view((v4).data, 1024);
  t61 = ((int64_t)(v7));
  t62 = ((int64_t)(((int64_t)UINT64_C(256))));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) * (uint64_t)(t62)))));
  t64 = ((int64_t)(v13));
  t65 = ((int64_t)((uint64_t)(t63) + (uint64_t)(t64)));
  t66 = ((float *)t60.data) + gg_index(t65, t60.len);
  t67 = ((float)((*t66)));
  t68 = ((float)(v8));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  v9 = ((uint8_t)UINT64_C(1));
  }
  }
  }
label10:;
  t58++; goto label9;
label11:;
  }
  }
  t70 = ((float)(v8));
  t71 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t72 = ((t70) == (t71));
  if (t72) {
  {
  v14 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  } else {
  t73 = v9;
  if (t73) {
  {
  t74 = ((float)(gg_double_bits(UINT64_C(4638637247447433216))));
  t75 = ((float)((-(t74))));
  t76 = ((float)(v8));
  v14 = ((float)(((t75) / (t76))));
  }
  } else {
  {
  t77 = ((float)(((float)(gg_double_bits(UINT64_C(4638637247447433216))))));
  t78 = ((float)(v8));
  v14 = ((float)(((t77) / (t78))));
  }
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t79 = v7;
  t80 = ((float *)(v5).data) + gg_index(t79, ((int64_t)UINT64_C(4)));
  (*t80) = ((float)(v14));
  t81 = ((int64_t)(v6));
  t82 = ((int64_t)(((int64_t)UINT64_C(1168))));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t82)))));
  t84 = ((int64_t)(v7));
  t85 = ((int64_t)(((int64_t)UINT64_C(4))));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t83) + (uint64_t)(t86)))));
  t87 = ((float)(v8));
  t88 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t89 = ((t87) == (t88));
  if (t89) {
  {
  v16 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  } else {
  {
  t90 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t91 = ((float)(v14));
  v16 = ((float)(((t90) / (t91))));
  }
  }
  t92 = v1;
  t93 = ((int64_t)(v15));
  t94 = ((float)(v16));
  t95 = fg_q8k_float_bits(t94);
  t96 = ((uint32_t)(t95));
  gg_function_7(t92, t93, t96, base + 0);
  }
label4:;
  t26++; goto label3;
label5:;
  t98 = ((int64_t)(((int64_t)UINT64_C(256))));
  t99 = ((int64_t)(((int64_t)UINT64_C(4))));
  t100 = ((int64_t)((uint64_t)(t98) * (uint64_t)(t99)));
  t97 = 0;
label12:;
  if (t97 >= t100) goto label14;
  v17 = t97;
  {
  t101 = ((int64_t)(v17));
  t102 = ((int64_t)(((int64_t)UINT64_C(32))));
  t103 = ((int64_t)(((int64_t)gg_mod(t101, t102))));
  t104 = ((int64_t)(((int64_t)UINT64_C(8))));
  v18 = ((int64_t)(((int64_t)gg_div(t103, t104))));
  t105 = ((int64_t)(v17));
  t106 = ((int64_t)(((int64_t)UINT64_C(32))));
  t107 = ((int64_t)(((int64_t)gg_div(t105, t106))));
  t108 = ((int64_t)(((int64_t)UINT64_C(8))));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t107) * (uint64_t)(t108)))));
  t110 = ((int64_t)(v17));
  t111 = ((int64_t)(((int64_t)UINT64_C(8))));
  t112 = ((int64_t)(((int64_t)gg_mod(t110, t111))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t109) + (uint64_t)(t112)))));
  t113 = gg_view((v4).data, 1024);
  t114 = ((int64_t)(v18));
  t115 = ((int64_t)(((int64_t)UINT64_C(256))));
  t116 = ((int64_t)(((int64_t)((uint64_t)(t114) * (uint64_t)(t115)))));
  t117 = ((int64_t)(v19));
  t118 = ((int64_t)((uint64_t)(t116) + (uint64_t)(t117)));
  t119 = ((float *)t113.data) + gg_index(t118, t113.len);
  t120 = ((float)((*t119)));
  /* FG_DIRECT_SLICE_INDEX */
  t121 = v18;
  t122 = ((float *)(v5).data) + gg_index(t121, ((int64_t)UINT64_C(4)));
  t123 = ((float)((*t122)));
  t124 = ((float)(((t120) * (t123))));
  v20 = gg_function_8(t124, base + 0);
  t125 = v1;
  t126 = ((int64_t)(v6));
  t127 = ((int64_t)(((int64_t)UINT64_C(1168))));
  t128 = ((int64_t)(((int64_t)((uint64_t)(t126) * (uint64_t)(t127)))));
  t129 = ((int64_t)(((int64_t)UINT64_C(16))));
  t130 = ((int64_t)(((int64_t)((uint64_t)(t128) + (uint64_t)(t129)))));
  t131 = ((int64_t)(v17));
  t132 = ((int64_t)((uint64_t)(t130) + (uint64_t)(t131)));
  t133 = ((uint8_t *)t125.data) + gg_index(t132, t125.len);
  t134 = ((int32_t)(v20));
  t135 = ((int32_t)(((int64_t)UINT64_C(255))));
  (*t133) = ((uint8_t)(((uint8_t)(((t134) & (t135))))));
  }
label13:;
  t97++; goto label12;
label14:;
  t137 = ((int64_t)UINT64_C(4));
  t136 = 0;
label15:;
  if (t136 >= t137) goto label17;
  v21 = t136;
  {
  t139 = ((int64_t)UINT64_C(16));
  t138 = 0;
label18:;
  if (t138 >= t139) goto label20;
  v22 = t138;
  {
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
  t141 = ((int64_t)UINT64_C(8));
  t140 = 0;
label21:;
  if (t140 >= t141) goto label23;
  v24 = t140;
  {
  t142 = v23;
  t143 = v1;
  t144 = ((int64_t)(v6));
  t145 = ((int64_t)(((int64_t)UINT64_C(1168))));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t145)))));
  t147 = ((int64_t)(((int64_t)UINT64_C(16))));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t146) + (uint64_t)(t147)))));
  t149 = ((int64_t)(((int64_t)UINT64_C(2))));
  t150 = ((int64_t)(v22));
  t151 = ((int64_t)(((int64_t)((uint64_t)(t149) * (uint64_t)(t150)))));
  t152 = ((int64_t)(((int64_t)UINT64_C(32))));
  t153 = ((int64_t)(((int64_t)((uint64_t)(t151) * (uint64_t)(t152)))));
  t154 = ((int64_t)(v21));
  t155 = ((int64_t)(((int64_t)UINT64_C(8))));
  t156 = ((int64_t)(((int64_t)((uint64_t)(t154) * (uint64_t)(t155)))));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t156)))));
  t158 = ((int64_t)(v24));
  t159 = ((int64_t)(((int64_t)((uint64_t)(t157) + (uint64_t)(t158)))));
  t160 = ((int64_t)((uint64_t)(t148) + (uint64_t)(t159)));
  t161 = ((uint8_t *)t143.data) + gg_index(t160, t143.len);
  t162 = ((int32_t)(((int32_t)(((int8_t)((*t161)))))));
  v23 = ((int32_t)((uint64_t)(t142) + (uint64_t)(t162)));
  }
label22:;
  t140++; goto label21;
label23:;
  t164 = ((int64_t)UINT64_C(8));
  t163 = 0;
label24:;
  if (t163 >= t164) goto label26;
  v25 = t163;
  {
  t165 = v23;
  t166 = v1;
  t167 = ((int64_t)(v6));
  t168 = ((int64_t)(((int64_t)UINT64_C(1168))));
  t169 = ((int64_t)(((int64_t)((uint64_t)(t167) * (uint64_t)(t168)))));
  t170 = ((int64_t)(((int64_t)UINT64_C(16))));
  t171 = ((int64_t)(((int64_t)((uint64_t)(t169) + (uint64_t)(t170)))));
  t172 = ((int64_t)(((int64_t)UINT64_C(2))));
  t173 = ((int64_t)(v22));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t172) * (uint64_t)(t173)))));
  t175 = ((int64_t)(((int64_t)UINT64_C(1))));
  t176 = ((int64_t)(((int64_t)((uint64_t)(t174) + (uint64_t)(t175)))));
  t177 = ((int64_t)(((int64_t)UINT64_C(32))));
  t178 = ((int64_t)(((int64_t)((uint64_t)(t176) * (uint64_t)(t177)))));
  t179 = ((int64_t)(v21));
  t180 = ((int64_t)(((int64_t)UINT64_C(8))));
  t181 = ((int64_t)(((int64_t)((uint64_t)(t179) * (uint64_t)(t180)))));
  t182 = ((int64_t)(((int64_t)((uint64_t)(t178) + (uint64_t)(t181)))));
  t183 = ((int64_t)(v25));
  t184 = ((int64_t)(((int64_t)((uint64_t)(t182) + (uint64_t)(t183)))));
  t185 = ((int64_t)((uint64_t)(t171) + (uint64_t)(t184)));
  t186 = ((uint8_t *)t166.data) + gg_index(t185, t166.len);
  t187 = ((int32_t)(((int32_t)(((int8_t)((*t186)))))));
  v23 = ((int32_t)((uint64_t)(t165) + (uint64_t)(t187)));
  }
label25:;
  t163++; goto label24;
label26:;
  t188 = ((int64_t)(v21));
  t189 = ((int64_t)(((int64_t)UINT64_C(4))));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t188) * (uint64_t)(t189)))));
  t191 = ((int64_t)(v22));
  t192 = ((int64_t)(((int64_t)UINT64_C(4))));
  t193 = ((int64_t)(((int64_t)gg_div(t191, t192))));
  t194 = ((int64_t)(((int64_t)UINT64_C(16))));
  t195 = ((int64_t)(((int64_t)((uint64_t)(t193) * (uint64_t)(t194)))));
  t196 = ((int64_t)(((int64_t)((uint64_t)(t190) + (uint64_t)(t195)))));
  t197 = ((int64_t)(v22));
  t198 = ((int64_t)(((int64_t)UINT64_C(4))));
  t199 = ((int64_t)(((int64_t)gg_mod(t197, t198))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t196) + (uint64_t)(t199)))));
  t200 = v1;
  t201 = ((int64_t)(v6));
  t202 = ((int64_t)(((int64_t)UINT64_C(1168))));
  t203 = ((int64_t)(((int64_t)((uint64_t)(t201) * (uint64_t)(t202)))));
  t204 = ((int64_t)(((int64_t)UINT64_C(1040))));
  t205 = ((int64_t)(((int64_t)((uint64_t)(t203) + (uint64_t)(t204)))));
  t206 = ((int64_t)(((int64_t)UINT64_C(2))));
  t207 = ((int64_t)(v26));
  t208 = ((int64_t)(((int64_t)((uint64_t)(t206) * (uint64_t)(t207)))));
  t209 = ((int64_t)(((int64_t)((uint64_t)(t205) + (uint64_t)(t208)))));
  t210 = ((int64_t)(v23));
  gg_function_3(t200, t209, t210, base + 0);
  }
label19:;
  t138++; goto label18;
label20:;
  }
label16:;
  t136++; goto label15;
label17:;
  }
label1:;
  t24++; goto label0;
label2:;
  t211 = ((int64_t)(((int64_t)UINT64_C(1))));
  t212 = ((int64_t)(v3));
  fg006_repack_witness(t211, t212);
  }
gg_done:;
}
static void gg_function_1(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  uint8_t t5;
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
  gg_slice t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  gg_slice t58;
  int64_t t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
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
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  gg_slice t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint8_t * t106;
  gg_slice t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  uint8_t * t113;
  uint8_t t114;
  uint8_t t115;
  int64_t t116;
  uint8_t t117;
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  uint8_t * t135;
  gg_slice t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  uint8_t * t142;
  uint8_t t143;
  uint8_t t144;
  int64_t t145;
  uint8_t * t146;
  gg_slice t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint8_t * t155;
  uint8_t t156;
  uint8_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  gg_slice t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint8_t * t168;
  int64_t t169;
  uint8_t * t170;
  uint8_t t171;
  uint8_t t172;
  uint8_t t173;
  int64_t t174;
  uint8_t * t175;
  uint8_t t176;
  uint8_t t177;
  uint8_t t178;
  int64_t t179;
  uint8_t t180;
  gg_slice t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  uint8_t * t185;
  int64_t t186;
  uint8_t * t187;
  uint8_t t188;
  uint8_t t189;
  uint8_t t190;
  int64_t t191;
  uint8_t * t192;
  uint8_t t193;
  uint8_t t194;
  uint8_t t195;
  int64_t t196;
  uint8_t t197;
  gg_slice t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  uint8_t * t202;
  int64_t t203;
  uint8_t * t204;
  uint8_t t205;
  uint8_t t206;
  uint8_t t207;
  int64_t t208;
  uint8_t * t209;
  uint8_t t210;
  uint8_t t211;
  uint8_t t212;
  int64_t t213;
  uint8_t t214;
  gg_slice t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  uint8_t * t219;
  int64_t t220;
  uint8_t * t221;
  uint8_t t222;
  uint8_t t223;
  uint8_t t224;
  int64_t t225;
  uint8_t * t226;
  uint8_t t227;
  uint8_t t228;
  uint8_t t229;
  int64_t t230;
  uint8_t t231;
  gg_slice t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  uint8_t * t236;
  int64_t t237;
  uint8_t * t238;
  uint8_t t239;
  uint8_t t240;
  uint8_t t241;
  int64_t t242;
  uint8_t * t243;
  uint8_t t244;
  uint8_t t245;
  uint8_t t246;
  int64_t t247;
  uint8_t t248;
  gg_slice t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  uint8_t * t253;
  int64_t t254;
  uint8_t * t255;
  uint8_t t256;
  uint8_t t257;
  uint8_t t258;
  int64_t t259;
  uint8_t * t260;
  uint8_t t261;
  uint8_t t262;
  uint8_t t263;
  int64_t t264;
  uint8_t t265;
  gg_slice t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  uint8_t * t270;
  int64_t t271;
  uint8_t * t272;
  uint8_t t273;
  uint8_t t274;
  uint8_t t275;
  int64_t t276;
  uint8_t * t277;
  uint8_t t278;
  uint8_t t279;
  uint8_t t280;
  int64_t t281;
  uint8_t t282;
  gg_slice t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  uint8_t * t287;
  int64_t t288;
  uint8_t * t289;
  uint8_t t290;
  uint8_t t291;
  uint8_t t292;
  int64_t t293;
  uint8_t * t294;
  uint8_t t295;
  uint8_t t296;
  uint8_t t297;
  int64_t t298;
  uint8_t t299;
  gg_slice t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  uint8_t * t304;
  int64_t t305;
  uint8_t * t306;
  uint8_t t307;
  uint8_t t308;
  uint8_t t309;
  int64_t t310;
  uint8_t * t311;
  uint8_t t312;
  uint8_t t313;
  uint8_t t314;
  int64_t t315;
  uint8_t t316;
  gg_slice t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  uint8_t * t321;
  int64_t t322;
  uint8_t * t323;
  uint8_t t324;
  uint8_t t325;
  uint8_t t326;
  int64_t t327;
  uint8_t * t328;
  uint8_t t329;
  uint8_t t330;
  uint8_t t331;
  int64_t t332;
  uint8_t t333;
  gg_slice t334;
  int64_t t335;
  int64_t t336;
  int64_t t337;
  uint8_t * t338;
  int64_t t339;
  uint8_t * t340;
  uint8_t t341;
  uint8_t t342;
  uint8_t t343;
  int64_t t344;
  uint8_t * t345;
  uint8_t t346;
  uint8_t t347;
  uint8_t t348;
  int64_t t349;
  uint8_t t350;
  gg_slice t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  uint8_t * t355;
  int64_t t356;
  uint8_t * t357;
  uint8_t t358;
  uint8_t t359;
  uint8_t t360;
  int64_t t361;
  uint8_t * t362;
  uint8_t t363;
  uint8_t t364;
  uint8_t t365;
  int64_t t366;
  uint8_t t367;
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
  int64_t t381;
  int64_t t382;
  uint8_t * t383;
  gg_slice t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  int64_t t388;
  int64_t t389;
  uint8_t * t390;
  uint8_t t391;
  uint8_t t392;
  uint8_t t393;
  int64_t t394;
  uint8_t t395;
  gg_slice t396;
  int64_t t397;
  int64_t t398;
  int64_t t399;
  int64_t t400;
  int64_t t401;
  int64_t t402;
  int64_t t403;
  uint8_t * t404;
  uint8_t t405;
  uint8_t t406;
  uint8_t t407;
  int64_t t408;
  uint8_t * t409;
  gg_slice t410;
  int64_t t411;
  int64_t t412;
  int64_t t413;
  int64_t t414;
  int64_t t415;
  int64_t t416;
  int64_t t417;
  uint8_t * t418;
  uint8_t t419;
  uint8_t t420;
  uint8_t t421;
  int64_t t422;
  uint8_t t423;
  gg_slice t424;
  int64_t t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  int64_t t429;
  int64_t t430;
  int64_t t431;
  uint8_t * t432;
  uint8_t t433;
  uint8_t t434;
  uint8_t t435;
  int64_t t436;
  uint8_t t437;
  int64_t t438;
  int64_t t439;
  int64_t t440;
  int64_t t441;
  int64_t t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  gg_slice t446;
  int64_t t447;
  int64_t t448;
  int64_t t449;
  uint8_t * t450;
  int64_t t451;
  uint8_t * t452;
  uint8_t t453;
  uint8_t t454;
  uint8_t t455;
  int64_t t456;
  uint8_t * t457;
  uint8_t t458;
  uint8_t t459;
  uint8_t t460;
  int64_t t461;
  uint8_t t462;
  gg_slice t463;
  int64_t t464;
  int64_t t465;
  int64_t t466;
  uint8_t * t467;
  int64_t t468;
  uint8_t * t469;
  uint8_t t470;
  uint8_t t471;
  uint8_t t472;
  int64_t t473;
  uint8_t * t474;
  uint8_t t475;
  uint8_t t476;
  uint8_t t477;
  int64_t t478;
  uint8_t t479;
  gg_slice t480;
  int64_t t481;
  int64_t t482;
  int64_t t483;
  uint8_t * t484;
  int64_t t485;
  uint8_t * t486;
  uint8_t t487;
  uint8_t t488;
  uint8_t t489;
  int64_t t490;
  uint8_t * t491;
  uint8_t t492;
  uint8_t t493;
  uint8_t t494;
  int64_t t495;
  uint8_t t496;
  gg_slice t497;
  int64_t t498;
  int64_t t499;
  int64_t t500;
  uint8_t * t501;
  int64_t t502;
  uint8_t * t503;
  uint8_t t504;
  uint8_t t505;
  uint8_t t506;
  int64_t t507;
  uint8_t * t508;
  uint8_t t509;
  uint8_t t510;
  uint8_t t511;
  int64_t t512;
  uint8_t t513;
  gg_slice t514;
  int64_t t515;
  int64_t t516;
  int64_t t517;
  uint8_t * t518;
  int64_t t519;
  uint8_t * t520;
  uint8_t t521;
  uint8_t t522;
  uint8_t t523;
  int64_t t524;
  uint8_t * t525;
  uint8_t t526;
  uint8_t t527;
  uint8_t t528;
  int64_t t529;
  uint8_t t530;
  gg_slice t531;
  int64_t t532;
  int64_t t533;
  int64_t t534;
  uint8_t * t535;
  int64_t t536;
  uint8_t * t537;
  uint8_t t538;
  uint8_t t539;
  uint8_t t540;
  int64_t t541;
  uint8_t * t542;
  uint8_t t543;
  uint8_t t544;
  uint8_t t545;
  int64_t t546;
  uint8_t t547;
  gg_slice t548;
  int64_t t549;
  int64_t t550;
  int64_t t551;
  uint8_t * t552;
  int64_t t553;
  uint8_t * t554;
  uint8_t t555;
  uint8_t t556;
  uint8_t t557;
  int64_t t558;
  uint8_t * t559;
  uint8_t t560;
  uint8_t t561;
  uint8_t t562;
  int64_t t563;
  uint8_t t564;
  gg_slice t565;
  int64_t t566;
  int64_t t567;
  int64_t t568;
  uint8_t * t569;
  int64_t t570;
  uint8_t * t571;
  uint8_t t572;
  uint8_t t573;
  uint8_t t574;
  int64_t t575;
  uint8_t * t576;
  uint8_t t577;
  uint8_t t578;
  uint8_t t579;
  int64_t t580;
  uint8_t t581;
  gg_slice t582;
  int64_t t583;
  int64_t t584;
  int64_t t585;
  uint8_t * t586;
  int64_t t587;
  uint8_t * t588;
  uint8_t t589;
  uint8_t t590;
  uint8_t t591;
  int64_t t592;
  uint8_t * t593;
  uint8_t t594;
  uint8_t t595;
  uint8_t t596;
  int64_t t597;
  uint8_t t598;
  gg_slice t599;
  int64_t t600;
  int64_t t601;
  int64_t t602;
  uint8_t * t603;
  int64_t t604;
  uint8_t * t605;
  uint8_t t606;
  uint8_t t607;
  uint8_t t608;
  int64_t t609;
  uint8_t * t610;
  uint8_t t611;
  uint8_t t612;
  uint8_t t613;
  int64_t t614;
  uint8_t t615;
  gg_slice t616;
  int64_t t617;
  int64_t t618;
  int64_t t619;
  uint8_t * t620;
  int64_t t621;
  uint8_t * t622;
  uint8_t t623;
  uint8_t t624;
  uint8_t t625;
  int64_t t626;
  uint8_t * t627;
  uint8_t t628;
  uint8_t t629;
  uint8_t t630;
  int64_t t631;
  uint8_t t632;
  gg_slice t633;
  int64_t t634;
  int64_t t635;
  int64_t t636;
  uint8_t * t637;
  int64_t t638;
  uint8_t * t639;
  uint8_t t640;
  uint8_t t641;
  uint8_t t642;
  int64_t t643;
  uint8_t * t644;
  uint8_t t645;
  uint8_t t646;
  uint8_t t647;
  int64_t t648;
  uint8_t t649;
  int64_t t650;
  int64_t t651;
  int64_t t652;
  int64_t t653;
  int64_t t654;
  int64_t t655;
  gg_slice v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  gg_type_10 v16;
  gg_type_10 v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t0) == (t1)))) gg_fail("assertion failed");
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) > (t3));
  t5 = t4;
  if (t5) {
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)(((int64_t)gg_mod(t6, t7))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t8) == (t9));
  }
  t10 = t5;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) > (t13));
  }
  if (!(t11)) gg_fail("assertion failed");
  t14 = ((int64_t)((v0).len));
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(8))));
  t17 = ((int64_t)(((int64_t)gg_div(t15, t16))));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(((int64_t)UINT64_C(1152))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  if (!(((t14) == (t21)))) gg_fail("assertion failed");
  t22 = ((int64_t)((v1).len));
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t26 = ((int64_t)(((int64_t)UINT64_C(144))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  if (!(((t22) == (t27)))) gg_fail("assertion failed");
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(((int64_t)UINT64_C(8))));
  t31 = ((int64_t)gg_div(t29, t30));
  t28 = 0;
label0:;
  if (t28 >= t31) goto label2;
  v5 = t28;
  {
  t33 = v3;
  t32 = 0;
label3:;
  if (t32 >= t33) goto label5;
  v6 = t32;
  {
  t34 = ((int64_t)(v5));
  t35 = ((int64_t)(v3));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  t37 = ((int64_t)(v6));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)UINT64_C(1152))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  t41 = ((int64_t)UINT64_C(8));
  t40 = 0;
label6:;
  if (t40 >= t41) goto label8;
  v8 = t40;
  {
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(((int64_t)UINT64_C(8))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(v8));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((int64_t)(v3));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)(v6));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  t51 = ((int64_t)(((int64_t)UINT64_C(144))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t52 = v0;
  t53 = ((int64_t)(v7));
  t54 = ((int64_t)(((int64_t)UINT64_C(2))));
  t55 = ((int64_t)(v8));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t56)))));
  t58 = v1;
  t59 = ((int64_t)(v9));
  t60 = gg_function_2(t58, t59, base + 0);
  gg_function_3(t52, t57, t60, base + 0);
  t61 = v0;
  t62 = ((int64_t)(v7));
  t63 = ((int64_t)(((int64_t)UINT64_C(16))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t66 = ((int64_t)(v8));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t67)))));
  t69 = v1;
  t70 = ((int64_t)(v9));
  t71 = ((int64_t)(((int64_t)UINT64_C(2))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  t73 = gg_function_2(t69, t72, base + 0);
  gg_function_3(t61, t68, t73, base + 0);
  }
label7:;
  t40++; goto label6;
label8:;
  t75 = ((int64_t)UINT64_C(128));
  t74 = 0;
label9:;
  if (t74 >= t75) goto label11;
  v10 = t74;
  {
  t76 = ((int64_t)(v10));
  t77 = ((int64_t)(((int64_t)UINT64_C(8))));
  v11 = ((int64_t)(((int64_t)gg_mod(t76, t77))));
  t78 = ((int64_t)(v5));
  t79 = ((int64_t)(((int64_t)UINT64_C(8))));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  t81 = ((int64_t)(v11));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t81)))));
  t83 = ((int64_t)(v3));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) * (uint64_t)(t83)))));
  t85 = ((int64_t)(v6));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t85)))));
  t87 = ((int64_t)(((int64_t)UINT64_C(144))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t87)))));
  t88 = ((int64_t)(((int64_t)UINT64_C(16))));
  t89 = ((int64_t)(v10));
  t90 = ((int64_t)(((int64_t)UINT64_C(8))));
  t91 = ((int64_t)(((int64_t)gg_div(t89, t90))));
  t92 = ((int64_t)(((int64_t)UINT64_C(8))));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t92)))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t93)))));
  t94 = ((int64_t)(v7));
  t95 = ((int64_t)(((int64_t)UINT64_C(128))));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t94) + (uint64_t)(t95)))));
  t97 = ((int64_t)(v10));
  t98 = ((int64_t)(((int64_t)UINT64_C(8))));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t97) * (uint64_t)(t98)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t99)))));
  t101 = ((int64_t)UINT64_C(8));
  t100 = 0;
label12:;
  if (t100 >= t101) goto label14;
  v15 = t100;
  {
  t102 = v0;
  t103 = ((int64_t)(v14));
  t104 = ((int64_t)(v15));
  t105 = ((int64_t)((uint64_t)(t103) + (uint64_t)(t104)));
  t106 = ((uint8_t *)t102.data) + gg_index(t105, t102.len);
  t107 = v1;
  t108 = ((int64_t)(v12));
  t109 = ((int64_t)(v13));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) + (uint64_t)(t109)))));
  t111 = ((int64_t)(v15));
  t112 = ((int64_t)((uint64_t)(t110) + (uint64_t)(t111)));
  t113 = ((uint8_t *)t107.data) + gg_index(t112, t107.len);
  (*t106) = ((uint8_t)((*t113)));
  }
label13:;
  t100++; goto label12;
label14:;
  }
label10:;
  t74++; goto label9;
label11:;
  memset(&t114, 0, sizeof(t114));
  t115 = ((uint8_t)(t114));
  for (t116=0; t116<8; t116++) (v16).data[t116] = t115;
  memset(&t117, 0, sizeof(t117));
  t118 = ((uint8_t)(t117));
  for (t119=0; t119<8; t119++) (v17).data[t119] = t118;
  t121 = ((int64_t)UINT64_C(4));
  t120 = 0;
label15:;
  if (t120 >= t121) goto label17;
  v18 = t120;
  {
  t123 = ((int64_t)UINT64_C(8));
  t122 = 0;
label18:;
  if (t122 >= t123) goto label20;
  v19 = t122;
  {
  t124 = ((int64_t)(v5));
  t125 = ((int64_t)(((int64_t)UINT64_C(8))));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t124) * (uint64_t)(t125)))));
  t127 = ((int64_t)(v19));
  t128 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t127)))));
  t129 = ((int64_t)(v3));
  t130 = ((int64_t)(((int64_t)((uint64_t)(t128) * (uint64_t)(t129)))));
  t131 = ((int64_t)(v6));
  t132 = ((int64_t)(((int64_t)((uint64_t)(t130) + (uint64_t)(t131)))));
  t133 = ((int64_t)(((int64_t)UINT64_C(144))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t132) * (uint64_t)(t133)))));
  /* FG_DIRECT_SLICE_INDEX */
  t134 = v19;
  t135 = ((uint8_t *)(v16).data) + gg_index(t134, ((int64_t)UINT64_C(8)));
  t136 = v1;
  t137 = ((int64_t)(v20));
  t138 = ((int64_t)(((int64_t)UINT64_C(4))));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t137) + (uint64_t)(t138)))));
  t140 = ((int64_t)(v18));
  t141 = ((int64_t)((uint64_t)(t139) + (uint64_t)(t140)));
  t142 = ((uint8_t *)t136.data) + gg_index(t141, t136.len);
  t143 = ((uint8_t)((*t142)));
  t144 = ((uint8_t)(((int64_t)UINT64_C(63))));
  (*t135) = ((uint8_t)(((t143) & (t144))));
  /* FG_DIRECT_SLICE_INDEX */
  t145 = v19;
  t146 = ((uint8_t *)(v17).data) + gg_index(t145, ((int64_t)UINT64_C(8)));
  t147 = v1;
  t148 = ((int64_t)(v20));
  t149 = ((int64_t)(((int64_t)UINT64_C(4))));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) + (uint64_t)(t149)))));
  t151 = ((int64_t)(v18));
  t152 = ((int64_t)(((int64_t)((uint64_t)(t150) + (uint64_t)(t151)))));
  t153 = ((int64_t)(((int64_t)UINT64_C(4))));
  t154 = ((int64_t)((uint64_t)(t152) + (uint64_t)(t153)));
  t155 = ((uint8_t *)t147.data) + gg_index(t154, t147.len);
  t156 = ((uint8_t)((*t155)));
  t157 = ((uint8_t)(((int64_t)UINT64_C(63))));
  (*t146) = ((uint8_t)(((t156) & (t157))));
  }
label19:;
  t122++; goto label18;
label20:;
  t158 = ((int64_t)(v7));
  t159 = ((int64_t)(((int64_t)UINT64_C(32))));
  t160 = ((int64_t)(((int64_t)((uint64_t)(t158) + (uint64_t)(t159)))));
  t161 = ((int64_t)(v18));
  t162 = ((int64_t)(((int64_t)UINT64_C(12))));
  t163 = ((int64_t)(((int64_t)((uint64_t)(t161) * (uint64_t)(t162)))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t160) + (uint64_t)(t163)))));
  t164 = v0;
  t165 = ((int64_t)(v21));
  t166 = ((int64_t)(((int64_t)UINT64_C(0))));
  t167 = ((int64_t)((uint64_t)(t165) + (uint64_t)(t166)));
  t168 = ((uint8_t *)t164.data) + gg_index(t167, t164.len);
  /* FG_DIRECT_SLICE_INDEX */
  t169 = ((int64_t)UINT64_C(0));
  t170 = ((uint8_t *)(v16).data) + gg_index(t169, ((int64_t)UINT64_C(8)));
  t171 = ((uint8_t)((*t170)));
  t172 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t173 = ((uint8_t)(((t171) & (t172))));
  /* FG_DIRECT_SLICE_INDEX */
  t174 = ((int64_t)UINT64_C(4));
  t175 = ((uint8_t *)(v16).data) + gg_index(t174, ((int64_t)UINT64_C(8)));
  t176 = ((uint8_t)((*t175)));
  t177 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t178 = ((uint8_t)(((t176) & (t177))));
  t179 = ((int64_t)(((int64_t)UINT64_C(2))));
  t180 = ((uint8_t)(((uint8_t)((uint64_t)(t178) << ((uint64_t)(t179) & 7)))));
  (*t168) = ((uint8_t)(((uint8_t)((uint64_t)(t173) + (uint64_t)(t180)))));
  t181 = v0;
  t182 = ((int64_t)(v21));
  t183 = ((int64_t)(((int64_t)UINT64_C(1))));
  t184 = ((int64_t)((uint64_t)(t182) + (uint64_t)(t183)));
  t185 = ((uint8_t *)t181.data) + gg_index(t184, t181.len);
  /* FG_DIRECT_SLICE_INDEX */
  t186 = ((int64_t)UINT64_C(1));
  t187 = ((uint8_t *)(v16).data) + gg_index(t186, ((int64_t)UINT64_C(8)));
  t188 = ((uint8_t)((*t187)));
  t189 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t190 = ((uint8_t)(((t188) & (t189))));
  /* FG_DIRECT_SLICE_INDEX */
  t191 = ((int64_t)UINT64_C(5));
  t192 = ((uint8_t *)(v16).data) + gg_index(t191, ((int64_t)UINT64_C(8)));
  t193 = ((uint8_t)((*t192)));
  t194 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t195 = ((uint8_t)(((t193) & (t194))));
  t196 = ((int64_t)(((int64_t)UINT64_C(2))));
  t197 = ((uint8_t)(((uint8_t)((uint64_t)(t195) << ((uint64_t)(t196) & 7)))));
  (*t185) = ((uint8_t)(((uint8_t)((uint64_t)(t190) + (uint64_t)(t197)))));
  t198 = v0;
  t199 = ((int64_t)(v21));
  t200 = ((int64_t)(((int64_t)UINT64_C(2))));
  t201 = ((int64_t)((uint64_t)(t199) + (uint64_t)(t200)));
  t202 = ((uint8_t *)t198.data) + gg_index(t201, t198.len);
  /* FG_DIRECT_SLICE_INDEX */
  t203 = ((int64_t)UINT64_C(2));
  t204 = ((uint8_t *)(v16).data) + gg_index(t203, ((int64_t)UINT64_C(8)));
  t205 = ((uint8_t)((*t204)));
  t206 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t207 = ((uint8_t)(((t205) & (t206))));
  /* FG_DIRECT_SLICE_INDEX */
  t208 = ((int64_t)UINT64_C(6));
  t209 = ((uint8_t *)(v16).data) + gg_index(t208, ((int64_t)UINT64_C(8)));
  t210 = ((uint8_t)((*t209)));
  t211 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t212 = ((uint8_t)(((t210) & (t211))));
  t213 = ((int64_t)(((int64_t)UINT64_C(2))));
  t214 = ((uint8_t)(((uint8_t)((uint64_t)(t212) << ((uint64_t)(t213) & 7)))));
  (*t202) = ((uint8_t)(((uint8_t)((uint64_t)(t207) + (uint64_t)(t214)))));
  t215 = v0;
  t216 = ((int64_t)(v21));
  t217 = ((int64_t)(((int64_t)UINT64_C(3))));
  t218 = ((int64_t)((uint64_t)(t216) + (uint64_t)(t217)));
  t219 = ((uint8_t *)t215.data) + gg_index(t218, t215.len);
  /* FG_DIRECT_SLICE_INDEX */
  t220 = ((int64_t)UINT64_C(3));
  t221 = ((uint8_t *)(v16).data) + gg_index(t220, ((int64_t)UINT64_C(8)));
  t222 = ((uint8_t)((*t221)));
  t223 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t224 = ((uint8_t)(((t222) & (t223))));
  /* FG_DIRECT_SLICE_INDEX */
  t225 = ((int64_t)UINT64_C(7));
  t226 = ((uint8_t *)(v16).data) + gg_index(t225, ((int64_t)UINT64_C(8)));
  t227 = ((uint8_t)((*t226)));
  t228 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t229 = ((uint8_t)(((t227) & (t228))));
  t230 = ((int64_t)(((int64_t)UINT64_C(2))));
  t231 = ((uint8_t)(((uint8_t)((uint64_t)(t229) << ((uint64_t)(t230) & 7)))));
  (*t219) = ((uint8_t)(((uint8_t)((uint64_t)(t224) + (uint64_t)(t231)))));
  t232 = v0;
  t233 = ((int64_t)(v21));
  t234 = ((int64_t)(((int64_t)UINT64_C(4))));
  t235 = ((int64_t)((uint64_t)(t233) + (uint64_t)(t234)));
  t236 = ((uint8_t *)t232.data) + gg_index(t235, t232.len);
  /* FG_DIRECT_SLICE_INDEX */
  t237 = ((int64_t)UINT64_C(0));
  t238 = ((uint8_t *)(v17).data) + gg_index(t237, ((int64_t)UINT64_C(8)));
  t239 = ((uint8_t)((*t238)));
  t240 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t241 = ((uint8_t)(((t239) & (t240))));
  /* FG_DIRECT_SLICE_INDEX */
  t242 = ((int64_t)UINT64_C(4));
  t243 = ((uint8_t *)(v17).data) + gg_index(t242, ((int64_t)UINT64_C(8)));
  t244 = ((uint8_t)((*t243)));
  t245 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t246 = ((uint8_t)(((t244) & (t245))));
  t247 = ((int64_t)(((int64_t)UINT64_C(2))));
  t248 = ((uint8_t)(((uint8_t)((uint64_t)(t246) << ((uint64_t)(t247) & 7)))));
  (*t236) = ((uint8_t)(((uint8_t)((uint64_t)(t241) + (uint64_t)(t248)))));
  t249 = v0;
  t250 = ((int64_t)(v21));
  t251 = ((int64_t)(((int64_t)UINT64_C(5))));
  t252 = ((int64_t)((uint64_t)(t250) + (uint64_t)(t251)));
  t253 = ((uint8_t *)t249.data) + gg_index(t252, t249.len);
  /* FG_DIRECT_SLICE_INDEX */
  t254 = ((int64_t)UINT64_C(1));
  t255 = ((uint8_t *)(v17).data) + gg_index(t254, ((int64_t)UINT64_C(8)));
  t256 = ((uint8_t)((*t255)));
  t257 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t258 = ((uint8_t)(((t256) & (t257))));
  /* FG_DIRECT_SLICE_INDEX */
  t259 = ((int64_t)UINT64_C(5));
  t260 = ((uint8_t *)(v17).data) + gg_index(t259, ((int64_t)UINT64_C(8)));
  t261 = ((uint8_t)((*t260)));
  t262 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t263 = ((uint8_t)(((t261) & (t262))));
  t264 = ((int64_t)(((int64_t)UINT64_C(2))));
  t265 = ((uint8_t)(((uint8_t)((uint64_t)(t263) << ((uint64_t)(t264) & 7)))));
  (*t253) = ((uint8_t)(((uint8_t)((uint64_t)(t258) + (uint64_t)(t265)))));
  t266 = v0;
  t267 = ((int64_t)(v21));
  t268 = ((int64_t)(((int64_t)UINT64_C(6))));
  t269 = ((int64_t)((uint64_t)(t267) + (uint64_t)(t268)));
  t270 = ((uint8_t *)t266.data) + gg_index(t269, t266.len);
  /* FG_DIRECT_SLICE_INDEX */
  t271 = ((int64_t)UINT64_C(2));
  t272 = ((uint8_t *)(v17).data) + gg_index(t271, ((int64_t)UINT64_C(8)));
  t273 = ((uint8_t)((*t272)));
  t274 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t275 = ((uint8_t)(((t273) & (t274))));
  /* FG_DIRECT_SLICE_INDEX */
  t276 = ((int64_t)UINT64_C(6));
  t277 = ((uint8_t *)(v17).data) + gg_index(t276, ((int64_t)UINT64_C(8)));
  t278 = ((uint8_t)((*t277)));
  t279 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t280 = ((uint8_t)(((t278) & (t279))));
  t281 = ((int64_t)(((int64_t)UINT64_C(2))));
  t282 = ((uint8_t)(((uint8_t)((uint64_t)(t280) << ((uint64_t)(t281) & 7)))));
  (*t270) = ((uint8_t)(((uint8_t)((uint64_t)(t275) + (uint64_t)(t282)))));
  t283 = v0;
  t284 = ((int64_t)(v21));
  t285 = ((int64_t)(((int64_t)UINT64_C(7))));
  t286 = ((int64_t)((uint64_t)(t284) + (uint64_t)(t285)));
  t287 = ((uint8_t *)t283.data) + gg_index(t286, t283.len);
  /* FG_DIRECT_SLICE_INDEX */
  t288 = ((int64_t)UINT64_C(3));
  t289 = ((uint8_t *)(v17).data) + gg_index(t288, ((int64_t)UINT64_C(8)));
  t290 = ((uint8_t)((*t289)));
  t291 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t292 = ((uint8_t)(((t290) & (t291))));
  /* FG_DIRECT_SLICE_INDEX */
  t293 = ((int64_t)UINT64_C(7));
  t294 = ((uint8_t *)(v17).data) + gg_index(t293, ((int64_t)UINT64_C(8)));
  t295 = ((uint8_t)((*t294)));
  t296 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t297 = ((uint8_t)(((t295) & (t296))));
  t298 = ((int64_t)(((int64_t)UINT64_C(2))));
  t299 = ((uint8_t)(((uint8_t)((uint64_t)(t297) << ((uint64_t)(t298) & 7)))));
  (*t287) = ((uint8_t)(((uint8_t)((uint64_t)(t292) + (uint64_t)(t299)))));
  t300 = v0;
  t301 = ((int64_t)(v21));
  t302 = ((int64_t)(((int64_t)UINT64_C(8))));
  t303 = ((int64_t)((uint64_t)(t301) + (uint64_t)(t302)));
  t304 = ((uint8_t *)t300.data) + gg_index(t303, t300.len);
  /* FG_DIRECT_SLICE_INDEX */
  t305 = ((int64_t)UINT64_C(4));
  t306 = ((uint8_t *)(v16).data) + gg_index(t305, ((int64_t)UINT64_C(8)));
  t307 = ((uint8_t)((*t306)));
  t308 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t309 = ((uint8_t)(((t307) & (t308))));
  /* FG_DIRECT_SLICE_INDEX */
  t310 = ((int64_t)UINT64_C(4));
  t311 = ((uint8_t *)(v17).data) + gg_index(t310, ((int64_t)UINT64_C(8)));
  t312 = ((uint8_t)((*t311)));
  t313 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t314 = ((uint8_t)(((t312) & (t313))));
  t315 = ((int64_t)(((int64_t)UINT64_C(4))));
  t316 = ((uint8_t)(((uint8_t)((uint64_t)(t314) << ((uint64_t)(t315) & 7)))));
  (*t304) = ((uint8_t)(((uint8_t)((uint64_t)(t309) + (uint64_t)(t316)))));
  t317 = v0;
  t318 = ((int64_t)(v21));
  t319 = ((int64_t)(((int64_t)UINT64_C(9))));
  t320 = ((int64_t)((uint64_t)(t318) + (uint64_t)(t319)));
  t321 = ((uint8_t *)t317.data) + gg_index(t320, t317.len);
  /* FG_DIRECT_SLICE_INDEX */
  t322 = ((int64_t)UINT64_C(5));
  t323 = ((uint8_t *)(v16).data) + gg_index(t322, ((int64_t)UINT64_C(8)));
  t324 = ((uint8_t)((*t323)));
  t325 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t326 = ((uint8_t)(((t324) & (t325))));
  /* FG_DIRECT_SLICE_INDEX */
  t327 = ((int64_t)UINT64_C(5));
  t328 = ((uint8_t *)(v17).data) + gg_index(t327, ((int64_t)UINT64_C(8)));
  t329 = ((uint8_t)((*t328)));
  t330 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t331 = ((uint8_t)(((t329) & (t330))));
  t332 = ((int64_t)(((int64_t)UINT64_C(4))));
  t333 = ((uint8_t)(((uint8_t)((uint64_t)(t331) << ((uint64_t)(t332) & 7)))));
  (*t321) = ((uint8_t)(((uint8_t)((uint64_t)(t326) + (uint64_t)(t333)))));
  t334 = v0;
  t335 = ((int64_t)(v21));
  t336 = ((int64_t)(((int64_t)UINT64_C(10))));
  t337 = ((int64_t)((uint64_t)(t335) + (uint64_t)(t336)));
  t338 = ((uint8_t *)t334.data) + gg_index(t337, t334.len);
  /* FG_DIRECT_SLICE_INDEX */
  t339 = ((int64_t)UINT64_C(6));
  t340 = ((uint8_t *)(v16).data) + gg_index(t339, ((int64_t)UINT64_C(8)));
  t341 = ((uint8_t)((*t340)));
  t342 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t343 = ((uint8_t)(((t341) & (t342))));
  /* FG_DIRECT_SLICE_INDEX */
  t344 = ((int64_t)UINT64_C(6));
  t345 = ((uint8_t *)(v17).data) + gg_index(t344, ((int64_t)UINT64_C(8)));
  t346 = ((uint8_t)((*t345)));
  t347 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t348 = ((uint8_t)(((t346) & (t347))));
  t349 = ((int64_t)(((int64_t)UINT64_C(4))));
  t350 = ((uint8_t)(((uint8_t)((uint64_t)(t348) << ((uint64_t)(t349) & 7)))));
  (*t338) = ((uint8_t)(((uint8_t)((uint64_t)(t343) + (uint64_t)(t350)))));
  t351 = v0;
  t352 = ((int64_t)(v21));
  t353 = ((int64_t)(((int64_t)UINT64_C(11))));
  t354 = ((int64_t)((uint64_t)(t352) + (uint64_t)(t353)));
  t355 = ((uint8_t *)t351.data) + gg_index(t354, t351.len);
  /* FG_DIRECT_SLICE_INDEX */
  t356 = ((int64_t)UINT64_C(7));
  t357 = ((uint8_t *)(v16).data) + gg_index(t356, ((int64_t)UINT64_C(8)));
  t358 = ((uint8_t)((*t357)));
  t359 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t360 = ((uint8_t)(((t358) & (t359))));
  /* FG_DIRECT_SLICE_INDEX */
  t361 = ((int64_t)UINT64_C(7));
  t362 = ((uint8_t *)(v17).data) + gg_index(t361, ((int64_t)UINT64_C(8)));
  t363 = ((uint8_t)((*t362)));
  t364 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t365 = ((uint8_t)(((t363) & (t364))));
  t366 = ((int64_t)(((int64_t)UINT64_C(4))));
  t367 = ((uint8_t)(((uint8_t)((uint64_t)(t365) << ((uint64_t)(t366) & 7)))));
  (*t355) = ((uint8_t)(((uint8_t)((uint64_t)(t360) + (uint64_t)(t367)))));
  }
label16:;
  t120++; goto label15;
label17:;
  t369 = ((int64_t)UINT64_C(4));
  t368 = 0;
label21:;
  if (t368 >= t369) goto label23;
  v22 = t368;
  {
  t371 = ((int64_t)UINT64_C(8));
  t370 = 0;
label24:;
  if (t370 >= t371) goto label26;
  v23 = t370;
  {
  t372 = ((int64_t)(v5));
  t373 = ((int64_t)(((int64_t)UINT64_C(8))));
  t374 = ((int64_t)(((int64_t)((uint64_t)(t372) * (uint64_t)(t373)))));
  t375 = ((int64_t)(v23));
  t376 = ((int64_t)(((int64_t)((uint64_t)(t374) + (uint64_t)(t375)))));
  t377 = ((int64_t)(v3));
  t378 = ((int64_t)(((int64_t)((uint64_t)(t376) * (uint64_t)(t377)))));
  t379 = ((int64_t)(v6));
  t380 = ((int64_t)(((int64_t)((uint64_t)(t378) + (uint64_t)(t379)))));
  t381 = ((int64_t)(((int64_t)UINT64_C(144))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t380) * (uint64_t)(t381)))));
  /* FG_DIRECT_SLICE_INDEX */
  t382 = v23;
  t383 = ((uint8_t *)(v16).data) + gg_index(t382, ((int64_t)UINT64_C(8)));
  t384 = v1;
  t385 = ((int64_t)(v24));
  t386 = ((int64_t)(((int64_t)UINT64_C(4))));
  t387 = ((int64_t)(((int64_t)((uint64_t)(t385) + (uint64_t)(t386)))));
  t388 = ((int64_t)(v22));
  t389 = ((int64_t)((uint64_t)(t387) + (uint64_t)(t388)));
  t390 = ((uint8_t *)t384.data) + gg_index(t389, t384.len);
  t391 = ((uint8_t)((*t390)));
  t392 = ((uint8_t)(((int64_t)UINT64_C(192))));
  t393 = ((uint8_t)(((t391) & (t392))));
  t394 = ((int64_t)(((int64_t)UINT64_C(2))));
  t395 = ((uint8_t)(((t393) >> ((uint64_t)(t394) & 7))));
  t396 = v1;
  t397 = ((int64_t)(v24));
  t398 = ((int64_t)(((int64_t)UINT64_C(4))));
  t399 = ((int64_t)(((int64_t)((uint64_t)(t397) + (uint64_t)(t398)))));
  t400 = ((int64_t)(v22));
  t401 = ((int64_t)(((int64_t)((uint64_t)(t399) + (uint64_t)(t400)))));
  t402 = ((int64_t)(((int64_t)UINT64_C(8))));
  t403 = ((int64_t)((uint64_t)(t401) + (uint64_t)(t402)));
  t404 = ((uint8_t *)t396.data) + gg_index(t403, t396.len);
  t405 = ((uint8_t)((*t404)));
  t406 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t407 = ((uint8_t)(((t405) & (t406))));
  (*t383) = ((uint8_t)(((t395) | (t407))));
  /* FG_DIRECT_SLICE_INDEX */
  t408 = v23;
  t409 = ((uint8_t *)(v17).data) + gg_index(t408, ((int64_t)UINT64_C(8)));
  t410 = v1;
  t411 = ((int64_t)(v24));
  t412 = ((int64_t)(((int64_t)UINT64_C(4))));
  t413 = ((int64_t)(((int64_t)((uint64_t)(t411) + (uint64_t)(t412)))));
  t414 = ((int64_t)(v22));
  t415 = ((int64_t)(((int64_t)((uint64_t)(t413) + (uint64_t)(t414)))));
  t416 = ((int64_t)(((int64_t)UINT64_C(4))));
  t417 = ((int64_t)((uint64_t)(t415) + (uint64_t)(t416)));
  t418 = ((uint8_t *)t410.data) + gg_index(t417, t410.len);
  t419 = ((uint8_t)((*t418)));
  t420 = ((uint8_t)(((int64_t)UINT64_C(192))));
  t421 = ((uint8_t)(((t419) & (t420))));
  t422 = ((int64_t)(((int64_t)UINT64_C(2))));
  t423 = ((uint8_t)(((t421) >> ((uint64_t)(t422) & 7))));
  t424 = v1;
  t425 = ((int64_t)(v24));
  t426 = ((int64_t)(((int64_t)UINT64_C(4))));
  t427 = ((int64_t)(((int64_t)((uint64_t)(t425) + (uint64_t)(t426)))));
  t428 = ((int64_t)(v22));
  t429 = ((int64_t)(((int64_t)((uint64_t)(t427) + (uint64_t)(t428)))));
  t430 = ((int64_t)(((int64_t)UINT64_C(8))));
  t431 = ((int64_t)((uint64_t)(t429) + (uint64_t)(t430)));
  t432 = ((uint8_t *)t424.data) + gg_index(t431, t424.len);
  t433 = ((uint8_t)((*t432)));
  t434 = ((uint8_t)(((int64_t)UINT64_C(240))));
  t435 = ((uint8_t)(((t433) & (t434))));
  t436 = ((int64_t)(((int64_t)UINT64_C(4))));
  t437 = ((uint8_t)(((t435) >> ((uint64_t)(t436) & 7))));
  (*t409) = ((uint8_t)(((t423) | (t437))));
  }
label25:;
  t370++; goto label24;
label26:;
  t438 = ((int64_t)(v7));
  t439 = ((int64_t)(((int64_t)UINT64_C(32))));
  t440 = ((int64_t)(((int64_t)((uint64_t)(t438) + (uint64_t)(t439)))));
  t441 = ((int64_t)(v22));
  t442 = ((int64_t)(((int64_t)UINT64_C(12))));
  t443 = ((int64_t)(((int64_t)((uint64_t)(t441) * (uint64_t)(t442)))));
  t444 = ((int64_t)(((int64_t)((uint64_t)(t440) + (uint64_t)(t443)))));
  t445 = ((int64_t)(((int64_t)UINT64_C(48))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t444) + (uint64_t)(t445)))));
  t446 = v0;
  t447 = ((int64_t)(v25));
  t448 = ((int64_t)(((int64_t)UINT64_C(0))));
  t449 = ((int64_t)((uint64_t)(t447) + (uint64_t)(t448)));
  t450 = ((uint8_t *)t446.data) + gg_index(t449, t446.len);
  /* FG_DIRECT_SLICE_INDEX */
  t451 = ((int64_t)UINT64_C(0));
  t452 = ((uint8_t *)(v16).data) + gg_index(t451, ((int64_t)UINT64_C(8)));
  t453 = ((uint8_t)((*t452)));
  t454 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t455 = ((uint8_t)(((t453) & (t454))));
  /* FG_DIRECT_SLICE_INDEX */
  t456 = ((int64_t)UINT64_C(4));
  t457 = ((uint8_t *)(v16).data) + gg_index(t456, ((int64_t)UINT64_C(8)));
  t458 = ((uint8_t)((*t457)));
  t459 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t460 = ((uint8_t)(((t458) & (t459))));
  t461 = ((int64_t)(((int64_t)UINT64_C(2))));
  t462 = ((uint8_t)(((uint8_t)((uint64_t)(t460) << ((uint64_t)(t461) & 7)))));
  (*t450) = ((uint8_t)(((uint8_t)((uint64_t)(t455) + (uint64_t)(t462)))));
  t463 = v0;
  t464 = ((int64_t)(v25));
  t465 = ((int64_t)(((int64_t)UINT64_C(1))));
  t466 = ((int64_t)((uint64_t)(t464) + (uint64_t)(t465)));
  t467 = ((uint8_t *)t463.data) + gg_index(t466, t463.len);
  /* FG_DIRECT_SLICE_INDEX */
  t468 = ((int64_t)UINT64_C(1));
  t469 = ((uint8_t *)(v16).data) + gg_index(t468, ((int64_t)UINT64_C(8)));
  t470 = ((uint8_t)((*t469)));
  t471 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t472 = ((uint8_t)(((t470) & (t471))));
  /* FG_DIRECT_SLICE_INDEX */
  t473 = ((int64_t)UINT64_C(5));
  t474 = ((uint8_t *)(v16).data) + gg_index(t473, ((int64_t)UINT64_C(8)));
  t475 = ((uint8_t)((*t474)));
  t476 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t477 = ((uint8_t)(((t475) & (t476))));
  t478 = ((int64_t)(((int64_t)UINT64_C(2))));
  t479 = ((uint8_t)(((uint8_t)((uint64_t)(t477) << ((uint64_t)(t478) & 7)))));
  (*t467) = ((uint8_t)(((uint8_t)((uint64_t)(t472) + (uint64_t)(t479)))));
  t480 = v0;
  t481 = ((int64_t)(v25));
  t482 = ((int64_t)(((int64_t)UINT64_C(2))));
  t483 = ((int64_t)((uint64_t)(t481) + (uint64_t)(t482)));
  t484 = ((uint8_t *)t480.data) + gg_index(t483, t480.len);
  /* FG_DIRECT_SLICE_INDEX */
  t485 = ((int64_t)UINT64_C(2));
  t486 = ((uint8_t *)(v16).data) + gg_index(t485, ((int64_t)UINT64_C(8)));
  t487 = ((uint8_t)((*t486)));
  t488 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t489 = ((uint8_t)(((t487) & (t488))));
  /* FG_DIRECT_SLICE_INDEX */
  t490 = ((int64_t)UINT64_C(6));
  t491 = ((uint8_t *)(v16).data) + gg_index(t490, ((int64_t)UINT64_C(8)));
  t492 = ((uint8_t)((*t491)));
  t493 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t494 = ((uint8_t)(((t492) & (t493))));
  t495 = ((int64_t)(((int64_t)UINT64_C(2))));
  t496 = ((uint8_t)(((uint8_t)((uint64_t)(t494) << ((uint64_t)(t495) & 7)))));
  (*t484) = ((uint8_t)(((uint8_t)((uint64_t)(t489) + (uint64_t)(t496)))));
  t497 = v0;
  t498 = ((int64_t)(v25));
  t499 = ((int64_t)(((int64_t)UINT64_C(3))));
  t500 = ((int64_t)((uint64_t)(t498) + (uint64_t)(t499)));
  t501 = ((uint8_t *)t497.data) + gg_index(t500, t497.len);
  /* FG_DIRECT_SLICE_INDEX */
  t502 = ((int64_t)UINT64_C(3));
  t503 = ((uint8_t *)(v16).data) + gg_index(t502, ((int64_t)UINT64_C(8)));
  t504 = ((uint8_t)((*t503)));
  t505 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t506 = ((uint8_t)(((t504) & (t505))));
  /* FG_DIRECT_SLICE_INDEX */
  t507 = ((int64_t)UINT64_C(7));
  t508 = ((uint8_t *)(v16).data) + gg_index(t507, ((int64_t)UINT64_C(8)));
  t509 = ((uint8_t)((*t508)));
  t510 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t511 = ((uint8_t)(((t509) & (t510))));
  t512 = ((int64_t)(((int64_t)UINT64_C(2))));
  t513 = ((uint8_t)(((uint8_t)((uint64_t)(t511) << ((uint64_t)(t512) & 7)))));
  (*t501) = ((uint8_t)(((uint8_t)((uint64_t)(t506) + (uint64_t)(t513)))));
  t514 = v0;
  t515 = ((int64_t)(v25));
  t516 = ((int64_t)(((int64_t)UINT64_C(4))));
  t517 = ((int64_t)((uint64_t)(t515) + (uint64_t)(t516)));
  t518 = ((uint8_t *)t514.data) + gg_index(t517, t514.len);
  /* FG_DIRECT_SLICE_INDEX */
  t519 = ((int64_t)UINT64_C(0));
  t520 = ((uint8_t *)(v17).data) + gg_index(t519, ((int64_t)UINT64_C(8)));
  t521 = ((uint8_t)((*t520)));
  t522 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t523 = ((uint8_t)(((t521) & (t522))));
  /* FG_DIRECT_SLICE_INDEX */
  t524 = ((int64_t)UINT64_C(4));
  t525 = ((uint8_t *)(v17).data) + gg_index(t524, ((int64_t)UINT64_C(8)));
  t526 = ((uint8_t)((*t525)));
  t527 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t528 = ((uint8_t)(((t526) & (t527))));
  t529 = ((int64_t)(((int64_t)UINT64_C(2))));
  t530 = ((uint8_t)(((uint8_t)((uint64_t)(t528) << ((uint64_t)(t529) & 7)))));
  (*t518) = ((uint8_t)(((uint8_t)((uint64_t)(t523) + (uint64_t)(t530)))));
  t531 = v0;
  t532 = ((int64_t)(v25));
  t533 = ((int64_t)(((int64_t)UINT64_C(5))));
  t534 = ((int64_t)((uint64_t)(t532) + (uint64_t)(t533)));
  t535 = ((uint8_t *)t531.data) + gg_index(t534, t531.len);
  /* FG_DIRECT_SLICE_INDEX */
  t536 = ((int64_t)UINT64_C(1));
  t537 = ((uint8_t *)(v17).data) + gg_index(t536, ((int64_t)UINT64_C(8)));
  t538 = ((uint8_t)((*t537)));
  t539 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t540 = ((uint8_t)(((t538) & (t539))));
  /* FG_DIRECT_SLICE_INDEX */
  t541 = ((int64_t)UINT64_C(5));
  t542 = ((uint8_t *)(v17).data) + gg_index(t541, ((int64_t)UINT64_C(8)));
  t543 = ((uint8_t)((*t542)));
  t544 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t545 = ((uint8_t)(((t543) & (t544))));
  t546 = ((int64_t)(((int64_t)UINT64_C(2))));
  t547 = ((uint8_t)(((uint8_t)((uint64_t)(t545) << ((uint64_t)(t546) & 7)))));
  (*t535) = ((uint8_t)(((uint8_t)((uint64_t)(t540) + (uint64_t)(t547)))));
  t548 = v0;
  t549 = ((int64_t)(v25));
  t550 = ((int64_t)(((int64_t)UINT64_C(6))));
  t551 = ((int64_t)((uint64_t)(t549) + (uint64_t)(t550)));
  t552 = ((uint8_t *)t548.data) + gg_index(t551, t548.len);
  /* FG_DIRECT_SLICE_INDEX */
  t553 = ((int64_t)UINT64_C(2));
  t554 = ((uint8_t *)(v17).data) + gg_index(t553, ((int64_t)UINT64_C(8)));
  t555 = ((uint8_t)((*t554)));
  t556 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t557 = ((uint8_t)(((t555) & (t556))));
  /* FG_DIRECT_SLICE_INDEX */
  t558 = ((int64_t)UINT64_C(6));
  t559 = ((uint8_t *)(v17).data) + gg_index(t558, ((int64_t)UINT64_C(8)));
  t560 = ((uint8_t)((*t559)));
  t561 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t562 = ((uint8_t)(((t560) & (t561))));
  t563 = ((int64_t)(((int64_t)UINT64_C(2))));
  t564 = ((uint8_t)(((uint8_t)((uint64_t)(t562) << ((uint64_t)(t563) & 7)))));
  (*t552) = ((uint8_t)(((uint8_t)((uint64_t)(t557) + (uint64_t)(t564)))));
  t565 = v0;
  t566 = ((int64_t)(v25));
  t567 = ((int64_t)(((int64_t)UINT64_C(7))));
  t568 = ((int64_t)((uint64_t)(t566) + (uint64_t)(t567)));
  t569 = ((uint8_t *)t565.data) + gg_index(t568, t565.len);
  /* FG_DIRECT_SLICE_INDEX */
  t570 = ((int64_t)UINT64_C(3));
  t571 = ((uint8_t *)(v17).data) + gg_index(t570, ((int64_t)UINT64_C(8)));
  t572 = ((uint8_t)((*t571)));
  t573 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t574 = ((uint8_t)(((t572) & (t573))));
  /* FG_DIRECT_SLICE_INDEX */
  t575 = ((int64_t)UINT64_C(7));
  t576 = ((uint8_t *)(v17).data) + gg_index(t575, ((int64_t)UINT64_C(8)));
  t577 = ((uint8_t)((*t576)));
  t578 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t579 = ((uint8_t)(((t577) & (t578))));
  t580 = ((int64_t)(((int64_t)UINT64_C(2))));
  t581 = ((uint8_t)(((uint8_t)((uint64_t)(t579) << ((uint64_t)(t580) & 7)))));
  (*t569) = ((uint8_t)(((uint8_t)((uint64_t)(t574) + (uint64_t)(t581)))));
  t582 = v0;
  t583 = ((int64_t)(v25));
  t584 = ((int64_t)(((int64_t)UINT64_C(8))));
  t585 = ((int64_t)((uint64_t)(t583) + (uint64_t)(t584)));
  t586 = ((uint8_t *)t582.data) + gg_index(t585, t582.len);
  /* FG_DIRECT_SLICE_INDEX */
  t587 = ((int64_t)UINT64_C(4));
  t588 = ((uint8_t *)(v16).data) + gg_index(t587, ((int64_t)UINT64_C(8)));
  t589 = ((uint8_t)((*t588)));
  t590 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t591 = ((uint8_t)(((t589) & (t590))));
  /* FG_DIRECT_SLICE_INDEX */
  t592 = ((int64_t)UINT64_C(4));
  t593 = ((uint8_t *)(v17).data) + gg_index(t592, ((int64_t)UINT64_C(8)));
  t594 = ((uint8_t)((*t593)));
  t595 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t596 = ((uint8_t)(((t594) & (t595))));
  t597 = ((int64_t)(((int64_t)UINT64_C(4))));
  t598 = ((uint8_t)(((uint8_t)((uint64_t)(t596) << ((uint64_t)(t597) & 7)))));
  (*t586) = ((uint8_t)(((uint8_t)((uint64_t)(t591) + (uint64_t)(t598)))));
  t599 = v0;
  t600 = ((int64_t)(v25));
  t601 = ((int64_t)(((int64_t)UINT64_C(9))));
  t602 = ((int64_t)((uint64_t)(t600) + (uint64_t)(t601)));
  t603 = ((uint8_t *)t599.data) + gg_index(t602, t599.len);
  /* FG_DIRECT_SLICE_INDEX */
  t604 = ((int64_t)UINT64_C(5));
  t605 = ((uint8_t *)(v16).data) + gg_index(t604, ((int64_t)UINT64_C(8)));
  t606 = ((uint8_t)((*t605)));
  t607 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t608 = ((uint8_t)(((t606) & (t607))));
  /* FG_DIRECT_SLICE_INDEX */
  t609 = ((int64_t)UINT64_C(5));
  t610 = ((uint8_t *)(v17).data) + gg_index(t609, ((int64_t)UINT64_C(8)));
  t611 = ((uint8_t)((*t610)));
  t612 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t613 = ((uint8_t)(((t611) & (t612))));
  t614 = ((int64_t)(((int64_t)UINT64_C(4))));
  t615 = ((uint8_t)(((uint8_t)((uint64_t)(t613) << ((uint64_t)(t614) & 7)))));
  (*t603) = ((uint8_t)(((uint8_t)((uint64_t)(t608) + (uint64_t)(t615)))));
  t616 = v0;
  t617 = ((int64_t)(v25));
  t618 = ((int64_t)(((int64_t)UINT64_C(10))));
  t619 = ((int64_t)((uint64_t)(t617) + (uint64_t)(t618)));
  t620 = ((uint8_t *)t616.data) + gg_index(t619, t616.len);
  /* FG_DIRECT_SLICE_INDEX */
  t621 = ((int64_t)UINT64_C(6));
  t622 = ((uint8_t *)(v16).data) + gg_index(t621, ((int64_t)UINT64_C(8)));
  t623 = ((uint8_t)((*t622)));
  t624 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t625 = ((uint8_t)(((t623) & (t624))));
  /* FG_DIRECT_SLICE_INDEX */
  t626 = ((int64_t)UINT64_C(6));
  t627 = ((uint8_t *)(v17).data) + gg_index(t626, ((int64_t)UINT64_C(8)));
  t628 = ((uint8_t)((*t627)));
  t629 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t630 = ((uint8_t)(((t628) & (t629))));
  t631 = ((int64_t)(((int64_t)UINT64_C(4))));
  t632 = ((uint8_t)(((uint8_t)((uint64_t)(t630) << ((uint64_t)(t631) & 7)))));
  (*t620) = ((uint8_t)(((uint8_t)((uint64_t)(t625) + (uint64_t)(t632)))));
  t633 = v0;
  t634 = ((int64_t)(v25));
  t635 = ((int64_t)(((int64_t)UINT64_C(11))));
  t636 = ((int64_t)((uint64_t)(t634) + (uint64_t)(t635)));
  t637 = ((uint8_t *)t633.data) + gg_index(t636, t633.len);
  /* FG_DIRECT_SLICE_INDEX */
  t638 = ((int64_t)UINT64_C(7));
  t639 = ((uint8_t *)(v16).data) + gg_index(t638, ((int64_t)UINT64_C(8)));
  t640 = ((uint8_t)((*t639)));
  t641 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t642 = ((uint8_t)(((t640) & (t641))));
  /* FG_DIRECT_SLICE_INDEX */
  t643 = ((int64_t)UINT64_C(7));
  t644 = ((uint8_t *)(v17).data) + gg_index(t643, ((int64_t)UINT64_C(8)));
  t645 = ((uint8_t)((*t644)));
  t646 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t647 = ((uint8_t)(((t645) & (t646))));
  t648 = ((int64_t)(((int64_t)UINT64_C(4))));
  t649 = ((uint8_t)(((uint8_t)((uint64_t)(t647) << ((uint64_t)(t648) & 7)))));
  (*t637) = ((uint8_t)(((uint8_t)((uint64_t)(t642) + (uint64_t)(t649)))));
  }
label22:;
  t368++; goto label21;
label23:;
  }
label4:;
  t32++; goto label3;
label5:;
  }
label1:;
  t28++; goto label0;
label2:;
  t650 = ((int64_t)(((int64_t)UINT64_C(0))));
  t651 = ((int64_t)(v2));
  t652 = ((int64_t)(((int64_t)UINT64_C(8))));
  t653 = ((int64_t)(((int64_t)gg_div(t651, t652))));
  t654 = ((int64_t)(v3));
  t655 = ((int64_t)(((int64_t)((uint64_t)(t653) * (uint64_t)(t654)))));
  fg006_repack_witness(t650, t655);
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  float t6;
  float t7;
  int64_t t8;
  gg_slice t9;
  gg_slice t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  gg_slice t14;
  gg_slice t15;
  int64_t t16;
  int64_t t17;
  gg_slice t18;
  int64_t t19;
  gg_slice t20;
  gg_slice t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  gg_slice t25;
  int64_t t26;
  gg_slice t27;
  gg_slice t28;
  int64_t t29;
  int64_t t30;
  gg_header t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  gg_type_2 v0;
  gg_type_2 v1;
  gg_type_4 v2;
  {
  memset(&t0, 0, sizeof(t0));
  t1 = ((uint8_t)(t0));
  for (t2=0; t2<1; t2++) (v0).data[t2] = t1;
  memset(&t3, 0, sizeof(t3));
  t4 = ((uint8_t)(t3));
  for (t5=0; t5<1; t5++) (v1).data[t5] = t4;
  memset(&t6, 0, sizeof(t6));
  t7 = ((float)(t6));
  for (t8=0; t8<1; t8++) (v2).data[t8] = t7;
  t9 = gg_view((v0).data, 1);
  t10 = gg_view((v1).data, 1);
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_function_1(t9, t10, t11, t12, t13, base + 0);
  t14 = gg_view((v2).data, 1);
  t15 = gg_view((v0).data, 1);
  t16 = ((int64_t)(((int64_t)UINT64_C(256))));
  gg_function_5(t14, t15, t16, base + 0);
  t17 = ((int64_t)(((int64_t)UINT64_C(256))));
  t18 = gg_view((v2).data, 1);
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = gg_view((v1).data, 1);
  t21 = gg_view((v1).data, 1);
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  t23 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_function_9(t17, t18, t19, t20, t21, t22, t23, base + 0);
  t24 = ((int64_t)(((int64_t)UINT64_C(256))));
  t25 = gg_view((v2).data, 1);
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = gg_view((v1).data, 1);
  t28 = gg_view((v1).data, 1);
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  t30 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_function_16(t24, t25, t26, t27, t28, t29, t30, base + 0);
  t31.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t31.len = 0;
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = gg_function_17(t32, t33, base + 1);
  gg_integer(gg_ref(&t31, gg_get_stack(base + 0)), t34);
  gg_append(gg_ref(&t31, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t31).base, (t31).len));
  }
gg_done:;
  for (t35 = 0; t35 < 1; t35++) gg_stacks[base + t35].top = 0;
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
