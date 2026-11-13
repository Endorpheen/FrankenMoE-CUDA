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
typedef struct { gg_type_7 data[8]; } gg_type_27;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_27 data[256]; } gg_type_28;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[256]; } gg_type_29;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[16]; } gg_type_30;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_30 data[256]; } gg_type_31;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_26 {
  gg_type_28 f24;
  gg_type_29 f25;
  gg_type_29 f26;
  gg_type_31 f27;
  gg_type_31 f28;
};
#pragma pack(pop)
typedef struct gg_type_32 gg_type_32;
#pragma pack(push, 1)
typedef struct { gg_type_18 data[256]; } gg_type_33;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_34;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_34 data[512]; } gg_type_35;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_32 {
  gg_type_33 f30;
  gg_type_35 f31;
  gg_type_29 f32;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_36;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_36 data[8192]; } gg_type_37;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_25 {
  gg_type_26 f29;
  gg_type_32 f33;
  gg_type_37 f34;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[1]; } gg_type_38;
#pragma pack(pop)
typedef gg_type_1 *gg_type_39;
typedef gg_type_25 *gg_type_44;
typedef gg_type_32 *gg_type_49;
typedef gg_type_15 *gg_type_50;
typedef gg_type_26 *gg_type_51;
typedef gg_type_10 *gg_type_52;
typedef gg_type_3 *gg_type_53;
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_54;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_55;
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
static uint8_t gg_eq_38(gg_type_38 a, gg_type_38 b);
static uint8_t gg_equal_elements_38(gg_slice a, gg_slice b);
static void gg_render_38(gg_reference out, gg_type_38 value);
static uint8_t gg_eq_39(gg_type_39 a, gg_type_39 b);
static uint8_t gg_equal_elements_39(gg_slice a, gg_slice b);
static void gg_render_39(gg_reference out, gg_type_39 value);
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
static uint8_t gg_eq_44(gg_type_44 a, gg_type_44 b);
static uint8_t gg_equal_elements_44(gg_slice a, gg_slice b);
static void gg_render_44(gg_reference out, gg_type_44 value);
static uint8_t gg_eq_45(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_45(gg_slice a, gg_slice b);
static void gg_render_45(gg_reference out, gg_slice value);
static uint8_t gg_eq_46(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_46(gg_slice a, gg_slice b);
static void gg_render_46(gg_reference out, uint8_t value);
static uint8_t gg_eq_48(double a, double b);
static uint8_t gg_equal_elements_48(gg_slice a, gg_slice b);
static void gg_render_48(gg_reference out, double value);
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
static uint8_t gg_eq_55(gg_type_55 a, gg_type_55 b);
static uint8_t gg_equal_elements_55(gg_slice a, gg_slice b);
static void gg_render_55(gg_reference out, gg_type_55 value);
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
if(!gg_eq_32(a.f33,b.f33))return 0;
if(!gg_eq_37(a.f34,b.f34))return 0;
return 1;
}
static void gg_render_25(gg_reference out, gg_type_25 value) {
gg_append(out,"\127\157\162\153\163\160\141\143\145\040\173\040",12,1);
gg_render_26(out, value.f29);
gg_append(out,", ",2,1);
gg_render_32(out, value.f33);
gg_append(out,", ",2,1);
gg_render_37(out, value.f34);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((gg_type_26*)a.data)[i],((gg_type_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b) {
if(!gg_eq_28(a.f24,b.f24))return 0;
if(!gg_eq_29(a.f25,b.f25))return 0;
if(!gg_eq_29(a.f26,b.f26))return 0;
if(!gg_eq_31(a.f27,b.f27))return 0;
if(!gg_eq_31(a.f28,b.f28))return 0;
return 1;
}
static void gg_render_26(gg_reference out, gg_type_26 value) {
gg_append(out,"\127\145\151\147\150\164\124\151\154\145\040\173\040",13,1);
gg_render_28(out, value.f24);
gg_append(out,", ",2,1);
gg_render_29(out, value.f25);
gg_append(out,", ",2,1);
gg_render_29(out, value.f26);
gg_append(out,", ",2,1);
gg_render_31(out, value.f27);
gg_append(out,", ",2,1);
gg_render_31(out, value.f28);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((gg_type_27*)a.data)[i],((gg_type_27*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b) {
return gg_equal_elements_7(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_27(gg_reference out, gg_type_27 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_7*)view.data)[i]).data, 32));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_type_28*)a.data)[i],((gg_type_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
return gg_equal_elements_27(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_27(out, ((gg_type_27*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((gg_type_29*)a.data)[i],((gg_type_29*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b) {
return gg_equal_elements_16(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_29(gg_reference out, gg_type_29 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_30(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_30(((gg_type_30*)a.data)[i],((gg_type_30*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_30(gg_type_30 a, gg_type_30 b) {
return gg_equal_elements_23(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_30(gg_reference out, gg_type_30 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_23(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_31(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_31(((gg_type_31*)a.data)[i],((gg_type_31*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b) {
return gg_equal_elements_30(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_31(gg_reference out, gg_type_31 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_30(out, ((gg_type_30*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_32(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_32(((gg_type_32*)a.data)[i],((gg_type_32*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b) {
if(!gg_eq_33(a.f30,b.f30))return 0;
if(!gg_eq_35(a.f31,b.f31))return 0;
if(!gg_eq_29(a.f32,b.f32))return 0;
return 1;
}
static void gg_render_32(gg_reference out, gg_type_32 value) {
gg_append(out,"\101\143\164\151\166\141\164\151\157\156\124\151\154\145\040\173\040",17,1);
gg_render_33(out, value.f30);
gg_append(out,", ",2,1);
gg_render_35(out, value.f31);
gg_append(out,", ",2,1);
gg_render_29(out, value.f32);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_33(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_33(((gg_type_33*)a.data)[i],((gg_type_33*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_33(gg_type_33 a, gg_type_33 b) {
return gg_equal_elements_18(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_33(gg_reference out, gg_type_33 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_18(out, ((gg_type_18*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_34(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_34(((gg_type_34*)a.data)[i],((gg_type_34*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_34(gg_type_34 a, gg_type_34 b) {
return gg_equal_elements_23(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_34(gg_reference out, gg_type_34 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_23(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_35(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_35(((gg_type_35*)a.data)[i],((gg_type_35*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b) {
return gg_equal_elements_34(gg_view((a).data, 512),gg_view((b).data, 512));
}
static void gg_render_35(gg_reference out, gg_type_35 value) {
int64_t i; gg_slice view=gg_view((value).data, 512);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_34(out, ((gg_type_34*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_36(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_36(((gg_type_36*)a.data)[i],((gg_type_36*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_36(gg_type_36 a, gg_type_36 b) {
return gg_equal_elements_16(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_36(gg_reference out, gg_type_36 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_37(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_37(((gg_type_37*)a.data)[i],((gg_type_37*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_37(gg_type_37 a, gg_type_37 b) {
return gg_equal_elements_36(gg_view((a).data, 8192),gg_view((b).data, 8192));
}
static void gg_render_37(gg_reference out, gg_type_37 value) {
int64_t i; gg_slice view=gg_view((value).data, 8192);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_36(out, ((gg_type_36*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_38(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_38(((gg_type_38*)a.data)[i],((gg_type_38*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_38(gg_type_38 a, gg_type_38 b) {
return gg_equal_elements_16(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_38(gg_reference out, gg_type_38 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_39(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_39(((gg_type_39*)a.data)[i],((gg_type_39*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_39(gg_type_39 a, gg_type_39 b) {
return a == b;
}
static void gg_render_39(gg_reference out, gg_type_39 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_40(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_40(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_40(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_40(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((gg_type_3*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_41(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_41(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_41(gg_slice a, gg_slice b) {
return gg_equal_elements_10(a,b);
}
static void gg_render_41(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((gg_type_10*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_42(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_42(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_42(gg_slice a, gg_slice b) {
return gg_equal_elements_15(a,b);
}
static void gg_render_42(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_15(out, ((gg_type_15*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_43(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_43(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_43(gg_slice a, gg_slice b) {
return gg_equal_elements_22(a,b);
}
static void gg_render_43(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_22(out, ((gg_type_22*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_44(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_44(((gg_type_44*)a.data)[i],((gg_type_44*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_44(gg_type_44 a, gg_type_44 b) {
return a == b;
}
static void gg_render_44(gg_reference out, gg_type_44 value) {
gg_render_25(out,*value);
}
static uint8_t gg_equal_elements_45(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_45(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_45(gg_slice a, gg_slice b) {
return gg_equal_elements_16(a,b);
}
static void gg_render_45(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_46(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_46(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_46(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_46(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_48(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_48(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_48(double a, double b) {
return a == b;
}
static void gg_render_48(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_49(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_49(((gg_type_49*)a.data)[i],((gg_type_49*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_49(gg_type_49 a, gg_type_49 b) {
return a == b;
}
static void gg_render_49(gg_reference out, gg_type_49 value) {
gg_render_32(out,*value);
}
static uint8_t gg_equal_elements_50(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_50(((gg_type_50*)a.data)[i],((gg_type_50*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_50(gg_type_50 a, gg_type_50 b) {
return a == b;
}
static void gg_render_50(gg_reference out, gg_type_50 value) {
gg_render_15(out,*value);
}
static uint8_t gg_equal_elements_51(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_51(((gg_type_51*)a.data)[i],((gg_type_51*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_51(gg_type_51 a, gg_type_51 b) {
return a == b;
}
static void gg_render_51(gg_reference out, gg_type_51 value) {
gg_render_26(out,*value);
}
static uint8_t gg_equal_elements_52(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_52(((gg_type_52*)a.data)[i],((gg_type_52*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_52(gg_type_52 a, gg_type_52 b) {
return a == b;
}
static void gg_render_52(gg_reference out, gg_type_52 value) {
gg_render_10(out,*value);
}
static uint8_t gg_equal_elements_53(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_53(((gg_type_53*)a.data)[i],((gg_type_53*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_53(gg_type_53 a, gg_type_53 b) {
return a == b;
}
static void gg_render_53(gg_reference out, gg_type_53 value) {
gg_render_3(out,*value);
}
static uint8_t gg_equal_elements_54(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_54(((gg_type_54*)a.data)[i],((gg_type_54*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_54(gg_type_54 a, gg_type_54 b) {
return gg_equal_elements_23(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_54(gg_reference out, gg_type_54 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_23(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_55(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_55(((gg_type_55*)a.data)[i],((gg_type_55*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_55(gg_type_55 a, gg_type_55 b) {
return gg_equal_elements_16(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_55(gg_reference out, gg_type_55 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static uint8_t gg_function_1(gg_type_39 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_type_44 p5, gg_slice p6, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_3(int64_t p0, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t base);
static void gg_function_5(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, gg_type_44 p8, uint8_t p9, int64_t base);
static gg_type_17 gg_function_6(gg_type_17 p0, int64_t base);
static void gg_function_7(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_type_44 p6, int64_t base);
static gg_type_20 gg_function_8(int16_t p158, int64_t base);
static gg_type_20 gg_function_10(gg_type_7 p183, gg_type_17 p184, int64_t base);
static gg_type_34 gg_function_11(gg_type_20 p191, gg_type_20 p192, int64_t base);
static void gg_function_12(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_type_44 p6, int64_t base);
static gg_type_7 gg_function_13(gg_type_7 p226, uint8_t p227, int64_t base);
static gg_type_7 gg_function_14(gg_type_7 p233, uint8_t p234, int64_t base);
static void gg_function_15(gg_type_44 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static gg_type_17 gg_function_16(gg_type_7 p0, int64_t base);
static gg_type_20 gg_function_17(int16_t p0, int16_t p1, int64_t base);
static gg_type_7 gg_function_18(gg_type_17 p0, int64_t base);
static gg_type_34 gg_function_19(gg_type_34 p304, gg_type_34 p305, int64_t base);
static int32_t gg_function_20(gg_type_34 p0, int64_t base);
static gg_type_54 gg_function_21(gg_type_34 p344, int64_t base);
static gg_type_34 gg_function_22(int32_t p0, int64_t base);
static gg_type_34 gg_function_23(gg_type_34 p0, gg_type_34 p1, int64_t base);
static gg_type_36 gg_function_24(float p0, gg_type_34 p1, int64_t base);
static void gg_function_27(gg_type_44 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static gg_type_34 gg_function_28(gg_type_34 p448, gg_type_34 p449, int64_t base);
static gg_type_36 gg_function_29(float p461, gg_type_34 p462, gg_type_36 p463, int64_t base);
/* FG_PACKED_MATCH:fma32 */
static gg_type_36 gg_function_29(float p461, gg_type_34 p462, gg_type_36 p463, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__) && defined(__FMA__)
  { gg_type_36 packed_result;
    _mm256_storeu_ps(packed_result.data, _mm256_fmadd_ps(_mm256_set1_ps(p461), _mm256_cvtepi32_ps(_mm256_loadu_si256((const __m256i*)p462.data)), _mm256_loadu_ps(p463.data)));
    return packed_result;
  }
#endif
  gg_type_36 t0;
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
  float v461;
  gg_type_34 v462;
  gg_type_36 v463;
  gg_type_36 v464;
  int64_t v466;
  v461 = p461;
  v462 = p462;
  v463 = p463;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v464).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v466 = t4;
  {
  t6 = gg_view((v464).data, 8);
  t7 = v466;
  t8 = ((float *)t6.data) + gg_index(t7, t6.len);
  t9 = ((float)(v461));
  t10 = gg_view((v462).data, 8);
  t11 = v466;
  t12 = ((int32_t *)t10.data) + gg_index(t11, t10.len);
  t13 = ((float)(((float)((*t12)))));
  t14 = gg_view((v463).data, 8);
  t15 = v466;
  t16 = ((float *)t14.data) + gg_index(t15, t14.len);
  t17 = ((float)((*t16)));
  t18 = fmaf(t9, t13, t17);
  (*t8) = ((float)(t18));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v464;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_36 gg_function_24(float p0, gg_type_34 p1, int64_t base) {
  gg_type_36 t0;
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
  gg_type_34 v1;
  gg_type_36 v2;
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
static gg_type_54 gg_function_21(gg_type_34 p344, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_54 packed_result;
    _mm_storeu_si128((__m128i*)packed_result.data, _mm_hadd_epi32(_mm256_castsi256_si128(_mm256_loadu_si256((const __m256i*)p344.data)), _mm256_extracti128_si256(_mm256_loadu_si256((const __m256i*)p344.data), 1)));
    return packed_result;
  }
#endif
  gg_type_54 t0;
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
  gg_type_34 v344;
  gg_type_54 v345;
  int64_t v347;
  v344 = p344;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<4; t3++) (v345).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(4));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v347 = t4;
  {
  t6 = gg_view((v345).data, 4);
  t7 = v347;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v344).data, 8);
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(v347));
  t12 = ((int64_t)((uint64_t)(t10) * (uint64_t)(t11)));
  t13 = ((int32_t *)t9.data) + gg_index(t12, t9.len);
  t14 = ((int32_t)((*t13)));
  t15 = gg_view((v344).data, 8);
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((int64_t)(v347));
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
  t0 = v345;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_20(gg_type_34 p0, int64_t base) {
  int32_t t0;
  gg_type_34 t1;
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
  gg_type_34 v0;
  gg_type_54 v1;
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
static gg_type_34 gg_function_19(gg_type_34 p304, gg_type_34 p305, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_34 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)p304.data), _mm256_loadu_si256((const __m256i*)p305.data)));
    return packed_result;
  }
#endif
  gg_type_34 t0;
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
  gg_type_34 v304;
  gg_type_34 v305;
  gg_type_34 v306;
  int64_t v308;
  v304 = p304;
  v305 = p305;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v306).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v308 = t4;
  {
  t6 = gg_view((v306).data, 8);
  t7 = v308;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v304).data, 8);
  t10 = v308;
  t11 = ((int32_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int32_t)((*t11)));
  t13 = gg_view((v305).data, 8);
  t14 = v308;
  t15 = ((int32_t *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int32_t)((*t15)));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t12) + (uint64_t)(t16)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v306;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:mul32 */
static gg_type_34 gg_function_28(gg_type_34 p448, gg_type_34 p449, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_34 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_mullo_epi32(_mm256_loadu_si256((const __m256i*)p448.data), _mm256_loadu_si256((const __m256i*)p449.data)));
    return packed_result;
  }
#endif
  gg_type_34 t0;
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
  gg_type_34 v448;
  gg_type_34 v449;
  gg_type_34 v450;
  int64_t v452;
  v448 = p448;
  v449 = p449;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v450).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v452 = t4;
  {
  t6 = gg_view((v450).data, 8);
  t7 = v452;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v448).data, 8);
  t10 = v452;
  t11 = ((int32_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int32_t)((*t11)));
  t13 = gg_view((v449).data, 8);
  t14 = v452;
  t15 = ((int32_t *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int32_t)((*t15)));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t12) * (uint64_t)(t16)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v450;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_34 gg_function_22(int32_t p0, int64_t base) {
  gg_type_34 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int32_t * t7;
  int32_t v0;
  gg_type_34 v1;
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
static gg_type_34 gg_function_11(gg_type_20 p191, gg_type_20 p192, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_34 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_madd_epi16(_mm256_loadu_si256((const __m256i*)p191.data), _mm256_loadu_si256((const __m256i*)p192.data)));
    return packed_result;
  }
#endif
  gg_type_34 t0;
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
  gg_type_34 v193;
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
static void gg_function_27(gg_type_44 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  int32_t t25;
  int32_t t26;
  int64_t t27;
  int32_t t28;
  int32_t t29;
  int64_t t30;
  int32_t t31;
  int32_t t32;
  int64_t t33;
  int32_t t34;
  int32_t t35;
  int64_t t36;
  int32_t t37;
  int32_t t38;
  int64_t t39;
  int32_t t40;
  int32_t t41;
  int64_t t42;
  int32_t t43;
  int32_t t44;
  int64_t t45;
  int32_t t46;
  int32_t t47;
  int64_t t48;
  int32_t t49;
  int32_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  gg_slice t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  gg_type_27 * t60;
  gg_slice t61;
  int64_t t62;
  gg_type_7 * t63;
  int64_t t64;
  gg_type_30 * t65;
  gg_slice t66;
  int64_t t67;
  int32_t * t68;
  int16_t t69;
  gg_type_34 t70;
  gg_type_20 t71;
  gg_type_7 t72;
  gg_slice t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  gg_type_18 * t81;
  gg_slice t82;
  int64_t t83;
  gg_type_17 * t84;
  gg_type_17 t85;
  gg_type_20 t86;
  gg_type_34 t87;
  gg_type_34 t88;
  gg_type_20 t89;
  gg_type_7 t90;
  gg_slice t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  gg_type_18 * t99;
  gg_slice t100;
  int64_t t101;
  gg_type_17 * t102;
  gg_type_17 t103;
  gg_type_20 t104;
  gg_type_34 t105;
  gg_type_34 t106;
  gg_type_20 t107;
  gg_type_7 t108;
  gg_slice t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  gg_type_18 * t117;
  gg_slice t118;
  int64_t t119;
  gg_type_17 * t120;
  gg_type_17 t121;
  gg_type_20 t122;
  gg_type_34 t123;
  gg_type_34 t124;
  gg_type_20 t125;
  gg_type_7 t126;
  gg_slice t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  gg_type_18 * t135;
  gg_slice t136;
  int64_t t137;
  gg_type_17 * t138;
  gg_type_17 t139;
  gg_type_20 t140;
  gg_type_34 t141;
  gg_type_34 t142;
  gg_type_20 t143;
  gg_type_7 t144;
  gg_slice t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  gg_type_18 * t153;
  gg_slice t154;
  int64_t t155;
  gg_type_17 * t156;
  gg_type_17 t157;
  gg_type_20 t158;
  gg_type_34 t159;
  gg_type_34 t160;
  gg_type_20 t161;
  gg_type_7 t162;
  gg_slice t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  gg_type_18 * t171;
  gg_slice t172;
  int64_t t173;
  gg_type_17 * t174;
  gg_type_17 t175;
  gg_type_20 t176;
  gg_type_34 t177;
  gg_type_34 t178;
  gg_type_20 t179;
  gg_type_7 t180;
  gg_slice t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  gg_type_18 * t189;
  gg_slice t190;
  int64_t t191;
  gg_type_17 * t192;
  gg_type_17 t193;
  gg_type_20 t194;
  gg_type_34 t195;
  gg_type_34 t196;
  gg_type_20 t197;
  gg_type_7 t198;
  gg_slice t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  gg_type_18 * t207;
  gg_slice t208;
  int64_t t209;
  gg_type_17 * t210;
  gg_type_17 t211;
  gg_type_20 t212;
  gg_type_34 t213;
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
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  gg_type_34 t240;
  int64_t t241;
  gg_type_34 * t242;
  gg_type_34 t243;
  int64_t t244;
  gg_type_34 * t245;
  gg_type_34 t246;
  gg_type_34 t247;
  int64_t t248;
  gg_type_30 * t249;
  gg_slice t250;
  int64_t t251;
  int32_t * t252;
  int32_t t253;
  gg_type_34 t254;
  gg_type_34 t255;
  int32_t t256;
  int32_t t257;
  int64_t t258;
  int64_t t259;
  int32_t * t260;
  gg_type_34 t261;
  int64_t t262;
  int32_t * t263;
  gg_type_34 t264;
  int64_t t265;
  int32_t * t266;
  gg_type_34 t267;
  int64_t t268;
  int32_t * t269;
  gg_type_34 t270;
  int64_t t271;
  int32_t * t272;
  gg_type_34 t273;
  int64_t t274;
  int32_t * t275;
  gg_type_34 t276;
  int64_t t277;
  int32_t * t278;
  gg_type_34 t279;
  int64_t t280;
  int32_t * t281;
  gg_type_34 t282;
  int64_t t283;
  float * t284;
  float t285;
  float t286;
  gg_type_34 t287;
  int64_t t288;
  float * t289;
  float t290;
  gg_type_34 t291;
  gg_type_36 t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  gg_type_36 * t304;
  gg_slice t305;
  int64_t t306;
  float * t307;
  int64_t t308;
  float * t309;
  float t310;
  gg_slice t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  float * t319;
  float t320;
  int64_t t321;
  gg_type_36 * t322;
  gg_slice t323;
  int64_t t324;
  float * t325;
  float t326;
  float t327;
  int64_t t328;
  gg_type_44 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_51 v6;
  gg_type_49 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  gg_type_34 v17;
  gg_type_34 v18;
  gg_type_34 v19;
  gg_type_34 v20;
  gg_type_34 v21;
  gg_type_34 v22;
  gg_type_34 v23;
  gg_type_34 v24;
  gg_type_34 v25;
  int64_t v26;
  gg_type_7 v27;
  gg_type_20 v28;
  int64_t v29;
  int64_t v30;
  gg_type_34 v31;
  gg_type_36 v32;
  int64_t v33;
  int64_t v34;
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
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(v5));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = ((int64_t)UINT64_C(2));
  t19 = 0;
label3:;
  if (t19 >= t20) goto label5;
  v15 = t19;
  {
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(v15));
  t23 = ((int64_t)(((int64_t)UINT64_C(8))));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t24)))));
  memset(&t25, 0, sizeof(t25));
  t26 = ((int32_t)(t25));
  for (t27=0; t27<8; t27++) (v17).data[t27] = t26;
  memset(&t28, 0, sizeof(t28));
  t29 = ((int32_t)(t28));
  for (t30=0; t30<8; t30++) (v18).data[t30] = t29;
  memset(&t31, 0, sizeof(t31));
  t32 = ((int32_t)(t31));
  for (t33=0; t33<8; t33++) (v19).data[t33] = t32;
  memset(&t34, 0, sizeof(t34));
  t35 = ((int32_t)(t34));
  for (t36=0; t36<8; t36++) (v20).data[t36] = t35;
  memset(&t37, 0, sizeof(t37));
  t38 = ((int32_t)(t37));
  for (t39=0; t39<8; t39++) (v21).data[t39] = t38;
  memset(&t40, 0, sizeof(t40));
  t41 = ((int32_t)(t40));
  for (t42=0; t42<8; t42++) (v22).data[t42] = t41;
  memset(&t43, 0, sizeof(t43));
  t44 = ((int32_t)(t43));
  for (t45=0; t45<8; t45++) (v23).data[t45] = t44;
  memset(&t46, 0, sizeof(t46));
  t47 = ((int32_t)(t46));
  for (t48=0; t48<8; t48++) (v24).data[t48] = t47;
  memset(&t49, 0, sizeof(t49));
  t50 = ((int32_t)(t49));
  for (t51=0; t51<8; t51++) (v25).data[t51] = t50;
  t53 = ((int64_t)UINT64_C(8));
  t52 = 0;
label6:;
  if (t52 >= t53) goto label8;
  v26 = t52;
  {
  t54 = gg_view((((*(v6))).f24).data, 256);
  t55 = ((int64_t)(v12));
  t56 = ((int64_t)(v4));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t58 = ((int64_t)(v5));
  t59 = ((int64_t)((uint64_t)(t57) + (uint64_t)(t58)));
  t60 = ((gg_type_27 *)t54.data) + gg_index(t59, t54.len);
  t61 = gg_view(((*t60)).data, 8);
  t62 = v26;
  t63 = ((gg_type_7 *)t61.data) + gg_index(t62, t61.len);
  v27 = (*t63);
  /* FG_DIRECT_SLICE_INDEX */
  t64 = v14;
  t65 = ((gg_type_30 *)(((*(v6))).f27).data) + gg_index(t64, ((int64_t)UINT64_C(256)));
  t66 = gg_view(((*t65)).data, 16);
  t67 = v26;
  t68 = ((int32_t *)t66.data) + gg_index(t67, t66.len);
  t69 = ((int16_t)(((int16_t)((*t68)))));
  v28 = gg_function_8(t69, base + 0);
  t70 = v17;
  t71 = v28;
  t72 = v27;
  t73 = gg_view((((*(v7))).f30).data, 256);
  t74 = ((int64_t)(v16));
  t75 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t74) + (uint64_t)(t75)))));
  t77 = ((int64_t)(v4));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t76) * (uint64_t)(t77)))));
  t79 = ((int64_t)(v5));
  t80 = ((int64_t)((uint64_t)(t78) + (uint64_t)(t79)));
  t81 = ((gg_type_18 *)t73.data) + gg_index(t80, t73.len);
  t82 = gg_view(((*t81)).data, 8);
  t83 = v26;
  t84 = ((gg_type_17 *)t82.data) + gg_index(t83, t82.len);
  t85 = (*t84);
  t86 = gg_function_10(t72, t85, base + 0);
  t87 = gg_function_11(t71, t86, base + 0);
  v17 = gg_function_19(t70, t87, base + 0);
  t88 = v18;
  t89 = v28;
  t90 = v27;
  t91 = gg_view((((*(v7))).f30).data, 256);
  t92 = ((int64_t)(v16));
  t93 = ((int64_t)(((int64_t)UINT64_C(1))));
  t94 = ((int64_t)(((int64_t)((uint64_t)(t92) + (uint64_t)(t93)))));
  t95 = ((int64_t)(v4));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t94) * (uint64_t)(t95)))));
  t97 = ((int64_t)(v5));
  t98 = ((int64_t)((uint64_t)(t96) + (uint64_t)(t97)));
  t99 = ((gg_type_18 *)t91.data) + gg_index(t98, t91.len);
  t100 = gg_view(((*t99)).data, 8);
  t101 = v26;
  t102 = ((gg_type_17 *)t100.data) + gg_index(t101, t100.len);
  t103 = (*t102);
  t104 = gg_function_10(t90, t103, base + 0);
  t105 = gg_function_11(t89, t104, base + 0);
  v18 = gg_function_19(t88, t105, base + 0);
  t106 = v19;
  t107 = v28;
  t108 = v27;
  t109 = gg_view((((*(v7))).f30).data, 256);
  t110 = ((int64_t)(v16));
  t111 = ((int64_t)(((int64_t)UINT64_C(2))));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  t113 = ((int64_t)(v4));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t113)))));
  t115 = ((int64_t)(v5));
  t116 = ((int64_t)((uint64_t)(t114) + (uint64_t)(t115)));
  t117 = ((gg_type_18 *)t109.data) + gg_index(t116, t109.len);
  t118 = gg_view(((*t117)).data, 8);
  t119 = v26;
  t120 = ((gg_type_17 *)t118.data) + gg_index(t119, t118.len);
  t121 = (*t120);
  t122 = gg_function_10(t108, t121, base + 0);
  t123 = gg_function_11(t107, t122, base + 0);
  v19 = gg_function_19(t106, t123, base + 0);
  t124 = v20;
  t125 = v28;
  t126 = v27;
  t127 = gg_view((((*(v7))).f30).data, 256);
  t128 = ((int64_t)(v16));
  t129 = ((int64_t)(((int64_t)UINT64_C(3))));
  t130 = ((int64_t)(((int64_t)((uint64_t)(t128) + (uint64_t)(t129)))));
  t131 = ((int64_t)(v4));
  t132 = ((int64_t)(((int64_t)((uint64_t)(t130) * (uint64_t)(t131)))));
  t133 = ((int64_t)(v5));
  t134 = ((int64_t)((uint64_t)(t132) + (uint64_t)(t133)));
  t135 = ((gg_type_18 *)t127.data) + gg_index(t134, t127.len);
  t136 = gg_view(((*t135)).data, 8);
  t137 = v26;
  t138 = ((gg_type_17 *)t136.data) + gg_index(t137, t136.len);
  t139 = (*t138);
  t140 = gg_function_10(t126, t139, base + 0);
  t141 = gg_function_11(t125, t140, base + 0);
  v20 = gg_function_19(t124, t141, base + 0);
  t142 = v21;
  t143 = v28;
  t144 = v27;
  t145 = gg_view((((*(v7))).f30).data, 256);
  t146 = ((int64_t)(v16));
  t147 = ((int64_t)(((int64_t)UINT64_C(4))));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t146) + (uint64_t)(t147)))));
  t149 = ((int64_t)(v4));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) * (uint64_t)(t149)))));
  t151 = ((int64_t)(v5));
  t152 = ((int64_t)((uint64_t)(t150) + (uint64_t)(t151)));
  t153 = ((gg_type_18 *)t145.data) + gg_index(t152, t145.len);
  t154 = gg_view(((*t153)).data, 8);
  t155 = v26;
  t156 = ((gg_type_17 *)t154.data) + gg_index(t155, t154.len);
  t157 = (*t156);
  t158 = gg_function_10(t144, t157, base + 0);
  t159 = gg_function_11(t143, t158, base + 0);
  v21 = gg_function_19(t142, t159, base + 0);
  t160 = v22;
  t161 = v28;
  t162 = v27;
  t163 = gg_view((((*(v7))).f30).data, 256);
  t164 = ((int64_t)(v16));
  t165 = ((int64_t)(((int64_t)UINT64_C(5))));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t165)))));
  t167 = ((int64_t)(v4));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))));
  t169 = ((int64_t)(v5));
  t170 = ((int64_t)((uint64_t)(t168) + (uint64_t)(t169)));
  t171 = ((gg_type_18 *)t163.data) + gg_index(t170, t163.len);
  t172 = gg_view(((*t171)).data, 8);
  t173 = v26;
  t174 = ((gg_type_17 *)t172.data) + gg_index(t173, t172.len);
  t175 = (*t174);
  t176 = gg_function_10(t162, t175, base + 0);
  t177 = gg_function_11(t161, t176, base + 0);
  v22 = gg_function_19(t160, t177, base + 0);
  t178 = v23;
  t179 = v28;
  t180 = v27;
  t181 = gg_view((((*(v7))).f30).data, 256);
  t182 = ((int64_t)(v16));
  t183 = ((int64_t)(((int64_t)UINT64_C(6))));
  t184 = ((int64_t)(((int64_t)((uint64_t)(t182) + (uint64_t)(t183)))));
  t185 = ((int64_t)(v4));
  t186 = ((int64_t)(((int64_t)((uint64_t)(t184) * (uint64_t)(t185)))));
  t187 = ((int64_t)(v5));
  t188 = ((int64_t)((uint64_t)(t186) + (uint64_t)(t187)));
  t189 = ((gg_type_18 *)t181.data) + gg_index(t188, t181.len);
  t190 = gg_view(((*t189)).data, 8);
  t191 = v26;
  t192 = ((gg_type_17 *)t190.data) + gg_index(t191, t190.len);
  t193 = (*t192);
  t194 = gg_function_10(t180, t193, base + 0);
  t195 = gg_function_11(t179, t194, base + 0);
  v23 = gg_function_19(t178, t195, base + 0);
  t196 = v24;
  t197 = v28;
  t198 = v27;
  t199 = gg_view((((*(v7))).f30).data, 256);
  t200 = ((int64_t)(v16));
  t201 = ((int64_t)(((int64_t)UINT64_C(7))));
  t202 = ((int64_t)(((int64_t)((uint64_t)(t200) + (uint64_t)(t201)))));
  t203 = ((int64_t)(v4));
  t204 = ((int64_t)(((int64_t)((uint64_t)(t202) * (uint64_t)(t203)))));
  t205 = ((int64_t)(v5));
  t206 = ((int64_t)((uint64_t)(t204) + (uint64_t)(t205)));
  t207 = ((gg_type_18 *)t199.data) + gg_index(t206, t199.len);
  t208 = gg_view(((*t207)).data, 8);
  t209 = v26;
  t210 = ((gg_type_17 *)t208.data) + gg_index(t209, t208.len);
  t211 = (*t210);
  t212 = gg_function_10(t198, t211, base + 0);
  t213 = gg_function_11(t197, t212, base + 0);
  v24 = gg_function_19(t196, t213, base + 0);
  t214 = ((int64_t)(v5));
  t215 = ((int64_t)(((int64_t)UINT64_C(16))));
  t216 = ((int64_t)(((int64_t)((uint64_t)(t214) * (uint64_t)(t215)))));
  t217 = ((int64_t)(((int64_t)UINT64_C(2))));
  t218 = ((int64_t)(v26));
  t219 = ((int64_t)(((int64_t)((uint64_t)(t217) * (uint64_t)(t218)))));
  t220 = ((int64_t)(((int64_t)((uint64_t)(t216) + (uint64_t)(t219)))));
  t221 = ((int64_t)(v10));
  t222 = ((int64_t)(((int64_t)((uint64_t)(t220) * (uint64_t)(t221)))));
  t223 = ((int64_t)(v16));
  t224 = ((int64_t)(((int64_t)((uint64_t)(t222) + (uint64_t)(t223)))));
  t225 = ((int64_t)(((int64_t)UINT64_C(8))));
  v29 = ((int64_t)(((int64_t)gg_div(t224, t225))));
  t226 = ((int64_t)(v5));
  t227 = ((int64_t)(((int64_t)UINT64_C(16))));
  t228 = ((int64_t)(((int64_t)((uint64_t)(t226) * (uint64_t)(t227)))));
  t229 = ((int64_t)(((int64_t)UINT64_C(2))));
  t230 = ((int64_t)(v26));
  t231 = ((int64_t)(((int64_t)((uint64_t)(t229) * (uint64_t)(t230)))));
  t232 = ((int64_t)(((int64_t)((uint64_t)(t228) + (uint64_t)(t231)))));
  t233 = ((int64_t)(((int64_t)UINT64_C(1))));
  t234 = ((int64_t)(((int64_t)((uint64_t)(t232) + (uint64_t)(t233)))));
  t235 = ((int64_t)(v10));
  t236 = ((int64_t)(((int64_t)((uint64_t)(t234) * (uint64_t)(t235)))));
  t237 = ((int64_t)(v16));
  t238 = ((int64_t)(((int64_t)((uint64_t)(t236) + (uint64_t)(t237)))));
  t239 = ((int64_t)(((int64_t)UINT64_C(8))));
  v30 = ((int64_t)(((int64_t)gg_div(t238, t239))));
  t240 = v25;
  /* FG_DIRECT_SLICE_INDEX */
  t241 = v29;
  t242 = ((gg_type_34 *)(((*(v7))).f31).data) + gg_index(t241, ((int64_t)UINT64_C(512)));
  t243 = (*t242);
  /* FG_DIRECT_SLICE_INDEX */
  t244 = v30;
  t245 = ((gg_type_34 *)(((*(v7))).f31).data) + gg_index(t244, ((int64_t)UINT64_C(512)));
  t246 = (*t245);
  t247 = gg_function_19(t243, t246, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t248 = v14;
  t249 = ((gg_type_30 *)(((*(v6))).f28).data) + gg_index(t248, ((int64_t)UINT64_C(256)));
  t250 = gg_view(((*t249)).data, 16);
  t251 = v26;
  t252 = ((int32_t *)t250.data) + gg_index(t251, t250.len);
  t253 = ((int32_t)((*t252)));
  t254 = gg_function_22(t253, base + 0);
  t255 = gg_function_28(t247, t254, base + 0);
  v25 = gg_function_19(t240, t255, base + 0);
  }
label7:;
  t52++; goto label6;
label8:;
  memset(&t256, 0, sizeof(t256));
  t257 = ((int32_t)(t256));
  for (t258=0; t258<8; t258++) (v31).data[t258] = t257;
  /* FG_DIRECT_SLICE_INDEX */
  t259 = ((int64_t)UINT64_C(0));
  t260 = ((int32_t *)(v31).data) + gg_index(t259, ((int64_t)UINT64_C(8)));
  t261 = v17;
  (*t260) = gg_function_20(t261, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t262 = ((int64_t)UINT64_C(1));
  t263 = ((int32_t *)(v31).data) + gg_index(t262, ((int64_t)UINT64_C(8)));
  t264 = v18;
  (*t263) = gg_function_20(t264, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t265 = ((int64_t)UINT64_C(2));
  t266 = ((int32_t *)(v31).data) + gg_index(t265, ((int64_t)UINT64_C(8)));
  t267 = v19;
  (*t266) = gg_function_20(t267, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t268 = ((int64_t)UINT64_C(3));
  t269 = ((int32_t *)(v31).data) + gg_index(t268, ((int64_t)UINT64_C(8)));
  t270 = v20;
  (*t269) = gg_function_20(t270, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t271 = ((int64_t)UINT64_C(4));
  t272 = ((int32_t *)(v31).data) + gg_index(t271, ((int64_t)UINT64_C(8)));
  t273 = v21;
  (*t272) = gg_function_20(t273, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t274 = ((int64_t)UINT64_C(5));
  t275 = ((int32_t *)(v31).data) + gg_index(t274, ((int64_t)UINT64_C(8)));
  t276 = v22;
  (*t275) = gg_function_20(t276, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t277 = ((int64_t)UINT64_C(6));
  t278 = ((int32_t *)(v31).data) + gg_index(t277, ((int64_t)UINT64_C(8)));
  t279 = v23;
  (*t278) = gg_function_20(t279, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t280 = ((int64_t)UINT64_C(7));
  t281 = ((int32_t *)(v31).data) + gg_index(t280, ((int64_t)UINT64_C(8)));
  t282 = v24;
  (*t281) = gg_function_20(t282, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t283 = v13;
  t284 = ((float *)(((*(v6))).f26).data) + gg_index(t283, ((int64_t)UINT64_C(256)));
  t285 = (*t284);
  t286 = ((float)((-(t285))));
  t287 = v25;
  /* FG_DIRECT_SLICE_INDEX */
  t288 = v13;
  t289 = ((float *)(((*(v6))).f25).data) + gg_index(t288, ((int64_t)UINT64_C(256)));
  t290 = ((float)((*t289)));
  t291 = v31;
  t292 = gg_function_24(t290, t291, base + 0);
  v32 = gg_function_29(t286, t287, t292, base + 0);
  t293 = ((int64_t)(v3));
  t294 = ((int64_t)(v11));
  t295 = ((int64_t)(((int64_t)((uint64_t)(t293) + (uint64_t)(t294)))));
  t296 = ((int64_t)(((int64_t)UINT64_C(32))));
  t297 = ((int64_t)(((int64_t)((uint64_t)(t295) * (uint64_t)(t296)))));
  t298 = ((int64_t)(v16));
  t299 = ((int64_t)(((int64_t)UINT64_C(8))));
  t300 = ((int64_t)(((int64_t)gg_div(t298, t299))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t297) + (uint64_t)(t300)))));
  t302 = ((int64_t)UINT64_C(8));
  t301 = 0;
label9:;
  if (t301 >= t302) goto label11;
  v34 = t301;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t303 = v33;
  t304 = ((gg_type_36 *)(((*(v0))).f34).data) + gg_index(t303, ((int64_t)UINT64_C(8192)));
  t305 = gg_view(((*t304)).data, 8);
  t306 = v34;
  t307 = ((float *)t305.data) + gg_index(t306, t305.len);
  /* FG_DIRECT_SLICE_INDEX */
  t308 = v34;
  t309 = ((float *)(v32).data) + gg_index(t308, ((int64_t)UINT64_C(8)));
  t310 = ((float)((*t309)));
  t311 = gg_view((((*(v7))).f32).data, 256);
  t312 = ((int64_t)(v5));
  t313 = ((int64_t)(((int64_t)UINT64_C(16))));
  t314 = ((int64_t)(((int64_t)((uint64_t)(t312) * (uint64_t)(t313)))));
  t315 = ((int64_t)(v16));
  t316 = ((int64_t)(((int64_t)((uint64_t)(t314) + (uint64_t)(t315)))));
  t317 = ((int64_t)(v34));
  t318 = ((int64_t)((uint64_t)(t316) + (uint64_t)(t317)));
  t319 = ((float *)t311.data) + gg_index(t318, t311.len);
  t320 = ((float)((*t319)));
  /* FG_DIRECT_SLICE_INDEX */
  t321 = v33;
  t322 = ((gg_type_36 *)(((*(v0))).f34).data) + gg_index(t321, ((int64_t)UINT64_C(8192)));
  t323 = gg_view(((*t322)).data, 8);
  t324 = v34;
  t325 = ((float *)t323.data) + gg_index(t324, t323.len);
  t326 = ((float)((*t325)));
  t327 = fmaf(t310, t320, t326);
  (*t307) = ((float)(t327));
  }
label10:;
  t301++; goto label9;
label11:;
  }
label4:;
  t19++; goto label3;
label5:;
  }
label1:;
  t7++; goto label0;
label2:;
  t328 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg_tiled_witness(t328);
  }
gg_done:;
}
static gg_type_34 gg_function_23(gg_type_34 p0, gg_type_34 p1, int64_t base) {
  gg_type_34 t0;
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
  gg_type_34 v0;
  gg_type_34 v1;
  gg_type_34 v2;
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
static void gg_function_15(gg_type_44 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  int32_t t22;
  int32_t t23;
  int64_t t24;
  int32_t t25;
  int32_t t26;
  int64_t t27;
  int32_t t28;
  int32_t t29;
  int64_t t30;
  int32_t t31;
  int32_t t32;
  int64_t t33;
  int32_t t34;
  int32_t t35;
  int64_t t36;
  int32_t t37;
  int32_t t38;
  int64_t t39;
  int32_t t40;
  int32_t t41;
  int64_t t42;
  int32_t t43;
  int32_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  gg_slice t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  gg_type_27 * t54;
  gg_slice t55;
  int64_t t56;
  gg_type_7 * t57;
  gg_type_7 t58;
  int64_t t59;
  gg_type_30 * t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int32_t * t65;
  int16_t t66;
  int64_t t67;
  gg_type_30 * t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int32_t * t75;
  int16_t t76;
  gg_type_34 t77;
  gg_type_20 t78;
  gg_slice t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  gg_type_18 * t87;
  gg_slice t88;
  int64_t t89;
  gg_type_17 * t90;
  gg_type_17 t91;
  gg_type_7 t92;
  gg_type_17 t93;
  gg_type_20 t94;
  gg_type_34 t95;
  gg_type_34 t96;
  gg_type_20 t97;
  gg_slice t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  gg_type_18 * t106;
  gg_slice t107;
  int64_t t108;
  gg_type_17 * t109;
  gg_type_17 t110;
  gg_type_7 t111;
  gg_type_17 t112;
  gg_type_20 t113;
  gg_type_34 t114;
  gg_type_34 t115;
  gg_type_20 t116;
  gg_slice t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  gg_type_18 * t125;
  gg_slice t126;
  int64_t t127;
  gg_type_17 * t128;
  gg_type_17 t129;
  gg_type_7 t130;
  gg_type_17 t131;
  gg_type_20 t132;
  gg_type_34 t133;
  gg_type_34 t134;
  gg_type_20 t135;
  gg_slice t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  gg_type_18 * t144;
  gg_slice t145;
  int64_t t146;
  gg_type_17 * t147;
  gg_type_17 t148;
  gg_type_7 t149;
  gg_type_17 t150;
  gg_type_20 t151;
  gg_type_34 t152;
  gg_type_34 t153;
  gg_type_20 t154;
  gg_slice t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  gg_type_18 * t163;
  gg_slice t164;
  int64_t t165;
  gg_type_17 * t166;
  gg_type_17 t167;
  gg_type_7 t168;
  gg_type_17 t169;
  gg_type_20 t170;
  gg_type_34 t171;
  gg_type_34 t172;
  gg_type_20 t173;
  gg_slice t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  gg_type_18 * t182;
  gg_slice t183;
  int64_t t184;
  gg_type_17 * t185;
  gg_type_17 t186;
  gg_type_7 t187;
  gg_type_17 t188;
  gg_type_20 t189;
  gg_type_34 t190;
  gg_type_34 t191;
  gg_type_20 t192;
  gg_slice t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  gg_type_18 * t201;
  gg_slice t202;
  int64_t t203;
  gg_type_17 * t204;
  gg_type_17 t205;
  gg_type_7 t206;
  gg_type_17 t207;
  gg_type_20 t208;
  gg_type_34 t209;
  gg_type_34 t210;
  gg_type_20 t211;
  gg_slice t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  gg_type_18 * t220;
  gg_slice t221;
  int64_t t222;
  gg_type_17 * t223;
  gg_type_17 t224;
  gg_type_7 t225;
  gg_type_17 t226;
  gg_type_20 t227;
  gg_type_34 t228;
  int32_t t229;
  int32_t t230;
  int64_t t231;
  int64_t t232;
  int32_t * t233;
  gg_type_34 t234;
  int64_t t235;
  int32_t * t236;
  gg_type_34 t237;
  int64_t t238;
  int32_t * t239;
  gg_type_34 t240;
  int64_t t241;
  int32_t * t242;
  gg_type_34 t243;
  int64_t t244;
  int32_t * t245;
  gg_type_34 t246;
  int64_t t247;
  int32_t * t248;
  gg_type_34 t249;
  int64_t t250;
  int32_t * t251;
  gg_type_34 t252;
  int64_t t253;
  int32_t * t254;
  gg_type_34 t255;
  int64_t t256;
  float * t257;
  float t258;
  gg_type_34 t259;
  int64_t t260;
  gg_type_30 * t261;
  gg_slice t262;
  int64_t t263;
  int32_t * t264;
  int32_t t265;
  gg_type_34 t266;
  gg_type_34 t267;
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
  int64_t t278;
  gg_type_36 * t279;
  gg_slice t280;
  int64_t t281;
  float * t282;
  int64_t t283;
  float * t284;
  float t285;
  gg_slice t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  float * t294;
  float t295;
  int64_t t296;
  gg_type_36 * t297;
  gg_slice t298;
  int64_t t299;
  float * t300;
  float t301;
  float t302;
  int64_t t303;
  gg_type_44 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_51 v6;
  gg_type_49 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  gg_type_34 v16;
  gg_type_34 v17;
  gg_type_34 v18;
  gg_type_34 v19;
  gg_type_34 v20;
  gg_type_34 v21;
  gg_type_34 v22;
  gg_type_34 v23;
  int64_t v24;
  gg_type_17 v25;
  gg_type_20 v26;
  gg_type_34 v27;
  gg_type_36 v28;
  int64_t v29;
  int64_t v30;
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
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((int64_t)(v5));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t17 = ((int64_t)UINT64_C(2));
  t16 = 0;
label3:;
  if (t16 >= t17) goto label5;
  v14 = t16;
  {
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(v14));
  t20 = ((int64_t)(((int64_t)UINT64_C(8))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t21)))));
  memset(&t22, 0, sizeof(t22));
  t23 = ((int32_t)(t22));
  for (t24=0; t24<8; t24++) (v16).data[t24] = t23;
  memset(&t25, 0, sizeof(t25));
  t26 = ((int32_t)(t25));
  for (t27=0; t27<8; t27++) (v17).data[t27] = t26;
  memset(&t28, 0, sizeof(t28));
  t29 = ((int32_t)(t28));
  for (t30=0; t30<8; t30++) (v18).data[t30] = t29;
  memset(&t31, 0, sizeof(t31));
  t32 = ((int32_t)(t31));
  for (t33=0; t33<8; t33++) (v19).data[t33] = t32;
  memset(&t34, 0, sizeof(t34));
  t35 = ((int32_t)(t34));
  for (t36=0; t36<8; t36++) (v20).data[t36] = t35;
  memset(&t37, 0, sizeof(t37));
  t38 = ((int32_t)(t37));
  for (t39=0; t39<8; t39++) (v21).data[t39] = t38;
  memset(&t40, 0, sizeof(t40));
  t41 = ((int32_t)(t40));
  for (t42=0; t42<8; t42++) (v22).data[t42] = t41;
  memset(&t43, 0, sizeof(t43));
  t44 = ((int32_t)(t43));
  for (t45=0; t45<8; t45++) (v23).data[t45] = t44;
  t47 = ((int64_t)UINT64_C(8));
  t46 = 0;
label6:;
  if (t46 >= t47) goto label8;
  v24 = t46;
  {
  t48 = gg_view((((*(v6))).f24).data, 256);
  t49 = ((int64_t)(v11));
  t50 = ((int64_t)(v4));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))));
  t52 = ((int64_t)(v5));
  t53 = ((int64_t)((uint64_t)(t51) + (uint64_t)(t52)));
  t54 = ((gg_type_27 *)t48.data) + gg_index(t53, t48.len);
  t55 = gg_view(((*t54)).data, 8);
  t56 = v24;
  t57 = ((gg_type_7 *)t55.data) + gg_index(t56, t55.len);
  t58 = (*t57);
  v25 = gg_function_16(t58, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t59 = v13;
  t60 = ((gg_type_30 *)(((*(v6))).f27).data) + gg_index(t59, ((int64_t)UINT64_C(256)));
  t61 = gg_view(((*t60)).data, 16);
  t62 = ((int64_t)(((int64_t)UINT64_C(2))));
  t63 = ((int64_t)(v24));
  t64 = ((int64_t)((uint64_t)(t62) * (uint64_t)(t63)));
  t65 = ((int32_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((int16_t)(((int16_t)((*t65)))));
  /* FG_DIRECT_SLICE_INDEX */
  t67 = v13;
  t68 = ((gg_type_30 *)(((*(v6))).f27).data) + gg_index(t67, ((int64_t)UINT64_C(256)));
  t69 = gg_view(((*t68)).data, 16);
  t70 = ((int64_t)(((int64_t)UINT64_C(2))));
  t71 = ((int64_t)(v24));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)UINT64_C(1))));
  t74 = ((int64_t)((uint64_t)(t72) + (uint64_t)(t73)));
  t75 = ((int32_t *)t69.data) + gg_index(t74, t69.len);
  t76 = ((int16_t)(((int16_t)((*t75)))));
  v26 = gg_function_17(t66, t76, base + 0);
  t77 = v16;
  t78 = v26;
  t79 = gg_view((((*(v7))).f30).data, 256);
  t80 = ((int64_t)(v15));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t81)))));
  t83 = ((int64_t)(v4));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) * (uint64_t)(t83)))));
  t85 = ((int64_t)(v5));
  t86 = ((int64_t)((uint64_t)(t84) + (uint64_t)(t85)));
  t87 = ((gg_type_18 *)t79.data) + gg_index(t86, t79.len);
  t88 = gg_view(((*t87)).data, 8);
  t89 = v24;
  t90 = ((gg_type_17 *)t88.data) + gg_index(t89, t88.len);
  t91 = (*t90);
  t92 = gg_function_18(t91, base + 0);
  t93 = v25;
  t94 = gg_function_10(t92, t93, base + 0);
  t95 = gg_function_11(t78, t94, base + 0);
  v16 = gg_function_19(t77, t95, base + 0);
  t96 = v17;
  t97 = v26;
  t98 = gg_view((((*(v7))).f30).data, 256);
  t99 = ((int64_t)(v15));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) + (uint64_t)(t100)))));
  t102 = ((int64_t)(v4));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) * (uint64_t)(t102)))));
  t104 = ((int64_t)(v5));
  t105 = ((int64_t)((uint64_t)(t103) + (uint64_t)(t104)));
  t106 = ((gg_type_18 *)t98.data) + gg_index(t105, t98.len);
  t107 = gg_view(((*t106)).data, 8);
  t108 = v24;
  t109 = ((gg_type_17 *)t107.data) + gg_index(t108, t107.len);
  t110 = (*t109);
  t111 = gg_function_18(t110, base + 0);
  t112 = v25;
  t113 = gg_function_10(t111, t112, base + 0);
  t114 = gg_function_11(t97, t113, base + 0);
  v17 = gg_function_19(t96, t114, base + 0);
  t115 = v18;
  t116 = v26;
  t117 = gg_view((((*(v7))).f30).data, 256);
  t118 = ((int64_t)(v15));
  t119 = ((int64_t)(((int64_t)UINT64_C(2))));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t118) + (uint64_t)(t119)))));
  t121 = ((int64_t)(v4));
  t122 = ((int64_t)(((int64_t)((uint64_t)(t120) * (uint64_t)(t121)))));
  t123 = ((int64_t)(v5));
  t124 = ((int64_t)((uint64_t)(t122) + (uint64_t)(t123)));
  t125 = ((gg_type_18 *)t117.data) + gg_index(t124, t117.len);
  t126 = gg_view(((*t125)).data, 8);
  t127 = v24;
  t128 = ((gg_type_17 *)t126.data) + gg_index(t127, t126.len);
  t129 = (*t128);
  t130 = gg_function_18(t129, base + 0);
  t131 = v25;
  t132 = gg_function_10(t130, t131, base + 0);
  t133 = gg_function_11(t116, t132, base + 0);
  v18 = gg_function_19(t115, t133, base + 0);
  t134 = v19;
  t135 = v26;
  t136 = gg_view((((*(v7))).f30).data, 256);
  t137 = ((int64_t)(v15));
  t138 = ((int64_t)(((int64_t)UINT64_C(3))));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t137) + (uint64_t)(t138)))));
  t140 = ((int64_t)(v4));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t139) * (uint64_t)(t140)))));
  t142 = ((int64_t)(v5));
  t143 = ((int64_t)((uint64_t)(t141) + (uint64_t)(t142)));
  t144 = ((gg_type_18 *)t136.data) + gg_index(t143, t136.len);
  t145 = gg_view(((*t144)).data, 8);
  t146 = v24;
  t147 = ((gg_type_17 *)t145.data) + gg_index(t146, t145.len);
  t148 = (*t147);
  t149 = gg_function_18(t148, base + 0);
  t150 = v25;
  t151 = gg_function_10(t149, t150, base + 0);
  t152 = gg_function_11(t135, t151, base + 0);
  v19 = gg_function_19(t134, t152, base + 0);
  t153 = v20;
  t154 = v26;
  t155 = gg_view((((*(v7))).f30).data, 256);
  t156 = ((int64_t)(v15));
  t157 = ((int64_t)(((int64_t)UINT64_C(4))));
  t158 = ((int64_t)(((int64_t)((uint64_t)(t156) + (uint64_t)(t157)))));
  t159 = ((int64_t)(v4));
  t160 = ((int64_t)(((int64_t)((uint64_t)(t158) * (uint64_t)(t159)))));
  t161 = ((int64_t)(v5));
  t162 = ((int64_t)((uint64_t)(t160) + (uint64_t)(t161)));
  t163 = ((gg_type_18 *)t155.data) + gg_index(t162, t155.len);
  t164 = gg_view(((*t163)).data, 8);
  t165 = v24;
  t166 = ((gg_type_17 *)t164.data) + gg_index(t165, t164.len);
  t167 = (*t166);
  t168 = gg_function_18(t167, base + 0);
  t169 = v25;
  t170 = gg_function_10(t168, t169, base + 0);
  t171 = gg_function_11(t154, t170, base + 0);
  v20 = gg_function_19(t153, t171, base + 0);
  t172 = v21;
  t173 = v26;
  t174 = gg_view((((*(v7))).f30).data, 256);
  t175 = ((int64_t)(v15));
  t176 = ((int64_t)(((int64_t)UINT64_C(5))));
  t177 = ((int64_t)(((int64_t)((uint64_t)(t175) + (uint64_t)(t176)))));
  t178 = ((int64_t)(v4));
  t179 = ((int64_t)(((int64_t)((uint64_t)(t177) * (uint64_t)(t178)))));
  t180 = ((int64_t)(v5));
  t181 = ((int64_t)((uint64_t)(t179) + (uint64_t)(t180)));
  t182 = ((gg_type_18 *)t174.data) + gg_index(t181, t174.len);
  t183 = gg_view(((*t182)).data, 8);
  t184 = v24;
  t185 = ((gg_type_17 *)t183.data) + gg_index(t184, t183.len);
  t186 = (*t185);
  t187 = gg_function_18(t186, base + 0);
  t188 = v25;
  t189 = gg_function_10(t187, t188, base + 0);
  t190 = gg_function_11(t173, t189, base + 0);
  v21 = gg_function_19(t172, t190, base + 0);
  t191 = v22;
  t192 = v26;
  t193 = gg_view((((*(v7))).f30).data, 256);
  t194 = ((int64_t)(v15));
  t195 = ((int64_t)(((int64_t)UINT64_C(6))));
  t196 = ((int64_t)(((int64_t)((uint64_t)(t194) + (uint64_t)(t195)))));
  t197 = ((int64_t)(v4));
  t198 = ((int64_t)(((int64_t)((uint64_t)(t196) * (uint64_t)(t197)))));
  t199 = ((int64_t)(v5));
  t200 = ((int64_t)((uint64_t)(t198) + (uint64_t)(t199)));
  t201 = ((gg_type_18 *)t193.data) + gg_index(t200, t193.len);
  t202 = gg_view(((*t201)).data, 8);
  t203 = v24;
  t204 = ((gg_type_17 *)t202.data) + gg_index(t203, t202.len);
  t205 = (*t204);
  t206 = gg_function_18(t205, base + 0);
  t207 = v25;
  t208 = gg_function_10(t206, t207, base + 0);
  t209 = gg_function_11(t192, t208, base + 0);
  v22 = gg_function_19(t191, t209, base + 0);
  t210 = v23;
  t211 = v26;
  t212 = gg_view((((*(v7))).f30).data, 256);
  t213 = ((int64_t)(v15));
  t214 = ((int64_t)(((int64_t)UINT64_C(7))));
  t215 = ((int64_t)(((int64_t)((uint64_t)(t213) + (uint64_t)(t214)))));
  t216 = ((int64_t)(v4));
  t217 = ((int64_t)(((int64_t)((uint64_t)(t215) * (uint64_t)(t216)))));
  t218 = ((int64_t)(v5));
  t219 = ((int64_t)((uint64_t)(t217) + (uint64_t)(t218)));
  t220 = ((gg_type_18 *)t212.data) + gg_index(t219, t212.len);
  t221 = gg_view(((*t220)).data, 8);
  t222 = v24;
  t223 = ((gg_type_17 *)t221.data) + gg_index(t222, t221.len);
  t224 = (*t223);
  t225 = gg_function_18(t224, base + 0);
  t226 = v25;
  t227 = gg_function_10(t225, t226, base + 0);
  t228 = gg_function_11(t211, t227, base + 0);
  v23 = gg_function_19(t210, t228, base + 0);
  }
label7:;
  t46++; goto label6;
label8:;
  memset(&t229, 0, sizeof(t229));
  t230 = ((int32_t)(t229));
  for (t231=0; t231<8; t231++) (v27).data[t231] = t230;
  /* FG_DIRECT_SLICE_INDEX */
  t232 = ((int64_t)UINT64_C(0));
  t233 = ((int32_t *)(v27).data) + gg_index(t232, ((int64_t)UINT64_C(8)));
  t234 = v16;
  (*t233) = gg_function_20(t234, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t235 = ((int64_t)UINT64_C(1));
  t236 = ((int32_t *)(v27).data) + gg_index(t235, ((int64_t)UINT64_C(8)));
  t237 = v17;
  (*t236) = gg_function_20(t237, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t238 = ((int64_t)UINT64_C(2));
  t239 = ((int32_t *)(v27).data) + gg_index(t238, ((int64_t)UINT64_C(8)));
  t240 = v18;
  (*t239) = gg_function_20(t240, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t241 = ((int64_t)UINT64_C(3));
  t242 = ((int32_t *)(v27).data) + gg_index(t241, ((int64_t)UINT64_C(8)));
  t243 = v19;
  (*t242) = gg_function_20(t243, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t244 = ((int64_t)UINT64_C(4));
  t245 = ((int32_t *)(v27).data) + gg_index(t244, ((int64_t)UINT64_C(8)));
  t246 = v20;
  (*t245) = gg_function_20(t246, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t247 = ((int64_t)UINT64_C(5));
  t248 = ((int32_t *)(v27).data) + gg_index(t247, ((int64_t)UINT64_C(8)));
  t249 = v21;
  (*t248) = gg_function_20(t249, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t250 = ((int64_t)UINT64_C(6));
  t251 = ((int32_t *)(v27).data) + gg_index(t250, ((int64_t)UINT64_C(8)));
  t252 = v22;
  (*t251) = gg_function_20(t252, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t253 = ((int64_t)UINT64_C(7));
  t254 = ((int32_t *)(v27).data) + gg_index(t253, ((int64_t)UINT64_C(8)));
  t255 = v23;
  (*t254) = gg_function_20(t255, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t256 = v12;
  t257 = ((float *)(((*(v6))).f25).data) + gg_index(t256, ((int64_t)UINT64_C(256)));
  t258 = ((float)((*t257)));
  t259 = v27;
  /* FG_DIRECT_SLICE_INDEX */
  t260 = v13;
  t261 = ((gg_type_30 *)(((*(v6))).f28).data) + gg_index(t260, ((int64_t)UINT64_C(256)));
  t262 = gg_view(((*t261)).data, 16);
  t263 = ((int64_t)UINT64_C(0));
  t264 = ((int32_t *)t262.data) + gg_index(t263, t262.len);
  t265 = ((int32_t)((*t264)));
  t266 = gg_function_22(t265, base + 0);
  t267 = gg_function_23(t259, t266, base + 0);
  v28 = gg_function_24(t258, t267, base + 0);
  t268 = ((int64_t)(v3));
  t269 = ((int64_t)(v10));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t268) + (uint64_t)(t269)))));
  t271 = ((int64_t)(((int64_t)UINT64_C(32))));
  t272 = ((int64_t)(((int64_t)((uint64_t)(t270) * (uint64_t)(t271)))));
  t273 = ((int64_t)(v15));
  t274 = ((int64_t)(((int64_t)UINT64_C(8))));
  t275 = ((int64_t)(((int64_t)gg_div(t273, t274))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t272) + (uint64_t)(t275)))));
  t277 = ((int64_t)UINT64_C(8));
  t276 = 0;
label9:;
  if (t276 >= t277) goto label11;
  v30 = t276;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t278 = v29;
  t279 = ((gg_type_36 *)(((*(v0))).f34).data) + gg_index(t278, ((int64_t)UINT64_C(8192)));
  t280 = gg_view(((*t279)).data, 8);
  t281 = v30;
  t282 = ((float *)t280.data) + gg_index(t281, t280.len);
  /* FG_DIRECT_SLICE_INDEX */
  t283 = v30;
  t284 = ((float *)(v28).data) + gg_index(t283, ((int64_t)UINT64_C(8)));
  t285 = ((float)((*t284)));
  t286 = gg_view((((*(v7))).f32).data, 256);
  t287 = ((int64_t)(v5));
  t288 = ((int64_t)(((int64_t)UINT64_C(16))));
  t289 = ((int64_t)(((int64_t)((uint64_t)(t287) * (uint64_t)(t288)))));
  t290 = ((int64_t)(v15));
  t291 = ((int64_t)(((int64_t)((uint64_t)(t289) + (uint64_t)(t290)))));
  t292 = ((int64_t)(v30));
  t293 = ((int64_t)((uint64_t)(t291) + (uint64_t)(t292)));
  t294 = ((float *)t286.data) + gg_index(t293, t286.len);
  t295 = ((float)((*t294)));
  /* FG_DIRECT_SLICE_INDEX */
  t296 = v29;
  t297 = ((gg_type_36 *)(((*(v0))).f34).data) + gg_index(t296, ((int64_t)UINT64_C(8192)));
  t298 = gg_view(((*t297)).data, 8);
  t299 = v30;
  t300 = ((float *)t298.data) + gg_index(t299, t298.len);
  t301 = ((float)((*t300)));
  t302 = fmaf(t285, t295, t301);
  (*t282) = ((float)(t302));
  }
label10:;
  t276++; goto label9;
label11:;
  }
label4:;
  t16++; goto label3;
label5:;
  }
label1:;
  t4++; goto label0;
label2:;
  t303 = ((int64_t)(((int64_t)UINT64_C(14))));
  fg_tiled_witness(t303);
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
static void gg_function_5(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, gg_type_44 p8, uint8_t p9, int64_t base) {
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
  uint8_t t19;
  gg_slice t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  gg_type_22 * t24;
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
  gg_type_15 * t38;
  int64_t t39;
  float * t40;
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
  gg_slice t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  gg_type_34 * t55;
  gg_slice t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int32_t * t60;
  int64_t t61;
  int16_t * t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  gg_type_18 * t66;
  gg_slice t67;
  int64_t t68;
  gg_type_17 * t69;
  uint8_t t70;
  int64_t t71;
  gg_type_17 * t72;
  gg_type_17 t73;
  int64_t t74;
  gg_type_17 * t75;
  int64_t t76;
  float * t77;
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
  int32_t * t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  gg_type_18 * t101;
  gg_slice t102;
  int64_t t103;
  gg_type_17 * t104;
  uint8_t t105;
  gg_type_17 t106;
  int8_t t107;
  int8_t t108;
  int64_t t109;
  gg_type_17 t110;
  int8_t t111;
  int8_t t112;
  int64_t t113;
  gg_slice v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  gg_type_44 v8;
  uint8_t v9;
  gg_type_49 v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  gg_type_22 v16;
  int64_t v17;
  gg_type_50 v18;
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
  v14 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  t13 = ((int64_t)(v12));
  t14 = ((int64_t)(((int64_t)UINT64_C(16))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = ((int64_t)(v13));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
  t17 = ((int64_t)(v13));
  t18 = ((int64_t)(v3));
  t19 = ((t17) < (t18));
  if (t19) {
  {
  t20 = v1;
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(v13));
  t23 = ((int64_t)((uint64_t)(t21) + (uint64_t)(t22)));
  t24 = ((gg_type_22 *)t20.data) + gg_index(t23, t20.len);
  v16 = (*t24);
  t25 = ((int64_t)(((int64_t)((v16).f22))));
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(((int64_t)gg_mod(t25, t26))));
  t28 = ((int64_t)(((int64_t)((v16).f23))));
  t29 = ((int64_t)(v4));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t30)))));
  t32 = ((int64_t)(v5));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t34 = ((int64_t)(v6));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
  t36 = ((int64_t)(v12));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  /* FG_DIRECT_SLICE_INDEX */
  t37 = v17;
  t38 = ((gg_type_15 *)(v0).data) + gg_index(t37, (v0).len);
  v18 = t38;
  /* FG_DIRECT_SLICE_INDEX */
  t39 = v15;
  t40 = ((float *)(((*(v10))).f32).data) + gg_index(t39, ((int64_t)UINT64_C(256)));
  (*t40) = ((float)(((*(v18))).f19));
  t42 = ((int64_t)UINT64_C(16));
  t41 = 0;
label6:;
  if (t41 >= t42) goto label8;
  v19 = t41;
  {
  t43 = ((int64_t)(v12));
  t44 = ((int64_t)(((int64_t)UINT64_C(16))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((int64_t)(v19));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  t48 = ((int64_t)(v11));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  t50 = ((int64_t)(v13));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t50)))));
  t51 = gg_view((((*(v10))).f31).data, 512);
  t52 = ((int64_t)(v20));
  t53 = ((int64_t)(((int64_t)UINT64_C(8))));
  t54 = ((int64_t)gg_div(t52, t53));
  t55 = ((gg_type_34 *)t51.data) + gg_index(t54, t51.len);
  t56 = gg_view(((*t55)).data, 8);
  t57 = ((int64_t)(v20));
  t58 = ((int64_t)(((int64_t)UINT64_C(8))));
  t59 = ((int64_t)gg_mod(t57, t58));
  t60 = ((int32_t *)t56.data) + gg_index(t59, t56.len);
  /* FG_DIRECT_SLICE_INDEX */
  t61 = v19;
  t62 = ((int16_t *)(((*(v18))).f21).data) + gg_index(t61, ((int64_t)UINT64_C(16)));
  (*t60) = ((int32_t)(((int32_t)((*t62)))));
  }
label7:;
  t41++; goto label6;
label8:;
  t64 = ((int64_t)UINT64_C(8));
  t63 = 0;
label9:;
  if (t63 >= t64) goto label11;
  v21 = t63;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t65 = v14;
  t66 = ((gg_type_18 *)(((*(v10))).f30).data) + gg_index(t65, ((int64_t)UINT64_C(256)));
  t67 = gg_view(((*t66)).data, 8);
  t68 = v21;
  t69 = ((gg_type_17 *)t67.data) + gg_index(t68, t67.len);
  t70 = v9;
  if (t70) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t71 = v21;
  t72 = ((gg_type_17 *)(((*(v18))).f20).data) + gg_index(t71, ((int64_t)UINT64_C(8)));
  t73 = (*t72);
  (*t69) = gg_function_6(t73, base + 0);
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t74 = v21;
  t75 = ((gg_type_17 *)(((*(v18))).f20).data) + gg_index(t74, ((int64_t)UINT64_C(8)));
  (*t69) = (*t75);
  }
  }
  }
label10:;
  t63++; goto label9;
label11:;
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t76 = v15;
  t77 = ((float *)(((*(v10))).f32).data) + gg_index(t76, ((int64_t)UINT64_C(256)));
  (*t77) = ((float)(gg_double_bits(UINT64_C(0))));
  t79 = ((int64_t)UINT64_C(16));
  t78 = 0;
label12:;
  if (t78 >= t79) goto label14;
  v22 = t78;
  {
  t80 = ((int64_t)(v12));
  t81 = ((int64_t)(((int64_t)UINT64_C(16))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  t83 = ((int64_t)(v22));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  t85 = ((int64_t)(v11));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))));
  t87 = ((int64_t)(v13));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))));
  t88 = gg_view((((*(v10))).f31).data, 512);
  t89 = ((int64_t)(v23));
  t90 = ((int64_t)(((int64_t)UINT64_C(8))));
  t91 = ((int64_t)gg_div(t89, t90));
  t92 = ((gg_type_34 *)t88.data) + gg_index(t91, t88.len);
  t93 = gg_view(((*t92)).data, 8);
  t94 = ((int64_t)(v23));
  t95 = ((int64_t)(((int64_t)UINT64_C(8))));
  t96 = ((int64_t)gg_mod(t94, t95));
  t97 = ((int32_t *)t93.data) + gg_index(t96, t93.len);
  (*t97) = ((int32_t)(((int64_t)UINT64_C(0))));
  }
label13:;
  t78++; goto label12;
label14:;
  t99 = ((int64_t)UINT64_C(8));
  t98 = 0;
label15:;
  if (t98 >= t99) goto label17;
  v24 = t98;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t100 = v14;
  t101 = ((gg_type_18 *)(((*(v10))).f30).data) + gg_index(t100, ((int64_t)UINT64_C(256)));
  t102 = gg_view(((*t101)).data, 8);
  t103 = v24;
  t104 = ((gg_type_17 *)t102.data) + gg_index(t103, t102.len);
  t105 = v9;
  if (t105) {
  {
  memset(&t107, 0, sizeof(t107));
  t108 = ((int8_t)(t107));
  for (t109=0; t109<32; t109++) (t106).data[t109] = t108;
  t110 = t106;
  (*t104) = gg_function_6(t110, base + 0);
  }
  } else {
  {
  memset(&t111, 0, sizeof(t111));
  t112 = ((int8_t)(t111));
  for (t113=0; t113<32; t113++) ((*t104)).data[t113] = t112;
  }
  }
  }
label16:;
  t98++; goto label15;
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
static void gg_function_12(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_type_44 p6, int64_t base) {
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
  uint8_t t16;
  gg_slice t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  gg_type_3 * t27;
  int64_t t28;
  float * t29;
  uint16_t t30;
  float t31;
  int64_t t32;
  float * t33;
  uint16_t t34;
  float t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  gg_type_30 * t39;
  gg_slice t40;
  int64_t t41;
  int32_t * t42;
  int64_t t43;
  uint8_t * t44;
  uint8_t t45;
  uint8_t t46;
  int64_t t47;
  gg_type_30 * t48;
  gg_slice t49;
  int64_t t50;
  int32_t * t51;
  gg_slice t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t * t56;
  uint8_t t57;
  uint8_t t58;
  int64_t t59;
  gg_type_30 * t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int32_t * t65;
  gg_slice t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t * t70;
  uint8_t t71;
  uint8_t t72;
  uint8_t t73;
  int64_t t74;
  uint8_t * t75;
  uint8_t t76;
  int64_t t77;
  uint8_t t78;
  int64_t t79;
  uint8_t t80;
  int64_t t81;
  gg_type_30 * t82;
  gg_slice t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int32_t * t87;
  gg_slice t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t * t92;
  uint8_t t93;
  int64_t t94;
  uint8_t t95;
  gg_slice t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t * t100;
  uint8_t t101;
  int64_t t102;
  uint8_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  gg_type_27 * t107;
  gg_slice t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  gg_type_7 * t112;
  int64_t t113;
  gg_type_7 * t114;
  gg_type_7 t115;
  uint8_t t116;
  int64_t t117;
  gg_type_27 * t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  gg_type_7 * t125;
  int64_t t126;
  gg_type_7 * t127;
  gg_type_7 t128;
  uint8_t t129;
  int64_t t130;
  float * t131;
  int64_t t132;
  float * t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  gg_type_30 * t137;
  gg_slice t138;
  int64_t t139;
  int32_t * t140;
  int64_t t141;
  gg_type_30 * t142;
  gg_slice t143;
  int64_t t144;
  int32_t * t145;
  int64_t t146;
  gg_type_27 * t147;
  gg_slice t148;
  int64_t t149;
  gg_type_7 * t150;
  uint8_t t151;
  uint8_t t152;
  int64_t t153;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_44 v6;
  gg_type_51 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  gg_type_53 v13;
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
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((int64_t)(v8));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  v12 = ((int64_t)(v11));
  t14 = ((int64_t)(v9));
  t15 = ((int64_t)(v4));
  t16 = ((t14) < (t15));
  if (t16) {
  {
  t17 = v0;
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(v9));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))));
  t25 = ((int64_t)(v8));
  t26 = ((int64_t)((uint64_t)(t24) + (uint64_t)(t25)));
  t27 = ((gg_type_3 *)t17.data) + gg_index(t26, t17.len);
  v13 = t27;
  /* FG_DIRECT_SLICE_INDEX */
  t28 = v10;
  t29 = ((float *)(((*(v7))).f25).data) + gg_index(t28, ((int64_t)UINT64_C(256)));
  t30 = ((uint16_t)(((*(v13))).f11));
  t31 = fg_tiled_half(t30);
  (*t29) = ((float)(t31));
  /* FG_DIRECT_SLICE_INDEX */
  t32 = v10;
  t33 = ((float *)(((*(v7))).f26).data) + gg_index(t32, ((int64_t)UINT64_C(256)));
  t34 = ((uint16_t)(((*(v13))).f12));
  t35 = fg_tiled_half(t34);
  (*t33) = ((float)(t35));
  t37 = ((int64_t)UINT64_C(4));
  t36 = 0;
label6:;
  if (t36 >= t37) goto label8;
  v14 = t36;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t38 = v11;
  t39 = ((gg_type_30 *)(((*(v7))).f27).data) + gg_index(t38, ((int64_t)UINT64_C(256)));
  t40 = gg_view(((*t39)).data, 16);
  t41 = v14;
  t42 = ((int32_t *)t40.data) + gg_index(t41, t40.len);
  /* FG_DIRECT_SLICE_INDEX */
  t43 = v14;
  t44 = ((uint8_t *)(((*(v13))).f13).data) + gg_index(t43, ((int64_t)UINT64_C(12)));
  t45 = ((uint8_t)((*t44)));
  t46 = ((uint8_t)(((int64_t)UINT64_C(63))));
  (*t42) = ((int32_t)(((int32_t)(((t45) & (t46))))));
  /* FG_DIRECT_SLICE_INDEX */
  t47 = v11;
  t48 = ((gg_type_30 *)(((*(v7))).f28).data) + gg_index(t47, ((int64_t)UINT64_C(256)));
  t49 = gg_view(((*t48)).data, 16);
  t50 = v14;
  t51 = ((int32_t *)t49.data) + gg_index(t50, t49.len);
  t52 = gg_view((((*(v13))).f13).data, 12);
  t53 = ((int64_t)(v14));
  t54 = ((int64_t)(((int64_t)UINT64_C(4))));
  t55 = ((int64_t)((uint64_t)(t53) + (uint64_t)(t54)));
  t56 = ((uint8_t *)t52.data) + gg_index(t55, t52.len);
  t57 = ((uint8_t)((*t56)));
  t58 = ((uint8_t)(((int64_t)UINT64_C(63))));
  (*t51) = ((int32_t)(((int32_t)(((t57) & (t58))))));
  /* FG_DIRECT_SLICE_INDEX */
  t59 = v11;
  t60 = ((gg_type_30 *)(((*(v7))).f27).data) + gg_index(t59, ((int64_t)UINT64_C(256)));
  t61 = gg_view(((*t60)).data, 16);
  t62 = ((int64_t)(v14));
  t63 = ((int64_t)(((int64_t)UINT64_C(4))));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((int32_t *)t61.data) + gg_index(t64, t61.len);
  t66 = gg_view((((*(v13))).f13).data, 12);
  t67 = ((int64_t)(v14));
  t68 = ((int64_t)(((int64_t)UINT64_C(8))));
  t69 = ((int64_t)((uint64_t)(t67) + (uint64_t)(t68)));
  t70 = ((uint8_t *)t66.data) + gg_index(t69, t66.len);
  t71 = ((uint8_t)((*t70)));
  t72 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t73 = ((uint8_t)(((t71) & (t72))));
  /* FG_DIRECT_SLICE_INDEX */
  t74 = v14;
  t75 = ((uint8_t *)(((*(v13))).f13).data) + gg_index(t74, ((int64_t)UINT64_C(12)));
  t76 = ((uint8_t)((*t75)));
  t77 = ((int64_t)(((int64_t)UINT64_C(6))));
  t78 = ((uint8_t)(((t76) >> ((uint64_t)(t77) & 7))));
  t79 = ((int64_t)(((int64_t)UINT64_C(4))));
  t80 = ((uint8_t)(((uint8_t)((uint64_t)(t78) << ((uint64_t)(t79) & 7)))));
  (*t65) = ((int32_t)(((int32_t)(((t73) | (t80))))));
  /* FG_DIRECT_SLICE_INDEX */
  t81 = v11;
  t82 = ((gg_type_30 *)(((*(v7))).f28).data) + gg_index(t81, ((int64_t)UINT64_C(256)));
  t83 = gg_view(((*t82)).data, 16);
  t84 = ((int64_t)(v14));
  t85 = ((int64_t)(((int64_t)UINT64_C(4))));
  t86 = ((int64_t)((uint64_t)(t84) + (uint64_t)(t85)));
  t87 = ((int32_t *)t83.data) + gg_index(t86, t83.len);
  t88 = gg_view((((*(v13))).f13).data, 12);
  t89 = ((int64_t)(v14));
  t90 = ((int64_t)(((int64_t)UINT64_C(8))));
  t91 = ((int64_t)((uint64_t)(t89) + (uint64_t)(t90)));
  t92 = ((uint8_t *)t88.data) + gg_index(t91, t88.len);
  t93 = ((uint8_t)((*t92)));
  t94 = ((int64_t)(((int64_t)UINT64_C(4))));
  t95 = ((uint8_t)(((t93) >> ((uint64_t)(t94) & 7))));
  t96 = gg_view((((*(v13))).f13).data, 12);
  t97 = ((int64_t)(v14));
  t98 = ((int64_t)(((int64_t)UINT64_C(4))));
  t99 = ((int64_t)((uint64_t)(t97) + (uint64_t)(t98)));
  t100 = ((uint8_t *)t96.data) + gg_index(t99, t96.len);
  t101 = ((uint8_t)((*t100)));
  t102 = ((int64_t)(((int64_t)UINT64_C(6))));
  t103 = ((uint8_t)(((t101) >> ((uint64_t)(t102) & 7))));
  t104 = ((int64_t)(((int64_t)UINT64_C(4))));
  t105 = ((uint8_t)(((uint8_t)((uint64_t)(t103) << ((uint64_t)(t104) & 7)))));
  (*t87) = ((int32_t)(((int32_t)(((t95) | (t105))))));
  /* FG_DIRECT_SLICE_INDEX */
  t106 = v12;
  t107 = ((gg_type_27 *)(((*(v7))).f24).data) + gg_index(t106, ((int64_t)UINT64_C(256)));
  t108 = gg_view(((*t107)).data, 8);
  t109 = ((int64_t)(((int64_t)UINT64_C(2))));
  t110 = ((int64_t)(v14));
  t111 = ((int64_t)((uint64_t)(t109) * (uint64_t)(t110)));
  t112 = ((gg_type_7 *)t108.data) + gg_index(t111, t108.len);
  /* FG_DIRECT_SLICE_INDEX */
  t113 = v14;
  t114 = ((gg_type_7 *)(((*(v13))).f14).data) + gg_index(t113, ((int64_t)UINT64_C(4)));
  t115 = (*t114);
  t116 = ((uint8_t)(((int64_t)UINT64_C(15))));
  (*t112) = gg_function_13(t115, t116, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t117 = v12;
  t118 = ((gg_type_27 *)(((*(v7))).f24).data) + gg_index(t117, ((int64_t)UINT64_C(256)));
  t119 = gg_view(((*t118)).data, 8);
  t120 = ((int64_t)(((int64_t)UINT64_C(2))));
  t121 = ((int64_t)(v14));
  t122 = ((int64_t)(((int64_t)((uint64_t)(t120) * (uint64_t)(t121)))));
  t123 = ((int64_t)(((int64_t)UINT64_C(1))));
  t124 = ((int64_t)((uint64_t)(t122) + (uint64_t)(t123)));
  t125 = ((gg_type_7 *)t119.data) + gg_index(t124, t119.len);
  /* FG_DIRECT_SLICE_INDEX */
  t126 = v14;
  t127 = ((gg_type_7 *)(((*(v13))).f14).data) + gg_index(t126, ((int64_t)UINT64_C(4)));
  t128 = (*t127);
  t129 = ((uint8_t)(((int64_t)UINT64_C(4))));
  (*t125) = gg_function_14(t128, t129, base + 0);
  }
label7:;
  t36++; goto label6;
label8:;
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t130 = v10;
  t131 = ((float *)(((*(v7))).f25).data) + gg_index(t130, ((int64_t)UINT64_C(256)));
  (*t131) = ((float)(gg_double_bits(UINT64_C(0))));
  /* FG_DIRECT_SLICE_INDEX */
  t132 = v10;
  t133 = ((float *)(((*(v7))).f26).data) + gg_index(t132, ((int64_t)UINT64_C(256)));
  (*t133) = ((float)(gg_double_bits(UINT64_C(0))));
  t135 = ((int64_t)UINT64_C(8));
  t134 = 0;
label9:;
  if (t134 >= t135) goto label11;
  v15 = t134;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t136 = v11;
  t137 = ((gg_type_30 *)(((*(v7))).f27).data) + gg_index(t136, ((int64_t)UINT64_C(256)));
  t138 = gg_view(((*t137)).data, 16);
  t139 = v15;
  t140 = ((int32_t *)t138.data) + gg_index(t139, t138.len);
  (*t140) = ((int32_t)(((int64_t)UINT64_C(0))));
  /* FG_DIRECT_SLICE_INDEX */
  t141 = v11;
  t142 = ((gg_type_30 *)(((*(v7))).f28).data) + gg_index(t141, ((int64_t)UINT64_C(256)));
  t143 = gg_view(((*t142)).data, 16);
  t144 = v15;
  t145 = ((int32_t *)t143.data) + gg_index(t144, t143.len);
  (*t145) = ((int32_t)(((int64_t)UINT64_C(0))));
  /* FG_DIRECT_SLICE_INDEX */
  t146 = v12;
  t147 = ((gg_type_27 *)(((*(v7))).f24).data) + gg_index(t146, ((int64_t)UINT64_C(256)));
  t148 = gg_view(((*t147)).data, 8);
  t149 = v15;
  t150 = ((gg_type_7 *)t148.data) + gg_index(t149, t148.len);
  memset(&t151, 0, sizeof(t151));
  t152 = ((uint8_t)(t151));
  for (t153=0; t153<32; t153++) ((*t150)).data[t153] = t152;
  }
label10:;
  t134++; goto label9;
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
static void gg_function_7(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_type_44 p6, int64_t base) {
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
  float * t23;
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
  gg_type_10 * t37;
  int64_t t38;
  float * t39;
  uint16_t t40;
  float t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  gg_type_30 * t45;
  gg_slice t46;
  int64_t t47;
  int32_t * t48;
  int64_t t49;
  int8_t * t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  gg_type_7 * t56;
  gg_slice t57;
  int64_t t58;
  uint8_t * t59;
  gg_slice t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  gg_type_7 * t64;
  gg_slice t65;
  int64_t t66;
  uint8_t * t67;
  gg_slice t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  gg_type_7 * t74;
  gg_slice t75;
  int64_t t76;
  uint8_t * t77;
  int64_t t78;
  gg_type_27 * t79;
  gg_slice t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  gg_type_7 * t84;
  gg_slice t85;
  int64_t t86;
  uint8_t * t87;
  uint8_t t88;
  uint8_t t89;
  uint8_t t90;
  uint8_t t91;
  uint8_t t92;
  uint8_t t93;
  int64_t t94;
  uint8_t t95;
  int64_t t96;
  gg_type_27 * t97;
  gg_slice t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  gg_type_7 * t104;
  gg_slice t105;
  int64_t t106;
  uint8_t * t107;
  uint8_t t108;
  uint8_t t109;
  uint8_t t110;
  uint8_t t111;
  uint8_t t112;
  uint8_t t113;
  int64_t t114;
  uint8_t t115;
  int64_t t116;
  gg_type_27 * t117;
  gg_slice t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  gg_type_7 * t124;
  gg_slice t125;
  int64_t t126;
  uint8_t * t127;
  uint8_t t128;
  int64_t t129;
  uint8_t t130;
  uint8_t t131;
  uint8_t t132;
  uint8_t t133;
  int64_t t134;
  gg_type_27 * t135;
  gg_slice t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  gg_type_7 * t142;
  gg_slice t143;
  int64_t t144;
  uint8_t * t145;
  uint8_t t146;
  int64_t t147;
  uint8_t t148;
  uint8_t t149;
  uint8_t t150;
  uint8_t t151;
  int64_t t152;
  uint8_t t153;
  int64_t t154;
  int64_t t155;
  gg_type_20 t156;
  int64_t t157;
  gg_type_27 * t158;
  gg_slice t159;
  int64_t t160;
  gg_type_7 * t161;
  gg_type_7 t162;
  gg_type_17 t163;
  gg_type_20 t164;
  int64_t t165;
  int32_t * t166;
  int32_t t167;
  int64_t t168;
  int32_t * t169;
  int32_t t170;
  int32_t t171;
  int64_t t172;
  int32_t * t173;
  int32_t t174;
  int64_t t175;
  int32_t * t176;
  int32_t t177;
  int32_t t178;
  int64_t t179;
  int32_t * t180;
  int32_t t181;
  int64_t t182;
  int32_t * t183;
  int32_t t184;
  int32_t t185;
  int64_t t186;
  int32_t * t187;
  int32_t t188;
  int64_t t189;
  int32_t * t190;
  int32_t t191;
  int32_t t192;
  int32_t t193;
  int32_t t194;
  int64_t t195;
  gg_type_30 * t196;
  gg_slice t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int32_t * t201;
  int32_t t202;
  int32_t t203;
  int32_t t204;
  int32_t t205;
  int32_t t206;
  int32_t t207;
  int32_t t208;
  int64_t t209;
  gg_type_30 * t210;
  gg_slice t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int32_t * t217;
  int32_t t218;
  int32_t t219;
  int32_t t220;
  int32_t t221;
  int32_t t222;
  int32_t t223;
  int32_t t224;
  int64_t t225;
  gg_type_30 * t226;
  gg_slice t227;
  int64_t t228;
  int32_t * t229;
  int64_t t230;
  float * t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  gg_type_30 * t235;
  gg_slice t236;
  int64_t t237;
  int32_t * t238;
  int64_t t239;
  gg_type_30 * t240;
  gg_slice t241;
  int64_t t242;
  int32_t * t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  gg_type_27 * t247;
  gg_slice t248;
  int64_t t249;
  gg_type_7 * t250;
  uint8_t t251;
  uint8_t t252;
  int64_t t253;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_44 v6;
  gg_type_51 v7;
  gg_type_17 v8;
  int64_t v9;
  gg_type_20 v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  gg_type_52 v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  uint8_t v20;
  uint8_t v21;
  uint8_t v22;
  int32_t v23;
  int64_t v24;
  gg_type_34 v25;
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
  t19 = ((int64_t)(v5));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t21 = ((int64_t)(v11));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t21)))));
  v15 = ((int64_t)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t22 = v13;
  t23 = ((float *)(((*(v7))).f26).data) + gg_index(t22, ((int64_t)UINT64_C(256)));
  (*t23) = ((float)(gg_double_bits(UINT64_C(0))));
  t24 = ((int64_t)(v12));
  t25 = ((int64_t)(v4));
  t26 = ((t24) < (t25));
  if (t26) {
  {
  t27 = v0;
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v12));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = ((int64_t)(v11));
  t36 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  t37 = ((gg_type_10 *)t27.data) + gg_index(t36, t27.len);
  v16 = t37;
  /* FG_DIRECT_SLICE_INDEX */
  t38 = v13;
  t39 = ((float *)(((*(v7))).f25).data) + gg_index(t38, ((int64_t)UINT64_C(256)));
  t40 = ((uint16_t)(((*(v16))).f18));
  t41 = fg_tiled_half(t40);
  (*t39) = ((float)(t41));
  t43 = ((int64_t)UINT64_C(16));
  t42 = 0;
label9:;
  if (t42 >= t43) goto label11;
  v17 = t42;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t44 = v14;
  t45 = ((gg_type_30 *)(((*(v7))).f27).data) + gg_index(t44, ((int64_t)UINT64_C(256)));
  t46 = gg_view(((*t45)).data, 16);
  t47 = v17;
  t48 = ((int32_t *)t46.data) + gg_index(t47, t46.len);
  /* FG_DIRECT_SLICE_INDEX */
  t49 = v17;
  t50 = ((int8_t *)(((*(v16))).f17).data) + gg_index(t49, ((int64_t)UINT64_C(16)));
  (*t48) = ((int32_t)(((int32_t)((*t50)))));
  }
label10:;
  t42++; goto label9;
label11:;
  t52 = ((int64_t)UINT64_C(2));
  t51 = 0;
label12:;
  if (t51 >= t52) goto label14;
  v18 = t51;
  {
  t54 = ((int64_t)UINT64_C(32));
  t53 = 0;
label15:;
  if (t53 >= t54) goto label17;
  v19 = t53;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t55 = v18;
  t56 = ((gg_type_7 *)(((*(v16))).f16).data) + gg_index(t55, ((int64_t)UINT64_C(2)));
  t57 = gg_view(((*t56)).data, 32);
  t58 = v19;
  t59 = ((uint8_t *)t57.data) + gg_index(t58, t57.len);
  v20 = ((uint8_t)((*t59)));
  t60 = gg_view((((*(v16))).f15).data, 4);
  t61 = ((int64_t)(((int64_t)UINT64_C(2))));
  t62 = ((int64_t)(v18));
  t63 = ((int64_t)((uint64_t)(t61) * (uint64_t)(t62)));
  t64 = ((gg_type_7 *)t60.data) + gg_index(t63, t60.len);
  t65 = gg_view(((*t64)).data, 32);
  t66 = v19;
  t67 = ((uint8_t *)t65.data) + gg_index(t66, t65.len);
  v21 = ((uint8_t)((*t67)));
  t68 = gg_view((((*(v16))).f15).data, 4);
  t69 = ((int64_t)(((int64_t)UINT64_C(2))));
  t70 = ((int64_t)(v18));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((int64_t)(((int64_t)UINT64_C(1))));
  t73 = ((int64_t)((uint64_t)(t71) + (uint64_t)(t72)));
  t74 = ((gg_type_7 *)t68.data) + gg_index(t73, t68.len);
  t75 = gg_view(((*t74)).data, 32);
  t76 = v19;
  t77 = ((uint8_t *)t75.data) + gg_index(t76, t75.len);
  v22 = ((uint8_t)((*t77)));
  /* FG_DIRECT_SLICE_INDEX */
  t78 = v15;
  t79 = ((gg_type_27 *)(((*(v7))).f24).data) + gg_index(t78, ((int64_t)UINT64_C(256)));
  t80 = gg_view(((*t79)).data, 8);
  t81 = ((int64_t)(((int64_t)UINT64_C(4))));
  t82 = ((int64_t)(v18));
  t83 = ((int64_t)((uint64_t)(t81) * (uint64_t)(t82)));
  t84 = ((gg_type_7 *)t80.data) + gg_index(t83, t80.len);
  t85 = gg_view(((*t84)).data, 32);
  t86 = v19;
  t87 = ((uint8_t *)t85.data) + gg_index(t86, t85.len);
  t88 = ((uint8_t)(v21));
  t89 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t90 = ((uint8_t)(((t88) & (t89))));
  t91 = ((uint8_t)(v20));
  t92 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t93 = ((uint8_t)(((t91) & (t92))));
  t94 = ((int64_t)(((int64_t)UINT64_C(4))));
  t95 = ((uint8_t)(((uint8_t)((uint64_t)(t93) << ((uint64_t)(t94) & 7)))));
  (*t87) = ((uint8_t)(((t90) | (t95))));
  /* FG_DIRECT_SLICE_INDEX */
  t96 = v15;
  t97 = ((gg_type_27 *)(((*(v7))).f24).data) + gg_index(t96, ((int64_t)UINT64_C(256)));
  t98 = gg_view(((*t97)).data, 8);
  t99 = ((int64_t)(((int64_t)UINT64_C(4))));
  t100 = ((int64_t)(v18));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t100)))));
  t102 = ((int64_t)(((int64_t)UINT64_C(1))));
  t103 = ((int64_t)((uint64_t)(t101) + (uint64_t)(t102)));
  t104 = ((gg_type_7 *)t98.data) + gg_index(t103, t98.len);
  t105 = gg_view(((*t104)).data, 32);
  t106 = v19;
  t107 = ((uint8_t *)t105.data) + gg_index(t106, t105.len);
  t108 = ((uint8_t)(v22));
  t109 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t110 = ((uint8_t)(((t108) & (t109))));
  t111 = ((uint8_t)(v20));
  t112 = ((uint8_t)(((int64_t)UINT64_C(12))));
  t113 = ((uint8_t)(((t111) & (t112))));
  t114 = ((int64_t)(((int64_t)UINT64_C(2))));
  t115 = ((uint8_t)(((uint8_t)((uint64_t)(t113) << ((uint64_t)(t114) & 7)))));
  (*t107) = ((uint8_t)(((t110) | (t115))));
  /* FG_DIRECT_SLICE_INDEX */
  t116 = v15;
  t117 = ((gg_type_27 *)(((*(v7))).f24).data) + gg_index(t116, ((int64_t)UINT64_C(256)));
  t118 = gg_view(((*t117)).data, 8);
  t119 = ((int64_t)(((int64_t)UINT64_C(4))));
  t120 = ((int64_t)(v18));
  t121 = ((int64_t)(((int64_t)((uint64_t)(t119) * (uint64_t)(t120)))));
  t122 = ((int64_t)(((int64_t)UINT64_C(2))));
  t123 = ((int64_t)((uint64_t)(t121) + (uint64_t)(t122)));
  t124 = ((gg_type_7 *)t118.data) + gg_index(t123, t118.len);
  t125 = gg_view(((*t124)).data, 32);
  t126 = v19;
  t127 = ((uint8_t *)t125.data) + gg_index(t126, t125.len);
  t128 = ((uint8_t)(v21));
  t129 = ((int64_t)(((int64_t)UINT64_C(4))));
  t130 = ((uint8_t)(((t128) >> ((uint64_t)(t129) & 7))));
  t131 = ((uint8_t)(v20));
  t132 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t133 = ((uint8_t)(((t131) & (t132))));
  (*t127) = ((uint8_t)(((t130) | (t133))));
  /* FG_DIRECT_SLICE_INDEX */
  t134 = v15;
  t135 = ((gg_type_27 *)(((*(v7))).f24).data) + gg_index(t134, ((int64_t)UINT64_C(256)));
  t136 = gg_view(((*t135)).data, 8);
  t137 = ((int64_t)(((int64_t)UINT64_C(4))));
  t138 = ((int64_t)(v18));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t137) * (uint64_t)(t138)))));
  t140 = ((int64_t)(((int64_t)UINT64_C(3))));
  t141 = ((int64_t)((uint64_t)(t139) + (uint64_t)(t140)));
  t142 = ((gg_type_7 *)t136.data) + gg_index(t141, t136.len);
  t143 = gg_view(((*t142)).data, 32);
  t144 = v19;
  t145 = ((uint8_t *)t143.data) + gg_index(t144, t143.len);
  t146 = ((uint8_t)(v22));
  t147 = ((int64_t)(((int64_t)UINT64_C(4))));
  t148 = ((uint8_t)(((t146) >> ((uint64_t)(t147) & 7))));
  t149 = ((uint8_t)(v20));
  t150 = ((uint8_t)(((int64_t)UINT64_C(192))));
  t151 = ((uint8_t)(((t149) & (t150))));
  t152 = ((int64_t)(((int64_t)UINT64_C(2))));
  t153 = ((uint8_t)(((t151) >> ((uint64_t)(t152) & 7))));
  (*t145) = ((uint8_t)(((t148) | (t153))));
  }
label16:;
  t53++; goto label15;
label17:;
  }
label13:;
  t51++; goto label12;
label14:;
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
  t155 = ((int64_t)UINT64_C(8));
  t154 = 0;
label18:;
  if (t154 >= t155) goto label20;
  v24 = t154;
  {
  t156 = v10;
  /* FG_DIRECT_SLICE_INDEX */
  t157 = v15;
  t158 = ((gg_type_27 *)(((*(v7))).f24).data) + gg_index(t157, ((int64_t)UINT64_C(256)));
  t159 = gg_view(((*t158)).data, 8);
  t160 = v24;
  t161 = ((gg_type_7 *)t159.data) + gg_index(t160, t159.len);
  t162 = (*t161);
  t163 = v8;
  t164 = gg_function_10(t162, t163, base + 0);
  v25 = gg_function_11(t156, t164, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t165 = ((int64_t)UINT64_C(0));
  t166 = ((int32_t *)(v25).data) + gg_index(t165, ((int64_t)UINT64_C(8)));
  t167 = ((int32_t)((*t166)));
  /* FG_DIRECT_SLICE_INDEX */
  t168 = ((int64_t)UINT64_C(1));
  t169 = ((int32_t *)(v25).data) + gg_index(t168, ((int64_t)UINT64_C(8)));
  t170 = ((int32_t)((*t169)));
  t171 = ((int32_t)(((int32_t)((uint64_t)(t167) + (uint64_t)(t170)))));
  /* FG_DIRECT_SLICE_INDEX */
  t172 = ((int64_t)UINT64_C(2));
  t173 = ((int32_t *)(v25).data) + gg_index(t172, ((int64_t)UINT64_C(8)));
  t174 = ((int32_t)((*t173)));
  /* FG_DIRECT_SLICE_INDEX */
  t175 = ((int64_t)UINT64_C(3));
  t176 = ((int32_t *)(v25).data) + gg_index(t175, ((int64_t)UINT64_C(8)));
  t177 = ((int32_t)((*t176)));
  t178 = ((int32_t)(((int32_t)((uint64_t)(t174) + (uint64_t)(t177)))));
  v26 = ((int32_t)(((int32_t)((uint64_t)(t171) + (uint64_t)(t178)))));
  /* FG_DIRECT_SLICE_INDEX */
  t179 = ((int64_t)UINT64_C(4));
  t180 = ((int32_t *)(v25).data) + gg_index(t179, ((int64_t)UINT64_C(8)));
  t181 = ((int32_t)((*t180)));
  /* FG_DIRECT_SLICE_INDEX */
  t182 = ((int64_t)UINT64_C(5));
  t183 = ((int32_t *)(v25).data) + gg_index(t182, ((int64_t)UINT64_C(8)));
  t184 = ((int32_t)((*t183)));
  t185 = ((int32_t)(((int32_t)((uint64_t)(t181) + (uint64_t)(t184)))));
  /* FG_DIRECT_SLICE_INDEX */
  t186 = ((int64_t)UINT64_C(6));
  t187 = ((int32_t *)(v25).data) + gg_index(t186, ((int64_t)UINT64_C(8)));
  t188 = ((int32_t)((*t187)));
  /* FG_DIRECT_SLICE_INDEX */
  t189 = ((int64_t)UINT64_C(7));
  t190 = ((int32_t *)(v25).data) + gg_index(t189, ((int64_t)UINT64_C(8)));
  t191 = ((int32_t)((*t190)));
  t192 = ((int32_t)(((int32_t)((uint64_t)(t188) + (uint64_t)(t191)))));
  v27 = ((int32_t)(((int32_t)((uint64_t)(t185) + (uint64_t)(t192)))));
  t193 = v23;
  t194 = ((int32_t)(((int64_t)UINT64_C(128))));
  /* FG_DIRECT_SLICE_INDEX */
  t195 = v14;
  t196 = ((gg_type_30 *)(((*(v7))).f27).data) + gg_index(t195, ((int64_t)UINT64_C(256)));
  t197 = gg_view(((*t196)).data, 16);
  t198 = ((int64_t)(((int64_t)UINT64_C(2))));
  t199 = ((int64_t)(v24));
  t200 = ((int64_t)((uint64_t)(t198) * (uint64_t)(t199)));
  t201 = ((int32_t *)t197.data) + gg_index(t200, t197.len);
  t202 = ((int32_t)((*t201)));
  t203 = ((int32_t)(((int32_t)((uint64_t)(t194) * (uint64_t)(t202)))));
  t204 = ((int32_t)(v26));
  t205 = ((int32_t)(((int64_t)UINT64_C(512))));
  t206 = ((int32_t)(((int32_t)((uint64_t)(t204) - (uint64_t)(t205)))));
  t207 = ((int32_t)(((int32_t)((uint64_t)(t203) * (uint64_t)(t206)))));
  t208 = ((int32_t)(((int64_t)UINT64_C(128))));
  /* FG_DIRECT_SLICE_INDEX */
  t209 = v14;
  t210 = ((gg_type_30 *)(((*(v7))).f27).data) + gg_index(t209, ((int64_t)UINT64_C(256)));
  t211 = gg_view(((*t210)).data, 16);
  t212 = ((int64_t)(((int64_t)UINT64_C(2))));
  t213 = ((int64_t)(v24));
  t214 = ((int64_t)(((int64_t)((uint64_t)(t212) * (uint64_t)(t213)))));
  t215 = ((int64_t)(((int64_t)UINT64_C(1))));
  t216 = ((int64_t)((uint64_t)(t214) + (uint64_t)(t215)));
  t217 = ((int32_t *)t211.data) + gg_index(t216, t211.len);
  t218 = ((int32_t)((*t217)));
  t219 = ((int32_t)(((int32_t)((uint64_t)(t208) * (uint64_t)(t218)))));
  t220 = ((int32_t)(v27));
  t221 = ((int32_t)(((int64_t)UINT64_C(512))));
  t222 = ((int32_t)(((int32_t)((uint64_t)(t220) - (uint64_t)(t221)))));
  t223 = ((int32_t)(((int32_t)((uint64_t)(t219) * (uint64_t)(t222)))));
  t224 = ((int32_t)(((int32_t)((uint64_t)(t207) + (uint64_t)(t223)))));
  v23 = ((int32_t)((uint64_t)(t193) + (uint64_t)(t224)));
  }
label19:;
  t154++; goto label18;
label20:;
  /* FG_DIRECT_SLICE_INDEX */
  t225 = v14;
  t226 = ((gg_type_30 *)(((*(v7))).f28).data) + gg_index(t225, ((int64_t)UINT64_C(256)));
  t227 = gg_view(((*t226)).data, 16);
  t228 = ((int64_t)UINT64_C(0));
  t229 = ((int32_t *)t227.data) + gg_index(t228, t227.len);
  (*t229) = ((int32_t)(v23));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t230 = v13;
  t231 = ((float *)(((*(v7))).f25).data) + gg_index(t230, ((int64_t)UINT64_C(256)));
  (*t231) = ((float)(gg_double_bits(UINT64_C(0))));
  t233 = ((int64_t)UINT64_C(16));
  t232 = 0;
label21:;
  if (t232 >= t233) goto label23;
  v28 = t232;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t234 = v14;
  t235 = ((gg_type_30 *)(((*(v7))).f27).data) + gg_index(t234, ((int64_t)UINT64_C(256)));
  t236 = gg_view(((*t235)).data, 16);
  t237 = v28;
  t238 = ((int32_t *)t236.data) + gg_index(t237, t236.len);
  (*t238) = ((int32_t)(((int64_t)UINT64_C(0))));
  /* FG_DIRECT_SLICE_INDEX */
  t239 = v14;
  t240 = ((gg_type_30 *)(((*(v7))).f28).data) + gg_index(t239, ((int64_t)UINT64_C(256)));
  t241 = gg_view(((*t240)).data, 16);
  t242 = v28;
  t243 = ((int32_t *)t241.data) + gg_index(t242, t241.len);
  (*t243) = ((int32_t)(((int64_t)UINT64_C(0))));
  }
label22:;
  t232++; goto label21;
label23:;
  t245 = ((int64_t)UINT64_C(8));
  t244 = 0;
label24:;
  if (t244 >= t245) goto label26;
  v29 = t244;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t246 = v15;
  t247 = ((gg_type_27 *)(((*(v7))).f24).data) + gg_index(t246, ((int64_t)UINT64_C(256)));
  t248 = gg_view(((*t247)).data, 8);
  t249 = v29;
  t250 = ((gg_type_7 *)t248.data) + gg_index(t249, t248.len);
  memset(&t251, 0, sizeof(t251));
  t252 = ((uint8_t)(t251));
  for (t253=0; t253<32; t253++) ((*t250)).data[t253] = t252;
  }
label25:;
  t244++; goto label24;
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
static uint8_t gg_function_1(gg_type_39 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_type_44 p5, gg_slice p6, int64_t base) {
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
  gg_type_36 * t92;
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
  gg_type_44 t117;
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
  gg_type_44 t132;
  gg_slice t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  gg_type_44 t139;
  int64_t t140;
  int64_t t141;
  uint8_t t142;
  gg_type_44 t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  gg_type_44 t151;
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
  gg_type_44 t172;
  gg_slice t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  gg_type_44 t179;
  gg_slice t180;
  gg_slice t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  gg_type_44 t188;
  uint8_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  uint8_t t194;
  gg_type_44 t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  gg_type_44 t201;
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
  gg_type_36 * t241;
  gg_slice t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  float * t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  gg_type_39 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_type_44 v5;
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
  t92 = ((gg_type_36 *)t88.data) + gg_index(t91, t88.len);
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
  gg_function_27(t151, t152, t153, t156, t157, t158, base + 0);
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
  gg_function_27(t201, t202, t203, t204, t205, t206, base + 0);
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
  t241 = ((gg_type_36 *)t237.data) + gg_index(t240, t237.len);
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
  gg_type_27 t62;
  gg_type_7 t63;
  uint8_t t64;
  uint8_t t65;
  int64_t t66;
  gg_type_7 t67;
  int64_t t68;
  gg_type_27 t69;
  int64_t t70;
  float t71;
  float t72;
  int64_t t73;
  float t74;
  float t75;
  int64_t t76;
  gg_type_30 t77;
  int32_t t78;
  int32_t t79;
  int64_t t80;
  gg_type_30 t81;
  int64_t t82;
  gg_type_30 t83;
  int32_t t84;
  int32_t t85;
  int64_t t86;
  gg_type_30 t87;
  int64_t t88;
  gg_type_18 t89;
  gg_type_17 t90;
  int8_t t91;
  int8_t t92;
  int64_t t93;
  gg_type_17 t94;
  int64_t t95;
  gg_type_18 t96;
  int64_t t97;
  gg_type_34 t98;
  int32_t t99;
  int32_t t100;
  int64_t t101;
  gg_type_34 t102;
  int64_t t103;
  float t104;
  float t105;
  int64_t t106;
  gg_type_36 t107;
  float t108;
  float t109;
  int64_t t110;
  gg_type_36 t111;
  int64_t t112;
  float t113;
  float t114;
  int64_t t115;
  gg_header t116;
  gg_type_39 t117;
  gg_slice t118;
  gg_slice t119;
  gg_slice t120;
  gg_slice t121;
  gg_type_44 t122;
  gg_slice t123;
  uint8_t t124;
  int64_t t125;
  gg_type_1 v0;
  gg_type_9 v1;
  gg_type_14 v2;
  gg_type_21 v3;
  gg_type_24 v4;
  gg_type_25 v5;
  gg_type_38 v6;
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
  memset(&t64, 0, sizeof(t64));
  t65 = ((uint8_t)(t64));
  for (t66=0; t66<32; t66++) (t63).data[t66] = t65;
  t67 = t63;
  for (t68=0; t68<8; t68++) (t62).data[t68] = t67;
  t69 = t62;
  for (t70=0; t70<256; t70++) (((v5).f29).f24).data[t70] = t69;
  memset(&t71, 0, sizeof(t71));
  t72 = ((float)(t71));
  for (t73=0; t73<256; t73++) (((v5).f29).f25).data[t73] = t72;
  memset(&t74, 0, sizeof(t74));
  t75 = ((float)(t74));
  for (t76=0; t76<256; t76++) (((v5).f29).f26).data[t76] = t75;
  memset(&t78, 0, sizeof(t78));
  t79 = ((int32_t)(t78));
  for (t80=0; t80<16; t80++) (t77).data[t80] = t79;
  t81 = t77;
  for (t82=0; t82<256; t82++) (((v5).f29).f27).data[t82] = t81;
  memset(&t84, 0, sizeof(t84));
  t85 = ((int32_t)(t84));
  for (t86=0; t86<16; t86++) (t83).data[t86] = t85;
  t87 = t83;
  for (t88=0; t88<256; t88++) (((v5).f29).f28).data[t88] = t87;
  memset(&((v5).f33), 0, sizeof((v5).f33));
  memset(&t91, 0, sizeof(t91));
  t92 = ((int8_t)(t91));
  for (t93=0; t93<32; t93++) (t90).data[t93] = t92;
  t94 = t90;
  for (t95=0; t95<8; t95++) (t89).data[t95] = t94;
  t96 = t89;
  for (t97=0; t97<256; t97++) (((v5).f33).f30).data[t97] = t96;
  memset(&t99, 0, sizeof(t99));
  t100 = ((int32_t)(t99));
  for (t101=0; t101<8; t101++) (t98).data[t101] = t100;
  t102 = t98;
  for (t103=0; t103<512; t103++) (((v5).f33).f31).data[t103] = t102;
  memset(&t104, 0, sizeof(t104));
  t105 = ((float)(t104));
  for (t106=0; t106<256; t106++) (((v5).f33).f32).data[t106] = t105;
  memset(&t108, 0, sizeof(t108));
  t109 = ((float)(t108));
  for (t110=0; t110<8; t110++) (t107).data[t110] = t109;
  t111 = t107;
  for (t112=0; t112<8192; t112++) ((v5).f34).data[t112] = t111;
  memset(&t113, 0, sizeof(t113));
  t114 = ((float)(t113));
  for (t115=0; t115<1; t115++) (v6).data[t115] = t114;
  t116.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t116.len = 0;
  t117 = &(v0);
  t118 = gg_view((v1).data, 1);
  t119 = gg_view((v2).data, 1);
  t120 = gg_view((v3).data, 1);
  t121 = gg_view((v4).data, 1);
  t122 = &(v5);
  t123 = gg_view((v6).data, 1);
  t124 = gg_function_1(t117, t118, t119, t120, t121, t122, t123, base + 1);
  gg_bool(gg_ref(&t116, gg_get_stack(base + 0)), t124);
  gg_append(gg_ref(&t116, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t116).base, (t116).len));
  }
gg_done:;
  for (t125 = 0; t125 < 1; t125++) gg_stacks[base + t125].top = 0;
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
