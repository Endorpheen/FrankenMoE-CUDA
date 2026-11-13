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
typedef struct gg_type_2 gg_type_2;
#pragma pack(push, 1)
typedef struct { int64_t data[4]; } gg_type_4;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_2 {
  gg_type_4 f0;
  gg_type_4 f1;
  int64_t f2;
  int64_t f3;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int64_t data[16]; } gg_type_5;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_7;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_1 {
  gg_type_2 f4;
  gg_type_2 f5;
  gg_type_2 f6;
  gg_type_2 f7;
  int64_t f8;
  int64_t f9;
  int64_t f10;
  gg_type_5 f11;
  gg_type_7 f12;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[4]; } gg_type_9;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[1]; } gg_type_10;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int64_t data[1]; } gg_type_11;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[8]; } gg_type_12;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_12 data[1]; } gg_type_13;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_9 data[1]; } gg_type_14;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int8_t data[8]; } gg_type_16;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_16 data[1]; } gg_type_17;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[128]; } gg_type_18;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int8_t data[16]; } gg_type_19;
#pragma pack(pop)
typedef gg_type_1 *gg_type_20;
typedef gg_type_2 *gg_type_31;
typedef struct gg_type_32 gg_type_32;
#pragma pack(push, 1)
struct gg_type_32 {
  int64_t f13;
  int64_t f14;
  int64_t f15;
  int64_t f16;
  int64_t f17;
};
#pragma pack(pop)
typedef struct gg_type_38 gg_type_38;
#pragma pack(push, 1)
typedef struct { uint8_t data[32]; } gg_type_39;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_39 data[8]; } gg_type_40;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_40 data[256]; } gg_type_41;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[256]; } gg_type_42;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_43;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_43 data[512]; } gg_type_44;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_38 {
  gg_type_41 f18;
  gg_type_42 f19;
  gg_type_42 f20;
  gg_type_44 f21;
  gg_type_44 f22;
};
#pragma pack(pop)
typedef gg_type_38 *gg_type_45;
#pragma pack(push, 1)
typedef struct { float data[2]; } gg_type_47;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_48;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_7 data[4]; } gg_type_49;
#pragma pack(pop)
typedef struct gg_type_50 gg_type_50;
#pragma pack(push, 1)
typedef struct { gg_type_2 data[7]; } gg_type_51;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_50 {
  gg_type_51 f23;
  int64_t f24;
  int64_t f25;
  int64_t f26;
};
#pragma pack(pop)
typedef gg_type_50 *gg_type_52;
typedef struct gg_type_53 gg_type_53;
#pragma pack(push, 1)
struct gg_type_53 {
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
  int64_t f44;
  int64_t f45;
  int64_t f46;
  int64_t f47;
  int64_t f48;
  int64_t f49;
  int64_t f50;
  int64_t f51;
  int64_t f52;
  int64_t f53;
  int64_t f54;
  int64_t f55;
  int64_t f56;
  int64_t f57;
  int64_t f58;
  int64_t f59;
  int64_t f60;
  int64_t f61;
  int64_t f62;
  int64_t f63;
  int64_t f64;
  int64_t f65;
  int64_t f66;
  int64_t f67;
  int64_t f68;
  int64_t f69;
  int64_t f70;
};
#pragma pack(pop)
typedef struct gg_type_54 gg_type_54;
#pragma pack(push, 1)
struct gg_type_54 {
  int32_t f71;
  int32_t f72;
};
#pragma pack(pop)
typedef struct gg_type_55 gg_type_55;
#pragma pack(push, 1)
typedef struct { int8_t data[32]; } gg_type_56;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_56 data[8]; } gg_type_57;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int16_t data[16]; } gg_type_58;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_55 {
  float f73;
  gg_type_57 f74;
  gg_type_58 f75;
};
#pragma pack(pop)
typedef struct gg_type_59 gg_type_59;
#pragma pack(push, 1)
typedef struct { gg_type_57 data[256]; } gg_type_60;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_59 {
  gg_type_60 f76;
  gg_type_44 f77;
  gg_type_42 f78;
};
#pragma pack(pop)
typedef struct gg_type_61 gg_type_61;
#pragma pack(push, 1)
typedef struct { gg_type_7 data[8192]; } gg_type_62;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_61 {
  gg_type_38 f79;
  gg_type_59 f80;
  gg_type_62 f81;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_63;
#pragma pack(pop)
typedef gg_type_53 *gg_type_64;
typedef gg_type_61 *gg_type_65;
#pragma pack(push, 1)
typedef struct { uint8_t data[18]; } gg_type_66;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[32]; } gg_type_67;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, gg_type_1 value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(int64_t a, int64_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, int64_t value);
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_type_4 value);
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_type_5 value);
static uint8_t gg_eq_6(float a, float b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, float value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_8(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, uint8_t value);
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
static uint8_t gg_eq_15(int8_t a, int8_t b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, int8_t value);
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
static uint8_t gg_eq_21(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_21(gg_slice a, gg_slice b);
static void gg_render_21(gg_reference out, gg_slice value);
static uint8_t gg_eq_22(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_22(gg_slice a, gg_slice b);
static void gg_render_22(gg_reference out, gg_slice value);
static uint8_t gg_eq_23(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_23(gg_slice a, gg_slice b);
static void gg_render_23(gg_reference out, gg_slice value);
static uint8_t gg_eq_24(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_24(gg_slice a, gg_slice b);
static void gg_render_24(gg_reference out, gg_slice value);
static uint8_t gg_eq_25(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_25(gg_slice a, gg_slice b);
static void gg_render_25(gg_reference out, gg_slice value);
static uint8_t gg_eq_26(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_26(gg_slice a, gg_slice b);
static void gg_render_26(gg_reference out, gg_slice value);
static uint8_t gg_eq_27(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_27(gg_slice a, gg_slice b);
static void gg_render_27(gg_reference out, gg_slice value);
static uint8_t gg_eq_28(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_28(gg_slice a, gg_slice b);
static void gg_render_28(gg_reference out, gg_slice value);
static uint8_t gg_eq_29(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_29(gg_slice a, gg_slice b);
static void gg_render_29(gg_reference out, uint8_t value);
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b);
static uint8_t gg_equal_elements_31(gg_slice a, gg_slice b);
static void gg_render_31(gg_reference out, gg_type_31 value);
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b);
static uint8_t gg_equal_elements_32(gg_slice a, gg_slice b);
static void gg_render_32(gg_reference out, gg_type_32 value);
static uint8_t gg_eq_33(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_33(gg_slice a, gg_slice b);
static void gg_render_33(gg_reference out, uint32_t value);
static uint8_t gg_eq_34(int32_t a, int32_t b);
static uint8_t gg_equal_elements_34(gg_slice a, gg_slice b);
static void gg_render_34(gg_reference out, int32_t value);
static uint8_t gg_eq_35(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_35(gg_slice a, gg_slice b);
static void gg_render_35(gg_reference out, uint64_t value);
static uint8_t gg_eq_36(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_36(gg_slice a, gg_slice b);
static void gg_render_36(gg_reference out, uint16_t value);
static uint8_t gg_eq_37(double a, double b);
static uint8_t gg_equal_elements_37(gg_slice a, gg_slice b);
static void gg_render_37(gg_reference out, double value);
static uint8_t gg_eq_38(gg_type_38 a, gg_type_38 b);
static uint8_t gg_equal_elements_38(gg_slice a, gg_slice b);
static void gg_render_38(gg_reference out, gg_type_38 value);
static uint8_t gg_eq_39(gg_type_39 a, gg_type_39 b);
static uint8_t gg_equal_elements_39(gg_slice a, gg_slice b);
static void gg_render_39(gg_reference out, gg_type_39 value);
static uint8_t gg_eq_40(gg_type_40 a, gg_type_40 b);
static uint8_t gg_equal_elements_40(gg_slice a, gg_slice b);
static void gg_render_40(gg_reference out, gg_type_40 value);
static uint8_t gg_eq_41(gg_type_41 a, gg_type_41 b);
static uint8_t gg_equal_elements_41(gg_slice a, gg_slice b);
static void gg_render_41(gg_reference out, gg_type_41 value);
static uint8_t gg_eq_42(gg_type_42 a, gg_type_42 b);
static uint8_t gg_equal_elements_42(gg_slice a, gg_slice b);
static void gg_render_42(gg_reference out, gg_type_42 value);
static uint8_t gg_eq_43(gg_type_43 a, gg_type_43 b);
static uint8_t gg_equal_elements_43(gg_slice a, gg_slice b);
static void gg_render_43(gg_reference out, gg_type_43 value);
static uint8_t gg_eq_44(gg_type_44 a, gg_type_44 b);
static uint8_t gg_equal_elements_44(gg_slice a, gg_slice b);
static void gg_render_44(gg_reference out, gg_type_44 value);
static uint8_t gg_eq_45(gg_type_45 a, gg_type_45 b);
static uint8_t gg_equal_elements_45(gg_slice a, gg_slice b);
static void gg_render_45(gg_reference out, gg_type_45 value);
static uint8_t gg_eq_46(int16_t a, int16_t b);
static uint8_t gg_equal_elements_46(gg_slice a, gg_slice b);
static void gg_render_46(gg_reference out, int16_t value);
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
static uint8_t gg_eq_59(gg_type_59 a, gg_type_59 b);
static uint8_t gg_equal_elements_59(gg_slice a, gg_slice b);
static void gg_render_59(gg_reference out, gg_type_59 value);
static uint8_t gg_eq_60(gg_type_60 a, gg_type_60 b);
static uint8_t gg_equal_elements_60(gg_slice a, gg_slice b);
static void gg_render_60(gg_reference out, gg_type_60 value);
static uint8_t gg_eq_61(gg_type_61 a, gg_type_61 b);
static uint8_t gg_equal_elements_61(gg_slice a, gg_slice b);
static void gg_render_61(gg_reference out, gg_type_61 value);
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
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((gg_type_1*)a.data)[i],((gg_type_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b) {
if(!gg_eq_2(a.f4,b.f4))return 0;
if(!gg_eq_2(a.f5,b.f5))return 0;
if(!gg_eq_2(a.f6,b.f6))return 0;
if(!gg_eq_2(a.f7,b.f7))return 0;
if(!gg_eq_3(a.f8,b.f8))return 0;
if(!gg_eq_3(a.f9,b.f9))return 0;
if(!gg_eq_3(a.f10,b.f10))return 0;
if(!gg_eq_5(a.f11,b.f11))return 0;
if(!gg_eq_7(a.f12,b.f12))return 0;
return 1;
}
static void gg_render_1(gg_reference out, gg_type_1 value) {
gg_append(out,"\116\157\144\145\040\173\040",7,1);
gg_render_2(out, value.f4);
gg_append(out,", ",2,1);
gg_render_2(out, value.f5);
gg_append(out,", ",2,1);
gg_render_2(out, value.f6);
gg_append(out,", ",2,1);
gg_render_2(out, value.f7);
gg_append(out,", ",2,1);
gg_render_3(out, value.f8);
gg_append(out,", ",2,1);
gg_render_3(out, value.f9);
gg_append(out,", ",2,1);
gg_render_3(out, value.f10);
gg_append(out,", ",2,1);
gg_render_5(out, value.f11);
gg_append(out,", ",2,1);
gg_render_7(out, value.f12);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
if(!gg_eq_4(a.f0,b.f0))return 0;
if(!gg_eq_4(a.f1,b.f1))return 0;
if(!gg_eq_3(a.f2,b.f2))return 0;
if(!gg_eq_3(a.f3,b.f3))return 0;
return 1;
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
gg_append(out,"\124\145\156\163\157\162\040\173\040",9,1);
gg_render_4(out, value.f0);
gg_append(out,", ",2,1);
gg_render_4(out, value.f1);
gg_append(out,", ",2,1);
gg_render_3(out, value.f2);
gg_append(out,", ",2,1);
gg_render_3(out, value.f3);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_type_4*)a.data)[i],((gg_type_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b) {
return gg_equal_elements_3(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_type_5*)a.data)[i],((gg_type_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
return gg_equal_elements_3(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(float a, float b) {
return a == b;
}
static void gg_render_6(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_6(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_8(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((gg_type_9*)a.data)[i],((gg_type_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b) {
return gg_equal_elements_8(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
gg_text(out,gg_view((value).data, 4));
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
return gg_equal_elements_6(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_type_11*)a.data)[i],((gg_type_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b) {
return gg_equal_elements_3(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_11(gg_reference out, gg_type_11 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return gg_equal_elements_8(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
gg_text(out,gg_view((value).data, 8));
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
return gg_equal_elements_12(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_12*)view.data)[i]).data, 8));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_type_14*)a.data)[i],((gg_type_14*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b) {
return gg_equal_elements_9(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_14(gg_reference out, gg_type_14 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_9*)view.data)[i]).data, 4));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_15(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((gg_type_16*)a.data)[i],((gg_type_16*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b) {
return gg_equal_elements_15(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_16(gg_reference out, gg_type_16 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_15(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_type_17*)a.data)[i],((gg_type_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b) {
return gg_equal_elements_16(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_17(gg_reference out, gg_type_17 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((gg_type_16*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_type_18*)a.data)[i],((gg_type_18*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b) {
return gg_equal_elements_8(gg_view((a).data, 128),gg_view((b).data, 128));
}
static void gg_render_18(gg_reference out, gg_type_18 value) {
gg_text(out,gg_view((value).data, 128));
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((gg_type_19*)a.data)[i],((gg_type_19*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b) {
return gg_equal_elements_15(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_19(gg_reference out, gg_type_19 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_15(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((gg_type_20*)a.data)[i],((gg_type_20*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b) {
return a == b;
}
static void gg_render_20(gg_reference out, gg_type_20 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_21(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_22(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(gg_slice a, gg_slice b) {
return gg_equal_elements_6(a,b);
}
static void gg_render_23(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_24(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(gg_slice a, gg_slice b) {
return gg_equal_elements_12(a,b);
}
static void gg_render_25(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_12*)view.data)[i]).data, 8));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_slice a, gg_slice b) {
return gg_equal_elements_9(a,b);
}
static void gg_render_26(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_9*)view.data)[i]).data, 4));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_slice a, gg_slice b) {
return gg_equal_elements_16(a,b);
}
static void gg_render_27(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_16(out, ((gg_type_16*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_slice a, gg_slice b) {
return gg_equal_elements_15(a,b);
}
static void gg_render_28(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_15(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_29(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_31(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_31(((gg_type_31*)a.data)[i],((gg_type_31*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_31(gg_type_31 a, gg_type_31 b) {
return a == b;
}
static void gg_render_31(gg_reference out, gg_type_31 value) {
gg_render_2(out,*value);
}
static uint8_t gg_equal_elements_32(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_32(((gg_type_32*)a.data)[i],((gg_type_32*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_32(gg_type_32 a, gg_type_32 b) {
if(!gg_eq_3(a.f13,b.f13))return 0;
if(!gg_eq_3(a.f14,b.f14))return 0;
if(!gg_eq_3(a.f15,b.f15))return 0;
if(!gg_eq_3(a.f16,b.f16))return 0;
if(!gg_eq_3(a.f17,b.f17))return 0;
return 1;
}
static void gg_render_32(gg_reference out, gg_type_32 value) {
gg_append(out,"\124\151\154\145\124\162\141\151\164\163\040\173\040",13,1);
gg_render_3(out, value.f13);
gg_append(out,", ",2,1);
gg_render_3(out, value.f14);
gg_append(out,", ",2,1);
gg_render_3(out, value.f15);
gg_append(out,", ",2,1);
gg_render_3(out, value.f16);
gg_append(out,", ",2,1);
gg_render_3(out, value.f17);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_33(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_33(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_33(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_33(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_34(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_34(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_34(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_34(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_35(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_35(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_35(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_35(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_36(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_36(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_36(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_36(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_37(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_37(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_37(double a, double b) {
return a == b;
}
static void gg_render_37(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_38(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_38(((gg_type_38*)a.data)[i],((gg_type_38*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_38(gg_type_38 a, gg_type_38 b) {
if(!gg_eq_41(a.f18,b.f18))return 0;
if(!gg_eq_42(a.f19,b.f19))return 0;
if(!gg_eq_42(a.f20,b.f20))return 0;
if(!gg_eq_44(a.f21,b.f21))return 0;
if(!gg_eq_44(a.f22,b.f22))return 0;
return 1;
}
static void gg_render_38(gg_reference out, gg_type_38 value) {
gg_append(out,"\127\145\151\147\150\164\124\151\154\145\040\173\040",13,1);
gg_render_41(out, value.f18);
gg_append(out,", ",2,1);
gg_render_42(out, value.f19);
gg_append(out,", ",2,1);
gg_render_42(out, value.f20);
gg_append(out,", ",2,1);
gg_render_44(out, value.f21);
gg_append(out,", ",2,1);
gg_render_44(out, value.f22);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_39(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_39(((gg_type_39*)a.data)[i],((gg_type_39*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_39(gg_type_39 a, gg_type_39 b) {
return gg_equal_elements_8(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_39(gg_reference out, gg_type_39 value) {
gg_text(out,gg_view((value).data, 32));
}
static uint8_t gg_equal_elements_40(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_40(((gg_type_40*)a.data)[i],((gg_type_40*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_40(gg_type_40 a, gg_type_40 b) {
return gg_equal_elements_39(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_40(gg_reference out, gg_type_40 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_39*)view.data)[i]).data, 32));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_41(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_41(((gg_type_41*)a.data)[i],((gg_type_41*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_41(gg_type_41 a, gg_type_41 b) {
return gg_equal_elements_40(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_41(gg_reference out, gg_type_41 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_40(out, ((gg_type_40*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_42(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_42(((gg_type_42*)a.data)[i],((gg_type_42*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_42(gg_type_42 a, gg_type_42 b) {
return gg_equal_elements_6(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_42(gg_reference out, gg_type_42 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_43(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_43(((gg_type_43*)a.data)[i],((gg_type_43*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_43(gg_type_43 a, gg_type_43 b) {
return gg_equal_elements_34(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_43(gg_reference out, gg_type_43 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_34(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_44(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_44(((gg_type_44*)a.data)[i],((gg_type_44*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_44(gg_type_44 a, gg_type_44 b) {
return gg_equal_elements_43(gg_view((a).data, 512),gg_view((b).data, 512));
}
static void gg_render_44(gg_reference out, gg_type_44 value) {
int64_t i; gg_slice view=gg_view((value).data, 512);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_43(out, ((gg_type_43*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_45(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_45(((gg_type_45*)a.data)[i],((gg_type_45*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_45(gg_type_45 a, gg_type_45 b) {
return a == b;
}
static void gg_render_45(gg_reference out, gg_type_45 value) {
gg_render_38(out,*value);
}
static uint8_t gg_equal_elements_46(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_46(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_46(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_46(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_47(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_47(((gg_type_47*)a.data)[i],((gg_type_47*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_47(gg_type_47 a, gg_type_47 b) {
return gg_equal_elements_6(gg_view((a).data, 2),gg_view((b).data, 2));
}
static void gg_render_47(gg_reference out, gg_type_47 value) {
int64_t i; gg_slice view=gg_view((value).data, 2);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_48(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_48(((gg_type_48*)a.data)[i],((gg_type_48*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_48(gg_type_48 a, gg_type_48 b) {
return gg_equal_elements_6(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_48(gg_reference out, gg_type_48 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_49(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_49(((gg_type_49*)a.data)[i],((gg_type_49*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_49(gg_type_49 a, gg_type_49 b) {
return gg_equal_elements_7(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_49(gg_reference out, gg_type_49 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_7(out, ((gg_type_7*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_50(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_50(((gg_type_50*)a.data)[i],((gg_type_50*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_50(gg_type_50 a, gg_type_50 b) {
if(!gg_eq_51(a.f23,b.f23))return 0;
if(!gg_eq_3(a.f24,b.f24))return 0;
if(!gg_eq_3(a.f25,b.f25))return 0;
if(!gg_eq_3(a.f26,b.f26))return 0;
return 1;
}
static void gg_render_50(gg_reference out, gg_type_50 value) {
gg_append(out,"\123\143\141\156\120\154\141\156\040\173\040",11,1);
gg_render_51(out, value.f23);
gg_append(out,", ",2,1);
gg_render_3(out, value.f24);
gg_append(out,", ",2,1);
gg_render_3(out, value.f25);
gg_append(out,", ",2,1);
gg_render_3(out, value.f26);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_51(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_51(((gg_type_51*)a.data)[i],((gg_type_51*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_51(gg_type_51 a, gg_type_51 b) {
return gg_equal_elements_2(gg_view((a).data, 7),gg_view((b).data, 7));
}
static void gg_render_51(gg_reference out, gg_type_51 value) {
int64_t i; gg_slice view=gg_view((value).data, 7);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_2(out, ((gg_type_2*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_52(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_52(((gg_type_52*)a.data)[i],((gg_type_52*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_52(gg_type_52 a, gg_type_52 b) {
return a == b;
}
static void gg_render_52(gg_reference out, gg_type_52 value) {
gg_render_50(out,*value);
}
static uint8_t gg_equal_elements_53(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_53(((gg_type_53*)a.data)[i],((gg_type_53*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_53(gg_type_53 a, gg_type_53 b) {
if(!gg_eq_3(a.f27,b.f27))return 0;
if(!gg_eq_3(a.f28,b.f28))return 0;
if(!gg_eq_3(a.f29,b.f29))return 0;
if(!gg_eq_3(a.f30,b.f30))return 0;
if(!gg_eq_3(a.f31,b.f31))return 0;
if(!gg_eq_3(a.f32,b.f32))return 0;
if(!gg_eq_3(a.f33,b.f33))return 0;
if(!gg_eq_3(a.f34,b.f34))return 0;
if(!gg_eq_3(a.f35,b.f35))return 0;
if(!gg_eq_3(a.f36,b.f36))return 0;
if(!gg_eq_3(a.f37,b.f37))return 0;
if(!gg_eq_3(a.f38,b.f38))return 0;
if(!gg_eq_3(a.f39,b.f39))return 0;
if(!gg_eq_3(a.f40,b.f40))return 0;
if(!gg_eq_3(a.f41,b.f41))return 0;
if(!gg_eq_3(a.f42,b.f42))return 0;
if(!gg_eq_3(a.f43,b.f43))return 0;
if(!gg_eq_3(a.f44,b.f44))return 0;
if(!gg_eq_3(a.f45,b.f45))return 0;
if(!gg_eq_3(a.f46,b.f46))return 0;
if(!gg_eq_3(a.f47,b.f47))return 0;
if(!gg_eq_3(a.f48,b.f48))return 0;
if(!gg_eq_3(a.f49,b.f49))return 0;
if(!gg_eq_3(a.f50,b.f50))return 0;
if(!gg_eq_3(a.f51,b.f51))return 0;
if(!gg_eq_3(a.f52,b.f52))return 0;
if(!gg_eq_3(a.f53,b.f53))return 0;
if(!gg_eq_3(a.f54,b.f54))return 0;
if(!gg_eq_3(a.f55,b.f55))return 0;
if(!gg_eq_3(a.f56,b.f56))return 0;
if(!gg_eq_3(a.f57,b.f57))return 0;
if(!gg_eq_3(a.f58,b.f58))return 0;
if(!gg_eq_3(a.f59,b.f59))return 0;
if(!gg_eq_3(a.f60,b.f60))return 0;
if(!gg_eq_3(a.f61,b.f61))return 0;
if(!gg_eq_3(a.f62,b.f62))return 0;
if(!gg_eq_3(a.f63,b.f63))return 0;
if(!gg_eq_3(a.f64,b.f64))return 0;
if(!gg_eq_3(a.f65,b.f65))return 0;
if(!gg_eq_3(a.f66,b.f66))return 0;
if(!gg_eq_3(a.f67,b.f67))return 0;
if(!gg_eq_3(a.f68,b.f68))return 0;
if(!gg_eq_3(a.f69,b.f69))return 0;
if(!gg_eq_3(a.f70,b.f70))return 0;
return 1;
}
static void gg_render_53(gg_reference out, gg_type_53 value) {
gg_append(out,"\120\154\141\156\040\173\040",7,1);
gg_render_3(out, value.f27);
gg_append(out,", ",2,1);
gg_render_3(out, value.f28);
gg_append(out,", ",2,1);
gg_render_3(out, value.f29);
gg_append(out,", ",2,1);
gg_render_3(out, value.f30);
gg_append(out,", ",2,1);
gg_render_3(out, value.f31);
gg_append(out,", ",2,1);
gg_render_3(out, value.f32);
gg_append(out,", ",2,1);
gg_render_3(out, value.f33);
gg_append(out,", ",2,1);
gg_render_3(out, value.f34);
gg_append(out,", ",2,1);
gg_render_3(out, value.f35);
gg_append(out,", ",2,1);
gg_render_3(out, value.f36);
gg_append(out,", ",2,1);
gg_render_3(out, value.f37);
gg_append(out,", ",2,1);
gg_render_3(out, value.f38);
gg_append(out,", ",2,1);
gg_render_3(out, value.f39);
gg_append(out,", ",2,1);
gg_render_3(out, value.f40);
gg_append(out,", ",2,1);
gg_render_3(out, value.f41);
gg_append(out,", ",2,1);
gg_render_3(out, value.f42);
gg_append(out,", ",2,1);
gg_render_3(out, value.f43);
gg_append(out,", ",2,1);
gg_render_3(out, value.f44);
gg_append(out,", ",2,1);
gg_render_3(out, value.f45);
gg_append(out,", ",2,1);
gg_render_3(out, value.f46);
gg_append(out,", ",2,1);
gg_render_3(out, value.f47);
gg_append(out,", ",2,1);
gg_render_3(out, value.f48);
gg_append(out,", ",2,1);
gg_render_3(out, value.f49);
gg_append(out,", ",2,1);
gg_render_3(out, value.f50);
gg_append(out,", ",2,1);
gg_render_3(out, value.f51);
gg_append(out,", ",2,1);
gg_render_3(out, value.f52);
gg_append(out,", ",2,1);
gg_render_3(out, value.f53);
gg_append(out,", ",2,1);
gg_render_3(out, value.f54);
gg_append(out,", ",2,1);
gg_render_3(out, value.f55);
gg_append(out,", ",2,1);
gg_render_3(out, value.f56);
gg_append(out,", ",2,1);
gg_render_3(out, value.f57);
gg_append(out,", ",2,1);
gg_render_3(out, value.f58);
gg_append(out,", ",2,1);
gg_render_3(out, value.f59);
gg_append(out,", ",2,1);
gg_render_3(out, value.f60);
gg_append(out,", ",2,1);
gg_render_3(out, value.f61);
gg_append(out,", ",2,1);
gg_render_3(out, value.f62);
gg_append(out,", ",2,1);
gg_render_3(out, value.f63);
gg_append(out,", ",2,1);
gg_render_3(out, value.f64);
gg_append(out,", ",2,1);
gg_render_3(out, value.f65);
gg_append(out,", ",2,1);
gg_render_3(out, value.f66);
gg_append(out,", ",2,1);
gg_render_3(out, value.f67);
gg_append(out,", ",2,1);
gg_render_3(out, value.f68);
gg_append(out,", ",2,1);
gg_render_3(out, value.f69);
gg_append(out,", ",2,1);
gg_render_3(out, value.f70);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_54(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_54(((gg_type_54*)a.data)[i],((gg_type_54*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_54(gg_type_54 a, gg_type_54 b) {
if(!gg_eq_34(a.f71,b.f71))return 0;
if(!gg_eq_34(a.f72,b.f72))return 0;
return 1;
}
static void gg_render_54(gg_reference out, gg_type_54 value) {
gg_append(out,"\122\157\165\164\145\040\173\040",8,1);
gg_render_34(out, value.f71);
gg_append(out,", ",2,1);
gg_render_34(out, value.f72);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_55(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_55(((gg_type_55*)a.data)[i],((gg_type_55*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_55(gg_type_55 a, gg_type_55 b) {
if(!gg_eq_6(a.f73,b.f73))return 0;
if(!gg_eq_57(a.f74,b.f74))return 0;
if(!gg_eq_58(a.f75,b.f75))return 0;
return 1;
}
static void gg_render_55(gg_reference out, gg_type_55 value) {
gg_append(out,"\121\070\040\173\040",5,1);
gg_render_6(out, value.f73);
gg_append(out,", ",2,1);
gg_render_57(out, value.f74);
gg_append(out,", ",2,1);
gg_render_58(out, value.f75);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_56(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_56(((gg_type_56*)a.data)[i],((gg_type_56*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_56(gg_type_56 a, gg_type_56 b) {
return gg_equal_elements_15(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_56(gg_reference out, gg_type_56 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_15(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_57(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_57(((gg_type_57*)a.data)[i],((gg_type_57*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_57(gg_type_57 a, gg_type_57 b) {
return gg_equal_elements_56(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_57(gg_reference out, gg_type_57 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_56(out, ((gg_type_56*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_58(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_58(((gg_type_58*)a.data)[i],((gg_type_58*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_58(gg_type_58 a, gg_type_58 b) {
return gg_equal_elements_46(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_58(gg_reference out, gg_type_58 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_46(out, ((int16_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_59(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_59(((gg_type_59*)a.data)[i],((gg_type_59*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_59(gg_type_59 a, gg_type_59 b) {
if(!gg_eq_60(a.f76,b.f76))return 0;
if(!gg_eq_44(a.f77,b.f77))return 0;
if(!gg_eq_42(a.f78,b.f78))return 0;
return 1;
}
static void gg_render_59(gg_reference out, gg_type_59 value) {
gg_append(out,"\101\143\164\151\166\141\164\151\157\156\124\151\154\145\040\173\040",17,1);
gg_render_60(out, value.f76);
gg_append(out,", ",2,1);
gg_render_44(out, value.f77);
gg_append(out,", ",2,1);
gg_render_42(out, value.f78);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_60(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_60(((gg_type_60*)a.data)[i],((gg_type_60*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_60(gg_type_60 a, gg_type_60 b) {
return gg_equal_elements_57(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_60(gg_reference out, gg_type_60 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_57(out, ((gg_type_57*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_61(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_61(((gg_type_61*)a.data)[i],((gg_type_61*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_61(gg_type_61 a, gg_type_61 b) {
if(!gg_eq_38(a.f79,b.f79))return 0;
if(!gg_eq_59(a.f80,b.f80))return 0;
if(!gg_eq_62(a.f81,b.f81))return 0;
return 1;
}
static void gg_render_61(gg_reference out, gg_type_61 value) {
gg_append(out,"\127\157\162\153\163\160\141\143\145\040\173\040",12,1);
gg_render_38(out, value.f79);
gg_append(out,", ",2,1);
gg_render_59(out, value.f80);
gg_append(out,", ",2,1);
gg_render_62(out, value.f81);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_62(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_62(((gg_type_62*)a.data)[i],((gg_type_62*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_62(gg_type_62 a, gg_type_62 b) {
return gg_equal_elements_7(gg_view((a).data, 8192),gg_view((b).data, 8192));
}
static void gg_render_62(gg_reference out, gg_type_62 value) {
int64_t i; gg_slice view=gg_view((value).data, 8192);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_7(out, ((gg_type_7*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_63(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_63(((gg_type_63*)a.data)[i],((gg_type_63*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_63(gg_type_63 a, gg_type_63 b) {
return gg_equal_elements_34(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_63(gg_reference out, gg_type_63 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_34(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_64(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_64(((gg_type_64*)a.data)[i],((gg_type_64*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_64(gg_type_64 a, gg_type_64 b) {
return a == b;
}
static void gg_render_64(gg_reference out, gg_type_64 value) {
gg_render_53(out,*value);
}
static uint8_t gg_equal_elements_65(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_65(((gg_type_65*)a.data)[i],((gg_type_65*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_65(gg_type_65 a, gg_type_65 b) {
return a == b;
}
static void gg_render_65(gg_reference out, gg_type_65 value) {
gg_render_61(out,*value);
}
static uint8_t gg_equal_elements_66(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_66(((gg_type_66*)a.data)[i],((gg_type_66*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_66(gg_type_66 a, gg_type_66 b) {
return gg_equal_elements_8(gg_view((a).data, 18),gg_view((b).data, 18));
}
static void gg_render_66(gg_reference out, gg_type_66 value) {
gg_text(out,gg_view((value).data, 18));
}
static uint8_t gg_equal_elements_67(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_67(((gg_type_67*)a.data)[i],((gg_type_67*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_67(gg_type_67 a, gg_type_67 b) {
return gg_equal_elements_6(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_67(gg_reference out, gg_type_67 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static uint8_t gg_function_1(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, gg_slice p14, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_3(gg_type_31 p0, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t base);
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_6(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t base);
static gg_type_32 gg_function_9(int64_t p0, int64_t base);
static int64_t gg_function_10(int64_t p0, int64_t base);
static void gg_function_11(gg_type_31 p0, int64_t p1, int64_t base);
static int64_t gg_function_12(gg_type_31 p0, int64_t base);
static int64_t gg_function_13(gg_type_31 p0, int64_t base);
static int64_t gg_function_14(gg_type_20 p0, int64_t base);
static int64_t gg_function_15(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t base);
static int64_t gg_function_16(gg_type_31 p0, int64_t p1, int64_t base);
static uint32_t gg_function_17(gg_slice p0, int64_t p1, int64_t base);
static uint8_t gg_function_18(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, int64_t base);
static float gg_function_19(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, gg_slice p4, int64_t base);
static uint32_t gg_function_20(gg_slice p0, int64_t p1, int64_t base);
static uint16_t gg_function_21(gg_slice p0, int64_t p1, int64_t base);
static float gg_function_23(uint8_t p0, int64_t base);
static float gg_function_24(uint8_t p0, int64_t base);
static void gg_function_27(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, gg_type_45 p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base);
static void gg_function_28(gg_type_45 p0, int64_t p1, int32_t p2, int64_t base);
static void gg_function_29(gg_type_45 p0, int64_t p1, int32_t p2, int64_t base);
static gg_type_39 gg_function_30(gg_slice p0, int64_t p1, int64_t base);
static gg_type_39 gg_function_31(gg_type_39 p422, uint8_t p423, int64_t base);
static gg_type_39 gg_function_32(gg_type_39 p430, uint8_t p431, int64_t base);
static gg_type_39 gg_function_33(gg_type_39 p511, gg_type_39 p512, int64_t base);
static void gg_function_34(gg_type_45 p0, int64_t p1, int64_t p2, gg_type_12 p3, uint8_t p4, int64_t base);
static void gg_function_35(gg_type_45 p0, int64_t p1, int64_t p2, uint8_t p3, int64_t base);
static void gg_function_36(gg_type_45 p0, int64_t p1, int64_t p2, gg_type_16 p3, int16_t p4, int64_t base);
static void gg_function_37(gg_type_31 p0, gg_slice p1, int64_t p2, float p3, int64_t base);
static void gg_function_38(gg_type_31 p0, gg_slice p1, int64_t p2, float p3, int64_t base);
static void gg_function_40(gg_slice p0, int64_t p1, uint32_t p2, int64_t base);
static void gg_function_42(gg_slice p0, int64_t p1, uint16_t p2, int64_t base);
static int32_t gg_function_43(float p0, int64_t base);
static uint8_t gg_function_45(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static int64_t gg_function_46(gg_type_31 p0, gg_type_31 p1, int64_t p2, int64_t base);
static float gg_function_47(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t base);
static uint16_t gg_function_49(gg_slice p0, int64_t p1, int64_t base);
static float gg_function_51(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t base);
static uint8_t gg_function_52(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static float gg_function_53(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t base);
static float gg_function_54(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t base);
static uint8_t gg_function_55(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static uint8_t gg_function_57(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t base);
static uint8_t gg_function_58(gg_type_31 p0, gg_type_31 p1, int64_t base);
static gg_type_47 gg_function_64(float p0, float p1, gg_type_47 p2, int64_t p3, float p4, float p5, int64_t base);
static uint8_t gg_function_67(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t base);
static float gg_function_68(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t p3, gg_type_31 p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static uint8_t gg_function_70(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static uint8_t gg_function_71(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static float gg_function_74(gg_type_20 p0, int64_t p1, float p2, int64_t base);
static float gg_function_80(gg_type_20 p0, float p1, int64_t base);
static float gg_function_81(float p0, int64_t base);
static float gg_function_82(float p0, int64_t base);
static float gg_function_84(float p0, int64_t base);
static float gg_function_85(float p0, int64_t base);
static uint8_t gg_function_86(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static float gg_function_87(gg_type_20 p0, float p1, int64_t p2, int64_t p3, int64_t base);
static float gg_function_88(float p0, int64_t base);
static float gg_function_89(float p0, float p1, int64_t base);
static float gg_function_90(float p0, float p1, int64_t base);
static uint8_t gg_function_91(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static uint8_t gg_function_92(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static uint8_t gg_function_93(gg_type_20 p0, gg_slice p1, gg_slice p2, int64_t base);
static int64_t gg_function_94(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_95(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t base);
static uint8_t gg_function_96(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t base) {
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
static int64_t gg_function_12(gg_type_31 p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t * t5;
  int64_t t6;
  gg_type_31 v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((int64_t)UINT64_C(4));
  t1 = 0;
label0:;
  if (t1 >= t2) goto label2;
  v2 = t1;
  {
  t3 = ((int64_t)(v1));
  /* FG_DIRECT_SLICE_INDEX */
  t4 = v2;
  t5 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t4, ((int64_t)UINT64_C(4)));
  t6 = ((int64_t)((*t5)));
  v1 = gg_function_2(t3, t6, base + 0);
  }
label1:;
  t1++; goto label0;
label2:;
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_40(gg_slice p0, int64_t p1, uint32_t p2, int64_t base) {
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
  gg_slice t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t * t14;
  uint32_t t15;
  int64_t t16;
  gg_slice t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t * t21;
  uint32_t t22;
  int64_t t23;
  gg_slice t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t * t28;
  uint32_t t29;
  int64_t t30;
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
  (*t9) = ((uint8_t)(((uint8_t)(v2))));
  t10 = v0;
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = ((int64_t)((uint64_t)(t11) + (uint64_t)(t12)));
  t14 = ((uint8_t *)t10.data) + gg_index(t13, t10.len);
  t15 = ((uint32_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(8))));
  (*t14) = ((uint8_t)(((uint8_t)(((t15) >> ((uint64_t)(t16) & 31))))));
  t17 = v0;
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  t21 = ((uint8_t *)t17.data) + gg_index(t20, t17.len);
  t22 = ((uint32_t)(v2));
  t23 = ((int64_t)(((int64_t)UINT64_C(16))));
  (*t21) = ((uint8_t)(((uint8_t)(((t22) >> ((uint64_t)(t23) & 31))))));
  t24 = v0;
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(3))));
  t27 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  t28 = ((uint8_t *)t24.data) + gg_index(t27, t24.len);
  t29 = ((uint32_t)(v2));
  t30 = ((int64_t)(((int64_t)UINT64_C(24))));
  (*t28) = ((uint8_t)(((uint8_t)(((t29) >> ((uint64_t)(t30) & 31))))));
  }
gg_done:;
}
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t base) {
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
static int64_t gg_function_16(gg_type_31 p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  gg_type_31 t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t * t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t * t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t * t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t * t27;
  int64_t t28;
  gg_type_31 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = v0;
  t7 = gg_function_12(t6, base + 0);
  t8 = ((int64_t)(t7));
  t4 = ((t5) < (t8));
  }
  if (!(t4)) gg_fail("assertion failed");
  v2 = ((int64_t)(v1));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)UINT64_C(4));
  t9 = 0;
label0:;
  if (t9 >= t10) goto label2;
  v4 = t9;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v4;
  t12 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t11, ((int64_t)UINT64_C(4)));
  t13 = ((int64_t)((*t12)));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t13) > (t14)))) gg_fail("assertion failed");
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(v2));
  /* FG_DIRECT_SLICE_INDEX */
  t17 = v4;
  t18 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t17, ((int64_t)UINT64_C(4)));
  t19 = ((int64_t)((*t18)));
  t20 = ((int64_t)(((int64_t)gg_mod(t16, t19))));
  /* FG_DIRECT_SLICE_INDEX */
  t21 = v4;
  t22 = ((int64_t *)(((*(v0))).f1).data) + gg_index(t21, ((int64_t)UINT64_C(4)));
  t23 = ((int64_t)((*t22)));
  t24 = gg_function_2(t20, t23, base + 0);
  v3 = gg_function_5(t15, t24, base + 0);
  t25 = v2;
  /* FG_DIRECT_SLICE_INDEX */
  t26 = v4;
  t27 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t26, ((int64_t)UINT64_C(4)));
  t28 = ((int64_t)((*t27)));
  v2 = ((int64_t)gg_div(t25, t28));
  }
label1:;
  t9++; goto label0;
label2:;
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint16_t gg_function_49(gg_slice p0, int64_t p1, int64_t base) {
  uint16_t t0;
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
  uint16_t t11;
  gg_slice t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t * t16;
  uint16_t t17;
  int64_t t18;
  uint16_t t19;
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
  t11 = ((uint16_t)(((uint16_t)((*t10)))));
  t12 = v0;
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  t16 = ((uint8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((uint16_t)(((uint16_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((uint16_t)(((uint16_t)((uint64_t)(t17) << ((uint64_t)(t18) & 15)))));
  t0 = ((uint16_t)(((t11) | (t19))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint32_t gg_function_17(gg_slice p0, int64_t p1, int64_t base) {
  uint32_t t0;
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
  t11 = ((uint32_t)(((uint32_t)((*t10)))));
  t12 = v0;
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  t16 = ((uint8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((uint32_t)(((uint32_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((uint32_t)(((uint32_t)((uint64_t)(t17) << ((uint64_t)(t18) & 31)))));
  t20 = ((uint32_t)(((t11) | (t19))));
  t21 = v0;
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((uint8_t *)t21.data) + gg_index(t24, t21.len);
  t26 = ((uint32_t)(((uint32_t)((*t25)))));
  t27 = ((int64_t)(((int64_t)UINT64_C(16))));
  t28 = ((uint32_t)(((uint32_t)((uint64_t)(t26) << ((uint64_t)(t27) & 31)))));
  t29 = ((uint32_t)(((t20) | (t28))));
  t30 = v0;
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  t33 = ((int64_t)((uint64_t)(t31) + (uint64_t)(t32)));
  t34 = ((uint8_t *)t30.data) + gg_index(t33, t30.len);
  t35 = ((uint32_t)(((uint32_t)((*t34)))));
  t36 = ((int64_t)(((int64_t)UINT64_C(24))));
  t37 = ((uint32_t)(((uint32_t)((uint64_t)(t35) << ((uint64_t)(t36) & 31)))));
  t0 = ((uint32_t)(((t29) | (t37))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_47(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  gg_slice t4;
  int64_t t5;
  uint32_t t6;
  float t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  gg_slice t11;
  int64_t t12;
  uint16_t t13;
  float t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  gg_slice t18;
  int64_t t19;
  uint16_t t20;
  uint32_t t21;
  int64_t t22;
  uint32_t t23;
  float t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  gg_slice t28;
  int64_t t29;
  uint32_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  gg_slice t34;
  int64_t t35;
  uint16_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint8_t * t48;
  gg_type_31 v0;
  gg_slice v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((*(v0))).f2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = v1;
  t5 = ((int64_t)(v2));
  t6 = gg_function_17(t4, t5, base + 0);
  t7 = fg143_from_bits(t6);
  t0 = ((float)(t7));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(((*(v0))).f2));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t11 = v1;
  t12 = ((int64_t)(v2));
  t13 = gg_function_49(t11, t12, base + 0);
  t14 = fg143_half(t13);
  t0 = ((float)(t14));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(((*(v0))).f2));
  t16 = ((int64_t)(((int64_t)UINT64_C(30))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t18 = v1;
  t19 = ((int64_t)(v2));
  t20 = gg_function_49(t18, t19, base + 0);
  t21 = ((uint32_t)(((uint32_t)(t20))));
  t22 = ((int64_t)(((int64_t)UINT64_C(16))));
  t23 = ((uint32_t)(((uint32_t)((uint64_t)(t21) << ((uint64_t)(t22) & 31)))));
  t24 = fg143_from_bits(t23);
  t0 = ((float)(t24));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(((*(v0))).f2));
  t26 = ((int64_t)(((int64_t)UINT64_C(26))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = v1;
  t29 = ((int64_t)(v2));
  t30 = gg_function_17(t28, t29, base + 0);
  t0 = ((float)(((float)(((int32_t)(t30))))));
  goto gg_done;
  }
  }
  t31 = ((int64_t)(((*(v0))).f2));
  t32 = ((int64_t)(((int64_t)UINT64_C(25))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t34 = v1;
  t35 = ((int64_t)(v2));
  t36 = gg_function_49(t34, t35, base + 0);
  t0 = ((float)(((float)(((int16_t)(t36))))));
  goto gg_done;
  }
  }
  t37 = ((int64_t)(((*(v0))).f2));
  t38 = ((int64_t)(((int64_t)UINT64_C(24))));
  t39 = ((t37) == (t38));
  t40 = t39;
  if (t40) {
  t41 = ((int64_t)(v2));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t41) >= (t42));
  }
  t43 = t40;
  t44 = t43;
  if (t44) {
  t45 = ((int64_t)(v2));
  t46 = ((int64_t)((v1).len));
  t44 = ((t45) < (t46));
  }
  if (!(t44)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t47 = v2;
  t48 = ((uint8_t *)(v1).data) + gg_index(t47, (v1).len);
  t0 = ((float)(((float)(((int8_t)((*t48)))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_51(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t base) {
  float t0;
  gg_type_31 t1;
  gg_slice t2;
  gg_type_31 t3;
  int64_t t4;
  int64_t t5;
  gg_type_31 v0;
  gg_slice v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = v0;
  t2 = v1;
  t3 = v0;
  t4 = ((int64_t)(v2));
  t5 = gg_function_16(t3, t4, base + 0);
  t0 = gg_function_47(t1, t2, t5, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_13(gg_type_31 p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t * t2;
  int64_t t3;
  int64_t t4;
  int64_t * t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_type_31 v0;
  v0 = p0;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1 = ((int64_t)UINT64_C(1));
  t2 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t1, ((int64_t)UINT64_C(4)));
  t3 = ((int64_t)((*t2)));
  /* FG_DIRECT_SLICE_INDEX */
  t4 = ((int64_t)UINT64_C(2));
  t5 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t4, ((int64_t)UINT64_C(4)));
  t6 = ((int64_t)((*t5)));
  t7 = gg_function_2(t3, t6, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(3));
  t9 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = ((int64_t)((*t9)));
  t0 = gg_function_2(t7, t10, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_14(gg_type_20 p0, int64_t base) {
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
  gg_type_20 v0;
  v0 = p0;
  {
  t1 = ((int64_t)(((*(v0))).f10));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f9));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f9));
  t10 = ((int64_t)(((*(v0))).f10));
  t8 = ((t9) < (t10));
  }
  if (!(t8)) gg_fail("assertion failed");
  t0 = ((int64_t)(((*(v0))).f9));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_96(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t * t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  gg_type_20 t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  gg_type_31 t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  gg_slice t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t * t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  gg_slice t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t * t47;
  gg_slice t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t * t52;
  gg_type_31 t53;
  gg_slice t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  gg_type_31 t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t * t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
  float t72;
  float t73;
  float t74;
  float t75;
  uint8_t t76;
  gg_slice t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t * t83;
  gg_slice t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t * t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t * t93;
  int64_t t94;
  gg_slice t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t * t99;
  int64_t t100;
  int64_t t101;
  gg_slice t102;
  gg_type_31 t103;
  int64_t t104;
  int64_t t105;
  int64_t * t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  gg_slice t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t * t116;
  uint32_t t117;
  int64_t t118;
  int64_t t119;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  float v12;
  float v13;
  uint8_t v14;
  int64_t v15;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(69))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f8));
  t6 = ((int64_t)(((int64_t)UINT64_C(70))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  v4 = ((int64_t)((*t9)));
  t10 = ((int64_t)((((*(v0))).f7).f2));
  t11 = ((int64_t)(((int64_t)UINT64_C(26))));
  t12 = ((t10) == (t11));
  t13 = t12;
  if (t13) {
  /* FG_DIRECT_SLICE_INDEX */
  t14 = ((int64_t)UINT64_C(0));
  t15 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t14, ((int64_t)UINT64_C(4)));
  t16 = ((int64_t)((*t15)));
  t17 = ((int64_t)(v4));
  t13 = ((t16) <= (t17));
  }
  if (!(t13)) gg_fail("assertion failed");
  t18 = ((int64_t)((v3).len));
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(((*(v0))).f10));
  t21 = gg_function_2(t19, t20, base + 0);
  t22 = ((int64_t)(t21));
  if (!(((t18) >= (t22)))) gg_fail("assertion failed");
  t23 = v0;
  v5 = gg_function_14(t23, base + 0);
  t24 = ((int64_t)(((*(v0))).f9));
  t25 = ((int64_t)(v4));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
label1:;
  t26 = ((int64_t)(v5));
  t27 = &(((*(v0))).f4);
  t28 = gg_function_13(t27, base + 0);
  t29 = ((int64_t)(t28));
  if (!(((t26) < (t29)))) goto label2;
  {
  t31 = v4;
  t30 = 0;
label3:;
  if (t30 >= t31) goto label5;
  v7 = t30;
  {
  t32 = v3;
  t33 = ((int64_t)(v6));
  t34 = ((int64_t)(v7));
  t35 = ((int64_t)((uint64_t)(t33) + (uint64_t)(t34)));
  t36 = ((int64_t *)t32.data) + gg_index(t35, t32.len);
  (*t36) = ((int64_t)(v7));
  }
label4:;
  t30++; goto label3;
label5:;
  t38 = v4;
  t37 = 0;
label6:;
  if (t37 >= t38) goto label8;
  v8 = t37;
  {
  v9 = ((int64_t)(v8));
label10:;
  t39 = ((int64_t)(v9));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t39) > (t40)))) goto label11;
  {
  t41 = v3;
  t42 = ((int64_t)(v6));
  t43 = ((int64_t)(v9));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  t46 = ((int64_t)((uint64_t)(t44) - (uint64_t)(t45)));
  t47 = ((int64_t *)t41.data) + gg_index(t46, t41.len);
  v10 = ((int64_t)((*t47)));
  t48 = v3;
  t49 = ((int64_t)(v6));
  t50 = ((int64_t)(v9));
  t51 = ((int64_t)((uint64_t)(t49) + (uint64_t)(t50)));
  t52 = ((int64_t *)t48.data) + gg_index(t51, t48.len);
  v11 = ((int64_t)((*t52)));
  t53 = &(((*(v0))).f4);
  t54 = v1;
  t55 = ((int64_t)(v5));
  t56 = ((int64_t)(v4));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t58 = ((int64_t)(v10));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  v12 = gg_function_51(t53, t54, t59, base + 0);
  t60 = &(((*(v0))).f4);
  t61 = v1;
  t62 = ((int64_t)(v5));
  t63 = ((int64_t)(v4));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t65 = ((int64_t)(v11));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
  v13 = gg_function_51(t60, t61, t66, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t67 = ((int64_t)UINT64_C(0));
  t68 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t67, ((int64_t)UINT64_C(16)));
  t69 = ((int64_t)((*t68)));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((t69) == (t70));
  if (t71) {
  {
  t72 = ((float)(v12));
  t73 = ((float)(v13));
  v14 = ((t72) <= (t73));
  }
  } else {
  {
  t74 = ((float)(v12));
  t75 = ((float)(v13));
  v14 = ((t74) >= (t75));
  }
  }
  t76 = v14;
  if (t76) {
  {
  goto label11;
  }
  }
  t77 = v3;
  t78 = ((int64_t)(v6));
  t79 = ((int64_t)(v9));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t79)))));
  t81 = ((int64_t)(((int64_t)UINT64_C(1))));
  t82 = ((int64_t)((uint64_t)(t80) - (uint64_t)(t81)));
  t83 = ((int64_t *)t77.data) + gg_index(t82, t77.len);
  (*t83) = ((int64_t)(v11));
  t84 = v3;
  t85 = ((int64_t)(v6));
  t86 = ((int64_t)(v9));
  t87 = ((int64_t)((uint64_t)(t85) + (uint64_t)(t86)));
  t88 = ((int64_t *)t84.data) + gg_index(t87, t84.len);
  (*t88) = ((int64_t)(v10));
  t89 = v9;
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t89) - (uint64_t)(t90)));
  }
  goto label10;
label11:;
  }
label7:;
  t37++; goto label6;
label8:;
  /* FG_DIRECT_SLICE_INDEX */
  t92 = ((int64_t)UINT64_C(0));
  t93 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t92, ((int64_t)UINT64_C(4)));
  t94 = (*t93);
  t91 = 0;
label12:;
  if (t91 >= t94) goto label14;
  v15 = t91;
  {
  t95 = v3;
  t96 = ((int64_t)(v6));
  t97 = ((int64_t)(v15));
  t98 = ((int64_t)((uint64_t)(t96) + (uint64_t)(t97)));
  t99 = ((int64_t *)t95.data) + gg_index(t98, t95.len);
  t100 = ((int64_t)((*t99)));
  t101 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  if (!(((t100) <= (t101)))) gg_fail("assertion failed");
  t102 = v2;
  t103 = &(((*(v0))).f7);
  t104 = ((int64_t)(v5));
  /* FG_DIRECT_SLICE_INDEX */
  t105 = ((int64_t)UINT64_C(0));
  t106 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t105, ((int64_t)UINT64_C(4)));
  t107 = ((int64_t)((*t106)));
  t108 = ((int64_t)(((int64_t)((uint64_t)(t104) * (uint64_t)(t107)))));
  t109 = ((int64_t)(v15));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) + (uint64_t)(t109)))));
  t111 = gg_function_16(t103, t110, base + 0);
  t112 = v3;
  t113 = ((int64_t)(v6));
  t114 = ((int64_t)(v15));
  t115 = ((int64_t)((uint64_t)(t113) + (uint64_t)(t114)));
  t116 = ((int64_t *)t112.data) + gg_index(t115, t112.len);
  t117 = ((uint32_t)(((uint32_t)((*t116)))));
  gg_function_40(t102, t111, t117, base + 0);
  }
label13:;
  t91++; goto label12;
label14:;
  t118 = v5;
  t119 = ((int64_t)(((*(v0))).f10));
  v5 = ((int64_t)((uint64_t)(t118) + (uint64_t)(t119)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_43(float p0, int64_t base) {
  int32_t t0;
  float t1;
  uint32_t t2;
  uint32_t t3;
  uint32_t t4;
  uint32_t t5;
  int64_t t6;
  uint32_t t7;
  uint32_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint32_t t14;
  uint32_t t15;
  uint32_t t16;
  uint32_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  uint32_t t21;
  int64_t t22;
  int64_t t23;
  uint32_t t24;
  uint32_t t25;
  int64_t t26;
  int64_t t27;
  uint32_t t28;
  uint32_t t29;
  int64_t t30;
  uint32_t t31;
  uint32_t t32;
  uint8_t t33;
  uint32_t t34;
  uint32_t t35;
  uint32_t t36;
  uint32_t t37;
  uint32_t t38;
  uint32_t t39;
  float v0;
  uint32_t v1;
  uint32_t v2;
  int64_t v3;
  uint32_t v4;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = fg143_bits(t1);
  v1 = ((uint32_t)(t2));
  t3 = ((uint32_t)(v1));
  t4 = ((uint32_t)(((int64_t)UINT64_C(2147483647))));
  v2 = ((uint32_t)(((t3) & (t4))));
  t5 = ((uint32_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(23))));
  t7 = ((uint32_t)(((t5) >> ((uint64_t)(t6) & 31))));
  t8 = ((uint32_t)(((int64_t)UINT64_C(255))));
  v3 = ((int64_t)(((int64_t)(((t7) & (t8))))));
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(127))));
  t11 = ((t9) < (t10));
  if (t11) {
  {
  t0 = ((int32_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(158))));
  if (!(((t12) <= (t13)))) gg_fail("assertion failed");
  t14 = ((uint32_t)(v2));
  t15 = ((uint32_t)(((int64_t)UINT64_C(8388607))));
  t16 = ((uint32_t)(((t14) & (t15))));
  t17 = ((uint32_t)(((int64_t)UINT64_C(8388608))));
  v4 = ((uint32_t)(((t16) | (t17))));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(150))));
  t20 = ((t18) >= (t19));
  if (t20) {
  {
  t21 = ((uint32_t)(v4));
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(((int64_t)UINT64_C(150))));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t22) - (uint64_t)(t23)))))));
  v4 = ((uint32_t)(((uint32_t)((uint64_t)(t21) << ((uint64_t)(t24) & 31)))));
  }
  } else {
  {
  t25 = ((uint32_t)(v4));
  t26 = ((int64_t)(((int64_t)UINT64_C(150))));
  t27 = ((int64_t)(v3));
  t28 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t26) - (uint64_t)(t27)))))));
  v4 = ((uint32_t)(((t25) >> ((uint64_t)(t28) & 31))));
  }
  }
  t29 = ((uint32_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(31))));
  t31 = ((uint32_t)(((t29) >> ((uint64_t)(t30) & 31))));
  t32 = ((uint32_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  if (t33) {
  {
  t34 = ((uint32_t)(v4));
  t35 = ((uint32_t)(((int64_t)UINT64_C(2147483648))));
  if (!(((t34) <= (t35)))) gg_fail("assertion failed");
  t36 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
  t37 = ((uint32_t)(v4));
  t0 = ((int32_t)(((int32_t)(((uint32_t)((uint64_t)(t36) - (uint64_t)(t37)))))));
  goto gg_done;
  }
  }
  t38 = ((uint32_t)(v4));
  t39 = ((uint32_t)(((int64_t)UINT64_C(2147483647))));
  if (!(((t38) <= (t39)))) gg_fail("assertion failed");
  t0 = ((int32_t)(((int32_t)(v4))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_42(gg_slice p0, int64_t p1, uint16_t p2, int64_t base) {
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
  gg_slice t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t * t14;
  uint16_t t15;
  int64_t t16;
  gg_slice v0;
  int64_t v1;
  uint16_t v2;
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
  (*t9) = ((uint8_t)(((uint8_t)(v2))));
  t10 = v0;
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = ((int64_t)((uint64_t)(t11) + (uint64_t)(t12)));
  t14 = ((uint8_t *)t10.data) + gg_index(t13, t10.len);
  t15 = ((uint16_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(8))));
  (*t14) = ((uint8_t)(((uint8_t)(((t15) >> ((uint64_t)(t16) & 15))))));
  }
gg_done:;
}
static void gg_function_38(gg_type_31 p0, gg_slice p1, int64_t p2, float p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  gg_slice t3;
  int64_t t4;
  float t5;
  uint32_t t6;
  uint32_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  gg_slice t11;
  int64_t t12;
  float t13;
  uint16_t t14;
  uint16_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  float t19;
  uint32_t t20;
  uint32_t t21;
  uint32_t t22;
  uint32_t t23;
  uint32_t t24;
  uint8_t t25;
  uint32_t t26;
  int64_t t27;
  uint32_t t28;
  uint32_t t29;
  uint32_t t30;
  uint32_t t31;
  uint32_t t32;
  int64_t t33;
  uint32_t t34;
  uint32_t t35;
  uint32_t t36;
  uint32_t t37;
  uint32_t t38;
  int64_t t39;
  gg_slice t40;
  int64_t t41;
  uint16_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  gg_slice t46;
  int64_t t47;
  float t48;
  int32_t t49;
  uint32_t t50;
  int64_t t51;
  int64_t t52;
  uint8_t t53;
  gg_slice t54;
  int64_t t55;
  float t56;
  int32_t t57;
  uint16_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t * t70;
  float t71;
  int32_t t72;
  gg_type_31 v0;
  gg_slice v1;
  int64_t v2;
  float v3;
  uint32_t v4;
  uint16_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(((*(v0))).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  t3 = v1;
  t4 = ((int64_t)(v2));
  t5 = ((float)(v3));
  t6 = fg143_bits(t5);
  t7 = ((uint32_t)(t6));
  gg_function_40(t3, t4, t7, base + 0);
  }
  } else {
  t8 = ((int64_t)(((*(v0))).f2));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t11 = v1;
  t12 = ((int64_t)(v2));
  t13 = ((float)(v3));
  t14 = fg143_to_half(t13);
  t15 = ((uint16_t)(t14));
  gg_function_42(t11, t12, t15, base + 0);
  }
  } else {
  t16 = ((int64_t)(((*(v0))).f2));
  t17 = ((int64_t)(((int64_t)UINT64_C(30))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t19 = ((float)(v3));
  t20 = fg143_bits(t19);
  v4 = ((uint32_t)(t20));
  t21 = ((uint32_t)(v4));
  t22 = ((uint32_t)(((int64_t)UINT64_C(2147483647))));
  t23 = ((uint32_t)(((t21) & (t22))));
  t24 = ((uint32_t)(((int64_t)UINT64_C(2139095040))));
  t25 = ((t23) > (t24));
  if (t25) {
  {
  t26 = ((uint32_t)(v4));
  t27 = ((int64_t)(((int64_t)UINT64_C(16))));
  t28 = ((uint32_t)(((t26) >> ((uint64_t)(t27) & 31))));
  t29 = ((uint32_t)(((int64_t)UINT64_C(64))));
  v5 = ((uint16_t)(((uint16_t)(((t28) | (t29))))));
  }
  } else {
  {
  t30 = ((uint32_t)(v4));
  t31 = ((uint32_t)(((int64_t)UINT64_C(32767))));
  t32 = ((uint32_t)(v4));
  t33 = ((int64_t)(((int64_t)UINT64_C(16))));
  t34 = ((uint32_t)(((t32) >> ((uint64_t)(t33) & 31))));
  t35 = ((uint32_t)(((int64_t)UINT64_C(1))));
  t36 = ((uint32_t)(((t34) & (t35))));
  t37 = ((uint32_t)(((uint32_t)((uint64_t)(t31) + (uint64_t)(t36)))));
  t38 = ((uint32_t)(((uint32_t)((uint64_t)(t30) + (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)UINT64_C(16))));
  v5 = ((uint16_t)(((uint16_t)(((t38) >> ((uint64_t)(t39) & 31))))));
  }
  }
  t40 = v1;
  t41 = ((int64_t)(v2));
  t42 = ((uint16_t)(v5));
  gg_function_42(t40, t41, t42, base + 0);
  }
  } else {
  t43 = ((int64_t)(((*(v0))).f2));
  t44 = ((int64_t)(((int64_t)UINT64_C(26))));
  t45 = ((t43) == (t44));
  if (t45) {
  {
  t46 = v1;
  t47 = ((int64_t)(v2));
  t48 = ((float)(v3));
  t49 = gg_function_43(t48, base + 0);
  t50 = ((uint32_t)(((uint32_t)(t49))));
  gg_function_40(t46, t47, t50, base + 0);
  }
  } else {
  t51 = ((int64_t)(((*(v0))).f2));
  t52 = ((int64_t)(((int64_t)UINT64_C(25))));
  t53 = ((t51) == (t52));
  if (t53) {
  {
  t54 = v1;
  t55 = ((int64_t)(v2));
  t56 = ((float)(v3));
  t57 = gg_function_43(t56, base + 0);
  t58 = ((uint16_t)(((uint16_t)(t57))));
  gg_function_42(t54, t55, t58, base + 0);
  }
  } else {
  {
  t59 = ((int64_t)(((*(v0))).f2));
  t60 = ((int64_t)(((int64_t)UINT64_C(24))));
  t61 = ((t59) == (t60));
  t62 = t61;
  if (t62) {
  t63 = ((int64_t)(v2));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((t63) >= (t64));
  }
  t65 = t62;
  t66 = t65;
  if (t66) {
  t67 = ((int64_t)(v2));
  t68 = ((int64_t)((v1).len));
  t66 = ((t67) < (t68));
  }
  if (!(t66)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t69 = v2;
  t70 = ((uint8_t *)(v1).data) + gg_index(t69, (v1).len);
  t71 = ((float)(v3));
  t72 = gg_function_43(t71, base + 0);
  (*t70) = ((uint8_t)(((uint8_t)(t72))));
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_37(gg_type_31 p0, gg_slice p1, int64_t p2, float p3, int64_t base) {
  gg_type_31 t0;
  gg_slice t1;
  gg_type_31 t2;
  int64_t t3;
  int64_t t4;
  float t5;
  gg_type_31 v0;
  gg_slice v1;
  int64_t v2;
  float v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = v0;
  t1 = v1;
  t2 = v0;
  t3 = ((int64_t)(v2));
  t4 = gg_function_16(t2, t3, base + 0);
  t5 = ((float)(v3));
  gg_function_38(t0, t1, t4, t5, base + 0);
  }
gg_done:;
}
static uint8_t gg_function_58(gg_type_31 p0, gg_type_31 p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t * t4;
  int64_t t5;
  int64_t t6;
  int64_t * t7;
  int64_t t8;
  uint8_t t9;
  gg_type_31 v0;
  gg_type_31 v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t2 = ((int64_t)UINT64_C(4));
  t1 = 0;
label0:;
  if (t1 >= t2) goto label2;
  v2 = t1;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t3 = v2;
  t4 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t3, ((int64_t)UINT64_C(4)));
  t5 = ((int64_t)((*t4)));
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v2;
  t7 = ((int64_t *)(((*(v1))).f0).data) + gg_index(t6, ((int64_t)UINT64_C(4)));
  t8 = ((int64_t)((*t7)));
  t9 = ((t5) != (t8));
  if (t9) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  }
label1:;
  t1++; goto label0;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_95(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  uint8_t t13;
  gg_type_31 t14;
  gg_type_31 t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  gg_type_20 t24;
  int64_t t25;
  gg_type_31 t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  gg_type_31 t32;
  gg_type_31 t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  double t41;
  gg_type_31 t42;
  gg_slice t43;
  int64_t t44;
  float t45;
  float t46;
  gg_type_31 t47;
  gg_slice t48;
  int64_t t49;
  float t50;
  float t51;
  double t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  gg_type_31 t59;
  gg_slice t60;
  int64_t t61;
  gg_type_31 t62;
  gg_slice t63;
  int64_t t64;
  gg_type_31 t65;
  gg_slice t66;
  int64_t t67;
  int64_t t68;
  float * t69;
  float t70;
  float t71;
  float t72;
  float t73;
  float t74;
  float t75;
  float t76;
  int64_t t77;
  int64_t t78;
  int64_t * t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t * t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t * t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t * t91;
  int64_t t92;
  int64_t t93;
  int64_t * t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t * t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  float * t106;
  float t107;
  float t108;
  uint8_t t109;
  float t110;
  int64_t t111;
  float * t112;
  float t113;
  float t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  float t121;
  float t122;
  float t123;
  float t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint8_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  float t137;
  float t138;
  uint32_t t139;
  float t140;
  int64_t t141;
  int64_t t142;
  gg_type_31 t143;
  gg_slice t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  float t150;
  float t151;
  int64_t t152;
  float * t153;
  float t154;
  int64_t t155;
  int64_t * t156;
  int64_t t157;
  int64_t t158;
  uint8_t t159;
  int64_t t160;
  int64_t * t161;
  int64_t t162;
  int64_t t163;
  uint8_t t164;
  uint8_t t165;
  int64_t t166;
  int64_t * t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t * t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t * t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t * t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t * t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t * t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t * t197;
  int64_t t198;
  int64_t t199;
  float t200;
  float t201;
  gg_type_31 t202;
  gg_slice t203;
  int64_t t204;
  float t205;
  float t206;
  float t207;
  gg_slice t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  float * t212;
  float t213;
  float t214;
  uint8_t t215;
  int64_t t216;
  int64_t * t217;
  int64_t t218;
  int64_t t219;
  uint8_t t220;
  gg_type_31 t221;
  gg_slice t222;
  int64_t t223;
  float t224;
  float t225;
  uint8_t t226;
  int64_t t227;
  int64_t t228;
  gg_slice t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  float * t233;
  float t234;
  float t235;
  float t236;
  float t237;
  gg_slice t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  float * t242;
  double t243;
  double t244;
  int64_t t245;
  int64_t * t246;
  int64_t t247;
  int64_t t248;
  uint8_t t249;
  double t250;
  gg_type_31 t251;
  gg_slice t252;
  int64_t t253;
  float t254;
  float t255;
  float t256;
  float t257;
  float t258;
  double t259;
  double t260;
  double t261;
  double t262;
  double t263;
  int64_t t264;
  int64_t t265;
  gg_type_31 t266;
  gg_slice t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  gg_slice t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  float * t277;
  float t278;
  float t279;
  float t280;
  int64_t t281;
  int64_t t282;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  double v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  float v20;
  float v21;
  float v22;
  int64_t v23;
  float v24;
  int64_t v25;
  float v26;
  double v27;
  int64_t v28;
  float v29;
  float v30;
  int64_t v31;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(46))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f8));
  t6 = ((int64_t)(((int64_t)UINT64_C(47))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  v6 = ((int64_t)((*t9)));
  t10 = ((int64_t)(v6));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t10) > (t11));
  t13 = t12;
  if (t13) {
  t14 = &(((*(v0))).f4);
  t15 = &(((*(v0))).f7);
  t16 = gg_function_58(t14, t15, base + 0);
  t13 = t16;
  }
  if (!(t13)) gg_fail("assertion failed");
  t17 = ((int64_t)((v5).len));
  t18 = ((int64_t)(v6));
  t19 = ((int64_t)(((*(v0))).f10));
  t20 = gg_function_2(t18, t19, base + 0);
  t21 = ((int64_t)(t20));
  if (!(((t17) >= (t21)))) gg_fail("assertion failed");
  t22 = ((int64_t)(((*(v0))).f9));
  t23 = ((int64_t)(v6));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t24 = v0;
  v8 = gg_function_14(t24, base + 0);
label1:;
  t25 = ((int64_t)(v8));
  t26 = &(((*(v0))).f4);
  t27 = gg_function_13(t26, base + 0);
  t28 = ((int64_t)(t27));
  if (!(((t25) < (t28)))) goto label2;
  {
  t29 = ((int64_t)(((*(v0))).f8));
  t30 = ((int64_t)(((int64_t)UINT64_C(47))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = &(((*(v0))).f4);
  t33 = &(((*(v0))).f5);
  t34 = gg_function_58(t32, t33, base + 0);
  if (!(t34)) gg_fail("assertion failed");
  v9 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t36 = v6;
  t35 = 0;
label3:;
  if (t35 >= t36) goto label5;
  v10 = t35;
  {
  t37 = ((int64_t)(v8));
  t38 = ((int64_t)(v6));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(v10));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  t41 = v9;
  t42 = &(((*(v0))).f4);
  t43 = v1;
  t44 = ((int64_t)(v11));
  t45 = gg_function_51(t42, t43, t44, base + 0);
  t46 = ((float)(t45));
  t47 = &(((*(v0))).f5);
  t48 = v2;
  t49 = ((int64_t)(v11));
  t50 = gg_function_51(t47, t48, t49, base + 0);
  t51 = ((float)(t50));
  t52 = ((double)(((double)(((t46) * (t51))))));
  v9 = ((t41) + (t52));
  }
label4:;
  t35++; goto label3;
label5:;
  t54 = v6;
  t53 = 0;
label6:;
  if (t53 >= t54) goto label8;
  v12 = t53;
  {
  t55 = ((int64_t)(v8));
  t56 = ((int64_t)(v6));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t58 = ((int64_t)(v12));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t59 = &(((*(v0))).f4);
  t60 = v1;
  t61 = ((int64_t)(v13));
  v14 = gg_function_51(t59, t60, t61, base + 0);
  t62 = &(((*(v0))).f5);
  t63 = v2;
  t64 = ((int64_t)(v13));
  v15 = gg_function_51(t62, t63, t64, base + 0);
  t65 = &(((*(v0))).f7);
  t66 = v4;
  t67 = ((int64_t)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t68 = ((int64_t)UINT64_C(0));
  t69 = ((float *)(((*(v0))).f12).data) + gg_index(t68, ((int64_t)UINT64_C(8)));
  t70 = ((float)((*t69)));
  t71 = ((float)(v15));
  t72 = ((float)(((t70) * (t71))));
  t73 = ((float)(v14));
  t74 = ((float)(((float)(v9))));
  t75 = ((float)(((t73) - (t74))));
  t76 = ((float)(((t72) * (t75))));
  gg_function_37(t65, t66, t67, t76, base + 0);
  }
label7:;
  t53++; goto label6;
label8:;
  }
  } else {
  {
  t77 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t78 = ((int64_t)UINT64_C(1));
  t79 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t78, ((int64_t)UINT64_C(4)));
  t80 = ((int64_t)((*t79)));
  v16 = ((int64_t)(((int64_t)gg_mod(t77, t80))));
  t81 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t82 = ((int64_t)UINT64_C(1));
  t83 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t82, ((int64_t)UINT64_C(4)));
  t84 = ((int64_t)((*t83)));
  t85 = ((int64_t)(((int64_t)gg_div(t81, t84))));
  /* FG_DIRECT_SLICE_INDEX */
  t86 = ((int64_t)UINT64_C(2));
  t87 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t86, ((int64_t)UINT64_C(4)));
  t88 = ((int64_t)((*t87)));
  v17 = ((int64_t)(((int64_t)gg_mod(t85, t88))));
  t89 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t90 = ((int64_t)UINT64_C(1));
  t91 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t90, ((int64_t)UINT64_C(4)));
  t92 = ((int64_t)((*t91)));
  /* FG_DIRECT_SLICE_INDEX */
  t93 = ((int64_t)UINT64_C(2));
  t94 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t93, ((int64_t)UINT64_C(4)));
  t95 = ((int64_t)((*t94)));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t92) * (uint64_t)(t95)))));
  v18 = ((int64_t)(((int64_t)gg_div(t89, t96))));
  v19 = ((int64_t)(((int64_t)UINT64_C(1))));
label10:;
  t97 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t98 = ((int64_t)UINT64_C(2));
  t99 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t98, ((int64_t)UINT64_C(4)));
  t100 = ((int64_t)((*t99)));
  t101 = ((int64_t)(((int64_t)UINT64_C(2))));
  t102 = ((int64_t)(((int64_t)gg_div(t100, t101))));
  if (!(((t97) <= (t102)))) goto label11;
  {
  t103 = v19;
  t104 = ((int64_t)(((int64_t)UINT64_C(2))));
  v19 = ((int64_t)((uint64_t)(t103) * (uint64_t)(t104)));
  }
  goto label10;
label11:;
  v20 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  /* FG_DIRECT_SLICE_INDEX */
  t105 = ((int64_t)UINT64_C(1));
  t106 = ((float *)(((*(v0))).f12).data) + gg_index(t105, ((int64_t)UINT64_C(8)));
  t107 = ((float)((*t106)));
  t108 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t109 = ((t107) > (t108));
  if (t109) {
  {
  t110 = ((float)(((float)(((int64_t)UINT64_C(2))))));
  /* FG_DIRECT_SLICE_INDEX */
  t111 = ((int64_t)UINT64_C(1));
  t112 = ((float *)(((*(v0))).f12).data) + gg_index(t111, ((int64_t)UINT64_C(8)));
  t113 = (*t112);
  t114 = ((float)((-(t113))));
  t116 = ((int64_t)(v17));
  t117 = ((int64_t)(v19));
  t118 = ((t116) < (t117));
  if (t118) {
  {
  t115 = ((int64_t)(v19));
  }
  } else {
  {
  t119 = ((int64_t)(((int64_t)UINT64_C(2))));
  t120 = ((int64_t)(v19));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t119) * (uint64_t)(t120)))));
  }
  }
  t121 = ((float)(((float)(t115))));
  t122 = ((float)(((t114) / (t121))));
  t123 = powf(t110, t122);
  v21 = ((float)(t123));
  t124 = ((float)(v21));
  t126 = ((int64_t)(v17));
  t127 = ((int64_t)(v19));
  t128 = ((t126) < (t127));
  if (t128) {
  {
  t129 = ((int64_t)(v17));
  t130 = ((int64_t)(((int64_t)UINT64_C(1))));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t129) + (uint64_t)(t130)))));
  }
  } else {
  {
  t131 = ((int64_t)(((int64_t)UINT64_C(2))));
  t132 = ((int64_t)(v17));
  t133 = ((int64_t)(v19));
  t134 = ((int64_t)(((int64_t)((uint64_t)(t132) - (uint64_t)(t133)))));
  t135 = ((int64_t)(((int64_t)((uint64_t)(t131) * (uint64_t)(t134)))));
  t136 = ((int64_t)(((int64_t)UINT64_C(1))));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t135) + (uint64_t)(t136)))));
  }
  }
  t137 = ((float)(((float)(t125))));
  t138 = powf(t124, t137);
  v20 = ((float)(t138));
  }
  }
  t139 = ((uint32_t)(((int64_t)UINT64_C(4286578688))));
  t140 = fg143_from_bits(t139);
  v22 = ((float)(t140));
  t142 = v6;
  t141 = 0;
label12:;
  if (t141 >= t142) goto label14;
  v23 = t141;
  {
  t143 = &(((*(v0))).f4);
  t144 = v1;
  t145 = ((int64_t)(v8));
  t146 = ((int64_t)(v6));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t145) * (uint64_t)(t146)))));
  t148 = ((int64_t)(v23));
  t149 = ((int64_t)(((int64_t)((uint64_t)(t147) + (uint64_t)(t148)))));
  t150 = gg_function_51(t143, t144, t149, base + 0);
  t151 = ((float)(t150));
  /* FG_DIRECT_SLICE_INDEX */
  t152 = ((int64_t)UINT64_C(0));
  t153 = ((float *)(((*(v0))).f12).data) + gg_index(t152, ((int64_t)UINT64_C(8)));
  t154 = ((float)((*t153)));
  v24 = ((float)(((t151) * (t154))));
  /* FG_DIRECT_SLICE_INDEX */
  t155 = ((int64_t)UINT64_C(0));
  t156 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t155, ((int64_t)UINT64_C(16)));
  t157 = ((int64_t)((*t156)));
  t158 = ((int64_t)(((int64_t)UINT64_C(0))));
  t159 = ((t157) != (t158));
  if (t159) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t160 = ((int64_t)UINT64_C(0));
  t161 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t160, ((int64_t)UINT64_C(4)));
  t162 = ((int64_t)((*t161)));
  t163 = ((int64_t)(v6));
  t164 = ((t162) >= (t163));
  t165 = t164;
  if (t165) {
  /* FG_DIRECT_SLICE_INDEX */
  t166 = ((int64_t)UINT64_C(1));
  t167 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t166, ((int64_t)UINT64_C(4)));
  t168 = ((int64_t)((*t167)));
  t169 = ((int64_t)(v16));
  t165 = ((t168) > (t169));
  }
  if (!(t165)) gg_fail("assertion failed");
  t170 = ((int64_t)(v23));
  /* FG_DIRECT_SLICE_INDEX */
  t171 = ((int64_t)UINT64_C(0));
  t172 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t171, ((int64_t)UINT64_C(4)));
  t173 = ((int64_t)((*t172)));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t170) * (uint64_t)(t173)))));
  t175 = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t176 = ((int64_t)UINT64_C(1));
  t177 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t176, ((int64_t)UINT64_C(4)));
  t178 = ((int64_t)((*t177)));
  t179 = ((int64_t)(((int64_t)((uint64_t)(t175) * (uint64_t)(t178)))));
  t180 = ((int64_t)(((int64_t)((uint64_t)(t174) + (uint64_t)(t179)))));
  t181 = ((int64_t)(v17));
  /* FG_DIRECT_SLICE_INDEX */
  t182 = ((int64_t)UINT64_C(2));
  t183 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t182, ((int64_t)UINT64_C(4)));
  t184 = ((int64_t)((*t183)));
  t185 = ((int64_t)(((int64_t)gg_mod(t181, t184))));
  /* FG_DIRECT_SLICE_INDEX */
  t186 = ((int64_t)UINT64_C(2));
  t187 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t186, ((int64_t)UINT64_C(4)));
  t188 = ((int64_t)((*t187)));
  t189 = ((int64_t)(((int64_t)((uint64_t)(t185) * (uint64_t)(t188)))));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t180) + (uint64_t)(t189)))));
  t191 = ((int64_t)(v18));
  /* FG_DIRECT_SLICE_INDEX */
  t192 = ((int64_t)UINT64_C(3));
  t193 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t192, ((int64_t)UINT64_C(4)));
  t194 = ((int64_t)((*t193)));
  t195 = ((int64_t)(((int64_t)gg_mod(t191, t194))));
  /* FG_DIRECT_SLICE_INDEX */
  t196 = ((int64_t)UINT64_C(3));
  t197 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t196, ((int64_t)UINT64_C(4)));
  t198 = ((int64_t)((*t197)));
  t199 = ((int64_t)(((int64_t)((uint64_t)(t195) * (uint64_t)(t198)))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t190) + (uint64_t)(t199)))));
  t200 = v24;
  t201 = ((float)(v20));
  t202 = &(((*(v0))).f5);
  t203 = v2;
  t204 = ((int64_t)(v25));
  t205 = gg_function_47(t202, t203, t204, base + 0);
  t206 = ((float)(t205));
  t207 = ((float)(((t201) * (t206))));
  v24 = ((t200) + (t207));
  }
  }
  t208 = v5;
  t209 = ((int64_t)(v7));
  t210 = ((int64_t)(v23));
  t211 = ((int64_t)((uint64_t)(t209) + (uint64_t)(t210)));
  t212 = ((float *)t208.data) + gg_index(t211, t208.len);
  (*t212) = ((float)(v24));
  t213 = ((float)(v24));
  t214 = ((float)(v22));
  t215 = ((t213) > (t214));
  if (t215) {
  {
  v22 = ((float)(v24));
  }
  }
  }
label13:;
  t141++; goto label12;
label14:;
  /* FG_DIRECT_SLICE_INDEX */
  t216 = ((int64_t)UINT64_C(1));
  t217 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t216, ((int64_t)UINT64_C(16)));
  t218 = ((int64_t)((*t217)));
  t219 = ((int64_t)(((int64_t)UINT64_C(0))));
  t220 = ((t218) != (t219));
  if (t220) {
  {
  t221 = &(((*(v0))).f6);
  t222 = v3;
  t223 = ((int64_t)(v17));
  v26 = gg_function_51(t221, t222, t223, base + 0);
  t224 = ((float)(v26));
  t225 = ((float)(v22));
  t226 = ((t224) > (t225));
  if (t226) {
  {
  v22 = ((float)(v26));
  }
  }
  }
  }
  v27 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t228 = v6;
  t227 = 0;
label15:;
  if (t227 >= t228) goto label17;
  v28 = t227;
  {
  t229 = v5;
  t230 = ((int64_t)(v7));
  t231 = ((int64_t)(v28));
  t232 = ((int64_t)((uint64_t)(t230) + (uint64_t)(t231)));
  t233 = ((float *)t229.data) + gg_index(t232, t229.len);
  t234 = ((float)((*t233)));
  t235 = ((float)(v22));
  t236 = ((float)(((t234) - (t235))));
  t237 = expf(t236);
  v29 = ((float)(t237));
  t238 = v5;
  t239 = ((int64_t)(v7));
  t240 = ((int64_t)(v28));
  t241 = ((int64_t)((uint64_t)(t239) + (uint64_t)(t240)));
  t242 = ((float *)t238.data) + gg_index(t241, t238.len);
  (*t242) = ((float)(v29));
  t243 = v27;
  t244 = ((double)(((double)(v29))));
  v27 = ((t243) + (t244));
  }
label16:;
  t227++; goto label15;
label17:;
  /* FG_DIRECT_SLICE_INDEX */
  t245 = ((int64_t)UINT64_C(1));
  t246 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t245, ((int64_t)UINT64_C(16)));
  t247 = ((int64_t)((*t246)));
  t248 = ((int64_t)(((int64_t)UINT64_C(0))));
  t249 = ((t247) != (t248));
  if (t249) {
  {
  t250 = v27;
  t251 = &(((*(v0))).f6);
  t252 = v3;
  t253 = ((int64_t)(v17));
  t254 = gg_function_51(t251, t252, t253, base + 0);
  t255 = ((float)(t254));
  t256 = ((float)(v22));
  t257 = ((float)(((t255) - (t256))));
  t258 = expf(t257);
  t259 = ((double)(((double)(t258))));
  v27 = ((t250) + (t259));
  }
  }
  t260 = ((double)(v27));
  t261 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  if (!(((t260) > (t261)))) gg_fail("assertion failed");
  t262 = ((double)(((double)(((int64_t)UINT64_C(1))))));
  t263 = ((double)(v27));
  v30 = ((float)(((float)(((t262) / (t263))))));
  t265 = v6;
  t264 = 0;
label18:;
  if (t264 >= t265) goto label20;
  v31 = t264;
  {
  t266 = &(((*(v0))).f7);
  t267 = v4;
  t268 = ((int64_t)(v8));
  t269 = ((int64_t)(v6));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t268) * (uint64_t)(t269)))));
  t271 = ((int64_t)(v31));
  t272 = ((int64_t)(((int64_t)((uint64_t)(t270) + (uint64_t)(t271)))));
  t273 = v5;
  t274 = ((int64_t)(v7));
  t275 = ((int64_t)(v31));
  t276 = ((int64_t)((uint64_t)(t274) + (uint64_t)(t275)));
  t277 = ((float *)t273.data) + gg_index(t276, t273.len);
  t278 = ((float)((*t277)));
  t279 = ((float)(v30));
  t280 = ((float)(((t278) * (t279))));
  gg_function_37(t266, t267, t272, t280, base + 0);
  }
label19:;
  t264++; goto label18;
label20:;
  }
  }
  t281 = v8;
  t282 = ((int64_t)(((*(v0))).f10));
  v8 = ((int64_t)((uint64_t)(t281) + (uint64_t)(t282)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_54(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t base) {
  /* FG_SHARED_BODY:47 */
  return gg_function_47(p0, p1, p2, base);
}
static float gg_function_53(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t base) {
  /* FG_SHARED_BODY:51 */
  return gg_function_51(p0, p1, p2, base);
}
static int64_t gg_function_94(int64_t p0, int64_t p1, int64_t base) {
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
static uint8_t gg_function_93(gg_type_20 p0, gg_slice p1, gg_slice p2, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  gg_type_31 t4;
  gg_type_31 t5;
  uint8_t t6;
  int64_t t7;
  int64_t * t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t * t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  gg_type_20 t19;
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
  int64_t * t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t * t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t * t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t * t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t * t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t * t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t * t62;
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
  double t74;
  gg_type_31 t75;
  gg_slice t76;
  int64_t t77;
  float t78;
  double t79;
  double t80;
  double t81;
  double t82;
  double t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t * t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t * t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t * t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t * t102;
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
  gg_type_31 t114;
  gg_slice t115;
  int64_t t116;
  float t117;
  float t118;
  float t119;
  double t120;
  float t121;
  float t122;
  double t123;
  gg_type_31 t124;
  gg_slice t125;
  int64_t t126;
  float t127;
  double t128;
  double t129;
  float t130;
  double t131;
  double t132;
  float t133;
  int64_t t134;
  float * t135;
  float t136;
  float t137;
  float t138;
  float t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t * t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t * t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t * t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t * t158;
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
  gg_type_31 t170;
  gg_slice t171;
  int64_t t172;
  gg_type_31 t173;
  gg_slice t174;
  int64_t t175;
  float t176;
  float t177;
  float t178;
  float t179;
  int64_t t180;
  int64_t t181;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  double v11;
  int64_t v12;
  int64_t v13;
  double v14;
  int64_t v15;
  int64_t v16;
  float v17;
  double v18;
  int64_t v19;
  int64_t v20;
  double v21;
  int64_t v22;
  int64_t v23;
  float v24;
  float v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(27))));
  t3 = ((t1) != (t2));
  if (t3) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t4 = &(((*(v0))).f4);
  t5 = &(((*(v0))).f7);
  t6 = gg_function_58(t4, t5, base + 0);
  if (!(t6)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t7 = ((int64_t)UINT64_C(0));
  t8 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t7, ((int64_t)UINT64_C(16)));
  v3 = ((int64_t)((*t8)));
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t9) > (t10)))) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t11 = ((int64_t)UINT64_C(2));
  t12 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t11, ((int64_t)UINT64_C(4)));
  v4 = ((int64_t)((*t12)));
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) - (uint64_t)(t16)))));
  t18 = ((int64_t)(v3));
  v5 = ((int64_t)(((int64_t)gg_div(t17, t18))));
  t19 = v0;
  v6 = gg_function_14(t19, base + 0);
label1:;
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(v3));
  if (!(((t20) < (t21)))) goto label2;
  {
  t22 = ((int64_t)(v6));
  t23 = ((int64_t)(v5));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t24 = ((int64_t)(v7));
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = ((int64_t)(v4));
  v8 = gg_function_94(t26, t27, base + 0);
  t28 = ((int64_t)(v8));
  t29 = ((int64_t)(v7));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) - (uint64_t)(t29)))));
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = ((int64_t)((*t32)));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t33)))));
  /* FG_DIRECT_SLICE_INDEX */
  t35 = ((int64_t)UINT64_C(1));
  t36 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t35, ((int64_t)UINT64_C(4)));
  t37 = ((int64_t)((*t36)));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t37)))));
  t38 = ((int64_t)(v9));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t38) > (t39)))) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t41 = ((int64_t)UINT64_C(3));
  t42 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t41, ((int64_t)UINT64_C(4)));
  t43 = (*t42);
  t40 = 0;
label3:;
  if (t40 >= t43) goto label5;
  v10 = t40;
  {
  v11 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t45 = ((int64_t)(v8));
  t46 = ((int64_t)(v7));
  t47 = ((int64_t)((uint64_t)(t45) - (uint64_t)(t46)));
  t44 = 0;
label6:;
  if (t44 >= t47) goto label8;
  v12 = t44;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t49 = ((int64_t)UINT64_C(1));
  t50 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t49, ((int64_t)UINT64_C(4)));
  t51 = (*t50);
  t48 = 0;
label9:;
  if (t48 >= t51) goto label11;
  v13 = t48;
  {
  v14 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  /* FG_DIRECT_SLICE_INDEX */
  t53 = ((int64_t)UINT64_C(0));
  t54 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t53, ((int64_t)UINT64_C(4)));
  t55 = (*t54);
  t52 = 0;
label12:;
  if (t52 >= t55) goto label14;
  v15 = t52;
  {
  t56 = ((int64_t)(v15));
  /* FG_DIRECT_SLICE_INDEX */
  t57 = ((int64_t)UINT64_C(0));
  t58 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t57, ((int64_t)UINT64_C(4)));
  t59 = ((int64_t)((*t58)));
  t60 = ((int64_t)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t61 = ((int64_t)UINT64_C(1));
  t62 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t61, ((int64_t)UINT64_C(4)));
  t63 = ((int64_t)((*t62)));
  t64 = ((int64_t)(v12));
  t65 = ((int64_t)(v7));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
  t67 = ((int64_t)(v4));
  t68 = ((int64_t)(v10));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t69)))));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t70)))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t72)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t73)))));
  t74 = v14;
  t75 = &(((*(v0))).f4);
  t76 = v1;
  t77 = ((int64_t)(v16));
  t78 = gg_function_51(t75, t76, t77, base + 0);
  t79 = ((double)(((double)(t78))));
  v14 = ((t74) + (t79));
  }
label13:;
  t52++; goto label12;
label14:;
  t80 = v11;
  t81 = ((double)(v14));
  v11 = ((t80) + (t81));
  }
label10:;
  t48++; goto label9;
label11:;
  }
label7:;
  t44++; goto label6;
label8:;
  t82 = ((double)(v11));
  t83 = ((double)(((double)(v9))));
  v17 = ((float)(((float)(((t82) / (t83))))));
  v18 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t85 = ((int64_t)(v8));
  t86 = ((int64_t)(v7));
  t87 = ((int64_t)((uint64_t)(t85) - (uint64_t)(t86)));
  t84 = 0;
label15:;
  if (t84 >= t87) goto label17;
  v19 = t84;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t89 = ((int64_t)UINT64_C(1));
  t90 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t89, ((int64_t)UINT64_C(4)));
  t91 = (*t90);
  t88 = 0;
label18:;
  if (t88 >= t91) goto label20;
  v20 = t88;
  {
  v21 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  /* FG_DIRECT_SLICE_INDEX */
  t93 = ((int64_t)UINT64_C(0));
  t94 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t93, ((int64_t)UINT64_C(4)));
  t95 = (*t94);
  t92 = 0;
label21:;
  if (t92 >= t95) goto label23;
  v22 = t92;
  {
  t96 = ((int64_t)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t97 = ((int64_t)UINT64_C(0));
  t98 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t97, ((int64_t)UINT64_C(4)));
  t99 = ((int64_t)((*t98)));
  t100 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t101 = ((int64_t)UINT64_C(1));
  t102 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t101, ((int64_t)UINT64_C(4)));
  t103 = ((int64_t)((*t102)));
  t104 = ((int64_t)(v19));
  t105 = ((int64_t)(v7));
  t106 = ((int64_t)(((int64_t)((uint64_t)(t104) + (uint64_t)(t105)))));
  t107 = ((int64_t)(v4));
  t108 = ((int64_t)(v10));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t107) * (uint64_t)(t108)))));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t106) + (uint64_t)(t109)))));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t110)))));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t111)))));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t112)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t113)))));
  t114 = &(((*(v0))).f4);
  t115 = v1;
  t116 = ((int64_t)(v23));
  t117 = gg_function_51(t114, t115, t116, base + 0);
  t118 = ((float)(t117));
  t119 = ((float)(v17));
  v24 = ((float)(((t118) - (t119))));
  t120 = v21;
  t121 = ((float)(v24));
  t122 = ((float)(v24));
  t123 = ((double)(((double)(((t121) * (t122))))));
  v21 = ((t120) + (t123));
  t124 = &(((*(v0))).f7);
  t125 = v2;
  t126 = ((int64_t)(v23));
  t127 = ((float)(v24));
  gg_function_37(t124, t125, t126, t127, base + 0);
  }
label22:;
  t92++; goto label21;
label23:;
  t128 = v18;
  t129 = ((double)(v21));
  v18 = ((t128) + (t129));
  }
label19:;
  t88++; goto label18;
label20:;
  }
label16:;
  t84++; goto label15;
label17:;
  t130 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t131 = ((double)(v18));
  t132 = ((double)(((double)(v9))));
  t133 = ((float)(((float)(((t131) / (t132))))));
  /* FG_DIRECT_SLICE_INDEX */
  t134 = ((int64_t)UINT64_C(0));
  t135 = ((float *)(((*(v0))).f12).data) + gg_index(t134, ((int64_t)UINT64_C(8)));
  t136 = ((float)((*t135)));
  t137 = ((float)(((t133) + (t136))));
  t138 = sqrtf(t137);
  t139 = ((float)(t138));
  v25 = ((float)(((t130) / (t139))));
  t141 = ((int64_t)(v8));
  t142 = ((int64_t)(v7));
  t143 = ((int64_t)((uint64_t)(t141) - (uint64_t)(t142)));
  t140 = 0;
label24:;
  if (t140 >= t143) goto label26;
  v26 = t140;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t145 = ((int64_t)UINT64_C(1));
  t146 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t145, ((int64_t)UINT64_C(4)));
  t147 = (*t146);
  t144 = 0;
label27:;
  if (t144 >= t147) goto label29;
  v27 = t144;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t149 = ((int64_t)UINT64_C(0));
  t150 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t149, ((int64_t)UINT64_C(4)));
  t151 = (*t150);
  t148 = 0;
label30:;
  if (t148 >= t151) goto label32;
  v28 = t148;
  {
  t152 = ((int64_t)(v28));
  /* FG_DIRECT_SLICE_INDEX */
  t153 = ((int64_t)UINT64_C(0));
  t154 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t153, ((int64_t)UINT64_C(4)));
  t155 = ((int64_t)((*t154)));
  t156 = ((int64_t)(v27));
  /* FG_DIRECT_SLICE_INDEX */
  t157 = ((int64_t)UINT64_C(1));
  t158 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t157, ((int64_t)UINT64_C(4)));
  t159 = ((int64_t)((*t158)));
  t160 = ((int64_t)(v26));
  t161 = ((int64_t)(v7));
  t162 = ((int64_t)(((int64_t)((uint64_t)(t160) + (uint64_t)(t161)))));
  t163 = ((int64_t)(v4));
  t164 = ((int64_t)(v10));
  t165 = ((int64_t)(((int64_t)((uint64_t)(t163) * (uint64_t)(t164)))));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t162) + (uint64_t)(t165)))));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t159) * (uint64_t)(t166)))));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t156) + (uint64_t)(t167)))));
  t169 = ((int64_t)(((int64_t)((uint64_t)(t155) * (uint64_t)(t168)))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t152) + (uint64_t)(t169)))));
  t170 = &(((*(v0))).f7);
  t171 = v2;
  t172 = ((int64_t)(v29));
  t173 = &(((*(v0))).f7);
  t174 = v2;
  t175 = ((int64_t)(v29));
  t176 = gg_function_51(t173, t174, t175, base + 0);
  t177 = ((float)(t176));
  t178 = ((float)(v25));
  t179 = ((float)(((t177) * (t178))));
  gg_function_37(t170, t171, t172, t179, base + 0);
  }
label31:;
  t148++; goto label30;
label32:;
  }
label28:;
  t144++; goto label27;
label29:;
  }
label25:;
  t140++; goto label24;
label26:;
  }
label4:;
  t40++; goto label3;
label5:;
  t180 = v6;
  t181 = ((int64_t)(((*(v0))).f10));
  v6 = ((int64_t)((uint64_t)(t180) + (uint64_t)(t181)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_46(gg_type_31 p0, gg_type_31 p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t * t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t * t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t * t16;
  int64_t t17;
  int64_t t18;
  int64_t * t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t * t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t * t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t * t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t * t38;
  int64_t t39;
  int64_t t40;
  gg_type_31 v0;
  gg_type_31 v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((int64_t)(v2));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)UINT64_C(4));
  t1 = 0;
label0:;
  if (t1 >= t2) goto label2;
  v5 = t1;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t3 = v5;
  t4 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t3, ((int64_t)UINT64_C(4)));
  t5 = ((int64_t)((*t4)));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) > (t6));
  t8 = t7;
  if (t8) {
  /* FG_DIRECT_SLICE_INDEX */
  t9 = v5;
  t10 = ((int64_t *)(((*(v1))).f0).data) + gg_index(t9, ((int64_t)UINT64_C(4)));
  t11 = ((int64_t)((*t10)));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t11) > (t12));
  }
  t13 = t8;
  t14 = t13;
  if (t14) {
  /* FG_DIRECT_SLICE_INDEX */
  t15 = v5;
  t16 = ((int64_t *)(((*(v1))).f0).data) + gg_index(t15, ((int64_t)UINT64_C(4)));
  t17 = ((int64_t)((*t16)));
  /* FG_DIRECT_SLICE_INDEX */
  t18 = v5;
  t19 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t18, ((int64_t)UINT64_C(4)));
  t20 = ((int64_t)((*t19)));
  t21 = ((int64_t)(((int64_t)gg_mod(t17, t20))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t21) == (t22));
  }
  if (!(t14)) gg_fail("assertion failed");
  t23 = ((int64_t)(v3));
  /* FG_DIRECT_SLICE_INDEX */
  t24 = v5;
  t25 = ((int64_t *)(((*(v1))).f0).data) + gg_index(t24, ((int64_t)UINT64_C(4)));
  t26 = ((int64_t)((*t25)));
  v6 = ((int64_t)(((int64_t)gg_mod(t23, t26))));
  t27 = v3;
  /* FG_DIRECT_SLICE_INDEX */
  t28 = v5;
  t29 = ((int64_t *)(((*(v1))).f0).data) + gg_index(t28, ((int64_t)UINT64_C(4)));
  t30 = ((int64_t)((*t29)));
  v3 = ((int64_t)gg_div(t27, t30));
  t31 = ((int64_t)(v4));
  t32 = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t33 = v5;
  t34 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t33, ((int64_t)UINT64_C(4)));
  t35 = ((int64_t)((*t34)));
  t36 = ((int64_t)(((int64_t)gg_mod(t32, t35))));
  /* FG_DIRECT_SLICE_INDEX */
  t37 = v5;
  t38 = ((int64_t *)(((*(v0))).f1).data) + gg_index(t37, ((int64_t)UINT64_C(4)));
  t39 = ((int64_t)((*t38)));
  t40 = gg_function_2(t36, t39, base + 0);
  v4 = gg_function_5(t31, t40, base + 0);
  }
label1:;
  t1++; goto label0;
label2:;
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_92(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  gg_type_31 t16;
  gg_type_31 t17;
  uint8_t t18;
  int64_t t19;
  int64_t * t20;
  int64_t t21;
  int64_t t22;
  gg_type_20 t23;
  int64_t t24;
  gg_type_31 t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  gg_type_31 t30;
  gg_slice t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  double t37;
  double t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  gg_type_31 t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  double t49;
  float t50;
  float t51;
  double t52;
  double t53;
  float t54;
  float t55;
  double t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  double t60;
  double t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  gg_type_31 t67;
  gg_slice t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  float t74;
  float t75;
  float t76;
  double t77;
  float t78;
  float t79;
  double t80;
  int64_t t81;
  int64_t t82;
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  float t87;
  float t88;
  double t89;
  double t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  float t94;
  float t95;
  float t96;
  float t97;
  int64_t t98;
  float * t99;
  float t100;
  float t101;
  float t102;
  float t103;
  float t104;
  int64_t t105;
  float * t106;
  float t107;
  float t108;
  float t109;
  float t110;
  int64_t t111;
  int64_t t112;
  gg_type_31 t113;
  gg_slice t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  uint8_t t122;
  float t123;
  gg_type_31 t124;
  gg_slice t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  float t131;
  float t132;
  float t133;
  float t134;
  float t135;
  int64_t t136;
  float * t137;
  float t138;
  float t139;
  float t140;
  float t141;
  float t142;
  float t143;
  float t144;
  float t145;
  float t146;
  float t147;
  float t148;
  int64_t t149;
  int64_t * t150;
  int64_t t151;
  int64_t t152;
  uint8_t t153;
  uint8_t t154;
  int64_t t155;
  int64_t t156;
  uint8_t t157;
  float t158;
  gg_type_31 t159;
  gg_slice t160;
  gg_type_31 t161;
  gg_type_31 t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  float t169;
  float t170;
  gg_type_31 t171;
  gg_slice t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  float t178;
  int64_t t179;
  int64_t t180;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  double v6;
  double v7;
  double v8;
  int64_t v9;
  float v10;
  float v11;
  float v12;
  int64_t v13;
  float v14;
  float v15;
  float v16;
  int64_t v17;
  float v18;
  float v19;
  float v20;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(24))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f8));
  t6 = ((int64_t)(((int64_t)UINT64_C(25))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f8));
  t10 = ((int64_t)(((int64_t)UINT64_C(28))));
  t8 = ((t9) != (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(((*(v0))).f8));
  t14 = ((int64_t)(((int64_t)UINT64_C(26))));
  t12 = ((t13) != (t14));
  }
  t15 = t12;
  if (t15) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t16 = &(((*(v0))).f4);
  t17 = &(((*(v0))).f7);
  t18 = gg_function_58(t16, t17, base + 0);
  if (!(t18)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t19 = ((int64_t)UINT64_C(0));
  t20 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t19, ((int64_t)UINT64_C(4)));
  v4 = ((int64_t)((*t20)));
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t21) > (t22)))) gg_fail("assertion failed");
  t23 = v0;
  v5 = gg_function_14(t23, base + 0);
label1:;
  t24 = ((int64_t)(v5));
  t25 = &(((*(v0))).f4);
  t26 = gg_function_13(t25, base + 0);
  t27 = ((int64_t)(t26));
  if (!(((t24) < (t27)))) goto label2;
  {
  v6 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  v7 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  v8 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t29 = v4;
  t28 = 0;
label3:;
  if (t28 >= t29) goto label5;
  v9 = t28;
  {
  t30 = &(((*(v0))).f4);
  t31 = v1;
  t32 = ((int64_t)(v5));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((int64_t)(v9));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  v10 = gg_function_51(t30, t31, t36, base + 0);
  t37 = v6;
  t38 = ((double)(((double)(v10))));
  v6 = ((t37) + (t38));
  t39 = ((int64_t)(((*(v0))).f8));
  t40 = ((int64_t)(((int64_t)UINT64_C(26))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  t42 = &(((*(v0))).f5);
  t43 = v2;
  t44 = ((int64_t)(v5));
  t45 = ((int64_t)(v4));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t47 = ((int64_t)(v9));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  v11 = gg_function_51(t42, t43, t48, base + 0);
  t49 = v7;
  t50 = ((float)(v11));
  t51 = ((float)(v11));
  t52 = ((double)(((double)(((t50) * (t51))))));
  v7 = ((t49) + (t52));
  t53 = v8;
  t54 = ((float)(v11));
  t55 = ((float)(v10));
  t56 = ((double)(((double)(((t54) * (t55))))));
  v8 = ((t53) + (t56));
  }
  }
  }
label4:;
  t28++; goto label3;
label5:;
  t57 = ((int64_t)(((*(v0))).f8));
  t58 = ((int64_t)(((int64_t)UINT64_C(24))));
  t59 = ((t57) == (t58));
  if (t59) {
  {
  t60 = ((double)(v6));
  t61 = ((double)(((double)(v4))));
  v12 = ((float)(((float)(((t60) / (t61))))));
  }
  } else {
  {
  v12 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  }
  t62 = ((int64_t)(((*(v0))).f8));
  t63 = ((int64_t)(((int64_t)UINT64_C(26))));
  t64 = ((t62) != (t63));
  if (t64) {
  {
  t66 = v4;
  t65 = 0;
label6:;
  if (t65 >= t66) goto label8;
  v13 = t65;
  {
  t67 = &(((*(v0))).f4);
  t68 = v1;
  t69 = ((int64_t)(v5));
  t70 = ((int64_t)(v4));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((int64_t)(v13));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
  t74 = gg_function_51(t67, t68, t73, base + 0);
  t75 = ((float)(t74));
  t76 = ((float)(v12));
  v14 = ((float)(((t75) - (t76))));
  t77 = v7;
  t78 = ((float)(v14));
  t79 = ((float)(v14));
  t80 = ((double)(((double)(((t78) * (t79))))));
  v7 = ((t77) + (t80));
  }
label7:;
  t65++; goto label6;
label8:;
  }
  }
  t81 = ((int64_t)(((*(v0))).f8));
  t82 = ((int64_t)(((int64_t)UINT64_C(28))));
  t83 = ((t81) == (t82));
  if (t83) {
  {
  v15 = ((float)(((float)(v7))));
  }
  } else {
  t84 = ((int64_t)(((*(v0))).f8));
  t85 = ((int64_t)(((int64_t)UINT64_C(26))));
  t86 = ((t84) == (t85));
  if (t86) {
  {
  t87 = ((float)(((float)(v7))));
  t88 = ((float)(((float)(v4))));
  v15 = ((float)(((t87) / (t88))));
  }
  } else {
  {
  t89 = ((double)(v7));
  t90 = ((double)(((double)(v4))));
  v15 = ((float)(((float)(((t89) / (t90))))));
  }
  }
  }
  t91 = ((int64_t)(((*(v0))).f8));
  t92 = ((int64_t)(((int64_t)UINT64_C(28))));
  t93 = ((t91) == (t92));
  if (t93) {
  {
  t94 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t95 = ((float)(v15));
  t96 = sqrtf(t95);
  t97 = ((float)(t96));
  /* FG_DIRECT_SLICE_INDEX */
  t98 = ((int64_t)UINT64_C(0));
  t99 = ((float *)(((*(v0))).f12).data) + gg_index(t98, ((int64_t)UINT64_C(8)));
  t100 = ((float)((*t99)));
  t101 = fmaxf(t97, t100);
  t102 = ((float)(t101));
  v16 = ((float)(((t94) / (t102))));
  }
  } else {
  {
  t103 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t104 = ((float)(v15));
  /* FG_DIRECT_SLICE_INDEX */
  t105 = ((int64_t)UINT64_C(0));
  t106 = ((float *)(((*(v0))).f12).data) + gg_index(t105, ((int64_t)UINT64_C(8)));
  t107 = ((float)((*t106)));
  t108 = ((float)(((t104) + (t107))));
  t109 = sqrtf(t108);
  t110 = ((float)(t109));
  v16 = ((float)(((t103) / (t110))));
  }
  }
  t112 = v4;
  t111 = 0;
label9:;
  if (t111 >= t112) goto label11;
  v17 = t111;
  {
  t113 = &(((*(v0))).f4);
  t114 = v1;
  t115 = ((int64_t)(v5));
  t116 = ((int64_t)(v4));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) * (uint64_t)(t116)))));
  t118 = ((int64_t)(v17));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) + (uint64_t)(t118)))));
  v18 = gg_function_51(t113, t114, t119, base + 0);
  t120 = ((int64_t)(((*(v0))).f8));
  t121 = ((int64_t)(((int64_t)UINT64_C(26))));
  t122 = ((t120) == (t121));
  if (t122) {
  {
  t123 = ((float)(v18));
  t124 = &(((*(v0))).f5);
  t125 = v2;
  t126 = ((int64_t)(v5));
  t127 = ((int64_t)(v4));
  t128 = ((int64_t)(((int64_t)((uint64_t)(t126) * (uint64_t)(t127)))));
  t129 = ((int64_t)(v17));
  t130 = ((int64_t)(((int64_t)((uint64_t)(t128) + (uint64_t)(t129)))));
  t131 = gg_function_51(t124, t125, t130, base + 0);
  t132 = ((float)(t131));
  t133 = ((float)(((float)(v8))));
  t134 = ((float)(((t132) * (t133))));
  t135 = ((float)(((float)(v7))));
  /* FG_DIRECT_SLICE_INDEX */
  t136 = ((int64_t)UINT64_C(0));
  t137 = ((float *)(((*(v0))).f12).data) + gg_index(t136, ((int64_t)UINT64_C(8)));
  t138 = ((float)((*t137)));
  t139 = ((float)(((float)(v4))));
  t140 = ((float)(((t138) * (t139))));
  t141 = ((float)(((t135) + (t140))));
  t142 = ((float)(((t134) / (t141))));
  t143 = ((float)(((t123) - (t142))));
  t144 = ((float)(v16));
  v19 = ((float)(((t143) * (t144))));
  }
  } else {
  {
  t145 = ((float)(v18));
  t146 = ((float)(v12));
  t147 = ((float)(((t145) - (t146))));
  t148 = ((float)(v16));
  v19 = ((float)(((t147) * (t148))));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t149 = ((int64_t)UINT64_C(14));
  t150 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t149, ((int64_t)UINT64_C(16)));
  t151 = ((int64_t)((*t150)));
  t152 = ((int64_t)(((int64_t)UINT64_C(0))));
  t153 = ((t151) != (t152));
  t154 = t153;
  if (t154) {
  t155 = ((int64_t)(((*(v0))).f8));
  t156 = ((int64_t)(((int64_t)UINT64_C(25))));
  t154 = ((t155) == (t156));
  }
  t157 = t154;
  if (t157) {
  {
  t158 = ((float)(v19));
  t159 = &(((*(v0))).f5);
  t160 = v2;
  t161 = &(((*(v0))).f5);
  t162 = &(((*(v0))).f7);
  t163 = ((int64_t)(v5));
  t164 = ((int64_t)(v4));
  t165 = ((int64_t)(((int64_t)((uint64_t)(t163) * (uint64_t)(t164)))));
  t166 = ((int64_t)(v17));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t165) + (uint64_t)(t166)))));
  t168 = gg_function_46(t161, t162, t167, base + 0);
  t169 = gg_function_47(t159, t160, t168, base + 0);
  t170 = ((float)(t169));
  v20 = ((float)(((t158) * (t170))));
  }
  } else {
  {
  v20 = ((float)(v19));
  }
  }
  t171 = &(((*(v0))).f7);
  t172 = v3;
  t173 = ((int64_t)(v5));
  t174 = ((int64_t)(v4));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) * (uint64_t)(t174)))));
  t176 = ((int64_t)(v17));
  t177 = ((int64_t)(((int64_t)((uint64_t)(t175) + (uint64_t)(t176)))));
  t178 = ((float)(v20));
  gg_function_37(t171, t172, t177, t178, base + 0);
  }
label10:;
  t111++; goto label9;
label11:;
  t179 = v5;
  t180 = ((int64_t)(((*(v0))).f10));
  v5 = ((int64_t)((uint64_t)(t179) + (uint64_t)(t180)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_91(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  gg_type_31 t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t * t36;
  int64_t t37;
  double t38;
  gg_type_31 t39;
  gg_slice t40;
  int64_t t41;
  int64_t t42;
  int64_t * t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  float t48;
  double t49;
  double t50;
  double t51;
  gg_type_31 t52;
  gg_slice t53;
  int64_t t54;
  float t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  uint8_t t66;
  gg_type_31 t67;
  gg_type_31 t68;
  uint8_t t69;
  int64_t t70;
  int64_t t71;
  uint8_t t72;
  int64_t t73;
  gg_type_31 t74;
  int64_t t75;
  int64_t t76;
  gg_slice t77;
  gg_type_31 t78;
  int64_t t79;
  int64_t t80;
  uint32_t t81;
  uint32_t t82;
  gg_slice t83;
  gg_type_31 t84;
  int64_t t85;
  int64_t t86;
  uint32_t t87;
  uint32_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t * t97;
  uint64_t t98;
  int64_t t99;
  int64_t t100;
  uint64_t t101;
  gg_type_20 t102;
  int64_t t103;
  int64_t * t104;
  int64_t t105;
  gg_type_31 t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  uint8_t t111;
  int64_t t112;
  int64_t t113;
  gg_type_31 t114;
  gg_slice t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  gg_type_31 t121;
  gg_slice t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  float t128;
  float t129;
  uint8_t t130;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  uint8_t t134;
  int64_t t135;
  int64_t t136;
  gg_slice t137;
  gg_type_31 t138;
  int64_t t139;
  int64_t t140;
  uint32_t t141;
  int64_t t142;
  int64_t t143;
  uint8_t t144;
  int64_t t145;
  int64_t t146;
  float t147;
  gg_type_31 t148;
  gg_slice t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  float t155;
  float t156;
  gg_type_31 t157;
  gg_slice t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  float t164;
  int64_t t165;
  int64_t t166;
  double t167;
  gg_type_31 t168;
  gg_slice t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  float t175;
  double t176;
  int64_t t177;
  int64_t t178;
  uint8_t t179;
  double t180;
  double t181;
  gg_type_31 t182;
  gg_slice t183;
  int64_t t184;
  float t185;
  int64_t t186;
  int64_t t187;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  double v4;
  int64_t v5;
  double v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  uint64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  double v20;
  int64_t v21;
  float v22;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(14))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f8));
  t6 = ((int64_t)(((int64_t)UINT64_C(15))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f8));
  t10 = ((int64_t)(((int64_t)UINT64_C(17))));
  t8 = ((t9) != (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(((*(v0))).f8));
  t14 = ((int64_t)(((int64_t)UINT64_C(16))));
  t12 = ((t13) != (t14));
  }
  t15 = t12;
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(((*(v0))).f8));
  t18 = ((int64_t)(((int64_t)UINT64_C(18))));
  t16 = ((t17) != (t18));
  }
  t19 = t16;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(((*(v0))).f8));
  t22 = ((int64_t)(((int64_t)UINT64_C(19))));
  t20 = ((t21) != (t22));
  }
  t23 = t20;
  if (t23) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t24 = ((int64_t)(((*(v0))).f8));
  t25 = ((int64_t)(((int64_t)UINT64_C(14))));
  t26 = ((t24) == (t25));
  if (t26) {
  {
  t27 = ((int64_t)(((*(v0))).f9));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t27) == (t28));
  if (t29) {
  {
  v4 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t31 = &(((*(v0))).f4);
  t32 = gg_function_13(t31, base + 0);
  t33 = t32;
  t30 = 0;
label0:;
  if (t30 >= t33) goto label2;
  v5 = t30;
  {
  v6 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  /* FG_DIRECT_SLICE_INDEX */
  t35 = ((int64_t)UINT64_C(0));
  t36 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t35, ((int64_t)UINT64_C(4)));
  t37 = (*t36);
  t34 = 0;
label3:;
  if (t34 >= t37) goto label5;
  v7 = t34;
  {
  t38 = v6;
  t39 = &(((*(v0))).f4);
  t40 = v1;
  t41 = ((int64_t)(v5));
  /* FG_DIRECT_SLICE_INDEX */
  t42 = ((int64_t)UINT64_C(0));
  t43 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t42, ((int64_t)UINT64_C(4)));
  t44 = ((int64_t)((*t43)));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t44)))));
  t46 = ((int64_t)(v7));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  t48 = gg_function_51(t39, t40, t47, base + 0);
  t49 = ((double)(((double)(t48))));
  v6 = ((t38) + (t49));
  }
label4:;
  t34++; goto label3;
label5:;
  t50 = v4;
  t51 = ((double)(v6));
  v4 = ((t50) + (t51));
  }
label1:;
  t30++; goto label0;
label2:;
  t52 = &(((*(v0))).f7);
  t53 = v3;
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((float)(((float)(v4))));
  gg_function_37(t52, t53, t54, t55, base + 0);
  }
  }
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t56 = ((int64_t)(((*(v0))).f8));
  t57 = ((int64_t)(((int64_t)UINT64_C(19))));
  t58 = ((t56) == (t57));
  if (t58) {
  {
  t59 = ((int64_t)((((*(v0))).f4).f2));
  t60 = ((int64_t)(((int64_t)UINT64_C(26))));
  t61 = ((t59) == (t60));
  t62 = t61;
  if (t62) {
  t63 = ((int64_t)((((*(v0))).f5).f2));
  t64 = ((int64_t)(((int64_t)UINT64_C(26))));
  t62 = ((t63) == (t64));
  }
  t65 = t62;
  t66 = t65;
  if (t66) {
  t67 = &(((*(v0))).f4);
  t68 = &(((*(v0))).f5);
  t69 = gg_function_58(t67, t68, base + 0);
  t66 = t69;
  }
  if (!(t66)) gg_fail("assertion failed");
  t70 = ((int64_t)(((*(v0))).f9));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((t70) == (t71));
  if (t72) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = &(((*(v0))).f4);
  t75 = gg_function_12(t74, base + 0);
  t76 = t75;
  t73 = 0;
label6:;
  if (t73 >= t76) goto label8;
  v9 = t73;
  {
  t77 = v1;
  t78 = &(((*(v0))).f4);
  t79 = ((int64_t)(v9));
  t80 = gg_function_16(t78, t79, base + 0);
  t81 = gg_function_17(t77, t80, base + 0);
  t82 = ((uint32_t)(t81));
  t83 = v2;
  t84 = &(((*(v0))).f5);
  t85 = ((int64_t)(v9));
  t86 = gg_function_16(t84, t85, base + 0);
  t87 = gg_function_17(t83, t86, base + 0);
  t88 = ((uint32_t)(t87));
  t89 = ((t82) == (t88));
  if (t89) {
  {
  t90 = v8;
  t91 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t90) + (uint64_t)(t91)));
  }
  }
  }
label7:;
  t73++; goto label6;
label8:;
  t92 = ((int64_t)((((*(v0))).f7).f3));
  t93 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t92) >= (t93)))) gg_fail("assertion failed");
  v10 = ((uint64_t)(((uint64_t)(v8))));
  t95 = ((int64_t)UINT64_C(8));
  t94 = 0;
label9:;
  if (t94 >= t95) goto label11;
  v11 = t94;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t96 = v11;
  t97 = ((uint8_t *)(v3).data) + gg_index(t96, (v3).len);
  t98 = ((uint64_t)(v10));
  t99 = ((int64_t)(((int64_t)UINT64_C(8))));
  t100 = ((int64_t)(v11));
  t101 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t100)))))));
  (*t97) = ((uint8_t)(((uint8_t)(((t98) >> ((uint64_t)(t101) & 63))))));
  }
label10:;
  t94++; goto label9;
label11:;
  }
  }
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t102 = v0;
  v12 = gg_function_14(t102, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t103 = ((int64_t)UINT64_C(0));
  t104 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t103, ((int64_t)UINT64_C(4)));
  v13 = ((int64_t)((*t104)));
label13:;
  t105 = ((int64_t)(v12));
  t106 = &(((*(v0))).f4);
  t107 = gg_function_13(t106, base + 0);
  t108 = ((int64_t)(t107));
  if (!(((t105) < (t108)))) goto label14;
  {
  t109 = ((int64_t)(((*(v0))).f8));
  t110 = ((int64_t)(((int64_t)UINT64_C(18))));
  t111 = ((t109) == (t110));
  if (t111) {
  {
  t112 = ((int64_t)(v13));
  t113 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t112) > (t113)))) gg_fail("assertion failed");
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = &(((*(v0))).f4);
  t115 = v1;
  t116 = ((int64_t)(v12));
  t117 = ((int64_t)(v13));
  t118 = ((int64_t)(((int64_t)((uint64_t)(t116) * (uint64_t)(t117)))));
  v15 = gg_function_51(t114, t115, t118, base + 0);
  t120 = v13;
  t119 = 0;
label15:;
  if (t119 >= t120) goto label17;
  v16 = t119;
  {
  t121 = &(((*(v0))).f4);
  t122 = v1;
  t123 = ((int64_t)(v12));
  t124 = ((int64_t)(v13));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) * (uint64_t)(t124)))));
  t126 = ((int64_t)(v16));
  t127 = ((int64_t)(((int64_t)((uint64_t)(t125) + (uint64_t)(t126)))));
  v17 = gg_function_51(t121, t122, t127, base + 0);
  t128 = ((float)(v17));
  t129 = ((float)(v15));
  t130 = ((t128) > (t129));
  if (t130) {
  {
  v15 = ((float)(v17));
  v14 = ((int64_t)(v16));
  }
  }
  }
label16:;
  t119++; goto label15;
label17:;
  t131 = ((int64_t)((((*(v0))).f7).f2));
  t132 = ((int64_t)(((int64_t)UINT64_C(26))));
  t133 = ((t131) == (t132));
  t134 = t133;
  if (t134) {
  t135 = ((int64_t)(v14));
  t136 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t134 = ((t135) <= (t136));
  }
  if (!(t134)) gg_fail("assertion failed");
  t137 = v3;
  t138 = &(((*(v0))).f7);
  t139 = ((int64_t)(v12));
  t140 = gg_function_16(t138, t139, base + 0);
  t141 = ((uint32_t)(((uint32_t)(v14))));
  gg_function_40(t137, t140, t141, base + 0);
  }
  } else {
  t142 = ((int64_t)(((*(v0))).f8));
  t143 = ((int64_t)(((int64_t)UINT64_C(16))));
  t144 = ((t142) == (t143));
  if (t144) {
  {
  v18 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t146 = v13;
  t145 = 0;
label18:;
  if (t145 >= t146) goto label20;
  v19 = t145;
  {
  t147 = v18;
  t148 = &(((*(v0))).f4);
  t149 = v1;
  t150 = ((int64_t)(v12));
  t151 = ((int64_t)(v13));
  t152 = ((int64_t)(((int64_t)((uint64_t)(t150) * (uint64_t)(t151)))));
  t153 = ((int64_t)(v19));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t152) + (uint64_t)(t153)))));
  t155 = gg_function_51(t148, t149, t154, base + 0);
  t156 = ((float)(t155));
  v18 = ((t147) + (t156));
  t157 = &(((*(v0))).f7);
  t158 = v3;
  t159 = ((int64_t)(v12));
  t160 = ((int64_t)(v13));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t159) * (uint64_t)(t160)))));
  t162 = ((int64_t)(v19));
  t163 = ((int64_t)(((int64_t)((uint64_t)(t161) + (uint64_t)(t162)))));
  t164 = ((float)(v18));
  gg_function_37(t157, t158, t163, t164, base + 0);
  }
label19:;
  t145++; goto label18;
label20:;
  }
  } else {
  {
  v20 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t166 = v13;
  t165 = 0;
label21:;
  if (t165 >= t166) goto label23;
  v21 = t165;
  {
  t167 = v20;
  t168 = &(((*(v0))).f4);
  t169 = v1;
  t170 = ((int64_t)(v12));
  t171 = ((int64_t)(v13));
  t172 = ((int64_t)(((int64_t)((uint64_t)(t170) * (uint64_t)(t171)))));
  t173 = ((int64_t)(v21));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t172) + (uint64_t)(t173)))));
  t175 = gg_function_51(t168, t169, t174, base + 0);
  t176 = ((double)(((double)(t175))));
  v20 = ((t167) + (t176));
  }
label22:;
  t165++; goto label21;
label23:;
  t177 = ((int64_t)(((*(v0))).f8));
  t178 = ((int64_t)(((int64_t)UINT64_C(17))));
  t179 = ((t177) == (t178));
  if (t179) {
  {
  t180 = ((double)(v20));
  t181 = ((double)(((double)(v13))));
  v22 = ((float)(((float)(((t180) / (t181))))));
  }
  } else {
  {
  v22 = ((float)(((float)(v20))));
  }
  }
  t182 = &(((*(v0))).f7);
  t183 = v3;
  t184 = ((int64_t)(v12));
  t185 = ((float)(v22));
  gg_function_37(t182, t183, t184, t185, base + 0);
  }
  }
  }
  t186 = v12;
  t187 = ((int64_t)(((*(v0))).f10));
  v12 = ((int64_t)((uint64_t)(t186) + (uint64_t)(t187)));
  }
  goto label13;
label14:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_85(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t3 = v0;
  t4 = ((float)((-(t3))));
  t5 = expf(t4);
  t6 = ((float)(t5));
  t7 = ((float)(((t2) + (t6))));
  t0 = ((float)(((t1) / (t7))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_89(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v1));
  t2 = ((float)(v0));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((float)(v1));
  }
  } else {
  {
  t0 = ((float)(v0));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_90(float p0, float p1, int64_t base) {
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
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((float)(v1));
  }
  } else {
  {
  t0 = ((float)(v0));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_88(float p0, int64_t base) {
  float t0;
  float t1;
  uint32_t t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float t8;
  float t9;
  float t10;
  uint32_t t11;
  float t12;
  float t13;
  float t14;
  float t15;
  uint32_t t16;
  float t17;
  float t18;
  float t19;
  float t20;
  float t21;
  float t22;
  float t23;
  uint32_t t24;
  uint32_t t25;
  int64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint32_t t29;
  float t30;
  float t31;
  float t32;
  uint32_t t33;
  float t34;
  float t35;
  float t36;
  uint32_t t37;
  float t38;
  float t39;
  float t40;
  float t41;
  float t42;
  uint32_t t43;
  float t44;
  float t45;
  float t46;
  uint32_t t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
  float t53;
  float t54;
  uint32_t t55;
  float t56;
  float t57;
  float t58;
  float t59;
  float t60;
  float t61;
  float t62;
  float t63;
  float t64;
  uint8_t t65;
  uint8_t t66;
  float t67;
  float t68;
  float t69;
  float t70;
  float t71;
  float t72;
  uint8_t t73;
  uint64_t t74;
  uint64_t t75;
  uint32_t t76;
  float t77;
  uint64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint32_t t82;
  float t83;
  float t84;
  float t85;
  float t86;
  float t87;
  uint8_t t88;
  float t89;
  float t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  float v0;
  float v1;
  float v2;
  float v3;
  uint32_t v4;
  float v5;
  float v6;
  float v7;
  uint32_t v8;
  float v9;
  float v10;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = ((uint32_t)(((int64_t)UINT64_C(1069066811))));
  t3 = fg143_from_bits(t2);
  t4 = ((float)(t3));
  t5 = ((float)(((float)(((int64_t)UINT64_C(12582912))))));
  t6 = fmaf(t1, t4, t5);
  v1 = ((float)(t6));
  t7 = ((float)(v1));
  t8 = ((float)(((float)(((int64_t)UINT64_C(12582912))))));
  v2 = ((float)(((t7) - (t8))));
  t9 = v2;
  t10 = ((float)((-(t9))));
  t11 = ((uint32_t)(((int64_t)UINT64_C(901758606))));
  t12 = fg143_from_bits(t11);
  t13 = ((float)(t12));
  t14 = v2;
  t15 = ((float)((-(t14))));
  t16 = ((uint32_t)(((int64_t)UINT64_C(1060205056))));
  t17 = fg143_from_bits(t16);
  t18 = ((float)(t17));
  t19 = ((float)(v0));
  t20 = fmaf(t15, t18, t19);
  t21 = ((float)(t20));
  t22 = fmaf(t10, t13, t21);
  v3 = ((float)(t22));
  t23 = ((float)(v1));
  t24 = fg143_bits(t23);
  t25 = ((uint32_t)(t24));
  t26 = ((int64_t)(((int64_t)UINT64_C(23))));
  v4 = ((uint32_t)(((uint32_t)((uint64_t)(t25) << ((uint64_t)(t26) & 31)))));
  t27 = ((uint64_t)(((uint64_t)(v4))));
  t28 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1065353216))))));
  t29 = ((uint32_t)(((uint32_t)(((uint64_t)((uint64_t)(t27) + (uint64_t)(t28)))))));
  t30 = fg143_from_bits(t29);
  v5 = ((float)(t30));
  t31 = ((float)(v3));
  t32 = ((float)(v3));
  v6 = ((float)(((t31) * (t32))));
  t33 = ((uint32_t)(((int64_t)UINT64_C(1007099920))));
  t34 = fg143_from_bits(t33);
  t35 = ((float)(t34));
  t36 = ((float)(v3));
  t37 = ((uint32_t)(((int64_t)UINT64_C(1026268951))));
  t38 = fg143_from_bits(t37);
  t39 = ((float)(t38));
  t40 = fmaf(t35, t36, t39);
  t41 = ((float)(t40));
  t42 = ((float)(v6));
  t43 = ((uint32_t)(((int64_t)UINT64_C(1042984755))));
  t44 = fg143_from_bits(t43);
  t45 = ((float)(t44));
  t46 = ((float)(v3));
  t47 = ((uint32_t)(((int64_t)UINT64_C(1056964315))));
  t48 = fg143_from_bits(t47);
  t49 = ((float)(t48));
  t50 = fmaf(t45, t46, t49);
  t51 = ((float)(t50));
  t52 = fmaf(t41, t42, t51);
  t53 = ((float)(t52));
  t54 = ((float)(v6));
  t55 = ((uint32_t)(((int64_t)UINT64_C(1065353206))));
  t56 = fg143_from_bits(t55);
  t57 = ((float)(t56));
  t58 = ((float)(v3));
  t59 = ((float)(((t57) * (t58))));
  t60 = fmaf(t53, t54, t59);
  v7 = ((float)(t60));
  t61 = ((float)(v2));
  t62 = fabsf(t61);
  t63 = ((float)(t62));
  t64 = ((float)(((float)(((int64_t)UINT64_C(126))))));
  t65 = ((t63) > (t64));
  t66 = (!(t65));
  if (t66) {
  {
  t67 = ((float)(v7));
  t68 = ((float)(v5));
  t69 = ((float)(v5));
  t70 = fmaf(t67, t68, t69);
  t0 = ((float)(t70));
  goto gg_done;
  }
  }
  t71 = ((float)(v2));
  t72 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t73 = ((t71) <= (t72));
  if (t73) {
  {
  v8 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(2181038080))))));
  }
  } else {
  {
  v8 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t74 = ((uint64_t)(((uint64_t)(v8))));
  t75 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2130706432))))));
  t76 = ((uint32_t)(((uint32_t)(((uint64_t)((uint64_t)(t74) + (uint64_t)(t75)))))));
  t77 = fg143_from_bits(t76);
  v9 = ((float)(t77));
  t78 = ((uint64_t)(((uint64_t)(v4))));
  t79 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4294967296))))));
  t80 = ((uint64_t)(((uint64_t)((uint64_t)(t78) + (uint64_t)(t79)))));
  t81 = ((uint64_t)(((uint64_t)(v8))));
  t82 = ((uint32_t)(((uint32_t)(((uint64_t)((uint64_t)(t80) - (uint64_t)(t81)))))));
  t83 = fg143_from_bits(t82);
  v10 = ((float)(t83));
  t84 = ((float)(v2));
  t85 = fabsf(t84);
  t86 = ((float)(t85));
  t87 = ((float)(((float)(((int64_t)UINT64_C(192))))));
  t88 = ((t86) > (t87));
  if (t88) {
  {
  t89 = ((float)(v9));
  t90 = ((float)(v9));
  t0 = ((float)(((t89) * (t90))));
  goto gg_done;
  }
  }
  t91 = ((float)(v10));
  t92 = ((float)(v7));
  t93 = ((float)(v10));
  t94 = fmaf(t91, t92, t93);
  t95 = ((float)(t94));
  t96 = ((float)(v9));
  t0 = ((float)(((t95) * (t96))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_87(gg_type_20 p0, float p1, int64_t p2, int64_t p3, int64_t base) {
  float t0;
  int64_t t1;
  int64_t * t2;
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
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  float t22;
  float t23;
  float t24;
  float t25;
  float t26;
  float t27;
  float t28;
  float t29;
  float t30;
  gg_type_20 v0;
  float v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1 = ((int64_t)UINT64_C(12));
  t2 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t1, ((int64_t)UINT64_C(16)));
  t3 = ((int64_t)((*t2)));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  t6 = t5;
  if (t6) {
  t7 = ((int64_t)((((*(v0))).f4).f2));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t7) == (t8));
  }
  t9 = t6;
  t10 = t9;
  if (t10) {
  t11 = ((int64_t)((((*(v0))).f7).f2));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t11) == (t12));
  }
  t13 = t10;
  t14 = t13;
  if (t14) {
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)UINT64_C(8))));
  t18 = ((int64_t)(((int64_t)gg_div(t16, t17))));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t14 = ((t15) < (t20));
  }
  t21 = t14;
  if (t21) {
  {
  t22 = ((float)(v1));
  t23 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t24 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t25 = ((float)(v1));
  t26 = ((float)(((t24) - (t25))));
  t27 = gg_function_88(t26, base + 0);
  t28 = ((float)(t27));
  t29 = ((float)(((t23) + (t28))));
  t0 = ((float)(((t22) / (t29))));
  goto gg_done;
  }
  }
  t30 = ((float)(v1));
  t0 = gg_function_85(t30, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_84(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float t8;
  float t9;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t3 = ((float)(gg_double_bits(UINT64_C(4610343945738431496))));
  t4 = ((float)((-(t3))));
  t5 = ((float)(v0));
  t6 = ((float)(((t4) * (t5))));
  t7 = expf(t6);
  t8 = ((float)(t7));
  t9 = ((float)(((t2) + (t8))));
  t0 = ((float)(((t1) / (t9))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_82(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float t8;
  float t9;
  float t10;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(((float)(gg_double_bits(UINT64_C(4602678819172646912))))));
  t2 = ((float)(v0));
  t3 = ((float)(((t1) * (t2))));
  t4 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t5 = ((float)(v0));
  t6 = ((float)(((float)(gg_double_bits(UINT64_C(4604544271217802188))))));
  t7 = ((float)(((t5) * (t6))));
  t8 = erff(t7);
  t9 = ((float)(t8));
  t10 = ((float)(((t4) + (t9))));
  t0 = ((float)(((t3) * (t10))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_81(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float t8;
  float t9;
  float t10;
  float t11;
  float t12;
  float t13;
  float t14;
  float t15;
  float t16;
  float t17;
  float t18;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(((float)(gg_double_bits(UINT64_C(4602678819172646912))))));
  t2 = ((float)(v0));
  t3 = ((float)(((t1) * (t2))));
  t4 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t5 = ((float)(((float)(gg_double_bits(UINT64_C(4605361924766709329))))));
  t6 = ((float)(v0));
  t7 = ((float)(((t5) * (t6))));
  t8 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t9 = ((float)(((float)(gg_double_bits(UINT64_C(4586604931670606327))))));
  t10 = ((float)(v0));
  t11 = ((float)(((t9) * (t10))));
  t12 = ((float)(v0));
  t13 = ((float)(((t11) * (t12))));
  t14 = ((float)(((t8) + (t13))));
  t15 = ((float)(((t7) * (t14))));
  t16 = tanhf(t15);
  t17 = ((float)(t16));
  t18 = ((float)(((t4) + (t17))));
  t0 = ((float)(((t3) * (t18))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_80(gg_type_20 p0, float p1, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t * t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  float t10;
  float t11;
  float t12;
  uint8_t t13;
  float t14;
  float t15;
  uint8_t t16;
  float t17;
  uint16_t t18;
  uint16_t t19;
  float t20;
  float t21;
  float t22;
  uint16_t t23;
  uint16_t t24;
  float t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  float t29;
  float t30;
  uint16_t t31;
  uint16_t t32;
  float t33;
  float t34;
  gg_type_20 v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)((((*(v0))).f4).f2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (t4) {
  /* FG_DIRECT_SLICE_INDEX */
  t5 = ((int64_t)UINT64_C(13));
  t6 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t5, ((int64_t)UINT64_C(16)));
  t7 = ((int64_t)((*t6)));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t7) != (t8));
  }
  t9 = t4;
  if (t9) {
  {
  t10 = ((float)(v1));
  t11 = ((float)(((int64_t)UINT64_C(10))));
  t12 = ((float)((-(t11))));
  t13 = ((t10) <= (t12));
  if (t13) {
  {
  t0 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t14 = ((float)(v1));
  t15 = ((float)(((float)(((int64_t)UINT64_C(10))))));
  t16 = ((t14) >= (t15));
  if (t16) {
  {
  t0 = ((float)(v1));
  goto gg_done;
  }
  }
  t17 = ((float)(v1));
  t18 = fg143_to_half(t17);
  t19 = ((uint16_t)(t18));
  t20 = fg143_half(t19);
  t21 = ((float)(t20));
  t22 = gg_function_81(t21, base + 0);
  t23 = fg143_to_half(t22);
  t24 = ((uint16_t)(t23));
  t25 = fg143_half(t24);
  t0 = ((float)(t25));
  goto gg_done;
  }
  }
  t26 = ((int64_t)((((*(v0))).f4).f2));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t29 = ((float)(v1));
  t30 = gg_function_81(t29, base + 0);
  t31 = fg143_to_half(t30);
  t32 = ((uint16_t)(t31));
  t33 = fg143_half(t32);
  t0 = ((float)(t33));
  goto gg_done;
  }
  }
  t34 = ((float)(v1));
  t0 = gg_function_81(t34, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_86(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t * t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint8_t t9;
  gg_type_31 t10;
  int64_t t11;
  int64_t t12;
  gg_type_31 t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t * t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t * t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  gg_type_31 t25;
  gg_type_31 t26;
  uint8_t t27;
  uint8_t t28;
  uint8_t t29;
  gg_type_31 t30;
  gg_type_31 t31;
  uint8_t t32;
  int64_t t33;
  int64_t * t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  gg_type_20 t39;
  int64_t t40;
  gg_type_31 t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  gg_type_31 t46;
  gg_slice t47;
  int64_t t48;
  int64_t t49;
  int64_t * t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  uint8_t t57;
  uint8_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  gg_type_31 t63;
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  int64_t * t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  gg_type_31 t72;
  gg_slice t73;
  int64_t t74;
  int64_t t75;
  int64_t * t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  uint8_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t * t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
  float t90;
  float t91;
  float t92;
  uint8_t t93;
  float t94;
  float t95;
  int64_t t96;
  int64_t t97;
  uint8_t t98;
  gg_type_20 t99;
  float t100;
  float t101;
  float t102;
  float t103;
  int64_t t104;
  int64_t t105;
  uint8_t t106;
  float t107;
  float t108;
  float t109;
  float t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  float t114;
  float t115;
  float t116;
  float t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
  gg_type_20 t121;
  float t122;
  int64_t t123;
  int64_t t124;
  float t125;
  float t126;
  float t127;
  int64_t t128;
  int64_t t129;
  uint8_t t130;
  float t131;
  int64_t t132;
  float * t133;
  float t134;
  float t135;
  int64_t t136;
  float * t137;
  float t138;
  float t139;
  int64_t t140;
  float * t141;
  float t142;
  float t143;
  float t144;
  float t145;
  int64_t t146;
  float * t147;
  float t148;
  float t149;
  float t150;
  float t151;
  float t152;
  float t153;
  float t154;
  float t155;
  float t156;
  float t157;
  float t158;
  int64_t t159;
  int64_t t160;
  float t161;
  int64_t t162;
  float * t163;
  float t164;
  float t165;
  int64_t t166;
  float * t167;
  float t168;
  float t169;
  float t170;
  int64_t t171;
  float * t172;
  float t173;
  float t174;
  float t175;
  float t176;
  float t177;
  gg_type_31 t178;
  gg_slice t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  float t185;
  int64_t t186;
  int64_t t187;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  uint8_t v5;
  uint8_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  float v10;
  int64_t v11;
  float v12;
  float v13;
  float v14;
  float v15;
  float v16;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(100))));
  t3 = ((t1) != (t2));
  if (t3) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t4 = ((int64_t)UINT64_C(0));
  t5 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t4, ((int64_t)UINT64_C(4)));
  v4 = ((int64_t)((*t5)));
  t6 = ((int64_t)(v4));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) > (t7));
  t9 = t8;
  if (t9) {
  t10 = &(((*(v0))).f4);
  t11 = gg_function_13(t10, base + 0);
  t12 = ((int64_t)(t11));
  t13 = &(((*(v0))).f7);
  t14 = gg_function_13(t13, base + 0);
  t15 = ((int64_t)(t14));
  t9 = ((t12) == (t15));
  }
  if (!(t9)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t16 = ((int64_t)UINT64_C(2));
  t17 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t16, ((int64_t)UINT64_C(16)));
  t18 = ((int64_t)((*t17)));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = ((t18) != (t19));
  /* FG_DIRECT_SLICE_INDEX */
  t20 = ((int64_t)UINT64_C(1));
  t21 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t20, ((int64_t)UINT64_C(16)));
  t22 = ((int64_t)((*t21)));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  v6 = ((t22) != (t23));
  t24 = v5;
  if (t24) {
  {
  t25 = &(((*(v0))).f4);
  t26 = &(((*(v0))).f5);
  t27 = gg_function_58(t25, t26, base + 0);
  t28 = t27;
  t29 = t28;
  if (t29) {
  t30 = &(((*(v0))).f4);
  t31 = &(((*(v0))).f7);
  t32 = gg_function_58(t30, t31, base + 0);
  t29 = t32;
  }
  if (!(t29)) gg_fail("assertion failed");
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t33 = ((int64_t)UINT64_C(0));
  t34 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t33, ((int64_t)UINT64_C(4)));
  t35 = ((int64_t)((*t34)));
  t36 = ((int64_t)(((int64_t)UINT64_C(2))));
  t37 = ((int64_t)(v4));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  if (!(((t35) == (t38)))) gg_fail("assertion failed");
  }
  }
  t39 = v0;
  v7 = gg_function_14(t39, base + 0);
label1:;
  t40 = ((int64_t)(v7));
  t41 = &(((*(v0))).f7);
  t42 = gg_function_13(t41, base + 0);
  t43 = ((int64_t)(t42));
  if (!(((t40) < (t43)))) goto label2;
  {
  t45 = v4;
  t44 = 0;
label3:;
  if (t44 >= t45) goto label5;
  v8 = t44;
  {
  t46 = &(((*(v0))).f4);
  t47 = v1;
  t48 = ((int64_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t49 = ((int64_t)UINT64_C(0));
  t50 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t49, ((int64_t)UINT64_C(4)));
  t51 = ((int64_t)((*t50)));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t51)))));
  t53 = ((int64_t)(v8));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t56 = v5;
  t57 = t56;
  if (!t57) {
  t58 = v6;
  t57 = (!(t58));
  }
  t59 = t57;
  if (t59) {
  {
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t55 = ((int64_t)(v4));
  }
  }
  t60 = ((int64_t)(t55));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t60)))));
  v9 = gg_function_51(t46, t47, t61, base + 0);
  t62 = v5;
  if (t62) {
  {
  t63 = &(((*(v0))).f5);
  t64 = v2;
  t65 = ((int64_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t66 = ((int64_t)UINT64_C(0));
  t67 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t66, ((int64_t)UINT64_C(4)));
  t68 = ((int64_t)((*t67)));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t68)))));
  t70 = ((int64_t)(v8));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) + (uint64_t)(t70)))));
  v10 = gg_function_51(t63, t64, t71, base + 0);
  }
  } else {
  {
  t72 = &(((*(v0))).f4);
  t73 = v1;
  t74 = ((int64_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t75 = ((int64_t)UINT64_C(0));
  t76 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t75, ((int64_t)UINT64_C(4)));
  t77 = ((int64_t)((*t76)));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t74) * (uint64_t)(t77)))));
  t79 = ((int64_t)(v8));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t79)))));
  t82 = v6;
  if (t82) {
  {
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t81 = ((int64_t)(v4));
  }
  }
  t83 = ((int64_t)(t81));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t83)))));
  v10 = gg_function_51(t72, t73, t84, base + 0);
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t85 = ((int64_t)UINT64_C(0));
  t86 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t85, ((int64_t)UINT64_C(16)));
  v11 = ((int64_t)((*t86)));
  v12 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t87 = ((int64_t)(v11));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((t87) == (t88));
  if (t89) {
  {
  t91 = ((float)(v9));
  t92 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t93 = ((t91) > (t92));
  if (t93) {
  {
  t90 = ((float)(v9));
  }
  } else {
  {
  t90 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  }
  t94 = ((float)(t90));
  t95 = ((float)(v10));
  v12 = ((float)(((t94) * (t95))));
  }
  } else {
  t96 = ((int64_t)(v11));
  t97 = ((int64_t)(((int64_t)UINT64_C(1))));
  t98 = ((t96) == (t97));
  if (t98) {
  {
  t99 = v0;
  t100 = ((float)(v9));
  t101 = gg_function_80(t99, t100, base + 0);
  t102 = ((float)(t101));
  t103 = ((float)(v10));
  v12 = ((float)(((t102) * (t103))));
  }
  } else {
  t104 = ((int64_t)(v11));
  t105 = ((int64_t)(((int64_t)UINT64_C(4))));
  t106 = ((t104) == (t105));
  if (t106) {
  {
  t107 = ((float)(v9));
  t108 = gg_function_82(t107, base + 0);
  t109 = ((float)(t108));
  t110 = ((float)(v10));
  v12 = ((float)(((t109) * (t110))));
  }
  } else {
  t111 = ((int64_t)(v11));
  t112 = ((int64_t)(((int64_t)UINT64_C(5))));
  t113 = ((t111) == (t112));
  if (t113) {
  {
  t114 = ((float)(v9));
  t115 = gg_function_84(t114, base + 0);
  t116 = ((float)(t115));
  t117 = ((float)(v10));
  v12 = ((float)(((t116) * (t117))));
  }
  } else {
  t118 = ((int64_t)(v11));
  t119 = ((int64_t)(((int64_t)UINT64_C(2))));
  t120 = ((t118) == (t119));
  if (t120) {
  {
  t121 = v0;
  t122 = ((float)(v9));
  t123 = ((int64_t)(v8));
  t124 = ((int64_t)(v4));
  t125 = gg_function_87(t121, t122, t123, t124, base + 0);
  t126 = ((float)(t125));
  t127 = ((float)(v10));
  v12 = ((float)(((t126) * (t127))));
  }
  } else {
  t128 = ((int64_t)(v11));
  t129 = ((int64_t)(((int64_t)UINT64_C(3))));
  t130 = ((t128) == (t129));
  if (t130) {
  {
  t131 = ((float)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t132 = ((int64_t)UINT64_C(1));
  t133 = ((float *)(((*(v0))).f12).data) + gg_index(t132, ((int64_t)UINT64_C(8)));
  t134 = ((float)((*t133)));
  v13 = gg_function_89(t131, t134, base + 0);
  t135 = ((float)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t136 = ((int64_t)UINT64_C(1));
  t137 = ((float *)(((*(v0))).f12).data) + gg_index(t136, ((int64_t)UINT64_C(8)));
  t138 = ((float)((*t137)));
  t139 = gg_function_89(t135, t138, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t140 = ((int64_t)UINT64_C(1));
  t141 = ((float *)(((*(v0))).f12).data) + gg_index(t140, ((int64_t)UINT64_C(8)));
  t142 = (*t141);
  t143 = ((float)((-(t142))));
  v14 = gg_function_90(t139, t143, base + 0);
  t144 = ((float)(v13));
  t145 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  /* FG_DIRECT_SLICE_INDEX */
  t146 = ((int64_t)UINT64_C(0));
  t147 = ((float *)(((*(v0))).f12).data) + gg_index(t146, ((int64_t)UINT64_C(8)));
  t148 = (*t147);
  t149 = ((float)((-(t148))));
  t150 = ((float)(v13));
  t151 = ((float)(((t149) * (t150))));
  t152 = expf(t151);
  t153 = ((float)(t152));
  t154 = ((float)(((t145) + (t153))));
  t155 = ((float)(((t144) / (t154))));
  t156 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t157 = ((float)(v14));
  t158 = ((float)(((t156) + (t157))));
  v12 = ((float)(((t155) * (t158))));
  }
  } else {
  {
  t159 = ((int64_t)(v11));
  t160 = ((int64_t)(((int64_t)UINT64_C(6))));
  if (!(((t159) == (t160)))) gg_fail("assertion failed");
  t161 = ((float)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t162 = ((int64_t)UINT64_C(1));
  t163 = ((float *)(((*(v0))).f12).data) + gg_index(t162, ((int64_t)UINT64_C(8)));
  t164 = ((float)((*t163)));
  v15 = gg_function_89(t161, t164, base + 0);
  t165 = ((float)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t166 = ((int64_t)UINT64_C(1));
  t167 = ((float *)(((*(v0))).f12).data) + gg_index(t166, ((int64_t)UINT64_C(8)));
  t168 = (*t167);
  t169 = ((float)((-(t168))));
  t170 = gg_function_90(t165, t169, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t171 = ((int64_t)UINT64_C(1));
  t172 = ((float *)(((*(v0))).f12).data) + gg_index(t171, ((int64_t)UINT64_C(8)));
  t173 = ((float)((*t172)));
  v16 = gg_function_89(t170, t173, base + 0);
  t174 = ((float)(v15));
  t175 = gg_function_85(t174, base + 0);
  t176 = ((float)(t175));
  t177 = ((float)(v16));
  v12 = ((float)(((t176) * (t177))));
  }
  }
  }
  }
  }
  }
  }
  t178 = &(((*(v0))).f7);
  t179 = v3;
  t180 = ((int64_t)(v7));
  t181 = ((int64_t)(v4));
  t182 = ((int64_t)(((int64_t)((uint64_t)(t180) * (uint64_t)(t181)))));
  t183 = ((int64_t)(v8));
  t184 = ((int64_t)(((int64_t)((uint64_t)(t182) + (uint64_t)(t183)))));
  t185 = ((float)(v12));
  gg_function_37(t178, t179, t184, t185, base + 0);
  }
label4:;
  t44++; goto label3;
label5:;
  t186 = v7;
  t187 = ((int64_t)(((*(v0))).f10));
  v7 = ((int64_t)((uint64_t)(t186) + (uint64_t)(t187)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_74(gg_type_20 p0, int64_t p1, float p2, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  float t4;
  float t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  float t9;
  float t10;
  uint8_t t11;
  float t12;
  float t13;
  uint8_t t14;
  float t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  float t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  float t23;
  float t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  float t29;
  float t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  float t34;
  float t35;
  uint8_t t36;
  float t37;
  float t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  float t42;
  float t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
  float t53;
  float t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  float t58;
  float t59;
  float t60;
  float t61;
  float t62;
  float t63;
  float t64;
  float t65;
  float t66;
  float t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
  float t71;
  float t72;
  int64_t t73;
  int64_t t74;
  uint8_t t75;
  float t76;
  float t77;
  float t78;
  float t79;
  float t80;
  float t81;
  float t82;
  float t83;
  float t84;
  float t85;
  float t86;
  float t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
  float t91;
  float t92;
  float t93;
  float t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  float t98;
  float t99;
  uint8_t t100;
  float t101;
  float t102;
  float t103;
  float t104;
  float t105;
  float t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  float t110;
  float t111;
  int64_t t112;
  int64_t t113;
  uint8_t t114;
  float t115;
  float t116;
  int64_t t117;
  int64_t t118;
  uint8_t t119;
  float t120;
  float t121;
  int64_t t122;
  int64_t t123;
  uint8_t t124;
  float t125;
  float t126;
  int64_t t127;
  int64_t t128;
  uint8_t t129;
  float t130;
  float t131;
  uint8_t t132;
  int64_t t133;
  float * t134;
  float t135;
  float t136;
  float t137;
  float t138;
  int64_t t139;
  float * t140;
  float t141;
  float t142;
  float t143;
  float t144;
  float t145;
  int64_t t146;
  float * t147;
  float t148;
  float t149;
  float t150;
  float t151;
  float t152;
  float t153;
  float t154;
  int64_t t155;
  float * t156;
  float t157;
  int64_t t158;
  float * t159;
  float t160;
  float t161;
  float t162;
  float t163;
  int64_t t164;
  int64_t t165;
  uint8_t t166;
  gg_type_20 t167;
  float t168;
  int64_t t169;
  int64_t t170;
  uint8_t t171;
  float t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  float t176;
  int64_t t177;
  int64_t t178;
  float t179;
  gg_type_20 v0;
  int64_t v1;
  float v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v2));
  t5 = fabsf(t4);
  t0 = ((float)(t5));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((t6) == (t7));
  if (t8) {
  {
  t9 = ((float)(v2));
  t10 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t11 = ((t9) > (t10));
  if (t11) {
  {
  t0 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  t12 = ((float)(v2));
  t13 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t15 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t0 = ((float)((-(t15))));
  }
  } else {
  {
  t0 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  }
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t19 = v2;
  t0 = ((float)((-(t19))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v1));
  t21 = ((int64_t)(((int64_t)UINT64_C(3))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  t23 = ((float)(v2));
  t24 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t25 = ((t23) > (t24));
  if (t25) {
  {
  t0 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  {
  t0 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  goto gg_done;
  }
  }
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(((int64_t)UINT64_C(4))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t29 = ((float)(v2));
  t30 = tanhf(t29);
  t0 = ((float)(t30));
  goto gg_done;
  }
  }
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(5))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t34 = ((float)(v2));
  t35 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t36 = ((t34) > (t35));
  if (t36) {
  {
  t0 = ((float)(v2));
  }
  } else {
  {
  t37 = ((float)(v2));
  t38 = expm1f(t37);
  t0 = ((float)(t38));
  }
  }
  goto gg_done;
  }
  }
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(((int64_t)UINT64_C(6))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  t42 = ((float)(v2));
  t43 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t44 = ((t42) > (t43));
  if (t44) {
  {
  t0 = ((float)(v2));
  }
  } else {
  {
  t0 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  goto gg_done;
  }
  }
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(((int64_t)UINT64_C(7))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  t48 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t49 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t50 = v2;
  t51 = ((float)((-(t50))));
  t52 = expf(t51);
  t53 = ((float)(t52));
  t54 = ((float)(((t49) + (t53))));
  t0 = ((float)(((t48) / (t54))));
  goto gg_done;
  }
  }
  t55 = ((int64_t)(v1));
  t56 = ((int64_t)(((int64_t)UINT64_C(12))));
  t57 = ((t55) == (t56));
  if (t57) {
  {
  t58 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t59 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t60 = ((float)(v2));
  t61 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t62 = ((float)(((t60) + (t61))));
  t63 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t64 = ((float)(((t62) / (t63))));
  t65 = fmaxf(t59, t64);
  t66 = ((float)(t65));
  t67 = fminf(t58, t66);
  t0 = ((float)(t67));
  goto gg_done;
  }
  }
  t68 = ((int64_t)(v1));
  t69 = ((int64_t)(((int64_t)UINT64_C(13))));
  t70 = ((t68) == (t69));
  if (t70) {
  {
  t71 = ((float)(v2));
  t72 = expf(t71);
  t0 = ((float)(t72));
  goto gg_done;
  }
  }
  t73 = ((int64_t)(v1));
  t74 = ((int64_t)(((int64_t)UINT64_C(11))));
  t75 = ((t73) == (t74));
  if (t75) {
  {
  t76 = ((float)(v2));
  t77 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t78 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t79 = ((float)(v2));
  t80 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t81 = ((float)(((t79) + (t80))));
  t82 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t83 = ((float)(((t81) / (t82))));
  t84 = fmaxf(t78, t83);
  t85 = ((float)(t84));
  t86 = fminf(t77, t85);
  t87 = ((float)(t86));
  t0 = ((float)(((t76) * (t87))));
  goto gg_done;
  }
  }
  t88 = ((int64_t)(v1));
  t89 = ((int64_t)(((int64_t)UINT64_C(14))));
  t90 = ((t88) == (t89));
  if (t90) {
  {
  t91 = ((float)(v2));
  t92 = expf(t91);
  t93 = ((float)(t92));
  t94 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t0 = ((float)(((t93) - (t94))));
  goto gg_done;
  }
  }
  t95 = ((int64_t)(v1));
  t96 = ((int64_t)(((int64_t)UINT64_C(15))));
  t97 = ((t95) == (t96));
  if (t97) {
  {
  t98 = ((float)(v2));
  t99 = ((float)(((float)(gg_double_bits(UINT64_C(4626322717216342016))))));
  t100 = ((t98) > (t99));
  if (t100) {
  {
  t0 = ((float)(v2));
  }
  } else {
  {
  t101 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t102 = ((float)(v2));
  t103 = expf(t102);
  t104 = ((float)(t103));
  t105 = ((float)(((t101) + (t104))));
  t106 = logf(t105);
  t0 = ((float)(t106));
  }
  }
  goto gg_done;
  }
  }
  t107 = ((int64_t)(v1));
  t108 = ((int64_t)(((int64_t)UINT64_C(18))));
  t109 = ((t107) == (t108));
  if (t109) {
  {
  t110 = ((float)(v2));
  t111 = floorf(t110);
  t0 = ((float)(t111));
  goto gg_done;
  }
  }
  t112 = ((int64_t)(v1));
  t113 = ((int64_t)(((int64_t)UINT64_C(19))));
  t114 = ((t112) == (t113));
  if (t114) {
  {
  t115 = ((float)(v2));
  t116 = ceilf(t115);
  t0 = ((float)(t116));
  goto gg_done;
  }
  }
  t117 = ((int64_t)(v1));
  t118 = ((int64_t)(((int64_t)UINT64_C(20))));
  t119 = ((t117) == (t118));
  if (t119) {
  {
  t120 = ((float)(v2));
  t121 = roundf(t120);
  t0 = ((float)(t121));
  goto gg_done;
  }
  }
  t122 = ((int64_t)(v1));
  t123 = ((int64_t)(((int64_t)UINT64_C(21))));
  t124 = ((t122) == (t123));
  if (t124) {
  {
  t125 = ((float)(v2));
  t126 = truncf(t125);
  t0 = ((float)(t126));
  goto gg_done;
  }
  }
  t127 = ((int64_t)(v1));
  t128 = ((int64_t)(((int64_t)UINT64_C(17))));
  t129 = ((t127) == (t128));
  if (t129) {
  {
  t130 = ((float)(v2));
  t131 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t132 = ((t130) > (t131));
  if (t132) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t133 = ((int64_t)UINT64_C(1));
  t134 = ((float *)(((*(v0))).f12).data) + gg_index(t133, ((int64_t)UINT64_C(8)));
  t135 = ((float)((*t134)));
  t136 = ((float)(v2));
  t137 = ((float)(((t135) * (t136))));
  t138 = ((float)(v2));
  /* FG_DIRECT_SLICE_INDEX */
  t139 = ((int64_t)UINT64_C(2));
  t140 = ((float *)(((*(v0))).f12).data) + gg_index(t139, ((int64_t)UINT64_C(8)));
  t141 = ((float)((*t140)));
  t142 = ((float)(v2));
  t143 = ((float)(((t141) * (t142))));
  t144 = fmaf(t137, t138, t143);
  t0 = ((float)(t144));
  }
  } else {
  {
  t145 = ((float)(v2));
  /* FG_DIRECT_SLICE_INDEX */
  t146 = ((int64_t)UINT64_C(3));
  t147 = ((float *)(((*(v0))).f12).data) + gg_index(t146, ((int64_t)UINT64_C(8)));
  t148 = ((float)((*t147)));
  t149 = fminf(t145, t148);
  t150 = ((float)(t149));
  t151 = expm1f(t150);
  t152 = ((float)(t151));
  t153 = ((float)(v2));
  t154 = ((float)(((t152) - (t153))));
  /* FG_DIRECT_SLICE_INDEX */
  t155 = ((int64_t)UINT64_C(0));
  t156 = ((float *)(((*(v0))).f12).data) + gg_index(t155, ((int64_t)UINT64_C(8)));
  t157 = ((float)((*t156)));
  /* FG_DIRECT_SLICE_INDEX */
  t158 = ((int64_t)UINT64_C(2));
  t159 = ((float *)(((*(v0))).f12).data) + gg_index(t158, ((int64_t)UINT64_C(8)));
  t160 = ((float)((*t159)));
  t161 = ((float)(v2));
  t162 = ((float)(((t160) * (t161))));
  t163 = fmaf(t154, t157, t162);
  t0 = ((float)(t163));
  }
  }
  goto gg_done;
  }
  }
  t164 = ((int64_t)(v1));
  t165 = ((int64_t)(((int64_t)UINT64_C(8))));
  t166 = ((t164) == (t165));
  if (t166) {
  {
  t167 = v0;
  t168 = ((float)(v2));
  t0 = gg_function_80(t167, t168, base + 0);
  goto gg_done;
  }
  }
  t169 = ((int64_t)(v1));
  t170 = ((int64_t)(((int64_t)UINT64_C(16))));
  t171 = ((t169) == (t170));
  if (t171) {
  {
  t172 = ((float)(v2));
  t0 = gg_function_82(t172, base + 0);
  goto gg_done;
  }
  }
  t173 = ((int64_t)(v1));
  t174 = ((int64_t)(((int64_t)UINT64_C(9))));
  t175 = ((t173) == (t174));
  if (t175) {
  {
  t176 = ((float)(v2));
  t0 = gg_function_84(t176, base + 0);
  goto gg_done;
  }
  }
  t177 = ((int64_t)(v1));
  t178 = ((int64_t)(((int64_t)UINT64_C(10))));
  if (!(((t177) == (t178)))) gg_fail("assertion failed");
  t179 = ((float)(v2));
  t0 = gg_function_85(t179, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_71(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  gg_type_20 t60;
  gg_type_31 t61;
  int64_t t62;
  int64_t t63;
  gg_type_31 t64;
  gg_slice t65;
  gg_type_31 t66;
  gg_type_31 t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  uint8_t t72;
  uint8_t t73;
  int64_t t74;
  int64_t t75;
  uint8_t t76;
  uint8_t t77;
  int64_t t78;
  int64_t t79;
  uint8_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  uint8_t t84;
  uint8_t t85;
  int64_t t86;
  int64_t t87;
  uint8_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
  gg_type_31 t93;
  gg_slice t94;
  gg_type_31 t95;
  gg_type_31 t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  float t106;
  float t107;
  int64_t t108;
  int64_t t109;
  uint8_t t110;
  float t111;
  float t112;
  int64_t t113;
  int64_t t114;
  uint8_t t115;
  float t116;
  float t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
  float t121;
  float t122;
  float t123;
  float t124;
  float t125;
  float t126;
  float t127;
  float t128;
  float t129;
  float t130;
  float t131;
  float t132;
  float t133;
  float t134;
  float t135;
  float t136;
  float t137;
  float t138;
  float t139;
  int64_t t140;
  int64_t t141;
  uint8_t t142;
  float t143;
  int64_t t144;
  float * t145;
  float t146;
  float t147;
  int64_t t148;
  float * t149;
  float t150;
  int64_t t151;
  int64_t t152;
  uint8_t t153;
  float t154;
  float t155;
  int64_t t156;
  int64_t t157;
  uint8_t t158;
  float t159;
  float t160;
  int64_t t161;
  int64_t t162;
  uint8_t t163;
  float t164;
  float t165;
  int64_t t166;
  int64_t t167;
  uint8_t t168;
  float t169;
  float t170;
  int64_t t171;
  int64_t t172;
  uint8_t t173;
  float t174;
  float t175;
  int64_t t176;
  int64_t t177;
  uint8_t t178;
  float t179;
  int64_t t180;
  float * t181;
  float t182;
  float t183;
  float t184;
  int64_t t185;
  float * t186;
  float t187;
  float t188;
  int64_t t189;
  int64_t t190;
  uint8_t t191;
  float t192;
  float t193;
  uint8_t t194;
  float t195;
  int64_t t196;
  float * t197;
  float t198;
  gg_type_20 t199;
  int64_t t200;
  int64_t * t201;
  int64_t t202;
  float t203;
  gg_type_31 t204;
  gg_slice t205;
  int64_t t206;
  float t207;
  int64_t t208;
  int64_t t209;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  float v6;
  float v7;
  float v8;
  float v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f8));
  t6 = ((int64_t)(((int64_t)UINT64_C(4))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f8));
  t10 = ((int64_t)(((int64_t)UINT64_C(6))));
  t8 = ((t9) != (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(((*(v0))).f8));
  t14 = ((int64_t)(((int64_t)UINT64_C(7))));
  t12 = ((t13) != (t14));
  }
  t15 = t12;
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(((*(v0))).f8));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t16 = ((t17) != (t18));
  }
  t19 = t16;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(((*(v0))).f8));
  t22 = ((int64_t)(((int64_t)UINT64_C(32))));
  t20 = ((t21) != (t22));
  }
  t23 = t20;
  t24 = t23;
  if (t24) {
  t25 = ((int64_t)(((*(v0))).f8));
  t26 = ((int64_t)(((int64_t)UINT64_C(9))));
  t24 = ((t25) != (t26));
  }
  t27 = t24;
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(((*(v0))).f8));
  t30 = ((int64_t)(((int64_t)UINT64_C(10))));
  t28 = ((t29) != (t30));
  }
  t31 = t28;
  t32 = t31;
  if (t32) {
  t33 = ((int64_t)(((*(v0))).f8));
  t34 = ((int64_t)(((int64_t)UINT64_C(12))));
  t32 = ((t33) != (t34));
  }
  t35 = t32;
  t36 = t35;
  if (t36) {
  t37 = ((int64_t)(((*(v0))).f8));
  t38 = ((int64_t)(((int64_t)UINT64_C(13))));
  t36 = ((t37) != (t38));
  }
  t39 = t36;
  t40 = t39;
  if (t40) {
  t41 = ((int64_t)(((*(v0))).f8));
  t42 = ((int64_t)(((int64_t)UINT64_C(11))));
  t40 = ((t41) != (t42));
  }
  t43 = t40;
  t44 = t43;
  if (t44) {
  t45 = ((int64_t)(((*(v0))).f8));
  t46 = ((int64_t)(((int64_t)UINT64_C(50))));
  t44 = ((t45) != (t46));
  }
  t47 = t44;
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(((*(v0))).f8));
  t50 = ((int64_t)(((int64_t)UINT64_C(71))));
  t48 = ((t49) != (t50));
  }
  t51 = t48;
  t52 = t51;
  if (t52) {
  t53 = ((int64_t)(((*(v0))).f8));
  t54 = ((int64_t)(((int64_t)UINT64_C(91))));
  t52 = ((t53) != (t54));
  }
  t55 = t52;
  t56 = t55;
  if (t56) {
  t57 = ((int64_t)(((*(v0))).f8));
  t58 = ((int64_t)(((int64_t)UINT64_C(23))));
  t56 = ((t57) != (t58));
  }
  t59 = t56;
  if (t59) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t60 = v0;
  v4 = gg_function_14(t60, base + 0);
  t61 = &(((*(v0))).f7);
  v5 = gg_function_12(t61, base + 0);
label1:;
  t62 = ((int64_t)(v4));
  t63 = ((int64_t)(v5));
  if (!(((t62) < (t63)))) goto label2;
  {
  t64 = &(((*(v0))).f4);
  t65 = v1;
  t66 = &(((*(v0))).f4);
  t67 = &(((*(v0))).f7);
  t68 = ((int64_t)(v4));
  t69 = gg_function_46(t66, t67, t68, base + 0);
  v6 = gg_function_47(t64, t65, t69, base + 0);
  v7 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t70 = ((int64_t)(((*(v0))).f8));
  t71 = ((int64_t)(((int64_t)UINT64_C(2))));
  t72 = ((t70) == (t71));
  t73 = t72;
  if (!t73) {
  t74 = ((int64_t)(((*(v0))).f8));
  t75 = ((int64_t)(((int64_t)UINT64_C(4))));
  t73 = ((t74) == (t75));
  }
  t76 = t73;
  t77 = t76;
  if (!t77) {
  t78 = ((int64_t)(((*(v0))).f8));
  t79 = ((int64_t)(((int64_t)UINT64_C(6))));
  t77 = ((t78) == (t79));
  }
  t80 = t77;
  t81 = t80;
  if (!t81) {
  t82 = ((int64_t)(((*(v0))).f8));
  t83 = ((int64_t)(((int64_t)UINT64_C(7))));
  t81 = ((t82) == (t83));
  }
  t84 = t81;
  t85 = t84;
  if (!t85) {
  t86 = ((int64_t)(((*(v0))).f8));
  t87 = ((int64_t)(((int64_t)UINT64_C(8))));
  t85 = ((t86) == (t87));
  }
  t88 = t85;
  t89 = t88;
  if (!t89) {
  t90 = ((int64_t)(((*(v0))).f8));
  t91 = ((int64_t)(((int64_t)UINT64_C(23))));
  t89 = ((t90) == (t91));
  }
  t92 = t89;
  if (t92) {
  {
  t93 = &(((*(v0))).f5);
  t94 = v2;
  t95 = &(((*(v0))).f5);
  t96 = &(((*(v0))).f7);
  t97 = ((int64_t)(v4));
  t98 = gg_function_46(t95, t96, t97, base + 0);
  v8 = gg_function_47(t93, t94, t98, base + 0);
  t99 = ((int64_t)(((*(v0))).f8));
  t100 = ((int64_t)(((int64_t)UINT64_C(2))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (!t102) {
  t103 = ((int64_t)(((*(v0))).f8));
  t104 = ((int64_t)(((int64_t)UINT64_C(4))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  t106 = ((float)(v6));
  t107 = ((float)(v8));
  v7 = ((float)(((t106) + (t107))));
  }
  } else {
  t108 = ((int64_t)(((*(v0))).f8));
  t109 = ((int64_t)(((int64_t)UINT64_C(6))));
  t110 = ((t108) == (t109));
  if (t110) {
  {
  t111 = ((float)(v6));
  t112 = ((float)(v8));
  v7 = ((float)(((t111) - (t112))));
  }
  } else {
  t113 = ((int64_t)(((*(v0))).f8));
  t114 = ((int64_t)(((int64_t)UINT64_C(7))));
  t115 = ((t113) == (t114));
  if (t115) {
  {
  t116 = ((float)(v6));
  t117 = ((float)(v8));
  v7 = ((float)(((t116) * (t117))));
  }
  } else {
  t118 = ((int64_t)(((*(v0))).f8));
  t119 = ((int64_t)(((int64_t)UINT64_C(8))));
  t120 = ((t118) == (t119));
  if (t120) {
  {
  t121 = ((float)(v6));
  t122 = ((float)(v8));
  v7 = ((float)(((t121) / (t122))));
  }
  } else {
  {
  t123 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t124 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t125 = v8;
  t126 = ((float)((-(t125))));
  t127 = expf(t126);
  t128 = ((float)(t127));
  t129 = ((float)(((t124) + (t128))));
  v9 = ((float)(((t123) / (t129))));
  t130 = ((float)(v6));
  t131 = ((float)(v9));
  t132 = ((float)(((t130) * (t131))));
  t133 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t134 = ((float)(v8));
  t135 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t136 = ((float)(v9));
  t137 = ((float)(((t135) - (t136))));
  t138 = ((float)(((t134) * (t137))));
  t139 = ((float)(((t133) + (t138))));
  v7 = ((float)(((t132) * (t139))));
  }
  }
  }
  }
  }
  }
  } else {
  t140 = ((int64_t)(((*(v0))).f8));
  t141 = ((int64_t)(((int64_t)UINT64_C(32))));
  t142 = ((t140) == (t141));
  if (t142) {
  {
  t143 = ((float)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t144 = ((int64_t)UINT64_C(0));
  t145 = ((float *)(((*(v0))).f12).data) + gg_index(t144, ((int64_t)UINT64_C(8)));
  t146 = ((float)((*t145)));
  t147 = ((float)(((t143) * (t146))));
  /* FG_DIRECT_SLICE_INDEX */
  t148 = ((int64_t)UINT64_C(1));
  t149 = ((float *)(((*(v0))).f12).data) + gg_index(t148, ((int64_t)UINT64_C(8)));
  t150 = ((float)((*t149)));
  v7 = ((float)(((t147) + (t150))));
  }
  } else {
  t151 = ((int64_t)(((*(v0))).f8));
  t152 = ((int64_t)(((int64_t)UINT64_C(9))));
  t153 = ((t151) == (t152));
  if (t153) {
  {
  t154 = ((float)(v6));
  t155 = ((float)(v6));
  v7 = ((float)(((t154) * (t155))));
  }
  } else {
  t156 = ((int64_t)(((*(v0))).f8));
  t157 = ((int64_t)(((int64_t)UINT64_C(10))));
  t158 = ((t156) == (t157));
  if (t158) {
  {
  t159 = ((float)(v6));
  t160 = sqrtf(t159);
  v7 = ((float)(t160));
  }
  } else {
  t161 = ((int64_t)(((*(v0))).f8));
  t162 = ((int64_t)(((int64_t)UINT64_C(12))));
  t163 = ((t161) == (t162));
  if (t163) {
  {
  t164 = ((float)(v6));
  t165 = sinf(t164);
  v7 = ((float)(t165));
  }
  } else {
  t166 = ((int64_t)(((*(v0))).f8));
  t167 = ((int64_t)(((int64_t)UINT64_C(13))));
  t168 = ((t166) == (t167));
  if (t168) {
  {
  t169 = ((float)(v6));
  t170 = cosf(t169);
  v7 = ((float)(t170));
  }
  } else {
  t171 = ((int64_t)(((*(v0))).f8));
  t172 = ((int64_t)(((int64_t)UINT64_C(11))));
  t173 = ((t171) == (t172));
  if (t173) {
  {
  t174 = ((float)(v6));
  t175 = logf(t174);
  v7 = ((float)(t175));
  }
  } else {
  t176 = ((int64_t)(((*(v0))).f8));
  t177 = ((int64_t)(((int64_t)UINT64_C(50))));
  t178 = ((t176) == (t177));
  if (t178) {
  {
  t179 = ((float)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t180 = ((int64_t)UINT64_C(0));
  t181 = ((float *)(((*(v0))).f12).data) + gg_index(t180, ((int64_t)UINT64_C(8)));
  t182 = ((float)((*t181)));
  t183 = fmaxf(t179, t182);
  t184 = ((float)(t183));
  /* FG_DIRECT_SLICE_INDEX */
  t185 = ((int64_t)UINT64_C(1));
  t186 = ((float *)(((*(v0))).f12).data) + gg_index(t185, ((int64_t)UINT64_C(8)));
  t187 = ((float)((*t186)));
  t188 = fminf(t184, t187);
  v7 = ((float)(t188));
  }
  } else {
  t189 = ((int64_t)(((*(v0))).f8));
  t190 = ((int64_t)(((int64_t)UINT64_C(71))));
  t191 = ((t189) == (t190));
  if (t191) {
  {
  t192 = ((float)(v6));
  t193 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t194 = ((t192) > (t193));
  if (t194) {
  {
  v7 = ((float)(v6));
  }
  } else {
  {
  t195 = ((float)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t196 = ((int64_t)UINT64_C(0));
  t197 = ((float *)(((*(v0))).f12).data) + gg_index(t196, ((int64_t)UINT64_C(8)));
  t198 = ((float)((*t197)));
  v7 = ((float)(((t195) * (t198))));
  }
  }
  }
  } else {
  {
  t199 = v0;
  /* FG_DIRECT_SLICE_INDEX */
  t200 = ((int64_t)UINT64_C(0));
  t201 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t200, ((int64_t)UINT64_C(16)));
  t202 = ((int64_t)((*t201)));
  t203 = ((float)(v6));
  v7 = gg_function_74(t199, t202, t203, base + 0);
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
  t204 = &(((*(v0))).f7);
  t205 = v3;
  t206 = ((int64_t)(v4));
  t207 = ((float)(v7));
  gg_function_37(t204, t205, t206, t207, base + 0);
  t208 = v4;
  t209 = ((int64_t)(((*(v0))).f10));
  v4 = ((int64_t)((uint64_t)(t208) + (uint64_t)(t209)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_70(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t * t5;
  int64_t t6;
  int64_t * t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  int64_t * t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t * t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t * t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t * t33;
  int64_t t34;
  int64_t t35;
  gg_type_20 t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  gg_type_31 t45;
  gg_slice t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t * t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t * t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t * t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  gg_type_31 t66;
  gg_slice t67;
  int64_t t68;
  int64_t t69;
  int64_t * t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t * t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  float t79;
  float t80;
  float t81;
  float t82;
  gg_type_31 t83;
  gg_slice t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  float t94;
  int64_t t95;
  int64_t t96;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  float v11;
  int64_t v12;
  float v13;
  float v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(76))));
  t3 = ((t1) != (t2));
  if (t3) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t4 = ((int64_t)UINT64_C(0));
  t5 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t4, ((int64_t)UINT64_C(4)));
  v4 = ((int64_t)((*t5)));
  /* FG_DIRECT_SLICE_INDEX */
  t6 = ((int64_t)UINT64_C(1));
  t7 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t6, ((int64_t)UINT64_C(4)));
  v5 = ((int64_t)((*t7)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(1));
  t9 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  v6 = ((int64_t)((*t9)));
  /* FG_DIRECT_SLICE_INDEX */
  t10 = ((int64_t)UINT64_C(2));
  t11 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t10, ((int64_t)UINT64_C(4)));
  v7 = ((int64_t)((*t11)));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) > (t13));
  t15 = t14;
  if (t15) {
  /* FG_DIRECT_SLICE_INDEX */
  t16 = ((int64_t)UINT64_C(0));
  t17 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t16, ((int64_t)UINT64_C(4)));
  t18 = ((int64_t)((*t17)));
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) - (uint64_t)(t20)))));
  t22 = ((int64_t)(v6));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  t15 = ((t18) == (t23));
  }
  t24 = t15;
  t25 = t24;
  if (t25) {
  /* FG_DIRECT_SLICE_INDEX */
  t26 = ((int64_t)UINT64_C(1));
  t27 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t26, ((int64_t)UINT64_C(4)));
  t28 = ((int64_t)((*t27)));
  t29 = ((int64_t)(v5));
  t25 = ((t28) == (t29));
  }
  t30 = t25;
  t31 = t30;
  if (t31) {
  /* FG_DIRECT_SLICE_INDEX */
  t32 = ((int64_t)UINT64_C(0));
  t33 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t32, ((int64_t)UINT64_C(4)));
  t34 = ((int64_t)((*t33)));
  t35 = ((int64_t)(v5));
  t31 = ((t34) == (t35));
  }
  if (!(t31)) gg_fail("assertion failed");
  t36 = v0;
  v8 = gg_function_14(t36, base + 0);
label1:;
  t37 = ((int64_t)(v8));
  t38 = ((int64_t)(v5));
  if (!(((t37) < (t38)))) goto label2;
  {
  t40 = v7;
  t39 = 0;
label3:;
  if (t39 >= t40) goto label5;
  v9 = t39;
  {
  t42 = v6;
  t41 = 0;
label6:;
  if (t41 >= t42) goto label8;
  v10 = t41;
  {
  v11 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t44 = v4;
  t43 = 0;
label9:;
  if (t43 >= t44) goto label11;
  v12 = t43;
  {
  t45 = &(((*(v0))).f4);
  t46 = v1;
  t47 = ((int64_t)(v10));
  t48 = ((int64_t)(v12));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  /* FG_DIRECT_SLICE_INDEX */
  t50 = ((int64_t)UINT64_C(0));
  t51 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t50, ((int64_t)UINT64_C(4)));
  t52 = ((int64_t)((*t51)));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t52)))));
  t54 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t55 = ((int64_t)UINT64_C(1));
  t56 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t55, ((int64_t)UINT64_C(4)));
  t57 = ((int64_t)((*t56)));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t57)))));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t58)))));
  t60 = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t61 = ((int64_t)UINT64_C(2));
  t62 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t61, ((int64_t)UINT64_C(4)));
  t63 = ((int64_t)((*t62)));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t64)))));
  v13 = gg_function_47(t45, t46, t65, base + 0);
  t66 = &(((*(v0))).f5);
  t67 = v2;
  t68 = ((int64_t)(v12));
  /* FG_DIRECT_SLICE_INDEX */
  t69 = ((int64_t)UINT64_C(0));
  t70 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t69, ((int64_t)UINT64_C(4)));
  t71 = ((int64_t)((*t70)));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t71)))));
  t73 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t74 = ((int64_t)UINT64_C(1));
  t75 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t74, ((int64_t)UINT64_C(4)));
  t76 = ((int64_t)((*t75)));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t76)))));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t77)))));
  v14 = gg_function_47(t66, t67, t78, base + 0);
  t79 = v11;
  t80 = ((float)(v13));
  t81 = ((float)(v14));
  t82 = ((float)(((t80) * (t81))));
  v11 = ((t79) + (t82));
  }
label10:;
  t43++; goto label9;
label11:;
  t83 = &(((*(v0))).f7);
  t84 = v3;
  t85 = ((int64_t)(v8));
  t86 = ((int64_t)(v5));
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v6));
  t89 = ((int64_t)(v9));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) * (uint64_t)(t89)))));
  t91 = ((int64_t)(((int64_t)((uint64_t)(t87) + (uint64_t)(t90)))));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t91)))));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t85) + (uint64_t)(t92)))));
  t94 = ((float)(v11));
  gg_function_37(t83, t84, t93, t94, base + 0);
  }
label7:;
  t41++; goto label6;
label8:;
  }
label4:;
  t39++; goto label3;
label5:;
  t95 = v8;
  t96 = ((int64_t)(((*(v0))).f10));
  v8 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_68(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t p3, gg_type_31 p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  gg_type_7 t3;
  float t4;
  float t5;
  int64_t t6;
  gg_type_7 t7;
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
  gg_type_31 t25;
  gg_slice t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  gg_type_31 t32;
  gg_slice t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  gg_type_7 * t40;
  gg_slice t41;
  int64_t t42;
  float * t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  int64_t t47;
  gg_type_7 * t48;
  gg_slice t49;
  int64_t t50;
  float * t51;
  float t52;
  float t53;
  float t54;
  float t55;
  float t56;
  float t57;
  int64_t t58;
  gg_type_7 * t59;
  gg_slice t60;
  int64_t t61;
  float * t62;
  float t63;
  float t64;
  int64_t t65;
  int64_t t66;
  float t67;
  float t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  float * t73;
  int64_t t74;
  gg_type_7 * t75;
  gg_slice t76;
  int64_t t77;
  float * t78;
  float t79;
  int64_t t80;
  gg_type_7 * t81;
  gg_slice t82;
  int64_t t83;
  float * t84;
  float t85;
  float t86;
  int64_t t87;
  gg_type_7 * t88;
  gg_slice t89;
  int64_t t90;
  float * t91;
  float t92;
  int64_t t93;
  gg_type_7 * t94;
  gg_slice t95;
  int64_t t96;
  float * t97;
  float t98;
  float t99;
  int64_t t100;
  float * t101;
  float t102;
  int64_t t103;
  float * t104;
  float t105;
  int64_t t106;
  float * t107;
  float t108;
  int64_t t109;
  float * t110;
  float t111;
  int64_t t112;
  float * t113;
  float t114;
  int64_t t115;
  float * t116;
  float t117;
  int64_t t118;
  float * t119;
  float t120;
  int64_t t121;
  float * t122;
  float t123;
  int64_t t124;
  int64_t t125;
  uint8_t t126;
  float t127;
  float t128;
  float t129;
  float t130;
  float t131;
  float t132;
  int64_t t133;
  int64_t t134;
  float t135;
  gg_type_31 t136;
  gg_slice t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  float t143;
  float t144;
  gg_type_31 t145;
  gg_slice t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  float t152;
  float t153;
  float t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  uint8_t t159;
  float t160;
  float t161;
  float t162;
  float t163;
  float t164;
  float t165;
  float t166;
  float t167;
  float t168;
  float t169;
  float t170;
  float t171;
  int64_t t172;
  int64_t t173;
  double t174;
  gg_type_31 t175;
  gg_slice t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  float t182;
  float t183;
  gg_type_31 t184;
  gg_slice t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  float t191;
  float t192;
  double t193;
  int64_t t194;
  int64_t t195;
  gg_type_31 v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  gg_type_31 v4;
  gg_slice v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  gg_type_49 v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  float v15;
  float v16;
  gg_type_7 v17;
  int64_t v18;
  float v19;
  float v20;
  float v21;
  float v22;
  float v23;
  double v24;
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
  t1 = ((int64_t)(v8));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) >= (t2)))) gg_fail("assertion failed");
  memset(&t4, 0, sizeof(t4));
  t5 = ((float)(t4));
  for (t6=0; t6<8; t6++) (t3).data[t6] = t5;
  t7 = t3;
  for (t8=0; t8<4; t8++) (v9).data[t8] = t7;
  t9 = ((int64_t)(v8));
  t10 = ((int64_t)(((int64_t)UINT64_C(32))));
  t11 = ((int64_t)(((int64_t)gg_div(t9, t10))));
  t12 = ((int64_t)(((int64_t)UINT64_C(32))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t13 = ((int64_t)(v11));
  t14 = ((int64_t)(v10));
  if (!(((t13) < (t14)))) goto label2;
  {
  t16 = ((int64_t)UINT64_C(4));
  t15 = 0;
label3:;
  if (t15 >= t16) goto label5;
  v12 = t15;
  {
  t18 = ((int64_t)UINT64_C(8));
  t17 = 0;
label6:;
  if (t17 >= t18) goto label8;
  v13 = t17;
  {
  t19 = ((int64_t)(v11));
  t20 = ((int64_t)(((int64_t)UINT64_C(8))));
  t21 = ((int64_t)(v12));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t22)))));
  t24 = ((int64_t)(v13));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))));
  t25 = v0;
  t26 = v1;
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(v14));
  t29 = ((int64_t)(v3));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t30)))));
  v15 = gg_function_47(t25, t26, t31, base + 0);
  t32 = v4;
  t33 = v5;
  t34 = ((int64_t)(v6));
  t35 = ((int64_t)(v14));
  t36 = ((int64_t)(v7));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t37)))));
  v16 = gg_function_47(t32, t33, t38, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t39 = v12;
  t40 = ((gg_type_7 *)(v9).data) + gg_index(t39, ((int64_t)UINT64_C(4)));
  t41 = gg_view(((*t40)).data, 8);
  t42 = v13;
  t43 = ((float *)t41.data) + gg_index(t42, t41.len);
  t44 = ((int64_t)(((*(v0))).f2));
  t45 = ((int64_t)(((int64_t)UINT64_C(30))));
  t46 = ((t44) == (t45));
  if (t46) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t47 = v12;
  t48 = ((gg_type_7 *)(v9).data) + gg_index(t47, ((int64_t)UINT64_C(4)));
  t49 = gg_view(((*t48)).data, 8);
  t50 = v13;
  t51 = ((float *)t49.data) + gg_index(t50, t49.len);
  t52 = ((float)((*t51)));
  t53 = ((float)(v15));
  t54 = ((float)(v16));
  t55 = ((float)(((t53) * (t54))));
  (*t43) = ((float)(((t52) + (t55))));
  }
  } else {
  {
  t56 = ((float)(v15));
  t57 = ((float)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t58 = v12;
  t59 = ((gg_type_7 *)(v9).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = gg_view(((*t59)).data, 8);
  t61 = v13;
  t62 = ((float *)t60.data) + gg_index(t61, t60.len);
  t63 = ((float)((*t62)));
  t64 = fmaf(t56, t57, t63);
  (*t43) = ((float)(t64));
  }
  }
  }
label7:;
  t17++; goto label6;
label8:;
  }
label4:;
  t15++; goto label3;
label5:;
  t65 = v11;
  t66 = ((int64_t)(((int64_t)UINT64_C(32))));
  v11 = ((int64_t)((uint64_t)(t65) + (uint64_t)(t66)));
  }
  goto label1;
label2:;
  memset(&t67, 0, sizeof(t67));
  t68 = ((float)(t67));
  for (t69=0; t69<8; t69++) (v17).data[t69] = t68;
  t71 = ((int64_t)UINT64_C(8));
  t70 = 0;
label9:;
  if (t70 >= t71) goto label11;
  v18 = t70;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t72 = v18;
  t73 = ((float *)(v17).data) + gg_index(t72, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t74 = ((int64_t)UINT64_C(0));
  t75 = ((gg_type_7 *)(v9).data) + gg_index(t74, ((int64_t)UINT64_C(4)));
  t76 = gg_view(((*t75)).data, 8);
  t77 = v18;
  t78 = ((float *)t76.data) + gg_index(t77, t76.len);
  t79 = ((float)((*t78)));
  /* FG_DIRECT_SLICE_INDEX */
  t80 = ((int64_t)UINT64_C(2));
  t81 = ((gg_type_7 *)(v9).data) + gg_index(t80, ((int64_t)UINT64_C(4)));
  t82 = gg_view(((*t81)).data, 8);
  t83 = v18;
  t84 = ((float *)t82.data) + gg_index(t83, t82.len);
  t85 = ((float)((*t84)));
  t86 = ((float)(((t79) + (t85))));
  /* FG_DIRECT_SLICE_INDEX */
  t87 = ((int64_t)UINT64_C(1));
  t88 = ((gg_type_7 *)(v9).data) + gg_index(t87, ((int64_t)UINT64_C(4)));
  t89 = gg_view(((*t88)).data, 8);
  t90 = v18;
  t91 = ((float *)t89.data) + gg_index(t90, t89.len);
  t92 = ((float)((*t91)));
  /* FG_DIRECT_SLICE_INDEX */
  t93 = ((int64_t)UINT64_C(3));
  t94 = ((gg_type_7 *)(v9).data) + gg_index(t93, ((int64_t)UINT64_C(4)));
  t95 = gg_view(((*t94)).data, 8);
  t96 = v18;
  t97 = ((float *)t95.data) + gg_index(t96, t95.len);
  t98 = ((float)((*t97)));
  t99 = ((float)(((t92) + (t98))));
  (*t73) = ((float)(((t86) + (t99))));
  }
label10:;
  t70++; goto label9;
label11:;
  /* FG_DIRECT_SLICE_INDEX */
  t100 = ((int64_t)UINT64_C(0));
  t101 = ((float *)(v17).data) + gg_index(t100, ((int64_t)UINT64_C(8)));
  t102 = ((float)((*t101)));
  /* FG_DIRECT_SLICE_INDEX */
  t103 = ((int64_t)UINT64_C(4));
  t104 = ((float *)(v17).data) + gg_index(t103, ((int64_t)UINT64_C(8)));
  t105 = ((float)((*t104)));
  v19 = ((float)(((t102) + (t105))));
  /* FG_DIRECT_SLICE_INDEX */
  t106 = ((int64_t)UINT64_C(1));
  t107 = ((float *)(v17).data) + gg_index(t106, ((int64_t)UINT64_C(8)));
  t108 = ((float)((*t107)));
  /* FG_DIRECT_SLICE_INDEX */
  t109 = ((int64_t)UINT64_C(5));
  t110 = ((float *)(v17).data) + gg_index(t109, ((int64_t)UINT64_C(8)));
  t111 = ((float)((*t110)));
  v20 = ((float)(((t108) + (t111))));
  /* FG_DIRECT_SLICE_INDEX */
  t112 = ((int64_t)UINT64_C(2));
  t113 = ((float *)(v17).data) + gg_index(t112, ((int64_t)UINT64_C(8)));
  t114 = ((float)((*t113)));
  /* FG_DIRECT_SLICE_INDEX */
  t115 = ((int64_t)UINT64_C(6));
  t116 = ((float *)(v17).data) + gg_index(t115, ((int64_t)UINT64_C(8)));
  t117 = ((float)((*t116)));
  v21 = ((float)(((t114) + (t117))));
  /* FG_DIRECT_SLICE_INDEX */
  t118 = ((int64_t)UINT64_C(3));
  t119 = ((float *)(v17).data) + gg_index(t118, ((int64_t)UINT64_C(8)));
  t120 = ((float)((*t119)));
  /* FG_DIRECT_SLICE_INDEX */
  t121 = ((int64_t)UINT64_C(7));
  t122 = ((float *)(v17).data) + gg_index(t121, ((int64_t)UINT64_C(8)));
  t123 = ((float)((*t122)));
  v22 = ((float)(((t120) + (t123))));
  t124 = ((int64_t)(((*(v0))).f2));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((t124) == (t125));
  if (t126) {
  {
  t127 = ((float)(v19));
  t128 = ((float)(v20));
  t129 = ((float)(((t127) + (t128))));
  t130 = ((float)(v21));
  t131 = ((float)(v22));
  t132 = ((float)(((t130) + (t131))));
  v23 = ((float)(((t129) + (t132))));
label13:;
  t133 = ((int64_t)(v11));
  t134 = ((int64_t)(v8));
  if (!(((t133) < (t134)))) goto label14;
  {
  t135 = v23;
  t136 = v0;
  t137 = v1;
  t138 = ((int64_t)(v2));
  t139 = ((int64_t)(v11));
  t140 = ((int64_t)(v3));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t139) * (uint64_t)(t140)))));
  t142 = ((int64_t)(((int64_t)((uint64_t)(t138) + (uint64_t)(t141)))));
  t143 = gg_function_47(t136, t137, t142, base + 0);
  t144 = ((float)(t143));
  t145 = v4;
  t146 = v5;
  t147 = ((int64_t)(v6));
  t148 = ((int64_t)(v11));
  t149 = ((int64_t)(v7));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) * (uint64_t)(t149)))));
  t151 = ((int64_t)(((int64_t)((uint64_t)(t147) + (uint64_t)(t150)))));
  t152 = gg_function_47(t145, t146, t151, base + 0);
  t153 = ((float)(t152));
  t154 = ((float)(((t144) * (t153))));
  v23 = ((t135) + (t154));
  t155 = v11;
  t156 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t155) + (uint64_t)(t156)));
  }
  goto label13;
label14:;
  t0 = ((float)(v23));
  goto gg_done;
  }
  }
  t157 = ((int64_t)(((*(v0))).f2));
  t158 = ((int64_t)(((int64_t)UINT64_C(30))));
  t159 = ((t157) == (t158));
  if (t159) {
  {
  t160 = ((float)(v19));
  t161 = ((float)(v21));
  t162 = ((float)(((t160) + (t161))));
  t163 = ((float)(v20));
  t164 = ((float)(v22));
  t165 = ((float)(((t163) + (t164))));
  v24 = ((double)(((double)(((t162) + (t165))))));
  }
  } else {
  {
  t166 = ((float)(v19));
  t167 = ((float)(v20));
  t168 = ((float)(((t166) + (t167))));
  t169 = ((float)(v21));
  t170 = ((float)(v22));
  t171 = ((float)(((t169) + (t170))));
  v24 = ((double)(((double)(((t168) + (t171))))));
  }
  }
label16:;
  t172 = ((int64_t)(v11));
  t173 = ((int64_t)(v8));
  if (!(((t172) < (t173)))) goto label17;
  {
  t174 = v24;
  t175 = v0;
  t176 = v1;
  t177 = ((int64_t)(v2));
  t178 = ((int64_t)(v11));
  t179 = ((int64_t)(v3));
  t180 = ((int64_t)(((int64_t)((uint64_t)(t178) * (uint64_t)(t179)))));
  t181 = ((int64_t)(((int64_t)((uint64_t)(t177) + (uint64_t)(t180)))));
  t182 = gg_function_47(t175, t176, t181, base + 0);
  t183 = ((float)(t182));
  t184 = v4;
  t185 = v5;
  t186 = ((int64_t)(v6));
  t187 = ((int64_t)(v11));
  t188 = ((int64_t)(v7));
  t189 = ((int64_t)(((int64_t)((uint64_t)(t187) * (uint64_t)(t188)))));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t186) + (uint64_t)(t189)))));
  t191 = gg_function_47(t184, t185, t190, base + 0);
  t192 = ((float)(t191));
  t193 = ((double)(((double)(((t183) * (t192))))));
  v24 = ((t174) + (t193));
  t194 = v11;
  t195 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t194) + (uint64_t)(t195)));
  }
  goto label16;
label17:;
  t0 = ((float)(((float)(v24))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_67(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t base) {
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
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
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
  int64_t t34;
  int64_t * t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  gg_type_20 t42;
  int64_t t43;
  gg_type_31 t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t * t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t * t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t * t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t * t61;
  int64_t t62;
  int64_t t63;
  int64_t * t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
  int64_t t72;
  int64_t * t73;
  int64_t t74;
  int64_t t75;
  int64_t * t76;
  int64_t t77;
  uint8_t t78;
  uint8_t t79;
  int64_t t80;
  int64_t * t81;
  int64_t t82;
  int64_t t83;
  uint8_t t84;
  uint8_t t85;
  int64_t t86;
  int64_t * t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
  uint8_t t91;
  int64_t t92;
  int64_t * t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
  uint8_t t97;
  int64_t t98;
  int64_t * t99;
  int64_t t100;
  int64_t t101;
  int64_t * t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint8_t t106;
  uint8_t t107;
  int64_t t108;
  int64_t * t109;
  int64_t t110;
  int64_t t111;
  int64_t * t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t * t118;
  int64_t t119;
  int64_t t120;
  int64_t * t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t * t126;
  int64_t t127;
  int64_t t128;
  int64_t * t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t * t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t * t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t * t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t * t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t * t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t * t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t * t165;
  int64_t t166;
  int64_t * t167;
  int64_t t168;
  int64_t * t169;
  int64_t t170;
  int64_t t171;
  uint8_t t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  uint8_t t176;
  int64_t t177;
  int64_t * t178;
  int64_t t179;
  int64_t t180;
  int64_t * t181;
  int64_t t182;
  uint8_t t183;
  uint8_t t184;
  int64_t t185;
  int64_t * t186;
  int64_t t187;
  int64_t t188;
  uint8_t t189;
  uint8_t t190;
  int64_t t191;
  int64_t * t192;
  int64_t t193;
  int64_t t194;
  gg_slice t195;
  gg_type_31 t196;
  int64_t t197;
  int64_t t198;
  int64_t * t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  uint32_t t205;
  int32_t t206;
  int32_t t207;
  uint8_t t208;
  uint8_t t209;
  int64_t t210;
  int64_t t211;
  int64_t * t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t * t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t * t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t * t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t * t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t * t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t * t239;
  int64_t t240;
  int64_t * t241;
  int64_t t242;
  int64_t * t243;
  int64_t t244;
  int64_t * t245;
  int64_t t246;
  int64_t t247;
  int64_t * t248;
  int64_t t249;
  uint8_t t250;
  uint8_t t251;
  int64_t t252;
  int64_t * t253;
  int64_t t254;
  int64_t t255;
  uint8_t t256;
  uint8_t t257;
  int64_t t258;
  int64_t * t259;
  int64_t t260;
  int64_t t261;
  int64_t * t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t * t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t * t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  int64_t * t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  int64_t * t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t * t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  int64_t * t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t * t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t * t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  int64_t * t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t * t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t * t313;
  int64_t t314;
  int64_t * t315;
  int64_t t316;
  int64_t * t317;
  gg_type_31 t318;
  gg_slice t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  gg_type_31 t325;
  gg_slice t326;
  int64_t t327;
  int64_t t328;
  gg_type_31 t329;
  gg_slice t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  float t334;
  int64_t t335;
  int64_t t336;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
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
  int64_t v16;
  int64_t v17;
  int32_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(29))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f8));
  t6 = ((int64_t)(((int64_t)UINT64_C(30))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f8));
  t10 = ((int64_t)(((int64_t)UINT64_C(31))));
  t8 = ((t9) != (t10));
  }
  t11 = t8;
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t12 = ((int64_t)((((*(v0))).f4).f2));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) != (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)((((*(v0))).f4).f2));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((t16) != (t17));
  }
  t18 = t15;
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)((((*(v0))).f4).f2));
  t21 = ((int64_t)(((int64_t)UINT64_C(30))));
  t19 = ((t20) != (t21));
  }
  t22 = t19;
  if (t22) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t23 = ((int64_t)((((*(v0))).f5).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) != (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) != (t28));
  }
  t29 = t26;
  t30 = t29;
  if (t30) {
  t31 = ((int64_t)((((*(v0))).f5).f2));
  t32 = ((int64_t)(((int64_t)UINT64_C(30))));
  t30 = ((t31) != (t32));
  }
  t33 = t30;
  if (t33) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t34 = ((int64_t)UINT64_C(0));
  t35 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t34, ((int64_t)UINT64_C(4)));
  v5 = ((int64_t)((*t35)));
  t36 = ((int64_t)(v5));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) > (t37));
  t39 = t38;
  if (t39) {
  t40 = ((int64_t)((((*(v0))).f7).f2));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t40) == (t41));
  }
  if (!(t39)) gg_fail("assertion failed");
  t42 = v0;
  v6 = gg_function_14(t42, base + 0);
label1:;
  t43 = ((int64_t)(v6));
  t44 = &(((*(v0))).f7);
  t45 = gg_function_13(t44, base + 0);
  t46 = ((int64_t)(t45));
  if (!(((t43) < (t46)))) goto label2;
  {
  t47 = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t48 = ((int64_t)UINT64_C(1));
  t49 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t48, ((int64_t)UINT64_C(4)));
  t50 = ((int64_t)((*t49)));
  v7 = ((int64_t)(((int64_t)gg_mod(t47, t50))));
  t51 = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t52 = ((int64_t)UINT64_C(1));
  t53 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t52, ((int64_t)UINT64_C(4)));
  t54 = ((int64_t)((*t53)));
  t55 = ((int64_t)(((int64_t)gg_div(t51, t54))));
  /* FG_DIRECT_SLICE_INDEX */
  t56 = ((int64_t)UINT64_C(2));
  t57 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t56, ((int64_t)UINT64_C(4)));
  t58 = ((int64_t)((*t57)));
  v8 = ((int64_t)(((int64_t)gg_mod(t55, t58))));
  t59 = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t60 = ((int64_t)UINT64_C(1));
  t61 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t60, ((int64_t)UINT64_C(4)));
  t62 = ((int64_t)((*t61)));
  /* FG_DIRECT_SLICE_INDEX */
  t63 = ((int64_t)UINT64_C(2));
  t64 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t63, ((int64_t)UINT64_C(4)));
  t65 = ((int64_t)((*t64)));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t65)))));
  v9 = ((int64_t)(((int64_t)gg_div(t59, t66))));
  t68 = v5;
  t67 = 0;
label3:;
  if (t67 >= t68) goto label5;
  v10 = t67;
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((int64_t)(((*(v0))).f8));
  t70 = ((int64_t)(((int64_t)UINT64_C(29))));
  t71 = ((t69) == (t70));
  if (t71) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t72 = ((int64_t)UINT64_C(0));
  t73 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t72, ((int64_t)UINT64_C(4)));
  t74 = ((int64_t)((*t73)));
  /* FG_DIRECT_SLICE_INDEX */
  t75 = ((int64_t)UINT64_C(0));
  t76 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t75, ((int64_t)UINT64_C(4)));
  t77 = ((int64_t)((*t76)));
  t78 = ((t74) == (t77));
  t79 = t78;
  if (t79) {
  /* FG_DIRECT_SLICE_INDEX */
  t80 = ((int64_t)UINT64_C(1));
  t81 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t80, ((int64_t)UINT64_C(4)));
  t82 = ((int64_t)((*t81)));
  t83 = ((int64_t)(v5));
  t79 = ((t82) == (t83));
  }
  t84 = t79;
  t85 = t84;
  if (t85) {
  /* FG_DIRECT_SLICE_INDEX */
  t86 = ((int64_t)UINT64_C(2));
  t87 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t86, ((int64_t)UINT64_C(4)));
  t88 = ((int64_t)((*t87)));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((t88) > (t89));
  }
  t90 = t85;
  t91 = t90;
  if (t91) {
  /* FG_DIRECT_SLICE_INDEX */
  t92 = ((int64_t)UINT64_C(3));
  t93 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t92, ((int64_t)UINT64_C(4)));
  t94 = ((int64_t)((*t93)));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = ((t94) > (t95));
  }
  t96 = t91;
  t97 = t96;
  if (t97) {
  /* FG_DIRECT_SLICE_INDEX */
  t98 = ((int64_t)UINT64_C(2));
  t99 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t98, ((int64_t)UINT64_C(4)));
  t100 = ((int64_t)((*t99)));
  /* FG_DIRECT_SLICE_INDEX */
  t101 = ((int64_t)UINT64_C(2));
  t102 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t101, ((int64_t)UINT64_C(4)));
  t103 = ((int64_t)((*t102)));
  t104 = ((int64_t)(((int64_t)gg_mod(t100, t103))));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = ((t104) == (t105));
  }
  t106 = t97;
  t107 = t106;
  if (t107) {
  /* FG_DIRECT_SLICE_INDEX */
  t108 = ((int64_t)UINT64_C(3));
  t109 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t108, ((int64_t)UINT64_C(4)));
  t110 = ((int64_t)((*t109)));
  /* FG_DIRECT_SLICE_INDEX */
  t111 = ((int64_t)UINT64_C(3));
  t112 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t111, ((int64_t)UINT64_C(4)));
  t113 = ((int64_t)((*t112)));
  t114 = ((int64_t)(((int64_t)gg_mod(t110, t113))));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = ((t114) == (t115));
  }
  if (!(t107)) gg_fail("assertion failed");
  t116 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t117 = ((int64_t)UINT64_C(2));
  t118 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t117, ((int64_t)UINT64_C(4)));
  t119 = ((int64_t)((*t118)));
  /* FG_DIRECT_SLICE_INDEX */
  t120 = ((int64_t)UINT64_C(2));
  t121 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t120, ((int64_t)UINT64_C(4)));
  t122 = ((int64_t)((*t121)));
  t123 = ((int64_t)(((int64_t)gg_div(t119, t122))));
  v16 = ((int64_t)(((int64_t)gg_div(t116, t123))));
  t124 = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t125 = ((int64_t)UINT64_C(3));
  t126 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t125, ((int64_t)UINT64_C(4)));
  t127 = ((int64_t)((*t126)));
  /* FG_DIRECT_SLICE_INDEX */
  t128 = ((int64_t)UINT64_C(3));
  t129 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t128, ((int64_t)UINT64_C(4)));
  t130 = ((int64_t)((*t129)));
  t131 = ((int64_t)(((int64_t)gg_div(t127, t130))));
  v17 = ((int64_t)(((int64_t)gg_div(t124, t131))));
  t132 = ((int64_t)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t133 = ((int64_t)UINT64_C(1));
  t134 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t133, ((int64_t)UINT64_C(4)));
  t135 = ((int64_t)((*t134)));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t132) * (uint64_t)(t135)))));
  t137 = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t138 = ((int64_t)UINT64_C(2));
  t139 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t138, ((int64_t)UINT64_C(4)));
  t140 = ((int64_t)((*t139)));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t137) * (uint64_t)(t140)))));
  t142 = ((int64_t)(((int64_t)((uint64_t)(t136) + (uint64_t)(t141)))));
  t143 = ((int64_t)(v17));
  /* FG_DIRECT_SLICE_INDEX */
  t144 = ((int64_t)UINT64_C(3));
  t145 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t144, ((int64_t)UINT64_C(4)));
  t146 = ((int64_t)((*t145)));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t143) * (uint64_t)(t146)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t142) + (uint64_t)(t147)))));
  t148 = ((int64_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t149 = ((int64_t)UINT64_C(1));
  t150 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t149, ((int64_t)UINT64_C(4)));
  t151 = ((int64_t)((*t150)));
  t152 = ((int64_t)(((int64_t)((uint64_t)(t148) * (uint64_t)(t151)))));
  t153 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t154 = ((int64_t)UINT64_C(2));
  t155 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t154, ((int64_t)UINT64_C(4)));
  t156 = ((int64_t)((*t155)));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t153) * (uint64_t)(t156)))));
  t158 = ((int64_t)(((int64_t)((uint64_t)(t152) + (uint64_t)(t157)))));
  t159 = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t160 = ((int64_t)UINT64_C(3));
  t161 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t160, ((int64_t)UINT64_C(4)));
  t162 = ((int64_t)((*t161)));
  t163 = ((int64_t)(((int64_t)((uint64_t)(t159) * (uint64_t)(t162)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t158) + (uint64_t)(t163)))));
  /* FG_DIRECT_SLICE_INDEX */
  t164 = ((int64_t)UINT64_C(0));
  t165 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t164, ((int64_t)UINT64_C(4)));
  v13 = ((int64_t)((*t165)));
  /* FG_DIRECT_SLICE_INDEX */
  t166 = ((int64_t)UINT64_C(0));
  t167 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t166, ((int64_t)UINT64_C(4)));
  v14 = ((int64_t)((*t167)));
  /* FG_DIRECT_SLICE_INDEX */
  t168 = ((int64_t)UINT64_C(0));
  t169 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t168, ((int64_t)UINT64_C(4)));
  v15 = ((int64_t)((*t169)));
  }
  } else {
  t170 = ((int64_t)(((*(v0))).f8));
  t171 = ((int64_t)(((int64_t)UINT64_C(30))));
  t172 = ((t170) == (t171));
  if (t172) {
  {
  t173 = ((int64_t)((((*(v0))).f6).f2));
  t174 = ((int64_t)(((int64_t)UINT64_C(26))));
  t175 = ((t173) == (t174));
  t176 = t175;
  if (t176) {
  /* FG_DIRECT_SLICE_INDEX */
  t177 = ((int64_t)UINT64_C(0));
  t178 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t177, ((int64_t)UINT64_C(4)));
  t179 = ((int64_t)((*t178)));
  /* FG_DIRECT_SLICE_INDEX */
  t180 = ((int64_t)UINT64_C(0));
  t181 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t180, ((int64_t)UINT64_C(4)));
  t182 = ((int64_t)((*t181)));
  t176 = ((t179) == (t182));
  }
  t183 = t176;
  t184 = t183;
  if (t184) {
  /* FG_DIRECT_SLICE_INDEX */
  t185 = ((int64_t)UINT64_C(1));
  t186 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t185, ((int64_t)UINT64_C(4)));
  t187 = ((int64_t)((*t186)));
  t188 = ((int64_t)(v5));
  t184 = ((t187) == (t188));
  }
  t189 = t184;
  t190 = t189;
  if (t190) {
  /* FG_DIRECT_SLICE_INDEX */
  t191 = ((int64_t)UINT64_C(1));
  t192 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t191, ((int64_t)UINT64_C(4)));
  t193 = ((int64_t)((*t192)));
  t194 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = ((t193) > (t194));
  }
  if (!(t190)) gg_fail("assertion failed");
  t195 = v3;
  t196 = &(((*(v0))).f6);
  t197 = ((int64_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t198 = ((int64_t)UINT64_C(0));
  t199 = ((int64_t *)((((*(v0))).f6).f0).data) + gg_index(t198, ((int64_t)UINT64_C(4)));
  t200 = ((int64_t)((*t199)));
  t201 = ((int64_t)(v8));
  t202 = ((int64_t)(((int64_t)((uint64_t)(t200) * (uint64_t)(t201)))));
  t203 = ((int64_t)(((int64_t)((uint64_t)(t197) + (uint64_t)(t202)))));
  t204 = gg_function_16(t196, t203, base + 0);
  t205 = gg_function_17(t195, t204, base + 0);
  v18 = ((int32_t)(((int32_t)(t205))));
  t206 = ((int32_t)(v18));
  t207 = ((int32_t)(((int64_t)UINT64_C(0))));
  t208 = ((t206) >= (t207));
  t209 = t208;
  if (t209) {
  t210 = ((int64_t)(((int64_t)(v18))));
  /* FG_DIRECT_SLICE_INDEX */
  t211 = ((int64_t)UINT64_C(2));
  t212 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t211, ((int64_t)UINT64_C(4)));
  t213 = ((int64_t)((*t212)));
  t209 = ((t210) < (t213));
  }
  if (!(t209)) gg_fail("assertion failed");
  t214 = ((int64_t)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t215 = ((int64_t)UINT64_C(1));
  t216 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t215, ((int64_t)UINT64_C(4)));
  t217 = ((int64_t)((*t216)));
  t218 = ((int64_t)(((int64_t)((uint64_t)(t214) * (uint64_t)(t217)))));
  t219 = ((int64_t)(((int64_t)(v18))));
  /* FG_DIRECT_SLICE_INDEX */
  t220 = ((int64_t)UINT64_C(2));
  t221 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t220, ((int64_t)UINT64_C(4)));
  t222 = ((int64_t)((*t221)));
  t223 = ((int64_t)(((int64_t)((uint64_t)(t219) * (uint64_t)(t222)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t218) + (uint64_t)(t223)))));
  t224 = ((int64_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t225 = ((int64_t)UINT64_C(1));
  t226 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t225, ((int64_t)UINT64_C(4)));
  t227 = ((int64_t)((*t226)));
  t228 = ((int64_t)(((int64_t)gg_mod(t224, t227))));
  /* FG_DIRECT_SLICE_INDEX */
  t229 = ((int64_t)UINT64_C(1));
  t230 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t229, ((int64_t)UINT64_C(4)));
  t231 = ((int64_t)((*t230)));
  t232 = ((int64_t)(((int64_t)((uint64_t)(t228) * (uint64_t)(t231)))));
  t233 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t234 = ((int64_t)UINT64_C(2));
  t235 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t234, ((int64_t)UINT64_C(4)));
  t236 = ((int64_t)((*t235)));
  t237 = ((int64_t)(((int64_t)((uint64_t)(t233) * (uint64_t)(t236)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t232) + (uint64_t)(t237)))));
  /* FG_DIRECT_SLICE_INDEX */
  t238 = ((int64_t)UINT64_C(0));
  t239 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t238, ((int64_t)UINT64_C(4)));
  v13 = ((int64_t)((*t239)));
  /* FG_DIRECT_SLICE_INDEX */
  t240 = ((int64_t)UINT64_C(0));
  t241 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t240, ((int64_t)UINT64_C(4)));
  v14 = ((int64_t)((*t241)));
  /* FG_DIRECT_SLICE_INDEX */
  t242 = ((int64_t)UINT64_C(0));
  t243 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t242, ((int64_t)UINT64_C(4)));
  v15 = ((int64_t)((*t243)));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t244 = ((int64_t)UINT64_C(1));
  t245 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t244, ((int64_t)UINT64_C(4)));
  t246 = ((int64_t)((*t245)));
  /* FG_DIRECT_SLICE_INDEX */
  t247 = ((int64_t)UINT64_C(1));
  t248 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t247, ((int64_t)UINT64_C(4)));
  t249 = ((int64_t)((*t248)));
  t250 = ((t246) == (t249));
  t251 = t250;
  if (t251) {
  /* FG_DIRECT_SLICE_INDEX */
  t252 = ((int64_t)UINT64_C(0));
  t253 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t252, ((int64_t)UINT64_C(4)));
  t254 = ((int64_t)((*t253)));
  t255 = ((int64_t)(v5));
  t251 = ((t254) == (t255));
  }
  t256 = t251;
  t257 = t256;
  if (t257) {
  /* FG_DIRECT_SLICE_INDEX */
  t258 = ((int64_t)UINT64_C(0));
  t259 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t258, ((int64_t)UINT64_C(4)));
  t260 = ((int64_t)((*t259)));
  /* FG_DIRECT_SLICE_INDEX */
  t261 = ((int64_t)UINT64_C(1));
  t262 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t261, ((int64_t)UINT64_C(4)));
  t263 = ((int64_t)((*t262)));
  t257 = ((t260) == (t263));
  }
  if (!(t257)) gg_fail("assertion failed");
  t264 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t265 = ((int64_t)UINT64_C(2));
  t266 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t265, ((int64_t)UINT64_C(4)));
  t267 = ((int64_t)((*t266)));
  v19 = ((int64_t)(((int64_t)gg_mod(t264, t267))));
  t268 = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t269 = ((int64_t)UINT64_C(3));
  t270 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t269, ((int64_t)UINT64_C(4)));
  t271 = ((int64_t)((*t270)));
  v20 = ((int64_t)(((int64_t)gg_mod(t268, t271))));
  t272 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t273 = ((int64_t)UINT64_C(2));
  t274 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t273, ((int64_t)UINT64_C(4)));
  t275 = ((int64_t)((*t274)));
  v21 = ((int64_t)(((int64_t)gg_mod(t272, t275))));
  t276 = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t277 = ((int64_t)UINT64_C(3));
  t278 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t277, ((int64_t)UINT64_C(4)));
  t279 = ((int64_t)((*t278)));
  v22 = ((int64_t)(((int64_t)gg_mod(t276, t279))));
  t280 = ((int64_t)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t281 = ((int64_t)UINT64_C(0));
  t282 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t281, ((int64_t)UINT64_C(4)));
  t283 = ((int64_t)((*t282)));
  t284 = ((int64_t)(((int64_t)((uint64_t)(t280) * (uint64_t)(t283)))));
  t285 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t286 = ((int64_t)UINT64_C(2));
  t287 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t286, ((int64_t)UINT64_C(4)));
  t288 = ((int64_t)((*t287)));
  t289 = ((int64_t)(((int64_t)((uint64_t)(t285) * (uint64_t)(t288)))));
  t290 = ((int64_t)(((int64_t)((uint64_t)(t284) + (uint64_t)(t289)))));
  t291 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t292 = ((int64_t)UINT64_C(3));
  t293 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t292, ((int64_t)UINT64_C(4)));
  t294 = ((int64_t)((*t293)));
  t295 = ((int64_t)(((int64_t)((uint64_t)(t291) * (uint64_t)(t294)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t290) + (uint64_t)(t295)))));
  t296 = ((int64_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t297 = ((int64_t)UINT64_C(0));
  t298 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t297, ((int64_t)UINT64_C(4)));
  t299 = ((int64_t)((*t298)));
  t300 = ((int64_t)(((int64_t)((uint64_t)(t296) * (uint64_t)(t299)))));
  t301 = ((int64_t)(v21));
  /* FG_DIRECT_SLICE_INDEX */
  t302 = ((int64_t)UINT64_C(2));
  t303 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t302, ((int64_t)UINT64_C(4)));
  t304 = ((int64_t)((*t303)));
  t305 = ((int64_t)(((int64_t)((uint64_t)(t301) * (uint64_t)(t304)))));
  t306 = ((int64_t)(((int64_t)((uint64_t)(t300) + (uint64_t)(t305)))));
  t307 = ((int64_t)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t308 = ((int64_t)UINT64_C(3));
  t309 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t308, ((int64_t)UINT64_C(4)));
  t310 = ((int64_t)((*t309)));
  t311 = ((int64_t)(((int64_t)((uint64_t)(t307) * (uint64_t)(t310)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t306) + (uint64_t)(t311)))));
  /* FG_DIRECT_SLICE_INDEX */
  t312 = ((int64_t)UINT64_C(1));
  t313 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t312, ((int64_t)UINT64_C(4)));
  v13 = ((int64_t)((*t313)));
  /* FG_DIRECT_SLICE_INDEX */
  t314 = ((int64_t)UINT64_C(1));
  t315 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t314, ((int64_t)UINT64_C(4)));
  v14 = ((int64_t)((*t315)));
  /* FG_DIRECT_SLICE_INDEX */
  t316 = ((int64_t)UINT64_C(1));
  t317 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t316, ((int64_t)UINT64_C(4)));
  v15 = ((int64_t)((*t317)));
  }
  }
  }
  t318 = &(((*(v0))).f7);
  t319 = v4;
  t320 = ((int64_t)(v6));
  t321 = ((int64_t)(v5));
  t322 = ((int64_t)(((int64_t)((uint64_t)(t320) * (uint64_t)(t321)))));
  t323 = ((int64_t)(v10));
  t324 = ((int64_t)(((int64_t)((uint64_t)(t322) + (uint64_t)(t323)))));
  t325 = &(((*(v0))).f4);
  t326 = v1;
  t327 = ((int64_t)(v11));
  t328 = ((int64_t)(v13));
  t329 = &(((*(v0))).f5);
  t330 = v2;
  t331 = ((int64_t)(v12));
  t332 = ((int64_t)(v14));
  t333 = ((int64_t)(v15));
  t334 = gg_function_68(t325, t326, t327, t328, t329, t330, t331, t332, t333, base + 0);
  gg_function_37(t318, t319, t324, t334, base + 0);
  }
label4:;
  t67++; goto label3;
label5:;
  t335 = v6;
  t336 = ((int64_t)(((*(v0))).f10));
  v6 = ((int64_t)((uint64_t)(t335) + (uint64_t)(t336)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_4(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
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
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(26))));
  t4 = ((t5) == (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(4))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((t8) == (t9));
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(30))));
  t11 = ((t12) == (t13));
  }
  t14 = t11;
  t15 = t14;
  if (!t15) {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(25))));
  t15 = ((t16) == (t17));
  }
  t18 = t15;
  if (t18) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(24))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(27))));
  t24 = ((t22) == (t23));
  t25 = t24;
  if (!t25) {
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(28))));
  t25 = ((t26) == (t27));
  }
  t28 = t25;
  if (t28) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(8))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_47 gg_function_64(float p0, float p1, gg_type_47 p2, int64_t p3, float p4, float p5, int64_t base) {
  gg_type_47 t0;
  float t1;
  float t2;
  float t3;
  float t4;
  uint8_t t5;
  float t6;
  float t7;
  float t8;
  int64_t t9;
  int64_t t10;
  float t11;
  int64_t t12;
  float * t13;
  float t14;
  float t15;
  float t16;
  int64_t t17;
  float * t18;
  float t19;
  int64_t t20;
  float * t21;
  float t22;
  float t23;
  float t24;
  float t25;
  float t26;
  float t27;
  float t28;
  float t29;
  float t30;
  float t31;
  float t32;
  float t33;
  float t34;
  float t35;
  float t36;
  float t37;
  float t38;
  float t39;
  float t40;
  float t41;
  float t42;
  float t43;
  float t44;
  float t45;
  float t46;
  float t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
  int64_t t53;
  int64_t t54;
  float * t55;
  float t56;
  float t57;
  float t58;
  float t59;
  int64_t t60;
  float * t61;
  float t62;
  float t63;
  float t64;
  float t65;
  float v0;
  float v1;
  gg_type_47 v2;
  int64_t v3;
  float v4;
  float v5;
  float v6;
  float v7;
  float v8;
  float v9;
  gg_type_47 v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  v6 = ((float)(((t1) * (t2))));
  v7 = ((float)(v5));
  t3 = ((float)(v4));
  t4 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t5 = ((t3) != (t4));
  if (t5) {
  {
  t6 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t7 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t8 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((float)(((float)(((int64_t)gg_div(t9, t10))))));
  /* FG_DIRECT_SLICE_INDEX */
  t12 = ((int64_t)UINT64_C(0));
  t13 = ((float *)(v2).data) + gg_index(t12, ((int64_t)UINT64_C(2)));
  t14 = ((float)((*t13)));
  t15 = ((float)(((t11) - (t14))));
  t16 = ((float)(((float)(gg_double_bits(UINT64_C(4562254508917369340))))));
  /* FG_DIRECT_SLICE_INDEX */
  t17 = ((int64_t)UINT64_C(1));
  t18 = ((float *)(v2).data) + gg_index(t17, ((int64_t)UINT64_C(2)));
  t19 = ((float)((*t18)));
  /* FG_DIRECT_SLICE_INDEX */
  t20 = ((int64_t)UINT64_C(0));
  t21 = ((float *)(v2).data) + gg_index(t20, ((int64_t)UINT64_C(2)));
  t22 = ((float)((*t21)));
  t23 = ((float)(((t19) - (t22))));
  t24 = fmaxf(t16, t23);
  t25 = ((float)(t24));
  t26 = ((float)(((t15) / (t25))));
  t27 = fmaxf(t8, t26);
  t28 = ((float)(t27));
  t29 = fminf(t7, t28);
  t30 = ((float)(t29));
  v8 = ((float)(((t6) - (t30))));
  t31 = ((float)(v8));
  t32 = ((float)(v4));
  v9 = ((float)(((t31) * (t32))));
  t33 = ((float)(v6));
  t34 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t35 = ((float)(v9));
  t36 = ((float)(((t34) - (t35))));
  t37 = ((float)(((t33) * (t36))));
  t38 = ((float)(v0));
  t39 = ((float)(v9));
  t40 = ((float)(((t38) * (t39))));
  v6 = ((float)(((t37) + (t40))));
  t41 = v7;
  t42 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t43 = ((float)(((float)(gg_double_bits(UINT64_C(4591870180066957722))))));
  t44 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t45 = ((float)(v1));
  t46 = ((float)(((t44) / (t45))));
  t47 = logf(t46);
  t48 = ((float)(t47));
  t49 = ((float)(((t43) * (t48))));
  t50 = ((float)(((t42) + (t49))));
  v7 = ((t41) * (t50));
  }
  }
  memset(&t51, 0, sizeof(t51));
  t52 = ((float)(t51));
  for (t53=0; t53<2; t53++) (v10).data[t53] = t52;
  /* FG_DIRECT_SLICE_INDEX */
  t54 = ((int64_t)UINT64_C(0));
  t55 = ((float *)(v10).data) + gg_index(t54, ((int64_t)UINT64_C(2)));
  t56 = ((float)(v6));
  t57 = cosf(t56);
  t58 = ((float)(t57));
  t59 = ((float)(v7));
  (*t55) = ((float)(((t58) * (t59))));
  /* FG_DIRECT_SLICE_INDEX */
  t60 = ((int64_t)UINT64_C(1));
  t61 = ((float *)(v10).data) + gg_index(t60, ((int64_t)UINT64_C(2)));
  t62 = ((float)(v6));
  t63 = sinf(t62);
  t64 = ((float)(t63));
  t65 = ((float)(v7));
  (*t61) = ((float)(((t64) * (t65))));
  t0 = v10;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_57(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  gg_type_31 t8;
  gg_type_31 t9;
  uint8_t t10;
  uint8_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t * t24;
  int64_t t25;
  int64_t * t26;
  int64_t t27;
  int64_t * t28;
  int64_t t29;
  int64_t * t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  uint8_t t58;
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
  float * t69;
  float t70;
  float t71;
  uint8_t t72;
  uint8_t t73;
  int64_t t74;
  float * t75;
  float t76;
  float t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint8_t t91;
  uint8_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  uint8_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  float * t114;
  float t115;
  float t116;
  float t117;
  float t118;
  float t119;
  float t120;
  int64_t t121;
  int64_t t122;
  uint8_t t123;
  float t124;
  float t125;
  float t126;
  int64_t t127;
  float t128;
  int64_t t129;
  float * t130;
  float t131;
  float t132;
  float t133;
  int64_t t134;
  float * t135;
  float t136;
  float t137;
  float t138;
  int64_t t139;
  float * t140;
  float t141;
  float t142;
  float t143;
  float t144;
  float t145;
  float t146;
  float t147;
  float t148;
  float t149;
  float t150;
  float t151;
  float t152;
  int64_t t153;
  float * t154;
  int64_t t155;
  int64_t t156;
  float t157;
  float t158;
  float t159;
  int64_t t160;
  float * t161;
  float t162;
  float t163;
  float t164;
  float t165;
  float t166;
  float t167;
  float t168;
  float t169;
  float t170;
  float t171;
  float t172;
  float t173;
  int64_t t174;
  int64_t t175;
  gg_type_20 t176;
  int64_t t177;
  gg_type_31 t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t * t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t * t187;
  int64_t t188;
  float t189;
  float t190;
  int64_t t191;
  float t192;
  float t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  uint8_t t199;
  int64_t t200;
  int64_t t201;
  int64_t * t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  float * t206;
  gg_slice t207;
  gg_type_31 t208;
  int64_t t209;
  int64_t t210;
  uint32_t t211;
  int64_t t212;
  float * t213;
  int64_t t214;
  float * t215;
  int64_t t216;
  int64_t * t217;
  int64_t t218;
  int64_t t219;
  int64_t * t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t * t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t * t228;
  int64_t t229;
  uint8_t t230;
  int64_t t231;
  int64_t t232;
  uint8_t t233;
  uint8_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  float * t244;
  uint8_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t * t249;
  int64_t t250;
  int64_t t251;
  int64_t * t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t * t256;
  int64_t t257;
  uint8_t t258;
  int64_t t259;
  int64_t t260;
  uint8_t t261;
  int64_t t262;
  float * t263;
  int64_t t264;
  float * t265;
  int64_t t266;
  int64_t t267;
  int64_t * t268;
  int64_t t269;
  uint8_t t270;
  int64_t t271;
  float * t272;
  int64_t t273;
  float * t274;
  int64_t t275;
  int64_t t276;
  uint8_t t277;
  int64_t t278;
  float * t279;
  int64_t t280;
  float * t281;
  int64_t t282;
  int64_t t283;
  uint8_t t284;
  int64_t t285;
  float * t286;
  int64_t t287;
  float * t288;
  uint8_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  uint8_t t294;
  uint8_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t * t299;
  int64_t t300;
  int64_t t301;
  uint8_t t302;
  int64_t t303;
  float * t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  uint8_t t309;
  uint8_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t * t314;
  int64_t t315;
  int64_t t316;
  uint8_t t317;
  int64_t t318;
  float * t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  uint8_t t324;
  uint8_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  int64_t * t329;
  int64_t t330;
  int64_t t331;
  uint8_t t332;
  int64_t t333;
  float * t334;
  int64_t t335;
  float * t336;
  int64_t t337;
  int64_t t338;
  int64_t * t339;
  int64_t t340;
  uint8_t t341;
  int64_t t342;
  int64_t t343;
  uint8_t t344;
  int64_t t345;
  int64_t t346;
  uint8_t t347;
  int64_t t348;
  float * t349;
  int64_t t350;
  int64_t * t351;
  int64_t t352;
  int64_t t353;
  uint8_t t354;
  gg_type_31 t355;
  gg_slice t356;
  int64_t t357;
  float t358;
  float t359;
  float t360;
  int64_t t361;
  float * t362;
  float t363;
  gg_type_47 t364;
  int64_t t365;
  int64_t t366;
  float * t367;
  float t368;
  int64_t t369;
  float * t370;
  float t371;
  gg_slice t372;
  int64_t t373;
  int64_t t374;
  int64_t t375;
  float * t376;
  int64_t t377;
  float * t378;
  gg_slice t379;
  int64_t t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  int64_t t384;
  float * t385;
  int64_t t386;
  float * t387;
  float t388;
  float t389;
  int64_t t390;
  int64_t t391;
  int64_t t392;
  float * t393;
  float t394;
  float t395;
  uint8_t t396;
  int64_t t397;
  int64_t t398;
  uint8_t t399;
  uint8_t t400;
  int64_t t401;
  int64_t t402;
  int64_t t403;
  int64_t t404;
  uint8_t t405;
  int64_t t406;
  int64_t t407;
  int64_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  int64_t t413;
  int64_t t414;
  int64_t t415;
  gg_type_31 t416;
  gg_slice t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  int64_t t421;
  int64_t t422;
  gg_type_31 t423;
  gg_slice t424;
  int64_t t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  int64_t t429;
  int64_t t430;
  int64_t t431;
  gg_slice t432;
  int64_t t433;
  int64_t t434;
  int64_t t435;
  float * t436;
  gg_slice t437;
  int64_t t438;
  int64_t t439;
  int64_t t440;
  int64_t t441;
  int64_t t442;
  float * t443;
  gg_type_31 t444;
  gg_slice t445;
  int64_t t446;
  int64_t t447;
  int64_t t448;
  int64_t t449;
  int64_t t450;
  float t451;
  float t452;
  float t453;
  float t454;
  float t455;
  float t456;
  float t457;
  gg_type_31 t458;
  gg_slice t459;
  int64_t t460;
  int64_t t461;
  int64_t t462;
  int64_t t463;
  int64_t t464;
  int64_t t465;
  int64_t t466;
  float t467;
  float t468;
  float t469;
  float t470;
  float t471;
  float t472;
  float t473;
  uint8_t t474;
  uint8_t t475;
  int64_t t476;
  int64_t t477;
  int64_t t478;
  int64_t t479;
  int64_t t480;
  uint8_t t481;
  uint8_t t482;
  int64_t t483;
  int64_t t484;
  int64_t t485;
  int64_t t486;
  uint8_t t487;
  gg_type_31 t488;
  int64_t t489;
  int64_t t490;
  int64_t t491;
  int64_t t492;
  int64_t t493;
  gg_type_31 t494;
  int64_t t495;
  int64_t t496;
  int64_t t497;
  int64_t t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  gg_slice t502;
  int64_t t503;
  int64_t t504;
  int64_t t505;
  uint8_t * t506;
  gg_slice t507;
  int64_t t508;
  int64_t t509;
  int64_t t510;
  uint8_t * t511;
  int64_t t512;
  int64_t t513;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  uint8_t v11;
  uint8_t v12;
  uint8_t v13;
  float v14;
  float v15;
  float v16;
  gg_type_47 v17;
  float v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  gg_type_48 v22;
  gg_type_48 v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  float v29;
  int64_t v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  float v34;
  gg_type_47 v35;
  int64_t v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
  int64_t v42;
  float v43;
  float v44;
  float v45;
  float v46;
  int64_t v47;
  int64_t v48;
  int64_t v49;
  int64_t v50;
  int64_t v51;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(48))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f8));
  t6 = ((int64_t)(((int64_t)UINT64_C(49))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t8 = &(((*(v0))).f4);
  t9 = &(((*(v0))).f7);
  t10 = gg_function_58(t8, t9, base + 0);
  t11 = t10;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)((((*(v0))).f5).f2));
  t14 = ((int64_t)(((int64_t)UINT64_C(26))));
  t12 = ((t13) == (t14));
  }
  t15 = t12;
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)((((*(v0))).f4).f2));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) == (t18));
  t20 = t19;
  if (!t20) {
  t21 = ((int64_t)((((*(v0))).f4).f2));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((t21) == (t22));
  }
  t16 = t20;
  }
  if (!(t16)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t23 = ((int64_t)UINT64_C(0));
  t24 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t23, ((int64_t)UINT64_C(16)));
  v6 = ((int64_t)((*t24)));
  /* FG_DIRECT_SLICE_INDEX */
  t25 = ((int64_t)UINT64_C(1));
  t26 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t25, ((int64_t)UINT64_C(16)));
  v7 = ((int64_t)((*t26)));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = ((int64_t)UINT64_C(2));
  t28 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t27, ((int64_t)UINT64_C(16)));
  v8 = ((int64_t)((*t28)));
  /* FG_DIRECT_SLICE_INDEX */
  t29 = ((int64_t)UINT64_C(3));
  t30 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t29, ((int64_t)UINT64_C(16)));
  v9 = ((int64_t)((*t30)));
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  v10 = ((int64_t)((*t32)));
  t33 = ((int64_t)(v6));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) > (t34));
  t36 = t35;
  if (t36) {
  t37 = ((int64_t)(v6));
  t38 = ((int64_t)(((int64_t)UINT64_C(2))));
  t39 = ((int64_t)(((int64_t)gg_mod(t37, t38))));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t39) == (t40));
  }
  t41 = t36;
  t42 = t41;
  if (t42) {
  t43 = ((int64_t)(v9));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t43) >= (t44));
  }
  t45 = t42;
  t46 = t45;
  if (t46) {
  t47 = ((int64_t)(v9));
  t48 = ((int64_t)(((int64_t)UINT64_C(2))));
  t49 = ((int64_t)(((int64_t)gg_mod(t47, t48))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((t49) == (t50));
  }
  t51 = t46;
  t52 = t51;
  if (t52) {
  t53 = ((int64_t)(v9));
  t54 = ((int64_t)(v6));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t54)))));
  t56 = ((int64_t)(v10));
  t52 = ((t55) <= (t56));
  }
  t57 = t52;
  t58 = t57;
  if (t58) {
  t59 = ((int64_t)(v10));
  t60 = ((int64_t)(((int64_t)UINT64_C(2))));
  t61 = ((int64_t)(((int64_t)gg_mod(t59, t60))));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((t61) == (t62));
  }
  if (!(t58)) gg_fail("assertion failed");
  t63 = ((int64_t)((v5).len));
  t64 = ((int64_t)(v10));
  t65 = ((int64_t)(((*(v0))).f10));
  t66 = gg_function_2(t64, t65, base + 0);
  t67 = ((int64_t)(t66));
  if (!(((t63) >= (t67)))) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t68 = ((int64_t)UINT64_C(0));
  t69 = ((float *)(((*(v0))).f12).data) + gg_index(t68, ((int64_t)UINT64_C(8)));
  t70 = ((float)((*t69)));
  t71 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t72 = ((t70) > (t71));
  t73 = t72;
  if (t73) {
  /* FG_DIRECT_SLICE_INDEX */
  t74 = ((int64_t)UINT64_C(1));
  t75 = ((float *)(((*(v0))).f12).data) + gg_index(t74, ((int64_t)UINT64_C(8)));
  t76 = ((float)((*t75)));
  t77 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t73 = ((t76) > (t77));
  }
  if (!(t73)) gg_fail("assertion failed");
  t78 = ((int64_t)(v7));
  t79 = ((int64_t)(((int64_t)UINT64_C(24))));
  v11 = ((t78) == (t79));
  t80 = ((int64_t)(v7));
  t81 = ((int64_t)(((int64_t)UINT64_C(8))));
  t82 = ((int64_t)(((t80) & (t81))));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  v12 = ((t82) != (t83));
  t84 = ((int64_t)(v7));
  t85 = ((int64_t)(((int64_t)UINT64_C(40))));
  v13 = ((t84) == (t85));
  t86 = v11;
  if (t86) {
  {
  t87 = ((int64_t)(v6));
  t88 = ((int64_t)(v10));
  t89 = ((int64_t)(((int64_t)UINT64_C(2))));
  t90 = ((int64_t)(((int64_t)gg_div(t88, t89))));
  t91 = ((t87) == (t90));
  t92 = t91;
  if (t92) {
  t93 = ((int64_t)(v9));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((t93) == (t94));
  }
  if (!(t92)) gg_fail("assertion failed");
  }
  }
  t95 = ((int64_t)(v7));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = ((t95) == (t96));
  t98 = t97;
  if (!t98) {
  t99 = ((int64_t)(v7));
  t100 = ((int64_t)(((int64_t)UINT64_C(2))));
  t98 = ((t99) == (t100));
  }
  t101 = t98;
  t102 = t101;
  if (!t102) {
  t103 = ((int64_t)(v7));
  t104 = ((int64_t)(((int64_t)UINT64_C(8))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  t106 = t105;
  if (!t106) {
  t107 = ((int64_t)(v7));
  t108 = ((int64_t)(((int64_t)UINT64_C(24))));
  t106 = ((t107) == (t108));
  }
  t109 = t106;
  t110 = t109;
  if (!t110) {
  t111 = ((int64_t)(v7));
  t112 = ((int64_t)(((int64_t)UINT64_C(40))));
  t110 = ((t111) == (t112));
  }
  if (!(t110)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t113 = ((int64_t)UINT64_C(0));
  t114 = ((float *)(((*(v0))).f12).data) + gg_index(t113, ((int64_t)UINT64_C(8)));
  t115 = ((float)((*t114)));
  t116 = ((float)(((int64_t)UINT64_C(2))));
  t117 = ((float)((-(t116))));
  t118 = ((float)(((float)(v6))));
  t119 = ((float)(((t117) / (t118))));
  t120 = powf(t115, t119);
  v14 = ((float)(t120));
  t121 = ((int64_t)(((*(v0))).f8));
  t122 = ((int64_t)(((int64_t)UINT64_C(48))));
  t123 = ((t121) == (t122));
  if (t123) {
  {
  v15 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t124 = ((float)(((int64_t)UINT64_C(1))));
  v15 = ((float)((-(t124))));
  }
  }
  v16 = ((float)(((float)(gg_double_bits(UINT64_C(4618760256179416344))))));
  memset(&t125, 0, sizeof(t125));
  t126 = ((float)(t125));
  for (t127=0; t127<2; t127++) (v17).data[t127] = t126;
  t128 = ((float)(((float)(((int64_t)UINT64_C(2))))));
  /* FG_DIRECT_SLICE_INDEX */
  t129 = ((int64_t)UINT64_C(0));
  t130 = ((float *)(((*(v0))).f12).data) + gg_index(t129, ((int64_t)UINT64_C(8)));
  t131 = ((float)((*t130)));
  t132 = logf(t131);
  t133 = ((float)(t132));
  v18 = ((float)(((t128) * (t133))));
  /* FG_DIRECT_SLICE_INDEX */
  t134 = ((int64_t)UINT64_C(0));
  t135 = ((float *)(v17).data) + gg_index(t134, ((int64_t)UINT64_C(2)));
  t136 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t137 = ((float)(((float)(v6))));
  t138 = ((float)(((float)(v8))));
  /* FG_DIRECT_SLICE_INDEX */
  t139 = ((int64_t)UINT64_C(4));
  t140 = ((float *)(((*(v0))).f12).data) + gg_index(t139, ((int64_t)UINT64_C(8)));
  t141 = ((float)((*t140)));
  t142 = ((float)(v16));
  t143 = ((float)(((t141) * (t142))));
  t144 = ((float)(((t138) / (t143))));
  t145 = logf(t144);
  t146 = ((float)(t145));
  t147 = ((float)(((t137) * (t146))));
  t148 = ((float)(v18));
  t149 = ((float)(((t147) / (t148))));
  t150 = floorf(t149);
  t151 = ((float)(t150));
  t152 = fmaxf(t136, t151);
  (*t135) = ((float)(t152));
  /* FG_DIRECT_SLICE_INDEX */
  t153 = ((int64_t)UINT64_C(1));
  t154 = ((float *)(v17).data) + gg_index(t153, ((int64_t)UINT64_C(2)));
  t155 = ((int64_t)(v6));
  t156 = ((int64_t)(((int64_t)UINT64_C(1))));
  t157 = ((float)(((float)(((int64_t)((uint64_t)(t155) - (uint64_t)(t156)))))));
  t158 = ((float)(((float)(v6))));
  t159 = ((float)(((float)(v8))));
  /* FG_DIRECT_SLICE_INDEX */
  t160 = ((int64_t)UINT64_C(5));
  t161 = ((float *)(((*(v0))).f12).data) + gg_index(t160, ((int64_t)UINT64_C(8)));
  t162 = ((float)((*t161)));
  t163 = ((float)(v16));
  t164 = ((float)(((t162) * (t163))));
  t165 = ((float)(((t159) / (t164))));
  t166 = logf(t165);
  t167 = ((float)(t166));
  t168 = ((float)(((t158) * (t167))));
  t169 = ((float)(v18));
  t170 = ((float)(((t168) / (t169))));
  t171 = ceilf(t170);
  t172 = ((float)(t171));
  t173 = fminf(t157, t172);
  (*t154) = ((float)(t173));
  t174 = ((int64_t)(((*(v0))).f9));
  t175 = ((int64_t)(v10));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t174) * (uint64_t)(t175)))));
  t176 = v0;
  v20 = gg_function_14(t176, base + 0);
label1:;
  t177 = ((int64_t)(v20));
  t178 = &(((*(v0))).f4);
  t179 = gg_function_13(t178, base + 0);
  t180 = ((int64_t)(t179));
  if (!(((t177) < (t180)))) goto label2;
  {
  t181 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t182 = ((int64_t)UINT64_C(1));
  t183 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t182, ((int64_t)UINT64_C(4)));
  t184 = ((int64_t)((*t183)));
  t185 = ((int64_t)(((int64_t)gg_div(t181, t184))));
  /* FG_DIRECT_SLICE_INDEX */
  t186 = ((int64_t)UINT64_C(2));
  t187 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t186, ((int64_t)UINT64_C(4)));
  t188 = ((int64_t)((*t187)));
  v21 = ((int64_t)(((int64_t)gg_mod(t185, t188))));
  memset(&t189, 0, sizeof(t189));
  t190 = ((float)(t189));
  for (t191=0; t191<4; t191++) (v22).data[t191] = t190;
  memset(&t192, 0, sizeof(t192));
  t193 = ((float)(t192));
  for (t194=0; t194<4; t194++) (v23).data[t194] = t193;
  t196 = ((int64_t)UINT64_C(4));
  t195 = 0;
label3:;
  if (t195 >= t196) goto label5;
  v24 = t195;
  {
  t197 = ((int64_t)(v21));
  t199 = v12;
  if (t199) {
  {
  t200 = ((int64_t)(v24));
  /* FG_DIRECT_SLICE_INDEX */
  t201 = ((int64_t)UINT64_C(2));
  t202 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t201, ((int64_t)UINT64_C(4)));
  t203 = ((int64_t)((*t202)));
  t198 = ((int64_t)(((int64_t)((uint64_t)(t200) * (uint64_t)(t203)))));
  }
  } else {
  {
  t198 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t204 = ((int64_t)(t198));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t197) + (uint64_t)(t204)))));
  /* FG_DIRECT_SLICE_INDEX */
  t205 = v24;
  t206 = ((float *)(v22).data) + gg_index(t205, ((int64_t)UINT64_C(4)));
  t207 = v2;
  t208 = &(((*(v0))).f5);
  t209 = ((int64_t)(v25));
  t210 = gg_function_16(t208, t209, base + 0);
  t211 = gg_function_17(t207, t210, base + 0);
  (*t206) = ((float)(((float)(((int32_t)(t211))))));
  /* FG_DIRECT_SLICE_INDEX */
  t212 = v24;
  t213 = ((float *)(v23).data) + gg_index(t212, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t214 = v24;
  t215 = ((float *)(v22).data) + gg_index(t214, ((int64_t)UINT64_C(4)));
  (*t213) = ((float)((*t215)));
  }
label4:;
  t195++; goto label3;
label5:;
  /* FG_DIRECT_SLICE_INDEX */
  t216 = ((int64_t)UINT64_C(4));
  t217 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t216, ((int64_t)UINT64_C(16)));
  t218 = ((int64_t)((*t217)));
  /* FG_DIRECT_SLICE_INDEX */
  t219 = ((int64_t)UINT64_C(5));
  t220 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t219, ((int64_t)UINT64_C(16)));
  t221 = ((int64_t)((*t220)));
  t222 = ((int64_t)(((int64_t)((uint64_t)(t218) + (uint64_t)(t221)))));
  /* FG_DIRECT_SLICE_INDEX */
  t223 = ((int64_t)UINT64_C(6));
  t224 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t223, ((int64_t)UINT64_C(16)));
  t225 = ((int64_t)((*t224)));
  t226 = ((int64_t)(((int64_t)((uint64_t)(t222) + (uint64_t)(t225)))));
  /* FG_DIRECT_SLICE_INDEX */
  t227 = ((int64_t)UINT64_C(7));
  t228 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t227, ((int64_t)UINT64_C(16)));
  t229 = ((int64_t)((*t228)));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t226) + (uint64_t)(t229)))));
  t230 = v12;
  if (t230) {
  {
  t231 = ((int64_t)(v26));
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((t231) > (t232));
  t234 = t233;
  if (t234) {
  t235 = ((int64_t)(v26));
  t236 = ((int64_t)(v10));
  t234 = ((t235) <= (t236));
  }
  if (!(t234)) gg_fail("assertion failed");
  }
  }
  t238 = ((int64_t)(v10));
  t239 = ((int64_t)(((int64_t)UINT64_C(2))));
  t240 = ((int64_t)gg_div(t238, t239));
  t237 = 0;
label6:;
  if (t237 >= t240) goto label8;
  v27 = t237;
  {
  t241 = ((int64_t)(((int64_t)UINT64_C(2))));
  t242 = ((int64_t)(v27));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t241) * (uint64_t)(t242)))));
  /* FG_DIRECT_SLICE_INDEX */
  t243 = ((int64_t)UINT64_C(0));
  t244 = ((float *)(v23).data) + gg_index(t243, ((int64_t)UINT64_C(4)));
  v29 = ((float)((*t244)));
  t245 = v12;
  if (t245) {
  {
  t246 = ((int64_t)(v27));
  t247 = ((int64_t)(v26));
  v30 = ((int64_t)(((int64_t)gg_mod(t246, t247))));
  /* FG_DIRECT_SLICE_INDEX */
  t248 = ((int64_t)UINT64_C(4));
  t249 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t248, ((int64_t)UINT64_C(16)));
  t250 = ((int64_t)((*t249)));
  /* FG_DIRECT_SLICE_INDEX */
  t251 = ((int64_t)UINT64_C(5));
  t252 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t251, ((int64_t)UINT64_C(16)));
  t253 = ((int64_t)((*t252)));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t250) + (uint64_t)(t253)))));
  t254 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t255 = ((int64_t)UINT64_C(6));
  t256 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t255, ((int64_t)UINT64_C(16)));
  t257 = ((int64_t)((*t256)));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t254) + (uint64_t)(t257)))));
  t258 = v11;
  if (t258) {
  {
  t259 = ((int64_t)(v30));
  t260 = ((int64_t)(((int64_t)UINT64_C(0))));
  t261 = ((t259) == (t260));
  if (t261) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t262 = ((int64_t)UINT64_C(0));
  t263 = ((float *)(v23).data) + gg_index(t262, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t264 = ((int64_t)UINT64_C(0));
  t265 = ((float *)(v22).data) + gg_index(t264, ((int64_t)UINT64_C(4)));
  (*t263) = ((float)((*t265)));
  }
  } else {
  t266 = ((int64_t)(v30));
  /* FG_DIRECT_SLICE_INDEX */
  t267 = ((int64_t)UINT64_C(4));
  t268 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t267, ((int64_t)UINT64_C(16)));
  t269 = ((int64_t)((*t268)));
  t270 = ((t266) == (t269));
  if (t270) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t271 = ((int64_t)UINT64_C(1));
  t272 = ((float *)(v23).data) + gg_index(t271, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t273 = ((int64_t)UINT64_C(1));
  t274 = ((float *)(v22).data) + gg_index(t273, ((int64_t)UINT64_C(4)));
  (*t272) = ((float)((*t274)));
  }
  } else {
  t275 = ((int64_t)(v30));
  t276 = ((int64_t)(v31));
  t277 = ((t275) == (t276));
  if (t277) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t278 = ((int64_t)UINT64_C(2));
  t279 = ((float *)(v23).data) + gg_index(t278, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t280 = ((int64_t)UINT64_C(2));
  t281 = ((float *)(v22).data) + gg_index(t280, ((int64_t)UINT64_C(4)));
  (*t279) = ((float)((*t281)));
  }
  } else {
  t282 = ((int64_t)(v30));
  t283 = ((int64_t)(v32));
  t284 = ((t282) == (t283));
  if (t284) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t285 = ((int64_t)UINT64_C(3));
  t286 = ((float *)(v23).data) + gg_index(t285, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t287 = ((int64_t)UINT64_C(3));
  t288 = ((float *)(v22).data) + gg_index(t287, ((int64_t)UINT64_C(4)));
  (*t286) = ((float)((*t288)));
  }
  }
  }
  }
  }
  }
  }
  t289 = v13;
  if (t289) {
  {
  t290 = ((int64_t)(v30));
  t291 = ((int64_t)(((int64_t)UINT64_C(3))));
  t292 = ((int64_t)(((int64_t)gg_mod(t290, t291))));
  t293 = ((int64_t)(((int64_t)UINT64_C(1))));
  t294 = ((t292) == (t293));
  t295 = t294;
  if (t295) {
  t296 = ((int64_t)(v30));
  t297 = ((int64_t)(((int64_t)UINT64_C(3))));
  /* FG_DIRECT_SLICE_INDEX */
  t298 = ((int64_t)UINT64_C(5));
  t299 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t298, ((int64_t)UINT64_C(16)));
  t300 = ((int64_t)((*t299)));
  t301 = ((int64_t)(((int64_t)((uint64_t)(t297) * (uint64_t)(t300)))));
  t295 = ((t296) < (t301));
  }
  t302 = t295;
  if (t302) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t303 = ((int64_t)UINT64_C(1));
  t304 = ((float *)(v23).data) + gg_index(t303, ((int64_t)UINT64_C(4)));
  v29 = ((float)((*t304)));
  }
  } else {
  t305 = ((int64_t)(v30));
  t306 = ((int64_t)(((int64_t)UINT64_C(3))));
  t307 = ((int64_t)(((int64_t)gg_mod(t305, t306))));
  t308 = ((int64_t)(((int64_t)UINT64_C(2))));
  t309 = ((t307) == (t308));
  t310 = t309;
  if (t310) {
  t311 = ((int64_t)(v30));
  t312 = ((int64_t)(((int64_t)UINT64_C(3))));
  /* FG_DIRECT_SLICE_INDEX */
  t313 = ((int64_t)UINT64_C(6));
  t314 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t313, ((int64_t)UINT64_C(16)));
  t315 = ((int64_t)((*t314)));
  t316 = ((int64_t)(((int64_t)((uint64_t)(t312) * (uint64_t)(t315)))));
  t310 = ((t311) < (t316));
  }
  t317 = t310;
  if (t317) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t318 = ((int64_t)UINT64_C(2));
  t319 = ((float *)(v23).data) + gg_index(t318, ((int64_t)UINT64_C(4)));
  v29 = ((float)((*t319)));
  }
  } else {
  t320 = ((int64_t)(v30));
  t321 = ((int64_t)(((int64_t)UINT64_C(3))));
  t322 = ((int64_t)(((int64_t)gg_mod(t320, t321))));
  t323 = ((int64_t)(((int64_t)UINT64_C(0))));
  t324 = ((t322) == (t323));
  t325 = t324;
  if (t325) {
  t326 = ((int64_t)(v30));
  t327 = ((int64_t)(((int64_t)UINT64_C(3))));
  /* FG_DIRECT_SLICE_INDEX */
  t328 = ((int64_t)UINT64_C(4));
  t329 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t328, ((int64_t)UINT64_C(16)));
  t330 = ((int64_t)((*t329)));
  t331 = ((int64_t)(((int64_t)((uint64_t)(t327) * (uint64_t)(t330)))));
  t325 = ((t326) < (t331));
  }
  t332 = t325;
  if (t332) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t333 = ((int64_t)UINT64_C(0));
  t334 = ((float *)(v23).data) + gg_index(t333, ((int64_t)UINT64_C(4)));
  v29 = ((float)((*t334)));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t335 = ((int64_t)UINT64_C(3));
  t336 = ((float *)(v23).data) + gg_index(t335, ((int64_t)UINT64_C(4)));
  v29 = ((float)((*t336)));
  }
  }
  }
  }
  }
  } else {
  {
  t337 = ((int64_t)(v30));
  /* FG_DIRECT_SLICE_INDEX */
  t338 = ((int64_t)UINT64_C(4));
  t339 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t338, ((int64_t)UINT64_C(16)));
  t340 = ((int64_t)((*t339)));
  t341 = ((t337) < (t340));
  if (t341) {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  t342 = ((int64_t)(v30));
  t343 = ((int64_t)(v31));
  t344 = ((t342) < (t343));
  if (t344) {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  t345 = ((int64_t)(v30));
  t346 = ((int64_t)(v32));
  t347 = ((t345) < (t346));
  if (t347) {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  }
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t348 = v33;
  t349 = ((float *)(v23).data) + gg_index(t348, ((int64_t)UINT64_C(4)));
  v29 = ((float)((*t349)));
  }
  }
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t350 = ((int64_t)UINT64_C(8));
  t351 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t350, ((int64_t)UINT64_C(16)));
  t352 = ((int64_t)((*t351)));
  t353 = ((int64_t)(((int64_t)UINT64_C(0))));
  t354 = ((t352) != (t353));
  if (t354) {
  {
  t355 = &(((*(v0))).f6);
  t356 = v3;
  t357 = ((int64_t)(v27));
  v34 = gg_function_51(t355, t356, t357, base + 0);
  }
  } else {
  {
  v34 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  }
  }
  t358 = ((float)(v29));
  t359 = ((float)(v34));
  t360 = ((float)(((t358) / (t359))));
  /* FG_DIRECT_SLICE_INDEX */
  t361 = ((int64_t)UINT64_C(1));
  t362 = ((float *)(((*(v0))).f12).data) + gg_index(t361, ((int64_t)UINT64_C(8)));
  t363 = ((float)((*t362)));
  t364 = v17;
  t365 = ((int64_t)(v28));
  /* FG_DIRECT_SLICE_INDEX */
  t366 = ((int64_t)UINT64_C(2));
  t367 = ((float *)(((*(v0))).f12).data) + gg_index(t366, ((int64_t)UINT64_C(8)));
  t368 = ((float)((*t367)));
  /* FG_DIRECT_SLICE_INDEX */
  t369 = ((int64_t)UINT64_C(3));
  t370 = ((float *)(((*(v0))).f12).data) + gg_index(t369, ((int64_t)UINT64_C(8)));
  t371 = ((float)((*t370)));
  v35 = gg_function_64(t360, t363, t364, t365, t368, t371, base + 0);
  t372 = v5;
  t373 = ((int64_t)(v19));
  t374 = ((int64_t)(v28));
  t375 = ((int64_t)((uint64_t)(t373) + (uint64_t)(t374)));
  t376 = ((float *)t372.data) + gg_index(t375, t372.len);
  /* FG_DIRECT_SLICE_INDEX */
  t377 = ((int64_t)UINT64_C(0));
  t378 = ((float *)(v35).data) + gg_index(t377, ((int64_t)UINT64_C(2)));
  (*t376) = ((float)((*t378)));
  t379 = v5;
  t380 = ((int64_t)(v19));
  t381 = ((int64_t)(v28));
  t382 = ((int64_t)(((int64_t)((uint64_t)(t380) + (uint64_t)(t381)))));
  t383 = ((int64_t)(((int64_t)UINT64_C(1))));
  t384 = ((int64_t)((uint64_t)(t382) + (uint64_t)(t383)));
  t385 = ((float *)t379.data) + gg_index(t384, t379.len);
  /* FG_DIRECT_SLICE_INDEX */
  t386 = ((int64_t)UINT64_C(1));
  t387 = ((float *)(v35).data) + gg_index(t386, ((int64_t)UINT64_C(2)));
  t388 = ((float)((*t387)));
  t389 = ((float)(v15));
  (*t385) = ((float)(((t388) * (t389))));
  t391 = ((int64_t)UINT64_C(4));
  t390 = 0;
label9:;
  if (t390 >= t391) goto label11;
  v36 = t390;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t392 = v36;
  t393 = ((float *)(v23).data) + gg_index(t392, ((int64_t)UINT64_C(4)));
  t394 = (*t393);
  t395 = ((float)(v14));
  (*t393) = ((t394) * (t395));
  }
label10:;
  t390++; goto label9;
label11:;
  }
label7:;
  t237++; goto label6;
label8:;
  t396 = v11;
  if (t396) {
  {
  v37 = ((int64_t)(v10));
  }
  } else {
  {
  v37 = ((int64_t)(v6));
  }
  }
  t397 = ((int64_t)(v7));
  t398 = ((int64_t)(((int64_t)UINT64_C(0))));
  t399 = ((t397) == (t398));
  if (t399) {
  {
  v38 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  t400 = v11;
  if (t400) {
  {
  v38 = ((int64_t)(v6));
  }
  } else {
  {
  t401 = ((int64_t)(v6));
  t402 = ((int64_t)(((int64_t)UINT64_C(2))));
  v38 = ((int64_t)(((int64_t)gg_div(t401, t402))));
  }
  }
  }
  t403 = ((int64_t)(v7));
  t404 = ((int64_t)(((int64_t)UINT64_C(0))));
  t405 = ((t403) == (t404));
  if (t405) {
  {
  v39 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v39 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  }
  t407 = ((int64_t)(v37));
  t408 = ((int64_t)(((int64_t)UINT64_C(2))));
  t409 = ((int64_t)gg_div(t407, t408));
  t406 = 0;
label12:;
  if (t406 >= t409) goto label14;
  v40 = t406;
  {
  t410 = ((int64_t)(((int64_t)UINT64_C(2))));
  t411 = ((int64_t)(v40));
  v41 = ((int64_t)(((int64_t)((uint64_t)(t410) * (uint64_t)(t411)))));
  t412 = ((int64_t)(v9));
  t413 = ((int64_t)(v41));
  t414 = ((int64_t)(v39));
  t415 = ((int64_t)(((int64_t)gg_div(t413, t414))));
  v42 = ((int64_t)(((int64_t)((uint64_t)(t412) + (uint64_t)(t415)))));
  t416 = &(((*(v0))).f4);
  t417 = v1;
  t418 = ((int64_t)(v20));
  t419 = ((int64_t)(v10));
  t420 = ((int64_t)(((int64_t)((uint64_t)(t418) * (uint64_t)(t419)))));
  t421 = ((int64_t)(v42));
  t422 = ((int64_t)(((int64_t)((uint64_t)(t420) + (uint64_t)(t421)))));
  v43 = gg_function_51(t416, t417, t422, base + 0);
  t423 = &(((*(v0))).f4);
  t424 = v1;
  t425 = ((int64_t)(v20));
  t426 = ((int64_t)(v10));
  t427 = ((int64_t)(((int64_t)((uint64_t)(t425) * (uint64_t)(t426)))));
  t428 = ((int64_t)(v42));
  t429 = ((int64_t)(((int64_t)((uint64_t)(t427) + (uint64_t)(t428)))));
  t430 = ((int64_t)(v38));
  t431 = ((int64_t)(((int64_t)((uint64_t)(t429) + (uint64_t)(t430)))));
  v44 = gg_function_51(t423, t424, t431, base + 0);
  t432 = v5;
  t433 = ((int64_t)(v19));
  t434 = ((int64_t)(v41));
  t435 = ((int64_t)((uint64_t)(t433) + (uint64_t)(t434)));
  t436 = ((float *)t432.data) + gg_index(t435, t432.len);
  v45 = ((float)((*t436)));
  t437 = v5;
  t438 = ((int64_t)(v19));
  t439 = ((int64_t)(v41));
  t440 = ((int64_t)(((int64_t)((uint64_t)(t438) + (uint64_t)(t439)))));
  t441 = ((int64_t)(((int64_t)UINT64_C(1))));
  t442 = ((int64_t)((uint64_t)(t440) + (uint64_t)(t441)));
  t443 = ((float *)t437.data) + gg_index(t442, t437.len);
  v46 = ((float)((*t443)));
  t444 = &(((*(v0))).f7);
  t445 = v4;
  t446 = ((int64_t)(v20));
  t447 = ((int64_t)(v10));
  t448 = ((int64_t)(((int64_t)((uint64_t)(t446) * (uint64_t)(t447)))));
  t449 = ((int64_t)(v42));
  t450 = ((int64_t)(((int64_t)((uint64_t)(t448) + (uint64_t)(t449)))));
  t451 = ((float)(v43));
  t452 = ((float)(v45));
  t453 = ((float)(((t451) * (t452))));
  t454 = ((float)(v44));
  t455 = ((float)(v46));
  t456 = ((float)(((t454) * (t455))));
  t457 = ((float)(((t453) - (t456))));
  gg_function_37(t444, t445, t450, t457, base + 0);
  t458 = &(((*(v0))).f7);
  t459 = v4;
  t460 = ((int64_t)(v20));
  t461 = ((int64_t)(v10));
  t462 = ((int64_t)(((int64_t)((uint64_t)(t460) * (uint64_t)(t461)))));
  t463 = ((int64_t)(v42));
  t464 = ((int64_t)(((int64_t)((uint64_t)(t462) + (uint64_t)(t463)))));
  t465 = ((int64_t)(v38));
  t466 = ((int64_t)(((int64_t)((uint64_t)(t464) + (uint64_t)(t465)))));
  t467 = ((float)(v43));
  t468 = ((float)(v46));
  t469 = ((float)(((t467) * (t468))));
  t470 = ((float)(v44));
  t471 = ((float)(v45));
  t472 = ((float)(((t470) * (t471))));
  t473 = ((float)(((t469) + (t472))));
  gg_function_37(t458, t459, t466, t473, base + 0);
  }
label13:;
  t406++; goto label12;
label14:;
  t474 = v11;
  t475 = (!(t474));
  if (t475) {
  {
  t476 = ((int64_t)((((*(v0))).f4).f2));
  v47 = gg_function_4(t476, base + 0);
  t478 = v10;
  t477 = 0;
label15:;
  if (t477 >= t478) goto label17;
  v48 = t477;
  {
  t479 = ((int64_t)(v48));
  t480 = ((int64_t)(v9));
  t481 = ((t479) < (t480));
  t482 = t481;
  if (!t482) {
  t483 = ((int64_t)(v48));
  t484 = ((int64_t)(v9));
  t485 = ((int64_t)(v6));
  t486 = ((int64_t)(((int64_t)((uint64_t)(t484) + (uint64_t)(t485)))));
  t482 = ((t483) >= (t486));
  }
  t487 = t482;
  if (t487) {
  {
  t488 = &(((*(v0))).f4);
  t489 = ((int64_t)(v20));
  t490 = ((int64_t)(v10));
  t491 = ((int64_t)(((int64_t)((uint64_t)(t489) * (uint64_t)(t490)))));
  t492 = ((int64_t)(v48));
  t493 = ((int64_t)(((int64_t)((uint64_t)(t491) + (uint64_t)(t492)))));
  v49 = gg_function_16(t488, t493, base + 0);
  t494 = &(((*(v0))).f7);
  t495 = ((int64_t)(v20));
  t496 = ((int64_t)(v10));
  t497 = ((int64_t)(((int64_t)((uint64_t)(t495) * (uint64_t)(t496)))));
  t498 = ((int64_t)(v48));
  t499 = ((int64_t)(((int64_t)((uint64_t)(t497) + (uint64_t)(t498)))));
  v50 = gg_function_16(t494, t499, base + 0);
  t501 = v47;
  t500 = 0;
label18:;
  if (t500 >= t501) goto label20;
  v51 = t500;
  {
  t502 = v4;
  t503 = ((int64_t)(v50));
  t504 = ((int64_t)(v51));
  t505 = ((int64_t)((uint64_t)(t503) + (uint64_t)(t504)));
  t506 = ((uint8_t *)t502.data) + gg_index(t505, t502.len);
  t507 = v1;
  t508 = ((int64_t)(v49));
  t509 = ((int64_t)(v51));
  t510 = ((int64_t)((uint64_t)(t508) + (uint64_t)(t509)));
  t511 = ((uint8_t *)t507.data) + gg_index(t510, t507.len);
  (*t506) = ((uint8_t)((*t511)));
  }
label19:;
  t500++; goto label18;
label20:;
  }
  }
  }
label16:;
  t477++; goto label15;
label17:;
  }
  }
  t512 = v20;
  t513 = ((int64_t)(((*(v0))).f10));
  v20 = ((int64_t)((uint64_t)(t512) + (uint64_t)(t513)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_55(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  gg_type_20 t20;
  gg_type_31 t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t * t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t * t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t * t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t * t38;
  int64_t t39;
  int64_t t40;
  int64_t * t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t * t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t * t50;
  int64_t t51;
  int64_t t52;
  int64_t * t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t * t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t * t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t * t68;
  uint8_t t69;
  int64_t t70;
  int64_t * t71;
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
  int64_t * t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t * t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t * t94;
  int64_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  int64_t * t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t * t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t * t111;
  int64_t t112;
  uint8_t t113;
  int64_t t114;
  int64_t * t115;
  uint8_t t116;
  int64_t t117;
  int64_t t118;
  uint8_t t119;
  uint8_t t120;
  int64_t t121;
  int64_t t122;
  int64_t * t123;
  int64_t t124;
  uint8_t t125;
  uint8_t t126;
  int64_t t127;
  int64_t t128;
  uint8_t t129;
  uint8_t t130;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  int64_t t134;
  int64_t t135;
  int64_t * t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  int64_t * t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t * t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t * t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t * t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t * t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  gg_type_31 t168;
  gg_slice t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  uint8_t t173;
  uint8_t t174;
  int64_t t175;
  int64_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t * t181;
  uint8_t t182;
  int64_t t183;
  int64_t * t184;
  uint8_t t185;
  int64_t t186;
  int64_t * t187;
  int64_t t188;
  int64_t * t189;
  uint8_t t190;
  int64_t t191;
  int64_t * t192;
  uint8_t t193;
  int64_t t194;
  int64_t * t195;
  int64_t t196;
  int64_t * t197;
  uint8_t t198;
  int64_t t199;
  int64_t * t200;
  int64_t t201;
  int64_t t202;
  uint8_t t203;
  uint8_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t * t208;
  int64_t t209;
  int64_t t210;
  uint8_t t211;
  uint32_t t212;
  float t213;
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
  uint8_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  uint8_t t233;
  uint8_t t234;
  int64_t t235;
  int64_t t236;
  int64_t * t237;
  int64_t t238;
  uint8_t t239;
  uint8_t t240;
  int64_t t241;
  int64_t t242;
  uint8_t t243;
  uint8_t t244;
  int64_t t245;
  int64_t t246;
  int64_t * t247;
  int64_t t248;
  uint8_t t249;
  gg_type_31 t250;
  gg_slice t251;
  int64_t t252;
  int64_t t253;
  int64_t * t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t * t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t * t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t * t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  uint8_t t275;
  float t276;
  float t277;
  float t278;
  float t279;
  float t280;
  uint8_t t281;
  float t282;
  int64_t t283;
  int64_t t284;
  float t285;
  int64_t t286;
  int64_t t287;
  uint8_t t288;
  int64_t t289;
  int64_t * t290;
  int64_t t291;
  int64_t * t292;
  int64_t t293;
  int64_t t294;
  uint8_t t295;
  uint8_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
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
  int64_t * t313;
  int64_t t314;
  uint8_t t315;
  uint8_t t316;
  int64_t t317;
  int64_t t318;
  int64_t * t319;
  int64_t t320;
  uint8_t t321;
  gg_type_31 t322;
  gg_slice t323;
  int64_t t324;
  int64_t t325;
  int64_t * t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  int64_t * t331;
  int64_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  int64_t t336;
  int64_t * t337;
  int64_t t338;
  int64_t t339;
  int64_t t340;
  int64_t t341;
  int64_t * t342;
  int64_t t343;
  int64_t * t344;
  int64_t t345;
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
  int64_t t357;
  int64_t t358;
  gg_type_31 t359;
  gg_slice t360;
  int64_t t361;
  int64_t t362;
  int64_t * t363;
  int64_t t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  int64_t * t370;
  int64_t t371;
  int64_t t372;
  int64_t t373;
  int64_t t374;
  int64_t t375;
  int64_t t376;
  int64_t t377;
  int64_t * t378;
  int64_t t379;
  int64_t t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  int64_t * t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  gg_type_31 t388;
  gg_slice t389;
  int64_t t390;
  float t391;
  int64_t t392;
  int64_t t393;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  float v10;
  uint8_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  uint8_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  uint8_t v28;
  int64_t v29;
  int64_t v30;
  int64_t v31;
  int64_t v32;
  float v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
  int64_t v42;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(52))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f8));
  t6 = ((int64_t)(((int64_t)UINT64_C(60))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f8));
  t10 = ((int64_t)(((int64_t)UINT64_C(61))));
  t8 = ((t9) != (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(((*(v0))).f8));
  t14 = ((int64_t)(((int64_t)UINT64_C(78))));
  t12 = ((t13) != (t14));
  }
  t15 = t12;
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(((*(v0))).f8));
  t18 = ((int64_t)(((int64_t)UINT64_C(79))));
  t16 = ((t17) != (t18));
  }
  t19 = t16;
  if (t19) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t20 = v0;
  v4 = gg_function_14(t20, base + 0);
  t21 = &(((*(v0))).f7);
  v5 = gg_function_12(t21, base + 0);
label1:;
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(v5));
  if (!(((t22) < (t23)))) goto label2;
  {
  t24 = ((int64_t)(v4));
  /* FG_DIRECT_SLICE_INDEX */
  t25 = ((int64_t)UINT64_C(0));
  t26 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t25, ((int64_t)UINT64_C(4)));
  t27 = ((int64_t)((*t26)));
  v6 = ((int64_t)(((int64_t)gg_mod(t24, t27))));
  t28 = ((int64_t)(v4));
  /* FG_DIRECT_SLICE_INDEX */
  t29 = ((int64_t)UINT64_C(0));
  t30 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t29, ((int64_t)UINT64_C(4)));
  t31 = ((int64_t)((*t30)));
  t32 = ((int64_t)(((int64_t)gg_div(t28, t31))));
  /* FG_DIRECT_SLICE_INDEX */
  t33 = ((int64_t)UINT64_C(1));
  t34 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t33, ((int64_t)UINT64_C(4)));
  t35 = ((int64_t)((*t34)));
  v7 = ((int64_t)(((int64_t)gg_mod(t32, t35))));
  t36 = ((int64_t)(v4));
  /* FG_DIRECT_SLICE_INDEX */
  t37 = ((int64_t)UINT64_C(0));
  t38 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t37, ((int64_t)UINT64_C(4)));
  t39 = ((int64_t)((*t38)));
  /* FG_DIRECT_SLICE_INDEX */
  t40 = ((int64_t)UINT64_C(1));
  t41 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t40, ((int64_t)UINT64_C(4)));
  t42 = ((int64_t)((*t41)));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t42)))));
  t44 = ((int64_t)(((int64_t)gg_div(t36, t43))));
  /* FG_DIRECT_SLICE_INDEX */
  t45 = ((int64_t)UINT64_C(2));
  t46 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t45, ((int64_t)UINT64_C(4)));
  t47 = ((int64_t)((*t46)));
  v8 = ((int64_t)(((int64_t)gg_mod(t44, t47))));
  t48 = ((int64_t)(v4));
  /* FG_DIRECT_SLICE_INDEX */
  t49 = ((int64_t)UINT64_C(0));
  t50 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t49, ((int64_t)UINT64_C(4)));
  t51 = ((int64_t)((*t50)));
  /* FG_DIRECT_SLICE_INDEX */
  t52 = ((int64_t)UINT64_C(1));
  t53 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t52, ((int64_t)UINT64_C(4)));
  t54 = ((int64_t)((*t53)));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t54)))));
  /* FG_DIRECT_SLICE_INDEX */
  t56 = ((int64_t)UINT64_C(2));
  t57 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t56, ((int64_t)UINT64_C(4)));
  t58 = ((int64_t)((*t57)));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t58)))));
  v9 = ((int64_t)(((int64_t)gg_div(t48, t59))));
  v10 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t60 = ((int64_t)(((*(v0))).f8));
  t61 = ((int64_t)(((int64_t)UINT64_C(52))));
  t62 = ((t60) == (t61));
  if (t62) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t63 = ((int64_t)UINT64_C(6));
  t64 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t63, ((int64_t)UINT64_C(16)));
  t65 = ((int64_t)((*t64)));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = ((t65) != (t66));
  /* FG_DIRECT_SLICE_INDEX */
  t67 = ((int64_t)UINT64_C(0));
  t68 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t67, ((int64_t)UINT64_C(4)));
  v12 = ((int64_t)((*t68)));
  t69 = v11;
  if (t69) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t70 = ((int64_t)UINT64_C(1));
  t71 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t70, ((int64_t)UINT64_C(4)));
  v13 = ((int64_t)((*t71)));
  }
  } else {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t72 = ((int64_t)(v6));
  t73 = ((int64_t)(v12));
  t74 = ((int64_t)(v13));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))));
  v14 = ((int64_t)(((int64_t)gg_div(t72, t75))));
  t76 = ((int64_t)(v6));
  t77 = ((int64_t)(v12));
  t78 = ((int64_t)(v13));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t78)))));
  v15 = ((int64_t)(((int64_t)gg_mod(t76, t79))));
  t80 = ((int64_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t81 = ((int64_t)UINT64_C(0));
  t82 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t81, ((int64_t)UINT64_C(16)));
  t83 = ((int64_t)((*t82)));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t83)))));
  t85 = ((int64_t)(v15));
  t86 = ((int64_t)(v12));
  t87 = ((int64_t)(((int64_t)gg_mod(t85, t86))));
  /* FG_DIRECT_SLICE_INDEX */
  t88 = ((int64_t)UINT64_C(4));
  t89 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t88, ((int64_t)UINT64_C(16)));
  t90 = ((int64_t)((*t89)));
  t91 = ((int64_t)(((int64_t)((uint64_t)(t87) * (uint64_t)(t90)))));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t91)))));
  /* FG_DIRECT_SLICE_INDEX */
  t93 = ((int64_t)UINT64_C(2));
  t94 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t93, ((int64_t)UINT64_C(16)));
  t95 = ((int64_t)((*t94)));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t92) - (uint64_t)(t95)))));
  t96 = v11;
  if (t96) {
  {
  t97 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t98 = ((int64_t)UINT64_C(1));
  t99 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t98, ((int64_t)UINT64_C(16)));
  t100 = ((int64_t)((*t99)));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t97) * (uint64_t)(t100)))));
  t102 = ((int64_t)(v15));
  t103 = ((int64_t)(v12));
  t104 = ((int64_t)(((int64_t)gg_div(t102, t103))));
  /* FG_DIRECT_SLICE_INDEX */
  t105 = ((int64_t)UINT64_C(5));
  t106 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t105, ((int64_t)UINT64_C(16)));
  t107 = ((int64_t)((*t106)));
  t108 = ((int64_t)(((int64_t)((uint64_t)(t104) * (uint64_t)(t107)))));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t101) + (uint64_t)(t108)))));
  /* FG_DIRECT_SLICE_INDEX */
  t110 = ((int64_t)UINT64_C(3));
  t111 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t110, ((int64_t)UINT64_C(16)));
  t112 = ((int64_t)((*t111)));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t109) - (uint64_t)(t112)))));
  }
  } else {
  {
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t113 = v11;
  if (t113) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t114 = ((int64_t)UINT64_C(1));
  t115 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t114, ((int64_t)UINT64_C(4)));
  v18 = ((int64_t)((*t115)));
  }
  } else {
  {
  v18 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t116 = v11;
  if (t116) {
  {
  v19 = ((int64_t)(v9));
  }
  } else {
  {
  v19 = ((int64_t)(v8));
  }
  }
  t117 = ((int64_t)(v16));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((t117) >= (t118));
  t120 = t119;
  if (t120) {
  t121 = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t122 = ((int64_t)UINT64_C(0));
  t123 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t122, ((int64_t)UINT64_C(4)));
  t124 = ((int64_t)((*t123)));
  t120 = ((t121) < (t124));
  }
  t125 = t120;
  t126 = t125;
  if (t126) {
  t127 = ((int64_t)(v17));
  t128 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((t127) >= (t128));
  }
  t129 = t126;
  t130 = t129;
  if (t130) {
  t131 = ((int64_t)(v17));
  t132 = ((int64_t)(v18));
  t130 = ((t131) < (t132));
  }
  t133 = t130;
  if (t133) {
  {
  t134 = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t135 = ((int64_t)UINT64_C(0));
  t136 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t135, ((int64_t)UINT64_C(4)));
  t137 = ((int64_t)((*t136)));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t134) * (uint64_t)(t137)))));
  t140 = v11;
  if (t140) {
  {
  t141 = ((int64_t)(v17));
  /* FG_DIRECT_SLICE_INDEX */
  t142 = ((int64_t)UINT64_C(1));
  t143 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t142, ((int64_t)UINT64_C(4)));
  t144 = ((int64_t)((*t143)));
  t145 = ((int64_t)(((int64_t)((uint64_t)(t141) * (uint64_t)(t144)))));
  t146 = ((int64_t)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t147 = ((int64_t)UINT64_C(2));
  t148 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t147, ((int64_t)UINT64_C(4)));
  t149 = ((int64_t)((*t148)));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t146) * (uint64_t)(t149)))));
  t151 = ((int64_t)(((int64_t)((uint64_t)(t145) + (uint64_t)(t150)))));
  t152 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t153 = ((int64_t)UINT64_C(3));
  t154 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t153, ((int64_t)UINT64_C(4)));
  t155 = ((int64_t)((*t154)));
  t156 = ((int64_t)(((int64_t)((uint64_t)(t152) * (uint64_t)(t155)))));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t151) + (uint64_t)(t156)))));
  }
  } else {
  {
  t157 = ((int64_t)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t158 = ((int64_t)UINT64_C(1));
  t159 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t158, ((int64_t)UINT64_C(4)));
  t160 = ((int64_t)((*t159)));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t157) * (uint64_t)(t160)))));
  t162 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t163 = ((int64_t)UINT64_C(2));
  t164 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t163, ((int64_t)UINT64_C(4)));
  t165 = ((int64_t)((*t164)));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t162) * (uint64_t)(t165)))));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t161) + (uint64_t)(t166)))));
  }
  }
  t167 = ((int64_t)(t139));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t138) + (uint64_t)(t167)))));
  t168 = &(((*(v0))).f5);
  t169 = v2;
  t170 = ((int64_t)(v20));
  v10 = gg_function_47(t168, t169, t170, base + 0);
  }
  }
  }
  } else {
  t171 = ((int64_t)(((*(v0))).f8));
  t172 = ((int64_t)(((int64_t)UINT64_C(60))));
  t173 = ((t171) == (t172));
  t174 = t173;
  if (!t174) {
  t175 = ((int64_t)(((*(v0))).f8));
  t176 = ((int64_t)(((int64_t)UINT64_C(61))));
  t174 = ((t175) == (t176));
  }
  t177 = t174;
  if (t177) {
  {
  t178 = ((int64_t)(((*(v0))).f8));
  t179 = ((int64_t)(((int64_t)UINT64_C(61))));
  v21 = ((t178) == (t179));
  /* FG_DIRECT_SLICE_INDEX */
  t180 = ((int64_t)UINT64_C(1));
  t181 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t180, ((int64_t)UINT64_C(16)));
  v22 = ((int64_t)((*t181)));
  t182 = v21;
  if (t182) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t183 = ((int64_t)UINT64_C(2));
  t184 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t183, ((int64_t)UINT64_C(16)));
  v23 = ((int64_t)((*t184)));
  }
  } else {
  {
  v23 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t185 = v21;
  if (t185) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t186 = ((int64_t)UINT64_C(3));
  t187 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t186, ((int64_t)UINT64_C(16)));
  v24 = ((int64_t)((*t187)));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t188 = ((int64_t)UINT64_C(2));
  t189 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t188, ((int64_t)UINT64_C(16)));
  v24 = ((int64_t)((*t189)));
  }
  }
  t190 = v21;
  if (t190) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t191 = ((int64_t)UINT64_C(4));
  t192 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t191, ((int64_t)UINT64_C(16)));
  v25 = ((int64_t)((*t192)));
  }
  } else {
  {
  v25 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t193 = v21;
  if (t193) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t194 = ((int64_t)UINT64_C(5));
  t195 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t194, ((int64_t)UINT64_C(16)));
  v26 = ((int64_t)((*t195)));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t196 = ((int64_t)UINT64_C(3));
  t197 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t196, ((int64_t)UINT64_C(16)));
  v26 = ((int64_t)((*t197)));
  }
  }
  t198 = v21;
  if (t198) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t199 = ((int64_t)UINT64_C(6));
  t200 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t199, ((int64_t)UINT64_C(16)));
  v27 = ((int64_t)((*t200)));
  }
  } else {
  {
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t201 = ((int64_t)(v22));
  t202 = ((int64_t)(((int64_t)UINT64_C(0))));
  t203 = ((t201) > (t202));
  t204 = t203;
  if (t204) {
  t205 = ((int64_t)(v23));
  t206 = ((int64_t)(((int64_t)UINT64_C(0))));
  t204 = ((t205) > (t206));
  }
  if (!(t204)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t207 = ((int64_t)UINT64_C(0));
  t208 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t207, ((int64_t)UINT64_C(16)));
  t209 = ((int64_t)((*t208)));
  t210 = ((int64_t)(((int64_t)UINT64_C(0))));
  v28 = ((t209) == (t210));
  t211 = v28;
  if (t211) {
  {
  v10 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t212 = ((uint32_t)(((int64_t)UINT64_C(4286578687))));
  t213 = fg143_from_bits(t212);
  v10 = ((float)(t213));
  }
  }
  t215 = v23;
  t214 = 0;
label3:;
  if (t214 >= t215) goto label5;
  v29 = t214;
  {
  t217 = v22;
  t216 = 0;
label6:;
  if (t216 >= t217) goto label8;
  v30 = t216;
  {
  t218 = ((int64_t)(v6));
  t219 = ((int64_t)(v24));
  t220 = ((int64_t)(((int64_t)((uint64_t)(t218) * (uint64_t)(t219)))));
  t221 = ((int64_t)(v26));
  t222 = ((int64_t)(((int64_t)((uint64_t)(t220) - (uint64_t)(t221)))));
  t223 = ((int64_t)(v30));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t222) + (uint64_t)(t223)))));
  t224 = v21;
  if (t224) {
  {
  t225 = ((int64_t)(v7));
  t226 = ((int64_t)(v25));
  t227 = ((int64_t)(((int64_t)((uint64_t)(t225) * (uint64_t)(t226)))));
  t228 = ((int64_t)(v27));
  t229 = ((int64_t)(((int64_t)((uint64_t)(t227) - (uint64_t)(t228)))));
  t230 = ((int64_t)(v29));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t229) + (uint64_t)(t230)))));
  }
  } else {
  {
  v32 = ((int64_t)(v7));
  }
  }
  t231 = ((int64_t)(v31));
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((t231) >= (t232));
  t234 = t233;
  if (t234) {
  t235 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t236 = ((int64_t)UINT64_C(0));
  t237 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t236, ((int64_t)UINT64_C(4)));
  t238 = ((int64_t)((*t237)));
  t234 = ((t235) < (t238));
  }
  t239 = t234;
  t240 = t239;
  if (t240) {
  t241 = ((int64_t)(v32));
  t242 = ((int64_t)(((int64_t)UINT64_C(0))));
  t240 = ((t241) >= (t242));
  }
  t243 = t240;
  t244 = t243;
  if (t244) {
  t245 = ((int64_t)(v32));
  /* FG_DIRECT_SLICE_INDEX */
  t246 = ((int64_t)UINT64_C(1));
  t247 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t246, ((int64_t)UINT64_C(4)));
  t248 = ((int64_t)((*t247)));
  t244 = ((t245) < (t248));
  }
  t249 = t244;
  if (t249) {
  {
  t250 = &(((*(v0))).f4);
  t251 = v1;
  t252 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t253 = ((int64_t)UINT64_C(0));
  t254 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t253, ((int64_t)UINT64_C(4)));
  t255 = ((int64_t)((*t254)));
  t256 = ((int64_t)(((int64_t)((uint64_t)(t252) * (uint64_t)(t255)))));
  t257 = ((int64_t)(v32));
  /* FG_DIRECT_SLICE_INDEX */
  t258 = ((int64_t)UINT64_C(1));
  t259 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t258, ((int64_t)UINT64_C(4)));
  t260 = ((int64_t)((*t259)));
  t261 = ((int64_t)(((int64_t)((uint64_t)(t257) * (uint64_t)(t260)))));
  t262 = ((int64_t)(((int64_t)((uint64_t)(t256) + (uint64_t)(t261)))));
  t263 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t264 = ((int64_t)UINT64_C(2));
  t265 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t264, ((int64_t)UINT64_C(4)));
  t266 = ((int64_t)((*t265)));
  t267 = ((int64_t)(((int64_t)((uint64_t)(t263) * (uint64_t)(t266)))));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t262) + (uint64_t)(t267)))));
  t269 = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t270 = ((int64_t)UINT64_C(3));
  t271 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t270, ((int64_t)UINT64_C(4)));
  t272 = ((int64_t)((*t271)));
  t273 = ((int64_t)(((int64_t)((uint64_t)(t269) * (uint64_t)(t272)))));
  t274 = ((int64_t)(((int64_t)((uint64_t)(t268) + (uint64_t)(t273)))));
  v33 = gg_function_47(t250, t251, t274, base + 0);
  t275 = v28;
  if (t275) {
  {
  t276 = ((float)(v10));
  t277 = ((float)(v33));
  v10 = ((float)(((t276) + (t277))));
  }
  } else {
  {
  t278 = ((float)(v33));
  t279 = ((float)(v10));
  t280 = fmaxf(t278, t279);
  v10 = ((float)(t280));
  }
  }
  }
  }
  }
label7:;
  t216++; goto label6;
label8:;
  }
label4:;
  t214++; goto label3;
label5:;
  t281 = v28;
  if (t281) {
  {
  t282 = v10;
  t283 = ((int64_t)(v22));
  t284 = ((int64_t)(v23));
  t285 = ((float)(((float)(((int64_t)((uint64_t)(t283) * (uint64_t)(t284)))))));
  v10 = ((t282) / (t285));
  }
  }
  }
  } else {
  t286 = ((int64_t)(((*(v0))).f8));
  t287 = ((int64_t)(((int64_t)UINT64_C(78))));
  t288 = ((t286) == (t287));
  if (t288) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t289 = ((int64_t)UINT64_C(0));
  t290 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t289, ((int64_t)UINT64_C(16)));
  v34 = ((int64_t)((*t290)));
  /* FG_DIRECT_SLICE_INDEX */
  t291 = ((int64_t)UINT64_C(1));
  t292 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t291, ((int64_t)UINT64_C(16)));
  v35 = ((int64_t)((*t292)));
  t293 = ((int64_t)(v34));
  t294 = ((int64_t)(((int64_t)UINT64_C(0))));
  t295 = ((t293) > (t294));
  t296 = t295;
  if (t296) {
  t297 = ((int64_t)(v35));
  t298 = ((int64_t)(((int64_t)UINT64_C(0))));
  t296 = ((t297) > (t298));
  }
  if (!(t296)) gg_fail("assertion failed");
  t299 = ((int64_t)(v9));
  t300 = ((int64_t)(v35));
  v36 = ((int64_t)(((int64_t)gg_mod(t299, t300))));
  t301 = ((int64_t)(v9));
  t302 = ((int64_t)(v35));
  v37 = ((int64_t)(((int64_t)gg_div(t301, t302))));
  t303 = ((int64_t)(v36));
  t304 = ((int64_t)(v34));
  t305 = ((int64_t)(((int64_t)((uint64_t)(t303) * (uint64_t)(t304)))));
  t306 = ((int64_t)(v7));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t305) + (uint64_t)(t306)))));
  t307 = ((int64_t)(v37));
  t308 = ((int64_t)(v34));
  t309 = ((int64_t)(((int64_t)((uint64_t)(t307) * (uint64_t)(t308)))));
  t310 = ((int64_t)(v8));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t309) + (uint64_t)(t310)))));
  t311 = ((int64_t)(v38));
  /* FG_DIRECT_SLICE_INDEX */
  t312 = ((int64_t)UINT64_C(1));
  t313 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t312, ((int64_t)UINT64_C(4)));
  t314 = ((int64_t)((*t313)));
  t315 = ((t311) < (t314));
  t316 = t315;
  if (t316) {
  t317 = ((int64_t)(v39));
  /* FG_DIRECT_SLICE_INDEX */
  t318 = ((int64_t)UINT64_C(2));
  t319 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t318, ((int64_t)UINT64_C(4)));
  t320 = ((int64_t)((*t319)));
  t316 = ((t317) < (t320));
  }
  t321 = t316;
  if (t321) {
  {
  t322 = &(((*(v0))).f4);
  t323 = v1;
  t324 = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t325 = ((int64_t)UINT64_C(0));
  t326 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t325, ((int64_t)UINT64_C(4)));
  t327 = ((int64_t)((*t326)));
  t328 = ((int64_t)(((int64_t)((uint64_t)(t324) * (uint64_t)(t327)))));
  t329 = ((int64_t)(v38));
  /* FG_DIRECT_SLICE_INDEX */
  t330 = ((int64_t)UINT64_C(1));
  t331 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t330, ((int64_t)UINT64_C(4)));
  t332 = ((int64_t)((*t331)));
  t333 = ((int64_t)(((int64_t)((uint64_t)(t329) * (uint64_t)(t332)))));
  t334 = ((int64_t)(((int64_t)((uint64_t)(t328) + (uint64_t)(t333)))));
  t335 = ((int64_t)(v39));
  /* FG_DIRECT_SLICE_INDEX */
  t336 = ((int64_t)UINT64_C(2));
  t337 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t336, ((int64_t)UINT64_C(4)));
  t338 = ((int64_t)((*t337)));
  t339 = ((int64_t)(((int64_t)((uint64_t)(t335) * (uint64_t)(t338)))));
  t340 = ((int64_t)(((int64_t)((uint64_t)(t334) + (uint64_t)(t339)))));
  v10 = gg_function_47(t322, t323, t340, base + 0);
  }
  }
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t341 = ((int64_t)UINT64_C(0));
  t342 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t341, ((int64_t)UINT64_C(16)));
  v40 = ((int64_t)((*t342)));
  /* FG_DIRECT_SLICE_INDEX */
  t343 = ((int64_t)UINT64_C(1));
  t344 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t343, ((int64_t)UINT64_C(4)));
  t345 = ((int64_t)((*t344)));
  t346 = ((int64_t)(v40));
  t347 = ((int64_t)(((int64_t)((uint64_t)(t345) + (uint64_t)(t346)))));
  t348 = ((int64_t)(((int64_t)UINT64_C(1))));
  t349 = ((int64_t)(((int64_t)((uint64_t)(t347) - (uint64_t)(t348)))));
  t350 = ((int64_t)(v40));
  v41 = ((int64_t)(((int64_t)gg_div(t349, t350))));
  t351 = ((int64_t)(v8));
  t352 = ((int64_t)(v40));
  t353 = ((int64_t)(((int64_t)gg_div(t351, t352))));
  t354 = ((int64_t)(v41));
  t355 = ((int64_t)(((int64_t)((uint64_t)(t353) * (uint64_t)(t354)))));
  t356 = ((int64_t)(v7));
  t357 = ((int64_t)(v40));
  t358 = ((int64_t)(((int64_t)gg_div(t356, t357))));
  v42 = ((int64_t)(((int64_t)((uint64_t)(t355) + (uint64_t)(t358)))));
  t359 = &(((*(v0))).f4);
  t360 = v1;
  t361 = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t362 = ((int64_t)UINT64_C(0));
  t363 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t362, ((int64_t)UINT64_C(4)));
  t364 = ((int64_t)((*t363)));
  t365 = ((int64_t)(((int64_t)((uint64_t)(t361) * (uint64_t)(t364)))));
  t366 = ((int64_t)(v7));
  t367 = ((int64_t)(v40));
  t368 = ((int64_t)(((int64_t)gg_mod(t366, t367))));
  /* FG_DIRECT_SLICE_INDEX */
  t369 = ((int64_t)UINT64_C(1));
  t370 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t369, ((int64_t)UINT64_C(4)));
  t371 = ((int64_t)((*t370)));
  t372 = ((int64_t)(((int64_t)((uint64_t)(t368) * (uint64_t)(t371)))));
  t373 = ((int64_t)(((int64_t)((uint64_t)(t365) + (uint64_t)(t372)))));
  t374 = ((int64_t)(v8));
  t375 = ((int64_t)(v40));
  t376 = ((int64_t)(((int64_t)gg_mod(t374, t375))));
  /* FG_DIRECT_SLICE_INDEX */
  t377 = ((int64_t)UINT64_C(2));
  t378 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t377, ((int64_t)UINT64_C(4)));
  t379 = ((int64_t)((*t378)));
  t380 = ((int64_t)(((int64_t)((uint64_t)(t376) * (uint64_t)(t379)))));
  t381 = ((int64_t)(((int64_t)((uint64_t)(t373) + (uint64_t)(t380)))));
  t382 = ((int64_t)(v42));
  /* FG_DIRECT_SLICE_INDEX */
  t383 = ((int64_t)UINT64_C(3));
  t384 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t383, ((int64_t)UINT64_C(4)));
  t385 = ((int64_t)((*t384)));
  t386 = ((int64_t)(((int64_t)((uint64_t)(t382) * (uint64_t)(t385)))));
  t387 = ((int64_t)(((int64_t)((uint64_t)(t381) + (uint64_t)(t386)))));
  v10 = gg_function_47(t359, t360, t387, base + 0);
  }
  }
  }
  }
  t388 = &(((*(v0))).f7);
  t389 = v3;
  t390 = ((int64_t)(v4));
  t391 = ((float)(v10));
  gg_function_37(t388, t389, t390, t391, base + 0);
  t392 = v4;
  t393 = ((int64_t)(((*(v0))).f10));
  v4 = ((int64_t)((uint64_t)(t392) + (uint64_t)(t393)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_15(gg_type_31 p0, gg_slice p1, int64_t p2, int64_t base) {
  int64_t t0;
  gg_type_31 t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  gg_slice t6;
  int64_t t7;
  uint32_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  gg_slice t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t * t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint64_t t33;
  gg_type_31 v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = v0;
  t2 = ((int64_t)(v2));
  v3 = gg_function_16(t1, t2, base + 0);
  t3 = ((int64_t)(((*(v0))).f2));
  t4 = ((int64_t)(((int64_t)UINT64_C(26))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  t6 = v1;
  t7 = ((int64_t)(v3));
  t8 = gg_function_17(t6, t7, base + 0);
  t0 = ((int64_t)(((int64_t)(((int32_t)(t8))))));
  goto gg_done;
  }
  }
  t9 = ((int64_t)(((*(v0))).f2));
  t10 = ((int64_t)(((int64_t)UINT64_C(27))));
  t11 = ((t9) == (t10));
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t13) >= (t14));
  }
  t15 = t12;
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)((v1).len));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) - (uint64_t)(t19)))));
  t16 = ((t17) <= (t20));
  }
  if (!(t16)) gg_fail("assertion failed");
  v4 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)UINT64_C(8));
  t21 = 0;
label0:;
  if (t21 >= t22) goto label2;
  v5 = t21;
  {
  t23 = ((uint64_t)(v4));
  t24 = v1;
  t25 = ((int64_t)(v3));
  t26 = ((int64_t)(v5));
  t27 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  t28 = ((uint8_t *)t24.data) + gg_index(t27, t24.len);
  t29 = ((uint64_t)(((uint64_t)((*t28)))));
  t30 = ((int64_t)(((int64_t)UINT64_C(8))));
  t31 = ((int64_t)(v5));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))))));
  t33 = ((uint64_t)(((uint64_t)((uint64_t)(t29) << ((uint64_t)(t32) & 63)))));
  v4 = ((uint64_t)(((t23) | (t33))));
  }
label1:;
  t21++; goto label0;
label2:;
  t0 = ((int64_t)(((int64_t)(v4))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_52(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  gg_type_20 t31;
  int64_t t32;
  gg_type_31 t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t * t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t * t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t * t46;
  int64_t t47;
  int64_t t48;
  int64_t * t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t * t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t * t62;
  int64_t t63;
  int64_t t64;
  gg_type_31 t65;
  gg_slice t66;
  int64_t t67;
  float t68;
  uint8_t t69;
  gg_type_31 t70;
  gg_slice t71;
  int64_t t72;
  float t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  gg_type_31 t83;
  int64_t t84;
  int64_t t85;
  gg_type_31 t86;
  gg_slice t87;
  int64_t t88;
  float t89;
  int64_t t90;
  gg_type_31 t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t * t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t * t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t * t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t * t114;
  int64_t t115;
  gg_type_31 t116;
  gg_slice t117;
  int64_t t118;
  gg_type_31 t119;
  gg_slice t120;
  int64_t t121;
  float t122;
  float t123;
  gg_type_31 t124;
  gg_slice t125;
  int64_t t126;
  float t127;
  float t128;
  float t129;
  int64_t t130;
  int64_t t131;
  uint8_t t132;
  int64_t t133;
  int64_t t134;
  uint8_t t135;
  int64_t t136;
  gg_type_31 t137;
  int64_t t138;
  int64_t t139;
  gg_type_31 t140;
  gg_slice t141;
  int64_t t142;
  float t143;
  int64_t t144;
  int64_t * t145;
  int64_t t146;
  gg_type_31 t147;
  int64_t t148;
  int64_t t149;
  gg_type_31 t150;
  gg_slice t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint8_t t155;
  uint8_t t156;
  int64_t t157;
  int64_t t158;
  int64_t * t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  gg_type_31 t167;
  gg_slice t168;
  int64_t t169;
  gg_type_31 t170;
  gg_slice t171;
  int64_t t172;
  float t173;
  float t174;
  gg_type_31 t175;
  gg_slice t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  float t182;
  float t183;
  float t184;
  int64_t t185;
  int64_t t186;
  uint8_t t187;
  int64_t t188;
  int64_t t189;
  uint8_t t190;
  int64_t t191;
  int64_t * t192;
  int64_t t193;
  int64_t t194;
  int64_t * t195;
  int64_t t196;
  int64_t t197;
  gg_type_31 t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t * t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t * t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t * t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t * t215;
  int64_t t216;
  int64_t t217;
  int64_t * t218;
  int64_t t219;
  int64_t t220;
  gg_type_31 t221;
  gg_slice t222;
  int64_t t223;
  int64_t t224;
  int64_t * t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t * t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t * t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t * t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  uint8_t t246;
  uint8_t t247;
  int64_t t248;
  int64_t t249;
  int64_t * t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t * t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t * t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t * t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t * t273;
  int64_t t274;
  int64_t t275;
  gg_type_31 t276;
  gg_slice t277;
  int64_t t278;
  gg_type_31 t279;
  gg_slice t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  float t286;
  int64_t t287;
  int64_t t288;
  uint8_t t289;
  int64_t t290;
  int64_t t291;
  uint8_t t292;
  gg_type_31 t293;
  int64_t t294;
  int64_t t295;
  gg_type_31 t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t * t300;
  int64_t t301;
  int64_t t302;
  uint8_t t303;
  int64_t t304;
  gg_type_31 t305;
  int64_t t306;
  int64_t t307;
  gg_type_31 t308;
  int64_t t309;
  gg_type_31 t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  gg_slice t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  uint8_t * t320;
  gg_slice t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  uint8_t * t325;
  int64_t t326;
  gg_type_31 t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  int64_t * t331;
  int64_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  int64_t * t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  int64_t * t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  uint8_t t344;
  int64_t t345;
  int64_t * t346;
  gg_slice t347;
  int64_t t348;
  int64_t t349;
  int64_t t350;
  int64_t * t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  gg_type_31 t356;
  gg_slice t357;
  int64_t t358;
  gg_type_31 t359;
  gg_slice t360;
  int64_t t361;
  float t362;
  int64_t t363;
  int64_t t364;
  uint8_t t365;
  gg_type_31 t366;
  gg_slice t367;
  int64_t t368;
  float t369;
  float t370;
  float t371;
  float t372;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
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
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
  int64_t v38;
  float v39;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(42))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f8));
  t6 = ((int64_t)(((int64_t)UINT64_C(41))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f8));
  t10 = ((int64_t)(((int64_t)UINT64_C(21))));
  t8 = ((t9) != (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(((*(v0))).f8));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t12 = ((t13) != (t14));
  }
  t15 = t12;
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(((*(v0))).f8));
  t18 = ((int64_t)(((int64_t)UINT64_C(5))));
  t16 = ((t17) != (t18));
  }
  t19 = t16;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(((*(v0))).f8));
  t22 = ((int64_t)(((int64_t)UINT64_C(33))));
  t20 = ((t21) != (t22));
  }
  t23 = t20;
  t24 = t23;
  if (t24) {
  t25 = ((int64_t)(((*(v0))).f8));
  t26 = ((int64_t)(((int64_t)UINT64_C(72))));
  t24 = ((t25) != (t26));
  }
  t27 = t24;
  if (t27) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(((*(v0))).f8));
  t29 = ((int64_t)(((int64_t)UINT64_C(72))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t31 = v0;
  v4 = gg_function_14(t31, base + 0);
label1:;
  t32 = ((int64_t)(v4));
  t33 = &(((*(v0))).f7);
  t34 = gg_function_12(t33, base + 0);
  t35 = ((int64_t)(t34));
  if (!(((t32) < (t35)))) goto label2;
  {
  t36 = ((int64_t)(v4));
  /* FG_DIRECT_SLICE_INDEX */
  t37 = ((int64_t)UINT64_C(0));
  t38 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t37, ((int64_t)UINT64_C(4)));
  t39 = ((int64_t)((*t38)));
  v5 = ((int64_t)(((int64_t)gg_mod(t36, t39))));
  t40 = ((int64_t)(v4));
  /* FG_DIRECT_SLICE_INDEX */
  t41 = ((int64_t)UINT64_C(0));
  t42 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t41, ((int64_t)UINT64_C(4)));
  t43 = ((int64_t)((*t42)));
  t44 = ((int64_t)(((int64_t)gg_div(t40, t43))));
  /* FG_DIRECT_SLICE_INDEX */
  t45 = ((int64_t)UINT64_C(1));
  t46 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t45, ((int64_t)UINT64_C(4)));
  t47 = ((int64_t)((*t46)));
  v6 = ((int64_t)(((int64_t)gg_mod(t44, t47))));
  /* FG_DIRECT_SLICE_INDEX */
  t48 = ((int64_t)UINT64_C(0));
  t49 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t48, ((int64_t)UINT64_C(16)));
  t50 = ((int64_t)((*t49)));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((t50) != (t51));
  if (t52) {
  {
  t53 = ((int64_t)(v5));
  t54 = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t55 = ((int64_t)UINT64_C(1));
  t56 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t55, ((int64_t)UINT64_C(16)));
  t57 = ((int64_t)((*t56)));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t57)))));
  v7 = ((t53) >= (t58));
  }
  } else {
  {
  t59 = ((int64_t)(v5));
  t60 = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t61 = ((int64_t)UINT64_C(1));
  t62 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t61, ((int64_t)UINT64_C(16)));
  t63 = ((int64_t)((*t62)));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t63)))));
  v7 = ((t59) <= (t64));
  }
  }
  t65 = &(((*(v0))).f7);
  t66 = v3;
  t67 = ((int64_t)(v4));
  t69 = v7;
  if (t69) {
  {
  t70 = &(((*(v0))).f4);
  t71 = v1;
  t72 = ((int64_t)(v4));
  t68 = gg_function_51(t70, t71, t72, base + 0);
  }
  } else {
  {
  t68 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  }
  t73 = ((float)(t68));
  gg_function_37(t65, t66, t67, t73, base + 0);
  t74 = v4;
  t75 = ((int64_t)(((*(v0))).f10));
  v4 = ((int64_t)((uint64_t)(t74) + (uint64_t)(t75)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t76 = ((int64_t)(((*(v0))).f8));
  t77 = ((int64_t)(((int64_t)UINT64_C(21))));
  t78 = ((t76) == (t77));
  if (t78) {
  {
  t79 = ((int64_t)(((*(v0))).f9));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = ((t79) != (t80));
  if (t81) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t83 = &(((*(v0))).f7);
  t84 = gg_function_12(t83, base + 0);
  t85 = t84;
  t82 = 0;
label3:;
  if (t82 >= t85) goto label5;
  v8 = t82;
  {
  t86 = &(((*(v0))).f7);
  t87 = v3;
  t88 = ((int64_t)(v8));
  t89 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  gg_function_37(t86, t87, t88, t89, base + 0);
  }
label4:;
  t82++; goto label3;
label5:;
  t91 = &(((*(v0))).f4);
  t92 = gg_function_12(t91, base + 0);
  t93 = t92;
  t90 = 0;
label6:;
  if (t90 >= t93) goto label8;
  v9 = t90;
  {
  v10 = ((int64_t)(v9));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  v12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t95 = ((int64_t)UINT64_C(4));
  t94 = 0;
label9:;
  if (t94 >= t95) goto label11;
  v13 = t94;
  {
  t96 = ((int64_t)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t97 = v13;
  t98 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t97, ((int64_t)UINT64_C(4)));
  t99 = ((int64_t)((*t98)));
  v14 = ((int64_t)(((int64_t)gg_mod(t96, t99))));
  t100 = v10;
  /* FG_DIRECT_SLICE_INDEX */
  t101 = v13;
  t102 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t101, ((int64_t)UINT64_C(4)));
  t103 = ((int64_t)((*t102)));
  v10 = ((int64_t)gg_div(t100, t103));
  t104 = v11;
  t105 = ((int64_t)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t106 = v13;
  t107 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t106, ((int64_t)UINT64_C(4)));
  t108 = ((int64_t)((*t107)));
  t109 = ((int64_t)(((int64_t)gg_mod(t105, t108))));
  t110 = ((int64_t)(v12));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t109) * (uint64_t)(t110)))));
  v11 = ((int64_t)((uint64_t)(t104) + (uint64_t)(t111)));
  t112 = v12;
  /* FG_DIRECT_SLICE_INDEX */
  t113 = v13;
  t114 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t113, ((int64_t)UINT64_C(4)));
  t115 = ((int64_t)((*t114)));
  v12 = ((int64_t)((uint64_t)(t112) * (uint64_t)(t115)));
  }
label10:;
  t94++; goto label9;
label11:;
  t116 = &(((*(v0))).f7);
  t117 = v3;
  t118 = ((int64_t)(v11));
  t119 = &(((*(v0))).f7);
  t120 = v3;
  t121 = ((int64_t)(v11));
  t122 = gg_function_51(t119, t120, t121, base + 0);
  t123 = ((float)(t122));
  t124 = &(((*(v0))).f4);
  t125 = v1;
  t126 = ((int64_t)(v9));
  t127 = gg_function_51(t124, t125, t126, base + 0);
  t128 = ((float)(t127));
  t129 = ((float)(((t123) + (t128))));
  gg_function_37(t116, t117, t118, t129, base + 0);
  }
label7:;
  t90++; goto label6;
label8:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t130 = ((int64_t)(((*(v0))).f8));
  t131 = ((int64_t)(((int64_t)UINT64_C(41))));
  t132 = ((t130) == (t131));
  if (t132) {
  {
  t133 = ((int64_t)(((*(v0))).f9));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((t133) != (t134));
  if (t135) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t137 = &(((*(v0))).f7);
  t138 = gg_function_12(t137, base + 0);
  t139 = t138;
  t136 = 0;
label12:;
  if (t136 >= t139) goto label14;
  v15 = t136;
  {
  t140 = &(((*(v0))).f7);
  t141 = v3;
  t142 = ((int64_t)(v15));
  t143 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  gg_function_37(t140, t141, t142, t143, base + 0);
  }
label13:;
  t136++; goto label12;
label14:;
  /* FG_DIRECT_SLICE_INDEX */
  t144 = ((int64_t)UINT64_C(0));
  t145 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t144, ((int64_t)UINT64_C(4)));
  v16 = ((int64_t)((*t145)));
  t147 = &(((*(v0))).f4);
  t148 = gg_function_13(t147, base + 0);
  t149 = t148;
  t146 = 0;
label15:;
  if (t146 >= t149) goto label17;
  v17 = t146;
  {
  t150 = &(((*(v0))).f5);
  t151 = v2;
  t152 = ((int64_t)(v17));
  v18 = gg_function_15(t150, t151, t152, base + 0);
  t153 = ((int64_t)(v18));
  t154 = ((int64_t)(((int64_t)UINT64_C(0))));
  t155 = ((t153) >= (t154));
  t156 = t155;
  if (t156) {
  t157 = ((int64_t)(v18));
  /* FG_DIRECT_SLICE_INDEX */
  t158 = ((int64_t)UINT64_C(1));
  t159 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t158, ((int64_t)UINT64_C(4)));
  t160 = ((int64_t)((*t159)));
  t156 = ((t157) < (t160));
  }
  if (!(t156)) gg_fail("assertion failed");
  t162 = v16;
  t161 = 0;
label18:;
  if (t161 >= t162) goto label20;
  v19 = t161;
  {
  t163 = ((int64_t)(v19));
  t164 = ((int64_t)(v18));
  t165 = ((int64_t)(v16));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t164) * (uint64_t)(t165)))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t163) + (uint64_t)(t166)))));
  t167 = &(((*(v0))).f7);
  t168 = v3;
  t169 = ((int64_t)(v20));
  t170 = &(((*(v0))).f7);
  t171 = v3;
  t172 = ((int64_t)(v20));
  t173 = gg_function_51(t170, t171, t172, base + 0);
  t174 = ((float)(t173));
  t175 = &(((*(v0))).f4);
  t176 = v1;
  t177 = ((int64_t)(v17));
  t178 = ((int64_t)(v16));
  t179 = ((int64_t)(((int64_t)((uint64_t)(t177) * (uint64_t)(t178)))));
  t180 = ((int64_t)(v19));
  t181 = ((int64_t)(((int64_t)((uint64_t)(t179) + (uint64_t)(t180)))));
  t182 = gg_function_51(t175, t176, t181, base + 0);
  t183 = ((float)(t182));
  t184 = ((float)(((t174) + (t183))));
  gg_function_37(t167, t168, t169, t184, base + 0);
  }
label19:;
  t161++; goto label18;
label20:;
  }
label16:;
  t146++; goto label15;
label17:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t185 = ((int64_t)(((*(v0))).f8));
  t186 = ((int64_t)(((int64_t)UINT64_C(42))));
  t187 = ((t185) == (t186));
  if (t187) {
  {
  t188 = ((int64_t)(((*(v0))).f9));
  t189 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = ((t188) != (t189));
  if (t190) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t191 = ((int64_t)UINT64_C(0));
  t192 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t191, ((int64_t)UINT64_C(4)));
  v21 = ((int64_t)((*t192)));
  t193 = ((int64_t)(v21));
  /* FG_DIRECT_SLICE_INDEX */
  t194 = ((int64_t)UINT64_C(0));
  t195 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t194, ((int64_t)UINT64_C(4)));
  t196 = ((int64_t)((*t195)));
  if (!(((t193) == (t196)))) gg_fail("assertion failed");
  t198 = &(((*(v0))).f4);
  t199 = gg_function_13(t198, base + 0);
  t200 = t199;
  t197 = 0;
label21:;
  if (t197 >= t200) goto label23;
  v22 = t197;
  {
  t201 = ((int64_t)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t202 = ((int64_t)UINT64_C(1));
  t203 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t202, ((int64_t)UINT64_C(4)));
  t204 = ((int64_t)((*t203)));
  v23 = ((int64_t)(((int64_t)gg_mod(t201, t204))));
  t205 = ((int64_t)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t206 = ((int64_t)UINT64_C(1));
  t207 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t206, ((int64_t)UINT64_C(4)));
  t208 = ((int64_t)((*t207)));
  t209 = ((int64_t)(((int64_t)gg_div(t205, t208))));
  /* FG_DIRECT_SLICE_INDEX */
  t210 = ((int64_t)UINT64_C(2));
  t211 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t210, ((int64_t)UINT64_C(4)));
  t212 = ((int64_t)((*t211)));
  v24 = ((int64_t)(((int64_t)gg_mod(t209, t212))));
  t213 = ((int64_t)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t214 = ((int64_t)UINT64_C(1));
  t215 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t214, ((int64_t)UINT64_C(4)));
  t216 = ((int64_t)((*t215)));
  /* FG_DIRECT_SLICE_INDEX */
  t217 = ((int64_t)UINT64_C(2));
  t218 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t217, ((int64_t)UINT64_C(4)));
  t219 = ((int64_t)((*t218)));
  t220 = ((int64_t)(((int64_t)((uint64_t)(t216) * (uint64_t)(t219)))));
  v25 = ((int64_t)(((int64_t)gg_div(t213, t220))));
  t221 = &(((*(v0))).f5);
  t222 = v2;
  t223 = ((int64_t)(v23));
  /* FG_DIRECT_SLICE_INDEX */
  t224 = ((int64_t)UINT64_C(0));
  t225 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t224, ((int64_t)UINT64_C(4)));
  t226 = ((int64_t)((*t225)));
  t227 = ((int64_t)(v24));
  /* FG_DIRECT_SLICE_INDEX */
  t228 = ((int64_t)UINT64_C(1));
  t229 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t228, ((int64_t)UINT64_C(4)));
  t230 = ((int64_t)((*t229)));
  t231 = ((int64_t)(((int64_t)gg_mod(t227, t230))));
  /* FG_DIRECT_SLICE_INDEX */
  t232 = ((int64_t)UINT64_C(1));
  t233 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t232, ((int64_t)UINT64_C(4)));
  t234 = ((int64_t)((*t233)));
  t235 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t236 = ((int64_t)UINT64_C(2));
  t237 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t236, ((int64_t)UINT64_C(4)));
  t238 = ((int64_t)((*t237)));
  t239 = ((int64_t)(((int64_t)gg_mod(t235, t238))));
  t240 = ((int64_t)(((int64_t)((uint64_t)(t234) * (uint64_t)(t239)))));
  t241 = ((int64_t)(((int64_t)((uint64_t)(t231) + (uint64_t)(t240)))));
  t242 = ((int64_t)(((int64_t)((uint64_t)(t226) * (uint64_t)(t241)))));
  t243 = ((int64_t)(((int64_t)((uint64_t)(t223) + (uint64_t)(t242)))));
  v26 = gg_function_15(t221, t222, t243, base + 0);
  t244 = ((int64_t)(v26));
  t245 = ((int64_t)(((int64_t)UINT64_C(0))));
  t246 = ((t244) >= (t245));
  t247 = t246;
  if (t247) {
  t248 = ((int64_t)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t249 = ((int64_t)UINT64_C(1));
  t250 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t249, ((int64_t)UINT64_C(4)));
  t251 = ((int64_t)((*t250)));
  t247 = ((t248) < (t251));
  }
  if (!(t247)) gg_fail("assertion failed");
  t253 = v21;
  t252 = 0;
label24:;
  if (t252 >= t253) goto label26;
  v27 = t252;
  {
  t254 = ((int64_t)(v27));
  /* FG_DIRECT_SLICE_INDEX */
  t255 = ((int64_t)UINT64_C(0));
  t256 = ((int64_t *)((((*(v0))).f7).f1).data) + gg_index(t255, ((int64_t)UINT64_C(4)));
  t257 = ((int64_t)((*t256)));
  t258 = ((int64_t)(((int64_t)((uint64_t)(t254) * (uint64_t)(t257)))));
  t259 = ((int64_t)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t260 = ((int64_t)UINT64_C(1));
  t261 = ((int64_t *)((((*(v0))).f7).f1).data) + gg_index(t260, ((int64_t)UINT64_C(4)));
  t262 = ((int64_t)((*t261)));
  t263 = ((int64_t)(((int64_t)((uint64_t)(t259) * (uint64_t)(t262)))));
  t264 = ((int64_t)(((int64_t)((uint64_t)(t258) + (uint64_t)(t263)))));
  t265 = ((int64_t)(v24));
  /* FG_DIRECT_SLICE_INDEX */
  t266 = ((int64_t)UINT64_C(2));
  t267 = ((int64_t *)((((*(v0))).f7).f1).data) + gg_index(t266, ((int64_t)UINT64_C(4)));
  t268 = ((int64_t)((*t267)));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t265) * (uint64_t)(t268)))));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t264) + (uint64_t)(t269)))));
  t271 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t272 = ((int64_t)UINT64_C(3));
  t273 = ((int64_t *)((((*(v0))).f7).f1).data) + gg_index(t272, ((int64_t)UINT64_C(4)));
  t274 = ((int64_t)((*t273)));
  t275 = ((int64_t)(((int64_t)((uint64_t)(t271) * (uint64_t)(t274)))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t270) + (uint64_t)(t275)))));
  t276 = &(((*(v0))).f7);
  t277 = v3;
  t278 = ((int64_t)(v28));
  t279 = &(((*(v0))).f4);
  t280 = v1;
  t281 = ((int64_t)(v22));
  t282 = ((int64_t)(v21));
  t283 = ((int64_t)(((int64_t)((uint64_t)(t281) * (uint64_t)(t282)))));
  t284 = ((int64_t)(v27));
  t285 = ((int64_t)(((int64_t)((uint64_t)(t283) + (uint64_t)(t284)))));
  t286 = gg_function_51(t279, t280, t285, base + 0);
  gg_function_38(t276, t277, t278, t286, base + 0);
  }
label25:;
  t252++; goto label24;
label26:;
  }
label22:;
  t197++; goto label21;
label23:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t287 = ((int64_t)(((*(v0))).f8));
  t288 = ((int64_t)(((int64_t)UINT64_C(3))));
  t289 = ((t287) == (t288));
  if (t289) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t290 = ((int64_t)(((*(v0))).f9));
  t291 = ((int64_t)(((int64_t)UINT64_C(0))));
  t292 = ((t290) != (t291));
  if (t292) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t293 = &(((*(v0))).f4);
  t294 = gg_function_12(t293, base + 0);
  t295 = ((int64_t)(t294));
  t296 = &(((*(v0))).f7);
  t297 = gg_function_12(t296, base + 0);
  t298 = ((int64_t)(t297));
  if (!(((t295) == (t298)))) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t299 = ((int64_t)UINT64_C(4));
  t300 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t299, ((int64_t)UINT64_C(16)));
  t301 = ((int64_t)((*t300)));
  t302 = ((int64_t)(((int64_t)UINT64_C(0))));
  t303 = ((t301) == (t302));
  if (t303) {
  {
  t305 = &(((*(v0))).f4);
  t306 = gg_function_12(t305, base + 0);
  t307 = t306;
  t304 = 0;
label27:;
  if (t304 >= t307) goto label29;
  v29 = t304;
  {
  t308 = &(((*(v0))).f4);
  t309 = ((int64_t)(v29));
  v30 = gg_function_16(t308, t309, base + 0);
  t310 = &(((*(v0))).f7);
  t311 = ((int64_t)(v29));
  v31 = gg_function_16(t310, t311, base + 0);
  t313 = ((int64_t)((((*(v0))).f4).f2));
  t314 = gg_function_4(t313, base + 0);
  t315 = t314;
  t312 = 0;
label30:;
  if (t312 >= t315) goto label32;
  v32 = t312;
  {
  t316 = v3;
  t317 = ((int64_t)(v31));
  t318 = ((int64_t)(v32));
  t319 = ((int64_t)((uint64_t)(t317) + (uint64_t)(t318)));
  t320 = ((uint8_t *)t316.data) + gg_index(t319, t316.len);
  t321 = v1;
  t322 = ((int64_t)(v30));
  t323 = ((int64_t)(v32));
  t324 = ((int64_t)((uint64_t)(t322) + (uint64_t)(t323)));
  t325 = ((uint8_t *)t321.data) + gg_index(t324, t321.len);
  (*t320) = ((uint8_t)((*t325)));
  }
label31:;
  t312++; goto label30;
label32:;
  }
label28:;
  t304++; goto label27;
label29:;
  }
  }
  t327 = &(((*(v0))).f5);
  t328 = gg_function_12(t327, base + 0);
  t329 = t328;
  t326 = 0;
label33:;
  if (t326 >= t329) goto label35;
  v33 = t326;
  {
  v34 = ((int64_t)(v33));
  /* FG_DIRECT_SLICE_INDEX */
  t330 = ((int64_t)UINT64_C(3));
  t331 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t330, ((int64_t)UINT64_C(16)));
  v35 = ((int64_t)((*t331)));
  t333 = ((int64_t)UINT64_C(4));
  t332 = 0;
label36:;
  if (t332 >= t333) goto label38;
  v36 = t332;
  {
  t334 = ((int64_t)(v34));
  /* FG_DIRECT_SLICE_INDEX */
  t335 = v36;
  t336 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t335, ((int64_t)UINT64_C(4)));
  t337 = ((int64_t)((*t336)));
  v37 = ((int64_t)(((int64_t)gg_mod(t334, t337))));
  t338 = v34;
  /* FG_DIRECT_SLICE_INDEX */
  t339 = v36;
  t340 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t339, ((int64_t)UINT64_C(4)));
  t341 = ((int64_t)((*t340)));
  v34 = ((int64_t)gg_div(t338, t341));
  t342 = ((int64_t)(v36));
  t343 = ((int64_t)(((int64_t)UINT64_C(0))));
  t344 = ((t342) == (t343));
  if (t344) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t345 = ((int64_t)UINT64_C(0));
  t346 = ((int64_t *)((((*(v0))).f7).f1).data) + gg_index(t345, ((int64_t)UINT64_C(4)));
  v38 = ((int64_t)((*t346)));
  }
  } else {
  {
  t347 = gg_view((((*(v0))).f11).data, 16);
  t348 = ((int64_t)(v36));
  t349 = ((int64_t)(((int64_t)UINT64_C(1))));
  t350 = ((int64_t)((uint64_t)(t348) - (uint64_t)(t349)));
  t351 = ((int64_t *)t347.data) + gg_index(t350, t347.len);
  v38 = ((int64_t)((*t351)));
  }
  }
  t352 = ((int64_t)(v35));
  t353 = ((int64_t)(v37));
  t354 = ((int64_t)(v38));
  t355 = gg_function_2(t353, t354, base + 0);
  v35 = gg_function_5(t352, t355, base + 0);
  }
label37:;
  t332++; goto label36;
label38:;
  t356 = &(((*(v0))).f5);
  t357 = v2;
  t358 = ((int64_t)(v33));
  v39 = gg_function_51(t356, t357, t358, base + 0);
  t359 = &(((*(v0))).f7);
  t360 = v3;
  t361 = ((int64_t)(v35));
  t363 = ((int64_t)(((*(v0))).f8));
  t364 = ((int64_t)(((int64_t)UINT64_C(5))));
  t365 = ((t363) == (t364));
  if (t365) {
  {
  t366 = &(((*(v0))).f7);
  t367 = v3;
  t368 = ((int64_t)(v35));
  t369 = gg_function_47(t366, t367, t368, base + 0);
  t370 = ((float)(t369));
  t371 = ((float)(v39));
  t362 = ((float)(((t370) + (t371))));
  }
  } else {
  {
  t362 = ((float)(v39));
  }
  }
  t372 = ((float)(t362));
  gg_function_38(t359, t360, t361, t372, base + 0);
  }
label34:;
  t326++; goto label33;
label35:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_45(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  gg_type_20 t60;
  gg_type_31 t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  uint8_t t68;
  uint8_t t69;
  int64_t t70;
  int64_t t71;
  uint8_t t72;
  uint8_t t73;
  int64_t t74;
  int64_t t75;
  uint8_t t76;
  int64_t t77;
  gg_type_31 t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  gg_type_31 t84;
  int64_t t85;
  gg_type_31 t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  gg_slice t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  uint8_t * t94;
  gg_slice t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint8_t * t99;
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
  int64_t * t110;
  int64_t t111;
  int64_t t112;
  int64_t * t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t * t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
  int64_t t122;
  float * t123;
  int64_t t124;
  int64_t t125;
  uint8_t t126;
  int64_t t127;
  float * t128;
  float t129;
  int64_t t130;
  float * t131;
  float t132;
  float t133;
  float t134;
  int64_t t135;
  int64_t t136;
  uint8_t t137;
  gg_type_31 t138;
  gg_slice t139;
  gg_type_31 t140;
  gg_type_31 t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
  int64_t t147;
  int64_t * t148;
  int64_t t149;
  int64_t t150;
  uint8_t t151;
  uint8_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t * t156;
  int64_t t157;
  int64_t t158;
  int64_t * t159;
  int64_t t160;
  uint8_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t * t166;
  int64_t t167;
  int64_t t168;
  int64_t * t169;
  int64_t t170;
  int64_t t171;
  gg_type_31 t172;
  gg_slice t173;
  int64_t t174;
  int64_t t175;
  int64_t * t176;
  int64_t t177;
  int64_t t178;
  int64_t * t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t * t185;
  int64_t t186;
  int64_t t187;
  int64_t * t188;
  int64_t t189;
  int64_t t190;
  gg_type_31 t191;
  gg_slice t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  uint8_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t * t200;
  int64_t t201;
  gg_slice t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t * t206;
  int64_t t207;
  int64_t t208;
  int64_t * t209;
  int64_t t210;
  int64_t t211;
  uint8_t t212;
  int64_t t213;
  int64_t * t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t * t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t * t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t * t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  uint8_t t231;
  uint8_t t232;
  int64_t t233;
  int64_t t234;
  int64_t * t235;
  int64_t t236;
  uint8_t t237;
  int64_t t238;
  int64_t t239;
  uint8_t t240;
  uint8_t t241;
  int64_t t242;
  int64_t t243;
  int64_t * t244;
  int64_t t245;
  uint8_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t * t250;
  int64_t t251;
  int64_t t252;
  uint8_t t253;
  gg_type_31 t254;
  gg_slice t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  uint8_t t259;
  int64_t t260;
  int64_t * t261;
  int64_t t262;
  int64_t * t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  uint8_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t * t272;
  int64_t t273;
  uint8_t t274;
  int64_t t275;
  int64_t t276;
  int64_t * t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  uint8_t t285;
  uint8_t t286;
  int64_t t287;
  int64_t t288;
  int64_t * t289;
  int64_t t290;
  gg_type_31 t291;
  gg_slice t292;
  int64_t t293;
  int64_t t294;
  int64_t * t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t * t299;
  int64_t t300;
  int64_t t301;
  int64_t * t302;
  int64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t * t307;
  int64_t t308;
  int64_t t309;
  int64_t * t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t * t315;
  int64_t t316;
  int64_t t317;
  int64_t * t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  uint8_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  int64_t * t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t * t332;
  int64_t t333;
  int64_t t334;
  int64_t * t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  int64_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  int64_t * t346;
  int64_t t347;
  int64_t t348;
  gg_type_31 t349;
  gg_slice t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  uint8_t t354;
  int64_t t355;
  int64_t * t356;
  int64_t t357;
  int64_t t358;
  int64_t * t359;
  int64_t t360;
  uint8_t t361;
  gg_type_31 t362;
  gg_slice t363;
  int64_t t364;
  int64_t * t365;
  int64_t t366;
  int64_t t367;
  int64_t * t368;
  int64_t t369;
  int64_t t370;
  int64_t * t371;
  int64_t t372;
  int64_t t373;
  int64_t * t374;
  int64_t t375;
  int64_t t376;
  int64_t * t377;
  int64_t t378;
  int64_t t379;
  int64_t t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  int64_t t384;
  uint8_t t385;
  uint8_t t386;
  int64_t t387;
  int64_t t388;
  uint8_t t389;
  gg_type_31 t390;
  gg_slice t391;
  int64_t t392;
  int64_t t393;
  int64_t * t394;
  int64_t t395;
  int64_t t396;
  int64_t * t397;
  int64_t t398;
  int64_t t399;
  int64_t * t400;
  int64_t t401;
  int64_t t402;
  uint8_t t403;
  int64_t t404;
  int64_t t405;
  uint8_t t406;
  uint32_t t407;
  float t408;
  int64_t t409;
  int64_t t410;
  uint8_t t411;
  int64_t t412;
  int64_t t413;
  int64_t * t414;
  int64_t t415;
  gg_slice t416;
  gg_type_31 t417;
  int64_t t418;
  int64_t t419;
  uint32_t t420;
  int32_t t421;
  int32_t t422;
  uint8_t t423;
  uint8_t t424;
  int64_t t425;
  int64_t t426;
  int64_t * t427;
  int64_t t428;
  int64_t t429;
  int64_t * t430;
  int64_t t431;
  int64_t t432;
  int64_t * t433;
  int64_t t434;
  int64_t t435;
  int64_t t436;
  int64_t t437;
  int64_t * t438;
  int64_t t439;
  int64_t t440;
  int64_t t441;
  int64_t t442;
  int64_t * t443;
  int64_t t444;
  int64_t t445;
  int64_t * t446;
  int64_t t447;
  int64_t t448;
  int64_t t449;
  int64_t t450;
  int64_t * t451;
  int64_t t452;
  int64_t t453;
  int64_t * t454;
  int64_t t455;
  int64_t t456;
  gg_type_31 t457;
  gg_slice t458;
  int64_t t459;
  int64_t t460;
  int64_t t461;
  uint8_t t462;
  int64_t t463;
  gg_type_31 t464;
  int64_t t465;
  int64_t t466;
  gg_type_31 t467;
  gg_slice t468;
  int64_t t469;
  gg_type_31 t470;
  gg_slice t471;
  int64_t t472;
  float t473;
  int64_t t474;
  int64_t t475;
  gg_type_20 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  gg_type_4 v12;
  int64_t v13;
  float v14;
  int64_t v15;
  int64_t v16;
  gg_type_4 v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  uint8_t v22;
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
  int32_t v34;
  int64_t v35;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  v4 = ((int64_t)(((*(v0))).f8));
  t1 = ((int64_t)(v4));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(34))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)UINT64_C(35))));
  t8 = ((t9) != (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(((int64_t)UINT64_C(20))));
  t12 = ((t13) != (t14));
  }
  t15 = t12;
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(v4));
  t18 = ((int64_t)(((int64_t)UINT64_C(22))));
  t16 = ((t17) != (t18));
  }
  t19 = t16;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(((int64_t)UINT64_C(64))));
  t20 = ((t21) != (t22));
  }
  t23 = t20;
  t24 = t23;
  if (t24) {
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(((int64_t)UINT64_C(65))));
  t24 = ((t25) != (t26));
  }
  t27 = t24;
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(v4));
  t30 = ((int64_t)(((int64_t)UINT64_C(66))));
  t28 = ((t29) != (t30));
  }
  t31 = t28;
  t32 = t31;
  if (t32) {
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)UINT64_C(67))));
  t32 = ((t33) != (t34));
  }
  t35 = t32;
  t36 = t35;
  if (t36) {
  t37 = ((int64_t)(v4));
  t38 = ((int64_t)(((int64_t)UINT64_C(73))));
  t36 = ((t37) != (t38));
  }
  t39 = t36;
  t40 = t39;
  if (t40) {
  t41 = ((int64_t)(v4));
  t42 = ((int64_t)(((int64_t)UINT64_C(43))));
  t40 = ((t41) != (t42));
  }
  t43 = t40;
  t44 = t43;
  if (t44) {
  t45 = ((int64_t)(v4));
  t46 = ((int64_t)(((int64_t)UINT64_C(44))));
  t44 = ((t45) != (t46));
  }
  t47 = t44;
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(v4));
  t50 = ((int64_t)(((int64_t)UINT64_C(45))));
  t48 = ((t49) != (t50));
  }
  t51 = t48;
  t52 = t51;
  if (t52) {
  t53 = ((int64_t)(v4));
  t54 = ((int64_t)(((int64_t)UINT64_C(40))));
  t52 = ((t53) != (t54));
  }
  t55 = t52;
  t56 = t55;
  if (t56) {
  t57 = ((int64_t)(v4));
  t58 = ((int64_t)(((int64_t)UINT64_C(42))));
  t56 = ((t57) != (t58));
  }
  t59 = t56;
  if (t59) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t60 = v0;
  v5 = gg_function_14(t60, base + 0);
  t61 = &(((*(v0))).f7);
  v6 = gg_function_12(t61, base + 0);
  t62 = ((int64_t)(v4));
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  t64 = ((t62) == (t63));
  t65 = t64;
  if (!t65) {
  t66 = ((int64_t)(v4));
  t67 = ((int64_t)(((int64_t)UINT64_C(34))));
  t65 = ((t66) == (t67));
  }
  t68 = t65;
  t69 = t68;
  if (!t69) {
  t70 = ((int64_t)(v4));
  t71 = ((int64_t)(((int64_t)UINT64_C(35))));
  t69 = ((t70) == (t71));
  }
  t72 = t69;
  t73 = t72;
  if (t73) {
  t74 = ((int64_t)((((*(v0))).f4).f2));
  t75 = ((int64_t)((((*(v0))).f7).f2));
  t73 = ((t74) == (t75));
  }
  t76 = t73;
  if (t76) {
  {
  t77 = ((int64_t)(v6));
  t78 = &(((*(v0))).f4);
  t79 = gg_function_12(t78, base + 0);
  t80 = ((int64_t)(t79));
  if (!(((t77) == (t80)))) gg_fail("assertion failed");
  t81 = ((int64_t)((((*(v0))).f4).f2));
  v7 = gg_function_4(t81, base + 0);
label1:;
  t82 = ((int64_t)(v5));
  t83 = ((int64_t)(v6));
  if (!(((t82) < (t83)))) goto label2;
  {
  t84 = &(((*(v0))).f4);
  t85 = ((int64_t)(v5));
  v8 = gg_function_16(t84, t85, base + 0);
  t86 = &(((*(v0))).f7);
  t87 = ((int64_t)(v5));
  v9 = gg_function_16(t86, t87, base + 0);
  t89 = v7;
  t88 = 0;
label3:;
  if (t88 >= t89) goto label5;
  v10 = t88;
  {
  t90 = v3;
  t91 = ((int64_t)(v9));
  t92 = ((int64_t)(v10));
  t93 = ((int64_t)((uint64_t)(t91) + (uint64_t)(t92)));
  t94 = ((uint8_t *)t90.data) + gg_index(t93, t90.len);
  t95 = v1;
  t96 = ((int64_t)(v8));
  t97 = ((int64_t)(v10));
  t98 = ((int64_t)((uint64_t)(t96) + (uint64_t)(t97)));
  t99 = ((uint8_t *)t95.data) + gg_index(t98, t95.len);
  (*t94) = ((uint8_t)((*t99)));
  }
label4:;
  t88++; goto label3;
label5:;
  t100 = v5;
  t101 = ((int64_t)(((*(v0))).f10));
  v5 = ((int64_t)((uint64_t)(t100) + (uint64_t)(t101)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
label7:;
  t102 = ((int64_t)(v5));
  t103 = ((int64_t)(v6));
  if (!(((t102) < (t103)))) goto label8;
  {
  v11 = ((int64_t)(v5));
  memset(&t104, 0, sizeof(t104));
  t105 = ((int64_t)(t104));
  for (t106=0; t106<4; t106++) (v12).data[t106] = t105;
  t108 = ((int64_t)UINT64_C(4));
  t107 = 0;
label9:;
  if (t107 >= t108) goto label11;
  v13 = t107;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t109 = v13;
  t110 = ((int64_t *)(v12).data) + gg_index(t109, ((int64_t)UINT64_C(4)));
  t111 = ((int64_t)(v11));
  /* FG_DIRECT_SLICE_INDEX */
  t112 = v13;
  t113 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t112, ((int64_t)UINT64_C(4)));
  t114 = ((int64_t)((*t113)));
  (*t110) = ((int64_t)(((int64_t)gg_mod(t111, t114))));
  t115 = v11;
  /* FG_DIRECT_SLICE_INDEX */
  t116 = v13;
  t117 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t116, ((int64_t)UINT64_C(4)));
  t118 = ((int64_t)((*t117)));
  v11 = ((int64_t)gg_div(t115, t118));
  }
label10:;
  t107++; goto label9;
label11:;
  v14 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v15 = ((int64_t)(v5));
  t119 = ((int64_t)(v4));
  t120 = ((int64_t)(((int64_t)UINT64_C(73))));
  t121 = ((t119) == (t120));
  if (t121) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t122 = ((int64_t)UINT64_C(0));
  t123 = ((float *)(((*(v0))).f12).data) + gg_index(t122, ((int64_t)UINT64_C(8)));
  v14 = ((float)((*t123)));
  }
  } else {
  t124 = ((int64_t)(v4));
  t125 = ((int64_t)(((int64_t)UINT64_C(67))));
  t126 = ((t124) == (t125));
  if (t126) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t127 = ((int64_t)UINT64_C(0));
  t128 = ((float *)(((*(v0))).f12).data) + gg_index(t127, ((int64_t)UINT64_C(8)));
  t129 = ((float)((*t128)));
  /* FG_DIRECT_SLICE_INDEX */
  t130 = ((int64_t)UINT64_C(2));
  t131 = ((float *)(((*(v0))).f12).data) + gg_index(t130, ((int64_t)UINT64_C(8)));
  t132 = ((float)((*t131)));
  t133 = ((float)(((float)(v5))));
  t134 = ((float)(((t132) * (t133))));
  v14 = ((float)(((t129) + (t134))));
  }
  } else {
  t135 = ((int64_t)(v4));
  t136 = ((int64_t)(((int64_t)UINT64_C(20))));
  t137 = ((t135) == (t136));
  if (t137) {
  {
  t138 = &(((*(v0))).f4);
  t139 = v1;
  t140 = &(((*(v0))).f4);
  t141 = &(((*(v0))).f7);
  t142 = ((int64_t)(v5));
  t143 = gg_function_46(t140, t141, t142, base + 0);
  v14 = gg_function_47(t138, t139, t143, base + 0);
  }
  } else {
  t144 = ((int64_t)(v4));
  t145 = ((int64_t)(((int64_t)UINT64_C(22))));
  t146 = ((t144) == (t145));
  if (t146) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t147 = ((int64_t)UINT64_C(0));
  t148 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t147, ((int64_t)UINT64_C(16)));
  v16 = ((int64_t)((*t148)));
  t149 = ((int64_t)(v16));
  t150 = ((int64_t)(((int64_t)UINT64_C(0))));
  t151 = ((t149) >= (t150));
  t152 = t151;
  if (t152) {
  t153 = ((int64_t)(v16));
  t154 = ((int64_t)(((int64_t)UINT64_C(4))));
  t152 = ((t153) < (t154));
  }
  if (!(t152)) gg_fail("assertion failed");
  v17 = v12;
  /* FG_DIRECT_SLICE_INDEX */
  t155 = v16;
  t156 = ((int64_t *)(v12).data) + gg_index(t155, ((int64_t)UINT64_C(4)));
  t157 = ((int64_t)((*t156)));
  /* FG_DIRECT_SLICE_INDEX */
  t158 = v16;
  t159 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t158, ((int64_t)UINT64_C(4)));
  t160 = ((int64_t)((*t159)));
  t161 = ((t157) < (t160));
  if (t161) {
  {
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t163 = ((int64_t)UINT64_C(4));
  t162 = 0;
label12:;
  if (t162 >= t163) goto label14;
  v19 = t162;
  {
  t164 = ((int64_t)(v18));
  /* FG_DIRECT_SLICE_INDEX */
  t165 = v19;
  t166 = ((int64_t *)(v17).data) + gg_index(t165, ((int64_t)UINT64_C(4)));
  t167 = ((int64_t)((*t166)));
  /* FG_DIRECT_SLICE_INDEX */
  t168 = v19;
  t169 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t168, ((int64_t)UINT64_C(4)));
  t170 = ((int64_t)((*t169)));
  t171 = gg_function_2(t167, t170, base + 0);
  v18 = gg_function_5(t164, t171, base + 0);
  }
label13:;
  t162++; goto label12;
label14:;
  t172 = &(((*(v0))).f4);
  t173 = v1;
  t174 = ((int64_t)(v18));
  v14 = gg_function_47(t172, t173, t174, base + 0);
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t175 = v16;
  t176 = ((int64_t *)(v17).data) + gg_index(t175, ((int64_t)UINT64_C(4)));
  t177 = (*t176);
  /* FG_DIRECT_SLICE_INDEX */
  t178 = v16;
  t179 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t178, ((int64_t)UINT64_C(4)));
  t180 = ((int64_t)((*t179)));
  (*t176) = ((int64_t)((uint64_t)(t177) - (uint64_t)(t180)));
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t182 = ((int64_t)UINT64_C(4));
  t181 = 0;
label15:;
  if (t181 >= t182) goto label17;
  v21 = t181;
  {
  t183 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t184 = v21;
  t185 = ((int64_t *)(v17).data) + gg_index(t184, ((int64_t)UINT64_C(4)));
  t186 = ((int64_t)((*t185)));
  /* FG_DIRECT_SLICE_INDEX */
  t187 = v21;
  t188 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t187, ((int64_t)UINT64_C(4)));
  t189 = ((int64_t)((*t188)));
  t190 = gg_function_2(t186, t189, base + 0);
  v20 = gg_function_5(t183, t190, base + 0);
  }
label16:;
  t181++; goto label15;
label17:;
  t191 = &(((*(v0))).f5);
  t192 = v2;
  t193 = ((int64_t)(v20));
  v14 = gg_function_47(t191, t192, t193, base + 0);
  }
  }
  }
  } else {
  t194 = ((int64_t)(v4));
  t195 = ((int64_t)(((int64_t)UINT64_C(64))));
  t196 = ((t194) == (t195));
  if (t196) {
  {
  v22 = ((uint8_t)UINT64_C(1));
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t198 = ((int64_t)UINT64_C(4));
  t197 = 0;
label18:;
  if (t197 >= t198) goto label20;
  v24 = t197;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t199 = v24;
  t200 = ((int64_t *)(v12).data) + gg_index(t199, ((int64_t)UINT64_C(4)));
  t201 = ((int64_t)((*t200)));
  t202 = gg_view((((*(v0))).f11).data, 16);
  t203 = ((int64_t)(((int64_t)UINT64_C(2))));
  t204 = ((int64_t)(v24));
  t205 = ((int64_t)((uint64_t)(t203) * (uint64_t)(t204)));
  t206 = ((int64_t *)t202.data) + gg_index(t205, t202.len);
  t207 = ((int64_t)((*t206)));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t201) - (uint64_t)(t207)))));
  /* FG_DIRECT_SLICE_INDEX */
  t208 = ((int64_t)UINT64_C(8));
  t209 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t208, ((int64_t)UINT64_C(16)));
  t210 = ((int64_t)((*t209)));
  t211 = ((int64_t)(((int64_t)UINT64_C(0))));
  t212 = ((t210) != (t211));
  if (t212) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t213 = v24;
  t214 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t213, ((int64_t)UINT64_C(4)));
  t215 = ((int64_t)((*t214)));
  t216 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t215) > (t216)))) gg_fail("assertion failed");
  t217 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t218 = v24;
  t219 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t218, ((int64_t)UINT64_C(4)));
  t220 = ((int64_t)((*t219)));
  t221 = ((int64_t)(((int64_t)gg_mod(t217, t220))));
  /* FG_DIRECT_SLICE_INDEX */
  t222 = v24;
  t223 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t222, ((int64_t)UINT64_C(4)));
  t224 = ((int64_t)((*t223)));
  t225 = ((int64_t)(((int64_t)((uint64_t)(t221) + (uint64_t)(t224)))));
  /* FG_DIRECT_SLICE_INDEX */
  t226 = v24;
  t227 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t226, ((int64_t)UINT64_C(4)));
  t228 = ((int64_t)((*t227)));
  v25 = ((int64_t)(((int64_t)gg_mod(t225, t228))));
  }
  } else {
  t229 = ((int64_t)(v25));
  t230 = ((int64_t)(((int64_t)UINT64_C(0))));
  t231 = ((t229) < (t230));
  t232 = t231;
  if (!t232) {
  t233 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t234 = v24;
  t235 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t234, ((int64_t)UINT64_C(4)));
  t236 = ((int64_t)((*t235)));
  t232 = ((t233) >= (t236));
  }
  t237 = t232;
  if (t237) {
  {
  v22 = ((uint8_t)UINT64_C(0));
  }
  }
  }
  t238 = ((int64_t)(v25));
  t239 = ((int64_t)(((int64_t)UINT64_C(0))));
  t240 = ((t238) >= (t239));
  t241 = t240;
  if (t241) {
  t242 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t243 = v24;
  t244 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t243, ((int64_t)UINT64_C(4)));
  t245 = ((int64_t)((*t244)));
  t241 = ((t242) < (t245));
  }
  t246 = t241;
  if (t246) {
  {
  t247 = ((int64_t)(v23));
  t248 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t249 = v24;
  t250 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t249, ((int64_t)UINT64_C(4)));
  t251 = ((int64_t)((*t250)));
  t252 = gg_function_2(t248, t251, base + 0);
  v23 = gg_function_5(t247, t252, base + 0);
  }
  }
  }
label19:;
  t197++; goto label18;
label20:;
  t253 = v22;
  if (t253) {
  {
  t254 = &(((*(v0))).f4);
  t255 = v1;
  t256 = ((int64_t)(v23));
  v14 = gg_function_47(t254, t255, t256, base + 0);
  }
  }
  }
  } else {
  t257 = ((int64_t)(v4));
  t258 = ((int64_t)(((int64_t)UINT64_C(65))));
  t259 = ((t257) == (t258));
  if (t259) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t260 = ((int64_t)UINT64_C(0));
  t261 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t260, ((int64_t)UINT64_C(16)));
  v26 = ((int64_t)((*t261)));
  /* FG_DIRECT_SLICE_INDEX */
  t262 = ((int64_t)UINT64_C(0));
  t263 = ((int64_t *)(v12).data) + gg_index(t262, ((int64_t)UINT64_C(4)));
  t264 = ((int64_t)((*t263)));
  t265 = ((int64_t)(v26));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t264) - (uint64_t)(t265)))));
  t266 = ((int64_t)(v27));
  t267 = ((int64_t)(((int64_t)UINT64_C(0))));
  t268 = ((t266) < (t267));
  if (t268) {
  {
  t269 = v27;
  v28 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t269)))));
  }
  } else {
  t270 = ((int64_t)(v27));
  /* FG_DIRECT_SLICE_INDEX */
  t271 = ((int64_t)UINT64_C(0));
  t272 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t271, ((int64_t)UINT64_C(4)));
  t273 = ((int64_t)((*t272)));
  t274 = ((t270) >= (t273));
  if (t274) {
  {
  t275 = ((int64_t)(((int64_t)UINT64_C(2))));
  /* FG_DIRECT_SLICE_INDEX */
  t276 = ((int64_t)UINT64_C(0));
  t277 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t276, ((int64_t)UINT64_C(4)));
  t278 = ((int64_t)((*t277)));
  t279 = ((int64_t)(((int64_t)UINT64_C(1))));
  t280 = ((int64_t)(((int64_t)((uint64_t)(t278) - (uint64_t)(t279)))));
  t281 = ((int64_t)(((int64_t)((uint64_t)(t275) * (uint64_t)(t280)))));
  t282 = ((int64_t)(v27));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t281) - (uint64_t)(t282)))));
  }
  } else {
  {
  v28 = ((int64_t)(v27));
  }
  }
  }
  t283 = ((int64_t)(v28));
  t284 = ((int64_t)(((int64_t)UINT64_C(0))));
  t285 = ((t283) >= (t284));
  t286 = t285;
  if (t286) {
  t287 = ((int64_t)(v28));
  /* FG_DIRECT_SLICE_INDEX */
  t288 = ((int64_t)UINT64_C(0));
  t289 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t288, ((int64_t)UINT64_C(4)));
  t290 = ((int64_t)((*t289)));
  t286 = ((t287) < (t290));
  }
  if (!(t286)) gg_fail("assertion failed");
  t291 = &(((*(v0))).f4);
  t292 = v1;
  t293 = ((int64_t)(v28));
  /* FG_DIRECT_SLICE_INDEX */
  t294 = ((int64_t)UINT64_C(0));
  t295 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t294, ((int64_t)UINT64_C(4)));
  t296 = ((int64_t)((*t295)));
  t297 = ((int64_t)(((int64_t)((uint64_t)(t293) * (uint64_t)(t296)))));
  /* FG_DIRECT_SLICE_INDEX */
  t298 = ((int64_t)UINT64_C(1));
  t299 = ((int64_t *)(v12).data) + gg_index(t298, ((int64_t)UINT64_C(4)));
  t300 = ((int64_t)((*t299)));
  /* FG_DIRECT_SLICE_INDEX */
  t301 = ((int64_t)UINT64_C(1));
  t302 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t301, ((int64_t)UINT64_C(4)));
  t303 = ((int64_t)((*t302)));
  t304 = ((int64_t)(((int64_t)((uint64_t)(t300) * (uint64_t)(t303)))));
  t305 = ((int64_t)(((int64_t)((uint64_t)(t297) + (uint64_t)(t304)))));
  /* FG_DIRECT_SLICE_INDEX */
  t306 = ((int64_t)UINT64_C(2));
  t307 = ((int64_t *)(v12).data) + gg_index(t306, ((int64_t)UINT64_C(4)));
  t308 = ((int64_t)((*t307)));
  /* FG_DIRECT_SLICE_INDEX */
  t309 = ((int64_t)UINT64_C(2));
  t310 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t309, ((int64_t)UINT64_C(4)));
  t311 = ((int64_t)((*t310)));
  t312 = ((int64_t)(((int64_t)((uint64_t)(t308) * (uint64_t)(t311)))));
  t313 = ((int64_t)(((int64_t)((uint64_t)(t305) + (uint64_t)(t312)))));
  /* FG_DIRECT_SLICE_INDEX */
  t314 = ((int64_t)UINT64_C(3));
  t315 = ((int64_t *)(v12).data) + gg_index(t314, ((int64_t)UINT64_C(4)));
  t316 = ((int64_t)((*t315)));
  /* FG_DIRECT_SLICE_INDEX */
  t317 = ((int64_t)UINT64_C(3));
  t318 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t317, ((int64_t)UINT64_C(4)));
  t319 = ((int64_t)((*t318)));
  t320 = ((int64_t)(((int64_t)((uint64_t)(t316) * (uint64_t)(t319)))));
  t321 = ((int64_t)(((int64_t)((uint64_t)(t313) + (uint64_t)(t320)))));
  v14 = gg_function_47(t291, t292, t321, base + 0);
  }
  } else {
  t322 = ((int64_t)(v4));
  t323 = ((int64_t)(((int64_t)UINT64_C(66))));
  t324 = ((t322) == (t323));
  if (t324) {
  {
  v29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t326 = ((int64_t)UINT64_C(4));
  t325 = 0;
label21:;
  if (t325 >= t326) goto label23;
  v30 = t325;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t327 = v30;
  t328 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t327, ((int64_t)UINT64_C(4)));
  v31 = ((int64_t)((*t328)));
  t329 = ((int64_t)(v31));
  t330 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t329) > (t330)))) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t331 = v30;
  t332 = ((int64_t *)(v12).data) + gg_index(t331, ((int64_t)UINT64_C(4)));
  t333 = ((int64_t)((*t332)));
  /* FG_DIRECT_SLICE_INDEX */
  t334 = v30;
  t335 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t334, ((int64_t)UINT64_C(16)));
  t336 = ((int64_t)((*t335)));
  t337 = ((int64_t)(((int64_t)((uint64_t)(t333) - (uint64_t)(t336)))));
  t338 = ((int64_t)(v31));
  t339 = ((int64_t)(((int64_t)gg_mod(t337, t338))));
  t340 = ((int64_t)(v31));
  t341 = ((int64_t)(((int64_t)((uint64_t)(t339) + (uint64_t)(t340)))));
  t342 = ((int64_t)(v31));
  v32 = ((int64_t)(((int64_t)gg_mod(t341, t342))));
  t343 = ((int64_t)(v29));
  t344 = ((int64_t)(v32));
  /* FG_DIRECT_SLICE_INDEX */
  t345 = v30;
  t346 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t345, ((int64_t)UINT64_C(4)));
  t347 = ((int64_t)((*t346)));
  t348 = gg_function_2(t344, t347, base + 0);
  v29 = gg_function_5(t343, t348, base + 0);
  }
label22:;
  t325++; goto label21;
label23:;
  t349 = &(((*(v0))).f4);
  t350 = v1;
  t351 = ((int64_t)(v29));
  v14 = gg_function_47(t349, t350, t351, base + 0);
  }
  } else {
  t352 = ((int64_t)(v4));
  t353 = ((int64_t)(((int64_t)UINT64_C(43))));
  t354 = ((t352) == (t353));
  if (t354) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t355 = ((int64_t)UINT64_C(0));
  t356 = ((int64_t *)(v12).data) + gg_index(t355, ((int64_t)UINT64_C(4)));
  t357 = ((int64_t)((*t356)));
  /* FG_DIRECT_SLICE_INDEX */
  t358 = ((int64_t)UINT64_C(1));
  t359 = ((int64_t *)(v12).data) + gg_index(t358, ((int64_t)UINT64_C(4)));
  t360 = ((int64_t)((*t359)));
  t361 = ((t357) == (t360));
  if (t361) {
  {
  t362 = &(((*(v0))).f4);
  t363 = v1;
  /* FG_DIRECT_SLICE_INDEX */
  t364 = ((int64_t)UINT64_C(0));
  t365 = ((int64_t *)(v12).data) + gg_index(t364, ((int64_t)UINT64_C(4)));
  t366 = ((int64_t)((*t365)));
  /* FG_DIRECT_SLICE_INDEX */
  t367 = ((int64_t)UINT64_C(0));
  t368 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t367, ((int64_t)UINT64_C(4)));
  t369 = ((int64_t)((*t368)));
  /* FG_DIRECT_SLICE_INDEX */
  t370 = ((int64_t)UINT64_C(2));
  t371 = ((int64_t *)(v12).data) + gg_index(t370, ((int64_t)UINT64_C(4)));
  t372 = ((int64_t)((*t371)));
  /* FG_DIRECT_SLICE_INDEX */
  t373 = ((int64_t)UINT64_C(2));
  t374 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t373, ((int64_t)UINT64_C(4)));
  t375 = ((int64_t)((*t374)));
  /* FG_DIRECT_SLICE_INDEX */
  t376 = ((int64_t)UINT64_C(3));
  t377 = ((int64_t *)(v12).data) + gg_index(t376, ((int64_t)UINT64_C(4)));
  t378 = ((int64_t)((*t377)));
  t379 = ((int64_t)(((int64_t)((uint64_t)(t375) * (uint64_t)(t378)))));
  t380 = ((int64_t)(((int64_t)((uint64_t)(t372) + (uint64_t)(t379)))));
  t381 = ((int64_t)(((int64_t)((uint64_t)(t369) * (uint64_t)(t380)))));
  t382 = ((int64_t)(((int64_t)((uint64_t)(t366) + (uint64_t)(t381)))));
  v14 = gg_function_51(t362, t363, t382, base + 0);
  }
  }
  }
  } else {
  t383 = ((int64_t)(v4));
  t384 = ((int64_t)(((int64_t)UINT64_C(44))));
  t385 = ((t383) == (t384));
  t386 = t385;
  if (!t386) {
  t387 = ((int64_t)(v4));
  t388 = ((int64_t)(((int64_t)UINT64_C(45))));
  t386 = ((t387) == (t388));
  }
  t389 = t386;
  if (t389) {
  {
  t390 = &(((*(v0))).f4);
  t391 = v1;
  t392 = ((int64_t)(v5));
  v14 = gg_function_51(t390, t391, t392, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t393 = ((int64_t)UINT64_C(0));
  t394 = ((int64_t *)(v12).data) + gg_index(t393, ((int64_t)UINT64_C(4)));
  t395 = ((int64_t)((*t394)));
  /* FG_DIRECT_SLICE_INDEX */
  t396 = ((int64_t)UINT64_C(0));
  t397 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t396, ((int64_t)UINT64_C(16)));
  t398 = ((int64_t)((*t397)));
  /* FG_DIRECT_SLICE_INDEX */
  t399 = ((int64_t)UINT64_C(1));
  t400 = ((int64_t *)(v12).data) + gg_index(t399, ((int64_t)UINT64_C(4)));
  t401 = ((int64_t)((*t400)));
  t402 = ((int64_t)(((int64_t)((uint64_t)(t398) + (uint64_t)(t401)))));
  t403 = ((t395) > (t402));
  if (t403) {
  {
  t404 = ((int64_t)(v4));
  t405 = ((int64_t)(((int64_t)UINT64_C(44))));
  t406 = ((t404) == (t405));
  if (t406) {
  {
  t407 = ((uint32_t)(((int64_t)UINT64_C(4286578688))));
  t408 = fg143_from_bits(t407);
  v14 = ((float)(t408));
  }
  } else {
  {
  v14 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  }
  }
  }
  }
  } else {
  t409 = ((int64_t)(v4));
  t410 = ((int64_t)(((int64_t)UINT64_C(40))));
  t411 = ((t409) == (t410));
  if (t411) {
  {
  t412 = ((int64_t)(v5));
  /* FG_DIRECT_SLICE_INDEX */
  t413 = ((int64_t)UINT64_C(0));
  t414 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t413, ((int64_t)UINT64_C(4)));
  t415 = ((int64_t)((*t414)));
  v33 = ((int64_t)(((int64_t)gg_div(t412, t415))));
  t416 = v2;
  t417 = &(((*(v0))).f5);
  t418 = ((int64_t)(v33));
  t419 = gg_function_16(t417, t418, base + 0);
  t420 = gg_function_17(t416, t419, base + 0);
  v34 = ((int32_t)(((int32_t)(t420))));
  t421 = ((int32_t)(v34));
  t422 = ((int32_t)(((int64_t)UINT64_C(0))));
  t423 = ((t421) >= (t422));
  t424 = t423;
  if (t424) {
  t425 = ((int64_t)(((int64_t)(v34))));
  /* FG_DIRECT_SLICE_INDEX */
  t426 = ((int64_t)UINT64_C(1));
  t427 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t426, ((int64_t)UINT64_C(4)));
  t428 = ((int64_t)((*t427)));
  t424 = ((t425) < (t428));
  }
  if (!(t424)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t429 = ((int64_t)UINT64_C(0));
  t430 = ((int64_t *)(v12).data) + gg_index(t429, ((int64_t)UINT64_C(4)));
  t431 = ((int64_t)((*t430)));
  /* FG_DIRECT_SLICE_INDEX */
  t432 = ((int64_t)UINT64_C(0));
  t433 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t432, ((int64_t)UINT64_C(4)));
  t434 = ((int64_t)((*t433)));
  t435 = ((int64_t)(((int64_t)((uint64_t)(t431) * (uint64_t)(t434)))));
  t436 = ((int64_t)(((int64_t)(v34))));
  /* FG_DIRECT_SLICE_INDEX */
  t437 = ((int64_t)UINT64_C(1));
  t438 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t437, ((int64_t)UINT64_C(4)));
  t439 = ((int64_t)((*t438)));
  t440 = ((int64_t)(((int64_t)((uint64_t)(t436) * (uint64_t)(t439)))));
  t441 = ((int64_t)(((int64_t)((uint64_t)(t435) + (uint64_t)(t440)))));
  /* FG_DIRECT_SLICE_INDEX */
  t442 = ((int64_t)UINT64_C(2));
  t443 = ((int64_t *)(v12).data) + gg_index(t442, ((int64_t)UINT64_C(4)));
  t444 = ((int64_t)((*t443)));
  /* FG_DIRECT_SLICE_INDEX */
  t445 = ((int64_t)UINT64_C(2));
  t446 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t445, ((int64_t)UINT64_C(4)));
  t447 = ((int64_t)((*t446)));
  t448 = ((int64_t)(((int64_t)((uint64_t)(t444) * (uint64_t)(t447)))));
  t449 = ((int64_t)(((int64_t)((uint64_t)(t441) + (uint64_t)(t448)))));
  /* FG_DIRECT_SLICE_INDEX */
  t450 = ((int64_t)UINT64_C(3));
  t451 = ((int64_t *)(v12).data) + gg_index(t450, ((int64_t)UINT64_C(4)));
  t452 = ((int64_t)((*t451)));
  /* FG_DIRECT_SLICE_INDEX */
  t453 = ((int64_t)UINT64_C(3));
  t454 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t453, ((int64_t)UINT64_C(4)));
  t455 = ((int64_t)((*t454)));
  t456 = ((int64_t)(((int64_t)((uint64_t)(t452) * (uint64_t)(t455)))));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t449) + (uint64_t)(t456)))));
  t457 = &(((*(v0))).f4);
  t458 = v1;
  t459 = ((int64_t)(v35));
  v14 = gg_function_47(t457, t458, t459, base + 0);
  }
  } else {
  t460 = ((int64_t)(v4));
  t461 = ((int64_t)(((int64_t)UINT64_C(42))));
  t462 = ((t460) == (t461));
  if (t462) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  } else {
  {
  t463 = ((int64_t)(v6));
  t464 = &(((*(v0))).f4);
  t465 = gg_function_12(t464, base + 0);
  t466 = ((int64_t)(t465));
  if (!(((t463) == (t466)))) gg_fail("assertion failed");
  t467 = &(((*(v0))).f4);
  t468 = v1;
  t469 = ((int64_t)(v5));
  v14 = gg_function_51(t467, t468, t469, base + 0);
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
  t470 = &(((*(v0))).f7);
  t471 = v3;
  t472 = ((int64_t)(v15));
  t473 = ((float)(v14));
  gg_function_37(t470, t471, t472, t473, base + 0);
  t474 = v5;
  t475 = ((int64_t)(((*(v0))).f10));
  v5 = ((int64_t)((uint64_t)(t474) + (uint64_t)(t475)));
  }
  goto label7;
label8:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_11(gg_type_31 p0, int64_t p1, int64_t base) {
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
  int64_t * t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t * t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t * t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t * t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  gg_type_31 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint8_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(((*(v0))).f3));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(((*(v0))).f3));
  t5 = ((int64_t)(v1));
  t3 = ((t4) <= (t5));
  }
  if (!(t3)) gg_fail("assertion failed");
  t6 = ((int64_t)(((*(v0))).f2));
  v2 = gg_function_4(t6, base + 0);
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t7) > (t8)))) gg_fail("assertion failed");
  v3 = ((int64_t)(v2));
  v4 = ((uint8_t)UINT64_C(0));
  t10 = ((int64_t)UINT64_C(4));
  t9 = 0;
label0:;
  if (t9 >= t10) goto label2;
  v5 = t9;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v5;
  t12 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t11, ((int64_t)UINT64_C(4)));
  t13 = ((int64_t)((*t12)));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) >= (t14));
  t16 = t15;
  if (t16) {
  /* FG_DIRECT_SLICE_INDEX */
  t17 = v5;
  t18 = ((int64_t *)(((*(v0))).f1).data) + gg_index(t17, ((int64_t)UINT64_C(4)));
  t19 = ((int64_t)((*t18)));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t19) >= (t20));
  }
  if (!(t16)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t21 = v5;
  t22 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t21, ((int64_t)UINT64_C(4)));
  t23 = ((int64_t)((*t22)));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) == (t24));
  if (t25) {
  {
  v4 = ((uint8_t)UINT64_C(1));
  }
  } else {
  {
  t26 = ((int64_t)(v3));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = v5;
  t28 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t27, ((int64_t)UINT64_C(4)));
  t29 = ((int64_t)((*t28)));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) - (uint64_t)(t30)))));
  /* FG_DIRECT_SLICE_INDEX */
  t32 = v5;
  t33 = ((int64_t *)(((*(v0))).f1).data) + gg_index(t32, ((int64_t)UINT64_C(4)));
  t34 = ((int64_t)((*t33)));
  t35 = gg_function_2(t31, t34, base + 0);
  v3 = gg_function_5(t26, t35, base + 0);
  }
  }
  }
label1:;
  t9++; goto label0;
label2:;
  t36 = v4;
  t37 = t36;
  if (!t37) {
  t38 = ((int64_t)(v3));
  t39 = ((int64_t)(((*(v0))).f3));
  t37 = ((t38) <= (t39));
  }
  if (!(t37)) gg_fail("assertion failed");
  }
gg_done:;
}
static uint8_t gg_function_3(gg_type_31 p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  gg_type_31 v0;
  v0 = p0;
  {
  t1 = ((int64_t)(((*(v0))).f2));
  t2 = gg_function_4(t1, base + 0);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = ((t3) > (t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_35(gg_type_45 p0, int64_t p1, int64_t p2, uint8_t p3, int64_t base) {
  int64_t t0;
  gg_type_40 * t1;
  gg_slice t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_type_39 * t6;
  gg_slice t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t * t11;
  gg_type_45 v0;
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
  t1 = ((gg_type_40 *)(((*(v0))).f18).data) + gg_index(t0, ((int64_t)UINT64_C(256)));
  t2 = gg_view(((*t1)).data, 8);
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(32))));
  t5 = ((int64_t)gg_div(t3, t4));
  t6 = ((gg_type_39 *)t2.data) + gg_index(t5, t2.len);
  t7 = gg_view(((*t6)).data, 32);
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(32))));
  t10 = ((int64_t)gg_mod(t8, t9));
  t11 = ((uint8_t *)t7.data) + gg_index(t10, t7.len);
  (*t11) = ((uint8_t)(v3));
  }
gg_done:;
}
static void gg_function_36(gg_type_45 p0, int64_t p1, int64_t p2, gg_type_16 p3, int16_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  gg_type_45 t2;
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
  gg_type_45 v0;
  int64_t v1;
  int64_t v2;
  gg_type_16 v3;
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
  gg_function_35(t2, t3, t6, t15, base + 0);
  }
label1:;
  t0++; goto label0;
label2:;
  }
gg_done:;
}
static void gg_function_29(gg_type_45 p0, int64_t p1, int32_t p2, int64_t base) {
  gg_slice t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  gg_type_43 * t4;
  gg_slice t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t * t9;
  gg_type_45 v0;
  int64_t v1;
  int32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = gg_view((((*(v0))).f21).data, 512);
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)gg_div(t1, t2));
  t4 = ((gg_type_43 *)t0.data) + gg_index(t3, t0.len);
  t5 = gg_view(((*t4)).data, 8);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)gg_mod(t6, t7));
  t9 = ((int32_t *)t5.data) + gg_index(t8, t5.len);
  (*t9) = ((int32_t)(v2));
  }
gg_done:;
}
static uint16_t gg_function_21(gg_slice p0, int64_t p1, int64_t base) {
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
static void gg_function_34(gg_type_45 p0, int64_t p1, int64_t p2, gg_type_12 p3, uint8_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  uint8_t t4;
  uint8_t t5;
  uint8_t t6;
  uint8_t t7;
  gg_type_45 t8;
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
  gg_type_45 v0;
  int64_t v1;
  int64_t v2;
  gg_type_12 v3;
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
  gg_function_35(t8, t9, t12, t21, base + 0);
  }
label1:;
  t0++; goto label0;
label2:;
  }
gg_done:;
}
static uint32_t gg_function_20(gg_slice p0, int64_t p1, int64_t base) {
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
  t3 = gg_function_21(t1, t2, base + 0);
  t4 = ((uint32_t)(((uint32_t)(t3))));
  t5 = v0;
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t9 = gg_function_21(t5, t8, base + 0);
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
static gg_type_39 gg_function_33(gg_type_39 p511, gg_type_39 p512, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_39 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)p511.data), _mm256_loadu_si256((const __m256i*)p512.data)));
    return packed_result;
  }
#endif
  gg_type_39 t0;
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
  gg_type_39 v511;
  gg_type_39 v512;
  gg_type_39 v513;
  int64_t v515;
  int64_t v516;
  v511 = p511;
  v512 = p512;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v513).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v515 = t4;
  {
  t6 = ((int64_t)(v515));
  t7 = ((int64_t)(((int64_t)UINT64_C(16))));
  t8 = ((int64_t)(((int64_t)gg_div(t6, t7))));
  t9 = ((int64_t)(((int64_t)UINT64_C(16))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t11 = gg_view((v512).data, 32);
  t12 = v515;
  t13 = ((uint8_t *)t11.data) + gg_index(t12, t11.len);
  t14 = ((uint8_t)((*t13)));
  t15 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t16 = ((int64_t)(((int64_t)(((t14) & (t15))))));
  v516 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t16)))));
  t17 = gg_view((v513).data, 32);
  t18 = v515;
  t19 = ((uint8_t *)t17.data) + gg_index(t18, t17.len);
  t20 = gg_view((v512).data, 32);
  t21 = v515;
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
  t28 = gg_view((v511).data, 32);
  t29 = v516;
  t30 = ((uint8_t *)t28.data) + gg_index(t29, t28.len);
  (*t19) = ((uint8_t)((*t30)));
  }
  }
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v513;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:shr8 */
static gg_type_39 gg_function_32(gg_type_39 p430, uint8_t p431, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_39 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)p430.data), _mm_cvtsi32_si128((p431 & 7))), _mm256_set1_epi8((unsigned char)(255u >> (p431 & 7)))));
    return packed_result;
  }
#endif
  gg_type_39 t0;
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
  gg_type_39 v430;
  uint8_t v431;
  gg_type_39 v432;
  int64_t v434;
  v430 = p430;
  v431 = p431;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v432).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v434 = t4;
  {
  t6 = gg_view((v432).data, 32);
  t7 = v434;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v430).data, 32);
  t10 = v434;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v431));
  (*t8) = ((uint8_t)(((t12) >> ((uint64_t)(t13) & 7))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v432;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:and8 */
static gg_type_39 gg_function_31(gg_type_39 p422, uint8_t p423, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_39 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_loadu_si256((const __m256i*)p422.data), _mm256_set1_epi8(p423)));
    return packed_result;
  }
#endif
  gg_type_39 t0;
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
  gg_type_39 v422;
  uint8_t v423;
  gg_type_39 v424;
  int64_t v426;
  v422 = p422;
  v423 = p423;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v424).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v426 = t4;
  {
  t6 = gg_view((v424).data, 32);
  t7 = v426;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v422).data, 32);
  t10 = v426;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v423));
  (*t8) = ((uint8_t)(((t12) & (t13))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v424;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_39 gg_function_30(gg_slice p0, int64_t p1, int64_t base) {
  gg_type_39 t0;
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
  gg_type_39 v3;
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
static void gg_function_28(gg_type_45 p0, int64_t p1, int32_t p2, int64_t base) {
  gg_slice t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  gg_type_43 * t4;
  gg_slice t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t * t9;
  gg_type_45 v0;
  int64_t v1;
  int32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = gg_view((((*(v0))).f22).data, 512);
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)gg_div(t1, t2));
  t4 = ((gg_type_43 *)t0.data) + gg_index(t3, t0.len);
  t5 = gg_view(((*t4)).data, 8);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)gg_mod(t6, t7));
  t9 = ((int32_t *)t5.data) + gg_index(t8, t5.len);
  (*t9) = ((int32_t)(v2));
  }
gg_done:;
}
static gg_type_32 gg_function_9(int64_t p0, int64_t base) {
  gg_type_32 t0;
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
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(144))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(1))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(176))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(1))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(210))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(3))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(110))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(4))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(84))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(1))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(5))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(136))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(6))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(66))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(7))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(74))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(8))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(82))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(9))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(98))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(10))));
  t39 = ((t37) == (t38));
  if (t39) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(110))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(11))));
  t42 = ((t40) == (t41));
  if (t42) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(50))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(56))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f16 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f17 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_27(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, gg_type_45 p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  float * t6;
  int64_t t7;
  int64_t t8;
  gg_type_45 t9;
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
  gg_type_45 t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  uint8_t t63;
  int32_t t64;
  gg_type_45 t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  uint8_t t70;
  int32_t t71;
  gg_type_45 t72;
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
  gg_type_45 t87;
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
  gg_type_39 t116;
  uint8_t t117;
  gg_type_39 t118;
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
  gg_type_40 * t164;
  gg_slice t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  gg_type_39 * t169;
  int64_t t170;
  gg_type_40 * t171;
  gg_slice t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  gg_type_39 * t178;
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
  gg_type_45 t192;
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
  gg_type_40 * t240;
  gg_slice t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  gg_type_39 * t245;
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
  gg_type_40 * t258;
  gg_slice t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  gg_type_39 * t265;
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
  gg_type_40 * t278;
  gg_slice t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  gg_type_39 * t285;
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
  gg_type_40 * t296;
  gg_slice t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  gg_type_39 * t303;
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
  gg_type_45 t349;
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
  gg_type_45 t364;
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
  gg_type_45 t383;
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
  gg_type_45 t402;
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
  gg_type_40 * t465;
  gg_slice t466;
  int64_t t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
  gg_type_39 * t472;
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
  gg_type_45 t494;
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
  gg_type_45 t506;
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
  gg_type_40 * t531;
  gg_slice t532;
  int64_t t533;
  int64_t t534;
  int64_t t535;
  int64_t t536;
  int64_t t537;
  gg_type_39 * t538;
  gg_type_39 t539;
  int64_t t540;
  int64_t t541;
  uint8_t t542;
  gg_type_39 t543;
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
  gg_type_45 t577;
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
  gg_type_39 t620;
  gg_type_39 t621;
  uint8_t t622;
  gg_type_39 t623;
  gg_type_39 t624;
  gg_type_39 t625;
  uint8_t t626;
  gg_type_39 t627;
  int64_t t628;
  int64_t t629;
  int64_t t630;
  gg_type_40 * t631;
  gg_slice t632;
  int64_t t633;
  int64_t t634;
  int64_t t635;
  gg_type_39 * t636;
  gg_slice t637;
  int64_t t638;
  uint8_t * t639;
  int64_t t640;
  uint8_t * t641;
  int64_t t642;
  gg_type_40 * t643;
  gg_slice t644;
  int64_t t645;
  int64_t t646;
  int64_t t647;
  gg_type_39 * t648;
  gg_slice t649;
  int64_t t650;
  int64_t t651;
  int64_t t652;
  uint8_t * t653;
  int64_t t654;
  uint8_t * t655;
  int64_t t656;
  gg_type_40 * t657;
  gg_slice t658;
  int64_t t659;
  int64_t t660;
  int64_t t661;
  int64_t t662;
  int64_t t663;
  gg_type_39 * t664;
  gg_slice t665;
  int64_t t666;
  uint8_t * t667;
  gg_slice t668;
  int64_t t669;
  int64_t t670;
  int64_t t671;
  uint8_t * t672;
  int64_t t673;
  gg_type_40 * t674;
  gg_slice t675;
  int64_t t676;
  int64_t t677;
  int64_t t678;
  int64_t t679;
  int64_t t680;
  gg_type_39 * t681;
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
  gg_type_45 t715;
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
  gg_type_45 t728;
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
  gg_type_12 * t750;
  gg_type_12 t751;
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
  gg_type_45 t785;
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
  gg_type_45 t803;
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
  gg_type_45 t840;
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
  gg_type_12 * t853;
  gg_type_12 t854;
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
  gg_type_45 t889;
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
  gg_type_12 * t900;
  gg_type_12 t901;
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
  gg_type_45 t935;
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
  gg_type_9 * t963;
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
  gg_type_9 * t981;
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
  gg_type_45 t998;
  int64_t t999;
  int64_t t1000;
  int64_t t1001;
  int64_t t1002;
  int64_t t1003;
  int64_t t1004;
  int64_t t1005;
  int64_t t1006;
  gg_type_12 t1007;
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
  gg_type_45 t1038;
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
  gg_type_9 * t1114;
  gg_slice t1115;
  int64_t t1116;
  gg_type_9 * t1117;
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
  gg_type_45 t1134;
  int64_t t1135;
  int64_t t1136;
  int64_t t1137;
  int64_t t1138;
  int64_t t1139;
  int64_t t1140;
  int64_t t1141;
  int64_t t1142;
  gg_type_12 t1143;
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
  gg_type_45 t1177;
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
  gg_type_45 t1219;
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
  gg_type_16 * t1230;
  gg_type_16 t1231;
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
  gg_type_45 t1291;
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
  gg_type_45 t1306;
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
  gg_type_45 t1375;
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
  gg_type_16 * t1386;
  gg_type_16 t1387;
  int16_t t1388;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  gg_type_45 v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  gg_slice v10;
  gg_slice v11;
  gg_slice v12;
  gg_slice v13;
  gg_type_32 v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  uint8_t v19;
  uint8_t v20;
  uint8_t v21;
  int64_t v22;
  int64_t v23;
  gg_type_39 v24;
  gg_type_39 v25;
  gg_type_39 v26;
  gg_type_39 v27;
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
  gg_type_39 v41;
  gg_type_39 v42;
  gg_type_39 v43;
  int64_t v44;
  int64_t v45;
  int64_t v46;
  gg_type_39 v47;
  int64_t v48;
  uint16_t v49;
  int64_t v50;
  uint16_t v51;
  gg_type_39 v52;
  int64_t v53;
  uint8_t v54;
  int64_t v55;
  gg_type_39 v56;
  gg_type_39 v57;
  gg_type_39 v58;
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
  gg_type_9 v71;
  gg_type_9 v72;
  gg_type_12 v73;
  int64_t v74;
  int64_t v75;
  uint8_t v76;
  int64_t v77;
  uint16_t v78;
  uint16_t v79;
  uint16_t v80;
  gg_type_9 v81;
  gg_type_9 v82;
  gg_type_12 v83;
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
  v14 = gg_function_9(t0, base + 0);
  t1 = ((int64_t)(((int64_t)UINT64_C(256))));
  t2 = ((int64_t)((v14).f14));
  v15 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v15));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  /* FG_DIRECT_SLICE_INDEX */
  t5 = v4;
  t6 = ((float *)(((*(v5))).f20).data) + gg_index(t5, ((int64_t)UINT64_C(256)));
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
  gg_function_28(t9, t12, t13, base + 0);
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
  t22 = ((float *)(((*(v5))).f19).data) + gg_index(t21, ((int64_t)UINT64_C(256)));
  t23 = v1;
  t24 = ((int64_t)(v2));
  t25 = gg_function_21(t23, t24, base + 0);
  t26 = fg141_half(t25);
  (*t22) = ((float)(t26));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = v4;
  t28 = ((float *)(((*(v5))).f20).data) + gg_index(t27, ((int64_t)UINT64_C(256)));
  t29 = v1;
  t30 = ((int64_t)(v2));
  t31 = ((int64_t)(((int64_t)UINT64_C(2))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  t33 = gg_function_21(t29, t32, base + 0);
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
  gg_function_29(t58, t61, t64, base + 0);
  t65 = v5;
  t66 = ((int64_t)(v16));
  t67 = ((int64_t)(v18));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t69 = ((uint8_t)(v20));
  t70 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t71 = ((int32_t)(((int32_t)(((t69) & (t70))))));
  gg_function_28(t65, t68, t71, base + 0);
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
  gg_function_29(t72, t77, t86, base + 0);
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
  gg_function_28(t87, t92, t101, base + 0);
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
  v24 = gg_function_30(t110, t115, base + 0);
  t116 = v24;
  t117 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v25 = gg_function_31(t116, t117, base + 0);
  t118 = v24;
  t119 = ((uint8_t)(((int64_t)UINT64_C(4))));
  v26 = gg_function_32(t118, t119, base + 0);
  t120 = ((int64_t)(v0));
  t121 = ((int64_t)(((int64_t)UINT64_C(1))));
  t122 = ((t120) == (t121));
  if (t122) {
  {
  t123 = v1;
  t124 = ((int64_t)(v2));
  t125 = ((int64_t)(((int64_t)UINT64_C(16))));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t124) + (uint64_t)(t125)))));
  v27 = gg_function_30(t123, t126, base + 0);
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
  t164 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t163, ((int64_t)UINT64_C(256)));
  t165 = gg_view(((*t164)).data, 8);
  t166 = ((int64_t)(((int64_t)UINT64_C(2))));
  t167 = ((int64_t)(v23));
  t168 = ((int64_t)((uint64_t)(t166) * (uint64_t)(t167)));
  t169 = ((gg_type_39 *)t165.data) + gg_index(t168, t165.len);
  (*t169) = v25;
  /* FG_DIRECT_SLICE_INDEX */
  t170 = v3;
  t171 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t170, ((int64_t)UINT64_C(256)));
  t172 = gg_view(((*t171)).data, 8);
  t173 = ((int64_t)(((int64_t)UINT64_C(2))));
  t174 = ((int64_t)(v23));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) * (uint64_t)(t174)))));
  t176 = ((int64_t)(((int64_t)UINT64_C(1))));
  t177 = ((int64_t)((uint64_t)(t175) + (uint64_t)(t176)));
  t178 = ((gg_type_39 *)t172.data) + gg_index(t177, t172.len);
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
  t183 = ((float *)(((*(v5))).f19).data) + gg_index(t182, ((int64_t)UINT64_C(256)));
  t184 = v1;
  t185 = ((int64_t)(v2));
  t186 = ((int64_t)(((int64_t)UINT64_C(208))));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t185) + (uint64_t)(t186)))));
  t188 = gg_function_21(t184, t187, base + 0);
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
  gg_function_29(t192, t195, t203, base + 0);
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
  t240 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t239, ((int64_t)UINT64_C(256)));
  t241 = gg_view(((*t240)).data, 8);
  t242 = ((int64_t)(v30));
  t243 = ((int64_t)(((int64_t)UINT64_C(4))));
  t244 = ((int64_t)((uint64_t)(t242) * (uint64_t)(t243)));
  t245 = ((gg_type_39 *)t241.data) + gg_index(t244, t241.len);
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
  t258 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t257, ((int64_t)UINT64_C(256)));
  t259 = gg_view(((*t258)).data, 8);
  t260 = ((int64_t)(v30));
  t261 = ((int64_t)(((int64_t)UINT64_C(4))));
  t262 = ((int64_t)(((int64_t)((uint64_t)(t260) * (uint64_t)(t261)))));
  t263 = ((int64_t)(((int64_t)UINT64_C(1))));
  t264 = ((int64_t)((uint64_t)(t262) + (uint64_t)(t263)));
  t265 = ((gg_type_39 *)t259.data) + gg_index(t264, t259.len);
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
  t278 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t277, ((int64_t)UINT64_C(256)));
  t279 = gg_view(((*t278)).data, 8);
  t280 = ((int64_t)(v30));
  t281 = ((int64_t)(((int64_t)UINT64_C(4))));
  t282 = ((int64_t)(((int64_t)((uint64_t)(t280) * (uint64_t)(t281)))));
  t283 = ((int64_t)(((int64_t)UINT64_C(2))));
  t284 = ((int64_t)((uint64_t)(t282) + (uint64_t)(t283)));
  t285 = ((gg_type_39 *)t279.data) + gg_index(t284, t279.len);
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
  t296 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t295, ((int64_t)UINT64_C(256)));
  t297 = gg_view(((*t296)).data, 8);
  t298 = ((int64_t)(v30));
  t299 = ((int64_t)(((int64_t)UINT64_C(4))));
  t300 = ((int64_t)(((int64_t)((uint64_t)(t298) * (uint64_t)(t299)))));
  t301 = ((int64_t)(((int64_t)UINT64_C(3))));
  t302 = ((int64_t)((uint64_t)(t300) + (uint64_t)(t301)));
  t303 = ((gg_type_39 *)t297.data) + gg_index(t302, t297.len);
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
  t319 = ((float *)(((*(v5))).f19).data) + gg_index(t318, ((int64_t)UINT64_C(256)));
  t320 = v1;
  t321 = ((int64_t)(v2));
  t322 = ((int64_t)(((int64_t)UINT64_C(108))));
  t323 = ((int64_t)(((int64_t)((uint64_t)(t321) + (uint64_t)(t322)))));
  t324 = gg_function_21(t320, t323, base + 0);
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
  gg_function_29(t349, t352, t363, base + 0);
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
  gg_function_29(t364, t369, t382, base + 0);
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
  gg_function_29(t383, t388, t401, base + 0);
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
  gg_function_29(t402, t407, t420, base + 0);
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
  v41 = gg_function_30(t425, t432, base + 0);
  t433 = v1;
  t434 = ((int64_t)(v2));
  v42 = gg_function_30(t433, t434, base + 0);
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
  t465 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t464, ((int64_t)UINT64_C(256)));
  t466 = gg_view(((*t465)).data, 8);
  t467 = ((int64_t)(v39));
  t468 = ((int64_t)(((int64_t)UINT64_C(4))));
  t469 = ((int64_t)(((int64_t)((uint64_t)(t467) * (uint64_t)(t468)))));
  t470 = ((int64_t)(v40));
  t471 = ((int64_t)((uint64_t)(t469) + (uint64_t)(t470)));
  t472 = ((gg_type_39 *)t466.data) + gg_index(t471, t466.len);
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
  t477 = ((float *)(((*(v5))).f19).data) + gg_index(t476, ((int64_t)UINT64_C(256)));
  t478 = v1;
  t479 = ((int64_t)(v2));
  t480 = ((int64_t)(((int64_t)UINT64_C(80))));
  t481 = ((int64_t)(((int64_t)((uint64_t)(t479) + (uint64_t)(t480)))));
  t482 = gg_function_21(t478, t481, base + 0);
  t483 = fg141_half(t482);
  (*t477) = ((float)(t483));
  /* FG_DIRECT_SLICE_INDEX */
  t484 = v4;
  t485 = ((float *)(((*(v5))).f20).data) + gg_index(t484, ((int64_t)UINT64_C(256)));
  t486 = v1;
  t487 = ((int64_t)(v2));
  t488 = ((int64_t)(((int64_t)UINT64_C(82))));
  t489 = ((int64_t)(((int64_t)((uint64_t)(t487) + (uint64_t)(t488)))));
  t490 = gg_function_21(t486, t489, base + 0);
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
  gg_function_29(t494, t497, t505, base + 0);
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
  gg_function_28(t506, t509, t517, base + 0);
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
  v47 = gg_function_30(t520, t527, base + 0);
  t529 = ((int64_t)UINT64_C(4));
  t528 = 0;
label39:;
  if (t528 >= t529) goto label41;
  v48 = t528;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t530 = v3;
  t531 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t530, ((int64_t)UINT64_C(256)));
  t532 = gg_view(((*t531)).data, 8);
  t533 = ((int64_t)(v46));
  t534 = ((int64_t)(((int64_t)UINT64_C(4))));
  t535 = ((int64_t)(((int64_t)((uint64_t)(t533) * (uint64_t)(t534)))));
  t536 = ((int64_t)(v48));
  t537 = ((int64_t)((uint64_t)(t535) + (uint64_t)(t536)));
  t538 = ((gg_type_39 *)t532.data) + gg_index(t537, t532.len);
  t539 = v47;
  t540 = ((int64_t)(((int64_t)UINT64_C(2))));
  t541 = ((int64_t)(v48));
  t542 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t540) * (uint64_t)(t541)))))));
  t543 = gg_function_32(t539, t542, base + 0);
  t544 = ((uint8_t)(((int64_t)UINT64_C(3))));
  (*t538) = gg_function_31(t543, t544, base + 0);
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
  t549 = ((float *)(((*(v5))).f19).data) + gg_index(t548, ((int64_t)UINT64_C(256)));
  t550 = v1;
  t551 = ((int64_t)(v2));
  t552 = gg_function_21(t550, t551, base + 0);
  t553 = fg141_half(t552);
  (*t549) = ((float)(t553));
  t554 = v1;
  t555 = ((int64_t)(v2));
  t556 = ((int64_t)(((int64_t)UINT64_C(2))));
  t557 = ((int64_t)(((int64_t)((uint64_t)(t555) + (uint64_t)(t556)))));
  v49 = gg_function_21(t554, t557, base + 0);
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
  gg_function_29(t577, t580, t593, base + 0);
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
  v56 = gg_function_30(t612, t619, base + 0);
  t620 = v52;
  t621 = v56;
  t622 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t623 = gg_function_31(t621, t622, base + 0);
  v57 = gg_function_33(t620, t623, base + 0);
  t624 = v52;
  t625 = v56;
  t626 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t627 = gg_function_32(t625, t626, base + 0);
  v58 = gg_function_33(t624, t627, base + 0);
  t629 = ((int64_t)UINT64_C(16));
  t628 = 0;
label51:;
  if (t628 >= t629) goto label53;
  v59 = t628;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t630 = v3;
  t631 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t630, ((int64_t)UINT64_C(256)));
  t632 = gg_view(((*t631)).data, 8);
  t633 = ((int64_t)(((int64_t)UINT64_C(2))));
  t634 = ((int64_t)(v55));
  t635 = ((int64_t)((uint64_t)(t633) * (uint64_t)(t634)));
  t636 = ((gg_type_39 *)t632.data) + gg_index(t635, t632.len);
  t637 = gg_view(((*t636)).data, 32);
  t638 = v59;
  t639 = ((uint8_t *)t637.data) + gg_index(t638, t637.len);
  /* FG_DIRECT_SLICE_INDEX */
  t640 = v59;
  t641 = ((uint8_t *)(v57).data) + gg_index(t640, ((int64_t)UINT64_C(32)));
  (*t639) = ((uint8_t)((*t641)));
  /* FG_DIRECT_SLICE_INDEX */
  t642 = v3;
  t643 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t642, ((int64_t)UINT64_C(256)));
  t644 = gg_view(((*t643)).data, 8);
  t645 = ((int64_t)(((int64_t)UINT64_C(2))));
  t646 = ((int64_t)(v55));
  t647 = ((int64_t)((uint64_t)(t645) * (uint64_t)(t646)));
  t648 = ((gg_type_39 *)t644.data) + gg_index(t647, t644.len);
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
  t657 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t656, ((int64_t)UINT64_C(256)));
  t658 = gg_view(((*t657)).data, 8);
  t659 = ((int64_t)(((int64_t)UINT64_C(2))));
  t660 = ((int64_t)(v55));
  t661 = ((int64_t)(((int64_t)((uint64_t)(t659) * (uint64_t)(t660)))));
  t662 = ((int64_t)(((int64_t)UINT64_C(1))));
  t663 = ((int64_t)((uint64_t)(t661) + (uint64_t)(t662)));
  t664 = ((gg_type_39 *)t658.data) + gg_index(t663, t658.len);
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
  t674 = ((gg_type_40 *)(((*(v5))).f18).data) + gg_index(t673, ((int64_t)UINT64_C(256)));
  t675 = gg_view(((*t674)).data, 8);
  t676 = ((int64_t)(((int64_t)UINT64_C(2))));
  t677 = ((int64_t)(v55));
  t678 = ((int64_t)(((int64_t)((uint64_t)(t676) * (uint64_t)(t677)))));
  t679 = ((int64_t)(((int64_t)UINT64_C(1))));
  t680 = ((int64_t)((uint64_t)(t678) + (uint64_t)(t679)));
  t681 = ((gg_type_39 *)t675.data) + gg_index(t680, t675.len);
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
  t696 = ((float *)(((*(v5))).f19).data) + gg_index(t695, ((int64_t)UINT64_C(256)));
  t697 = v1;
  t698 = ((int64_t)(v2));
  t699 = gg_function_21(t697, t698, base + 0);
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
  v61 = gg_function_20(t705, t714, base + 0);
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
  gg_function_29(t715, t718, t725, base + 0);
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
  t750 = ((gg_type_12 *)t737.data) + gg_index(t749, t737.len);
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
  gg_function_34(t728, t729, t736, t751, t761, base + 0);
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
  t770 = ((float *)(((*(v5))).f19).data) + gg_index(t769, ((int64_t)UINT64_C(256)));
  t771 = v1;
  t772 = ((int64_t)(v2));
  t773 = gg_function_21(t771, t772, base + 0);
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
  gg_function_29(t785, t790, t802, base + 0);
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
  gg_function_29(t803, t810, t822, base + 0);
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
  v66 = gg_function_21(t828, t839, base + 0);
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
  t853 = ((gg_type_12 *)t849.data) + gg_index(t852, t849.len);
  t854 = (*t853);
  t855 = v12;
  t856 = ((uint16_t)(v66));
  t857 = ((int64_t)(((int64_t)UINT64_C(9))));
  t858 = ((int64_t)(((t856) >> ((uint64_t)(t857) & 15))));
  t859 = ((uint8_t *)t855.data) + gg_index(t858, t855.len);
  t860 = ((uint8_t)((*t859)));
  gg_function_34(t840, t841, t848, t854, t860, base + 0);
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
  t900 = ((gg_type_12 *)t898.data) + gg_index(t899, t898.len);
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
  gg_function_34(t889, t890, t897, t901, t913, base + 0);
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
  t918 = ((float *)(((*(v5))).f19).data) + gg_index(t917, ((int64_t)UINT64_C(256)));
  t919 = v1;
  t920 = ((int64_t)(v2));
  t921 = gg_function_21(t919, t920, base + 0);
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
  v69 = gg_function_20(t927, t934, base + 0);
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
  gg_function_29(t935, t938, t945, base + 0);
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
  t963 = ((gg_type_9 *)t948.data) + gg_index(t962, t948.len);
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
  t981 = ((gg_type_9 *)t964.data) + gg_index(t980, t964.len);
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
  gg_function_34(t998, t999, t1006, t1007, t1017, base + 0);
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
  t1022 = ((float *)(((*(v5))).f19).data) + gg_index(t1021, ((int64_t)UINT64_C(256)));
  t1023 = v1;
  t1024 = ((int64_t)(v2));
  t1025 = gg_function_21(t1023, t1024, base + 0);
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
  gg_function_29(t1038, t1041, t1054, base + 0);
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
  t1114 = ((gg_type_9 *)t1112.data) + gg_index(t1113, t1112.len);
  v81 = (*t1114);
  t1115 = v10;
  t1116 = ((int64_t)(v80));
  t1117 = ((gg_type_9 *)t1115.data) + gg_index(t1116, t1115.len);
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
  gg_function_34(t1134, t1135, t1142, t1143, t1155, base + 0);
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
  t1160 = ((float *)(((*(v5))).f19).data) + gg_index(t1159, ((int64_t)UINT64_C(256)));
  t1161 = v1;
  t1162 = ((int64_t)(v2));
  t1163 = gg_function_21(t1161, t1162, base + 0);
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
  v86 = gg_function_21(t1169, t1176, base + 0);
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
  gg_function_29(t1177, t1180, t1189, base + 0);
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
  t1230 = ((gg_type_16 *)t1228.data) + gg_index(t1229, t1228.len);
  t1231 = (*t1230);
  t1232 = ((int16_t)(v87));
  gg_function_36(t1219, t1220, t1227, t1231, t1232, base + 0);
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
  v90 = gg_function_21(t1235, t1238, base + 0);
  t1239 = v1;
  t1240 = ((int64_t)(v2));
  t1241 = ((int64_t)(((int64_t)UINT64_C(50))));
  t1242 = ((int64_t)(((int64_t)((uint64_t)(t1240) + (uint64_t)(t1241)))));
  v91 = gg_function_21(t1239, t1242, base + 0);
  t1243 = v1;
  t1244 = ((int64_t)(v2));
  t1245 = ((int64_t)(((int64_t)UINT64_C(52))));
  t1246 = ((int64_t)(((int64_t)((uint64_t)(t1244) + (uint64_t)(t1245)))));
  v92 = gg_function_21(t1243, t1246, base + 0);
  t1247 = v1;
  t1248 = ((int64_t)(v2));
  t1249 = ((int64_t)(((int64_t)UINT64_C(54))));
  t1250 = ((int64_t)(((int64_t)((uint64_t)(t1248) + (uint64_t)(t1249)))));
  v93 = gg_function_21(t1247, t1250, base + 0);
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
  t1270 = ((float *)(((*(v5))).f19).data) + gg_index(t1269, ((int64_t)UINT64_C(256)));
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
  v96 = gg_function_21(t1277, t1286, base + 0);
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
  gg_function_29(t1291, t1296, t1305, base + 0);
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
  gg_function_29(t1306, t1313, t1324, base + 0);
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
  t1386 = ((gg_type_16 *)t1384.data) + gg_index(t1385, t1384.len);
  t1387 = (*t1386);
  t1388 = ((int16_t)(v102));
  gg_function_36(t1375, t1376, t1383, t1387, t1388, base + 0);
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
static float gg_function_24(uint8_t p0, int64_t base) {
  float t0;
  uint8_t t1;
  uint8_t t2;
  uint8_t t3;
  uint8_t t4;
  uint8_t t5;
  uint8_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  uint8_t t10;
  uint8_t t11;
  uint8_t t12;
  uint8_t t13;
  int32_t t14;
  int32_t t15;
  uint8_t t16;
  float t17;
  int64_t t18;
  int32_t t19;
  float t20;
  float t21;
  float t22;
  float t23;
  float t24;
  float t25;
  float t26;
  float t27;
  int32_t t28;
  int32_t t29;
  int32_t t30;
  float t31;
  float t32;
  float t33;
  uint8_t v0;
  int32_t v1;
  float v2;
  v0 = p0;
  {
  t1 = ((uint8_t)(v0));
  t2 = ((uint8_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((uint8_t)(v0));
  t6 = ((uint8_t)(((int64_t)UINT64_C(127))));
  t4 = ((t5) == (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t8 = ((uint8_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(3))));
  t10 = ((uint8_t)(((t8) >> ((uint64_t)(t9) & 7))));
  t11 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v1 = ((int32_t)(((int32_t)(((t10) & (t11))))));
  t12 = ((uint8_t)(v0));
  t13 = ((uint8_t)(((int64_t)UINT64_C(7))));
  v2 = ((float)(((float)(((t12) & (t13))))));
  t14 = ((int32_t)(v1));
  t15 = ((int32_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = ((float)(v2));
  t18 = ((int64_t)UINT64_C(9));
  t19 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t18)))));
  t20 = ldexpf(t17, t19);
  t21 = ((float)(t20));
  t22 = ((float)(((float)(gg_double_bits(UINT64_C(4602678819172646912))))));
  t0 = ((float)(((t21) * (t22))));
  goto gg_done;
  }
  }
  t23 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t24 = ((float)(v2));
  t25 = ((float)(((float)(((int64_t)UINT64_C(8))))));
  t26 = ((float)(((t24) / (t25))));
  t27 = ((float)(((t23) + (t26))));
  t28 = ((int32_t)(v1));
  t29 = ((int32_t)(((int64_t)UINT64_C(7))));
  t30 = ((int32_t)(((int32_t)((uint64_t)(t28) - (uint64_t)(t29)))));
  t31 = ldexpf(t27, t30);
  t32 = ((float)(t31));
  t33 = ((float)(((float)(gg_double_bits(UINT64_C(4602678819172646912))))));
  t0 = ((float)(((t32) * (t33))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_23(uint8_t p0, int64_t base) {
  float t0;
  uint8_t t1;
  uint8_t t2;
  uint8_t t3;
  uint8_t t4;
  uint8_t t5;
  uint8_t t6;
  uint8_t t7;
  uint8_t t8;
  uint8_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  uint8_t v0;
  uint8_t v1;
  int64_t v2;
  v0 = p0;
  {
  t1 = ((uint8_t)(v0));
  t2 = ((uint8_t)(((int64_t)UINT64_C(7))));
  v1 = ((uint8_t)(((t1) & (t2))));
  t3 = ((uint8_t)(v1));
  t4 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t5 = ((t3) <= (t4));
  if (t5) {
  {
  v2 = ((int64_t)(((int32_t)(v1))));
  }
  } else {
  t6 = ((uint8_t)(v1));
  t7 = ((uint8_t)(((int64_t)UINT64_C(5))));
  t8 = ((t6) == (t7));
  if (t8) {
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(6))));
  }
  } else {
  t9 = ((uint8_t)(v1));
  t10 = ((uint8_t)(((int64_t)UINT64_C(6))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(8))));
  }
  } else {
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(12))));
  }
  }
  }
  }
  t13 = ((uint8_t)(v0));
  t14 = ((uint8_t)(((int64_t)UINT64_C(8))));
  t15 = ((t13) < (t14));
  if (t15) {
  {
  t12 = ((int64_t)(v2));
  }
  } else {
  {
  t16 = v2;
  t12 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t16)))));
  }
  }
  t0 = ((float)(((float)(t12))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_19(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, gg_slice p4, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  gg_slice t4;
  int64_t t5;
  uint32_t t6;
  float t7;
  float t8;
  gg_slice t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t * t15;
  float t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  uint8_t * t21;
  uint32_t t22;
  uint8_t t23;
  uint8_t t24;
  uint8_t t25;
  uint32_t t26;
  uint32_t t27;
  uint8_t t28;
  uint8_t t29;
  uint32_t t30;
  int64_t t31;
  uint32_t t32;
  float t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t * t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  uint8_t t46;
  uint8_t t47;
  uint8_t t48;
  int64_t t49;
  uint8_t t50;
  float t51;
  float t52;
  float t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
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
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t * t73;
  int64_t t74;
  int64_t t75;
  uint8_t t76;
  uint8_t t77;
  uint8_t t78;
  uint8_t t79;
  int64_t t80;
  uint8_t t81;
  float t82;
  float t83;
  gg_slice t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  uint8_t * t88;
  uint8_t t89;
  float t90;
  float t91;
  gg_slice t92;
  int64_t t93;
  uint16_t t94;
  float t95;
  int64_t t96;
  int64_t t97;
  uint8_t t98;
  gg_slice t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint8_t * t107;
  uint8_t t108;
  int64_t t109;
  int64_t t110;
  uint8_t t111;
  uint8_t t112;
  uint8_t t113;
  uint8_t t114;
  uint8_t t115;
  uint8_t t116;
  float t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
  gg_slice t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  uint8_t * t129;
  uint8_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  uint8_t t135;
  uint8_t t136;
  uint8_t t137;
  int32_t t138;
  int32_t t139;
  float t140;
  float t141;
  int64_t t142;
  int64_t t143;
  uint8_t t144;
  gg_slice t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  uint8_t * t151;
  float t152;
  float t153;
  int64_t t154;
  int64_t t155;
  uint8_t t156;
  gg_slice t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  uint8_t * t163;
  float t164;
  float t165;
  int64_t t166;
  int64_t t167;
  uint8_t t168;
  uint8_t t169;
  int64_t t170;
  int64_t t171;
  uint8_t t172;
  uint8_t t173;
  int64_t t174;
  int64_t t175;
  uint8_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  uint8_t t181;
  int64_t t182;
  gg_slice t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  uint8_t * t189;
  int64_t t190;
  int64_t t191;
  uint8_t t192;
  uint8_t t193;
  uint8_t t194;
  uint8_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  uint8_t t199;
  gg_slice t200;
  int64_t t201;
  int8_t * t202;
  float t203;
  float t204;
  int64_t t205;
  int64_t t206;
  uint8_t t207;
  int32_t t208;
  int32_t t209;
  float t210;
  float t211;
  float t212;
  float t213;
  float t214;
  gg_slice t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  uint16_t t219;
  float t220;
  float t221;
  int64_t t222;
  int64_t t223;
  uint8_t t224;
  uint8_t t225;
  int64_t t226;
  int64_t t227;
  gg_slice t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  gg_slice t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  uint8_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  uint8_t * t248;
  uint8_t t249;
  int64_t t250;
  int64_t t251;
  uint8_t t252;
  uint8_t t253;
  uint8_t t254;
  uint8_t t255;
  int64_t t256;
  uint32_t t257;
  uint32_t t258;
  uint32_t t259;
  uint32_t t260;
  uint32_t t261;
  uint32_t t262;
  int64_t t263;
  uint32_t t264;
  int64_t t265;
  int64_t t266;
  uint8_t t267;
  int32_t t268;
  int32_t t269;
  float t270;
  float t271;
  float t272;
  float t273;
  float t274;
  gg_slice t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  uint16_t t279;
  float t280;
  float t281;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  gg_slice v4;
  uint8_t v5;
  float v6;
  uint8_t v7;
  uint8_t v8;
  int64_t v9;
  int64_t v10;
  uint8_t v11;
  uint8_t v12;
  float v13;
  uint8_t v14;
  uint8_t v15;
  int64_t v16;
  uint8_t v17;
  uint8_t v18;
  uint32_t v19;
  uint8_t v20;
  uint32_t v21;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(15))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = v1;
  t5 = ((int64_t)(v2));
  t6 = gg_function_20(t4, t5, base + 0);
  t7 = fg143_codec_from_bits(t6);
  t8 = ((float)(t7));
  t9 = v1;
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(((int64_t)UINT64_C(4))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((uint8_t *)t9.data) + gg_index(t14, t9.len);
  t16 = ((float)(((float)(((int8_t)((*t15)))))));
  t0 = ((float)(((t8) * (t16))));
  goto gg_done;
  }
  }
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(39))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t20 = v2;
  t21 = ((uint8_t *)(v1).data) + gg_index(t20, (v1).len);
  v5 = ((uint8_t)((*t21)));
  t23 = ((uint8_t)(v5));
  t24 = ((uint8_t)(((int64_t)UINT64_C(2))));
  t25 = ((t23) < (t24));
  if (t25) {
  {
  t26 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(2097152))))));
  t27 = ((uint32_t)(((uint32_t)(v5))));
  t22 = ((uint32_t)(((uint32_t)((uint64_t)(t26) << ((uint64_t)(t27) & 31)))));
  }
  } else {
  {
  t28 = ((uint8_t)(v5));
  t29 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t30 = ((uint32_t)(((uint32_t)(((uint8_t)((uint64_t)(t28) - (uint64_t)(t29)))))));
  t31 = ((int64_t)(((int64_t)UINT64_C(23))));
  t22 = ((uint32_t)(((uint32_t)((uint64_t)(t30) << ((uint64_t)(t31) & 31)))));
  }
  }
  t32 = ((uint32_t)(t22));
  t33 = fg143_codec_from_bits(t32);
  v6 = ((float)(t33));
  t34 = v1;
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t38 = ((int64_t)(v3));
  t39 = ((int64_t)(((int64_t)UINT64_C(16))));
  t40 = ((int64_t)(((int64_t)gg_mod(t38, t39))));
  t41 = ((int64_t)((uint64_t)(t37) + (uint64_t)(t40)));
  t42 = ((uint8_t *)t34.data) + gg_index(t41, t34.len);
  v7 = ((uint8_t)((*t42)));
  t43 = ((int64_t)(v3));
  t44 = ((int64_t)(((int64_t)UINT64_C(16))));
  t45 = ((t43) < (t44));
  if (t45) {
  {
  t46 = ((uint8_t)(v7));
  t47 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v8 = ((uint8_t)(((t46) & (t47))));
  }
  } else {
  {
  t48 = ((uint8_t)(v7));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  v8 = ((uint8_t)(((t48) >> ((uint64_t)(t49) & 7))));
  }
  }
  t50 = ((uint8_t)(v8));
  t51 = gg_function_23(t50, base + 0);
  t52 = ((float)(t51));
  t53 = ((float)(v6));
  t0 = ((float)(((t52) * (t53))));
  goto gg_done;
  }
  }
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(((int64_t)UINT64_C(40))));
  t56 = ((t54) == (t55));
  if (t56) {
  {
  t57 = ((int64_t)(v3));
  t58 = ((int64_t)(((int64_t)UINT64_C(16))));
  v9 = ((int64_t)(((int64_t)gg_div(t57, t58))));
  t59 = ((int64_t)(v3));
  t60 = ((int64_t)(((int64_t)UINT64_C(16))));
  v10 = ((int64_t)(((int64_t)gg_mod(t59, t60))));
  t61 = v1;
  t62 = ((int64_t)(v2));
  t63 = ((int64_t)(((int64_t)UINT64_C(4))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  t65 = ((int64_t)(v9));
  t66 = ((int64_t)(((int64_t)UINT64_C(8))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t67)))));
  t69 = ((int64_t)(v10));
  t70 = ((int64_t)(((int64_t)UINT64_C(8))));
  t71 = ((int64_t)(((int64_t)gg_mod(t69, t70))));
  t72 = ((int64_t)((uint64_t)(t68) + (uint64_t)(t71)));
  t73 = ((uint8_t *)t61.data) + gg_index(t72, t61.len);
  v11 = ((uint8_t)((*t73)));
  t74 = ((int64_t)(v10));
  t75 = ((int64_t)(((int64_t)UINT64_C(8))));
  t76 = ((t74) < (t75));
  if (t76) {
  {
  t77 = ((uint8_t)(v11));
  t78 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v12 = ((uint8_t)(((t77) & (t78))));
  }
  } else {
  {
  t79 = ((uint8_t)(v11));
  t80 = ((int64_t)(((int64_t)UINT64_C(4))));
  v12 = ((uint8_t)(((t79) >> ((uint64_t)(t80) & 7))));
  }
  }
  t81 = ((uint8_t)(v12));
  t82 = gg_function_23(t81, base + 0);
  t83 = ((float)(t82));
  t84 = v1;
  t85 = ((int64_t)(v2));
  t86 = ((int64_t)(v9));
  t87 = ((int64_t)((uint64_t)(t85) + (uint64_t)(t86)));
  t88 = ((uint8_t *)t84.data) + gg_index(t87, t84.len);
  t89 = ((uint8_t)((*t88)));
  t90 = gg_function_24(t89, base + 0);
  t91 = ((float)(t90));
  t0 = ((float)(((t83) * (t91))));
  goto gg_done;
  }
  }
  t92 = v1;
  t93 = ((int64_t)(v2));
  t94 = gg_function_21(t92, t93, base + 0);
  t95 = fg141_half(t94);
  v13 = ((float)(t95));
  t96 = ((int64_t)(v0));
  t97 = ((int64_t)(((int64_t)UINT64_C(41))));
  t98 = ((t96) == (t97));
  if (t98) {
  {
  t99 = v1;
  t100 = ((int64_t)(v2));
  t101 = ((int64_t)(((int64_t)UINT64_C(2))));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t101)))));
  t103 = ((int64_t)(v3));
  t104 = ((int64_t)(((int64_t)UINT64_C(8))));
  t105 = ((int64_t)(((int64_t)gg_div(t103, t104))));
  t106 = ((int64_t)((uint64_t)(t102) + (uint64_t)(t105)));
  t107 = ((uint8_t *)t99.data) + gg_index(t106, t99.len);
  t108 = ((uint8_t)((*t107)));
  t109 = ((int64_t)(v3));
  t110 = ((int64_t)(((int64_t)UINT64_C(8))));
  t111 = ((uint8_t)(((uint8_t)(((int64_t)gg_mod(t109, t110))))));
  t112 = ((uint8_t)(((t108) >> ((uint64_t)(t111) & 7))));
  t113 = ((uint8_t)(((int64_t)UINT64_C(1))));
  v14 = ((uint8_t)(((t112) & (t113))));
  t114 = ((uint8_t)(v14));
  t115 = ((uint8_t)(((int64_t)UINT64_C(0))));
  t116 = ((t114) != (t115));
  if (t116) {
  {
  t0 = ((float)(v13));
  }
  } else {
  {
  t117 = v13;
  t0 = ((float)((-(t117))));
  }
  }
  goto gg_done;
  }
  }
  t118 = ((int64_t)(v0));
  t119 = ((int64_t)(((int64_t)UINT64_C(42))));
  t120 = ((t118) == (t119));
  if (t120) {
  {
  t121 = v1;
  t122 = ((int64_t)(v2));
  t123 = ((int64_t)(((int64_t)UINT64_C(2))));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t122) + (uint64_t)(t123)))));
  t125 = ((int64_t)(v3));
  t126 = ((int64_t)(((int64_t)UINT64_C(4))));
  t127 = ((int64_t)(((int64_t)gg_div(t125, t126))));
  t128 = ((int64_t)((uint64_t)(t124) + (uint64_t)(t127)));
  t129 = ((uint8_t *)t121.data) + gg_index(t128, t121.len);
  t130 = ((uint8_t)((*t129)));
  t131 = ((int64_t)(((int64_t)UINT64_C(2))));
  t132 = ((int64_t)(v3));
  t133 = ((int64_t)(((int64_t)UINT64_C(4))));
  t134 = ((int64_t)(((int64_t)gg_mod(t132, t133))));
  t135 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t131) * (uint64_t)(t134)))))));
  t136 = ((uint8_t)(((t130) >> ((uint64_t)(t135) & 7))));
  t137 = ((uint8_t)(((int64_t)UINT64_C(3))));
  v15 = ((uint8_t)(((t136) & (t137))));
  t138 = ((int32_t)(((int32_t)(v15))));
  t139 = ((int32_t)(((int64_t)UINT64_C(1))));
  t140 = ((float)(((float)(((int32_t)((uint64_t)(t138) - (uint64_t)(t139)))))));
  t141 = ((float)(v13));
  t0 = ((float)(((t140) * (t141))));
  goto gg_done;
  }
  }
  t142 = ((int64_t)(v0));
  t143 = ((int64_t)(((int64_t)UINT64_C(8))));
  t144 = ((t142) == (t143));
  if (t144) {
  {
  t145 = v1;
  t146 = ((int64_t)(v2));
  t147 = ((int64_t)(((int64_t)UINT64_C(2))));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t146) + (uint64_t)(t147)))));
  t149 = ((int64_t)(v3));
  t150 = ((int64_t)((uint64_t)(t148) + (uint64_t)(t149)));
  t151 = ((uint8_t *)t145.data) + gg_index(t150, t145.len);
  t152 = ((float)(((float)(((int8_t)((*t151)))))));
  t153 = ((float)(v13));
  t0 = ((float)(((t152) * (t153))));
  goto gg_done;
  }
  }
  t154 = ((int64_t)(v0));
  t155 = ((int64_t)(((int64_t)UINT64_C(9))));
  t156 = ((t154) == (t155));
  if (t156) {
  {
  t157 = v1;
  t158 = ((int64_t)(v2));
  t159 = ((int64_t)(((int64_t)UINT64_C(4))));
  t160 = ((int64_t)(((int64_t)((uint64_t)(t158) + (uint64_t)(t159)))));
  t161 = ((int64_t)(v3));
  t162 = ((int64_t)((uint64_t)(t160) + (uint64_t)(t161)));
  t163 = ((uint8_t *)t157.data) + gg_index(t162, t157.len);
  t164 = ((float)(((float)(((int8_t)((*t163)))))));
  t165 = ((float)(v13));
  t0 = ((float)(((t164) * (t165))));
  goto gg_done;
  }
  }
  t166 = ((int64_t)(v0));
  t167 = ((int64_t)(((int64_t)UINT64_C(2))));
  t168 = ((t166) == (t167));
  t169 = t168;
  if (!t169) {
  t170 = ((int64_t)(v0));
  t171 = ((int64_t)(((int64_t)UINT64_C(3))));
  t169 = ((t170) == (t171));
  }
  t172 = t169;
  t173 = t172;
  if (!t173) {
  t174 = ((int64_t)(v0));
  t175 = ((int64_t)(((int64_t)UINT64_C(20))));
  t173 = ((t174) == (t175));
  }
  t176 = t173;
  if (t176) {
  {
  t177 = ((int64_t)(v2));
  t179 = ((int64_t)(v0));
  t180 = ((int64_t)(((int64_t)UINT64_C(3))));
  t181 = ((t179) == (t180));
  if (t181) {
  {
  t178 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  } else {
  {
  t178 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  }
  t182 = ((int64_t)(t178));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t177) + (uint64_t)(t182)))));
  t183 = v1;
  t184 = ((int64_t)(v16));
  t185 = ((int64_t)(v3));
  t186 = ((int64_t)(((int64_t)UINT64_C(16))));
  t187 = ((int64_t)(((int64_t)gg_mod(t185, t186))));
  t188 = ((int64_t)((uint64_t)(t184) + (uint64_t)(t187)));
  t189 = ((uint8_t *)t183.data) + gg_index(t188, t183.len);
  v17 = ((uint8_t)((*t189)));
  t190 = ((int64_t)(v3));
  t191 = ((int64_t)(((int64_t)UINT64_C(16))));
  t192 = ((t190) < (t191));
  if (t192) {
  {
  t193 = ((uint8_t)(v17));
  t194 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v18 = ((uint8_t)(((t193) & (t194))));
  }
  } else {
  {
  t195 = ((uint8_t)(v17));
  t196 = ((int64_t)(((int64_t)UINT64_C(4))));
  v18 = ((uint8_t)(((t195) >> ((uint64_t)(t196) & 7))));
  }
  }
  t197 = ((int64_t)(v0));
  t198 = ((int64_t)(((int64_t)UINT64_C(20))));
  t199 = ((t197) == (t198));
  if (t199) {
  {
  t200 = v4;
  t201 = ((int64_t)(v18));
  t202 = ((int8_t *)t200.data) + gg_index(t201, t200.len);
  t203 = ((float)(((float)((*t202)))));
  t204 = ((float)(v13));
  t0 = ((float)(((t203) * (t204))));
  goto gg_done;
  }
  }
  t205 = ((int64_t)(v0));
  t206 = ((int64_t)(((int64_t)UINT64_C(2))));
  t207 = ((t205) == (t206));
  if (t207) {
  {
  t208 = ((int32_t)(((int32_t)(v18))));
  t209 = ((int32_t)(((int64_t)UINT64_C(8))));
  t210 = ((float)(((float)(((int32_t)((uint64_t)(t208) - (uint64_t)(t209)))))));
  t211 = ((float)(v13));
  t0 = ((float)(((t210) * (t211))));
  goto gg_done;
  }
  }
  t212 = ((float)(((float)(v18))));
  t213 = ((float)(v13));
  t214 = ((float)(((t212) * (t213))));
  t215 = v1;
  t216 = ((int64_t)(v2));
  t217 = ((int64_t)(((int64_t)UINT64_C(2))));
  t218 = ((int64_t)(((int64_t)((uint64_t)(t216) + (uint64_t)(t217)))));
  t219 = gg_function_21(t215, t218, base + 0);
  t220 = fg141_half(t219);
  t221 = ((float)(t220));
  t0 = ((float)(((t214) + (t221))));
  goto gg_done;
  }
  }
  t222 = ((int64_t)(v0));
  t223 = ((int64_t)(((int64_t)UINT64_C(6))));
  t224 = ((t222) == (t223));
  t225 = t224;
  if (!t225) {
  t226 = ((int64_t)(v0));
  t227 = ((int64_t)(((int64_t)UINT64_C(7))));
  t225 = ((t226) == (t227));
  }
  if (!(t225)) gg_fail("assertion failed");
  t228 = v1;
  t229 = ((int64_t)(v2));
  t231 = ((int64_t)(v0));
  t232 = ((int64_t)(((int64_t)UINT64_C(7))));
  t233 = ((t231) == (t232));
  if (t233) {
  {
  t230 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  } else {
  {
  t230 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  }
  t234 = ((int64_t)(t230));
  t235 = ((int64_t)(((int64_t)((uint64_t)(t229) + (uint64_t)(t234)))));
  v19 = gg_function_20(t228, t235, base + 0);
  t236 = v1;
  t237 = ((int64_t)(v2));
  t239 = ((int64_t)(v0));
  t240 = ((int64_t)(((int64_t)UINT64_C(7))));
  t241 = ((t239) == (t240));
  if (t241) {
  {
  t238 = ((int64_t)(((int64_t)UINT64_C(8))));
  }
  } else {
  {
  t238 = ((int64_t)(((int64_t)UINT64_C(6))));
  }
  }
  t242 = ((int64_t)(t238));
  t243 = ((int64_t)(((int64_t)((uint64_t)(t237) + (uint64_t)(t242)))));
  t244 = ((int64_t)(v3));
  t245 = ((int64_t)(((int64_t)UINT64_C(16))));
  t246 = ((int64_t)(((int64_t)gg_mod(t244, t245))));
  t247 = ((int64_t)((uint64_t)(t243) + (uint64_t)(t246)));
  t248 = ((uint8_t *)t236.data) + gg_index(t247, t236.len);
  v20 = ((uint8_t)((*t248)));
  t250 = ((int64_t)(v3));
  t251 = ((int64_t)(((int64_t)UINT64_C(16))));
  t252 = ((t250) < (t251));
  if (t252) {
  {
  t253 = ((uint8_t)(v20));
  t254 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t249 = ((uint8_t)(((t253) & (t254))));
  }
  } else {
  {
  t255 = ((uint8_t)(v20));
  t256 = ((int64_t)(((int64_t)UINT64_C(4))));
  t249 = ((uint8_t)(((t255) >> ((uint64_t)(t256) & 7))));
  }
  }
  t257 = ((uint32_t)(((uint32_t)(t249))));
  t258 = ((uint32_t)(v19));
  t259 = ((uint32_t)(((uint32_t)(v3))));
  t260 = ((uint32_t)(((t258) >> ((uint64_t)(t259) & 31))));
  t261 = ((uint32_t)(((int64_t)UINT64_C(1))));
  t262 = ((uint32_t)(((t260) & (t261))));
  t263 = ((int64_t)(((int64_t)UINT64_C(4))));
  t264 = ((uint32_t)(((uint32_t)((uint64_t)(t262) << ((uint64_t)(t263) & 31)))));
  v21 = ((uint32_t)(((t257) | (t264))));
  t265 = ((int64_t)(v0));
  t266 = ((int64_t)(((int64_t)UINT64_C(6))));
  t267 = ((t265) == (t266));
  if (t267) {
  {
  t268 = ((int32_t)(((int32_t)(v21))));
  t269 = ((int32_t)(((int64_t)UINT64_C(16))));
  t270 = ((float)(((float)(((int32_t)((uint64_t)(t268) - (uint64_t)(t269)))))));
  t271 = ((float)(v13));
  t0 = ((float)(((t270) * (t271))));
  goto gg_done;
  }
  }
  t272 = ((float)(((float)(v21))));
  t273 = ((float)(v13));
  t274 = ((float)(((t272) * (t273))));
  t275 = v1;
  t276 = ((int64_t)(v2));
  t277 = ((int64_t)(((int64_t)UINT64_C(2))));
  t278 = ((int64_t)(((int64_t)((uint64_t)(t276) + (uint64_t)(t277)))));
  t279 = gg_function_21(t275, t278, base + 0);
  t280 = fg141_half(t279);
  t281 = ((float)(t280));
  t0 = ((float)(((t274) + (t281))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(int64_t p0, int64_t base) {
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
  uint8_t t11;
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
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(20))));
  t4 = ((t5) == (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(41))));
  t8 = ((t9) == (t10));
  }
  t11 = t8;
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(42))));
  t12 = ((t13) == (t14));
  }
  t15 = t12;
  if (t15) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(18))));
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(3))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(20))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(6))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(22))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(7))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(24))));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(8))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(34))));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(9))));
  t30 = ((t28) == (t29));
  t31 = t30;
  if (!t31) {
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(40))));
  t31 = ((t32) == (t33));
  }
  t34 = t31;
  if (t34) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(36))));
  goto gg_done;
  }
  }
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(39))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(17))));
  goto gg_done;
  }
  }
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(15))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(292))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_8(int64_t p0, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(41))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(128))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(42))));
  t6 = ((t4) == (t5));
  t7 = t6;
  if (!t7) {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(40))));
  t7 = ((t8) == (t9));
  }
  t10 = t7;
  if (t10) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(64))));
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(15))));
  t13 = ((t11) == (t12));
  if (t13) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(256))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(32))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_7(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
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
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(12))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(13))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(14))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(11))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(3))));
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(10))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(4))));
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(23))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(5))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(16))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(6))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(17))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(7))));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(22))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(8))));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(18))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(9))));
  goto gg_done;
  }
  }
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(21))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(10))));
  goto gg_done;
  }
  }
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(19))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(11))));
  goto gg_done;
  }
  }
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(29))));
  t39 = ((t37) == (t38));
  if (t39) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(12))));
  goto gg_done;
  }
  }
  t40 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t40)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_18(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
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
  gg_slice t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  float * t34;
  int64_t t35;
  gg_slice t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  gg_slice t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  gg_type_40 t55;
  gg_type_39 t56;
  uint8_t t57;
  uint8_t t58;
  int64_t t59;
  gg_type_39 t60;
  int64_t t61;
  gg_type_40 t62;
  int64_t t63;
  float t64;
  float t65;
  int64_t t66;
  float t67;
  float t68;
  int64_t t69;
  gg_type_43 t70;
  int32_t t71;
  int32_t t72;
  int64_t t73;
  gg_type_43 t74;
  int64_t t75;
  gg_type_43 t76;
  int32_t t77;
  int32_t t78;
  int64_t t79;
  gg_type_43 t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  gg_slice t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  gg_type_45 t93;
  gg_slice t94;
  gg_slice t95;
  gg_slice t96;
  gg_slice t97;
  gg_slice t98;
  gg_slice t99;
  gg_slice t100;
  gg_slice t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  gg_slice t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  gg_type_43 * t110;
  gg_slice t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int32_t * t115;
  gg_slice t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  gg_type_43 * t120;
  gg_slice t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int32_t * t125;
  int64_t t126;
  gg_type_40 * t127;
  gg_slice t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  gg_type_39 * t132;
  gg_slice t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  uint8_t * t137;
  int32_t t138;
  int32_t t139;
  gg_slice t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  float * t146;
  int64_t t147;
  float * t148;
  float t149;
  float t150;
  float t151;
  float t152;
  float t153;
  int64_t t154;
  float * t155;
  float t156;
  float t157;
  float t158;
  int64_t v0;
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
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  gg_type_32 v16;
  gg_type_38 v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  float v21;
  float v22;
  int32_t v23;
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
  t1 = ((int64_t)(v0));
  v11 = gg_function_7(t1, base + 0);
  t2 = ((int64_t)(v11));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) < (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v0));
  v12 = gg_function_8(t5, base + 0);
  t6 = ((int64_t)(v0));
  v13 = gg_function_10(t6, base + 0);
  t7 = ((int64_t)(v13));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t10 = ((int64_t)((v2).len));
  t11 = ((int64_t)(v12));
  t12 = ((int64_t)(((int64_t)gg_mod(t10, t11))));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) == (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)((v1).len));
  t17 = ((int64_t)((v2).len));
  t18 = ((int64_t)(v12));
  t19 = ((int64_t)(((int64_t)gg_div(t17, t18))));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t15 = ((t16) >= (t21));
  }
  if (!(t15)) gg_fail("assertion failed");
  t23 = ((int64_t)((v2).len));
  t24 = ((int64_t)(v12));
  t25 = ((int64_t)gg_div(t23, t24));
  t22 = 0;
label0:;
  if (t22 >= t25) goto label2;
  v14 = t22;
  {
  t27 = v12;
  t26 = 0;
label3:;
  if (t26 >= t27) goto label5;
  v15 = t26;
  {
  t28 = v2;
  t29 = ((int64_t)(v14));
  t30 = ((int64_t)(v12));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t32 = ((int64_t)(v15));
  t33 = ((int64_t)((uint64_t)(t31) + (uint64_t)(t32)));
  t34 = ((float *)t28.data) + gg_index(t33, t28.len);
  t35 = ((int64_t)(v0));
  t36 = v1;
  t37 = ((int64_t)(v14));
  t38 = ((int64_t)(v13));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(v15));
  t41 = v10;
  (*t34) = gg_function_19(t35, t36, t39, t40, t41, base + 0);
  }
label4:;
  t26++; goto label3;
label5:;
  }
label1:;
  t22++; goto label0;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t42 = ((int64_t)(v11));
  v16 = gg_function_9(t42, base + 0);
  t43 = ((int64_t)((v2).len));
  t44 = ((int64_t)(((int64_t)UINT64_C(256))));
  t45 = ((int64_t)(((int64_t)gg_mod(t43, t44))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((t45) == (t46));
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)((v1).len));
  t50 = ((int64_t)((v2).len));
  t51 = ((int64_t)(((int64_t)UINT64_C(256))));
  t52 = ((int64_t)(((int64_t)gg_div(t50, t51))));
  t53 = ((int64_t)((v16).f13));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  t48 = ((t49) >= (t54));
  }
  if (!(t48)) gg_fail("assertion failed");
  memset(&(v17), 0, sizeof(v17));
  memset(&t57, 0, sizeof(t57));
  t58 = ((uint8_t)(t57));
  for (t59=0; t59<32; t59++) (t56).data[t59] = t58;
  t60 = t56;
  for (t61=0; t61<8; t61++) (t55).data[t61] = t60;
  t62 = t55;
  for (t63=0; t63<256; t63++) ((v17).f18).data[t63] = t62;
  memset(&t64, 0, sizeof(t64));
  t65 = ((float)(t64));
  for (t66=0; t66<256; t66++) ((v17).f19).data[t66] = t65;
  memset(&t67, 0, sizeof(t67));
  t68 = ((float)(t67));
  for (t69=0; t69<256; t69++) ((v17).f20).data[t69] = t68;
  memset(&t71, 0, sizeof(t71));
  t72 = ((int32_t)(t71));
  for (t73=0; t73<8; t73++) (t70).data[t73] = t72;
  t74 = t70;
  for (t75=0; t75<512; t75++) ((v17).f21).data[t75] = t74;
  memset(&t77, 0, sizeof(t77));
  t78 = ((int32_t)(t77));
  for (t79=0; t79<8; t79++) (t76).data[t79] = t78;
  t80 = t76;
  for (t81=0; t81<512; t81++) ((v17).f22).data[t81] = t80;
  t83 = ((int64_t)((v2).len));
  t84 = ((int64_t)(((int64_t)UINT64_C(256))));
  t85 = ((int64_t)gg_div(t83, t84));
  t82 = 0;
label6:;
  if (t82 >= t85) goto label8;
  v18 = t82;
  {
  t86 = ((int64_t)(v11));
  t87 = v1;
  t88 = ((int64_t)(v18));
  t89 = ((int64_t)((v16).f13));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) * (uint64_t)(t89)))));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = &(v17);
  t94 = v3;
  t95 = v4;
  t96 = v5;
  t97 = v6;
  t98 = v7;
  t99 = v8;
  t100 = v9;
  t101 = v10;
  gg_function_27(t86, t87, t90, t91, t92, t93, t94, t95, t96, t97, t98, t99, t100, t101, base + 0);
  t103 = ((int64_t)UINT64_C(256));
  t102 = 0;
label9:;
  if (t102 >= t103) goto label11;
  v19 = t102;
  {
  t104 = ((int64_t)(v19));
  t105 = ((int64_t)((v16).f14));
  v20 = ((int64_t)(((int64_t)gg_div(t104, t105))));
  t106 = gg_view(((v17).f21).data, 512);
  t107 = ((int64_t)(v20));
  t108 = ((int64_t)(((int64_t)UINT64_C(8))));
  t109 = ((int64_t)gg_div(t107, t108));
  t110 = ((gg_type_43 *)t106.data) + gg_index(t109, t106.len);
  t111 = gg_view(((*t110)).data, 8);
  t112 = ((int64_t)(v20));
  t113 = ((int64_t)(((int64_t)UINT64_C(8))));
  t114 = ((int64_t)gg_mod(t112, t113));
  t115 = ((int32_t *)t111.data) + gg_index(t114, t111.len);
  v21 = ((float)(((float)((*t115)))));
  t116 = gg_view(((v17).f22).data, 512);
  t117 = ((int64_t)(v20));
  t118 = ((int64_t)(((int64_t)UINT64_C(8))));
  t119 = ((int64_t)gg_div(t117, t118));
  t120 = ((gg_type_43 *)t116.data) + gg_index(t119, t116.len);
  t121 = gg_view(((*t120)).data, 8);
  t122 = ((int64_t)(v20));
  t123 = ((int64_t)(((int64_t)UINT64_C(8))));
  t124 = ((int64_t)gg_mod(t122, t123));
  t125 = ((int32_t *)t121.data) + gg_index(t124, t121.len);
  v22 = ((float)(((float)((*t125)))));
  /* FG_DIRECT_SLICE_INDEX */
  t126 = ((int64_t)UINT64_C(0));
  t127 = ((gg_type_40 *)((v17).f18).data) + gg_index(t126, ((int64_t)UINT64_C(256)));
  t128 = gg_view(((*t127)).data, 8);
  t129 = ((int64_t)(v19));
  t130 = ((int64_t)(((int64_t)UINT64_C(32))));
  t131 = ((int64_t)gg_div(t129, t130));
  t132 = ((gg_type_39 *)t128.data) + gg_index(t131, t128.len);
  t133 = gg_view(((*t132)).data, 32);
  t134 = ((int64_t)(v19));
  t135 = ((int64_t)(((int64_t)UINT64_C(32))));
  t136 = ((int64_t)gg_mod(t134, t135));
  t137 = ((uint8_t *)t133.data) + gg_index(t136, t133.len);
  t138 = ((int32_t)(((int32_t)((*t137)))));
  t139 = ((int32_t)(((int32_t)((v16).f16))));
  v23 = ((int32_t)(((int32_t)((uint64_t)(t138) - (uint64_t)(t139)))));
  t140 = v2;
  t141 = ((int64_t)(v18));
  t142 = ((int64_t)(((int64_t)UINT64_C(256))));
  t143 = ((int64_t)(((int64_t)((uint64_t)(t141) * (uint64_t)(t142)))));
  t144 = ((int64_t)(v19));
  t145 = ((int64_t)((uint64_t)(t143) + (uint64_t)(t144)));
  t146 = ((float *)t140.data) + gg_index(t145, t140.len);
  /* FG_DIRECT_SLICE_INDEX */
  t147 = ((int64_t)UINT64_C(0));
  t148 = ((float *)((v17).f19).data) + gg_index(t147, ((int64_t)UINT64_C(256)));
  t149 = ((float)((*t148)));
  t150 = ((float)(v21));
  t151 = ((float)(((t149) * (t150))));
  t152 = ((float)(((float)(v23))));
  t153 = ((float)(((t151) * (t152))));
  /* FG_DIRECT_SLICE_INDEX */
  t154 = ((int64_t)UINT64_C(0));
  t155 = ((float *)((v17).f20).data) + gg_index(t154, ((int64_t)UINT64_C(256)));
  t156 = ((float)((*t155)));
  t157 = ((float)(v22));
  t158 = ((float)(((t156) * (t157))));
  (*t146) = ((float)(((t153) - (t158))));
  }
label10:;
  t102++; goto label9;
label11:;
  }
label7:;
  t82++; goto label6;
label8:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_6(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  uint8_t t19;
  gg_type_31 t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  gg_type_32 t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t * t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  uint8_t t42;
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
  uint8_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  gg_type_31 t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t * t72;
  int64_t t73;
  int64_t t74;
  gg_type_31 t75;
  int64_t t76;
  gg_type_31 t77;
  int64_t t78;
  int64_t t79;
  gg_type_31 t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint8_t t85;
  gg_type_31 t86;
  gg_type_31 t87;
  gg_type_20 t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  int64_t t94;
  int64_t t95;
  int64_t * t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t * t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t * t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  int64_t t110;
  int64_t * t111;
  int64_t t112;
  int64_t t113;
  gg_type_31 t114;
  gg_slice t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  uint8_t t119;
  uint8_t t120;
  int64_t t121;
  int64_t t122;
  int64_t * t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t * t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t * t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t * t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t * t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t * t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t * t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t * t155;
  int64_t t156;
  int64_t t157;
  int64_t * t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t * t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t * t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t * t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  uint8_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  uint8_t t183;
  uint8_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  gg_slice t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  gg_slice t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  gg_slice t200;
  gg_slice t201;
  gg_slice t202;
  gg_slice t203;
  gg_slice t204;
  gg_slice t205;
  gg_slice t206;
  gg_slice t207;
  gg_slice t208;
  gg_slice t209;
  uint8_t t210;
  int64_t t211;
  int64_t t212;
  gg_type_31 t213;
  gg_slice t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  float * t221;
  float t222;
  int64_t t223;
  int64_t t224;
  gg_type_20 v0;
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
  gg_slice v29;
  int64_t v30;
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
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f8));
  t6 = ((int64_t)(((int64_t)UINT64_C(34))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f8));
  t10 = ((int64_t)(((int64_t)UINT64_C(35))));
  t8 = ((t9) != (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(((*(v0))).f8));
  t14 = ((int64_t)(((int64_t)UINT64_C(40))));
  t12 = ((t13) != (t14));
  }
  t15 = t12;
  if (t15) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t16 = ((int64_t)((((*(v0))).f7).f2));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  t19 = t18;
  if (!t19) {
  t20 = &(((*(v0))).f4);
  t21 = gg_function_3(t20, base + 0);
  t19 = t21;
  }
  t22 = t19;
  if (t22) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t23 = ((int64_t)((((*(v0))).f4).f2));
  v13 = gg_function_7(t23, base + 0);
  t24 = ((int64_t)(v13));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) >= (t25));
  if (t26) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  } else {
  {
  t27 = ((int64_t)((((*(v0))).f4).f2));
  v14 = gg_function_8(t27, base + 0);
  }
  }
  t28 = ((int64_t)(v13));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) >= (t29));
  if (t30) {
  {
  t31 = ((int64_t)(v13));
  t32 = gg_function_9(t31, base + 0);
  v15 = ((int64_t)((t32).f13));
  }
  } else {
  {
  t33 = ((int64_t)((((*(v0))).f4).f2));
  v15 = gg_function_10(t33, base + 0);
  }
  }
  t34 = ((int64_t)(v15));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t37 = ((int64_t)UINT64_C(0));
  t38 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t37, ((int64_t)UINT64_C(4)));
  v16 = ((int64_t)((*t38)));
  t39 = ((int64_t)(v16));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((t39) > (t40));
  t42 = t41;
  if (t42) {
  t43 = ((int64_t)(v16));
  t44 = ((int64_t)(v14));
  t45 = ((int64_t)(((int64_t)gg_mod(t43, t44))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t45) == (t46));
  }
  if (!(t42)) gg_fail("assertion failed");
  t47 = ((int64_t)(v16));
  t48 = ((int64_t)(v14));
  t49 = ((int64_t)(((int64_t)gg_div(t47, t48))));
  t50 = ((int64_t)(v15));
  v17 = gg_function_2(t49, t50, base + 0);
  t51 = ((int64_t)((((*(v0))).f4).f3));
  t52 = ((int64_t)((v1).len));
  t53 = ((t51) <= (t52));
  t54 = t53;
  if (t54) {
  t55 = ((int64_t)((v4).len));
  t56 = ((int64_t)(v16));
  t57 = ((int64_t)(((*(v0))).f10));
  t58 = gg_function_2(t56, t57, base + 0);
  t59 = ((int64_t)(t58));
  t54 = ((t55) >= (t59));
  }
  if (!(t54)) gg_fail("assertion failed");
  t60 = &(((*(v0))).f7);
  t61 = ((int64_t)((v3).len));
  gg_function_11(t60, t61, base + 0);
  t62 = ((int64_t)(((*(v0))).f9));
  t63 = ((int64_t)(v16));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t64 = ((int64_t)(((*(v0))).f8));
  t65 = ((int64_t)(((int64_t)UINT64_C(40))));
  t66 = ((t64) == (t65));
  if (t66) {
  {
  t67 = ((int64_t)((((*(v0))).f5).f2));
  t68 = ((int64_t)(((int64_t)UINT64_C(26))));
  t69 = ((t67) == (t68));
  t70 = t69;
  if (t70) {
  /* FG_DIRECT_SLICE_INDEX */
  t71 = ((int64_t)UINT64_C(0));
  t72 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t71, ((int64_t)UINT64_C(4)));
  t73 = ((int64_t)((*t72)));
  t74 = ((int64_t)(v16));
  t70 = ((t73) == (t74));
  }
  if (!(t70)) gg_fail("assertion failed");
  t75 = &(((*(v0))).f5);
  t76 = ((int64_t)((v2).len));
  gg_function_11(t75, t76, base + 0);
  }
  } else {
  {
  t77 = &(((*(v0))).f4);
  t78 = gg_function_12(t77, base + 0);
  t79 = ((int64_t)(t78));
  t80 = &(((*(v0))).f7);
  t81 = gg_function_12(t80, base + 0);
  t82 = ((int64_t)(t81));
  if (!(((t79) == (t82)))) gg_fail("assertion failed");
  }
  }
  t83 = ((int64_t)(((*(v0))).f8));
  t84 = ((int64_t)(((int64_t)UINT64_C(40))));
  t85 = ((t83) == (t84));
  if (t85) {
  {
  t86 = &(((*(v0))).f7);
  v19 = gg_function_13(t86, base + 0);
  }
  } else {
  {
  t87 = &(((*(v0))).f4);
  v19 = gg_function_13(t87, base + 0);
  }
  }
  t88 = v0;
  v20 = gg_function_14(t88, base + 0);
label1:;
  t89 = ((int64_t)(v20));
  t90 = ((int64_t)(v19));
  if (!(((t89) < (t90)))) goto label2;
  {
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = ((int64_t)(((*(v0))).f8));
  t92 = ((int64_t)(((int64_t)UINT64_C(40))));
  t93 = ((t91) == (t92));
  if (t93) {
  {
  t94 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t95 = ((int64_t)UINT64_C(0));
  t96 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t95, ((int64_t)UINT64_C(4)));
  t97 = ((int64_t)((*t96)));
  v22 = ((int64_t)(((int64_t)gg_mod(t94, t97))));
  t98 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t99 = ((int64_t)UINT64_C(0));
  t100 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t99, ((int64_t)UINT64_C(4)));
  t101 = ((int64_t)((*t100)));
  t102 = ((int64_t)(((int64_t)gg_div(t98, t101))));
  /* FG_DIRECT_SLICE_INDEX */
  t103 = ((int64_t)UINT64_C(1));
  t104 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t103, ((int64_t)UINT64_C(4)));
  t105 = ((int64_t)((*t104)));
  v23 = ((int64_t)(((int64_t)gg_mod(t102, t105))));
  t106 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = ((int64_t)((*t108)));
  /* FG_DIRECT_SLICE_INDEX */
  t110 = ((int64_t)UINT64_C(1));
  t111 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t110, ((int64_t)UINT64_C(4)));
  t112 = ((int64_t)((*t111)));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t109) * (uint64_t)(t112)))));
  v24 = ((int64_t)(((int64_t)gg_div(t106, t113))));
  t114 = &(((*(v0))).f5);
  t115 = v2;
  t116 = ((int64_t)(v20));
  v25 = gg_function_15(t114, t115, t116, base + 0);
  t117 = ((int64_t)(v25));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((t117) >= (t118));
  t120 = t119;
  if (t120) {
  t121 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t122 = ((int64_t)UINT64_C(1));
  t123 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t122, ((int64_t)UINT64_C(4)));
  t124 = ((int64_t)((*t123)));
  t120 = ((t121) < (t124));
  }
  if (!(t120)) gg_fail("assertion failed");
  t125 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t126 = ((int64_t)UINT64_C(1));
  t127 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t126, ((int64_t)UINT64_C(4)));
  t128 = ((int64_t)((*t127)));
  t129 = ((int64_t)(((int64_t)((uint64_t)(t125) * (uint64_t)(t128)))));
  t130 = ((int64_t)(v23));
  /* FG_DIRECT_SLICE_INDEX */
  t131 = ((int64_t)UINT64_C(2));
  t132 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t131, ((int64_t)UINT64_C(4)));
  t133 = ((int64_t)((*t132)));
  t134 = ((int64_t)(((int64_t)((uint64_t)(t130) * (uint64_t)(t133)))));
  t135 = ((int64_t)(((int64_t)((uint64_t)(t129) + (uint64_t)(t134)))));
  t136 = ((int64_t)(v24));
  /* FG_DIRECT_SLICE_INDEX */
  t137 = ((int64_t)UINT64_C(3));
  t138 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t137, ((int64_t)UINT64_C(4)));
  t139 = ((int64_t)((*t138)));
  t140 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t139)))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t135) + (uint64_t)(t140)))));
  }
  } else {
  {
  t141 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t142 = ((int64_t)UINT64_C(1));
  t143 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t142, ((int64_t)UINT64_C(4)));
  t144 = ((int64_t)((*t143)));
  v26 = ((int64_t)(((int64_t)gg_mod(t141, t144))));
  t145 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t146 = ((int64_t)UINT64_C(1));
  t147 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t146, ((int64_t)UINT64_C(4)));
  t148 = ((int64_t)((*t147)));
  t149 = ((int64_t)(((int64_t)gg_div(t145, t148))));
  /* FG_DIRECT_SLICE_INDEX */
  t150 = ((int64_t)UINT64_C(2));
  t151 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t150, ((int64_t)UINT64_C(4)));
  t152 = ((int64_t)((*t151)));
  v27 = ((int64_t)(((int64_t)gg_mod(t149, t152))));
  t153 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t154 = ((int64_t)UINT64_C(1));
  t155 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t154, ((int64_t)UINT64_C(4)));
  t156 = ((int64_t)((*t155)));
  /* FG_DIRECT_SLICE_INDEX */
  t157 = ((int64_t)UINT64_C(2));
  t158 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t157, ((int64_t)UINT64_C(4)));
  t159 = ((int64_t)((*t158)));
  t160 = ((int64_t)(((int64_t)((uint64_t)(t156) * (uint64_t)(t159)))));
  v28 = ((int64_t)(((int64_t)gg_div(t153, t160))));
  t161 = ((int64_t)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t162 = ((int64_t)UINT64_C(1));
  t163 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t162, ((int64_t)UINT64_C(4)));
  t164 = ((int64_t)((*t163)));
  t165 = ((int64_t)(((int64_t)((uint64_t)(t161) * (uint64_t)(t164)))));
  t166 = ((int64_t)(v27));
  /* FG_DIRECT_SLICE_INDEX */
  t167 = ((int64_t)UINT64_C(2));
  t168 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t167, ((int64_t)UINT64_C(4)));
  t169 = ((int64_t)((*t168)));
  t170 = ((int64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t169)))));
  t171 = ((int64_t)(((int64_t)((uint64_t)(t165) + (uint64_t)(t170)))));
  t172 = ((int64_t)(v28));
  /* FG_DIRECT_SLICE_INDEX */
  t173 = ((int64_t)UINT64_C(3));
  t174 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t173, ((int64_t)UINT64_C(4)));
  t175 = ((int64_t)((*t174)));
  t176 = ((int64_t)(((int64_t)((uint64_t)(t172) * (uint64_t)(t175)))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t171) + (uint64_t)(t176)))));
  }
  }
  t177 = ((int64_t)(v21));
  t178 = ((int64_t)(((int64_t)UINT64_C(0))));
  t179 = ((t177) >= (t178));
  t180 = t179;
  if (t180) {
  t181 = ((int64_t)(v21));
  t182 = ((int64_t)((((*(v0))).f4).f3));
  t180 = ((t181) <= (t182));
  }
  t183 = t180;
  t184 = t183;
  if (t184) {
  t185 = ((int64_t)(v17));
  t186 = ((int64_t)((((*(v0))).f4).f3));
  t187 = ((int64_t)(v21));
  t188 = ((int64_t)(((int64_t)((uint64_t)(t186) - (uint64_t)(t187)))));
  t184 = ((t185) <= (t188));
  }
  if (!(t184)) gg_fail("assertion failed");
  t189 = v4;
  t190 = v18;
  t191 = ((int64_t)(v18));
  t192 = ((int64_t)(v16));
  t193 = ((int64_t)((uint64_t)(t191) + (uint64_t)(t192)));
  v29 = gg_sub(t189, t190, t193, sizeof(float));
  t194 = ((int64_t)((((*(v0))).f4).f2));
  t195 = v1;
  t196 = v21;
  t197 = ((int64_t)(v21));
  t198 = ((int64_t)(v17));
  t199 = ((int64_t)((uint64_t)(t197) + (uint64_t)(t198)));
  t200 = gg_sub(t195, t196, t199, sizeof(uint8_t));
  t201 = v29;
  t202 = v5;
  t203 = v6;
  t204 = v7;
  t205 = v8;
  t206 = v9;
  t207 = v10;
  t208 = v11;
  t209 = v12;
  t210 = gg_function_18(t194, t200, t201, t202, t203, t204, t205, t206, t207, t208, t209, base + 0);
  if (!(t210)) gg_fail("assertion failed");
  t212 = v16;
  t211 = 0;
label3:;
  if (t211 >= t212) goto label5;
  v30 = t211;
  {
  t213 = &(((*(v0))).f7);
  t214 = v3;
  t215 = ((int64_t)(v20));
  t216 = ((int64_t)(v16));
  t217 = ((int64_t)(((int64_t)((uint64_t)(t215) * (uint64_t)(t216)))));
  t218 = ((int64_t)(v30));
  t219 = ((int64_t)(((int64_t)((uint64_t)(t217) + (uint64_t)(t218)))));
  /* FG_DIRECT_SLICE_INDEX */
  t220 = v30;
  t221 = ((float *)(v29).data) + gg_index(t220, (v29).len);
  t222 = ((float)((*t221)));
  gg_function_37(t213, t214, t219, t222, base + 0);
  }
label4:;
  t211++; goto label3;
label5:;
  t223 = v20;
  t224 = ((int64_t)(((*(v0))).f10));
  v20 = ((int64_t)((uint64_t)(t223) + (uint64_t)(t224)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_1(gg_type_20 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, gg_slice p14, int64_t base) {
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
  int64_t t11;
  int64_t * t12;
  int64_t t13;
  int64_t t14;
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
  int64_t t33;
  int64_t * t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t * t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  uint8_t t63;
  gg_type_31 t64;
  uint8_t t65;
  uint8_t t66;
  uint8_t t67;
  uint8_t t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
  int64_t t72;
  int64_t * t73;
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
  int64_t * t86;
  int64_t t87;
  int64_t * t88;
  int64_t t89;
  int64_t * t90;
  int64_t t91;
  int64_t * t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  gg_type_20 t97;
  gg_slice t98;
  gg_slice t99;
  gg_slice t100;
  gg_slice t101;
  gg_slice t102;
  gg_slice t103;
  gg_slice t104;
  gg_slice t105;
  gg_slice t106;
  gg_slice t107;
  gg_slice t108;
  gg_slice t109;
  uint8_t t110;
  uint8_t t111;
  int64_t t112;
  gg_type_31 t113;
  int64_t t114;
  gg_type_31 t115;
  uint8_t t116;
  uint8_t t117;
  uint8_t t118;
  uint8_t t119;
  gg_type_31 t120;
  uint8_t t121;
  uint8_t t122;
  uint8_t t123;
  gg_type_31 t124;
  int64_t t125;
  gg_type_31 t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  uint8_t t130;
  gg_type_31 t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  uint8_t t135;
  gg_type_31 t136;
  int64_t t137;
  gg_type_20 t138;
  gg_slice t139;
  gg_slice t140;
  gg_slice t141;
  uint8_t t142;
  uint8_t t143;
  int64_t t144;
  gg_type_31 t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  uint8_t t149;
  uint8_t t150;
  int64_t t151;
  int64_t t152;
  uint8_t t153;
  uint8_t t154;
  int64_t t155;
  int64_t t156;
  uint8_t t157;
  uint8_t t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
  int64_t t162;
  int64_t t163;
  uint8_t t164;
  gg_type_20 t165;
  gg_slice t166;
  gg_slice t167;
  gg_slice t168;
  uint8_t t169;
  uint8_t t170;
  uint8_t t171;
  gg_type_20 t172;
  gg_slice t173;
  gg_slice t174;
  gg_slice t175;
  uint8_t t176;
  uint8_t t177;
  uint8_t t178;
  gg_type_20 t179;
  gg_slice t180;
  gg_slice t181;
  gg_slice t182;
  gg_slice t183;
  gg_slice t184;
  uint8_t t185;
  uint8_t t186;
  uint8_t t187;
  gg_type_20 t188;
  gg_slice t189;
  gg_slice t190;
  gg_slice t191;
  gg_slice t192;
  uint8_t t193;
  uint8_t t194;
  uint8_t t195;
  gg_type_20 t196;
  gg_slice t197;
  gg_slice t198;
  gg_slice t199;
  uint8_t t200;
  uint8_t t201;
  uint8_t t202;
  gg_type_20 t203;
  gg_slice t204;
  gg_slice t205;
  gg_slice t206;
  uint8_t t207;
  uint8_t t208;
  uint8_t t209;
  gg_type_20 t210;
  gg_slice t211;
  gg_slice t212;
  gg_slice t213;
  uint8_t t214;
  uint8_t t215;
  uint8_t t216;
  gg_type_20 t217;
  gg_slice t218;
  gg_slice t219;
  gg_slice t220;
  uint8_t t221;
  uint8_t t222;
  uint8_t t223;
  gg_type_20 t224;
  gg_slice t225;
  gg_slice t226;
  gg_slice t227;
  uint8_t t228;
  uint8_t t229;
  uint8_t t230;
  gg_type_20 t231;
  gg_slice t232;
  gg_slice t233;
  uint8_t t234;
  uint8_t t235;
  uint8_t t236;
  gg_type_20 t237;
  gg_slice t238;
  gg_slice t239;
  gg_slice t240;
  gg_slice t241;
  gg_slice t242;
  uint8_t t243;
  uint8_t t244;
  uint8_t t245;
  gg_type_20 t246;
  gg_slice t247;
  gg_slice t248;
  gg_slice t249;
  uint8_t t250;
  uint8_t t251;
  int64_t t252;
  gg_type_31 t253;
  int64_t t254;
  gg_type_20 v0;
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
  gg_slice v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  uint8_t v19;
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
  {
  t1 = ((int64_t)(((*(v0))).f10));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f9));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f9));
  t10 = ((int64_t)(((*(v0))).f10));
  t8 = ((t9) < (t10));
  }
  if (!(t8)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t11 = ((int64_t)UINT64_C(15));
  t12 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t11, ((int64_t)UINT64_C(16)));
  t13 = ((int64_t)((*t12)));
  t14 = ((int64_t)(((int64_t)UINT64_C(1464816203))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t16 = ((int64_t)((v6).len));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t16) >= (t17)))) gg_fail("assertion failed");
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((*(v0))).f8));
  t19 = ((int64_t)(((int64_t)UINT64_C(46))));
  t20 = ((t18) == (t19));
  t21 = t20;
  if (!t21) {
  t22 = ((int64_t)(((*(v0))).f8));
  t23 = ((int64_t)(((int64_t)UINT64_C(47))));
  t21 = ((t22) == (t23));
  }
  t24 = t21;
  t25 = t24;
  if (!t25) {
  t26 = ((int64_t)(((*(v0))).f8));
  t27 = ((int64_t)(((int64_t)UINT64_C(48))));
  t25 = ((t26) == (t27));
  }
  t28 = t25;
  t29 = t28;
  if (!t29) {
  t30 = ((int64_t)(((*(v0))).f8));
  t31 = ((int64_t)(((int64_t)UINT64_C(49))));
  t29 = ((t30) == (t31));
  }
  t32 = t29;
  if (t32) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t33 = ((int64_t)UINT64_C(0));
  t34 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t33, ((int64_t)UINT64_C(4)));
  t35 = ((int64_t)((*t34)));
  t36 = ((int64_t)(((*(v0))).f10));
  v15 = gg_function_2(t35, t36, base + 0);
  }
  }
  t37 = ((int64_t)(((*(v0))).f8));
  t38 = ((int64_t)(((int64_t)UINT64_C(69))));
  t39 = ((t37) == (t38));
  t40 = t39;
  if (!t40) {
  t41 = ((int64_t)(((*(v0))).f8));
  t42 = ((int64_t)(((int64_t)UINT64_C(70))));
  t40 = ((t41) == (t42));
  }
  t43 = t40;
  if (t43) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t44 = ((int64_t)UINT64_C(0));
  t45 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t44, ((int64_t)UINT64_C(4)));
  t46 = ((int64_t)((*t45)));
  t47 = ((int64_t)(((*(v0))).f10));
  v16 = gg_function_2(t46, t47, base + 0);
  }
  }
  t48 = ((int64_t)(((*(v0))).f8));
  t49 = ((int64_t)(((int64_t)UINT64_C(1))));
  t50 = ((t48) == (t49));
  t51 = t50;
  if (!t51) {
  t52 = ((int64_t)(((*(v0))).f8));
  t53 = ((int64_t)(((int64_t)UINT64_C(34))));
  t51 = ((t52) == (t53));
  }
  t54 = t51;
  t55 = t54;
  if (!t55) {
  t56 = ((int64_t)(((*(v0))).f8));
  t57 = ((int64_t)(((int64_t)UINT64_C(35))));
  t55 = ((t56) == (t57));
  }
  t58 = t55;
  t59 = t58;
  if (!t59) {
  t60 = ((int64_t)(((*(v0))).f8));
  t61 = ((int64_t)(((int64_t)UINT64_C(40))));
  t59 = ((t60) == (t61));
  }
  t62 = t59;
  t63 = t62;
  if (t63) {
  t64 = &(((*(v0))).f4);
  t65 = gg_function_3(t64, base + 0);
  t66 = t65;
  t63 = (!(t66));
  }
  t67 = t63;
  t68 = t67;
  if (t68) {
  t69 = ((int64_t)((((*(v0))).f7).f2));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = ((t69) == (t70));
  }
  t71 = t68;
  if (t71) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t72 = ((int64_t)UINT64_C(0));
  t73 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t72, ((int64_t)UINT64_C(4)));
  t74 = ((int64_t)((*t73)));
  t75 = ((int64_t)(((*(v0))).f10));
  v15 = gg_function_2(t74, t75, base + 0);
  }
  }
  t76 = ((int64_t)(v15));
  t77 = ((int64_t)(((int64_t)UINT64_C(4))));
  v17 = gg_function_2(t76, t77, base + 0);
  t78 = ((int64_t)(v17));
  t79 = ((int64_t)(((int64_t)UINT64_C(7))));
  t80 = gg_function_5(t78, t79, base + 0);
  t81 = ((int64_t)(t80));
  t82 = ((int64_t)(((int64_t)UINT64_C(8))));
  t83 = ((int64_t)(((int64_t)gg_div(t81, t82))));
  t84 = ((int64_t)(((int64_t)UINT64_C(8))));
  v18 = gg_function_2(t83, t84, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t85 = ((int64_t)UINT64_C(0));
  t86 = ((int64_t *)(v6).data) + gg_index(t85, (v6).len);
  (*t86) = ((int64_t)(v15));
  /* FG_DIRECT_SLICE_INDEX */
  t87 = ((int64_t)UINT64_C(1));
  t88 = ((int64_t *)(v6).data) + gg_index(t87, (v6).len);
  (*t88) = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t89 = ((int64_t)UINT64_C(2));
  t90 = ((int64_t *)(v6).data) + gg_index(t89, (v6).len);
  (*t90) = ((int64_t)(v18));
  /* FG_DIRECT_SLICE_INDEX */
  t91 = ((int64_t)UINT64_C(3));
  t92 = ((int64_t *)(v6).data) + gg_index(t91, (v6).len);
  t93 = ((int64_t)(v18));
  t94 = ((int64_t)(v16));
  t95 = ((int64_t)(((int64_t)UINT64_C(8))));
  t96 = gg_function_2(t94, t95, base + 0);
  (*t92) = gg_function_5(t93, t96, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t97 = v0;
  t98 = v1;
  t99 = v2;
  t100 = v4;
  t101 = v5;
  t102 = v7;
  t103 = v8;
  t104 = v9;
  t105 = v10;
  t106 = v11;
  t107 = v12;
  t108 = v13;
  t109 = v14;
  t110 = gg_function_6(t97, t98, t99, t100, t101, t102, t103, t104, t105, t106, t107, t108, t109, base + 0);
  t111 = t110;
  if (t111) {
  {
  t112 = ((int64_t)(((*(v0))).f8));
  t113 = &(((*(v0))).f7);
  t114 = gg_function_12(t113, base + 0);
  fg143_witness(t112, t114);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t115 = &(((*(v0))).f4);
  t116 = gg_function_3(t115, base + 0);
  t117 = t116;
  t118 = (!(t117));
  t119 = t118;
  if (!t119) {
  t120 = &(((*(v0))).f7);
  t121 = gg_function_3(t120, base + 0);
  t122 = t121;
  t119 = (!(t122));
  }
  t123 = t119;
  if (t123) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t124 = &(((*(v0))).f7);
  t125 = ((int64_t)((v4).len));
  gg_function_11(t124, t125, base + 0);
  t126 = &(((*(v0))).f4);
  t127 = ((int64_t)((v1).len));
  gg_function_11(t126, t127, base + 0);
  t128 = ((int64_t)((((*(v0))).f5).f3));
  t129 = ((int64_t)(((int64_t)UINT64_C(0))));
  t130 = ((t128) > (t129));
  if (t130) {
  {
  t131 = &(((*(v0))).f5);
  t132 = ((int64_t)((v2).len));
  gg_function_11(t131, t132, base + 0);
  }
  }
  t133 = ((int64_t)((((*(v0))).f6).f3));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((t133) > (t134));
  if (t135) {
  {
  t136 = &(((*(v0))).f6);
  t137 = ((int64_t)((v3).len));
  gg_function_11(t136, t137, base + 0);
  }
  }
  t138 = v0;
  t139 = v1;
  t140 = v2;
  t141 = v4;
  t142 = gg_function_45(t138, t139, t140, t141, base + 0);
  t143 = t142;
  if (t143) {
  {
  t144 = ((int64_t)(((*(v0))).f8));
  t145 = &(((*(v0))).f7);
  t146 = gg_function_12(t145, base + 0);
  fg143_witness(t144, t146);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t147 = ((int64_t)((((*(v0))).f4).f2));
  t148 = ((int64_t)(((int64_t)UINT64_C(27))));
  t149 = ((t147) == (t148));
  t150 = t149;
  if (!t150) {
  t151 = ((int64_t)((((*(v0))).f4).f2));
  t152 = ((int64_t)(((int64_t)UINT64_C(28))));
  t150 = ((t151) == (t152));
  }
  t153 = t150;
  t154 = t153;
  if (!t154) {
  t155 = ((int64_t)((((*(v0))).f7).f2));
  t156 = ((int64_t)(((int64_t)UINT64_C(27))));
  t154 = ((t155) == (t156));
  }
  t157 = t154;
  t158 = t157;
  if (!t158) {
  t159 = ((int64_t)((((*(v0))).f7).f2));
  t160 = ((int64_t)(((int64_t)UINT64_C(28))));
  t158 = ((t159) == (t160));
  }
  t161 = t158;
  if (t161) {
  {
  t162 = ((int64_t)(((*(v0))).f8));
  t163 = ((int64_t)(((int64_t)UINT64_C(19))));
  t164 = ((t162) != (t163));
  if (t164) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  }
  }
  t165 = v0;
  t166 = v1;
  t167 = v2;
  t168 = v4;
  t169 = gg_function_52(t165, t166, t167, t168, base + 0);
  t170 = t169;
  t171 = t170;
  if (!t171) {
  t172 = v0;
  t173 = v1;
  t174 = v2;
  t175 = v4;
  t176 = gg_function_55(t172, t173, t174, t175, base + 0);
  t171 = t176;
  }
  t177 = t171;
  t178 = t177;
  if (!t178) {
  t179 = v0;
  t180 = v1;
  t181 = v2;
  t182 = v3;
  t183 = v4;
  t184 = v5;
  t185 = gg_function_57(t179, t180, t181, t182, t183, t184, base + 0);
  t178 = t185;
  }
  t186 = t178;
  t187 = t186;
  if (!t187) {
  t188 = v0;
  t189 = v1;
  t190 = v2;
  t191 = v3;
  t192 = v4;
  t193 = gg_function_67(t188, t189, t190, t191, t192, base + 0);
  t187 = t193;
  }
  t194 = t187;
  t195 = t194;
  if (!t195) {
  t196 = v0;
  t197 = v1;
  t198 = v2;
  t199 = v4;
  t200 = gg_function_70(t196, t197, t198, t199, base + 0);
  t195 = t200;
  }
  t201 = t195;
  t202 = t201;
  if (!t202) {
  t203 = v0;
  t204 = v1;
  t205 = v2;
  t206 = v4;
  t207 = gg_function_71(t203, t204, t205, t206, base + 0);
  t202 = t207;
  }
  t208 = t202;
  t209 = t208;
  if (!t209) {
  t210 = v0;
  t211 = v1;
  t212 = v2;
  t213 = v4;
  t214 = gg_function_86(t210, t211, t212, t213, base + 0);
  t209 = t214;
  }
  t215 = t209;
  t216 = t215;
  if (!t216) {
  t217 = v0;
  t218 = v1;
  t219 = v2;
  t220 = v4;
  t221 = gg_function_91(t217, t218, t219, t220, base + 0);
  t216 = t221;
  }
  t222 = t216;
  t223 = t222;
  if (!t223) {
  t224 = v0;
  t225 = v1;
  t226 = v2;
  t227 = v4;
  t228 = gg_function_92(t224, t225, t226, t227, base + 0);
  t223 = t228;
  }
  t229 = t223;
  t230 = t229;
  if (!t230) {
  t231 = v0;
  t232 = v1;
  t233 = v4;
  t234 = gg_function_93(t231, t232, t233, base + 0);
  t230 = t234;
  }
  t235 = t230;
  t236 = t235;
  if (!t236) {
  t237 = v0;
  t238 = v1;
  t239 = v2;
  t240 = v3;
  t241 = v4;
  t242 = v5;
  t243 = gg_function_95(t237, t238, t239, t240, t241, t242, base + 0);
  t236 = t243;
  }
  t244 = t236;
  t245 = t244;
  if (!t245) {
  t246 = v0;
  t247 = v1;
  t248 = v4;
  t249 = v6;
  t250 = gg_function_96(t246, t247, t248, t249, base + 0);
  t245 = t250;
  }
  v19 = t245;
  t251 = v19;
  if (t251) {
  {
  t252 = ((int64_t)(((*(v0))).f8));
  t253 = &(((*(v0))).f7);
  t254 = gg_function_12(t253, base + 0);
  fg143_witness(t252, t254);
  }
  }
  t0 = v19;
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
  float t38;
  float t39;
  int64_t t40;
  uint8_t t41;
  uint8_t t42;
  int64_t t43;
  uint8_t t44;
  uint8_t t45;
  int64_t t46;
  float t47;
  float t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  gg_type_12 t53;
  uint8_t t54;
  uint8_t t55;
  int64_t t56;
  gg_type_12 t57;
  int64_t t58;
  gg_type_9 t59;
  uint8_t t60;
  uint8_t t61;
  int64_t t62;
  gg_type_9 t63;
  int64_t t64;
  gg_type_16 t65;
  int8_t t66;
  int8_t t67;
  int64_t t68;
  gg_type_16 t69;
  int64_t t70;
  uint8_t t71;
  uint8_t t72;
  int64_t t73;
  int8_t t74;
  int8_t t75;
  int64_t t76;
  gg_type_20 t77;
  gg_slice t78;
  gg_slice t79;
  gg_slice t80;
  gg_slice t81;
  gg_slice t82;
  gg_slice t83;
  gg_slice t84;
  gg_slice t85;
  gg_slice t86;
  gg_slice t87;
  gg_slice t88;
  gg_slice t89;
  gg_slice t90;
  gg_slice t91;
  gg_header t92;
  int64_t t93;
  gg_type_1 v0;
  gg_type_9 v1;
  gg_type_9 v2;
  gg_type_10 v3;
  gg_type_11 v4;
  gg_type_13 v5;
  gg_type_14 v6;
  gg_type_17 v7;
  gg_type_18 v8;
  gg_type_19 v9;
  uint8_t v10;
  {
  memset(&(v0), 0, sizeof(v0));
  memset(&((v0).f4), 0, sizeof((v0).f4));
  memset(&t0, 0, sizeof(t0));
  t1 = ((int64_t)(t0));
  for (t2=0; t2<4; t2++) (((v0).f4).f0).data[t2] = t1;
  memset(&t3, 0, sizeof(t3));
  t4 = ((int64_t)(t3));
  for (t5=0; t5<4; t5++) (((v0).f4).f1).data[t5] = t4;
  memset(&t6, 0, sizeof(t6));
  ((v0).f4).f2 = ((int64_t)(t6));
  memset(&t7, 0, sizeof(t7));
  ((v0).f4).f3 = ((int64_t)(t7));
  memset(&((v0).f5), 0, sizeof((v0).f5));
  memset(&t8, 0, sizeof(t8));
  t9 = ((int64_t)(t8));
  for (t10=0; t10<4; t10++) (((v0).f5).f0).data[t10] = t9;
  memset(&t11, 0, sizeof(t11));
  t12 = ((int64_t)(t11));
  for (t13=0; t13<4; t13++) (((v0).f5).f1).data[t13] = t12;
  memset(&t14, 0, sizeof(t14));
  ((v0).f5).f2 = ((int64_t)(t14));
  memset(&t15, 0, sizeof(t15));
  ((v0).f5).f3 = ((int64_t)(t15));
  memset(&((v0).f6), 0, sizeof((v0).f6));
  memset(&t16, 0, sizeof(t16));
  t17 = ((int64_t)(t16));
  for (t18=0; t18<4; t18++) (((v0).f6).f0).data[t18] = t17;
  memset(&t19, 0, sizeof(t19));
  t20 = ((int64_t)(t19));
  for (t21=0; t21<4; t21++) (((v0).f6).f1).data[t21] = t20;
  memset(&t22, 0, sizeof(t22));
  ((v0).f6).f2 = ((int64_t)(t22));
  memset(&t23, 0, sizeof(t23));
  ((v0).f6).f3 = ((int64_t)(t23));
  memset(&((v0).f7), 0, sizeof((v0).f7));
  memset(&t24, 0, sizeof(t24));
  t25 = ((int64_t)(t24));
  for (t26=0; t26<4; t26++) (((v0).f7).f0).data[t26] = t25;
  memset(&t27, 0, sizeof(t27));
  t28 = ((int64_t)(t27));
  for (t29=0; t29<4; t29++) (((v0).f7).f1).data[t29] = t28;
  memset(&t30, 0, sizeof(t30));
  ((v0).f7).f2 = ((int64_t)(t30));
  memset(&t31, 0, sizeof(t31));
  ((v0).f7).f3 = ((int64_t)(t31));
  memset(&t32, 0, sizeof(t32));
  (v0).f8 = ((int64_t)(t32));
  memset(&t33, 0, sizeof(t33));
  (v0).f9 = ((int64_t)(t33));
  memset(&t34, 0, sizeof(t34));
  (v0).f10 = ((int64_t)(t34));
  memset(&t35, 0, sizeof(t35));
  t36 = ((int64_t)(t35));
  for (t37=0; t37<16; t37++) ((v0).f11).data[t37] = t36;
  memset(&t38, 0, sizeof(t38));
  t39 = ((float)(t38));
  for (t40=0; t40<8; t40++) ((v0).f12).data[t40] = t39;
  (v0).f10 = ((int64_t)(((int64_t)UINT64_C(1))));
  memset(&t41, 0, sizeof(t41));
  t42 = ((uint8_t)(t41));
  for (t43=0; t43<4; t43++) (v1).data[t43] = t42;
  memset(&t44, 0, sizeof(t44));
  t45 = ((uint8_t)(t44));
  for (t46=0; t46<4; t46++) (v2).data[t46] = t45;
  memset(&t47, 0, sizeof(t47));
  t48 = ((float)(t47));
  for (t49=0; t49<1; t49++) (v3).data[t49] = t48;
  memset(&t50, 0, sizeof(t50));
  t51 = ((int64_t)(t50));
  for (t52=0; t52<1; t52++) (v4).data[t52] = t51;
  memset(&t54, 0, sizeof(t54));
  t55 = ((uint8_t)(t54));
  for (t56=0; t56<8; t56++) (t53).data[t56] = t55;
  t57 = t53;
  for (t58=0; t58<1; t58++) (v5).data[t58] = t57;
  memset(&t60, 0, sizeof(t60));
  t61 = ((uint8_t)(t60));
  for (t62=0; t62<4; t62++) (t59).data[t62] = t61;
  t63 = t59;
  for (t64=0; t64<1; t64++) (v6).data[t64] = t63;
  memset(&t66, 0, sizeof(t66));
  t67 = ((int8_t)(t66));
  for (t68=0; t68<8; t68++) (t65).data[t68] = t67;
  t69 = t65;
  for (t70=0; t70<1; t70++) (v7).data[t70] = t69;
  memset(&t71, 0, sizeof(t71));
  t72 = ((uint8_t)(t71));
  for (t73=0; t73<128; t73++) (v8).data[t73] = t72;
  memset(&t74, 0, sizeof(t74));
  t75 = ((int8_t)(t74));
  for (t76=0; t76<16; t76++) (v9).data[t76] = t75;
  t77 = &(v0);
  t78 = gg_view((v1).data, 4);
  t79 = gg_view((v1).data, 4);
  t80 = gg_view((v1).data, 4);
  t81 = gg_view((v2).data, 4);
  t82 = gg_view((v3).data, 1);
  t83 = gg_view((v4).data, 1);
  t84 = gg_view((v5).data, 1);
  t85 = gg_view((v5).data, 1);
  t86 = gg_view((v5).data, 1);
  t87 = gg_view((v6).data, 1);
  t88 = gg_view((v6).data, 1);
  t89 = gg_view((v7).data, 1);
  t90 = gg_view((v8).data, 128);
  t91 = gg_view((v9).data, 16);
  v10 = gg_function_1(t77, t78, t79, t80, t81, t82, t83, t84, t85, t86, t87, t88, t89, t90, t91, base + 0);
  t92.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t92.len = 0;
  gg_bool(gg_ref(&t92, gg_get_stack(base + 0)), v10);
  gg_append(gg_ref(&t92, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t92).base, (t92).len));
  }
gg_done:;
  for (t93 = 0; t93 < 1; t93++) gg_stacks[base + t93].top = 0;
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
