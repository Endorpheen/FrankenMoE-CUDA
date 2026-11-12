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
struct gg_type_1 {
  int64_t f0;
  int64_t f1;
  int64_t f2;
  int64_t f3;
  int64_t f4;
  int64_t f5;
  int64_t f6;
  int64_t f7;
  int64_t f8;
  int64_t f9;
  int64_t f10;
  int64_t f11;
  int64_t f12;
  int64_t f13;
  int64_t f14;
  int64_t f15;
};
#pragma pack(pop)
typedef struct gg_type_3 gg_type_3;
#pragma pack(push, 1)
typedef struct { uint8_t data[16]; } gg_type_6;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_3 {
  uint16_t f16;
  gg_type_6 f17;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_3 data[1]; } gg_type_7;
#pragma pack(pop)
typedef struct gg_type_8 gg_type_8;
#pragma pack(push, 1)
typedef struct { uint8_t data[4]; } gg_type_9;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_8 {
  uint16_t f18;
  gg_type_9 f19;
  gg_type_6 f20;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_8 data[1]; } gg_type_10;
#pragma pack(pop)
typedef struct gg_type_11 gg_type_11;
#pragma pack(push, 1)
struct gg_type_11 {
  uint16_t f21;
  gg_type_6 f22;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_11 data[1]; } gg_type_12;
#pragma pack(pop)
typedef struct gg_type_13 gg_type_13;
#pragma pack(push, 1)
typedef struct { uint8_t data[64]; } gg_type_14;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_13 {
  uint16_t f23;
  gg_type_14 f24;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_13 data[1]; } gg_type_15;
#pragma pack(pop)
typedef struct gg_type_16 gg_type_16;
#pragma pack(push, 1)
typedef struct { uint8_t data[8]; } gg_type_17;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_16 {
  uint16_t f25;
  gg_type_14 f26;
  gg_type_17 f27;
  gg_type_17 f28;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_16 data[1]; } gg_type_18;
#pragma pack(pop)
typedef struct gg_type_19 gg_type_19;
#pragma pack(push, 1)
typedef struct { int8_t data[32]; } gg_type_21;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_19 {
  uint16_t f29;
  gg_type_21 f30;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_19 data[2]; } gg_type_22;
#pragma pack(pop)
typedef struct gg_type_23 gg_type_23;
#pragma pack(push, 1)
typedef struct { gg_type_21 data[8]; } gg_type_25;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int16_t data[16]; } gg_type_27;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_23 {
  float f31;
  gg_type_25 f32;
  gg_type_27 f33;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_23 data[1]; } gg_type_28;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[256]; } gg_type_29;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_17 data[1]; } gg_type_30;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[128]; } gg_type_31;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int8_t data[16]; } gg_type_32;
#pragma pack(pop)
typedef struct gg_type_33 gg_type_33;
#pragma pack(push, 1)
struct gg_type_33 {
  int32_t f34;
  int32_t f35;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_33 data[3]; } gg_type_35;
#pragma pack(pop)
typedef gg_type_1 *gg_type_36;
typedef gg_type_3 *gg_type_50;
typedef gg_type_8 *gg_type_51;
typedef gg_type_11 *gg_type_53;
#pragma pack(push, 1)
typedef struct { uint8_t data[32]; } gg_type_54;
#pragma pack(pop)
typedef struct gg_type_55 gg_type_55;
#pragma pack(push, 1)
struct gg_type_55 {
  gg_type_54 f36;
  gg_type_54 f37;
  gg_type_27 f38;
};
#pragma pack(pop)
typedef gg_type_55 *gg_type_56;
typedef gg_type_13 *gg_type_57;
typedef gg_type_14 *gg_type_58;
typedef gg_type_16 *gg_type_60;
typedef gg_type_33 *gg_type_62;
#pragma pack(push, 1)
typedef struct { float data[3]; } gg_type_63;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_64;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_65;
#pragma pack(pop)
typedef gg_type_55 *gg_type_66;
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_67;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_68;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, gg_type_1 value);
static uint8_t gg_eq_2(int64_t a, int64_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, int64_t value);
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, gg_type_3 value);
static uint8_t gg_eq_4(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint16_t value);
static uint8_t gg_eq_5(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, uint8_t value);
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, gg_type_6 value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, gg_type_9 value);
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
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, gg_type_16 value);
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_type_17 value);
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, gg_type_18 value);
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, gg_type_19 value);
static uint8_t gg_eq_20(int8_t a, int8_t b);
static uint8_t gg_equal_elements_20(gg_slice a, gg_slice b);
static void gg_render_20(gg_reference out, int8_t value);
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b);
static uint8_t gg_equal_elements_21(gg_slice a, gg_slice b);
static void gg_render_21(gg_reference out, gg_type_21 value);
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b);
static uint8_t gg_equal_elements_22(gg_slice a, gg_slice b);
static void gg_render_22(gg_reference out, gg_type_22 value);
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b);
static uint8_t gg_equal_elements_23(gg_slice a, gg_slice b);
static void gg_render_23(gg_reference out, gg_type_23 value);
static uint8_t gg_eq_24(float a, float b);
static uint8_t gg_equal_elements_24(gg_slice a, gg_slice b);
static void gg_render_24(gg_reference out, float value);
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b);
static uint8_t gg_equal_elements_25(gg_slice a, gg_slice b);
static void gg_render_25(gg_reference out, gg_type_25 value);
static uint8_t gg_eq_26(int16_t a, int16_t b);
static uint8_t gg_equal_elements_26(gg_slice a, gg_slice b);
static void gg_render_26(gg_reference out, int16_t value);
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b);
static uint8_t gg_equal_elements_27(gg_slice a, gg_slice b);
static void gg_render_27(gg_reference out, gg_type_27 value);
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b);
static uint8_t gg_equal_elements_28(gg_slice a, gg_slice b);
static void gg_render_28(gg_reference out, gg_type_28 value);
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b);
static uint8_t gg_equal_elements_29(gg_slice a, gg_slice b);
static void gg_render_29(gg_reference out, gg_type_29 value);
static uint8_t gg_eq_30(gg_type_30 a, gg_type_30 b);
static uint8_t gg_equal_elements_30(gg_slice a, gg_slice b);
static void gg_render_30(gg_reference out, gg_type_30 value);
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b);
static uint8_t gg_equal_elements_31(gg_slice a, gg_slice b);
static void gg_render_31(gg_reference out, gg_type_31 value);
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b);
static uint8_t gg_equal_elements_32(gg_slice a, gg_slice b);
static void gg_render_32(gg_reference out, gg_type_32 value);
static uint8_t gg_eq_33(gg_type_33 a, gg_type_33 b);
static uint8_t gg_equal_elements_33(gg_slice a, gg_slice b);
static void gg_render_33(gg_reference out, gg_type_33 value);
static uint8_t gg_eq_34(int32_t a, int32_t b);
static uint8_t gg_equal_elements_34(gg_slice a, gg_slice b);
static void gg_render_34(gg_reference out, int32_t value);
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b);
static uint8_t gg_equal_elements_35(gg_slice a, gg_slice b);
static void gg_render_35(gg_reference out, gg_type_35 value);
static uint8_t gg_eq_36(gg_type_36 a, gg_type_36 b);
static uint8_t gg_equal_elements_36(gg_slice a, gg_slice b);
static void gg_render_36(gg_reference out, gg_type_36 value);
static uint8_t gg_eq_37(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_37(gg_slice a, gg_slice b);
static void gg_render_37(gg_reference out, gg_slice value);
static uint8_t gg_eq_38(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_38(gg_slice a, gg_slice b);
static void gg_render_38(gg_reference out, gg_slice value);
static uint8_t gg_eq_39(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_39(gg_slice a, gg_slice b);
static void gg_render_39(gg_reference out, gg_slice value);
static uint8_t gg_eq_40(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_40(gg_slice a, gg_slice b);
static void gg_render_40(gg_reference out, gg_slice value);
static uint8_t gg_eq_41(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_41(gg_slice a, gg_slice b);
static void gg_render_41(gg_reference out, gg_slice value);
static uint8_t gg_eq_42(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_42(gg_slice a, gg_slice b);
static void gg_render_42(gg_reference out, gg_slice value);
static uint8_t gg_eq_43(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_43(gg_slice a, gg_slice b);
static void gg_render_43(gg_reference out, gg_slice value);
static uint8_t gg_eq_44(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_44(gg_slice a, gg_slice b);
static void gg_render_44(gg_reference out, gg_slice value);
static uint8_t gg_eq_45(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_45(gg_slice a, gg_slice b);
static void gg_render_45(gg_reference out, gg_slice value);
static uint8_t gg_eq_46(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_46(gg_slice a, gg_slice b);
static void gg_render_46(gg_reference out, gg_slice value);
static uint8_t gg_eq_47(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_47(gg_slice a, gg_slice b);
static void gg_render_47(gg_reference out, gg_slice value);
static uint8_t gg_eq_48(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_48(gg_slice a, gg_slice b);
static void gg_render_48(gg_reference out, gg_slice value);
static uint8_t gg_eq_49(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_49(gg_slice a, gg_slice b);
static void gg_render_49(gg_reference out, uint8_t value);
static uint8_t gg_eq_50(gg_type_50 a, gg_type_50 b);
static uint8_t gg_equal_elements_50(gg_slice a, gg_slice b);
static void gg_render_50(gg_reference out, gg_type_50 value);
static uint8_t gg_eq_51(gg_type_51 a, gg_type_51 b);
static uint8_t gg_equal_elements_51(gg_slice a, gg_slice b);
static void gg_render_51(gg_reference out, gg_type_51 value);
static uint8_t gg_eq_53(gg_type_53 a, gg_type_53 b);
static uint8_t gg_equal_elements_53(gg_slice a, gg_slice b);
static void gg_render_53(gg_reference out, gg_type_53 value);
static uint8_t gg_eq_54(gg_type_54 a, gg_type_54 b);
static uint8_t gg_equal_elements_54(gg_slice a, gg_slice b);
static void gg_render_54(gg_reference out, gg_type_54 value);
static uint8_t gg_eq_55(gg_type_55 a, gg_type_55 b);
static uint8_t gg_equal_elements_55(gg_slice a, gg_slice b);
static void gg_render_55(gg_reference out, gg_type_55 value);
static uint8_t gg_eq_56(gg_type_56 a, gg_type_56 b);
static uint8_t gg_equal_elements_56(gg_slice a, gg_slice b);
static void gg_render_56(gg_reference out, gg_type_56 value);
static uint8_t gg_eq_57(gg_type_57 a, gg_type_57 b);
static uint8_t gg_equal_elements_57(gg_slice a, gg_slice b);
static void gg_render_57(gg_reference out, gg_type_57 value);
static uint8_t gg_eq_58(gg_type_58 a, gg_type_58 b);
static uint8_t gg_equal_elements_58(gg_slice a, gg_slice b);
static void gg_render_58(gg_reference out, gg_type_58 value);
static uint8_t gg_eq_59(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_59(gg_slice a, gg_slice b);
static void gg_render_59(gg_reference out, uint32_t value);
static uint8_t gg_eq_60(gg_type_60 a, gg_type_60 b);
static uint8_t gg_equal_elements_60(gg_slice a, gg_slice b);
static void gg_render_60(gg_reference out, gg_type_60 value);
static uint8_t gg_eq_61(double a, double b);
static uint8_t gg_equal_elements_61(gg_slice a, gg_slice b);
static void gg_render_61(gg_reference out, double value);
static uint8_t gg_eq_62(gg_type_62 a, gg_type_62 b);
static uint8_t gg_equal_elements_62(gg_slice a, gg_slice b);
static void gg_render_62(gg_reference out, gg_type_62 value);
static uint8_t gg_eq_63(gg_type_63 a, gg_type_63 b);
static uint8_t gg_equal_elements_63(gg_slice a, gg_slice b);
static void gg_render_63(gg_reference out, gg_type_63 value);
static uint8_t gg_eq_64(gg_type_64 a, gg_type_64 b);
static uint8_t gg_equal_elements_64(gg_slice a, gg_slice b);
static void gg_render_64(gg_reference out, gg_type_64 value);
static uint8_t gg_eq_65(gg_type_65 a, gg_type_65 b);
static uint8_t gg_equal_elements_65(gg_slice a, gg_slice b);
static void gg_render_65(gg_reference out, gg_type_65 value);
static uint8_t gg_eq_66(gg_type_66 a, gg_type_66 b);
static uint8_t gg_equal_elements_66(gg_slice a, gg_slice b);
static void gg_render_66(gg_reference out, gg_type_66 value);
static uint8_t gg_eq_67(gg_type_67 a, gg_type_67 b);
static uint8_t gg_equal_elements_67(gg_slice a, gg_slice b);
static void gg_render_67(gg_reference out, gg_type_67 value);
static uint8_t gg_eq_68(gg_type_68 a, gg_type_68 b);
static uint8_t gg_equal_elements_68(gg_slice a, gg_slice b);
static void gg_render_68(gg_reference out, gg_type_68 value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((gg_type_1*)a.data)[i],((gg_type_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b) {
if(!gg_eq_2(a.f0,b.f0))return 0;
if(!gg_eq_2(a.f1,b.f1))return 0;
if(!gg_eq_2(a.f2,b.f2))return 0;
if(!gg_eq_2(a.f3,b.f3))return 0;
if(!gg_eq_2(a.f4,b.f4))return 0;
if(!gg_eq_2(a.f5,b.f5))return 0;
if(!gg_eq_2(a.f6,b.f6))return 0;
if(!gg_eq_2(a.f7,b.f7))return 0;
if(!gg_eq_2(a.f8,b.f8))return 0;
if(!gg_eq_2(a.f9,b.f9))return 0;
if(!gg_eq_2(a.f10,b.f10))return 0;
if(!gg_eq_2(a.f11,b.f11))return 0;
if(!gg_eq_2(a.f12,b.f12))return 0;
if(!gg_eq_2(a.f13,b.f13))return 0;
if(!gg_eq_2(a.f14,b.f14))return 0;
if(!gg_eq_2(a.f15,b.f15))return 0;
return 1;
}
static void gg_render_1(gg_reference out, gg_type_1 value) {
gg_append(out,"\124\141\163\153\040\173\040",7,1);
gg_render_2(out, value.f0);
gg_append(out,", ",2,1);
gg_render_2(out, value.f1);
gg_append(out,", ",2,1);
gg_render_2(out, value.f2);
gg_append(out,", ",2,1);
gg_render_2(out, value.f3);
gg_append(out,", ",2,1);
gg_render_2(out, value.f4);
gg_append(out,", ",2,1);
gg_render_2(out, value.f5);
gg_append(out,", ",2,1);
gg_render_2(out, value.f6);
gg_append(out,", ",2,1);
gg_render_2(out, value.f7);
gg_append(out,", ",2,1);
gg_render_2(out, value.f8);
gg_append(out,", ",2,1);
gg_render_2(out, value.f9);
gg_append(out,", ",2,1);
gg_render_2(out, value.f10);
gg_append(out,", ",2,1);
gg_render_2(out, value.f11);
gg_append(out,", ",2,1);
gg_render_2(out, value.f12);
gg_append(out,", ",2,1);
gg_render_2(out, value.f13);
gg_append(out,", ",2,1);
gg_render_2(out, value.f14);
gg_append(out,", ",2,1);
gg_render_2(out, value.f15);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((gg_type_3*)a.data)[i],((gg_type_3*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b) {
if(!gg_eq_4(a.f16,b.f16))return 0;
if(!gg_eq_6(a.f17,b.f17))return 0;
return 1;
}
static void gg_render_3(gg_reference out, gg_type_3 value) {
gg_append(out,"\121\062\040\173\040",5,1);
gg_render_4(out, value.f16);
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f17).data, 16));
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_type_6*)a.data)[i],((gg_type_6*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b) {
return gg_equal_elements_5(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_6(gg_reference out, gg_type_6 value) {
gg_text(out,gg_view((value).data, 16));
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_3(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((gg_type_3*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
if(!gg_eq_4(a.f18,b.f18))return 0;
if(!gg_eq_9(a.f19,b.f19))return 0;
if(!gg_eq_6(a.f20,b.f20))return 0;
return 1;
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
gg_append(out,"\121\065\040\173\040",5,1);
gg_render_4(out, value.f18);
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f19).data, 4));
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f20).data, 16));
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((gg_type_9*)a.data)[i],((gg_type_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b) {
return gg_equal_elements_5(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
gg_text(out,gg_view((value).data, 4));
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
return gg_equal_elements_8(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((gg_type_8*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_type_11*)a.data)[i],((gg_type_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b) {
if(!gg_eq_4(a.f21,b.f21))return 0;
if(!gg_eq_6(a.f22,b.f22))return 0;
return 1;
}
static void gg_render_11(gg_reference out, gg_type_11 value) {
gg_append(out,"\116\114\064\040\173\040",6,1);
gg_render_4(out, value.f21);
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f22).data, 16));
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return gg_equal_elements_11(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_11(out, ((gg_type_11*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
if(!gg_eq_4(a.f23,b.f23))return 0;
if(!gg_eq_14(a.f24,b.f24))return 0;
return 1;
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
gg_append(out,"\111\121\062\130\040\173\040",7,1);
gg_render_4(out, value.f23);
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f24).data, 64));
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_type_14*)a.data)[i],((gg_type_14*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b) {
return gg_equal_elements_5(gg_view((a).data, 64),gg_view((b).data, 64));
}
static void gg_render_14(gg_reference out, gg_type_14 value) {
gg_text(out,gg_view((value).data, 64));
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((gg_type_15*)a.data)[i],((gg_type_15*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(gg_type_15 a, gg_type_15 b) {
return gg_equal_elements_13(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_15(gg_reference out, gg_type_15 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_13(out, ((gg_type_13*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((gg_type_16*)a.data)[i],((gg_type_16*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b) {
if(!gg_eq_4(a.f25,b.f25))return 0;
if(!gg_eq_14(a.f26,b.f26))return 0;
if(!gg_eq_17(a.f27,b.f27))return 0;
if(!gg_eq_17(a.f28,b.f28))return 0;
return 1;
}
static void gg_render_16(gg_reference out, gg_type_16 value) {
gg_append(out,"\111\121\062\123\040\173\040",7,1);
gg_render_4(out, value.f25);
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f26).data, 64));
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f27).data, 8));
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f28).data, 8));
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_type_17*)a.data)[i],((gg_type_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b) {
return gg_equal_elements_5(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_17(gg_reference out, gg_type_17 value) {
gg_text(out,gg_view((value).data, 8));
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_type_18*)a.data)[i],((gg_type_18*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b) {
return gg_equal_elements_16(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_18(gg_reference out, gg_type_18 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((gg_type_16*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((gg_type_19*)a.data)[i],((gg_type_19*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b) {
if(!gg_eq_4(a.f29,b.f29))return 0;
if(!gg_eq_21(a.f30,b.f30))return 0;
return 1;
}
static void gg_render_19(gg_reference out, gg_type_19 value) {
gg_append(out,"\131\060\040\173\040",5,1);
gg_render_4(out, value.f29);
gg_append(out,", ",2,1);
gg_render_21(out, value.f30);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_20(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((gg_type_21*)a.data)[i],((gg_type_21*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b) {
return gg_equal_elements_20(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_21(gg_reference out, gg_type_21 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_20(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((gg_type_22*)a.data)[i],((gg_type_22*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b) {
return gg_equal_elements_19(gg_view((a).data, 2),gg_view((b).data, 2));
}
static void gg_render_22(gg_reference out, gg_type_22 value) {
int64_t i; gg_slice view=gg_view((value).data, 2);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_19(out, ((gg_type_19*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((gg_type_23*)a.data)[i],((gg_type_23*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b) {
if(!gg_eq_24(a.f31,b.f31))return 0;
if(!gg_eq_25(a.f32,b.f32))return 0;
if(!gg_eq_27(a.f33,b.f33))return 0;
return 1;
}
static void gg_render_23(gg_reference out, gg_type_23 value) {
gg_append(out,"\131\113\040\173\040",5,1);
gg_render_24(out, value.f31);
gg_append(out,", ",2,1);
gg_render_25(out, value.f32);
gg_append(out,", ",2,1);
gg_render_27(out, value.f33);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(float a, float b) {
return a == b;
}
static void gg_render_24(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((gg_type_25*)a.data)[i],((gg_type_25*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b) {
return gg_equal_elements_21(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_25(gg_reference out, gg_type_25 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_21(out, ((gg_type_21*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_26(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((gg_type_27*)a.data)[i],((gg_type_27*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b) {
return gg_equal_elements_26(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_27(gg_reference out, gg_type_27 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_26(out, ((int16_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_type_28*)a.data)[i],((gg_type_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
return gg_equal_elements_23(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_23(out, ((gg_type_23*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((gg_type_29*)a.data)[i],((gg_type_29*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b) {
return gg_equal_elements_24(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_29(gg_reference out, gg_type_29 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_24(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_30(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_30(((gg_type_30*)a.data)[i],((gg_type_30*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_30(gg_type_30 a, gg_type_30 b) {
return gg_equal_elements_17(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_30(gg_reference out, gg_type_30 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_17*)view.data)[i]).data, 8));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_31(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_31(((gg_type_31*)a.data)[i],((gg_type_31*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b) {
return gg_equal_elements_5(gg_view((a).data, 128),gg_view((b).data, 128));
}
static void gg_render_31(gg_reference out, gg_type_31 value) {
gg_text(out,gg_view((value).data, 128));
}
static uint8_t gg_equal_elements_32(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_32(((gg_type_32*)a.data)[i],((gg_type_32*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b) {
return gg_equal_elements_20(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_32(gg_reference out, gg_type_32 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_20(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_33(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_33(((gg_type_33*)a.data)[i],((gg_type_33*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_33(gg_type_33 a, gg_type_33 b) {
if(!gg_eq_34(a.f34,b.f34))return 0;
if(!gg_eq_34(a.f35,b.f35))return 0;
return 1;
}
static void gg_render_33(gg_reference out, gg_type_33 value) {
gg_append(out,"\122\157\165\164\145\040\173\040",8,1);
gg_render_34(out, value.f34);
gg_append(out,", ",2,1);
gg_render_34(out, value.f35);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_34(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_34(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_34(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_34(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_35(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_35(((gg_type_35*)a.data)[i],((gg_type_35*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b) {
return gg_equal_elements_33(gg_view((a).data, 3),gg_view((b).data, 3));
}
static void gg_render_35(gg_reference out, gg_type_35 value) {
int64_t i; gg_slice view=gg_view((value).data, 3);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_33(out, ((gg_type_33*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_36(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_36(((gg_type_36*)a.data)[i],((gg_type_36*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_36(gg_type_36 a, gg_type_36 b) {
return a == b;
}
static void gg_render_36(gg_reference out, gg_type_36 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_37(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_37(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_37(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_37(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((gg_type_3*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_38(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_38(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_38(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_38(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((gg_type_8*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_39(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_39(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_39(gg_slice a, gg_slice b) {
return gg_equal_elements_11(a,b);
}
static void gg_render_39(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_11(out, ((gg_type_11*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_40(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_40(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_40(gg_slice a, gg_slice b) {
return gg_equal_elements_13(a,b);
}
static void gg_render_40(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_13(out, ((gg_type_13*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_41(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_41(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_41(gg_slice a, gg_slice b) {
return gg_equal_elements_16(a,b);
}
static void gg_render_41(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((gg_type_16*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_42(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_42(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_42(gg_slice a, gg_slice b) {
return gg_equal_elements_19(a,b);
}
static void gg_render_42(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_19(out, ((gg_type_19*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_43(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_43(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_43(gg_slice a, gg_slice b) {
return gg_equal_elements_23(a,b);
}
static void gg_render_43(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_23(out, ((gg_type_23*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_44(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_44(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_44(gg_slice a, gg_slice b) {
return gg_equal_elements_24(a,b);
}
static void gg_render_44(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_24(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_45(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_45(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_45(gg_slice a, gg_slice b) {
return gg_equal_elements_17(a,b);
}
static void gg_render_45(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_17*)view.data)[i]).data, 8));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_46(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_46(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_46(gg_slice a, gg_slice b) {
return gg_equal_elements_5(a,b);
}
static void gg_render_46(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_47(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_47(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_47(gg_slice a, gg_slice b) {
return gg_equal_elements_20(a,b);
}
static void gg_render_47(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_20(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_48(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_48(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_48(gg_slice a, gg_slice b) {
return gg_equal_elements_33(a,b);
}
static void gg_render_48(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_33(out, ((gg_type_33*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_49(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_49(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_49(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_49(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_50(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_50(((gg_type_50*)a.data)[i],((gg_type_50*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_50(gg_type_50 a, gg_type_50 b) {
return a == b;
}
static void gg_render_50(gg_reference out, gg_type_50 value) {
gg_render_3(out,*value);
}
static uint8_t gg_equal_elements_51(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_51(((gg_type_51*)a.data)[i],((gg_type_51*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_51(gg_type_51 a, gg_type_51 b) {
return a == b;
}
static void gg_render_51(gg_reference out, gg_type_51 value) {
gg_render_8(out,*value);
}
static uint8_t gg_equal_elements_53(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_53(((gg_type_53*)a.data)[i],((gg_type_53*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_53(gg_type_53 a, gg_type_53 b) {
return a == b;
}
static void gg_render_53(gg_reference out, gg_type_53 value) {
gg_render_11(out,*value);
}
static uint8_t gg_equal_elements_54(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_54(((gg_type_54*)a.data)[i],((gg_type_54*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_54(gg_type_54 a, gg_type_54 b) {
return gg_equal_elements_5(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_54(gg_reference out, gg_type_54 value) {
gg_text(out,gg_view((value).data, 32));
}
static uint8_t gg_equal_elements_55(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_55(((gg_type_55*)a.data)[i],((gg_type_55*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_55(gg_type_55 a, gg_type_55 b) {
if(!gg_eq_54(a.f36,b.f36))return 0;
if(!gg_eq_54(a.f37,b.f37))return 0;
if(!gg_eq_27(a.f38,b.f38))return 0;
return 1;
}
static void gg_render_55(gg_reference out, gg_type_55 value) {
gg_append(out,"\104\145\143\157\144\145\144\040\173\040",10,1);
gg_quote(out, gg_view((value.f36).data, 32));
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f37).data, 32));
gg_append(out,", ",2,1);
gg_render_27(out, value.f38);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_56(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_56(((gg_type_56*)a.data)[i],((gg_type_56*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_56(gg_type_56 a, gg_type_56 b) {
return a == b;
}
static void gg_render_56(gg_reference out, gg_type_56 value) {
gg_render_55(out,*value);
}
static uint8_t gg_equal_elements_57(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_57(((gg_type_57*)a.data)[i],((gg_type_57*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_57(gg_type_57 a, gg_type_57 b) {
return a == b;
}
static void gg_render_57(gg_reference out, gg_type_57 value) {
gg_render_13(out,*value);
}
static uint8_t gg_equal_elements_58(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_58(((gg_type_58*)a.data)[i],((gg_type_58*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_58(gg_type_58 a, gg_type_58 b) {
return a == b;
}
static void gg_render_58(gg_reference out, gg_type_58 value) {
gg_render_14(out,*value);
}
static uint8_t gg_equal_elements_59(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_59(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_59(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_59(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_60(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_60(((gg_type_60*)a.data)[i],((gg_type_60*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_60(gg_type_60 a, gg_type_60 b) {
return a == b;
}
static void gg_render_60(gg_reference out, gg_type_60 value) {
gg_render_16(out,*value);
}
static uint8_t gg_equal_elements_61(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_61(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_61(double a, double b) {
return a == b;
}
static void gg_render_61(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_62(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_62(((gg_type_62*)a.data)[i],((gg_type_62*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_62(gg_type_62 a, gg_type_62 b) {
return a == b;
}
static void gg_render_62(gg_reference out, gg_type_62 value) {
gg_render_33(out,*value);
}
static uint8_t gg_equal_elements_63(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_63(((gg_type_63*)a.data)[i],((gg_type_63*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_63(gg_type_63 a, gg_type_63 b) {
return gg_equal_elements_24(gg_view((a).data, 3),gg_view((b).data, 3));
}
static void gg_render_63(gg_reference out, gg_type_63 value) {
int64_t i; gg_slice view=gg_view((value).data, 3);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_24(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_64(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_64(((gg_type_64*)a.data)[i],((gg_type_64*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_64(gg_type_64 a, gg_type_64 b) {
return gg_equal_elements_24(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_64(gg_reference out, gg_type_64 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_24(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_65(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_65(((gg_type_65*)a.data)[i],((gg_type_65*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_65(gg_type_65 a, gg_type_65 b) {
return gg_equal_elements_34(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_65(gg_reference out, gg_type_65 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_34(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_66(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_66(((gg_type_66*)a.data)[i],((gg_type_66*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_66(gg_type_66 a, gg_type_66 b) {
return a == b;
}
static void gg_render_66(gg_reference out, gg_type_66 value) {
gg_render_55(out,*value);
}
static uint8_t gg_equal_elements_67(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_67(((gg_type_67*)a.data)[i],((gg_type_67*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_67(gg_type_67 a, gg_type_67 b) {
return gg_equal_elements_34(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_67(gg_reference out, gg_type_67 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_34(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_68(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_68(((gg_type_68*)a.data)[i],((gg_type_68*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_68(gg_type_68 a, gg_type_68 b) {
return gg_equal_elements_24(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_68(gg_reference out, gg_type_68 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_24(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(gg_type_36 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base);
static void gg_function_2(int64_t p0, int64_t p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, int64_t base);
static gg_type_21 gg_function_4(gg_type_51 p0, int64_t base);
static gg_type_21 gg_function_5(gg_type_53 p0, gg_slice p1, int64_t base);
static gg_type_54 gg_function_6(gg_type_54 p148, gg_type_54 p149, int64_t base);
static gg_type_21 gg_function_7(gg_type_54 p0, int64_t base);
static void gg_function_8(int64_t p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_56 p8, int64_t base);
static uint32_t gg_function_9(gg_type_58 p0, int64_t p1, int64_t base);
static gg_type_27 gg_function_10(int16_t p189, int64_t base);
static gg_type_27 gg_function_11(int16_t p0, int16_t p1, int64_t base);
static int64_t gg_function_12(gg_type_36 p0, gg_type_62 p1, int64_t base);
static int64_t gg_function_13(gg_type_36 p0, gg_type_62 p1, int64_t base);
static gg_type_63 gg_function_14(int64_t p0, gg_slice p1, gg_slice p2, int64_t p3, gg_slice p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, gg_slice p10, gg_slice p11, gg_slice p12, int64_t base);
static void gg_function_15(int64_t p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_56 p8, int64_t base);
static void gg_function_16(int64_t p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_56 p8, int64_t base);
static gg_type_65 gg_function_17(gg_type_66 p0, gg_type_21 p1, int64_t base);
static gg_type_21 gg_function_18(gg_type_21 p0, gg_type_54 p1, int64_t base);
static gg_type_27 gg_function_19(gg_type_54 p364, gg_type_21 p365, int64_t base);
static gg_type_65 gg_function_20(gg_type_27 p372, gg_type_27 p373, int64_t base);
static gg_type_65 gg_function_21(gg_type_65 p381, gg_type_65 p382, int64_t base);
static gg_type_64 gg_function_22(float p406, gg_type_65 p407, gg_type_64 p408, int64_t base);
static float gg_function_24(gg_type_64 p0, int64_t base);
static float gg_function_25(gg_type_36 p0, int64_t p1, int64_t p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base);
static float gg_function_26(gg_slice p0, gg_slice p1, int64_t base);
static gg_type_64 gg_function_27(gg_type_50 p0, gg_slice p1, int64_t p2, gg_type_64 p3, int64_t base);
static gg_type_54 gg_function_28(gg_type_50 p0, int64_t p1, int64_t base);
static gg_type_27 gg_function_29(gg_type_27 p0, gg_type_27 p1, int64_t base);
static gg_type_64 gg_function_30(gg_type_64 p0, gg_type_64 p1, int64_t base);
static float gg_function_31(gg_slice p0, gg_slice p1, int64_t base);
static gg_type_21 gg_function_32(gg_type_51 p0, int64_t base);
static gg_type_65 gg_function_33(gg_type_21 p0, gg_type_21 p1, int64_t base);
static float gg_function_34(gg_slice p0, gg_slice p1, gg_slice p2, int64_t base);
static gg_type_21 gg_function_35(gg_type_53 p0, gg_slice p1, int64_t base);
static float gg_function_24(gg_type_64 p0, int64_t base) {
  float t0;
  int64_t t1;
  float * t2;
  float t3;
  int64_t t4;
  float * t5;
  float t6;
  float t7;
  int64_t t8;
  float * t9;
  float t10;
  int64_t t11;
  float * t12;
  float t13;
  float t14;
  float t15;
  int64_t t16;
  float * t17;
  float t18;
  int64_t t19;
  float * t20;
  float t21;
  float t22;
  int64_t t23;
  float * t24;
  float t25;
  int64_t t26;
  float * t27;
  float t28;
  float t29;
  float t30;
  gg_type_64 v0;
  v0 = p0;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1 = ((int64_t)UINT64_C(4));
  t2 = ((float *)(v0).data) + gg_index(t1, ((int64_t)UINT64_C(8)));
  t3 = ((float)((*t2)));
  /* FG_DIRECT_SLICE_INDEX */
  t4 = ((int64_t)UINT64_C(0));
  t5 = ((float *)(v0).data) + gg_index(t4, ((int64_t)UINT64_C(8)));
  t6 = ((float)((*t5)));
  t7 = ((float)(((t3) + (t6))));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(6));
  t9 = ((float *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(8)));
  t10 = ((float)((*t9)));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = ((int64_t)UINT64_C(2));
  t12 = ((float *)(v0).data) + gg_index(t11, ((int64_t)UINT64_C(8)));
  t13 = ((float)((*t12)));
  t14 = ((float)(((t10) + (t13))));
  t15 = ((float)(((t7) + (t14))));
  /* FG_DIRECT_SLICE_INDEX */
  t16 = ((int64_t)UINT64_C(5));
  t17 = ((float *)(v0).data) + gg_index(t16, ((int64_t)UINT64_C(8)));
  t18 = ((float)((*t17)));
  /* FG_DIRECT_SLICE_INDEX */
  t19 = ((int64_t)UINT64_C(1));
  t20 = ((float *)(v0).data) + gg_index(t19, ((int64_t)UINT64_C(8)));
  t21 = ((float)((*t20)));
  t22 = ((float)(((t18) + (t21))));
  /* FG_DIRECT_SLICE_INDEX */
  t23 = ((int64_t)UINT64_C(7));
  t24 = ((float *)(v0).data) + gg_index(t23, ((int64_t)UINT64_C(8)));
  t25 = ((float)((*t24)));
  /* FG_DIRECT_SLICE_INDEX */
  t26 = ((int64_t)UINT64_C(3));
  t27 = ((float *)(v0).data) + gg_index(t26, ((int64_t)UINT64_C(8)));
  t28 = ((float)((*t27)));
  t29 = ((float)(((t25) + (t28))));
  t30 = ((float)(((t22) + (t29))));
  t0 = ((float)(((t15) + (t30))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:fma32 */
static gg_type_64 gg_function_22(float p406, gg_type_65 p407, gg_type_64 p408, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__) && defined(__FMA__)
  { gg_type_64 packed_result;
    _mm256_storeu_ps(packed_result.data, _mm256_fmadd_ps(_mm256_set1_ps(p406), _mm256_cvtepi32_ps(_mm256_loadu_si256((const __m256i*)p407.data)), _mm256_loadu_ps(p408.data)));
    return packed_result;
  }
#endif
  gg_type_64 t0;
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
  float v406;
  gg_type_65 v407;
  gg_type_64 v408;
  gg_type_64 v409;
  int64_t v411;
  v406 = p406;
  v407 = p407;
  v408 = p408;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v409).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v411 = t4;
  {
  t6 = gg_view((v409).data, 8);
  t7 = v411;
  t8 = ((float *)t6.data) + gg_index(t7, t6.len);
  t9 = ((float)(v406));
  t10 = gg_view((v407).data, 8);
  t11 = v411;
  t12 = ((int32_t *)t10.data) + gg_index(t11, t10.len);
  t13 = ((float)(((float)((*t12)))));
  t14 = gg_view((v408).data, 8);
  t15 = v411;
  t16 = ((float *)t14.data) + gg_index(t15, t14.len);
  t17 = ((float)((*t16)));
  t18 = fmaf(t9, t13, t17);
  (*t8) = ((float)(t18));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v409;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:add32 */
static gg_type_65 gg_function_21(gg_type_65 p381, gg_type_65 p382, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_65 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)p381.data), _mm256_loadu_si256((const __m256i*)p382.data)));
    return packed_result;
  }
#endif
  gg_type_65 t0;
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
  gg_type_65 v381;
  gg_type_65 v382;
  gg_type_65 v383;
  int64_t v385;
  v381 = p381;
  v382 = p382;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v383).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v385 = t4;
  {
  t6 = gg_view((v383).data, 8);
  t7 = v385;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v381).data, 8);
  t10 = v385;
  t11 = ((int32_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int32_t)((*t11)));
  t13 = gg_view((v382).data, 8);
  t14 = v385;
  t15 = ((int32_t *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int32_t)((*t15)));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t12) + (uint64_t)(t16)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v383;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:madd16 */
static gg_type_65 gg_function_20(gg_type_27 p372, gg_type_27 p373, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_65 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_madd_epi16(_mm256_loadu_si256((const __m256i*)p372.data), _mm256_loadu_si256((const __m256i*)p373.data)));
    return packed_result;
  }
#endif
  gg_type_65 t0;
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
  gg_type_27 v372;
  gg_type_27 v373;
  gg_type_65 v374;
  int64_t v376;
  int64_t v377;
  v372 = p372;
  v373 = p373;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v374).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v376 = t4;
  {
  t6 = gg_view((v372).data, 16);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v376));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((int16_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int64_t)(((int64_t)((*t10)))));
  t12 = gg_view((v373).data, 16);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v376));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((int16_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int64_t)(((int64_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v372).data, 16);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v376));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((int16_t *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int64_t)(((int64_t)((*t25)))));
  t27 = gg_view((v373).data, 16);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v376));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((int16_t *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int64_t)(((int64_t)((*t33)))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v377 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  t36 = gg_view((v374).data, 8);
  t37 = v376;
  t38 = ((int32_t *)t36.data) + gg_index(t37, t36.len);
  (*t38) = ((int32_t)(((int32_t)(v377))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v374;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:maddubs */
static gg_type_27 gg_function_19(gg_type_54 p364, gg_type_21 p365, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_27 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_maddubs_epi16(_mm256_loadu_si256((const __m256i*)p364.data), _mm256_loadu_si256((const __m256i*)p365.data)));
    return packed_result;
  }
#endif
  gg_type_27 t0;
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
  gg_type_54 v364;
  gg_type_21 v365;
  gg_type_27 v366;
  int64_t v368;
  int32_t v369;
  v364 = p364;
  v365 = p365;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v366).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v368 = t4;
  {
  t6 = gg_view((v364).data, 32);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v368));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((uint8_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int32_t)(((int32_t)((*t10)))));
  t12 = gg_view((v365).data, 32);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v368));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((int8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int32_t)(((int32_t)((*t16)))));
  t18 = ((int32_t)(((int32_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v364).data, 32);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v368));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((uint8_t *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int32_t)(((int32_t)((*t25)))));
  t27 = gg_view((v365).data, 32);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v368));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((int8_t *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int32_t)(((int32_t)((*t33)))));
  t35 = ((int32_t)(((int32_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v369 = ((int32_t)(((int32_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  t36 = gg_view((v366).data, 16);
  t37 = v368;
  t38 = ((int16_t *)t36.data) + gg_index(t37, t36.len);
  t40 = ((int32_t)(v369));
  t41 = ((int64_t)UINT64_C(32768));
  t42 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t41)))));
  t43 = ((t40) < (t42));
  if (t43) {
  {
  t44 = ((int64_t)UINT64_C(32768));
  t39 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t44)))));
  }
  } else {
  t45 = ((int32_t)(v369));
  t46 = ((int32_t)(((int64_t)UINT64_C(32767))));
  t47 = ((t45) > (t46));
  if (t47) {
  {
  t39 = ((int64_t)(((int64_t)UINT64_C(32767))));
  }
  } else {
  {
  t39 = ((int64_t)(v369));
  }
  }
  }
  (*t38) = ((int16_t)(((int16_t)(t39))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v366;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_21 gg_function_18(gg_type_21 p0, gg_type_54 p1, int64_t base) {
  gg_type_21 t0;
  int8_t t1;
  int8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int8_t * t7;
  int64_t t8;
  int8_t * t9;
  uint8_t t10;
  int64_t t11;
  uint8_t * t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  uint8_t * t16;
  uint8_t t17;
  gg_type_21 v0;
  gg_type_54 v1;
  gg_type_21 v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int8_t)(t1));
  for (t3=0; t3<32; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((int8_t *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v3;
  t9 = ((int8_t *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(32)));
  t10 = ((uint8_t)(((uint8_t)((*t9)))));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v3;
  t12 = ((uint8_t *)(v1).data) + gg_index(t11, ((int64_t)UINT64_C(32)));
  t13 = ((uint8_t)((*t12)));
  t14 = ((uint8_t)(((t10) ^ (t13))));
  /* FG_DIRECT_SLICE_INDEX */
  t15 = v3;
  t16 = ((uint8_t *)(v1).data) + gg_index(t15, ((int64_t)UINT64_C(32)));
  t17 = ((uint8_t)((*t16)));
  (*t7) = ((int8_t)(((int8_t)(((uint8_t)((uint64_t)(t14) - (uint64_t)(t17)))))));
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
static gg_type_65 gg_function_17(gg_type_66 p0, gg_type_21 p1, int64_t base) {
  gg_type_65 t0;
  gg_type_27 t1;
  gg_type_54 t2;
  gg_type_21 t3;
  gg_type_54 t4;
  gg_type_21 t5;
  gg_type_27 t6;
  gg_type_66 v0;
  gg_type_21 v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((*(v0))).f38;
  t2 = ((*(v0))).f36;
  t3 = v1;
  t4 = ((*(v0))).f37;
  t5 = gg_function_18(t3, t4, base + 0);
  t6 = gg_function_19(t2, t5, base + 0);
  t0 = gg_function_20(t1, t6, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_27 gg_function_11(int16_t p0, int16_t p1, int64_t base) {
  gg_type_27 t0;
  int16_t t1;
  int16_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int16_t * t7;
  gg_slice t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int16_t * t12;
  int16_t v0;
  int16_t v1;
  gg_type_27 v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((int16_t *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(16)));
  (*t7) = ((int16_t)(v0));
  t8 = gg_view((v2).data, 16);
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(8))));
  t11 = ((int64_t)((uint64_t)(t9) + (uint64_t)(t10)));
  t12 = ((int16_t *)t8.data) + gg_index(t11, t8.len);
  (*t12) = ((int16_t)(v1));
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
/* FG_PACKED_MATCH:splat16 */
static gg_type_27 gg_function_10(int16_t p189, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_27 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_set1_epi16(p189));
    return packed_result;
  }
#endif
  gg_type_27 t0;
  int16_t t1;
  int16_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int16_t * t8;
  int16_t v189;
  gg_type_27 v190;
  int64_t v192;
  v189 = p189;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v190).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v192 = t4;
  {
  t6 = gg_view((v190).data, 16);
  t7 = v192;
  t8 = ((int16_t *)t6.data) + gg_index(t7, t6.len);
  (*t8) = ((int16_t)(v189));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v190;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint32_t gg_function_9(gg_type_58 p0, int64_t p1, int64_t base) {
  uint32_t t0;
  int64_t t1;
  uint8_t * t2;
  uint32_t t3;
  gg_slice t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t * t8;
  uint32_t t9;
  int64_t t10;
  uint32_t t11;
  uint32_t t12;
  gg_slice t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t * t17;
  uint32_t t18;
  int64_t t19;
  uint32_t t20;
  uint32_t t21;
  gg_slice t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t * t26;
  uint32_t t27;
  int64_t t28;
  uint32_t t29;
  gg_type_58 v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1 = v1;
  t2 = ((uint8_t *)((*(v0))).data) + gg_index(t1, ((int64_t)UINT64_C(64)));
  t3 = ((uint32_t)(((uint32_t)((*t2)))));
  t4 = gg_view(((*(v0))).data, 64);
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)((uint64_t)(t5) + (uint64_t)(t6)));
  t8 = ((uint8_t *)t4.data) + gg_index(t7, t4.len);
  t9 = ((uint32_t)(((uint32_t)((*t8)))));
  t10 = ((int64_t)(((int64_t)UINT64_C(8))));
  t11 = ((uint32_t)(((uint32_t)((uint64_t)(t9) << ((uint64_t)(t10) & 31)))));
  t12 = ((uint32_t)(((t3) | (t11))));
  t13 = gg_view(((*(v0))).data, 64);
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  t16 = ((int64_t)((uint64_t)(t14) + (uint64_t)(t15)));
  t17 = ((uint8_t *)t13.data) + gg_index(t16, t13.len);
  t18 = ((uint32_t)(((uint32_t)((*t17)))));
  t19 = ((int64_t)(((int64_t)UINT64_C(16))));
  t20 = ((uint32_t)(((uint32_t)((uint64_t)(t18) << ((uint64_t)(t19) & 31)))));
  t21 = ((uint32_t)(((t12) | (t20))));
  t22 = gg_view(((*(v0))).data, 64);
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(3))));
  t25 = ((int64_t)((uint64_t)(t23) + (uint64_t)(t24)));
  t26 = ((uint8_t *)t22.data) + gg_index(t25, t22.len);
  t27 = ((uint32_t)(((uint32_t)((*t26)))));
  t28 = ((int64_t)(((int64_t)UINT64_C(24))));
  t29 = ((uint32_t)(((uint32_t)((uint64_t)(t27) << ((uint64_t)(t28) & 31)))));
  t0 = ((uint32_t)(((t21) | (t29))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_16(int64_t p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_56 p8, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  gg_type_13 * t4;
  gg_type_58 t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint32_t t11;
  uint32_t t12;
  int64_t t13;
  uint32_t t14;
  uint32_t t15;
  uint32_t t16;
  int16_t t17;
  gg_type_27 t18;
  int64_t t19;
  int64_t t20;
  gg_slice t21;
  gg_slice t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t * t28;
  int64_t t29;
  gg_type_17 * t30;
  gg_slice t31;
  uint32_t t32;
  int64_t t33;
  int64_t t34;
  uint32_t t35;
  uint32_t t36;
  uint32_t t37;
  int64_t t38;
  uint8_t * t39;
  int64_t t40;
  int64_t t41;
  gg_slice t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint8_t * t48;
  int64_t t49;
  uint8_t * t50;
  gg_slice t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t * t57;
  int64_t t58;
  uint8_t t59;
  uint8_t t60;
  uint8_t t61;
  uint8_t t62;
  uint8_t t63;
  uint8_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  gg_type_16 * t69;
  int64_t t70;
  uint8_t * t71;
  uint8_t t72;
  uint8_t t73;
  int16_t t74;
  int16_t t75;
  int16_t t76;
  int16_t t77;
  int16_t t78;
  int64_t t79;
  uint8_t * t80;
  uint8_t t81;
  int64_t t82;
  int16_t t83;
  int16_t t84;
  int16_t t85;
  int16_t t86;
  int16_t t87;
  gg_type_27 t88;
  int64_t t89;
  int64_t t90;
  gg_slice t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t * t97;
  uint16_t t98;
  int64_t t99;
  uint8_t * t100;
  uint16_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint16_t t106;
  uint16_t t107;
  uint16_t t108;
  uint16_t t109;
  gg_slice t110;
  int64_t t111;
  gg_type_17 * t112;
  gg_slice t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint8_t * t121;
  int64_t t122;
  int64_t t123;
  gg_slice t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  uint8_t * t130;
  int64_t t131;
  uint8_t * t132;
  gg_slice t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  uint8_t * t139;
  int64_t t140;
  uint8_t t141;
  uint8_t t142;
  uint8_t t143;
  uint8_t t144;
  uint8_t t145;
  uint8_t t146;
  uint8_t t147;
  int64_t v0;
  gg_slice v1;
  gg_slice v2;
  int64_t v3;
  int64_t v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_type_56 v8;
  gg_type_57 v9;
  uint32_t v10;
  int64_t v11;
  gg_type_17 v12;
  uint8_t v13;
  int64_t v14;
  gg_type_60 v15;
  int64_t v16;
  uint16_t v17;
  gg_type_17 v18;
  uint8_t v19;
  int64_t v20;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(3))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t3 = v3;
  t4 = ((gg_type_13 *)(v1).data) + gg_index(t3, (v1).len);
  v9 = t4;
  t5 = &(((*(v9))).f24);
  t6 = ((int64_t)(v4));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = ((int64_t)(((int64_t)UINT64_C(4))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  v10 = gg_function_9(t5, t10, base + 0);
  t11 = ((uint32_t)(((int64_t)UINT64_C(2))));
  t12 = ((uint32_t)(v10));
  t13 = ((int64_t)(((int64_t)UINT64_C(28))));
  t14 = ((uint32_t)(((t12) >> ((uint64_t)(t13) & 31))));
  t15 = ((uint32_t)(((uint32_t)((uint64_t)(t11) * (uint64_t)(t14)))));
  t16 = ((uint32_t)(((int64_t)UINT64_C(1))));
  t17 = ((int16_t)(((int16_t)(((uint32_t)((uint64_t)(t15) + (uint64_t)(t16)))))));
  t18 = gg_function_10(t17, base + 0);
  ((*(v8))).f38 = t18;
  t20 = ((int64_t)UINT64_C(4));
  t19 = 0;
label0:;
  if (t19 >= t20) goto label2;
  v11 = t19;
  {
  t21 = v5;
  t22 = gg_view((((*(v9))).f24).data, 64);
  t23 = ((int64_t)(v4));
  t24 = ((int64_t)(((int64_t)UINT64_C(8))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t26 = ((int64_t)(v11));
  t27 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  t28 = ((uint8_t *)t22.data) + gg_index(t27, t22.len);
  t29 = ((int64_t)((*t28)));
  t30 = ((gg_type_17 *)t21.data) + gg_index(t29, t21.len);
  v12 = (*t30);
  t31 = v7;
  t32 = ((uint32_t)(v10));
  t33 = ((int64_t)(((int64_t)UINT64_C(7))));
  t34 = ((int64_t)(v11));
  t35 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))))));
  t36 = ((uint32_t)(((t32) >> ((uint64_t)(t35) & 31))));
  t37 = ((uint32_t)(((int64_t)UINT64_C(127))));
  t38 = ((int64_t)(((t36) & (t37))));
  t39 = ((uint8_t *)t31.data) + gg_index(t38, t31.len);
  v13 = ((uint8_t)((*t39)));
  t41 = ((int64_t)UINT64_C(8));
  t40 = 0;
label3:;
  if (t40 >= t41) goto label5;
  v14 = t40;
  {
  t42 = gg_view((((*(v8))).f36).data, 32);
  t43 = ((int64_t)(v11));
  t44 = ((int64_t)(((int64_t)UINT64_C(8))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((int64_t)(v14));
  t47 = ((int64_t)((uint64_t)(t45) + (uint64_t)(t46)));
  t48 = ((uint8_t *)t42.data) + gg_index(t47, t42.len);
  /* FG_DIRECT_SLICE_INDEX */
  t49 = v14;
  t50 = ((uint8_t *)(v12).data) + gg_index(t49, ((int64_t)UINT64_C(8)));
  (*t48) = ((uint8_t)((*t50)));
  t51 = gg_view((((*(v8))).f37).data, 32);
  t52 = ((int64_t)(v11));
  t53 = ((int64_t)(((int64_t)UINT64_C(8))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  t55 = ((int64_t)(v14));
  t56 = ((int64_t)((uint64_t)(t54) + (uint64_t)(t55)));
  t57 = ((uint8_t *)t51.data) + gg_index(t56, t51.len);
  t59 = ((uint8_t)(v13));
  t60 = ((uint8_t)(((uint8_t)(v14))));
  t61 = ((uint8_t)(((t59) >> ((uint64_t)(t60) & 7))));
  t62 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t63 = ((uint8_t)(((t61) & (t62))));
  t64 = ((uint8_t)(((int64_t)UINT64_C(0))));
  t65 = ((t63) != (t64));
  if (t65) {
  {
  t58 = ((int64_t)(((int64_t)UINT64_C(255))));
  }
  } else {
  {
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  (*t57) = ((uint8_t)(((uint8_t)(t58))));
  }
label4:;
  t40++; goto label3;
label5:;
  }
label1:;
  t19++; goto label0;
label2:;
  }
  } else {
  {
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t66) == (t67)))) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t68 = v3;
  t69 = ((gg_type_16 *)(v2).data) + gg_index(t68, (v2).len);
  v15 = t69;
  /* FG_DIRECT_SLICE_INDEX */
  t70 = v4;
  t71 = ((uint8_t *)(((*(v15))).f28).data) + gg_index(t70, ((int64_t)UINT64_C(8)));
  t72 = ((uint8_t)((*t71)));
  t73 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t74 = ((int16_t)(((int16_t)(((t72) & (t73))))));
  t75 = ((int16_t)(((int64_t)UINT64_C(2))));
  t76 = ((int16_t)(((int16_t)((uint64_t)(t74) * (uint64_t)(t75)))));
  t77 = ((int16_t)(((int64_t)UINT64_C(1))));
  t78 = ((int16_t)(((int16_t)((uint64_t)(t76) + (uint64_t)(t77)))));
  /* FG_DIRECT_SLICE_INDEX */
  t79 = v4;
  t80 = ((uint8_t *)(((*(v15))).f28).data) + gg_index(t79, ((int64_t)UINT64_C(8)));
  t81 = ((uint8_t)((*t80)));
  t82 = ((int64_t)(((int64_t)UINT64_C(4))));
  t83 = ((int16_t)(((int16_t)(((t81) >> ((uint64_t)(t82) & 7))))));
  t84 = ((int16_t)(((int64_t)UINT64_C(2))));
  t85 = ((int16_t)(((int16_t)((uint64_t)(t83) * (uint64_t)(t84)))));
  t86 = ((int16_t)(((int64_t)UINT64_C(1))));
  t87 = ((int16_t)(((int16_t)((uint64_t)(t85) + (uint64_t)(t86)))));
  t88 = gg_function_11(t78, t87, base + 0);
  ((*(v8))).f38 = t88;
  t90 = ((int64_t)UINT64_C(4));
  t89 = 0;
label6:;
  if (t89 >= t90) goto label8;
  v16 = t89;
  {
  t91 = gg_view((((*(v15))).f26).data, 64);
  t92 = ((int64_t)(v4));
  t93 = ((int64_t)(((int64_t)UINT64_C(4))));
  t94 = ((int64_t)(((int64_t)((uint64_t)(t92) * (uint64_t)(t93)))));
  t95 = ((int64_t)(v16));
  t96 = ((int64_t)((uint64_t)(t94) + (uint64_t)(t95)));
  t97 = ((uint8_t *)t91.data) + gg_index(t96, t91.len);
  t98 = ((uint16_t)(((uint16_t)((*t97)))));
  /* FG_DIRECT_SLICE_INDEX */
  t99 = v4;
  t100 = ((uint8_t *)(((*(v15))).f27).data) + gg_index(t99, ((int64_t)UINT64_C(8)));
  t101 = ((uint16_t)(((uint16_t)((*t100)))));
  t102 = ((int64_t)(((int64_t)UINT64_C(8))));
  t103 = ((int64_t)(((int64_t)UINT64_C(2))));
  t104 = ((int64_t)(v16));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t104)))));
  t106 = ((uint16_t)(((uint16_t)(((int64_t)((uint64_t)(t102) - (uint64_t)(t105)))))));
  t107 = ((uint16_t)(((uint16_t)((uint64_t)(t101) << ((uint64_t)(t106) & 15)))));
  t108 = ((uint16_t)(((int64_t)UINT64_C(768))));
  t109 = ((uint16_t)(((t107) & (t108))));
  v17 = ((uint16_t)(((t98) | (t109))));
  t110 = v6;
  t111 = ((int64_t)(v17));
  t112 = ((gg_type_17 *)t110.data) + gg_index(t111, t110.len);
  v18 = (*t112);
  t113 = gg_view((((*(v15))).f26).data, 64);
  t114 = ((int64_t)(((int64_t)UINT64_C(32))));
  t115 = ((int64_t)(v4));
  t116 = ((int64_t)(((int64_t)UINT64_C(4))));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) * (uint64_t)(t116)))));
  t118 = ((int64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t117)))));
  t119 = ((int64_t)(v16));
  t120 = ((int64_t)((uint64_t)(t118) + (uint64_t)(t119)));
  t121 = ((uint8_t *)t113.data) + gg_index(t120, t113.len);
  v19 = ((uint8_t)((*t121)));
  t123 = ((int64_t)UINT64_C(8));
  t122 = 0;
label9:;
  if (t122 >= t123) goto label11;
  v20 = t122;
  {
  t124 = gg_view((((*(v8))).f36).data, 32);
  t125 = ((int64_t)(v16));
  t126 = ((int64_t)(((int64_t)UINT64_C(8))));
  t127 = ((int64_t)(((int64_t)((uint64_t)(t125) * (uint64_t)(t126)))));
  t128 = ((int64_t)(v20));
  t129 = ((int64_t)((uint64_t)(t127) + (uint64_t)(t128)));
  t130 = ((uint8_t *)t124.data) + gg_index(t129, t124.len);
  /* FG_DIRECT_SLICE_INDEX */
  t131 = v20;
  t132 = ((uint8_t *)(v18).data) + gg_index(t131, ((int64_t)UINT64_C(8)));
  (*t130) = ((uint8_t)((*t132)));
  t133 = gg_view((((*(v8))).f37).data, 32);
  t134 = ((int64_t)(v16));
  t135 = ((int64_t)(((int64_t)UINT64_C(8))));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t134) * (uint64_t)(t135)))));
  t137 = ((int64_t)(v20));
  t138 = ((int64_t)((uint64_t)(t136) + (uint64_t)(t137)));
  t139 = ((uint8_t *)t133.data) + gg_index(t138, t133.len);
  t141 = ((uint8_t)(v19));
  t142 = ((uint8_t)(((uint8_t)(v20))));
  t143 = ((uint8_t)(((t141) >> ((uint64_t)(t142) & 7))));
  t144 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t145 = ((uint8_t)(((t143) & (t144))));
  t146 = ((uint8_t)(((int64_t)UINT64_C(0))));
  t147 = ((t145) != (t146));
  if (t147) {
  {
  t140 = ((int64_t)(((int64_t)UINT64_C(255))));
  }
  } else {
  {
  t140 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  (*t139) = ((uint8_t)(((uint8_t)(t140))));
  }
label10:;
  t122++; goto label9;
label11:;
  }
label7:;
  t89++; goto label6;
label8:;
  }
  }
  }
gg_done:;
}
static void gg_function_15(int64_t p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_56 p8, int64_t base) {
  /* FG_SHARED_BODY:16 */
  gg_function_16(p0, p1, p2, p3, p4, p5, p6, p7, p8, base);
}
static gg_type_63 gg_function_14(int64_t p0, gg_slice p1, gg_slice p2, int64_t p3, gg_slice p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, gg_slice p10, gg_slice p11, gg_slice p12, int64_t base) {
  gg_type_63 t0;
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
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  float t31;
  float t32;
  int64_t t33;
  float t34;
  float t35;
  int64_t t36;
  float t37;
  float t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int32_t t42;
  int32_t t43;
  int64_t t44;
  int32_t t45;
  int32_t t46;
  int64_t t47;
  int32_t t48;
  int32_t t49;
  int64_t t50;
  int32_t t51;
  int32_t t52;
  int64_t t53;
  int32_t t54;
  int32_t t55;
  int64_t t56;
  int32_t t57;
  int32_t t58;
  int64_t t59;
  uint8_t t60;
  uint8_t t61;
  int64_t t62;
  uint8_t t63;
  uint8_t t64;
  int64_t t65;
  int16_t t66;
  int16_t t67;
  int64_t t68;
  uint8_t t69;
  uint8_t t70;
  int64_t t71;
  uint8_t t72;
  uint8_t t73;
  int64_t t74;
  int16_t t75;
  int16_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  gg_slice t81;
  gg_slice t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  gg_slice t89;
  gg_slice t90;
  gg_slice t91;
  gg_type_56 t92;
  int64_t t93;
  gg_slice t94;
  gg_slice t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  gg_slice t104;
  gg_slice t105;
  gg_slice t106;
  gg_type_56 t107;
  gg_type_65 t108;
  gg_type_66 t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  gg_type_23 * t114;
  gg_slice t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  gg_type_21 * t119;
  gg_type_21 t120;
  gg_type_65 t121;
  gg_type_65 t122;
  gg_type_66 t123;
  gg_slice t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  gg_type_23 * t128;
  gg_slice t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  gg_type_21 * t135;
  gg_type_21 t136;
  gg_type_65 t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
  gg_type_65 t141;
  gg_type_66 t142;
  gg_slice t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  gg_type_23 * t147;
  gg_slice t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  gg_type_21 * t152;
  gg_type_21 t153;
  gg_type_65 t154;
  gg_type_65 t155;
  gg_type_66 t156;
  gg_slice t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  gg_type_23 * t161;
  gg_slice t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  gg_type_21 * t168;
  gg_type_21 t169;
  gg_type_65 t170;
  int64_t t171;
  int64_t t172;
  uint8_t t173;
  gg_type_65 t174;
  gg_type_66 t175;
  gg_slice t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  gg_type_23 * t180;
  gg_slice t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  gg_type_21 * t185;
  gg_type_21 t186;
  gg_type_65 t187;
  gg_type_65 t188;
  gg_type_66 t189;
  gg_slice t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  gg_type_23 * t194;
  gg_slice t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  gg_type_21 * t201;
  gg_type_21 t202;
  gg_type_65 t203;
  int64_t t204;
  int64_t t205;
  uint8_t t206;
  gg_slice t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  gg_type_13 * t211;
  uint16_t t212;
  float t213;
  gg_slice t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  gg_type_16 * t218;
  uint16_t t219;
  float t220;
  float t221;
  gg_slice t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  gg_type_23 * t226;
  float t227;
  float t228;
  gg_type_65 t229;
  gg_type_65 t230;
  gg_type_65 t231;
  gg_type_64 t232;
  int64_t t233;
  int64_t t234;
  uint8_t t235;
  float t236;
  gg_slice t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  gg_type_23 * t241;
  float t242;
  float t243;
  gg_type_65 t244;
  gg_type_65 t245;
  gg_type_65 t246;
  gg_type_64 t247;
  int64_t t248;
  int64_t t249;
  uint8_t t250;
  float t251;
  gg_slice t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  gg_type_23 * t256;
  float t257;
  float t258;
  gg_type_65 t259;
  gg_type_65 t260;
  gg_type_65 t261;
  gg_type_64 t262;
  float t263;
  float t264;
  int64_t t265;
  int64_t t266;
  float * t267;
  float t268;
  gg_type_64 t269;
  float t270;
  float t271;
  int64_t t272;
  int64_t t273;
  uint8_t t274;
  int64_t t275;
  float * t276;
  float t277;
  gg_type_64 t278;
  float t279;
  float t280;
  int64_t t281;
  int64_t t282;
  uint8_t t283;
  int64_t t284;
  float * t285;
  float t286;
  gg_type_64 t287;
  float t288;
  float t289;
  int64_t v0;
  gg_slice v1;
  gg_slice v2;
  int64_t v3;
  gg_slice v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  gg_slice v10;
  gg_slice v11;
  gg_slice v12;
  gg_type_64 v13;
  gg_type_64 v14;
  gg_type_64 v15;
  int64_t v16;
  gg_type_65 v17;
  gg_type_65 v18;
  gg_type_65 v19;
  gg_type_65 v20;
  gg_type_65 v21;
  gg_type_65 v22;
  gg_type_55 v23;
  gg_type_55 v24;
  int64_t v25;
  float v26;
  gg_type_63 v27;
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
  {
  t1 = ((int64_t)(v6));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v6));
  t6 = ((int64_t)(((int64_t)UINT64_C(3))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) >= (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)((v4).len));
  t14 = ((int64_t)(v7));
  t15 = ((int64_t)(v5));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t12 = ((t13) >= (t16));
  }
  if (!(t12)) gg_fail("assertion failed");
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(((int64_t)UINT64_C(2))));
  t19 = ((t17) >= (t18));
  if (t19) {
  {
  t20 = ((int64_t)((v4).len));
  t21 = ((int64_t)(v8));
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  if (!(((t20) >= (t23)))) gg_fail("assertion failed");
  }
  }
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)UINT64_C(3))));
  t26 = ((t24) >= (t25));
  if (t26) {
  {
  t27 = ((int64_t)((v4).len));
  t28 = ((int64_t)(v9));
  t29 = ((int64_t)(v5));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  if (!(((t27) >= (t30)))) gg_fail("assertion failed");
  }
  }
  memset(&t31, 0, sizeof(t31));
  t32 = ((float)(t31));
  for (t33=0; t33<8; t33++) (v13).data[t33] = t32;
  memset(&t34, 0, sizeof(t34));
  t35 = ((float)(t34));
  for (t36=0; t36<8; t36++) (v14).data[t36] = t35;
  memset(&t37, 0, sizeof(t37));
  t38 = ((float)(t37));
  for (t39=0; t39<8; t39++) (v15).data[t39] = t38;
  t41 = v5;
  t40 = 0;
label0:;
  if (t40 >= t41) goto label2;
  v16 = t40;
  {
  memset(&t42, 0, sizeof(t42));
  t43 = ((int32_t)(t42));
  for (t44=0; t44<8; t44++) (v17).data[t44] = t43;
  memset(&t45, 0, sizeof(t45));
  t46 = ((int32_t)(t45));
  for (t47=0; t47<8; t47++) (v18).data[t47] = t46;
  memset(&t48, 0, sizeof(t48));
  t49 = ((int32_t)(t48));
  for (t50=0; t50<8; t50++) (v19).data[t50] = t49;
  memset(&t51, 0, sizeof(t51));
  t52 = ((int32_t)(t51));
  for (t53=0; t53<8; t53++) (v20).data[t53] = t52;
  memset(&t54, 0, sizeof(t54));
  t55 = ((int32_t)(t54));
  for (t56=0; t56<8; t56++) (v21).data[t56] = t55;
  memset(&t57, 0, sizeof(t57));
  t58 = ((int32_t)(t57));
  for (t59=0; t59<8; t59++) (v22).data[t59] = t58;
  memset(&(v23), 0, sizeof(v23));
  memset(&t60, 0, sizeof(t60));
  t61 = ((uint8_t)(t60));
  for (t62=0; t62<32; t62++) ((v23).f36).data[t62] = t61;
  memset(&t63, 0, sizeof(t63));
  t64 = ((uint8_t)(t63));
  for (t65=0; t65<32; t65++) ((v23).f37).data[t65] = t64;
  memset(&t66, 0, sizeof(t66));
  t67 = ((int16_t)(t66));
  for (t68=0; t68<16; t68++) ((v23).f38).data[t68] = t67;
  memset(&(v24), 0, sizeof(v24));
  memset(&t69, 0, sizeof(t69));
  t70 = ((uint8_t)(t69));
  for (t71=0; t71<32; t71++) ((v24).f36).data[t71] = t70;
  memset(&t72, 0, sizeof(t72));
  t73 = ((uint8_t)(t72));
  for (t74=0; t74<32; t74++) ((v24).f37).data[t74] = t73;
  memset(&t75, 0, sizeof(t75));
  t76 = ((int16_t)(t75));
  for (t77=0; t77<16; t77++) ((v24).f38).data[t77] = t76;
  t79 = ((int64_t)UINT64_C(4));
  t78 = 0;
label3:;
  if (t78 >= t79) goto label5;
  v25 = t78;
  {
  t80 = ((int64_t)(v0));
  t81 = v1;
  t82 = v2;
  t83 = ((int64_t)(v3));
  t84 = ((int64_t)(v16));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t83) + (uint64_t)(t84)))));
  t86 = ((int64_t)(v25));
  t87 = ((int64_t)(((int64_t)UINT64_C(2))));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t87)))));
  t89 = v10;
  t90 = v11;
  t91 = v12;
  t92 = &(v23);
  gg_function_16(t80, t81, t82, t85, t88, t89, t90, t91, t92, base + 0);
  t93 = ((int64_t)(v0));
  t94 = v1;
  t95 = v2;
  t96 = ((int64_t)(v3));
  t97 = ((int64_t)(v16));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  t99 = ((int64_t)(v25));
  t100 = ((int64_t)(((int64_t)UINT64_C(2))));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t100)))));
  t102 = ((int64_t)(((int64_t)UINT64_C(1))));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) + (uint64_t)(t102)))));
  t104 = v10;
  t105 = v11;
  t106 = v12;
  t107 = &(v24);
  gg_function_16(t93, t94, t95, t98, t103, t104, t105, t106, t107, base + 0);
  t108 = v17;
  t109 = &(v23);
  t110 = v4;
  t111 = ((int64_t)(v7));
  t112 = ((int64_t)(v16));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((gg_type_23 *)t110.data) + gg_index(t113, t110.len);
  t115 = gg_view((((*t114)).f32).data, 8);
  t116 = ((int64_t)(v25));
  t117 = ((int64_t)(((int64_t)UINT64_C(2))));
  t118 = ((int64_t)((uint64_t)(t116) * (uint64_t)(t117)));
  t119 = ((gg_type_21 *)t115.data) + gg_index(t118, t115.len);
  t120 = (*t119);
  t121 = gg_function_17(t109, t120, base + 0);
  v17 = gg_function_21(t108, t121, base + 0);
  t122 = v18;
  t123 = &(v24);
  t124 = v4;
  t125 = ((int64_t)(v7));
  t126 = ((int64_t)(v16));
  t127 = ((int64_t)((uint64_t)(t125) + (uint64_t)(t126)));
  t128 = ((gg_type_23 *)t124.data) + gg_index(t127, t124.len);
  t129 = gg_view((((*t128)).f32).data, 8);
  t130 = ((int64_t)(v25));
  t131 = ((int64_t)(((int64_t)UINT64_C(2))));
  t132 = ((int64_t)(((int64_t)((uint64_t)(t130) * (uint64_t)(t131)))));
  t133 = ((int64_t)(((int64_t)UINT64_C(1))));
  t134 = ((int64_t)((uint64_t)(t132) + (uint64_t)(t133)));
  t135 = ((gg_type_21 *)t129.data) + gg_index(t134, t129.len);
  t136 = (*t135);
  t137 = gg_function_17(t123, t136, base + 0);
  v18 = gg_function_21(t122, t137, base + 0);
  t138 = ((int64_t)(v6));
  t139 = ((int64_t)(((int64_t)UINT64_C(2))));
  t140 = ((t138) >= (t139));
  if (t140) {
  {
  t141 = v19;
  t142 = &(v23);
  t143 = v4;
  t144 = ((int64_t)(v8));
  t145 = ((int64_t)(v16));
  t146 = ((int64_t)((uint64_t)(t144) + (uint64_t)(t145)));
  t147 = ((gg_type_23 *)t143.data) + gg_index(t146, t143.len);
  t148 = gg_view((((*t147)).f32).data, 8);
  t149 = ((int64_t)(v25));
  t150 = ((int64_t)(((int64_t)UINT64_C(2))));
  t151 = ((int64_t)((uint64_t)(t149) * (uint64_t)(t150)));
  t152 = ((gg_type_21 *)t148.data) + gg_index(t151, t148.len);
  t153 = (*t152);
  t154 = gg_function_17(t142, t153, base + 0);
  v19 = gg_function_21(t141, t154, base + 0);
  t155 = v20;
  t156 = &(v24);
  t157 = v4;
  t158 = ((int64_t)(v8));
  t159 = ((int64_t)(v16));
  t160 = ((int64_t)((uint64_t)(t158) + (uint64_t)(t159)));
  t161 = ((gg_type_23 *)t157.data) + gg_index(t160, t157.len);
  t162 = gg_view((((*t161)).f32).data, 8);
  t163 = ((int64_t)(v25));
  t164 = ((int64_t)(((int64_t)UINT64_C(2))));
  t165 = ((int64_t)(((int64_t)((uint64_t)(t163) * (uint64_t)(t164)))));
  t166 = ((int64_t)(((int64_t)UINT64_C(1))));
  t167 = ((int64_t)((uint64_t)(t165) + (uint64_t)(t166)));
  t168 = ((gg_type_21 *)t162.data) + gg_index(t167, t162.len);
  t169 = (*t168);
  t170 = gg_function_17(t156, t169, base + 0);
  v20 = gg_function_21(t155, t170, base + 0);
  }
  }
  t171 = ((int64_t)(v6));
  t172 = ((int64_t)(((int64_t)UINT64_C(3))));
  t173 = ((t171) >= (t172));
  if (t173) {
  {
  t174 = v21;
  t175 = &(v23);
  t176 = v4;
  t177 = ((int64_t)(v9));
  t178 = ((int64_t)(v16));
  t179 = ((int64_t)((uint64_t)(t177) + (uint64_t)(t178)));
  t180 = ((gg_type_23 *)t176.data) + gg_index(t179, t176.len);
  t181 = gg_view((((*t180)).f32).data, 8);
  t182 = ((int64_t)(v25));
  t183 = ((int64_t)(((int64_t)UINT64_C(2))));
  t184 = ((int64_t)((uint64_t)(t182) * (uint64_t)(t183)));
  t185 = ((gg_type_21 *)t181.data) + gg_index(t184, t181.len);
  t186 = (*t185);
  t187 = gg_function_17(t175, t186, base + 0);
  v21 = gg_function_21(t174, t187, base + 0);
  t188 = v22;
  t189 = &(v24);
  t190 = v4;
  t191 = ((int64_t)(v9));
  t192 = ((int64_t)(v16));
  t193 = ((int64_t)((uint64_t)(t191) + (uint64_t)(t192)));
  t194 = ((gg_type_23 *)t190.data) + gg_index(t193, t190.len);
  t195 = gg_view((((*t194)).f32).data, 8);
  t196 = ((int64_t)(v25));
  t197 = ((int64_t)(((int64_t)UINT64_C(2))));
  t198 = ((int64_t)(((int64_t)((uint64_t)(t196) * (uint64_t)(t197)))));
  t199 = ((int64_t)(((int64_t)UINT64_C(1))));
  t200 = ((int64_t)((uint64_t)(t198) + (uint64_t)(t199)));
  t201 = ((gg_type_21 *)t195.data) + gg_index(t200, t195.len);
  t202 = (*t201);
  t203 = gg_function_17(t189, t202, base + 0);
  v22 = gg_function_21(t188, t203, base + 0);
  }
  }
  }
label4:;
  t78++; goto label3;
label5:;
  t204 = ((int64_t)(v0));
  t205 = ((int64_t)(((int64_t)UINT64_C(3))));
  t206 = ((t204) == (t205));
  if (t206) {
  {
  t207 = v1;
  t208 = ((int64_t)(v3));
  t209 = ((int64_t)(v16));
  t210 = ((int64_t)((uint64_t)(t208) + (uint64_t)(t209)));
  t211 = ((gg_type_13 *)t207.data) + gg_index(t210, t207.len);
  t212 = ((uint16_t)(((*t211)).f23));
  t213 = fg139_half_float(t212);
  v26 = ((float)(t213));
  }
  } else {
  {
  t214 = v2;
  t215 = ((int64_t)(v3));
  t216 = ((int64_t)(v16));
  t217 = ((int64_t)((uint64_t)(t215) + (uint64_t)(t216)));
  t218 = ((gg_type_16 *)t214.data) + gg_index(t217, t214.len);
  t219 = ((uint16_t)(((*t218)).f25));
  t220 = fg139_half_float(t219);
  v26 = ((float)(t220));
  }
  }
  t221 = ((float)(v26));
  t222 = v4;
  t223 = ((int64_t)(v7));
  t224 = ((int64_t)(v16));
  t225 = ((int64_t)((uint64_t)(t223) + (uint64_t)(t224)));
  t226 = ((gg_type_23 *)t222.data) + gg_index(t225, t222.len);
  t227 = ((float)(((*t226)).f31));
  t228 = ((float)(((t221) * (t227))));
  t229 = v17;
  t230 = v18;
  t231 = gg_function_21(t229, t230, base + 0);
  t232 = v13;
  v13 = gg_function_22(t228, t231, t232, base + 0);
  t233 = ((int64_t)(v6));
  t234 = ((int64_t)(((int64_t)UINT64_C(2))));
  t235 = ((t233) >= (t234));
  if (t235) {
  {
  t236 = ((float)(v26));
  t237 = v4;
  t238 = ((int64_t)(v8));
  t239 = ((int64_t)(v16));
  t240 = ((int64_t)((uint64_t)(t238) + (uint64_t)(t239)));
  t241 = ((gg_type_23 *)t237.data) + gg_index(t240, t237.len);
  t242 = ((float)(((*t241)).f31));
  t243 = ((float)(((t236) * (t242))));
  t244 = v19;
  t245 = v20;
  t246 = gg_function_21(t244, t245, base + 0);
  t247 = v14;
  v14 = gg_function_22(t243, t246, t247, base + 0);
  }
  }
  t248 = ((int64_t)(v6));
  t249 = ((int64_t)(((int64_t)UINT64_C(3))));
  t250 = ((t248) >= (t249));
  if (t250) {
  {
  t251 = ((float)(v26));
  t252 = v4;
  t253 = ((int64_t)(v9));
  t254 = ((int64_t)(v16));
  t255 = ((int64_t)((uint64_t)(t253) + (uint64_t)(t254)));
  t256 = ((gg_type_23 *)t252.data) + gg_index(t255, t252.len);
  t257 = ((float)(((*t256)).f31));
  t258 = ((float)(((t251) * (t257))));
  t259 = v21;
  t260 = v22;
  t261 = gg_function_21(t259, t260, base + 0);
  t262 = v15;
  v15 = gg_function_22(t258, t261, t262, base + 0);
  }
  }
  }
label1:;
  t40++; goto label0;
label2:;
  memset(&t263, 0, sizeof(t263));
  t264 = ((float)(t263));
  for (t265=0; t265<3; t265++) (v27).data[t265] = t264;
  /* FG_DIRECT_SLICE_INDEX */
  t266 = ((int64_t)UINT64_C(0));
  t267 = ((float *)(v27).data) + gg_index(t266, ((int64_t)UINT64_C(3)));
  t268 = ((float)(((float)(gg_double_bits(UINT64_C(4593671619917905920))))));
  t269 = v13;
  t270 = gg_function_24(t269, base + 0);
  t271 = ((float)(t270));
  (*t267) = ((float)(((t268) * (t271))));
  t272 = ((int64_t)(v6));
  t273 = ((int64_t)(((int64_t)UINT64_C(2))));
  t274 = ((t272) >= (t273));
  if (t274) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t275 = ((int64_t)UINT64_C(1));
  t276 = ((float *)(v27).data) + gg_index(t275, ((int64_t)UINT64_C(3)));
  t277 = ((float)(((float)(gg_double_bits(UINT64_C(4593671619917905920))))));
  t278 = v14;
  t279 = gg_function_24(t278, base + 0);
  t280 = ((float)(t279));
  (*t276) = ((float)(((t277) * (t280))));
  }
  }
  t281 = ((int64_t)(v6));
  t282 = ((int64_t)(((int64_t)UINT64_C(3))));
  t283 = ((t281) >= (t282));
  if (t283) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t284 = ((int64_t)UINT64_C(2));
  t285 = ((float *)(v27).data) + gg_index(t284, ((int64_t)UINT64_C(3)));
  t286 = ((float)(((float)(gg_double_bits(UINT64_C(4593671619917905920))))));
  t287 = v15;
  t288 = gg_function_24(t287, base + 0);
  t289 = ((float)(t288));
  (*t285) = ((float)(((t286) * (t289))));
  }
  }
  t0 = v27;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_64 gg_function_30(gg_type_64 p0, gg_type_64 p1, int64_t base) {
  gg_type_64 t0;
  float t1;
  float t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  float * t7;
  int64_t t8;
  float * t9;
  float t10;
  int64_t t11;
  float * t12;
  float t13;
  gg_type_64 v0;
  gg_type_64 v1;
  gg_type_64 v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((float *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v3;
  t9 = ((float *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(8)));
  t10 = ((float)((*t9)));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v3;
  t12 = ((float *)(v1).data) + gg_index(t11, ((int64_t)UINT64_C(8)));
  t13 = ((float)((*t12)));
  (*t7) = ((float)(((t10) + (t13))));
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
static gg_type_65 gg_function_33(gg_type_21 p0, gg_type_21 p1, int64_t base) {
  gg_type_65 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  int8_t t4;
  int8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t * t10;
  int16_t t11;
  int64_t t12;
  int8_t * t13;
  int8_t t14;
  int8_t t15;
  uint8_t t16;
  int64_t t17;
  int8_t * t18;
  int16_t t19;
  int64_t t20;
  int8_t * t21;
  int64_t t22;
  int8_t * t23;
  int64_t t24;
  int64_t t25;
  int8_t * t26;
  int8_t t27;
  int8_t t28;
  uint8_t t29;
  int64_t t30;
  int8_t * t31;
  int16_t t32;
  int64_t t33;
  int8_t * t34;
  int8_t t35;
  int8_t t36;
  uint8_t t37;
  int64_t t38;
  int8_t * t39;
  int16_t t40;
  gg_type_27 t41;
  gg_type_54 t42;
  gg_type_21 t43;
  gg_type_27 t44;
  gg_type_21 v0;
  gg_type_21 v1;
  gg_type_54 v2;
  gg_type_21 v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v2).data[t3] = t2;
  memset(&t4, 0, sizeof(t4));
  t5 = ((int8_t)(t4));
  for (t6=0; t6<32; t6++) (v3).data[t6] = t5;
  t8 = ((int64_t)UINT64_C(32));
  t7 = 0;
label0:;
  if (t7 >= t8) goto label2;
  v4 = t7;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t9 = v4;
  t10 = ((uint8_t *)(v2).data) + gg_index(t9, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t12 = v4;
  t13 = ((int8_t *)(v0).data) + gg_index(t12, ((int64_t)UINT64_C(32)));
  t14 = ((int8_t)((*t13)));
  t15 = ((int8_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) < (t15));
  if (t16) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t17 = v4;
  t18 = ((int8_t *)(v0).data) + gg_index(t17, ((int64_t)UINT64_C(32)));
  t19 = ((int16_t)((*t18)));
  t11 = ((int16_t)(((int16_t)((uint64_t)0 - (uint64_t)(t19)))));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t20 = v4;
  t21 = ((int8_t *)(v0).data) + gg_index(t20, ((int64_t)UINT64_C(32)));
  t11 = ((int16_t)(((int16_t)((*t21)))));
  }
  }
  (*t10) = ((uint8_t)(((uint8_t)(t11))));
  /* FG_DIRECT_SLICE_INDEX */
  t22 = v4;
  t23 = ((int8_t *)(v3).data) + gg_index(t22, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v4;
  t26 = ((int8_t *)(v0).data) + gg_index(t25, ((int64_t)UINT64_C(32)));
  t27 = ((int8_t)((*t26)));
  t28 = ((int8_t)(((int64_t)UINT64_C(0))));
  t29 = ((t27) < (t28));
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t30 = v4;
  t31 = ((int8_t *)(v1).data) + gg_index(t30, ((int64_t)UINT64_C(32)));
  t32 = ((int16_t)((*t31)));
  t24 = ((int64_t)(((int16_t)((uint64_t)0 - (uint64_t)(t32)))));
  }
  } else {
  /* FG_DIRECT_SLICE_INDEX */
  t33 = v4;
  t34 = ((int8_t *)(v0).data) + gg_index(t33, ((int64_t)UINT64_C(32)));
  t35 = ((int8_t)((*t34)));
  t36 = ((int8_t)(((int64_t)UINT64_C(0))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t38 = v4;
  t39 = ((int8_t *)(v1).data) + gg_index(t38, ((int64_t)UINT64_C(32)));
  t24 = ((int64_t)(((int16_t)((*t39)))));
  }
  }
  }
  (*t23) = ((int8_t)(((int8_t)(t24))));
  }
label1:;
  t7++; goto label0;
label2:;
  t40 = ((int16_t)(((int64_t)UINT64_C(1))));
  t41 = gg_function_10(t40, base + 0);
  t42 = v2;
  t43 = v3;
  t44 = gg_function_19(t42, t43, base + 0);
  t0 = gg_function_20(t41, t44, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_21 gg_function_7(gg_type_54 p0, int64_t base) {
  gg_type_21 t0;
  int8_t t1;
  int8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int8_t * t7;
  int64_t t8;
  uint8_t * t9;
  gg_type_54 v0;
  gg_type_21 v1;
  int64_t v2;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int8_t)(t1));
  for (t3=0; t3<32; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((int8_t *)(v1).data) + gg_index(t6, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v2;
  t9 = ((uint8_t *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(32)));
  (*t7) = ((int8_t)(((int8_t)((*t9)))));
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
/* FG_PACKED_MATCH:shuffle8 */
static gg_type_54 gg_function_6(gg_type_54 p148, gg_type_54 p149, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_54 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)p148.data), _mm256_loadu_si256((const __m256i*)p149.data)));
    return packed_result;
  }
#endif
  gg_type_54 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  uint8_t * t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  gg_slice t17;
  int64_t t18;
  uint8_t * t19;
  gg_slice t20;
  int64_t t21;
  uint8_t * t22;
  uint8_t t23;
  uint8_t t24;
  uint8_t t25;
  uint8_t t26;
  uint8_t t27;
  gg_slice t28;
  int64_t t29;
  uint8_t * t30;
  gg_type_54 v148;
  gg_type_54 v149;
  gg_type_54 v150;
  int64_t v152;
  int64_t v153;
  v148 = p148;
  v149 = p149;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v150).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v152 = t4;
  {
  t6 = ((int64_t)(v152));
  t7 = ((int64_t)(((int64_t)UINT64_C(16))));
  t8 = ((int64_t)(((int64_t)gg_div(t6, t7))));
  t9 = ((int64_t)(((int64_t)UINT64_C(16))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t11 = gg_view((v149).data, 32);
  t12 = v152;
  t13 = ((uint8_t *)t11.data) + gg_index(t12, t11.len);
  t14 = ((uint8_t)((*t13)));
  t15 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t16 = ((int64_t)(((int64_t)(((t14) & (t15))))));
  v153 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t16)))));
  t17 = gg_view((v150).data, 32);
  t18 = v152;
  t19 = ((uint8_t *)t17.data) + gg_index(t18, t17.len);
  t20 = gg_view((v149).data, 32);
  t21 = v152;
  t22 = ((uint8_t *)t20.data) + gg_index(t21, t20.len);
  t23 = ((uint8_t)((*t22)));
  t24 = ((uint8_t)(((int64_t)UINT64_C(128))));
  t25 = ((uint8_t)(((t23) & (t24))));
  t26 = ((uint8_t)(((int64_t)UINT64_C(0))));
  t27 = ((t25) != (t26));
  if (t27) {
  {
  (*t19) = ((uint8_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t28 = gg_view((v148).data, 32);
  t29 = v153;
  t30 = ((uint8_t *)t28.data) + gg_index(t29, t28.len);
  (*t19) = ((uint8_t)((*t30)));
  }
  }
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v150;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_21 gg_function_35(gg_type_53 p0, gg_slice p1, int64_t base) {
  gg_type_21 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  uint8_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t * t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int8_t * t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t * t19;
  int64_t t20;
  uint8_t * t21;
  uint8_t t22;
  uint8_t t23;
  gg_slice t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t * t28;
  int64_t t29;
  uint8_t * t30;
  uint8_t t31;
  int64_t t32;
  gg_type_54 t33;
  gg_type_54 t34;
  gg_type_54 t35;
  gg_type_53 v0;
  gg_slice v1;
  gg_type_54 v2;
  gg_type_54 v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v2).data[t3] = t2;
  memset(&t4, 0, sizeof(t4));
  t5 = ((uint8_t)(t4));
  for (t6=0; t6<32; t6++) (v3).data[t6] = t5;
  t8 = ((int64_t)UINT64_C(32));
  t7 = 0;
label0:;
  if (t7 >= t8) goto label2;
  v4 = t7;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t9 = v4;
  t10 = ((uint8_t *)(v2).data) + gg_index(t9, ((int64_t)UINT64_C(32)));
  t11 = v1;
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(16))));
  t14 = ((int64_t)gg_mod(t12, t13));
  t15 = ((int8_t *)t11.data) + gg_index(t14, t11.len);
  (*t10) = ((uint8_t)(((uint8_t)((*t15)))));
  }
label1:;
  t7++; goto label0;
label2:;
  t17 = ((int64_t)UINT64_C(16));
  t16 = 0;
label3:;
  if (t16 >= t17) goto label5;
  v5 = t16;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t18 = v5;
  t19 = ((uint8_t *)(v3).data) + gg_index(t18, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t20 = v5;
  t21 = ((uint8_t *)(((*(v0))).f22).data) + gg_index(t20, ((int64_t)UINT64_C(16)));
  t22 = ((uint8_t)((*t21)));
  t23 = ((uint8_t)(((int64_t)UINT64_C(15))));
  (*t19) = ((uint8_t)(((t22) & (t23))));
  t24 = gg_view((v3).data, 32);
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(((int64_t)UINT64_C(16))));
  t27 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  t28 = ((uint8_t *)t24.data) + gg_index(t27, t24.len);
  /* FG_DIRECT_SLICE_INDEX */
  t29 = v5;
  t30 = ((uint8_t *)(((*(v0))).f22).data) + gg_index(t29, ((int64_t)UINT64_C(16)));
  t31 = ((uint8_t)((*t30)));
  t32 = ((int64_t)(((int64_t)UINT64_C(4))));
  (*t28) = ((uint8_t)(((t31) >> ((uint64_t)(t32) & 7))));
  }
label4:;
  t16++; goto label3;
label5:;
  t33 = v2;
  t34 = v3;
  t35 = gg_function_6(t33, t34, base + 0);
  t0 = gg_function_7(t35, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_34(gg_slice p0, gg_slice p1, gg_slice p2, int64_t base) {
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
  int64_t t12;
  int64_t t13;
  gg_type_19 * t14;
  uint16_t t15;
  float t16;
  float t17;
  int64_t t18;
  gg_type_11 * t19;
  uint16_t t20;
  float t21;
  float t22;
  float t23;
  int64_t t24;
  gg_type_11 * t25;
  gg_type_53 t26;
  gg_slice t27;
  gg_type_21 t28;
  int64_t t29;
  gg_type_19 * t30;
  gg_type_21 t31;
  gg_type_65 t32;
  gg_type_64 t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  gg_type_19 * t38;
  uint16_t t39;
  float t40;
  float t41;
  gg_slice t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  gg_type_11 * t46;
  uint16_t t47;
  float t48;
  float t49;
  float t50;
  gg_slice t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  gg_type_11 * t55;
  gg_type_53 t56;
  gg_slice t57;
  gg_type_21 t58;
  gg_slice t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  gg_type_19 * t63;
  gg_type_21 t64;
  gg_type_65 t65;
  gg_type_64 t66;
  int64_t t67;
  int64_t t68;
  gg_type_64 t69;
  gg_type_64 t70;
  gg_type_64 t71;
  int64_t t72;
  int64_t t73;
  uint8_t t74;
  int64_t t75;
  int64_t t76;
  int32_t t77;
  int64_t t78;
  gg_type_19 * t79;
  gg_slice t80;
  int64_t t81;
  int8_t * t82;
  int32_t t83;
  gg_slice t84;
  int64_t t85;
  gg_type_11 * t86;
  gg_slice t87;
  int64_t t88;
  uint8_t * t89;
  uint8_t t90;
  uint8_t t91;
  int64_t t92;
  int8_t * t93;
  int32_t t94;
  int32_t t95;
  int32_t t96;
  int64_t t97;
  gg_type_19 * t98;
  gg_slice t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int8_t * t103;
  int32_t t104;
  gg_slice t105;
  int64_t t106;
  gg_type_11 * t107;
  gg_slice t108;
  int64_t t109;
  uint8_t * t110;
  uint8_t t111;
  int64_t t112;
  int64_t t113;
  int8_t * t114;
  int32_t t115;
  int32_t t116;
  int64_t t117;
  gg_type_19 * t118;
  uint16_t t119;
  float t120;
  float t121;
  int64_t t122;
  gg_type_11 * t123;
  uint16_t t124;
  float t125;
  float t126;
  float t127;
  int32_t t128;
  int32_t t129;
  float t130;
  float t131;
  float t132;
  gg_slice v0;
  gg_slice v1;
  gg_slice v2;
  gg_type_64 v3;
  gg_type_64 v4;
  int64_t v5;
  float v6;
  int32_t v7;
  int32_t v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)((v1).len));
  t2 = ((int64_t)((v0).len));
  if (!(((t1) >= (t2)))) gg_fail("assertion failed");
  memset(&t3, 0, sizeof(t3));
  t4 = ((float)(t3));
  for (t5=0; t5<8; t5++) (v3).data[t5] = t4;
  memset(&t6, 0, sizeof(t6));
  t7 = ((float)(t6));
  for (t8=0; t8<8; t8++) (v4).data[t8] = t7;
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t12 = ((int64_t)((v0).len));
  if (!(((t11) < (t12)))) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t13 = v5;
  t14 = ((gg_type_19 *)(v1).data) + gg_index(t13, (v1).len);
  t15 = ((uint16_t)(((*t14)).f29));
  t16 = fg139_half_float(t15);
  t17 = ((float)(t16));
  /* FG_DIRECT_SLICE_INDEX */
  t18 = v5;
  t19 = ((gg_type_11 *)(v0).data) + gg_index(t18, (v0).len);
  t20 = ((uint16_t)(((*t19)).f21));
  t21 = fg139_half_float(t20);
  t22 = ((float)(t21));
  t23 = ((float)(((t17) * (t22))));
  /* FG_DIRECT_SLICE_INDEX */
  t24 = v5;
  t25 = ((gg_type_11 *)(v0).data) + gg_index(t24, (v0).len);
  t26 = t25;
  t27 = v2;
  t28 = gg_function_35(t26, t27, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t29 = v5;
  t30 = ((gg_type_19 *)(v1).data) + gg_index(t29, (v1).len);
  t31 = ((*t30)).f30;
  t32 = gg_function_33(t28, t31, base + 0);
  t33 = v3;
  v3 = gg_function_22(t23, t32, t33, base + 0);
  t34 = v1;
  t35 = ((int64_t)(v5));
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((gg_type_19 *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)(((*t38)).f29));
  t40 = fg139_half_float(t39);
  t41 = ((float)(t40));
  t42 = v0;
  t43 = ((int64_t)(v5));
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  t45 = ((int64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  t46 = ((gg_type_11 *)t42.data) + gg_index(t45, t42.len);
  t47 = ((uint16_t)(((*t46)).f21));
  t48 = fg139_half_float(t47);
  t49 = ((float)(t48));
  t50 = ((float)(((t41) * (t49))));
  t51 = v0;
  t52 = ((int64_t)(v5));
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  t54 = ((int64_t)((uint64_t)(t52) + (uint64_t)(t53)));
  t55 = ((gg_type_11 *)t51.data) + gg_index(t54, t51.len);
  t56 = t55;
  t57 = v2;
  t58 = gg_function_35(t56, t57, base + 0);
  t59 = v1;
  t60 = ((int64_t)(v5));
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  t62 = ((int64_t)((uint64_t)(t60) + (uint64_t)(t61)));
  t63 = ((gg_type_19 *)t59.data) + gg_index(t62, t59.len);
  t64 = ((*t63)).f30;
  t65 = gg_function_33(t58, t64, base + 0);
  t66 = v4;
  v4 = gg_function_22(t50, t65, t66, base + 0);
  t67 = v5;
  t68 = ((int64_t)(((int64_t)UINT64_C(2))));
  v5 = ((int64_t)((uint64_t)(t67) + (uint64_t)(t68)));
  }
  goto label1;
label2:;
  t69 = v3;
  t70 = v4;
  t71 = gg_function_30(t69, t70, base + 0);
  v6 = gg_function_24(t71, base + 0);
  t72 = ((int64_t)(v5));
  t73 = ((int64_t)((v0).len));
  t74 = ((t72) < (t73));
  if (t74) {
  {
  v7 = ((int32_t)(((int64_t)UINT64_C(0))));
  v8 = ((int32_t)(((int64_t)UINT64_C(0))));
  t76 = ((int64_t)UINT64_C(16));
  t75 = 0;
label3:;
  if (t75 >= t76) goto label5;
  v9 = t75;
  {
  t77 = v7;
  /* FG_DIRECT_SLICE_INDEX */
  t78 = v5;
  t79 = ((gg_type_19 *)(v1).data) + gg_index(t78, (v1).len);
  t80 = gg_view((((*t79)).f30).data, 32);
  t81 = v9;
  t82 = ((int8_t *)t80.data) + gg_index(t81, t80.len);
  t83 = ((int32_t)(((int32_t)((*t82)))));
  t84 = v2;
  /* FG_DIRECT_SLICE_INDEX */
  t85 = v5;
  t86 = ((gg_type_11 *)(v0).data) + gg_index(t85, (v0).len);
  t87 = gg_view((((*t86)).f22).data, 16);
  t88 = v9;
  t89 = ((uint8_t *)t87.data) + gg_index(t88, t87.len);
  t90 = ((uint8_t)((*t89)));
  t91 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t92 = ((int64_t)(((t90) & (t91))));
  t93 = ((int8_t *)t84.data) + gg_index(t92, t84.len);
  t94 = ((int32_t)(((int32_t)((*t93)))));
  t95 = ((int32_t)(((int32_t)((uint64_t)(t83) * (uint64_t)(t94)))));
  v7 = ((int32_t)((uint64_t)(t77) + (uint64_t)(t95)));
  t96 = v8;
  /* FG_DIRECT_SLICE_INDEX */
  t97 = v5;
  t98 = ((gg_type_19 *)(v1).data) + gg_index(t97, (v1).len);
  t99 = gg_view((((*t98)).f30).data, 32);
  t100 = ((int64_t)(v9));
  t101 = ((int64_t)(((int64_t)UINT64_C(16))));
  t102 = ((int64_t)((uint64_t)(t100) + (uint64_t)(t101)));
  t103 = ((int8_t *)t99.data) + gg_index(t102, t99.len);
  t104 = ((int32_t)(((int32_t)((*t103)))));
  t105 = v2;
  /* FG_DIRECT_SLICE_INDEX */
  t106 = v5;
  t107 = ((gg_type_11 *)(v0).data) + gg_index(t106, (v0).len);
  t108 = gg_view((((*t107)).f22).data, 16);
  t109 = v9;
  t110 = ((uint8_t *)t108.data) + gg_index(t109, t108.len);
  t111 = ((uint8_t)((*t110)));
  t112 = ((int64_t)(((int64_t)UINT64_C(4))));
  t113 = ((int64_t)(((t111) >> ((uint64_t)(t112) & 7))));
  t114 = ((int8_t *)t105.data) + gg_index(t113, t105.len);
  t115 = ((int32_t)(((int32_t)((*t114)))));
  t116 = ((int32_t)(((int32_t)((uint64_t)(t104) * (uint64_t)(t115)))));
  v8 = ((int32_t)((uint64_t)(t96) + (uint64_t)(t116)));
  }
label4:;
  t75++; goto label3;
label5:;
  /* FG_DIRECT_SLICE_INDEX */
  t117 = v5;
  t118 = ((gg_type_19 *)(v1).data) + gg_index(t117, (v1).len);
  t119 = ((uint16_t)(((*t118)).f29));
  t120 = fg139_half_float(t119);
  t121 = ((float)(t120));
  /* FG_DIRECT_SLICE_INDEX */
  t122 = v5;
  t123 = ((gg_type_11 *)(v0).data) + gg_index(t122, (v0).len);
  t124 = ((uint16_t)(((*t123)).f21));
  t125 = fg139_half_float(t124);
  t126 = ((float)(t125));
  t127 = ((float)(((t121) * (t126))));
  t128 = ((int32_t)(v7));
  t129 = ((int32_t)(v8));
  t130 = ((float)(((float)(((int32_t)((uint64_t)(t128) + (uint64_t)(t129)))))));
  t131 = ((float)(v6));
  t132 = fmaf(t127, t130, t131);
  v6 = ((float)(t132));
  }
  }
  t0 = ((float)(v6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_21 gg_function_32(gg_type_51 p0, int64_t base) {
  gg_type_21 t0;
  int8_t t1;
  int8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t * t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  uint8_t t17;
  uint8_t t18;
  gg_slice t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t * t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  uint8_t t28;
  uint8_t t29;
  int64_t t30;
  int8_t * t31;
  int16_t t32;
  int16_t t33;
  int16_t t34;
  int16_t t35;
  int16_t t36;
  int16_t t37;
  gg_type_51 v0;
  gg_type_21 v1;
  int64_t v2;
  uint8_t v3;
  uint8_t v4;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int8_t)(t1));
  for (t3=0; t3<32; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  t6 = gg_view((((*(v0))).f20).data, 16);
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  t9 = ((int64_t)gg_mod(t7, t8));
  t10 = ((uint8_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((uint8_t)((*t10)));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(16))));
  t14 = ((int64_t)(((int64_t)gg_div(t12, t13))));
  t15 = ((int64_t)(((int64_t)UINT64_C(4))));
  t16 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))))));
  t17 = ((uint8_t)(((t11) >> ((uint64_t)(t16) & 7))));
  t18 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v3 = ((uint8_t)(((t17) & (t18))));
  t19 = gg_view((((*(v0))).f19).data, 4);
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(((int64_t)UINT64_C(8))));
  t22 = ((int64_t)gg_div(t20, t21));
  t23 = ((uint8_t *)t19.data) + gg_index(t22, t19.len);
  t24 = ((uint8_t)((*t23)));
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)UINT64_C(8))));
  t27 = ((uint8_t)(((uint8_t)(((int64_t)gg_mod(t25, t26))))));
  t28 = ((uint8_t)(((t24) >> ((uint64_t)(t27) & 7))));
  t29 = ((uint8_t)(((int64_t)UINT64_C(1))));
  v4 = ((uint8_t)(((t28) & (t29))));
  /* FG_DIRECT_SLICE_INDEX */
  t30 = v2;
  t31 = ((int8_t *)(v1).data) + gg_index(t30, ((int64_t)UINT64_C(32)));
  t32 = ((int16_t)(((int16_t)(v3))));
  t33 = ((int16_t)(((int16_t)(v4))));
  t34 = ((int16_t)(((int64_t)UINT64_C(16))));
  t35 = ((int16_t)(((int16_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((int16_t)(((int16_t)((uint64_t)(t32) + (uint64_t)(t35)))));
  t37 = ((int16_t)(((int64_t)UINT64_C(16))));
  (*t31) = ((int8_t)(((int8_t)(((int16_t)((uint64_t)(t36) - (uint64_t)(t37)))))));
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
static float gg_function_31(gg_slice p0, gg_slice p1, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  float t3;
  float t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  gg_type_8 * t9;
  uint16_t t10;
  float t11;
  float t12;
  int64_t t13;
  gg_type_19 * t14;
  uint16_t t15;
  float t16;
  float t17;
  float t18;
  int64_t t19;
  gg_type_8 * t20;
  gg_type_51 t21;
  gg_type_21 t22;
  int64_t t23;
  gg_type_19 * t24;
  gg_type_21 t25;
  gg_type_65 t26;
  gg_type_64 t27;
  gg_type_64 t28;
  gg_slice v0;
  gg_slice v1;
  gg_type_64 v2;
  int64_t v3;
  float v4;
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
  t9 = ((gg_type_8 *)(v0).data) + gg_index(t8, (v0).len);
  t10 = ((uint16_t)(((*t9)).f18));
  t11 = fg139_half_float(t10);
  t12 = ((float)(t11));
  /* FG_DIRECT_SLICE_INDEX */
  t13 = v3;
  t14 = ((gg_type_19 *)(v1).data) + gg_index(t13, (v1).len);
  t15 = ((uint16_t)(((*t14)).f29));
  t16 = fg139_half_float(t15);
  t17 = ((float)(t16));
  v4 = ((float)(((t12) * (t17))));
  t18 = ((float)(v4));
  /* FG_DIRECT_SLICE_INDEX */
  t19 = v3;
  t20 = ((gg_type_8 *)(v0).data) + gg_index(t19, (v0).len);
  t21 = t20;
  t22 = gg_function_32(t21, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t23 = v3;
  t24 = ((gg_type_19 *)(v1).data) + gg_index(t23, (v1).len);
  t25 = ((*t24)).f30;
  t26 = gg_function_33(t22, t25, base + 0);
  t27 = v2;
  v2 = gg_function_22(t18, t26, t27, base + 0);
  }
label1:;
  t6++; goto label0;
label2:;
  t28 = v2;
  t0 = gg_function_24(t28, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_27 gg_function_29(gg_type_27 p0, gg_type_27 p1, int64_t base) {
  gg_type_27 t0;
  int16_t t1;
  int16_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int16_t * t7;
  int64_t t8;
  int16_t * t9;
  int16_t t10;
  int64_t t11;
  int16_t * t12;
  int16_t t13;
  gg_type_27 v0;
  gg_type_27 v1;
  gg_type_27 v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((int16_t *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(16)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v3;
  t9 = ((int16_t *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(16)));
  t10 = ((int16_t)((*t9)));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v3;
  t12 = ((int16_t *)(v1).data) + gg_index(t11, ((int64_t)UINT64_C(16)));
  t13 = ((int16_t)((*t12)));
  (*t7) = ((int16_t)(((int16_t)((uint64_t)(t10) - (uint64_t)(t13)))));
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
static gg_type_54 gg_function_28(gg_type_50 p0, int64_t p1, int64_t base) {
  gg_type_54 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t * t7;
  gg_slice t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t * t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  uint8_t t23;
  uint8_t t24;
  gg_type_50 v0;
  int64_t v1;
  gg_type_54 v2;
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
  t8 = gg_view((((*(v0))).f17).data, 16);
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(8))));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(4))));
  t14 = ((int64_t)(((int64_t)gg_div(t12, t13))));
  t15 = ((int64_t)((uint64_t)(t11) + (uint64_t)(t14)));
  t16 = ((uint8_t *)t8.data) + gg_index(t15, t8.len);
  t17 = ((uint8_t)((*t16)));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = ((int64_t)(((int64_t)gg_mod(t18, t19))));
  t21 = ((int64_t)(((int64_t)UINT64_C(2))));
  t22 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))))));
  t23 = ((uint8_t)(((t17) >> ((uint64_t)(t22) & 7))));
  t24 = ((uint8_t)(((int64_t)UINT64_C(3))));
  (*t7) = ((uint8_t)(((t23) & (t24))));
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
static gg_type_64 gg_function_27(gg_type_50 p0, gg_slice p1, int64_t p2, gg_type_64 p3, int64_t base) {
  gg_type_64 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t * t7;
  gg_type_50 t8;
  int64_t t9;
  gg_type_54 t10;
  int64_t t11;
  gg_type_19 * t12;
  gg_type_21 t13;
  gg_type_27 t14;
  gg_type_54 t15;
  int64_t t16;
  gg_type_19 * t17;
  gg_type_21 t18;
  gg_type_27 t19;
  gg_type_50 t20;
  int64_t t21;
  gg_type_54 t22;
  gg_slice t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  gg_type_19 * t27;
  gg_type_21 t28;
  gg_type_27 t29;
  gg_type_54 t30;
  gg_slice t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  gg_type_19 * t35;
  gg_type_21 t36;
  gg_type_27 t37;
  int16_t t38;
  gg_type_27 t39;
  gg_type_27 t40;
  int16_t t41;
  gg_type_27 t42;
  gg_type_27 t43;
  uint16_t t44;
  float t45;
  float t46;
  int64_t t47;
  gg_type_19 * t48;
  uint16_t t49;
  float t50;
  float t51;
  float t52;
  gg_type_65 t53;
  gg_type_64 t54;
  float t55;
  gg_slice t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  gg_type_19 * t60;
  uint16_t t61;
  float t62;
  float t63;
  float t64;
  gg_type_65 t65;
  gg_type_64 t66;
  gg_type_50 v0;
  gg_slice v1;
  int64_t v2;
  gg_type_64 v3;
  gg_type_54 v4;
  int64_t v5;
  gg_type_27 v6;
  gg_type_27 v7;
  gg_type_65 v8;
  gg_type_65 v9;
  float v10;
  gg_type_64 v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v4).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v5 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v5;
  t7 = ((uint8_t *)(v4).data) + gg_index(t6, ((int64_t)UINT64_C(32)));
  (*t7) = ((uint8_t)(((int64_t)UINT64_C(1))));
  }
label1:;
  t4++; goto label0;
label2:;
  t8 = v0;
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = gg_function_28(t8, t9, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v2;
  t12 = ((gg_type_19 *)(v1).data) + gg_index(t11, (v1).len);
  t13 = ((*t12)).f30;
  t14 = gg_function_19(t10, t13, base + 0);
  t15 = v4;
  /* FG_DIRECT_SLICE_INDEX */
  t16 = v2;
  t17 = ((gg_type_19 *)(v1).data) + gg_index(t16, (v1).len);
  t18 = ((*t17)).f30;
  t19 = gg_function_19(t15, t18, base + 0);
  v6 = gg_function_29(t14, t19, base + 0);
  t20 = v0;
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = gg_function_28(t20, t21, base + 0);
  t23 = v1;
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((int64_t)((uint64_t)(t24) + (uint64_t)(t25)));
  t27 = ((gg_type_19 *)t23.data) + gg_index(t26, t23.len);
  t28 = ((*t27)).f30;
  t29 = gg_function_19(t22, t28, base + 0);
  t30 = v4;
  t31 = v1;
  t32 = ((int64_t)(v2));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = ((int64_t)((uint64_t)(t32) + (uint64_t)(t33)));
  t35 = ((gg_type_19 *)t31.data) + gg_index(t34, t31.len);
  t36 = ((*t35)).f30;
  t37 = gg_function_19(t30, t36, base + 0);
  v7 = gg_function_29(t29, t37, base + 0);
  t38 = ((int16_t)(((int64_t)UINT64_C(1))));
  t39 = gg_function_10(t38, base + 0);
  t40 = v6;
  v8 = gg_function_20(t39, t40, base + 0);
  t41 = ((int16_t)(((int64_t)UINT64_C(1))));
  t42 = gg_function_10(t41, base + 0);
  t43 = v7;
  v9 = gg_function_20(t42, t43, base + 0);
  t44 = ((uint16_t)(((*(v0))).f16));
  t45 = fg139_half_float(t44);
  v10 = ((float)(t45));
  t46 = ((float)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t47 = v2;
  t48 = ((gg_type_19 *)(v1).data) + gg_index(t47, (v1).len);
  t49 = ((uint16_t)(((*t48)).f29));
  t50 = fg139_half_float(t49);
  t51 = ((float)(t50));
  t52 = ((float)(((t46) * (t51))));
  t53 = v8;
  t54 = v3;
  v11 = gg_function_22(t52, t53, t54, base + 0);
  t55 = ((float)(v10));
  t56 = v1;
  t57 = ((int64_t)(v2));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t59 = ((int64_t)((uint64_t)(t57) + (uint64_t)(t58)));
  t60 = ((gg_type_19 *)t56.data) + gg_index(t59, t56.len);
  t61 = ((uint16_t)(((*t60)).f29));
  t62 = fg139_half_float(t61);
  t63 = ((float)(t62));
  t64 = ((float)(((t55) * (t63))));
  t65 = v9;
  t66 = v11;
  t0 = gg_function_22(t64, t65, t66, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_26(gg_slice p0, gg_slice p1, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  float t5;
  float t6;
  int64_t t7;
  float t8;
  float t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  gg_type_3 * t16;
  gg_type_50 t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  gg_type_64 t22;
  gg_slice t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  gg_type_3 * t27;
  gg_type_50 t28;
  gg_slice t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  gg_type_64 t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  gg_type_3 * t42;
  gg_type_50 t43;
  gg_slice t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  gg_type_64 t48;
  gg_type_64 t49;
  gg_type_64 t50;
  gg_type_64 t51;
  gg_slice v0;
  gg_slice v1;
  gg_type_64 v2;
  gg_type_64 v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)((v1).len));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)((v0).len));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  if (!(((t1) >= (t4)))) gg_fail("assertion failed");
  memset(&t5, 0, sizeof(t5));
  t6 = ((float)(t5));
  for (t7=0; t7<8; t7++) (v2).data[t7] = t6;
  memset(&t8, 0, sizeof(t8));
  t9 = ((float)(t8));
  for (t10=0; t10<8; t10++) (v3).data[t10] = t9;
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t11 = ((int64_t)(v4));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  t14 = ((int64_t)((v0).len));
  if (!(((t13) < (t14)))) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t15 = v4;
  t16 = ((gg_type_3 *)(v0).data) + gg_index(t15, (v0).len);
  t17 = t16;
  t18 = v1;
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = v2;
  v2 = gg_function_27(t17, t18, t21, t22, base + 0);
  t23 = v0;
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((int64_t)((uint64_t)(t24) + (uint64_t)(t25)));
  t27 = ((gg_type_3 *)t23.data) + gg_index(t26, t23.len);
  t28 = t27;
  t29 = v1;
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = ((int64_t)(v4));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(((int64_t)UINT64_C(2))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = v3;
  v3 = gg_function_27(t28, t29, t34, t35, base + 0);
  t36 = v4;
  t37 = ((int64_t)(((int64_t)UINT64_C(2))));
  v4 = ((int64_t)((uint64_t)(t36) + (uint64_t)(t37)));
  }
  goto label1;
label2:;
  t38 = ((int64_t)(v4));
  t39 = ((int64_t)((v0).len));
  t40 = ((t38) < (t39));
  if (t40) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t41 = v4;
  t42 = ((gg_type_3 *)(v0).data) + gg_index(t41, (v0).len);
  t43 = t42;
  t44 = v1;
  t45 = ((int64_t)(((int64_t)UINT64_C(2))));
  t46 = ((int64_t)(v4));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = v2;
  v2 = gg_function_27(t43, t44, t47, t48, base + 0);
  }
  }
  t49 = v2;
  t50 = v3;
  t51 = gg_function_30(t49, t50, base + 0);
  t0 = gg_function_24(t51, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_25(gg_type_36 p0, int64_t p1, int64_t p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  gg_slice t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  gg_slice t9;
  gg_slice t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  gg_slice t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  gg_slice t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  gg_slice t26;
  gg_slice t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  gg_slice t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  gg_slice t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  gg_slice t41;
  gg_slice t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  gg_slice t47;
  gg_slice t48;
  int64_t t49;
  gg_slice t50;
  gg_slice t51;
  int64_t t52;
  gg_slice t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  gg_slice t59;
  gg_slice t60;
  gg_slice t61;
  int64_t t62;
  float * t63;
  gg_type_36 v0;
  int64_t v1;
  int64_t v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  gg_slice v10;
  gg_slice v11;
  gg_slice v12;
  gg_slice v13;
  gg_type_63 v14;
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
  {
  t1 = ((int64_t)(((*(v0))).f1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = v3;
  t5 = v1;
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((*(v0))).f2));
  t8 = ((int64_t)((uint64_t)(t6) + (uint64_t)(t7)));
  t9 = gg_sub(t4, t5, t8, sizeof(gg_type_3));
  t10 = v8;
  t11 = v2;
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(((*(v0))).f2));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t15)));
  t17 = gg_sub(t10, t11, t16, sizeof(gg_type_19));
  t0 = gg_function_26(t9, t17, base + 0);
  goto gg_done;
  }
  }
  t18 = ((int64_t)(((*(v0))).f1));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((t18) == (t19));
  if (t20) {
  {
  t21 = v4;
  t22 = v1;
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((*(v0))).f2));
  t25 = ((int64_t)((uint64_t)(t23) + (uint64_t)(t24)));
  t26 = gg_sub(t21, t22, t25, sizeof(gg_type_8));
  t27 = v8;
  t28 = v2;
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(((*(v0))).f2));
  t31 = ((int64_t)((uint64_t)(t29) + (uint64_t)(t30)));
  t32 = gg_sub(t27, t28, t31, sizeof(gg_type_19));
  t0 = gg_function_31(t26, t32, base + 0);
  goto gg_done;
  }
  }
  t33 = ((int64_t)(((*(v0))).f1));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t36 = v5;
  t37 = v1;
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(((*(v0))).f2));
  t40 = ((int64_t)((uint64_t)(t38) + (uint64_t)(t39)));
  t41 = gg_sub(t36, t37, t40, sizeof(gg_type_11));
  t42 = v8;
  t43 = v2;
  t44 = ((int64_t)(v2));
  t45 = ((int64_t)(((*(v0))).f2));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = gg_sub(t42, t43, t46, sizeof(gg_type_19));
  t48 = v13;
  t0 = gg_function_34(t41, t47, t48, base + 0);
  goto gg_done;
  }
  }
  t49 = ((int64_t)(((*(v0))).f1));
  t50 = v6;
  t51 = v7;
  t52 = ((int64_t)(v1));
  t53 = v9;
  t54 = ((int64_t)(((*(v0))).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(1))));
  t56 = ((int64_t)(v2));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = v10;
  t60 = v11;
  t61 = v12;
  v14 = gg_function_14(t49, t50, t51, t52, t53, t54, t55, t56, t57, t58, t59, t60, t61, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t62 = ((int64_t)UINT64_C(0));
  t63 = ((float *)(v14).data) + gg_index(t62, ((int64_t)UINT64_C(3)));
  t0 = ((float)((*t63)));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_12(gg_type_36 p0, gg_type_62 p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
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
  gg_type_36 v0;
  gg_type_62 v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)(((*(v1))).f34))));
  t2 = ((int64_t)(((*(v0))).f7));
  v2 = ((int64_t)(((int64_t)gg_mod(t1, t2))));
  v3 = ((int64_t)(((int64_t)(((*(v1))).f35))));
  t3 = ((int64_t)(((*(v0))).f11));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  if (t5) {
  {
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((*(v0))).f7));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t9)))));
  t11 = ((int64_t)(((*(v0))).f12));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((*(v0))).f13));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(((*(v0))).f14));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t17)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_13(gg_type_36 p0, gg_type_62 p1, int64_t base) {
  /* FG_SHARED_BODY:12 */
  return gg_function_12(p0, p1, base);
}
static void gg_function_8(int64_t p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_56 p8, int64_t base) {
  /* FG_SHARED_BODY:16 */
  gg_function_16(p0, p1, p2, p3, p4, p5, p6, p7, p8, base);
}
static gg_type_21 gg_function_5(gg_type_53 p0, gg_slice p1, int64_t base) {
  /* FG_SHARED_BODY:35 */
  return gg_function_35(p0, p1, base);
}
static gg_type_21 gg_function_4(gg_type_51 p0, int64_t base) {
  /* FG_SHARED_BODY:32 */
  return gg_function_32(p0, base);
}
static void gg_function_2(int64_t p0, int64_t p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  gg_type_3 * t6;
  uint16_t t7;
  float t8;
  int64_t t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t * t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  uint8_t t23;
  gg_slice t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  float * t30;
  float t31;
  int32_t t32;
  int32_t t33;
  float t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  int64_t t38;
  gg_type_8 * t39;
  uint16_t t40;
  float t41;
  gg_type_51 t42;
  int64_t t43;
  int64_t t44;
  gg_slice t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  float * t51;
  float t52;
  int64_t t53;
  int8_t * t54;
  float t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  int64_t t59;
  gg_type_11 * t60;
  uint16_t t61;
  float t62;
  gg_type_53 t63;
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  gg_slice t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  float * t73;
  float t74;
  int64_t t75;
  int8_t * t76;
  float t77;
  int64_t t78;
  int64_t t79;
  uint8_t t80;
  int64_t t81;
  gg_type_13 * t82;
  uint16_t t83;
  float t84;
  int64_t t85;
  gg_type_16 * t86;
  uint16_t t87;
  float t88;
  uint8_t t89;
  uint8_t t90;
  int64_t t91;
  uint8_t t92;
  uint8_t t93;
  int64_t t94;
  int16_t t95;
  int16_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  gg_slice t101;
  gg_slice t102;
  int64_t t103;
  int64_t t104;
  gg_slice t105;
  gg_slice t106;
  gg_slice t107;
  gg_type_56 t108;
  int64_t t109;
  int64_t t110;
  float t111;
  float t112;
  float t113;
  gg_slice t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int16_t * t118;
  float t119;
  float t120;
  int64_t t121;
  uint8_t * t122;
  float t123;
  gg_slice t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  float * t134;
  int64_t t135;
  uint8_t * t136;
  uint8_t t137;
  uint8_t t138;
  uint8_t t139;
  float t140;
  int64_t v0;
  int64_t v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  gg_slice v10;
  gg_slice v11;
  int64_t v12;
  gg_type_50 v13;
  float v14;
  int64_t v15;
  int32_t v16;
  gg_type_51 v17;
  float v18;
  gg_type_21 v19;
  int64_t v20;
  gg_type_53 v21;
  float v22;
  gg_type_21 v23;
  int64_t v24;
  float v25;
  gg_type_55 v26;
  int64_t v27;
  int64_t v28;
  float v29;
  float v30;
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
  {
  t1 = v1;
  t0 = 0;
label0:;
  if (t0 >= t1) goto label2;
  v12 = t0;
  {
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) == (t3));
  if (t4) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t5 = v12;
  t6 = ((gg_type_3 *)(v2).data) + gg_index(t5, (v2).len);
  v13 = t6;
  t7 = ((uint16_t)(((*(v13))).f16));
  t8 = fg139_half_float(t7);
  v14 = ((float)(t8));
  t10 = ((int64_t)UINT64_C(64));
  t9 = 0;
label3:;
  if (t9 >= t10) goto label5;
  v15 = t9;
  {
  t11 = gg_view((((*(v13))).f17).data, 16);
  t12 = ((int64_t)(v15));
  t13 = ((int64_t)(((int64_t)UINT64_C(4))));
  t14 = ((int64_t)gg_div(t12, t13));
  t15 = ((uint8_t *)t11.data) + gg_index(t14, t11.len);
  t16 = ((uint8_t)((*t15)));
  t17 = ((int64_t)(v15));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = ((int64_t)(((int64_t)gg_mod(t17, t18))));
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))))));
  t22 = ((uint8_t)(((t16) >> ((uint64_t)(t21) & 7))));
  t23 = ((uint8_t)(((int64_t)UINT64_C(3))));
  v16 = ((int32_t)(((int32_t)(((t22) & (t23))))));
  t24 = v7;
  t25 = ((int64_t)(v12));
  t26 = ((int64_t)(((int64_t)UINT64_C(64))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  t28 = ((int64_t)(v15));
  t29 = ((int64_t)((uint64_t)(t27) + (uint64_t)(t28)));
  t30 = ((float *)t24.data) + gg_index(t29, t24.len);
  t31 = ((float)(v14));
  t32 = ((int32_t)(v16));
  t33 = ((int32_t)(((int64_t)UINT64_C(1))));
  t34 = ((float)(((float)(((int32_t)((uint64_t)(t32) - (uint64_t)(t33)))))));
  (*t30) = ((float)(((t31) * (t34))));
  }
label4:;
  t9++; goto label3;
label5:;
  }
  } else {
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t38 = v12;
  t39 = ((gg_type_8 *)(v3).data) + gg_index(t38, (v3).len);
  v17 = t39;
  t40 = ((uint16_t)(((*(v17))).f18));
  t41 = fg139_half_float(t40);
  v18 = ((float)(t41));
  t42 = v17;
  v19 = gg_function_32(t42, base + 0);
  t44 = ((int64_t)UINT64_C(32));
  t43 = 0;
label6:;
  if (t43 >= t44) goto label8;
  v20 = t43;
  {
  t45 = v7;
  t46 = ((int64_t)(v12));
  t47 = ((int64_t)(((int64_t)UINT64_C(32))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)(v20));
  t50 = ((int64_t)((uint64_t)(t48) + (uint64_t)(t49)));
  t51 = ((float *)t45.data) + gg_index(t50, t45.len);
  t52 = ((float)(v18));
  /* FG_DIRECT_SLICE_INDEX */
  t53 = v20;
  t54 = ((int8_t *)(v19).data) + gg_index(t53, ((int64_t)UINT64_C(32)));
  t55 = ((float)(((float)((*t54)))));
  (*t51) = ((float)(((t52) * (t55))));
  }
label7:;
  t43++; goto label6;
label8:;
  }
  } else {
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t58 = ((t56) == (t57));
  if (t58) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t59 = v12;
  t60 = ((gg_type_11 *)(v4).data) + gg_index(t59, (v4).len);
  v21 = t60;
  t61 = ((uint16_t)(((*(v21))).f21));
  t62 = fg139_half_float(t61);
  v22 = ((float)(t62));
  t63 = v21;
  t64 = v11;
  v23 = gg_function_35(t63, t64, base + 0);
  t66 = ((int64_t)UINT64_C(32));
  t65 = 0;
label9:;
  if (t65 >= t66) goto label11;
  v24 = t65;
  {
  t67 = v7;
  t68 = ((int64_t)(v12));
  t69 = ((int64_t)(((int64_t)UINT64_C(32))));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t69)))));
  t71 = ((int64_t)(v24));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((float *)t67.data) + gg_index(t72, t67.len);
  t74 = ((float)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t75 = v24;
  t76 = ((int8_t *)(v23).data) + gg_index(t75, ((int64_t)UINT64_C(32)));
  t77 = ((float)(((float)((*t76)))));
  (*t73) = ((float)(((t74) * (t77))));
  }
label10:;
  t65++; goto label9;
label11:;
  }
  } else {
  {
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(((int64_t)UINT64_C(3))));
  t80 = ((t78) == (t79));
  if (t80) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t81 = v12;
  t82 = ((gg_type_13 *)(v5).data) + gg_index(t81, (v5).len);
  t83 = ((uint16_t)(((*t82)).f23));
  t84 = fg139_half_float(t83);
  v25 = ((float)(t84));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t85 = v12;
  t86 = ((gg_type_16 *)(v6).data) + gg_index(t85, (v6).len);
  t87 = ((uint16_t)(((*t86)).f25));
  t88 = fg139_half_float(t87);
  v25 = ((float)(t88));
  }
  }
  memset(&(v26), 0, sizeof(v26));
  memset(&t89, 0, sizeof(t89));
  t90 = ((uint8_t)(t89));
  for (t91=0; t91<32; t91++) ((v26).f36).data[t91] = t90;
  memset(&t92, 0, sizeof(t92));
  t93 = ((uint8_t)(t92));
  for (t94=0; t94<32; t94++) ((v26).f37).data[t94] = t93;
  memset(&t95, 0, sizeof(t95));
  t96 = ((int16_t)(t95));
  for (t97=0; t97<16; t97++) ((v26).f38).data[t97] = t96;
  t99 = ((int64_t)UINT64_C(8));
  t98 = 0;
label12:;
  if (t98 >= t99) goto label14;
  v27 = t98;
  {
  t100 = ((int64_t)(v0));
  t101 = v5;
  t102 = v6;
  t103 = ((int64_t)(v12));
  t104 = ((int64_t)(v27));
  t105 = v8;
  t106 = v9;
  t107 = v10;
  t108 = &(v26);
  gg_function_16(t100, t101, t102, t103, t104, t105, t106, t107, t108, base + 0);
  t110 = ((int64_t)UINT64_C(32));
  t109 = 0;
label15:;
  if (t109 >= t110) goto label17;
  v28 = t109;
  {
  t111 = ((float)(((float)(gg_double_bits(UINT64_C(4593671619917905920))))));
  t112 = ((float)(v25));
  t113 = ((float)(((t111) * (t112))));
  t114 = gg_view(((v26).f38).data, 16);
  t115 = ((int64_t)(v28));
  t116 = ((int64_t)(((int64_t)UINT64_C(2))));
  t117 = ((int64_t)gg_div(t115, t116));
  t118 = ((int16_t *)t114.data) + gg_index(t117, t114.len);
  t119 = ((float)(((float)((*t118)))));
  v29 = ((float)(((t113) * (t119))));
  t120 = ((float)(v29));
  /* FG_DIRECT_SLICE_INDEX */
  t121 = v28;
  t122 = ((uint8_t *)((v26).f36).data) + gg_index(t121, ((int64_t)UINT64_C(32)));
  t123 = ((float)(((float)((*t122)))));
  v30 = ((float)(((t120) * (t123))));
  t124 = v7;
  t125 = ((int64_t)(v12));
  t126 = ((int64_t)(((int64_t)UINT64_C(256))));
  t127 = ((int64_t)(((int64_t)((uint64_t)(t125) * (uint64_t)(t126)))));
  t128 = ((int64_t)(v27));
  t129 = ((int64_t)(((int64_t)UINT64_C(32))));
  t130 = ((int64_t)(((int64_t)((uint64_t)(t128) * (uint64_t)(t129)))));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t127) + (uint64_t)(t130)))));
  t132 = ((int64_t)(v28));
  t133 = ((int64_t)((uint64_t)(t131) + (uint64_t)(t132)));
  t134 = ((float *)t124.data) + gg_index(t133, t124.len);
  /* FG_DIRECT_SLICE_INDEX */
  t135 = v28;
  t136 = ((uint8_t *)((v26).f37).data) + gg_index(t135, ((int64_t)UINT64_C(32)));
  t137 = ((uint8_t)((*t136)));
  t138 = ((uint8_t)(((int64_t)UINT64_C(0))));
  t139 = ((t137) != (t138));
  if (t139) {
  {
  t140 = v30;
  (*t134) = ((float)((-(t140))));
  }
  } else {
  {
  (*t134) = ((float)(v30));
  }
  }
  }
label16:;
  t109++; goto label15;
label17:;
  }
label13:;
  t98++; goto label12;
label14:;
  }
  }
  }
  }
  }
label1:;
  t0++; goto label0;
label2:;
  }
gg_done:;
}
static void gg_function_1(gg_type_36 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  gg_slice t19;
  gg_slice t20;
  gg_slice t21;
  gg_slice t22;
  gg_slice t23;
  gg_slice t24;
  gg_slice t25;
  gg_slice t26;
  gg_slice t27;
  gg_slice t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  gg_type_33 * t53;
  gg_slice t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  gg_type_33 * t58;
  gg_type_36 t59;
  gg_type_62 t60;
  gg_type_36 t61;
  gg_type_62 t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  gg_type_36 t66;
  gg_slice t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  gg_type_33 * t71;
  gg_type_62 t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  gg_slice t76;
  gg_slice t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  gg_slice t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  gg_slice t89;
  gg_slice t90;
  gg_slice t91;
  int64_t t92;
  int64_t t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  gg_type_33 * t98;
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
  float * t110;
  int64_t t111;
  float * t112;
  int64_t t113;
  int64_t t114;
  uint8_t t115;
  int64_t t116;
  int64_t t117;
  uint8_t t118;
  uint8_t t119;
  int64_t t120;
  int64_t t121;
  uint8_t t122;
  uint8_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  gg_slice t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  gg_type_33 * t134;
  gg_type_36 t135;
  gg_type_62 t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  gg_slice t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  float * t151;
  gg_type_36 t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  gg_slice t159;
  gg_slice t160;
  gg_slice t161;
  gg_slice t162;
  gg_slice t163;
  gg_slice t164;
  gg_slice t165;
  gg_slice t166;
  gg_slice t167;
  gg_slice t168;
  gg_slice t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  gg_slice t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  float * t178;
  gg_type_36 t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  gg_slice t186;
  gg_slice t187;
  gg_slice t188;
  gg_slice t189;
  gg_slice t190;
  gg_slice t191;
  gg_slice t192;
  gg_slice t193;
  gg_slice t194;
  gg_slice t195;
  gg_slice t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  gg_type_36 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  gg_slice v10;
  gg_slice v11;
  gg_slice v12;
  gg_slice v13;
  gg_type_33 v14;
  gg_type_33 v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  gg_type_63 v20;
  int64_t v21;
  gg_type_33 v22;
  int64_t v23;
  int64_t v24;
  gg_type_33 v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
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
  {
  t0 = ((int64_t)(((*(v0))).f1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(((*(v0))).f1));
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = ((t4) <= (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(((*(v0))).f2));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) >= (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(((*(v0))).f3));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) >= (t13));
  }
  if (!(t11)) gg_fail("assertion failed");
  t14 = ((int64_t)(((*(v0))).f0));
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = ((int64_t)(((*(v0))).f1));
  t18 = ((int64_t)(((*(v0))).f2));
  t19 = v1;
  t20 = v2;
  t21 = v3;
  t22 = v4;
  t23 = v5;
  t24 = v8;
  t25 = v9;
  t26 = v10;
  t27 = v11;
  t28 = v12;
  gg_function_2(t17, t18, t19, t20, t21, t22, t23, t24, t25, t26, t27, t28, base + 0);
  }
  } else {
  t29 = ((int64_t)(((*(v0))).f0));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(((*(v0))).f1));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  t34 = ((t32) == (t33));
  t35 = t34;
  if (t35) {
  t36 = ((int64_t)(((*(v0))).f6));
  t37 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = ((t36) >= (t37));
  }
  t38 = t35;
  t39 = t38;
  if (t39) {
  t40 = ((int64_t)(((*(v0))).f6));
  t41 = ((int64_t)(((int64_t)UINT64_C(3))));
  t39 = ((t40) <= (t41));
  }
  t42 = t39;
  t43 = t42;
  if (t43) {
  t44 = ((int64_t)(((*(v0))).f7));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t44) > (t45));
  }
  t46 = t43;
  t47 = t46;
  if (t47) {
  t48 = ((int64_t)((v13).len));
  t49 = ((int64_t)(((*(v0))).f10));
  t50 = ((int64_t)(((*(v0))).f6));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t50)))));
  t47 = ((t48) >= (t51));
  }
  if (!(t47)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t52 = ((*(v0))).f10;
  t53 = ((gg_type_33 *)(v13).data) + gg_index(t52, (v13).len);
  v14 = (*t53);
  t54 = v13;
  t55 = ((int64_t)(((*(v0))).f10));
  t56 = ((int64_t)(((int64_t)UINT64_C(1))));
  t57 = ((int64_t)((uint64_t)(t55) + (uint64_t)(t56)));
  t58 = ((gg_type_33 *)t54.data) + gg_index(t57, t54.len);
  v15 = (*t58);
  t59 = v0;
  t60 = &(v14);
  v16 = gg_function_12(t59, t60, base + 0);
  t61 = v0;
  t62 = &(v15);
  v17 = gg_function_12(t61, t62, base + 0);
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((int64_t)(((*(v0))).f6));
  t64 = ((int64_t)(((int64_t)UINT64_C(3))));
  t65 = ((t63) == (t64));
  if (t65) {
  {
  t66 = v0;
  t67 = v13;
  t68 = ((int64_t)(((*(v0))).f10));
  t69 = ((int64_t)(((int64_t)UINT64_C(2))));
  t70 = ((int64_t)((uint64_t)(t68) + (uint64_t)(t69)));
  t71 = ((gg_type_33 *)t67.data) + gg_index(t70, t67.len);
  t72 = t71;
  v18 = gg_function_12(t66, t72, base + 0);
  }
  }
  t74 = ((*(v0))).f3;
  t73 = 0;
label0:;
  if (t73 >= t74) goto label2;
  v19 = t73;
  {
  t75 = ((int64_t)(((int64_t)UINT64_C(4))));
  t76 = v4;
  t77 = v5;
  t78 = ((int64_t)(((*(v0))).f5));
  t79 = ((int64_t)(v19));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t79)))));
  t81 = ((int64_t)(((*(v0))).f4));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  t83 = v7;
  t84 = ((int64_t)(((*(v0))).f2));
  t85 = ((int64_t)(((*(v0))).f6));
  t86 = ((int64_t)(v16));
  t87 = ((int64_t)(v17));
  t88 = ((int64_t)(v18));
  t89 = v9;
  t90 = v10;
  t91 = v11;
  v20 = gg_function_14(t75, t76, t77, t82, t83, t84, t85, t86, t87, t88, t89, t90, t91, base + 0);
  t93 = ((*(v0))).f6;
  t92 = 0;
label3:;
  if (t92 >= t93) goto label5;
  v21 = t92;
  {
  t94 = v13;
  t95 = ((int64_t)(((*(v0))).f10));
  t96 = ((int64_t)(v21));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((gg_type_33 *)t94.data) + gg_index(t97, t94.len);
  v22 = (*t98);
  t99 = ((int64_t)(((int64_t)((v22).f34))));
  t100 = ((int64_t)(((*(v0))).f8));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t100)))));
  t102 = ((int64_t)(((int64_t)((v22).f35))));
  t103 = ((int64_t)(((*(v0))).f9));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t102) * (uint64_t)(t103)))));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t101) + (uint64_t)(t104)))));
  t106 = ((int64_t)(((*(v0))).f5));
  t107 = ((int64_t)(((int64_t)((uint64_t)(t105) + (uint64_t)(t106)))));
  t108 = ((int64_t)(v19));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t107) + (uint64_t)(t108)))));
  /* FG_DIRECT_SLICE_INDEX */
  t109 = v23;
  t110 = ((float *)(v8).data) + gg_index(t109, (v8).len);
  /* FG_DIRECT_SLICE_INDEX */
  t111 = v21;
  t112 = ((float *)(v20).data) + gg_index(t111, ((int64_t)UINT64_C(3)));
  (*t110) = ((float)((*t112)));
  }
label4:;
  t92++; goto label3;
label5:;
  }
label1:;
  t73++; goto label0;
label2:;
  }
  } else {
  t113 = ((int64_t)(((*(v0))).f0));
  t114 = ((int64_t)(((int64_t)UINT64_C(3))));
  t115 = ((t113) == (t114));
  if (t115) {
  {
  t116 = ((int64_t)(((*(v0))).f6));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t118 = ((t116) >= (t117));
  t119 = t118;
  if (t119) {
  t120 = ((int64_t)(((*(v0))).f7));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((t120) > (t121));
  }
  t122 = t119;
  t123 = t122;
  if (t123) {
  t124 = ((int64_t)((v13).len));
  t125 = ((int64_t)(((*(v0))).f10));
  t126 = ((int64_t)(((*(v0))).f6));
  t127 = ((int64_t)(((int64_t)((uint64_t)(t125) + (uint64_t)(t126)))));
  t123 = ((t124) >= (t127));
  }
  if (!(t123)) gg_fail("assertion failed");
  t129 = ((*(v0))).f6;
  t128 = 0;
label6:;
  if (t128 >= t129) goto label8;
  v24 = t128;
  {
  t130 = v13;
  t131 = ((int64_t)(((*(v0))).f10));
  t132 = ((int64_t)(v24));
  t133 = ((int64_t)((uint64_t)(t131) + (uint64_t)(t132)));
  t134 = ((gg_type_33 *)t130.data) + gg_index(t133, t130.len);
  v25 = (*t134);
  t135 = v0;
  t136 = &(v25);
  v26 = gg_function_12(t135, t136, base + 0);
  t137 = ((int64_t)(((int64_t)((v25).f34))));
  t138 = ((int64_t)(((*(v0))).f8));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t137) * (uint64_t)(t138)))));
  t140 = ((int64_t)(((int64_t)((v25).f35))));
  t141 = ((int64_t)(((*(v0))).f9));
  t142 = ((int64_t)(((int64_t)((uint64_t)(t140) * (uint64_t)(t141)))));
  t143 = ((int64_t)(((int64_t)((uint64_t)(t139) + (uint64_t)(t142)))));
  t144 = ((int64_t)(((*(v0))).f5));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t143) + (uint64_t)(t144)))));
  t146 = ((*(v0))).f3;
  t145 = 0;
label9:;
  if (t145 >= t146) goto label11;
  v28 = t145;
  {
  t147 = v8;
  t148 = ((int64_t)(v27));
  t149 = ((int64_t)(v28));
  t150 = ((int64_t)((uint64_t)(t148) + (uint64_t)(t149)));
  t151 = ((float *)t147.data) + gg_index(t150, t147.len);
  t152 = v0;
  t153 = ((int64_t)(((*(v0))).f5));
  t154 = ((int64_t)(v28));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t154)))));
  t156 = ((int64_t)(((*(v0))).f4));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) * (uint64_t)(t156)))));
  t158 = ((int64_t)(v26));
  t159 = v1;
  t160 = v2;
  t161 = v3;
  t162 = v4;
  t163 = v5;
  t164 = v6;
  t165 = v7;
  t166 = v9;
  t167 = v10;
  t168 = v11;
  t169 = v12;
  (*t151) = gg_function_25(t152, t157, t158, t159, t160, t161, t162, t163, t164, t165, t166, t167, t168, t169, base + 0);
  }
label10:;
  t145++; goto label9;
label11:;
  }
label7:;
  t128++; goto label6;
label8:;
  }
  } else {
  {
  t170 = ((int64_t)(((*(v0))).f0));
  t171 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t170) == (t171)))) gg_fail("assertion failed");
  t173 = ((*(v0))).f3;
  t172 = 0;
label12:;
  if (t172 >= t173) goto label14;
  v29 = t172;
  {
  t174 = v8;
  t175 = ((int64_t)(v29));
  t176 = ((int64_t)(((*(v0))).f15));
  t177 = ((int64_t)((uint64_t)(t175) * (uint64_t)(t176)));
  t178 = ((float *)t174.data) + gg_index(t177, t174.len);
  t179 = v0;
  t180 = ((int64_t)(((*(v0))).f5));
  t181 = ((int64_t)(v29));
  t182 = ((int64_t)(((int64_t)((uint64_t)(t180) + (uint64_t)(t181)))));
  t183 = ((int64_t)(((*(v0))).f4));
  t184 = ((int64_t)(((int64_t)((uint64_t)(t182) * (uint64_t)(t183)))));
  t185 = ((int64_t)(((int64_t)UINT64_C(0))));
  t186 = v1;
  t187 = v2;
  t188 = v3;
  t189 = v4;
  t190 = v5;
  t191 = v6;
  t192 = v7;
  t193 = v9;
  t194 = v10;
  t195 = v11;
  t196 = v12;
  (*t178) = gg_function_25(t179, t184, t185, t186, t187, t188, t189, t190, t191, t192, t193, t194, t195, t196, base + 0);
  }
label13:;
  t172++; goto label12;
label14:;
  }
  }
  }
  }
  t197 = ((int64_t)(((*(v0))).f0));
  t198 = ((int64_t)(((*(v0))).f1));
  t199 = ((int64_t)(((*(v0))).f2));
  t200 = ((int64_t)(((*(v0))).f3));
  t201 = ((int64_t)(((*(v0))).f6));
  fg139_math_witness(t197, t198, t199, t200, t201);
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
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
  int64_t t13;
  int64_t t14;
  int64_t t15;
  gg_type_3 t16;
  uint16_t t17;
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  gg_type_3 t21;
  int64_t t22;
  gg_type_8 t23;
  uint16_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  uint8_t t28;
  uint8_t t29;
  int64_t t30;
  gg_type_8 t31;
  int64_t t32;
  gg_type_11 t33;
  uint16_t t34;
  uint8_t t35;
  uint8_t t36;
  int64_t t37;
  gg_type_11 t38;
  int64_t t39;
  gg_type_13 t40;
  uint16_t t41;
  uint8_t t42;
  uint8_t t43;
  int64_t t44;
  gg_type_13 t45;
  int64_t t46;
  gg_type_16 t47;
  uint16_t t48;
  uint8_t t49;
  uint8_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  uint8_t t55;
  uint8_t t56;
  int64_t t57;
  gg_type_16 t58;
  int64_t t59;
  gg_type_19 t60;
  uint16_t t61;
  int8_t t62;
  int8_t t63;
  int64_t t64;
  gg_type_19 t65;
  int64_t t66;
  gg_type_23 t67;
  float t68;
  gg_type_21 t69;
  int8_t t70;
  int8_t t71;
  int64_t t72;
  gg_type_21 t73;
  int64_t t74;
  int16_t t75;
  int16_t t76;
  int64_t t77;
  gg_type_23 t78;
  int64_t t79;
  float t80;
  float t81;
  int64_t t82;
  gg_type_17 t83;
  uint8_t t84;
  uint8_t t85;
  int64_t t86;
  gg_type_17 t87;
  int64_t t88;
  uint8_t t89;
  uint8_t t90;
  int64_t t91;
  int8_t t92;
  int8_t t93;
  int64_t t94;
  gg_type_33 t95;
  int32_t t96;
  int32_t t97;
  gg_type_33 t98;
  int64_t t99;
  gg_type_36 t100;
  gg_slice t101;
  gg_slice t102;
  gg_slice t103;
  gg_slice t104;
  gg_slice t105;
  gg_slice t106;
  gg_slice t107;
  gg_slice t108;
  gg_slice t109;
  gg_slice t110;
  gg_slice t111;
  gg_slice t112;
  gg_slice t113;
  gg_header t114;
  int64_t t115;
  float * t116;
  int64_t t117;
  gg_type_1 v0;
  gg_type_7 v1;
  gg_type_10 v2;
  gg_type_12 v3;
  gg_type_15 v4;
  gg_type_18 v5;
  gg_type_22 v6;
  gg_type_28 v7;
  gg_type_29 v8;
  gg_type_30 v9;
  gg_type_31 v10;
  gg_type_32 v11;
  gg_type_35 v12;
  {
  memset(&(v0), 0, sizeof(v0));
  memset(&t0, 0, sizeof(t0));
  (v0).f0 = ((int64_t)(t0));
  memset(&t1, 0, sizeof(t1));
  (v0).f1 = ((int64_t)(t1));
  memset(&t2, 0, sizeof(t2));
  (v0).f2 = ((int64_t)(t2));
  memset(&t3, 0, sizeof(t3));
  (v0).f3 = ((int64_t)(t3));
  memset(&t4, 0, sizeof(t4));
  (v0).f4 = ((int64_t)(t4));
  memset(&t5, 0, sizeof(t5));
  (v0).f5 = ((int64_t)(t5));
  memset(&t6, 0, sizeof(t6));
  (v0).f6 = ((int64_t)(t6));
  memset(&t7, 0, sizeof(t7));
  (v0).f7 = ((int64_t)(t7));
  memset(&t8, 0, sizeof(t8));
  (v0).f8 = ((int64_t)(t8));
  memset(&t9, 0, sizeof(t9));
  (v0).f9 = ((int64_t)(t9));
  memset(&t10, 0, sizeof(t10));
  (v0).f10 = ((int64_t)(t10));
  memset(&t11, 0, sizeof(t11));
  (v0).f11 = ((int64_t)(t11));
  memset(&t12, 0, sizeof(t12));
  (v0).f12 = ((int64_t)(t12));
  memset(&t13, 0, sizeof(t13));
  (v0).f13 = ((int64_t)(t13));
  memset(&t14, 0, sizeof(t14));
  (v0).f14 = ((int64_t)(t14));
  memset(&t15, 0, sizeof(t15));
  (v0).f15 = ((int64_t)(t15));
  (v0).f2 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v0).f3 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v0).f4 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v0).f6 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v0).f7 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v0).f15 = ((int64_t)(((int64_t)UINT64_C(1))));
  memset(&(t16), 0, sizeof(t16));
  memset(&t17, 0, sizeof(t17));
  (t16).f16 = ((uint16_t)(t17));
  memset(&t18, 0, sizeof(t18));
  t19 = ((uint8_t)(t18));
  for (t20=0; t20<16; t20++) ((t16).f17).data[t20] = t19;
  t21 = t16;
  for (t22=0; t22<1; t22++) (v1).data[t22] = t21;
  memset(&(t23), 0, sizeof(t23));
  memset(&t24, 0, sizeof(t24));
  (t23).f18 = ((uint16_t)(t24));
  memset(&t25, 0, sizeof(t25));
  t26 = ((uint8_t)(t25));
  for (t27=0; t27<4; t27++) ((t23).f19).data[t27] = t26;
  memset(&t28, 0, sizeof(t28));
  t29 = ((uint8_t)(t28));
  for (t30=0; t30<16; t30++) ((t23).f20).data[t30] = t29;
  t31 = t23;
  for (t32=0; t32<1; t32++) (v2).data[t32] = t31;
  memset(&(t33), 0, sizeof(t33));
  memset(&t34, 0, sizeof(t34));
  (t33).f21 = ((uint16_t)(t34));
  memset(&t35, 0, sizeof(t35));
  t36 = ((uint8_t)(t35));
  for (t37=0; t37<16; t37++) ((t33).f22).data[t37] = t36;
  t38 = t33;
  for (t39=0; t39<1; t39++) (v3).data[t39] = t38;
  memset(&(t40), 0, sizeof(t40));
  memset(&t41, 0, sizeof(t41));
  (t40).f23 = ((uint16_t)(t41));
  memset(&t42, 0, sizeof(t42));
  t43 = ((uint8_t)(t42));
  for (t44=0; t44<64; t44++) ((t40).f24).data[t44] = t43;
  t45 = t40;
  for (t46=0; t46<1; t46++) (v4).data[t46] = t45;
  memset(&(t47), 0, sizeof(t47));
  memset(&t48, 0, sizeof(t48));
  (t47).f25 = ((uint16_t)(t48));
  memset(&t49, 0, sizeof(t49));
  t50 = ((uint8_t)(t49));
  for (t51=0; t51<64; t51++) ((t47).f26).data[t51] = t50;
  memset(&t52, 0, sizeof(t52));
  t53 = ((uint8_t)(t52));
  for (t54=0; t54<8; t54++) ((t47).f27).data[t54] = t53;
  memset(&t55, 0, sizeof(t55));
  t56 = ((uint8_t)(t55));
  for (t57=0; t57<8; t57++) ((t47).f28).data[t57] = t56;
  t58 = t47;
  for (t59=0; t59<1; t59++) (v5).data[t59] = t58;
  memset(&(t60), 0, sizeof(t60));
  memset(&t61, 0, sizeof(t61));
  (t60).f29 = ((uint16_t)(t61));
  memset(&t62, 0, sizeof(t62));
  t63 = ((int8_t)(t62));
  for (t64=0; t64<32; t64++) ((t60).f30).data[t64] = t63;
  t65 = t60;
  for (t66=0; t66<2; t66++) (v6).data[t66] = t65;
  memset(&(t67), 0, sizeof(t67));
  memset(&t68, 0, sizeof(t68));
  (t67).f31 = ((float)(t68));
  memset(&t70, 0, sizeof(t70));
  t71 = ((int8_t)(t70));
  for (t72=0; t72<32; t72++) (t69).data[t72] = t71;
  t73 = t69;
  for (t74=0; t74<8; t74++) ((t67).f32).data[t74] = t73;
  memset(&t75, 0, sizeof(t75));
  t76 = ((int16_t)(t75));
  for (t77=0; t77<16; t77++) ((t67).f33).data[t77] = t76;
  t78 = t67;
  for (t79=0; t79<1; t79++) (v7).data[t79] = t78;
  memset(&t80, 0, sizeof(t80));
  t81 = ((float)(t80));
  for (t82=0; t82<256; t82++) (v8).data[t82] = t81;
  memset(&t84, 0, sizeof(t84));
  t85 = ((uint8_t)(t84));
  for (t86=0; t86<8; t86++) (t83).data[t86] = t85;
  t87 = t83;
  for (t88=0; t88<1; t88++) (v9).data[t88] = t87;
  memset(&t89, 0, sizeof(t89));
  t90 = ((uint8_t)(t89));
  for (t91=0; t91<128; t91++) (v10).data[t91] = t90;
  memset(&t92, 0, sizeof(t92));
  t93 = ((int8_t)(t92));
  for (t94=0; t94<16; t94++) (v11).data[t94] = t93;
  memset(&(t95), 0, sizeof(t95));
  memset(&t96, 0, sizeof(t96));
  (t95).f34 = ((int32_t)(t96));
  memset(&t97, 0, sizeof(t97));
  (t95).f35 = ((int32_t)(t97));
  t98 = t95;
  for (t99=0; t99<3; t99++) (v12).data[t99] = t98;
  t100 = &(v0);
  t101 = gg_view((v1).data, 1);
  t102 = gg_view((v2).data, 1);
  t103 = gg_view((v3).data, 1);
  t104 = gg_view((v4).data, 1);
  t105 = gg_view((v5).data, 1);
  t106 = gg_view((v6).data, 2);
  t107 = gg_view((v7).data, 1);
  t108 = gg_view((v8).data, 256);
  t109 = gg_view((v9).data, 1);
  t110 = gg_view((v9).data, 1);
  t111 = gg_view((v10).data, 128);
  t112 = gg_view((v11).data, 16);
  t113 = gg_view((v12).data, 3);
  gg_function_1(t100, t101, t102, t103, t104, t105, t106, t107, t108, t109, t110, t111, t112, t113, base + 0);
  t114.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t114.len = 0;
  /* FG_DIRECT_SLICE_INDEX */
  t115 = ((int64_t)UINT64_C(0));
  t116 = ((float *)(v8).data) + gg_index(t115, ((int64_t)UINT64_C(256)));
  gg_float32(gg_ref(&t114, gg_get_stack(base + 0)), (*t116));
  gg_append(gg_ref(&t114, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t114).base, (t114).len));
  }
gg_done:;
  for (t117 = 0; t117 < 1; t117++) gg_stacks[base + t117].top = 0;
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
