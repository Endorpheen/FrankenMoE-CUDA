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
  int64_t f16;
  int64_t f17;
  int64_t f18;
  int64_t f19;
  int64_t f20;
  int64_t f21;
  int64_t f22;
  int64_t f23;
  int64_t f24;
  int64_t f25;
  int64_t f26;
  int64_t f27;
  int64_t f28;
  int64_t f29;
  int64_t f30;
  int64_t f31;
  int64_t f32;
  int64_t f33;
  int64_t f34;
  int64_t f35;
  int64_t f36;
  int64_t f37;
  int64_t f38;
  int64_t f39;
  int64_t f40;
  int64_t f41;
  int64_t f42;
  int64_t f43;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[1]; } gg_type_4;
#pragma pack(pop)
typedef struct gg_type_5 gg_type_5;
#pragma pack(push, 1)
typedef struct { int8_t data[32]; } gg_type_8;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_8 data[8]; } gg_type_9;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int16_t data[16]; } gg_type_11;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_5 {
  float f44;
  gg_type_9 f45;
  gg_type_11 f46;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_5 data[1]; } gg_type_12;
#pragma pack(pop)
typedef struct gg_type_13 gg_type_13;
#pragma pack(push, 1)
struct gg_type_13 {
  int32_t f47;
  int32_t f48;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_13 data[1]; } gg_type_15;
#pragma pack(pop)
typedef struct gg_type_16 gg_type_16;
typedef struct gg_type_17 gg_type_17;
#pragma pack(push, 1)
typedef struct { uint8_t data[32]; } gg_type_18;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_18 data[8]; } gg_type_19;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_19 data[256]; } gg_type_20;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[256]; } gg_type_21;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_22;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_22 data[512]; } gg_type_23;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_17 {
  gg_type_20 f49;
  gg_type_21 f50;
  gg_type_21 f51;
  gg_type_23 f52;
  gg_type_23 f53;
};
#pragma pack(pop)
typedef struct gg_type_24 gg_type_24;
#pragma pack(push, 1)
typedef struct { gg_type_9 data[256]; } gg_type_25;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_24 {
  gg_type_25 f55;
  gg_type_23 f56;
  gg_type_21 f57;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_26;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_26 data[8192]; } gg_type_27;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_16 {
  gg_type_17 f54;
  gg_type_24 f58;
  gg_type_27 f59;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[8]; } gg_type_28;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_28 data[1]; } gg_type_29;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[4]; } gg_type_30;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_30 data[1]; } gg_type_31;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int8_t data[8]; } gg_type_32;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_32 data[1]; } gg_type_33;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[128]; } gg_type_34;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int8_t data[16]; } gg_type_35;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[1]; } gg_type_36;
#pragma pack(pop)
typedef gg_type_1 *gg_type_37;
typedef gg_type_16 *gg_type_41;
typedef struct gg_type_49 gg_type_49;
#pragma pack(push, 1)
struct gg_type_49 {
  int64_t f60;
  int64_t f61;
  int64_t f62;
  int64_t f63;
  int64_t f64;
};
#pragma pack(pop)
typedef gg_type_17 *gg_type_50;
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_54;
#pragma pack(pop)
typedef gg_type_24 *gg_type_55;
typedef gg_type_5 *gg_type_56;
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_57;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, gg_type_1 value);
static uint8_t gg_eq_2(int64_t a, int64_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, int64_t value);
static uint8_t gg_eq_3(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint8_t value);
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_type_4 value);
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_type_5 value);
static uint8_t gg_eq_6(float a, float b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, float value);
static uint8_t gg_eq_7(int8_t a, int8_t b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, int8_t value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, gg_type_9 value);
static uint8_t gg_eq_10(int16_t a, int16_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, int16_t value);
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, gg_type_11 value);
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_type_12 value);
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_type_13 value);
static uint8_t gg_eq_14(int32_t a, int32_t b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, int32_t value);
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
static uint8_t gg_eq_41(gg_type_41 a, gg_type_41 b);
static uint8_t gg_equal_elements_41(gg_slice a, gg_slice b);
static void gg_render_41(gg_reference out, gg_type_41 value);
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
static uint8_t gg_eq_47(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_47(gg_slice a, gg_slice b);
static void gg_render_47(gg_reference out, uint8_t value);
static uint8_t gg_eq_49(gg_type_49 a, gg_type_49 b);
static uint8_t gg_equal_elements_49(gg_slice a, gg_slice b);
static void gg_render_49(gg_reference out, gg_type_49 value);
static uint8_t gg_eq_50(gg_type_50 a, gg_type_50 b);
static uint8_t gg_equal_elements_50(gg_slice a, gg_slice b);
static void gg_render_50(gg_reference out, gg_type_50 value);
static uint8_t gg_eq_51(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_51(gg_slice a, gg_slice b);
static void gg_render_51(gg_reference out, uint16_t value);
static uint8_t gg_eq_52(double a, double b);
static uint8_t gg_equal_elements_52(gg_slice a, gg_slice b);
static void gg_render_52(gg_reference out, double value);
static uint8_t gg_eq_53(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_53(gg_slice a, gg_slice b);
static void gg_render_53(gg_reference out, uint32_t value);
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
if(!gg_eq_2(a.f16,b.f16))return 0;
if(!gg_eq_2(a.f17,b.f17))return 0;
if(!gg_eq_2(a.f18,b.f18))return 0;
if(!gg_eq_2(a.f19,b.f19))return 0;
if(!gg_eq_2(a.f20,b.f20))return 0;
if(!gg_eq_2(a.f21,b.f21))return 0;
if(!gg_eq_2(a.f22,b.f22))return 0;
if(!gg_eq_2(a.f23,b.f23))return 0;
if(!gg_eq_2(a.f24,b.f24))return 0;
if(!gg_eq_2(a.f25,b.f25))return 0;
if(!gg_eq_2(a.f26,b.f26))return 0;
if(!gg_eq_2(a.f27,b.f27))return 0;
if(!gg_eq_2(a.f28,b.f28))return 0;
if(!gg_eq_2(a.f29,b.f29))return 0;
if(!gg_eq_2(a.f30,b.f30))return 0;
if(!gg_eq_2(a.f31,b.f31))return 0;
if(!gg_eq_2(a.f32,b.f32))return 0;
if(!gg_eq_2(a.f33,b.f33))return 0;
if(!gg_eq_2(a.f34,b.f34))return 0;
if(!gg_eq_2(a.f35,b.f35))return 0;
if(!gg_eq_2(a.f36,b.f36))return 0;
if(!gg_eq_2(a.f37,b.f37))return 0;
if(!gg_eq_2(a.f38,b.f38))return 0;
if(!gg_eq_2(a.f39,b.f39))return 0;
if(!gg_eq_2(a.f40,b.f40))return 0;
if(!gg_eq_2(a.f41,b.f41))return 0;
if(!gg_eq_2(a.f42,b.f42))return 0;
if(!gg_eq_2(a.f43,b.f43))return 0;
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
gg_append(out,", ",2,1);
gg_render_2(out, value.f16);
gg_append(out,", ",2,1);
gg_render_2(out, value.f17);
gg_append(out,", ",2,1);
gg_render_2(out, value.f18);
gg_append(out,", ",2,1);
gg_render_2(out, value.f19);
gg_append(out,", ",2,1);
gg_render_2(out, value.f20);
gg_append(out,", ",2,1);
gg_render_2(out, value.f21);
gg_append(out,", ",2,1);
gg_render_2(out, value.f22);
gg_append(out,", ",2,1);
gg_render_2(out, value.f23);
gg_append(out,", ",2,1);
gg_render_2(out, value.f24);
gg_append(out,", ",2,1);
gg_render_2(out, value.f25);
gg_append(out,", ",2,1);
gg_render_2(out, value.f26);
gg_append(out,", ",2,1);
gg_render_2(out, value.f27);
gg_append(out,", ",2,1);
gg_render_2(out, value.f28);
gg_append(out,", ",2,1);
gg_render_2(out, value.f29);
gg_append(out,", ",2,1);
gg_render_2(out, value.f30);
gg_append(out,", ",2,1);
gg_render_2(out, value.f31);
gg_append(out,", ",2,1);
gg_render_2(out, value.f32);
gg_append(out,", ",2,1);
gg_render_2(out, value.f33);
gg_append(out,", ",2,1);
gg_render_2(out, value.f34);
gg_append(out,", ",2,1);
gg_render_2(out, value.f35);
gg_append(out,", ",2,1);
gg_render_2(out, value.f36);
gg_append(out,", ",2,1);
gg_render_2(out, value.f37);
gg_append(out,", ",2,1);
gg_render_2(out, value.f38);
gg_append(out,", ",2,1);
gg_render_2(out, value.f39);
gg_append(out,", ",2,1);
gg_render_2(out, value.f40);
gg_append(out,", ",2,1);
gg_render_2(out, value.f41);
gg_append(out,", ",2,1);
gg_render_2(out, value.f42);
gg_append(out,", ",2,1);
gg_render_2(out, value.f43);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, int64_t value) {
gg_integer(out,value);
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
return gg_equal_elements_3(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
gg_text(out,gg_view((value).data, 1));
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_type_5*)a.data)[i],((gg_type_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
if(!gg_eq_6(a.f44,b.f44))return 0;
if(!gg_eq_9(a.f45,b.f45))return 0;
if(!gg_eq_11(a.f46,b.f46))return 0;
return 1;
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
gg_append(out,"\121\070\040\173\040",5,1);
gg_render_6(out, value.f44);
gg_append(out,", ",2,1);
gg_render_9(out, value.f45);
gg_append(out,", ",2,1);
gg_render_11(out, value.f46);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(float a, float b) {
return a == b;
}
static void gg_render_6(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_7(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
return gg_equal_elements_7(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_7(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((gg_type_9*)a.data)[i],((gg_type_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b) {
return gg_equal_elements_8(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((gg_type_8*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_10(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_type_11*)a.data)[i],((gg_type_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b) {
return gg_equal_elements_10(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_11(gg_reference out, gg_type_11 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((int16_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return gg_equal_elements_5(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((gg_type_5*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
if(!gg_eq_14(a.f47,b.f47))return 0;
if(!gg_eq_14(a.f48,b.f48))return 0;
return 1;
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
gg_append(out,"\122\157\165\164\145\040\173\040",8,1);
gg_render_14(out, value.f47);
gg_append(out,", ",2,1);
gg_render_14(out, value.f48);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_14(gg_reference out, int32_t value) {
gg_integer(out,value);
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
if(!gg_eq_17(a.f54,b.f54))return 0;
if(!gg_eq_24(a.f58,b.f58))return 0;
if(!gg_eq_27(a.f59,b.f59))return 0;
return 1;
}
static void gg_render_16(gg_reference out, gg_type_16 value) {
gg_append(out,"\127\157\162\153\163\160\141\143\145\040\173\040",12,1);
gg_render_17(out, value.f54);
gg_append(out,", ",2,1);
gg_render_24(out, value.f58);
gg_append(out,", ",2,1);
gg_render_27(out, value.f59);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_type_17*)a.data)[i],((gg_type_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b) {
if(!gg_eq_20(a.f49,b.f49))return 0;
if(!gg_eq_21(a.f50,b.f50))return 0;
if(!gg_eq_21(a.f51,b.f51))return 0;
if(!gg_eq_23(a.f52,b.f52))return 0;
if(!gg_eq_23(a.f53,b.f53))return 0;
return 1;
}
static void gg_render_17(gg_reference out, gg_type_17 value) {
gg_append(out,"\127\145\151\147\150\164\124\151\154\145\040\173\040",13,1);
gg_render_20(out, value.f49);
gg_append(out,", ",2,1);
gg_render_21(out, value.f50);
gg_append(out,", ",2,1);
gg_render_21(out, value.f51);
gg_append(out,", ",2,1);
gg_render_23(out, value.f52);
gg_append(out,", ",2,1);
gg_render_23(out, value.f53);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_type_18*)a.data)[i],((gg_type_18*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b) {
return gg_equal_elements_3(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_18(gg_reference out, gg_type_18 value) {
gg_text(out,gg_view((value).data, 32));
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((gg_type_19*)a.data)[i],((gg_type_19*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b) {
return gg_equal_elements_18(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_19(gg_reference out, gg_type_19 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_18*)view.data)[i]).data, 32));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((gg_type_20*)a.data)[i],((gg_type_20*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b) {
return gg_equal_elements_19(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_20(gg_reference out, gg_type_20 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_19(out, ((gg_type_19*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((gg_type_21*)a.data)[i],((gg_type_21*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(gg_type_21 a, gg_type_21 b) {
return gg_equal_elements_6(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_21(gg_reference out, gg_type_21 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((gg_type_22*)a.data)[i],((gg_type_22*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(gg_type_22 a, gg_type_22 b) {
return gg_equal_elements_14(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_22(gg_reference out, gg_type_22 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_14(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((gg_type_23*)a.data)[i],((gg_type_23*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(gg_type_23 a, gg_type_23 b) {
return gg_equal_elements_22(gg_view((a).data, 512),gg_view((b).data, 512));
}
static void gg_render_23(gg_reference out, gg_type_23 value) {
int64_t i; gg_slice view=gg_view((value).data, 512);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_22(out, ((gg_type_22*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((gg_type_24*)a.data)[i],((gg_type_24*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b) {
if(!gg_eq_25(a.f55,b.f55))return 0;
if(!gg_eq_23(a.f56,b.f56))return 0;
if(!gg_eq_21(a.f57,b.f57))return 0;
return 1;
}
static void gg_render_24(gg_reference out, gg_type_24 value) {
gg_append(out,"\101\143\164\151\166\141\164\151\157\156\124\151\154\145\040\173\040",17,1);
gg_render_25(out, value.f55);
gg_append(out,", ",2,1);
gg_render_23(out, value.f56);
gg_append(out,", ",2,1);
gg_render_21(out, value.f57);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((gg_type_25*)a.data)[i],((gg_type_25*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b) {
return gg_equal_elements_9(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_25(gg_reference out, gg_type_25 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_9(out, ((gg_type_9*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((gg_type_26*)a.data)[i],((gg_type_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b) {
return gg_equal_elements_6(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_26(gg_reference out, gg_type_26 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((gg_type_27*)a.data)[i],((gg_type_27*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b) {
return gg_equal_elements_26(gg_view((a).data, 8192),gg_view((b).data, 8192));
}
static void gg_render_27(gg_reference out, gg_type_27 value) {
int64_t i; gg_slice view=gg_view((value).data, 8192);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_26(out, ((gg_type_26*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_type_28*)a.data)[i],((gg_type_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
return gg_equal_elements_3(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
gg_text(out,gg_view((value).data, 8));
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((gg_type_29*)a.data)[i],((gg_type_29*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b) {
return gg_equal_elements_28(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_29(gg_reference out, gg_type_29 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_28*)view.data)[i]).data, 8));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_30(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_30(((gg_type_30*)a.data)[i],((gg_type_30*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_30(gg_type_30 a, gg_type_30 b) {
return gg_equal_elements_3(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_30(gg_reference out, gg_type_30 value) {
gg_text(out,gg_view((value).data, 4));
}
static uint8_t gg_equal_elements_31(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_31(((gg_type_31*)a.data)[i],((gg_type_31*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b) {
return gg_equal_elements_30(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_31(gg_reference out, gg_type_31 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_30*)view.data)[i]).data, 4));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_32(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_32(((gg_type_32*)a.data)[i],((gg_type_32*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b) {
return gg_equal_elements_7(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_32(gg_reference out, gg_type_32 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_7(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_33(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_33(((gg_type_33*)a.data)[i],((gg_type_33*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_33(gg_type_33 a, gg_type_33 b) {
return gg_equal_elements_32(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_33(gg_reference out, gg_type_33 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_32(out, ((gg_type_32*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_34(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_34(((gg_type_34*)a.data)[i],((gg_type_34*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_34(gg_type_34 a, gg_type_34 b) {
return gg_equal_elements_3(gg_view((a).data, 128),gg_view((b).data, 128));
}
static void gg_render_34(gg_reference out, gg_type_34 value) {
gg_text(out,gg_view((value).data, 128));
}
static uint8_t gg_equal_elements_35(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_35(((gg_type_35*)a.data)[i],((gg_type_35*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b) {
return gg_equal_elements_7(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_35(gg_reference out, gg_type_35 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_7(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_36(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_36(((gg_type_36*)a.data)[i],((gg_type_36*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_36(gg_type_36 a, gg_type_36 b) {
return gg_equal_elements_6(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_36(gg_reference out, gg_type_36 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
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
gg_text(out,value);
}
static uint8_t gg_equal_elements_39(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_39(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_39(gg_slice a, gg_slice b) {
return gg_equal_elements_5(a,b);
}
static void gg_render_39(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((gg_type_5*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_40(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_40(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_40(gg_slice a, gg_slice b) {
return gg_equal_elements_13(a,b);
}
static void gg_render_40(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_13(out, ((gg_type_13*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_41(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_41(((gg_type_41*)a.data)[i],((gg_type_41*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_41(gg_type_41 a, gg_type_41 b) {
return a == b;
}
static void gg_render_41(gg_reference out, gg_type_41 value) {
gg_render_16(out,*value);
}
static uint8_t gg_equal_elements_42(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_42(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_42(gg_slice a, gg_slice b) {
return gg_equal_elements_28(a,b);
}
static void gg_render_42(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_28*)view.data)[i]).data, 8));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_43(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_43(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_43(gg_slice a, gg_slice b) {
return gg_equal_elements_30(a,b);
}
static void gg_render_43(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_30*)view.data)[i]).data, 4));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_44(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_44(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_44(gg_slice a, gg_slice b) {
return gg_equal_elements_32(a,b);
}
static void gg_render_44(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_32(out, ((gg_type_32*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_45(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_45(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_45(gg_slice a, gg_slice b) {
return gg_equal_elements_7(a,b);
}
static void gg_render_45(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_7(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_46(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_46(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_46(gg_slice a, gg_slice b) {
return gg_equal_elements_6(a,b);
}
static void gg_render_46(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_47(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_47(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_47(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_47(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_49(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_49(((gg_type_49*)a.data)[i],((gg_type_49*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_49(gg_type_49 a, gg_type_49 b) {
if(!gg_eq_2(a.f60,b.f60))return 0;
if(!gg_eq_2(a.f61,b.f61))return 0;
if(!gg_eq_2(a.f62,b.f62))return 0;
if(!gg_eq_2(a.f63,b.f63))return 0;
if(!gg_eq_2(a.f64,b.f64))return 0;
return 1;
}
static void gg_render_49(gg_reference out, gg_type_49 value) {
gg_append(out,"\124\151\154\145\124\162\141\151\164\163\040\173\040",13,1);
gg_render_2(out, value.f60);
gg_append(out,", ",2,1);
gg_render_2(out, value.f61);
gg_append(out,", ",2,1);
gg_render_2(out, value.f62);
gg_append(out,", ",2,1);
gg_render_2(out, value.f63);
gg_append(out,", ",2,1);
gg_render_2(out, value.f64);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_50(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_50(((gg_type_50*)a.data)[i],((gg_type_50*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_50(gg_type_50 a, gg_type_50 b) {
return a == b;
}
static void gg_render_50(gg_reference out, gg_type_50 value) {
gg_render_17(out,*value);
}
static uint8_t gg_equal_elements_51(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_51(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_51(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_51(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_52(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_52(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_52(double a, double b) {
return a == b;
}
static void gg_render_52(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_53(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_53(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_53(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_53(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_54(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_54(((gg_type_54*)a.data)[i],((gg_type_54*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_54(gg_type_54 a, gg_type_54 b) {
return gg_equal_elements_14(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_54(gg_reference out, gg_type_54 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_14(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_55(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_55(((gg_type_55*)a.data)[i],((gg_type_55*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_55(gg_type_55 a, gg_type_55 b) {
return a == b;
}
static void gg_render_55(gg_reference out, gg_type_55 value) {
gg_render_24(out,*value);
}
static uint8_t gg_equal_elements_56(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_56(((gg_type_56*)a.data)[i],((gg_type_56*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_56(gg_type_56 a, gg_type_56 b) {
return a == b;
}
static void gg_render_56(gg_reference out, gg_type_56 value) {
gg_render_5(out,*value);
}
static uint8_t gg_equal_elements_57(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_57(((gg_type_57*)a.data)[i],((gg_type_57*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_57(gg_type_57 a, gg_type_57 b) {
return gg_equal_elements_6(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_57(gg_reference out, gg_type_57 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_type_41 p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base);
static int64_t gg_function_2(gg_type_37 p0, int64_t base);
static int64_t gg_function_3(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t base);
static void gg_function_5(gg_type_37 p0, gg_slice p1, gg_type_41 p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, int64_t base);
static gg_type_49 gg_function_6(int64_t p0, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t base);
static gg_type_11 gg_function_8(int16_t p159, int64_t base);
static void gg_function_9(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, gg_type_50 p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base);
static void gg_function_10(gg_type_50 p0, int64_t p1, int32_t p2, int64_t base);
static uint16_t gg_function_11(gg_slice p0, int64_t p1, int64_t base);
static void gg_function_13(gg_type_50 p0, int64_t p1, int32_t p2, int64_t base);
static gg_type_18 gg_function_14(gg_slice p0, int64_t p1, int64_t base);
static gg_type_18 gg_function_15(gg_type_18 p237, uint8_t p238, int64_t base);
static gg_type_18 gg_function_16(gg_type_18 p245, uint8_t p246, int64_t base);
static gg_type_18 gg_function_17(gg_type_18 p326, gg_type_18 p327, int64_t base);
static uint32_t gg_function_18(gg_slice p0, int64_t p1, int64_t base);
static void gg_function_19(gg_type_50 p0, int64_t p1, int64_t p2, gg_type_28 p3, uint8_t p4, int64_t base);
static void gg_function_20(gg_type_50 p0, int64_t p1, int64_t p2, uint8_t p3, int64_t base);
static void gg_function_21(gg_type_50 p0, int64_t p1, int64_t p2, gg_type_32 p3, int16_t p4, int64_t base);
static gg_type_11 gg_function_22(gg_type_18 p463, gg_type_8 p464, int64_t base);
static gg_type_22 gg_function_23(gg_type_11 p471, gg_type_11 p472, int64_t base);
static int32_t gg_function_24(gg_type_22 p0, int64_t base);
static gg_type_54 gg_function_25(gg_type_22 p481, int64_t base);
static void gg_function_27(gg_type_37 p0, gg_type_41 p1, int64_t base);
static void gg_function_28(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static gg_type_22 gg_function_29(gg_type_22 p541, gg_type_22 p542, int64_t base);
static gg_type_22 gg_function_30(int32_t p0, int64_t base);
static gg_type_22 gg_function_31(gg_type_22 p578, gg_type_22 p579, int64_t base);
static gg_type_26 gg_function_32(float p0, gg_type_22 p1, int64_t base);
static gg_type_26 gg_function_33(float p597, gg_type_22 p598, gg_type_26 p599, int64_t base);
static void gg_function_36(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static gg_type_11 gg_function_37(int16_t p0, int16_t p1, int64_t base);
static gg_type_8 gg_function_38(gg_type_18 p0, uint8_t p1, int64_t base);
static gg_type_18 gg_function_39(gg_type_8 p0, int64_t base);
static gg_type_22 gg_function_40(gg_type_22 p0, gg_type_22 p1, int64_t base);
static void gg_function_41(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static void gg_function_42(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static void gg_function_43(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static gg_type_11 gg_function_44(gg_type_8 p0, gg_type_8 p1, int64_t base);
static void gg_function_45(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static void gg_function_46(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static void gg_function_47(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_type_41 p3, int64_t base);
static gg_type_8 gg_function_48(gg_type_8 p0, int64_t base);
static uint8_t gg_function_49(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_type_41 p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base);
static int64_t gg_function_50(int64_t p0, int64_t p1, int64_t base);
static void gg_function_51(gg_type_37 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_41 p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, gg_slice p14, gg_slice p15, gg_slice p16, gg_slice p17, int64_t base);
static int64_t gg_function_52(int64_t p0, int64_t p1, int64_t base);
static void gg_function_53(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_type_41 p3, int64_t base);
static void gg_function_54(gg_type_41 p0, int64_t p1, int64_t p2, uint8_t p3, int64_t base);
static void gg_function_55(gg_type_37 p0, gg_slice p1, gg_type_41 p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, int64_t base);
static void gg_function_56(gg_type_37 p0, gg_type_41 p1, int64_t base);
static uint8_t gg_function_57(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_type_41 p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base);
static void gg_function_61(gg_type_37 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_41 p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, gg_slice p14, gg_slice p15, gg_slice p16, gg_slice p17, int64_t base);
static void gg_function_62(gg_type_37 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_41 p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, gg_slice p14, gg_slice p15, gg_slice p16, gg_slice p17, int64_t base);
static void gg_function_63(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_type_41 p3, int64_t base);
static void gg_function_64(gg_type_37 p0, gg_slice p1, gg_type_41 p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, int64_t base);
static void gg_function_65(gg_type_37 p0, gg_type_41 p1, int64_t base);
static gg_type_26 gg_function_32(float p0, gg_type_22 p1, int64_t base) {
  gg_type_26 t0;
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
  gg_type_22 v1;
  gg_type_26 v2;
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
static gg_type_22 gg_function_40(gg_type_22 p0, gg_type_22 p1, int64_t base) {
  gg_type_22 t0;
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
  gg_type_22 v0;
  gg_type_22 v1;
  gg_type_22 v2;
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
static gg_type_22 gg_function_30(int32_t p0, int64_t base) {
  gg_type_22 t0;
  int32_t t1;
  int32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int32_t * t7;
  int32_t v0;
  gg_type_22 v1;
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
/* FG_PACKED_MATCH:pair_sum32 */
static gg_type_54 gg_function_25(gg_type_22 p481, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_54 packed_result;
    _mm_storeu_si128((__m128i*)packed_result.data, _mm_hadd_epi32(_mm256_castsi256_si128(_mm256_loadu_si256((const __m256i*)p481.data)), _mm256_extracti128_si256(_mm256_loadu_si256((const __m256i*)p481.data), 1)));
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
  gg_type_22 v481;
  gg_type_54 v482;
  int64_t v484;
  v481 = p481;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<4; t3++) (v482).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(4));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v484 = t4;
  {
  t6 = gg_view((v482).data, 4);
  t7 = v484;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v481).data, 8);
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(v484));
  t12 = ((int64_t)((uint64_t)(t10) * (uint64_t)(t11)));
  t13 = ((int32_t *)t9.data) + gg_index(t12, t9.len);
  t14 = ((int32_t)((*t13)));
  t15 = gg_view((v481).data, 8);
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((int64_t)(v484));
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
  t0 = v482;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_24(gg_type_22 p0, int64_t base) {
  int32_t t0;
  gg_type_22 t1;
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
  gg_type_22 v0;
  gg_type_54 v1;
  v0 = p0;
  {
  t1 = v0;
  v1 = gg_function_25(t1, base + 0);
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
static gg_type_22 gg_function_29(gg_type_22 p541, gg_type_22 p542, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_22 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)p541.data), _mm256_loadu_si256((const __m256i*)p542.data)));
    return packed_result;
  }
#endif
  gg_type_22 t0;
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
  gg_type_22 v541;
  gg_type_22 v542;
  gg_type_22 v543;
  int64_t v545;
  v541 = p541;
  v542 = p542;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v543).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v545 = t4;
  {
  t6 = gg_view((v543).data, 8);
  t7 = v545;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v541).data, 8);
  t10 = v545;
  t11 = ((int32_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int32_t)((*t11)));
  t13 = gg_view((v542).data, 8);
  t14 = v545;
  t15 = ((int32_t *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int32_t)((*t15)));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t12) + (uint64_t)(t16)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v543;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:madd16 */
static gg_type_22 gg_function_23(gg_type_11 p471, gg_type_11 p472, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_22 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_madd_epi16(_mm256_loadu_si256((const __m256i*)p471.data), _mm256_loadu_si256((const __m256i*)p472.data)));
    return packed_result;
  }
#endif
  gg_type_22 t0;
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
  gg_type_11 v471;
  gg_type_11 v472;
  gg_type_22 v473;
  int64_t v475;
  int64_t v476;
  v471 = p471;
  v472 = p472;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v473).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v475 = t4;
  {
  t6 = gg_view((v471).data, 16);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v475));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((int16_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int64_t)(((int64_t)((*t10)))));
  t12 = gg_view((v472).data, 16);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v475));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((int16_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int64_t)(((int64_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v471).data, 16);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v475));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((int16_t *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int64_t)(((int64_t)((*t25)))));
  t27 = gg_view((v472).data, 16);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v475));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((int16_t *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int64_t)(((int64_t)((*t33)))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v476 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  t36 = gg_view((v473).data, 8);
  t37 = v475;
  t38 = ((int32_t *)t36.data) + gg_index(t37, t36.len);
  (*t38) = ((int32_t)(((int32_t)(v476))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v473;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:maddubs */
static gg_type_11 gg_function_22(gg_type_18 p463, gg_type_8 p464, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_11 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_maddubs_epi16(_mm256_loadu_si256((const __m256i*)p463.data), _mm256_loadu_si256((const __m256i*)p464.data)));
    return packed_result;
  }
#endif
  gg_type_11 t0;
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
  gg_type_18 v463;
  gg_type_8 v464;
  gg_type_11 v465;
  int64_t v467;
  int32_t v468;
  v463 = p463;
  v464 = p464;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v465).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v467 = t4;
  {
  t6 = gg_view((v463).data, 32);
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v467));
  t9 = ((int64_t)((uint64_t)(t7) * (uint64_t)(t8)));
  t10 = ((uint8_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int32_t)(((int32_t)((*t10)))));
  t12 = gg_view((v464).data, 32);
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v467));
  t15 = ((int64_t)((uint64_t)(t13) * (uint64_t)(t14)));
  t16 = ((int8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((int32_t)(((int32_t)((*t16)))));
  t18 = ((int32_t)(((int32_t)((uint64_t)(t11) * (uint64_t)(t17)))));
  t19 = gg_view((v463).data, 32);
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v467));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((uint8_t *)t19.data) + gg_index(t24, t19.len);
  t26 = ((int32_t)(((int32_t)((*t25)))));
  t27 = gg_view((v464).data, 32);
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v467));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  t33 = ((int8_t *)t27.data) + gg_index(t32, t27.len);
  t34 = ((int32_t)(((int32_t)((*t33)))));
  t35 = ((int32_t)(((int32_t)((uint64_t)(t26) * (uint64_t)(t34)))));
  v468 = ((int32_t)(((int32_t)((uint64_t)(t18) + (uint64_t)(t35)))));
  t36 = gg_view((v465).data, 16);
  t37 = v467;
  t38 = ((int16_t *)t36.data) + gg_index(t37, t36.len);
  t40 = ((int32_t)(v468));
  t41 = ((int64_t)UINT64_C(32768));
  t42 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t41)))));
  t43 = ((t40) < (t42));
  if (t43) {
  {
  t44 = ((int64_t)UINT64_C(32768));
  t39 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t44)))));
  }
  } else {
  t45 = ((int32_t)(v468));
  t46 = ((int32_t)(((int64_t)UINT64_C(32767))));
  t47 = ((t45) > (t46));
  if (t47) {
  {
  t39 = ((int64_t)(((int64_t)UINT64_C(32767))));
  }
  } else {
  {
  t39 = ((int64_t)(v468));
  }
  }
  }
  (*t38) = ((int16_t)(((int16_t)(t39))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v465;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_18 gg_function_39(gg_type_8 p0, int64_t base) {
  gg_type_18 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t * t7;
  int64_t t8;
  int8_t * t9;
  gg_type_8 v0;
  gg_type_18 v1;
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
static gg_type_8 gg_function_38(gg_type_18 p0, uint8_t p1, int64_t base) {
  gg_type_8 t0;
  int8_t t1;
  int8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int8_t * t7;
  int64_t t8;
  uint8_t * t9;
  uint8_t t10;
  uint8_t t11;
  gg_type_18 v0;
  uint8_t v1;
  gg_type_8 v2;
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
  t9 = ((uint8_t *)(v0).data) + gg_index(t8, ((int64_t)UINT64_C(32)));
  t10 = ((uint8_t)((*t9)));
  t11 = ((uint8_t)(v1));
  (*t7) = ((int8_t)(((int8_t)(((uint8_t)((uint64_t)(t10) - (uint64_t)(t11)))))));
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
static gg_type_11 gg_function_37(int16_t p0, int16_t p1, int64_t base) {
  gg_type_11 t0;
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
  gg_type_11 v2;
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
static void gg_function_46(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  int32_t t21;
  int32_t t22;
  int64_t t23;
  int32_t t24;
  int32_t t25;
  int64_t t26;
  int32_t t27;
  int32_t t28;
  int64_t t29;
  int32_t t30;
  int32_t t31;
  int64_t t32;
  int32_t t33;
  int32_t t34;
  int64_t t35;
  int32_t t36;
  int32_t t37;
  int64_t t38;
  int32_t t39;
  int32_t t40;
  int64_t t41;
  int32_t t42;
  int32_t t43;
  int64_t t44;
  int32_t t45;
  int32_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  gg_type_19 * t51;
  gg_slice t52;
  int64_t t53;
  gg_type_18 * t54;
  gg_slice t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  gg_type_22 * t63;
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int32_t * t70;
  gg_slice t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  gg_type_22 * t79;
  gg_slice t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int32_t * t88;
  int16_t t89;
  int16_t t90;
  gg_type_18 t91;
  uint8_t t92;
  gg_type_22 t93;
  gg_type_11 t94;
  gg_slice t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  gg_type_9 * t103;
  gg_slice t104;
  int64_t t105;
  gg_type_8 * t106;
  gg_type_8 t107;
  gg_type_18 t108;
  gg_type_8 t109;
  gg_type_11 t110;
  gg_type_22 t111;
  gg_type_22 t112;
  gg_type_11 t113;
  gg_slice t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  gg_type_9 * t122;
  gg_slice t123;
  int64_t t124;
  gg_type_8 * t125;
  gg_type_8 t126;
  gg_type_18 t127;
  gg_type_8 t128;
  gg_type_11 t129;
  gg_type_22 t130;
  gg_type_22 t131;
  gg_type_11 t132;
  gg_slice t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  gg_type_9 * t141;
  gg_slice t142;
  int64_t t143;
  gg_type_8 * t144;
  gg_type_8 t145;
  gg_type_18 t146;
  gg_type_8 t147;
  gg_type_11 t148;
  gg_type_22 t149;
  gg_type_22 t150;
  gg_type_11 t151;
  gg_slice t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  gg_type_9 * t160;
  gg_slice t161;
  int64_t t162;
  gg_type_8 * t163;
  gg_type_8 t164;
  gg_type_18 t165;
  gg_type_8 t166;
  gg_type_11 t167;
  gg_type_22 t168;
  gg_type_22 t169;
  gg_type_11 t170;
  gg_slice t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  gg_type_9 * t179;
  gg_slice t180;
  int64_t t181;
  gg_type_8 * t182;
  gg_type_8 t183;
  gg_type_18 t184;
  gg_type_8 t185;
  gg_type_11 t186;
  gg_type_22 t187;
  gg_type_22 t188;
  gg_type_11 t189;
  gg_slice t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  gg_type_9 * t198;
  gg_slice t199;
  int64_t t200;
  gg_type_8 * t201;
  gg_type_8 t202;
  gg_type_18 t203;
  gg_type_8 t204;
  gg_type_11 t205;
  gg_type_22 t206;
  gg_type_22 t207;
  gg_type_11 t208;
  gg_slice t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  gg_type_9 * t217;
  gg_slice t218;
  int64_t t219;
  gg_type_8 * t220;
  gg_type_8 t221;
  gg_type_18 t222;
  gg_type_8 t223;
  gg_type_11 t224;
  gg_type_22 t225;
  gg_type_22 t226;
  gg_type_11 t227;
  gg_slice t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  gg_type_9 * t236;
  gg_slice t237;
  int64_t t238;
  gg_type_8 * t239;
  gg_type_8 t240;
  gg_type_18 t241;
  gg_type_8 t242;
  gg_type_11 t243;
  gg_type_22 t244;
  int32_t t245;
  int32_t t246;
  int64_t t247;
  int64_t t248;
  int32_t * t249;
  gg_type_22 t250;
  int64_t t251;
  int32_t * t252;
  gg_type_22 t253;
  int64_t t254;
  int32_t * t255;
  gg_type_22 t256;
  int64_t t257;
  int32_t * t258;
  gg_type_22 t259;
  int64_t t260;
  int32_t * t261;
  gg_type_22 t262;
  int64_t t263;
  int32_t * t264;
  gg_type_22 t265;
  int64_t t266;
  int32_t * t267;
  gg_type_22 t268;
  int64_t t269;
  int32_t * t270;
  gg_type_22 t271;
  gg_type_22 t272;
  gg_slice t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  gg_type_22 * t277;
  gg_slice t278;
  int64_t t279;
  int32_t * t280;
  int32_t t281;
  gg_type_22 t282;
  int64_t t283;
  float * t284;
  float t285;
  gg_type_22 t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  gg_type_26 * t298;
  gg_slice t299;
  int64_t t300;
  float * t301;
  int64_t t302;
  float * t303;
  float t304;
  gg_slice t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  float * t313;
  float t314;
  int64_t t315;
  gg_type_26 * t316;
  gg_slice t317;
  int64_t t318;
  float * t319;
  float t320;
  float t321;
  gg_type_41 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_50 v6;
  gg_type_55 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  gg_type_22 v15;
  gg_type_22 v16;
  gg_type_22 v17;
  gg_type_22 v18;
  gg_type_22 v19;
  gg_type_22 v20;
  gg_type_22 v21;
  gg_type_22 v22;
  gg_type_22 v23;
  int64_t v24;
  gg_type_18 v25;
  int32_t v26;
  int32_t v27;
  gg_type_11 v28;
  gg_type_8 v29;
  gg_type_22 v30;
  gg_type_26 v31;
  int64_t v32;
  int64_t v33;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  v6 = &(((*(v0))).f54);
  v7 = &(((*(v0))).f58);
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  } else {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  }
  t4 = ((int64_t)UINT64_C(16));
  t3 = 0;
label0:;
  if (t3 >= t4) goto label2;
  v9 = t3;
  {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v9));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(v10));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t11 = ((int64_t)(v10));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(v5));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)UINT64_C(2));
  t15 = 0;
label3:;
  if (t15 >= t16) goto label5;
  v13 = t15;
  {
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(v13));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t20)))));
  memset(&t21, 0, sizeof(t21));
  t22 = ((int32_t)(t21));
  for (t23=0; t23<8; t23++) (v15).data[t23] = t22;
  memset(&t24, 0, sizeof(t24));
  t25 = ((int32_t)(t24));
  for (t26=0; t26<8; t26++) (v16).data[t26] = t25;
  memset(&t27, 0, sizeof(t27));
  t28 = ((int32_t)(t27));
  for (t29=0; t29<8; t29++) (v17).data[t29] = t28;
  memset(&t30, 0, sizeof(t30));
  t31 = ((int32_t)(t30));
  for (t32=0; t32<8; t32++) (v18).data[t32] = t31;
  memset(&t33, 0, sizeof(t33));
  t34 = ((int32_t)(t33));
  for (t35=0; t35<8; t35++) (v19).data[t35] = t34;
  memset(&t36, 0, sizeof(t36));
  t37 = ((int32_t)(t36));
  for (t38=0; t38<8; t38++) (v20).data[t38] = t37;
  memset(&t39, 0, sizeof(t39));
  t40 = ((int32_t)(t39));
  for (t41=0; t41<8; t41++) (v21).data[t41] = t40;
  memset(&t42, 0, sizeof(t42));
  t43 = ((int32_t)(t42));
  for (t44=0; t44<8; t44++) (v22).data[t44] = t43;
  memset(&t45, 0, sizeof(t45));
  t46 = ((int32_t)(t45));
  for (t47=0; t47<8; t47++) (v23).data[t47] = t46;
  t49 = ((int64_t)UINT64_C(8));
  t48 = 0;
label6:;
  if (t48 >= t49) goto label8;
  v24 = t48;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t50 = v12;
  t51 = ((gg_type_19 *)(((*(v6))).f49).data) + gg_index(t50, ((int64_t)UINT64_C(256)));
  t52 = gg_view(((*t51)).data, 8);
  t53 = v24;
  t54 = ((gg_type_18 *)t52.data) + gg_index(t53, t52.len);
  v25 = (*t54);
  t55 = gg_view((((*(v6))).f52).data, 512);
  t56 = ((int64_t)(v12));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  t59 = ((int64_t)(v24));
  t60 = ((int64_t)(((int64_t)UINT64_C(4))));
  t61 = ((int64_t)(((int64_t)gg_div(t59, t60))));
  t62 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t61)));
  t63 = ((gg_type_22 *)t55.data) + gg_index(t62, t55.len);
  t64 = gg_view(((*t63)).data, 8);
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t66 = ((int64_t)(v24));
  t67 = ((int64_t)(((int64_t)UINT64_C(4))));
  t68 = ((int64_t)(((int64_t)gg_mod(t66, t67))));
  t69 = ((int64_t)((uint64_t)(t65) * (uint64_t)(t68)));
  t70 = ((int32_t *)t64.data) + gg_index(t69, t64.len);
  v26 = ((int32_t)((*t70)));
  t71 = gg_view((((*(v6))).f52).data, 512);
  t72 = ((int64_t)(v12));
  t73 = ((int64_t)(((int64_t)UINT64_C(2))));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  t75 = ((int64_t)(v24));
  t76 = ((int64_t)(((int64_t)UINT64_C(4))));
  t77 = ((int64_t)(((int64_t)gg_div(t75, t76))));
  t78 = ((int64_t)((uint64_t)(t74) + (uint64_t)(t77)));
  t79 = ((gg_type_22 *)t71.data) + gg_index(t78, t71.len);
  t80 = gg_view(((*t79)).data, 8);
  t81 = ((int64_t)(((int64_t)UINT64_C(2))));
  t82 = ((int64_t)(v24));
  t83 = ((int64_t)(((int64_t)UINT64_C(4))));
  t84 = ((int64_t)(((int64_t)gg_mod(t82, t83))));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t84)))));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((int64_t)((uint64_t)(t85) + (uint64_t)(t86)));
  t88 = ((int32_t *)t80.data) + gg_index(t87, t80.len);
  v27 = ((int32_t)((*t88)));
  t89 = ((int16_t)(((int16_t)(v26))));
  t90 = ((int16_t)(((int16_t)(v27))));
  v28 = gg_function_37(t89, t90, base + 0);
  t91 = v25;
  t92 = ((uint8_t)(((int64_t)UINT64_C(128))));
  v29 = gg_function_38(t91, t92, base + 0);
  t93 = v15;
  t94 = v28;
  t95 = gg_view((((*(v7))).f55).data, 256);
  t96 = ((int64_t)(v14));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  t99 = ((int64_t)(v4));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  t101 = ((int64_t)(v5));
  t102 = ((int64_t)((uint64_t)(t100) + (uint64_t)(t101)));
  t103 = ((gg_type_9 *)t95.data) + gg_index(t102, t95.len);
  t104 = gg_view(((*t103)).data, 8);
  t105 = v24;
  t106 = ((gg_type_8 *)t104.data) + gg_index(t105, t104.len);
  t107 = (*t106);
  t108 = gg_function_39(t107, base + 0);
  t109 = v29;
  t110 = gg_function_22(t108, t109, base + 0);
  t111 = gg_function_23(t94, t110, base + 0);
  v15 = gg_function_29(t93, t111, base + 0);
  t112 = v16;
  t113 = v28;
  t114 = gg_view((((*(v7))).f55).data, 256);
  t115 = ((int64_t)(v14));
  t116 = ((int64_t)(((int64_t)UINT64_C(1))));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = ((int64_t)(v4));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) * (uint64_t)(t118)))));
  t120 = ((int64_t)(v5));
  t121 = ((int64_t)((uint64_t)(t119) + (uint64_t)(t120)));
  t122 = ((gg_type_9 *)t114.data) + gg_index(t121, t114.len);
  t123 = gg_view(((*t122)).data, 8);
  t124 = v24;
  t125 = ((gg_type_8 *)t123.data) + gg_index(t124, t123.len);
  t126 = (*t125);
  t127 = gg_function_39(t126, base + 0);
  t128 = v29;
  t129 = gg_function_22(t127, t128, base + 0);
  t130 = gg_function_23(t113, t129, base + 0);
  v16 = gg_function_29(t112, t130, base + 0);
  t131 = v17;
  t132 = v28;
  t133 = gg_view((((*(v7))).f55).data, 256);
  t134 = ((int64_t)(v14));
  t135 = ((int64_t)(((int64_t)UINT64_C(2))));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t134) + (uint64_t)(t135)))));
  t137 = ((int64_t)(v4));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t137)))));
  t139 = ((int64_t)(v5));
  t140 = ((int64_t)((uint64_t)(t138) + (uint64_t)(t139)));
  t141 = ((gg_type_9 *)t133.data) + gg_index(t140, t133.len);
  t142 = gg_view(((*t141)).data, 8);
  t143 = v24;
  t144 = ((gg_type_8 *)t142.data) + gg_index(t143, t142.len);
  t145 = (*t144);
  t146 = gg_function_39(t145, base + 0);
  t147 = v29;
  t148 = gg_function_22(t146, t147, base + 0);
  t149 = gg_function_23(t132, t148, base + 0);
  v17 = gg_function_29(t131, t149, base + 0);
  t150 = v18;
  t151 = v28;
  t152 = gg_view((((*(v7))).f55).data, 256);
  t153 = ((int64_t)(v14));
  t154 = ((int64_t)(((int64_t)UINT64_C(3))));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t154)))));
  t156 = ((int64_t)(v4));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) * (uint64_t)(t156)))));
  t158 = ((int64_t)(v5));
  t159 = ((int64_t)((uint64_t)(t157) + (uint64_t)(t158)));
  t160 = ((gg_type_9 *)t152.data) + gg_index(t159, t152.len);
  t161 = gg_view(((*t160)).data, 8);
  t162 = v24;
  t163 = ((gg_type_8 *)t161.data) + gg_index(t162, t161.len);
  t164 = (*t163);
  t165 = gg_function_39(t164, base + 0);
  t166 = v29;
  t167 = gg_function_22(t165, t166, base + 0);
  t168 = gg_function_23(t151, t167, base + 0);
  v18 = gg_function_29(t150, t168, base + 0);
  t169 = v19;
  t170 = v28;
  t171 = gg_view((((*(v7))).f55).data, 256);
  t172 = ((int64_t)(v14));
  t173 = ((int64_t)(((int64_t)UINT64_C(4))));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t172) + (uint64_t)(t173)))));
  t175 = ((int64_t)(v4));
  t176 = ((int64_t)(((int64_t)((uint64_t)(t174) * (uint64_t)(t175)))));
  t177 = ((int64_t)(v5));
  t178 = ((int64_t)((uint64_t)(t176) + (uint64_t)(t177)));
  t179 = ((gg_type_9 *)t171.data) + gg_index(t178, t171.len);
  t180 = gg_view(((*t179)).data, 8);
  t181 = v24;
  t182 = ((gg_type_8 *)t180.data) + gg_index(t181, t180.len);
  t183 = (*t182);
  t184 = gg_function_39(t183, base + 0);
  t185 = v29;
  t186 = gg_function_22(t184, t185, base + 0);
  t187 = gg_function_23(t170, t186, base + 0);
  v19 = gg_function_29(t169, t187, base + 0);
  t188 = v20;
  t189 = v28;
  t190 = gg_view((((*(v7))).f55).data, 256);
  t191 = ((int64_t)(v14));
  t192 = ((int64_t)(((int64_t)UINT64_C(5))));
  t193 = ((int64_t)(((int64_t)((uint64_t)(t191) + (uint64_t)(t192)))));
  t194 = ((int64_t)(v4));
  t195 = ((int64_t)(((int64_t)((uint64_t)(t193) * (uint64_t)(t194)))));
  t196 = ((int64_t)(v5));
  t197 = ((int64_t)((uint64_t)(t195) + (uint64_t)(t196)));
  t198 = ((gg_type_9 *)t190.data) + gg_index(t197, t190.len);
  t199 = gg_view(((*t198)).data, 8);
  t200 = v24;
  t201 = ((gg_type_8 *)t199.data) + gg_index(t200, t199.len);
  t202 = (*t201);
  t203 = gg_function_39(t202, base + 0);
  t204 = v29;
  t205 = gg_function_22(t203, t204, base + 0);
  t206 = gg_function_23(t189, t205, base + 0);
  v20 = gg_function_29(t188, t206, base + 0);
  t207 = v21;
  t208 = v28;
  t209 = gg_view((((*(v7))).f55).data, 256);
  t210 = ((int64_t)(v14));
  t211 = ((int64_t)(((int64_t)UINT64_C(6))));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t210) + (uint64_t)(t211)))));
  t213 = ((int64_t)(v4));
  t214 = ((int64_t)(((int64_t)((uint64_t)(t212) * (uint64_t)(t213)))));
  t215 = ((int64_t)(v5));
  t216 = ((int64_t)((uint64_t)(t214) + (uint64_t)(t215)));
  t217 = ((gg_type_9 *)t209.data) + gg_index(t216, t209.len);
  t218 = gg_view(((*t217)).data, 8);
  t219 = v24;
  t220 = ((gg_type_8 *)t218.data) + gg_index(t219, t218.len);
  t221 = (*t220);
  t222 = gg_function_39(t221, base + 0);
  t223 = v29;
  t224 = gg_function_22(t222, t223, base + 0);
  t225 = gg_function_23(t208, t224, base + 0);
  v21 = gg_function_29(t207, t225, base + 0);
  t226 = v22;
  t227 = v28;
  t228 = gg_view((((*(v7))).f55).data, 256);
  t229 = ((int64_t)(v14));
  t230 = ((int64_t)(((int64_t)UINT64_C(7))));
  t231 = ((int64_t)(((int64_t)((uint64_t)(t229) + (uint64_t)(t230)))));
  t232 = ((int64_t)(v4));
  t233 = ((int64_t)(((int64_t)((uint64_t)(t231) * (uint64_t)(t232)))));
  t234 = ((int64_t)(v5));
  t235 = ((int64_t)((uint64_t)(t233) + (uint64_t)(t234)));
  t236 = ((gg_type_9 *)t228.data) + gg_index(t235, t228.len);
  t237 = gg_view(((*t236)).data, 8);
  t238 = v24;
  t239 = ((gg_type_8 *)t237.data) + gg_index(t238, t237.len);
  t240 = (*t239);
  t241 = gg_function_39(t240, base + 0);
  t242 = v29;
  t243 = gg_function_22(t241, t242, base + 0);
  t244 = gg_function_23(t227, t243, base + 0);
  v22 = gg_function_29(t226, t244, base + 0);
  }
label7:;
  t48++; goto label6;
label8:;
  memset(&t245, 0, sizeof(t245));
  t246 = ((int32_t)(t245));
  for (t247=0; t247<8; t247++) (v30).data[t247] = t246;
  /* FG_DIRECT_SLICE_INDEX */
  t248 = ((int64_t)UINT64_C(0));
  t249 = ((int32_t *)(v30).data) + gg_index(t248, ((int64_t)UINT64_C(8)));
  t250 = v15;
  (*t249) = gg_function_24(t250, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t251 = ((int64_t)UINT64_C(1));
  t252 = ((int32_t *)(v30).data) + gg_index(t251, ((int64_t)UINT64_C(8)));
  t253 = v16;
  (*t252) = gg_function_24(t253, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t254 = ((int64_t)UINT64_C(2));
  t255 = ((int32_t *)(v30).data) + gg_index(t254, ((int64_t)UINT64_C(8)));
  t256 = v17;
  (*t255) = gg_function_24(t256, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t257 = ((int64_t)UINT64_C(3));
  t258 = ((int32_t *)(v30).data) + gg_index(t257, ((int64_t)UINT64_C(8)));
  t259 = v18;
  (*t258) = gg_function_24(t259, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t260 = ((int64_t)UINT64_C(4));
  t261 = ((int32_t *)(v30).data) + gg_index(t260, ((int64_t)UINT64_C(8)));
  t262 = v19;
  (*t261) = gg_function_24(t262, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t263 = ((int64_t)UINT64_C(5));
  t264 = ((int32_t *)(v30).data) + gg_index(t263, ((int64_t)UINT64_C(8)));
  t265 = v20;
  (*t264) = gg_function_24(t265, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t266 = ((int64_t)UINT64_C(6));
  t267 = ((int32_t *)(v30).data) + gg_index(t266, ((int64_t)UINT64_C(8)));
  t268 = v21;
  (*t267) = gg_function_24(t268, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t269 = ((int64_t)UINT64_C(7));
  t270 = ((int32_t *)(v30).data) + gg_index(t269, ((int64_t)UINT64_C(8)));
  t271 = v22;
  (*t270) = gg_function_24(t271, base + 0);
  t272 = v30;
  t273 = gg_view((((*(v6))).f53).data, 512);
  t274 = ((int64_t)(v12));
  t275 = ((int64_t)(((int64_t)UINT64_C(2))));
  t276 = ((int64_t)((uint64_t)(t274) * (uint64_t)(t275)));
  t277 = ((gg_type_22 *)t273.data) + gg_index(t276, t273.len);
  t278 = gg_view(((*t277)).data, 8);
  t279 = ((int64_t)UINT64_C(0));
  t280 = ((int32_t *)t278.data) + gg_index(t279, t278.len);
  t281 = ((int32_t)((*t280)));
  t282 = gg_function_30(t281, base + 0);
  v30 = gg_function_40(t272, t282, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t283 = v11;
  t284 = ((float *)(((*(v6))).f50).data) + gg_index(t283, ((int64_t)UINT64_C(256)));
  t285 = ((float)((*t284)));
  t286 = v30;
  v31 = gg_function_32(t285, t286, base + 0);
  t287 = ((int64_t)(v3));
  t288 = ((int64_t)(v9));
  t289 = ((int64_t)(((int64_t)((uint64_t)(t287) + (uint64_t)(t288)))));
  t290 = ((int64_t)(((int64_t)UINT64_C(32))));
  t291 = ((int64_t)(((int64_t)((uint64_t)(t289) * (uint64_t)(t290)))));
  t292 = ((int64_t)(v14));
  t293 = ((int64_t)(((int64_t)UINT64_C(8))));
  t294 = ((int64_t)(((int64_t)gg_div(t292, t293))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t291) + (uint64_t)(t294)))));
  t296 = ((int64_t)UINT64_C(8));
  t295 = 0;
label9:;
  if (t295 >= t296) goto label11;
  v33 = t295;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t297 = v32;
  t298 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t297, ((int64_t)UINT64_C(8192)));
  t299 = gg_view(((*t298)).data, 8);
  t300 = v33;
  t301 = ((float *)t299.data) + gg_index(t300, t299.len);
  /* FG_DIRECT_SLICE_INDEX */
  t302 = v33;
  t303 = ((float *)(v31).data) + gg_index(t302, ((int64_t)UINT64_C(8)));
  t304 = ((float)((*t303)));
  t305 = gg_view((((*(v7))).f57).data, 256);
  t306 = ((int64_t)(v5));
  t307 = ((int64_t)(((int64_t)UINT64_C(16))));
  t308 = ((int64_t)(((int64_t)((uint64_t)(t306) * (uint64_t)(t307)))));
  t309 = ((int64_t)(v14));
  t310 = ((int64_t)(((int64_t)((uint64_t)(t308) + (uint64_t)(t309)))));
  t311 = ((int64_t)(v33));
  t312 = ((int64_t)((uint64_t)(t310) + (uint64_t)(t311)));
  t313 = ((float *)t305.data) + gg_index(t312, t305.len);
  t314 = ((float)((*t313)));
  /* FG_DIRECT_SLICE_INDEX */
  t315 = v32;
  t316 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t315, ((int64_t)UINT64_C(8192)));
  t317 = gg_view(((*t316)).data, 8);
  t318 = v33;
  t319 = ((float *)t317.data) + gg_index(t318, t317.len);
  t320 = ((float)((*t319)));
  t321 = fmaf(t304, t314, t320);
  (*t301) = ((float)(t321));
  }
label10:;
  t295++; goto label9;
label11:;
  }
label4:;
  t15++; goto label3;
label5:;
  }
label1:;
  t3++; goto label0;
label2:;
  }
gg_done:;
}
/* FG_PACKED_MATCH:splat16 */
static gg_type_11 gg_function_8(int16_t p159, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_11 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_set1_epi16(p159));
    return packed_result;
  }
#endif
  gg_type_11 t0;
  int16_t t1;
  int16_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_slice t6;
  int64_t t7;
  int16_t * t8;
  int16_t v159;
  gg_type_11 v160;
  int64_t v162;
  v159 = p159;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int16_t)(t1));
  for (t3=0; t3<16; t3++) (v160).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(16));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v162 = t4;
  {
  t6 = gg_view((v160).data, 16);
  t7 = v162;
  t8 = ((int16_t *)t6.data) + gg_index(t7, t6.len);
  (*t8) = ((int16_t)(v159));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v160;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_45(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  int32_t t21;
  int32_t t22;
  int64_t t23;
  int32_t t24;
  int32_t t25;
  int64_t t26;
  int32_t t27;
  int32_t t28;
  int64_t t29;
  int32_t t30;
  int32_t t31;
  int64_t t32;
  int32_t t33;
  int32_t t34;
  int64_t t35;
  int32_t t36;
  int32_t t37;
  int64_t t38;
  int32_t t39;
  int32_t t40;
  int64_t t41;
  int32_t t42;
  int32_t t43;
  int64_t t44;
  int32_t t45;
  int32_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  gg_type_19 * t51;
  gg_slice t52;
  int64_t t53;
  gg_type_18 * t54;
  int64_t t55;
  gg_type_22 * t56;
  gg_slice t57;
  int64_t t58;
  int32_t * t59;
  int16_t t60;
  gg_type_18 t61;
  uint8_t t62;
  gg_type_22 t63;
  gg_type_11 t64;
  gg_slice t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  gg_type_9 * t73;
  gg_slice t74;
  int64_t t75;
  gg_type_8 * t76;
  gg_type_8 t77;
  gg_type_18 t78;
  gg_type_8 t79;
  gg_type_11 t80;
  gg_type_22 t81;
  gg_type_22 t82;
  gg_type_11 t83;
  gg_slice t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  gg_type_9 * t92;
  gg_slice t93;
  int64_t t94;
  gg_type_8 * t95;
  gg_type_8 t96;
  gg_type_18 t97;
  gg_type_8 t98;
  gg_type_11 t99;
  gg_type_22 t100;
  gg_type_22 t101;
  gg_type_11 t102;
  gg_slice t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  gg_type_9 * t111;
  gg_slice t112;
  int64_t t113;
  gg_type_8 * t114;
  gg_type_8 t115;
  gg_type_18 t116;
  gg_type_8 t117;
  gg_type_11 t118;
  gg_type_22 t119;
  gg_type_22 t120;
  gg_type_11 t121;
  gg_slice t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  gg_type_9 * t130;
  gg_slice t131;
  int64_t t132;
  gg_type_8 * t133;
  gg_type_8 t134;
  gg_type_18 t135;
  gg_type_8 t136;
  gg_type_11 t137;
  gg_type_22 t138;
  gg_type_22 t139;
  gg_type_11 t140;
  gg_slice t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  gg_type_9 * t149;
  gg_slice t150;
  int64_t t151;
  gg_type_8 * t152;
  gg_type_8 t153;
  gg_type_18 t154;
  gg_type_8 t155;
  gg_type_11 t156;
  gg_type_22 t157;
  gg_type_22 t158;
  gg_type_11 t159;
  gg_slice t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  gg_type_9 * t168;
  gg_slice t169;
  int64_t t170;
  gg_type_8 * t171;
  gg_type_8 t172;
  gg_type_18 t173;
  gg_type_8 t174;
  gg_type_11 t175;
  gg_type_22 t176;
  gg_type_22 t177;
  gg_type_11 t178;
  gg_slice t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  gg_type_9 * t187;
  gg_slice t188;
  int64_t t189;
  gg_type_8 * t190;
  gg_type_8 t191;
  gg_type_18 t192;
  gg_type_8 t193;
  gg_type_11 t194;
  gg_type_22 t195;
  gg_type_22 t196;
  gg_type_11 t197;
  gg_slice t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  gg_type_9 * t206;
  gg_slice t207;
  int64_t t208;
  gg_type_8 * t209;
  gg_type_8 t210;
  gg_type_18 t211;
  gg_type_8 t212;
  gg_type_11 t213;
  gg_type_22 t214;
  int32_t t215;
  int32_t t216;
  int64_t t217;
  int64_t t218;
  int32_t * t219;
  gg_type_22 t220;
  int64_t t221;
  int32_t * t222;
  gg_type_22 t223;
  int64_t t224;
  int32_t * t225;
  gg_type_22 t226;
  int64_t t227;
  int32_t * t228;
  gg_type_22 t229;
  int64_t t230;
  int32_t * t231;
  gg_type_22 t232;
  int64_t t233;
  int32_t * t234;
  gg_type_22 t235;
  int64_t t236;
  int32_t * t237;
  gg_type_22 t238;
  int64_t t239;
  int32_t * t240;
  gg_type_22 t241;
  gg_type_22 t242;
  gg_slice t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  gg_type_22 * t247;
  gg_slice t248;
  int64_t t249;
  int32_t * t250;
  int32_t t251;
  gg_type_22 t252;
  int64_t t253;
  float * t254;
  float t255;
  gg_type_22 t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  gg_type_26 * t268;
  gg_slice t269;
  int64_t t270;
  float * t271;
  int64_t t272;
  float * t273;
  float t274;
  gg_slice t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  float * t283;
  float t284;
  int64_t t285;
  gg_type_26 * t286;
  gg_slice t287;
  int64_t t288;
  float * t289;
  float t290;
  float t291;
  gg_type_41 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_50 v6;
  gg_type_55 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  gg_type_22 v15;
  gg_type_22 v16;
  gg_type_22 v17;
  gg_type_22 v18;
  gg_type_22 v19;
  gg_type_22 v20;
  gg_type_22 v21;
  gg_type_22 v22;
  gg_type_22 v23;
  int64_t v24;
  gg_type_18 v25;
  gg_type_11 v26;
  gg_type_8 v27;
  gg_type_22 v28;
  gg_type_26 v29;
  int64_t v30;
  int64_t v31;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  v6 = &(((*(v0))).f54);
  v7 = &(((*(v0))).f58);
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  } else {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  }
  t4 = ((int64_t)UINT64_C(16));
  t3 = 0;
label0:;
  if (t3 >= t4) goto label2;
  v9 = t3;
  {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v9));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(v10));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t11 = ((int64_t)(v10));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(v5));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)UINT64_C(2));
  t15 = 0;
label3:;
  if (t15 >= t16) goto label5;
  v13 = t15;
  {
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(v13));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t20)))));
  memset(&t21, 0, sizeof(t21));
  t22 = ((int32_t)(t21));
  for (t23=0; t23<8; t23++) (v15).data[t23] = t22;
  memset(&t24, 0, sizeof(t24));
  t25 = ((int32_t)(t24));
  for (t26=0; t26<8; t26++) (v16).data[t26] = t25;
  memset(&t27, 0, sizeof(t27));
  t28 = ((int32_t)(t27));
  for (t29=0; t29<8; t29++) (v17).data[t29] = t28;
  memset(&t30, 0, sizeof(t30));
  t31 = ((int32_t)(t30));
  for (t32=0; t32<8; t32++) (v18).data[t32] = t31;
  memset(&t33, 0, sizeof(t33));
  t34 = ((int32_t)(t33));
  for (t35=0; t35<8; t35++) (v19).data[t35] = t34;
  memset(&t36, 0, sizeof(t36));
  t37 = ((int32_t)(t36));
  for (t38=0; t38<8; t38++) (v20).data[t38] = t37;
  memset(&t39, 0, sizeof(t39));
  t40 = ((int32_t)(t39));
  for (t41=0; t41<8; t41++) (v21).data[t41] = t40;
  memset(&t42, 0, sizeof(t42));
  t43 = ((int32_t)(t42));
  for (t44=0; t44<8; t44++) (v22).data[t44] = t43;
  memset(&t45, 0, sizeof(t45));
  t46 = ((int32_t)(t45));
  for (t47=0; t47<8; t47++) (v23).data[t47] = t46;
  t49 = ((int64_t)UINT64_C(8));
  t48 = 0;
label6:;
  if (t48 >= t49) goto label8;
  v24 = t48;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t50 = v12;
  t51 = ((gg_type_19 *)(((*(v6))).f49).data) + gg_index(t50, ((int64_t)UINT64_C(256)));
  t52 = gg_view(((*t51)).data, 8);
  t53 = v24;
  t54 = ((gg_type_18 *)t52.data) + gg_index(t53, t52.len);
  v25 = (*t54);
  /* FG_DIRECT_SLICE_INDEX */
  t55 = v12;
  t56 = ((gg_type_22 *)(((*(v6))).f52).data) + gg_index(t55, ((int64_t)UINT64_C(512)));
  t57 = gg_view(((*t56)).data, 8);
  t58 = v24;
  t59 = ((int32_t *)t57.data) + gg_index(t58, t57.len);
  t60 = ((int16_t)(((int16_t)((*t59)))));
  v26 = gg_function_8(t60, base + 0);
  t61 = v25;
  t62 = ((uint8_t)(((int64_t)UINT64_C(128))));
  v27 = gg_function_38(t61, t62, base + 0);
  t63 = v15;
  t64 = v26;
  t65 = gg_view((((*(v7))).f55).data, 256);
  t66 = ((int64_t)(v14));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t69 = ((int64_t)(v4));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t69)))));
  t71 = ((int64_t)(v5));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((gg_type_9 *)t65.data) + gg_index(t72, t65.len);
  t74 = gg_view(((*t73)).data, 8);
  t75 = v24;
  t76 = ((gg_type_8 *)t74.data) + gg_index(t75, t74.len);
  t77 = (*t76);
  t78 = gg_function_39(t77, base + 0);
  t79 = v27;
  t80 = gg_function_22(t78, t79, base + 0);
  t81 = gg_function_23(t64, t80, base + 0);
  v15 = gg_function_29(t63, t81, base + 0);
  t82 = v16;
  t83 = v26;
  t84 = gg_view((((*(v7))).f55).data, 256);
  t85 = ((int64_t)(v14));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t85) + (uint64_t)(t86)))));
  t88 = ((int64_t)(v4));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t87) * (uint64_t)(t88)))));
  t90 = ((int64_t)(v5));
  t91 = ((int64_t)((uint64_t)(t89) + (uint64_t)(t90)));
  t92 = ((gg_type_9 *)t84.data) + gg_index(t91, t84.len);
  t93 = gg_view(((*t92)).data, 8);
  t94 = v24;
  t95 = ((gg_type_8 *)t93.data) + gg_index(t94, t93.len);
  t96 = (*t95);
  t97 = gg_function_39(t96, base + 0);
  t98 = v27;
  t99 = gg_function_22(t97, t98, base + 0);
  t100 = gg_function_23(t83, t99, base + 0);
  v16 = gg_function_29(t82, t100, base + 0);
  t101 = v17;
  t102 = v26;
  t103 = gg_view((((*(v7))).f55).data, 256);
  t104 = ((int64_t)(v14));
  t105 = ((int64_t)(((int64_t)UINT64_C(2))));
  t106 = ((int64_t)(((int64_t)((uint64_t)(t104) + (uint64_t)(t105)))));
  t107 = ((int64_t)(v4));
  t108 = ((int64_t)(((int64_t)((uint64_t)(t106) * (uint64_t)(t107)))));
  t109 = ((int64_t)(v5));
  t110 = ((int64_t)((uint64_t)(t108) + (uint64_t)(t109)));
  t111 = ((gg_type_9 *)t103.data) + gg_index(t110, t103.len);
  t112 = gg_view(((*t111)).data, 8);
  t113 = v24;
  t114 = ((gg_type_8 *)t112.data) + gg_index(t113, t112.len);
  t115 = (*t114);
  t116 = gg_function_39(t115, base + 0);
  t117 = v27;
  t118 = gg_function_22(t116, t117, base + 0);
  t119 = gg_function_23(t102, t118, base + 0);
  v17 = gg_function_29(t101, t119, base + 0);
  t120 = v18;
  t121 = v26;
  t122 = gg_view((((*(v7))).f55).data, 256);
  t123 = ((int64_t)(v14));
  t124 = ((int64_t)(((int64_t)UINT64_C(3))));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) + (uint64_t)(t124)))));
  t126 = ((int64_t)(v4));
  t127 = ((int64_t)(((int64_t)((uint64_t)(t125) * (uint64_t)(t126)))));
  t128 = ((int64_t)(v5));
  t129 = ((int64_t)((uint64_t)(t127) + (uint64_t)(t128)));
  t130 = ((gg_type_9 *)t122.data) + gg_index(t129, t122.len);
  t131 = gg_view(((*t130)).data, 8);
  t132 = v24;
  t133 = ((gg_type_8 *)t131.data) + gg_index(t132, t131.len);
  t134 = (*t133);
  t135 = gg_function_39(t134, base + 0);
  t136 = v27;
  t137 = gg_function_22(t135, t136, base + 0);
  t138 = gg_function_23(t121, t137, base + 0);
  v18 = gg_function_29(t120, t138, base + 0);
  t139 = v19;
  t140 = v26;
  t141 = gg_view((((*(v7))).f55).data, 256);
  t142 = ((int64_t)(v14));
  t143 = ((int64_t)(((int64_t)UINT64_C(4))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t142) + (uint64_t)(t143)))));
  t145 = ((int64_t)(v4));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t145)))));
  t147 = ((int64_t)(v5));
  t148 = ((int64_t)((uint64_t)(t146) + (uint64_t)(t147)));
  t149 = ((gg_type_9 *)t141.data) + gg_index(t148, t141.len);
  t150 = gg_view(((*t149)).data, 8);
  t151 = v24;
  t152 = ((gg_type_8 *)t150.data) + gg_index(t151, t150.len);
  t153 = (*t152);
  t154 = gg_function_39(t153, base + 0);
  t155 = v27;
  t156 = gg_function_22(t154, t155, base + 0);
  t157 = gg_function_23(t140, t156, base + 0);
  v19 = gg_function_29(t139, t157, base + 0);
  t158 = v20;
  t159 = v26;
  t160 = gg_view((((*(v7))).f55).data, 256);
  t161 = ((int64_t)(v14));
  t162 = ((int64_t)(((int64_t)UINT64_C(5))));
  t163 = ((int64_t)(((int64_t)((uint64_t)(t161) + (uint64_t)(t162)))));
  t164 = ((int64_t)(v4));
  t165 = ((int64_t)(((int64_t)((uint64_t)(t163) * (uint64_t)(t164)))));
  t166 = ((int64_t)(v5));
  t167 = ((int64_t)((uint64_t)(t165) + (uint64_t)(t166)));
  t168 = ((gg_type_9 *)t160.data) + gg_index(t167, t160.len);
  t169 = gg_view(((*t168)).data, 8);
  t170 = v24;
  t171 = ((gg_type_8 *)t169.data) + gg_index(t170, t169.len);
  t172 = (*t171);
  t173 = gg_function_39(t172, base + 0);
  t174 = v27;
  t175 = gg_function_22(t173, t174, base + 0);
  t176 = gg_function_23(t159, t175, base + 0);
  v20 = gg_function_29(t158, t176, base + 0);
  t177 = v21;
  t178 = v26;
  t179 = gg_view((((*(v7))).f55).data, 256);
  t180 = ((int64_t)(v14));
  t181 = ((int64_t)(((int64_t)UINT64_C(6))));
  t182 = ((int64_t)(((int64_t)((uint64_t)(t180) + (uint64_t)(t181)))));
  t183 = ((int64_t)(v4));
  t184 = ((int64_t)(((int64_t)((uint64_t)(t182) * (uint64_t)(t183)))));
  t185 = ((int64_t)(v5));
  t186 = ((int64_t)((uint64_t)(t184) + (uint64_t)(t185)));
  t187 = ((gg_type_9 *)t179.data) + gg_index(t186, t179.len);
  t188 = gg_view(((*t187)).data, 8);
  t189 = v24;
  t190 = ((gg_type_8 *)t188.data) + gg_index(t189, t188.len);
  t191 = (*t190);
  t192 = gg_function_39(t191, base + 0);
  t193 = v27;
  t194 = gg_function_22(t192, t193, base + 0);
  t195 = gg_function_23(t178, t194, base + 0);
  v21 = gg_function_29(t177, t195, base + 0);
  t196 = v22;
  t197 = v26;
  t198 = gg_view((((*(v7))).f55).data, 256);
  t199 = ((int64_t)(v14));
  t200 = ((int64_t)(((int64_t)UINT64_C(7))));
  t201 = ((int64_t)(((int64_t)((uint64_t)(t199) + (uint64_t)(t200)))));
  t202 = ((int64_t)(v4));
  t203 = ((int64_t)(((int64_t)((uint64_t)(t201) * (uint64_t)(t202)))));
  t204 = ((int64_t)(v5));
  t205 = ((int64_t)((uint64_t)(t203) + (uint64_t)(t204)));
  t206 = ((gg_type_9 *)t198.data) + gg_index(t205, t198.len);
  t207 = gg_view(((*t206)).data, 8);
  t208 = v24;
  t209 = ((gg_type_8 *)t207.data) + gg_index(t208, t207.len);
  t210 = (*t209);
  t211 = gg_function_39(t210, base + 0);
  t212 = v27;
  t213 = gg_function_22(t211, t212, base + 0);
  t214 = gg_function_23(t197, t213, base + 0);
  v22 = gg_function_29(t196, t214, base + 0);
  }
label7:;
  t48++; goto label6;
label8:;
  memset(&t215, 0, sizeof(t215));
  t216 = ((int32_t)(t215));
  for (t217=0; t217<8; t217++) (v28).data[t217] = t216;
  /* FG_DIRECT_SLICE_INDEX */
  t218 = ((int64_t)UINT64_C(0));
  t219 = ((int32_t *)(v28).data) + gg_index(t218, ((int64_t)UINT64_C(8)));
  t220 = v15;
  (*t219) = gg_function_24(t220, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t221 = ((int64_t)UINT64_C(1));
  t222 = ((int32_t *)(v28).data) + gg_index(t221, ((int64_t)UINT64_C(8)));
  t223 = v16;
  (*t222) = gg_function_24(t223, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t224 = ((int64_t)UINT64_C(2));
  t225 = ((int32_t *)(v28).data) + gg_index(t224, ((int64_t)UINT64_C(8)));
  t226 = v17;
  (*t225) = gg_function_24(t226, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t227 = ((int64_t)UINT64_C(3));
  t228 = ((int32_t *)(v28).data) + gg_index(t227, ((int64_t)UINT64_C(8)));
  t229 = v18;
  (*t228) = gg_function_24(t229, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t230 = ((int64_t)UINT64_C(4));
  t231 = ((int32_t *)(v28).data) + gg_index(t230, ((int64_t)UINT64_C(8)));
  t232 = v19;
  (*t231) = gg_function_24(t232, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t233 = ((int64_t)UINT64_C(5));
  t234 = ((int32_t *)(v28).data) + gg_index(t233, ((int64_t)UINT64_C(8)));
  t235 = v20;
  (*t234) = gg_function_24(t235, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t236 = ((int64_t)UINT64_C(6));
  t237 = ((int32_t *)(v28).data) + gg_index(t236, ((int64_t)UINT64_C(8)));
  t238 = v21;
  (*t237) = gg_function_24(t238, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t239 = ((int64_t)UINT64_C(7));
  t240 = ((int32_t *)(v28).data) + gg_index(t239, ((int64_t)UINT64_C(8)));
  t241 = v22;
  (*t240) = gg_function_24(t241, base + 0);
  t242 = v28;
  t243 = gg_view((((*(v6))).f53).data, 512);
  t244 = ((int64_t)(v12));
  t245 = ((int64_t)(((int64_t)UINT64_C(1))));
  t246 = ((int64_t)((uint64_t)(t244) * (uint64_t)(t245)));
  t247 = ((gg_type_22 *)t243.data) + gg_index(t246, t243.len);
  t248 = gg_view(((*t247)).data, 8);
  t249 = ((int64_t)UINT64_C(0));
  t250 = ((int32_t *)t248.data) + gg_index(t249, t248.len);
  t251 = ((int32_t)((*t250)));
  t252 = gg_function_30(t251, base + 0);
  v28 = gg_function_40(t242, t252, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t253 = v11;
  t254 = ((float *)(((*(v6))).f50).data) + gg_index(t253, ((int64_t)UINT64_C(256)));
  t255 = ((float)((*t254)));
  t256 = v28;
  v29 = gg_function_32(t255, t256, base + 0);
  t257 = ((int64_t)(v3));
  t258 = ((int64_t)(v9));
  t259 = ((int64_t)(((int64_t)((uint64_t)(t257) + (uint64_t)(t258)))));
  t260 = ((int64_t)(((int64_t)UINT64_C(32))));
  t261 = ((int64_t)(((int64_t)((uint64_t)(t259) * (uint64_t)(t260)))));
  t262 = ((int64_t)(v14));
  t263 = ((int64_t)(((int64_t)UINT64_C(8))));
  t264 = ((int64_t)(((int64_t)gg_div(t262, t263))));
  v30 = ((int64_t)(((int64_t)((uint64_t)(t261) + (uint64_t)(t264)))));
  t266 = ((int64_t)UINT64_C(8));
  t265 = 0;
label9:;
  if (t265 >= t266) goto label11;
  v31 = t265;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t267 = v30;
  t268 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t267, ((int64_t)UINT64_C(8192)));
  t269 = gg_view(((*t268)).data, 8);
  t270 = v31;
  t271 = ((float *)t269.data) + gg_index(t270, t269.len);
  /* FG_DIRECT_SLICE_INDEX */
  t272 = v31;
  t273 = ((float *)(v29).data) + gg_index(t272, ((int64_t)UINT64_C(8)));
  t274 = ((float)((*t273)));
  t275 = gg_view((((*(v7))).f57).data, 256);
  t276 = ((int64_t)(v5));
  t277 = ((int64_t)(((int64_t)UINT64_C(16))));
  t278 = ((int64_t)(((int64_t)((uint64_t)(t276) * (uint64_t)(t277)))));
  t279 = ((int64_t)(v14));
  t280 = ((int64_t)(((int64_t)((uint64_t)(t278) + (uint64_t)(t279)))));
  t281 = ((int64_t)(v31));
  t282 = ((int64_t)((uint64_t)(t280) + (uint64_t)(t281)));
  t283 = ((float *)t275.data) + gg_index(t282, t275.len);
  t284 = ((float)((*t283)));
  /* FG_DIRECT_SLICE_INDEX */
  t285 = v30;
  t286 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t285, ((int64_t)UINT64_C(8192)));
  t287 = gg_view(((*t286)).data, 8);
  t288 = v31;
  t289 = ((float *)t287.data) + gg_index(t288, t287.len);
  t290 = ((float)((*t289)));
  t291 = fmaf(t274, t284, t290);
  (*t271) = ((float)(t291));
  }
label10:;
  t265++; goto label9;
label11:;
  }
label4:;
  t15++; goto label3;
label5:;
  }
label1:;
  t3++; goto label0;
label2:;
  }
gg_done:;
}
static gg_type_11 gg_function_44(gg_type_8 p0, gg_type_8 p1, int64_t base) {
  gg_type_11 t0;
  uint8_t t1;
  uint8_t t2;
  int64_t t3;
  int8_t t4;
  int8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int8_t * t10;
  int64_t t11;
  int8_t * t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  uint8_t t16;
  uint8_t t17;
  uint8_t t18;
  uint8_t t19;
  uint8_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  uint8_t t24;
  int64_t t25;
  uint8_t * t26;
  uint8_t t27;
  uint8_t t28;
  uint8_t t29;
  uint8_t t30;
  int64_t t31;
  int8_t * t32;
  uint8_t t33;
  uint8_t t34;
  uint8_t t35;
  uint8_t t36;
  uint8_t t37;
  uint8_t t38;
  gg_type_18 t39;
  gg_type_8 t40;
  gg_type_8 v0;
  gg_type_8 v1;
  gg_type_18 v2;
  gg_type_8 v3;
  int64_t v4;
  uint8_t v5;
  uint8_t v6;
  uint8_t v7;
  uint8_t v8;
  uint8_t v9;
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
  t10 = ((int8_t *)(v0).data) + gg_index(t9, ((int64_t)UINT64_C(32)));
  v5 = ((uint8_t)(((uint8_t)((*t10)))));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v4;
  t12 = ((int8_t *)(v1).data) + gg_index(t11, ((int64_t)UINT64_C(32)));
  v6 = ((uint8_t)(((uint8_t)((*t12)))));
  t13 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(0))))));
  t14 = ((uint8_t)(v5));
  t15 = ((int64_t)(((int64_t)UINT64_C(7))));
  t16 = ((uint8_t)(((t14) >> ((uint64_t)(t15) & 7))));
  v7 = ((uint8_t)(((uint8_t)(((uint8_t)((uint64_t)(t13) - (uint64_t)(t16)))))));
  t17 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(0))))));
  t18 = ((uint8_t)(v5));
  v8 = ((uint8_t)(((uint8_t)(((uint8_t)((uint64_t)(t17) - (uint64_t)(t18)))))));
  t19 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(0))))));
  t20 = ((uint8_t)(v5));
  t21 = ((uint8_t)(v8));
  t22 = ((uint8_t)(((t20) | (t21))));
  t23 = ((int64_t)(((int64_t)UINT64_C(7))));
  t24 = ((uint8_t)(((t22) >> ((uint64_t)(t23) & 7))));
  v9 = ((uint8_t)(((uint8_t)(((uint8_t)((uint64_t)(t19) - (uint64_t)(t24)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v4;
  t26 = ((uint8_t *)(v2).data) + gg_index(t25, ((int64_t)UINT64_C(32)));
  t27 = ((uint8_t)(v5));
  t28 = ((uint8_t)(v7));
  t29 = ((uint8_t)(((t27) ^ (t28))));
  t30 = ((uint8_t)(v7));
  (*t26) = ((uint8_t)(((uint8_t)(((uint8_t)((uint64_t)(t29) - (uint64_t)(t30)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t31 = v4;
  t32 = ((int8_t *)(v3).data) + gg_index(t31, ((int64_t)UINT64_C(32)));
  t33 = ((uint8_t)(v6));
  t34 = ((uint8_t)(v7));
  t35 = ((uint8_t)(((t33) ^ (t34))));
  t36 = ((uint8_t)(v7));
  t37 = ((uint8_t)(((uint8_t)(((uint8_t)((uint64_t)(t35) - (uint64_t)(t36)))))));
  t38 = ((uint8_t)(v9));
  (*t32) = ((int8_t)(((int8_t)(((t37) & (t38))))));
  }
label1:;
  t7++; goto label0;
label2:;
  t39 = v2;
  t40 = v3;
  t0 = gg_function_22(t39, t40, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_43(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  int32_t t21;
  int32_t t22;
  int64_t t23;
  int32_t t24;
  int32_t t25;
  int64_t t26;
  int32_t t27;
  int32_t t28;
  int64_t t29;
  int32_t t30;
  int32_t t31;
  int64_t t32;
  int32_t t33;
  int32_t t34;
  int64_t t35;
  int32_t t36;
  int32_t t37;
  int64_t t38;
  int32_t t39;
  int32_t t40;
  int64_t t41;
  int32_t t42;
  int32_t t43;
  int64_t t44;
  int32_t t45;
  int32_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  gg_type_19 * t51;
  gg_slice t52;
  int64_t t53;
  gg_type_18 * t54;
  int64_t t55;
  gg_type_22 * t56;
  gg_slice t57;
  int64_t t58;
  int32_t * t59;
  int16_t t60;
  gg_type_18 t61;
  uint8_t t62;
  gg_type_22 t63;
  gg_type_11 t64;
  gg_type_8 t65;
  gg_slice t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  gg_type_9 * t74;
  gg_slice t75;
  int64_t t76;
  gg_type_8 * t77;
  gg_type_8 t78;
  gg_type_11 t79;
  gg_type_22 t80;
  gg_type_22 t81;
  gg_type_11 t82;
  gg_type_8 t83;
  gg_slice t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  gg_type_9 * t92;
  gg_slice t93;
  int64_t t94;
  gg_type_8 * t95;
  gg_type_8 t96;
  gg_type_11 t97;
  gg_type_22 t98;
  gg_type_22 t99;
  gg_type_11 t100;
  gg_type_8 t101;
  gg_slice t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  gg_type_9 * t110;
  gg_slice t111;
  int64_t t112;
  gg_type_8 * t113;
  gg_type_8 t114;
  gg_type_11 t115;
  gg_type_22 t116;
  gg_type_22 t117;
  gg_type_11 t118;
  gg_type_8 t119;
  gg_slice t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  gg_type_9 * t128;
  gg_slice t129;
  int64_t t130;
  gg_type_8 * t131;
  gg_type_8 t132;
  gg_type_11 t133;
  gg_type_22 t134;
  gg_type_22 t135;
  gg_type_11 t136;
  gg_type_8 t137;
  gg_slice t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  gg_type_9 * t146;
  gg_slice t147;
  int64_t t148;
  gg_type_8 * t149;
  gg_type_8 t150;
  gg_type_11 t151;
  gg_type_22 t152;
  gg_type_22 t153;
  gg_type_11 t154;
  gg_type_8 t155;
  gg_slice t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  gg_type_9 * t164;
  gg_slice t165;
  int64_t t166;
  gg_type_8 * t167;
  gg_type_8 t168;
  gg_type_11 t169;
  gg_type_22 t170;
  gg_type_22 t171;
  gg_type_11 t172;
  gg_type_8 t173;
  gg_slice t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  gg_type_9 * t182;
  gg_slice t183;
  int64_t t184;
  gg_type_8 * t185;
  gg_type_8 t186;
  gg_type_11 t187;
  gg_type_22 t188;
  gg_type_22 t189;
  gg_type_11 t190;
  gg_type_8 t191;
  gg_slice t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  gg_type_9 * t200;
  gg_slice t201;
  int64_t t202;
  gg_type_8 * t203;
  gg_type_8 t204;
  gg_type_11 t205;
  gg_type_22 t206;
  int32_t t207;
  int32_t t208;
  int64_t t209;
  int64_t t210;
  int32_t * t211;
  gg_type_22 t212;
  int64_t t213;
  int32_t * t214;
  gg_type_22 t215;
  int64_t t216;
  int32_t * t217;
  gg_type_22 t218;
  int64_t t219;
  int32_t * t220;
  gg_type_22 t221;
  int64_t t222;
  int32_t * t223;
  gg_type_22 t224;
  int64_t t225;
  int32_t * t226;
  gg_type_22 t227;
  int64_t t228;
  int32_t * t229;
  gg_type_22 t230;
  int64_t t231;
  int32_t * t232;
  gg_type_22 t233;
  int64_t t234;
  float * t235;
  float t236;
  gg_type_22 t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  gg_type_26 * t249;
  gg_slice t250;
  int64_t t251;
  float * t252;
  int64_t t253;
  float * t254;
  float t255;
  gg_slice t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  float * t264;
  float t265;
  int64_t t266;
  gg_type_26 * t267;
  gg_slice t268;
  int64_t t269;
  float * t270;
  float t271;
  float t272;
  gg_type_41 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_50 v6;
  gg_type_55 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  gg_type_22 v15;
  gg_type_22 v16;
  gg_type_22 v17;
  gg_type_22 v18;
  gg_type_22 v19;
  gg_type_22 v20;
  gg_type_22 v21;
  gg_type_22 v22;
  gg_type_22 v23;
  int64_t v24;
  gg_type_18 v25;
  gg_type_11 v26;
  gg_type_8 v27;
  gg_type_22 v28;
  gg_type_26 v29;
  int64_t v30;
  int64_t v31;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  v6 = &(((*(v0))).f54);
  v7 = &(((*(v0))).f58);
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  } else {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  }
  t4 = ((int64_t)UINT64_C(16));
  t3 = 0;
label0:;
  if (t3 >= t4) goto label2;
  v9 = t3;
  {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v9));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(v10));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t11 = ((int64_t)(v10));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(v5));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)UINT64_C(2));
  t15 = 0;
label3:;
  if (t15 >= t16) goto label5;
  v13 = t15;
  {
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(v13));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t20)))));
  memset(&t21, 0, sizeof(t21));
  t22 = ((int32_t)(t21));
  for (t23=0; t23<8; t23++) (v15).data[t23] = t22;
  memset(&t24, 0, sizeof(t24));
  t25 = ((int32_t)(t24));
  for (t26=0; t26<8; t26++) (v16).data[t26] = t25;
  memset(&t27, 0, sizeof(t27));
  t28 = ((int32_t)(t27));
  for (t29=0; t29<8; t29++) (v17).data[t29] = t28;
  memset(&t30, 0, sizeof(t30));
  t31 = ((int32_t)(t30));
  for (t32=0; t32<8; t32++) (v18).data[t32] = t31;
  memset(&t33, 0, sizeof(t33));
  t34 = ((int32_t)(t33));
  for (t35=0; t35<8; t35++) (v19).data[t35] = t34;
  memset(&t36, 0, sizeof(t36));
  t37 = ((int32_t)(t36));
  for (t38=0; t38<8; t38++) (v20).data[t38] = t37;
  memset(&t39, 0, sizeof(t39));
  t40 = ((int32_t)(t39));
  for (t41=0; t41<8; t41++) (v21).data[t41] = t40;
  memset(&t42, 0, sizeof(t42));
  t43 = ((int32_t)(t42));
  for (t44=0; t44<8; t44++) (v22).data[t44] = t43;
  memset(&t45, 0, sizeof(t45));
  t46 = ((int32_t)(t45));
  for (t47=0; t47<8; t47++) (v23).data[t47] = t46;
  t49 = ((int64_t)UINT64_C(8));
  t48 = 0;
label6:;
  if (t48 >= t49) goto label8;
  v24 = t48;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t50 = v12;
  t51 = ((gg_type_19 *)(((*(v6))).f49).data) + gg_index(t50, ((int64_t)UINT64_C(256)));
  t52 = gg_view(((*t51)).data, 8);
  t53 = v24;
  t54 = ((gg_type_18 *)t52.data) + gg_index(t53, t52.len);
  v25 = (*t54);
  /* FG_DIRECT_SLICE_INDEX */
  t55 = v12;
  t56 = ((gg_type_22 *)(((*(v6))).f52).data) + gg_index(t55, ((int64_t)UINT64_C(512)));
  t57 = gg_view(((*t56)).data, 8);
  t58 = v24;
  t59 = ((int32_t *)t57.data) + gg_index(t58, t57.len);
  t60 = ((int16_t)(((int16_t)((*t59)))));
  v26 = gg_function_8(t60, base + 0);
  t61 = v25;
  t62 = ((uint8_t)(((int64_t)UINT64_C(128))));
  v27 = gg_function_38(t61, t62, base + 0);
  t63 = v15;
  t64 = v26;
  t65 = v27;
  t66 = gg_view((((*(v7))).f55).data, 256);
  t67 = ((int64_t)(v14));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  t70 = ((int64_t)(v4));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((int64_t)(v5));
  t73 = ((int64_t)((uint64_t)(t71) + (uint64_t)(t72)));
  t74 = ((gg_type_9 *)t66.data) + gg_index(t73, t66.len);
  t75 = gg_view(((*t74)).data, 8);
  t76 = v24;
  t77 = ((gg_type_8 *)t75.data) + gg_index(t76, t75.len);
  t78 = (*t77);
  t79 = gg_function_44(t65, t78, base + 0);
  t80 = gg_function_23(t64, t79, base + 0);
  v15 = gg_function_29(t63, t80, base + 0);
  t81 = v16;
  t82 = v26;
  t83 = v27;
  t84 = gg_view((((*(v7))).f55).data, 256);
  t85 = ((int64_t)(v14));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t85) + (uint64_t)(t86)))));
  t88 = ((int64_t)(v4));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t87) * (uint64_t)(t88)))));
  t90 = ((int64_t)(v5));
  t91 = ((int64_t)((uint64_t)(t89) + (uint64_t)(t90)));
  t92 = ((gg_type_9 *)t84.data) + gg_index(t91, t84.len);
  t93 = gg_view(((*t92)).data, 8);
  t94 = v24;
  t95 = ((gg_type_8 *)t93.data) + gg_index(t94, t93.len);
  t96 = (*t95);
  t97 = gg_function_44(t83, t96, base + 0);
  t98 = gg_function_23(t82, t97, base + 0);
  v16 = gg_function_29(t81, t98, base + 0);
  t99 = v17;
  t100 = v26;
  t101 = v27;
  t102 = gg_view((((*(v7))).f55).data, 256);
  t103 = ((int64_t)(v14));
  t104 = ((int64_t)(((int64_t)UINT64_C(2))));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t103) + (uint64_t)(t104)))));
  t106 = ((int64_t)(v4));
  t107 = ((int64_t)(((int64_t)((uint64_t)(t105) * (uint64_t)(t106)))));
  t108 = ((int64_t)(v5));
  t109 = ((int64_t)((uint64_t)(t107) + (uint64_t)(t108)));
  t110 = ((gg_type_9 *)t102.data) + gg_index(t109, t102.len);
  t111 = gg_view(((*t110)).data, 8);
  t112 = v24;
  t113 = ((gg_type_8 *)t111.data) + gg_index(t112, t111.len);
  t114 = (*t113);
  t115 = gg_function_44(t101, t114, base + 0);
  t116 = gg_function_23(t100, t115, base + 0);
  v17 = gg_function_29(t99, t116, base + 0);
  t117 = v18;
  t118 = v26;
  t119 = v27;
  t120 = gg_view((((*(v7))).f55).data, 256);
  t121 = ((int64_t)(v14));
  t122 = ((int64_t)(((int64_t)UINT64_C(3))));
  t123 = ((int64_t)(((int64_t)((uint64_t)(t121) + (uint64_t)(t122)))));
  t124 = ((int64_t)(v4));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) * (uint64_t)(t124)))));
  t126 = ((int64_t)(v5));
  t127 = ((int64_t)((uint64_t)(t125) + (uint64_t)(t126)));
  t128 = ((gg_type_9 *)t120.data) + gg_index(t127, t120.len);
  t129 = gg_view(((*t128)).data, 8);
  t130 = v24;
  t131 = ((gg_type_8 *)t129.data) + gg_index(t130, t129.len);
  t132 = (*t131);
  t133 = gg_function_44(t119, t132, base + 0);
  t134 = gg_function_23(t118, t133, base + 0);
  v18 = gg_function_29(t117, t134, base + 0);
  t135 = v19;
  t136 = v26;
  t137 = v27;
  t138 = gg_view((((*(v7))).f55).data, 256);
  t139 = ((int64_t)(v14));
  t140 = ((int64_t)(((int64_t)UINT64_C(4))));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t139) + (uint64_t)(t140)))));
  t142 = ((int64_t)(v4));
  t143 = ((int64_t)(((int64_t)((uint64_t)(t141) * (uint64_t)(t142)))));
  t144 = ((int64_t)(v5));
  t145 = ((int64_t)((uint64_t)(t143) + (uint64_t)(t144)));
  t146 = ((gg_type_9 *)t138.data) + gg_index(t145, t138.len);
  t147 = gg_view(((*t146)).data, 8);
  t148 = v24;
  t149 = ((gg_type_8 *)t147.data) + gg_index(t148, t147.len);
  t150 = (*t149);
  t151 = gg_function_44(t137, t150, base + 0);
  t152 = gg_function_23(t136, t151, base + 0);
  v19 = gg_function_29(t135, t152, base + 0);
  t153 = v20;
  t154 = v26;
  t155 = v27;
  t156 = gg_view((((*(v7))).f55).data, 256);
  t157 = ((int64_t)(v14));
  t158 = ((int64_t)(((int64_t)UINT64_C(5))));
  t159 = ((int64_t)(((int64_t)((uint64_t)(t157) + (uint64_t)(t158)))));
  t160 = ((int64_t)(v4));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t159) * (uint64_t)(t160)))));
  t162 = ((int64_t)(v5));
  t163 = ((int64_t)((uint64_t)(t161) + (uint64_t)(t162)));
  t164 = ((gg_type_9 *)t156.data) + gg_index(t163, t156.len);
  t165 = gg_view(((*t164)).data, 8);
  t166 = v24;
  t167 = ((gg_type_8 *)t165.data) + gg_index(t166, t165.len);
  t168 = (*t167);
  t169 = gg_function_44(t155, t168, base + 0);
  t170 = gg_function_23(t154, t169, base + 0);
  v20 = gg_function_29(t153, t170, base + 0);
  t171 = v21;
  t172 = v26;
  t173 = v27;
  t174 = gg_view((((*(v7))).f55).data, 256);
  t175 = ((int64_t)(v14));
  t176 = ((int64_t)(((int64_t)UINT64_C(6))));
  t177 = ((int64_t)(((int64_t)((uint64_t)(t175) + (uint64_t)(t176)))));
  t178 = ((int64_t)(v4));
  t179 = ((int64_t)(((int64_t)((uint64_t)(t177) * (uint64_t)(t178)))));
  t180 = ((int64_t)(v5));
  t181 = ((int64_t)((uint64_t)(t179) + (uint64_t)(t180)));
  t182 = ((gg_type_9 *)t174.data) + gg_index(t181, t174.len);
  t183 = gg_view(((*t182)).data, 8);
  t184 = v24;
  t185 = ((gg_type_8 *)t183.data) + gg_index(t184, t183.len);
  t186 = (*t185);
  t187 = gg_function_44(t173, t186, base + 0);
  t188 = gg_function_23(t172, t187, base + 0);
  v21 = gg_function_29(t171, t188, base + 0);
  t189 = v22;
  t190 = v26;
  t191 = v27;
  t192 = gg_view((((*(v7))).f55).data, 256);
  t193 = ((int64_t)(v14));
  t194 = ((int64_t)(((int64_t)UINT64_C(7))));
  t195 = ((int64_t)(((int64_t)((uint64_t)(t193) + (uint64_t)(t194)))));
  t196 = ((int64_t)(v4));
  t197 = ((int64_t)(((int64_t)((uint64_t)(t195) * (uint64_t)(t196)))));
  t198 = ((int64_t)(v5));
  t199 = ((int64_t)((uint64_t)(t197) + (uint64_t)(t198)));
  t200 = ((gg_type_9 *)t192.data) + gg_index(t199, t192.len);
  t201 = gg_view(((*t200)).data, 8);
  t202 = v24;
  t203 = ((gg_type_8 *)t201.data) + gg_index(t202, t201.len);
  t204 = (*t203);
  t205 = gg_function_44(t191, t204, base + 0);
  t206 = gg_function_23(t190, t205, base + 0);
  v22 = gg_function_29(t189, t206, base + 0);
  }
label7:;
  t48++; goto label6;
label8:;
  memset(&t207, 0, sizeof(t207));
  t208 = ((int32_t)(t207));
  for (t209=0; t209<8; t209++) (v28).data[t209] = t208;
  /* FG_DIRECT_SLICE_INDEX */
  t210 = ((int64_t)UINT64_C(0));
  t211 = ((int32_t *)(v28).data) + gg_index(t210, ((int64_t)UINT64_C(8)));
  t212 = v15;
  (*t211) = gg_function_24(t212, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t213 = ((int64_t)UINT64_C(1));
  t214 = ((int32_t *)(v28).data) + gg_index(t213, ((int64_t)UINT64_C(8)));
  t215 = v16;
  (*t214) = gg_function_24(t215, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t216 = ((int64_t)UINT64_C(2));
  t217 = ((int32_t *)(v28).data) + gg_index(t216, ((int64_t)UINT64_C(8)));
  t218 = v17;
  (*t217) = gg_function_24(t218, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t219 = ((int64_t)UINT64_C(3));
  t220 = ((int32_t *)(v28).data) + gg_index(t219, ((int64_t)UINT64_C(8)));
  t221 = v18;
  (*t220) = gg_function_24(t221, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t222 = ((int64_t)UINT64_C(4));
  t223 = ((int32_t *)(v28).data) + gg_index(t222, ((int64_t)UINT64_C(8)));
  t224 = v19;
  (*t223) = gg_function_24(t224, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t225 = ((int64_t)UINT64_C(5));
  t226 = ((int32_t *)(v28).data) + gg_index(t225, ((int64_t)UINT64_C(8)));
  t227 = v20;
  (*t226) = gg_function_24(t227, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t228 = ((int64_t)UINT64_C(6));
  t229 = ((int32_t *)(v28).data) + gg_index(t228, ((int64_t)UINT64_C(8)));
  t230 = v21;
  (*t229) = gg_function_24(t230, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t231 = ((int64_t)UINT64_C(7));
  t232 = ((int32_t *)(v28).data) + gg_index(t231, ((int64_t)UINT64_C(8)));
  t233 = v22;
  (*t232) = gg_function_24(t233, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t234 = v11;
  t235 = ((float *)(((*(v6))).f50).data) + gg_index(t234, ((int64_t)UINT64_C(256)));
  t236 = ((float)((*t235)));
  t237 = v28;
  v29 = gg_function_32(t236, t237, base + 0);
  t238 = ((int64_t)(v3));
  t239 = ((int64_t)(v9));
  t240 = ((int64_t)(((int64_t)((uint64_t)(t238) + (uint64_t)(t239)))));
  t241 = ((int64_t)(((int64_t)UINT64_C(32))));
  t242 = ((int64_t)(((int64_t)((uint64_t)(t240) * (uint64_t)(t241)))));
  t243 = ((int64_t)(v14));
  t244 = ((int64_t)(((int64_t)UINT64_C(8))));
  t245 = ((int64_t)(((int64_t)gg_div(t243, t244))));
  v30 = ((int64_t)(((int64_t)((uint64_t)(t242) + (uint64_t)(t245)))));
  t247 = ((int64_t)UINT64_C(8));
  t246 = 0;
label9:;
  if (t246 >= t247) goto label11;
  v31 = t246;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t248 = v30;
  t249 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t248, ((int64_t)UINT64_C(8192)));
  t250 = gg_view(((*t249)).data, 8);
  t251 = v31;
  t252 = ((float *)t250.data) + gg_index(t251, t250.len);
  /* FG_DIRECT_SLICE_INDEX */
  t253 = v31;
  t254 = ((float *)(v29).data) + gg_index(t253, ((int64_t)UINT64_C(8)));
  t255 = ((float)((*t254)));
  t256 = gg_view((((*(v7))).f57).data, 256);
  t257 = ((int64_t)(v5));
  t258 = ((int64_t)(((int64_t)UINT64_C(16))));
  t259 = ((int64_t)(((int64_t)((uint64_t)(t257) * (uint64_t)(t258)))));
  t260 = ((int64_t)(v14));
  t261 = ((int64_t)(((int64_t)((uint64_t)(t259) + (uint64_t)(t260)))));
  t262 = ((int64_t)(v31));
  t263 = ((int64_t)((uint64_t)(t261) + (uint64_t)(t262)));
  t264 = ((float *)t256.data) + gg_index(t263, t256.len);
  t265 = ((float)((*t264)));
  /* FG_DIRECT_SLICE_INDEX */
  t266 = v30;
  t267 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t266, ((int64_t)UINT64_C(8192)));
  t268 = gg_view(((*t267)).data, 8);
  t269 = v31;
  t270 = ((float *)t268.data) + gg_index(t269, t268.len);
  t271 = ((float)((*t270)));
  t272 = fmaf(t255, t265, t271);
  (*t252) = ((float)(t272));
  }
label10:;
  t246++; goto label9;
label11:;
  }
label4:;
  t15++; goto label3;
label5:;
  }
label1:;
  t3++; goto label0;
label2:;
  }
gg_done:;
}
/* FG_PACKED_MATCH:fma32 */
static gg_type_26 gg_function_33(float p597, gg_type_22 p598, gg_type_26 p599, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__) && defined(__FMA__)
  { gg_type_26 packed_result;
    _mm256_storeu_ps(packed_result.data, _mm256_fmadd_ps(_mm256_set1_ps(p597), _mm256_cvtepi32_ps(_mm256_loadu_si256((const __m256i*)p598.data)), _mm256_loadu_ps(p599.data)));
    return packed_result;
  }
#endif
  gg_type_26 t0;
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
  float v597;
  gg_type_22 v598;
  gg_type_26 v599;
  gg_type_26 v600;
  int64_t v602;
  v597 = p597;
  v598 = p598;
  v599 = p599;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v600).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v602 = t4;
  {
  t6 = gg_view((v600).data, 8);
  t7 = v602;
  t8 = ((float *)t6.data) + gg_index(t7, t6.len);
  t9 = ((float)(v597));
  t10 = gg_view((v598).data, 8);
  t11 = v602;
  t12 = ((int32_t *)t10.data) + gg_index(t11, t10.len);
  t13 = ((float)(((float)((*t12)))));
  t14 = gg_view((v599).data, 8);
  t15 = v602;
  t16 = ((float *)t14.data) + gg_index(t15, t14.len);
  t17 = ((float)((*t16)));
  t18 = fmaf(t9, t13, t17);
  (*t8) = ((float)(t18));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v600;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:mul32 */
static gg_type_22 gg_function_31(gg_type_22 p578, gg_type_22 p579, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_22 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_mullo_epi32(_mm256_loadu_si256((const __m256i*)p578.data), _mm256_loadu_si256((const __m256i*)p579.data)));
    return packed_result;
  }
#endif
  gg_type_22 t0;
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
  gg_type_22 v578;
  gg_type_22 v579;
  gg_type_22 v580;
  int64_t v582;
  v578 = p578;
  v579 = p579;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((int32_t)(t1));
  for (t3=0; t3<8; t3++) (v580).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(8));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v582 = t4;
  {
  t6 = gg_view((v580).data, 8);
  t7 = v582;
  t8 = ((int32_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v578).data, 8);
  t10 = v582;
  t11 = ((int32_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((int32_t)((*t11)));
  t13 = gg_view((v579).data, 8);
  t14 = v582;
  t15 = ((int32_t *)t13.data) + gg_index(t14, t13.len);
  t16 = ((int32_t)((*t15)));
  (*t8) = ((int32_t)(((int32_t)((uint64_t)(t12) * (uint64_t)(t16)))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v580;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_42(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  int32_t t21;
  int32_t t22;
  int64_t t23;
  int32_t t24;
  int32_t t25;
  int64_t t26;
  int32_t t27;
  int32_t t28;
  int64_t t29;
  int32_t t30;
  int32_t t31;
  int64_t t32;
  int32_t t33;
  int32_t t34;
  int64_t t35;
  int32_t t36;
  int32_t t37;
  int64_t t38;
  int32_t t39;
  int32_t t40;
  int64_t t41;
  int32_t t42;
  int32_t t43;
  int64_t t44;
  int32_t t45;
  int32_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  gg_type_19 * t51;
  gg_slice t52;
  int64_t t53;
  gg_type_18 * t54;
  gg_slice t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  gg_type_22 * t63;
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int32_t * t70;
  gg_slice t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  gg_type_22 * t79;
  gg_slice t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int32_t * t88;
  int16_t t89;
  int16_t t90;
  gg_type_22 t91;
  gg_type_11 t92;
  gg_type_18 t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  gg_type_9 * t102;
  gg_slice t103;
  int64_t t104;
  gg_type_8 * t105;
  gg_type_8 t106;
  gg_type_11 t107;
  gg_type_22 t108;
  gg_type_22 t109;
  gg_type_11 t110;
  gg_type_18 t111;
  gg_slice t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  gg_type_9 * t120;
  gg_slice t121;
  int64_t t122;
  gg_type_8 * t123;
  gg_type_8 t124;
  gg_type_11 t125;
  gg_type_22 t126;
  gg_type_22 t127;
  gg_type_11 t128;
  gg_type_18 t129;
  gg_slice t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  gg_type_9 * t138;
  gg_slice t139;
  int64_t t140;
  gg_type_8 * t141;
  gg_type_8 t142;
  gg_type_11 t143;
  gg_type_22 t144;
  gg_type_22 t145;
  gg_type_11 t146;
  gg_type_18 t147;
  gg_slice t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  gg_type_9 * t156;
  gg_slice t157;
  int64_t t158;
  gg_type_8 * t159;
  gg_type_8 t160;
  gg_type_11 t161;
  gg_type_22 t162;
  gg_type_22 t163;
  gg_type_11 t164;
  gg_type_18 t165;
  gg_slice t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  gg_type_9 * t174;
  gg_slice t175;
  int64_t t176;
  gg_type_8 * t177;
  gg_type_8 t178;
  gg_type_11 t179;
  gg_type_22 t180;
  gg_type_22 t181;
  gg_type_11 t182;
  gg_type_18 t183;
  gg_slice t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  gg_type_9 * t192;
  gg_slice t193;
  int64_t t194;
  gg_type_8 * t195;
  gg_type_8 t196;
  gg_type_11 t197;
  gg_type_22 t198;
  gg_type_22 t199;
  gg_type_11 t200;
  gg_type_18 t201;
  gg_slice t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  gg_type_9 * t210;
  gg_slice t211;
  int64_t t212;
  gg_type_8 * t213;
  gg_type_8 t214;
  gg_type_11 t215;
  gg_type_22 t216;
  gg_type_22 t217;
  gg_type_11 t218;
  gg_type_18 t219;
  gg_slice t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  gg_type_9 * t228;
  gg_slice t229;
  int64_t t230;
  gg_type_8 * t231;
  gg_type_8 t232;
  gg_type_11 t233;
  gg_type_22 t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
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
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  gg_type_22 t261;
  int64_t t262;
  gg_type_22 * t263;
  gg_type_22 t264;
  gg_slice t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  gg_type_22 * t273;
  gg_slice t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  int32_t * t280;
  int32_t t281;
  gg_type_22 t282;
  gg_type_22 t283;
  int64_t t284;
  gg_type_22 * t285;
  gg_type_22 t286;
  gg_slice t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  gg_type_22 * t295;
  gg_slice t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  int32_t * t304;
  int32_t t305;
  gg_type_22 t306;
  gg_type_22 t307;
  gg_type_22 t308;
  int32_t t309;
  int32_t t310;
  int64_t t311;
  int64_t t312;
  int32_t * t313;
  gg_type_22 t314;
  int64_t t315;
  int32_t * t316;
  gg_type_22 t317;
  int64_t t318;
  int32_t * t319;
  gg_type_22 t320;
  int64_t t321;
  int32_t * t322;
  gg_type_22 t323;
  int64_t t324;
  int32_t * t325;
  gg_type_22 t326;
  int64_t t327;
  int32_t * t328;
  gg_type_22 t329;
  int64_t t330;
  int32_t * t331;
  gg_type_22 t332;
  int64_t t333;
  int32_t * t334;
  gg_type_22 t335;
  int64_t t336;
  float * t337;
  float t338;
  float t339;
  gg_type_22 t340;
  int64_t t341;
  float * t342;
  float t343;
  gg_type_22 t344;
  gg_type_26 t345;
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
  int64_t t356;
  gg_type_26 * t357;
  gg_slice t358;
  int64_t t359;
  float * t360;
  int64_t t361;
  float * t362;
  float t363;
  gg_slice t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  float * t372;
  float t373;
  int64_t t374;
  gg_type_26 * t375;
  gg_slice t376;
  int64_t t377;
  float * t378;
  float t379;
  float t380;
  gg_type_41 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_50 v6;
  gg_type_55 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  gg_type_22 v15;
  gg_type_22 v16;
  gg_type_22 v17;
  gg_type_22 v18;
  gg_type_22 v19;
  gg_type_22 v20;
  gg_type_22 v21;
  gg_type_22 v22;
  gg_type_22 v23;
  int64_t v24;
  gg_type_18 v25;
  int32_t v26;
  int32_t v27;
  gg_type_11 v28;
  int64_t v29;
  int64_t v30;
  gg_type_22 v31;
  gg_type_26 v32;
  int64_t v33;
  int64_t v34;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  v6 = &(((*(v0))).f54);
  v7 = &(((*(v0))).f58);
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  } else {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  }
  t4 = ((int64_t)UINT64_C(16));
  t3 = 0;
label0:;
  if (t3 >= t4) goto label2;
  v9 = t3;
  {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v9));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(v10));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t11 = ((int64_t)(v10));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(v5));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)UINT64_C(2));
  t15 = 0;
label3:;
  if (t15 >= t16) goto label5;
  v13 = t15;
  {
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(v13));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t20)))));
  memset(&t21, 0, sizeof(t21));
  t22 = ((int32_t)(t21));
  for (t23=0; t23<8; t23++) (v15).data[t23] = t22;
  memset(&t24, 0, sizeof(t24));
  t25 = ((int32_t)(t24));
  for (t26=0; t26<8; t26++) (v16).data[t26] = t25;
  memset(&t27, 0, sizeof(t27));
  t28 = ((int32_t)(t27));
  for (t29=0; t29<8; t29++) (v17).data[t29] = t28;
  memset(&t30, 0, sizeof(t30));
  t31 = ((int32_t)(t30));
  for (t32=0; t32<8; t32++) (v18).data[t32] = t31;
  memset(&t33, 0, sizeof(t33));
  t34 = ((int32_t)(t33));
  for (t35=0; t35<8; t35++) (v19).data[t35] = t34;
  memset(&t36, 0, sizeof(t36));
  t37 = ((int32_t)(t36));
  for (t38=0; t38<8; t38++) (v20).data[t38] = t37;
  memset(&t39, 0, sizeof(t39));
  t40 = ((int32_t)(t39));
  for (t41=0; t41<8; t41++) (v21).data[t41] = t40;
  memset(&t42, 0, sizeof(t42));
  t43 = ((int32_t)(t42));
  for (t44=0; t44<8; t44++) (v22).data[t44] = t43;
  memset(&t45, 0, sizeof(t45));
  t46 = ((int32_t)(t45));
  for (t47=0; t47<8; t47++) (v23).data[t47] = t46;
  t49 = ((int64_t)UINT64_C(8));
  t48 = 0;
label6:;
  if (t48 >= t49) goto label8;
  v24 = t48;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t50 = v12;
  t51 = ((gg_type_19 *)(((*(v6))).f49).data) + gg_index(t50, ((int64_t)UINT64_C(256)));
  t52 = gg_view(((*t51)).data, 8);
  t53 = v24;
  t54 = ((gg_type_18 *)t52.data) + gg_index(t53, t52.len);
  v25 = (*t54);
  t55 = gg_view((((*(v6))).f52).data, 512);
  t56 = ((int64_t)(v12));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  t59 = ((int64_t)(v24));
  t60 = ((int64_t)(((int64_t)UINT64_C(4))));
  t61 = ((int64_t)(((int64_t)gg_div(t59, t60))));
  t62 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t61)));
  t63 = ((gg_type_22 *)t55.data) + gg_index(t62, t55.len);
  t64 = gg_view(((*t63)).data, 8);
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t66 = ((int64_t)(v24));
  t67 = ((int64_t)(((int64_t)UINT64_C(4))));
  t68 = ((int64_t)(((int64_t)gg_mod(t66, t67))));
  t69 = ((int64_t)((uint64_t)(t65) * (uint64_t)(t68)));
  t70 = ((int32_t *)t64.data) + gg_index(t69, t64.len);
  v26 = ((int32_t)((*t70)));
  t71 = gg_view((((*(v6))).f52).data, 512);
  t72 = ((int64_t)(v12));
  t73 = ((int64_t)(((int64_t)UINT64_C(2))));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  t75 = ((int64_t)(v24));
  t76 = ((int64_t)(((int64_t)UINT64_C(4))));
  t77 = ((int64_t)(((int64_t)gg_div(t75, t76))));
  t78 = ((int64_t)((uint64_t)(t74) + (uint64_t)(t77)));
  t79 = ((gg_type_22 *)t71.data) + gg_index(t78, t71.len);
  t80 = gg_view(((*t79)).data, 8);
  t81 = ((int64_t)(((int64_t)UINT64_C(2))));
  t82 = ((int64_t)(v24));
  t83 = ((int64_t)(((int64_t)UINT64_C(4))));
  t84 = ((int64_t)(((int64_t)gg_mod(t82, t83))));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t84)))));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((int64_t)((uint64_t)(t85) + (uint64_t)(t86)));
  t88 = ((int32_t *)t80.data) + gg_index(t87, t80.len);
  v27 = ((int32_t)((*t88)));
  t89 = ((int16_t)(((int16_t)(v26))));
  t90 = ((int16_t)(((int16_t)(v27))));
  v28 = gg_function_37(t89, t90, base + 0);
  t91 = v15;
  t92 = v28;
  t93 = v25;
  t94 = gg_view((((*(v7))).f55).data, 256);
  t95 = ((int64_t)(v14));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t95) + (uint64_t)(t96)))));
  t98 = ((int64_t)(v4));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t97) * (uint64_t)(t98)))));
  t100 = ((int64_t)(v5));
  t101 = ((int64_t)((uint64_t)(t99) + (uint64_t)(t100)));
  t102 = ((gg_type_9 *)t94.data) + gg_index(t101, t94.len);
  t103 = gg_view(((*t102)).data, 8);
  t104 = v24;
  t105 = ((gg_type_8 *)t103.data) + gg_index(t104, t103.len);
  t106 = (*t105);
  t107 = gg_function_22(t93, t106, base + 0);
  t108 = gg_function_23(t92, t107, base + 0);
  v15 = gg_function_29(t91, t108, base + 0);
  t109 = v16;
  t110 = v28;
  t111 = v25;
  t112 = gg_view((((*(v7))).f55).data, 256);
  t113 = ((int64_t)(v14));
  t114 = ((int64_t)(((int64_t)UINT64_C(1))));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t113) + (uint64_t)(t114)))));
  t116 = ((int64_t)(v4));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) * (uint64_t)(t116)))));
  t118 = ((int64_t)(v5));
  t119 = ((int64_t)((uint64_t)(t117) + (uint64_t)(t118)));
  t120 = ((gg_type_9 *)t112.data) + gg_index(t119, t112.len);
  t121 = gg_view(((*t120)).data, 8);
  t122 = v24;
  t123 = ((gg_type_8 *)t121.data) + gg_index(t122, t121.len);
  t124 = (*t123);
  t125 = gg_function_22(t111, t124, base + 0);
  t126 = gg_function_23(t110, t125, base + 0);
  v16 = gg_function_29(t109, t126, base + 0);
  t127 = v17;
  t128 = v28;
  t129 = v25;
  t130 = gg_view((((*(v7))).f55).data, 256);
  t131 = ((int64_t)(v14));
  t132 = ((int64_t)(((int64_t)UINT64_C(2))));
  t133 = ((int64_t)(((int64_t)((uint64_t)(t131) + (uint64_t)(t132)))));
  t134 = ((int64_t)(v4));
  t135 = ((int64_t)(((int64_t)((uint64_t)(t133) * (uint64_t)(t134)))));
  t136 = ((int64_t)(v5));
  t137 = ((int64_t)((uint64_t)(t135) + (uint64_t)(t136)));
  t138 = ((gg_type_9 *)t130.data) + gg_index(t137, t130.len);
  t139 = gg_view(((*t138)).data, 8);
  t140 = v24;
  t141 = ((gg_type_8 *)t139.data) + gg_index(t140, t139.len);
  t142 = (*t141);
  t143 = gg_function_22(t129, t142, base + 0);
  t144 = gg_function_23(t128, t143, base + 0);
  v17 = gg_function_29(t127, t144, base + 0);
  t145 = v18;
  t146 = v28;
  t147 = v25;
  t148 = gg_view((((*(v7))).f55).data, 256);
  t149 = ((int64_t)(v14));
  t150 = ((int64_t)(((int64_t)UINT64_C(3))));
  t151 = ((int64_t)(((int64_t)((uint64_t)(t149) + (uint64_t)(t150)))));
  t152 = ((int64_t)(v4));
  t153 = ((int64_t)(((int64_t)((uint64_t)(t151) * (uint64_t)(t152)))));
  t154 = ((int64_t)(v5));
  t155 = ((int64_t)((uint64_t)(t153) + (uint64_t)(t154)));
  t156 = ((gg_type_9 *)t148.data) + gg_index(t155, t148.len);
  t157 = gg_view(((*t156)).data, 8);
  t158 = v24;
  t159 = ((gg_type_8 *)t157.data) + gg_index(t158, t157.len);
  t160 = (*t159);
  t161 = gg_function_22(t147, t160, base + 0);
  t162 = gg_function_23(t146, t161, base + 0);
  v18 = gg_function_29(t145, t162, base + 0);
  t163 = v19;
  t164 = v28;
  t165 = v25;
  t166 = gg_view((((*(v7))).f55).data, 256);
  t167 = ((int64_t)(v14));
  t168 = ((int64_t)(((int64_t)UINT64_C(4))));
  t169 = ((int64_t)(((int64_t)((uint64_t)(t167) + (uint64_t)(t168)))));
  t170 = ((int64_t)(v4));
  t171 = ((int64_t)(((int64_t)((uint64_t)(t169) * (uint64_t)(t170)))));
  t172 = ((int64_t)(v5));
  t173 = ((int64_t)((uint64_t)(t171) + (uint64_t)(t172)));
  t174 = ((gg_type_9 *)t166.data) + gg_index(t173, t166.len);
  t175 = gg_view(((*t174)).data, 8);
  t176 = v24;
  t177 = ((gg_type_8 *)t175.data) + gg_index(t176, t175.len);
  t178 = (*t177);
  t179 = gg_function_22(t165, t178, base + 0);
  t180 = gg_function_23(t164, t179, base + 0);
  v19 = gg_function_29(t163, t180, base + 0);
  t181 = v20;
  t182 = v28;
  t183 = v25;
  t184 = gg_view((((*(v7))).f55).data, 256);
  t185 = ((int64_t)(v14));
  t186 = ((int64_t)(((int64_t)UINT64_C(5))));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t185) + (uint64_t)(t186)))));
  t188 = ((int64_t)(v4));
  t189 = ((int64_t)(((int64_t)((uint64_t)(t187) * (uint64_t)(t188)))));
  t190 = ((int64_t)(v5));
  t191 = ((int64_t)((uint64_t)(t189) + (uint64_t)(t190)));
  t192 = ((gg_type_9 *)t184.data) + gg_index(t191, t184.len);
  t193 = gg_view(((*t192)).data, 8);
  t194 = v24;
  t195 = ((gg_type_8 *)t193.data) + gg_index(t194, t193.len);
  t196 = (*t195);
  t197 = gg_function_22(t183, t196, base + 0);
  t198 = gg_function_23(t182, t197, base + 0);
  v20 = gg_function_29(t181, t198, base + 0);
  t199 = v21;
  t200 = v28;
  t201 = v25;
  t202 = gg_view((((*(v7))).f55).data, 256);
  t203 = ((int64_t)(v14));
  t204 = ((int64_t)(((int64_t)UINT64_C(6))));
  t205 = ((int64_t)(((int64_t)((uint64_t)(t203) + (uint64_t)(t204)))));
  t206 = ((int64_t)(v4));
  t207 = ((int64_t)(((int64_t)((uint64_t)(t205) * (uint64_t)(t206)))));
  t208 = ((int64_t)(v5));
  t209 = ((int64_t)((uint64_t)(t207) + (uint64_t)(t208)));
  t210 = ((gg_type_9 *)t202.data) + gg_index(t209, t202.len);
  t211 = gg_view(((*t210)).data, 8);
  t212 = v24;
  t213 = ((gg_type_8 *)t211.data) + gg_index(t212, t211.len);
  t214 = (*t213);
  t215 = gg_function_22(t201, t214, base + 0);
  t216 = gg_function_23(t200, t215, base + 0);
  v21 = gg_function_29(t199, t216, base + 0);
  t217 = v22;
  t218 = v28;
  t219 = v25;
  t220 = gg_view((((*(v7))).f55).data, 256);
  t221 = ((int64_t)(v14));
  t222 = ((int64_t)(((int64_t)UINT64_C(7))));
  t223 = ((int64_t)(((int64_t)((uint64_t)(t221) + (uint64_t)(t222)))));
  t224 = ((int64_t)(v4));
  t225 = ((int64_t)(((int64_t)((uint64_t)(t223) * (uint64_t)(t224)))));
  t226 = ((int64_t)(v5));
  t227 = ((int64_t)((uint64_t)(t225) + (uint64_t)(t226)));
  t228 = ((gg_type_9 *)t220.data) + gg_index(t227, t220.len);
  t229 = gg_view(((*t228)).data, 8);
  t230 = v24;
  t231 = ((gg_type_8 *)t229.data) + gg_index(t230, t229.len);
  t232 = (*t231);
  t233 = gg_function_22(t219, t232, base + 0);
  t234 = gg_function_23(t218, t233, base + 0);
  v22 = gg_function_29(t217, t234, base + 0);
  t235 = ((int64_t)(v5));
  t236 = ((int64_t)(((int64_t)UINT64_C(16))));
  t237 = ((int64_t)(((int64_t)((uint64_t)(t235) * (uint64_t)(t236)))));
  t238 = ((int64_t)(((int64_t)UINT64_C(2))));
  t239 = ((int64_t)(v24));
  t240 = ((int64_t)(((int64_t)((uint64_t)(t238) * (uint64_t)(t239)))));
  t241 = ((int64_t)(((int64_t)((uint64_t)(t237) + (uint64_t)(t240)))));
  t242 = ((int64_t)(v8));
  t243 = ((int64_t)(((int64_t)((uint64_t)(t241) * (uint64_t)(t242)))));
  t244 = ((int64_t)(v14));
  t245 = ((int64_t)(((int64_t)((uint64_t)(t243) + (uint64_t)(t244)))));
  t246 = ((int64_t)(((int64_t)UINT64_C(8))));
  v29 = ((int64_t)(((int64_t)gg_div(t245, t246))));
  t247 = ((int64_t)(v5));
  t248 = ((int64_t)(((int64_t)UINT64_C(16))));
  t249 = ((int64_t)(((int64_t)((uint64_t)(t247) * (uint64_t)(t248)))));
  t250 = ((int64_t)(((int64_t)UINT64_C(2))));
  t251 = ((int64_t)(v24));
  t252 = ((int64_t)(((int64_t)((uint64_t)(t250) * (uint64_t)(t251)))));
  t253 = ((int64_t)(((int64_t)((uint64_t)(t249) + (uint64_t)(t252)))));
  t254 = ((int64_t)(((int64_t)UINT64_C(1))));
  t255 = ((int64_t)(((int64_t)((uint64_t)(t253) + (uint64_t)(t254)))));
  t256 = ((int64_t)(v8));
  t257 = ((int64_t)(((int64_t)((uint64_t)(t255) * (uint64_t)(t256)))));
  t258 = ((int64_t)(v14));
  t259 = ((int64_t)(((int64_t)((uint64_t)(t257) + (uint64_t)(t258)))));
  t260 = ((int64_t)(((int64_t)UINT64_C(8))));
  v30 = ((int64_t)(((int64_t)gg_div(t259, t260))));
  t261 = v23;
  /* FG_DIRECT_SLICE_INDEX */
  t262 = v29;
  t263 = ((gg_type_22 *)(((*(v7))).f56).data) + gg_index(t262, ((int64_t)UINT64_C(512)));
  t264 = (*t263);
  t265 = gg_view((((*(v6))).f53).data, 512);
  t266 = ((int64_t)(v12));
  t267 = ((int64_t)(((int64_t)UINT64_C(2))));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t266) * (uint64_t)(t267)))));
  t269 = ((int64_t)(v24));
  t270 = ((int64_t)(((int64_t)UINT64_C(4))));
  t271 = ((int64_t)(((int64_t)gg_div(t269, t270))));
  t272 = ((int64_t)((uint64_t)(t268) + (uint64_t)(t271)));
  t273 = ((gg_type_22 *)t265.data) + gg_index(t272, t265.len);
  t274 = gg_view(((*t273)).data, 8);
  t275 = ((int64_t)(((int64_t)UINT64_C(2))));
  t276 = ((int64_t)(v24));
  t277 = ((int64_t)(((int64_t)UINT64_C(4))));
  t278 = ((int64_t)(((int64_t)gg_mod(t276, t277))));
  t279 = ((int64_t)((uint64_t)(t275) * (uint64_t)(t278)));
  t280 = ((int32_t *)t274.data) + gg_index(t279, t274.len);
  t281 = ((int32_t)((*t280)));
  t282 = gg_function_30(t281, base + 0);
  t283 = gg_function_31(t264, t282, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t284 = v30;
  t285 = ((gg_type_22 *)(((*(v7))).f56).data) + gg_index(t284, ((int64_t)UINT64_C(512)));
  t286 = (*t285);
  t287 = gg_view((((*(v6))).f53).data, 512);
  t288 = ((int64_t)(v12));
  t289 = ((int64_t)(((int64_t)UINT64_C(2))));
  t290 = ((int64_t)(((int64_t)((uint64_t)(t288) * (uint64_t)(t289)))));
  t291 = ((int64_t)(v24));
  t292 = ((int64_t)(((int64_t)UINT64_C(4))));
  t293 = ((int64_t)(((int64_t)gg_div(t291, t292))));
  t294 = ((int64_t)((uint64_t)(t290) + (uint64_t)(t293)));
  t295 = ((gg_type_22 *)t287.data) + gg_index(t294, t287.len);
  t296 = gg_view(((*t295)).data, 8);
  t297 = ((int64_t)(((int64_t)UINT64_C(2))));
  t298 = ((int64_t)(v24));
  t299 = ((int64_t)(((int64_t)UINT64_C(4))));
  t300 = ((int64_t)(((int64_t)gg_mod(t298, t299))));
  t301 = ((int64_t)(((int64_t)((uint64_t)(t297) * (uint64_t)(t300)))));
  t302 = ((int64_t)(((int64_t)UINT64_C(1))));
  t303 = ((int64_t)((uint64_t)(t301) + (uint64_t)(t302)));
  t304 = ((int32_t *)t296.data) + gg_index(t303, t296.len);
  t305 = ((int32_t)((*t304)));
  t306 = gg_function_30(t305, base + 0);
  t307 = gg_function_31(t286, t306, base + 0);
  t308 = gg_function_29(t283, t307, base + 0);
  v23 = gg_function_29(t261, t308, base + 0);
  }
label7:;
  t48++; goto label6;
label8:;
  memset(&t309, 0, sizeof(t309));
  t310 = ((int32_t)(t309));
  for (t311=0; t311<8; t311++) (v31).data[t311] = t310;
  /* FG_DIRECT_SLICE_INDEX */
  t312 = ((int64_t)UINT64_C(0));
  t313 = ((int32_t *)(v31).data) + gg_index(t312, ((int64_t)UINT64_C(8)));
  t314 = v15;
  (*t313) = gg_function_24(t314, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t315 = ((int64_t)UINT64_C(1));
  t316 = ((int32_t *)(v31).data) + gg_index(t315, ((int64_t)UINT64_C(8)));
  t317 = v16;
  (*t316) = gg_function_24(t317, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t318 = ((int64_t)UINT64_C(2));
  t319 = ((int32_t *)(v31).data) + gg_index(t318, ((int64_t)UINT64_C(8)));
  t320 = v17;
  (*t319) = gg_function_24(t320, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t321 = ((int64_t)UINT64_C(3));
  t322 = ((int32_t *)(v31).data) + gg_index(t321, ((int64_t)UINT64_C(8)));
  t323 = v18;
  (*t322) = gg_function_24(t323, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t324 = ((int64_t)UINT64_C(4));
  t325 = ((int32_t *)(v31).data) + gg_index(t324, ((int64_t)UINT64_C(8)));
  t326 = v19;
  (*t325) = gg_function_24(t326, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t327 = ((int64_t)UINT64_C(5));
  t328 = ((int32_t *)(v31).data) + gg_index(t327, ((int64_t)UINT64_C(8)));
  t329 = v20;
  (*t328) = gg_function_24(t329, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t330 = ((int64_t)UINT64_C(6));
  t331 = ((int32_t *)(v31).data) + gg_index(t330, ((int64_t)UINT64_C(8)));
  t332 = v21;
  (*t331) = gg_function_24(t332, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t333 = ((int64_t)UINT64_C(7));
  t334 = ((int32_t *)(v31).data) + gg_index(t333, ((int64_t)UINT64_C(8)));
  t335 = v22;
  (*t334) = gg_function_24(t335, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t336 = v11;
  t337 = ((float *)(((*(v6))).f51).data) + gg_index(t336, ((int64_t)UINT64_C(256)));
  t338 = (*t337);
  t339 = ((float)((-(t338))));
  t340 = v23;
  /* FG_DIRECT_SLICE_INDEX */
  t341 = v11;
  t342 = ((float *)(((*(v6))).f50).data) + gg_index(t341, ((int64_t)UINT64_C(256)));
  t343 = ((float)((*t342)));
  t344 = v31;
  t345 = gg_function_32(t343, t344, base + 0);
  v32 = gg_function_33(t339, t340, t345, base + 0);
  t346 = ((int64_t)(v3));
  t347 = ((int64_t)(v9));
  t348 = ((int64_t)(((int64_t)((uint64_t)(t346) + (uint64_t)(t347)))));
  t349 = ((int64_t)(((int64_t)UINT64_C(32))));
  t350 = ((int64_t)(((int64_t)((uint64_t)(t348) * (uint64_t)(t349)))));
  t351 = ((int64_t)(v14));
  t352 = ((int64_t)(((int64_t)UINT64_C(8))));
  t353 = ((int64_t)(((int64_t)gg_div(t351, t352))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t350) + (uint64_t)(t353)))));
  t355 = ((int64_t)UINT64_C(8));
  t354 = 0;
label9:;
  if (t354 >= t355) goto label11;
  v34 = t354;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t356 = v33;
  t357 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t356, ((int64_t)UINT64_C(8192)));
  t358 = gg_view(((*t357)).data, 8);
  t359 = v34;
  t360 = ((float *)t358.data) + gg_index(t359, t358.len);
  /* FG_DIRECT_SLICE_INDEX */
  t361 = v34;
  t362 = ((float *)(v32).data) + gg_index(t361, ((int64_t)UINT64_C(8)));
  t363 = ((float)((*t362)));
  t364 = gg_view((((*(v7))).f57).data, 256);
  t365 = ((int64_t)(v5));
  t366 = ((int64_t)(((int64_t)UINT64_C(16))));
  t367 = ((int64_t)(((int64_t)((uint64_t)(t365) * (uint64_t)(t366)))));
  t368 = ((int64_t)(v14));
  t369 = ((int64_t)(((int64_t)((uint64_t)(t367) + (uint64_t)(t368)))));
  t370 = ((int64_t)(v34));
  t371 = ((int64_t)((uint64_t)(t369) + (uint64_t)(t370)));
  t372 = ((float *)t364.data) + gg_index(t371, t364.len);
  t373 = ((float)((*t372)));
  /* FG_DIRECT_SLICE_INDEX */
  t374 = v33;
  t375 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t374, ((int64_t)UINT64_C(8192)));
  t376 = gg_view(((*t375)).data, 8);
  t377 = v34;
  t378 = ((float *)t376.data) + gg_index(t377, t376.len);
  t379 = ((float)((*t378)));
  t380 = fmaf(t363, t373, t379);
  (*t360) = ((float)(t380));
  }
label10:;
  t354++; goto label9;
label11:;
  }
label4:;
  t15++; goto label3;
label5:;
  }
label1:;
  t3++; goto label0;
label2:;
  }
gg_done:;
}
static void gg_function_41(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  int32_t t21;
  int32_t t22;
  int64_t t23;
  int32_t t24;
  int32_t t25;
  int64_t t26;
  int32_t t27;
  int32_t t28;
  int64_t t29;
  int32_t t30;
  int32_t t31;
  int64_t t32;
  int32_t t33;
  int32_t t34;
  int64_t t35;
  int32_t t36;
  int32_t t37;
  int64_t t38;
  int32_t t39;
  int32_t t40;
  int64_t t41;
  int32_t t42;
  int32_t t43;
  int64_t t44;
  int32_t t45;
  int32_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  gg_type_19 * t51;
  gg_slice t52;
  int64_t t53;
  gg_type_18 * t54;
  gg_slice t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  gg_type_22 * t63;
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int32_t * t70;
  gg_slice t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  gg_type_22 * t79;
  gg_slice t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int32_t * t88;
  int16_t t89;
  int16_t t90;
  gg_type_18 t91;
  uint8_t t92;
  gg_type_22 t93;
  gg_type_11 t94;
  gg_slice t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  gg_type_9 * t103;
  gg_slice t104;
  int64_t t105;
  gg_type_8 * t106;
  gg_type_8 t107;
  gg_type_18 t108;
  gg_type_8 t109;
  gg_type_11 t110;
  gg_type_22 t111;
  gg_type_22 t112;
  gg_type_11 t113;
  gg_slice t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  gg_type_9 * t122;
  gg_slice t123;
  int64_t t124;
  gg_type_8 * t125;
  gg_type_8 t126;
  gg_type_18 t127;
  gg_type_8 t128;
  gg_type_11 t129;
  gg_type_22 t130;
  gg_type_22 t131;
  gg_type_11 t132;
  gg_slice t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  gg_type_9 * t141;
  gg_slice t142;
  int64_t t143;
  gg_type_8 * t144;
  gg_type_8 t145;
  gg_type_18 t146;
  gg_type_8 t147;
  gg_type_11 t148;
  gg_type_22 t149;
  gg_type_22 t150;
  gg_type_11 t151;
  gg_slice t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  gg_type_9 * t160;
  gg_slice t161;
  int64_t t162;
  gg_type_8 * t163;
  gg_type_8 t164;
  gg_type_18 t165;
  gg_type_8 t166;
  gg_type_11 t167;
  gg_type_22 t168;
  gg_type_22 t169;
  gg_type_11 t170;
  gg_slice t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  gg_type_9 * t179;
  gg_slice t180;
  int64_t t181;
  gg_type_8 * t182;
  gg_type_8 t183;
  gg_type_18 t184;
  gg_type_8 t185;
  gg_type_11 t186;
  gg_type_22 t187;
  gg_type_22 t188;
  gg_type_11 t189;
  gg_slice t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  gg_type_9 * t198;
  gg_slice t199;
  int64_t t200;
  gg_type_8 * t201;
  gg_type_8 t202;
  gg_type_18 t203;
  gg_type_8 t204;
  gg_type_11 t205;
  gg_type_22 t206;
  gg_type_22 t207;
  gg_type_11 t208;
  gg_slice t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  gg_type_9 * t217;
  gg_slice t218;
  int64_t t219;
  gg_type_8 * t220;
  gg_type_8 t221;
  gg_type_18 t222;
  gg_type_8 t223;
  gg_type_11 t224;
  gg_type_22 t225;
  gg_type_22 t226;
  gg_type_11 t227;
  gg_slice t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  gg_type_9 * t236;
  gg_slice t237;
  int64_t t238;
  gg_type_8 * t239;
  gg_type_8 t240;
  gg_type_18 t241;
  gg_type_8 t242;
  gg_type_11 t243;
  gg_type_22 t244;
  int32_t t245;
  int32_t t246;
  int64_t t247;
  int64_t t248;
  int32_t * t249;
  gg_type_22 t250;
  int64_t t251;
  int32_t * t252;
  gg_type_22 t253;
  int64_t t254;
  int32_t * t255;
  gg_type_22 t256;
  int64_t t257;
  int32_t * t258;
  gg_type_22 t259;
  int64_t t260;
  int32_t * t261;
  gg_type_22 t262;
  int64_t t263;
  int32_t * t264;
  gg_type_22 t265;
  int64_t t266;
  int32_t * t267;
  gg_type_22 t268;
  int64_t t269;
  int32_t * t270;
  gg_type_22 t271;
  gg_type_22 t272;
  gg_slice t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  gg_type_22 * t277;
  gg_slice t278;
  int64_t t279;
  int32_t * t280;
  int32_t t281;
  gg_type_22 t282;
  int64_t t283;
  float * t284;
  float t285;
  gg_type_22 t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  gg_type_26 * t298;
  gg_slice t299;
  int64_t t300;
  float * t301;
  int64_t t302;
  float * t303;
  float t304;
  gg_slice t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  float * t313;
  float t314;
  int64_t t315;
  gg_type_26 * t316;
  gg_slice t317;
  int64_t t318;
  float * t319;
  float t320;
  float t321;
  gg_type_41 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_50 v6;
  gg_type_55 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  gg_type_22 v15;
  gg_type_22 v16;
  gg_type_22 v17;
  gg_type_22 v18;
  gg_type_22 v19;
  gg_type_22 v20;
  gg_type_22 v21;
  gg_type_22 v22;
  gg_type_22 v23;
  int64_t v24;
  gg_type_18 v25;
  int32_t v26;
  int32_t v27;
  gg_type_11 v28;
  gg_type_8 v29;
  gg_type_22 v30;
  gg_type_26 v31;
  int64_t v32;
  int64_t v33;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  v6 = &(((*(v0))).f54);
  v7 = &(((*(v0))).f58);
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  } else {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  }
  t4 = ((int64_t)UINT64_C(16));
  t3 = 0;
label0:;
  if (t3 >= t4) goto label2;
  v9 = t3;
  {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v9));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(v10));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t11 = ((int64_t)(v10));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(v5));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)UINT64_C(2));
  t15 = 0;
label3:;
  if (t15 >= t16) goto label5;
  v13 = t15;
  {
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(v13));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t20)))));
  memset(&t21, 0, sizeof(t21));
  t22 = ((int32_t)(t21));
  for (t23=0; t23<8; t23++) (v15).data[t23] = t22;
  memset(&t24, 0, sizeof(t24));
  t25 = ((int32_t)(t24));
  for (t26=0; t26<8; t26++) (v16).data[t26] = t25;
  memset(&t27, 0, sizeof(t27));
  t28 = ((int32_t)(t27));
  for (t29=0; t29<8; t29++) (v17).data[t29] = t28;
  memset(&t30, 0, sizeof(t30));
  t31 = ((int32_t)(t30));
  for (t32=0; t32<8; t32++) (v18).data[t32] = t31;
  memset(&t33, 0, sizeof(t33));
  t34 = ((int32_t)(t33));
  for (t35=0; t35<8; t35++) (v19).data[t35] = t34;
  memset(&t36, 0, sizeof(t36));
  t37 = ((int32_t)(t36));
  for (t38=0; t38<8; t38++) (v20).data[t38] = t37;
  memset(&t39, 0, sizeof(t39));
  t40 = ((int32_t)(t39));
  for (t41=0; t41<8; t41++) (v21).data[t41] = t40;
  memset(&t42, 0, sizeof(t42));
  t43 = ((int32_t)(t42));
  for (t44=0; t44<8; t44++) (v22).data[t44] = t43;
  memset(&t45, 0, sizeof(t45));
  t46 = ((int32_t)(t45));
  for (t47=0; t47<8; t47++) (v23).data[t47] = t46;
  t49 = ((int64_t)UINT64_C(8));
  t48 = 0;
label6:;
  if (t48 >= t49) goto label8;
  v24 = t48;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t50 = v12;
  t51 = ((gg_type_19 *)(((*(v6))).f49).data) + gg_index(t50, ((int64_t)UINT64_C(256)));
  t52 = gg_view(((*t51)).data, 8);
  t53 = v24;
  t54 = ((gg_type_18 *)t52.data) + gg_index(t53, t52.len);
  v25 = (*t54);
  t55 = gg_view((((*(v6))).f52).data, 512);
  t56 = ((int64_t)(v12));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  t59 = ((int64_t)(v24));
  t60 = ((int64_t)(((int64_t)UINT64_C(4))));
  t61 = ((int64_t)(((int64_t)gg_div(t59, t60))));
  t62 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t61)));
  t63 = ((gg_type_22 *)t55.data) + gg_index(t62, t55.len);
  t64 = gg_view(((*t63)).data, 8);
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t66 = ((int64_t)(v24));
  t67 = ((int64_t)(((int64_t)UINT64_C(4))));
  t68 = ((int64_t)(((int64_t)gg_mod(t66, t67))));
  t69 = ((int64_t)((uint64_t)(t65) * (uint64_t)(t68)));
  t70 = ((int32_t *)t64.data) + gg_index(t69, t64.len);
  v26 = ((int32_t)((*t70)));
  t71 = gg_view((((*(v6))).f52).data, 512);
  t72 = ((int64_t)(v12));
  t73 = ((int64_t)(((int64_t)UINT64_C(2))));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  t75 = ((int64_t)(v24));
  t76 = ((int64_t)(((int64_t)UINT64_C(4))));
  t77 = ((int64_t)(((int64_t)gg_div(t75, t76))));
  t78 = ((int64_t)((uint64_t)(t74) + (uint64_t)(t77)));
  t79 = ((gg_type_22 *)t71.data) + gg_index(t78, t71.len);
  t80 = gg_view(((*t79)).data, 8);
  t81 = ((int64_t)(((int64_t)UINT64_C(2))));
  t82 = ((int64_t)(v24));
  t83 = ((int64_t)(((int64_t)UINT64_C(4))));
  t84 = ((int64_t)(((int64_t)gg_mod(t82, t83))));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t84)))));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((int64_t)((uint64_t)(t85) + (uint64_t)(t86)));
  t88 = ((int32_t *)t80.data) + gg_index(t87, t80.len);
  v27 = ((int32_t)((*t88)));
  t89 = ((int16_t)(((int16_t)(v26))));
  t90 = ((int16_t)(((int16_t)(v27))));
  v28 = gg_function_37(t89, t90, base + 0);
  t91 = v25;
  t92 = ((uint8_t)(((int64_t)UINT64_C(4))));
  v29 = gg_function_38(t91, t92, base + 0);
  t93 = v15;
  t94 = v28;
  t95 = gg_view((((*(v7))).f55).data, 256);
  t96 = ((int64_t)(v14));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  t99 = ((int64_t)(v4));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  t101 = ((int64_t)(v5));
  t102 = ((int64_t)((uint64_t)(t100) + (uint64_t)(t101)));
  t103 = ((gg_type_9 *)t95.data) + gg_index(t102, t95.len);
  t104 = gg_view(((*t103)).data, 8);
  t105 = v24;
  t106 = ((gg_type_8 *)t104.data) + gg_index(t105, t104.len);
  t107 = (*t106);
  t108 = gg_function_39(t107, base + 0);
  t109 = v29;
  t110 = gg_function_22(t108, t109, base + 0);
  t111 = gg_function_23(t94, t110, base + 0);
  v15 = gg_function_29(t93, t111, base + 0);
  t112 = v16;
  t113 = v28;
  t114 = gg_view((((*(v7))).f55).data, 256);
  t115 = ((int64_t)(v14));
  t116 = ((int64_t)(((int64_t)UINT64_C(1))));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = ((int64_t)(v4));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) * (uint64_t)(t118)))));
  t120 = ((int64_t)(v5));
  t121 = ((int64_t)((uint64_t)(t119) + (uint64_t)(t120)));
  t122 = ((gg_type_9 *)t114.data) + gg_index(t121, t114.len);
  t123 = gg_view(((*t122)).data, 8);
  t124 = v24;
  t125 = ((gg_type_8 *)t123.data) + gg_index(t124, t123.len);
  t126 = (*t125);
  t127 = gg_function_39(t126, base + 0);
  t128 = v29;
  t129 = gg_function_22(t127, t128, base + 0);
  t130 = gg_function_23(t113, t129, base + 0);
  v16 = gg_function_29(t112, t130, base + 0);
  t131 = v17;
  t132 = v28;
  t133 = gg_view((((*(v7))).f55).data, 256);
  t134 = ((int64_t)(v14));
  t135 = ((int64_t)(((int64_t)UINT64_C(2))));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t134) + (uint64_t)(t135)))));
  t137 = ((int64_t)(v4));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t137)))));
  t139 = ((int64_t)(v5));
  t140 = ((int64_t)((uint64_t)(t138) + (uint64_t)(t139)));
  t141 = ((gg_type_9 *)t133.data) + gg_index(t140, t133.len);
  t142 = gg_view(((*t141)).data, 8);
  t143 = v24;
  t144 = ((gg_type_8 *)t142.data) + gg_index(t143, t142.len);
  t145 = (*t144);
  t146 = gg_function_39(t145, base + 0);
  t147 = v29;
  t148 = gg_function_22(t146, t147, base + 0);
  t149 = gg_function_23(t132, t148, base + 0);
  v17 = gg_function_29(t131, t149, base + 0);
  t150 = v18;
  t151 = v28;
  t152 = gg_view((((*(v7))).f55).data, 256);
  t153 = ((int64_t)(v14));
  t154 = ((int64_t)(((int64_t)UINT64_C(3))));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t154)))));
  t156 = ((int64_t)(v4));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) * (uint64_t)(t156)))));
  t158 = ((int64_t)(v5));
  t159 = ((int64_t)((uint64_t)(t157) + (uint64_t)(t158)));
  t160 = ((gg_type_9 *)t152.data) + gg_index(t159, t152.len);
  t161 = gg_view(((*t160)).data, 8);
  t162 = v24;
  t163 = ((gg_type_8 *)t161.data) + gg_index(t162, t161.len);
  t164 = (*t163);
  t165 = gg_function_39(t164, base + 0);
  t166 = v29;
  t167 = gg_function_22(t165, t166, base + 0);
  t168 = gg_function_23(t151, t167, base + 0);
  v18 = gg_function_29(t150, t168, base + 0);
  t169 = v19;
  t170 = v28;
  t171 = gg_view((((*(v7))).f55).data, 256);
  t172 = ((int64_t)(v14));
  t173 = ((int64_t)(((int64_t)UINT64_C(4))));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t172) + (uint64_t)(t173)))));
  t175 = ((int64_t)(v4));
  t176 = ((int64_t)(((int64_t)((uint64_t)(t174) * (uint64_t)(t175)))));
  t177 = ((int64_t)(v5));
  t178 = ((int64_t)((uint64_t)(t176) + (uint64_t)(t177)));
  t179 = ((gg_type_9 *)t171.data) + gg_index(t178, t171.len);
  t180 = gg_view(((*t179)).data, 8);
  t181 = v24;
  t182 = ((gg_type_8 *)t180.data) + gg_index(t181, t180.len);
  t183 = (*t182);
  t184 = gg_function_39(t183, base + 0);
  t185 = v29;
  t186 = gg_function_22(t184, t185, base + 0);
  t187 = gg_function_23(t170, t186, base + 0);
  v19 = gg_function_29(t169, t187, base + 0);
  t188 = v20;
  t189 = v28;
  t190 = gg_view((((*(v7))).f55).data, 256);
  t191 = ((int64_t)(v14));
  t192 = ((int64_t)(((int64_t)UINT64_C(5))));
  t193 = ((int64_t)(((int64_t)((uint64_t)(t191) + (uint64_t)(t192)))));
  t194 = ((int64_t)(v4));
  t195 = ((int64_t)(((int64_t)((uint64_t)(t193) * (uint64_t)(t194)))));
  t196 = ((int64_t)(v5));
  t197 = ((int64_t)((uint64_t)(t195) + (uint64_t)(t196)));
  t198 = ((gg_type_9 *)t190.data) + gg_index(t197, t190.len);
  t199 = gg_view(((*t198)).data, 8);
  t200 = v24;
  t201 = ((gg_type_8 *)t199.data) + gg_index(t200, t199.len);
  t202 = (*t201);
  t203 = gg_function_39(t202, base + 0);
  t204 = v29;
  t205 = gg_function_22(t203, t204, base + 0);
  t206 = gg_function_23(t189, t205, base + 0);
  v20 = gg_function_29(t188, t206, base + 0);
  t207 = v21;
  t208 = v28;
  t209 = gg_view((((*(v7))).f55).data, 256);
  t210 = ((int64_t)(v14));
  t211 = ((int64_t)(((int64_t)UINT64_C(6))));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t210) + (uint64_t)(t211)))));
  t213 = ((int64_t)(v4));
  t214 = ((int64_t)(((int64_t)((uint64_t)(t212) * (uint64_t)(t213)))));
  t215 = ((int64_t)(v5));
  t216 = ((int64_t)((uint64_t)(t214) + (uint64_t)(t215)));
  t217 = ((gg_type_9 *)t209.data) + gg_index(t216, t209.len);
  t218 = gg_view(((*t217)).data, 8);
  t219 = v24;
  t220 = ((gg_type_8 *)t218.data) + gg_index(t219, t218.len);
  t221 = (*t220);
  t222 = gg_function_39(t221, base + 0);
  t223 = v29;
  t224 = gg_function_22(t222, t223, base + 0);
  t225 = gg_function_23(t208, t224, base + 0);
  v21 = gg_function_29(t207, t225, base + 0);
  t226 = v22;
  t227 = v28;
  t228 = gg_view((((*(v7))).f55).data, 256);
  t229 = ((int64_t)(v14));
  t230 = ((int64_t)(((int64_t)UINT64_C(7))));
  t231 = ((int64_t)(((int64_t)((uint64_t)(t229) + (uint64_t)(t230)))));
  t232 = ((int64_t)(v4));
  t233 = ((int64_t)(((int64_t)((uint64_t)(t231) * (uint64_t)(t232)))));
  t234 = ((int64_t)(v5));
  t235 = ((int64_t)((uint64_t)(t233) + (uint64_t)(t234)));
  t236 = ((gg_type_9 *)t228.data) + gg_index(t235, t228.len);
  t237 = gg_view(((*t236)).data, 8);
  t238 = v24;
  t239 = ((gg_type_8 *)t237.data) + gg_index(t238, t237.len);
  t240 = (*t239);
  t241 = gg_function_39(t240, base + 0);
  t242 = v29;
  t243 = gg_function_22(t241, t242, base + 0);
  t244 = gg_function_23(t227, t243, base + 0);
  v22 = gg_function_29(t226, t244, base + 0);
  }
label7:;
  t48++; goto label6;
label8:;
  memset(&t245, 0, sizeof(t245));
  t246 = ((int32_t)(t245));
  for (t247=0; t247<8; t247++) (v30).data[t247] = t246;
  /* FG_DIRECT_SLICE_INDEX */
  t248 = ((int64_t)UINT64_C(0));
  t249 = ((int32_t *)(v30).data) + gg_index(t248, ((int64_t)UINT64_C(8)));
  t250 = v15;
  (*t249) = gg_function_24(t250, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t251 = ((int64_t)UINT64_C(1));
  t252 = ((int32_t *)(v30).data) + gg_index(t251, ((int64_t)UINT64_C(8)));
  t253 = v16;
  (*t252) = gg_function_24(t253, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t254 = ((int64_t)UINT64_C(2));
  t255 = ((int32_t *)(v30).data) + gg_index(t254, ((int64_t)UINT64_C(8)));
  t256 = v17;
  (*t255) = gg_function_24(t256, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t257 = ((int64_t)UINT64_C(3));
  t258 = ((int32_t *)(v30).data) + gg_index(t257, ((int64_t)UINT64_C(8)));
  t259 = v18;
  (*t258) = gg_function_24(t259, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t260 = ((int64_t)UINT64_C(4));
  t261 = ((int32_t *)(v30).data) + gg_index(t260, ((int64_t)UINT64_C(8)));
  t262 = v19;
  (*t261) = gg_function_24(t262, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t263 = ((int64_t)UINT64_C(5));
  t264 = ((int32_t *)(v30).data) + gg_index(t263, ((int64_t)UINT64_C(8)));
  t265 = v20;
  (*t264) = gg_function_24(t265, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t266 = ((int64_t)UINT64_C(6));
  t267 = ((int32_t *)(v30).data) + gg_index(t266, ((int64_t)UINT64_C(8)));
  t268 = v21;
  (*t267) = gg_function_24(t268, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t269 = ((int64_t)UINT64_C(7));
  t270 = ((int32_t *)(v30).data) + gg_index(t269, ((int64_t)UINT64_C(8)));
  t271 = v22;
  (*t270) = gg_function_24(t271, base + 0);
  t272 = v30;
  t273 = gg_view((((*(v6))).f53).data, 512);
  t274 = ((int64_t)(v12));
  t275 = ((int64_t)(((int64_t)UINT64_C(2))));
  t276 = ((int64_t)((uint64_t)(t274) * (uint64_t)(t275)));
  t277 = ((gg_type_22 *)t273.data) + gg_index(t276, t273.len);
  t278 = gg_view(((*t277)).data, 8);
  t279 = ((int64_t)UINT64_C(0));
  t280 = ((int32_t *)t278.data) + gg_index(t279, t278.len);
  t281 = ((int32_t)((*t280)));
  t282 = gg_function_30(t281, base + 0);
  v30 = gg_function_40(t272, t282, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t283 = v11;
  t284 = ((float *)(((*(v6))).f50).data) + gg_index(t283, ((int64_t)UINT64_C(256)));
  t285 = ((float)((*t284)));
  t286 = v30;
  v31 = gg_function_32(t285, t286, base + 0);
  t287 = ((int64_t)(v3));
  t288 = ((int64_t)(v9));
  t289 = ((int64_t)(((int64_t)((uint64_t)(t287) + (uint64_t)(t288)))));
  t290 = ((int64_t)(((int64_t)UINT64_C(32))));
  t291 = ((int64_t)(((int64_t)((uint64_t)(t289) * (uint64_t)(t290)))));
  t292 = ((int64_t)(v14));
  t293 = ((int64_t)(((int64_t)UINT64_C(8))));
  t294 = ((int64_t)(((int64_t)gg_div(t292, t293))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t291) + (uint64_t)(t294)))));
  t296 = ((int64_t)UINT64_C(8));
  t295 = 0;
label9:;
  if (t295 >= t296) goto label11;
  v33 = t295;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t297 = v32;
  t298 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t297, ((int64_t)UINT64_C(8192)));
  t299 = gg_view(((*t298)).data, 8);
  t300 = v33;
  t301 = ((float *)t299.data) + gg_index(t300, t299.len);
  /* FG_DIRECT_SLICE_INDEX */
  t302 = v33;
  t303 = ((float *)(v31).data) + gg_index(t302, ((int64_t)UINT64_C(8)));
  t304 = ((float)((*t303)));
  t305 = gg_view((((*(v7))).f57).data, 256);
  t306 = ((int64_t)(v5));
  t307 = ((int64_t)(((int64_t)UINT64_C(16))));
  t308 = ((int64_t)(((int64_t)((uint64_t)(t306) * (uint64_t)(t307)))));
  t309 = ((int64_t)(v14));
  t310 = ((int64_t)(((int64_t)((uint64_t)(t308) + (uint64_t)(t309)))));
  t311 = ((int64_t)(v33));
  t312 = ((int64_t)((uint64_t)(t310) + (uint64_t)(t311)));
  t313 = ((float *)t305.data) + gg_index(t312, t305.len);
  t314 = ((float)((*t313)));
  /* FG_DIRECT_SLICE_INDEX */
  t315 = v32;
  t316 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t315, ((int64_t)UINT64_C(8192)));
  t317 = gg_view(((*t316)).data, 8);
  t318 = v33;
  t319 = ((float *)t317.data) + gg_index(t318, t317.len);
  t320 = ((float)((*t319)));
  t321 = fmaf(t304, t314, t320);
  (*t301) = ((float)(t321));
  }
label10:;
  t295++; goto label9;
label11:;
  }
label4:;
  t15++; goto label3;
label5:;
  }
label1:;
  t3++; goto label0;
label2:;
  }
gg_done:;
}
static void gg_function_36(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  int32_t t21;
  int32_t t22;
  int64_t t23;
  int32_t t24;
  int32_t t25;
  int64_t t26;
  int32_t t27;
  int32_t t28;
  int64_t t29;
  int32_t t30;
  int32_t t31;
  int64_t t32;
  int32_t t33;
  int32_t t34;
  int64_t t35;
  int32_t t36;
  int32_t t37;
  int64_t t38;
  int32_t t39;
  int32_t t40;
  int64_t t41;
  int32_t t42;
  int32_t t43;
  int64_t t44;
  int32_t t45;
  int32_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  gg_type_19 * t51;
  gg_slice t52;
  int64_t t53;
  gg_type_18 * t54;
  gg_slice t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  gg_type_22 * t63;
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int32_t * t70;
  gg_slice t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  gg_type_22 * t79;
  gg_slice t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int32_t * t88;
  int16_t t89;
  int16_t t90;
  gg_type_18 t91;
  uint8_t t92;
  gg_type_22 t93;
  gg_type_11 t94;
  gg_slice t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  gg_type_9 * t103;
  gg_slice t104;
  int64_t t105;
  gg_type_8 * t106;
  gg_type_8 t107;
  gg_type_18 t108;
  gg_type_8 t109;
  gg_type_11 t110;
  gg_type_22 t111;
  gg_type_22 t112;
  gg_type_11 t113;
  gg_slice t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  gg_type_9 * t122;
  gg_slice t123;
  int64_t t124;
  gg_type_8 * t125;
  gg_type_8 t126;
  gg_type_18 t127;
  gg_type_8 t128;
  gg_type_11 t129;
  gg_type_22 t130;
  gg_type_22 t131;
  gg_type_11 t132;
  gg_slice t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  gg_type_9 * t141;
  gg_slice t142;
  int64_t t143;
  gg_type_8 * t144;
  gg_type_8 t145;
  gg_type_18 t146;
  gg_type_8 t147;
  gg_type_11 t148;
  gg_type_22 t149;
  gg_type_22 t150;
  gg_type_11 t151;
  gg_slice t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  gg_type_9 * t160;
  gg_slice t161;
  int64_t t162;
  gg_type_8 * t163;
  gg_type_8 t164;
  gg_type_18 t165;
  gg_type_8 t166;
  gg_type_11 t167;
  gg_type_22 t168;
  gg_type_22 t169;
  gg_type_11 t170;
  gg_slice t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  gg_type_9 * t179;
  gg_slice t180;
  int64_t t181;
  gg_type_8 * t182;
  gg_type_8 t183;
  gg_type_18 t184;
  gg_type_8 t185;
  gg_type_11 t186;
  gg_type_22 t187;
  gg_type_22 t188;
  gg_type_11 t189;
  gg_slice t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  gg_type_9 * t198;
  gg_slice t199;
  int64_t t200;
  gg_type_8 * t201;
  gg_type_8 t202;
  gg_type_18 t203;
  gg_type_8 t204;
  gg_type_11 t205;
  gg_type_22 t206;
  gg_type_22 t207;
  gg_type_11 t208;
  gg_slice t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  gg_type_9 * t217;
  gg_slice t218;
  int64_t t219;
  gg_type_8 * t220;
  gg_type_8 t221;
  gg_type_18 t222;
  gg_type_8 t223;
  gg_type_11 t224;
  gg_type_22 t225;
  gg_type_22 t226;
  gg_type_11 t227;
  gg_slice t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  gg_type_9 * t236;
  gg_slice t237;
  int64_t t238;
  gg_type_8 * t239;
  gg_type_8 t240;
  gg_type_18 t241;
  gg_type_8 t242;
  gg_type_11 t243;
  gg_type_22 t244;
  int32_t t245;
  int32_t t246;
  int64_t t247;
  int64_t t248;
  int32_t * t249;
  gg_type_22 t250;
  int64_t t251;
  int32_t * t252;
  gg_type_22 t253;
  int64_t t254;
  int32_t * t255;
  gg_type_22 t256;
  int64_t t257;
  int32_t * t258;
  gg_type_22 t259;
  int64_t t260;
  int32_t * t261;
  gg_type_22 t262;
  int64_t t263;
  int32_t * t264;
  gg_type_22 t265;
  int64_t t266;
  int32_t * t267;
  gg_type_22 t268;
  int64_t t269;
  int32_t * t270;
  gg_type_22 t271;
  gg_type_22 t272;
  gg_slice t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  gg_type_22 * t277;
  gg_slice t278;
  int64_t t279;
  int32_t * t280;
  int32_t t281;
  gg_type_22 t282;
  int64_t t283;
  float * t284;
  float t285;
  gg_type_22 t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  gg_type_26 * t298;
  gg_slice t299;
  int64_t t300;
  float * t301;
  int64_t t302;
  float * t303;
  float t304;
  gg_slice t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  float * t313;
  float t314;
  int64_t t315;
  gg_type_26 * t316;
  gg_slice t317;
  int64_t t318;
  float * t319;
  float t320;
  float t321;
  gg_type_41 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_50 v6;
  gg_type_55 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  gg_type_22 v15;
  gg_type_22 v16;
  gg_type_22 v17;
  gg_type_22 v18;
  gg_type_22 v19;
  gg_type_22 v20;
  gg_type_22 v21;
  gg_type_22 v22;
  gg_type_22 v23;
  int64_t v24;
  gg_type_18 v25;
  int32_t v26;
  int32_t v27;
  gg_type_11 v28;
  gg_type_8 v29;
  gg_type_22 v30;
  gg_type_26 v31;
  int64_t v32;
  int64_t v33;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  v6 = &(((*(v0))).f54);
  v7 = &(((*(v0))).f58);
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  } else {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  }
  t4 = ((int64_t)UINT64_C(16));
  t3 = 0;
label0:;
  if (t3 >= t4) goto label2;
  v9 = t3;
  {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v9));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(v10));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t11 = ((int64_t)(v10));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(v5));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)UINT64_C(2));
  t15 = 0;
label3:;
  if (t15 >= t16) goto label5;
  v13 = t15;
  {
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(v13));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t20)))));
  memset(&t21, 0, sizeof(t21));
  t22 = ((int32_t)(t21));
  for (t23=0; t23<8; t23++) (v15).data[t23] = t22;
  memset(&t24, 0, sizeof(t24));
  t25 = ((int32_t)(t24));
  for (t26=0; t26<8; t26++) (v16).data[t26] = t25;
  memset(&t27, 0, sizeof(t27));
  t28 = ((int32_t)(t27));
  for (t29=0; t29<8; t29++) (v17).data[t29] = t28;
  memset(&t30, 0, sizeof(t30));
  t31 = ((int32_t)(t30));
  for (t32=0; t32<8; t32++) (v18).data[t32] = t31;
  memset(&t33, 0, sizeof(t33));
  t34 = ((int32_t)(t33));
  for (t35=0; t35<8; t35++) (v19).data[t35] = t34;
  memset(&t36, 0, sizeof(t36));
  t37 = ((int32_t)(t36));
  for (t38=0; t38<8; t38++) (v20).data[t38] = t37;
  memset(&t39, 0, sizeof(t39));
  t40 = ((int32_t)(t39));
  for (t41=0; t41<8; t41++) (v21).data[t41] = t40;
  memset(&t42, 0, sizeof(t42));
  t43 = ((int32_t)(t42));
  for (t44=0; t44<8; t44++) (v22).data[t44] = t43;
  memset(&t45, 0, sizeof(t45));
  t46 = ((int32_t)(t45));
  for (t47=0; t47<8; t47++) (v23).data[t47] = t46;
  t49 = ((int64_t)UINT64_C(8));
  t48 = 0;
label6:;
  if (t48 >= t49) goto label8;
  v24 = t48;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t50 = v12;
  t51 = ((gg_type_19 *)(((*(v6))).f49).data) + gg_index(t50, ((int64_t)UINT64_C(256)));
  t52 = gg_view(((*t51)).data, 8);
  t53 = v24;
  t54 = ((gg_type_18 *)t52.data) + gg_index(t53, t52.len);
  v25 = (*t54);
  t55 = gg_view((((*(v6))).f52).data, 512);
  t56 = ((int64_t)(v12));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  t59 = ((int64_t)(v24));
  t60 = ((int64_t)(((int64_t)UINT64_C(4))));
  t61 = ((int64_t)(((int64_t)gg_div(t59, t60))));
  t62 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t61)));
  t63 = ((gg_type_22 *)t55.data) + gg_index(t62, t55.len);
  t64 = gg_view(((*t63)).data, 8);
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t66 = ((int64_t)(v24));
  t67 = ((int64_t)(((int64_t)UINT64_C(4))));
  t68 = ((int64_t)(((int64_t)gg_mod(t66, t67))));
  t69 = ((int64_t)((uint64_t)(t65) * (uint64_t)(t68)));
  t70 = ((int32_t *)t64.data) + gg_index(t69, t64.len);
  v26 = ((int32_t)((*t70)));
  t71 = gg_view((((*(v6))).f52).data, 512);
  t72 = ((int64_t)(v12));
  t73 = ((int64_t)(((int64_t)UINT64_C(2))));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  t75 = ((int64_t)(v24));
  t76 = ((int64_t)(((int64_t)UINT64_C(4))));
  t77 = ((int64_t)(((int64_t)gg_div(t75, t76))));
  t78 = ((int64_t)((uint64_t)(t74) + (uint64_t)(t77)));
  t79 = ((gg_type_22 *)t71.data) + gg_index(t78, t71.len);
  t80 = gg_view(((*t79)).data, 8);
  t81 = ((int64_t)(((int64_t)UINT64_C(2))));
  t82 = ((int64_t)(v24));
  t83 = ((int64_t)(((int64_t)UINT64_C(4))));
  t84 = ((int64_t)(((int64_t)gg_mod(t82, t83))));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t84)))));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((int64_t)((uint64_t)(t85) + (uint64_t)(t86)));
  t88 = ((int32_t *)t80.data) + gg_index(t87, t80.len);
  v27 = ((int32_t)((*t88)));
  t89 = ((int16_t)(((int16_t)(v26))));
  t90 = ((int16_t)(((int16_t)(v27))));
  v28 = gg_function_37(t89, t90, base + 0);
  t91 = v25;
  t92 = ((uint8_t)(((int64_t)UINT64_C(32))));
  v29 = gg_function_38(t91, t92, base + 0);
  t93 = v15;
  t94 = v28;
  t95 = gg_view((((*(v7))).f55).data, 256);
  t96 = ((int64_t)(v14));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  t99 = ((int64_t)(v4));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  t101 = ((int64_t)(v5));
  t102 = ((int64_t)((uint64_t)(t100) + (uint64_t)(t101)));
  t103 = ((gg_type_9 *)t95.data) + gg_index(t102, t95.len);
  t104 = gg_view(((*t103)).data, 8);
  t105 = v24;
  t106 = ((gg_type_8 *)t104.data) + gg_index(t105, t104.len);
  t107 = (*t106);
  t108 = gg_function_39(t107, base + 0);
  t109 = v29;
  t110 = gg_function_22(t108, t109, base + 0);
  t111 = gg_function_23(t94, t110, base + 0);
  v15 = gg_function_29(t93, t111, base + 0);
  t112 = v16;
  t113 = v28;
  t114 = gg_view((((*(v7))).f55).data, 256);
  t115 = ((int64_t)(v14));
  t116 = ((int64_t)(((int64_t)UINT64_C(1))));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = ((int64_t)(v4));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) * (uint64_t)(t118)))));
  t120 = ((int64_t)(v5));
  t121 = ((int64_t)((uint64_t)(t119) + (uint64_t)(t120)));
  t122 = ((gg_type_9 *)t114.data) + gg_index(t121, t114.len);
  t123 = gg_view(((*t122)).data, 8);
  t124 = v24;
  t125 = ((gg_type_8 *)t123.data) + gg_index(t124, t123.len);
  t126 = (*t125);
  t127 = gg_function_39(t126, base + 0);
  t128 = v29;
  t129 = gg_function_22(t127, t128, base + 0);
  t130 = gg_function_23(t113, t129, base + 0);
  v16 = gg_function_29(t112, t130, base + 0);
  t131 = v17;
  t132 = v28;
  t133 = gg_view((((*(v7))).f55).data, 256);
  t134 = ((int64_t)(v14));
  t135 = ((int64_t)(((int64_t)UINT64_C(2))));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t134) + (uint64_t)(t135)))));
  t137 = ((int64_t)(v4));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t137)))));
  t139 = ((int64_t)(v5));
  t140 = ((int64_t)((uint64_t)(t138) + (uint64_t)(t139)));
  t141 = ((gg_type_9 *)t133.data) + gg_index(t140, t133.len);
  t142 = gg_view(((*t141)).data, 8);
  t143 = v24;
  t144 = ((gg_type_8 *)t142.data) + gg_index(t143, t142.len);
  t145 = (*t144);
  t146 = gg_function_39(t145, base + 0);
  t147 = v29;
  t148 = gg_function_22(t146, t147, base + 0);
  t149 = gg_function_23(t132, t148, base + 0);
  v17 = gg_function_29(t131, t149, base + 0);
  t150 = v18;
  t151 = v28;
  t152 = gg_view((((*(v7))).f55).data, 256);
  t153 = ((int64_t)(v14));
  t154 = ((int64_t)(((int64_t)UINT64_C(3))));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t154)))));
  t156 = ((int64_t)(v4));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) * (uint64_t)(t156)))));
  t158 = ((int64_t)(v5));
  t159 = ((int64_t)((uint64_t)(t157) + (uint64_t)(t158)));
  t160 = ((gg_type_9 *)t152.data) + gg_index(t159, t152.len);
  t161 = gg_view(((*t160)).data, 8);
  t162 = v24;
  t163 = ((gg_type_8 *)t161.data) + gg_index(t162, t161.len);
  t164 = (*t163);
  t165 = gg_function_39(t164, base + 0);
  t166 = v29;
  t167 = gg_function_22(t165, t166, base + 0);
  t168 = gg_function_23(t151, t167, base + 0);
  v18 = gg_function_29(t150, t168, base + 0);
  t169 = v19;
  t170 = v28;
  t171 = gg_view((((*(v7))).f55).data, 256);
  t172 = ((int64_t)(v14));
  t173 = ((int64_t)(((int64_t)UINT64_C(4))));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t172) + (uint64_t)(t173)))));
  t175 = ((int64_t)(v4));
  t176 = ((int64_t)(((int64_t)((uint64_t)(t174) * (uint64_t)(t175)))));
  t177 = ((int64_t)(v5));
  t178 = ((int64_t)((uint64_t)(t176) + (uint64_t)(t177)));
  t179 = ((gg_type_9 *)t171.data) + gg_index(t178, t171.len);
  t180 = gg_view(((*t179)).data, 8);
  t181 = v24;
  t182 = ((gg_type_8 *)t180.data) + gg_index(t181, t180.len);
  t183 = (*t182);
  t184 = gg_function_39(t183, base + 0);
  t185 = v29;
  t186 = gg_function_22(t184, t185, base + 0);
  t187 = gg_function_23(t170, t186, base + 0);
  v19 = gg_function_29(t169, t187, base + 0);
  t188 = v20;
  t189 = v28;
  t190 = gg_view((((*(v7))).f55).data, 256);
  t191 = ((int64_t)(v14));
  t192 = ((int64_t)(((int64_t)UINT64_C(5))));
  t193 = ((int64_t)(((int64_t)((uint64_t)(t191) + (uint64_t)(t192)))));
  t194 = ((int64_t)(v4));
  t195 = ((int64_t)(((int64_t)((uint64_t)(t193) * (uint64_t)(t194)))));
  t196 = ((int64_t)(v5));
  t197 = ((int64_t)((uint64_t)(t195) + (uint64_t)(t196)));
  t198 = ((gg_type_9 *)t190.data) + gg_index(t197, t190.len);
  t199 = gg_view(((*t198)).data, 8);
  t200 = v24;
  t201 = ((gg_type_8 *)t199.data) + gg_index(t200, t199.len);
  t202 = (*t201);
  t203 = gg_function_39(t202, base + 0);
  t204 = v29;
  t205 = gg_function_22(t203, t204, base + 0);
  t206 = gg_function_23(t189, t205, base + 0);
  v20 = gg_function_29(t188, t206, base + 0);
  t207 = v21;
  t208 = v28;
  t209 = gg_view((((*(v7))).f55).data, 256);
  t210 = ((int64_t)(v14));
  t211 = ((int64_t)(((int64_t)UINT64_C(6))));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t210) + (uint64_t)(t211)))));
  t213 = ((int64_t)(v4));
  t214 = ((int64_t)(((int64_t)((uint64_t)(t212) * (uint64_t)(t213)))));
  t215 = ((int64_t)(v5));
  t216 = ((int64_t)((uint64_t)(t214) + (uint64_t)(t215)));
  t217 = ((gg_type_9 *)t209.data) + gg_index(t216, t209.len);
  t218 = gg_view(((*t217)).data, 8);
  t219 = v24;
  t220 = ((gg_type_8 *)t218.data) + gg_index(t219, t218.len);
  t221 = (*t220);
  t222 = gg_function_39(t221, base + 0);
  t223 = v29;
  t224 = gg_function_22(t222, t223, base + 0);
  t225 = gg_function_23(t208, t224, base + 0);
  v21 = gg_function_29(t207, t225, base + 0);
  t226 = v22;
  t227 = v28;
  t228 = gg_view((((*(v7))).f55).data, 256);
  t229 = ((int64_t)(v14));
  t230 = ((int64_t)(((int64_t)UINT64_C(7))));
  t231 = ((int64_t)(((int64_t)((uint64_t)(t229) + (uint64_t)(t230)))));
  t232 = ((int64_t)(v4));
  t233 = ((int64_t)(((int64_t)((uint64_t)(t231) * (uint64_t)(t232)))));
  t234 = ((int64_t)(v5));
  t235 = ((int64_t)((uint64_t)(t233) + (uint64_t)(t234)));
  t236 = ((gg_type_9 *)t228.data) + gg_index(t235, t228.len);
  t237 = gg_view(((*t236)).data, 8);
  t238 = v24;
  t239 = ((gg_type_8 *)t237.data) + gg_index(t238, t237.len);
  t240 = (*t239);
  t241 = gg_function_39(t240, base + 0);
  t242 = v29;
  t243 = gg_function_22(t241, t242, base + 0);
  t244 = gg_function_23(t227, t243, base + 0);
  v22 = gg_function_29(t226, t244, base + 0);
  }
label7:;
  t48++; goto label6;
label8:;
  memset(&t245, 0, sizeof(t245));
  t246 = ((int32_t)(t245));
  for (t247=0; t247<8; t247++) (v30).data[t247] = t246;
  /* FG_DIRECT_SLICE_INDEX */
  t248 = ((int64_t)UINT64_C(0));
  t249 = ((int32_t *)(v30).data) + gg_index(t248, ((int64_t)UINT64_C(8)));
  t250 = v15;
  (*t249) = gg_function_24(t250, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t251 = ((int64_t)UINT64_C(1));
  t252 = ((int32_t *)(v30).data) + gg_index(t251, ((int64_t)UINT64_C(8)));
  t253 = v16;
  (*t252) = gg_function_24(t253, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t254 = ((int64_t)UINT64_C(2));
  t255 = ((int32_t *)(v30).data) + gg_index(t254, ((int64_t)UINT64_C(8)));
  t256 = v17;
  (*t255) = gg_function_24(t256, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t257 = ((int64_t)UINT64_C(3));
  t258 = ((int32_t *)(v30).data) + gg_index(t257, ((int64_t)UINT64_C(8)));
  t259 = v18;
  (*t258) = gg_function_24(t259, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t260 = ((int64_t)UINT64_C(4));
  t261 = ((int32_t *)(v30).data) + gg_index(t260, ((int64_t)UINT64_C(8)));
  t262 = v19;
  (*t261) = gg_function_24(t262, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t263 = ((int64_t)UINT64_C(5));
  t264 = ((int32_t *)(v30).data) + gg_index(t263, ((int64_t)UINT64_C(8)));
  t265 = v20;
  (*t264) = gg_function_24(t265, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t266 = ((int64_t)UINT64_C(6));
  t267 = ((int32_t *)(v30).data) + gg_index(t266, ((int64_t)UINT64_C(8)));
  t268 = v21;
  (*t267) = gg_function_24(t268, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t269 = ((int64_t)UINT64_C(7));
  t270 = ((int32_t *)(v30).data) + gg_index(t269, ((int64_t)UINT64_C(8)));
  t271 = v22;
  (*t270) = gg_function_24(t271, base + 0);
  t272 = v30;
  t273 = gg_view((((*(v6))).f53).data, 512);
  t274 = ((int64_t)(v12));
  t275 = ((int64_t)(((int64_t)UINT64_C(2))));
  t276 = ((int64_t)((uint64_t)(t274) * (uint64_t)(t275)));
  t277 = ((gg_type_22 *)t273.data) + gg_index(t276, t273.len);
  t278 = gg_view(((*t277)).data, 8);
  t279 = ((int64_t)UINT64_C(0));
  t280 = ((int32_t *)t278.data) + gg_index(t279, t278.len);
  t281 = ((int32_t)((*t280)));
  t282 = gg_function_30(t281, base + 0);
  v30 = gg_function_40(t272, t282, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t283 = v11;
  t284 = ((float *)(((*(v6))).f50).data) + gg_index(t283, ((int64_t)UINT64_C(256)));
  t285 = ((float)((*t284)));
  t286 = v30;
  v31 = gg_function_32(t285, t286, base + 0);
  t287 = ((int64_t)(v3));
  t288 = ((int64_t)(v9));
  t289 = ((int64_t)(((int64_t)((uint64_t)(t287) + (uint64_t)(t288)))));
  t290 = ((int64_t)(((int64_t)UINT64_C(32))));
  t291 = ((int64_t)(((int64_t)((uint64_t)(t289) * (uint64_t)(t290)))));
  t292 = ((int64_t)(v14));
  t293 = ((int64_t)(((int64_t)UINT64_C(8))));
  t294 = ((int64_t)(((int64_t)gg_div(t292, t293))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t291) + (uint64_t)(t294)))));
  t296 = ((int64_t)UINT64_C(8));
  t295 = 0;
label9:;
  if (t295 >= t296) goto label11;
  v33 = t295;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t297 = v32;
  t298 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t297, ((int64_t)UINT64_C(8192)));
  t299 = gg_view(((*t298)).data, 8);
  t300 = v33;
  t301 = ((float *)t299.data) + gg_index(t300, t299.len);
  /* FG_DIRECT_SLICE_INDEX */
  t302 = v33;
  t303 = ((float *)(v31).data) + gg_index(t302, ((int64_t)UINT64_C(8)));
  t304 = ((float)((*t303)));
  t305 = gg_view((((*(v7))).f57).data, 256);
  t306 = ((int64_t)(v5));
  t307 = ((int64_t)(((int64_t)UINT64_C(16))));
  t308 = ((int64_t)(((int64_t)((uint64_t)(t306) * (uint64_t)(t307)))));
  t309 = ((int64_t)(v14));
  t310 = ((int64_t)(((int64_t)((uint64_t)(t308) + (uint64_t)(t309)))));
  t311 = ((int64_t)(v33));
  t312 = ((int64_t)((uint64_t)(t310) + (uint64_t)(t311)));
  t313 = ((float *)t305.data) + gg_index(t312, t305.len);
  t314 = ((float)((*t313)));
  /* FG_DIRECT_SLICE_INDEX */
  t315 = v32;
  t316 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t315, ((int64_t)UINT64_C(8192)));
  t317 = gg_view(((*t316)).data, 8);
  t318 = v33;
  t319 = ((float *)t317.data) + gg_index(t318, t317.len);
  t320 = ((float)((*t319)));
  t321 = fmaf(t304, t314, t320);
  (*t301) = ((float)(t321));
  }
label10:;
  t295++; goto label9;
label11:;
  }
label4:;
  t15++; goto label3;
label5:;
  }
label1:;
  t3++; goto label0;
label2:;
  }
gg_done:;
}
static void gg_function_28(gg_type_41 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  int32_t t21;
  int32_t t22;
  int64_t t23;
  int32_t t24;
  int32_t t25;
  int64_t t26;
  int32_t t27;
  int32_t t28;
  int64_t t29;
  int32_t t30;
  int32_t t31;
  int64_t t32;
  int32_t t33;
  int32_t t34;
  int64_t t35;
  int32_t t36;
  int32_t t37;
  int64_t t38;
  int32_t t39;
  int32_t t40;
  int64_t t41;
  int32_t t42;
  int32_t t43;
  int64_t t44;
  int32_t t45;
  int32_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  gg_type_19 * t51;
  gg_slice t52;
  int64_t t53;
  gg_type_18 * t54;
  int64_t t55;
  gg_type_22 * t56;
  gg_slice t57;
  int64_t t58;
  int32_t * t59;
  int16_t t60;
  gg_type_22 t61;
  gg_type_11 t62;
  gg_type_18 t63;
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  gg_type_9 * t72;
  gg_slice t73;
  int64_t t74;
  gg_type_8 * t75;
  gg_type_8 t76;
  gg_type_11 t77;
  gg_type_22 t78;
  gg_type_22 t79;
  gg_type_11 t80;
  gg_type_18 t81;
  gg_slice t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  gg_type_9 * t90;
  gg_slice t91;
  int64_t t92;
  gg_type_8 * t93;
  gg_type_8 t94;
  gg_type_11 t95;
  gg_type_22 t96;
  gg_type_22 t97;
  gg_type_11 t98;
  gg_type_18 t99;
  gg_slice t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  gg_type_9 * t108;
  gg_slice t109;
  int64_t t110;
  gg_type_8 * t111;
  gg_type_8 t112;
  gg_type_11 t113;
  gg_type_22 t114;
  gg_type_22 t115;
  gg_type_11 t116;
  gg_type_18 t117;
  gg_slice t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  gg_type_9 * t126;
  gg_slice t127;
  int64_t t128;
  gg_type_8 * t129;
  gg_type_8 t130;
  gg_type_11 t131;
  gg_type_22 t132;
  gg_type_22 t133;
  gg_type_11 t134;
  gg_type_18 t135;
  gg_slice t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  gg_type_9 * t144;
  gg_slice t145;
  int64_t t146;
  gg_type_8 * t147;
  gg_type_8 t148;
  gg_type_11 t149;
  gg_type_22 t150;
  gg_type_22 t151;
  gg_type_11 t152;
  gg_type_18 t153;
  gg_slice t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  gg_type_9 * t162;
  gg_slice t163;
  int64_t t164;
  gg_type_8 * t165;
  gg_type_8 t166;
  gg_type_11 t167;
  gg_type_22 t168;
  gg_type_22 t169;
  gg_type_11 t170;
  gg_type_18 t171;
  gg_slice t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  gg_type_9 * t180;
  gg_slice t181;
  int64_t t182;
  gg_type_8 * t183;
  gg_type_8 t184;
  gg_type_11 t185;
  gg_type_22 t186;
  gg_type_22 t187;
  gg_type_11 t188;
  gg_type_18 t189;
  gg_slice t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  gg_type_9 * t198;
  gg_slice t199;
  int64_t t200;
  gg_type_8 * t201;
  gg_type_8 t202;
  gg_type_11 t203;
  gg_type_22 t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
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
  gg_type_22 t231;
  int64_t t232;
  gg_type_22 * t233;
  gg_type_22 t234;
  int64_t t235;
  gg_type_22 * t236;
  gg_type_22 t237;
  gg_type_22 t238;
  int64_t t239;
  gg_type_22 * t240;
  gg_slice t241;
  int64_t t242;
  int32_t * t243;
  int32_t t244;
  gg_type_22 t245;
  gg_type_22 t246;
  int32_t t247;
  int32_t t248;
  int64_t t249;
  int64_t t250;
  int32_t * t251;
  gg_type_22 t252;
  int64_t t253;
  int32_t * t254;
  gg_type_22 t255;
  int64_t t256;
  int32_t * t257;
  gg_type_22 t258;
  int64_t t259;
  int32_t * t260;
  gg_type_22 t261;
  int64_t t262;
  int32_t * t263;
  gg_type_22 t264;
  int64_t t265;
  int32_t * t266;
  gg_type_22 t267;
  int64_t t268;
  int32_t * t269;
  gg_type_22 t270;
  int64_t t271;
  int32_t * t272;
  gg_type_22 t273;
  int64_t t274;
  float * t275;
  float t276;
  float t277;
  gg_type_22 t278;
  int64_t t279;
  float * t280;
  float t281;
  gg_type_22 t282;
  gg_type_26 t283;
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
  int64_t t294;
  gg_type_26 * t295;
  gg_slice t296;
  int64_t t297;
  float * t298;
  int64_t t299;
  float * t300;
  float t301;
  gg_slice t302;
  int64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  float * t310;
  float t311;
  int64_t t312;
  gg_type_26 * t313;
  gg_slice t314;
  int64_t t315;
  float * t316;
  float t317;
  float t318;
  gg_type_41 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_50 v6;
  gg_type_55 v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  gg_type_22 v15;
  gg_type_22 v16;
  gg_type_22 v17;
  gg_type_22 v18;
  gg_type_22 v19;
  gg_type_22 v20;
  gg_type_22 v21;
  gg_type_22 v22;
  gg_type_22 v23;
  int64_t v24;
  gg_type_18 v25;
  gg_type_11 v26;
  int64_t v27;
  int64_t v28;
  gg_type_22 v29;
  gg_type_26 v30;
  int64_t v31;
  int64_t v32;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  v6 = &(((*(v0))).f54);
  v7 = &(((*(v0))).f58);
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  } else {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  }
  t4 = ((int64_t)UINT64_C(16));
  t3 = 0;
label0:;
  if (t3 >= t4) goto label2;
  v9 = t3;
  {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v9));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(v10));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t11 = ((int64_t)(v10));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(v5));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)UINT64_C(2));
  t15 = 0;
label3:;
  if (t15 >= t16) goto label5;
  v13 = t15;
  {
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(v13));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t20)))));
  memset(&t21, 0, sizeof(t21));
  t22 = ((int32_t)(t21));
  for (t23=0; t23<8; t23++) (v15).data[t23] = t22;
  memset(&t24, 0, sizeof(t24));
  t25 = ((int32_t)(t24));
  for (t26=0; t26<8; t26++) (v16).data[t26] = t25;
  memset(&t27, 0, sizeof(t27));
  t28 = ((int32_t)(t27));
  for (t29=0; t29<8; t29++) (v17).data[t29] = t28;
  memset(&t30, 0, sizeof(t30));
  t31 = ((int32_t)(t30));
  for (t32=0; t32<8; t32++) (v18).data[t32] = t31;
  memset(&t33, 0, sizeof(t33));
  t34 = ((int32_t)(t33));
  for (t35=0; t35<8; t35++) (v19).data[t35] = t34;
  memset(&t36, 0, sizeof(t36));
  t37 = ((int32_t)(t36));
  for (t38=0; t38<8; t38++) (v20).data[t38] = t37;
  memset(&t39, 0, sizeof(t39));
  t40 = ((int32_t)(t39));
  for (t41=0; t41<8; t41++) (v21).data[t41] = t40;
  memset(&t42, 0, sizeof(t42));
  t43 = ((int32_t)(t42));
  for (t44=0; t44<8; t44++) (v22).data[t44] = t43;
  memset(&t45, 0, sizeof(t45));
  t46 = ((int32_t)(t45));
  for (t47=0; t47<8; t47++) (v23).data[t47] = t46;
  t49 = ((int64_t)UINT64_C(8));
  t48 = 0;
label6:;
  if (t48 >= t49) goto label8;
  v24 = t48;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t50 = v12;
  t51 = ((gg_type_19 *)(((*(v6))).f49).data) + gg_index(t50, ((int64_t)UINT64_C(256)));
  t52 = gg_view(((*t51)).data, 8);
  t53 = v24;
  t54 = ((gg_type_18 *)t52.data) + gg_index(t53, t52.len);
  v25 = (*t54);
  /* FG_DIRECT_SLICE_INDEX */
  t55 = v12;
  t56 = ((gg_type_22 *)(((*(v6))).f52).data) + gg_index(t55, ((int64_t)UINT64_C(512)));
  t57 = gg_view(((*t56)).data, 8);
  t58 = v24;
  t59 = ((int32_t *)t57.data) + gg_index(t58, t57.len);
  t60 = ((int16_t)(((int16_t)((*t59)))));
  v26 = gg_function_8(t60, base + 0);
  t61 = v15;
  t62 = v26;
  t63 = v25;
  t64 = gg_view((((*(v7))).f55).data, 256);
  t65 = ((int64_t)(v14));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = ((int64_t)(v4));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int64_t)(v5));
  t71 = ((int64_t)((uint64_t)(t69) + (uint64_t)(t70)));
  t72 = ((gg_type_9 *)t64.data) + gg_index(t71, t64.len);
  t73 = gg_view(((*t72)).data, 8);
  t74 = v24;
  t75 = ((gg_type_8 *)t73.data) + gg_index(t74, t73.len);
  t76 = (*t75);
  t77 = gg_function_22(t63, t76, base + 0);
  t78 = gg_function_23(t62, t77, base + 0);
  v15 = gg_function_29(t61, t78, base + 0);
  t79 = v16;
  t80 = v26;
  t81 = v25;
  t82 = gg_view((((*(v7))).f55).data, 256);
  t83 = ((int64_t)(v14));
  t84 = ((int64_t)(((int64_t)UINT64_C(1))));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t83) + (uint64_t)(t84)))));
  t86 = ((int64_t)(v4));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t86)))));
  t88 = ((int64_t)(v5));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((gg_type_9 *)t82.data) + gg_index(t89, t82.len);
  t91 = gg_view(((*t90)).data, 8);
  t92 = v24;
  t93 = ((gg_type_8 *)t91.data) + gg_index(t92, t91.len);
  t94 = (*t93);
  t95 = gg_function_22(t81, t94, base + 0);
  t96 = gg_function_23(t80, t95, base + 0);
  v16 = gg_function_29(t79, t96, base + 0);
  t97 = v17;
  t98 = v26;
  t99 = v25;
  t100 = gg_view((((*(v7))).f55).data, 256);
  t101 = ((int64_t)(v14));
  t102 = ((int64_t)(((int64_t)UINT64_C(2))));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) + (uint64_t)(t102)))));
  t104 = ((int64_t)(v4));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t104)))));
  t106 = ((int64_t)(v5));
  t107 = ((int64_t)((uint64_t)(t105) + (uint64_t)(t106)));
  t108 = ((gg_type_9 *)t100.data) + gg_index(t107, t100.len);
  t109 = gg_view(((*t108)).data, 8);
  t110 = v24;
  t111 = ((gg_type_8 *)t109.data) + gg_index(t110, t109.len);
  t112 = (*t111);
  t113 = gg_function_22(t99, t112, base + 0);
  t114 = gg_function_23(t98, t113, base + 0);
  v17 = gg_function_29(t97, t114, base + 0);
  t115 = v18;
  t116 = v26;
  t117 = v25;
  t118 = gg_view((((*(v7))).f55).data, 256);
  t119 = ((int64_t)(v14));
  t120 = ((int64_t)(((int64_t)UINT64_C(3))));
  t121 = ((int64_t)(((int64_t)((uint64_t)(t119) + (uint64_t)(t120)))));
  t122 = ((int64_t)(v4));
  t123 = ((int64_t)(((int64_t)((uint64_t)(t121) * (uint64_t)(t122)))));
  t124 = ((int64_t)(v5));
  t125 = ((int64_t)((uint64_t)(t123) + (uint64_t)(t124)));
  t126 = ((gg_type_9 *)t118.data) + gg_index(t125, t118.len);
  t127 = gg_view(((*t126)).data, 8);
  t128 = v24;
  t129 = ((gg_type_8 *)t127.data) + gg_index(t128, t127.len);
  t130 = (*t129);
  t131 = gg_function_22(t117, t130, base + 0);
  t132 = gg_function_23(t116, t131, base + 0);
  v18 = gg_function_29(t115, t132, base + 0);
  t133 = v19;
  t134 = v26;
  t135 = v25;
  t136 = gg_view((((*(v7))).f55).data, 256);
  t137 = ((int64_t)(v14));
  t138 = ((int64_t)(((int64_t)UINT64_C(4))));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t137) + (uint64_t)(t138)))));
  t140 = ((int64_t)(v4));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t139) * (uint64_t)(t140)))));
  t142 = ((int64_t)(v5));
  t143 = ((int64_t)((uint64_t)(t141) + (uint64_t)(t142)));
  t144 = ((gg_type_9 *)t136.data) + gg_index(t143, t136.len);
  t145 = gg_view(((*t144)).data, 8);
  t146 = v24;
  t147 = ((gg_type_8 *)t145.data) + gg_index(t146, t145.len);
  t148 = (*t147);
  t149 = gg_function_22(t135, t148, base + 0);
  t150 = gg_function_23(t134, t149, base + 0);
  v19 = gg_function_29(t133, t150, base + 0);
  t151 = v20;
  t152 = v26;
  t153 = v25;
  t154 = gg_view((((*(v7))).f55).data, 256);
  t155 = ((int64_t)(v14));
  t156 = ((int64_t)(((int64_t)UINT64_C(5))));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) + (uint64_t)(t156)))));
  t158 = ((int64_t)(v4));
  t159 = ((int64_t)(((int64_t)((uint64_t)(t157) * (uint64_t)(t158)))));
  t160 = ((int64_t)(v5));
  t161 = ((int64_t)((uint64_t)(t159) + (uint64_t)(t160)));
  t162 = ((gg_type_9 *)t154.data) + gg_index(t161, t154.len);
  t163 = gg_view(((*t162)).data, 8);
  t164 = v24;
  t165 = ((gg_type_8 *)t163.data) + gg_index(t164, t163.len);
  t166 = (*t165);
  t167 = gg_function_22(t153, t166, base + 0);
  t168 = gg_function_23(t152, t167, base + 0);
  v20 = gg_function_29(t151, t168, base + 0);
  t169 = v21;
  t170 = v26;
  t171 = v25;
  t172 = gg_view((((*(v7))).f55).data, 256);
  t173 = ((int64_t)(v14));
  t174 = ((int64_t)(((int64_t)UINT64_C(6))));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) + (uint64_t)(t174)))));
  t176 = ((int64_t)(v4));
  t177 = ((int64_t)(((int64_t)((uint64_t)(t175) * (uint64_t)(t176)))));
  t178 = ((int64_t)(v5));
  t179 = ((int64_t)((uint64_t)(t177) + (uint64_t)(t178)));
  t180 = ((gg_type_9 *)t172.data) + gg_index(t179, t172.len);
  t181 = gg_view(((*t180)).data, 8);
  t182 = v24;
  t183 = ((gg_type_8 *)t181.data) + gg_index(t182, t181.len);
  t184 = (*t183);
  t185 = gg_function_22(t171, t184, base + 0);
  t186 = gg_function_23(t170, t185, base + 0);
  v21 = gg_function_29(t169, t186, base + 0);
  t187 = v22;
  t188 = v26;
  t189 = v25;
  t190 = gg_view((((*(v7))).f55).data, 256);
  t191 = ((int64_t)(v14));
  t192 = ((int64_t)(((int64_t)UINT64_C(7))));
  t193 = ((int64_t)(((int64_t)((uint64_t)(t191) + (uint64_t)(t192)))));
  t194 = ((int64_t)(v4));
  t195 = ((int64_t)(((int64_t)((uint64_t)(t193) * (uint64_t)(t194)))));
  t196 = ((int64_t)(v5));
  t197 = ((int64_t)((uint64_t)(t195) + (uint64_t)(t196)));
  t198 = ((gg_type_9 *)t190.data) + gg_index(t197, t190.len);
  t199 = gg_view(((*t198)).data, 8);
  t200 = v24;
  t201 = ((gg_type_8 *)t199.data) + gg_index(t200, t199.len);
  t202 = (*t201);
  t203 = gg_function_22(t189, t202, base + 0);
  t204 = gg_function_23(t188, t203, base + 0);
  v22 = gg_function_29(t187, t204, base + 0);
  t205 = ((int64_t)(v5));
  t206 = ((int64_t)(((int64_t)UINT64_C(16))));
  t207 = ((int64_t)(((int64_t)((uint64_t)(t205) * (uint64_t)(t206)))));
  t208 = ((int64_t)(((int64_t)UINT64_C(2))));
  t209 = ((int64_t)(v24));
  t210 = ((int64_t)(((int64_t)((uint64_t)(t208) * (uint64_t)(t209)))));
  t211 = ((int64_t)(((int64_t)((uint64_t)(t207) + (uint64_t)(t210)))));
  t212 = ((int64_t)(v8));
  t213 = ((int64_t)(((int64_t)((uint64_t)(t211) * (uint64_t)(t212)))));
  t214 = ((int64_t)(v14));
  t215 = ((int64_t)(((int64_t)((uint64_t)(t213) + (uint64_t)(t214)))));
  t216 = ((int64_t)(((int64_t)UINT64_C(8))));
  v27 = ((int64_t)(((int64_t)gg_div(t215, t216))));
  t217 = ((int64_t)(v5));
  t218 = ((int64_t)(((int64_t)UINT64_C(16))));
  t219 = ((int64_t)(((int64_t)((uint64_t)(t217) * (uint64_t)(t218)))));
  t220 = ((int64_t)(((int64_t)UINT64_C(2))));
  t221 = ((int64_t)(v24));
  t222 = ((int64_t)(((int64_t)((uint64_t)(t220) * (uint64_t)(t221)))));
  t223 = ((int64_t)(((int64_t)((uint64_t)(t219) + (uint64_t)(t222)))));
  t224 = ((int64_t)(((int64_t)UINT64_C(1))));
  t225 = ((int64_t)(((int64_t)((uint64_t)(t223) + (uint64_t)(t224)))));
  t226 = ((int64_t)(v8));
  t227 = ((int64_t)(((int64_t)((uint64_t)(t225) * (uint64_t)(t226)))));
  t228 = ((int64_t)(v14));
  t229 = ((int64_t)(((int64_t)((uint64_t)(t227) + (uint64_t)(t228)))));
  t230 = ((int64_t)(((int64_t)UINT64_C(8))));
  v28 = ((int64_t)(((int64_t)gg_div(t229, t230))));
  t231 = v23;
  /* FG_DIRECT_SLICE_INDEX */
  t232 = v27;
  t233 = ((gg_type_22 *)(((*(v7))).f56).data) + gg_index(t232, ((int64_t)UINT64_C(512)));
  t234 = (*t233);
  /* FG_DIRECT_SLICE_INDEX */
  t235 = v28;
  t236 = ((gg_type_22 *)(((*(v7))).f56).data) + gg_index(t235, ((int64_t)UINT64_C(512)));
  t237 = (*t236);
  t238 = gg_function_29(t234, t237, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t239 = v12;
  t240 = ((gg_type_22 *)(((*(v6))).f53).data) + gg_index(t239, ((int64_t)UINT64_C(512)));
  t241 = gg_view(((*t240)).data, 8);
  t242 = v24;
  t243 = ((int32_t *)t241.data) + gg_index(t242, t241.len);
  t244 = ((int32_t)((*t243)));
  t245 = gg_function_30(t244, base + 0);
  t246 = gg_function_31(t238, t245, base + 0);
  v23 = gg_function_29(t231, t246, base + 0);
  }
label7:;
  t48++; goto label6;
label8:;
  memset(&t247, 0, sizeof(t247));
  t248 = ((int32_t)(t247));
  for (t249=0; t249<8; t249++) (v29).data[t249] = t248;
  /* FG_DIRECT_SLICE_INDEX */
  t250 = ((int64_t)UINT64_C(0));
  t251 = ((int32_t *)(v29).data) + gg_index(t250, ((int64_t)UINT64_C(8)));
  t252 = v15;
  (*t251) = gg_function_24(t252, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t253 = ((int64_t)UINT64_C(1));
  t254 = ((int32_t *)(v29).data) + gg_index(t253, ((int64_t)UINT64_C(8)));
  t255 = v16;
  (*t254) = gg_function_24(t255, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t256 = ((int64_t)UINT64_C(2));
  t257 = ((int32_t *)(v29).data) + gg_index(t256, ((int64_t)UINT64_C(8)));
  t258 = v17;
  (*t257) = gg_function_24(t258, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t259 = ((int64_t)UINT64_C(3));
  t260 = ((int32_t *)(v29).data) + gg_index(t259, ((int64_t)UINT64_C(8)));
  t261 = v18;
  (*t260) = gg_function_24(t261, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t262 = ((int64_t)UINT64_C(4));
  t263 = ((int32_t *)(v29).data) + gg_index(t262, ((int64_t)UINT64_C(8)));
  t264 = v19;
  (*t263) = gg_function_24(t264, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t265 = ((int64_t)UINT64_C(5));
  t266 = ((int32_t *)(v29).data) + gg_index(t265, ((int64_t)UINT64_C(8)));
  t267 = v20;
  (*t266) = gg_function_24(t267, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t268 = ((int64_t)UINT64_C(6));
  t269 = ((int32_t *)(v29).data) + gg_index(t268, ((int64_t)UINT64_C(8)));
  t270 = v21;
  (*t269) = gg_function_24(t270, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t271 = ((int64_t)UINT64_C(7));
  t272 = ((int32_t *)(v29).data) + gg_index(t271, ((int64_t)UINT64_C(8)));
  t273 = v22;
  (*t272) = gg_function_24(t273, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t274 = v11;
  t275 = ((float *)(((*(v6))).f51).data) + gg_index(t274, ((int64_t)UINT64_C(256)));
  t276 = (*t275);
  t277 = ((float)((-(t276))));
  t278 = v23;
  /* FG_DIRECT_SLICE_INDEX */
  t279 = v11;
  t280 = ((float *)(((*(v6))).f50).data) + gg_index(t279, ((int64_t)UINT64_C(256)));
  t281 = ((float)((*t280)));
  t282 = v29;
  t283 = gg_function_32(t281, t282, base + 0);
  v30 = gg_function_33(t277, t278, t283, base + 0);
  t284 = ((int64_t)(v3));
  t285 = ((int64_t)(v9));
  t286 = ((int64_t)(((int64_t)((uint64_t)(t284) + (uint64_t)(t285)))));
  t287 = ((int64_t)(((int64_t)UINT64_C(32))));
  t288 = ((int64_t)(((int64_t)((uint64_t)(t286) * (uint64_t)(t287)))));
  t289 = ((int64_t)(v14));
  t290 = ((int64_t)(((int64_t)UINT64_C(8))));
  t291 = ((int64_t)(((int64_t)gg_div(t289, t290))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t288) + (uint64_t)(t291)))));
  t293 = ((int64_t)UINT64_C(8));
  t292 = 0;
label9:;
  if (t292 >= t293) goto label11;
  v32 = t292;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t294 = v31;
  t295 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t294, ((int64_t)UINT64_C(8192)));
  t296 = gg_view(((*t295)).data, 8);
  t297 = v32;
  t298 = ((float *)t296.data) + gg_index(t297, t296.len);
  /* FG_DIRECT_SLICE_INDEX */
  t299 = v32;
  t300 = ((float *)(v30).data) + gg_index(t299, ((int64_t)UINT64_C(8)));
  t301 = ((float)((*t300)));
  t302 = gg_view((((*(v7))).f57).data, 256);
  t303 = ((int64_t)(v5));
  t304 = ((int64_t)(((int64_t)UINT64_C(16))));
  t305 = ((int64_t)(((int64_t)((uint64_t)(t303) * (uint64_t)(t304)))));
  t306 = ((int64_t)(v14));
  t307 = ((int64_t)(((int64_t)((uint64_t)(t305) + (uint64_t)(t306)))));
  t308 = ((int64_t)(v32));
  t309 = ((int64_t)((uint64_t)(t307) + (uint64_t)(t308)));
  t310 = ((float *)t302.data) + gg_index(t309, t302.len);
  t311 = ((float)((*t310)));
  /* FG_DIRECT_SLICE_INDEX */
  t312 = v31;
  t313 = ((gg_type_26 *)(((*(v0))).f59).data) + gg_index(t312, ((int64_t)UINT64_C(8192)));
  t314 = gg_view(((*t313)).data, 8);
  t315 = v32;
  t316 = ((float *)t314.data) + gg_index(t315, t314.len);
  t317 = ((float)((*t316)));
  t318 = fmaf(t301, t311, t317);
  (*t298) = ((float)(t318));
  }
label10:;
  t292++; goto label9;
label11:;
  }
label4:;
  t15++; goto label3;
label5:;
  }
label1:;
  t3++; goto label0;
label2:;
  }
gg_done:;
}
static void gg_function_65(gg_type_37 p0, gg_type_41 p1, int64_t base) {
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
  uint8_t t14;
  uint8_t t15;
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
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
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
  uint8_t t48;
  uint8_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  gg_type_41 t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint8_t t78;
  gg_type_41 t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  uint8_t t87;
  gg_type_41 t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
  gg_type_41 t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  gg_type_41 t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint8_t t114;
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
  uint8_t t126;
  gg_type_41 t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  uint8_t t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  uint8_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  gg_type_41 t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  gg_type_37 v0;
  gg_type_41 v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(((*(v0))).f6));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(((*(v0))).f6));
  t5 = ((int64_t)(((int64_t)UINT64_C(16))));
  t3 = ((t4) <= (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(((*(v0))).f10));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) >= (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(((*(v0))).f10));
  t13 = ((int64_t)(((*(v0))).f6));
  t11 = ((t12) < (t13));
  }
  t14 = t11;
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(((*(v0))).f7));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) >= (t17));
  }
  t18 = t15;
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(((*(v0))).f8));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t20) >= (t21));
  }
  t22 = t19;
  t23 = t22;
  if (t23) {
  t24 = ((int64_t)(((*(v0))).f9));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t24) >= (t25));
  }
  t26 = t23;
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(((*(v0))).f7));
  t29 = ((int64_t)(((int64_t)UINT64_C(16))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(256))));
  t32 = ((int64_t)(((*(v0))).f6));
  t33 = ((int64_t)(((int64_t)gg_div(t31, t32))));
  t27 = ((t30) <= (t33));
  }
  t34 = t27;
  t35 = t34;
  if (t35) {
  t36 = ((int64_t)(((*(v0))).f8));
  t37 = ((int64_t)(((int64_t)UINT64_C(16))));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)UINT64_C(256))));
  t40 = ((int64_t)(((*(v0))).f6));
  t41 = ((int64_t)(((int64_t)gg_div(t39, t40))));
  t35 = ((t38) <= (t41));
  }
  t42 = t35;
  t43 = t42;
  if (t43) {
  t44 = ((int64_t)(((*(v0))).f9));
  t45 = ((int64_t)(((int64_t)UINT64_C(16))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)UINT64_C(256))));
  t43 = ((t46) <= (t47));
  }
  t48 = t43;
  t49 = t48;
  if (t49) {
  t50 = ((int64_t)(((*(v0))).f8));
  t51 = ((int64_t)(((int64_t)UINT64_C(16))));
  t52 = ((int64_t)(((int64_t)gg_mod(t50, t51))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((t52) == (t53));
  }
  if (!(t49)) gg_fail("assertion failed");
  t54 = ((int64_t)(((*(v0))).f6));
  t55 = ((int64_t)(((int64_t)UINT64_C(1))));
  t56 = ((t54) > (t55));
  if (t56) {
  {
  t57 = ((int64_t)(((*(v0))).f7));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((t57) == (t58));
  t60 = t59;
  if (t60) {
  t61 = ((int64_t)(((*(v0))).f8));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((t61) == (t62));
  }
  if (!(t60)) gg_fail("assertion failed");
  }
  }
  t63 = ((int64_t)(((*(v0))).f1));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((t63) == (t64));
  t66 = t65;
  if (!t66) {
  t67 = ((int64_t)(((*(v0))).f1));
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  t66 = ((t67) == (t68));
  }
  t69 = t66;
  if (t69) {
  {
  t70 = v1;
  t71 = ((int64_t)(((*(v0))).f7));
  t72 = ((int64_t)(((*(v0))).f8));
  t73 = ((int64_t)(((*(v0))).f9));
  t74 = ((int64_t)(((*(v0))).f6));
  t75 = ((int64_t)(((*(v0))).f10));
  gg_function_28(t70, t71, t72, t73, t74, t75, base + 0);
  }
  } else {
  t76 = ((int64_t)(((*(v0))).f1));
  t77 = ((int64_t)(((int64_t)UINT64_C(2))));
  t78 = ((t76) == (t77));
  if (t78) {
  {
  t79 = v1;
  t80 = ((int64_t)(((*(v0))).f7));
  t81 = ((int64_t)(((*(v0))).f8));
  t82 = ((int64_t)(((*(v0))).f9));
  t83 = ((int64_t)(((*(v0))).f6));
  t84 = ((int64_t)(((*(v0))).f10));
  gg_function_36(t79, t80, t81, t82, t83, t84, base + 0);
  }
  } else {
  t85 = ((int64_t)(((*(v0))).f1));
  t86 = ((int64_t)(((int64_t)UINT64_C(3))));
  t87 = ((t85) == (t86));
  if (t87) {
  {
  t88 = v1;
  t89 = ((int64_t)(((*(v0))).f7));
  t90 = ((int64_t)(((*(v0))).f8));
  t91 = ((int64_t)(((*(v0))).f9));
  t92 = ((int64_t)(((*(v0))).f6));
  t93 = ((int64_t)(((*(v0))).f10));
  gg_function_41(t88, t89, t90, t91, t92, t93, base + 0);
  }
  } else {
  t94 = ((int64_t)(((*(v0))).f1));
  t95 = ((int64_t)(((int64_t)UINT64_C(4))));
  t96 = ((t94) == (t95));
  if (t96) {
  {
  t97 = v1;
  t98 = ((int64_t)(((*(v0))).f7));
  t99 = ((int64_t)(((*(v0))).f8));
  t100 = ((int64_t)(((*(v0))).f9));
  t101 = ((int64_t)(((*(v0))).f6));
  t102 = ((int64_t)(((*(v0))).f10));
  gg_function_42(t97, t98, t99, t100, t101, t102, base + 0);
  }
  } else {
  t103 = ((int64_t)(((*(v0))).f1));
  t104 = ((int64_t)(((int64_t)UINT64_C(5))));
  t105 = ((t103) == (t104));
  if (t105) {
  {
  t106 = v1;
  t107 = ((int64_t)(((*(v0))).f7));
  t108 = ((int64_t)(((*(v0))).f8));
  t109 = ((int64_t)(((*(v0))).f9));
  t110 = ((int64_t)(((*(v0))).f6));
  t111 = ((int64_t)(((*(v0))).f10));
  gg_function_43(t106, t107, t108, t109, t110, t111, base + 0);
  }
  } else {
  t112 = ((int64_t)(((*(v0))).f1));
  t113 = ((int64_t)(((int64_t)UINT64_C(6))));
  t114 = ((t112) == (t113));
  t115 = t114;
  if (!t115) {
  t116 = ((int64_t)(((*(v0))).f1));
  t117 = ((int64_t)(((int64_t)UINT64_C(9))));
  t115 = ((t116) == (t117));
  }
  t118 = t115;
  t119 = t118;
  if (!t119) {
  t120 = ((int64_t)(((*(v0))).f1));
  t121 = ((int64_t)(((int64_t)UINT64_C(10))));
  t119 = ((t120) == (t121));
  }
  t122 = t119;
  t123 = t122;
  if (!t123) {
  t124 = ((int64_t)(((*(v0))).f1));
  t125 = ((int64_t)(((int64_t)UINT64_C(11))));
  t123 = ((t124) == (t125));
  }
  t126 = t123;
  if (t126) {
  {
  t127 = v1;
  t128 = ((int64_t)(((*(v0))).f7));
  t129 = ((int64_t)(((*(v0))).f8));
  t130 = ((int64_t)(((*(v0))).f9));
  t131 = ((int64_t)(((*(v0))).f6));
  t132 = ((int64_t)(((*(v0))).f10));
  gg_function_45(t127, t128, t129, t130, t131, t132, base + 0);
  }
  } else {
  {
  t133 = ((int64_t)(((*(v0))).f1));
  t134 = ((int64_t)(((int64_t)UINT64_C(7))));
  t135 = ((t133) == (t134));
  t136 = t135;
  if (!t136) {
  t137 = ((int64_t)(((*(v0))).f1));
  t138 = ((int64_t)(((int64_t)UINT64_C(8))));
  t136 = ((t137) == (t138));
  }
  t139 = t136;
  t140 = t139;
  if (!t140) {
  t141 = ((int64_t)(((*(v0))).f1));
  t142 = ((int64_t)(((int64_t)UINT64_C(12))));
  t140 = ((t141) == (t142));
  }
  if (!(t140)) gg_fail("assertion failed");
  t143 = v1;
  t144 = ((int64_t)(((*(v0))).f7));
  t145 = ((int64_t)(((*(v0))).f8));
  t146 = ((int64_t)(((*(v0))).f9));
  t147 = ((int64_t)(((*(v0))).f6));
  t148 = ((int64_t)(((*(v0))).f10));
  gg_function_46(t143, t144, t145, t146, t147, t148, base + 0);
  }
  }
  }
  }
  }
  }
  }
  t149 = ((int64_t)(((int64_t)UINT64_C(1))));
  t150 = ((int64_t)(((*(v0))).f1));
  t151 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg141_witness(t149, t150, t151);
  }
gg_done:;
}
static gg_type_8 gg_function_48(gg_type_8 p0, int64_t base) {
  gg_type_8 t0;
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
  gg_type_8 v0;
  gg_type_8 v1;
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
static void gg_function_54(gg_type_41 p0, int64_t p1, int64_t p2, uint8_t p3, int64_t base) {
  uint8_t t0;
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
  gg_type_9 * t14;
  gg_slice t15;
  int64_t t16;
  gg_type_8 * t17;
  int64_t t18;
  gg_type_9 * t19;
  gg_slice t20;
  int64_t t21;
  gg_type_8 * t22;
  gg_type_8 t23;
  gg_type_41 v0;
  int64_t v1;
  int64_t v2;
  uint8_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = v3;
  if (t0) {
  {
  t2 = ((int64_t)UINT64_C(16));
  t1 = 0;
label0:;
  if (t1 >= t2) goto label2;
  v4 = t1;
  {
  t4 = v2;
  t3 = 0;
label3:;
  if (t3 >= t4) goto label5;
  v5 = t3;
  {
  t6 = ((int64_t)UINT64_C(8));
  t5 = 0;
label6:;
  if (t5 >= t6) goto label8;
  v6 = t5;
  {
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v4));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(v5));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  /* FG_DIRECT_SLICE_INDEX */
  t13 = v7;
  t14 = ((gg_type_9 *)((((*(v0))).f58).f55).data) + gg_index(t13, ((int64_t)UINT64_C(256)));
  t15 = gg_view(((*t14)).data, 8);
  t16 = v6;
  t17 = ((gg_type_8 *)t15.data) + gg_index(t16, t15.len);
  /* FG_DIRECT_SLICE_INDEX */
  t18 = v7;
  t19 = ((gg_type_9 *)((((*(v0))).f58).f55).data) + gg_index(t18, ((int64_t)UINT64_C(256)));
  t20 = gg_view(((*t19)).data, 8);
  t21 = v6;
  t22 = ((gg_type_8 *)t20.data) + gg_index(t21, t20.len);
  t23 = (*t22);
  (*t17) = gg_function_48(t23, base + 0);
  }
label7:;
  t5++; goto label6;
label8:;
  }
label4:;
  t3++; goto label3;
label5:;
  }
label1:;
  t1++; goto label0;
label2:;
  }
  }
  }
gg_done:;
}
static int64_t gg_function_7(int64_t p0, int64_t base) {
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
static gg_type_49 gg_function_6(int64_t p0, int64_t base) {
  gg_type_49 t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(13))));
  t4 = ((t5) < (t6));
  }
  if (!(t4)) gg_fail("assertion failed");
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(144))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(1))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(176))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(1))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(210))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(3))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(110))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(4))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(84))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(1))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(5))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(136))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(6))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(66))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(7))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(74))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(8))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(82))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(9))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(98))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(10))));
  t39 = ((t37) == (t38));
  if (t39) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(110))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(11))));
  t42 = ((t40) == (t41));
  if (t42) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(50))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  memset(&(t0), 0, sizeof(t0));
  (t0).f60 = ((int64_t)(((int64_t)UINT64_C(56))));
  (t0).f61 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f62 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f63 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f64 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_63(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_type_41 p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
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
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
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
  int64_t t42;
  int64_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
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
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  gg_slice t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  gg_type_13 * t86;
  int32_t t87;
  int32_t t88;
  uint8_t t89;
  uint8_t t90;
  int32_t t91;
  int32_t t92;
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
  int64_t t104;
  int64_t t105;
  gg_type_5 * t106;
  int64_t t107;
  float * t108;
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
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  gg_type_22 * t123;
  gg_slice t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int32_t * t128;
  int64_t t129;
  int16_t * t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  gg_type_9 * t134;
  gg_slice t135;
  int64_t t136;
  gg_type_8 * t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
  uint8_t t141;
  int64_t t142;
  int64_t t143;
  uint8_t t144;
  int64_t t145;
  gg_type_8 * t146;
  gg_type_8 t147;
  int64_t t148;
  gg_type_8 * t149;
  int64_t t150;
  float * t151;
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
  gg_slice t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  gg_type_22 * t166;
  gg_slice t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int32_t * t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  gg_type_9 * t175;
  gg_slice t176;
  int64_t t177;
  gg_type_8 * t178;
  int64_t t179;
  int64_t t180;
  uint8_t t181;
  uint8_t t182;
  int64_t t183;
  int64_t t184;
  uint8_t t185;
  gg_type_8 t186;
  int8_t t187;
  int8_t t188;
  int64_t t189;
  gg_type_8 t190;
  int8_t t191;
  int8_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  gg_type_37 v0;
  gg_slice v1;
  gg_slice v2;
  gg_type_41 v3;
  gg_type_49 v4;
  gg_type_55 v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  gg_type_13 v12;
  gg_type_56 v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(((*(v0))).f1));
  v4 = gg_function_6(t0, base + 0);
  v5 = &(((*(v3))).f58);
  t1 = ((int64_t)(((*(v0))).f6));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  } else {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  }
  t4 = ((int64_t)(((*(v0))).f5));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) >= (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(((*(v0))).f5));
  t9 = ((int64_t)(((int64_t)UINT64_C(256))));
  t7 = ((t8) <= (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(((*(v0))).f6));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((t12) >= (t13));
  }
  t14 = t11;
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(((*(v0))).f6));
  t17 = ((int64_t)(((int64_t)UINT64_C(16))));
  t15 = ((t16) <= (t17));
  }
  t18 = t15;
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(((*(v0))).f5));
  t21 = gg_function_7(t20, base + 0);
  t22 = ((int64_t)(t21));
  t23 = ((int64_t)(((*(v0))).f6));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((int64_t)(((int64_t)UINT64_C(256))));
  t19 = ((t24) <= (t25));
  }
  t26 = t19;
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(((*(v0))).f11));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t28) >= (t29));
  }
  t30 = t27;
  t31 = t30;
  if (t31) {
  t32 = ((int64_t)(((*(v0))).f12));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t32) > (t33));
  }
  t34 = t31;
  t35 = t34;
  if (t35) {
  t36 = ((int64_t)(((*(v0))).f14));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t36) > (t37));
  }
  t38 = t35;
  t39 = t38;
  if (t39) {
  t40 = ((int64_t)(((*(v0))).f4));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t40) >= (t41));
  }
  if (!(t39)) gg_fail("assertion failed");
  t42 = ((int64_t)(((*(v0))).f6));
  t43 = ((int64_t)(((int64_t)UINT64_C(1))));
  t44 = ((t42) > (t43));
  if (t44) {
  {
  t45 = ((int64_t)(((*(v0))).f5));
  t46 = ((int64_t)(((int64_t)UINT64_C(16))));
  if (!(((t45) <= (t46)))) gg_fail("assertion failed");
  }
  }
  t47 = ((int64_t)(((*(v0))).f18));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((t47) == (t48));
  if (t49) {
  {
  t50 = ((int64_t)(((*(v0))).f11));
  t51 = ((int64_t)(((*(v0))).f5));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t51)))));
  t53 = ((int64_t)((v2).len));
  if (!(((t52) <= (t53)))) gg_fail("assertion failed");
  }
  }
  t55 = ((*(v0))).f6;
  t54 = 0;
label0:;
  if (t54 >= t55) goto label2;
  v7 = t54;
  {
  t57 = ((int64_t)(((*(v0))).f5));
  t58 = gg_function_7(t57, base + 0);
  t59 = t58;
  t56 = 0;
label3:;
  if (t56 >= t59) goto label5;
  v8 = t56;
  {
  t60 = ((int64_t)(v8));
  t61 = ((int64_t)(((*(v0))).f6));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = ((int64_t)(v7));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  t64 = ((int64_t)(v7));
  t65 = ((int64_t)(((int64_t)UINT64_C(16))));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))));
  t67 = ((int64_t)(v8));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t68 = ((int64_t)(v8));
  t69 = ((int64_t)(((*(v0))).f5));
  t70 = ((t68) < (t69));
  if (t70) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((int64_t)(((*(v0))).f18));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((t71) != (t72));
  if (t73) {
  {
  t74 = ((int64_t)(((*(v0))).f16));
  t75 = ((int64_t)(v8));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t74) + (uint64_t)(t75)))));
  t77 = ((int64_t)(((*(v0))).f14));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t76) * (uint64_t)(t77)))));
  t79 = ((int64_t)(((*(v0))).f4));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t79)))));
  t81 = ((int64_t)(v7));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t81)))));
  }
  } else {
  {
  t82 = v2;
  t83 = ((int64_t)(((*(v0))).f11));
  t84 = ((int64_t)(v8));
  t85 = ((int64_t)((uint64_t)(t83) + (uint64_t)(t84)));
  t86 = ((gg_type_13 *)t82.data) + gg_index(t85, t82.len);
  v12 = (*t86);
  t87 = ((int32_t)((v12).f47));
  t88 = ((int32_t)(((int64_t)UINT64_C(0))));
  t89 = ((t87) >= (t88));
  t90 = t89;
  if (t90) {
  t91 = ((int32_t)((v12).f48));
  t92 = ((int32_t)(((int64_t)UINT64_C(0))));
  t90 = ((t91) >= (t92));
  }
  if (!(t90)) gg_fail("assertion failed");
  t93 = ((int64_t)(((int64_t)((v12).f47))));
  t94 = ((int64_t)(((*(v0))).f12));
  t95 = ((int64_t)(((int64_t)gg_mod(t93, t94))));
  t96 = ((int64_t)(((int64_t)((v12).f48))));
  t97 = ((int64_t)(((*(v0))).f12));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) * (uint64_t)(t97)))));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t95) + (uint64_t)(t98)))));
  t100 = ((int64_t)(((*(v0))).f14));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t100)))));
  t102 = ((int64_t)(((*(v0))).f4));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) + (uint64_t)(t102)))));
  t104 = ((int64_t)(v7));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t103) + (uint64_t)(t104)))));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t105 = v11;
  t106 = ((gg_type_5 *)(v1).data) + gg_index(t105, (v1).len);
  v13 = t106;
  /* FG_DIRECT_SLICE_INDEX */
  t107 = v10;
  t108 = ((float *)(((*(v5))).f57).data) + gg_index(t107, ((int64_t)UINT64_C(256)));
  (*t108) = ((float)(((*(v13))).f44));
  t110 = ((int64_t)UINT64_C(16));
  t109 = 0;
label6:;
  if (t109 >= t110) goto label8;
  v14 = t109;
  {
  t111 = ((int64_t)(v7));
  t112 = ((int64_t)(((int64_t)UINT64_C(16))));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t111) * (uint64_t)(t112)))));
  t114 = ((int64_t)(v14));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t113) + (uint64_t)(t114)))));
  t116 = ((int64_t)(v6));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) * (uint64_t)(t116)))));
  t118 = ((int64_t)(v8));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t117) + (uint64_t)(t118)))));
  t119 = gg_view((((*(v5))).f56).data, 512);
  t120 = ((int64_t)(v15));
  t121 = ((int64_t)(((int64_t)UINT64_C(8))));
  t122 = ((int64_t)gg_div(t120, t121));
  t123 = ((gg_type_22 *)t119.data) + gg_index(t122, t119.len);
  t124 = gg_view(((*t123)).data, 8);
  t125 = ((int64_t)(v15));
  t126 = ((int64_t)(((int64_t)UINT64_C(8))));
  t127 = ((int64_t)gg_mod(t125, t126));
  t128 = ((int32_t *)t124.data) + gg_index(t127, t124.len);
  /* FG_DIRECT_SLICE_INDEX */
  t129 = v14;
  t130 = ((int16_t *)(((*(v13))).f46).data) + gg_index(t129, ((int64_t)UINT64_C(16)));
  (*t128) = ((int32_t)(((int32_t)((*t130)))));
  }
label7:;
  t109++; goto label6;
label8:;
  t132 = ((int64_t)UINT64_C(8));
  t131 = 0;
label9:;
  if (t131 >= t132) goto label11;
  v16 = t131;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t133 = v9;
  t134 = ((gg_type_9 *)(((*(v5))).f55).data) + gg_index(t133, ((int64_t)UINT64_C(256)));
  t135 = gg_view(((*t134)).data, 8);
  t136 = v16;
  t137 = ((gg_type_8 *)t135.data) + gg_index(t136, t135.len);
  t138 = ((int64_t)((v4).f64));
  t139 = ((int64_t)(((int64_t)UINT64_C(0))));
  t140 = ((t138) != (t139));
  t141 = t140;
  if (t141) {
  t142 = ((int64_t)(((*(v0))).f19));
  t143 = ((int64_t)(((int64_t)UINT64_C(0))));
  t141 = ((t142) == (t143));
  }
  t144 = t141;
  if (t144) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t145 = v16;
  t146 = ((gg_type_8 *)(((*(v13))).f45).data) + gg_index(t145, ((int64_t)UINT64_C(8)));
  t147 = (*t146);
  (*t137) = gg_function_48(t147, base + 0);
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t148 = v16;
  t149 = ((gg_type_8 *)(((*(v13))).f45).data) + gg_index(t148, ((int64_t)UINT64_C(8)));
  (*t137) = (*t149);
  }
  }
  }
label10:;
  t131++; goto label9;
label11:;
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t150 = v10;
  t151 = ((float *)(((*(v5))).f57).data) + gg_index(t150, ((int64_t)UINT64_C(256)));
  (*t151) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t153 = ((int64_t)UINT64_C(16));
  t152 = 0;
label12:;
  if (t152 >= t153) goto label14;
  v17 = t152;
  {
  t154 = ((int64_t)(v7));
  t155 = ((int64_t)(((int64_t)UINT64_C(16))));
  t156 = ((int64_t)(((int64_t)((uint64_t)(t154) * (uint64_t)(t155)))));
  t157 = ((int64_t)(v17));
  t158 = ((int64_t)(((int64_t)((uint64_t)(t156) + (uint64_t)(t157)))));
  t159 = ((int64_t)(v6));
  t160 = ((int64_t)(((int64_t)((uint64_t)(t158) * (uint64_t)(t159)))));
  t161 = ((int64_t)(v8));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t160) + (uint64_t)(t161)))));
  t162 = gg_view((((*(v5))).f56).data, 512);
  t163 = ((int64_t)(v18));
  t164 = ((int64_t)(((int64_t)UINT64_C(8))));
  t165 = ((int64_t)gg_div(t163, t164));
  t166 = ((gg_type_22 *)t162.data) + gg_index(t165, t162.len);
  t167 = gg_view(((*t166)).data, 8);
  t168 = ((int64_t)(v18));
  t169 = ((int64_t)(((int64_t)UINT64_C(8))));
  t170 = ((int64_t)gg_mod(t168, t169));
  t171 = ((int32_t *)t167.data) + gg_index(t170, t167.len);
  (*t171) = ((int32_t)(((int64_t)UINT64_C(0))));
  }
label13:;
  t152++; goto label12;
label14:;
  t173 = ((int64_t)UINT64_C(8));
  t172 = 0;
label15:;
  if (t172 >= t173) goto label17;
  v19 = t172;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t174 = v9;
  t175 = ((gg_type_9 *)(((*(v5))).f55).data) + gg_index(t174, ((int64_t)UINT64_C(256)));
  t176 = gg_view(((*t175)).data, 8);
  t177 = v19;
  t178 = ((gg_type_8 *)t176.data) + gg_index(t177, t176.len);
  t179 = ((int64_t)((v4).f64));
  t180 = ((int64_t)(((int64_t)UINT64_C(0))));
  t181 = ((t179) != (t180));
  t182 = t181;
  if (t182) {
  t183 = ((int64_t)(((*(v0))).f19));
  t184 = ((int64_t)(((int64_t)UINT64_C(0))));
  t182 = ((t183) == (t184));
  }
  t185 = t182;
  if (t185) {
  {
  memset(&t187, 0, sizeof(t187));
  t188 = ((int8_t)(t187));
  for (t189=0; t189<32; t189++) (t186).data[t189] = t188;
  t190 = t186;
  (*t178) = gg_function_48(t190, base + 0);
  }
  } else {
  {
  memset(&t191, 0, sizeof(t191));
  t192 = ((int8_t)(t191));
  for (t193=0; t193<32; t193++) ((*t178)).data[t193] = t192;
  }
  }
  }
label16:;
  t172++; goto label15;
label17:;
  }
  }
  }
label4:;
  t56++; goto label3;
label5:;
  }
label1:;
  t54++; goto label0;
label2:;
  t194 = ((int64_t)(((int64_t)UINT64_C(2))));
  t195 = ((int64_t)(((*(v0))).f1));
  t196 = ((int64_t)(((*(v0))).f5));
  t197 = ((int64_t)(((*(v0))).f6));
  t198 = ((int64_t)(((int64_t)((uint64_t)(t196) * (uint64_t)(t197)))));
  fg141_witness(t194, t195, t198);
  }
gg_done:;
}
static void gg_function_10(gg_type_50 p0, int64_t p1, int32_t p2, int64_t base) {
  gg_slice t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  gg_type_22 * t4;
  gg_slice t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t * t9;
  gg_type_50 v0;
  int64_t v1;
  int32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = gg_view((((*(v0))).f53).data, 512);
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)gg_div(t1, t2));
  t4 = ((gg_type_22 *)t0.data) + gg_index(t3, t0.len);
  t5 = gg_view(((*t4)).data, 8);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)gg_mod(t6, t7));
  t9 = ((int32_t *)t5.data) + gg_index(t8, t5.len);
  (*t9) = ((int32_t)(v2));
  }
gg_done:;
}
static void gg_function_13(gg_type_50 p0, int64_t p1, int32_t p2, int64_t base) {
  gg_slice t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  gg_type_22 * t4;
  gg_slice t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t * t9;
  gg_type_50 v0;
  int64_t v1;
  int32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = gg_view((((*(v0))).f52).data, 512);
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)gg_div(t1, t2));
  t4 = ((gg_type_22 *)t0.data) + gg_index(t3, t0.len);
  t5 = gg_view(((*t4)).data, 8);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)gg_mod(t6, t7));
  t9 = ((int32_t *)t5.data) + gg_index(t8, t5.len);
  (*t9) = ((int32_t)(v2));
  }
gg_done:;
}
static void gg_function_20(gg_type_50 p0, int64_t p1, int64_t p2, uint8_t p3, int64_t base) {
  int64_t t0;
  gg_type_19 * t1;
  gg_slice t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_type_18 * t6;
  gg_slice t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t * t11;
  gg_type_50 v0;
  int64_t v1;
  int64_t v2;
  uint8_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t0 = v1;
  t1 = ((gg_type_19 *)(((*(v0))).f49).data) + gg_index(t0, ((int64_t)UINT64_C(256)));
  t2 = gg_view(((*t1)).data, 8);
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(32))));
  t5 = ((int64_t)gg_div(t3, t4));
  t6 = ((gg_type_18 *)t2.data) + gg_index(t5, t2.len);
  t7 = gg_view(((*t6)).data, 32);
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(32))));
  t10 = ((int64_t)gg_mod(t8, t9));
  t11 = ((uint8_t *)t7.data) + gg_index(t10, t7.len);
  (*t11) = ((uint8_t)(v3));
  }
gg_done:;
}
static void gg_function_21(gg_type_50 p0, int64_t p1, int64_t p2, gg_type_32 p3, int16_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  gg_type_50 t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int16_t t7;
  int16_t t8;
  int16_t t9;
  int16_t t10;
  int64_t t11;
  int8_t * t12;
  int16_t t13;
  int16_t t14;
  uint8_t t15;
  gg_type_50 v0;
  int64_t v1;
  int64_t v2;
  gg_type_32 v3;
  int16_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)UINT64_C(8));
  t0 = 0;
label0:;
  if (t0 >= t1) goto label2;
  v5 = t0;
  {
  t2 = v0;
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = ((int16_t)(((int64_t)UINT64_C(128))));
  t8 = ((int16_t)(v4));
  t9 = ((int16_t)(((int16_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = ((int16_t)(((int64_t)UINT64_C(8))));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v5;
  t12 = ((int8_t *)(v3).data) + gg_index(t11, ((int64_t)UINT64_C(8)));
  t13 = ((int16_t)(((int16_t)((*t12)))));
  t14 = ((int16_t)(((int16_t)((uint64_t)(t10) * (uint64_t)(t13)))));
  t15 = ((uint8_t)(((uint8_t)(((int16_t)((uint64_t)(t9) + (uint64_t)(t14)))))));
  gg_function_20(t2, t3, t6, t15, base + 0);
  }
label1:;
  t0++; goto label0;
label2:;
  }
gg_done:;
}
static uint16_t gg_function_11(gg_slice p0, int64_t p1, int64_t base) {
  uint16_t t0;
  int64_t t1;
  uint8_t * t2;
  uint16_t t3;
  gg_slice t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t * t8;
  uint16_t t9;
  int64_t t10;
  uint16_t t11;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1 = v1;
  t2 = ((uint8_t *)(v0).data) + gg_index(t1, (v0).len);
  t3 = ((uint16_t)(((uint16_t)((*t2)))));
  t4 = v0;
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)((uint64_t)(t5) + (uint64_t)(t6)));
  t8 = ((uint8_t *)t4.data) + gg_index(t7, t4.len);
  t9 = ((uint16_t)(((uint16_t)((*t8)))));
  t10 = ((int64_t)(((int64_t)UINT64_C(8))));
  t11 = ((uint16_t)(((uint16_t)((uint64_t)(t9) << ((uint64_t)(t10) & 15)))));
  t0 = ((uint16_t)(((t3) | (t11))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_19(gg_type_50 p0, int64_t p1, int64_t p2, gg_type_28 p3, uint8_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  uint8_t t4;
  uint8_t t5;
  uint8_t t6;
  uint8_t t7;
  gg_type_50 t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t * t14;
  uint8_t t15;
  uint8_t t16;
  uint8_t t17;
  uint8_t t18;
  uint8_t t19;
  uint8_t t20;
  uint8_t t21;
  gg_type_50 v0;
  int64_t v1;
  int64_t v2;
  gg_type_28 v3;
  uint8_t v4;
  int64_t v5;
  uint8_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)UINT64_C(8));
  t0 = 0;
label0:;
  if (t0 >= t1) goto label2;
  v5 = t0;
  {
  t2 = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(0))))));
  t3 = ((uint8_t)(v4));
  t4 = ((uint8_t)(((uint8_t)(v5))));
  t5 = ((uint8_t)(((t3) >> ((uint64_t)(t4) & 7))));
  t6 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t7 = ((uint8_t)(((t5) & (t6))));
  v6 = ((uint8_t)(((uint8_t)(((uint8_t)((uint64_t)(t2) - (uint64_t)(t7)))))));
  t8 = v0;
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  /* FG_DIRECT_SLICE_INDEX */
  t13 = v5;
  t14 = ((uint8_t *)(v3).data) + gg_index(t13, ((int64_t)UINT64_C(8)));
  t15 = ((uint8_t)((*t14)));
  t16 = ((uint8_t)(v6));
  t17 = ((uint8_t)(((t15) ^ (t16))));
  t18 = ((uint8_t)(v6));
  t19 = ((uint8_t)(((uint8_t)((uint64_t)(t17) - (uint64_t)(t18)))));
  t20 = ((uint8_t)(((int64_t)UINT64_C(128))));
  t21 = ((uint8_t)(((uint8_t)(((uint8_t)((uint64_t)(t19) + (uint64_t)(t20)))))));
  gg_function_20(t8, t9, t12, t21, base + 0);
  }
label1:;
  t0++; goto label0;
label2:;
  }
gg_done:;
}
static uint32_t gg_function_18(gg_slice p0, int64_t p1, int64_t base) {
  uint32_t t0;
  gg_slice t1;
  int64_t t2;
  uint16_t t3;
  uint32_t t4;
  gg_slice t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint16_t t9;
  uint32_t t10;
  int64_t t11;
  uint32_t t12;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  t3 = gg_function_11(t1, t2, base + 0);
  t4 = ((uint32_t)(((uint32_t)(t3))));
  t5 = v0;
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t9 = gg_function_11(t5, t8, base + 0);
  t10 = ((uint32_t)(((uint32_t)(t9))));
  t11 = ((int64_t)(((int64_t)UINT64_C(16))));
  t12 = ((uint32_t)(((uint32_t)((uint64_t)(t10) << ((uint64_t)(t11) & 31)))));
  t0 = ((uint32_t)(((t4) | (t12))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:shuffle8 */
static gg_type_18 gg_function_17(gg_type_18 p326, gg_type_18 p327, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_18 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)p326.data), _mm256_loadu_si256((const __m256i*)p327.data)));
    return packed_result;
  }
#endif
  gg_type_18 t0;
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
  gg_type_18 v326;
  gg_type_18 v327;
  gg_type_18 v328;
  int64_t v330;
  int64_t v331;
  v326 = p326;
  v327 = p327;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v328).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v330 = t4;
  {
  t6 = ((int64_t)(v330));
  t7 = ((int64_t)(((int64_t)UINT64_C(16))));
  t8 = ((int64_t)(((int64_t)gg_div(t6, t7))));
  t9 = ((int64_t)(((int64_t)UINT64_C(16))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t11 = gg_view((v327).data, 32);
  t12 = v330;
  t13 = ((uint8_t *)t11.data) + gg_index(t12, t11.len);
  t14 = ((uint8_t)((*t13)));
  t15 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t16 = ((int64_t)(((int64_t)(((t14) & (t15))))));
  v331 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t16)))));
  t17 = gg_view((v328).data, 32);
  t18 = v330;
  t19 = ((uint8_t *)t17.data) + gg_index(t18, t17.len);
  t20 = gg_view((v327).data, 32);
  t21 = v330;
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
  t28 = gg_view((v326).data, 32);
  t29 = v331;
  t30 = ((uint8_t *)t28.data) + gg_index(t29, t28.len);
  (*t19) = ((uint8_t)((*t30)));
  }
  }
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v328;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:shr8 */
static gg_type_18 gg_function_16(gg_type_18 p245, uint8_t p246, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_18 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)p245.data), _mm_cvtsi32_si128((p246 & 7))), _mm256_set1_epi8((unsigned char)(255u >> (p246 & 7)))));
    return packed_result;
  }
#endif
  gg_type_18 t0;
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
  gg_type_18 v245;
  uint8_t v246;
  gg_type_18 v247;
  int64_t v249;
  v245 = p245;
  v246 = p246;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v247).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v249 = t4;
  {
  t6 = gg_view((v247).data, 32);
  t7 = v249;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v245).data, 32);
  t10 = v249;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v246));
  (*t8) = ((uint8_t)(((t12) >> ((uint64_t)(t13) & 7))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v247;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:and8 */
static gg_type_18 gg_function_15(gg_type_18 p237, uint8_t p238, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_18 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_loadu_si256((const __m256i*)p237.data), _mm256_set1_epi8(p238)));
    return packed_result;
  }
#endif
  gg_type_18 t0;
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
  gg_type_18 v237;
  uint8_t v238;
  gg_type_18 v239;
  int64_t v241;
  v237 = p237;
  v238 = p238;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v239).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v241 = t4;
  {
  t6 = gg_view((v239).data, 32);
  t7 = v241;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v237).data, 32);
  t10 = v241;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v238));
  (*t8) = ((uint8_t)(((t12) & (t13))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v239;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_18 gg_function_14(gg_slice p0, int64_t p1, int64_t base) {
  gg_type_18 t0;
  gg_slice t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t * t12;
  int64_t t13;
  uint8_t * t14;
  gg_slice v0;
  int64_t v1;
  gg_slice v2;
  gg_type_18 v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t1 = v0;
  t2 = v1;
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(32))));
  t5 = ((int64_t)((uint64_t)(t3) + (uint64_t)(t4)));
  v2 = gg_sub(t1, t2, t5, sizeof(uint8_t));
  memset(&t6, 0, sizeof(t6));
  t7 = ((uint8_t)(t6));
  for (t8=0; t8<32; t8++) (v3).data[t8] = t7;
  t10 = ((int64_t)UINT64_C(32));
  t9 = 0;
label0:;
  if (t9 >= t10) goto label2;
  v4 = t9;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v4;
  t12 = ((uint8_t *)(v3).data) + gg_index(t11, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t13 = v4;
  t14 = ((uint8_t *)(v2).data) + gg_index(t13, (v2).len);
  (*t12) = ((uint8_t)((*t14)));
  }
label1:;
  t9++; goto label0;
label2:;
  t0 = v3;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_9(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, gg_type_50 p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  float * t6;
  int64_t t7;
  int64_t t8;
  gg_type_50 t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int32_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  int64_t t21;
  float * t22;
  gg_slice t23;
  int64_t t24;
  uint16_t t25;
  float t26;
  int64_t t27;
  float * t28;
  gg_slice t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint16_t t33;
  float t34;
  int64_t t35;
  int64_t t36;
  gg_slice t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t * t43;
  gg_slice t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t * t50;
  gg_slice t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t * t57;
  gg_type_50 t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  uint8_t t63;
  int32_t t64;
  gg_type_50 t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  uint8_t t70;
  int32_t t71;
  gg_type_50 t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint8_t t78;
  uint8_t t79;
  uint8_t t80;
  uint8_t t81;
  int64_t t82;
  uint8_t t83;
  int64_t t84;
  uint8_t t85;
  int32_t t86;
  gg_type_50 t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  int64_t t94;
  uint8_t t95;
  uint8_t t96;
  int64_t t97;
  uint8_t t98;
  int64_t t99;
  uint8_t t100;
  int32_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  gg_type_18 t116;
  uint8_t t117;
  gg_type_18 t118;
  uint8_t t119;
  int64_t t120;
  int64_t t121;
  uint8_t t122;
  gg_slice t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  uint8_t * t130;
  int64_t t131;
  uint8_t * t132;
  uint8_t t133;
  int64_t t134;
  uint8_t * t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  uint8_t t139;
  uint8_t t140;
  uint8_t t141;
  uint8_t t142;
  int64_t t143;
  uint8_t t144;
  int64_t t145;
  uint8_t * t146;
  int64_t t147;
  uint8_t * t148;
  uint8_t t149;
  int64_t t150;
  uint8_t * t151;
  uint8_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  uint8_t t157;
  uint8_t t158;
  uint8_t t159;
  uint8_t t160;
  int64_t t161;
  uint8_t t162;
  int64_t t163;
  gg_type_19 * t164;
  gg_slice t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  gg_type_18 * t169;
  int64_t t170;
  gg_type_19 * t171;
  gg_slice t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  gg_type_18 * t178;
  int64_t t179;
  int64_t t180;
  uint8_t t181;
  int64_t t182;
  float * t183;
  gg_slice t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  uint16_t t188;
  float t189;
  int64_t t190;
  int64_t t191;
  gg_type_50 t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  gg_slice t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  uint8_t * t202;
  int32_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  gg_slice t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  uint8_t * t218;
  gg_slice t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  uint8_t * t227;
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
  uint8_t * t238;
  int64_t t239;
  gg_type_19 * t240;
  gg_slice t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  gg_type_18 * t245;
  gg_slice t246;
  int64_t t247;
  uint8_t * t248;
  uint8_t t249;
  uint8_t t250;
  uint8_t t251;
  uint8_t t252;
  uint8_t t253;
  uint8_t t254;
  int64_t t255;
  uint8_t t256;
  int64_t t257;
  gg_type_19 * t258;
  gg_slice t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  gg_type_18 * t265;
  gg_slice t266;
  int64_t t267;
  uint8_t * t268;
  uint8_t t269;
  uint8_t t270;
  uint8_t t271;
  uint8_t t272;
  uint8_t t273;
  uint8_t t274;
  int64_t t275;
  uint8_t t276;
  int64_t t277;
  gg_type_19 * t278;
  gg_slice t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  gg_type_18 * t285;
  gg_slice t286;
  int64_t t287;
  uint8_t * t288;
  uint8_t t289;
  int64_t t290;
  uint8_t t291;
  uint8_t t292;
  uint8_t t293;
  uint8_t t294;
  int64_t t295;
  gg_type_19 * t296;
  gg_slice t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  gg_type_18 * t303;
  gg_slice t304;
  int64_t t305;
  uint8_t * t306;
  uint8_t t307;
  int64_t t308;
  uint8_t t309;
  uint8_t t310;
  uint8_t t311;
  uint8_t t312;
  int64_t t313;
  uint8_t t314;
  int64_t t315;
  int64_t t316;
  uint8_t t317;
  int64_t t318;
  float * t319;
  gg_slice t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  uint16_t t324;
  float t325;
  int64_t t326;
  int64_t t327;
  gg_slice t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  uint8_t * t334;
  gg_slice t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  int64_t t340;
  uint8_t * t341;
  gg_slice t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  uint8_t * t348;
  gg_type_50 t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  uint8_t t353;
  uint8_t t354;
  uint8_t t355;
  uint8_t t356;
  uint8_t t357;
  uint8_t t358;
  int64_t t359;
  uint8_t t360;
  int32_t t361;
  int32_t t362;
  int32_t t363;
  gg_type_50 t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  uint8_t t370;
  uint8_t t371;
  uint8_t t372;
  uint8_t t373;
  int64_t t374;
  uint8_t t375;
  uint8_t t376;
  uint8_t t377;
  int64_t t378;
  uint8_t t379;
  int32_t t380;
  int32_t t381;
  int32_t t382;
  gg_type_50 t383;
  int64_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  int64_t t388;
  uint8_t t389;
  int64_t t390;
  uint8_t t391;
  uint8_t t392;
  int64_t t393;
  uint8_t t394;
  uint8_t t395;
  uint8_t t396;
  int64_t t397;
  uint8_t t398;
  int32_t t399;
  int32_t t400;
  int32_t t401;
  gg_type_50 t402;
  int64_t t403;
  int64_t t404;
  int64_t t405;
  int64_t t406;
  int64_t t407;
  uint8_t t408;
  int64_t t409;
  uint8_t t410;
  uint8_t t411;
  int64_t t412;
  uint8_t t413;
  uint8_t t414;
  uint8_t t415;
  int64_t t416;
  uint8_t t417;
  int32_t t418;
  int32_t t419;
  int32_t t420;
  int64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  gg_slice t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  int64_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  gg_slice t433;
  int64_t t434;
  uint8_t t435;
  uint8_t t436;
  int64_t t437;
  int64_t t438;
  int64_t t439;
  int64_t t440;
  uint8_t * t441;
  int64_t t442;
  uint8_t * t443;
  uint8_t t444;
  int64_t t445;
  int64_t t446;
  uint8_t t447;
  uint8_t t448;
  uint8_t t449;
  uint8_t t450;
  int64_t t451;
  uint8_t * t452;
  uint8_t t453;
  int64_t t454;
  int64_t t455;
  int64_t t456;
  int64_t t457;
  uint8_t t458;
  uint8_t t459;
  uint8_t t460;
  uint8_t t461;
  int64_t t462;
  uint8_t t463;
  int64_t t464;
  gg_type_19 * t465;
  gg_slice t466;
  int64_t t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
  gg_type_18 * t472;
  int64_t t473;
  int64_t t474;
  uint8_t t475;
  int64_t t476;
  float * t477;
  gg_slice t478;
  int64_t t479;
  int64_t t480;
  int64_t t481;
  uint16_t t482;
  float t483;
  int64_t t484;
  float * t485;
  gg_slice t486;
  int64_t t487;
  int64_t t488;
  int64_t t489;
  uint16_t t490;
  float t491;
  int64_t t492;
  int64_t t493;
  gg_type_50 t494;
  int64_t t495;
  int64_t t496;
  int64_t t497;
  gg_slice t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  uint8_t * t502;
  uint8_t t503;
  uint8_t t504;
  int32_t t505;
  gg_type_50 t506;
  int64_t t507;
  int64_t t508;
  int64_t t509;
  gg_slice t510;
  int64_t t511;
  int64_t t512;
  int64_t t513;
  uint8_t * t514;
  uint8_t t515;
  int64_t t516;
  int32_t t517;
  int64_t t518;
  int64_t t519;
  gg_slice t520;
  int64_t t521;
  int64_t t522;
  int64_t t523;
  int64_t t524;
  int64_t t525;
  int64_t t526;
  int64_t t527;
  int64_t t528;
  int64_t t529;
  int64_t t530;
  gg_type_19 * t531;
  gg_slice t532;
  int64_t t533;
  int64_t t534;
  int64_t t535;
  int64_t t536;
  int64_t t537;
  gg_type_18 * t538;
  gg_type_18 t539;
  int64_t t540;
  int64_t t541;
  uint8_t t542;
  gg_type_18 t543;
  uint8_t t544;
  int64_t t545;
  int64_t t546;
  uint8_t t547;
  int64_t t548;
  float * t549;
  gg_slice t550;
  int64_t t551;
  uint16_t t552;
  float t553;
  gg_slice t554;
  int64_t t555;
  int64_t t556;
  int64_t t557;
  int64_t t558;
  int64_t t559;
  gg_slice t560;
  int64_t t561;
  int64_t t562;
  int64_t t563;
  int64_t t564;
  int64_t t565;
  int64_t t566;
  int64_t t567;
  uint8_t * t568;
  uint8_t t569;
  int64_t t570;
  int64_t t571;
  int64_t t572;
  int64_t t573;
  uint8_t t574;
  uint8_t t575;
  uint8_t t576;
  gg_type_50 t577;
  int64_t t578;
  int64_t t579;
  int64_t t580;
  uint16_t t581;
  uint16_t t582;
  int64_t t583;
  int64_t t584;
  uint16_t t585;
  uint16_t t586;
  uint16_t t587;
  uint16_t t588;
  int64_t t589;
  uint16_t t590;
  int32_t t591;
  int32_t t592;
  int32_t t593;
  uint8_t t594;
  uint8_t t595;
  int64_t t596;
  int64_t t597;
  int64_t t598;
  int64_t t599;
  int8_t * t600;
  int16_t t601;
  int16_t t602;
  int64_t t603;
  uint8_t * t604;
  gg_slice t605;
  int64_t t606;
  int64_t t607;
  int64_t t608;
  uint8_t * t609;
  int64_t t610;
  int64_t t611;
  gg_slice t612;
  int64_t t613;
  int64_t t614;
  int64_t t615;
  int64_t t616;
  int64_t t617;
  int64_t t618;
  int64_t t619;
  gg_type_18 t620;
  gg_type_18 t621;
  uint8_t t622;
  gg_type_18 t623;
  gg_type_18 t624;
  gg_type_18 t625;
  uint8_t t626;
  gg_type_18 t627;
  int64_t t628;
  int64_t t629;
  int64_t t630;
  gg_type_19 * t631;
  gg_slice t632;
  int64_t t633;
  int64_t t634;
  int64_t t635;
  gg_type_18 * t636;
  gg_slice t637;
  int64_t t638;
  uint8_t * t639;
  int64_t t640;
  uint8_t * t641;
  int64_t t642;
  gg_type_19 * t643;
  gg_slice t644;
  int64_t t645;
  int64_t t646;
  int64_t t647;
  gg_type_18 * t648;
  gg_slice t649;
  int64_t t650;
  int64_t t651;
  int64_t t652;
  uint8_t * t653;
  int64_t t654;
  uint8_t * t655;
  int64_t t656;
  gg_type_19 * t657;
  gg_slice t658;
  int64_t t659;
  int64_t t660;
  int64_t t661;
  int64_t t662;
  int64_t t663;
  gg_type_18 * t664;
  gg_slice t665;
  int64_t t666;
  uint8_t * t667;
  gg_slice t668;
  int64_t t669;
  int64_t t670;
  int64_t t671;
  uint8_t * t672;
  int64_t t673;
  gg_type_19 * t674;
  gg_slice t675;
  int64_t t676;
  int64_t t677;
  int64_t t678;
  int64_t t679;
  int64_t t680;
  gg_type_18 * t681;
  gg_slice t682;
  int64_t t683;
  int64_t t684;
  int64_t t685;
  uint8_t * t686;
  gg_slice t687;
  int64_t t688;
  int64_t t689;
  int64_t t690;
  uint8_t * t691;
  int64_t t692;
  int64_t t693;
  uint8_t t694;
  int64_t t695;
  float * t696;
  gg_slice t697;
  int64_t t698;
  uint16_t t699;
  float t700;
  float t701;
  float t702;
  int64_t t703;
  int64_t t704;
  gg_slice t705;
  int64_t t706;
  int64_t t707;
  int64_t t708;
  int64_t t709;
  int64_t t710;
  int64_t t711;
  int64_t t712;
  int64_t t713;
  int64_t t714;
  gg_type_50 t715;
  int64_t t716;
  int64_t t717;
  int64_t t718;
  uint32_t t719;
  uint32_t t720;
  int64_t t721;
  uint32_t t722;
  uint32_t t723;
  uint32_t t724;
  int32_t t725;
  int64_t t726;
  int64_t t727;
  gg_type_50 t728;
  int64_t t729;
  int64_t t730;
  int64_t t731;
  int64_t t732;
  int64_t t733;
  int64_t t734;
  int64_t t735;
  int64_t t736;
  gg_slice t737;
  gg_slice t738;
  int64_t t739;
  int64_t t740;
  int64_t t741;
  int64_t t742;
  int64_t t743;
  int64_t t744;
  int64_t t745;
  int64_t t746;
  int64_t t747;
  uint8_t * t748;
  int64_t t749;
  gg_type_28 * t750;
  gg_type_28 t751;
  gg_slice t752;
  uint32_t t753;
  int64_t t754;
  int64_t t755;
  uint32_t t756;
  uint32_t t757;
  uint32_t t758;
  int64_t t759;
  uint8_t * t760;
  uint8_t t761;
  int64_t t762;
  int64_t t763;
  uint8_t t764;
  uint8_t t765;
  int64_t t766;
  int64_t t767;
  uint8_t t768;
  int64_t t769;
  float * t770;
  gg_slice t771;
  int64_t t772;
  uint16_t t773;
  float t774;
  float t775;
  float t776;
  int64_t t777;
  int64_t t778;
  int64_t t779;
  int64_t t780;
  uint8_t t781;
  int64_t t782;
  int64_t t783;
  int64_t t784;
  gg_type_50 t785;
  int64_t t786;
  int64_t t787;
  int64_t t788;
  int64_t t789;
  int64_t t790;
  uint8_t t791;
  gg_slice t792;
  int64_t t793;
  int64_t t794;
  int64_t t795;
  uint8_t * t796;
  uint8_t t797;
  uint8_t t798;
  uint8_t t799;
  uint8_t t800;
  uint8_t t801;
  int32_t t802;
  gg_type_50 t803;
  int64_t t804;
  int64_t t805;
  int64_t t806;
  int64_t t807;
  int64_t t808;
  int64_t t809;
  int64_t t810;
  uint8_t t811;
  gg_slice t812;
  int64_t t813;
  int64_t t814;
  int64_t t815;
  uint8_t * t816;
  uint8_t t817;
  int64_t t818;
  uint8_t t819;
  uint8_t t820;
  uint8_t t821;
  int32_t t822;
  int64_t t823;
  int64_t t824;
  int64_t t825;
  int64_t t826;
  uint8_t t827;
  gg_slice t828;
  int64_t t829;
  int64_t t830;
  int64_t t831;
  int64_t t832;
  int64_t t833;
  int64_t t834;
  int64_t t835;
  int64_t t836;
  int64_t t837;
  int64_t t838;
  int64_t t839;
  gg_type_50 t840;
  int64_t t841;
  int64_t t842;
  int64_t t843;
  int64_t t844;
  int64_t t845;
  int64_t t846;
  int64_t t847;
  int64_t t848;
  gg_slice t849;
  uint16_t t850;
  uint16_t t851;
  int64_t t852;
  gg_type_28 * t853;
  gg_type_28 t854;
  gg_slice t855;
  uint16_t t856;
  int64_t t857;
  int64_t t858;
  uint8_t * t859;
  uint8_t t860;
  gg_slice t861;
  int64_t t862;
  int64_t t863;
  int64_t t864;
  int64_t t865;
  int64_t t866;
  int64_t t867;
  int64_t t868;
  int64_t t869;
  int64_t t870;
  uint8_t * t871;
  uint16_t t872;
  gg_slice t873;
  int64_t t874;
  int64_t t875;
  int64_t t876;
  int64_t t877;
  int64_t t878;
  uint8_t * t879;
  uint16_t t880;
  int64_t t881;
  int64_t t882;
  int64_t t883;
  int64_t t884;
  uint16_t t885;
  uint16_t t886;
  uint16_t t887;
  uint16_t t888;
  gg_type_50 t889;
  int64_t t890;
  int64_t t891;
  int64_t t892;
  int64_t t893;
  int64_t t894;
  int64_t t895;
  int64_t t896;
  int64_t t897;
  gg_slice t898;
  int64_t t899;
  gg_type_28 * t900;
  gg_type_28 t901;
  gg_slice t902;
  int64_t t903;
  int64_t t904;
  int64_t t905;
  int64_t t906;
  int64_t t907;
  int64_t t908;
  int64_t t909;
  int64_t t910;
  int64_t t911;
  uint8_t * t912;
  uint8_t t913;
  int64_t t914;
  int64_t t915;
  uint8_t t916;
  int64_t t917;
  float * t918;
  gg_slice t919;
  int64_t t920;
  uint16_t t921;
  float t922;
  float t923;
  float t924;
  int64_t t925;
  int64_t t926;
  gg_slice t927;
  int64_t t928;
  int64_t t929;
  int64_t t930;
  int64_t t931;
  int64_t t932;
  int64_t t933;
  int64_t t934;
  gg_type_50 t935;
  int64_t t936;
  int64_t t937;
  int64_t t938;
  uint32_t t939;
  uint32_t t940;
  int64_t t941;
  uint32_t t942;
  uint32_t t943;
  uint32_t t944;
  int32_t t945;
  int64_t t946;
  int64_t t947;
  gg_slice t948;
  gg_slice t949;
  int64_t t950;
  int64_t t951;
  int64_t t952;
  int64_t t953;
  int64_t t954;
  int64_t t955;
  int64_t t956;
  int64_t t957;
  int64_t t958;
  int64_t t959;
  int64_t t960;
  uint8_t * t961;
  int64_t t962;
  gg_type_30 * t963;
  gg_slice t964;
  gg_slice t965;
  int64_t t966;
  int64_t t967;
  int64_t t968;
  int64_t t969;
  int64_t t970;
  int64_t t971;
  int64_t t972;
  int64_t t973;
  int64_t t974;
  int64_t t975;
  int64_t t976;
  int64_t t977;
  int64_t t978;
  uint8_t * t979;
  int64_t t980;
  gg_type_30 * t981;
  uint8_t t982;
  uint8_t t983;
  int64_t t984;
  int64_t t985;
  int64_t t986;
  int64_t t987;
  uint8_t * t988;
  int64_t t989;
  uint8_t * t990;
  gg_slice t991;
  int64_t t992;
  int64_t t993;
  int64_t t994;
  uint8_t * t995;
  int64_t t996;
  uint8_t * t997;
  gg_type_50 t998;
  int64_t t999;
  int64_t t1000;
  int64_t t1001;
  int64_t t1002;
  int64_t t1003;
  int64_t t1004;
  int64_t t1005;
  int64_t t1006;
  gg_type_28 t1007;
  gg_slice t1008;
  uint32_t t1009;
  int64_t t1010;
  int64_t t1011;
  uint32_t t1012;
  uint32_t t1013;
  uint32_t t1014;
  int64_t t1015;
  uint8_t * t1016;
  uint8_t t1017;
  int64_t t1018;
  int64_t t1019;
  uint8_t t1020;
  int64_t t1021;
  float * t1022;
  gg_slice t1023;
  int64_t t1024;
  uint16_t t1025;
  float t1026;
  int64_t t1027;
  int64_t t1028;
  gg_slice t1029;
  int64_t t1030;
  int64_t t1031;
  int64_t t1032;
  int64_t t1033;
  int64_t t1034;
  int64_t t1035;
  int64_t t1036;
  uint8_t * t1037;
  gg_type_50 t1038;
  int64_t t1039;
  int64_t t1040;
  int64_t t1041;
  uint8_t t1042;
  uint8_t t1043;
  uint8_t t1044;
  int64_t t1045;
  int64_t t1046;
  int64_t t1047;
  int64_t t1048;
  uint8_t t1049;
  uint8_t t1050;
  uint8_t t1051;
  uint8_t t1052;
  uint8_t t1053;
  int32_t t1054;
  int64_t t1055;
  int64_t t1056;
  gg_slice t1057;
  int64_t t1058;
  int64_t t1059;
  int64_t t1060;
  int64_t t1061;
  int64_t t1062;
  uint8_t * t1063;
  gg_slice t1064;
  int64_t t1065;
  int64_t t1066;
  int64_t t1067;
  int64_t t1068;
  int64_t t1069;
  int64_t t1070;
  int64_t t1071;
  int64_t t1072;
  int64_t t1073;
  int64_t t1074;
  int64_t t1075;
  uint8_t * t1076;
  uint16_t t1077;
  uint16_t t1078;
  int64_t t1079;
  int64_t t1080;
  int64_t t1081;
  int64_t t1082;
  uint16_t t1083;
  uint16_t t1084;
  uint16_t t1085;
  uint16_t t1086;
  gg_slice t1087;
  int64_t t1088;
  int64_t t1089;
  int64_t t1090;
  int64_t t1091;
  int64_t t1092;
  int64_t t1093;
  int64_t t1094;
  int64_t t1095;
  int64_t t1096;
  int64_t t1097;
  int64_t t1098;
  int64_t t1099;
  int64_t t1100;
  uint8_t * t1101;
  uint16_t t1102;
  uint16_t t1103;
  int64_t t1104;
  int64_t t1105;
  int64_t t1106;
  int64_t t1107;
  uint16_t t1108;
  uint16_t t1109;
  uint16_t t1110;
  uint16_t t1111;
  gg_slice t1112;
  int64_t t1113;
  gg_type_30 * t1114;
  gg_slice t1115;
  int64_t t1116;
  gg_type_30 * t1117;
  uint8_t t1118;
  uint8_t t1119;
  int64_t t1120;
  int64_t t1121;
  int64_t t1122;
  int64_t t1123;
  uint8_t * t1124;
  int64_t t1125;
  uint8_t * t1126;
  gg_slice t1127;
  int64_t t1128;
  int64_t t1129;
  int64_t t1130;
  uint8_t * t1131;
  int64_t t1132;
  uint8_t * t1133;
  gg_type_50 t1134;
  int64_t t1135;
  int64_t t1136;
  int64_t t1137;
  int64_t t1138;
  int64_t t1139;
  int64_t t1140;
  int64_t t1141;
  int64_t t1142;
  gg_type_28 t1143;
  gg_slice t1144;
  int64_t t1145;
  int64_t t1146;
  int64_t t1147;
  int64_t t1148;
  int64_t t1149;
  int64_t t1150;
  int64_t t1151;
  int64_t t1152;
  int64_t t1153;
  uint8_t * t1154;
  uint8_t t1155;
  int64_t t1156;
  int64_t t1157;
  uint8_t t1158;
  int64_t t1159;
  float * t1160;
  gg_slice t1161;
  int64_t t1162;
  uint16_t t1163;
  float t1164;
  float t1165;
  float t1166;
  int64_t t1167;
  int64_t t1168;
  gg_slice t1169;
  int64_t t1170;
  int64_t t1171;
  int64_t t1172;
  int64_t t1173;
  int64_t t1174;
  int64_t t1175;
  int64_t t1176;
  gg_type_50 t1177;
  int64_t t1178;
  int64_t t1179;
  int64_t t1180;
  uint16_t t1181;
  uint16_t t1182;
  int64_t t1183;
  uint16_t t1184;
  uint16_t t1185;
  uint16_t t1186;
  uint16_t t1187;
  uint16_t t1188;
  int32_t t1189;
  uint16_t t1190;
  uint16_t t1191;
  uint16_t t1192;
  uint16_t t1193;
  uint8_t t1194;
  int16_t t1195;
  int64_t t1196;
  int64_t t1197;
  gg_slice t1198;
  int64_t t1199;
  int64_t t1200;
  int64_t t1201;
  int64_t t1202;
  int64_t t1203;
  int64_t t1204;
  int64_t t1205;
  int64_t t1206;
  int64_t t1207;
  uint8_t * t1208;
  uint16_t t1209;
  uint16_t t1210;
  int64_t t1211;
  int64_t t1212;
  uint16_t t1213;
  uint16_t t1214;
  uint16_t t1215;
  uint16_t t1216;
  int64_t t1217;
  uint16_t t1218;
  gg_type_50 t1219;
  int64_t t1220;
  int64_t t1221;
  int64_t t1222;
  int64_t t1223;
  int64_t t1224;
  int64_t t1225;
  int64_t t1226;
  int64_t t1227;
  gg_slice t1228;
  int64_t t1229;
  gg_type_32 * t1230;
  gg_type_32 t1231;
  int16_t t1232;
  int64_t t1233;
  int64_t t1234;
  gg_slice t1235;
  int64_t t1236;
  int64_t t1237;
  int64_t t1238;
  gg_slice t1239;
  int64_t t1240;
  int64_t t1241;
  int64_t t1242;
  gg_slice t1243;
  int64_t t1244;
  int64_t t1245;
  int64_t t1246;
  gg_slice t1247;
  int64_t t1248;
  int64_t t1249;
  int64_t t1250;
  uint16_t t1251;
  int64_t t1252;
  uint16_t t1253;
  uint16_t t1254;
  int64_t t1255;
  uint16_t t1256;
  uint16_t t1257;
  uint16_t t1258;
  uint16_t t1259;
  uint16_t t1260;
  int64_t t1261;
  uint16_t t1262;
  uint16_t t1263;
  uint16_t t1264;
  uint16_t t1265;
  uint16_t t1266;
  uint16_t t1267;
  uint16_t t1268;
  int64_t t1269;
  float * t1270;
  uint16_t t1271;
  float t1272;
  float t1273;
  float t1274;
  int64_t t1275;
  int64_t t1276;
  gg_slice t1277;
  int64_t t1278;
  int64_t t1279;
  int64_t t1280;
  int64_t t1281;
  int64_t t1282;
  int64_t t1283;
  int64_t t1284;
  int64_t t1285;
  int64_t t1286;
  int64_t t1287;
  int64_t t1288;
  int64_t t1289;
  int64_t t1290;
  gg_type_50 t1291;
  int64_t t1292;
  int64_t t1293;
  int64_t t1294;
  int64_t t1295;
  int64_t t1296;
  uint16_t t1297;
  uint16_t t1298;
  uint16_t t1299;
  uint16_t t1300;
  uint16_t t1301;
  uint16_t t1302;
  uint16_t t1303;
  uint16_t t1304;
  int32_t t1305;
  gg_type_50 t1306;
  int64_t t1307;
  int64_t t1308;
  int64_t t1309;
  int64_t t1310;
  int64_t t1311;
  int64_t t1312;
  int64_t t1313;
  uint16_t t1314;
  uint16_t t1315;
  uint16_t t1316;
  uint16_t t1317;
  uint16_t t1318;
  uint16_t t1319;
  uint16_t t1320;
  uint16_t t1321;
  uint16_t t1322;
  uint16_t t1323;
  int32_t t1324;
  int64_t t1325;
  int64_t t1326;
  gg_slice t1327;
  int64_t t1328;
  int64_t t1329;
  int64_t t1330;
  int64_t t1331;
  int64_t t1332;
  int64_t t1333;
  int64_t t1334;
  int64_t t1335;
  int64_t t1336;
  int64_t t1337;
  int64_t t1338;
  uint8_t * t1339;
  int64_t t1340;
  int64_t t1341;
  int64_t t1342;
  int64_t t1343;
  int64_t t1344;
  uint8_t t1345;
  gg_slice t1346;
  int64_t t1347;
  int64_t t1348;
  int64_t t1349;
  int64_t t1350;
  int64_t t1351;
  int64_t t1352;
  int64_t t1353;
  uint8_t * t1354;
  uint16_t t1355;
  uint16_t t1356;
  uint16_t t1357;
  uint16_t t1358;
  uint16_t t1359;
  uint16_t t1360;
  uint16_t t1361;
  int64_t t1362;
  int64_t t1363;
  int64_t t1364;
  int64_t t1365;
  int64_t t1366;
  uint8_t t1367;
  uint16_t t1368;
  uint16_t t1369;
  uint16_t t1370;
  uint16_t t1371;
  uint16_t t1372;
  uint8_t t1373;
  int16_t t1374;
  gg_type_50 t1375;
  int64_t t1376;
  int64_t t1377;
  int64_t t1378;
  int64_t t1379;
  int64_t t1380;
  int64_t t1381;
  int64_t t1382;
  int64_t t1383;
  gg_slice t1384;
  int64_t t1385;
  gg_type_32 * t1386;
  gg_type_32 t1387;
  int16_t t1388;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  gg_type_50 v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  gg_slice v10;
  gg_slice v11;
  gg_slice v12;
  gg_slice v13;
  gg_type_49 v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  uint8_t v19;
  uint8_t v20;
  uint8_t v21;
  int64_t v22;
  int64_t v23;
  gg_type_18 v24;
  gg_type_18 v25;
  gg_type_18 v26;
  gg_type_18 v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  int64_t v31;
  uint8_t v32;
  uint8_t v33;
  uint8_t v34;
  int64_t v35;
  uint8_t v36;
  uint8_t v37;
  uint8_t v38;
  int64_t v39;
  int64_t v40;
  gg_type_18 v41;
  gg_type_18 v42;
  gg_type_18 v43;
  int64_t v44;
  int64_t v45;
  int64_t v46;
  gg_type_18 v47;
  int64_t v48;
  uint16_t v49;
  int64_t v50;
  uint16_t v51;
  gg_type_18 v52;
  int64_t v53;
  uint8_t v54;
  int64_t v55;
  gg_type_18 v56;
  gg_type_18 v57;
  gg_type_18 v58;
  int64_t v59;
  int64_t v60;
  uint32_t v61;
  int64_t v62;
  int64_t v63;
  int64_t v64;
  int64_t v65;
  uint16_t v66;
  uint16_t v67;
  int64_t v68;
  uint32_t v69;
  int64_t v70;
  gg_type_30 v71;
  gg_type_30 v72;
  gg_type_28 v73;
  int64_t v74;
  int64_t v75;
  uint8_t v76;
  int64_t v77;
  uint16_t v78;
  uint16_t v79;
  uint16_t v80;
  gg_type_30 v81;
  gg_type_30 v82;
  gg_type_28 v83;
  int64_t v84;
  int64_t v85;
  uint16_t v86;
  int16_t v87;
  int64_t v88;
  uint16_t v89;
  uint16_t v90;
  uint16_t v91;
  uint16_t v92;
  uint16_t v93;
  uint16_t v94;
  int64_t v95;
  uint16_t v96;
  uint16_t v97;
  int64_t v98;
  uint16_t v99;
  uint16_t v100;
  uint16_t v101;
  int16_t v102;
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
  t0 = ((int64_t)(v0));
  v14 = gg_function_6(t0, base + 0);
  t1 = ((int64_t)(((int64_t)UINT64_C(256))));
  t2 = ((int64_t)((v14).f61));
  v15 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v15));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  /* FG_DIRECT_SLICE_INDEX */
  t5 = v4;
  t6 = ((float *)(((*(v5))).f51).data) + gg_index(t5, ((int64_t)UINT64_C(256)));
  (*t6) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t8 = v15;
  t7 = 0;
label0:;
  if (t7 >= t8) goto label2;
  v17 = t7;
  {
  t9 = v5;
  t10 = ((int64_t)(v16));
  t11 = ((int64_t)(v17));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int32_t)(((int64_t)UINT64_C(0))));
  gg_function_10(t9, t12, t13, base + 0);
  }
label1:;
  t7++; goto label0;
label2:;
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) == (t15));
  t17 = t16;
  if (!t17) {
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((t18) == (t19));
  }
  t20 = t17;
  if (t20) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t21 = v4;
  t22 = ((float *)(((*(v5))).f50).data) + gg_index(t21, ((int64_t)UINT64_C(256)));
  t23 = v1;
  t24 = ((int64_t)(v2));
  t25 = gg_function_11(t23, t24, base + 0);
  t26 = fg141_half(t25);
  (*t22) = ((float)(t26));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = v4;
  t28 = ((float *)(((*(v5))).f51).data) + gg_index(t27, ((int64_t)UINT64_C(256)));
  t29 = v1;
  t30 = ((int64_t)(v2));
  t31 = ((int64_t)(((int64_t)UINT64_C(2))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  t33 = gg_function_11(t29, t32, base + 0);
  t34 = fg141_half(t33);
  (*t28) = ((float)(t34));
  t36 = ((int64_t)UINT64_C(4));
  t35 = 0;
label3:;
  if (t35 >= t36) goto label5;
  v18 = t35;
  {
  t37 = v1;
  t38 = ((int64_t)(v2));
  t39 = ((int64_t)(((int64_t)UINT64_C(4))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  t41 = ((int64_t)(v18));
  t42 = ((int64_t)((uint64_t)(t40) + (uint64_t)(t41)));
  t43 = ((uint8_t *)t37.data) + gg_index(t42, t37.len);
  v19 = ((uint8_t)((*t43)));
  t44 = v1;
  t45 = ((int64_t)(v2));
  t46 = ((int64_t)(((int64_t)UINT64_C(8))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  t48 = ((int64_t)(v18));
  t49 = ((int64_t)((uint64_t)(t47) + (uint64_t)(t48)));
  t50 = ((uint8_t *)t44.data) + gg_index(t49, t44.len);
  v20 = ((uint8_t)((*t50)));
  t51 = v1;
  t52 = ((int64_t)(v2));
  t53 = ((int64_t)(((int64_t)UINT64_C(12))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((int64_t)(v18));
  t56 = ((int64_t)((uint64_t)(t54) + (uint64_t)(t55)));
  t57 = ((uint8_t *)t51.data) + gg_index(t56, t51.len);
  v21 = ((uint8_t)((*t57)));
  t58 = v5;
  t59 = ((int64_t)(v16));
  t60 = ((int64_t)(v18));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t60)))));
  t62 = ((uint8_t)(v19));
  t63 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t64 = ((int32_t)(((int32_t)(((t62) & (t63))))));
  gg_function_13(t58, t61, t64, base + 0);
  t65 = v5;
  t66 = ((int64_t)(v16));
  t67 = ((int64_t)(v18));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t69 = ((uint8_t)(v20));
  t70 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t71 = ((int32_t)(((int32_t)(((t69) & (t70))))));
  gg_function_10(t65, t68, t71, base + 0);
  t72 = v5;
  t73 = ((int64_t)(v16));
  t74 = ((int64_t)(v18));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t74)))));
  t76 = ((int64_t)(((int64_t)UINT64_C(4))));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t76)))));
  t78 = ((uint8_t)(v21));
  t79 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t80 = ((uint8_t)(((t78) & (t79))));
  t81 = ((uint8_t)(v19));
  t82 = ((int64_t)(((int64_t)UINT64_C(6))));
  t83 = ((uint8_t)(((t81) >> ((uint64_t)(t82) & 7))));
  t84 = ((int64_t)(((int64_t)UINT64_C(4))));
  t85 = ((uint8_t)(((uint8_t)((uint64_t)(t83) << ((uint64_t)(t84) & 7)))));
  t86 = ((int32_t)(((int32_t)(((t80) | (t85))))));
  gg_function_13(t72, t77, t86, base + 0);
  t87 = v5;
  t88 = ((int64_t)(v16));
  t89 = ((int64_t)(v18));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))));
  t91 = ((int64_t)(((int64_t)UINT64_C(4))));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t91)))));
  t93 = ((uint8_t)(v21));
  t94 = ((int64_t)(((int64_t)UINT64_C(4))));
  t95 = ((uint8_t)(((t93) >> ((uint64_t)(t94) & 7))));
  t96 = ((uint8_t)(v20));
  t97 = ((int64_t)(((int64_t)UINT64_C(6))));
  t98 = ((uint8_t)(((t96) >> ((uint64_t)(t97) & 7))));
  t99 = ((int64_t)(((int64_t)UINT64_C(4))));
  t100 = ((uint8_t)(((uint8_t)((uint64_t)(t98) << ((uint64_t)(t99) & 7)))));
  t101 = ((int32_t)(((int32_t)(((t95) | (t100))))));
  gg_function_10(t87, t92, t101, base + 0);
  }
label4:;
  t35++; goto label3;
label5:;
  t102 = ((int64_t)(v2));
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((t104) == (t105));
  if (t106) {
  {
  t103 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  } else {
  {
  t103 = ((int64_t)(((int64_t)UINT64_C(48))));
  }
  }
  t107 = ((int64_t)(t103));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t107)))));
  t109 = ((int64_t)UINT64_C(4));
  t108 = 0;
label6:;
  if (t108 >= t109) goto label8;
  v23 = t108;
  {
  t110 = v1;
  t111 = ((int64_t)(v22));
  t112 = ((int64_t)(v23));
  t113 = ((int64_t)(((int64_t)UINT64_C(32))));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t113)))));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t111) + (uint64_t)(t114)))));
  v24 = gg_function_14(t110, t115, base + 0);
  t116 = v24;
  t117 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v25 = gg_function_15(t116, t117, base + 0);
  t118 = v24;
  t119 = ((uint8_t)(((int64_t)UINT64_C(4))));
  v26 = gg_function_16(t118, t119, base + 0);
  t120 = ((int64_t)(v0));
  t121 = ((int64_t)(((int64_t)UINT64_C(1))));
  t122 = ((t120) == (t121));
  if (t122) {
  {
  t123 = v1;
  t124 = ((int64_t)(v2));
  t125 = ((int64_t)(((int64_t)UINT64_C(16))));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t124) + (uint64_t)(t125)))));
  v27 = gg_function_14(t123, t126, base + 0);
  t128 = ((int64_t)UINT64_C(32));
  t127 = 0;
label9:;
  if (t127 >= t128) goto label11;
  v28 = t127;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t129 = v28;
  t130 = ((uint8_t *)(v25).data) + gg_index(t129, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t131 = v28;
  t132 = ((uint8_t *)(v25).data) + gg_index(t131, ((int64_t)UINT64_C(32)));
  t133 = ((uint8_t)((*t132)));
  /* FG_DIRECT_SLICE_INDEX */
  t134 = v28;
  t135 = ((uint8_t *)(v27).data) + gg_index(t134, ((int64_t)UINT64_C(32)));
  t136 = ((uint8_t)((*t135)));
  t137 = ((int64_t)(((int64_t)UINT64_C(2))));
  t138 = ((int64_t)(v23));
  t139 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t137) * (uint64_t)(t138)))))));
  t140 = ((uint8_t)(((t136) >> ((uint64_t)(t139) & 7))));
  t141 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t142 = ((uint8_t)(((t140) & (t141))));
  t143 = ((int64_t)(((int64_t)UINT64_C(4))));
  t144 = ((uint8_t)(((uint8_t)((uint64_t)(t142) << ((uint64_t)(t143) & 7)))));
  (*t130) = ((uint8_t)(((t133) | (t144))));
  /* FG_DIRECT_SLICE_INDEX */
  t145 = v28;
  t146 = ((uint8_t *)(v26).data) + gg_index(t145, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t147 = v28;
  t148 = ((uint8_t *)(v26).data) + gg_index(t147, ((int64_t)UINT64_C(32)));
  t149 = ((uint8_t)((*t148)));
  /* FG_DIRECT_SLICE_INDEX */
  t150 = v28;
  t151 = ((uint8_t *)(v27).data) + gg_index(t150, ((int64_t)UINT64_C(32)));
  t152 = ((uint8_t)((*t151)));
  t153 = ((int64_t)(((int64_t)UINT64_C(2))));
  t154 = ((int64_t)(v23));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) * (uint64_t)(t154)))));
  t156 = ((int64_t)(((int64_t)UINT64_C(1))));
  t157 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t155) + (uint64_t)(t156)))))));
  t158 = ((uint8_t)(((t152) >> ((uint64_t)(t157) & 7))));
  t159 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t160 = ((uint8_t)(((t158) & (t159))));
  t161 = ((int64_t)(((int64_t)UINT64_C(4))));
  t162 = ((uint8_t)(((uint8_t)((uint64_t)(t160) << ((uint64_t)(t161) & 7)))));
  (*t146) = ((uint8_t)(((t149) | (t162))));
  }
label10:;
  t127++; goto label9;
label11:;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t163 = v3;
  t164 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t163, ((int64_t)UINT64_C(256)));
  t165 = gg_view(((*t164)).data, 8);
  t166 = ((int64_t)(((int64_t)UINT64_C(2))));
  t167 = ((int64_t)(v23));
  t168 = ((int64_t)((uint64_t)(t166) * (uint64_t)(t167)));
  t169 = ((gg_type_18 *)t165.data) + gg_index(t168, t165.len);
  (*t169) = v25;
  /* FG_DIRECT_SLICE_INDEX */
  t170 = v3;
  t171 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t170, ((int64_t)UINT64_C(256)));
  t172 = gg_view(((*t171)).data, 8);
  t173 = ((int64_t)(((int64_t)UINT64_C(2))));
  t174 = ((int64_t)(v23));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) * (uint64_t)(t174)))));
  t176 = ((int64_t)(((int64_t)UINT64_C(1))));
  t177 = ((int64_t)((uint64_t)(t175) + (uint64_t)(t176)));
  t178 = ((gg_type_18 *)t172.data) + gg_index(t177, t172.len);
  (*t178) = v26;
  }
label7:;
  t108++; goto label6;
label8:;
  }
  } else {
  t179 = ((int64_t)(v0));
  t180 = ((int64_t)(((int64_t)UINT64_C(2))));
  t181 = ((t179) == (t180));
  if (t181) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t182 = v4;
  t183 = ((float *)(((*(v5))).f50).data) + gg_index(t182, ((int64_t)UINT64_C(256)));
  t184 = v1;
  t185 = ((int64_t)(v2));
  t186 = ((int64_t)(((int64_t)UINT64_C(208))));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t185) + (uint64_t)(t186)))));
  t188 = gg_function_11(t184, t187, base + 0);
  t189 = fg141_half(t188);
  (*t183) = ((float)(t189));
  t191 = ((int64_t)UINT64_C(16));
  t190 = 0;
label12:;
  if (t190 >= t191) goto label14;
  v29 = t190;
  {
  t192 = v5;
  t193 = ((int64_t)(v16));
  t194 = ((int64_t)(v29));
  t195 = ((int64_t)(((int64_t)((uint64_t)(t193) + (uint64_t)(t194)))));
  t196 = v1;
  t197 = ((int64_t)(v2));
  t198 = ((int64_t)(((int64_t)UINT64_C(192))));
  t199 = ((int64_t)(((int64_t)((uint64_t)(t197) + (uint64_t)(t198)))));
  t200 = ((int64_t)(v29));
  t201 = ((int64_t)((uint64_t)(t199) + (uint64_t)(t200)));
  t202 = ((uint8_t *)t196.data) + gg_index(t201, t196.len);
  t203 = ((int32_t)(((int32_t)(((int8_t)((*t202)))))));
  gg_function_13(t192, t195, t203, base + 0);
  }
label13:;
  t190++; goto label12;
label14:;
  t205 = ((int64_t)UINT64_C(2));
  t204 = 0;
label15:;
  if (t204 >= t205) goto label17;
  v30 = t204;
  {
  t207 = ((int64_t)UINT64_C(32));
  t206 = 0;
label18:;
  if (t206 >= t207) goto label20;
  v31 = t206;
  {
  t208 = v1;
  t209 = ((int64_t)(v2));
  t210 = ((int64_t)(((int64_t)UINT64_C(128))));
  t211 = ((int64_t)(((int64_t)((uint64_t)(t209) + (uint64_t)(t210)))));
  t212 = ((int64_t)(v30));
  t213 = ((int64_t)(((int64_t)UINT64_C(32))));
  t214 = ((int64_t)(((int64_t)((uint64_t)(t212) * (uint64_t)(t213)))));
  t215 = ((int64_t)(((int64_t)((uint64_t)(t211) + (uint64_t)(t214)))));
  t216 = ((int64_t)(v31));
  t217 = ((int64_t)((uint64_t)(t215) + (uint64_t)(t216)));
  t218 = ((uint8_t *)t208.data) + gg_index(t217, t208.len);
  v32 = ((uint8_t)((*t218)));
  t219 = v1;
  t220 = ((int64_t)(v2));
  t221 = ((int64_t)(v30));
  t222 = ((int64_t)(((int64_t)UINT64_C(64))));
  t223 = ((int64_t)(((int64_t)((uint64_t)(t221) * (uint64_t)(t222)))));
  t224 = ((int64_t)(((int64_t)((uint64_t)(t220) + (uint64_t)(t223)))));
  t225 = ((int64_t)(v31));
  t226 = ((int64_t)((uint64_t)(t224) + (uint64_t)(t225)));
  t227 = ((uint8_t *)t219.data) + gg_index(t226, t219.len);
  v33 = ((uint8_t)((*t227)));
  t228 = v1;
  t229 = ((int64_t)(v2));
  t230 = ((int64_t)(v30));
  t231 = ((int64_t)(((int64_t)UINT64_C(64))));
  t232 = ((int64_t)(((int64_t)((uint64_t)(t230) * (uint64_t)(t231)))));
  t233 = ((int64_t)(((int64_t)((uint64_t)(t229) + (uint64_t)(t232)))));
  t234 = ((int64_t)(((int64_t)UINT64_C(32))));
  t235 = ((int64_t)(((int64_t)((uint64_t)(t233) + (uint64_t)(t234)))));
  t236 = ((int64_t)(v31));
  t237 = ((int64_t)((uint64_t)(t235) + (uint64_t)(t236)));
  t238 = ((uint8_t *)t228.data) + gg_index(t237, t228.len);
  v34 = ((uint8_t)((*t238)));
  /* FG_DIRECT_SLICE_INDEX */
  t239 = v3;
  t240 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t239, ((int64_t)UINT64_C(256)));
  t241 = gg_view(((*t240)).data, 8);
  t242 = ((int64_t)(v30));
  t243 = ((int64_t)(((int64_t)UINT64_C(4))));
  t244 = ((int64_t)((uint64_t)(t242) * (uint64_t)(t243)));
  t245 = ((gg_type_18 *)t241.data) + gg_index(t244, t241.len);
  t246 = gg_view(((*t245)).data, 32);
  t247 = v31;
  t248 = ((uint8_t *)t246.data) + gg_index(t247, t246.len);
  t249 = ((uint8_t)(v33));
  t250 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t251 = ((uint8_t)(((t249) & (t250))));
  t252 = ((uint8_t)(v32));
  t253 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t254 = ((uint8_t)(((t252) & (t253))));
  t255 = ((int64_t)(((int64_t)UINT64_C(4))));
  t256 = ((uint8_t)(((uint8_t)((uint64_t)(t254) << ((uint64_t)(t255) & 7)))));
  (*t248) = ((uint8_t)(((t251) | (t256))));
  /* FG_DIRECT_SLICE_INDEX */
  t257 = v3;
  t258 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t257, ((int64_t)UINT64_C(256)));
  t259 = gg_view(((*t258)).data, 8);
  t260 = ((int64_t)(v30));
  t261 = ((int64_t)(((int64_t)UINT64_C(4))));
  t262 = ((int64_t)(((int64_t)((uint64_t)(t260) * (uint64_t)(t261)))));
  t263 = ((int64_t)(((int64_t)UINT64_C(1))));
  t264 = ((int64_t)((uint64_t)(t262) + (uint64_t)(t263)));
  t265 = ((gg_type_18 *)t259.data) + gg_index(t264, t259.len);
  t266 = gg_view(((*t265)).data, 32);
  t267 = v31;
  t268 = ((uint8_t *)t266.data) + gg_index(t267, t266.len);
  t269 = ((uint8_t)(v34));
  t270 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t271 = ((uint8_t)(((t269) & (t270))));
  t272 = ((uint8_t)(v32));
  t273 = ((uint8_t)(((int64_t)UINT64_C(12))));
  t274 = ((uint8_t)(((t272) & (t273))));
  t275 = ((int64_t)(((int64_t)UINT64_C(2))));
  t276 = ((uint8_t)(((uint8_t)((uint64_t)(t274) << ((uint64_t)(t275) & 7)))));
  (*t268) = ((uint8_t)(((t271) | (t276))));
  /* FG_DIRECT_SLICE_INDEX */
  t277 = v3;
  t278 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t277, ((int64_t)UINT64_C(256)));
  t279 = gg_view(((*t278)).data, 8);
  t280 = ((int64_t)(v30));
  t281 = ((int64_t)(((int64_t)UINT64_C(4))));
  t282 = ((int64_t)(((int64_t)((uint64_t)(t280) * (uint64_t)(t281)))));
  t283 = ((int64_t)(((int64_t)UINT64_C(2))));
  t284 = ((int64_t)((uint64_t)(t282) + (uint64_t)(t283)));
  t285 = ((gg_type_18 *)t279.data) + gg_index(t284, t279.len);
  t286 = gg_view(((*t285)).data, 32);
  t287 = v31;
  t288 = ((uint8_t *)t286.data) + gg_index(t287, t286.len);
  t289 = ((uint8_t)(v33));
  t290 = ((int64_t)(((int64_t)UINT64_C(4))));
  t291 = ((uint8_t)(((t289) >> ((uint64_t)(t290) & 7))));
  t292 = ((uint8_t)(v32));
  t293 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t294 = ((uint8_t)(((t292) & (t293))));
  (*t288) = ((uint8_t)(((t291) | (t294))));
  /* FG_DIRECT_SLICE_INDEX */
  t295 = v3;
  t296 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t295, ((int64_t)UINT64_C(256)));
  t297 = gg_view(((*t296)).data, 8);
  t298 = ((int64_t)(v30));
  t299 = ((int64_t)(((int64_t)UINT64_C(4))));
  t300 = ((int64_t)(((int64_t)((uint64_t)(t298) * (uint64_t)(t299)))));
  t301 = ((int64_t)(((int64_t)UINT64_C(3))));
  t302 = ((int64_t)((uint64_t)(t300) + (uint64_t)(t301)));
  t303 = ((gg_type_18 *)t297.data) + gg_index(t302, t297.len);
  t304 = gg_view(((*t303)).data, 32);
  t305 = v31;
  t306 = ((uint8_t *)t304.data) + gg_index(t305, t304.len);
  t307 = ((uint8_t)(v34));
  t308 = ((int64_t)(((int64_t)UINT64_C(4))));
  t309 = ((uint8_t)(((t307) >> ((uint64_t)(t308) & 7))));
  t310 = ((uint8_t)(v32));
  t311 = ((uint8_t)(((int64_t)UINT64_C(192))));
  t312 = ((uint8_t)(((t310) & (t311))));
  t313 = ((int64_t)(((int64_t)UINT64_C(2))));
  t314 = ((uint8_t)(((t312) >> ((uint64_t)(t313) & 7))));
  (*t306) = ((uint8_t)(((t309) | (t314))));
  }
label19:;
  t206++; goto label18;
label20:;
  }
label16:;
  t204++; goto label15;
label17:;
  }
  } else {
  t315 = ((int64_t)(v0));
  t316 = ((int64_t)(((int64_t)UINT64_C(3))));
  t317 = ((t315) == (t316));
  if (t317) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t318 = v4;
  t319 = ((float *)(((*(v5))).f50).data) + gg_index(t318, ((int64_t)UINT64_C(256)));
  t320 = v1;
  t321 = ((int64_t)(v2));
  t322 = ((int64_t)(((int64_t)UINT64_C(108))));
  t323 = ((int64_t)(((int64_t)((uint64_t)(t321) + (uint64_t)(t322)))));
  t324 = gg_function_11(t320, t323, base + 0);
  t325 = fg141_half(t324);
  (*t319) = ((float)(t325));
  t327 = ((int64_t)UINT64_C(4));
  t326 = 0;
label21:;
  if (t326 >= t327) goto label23;
  v35 = t326;
  {
  t328 = v1;
  t329 = ((int64_t)(v2));
  t330 = ((int64_t)(((int64_t)UINT64_C(96))));
  t331 = ((int64_t)(((int64_t)((uint64_t)(t329) + (uint64_t)(t330)))));
  t332 = ((int64_t)(v35));
  t333 = ((int64_t)((uint64_t)(t331) + (uint64_t)(t332)));
  t334 = ((uint8_t *)t328.data) + gg_index(t333, t328.len);
  v36 = ((uint8_t)((*t334)));
  t335 = v1;
  t336 = ((int64_t)(v2));
  t337 = ((int64_t)(((int64_t)UINT64_C(100))));
  t338 = ((int64_t)(((int64_t)((uint64_t)(t336) + (uint64_t)(t337)))));
  t339 = ((int64_t)(v35));
  t340 = ((int64_t)((uint64_t)(t338) + (uint64_t)(t339)));
  t341 = ((uint8_t *)t335.data) + gg_index(t340, t335.len);
  v37 = ((uint8_t)((*t341)));
  t342 = v1;
  t343 = ((int64_t)(v2));
  t344 = ((int64_t)(((int64_t)UINT64_C(104))));
  t345 = ((int64_t)(((int64_t)((uint64_t)(t343) + (uint64_t)(t344)))));
  t346 = ((int64_t)(v35));
  t347 = ((int64_t)((uint64_t)(t345) + (uint64_t)(t346)));
  t348 = ((uint8_t *)t342.data) + gg_index(t347, t342.len);
  v38 = ((uint8_t)((*t348)));
  t349 = v5;
  t350 = ((int64_t)(v16));
  t351 = ((int64_t)(v35));
  t352 = ((int64_t)(((int64_t)((uint64_t)(t350) + (uint64_t)(t351)))));
  t353 = ((uint8_t)(v36));
  t354 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t355 = ((uint8_t)(((t353) & (t354))));
  t356 = ((uint8_t)(v38));
  t357 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t358 = ((uint8_t)(((t356) & (t357))));
  t359 = ((int64_t)(((int64_t)UINT64_C(4))));
  t360 = ((uint8_t)(((uint8_t)((uint64_t)(t358) << ((uint64_t)(t359) & 7)))));
  t361 = ((int32_t)(((int32_t)(((t355) | (t360))))));
  t362 = ((int32_t)(((int64_t)UINT64_C(32))));
  t363 = ((int32_t)(((int32_t)((uint64_t)(t361) - (uint64_t)(t362)))));
  gg_function_13(t349, t352, t363, base + 0);
  t364 = v5;
  t365 = ((int64_t)(v16));
  t366 = ((int64_t)(((int64_t)UINT64_C(4))));
  t367 = ((int64_t)(((int64_t)((uint64_t)(t365) + (uint64_t)(t366)))));
  t368 = ((int64_t)(v35));
  t369 = ((int64_t)(((int64_t)((uint64_t)(t367) + (uint64_t)(t368)))));
  t370 = ((uint8_t)(v37));
  t371 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t372 = ((uint8_t)(((t370) & (t371))));
  t373 = ((uint8_t)(v38));
  t374 = ((int64_t)(((int64_t)UINT64_C(2))));
  t375 = ((uint8_t)(((t373) >> ((uint64_t)(t374) & 7))));
  t376 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t377 = ((uint8_t)(((t375) & (t376))));
  t378 = ((int64_t)(((int64_t)UINT64_C(4))));
  t379 = ((uint8_t)(((uint8_t)((uint64_t)(t377) << ((uint64_t)(t378) & 7)))));
  t380 = ((int32_t)(((int32_t)(((t372) | (t379))))));
  t381 = ((int32_t)(((int64_t)UINT64_C(32))));
  t382 = ((int32_t)(((int32_t)((uint64_t)(t380) - (uint64_t)(t381)))));
  gg_function_13(t364, t369, t382, base + 0);
  t383 = v5;
  t384 = ((int64_t)(v16));
  t385 = ((int64_t)(((int64_t)UINT64_C(8))));
  t386 = ((int64_t)(((int64_t)((uint64_t)(t384) + (uint64_t)(t385)))));
  t387 = ((int64_t)(v35));
  t388 = ((int64_t)(((int64_t)((uint64_t)(t386) + (uint64_t)(t387)))));
  t389 = ((uint8_t)(v36));
  t390 = ((int64_t)(((int64_t)UINT64_C(4))));
  t391 = ((uint8_t)(((t389) >> ((uint64_t)(t390) & 7))));
  t392 = ((uint8_t)(v38));
  t393 = ((int64_t)(((int64_t)UINT64_C(4))));
  t394 = ((uint8_t)(((t392) >> ((uint64_t)(t393) & 7))));
  t395 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t396 = ((uint8_t)(((t394) & (t395))));
  t397 = ((int64_t)(((int64_t)UINT64_C(4))));
  t398 = ((uint8_t)(((uint8_t)((uint64_t)(t396) << ((uint64_t)(t397) & 7)))));
  t399 = ((int32_t)(((int32_t)(((t391) | (t398))))));
  t400 = ((int32_t)(((int64_t)UINT64_C(32))));
  t401 = ((int32_t)(((int32_t)((uint64_t)(t399) - (uint64_t)(t400)))));
  gg_function_13(t383, t388, t401, base + 0);
  t402 = v5;
  t403 = ((int64_t)(v16));
  t404 = ((int64_t)(((int64_t)UINT64_C(12))));
  t405 = ((int64_t)(((int64_t)((uint64_t)(t403) + (uint64_t)(t404)))));
  t406 = ((int64_t)(v35));
  t407 = ((int64_t)(((int64_t)((uint64_t)(t405) + (uint64_t)(t406)))));
  t408 = ((uint8_t)(v37));
  t409 = ((int64_t)(((int64_t)UINT64_C(4))));
  t410 = ((uint8_t)(((t408) >> ((uint64_t)(t409) & 7))));
  t411 = ((uint8_t)(v38));
  t412 = ((int64_t)(((int64_t)UINT64_C(6))));
  t413 = ((uint8_t)(((t411) >> ((uint64_t)(t412) & 7))));
  t414 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t415 = ((uint8_t)(((t413) & (t414))));
  t416 = ((int64_t)(((int64_t)UINT64_C(4))));
  t417 = ((uint8_t)(((uint8_t)((uint64_t)(t415) << ((uint64_t)(t416) & 7)))));
  t418 = ((int32_t)(((int32_t)(((t410) | (t417))))));
  t419 = ((int32_t)(((int64_t)UINT64_C(32))));
  t420 = ((int32_t)(((int32_t)((uint64_t)(t418) - (uint64_t)(t419)))));
  gg_function_13(t402, t407, t420, base + 0);
  }
label22:;
  t326++; goto label21;
label23:;
  t422 = ((int64_t)UINT64_C(2));
  t421 = 0;
label24:;
  if (t421 >= t422) goto label26;
  v39 = t421;
  {
  t424 = ((int64_t)UINT64_C(4));
  t423 = 0;
label27:;
  if (t423 >= t424) goto label29;
  v40 = t423;
  {
  t425 = v1;
  t426 = ((int64_t)(v2));
  t427 = ((int64_t)(((int64_t)UINT64_C(32))));
  t428 = ((int64_t)(((int64_t)((uint64_t)(t426) + (uint64_t)(t427)))));
  t429 = ((int64_t)(v39));
  t430 = ((int64_t)(((int64_t)UINT64_C(32))));
  t431 = ((int64_t)(((int64_t)((uint64_t)(t429) * (uint64_t)(t430)))));
  t432 = ((int64_t)(((int64_t)((uint64_t)(t428) + (uint64_t)(t431)))));
  v41 = gg_function_14(t425, t432, base + 0);
  t433 = v1;
  t434 = ((int64_t)(v2));
  v42 = gg_function_14(t433, t434, base + 0);
  memset(&t435, 0, sizeof(t435));
  t436 = ((uint8_t)(t435));
  for (t437=0; t437<32; t437++) (v43).data[t437] = t436;
  t439 = ((int64_t)UINT64_C(32));
  t438 = 0;
label30:;
  if (t438 >= t439) goto label32;
  v44 = t438;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t440 = v44;
  t441 = ((uint8_t *)(v43).data) + gg_index(t440, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t442 = v44;
  t443 = ((uint8_t *)(v41).data) + gg_index(t442, ((int64_t)UINT64_C(32)));
  t444 = ((uint8_t)((*t443)));
  t445 = ((int64_t)(((int64_t)UINT64_C(2))));
  t446 = ((int64_t)(v40));
  t447 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t445) * (uint64_t)(t446)))))));
  t448 = ((uint8_t)(((t444) >> ((uint64_t)(t447) & 7))));
  t449 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t450 = ((uint8_t)(((t448) & (t449))));
  /* FG_DIRECT_SLICE_INDEX */
  t451 = v44;
  t452 = ((uint8_t *)(v42).data) + gg_index(t451, ((int64_t)UINT64_C(32)));
  t453 = ((uint8_t)((*t452)));
  t454 = ((int64_t)(v39));
  t455 = ((int64_t)(((int64_t)UINT64_C(4))));
  t456 = ((int64_t)(((int64_t)((uint64_t)(t454) * (uint64_t)(t455)))));
  t457 = ((int64_t)(v40));
  t458 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t456) + (uint64_t)(t457)))))));
  t459 = ((uint8_t)(((t453) >> ((uint64_t)(t458) & 7))));
  t460 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t461 = ((uint8_t)(((t459) & (t460))));
  t462 = ((int64_t)(((int64_t)UINT64_C(2))));
  t463 = ((uint8_t)(((uint8_t)((uint64_t)(t461) << ((uint64_t)(t462) & 7)))));
  (*t441) = ((uint8_t)(((t450) | (t463))));
  }
label31:;
  t438++; goto label30;
label32:;
  /* FG_DIRECT_SLICE_INDEX */
  t464 = v3;
  t465 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t464, ((int64_t)UINT64_C(256)));
  t466 = gg_view(((*t465)).data, 8);
  t467 = ((int64_t)(v39));
  t468 = ((int64_t)(((int64_t)UINT64_C(4))));
  t469 = ((int64_t)(((int64_t)((uint64_t)(t467) * (uint64_t)(t468)))));
  t470 = ((int64_t)(v40));
  t471 = ((int64_t)((uint64_t)(t469) + (uint64_t)(t470)));
  t472 = ((gg_type_18 *)t466.data) + gg_index(t471, t466.len);
  (*t472) = v43;
  }
label28:;
  t423++; goto label27;
label29:;
  }
label25:;
  t421++; goto label24;
label26:;
  }
  } else {
  t473 = ((int64_t)(v0));
  t474 = ((int64_t)(((int64_t)UINT64_C(4))));
  t475 = ((t473) == (t474));
  if (t475) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t476 = v4;
  t477 = ((float *)(((*(v5))).f50).data) + gg_index(t476, ((int64_t)UINT64_C(256)));
  t478 = v1;
  t479 = ((int64_t)(v2));
  t480 = ((int64_t)(((int64_t)UINT64_C(80))));
  t481 = ((int64_t)(((int64_t)((uint64_t)(t479) + (uint64_t)(t480)))));
  t482 = gg_function_11(t478, t481, base + 0);
  t483 = fg141_half(t482);
  (*t477) = ((float)(t483));
  /* FG_DIRECT_SLICE_INDEX */
  t484 = v4;
  t485 = ((float *)(((*(v5))).f51).data) + gg_index(t484, ((int64_t)UINT64_C(256)));
  t486 = v1;
  t487 = ((int64_t)(v2));
  t488 = ((int64_t)(((int64_t)UINT64_C(82))));
  t489 = ((int64_t)(((int64_t)((uint64_t)(t487) + (uint64_t)(t488)))));
  t490 = gg_function_11(t486, t489, base + 0);
  t491 = fg141_half(t490);
  (*t485) = ((float)(t491));
  t493 = ((int64_t)UINT64_C(16));
  t492 = 0;
label33:;
  if (t492 >= t493) goto label35;
  v45 = t492;
  {
  t494 = v5;
  t495 = ((int64_t)(v16));
  t496 = ((int64_t)(v45));
  t497 = ((int64_t)(((int64_t)((uint64_t)(t495) + (uint64_t)(t496)))));
  t498 = v1;
  t499 = ((int64_t)(v2));
  t500 = ((int64_t)(v45));
  t501 = ((int64_t)((uint64_t)(t499) + (uint64_t)(t500)));
  t502 = ((uint8_t *)t498.data) + gg_index(t501, t498.len);
  t503 = ((uint8_t)((*t502)));
  t504 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t505 = ((int32_t)(((int32_t)(((t503) & (t504))))));
  gg_function_13(t494, t497, t505, base + 0);
  t506 = v5;
  t507 = ((int64_t)(v16));
  t508 = ((int64_t)(v45));
  t509 = ((int64_t)(((int64_t)((uint64_t)(t507) + (uint64_t)(t508)))));
  t510 = v1;
  t511 = ((int64_t)(v2));
  t512 = ((int64_t)(v45));
  t513 = ((int64_t)((uint64_t)(t511) + (uint64_t)(t512)));
  t514 = ((uint8_t *)t510.data) + gg_index(t513, t510.len);
  t515 = ((uint8_t)((*t514)));
  t516 = ((int64_t)(((int64_t)UINT64_C(4))));
  t517 = ((int32_t)(((int32_t)(((t515) >> ((uint64_t)(t516) & 7))))));
  gg_function_10(t506, t509, t517, base + 0);
  }
label34:;
  t492++; goto label33;
label35:;
  t519 = ((int64_t)UINT64_C(2));
  t518 = 0;
label36:;
  if (t518 >= t519) goto label38;
  v46 = t518;
  {
  t520 = v1;
  t521 = ((int64_t)(v2));
  t522 = ((int64_t)(((int64_t)UINT64_C(16))));
  t523 = ((int64_t)(((int64_t)((uint64_t)(t521) + (uint64_t)(t522)))));
  t524 = ((int64_t)(v46));
  t525 = ((int64_t)(((int64_t)UINT64_C(32))));
  t526 = ((int64_t)(((int64_t)((uint64_t)(t524) * (uint64_t)(t525)))));
  t527 = ((int64_t)(((int64_t)((uint64_t)(t523) + (uint64_t)(t526)))));
  v47 = gg_function_14(t520, t527, base + 0);
  t529 = ((int64_t)UINT64_C(4));
  t528 = 0;
label39:;
  if (t528 >= t529) goto label41;
  v48 = t528;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t530 = v3;
  t531 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t530, ((int64_t)UINT64_C(256)));
  t532 = gg_view(((*t531)).data, 8);
  t533 = ((int64_t)(v46));
  t534 = ((int64_t)(((int64_t)UINT64_C(4))));
  t535 = ((int64_t)(((int64_t)((uint64_t)(t533) * (uint64_t)(t534)))));
  t536 = ((int64_t)(v48));
  t537 = ((int64_t)((uint64_t)(t535) + (uint64_t)(t536)));
  t538 = ((gg_type_18 *)t532.data) + gg_index(t537, t532.len);
  t539 = v47;
  t540 = ((int64_t)(((int64_t)UINT64_C(2))));
  t541 = ((int64_t)(v48));
  t542 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t540) * (uint64_t)(t541)))))));
  t543 = gg_function_16(t539, t542, base + 0);
  t544 = ((uint8_t)(((int64_t)UINT64_C(3))));
  (*t538) = gg_function_15(t543, t544, base + 0);
  }
label40:;
  t528++; goto label39;
label41:;
  }
label37:;
  t518++; goto label36;
label38:;
  }
  } else {
  t545 = ((int64_t)(v0));
  t546 = ((int64_t)(((int64_t)UINT64_C(5))));
  t547 = ((t545) == (t546));
  if (t547) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t548 = v4;
  t549 = ((float *)(((*(v5))).f50).data) + gg_index(t548, ((int64_t)UINT64_C(256)));
  t550 = v1;
  t551 = ((int64_t)(v2));
  t552 = gg_function_11(t550, t551, base + 0);
  t553 = fg141_half(t552);
  (*t549) = ((float)(t553));
  t554 = v1;
  t555 = ((int64_t)(v2));
  t556 = ((int64_t)(((int64_t)UINT64_C(2))));
  t557 = ((int64_t)(((int64_t)((uint64_t)(t555) + (uint64_t)(t556)))));
  v49 = gg_function_11(t554, t557, base + 0);
  t559 = ((int64_t)UINT64_C(8));
  t558 = 0;
label42:;
  if (t558 >= t559) goto label44;
  v50 = t558;
  {
  t560 = v1;
  t561 = ((int64_t)(v2));
  t562 = ((int64_t)(((int64_t)UINT64_C(4))));
  t563 = ((int64_t)(((int64_t)((uint64_t)(t561) + (uint64_t)(t562)))));
  t564 = ((int64_t)(v50));
  t565 = ((int64_t)(((int64_t)UINT64_C(2))));
  t566 = ((int64_t)(((int64_t)gg_div(t564, t565))));
  t567 = ((int64_t)((uint64_t)(t563) + (uint64_t)(t566)));
  t568 = ((uint8_t *)t560.data) + gg_index(t567, t560.len);
  t569 = ((uint8_t)((*t568)));
  t570 = ((int64_t)(((int64_t)UINT64_C(4))));
  t571 = ((int64_t)(v50));
  t572 = ((int64_t)(((int64_t)UINT64_C(2))));
  t573 = ((int64_t)(((int64_t)gg_mod(t571, t572))));
  t574 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t570) * (uint64_t)(t573)))))));
  t575 = ((uint8_t)(((t569) >> ((uint64_t)(t574) & 7))));
  t576 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v51 = ((uint16_t)(((uint16_t)(((t575) & (t576))))));
  t577 = v5;
  t578 = ((int64_t)(v16));
  t579 = ((int64_t)(v50));
  t580 = ((int64_t)(((int64_t)((uint64_t)(t578) + (uint64_t)(t579)))));
  t581 = ((uint16_t)(v51));
  t582 = ((uint16_t)(v49));
  t583 = ((int64_t)(((int64_t)UINT64_C(2))));
  t584 = ((int64_t)(v50));
  t585 = ((uint16_t)(((uint16_t)(((int64_t)((uint64_t)(t583) * (uint64_t)(t584)))))));
  t586 = ((uint16_t)(((t582) >> ((uint64_t)(t585) & 15))));
  t587 = ((uint16_t)(((int64_t)UINT64_C(3))));
  t588 = ((uint16_t)(((t586) & (t587))));
  t589 = ((int64_t)(((int64_t)UINT64_C(4))));
  t590 = ((uint16_t)(((uint16_t)((uint64_t)(t588) << ((uint64_t)(t589) & 15)))));
  t591 = ((int32_t)(((int32_t)(((t581) | (t590))))));
  t592 = ((int32_t)(((int64_t)UINT64_C(32))));
  t593 = ((int32_t)(((int32_t)((uint64_t)(t591) - (uint64_t)(t592)))));
  gg_function_13(t577, t580, t593, base + 0);
  }
label43:;
  t558++; goto label42;
label44:;
  memset(&t594, 0, sizeof(t594));
  t595 = ((uint8_t)(t594));
  for (t596=0; t596<32; t596++) (v52).data[t596] = t595;
  t598 = ((int64_t)UINT64_C(16));
  t597 = 0;
label45:;
  if (t597 >= t598) goto label47;
  v53 = t597;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t599 = v53;
  t600 = ((int8_t *)(v13).data) + gg_index(t599, (v13).len);
  t601 = ((int16_t)(((int16_t)((*t600)))));
  t602 = ((int16_t)(((int64_t)UINT64_C(128))));
  v54 = ((uint8_t)(((uint8_t)(((int16_t)((uint64_t)(t601) + (uint64_t)(t602)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t603 = v53;
  t604 = ((uint8_t *)(v52).data) + gg_index(t603, ((int64_t)UINT64_C(32)));
  (*t604) = ((uint8_t)(v54));
  t605 = gg_view((v52).data, 32);
  t606 = ((int64_t)(v53));
  t607 = ((int64_t)(((int64_t)UINT64_C(16))));
  t608 = ((int64_t)((uint64_t)(t606) + (uint64_t)(t607)));
  t609 = ((uint8_t *)t605.data) + gg_index(t608, t605.len);
  (*t609) = ((uint8_t)(v54));
  }
label46:;
  t597++; goto label45;
label47:;
  t611 = ((int64_t)UINT64_C(4));
  t610 = 0;
label48:;
  if (t610 >= t611) goto label50;
  v55 = t610;
  {
  t612 = v1;
  t613 = ((int64_t)(v2));
  t614 = ((int64_t)(((int64_t)UINT64_C(8))));
  t615 = ((int64_t)(((int64_t)((uint64_t)(t613) + (uint64_t)(t614)))));
  t616 = ((int64_t)(v55));
  t617 = ((int64_t)(((int64_t)UINT64_C(32))));
  t618 = ((int64_t)(((int64_t)((uint64_t)(t616) * (uint64_t)(t617)))));
  t619 = ((int64_t)(((int64_t)((uint64_t)(t615) + (uint64_t)(t618)))));
  v56 = gg_function_14(t612, t619, base + 0);
  t620 = v52;
  t621 = v56;
  t622 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t623 = gg_function_15(t621, t622, base + 0);
  v57 = gg_function_17(t620, t623, base + 0);
  t624 = v52;
  t625 = v56;
  t626 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t627 = gg_function_16(t625, t626, base + 0);
  v58 = gg_function_17(t624, t627, base + 0);
  t629 = ((int64_t)UINT64_C(16));
  t628 = 0;
label51:;
  if (t628 >= t629) goto label53;
  v59 = t628;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t630 = v3;
  t631 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t630, ((int64_t)UINT64_C(256)));
  t632 = gg_view(((*t631)).data, 8);
  t633 = ((int64_t)(((int64_t)UINT64_C(2))));
  t634 = ((int64_t)(v55));
  t635 = ((int64_t)((uint64_t)(t633) * (uint64_t)(t634)));
  t636 = ((gg_type_18 *)t632.data) + gg_index(t635, t632.len);
  t637 = gg_view(((*t636)).data, 32);
  t638 = v59;
  t639 = ((uint8_t *)t637.data) + gg_index(t638, t637.len);
  /* FG_DIRECT_SLICE_INDEX */
  t640 = v59;
  t641 = ((uint8_t *)(v57).data) + gg_index(t640, ((int64_t)UINT64_C(32)));
  (*t639) = ((uint8_t)((*t641)));
  /* FG_DIRECT_SLICE_INDEX */
  t642 = v3;
  t643 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t642, ((int64_t)UINT64_C(256)));
  t644 = gg_view(((*t643)).data, 8);
  t645 = ((int64_t)(((int64_t)UINT64_C(2))));
  t646 = ((int64_t)(v55));
  t647 = ((int64_t)((uint64_t)(t645) * (uint64_t)(t646)));
  t648 = ((gg_type_18 *)t644.data) + gg_index(t647, t644.len);
  t649 = gg_view(((*t648)).data, 32);
  t650 = ((int64_t)(v59));
  t651 = ((int64_t)(((int64_t)UINT64_C(16))));
  t652 = ((int64_t)((uint64_t)(t650) + (uint64_t)(t651)));
  t653 = ((uint8_t *)t649.data) + gg_index(t652, t649.len);
  /* FG_DIRECT_SLICE_INDEX */
  t654 = v59;
  t655 = ((uint8_t *)(v58).data) + gg_index(t654, ((int64_t)UINT64_C(32)));
  (*t653) = ((uint8_t)((*t655)));
  /* FG_DIRECT_SLICE_INDEX */
  t656 = v3;
  t657 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t656, ((int64_t)UINT64_C(256)));
  t658 = gg_view(((*t657)).data, 8);
  t659 = ((int64_t)(((int64_t)UINT64_C(2))));
  t660 = ((int64_t)(v55));
  t661 = ((int64_t)(((int64_t)((uint64_t)(t659) * (uint64_t)(t660)))));
  t662 = ((int64_t)(((int64_t)UINT64_C(1))));
  t663 = ((int64_t)((uint64_t)(t661) + (uint64_t)(t662)));
  t664 = ((gg_type_18 *)t658.data) + gg_index(t663, t658.len);
  t665 = gg_view(((*t664)).data, 32);
  t666 = v59;
  t667 = ((uint8_t *)t665.data) + gg_index(t666, t665.len);
  t668 = gg_view((v57).data, 32);
  t669 = ((int64_t)(v59));
  t670 = ((int64_t)(((int64_t)UINT64_C(16))));
  t671 = ((int64_t)((uint64_t)(t669) + (uint64_t)(t670)));
  t672 = ((uint8_t *)t668.data) + gg_index(t671, t668.len);
  (*t667) = ((uint8_t)((*t672)));
  /* FG_DIRECT_SLICE_INDEX */
  t673 = v3;
  t674 = ((gg_type_19 *)(((*(v5))).f49).data) + gg_index(t673, ((int64_t)UINT64_C(256)));
  t675 = gg_view(((*t674)).data, 8);
  t676 = ((int64_t)(((int64_t)UINT64_C(2))));
  t677 = ((int64_t)(v55));
  t678 = ((int64_t)(((int64_t)((uint64_t)(t676) * (uint64_t)(t677)))));
  t679 = ((int64_t)(((int64_t)UINT64_C(1))));
  t680 = ((int64_t)((uint64_t)(t678) + (uint64_t)(t679)));
  t681 = ((gg_type_18 *)t675.data) + gg_index(t680, t675.len);
  t682 = gg_view(((*t681)).data, 32);
  t683 = ((int64_t)(v59));
  t684 = ((int64_t)(((int64_t)UINT64_C(16))));
  t685 = ((int64_t)((uint64_t)(t683) + (uint64_t)(t684)));
  t686 = ((uint8_t *)t682.data) + gg_index(t685, t682.len);
  t687 = gg_view((v58).data, 32);
  t688 = ((int64_t)(v59));
  t689 = ((int64_t)(((int64_t)UINT64_C(16))));
  t690 = ((int64_t)((uint64_t)(t688) + (uint64_t)(t689)));
  t691 = ((uint8_t *)t687.data) + gg_index(t690, t687.len);
  (*t686) = ((uint8_t)((*t691)));
  }
label52:;
  t628++; goto label51;
label53:;
  }
label49:;
  t610++; goto label48;
label50:;
  }
  } else {
  t692 = ((int64_t)(v0));
  t693 = ((int64_t)(((int64_t)UINT64_C(6))));
  t694 = ((t692) == (t693));
  if (t694) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t695 = v4;
  t696 = ((float *)(((*(v5))).f50).data) + gg_index(t695, ((int64_t)UINT64_C(256)));
  t697 = v1;
  t698 = ((int64_t)(v2));
  t699 = gg_function_11(t697, t698, base + 0);
  t700 = fg141_half(t699);
  t701 = ((float)(t700));
  t702 = ((float)(((float)(gg_double_bits(UINT64_C(4593671619917905920))))));
  (*t696) = ((float)(((t701) * (t702))));
  t704 = ((int64_t)UINT64_C(8));
  t703 = 0;
label54:;
  if (t703 >= t704) goto label56;
  v60 = t703;
  {
  t705 = v1;
  t706 = ((int64_t)(v2));
  t707 = ((int64_t)(((int64_t)UINT64_C(2))));
  t708 = ((int64_t)(((int64_t)((uint64_t)(t706) + (uint64_t)(t707)))));
  t709 = ((int64_t)(v60));
  t710 = ((int64_t)(((int64_t)UINT64_C(8))));
  t711 = ((int64_t)(((int64_t)((uint64_t)(t709) * (uint64_t)(t710)))));
  t712 = ((int64_t)(((int64_t)((uint64_t)(t708) + (uint64_t)(t711)))));
  t713 = ((int64_t)(((int64_t)UINT64_C(4))));
  t714 = ((int64_t)(((int64_t)((uint64_t)(t712) + (uint64_t)(t713)))));
  v61 = gg_function_18(t705, t714, base + 0);
  t715 = v5;
  t716 = ((int64_t)(v16));
  t717 = ((int64_t)(v60));
  t718 = ((int64_t)(((int64_t)((uint64_t)(t716) + (uint64_t)(t717)))));
  t719 = ((uint32_t)(((int64_t)UINT64_C(2))));
  t720 = ((uint32_t)(v61));
  t721 = ((int64_t)(((int64_t)UINT64_C(28))));
  t722 = ((uint32_t)(((t720) >> ((uint64_t)(t721) & 31))));
  t723 = ((uint32_t)(((uint32_t)((uint64_t)(t719) * (uint64_t)(t722)))));
  t724 = ((uint32_t)(((int64_t)UINT64_C(1))));
  t725 = ((int32_t)(((int32_t)(((uint32_t)((uint64_t)(t723) + (uint64_t)(t724)))))));
  gg_function_13(t715, t718, t725, base + 0);
  t727 = ((int64_t)UINT64_C(4));
  t726 = 0;
label57:;
  if (t726 >= t727) goto label59;
  v62 = t726;
  {
  t728 = v5;
  t729 = ((int64_t)(v3));
  t730 = ((int64_t)(v60));
  t731 = ((int64_t)(((int64_t)UINT64_C(32))));
  t732 = ((int64_t)(((int64_t)((uint64_t)(t730) * (uint64_t)(t731)))));
  t733 = ((int64_t)(v62));
  t734 = ((int64_t)(((int64_t)UINT64_C(8))));
  t735 = ((int64_t)(((int64_t)((uint64_t)(t733) * (uint64_t)(t734)))));
  t736 = ((int64_t)(((int64_t)((uint64_t)(t732) + (uint64_t)(t735)))));
  t737 = v6;
  t738 = v1;
  t739 = ((int64_t)(v2));
  t740 = ((int64_t)(((int64_t)UINT64_C(2))));
  t741 = ((int64_t)(((int64_t)((uint64_t)(t739) + (uint64_t)(t740)))));
  t742 = ((int64_t)(v60));
  t743 = ((int64_t)(((int64_t)UINT64_C(8))));
  t744 = ((int64_t)(((int64_t)((uint64_t)(t742) * (uint64_t)(t743)))));
  t745 = ((int64_t)(((int64_t)((uint64_t)(t741) + (uint64_t)(t744)))));
  t746 = ((int64_t)(v62));
  t747 = ((int64_t)((uint64_t)(t745) + (uint64_t)(t746)));
  t748 = ((uint8_t *)t738.data) + gg_index(t747, t738.len);
  t749 = ((int64_t)((*t748)));
  t750 = ((gg_type_28 *)t737.data) + gg_index(t749, t737.len);
  t751 = (*t750);
  t752 = v12;
  t753 = ((uint32_t)(v61));
  t754 = ((int64_t)(v62));
  t755 = ((int64_t)(((int64_t)UINT64_C(7))));
  t756 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t754) * (uint64_t)(t755)))))));
  t757 = ((uint32_t)(((t753) >> ((uint64_t)(t756) & 31))));
  t758 = ((uint32_t)(((int64_t)UINT64_C(127))));
  t759 = ((int64_t)(((t757) & (t758))));
  t760 = ((uint8_t *)t752.data) + gg_index(t759, t752.len);
  t761 = ((uint8_t)((*t760)));
  gg_function_19(t728, t729, t736, t751, t761, base + 0);
  }
label58:;
  t726++; goto label57;
label59:;
  }
label55:;
  t703++; goto label54;
label56:;
  }
  } else {
  t762 = ((int64_t)(v0));
  t763 = ((int64_t)(((int64_t)UINT64_C(7))));
  t764 = ((t762) == (t763));
  t765 = t764;
  if (!t765) {
  t766 = ((int64_t)(v0));
  t767 = ((int64_t)(((int64_t)UINT64_C(8))));
  t765 = ((t766) == (t767));
  }
  t768 = t765;
  if (t768) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t769 = v4;
  t770 = ((float *)(((*(v5))).f50).data) + gg_index(t769, ((int64_t)UINT64_C(256)));
  t771 = v1;
  t772 = ((int64_t)(v2));
  t773 = gg_function_11(t771, t772, base + 0);
  t774 = fg141_half(t773);
  t775 = ((float)(t774));
  t776 = ((float)(((float)(gg_double_bits(UINT64_C(4593671619917905920))))));
  (*t770) = ((float)(((t775) * (t776))));
  t777 = ((int64_t)(v2));
  t779 = ((int64_t)(v0));
  t780 = ((int64_t)(((int64_t)UINT64_C(7))));
  t781 = ((t779) == (t780));
  if (t781) {
  {
  t778 = ((int64_t)(((int64_t)UINT64_C(66))));
  }
  } else {
  {
  t778 = ((int64_t)(((int64_t)UINT64_C(74))));
  }
  }
  t782 = ((int64_t)(t778));
  v63 = ((int64_t)(((int64_t)((uint64_t)(t777) + (uint64_t)(t782)))));
  t784 = ((int64_t)UINT64_C(8));
  t783 = 0;
label60:;
  if (t783 >= t784) goto label62;
  v64 = t783;
  {
  t785 = v5;
  t786 = ((int64_t)(v16));
  t787 = ((int64_t)(((int64_t)UINT64_C(2))));
  t788 = ((int64_t)(v64));
  t789 = ((int64_t)(((int64_t)((uint64_t)(t787) * (uint64_t)(t788)))));
  t790 = ((int64_t)(((int64_t)((uint64_t)(t786) + (uint64_t)(t789)))));
  t791 = ((uint8_t)(((int64_t)UINT64_C(2))));
  t792 = v1;
  t793 = ((int64_t)(v63));
  t794 = ((int64_t)(v64));
  t795 = ((int64_t)((uint64_t)(t793) + (uint64_t)(t794)));
  t796 = ((uint8_t *)t792.data) + gg_index(t795, t792.len);
  t797 = ((uint8_t)((*t796)));
  t798 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t799 = ((uint8_t)(((t797) & (t798))));
  t800 = ((uint8_t)(((uint8_t)((uint64_t)(t791) * (uint64_t)(t799)))));
  t801 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t802 = ((int32_t)(((int32_t)(((uint8_t)((uint64_t)(t800) + (uint64_t)(t801)))))));
  gg_function_13(t785, t790, t802, base + 0);
  t803 = v5;
  t804 = ((int64_t)(v16));
  t805 = ((int64_t)(((int64_t)UINT64_C(2))));
  t806 = ((int64_t)(v64));
  t807 = ((int64_t)(((int64_t)((uint64_t)(t805) * (uint64_t)(t806)))));
  t808 = ((int64_t)(((int64_t)((uint64_t)(t804) + (uint64_t)(t807)))));
  t809 = ((int64_t)(((int64_t)UINT64_C(1))));
  t810 = ((int64_t)(((int64_t)((uint64_t)(t808) + (uint64_t)(t809)))));
  t811 = ((uint8_t)(((int64_t)UINT64_C(2))));
  t812 = v1;
  t813 = ((int64_t)(v63));
  t814 = ((int64_t)(v64));
  t815 = ((int64_t)((uint64_t)(t813) + (uint64_t)(t814)));
  t816 = ((uint8_t *)t812.data) + gg_index(t815, t812.len);
  t817 = ((uint8_t)((*t816)));
  t818 = ((int64_t)(((int64_t)UINT64_C(4))));
  t819 = ((uint8_t)(((t817) >> ((uint64_t)(t818) & 7))));
  t820 = ((uint8_t)(((uint8_t)((uint64_t)(t811) * (uint64_t)(t819)))));
  t821 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t822 = ((int32_t)(((int32_t)(((uint8_t)((uint64_t)(t820) + (uint64_t)(t821)))))));
  gg_function_13(t803, t810, t822, base + 0);
  t824 = ((int64_t)UINT64_C(4));
  t823 = 0;
label63:;
  if (t823 >= t824) goto label65;
  v65 = t823;
  {
  t825 = ((int64_t)(v0));
  t826 = ((int64_t)(((int64_t)UINT64_C(7))));
  t827 = ((t825) == (t826));
  if (t827) {
  {
  t828 = v1;
  t829 = ((int64_t)(v2));
  t830 = ((int64_t)(((int64_t)UINT64_C(2))));
  t831 = ((int64_t)(((int64_t)((uint64_t)(t829) + (uint64_t)(t830)))));
  t832 = ((int64_t)(v64));
  t833 = ((int64_t)(((int64_t)UINT64_C(4))));
  t834 = ((int64_t)(((int64_t)((uint64_t)(t832) * (uint64_t)(t833)))));
  t835 = ((int64_t)(v65));
  t836 = ((int64_t)(((int64_t)((uint64_t)(t834) + (uint64_t)(t835)))));
  t837 = ((int64_t)(((int64_t)UINT64_C(2))));
  t838 = ((int64_t)(((int64_t)((uint64_t)(t836) * (uint64_t)(t837)))));
  t839 = ((int64_t)(((int64_t)((uint64_t)(t831) + (uint64_t)(t838)))));
  v66 = gg_function_11(t828, t839, base + 0);
  t840 = v5;
  t841 = ((int64_t)(v3));
  t842 = ((int64_t)(v64));
  t843 = ((int64_t)(((int64_t)UINT64_C(32))));
  t844 = ((int64_t)(((int64_t)((uint64_t)(t842) * (uint64_t)(t843)))));
  t845 = ((int64_t)(v65));
  t846 = ((int64_t)(((int64_t)UINT64_C(8))));
  t847 = ((int64_t)(((int64_t)((uint64_t)(t845) * (uint64_t)(t846)))));
  t848 = ((int64_t)(((int64_t)((uint64_t)(t844) + (uint64_t)(t847)))));
  t849 = v7;
  t850 = ((uint16_t)(v66));
  t851 = ((uint16_t)(((int64_t)UINT64_C(511))));
  t852 = ((int64_t)(((t850) & (t851))));
  t853 = ((gg_type_28 *)t849.data) + gg_index(t852, t849.len);
  t854 = (*t853);
  t855 = v12;
  t856 = ((uint16_t)(v66));
  t857 = ((int64_t)(((int64_t)UINT64_C(9))));
  t858 = ((int64_t)(((t856) >> ((uint64_t)(t857) & 15))));
  t859 = ((uint8_t *)t855.data) + gg_index(t858, t855.len);
  t860 = ((uint8_t)((*t859)));
  gg_function_19(t840, t841, t848, t854, t860, base + 0);
  }
  } else {
  {
  t861 = v1;
  t862 = ((int64_t)(v2));
  t863 = ((int64_t)(((int64_t)UINT64_C(2))));
  t864 = ((int64_t)(((int64_t)((uint64_t)(t862) + (uint64_t)(t863)))));
  t865 = ((int64_t)(v64));
  t866 = ((int64_t)(((int64_t)UINT64_C(4))));
  t867 = ((int64_t)(((int64_t)((uint64_t)(t865) * (uint64_t)(t866)))));
  t868 = ((int64_t)(((int64_t)((uint64_t)(t864) + (uint64_t)(t867)))));
  t869 = ((int64_t)(v65));
  t870 = ((int64_t)((uint64_t)(t868) + (uint64_t)(t869)));
  t871 = ((uint8_t *)t861.data) + gg_index(t870, t861.len);
  t872 = ((uint16_t)(((uint16_t)((*t871)))));
  t873 = v1;
  t874 = ((int64_t)(v2));
  t875 = ((int64_t)(((int64_t)UINT64_C(66))));
  t876 = ((int64_t)(((int64_t)((uint64_t)(t874) + (uint64_t)(t875)))));
  t877 = ((int64_t)(v64));
  t878 = ((int64_t)((uint64_t)(t876) + (uint64_t)(t877)));
  t879 = ((uint8_t *)t873.data) + gg_index(t878, t873.len);
  t880 = ((uint16_t)(((uint16_t)((*t879)))));
  t881 = ((int64_t)(((int64_t)UINT64_C(8))));
  t882 = ((int64_t)(((int64_t)UINT64_C(2))));
  t883 = ((int64_t)(v65));
  t884 = ((int64_t)(((int64_t)((uint64_t)(t882) * (uint64_t)(t883)))));
  t885 = ((uint16_t)(((uint16_t)(((int64_t)((uint64_t)(t881) - (uint64_t)(t884)))))));
  t886 = ((uint16_t)(((uint16_t)((uint64_t)(t880) << ((uint64_t)(t885) & 15)))));
  t887 = ((uint16_t)(((int64_t)UINT64_C(768))));
  t888 = ((uint16_t)(((t886) & (t887))));
  v67 = ((uint16_t)(((t872) | (t888))));
  t889 = v5;
  t890 = ((int64_t)(v3));
  t891 = ((int64_t)(v64));
  t892 = ((int64_t)(((int64_t)UINT64_C(32))));
  t893 = ((int64_t)(((int64_t)((uint64_t)(t891) * (uint64_t)(t892)))));
  t894 = ((int64_t)(v65));
  t895 = ((int64_t)(((int64_t)UINT64_C(8))));
  t896 = ((int64_t)(((int64_t)((uint64_t)(t894) * (uint64_t)(t895)))));
  t897 = ((int64_t)(((int64_t)((uint64_t)(t893) + (uint64_t)(t896)))));
  t898 = v8;
  t899 = ((int64_t)(v67));
  t900 = ((gg_type_28 *)t898.data) + gg_index(t899, t898.len);
  t901 = (*t900);
  t902 = v1;
  t903 = ((int64_t)(v2));
  t904 = ((int64_t)(((int64_t)UINT64_C(34))));
  t905 = ((int64_t)(((int64_t)((uint64_t)(t903) + (uint64_t)(t904)))));
  t906 = ((int64_t)(v64));
  t907 = ((int64_t)(((int64_t)UINT64_C(4))));
  t908 = ((int64_t)(((int64_t)((uint64_t)(t906) * (uint64_t)(t907)))));
  t909 = ((int64_t)(((int64_t)((uint64_t)(t905) + (uint64_t)(t908)))));
  t910 = ((int64_t)(v65));
  t911 = ((int64_t)((uint64_t)(t909) + (uint64_t)(t910)));
  t912 = ((uint8_t *)t902.data) + gg_index(t911, t902.len);
  t913 = ((uint8_t)((*t912)));
  gg_function_19(t889, t890, t897, t901, t913, base + 0);
  }
  }
  }
label64:;
  t823++; goto label63;
label65:;
  }
label61:;
  t783++; goto label60;
label62:;
  }
  } else {
  t914 = ((int64_t)(v0));
  t915 = ((int64_t)(((int64_t)UINT64_C(9))));
  t916 = ((t914) == (t915));
  if (t916) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t917 = v4;
  t918 = ((float *)(((*(v5))).f50).data) + gg_index(t917, ((int64_t)UINT64_C(256)));
  t919 = v1;
  t920 = ((int64_t)(v2));
  t921 = gg_function_11(t919, t920, base + 0);
  t922 = fg141_half(t921);
  t923 = ((float)(t922));
  t924 = ((float)(((float)(gg_double_bits(UINT64_C(4598175219545276416))))));
  (*t918) = ((float)(((t923) * (t924))));
  t926 = ((int64_t)UINT64_C(8));
  t925 = 0;
label66:;
  if (t925 >= t926) goto label68;
  v68 = t925;
  {
  t927 = v1;
  t928 = ((int64_t)(v2));
  t929 = ((int64_t)(((int64_t)UINT64_C(66))));
  t930 = ((int64_t)(((int64_t)((uint64_t)(t928) + (uint64_t)(t929)))));
  t931 = ((int64_t)(v68));
  t932 = ((int64_t)(((int64_t)UINT64_C(4))));
  t933 = ((int64_t)(((int64_t)((uint64_t)(t931) * (uint64_t)(t932)))));
  t934 = ((int64_t)(((int64_t)((uint64_t)(t930) + (uint64_t)(t933)))));
  v69 = gg_function_18(t927, t934, base + 0);
  t935 = v5;
  t936 = ((int64_t)(v16));
  t937 = ((int64_t)(v68));
  t938 = ((int64_t)(((int64_t)((uint64_t)(t936) + (uint64_t)(t937)))));
  t939 = ((uint32_t)(((int64_t)UINT64_C(2))));
  t940 = ((uint32_t)(v69));
  t941 = ((int64_t)(((int64_t)UINT64_C(28))));
  t942 = ((uint32_t)(((t940) >> ((uint64_t)(t941) & 31))));
  t943 = ((uint32_t)(((uint32_t)((uint64_t)(t939) * (uint64_t)(t942)))));
  t944 = ((uint32_t)(((int64_t)UINT64_C(1))));
  t945 = ((int32_t)(((int32_t)(((uint32_t)((uint64_t)(t943) + (uint64_t)(t944)))))));
  gg_function_13(t935, t938, t945, base + 0);
  t947 = ((int64_t)UINT64_C(4));
  t946 = 0;
label69:;
  if (t946 >= t947) goto label71;
  v70 = t946;
  {
  t948 = v9;
  t949 = v1;
  t950 = ((int64_t)(v2));
  t951 = ((int64_t)(((int64_t)UINT64_C(2))));
  t952 = ((int64_t)(((int64_t)((uint64_t)(t950) + (uint64_t)(t951)))));
  t953 = ((int64_t)(v68));
  t954 = ((int64_t)(((int64_t)UINT64_C(8))));
  t955 = ((int64_t)(((int64_t)((uint64_t)(t953) * (uint64_t)(t954)))));
  t956 = ((int64_t)(((int64_t)((uint64_t)(t952) + (uint64_t)(t955)))));
  t957 = ((int64_t)(v70));
  t958 = ((int64_t)(((int64_t)UINT64_C(2))));
  t959 = ((int64_t)(((int64_t)((uint64_t)(t957) * (uint64_t)(t958)))));
  t960 = ((int64_t)((uint64_t)(t956) + (uint64_t)(t959)));
  t961 = ((uint8_t *)t949.data) + gg_index(t960, t949.len);
  t962 = ((int64_t)((*t961)));
  t963 = ((gg_type_30 *)t948.data) + gg_index(t962, t948.len);
  v71 = (*t963);
  t964 = v9;
  t965 = v1;
  t966 = ((int64_t)(v2));
  t967 = ((int64_t)(((int64_t)UINT64_C(2))));
  t968 = ((int64_t)(((int64_t)((uint64_t)(t966) + (uint64_t)(t967)))));
  t969 = ((int64_t)(v68));
  t970 = ((int64_t)(((int64_t)UINT64_C(8))));
  t971 = ((int64_t)(((int64_t)((uint64_t)(t969) * (uint64_t)(t970)))));
  t972 = ((int64_t)(((int64_t)((uint64_t)(t968) + (uint64_t)(t971)))));
  t973 = ((int64_t)(v70));
  t974 = ((int64_t)(((int64_t)UINT64_C(2))));
  t975 = ((int64_t)(((int64_t)((uint64_t)(t973) * (uint64_t)(t974)))));
  t976 = ((int64_t)(((int64_t)((uint64_t)(t972) + (uint64_t)(t975)))));
  t977 = ((int64_t)(((int64_t)UINT64_C(1))));
  t978 = ((int64_t)((uint64_t)(t976) + (uint64_t)(t977)));
  t979 = ((uint8_t *)t965.data) + gg_index(t978, t965.len);
  t980 = ((int64_t)((*t979)));
  t981 = ((gg_type_30 *)t964.data) + gg_index(t980, t964.len);
  v72 = (*t981);
  memset(&t982, 0, sizeof(t982));
  t983 = ((uint8_t)(t982));
  for (t984=0; t984<8; t984++) (v73).data[t984] = t983;
  t986 = ((int64_t)UINT64_C(4));
  t985 = 0;
label72:;
  if (t985 >= t986) goto label74;
  v74 = t985;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t987 = v74;
  t988 = ((uint8_t *)(v73).data) + gg_index(t987, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t989 = v74;
  t990 = ((uint8_t *)(v71).data) + gg_index(t989, ((int64_t)UINT64_C(4)));
  (*t988) = ((uint8_t)((*t990)));
  t991 = gg_view((v73).data, 8);
  t992 = ((int64_t)(v74));
  t993 = ((int64_t)(((int64_t)UINT64_C(4))));
  t994 = ((int64_t)((uint64_t)(t992) + (uint64_t)(t993)));
  t995 = ((uint8_t *)t991.data) + gg_index(t994, t991.len);
  /* FG_DIRECT_SLICE_INDEX */
  t996 = v74;
  t997 = ((uint8_t *)(v72).data) + gg_index(t996, ((int64_t)UINT64_C(4)));
  (*t995) = ((uint8_t)((*t997)));
  }
label73:;
  t985++; goto label72;
label74:;
  t998 = v5;
  t999 = ((int64_t)(v3));
  t1000 = ((int64_t)(v68));
  t1001 = ((int64_t)(((int64_t)UINT64_C(32))));
  t1002 = ((int64_t)(((int64_t)((uint64_t)(t1000) * (uint64_t)(t1001)))));
  t1003 = ((int64_t)(v70));
  t1004 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1005 = ((int64_t)(((int64_t)((uint64_t)(t1003) * (uint64_t)(t1004)))));
  t1006 = ((int64_t)(((int64_t)((uint64_t)(t1002) + (uint64_t)(t1005)))));
  t1007 = v73;
  t1008 = v12;
  t1009 = ((uint32_t)(v69));
  t1010 = ((int64_t)(v70));
  t1011 = ((int64_t)(((int64_t)UINT64_C(7))));
  t1012 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t1010) * (uint64_t)(t1011)))))));
  t1013 = ((uint32_t)(((t1009) >> ((uint64_t)(t1012) & 31))));
  t1014 = ((uint32_t)(((int64_t)UINT64_C(127))));
  t1015 = ((int64_t)(((t1013) & (t1014))));
  t1016 = ((uint8_t *)t1008.data) + gg_index(t1015, t1008.len);
  t1017 = ((uint8_t)((*t1016)));
  gg_function_19(t998, t999, t1006, t1007, t1017, base + 0);
  }
label70:;
  t946++; goto label69;
label71:;
  }
label67:;
  t925++; goto label66;
label68:;
  }
  } else {
  t1018 = ((int64_t)(v0));
  t1019 = ((int64_t)(((int64_t)UINT64_C(10))));
  t1020 = ((t1018) == (t1019));
  if (t1020) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1021 = v4;
  t1022 = ((float *)(((*(v5))).f50).data) + gg_index(t1021, ((int64_t)UINT64_C(256)));
  t1023 = v1;
  t1024 = ((int64_t)(v2));
  t1025 = gg_function_11(t1023, t1024, base + 0);
  t1026 = fg141_half(t1025);
  (*t1022) = ((float)(t1026));
  t1028 = ((int64_t)UINT64_C(8));
  t1027 = 0;
label75:;
  if (t1027 >= t1028) goto label77;
  v75 = t1027;
  {
  t1029 = v1;
  t1030 = ((int64_t)(v2));
  t1031 = ((int64_t)(((int64_t)UINT64_C(106))));
  t1032 = ((int64_t)(((int64_t)((uint64_t)(t1030) + (uint64_t)(t1031)))));
  t1033 = ((int64_t)(v75));
  t1034 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1035 = ((int64_t)(((int64_t)gg_div(t1033, t1034))));
  t1036 = ((int64_t)((uint64_t)(t1032) + (uint64_t)(t1035)));
  t1037 = ((uint8_t *)t1029.data) + gg_index(t1036, t1029.len);
  v76 = ((uint8_t)((*t1037)));
  t1038 = v5;
  t1039 = ((int64_t)(v16));
  t1040 = ((int64_t)(v75));
  t1041 = ((int64_t)(((int64_t)((uint64_t)(t1039) + (uint64_t)(t1040)))));
  t1042 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t1043 = ((uint8_t)(((int64_t)UINT64_C(2))));
  t1044 = ((uint8_t)(v76));
  t1045 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1046 = ((int64_t)(v75));
  t1047 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1048 = ((int64_t)(((int64_t)gg_mod(t1046, t1047))));
  t1049 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t1045) * (uint64_t)(t1048)))))));
  t1050 = ((uint8_t)(((t1044) >> ((uint64_t)(t1049) & 7))));
  t1051 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t1052 = ((uint8_t)(((t1050) & (t1051))));
  t1053 = ((uint8_t)(((uint8_t)((uint64_t)(t1043) * (uint64_t)(t1052)))));
  t1054 = ((int32_t)(((int32_t)(((uint8_t)((uint64_t)(t1042) + (uint64_t)(t1053)))))));
  gg_function_13(t1038, t1041, t1054, base + 0);
  t1056 = ((int64_t)UINT64_C(4));
  t1055 = 0;
label78:;
  if (t1055 >= t1056) goto label80;
  v77 = t1055;
  {
  t1057 = v1;
  t1058 = ((int64_t)(v2));
  t1059 = ((int64_t)(((int64_t)UINT64_C(66))));
  t1060 = ((int64_t)(((int64_t)((uint64_t)(t1058) + (uint64_t)(t1059)))));
  t1061 = ((int64_t)(v75));
  t1062 = ((int64_t)((uint64_t)(t1060) + (uint64_t)(t1061)));
  t1063 = ((uint8_t *)t1057.data) + gg_index(t1062, t1057.len);
  v78 = ((uint16_t)(((uint16_t)((*t1063)))));
  t1064 = v1;
  t1065 = ((int64_t)(v2));
  t1066 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1067 = ((int64_t)(((int64_t)((uint64_t)(t1065) + (uint64_t)(t1066)))));
  t1068 = ((int64_t)(v75));
  t1069 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1070 = ((int64_t)(((int64_t)((uint64_t)(t1068) * (uint64_t)(t1069)))));
  t1071 = ((int64_t)(((int64_t)((uint64_t)(t1067) + (uint64_t)(t1070)))));
  t1072 = ((int64_t)(v77));
  t1073 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1074 = ((int64_t)(((int64_t)((uint64_t)(t1072) * (uint64_t)(t1073)))));
  t1075 = ((int64_t)((uint64_t)(t1071) + (uint64_t)(t1074)));
  t1076 = ((uint8_t *)t1064.data) + gg_index(t1075, t1064.len);
  t1077 = ((uint16_t)(((uint16_t)((*t1076)))));
  t1078 = ((uint16_t)(v78));
  t1079 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1080 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1081 = ((int64_t)(v77));
  t1082 = ((int64_t)(((int64_t)((uint64_t)(t1080) * (uint64_t)(t1081)))));
  t1083 = ((uint16_t)(((uint16_t)(((int64_t)((uint64_t)(t1079) - (uint64_t)(t1082)))))));
  t1084 = ((uint16_t)(((uint16_t)((uint64_t)(t1078) << ((uint64_t)(t1083) & 15)))));
  t1085 = ((uint16_t)(((int64_t)UINT64_C(256))));
  t1086 = ((uint16_t)(((t1084) & (t1085))));
  v79 = ((uint16_t)(((t1077) | (t1086))));
  t1087 = v1;
  t1088 = ((int64_t)(v2));
  t1089 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1090 = ((int64_t)(((int64_t)((uint64_t)(t1088) + (uint64_t)(t1089)))));
  t1091 = ((int64_t)(v75));
  t1092 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1093 = ((int64_t)(((int64_t)((uint64_t)(t1091) * (uint64_t)(t1092)))));
  t1094 = ((int64_t)(((int64_t)((uint64_t)(t1090) + (uint64_t)(t1093)))));
  t1095 = ((int64_t)(v77));
  t1096 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1097 = ((int64_t)(((int64_t)((uint64_t)(t1095) * (uint64_t)(t1096)))));
  t1098 = ((int64_t)(((int64_t)((uint64_t)(t1094) + (uint64_t)(t1097)))));
  t1099 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1100 = ((int64_t)((uint64_t)(t1098) + (uint64_t)(t1099)));
  t1101 = ((uint8_t *)t1087.data) + gg_index(t1100, t1087.len);
  t1102 = ((uint16_t)(((uint16_t)((*t1101)))));
  t1103 = ((uint16_t)(v78));
  t1104 = ((int64_t)(((int64_t)UINT64_C(7))));
  t1105 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1106 = ((int64_t)(v77));
  t1107 = ((int64_t)(((int64_t)((uint64_t)(t1105) * (uint64_t)(t1106)))));
  t1108 = ((uint16_t)(((uint16_t)(((int64_t)((uint64_t)(t1104) - (uint64_t)(t1107)))))));
  t1109 = ((uint16_t)(((uint16_t)((uint64_t)(t1103) << ((uint64_t)(t1108) & 15)))));
  t1110 = ((uint16_t)(((int64_t)UINT64_C(256))));
  t1111 = ((uint16_t)(((t1109) & (t1110))));
  v80 = ((uint16_t)(((t1102) | (t1111))));
  t1112 = v10;
  t1113 = ((int64_t)(v79));
  t1114 = ((gg_type_30 *)t1112.data) + gg_index(t1113, t1112.len);
  v81 = (*t1114);
  t1115 = v10;
  t1116 = ((int64_t)(v80));
  t1117 = ((gg_type_30 *)t1115.data) + gg_index(t1116, t1115.len);
  v82 = (*t1117);
  memset(&t1118, 0, sizeof(t1118));
  t1119 = ((uint8_t)(t1118));
  for (t1120=0; t1120<8; t1120++) (v83).data[t1120] = t1119;
  t1122 = ((int64_t)UINT64_C(4));
  t1121 = 0;
label81:;
  if (t1121 >= t1122) goto label83;
  v84 = t1121;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1123 = v84;
  t1124 = ((uint8_t *)(v83).data) + gg_index(t1123, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t1125 = v84;
  t1126 = ((uint8_t *)(v81).data) + gg_index(t1125, ((int64_t)UINT64_C(4)));
  (*t1124) = ((uint8_t)((*t1126)));
  t1127 = gg_view((v83).data, 8);
  t1128 = ((int64_t)(v84));
  t1129 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1130 = ((int64_t)((uint64_t)(t1128) + (uint64_t)(t1129)));
  t1131 = ((uint8_t *)t1127.data) + gg_index(t1130, t1127.len);
  /* FG_DIRECT_SLICE_INDEX */
  t1132 = v84;
  t1133 = ((uint8_t *)(v82).data) + gg_index(t1132, ((int64_t)UINT64_C(4)));
  (*t1131) = ((uint8_t)((*t1133)));
  }
label82:;
  t1121++; goto label81;
label83:;
  t1134 = v5;
  t1135 = ((int64_t)(v3));
  t1136 = ((int64_t)(v75));
  t1137 = ((int64_t)(((int64_t)UINT64_C(32))));
  t1138 = ((int64_t)(((int64_t)((uint64_t)(t1136) * (uint64_t)(t1137)))));
  t1139 = ((int64_t)(v77));
  t1140 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1141 = ((int64_t)(((int64_t)((uint64_t)(t1139) * (uint64_t)(t1140)))));
  t1142 = ((int64_t)(((int64_t)((uint64_t)(t1138) + (uint64_t)(t1141)))));
  t1143 = v83;
  t1144 = v1;
  t1145 = ((int64_t)(v2));
  t1146 = ((int64_t)(((int64_t)UINT64_C(74))));
  t1147 = ((int64_t)(((int64_t)((uint64_t)(t1145) + (uint64_t)(t1146)))));
  t1148 = ((int64_t)(v75));
  t1149 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1150 = ((int64_t)(((int64_t)((uint64_t)(t1148) * (uint64_t)(t1149)))));
  t1151 = ((int64_t)(((int64_t)((uint64_t)(t1147) + (uint64_t)(t1150)))));
  t1152 = ((int64_t)(v77));
  t1153 = ((int64_t)((uint64_t)(t1151) + (uint64_t)(t1152)));
  t1154 = ((uint8_t *)t1144.data) + gg_index(t1153, t1144.len);
  t1155 = ((uint8_t)((*t1154)));
  gg_function_19(t1134, t1135, t1142, t1143, t1155, base + 0);
  }
label79:;
  t1055++; goto label78;
label80:;
  }
label76:;
  t1027++; goto label75;
label77:;
  }
  } else {
  t1156 = ((int64_t)(v0));
  t1157 = ((int64_t)(((int64_t)UINT64_C(11))));
  t1158 = ((t1156) == (t1157));
  if (t1158) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1159 = v4;
  t1160 = ((float *)(((*(v5))).f50).data) + gg_index(t1159, ((int64_t)UINT64_C(256)));
  t1161 = v1;
  t1162 = ((int64_t)(v2));
  t1163 = gg_function_11(t1161, t1162, base + 0);
  t1164 = fg141_half(t1163);
  t1165 = ((float)(t1164));
  t1166 = ((float)(((float)(gg_double_bits(UINT64_C(4593671619917905920))))));
  (*t1160) = ((float)(((t1165) * (t1166))));
  t1168 = ((int64_t)UINT64_C(8));
  t1167 = 0;
label84:;
  if (t1167 >= t1168) goto label86;
  v85 = t1167;
  {
  t1169 = v1;
  t1170 = ((int64_t)(v2));
  t1171 = ((int64_t)(((int64_t)UINT64_C(34))));
  t1172 = ((int64_t)(((int64_t)((uint64_t)(t1170) + (uint64_t)(t1171)))));
  t1173 = ((int64_t)(v85));
  t1174 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1175 = ((int64_t)(((int64_t)((uint64_t)(t1173) * (uint64_t)(t1174)))));
  t1176 = ((int64_t)(((int64_t)((uint64_t)(t1172) + (uint64_t)(t1175)))));
  v86 = gg_function_11(t1169, t1176, base + 0);
  t1177 = v5;
  t1178 = ((int64_t)(v16));
  t1179 = ((int64_t)(v85));
  t1180 = ((int64_t)(((int64_t)((uint64_t)(t1178) + (uint64_t)(t1179)))));
  t1181 = ((uint16_t)(((int64_t)UINT64_C(2))));
  t1182 = ((uint16_t)(v86));
  t1183 = ((int64_t)(((int64_t)UINT64_C(12))));
  t1184 = ((uint16_t)(((t1182) >> ((uint64_t)(t1183) & 15))));
  t1185 = ((uint16_t)(((int64_t)UINT64_C(7))));
  t1186 = ((uint16_t)(((t1184) & (t1185))));
  t1187 = ((uint16_t)(((uint16_t)((uint64_t)(t1181) * (uint64_t)(t1186)))));
  t1188 = ((uint16_t)(((int64_t)UINT64_C(1))));
  t1189 = ((int32_t)(((int32_t)(((uint16_t)((uint64_t)(t1187) + (uint64_t)(t1188)))))));
  gg_function_13(t1177, t1180, t1189, base + 0);
  t1190 = ((uint16_t)(v86));
  t1191 = ((uint16_t)(((int64_t)UINT64_C(32768))));
  t1192 = ((uint16_t)(((t1190) & (t1191))));
  t1193 = ((uint16_t)(((int64_t)UINT64_C(0))));
  t1194 = ((t1192) != (t1193));
  if (t1194) {
  {
  t1195 = ((int16_t)(((int64_t)UINT64_C(1))));
  v87 = ((int16_t)(((int64_t)((uint64_t)0 - (uint64_t)(t1195)))));
  }
  } else {
  {
  v87 = ((int16_t)(((int16_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t1197 = ((int64_t)UINT64_C(4));
  t1196 = 0;
label87:;
  if (t1196 >= t1197) goto label89;
  v88 = t1196;
  {
  t1198 = v1;
  t1199 = ((int64_t)(v2));
  t1200 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1201 = ((int64_t)(((int64_t)((uint64_t)(t1199) + (uint64_t)(t1200)))));
  t1202 = ((int64_t)(v85));
  t1203 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1204 = ((int64_t)(((int64_t)((uint64_t)(t1202) * (uint64_t)(t1203)))));
  t1205 = ((int64_t)(((int64_t)((uint64_t)(t1201) + (uint64_t)(t1204)))));
  t1206 = ((int64_t)(v88));
  t1207 = ((int64_t)((uint64_t)(t1205) + (uint64_t)(t1206)));
  t1208 = ((uint8_t *)t1198.data) + gg_index(t1207, t1198.len);
  t1209 = ((uint16_t)(((uint16_t)((*t1208)))));
  t1210 = ((uint16_t)(v86));
  t1211 = ((int64_t)(((int64_t)UINT64_C(3))));
  t1212 = ((int64_t)(v88));
  t1213 = ((uint16_t)(((uint16_t)(((int64_t)((uint64_t)(t1211) * (uint64_t)(t1212)))))));
  t1214 = ((uint16_t)(((t1210) >> ((uint64_t)(t1213) & 15))));
  t1215 = ((uint16_t)(((int64_t)UINT64_C(7))));
  t1216 = ((uint16_t)(((t1214) & (t1215))));
  t1217 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1218 = ((uint16_t)(((uint16_t)((uint64_t)(t1216) << ((uint64_t)(t1217) & 15)))));
  v89 = ((uint16_t)(((t1209) | (t1218))));
  t1219 = v5;
  t1220 = ((int64_t)(v3));
  t1221 = ((int64_t)(v85));
  t1222 = ((int64_t)(((int64_t)UINT64_C(32))));
  t1223 = ((int64_t)(((int64_t)((uint64_t)(t1221) * (uint64_t)(t1222)))));
  t1224 = ((int64_t)(v88));
  t1225 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1226 = ((int64_t)(((int64_t)((uint64_t)(t1224) * (uint64_t)(t1225)))));
  t1227 = ((int64_t)(((int64_t)((uint64_t)(t1223) + (uint64_t)(t1226)))));
  t1228 = v11;
  t1229 = ((int64_t)(v89));
  t1230 = ((gg_type_32 *)t1228.data) + gg_index(t1229, t1228.len);
  t1231 = (*t1230);
  t1232 = ((int16_t)(v87));
  gg_function_21(t1219, t1220, t1227, t1231, t1232, base + 0);
  }
label88:;
  t1196++; goto label87;
label89:;
  }
label85:;
  t1167++; goto label84;
label86:;
  }
  } else {
  {
  t1233 = ((int64_t)(v0));
  t1234 = ((int64_t)(((int64_t)UINT64_C(12))));
  if (!(((t1233) == (t1234)))) gg_fail("assertion failed");
  t1235 = v1;
  t1236 = ((int64_t)(v2));
  t1237 = ((int64_t)(((int64_t)UINT64_C(48))));
  t1238 = ((int64_t)(((int64_t)((uint64_t)(t1236) + (uint64_t)(t1237)))));
  v90 = gg_function_11(t1235, t1238, base + 0);
  t1239 = v1;
  t1240 = ((int64_t)(v2));
  t1241 = ((int64_t)(((int64_t)UINT64_C(50))));
  t1242 = ((int64_t)(((int64_t)((uint64_t)(t1240) + (uint64_t)(t1241)))));
  v91 = gg_function_11(t1239, t1242, base + 0);
  t1243 = v1;
  t1244 = ((int64_t)(v2));
  t1245 = ((int64_t)(((int64_t)UINT64_C(52))));
  t1246 = ((int64_t)(((int64_t)((uint64_t)(t1244) + (uint64_t)(t1245)))));
  v92 = gg_function_11(t1243, t1246, base + 0);
  t1247 = v1;
  t1248 = ((int64_t)(v2));
  t1249 = ((int64_t)(((int64_t)UINT64_C(54))));
  t1250 = ((int64_t)(((int64_t)((uint64_t)(t1248) + (uint64_t)(t1249)))));
  v93 = gg_function_11(t1247, t1250, base + 0);
  t1251 = ((uint16_t)(v90));
  t1252 = ((int64_t)(((int64_t)UINT64_C(12))));
  t1253 = ((uint16_t)(((t1251) >> ((uint64_t)(t1252) & 15))));
  t1254 = ((uint16_t)(v91));
  t1255 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1256 = ((uint16_t)(((t1254) >> ((uint64_t)(t1255) & 15))));
  t1257 = ((uint16_t)(((int64_t)UINT64_C(240))));
  t1258 = ((uint16_t)(((t1256) & (t1257))));
  t1259 = ((uint16_t)(((t1253) | (t1258))));
  t1260 = ((uint16_t)(v92));
  t1261 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1262 = ((uint16_t)(((t1260) >> ((uint64_t)(t1261) & 15))));
  t1263 = ((uint16_t)(((int64_t)UINT64_C(3840))));
  t1264 = ((uint16_t)(((t1262) & (t1263))));
  t1265 = ((uint16_t)(((t1259) | (t1264))));
  t1266 = ((uint16_t)(v93));
  t1267 = ((uint16_t)(((int64_t)UINT64_C(61440))));
  t1268 = ((uint16_t)(((t1266) & (t1267))));
  v94 = ((uint16_t)(((t1265) | (t1268))));
  /* FG_DIRECT_SLICE_INDEX */
  t1269 = v4;
  t1270 = ((float *)(((*(v5))).f50).data) + gg_index(t1269, ((int64_t)UINT64_C(256)));
  t1271 = ((uint16_t)(v94));
  t1272 = fg141_half(t1271);
  t1273 = ((float)(t1272));
  t1274 = ((float)(((float)(gg_double_bits(UINT64_C(4593671619917905920))))));
  (*t1270) = ((float)(((t1273) * (t1274))));
  t1276 = ((int64_t)UINT64_C(8));
  t1275 = 0;
label90:;
  if (t1275 >= t1276) goto label92;
  v95 = t1275;
  {
  t1277 = v1;
  t1278 = ((int64_t)(v2));
  t1279 = ((int64_t)(((int64_t)UINT64_C(48))));
  t1280 = ((int64_t)(((int64_t)((uint64_t)(t1278) + (uint64_t)(t1279)))));
  t1281 = ((int64_t)(v95));
  t1282 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1283 = ((int64_t)(((int64_t)gg_div(t1281, t1282))));
  t1284 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1285 = ((int64_t)(((int64_t)((uint64_t)(t1283) * (uint64_t)(t1284)))));
  t1286 = ((int64_t)(((int64_t)((uint64_t)(t1280) + (uint64_t)(t1285)))));
  v96 = gg_function_11(t1277, t1286, base + 0);
  t1287 = ((int64_t)(((int64_t)UINT64_C(6))));
  t1288 = ((int64_t)(v95));
  t1289 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1290 = ((int64_t)(((int64_t)gg_mod(t1288, t1289))));
  v97 = ((uint16_t)(((uint16_t)(((int64_t)((uint64_t)(t1287) * (uint64_t)(t1290)))))));
  t1291 = v5;
  t1292 = ((int64_t)(v16));
  t1293 = ((int64_t)(v95));
  t1294 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1295 = ((int64_t)(((int64_t)((uint64_t)(t1293) * (uint64_t)(t1294)))));
  t1296 = ((int64_t)(((int64_t)((uint64_t)(t1292) + (uint64_t)(t1295)))));
  t1297 = ((uint16_t)(((int64_t)UINT64_C(2))));
  t1298 = ((uint16_t)(v96));
  t1299 = ((uint16_t)(v97));
  t1300 = ((uint16_t)(((t1298) >> ((uint64_t)(t1299) & 15))));
  t1301 = ((uint16_t)(((int64_t)UINT64_C(7))));
  t1302 = ((uint16_t)(((t1300) & (t1301))));
  t1303 = ((uint16_t)(((uint16_t)((uint64_t)(t1297) * (uint64_t)(t1302)))));
  t1304 = ((uint16_t)(((int64_t)UINT64_C(1))));
  t1305 = ((int32_t)(((int32_t)(((uint16_t)((uint64_t)(t1303) + (uint64_t)(t1304)))))));
  gg_function_13(t1291, t1296, t1305, base + 0);
  t1306 = v5;
  t1307 = ((int64_t)(v16));
  t1308 = ((int64_t)(v95));
  t1309 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1310 = ((int64_t)(((int64_t)((uint64_t)(t1308) * (uint64_t)(t1309)))));
  t1311 = ((int64_t)(((int64_t)((uint64_t)(t1307) + (uint64_t)(t1310)))));
  t1312 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1313 = ((int64_t)(((int64_t)((uint64_t)(t1311) + (uint64_t)(t1312)))));
  t1314 = ((uint16_t)(((int64_t)UINT64_C(2))));
  t1315 = ((uint16_t)(v96));
  t1316 = ((uint16_t)(v97));
  t1317 = ((uint16_t)(((int64_t)UINT64_C(3))));
  t1318 = ((uint16_t)(((uint16_t)((uint64_t)(t1316) + (uint64_t)(t1317)))));
  t1319 = ((uint16_t)(((t1315) >> ((uint64_t)(t1318) & 15))));
  t1320 = ((uint16_t)(((int64_t)UINT64_C(7))));
  t1321 = ((uint16_t)(((t1319) & (t1320))));
  t1322 = ((uint16_t)(((uint16_t)((uint64_t)(t1314) * (uint64_t)(t1321)))));
  t1323 = ((uint16_t)(((int64_t)UINT64_C(1))));
  t1324 = ((int32_t)(((int32_t)(((uint16_t)((uint64_t)(t1322) + (uint64_t)(t1323)))))));
  gg_function_13(t1306, t1313, t1324, base + 0);
  t1326 = ((int64_t)UINT64_C(4));
  t1325 = 0;
label93:;
  if (t1325 >= t1326) goto label95;
  v98 = t1325;
  {
  t1327 = v1;
  t1328 = ((int64_t)(v2));
  t1329 = ((int64_t)(((int64_t)UINT64_C(32))));
  t1330 = ((int64_t)(((int64_t)((uint64_t)(t1328) + (uint64_t)(t1329)))));
  t1331 = ((int64_t)(v95));
  t1332 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1333 = ((int64_t)(((int64_t)((uint64_t)(t1331) * (uint64_t)(t1332)))));
  t1334 = ((int64_t)(((int64_t)((uint64_t)(t1330) + (uint64_t)(t1333)))));
  t1335 = ((int64_t)(v98));
  t1336 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1337 = ((int64_t)(((int64_t)gg_div(t1335, t1336))));
  t1338 = ((int64_t)((uint64_t)(t1334) + (uint64_t)(t1337)));
  t1339 = ((uint8_t *)t1327.data) + gg_index(t1338, t1327.len);
  v99 = ((uint16_t)(((uint16_t)((*t1339)))));
  t1341 = ((int64_t)(v98));
  t1342 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1343 = ((int64_t)(((int64_t)gg_mod(t1341, t1342))));
  t1344 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1345 = ((t1343) == (t1344));
  if (t1345) {
  {
  t1340 = ((int64_t)(((int64_t)UINT64_C(8))));
  }
  } else {
  {
  t1340 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  }
  v100 = ((uint16_t)(((uint16_t)(t1340))));
  t1346 = v1;
  t1347 = ((int64_t)(v2));
  t1348 = ((int64_t)(v95));
  t1349 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1350 = ((int64_t)(((int64_t)((uint64_t)(t1348) * (uint64_t)(t1349)))));
  t1351 = ((int64_t)(((int64_t)((uint64_t)(t1347) + (uint64_t)(t1350)))));
  t1352 = ((int64_t)(v98));
  t1353 = ((int64_t)((uint64_t)(t1351) + (uint64_t)(t1352)));
  t1354 = ((uint8_t *)t1346.data) + gg_index(t1353, t1346.len);
  t1355 = ((uint16_t)(((uint16_t)((*t1354)))));
  t1356 = ((uint16_t)(v99));
  t1357 = ((uint16_t)(v100));
  t1358 = ((uint16_t)(((uint16_t)((uint64_t)(t1356) << ((uint64_t)(t1357) & 15)))));
  t1359 = ((uint16_t)(((int64_t)UINT64_C(1792))));
  t1360 = ((uint16_t)(((t1358) & (t1359))));
  v101 = ((uint16_t)(((t1355) | (t1360))));
  t1361 = ((uint16_t)(v99));
  t1363 = ((int64_t)(v98));
  t1364 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1365 = ((int64_t)(((int64_t)gg_mod(t1363, t1364))));
  t1366 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1367 = ((t1365) == (t1366));
  if (t1367) {
  {
  t1362 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  } else {
  {
  t1362 = ((int64_t)(((int64_t)UINT64_C(7))));
  }
  }
  t1368 = ((uint16_t)(((uint16_t)(t1362))));
  t1369 = ((uint16_t)(((t1361) >> ((uint64_t)(t1368) & 15))));
  t1370 = ((uint16_t)(((int64_t)UINT64_C(1))));
  t1371 = ((uint16_t)(((t1369) & (t1370))));
  t1372 = ((uint16_t)(((int64_t)UINT64_C(0))));
  t1373 = ((t1371) != (t1372));
  if (t1373) {
  {
  t1374 = ((int16_t)(((int64_t)UINT64_C(1))));
  v102 = ((int16_t)(((int64_t)((uint64_t)0 - (uint64_t)(t1374)))));
  }
  } else {
  {
  v102 = ((int16_t)(((int16_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t1375 = v5;
  t1376 = ((int64_t)(v3));
  t1377 = ((int64_t)(v95));
  t1378 = ((int64_t)(((int64_t)UINT64_C(32))));
  t1379 = ((int64_t)(((int64_t)((uint64_t)(t1377) * (uint64_t)(t1378)))));
  t1380 = ((int64_t)(v98));
  t1381 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1382 = ((int64_t)(((int64_t)((uint64_t)(t1380) * (uint64_t)(t1381)))));
  t1383 = ((int64_t)(((int64_t)((uint64_t)(t1379) + (uint64_t)(t1382)))));
  t1384 = v11;
  t1385 = ((int64_t)(v101));
  t1386 = ((gg_type_32 *)t1384.data) + gg_index(t1385, t1384.len);
  t1387 = (*t1386);
  t1388 = ((int16_t)(v102));
  gg_function_21(t1375, t1376, t1383, t1387, t1388, base + 0);
  }
label94:;
  t1325++; goto label93;
label95:;
  }
label91:;
  t1275++; goto label90;
label92:;
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_64(gg_type_37 p0, gg_slice p1, gg_type_41 p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  int16_t t43;
  int8_t t44;
  int8_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int8_t * t50;
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
  uint8_t t67;
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
  gg_type_50 t85;
  gg_slice t86;
  gg_slice t87;
  gg_slice t88;
  gg_slice t89;
  gg_slice t90;
  gg_slice t91;
  gg_slice t92;
  gg_slice t93;
  int64_t t94;
  float * t95;
  int64_t t96;
  float * t97;
  int64_t t98;
  int64_t t99;
  gg_type_50 t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int32_t t106;
  gg_type_50 t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int32_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  gg_type_19 * t117;
  gg_slice t118;
  int64_t t119;
  gg_type_18 * t120;
  uint8_t t121;
  uint8_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint8_t t126;
  int64_t t127;
  int64_t t128;
  gg_type_11 t129;
  int64_t t130;
  gg_type_19 * t131;
  gg_slice t132;
  int64_t t133;
  gg_type_18 * t134;
  gg_type_18 t135;
  gg_type_8 t136;
  gg_type_11 t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
  int32_t t141;
  int32_t t142;
  gg_slice t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  gg_type_22 * t151;
  gg_slice t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int32_t * t160;
  int32_t t161;
  int32_t t162;
  gg_type_22 t163;
  int32_t t164;
  int32_t t165;
  int32_t t166;
  int32_t t167;
  int32_t t168;
  int32_t t169;
  int32_t t170;
  int64_t t171;
  int32_t * t172;
  int32_t t173;
  int64_t t174;
  int32_t * t175;
  int32_t t176;
  int32_t t177;
  int64_t t178;
  int32_t * t179;
  int32_t t180;
  int64_t t181;
  int32_t * t182;
  int32_t t183;
  int32_t t184;
  int64_t t185;
  int32_t * t186;
  int32_t t187;
  int64_t t188;
  int32_t * t189;
  int32_t t190;
  int32_t t191;
  int64_t t192;
  int32_t * t193;
  int32_t t194;
  int64_t t195;
  int32_t * t196;
  int32_t t197;
  int32_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int32_t t205;
  int32_t t206;
  gg_slice t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  gg_type_22 * t211;
  gg_slice t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int32_t * t216;
  int32_t t217;
  int32_t t218;
  int32_t t219;
  int32_t t220;
  int32_t t221;
  int32_t t222;
  int32_t t223;
  int32_t t224;
  int32_t t225;
  gg_slice t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  gg_type_22 * t232;
  gg_slice t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int32_t * t239;
  int32_t t240;
  int32_t t241;
  int32_t t242;
  int32_t t243;
  int32_t t244;
  int32_t t245;
  int32_t t246;
  int32_t t247;
  int32_t t248;
  gg_type_50 t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  int32_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  gg_type_37 v0;
  gg_slice v1;
  gg_type_41 v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  gg_slice v10;
  gg_type_49 v11;
  int64_t v12;
  gg_type_50 v13;
  gg_type_11 v14;
  gg_type_8 v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int32_t v23;
  int64_t v24;
  gg_type_22 v25;
  int32_t v26;
  int32_t v27;
  int64_t v28;
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
  {
  t0 = ((int64_t)(((*(v0))).f1));
  v11 = gg_function_6(t0, base + 0);
  t1 = ((int64_t)(((int64_t)UINT64_C(256))));
  t2 = ((int64_t)((v11).f61));
  v12 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  v13 = &(((*(v2))).f54);
  t3 = ((int64_t)(((*(v0))).f2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) >= (t4));
  t6 = t5;
  if (t6) {
  t7 = ((int64_t)(((*(v0))).f3));
  t8 = ((int64_t)((v11).f60));
  t6 = ((t7) >= (t8));
  }
  t9 = t6;
  t10 = t9;
  if (t10) {
  t11 = ((int64_t)(((*(v0))).f4));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t11) >= (t12));
  }
  t13 = t10;
  t14 = t13;
  if (t14) {
  t15 = ((int64_t)(((*(v0))).f5));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t15) >= (t16));
  }
  t17 = t14;
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(((*(v0))).f5));
  t20 = ((int64_t)(((int64_t)UINT64_C(256))));
  t18 = ((t19) <= (t20));
  }
  t21 = t18;
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(((*(v0))).f6));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = ((t23) >= (t24));
  }
  t25 = t22;
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)(((*(v0))).f6));
  t28 = ((int64_t)(((int64_t)UINT64_C(16))));
  t26 = ((t27) <= (t28));
  }
  t29 = t26;
  t30 = t29;
  if (t30) {
  t31 = ((int64_t)(((*(v0))).f5));
  t32 = gg_function_7(t31, base + 0);
  t33 = ((int64_t)(t32));
  t34 = ((int64_t)(((*(v0))).f6));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((int64_t)(((int64_t)UINT64_C(256))));
  t30 = ((t35) <= (t36));
  }
  if (!(t30)) gg_fail("assertion failed");
  t37 = ((int64_t)(((*(v0))).f6));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t39 = ((t37) == (t38));
  t40 = t39;
  if (!t40) {
  t41 = ((int64_t)(((*(v0))).f5));
  t42 = ((int64_t)(((int64_t)UINT64_C(16))));
  t40 = ((t41) <= (t42));
  }
  if (!(t40)) gg_fail("assertion failed");
  t43 = ((int16_t)(((int64_t)UINT64_C(1))));
  v14 = gg_function_8(t43, base + 0);
  memset(&t44, 0, sizeof(t44));
  t45 = ((int8_t)(t44));
  for (t46=0; t46<32; t46++) (v15).data[t46] = t45;
  t48 = ((int64_t)UINT64_C(32));
  t47 = 0;
label0:;
  if (t47 >= t48) goto label2;
  v16 = t47;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t49 = v16;
  t50 = ((int8_t *)(v15).data) + gg_index(t49, ((int64_t)UINT64_C(32)));
  (*t50) = ((int8_t)(((int64_t)UINT64_C(1))));
  }
label1:;
  t47++; goto label0;
label2:;
  t52 = ((*(v0))).f6;
  t51 = 0;
label3:;
  if (t51 >= t52) goto label5;
  v17 = t51;
  {
  t54 = ((int64_t)(((*(v0))).f5));
  t55 = gg_function_7(t54, base + 0);
  t56 = t55;
  t53 = 0;
label6:;
  if (t53 >= t56) goto label8;
  v18 = t53;
  {
  t57 = ((int64_t)(v18));
  t58 = ((int64_t)(((*(v0))).f6));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  t60 = ((int64_t)(v17));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t60)))));
  t61 = ((int64_t)(v17));
  t62 = ((int64_t)(((int64_t)UINT64_C(16))));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) * (uint64_t)(t62)))));
  t64 = ((int64_t)(v18));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t64)))));
  t65 = ((int64_t)(v18));
  t66 = ((int64_t)(((*(v0))).f5));
  t67 = ((t65) < (t66));
  if (t67) {
  {
  t68 = ((int64_t)(((*(v0))).f1));
  t69 = v1;
  t70 = ((int64_t)(((*(v0))).f15));
  t71 = ((int64_t)(((*(v0))).f2));
  t72 = ((int64_t)(v18));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
  t74 = ((int64_t)(((*(v0))).f3));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t75)))));
  t77 = ((int64_t)(((*(v0))).f4));
  t78 = ((int64_t)(v17));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  t80 = ((int64_t)((v11).f60));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t81)))));
  t83 = ((int64_t)(v19));
  t84 = ((int64_t)(v20));
  t85 = v13;
  t86 = v3;
  t87 = v4;
  t88 = v5;
  t89 = v6;
  t90 = v7;
  t91 = v8;
  t92 = v9;
  t93 = v10;
  gg_function_9(t68, t69, t82, t83, t84, t85, t86, t87, t88, t89, t90, t91, t92, t93, base + 0);
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t94 = v20;
  t95 = ((float *)(((*(v13))).f50).data) + gg_index(t94, ((int64_t)UINT64_C(256)));
  (*t95) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  /* FG_DIRECT_SLICE_INDEX */
  t96 = v20;
  t97 = ((float *)(((*(v13))).f51).data) + gg_index(t96, ((int64_t)UINT64_C(256)));
  (*t97) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t99 = v12;
  t98 = 0;
label9:;
  if (t98 >= t99) goto label11;
  v21 = t98;
  {
  t100 = v13;
  t101 = ((int64_t)(v19));
  t102 = ((int64_t)(v12));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) * (uint64_t)(t102)))));
  t104 = ((int64_t)(v21));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t103) + (uint64_t)(t104)))));
  t106 = ((int32_t)(((int64_t)UINT64_C(0))));
  gg_function_13(t100, t105, t106, base + 0);
  t107 = v13;
  t108 = ((int64_t)(v19));
  t109 = ((int64_t)(v12));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = ((int64_t)(v21));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  t113 = ((int32_t)(((int64_t)UINT64_C(0))));
  gg_function_10(t107, t112, t113, base + 0);
  }
label10:;
  t98++; goto label9;
label11:;
  t115 = ((int64_t)UINT64_C(8));
  t114 = 0;
label12:;
  if (t114 >= t115) goto label14;
  v22 = t114;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t116 = v19;
  t117 = ((gg_type_19 *)(((*(v13))).f49).data) + gg_index(t116, ((int64_t)UINT64_C(256)));
  t118 = gg_view(((*t117)).data, 8);
  t119 = v22;
  t120 = ((gg_type_18 *)t118.data) + gg_index(t119, t118.len);
  memset(&t121, 0, sizeof(t121));
  t122 = ((uint8_t)(t121));
  for (t123=0; t123<32; t123++) ((*t120)).data[t123] = t122;
  }
label13:;
  t114++; goto label12;
label14:;
  }
  }
  t124 = ((int64_t)((v11).f64));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((t124) != (t125));
  if (t126) {
  {
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
  t128 = ((int64_t)UINT64_C(8));
  t127 = 0;
label15:;
  if (t127 >= t128) goto label17;
  v24 = t127;
  {
  t129 = v14;
  /* FG_DIRECT_SLICE_INDEX */
  t130 = v19;
  t131 = ((gg_type_19 *)(((*(v13))).f49).data) + gg_index(t130, ((int64_t)UINT64_C(256)));
  t132 = gg_view(((*t131)).data, 8);
  t133 = v24;
  t134 = ((gg_type_18 *)t132.data) + gg_index(t133, t132.len);
  t135 = (*t134);
  t136 = v15;
  t137 = gg_function_22(t135, t136, base + 0);
  v25 = gg_function_23(t129, t137, base + 0);
  t138 = ((int64_t)((v11).f61));
  t139 = ((int64_t)(((int64_t)UINT64_C(32))));
  t140 = ((t138) == (t139));
  if (t140) {
  {
  t141 = v23;
  t142 = ((int32_t)(((int64_t)UINT64_C(128))));
  t143 = gg_view((((*(v13))).f52).data, 512);
  t144 = ((int64_t)(v19));
  t145 = ((int64_t)(v12));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t145)))));
  t147 = ((int64_t)(v24));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t146) + (uint64_t)(t147)))));
  t149 = ((int64_t)(((int64_t)UINT64_C(8))));
  t150 = ((int64_t)gg_div(t148, t149));
  t151 = ((gg_type_22 *)t143.data) + gg_index(t150, t143.len);
  t152 = gg_view(((*t151)).data, 8);
  t153 = ((int64_t)(v19));
  t154 = ((int64_t)(v12));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) * (uint64_t)(t154)))));
  t156 = ((int64_t)(v24));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) + (uint64_t)(t156)))));
  t158 = ((int64_t)(((int64_t)UINT64_C(8))));
  t159 = ((int64_t)gg_mod(t157, t158));
  t160 = ((int32_t *)t152.data) + gg_index(t159, t152.len);
  t161 = ((int32_t)((*t160)));
  t162 = ((int32_t)(((int32_t)((uint64_t)(t142) * (uint64_t)(t161)))));
  t163 = v25;
  t164 = gg_function_24(t163, base + 0);
  t165 = ((int32_t)(t164));
  t166 = ((int32_t)(((int64_t)UINT64_C(32))));
  t167 = ((int32_t)(((int32_t)((v11).f63))));
  t168 = ((int32_t)(((int32_t)((uint64_t)(t166) * (uint64_t)(t167)))));
  t169 = ((int32_t)(((int32_t)((uint64_t)(t165) - (uint64_t)(t168)))));
  t170 = ((int32_t)(((int32_t)((uint64_t)(t162) * (uint64_t)(t169)))));
  v23 = ((int32_t)((uint64_t)(t141) + (uint64_t)(t170)));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t171 = ((int64_t)UINT64_C(0));
  t172 = ((int32_t *)(v25).data) + gg_index(t171, ((int64_t)UINT64_C(8)));
  t173 = ((int32_t)((*t172)));
  /* FG_DIRECT_SLICE_INDEX */
  t174 = ((int64_t)UINT64_C(1));
  t175 = ((int32_t *)(v25).data) + gg_index(t174, ((int64_t)UINT64_C(8)));
  t176 = ((int32_t)((*t175)));
  t177 = ((int32_t)(((int32_t)((uint64_t)(t173) + (uint64_t)(t176)))));
  /* FG_DIRECT_SLICE_INDEX */
  t178 = ((int64_t)UINT64_C(2));
  t179 = ((int32_t *)(v25).data) + gg_index(t178, ((int64_t)UINT64_C(8)));
  t180 = ((int32_t)((*t179)));
  /* FG_DIRECT_SLICE_INDEX */
  t181 = ((int64_t)UINT64_C(3));
  t182 = ((int32_t *)(v25).data) + gg_index(t181, ((int64_t)UINT64_C(8)));
  t183 = ((int32_t)((*t182)));
  t184 = ((int32_t)(((int32_t)((uint64_t)(t180) + (uint64_t)(t183)))));
  v26 = ((int32_t)(((int32_t)((uint64_t)(t177) + (uint64_t)(t184)))));
  /* FG_DIRECT_SLICE_INDEX */
  t185 = ((int64_t)UINT64_C(4));
  t186 = ((int32_t *)(v25).data) + gg_index(t185, ((int64_t)UINT64_C(8)));
  t187 = ((int32_t)((*t186)));
  /* FG_DIRECT_SLICE_INDEX */
  t188 = ((int64_t)UINT64_C(5));
  t189 = ((int32_t *)(v25).data) + gg_index(t188, ((int64_t)UINT64_C(8)));
  t190 = ((int32_t)((*t189)));
  t191 = ((int32_t)(((int32_t)((uint64_t)(t187) + (uint64_t)(t190)))));
  /* FG_DIRECT_SLICE_INDEX */
  t192 = ((int64_t)UINT64_C(6));
  t193 = ((int32_t *)(v25).data) + gg_index(t192, ((int64_t)UINT64_C(8)));
  t194 = ((int32_t)((*t193)));
  /* FG_DIRECT_SLICE_INDEX */
  t195 = ((int64_t)UINT64_C(7));
  t196 = ((int32_t *)(v25).data) + gg_index(t195, ((int64_t)UINT64_C(8)));
  t197 = ((int32_t)((*t196)));
  t198 = ((int32_t)(((int32_t)((uint64_t)(t194) + (uint64_t)(t197)))));
  v27 = ((int32_t)(((int32_t)((uint64_t)(t191) + (uint64_t)(t198)))));
  t199 = ((int64_t)(v19));
  t200 = ((int64_t)(v12));
  t201 = ((int64_t)(((int64_t)((uint64_t)(t199) * (uint64_t)(t200)))));
  t202 = ((int64_t)(((int64_t)UINT64_C(2))));
  t203 = ((int64_t)(v24));
  t204 = ((int64_t)(((int64_t)((uint64_t)(t202) * (uint64_t)(t203)))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t201) + (uint64_t)(t204)))));
  t205 = v23;
  t206 = ((int32_t)(((int64_t)UINT64_C(128))));
  t207 = gg_view((((*(v13))).f52).data, 512);
  t208 = ((int64_t)(v28));
  t209 = ((int64_t)(((int64_t)UINT64_C(8))));
  t210 = ((int64_t)gg_div(t208, t209));
  t211 = ((gg_type_22 *)t207.data) + gg_index(t210, t207.len);
  t212 = gg_view(((*t211)).data, 8);
  t213 = ((int64_t)(v28));
  t214 = ((int64_t)(((int64_t)UINT64_C(8))));
  t215 = ((int64_t)gg_mod(t213, t214));
  t216 = ((int32_t *)t212.data) + gg_index(t215, t212.len);
  t217 = ((int32_t)((*t216)));
  t218 = ((int32_t)(((int32_t)((uint64_t)(t206) * (uint64_t)(t217)))));
  t219 = ((int32_t)(v26));
  t220 = ((int32_t)(((int64_t)UINT64_C(16))));
  t221 = ((int32_t)(((int32_t)((v11).f63))));
  t222 = ((int32_t)(((int32_t)((uint64_t)(t220) * (uint64_t)(t221)))));
  t223 = ((int32_t)(((int32_t)((uint64_t)(t219) - (uint64_t)(t222)))));
  t224 = ((int32_t)(((int32_t)((uint64_t)(t218) * (uint64_t)(t223)))));
  t225 = ((int32_t)(((int64_t)UINT64_C(128))));
  t226 = gg_view((((*(v13))).f52).data, 512);
  t227 = ((int64_t)(v28));
  t228 = ((int64_t)(((int64_t)UINT64_C(1))));
  t229 = ((int64_t)(((int64_t)((uint64_t)(t227) + (uint64_t)(t228)))));
  t230 = ((int64_t)(((int64_t)UINT64_C(8))));
  t231 = ((int64_t)gg_div(t229, t230));
  t232 = ((gg_type_22 *)t226.data) + gg_index(t231, t226.len);
  t233 = gg_view(((*t232)).data, 8);
  t234 = ((int64_t)(v28));
  t235 = ((int64_t)(((int64_t)UINT64_C(1))));
  t236 = ((int64_t)(((int64_t)((uint64_t)(t234) + (uint64_t)(t235)))));
  t237 = ((int64_t)(((int64_t)UINT64_C(8))));
  t238 = ((int64_t)gg_mod(t236, t237));
  t239 = ((int32_t *)t233.data) + gg_index(t238, t233.len);
  t240 = ((int32_t)((*t239)));
  t241 = ((int32_t)(((int32_t)((uint64_t)(t225) * (uint64_t)(t240)))));
  t242 = ((int32_t)(v27));
  t243 = ((int32_t)(((int64_t)UINT64_C(16))));
  t244 = ((int32_t)(((int32_t)((v11).f63))));
  t245 = ((int32_t)(((int32_t)((uint64_t)(t243) * (uint64_t)(t244)))));
  t246 = ((int32_t)(((int32_t)((uint64_t)(t242) - (uint64_t)(t245)))));
  t247 = ((int32_t)(((int32_t)((uint64_t)(t241) * (uint64_t)(t246)))));
  t248 = ((int32_t)(((int32_t)((uint64_t)(t224) + (uint64_t)(t247)))));
  v23 = ((int32_t)((uint64_t)(t205) + (uint64_t)(t248)));
  }
  }
  }
label16:;
  t127++; goto label15;
label17:;
  t249 = v13;
  t250 = ((int64_t)(v19));
  t251 = ((int64_t)(v12));
  t252 = ((int64_t)(((int64_t)((uint64_t)(t250) * (uint64_t)(t251)))));
  t253 = ((int32_t)(v23));
  gg_function_10(t249, t252, t253, base + 0);
  }
  }
  }
label7:;
  t53++; goto label6;
label8:;
  }
label4:;
  t51++; goto label3;
label5:;
  t254 = ((int64_t)(((int64_t)UINT64_C(0))));
  t255 = ((int64_t)(((*(v0))).f1));
  t256 = ((int64_t)(((*(v0))).f5));
  t257 = ((int64_t)(((*(v0))).f6));
  t258 = ((int64_t)(((int64_t)((uint64_t)(t256) * (uint64_t)(t257)))));
  fg141_witness(t254, t255, t258);
  }
gg_done:;
}
static int64_t gg_function_50(int64_t p0, int64_t p1, int64_t base) {
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
static int64_t gg_function_52(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(16))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(16))));
  }
  } else {
  {
  v2 = ((int64_t)(v0));
  }
  }
  t4 = ((int64_t)(((int64_t)UINT64_C(31))));
  t5 = ((int64_t)(((int64_t)UINT64_C(1024))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)UINT64_C(16))));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  v3 = ((int64_t)(((int64_t)gg_div(t6, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(256))));
  t12 = ((int64_t)(((int64_t)gg_div(t10, t11))));
  t13 = ((int64_t)(((int64_t)UINT64_C(256))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)UINT64_C(4096))));
  t16 = ((t14) > (t15));
  if (t16) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(4096))));
  }
  }
label1:;
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)UINT64_C(256))));
  t19 = ((t17) >= (t18));
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(((int64_t)gg_mod(t21, t22))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t23) != (t24));
  }
  if (!(t20)) goto label2;
  {
  t25 = v3;
  t26 = ((int64_t)(((int64_t)UINT64_C(256))));
  v3 = ((int64_t)((uint64_t)(t25) - (uint64_t)(t26)));
  }
  goto label1;
label2:;
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(256))));
  t29 = ((t27) < (t28));
  if (t29) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t0 = ((int64_t)(v3));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_62(gg_type_37 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_41 p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, gg_slice p14, gg_slice p15, gg_slice p16, gg_slice p17, int64_t base) {
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
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  uint8_t t19;
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
  gg_slice t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  gg_type_26 * t41;
  float t42;
  float t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  gg_type_37 t56;
  gg_slice t57;
  gg_slice t58;
  gg_type_41 t59;
  gg_type_41 t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
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
  gg_type_37 t76;
  gg_slice t77;
  gg_type_41 t78;
  gg_slice t79;
  gg_slice t80;
  gg_slice t81;
  gg_slice t82;
  gg_slice t83;
  gg_slice t84;
  gg_slice t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  gg_type_37 t91;
  gg_type_41 t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  gg_type_37 t99;
  gg_slice t100;
  gg_type_41 t101;
  gg_slice t102;
  gg_slice t103;
  gg_slice t104;
  gg_slice t105;
  gg_slice t106;
  gg_slice t107;
  gg_slice t108;
  gg_slice t109;
  gg_type_37 t110;
  gg_slice t111;
  gg_slice t112;
  gg_type_41 t113;
  int64_t t114;
  int64_t t115;
  gg_type_41 t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
  int64_t t122;
  int64_t t123;
  gg_type_37 t124;
  gg_type_41 t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint8_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  gg_slice t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  gg_type_13 * t145;
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
  gg_slice t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  float * t160;
  gg_slice t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  gg_type_26 * t169;
  gg_slice t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  float * t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  gg_type_37 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_type_41 v8;
  gg_slice v9;
  gg_slice v10;
  gg_slice v11;
  gg_slice v12;
  gg_slice v13;
  gg_slice v14;
  gg_slice v15;
  gg_slice v16;
  gg_slice v17;
  int64_t v18;
  gg_type_49 v19;
  gg_type_1 v20;
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
  int64_t v31;
  int64_t v32;
  gg_type_13 v33;
  int64_t v34;
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
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) > (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(256))));
  t7 = ((t8) <= (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) >= (t13));
  }
  t14 = t11;
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) > (t17));
  }
  t18 = t15;
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)UINT64_C(256))));
  t19 = ((t20) <= (t21));
  }
  if (!(t19)) gg_fail("assertion failed");
  t22 = ((int64_t)(((*(v0))).f20));
  t23 = ((int64_t)(((int64_t)UINT64_C(256))));
  v18 = ((int64_t)(((int64_t)gg_div(t22, t23))));
  t24 = ((int64_t)(((*(v0))).f1));
  v19 = gg_function_6(t24, base + 0);
  v20 = (*(v0));
  (v20).f19 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v20).f11 = ((int64_t)(v3));
  (v20).f16 = ((int64_t)(v3));
  t26 = ((int64_t)(v2));
  t27 = gg_function_7(t26, base + 0);
  t28 = t27;
  t25 = 0;
label0:;
  if (t25 >= t28) goto label2;
  v21 = t25;
  {
  t30 = ((int64_t)(v4));
  t31 = gg_function_7(t30, base + 0);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(((int64_t)UINT64_C(8))));
  t34 = ((int64_t)gg_div(t32, t33));
  t29 = 0;
label3:;
  if (t29 >= t34) goto label5;
  v22 = t29;
  {
  t35 = gg_view((((*(v8))).f59).data, 8192);
  t36 = ((int64_t)(v21));
  t37 = ((int64_t)(((int64_t)UINT64_C(32))));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int64_t)(v22));
  t40 = ((int64_t)((uint64_t)(t38) + (uint64_t)(t39)));
  t41 = ((gg_type_26 *)t35.data) + gg_index(t40, t35.len);
  memset(&t42, 0, sizeof(t42));
  t43 = ((float)(t42));
  for (t44=0; t44<8; t44++) ((*t41)).data[t44] = t43;
  }
label4:;
  t29++; goto label3;
label5:;
  }
label1:;
  t25++; goto label0;
label2:;
  t45 = ((int64_t)(v4));
  t46 = ((int64_t)(((int64_t)UINT64_C(16))));
  t47 = ((t45) <= (t46));
  if (t47) {
  {
  t48 = ((int64_t)(v4));
  t49 = ((int64_t)(((*(v0))).f20));
  v23 = gg_function_52(t48, t49, base + 0);
  t50 = ((int64_t)(v23));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t50) > (t51)))) gg_fail("assertion failed");
  t52 = ((int64_t)(v23));
  t53 = ((int64_t)(((int64_t)UINT64_C(256))));
  v24 = ((int64_t)(((int64_t)gg_div(t52, t53))));
  v25 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t54 = ((int64_t)(v25));
  t55 = ((int64_t)(v18));
  if (!(((t54) < (t55)))) goto label8;
  {
  (v20).f5 = ((int64_t)(v4));
  (v20).f4 = ((int64_t)(v25));
  (v20).f6 = ((int64_t)(v24));
  t56 = &(v20);
  t57 = v6;
  t58 = v7;
  t59 = v8;
  gg_function_63(t56, t57, t58, t59, base + 0);
  t60 = v8;
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((int64_t)(v24));
  t63 = ((int64_t)((v19).f64));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((t63) != (t64));
  gg_function_54(t60, t61, t62, t65, base + 0);
  v26 = ((int64_t)(v1));
label10:;
  t66 = ((int64_t)(v26));
  t67 = ((int64_t)(v1));
  t68 = ((int64_t)(v2));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  if (!(((t66) < (t69)))) goto label11;
  {
  (v20).f2 = ((int64_t)(v26));
  t70 = ((int64_t)(((int64_t)UINT64_C(16))));
  t71 = ((int64_t)(v1));
  t72 = ((int64_t)(v2));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
  t74 = ((int64_t)(v26));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) - (uint64_t)(t74)))));
  (v20).f5 = gg_function_50(t70, t75, base + 0);
  t76 = &(v20);
  t77 = v5;
  t78 = v8;
  t79 = v10;
  t80 = v11;
  t81 = v12;
  t82 = v13;
  t83 = v14;
  t84 = v15;
  t85 = v16;
  t86 = v17;
  gg_function_64(t76, t77, t78, t79, t80, t81, t82, t83, t84, t85, t86, base + 0);
  t88 = v24;
  t87 = 0;
label12:;
  if (t87 >= t88) goto label14;
  v27 = t87;
  {
  (v20).f7 = ((int64_t)(((int64_t)UINT64_C(0))));
  (v20).f8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((int64_t)(v26));
  t90 = ((int64_t)(v1));
  (v20).f9 = ((int64_t)(((int64_t)((uint64_t)(t89) - (uint64_t)(t90)))));
  (v20).f10 = ((int64_t)(v27));
  t91 = &(v20);
  t92 = v8;
  gg_function_65(t91, t92, base + 0);
  }
label13:;
  t87++; goto label12;
label14:;
  t93 = v26;
  t94 = ((int64_t)(((int64_t)UINT64_C(16))));
  v26 = ((int64_t)((uint64_t)(t93) + (uint64_t)(t94)));
  }
  goto label10;
label11:;
  t95 = v25;
  t96 = ((int64_t)(v24));
  v25 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  }
  goto label7;
label8:;
  }
  } else {
  {
  t98 = v18;
  t97 = 0;
label15:;
  if (t97 >= t98) goto label17;
  v28 = t97;
  {
  (v20).f2 = ((int64_t)(v1));
  (v20).f5 = ((int64_t)(v2));
  (v20).f4 = ((int64_t)(v28));
  (v20).f6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t99 = &(v20);
  t100 = v5;
  t101 = v8;
  t102 = v10;
  t103 = v11;
  t104 = v12;
  t105 = v13;
  t106 = v14;
  t107 = v15;
  t108 = v16;
  t109 = v17;
  gg_function_64(t99, t100, t101, t102, t103, t104, t105, t106, t107, t108, t109, base + 0);
  (v20).f5 = ((int64_t)(v4));
  t110 = &(v20);
  t111 = v6;
  t112 = v7;
  t113 = v8;
  gg_function_63(t110, t111, t112, t113, base + 0);
  v29 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t114 = ((int64_t)(v29));
  t115 = ((int64_t)(v4));
  if (!(((t114) < (t115)))) goto label20;
  {
  t116 = v8;
  t117 = ((int64_t)(v29));
  t118 = ((int64_t)(((int64_t)UINT64_C(1))));
  t119 = ((int64_t)((v19).f64));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = ((t119) != (t120));
  gg_function_54(t116, t117, t118, t121, base + 0);
  v30 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t122 = ((int64_t)(v30));
  t123 = ((int64_t)(v2));
  if (!(((t122) < (t123)))) goto label23;
  {
  (v20).f7 = ((int64_t)(v30));
  (v20).f8 = ((int64_t)(v29));
  (v20).f9 = ((int64_t)(v30));
  (v20).f10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = &(v20);
  t125 = v8;
  gg_function_65(t124, t125, base + 0);
  t126 = v30;
  t127 = ((int64_t)(((int64_t)UINT64_C(16))));
  v30 = ((int64_t)((uint64_t)(t126) + (uint64_t)(t127)));
  }
  goto label22;
label23:;
  t128 = v29;
  t129 = ((int64_t)(((int64_t)UINT64_C(16))));
  v29 = ((int64_t)((uint64_t)(t128) + (uint64_t)(t129)));
  }
  goto label19;
label20:;
  }
label16:;
  t97++; goto label15;
label17:;
  }
  }
  t131 = v4;
  t130 = 0;
label24:;
  if (t130 >= t131) goto label26;
  v31 = t130;
  {
  v32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t132 = ((int64_t)(((*(v0))).f18));
  t133 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = ((t132) != (t133));
  if (t134) {
  {
  t135 = ((int64_t)(((*(v0))).f17));
  t136 = ((int64_t)(v31));
  t137 = ((int64_t)(((*(v0))).f26));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t137)))));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t135) + (uint64_t)(t138)))));
  t140 = ((int64_t)(v1));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t139) + (uint64_t)(t140)))));
  }
  } else {
  {
  t141 = v7;
  t142 = ((int64_t)(v3));
  t143 = ((int64_t)(v31));
  t144 = ((int64_t)((uint64_t)(t142) + (uint64_t)(t143)));
  t145 = ((gg_type_13 *)t141.data) + gg_index(t144, t141.len);
  v33 = (*t145);
  t146 = ((int64_t)(((int64_t)((v33).f47))));
  t147 = ((int64_t)(((*(v0))).f26));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t146) * (uint64_t)(t147)))));
  t149 = ((int64_t)(((int64_t)((v33).f48))));
  t150 = ((int64_t)(((*(v0))).f27));
  t151 = ((int64_t)(((int64_t)((uint64_t)(t149) * (uint64_t)(t150)))));
  t152 = ((int64_t)(((int64_t)((uint64_t)(t148) + (uint64_t)(t151)))));
  t153 = ((int64_t)(v1));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t152) + (uint64_t)(t153)))));
  }
  }
  t155 = v2;
  t154 = 0;
label27:;
  if (t154 >= t155) goto label29;
  v34 = t154;
  {
  t156 = v9;
  t157 = ((int64_t)(v32));
  t158 = ((int64_t)(v34));
  t159 = ((int64_t)((uint64_t)(t157) + (uint64_t)(t158)));
  t160 = ((float *)t156.data) + gg_index(t159, t156.len);
  t161 = gg_view((((*(v8))).f59).data, 8192);
  t162 = ((int64_t)(v34));
  t163 = ((int64_t)(((int64_t)UINT64_C(32))));
  t164 = ((int64_t)(((int64_t)((uint64_t)(t162) * (uint64_t)(t163)))));
  t165 = ((int64_t)(v31));
  t166 = ((int64_t)(((int64_t)UINT64_C(8))));
  t167 = ((int64_t)(((int64_t)gg_div(t165, t166))));
  t168 = ((int64_t)((uint64_t)(t164) + (uint64_t)(t167)));
  t169 = ((gg_type_26 *)t161.data) + gg_index(t168, t161.len);
  t170 = gg_view(((*t169)).data, 8);
  t171 = ((int64_t)(v31));
  t172 = ((int64_t)(((int64_t)UINT64_C(8))));
  t173 = ((int64_t)gg_mod(t171, t172));
  t174 = ((float *)t170.data) + gg_index(t173, t170.len);
  (*t160) = ((float)((*t174)));
  }
label28:;
  t154++; goto label27;
label29:;
  }
label25:;
  t130++; goto label24;
label26:;
  t175 = ((int64_t)(((int64_t)UINT64_C(3))));
  t176 = ((int64_t)(((*(v0))).f1));
  t177 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg141_witness(t175, t176, t177);
  }
gg_done:;
}
static void gg_function_61(gg_type_37 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_41 p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, gg_slice p14, gg_slice p15, gg_slice p16, gg_slice p17, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
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
  int64_t t88;
  int64_t t89;
  int64_t t90;
  gg_type_37 t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  gg_slice t98;
  gg_slice t99;
  gg_slice t100;
  gg_type_41 t101;
  gg_slice t102;
  gg_slice t103;
  gg_slice t104;
  gg_slice t105;
  gg_slice t106;
  gg_slice t107;
  gg_slice t108;
  gg_slice t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  gg_type_37 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_type_41 v8;
  gg_slice v9;
  gg_slice v10;
  gg_slice v11;
  gg_slice v12;
  gg_slice v13;
  gg_slice v14;
  gg_slice v15;
  gg_slice v16;
  gg_slice v17;
  gg_type_1 v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
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
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(v2));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (!t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v4));
  t3 = ((t4) >= (t5));
  }
  t6 = t3;
  if (t6) {
  {
  goto gg_done;
  }
  }
  t7 = ((int64_t)(((*(v0))).f32));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) > (t8));
  t10 = t9;
  if (t10) {
  t11 = ((int64_t)(((*(v0))).f33));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t11) > (t12));
  }
  t13 = t10;
  t14 = t13;
  if (t14) {
  t15 = ((int64_t)(((*(v0))).f30));
  t16 = ((int64_t)(((*(v0))).f32));
  t17 = ((int64_t)(((int64_t)gg_mod(t15, t16))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t17) == (t18));
  }
  t19 = t14;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(((*(v0))).f31));
  t22 = ((int64_t)(((*(v0))).f33));
  t23 = ((int64_t)(((int64_t)gg_mod(t21, t22))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t23) == (t24));
  }
  if (!(t20)) gg_fail("assertion failed");
  v18 = (*(v0));
  (v18).f18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((int64_t)(((*(v0))).f30));
  t26 = ((int64_t)(((*(v0))).f32));
  v19 = ((int64_t)(((int64_t)gg_div(t25, t26))));
  t27 = ((int64_t)(((*(v0))).f31));
  t28 = ((int64_t)(((*(v0))).f33));
  v20 = ((int64_t)(((int64_t)gg_div(t27, t28))));
  v21 = ((int64_t)(v3));
label1:;
  t29 = ((int64_t)(v21));
  t30 = ((int64_t)(v4));
  if (!(((t29) < (t30)))) goto label2;
  {
  t31 = ((int64_t)(v21));
  t32 = ((int64_t)(((int64_t)UINT64_C(256))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = ((int64_t)(v4));
  t35 = gg_function_50(t33, t34, base + 0);
  t36 = ((int64_t)(v21));
  t37 = ((int64_t)(((*(v0))).f29));
  t38 = ((int64_t)(((int64_t)gg_div(t36, t37))));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  t41 = ((int64_t)(((*(v0))).f29));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) * (uint64_t)(t41)))));
  v22 = gg_function_50(t35, t42, base + 0);
  t43 = ((int64_t)(v21));
  t44 = ((int64_t)(((*(v0))).f30));
  t45 = ((int64_t)(((*(v0))).f29));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  v23 = ((int64_t)(((int64_t)gg_div(t43, t46))));
  t47 = ((int64_t)(v21));
  t48 = ((int64_t)(v23));
  t49 = ((int64_t)(((*(v0))).f30));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t49)))));
  t51 = ((int64_t)(((*(v0))).f29));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t47) - (uint64_t)(t52)))));
  t54 = ((int64_t)(((*(v0))).f29));
  v24 = ((int64_t)(((int64_t)gg_div(t53, t54))));
  t55 = ((int64_t)(v21));
  t56 = ((int64_t)(v23));
  t57 = ((int64_t)(((*(v0))).f30));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  t59 = ((int64_t)(((*(v0))).f29));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t55) - (uint64_t)(t60)))));
  t62 = ((int64_t)(v24));
  t63 = ((int64_t)(((*(v0))).f29));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t61) - (uint64_t)(t64)))));
  t65 = ((int64_t)(v24));
  t66 = ((int64_t)(v19));
  t67 = ((int64_t)(((int64_t)gg_div(t65, t66))));
  t68 = ((int64_t)(((*(v0))).f34));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int64_t)(v23));
  t71 = ((int64_t)(v20));
  t72 = ((int64_t)(((int64_t)gg_div(t70, t71))));
  t73 = ((int64_t)(((*(v0))).f35));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  (v18).f15 = ((int64_t)(((int64_t)((uint64_t)(t69) + (uint64_t)(t74)))));
  t75 = ((int64_t)(v24));
  t76 = ((int64_t)(((*(v0))).f27));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t75) * (uint64_t)(t76)))));
  t78 = ((int64_t)(v23));
  t79 = ((int64_t)(((*(v0))).f28));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t80)))));
  t82 = ((int64_t)(v25));
  t83 = ((int64_t)(((*(v0))).f26));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) * (uint64_t)(t83)))));
  (v18).f17 = ((int64_t)(((int64_t)((uint64_t)(t81) + (uint64_t)(t84)))));
  v26 = ((int64_t)(v1));
label4:;
  t85 = ((int64_t)(v26));
  t86 = ((int64_t)(v2));
  if (!(((t85) < (t86)))) goto label5;
  {
  t87 = ((int64_t)(((int64_t)UINT64_C(256))));
  t88 = ((int64_t)(v2));
  t89 = ((int64_t)(v26));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) - (uint64_t)(t89)))));
  v27 = gg_function_50(t87, t90, base + 0);
  t91 = &(v18);
  t92 = ((int64_t)(v26));
  t93 = ((int64_t)(v27));
  t94 = ((int64_t)(v21));
  t95 = ((int64_t)(v22));
  t96 = ((int64_t)(v21));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t95) - (uint64_t)(t96)))));
  t98 = v5;
  t99 = v6;
  t100 = v7;
  t101 = v8;
  t102 = v9;
  t103 = v10;
  t104 = v11;
  t105 = v12;
  t106 = v13;
  t107 = v14;
  t108 = v15;
  t109 = v16;
  t110 = v17;
  gg_function_62(t91, t92, t93, t94, t97, t98, t99, t100, t101, t102, t103, t104, t105, t106, t107, t108, t109, t110, base + 0);
  t111 = v26;
  t112 = ((int64_t)(((int64_t)UINT64_C(256))));
  v26 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  }
  goto label4;
label5:;
  v21 = ((int64_t)(v22));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static uint8_t gg_function_57(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_type_41 p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base) {
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
  uint8_t t40;
  uint8_t t41;
  int64_t t42;
  int64_t t43;
  uint8_t t44;
  uint8_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
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
  int64_t t104;
  int64_t t105;
  int64_t t106;
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
  uint8_t t119;
  int64_t t120;
  int32_t t121;
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
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  uint8_t t145;
  uint8_t t146;
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
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
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
  int64_t t186;
  gg_type_37 t187;
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
  gg_slice t201;
  gg_slice t202;
  gg_type_41 t203;
  gg_slice t204;
  gg_slice t205;
  gg_slice t206;
  gg_slice t207;
  gg_slice t208;
  gg_slice t209;
  gg_slice t210;
  gg_slice t211;
  gg_slice t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  uint8_t t217;
  int64_t t218;
  int32_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  gg_type_37 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_type_41 v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  gg_slice v10;
  gg_slice v11;
  gg_slice v12;
  gg_slice v13;
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
  int64_t v31;
  int64_t v32;
  int64_t v33;
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
  t1 = ((int64_t)(((*(v0))).f24));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(((*(v0))).f29));
  t6 = ((int64_t)(((int64_t)UINT64_C(8))));
  t7 = ((t5) < (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f25));
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
  t12 = ((int64_t)(((*(v0))).f20));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) > (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(((*(v0))).f20));
  t17 = ((int64_t)(((int64_t)UINT64_C(256))));
  t18 = ((int64_t)(((int64_t)gg_mod(t16, t17))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t18) == (t19));
  }
  t20 = t15;
  t21 = t20;
  if (t21) {
  t22 = ((int64_t)(((*(v0))).f21));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t22) > (t23));
  }
  t24 = t21;
  t25 = t24;
  if (t25) {
  t26 = ((int64_t)(((*(v0))).f29));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t26) > (t27));
  }
  t28 = t25;
  t29 = t28;
  if (t29) {
  t30 = ((int64_t)(((*(v0))).f30));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t30) > (t31));
  }
  t32 = t29;
  t33 = t32;
  if (t33) {
  t34 = ((int64_t)(((*(v0))).f31));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t34) > (t35));
  }
  t36 = t33;
  t37 = t36;
  if (t37) {
  t38 = ((int64_t)(((*(v0))).f23));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t38) > (t39));
  }
  t40 = t37;
  t41 = t40;
  if (t41) {
  t42 = ((int64_t)(((*(v0))).f22));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((t42) >= (t43));
  }
  t44 = t41;
  t45 = t44;
  if (t45) {
  t46 = ((int64_t)(((*(v0))).f22));
  t47 = ((int64_t)(((*(v0))).f23));
  t45 = ((t46) < (t47));
  }
  if (!(t45)) gg_fail("assertion failed");
  t48 = ((int64_t)(((*(v0))).f37));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((t48) != (t49));
  if (t50) {
  {
  t51 = ((int64_t)(((*(v0))).f20));
  t52 = ((int64_t)(((int64_t)UINT64_C(256))));
  t53 = ((int64_t)(((int64_t)gg_div(t51, t52))));
  t54 = ((int64_t)(((int64_t)UINT64_C(292))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t55 = ((int64_t)(v14));
  t56 = ((int64_t)(((*(v0))).f29));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t57 = ((int64_t)(v15));
  t58 = ((int64_t)(((*(v0))).f30));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  t59 = ((int64_t)(((*(v0))).f22));
  t60 = ((int64_t)(((*(v0))).f20));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  t62 = ((int64_t)(((int64_t)UINT64_C(256))));
  t63 = ((int64_t)(((int64_t)gg_div(t61, t62))));
  t64 = ((int64_t)(((*(v0))).f23));
  v17 = ((int64_t)(((int64_t)gg_div(t63, t64))));
  t65 = ((int64_t)(((*(v0))).f22));
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = ((int64_t)(((*(v0))).f20));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)UINT64_C(256))));
  t71 = ((int64_t)(((int64_t)gg_div(t69, t70))));
  t72 = ((int64_t)(((*(v0))).f23));
  v18 = ((int64_t)(((int64_t)gg_div(t71, t72))));
  t74 = ((*(v0))).f31;
  t73 = 0;
label0:;
  if (t73 >= t74) goto label2;
  v19 = t73;
  {
  t76 = ((*(v0))).f30;
  t75 = 0;
label3:;
  if (t75 >= t76) goto label5;
  v20 = t75;
  {
  t78 = ((*(v0))).f29;
  t77 = 0;
label6:;
  if (t77 >= t78) goto label8;
  v21 = t77;
  {
  t79 = ((int64_t)(v19));
  t80 = ((int64_t)(((*(v0))).f41));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  t82 = ((int64_t)(v20));
  t83 = ((int64_t)(((*(v0))).f40));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) * (uint64_t)(t83)))));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t81) + (uint64_t)(t84)))));
  t86 = ((int64_t)(v21));
  t87 = ((int64_t)(((*(v0))).f39));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t87)))));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t85) + (uint64_t)(t88)))));
  t90 = ((int64_t)(v17));
  t91 = ((int64_t)(((int64_t)UINT64_C(256))));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
  t93 = ((int64_t)(((*(v0))).f38));
  t94 = ((int64_t)(((int64_t)((uint64_t)(t92) * (uint64_t)(t93)))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t89) + (uint64_t)(t94)))));
  t95 = ((int64_t)(v19));
  t96 = ((int64_t)(v16));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t95) * (uint64_t)(t96)))));
  t98 = ((int64_t)(v20));
  t99 = ((int64_t)(v15));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t97) + (uint64_t)(t100)))));
  t102 = ((int64_t)(v21));
  t103 = ((int64_t)(v14));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t102) * (uint64_t)(t103)))));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t101) + (uint64_t)(t104)))));
  t106 = ((int64_t)(v17));
  t107 = ((int64_t)(((int64_t)UINT64_C(292))));
  t108 = ((int64_t)(((int64_t)((uint64_t)(t106) * (uint64_t)(t107)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t105) + (uint64_t)(t108)))));
  t109 = ((int64_t)(((*(v0))).f36));
  t110 = ((int64_t)(v22));
  t111 = ((int64_t)(v23));
  t112 = ((int64_t)(v18));
  t113 = ((int64_t)(v17));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) - (uint64_t)(t113)))));
  t115 = ((int64_t)(((int64_t)UINT64_C(256))));
  t116 = ((int64_t)(((int64_t)((uint64_t)(t114) * (uint64_t)(t115)))));
  fg141_quantize(t109, t110, t111, t116);
  }
label7:;
  t77++; goto label6;
label8:;
  }
label4:;
  t75++; goto label3;
label5:;
  }
label1:;
  t73++; goto label0;
label2:;
  }
  }
  t117 = ((int64_t)(((*(v0))).f22));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((t117) == (t118));
  if (t119) {
  {
  t120 = ((int64_t)(((*(v0))).f36));
  t121 = ((int32_t)(((int32_t)(((*(v0))).f23))));
  fg141_reset(t120, t121);
  }
  }
  t122 = ((int64_t)(((*(v0))).f36));
  fg141_barrier(t122);
  v24 = ((int64_t)(((*(v0))).f21));
  t123 = ((int64_t)(((*(v0))).f29));
  t124 = ((int64_t)(((*(v0))).f30));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) * (uint64_t)(t124)))));
  t126 = ((int64_t)(((*(v0))).f31));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t125) * (uint64_t)(t126)))));
  v26 = ((int64_t)(((int64_t)UINT64_C(256))));
  t127 = ((int64_t)(v24));
  t128 = ((int64_t)(v26));
  t129 = ((int64_t)(((int64_t)((uint64_t)(t127) + (uint64_t)(t128)))));
  t130 = ((int64_t)(((int64_t)UINT64_C(1))));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t129) - (uint64_t)(t130)))));
  t132 = ((int64_t)(v26));
  v27 = ((int64_t)(((int64_t)gg_div(t131, t132))));
  t133 = ((int64_t)(v25));
  t134 = ((int64_t)(v26));
  t135 = ((int64_t)(((int64_t)((uint64_t)(t133) + (uint64_t)(t134)))));
  t136 = ((int64_t)(((int64_t)UINT64_C(1))));
  t137 = ((int64_t)(((int64_t)((uint64_t)(t135) - (uint64_t)(t136)))));
  t138 = ((int64_t)(v26));
  v28 = ((int64_t)(((int64_t)gg_div(t137, t138))));
label10:;
  t139 = ((int64_t)(v27));
  t140 = ((int64_t)(v28));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t139) * (uint64_t)(t140)))));
  t142 = ((int64_t)(((*(v0))).f23));
  t143 = ((int64_t)(((int64_t)UINT64_C(4))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t142) * (uint64_t)(t143)))));
  t145 = ((t141) < (t144));
  t146 = t145;
  if (t146) {
  t147 = ((int64_t)(v26));
  t148 = ((int64_t)(((int64_t)UINT64_C(16))));
  t146 = ((t147) > (t148));
  }
  if (!(t146)) goto label11;
  {
  t149 = v26;
  t150 = ((int64_t)(((int64_t)UINT64_C(2))));
  v26 = ((int64_t)gg_div(t149, t150));
  t151 = ((int64_t)(v24));
  t152 = ((int64_t)(v26));
  t153 = ((int64_t)(((int64_t)((uint64_t)(t151) + (uint64_t)(t152)))));
  t154 = ((int64_t)(((int64_t)UINT64_C(1))));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) - (uint64_t)(t154)))));
  t156 = ((int64_t)(v26));
  v27 = ((int64_t)(((int64_t)gg_div(t155, t156))));
  t157 = ((int64_t)(v25));
  t158 = ((int64_t)(v26));
  t159 = ((int64_t)(((int64_t)((uint64_t)(t157) + (uint64_t)(t158)))));
  t160 = ((int64_t)(((int64_t)UINT64_C(1))));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t159) - (uint64_t)(t160)))));
  t162 = ((int64_t)(v26));
  v28 = ((int64_t)(((int64_t)gg_div(t161, t162))));
  }
  goto label10;
label11:;
  t163 = ((int64_t)(v24));
  t164 = ((int64_t)(v27));
  t165 = ((int64_t)(((int64_t)((uint64_t)(t163) + (uint64_t)(t164)))));
  t166 = ((int64_t)(((int64_t)UINT64_C(1))));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t165) - (uint64_t)(t166)))));
  t168 = ((int64_t)(v27));
  v29 = ((int64_t)(((int64_t)gg_div(t167, t168))));
  t169 = ((int64_t)(v25));
  t170 = ((int64_t)(v28));
  t171 = ((int64_t)(((int64_t)((uint64_t)(t169) + (uint64_t)(t170)))));
  t172 = ((int64_t)(((int64_t)UINT64_C(1))));
  t173 = ((int64_t)(((int64_t)((uint64_t)(t171) - (uint64_t)(t172)))));
  t174 = ((int64_t)(v28));
  v30 = ((int64_t)(((int64_t)gg_div(t173, t174))));
  v31 = ((int64_t)(((*(v0))).f22));
label13:;
  t175 = ((int64_t)(v31));
  t176 = ((int64_t)(v27));
  t177 = ((int64_t)(v28));
  t178 = ((int64_t)(((int64_t)((uint64_t)(t176) * (uint64_t)(t177)))));
  if (!(((t175) < (t178)))) goto label14;
  {
  t179 = ((int64_t)(v29));
  t180 = ((int64_t)(v31));
  t181 = ((int64_t)(v27));
  t182 = ((int64_t)(((int64_t)gg_mod(t180, t181))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t179) * (uint64_t)(t182)))));
  t183 = ((int64_t)(v30));
  t184 = ((int64_t)(v31));
  t185 = ((int64_t)(v27));
  t186 = ((int64_t)(((int64_t)gg_div(t184, t185))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t183) * (uint64_t)(t186)))));
  t187 = v0;
  t188 = ((int64_t)(v32));
  t189 = ((int64_t)(v32));
  t190 = ((int64_t)(v29));
  t191 = ((int64_t)(((int64_t)((uint64_t)(t189) + (uint64_t)(t190)))));
  t192 = ((int64_t)(v24));
  t193 = gg_function_50(t191, t192, base + 0);
  t194 = ((int64_t)(v33));
  t195 = ((int64_t)(v33));
  t196 = ((int64_t)(v30));
  t197 = ((int64_t)(((int64_t)((uint64_t)(t195) + (uint64_t)(t196)))));
  t198 = ((int64_t)(v25));
  t199 = gg_function_50(t197, t198, base + 0);
  t200 = v1;
  t201 = v2;
  t202 = v3;
  t203 = v4;
  t204 = v5;
  t205 = v6;
  t206 = v7;
  t207 = v8;
  t208 = v9;
  t209 = v10;
  t210 = v11;
  t211 = v12;
  t212 = v13;
  gg_function_61(t187, t188, t193, t194, t199, t200, t201, t202, t203, t204, t205, t206, t207, t208, t209, t210, t211, t212, base + 0);
  t213 = ((int64_t)(((*(v0))).f23));
  t214 = ((int64_t)(v27));
  t215 = ((int64_t)(v28));
  t216 = ((int64_t)(((int64_t)((uint64_t)(t214) * (uint64_t)(t215)))));
  t217 = ((t213) >= (t216));
  if (t217) {
  {
  goto label14;
  }
  }
  t218 = ((int64_t)(((*(v0))).f36));
  t219 = fg141_claim(t218);
  v31 = ((int64_t)(((int64_t)(t219))));
  }
  goto label13;
label14:;
  t220 = ((int64_t)(((int64_t)UINT64_C(5))));
  t221 = ((int64_t)(((*(v0))).f1));
  t222 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg141_witness(t220, t221, t222);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_56(gg_type_37 p0, gg_type_41 p1, int64_t base) {
  /* FG_SHARED_BODY:65 */
  gg_function_65(p0, p1, base);
}
static void gg_function_53(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_type_41 p3, int64_t base) {
  /* FG_SHARED_BODY:63 */
  gg_function_63(p0, p1, p2, p3, base);
}
static void gg_function_55(gg_type_37 p0, gg_slice p1, gg_type_41 p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, int64_t base) {
  /* FG_SHARED_BODY:64 */
  gg_function_64(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, base);
}
static void gg_function_51(gg_type_37 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_type_41 p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, gg_slice p14, gg_slice p15, gg_slice p16, gg_slice p17, int64_t base) {
  /* FG_SHARED_BODY:62 */
  gg_function_62(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, base);
}
static uint8_t gg_function_49(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_type_41 p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base) {
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
  uint8_t t40;
  uint8_t t41;
  int64_t t42;
  int64_t t43;
  uint8_t t44;
  uint8_t t45;
  int64_t t46;
  int64_t t47;
  uint8_t t48;
  uint8_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  uint8_t t55;
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
  int64_t t88;
  int64_t t89;
  gg_type_37 t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  gg_slice t95;
  gg_slice t96;
  gg_slice t97;
  gg_type_41 t98;
  gg_slice t99;
  gg_slice t100;
  gg_slice t101;
  gg_slice t102;
  gg_slice t103;
  gg_slice t104;
  gg_slice t105;
  gg_slice t106;
  gg_slice t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  gg_type_37 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_type_41 v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  gg_slice v10;
  gg_slice v11;
  gg_slice v12;
  gg_slice v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  gg_type_1 v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
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
  t1 = ((int64_t)(((*(v0))).f24));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(((*(v0))).f5));
  t6 = ((int64_t)(((int64_t)UINT64_C(8))));
  t7 = ((t5) < (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f25));
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
  t12 = ((int64_t)(((*(v0))).f20));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) > (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(((*(v0))).f20));
  t17 = ((int64_t)(((int64_t)UINT64_C(256))));
  t18 = ((int64_t)(((int64_t)gg_mod(t16, t17))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t18) == (t19));
  }
  t20 = t15;
  t21 = t20;
  if (t21) {
  t22 = ((int64_t)(((*(v0))).f21));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t22) > (t23));
  }
  t24 = t21;
  t25 = t24;
  if (t25) {
  t26 = ((int64_t)(((*(v0))).f23));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t26) > (t27));
  }
  t28 = t25;
  t29 = t28;
  if (t29) {
  t30 = ((int64_t)(((*(v0))).f22));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t30) >= (t31));
  }
  t32 = t29;
  t33 = t32;
  if (t33) {
  t34 = ((int64_t)(((*(v0))).f22));
  t35 = ((int64_t)(((*(v0))).f23));
  t33 = ((t34) < (t35));
  }
  t36 = t33;
  t37 = t36;
  if (t37) {
  t38 = ((int64_t)(((*(v0))).f12));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t38) > (t39));
  }
  t40 = t37;
  t41 = t40;
  if (t41) {
  t42 = ((int64_t)(((*(v0))).f5));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((t42) >= (t43));
  }
  t44 = t41;
  t45 = t44;
  if (t45) {
  t46 = ((int64_t)(((*(v0))).f5));
  t47 = ((int64_t)((v3).len));
  t45 = ((t46) <= (t47));
  }
  t48 = t45;
  t49 = t48;
  if (t49) {
  t50 = ((int64_t)(((*(v0))).f14));
  t51 = ((int64_t)(((*(v0))).f20));
  t52 = ((int64_t)(((int64_t)UINT64_C(256))));
  t53 = ((int64_t)(((int64_t)gg_div(t51, t52))));
  t49 = ((t50) >= (t53));
  }
  t54 = t49;
  t55 = t54;
  if (t55) {
  t56 = ((int64_t)(((*(v0))).f15));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((t56) >= (t57));
  }
  if (!(t55)) gg_fail("assertion failed");
  t58 = ((int64_t)(((*(v0))).f21));
  t59 = ((int64_t)(((int64_t)UINT64_C(63))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t59)))));
  t61 = ((int64_t)(((int64_t)UINT64_C(64))));
  v14 = ((int64_t)(((int64_t)gg_div(t60, t61))));
  t62 = ((int64_t)(v14));
  t63 = ((int64_t)(((*(v0))).f22));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t65 = ((int64_t)(((*(v0))).f23));
  v15 = ((int64_t)(((int64_t)gg_div(t64, t65))));
  t66 = ((int64_t)(v14));
  t67 = ((int64_t)(((*(v0))).f22));
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t69)))));
  t71 = ((int64_t)(((*(v0))).f23));
  v16 = ((int64_t)(((int64_t)gg_div(t70, t71))));
  v17 = (*(v0));
  (v17).f18 = ((int64_t)(((int64_t)UINT64_C(0))));
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t72 = ((int64_t)(v18));
  t73 = ((int64_t)(((*(v0))).f5));
  if (!(((t72) < (t73)))) goto label2;
  {
  t74 = ((int64_t)(((int64_t)UINT64_C(256))));
  t75 = ((int64_t)(((*(v0))).f5));
  t76 = ((int64_t)(v18));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t75) - (uint64_t)(t76)))));
  v19 = gg_function_50(t74, t77, base + 0);
  t79 = ((int64_t)(v16));
  t80 = ((int64_t)(v15));
  t81 = ((int64_t)((uint64_t)(t79) - (uint64_t)(t80)));
  t78 = 0;
label3:;
  if (t78 >= t81) goto label5;
  v20 = t78;
  {
  t82 = ((int64_t)(v15));
  t83 = ((int64_t)(v20));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  t85 = ((int64_t)(((int64_t)UINT64_C(64))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))));
  t86 = ((int64_t)(((int64_t)UINT64_C(64))));
  t87 = ((int64_t)(((*(v0))).f21));
  t88 = ((int64_t)(v21));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t87) - (uint64_t)(t88)))));
  v22 = gg_function_50(t86, t89, base + 0);
  t90 = &(v17);
  t91 = ((int64_t)(v21));
  t92 = ((int64_t)(v22));
  t93 = ((int64_t)(v18));
  t94 = ((int64_t)(v19));
  t95 = v1;
  t96 = v2;
  t97 = v3;
  t98 = v4;
  t99 = v5;
  t100 = v6;
  t101 = v7;
  t102 = v8;
  t103 = v9;
  t104 = v10;
  t105 = v11;
  t106 = v12;
  t107 = v13;
  gg_function_62(t90, t91, t92, t93, t94, t95, t96, t97, t98, t99, t100, t101, t102, t103, t104, t105, t106, t107, base + 0);
  }
label4:;
  t78++; goto label3;
label5:;
  t108 = v18;
  t109 = ((int64_t)(((int64_t)UINT64_C(256))));
  v18 = ((int64_t)((uint64_t)(t108) + (uint64_t)(t109)));
  }
  goto label1;
label2:;
  t110 = ((int64_t)(((int64_t)UINT64_C(4))));
  t111 = ((int64_t)(((*(v0))).f1));
  t112 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg141_witness(t110, t111, t112);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_47(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_type_41 p3, int64_t base) {
  /* FG_SHARED_BODY:63 */
  gg_function_63(p0, p1, p2, p3, base);
}
static void gg_function_27(gg_type_37 p0, gg_type_41 p1, int64_t base) {
  /* FG_SHARED_BODY:65 */
  gg_function_65(p0, p1, base);
}
static void gg_function_5(gg_type_37 p0, gg_slice p1, gg_type_41 p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, int64_t base) {
  /* FG_SHARED_BODY:64 */
  gg_function_64(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, base);
}
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
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
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) - (uint64_t)(t11)))));
  t8 = ((t9) <= (t12));
  }
  if (!(t8)) gg_fail("assertion failed");
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_3(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  if (!(t4)) gg_fail("assertion failed");
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)gg_div(t12, t13))));
  t10 = ((t11) <= (t14));
  }
  if (!(t10)) gg_fail("assertion failed");
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_2(gg_type_37 p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
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
  gg_type_37 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  {
  t1 = ((int64_t)(((*(v0))).f20));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f20));
  t6 = ((int64_t)(((int64_t)UINT64_C(256))));
  t7 = ((int64_t)(((int64_t)gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t7) == (t8));
  }
  t9 = t4;
  t10 = t9;
  if (t10) {
  t11 = ((int64_t)(((*(v0))).f29));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t11) > (t12));
  }
  t13 = t10;
  t14 = t13;
  if (t14) {
  t15 = ((int64_t)(((*(v0))).f30));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t15) > (t16));
  }
  t17 = t14;
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(((*(v0))).f31));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t19) > (t20));
  }
  t21 = t18;
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(((*(v0))).f23));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t23) > (t24));
  }
  t25 = t22;
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)(((*(v0))).f22));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t27) >= (t28));
  }
  t29 = t26;
  t30 = t29;
  if (t30) {
  t31 = ((int64_t)(((*(v0))).f22));
  t32 = ((int64_t)(((*(v0))).f23));
  t30 = ((t31) < (t32));
  }
  t33 = t30;
  t34 = t33;
  if (t34) {
  t35 = ((int64_t)(((*(v0))).f42));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((t35) >= (t36));
  }
  t37 = t34;
  t38 = t37;
  if (t38) {
  t39 = ((int64_t)(((*(v0))).f42));
  t40 = ((int64_t)(((int64_t)UINT64_C(64))));
  t38 = ((t39) < (t40));
  }
  if (!(t38)) gg_fail("assertion failed");
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((int64_t)(((*(v0))).f37));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t41) != (t42));
  if (t43) {
  {
  t44 = ((int64_t)(((*(v0))).f20));
  t45 = ((int64_t)(((int64_t)UINT64_C(256))));
  t46 = ((int64_t)(((int64_t)gg_div(t44, t45))));
  t47 = ((int64_t)(((int64_t)UINT64_C(292))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)(((*(v0))).f29));
  t50 = gg_function_3(t48, t49, base + 0);
  t51 = ((int64_t)(((*(v0))).f30));
  t52 = gg_function_3(t50, t51, base + 0);
  t53 = ((int64_t)(((*(v0))).f31));
  v1 = gg_function_3(t52, t53, base + 0);
  t54 = ((int64_t)(v1));
  t55 = ((int64_t)(((int64_t)UINT64_C(63))));
  t56 = gg_function_4(t54, t55, base + 0);
  t57 = ((int64_t)(t56));
  t58 = ((int64_t)(((int64_t)UINT64_C(64))));
  t59 = ((int64_t)(((int64_t)gg_div(t57, t58))));
  t60 = ((int64_t)(((int64_t)UINT64_C(64))));
  v1 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  }
  }
  t61 = ((int64_t)(((*(v0))).f42));
  t62 = ((int64_t)(v1));
  t63 = gg_function_4(t61, t62, base + 0);
  t64 = ((int64_t)(((int64_t)UINT64_C(63))));
  t65 = gg_function_4(t63, t64, base + 0);
  t66 = ((int64_t)(t65));
  t67 = ((int64_t)(((int64_t)UINT64_C(64))));
  t68 = ((int64_t)(((int64_t)gg_div(t66, t67))));
  t69 = ((int64_t)(((int64_t)UINT64_C(64))));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t69)))));
  t71 = ((int64_t)(((*(v0))).f42));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t70) - (uint64_t)(t71)))));
  t72 = ((int64_t)(v2));
  t73 = ((int64_t)(((*(v0))).f22));
  t74 = ((int64_t)(((int64_t)UINT64_C(524288))));
  t75 = gg_function_3(t73, t74, base + 0);
  v3 = gg_function_4(t72, t75, base + 0);
  t76 = ((int64_t)(v3));
  t77 = ((int64_t)(((int64_t)UINT64_C(445440))));
  t78 = gg_function_4(t76, t77, base + 0);
  t79 = ((int64_t)(t78));
  t80 = ((int64_t)(((*(v0))).f43));
  if (!(((t79) <= (t80)))) gg_fail("assertion failed");
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(gg_type_37 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_type_41 p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  gg_type_37 t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  gg_type_37 t8;
  gg_slice t9;
  gg_type_41 t10;
  gg_slice t11;
  gg_slice t12;
  gg_slice t13;
  gg_slice t14;
  gg_slice t15;
  gg_slice t16;
  gg_slice t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  gg_type_37 t22;
  gg_type_41 t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  gg_type_37 t27;
  gg_slice t28;
  gg_slice t29;
  gg_type_41 t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  gg_type_37 t34;
  gg_slice t35;
  gg_slice t36;
  gg_slice t37;
  gg_type_41 t38;
  gg_slice t39;
  gg_slice t40;
  gg_slice t41;
  gg_slice t42;
  gg_slice t43;
  gg_slice t44;
  gg_slice t45;
  gg_slice t46;
  gg_slice t47;
  uint8_t t48;
  uint8_t t49;
  int64_t t50;
  int64_t t51;
  gg_type_37 t52;
  gg_slice t53;
  gg_slice t54;
  gg_slice t55;
  gg_type_41 t56;
  gg_slice t57;
  gg_slice t58;
  gg_slice t59;
  gg_slice t60;
  gg_slice t61;
  gg_slice t62;
  gg_slice t63;
  gg_slice t64;
  gg_slice t65;
  uint8_t t66;
  uint8_t t67;
  gg_type_37 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_type_41 v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  gg_slice v10;
  gg_slice v11;
  gg_slice v12;
  gg_slice v13;
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
  t1 = ((int64_t)(((*(v0))).f0));
  t2 = ((int64_t)(((int64_t)UINT64_C(5))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = v0;
  t0 = gg_function_2(t4, base + 0);
  goto gg_done;
  }
  }
  t5 = ((int64_t)(((*(v0))).f0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = v0;
  t9 = v1;
  t10 = v4;
  t11 = v5;
  t12 = v6;
  t13 = v7;
  t14 = v8;
  t15 = v9;
  t16 = v10;
  t17 = v11;
  t18 = v12;
  gg_function_64(t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(((*(v0))).f0));
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = v0;
  t23 = v4;
  gg_function_65(t22, t23, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t24 = ((int64_t)(((*(v0))).f0));
  t25 = ((int64_t)(((int64_t)UINT64_C(2))));
  t26 = ((t24) == (t25));
  if (t26) {
  {
  t27 = v0;
  t28 = v2;
  t29 = v3;
  t30 = v4;
  gg_function_63(t27, t28, t29, t30, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t31 = ((int64_t)(((*(v0))).f0));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t34 = v0;
  t35 = v1;
  t36 = v2;
  t37 = v3;
  t38 = v4;
  t39 = v13;
  t40 = v5;
  t41 = v6;
  t42 = v7;
  t43 = v8;
  t44 = v9;
  t45 = v10;
  t46 = v11;
  t47 = v12;
  t48 = gg_function_49(t34, t35, t36, t37, t38, t39, t40, t41, t42, t43, t44, t45, t46, t47, base + 0);
  t49 = t48;
  if (t49) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t50 = ((int64_t)(((*(v0))).f0));
  t51 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t50) == (t51)))) gg_fail("assertion failed");
  t52 = v0;
  t53 = v1;
  t54 = v2;
  t55 = v3;
  t56 = v4;
  t57 = v13;
  t58 = v5;
  t59 = v6;
  t60 = v7;
  t61 = v8;
  t62 = v9;
  t63 = v10;
  t64 = v11;
  t65 = v12;
  t66 = gg_function_57(t52, t53, t54, t55, t56, t57, t58, t59, t60, t61, t62, t63, t64, t65, base + 0);
  t67 = t66;
  if (t67) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
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
  uint8_t t44;
  uint8_t t45;
  int64_t t46;
  gg_type_5 t47;
  float t48;
  gg_type_8 t49;
  int8_t t50;
  int8_t t51;
  int64_t t52;
  gg_type_8 t53;
  int64_t t54;
  int16_t t55;
  int16_t t56;
  int64_t t57;
  gg_type_5 t58;
  int64_t t59;
  gg_type_13 t60;
  int32_t t61;
  int32_t t62;
  gg_type_13 t63;
  int64_t t64;
  gg_type_19 t65;
  gg_type_18 t66;
  uint8_t t67;
  uint8_t t68;
  int64_t t69;
  gg_type_18 t70;
  int64_t t71;
  gg_type_19 t72;
  int64_t t73;
  float t74;
  float t75;
  int64_t t76;
  float t77;
  float t78;
  int64_t t79;
  gg_type_22 t80;
  int32_t t81;
  int32_t t82;
  int64_t t83;
  gg_type_22 t84;
  int64_t t85;
  gg_type_22 t86;
  int32_t t87;
  int32_t t88;
  int64_t t89;
  gg_type_22 t90;
  int64_t t91;
  gg_type_9 t92;
  gg_type_8 t93;
  int8_t t94;
  int8_t t95;
  int64_t t96;
  gg_type_8 t97;
  int64_t t98;
  gg_type_9 t99;
  int64_t t100;
  gg_type_22 t101;
  int32_t t102;
  int32_t t103;
  int64_t t104;
  gg_type_22 t105;
  int64_t t106;
  float t107;
  float t108;
  int64_t t109;
  gg_type_26 t110;
  float t111;
  float t112;
  int64_t t113;
  gg_type_26 t114;
  int64_t t115;
  gg_type_28 t116;
  uint8_t t117;
  uint8_t t118;
  int64_t t119;
  gg_type_28 t120;
  int64_t t121;
  gg_type_30 t122;
  uint8_t t123;
  uint8_t t124;
  int64_t t125;
  gg_type_30 t126;
  int64_t t127;
  gg_type_32 t128;
  int8_t t129;
  int8_t t130;
  int64_t t131;
  gg_type_32 t132;
  int64_t t133;
  uint8_t t134;
  uint8_t t135;
  int64_t t136;
  int8_t t137;
  int8_t t138;
  int64_t t139;
  float t140;
  float t141;
  int64_t t142;
  gg_type_37 t143;
  gg_slice t144;
  gg_slice t145;
  gg_slice t146;
  gg_type_41 t147;
  gg_slice t148;
  gg_slice t149;
  gg_slice t150;
  gg_slice t151;
  gg_slice t152;
  gg_slice t153;
  gg_slice t154;
  gg_slice t155;
  gg_slice t156;
  gg_header t157;
  int64_t t158;
  gg_type_1 v0;
  gg_type_4 v1;
  gg_type_12 v2;
  gg_type_15 v3;
  gg_type_16 v4;
  gg_type_29 v5;
  gg_type_31 v6;
  gg_type_33 v7;
  gg_type_34 v8;
  gg_type_35 v9;
  gg_type_36 v10;
  int64_t v11;
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
  memset(&t16, 0, sizeof(t16));
  (v0).f16 = ((int64_t)(t16));
  memset(&t17, 0, sizeof(t17));
  (v0).f17 = ((int64_t)(t17));
  memset(&t18, 0, sizeof(t18));
  (v0).f18 = ((int64_t)(t18));
  memset(&t19, 0, sizeof(t19));
  (v0).f19 = ((int64_t)(t19));
  memset(&t20, 0, sizeof(t20));
  (v0).f20 = ((int64_t)(t20));
  memset(&t21, 0, sizeof(t21));
  (v0).f21 = ((int64_t)(t21));
  memset(&t22, 0, sizeof(t22));
  (v0).f22 = ((int64_t)(t22));
  memset(&t23, 0, sizeof(t23));
  (v0).f23 = ((int64_t)(t23));
  memset(&t24, 0, sizeof(t24));
  (v0).f24 = ((int64_t)(t24));
  memset(&t25, 0, sizeof(t25));
  (v0).f25 = ((int64_t)(t25));
  memset(&t26, 0, sizeof(t26));
  (v0).f26 = ((int64_t)(t26));
  memset(&t27, 0, sizeof(t27));
  (v0).f27 = ((int64_t)(t27));
  memset(&t28, 0, sizeof(t28));
  (v0).f28 = ((int64_t)(t28));
  memset(&t29, 0, sizeof(t29));
  (v0).f29 = ((int64_t)(t29));
  memset(&t30, 0, sizeof(t30));
  (v0).f30 = ((int64_t)(t30));
  memset(&t31, 0, sizeof(t31));
  (v0).f31 = ((int64_t)(t31));
  memset(&t32, 0, sizeof(t32));
  (v0).f32 = ((int64_t)(t32));
  memset(&t33, 0, sizeof(t33));
  (v0).f33 = ((int64_t)(t33));
  memset(&t34, 0, sizeof(t34));
  (v0).f34 = ((int64_t)(t34));
  memset(&t35, 0, sizeof(t35));
  (v0).f35 = ((int64_t)(t35));
  memset(&t36, 0, sizeof(t36));
  (v0).f36 = ((int64_t)(t36));
  memset(&t37, 0, sizeof(t37));
  (v0).f37 = ((int64_t)(t37));
  memset(&t38, 0, sizeof(t38));
  (v0).f38 = ((int64_t)(t38));
  memset(&t39, 0, sizeof(t39));
  (v0).f39 = ((int64_t)(t39));
  memset(&t40, 0, sizeof(t40));
  (v0).f40 = ((int64_t)(t40));
  memset(&t41, 0, sizeof(t41));
  (v0).f41 = ((int64_t)(t41));
  memset(&t42, 0, sizeof(t42));
  (v0).f42 = ((int64_t)(t42));
  memset(&t43, 0, sizeof(t43));
  (v0).f43 = ((int64_t)(t43));
  (v0).f6 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v0).f3 = ((int64_t)(((int64_t)UINT64_C(144))));
  memset(&t44, 0, sizeof(t44));
  t45 = ((uint8_t)(t44));
  for (t46=0; t46<1; t46++) (v1).data[t46] = t45;
  memset(&(t47), 0, sizeof(t47));
  memset(&t48, 0, sizeof(t48));
  (t47).f44 = ((float)(t48));
  memset(&t50, 0, sizeof(t50));
  t51 = ((int8_t)(t50));
  for (t52=0; t52<32; t52++) (t49).data[t52] = t51;
  t53 = t49;
  for (t54=0; t54<8; t54++) ((t47).f45).data[t54] = t53;
  memset(&t55, 0, sizeof(t55));
  t56 = ((int16_t)(t55));
  for (t57=0; t57<16; t57++) ((t47).f46).data[t57] = t56;
  t58 = t47;
  for (t59=0; t59<1; t59++) (v2).data[t59] = t58;
  memset(&(t60), 0, sizeof(t60));
  memset(&t61, 0, sizeof(t61));
  (t60).f47 = ((int32_t)(t61));
  memset(&t62, 0, sizeof(t62));
  (t60).f48 = ((int32_t)(t62));
  t63 = t60;
  for (t64=0; t64<1; t64++) (v3).data[t64] = t63;
  memset(&(v4), 0, sizeof(v4));
  memset(&((v4).f54), 0, sizeof((v4).f54));
  memset(&t67, 0, sizeof(t67));
  t68 = ((uint8_t)(t67));
  for (t69=0; t69<32; t69++) (t66).data[t69] = t68;
  t70 = t66;
  for (t71=0; t71<8; t71++) (t65).data[t71] = t70;
  t72 = t65;
  for (t73=0; t73<256; t73++) (((v4).f54).f49).data[t73] = t72;
  memset(&t74, 0, sizeof(t74));
  t75 = ((float)(t74));
  for (t76=0; t76<256; t76++) (((v4).f54).f50).data[t76] = t75;
  memset(&t77, 0, sizeof(t77));
  t78 = ((float)(t77));
  for (t79=0; t79<256; t79++) (((v4).f54).f51).data[t79] = t78;
  memset(&t81, 0, sizeof(t81));
  t82 = ((int32_t)(t81));
  for (t83=0; t83<8; t83++) (t80).data[t83] = t82;
  t84 = t80;
  for (t85=0; t85<512; t85++) (((v4).f54).f52).data[t85] = t84;
  memset(&t87, 0, sizeof(t87));
  t88 = ((int32_t)(t87));
  for (t89=0; t89<8; t89++) (t86).data[t89] = t88;
  t90 = t86;
  for (t91=0; t91<512; t91++) (((v4).f54).f53).data[t91] = t90;
  memset(&((v4).f58), 0, sizeof((v4).f58));
  memset(&t94, 0, sizeof(t94));
  t95 = ((int8_t)(t94));
  for (t96=0; t96<32; t96++) (t93).data[t96] = t95;
  t97 = t93;
  for (t98=0; t98<8; t98++) (t92).data[t98] = t97;
  t99 = t92;
  for (t100=0; t100<256; t100++) (((v4).f58).f55).data[t100] = t99;
  memset(&t102, 0, sizeof(t102));
  t103 = ((int32_t)(t102));
  for (t104=0; t104<8; t104++) (t101).data[t104] = t103;
  t105 = t101;
  for (t106=0; t106<512; t106++) (((v4).f58).f56).data[t106] = t105;
  memset(&t107, 0, sizeof(t107));
  t108 = ((float)(t107));
  for (t109=0; t109<256; t109++) (((v4).f58).f57).data[t109] = t108;
  memset(&t111, 0, sizeof(t111));
  t112 = ((float)(t111));
  for (t113=0; t113<8; t113++) (t110).data[t113] = t112;
  t114 = t110;
  for (t115=0; t115<8192; t115++) ((v4).f59).data[t115] = t114;
  memset(&t117, 0, sizeof(t117));
  t118 = ((uint8_t)(t117));
  for (t119=0; t119<8; t119++) (t116).data[t119] = t118;
  t120 = t116;
  for (t121=0; t121<1; t121++) (v5).data[t121] = t120;
  memset(&t123, 0, sizeof(t123));
  t124 = ((uint8_t)(t123));
  for (t125=0; t125<4; t125++) (t122).data[t125] = t124;
  t126 = t122;
  for (t127=0; t127<1; t127++) (v6).data[t127] = t126;
  memset(&t129, 0, sizeof(t129));
  t130 = ((int8_t)(t129));
  for (t131=0; t131<8; t131++) (t128).data[t131] = t130;
  t132 = t128;
  for (t133=0; t133<1; t133++) (v7).data[t133] = t132;
  memset(&t134, 0, sizeof(t134));
  t135 = ((uint8_t)(t134));
  for (t136=0; t136<128; t136++) (v8).data[t136] = t135;
  memset(&t137, 0, sizeof(t137));
  t138 = ((int8_t)(t137));
  for (t139=0; t139<16; t139++) (v9).data[t139] = t138;
  memset(&t140, 0, sizeof(t140));
  t141 = ((float)(t140));
  for (t142=0; t142<1; t142++) (v10).data[t142] = t141;
  t143 = &(v0);
  t144 = gg_view((v1).data, 1);
  t145 = gg_view((v2).data, 1);
  t146 = gg_view((v3).data, 1);
  t147 = &(v4);
  t148 = gg_view((v5).data, 1);
  t149 = gg_view((v5).data, 1);
  t150 = gg_view((v5).data, 1);
  t151 = gg_view((v6).data, 1);
  t152 = gg_view((v6).data, 1);
  t153 = gg_view((v7).data, 1);
  t154 = gg_view((v8).data, 128);
  t155 = gg_view((v9).data, 16);
  t156 = gg_view((v10).data, 1);
  v11 = gg_function_1(t143, t144, t145, t146, t147, t148, t149, t150, t151, t152, t153, t154, t155, t156, base + 0);
  t157.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t157.len = 0;
  gg_integer(gg_ref(&t157, gg_get_stack(base + 0)), v11);
  gg_append(gg_ref(&t157, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t157).base, (t157).len));
  }
gg_done:;
  for (t158 = 0; t158 < 1; t158++) gg_stacks[base + t158].top = 0;
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
