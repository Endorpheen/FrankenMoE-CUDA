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


typedef struct gg_type_1 gg_type_1;
#pragma pack(push, 1)
typedef struct { uint8_t data[12]; } gg_type_4;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[32]; } gg_type_5;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_5 data[4]; } gg_type_6;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_1 {
  uint16_t f0;
  uint16_t f1;
  gg_type_4 f2;
  gg_type_6 f3;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_1 data[1]; } gg_type_7;
#pragma pack(pop)
typedef struct gg_type_8 gg_type_8;
#pragma pack(push, 1)
typedef struct { int8_t data[32]; } gg_type_11;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_11 data[8]; } gg_type_12;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int16_t data[16]; } gg_type_14;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_8 {
  float f4;
  gg_type_12 f5;
  gg_type_14 f6;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_8 data[1]; } gg_type_15;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_20;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_21;
#pragma pack(pop)
typedef gg_type_1 *gg_type_22;
typedef gg_type_8 *gg_type_23;
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_25;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_26;
#pragma pack(pop)
typedef struct gg_type_28 gg_type_28;
#pragma pack(push, 1)
struct gg_type_28 {
  float f7;
  float f8;
};
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, gg_type_1 value);
static uint8_t gg_eq_2(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint16_t value);
static uint8_t gg_eq_3(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint8_t value);
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_type_4 value);
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_type_5 value);
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, gg_type_6 value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(float a, float b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, float value);
static uint8_t gg_eq_10(int8_t a, int8_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, int8_t value);
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, gg_type_11 value);
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_type_12 value);
static uint8_t gg_eq_13(int16_t a, int16_t b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, int16_t value);
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, gg_type_14 value);
static uint8_t gg_eq_15(gg_type_15 a, gg_type_15 b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, gg_type_15 value);
static uint8_t gg_eq_16(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, gg_slice value);
static uint8_t gg_eq_17(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_slice value);
static uint8_t gg_eq_18(int64_t a, int64_t b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, int64_t value);
static uint8_t gg_eq_19(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, uint8_t value);
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
static uint8_t gg_eq_24(int32_t a, int32_t b);
static uint8_t gg_equal_elements_24(gg_slice a, gg_slice b);
static void gg_render_24(gg_reference out, int32_t value);
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b);
static uint8_t gg_equal_elements_25(gg_slice a, gg_slice b);
static void gg_render_25(gg_reference out, gg_type_25 value);
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b);
static uint8_t gg_equal_elements_26(gg_slice a, gg_slice b);
static void gg_render_26(gg_reference out, gg_type_26 value);
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b);
static uint8_t gg_equal_elements_28(gg_slice a, gg_slice b);
static void gg_render_28(gg_reference out, gg_type_28 value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((gg_type_1*)a.data)[i],((gg_type_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b) {
if(!gg_eq_2(a.f0,b.f0))return 0;
if(!gg_eq_2(a.f1,b.f1))return 0;
if(!gg_eq_4(a.f2,b.f2))return 0;
if(!gg_eq_6(a.f3,b.f3))return 0;
return 1;
}
static void gg_render_1(gg_reference out, gg_type_1 value) {
gg_append(out,"\121\064\102\154\157\143\153\040\173\040",10,1);
gg_render_2(out, value.f0);
gg_append(out,", ",2,1);
gg_render_2(out, value.f1);
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f2).data, 12));
gg_append(out,", ",2,1);
gg_render_6(out, value.f3);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_type_4*)a.data)[i],((gg_type_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b) {
return gg_equal_elements_3(gg_view((a).data, 12),gg_view((b).data, 12));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
gg_text(out,gg_view((value).data, 12));
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_type_5*)a.data)[i],((gg_type_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
return gg_equal_elements_3(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
gg_text(out,gg_view((value).data, 32));
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_type_6*)a.data)[i],((gg_type_6*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b) {
return gg_equal_elements_5(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_6(gg_reference out, gg_type_6 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_5*)view.data)[i]).data, 32));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_1(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((gg_type_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
if(!gg_eq_9(a.f4,b.f4))return 0;
if(!gg_eq_12(a.f5,b.f5))return 0;
if(!gg_eq_14(a.f6,b.f6))return 0;
return 1;
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
gg_append(out,"\121\070\102\154\157\143\153\040\173\040",10,1);
gg_render_9(out, value.f4);
gg_append(out,", ",2,1);
gg_render_12(out, value.f5);
gg_append(out,", ",2,1);
gg_render_14(out, value.f6);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(float a, float b) {
return a == b;
}
static void gg_render_9(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_10(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_type_11*)a.data)[i],((gg_type_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b) {
return gg_equal_elements_10(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_11(gg_reference out, gg_type_11 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return gg_equal_elements_11(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_11(out, ((gg_type_11*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_13(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_type_14*)a.data)[i],((gg_type_14*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b) {
return gg_equal_elements_13(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_14(gg_reference out, gg_type_14 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_13(out, ((int16_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((gg_type_15*)a.data)[i],((gg_type_15*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(gg_type_15 a, gg_type_15 b) {
return gg_equal_elements_8(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_15(gg_reference out, gg_type_15 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((gg_type_8*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_16(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((gg_type_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_17(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((gg_type_8*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_18(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_19(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((gg_type_20*)a.data)[i],((gg_type_20*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b) {
return gg_equal_elements_9(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_20(gg_reference out, gg_type_20 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_9(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((gg_type_21*)a.data)[i],((gg_type_21*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b) {
return gg_equal_elements_9(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_21(gg_reference out, gg_type_21 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_9(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((gg_type_22*)a.data)[i],((gg_type_22*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b) {
return a == b;
}
static void gg_render_22(gg_reference out, gg_type_22 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((gg_type_23*)a.data)[i],((gg_type_23*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b) {
return a == b;
}
static void gg_render_23(gg_reference out, gg_type_23 value) {
gg_render_8(out,*value);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_24(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((gg_type_25*)a.data)[i],((gg_type_25*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b) {
return gg_equal_elements_24(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_25(gg_reference out, gg_type_25 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_24(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((gg_type_26*)a.data)[i],((gg_type_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b) {
return gg_equal_elements_24(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_26(gg_reference out, gg_type_26 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_24(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_type_28*)a.data)[i],((gg_type_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
if(!gg_eq_9(a.f7,b.f7))return 0;
if(!gg_eq_9(a.f8,b.f8))return 0;
return 1;
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
gg_append(out,"\104\157\164\120\141\151\162\040\173\040",10,1);
gg_render_9(out, value.f7);
gg_append(out,", ",2,1);
gg_render_9(out, value.f8);
gg_append(out," }",2,1);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static float gg_function_1(gg_slice p0, gg_slice p1, int64_t base);
static gg_type_25 gg_function_3(gg_type_14 p44, int64_t base);
static gg_type_25 gg_function_4(gg_type_25 p50, gg_type_25 p51, int64_t base);
static gg_type_26 gg_function_5(gg_type_25 p57, int64_t base);
static gg_type_21 gg_function_6(float p63, gg_type_26 p64, gg_type_21 p65, int64_t base);
static gg_type_5 gg_function_8(gg_type_5 p78, uint8_t p79, int64_t base);
static gg_type_5 gg_function_9(gg_type_5 p86, uint8_t p87, int64_t base);
static gg_type_14 gg_function_10(int16_t p94, int64_t base);
static gg_type_14 gg_function_11(gg_type_5 p103, gg_type_11 p104, int64_t base);
static gg_type_25 gg_function_12(gg_type_14 p111, gg_type_14 p112, int64_t base);
static gg_type_25 gg_function_13(gg_type_25 p122, gg_type_25 p123, int64_t base);
static gg_type_20 gg_function_14(float p131, gg_type_25 p132, gg_type_20 p133, int64_t base);
/* FG_PACKED_MATCH:fma32 */
static gg_type_20 gg_function_14(float p131, gg_type_25 p132, gg_type_20 p133, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__) && defined(__FMA__)
  { gg_type_20 packed_result;
    _mm256_storeu_ps(packed_result.data, _mm256_fmadd_ps(_mm256_set1_ps(p131), _mm256_cvtepi32_ps(_mm256_loadu_si256((const __m256i*)p132.data)), _mm256_loadu_ps(p133.data)));
    return packed_result;
  }
#endif
  gg_type_20 t0;
  float t1;
  float t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  float * t8;
  float t9;
  gg_slice t10;
  int64_t t11;
  int32_t * t12;
  float t13;
  gg_slice t14;
  int64_t t15;
  float * t16;
  float t17;
  float t18;
  float v131;
  gg_type_25 v132;
  gg_type_20 v133;
  gg_type_20 v134;
  int64_t v136;
  v131 = p131;
  v132 = p132;
  v133 = p133;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v134).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v136 = t4;
  {
  t6 = gg_view((v134).data, 8);
  t7 = v136;
  t8 = ((float *)t6.data) + gg_index(t7, t6.len);
  t9 = ((float)(v131));
  t10 = gg_view((v132).data, 8);
  t11 = v136;
  t12 = ((int32_t *)t10.data) + gg_index(t11, t10.len);
  t13 = ((float)(((float)((*t12)))));
  t14 = gg_view((v133).data, 8);
  t15 = v136;
  t16 = ((float *)t14.data) + gg_index(t15, t14.len);
  t17 = ((float)((*t16)));
  t18 = fmaf(t9, t13, t17);
  (*t8) = ((float)(t18));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v134;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:add32 */
static gg_type_25 gg_function_13(gg_type_25 p122, gg_type_25 p123, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_25 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)p122.data), _mm256_loadu_si256((const __m256i*)p123.data)));
    return packed_result;
  }
#endif
  gg_type_25 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int32_t * t8;
  gg_slice t9;
  int64_t t10;
  int32_t * t11;
  int32_t t12;
  gg_slice t13;
  int64_t t14;
  int32_t * t15;
  int32_t t16;
  gg_type_25 v122;
  gg_type_25 v123;
  gg_type_25 v124;
  int64_t v126;
  v122 = p122;
  v123 = p123;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v124).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v126 = t4;
  {
  t6 = gg_view((v124).data, 8);
  t7 = v126;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v122).data, 8);
  t10 = v126;
  t11 = ((int32_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int32_t)((*t11)));
  t13 = gg_view((v123).data, 8);
  t14 = v126;
  t15 = ((int32_t *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int32_t)((*t15)));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t12) + (uint64_t)(t16)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v124;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:madd16 */
static gg_type_25 gg_function_12(gg_type_14 p111, gg_type_14 p112, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_25 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_madd_epi16(_mm256_loadu_si256((const __m256i*)p111.data), _mm256_loadu_si256((const __m256i*)p112.data)));
    return packed_result;
  }
#endif
  gg_type_25 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int16_t * t10;
  int64_t t11;
  gg_slice t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int16_t * t16;
  int64_t t17;
  int64_t t18;
  gg_slice t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int16_t * t25;
  int64_t t26;
  gg_slice t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int16_t * t33;
  int64_t t34;
  int64_t t35;
  gg_slice t36;
  int64_t t37;
  int32_t * t38;
  gg_type_14 v111;
  gg_type_14 v112;
  gg_type_25 v113;
  int64_t v115;
  int64_t v116;
  v111 = p111;
  v112 = p112;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v113).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v115 = t4;
  {
  t6 = gg_view((v111).data, 16);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v115));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((int16_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int64_t)(((int64_t)((*t10)))));
  t12 = gg_view((v112).data, 16);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v115));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((int16_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int64_t)(((int64_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v111).data, 16);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v115));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((int16_t *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int64_t)(((int64_t)((*t25)))));
  t27 = gg_view((v112).data, 16);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v115));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((int16_t *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int64_t)(((int64_t)((*t33)))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v116 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  t36 = gg_view((v113).data, 8);
  t37 = v115;
  t38 = ((int32_t *)t36.data) + gg_index(t37, t36.len);
  (*t38) = ((int32_t)(((int32_t)(v116))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v113;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:maddubs */
static gg_type_14 gg_function_11(gg_type_5 p103, gg_type_11 p104, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_14 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_maddubs_epi16(_mm256_loadu_si256((const __m256i*)p103.data), _mm256_loadu_si256((const __m256i*)p104.data)));
    return packed_result;
  }
#endif
  gg_type_14 t0;
  int16_t t1;
  int16_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t * t10;
  int32_t t11;
  gg_slice t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int8_t * t16;
  int32_t t17;
  int32_t t18;
  gg_slice t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t * t25;
  int32_t t26;
  gg_slice t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int8_t * t33;
  int32_t t34;
  int32_t t35;
  gg_slice t36;
  int64_t t37;
  int16_t * t38;
  int64_t t39;
  int32_t t40;
  int64_t t41;
  int32_t t42;
  uint8_t t43;
  int64_t t44;
  int32_t t45;
  int32_t t46;
  uint8_t t47;
  gg_type_5 v103;
  gg_type_11 v104;
  gg_type_14 v105;
  int64_t v107;
  int32_t v108;
  v103 = p103;
  v104 = p104;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v105).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v107 = t4;
  {
  t6 = gg_view((v103).data, 32);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v107));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((uint8_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int32_t)(((int32_t)((*t10)))));
  t12 = gg_view((v104).data, 32);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v107));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((int8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int32_t)(((int32_t)((*t16)))));
  t18 = ((int32_t)(((int32_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v103).data, 32);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v107));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((uint8_t *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int32_t)(((int32_t)((*t25)))));
  t27 = gg_view((v104).data, 32);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v107));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((int8_t *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int32_t)(((int32_t)((*t33)))));
  t35 = ((int32_t)(((int32_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v108 = ((int32_t)(((int32_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  t36 = gg_view((v105).data, 16);
  t37 = v107;
  t38 = ((int16_t *)t36.data) + gg_index(t37, t36.len);
  t40 = ((int32_t)(v108));
  t41 = ((int64_t)UINT64_C(32768));
  t42 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t41)))));
  t43 = ((t40) < (t42));
  if (t43) {
  {
  t44 = ((int64_t)UINT64_C(32768));
  t39 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t44)))));
  }
  } else {
  t45 = ((int32_t)(v108));
  t46 = ((int32_t)(((int64_t)UINT64_C(32767))));
  t47 = ((t45) > (t46));
  if (t47) {
  {
  t39 = ((int64_t)(((int64_t)UINT64_C(32767))));
  }
  } else {
  {
  t39 = ((int64_t)(v108));
  }
  }
  }
  (*t38) = ((int16_t)(((int16_t)(t39))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v105;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:splat16 */
static gg_type_14 gg_function_10(int16_t p94, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_14 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_set1_epi16(p94));
    return packed_result;
  }
#endif
  gg_type_14 t0;
  int16_t t1;
  int16_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int16_t * t8;
  int16_t v94;
  gg_type_14 v95;
  int64_t v97;
  v94 = p94;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v95).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v97 = t4;
  {
  t6 = gg_view((v95).data, 16);
  t7 = v97;
  t8 = ((int16_t *)t6.data) + gg_index(t7, t6.len);
  (*t8) = ((int16_t)(v94));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v95;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:shr8 */
static gg_type_5 gg_function_9(gg_type_5 p86, uint8_t p87, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_5 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)p86.data), _mm_cvtsi32_si128((p87 & 7))), _mm256_set1_epi8((unsigned char)(255u >> (p87 & 7)))));
    return packed_result;
  }
#endif
  gg_type_5 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  uint8_t * t8;
  gg_slice t9;
  int64_t t10;
  uint8_t * t11;
  uint8_t t12;
  uint8_t t13;
  gg_type_5 v86;
  uint8_t v87;
  gg_type_5 v88;
  int64_t v90;
  v86 = p86;
  v87 = p87;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v88).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v90 = t4;
  {
  t6 = gg_view((v88).data, 32);
  t7 = v90;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v86).data, 32);
  t10 = v90;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v87));
  (*t8) = ((uint8_t)(((t12) >> ((uint64_t)(t13) & 7))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v88;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:and8 */
static gg_type_5 gg_function_8(gg_type_5 p78, uint8_t p79, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_5 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_loadu_si256((const __m256i*)p78.data), _mm256_set1_epi8(p79)));
    return packed_result;
  }
#endif
  gg_type_5 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  uint8_t * t8;
  gg_slice t9;
  int64_t t10;
  uint8_t * t11;
  uint8_t t12;
  uint8_t t13;
  gg_type_5 v78;
  uint8_t v79;
  gg_type_5 v80;
  int64_t v82;
  v78 = p78;
  v79 = p79;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v80).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v82 = t4;
  {
  t6 = gg_view((v80).data, 32);
  t7 = v82;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v78).data, 32);
  t10 = v82;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v79));
  (*t8) = ((uint8_t)(((t12) & (t13))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v80;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:fma4 */
static gg_type_21 gg_function_6(float p63, gg_type_26 p64, gg_type_21 p65, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__) && defined(__FMA__)
  { gg_type_21 packed_result;
    _mm_storeu_ps(packed_result.data, _mm_fmadd_ps(_mm_set1_ps(p63), _mm_cvtepi32_ps(_mm_loadu_si128((const __m128i*)p64.data)), _mm_loadu_ps(p65.data)));
    return packed_result;
  }
#endif
  gg_type_21 t0;
  float t1;
  float t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  float * t8;
  float t9;
  gg_slice t10;
  int64_t t11;
  int32_t * t12;
  float t13;
  gg_slice t14;
  int64_t t15;
  float * t16;
  float t17;
  float t18;
  float v63;
  gg_type_26 v64;
  gg_type_21 v65;
  gg_type_21 v66;
  int64_t v68;
  v63 = p63;
  v64 = p64;
  v65 = p65;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<4; t3++) (v66).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(4));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v68 = t4;
  {
  t6 = gg_view((v66).data, 4);
  t7 = v68;
  t8 = ((float *)t6.data) + gg_index(t7, t6.len);
  t9 = ((float)(v63));
  t10 = gg_view((v64).data, 4);
  t11 = v68;
  t12 = ((int32_t *)t10.data) + gg_index(t11, t10.len);
  t13 = ((float)(((float)((*t12)))));
  t14 = gg_view((v65).data, 4);
  t15 = v68;
  t16 = ((float *)t14.data) + gg_index(t15, t14.len);
  t17 = ((float)((*t16)));
  t18 = fmaf(t9, t13, t17);
  (*t8) = ((float)(t18));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v66;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:pair_sum32 */
static gg_type_26 gg_function_5(gg_type_25 p57, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_26 packed_result;
    _mm_storeu_si128((__m128i*)packed_result.data, _mm_hadd_epi32(_mm256_castsi256_si128(_mm256_loadu_si256((const __m256i*)p57.data)), _mm256_extracti128_si256(_mm256_loadu_si256((const __m256i*)p57.data), 1)));
    return packed_result;
  }
#endif
  gg_type_26 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int32_t * t8;
  gg_slice t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int32_t * t13;
  int32_t t14;
  gg_slice t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int32_t * t21;
  int32_t t22;
  gg_type_25 v57;
  gg_type_26 v58;
  int64_t v60;
  v57 = p57;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<4; t3++) (v58).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(4));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v60 = t4;
  {
  t6 = gg_view((v58).data, 4);
  t7 = v60;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v57).data, 8);
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(v60));
  t12 = ((int64_t)((uint64_t)(t10) * (uint64_t)(t11)));
  t13 = ((int32_t *)t9.data) + gg_index(t12, t9.len);
  t14 = ((int32_t)((*t13)));
  t15 = gg_view((v57).data, 8);
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((int64_t)(v60));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  t21 = ((int32_t *)t15.data) + gg_index(t20, t15.len);
  t22 = ((int32_t)((*t21)));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t14) + (uint64_t)(t22)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v58;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:mul32 */
static gg_type_25 gg_function_4(gg_type_25 p50, gg_type_25 p51, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_25 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_mullo_epi32(_mm256_loadu_si256((const __m256i*)p50.data), _mm256_loadu_si256((const __m256i*)p51.data)));
    return packed_result;
  }
#endif
  gg_type_25 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int32_t * t8;
  gg_slice t9;
  int64_t t10;
  int32_t * t11;
  int32_t t12;
  gg_slice t13;
  int64_t t14;
  int32_t * t15;
  int32_t t16;
  gg_type_25 v50;
  gg_type_25 v51;
  gg_type_25 v52;
  int64_t v54;
  v50 = p50;
  v51 = p51;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v52).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v54 = t4;
  {
  t6 = gg_view((v52).data, 8);
  t7 = v54;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v50).data, 8);
  t10 = v54;
  t11 = ((int32_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int32_t)((*t11)));
  t13 = gg_view((v51).data, 8);
  t14 = v54;
  t15 = ((int32_t *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int32_t)((*t15)));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t12) * (uint64_t)(t16)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v52;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:pair_sum16 */
static gg_type_25 gg_function_3(gg_type_14 p44, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_25 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_madd_epi16(_mm256_loadu_si256((const __m256i*)p44.data), _mm256_set1_epi16(1)));
    return packed_result;
  }
#endif
  gg_type_25 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int32_t * t8;
  gg_slice t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int16_t * t13;
  int32_t t14;
  gg_slice t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int16_t * t21;
  int32_t t22;
  gg_type_14 v44;
  gg_type_25 v45;
  int64_t v47;
  v44 = p44;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v45).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v47 = t4;
  {
  t6 = gg_view((v45).data, 8);
  t7 = v47;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v44).data, 16);
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(v47));
  t12 = ((int64_t)((uint64_t)(t10) * (uint64_t)(t11)));
  t13 = ((int16_t *)t9.data) + gg_index(t12, t9.len);
  t14 = ((int32_t)(((int32_t)((*t13)))));
  t15 = gg_view((v44).data, 16);
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((int64_t)(v47));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  t21 = ((int16_t *)t15.data) + gg_index(t20, t15.len);
  t22 = ((int32_t)(((int32_t)((*t21)))));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t14) + (uint64_t)(t22)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v45;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_1(gg_slice p0, gg_slice p1, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  float t3;
  float t4;
  int64_t t5;
  float t6;
  float t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  gg_type_1 * t12;
  int64_t t13;
  gg_type_8 * t14;
  uint16_t t15;
  float t16;
  uint16_t t17;
  float t18;
  float t19;
  float t20;
  float t21;
  float t22;
  float t23;
  int32_t t24;
  int32_t t25;
  int64_t t26;
  int32_t t27;
  int32_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int32_t * t33;
  int64_t t34;
  uint8_t * t35;
  uint8_t t36;
  uint8_t t37;
  int64_t t38;
  int32_t * t39;
  gg_slice t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  uint8_t * t44;
  uint8_t t45;
  uint8_t t46;
  gg_slice t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int32_t * t51;
  gg_slice t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t * t56;
  uint8_t t57;
  uint8_t t58;
  uint8_t t59;
  int64_t t60;
  uint8_t * t61;
  uint8_t t62;
  int64_t t63;
  uint8_t t64;
  int64_t t65;
  uint8_t t66;
  gg_slice t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int32_t * t71;
  gg_slice t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint8_t * t76;
  uint8_t t77;
  int64_t t78;
  uint8_t t79;
  gg_slice t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint8_t * t84;
  uint8_t t85;
  int64_t t86;
  uint8_t t87;
  int64_t t88;
  uint8_t t89;
  float t90;
  gg_type_25 t91;
  gg_type_14 t92;
  gg_type_25 t93;
  gg_type_25 t94;
  gg_type_26 t95;
  gg_type_21 t96;
  int32_t t97;
  int32_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  gg_type_5 * t103;
  gg_type_5 t104;
  uint8_t t105;
  int64_t t106;
  gg_type_5 * t107;
  gg_type_5 t108;
  uint8_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int32_t * t114;
  int16_t t115;
  gg_slice t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int32_t * t122;
  int16_t t123;
  gg_type_14 t124;
  gg_type_5 t125;
  gg_slice t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  gg_type_11 * t130;
  gg_type_11 t131;
  gg_type_14 t132;
  gg_type_14 t133;
  gg_type_5 t134;
  gg_slice t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  gg_type_11 * t141;
  gg_type_11 t142;
  gg_type_14 t143;
  gg_type_25 t144;
  gg_type_25 t145;
  gg_type_25 t146;
  gg_type_25 t147;
  float t148;
  gg_type_25 t149;
  gg_type_20 t150;
  int64_t t151;
  float * t152;
  float t153;
  int64_t t154;
  float * t155;
  float t156;
  float t157;
  int64_t t158;
  float * t159;
  float t160;
  int64_t t161;
  float * t162;
  float t163;
  float t164;
  float t165;
  int64_t t166;
  float * t167;
  float t168;
  int64_t t169;
  float * t170;
  float t171;
  float t172;
  int64_t t173;
  float * t174;
  float t175;
  int64_t t176;
  float * t177;
  float t178;
  float t179;
  float t180;
  float t181;
  int64_t t182;
  float * t183;
  float t184;
  int64_t t185;
  float * t186;
  float t187;
  float t188;
  int64_t t189;
  float * t190;
  float t191;
  int64_t t192;
  float * t193;
  float t194;
  float t195;
  float t196;
  gg_slice v0;
  gg_slice v1;
  gg_type_20 v2;
  gg_type_21 v3;
  int64_t v4;
  gg_type_22 v5;
  gg_type_23 v6;
  float v7;
  float v8;
  float v9;
  float v10;
  gg_type_25 v11;
  gg_type_25 v12;
  int64_t v13;
  gg_type_25 v14;
  int64_t v15;
  gg_type_5 v16;
  gg_type_5 v17;
  gg_type_14 v18;
  gg_type_14 v19;
  gg_type_25 v20;
  gg_type_25 v21;
  float v22;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)((v1).len));
  t2 = ((int64_t)((v0).len));
  if (!(((t1) >= (t2)))) gg_fail("assertion failed");
  memset(&t3, 0, sizeof(t3));
  t4 = ((float)(t3));
  for (t5=0; t5<8; t5++) (v2).data[t5] = t4;
  memset(&t6, 0, sizeof(t6));
  t7 = ((float)(t6));
  for (t8=0; t8<4; t8++) (v3).data[t8] = t7;
  t10 = (v0).len;
  t9 = 0;
label0:;
  if (t9 >= t10) goto label2;
  v4 = t9;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v4;
  t12 = ((gg_type_1 *)(v0).data) + gg_index(t11, (v0).len);
  v5 = t12;
  /* FG_DIRECT_SLICE_INDEX */
  t13 = v4;
  t14 = ((gg_type_8 *)(v1).data) + gg_index(t13, (v1).len);
  v6 = t14;
  t15 = ((uint16_t)(((*(v5))).f0));
  t16 = fg_half_to_float(t15);
  v7 = ((float)(t16));
  t17 = ((uint16_t)(((*(v5))).f1));
  t18 = fg_half_to_float(t17);
  v8 = ((float)(t18));
  t19 = ((float)(((*(v6))).f4));
  t20 = ((float)(v7));
  v9 = ((float)(((t19) * (t20))));
  t21 = ((*(v6))).f4;
  t22 = ((float)((-(t21))));
  t23 = ((float)(v8));
  v10 = ((float)(((t22) * (t23))));
  memset(&t24, 0, sizeof(t24));
  t25 = ((int32_t)(t24));
  for (t26=0; t26<8; t26++) (v11).data[t26] = t25;
  memset(&t27, 0, sizeof(t27));
  t28 = ((int32_t)(t27));
  for (t29=0; t29<8; t29++) (v12).data[t29] = t28;
  t31 = ((int64_t)UINT64_C(4));
  t30 = 0;
label3:;
  if (t30 >= t31) goto label5;
  v13 = t30;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t32 = v13;
  t33 = ((int32_t *)(v11).data) + gg_index(t32, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t34 = v13;
  t35 = ((uint8_t *)(((*(v5))).f2).data) + gg_index(t34, ((int64_t)UINT64_C(12)));
  t36 = ((uint8_t)((*t35)));
  t37 = ((uint8_t)(((int64_t)UINT64_C(63))));
  (*t33) = ((int32_t)(((int32_t)(((t36) & (t37))))));
  /* FG_DIRECT_SLICE_INDEX */
  t38 = v13;
  t39 = ((int32_t *)(v12).data) + gg_index(t38, ((int64_t)UINT64_C(8)));
  t40 = gg_view((((*(v5))).f2).data, 12);
  t41 = ((int64_t)(v13));
  t42 = ((int64_t)(((int64_t)UINT64_C(4))));
  t43 = ((int64_t)((uint64_t)(t41) + (uint64_t)(t42)));
  t44 = ((uint8_t *)t40.data) + gg_index(t43, t40.len);
  t45 = ((uint8_t)((*t44)));
  t46 = ((uint8_t)(((int64_t)UINT64_C(63))));
  (*t39) = ((int32_t)(((int32_t)(((t45) & (t46))))));
  t47 = gg_view((v11).data, 8);
  t48 = ((int64_t)(v13));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)((uint64_t)(t48) + (uint64_t)(t49)));
  t51 = ((int32_t *)t47.data) + gg_index(t50, t47.len);
  t52 = gg_view((((*(v5))).f2).data, 12);
  t53 = ((int64_t)(v13));
  t54 = ((int64_t)(((int64_t)UINT64_C(8))));
  t55 = ((int64_t)((uint64_t)(t53) + (uint64_t)(t54)));
  t56 = ((uint8_t *)t52.data) + gg_index(t55, t52.len);
  t57 = ((uint8_t)((*t56)));
  t58 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t59 = ((uint8_t)(((t57) & (t58))));
  /* FG_DIRECT_SLICE_INDEX */
  t60 = v13;
  t61 = ((uint8_t *)(((*(v5))).f2).data) + gg_index(t60, ((int64_t)UINT64_C(12)));
  t62 = ((uint8_t)((*t61)));
  t63 = ((int64_t)(((int64_t)UINT64_C(6))));
  t64 = ((uint8_t)(((t62) >> ((uint64_t)(t63) & 7))));
  t65 = ((int64_t)(((int64_t)UINT64_C(4))));
  t66 = ((uint8_t)(((uint8_t)((uint64_t)(t64) << ((uint64_t)(t65) & 7)))));
  (*t51) = ((int32_t)(((int32_t)(((t59) | (t66))))));
  t67 = gg_view((v12).data, 8);
  t68 = ((int64_t)(v13));
  t69 = ((int64_t)(((int64_t)UINT64_C(4))));
  t70 = ((int64_t)((uint64_t)(t68) + (uint64_t)(t69)));
  t71 = ((int32_t *)t67.data) + gg_index(t70, t67.len);
  t72 = gg_view((((*(v5))).f2).data, 12);
  t73 = ((int64_t)(v13));
  t74 = ((int64_t)(((int64_t)UINT64_C(8))));
  t75 = ((int64_t)((uint64_t)(t73) + (uint64_t)(t74)));
  t76 = ((uint8_t *)t72.data) + gg_index(t75, t72.len);
  t77 = ((uint8_t)((*t76)));
  t78 = ((int64_t)(((int64_t)UINT64_C(4))));
  t79 = ((uint8_t)(((t77) >> ((uint64_t)(t78) & 7))));
  t80 = gg_view((((*(v5))).f2).data, 12);
  t81 = ((int64_t)(v13));
  t82 = ((int64_t)(((int64_t)UINT64_C(4))));
  t83 = ((int64_t)((uint64_t)(t81) + (uint64_t)(t82)));
  t84 = ((uint8_t *)t80.data) + gg_index(t83, t80.len);
  t85 = ((uint8_t)((*t84)));
  t86 = ((int64_t)(((int64_t)UINT64_C(6))));
  t87 = ((uint8_t)(((t85) >> ((uint64_t)(t86) & 7))));
  t88 = ((int64_t)(((int64_t)UINT64_C(4))));
  t89 = ((uint8_t)(((uint8_t)((uint64_t)(t87) << ((uint64_t)(t88) & 7)))));
  (*t71) = ((int32_t)(((int32_t)(((t79) | (t89))))));
  }
label4:;
  t30++; goto label3;
label5:;
  t90 = ((float)(v10));
  t91 = v12;
  t92 = ((*(v6))).f6;
  t93 = gg_function_3(t92, base + 0);
  t94 = gg_function_4(t91, t93, base + 0);
  t95 = gg_function_5(t94, base + 0);
  t96 = v3;
  v3 = gg_function_6(t90, t95, t96, base + 0);
  memset(&t97, 0, sizeof(t97));
  t98 = ((int32_t)(t97));
  for (t99=0; t99<8; t99++) (v14).data[t99] = t98;
  t101 = ((int64_t)UINT64_C(4));
  t100 = 0;
label6:;
  if (t100 >= t101) goto label8;
  v15 = t100;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t102 = v15;
  t103 = ((gg_type_5 *)(((*(v5))).f3).data) + gg_index(t102, ((int64_t)UINT64_C(4)));
  t104 = (*t103);
  t105 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v16 = gg_function_8(t104, t105, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t106 = v15;
  t107 = ((gg_type_5 *)(((*(v5))).f3).data) + gg_index(t106, ((int64_t)UINT64_C(4)));
  t108 = (*t107);
  t109 = ((uint8_t)(((int64_t)UINT64_C(4))));
  v17 = gg_function_9(t108, t109, base + 0);
  t110 = gg_view((v11).data, 8);
  t111 = ((int64_t)(((int64_t)UINT64_C(2))));
  t112 = ((int64_t)(v15));
  t113 = ((int64_t)((uint64_t)(t111) * (uint64_t)(t112)));
  t114 = ((int32_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((int16_t)(((int16_t)((*t114)))));
  v18 = gg_function_10(t115, base + 0);
  t116 = gg_view((v11).data, 8);
  t117 = ((int64_t)(((int64_t)UINT64_C(2))));
  t118 = ((int64_t)(v15));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) * (uint64_t)(t118)))));
  t120 = ((int64_t)(((int64_t)UINT64_C(1))));
  t121 = ((int64_t)((uint64_t)(t119) + (uint64_t)(t120)));
  t122 = ((int32_t *)t116.data) + gg_index(t121, t116.len);
  t123 = ((int16_t)(((int16_t)((*t122)))));
  v19 = gg_function_10(t123, base + 0);
  t124 = v18;
  t125 = v16;
  t126 = gg_view((((*(v6))).f5).data, 8);
  t127 = ((int64_t)(((int64_t)UINT64_C(2))));
  t128 = ((int64_t)(v15));
  t129 = ((int64_t)((uint64_t)(t127) * (uint64_t)(t128)));
  t130 = ((gg_type_11 *)t126.data) + gg_index(t129, t126.len);
  t131 = (*t130);
  t132 = gg_function_11(t125, t131, base + 0);
  v20 = gg_function_12(t124, t132, base + 0);
  t133 = v19;
  t134 = v17;
  t135 = gg_view((((*(v6))).f5).data, 8);
  t136 = ((int64_t)(((int64_t)UINT64_C(2))));
  t137 = ((int64_t)(v15));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t137)))));
  t139 = ((int64_t)(((int64_t)UINT64_C(1))));
  t140 = ((int64_t)((uint64_t)(t138) + (uint64_t)(t139)));
  t141 = ((gg_type_11 *)t135.data) + gg_index(t140, t135.len);
  t142 = (*t141);
  t143 = gg_function_11(t134, t142, base + 0);
  v21 = gg_function_12(t133, t143, base + 0);
  t144 = v14;
  t145 = v20;
  t146 = v21;
  t147 = gg_function_13(t145, t146, base + 0);
  v14 = gg_function_13(t144, t147, base + 0);
  }
label7:;
  t100++; goto label6;
label8:;
  t148 = ((float)(v9));
  t149 = v14;
  t150 = v2;
  v2 = gg_function_14(t148, t149, t150, base + 0);
  }
label1:;
  t9++; goto label0;
label2:;
  /* FG_DIRECT_SLICE_INDEX */
  t151 = ((int64_t)UINT64_C(4));
  t152 = ((float *)(v2).data) + gg_index(t151, ((int64_t)UINT64_C(8)));
  t153 = ((float)((*t152)));
  /* FG_DIRECT_SLICE_INDEX */
  t154 = ((int64_t)UINT64_C(0));
  t155 = ((float *)(v2).data) + gg_index(t154, ((int64_t)UINT64_C(8)));
  t156 = ((float)((*t155)));
  t157 = ((float)(((t153) + (t156))));
  /* FG_DIRECT_SLICE_INDEX */
  t158 = ((int64_t)UINT64_C(6));
  t159 = ((float *)(v2).data) + gg_index(t158, ((int64_t)UINT64_C(8)));
  t160 = ((float)((*t159)));
  /* FG_DIRECT_SLICE_INDEX */
  t161 = ((int64_t)UINT64_C(2));
  t162 = ((float *)(v2).data) + gg_index(t161, ((int64_t)UINT64_C(8)));
  t163 = ((float)((*t162)));
  t164 = ((float)(((t160) + (t163))));
  t165 = ((float)(((t157) + (t164))));
  /* FG_DIRECT_SLICE_INDEX */
  t166 = ((int64_t)UINT64_C(5));
  t167 = ((float *)(v2).data) + gg_index(t166, ((int64_t)UINT64_C(8)));
  t168 = ((float)((*t167)));
  /* FG_DIRECT_SLICE_INDEX */
  t169 = ((int64_t)UINT64_C(1));
  t170 = ((float *)(v2).data) + gg_index(t169, ((int64_t)UINT64_C(8)));
  t171 = ((float)((*t170)));
  t172 = ((float)(((t168) + (t171))));
  /* FG_DIRECT_SLICE_INDEX */
  t173 = ((int64_t)UINT64_C(7));
  t174 = ((float *)(v2).data) + gg_index(t173, ((int64_t)UINT64_C(8)));
  t175 = ((float)((*t174)));
  /* FG_DIRECT_SLICE_INDEX */
  t176 = ((int64_t)UINT64_C(3));
  t177 = ((float *)(v2).data) + gg_index(t176, ((int64_t)UINT64_C(8)));
  t178 = ((float)((*t177)));
  t179 = ((float)(((t175) + (t178))));
  t180 = ((float)(((t172) + (t179))));
  v22 = ((float)(((t165) + (t180))));
  t181 = ((float)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t182 = ((int64_t)UINT64_C(0));
  t183 = ((float *)(v3).data) + gg_index(t182, ((int64_t)UINT64_C(4)));
  t184 = ((float)((*t183)));
  /* FG_DIRECT_SLICE_INDEX */
  t185 = ((int64_t)UINT64_C(2));
  t186 = ((float *)(v3).data) + gg_index(t185, ((int64_t)UINT64_C(4)));
  t187 = ((float)((*t186)));
  t188 = ((float)(((t184) + (t187))));
  /* FG_DIRECT_SLICE_INDEX */
  t189 = ((int64_t)UINT64_C(1));
  t190 = ((float *)(v3).data) + gg_index(t189, ((int64_t)UINT64_C(4)));
  t191 = ((float)((*t190)));
  /* FG_DIRECT_SLICE_INDEX */
  t192 = ((int64_t)UINT64_C(3));
  t193 = ((float *)(v3).data) + gg_index(t192, ((int64_t)UINT64_C(4)));
  t194 = ((float)((*t193)));
  t195 = ((float)(((t191) + (t194))));
  t196 = ((float)(((t188) + (t195))));
  t0 = ((float)(((t181) + (t196))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  gg_type_1 t0;
  uint16_t t1;
  uint16_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  gg_type_5 t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  gg_type_5 t10;
  int64_t t11;
  gg_type_1 t12;
  int64_t t13;
  gg_type_8 t14;
  float t15;
  gg_type_11 t16;
  int8_t t17;
  int8_t t18;
  int64_t t19;
  gg_type_11 t20;
  int64_t t21;
  int16_t t22;
  int16_t t23;
  int64_t t24;
  gg_type_8 t25;
  int64_t t26;
  gg_header t27;
  gg_slice t28;
  gg_slice t29;
  float t30;
  int64_t t31;
  gg_type_7 v0;
  gg_type_15 v1;
  {
  memset(&(t0), 0, sizeof(t0));
  memset(&t1, 0, sizeof(t1));
  (t0).f0 = ((uint16_t)(t1));
  memset(&t2, 0, sizeof(t2));
  (t0).f1 = ((uint16_t)(t2));
  memset(&t3, 0, sizeof(t3));
  t4 = ((uint8_t)(t3));
  for (t5=0; t5<12; t5++) ((t0).f2).data[t5] = t4;
  memset(&t7, 0, sizeof(t7));
  t8 = ((uint8_t)(t7));
  for (t9=0; t9<32; t9++) (t6).data[t9] = t8;
  t10 = t6;
  for (t11=0; t11<4; t11++) ((t0).f3).data[t11] = t10;
  t12 = t0;
  for (t13=0; t13<1; t13++) (v0).data[t13] = t12;
  memset(&(t14), 0, sizeof(t14));
  memset(&t15, 0, sizeof(t15));
  (t14).f4 = ((float)(t15));
  memset(&t17, 0, sizeof(t17));
  t18 = ((int8_t)(t17));
  for (t19=0; t19<32; t19++) (t16).data[t19] = t18;
  t20 = t16;
  for (t21=0; t21<8; t21++) ((t14).f5).data[t21] = t20;
  memset(&t22, 0, sizeof(t22));
  t23 = ((int16_t)(t22));
  for (t24=0; t24<16; t24++) ((t14).f6).data[t24] = t23;
  t25 = t14;
  for (t26=0; t26<1; t26++) (v1).data[t26] = t25;
  t27.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t27.len = 0;
  t28 = gg_view((v0).data, 1);
  t29 = gg_view((v1).data, 1);
  t30 = gg_function_1(t28, t29, base + 1);
  gg_float32(gg_ref(&t27, gg_get_stack(base + 0)), t30);
  gg_append(gg_ref(&t27, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t27).base, (t27).len));
  }
gg_done:;
  for (t31 = 0; t31 < 1; t31++) gg_stacks[base + t31].top = 0;
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
