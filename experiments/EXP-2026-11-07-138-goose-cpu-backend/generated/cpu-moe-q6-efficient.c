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
typedef struct { uint8_t data[32]; } gg_type_3;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_3 data[4]; } gg_type_4;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_3 data[2]; } gg_type_5;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int8_t data[16]; } gg_type_7;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_1 {
  gg_type_4 f0;
  gg_type_5 f1;
  gg_type_7 f2;
  uint16_t f3;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_1 data[16]; } gg_type_9;
#pragma pack(pop)
typedef struct gg_type_10 gg_type_10;
#pragma pack(push, 1)
typedef struct { int8_t data[32]; } gg_type_12;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_12 data[8]; } gg_type_13;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int16_t data[16]; } gg_type_15;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_10 {
  float f4;
  gg_type_13 f5;
  gg_type_15 f6;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_10 data[1]; } gg_type_16;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[16]; } gg_type_20;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_22;
#pragma pack(pop)
typedef gg_type_1 *gg_type_23;
typedef gg_type_10 *gg_type_24;
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_26;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_28;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_29;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, gg_type_1 value);
static uint8_t gg_eq_2(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint8_t value);
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, gg_type_3 value);
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_type_4 value);
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_type_5 value);
static uint8_t gg_eq_6(int8_t a, int8_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, int8_t value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_8(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, uint16_t value);
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, gg_type_9 value);
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, gg_type_10 value);
static uint8_t gg_eq_11(float a, float b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, float value);
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_type_12 value);
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_type_13 value);
static uint8_t gg_eq_14(int16_t a, int16_t b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, int16_t value);
static uint8_t gg_eq_15(gg_type_15 a, gg_type_15 b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, gg_type_15 value);
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, gg_type_16 value);
static uint8_t gg_eq_17(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_slice value);
static uint8_t gg_eq_18(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, gg_slice value);
static uint8_t gg_eq_19(int64_t a, int64_t b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, int64_t value);
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b);
static uint8_t gg_equal_elements_20(gg_slice a, gg_slice b);
static void gg_render_20(gg_reference out, gg_type_20 value);
static uint8_t gg_eq_21(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_21(gg_slice a, gg_slice b);
static void gg_render_21(gg_reference out, uint8_t value);
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b);
static uint8_t gg_equal_elements_22(gg_slice a, gg_slice b);
static void gg_render_22(gg_reference out, gg_type_22 value);
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b);
static uint8_t gg_equal_elements_23(gg_slice a, gg_slice b);
static void gg_render_23(gg_reference out, gg_type_23 value);
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b);
static uint8_t gg_equal_elements_24(gg_slice a, gg_slice b);
static void gg_render_24(gg_reference out, gg_type_24 value);
static uint8_t gg_eq_25(int32_t a, int32_t b);
static uint8_t gg_equal_elements_25(gg_slice a, gg_slice b);
static void gg_render_25(gg_reference out, int32_t value);
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b);
static uint8_t gg_equal_elements_26(gg_slice a, gg_slice b);
static void gg_render_26(gg_reference out, gg_type_26 value);
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b);
static uint8_t gg_equal_elements_28(gg_slice a, gg_slice b);
static void gg_render_28(gg_reference out, gg_type_28 value);
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b);
static uint8_t gg_equal_elements_29(gg_slice a, gg_slice b);
static void gg_render_29(gg_reference out, gg_type_29 value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((gg_type_1*)a.data)[i],((gg_type_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b) {
if(!gg_eq_4(a.f0,b.f0))return 0;
if(!gg_eq_5(a.f1,b.f1))return 0;
if(!gg_eq_7(a.f2,b.f2))return 0;
if(!gg_eq_8(a.f3,b.f3))return 0;
return 1;
}
static void gg_render_1(gg_reference out, gg_type_1 value) {
gg_append(out,"\121\066\102\154\157\143\153\040\173\040",10,1);
gg_render_4(out, value.f0);
gg_append(out,", ",2,1);
gg_render_5(out, value.f1);
gg_append(out,", ",2,1);
gg_render_7(out, value.f2);
gg_append(out,", ",2,1);
gg_render_8(out, value.f3);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((gg_type_3*)a.data)[i],((gg_type_3*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b) {
return gg_equal_elements_2(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_3(gg_reference out, gg_type_3 value) {
gg_text(out,gg_view((value).data, 32));
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_type_4*)a.data)[i],((gg_type_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b) {
return gg_equal_elements_3(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_3*)view.data)[i]).data, 32));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_type_5*)a.data)[i],((gg_type_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
return gg_equal_elements_3(gg_view((a).data, 2),gg_view((b).data, 2));
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
int64_t i; gg_slice view=gg_view((value).data, 2);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_3*)view.data)[i]).data, 32));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_6(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_8(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((gg_type_9*)a.data)[i],((gg_type_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b) {
return gg_equal_elements_1(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((gg_type_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
if(!gg_eq_11(a.f4,b.f4))return 0;
if(!gg_eq_13(a.f5,b.f5))return 0;
if(!gg_eq_15(a.f6,b.f6))return 0;
return 1;
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
gg_append(out,"\121\070\102\154\157\143\153\040\173\040",10,1);
gg_render_11(out, value.f4);
gg_append(out,", ",2,1);
gg_render_13(out, value.f5);
gg_append(out,", ",2,1);
gg_render_15(out, value.f6);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(float a, float b) {
return a == b;
}
static void gg_render_11(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return gg_equal_elements_6(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
return gg_equal_elements_12(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_12(out, ((gg_type_12*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_14(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((gg_type_15*)a.data)[i],((gg_type_15*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(gg_type_15 a, gg_type_15 b) {
return gg_equal_elements_14(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_15(gg_reference out, gg_type_15 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_14(out, ((int16_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((gg_type_16*)a.data)[i],((gg_type_16*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b) {
return gg_equal_elements_10(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_16(gg_reference out, gg_type_16 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((gg_type_10*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_17(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((gg_type_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_slice a, gg_slice b) {
return gg_equal_elements_10(a,b);
}
static void gg_render_18(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((gg_type_10*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_19(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((gg_type_20*)a.data)[i],((gg_type_20*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b) {
return gg_equal_elements_11(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_20(gg_reference out, gg_type_20 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_11(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_21(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((gg_type_22*)a.data)[i],((gg_type_22*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b) {
return gg_equal_elements_11(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_22(gg_reference out, gg_type_22 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_11(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((gg_type_23*)a.data)[i],((gg_type_23*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b) {
return a == b;
}
static void gg_render_23(gg_reference out, gg_type_23 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((gg_type_24*)a.data)[i],((gg_type_24*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b) {
return a == b;
}
static void gg_render_24(gg_reference out, gg_type_24 value) {
gg_render_10(out,*value);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_25(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((gg_type_26*)a.data)[i],((gg_type_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b) {
return gg_equal_elements_25(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_26(gg_reference out, gg_type_26 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_25(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_type_28*)a.data)[i],((gg_type_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
return gg_equal_elements_25(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_25(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((gg_type_29*)a.data)[i],((gg_type_29*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b) {
return gg_equal_elements_11(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_29(gg_reference out, gg_type_29 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_11(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static gg_type_20 gg_function_1(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base);
static float gg_function_2(gg_slice p0, gg_slice p1, int64_t base);
static gg_type_15 gg_function_4(gg_type_7 p0, int64_t base);
static gg_type_26 gg_function_5(gg_type_15 p55, gg_type_15 p56, int64_t base);
static gg_type_26 gg_function_6(gg_type_26 p0, int64_t base);
static gg_type_3 gg_function_7(gg_type_3 p74, uint8_t p75, int64_t base);
static gg_type_3 gg_function_8(gg_type_3 p0, uint8_t p1, int64_t base);
static gg_type_3 gg_function_9(gg_type_3 p0, gg_type_3 p1, int64_t base);
static gg_type_3 gg_function_10(gg_type_3 p102, uint8_t p103, int64_t base);
static gg_type_15 gg_function_11(gg_type_7 p0, int64_t p1, int64_t base);
static gg_type_15 gg_function_12(gg_type_3 p126, gg_type_12 p127, int64_t base);
static gg_type_26 gg_function_13(gg_type_26 p147, gg_type_26 p148, int64_t base);
static gg_type_26 gg_function_14(gg_type_26 p0, gg_type_26 p1, int64_t base);
static gg_type_22 gg_function_15(float p165, gg_type_26 p166, gg_type_22 p167, int64_t base);
/* FG_PACKED_MATCH:fma32 */
static gg_type_22 gg_function_15(float p165, gg_type_26 p166, gg_type_22 p167, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__) && defined(__FMA__)
  { gg_type_22 packed_result;
    _mm256_storeu_ps(packed_result.data, _mm256_fmadd_ps(_mm256_set1_ps(p165), _mm256_cvtepi32_ps(_mm256_loadu_si256((const __m256i*)p166.data)), _mm256_loadu_ps(p167.data)));
    return packed_result;
  }
#endif
  gg_type_22 t0;
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
  float v165;
  gg_type_26 v166;
  gg_type_22 v167;
  gg_type_22 v168;
  int64_t v170;
  v165 = p165;
  v166 = p166;
  v167 = p167;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v168).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v170 = t4;
  {
  t6 = gg_view((v168).data, 8);
  t7 = v170;
  t8 = ((float *)t6.data) + gg_index(t7, t6.len);
  t9 = ((float)(v165));
  t10 = gg_view((v166).data, 8);
  t11 = v170;
  t12 = ((int32_t *)t10.data) + gg_index(t11, t10.len);
  t13 = ((float)(((float)((*t12)))));
  t14 = gg_view((v167).data, 8);
  t15 = v170;
  t16 = ((float *)t14.data) + gg_index(t15, t14.len);
  t17 = ((float)((*t16)));
  t18 = fmaf(t9, t13, t17);
  (*t8) = ((float)(t18));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v168;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_26 gg_function_14(gg_type_26 p0, gg_type_26 p1, int64_t base) {
  gg_type_26 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int32_t * t7;
  int64_t t8;
  int32_t * t9;
  int32_t t10;
  int64_t t11;
  int32_t * t12;
  int32_t t13;
  gg_type_26 v0;
  gg_type_26 v1;
  gg_type_26 v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((int32_t *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v3;
  t9 = ((int32_t *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(8)));
  t10 = ((int32_t)((*t9)));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v3;
  t12 = ((int32_t *)(v1).data) + gg_index(t11, ((int64_t)UINT64_C(8)));
  t13 = ((int32_t)((*t12)));
  (*t7) = ((int32_t)(((int32_t)((uint64_t)(t10) - (uint64_t)(t13)))));
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
/* FG_PACKED_MATCH:add32 */
static gg_type_26 gg_function_13(gg_type_26 p147, gg_type_26 p148, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_26 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)p147.data), _mm256_loadu_si256((const __m256i*)p148.data)));
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
  int32_t * t11;
  int32_t t12;
  gg_slice t13;
  int64_t t14;
  int32_t * t15;
  int32_t t16;
  gg_type_26 v147;
  gg_type_26 v148;
  gg_type_26 v149;
  int64_t v151;
  v147 = p147;
  v148 = p148;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v149).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v151 = t4;
  {
  t6 = gg_view((v149).data, 8);
  t7 = v151;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v147).data, 8);
  t10 = v151;
  t11 = ((int32_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int32_t)((*t11)));
  t13 = gg_view((v148).data, 8);
  t14 = v151;
  t15 = ((int32_t *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int32_t)((*t15)));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t12) + (uint64_t)(t16)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v149;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:madd16 */
static gg_type_26 gg_function_5(gg_type_15 p55, gg_type_15 p56, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_26 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_madd_epi16(_mm256_loadu_si256((const __m256i*)p55.data), _mm256_loadu_si256((const __m256i*)p56.data)));
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
  gg_type_15 v55;
  gg_type_15 v56;
  gg_type_26 v57;
  int64_t v59;
  int64_t v60;
  v55 = p55;
  v56 = p56;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v57).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v59 = t4;
  {
  t6 = gg_view((v55).data, 16);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v59));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((int16_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int64_t)(((int64_t)((*t10)))));
  t12 = gg_view((v56).data, 16);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v59));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((int16_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int64_t)(((int64_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v55).data, 16);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v59));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((int16_t *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int64_t)(((int64_t)((*t25)))));
  t27 = gg_view((v56).data, 16);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v59));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((int16_t *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int64_t)(((int64_t)((*t33)))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v60 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  t36 = gg_view((v57).data, 8);
  t37 = v59;
  t38 = ((int32_t *)t36.data) + gg_index(t37, t36.len);
  (*t38) = ((int32_t)(((int32_t)(v60))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v57;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:maddubs */
static gg_type_15 gg_function_12(gg_type_3 p126, gg_type_12 p127, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_15 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_maddubs_epi16(_mm256_loadu_si256((const __m256i*)p126.data), _mm256_loadu_si256((const __m256i*)p127.data)));
    return packed_result;
  }
#endif
  gg_type_15 t0;
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
  gg_type_3 v126;
  gg_type_12 v127;
  gg_type_15 v128;
  int64_t v130;
  int32_t v131;
  v126 = p126;
  v127 = p127;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v128).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v130 = t4;
  {
  t6 = gg_view((v126).data, 32);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v130));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((uint8_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int32_t)(((int32_t)((*t10)))));
  t12 = gg_view((v127).data, 32);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v130));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((int8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int32_t)(((int32_t)((*t16)))));
  t18 = ((int32_t)(((int32_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v126).data, 32);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v130));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((uint8_t *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int32_t)(((int32_t)((*t25)))));
  t27 = gg_view((v127).data, 32);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v130));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((int8_t *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int32_t)(((int32_t)((*t33)))));
  t35 = ((int32_t)(((int32_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v131 = ((int32_t)(((int32_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  t36 = gg_view((v128).data, 16);
  t37 = v130;
  t38 = ((int16_t *)t36.data) + gg_index(t37, t36.len);
  t40 = ((int32_t)(v131));
  t41 = ((int64_t)UINT64_C(32768));
  t42 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t41)))));
  t43 = ((t40) < (t42));
  if (t43) {
  {
  t44 = ((int64_t)UINT64_C(32768));
  t39 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t44)))));
  }
  } else {
  t45 = ((int32_t)(v131));
  t46 = ((int32_t)(((int64_t)UINT64_C(32767))));
  t47 = ((t45) > (t46));
  if (t47) {
  {
  t39 = ((int64_t)(((int64_t)UINT64_C(32767))));
  }
  } else {
  {
  t39 = ((int64_t)(v131));
  }
  }
  }
  (*t38) = ((int16_t)(((int16_t)(t39))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v128;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_15 gg_function_11(gg_type_7 p0, int64_t p1, int64_t base) {
  gg_type_15 t0;
  gg_slice t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int8_t * t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int8_t * t12;
  int16_t t13;
  int16_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int16_t * t19;
  gg_slice t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int16_t * t24;
  gg_type_7 v0;
  int64_t v1;
  int16_t v2;
  int16_t v3;
  gg_type_15 v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  {
  t1 = gg_view((v0).data, 16);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(2))));
  t4 = ((int64_t)((uint64_t)(t2) * (uint64_t)(t3)));
  t5 = ((int8_t *)t1.data) + gg_index(t4, t1.len);
  v2 = ((int16_t)(((int16_t)((*t5)))));
  t6 = gg_view((v0).data, 16);
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((int64_t)((uint64_t)(t9) + (uint64_t)(t10)));
  t12 = ((int8_t *)t6.data) + gg_index(t11, t6.len);
  v3 = ((int16_t)(((int16_t)((*t12)))));
  memset(&t13, 0, sizeof(t13));
  t14 = ((int16_t)(t13));
  for (t15=0; t15<16; t15++) (v4).data[t15] = t14;
  t17 = ((int64_t)UINT64_C(8));
  t16 = 0;
label0:;
  if (t16 >= t17) goto label2;
  v5 = t16;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t18 = v5;
  t19 = ((int16_t *)(v4).data) + gg_index(t18, ((int64_t)UINT64_C(16)));
  (*t19) = ((int16_t)(v2));
  t20 = gg_view((v4).data, 16);
  t21 = ((int64_t)(v5));
  t22 = ((int64_t)(((int64_t)UINT64_C(8))));
  t23 = ((int64_t)((uint64_t)(t21) + (uint64_t)(t22)));
  t24 = ((int16_t *)t20.data) + gg_index(t23, t20.len);
  (*t24) = ((int16_t)(v3));
  }
label1:;
  t16++; goto label0;
label2:;
  t0 = v4;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_3 gg_function_9(gg_type_3 p0, gg_type_3 p1, int64_t base) {
  gg_type_3 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t * t7;
  int64_t t8;
  uint8_t * t9;
  uint8_t t10;
  int64_t t11;
  uint8_t * t12;
  uint8_t t13;
  gg_type_3 v0;
  gg_type_3 v1;
  gg_type_3 v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((uint8_t *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v3;
  t9 = ((uint8_t *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(32)));
  t10 = ((uint8_t)((*t9)));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v3;
  t12 = ((uint8_t *)(v1).data) + gg_index(t11, ((int64_t)UINT64_C(32)));
  t13 = ((uint8_t)((*t12)));
  (*t7) = ((uint8_t)(((t10) | (t13))));
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
/* FG_PACKED_MATCH:shr8 */
static gg_type_3 gg_function_10(gg_type_3 p102, uint8_t p103, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_3 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)p102.data), _mm_cvtsi32_si128((p103 & 7))), _mm256_set1_epi8((unsigned char)(255u >> (p103 & 7)))));
    return packed_result;
  }
#endif
  gg_type_3 t0;
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
  gg_type_3 v102;
  uint8_t v103;
  gg_type_3 v104;
  int64_t v106;
  v102 = p102;
  v103 = p103;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v104).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v106 = t4;
  {
  t6 = gg_view((v104).data, 32);
  t7 = v106;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v102).data, 32);
  t10 = v106;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v103));
  (*t8) = ((uint8_t)(((t12) >> ((uint64_t)(t13) & 7))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v104;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:and8 */
static gg_type_3 gg_function_7(gg_type_3 p74, uint8_t p75, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_3 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_loadu_si256((const __m256i*)p74.data), _mm256_set1_epi8(p75)));
    return packed_result;
  }
#endif
  gg_type_3 t0;
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
  gg_type_3 v74;
  uint8_t v75;
  gg_type_3 v76;
  int64_t v78;
  v74 = p74;
  v75 = p75;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v76).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v78 = t4;
  {
  t6 = gg_view((v76).data, 32);
  t7 = v78;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v74).data, 32);
  t10 = v78;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v75));
  (*t8) = ((uint8_t)(((t12) & (t13))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v76;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_3 gg_function_8(gg_type_3 p0, uint8_t p1, int64_t base) {
  gg_type_3 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t * t7;
  int64_t t8;
  uint8_t * t9;
  uint8_t t10;
  uint8_t t11;
  gg_type_3 v0;
  uint8_t v1;
  gg_type_3 v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((uint8_t *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v3;
  t9 = ((uint8_t *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(32)));
  t10 = ((uint8_t)((*t9)));
  t11 = ((uint8_t)(v1));
  (*t7) = ((uint8_t)(((uint8_t)((uint64_t)(t10) << ((uint64_t)(t11) & 7)))));
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
static gg_type_26 gg_function_6(gg_type_26 p0, int64_t base) {
  gg_type_26 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int32_t * t7;
  int64_t t8;
  int32_t * t9;
  int32_t t10;
  int32_t t11;
  gg_type_26 v0;
  gg_type_26 v1;
  int64_t v2;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((int32_t *)(v1).data) + gg_index(t6, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v2;
  t9 = ((int32_t *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(8)));
  t10 = ((int32_t)((*t9)));
  t11 = ((int32_t)(((int64_t)UINT64_C(32))));
  (*t7) = ((int32_t)(((int32_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v1;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_15 gg_function_4(gg_type_7 p0, int64_t base) {
  gg_type_15 t0;
  int16_t t1;
  int16_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int16_t * t7;
  int64_t t8;
  int8_t * t9;
  gg_type_7 v0;
  gg_type_15 v1;
  int64_t v2;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((int16_t *)(v1).data) + gg_index(t6, ((int64_t)UINT64_C(16)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v2;
  t9 = ((int8_t *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(16)));
  (*t7) = ((int16_t)(((int16_t)((*t9)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v1;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_2(gg_slice p0, gg_slice p1, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  float t3;
  float t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  gg_type_1 * t9;
  int64_t t10;
  gg_type_10 * t11;
  float t12;
  uint16_t t13;
  float t14;
  float t15;
  gg_type_15 t16;
  gg_type_7 t17;
  gg_type_15 t18;
  gg_type_26 t19;
  int32_t t20;
  int32_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  gg_type_3 * t26;
  gg_slice t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  gg_type_3 * t31;
  gg_type_3 t32;
  uint8_t t33;
  gg_type_3 t34;
  gg_type_3 t35;
  uint8_t t36;
  gg_type_3 t37;
  uint8_t t38;
  gg_type_3 t39;
  gg_slice t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  gg_type_3 * t46;
  gg_type_3 t47;
  uint8_t t48;
  gg_type_3 t49;
  gg_type_3 t50;
  uint8_t t51;
  gg_type_3 t52;
  uint8_t t53;
  gg_type_3 t54;
  gg_slice t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  gg_type_3 * t59;
  gg_type_3 t60;
  uint8_t t61;
  gg_type_3 t62;
  gg_type_3 t63;
  uint8_t t64;
  gg_type_3 t65;
  gg_slice t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  gg_type_3 * t72;
  gg_type_3 t73;
  uint8_t t74;
  gg_type_3 t75;
  gg_type_3 t76;
  uint8_t t77;
  gg_type_3 t78;
  uint8_t t79;
  gg_type_3 t80;
  gg_type_7 t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  gg_type_15 t85;
  gg_type_3 t86;
  gg_slice t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  gg_type_12 * t91;
  gg_type_12 t92;
  gg_type_15 t93;
  gg_type_7 t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  gg_type_15 t100;
  gg_type_3 t101;
  gg_slice t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  gg_type_12 * t108;
  gg_type_12 t109;
  gg_type_15 t110;
  gg_type_7 t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  gg_type_15 t117;
  gg_type_3 t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  gg_type_12 * t125;
  gg_type_12 t126;
  gg_type_15 t127;
  gg_type_7 t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  gg_type_15 t134;
  gg_type_3 t135;
  gg_slice t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  gg_type_12 * t142;
  gg_type_12 t143;
  gg_type_15 t144;
  gg_type_26 t145;
  gg_type_26 t146;
  gg_type_26 t147;
  gg_type_26 t148;
  gg_type_26 t149;
  gg_type_26 t150;
  gg_type_26 t151;
  gg_type_26 t152;
  float t153;
  gg_type_26 t154;
  gg_type_26 t155;
  gg_type_26 t156;
  gg_type_22 t157;
  int64_t t158;
  float * t159;
  float t160;
  int64_t t161;
  float * t162;
  float t163;
  float t164;
  int64_t t165;
  float * t166;
  float t167;
  int64_t t168;
  float * t169;
  float t170;
  float t171;
  float t172;
  int64_t t173;
  float * t174;
  float t175;
  int64_t t176;
  float * t177;
  float t178;
  float t179;
  int64_t t180;
  float * t181;
  float t182;
  int64_t t183;
  float * t184;
  float t185;
  float t186;
  float t187;
  gg_slice v0;
  gg_slice v1;
  gg_type_22 v2;
  int64_t v3;
  gg_type_23 v4;
  gg_type_24 v5;
  float v6;
  gg_type_26 v7;
  gg_type_26 v8;
  int64_t v9;
  gg_type_3 v10;
  gg_type_3 v11;
  gg_type_3 v12;
  gg_type_3 v13;
  gg_type_3 v14;
  gg_type_26 v15;
  gg_type_26 v16;
  gg_type_26 v17;
  gg_type_26 v18;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)((v1).len));
  t2 = ((int64_t)((v0).len));
  if (!(((t1) >= (t2)))) gg_fail("assertion failed");
  memset(&t3, 0, sizeof(t3));
  t4 = ((float)(t3));
  for (t5=0; t5<8; t5++) (v2).data[t5] = t4;
  t7 = (v0).len;
  t6 = 0;
label0:;
  if (t6 >= t7) goto label2;
  v3 = t6;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v3;
  t9 = ((gg_type_1 *)(v0).data) + gg_index(t8, (v0).len);
  v4 = t9;
  /* FG_DIRECT_SLICE_INDEX */
  t10 = v3;
  t11 = ((gg_type_10 *)(v1).data) + gg_index(t10, (v1).len);
  v5 = t11;
  t12 = ((float)(((*(v5))).f4));
  t13 = ((uint16_t)(((*(v4))).f3));
  t14 = fg_half_to_float(t13);
  t15 = ((float)(t14));
  v6 = ((float)(((t12) * (t15))));
  t16 = ((*(v5))).f6;
  t17 = ((*(v4))).f2;
  t18 = gg_function_4(t17, base + 0);
  t19 = gg_function_5(t16, t18, base + 0);
  v7 = gg_function_6(t19, base + 0);
  memset(&t20, 0, sizeof(t20));
  t21 = ((int32_t)(t20));
  for (t22=0; t22<8; t22++) (v8).data[t22] = t21;
  t24 = ((int64_t)UINT64_C(2));
  t23 = 0;
label3:;
  if (t23 >= t24) goto label5;
  v9 = t23;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v9;
  t26 = ((gg_type_3 *)(((*(v4))).f1).data) + gg_index(t25, ((int64_t)UINT64_C(2)));
  v10 = (*t26);
  t27 = gg_view((((*(v4))).f0).data, 4);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v9));
  t30 = ((int64_t)((uint64_t)(t28) * (uint64_t)(t29)));
  t31 = ((gg_type_3 *)t27.data) + gg_index(t30, t27.len);
  t32 = (*t31);
  t33 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t34 = gg_function_7(t32, t33, base + 0);
  t35 = v10;
  t36 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t37 = gg_function_7(t35, t36, base + 0);
  t38 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t39 = gg_function_8(t37, t38, base + 0);
  v11 = gg_function_9(t34, t39, base + 0);
  t40 = gg_view((((*(v4))).f0).data, 4);
  t41 = ((int64_t)(((int64_t)UINT64_C(2))));
  t42 = ((int64_t)(v9));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  t45 = ((int64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  t46 = ((gg_type_3 *)t40.data) + gg_index(t45, t40.len);
  t47 = (*t46);
  t48 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t49 = gg_function_7(t47, t48, base + 0);
  t50 = v10;
  t51 = ((uint8_t)(((int64_t)UINT64_C(12))));
  t52 = gg_function_7(t50, t51, base + 0);
  t53 = ((uint8_t)(((int64_t)UINT64_C(2))));
  t54 = gg_function_8(t52, t53, base + 0);
  v12 = gg_function_9(t49, t54, base + 0);
  t55 = gg_view((((*(v4))).f0).data, 4);
  t56 = ((int64_t)(((int64_t)UINT64_C(2))));
  t57 = ((int64_t)(v9));
  t58 = ((int64_t)((uint64_t)(t56) * (uint64_t)(t57)));
  t59 = ((gg_type_3 *)t55.data) + gg_index(t58, t55.len);
  t60 = (*t59);
  t61 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t62 = gg_function_10(t60, t61, base + 0);
  t63 = v10;
  t64 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t65 = gg_function_7(t63, t64, base + 0);
  v13 = gg_function_9(t62, t65, base + 0);
  t66 = gg_view((((*(v4))).f0).data, 4);
  t67 = ((int64_t)(((int64_t)UINT64_C(2))));
  t68 = ((int64_t)(v9));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)UINT64_C(1))));
  t71 = ((int64_t)((uint64_t)(t69) + (uint64_t)(t70)));
  t72 = ((gg_type_3 *)t66.data) + gg_index(t71, t66.len);
  t73 = (*t72);
  t74 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t75 = gg_function_10(t73, t74, base + 0);
  t76 = v10;
  t77 = ((uint8_t)(((int64_t)UINT64_C(192))));
  t78 = gg_function_7(t76, t77, base + 0);
  t79 = ((uint8_t)(((int64_t)UINT64_C(2))));
  t80 = gg_function_10(t78, t79, base + 0);
  v14 = gg_function_9(t75, t80, base + 0);
  t81 = ((*(v4))).f2;
  t82 = ((int64_t)(((int64_t)UINT64_C(4))));
  t83 = ((int64_t)(v9));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) * (uint64_t)(t83)))));
  t85 = gg_function_11(t81, t84, base + 0);
  t86 = v11;
  t87 = gg_view((((*(v5))).f5).data, 8);
  t88 = ((int64_t)(((int64_t)UINT64_C(4))));
  t89 = ((int64_t)(v9));
  t90 = ((int64_t)((uint64_t)(t88) * (uint64_t)(t89)));
  t91 = ((gg_type_12 *)t87.data) + gg_index(t90, t87.len);
  t92 = (*t91);
  t93 = gg_function_12(t86, t92, base + 0);
  v15 = gg_function_5(t85, t93, base + 0);
  t94 = ((*(v4))).f2;
  t95 = ((int64_t)(((int64_t)UINT64_C(4))));
  t96 = ((int64_t)(v9));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t95) * (uint64_t)(t96)))));
  t98 = ((int64_t)(((int64_t)UINT64_C(1))));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t97) + (uint64_t)(t98)))));
  t100 = gg_function_11(t94, t99, base + 0);
  t101 = v12;
  t102 = gg_view((((*(v5))).f5).data, 8);
  t103 = ((int64_t)(((int64_t)UINT64_C(4))));
  t104 = ((int64_t)(v9));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t104)))));
  t106 = ((int64_t)(((int64_t)UINT64_C(1))));
  t107 = ((int64_t)((uint64_t)(t105) + (uint64_t)(t106)));
  t108 = ((gg_type_12 *)t102.data) + gg_index(t107, t102.len);
  t109 = (*t108);
  t110 = gg_function_12(t101, t109, base + 0);
  v16 = gg_function_5(t100, t110, base + 0);
  t111 = ((*(v4))).f2;
  t112 = ((int64_t)(((int64_t)UINT64_C(4))));
  t113 = ((int64_t)(v9));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t113)))));
  t115 = ((int64_t)(((int64_t)UINT64_C(2))));
  t116 = ((int64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t115)))));
  t117 = gg_function_11(t111, t116, base + 0);
  t118 = v13;
  t119 = gg_view((((*(v5))).f5).data, 8);
  t120 = ((int64_t)(((int64_t)UINT64_C(4))));
  t121 = ((int64_t)(v9));
  t122 = ((int64_t)(((int64_t)((uint64_t)(t120) * (uint64_t)(t121)))));
  t123 = ((int64_t)(((int64_t)UINT64_C(2))));
  t124 = ((int64_t)((uint64_t)(t122) + (uint64_t)(t123)));
  t125 = ((gg_type_12 *)t119.data) + gg_index(t124, t119.len);
  t126 = (*t125);
  t127 = gg_function_12(t118, t126, base + 0);
  v17 = gg_function_5(t117, t127, base + 0);
  t128 = ((*(v4))).f2;
  t129 = ((int64_t)(((int64_t)UINT64_C(4))));
  t130 = ((int64_t)(v9));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t129) * (uint64_t)(t130)))));
  t132 = ((int64_t)(((int64_t)UINT64_C(3))));
  t133 = ((int64_t)(((int64_t)((uint64_t)(t131) + (uint64_t)(t132)))));
  t134 = gg_function_11(t128, t133, base + 0);
  t135 = v14;
  t136 = gg_view((((*(v5))).f5).data, 8);
  t137 = ((int64_t)(((int64_t)UINT64_C(4))));
  t138 = ((int64_t)(v9));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t137) * (uint64_t)(t138)))));
  t140 = ((int64_t)(((int64_t)UINT64_C(3))));
  t141 = ((int64_t)((uint64_t)(t139) + (uint64_t)(t140)));
  t142 = ((gg_type_12 *)t136.data) + gg_index(t141, t136.len);
  t143 = (*t142);
  t144 = gg_function_12(t135, t143, base + 0);
  v18 = gg_function_5(t134, t144, base + 0);
  t145 = v8;
  t146 = v15;
  t147 = v16;
  t148 = gg_function_13(t146, t147, base + 0);
  v8 = gg_function_13(t145, t148, base + 0);
  t149 = v8;
  t150 = v17;
  t151 = v18;
  t152 = gg_function_13(t150, t151, base + 0);
  v8 = gg_function_13(t149, t152, base + 0);
  }
label4:;
  t23++; goto label3;
label5:;
  t153 = ((float)(v6));
  t154 = v8;
  t155 = v7;
  t156 = gg_function_14(t154, t155, base + 0);
  t157 = v2;
  v2 = gg_function_15(t153, t156, t157, base + 0);
  }
label1:;
  t6++; goto label0;
label2:;
  /* FG_DIRECT_SLICE_INDEX */
  t158 = ((int64_t)UINT64_C(4));
  t159 = ((float *)(v2).data) + gg_index(t158, ((int64_t)UINT64_C(8)));
  t160 = ((float)((*t159)));
  /* FG_DIRECT_SLICE_INDEX */
  t161 = ((int64_t)UINT64_C(0));
  t162 = ((float *)(v2).data) + gg_index(t161, ((int64_t)UINT64_C(8)));
  t163 = ((float)((*t162)));
  t164 = ((float)(((t160) + (t163))));
  /* FG_DIRECT_SLICE_INDEX */
  t165 = ((int64_t)UINT64_C(6));
  t166 = ((float *)(v2).data) + gg_index(t165, ((int64_t)UINT64_C(8)));
  t167 = ((float)((*t166)));
  /* FG_DIRECT_SLICE_INDEX */
  t168 = ((int64_t)UINT64_C(2));
  t169 = ((float *)(v2).data) + gg_index(t168, ((int64_t)UINT64_C(8)));
  t170 = ((float)((*t169)));
  t171 = ((float)(((t167) + (t170))));
  t172 = ((float)(((t164) + (t171))));
  /* FG_DIRECT_SLICE_INDEX */
  t173 = ((int64_t)UINT64_C(5));
  t174 = ((float *)(v2).data) + gg_index(t173, ((int64_t)UINT64_C(8)));
  t175 = ((float)((*t174)));
  /* FG_DIRECT_SLICE_INDEX */
  t176 = ((int64_t)UINT64_C(1));
  t177 = ((float *)(v2).data) + gg_index(t176, ((int64_t)UINT64_C(8)));
  t178 = ((float)((*t177)));
  t179 = ((float)(((t175) + (t178))));
  /* FG_DIRECT_SLICE_INDEX */
  t180 = ((int64_t)UINT64_C(7));
  t181 = ((float *)(v2).data) + gg_index(t180, ((int64_t)UINT64_C(8)));
  t182 = ((float)((*t181)));
  /* FG_DIRECT_SLICE_INDEX */
  t183 = ((int64_t)UINT64_C(3));
  t184 = ((float *)(v2).data) + gg_index(t183, ((int64_t)UINT64_C(8)));
  t185 = ((float)((*t184)));
  t186 = ((float)(((t182) + (t185))));
  t187 = ((float)(((t179) + (t186))));
  t0 = ((float)(((t172) + (t187))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_20 gg_function_1(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t base) {
  gg_type_20 t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  float t21;
  float t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  float * t27;
  gg_slice t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  gg_slice t37;
  gg_slice t38;
  int64_t t39;
  gg_slice t40;
  gg_slice v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  gg_type_20 v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) > (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(16))));
  t8 = ((t9) <= (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)((v0).len));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t12 = ((t13) >= (t16));
  }
  t17 = t12;
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)((v1).len));
  t20 = ((int64_t)(v2));
  t18 = ((t19) >= (t20));
  }
  if (!(t18)) gg_fail("assertion failed");
  memset(&t21, 0, sizeof(t21));
  t22 = ((float)(t21));
  for (t23=0; t23<16; t23++) (v4).data[t23] = t22;
  t25 = v3;
  t24 = 0;
label0:;
  if (t24 >= t25) goto label2;
  v5 = t24;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t26 = v5;
  t27 = ((float *)(v4).data) + gg_index(t26, ((int64_t)UINT64_C(16)));
  t28 = v0;
  t29 = ((int64_t)(v5));
  t30 = ((int64_t)(v2));
  t31 = ((int64_t)((uint64_t)(t29) * (uint64_t)(t30)));
  t32 = ((int64_t)(v5));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)((uint64_t)(t34) * (uint64_t)(t35)));
  t37 = gg_sub(t28, t31, t36, sizeof(gg_type_1));
  t38 = v1;
  t39 = v2;
  t40 = gg_sub(t38, 0, t39, sizeof(gg_type_10));
  (*t27) = gg_function_2(t37, t40, base + 0);
  }
label1:;
  t24++; goto label0;
label2:;
  t0 = v4;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  gg_type_1 t0;
  gg_type_3 t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  gg_type_3 t5;
  int64_t t6;
  gg_type_3 t7;
  uint8_t t8;
  uint8_t t9;
  int64_t t10;
  gg_type_3 t11;
  int64_t t12;
  int8_t t13;
  int8_t t14;
  int64_t t15;
  uint16_t t16;
  gg_type_1 t17;
  int64_t t18;
  gg_type_10 t19;
  float t20;
  gg_type_12 t21;
  int8_t t22;
  int8_t t23;
  int64_t t24;
  gg_type_12 t25;
  int64_t t26;
  int16_t t27;
  int16_t t28;
  int64_t t29;
  gg_type_10 t30;
  int64_t t31;
  gg_slice t32;
  gg_slice t33;
  int64_t t34;
  int64_t t35;
  gg_header t36;
  int64_t t37;
  float * t38;
  int64_t t39;
  gg_type_9 v0;
  gg_type_16 v1;
  gg_type_20 v2;
  {
  memset(&(t0), 0, sizeof(t0));
  memset(&t2, 0, sizeof(t2));
  t3 = ((uint8_t)(t2));
  for (t4=0; t4<32; t4++) (t1).data[t4] = t3;
  t5 = t1;
  for (t6=0; t6<4; t6++) ((t0).f0).data[t6] = t5;
  memset(&t8, 0, sizeof(t8));
  t9 = ((uint8_t)(t8));
  for (t10=0; t10<32; t10++) (t7).data[t10] = t9;
  t11 = t7;
  for (t12=0; t12<2; t12++) ((t0).f1).data[t12] = t11;
  memset(&t13, 0, sizeof(t13));
  t14 = ((int8_t)(t13));
  for (t15=0; t15<16; t15++) ((t0).f2).data[t15] = t14;
  memset(&t16, 0, sizeof(t16));
  (t0).f3 = ((uint16_t)(t16));
  t17 = t0;
  for (t18=0; t18<16; t18++) (v0).data[t18] = t17;
  memset(&(t19), 0, sizeof(t19));
  memset(&t20, 0, sizeof(t20));
  (t19).f4 = ((float)(t20));
  memset(&t22, 0, sizeof(t22));
  t23 = ((int8_t)(t22));
  for (t24=0; t24<32; t24++) (t21).data[t24] = t23;
  t25 = t21;
  for (t26=0; t26<8; t26++) ((t19).f5).data[t26] = t25;
  memset(&t27, 0, sizeof(t27));
  t28 = ((int16_t)(t27));
  for (t29=0; t29<16; t29++) ((t19).f6).data[t29] = t28;
  t30 = t19;
  for (t31=0; t31<1; t31++) (v1).data[t31] = t30;
  t32 = gg_view((v0).data, 16);
  t33 = gg_view((v1).data, 1);
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = ((int64_t)(((int64_t)UINT64_C(16))));
  v2 = gg_function_1(t32, t33, t34, t35, base + 0);
  t36.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t36.len = 0;
  /* FG_DIRECT_SLICE_INDEX */
  t37 = ((int64_t)UINT64_C(0));
  t38 = ((float *)(v2).data) + gg_index(t37, ((int64_t)UINT64_C(16)));
  gg_float32(gg_ref(&t36, gg_get_stack(base + 0)), (*t38));
  gg_append(gg_ref(&t36, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t36).base, (t36).len));
  }
gg_done:;
  for (t39 = 0; t39 < 1; t39++) gg_stacks[base + t39].top = 0;
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
