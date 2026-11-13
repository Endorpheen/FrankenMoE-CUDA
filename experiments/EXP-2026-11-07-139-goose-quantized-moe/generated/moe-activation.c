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
typedef struct { float data[32]; } gg_type_3;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_1 {
  gg_type_3 f0;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_1 data[1]; } gg_type_4;
#pragma pack(pop)
typedef struct gg_type_5 gg_type_5;
#pragma pack(push, 1)
typedef struct { float data[256]; } gg_type_6;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_5 {
  gg_type_6 f1;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_5 data[1]; } gg_type_7;
#pragma pack(pop)
typedef struct gg_type_8 gg_type_8;
#pragma pack(push, 1)
typedef struct { int8_t data[32]; } gg_type_11;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_8 {
  uint16_t f2;
  gg_type_11 f3;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_8 data[1]; } gg_type_12;
#pragma pack(pop)
typedef struct gg_type_13 gg_type_13;
#pragma pack(push, 1)
typedef struct { int8_t data[256]; } gg_type_14;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int16_t data[16]; } gg_type_16;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_13 {
  float f4;
  gg_type_14 f5;
  gg_type_16 f6;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_13 data[1]; } gg_type_17;
#pragma pack(pop)
typedef gg_type_1 *gg_type_24;
typedef gg_type_8 *gg_type_25;
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_26;
#pragma pack(pop)
typedef gg_type_13 *gg_type_31;
typedef gg_type_5 *gg_type_32;
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, gg_type_1 value);
static uint8_t gg_eq_2(float a, float b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, float value);
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, gg_type_3 value);
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
static uint8_t gg_eq_9(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, uint16_t value);
static uint8_t gg_eq_10(int8_t a, int8_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, int8_t value);
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
static uint8_t gg_eq_15(int16_t a, int16_t b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, int16_t value);
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, gg_type_16 value);
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_type_17 value);
static uint8_t gg_eq_18(int64_t a, int64_t b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, int64_t value);
static uint8_t gg_eq_19(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, gg_slice value);
static uint8_t gg_eq_20(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_20(gg_slice a, gg_slice b);
static void gg_render_20(gg_reference out, gg_slice value);
static uint8_t gg_eq_21(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_21(gg_slice a, gg_slice b);
static void gg_render_21(gg_reference out, gg_slice value);
static uint8_t gg_eq_22(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_22(gg_slice a, gg_slice b);
static void gg_render_22(gg_reference out, gg_slice value);
static uint8_t gg_eq_23(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_23(gg_slice a, gg_slice b);
static void gg_render_23(gg_reference out, uint8_t value);
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b);
static uint8_t gg_equal_elements_24(gg_slice a, gg_slice b);
static void gg_render_24(gg_reference out, gg_type_24 value);
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b);
static uint8_t gg_equal_elements_25(gg_slice a, gg_slice b);
static void gg_render_25(gg_reference out, gg_type_25 value);
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b);
static uint8_t gg_equal_elements_26(gg_slice a, gg_slice b);
static void gg_render_26(gg_reference out, gg_type_26 value);
static uint8_t gg_eq_27(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_27(gg_slice a, gg_slice b);
static void gg_render_27(gg_reference out, uint32_t value);
static uint8_t gg_eq_29(double a, double b);
static uint8_t gg_equal_elements_29(gg_slice a, gg_slice b);
static void gg_render_29(gg_reference out, double value);
static uint8_t gg_eq_30(int32_t a, int32_t b);
static uint8_t gg_equal_elements_30(gg_slice a, gg_slice b);
static void gg_render_30(gg_reference out, int32_t value);
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b);
static uint8_t gg_equal_elements_31(gg_slice a, gg_slice b);
static void gg_render_31(gg_reference out, gg_type_31 value);
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b);
static uint8_t gg_equal_elements_32(gg_slice a, gg_slice b);
static void gg_render_32(gg_reference out, gg_type_32 value);
static uint8_t gg_eq_33(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_33(gg_slice a, gg_slice b);
static void gg_render_33(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((gg_type_1*)a.data)[i],((gg_type_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b) {
if(!gg_eq_3(a.f0,b.f0))return 0;
return 1;
}
static void gg_render_1(gg_reference out, gg_type_1 value) {
gg_append(out,"\106\154\157\141\164\063\062\102\154\157\143\153\040\173\040",15,1);
gg_render_3(out, value.f0);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(float a, float b) {
return a == b;
}
static void gg_render_2(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((gg_type_3*)a.data)[i],((gg_type_3*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b) {
return gg_equal_elements_2(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_3(gg_reference out, gg_type_3 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_2(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_type_4*)a.data)[i],((gg_type_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b) {
return gg_equal_elements_1(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((gg_type_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_type_5*)a.data)[i],((gg_type_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
if(!gg_eq_6(a.f1,b.f1))return 0;
return 1;
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
gg_append(out,"\106\154\157\141\164\102\154\157\143\153\040\173\040",13,1);
gg_render_6(out, value.f1);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_type_6*)a.data)[i],((gg_type_6*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b) {
return gg_equal_elements_2(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_6(gg_reference out, gg_type_6 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_2(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_5(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((gg_type_5*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
if(!gg_eq_9(a.f2,b.f2))return 0;
if(!gg_eq_11(a.f3,b.f3))return 0;
return 1;
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
gg_append(out,"\121\070\132\145\162\157\040\173\040",9,1);
gg_render_9(out, value.f2);
gg_append(out,", ",2,1);
gg_render_11(out, value.f3);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_9(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
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
return gg_equal_elements_8(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((gg_type_8*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
if(!gg_eq_2(a.f4,b.f4))return 0;
if(!gg_eq_14(a.f5,b.f5))return 0;
if(!gg_eq_16(a.f6,b.f6))return 0;
return 1;
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
gg_append(out,"\121\070\102\154\157\143\153\040\173\040",10,1);
gg_render_2(out, value.f4);
gg_append(out,", ",2,1);
gg_render_14(out, value.f5);
gg_append(out,", ",2,1);
gg_render_16(out, value.f6);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_type_14*)a.data)[i],((gg_type_14*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b) {
return gg_equal_elements_10(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_14(gg_reference out, gg_type_14 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_15(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((gg_type_16*)a.data)[i],((gg_type_16*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b) {
return gg_equal_elements_15(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_16(gg_reference out, gg_type_16 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_15(out, ((int16_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_type_17*)a.data)[i],((gg_type_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b) {
return gg_equal_elements_13(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_17(gg_reference out, gg_type_17 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_13(out, ((gg_type_13*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_18(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_19(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((gg_type_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_slice a, gg_slice b) {
return gg_equal_elements_5(a,b);
}
static void gg_render_20(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((gg_type_5*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_21(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((gg_type_8*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(gg_slice a, gg_slice b) {
return gg_equal_elements_13(a,b);
}
static void gg_render_22(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_13(out, ((gg_type_13*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_23(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((gg_type_24*)a.data)[i],((gg_type_24*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b) {
return a == b;
}
static void gg_render_24(gg_reference out, gg_type_24 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((gg_type_25*)a.data)[i],((gg_type_25*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b) {
return a == b;
}
static void gg_render_25(gg_reference out, gg_type_25 value) {
gg_render_8(out,*value);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((gg_type_26*)a.data)[i],((gg_type_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b) {
return gg_equal_elements_2(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_26(gg_reference out, gg_type_26 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_2(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_27(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(double a, double b) {
return a == b;
}
static void gg_render_29(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_30(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_30(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_30(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_30(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_31(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_31(((gg_type_31*)a.data)[i],((gg_type_31*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b) {
return a == b;
}
static void gg_render_31(gg_reference out, gg_type_31 value) {
gg_render_13(out,*value);
}
static uint8_t gg_equal_elements_32(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_32(((gg_type_32*)a.data)[i],((gg_type_32*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b) {
return a == b;
}
static void gg_render_32(gg_reference out, gg_type_32 value) {
gg_render_5(out,*value);
}
static uint8_t gg_equal_elements_33(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_33(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_33(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_33(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t base);
static void gg_function_2(gg_slice p0, gg_slice p1, int64_t base);
static float gg_function_3(float p0, int64_t base);
static float gg_function_6(float p0, float p1, int64_t base);
static int8_t gg_function_8(float p0, int64_t base);
static int32_t gg_function_9(float p0, int64_t base);
static void gg_function_11(gg_slice p0, gg_slice p1, int64_t base);
static int32_t gg_function_12(float p0, int64_t base);
static int32_t gg_function_12(float p0, int64_t base) {
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
static void gg_function_11(gg_slice p0, gg_slice p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  gg_type_13 * t5;
  int64_t t6;
  gg_type_5 * t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  float * t11;
  float t12;
  float t13;
  uint8_t t14;
  float t15;
  float t16;
  float t17;
  uint8_t t18;
  float t19;
  float t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int8_t * t25;
  float t26;
  float t27;
  float t28;
  int64_t t29;
  int64_t t30;
  float t31;
  int64_t t32;
  float * t33;
  float t34;
  float t35;
  int64_t t36;
  int8_t * t37;
  int32_t t38;
  int32_t t39;
  int32_t t40;
  uint8_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int32_t t46;
  gg_slice t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int8_t * t53;
  int32_t t54;
  int64_t t55;
  int16_t * t56;
  float t57;
  float t58;
  gg_slice v0;
  gg_slice v1;
  int64_t v2;
  gg_type_31 v3;
  gg_type_32 v4;
  float v5;
  float v6;
  int64_t v7;
  float v8;
  float v9;
  int64_t v10;
  float v11;
  int64_t v12;
  int32_t v13;
  int64_t v14;
  int32_t v15;
  int64_t v16;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)((v0).len));
  t1 = ((int64_t)((v1).len));
  if (!(((t0) >= (t1)))) gg_fail("assertion failed");
  t3 = (v1).len;
  t2 = 0;
label0:;
  if (t2 >= t3) goto label2;
  v2 = t2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t4 = v2;
  t5 = ((gg_type_13 *)(v1).data) + gg_index(t4, (v1).len);
  v3 = t5;
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((gg_type_5 *)(v0).data) + gg_index(t6, (v0).len);
  v4 = t7;
  v5 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  v6 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t9 = ((int64_t)UINT64_C(256));
  t8 = 0;
label3:;
  if (t8 >= t9) goto label5;
  v7 = t8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t10 = v7;
  t11 = ((float *)(((*(v4))).f1).data) + gg_index(t10, ((int64_t)UINT64_C(256)));
  v8 = ((float)((*t11)));
  t12 = ((float)(v8));
  t13 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t15 = v8;
  v9 = ((float)((-(t15))));
  }
  } else {
  {
  v9 = ((float)(v8));
  }
  }
  t16 = ((float)(v9));
  t17 = ((float)(v6));
  t18 = ((t16) > (t17));
  if (t18) {
  {
  v6 = ((float)(v9));
  v5 = ((float)(v8));
  }
  }
  }
label4:;
  t8++; goto label3;
label5:;
  t19 = ((float)(v6));
  t20 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  ((*(v3))).f4 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t23 = ((int64_t)UINT64_C(256));
  t22 = 0;
label6:;
  if (t22 >= t23) goto label8;
  v10 = t22;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t24 = v10;
  t25 = ((int8_t *)(((*(v3))).f5).data) + gg_index(t24, ((int64_t)UINT64_C(256)));
  (*t25) = ((int8_t)(((int64_t)UINT64_C(0))));
  }
label7:;
  t22++; goto label6;
label8:;
  }
  } else {
  {
  t26 = ((float)(gg_double_bits(UINT64_C(4638637247447433216))));
  t27 = ((float)((-(t26))));
  t28 = ((float)(v5));
  v11 = ((float)(((t27) / (t28))));
  t30 = ((int64_t)UINT64_C(256));
  t29 = 0;
label9:;
  if (t29 >= t30) goto label11;
  v12 = t29;
  {
  t31 = ((float)(v11));
  /* FG_DIRECT_SLICE_INDEX */
  t32 = v12;
  t33 = ((float *)(((*(v4))).f1).data) + gg_index(t32, ((int64_t)UINT64_C(256)));
  t34 = ((float)((*t33)));
  t35 = ((float)(((t31) * (t34))));
  v13 = gg_function_12(t35, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t36 = v12;
  t37 = ((int8_t *)(((*(v3))).f5).data) + gg_index(t36, ((int64_t)UINT64_C(256)));
  t39 = ((int32_t)(v13));
  t40 = ((int32_t)(((int64_t)UINT64_C(127))));
  t41 = ((t39) < (t40));
  if (t41) {
  {
  t38 = ((int32_t)(v13));
  }
  } else {
  {
  t38 = ((int32_t)(((int64_t)UINT64_C(127))));
  }
  }
  (*t37) = ((int8_t)(((int8_t)(t38))));
  }
label10:;
  t29++; goto label9;
label11:;
  t43 = ((int64_t)UINT64_C(16));
  t42 = 0;
label12:;
  if (t42 >= t43) goto label14;
  v14 = t42;
  {
  v15 = ((int32_t)(((int64_t)UINT64_C(0))));
  t45 = ((int64_t)UINT64_C(16));
  t44 = 0;
label15:;
  if (t44 >= t45) goto label17;
  v16 = t44;
  {
  t46 = v15;
  t47 = gg_view((((*(v3))).f5).data, 256);
  t48 = ((int64_t)(v14));
  t49 = ((int64_t)(((int64_t)UINT64_C(16))));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t49)))));
  t51 = ((int64_t)(v16));
  t52 = ((int64_t)((uint64_t)(t50) + (uint64_t)(t51)));
  t53 = ((int8_t *)t47.data) + gg_index(t52, t47.len);
  t54 = ((int32_t)(((int32_t)((*t53)))));
  v15 = ((int32_t)((uint64_t)(t46) + (uint64_t)(t54)));
  }
label16:;
  t44++; goto label15;
label17:;
  /* FG_DIRECT_SLICE_INDEX */
  t55 = v14;
  t56 = ((int16_t *)(((*(v3))).f6).data) + gg_index(t55, ((int64_t)UINT64_C(16)));
  (*t56) = ((int16_t)(((int16_t)(v15))));
  }
label13:;
  t42++; goto label12;
label14:;
  t57 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t58 = ((float)(v11));
  ((*(v3))).f4 = ((float)(((t57) / (t58))));
  }
  }
  }
label1:;
  t2++; goto label0;
label2:;
  }
gg_done:;
}
static int32_t gg_function_9(float p0, int64_t base) {
  int32_t t0;
  float t1;
  uint32_t t2;
  uint32_t t3;
  int64_t t4;
  uint32_t t5;
  uint32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  int32_t t10;
  uint8_t t11;
  uint32_t t12;
  uint32_t t13;
  uint32_t t14;
  uint32_t t15;
  int32_t t16;
  int32_t t17;
  uint8_t t18;
  uint32_t t19;
  int32_t t20;
  int32_t t21;
  uint32_t t22;
  uint32_t t23;
  int32_t t24;
  int32_t t25;
  uint32_t t26;
  uint32_t t27;
  uint32_t t28;
  uint32_t t29;
  uint32_t t30;
  uint8_t t31;
  int32_t t32;
  float v0;
  uint32_t v1;
  int32_t v2;
  uint32_t v3;
  uint32_t v4;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = fg_q8k_float_bits(t1);
  v1 = ((uint32_t)(t2));
  t3 = ((uint32_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(23))));
  t5 = ((uint32_t)(((t3) >> ((uint64_t)(t4) & 31))));
  t6 = ((uint32_t)(((int64_t)UINT64_C(255))));
  t7 = ((int32_t)(((int32_t)(((t5) & (t6))))));
  t8 = ((int32_t)(((int64_t)UINT64_C(127))));
  v2 = ((int32_t)(((int32_t)((uint64_t)(t7) - (uint64_t)(t8)))));
  t9 = ((int32_t)(v2));
  t10 = ((int32_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) < (t10));
  if (t11) {
  {
  t0 = ((int32_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t12 = ((uint32_t)(v1));
  t13 = ((uint32_t)(((int64_t)UINT64_C(8388607))));
  t14 = ((uint32_t)(((t12) & (t13))));
  t15 = ((uint32_t)(((int64_t)UINT64_C(8388608))));
  v3 = ((uint32_t)(((t14) | (t15))));
  t16 = ((int32_t)(v2));
  t17 = ((int32_t)(((int64_t)UINT64_C(23))));
  t18 = ((t16) >= (t17));
  if (t18) {
  {
  t19 = ((uint32_t)(v3));
  t20 = ((int32_t)(v2));
  t21 = ((int32_t)(((int64_t)UINT64_C(23))));
  t22 = ((uint32_t)(((uint32_t)(((int32_t)((uint64_t)(t20) - (uint64_t)(t21)))))));
  v4 = ((uint32_t)(((uint32_t)((uint64_t)(t19) << ((uint64_t)(t22) & 31)))));
  }
  } else {
  {
  t23 = ((uint32_t)(v3));
  t24 = ((int32_t)(((int64_t)UINT64_C(23))));
  t25 = ((int32_t)(v2));
  t26 = ((uint32_t)(((uint32_t)(((int32_t)((uint64_t)(t24) - (uint64_t)(t25)))))));
  v4 = ((uint32_t)(((t23) >> ((uint64_t)(t26) & 31))));
  }
  }
  t27 = ((uint32_t)(v1));
  t28 = ((uint32_t)(((int64_t)UINT64_C(2147483648))));
  t29 = ((uint32_t)(((t27) & (t28))));
  t30 = ((uint32_t)(((int64_t)UINT64_C(0))));
  t31 = ((t29) != (t30));
  if (t31) {
  {
  t32 = ((int32_t)(v4));
  t0 = ((int32_t)(((int32_t)((uint64_t)0 - (uint64_t)(t32)))));
  }
  } else {
  {
  t0 = ((int32_t)(((int32_t)(v4))));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int8_t gg_function_8(float p0, int64_t base) {
  int8_t t0;
  float t1;
  uint32_t t2;
  uint32_t t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
  uint8_t t7;
  int8_t t8;
  float t9;
  float t10;
  float t11;
  float t12;
  float t13;
  uint8_t t14;
  uint8_t t15;
  float t16;
  float t17;
  uint8_t t18;
  uint8_t t19;
  int32_t t20;
  int32_t t21;
  int32_t t22;
  int32_t t23;
  uint8_t t24;
  int32_t t25;
  int32_t t26;
  float t27;
  float t28;
  float t29;
  uint8_t t30;
  uint8_t t31;
  float t32;
  float t33;
  float t34;
  uint8_t t35;
  uint8_t t36;
  int32_t t37;
  int32_t t38;
  int32_t t39;
  int32_t t40;
  uint8_t t41;
  int32_t t42;
  int32_t t43;
  int64_t t44;
  int32_t t45;
  int32_t t46;
  uint8_t t47;
  int32_t t48;
  int64_t t49;
  int32_t t50;
  uint8_t t51;
  int64_t t52;
  float v0;
  int32_t v1;
  float v2;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = fg_q8k_float_bits(t1);
  t3 = ((uint32_t)(t2));
  t4 = ((uint32_t)(((int64_t)UINT64_C(2147483647))));
  t5 = ((uint32_t)(((t3) & (t4))));
  t6 = ((uint32_t)(((int64_t)UINT64_C(1325400064))));
  t7 = ((t5) >= (t6));
  if (t7) {
  {
  t8 = ((int8_t)(((int64_t)UINT64_C(128))));
  t0 = ((int8_t)(((int64_t)((uint64_t)0 - (uint64_t)(t8)))));
  goto gg_done;
  }
  }
  t9 = ((float)(v0));
  v1 = gg_function_9(t9, base + 0);
  t10 = ((float)(v0));
  t11 = ((float)(((float)(v1))));
  v2 = ((float)(((t10) - (t11))));
  t12 = ((float)(v2));
  t13 = ((float)(((float)(gg_double_bits(UINT64_C(4602678819172646912))))));
  t14 = ((t12) > (t13));
  t15 = t14;
  if (!t15) {
  t16 = ((float)(v2));
  t17 = ((float)(((float)(gg_double_bits(UINT64_C(4602678819172646912))))));
  t18 = ((t16) == (t17));
  t19 = t18;
  if (t19) {
  t20 = ((int32_t)(v1));
  t21 = ((int32_t)(((int64_t)UINT64_C(1))));
  t22 = ((int32_t)(((t20) & (t21))));
  t23 = ((int32_t)(((int64_t)UINT64_C(0))));
  t19 = ((t22) != (t23));
  }
  t15 = t19;
  }
  t24 = t15;
  if (t24) {
  {
  t25 = v1;
  t26 = ((int32_t)(((int64_t)UINT64_C(1))));
  v1 = ((int32_t)((uint64_t)(t25) + (uint64_t)(t26)));
  }
  }
  t27 = ((float)(v2));
  t28 = ((float)(gg_double_bits(UINT64_C(4602678819172646912))));
  t29 = ((float)((-(t28))));
  t30 = ((t27) < (t29));
  t31 = t30;
  if (!t31) {
  t32 = ((float)(v2));
  t33 = ((float)(gg_double_bits(UINT64_C(4602678819172646912))));
  t34 = ((float)((-(t33))));
  t35 = ((t32) == (t34));
  t36 = t35;
  if (t36) {
  t37 = ((int32_t)(v1));
  t38 = ((int32_t)(((int64_t)UINT64_C(1))));
  t39 = ((int32_t)(((t37) & (t38))));
  t40 = ((int32_t)(((int64_t)UINT64_C(0))));
  t36 = ((t39) != (t40));
  }
  t31 = t36;
  }
  t41 = t31;
  if (t41) {
  {
  t42 = v1;
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
  v1 = ((int32_t)((uint64_t)(t42) - (uint64_t)(t43)));
  }
  }
  t45 = ((int32_t)(v1));
  t46 = ((int32_t)(((int64_t)UINT64_C(127))));
  t47 = ((t45) > (t46));
  if (t47) {
  {
  t44 = ((int64_t)(((int64_t)UINT64_C(127))));
  }
  } else {
  t48 = ((int32_t)(v1));
  t49 = ((int64_t)UINT64_C(128));
  t50 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t49)))));
  t51 = ((t48) < (t50));
  if (t51) {
  {
  t52 = ((int64_t)UINT64_C(128));
  t44 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t52)))));
  }
  } else {
  {
  t44 = ((int64_t)(v1));
  }
  }
  }
  t0 = ((int8_t)(((int8_t)(t44))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_6(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((float)(v0));
  }
  } else {
  {
  t0 = ((float)(v1));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_3(float p0, int64_t base) {
  float t0;
  float t1;
  uint32_t t2;
  uint32_t t3;
  uint32_t t4;
  uint32_t t5;
  float t6;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = fg_q8k_float_bits(t1);
  t3 = ((uint32_t)(t2));
  t4 = ((uint32_t)(((int64_t)UINT64_C(2147483647))));
  t5 = ((uint32_t)(((t3) & (t4))));
  t6 = fg139_float_from_bits(t5);
  t0 = ((float)(t6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_2(gg_slice p0, gg_slice p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  gg_type_1 * t5;
  int64_t t6;
  gg_type_8 * t7;
  float t8;
  float t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  float * t14;
  float t15;
  float t16;
  gg_slice t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  float * t21;
  float t22;
  float t23;
  float t24;
  gg_slice t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  float * t29;
  float t30;
  float t31;
  int64_t t32;
  float * t33;
  float t34;
  gg_slice t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  float * t39;
  float t40;
  float t41;
  int64_t t42;
  float * t43;
  float t44;
  int64_t t45;
  float * t46;
  float t47;
  float t48;
  int64_t t49;
  float * t50;
  float t51;
  int64_t t52;
  float * t53;
  float t54;
  float t55;
  int64_t t56;
  float * t57;
  float t58;
  int64_t t59;
  float * t60;
  float t61;
  float t62;
  int64_t t63;
  float * t64;
  float t65;
  int64_t t66;
  float * t67;
  float t68;
  float t69;
  float t70;
  float t71;
  float t72;
  float t73;
  float t74;
  uint16_t t75;
  float t76;
  float t77;
  uint8_t t78;
  float t79;
  float t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int8_t * t84;
  int64_t t85;
  float * t86;
  float t87;
  float t88;
  float t89;
  gg_slice v0;
  gg_slice v1;
  int64_t v2;
  gg_type_24 v3;
  gg_type_25 v4;
  gg_type_26 v5;
  int64_t v6;
  float v7;
  float v8;
  float v9;
  float v10;
  float v11;
  float v12;
  int64_t v13;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)((v0).len));
  t1 = ((int64_t)((v1).len));
  if (!(((t0) >= (t1)))) gg_fail("assertion failed");
  t3 = (v1).len;
  t2 = 0;
label0:;
  if (t2 >= t3) goto label2;
  v2 = t2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t4 = v2;
  t5 = ((gg_type_1 *)(v0).data) + gg_index(t4, (v0).len);
  v3 = t5;
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((gg_type_8 *)(v1).data) + gg_index(t6, (v1).len);
  v4 = t7;
  memset(&t8, 0, sizeof(t8));
  t9 = ((float)(t8));
  for (t10=0; t10<8; t10++) (v5).data[t10] = t9;
  t12 = ((int64_t)UINT64_C(8));
  t11 = 0;
label3:;
  if (t11 >= t12) goto label5;
  v6 = t11;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t13 = v6;
  t14 = ((float *)(((*(v3))).f0).data) + gg_index(t13, ((int64_t)UINT64_C(32)));
  t15 = ((float)((*t14)));
  t16 = gg_function_3(t15, base + 0);
  t17 = gg_view((((*(v3))).f0).data, 32);
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  t21 = ((float *)t17.data) + gg_index(t20, t17.len);
  t22 = ((float)((*t21)));
  t23 = gg_function_3(t22, base + 0);
  v7 = gg_function_6(t16, t23, base + 0);
  t24 = ((float)(v7));
  t25 = gg_view((((*(v3))).f0).data, 32);
  t26 = ((int64_t)(((int64_t)UINT64_C(16))));
  t27 = ((int64_t)(v6));
  t28 = ((int64_t)((uint64_t)(t26) + (uint64_t)(t27)));
  t29 = ((float *)t25.data) + gg_index(t28, t25.len);
  t30 = ((float)((*t29)));
  t31 = gg_function_3(t30, base + 0);
  v8 = gg_function_6(t24, t31, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t32 = v6;
  t33 = ((float *)(v5).data) + gg_index(t32, ((int64_t)UINT64_C(8)));
  t34 = ((float)(v8));
  t35 = gg_view((((*(v3))).f0).data, 32);
  t36 = ((int64_t)(((int64_t)UINT64_C(24))));
  t37 = ((int64_t)(v6));
  t38 = ((int64_t)((uint64_t)(t36) + (uint64_t)(t37)));
  t39 = ((float *)t35.data) + gg_index(t38, t35.len);
  t40 = ((float)((*t39)));
  t41 = gg_function_3(t40, base + 0);
  (*t33) = gg_function_6(t34, t41, base + 0);
  }
label4:;
  t11++; goto label3;
label5:;
  /* FG_DIRECT_SLICE_INDEX */
  t42 = ((int64_t)UINT64_C(4));
  t43 = ((float *)(v5).data) + gg_index(t42, ((int64_t)UINT64_C(8)));
  t44 = ((float)((*t43)));
  /* FG_DIRECT_SLICE_INDEX */
  t45 = ((int64_t)UINT64_C(0));
  t46 = ((float *)(v5).data) + gg_index(t45, ((int64_t)UINT64_C(8)));
  t47 = ((float)((*t46)));
  t48 = gg_function_6(t44, t47, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t49 = ((int64_t)UINT64_C(6));
  t50 = ((float *)(v5).data) + gg_index(t49, ((int64_t)UINT64_C(8)));
  t51 = ((float)((*t50)));
  /* FG_DIRECT_SLICE_INDEX */
  t52 = ((int64_t)UINT64_C(2));
  t53 = ((float *)(v5).data) + gg_index(t52, ((int64_t)UINT64_C(8)));
  t54 = ((float)((*t53)));
  t55 = gg_function_6(t51, t54, base + 0);
  v9 = gg_function_6(t48, t55, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t56 = ((int64_t)UINT64_C(5));
  t57 = ((float *)(v5).data) + gg_index(t56, ((int64_t)UINT64_C(8)));
  t58 = ((float)((*t57)));
  /* FG_DIRECT_SLICE_INDEX */
  t59 = ((int64_t)UINT64_C(1));
  t60 = ((float *)(v5).data) + gg_index(t59, ((int64_t)UINT64_C(8)));
  t61 = ((float)((*t60)));
  t62 = gg_function_6(t58, t61, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t63 = ((int64_t)UINT64_C(7));
  t64 = ((float *)(v5).data) + gg_index(t63, ((int64_t)UINT64_C(8)));
  t65 = ((float)((*t64)));
  /* FG_DIRECT_SLICE_INDEX */
  t66 = ((int64_t)UINT64_C(3));
  t67 = ((float *)(v5).data) + gg_index(t66, ((int64_t)UINT64_C(8)));
  t68 = ((float)((*t67)));
  t69 = gg_function_6(t65, t68, base + 0);
  v10 = gg_function_6(t62, t69, base + 0);
  t70 = ((float)(v9));
  t71 = ((float)(v10));
  v11 = gg_function_6(t70, t71, base + 0);
  t72 = ((float)(v11));
  t73 = ((float)(((float)(gg_double_bits(UINT64_C(4638637247447433216))))));
  t74 = ((float)(((t72) / (t73))));
  t75 = fg139_float_half(t74);
  ((*(v4))).f2 = ((uint16_t)(t75));
  t76 = ((float)(v11));
  t77 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t78 = ((t76) != (t77));
  if (t78) {
  {
  t79 = ((float)(((float)(gg_double_bits(UINT64_C(4638637247447433216))))));
  t80 = ((float)(v11));
  v12 = ((float)(((t79) / (t80))));
  }
  } else {
  {
  v12 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t82 = ((int64_t)UINT64_C(32));
  t81 = 0;
label6:;
  if (t81 >= t82) goto label8;
  v13 = t81;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = v13;
  t84 = ((int8_t *)(((*(v4))).f3).data) + gg_index(t83, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t85 = v13;
  t86 = ((float *)(((*(v3))).f0).data) + gg_index(t85, ((int64_t)UINT64_C(32)));
  t87 = ((float)((*t86)));
  t88 = ((float)(v12));
  t89 = ((float)(((t87) * (t88))));
  (*t84) = gg_function_8(t89, base + 0);
  }
label7:;
  t81++; goto label6;
label8:;
  }
label1:;
  t2++; goto label0;
label2:;
  }
gg_done:;
}
static void gg_function_1(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  gg_slice t3;
  gg_slice t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  gg_slice t9;
  gg_slice t10;
  int64_t t11;
  int64_t t12;
  int64_t v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  t3 = v1;
  t4 = v3;
  gg_function_2(t3, t4, base + 0);
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)((v3).len));
  fg139_quant_witness(t5, t6);
  }
  } else {
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  if (!(((t7) == (t8)))) gg_fail("assertion failed");
  t9 = v2;
  t10 = v4;
  gg_function_11(t9, t10, base + 0);
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((int64_t)((v4).len));
  fg139_quant_witness(t11, t12);
  }
  }
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  gg_type_1 t0;
  float t1;
  float t2;
  int64_t t3;
  gg_type_1 t4;
  int64_t t5;
  gg_type_5 t6;
  float t7;
  float t8;
  int64_t t9;
  gg_type_5 t10;
  int64_t t11;
  gg_type_8 t12;
  uint16_t t13;
  int8_t t14;
  int8_t t15;
  int64_t t16;
  gg_type_8 t17;
  int64_t t18;
  gg_type_13 t19;
  float t20;
  int8_t t21;
  int8_t t22;
  int64_t t23;
  int16_t t24;
  int16_t t25;
  int64_t t26;
  gg_type_13 t27;
  int64_t t28;
  int64_t t29;
  gg_slice t30;
  gg_slice t31;
  gg_slice t32;
  gg_slice t33;
  gg_header t34;
  int64_t t35;
  gg_type_8 * t36;
  int64_t t37;
  gg_type_4 v0;
  gg_type_7 v1;
  gg_type_12 v2;
  gg_type_17 v3;
  {
  memset(&(t0), 0, sizeof(t0));
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<32; t3++) ((t0).f0).data[t3] = t2;
  t4 = t0;
  for (t5=0; t5<1; t5++) (v0).data[t5] = t4;
  memset(&(t6), 0, sizeof(t6));
  memset(&t7, 0, sizeof(t7));
  t8 = ((float)(t7));
  for (t9=0; t9<256; t9++) ((t6).f1).data[t9] = t8;
  t10 = t6;
  for (t11=0; t11<1; t11++) (v1).data[t11] = t10;
  memset(&(t12), 0, sizeof(t12));
  memset(&t13, 0, sizeof(t13));
  (t12).f2 = ((uint16_t)(t13));
  memset(&t14, 0, sizeof(t14));
  t15 = ((int8_t)(t14));
  for (t16=0; t16<32; t16++) ((t12).f3).data[t16] = t15;
  t17 = t12;
  for (t18=0; t18<1; t18++) (v2).data[t18] = t17;
  memset(&(t19), 0, sizeof(t19));
  memset(&t20, 0, sizeof(t20));
  (t19).f4 = ((float)(t20));
  memset(&t21, 0, sizeof(t21));
  t22 = ((int8_t)(t21));
  for (t23=0; t23<256; t23++) ((t19).f5).data[t23] = t22;
  memset(&t24, 0, sizeof(t24));
  t25 = ((int16_t)(t24));
  for (t26=0; t26<16; t26++) ((t19).f6).data[t26] = t25;
  t27 = t19;
  for (t28=0; t28<1; t28++) (v3).data[t28] = t27;
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = gg_view((v0).data, 1);
  t31 = gg_view((v1).data, 1);
  t32 = gg_view((v2).data, 1);
  t33 = gg_view((v3).data, 1);
  gg_function_1(t29, t30, t31, t32, t33, base + 0);
  t34.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t34.len = 0;
  /* FG_DIRECT_SLICE_INDEX */
  t35 = ((int64_t)UINT64_C(0));
  t36 = ((gg_type_8 *)(v2).data) + gg_index(t35, ((int64_t)UINT64_C(1)));
  gg_unsigned(gg_ref(&t34, gg_get_stack(base + 0)), ((*t36)).f2);
  gg_append(gg_ref(&t34, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t34).base, (t34).len));
  }
gg_done:;
  for (t37 = 0; t37 < 1; t37++) gg_stacks[base + t37].top = 0;
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
