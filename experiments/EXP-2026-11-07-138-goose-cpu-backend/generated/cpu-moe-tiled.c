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
};
#pragma pack(pop)
typedef struct gg_type_3 gg_type_3;
#pragma pack(push, 1)
typedef struct { uint8_t data[12]; } gg_type_6;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[32]; } gg_type_7;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_7 data[4]; } gg_type_8;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_3 {
  uint16_t f11;
  uint16_t f12;
  gg_type_6 f13;
  gg_type_8 f14;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_3 data[1]; } gg_type_9;
#pragma pack(pop)
typedef struct gg_type_10 gg_type_10;
#pragma pack(push, 1)
typedef struct { gg_type_7 data[2]; } gg_type_11;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int8_t data[16]; } gg_type_13;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_10 {
  gg_type_8 f15;
  gg_type_11 f16;
  gg_type_13 f17;
  uint16_t f18;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_10 data[1]; } gg_type_14;
#pragma pack(pop)
typedef struct gg_type_15 gg_type_15;
#pragma pack(push, 1)
typedef struct { int8_t data[32]; } gg_type_17;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_17 data[8]; } gg_type_18;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int16_t data[16]; } gg_type_20;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_15 {
  float f19;
  gg_type_18 f20;
  gg_type_20 f21;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_15 data[1]; } gg_type_21;
#pragma pack(pop)
typedef struct gg_type_22 gg_type_22;
#pragma pack(push, 1)
struct gg_type_22 {
  int32_t f22;
  int32_t f23;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_22 data[1]; } gg_type_24;
#pragma pack(pop)
typedef struct gg_type_25 gg_type_25;
typedef struct gg_type_26 gg_type_26;
#pragma pack(push, 1)
typedef struct { gg_type_7 data[2048]; } gg_type_27;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[256]; } gg_type_28;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[4096]; } gg_type_29;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_26 {
  gg_type_27 f24;
  gg_type_28 f25;
  gg_type_28 f26;
  gg_type_29 f27;
  gg_type_29 f28;
};
#pragma pack(pop)
typedef struct gg_type_30 gg_type_30;
#pragma pack(push, 1)
typedef struct { gg_type_17 data[2048]; } gg_type_31;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_32;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_32 data[512]; } gg_type_33;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_30 {
  gg_type_31 f30;
  gg_type_33 f31;
  gg_type_28 f32;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_34;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_34 data[8192]; } gg_type_35;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_25 {
  gg_type_26 f29;
  gg_type_30 f33;
  gg_type_35 f34;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[1]; } gg_type_36;
#pragma pack(pop)
typedef gg_type_1 *gg_type_37;
typedef gg_type_25 *gg_type_42;
typedef gg_type_30 *gg_type_47;
typedef gg_type_15 *gg_type_48;
typedef gg_type_26 *gg_type_49;
typedef gg_type_10 *gg_type_50;
typedef gg_type_3 *gg_type_51;
#pragma pack(push, 1)
typedef struct { gg_type_32 data[8]; } gg_type_52;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_53;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_54;
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
static uint8_t gg_eq_12(int8_t a, int8_t b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, int8_t value);
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_type_13 value);
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, gg_type_14 value);
static uint8_t gg_eq_15(gg_type_15 a, gg_type_15 b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, gg_type_15 value);
static uint8_t gg_eq_16(float a, float b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, float value);
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_type_17 value);
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, gg_type_18 value);
static uint8_t gg_eq_19(int16_t a, int16_t b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, int16_t value);
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b);
static uint8_t gg_equal_elements_20(gg_slice a, gg_slice b);
static void gg_render_20(gg_reference out, gg_type_20 value);
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b);
static uint8_t gg_equal_elements_21(gg_slice a, gg_slice b);
static void gg_render_21(gg_reference out, gg_type_21 value);
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b);
static uint8_t gg_equal_elements_22(gg_slice a, gg_slice b);
static void gg_render_22(gg_reference out, gg_type_22 value);
static uint8_t gg_eq_23(int32_t a, int32_t b);
static uint8_t gg_equal_elements_23(gg_slice a, gg_slice b);
static void gg_render_23(gg_reference out, int32_t value);
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b);
static uint8_t gg_equal_elements_24(gg_slice a, gg_slice b);
static void gg_render_24(gg_reference out, gg_type_24 value);
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b);
static uint8_t gg_equal_elements_25(gg_slice a, gg_slice b);
static void gg_render_25(gg_reference out, gg_type_25 value);
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b);
static uint8_t gg_equal_elements_26(gg_slice a, gg_slice b);
static void gg_render_26(gg_reference out, gg_type_26 value);
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
static uint8_t gg_eq_34(gg_type_34 a, gg_type_34 b);
static uint8_t gg_equal_elements_34(gg_slice a, gg_slice b);
static void gg_render_34(gg_reference out, gg_type_34 value);
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b);
static uint8_t gg_equal_elements_35(gg_slice a, gg_slice b);
static void gg_render_35(gg_reference out, gg_type_35 value);
static uint8_t gg_eq_36(gg_type_36 a, gg_type_36 b);
static uint8_t gg_equal_elements_36(gg_slice a, gg_slice b);
static void gg_render_36(gg_reference out, gg_type_36 value);
static uint8_t gg_eq_37(gg_type_37 a, gg_type_37 b);
static uint8_t gg_equal_elements_37(gg_slice a, gg_slice b);
static void gg_render_37(gg_reference out, gg_type_37 value);
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
static uint8_t gg_eq_42(gg_type_42 a, gg_type_42 b);
static uint8_t gg_equal_elements_42(gg_slice a, gg_slice b);
static void gg_render_42(gg_reference out, gg_type_42 value);
static uint8_t gg_eq_43(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_43(gg_slice a, gg_slice b);
static void gg_render_43(gg_reference out, gg_slice value);
static uint8_t gg_eq_44(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_44(gg_slice a, gg_slice b);
static void gg_render_44(gg_reference out, uint8_t value);
static uint8_t gg_eq_46(double a, double b);
static uint8_t gg_equal_elements_46(gg_slice a, gg_slice b);
static void gg_render_46(gg_reference out, double value);
static uint8_t gg_eq_47(gg_type_47 a, gg_type_47 b);
static uint8_t gg_equal_elements_47(gg_slice a, gg_slice b);
static void gg_render_47(gg_reference out, gg_type_47 value);
static uint8_t gg_eq_48(gg_type_48 a, gg_type_48 b);
static uint8_t gg_equal_elements_48(gg_slice a, gg_slice b);
static void gg_render_48(gg_reference out, gg_type_48 value);
static uint8_t gg_eq_49(gg_type_49 a, gg_type_49 b);
static uint8_t gg_equal_elements_49(gg_slice a, gg_slice b);
static void gg_render_49(gg_reference out, gg_type_49 value);
static uint8_t gg_eq_50(gg_type_50 a, gg_type_50 b);
static uint8_t gg_equal_elements_50(gg_slice a, gg_slice b);
static void gg_render_50(gg_reference out, gg_type_50 value);
static uint8_t gg_eq_51(gg_type_51 a, gg_type_51 b);
static uint8_t gg_equal_elements_51(gg_slice a, gg_slice b);
static void gg_render_51(gg_reference out, gg_type_51 value);
static uint8_t gg_eq_52(gg_type_52 a, gg_type_52 b);
static uint8_t gg_equal_elements_52(gg_slice a, gg_slice b);
static void gg_render_52(gg_reference out, gg_type_52 value);
static uint8_t gg_eq_53(gg_type_53 a, gg_type_53 b);
static uint8_t gg_equal_elements_53(gg_slice a, gg_slice b);
static void gg_render_53(gg_reference out, gg_type_53 value);
static uint8_t gg_eq_54(gg_type_54 a, gg_type_54 b);
static uint8_t gg_equal_elements_54(gg_slice a, gg_slice b);
static void gg_render_54(gg_reference out, gg_type_54 value);
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
return 1;
}
static void gg_render_1(gg_reference out, gg_type_1 value) {
gg_append(out,"\120\154\141\156\040\173\040",7,1);
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
if(!gg_eq_4(a.f11,b.f11))return 0;
if(!gg_eq_4(a.f12,b.f12))return 0;
if(!gg_eq_6(a.f13,b.f13))return 0;
if(!gg_eq_8(a.f14,b.f14))return 0;
return 1;
}
static void gg_render_3(gg_reference out, gg_type_3 value) {
gg_append(out,"\121\064\040\173\040",5,1);
gg_render_4(out, value.f11);
gg_append(out,", ",2,1);
gg_render_4(out, value.f12);
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f13).data, 12));
gg_append(out,", ",2,1);
gg_render_8(out, value.f14);
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
return gg_equal_elements_5(gg_view((a).data, 12),gg_view((b).data, 12));
}
static void gg_render_6(gg_reference out, gg_type_6 value) {
gg_text(out,gg_view((value).data, 12));
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_5(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
gg_text(out,gg_view((value).data, 32));
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
return gg_equal_elements_7(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_7*)view.data)[i]).data, 32));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((gg_type_9*)a.data)[i],((gg_type_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b) {
return gg_equal_elements_3(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((gg_type_3*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
if(!gg_eq_8(a.f15,b.f15))return 0;
if(!gg_eq_11(a.f16,b.f16))return 0;
if(!gg_eq_13(a.f17,b.f17))return 0;
if(!gg_eq_4(a.f18,b.f18))return 0;
return 1;
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
gg_append(out,"\121\066\040\173\040",5,1);
gg_render_8(out, value.f15);
gg_append(out,", ",2,1);
gg_render_11(out, value.f16);
gg_append(out,", ",2,1);
gg_render_13(out, value.f17);
gg_append(out,", ",2,1);
gg_render_4(out, value.f18);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_type_11*)a.data)[i],((gg_type_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b) {
return gg_equal_elements_7(gg_view((a).data, 2),gg_view((b).data, 2));
}
static void gg_render_11(gg_reference out, gg_type_11 value) {
int64_t i; gg_slice view=gg_view((value).data, 2);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_7*)view.data)[i]).data, 32));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_12(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
return gg_equal_elements_12(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_12(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_type_14*)a.data)[i],((gg_type_14*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b) {
return gg_equal_elements_10(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_14(gg_reference out, gg_type_14 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((gg_type_10*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((gg_type_15*)a.data)[i],((gg_type_15*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(gg_type_15 a, gg_type_15 b) {
if(!gg_eq_16(a.f19,b.f19))return 0;
if(!gg_eq_18(a.f20,b.f20))return 0;
if(!gg_eq_20(a.f21,b.f21))return 0;
return 1;
}
static void gg_render_15(gg_reference out, gg_type_15 value) {
gg_append(out,"\121\070\040\173\040",5,1);
gg_render_16(out, value.f19);
gg_append(out,", ",2,1);
gg_render_18(out, value.f20);
gg_append(out,", ",2,1);
gg_render_20(out, value.f21);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(float a, float b) {
return a == b;
}
static void gg_render_16(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_type_17*)a.data)[i],((gg_type_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b) {
return gg_equal_elements_12(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_17(gg_reference out, gg_type_17 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_12(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_type_18*)a.data)[i],((gg_type_18*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b) {
return gg_equal_elements_17(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_18(gg_reference out, gg_type_18 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_17(out, ((gg_type_17*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_19(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((gg_type_20*)a.data)[i],((gg_type_20*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b) {
return gg_equal_elements_19(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_20(gg_reference out, gg_type_20 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_19(out, ((int16_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((gg_type_21*)a.data)[i],((gg_type_21*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b) {
return gg_equal_elements_15(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_21(gg_reference out, gg_type_21 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_15(out, ((gg_type_15*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((gg_type_22*)a.data)[i],((gg_type_22*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b) {
if(!gg_eq_23(a.f22,b.f22))return 0;
if(!gg_eq_23(a.f23,b.f23))return 0;
return 1;
}
static void gg_render_22(gg_reference out, gg_type_22 value) {
gg_append(out,"\122\157\165\164\145\040\173\040",8,1);
gg_render_23(out, value.f22);
gg_append(out,", ",2,1);
gg_render_23(out, value.f23);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_23(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((gg_type_24*)a.data)[i],((gg_type_24*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b) {
return gg_equal_elements_22(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_24(gg_reference out, gg_type_24 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_22(out, ((gg_type_22*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((gg_type_25*)a.data)[i],((gg_type_25*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b) {
if(!gg_eq_26(a.f29,b.f29))return 0;
if(!gg_eq_30(a.f33,b.f33))return 0;
if(!gg_eq_35(a.f34,b.f34))return 0;
return 1;
}
static void gg_render_25(gg_reference out, gg_type_25 value) {
gg_append(out,"\127\157\162\153\163\160\141\143\145\040\173\040",12,1);
gg_render_26(out, value.f29);
gg_append(out,", ",2,1);
gg_render_30(out, value.f33);
gg_append(out,", ",2,1);
gg_render_35(out, value.f34);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((gg_type_26*)a.data)[i],((gg_type_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b) {
if(!gg_eq_27(a.f24,b.f24))return 0;
if(!gg_eq_28(a.f25,b.f25))return 0;
if(!gg_eq_28(a.f26,b.f26))return 0;
if(!gg_eq_29(a.f27,b.f27))return 0;
if(!gg_eq_29(a.f28,b.f28))return 0;
return 1;
}
static void gg_render_26(gg_reference out, gg_type_26 value) {
gg_append(out,"\127\145\151\147\150\164\124\151\154\145\040\173\040",13,1);
gg_render_27(out, value.f24);
gg_append(out,", ",2,1);
gg_render_28(out, value.f25);
gg_append(out,", ",2,1);
gg_render_28(out, value.f26);
gg_append(out,", ",2,1);
gg_render_29(out, value.f27);
gg_append(out,", ",2,1);
gg_render_29(out, value.f28);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((gg_type_27*)a.data)[i],((gg_type_27*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b) {
return gg_equal_elements_7(gg_view((a).data, 2048),gg_view((b).data, 2048));
}
static void gg_render_27(gg_reference out, gg_type_27 value) {
int64_t i; gg_slice view=gg_view((value).data, 2048);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_7*)view.data)[i]).data, 32));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_type_28*)a.data)[i],((gg_type_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
return gg_equal_elements_16(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((gg_type_29*)a.data)[i],((gg_type_29*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b) {
return gg_equal_elements_23(gg_view((a).data, 4096),gg_view((b).data, 4096));
}
static void gg_render_29(gg_reference out, gg_type_29 value) {
int64_t i; gg_slice view=gg_view((value).data, 4096);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_23(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_30(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_30(((gg_type_30*)a.data)[i],((gg_type_30*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_30(gg_type_30 a, gg_type_30 b) {
if(!gg_eq_31(a.f30,b.f30))return 0;
if(!gg_eq_33(a.f31,b.f31))return 0;
if(!gg_eq_28(a.f32,b.f32))return 0;
return 1;
}
static void gg_render_30(gg_reference out, gg_type_30 value) {
gg_append(out,"\101\143\164\151\166\141\164\151\157\156\124\151\154\145\040\173\040",17,1);
gg_render_31(out, value.f30);
gg_append(out,", ",2,1);
gg_render_33(out, value.f31);
gg_append(out,", ",2,1);
gg_render_28(out, value.f32);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_31(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_31(((gg_type_31*)a.data)[i],((gg_type_31*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b) {
return gg_equal_elements_17(gg_view((a).data, 2048),gg_view((b).data, 2048));
}
static void gg_render_31(gg_reference out, gg_type_31 value) {
int64_t i; gg_slice view=gg_view((value).data, 2048);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_17(out, ((gg_type_17*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_32(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_32(((gg_type_32*)a.data)[i],((gg_type_32*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b) {
return gg_equal_elements_23(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_32(gg_reference out, gg_type_32 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_23(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_33(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_33(((gg_type_33*)a.data)[i],((gg_type_33*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_33(gg_type_33 a, gg_type_33 b) {
return gg_equal_elements_32(gg_view((a).data, 512),gg_view((b).data, 512));
}
static void gg_render_33(gg_reference out, gg_type_33 value) {
int64_t i; gg_slice view=gg_view((value).data, 512);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_32(out, ((gg_type_32*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_34(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_34(((gg_type_34*)a.data)[i],((gg_type_34*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_34(gg_type_34 a, gg_type_34 b) {
return gg_equal_elements_16(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_34(gg_reference out, gg_type_34 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_35(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_35(((gg_type_35*)a.data)[i],((gg_type_35*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b) {
return gg_equal_elements_34(gg_view((a).data, 8192),gg_view((b).data, 8192));
}
static void gg_render_35(gg_reference out, gg_type_35 value) {
int64_t i; gg_slice view=gg_view((value).data, 8192);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_34(out, ((gg_type_34*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_36(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_36(((gg_type_36*)a.data)[i],((gg_type_36*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_36(gg_type_36 a, gg_type_36 b) {
return gg_equal_elements_16(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_36(gg_reference out, gg_type_36 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_37(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_37(((gg_type_37*)a.data)[i],((gg_type_37*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_37(gg_type_37 a, gg_type_37 b) {
return a == b;
}
static void gg_render_37(gg_reference out, gg_type_37 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_38(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_38(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_38(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_38(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((gg_type_3*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_39(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_39(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_39(gg_slice a, gg_slice b) {
return gg_equal_elements_10(a,b);
}
static void gg_render_39(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((gg_type_10*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_40(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_40(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_40(gg_slice a, gg_slice b) {
return gg_equal_elements_15(a,b);
}
static void gg_render_40(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_15(out, ((gg_type_15*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_41(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_41(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_41(gg_slice a, gg_slice b) {
return gg_equal_elements_22(a,b);
}
static void gg_render_41(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_22(out, ((gg_type_22*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_42(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_42(((gg_type_42*)a.data)[i],((gg_type_42*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_42(gg_type_42 a, gg_type_42 b) {
return a == b;
}
static void gg_render_42(gg_reference out, gg_type_42 value) {
gg_render_25(out,*value);
}
static uint8_t gg_equal_elements_43(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_43(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_43(gg_slice a, gg_slice b) {
return gg_equal_elements_16(a,b);
}
static void gg_render_43(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_44(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_44(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_44(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_44(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_46(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_46(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_46(double a, double b) {
return a == b;
}
static void gg_render_46(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_47(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_47(((gg_type_47*)a.data)[i],((gg_type_47*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_47(gg_type_47 a, gg_type_47 b) {
return a == b;
}
static void gg_render_47(gg_reference out, gg_type_47 value) {
gg_render_30(out,*value);
}
static uint8_t gg_equal_elements_48(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_48(((gg_type_48*)a.data)[i],((gg_type_48*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_48(gg_type_48 a, gg_type_48 b) {
return a == b;
}
static void gg_render_48(gg_reference out, gg_type_48 value) {
gg_render_15(out,*value);
}
static uint8_t gg_equal_elements_49(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_49(((gg_type_49*)a.data)[i],((gg_type_49*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_49(gg_type_49 a, gg_type_49 b) {
return a == b;
}
static void gg_render_49(gg_reference out, gg_type_49 value) {
gg_render_26(out,*value);
}
static uint8_t gg_equal_elements_50(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_50(((gg_type_50*)a.data)[i],((gg_type_50*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_50(gg_type_50 a, gg_type_50 b) {
return a == b;
}
static void gg_render_50(gg_reference out, gg_type_50 value) {
gg_render_10(out,*value);
}
static uint8_t gg_equal_elements_51(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_51(((gg_type_51*)a.data)[i],((gg_type_51*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_51(gg_type_51 a, gg_type_51 b) {
return a == b;
}
static void gg_render_51(gg_reference out, gg_type_51 value) {
gg_render_3(out,*value);
}
static uint8_t gg_equal_elements_52(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_52(((gg_type_52*)a.data)[i],((gg_type_52*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_52(gg_type_52 a, gg_type_52 b) {
return gg_equal_elements_32(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_52(gg_reference out, gg_type_52 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_32(out, ((gg_type_32*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_53(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_53(((gg_type_53*)a.data)[i],((gg_type_53*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_53(gg_type_53 a, gg_type_53 b) {
return gg_equal_elements_23(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_53(gg_reference out, gg_type_53 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_23(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_54(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_54(((gg_type_54*)a.data)[i],((gg_type_54*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_54(gg_type_54 a, gg_type_54 b) {
return gg_equal_elements_16(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_54(gg_reference out, gg_type_54 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static uint8_t gg_function_1(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_type_42 p5, gg_slice p6, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_3(int64_t p0, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t base);
static void gg_function_5(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, gg_type_42 p8, uint8_t p9, int64_t base);
static gg_type_17 gg_function_6(gg_type_17 p0, int64_t base);
static void gg_function_7(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_type_42 p6, int64_t base);
static gg_type_20 gg_function_8(int16_t p158, int64_t base);
static gg_type_20 gg_function_10(gg_type_7 p183, gg_type_17 p184, int64_t base);
static gg_type_32 gg_function_11(gg_type_20 p191, gg_type_20 p192, int64_t base);
static void gg_function_12(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_type_42 p6, int64_t base);
static gg_type_7 gg_function_13(gg_type_7 p226, uint8_t p227, int64_t base);
static gg_type_7 gg_function_14(gg_type_7 p233, uint8_t p234, int64_t base);
static void gg_function_15(gg_type_42 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static gg_type_17 gg_function_16(gg_type_7 p0, int64_t base);
static gg_type_20 gg_function_17(int16_t p0, int16_t p1, int64_t base);
static gg_type_7 gg_function_18(gg_type_17 p0, int64_t base);
static gg_type_32 gg_function_19(gg_type_32 p292, gg_type_32 p293, int64_t base);
static int32_t gg_function_20(gg_type_32 p0, int64_t base);
static gg_type_53 gg_function_21(gg_type_32 p306, int64_t base);
static gg_type_32 gg_function_22(int32_t p0, int64_t base);
static gg_type_32 gg_function_23(gg_type_32 p0, gg_type_32 p1, int64_t base);
static gg_type_34 gg_function_24(float p0, gg_type_32 p1, int64_t base);
static gg_type_34 gg_function_25(gg_type_34 p0, gg_type_34 p1, gg_type_34 p2, int64_t base);
static void gg_function_28(gg_type_42 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static gg_type_32 gg_function_29(gg_type_32 p389, gg_type_32 p390, int64_t base);
static gg_type_34 gg_function_30(float p404, gg_type_32 p405, gg_type_34 p406, int64_t base);
static gg_type_34 gg_function_25(gg_type_34 p0, gg_type_34 p1, gg_type_34 p2, int64_t base) {
  gg_type_34 t0;
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
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_type_34 v0;
  gg_type_34 v1;
  gg_type_34 v2;
  gg_type_34 v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v3).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v4 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v4;
  t7 = ((float *)(v3).data) + gg_index(t6, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v4;
  t9 = ((float *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(8)));
  t10 = ((float)((*t9)));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v4;
  t12 = ((float *)(v1).data) + gg_index(t11, ((int64_t)UINT64_C(8)));
  t13 = ((float)((*t12)));
  /* FG_DIRECT_SLICE_INDEX */
  t14 = v4;
  t15 = ((float *)(v2).data) + gg_index(t14, ((int64_t)UINT64_C(8)));
  t16 = ((float)((*t15)));
  t17 = fmaf(t10, t13, t16);
  (*t7) = ((float)(t17));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v3;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:fma32 */
static gg_type_34 gg_function_30(float p404, gg_type_32 p405, gg_type_34 p406, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__) && defined(__FMA__)
  { gg_type_34 packed_result;
    _mm256_storeu_ps(packed_result.data, _mm256_fmadd_ps(_mm256_set1_ps(p404), _mm256_cvtepi32_ps(_mm256_loadu_si256((const __m256i*)p405.data)), _mm256_loadu_ps(p406.data)));
    return packed_result;
  }
#endif
  gg_type_34 t0;
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
  float v404;
  gg_type_32 v405;
  gg_type_34 v406;
  gg_type_34 v407;
  int64_t v409;
  v404 = p404;
  v405 = p405;
  v406 = p406;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v407).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v409 = t4;
  {
  t6 = gg_view((v407).data, 8);
  t7 = v409;
  t8 = ((float *)t6.data) + gg_index(t7, t6.len);
  t9 = ((float)(v404));
  t10 = gg_view((v405).data, 8);
  t11 = v409;
  t12 = ((int32_t *)t10.data) + gg_index(t11, t10.len);
  t13 = ((float)(((float)((*t12)))));
  t14 = gg_view((v406).data, 8);
  t15 = v409;
  t16 = ((float *)t14.data) + gg_index(t15, t14.len);
  t17 = ((float)((*t16)));
  t18 = fmaf(t9, t13, t17);
  (*t8) = ((float)(t18));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v407;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_34 gg_function_24(float p0, gg_type_32 p1, int64_t base) {
  gg_type_34 t0;
  float t1;
  float t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  float * t7;
  float t8;
  int64_t t9;
  int32_t * t10;
  float t11;
  float v0;
  gg_type_32 v1;
  gg_type_34 v2;
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
  t8 = ((float)(v0));
  /* FG_DIRECT_SLICE_INDEX */
  t9 = v3;
  t10 = ((int32_t *)(v1).data) + gg_index(t9, ((int64_t)UINT64_C(8)));
  t11 = ((float)(((float)((*t10)))));
  (*t7) = ((float)(((t8) * (t11))));
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
/* FG_PACKED_MATCH:pair_sum32 */
static gg_type_53 gg_function_21(gg_type_32 p306, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_53 packed_result;
    _mm_storeu_si128((__m128i*)packed_result.data, _mm_hadd_epi32(_mm256_castsi256_si128(_mm256_loadu_si256((const __m256i*)p306.data)), _mm256_extracti128_si256(_mm256_loadu_si256((const __m256i*)p306.data), 1)));
    return packed_result;
  }
#endif
  gg_type_53 t0;
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
  gg_type_32 v306;
  gg_type_53 v307;
  int64_t v309;
  v306 = p306;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<4; t3++) (v307).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(4));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v309 = t4;
  {
  t6 = gg_view((v307).data, 4);
  t7 = v309;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v306).data, 8);
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(v309));
  t12 = ((int64_t)((uint64_t)(t10) * (uint64_t)(t11)));
  t13 = ((int32_t *)t9.data) + gg_index(t12, t9.len);
  t14 = ((int32_t)((*t13)));
  t15 = gg_view((v306).data, 8);
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((int64_t)(v309));
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
  t0 = v307;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_20(gg_type_32 p0, int64_t base) {
  int32_t t0;
  gg_type_32 t1;
  int64_t t2;
  int32_t * t3;
  int32_t t4;
  int64_t t5;
  int32_t * t6;
  int32_t t7;
  int32_t t8;
  int64_t t9;
  int32_t * t10;
  int32_t t11;
  int64_t t12;
  int32_t * t13;
  int32_t t14;
  int32_t t15;
  gg_type_32 v0;
  gg_type_53 v1;
  v0 = p0;
  {
  t1 = v0;
  v1 = gg_function_21(t1, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t2 = ((int64_t)UINT64_C(0));
  t3 = ((int32_t *)(v1).data) + gg_index(t2, ((int64_t)UINT64_C(4)));
  t4 = ((int32_t)((*t3)));
  /* FG_DIRECT_SLICE_INDEX */
  t5 = ((int64_t)UINT64_C(1));
  t6 = ((int32_t *)(v1).data) + gg_index(t5, ((int64_t)UINT64_C(4)));
  t7 = ((int32_t)((*t6)));
  t8 = ((int32_t)(((int32_t)((uint64_t)(t4) + (uint64_t)(t7)))));
  /* FG_DIRECT_SLICE_INDEX */
  t9 = ((int64_t)UINT64_C(2));
  t10 = ((int32_t *)(v1).data) + gg_index(t9, ((int64_t)UINT64_C(4)));
  t11 = ((int32_t)((*t10)));
  /* FG_DIRECT_SLICE_INDEX */
  t12 = ((int64_t)UINT64_C(3));
  t13 = ((int32_t *)(v1).data) + gg_index(t12, ((int64_t)UINT64_C(4)));
  t14 = ((int32_t)((*t13)));
  t15 = ((int32_t)(((int32_t)((uint64_t)(t11) + (uint64_t)(t14)))));
  t0 = ((int32_t)(((int32_t)((uint64_t)(t8) + (uint64_t)(t15)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:add32 */
static gg_type_32 gg_function_19(gg_type_32 p292, gg_type_32 p293, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_32 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)p292.data), _mm256_loadu_si256((const __m256i*)p293.data)));
    return packed_result;
  }
#endif
  gg_type_32 t0;
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
  gg_type_32 v292;
  gg_type_32 v293;
  gg_type_32 v294;
  int64_t v296;
  v292 = p292;
  v293 = p293;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v294).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v296 = t4;
  {
  t6 = gg_view((v294).data, 8);
  t7 = v296;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v292).data, 8);
  t10 = v296;
  t11 = ((int32_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int32_t)((*t11)));
  t13 = gg_view((v293).data, 8);
  t14 = v296;
  t15 = ((int32_t *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int32_t)((*t15)));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t12) + (uint64_t)(t16)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v294;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:mul32 */
static gg_type_32 gg_function_29(gg_type_32 p389, gg_type_32 p390, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_32 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_mullo_epi32(_mm256_loadu_si256((const __m256i*)p389.data), _mm256_loadu_si256((const __m256i*)p390.data)));
    return packed_result;
  }
#endif
  gg_type_32 t0;
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
  gg_type_32 v389;
  gg_type_32 v390;
  gg_type_32 v391;
  int64_t v393;
  v389 = p389;
  v390 = p390;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v391).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v393 = t4;
  {
  t6 = gg_view((v391).data, 8);
  t7 = v393;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v389).data, 8);
  t10 = v393;
  t11 = ((int32_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int32_t)((*t11)));
  t13 = gg_view((v390).data, 8);
  t14 = v393;
  t15 = ((int32_t *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int32_t)((*t15)));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t12) * (uint64_t)(t16)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v391;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_32 gg_function_22(int32_t p0, int64_t base) {
  gg_type_32 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int32_t * t7;
  int32_t v0;
  gg_type_32 v1;
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
  (*t7) = ((int32_t)(v0));
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
/* FG_PACKED_MATCH:madd16 */
static gg_type_32 gg_function_11(gg_type_20 p191, gg_type_20 p192, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_32 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_madd_epi16(_mm256_loadu_si256((const __m256i*)p191.data), _mm256_loadu_si256((const __m256i*)p192.data)));
    return packed_result;
  }
#endif
  gg_type_32 t0;
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
  gg_type_20 v191;
  gg_type_20 v192;
  gg_type_32 v193;
  int64_t v195;
  int64_t v196;
  v191 = p191;
  v192 = p192;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v193).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v195 = t4;
  {
  t6 = gg_view((v191).data, 16);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v195));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((int16_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int64_t)(((int64_t)((*t10)))));
  t12 = gg_view((v192).data, 16);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v195));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((int16_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int64_t)(((int64_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v191).data, 16);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v195));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((int16_t *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int64_t)(((int64_t)((*t25)))));
  t27 = gg_view((v192).data, 16);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v195));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((int16_t *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int64_t)(((int64_t)((*t33)))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v196 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  t36 = gg_view((v193).data, 8);
  t37 = v195;
  t38 = ((int32_t *)t36.data) + gg_index(t37, t36.len);
  (*t38) = ((int32_t)(((int32_t)(v196))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v193;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:maddubs */
static gg_type_20 gg_function_10(gg_type_7 p183, gg_type_17 p184, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_20 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_maddubs_epi16(_mm256_loadu_si256((const __m256i*)p183.data), _mm256_loadu_si256((const __m256i*)p184.data)));
    return packed_result;
  }
#endif
  gg_type_20 t0;
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
  gg_type_7 v183;
  gg_type_17 v184;
  gg_type_20 v185;
  int64_t v187;
  int32_t v188;
  v183 = p183;
  v184 = p184;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v185).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v187 = t4;
  {
  t6 = gg_view((v183).data, 32);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v187));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((uint8_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int32_t)(((int32_t)((*t10)))));
  t12 = gg_view((v184).data, 32);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v187));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((int8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int32_t)(((int32_t)((*t16)))));
  t18 = ((int32_t)(((int32_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v183).data, 32);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v187));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((uint8_t *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int32_t)(((int32_t)((*t25)))));
  t27 = gg_view((v184).data, 32);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v187));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((int8_t *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int32_t)(((int32_t)((*t33)))));
  t35 = ((int32_t)(((int32_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v188 = ((int32_t)(((int32_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  t36 = gg_view((v185).data, 16);
  t37 = v187;
  t38 = ((int16_t *)t36.data) + gg_index(t37, t36.len);
  t40 = ((int32_t)(v188));
  t41 = ((int64_t)UINT64_C(32768));
  t42 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t41)))));
  t43 = ((t40) < (t42));
  if (t43) {
  {
  t44 = ((int64_t)UINT64_C(32768));
  t39 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t44)))));
  }
  } else {
  t45 = ((int32_t)(v188));
  t46 = ((int32_t)(((int64_t)UINT64_C(32767))));
  t47 = ((t45) > (t46));
  if (t47) {
  {
  t39 = ((int64_t)(((int64_t)UINT64_C(32767))));
  }
  } else {
  {
  t39 = ((int64_t)(v188));
  }
  }
  }
  (*t38) = ((int16_t)(((int16_t)(t39))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v185;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:splat16 */
static gg_type_20 gg_function_8(int16_t p158, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_20 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_set1_epi16(p158));
    return packed_result;
  }
#endif
  gg_type_20 t0;
  int16_t t1;
  int16_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int16_t * t8;
  int16_t v158;
  gg_type_20 v159;
  int64_t v161;
  v158 = p158;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v159).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v161 = t4;
  {
  t6 = gg_view((v159).data, 16);
  t7 = v161;
  t8 = ((int16_t *)t6.data) + gg_index(t7, t6.len);
  (*t8) = ((int16_t)(v158));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v159;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_28(gg_type_42 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
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
  gg_type_32 t29;
  int32_t t30;
  int32_t t31;
  int64_t t32;
  gg_type_32 t33;
  int64_t t34;
  int32_t t35;
  int32_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  gg_slice t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  gg_type_7 * t48;
  gg_slice t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int32_t * t53;
  int16_t t54;
  int64_t t55;
  int64_t t56;
  gg_slice t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  gg_type_17 * t67;
  int64_t t68;
  gg_type_32 * t69;
  int64_t t70;
  gg_type_32 * t71;
  gg_type_32 t72;
  gg_type_20 t73;
  gg_type_7 t74;
  gg_type_17 t75;
  gg_type_20 t76;
  gg_type_32 t77;
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
  int64_t t102;
  int64_t t103;
  gg_type_32 t104;
  int64_t t105;
  gg_type_32 * t106;
  gg_type_32 t107;
  int64_t t108;
  gg_type_32 * t109;
  gg_type_32 t110;
  gg_type_32 t111;
  gg_slice t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int32_t * t116;
  int32_t t117;
  gg_type_32 t118;
  gg_type_32 t119;
  int32_t t120;
  int32_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int32_t * t126;
  int64_t t127;
  gg_type_32 * t128;
  gg_type_32 t129;
  int64_t t130;
  float * t131;
  float t132;
  float t133;
  gg_type_32 t134;
  int64_t t135;
  float * t136;
  float t137;
  gg_type_32 t138;
  gg_type_34 t139;
  float t140;
  float t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  float * t146;
  gg_slice t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  float * t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  gg_type_34 * t165;
  gg_type_34 t166;
  gg_type_34 t167;
  int64_t t168;
  gg_type_34 * t169;
  gg_type_34 t170;
  int64_t t171;
  gg_type_42 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_49 v6;
  gg_type_47 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  gg_type_52 v17;
  gg_type_32 v18;
  int64_t v19;
  gg_type_7 v20;
  gg_type_20 v21;
  int64_t v22;
  gg_type_17 v23;
  int64_t v24;
  int64_t v25;
  gg_type_32 v26;
  int64_t v27;
  gg_type_34 v28;
  gg_type_34 v29;
  int64_t v30;
  int64_t v31;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  v6 = &(((*(v0))).f29);
  v7 = &(((*(v0))).f33);
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(8))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  t2 = ((int64_t)(v5));
  t3 = ((int64_t)(((int64_t)UINT64_C(8))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  v10 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  } else {
  {
  v10 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  }
  t8 = ((int64_t)UINT64_C(16));
  t7 = 0;
label0:;
  if (t7 >= t8) goto label2;
  v11 = t7;
  {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v11));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(((int64_t)UINT64_C(16))));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(v12));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t15 = ((int64_t)(v12));
  t16 = ((int64_t)(((int64_t)UINT64_C(8))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(v4));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(v5));
  t21 = ((int64_t)(((int64_t)UINT64_C(8))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t22)))));
  t24 = ((int64_t)UINT64_C(2));
  t23 = 0;
label3:;
  if (t23 >= t24) goto label5;
  v15 = t23;
  {
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(v15));
  t27 = ((int64_t)(((int64_t)UINT64_C(8))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t28)))));
  memset(&t30, 0, sizeof(t30));
  t31 = ((int32_t)(t30));
  for (t32=0; t32<8; t32++) (t29).data[t32] = t31;
  t33 = t29;
  for (t34=0; t34<8; t34++) (v17).data[t34] = t33;
  memset(&t35, 0, sizeof(t35));
  t36 = ((int32_t)(t35));
  for (t37=0; t37<8; t37++) (v18).data[t37] = t36;
  t39 = ((int64_t)UINT64_C(8));
  t38 = 0;
label6:;
  if (t38 >= t39) goto label8;
  v19 = t38;
  {
  t40 = gg_view((((*(v6))).f24).data, 2048);
  t41 = ((int64_t)(v12));
  t42 = ((int64_t)(v8));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t44 = ((int64_t)(v9));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t44)))));
  t46 = ((int64_t)(v19));
  t47 = ((int64_t)((uint64_t)(t45) + (uint64_t)(t46)));
  t48 = ((gg_type_7 *)t40.data) + gg_index(t47, t40.len);
  v20 = (*t48);
  t49 = gg_view((((*(v6))).f27).data, 4096);
  t50 = ((int64_t)(v14));
  t51 = ((int64_t)(v19));
  t52 = ((int64_t)((uint64_t)(t50) + (uint64_t)(t51)));
  t53 = ((int32_t *)t49.data) + gg_index(t52, t49.len);
  t54 = ((int16_t)(((int16_t)((*t53)))));
  v21 = gg_function_8(t54, base + 0);
  t56 = ((int64_t)UINT64_C(8));
  t55 = 0;
label9:;
  if (t55 >= t56) goto label11;
  v22 = t55;
  {
  t57 = gg_view((((*(v7))).f30).data, 2048);
  t58 = ((int64_t)(v16));
  t59 = ((int64_t)(v22));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t59)))));
  t61 = ((int64_t)(v8));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = ((int64_t)(v9));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  t65 = ((int64_t)(v19));
  t66 = ((int64_t)((uint64_t)(t64) + (uint64_t)(t65)));
  t67 = ((gg_type_17 *)t57.data) + gg_index(t66, t57.len);
  v23 = (*t67);
  /* FG_DIRECT_SLICE_INDEX */
  t68 = v22;
  t69 = ((gg_type_32 *)(v17).data) + gg_index(t68, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t70 = v22;
  t71 = ((gg_type_32 *)(v17).data) + gg_index(t70, ((int64_t)UINT64_C(8)));
  t72 = (*t71);
  t73 = v21;
  t74 = v20;
  t75 = v23;
  t76 = gg_function_10(t74, t75, base + 0);
  t77 = gg_function_11(t73, t76, base + 0);
  (*t69) = gg_function_19(t72, t77, base + 0);
  }
label10:;
  t55++; goto label9;
label11:;
  t78 = ((int64_t)(v5));
  t79 = ((int64_t)(((int64_t)UINT64_C(16))));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  t81 = ((int64_t)(((int64_t)UINT64_C(2))));
  t82 = ((int64_t)(v19));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t82)))));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t83)))));
  t85 = ((int64_t)(v10));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))));
  t87 = ((int64_t)(v16));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))));
  t89 = ((int64_t)(((int64_t)UINT64_C(8))));
  v24 = ((int64_t)(((int64_t)gg_div(t88, t89))));
  t90 = ((int64_t)(v5));
  t91 = ((int64_t)(((int64_t)UINT64_C(16))));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
  t93 = ((int64_t)(((int64_t)UINT64_C(2))));
  t94 = ((int64_t)(v19));
  t95 = ((int64_t)(((int64_t)((uint64_t)(t93) * (uint64_t)(t94)))));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t92) + (uint64_t)(t95)))));
  t97 = ((int64_t)(((int64_t)UINT64_C(1))));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  t99 = ((int64_t)(v10));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  t101 = ((int64_t)(v16));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t101)))));
  t103 = ((int64_t)(((int64_t)UINT64_C(8))));
  v25 = ((int64_t)(((int64_t)gg_div(t102, t103))));
  t104 = v18;
  /* FG_DIRECT_SLICE_INDEX */
  t105 = v24;
  t106 = ((gg_type_32 *)(((*(v7))).f31).data) + gg_index(t105, ((int64_t)UINT64_C(512)));
  t107 = (*t106);
  /* FG_DIRECT_SLICE_INDEX */
  t108 = v25;
  t109 = ((gg_type_32 *)(((*(v7))).f31).data) + gg_index(t108, ((int64_t)UINT64_C(512)));
  t110 = (*t109);
  t111 = gg_function_19(t107, t110, base + 0);
  t112 = gg_view((((*(v6))).f28).data, 4096);
  t113 = ((int64_t)(v14));
  t114 = ((int64_t)(v19));
  t115 = ((int64_t)((uint64_t)(t113) + (uint64_t)(t114)));
  t116 = ((int32_t *)t112.data) + gg_index(t115, t112.len);
  t117 = ((int32_t)((*t116)));
  t118 = gg_function_22(t117, base + 0);
  t119 = gg_function_29(t111, t118, base + 0);
  v18 = gg_function_19(t104, t119, base + 0);
  }
label7:;
  t38++; goto label6;
label8:;
  memset(&t120, 0, sizeof(t120));
  t121 = ((int32_t)(t120));
  for (t122=0; t122<8; t122++) (v26).data[t122] = t121;
  t124 = ((int64_t)UINT64_C(8));
  t123 = 0;
label12:;
  if (t123 >= t124) goto label14;
  v27 = t123;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t125 = v27;
  t126 = ((int32_t *)(v26).data) + gg_index(t125, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t127 = v27;
  t128 = ((gg_type_32 *)(v17).data) + gg_index(t127, ((int64_t)UINT64_C(8)));
  t129 = (*t128);
  (*t126) = gg_function_20(t129, base + 0);
  }
label13:;
  t123++; goto label12;
label14:;
  /* FG_DIRECT_SLICE_INDEX */
  t130 = v13;
  t131 = ((float *)(((*(v6))).f26).data) + gg_index(t130, ((int64_t)UINT64_C(256)));
  t132 = (*t131);
  t133 = ((float)((-(t132))));
  t134 = v18;
  /* FG_DIRECT_SLICE_INDEX */
  t135 = v13;
  t136 = ((float *)(((*(v6))).f25).data) + gg_index(t135, ((int64_t)UINT64_C(256)));
  t137 = ((float)((*t136)));
  t138 = v26;
  t139 = gg_function_24(t137, t138, base + 0);
  v28 = gg_function_30(t133, t134, t139, base + 0);
  memset(&t140, 0, sizeof(t140));
  t141 = ((float)(t140));
  for (t142=0; t142<8; t142++) (v29).data[t142] = t141;
  t144 = ((int64_t)UINT64_C(8));
  t143 = 0;
label15:;
  if (t143 >= t144) goto label17;
  v30 = t143;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t145 = v30;
  t146 = ((float *)(v29).data) + gg_index(t145, ((int64_t)UINT64_C(8)));
  t147 = gg_view((((*(v7))).f32).data, 256);
  t148 = ((int64_t)(v5));
  t149 = ((int64_t)(((int64_t)UINT64_C(16))));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) * (uint64_t)(t149)))));
  t151 = ((int64_t)(v16));
  t152 = ((int64_t)(((int64_t)((uint64_t)(t150) + (uint64_t)(t151)))));
  t153 = ((int64_t)(v30));
  t154 = ((int64_t)((uint64_t)(t152) + (uint64_t)(t153)));
  t155 = ((float *)t147.data) + gg_index(t154, t147.len);
  (*t146) = ((float)((*t155)));
  }
label16:;
  t143++; goto label15;
label17:;
  t156 = ((int64_t)(v3));
  t157 = ((int64_t)(v11));
  t158 = ((int64_t)(((int64_t)((uint64_t)(t156) + (uint64_t)(t157)))));
  t159 = ((int64_t)(((int64_t)UINT64_C(32))));
  t160 = ((int64_t)(((int64_t)((uint64_t)(t158) * (uint64_t)(t159)))));
  t161 = ((int64_t)(v16));
  t162 = ((int64_t)(((int64_t)UINT64_C(8))));
  t163 = ((int64_t)(((int64_t)gg_div(t161, t162))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t160) + (uint64_t)(t163)))));
  /* FG_DIRECT_SLICE_INDEX */
  t164 = v31;
  t165 = ((gg_type_34 *)(((*(v0))).f34).data) + gg_index(t164, ((int64_t)UINT64_C(8192)));
  t166 = v28;
  t167 = v29;
  /* FG_DIRECT_SLICE_INDEX */
  t168 = v31;
  t169 = ((gg_type_34 *)(((*(v0))).f34).data) + gg_index(t168, ((int64_t)UINT64_C(8192)));
  t170 = (*t169);
  (*t165) = gg_function_25(t166, t167, t170, base + 0);
  }
label4:;
  t23++; goto label3;
label5:;
  }
label1:;
  t7++; goto label0;
label2:;
  t171 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg_tiled_witness(t171);
  }
gg_done:;
}
static gg_type_32 gg_function_23(gg_type_32 p0, gg_type_32 p1, int64_t base) {
  gg_type_32 t0;
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
  gg_type_32 v0;
  gg_type_32 v1;
  gg_type_32 v2;
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
static gg_type_7 gg_function_18(gg_type_17 p0, int64_t base) {
  gg_type_7 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t * t7;
  int64_t t8;
  int8_t * t9;
  gg_type_17 v0;
  gg_type_7 v1;
  int64_t v2;
  v0 = p0;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v1).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v2 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((uint8_t *)(v1).data) + gg_index(t6, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v2;
  t9 = ((int8_t *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(32)));
  (*t7) = ((uint8_t)(((uint8_t)((*t9)))));
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
static gg_type_20 gg_function_17(int16_t p0, int16_t p1, int64_t base) {
  gg_type_20 t0;
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
  gg_type_20 v2;
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
static gg_type_17 gg_function_16(gg_type_7 p0, int64_t base) {
  gg_type_17 t0;
  int8_t t1;
  int8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int8_t * t7;
  int64_t t8;
  uint8_t * t9;
  int16_t t10;
  int16_t t11;
  gg_type_7 v0;
  gg_type_17 v1;
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
  t10 = ((int16_t)(((int16_t)((*t9)))));
  t11 = ((int16_t)(((int64_t)UINT64_C(32))));
  (*t7) = ((int8_t)(((int8_t)(((int16_t)((uint64_t)(t10) - (uint64_t)(t11)))))));
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
static void gg_function_15(gg_type_42 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  gg_type_32 t26;
  int32_t t27;
  int32_t t28;
  int64_t t29;
  gg_type_32 t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  gg_type_7 * t42;
  gg_type_7 t43;
  gg_slice t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int32_t * t50;
  int16_t t51;
  gg_slice t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int32_t * t60;
  int16_t t61;
  int64_t t62;
  int64_t t63;
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  gg_type_17 * t74;
  gg_type_17 t75;
  int64_t t76;
  gg_type_32 * t77;
  int64_t t78;
  gg_type_32 * t79;
  gg_type_32 t80;
  gg_type_20 t81;
  gg_type_7 t82;
  gg_type_17 t83;
  gg_type_20 t84;
  gg_type_32 t85;
  int32_t t86;
  int32_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int32_t * t92;
  int64_t t93;
  gg_type_32 * t94;
  gg_type_32 t95;
  int64_t t96;
  float * t97;
  float t98;
  gg_type_32 t99;
  int64_t t100;
  int32_t * t101;
  int32_t t102;
  gg_type_32 t103;
  gg_type_32 t104;
  float t105;
  float t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  float * t111;
  gg_slice t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  float * t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  gg_type_34 * t130;
  gg_type_34 t131;
  gg_type_34 t132;
  int64_t t133;
  gg_type_34 * t134;
  gg_type_34 t135;
  int64_t t136;
  gg_type_42 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_49 v6;
  gg_type_47 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  gg_type_52 v16;
  int64_t v17;
  gg_type_17 v18;
  gg_type_20 v19;
  int64_t v20;
  gg_type_7 v21;
  gg_type_32 v22;
  int64_t v23;
  gg_type_34 v24;
  gg_type_34 v25;
  int64_t v26;
  int64_t v27;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  v6 = &(((*(v0))).f29);
  v7 = &(((*(v0))).f33);
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(8))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  t2 = ((int64_t)(v5));
  t3 = ((int64_t)(((int64_t)UINT64_C(8))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v10 = t4;
  {
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(v10));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t8 = ((int64_t)(v5));
  t9 = ((int64_t)(((int64_t)UINT64_C(16))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t11 = ((int64_t)(v11));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t12 = ((int64_t)(v11));
  t13 = ((int64_t)(((int64_t)UINT64_C(16))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)(((int64_t)UINT64_C(16))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t19)))));
  t21 = ((int64_t)UINT64_C(2));
  t20 = 0;
label3:;
  if (t20 >= t21) goto label5;
  v14 = t20;
  {
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(v14));
  t24 = ((int64_t)(((int64_t)UINT64_C(8))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t25)))));
  memset(&t27, 0, sizeof(t27));
  t28 = ((int32_t)(t27));
  for (t29=0; t29<8; t29++) (t26).data[t29] = t28;
  t30 = t26;
  for (t31=0; t31<8; t31++) (v16).data[t31] = t30;
  t33 = ((int64_t)UINT64_C(8));
  t32 = 0;
label6:;
  if (t32 >= t33) goto label8;
  v17 = t32;
  {
  t34 = gg_view((((*(v6))).f24).data, 2048);
  t35 = ((int64_t)(v11));
  t36 = ((int64_t)(v8));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  t38 = ((int64_t)(v9));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t40 = ((int64_t)(v17));
  t41 = ((int64_t)((uint64_t)(t39) + (uint64_t)(t40)));
  t42 = ((gg_type_7 *)t34.data) + gg_index(t41, t34.len);
  t43 = (*t42);
  v18 = gg_function_16(t43, base + 0);
  t44 = gg_view((((*(v6))).f27).data, 4096);
  t45 = ((int64_t)(v13));
  t46 = ((int64_t)(((int64_t)UINT64_C(2))));
  t47 = ((int64_t)(v17));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)((uint64_t)(t45) + (uint64_t)(t48)));
  t50 = ((int32_t *)t44.data) + gg_index(t49, t44.len);
  t51 = ((int16_t)(((int16_t)((*t50)))));
  t52 = gg_view((((*(v6))).f27).data, 4096);
  t53 = ((int64_t)(v13));
  t54 = ((int64_t)(((int64_t)UINT64_C(2))));
  t55 = ((int64_t)(v17));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t56)))));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t59 = ((int64_t)((uint64_t)(t57) + (uint64_t)(t58)));
  t60 = ((int32_t *)t52.data) + gg_index(t59, t52.len);
  t61 = ((int16_t)(((int16_t)((*t60)))));
  v19 = gg_function_17(t51, t61, base + 0);
  t63 = ((int64_t)UINT64_C(8));
  t62 = 0;
label9:;
  if (t62 >= t63) goto label11;
  v20 = t62;
  {
  t64 = gg_view((((*(v7))).f30).data, 2048);
  t65 = ((int64_t)(v15));
  t66 = ((int64_t)(v20));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = ((int64_t)(v8));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int64_t)(v9));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) + (uint64_t)(t70)))));
  t72 = ((int64_t)(v17));
  t73 = ((int64_t)((uint64_t)(t71) + (uint64_t)(t72)));
  t74 = ((gg_type_17 *)t64.data) + gg_index(t73, t64.len);
  t75 = (*t74);
  v21 = gg_function_18(t75, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t76 = v20;
  t77 = ((gg_type_32 *)(v16).data) + gg_index(t76, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t78 = v20;
  t79 = ((gg_type_32 *)(v16).data) + gg_index(t78, ((int64_t)UINT64_C(8)));
  t80 = (*t79);
  t81 = v19;
  t82 = v21;
  t83 = v18;
  t84 = gg_function_10(t82, t83, base + 0);
  t85 = gg_function_11(t81, t84, base + 0);
  (*t77) = gg_function_19(t80, t85, base + 0);
  }
label10:;
  t62++; goto label9;
label11:;
  }
label7:;
  t32++; goto label6;
label8:;
  memset(&t86, 0, sizeof(t86));
  t87 = ((int32_t)(t86));
  for (t88=0; t88<8; t88++) (v22).data[t88] = t87;
  t90 = ((int64_t)UINT64_C(8));
  t89 = 0;
label12:;
  if (t89 >= t90) goto label14;
  v23 = t89;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t91 = v23;
  t92 = ((int32_t *)(v22).data) + gg_index(t91, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t93 = v23;
  t94 = ((gg_type_32 *)(v16).data) + gg_index(t93, ((int64_t)UINT64_C(8)));
  t95 = (*t94);
  (*t92) = gg_function_20(t95, base + 0);
  }
label13:;
  t89++; goto label12;
label14:;
  /* FG_DIRECT_SLICE_INDEX */
  t96 = v12;
  t97 = ((float *)(((*(v6))).f25).data) + gg_index(t96, ((int64_t)UINT64_C(256)));
  t98 = ((float)((*t97)));
  t99 = v22;
  /* FG_DIRECT_SLICE_INDEX */
  t100 = v13;
  t101 = ((int32_t *)(((*(v6))).f28).data) + gg_index(t100, ((int64_t)UINT64_C(4096)));
  t102 = ((int32_t)((*t101)));
  t103 = gg_function_22(t102, base + 0);
  t104 = gg_function_23(t99, t103, base + 0);
  v24 = gg_function_24(t98, t104, base + 0);
  memset(&t105, 0, sizeof(t105));
  t106 = ((float)(t105));
  for (t107=0; t107<8; t107++) (v25).data[t107] = t106;
  t109 = ((int64_t)UINT64_C(8));
  t108 = 0;
label15:;
  if (t108 >= t109) goto label17;
  v26 = t108;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t110 = v26;
  t111 = ((float *)(v25).data) + gg_index(t110, ((int64_t)UINT64_C(8)));
  t112 = gg_view((((*(v7))).f32).data, 256);
  t113 = ((int64_t)(v5));
  t114 = ((int64_t)(((int64_t)UINT64_C(16))));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t113) * (uint64_t)(t114)))));
  t116 = ((int64_t)(v15));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = ((int64_t)(v26));
  t119 = ((int64_t)((uint64_t)(t117) + (uint64_t)(t118)));
  t120 = ((float *)t112.data) + gg_index(t119, t112.len);
  (*t111) = ((float)((*t120)));
  }
label16:;
  t108++; goto label15;
label17:;
  t121 = ((int64_t)(v3));
  t122 = ((int64_t)(v10));
  t123 = ((int64_t)(((int64_t)((uint64_t)(t121) + (uint64_t)(t122)))));
  t124 = ((int64_t)(((int64_t)UINT64_C(32))));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) * (uint64_t)(t124)))));
  t126 = ((int64_t)(v15));
  t127 = ((int64_t)(((int64_t)UINT64_C(8))));
  t128 = ((int64_t)(((int64_t)gg_div(t126, t127))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t125) + (uint64_t)(t128)))));
  /* FG_DIRECT_SLICE_INDEX */
  t129 = v27;
  t130 = ((gg_type_34 *)(((*(v0))).f34).data) + gg_index(t129, ((int64_t)UINT64_C(8192)));
  t131 = v24;
  t132 = v25;
  /* FG_DIRECT_SLICE_INDEX */
  t133 = v27;
  t134 = ((gg_type_34 *)(((*(v0))).f34).data) + gg_index(t133, ((int64_t)UINT64_C(8192)));
  t135 = (*t134);
  (*t130) = gg_function_25(t131, t132, t135, base + 0);
  }
label4:;
  t20++; goto label3;
label5:;
  }
label1:;
  t4++; goto label0;
label2:;
  t136 = ((int64_t)(((int64_t)UINT64_C(14))));
  fg_tiled_witness(t136);
  }
gg_done:;
}
static gg_type_17 gg_function_6(gg_type_17 p0, int64_t base) {
  gg_type_17 t0;
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
  uint8_t t11;
  gg_type_17 v0;
  gg_type_17 v1;
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
  t9 = ((int8_t *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(32)));
  t10 = ((uint8_t)(((uint8_t)((*t9)))));
  t11 = ((uint8_t)(((int64_t)UINT64_C(128))));
  (*t7) = ((int8_t)(((int8_t)(((t10) ^ (t11))))));
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
static int64_t gg_function_3(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(15))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)UINT64_C(16))));
  t5 = ((int64_t)(((int64_t)gg_div(t3, t4))));
  t6 = ((int64_t)(((int64_t)UINT64_C(16))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_5(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, gg_type_42 p8, uint8_t p9, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
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
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  gg_slice t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  gg_type_22 * t26;
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
  gg_type_15 * t40;
  int64_t t41;
  float * t42;
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
  gg_slice t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  gg_type_32 * t57;
  gg_slice t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int32_t * t62;
  int64_t t63;
  int16_t * t64;
  int64_t t65;
  int64_t t66;
  gg_slice t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  gg_type_17 * t71;
  uint8_t t72;
  int64_t t73;
  gg_type_17 * t74;
  gg_type_17 t75;
  int64_t t76;
  gg_type_17 * t77;
  int64_t t78;
  float * t79;
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
  gg_slice t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  gg_type_32 * t94;
  gg_slice t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int32_t * t99;
  int64_t t100;
  int64_t t101;
  gg_slice t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  gg_type_17 * t106;
  uint8_t t107;
  gg_type_17 t108;
  int8_t t109;
  int8_t t110;
  int64_t t111;
  gg_type_17 t112;
  int8_t t113;
  int8_t t114;
  int64_t t115;
  gg_slice v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  gg_type_42 v8;
  uint8_t v9;
  gg_type_47 v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  gg_type_22 v16;
  int64_t v17;
  gg_type_48 v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
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
  v10 = &(((*(v8))).f33);
  t0 = ((int64_t)(v7));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  } else {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  }
  t4 = v7;
  t3 = 0;
label0:;
  if (t3 >= t4) goto label2;
  v12 = t3;
  {
  t6 = ((int64_t)(v3));
  t7 = gg_function_3(t6, base + 0);
  t8 = t7;
  t5 = 0;
label3:;
  if (t5 >= t8) goto label5;
  v13 = t5;
  {
  t9 = ((int64_t)(v13));
  t10 = ((int64_t)(v7));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(v12));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)UINT64_C(8))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t15 = ((int64_t)(v12));
  t16 = ((int64_t)(((int64_t)UINT64_C(16))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(v13));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t19 = ((int64_t)(v13));
  t20 = ((int64_t)(v3));
  t21 = ((t19) < (t20));
  if (t21) {
  {
  t22 = v1;
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(v13));
  t25 = ((int64_t)((uint64_t)(t23) + (uint64_t)(t24)));
  t26 = ((gg_type_22 *)t22.data) + gg_index(t25, t22.len);
  v16 = (*t26);
  t27 = ((int64_t)(((int64_t)((v16).f22))));
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(((int64_t)gg_mod(t27, t28))));
  t30 = ((int64_t)(((int64_t)((v16).f23))));
  t31 = ((int64_t)(v4));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t32)))));
  t34 = ((int64_t)(v5));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((int64_t)(v6));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t38 = ((int64_t)(v12));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  /* FG_DIRECT_SLICE_INDEX */
  t39 = v17;
  t40 = ((gg_type_15 *)(v0).data) + gg_index(t39, (v0).len);
  v18 = t40;
  /* FG_DIRECT_SLICE_INDEX */
  t41 = v15;
  t42 = ((float *)(((*(v10))).f32).data) + gg_index(t41, ((int64_t)UINT64_C(256)));
  (*t42) = ((float)(((*(v18))).f19));
  t44 = ((int64_t)UINT64_C(16));
  t43 = 0;
label6:;
  if (t43 >= t44) goto label8;
  v19 = t43;
  {
  t45 = ((int64_t)(v12));
  t46 = ((int64_t)(((int64_t)UINT64_C(16))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = ((int64_t)(v19));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t50 = ((int64_t)(v11));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))));
  t52 = ((int64_t)(v13));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t51) + (uint64_t)(t52)))));
  t53 = gg_view((((*(v10))).f31).data, 512);
  t54 = ((int64_t)(v20));
  t55 = ((int64_t)(((int64_t)UINT64_C(8))));
  t56 = ((int64_t)gg_div(t54, t55));
  t57 = ((gg_type_32 *)t53.data) + gg_index(t56, t53.len);
  t58 = gg_view(((*t57)).data, 8);
  t59 = ((int64_t)(v20));
  t60 = ((int64_t)(((int64_t)UINT64_C(8))));
  t61 = ((int64_t)gg_mod(t59, t60));
  t62 = ((int32_t *)t58.data) + gg_index(t61, t58.len);
  /* FG_DIRECT_SLICE_INDEX */
  t63 = v19;
  t64 = ((int16_t *)(((*(v18))).f21).data) + gg_index(t63, ((int64_t)UINT64_C(16)));
  (*t62) = ((int32_t)(((int32_t)((*t64)))));
  }
label7:;
  t43++; goto label6;
label8:;
  t66 = ((int64_t)UINT64_C(8));
  t65 = 0;
label9:;
  if (t65 >= t66) goto label11;
  v21 = t65;
  {
  t67 = gg_view((((*(v10))).f30).data, 2048);
  t68 = ((int64_t)(v14));
  t69 = ((int64_t)(v21));
  t70 = ((int64_t)((uint64_t)(t68) + (uint64_t)(t69)));
  t71 = ((gg_type_17 *)t67.data) + gg_index(t70, t67.len);
  t72 = v9;
  if (t72) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t73 = v21;
  t74 = ((gg_type_17 *)(((*(v18))).f20).data) + gg_index(t73, ((int64_t)UINT64_C(8)));
  t75 = (*t74);
  (*t71) = gg_function_6(t75, base + 0);
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t76 = v21;
  t77 = ((gg_type_17 *)(((*(v18))).f20).data) + gg_index(t76, ((int64_t)UINT64_C(8)));
  (*t71) = (*t77);
  }
  }
  }
label10:;
  t65++; goto label9;
label11:;
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t78 = v15;
  t79 = ((float *)(((*(v10))).f32).data) + gg_index(t78, ((int64_t)UINT64_C(256)));
  (*t79) = ((float)(gg_double_bits(UINT64_C(0))));
  t81 = ((int64_t)UINT64_C(16));
  t80 = 0;
label12:;
  if (t80 >= t81) goto label14;
  v22 = t80;
  {
  t82 = ((int64_t)(v12));
  t83 = ((int64_t)(((int64_t)UINT64_C(16))));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) * (uint64_t)(t83)))));
  t85 = ((int64_t)(v22));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t85)))));
  t87 = ((int64_t)(v11));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t87)))));
  t89 = ((int64_t)(v13));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))));
  t90 = gg_view((((*(v10))).f31).data, 512);
  t91 = ((int64_t)(v23));
  t92 = ((int64_t)(((int64_t)UINT64_C(8))));
  t93 = ((int64_t)gg_div(t91, t92));
  t94 = ((gg_type_32 *)t90.data) + gg_index(t93, t90.len);
  t95 = gg_view(((*t94)).data, 8);
  t96 = ((int64_t)(v23));
  t97 = ((int64_t)(((int64_t)UINT64_C(8))));
  t98 = ((int64_t)gg_mod(t96, t97));
  t99 = ((int32_t *)t95.data) + gg_index(t98, t95.len);
  (*t99) = ((int32_t)(((int64_t)UINT64_C(0))));
  }
label13:;
  t80++; goto label12;
label14:;
  t101 = ((int64_t)UINT64_C(8));
  t100 = 0;
label15:;
  if (t100 >= t101) goto label17;
  v24 = t100;
  {
  t102 = gg_view((((*(v10))).f30).data, 2048);
  t103 = ((int64_t)(v14));
  t104 = ((int64_t)(v24));
  t105 = ((int64_t)((uint64_t)(t103) + (uint64_t)(t104)));
  t106 = ((gg_type_17 *)t102.data) + gg_index(t105, t102.len);
  t107 = v9;
  if (t107) {
  {
  memset(&t109, 0, sizeof(t109));
  t110 = ((int8_t)(t109));
  for (t111=0; t111<32; t111++) (t108).data[t111] = t110;
  t112 = t108;
  (*t106) = gg_function_6(t112, base + 0);
  }
  } else {
  {
  memset(&t113, 0, sizeof(t113));
  t114 = ((int8_t)(t113));
  for (t115=0; t115<32; t115++) ((*t106)).data[t115] = t114;
  }
  }
  }
label16:;
  t100++; goto label15;
label17:;
  }
  }
  }
label4:;
  t5++; goto label3;
label5:;
  }
label1:;
  t3++; goto label0;
label2:;
  }
gg_done:;
}
/* FG_PACKED_MATCH:shr8 */
static gg_type_7 gg_function_14(gg_type_7 p233, uint8_t p234, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_7 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)p233.data), _mm_cvtsi32_si128((p234 & 7))), _mm256_set1_epi8((unsigned char)(255u >> (p234 & 7)))));
    return packed_result;
  }
#endif
  gg_type_7 t0;
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
  gg_type_7 v233;
  uint8_t v234;
  gg_type_7 v235;
  int64_t v237;
  v233 = p233;
  v234 = p234;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v235).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v237 = t4;
  {
  t6 = gg_view((v235).data, 32);
  t7 = v237;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v233).data, 32);
  t10 = v237;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v234));
  (*t8) = ((uint8_t)(((t12) >> ((uint64_t)(t13) & 7))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v235;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:and8 */
static gg_type_7 gg_function_13(gg_type_7 p226, uint8_t p227, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_7 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_loadu_si256((const __m256i*)p226.data), _mm256_set1_epi8(p227)));
    return packed_result;
  }
#endif
  gg_type_7 t0;
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
  gg_type_7 v226;
  uint8_t v227;
  gg_type_7 v228;
  int64_t v230;
  v226 = p226;
  v227 = p227;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v228).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v230 = t4;
  {
  t6 = gg_view((v228).data, 32);
  t7 = v230;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v226).data, 32);
  t10 = v230;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v227));
  (*t8) = ((uint8_t)(((t12) & (t13))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v228;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_12(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_type_42 p6, int64_t base) {
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
  uint8_t t26;
  gg_slice t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  gg_type_3 * t37;
  int64_t t38;
  float * t39;
  uint16_t t40;
  float t41;
  int64_t t42;
  float * t43;
  uint16_t t44;
  float t45;
  int64_t t46;
  int64_t t47;
  gg_slice t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int32_t * t52;
  int64_t t53;
  uint8_t * t54;
  uint8_t t55;
  uint8_t t56;
  gg_slice t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int32_t * t61;
  gg_slice t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t * t66;
  uint8_t t67;
  uint8_t t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int32_t * t75;
  gg_slice t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint8_t * t80;
  uint8_t t81;
  uint8_t t82;
  uint8_t t83;
  int64_t t84;
  uint8_t * t85;
  uint8_t t86;
  int64_t t87;
  uint8_t t88;
  int64_t t89;
  uint8_t t90;
  gg_slice t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int32_t * t97;
  gg_slice t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  uint8_t * t102;
  uint8_t t103;
  int64_t t104;
  uint8_t t105;
  gg_slice t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  uint8_t * t110;
  uint8_t t111;
  int64_t t112;
  uint8_t t113;
  int64_t t114;
  uint8_t t115;
  gg_slice t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  gg_type_7 * t122;
  int64_t t123;
  gg_type_7 * t124;
  gg_type_7 t125;
  uint8_t t126;
  gg_slice t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  gg_type_7 * t135;
  int64_t t136;
  gg_type_7 * t137;
  gg_type_7 t138;
  uint8_t t139;
  int64_t t140;
  float * t141;
  int64_t t142;
  float * t143;
  int64_t t144;
  int64_t t145;
  gg_slice t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int32_t * t150;
  gg_slice t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int32_t * t155;
  gg_slice t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  gg_type_7 * t160;
  uint8_t t161;
  uint8_t t162;
  int64_t t163;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_42 v6;
  gg_type_49 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  gg_type_51 v13;
  int64_t v14;
  int64_t v15;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  v7 = &(((*(v6))).f29);
  t1 = v5;
  t0 = 0;
label0:;
  if (t0 >= t1) goto label2;
  v8 = t0;
  {
  t3 = ((int64_t)(v4));
  t4 = gg_function_3(t3, base + 0);
  t5 = t4;
  t2 = 0;
label3:;
  if (t2 >= t5) goto label5;
  v9 = t2;
  {
  t6 = ((int64_t)(v8));
  t7 = ((int64_t)(((int64_t)UINT64_C(16))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = ((int64_t)(v9));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t10 = ((int64_t)(v9));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((int64_t)(v8));
  t16 = ((int64_t)(((int64_t)UINT64_C(8))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t17)))));
  t18 = ((int64_t)(v9));
  t19 = ((int64_t)(v5));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t21 = ((int64_t)(v8));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(8))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t24 = ((int64_t)(v9));
  t25 = ((int64_t)(v4));
  t26 = ((t24) < (t25));
  if (t26) {
  {
  t27 = v0;
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v9));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = ((int64_t)(v8));
  t36 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  t37 = ((gg_type_3 *)t27.data) + gg_index(t36, t27.len);
  v13 = t37;
  /* FG_DIRECT_SLICE_INDEX */
  t38 = v10;
  t39 = ((float *)(((*(v7))).f25).data) + gg_index(t38, ((int64_t)UINT64_C(256)));
  t40 = ((uint16_t)(((*(v13))).f11));
  t41 = fg_tiled_half(t40);
  (*t39) = ((float)(t41));
  /* FG_DIRECT_SLICE_INDEX */
  t42 = v10;
  t43 = ((float *)(((*(v7))).f26).data) + gg_index(t42, ((int64_t)UINT64_C(256)));
  t44 = ((uint16_t)(((*(v13))).f12));
  t45 = fg_tiled_half(t44);
  (*t43) = ((float)(t45));
  t47 = ((int64_t)UINT64_C(4));
  t46 = 0;
label6:;
  if (t46 >= t47) goto label8;
  v14 = t46;
  {
  t48 = gg_view((((*(v7))).f27).data, 4096);
  t49 = ((int64_t)(v11));
  t50 = ((int64_t)(v14));
  t51 = ((int64_t)((uint64_t)(t49) + (uint64_t)(t50)));
  t52 = ((int32_t *)t48.data) + gg_index(t51, t48.len);
  /* FG_DIRECT_SLICE_INDEX */
  t53 = v14;
  t54 = ((uint8_t *)(((*(v13))).f13).data) + gg_index(t53, ((int64_t)UINT64_C(12)));
  t55 = ((uint8_t)((*t54)));
  t56 = ((uint8_t)(((int64_t)UINT64_C(63))));
  (*t52) = ((int32_t)(((int32_t)(((t55) & (t56))))));
  t57 = gg_view((((*(v7))).f28).data, 4096);
  t58 = ((int64_t)(v11));
  t59 = ((int64_t)(v14));
  t60 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t59)));
  t61 = ((int32_t *)t57.data) + gg_index(t60, t57.len);
  t62 = gg_view((((*(v13))).f13).data, 12);
  t63 = ((int64_t)(v14));
  t64 = ((int64_t)(((int64_t)UINT64_C(4))));
  t65 = ((int64_t)((uint64_t)(t63) + (uint64_t)(t64)));
  t66 = ((uint8_t *)t62.data) + gg_index(t65, t62.len);
  t67 = ((uint8_t)((*t66)));
  t68 = ((uint8_t)(((int64_t)UINT64_C(63))));
  (*t61) = ((int32_t)(((int32_t)(((t67) & (t68))))));
  t69 = gg_view((((*(v7))).f27).data, 4096);
  t70 = ((int64_t)(v11));
  t71 = ((int64_t)(v14));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)UINT64_C(4))));
  t74 = ((int64_t)((uint64_t)(t72) + (uint64_t)(t73)));
  t75 = ((int32_t *)t69.data) + gg_index(t74, t69.len);
  t76 = gg_view((((*(v13))).f13).data, 12);
  t77 = ((int64_t)(v14));
  t78 = ((int64_t)(((int64_t)UINT64_C(8))));
  t79 = ((int64_t)((uint64_t)(t77) + (uint64_t)(t78)));
  t80 = ((uint8_t *)t76.data) + gg_index(t79, t76.len);
  t81 = ((uint8_t)((*t80)));
  t82 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t83 = ((uint8_t)(((t81) & (t82))));
  /* FG_DIRECT_SLICE_INDEX */
  t84 = v14;
  t85 = ((uint8_t *)(((*(v13))).f13).data) + gg_index(t84, ((int64_t)UINT64_C(12)));
  t86 = ((uint8_t)((*t85)));
  t87 = ((int64_t)(((int64_t)UINT64_C(6))));
  t88 = ((uint8_t)(((t86) >> ((uint64_t)(t87) & 7))));
  t89 = ((int64_t)(((int64_t)UINT64_C(4))));
  t90 = ((uint8_t)(((uint8_t)((uint64_t)(t88) << ((uint64_t)(t89) & 7)))));
  (*t75) = ((int32_t)(((int32_t)(((t83) | (t90))))));
  t91 = gg_view((((*(v7))).f28).data, 4096);
  t92 = ((int64_t)(v11));
  t93 = ((int64_t)(v14));
  t94 = ((int64_t)(((int64_t)((uint64_t)(t92) + (uint64_t)(t93)))));
  t95 = ((int64_t)(((int64_t)UINT64_C(4))));
  t96 = ((int64_t)((uint64_t)(t94) + (uint64_t)(t95)));
  t97 = ((int32_t *)t91.data) + gg_index(t96, t91.len);
  t98 = gg_view((((*(v13))).f13).data, 12);
  t99 = ((int64_t)(v14));
  t100 = ((int64_t)(((int64_t)UINT64_C(8))));
  t101 = ((int64_t)((uint64_t)(t99) + (uint64_t)(t100)));
  t102 = ((uint8_t *)t98.data) + gg_index(t101, t98.len);
  t103 = ((uint8_t)((*t102)));
  t104 = ((int64_t)(((int64_t)UINT64_C(4))));
  t105 = ((uint8_t)(((t103) >> ((uint64_t)(t104) & 7))));
  t106 = gg_view((((*(v13))).f13).data, 12);
  t107 = ((int64_t)(v14));
  t108 = ((int64_t)(((int64_t)UINT64_C(4))));
  t109 = ((int64_t)((uint64_t)(t107) + (uint64_t)(t108)));
  t110 = ((uint8_t *)t106.data) + gg_index(t109, t106.len);
  t111 = ((uint8_t)((*t110)));
  t112 = ((int64_t)(((int64_t)UINT64_C(6))));
  t113 = ((uint8_t)(((t111) >> ((uint64_t)(t112) & 7))));
  t114 = ((int64_t)(((int64_t)UINT64_C(4))));
  t115 = ((uint8_t)(((uint8_t)((uint64_t)(t113) << ((uint64_t)(t114) & 7)))));
  (*t97) = ((int32_t)(((int32_t)(((t105) | (t115))))));
  t116 = gg_view((((*(v7))).f24).data, 2048);
  t117 = ((int64_t)(v12));
  t118 = ((int64_t)(((int64_t)UINT64_C(2))));
  t119 = ((int64_t)(v14));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t118) * (uint64_t)(t119)))));
  t121 = ((int64_t)((uint64_t)(t117) + (uint64_t)(t120)));
  t122 = ((gg_type_7 *)t116.data) + gg_index(t121, t116.len);
  /* FG_DIRECT_SLICE_INDEX */
  t123 = v14;
  t124 = ((gg_type_7 *)(((*(v13))).f14).data) + gg_index(t123, ((int64_t)UINT64_C(4)));
  t125 = (*t124);
  t126 = ((uint8_t)(((int64_t)UINT64_C(15))));
  (*t122) = gg_function_13(t125, t126, base + 0);
  t127 = gg_view((((*(v7))).f24).data, 2048);
  t128 = ((int64_t)(v12));
  t129 = ((int64_t)(((int64_t)UINT64_C(2))));
  t130 = ((int64_t)(v14));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t129) * (uint64_t)(t130)))));
  t132 = ((int64_t)(((int64_t)((uint64_t)(t128) + (uint64_t)(t131)))));
  t133 = ((int64_t)(((int64_t)UINT64_C(1))));
  t134 = ((int64_t)((uint64_t)(t132) + (uint64_t)(t133)));
  t135 = ((gg_type_7 *)t127.data) + gg_index(t134, t127.len);
  /* FG_DIRECT_SLICE_INDEX */
  t136 = v14;
  t137 = ((gg_type_7 *)(((*(v13))).f14).data) + gg_index(t136, ((int64_t)UINT64_C(4)));
  t138 = (*t137);
  t139 = ((uint8_t)(((int64_t)UINT64_C(4))));
  (*t135) = gg_function_14(t138, t139, base + 0);
  }
label7:;
  t46++; goto label6;
label8:;
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t140 = v10;
  t141 = ((float *)(((*(v7))).f25).data) + gg_index(t140, ((int64_t)UINT64_C(256)));
  (*t141) = ((float)(gg_double_bits(UINT64_C(0))));
  /* FG_DIRECT_SLICE_INDEX */
  t142 = v10;
  t143 = ((float *)(((*(v7))).f26).data) + gg_index(t142, ((int64_t)UINT64_C(256)));
  (*t143) = ((float)(gg_double_bits(UINT64_C(0))));
  t145 = ((int64_t)UINT64_C(8));
  t144 = 0;
label9:;
  if (t144 >= t145) goto label11;
  v15 = t144;
  {
  t146 = gg_view((((*(v7))).f27).data, 4096);
  t147 = ((int64_t)(v11));
  t148 = ((int64_t)(v15));
  t149 = ((int64_t)((uint64_t)(t147) + (uint64_t)(t148)));
  t150 = ((int32_t *)t146.data) + gg_index(t149, t146.len);
  (*t150) = ((int32_t)(((int64_t)UINT64_C(0))));
  t151 = gg_view((((*(v7))).f28).data, 4096);
  t152 = ((int64_t)(v11));
  t153 = ((int64_t)(v15));
  t154 = ((int64_t)((uint64_t)(t152) + (uint64_t)(t153)));
  t155 = ((int32_t *)t151.data) + gg_index(t154, t151.len);
  (*t155) = ((int32_t)(((int64_t)UINT64_C(0))));
  t156 = gg_view((((*(v7))).f24).data, 2048);
  t157 = ((int64_t)(v12));
  t158 = ((int64_t)(v15));
  t159 = ((int64_t)((uint64_t)(t157) + (uint64_t)(t158)));
  t160 = ((gg_type_7 *)t156.data) + gg_index(t159, t156.len);
  memset(&t161, 0, sizeof(t161));
  t162 = ((uint8_t)(t161));
  for (t163=0; t163<32; t163++) ((*t160)).data[t163] = t162;
  }
label10:;
  t144++; goto label9;
label11:;
  }
  }
  }
label4:;
  t2++; goto label3;
label5:;
  }
label1:;
  t0++; goto label0;
label2:;
  }
gg_done:;
}
static void gg_function_7(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_type_42 p6, int64_t base) {
  int8_t t0;
  int8_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int8_t * t6;
  int16_t t7;
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
  float * t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  gg_slice t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  gg_type_10 * t47;
  int64_t t48;
  float * t49;
  uint16_t t50;
  float t51;
  int64_t t52;
  int64_t t53;
  gg_slice t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int32_t * t58;
  int64_t t59;
  int8_t * t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  gg_type_7 * t66;
  gg_slice t67;
  int64_t t68;
  uint8_t * t69;
  gg_slice t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  gg_type_7 * t74;
  gg_slice t75;
  int64_t t76;
  uint8_t * t77;
  gg_slice t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  gg_type_7 * t84;
  gg_slice t85;
  int64_t t86;
  uint8_t * t87;
  gg_slice t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  gg_type_7 * t94;
  gg_slice t95;
  int64_t t96;
  uint8_t * t97;
  uint8_t t98;
  uint8_t t99;
  uint8_t t100;
  uint8_t t101;
  uint8_t t102;
  uint8_t t103;
  int64_t t104;
  uint8_t t105;
  gg_slice t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  gg_type_7 * t114;
  gg_slice t115;
  int64_t t116;
  uint8_t * t117;
  uint8_t t118;
  uint8_t t119;
  uint8_t t120;
  uint8_t t121;
  uint8_t t122;
  uint8_t t123;
  int64_t t124;
  uint8_t t125;
  gg_slice t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  gg_type_7 * t134;
  gg_slice t135;
  int64_t t136;
  uint8_t * t137;
  uint8_t t138;
  int64_t t139;
  uint8_t t140;
  uint8_t t141;
  uint8_t t142;
  uint8_t t143;
  gg_slice t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  gg_type_7 * t152;
  gg_slice t153;
  int64_t t154;
  uint8_t * t155;
  uint8_t t156;
  int64_t t157;
  uint8_t t158;
  uint8_t t159;
  uint8_t t160;
  uint8_t t161;
  int64_t t162;
  uint8_t t163;
  int64_t t164;
  int64_t t165;
  gg_type_20 t166;
  gg_slice t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  gg_type_7 * t171;
  gg_type_7 t172;
  gg_type_17 t173;
  gg_type_20 t174;
  int64_t t175;
  int32_t * t176;
  int32_t t177;
  int64_t t178;
  int32_t * t179;
  int32_t t180;
  int32_t t181;
  int64_t t182;
  int32_t * t183;
  int32_t t184;
  int64_t t185;
  int32_t * t186;
  int32_t t187;
  int32_t t188;
  int64_t t189;
  int32_t * t190;
  int32_t t191;
  int64_t t192;
  int32_t * t193;
  int32_t t194;
  int32_t t195;
  int64_t t196;
  int32_t * t197;
  int32_t t198;
  int64_t t199;
  int32_t * t200;
  int32_t t201;
  int32_t t202;
  int32_t t203;
  int32_t t204;
  gg_slice t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int32_t * t211;
  int32_t t212;
  int32_t t213;
  int32_t t214;
  int32_t t215;
  int32_t t216;
  int32_t t217;
  int32_t t218;
  gg_slice t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int32_t * t227;
  int32_t t228;
  int32_t t229;
  int32_t t230;
  int32_t t231;
  int32_t t232;
  int32_t t233;
  int32_t t234;
  int64_t t235;
  int32_t * t236;
  int64_t t237;
  float * t238;
  int64_t t239;
  int64_t t240;
  gg_slice t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int32_t * t245;
  gg_slice t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int32_t * t250;
  int64_t t251;
  int64_t t252;
  gg_slice t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  gg_type_7 * t257;
  uint8_t t258;
  uint8_t t259;
  int64_t t260;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_42 v6;
  gg_type_49 v7;
  gg_type_17 v8;
  int64_t v9;
  gg_type_20 v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  gg_type_50 v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  uint8_t v20;
  uint8_t v21;
  uint8_t v22;
  int32_t v23;
  int64_t v24;
  gg_type_32 v25;
  int32_t v26;
  int32_t v27;
  int64_t v28;
  int64_t v29;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  v7 = &(((*(v6))).f29);
  memset(&t0, 0, sizeof(t0));
  t1 = ((int8_t)(t0));
  for (t2=0; t2<32; t2++) (v8).data[t2] = t1;
  t4 = ((int64_t)UINT64_C(32));
  t3 = 0;
label0:;
  if (t3 >= t4) goto label2;
  v9 = t3;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t5 = v9;
  t6 = ((int8_t *)(v8).data) + gg_index(t5, ((int64_t)UINT64_C(32)));
  (*t6) = ((int8_t)(((int64_t)UINT64_C(1))));
  }
label1:;
  t3++; goto label0;
label2:;
  t7 = ((int16_t)(((int64_t)UINT64_C(1))));
  v10 = gg_function_8(t7, base + 0);
  t9 = v5;
  t8 = 0;
label3:;
  if (t8 >= t9) goto label5;
  v11 = t8;
  {
  t11 = ((int64_t)(v4));
  t12 = gg_function_3(t11, base + 0);
  t13 = t12;
  t10 = 0;
label6:;
  if (t10 >= t13) goto label8;
  v12 = t10;
  {
  t14 = ((int64_t)(v11));
  t15 = ((int64_t)(((int64_t)UINT64_C(16))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(v12));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  t18 = ((int64_t)(v12));
  t19 = ((int64_t)(((int64_t)UINT64_C(16))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t21 = ((int64_t)(v5));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(v11));
  t24 = ((int64_t)(((int64_t)UINT64_C(16))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t25)))));
  t26 = ((int64_t)(v12));
  t27 = ((int64_t)(v5));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t29 = ((int64_t)(v11));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(8))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  /* FG_DIRECT_SLICE_INDEX */
  t32 = v13;
  t33 = ((float *)(((*(v7))).f26).data) + gg_index(t32, ((int64_t)UINT64_C(256)));
  (*t33) = ((float)(gg_double_bits(UINT64_C(0))));
  t34 = ((int64_t)(v12));
  t35 = ((int64_t)(v4));
  t36 = ((t34) < (t35));
  if (t36) {
  {
  t37 = v0;
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(v12));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  t41 = ((int64_t)(v2));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) * (uint64_t)(t41)))));
  t43 = ((int64_t)(v3));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = ((int64_t)(v11));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((gg_type_10 *)t37.data) + gg_index(t46, t37.len);
  v16 = t47;
  /* FG_DIRECT_SLICE_INDEX */
  t48 = v13;
  t49 = ((float *)(((*(v7))).f25).data) + gg_index(t48, ((int64_t)UINT64_C(256)));
  t50 = ((uint16_t)(((*(v16))).f18));
  t51 = fg_tiled_half(t50);
  (*t49) = ((float)(t51));
  t53 = ((int64_t)UINT64_C(16));
  t52 = 0;
label9:;
  if (t52 >= t53) goto label11;
  v17 = t52;
  {
  t54 = gg_view((((*(v7))).f27).data, 4096);
  t55 = ((int64_t)(v14));
  t56 = ((int64_t)(v17));
  t57 = ((int64_t)((uint64_t)(t55) + (uint64_t)(t56)));
  t58 = ((int32_t *)t54.data) + gg_index(t57, t54.len);
  /* FG_DIRECT_SLICE_INDEX */
  t59 = v17;
  t60 = ((int8_t *)(((*(v16))).f17).data) + gg_index(t59, ((int64_t)UINT64_C(16)));
  (*t58) = ((int32_t)(((int32_t)((*t60)))));
  }
label10:;
  t52++; goto label9;
label11:;
  t62 = ((int64_t)UINT64_C(2));
  t61 = 0;
label12:;
  if (t61 >= t62) goto label14;
  v18 = t61;
  {
  t64 = ((int64_t)UINT64_C(32));
  t63 = 0;
label15:;
  if (t63 >= t64) goto label17;
  v19 = t63;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t65 = v18;
  t66 = ((gg_type_7 *)(((*(v16))).f16).data) + gg_index(t65, ((int64_t)UINT64_C(2)));
  t67 = gg_view(((*t66)).data, 32);
  t68 = v19;
  t69 = ((uint8_t *)t67.data) + gg_index(t68, t67.len);
  v20 = ((uint8_t)((*t69)));
  t70 = gg_view((((*(v16))).f15).data, 4);
  t71 = ((int64_t)(((int64_t)UINT64_C(2))));
  t72 = ((int64_t)(v18));
  t73 = ((int64_t)((uint64_t)(t71) * (uint64_t)(t72)));
  t74 = ((gg_type_7 *)t70.data) + gg_index(t73, t70.len);
  t75 = gg_view(((*t74)).data, 32);
  t76 = v19;
  t77 = ((uint8_t *)t75.data) + gg_index(t76, t75.len);
  v21 = ((uint8_t)((*t77)));
  t78 = gg_view((((*(v16))).f15).data, 4);
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((int64_t)(v18));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)UINT64_C(1))));
  t83 = ((int64_t)((uint64_t)(t81) + (uint64_t)(t82)));
  t84 = ((gg_type_7 *)t78.data) + gg_index(t83, t78.len);
  t85 = gg_view(((*t84)).data, 32);
  t86 = v19;
  t87 = ((uint8_t *)t85.data) + gg_index(t86, t85.len);
  v22 = ((uint8_t)((*t87)));
  t88 = gg_view((((*(v7))).f24).data, 2048);
  t89 = ((int64_t)(v15));
  t90 = ((int64_t)(((int64_t)UINT64_C(4))));
  t91 = ((int64_t)(v18));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
  t93 = ((int64_t)((uint64_t)(t89) + (uint64_t)(t92)));
  t94 = ((gg_type_7 *)t88.data) + gg_index(t93, t88.len);
  t95 = gg_view(((*t94)).data, 32);
  t96 = v19;
  t97 = ((uint8_t *)t95.data) + gg_index(t96, t95.len);
  t98 = ((uint8_t)(v21));
  t99 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t100 = ((uint8_t)(((t98) & (t99))));
  t101 = ((uint8_t)(v20));
  t102 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t103 = ((uint8_t)(((t101) & (t102))));
  t104 = ((int64_t)(((int64_t)UINT64_C(4))));
  t105 = ((uint8_t)(((uint8_t)((uint64_t)(t103) << ((uint64_t)(t104) & 7)))));
  (*t97) = ((uint8_t)(((t100) | (t105))));
  t106 = gg_view((((*(v7))).f24).data, 2048);
  t107 = ((int64_t)(v15));
  t108 = ((int64_t)(((int64_t)UINT64_C(4))));
  t109 = ((int64_t)(v18));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t107) + (uint64_t)(t110)))));
  t112 = ((int64_t)(((int64_t)UINT64_C(1))));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((gg_type_7 *)t106.data) + gg_index(t113, t106.len);
  t115 = gg_view(((*t114)).data, 32);
  t116 = v19;
  t117 = ((uint8_t *)t115.data) + gg_index(t116, t115.len);
  t118 = ((uint8_t)(v22));
  t119 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t120 = ((uint8_t)(((t118) & (t119))));
  t121 = ((uint8_t)(v20));
  t122 = ((uint8_t)(((int64_t)UINT64_C(12))));
  t123 = ((uint8_t)(((t121) & (t122))));
  t124 = ((int64_t)(((int64_t)UINT64_C(2))));
  t125 = ((uint8_t)(((uint8_t)((uint64_t)(t123) << ((uint64_t)(t124) & 7)))));
  (*t117) = ((uint8_t)(((t120) | (t125))));
  t126 = gg_view((((*(v7))).f24).data, 2048);
  t127 = ((int64_t)(v15));
  t128 = ((int64_t)(((int64_t)UINT64_C(4))));
  t129 = ((int64_t)(v18));
  t130 = ((int64_t)(((int64_t)((uint64_t)(t128) * (uint64_t)(t129)))));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t127) + (uint64_t)(t130)))));
  t132 = ((int64_t)(((int64_t)UINT64_C(2))));
  t133 = ((int64_t)((uint64_t)(t131) + (uint64_t)(t132)));
  t134 = ((gg_type_7 *)t126.data) + gg_index(t133, t126.len);
  t135 = gg_view(((*t134)).data, 32);
  t136 = v19;
  t137 = ((uint8_t *)t135.data) + gg_index(t136, t135.len);
  t138 = ((uint8_t)(v21));
  t139 = ((int64_t)(((int64_t)UINT64_C(4))));
  t140 = ((uint8_t)(((t138) >> ((uint64_t)(t139) & 7))));
  t141 = ((uint8_t)(v20));
  t142 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t143 = ((uint8_t)(((t141) & (t142))));
  (*t137) = ((uint8_t)(((t140) | (t143))));
  t144 = gg_view((((*(v7))).f24).data, 2048);
  t145 = ((int64_t)(v15));
  t146 = ((int64_t)(((int64_t)UINT64_C(4))));
  t147 = ((int64_t)(v18));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t146) * (uint64_t)(t147)))));
  t149 = ((int64_t)(((int64_t)((uint64_t)(t145) + (uint64_t)(t148)))));
  t150 = ((int64_t)(((int64_t)UINT64_C(3))));
  t151 = ((int64_t)((uint64_t)(t149) + (uint64_t)(t150)));
  t152 = ((gg_type_7 *)t144.data) + gg_index(t151, t144.len);
  t153 = gg_view(((*t152)).data, 32);
  t154 = v19;
  t155 = ((uint8_t *)t153.data) + gg_index(t154, t153.len);
  t156 = ((uint8_t)(v22));
  t157 = ((int64_t)(((int64_t)UINT64_C(4))));
  t158 = ((uint8_t)(((t156) >> ((uint64_t)(t157) & 7))));
  t159 = ((uint8_t)(v20));
  t160 = ((uint8_t)(((int64_t)UINT64_C(192))));
  t161 = ((uint8_t)(((t159) & (t160))));
  t162 = ((int64_t)(((int64_t)UINT64_C(2))));
  t163 = ((uint8_t)(((t161) >> ((uint64_t)(t162) & 7))));
  (*t155) = ((uint8_t)(((t158) | (t163))));
  }
label16:;
  t63++; goto label15;
label17:;
  }
label13:;
  t61++; goto label12;
label14:;
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
  t165 = ((int64_t)UINT64_C(8));
  t164 = 0;
label18:;
  if (t164 >= t165) goto label20;
  v24 = t164;
  {
  t166 = v10;
  t167 = gg_view((((*(v7))).f24).data, 2048);
  t168 = ((int64_t)(v15));
  t169 = ((int64_t)(v24));
  t170 = ((int64_t)((uint64_t)(t168) + (uint64_t)(t169)));
  t171 = ((gg_type_7 *)t167.data) + gg_index(t170, t167.len);
  t172 = (*t171);
  t173 = v8;
  t174 = gg_function_10(t172, t173, base + 0);
  v25 = gg_function_11(t166, t174, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t175 = ((int64_t)UINT64_C(0));
  t176 = ((int32_t *)(v25).data) + gg_index(t175, ((int64_t)UINT64_C(8)));
  t177 = ((int32_t)((*t176)));
  /* FG_DIRECT_SLICE_INDEX */
  t178 = ((int64_t)UINT64_C(1));
  t179 = ((int32_t *)(v25).data) + gg_index(t178, ((int64_t)UINT64_C(8)));
  t180 = ((int32_t)((*t179)));
  t181 = ((int32_t)(((int32_t)((uint64_t)(t177) + (uint64_t)(t180)))));
  /* FG_DIRECT_SLICE_INDEX */
  t182 = ((int64_t)UINT64_C(2));
  t183 = ((int32_t *)(v25).data) + gg_index(t182, ((int64_t)UINT64_C(8)));
  t184 = ((int32_t)((*t183)));
  /* FG_DIRECT_SLICE_INDEX */
  t185 = ((int64_t)UINT64_C(3));
  t186 = ((int32_t *)(v25).data) + gg_index(t185, ((int64_t)UINT64_C(8)));
  t187 = ((int32_t)((*t186)));
  t188 = ((int32_t)(((int32_t)((uint64_t)(t184) + (uint64_t)(t187)))));
  v26 = ((int32_t)(((int32_t)((uint64_t)(t181) + (uint64_t)(t188)))));
  /* FG_DIRECT_SLICE_INDEX */
  t189 = ((int64_t)UINT64_C(4));
  t190 = ((int32_t *)(v25).data) + gg_index(t189, ((int64_t)UINT64_C(8)));
  t191 = ((int32_t)((*t190)));
  /* FG_DIRECT_SLICE_INDEX */
  t192 = ((int64_t)UINT64_C(5));
  t193 = ((int32_t *)(v25).data) + gg_index(t192, ((int64_t)UINT64_C(8)));
  t194 = ((int32_t)((*t193)));
  t195 = ((int32_t)(((int32_t)((uint64_t)(t191) + (uint64_t)(t194)))));
  /* FG_DIRECT_SLICE_INDEX */
  t196 = ((int64_t)UINT64_C(6));
  t197 = ((int32_t *)(v25).data) + gg_index(t196, ((int64_t)UINT64_C(8)));
  t198 = ((int32_t)((*t197)));
  /* FG_DIRECT_SLICE_INDEX */
  t199 = ((int64_t)UINT64_C(7));
  t200 = ((int32_t *)(v25).data) + gg_index(t199, ((int64_t)UINT64_C(8)));
  t201 = ((int32_t)((*t200)));
  t202 = ((int32_t)(((int32_t)((uint64_t)(t198) + (uint64_t)(t201)))));
  v27 = ((int32_t)(((int32_t)((uint64_t)(t195) + (uint64_t)(t202)))));
  t203 = v23;
  t204 = ((int32_t)(((int64_t)UINT64_C(128))));
  t205 = gg_view((((*(v7))).f27).data, 4096);
  t206 = ((int64_t)(v14));
  t207 = ((int64_t)(((int64_t)UINT64_C(2))));
  t208 = ((int64_t)(v24));
  t209 = ((int64_t)(((int64_t)((uint64_t)(t207) * (uint64_t)(t208)))));
  t210 = ((int64_t)((uint64_t)(t206) + (uint64_t)(t209)));
  t211 = ((int32_t *)t205.data) + gg_index(t210, t205.len);
  t212 = ((int32_t)((*t211)));
  t213 = ((int32_t)(((int32_t)((uint64_t)(t204) * (uint64_t)(t212)))));
  t214 = ((int32_t)(v26));
  t215 = ((int32_t)(((int64_t)UINT64_C(512))));
  t216 = ((int32_t)(((int32_t)((uint64_t)(t214) - (uint64_t)(t215)))));
  t217 = ((int32_t)(((int32_t)((uint64_t)(t213) * (uint64_t)(t216)))));
  t218 = ((int32_t)(((int64_t)UINT64_C(128))));
  t219 = gg_view((((*(v7))).f27).data, 4096);
  t220 = ((int64_t)(v14));
  t221 = ((int64_t)(((int64_t)UINT64_C(2))));
  t222 = ((int64_t)(v24));
  t223 = ((int64_t)(((int64_t)((uint64_t)(t221) * (uint64_t)(t222)))));
  t224 = ((int64_t)(((int64_t)((uint64_t)(t220) + (uint64_t)(t223)))));
  t225 = ((int64_t)(((int64_t)UINT64_C(1))));
  t226 = ((int64_t)((uint64_t)(t224) + (uint64_t)(t225)));
  t227 = ((int32_t *)t219.data) + gg_index(t226, t219.len);
  t228 = ((int32_t)((*t227)));
  t229 = ((int32_t)(((int32_t)((uint64_t)(t218) * (uint64_t)(t228)))));
  t230 = ((int32_t)(v27));
  t231 = ((int32_t)(((int64_t)UINT64_C(512))));
  t232 = ((int32_t)(((int32_t)((uint64_t)(t230) - (uint64_t)(t231)))));
  t233 = ((int32_t)(((int32_t)((uint64_t)(t229) * (uint64_t)(t232)))));
  t234 = ((int32_t)(((int32_t)((uint64_t)(t217) + (uint64_t)(t233)))));
  v23 = ((int32_t)((uint64_t)(t203) + (uint64_t)(t234)));
  }
label19:;
  t164++; goto label18;
label20:;
  /* FG_DIRECT_SLICE_INDEX */
  t235 = v14;
  t236 = ((int32_t *)(((*(v7))).f28).data) + gg_index(t235, ((int64_t)UINT64_C(4096)));
  (*t236) = ((int32_t)(v23));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t237 = v13;
  t238 = ((float *)(((*(v7))).f25).data) + gg_index(t237, ((int64_t)UINT64_C(256)));
  (*t238) = ((float)(gg_double_bits(UINT64_C(0))));
  t240 = ((int64_t)UINT64_C(16));
  t239 = 0;
label21:;
  if (t239 >= t240) goto label23;
  v28 = t239;
  {
  t241 = gg_view((((*(v7))).f27).data, 4096);
  t242 = ((int64_t)(v14));
  t243 = ((int64_t)(v28));
  t244 = ((int64_t)((uint64_t)(t242) + (uint64_t)(t243)));
  t245 = ((int32_t *)t241.data) + gg_index(t244, t241.len);
  (*t245) = ((int32_t)(((int64_t)UINT64_C(0))));
  t246 = gg_view((((*(v7))).f28).data, 4096);
  t247 = ((int64_t)(v14));
  t248 = ((int64_t)(v28));
  t249 = ((int64_t)((uint64_t)(t247) + (uint64_t)(t248)));
  t250 = ((int32_t *)t246.data) + gg_index(t249, t246.len);
  (*t250) = ((int32_t)(((int64_t)UINT64_C(0))));
  }
label22:;
  t239++; goto label21;
label23:;
  t252 = ((int64_t)UINT64_C(8));
  t251 = 0;
label24:;
  if (t251 >= t252) goto label26;
  v29 = t251;
  {
  t253 = gg_view((((*(v7))).f24).data, 2048);
  t254 = ((int64_t)(v15));
  t255 = ((int64_t)(v29));
  t256 = ((int64_t)((uint64_t)(t254) + (uint64_t)(t255)));
  t257 = ((gg_type_7 *)t253.data) + gg_index(t256, t253.len);
  memset(&t258, 0, sizeof(t258));
  t259 = ((uint8_t)(t258));
  for (t260=0; t260<32; t260++) ((*t257)).data[t260] = t259;
  }
label25:;
  t251++; goto label24;
label26:;
  }
  }
  }
label7:;
  t10++; goto label6;
label8:;
  }
label4:;
  t8++; goto label3;
label5:;
  }
gg_done:;
}
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((int64_t)(v0));
  }
  } else {
  {
  t0 = ((int64_t)(v1));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t26;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(31))));
  t2 = ((int64_t)(((int64_t)UINT64_C(1024))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)UINT64_C(16))));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  v2 = ((int64_t)(((int64_t)gg_div(t3, t6))));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)UINT64_C(256))));
  t9 = ((int64_t)(((int64_t)gg_div(t7, t8))));
  t10 = ((int64_t)(((int64_t)UINT64_C(256))));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(4096))));
  t13 = ((t11) > (t12));
  if (t13) {
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(4096))));
  }
  }
label1:;
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)UINT64_C(256))));
  t16 = ((t14) >= (t15));
  t17 = t16;
  if (t17) {
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)gg_mod(t18, t19))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t20) != (t21));
  }
  if (!(t17)) goto label2;
  {
  t22 = v2;
  t23 = ((int64_t)(((int64_t)UINT64_C(256))));
  v2 = ((int64_t)((uint64_t)(t22) - (uint64_t)(t23)));
  }
  goto label1;
label2:;
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(((int64_t)UINT64_C(256))));
  t26 = ((t24) < (t25));
  if (t26) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t0 = ((int64_t)(v2));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_1(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_type_42 p5, gg_slice p6, int64_t base) {
  uint8_t t0;
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
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
  uint8_t t43;
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
  gg_slice t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  gg_type_34 * t92;
  gg_slice t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  float * t97;
  int64_t t98;
  int64_t t99;
  uint8_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  gg_slice t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  gg_type_42 t117;
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  uint8_t t125;
  gg_slice t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  gg_type_42 t132;
  gg_slice t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  gg_type_42 t139;
  int64_t t140;
  int64_t t141;
  uint8_t t142;
  gg_type_42 t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  gg_type_42 t151;
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
  int64_t t162;
  int64_t t163;
  int64_t t164;
  uint8_t t165;
  gg_slice t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  gg_type_42 t172;
  gg_slice t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  gg_type_42 t179;
  gg_slice t180;
  gg_slice t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  gg_type_42 t188;
  uint8_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  uint8_t t194;
  gg_type_42 t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  gg_type_42 t201;
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
  gg_slice t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  gg_type_22 * t217;
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
  gg_slice t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  float * t236;
  gg_slice t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  gg_type_34 * t241;
  gg_slice t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  float * t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  gg_type_37 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_type_42 v5;
  gg_slice v6;
  uint8_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
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
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  gg_type_22 v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(((*(v0))).f6));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)((v4).len));
  t6 = ((int64_t)(((int64_t)UINT64_C(8))));
  t7 = ((t5) < (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f7));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) == (t10));
  }
  t4 = t8;
  }
  t11 = t4;
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(((*(v0))).f0));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) > (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(((*(v0))).f0));
  t17 = ((int64_t)(((int64_t)UINT64_C(256))));
  t18 = ((int64_t)(((int64_t)gg_mod(t16, t17))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t18) == (t19));
  }
  t20 = t15;
  t21 = t20;
  if (t21) {
  t22 = ((int64_t)(((*(v0))).f1));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t22) > (t23));
  }
  t24 = t21;
  t25 = t24;
  if (t25) {
  t26 = ((int64_t)(((*(v0))).f5));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t26) > (t27));
  }
  t28 = t25;
  t29 = t28;
  if (t29) {
  t30 = ((int64_t)(((*(v0))).f4));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t30) >= (t31));
  }
  t32 = t29;
  t33 = t32;
  if (t33) {
  t34 = ((int64_t)(((*(v0))).f4));
  t35 = ((int64_t)(((*(v0))).f5));
  t33 = ((t34) < (t35));
  }
  t36 = t33;
  t37 = t36;
  if (t37) {
  t38 = ((int64_t)(((*(v0))).f3));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t38) > (t39));
  }
  if (!(t37)) gg_fail("assertion failed");
  t40 = ((int64_t)(((*(v0))).f10));
  t41 = ((int64_t)(((int64_t)UINT64_C(14))));
  v7 = ((t40) == (t41));
  t42 = v7;
  t43 = t42;
  if (!t43) {
  t44 = ((int64_t)(((*(v0))).f10));
  t45 = ((int64_t)(((int64_t)UINT64_C(12))));
  t43 = ((t44) == (t45));
  }
  if (!(t43)) gg_fail("assertion failed");
  t46 = ((int64_t)(((*(v0))).f0));
  t47 = ((int64_t)(((int64_t)UINT64_C(256))));
  v8 = ((int64_t)(((int64_t)gg_div(t46, t47))));
  t48 = ((int64_t)(((*(v0))).f1));
  t49 = ((int64_t)(((int64_t)UINT64_C(63))));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  t51 = ((int64_t)(((int64_t)UINT64_C(64))));
  v9 = ((int64_t)(((int64_t)gg_div(t50, t51))));
  t52 = ((int64_t)(v9));
  t53 = ((int64_t)(((*(v0))).f4));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  t55 = ((int64_t)(((*(v0))).f5));
  v10 = ((int64_t)(((int64_t)gg_div(t54, t55))));
  t56 = ((int64_t)(v9));
  t57 = ((int64_t)(((*(v0))).f4));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t59)))));
  t61 = ((int64_t)(((*(v0))).f5));
  v11 = ((int64_t)(((int64_t)gg_div(t60, t61))));
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t62 = ((int64_t)(v12));
  t63 = ((int64_t)((v4).len));
  if (!(((t62) < (t63)))) goto label2;
  {
  t64 = ((int64_t)(((int64_t)UINT64_C(256))));
  t65 = ((int64_t)((v4).len));
  t66 = ((int64_t)(v12));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) - (uint64_t)(t66)))));
  v13 = gg_function_2(t64, t67, base + 0);
  v14 = ((int64_t)(v10));
label4:;
  t68 = ((int64_t)(v14));
  t69 = ((int64_t)(v11));
  if (!(((t68) < (t69)))) goto label5;
  {
  t70 = ((int64_t)(v14));
  t71 = ((int64_t)(((int64_t)UINT64_C(64))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))));
  t72 = ((int64_t)(v15));
  t73 = ((int64_t)(((int64_t)UINT64_C(64))));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  t75 = ((int64_t)(((*(v0))).f1));
  v16 = gg_function_2(t74, t75, base + 0);
  t76 = ((int64_t)(v16));
  t77 = ((int64_t)(v15));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t76) - (uint64_t)(t77)))));
  t79 = v17;
  t78 = 0;
label6:;
  if (t78 >= t79) goto label8;
  v18 = t78;
  {
  t81 = ((int64_t)(v13));
  t82 = gg_function_3(t81, base + 0);
  t83 = t82;
  t80 = 0;
label9:;
  if (t80 >= t83) goto label11;
  v19 = t80;
  {
  t84 = ((int64_t)(v18));
  t85 = ((int64_t)(((int64_t)UINT64_C(256))));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))));
  t87 = ((int64_t)(v19));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))));
  t88 = gg_view((((*(v5))).f34).data, 8192);
  t89 = ((int64_t)(v20));
  t90 = ((int64_t)(((int64_t)UINT64_C(8))));
  t91 = ((int64_t)gg_div(t89, t90));
  t92 = ((gg_type_34 *)t88.data) + gg_index(t91, t88.len);
  t93 = gg_view(((*t92)).data, 8);
  t94 = ((int64_t)(v20));
  t95 = ((int64_t)(((int64_t)UINT64_C(8))));
  t96 = ((int64_t)gg_mod(t94, t95));
  t97 = ((float *)t93.data) + gg_index(t96, t93.len);
  (*t97) = ((float)(gg_double_bits(UINT64_C(0))));
  }
label10:;
  t80++; goto label9;
label11:;
  }
label7:;
  t78++; goto label6;
label8:;
  t98 = ((int64_t)(v13));
  t99 = ((int64_t)(((int64_t)UINT64_C(16))));
  t100 = ((t98) <= (t99));
  if (t100) {
  {
  t101 = ((int64_t)(v13));
  t102 = ((int64_t)(((*(v0))).f0));
  v21 = gg_function_4(t101, t102, base + 0);
  t103 = ((int64_t)(v21));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t103) > (t104)))) gg_fail("assertion failed");
  t105 = ((int64_t)(v21));
  t106 = ((int64_t)(((int64_t)UINT64_C(256))));
  v22 = ((int64_t)(((int64_t)gg_div(t105, t106))));
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t107 = ((int64_t)(v23));
  t108 = ((int64_t)(v8));
  if (!(((t107) < (t108)))) goto label14;
  {
  t109 = v3;
  t110 = v4;
  t111 = ((int64_t)(v12));
  t112 = ((int64_t)(v13));
  t113 = ((int64_t)(((*(v0))).f3));
  t114 = ((int64_t)(v8));
  t115 = ((int64_t)(v23));
  t116 = ((int64_t)(v22));
  t117 = v5;
  t118 = v7;
  gg_function_5(t109, t110, t111, t112, t113, t114, t115, t116, t117, t118, base + 0);
  v24 = ((int64_t)(v15));
label16:;
  t119 = ((int64_t)(v24));
  t120 = ((int64_t)(v16));
  if (!(((t119) < (t120)))) goto label17;
  {
  t121 = ((int64_t)(((int64_t)UINT64_C(16))));
  t122 = ((int64_t)(v16));
  t123 = ((int64_t)(v24));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t122) - (uint64_t)(t123)))));
  v25 = gg_function_2(t121, t124, base + 0);
  t125 = v7;
  if (t125) {
  {
  t126 = v2;
  t127 = ((int64_t)(v24));
  t128 = ((int64_t)(((*(v0))).f2));
  t129 = ((int64_t)(v23));
  t130 = ((int64_t)(v25));
  t131 = ((int64_t)(v22));
  t132 = v5;
  gg_function_7(t126, t127, t128, t129, t130, t131, t132, base + 0);
  }
  } else {
  {
  t133 = v1;
  t134 = ((int64_t)(v24));
  t135 = ((int64_t)(((*(v0))).f2));
  t136 = ((int64_t)(v23));
  t137 = ((int64_t)(v25));
  t138 = ((int64_t)(v22));
  t139 = v5;
  gg_function_12(t133, t134, t135, t136, t137, t138, t139, base + 0);
  }
  }
  t141 = v22;
  t140 = 0;
label18:;
  if (t140 >= t141) goto label20;
  v26 = t140;
  {
  t142 = v7;
  if (t142) {
  {
  t143 = v5;
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t145 = ((int64_t)(((int64_t)UINT64_C(0))));
  t146 = ((int64_t)(v24));
  t147 = ((int64_t)(v15));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t146) - (uint64_t)(t147)))));
  t149 = ((int64_t)(v22));
  t150 = ((int64_t)(v26));
  gg_function_15(t143, t144, t145, t148, t149, t150, base + 0);
  }
  } else {
  {
  t151 = v5;
  t152 = ((int64_t)(((int64_t)UINT64_C(0))));
  t153 = ((int64_t)(((int64_t)UINT64_C(0))));
  t154 = ((int64_t)(v24));
  t155 = ((int64_t)(v15));
  t156 = ((int64_t)(((int64_t)((uint64_t)(t154) - (uint64_t)(t155)))));
  t157 = ((int64_t)(v22));
  t158 = ((int64_t)(v26));
  gg_function_28(t151, t152, t153, t156, t157, t158, base + 0);
  }
  }
  }
label19:;
  t140++; goto label18;
label20:;
  t159 = v24;
  t160 = ((int64_t)(((int64_t)UINT64_C(16))));
  v24 = ((int64_t)((uint64_t)(t159) + (uint64_t)(t160)));
  }
  goto label16;
label17:;
  t161 = v23;
  t162 = ((int64_t)(v22));
  v23 = ((int64_t)((uint64_t)(t161) + (uint64_t)(t162)));
  }
  goto label13;
label14:;
  }
  } else {
  {
  t164 = v8;
  t163 = 0;
label21:;
  if (t163 >= t164) goto label23;
  v27 = t163;
  {
  t165 = v7;
  if (t165) {
  {
  t166 = v2;
  t167 = ((int64_t)(v15));
  t168 = ((int64_t)(((*(v0))).f2));
  t169 = ((int64_t)(v27));
  t170 = ((int64_t)(v17));
  t171 = ((int64_t)(((int64_t)UINT64_C(1))));
  t172 = v5;
  gg_function_7(t166, t167, t168, t169, t170, t171, t172, base + 0);
  }
  } else {
  {
  t173 = v1;
  t174 = ((int64_t)(v15));
  t175 = ((int64_t)(((*(v0))).f2));
  t176 = ((int64_t)(v27));
  t177 = ((int64_t)(v17));
  t178 = ((int64_t)(((int64_t)UINT64_C(1))));
  t179 = v5;
  gg_function_12(t173, t174, t175, t176, t177, t178, t179, base + 0);
  }
  }
  t180 = v3;
  t181 = v4;
  t182 = ((int64_t)(v12));
  t183 = ((int64_t)(v13));
  t184 = ((int64_t)(((*(v0))).f3));
  t185 = ((int64_t)(v8));
  t186 = ((int64_t)(v27));
  t187 = ((int64_t)(((int64_t)UINT64_C(1))));
  t188 = v5;
  t189 = v7;
  gg_function_5(t180, t181, t182, t183, t184, t185, t186, t187, t188, t189, base + 0);
  v28 = ((int64_t)(((int64_t)UINT64_C(0))));
label25:;
  t190 = ((int64_t)(v28));
  t191 = ((int64_t)(v13));
  if (!(((t190) < (t191)))) goto label26;
  {
  v29 = ((int64_t)(((int64_t)UINT64_C(0))));
label28:;
  t192 = ((int64_t)(v29));
  t193 = ((int64_t)(v17));
  if (!(((t192) < (t193)))) goto label29;
  {
  t194 = v7;
  if (t194) {
  {
  t195 = v5;
  t196 = ((int64_t)(v29));
  t197 = ((int64_t)(v28));
  t198 = ((int64_t)(v29));
  t199 = ((int64_t)(((int64_t)UINT64_C(1))));
  t200 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_15(t195, t196, t197, t198, t199, t200, base + 0);
  }
  } else {
  {
  t201 = v5;
  t202 = ((int64_t)(v29));
  t203 = ((int64_t)(v28));
  t204 = ((int64_t)(v29));
  t205 = ((int64_t)(((int64_t)UINT64_C(1))));
  t206 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_28(t201, t202, t203, t204, t205, t206, base + 0);
  }
  }
  t207 = v29;
  t208 = ((int64_t)(((int64_t)UINT64_C(16))));
  v29 = ((int64_t)((uint64_t)(t207) + (uint64_t)(t208)));
  }
  goto label28;
label29:;
  t209 = v28;
  t210 = ((int64_t)(((int64_t)UINT64_C(16))));
  v28 = ((int64_t)((uint64_t)(t209) + (uint64_t)(t210)));
  }
  goto label25;
label26:;
  }
label22:;
  t163++; goto label21;
label23:;
  }
  }
  t212 = v13;
  t211 = 0;
label30:;
  if (t211 >= t212) goto label32;
  v30 = t211;
  {
  t213 = v4;
  t214 = ((int64_t)(v12));
  t215 = ((int64_t)(v30));
  t216 = ((int64_t)((uint64_t)(t214) + (uint64_t)(t215)));
  t217 = ((gg_type_22 *)t213.data) + gg_index(t216, t213.len);
  v31 = (*t217);
  t218 = ((int64_t)(((int64_t)((v31).f22))));
  t219 = ((int64_t)(((*(v0))).f8));
  t220 = ((int64_t)(((int64_t)((uint64_t)(t218) * (uint64_t)(t219)))));
  t221 = ((int64_t)(((int64_t)((v31).f23))));
  t222 = ((int64_t)(((*(v0))).f9));
  t223 = ((int64_t)(((int64_t)((uint64_t)(t221) * (uint64_t)(t222)))));
  t224 = ((int64_t)(((int64_t)((uint64_t)(t220) + (uint64_t)(t223)))));
  t225 = ((int64_t)(v15));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t224) + (uint64_t)(t225)))));
  t227 = v17;
  t226 = 0;
label33:;
  if (t226 >= t227) goto label35;
  v33 = t226;
  {
  t228 = ((int64_t)(v33));
  t229 = ((int64_t)(((int64_t)UINT64_C(256))));
  t230 = ((int64_t)(((int64_t)((uint64_t)(t228) * (uint64_t)(t229)))));
  t231 = ((int64_t)(v30));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t230) + (uint64_t)(t231)))));
  t232 = v6;
  t233 = ((int64_t)(v32));
  t234 = ((int64_t)(v33));
  t235 = ((int64_t)((uint64_t)(t233) + (uint64_t)(t234)));
  t236 = ((float *)t232.data) + gg_index(t235, t232.len);
  t237 = gg_view((((*(v5))).f34).data, 8192);
  t238 = ((int64_t)(v34));
  t239 = ((int64_t)(((int64_t)UINT64_C(8))));
  t240 = ((int64_t)gg_div(t238, t239));
  t241 = ((gg_type_34 *)t237.data) + gg_index(t240, t237.len);
  t242 = gg_view(((*t241)).data, 8);
  t243 = ((int64_t)(v34));
  t244 = ((int64_t)(((int64_t)UINT64_C(8))));
  t245 = ((int64_t)gg_mod(t243, t244));
  t246 = ((float *)t242.data) + gg_index(t245, t242.len);
  (*t236) = ((float)((*t246)));
  }
label34:;
  t226++; goto label33;
label35:;
  }
label31:;
  t211++; goto label30;
label32:;
  t247 = v14;
  t248 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t247) + (uint64_t)(t248)));
  }
  goto label4;
label5:;
  t249 = v12;
  t250 = ((int64_t)(((int64_t)UINT64_C(256))));
  v12 = ((int64_t)((uint64_t)(t249) + (uint64_t)(t250)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
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
  gg_type_3 t11;
  uint16_t t12;
  uint16_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  gg_type_7 t17;
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  gg_type_7 t21;
  int64_t t22;
  gg_type_3 t23;
  int64_t t24;
  gg_type_10 t25;
  gg_type_7 t26;
  uint8_t t27;
  uint8_t t28;
  int64_t t29;
  gg_type_7 t30;
  int64_t t31;
  gg_type_7 t32;
  uint8_t t33;
  uint8_t t34;
  int64_t t35;
  gg_type_7 t36;
  int64_t t37;
  int8_t t38;
  int8_t t39;
  int64_t t40;
  uint16_t t41;
  gg_type_10 t42;
  int64_t t43;
  gg_type_15 t44;
  float t45;
  gg_type_17 t46;
  int8_t t47;
  int8_t t48;
  int64_t t49;
  gg_type_17 t50;
  int64_t t51;
  int16_t t52;
  int16_t t53;
  int64_t t54;
  gg_type_15 t55;
  int64_t t56;
  gg_type_22 t57;
  int32_t t58;
  int32_t t59;
  gg_type_22 t60;
  int64_t t61;
  gg_type_7 t62;
  uint8_t t63;
  uint8_t t64;
  int64_t t65;
  gg_type_7 t66;
  int64_t t67;
  float t68;
  float t69;
  int64_t t70;
  float t71;
  float t72;
  int64_t t73;
  int32_t t74;
  int32_t t75;
  int64_t t76;
  int32_t t77;
  int32_t t78;
  int64_t t79;
  gg_type_17 t80;
  int8_t t81;
  int8_t t82;
  int64_t t83;
  gg_type_17 t84;
  int64_t t85;
  gg_type_32 t86;
  int32_t t87;
  int32_t t88;
  int64_t t89;
  gg_type_32 t90;
  int64_t t91;
  float t92;
  float t93;
  int64_t t94;
  gg_type_34 t95;
  float t96;
  float t97;
  int64_t t98;
  gg_type_34 t99;
  int64_t t100;
  float t101;
  float t102;
  int64_t t103;
  gg_header t104;
  gg_type_37 t105;
  gg_slice t106;
  gg_slice t107;
  gg_slice t108;
  gg_slice t109;
  gg_type_42 t110;
  gg_slice t111;
  uint8_t t112;
  int64_t t113;
  gg_type_1 v0;
  gg_type_9 v1;
  gg_type_14 v2;
  gg_type_21 v3;
  gg_type_24 v4;
  gg_type_25 v5;
  gg_type_36 v6;
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
  memset(&(t11), 0, sizeof(t11));
  memset(&t12, 0, sizeof(t12));
  (t11).f11 = ((uint16_t)(t12));
  memset(&t13, 0, sizeof(t13));
  (t11).f12 = ((uint16_t)(t13));
  memset(&t14, 0, sizeof(t14));
  t15 = ((uint8_t)(t14));
  for (t16=0; t16<12; t16++) ((t11).f13).data[t16] = t15;
  memset(&t18, 0, sizeof(t18));
  t19 = ((uint8_t)(t18));
  for (t20=0; t20<32; t20++) (t17).data[t20] = t19;
  t21 = t17;
  for (t22=0; t22<4; t22++) ((t11).f14).data[t22] = t21;
  t23 = t11;
  for (t24=0; t24<1; t24++) (v1).data[t24] = t23;
  memset(&(t25), 0, sizeof(t25));
  memset(&t27, 0, sizeof(t27));
  t28 = ((uint8_t)(t27));
  for (t29=0; t29<32; t29++) (t26).data[t29] = t28;
  t30 = t26;
  for (t31=0; t31<4; t31++) ((t25).f15).data[t31] = t30;
  memset(&t33, 0, sizeof(t33));
  t34 = ((uint8_t)(t33));
  for (t35=0; t35<32; t35++) (t32).data[t35] = t34;
  t36 = t32;
  for (t37=0; t37<2; t37++) ((t25).f16).data[t37] = t36;
  memset(&t38, 0, sizeof(t38));
  t39 = ((int8_t)(t38));
  for (t40=0; t40<16; t40++) ((t25).f17).data[t40] = t39;
  memset(&t41, 0, sizeof(t41));
  (t25).f18 = ((uint16_t)(t41));
  t42 = t25;
  for (t43=0; t43<1; t43++) (v2).data[t43] = t42;
  memset(&(t44), 0, sizeof(t44));
  memset(&t45, 0, sizeof(t45));
  (t44).f19 = ((float)(t45));
  memset(&t47, 0, sizeof(t47));
  t48 = ((int8_t)(t47));
  for (t49=0; t49<32; t49++) (t46).data[t49] = t48;
  t50 = t46;
  for (t51=0; t51<8; t51++) ((t44).f20).data[t51] = t50;
  memset(&t52, 0, sizeof(t52));
  t53 = ((int16_t)(t52));
  for (t54=0; t54<16; t54++) ((t44).f21).data[t54] = t53;
  t55 = t44;
  for (t56=0; t56<1; t56++) (v3).data[t56] = t55;
  memset(&(t57), 0, sizeof(t57));
  memset(&t58, 0, sizeof(t58));
  (t57).f22 = ((int32_t)(t58));
  memset(&t59, 0, sizeof(t59));
  (t57).f23 = ((int32_t)(t59));
  t60 = t57;
  for (t61=0; t61<1; t61++) (v4).data[t61] = t60;
  memset(&(v5), 0, sizeof(v5));
  memset(&((v5).f29), 0, sizeof((v5).f29));
  memset(&t63, 0, sizeof(t63));
  t64 = ((uint8_t)(t63));
  for (t65=0; t65<32; t65++) (t62).data[t65] = t64;
  t66 = t62;
  for (t67=0; t67<2048; t67++) (((v5).f29).f24).data[t67] = t66;
  memset(&t68, 0, sizeof(t68));
  t69 = ((float)(t68));
  for (t70=0; t70<256; t70++) (((v5).f29).f25).data[t70] = t69;
  memset(&t71, 0, sizeof(t71));
  t72 = ((float)(t71));
  for (t73=0; t73<256; t73++) (((v5).f29).f26).data[t73] = t72;
  memset(&t74, 0, sizeof(t74));
  t75 = ((int32_t)(t74));
  for (t76=0; t76<4096; t76++) (((v5).f29).f27).data[t76] = t75;
  memset(&t77, 0, sizeof(t77));
  t78 = ((int32_t)(t77));
  for (t79=0; t79<4096; t79++) (((v5).f29).f28).data[t79] = t78;
  memset(&((v5).f33), 0, sizeof((v5).f33));
  memset(&t81, 0, sizeof(t81));
  t82 = ((int8_t)(t81));
  for (t83=0; t83<32; t83++) (t80).data[t83] = t82;
  t84 = t80;
  for (t85=0; t85<2048; t85++) (((v5).f33).f30).data[t85] = t84;
  memset(&t87, 0, sizeof(t87));
  t88 = ((int32_t)(t87));
  for (t89=0; t89<8; t89++) (t86).data[t89] = t88;
  t90 = t86;
  for (t91=0; t91<512; t91++) (((v5).f33).f31).data[t91] = t90;
  memset(&t92, 0, sizeof(t92));
  t93 = ((float)(t92));
  for (t94=0; t94<256; t94++) (((v5).f33).f32).data[t94] = t93;
  memset(&t96, 0, sizeof(t96));
  t97 = ((float)(t96));
  for (t98=0; t98<8; t98++) (t95).data[t98] = t97;
  t99 = t95;
  for (t100=0; t100<8192; t100++) ((v5).f34).data[t100] = t99;
  memset(&t101, 0, sizeof(t101));
  t102 = ((float)(t101));
  for (t103=0; t103<1; t103++) (v6).data[t103] = t102;
  t104.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t104.len = 0;
  t105 = &(v0);
  t106 = gg_view((v1).data, 1);
  t107 = gg_view((v2).data, 1);
  t108 = gg_view((v3).data, 1);
  t109 = gg_view((v4).data, 1);
  t110 = &(v5);
  t111 = gg_view((v6).data, 1);
  t112 = gg_function_1(t105, t106, t107, t108, t109, t110, t111, base + 1);
  gg_bool(gg_ref(&t104, gg_get_stack(base + 0)), t112);
  gg_append(gg_ref(&t104, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t104).base, (t104).len));
  }
gg_done:;
  for (t113 = 0; t113 < 1; t113++) gg_stacks[base + t113].top = 0;
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
