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
typedef struct { float data[16]; } gg_type_7;
#pragma pack(pop)
typedef struct gg_type_10 gg_type_10;
#pragma pack(push, 1)
struct gg_type_10 {
  uint32_t f0;
  uint32_t f1;
  uint32_t f2;
  uint32_t f3;
};
#pragma pack(pop)
typedef struct gg_type_11 gg_type_11;
#pragma pack(push, 1)
struct gg_type_11 {
  float f4;
  float f5;
  float f6;
  float f7;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[2]; } gg_type_12;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint32_t data[2]; } gg_type_13;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint32_t data[8]; } gg_type_14;
#pragma pack(pop)
typedef struct gg_type_15 gg_type_15;
#pragma pack(push, 1)
struct gg_type_15 {
  uint32_t f8;
  uint32_t f9;
};
#pragma pack(pop)
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
static uint8_t gg_eq_5(int32_t a, int32_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, int32_t value);
static uint8_t gg_eq_6(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint8_t value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_8(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, uint32_t value);
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, gg_type_10 value);
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, gg_type_11 value);
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_type_12 value);
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_type_13 value);
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, gg_type_14 value);
static uint8_t gg_eq_15(gg_type_15 a, gg_type_15 b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, gg_type_15 value);
static uint8_t gg_eq_16(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, uint8_t value);
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
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(float a, float b) {
return a == b;
}
static void gg_render_3(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(double a, double b) {
return a == b;
}
static void gg_render_4(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_3(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_8(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
if(!gg_eq_8(a.f0,b.f0))return 0;
if(!gg_eq_8(a.f1,b.f1))return 0;
if(!gg_eq_8(a.f2,b.f2))return 0;
if(!gg_eq_8(a.f3,b.f3))return 0;
return 1;
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
gg_append(out,"\125\064\040\173\040",5,1);
gg_render_8(out, value.f0);
gg_append(out,", ",2,1);
gg_render_8(out, value.f1);
gg_append(out,", ",2,1);
gg_render_8(out, value.f2);
gg_append(out,", ",2,1);
gg_render_8(out, value.f3);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_type_11*)a.data)[i],((gg_type_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b) {
if(!gg_eq_3(a.f4,b.f4))return 0;
if(!gg_eq_3(a.f5,b.f5))return 0;
if(!gg_eq_3(a.f6,b.f6))return 0;
if(!gg_eq_3(a.f7,b.f7))return 0;
return 1;
}
static void gg_render_11(gg_reference out, gg_type_11 value) {
gg_append(out,"\106\064\040\173\040",5,1);
gg_render_3(out, value.f4);
gg_append(out,", ",2,1);
gg_render_3(out, value.f5);
gg_append(out,", ",2,1);
gg_render_3(out, value.f6);
gg_append(out,", ",2,1);
gg_render_3(out, value.f7);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return gg_equal_elements_3(gg_view((a).data, 2),gg_view((b).data, 2));
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
int64_t i; gg_slice view=gg_view((value).data, 2);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
return gg_equal_elements_8(gg_view((a).data, 2),gg_view((b).data, 2));
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
int64_t i; gg_slice view=gg_view((value).data, 2);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((uint32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_type_14*)a.data)[i],((gg_type_14*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b) {
return gg_equal_elements_8(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_14(gg_reference out, gg_type_14 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((uint32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((gg_type_15*)a.data)[i],((gg_type_15*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(gg_type_15 a, gg_type_15 b) {
if(!gg_eq_8(a.f8,b.f8))return 0;
if(!gg_eq_8(a.f9,b.f9))return 0;
return 1;
}
static void gg_render_15(gg_reference out, gg_type_15 value) {
gg_append(out,"\125\062\040\173\040",5,1);
gg_render_8(out, value.f8);
gg_append(out,", ",2,1);
gg_render_8(out, value.f9);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_16(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, float p6, int32_t p7, int32_t p8, int64_t p9, int32_t p10, uint64_t p11, uint64_t p12, uint64_t p13, uint64_t p14, uint64_t p15, int32_t p16, int32_t p17, int32_t p18, uint8_t p19, int64_t base);
static uint64_t gg_function_6(uint64_t p0, int32_t p1, int32_t p2, int64_t base);
static int32_t gg_function_7(int32_t p0, int32_t p1, int64_t base);
static int32_t gg_function_11(int32_t p0, int32_t p1, int64_t base);
static int32_t gg_function_12(int32_t p0, int32_t p1, int64_t base);
static gg_type_10 gg_function_23(uint64_t p0, int64_t base);
static gg_type_10 gg_function_23(uint64_t p0, int64_t base) {
  gg_type_10 t0;
  uint64_t t1;
  gg_type_10 t2;
  uint64_t v0;
  gg_type_10 v1;
  v0 = p0;
  {
  t1 = ((uint64_t)(v0));
  t2 = fg176i_ldm4t(t1);
  v1 = t2;
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((uint32_t)((v1).f0));
  (t0).f1 = ((uint32_t)((v1).f2));
  (t0).f2 = ((uint32_t)((v1).f1));
  (t0).f3 = ((uint32_t)((v1).f3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_7(int32_t p0, int32_t p1, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int64_t t5;
  int32_t t6;
  int32_t v0;
  int32_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int32_t)(v0));
  t2 = ((int32_t)(v1));
  t3 = ((int32_t)(((int64_t)UINT64_C(8))));
  t4 = ((int32_t)(((int32_t)gg_mod(t2, t3))));
  t5 = ((int64_t)(((int64_t)UINT64_C(2))));
  t6 = ((int32_t)(((int32_t)((uint64_t)(t4) << ((uint64_t)(t5) & 31)))));
  t0 = ((int32_t)(((t1) ^ (t6))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_6(uint64_t p0, int32_t p1, int32_t p2, int64_t base) {
  uint64_t t0;
  int32_t t1;
  int32_t t2;
  uint64_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t v0;
  int32_t v1;
  int32_t v2;
  int32_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int32_t)(v2));
  t2 = ((int32_t)(((int64_t)UINT64_C(16))));
  v3 = ((int32_t)(((int32_t)gg_mod(t1, t2))));
  t3 = ((uint64_t)(v0));
  t4 = ((int32_t)(v1));
  t5 = ((int32_t)(v3));
  t6 = ((int32_t)(((int64_t)UINT64_C(128))));
  t7 = ((int32_t)(((int32_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((int32_t)(((int32_t)((uint64_t)(t4) + (uint64_t)(t7)))));
  t9 = ((int32_t)(v2));
  t10 = ((int32_t)(((int64_t)UINT64_C(16))));
  t11 = ((int32_t)(((int32_t)gg_div(t9, t10))));
  t12 = ((int32_t)(((int64_t)UINT64_C(4))));
  t13 = ((int32_t)(((int32_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int32_t)(((int32_t)((uint64_t)(t8) + (uint64_t)(t13)))));
  t15 = ((int32_t)(v3));
  t16 = gg_function_7(t14, t15, base + 0);
  t17 = ((uint64_t)(((uint64_t)(t16))));
  t18 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t19 = ((uint64_t)(((uint64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t3) + (uint64_t)(t19)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_12(int32_t p0, int32_t p1, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t v0;
  int32_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int32_t)(v0));
  t2 = ((int32_t)(((int64_t)UINT64_C(4))));
  t3 = ((int32_t)(((int32_t)gg_div(t1, t2))));
  t4 = ((int32_t)(((int64_t)UINT64_C(8))));
  t5 = ((int32_t)(((int32_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((int32_t)(v1));
  t7 = ((int32_t)(((int64_t)UINT64_C(4))));
  t8 = ((int32_t)(((int32_t)gg_mod(t6, t7))));
  t9 = ((int32_t)(((int64_t)UINT64_C(2))));
  t10 = ((int32_t)(((int32_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t11 = ((int32_t)(((int32_t)((uint64_t)(t5) + (uint64_t)(t10)))));
  t12 = ((int32_t)(v0));
  t13 = ((int32_t)(((int64_t)UINT64_C(2))));
  t14 = ((int32_t)(((int32_t)gg_mod(t12, t13))));
  t0 = ((int32_t)(((int32_t)((uint64_t)(t11) + (uint64_t)(t14)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_11(int32_t p0, int32_t p1, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  int32_t t10;
  int32_t v0;
  int32_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int32_t)(v0));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((int32_t)(((int32_t)gg_div(t1, t2))));
  t4 = ((int32_t)(((int64_t)UINT64_C(2))));
  t5 = ((int32_t)(((int32_t)gg_mod(t3, t4))));
  t6 = ((int32_t)(((int64_t)UINT64_C(8))));
  t7 = ((int32_t)(((int32_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((int32_t)(v1));
  t9 = ((int32_t)(((int64_t)UINT64_C(4))));
  t10 = ((int32_t)(((int32_t)gg_div(t8, t9))));
  t0 = ((int32_t)(((int32_t)((uint64_t)(t7) + (uint64_t)(t10)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, float p6, int32_t p7, int32_t p8, int64_t p9, int32_t p10, uint64_t p11, uint64_t p12, uint64_t p13, uint64_t p14, uint64_t p15, int32_t p16, int32_t p17, int32_t p18, uint8_t p19, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int32_t t8;
  int32_t t9;
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
  float t20;
  float t21;
  uint64_t t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  int32_t t26;
  int32_t t27;
  float t28;
  float t29;
  int64_t t30;
  int32_t t31;
  int32_t t32;
  uint64_t t33;
  int32_t t34;
  int32_t t35;
  uint64_t t36;
  uint64_t t37;
  uint32_t t38;
  uint64_t t39;
  int32_t t40;
  int32_t t41;
  int32_t t42;
  int32_t t43;
  uint64_t t44;
  uint64_t t45;
  uint32_t t46;
  uint64_t t47;
  int32_t t48;
  int32_t t49;
  int32_t t50;
  int32_t t51;
  uint64_t t52;
  uint64_t t53;
  uint32_t t54;
  uint64_t t55;
  int32_t t56;
  int32_t t57;
  int32_t t58;
  int32_t t59;
  uint64_t t60;
  uint64_t t61;
  uint32_t t62;
  int32_t t63;
  int32_t t64;
  uint64_t t65;
  int32_t t66;
  int32_t t67;
  int32_t t68;
  int32_t t69;
  int32_t t70;
  int32_t t71;
  int32_t t72;
  int32_t t73;
  int32_t t74;
  int32_t t75;
  int32_t t76;
  int32_t t77;
  int32_t t78;
  int32_t t79;
  uint64_t t80;
  gg_type_10 t81;
  gg_type_11 t82;
  gg_slice t83;
  int32_t t84;
  int32_t t85;
  int32_t t86;
  float * t87;
  gg_slice t88;
  int32_t t89;
  int32_t t90;
  int32_t t91;
  int32_t t92;
  int32_t t93;
  float * t94;
  gg_slice t95;
  int32_t t96;
  int32_t t97;
  int32_t t98;
  int32_t t99;
  int32_t t100;
  float * t101;
  gg_slice t102;
  int32_t t103;
  int32_t t104;
  int32_t t105;
  int32_t t106;
  int32_t t107;
  float * t108;
  gg_type_11 t109;
  uint32_t t110;
  uint32_t t111;
  uint32_t t112;
  uint32_t t113;
  uint32_t t114;
  uint32_t t115;
  gg_type_11 t116;
  gg_slice t117;
  int32_t t118;
  int32_t t119;
  int32_t t120;
  float * t121;
  gg_slice t122;
  int32_t t123;
  int32_t t124;
  int32_t t125;
  int32_t t126;
  int32_t t127;
  float * t128;
  gg_slice t129;
  int32_t t130;
  int32_t t131;
  int32_t t132;
  int32_t t133;
  int32_t t134;
  float * t135;
  gg_slice t136;
  int32_t t137;
  int32_t t138;
  int32_t t139;
  int32_t t140;
  int32_t t141;
  float * t142;
  gg_type_11 t143;
  gg_slice t144;
  int32_t t145;
  int32_t t146;
  int32_t t147;
  int32_t t148;
  int32_t t149;
  float * t150;
  gg_slice t151;
  int32_t t152;
  int32_t t153;
  int32_t t154;
  int32_t t155;
  int32_t t156;
  float * t157;
  gg_slice t158;
  int32_t t159;
  int32_t t160;
  int32_t t161;
  int32_t t162;
  int32_t t163;
  float * t164;
  gg_slice t165;
  int32_t t166;
  int32_t t167;
  int32_t t168;
  int32_t t169;
  int32_t t170;
  float * t171;
  gg_type_11 t172;
  uint32_t t173;
  uint32_t t174;
  uint32_t t175;
  uint32_t t176;
  uint32_t t177;
  uint32_t t178;
  gg_type_11 t179;
  gg_slice t180;
  int32_t t181;
  int32_t t182;
  int32_t t183;
  int32_t t184;
  int32_t t185;
  float * t186;
  gg_slice t187;
  int32_t t188;
  int32_t t189;
  int32_t t190;
  int32_t t191;
  int32_t t192;
  float * t193;
  gg_slice t194;
  int32_t t195;
  int32_t t196;
  int32_t t197;
  int32_t t198;
  int32_t t199;
  float * t200;
  gg_slice t201;
  int32_t t202;
  int32_t t203;
  int32_t t204;
  int32_t t205;
  int32_t t206;
  float * t207;
  int32_t t208;
  int32_t t209;
  int32_t t210;
  int32_t t211;
  float t212;
  float t213;
  int64_t t214;
  float t215;
  float t216;
  int64_t t217;
  float t218;
  float t219;
  int64_t t220;
  int64_t t221;
  float * t222;
  uint64_t t223;
  uint32_t t224;
  uint32_t t225;
  float t226;
  int64_t t227;
  float * t228;
  uint64_t t229;
  uint64_t t230;
  uint64_t t231;
  uint32_t t232;
  uint32_t t233;
  float t234;
  int64_t t235;
  float * t236;
  int64_t t237;
  float * t238;
  int64_t t239;
  float * t240;
  int64_t t241;
  float * t242;
  int32_t t243;
  int32_t t244;
  int32_t t245;
  int32_t t246;
  int32_t t247;
  int32_t t248;
  int32_t t249;
  int32_t t250;
  int32_t t251;
  int32_t t252;
  int32_t t253;
  int32_t t254;
  int32_t t255;
  int32_t t256;
  int32_t t257;
  int32_t t258;
  int32_t t259;
  int32_t t260;
  int32_t t261;
  int32_t t262;
  int32_t t263;
  int32_t t264;
  int32_t t265;
  int32_t t266;
  int32_t t267;
  int32_t t268;
  int32_t t269;
  int32_t t270;
  uint64_t t271;
  uint64_t t272;
  uint64_t t273;
  uint64_t t274;
  uint64_t t275;
  uint32_t t276;
  gg_slice t277;
  int32_t t278;
  int32_t t279;
  int32_t t280;
  int32_t t281;
  int32_t t282;
  float * t283;
  float t284;
  uint32_t t285;
  float t286;
  float t287;
  gg_slice t288;
  int32_t t289;
  int32_t t290;
  int32_t t291;
  int32_t t292;
  int32_t t293;
  float * t294;
  float t295;
  float t296;
  gg_slice t297;
  int32_t t298;
  int32_t t299;
  int32_t t300;
  int32_t t301;
  int32_t t302;
  int32_t t303;
  int32_t t304;
  float * t305;
  float t306;
  uint32_t t307;
  float t308;
  float t309;
  gg_slice t310;
  int32_t t311;
  int32_t t312;
  int32_t t313;
  int32_t t314;
  int32_t t315;
  int32_t t316;
  int32_t t317;
  float * t318;
  float t319;
  float t320;
  int32_t t321;
  int32_t t322;
  int32_t t323;
  int32_t t324;
  int32_t t325;
  int32_t t326;
  int32_t t327;
  int32_t t328;
  gg_slice t329;
  int32_t t330;
  int32_t t331;
  int32_t t332;
  int32_t t333;
  int32_t t334;
  float * t335;
  float t336;
  float t337;
  float t338;
  int32_t t339;
  float * t340;
  float t341;
  uint8_t t342;
  int32_t t343;
  float * t344;
  int32_t t345;
  int32_t t346;
  int32_t t347;
  int32_t t348;
  int32_t t349;
  int32_t t350;
  int32_t t351;
  float * t352;
  float t353;
  int32_t t354;
  float t355;
  float t356;
  int32_t t357;
  float * t358;
  float t359;
  uint8_t t360;
  int32_t t361;
  float * t362;
  int32_t t363;
  float * t364;
  float t365;
  int32_t t366;
  float t367;
  float t368;
  int32_t t369;
  float * t370;
  float t371;
  uint8_t t372;
  int32_t t373;
  float * t374;
  int32_t t375;
  int32_t t376;
  int32_t t377;
  int32_t t378;
  int32_t t379;
  int32_t t380;
  int32_t t381;
  int32_t t382;
  int32_t t383;
  int32_t t384;
  gg_slice t385;
  int32_t t386;
  int32_t t387;
  int32_t t388;
  int32_t t389;
  int32_t t390;
  float * t391;
  float t392;
  int32_t t393;
  float * t394;
  float t395;
  float t396;
  float t397;
  gg_slice t398;
  int32_t t399;
  int32_t t400;
  int32_t t401;
  int32_t t402;
  int32_t t403;
  float * t404;
  int32_t t405;
  float * t406;
  float t407;
  float t408;
  int32_t t409;
  int32_t t410;
  int32_t t411;
  int32_t t412;
  float t413;
  float t414;
  int64_t t415;
  int32_t t416;
  int32_t t417;
  int32_t t418;
  float * t419;
  float t420;
  int32_t t421;
  float * t422;
  float t423;
  float t424;
  float t425;
  float t426;
  double t427;
  float t428;
  uint8_t t429;
  int32_t t430;
  float * t431;
  float t432;
  uint32_t t433;
  uint32_t t434;
  uint32_t t435;
  uint32_t t436;
  float t437;
  uint64_t t438;
  int32_t t439;
  int32_t t440;
  uint64_t t441;
  uint64_t t442;
  int32_t t443;
  float * t444;
  float t445;
  uint32_t t446;
  uint32_t t447;
  uint64_t t448;
  int32_t t449;
  int32_t t450;
  uint64_t t451;
  uint64_t t452;
  uint32_t t453;
  uint32_t t454;
  float t455;
  uint64_t t456;
  int32_t t457;
  int32_t t458;
  uint64_t t459;
  uint64_t t460;
  int32_t t461;
  float * t462;
  float t463;
  float t464;
  int32_t t465;
  float * t466;
  float t467;
  float t468;
  float t469;
  uint32_t t470;
  uint32_t t471;
  int32_t t472;
  int32_t t473;
  uint32_t t474;
  uint32_t t475;
  int64_t t476;
  int32_t t477;
  int32_t t478;
  int32_t t479;
  uint32_t * t480;
  int32_t t481;
  float * t482;
  float t483;
  int32_t t484;
  float * t485;
  float t486;
  uint32_t t487;
  int32_t t488;
  int32_t t489;
  int32_t t490;
  int32_t t491;
  int32_t t492;
  int32_t t493;
  int32_t t494;
  int32_t t495;
  int32_t t496;
  int32_t t497;
  int32_t t498;
  int32_t t499;
  int32_t t500;
  int32_t t501;
  int32_t t502;
  int32_t t503;
  uint64_t t504;
  int32_t t505;
  int32_t t506;
  uint64_t t507;
  uint64_t t508;
  uint64_t t509;
  int32_t t510;
  int32_t t511;
  uint64_t t512;
  uint64_t t513;
  uint32_t t514;
  uint32_t t515;
  int32_t t516;
  uint32_t * t517;
  uint32_t t518;
  uint32_t t519;
  uint32_t t520;
  int32_t t521;
  int32_t t522;
  int32_t t523;
  int32_t t524;
  int32_t t525;
  int32_t t526;
  uint32_t t527;
  uint32_t t528;
  int64_t t529;
  int32_t t530;
  int32_t t531;
  int32_t t532;
  int32_t t533;
  gg_slice t534;
  int32_t t535;
  int32_t t536;
  int32_t t537;
  int32_t t538;
  int32_t t539;
  int32_t t540;
  int32_t t541;
  uint32_t * t542;
  gg_slice t543;
  int32_t t544;
  int32_t t545;
  int32_t t546;
  int32_t t547;
  int32_t t548;
  float * t549;
  float t550;
  gg_slice t551;
  int32_t t552;
  int32_t t553;
  int32_t t554;
  int32_t t555;
  int32_t t556;
  int32_t t557;
  int32_t t558;
  float * t559;
  float t560;
  uint32_t t561;
  int32_t t562;
  int32_t t563;
  int32_t t564;
  int32_t t565;
  uint8_t t566;
  uint8_t t567;
  uint64_t t568;
  uint64_t t569;
  int64_t t570;
  int64_t t571;
  int64_t t572;
  int64_t t573;
  int64_t t574;
  int64_t t575;
  int32_t t576;
  int32_t t577;
  int32_t t578;
  uint64_t t579;
  uint64_t t580;
  int64_t t581;
  int64_t t582;
  int64_t t583;
  int64_t t584;
  int32_t t585;
  int32_t t586;
  int32_t t587;
  int32_t t588;
  uint64_t t589;
  int32_t t590;
  int32_t t591;
  uint64_t t592;
  uint64_t t593;
  uint32_t t594;
  uint64_t t595;
  int32_t t596;
  int32_t t597;
  int32_t t598;
  int32_t t599;
  uint64_t t600;
  uint64_t t601;
  uint32_t t602;
  uint64_t t603;
  int32_t t604;
  int32_t t605;
  int32_t t606;
  int32_t t607;
  uint64_t t608;
  uint64_t t609;
  uint32_t t610;
  uint64_t t611;
  int32_t t612;
  int32_t t613;
  int32_t t614;
  int32_t t615;
  uint64_t t616;
  uint64_t t617;
  uint32_t t618;
  int32_t t619;
  int32_t t620;
  int32_t t621;
  int32_t t622;
  int32_t t623;
  int32_t t624;
  int32_t t625;
  int32_t t626;
  int32_t t627;
  int32_t t628;
  int32_t t629;
  int32_t t630;
  int32_t t631;
  int32_t t632;
  uint64_t t633;
  int32_t t634;
  int32_t t635;
  int32_t t636;
  int32_t t637;
  int32_t t638;
  int32_t t639;
  int32_t t640;
  int32_t t641;
  int32_t t642;
  int32_t t643;
  uint64_t t644;
  gg_type_15 t645;
  gg_slice t646;
  int32_t t647;
  int32_t t648;
  int32_t t649;
  uint32_t * t650;
  uint32_t t651;
  gg_slice t652;
  int32_t t653;
  int32_t t654;
  int32_t t655;
  int32_t t656;
  int32_t t657;
  uint32_t * t658;
  uint32_t t659;
  gg_slice t660;
  int32_t t661;
  int32_t t662;
  int32_t t663;
  int32_t t664;
  int32_t t665;
  uint32_t * t666;
  uint32_t t667;
  gg_slice t668;
  int32_t t669;
  int32_t t670;
  int32_t t671;
  int32_t t672;
  int32_t t673;
  uint32_t * t674;
  uint32_t t675;
  uint32_t t676;
  uint32_t t677;
  gg_type_15 t678;
  gg_type_15 t679;
  gg_slice t680;
  int32_t t681;
  int32_t t682;
  int32_t t683;
  uint32_t * t684;
  uint32_t t685;
  gg_slice t686;
  int32_t t687;
  int32_t t688;
  int32_t t689;
  int32_t t690;
  int32_t t691;
  uint32_t * t692;
  uint32_t t693;
  gg_slice t694;
  int32_t t695;
  int32_t t696;
  int32_t t697;
  int32_t t698;
  int32_t t699;
  uint32_t * t700;
  uint32_t t701;
  gg_slice t702;
  int32_t t703;
  int32_t t704;
  int32_t t705;
  int32_t t706;
  int32_t t707;
  uint32_t * t708;
  uint32_t t709;
  uint32_t t710;
  uint32_t t711;
  gg_type_15 t712;
  int32_t t713;
  int32_t t714;
  uint64_t t715;
  int32_t t716;
  int32_t t717;
  uint64_t t718;
  uint64_t t719;
  uint32_t t720;
  uint64_t t721;
  int32_t t722;
  int32_t t723;
  int32_t t724;
  int32_t t725;
  uint64_t t726;
  uint64_t t727;
  uint32_t t728;
  uint64_t t729;
  int32_t t730;
  int32_t t731;
  int32_t t732;
  int32_t t733;
  uint64_t t734;
  uint64_t t735;
  uint32_t t736;
  uint64_t t737;
  int32_t t738;
  int32_t t739;
  int32_t t740;
  int32_t t741;
  uint64_t t742;
  uint64_t t743;
  uint32_t t744;
  int32_t t745;
  int32_t t746;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  float v6;
  int32_t v7;
  int32_t v8;
  int64_t v9;
  int32_t v10;
  uint64_t v11;
  uint64_t v12;
  uint64_t v13;
  uint64_t v14;
  uint64_t v15;
  int32_t v16;
  int32_t v17;
  int32_t v18;
  uint8_t v19;
  uint64_t v20;
  uint64_t v21;
  uint64_t v22;
  int64_t v23;
  int64_t v24;
  int32_t v25;
  int32_t v26;
  int32_t v27;
  float v28;
  int32_t v29;
  gg_type_7 v30;
  int32_t v31;
  uint32_t v32;
  uint32_t v33;
  uint32_t v34;
  uint32_t v35;
  gg_type_10 v36;
  gg_type_11 v37;
  gg_type_11 v38;
  gg_type_12 v39;
  gg_type_12 v40;
  gg_type_12 v41;
  int32_t v42;
  int32_t v43;
  int32_t v44;
  uint32_t v45;
  int32_t v46;
  int32_t v47;
  float v48;
  int32_t v49;
  float v50;
  int32_t v51;
  int32_t v52;
  float v53;
  gg_type_12 v54;
  float v55;
  float v56;
  uint32_t v57;
  float v58;
  gg_type_13 v59;
  int32_t v60;
  int32_t v61;
  int32_t v62;
  gg_type_14 v63;
  int32_t v64;
  gg_type_15 v65;
  gg_type_15 v66;
  int32_t v67;
  int32_t v68;
  gg_type_10 v69;
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
  v20 = ((uint64_t)(v15));
  t0 = ((uint64_t)(v15));
  t1 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(16384))))));
  v21 = ((uint64_t)(((uint64_t)((uint64_t)(t0) + (uint64_t)(t1)))));
  t2 = ((uint64_t)(v15));
  t3 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32768))))));
  v22 = ((uint64_t)(((uint64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t4 = ((int64_t)(((int64_t)(v8))));
  t5 = ((int64_t)(((int64_t)UINT64_C(32))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t6 = ((int64_t)(((int64_t)(v7))));
  t7 = ((int64_t)(((int64_t)(v10))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t8 = ((int32_t)(v17));
  t9 = ((int32_t)(v18));
  t10 = ((int32_t)(((int32_t)gg_div(t8, t9))));
  t11 = ((int32_t)(((int64_t)UINT64_C(16))));
  v25 = ((int32_t)(((int32_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t12 = ((int32_t)(v17));
  t13 = ((int32_t)(v18));
  t14 = ((int32_t)(((int32_t)gg_mod(t12, t13))));
  t15 = ((int32_t)(((int64_t)UINT64_C(16))));
  v26 = ((int32_t)(((int32_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t16 = ((int32_t)(((int64_t)UINT64_C(32))));
  t17 = ((int32_t)(v18));
  t18 = ((int32_t)(((int64_t)UINT64_C(16))));
  t19 = ((int32_t)(((int32_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  v27 = ((int32_t)(((int32_t)gg_div(t16, t19))));
  t20 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t21 = ((float)(((float)(gg_double_bits(UINT64_C(4604418109348737398))))));
  v28 = ((float)(((t20) * (t21))));
  v29 = ((int32_t)(((int64_t)UINT64_C(0))));
  fg176i_cpwait();
  fg176i_barrier();
  t22 = ((uint64_t)(v1));
  t23 = ((uint64_t)(v21));
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(v23));
  t26 = ((int32_t)(v16));
  t27 = ((int32_t)(v17));
  fg175l_load_kv(t22, t23, t24, t25, t26, t27);
  memset(&t28, 0, sizeof(t28));
  t29 = ((float)(t28));
  for (t30=0; t30<16; t30++) (v30).data[t30] = t29;
  v31 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t31 = ((int32_t)(v31));
  t32 = ((int32_t)(((int64_t)UINT64_C(16))));
  if (!(((t31) < (t32)))) goto label2;
  {
  t33 = ((uint64_t)(v11));
  t34 = ((int32_t)(v31));
  t35 = ((int32_t)(((int64_t)UINT64_C(16))));
  t36 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t34) * (uint64_t)(t35)))))));
  t37 = ((uint64_t)(((uint64_t)((uint64_t)(t33) + (uint64_t)(t36)))));
  t38 = fg176i_ldu(t37);
  v32 = ((uint32_t)(t38));
  t39 = ((uint64_t)(v11));
  t40 = ((int32_t)(v31));
  t41 = ((int32_t)(((int64_t)UINT64_C(16))));
  t42 = ((int32_t)(((int32_t)((uint64_t)(t40) * (uint64_t)(t41)))));
  t43 = ((int32_t)(((int64_t)UINT64_C(4))));
  t44 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t42) + (uint64_t)(t43)))))));
  t45 = ((uint64_t)(((uint64_t)((uint64_t)(t39) + (uint64_t)(t44)))));
  t46 = fg176i_ldu(t45);
  v33 = ((uint32_t)(t46));
  t47 = ((uint64_t)(v11));
  t48 = ((int32_t)(v31));
  t49 = ((int32_t)(((int64_t)UINT64_C(16))));
  t50 = ((int32_t)(((int32_t)((uint64_t)(t48) * (uint64_t)(t49)))));
  t51 = ((int32_t)(((int64_t)UINT64_C(8))));
  t52 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t50) + (uint64_t)(t51)))))));
  t53 = ((uint64_t)(((uint64_t)((uint64_t)(t47) + (uint64_t)(t52)))));
  t54 = fg176i_ldu(t53);
  v34 = ((uint32_t)(t54));
  t55 = ((uint64_t)(v11));
  t56 = ((int32_t)(v31));
  t57 = ((int32_t)(((int64_t)UINT64_C(16))));
  t58 = ((int32_t)(((int32_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  t59 = ((int32_t)(((int64_t)UINT64_C(12))));
  t60 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t58) + (uint64_t)(t59)))))));
  t61 = ((uint64_t)(((uint64_t)((uint64_t)(t55) + (uint64_t)(t60)))));
  t62 = fg176i_ldu(t61);
  v35 = ((uint32_t)(t62));
  v29 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t63 = ((int32_t)(v29));
  t64 = ((int32_t)(v27));
  if (!(((t63) < (t64)))) goto label5;
  {
  t65 = ((uint64_t)(v20));
  t66 = ((int32_t)(v29));
  t67 = ((int32_t)(v18));
  t68 = ((int32_t)(((int32_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  t69 = ((int32_t)(((int64_t)UINT64_C(16))));
  t70 = ((int32_t)(((int32_t)((uint64_t)(t68) * (uint64_t)(t69)))));
  t71 = ((int32_t)(v26));
  t72 = ((int32_t)(((int32_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  t73 = ((int32_t)(((int64_t)UINT64_C(128))));
  t74 = ((int32_t)(((int32_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  t75 = ((int32_t)(v31));
  t76 = ((int32_t)(((int64_t)UINT64_C(8))));
  t77 = ((int32_t)(((int32_t)((uint64_t)(t75) * (uint64_t)(t76)))));
  t78 = ((int32_t)(((int32_t)((uint64_t)(t74) + (uint64_t)(t77)))));
  t79 = ((int32_t)(v16));
  t80 = gg_function_6(t65, t78, t79, base + 0);
  t81 = fg176i_ldm4(t80);
  v36 = t81;
  memset(&(t82), 0, sizeof(t82));
  t83 = gg_view((v30).data, 16);
  t84 = ((int32_t)(v29));
  t85 = ((int32_t)(((int64_t)UINT64_C(8))));
  t86 = ((int32_t)((uint64_t)(t84) * (uint64_t)(t85)));
  t87 = ((float *)t83.data) + gg_index(t86, t83.len);
  (t82).f4 = ((float)((*t87)));
  t88 = gg_view((v30).data, 16);
  t89 = ((int32_t)(v29));
  t90 = ((int32_t)(((int64_t)UINT64_C(8))));
  t91 = ((int32_t)(((int32_t)((uint64_t)(t89) * (uint64_t)(t90)))));
  t92 = ((int32_t)(((int64_t)UINT64_C(1))));
  t93 = ((int32_t)((uint64_t)(t91) + (uint64_t)(t92)));
  t94 = ((float *)t88.data) + gg_index(t93, t88.len);
  (t82).f5 = ((float)((*t94)));
  t95 = gg_view((v30).data, 16);
  t96 = ((int32_t)(v29));
  t97 = ((int32_t)(((int64_t)UINT64_C(8))));
  t98 = ((int32_t)(((int32_t)((uint64_t)(t96) * (uint64_t)(t97)))));
  t99 = ((int32_t)(((int64_t)UINT64_C(2))));
  t100 = ((int32_t)((uint64_t)(t98) + (uint64_t)(t99)));
  t101 = ((float *)t95.data) + gg_index(t100, t95.len);
  (t82).f6 = ((float)((*t101)));
  t102 = gg_view((v30).data, 16);
  t103 = ((int32_t)(v29));
  t104 = ((int32_t)(((int64_t)UINT64_C(8))));
  t105 = ((int32_t)(((int32_t)((uint64_t)(t103) * (uint64_t)(t104)))));
  t106 = ((int32_t)(((int64_t)UINT64_C(3))));
  t107 = ((int32_t)((uint64_t)(t105) + (uint64_t)(t106)));
  t108 = ((float *)t102.data) + gg_index(t107, t102.len);
  (t82).f7 = ((float)((*t108)));
  t109 = t82;
  t110 = ((uint32_t)(v32));
  t111 = ((uint32_t)(v33));
  t112 = ((uint32_t)(v34));
  t113 = ((uint32_t)(v35));
  t114 = ((uint32_t)((v36).f0));
  t115 = ((uint32_t)((v36).f2));
  t116 = fg176i_mma_f32(t109, t110, t111, t112, t113, t114, t115);
  v37 = t116;
  t117 = gg_view((v30).data, 16);
  t118 = ((int32_t)(v29));
  t119 = ((int32_t)(((int64_t)UINT64_C(8))));
  t120 = ((int32_t)((uint64_t)(t118) * (uint64_t)(t119)));
  t121 = ((float *)t117.data) + gg_index(t120, t117.len);
  (*t121) = ((float)((v37).f4));
  t122 = gg_view((v30).data, 16);
  t123 = ((int32_t)(v29));
  t124 = ((int32_t)(((int64_t)UINT64_C(8))));
  t125 = ((int32_t)(((int32_t)((uint64_t)(t123) * (uint64_t)(t124)))));
  t126 = ((int32_t)(((int64_t)UINT64_C(1))));
  t127 = ((int32_t)((uint64_t)(t125) + (uint64_t)(t126)));
  t128 = ((float *)t122.data) + gg_index(t127, t122.len);
  (*t128) = ((float)((v37).f5));
  t129 = gg_view((v30).data, 16);
  t130 = ((int32_t)(v29));
  t131 = ((int32_t)(((int64_t)UINT64_C(8))));
  t132 = ((int32_t)(((int32_t)((uint64_t)(t130) * (uint64_t)(t131)))));
  t133 = ((int32_t)(((int64_t)UINT64_C(2))));
  t134 = ((int32_t)((uint64_t)(t132) + (uint64_t)(t133)));
  t135 = ((float *)t129.data) + gg_index(t134, t129.len);
  (*t135) = ((float)((v37).f6));
  t136 = gg_view((v30).data, 16);
  t137 = ((int32_t)(v29));
  t138 = ((int32_t)(((int64_t)UINT64_C(8))));
  t139 = ((int32_t)(((int32_t)((uint64_t)(t137) * (uint64_t)(t138)))));
  t140 = ((int32_t)(((int64_t)UINT64_C(3))));
  t141 = ((int32_t)((uint64_t)(t139) + (uint64_t)(t140)));
  t142 = ((float *)t136.data) + gg_index(t141, t136.len);
  (*t142) = ((float)((v37).f7));
  memset(&(t143), 0, sizeof(t143));
  t144 = gg_view((v30).data, 16);
  t145 = ((int32_t)(v29));
  t146 = ((int32_t)(((int64_t)UINT64_C(8))));
  t147 = ((int32_t)(((int32_t)((uint64_t)(t145) * (uint64_t)(t146)))));
  t148 = ((int32_t)(((int64_t)UINT64_C(4))));
  t149 = ((int32_t)((uint64_t)(t147) + (uint64_t)(t148)));
  t150 = ((float *)t144.data) + gg_index(t149, t144.len);
  (t143).f4 = ((float)((*t150)));
  t151 = gg_view((v30).data, 16);
  t152 = ((int32_t)(v29));
  t153 = ((int32_t)(((int64_t)UINT64_C(8))));
  t154 = ((int32_t)(((int32_t)((uint64_t)(t152) * (uint64_t)(t153)))));
  t155 = ((int32_t)(((int64_t)UINT64_C(5))));
  t156 = ((int32_t)((uint64_t)(t154) + (uint64_t)(t155)));
  t157 = ((float *)t151.data) + gg_index(t156, t151.len);
  (t143).f5 = ((float)((*t157)));
  t158 = gg_view((v30).data, 16);
  t159 = ((int32_t)(v29));
  t160 = ((int32_t)(((int64_t)UINT64_C(8))));
  t161 = ((int32_t)(((int32_t)((uint64_t)(t159) * (uint64_t)(t160)))));
  t162 = ((int32_t)(((int64_t)UINT64_C(6))));
  t163 = ((int32_t)((uint64_t)(t161) + (uint64_t)(t162)));
  t164 = ((float *)t158.data) + gg_index(t163, t158.len);
  (t143).f6 = ((float)((*t164)));
  t165 = gg_view((v30).data, 16);
  t166 = ((int32_t)(v29));
  t167 = ((int32_t)(((int64_t)UINT64_C(8))));
  t168 = ((int32_t)(((int32_t)((uint64_t)(t166) * (uint64_t)(t167)))));
  t169 = ((int32_t)(((int64_t)UINT64_C(7))));
  t170 = ((int32_t)((uint64_t)(t168) + (uint64_t)(t169)));
  t171 = ((float *)t165.data) + gg_index(t170, t165.len);
  (t143).f7 = ((float)((*t171)));
  t172 = t143;
  t173 = ((uint32_t)(v32));
  t174 = ((uint32_t)(v33));
  t175 = ((uint32_t)(v34));
  t176 = ((uint32_t)(v35));
  t177 = ((uint32_t)((v36).f1));
  t178 = ((uint32_t)((v36).f3));
  t179 = fg176i_mma_f32(t172, t173, t174, t175, t176, t177, t178);
  v38 = t179;
  t180 = gg_view((v30).data, 16);
  t181 = ((int32_t)(v29));
  t182 = ((int32_t)(((int64_t)UINT64_C(8))));
  t183 = ((int32_t)(((int32_t)((uint64_t)(t181) * (uint64_t)(t182)))));
  t184 = ((int32_t)(((int64_t)UINT64_C(4))));
  t185 = ((int32_t)((uint64_t)(t183) + (uint64_t)(t184)));
  t186 = ((float *)t180.data) + gg_index(t185, t180.len);
  (*t186) = ((float)((v38).f4));
  t187 = gg_view((v30).data, 16);
  t188 = ((int32_t)(v29));
  t189 = ((int32_t)(((int64_t)UINT64_C(8))));
  t190 = ((int32_t)(((int32_t)((uint64_t)(t188) * (uint64_t)(t189)))));
  t191 = ((int32_t)(((int64_t)UINT64_C(5))));
  t192 = ((int32_t)((uint64_t)(t190) + (uint64_t)(t191)));
  t193 = ((float *)t187.data) + gg_index(t192, t187.len);
  (*t193) = ((float)((v38).f5));
  t194 = gg_view((v30).data, 16);
  t195 = ((int32_t)(v29));
  t196 = ((int32_t)(((int64_t)UINT64_C(8))));
  t197 = ((int32_t)(((int32_t)((uint64_t)(t195) * (uint64_t)(t196)))));
  t198 = ((int32_t)(((int64_t)UINT64_C(6))));
  t199 = ((int32_t)((uint64_t)(t197) + (uint64_t)(t198)));
  t200 = ((float *)t194.data) + gg_index(t199, t194.len);
  (*t200) = ((float)((v38).f6));
  t201 = gg_view((v30).data, 16);
  t202 = ((int32_t)(v29));
  t203 = ((int32_t)(((int64_t)UINT64_C(8))));
  t204 = ((int32_t)(((int32_t)((uint64_t)(t202) * (uint64_t)(t203)))));
  t205 = ((int32_t)(((int64_t)UINT64_C(7))));
  t206 = ((int32_t)((uint64_t)(t204) + (uint64_t)(t205)));
  t207 = ((float *)t201.data) + gg_index(t206, t201.len);
  (*t207) = ((float)((v38).f7));
  t208 = v29;
  t209 = ((int32_t)(((int64_t)UINT64_C(1))));
  v29 = ((int32_t)((uint64_t)(t208) + (uint64_t)(t209)));
  }
  goto label4;
label5:;
  t210 = v31;
  t211 = ((int32_t)(((int64_t)UINT64_C(1))));
  v31 = ((int32_t)((uint64_t)(t210) + (uint64_t)(t211)));
  }
  goto label1;
label2:;
  memset(&t212, 0, sizeof(t212));
  t213 = ((float)(t212));
  for (t214=0; t214<2; t214++) (v39).data[t214] = t213;
  memset(&t215, 0, sizeof(t215));
  t216 = ((float)(t215));
  for (t217=0; t217<2; t217++) (v40).data[t217] = t216;
  memset(&t218, 0, sizeof(t218));
  t219 = ((float)(t218));
  for (t220=0; t220<2; t220++) (v41).data[t220] = t219;
  /* FG_DIRECT_SLICE_INDEX */
  t221 = ((int64_t)UINT64_C(0));
  t222 = ((float *)(v39).data) + gg_index(t221, ((int64_t)UINT64_C(2)));
  t223 = ((uint64_t)(v13));
  t224 = fg176i_ldu(t223);
  t225 = ((uint32_t)(t224));
  t226 = fg176i_frombits(t225);
  (*t222) = ((float)(t226));
  /* FG_DIRECT_SLICE_INDEX */
  t227 = ((int64_t)UINT64_C(1));
  t228 = ((float *)(v39).data) + gg_index(t227, ((int64_t)UINT64_C(2)));
  t229 = ((uint64_t)(v13));
  t230 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t231 = ((uint64_t)(((uint64_t)((uint64_t)(t229) + (uint64_t)(t230)))));
  t232 = fg176i_ldu(t231);
  t233 = ((uint32_t)(t232));
  t234 = fg176i_frombits(t233);
  (*t228) = ((float)(t234));
  /* FG_DIRECT_SLICE_INDEX */
  t235 = ((int64_t)UINT64_C(0));
  t236 = ((float *)(v40).data) + gg_index(t235, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t237 = ((int64_t)UINT64_C(0));
  t238 = ((float *)(v39).data) + gg_index(t237, ((int64_t)UINT64_C(2)));
  (*t236) = ((float)((*t238)));
  /* FG_DIRECT_SLICE_INDEX */
  t239 = ((int64_t)UINT64_C(1));
  t240 = ((float *)(v40).data) + gg_index(t239, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t241 = ((int64_t)UINT64_C(1));
  t242 = ((float *)(v39).data) + gg_index(t241, ((int64_t)UINT64_C(2)));
  (*t240) = ((float)((*t242)));
  v29 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t243 = ((int32_t)(v29));
  t244 = ((int32_t)(v27));
  if (!(((t243) < (t244)))) goto label8;
  {
  t245 = ((int32_t)(v29));
  t246 = ((int32_t)(v18));
  t247 = ((int32_t)(((int32_t)((uint64_t)(t245) * (uint64_t)(t246)))));
  t248 = ((int32_t)(((int64_t)UINT64_C(16))));
  t249 = ((int32_t)(((int32_t)((uint64_t)(t247) * (uint64_t)(t248)))));
  t250 = ((int32_t)(v26));
  v42 = ((int32_t)(((int32_t)((uint64_t)(t249) + (uint64_t)(t250)))));
  v43 = ((int32_t)(((int64_t)UINT64_C(0))));
label10:;
  t251 = ((int32_t)(v43));
  t252 = ((int32_t)(((int64_t)UINT64_C(8))));
  if (!(((t251) < (t252)))) goto label11;
  {
  t253 = ((int32_t)(v25));
  t254 = ((int32_t)(v43));
  t255 = ((int32_t)(v16));
  t256 = gg_function_11(t254, t255, base + 0);
  t257 = ((int32_t)(t256));
  t258 = ((int32_t)(((int32_t)((uint64_t)(t253) + (uint64_t)(t257)))));
  t259 = ((int32_t)(((int64_t)UINT64_C(8))));
  t260 = ((int32_t)(((int32_t)gg_div(t258, t259))));
  t261 = ((int32_t)(((int64_t)UINT64_C(20))));
  t262 = ((int32_t)(((int32_t)((uint64_t)(t260) * (uint64_t)(t261)))));
  t263 = ((int32_t)(v42));
  t264 = ((int32_t)(v43));
  t265 = ((int32_t)(v16));
  t266 = gg_function_12(t264, t265, base + 0);
  t267 = ((int32_t)(t266));
  t268 = ((int32_t)(((int32_t)((uint64_t)(t263) + (uint64_t)(t267)))));
  t269 = ((int32_t)(((int64_t)UINT64_C(2))));
  t270 = ((int32_t)(((int32_t)gg_div(t268, t269))));
  v44 = ((int32_t)(((int32_t)((uint64_t)(t262) + (uint64_t)(t270)))));
  t271 = ((uint64_t)(v22));
  t272 = ((uint64_t)(((uint64_t)(v44))));
  t273 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t274 = ((uint64_t)(((uint64_t)((uint64_t)(t272) * (uint64_t)(t273)))));
  t275 = ((uint64_t)(((uint64_t)((uint64_t)(t271) + (uint64_t)(t274)))));
  t276 = fg176i_ldu(t275);
  v45 = ((uint32_t)(t276));
  t277 = gg_view((v30).data, 16);
  t278 = ((int32_t)(v29));
  t279 = ((int32_t)(((int64_t)UINT64_C(8))));
  t280 = ((int32_t)(((int32_t)((uint64_t)(t278) * (uint64_t)(t279)))));
  t281 = ((int32_t)(v43));
  t282 = ((int32_t)((uint64_t)(t280) + (uint64_t)(t281)));
  t283 = ((float *)t277.data) + gg_index(t282, t277.len);
  t284 = ((float)(v6));
  t285 = ((uint32_t)(v45));
  t286 = fg176i_hlo(t285);
  t287 = ((float)(t286));
  t288 = gg_view((v30).data, 16);
  t289 = ((int32_t)(v29));
  t290 = ((int32_t)(((int64_t)UINT64_C(8))));
  t291 = ((int32_t)(((int32_t)((uint64_t)(t289) * (uint64_t)(t290)))));
  t292 = ((int32_t)(v43));
  t293 = ((int32_t)((uint64_t)(t291) + (uint64_t)(t292)));
  t294 = ((float *)t288.data) + gg_index(t293, t288.len);
  t295 = ((float)((*t294)));
  t296 = fg176i_fmaf(t284, t287, t295);
  (*t283) = ((float)(t296));
  t297 = gg_view((v30).data, 16);
  t298 = ((int32_t)(v29));
  t299 = ((int32_t)(((int64_t)UINT64_C(8))));
  t300 = ((int32_t)(((int32_t)((uint64_t)(t298) * (uint64_t)(t299)))));
  t301 = ((int32_t)(v43));
  t302 = ((int32_t)(((int32_t)((uint64_t)(t300) + (uint64_t)(t301)))));
  t303 = ((int32_t)(((int64_t)UINT64_C(1))));
  t304 = ((int32_t)((uint64_t)(t302) + (uint64_t)(t303)));
  t305 = ((float *)t297.data) + gg_index(t304, t297.len);
  t306 = ((float)(v6));
  t307 = ((uint32_t)(v45));
  t308 = fg176i_hhi(t307);
  t309 = ((float)(t308));
  t310 = gg_view((v30).data, 16);
  t311 = ((int32_t)(v29));
  t312 = ((int32_t)(((int64_t)UINT64_C(8))));
  t313 = ((int32_t)(((int32_t)((uint64_t)(t311) * (uint64_t)(t312)))));
  t314 = ((int32_t)(v43));
  t315 = ((int32_t)(((int32_t)((uint64_t)(t313) + (uint64_t)(t314)))));
  t316 = ((int32_t)(((int64_t)UINT64_C(1))));
  t317 = ((int32_t)((uint64_t)(t315) + (uint64_t)(t316)));
  t318 = ((float *)t310.data) + gg_index(t317, t310.len);
  t319 = ((float)((*t318)));
  t320 = fg176i_fmaf(t306, t309, t319);
  (*t305) = ((float)(t320));
  t321 = v43;
  t322 = ((int32_t)(((int64_t)UINT64_C(2))));
  v43 = ((int32_t)((uint64_t)(t321) + (uint64_t)(t322)));
  }
  goto label10;
label11:;
  v46 = ((int32_t)(((int64_t)UINT64_C(0))));
label13:;
  t323 = ((int32_t)(v46));
  t324 = ((int32_t)(((int64_t)UINT64_C(8))));
  if (!(((t323) < (t324)))) goto label14;
  {
  t325 = ((int32_t)(v46));
  t326 = ((int32_t)(((int64_t)UINT64_C(2))));
  t327 = ((int32_t)(((int32_t)gg_div(t325, t326))));
  t328 = ((int32_t)(((int64_t)UINT64_C(2))));
  v47 = ((int32_t)(((int32_t)gg_mod(t327, t328))));
  t329 = gg_view((v30).data, 16);
  t330 = ((int32_t)(v29));
  t331 = ((int32_t)(((int64_t)UINT64_C(8))));
  t332 = ((int32_t)(((int32_t)((uint64_t)(t330) * (uint64_t)(t331)))));
  t333 = ((int32_t)(v46));
  t334 = ((int32_t)((uint64_t)(t332) + (uint64_t)(t333)));
  t335 = ((float *)t329.data) + gg_index(t334, t329.len);
  t336 = ((float)((*t335)));
  t337 = ((float)(v28));
  v48 = ((float)(((t336) + (t337))));
  t338 = ((float)(v48));
  /* FG_DIRECT_SLICE_INDEX */
  t339 = v47;
  t340 = ((float *)(v40).data) + gg_index(t339, ((int64_t)UINT64_C(2)));
  t341 = ((float)((*t340)));
  t342 = ((t338) > (t341));
  if (t342) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t343 = v47;
  t344 = ((float *)(v40).data) + gg_index(t343, ((int64_t)UINT64_C(2)));
  (*t344) = ((float)(v48));
  }
  }
  t345 = v46;
  t346 = ((int32_t)(((int64_t)UINT64_C(1))));
  v46 = ((int32_t)((uint64_t)(t345) + (uint64_t)(t346)));
  }
  goto label13;
label14:;
  t347 = v29;
  t348 = ((int32_t)(((int64_t)UINT64_C(1))));
  v29 = ((int32_t)((uint64_t)(t347) + (uint64_t)(t348)));
  }
  goto label7;
label8:;
  v49 = ((int32_t)(((int64_t)UINT64_C(0))));
label16:;
  t349 = ((int32_t)(v49));
  t350 = ((int32_t)(((int64_t)UINT64_C(2))));
  if (!(((t349) < (t350)))) goto label17;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t351 = v49;
  t352 = ((float *)(v40).data) + gg_index(t351, ((int64_t)UINT64_C(2)));
  t353 = ((float)((*t352)));
  t354 = ((int32_t)(((int64_t)UINT64_C(2))));
  t355 = fg176i_shflx(t353, t354);
  v50 = ((float)(t355));
  t356 = ((float)(v50));
  /* FG_DIRECT_SLICE_INDEX */
  t357 = v49;
  t358 = ((float *)(v40).data) + gg_index(t357, ((int64_t)UINT64_C(2)));
  t359 = ((float)((*t358)));
  t360 = ((t356) > (t359));
  if (t360) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t361 = v49;
  t362 = ((float *)(v40).data) + gg_index(t361, ((int64_t)UINT64_C(2)));
  (*t362) = ((float)(v50));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t363 = v49;
  t364 = ((float *)(v40).data) + gg_index(t363, ((int64_t)UINT64_C(2)));
  t365 = ((float)((*t364)));
  t366 = ((int32_t)(((int64_t)UINT64_C(1))));
  t367 = fg176i_shflx(t365, t366);
  v50 = ((float)(t367));
  t368 = ((float)(v50));
  /* FG_DIRECT_SLICE_INDEX */
  t369 = v49;
  t370 = ((float *)(v40).data) + gg_index(t369, ((int64_t)UINT64_C(2)));
  t371 = ((float)((*t370)));
  t372 = ((t368) > (t371));
  if (t372) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t373 = v49;
  t374 = ((float *)(v40).data) + gg_index(t373, ((int64_t)UINT64_C(2)));
  (*t374) = ((float)(v50));
  }
  }
  t375 = v49;
  t376 = ((int32_t)(((int64_t)UINT64_C(1))));
  v49 = ((int32_t)((uint64_t)(t375) + (uint64_t)(t376)));
  }
  goto label16;
label17:;
  v29 = ((int32_t)(((int64_t)UINT64_C(0))));
label19:;
  t377 = ((int32_t)(v29));
  t378 = ((int32_t)(v27));
  if (!(((t377) < (t378)))) goto label20;
  {
  v51 = ((int32_t)(((int64_t)UINT64_C(0))));
label22:;
  t379 = ((int32_t)(v51));
  t380 = ((int32_t)(((int64_t)UINT64_C(8))));
  if (!(((t379) < (t380)))) goto label23;
  {
  t381 = ((int32_t)(v51));
  t382 = ((int32_t)(((int64_t)UINT64_C(2))));
  t383 = ((int32_t)(((int32_t)gg_div(t381, t382))));
  t384 = ((int32_t)(((int64_t)UINT64_C(2))));
  v52 = ((int32_t)(((int32_t)gg_mod(t383, t384))));
  t385 = gg_view((v30).data, 16);
  t386 = ((int32_t)(v29));
  t387 = ((int32_t)(((int64_t)UINT64_C(8))));
  t388 = ((int32_t)(((int32_t)((uint64_t)(t386) * (uint64_t)(t387)))));
  t389 = ((int32_t)(v51));
  t390 = ((int32_t)((uint64_t)(t388) + (uint64_t)(t389)));
  t391 = ((float *)t385.data) + gg_index(t390, t385.len);
  t392 = ((float)((*t391)));
  /* FG_DIRECT_SLICE_INDEX */
  t393 = v52;
  t394 = ((float *)(v40).data) + gg_index(t393, ((int64_t)UINT64_C(2)));
  t395 = ((float)((*t394)));
  t396 = ((float)(((t392) - (t395))));
  t397 = fg176i_expf(t396);
  v53 = ((float)(t397));
  t398 = gg_view((v30).data, 16);
  t399 = ((int32_t)(v29));
  t400 = ((int32_t)(((int64_t)UINT64_C(8))));
  t401 = ((int32_t)(((int32_t)((uint64_t)(t399) * (uint64_t)(t400)))));
  t402 = ((int32_t)(v51));
  t403 = ((int32_t)((uint64_t)(t401) + (uint64_t)(t402)));
  t404 = ((float *)t398.data) + gg_index(t403, t398.len);
  (*t404) = ((float)(v53));
  /* FG_DIRECT_SLICE_INDEX */
  t405 = v52;
  t406 = ((float *)(v41).data) + gg_index(t405, ((int64_t)UINT64_C(2)));
  t407 = (*t406);
  t408 = ((float)(v53));
  (*t406) = ((t407) + (t408));
  t409 = v51;
  t410 = ((int32_t)(((int64_t)UINT64_C(1))));
  v51 = ((int32_t)((uint64_t)(t409) + (uint64_t)(t410)));
  }
  goto label22;
label23:;
  t411 = v29;
  t412 = ((int32_t)(((int64_t)UINT64_C(1))));
  v29 = ((int32_t)((uint64_t)(t411) + (uint64_t)(t412)));
  }
  goto label19;
label20:;
  memset(&t413, 0, sizeof(t413));
  t414 = ((float)(t413));
  for (t415=0; t415<2; t415++) (v54).data[t415] = t414;
  v49 = ((int32_t)(((int64_t)UINT64_C(0))));
label25:;
  t416 = ((int32_t)(v49));
  t417 = ((int32_t)(((int64_t)UINT64_C(2))));
  if (!(((t416) < (t417)))) goto label26;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t418 = v49;
  t419 = ((float *)(v39).data) + gg_index(t418, ((int64_t)UINT64_C(2)));
  t420 = ((float)((*t419)));
  /* FG_DIRECT_SLICE_INDEX */
  t421 = v49;
  t422 = ((float *)(v40).data) + gg_index(t421, ((int64_t)UINT64_C(2)));
  t423 = ((float)((*t422)));
  v55 = ((float)(((t420) - (t423))));
  t424 = ((float)(v55));
  t425 = fg176i_expf(t424);
  v56 = ((float)(t425));
  t426 = ((float)(v55));
  t427 = gg_double_bits(UINT64_C(4626322717216342016));
  t428 = ((float)((-(t427))));
  t429 = ((t426) >= (t428));
  if (t429) {
  {
  v57 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  v57 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t430 = v49;
  t431 = ((float *)(v54).data) + gg_index(t430, ((int64_t)UINT64_C(2)));
  t432 = ((float)(v56));
  t433 = fg176i_bits(t432);
  t434 = ((uint32_t)(t433));
  t435 = ((uint32_t)(v57));
  t436 = ((uint32_t)(((uint32_t)((uint64_t)(t434) * (uint64_t)(t435)))));
  t437 = fg176i_frombits(t436);
  (*t431) = ((float)(t437));
  t438 = ((uint64_t)(v13));
  t439 = ((int32_t)(v49));
  t440 = ((int32_t)(((int64_t)UINT64_C(4))));
  t441 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t439) * (uint64_t)(t440)))))));
  t442 = ((uint64_t)(((uint64_t)((uint64_t)(t438) + (uint64_t)(t441)))));
  /* FG_DIRECT_SLICE_INDEX */
  t443 = v49;
  t444 = ((float *)(v40).data) + gg_index(t443, ((int64_t)UINT64_C(2)));
  t445 = ((float)((*t444)));
  t446 = fg176i_bits(t445);
  t447 = ((uint32_t)(t446));
  fg176i_stu(t442, t447);
  t448 = ((uint64_t)(v14));
  t449 = ((int32_t)(v49));
  t450 = ((int32_t)(((int64_t)UINT64_C(4))));
  t451 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t449) * (uint64_t)(t450)))))));
  t452 = ((uint64_t)(((uint64_t)((uint64_t)(t448) + (uint64_t)(t451)))));
  t453 = fg176i_ldu(t452);
  t454 = ((uint32_t)(t453));
  t455 = fg176i_frombits(t454);
  v58 = ((float)(t455));
  t456 = ((uint64_t)(v14));
  t457 = ((int32_t)(v49));
  t458 = ((int32_t)(((int64_t)UINT64_C(4))));
  t459 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t457) * (uint64_t)(t458)))))));
  t460 = ((uint64_t)(((uint64_t)((uint64_t)(t456) + (uint64_t)(t459)))));
  /* FG_DIRECT_SLICE_INDEX */
  t461 = v49;
  t462 = ((float *)(v54).data) + gg_index(t461, ((int64_t)UINT64_C(2)));
  t463 = ((float)((*t462)));
  t464 = ((float)(v58));
  /* FG_DIRECT_SLICE_INDEX */
  t465 = v49;
  t466 = ((float *)(v41).data) + gg_index(t465, ((int64_t)UINT64_C(2)));
  t467 = ((float)((*t466)));
  t468 = fg176i_fmaf(t463, t464, t467);
  t469 = ((float)(t468));
  t470 = fg176i_bits(t469);
  t471 = ((uint32_t)(t470));
  fg176i_stu(t460, t471);
  t472 = v49;
  t473 = ((int32_t)(((int64_t)UINT64_C(1))));
  v49 = ((int32_t)((uint64_t)(t472) + (uint64_t)(t473)));
  }
  goto label25;
label26:;
  memset(&t474, 0, sizeof(t474));
  t475 = ((uint32_t)(t474));
  for (t476=0; t476<2; t476++) (v59).data[t476] = t475;
  v49 = ((int32_t)(((int64_t)UINT64_C(0))));
label28:;
  t477 = ((int32_t)(v49));
  t478 = ((int32_t)(((int64_t)UINT64_C(2))));
  if (!(((t477) < (t478)))) goto label29;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t479 = v49;
  t480 = ((uint32_t *)(v59).data) + gg_index(t479, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t481 = v49;
  t482 = ((float *)(v54).data) + gg_index(t481, ((int64_t)UINT64_C(2)));
  t483 = ((float)((*t482)));
  /* FG_DIRECT_SLICE_INDEX */
  t484 = v49;
  t485 = ((float *)(v54).data) + gg_index(t484, ((int64_t)UINT64_C(2)));
  t486 = ((float)((*t485)));
  t487 = fg176i_h2(t483, t486);
  (*t480) = ((uint32_t)(t487));
  t488 = v49;
  t489 = ((int32_t)(((int64_t)UINT64_C(1))));
  v49 = ((int32_t)((uint64_t)(t488) + (uint64_t)(t489)));
  }
  goto label28;
label29:;
  v60 = ((int32_t)(((int64_t)UINT64_C(0))));
label31:;
  t490 = ((int32_t)(v60));
  t491 = ((int32_t)(((int64_t)UINT64_C(16))));
  if (!(((t490) < (t491)))) goto label32;
  {
  v49 = ((int32_t)(((int64_t)UINT64_C(0))));
label34:;
  t492 = ((int32_t)(v49));
  t493 = ((int32_t)(((int64_t)UINT64_C(2))));
  if (!(((t492) < (t493)))) goto label35;
  {
  v61 = ((int32_t)(((int64_t)UINT64_C(0))));
label37:;
  t494 = ((int32_t)(v61));
  t495 = ((int32_t)(((int64_t)UINT64_C(2))));
  if (!(((t494) < (t495)))) goto label38;
  {
  t496 = ((int32_t)(v60));
  t497 = ((int32_t)(((int64_t)UINT64_C(4))));
  t498 = ((int32_t)(((int32_t)((uint64_t)(t496) * (uint64_t)(t497)))));
  t499 = ((int32_t)(v61));
  t500 = ((int32_t)(((int64_t)UINT64_C(2))));
  t501 = ((int32_t)(((int32_t)((uint64_t)(t499) * (uint64_t)(t500)))));
  t502 = ((int32_t)(((int32_t)((uint64_t)(t498) + (uint64_t)(t501)))));
  t503 = ((int32_t)(v49));
  v62 = ((int32_t)(((int32_t)((uint64_t)(t502) + (uint64_t)(t503)))));
  t504 = ((uint64_t)(v12));
  t505 = ((int32_t)(v62));
  t506 = ((int32_t)(((int64_t)UINT64_C(4))));
  t507 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t505) * (uint64_t)(t506)))))));
  t508 = ((uint64_t)(((uint64_t)((uint64_t)(t504) + (uint64_t)(t507)))));
  t509 = ((uint64_t)(v12));
  t510 = ((int32_t)(v62));
  t511 = ((int32_t)(((int64_t)UINT64_C(4))));
  t512 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t510) * (uint64_t)(t511)))))));
  t513 = ((uint64_t)(((uint64_t)((uint64_t)(t509) + (uint64_t)(t512)))));
  t514 = fg176i_ldu(t513);
  t515 = ((uint32_t)(t514));
  /* FG_DIRECT_SLICE_INDEX */
  t516 = v49;
  t517 = ((uint32_t *)(v59).data) + gg_index(t516, ((int64_t)UINT64_C(2)));
  t518 = ((uint32_t)((*t517)));
  t519 = fg176i_mulh2(t515, t518);
  t520 = ((uint32_t)(t519));
  fg176i_stu(t508, t520);
  t521 = v61;
  t522 = ((int32_t)(((int64_t)UINT64_C(1))));
  v61 = ((int32_t)((uint64_t)(t521) + (uint64_t)(t522)));
  }
  goto label37;
label38:;
  t523 = v49;
  t524 = ((int32_t)(((int64_t)UINT64_C(1))));
  v49 = ((int32_t)((uint64_t)(t523) + (uint64_t)(t524)));
  }
  goto label34;
label35:;
  t525 = v60;
  t526 = ((int32_t)(((int64_t)UINT64_C(1))));
  v60 = ((int32_t)((uint64_t)(t525) + (uint64_t)(t526)));
  }
  goto label31;
label32:;
  memset(&t527, 0, sizeof(t527));
  t528 = ((uint32_t)(t527));
  for (t529=0; t529<8; t529++) (v63).data[t529] = t528;
  v29 = ((int32_t)(((int64_t)UINT64_C(0))));
label40:;
  t530 = ((int32_t)(v29));
  t531 = ((int32_t)(v27));
  if (!(((t530) < (t531)))) goto label41;
  {
  v64 = ((int32_t)(((int64_t)UINT64_C(0))));
label43:;
  t532 = ((int32_t)(v64));
  t533 = ((int32_t)(((int64_t)UINT64_C(8))));
  if (!(((t532) < (t533)))) goto label44;
  {
  t534 = gg_view((v63).data, 8);
  t535 = ((int32_t)(v29));
  t536 = ((int32_t)(((int64_t)UINT64_C(4))));
  t537 = ((int32_t)(((int32_t)((uint64_t)(t535) * (uint64_t)(t536)))));
  t538 = ((int32_t)(v64));
  t539 = ((int32_t)(((int64_t)UINT64_C(2))));
  t540 = ((int32_t)(((int32_t)gg_div(t538, t539))));
  t541 = ((int32_t)((uint64_t)(t537) + (uint64_t)(t540)));
  t542 = ((uint32_t *)t534.data) + gg_index(t541, t534.len);
  t543 = gg_view((v30).data, 16);
  t544 = ((int32_t)(v29));
  t545 = ((int32_t)(((int64_t)UINT64_C(8))));
  t546 = ((int32_t)(((int32_t)((uint64_t)(t544) * (uint64_t)(t545)))));
  t547 = ((int32_t)(v64));
  t548 = ((int32_t)((uint64_t)(t546) + (uint64_t)(t547)));
  t549 = ((float *)t543.data) + gg_index(t548, t543.len);
  t550 = ((float)((*t549)));
  t551 = gg_view((v30).data, 16);
  t552 = ((int32_t)(v29));
  t553 = ((int32_t)(((int64_t)UINT64_C(8))));
  t554 = ((int32_t)(((int32_t)((uint64_t)(t552) * (uint64_t)(t553)))));
  t555 = ((int32_t)(v64));
  t556 = ((int32_t)(((int32_t)((uint64_t)(t554) + (uint64_t)(t555)))));
  t557 = ((int32_t)(((int64_t)UINT64_C(1))));
  t558 = ((int32_t)((uint64_t)(t556) + (uint64_t)(t557)));
  t559 = ((float *)t551.data) + gg_index(t558, t551.len);
  t560 = ((float)((*t559)));
  t561 = fg176i_h2(t550, t560);
  (*t542) = ((uint32_t)(t561));
  t562 = v64;
  t563 = ((int32_t)(((int64_t)UINT64_C(2))));
  v64 = ((int32_t)((uint64_t)(t562) + (uint64_t)(t563)));
  }
  goto label43;
label44:;
  t564 = v29;
  t565 = ((int32_t)(((int64_t)UINT64_C(1))));
  v29 = ((int32_t)((uint64_t)(t564) + (uint64_t)(t565)));
  }
  goto label40;
label41:;
  fg176i_cpwait();
  fg176i_barrier();
  t566 = v19;
  t567 = (!(t566));
  if (t567) {
  {
  t568 = ((uint64_t)(v2));
  t569 = ((uint64_t)(v22));
  t570 = ((int64_t)(v5));
  t571 = ((int64_t)(v23));
  t572 = ((int64_t)(((int64_t)UINT64_C(32))));
  t573 = ((int64_t)(((int64_t)((uint64_t)(t571) + (uint64_t)(t572)))));
  t574 = ((int64_t)(v24));
  t575 = ((int64_t)(v9));
  t576 = ((int32_t)(v10));
  t577 = ((int32_t)(v16));
  t578 = ((int32_t)(v17));
  fg175l_load_mask(t568, t569, t570, t573, t574, t575, t576, t577, t578);
  t579 = ((uint64_t)(v0));
  t580 = ((uint64_t)(v20));
  t581 = ((int64_t)(v3));
  t582 = ((int64_t)(v23));
  t583 = ((int64_t)(((int64_t)UINT64_C(32))));
  t584 = ((int64_t)(((int64_t)((uint64_t)(t582) + (uint64_t)(t583)))));
  t585 = ((int32_t)(v16));
  t586 = ((int32_t)(v17));
  fg175l_load_kv(t579, t580, t581, t584, t585, t586);
  }
  }
  v60 = ((int32_t)(((int64_t)UINT64_C(0))));
label46:;
  t587 = ((int32_t)(v60));
  t588 = ((int32_t)(((int64_t)UINT64_C(16))));
  if (!(((t587) < (t588)))) goto label47;
  {
  memset(&(v65), 0, sizeof(v65));
  t589 = ((uint64_t)(v12));
  t590 = ((int32_t)(v60));
  t591 = ((int32_t)(((int64_t)UINT64_C(16))));
  t592 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t590) * (uint64_t)(t591)))))));
  t593 = ((uint64_t)(((uint64_t)((uint64_t)(t589) + (uint64_t)(t592)))));
  t594 = fg176i_ldu(t593);
  (v65).f8 = ((uint32_t)(t594));
  t595 = ((uint64_t)(v12));
  t596 = ((int32_t)(v60));
  t597 = ((int32_t)(((int64_t)UINT64_C(16))));
  t598 = ((int32_t)(((int32_t)((uint64_t)(t596) * (uint64_t)(t597)))));
  t599 = ((int32_t)(((int64_t)UINT64_C(4))));
  t600 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t598) + (uint64_t)(t599)))))));
  t601 = ((uint64_t)(((uint64_t)((uint64_t)(t595) + (uint64_t)(t600)))));
  t602 = fg176i_ldu(t601);
  (v65).f9 = ((uint32_t)(t602));
  memset(&(v66), 0, sizeof(v66));
  t603 = ((uint64_t)(v12));
  t604 = ((int32_t)(v60));
  t605 = ((int32_t)(((int64_t)UINT64_C(16))));
  t606 = ((int32_t)(((int32_t)((uint64_t)(t604) * (uint64_t)(t605)))));
  t607 = ((int32_t)(((int64_t)UINT64_C(8))));
  t608 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t606) + (uint64_t)(t607)))))));
  t609 = ((uint64_t)(((uint64_t)((uint64_t)(t603) + (uint64_t)(t608)))));
  t610 = fg176i_ldu(t609);
  (v66).f8 = ((uint32_t)(t610));
  t611 = ((uint64_t)(v12));
  t612 = ((int32_t)(v60));
  t613 = ((int32_t)(((int64_t)UINT64_C(16))));
  t614 = ((int32_t)(((int32_t)((uint64_t)(t612) * (uint64_t)(t613)))));
  t615 = ((int32_t)(((int64_t)UINT64_C(12))));
  t616 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t614) + (uint64_t)(t615)))))));
  t617 = ((uint64_t)(((uint64_t)((uint64_t)(t611) + (uint64_t)(t616)))));
  t618 = fg176i_ldu(t617);
  (v66).f9 = ((uint32_t)(t618));
  v67 = ((int32_t)(((int64_t)UINT64_C(0))));
label49:;
  t619 = ((int32_t)(v67));
  t620 = ((int32_t)(((int64_t)UINT64_C(16))));
  t621 = ((int32_t)(v18));
  t622 = ((int32_t)(((int64_t)UINT64_C(8))));
  t623 = ((int32_t)(((int32_t)((uint64_t)(t621) * (uint64_t)(t622)))));
  t624 = ((int32_t)(((int32_t)gg_div(t620, t623))));
  if (!(((t619) < (t624)))) goto label50;
  {
  t625 = ((int32_t)(v67));
  t626 = ((int32_t)(v18));
  t627 = ((int32_t)(((int32_t)((uint64_t)(t625) * (uint64_t)(t626)))));
  t628 = ((int32_t)(((int64_t)UINT64_C(8))));
  t629 = ((int32_t)(((int32_t)((uint64_t)(t627) * (uint64_t)(t628)))));
  t630 = ((int32_t)(v26));
  t631 = ((int32_t)(((int64_t)UINT64_C(2))));
  t632 = ((int32_t)(((int32_t)gg_div(t630, t631))));
  v68 = ((int32_t)(((int32_t)((uint64_t)(t629) + (uint64_t)(t632)))));
  t633 = ((uint64_t)(v21));
  t634 = ((int32_t)(((int64_t)UINT64_C(2))));
  t635 = ((int32_t)(v68));
  t636 = ((int32_t)(((int32_t)((uint64_t)(t634) * (uint64_t)(t635)))));
  t637 = ((int32_t)(((int64_t)UINT64_C(128))));
  t638 = ((int32_t)(((int32_t)((uint64_t)(t636) * (uint64_t)(t637)))));
  t639 = ((int32_t)(v60));
  t640 = ((int32_t)(((int64_t)UINT64_C(8))));
  t641 = ((int32_t)(((int32_t)((uint64_t)(t639) * (uint64_t)(t640)))));
  t642 = ((int32_t)(((int32_t)((uint64_t)(t638) + (uint64_t)(t641)))));
  t643 = ((int32_t)(v16));
  t644 = gg_function_6(t633, t642, t643, base + 0);
  v69 = gg_function_23(t644, base + 0);
  t645 = v65;
  t646 = gg_view((v63).data, 8);
  t647 = ((int32_t)(v67));
  t648 = ((int32_t)(((int64_t)UINT64_C(4))));
  t649 = ((int32_t)((uint64_t)(t647) * (uint64_t)(t648)));
  t650 = ((uint32_t *)t646.data) + gg_index(t649, t646.len);
  t651 = ((uint32_t)((*t650)));
  t652 = gg_view((v63).data, 8);
  t653 = ((int32_t)(v67));
  t654 = ((int32_t)(((int64_t)UINT64_C(4))));
  t655 = ((int32_t)(((int32_t)((uint64_t)(t653) * (uint64_t)(t654)))));
  t656 = ((int32_t)(((int64_t)UINT64_C(1))));
  t657 = ((int32_t)((uint64_t)(t655) + (uint64_t)(t656)));
  t658 = ((uint32_t *)t652.data) + gg_index(t657, t652.len);
  t659 = ((uint32_t)((*t658)));
  t660 = gg_view((v63).data, 8);
  t661 = ((int32_t)(v67));
  t662 = ((int32_t)(((int64_t)UINT64_C(4))));
  t663 = ((int32_t)(((int32_t)((uint64_t)(t661) * (uint64_t)(t662)))));
  t664 = ((int32_t)(((int64_t)UINT64_C(2))));
  t665 = ((int32_t)((uint64_t)(t663) + (uint64_t)(t664)));
  t666 = ((uint32_t *)t660.data) + gg_index(t665, t660.len);
  t667 = ((uint32_t)((*t666)));
  t668 = gg_view((v63).data, 8);
  t669 = ((int32_t)(v67));
  t670 = ((int32_t)(((int64_t)UINT64_C(4))));
  t671 = ((int32_t)(((int32_t)((uint64_t)(t669) * (uint64_t)(t670)))));
  t672 = ((int32_t)(((int64_t)UINT64_C(3))));
  t673 = ((int32_t)((uint64_t)(t671) + (uint64_t)(t672)));
  t674 = ((uint32_t *)t668.data) + gg_index(t673, t668.len);
  t675 = ((uint32_t)((*t674)));
  t676 = ((uint32_t)((v69).f0));
  t677 = ((uint32_t)((v69).f2));
  t678 = fg176i_mma_f16(t645, t651, t659, t667, t675, t676, t677);
  v65 = t678;
  t679 = v66;
  t680 = gg_view((v63).data, 8);
  t681 = ((int32_t)(v67));
  t682 = ((int32_t)(((int64_t)UINT64_C(4))));
  t683 = ((int32_t)((uint64_t)(t681) * (uint64_t)(t682)));
  t684 = ((uint32_t *)t680.data) + gg_index(t683, t680.len);
  t685 = ((uint32_t)((*t684)));
  t686 = gg_view((v63).data, 8);
  t687 = ((int32_t)(v67));
  t688 = ((int32_t)(((int64_t)UINT64_C(4))));
  t689 = ((int32_t)(((int32_t)((uint64_t)(t687) * (uint64_t)(t688)))));
  t690 = ((int32_t)(((int64_t)UINT64_C(1))));
  t691 = ((int32_t)((uint64_t)(t689) + (uint64_t)(t690)));
  t692 = ((uint32_t *)t686.data) + gg_index(t691, t686.len);
  t693 = ((uint32_t)((*t692)));
  t694 = gg_view((v63).data, 8);
  t695 = ((int32_t)(v67));
  t696 = ((int32_t)(((int64_t)UINT64_C(4))));
  t697 = ((int32_t)(((int32_t)((uint64_t)(t695) * (uint64_t)(t696)))));
  t698 = ((int32_t)(((int64_t)UINT64_C(2))));
  t699 = ((int32_t)((uint64_t)(t697) + (uint64_t)(t698)));
  t700 = ((uint32_t *)t694.data) + gg_index(t699, t694.len);
  t701 = ((uint32_t)((*t700)));
  t702 = gg_view((v63).data, 8);
  t703 = ((int32_t)(v67));
  t704 = ((int32_t)(((int64_t)UINT64_C(4))));
  t705 = ((int32_t)(((int32_t)((uint64_t)(t703) * (uint64_t)(t704)))));
  t706 = ((int32_t)(((int64_t)UINT64_C(3))));
  t707 = ((int32_t)((uint64_t)(t705) + (uint64_t)(t706)));
  t708 = ((uint32_t *)t702.data) + gg_index(t707, t702.len);
  t709 = ((uint32_t)((*t708)));
  t710 = ((uint32_t)((v69).f1));
  t711 = ((uint32_t)((v69).f3));
  t712 = fg176i_mma_f16(t679, t685, t693, t701, t709, t710, t711);
  v66 = t712;
  t713 = v67;
  t714 = ((int32_t)(((int64_t)UINT64_C(1))));
  v67 = ((int32_t)((uint64_t)(t713) + (uint64_t)(t714)));
  }
  goto label49;
label50:;
  t715 = ((uint64_t)(v12));
  t716 = ((int32_t)(v60));
  t717 = ((int32_t)(((int64_t)UINT64_C(16))));
  t718 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t716) * (uint64_t)(t717)))))));
  t719 = ((uint64_t)(((uint64_t)((uint64_t)(t715) + (uint64_t)(t718)))));
  t720 = ((uint32_t)((v65).f8));
  fg176i_stu(t719, t720);
  t721 = ((uint64_t)(v12));
  t722 = ((int32_t)(v60));
  t723 = ((int32_t)(((int64_t)UINT64_C(16))));
  t724 = ((int32_t)(((int32_t)((uint64_t)(t722) * (uint64_t)(t723)))));
  t725 = ((int32_t)(((int64_t)UINT64_C(4))));
  t726 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t724) + (uint64_t)(t725)))))));
  t727 = ((uint64_t)(((uint64_t)((uint64_t)(t721) + (uint64_t)(t726)))));
  t728 = ((uint32_t)((v65).f9));
  fg176i_stu(t727, t728);
  t729 = ((uint64_t)(v12));
  t730 = ((int32_t)(v60));
  t731 = ((int32_t)(((int64_t)UINT64_C(16))));
  t732 = ((int32_t)(((int32_t)((uint64_t)(t730) * (uint64_t)(t731)))));
  t733 = ((int32_t)(((int64_t)UINT64_C(8))));
  t734 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t732) + (uint64_t)(t733)))))));
  t735 = ((uint64_t)(((uint64_t)((uint64_t)(t729) + (uint64_t)(t734)))));
  t736 = ((uint32_t)((v66).f8));
  fg176i_stu(t735, t736);
  t737 = ((uint64_t)(v12));
  t738 = ((int32_t)(v60));
  t739 = ((int32_t)(((int64_t)UINT64_C(16))));
  t740 = ((int32_t)(((int32_t)((uint64_t)(t738) * (uint64_t)(t739)))));
  t741 = ((int32_t)(((int64_t)UINT64_C(12))));
  t742 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t740) + (uint64_t)(t741)))))));
  t743 = ((uint64_t)(((uint64_t)((uint64_t)(t737) + (uint64_t)(t742)))));
  t744 = ((uint32_t)((v66).f9));
  fg176i_stu(t743, t744);
  t745 = v60;
  t746 = ((int32_t)(((int64_t)UINT64_C(1))));
  v60 = ((int32_t)((uint64_t)(t745) + (uint64_t)(t746)));
  }
  goto label46;
label47:;
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  float t6;
  int32_t t7;
  int32_t t8;
  int64_t t9;
  int32_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  uint8_t t19;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t1 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t6 = ((float)(gg_double_bits(UINT64_C(0))));
  t7 = ((int32_t)(((int64_t)UINT64_C(0))));
  t8 = ((int32_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t10 = ((int32_t)(((int64_t)UINT64_C(4))));
  t11 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t13 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t14 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t16 = ((int32_t)(((int64_t)UINT64_C(0))));
  t17 = ((int32_t)(((int64_t)UINT64_C(0))));
  t18 = ((int32_t)(((int64_t)UINT64_C(2))));
  t19 = ((uint8_t)UINT64_C(1));
  gg_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
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
