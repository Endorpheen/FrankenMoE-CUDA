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
typedef struct { uint8_t data[18]; } gg_type_2;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[32]; } gg_type_4;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[8]; } gg_type_5;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_5 data[1]; } gg_type_6;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[4]; } gg_type_7;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_7 data[1]; } gg_type_8;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int8_t data[8]; } gg_type_10;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_10 data[1]; } gg_type_11;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[128]; } gg_type_12;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int8_t data[16]; } gg_type_13;
#pragma pack(pop)
typedef struct gg_type_27 gg_type_27;
#pragma pack(push, 1)
struct gg_type_27 {
  int64_t f0;
  int64_t f1;
  int64_t f2;
  int64_t f3;
  int64_t f4;
};
#pragma pack(pop)
typedef struct gg_type_28 gg_type_28;
#pragma pack(push, 1)
typedef struct { uint8_t data[32]; } gg_type_29;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_29 data[8]; } gg_type_30;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_30 data[256]; } gg_type_31;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[256]; } gg_type_32;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_33;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_33 data[512]; } gg_type_34;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_28 {
  gg_type_31 f5;
  gg_type_32 f6;
  gg_type_32 f7;
  gg_type_34 f8;
  gg_type_34 f9;
};
#pragma pack(pop)
typedef gg_type_28 *gg_type_35;
typedef struct gg_type_37 gg_type_37;
#pragma pack(push, 1)
struct gg_type_37 {
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
};
#pragma pack(pop)
typedef struct gg_type_38 gg_type_38;
#pragma pack(push, 1)
struct gg_type_38 {
  int32_t f54;
  int32_t f55;
};
#pragma pack(pop)
typedef struct gg_type_39 gg_type_39;
#pragma pack(push, 1)
typedef struct { int8_t data[32]; } gg_type_40;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_40 data[8]; } gg_type_41;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int16_t data[16]; } gg_type_42;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_39 {
  float f56;
  gg_type_41 f57;
  gg_type_42 f58;
};
#pragma pack(pop)
typedef struct gg_type_43 gg_type_43;
#pragma pack(push, 1)
typedef struct { gg_type_41 data[256]; } gg_type_44;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_43 {
  gg_type_44 f59;
  gg_type_34 f60;
  gg_type_32 f61;
};
#pragma pack(pop)
typedef struct gg_type_45 gg_type_45;
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_46;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_46 data[8192]; } gg_type_47;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_45 {
  gg_type_28 f62;
  gg_type_43 f63;
  gg_type_47 f64;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_48;
#pragma pack(pop)
typedef gg_type_37 *gg_type_49;
typedef gg_type_45 *gg_type_50;
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_51;
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
static uint8_t gg_eq_9(int8_t a, int8_t b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, int8_t value);
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
static uint8_t gg_eq_14(int64_t a, int64_t b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, int64_t value);
static uint8_t gg_eq_15(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, gg_slice value);
static uint8_t gg_eq_16(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, gg_slice value);
static uint8_t gg_eq_17(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_slice value);
static uint8_t gg_eq_18(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, gg_slice value);
static uint8_t gg_eq_19(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, gg_slice value);
static uint8_t gg_eq_20(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_20(gg_slice a, gg_slice b);
static void gg_render_20(gg_reference out, gg_slice value);
static uint8_t gg_eq_21(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_21(gg_slice a, gg_slice b);
static void gg_render_21(gg_reference out, uint8_t value);
static uint8_t gg_eq_23(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_23(gg_slice a, gg_slice b);
static void gg_render_23(gg_reference out, uint32_t value);
static uint8_t gg_eq_24(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_24(gg_slice a, gg_slice b);
static void gg_render_24(gg_reference out, uint16_t value);
static uint8_t gg_eq_25(int32_t a, int32_t b);
static uint8_t gg_equal_elements_25(gg_slice a, gg_slice b);
static void gg_render_25(gg_reference out, int32_t value);
static uint8_t gg_eq_26(double a, double b);
static uint8_t gg_equal_elements_26(gg_slice a, gg_slice b);
static void gg_render_26(gg_reference out, double value);
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
static uint8_t gg_eq_36(int16_t a, int16_t b);
static uint8_t gg_equal_elements_36(gg_slice a, gg_slice b);
static void gg_render_36(gg_reference out, int16_t value);
static uint8_t gg_eq_37(gg_type_37 a, gg_type_37 b);
static uint8_t gg_equal_elements_37(gg_slice a, gg_slice b);
static void gg_render_37(gg_reference out, gg_type_37 value);
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
static uint8_t gg_eq_46(gg_type_46 a, gg_type_46 b);
static uint8_t gg_equal_elements_46(gg_slice a, gg_slice b);
static void gg_render_46(gg_reference out, gg_type_46 value);
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
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
return gg_equal_elements_1(gg_view((a).data, 18),gg_view((b).data, 18));
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
gg_text(out,gg_view((value).data, 18));
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
return gg_equal_elements_3(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_type_5*)a.data)[i],((gg_type_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
return gg_equal_elements_1(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
gg_text(out,gg_view((value).data, 8));
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_type_6*)a.data)[i],((gg_type_6*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b) {
return gg_equal_elements_5(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_6(gg_reference out, gg_type_6 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_5*)view.data)[i]).data, 8));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_1(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
gg_text(out,gg_view((value).data, 4));
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
return gg_equal_elements_7(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_7*)view.data)[i]).data, 4));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_9(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
return gg_equal_elements_9(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_9(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_type_11*)a.data)[i],((gg_type_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b) {
return gg_equal_elements_10(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_11(gg_reference out, gg_type_11 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((gg_type_10*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return gg_equal_elements_1(gg_view((a).data, 128),gg_view((b).data, 128));
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
gg_text(out,gg_view((value).data, 128));
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
return gg_equal_elements_9(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_9(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_14(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_15(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_16(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_slice a, gg_slice b) {
return gg_equal_elements_5(a,b);
}
static void gg_render_17(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_5*)view.data)[i]).data, 8));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_slice a, gg_slice b) {
return gg_equal_elements_7(a,b);
}
static void gg_render_18(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_7*)view.data)[i]).data, 4));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(gg_slice a, gg_slice b) {
return gg_equal_elements_10(a,b);
}
static void gg_render_19(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((gg_type_10*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_slice a, gg_slice b) {
return gg_equal_elements_9(a,b);
}
static void gg_render_20(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_9(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_21(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_23(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_24(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_25(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(double a, double b) {
return a == b;
}
static void gg_render_26(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((gg_type_27*)a.data)[i],((gg_type_27*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b) {
if(!gg_eq_14(a.f0,b.f0))return 0;
if(!gg_eq_14(a.f1,b.f1))return 0;
if(!gg_eq_14(a.f2,b.f2))return 0;
if(!gg_eq_14(a.f3,b.f3))return 0;
if(!gg_eq_14(a.f4,b.f4))return 0;
return 1;
}
static void gg_render_27(gg_reference out, gg_type_27 value) {
gg_append(out,"\124\151\154\145\124\162\141\151\164\163\040\173\040",13,1);
gg_render_14(out, value.f0);
gg_append(out,", ",2,1);
gg_render_14(out, value.f1);
gg_append(out,", ",2,1);
gg_render_14(out, value.f2);
gg_append(out,", ",2,1);
gg_render_14(out, value.f3);
gg_append(out,", ",2,1);
gg_render_14(out, value.f4);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_type_28*)a.data)[i],((gg_type_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
if(!gg_eq_31(a.f5,b.f5))return 0;
if(!gg_eq_32(a.f6,b.f6))return 0;
if(!gg_eq_32(a.f7,b.f7))return 0;
if(!gg_eq_34(a.f8,b.f8))return 0;
if(!gg_eq_34(a.f9,b.f9))return 0;
return 1;
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
gg_append(out,"\127\145\151\147\150\164\124\151\154\145\040\173\040",13,1);
gg_render_31(out, value.f5);
gg_append(out,", ",2,1);
gg_render_32(out, value.f6);
gg_append(out,", ",2,1);
gg_render_32(out, value.f7);
gg_append(out,", ",2,1);
gg_render_34(out, value.f8);
gg_append(out,", ",2,1);
gg_render_34(out, value.f9);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((gg_type_29*)a.data)[i],((gg_type_29*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b) {
return gg_equal_elements_1(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_29(gg_reference out, gg_type_29 value) {
gg_text(out,gg_view((value).data, 32));
}
static uint8_t gg_equal_elements_30(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_30(((gg_type_30*)a.data)[i],((gg_type_30*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_30(gg_type_30 a, gg_type_30 b) {
return gg_equal_elements_29(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_30(gg_reference out, gg_type_30 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_29*)view.data)[i]).data, 32));}gg_append(out,"]",1,1);
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
return gg_equal_elements_3(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_32(gg_reference out, gg_type_32 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_33(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_33(((gg_type_33*)a.data)[i],((gg_type_33*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_33(gg_type_33 a, gg_type_33 b) {
return gg_equal_elements_25(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_33(gg_reference out, gg_type_33 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_25(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_34(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_34(((gg_type_34*)a.data)[i],((gg_type_34*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_34(gg_type_34 a, gg_type_34 b) {
return gg_equal_elements_33(gg_view((a).data, 512),gg_view((b).data, 512));
}
static void gg_render_34(gg_reference out, gg_type_34 value) {
int64_t i; gg_slice view=gg_view((value).data, 512);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_33(out, ((gg_type_33*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_35(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_35(((gg_type_35*)a.data)[i],((gg_type_35*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b) {
return a == b;
}
static void gg_render_35(gg_reference out, gg_type_35 value) {
gg_render_28(out,*value);
}
static uint8_t gg_equal_elements_36(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_36(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_36(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_36(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_37(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_37(((gg_type_37*)a.data)[i],((gg_type_37*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_37(gg_type_37 a, gg_type_37 b) {
if(!gg_eq_14(a.f10,b.f10))return 0;
if(!gg_eq_14(a.f11,b.f11))return 0;
if(!gg_eq_14(a.f12,b.f12))return 0;
if(!gg_eq_14(a.f13,b.f13))return 0;
if(!gg_eq_14(a.f14,b.f14))return 0;
if(!gg_eq_14(a.f15,b.f15))return 0;
if(!gg_eq_14(a.f16,b.f16))return 0;
if(!gg_eq_14(a.f17,b.f17))return 0;
if(!gg_eq_14(a.f18,b.f18))return 0;
if(!gg_eq_14(a.f19,b.f19))return 0;
if(!gg_eq_14(a.f20,b.f20))return 0;
if(!gg_eq_14(a.f21,b.f21))return 0;
if(!gg_eq_14(a.f22,b.f22))return 0;
if(!gg_eq_14(a.f23,b.f23))return 0;
if(!gg_eq_14(a.f24,b.f24))return 0;
if(!gg_eq_14(a.f25,b.f25))return 0;
if(!gg_eq_14(a.f26,b.f26))return 0;
if(!gg_eq_14(a.f27,b.f27))return 0;
if(!gg_eq_14(a.f28,b.f28))return 0;
if(!gg_eq_14(a.f29,b.f29))return 0;
if(!gg_eq_14(a.f30,b.f30))return 0;
if(!gg_eq_14(a.f31,b.f31))return 0;
if(!gg_eq_14(a.f32,b.f32))return 0;
if(!gg_eq_14(a.f33,b.f33))return 0;
if(!gg_eq_14(a.f34,b.f34))return 0;
if(!gg_eq_14(a.f35,b.f35))return 0;
if(!gg_eq_14(a.f36,b.f36))return 0;
if(!gg_eq_14(a.f37,b.f37))return 0;
if(!gg_eq_14(a.f38,b.f38))return 0;
if(!gg_eq_14(a.f39,b.f39))return 0;
if(!gg_eq_14(a.f40,b.f40))return 0;
if(!gg_eq_14(a.f41,b.f41))return 0;
if(!gg_eq_14(a.f42,b.f42))return 0;
if(!gg_eq_14(a.f43,b.f43))return 0;
if(!gg_eq_14(a.f44,b.f44))return 0;
if(!gg_eq_14(a.f45,b.f45))return 0;
if(!gg_eq_14(a.f46,b.f46))return 0;
if(!gg_eq_14(a.f47,b.f47))return 0;
if(!gg_eq_14(a.f48,b.f48))return 0;
if(!gg_eq_14(a.f49,b.f49))return 0;
if(!gg_eq_14(a.f50,b.f50))return 0;
if(!gg_eq_14(a.f51,b.f51))return 0;
if(!gg_eq_14(a.f52,b.f52))return 0;
if(!gg_eq_14(a.f53,b.f53))return 0;
return 1;
}
static void gg_render_37(gg_reference out, gg_type_37 value) {
gg_append(out,"\120\154\141\156\040\173\040",7,1);
gg_render_14(out, value.f10);
gg_append(out,", ",2,1);
gg_render_14(out, value.f11);
gg_append(out,", ",2,1);
gg_render_14(out, value.f12);
gg_append(out,", ",2,1);
gg_render_14(out, value.f13);
gg_append(out,", ",2,1);
gg_render_14(out, value.f14);
gg_append(out,", ",2,1);
gg_render_14(out, value.f15);
gg_append(out,", ",2,1);
gg_render_14(out, value.f16);
gg_append(out,", ",2,1);
gg_render_14(out, value.f17);
gg_append(out,", ",2,1);
gg_render_14(out, value.f18);
gg_append(out,", ",2,1);
gg_render_14(out, value.f19);
gg_append(out,", ",2,1);
gg_render_14(out, value.f20);
gg_append(out,", ",2,1);
gg_render_14(out, value.f21);
gg_append(out,", ",2,1);
gg_render_14(out, value.f22);
gg_append(out,", ",2,1);
gg_render_14(out, value.f23);
gg_append(out,", ",2,1);
gg_render_14(out, value.f24);
gg_append(out,", ",2,1);
gg_render_14(out, value.f25);
gg_append(out,", ",2,1);
gg_render_14(out, value.f26);
gg_append(out,", ",2,1);
gg_render_14(out, value.f27);
gg_append(out,", ",2,1);
gg_render_14(out, value.f28);
gg_append(out,", ",2,1);
gg_render_14(out, value.f29);
gg_append(out,", ",2,1);
gg_render_14(out, value.f30);
gg_append(out,", ",2,1);
gg_render_14(out, value.f31);
gg_append(out,", ",2,1);
gg_render_14(out, value.f32);
gg_append(out,", ",2,1);
gg_render_14(out, value.f33);
gg_append(out,", ",2,1);
gg_render_14(out, value.f34);
gg_append(out,", ",2,1);
gg_render_14(out, value.f35);
gg_append(out,", ",2,1);
gg_render_14(out, value.f36);
gg_append(out,", ",2,1);
gg_render_14(out, value.f37);
gg_append(out,", ",2,1);
gg_render_14(out, value.f38);
gg_append(out,", ",2,1);
gg_render_14(out, value.f39);
gg_append(out,", ",2,1);
gg_render_14(out, value.f40);
gg_append(out,", ",2,1);
gg_render_14(out, value.f41);
gg_append(out,", ",2,1);
gg_render_14(out, value.f42);
gg_append(out,", ",2,1);
gg_render_14(out, value.f43);
gg_append(out,", ",2,1);
gg_render_14(out, value.f44);
gg_append(out,", ",2,1);
gg_render_14(out, value.f45);
gg_append(out,", ",2,1);
gg_render_14(out, value.f46);
gg_append(out,", ",2,1);
gg_render_14(out, value.f47);
gg_append(out,", ",2,1);
gg_render_14(out, value.f48);
gg_append(out,", ",2,1);
gg_render_14(out, value.f49);
gg_append(out,", ",2,1);
gg_render_14(out, value.f50);
gg_append(out,", ",2,1);
gg_render_14(out, value.f51);
gg_append(out,", ",2,1);
gg_render_14(out, value.f52);
gg_append(out,", ",2,1);
gg_render_14(out, value.f53);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_38(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_38(((gg_type_38*)a.data)[i],((gg_type_38*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_38(gg_type_38 a, gg_type_38 b) {
if(!gg_eq_25(a.f54,b.f54))return 0;
if(!gg_eq_25(a.f55,b.f55))return 0;
return 1;
}
static void gg_render_38(gg_reference out, gg_type_38 value) {
gg_append(out,"\122\157\165\164\145\040\173\040",8,1);
gg_render_25(out, value.f54);
gg_append(out,", ",2,1);
gg_render_25(out, value.f55);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_39(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_39(((gg_type_39*)a.data)[i],((gg_type_39*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_39(gg_type_39 a, gg_type_39 b) {
if(!gg_eq_3(a.f56,b.f56))return 0;
if(!gg_eq_41(a.f57,b.f57))return 0;
if(!gg_eq_42(a.f58,b.f58))return 0;
return 1;
}
static void gg_render_39(gg_reference out, gg_type_39 value) {
gg_append(out,"\121\070\040\173\040",5,1);
gg_render_3(out, value.f56);
gg_append(out,", ",2,1);
gg_render_41(out, value.f57);
gg_append(out,", ",2,1);
gg_render_42(out, value.f58);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_40(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_40(((gg_type_40*)a.data)[i],((gg_type_40*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_40(gg_type_40 a, gg_type_40 b) {
return gg_equal_elements_9(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_40(gg_reference out, gg_type_40 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_9(out, ((int8_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_41(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_41(((gg_type_41*)a.data)[i],((gg_type_41*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_41(gg_type_41 a, gg_type_41 b) {
return gg_equal_elements_40(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_41(gg_reference out, gg_type_41 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_40(out, ((gg_type_40*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_42(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_42(((gg_type_42*)a.data)[i],((gg_type_42*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_42(gg_type_42 a, gg_type_42 b) {
return gg_equal_elements_36(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_42(gg_reference out, gg_type_42 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_36(out, ((int16_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_43(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_43(((gg_type_43*)a.data)[i],((gg_type_43*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_43(gg_type_43 a, gg_type_43 b) {
if(!gg_eq_44(a.f59,b.f59))return 0;
if(!gg_eq_34(a.f60,b.f60))return 0;
if(!gg_eq_32(a.f61,b.f61))return 0;
return 1;
}
static void gg_render_43(gg_reference out, gg_type_43 value) {
gg_append(out,"\101\143\164\151\166\141\164\151\157\156\124\151\154\145\040\173\040",17,1);
gg_render_44(out, value.f59);
gg_append(out,", ",2,1);
gg_render_34(out, value.f60);
gg_append(out,", ",2,1);
gg_render_32(out, value.f61);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_44(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_44(((gg_type_44*)a.data)[i],((gg_type_44*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_44(gg_type_44 a, gg_type_44 b) {
return gg_equal_elements_41(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_44(gg_reference out, gg_type_44 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_41(out, ((gg_type_41*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_45(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_45(((gg_type_45*)a.data)[i],((gg_type_45*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_45(gg_type_45 a, gg_type_45 b) {
if(!gg_eq_28(a.f62,b.f62))return 0;
if(!gg_eq_43(a.f63,b.f63))return 0;
if(!gg_eq_47(a.f64,b.f64))return 0;
return 1;
}
static void gg_render_45(gg_reference out, gg_type_45 value) {
gg_append(out,"\127\157\162\153\163\160\141\143\145\040\173\040",12,1);
gg_render_28(out, value.f62);
gg_append(out,", ",2,1);
gg_render_43(out, value.f63);
gg_append(out,", ",2,1);
gg_render_47(out, value.f64);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_46(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_46(((gg_type_46*)a.data)[i],((gg_type_46*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_46(gg_type_46 a, gg_type_46 b) {
return gg_equal_elements_3(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_46(gg_reference out, gg_type_46 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_47(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_47(((gg_type_47*)a.data)[i],((gg_type_47*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_47(gg_type_47 a, gg_type_47 b) {
return gg_equal_elements_46(gg_view((a).data, 8192),gg_view((b).data, 8192));
}
static void gg_render_47(gg_reference out, gg_type_47 value) {
int64_t i; gg_slice view=gg_view((value).data, 8192);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_46(out, ((gg_type_46*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_48(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_48(((gg_type_48*)a.data)[i],((gg_type_48*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_48(gg_type_48 a, gg_type_48 b) {
return gg_equal_elements_25(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_48(gg_reference out, gg_type_48 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_25(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_49(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_49(((gg_type_49*)a.data)[i],((gg_type_49*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_49(gg_type_49 a, gg_type_49 b) {
return a == b;
}
static void gg_render_49(gg_reference out, gg_type_49 value) {
gg_render_37(out,*value);
}
static uint8_t gg_equal_elements_50(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_50(((gg_type_50*)a.data)[i],((gg_type_50*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_50(gg_type_50 a, gg_type_50 b) {
return a == b;
}
static void gg_render_50(gg_reference out, gg_type_50 value) {
gg_render_45(out,*value);
}
static uint8_t gg_equal_elements_51(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_51(((gg_type_51*)a.data)[i],((gg_type_51*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_51(gg_type_51 a, gg_type_51 b) {
return gg_equal_elements_3(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_51(gg_reference out, gg_type_51 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static uint8_t gg_function_1(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t base);
static int64_t gg_function_3(int64_t p0, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t base);
static float gg_function_5(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, gg_slice p4, int64_t base);
static uint32_t gg_function_6(gg_slice p0, int64_t p1, int64_t base);
static uint16_t gg_function_7(gg_slice p0, int64_t p1, int64_t base);
static float gg_function_9(uint8_t p0, int64_t base);
static float gg_function_10(uint8_t p0, int64_t base);
static gg_type_27 gg_function_13(int64_t p0, int64_t base);
static void gg_function_14(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, gg_type_35 p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base);
static void gg_function_15(gg_type_35 p0, int64_t p1, int32_t p2, int64_t base);
static void gg_function_16(gg_type_35 p0, int64_t p1, int32_t p2, int64_t base);
static gg_type_29 gg_function_17(gg_slice p0, int64_t p1, int64_t base);
static gg_type_29 gg_function_18(gg_type_29 p216, uint8_t p217, int64_t base);
static gg_type_29 gg_function_19(gg_type_29 p224, uint8_t p225, int64_t base);
static gg_type_29 gg_function_20(gg_type_29 p305, gg_type_29 p306, int64_t base);
static void gg_function_21(gg_type_35 p0, int64_t p1, int64_t p2, gg_type_5 p3, uint8_t p4, int64_t base);
static void gg_function_22(gg_type_35 p0, int64_t p1, int64_t p2, uint8_t p3, int64_t base);
static void gg_function_23(gg_type_35 p0, int64_t p1, int64_t p2, gg_type_10 p3, int16_t p4, int64_t base);
static void gg_function_22(gg_type_35 p0, int64_t p1, int64_t p2, uint8_t p3, int64_t base) {
  int64_t t0;
  gg_type_30 * t1;
  gg_slice t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  gg_type_29 * t6;
  gg_slice t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t * t11;
  gg_type_35 v0;
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
  t1 = ((gg_type_30 *)(((*(v0))).f5).data) + gg_index(t0, ((int64_t)UINT64_C(256)));
  t2 = gg_view(((*t1)).data, 8);
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(32))));
  t5 = ((int64_t)gg_div(t3, t4));
  t6 = ((gg_type_29 *)t2.data) + gg_index(t5, t2.len);
  t7 = gg_view(((*t6)).data, 32);
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(32))));
  t10 = ((int64_t)gg_mod(t8, t9));
  t11 = ((uint8_t *)t7.data) + gg_index(t10, t7.len);
  (*t11) = ((uint8_t)(v3));
  }
gg_done:;
}
static void gg_function_23(gg_type_35 p0, int64_t p1, int64_t p2, gg_type_10 p3, int16_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  gg_type_35 t2;
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
  gg_type_35 v0;
  int64_t v1;
  int64_t v2;
  gg_type_10 v3;
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
  gg_function_22(t2, t3, t6, t15, base + 0);
  }
label1:;
  t0++; goto label0;
label2:;
  }
gg_done:;
}
static void gg_function_16(gg_type_35 p0, int64_t p1, int32_t p2, int64_t base) {
  gg_slice t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  gg_type_33 * t4;
  gg_slice t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t * t9;
  gg_type_35 v0;
  int64_t v1;
  int32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = gg_view((((*(v0))).f8).data, 512);
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)gg_div(t1, t2));
  t4 = ((gg_type_33 *)t0.data) + gg_index(t3, t0.len);
  t5 = gg_view(((*t4)).data, 8);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)gg_mod(t6, t7));
  t9 = ((int32_t *)t5.data) + gg_index(t8, t5.len);
  (*t9) = ((int32_t)(v2));
  }
gg_done:;
}
static uint16_t gg_function_7(gg_slice p0, int64_t p1, int64_t base) {
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
static void gg_function_21(gg_type_35 p0, int64_t p1, int64_t p2, gg_type_5 p3, uint8_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  uint8_t t4;
  uint8_t t5;
  uint8_t t6;
  uint8_t t7;
  gg_type_35 t8;
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
  gg_type_35 v0;
  int64_t v1;
  int64_t v2;
  gg_type_5 v3;
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
  gg_function_22(t8, t9, t12, t21, base + 0);
  }
label1:;
  t0++; goto label0;
label2:;
  }
gg_done:;
}
static uint32_t gg_function_6(gg_slice p0, int64_t p1, int64_t base) {
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
  t3 = gg_function_7(t1, t2, base + 0);
  t4 = ((uint32_t)(((uint32_t)(t3))));
  t5 = v0;
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t9 = gg_function_7(t5, t8, base + 0);
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
static gg_type_29 gg_function_20(gg_type_29 p305, gg_type_29 p306, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_29 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)p305.data), _mm256_loadu_si256((const __m256i*)p306.data)));
    return packed_result;
  }
#endif
  gg_type_29 t0;
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
  gg_type_29 v305;
  gg_type_29 v306;
  gg_type_29 v307;
  int64_t v309;
  int64_t v310;
  v305 = p305;
  v306 = p306;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v307).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v309 = t4;
  {
  t6 = ((int64_t)(v309));
  t7 = ((int64_t)(((int64_t)UINT64_C(16))));
  t8 = ((int64_t)(((int64_t)gg_div(t6, t7))));
  t9 = ((int64_t)(((int64_t)UINT64_C(16))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t11 = gg_view((v306).data, 32);
  t12 = v309;
  t13 = ((uint8_t *)t11.data) + gg_index(t12, t11.len);
  t14 = ((uint8_t)((*t13)));
  t15 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t16 = ((int64_t)(((int64_t)(((t14) & (t15))))));
  v310 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t16)))));
  t17 = gg_view((v307).data, 32);
  t18 = v309;
  t19 = ((uint8_t *)t17.data) + gg_index(t18, t17.len);
  t20 = gg_view((v306).data, 32);
  t21 = v309;
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
  t28 = gg_view((v305).data, 32);
  t29 = v310;
  t30 = ((uint8_t *)t28.data) + gg_index(t29, t28.len);
  (*t19) = ((uint8_t)((*t30)));
  }
  }
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
/* FG_PACKED_MATCH:shr8 */
static gg_type_29 gg_function_19(gg_type_29 p224, uint8_t p225, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_29 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)p224.data), _mm_cvtsi32_si128((p225 & 7))), _mm256_set1_epi8((unsigned char)(255u >> (p225 & 7)))));
    return packed_result;
  }
#endif
  gg_type_29 t0;
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
  gg_type_29 v224;
  uint8_t v225;
  gg_type_29 v226;
  int64_t v228;
  v224 = p224;
  v225 = p225;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v226).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v228 = t4;
  {
  t6 = gg_view((v226).data, 32);
  t7 = v228;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v224).data, 32);
  t10 = v228;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v225));
  (*t8) = ((uint8_t)(((t12) >> ((uint64_t)(t13) & 7))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v226;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:and8 */
static gg_type_29 gg_function_18(gg_type_29 p216, uint8_t p217, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_29 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_loadu_si256((const __m256i*)p216.data), _mm256_set1_epi8(p217)));
    return packed_result;
  }
#endif
  gg_type_29 t0;
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
  gg_type_29 v216;
  uint8_t v217;
  gg_type_29 v218;
  int64_t v220;
  v216 = p216;
  v217 = p217;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v218).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v220 = t4;
  {
  t6 = gg_view((v218).data, 32);
  t7 = v220;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v216).data, 32);
  t10 = v220;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v217));
  (*t8) = ((uint8_t)(((t12) & (t13))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v218;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_29 gg_function_17(gg_slice p0, int64_t p1, int64_t base) {
  gg_type_29 t0;
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
  gg_type_29 v3;
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
static void gg_function_15(gg_type_35 p0, int64_t p1, int32_t p2, int64_t base) {
  gg_slice t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  gg_type_33 * t4;
  gg_slice t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t * t9;
  gg_type_35 v0;
  int64_t v1;
  int32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = gg_view((((*(v0))).f9).data, 512);
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)gg_div(t1, t2));
  t4 = ((gg_type_33 *)t0.data) + gg_index(t3, t0.len);
  t5 = gg_view(((*t4)).data, 8);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)gg_mod(t6, t7));
  t9 = ((int32_t *)t5.data) + gg_index(t8, t5.len);
  (*t9) = ((int32_t)(v2));
  }
gg_done:;
}
static gg_type_27 gg_function_13(int64_t p0, int64_t base) {
  gg_type_27 t0;
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
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(144))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(1))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(176))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(1))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(210))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(3))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(110))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(4))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(84))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(1))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(5))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(136))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(6))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(66))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(7))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(74))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(8))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(82))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(9))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(98))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(10))));
  t39 = ((t37) == (t38));
  if (t39) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(110))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(11))));
  t42 = ((t40) == (t41));
  if (t42) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(50))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(32))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(((int64_t)UINT64_C(56))));
  (t0).f1 = ((int64_t)(((int64_t)UINT64_C(16))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(128))));
  (t0).f4 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_14(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, gg_type_35 p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, gg_slice p11, gg_slice p12, gg_slice p13, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  float * t6;
  int64_t t7;
  int64_t t8;
  gg_type_35 t9;
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
  gg_type_35 t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  uint8_t t63;
  int32_t t64;
  gg_type_35 t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  uint8_t t70;
  int32_t t71;
  gg_type_35 t72;
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
  gg_type_35 t87;
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
  gg_type_29 t116;
  uint8_t t117;
  gg_type_29 t118;
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
  gg_type_30 * t164;
  gg_slice t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  gg_type_29 * t169;
  int64_t t170;
  gg_type_30 * t171;
  gg_slice t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  gg_type_29 * t178;
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
  gg_type_35 t192;
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
  gg_type_30 * t240;
  gg_slice t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  gg_type_29 * t245;
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
  gg_type_30 * t258;
  gg_slice t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  gg_type_29 * t265;
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
  gg_type_30 * t278;
  gg_slice t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  gg_type_29 * t285;
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
  gg_type_30 * t296;
  gg_slice t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  gg_type_29 * t303;
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
  gg_type_35 t349;
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
  gg_type_35 t364;
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
  gg_type_35 t383;
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
  gg_type_35 t402;
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
  gg_type_30 * t465;
  gg_slice t466;
  int64_t t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
  gg_type_29 * t472;
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
  gg_type_35 t494;
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
  gg_type_35 t506;
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
  gg_type_30 * t531;
  gg_slice t532;
  int64_t t533;
  int64_t t534;
  int64_t t535;
  int64_t t536;
  int64_t t537;
  gg_type_29 * t538;
  gg_type_29 t539;
  int64_t t540;
  int64_t t541;
  uint8_t t542;
  gg_type_29 t543;
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
  gg_type_35 t577;
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
  gg_type_29 t620;
  gg_type_29 t621;
  uint8_t t622;
  gg_type_29 t623;
  gg_type_29 t624;
  gg_type_29 t625;
  uint8_t t626;
  gg_type_29 t627;
  int64_t t628;
  int64_t t629;
  int64_t t630;
  gg_type_30 * t631;
  gg_slice t632;
  int64_t t633;
  int64_t t634;
  int64_t t635;
  gg_type_29 * t636;
  gg_slice t637;
  int64_t t638;
  uint8_t * t639;
  int64_t t640;
  uint8_t * t641;
  int64_t t642;
  gg_type_30 * t643;
  gg_slice t644;
  int64_t t645;
  int64_t t646;
  int64_t t647;
  gg_type_29 * t648;
  gg_slice t649;
  int64_t t650;
  int64_t t651;
  int64_t t652;
  uint8_t * t653;
  int64_t t654;
  uint8_t * t655;
  int64_t t656;
  gg_type_30 * t657;
  gg_slice t658;
  int64_t t659;
  int64_t t660;
  int64_t t661;
  int64_t t662;
  int64_t t663;
  gg_type_29 * t664;
  gg_slice t665;
  int64_t t666;
  uint8_t * t667;
  gg_slice t668;
  int64_t t669;
  int64_t t670;
  int64_t t671;
  uint8_t * t672;
  int64_t t673;
  gg_type_30 * t674;
  gg_slice t675;
  int64_t t676;
  int64_t t677;
  int64_t t678;
  int64_t t679;
  int64_t t680;
  gg_type_29 * t681;
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
  gg_type_35 t715;
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
  gg_type_35 t728;
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
  gg_type_5 * t750;
  gg_type_5 t751;
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
  gg_type_35 t785;
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
  gg_type_35 t803;
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
  gg_type_35 t840;
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
  gg_type_5 * t853;
  gg_type_5 t854;
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
  gg_type_35 t889;
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
  gg_type_5 * t900;
  gg_type_5 t901;
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
  gg_type_35 t935;
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
  gg_type_7 * t963;
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
  gg_type_7 * t981;
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
  gg_type_35 t998;
  int64_t t999;
  int64_t t1000;
  int64_t t1001;
  int64_t t1002;
  int64_t t1003;
  int64_t t1004;
  int64_t t1005;
  int64_t t1006;
  gg_type_5 t1007;
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
  gg_type_35 t1038;
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
  gg_type_7 * t1114;
  gg_slice t1115;
  int64_t t1116;
  gg_type_7 * t1117;
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
  gg_type_35 t1134;
  int64_t t1135;
  int64_t t1136;
  int64_t t1137;
  int64_t t1138;
  int64_t t1139;
  int64_t t1140;
  int64_t t1141;
  int64_t t1142;
  gg_type_5 t1143;
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
  gg_type_35 t1177;
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
  gg_type_35 t1219;
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
  gg_type_10 * t1230;
  gg_type_10 t1231;
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
  gg_type_35 t1291;
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
  gg_type_35 t1306;
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
  gg_type_35 t1375;
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
  gg_type_10 * t1386;
  gg_type_10 t1387;
  int16_t t1388;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  gg_type_35 v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  gg_slice v10;
  gg_slice v11;
  gg_slice v12;
  gg_slice v13;
  gg_type_27 v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  uint8_t v19;
  uint8_t v20;
  uint8_t v21;
  int64_t v22;
  int64_t v23;
  gg_type_29 v24;
  gg_type_29 v25;
  gg_type_29 v26;
  gg_type_29 v27;
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
  gg_type_29 v41;
  gg_type_29 v42;
  gg_type_29 v43;
  int64_t v44;
  int64_t v45;
  int64_t v46;
  gg_type_29 v47;
  int64_t v48;
  uint16_t v49;
  int64_t v50;
  uint16_t v51;
  gg_type_29 v52;
  int64_t v53;
  uint8_t v54;
  int64_t v55;
  gg_type_29 v56;
  gg_type_29 v57;
  gg_type_29 v58;
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
  gg_type_7 v71;
  gg_type_7 v72;
  gg_type_5 v73;
  int64_t v74;
  int64_t v75;
  uint8_t v76;
  int64_t v77;
  uint16_t v78;
  uint16_t v79;
  uint16_t v80;
  gg_type_7 v81;
  gg_type_7 v82;
  gg_type_5 v83;
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
  v14 = gg_function_13(t0, base + 0);
  t1 = ((int64_t)(((int64_t)UINT64_C(256))));
  t2 = ((int64_t)((v14).f1));
  v15 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v15));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  /* FG_DIRECT_SLICE_INDEX */
  t5 = v4;
  t6 = ((float *)(((*(v5))).f7).data) + gg_index(t5, ((int64_t)UINT64_C(256)));
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
  gg_function_15(t9, t12, t13, base + 0);
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
  t22 = ((float *)(((*(v5))).f6).data) + gg_index(t21, ((int64_t)UINT64_C(256)));
  t23 = v1;
  t24 = ((int64_t)(v2));
  t25 = gg_function_7(t23, t24, base + 0);
  t26 = fg141_half(t25);
  (*t22) = ((float)(t26));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = v4;
  t28 = ((float *)(((*(v5))).f7).data) + gg_index(t27, ((int64_t)UINT64_C(256)));
  t29 = v1;
  t30 = ((int64_t)(v2));
  t31 = ((int64_t)(((int64_t)UINT64_C(2))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  t33 = gg_function_7(t29, t32, base + 0);
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
  gg_function_16(t58, t61, t64, base + 0);
  t65 = v5;
  t66 = ((int64_t)(v16));
  t67 = ((int64_t)(v18));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t69 = ((uint8_t)(v20));
  t70 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t71 = ((int32_t)(((int32_t)(((t69) & (t70))))));
  gg_function_15(t65, t68, t71, base + 0);
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
  gg_function_16(t72, t77, t86, base + 0);
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
  gg_function_15(t87, t92, t101, base + 0);
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
  v24 = gg_function_17(t110, t115, base + 0);
  t116 = v24;
  t117 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v25 = gg_function_18(t116, t117, base + 0);
  t118 = v24;
  t119 = ((uint8_t)(((int64_t)UINT64_C(4))));
  v26 = gg_function_19(t118, t119, base + 0);
  t120 = ((int64_t)(v0));
  t121 = ((int64_t)(((int64_t)UINT64_C(1))));
  t122 = ((t120) == (t121));
  if (t122) {
  {
  t123 = v1;
  t124 = ((int64_t)(v2));
  t125 = ((int64_t)(((int64_t)UINT64_C(16))));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t124) + (uint64_t)(t125)))));
  v27 = gg_function_17(t123, t126, base + 0);
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
  t164 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t163, ((int64_t)UINT64_C(256)));
  t165 = gg_view(((*t164)).data, 8);
  t166 = ((int64_t)(((int64_t)UINT64_C(2))));
  t167 = ((int64_t)(v23));
  t168 = ((int64_t)((uint64_t)(t166) * (uint64_t)(t167)));
  t169 = ((gg_type_29 *)t165.data) + gg_index(t168, t165.len);
  (*t169) = v25;
  /* FG_DIRECT_SLICE_INDEX */
  t170 = v3;
  t171 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t170, ((int64_t)UINT64_C(256)));
  t172 = gg_view(((*t171)).data, 8);
  t173 = ((int64_t)(((int64_t)UINT64_C(2))));
  t174 = ((int64_t)(v23));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) * (uint64_t)(t174)))));
  t176 = ((int64_t)(((int64_t)UINT64_C(1))));
  t177 = ((int64_t)((uint64_t)(t175) + (uint64_t)(t176)));
  t178 = ((gg_type_29 *)t172.data) + gg_index(t177, t172.len);
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
  t183 = ((float *)(((*(v5))).f6).data) + gg_index(t182, ((int64_t)UINT64_C(256)));
  t184 = v1;
  t185 = ((int64_t)(v2));
  t186 = ((int64_t)(((int64_t)UINT64_C(208))));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t185) + (uint64_t)(t186)))));
  t188 = gg_function_7(t184, t187, base + 0);
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
  gg_function_16(t192, t195, t203, base + 0);
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
  t240 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t239, ((int64_t)UINT64_C(256)));
  t241 = gg_view(((*t240)).data, 8);
  t242 = ((int64_t)(v30));
  t243 = ((int64_t)(((int64_t)UINT64_C(4))));
  t244 = ((int64_t)((uint64_t)(t242) * (uint64_t)(t243)));
  t245 = ((gg_type_29 *)t241.data) + gg_index(t244, t241.len);
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
  t258 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t257, ((int64_t)UINT64_C(256)));
  t259 = gg_view(((*t258)).data, 8);
  t260 = ((int64_t)(v30));
  t261 = ((int64_t)(((int64_t)UINT64_C(4))));
  t262 = ((int64_t)(((int64_t)((uint64_t)(t260) * (uint64_t)(t261)))));
  t263 = ((int64_t)(((int64_t)UINT64_C(1))));
  t264 = ((int64_t)((uint64_t)(t262) + (uint64_t)(t263)));
  t265 = ((gg_type_29 *)t259.data) + gg_index(t264, t259.len);
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
  t278 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t277, ((int64_t)UINT64_C(256)));
  t279 = gg_view(((*t278)).data, 8);
  t280 = ((int64_t)(v30));
  t281 = ((int64_t)(((int64_t)UINT64_C(4))));
  t282 = ((int64_t)(((int64_t)((uint64_t)(t280) * (uint64_t)(t281)))));
  t283 = ((int64_t)(((int64_t)UINT64_C(2))));
  t284 = ((int64_t)((uint64_t)(t282) + (uint64_t)(t283)));
  t285 = ((gg_type_29 *)t279.data) + gg_index(t284, t279.len);
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
  t296 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t295, ((int64_t)UINT64_C(256)));
  t297 = gg_view(((*t296)).data, 8);
  t298 = ((int64_t)(v30));
  t299 = ((int64_t)(((int64_t)UINT64_C(4))));
  t300 = ((int64_t)(((int64_t)((uint64_t)(t298) * (uint64_t)(t299)))));
  t301 = ((int64_t)(((int64_t)UINT64_C(3))));
  t302 = ((int64_t)((uint64_t)(t300) + (uint64_t)(t301)));
  t303 = ((gg_type_29 *)t297.data) + gg_index(t302, t297.len);
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
  t319 = ((float *)(((*(v5))).f6).data) + gg_index(t318, ((int64_t)UINT64_C(256)));
  t320 = v1;
  t321 = ((int64_t)(v2));
  t322 = ((int64_t)(((int64_t)UINT64_C(108))));
  t323 = ((int64_t)(((int64_t)((uint64_t)(t321) + (uint64_t)(t322)))));
  t324 = gg_function_7(t320, t323, base + 0);
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
  gg_function_16(t349, t352, t363, base + 0);
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
  gg_function_16(t364, t369, t382, base + 0);
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
  gg_function_16(t383, t388, t401, base + 0);
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
  gg_function_16(t402, t407, t420, base + 0);
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
  v41 = gg_function_17(t425, t432, base + 0);
  t433 = v1;
  t434 = ((int64_t)(v2));
  v42 = gg_function_17(t433, t434, base + 0);
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
  t465 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t464, ((int64_t)UINT64_C(256)));
  t466 = gg_view(((*t465)).data, 8);
  t467 = ((int64_t)(v39));
  t468 = ((int64_t)(((int64_t)UINT64_C(4))));
  t469 = ((int64_t)(((int64_t)((uint64_t)(t467) * (uint64_t)(t468)))));
  t470 = ((int64_t)(v40));
  t471 = ((int64_t)((uint64_t)(t469) + (uint64_t)(t470)));
  t472 = ((gg_type_29 *)t466.data) + gg_index(t471, t466.len);
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
  t477 = ((float *)(((*(v5))).f6).data) + gg_index(t476, ((int64_t)UINT64_C(256)));
  t478 = v1;
  t479 = ((int64_t)(v2));
  t480 = ((int64_t)(((int64_t)UINT64_C(80))));
  t481 = ((int64_t)(((int64_t)((uint64_t)(t479) + (uint64_t)(t480)))));
  t482 = gg_function_7(t478, t481, base + 0);
  t483 = fg141_half(t482);
  (*t477) = ((float)(t483));
  /* FG_DIRECT_SLICE_INDEX */
  t484 = v4;
  t485 = ((float *)(((*(v5))).f7).data) + gg_index(t484, ((int64_t)UINT64_C(256)));
  t486 = v1;
  t487 = ((int64_t)(v2));
  t488 = ((int64_t)(((int64_t)UINT64_C(82))));
  t489 = ((int64_t)(((int64_t)((uint64_t)(t487) + (uint64_t)(t488)))));
  t490 = gg_function_7(t486, t489, base + 0);
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
  gg_function_16(t494, t497, t505, base + 0);
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
  gg_function_15(t506, t509, t517, base + 0);
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
  v47 = gg_function_17(t520, t527, base + 0);
  t529 = ((int64_t)UINT64_C(4));
  t528 = 0;
label39:;
  if (t528 >= t529) goto label41;
  v48 = t528;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t530 = v3;
  t531 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t530, ((int64_t)UINT64_C(256)));
  t532 = gg_view(((*t531)).data, 8);
  t533 = ((int64_t)(v46));
  t534 = ((int64_t)(((int64_t)UINT64_C(4))));
  t535 = ((int64_t)(((int64_t)((uint64_t)(t533) * (uint64_t)(t534)))));
  t536 = ((int64_t)(v48));
  t537 = ((int64_t)((uint64_t)(t535) + (uint64_t)(t536)));
  t538 = ((gg_type_29 *)t532.data) + gg_index(t537, t532.len);
  t539 = v47;
  t540 = ((int64_t)(((int64_t)UINT64_C(2))));
  t541 = ((int64_t)(v48));
  t542 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t540) * (uint64_t)(t541)))))));
  t543 = gg_function_19(t539, t542, base + 0);
  t544 = ((uint8_t)(((int64_t)UINT64_C(3))));
  (*t538) = gg_function_18(t543, t544, base + 0);
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
  t549 = ((float *)(((*(v5))).f6).data) + gg_index(t548, ((int64_t)UINT64_C(256)));
  t550 = v1;
  t551 = ((int64_t)(v2));
  t552 = gg_function_7(t550, t551, base + 0);
  t553 = fg141_half(t552);
  (*t549) = ((float)(t553));
  t554 = v1;
  t555 = ((int64_t)(v2));
  t556 = ((int64_t)(((int64_t)UINT64_C(2))));
  t557 = ((int64_t)(((int64_t)((uint64_t)(t555) + (uint64_t)(t556)))));
  v49 = gg_function_7(t554, t557, base + 0);
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
  gg_function_16(t577, t580, t593, base + 0);
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
  v56 = gg_function_17(t612, t619, base + 0);
  t620 = v52;
  t621 = v56;
  t622 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t623 = gg_function_18(t621, t622, base + 0);
  v57 = gg_function_20(t620, t623, base + 0);
  t624 = v52;
  t625 = v56;
  t626 = ((uint8_t)(((int64_t)UINT64_C(4))));
  t627 = gg_function_19(t625, t626, base + 0);
  v58 = gg_function_20(t624, t627, base + 0);
  t629 = ((int64_t)UINT64_C(16));
  t628 = 0;
label51:;
  if (t628 >= t629) goto label53;
  v59 = t628;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t630 = v3;
  t631 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t630, ((int64_t)UINT64_C(256)));
  t632 = gg_view(((*t631)).data, 8);
  t633 = ((int64_t)(((int64_t)UINT64_C(2))));
  t634 = ((int64_t)(v55));
  t635 = ((int64_t)((uint64_t)(t633) * (uint64_t)(t634)));
  t636 = ((gg_type_29 *)t632.data) + gg_index(t635, t632.len);
  t637 = gg_view(((*t636)).data, 32);
  t638 = v59;
  t639 = ((uint8_t *)t637.data) + gg_index(t638, t637.len);
  /* FG_DIRECT_SLICE_INDEX */
  t640 = v59;
  t641 = ((uint8_t *)(v57).data) + gg_index(t640, ((int64_t)UINT64_C(32)));
  (*t639) = ((uint8_t)((*t641)));
  /* FG_DIRECT_SLICE_INDEX */
  t642 = v3;
  t643 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t642, ((int64_t)UINT64_C(256)));
  t644 = gg_view(((*t643)).data, 8);
  t645 = ((int64_t)(((int64_t)UINT64_C(2))));
  t646 = ((int64_t)(v55));
  t647 = ((int64_t)((uint64_t)(t645) * (uint64_t)(t646)));
  t648 = ((gg_type_29 *)t644.data) + gg_index(t647, t644.len);
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
  t657 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t656, ((int64_t)UINT64_C(256)));
  t658 = gg_view(((*t657)).data, 8);
  t659 = ((int64_t)(((int64_t)UINT64_C(2))));
  t660 = ((int64_t)(v55));
  t661 = ((int64_t)(((int64_t)((uint64_t)(t659) * (uint64_t)(t660)))));
  t662 = ((int64_t)(((int64_t)UINT64_C(1))));
  t663 = ((int64_t)((uint64_t)(t661) + (uint64_t)(t662)));
  t664 = ((gg_type_29 *)t658.data) + gg_index(t663, t658.len);
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
  t674 = ((gg_type_30 *)(((*(v5))).f5).data) + gg_index(t673, ((int64_t)UINT64_C(256)));
  t675 = gg_view(((*t674)).data, 8);
  t676 = ((int64_t)(((int64_t)UINT64_C(2))));
  t677 = ((int64_t)(v55));
  t678 = ((int64_t)(((int64_t)((uint64_t)(t676) * (uint64_t)(t677)))));
  t679 = ((int64_t)(((int64_t)UINT64_C(1))));
  t680 = ((int64_t)((uint64_t)(t678) + (uint64_t)(t679)));
  t681 = ((gg_type_29 *)t675.data) + gg_index(t680, t675.len);
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
  t696 = ((float *)(((*(v5))).f6).data) + gg_index(t695, ((int64_t)UINT64_C(256)));
  t697 = v1;
  t698 = ((int64_t)(v2));
  t699 = gg_function_7(t697, t698, base + 0);
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
  v61 = gg_function_6(t705, t714, base + 0);
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
  gg_function_16(t715, t718, t725, base + 0);
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
  t750 = ((gg_type_5 *)t737.data) + gg_index(t749, t737.len);
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
  gg_function_21(t728, t729, t736, t751, t761, base + 0);
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
  t770 = ((float *)(((*(v5))).f6).data) + gg_index(t769, ((int64_t)UINT64_C(256)));
  t771 = v1;
  t772 = ((int64_t)(v2));
  t773 = gg_function_7(t771, t772, base + 0);
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
  gg_function_16(t785, t790, t802, base + 0);
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
  gg_function_16(t803, t810, t822, base + 0);
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
  v66 = gg_function_7(t828, t839, base + 0);
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
  t853 = ((gg_type_5 *)t849.data) + gg_index(t852, t849.len);
  t854 = (*t853);
  t855 = v12;
  t856 = ((uint16_t)(v66));
  t857 = ((int64_t)(((int64_t)UINT64_C(9))));
  t858 = ((int64_t)(((t856) >> ((uint64_t)(t857) & 15))));
  t859 = ((uint8_t *)t855.data) + gg_index(t858, t855.len);
  t860 = ((uint8_t)((*t859)));
  gg_function_21(t840, t841, t848, t854, t860, base + 0);
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
  t900 = ((gg_type_5 *)t898.data) + gg_index(t899, t898.len);
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
  gg_function_21(t889, t890, t897, t901, t913, base + 0);
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
  t918 = ((float *)(((*(v5))).f6).data) + gg_index(t917, ((int64_t)UINT64_C(256)));
  t919 = v1;
  t920 = ((int64_t)(v2));
  t921 = gg_function_7(t919, t920, base + 0);
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
  v69 = gg_function_6(t927, t934, base + 0);
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
  gg_function_16(t935, t938, t945, base + 0);
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
  t963 = ((gg_type_7 *)t948.data) + gg_index(t962, t948.len);
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
  t981 = ((gg_type_7 *)t964.data) + gg_index(t980, t964.len);
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
  gg_function_21(t998, t999, t1006, t1007, t1017, base + 0);
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
  t1022 = ((float *)(((*(v5))).f6).data) + gg_index(t1021, ((int64_t)UINT64_C(256)));
  t1023 = v1;
  t1024 = ((int64_t)(v2));
  t1025 = gg_function_7(t1023, t1024, base + 0);
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
  gg_function_16(t1038, t1041, t1054, base + 0);
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
  t1114 = ((gg_type_7 *)t1112.data) + gg_index(t1113, t1112.len);
  v81 = (*t1114);
  t1115 = v10;
  t1116 = ((int64_t)(v80));
  t1117 = ((gg_type_7 *)t1115.data) + gg_index(t1116, t1115.len);
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
  gg_function_21(t1134, t1135, t1142, t1143, t1155, base + 0);
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
  t1160 = ((float *)(((*(v5))).f6).data) + gg_index(t1159, ((int64_t)UINT64_C(256)));
  t1161 = v1;
  t1162 = ((int64_t)(v2));
  t1163 = gg_function_7(t1161, t1162, base + 0);
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
  v86 = gg_function_7(t1169, t1176, base + 0);
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
  gg_function_16(t1177, t1180, t1189, base + 0);
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
  t1230 = ((gg_type_10 *)t1228.data) + gg_index(t1229, t1228.len);
  t1231 = (*t1230);
  t1232 = ((int16_t)(v87));
  gg_function_23(t1219, t1220, t1227, t1231, t1232, base + 0);
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
  v90 = gg_function_7(t1235, t1238, base + 0);
  t1239 = v1;
  t1240 = ((int64_t)(v2));
  t1241 = ((int64_t)(((int64_t)UINT64_C(50))));
  t1242 = ((int64_t)(((int64_t)((uint64_t)(t1240) + (uint64_t)(t1241)))));
  v91 = gg_function_7(t1239, t1242, base + 0);
  t1243 = v1;
  t1244 = ((int64_t)(v2));
  t1245 = ((int64_t)(((int64_t)UINT64_C(52))));
  t1246 = ((int64_t)(((int64_t)((uint64_t)(t1244) + (uint64_t)(t1245)))));
  v92 = gg_function_7(t1243, t1246, base + 0);
  t1247 = v1;
  t1248 = ((int64_t)(v2));
  t1249 = ((int64_t)(((int64_t)UINT64_C(54))));
  t1250 = ((int64_t)(((int64_t)((uint64_t)(t1248) + (uint64_t)(t1249)))));
  v93 = gg_function_7(t1247, t1250, base + 0);
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
  t1270 = ((float *)(((*(v5))).f6).data) + gg_index(t1269, ((int64_t)UINT64_C(256)));
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
  v96 = gg_function_7(t1277, t1286, base + 0);
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
  gg_function_16(t1291, t1296, t1305, base + 0);
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
  gg_function_16(t1306, t1313, t1324, base + 0);
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
  t1386 = ((gg_type_10 *)t1384.data) + gg_index(t1385, t1384.len);
  t1387 = (*t1386);
  t1388 = ((int16_t)(v102));
  gg_function_23(t1375, t1376, t1383, t1387, t1388, base + 0);
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
static float gg_function_10(uint8_t p0, int64_t base) {
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
static float gg_function_9(uint8_t p0, int64_t base) {
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
static float gg_function_5(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, gg_slice p4, int64_t base) {
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
  t6 = gg_function_6(t4, t5, base + 0);
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
  t51 = gg_function_9(t50, base + 0);
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
  t82 = gg_function_9(t81, base + 0);
  t83 = ((float)(t82));
  t84 = v1;
  t85 = ((int64_t)(v2));
  t86 = ((int64_t)(v9));
  t87 = ((int64_t)((uint64_t)(t85) + (uint64_t)(t86)));
  t88 = ((uint8_t *)t84.data) + gg_index(t87, t84.len);
  t89 = ((uint8_t)((*t88)));
  t90 = gg_function_10(t89, base + 0);
  t91 = ((float)(t90));
  t0 = ((float)(((t83) * (t91))));
  goto gg_done;
  }
  }
  t92 = v1;
  t93 = ((int64_t)(v2));
  t94 = gg_function_7(t92, t93, base + 0);
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
  t219 = gg_function_7(t215, t218, base + 0);
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
  v19 = gg_function_6(t228, t235, base + 0);
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
  t279 = gg_function_7(t275, t278, base + 0);
  t280 = fg141_half(t279);
  t281 = ((float)(t280));
  t0 = ((float)(((t274) + (t281))));
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
static int64_t gg_function_3(int64_t p0, int64_t base) {
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
static int64_t gg_function_2(int64_t p0, int64_t base) {
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
static uint8_t gg_function_1(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, gg_slice p10, int64_t base) {
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
  gg_type_30 t55;
  gg_type_29 t56;
  uint8_t t57;
  uint8_t t58;
  int64_t t59;
  gg_type_29 t60;
  int64_t t61;
  gg_type_30 t62;
  int64_t t63;
  float t64;
  float t65;
  int64_t t66;
  float t67;
  float t68;
  int64_t t69;
  gg_type_33 t70;
  int32_t t71;
  int32_t t72;
  int64_t t73;
  gg_type_33 t74;
  int64_t t75;
  gg_type_33 t76;
  int32_t t77;
  int32_t t78;
  int64_t t79;
  gg_type_33 t80;
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
  gg_type_35 t93;
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
  gg_type_33 * t110;
  gg_slice t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int32_t * t115;
  gg_slice t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  gg_type_33 * t120;
  gg_slice t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int32_t * t125;
  int64_t t126;
  gg_type_30 * t127;
  gg_slice t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  gg_type_29 * t132;
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
  gg_type_27 v16;
  gg_type_28 v17;
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
  v11 = gg_function_2(t1, base + 0);
  t2 = ((int64_t)(v11));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) < (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v0));
  v12 = gg_function_3(t5, base + 0);
  t6 = ((int64_t)(v0));
  v13 = gg_function_4(t6, base + 0);
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
  (*t34) = gg_function_5(t35, t36, t39, t40, t41, base + 0);
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
  v16 = gg_function_13(t42, base + 0);
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
  t53 = ((int64_t)((v16).f0));
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
  for (t63=0; t63<256; t63++) ((v17).f5).data[t63] = t62;
  memset(&t64, 0, sizeof(t64));
  t65 = ((float)(t64));
  for (t66=0; t66<256; t66++) ((v17).f6).data[t66] = t65;
  memset(&t67, 0, sizeof(t67));
  t68 = ((float)(t67));
  for (t69=0; t69<256; t69++) ((v17).f7).data[t69] = t68;
  memset(&t71, 0, sizeof(t71));
  t72 = ((int32_t)(t71));
  for (t73=0; t73<8; t73++) (t70).data[t73] = t72;
  t74 = t70;
  for (t75=0; t75<512; t75++) ((v17).f8).data[t75] = t74;
  memset(&t77, 0, sizeof(t77));
  t78 = ((int32_t)(t77));
  for (t79=0; t79<8; t79++) (t76).data[t79] = t78;
  t80 = t76;
  for (t81=0; t81<512; t81++) ((v17).f9).data[t81] = t80;
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
  t89 = ((int64_t)((v16).f0));
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
  gg_function_14(t86, t87, t90, t91, t92, t93, t94, t95, t96, t97, t98, t99, t100, t101, base + 0);
  t103 = ((int64_t)UINT64_C(256));
  t102 = 0;
label9:;
  if (t102 >= t103) goto label11;
  v19 = t102;
  {
  t104 = ((int64_t)(v19));
  t105 = ((int64_t)((v16).f1));
  v20 = ((int64_t)(((int64_t)gg_div(t104, t105))));
  t106 = gg_view(((v17).f8).data, 512);
  t107 = ((int64_t)(v20));
  t108 = ((int64_t)(((int64_t)UINT64_C(8))));
  t109 = ((int64_t)gg_div(t107, t108));
  t110 = ((gg_type_33 *)t106.data) + gg_index(t109, t106.len);
  t111 = gg_view(((*t110)).data, 8);
  t112 = ((int64_t)(v20));
  t113 = ((int64_t)(((int64_t)UINT64_C(8))));
  t114 = ((int64_t)gg_mod(t112, t113));
  t115 = ((int32_t *)t111.data) + gg_index(t114, t111.len);
  v21 = ((float)(((float)((*t115)))));
  t116 = gg_view(((v17).f9).data, 512);
  t117 = ((int64_t)(v20));
  t118 = ((int64_t)(((int64_t)UINT64_C(8))));
  t119 = ((int64_t)gg_div(t117, t118));
  t120 = ((gg_type_33 *)t116.data) + gg_index(t119, t116.len);
  t121 = gg_view(((*t120)).data, 8);
  t122 = ((int64_t)(v20));
  t123 = ((int64_t)(((int64_t)UINT64_C(8))));
  t124 = ((int64_t)gg_mod(t122, t123));
  t125 = ((int32_t *)t121.data) + gg_index(t124, t121.len);
  v22 = ((float)(((float)((*t125)))));
  /* FG_DIRECT_SLICE_INDEX */
  t126 = ((int64_t)UINT64_C(0));
  t127 = ((gg_type_30 *)((v17).f5).data) + gg_index(t126, ((int64_t)UINT64_C(256)));
  t128 = gg_view(((*t127)).data, 8);
  t129 = ((int64_t)(v19));
  t130 = ((int64_t)(((int64_t)UINT64_C(32))));
  t131 = ((int64_t)gg_div(t129, t130));
  t132 = ((gg_type_29 *)t128.data) + gg_index(t131, t128.len);
  t133 = gg_view(((*t132)).data, 32);
  t134 = ((int64_t)(v19));
  t135 = ((int64_t)(((int64_t)UINT64_C(32))));
  t136 = ((int64_t)gg_mod(t134, t135));
  t137 = ((uint8_t *)t133.data) + gg_index(t136, t133.len);
  t138 = ((int32_t)(((int32_t)((*t137)))));
  t139 = ((int32_t)(((int32_t)((v16).f3))));
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
  t148 = ((float *)((v17).f6).data) + gg_index(t147, ((int64_t)UINT64_C(256)));
  t149 = ((float)((*t148)));
  t150 = ((float)(v21));
  t151 = ((float)(((t149) * (t150))));
  t152 = ((float)(((float)(v23))));
  t153 = ((float)(((t151) * (t152))));
  /* FG_DIRECT_SLICE_INDEX */
  t154 = ((int64_t)UINT64_C(0));
  t155 = ((float *)((v17).f7).data) + gg_index(t154, ((int64_t)UINT64_C(256)));
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
static void gg_function_0(int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  float t3;
  float t4;
  int64_t t5;
  gg_type_5 t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  gg_type_5 t10;
  int64_t t11;
  gg_type_7 t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  gg_type_7 t16;
  int64_t t17;
  gg_type_10 t18;
  int8_t t19;
  int8_t t20;
  int64_t t21;
  gg_type_10 t22;
  int64_t t23;
  uint8_t t24;
  uint8_t t25;
  int64_t t26;
  int8_t t27;
  int8_t t28;
  int64_t t29;
  int64_t t30;
  gg_slice t31;
  gg_slice t32;
  gg_slice t33;
  gg_slice t34;
  gg_slice t35;
  gg_slice t36;
  gg_slice t37;
  gg_slice t38;
  gg_slice t39;
  gg_slice t40;
  gg_header t41;
  int64_t t42;
  gg_type_2 v0;
  gg_type_4 v1;
  gg_type_6 v2;
  gg_type_8 v3;
  gg_type_11 v4;
  gg_type_12 v5;
  gg_type_13 v6;
  uint8_t v7;
  {
  memset(&t0, 0, sizeof(t0));
  t1 = ((uint8_t)(t0));
  for (t2=0; t2<18; t2++) (v0).data[t2] = t1;
  memset(&t3, 0, sizeof(t3));
  t4 = ((float)(t3));
  for (t5=0; t5<32; t5++) (v1).data[t5] = t4;
  memset(&t7, 0, sizeof(t7));
  t8 = ((uint8_t)(t7));
  for (t9=0; t9<8; t9++) (t6).data[t9] = t8;
  t10 = t6;
  for (t11=0; t11<1; t11++) (v2).data[t11] = t10;
  memset(&t13, 0, sizeof(t13));
  t14 = ((uint8_t)(t13));
  for (t15=0; t15<4; t15++) (t12).data[t15] = t14;
  t16 = t12;
  for (t17=0; t17<1; t17++) (v3).data[t17] = t16;
  memset(&t19, 0, sizeof(t19));
  t20 = ((int8_t)(t19));
  for (t21=0; t21<8; t21++) (t18).data[t21] = t20;
  t22 = t18;
  for (t23=0; t23<1; t23++) (v4).data[t23] = t22;
  memset(&t24, 0, sizeof(t24));
  t25 = ((uint8_t)(t24));
  for (t26=0; t26<128; t26++) (v5).data[t26] = t25;
  memset(&t27, 0, sizeof(t27));
  t28 = ((int8_t)(t27));
  for (t29=0; t29<16; t29++) (v6).data[t29] = t28;
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = gg_view((v0).data, 18);
  t32 = gg_view((v1).data, 32);
  t33 = gg_view((v2).data, 1);
  t34 = gg_view((v2).data, 1);
  t35 = gg_view((v2).data, 1);
  t36 = gg_view((v3).data, 1);
  t37 = gg_view((v3).data, 1);
  t38 = gg_view((v4).data, 1);
  t39 = gg_view((v5).data, 128);
  t40 = gg_view((v6).data, 16);
  v7 = gg_function_1(t30, t31, t32, t33, t34, t35, t36, t37, t38, t39, t40, base + 0);
  t41.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t41.len = 0;
  gg_bool(gg_ref(&t41, gg_get_stack(base + 0)), v7);
  gg_append(gg_ref(&t41, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t41).base, (t41).len));
  }
gg_done:;
  for (t42 = 0; t42 < 1; t42++) gg_stacks[base + t42].top = 0;
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
