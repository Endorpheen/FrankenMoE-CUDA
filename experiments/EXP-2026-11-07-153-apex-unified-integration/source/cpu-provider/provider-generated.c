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
typedef struct { uint8_t data[1]; } gg_type_11;
#pragma pack(pop)
typedef gg_type_1 *gg_type_12;
typedef struct gg_type_19 gg_type_19;
#pragma pack(push, 1)
struct gg_type_19 {
  int64_t f13;
  int64_t f14;
  int64_t f15;
};
#pragma pack(pop)
typedef gg_type_2 *gg_type_20;
typedef struct gg_type_24 gg_type_24;
#pragma pack(push, 1)
typedef struct { uint8_t data[32]; } gg_type_25;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_25 data[8]; } gg_type_26;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_26 data[256]; } gg_type_27;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_28;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_28 data[512]; } gg_type_29;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_24 {
  gg_type_27 f16;
  gg_type_10 f17;
  gg_type_10 f18;
  gg_type_29 f19;
  gg_type_29 f20;
};
#pragma pack(pop)
typedef gg_type_24 *gg_type_30;
#pragma pack(push, 1)
typedef struct { uint8_t data[144]; } gg_type_35;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[256]; } gg_type_36;
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
static uint8_t gg_eq_13(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_slice value);
static uint8_t gg_eq_14(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, gg_slice value);
static uint8_t gg_eq_15(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, gg_slice value);
static uint8_t gg_eq_16(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, gg_slice value);
static uint8_t gg_eq_17(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, uint8_t value);
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, gg_type_19 value);
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b);
static uint8_t gg_equal_elements_20(gg_slice a, gg_slice b);
static void gg_render_20(gg_reference out, gg_type_20 value);
static uint8_t gg_eq_21(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_21(gg_slice a, gg_slice b);
static void gg_render_21(gg_reference out, uint32_t value);
static uint8_t gg_eq_22(int32_t a, int32_t b);
static uint8_t gg_equal_elements_22(gg_slice a, gg_slice b);
static void gg_render_22(gg_reference out, int32_t value);
static uint8_t gg_eq_23(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_23(gg_slice a, gg_slice b);
static void gg_render_23(gg_reference out, uint64_t value);
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
static uint8_t gg_eq_31(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_31(gg_slice a, gg_slice b);
static void gg_render_31(gg_reference out, uint16_t value);
static uint8_t gg_eq_32(int8_t a, int8_t b);
static uint8_t gg_equal_elements_32(gg_slice a, gg_slice b);
static void gg_render_32(gg_reference out, int8_t value);
static uint8_t gg_eq_33(int16_t a, int16_t b);
static uint8_t gg_equal_elements_33(gg_slice a, gg_slice b);
static void gg_render_33(gg_reference out, int16_t value);
static uint8_t gg_eq_34(double a, double b);
static uint8_t gg_equal_elements_34(gg_slice a, gg_slice b);
static void gg_render_34(gg_reference out, double value);
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b);
static uint8_t gg_equal_elements_35(gg_slice a, gg_slice b);
static void gg_render_35(gg_reference out, gg_type_35 value);
static uint8_t gg_eq_36(gg_type_36 a, gg_type_36 b);
static uint8_t gg_equal_elements_36(gg_slice a, gg_slice b);
static void gg_render_36(gg_reference out, gg_type_36 value);
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
return gg_equal_elements_8(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_11(gg_reference out, gg_type_11 value) {
gg_text(out,gg_view((value).data, 1));
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return a == b;
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_13(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_14(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(gg_slice a, gg_slice b) {
return gg_equal_elements_6(a,b);
}
static void gg_render_15(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_16(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_17(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((gg_type_19*)a.data)[i],((gg_type_19*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b) {
if(!gg_eq_3(a.f13,b.f13))return 0;
if(!gg_eq_3(a.f14,b.f14))return 0;
if(!gg_eq_3(a.f15,b.f15))return 0;
return 1;
}
static void gg_render_19(gg_reference out, gg_type_19 value) {
gg_append(out,"\124\151\154\145\124\162\141\151\164\163\040\173\040",13,1);
gg_render_3(out, value.f13);
gg_append(out,", ",2,1);
gg_render_3(out, value.f14);
gg_append(out,", ",2,1);
gg_render_3(out, value.f15);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((gg_type_20*)a.data)[i],((gg_type_20*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(gg_type_20 a, gg_type_20 b) {
return a == b;
}
static void gg_render_20(gg_reference out, gg_type_20 value) {
gg_render_2(out,*value);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_21(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_22(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_23(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((gg_type_24*)a.data)[i],((gg_type_24*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b) {
if(!gg_eq_27(a.f16,b.f16))return 0;
if(!gg_eq_10(a.f17,b.f17))return 0;
if(!gg_eq_10(a.f18,b.f18))return 0;
if(!gg_eq_29(a.f19,b.f19))return 0;
if(!gg_eq_29(a.f20,b.f20))return 0;
return 1;
}
static void gg_render_24(gg_reference out, gg_type_24 value) {
gg_append(out,"\127\145\151\147\150\164\124\151\154\145\040\173\040",13,1);
gg_render_27(out, value.f16);
gg_append(out,", ",2,1);
gg_render_10(out, value.f17);
gg_append(out,", ",2,1);
gg_render_10(out, value.f18);
gg_append(out,", ",2,1);
gg_render_29(out, value.f19);
gg_append(out,", ",2,1);
gg_render_29(out, value.f20);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((gg_type_25*)a.data)[i],((gg_type_25*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b) {
return gg_equal_elements_8(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_25(gg_reference out, gg_type_25 value) {
gg_text(out,gg_view((value).data, 32));
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((gg_type_26*)a.data)[i],((gg_type_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b) {
return gg_equal_elements_25(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_26(gg_reference out, gg_type_26 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_quote(out, gg_view((((gg_type_25*)view.data)[i]).data, 32));}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((gg_type_27*)a.data)[i],((gg_type_27*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b) {
return gg_equal_elements_26(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_27(gg_reference out, gg_type_27 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_26(out, ((gg_type_26*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_type_28*)a.data)[i],((gg_type_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
return gg_equal_elements_22(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_22(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_29(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_29(((gg_type_29*)a.data)[i],((gg_type_29*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_29(gg_type_29 a, gg_type_29 b) {
return gg_equal_elements_28(gg_view((a).data, 512),gg_view((b).data, 512));
}
static void gg_render_29(gg_reference out, gg_type_29 value) {
int64_t i; gg_slice view=gg_view((value).data, 512);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_28(out, ((gg_type_28*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_30(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_30(((gg_type_30*)a.data)[i],((gg_type_30*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_30(gg_type_30 a, gg_type_30 b) {
return a == b;
}
static void gg_render_30(gg_reference out, gg_type_30 value) {
gg_render_24(out,*value);
}
static uint8_t gg_equal_elements_31(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_31(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_31(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_31(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_32(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_32(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_32(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_32(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_33(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_33(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_33(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_33(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_34(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_34(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_34(double a, double b) {
return a == b;
}
static void gg_render_34(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_35(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_35(((gg_type_35*)a.data)[i],((gg_type_35*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_35(gg_type_35 a, gg_type_35 b) {
return gg_equal_elements_8(gg_view((a).data, 144),gg_view((b).data, 144));
}
static void gg_render_35(gg_reference out, gg_type_35 value) {
gg_text(out,gg_view((value).data, 144));
}
static uint8_t gg_equal_elements_36(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_36(((gg_type_36*)a.data)[i],((gg_type_36*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_36(gg_type_36 a, gg_type_36 b) {
return gg_equal_elements_6(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_36(gg_reference out, gg_type_36 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static uint8_t gg_function_1(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, int64_t base);
static void gg_function_2(gg_type_12 p0, int64_t p1, gg_slice p2, int64_t base);
static int64_t gg_function_3(int64_t p0, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t base);
static gg_type_19 gg_function_5(int64_t p0, int64_t base);
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_8(gg_type_12 p0, gg_slice p1, int64_t base);
static int64_t gg_function_9(gg_type_20 p0, int64_t base);
static int64_t gg_function_10(gg_type_20 p0, gg_slice p1, int64_t p2, int64_t base);
static int64_t gg_function_11(gg_type_20 p0, int64_t p1, int64_t base);
static uint32_t gg_function_12(gg_slice p0, int64_t p1, int64_t base);
static uint8_t gg_function_13(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t base);
static uint8_t gg_function_14(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static int64_t gg_function_15(gg_type_20 p0, int64_t base);
static void gg_function_16(gg_type_20 p0, int64_t p1, int64_t base);
static int64_t gg_function_17(gg_type_12 p0, int64_t base);
static uint8_t gg_function_19(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t base);
static uint8_t gg_function_20(int64_t p0, gg_slice p1, gg_slice p2, int64_t base);
static void gg_function_21(int64_t p0, gg_slice p1, int64_t p2, gg_type_30 p3, int64_t base);
static void gg_function_22(gg_type_30 p0, int64_t p1, int32_t p2, int64_t base);
static void gg_function_23(gg_type_30 p0, int64_t p1, int32_t p2, int64_t base);
static uint16_t gg_function_24(gg_slice p0, int64_t p1, int64_t base);
static gg_type_25 gg_function_26(gg_slice p0, int64_t p1, int64_t base);
static gg_type_25 gg_function_27(gg_type_25 p307, uint8_t p308, int64_t base);
static gg_type_25 gg_function_28(gg_type_25 p315, uint8_t p316, int64_t base);
static uint8_t gg_function_29(int64_t p0, gg_slice p1, gg_slice p2, int64_t base);
static void gg_function_30(int64_t p0, gg_slice p1, int64_t p2, gg_type_30 p3, int64_t base);
static void gg_function_31(gg_type_30 p0, int64_t p1, int32_t p2, int64_t base);
static void gg_function_32(gg_type_30 p0, int64_t p1, int32_t p2, int64_t base);
static uint16_t gg_function_33(gg_slice p0, int64_t p1, int64_t base);
static gg_type_25 gg_function_34(gg_slice p0, int64_t p1, int64_t base);
static void gg_function_35(gg_type_20 p0, gg_slice p1, int64_t p2, float p3, int64_t base);
static void gg_function_36(gg_type_20 p0, gg_slice p1, int64_t p2, float p3, int64_t base);
static void gg_function_38(gg_slice p0, int64_t p1, uint32_t p2, int64_t base);
static void gg_function_40(gg_slice p0, int64_t p1, uint16_t p2, int64_t base);
static int32_t gg_function_41(float p0, int64_t base);
static uint8_t gg_function_42(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static float gg_function_43(gg_type_20 p0, gg_slice p1, int64_t p2, int64_t base);
static uint16_t gg_function_45(gg_slice p0, int64_t p1, int64_t base);
static uint8_t gg_function_47(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base);
static uint8_t gg_function_48(gg_type_20 p0, gg_type_20 p1, int64_t base);
static float gg_function_49(gg_type_20 p0, gg_slice p1, int64_t p2, int64_t base);
static float gg_function_50(gg_type_12 p0, float p1, int64_t base);
static float gg_function_51(float p0, int64_t base);
static float gg_function_53(float p0, int64_t base);
static float gg_function_55(gg_type_12 p0, float p1, int64_t p2, int64_t p3, int64_t base);
static float gg_function_56(float p0, int64_t base);
static float gg_function_59(float p0, int64_t base);
static float gg_function_61(gg_type_12 p0, float p1, int64_t p2, int64_t p3, int64_t base);
static float gg_function_62(float p0, int64_t base);
static float gg_function_63(float p0, float p1, int64_t base);
static float gg_function_64(float p0, float p1, int64_t base);
static uint8_t gg_function_65(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t base);
static uint8_t gg_function_66(gg_type_12 p0, gg_slice p1, gg_slice p2, int64_t base);
static uint8_t gg_function_67(gg_type_20 p0, int64_t base);
static uint8_t gg_function_68(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t base);
static int64_t gg_function_69(int64_t p0, int64_t p1, int64_t base);
static int32_t gg_function_41(float p0, int64_t base) {
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
static void gg_function_40(gg_slice p0, int64_t p1, uint16_t p2, int64_t base) {
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
static void gg_function_38(gg_slice p0, int64_t p1, uint32_t p2, int64_t base) {
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
static void gg_function_36(gg_type_20 p0, gg_slice p1, int64_t p2, float p3, int64_t base) {
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
  gg_type_20 v0;
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
  gg_function_38(t3, t4, t7, base + 0);
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
  gg_function_40(t11, t12, t15, base + 0);
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
  gg_function_40(t40, t41, t42, base + 0);
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
  t49 = gg_function_41(t48, base + 0);
  t50 = ((uint32_t)(((uint32_t)(t49))));
  gg_function_38(t46, t47, t50, base + 0);
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
  t57 = gg_function_41(t56, base + 0);
  t58 = ((uint16_t)(((uint16_t)(t57))));
  gg_function_40(t54, t55, t58, base + 0);
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
  t72 = gg_function_41(t71, base + 0);
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
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t base) {
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
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t base) {
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
static int64_t gg_function_9(gg_type_20 p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t * t5;
  int64_t t6;
  gg_type_20 v0;
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
  v1 = gg_function_6(t3, t6, base + 0);
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
static int64_t gg_function_11(gg_type_20 p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  gg_type_20 t6;
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
  gg_type_20 v0;
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
  t7 = gg_function_9(t6, base + 0);
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
  t24 = gg_function_6(t20, t23, base + 0);
  v3 = gg_function_7(t15, t24, base + 0);
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
static void gg_function_35(gg_type_20 p0, gg_slice p1, int64_t p2, float p3, int64_t base) {
  gg_type_20 t0;
  gg_slice t1;
  gg_type_20 t2;
  int64_t t3;
  int64_t t4;
  float t5;
  gg_type_20 v0;
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
  t4 = gg_function_11(t2, t3, base + 0);
  t5 = ((float)(v3));
  gg_function_36(t0, t1, t4, t5, base + 0);
  }
gg_done:;
}
static void gg_function_31(gg_type_30 p0, int64_t p1, int32_t p2, int64_t base) {
  gg_slice t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  gg_type_28 * t4;
  gg_slice t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t * t9;
  gg_type_30 v0;
  int64_t v1;
  int32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = gg_view((((*(v0))).f19).data, 512);
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)gg_div(t1, t2));
  t4 = ((gg_type_28 *)t0.data) + gg_index(t3, t0.len);
  t5 = gg_view(((*t4)).data, 8);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)gg_mod(t6, t7));
  t9 = ((int32_t *)t5.data) + gg_index(t8, t5.len);
  (*t9) = ((int32_t)(v2));
  }
gg_done:;
}
static uint16_t gg_function_33(gg_slice p0, int64_t p1, int64_t base) {
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
/* FG_PACKED_MATCH:shr8 */
static gg_type_25 gg_function_28(gg_type_25 p315, uint8_t p316, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_25 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)p315.data), _mm_cvtsi32_si128((p316 & 7))), _mm256_set1_epi8((unsigned char)(255u >> (p316 & 7)))));
    return packed_result;
  }
#endif
  gg_type_25 t0;
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
  gg_type_25 v315;
  uint8_t v316;
  gg_type_25 v317;
  int64_t v319;
  v315 = p315;
  v316 = p316;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v317).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v319 = t4;
  {
  t6 = gg_view((v317).data, 32);
  t7 = v319;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v315).data, 32);
  t10 = v319;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v316));
  (*t8) = ((uint8_t)(((t12) >> ((uint64_t)(t13) & 7))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v317;
  goto gg_done;
  }
gg_done:;
  return t0;
}
/* FG_PACKED_MATCH:and8 */
static gg_type_25 gg_function_27(gg_type_25 p307, uint8_t p308, int64_t base) {
#if FGOOSE_PACKED && defined(__AVX2__)
  { gg_type_25 packed_result;
    _mm256_storeu_si256((__m256i*)packed_result.data, _mm256_and_si256(_mm256_loadu_si256((const __m256i*)p307.data), _mm256_set1_epi8(p308)));
    return packed_result;
  }
#endif
  gg_type_25 t0;
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
  gg_type_25 v307;
  uint8_t v308;
  gg_type_25 v309;
  int64_t v311;
  v307 = p307;
  v308 = p308;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint8_t)(t1));
  for (t3=0; t3<32; t3++) (v309).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(32));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v311 = t4;
  {
  t6 = gg_view((v309).data, 32);
  t7 = v311;
  t8 = ((uint8_t *)t6.data) + gg_index(t7, t6.len);
  t9 = gg_view((v307).data, 32);
  t10 = v311;
  t11 = ((uint8_t *)t9.data) + gg_index(t10, t9.len);
  t12 = ((uint8_t)((*t11)));
  t13 = ((uint8_t)(v308));
  (*t8) = ((uint8_t)(((t12) & (t13))));
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v309;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_25 gg_function_34(gg_slice p0, int64_t p1, int64_t base) {
  gg_type_25 t0;
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
  gg_type_25 v3;
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
static void gg_function_32(gg_type_30 p0, int64_t p1, int32_t p2, int64_t base) {
  gg_slice t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  gg_type_28 * t4;
  gg_slice t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t * t9;
  gg_type_30 v0;
  int64_t v1;
  int32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = gg_view((((*(v0))).f20).data, 512);
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)gg_div(t1, t2));
  t4 = ((gg_type_28 *)t0.data) + gg_index(t3, t0.len);
  t5 = gg_view(((*t4)).data, 8);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)gg_mod(t6, t7));
  t9 = ((int32_t *)t5.data) + gg_index(t8, t5.len);
  (*t9) = ((int32_t)(v2));
  }
gg_done:;
}
static gg_type_19 gg_function_5(int64_t p0, int64_t base) {
  gg_type_19 t0;
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
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
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
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
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
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(32))));
  goto gg_done;
  }
  }
  memset(&(t0), 0, sizeof(t0));
  (t0).f13 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f14 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f15 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_30(int64_t p0, gg_slice p1, int64_t p2, gg_type_30 p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  float * t4;
  int64_t t5;
  float * t6;
  int64_t t7;
  int64_t t8;
  gg_type_30 t9;
  int64_t t10;
  int32_t t11;
  gg_type_30 t12;
  int64_t t13;
  int32_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  float * t19;
  gg_slice t20;
  int64_t t21;
  uint16_t t22;
  float t23;
  int64_t t24;
  float * t25;
  gg_slice t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint16_t t30;
  float t31;
  int64_t t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t * t40;
  gg_slice t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t * t47;
  gg_slice t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t * t54;
  gg_type_30 t55;
  int64_t t56;
  uint8_t t57;
  uint8_t t58;
  int32_t t59;
  gg_type_30 t60;
  int64_t t61;
  uint8_t t62;
  uint8_t t63;
  int32_t t64;
  gg_type_30 t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  uint8_t t70;
  uint8_t t71;
  uint8_t t72;
  int64_t t73;
  uint8_t t74;
  int64_t t75;
  uint8_t t76;
  int32_t t77;
  gg_type_30 t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  uint8_t t82;
  int64_t t83;
  uint8_t t84;
  uint8_t t85;
  int64_t t86;
  uint8_t t87;
  int64_t t88;
  uint8_t t89;
  int32_t t90;
  int64_t t91;
  int64_t t92;
  gg_slice t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  gg_type_25 t101;
  uint8_t t102;
  gg_type_25 t103;
  uint8_t t104;
  int64_t t105;
  gg_type_26 * t106;
  gg_slice t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  gg_type_25 * t111;
  int64_t t112;
  gg_type_26 * t113;
  gg_slice t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  gg_type_25 * t120;
  int64_t t121;
  int64_t t122;
  uint8_t t123;
  int64_t t124;
  float * t125;
  gg_slice t126;
  int64_t t127;
  uint16_t t128;
  float t129;
  int64_t t130;
  float * t131;
  gg_slice t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  uint16_t t136;
  float t137;
  int64_t t138;
  int64_t t139;
  gg_slice t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t * t146;
  gg_slice t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  uint8_t * t153;
  gg_slice t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  uint8_t * t160;
  gg_type_30 t161;
  int64_t t162;
  uint8_t t163;
  uint8_t t164;
  int32_t t165;
  gg_type_30 t166;
  int64_t t167;
  uint8_t t168;
  uint8_t t169;
  int32_t t170;
  gg_type_30 t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  uint8_t t176;
  uint8_t t177;
  uint8_t t178;
  int64_t t179;
  uint8_t t180;
  int64_t t181;
  uint8_t t182;
  int32_t t183;
  gg_type_30 t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  uint8_t t188;
  int64_t t189;
  uint8_t t190;
  uint8_t t191;
  int64_t t192;
  uint8_t t193;
  int64_t t194;
  uint8_t t195;
  int32_t t196;
  gg_slice t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  gg_slice t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  gg_type_25 t211;
  uint8_t t212;
  gg_type_25 t213;
  uint8_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  uint8_t * t218;
  int64_t t219;
  uint8_t * t220;
  uint8_t t221;
  int64_t t222;
  uint8_t * t223;
  uint8_t t224;
  int64_t t225;
  int64_t t226;
  uint8_t t227;
  uint8_t t228;
  uint8_t t229;
  uint8_t t230;
  int64_t t231;
  uint8_t t232;
  int64_t t233;
  uint8_t * t234;
  int64_t t235;
  uint8_t * t236;
  uint8_t t237;
  int64_t t238;
  uint8_t * t239;
  uint8_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  uint8_t t245;
  uint8_t t246;
  uint8_t t247;
  uint8_t t248;
  int64_t t249;
  uint8_t t250;
  int64_t t251;
  gg_type_26 * t252;
  gg_slice t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  gg_type_25 * t257;
  int64_t t258;
  gg_type_26 * t259;
  gg_slice t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  gg_type_25 * t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  float * t270;
  gg_slice t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  uint16_t t275;
  float t276;
  int64_t t277;
  int64_t t278;
  gg_type_30 t279;
  int64_t t280;
  gg_slice t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  uint8_t * t287;
  int32_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  gg_slice t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  uint8_t * t303;
  gg_slice t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  uint8_t * t312;
  gg_slice t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  uint8_t * t323;
  int64_t t324;
  gg_type_26 * t325;
  gg_slice t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  gg_type_25 * t330;
  gg_slice t331;
  int64_t t332;
  uint8_t * t333;
  uint8_t t334;
  uint8_t t335;
  uint8_t t336;
  uint8_t t337;
  uint8_t t338;
  uint8_t t339;
  int64_t t340;
  uint8_t t341;
  int64_t t342;
  gg_type_26 * t343;
  gg_slice t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  int64_t t349;
  gg_type_25 * t350;
  gg_slice t351;
  int64_t t352;
  uint8_t * t353;
  uint8_t t354;
  uint8_t t355;
  uint8_t t356;
  uint8_t t357;
  uint8_t t358;
  uint8_t t359;
  int64_t t360;
  uint8_t t361;
  int64_t t362;
  gg_type_26 * t363;
  gg_slice t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  gg_type_25 * t370;
  gg_slice t371;
  int64_t t372;
  uint8_t * t373;
  uint8_t t374;
  int64_t t375;
  uint8_t t376;
  uint8_t t377;
  uint8_t t378;
  uint8_t t379;
  int64_t t380;
  gg_type_26 * t381;
  gg_slice t382;
  int64_t t383;
  int64_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  gg_type_25 * t388;
  gg_slice t389;
  int64_t t390;
  uint8_t * t391;
  uint8_t t392;
  int64_t t393;
  uint8_t t394;
  uint8_t t395;
  uint8_t t396;
  uint8_t t397;
  int64_t t398;
  uint8_t t399;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  gg_type_30 v3;
  gg_type_19 v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  uint8_t v8;
  uint8_t v9;
  uint8_t v10;
  int64_t v11;
  gg_type_25 v12;
  gg_type_25 v13;
  gg_type_25 v14;
  int64_t v15;
  uint8_t v16;
  uint8_t v17;
  uint8_t v18;
  gg_type_25 v19;
  int64_t v20;
  gg_type_25 v21;
  gg_type_25 v22;
  gg_type_25 v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  uint8_t v28;
  uint8_t v29;
  uint8_t v30;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v0));
  v4 = gg_function_5(t0, base + 0);
  t1 = ((int64_t)(((int64_t)UINT64_C(256))));
  t2 = ((int64_t)((v4).f14));
  v5 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  /* FG_DIRECT_SLICE_INDEX */
  t3 = ((int64_t)UINT64_C(0));
  t4 = ((float *)(((*(v3))).f17).data) + gg_index(t3, ((int64_t)UINT64_C(1)));
  (*t4) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  /* FG_DIRECT_SLICE_INDEX */
  t5 = ((int64_t)UINT64_C(0));
  t6 = ((float *)(((*(v3))).f18).data) + gg_index(t5, ((int64_t)UINT64_C(1)));
  (*t6) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t8 = v5;
  t7 = 0;
label0:;
  if (t7 >= t8) goto label2;
  v6 = t7;
  {
  t9 = v3;
  t10 = ((int64_t)(v6));
  t11 = ((int32_t)(((int64_t)UINT64_C(0))));
  gg_function_31(t9, t10, t11, base + 0);
  t12 = v3;
  t13 = ((int64_t)(v6));
  t14 = ((int32_t)(((int64_t)UINT64_C(0))));
  gg_function_32(t12, t13, t14, base + 0);
  }
label1:;
  t7++; goto label0;
label2:;
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t18 = ((int64_t)UINT64_C(0));
  t19 = ((float *)(((*(v3))).f17).data) + gg_index(t18, ((int64_t)UINT64_C(1)));
  t20 = v1;
  t21 = ((int64_t)(v2));
  t22 = gg_function_33(t20, t21, base + 0);
  t23 = fg141_half(t22);
  (*t19) = ((float)(t23));
  /* FG_DIRECT_SLICE_INDEX */
  t24 = ((int64_t)UINT64_C(0));
  t25 = ((float *)(((*(v3))).f18).data) + gg_index(t24, ((int64_t)UINT64_C(1)));
  t26 = v1;
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = gg_function_33(t26, t29, base + 0);
  t31 = fg141_half(t30);
  (*t25) = ((float)(t31));
  t33 = ((int64_t)UINT64_C(4));
  t32 = 0;
label3:;
  if (t32 >= t33) goto label5;
  v7 = t32;
  {
  t34 = v1;
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)(((int64_t)UINT64_C(4))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t38 = ((int64_t)(v7));
  t39 = ((int64_t)((uint64_t)(t37) + (uint64_t)(t38)));
  t40 = ((uint8_t *)t34.data) + gg_index(t39, t34.len);
  v8 = ((uint8_t)((*t40)));
  t41 = v1;
  t42 = ((int64_t)(v2));
  t43 = ((int64_t)(((int64_t)UINT64_C(8))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = ((int64_t)(v7));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint8_t *)t41.data) + gg_index(t46, t41.len);
  v9 = ((uint8_t)((*t47)));
  t48 = v1;
  t49 = ((int64_t)(v2));
  t50 = ((int64_t)(((int64_t)UINT64_C(12))));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t50)))));
  t52 = ((int64_t)(v7));
  t53 = ((int64_t)((uint64_t)(t51) + (uint64_t)(t52)));
  t54 = ((uint8_t *)t48.data) + gg_index(t53, t48.len);
  v10 = ((uint8_t)((*t54)));
  t55 = v3;
  t56 = ((int64_t)(v7));
  t57 = ((uint8_t)(v8));
  t58 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t59 = ((int32_t)(((int32_t)(((t57) & (t58))))));
  gg_function_31(t55, t56, t59, base + 0);
  t60 = v3;
  t61 = ((int64_t)(v7));
  t62 = ((uint8_t)(v9));
  t63 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t64 = ((int32_t)(((int32_t)(((t62) & (t63))))));
  gg_function_32(t60, t61, t64, base + 0);
  t65 = v3;
  t66 = ((int64_t)(v7));
  t67 = ((int64_t)(((int64_t)UINT64_C(4))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t69 = ((uint8_t)(v10));
  t70 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t71 = ((uint8_t)(((t69) & (t70))));
  t72 = ((uint8_t)(v8));
  t73 = ((int64_t)(((int64_t)UINT64_C(6))));
  t74 = ((uint8_t)(((t72) >> ((uint64_t)(t73) & 7))));
  t75 = ((int64_t)(((int64_t)UINT64_C(4))));
  t76 = ((uint8_t)(((uint8_t)((uint64_t)(t74) << ((uint64_t)(t75) & 7)))));
  t77 = ((int32_t)(((int32_t)(((t71) | (t76))))));
  gg_function_31(t65, t68, t77, base + 0);
  t78 = v3;
  t79 = ((int64_t)(v7));
  t80 = ((int64_t)(((int64_t)UINT64_C(4))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  t82 = ((uint8_t)(v10));
  t83 = ((int64_t)(((int64_t)UINT64_C(4))));
  t84 = ((uint8_t)(((t82) >> ((uint64_t)(t83) & 7))));
  t85 = ((uint8_t)(v9));
  t86 = ((int64_t)(((int64_t)UINT64_C(6))));
  t87 = ((uint8_t)(((t85) >> ((uint64_t)(t86) & 7))));
  t88 = ((int64_t)(((int64_t)UINT64_C(4))));
  t89 = ((uint8_t)(((uint8_t)((uint64_t)(t87) << ((uint64_t)(t88) & 7)))));
  t90 = ((int32_t)(((int32_t)(((t84) | (t89))))));
  gg_function_32(t78, t81, t90, base + 0);
  }
label4:;
  t32++; goto label3;
label5:;
  t92 = ((int64_t)UINT64_C(4));
  t91 = 0;
label6:;
  if (t91 >= t92) goto label8;
  v11 = t91;
  {
  t93 = v1;
  t94 = ((int64_t)(v2));
  t95 = ((int64_t)(((int64_t)UINT64_C(16))));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t94) + (uint64_t)(t95)))));
  t97 = ((int64_t)(v11));
  t98 = ((int64_t)(((int64_t)UINT64_C(32))));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t97) * (uint64_t)(t98)))));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t99)))));
  v12 = gg_function_34(t93, t100, base + 0);
  t101 = v12;
  t102 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v13 = gg_function_27(t101, t102, base + 0);
  t103 = v12;
  t104 = ((uint8_t)(((int64_t)UINT64_C(4))));
  v14 = gg_function_28(t103, t104, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t105 = ((int64_t)UINT64_C(0));
  t106 = ((gg_type_26 *)(((*(v3))).f16).data) + gg_index(t105, ((int64_t)UINT64_C(256)));
  t107 = gg_view(((*t106)).data, 8);
  t108 = ((int64_t)(((int64_t)UINT64_C(2))));
  t109 = ((int64_t)(v11));
  t110 = ((int64_t)((uint64_t)(t108) * (uint64_t)(t109)));
  t111 = ((gg_type_25 *)t107.data) + gg_index(t110, t107.len);
  (*t111) = v13;
  /* FG_DIRECT_SLICE_INDEX */
  t112 = ((int64_t)UINT64_C(0));
  t113 = ((gg_type_26 *)(((*(v3))).f16).data) + gg_index(t112, ((int64_t)UINT64_C(256)));
  t114 = gg_view(((*t113)).data, 8);
  t115 = ((int64_t)(((int64_t)UINT64_C(2))));
  t116 = ((int64_t)(v11));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) * (uint64_t)(t116)))));
  t118 = ((int64_t)(((int64_t)UINT64_C(1))));
  t119 = ((int64_t)((uint64_t)(t117) + (uint64_t)(t118)));
  t120 = ((gg_type_25 *)t114.data) + gg_index(t119, t114.len);
  (*t120) = v14;
  }
label7:;
  t91++; goto label6;
label8:;
  }
  } else {
  t121 = ((int64_t)(v0));
  t122 = ((int64_t)(((int64_t)UINT64_C(1))));
  t123 = ((t121) == (t122));
  if (t123) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t124 = ((int64_t)UINT64_C(0));
  t125 = ((float *)(((*(v3))).f17).data) + gg_index(t124, ((int64_t)UINT64_C(1)));
  t126 = v1;
  t127 = ((int64_t)(v2));
  t128 = gg_function_33(t126, t127, base + 0);
  t129 = fg141_half(t128);
  (*t125) = ((float)(t129));
  /* FG_DIRECT_SLICE_INDEX */
  t130 = ((int64_t)UINT64_C(0));
  t131 = ((float *)(((*(v3))).f18).data) + gg_index(t130, ((int64_t)UINT64_C(1)));
  t132 = v1;
  t133 = ((int64_t)(v2));
  t134 = ((int64_t)(((int64_t)UINT64_C(2))));
  t135 = ((int64_t)(((int64_t)((uint64_t)(t133) + (uint64_t)(t134)))));
  t136 = gg_function_33(t132, t135, base + 0);
  t137 = fg141_half(t136);
  (*t131) = ((float)(t137));
  t139 = ((int64_t)UINT64_C(4));
  t138 = 0;
label9:;
  if (t138 >= t139) goto label11;
  v15 = t138;
  {
  t140 = v1;
  t141 = ((int64_t)(v2));
  t142 = ((int64_t)(((int64_t)UINT64_C(4))));
  t143 = ((int64_t)(((int64_t)((uint64_t)(t141) + (uint64_t)(t142)))));
  t144 = ((int64_t)(v15));
  t145 = ((int64_t)((uint64_t)(t143) + (uint64_t)(t144)));
  t146 = ((uint8_t *)t140.data) + gg_index(t145, t140.len);
  v16 = ((uint8_t)((*t146)));
  t147 = v1;
  t148 = ((int64_t)(v2));
  t149 = ((int64_t)(((int64_t)UINT64_C(8))));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) + (uint64_t)(t149)))));
  t151 = ((int64_t)(v15));
  t152 = ((int64_t)((uint64_t)(t150) + (uint64_t)(t151)));
  t153 = ((uint8_t *)t147.data) + gg_index(t152, t147.len);
  v17 = ((uint8_t)((*t153)));
  t154 = v1;
  t155 = ((int64_t)(v2));
  t156 = ((int64_t)(((int64_t)UINT64_C(12))));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) + (uint64_t)(t156)))));
  t158 = ((int64_t)(v15));
  t159 = ((int64_t)((uint64_t)(t157) + (uint64_t)(t158)));
  t160 = ((uint8_t *)t154.data) + gg_index(t159, t154.len);
  v18 = ((uint8_t)((*t160)));
  t161 = v3;
  t162 = ((int64_t)(v15));
  t163 = ((uint8_t)(v16));
  t164 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t165 = ((int32_t)(((int32_t)(((t163) & (t164))))));
  gg_function_31(t161, t162, t165, base + 0);
  t166 = v3;
  t167 = ((int64_t)(v15));
  t168 = ((uint8_t)(v17));
  t169 = ((uint8_t)(((int64_t)UINT64_C(63))));
  t170 = ((int32_t)(((int32_t)(((t168) & (t169))))));
  gg_function_32(t166, t167, t170, base + 0);
  t171 = v3;
  t172 = ((int64_t)(v15));
  t173 = ((int64_t)(((int64_t)UINT64_C(4))));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t172) + (uint64_t)(t173)))));
  t175 = ((uint8_t)(v18));
  t176 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t177 = ((uint8_t)(((t175) & (t176))));
  t178 = ((uint8_t)(v16));
  t179 = ((int64_t)(((int64_t)UINT64_C(6))));
  t180 = ((uint8_t)(((t178) >> ((uint64_t)(t179) & 7))));
  t181 = ((int64_t)(((int64_t)UINT64_C(4))));
  t182 = ((uint8_t)(((uint8_t)((uint64_t)(t180) << ((uint64_t)(t181) & 7)))));
  t183 = ((int32_t)(((int32_t)(((t177) | (t182))))));
  gg_function_31(t171, t174, t183, base + 0);
  t184 = v3;
  t185 = ((int64_t)(v15));
  t186 = ((int64_t)(((int64_t)UINT64_C(4))));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t185) + (uint64_t)(t186)))));
  t188 = ((uint8_t)(v18));
  t189 = ((int64_t)(((int64_t)UINT64_C(4))));
  t190 = ((uint8_t)(((t188) >> ((uint64_t)(t189) & 7))));
  t191 = ((uint8_t)(v17));
  t192 = ((int64_t)(((int64_t)UINT64_C(6))));
  t193 = ((uint8_t)(((t191) >> ((uint64_t)(t192) & 7))));
  t194 = ((int64_t)(((int64_t)UINT64_C(4))));
  t195 = ((uint8_t)(((uint8_t)((uint64_t)(t193) << ((uint64_t)(t194) & 7)))));
  t196 = ((int32_t)(((int32_t)(((t190) | (t195))))));
  gg_function_32(t184, t187, t196, base + 0);
  }
label10:;
  t138++; goto label9;
label11:;
  t197 = v1;
  t198 = ((int64_t)(v2));
  t199 = ((int64_t)(((int64_t)UINT64_C(16))));
  t200 = ((int64_t)(((int64_t)((uint64_t)(t198) + (uint64_t)(t199)))));
  v19 = gg_function_34(t197, t200, base + 0);
  t202 = ((int64_t)UINT64_C(4));
  t201 = 0;
label12:;
  if (t201 >= t202) goto label14;
  v20 = t201;
  {
  t203 = v1;
  t204 = ((int64_t)(v2));
  t205 = ((int64_t)(((int64_t)UINT64_C(48))));
  t206 = ((int64_t)(((int64_t)((uint64_t)(t204) + (uint64_t)(t205)))));
  t207 = ((int64_t)(v20));
  t208 = ((int64_t)(((int64_t)UINT64_C(32))));
  t209 = ((int64_t)(((int64_t)((uint64_t)(t207) * (uint64_t)(t208)))));
  t210 = ((int64_t)(((int64_t)((uint64_t)(t206) + (uint64_t)(t209)))));
  v21 = gg_function_34(t203, t210, base + 0);
  t211 = v21;
  t212 = ((uint8_t)(((int64_t)UINT64_C(15))));
  v22 = gg_function_27(t211, t212, base + 0);
  t213 = v21;
  t214 = ((uint8_t)(((int64_t)UINT64_C(4))));
  v23 = gg_function_28(t213, t214, base + 0);
  t216 = ((int64_t)UINT64_C(32));
  t215 = 0;
label15:;
  if (t215 >= t216) goto label17;
  v24 = t215;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t217 = v24;
  t218 = ((uint8_t *)(v22).data) + gg_index(t217, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t219 = v24;
  t220 = ((uint8_t *)(v22).data) + gg_index(t219, ((int64_t)UINT64_C(32)));
  t221 = ((uint8_t)((*t220)));
  /* FG_DIRECT_SLICE_INDEX */
  t222 = v24;
  t223 = ((uint8_t *)(v19).data) + gg_index(t222, ((int64_t)UINT64_C(32)));
  t224 = ((uint8_t)((*t223)));
  t225 = ((int64_t)(((int64_t)UINT64_C(2))));
  t226 = ((int64_t)(v20));
  t227 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t225) * (uint64_t)(t226)))))));
  t228 = ((uint8_t)(((t224) >> ((uint64_t)(t227) & 7))));
  t229 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t230 = ((uint8_t)(((t228) & (t229))));
  t231 = ((int64_t)(((int64_t)UINT64_C(4))));
  t232 = ((uint8_t)(((uint8_t)((uint64_t)(t230) << ((uint64_t)(t231) & 7)))));
  (*t218) = ((uint8_t)(((t221) | (t232))));
  /* FG_DIRECT_SLICE_INDEX */
  t233 = v24;
  t234 = ((uint8_t *)(v23).data) + gg_index(t233, ((int64_t)UINT64_C(32)));
  /* FG_DIRECT_SLICE_INDEX */
  t235 = v24;
  t236 = ((uint8_t *)(v23).data) + gg_index(t235, ((int64_t)UINT64_C(32)));
  t237 = ((uint8_t)((*t236)));
  /* FG_DIRECT_SLICE_INDEX */
  t238 = v24;
  t239 = ((uint8_t *)(v19).data) + gg_index(t238, ((int64_t)UINT64_C(32)));
  t240 = ((uint8_t)((*t239)));
  t241 = ((int64_t)(((int64_t)UINT64_C(2))));
  t242 = ((int64_t)(v20));
  t243 = ((int64_t)(((int64_t)((uint64_t)(t241) * (uint64_t)(t242)))));
  t244 = ((int64_t)(((int64_t)UINT64_C(1))));
  t245 = ((uint8_t)(((uint8_t)(((int64_t)((uint64_t)(t243) + (uint64_t)(t244)))))));
  t246 = ((uint8_t)(((t240) >> ((uint64_t)(t245) & 7))));
  t247 = ((uint8_t)(((int64_t)UINT64_C(1))));
  t248 = ((uint8_t)(((t246) & (t247))));
  t249 = ((int64_t)(((int64_t)UINT64_C(4))));
  t250 = ((uint8_t)(((uint8_t)((uint64_t)(t248) << ((uint64_t)(t249) & 7)))));
  (*t234) = ((uint8_t)(((t237) | (t250))));
  }
label16:;
  t215++; goto label15;
label17:;
  /* FG_DIRECT_SLICE_INDEX */
  t251 = ((int64_t)UINT64_C(0));
  t252 = ((gg_type_26 *)(((*(v3))).f16).data) + gg_index(t251, ((int64_t)UINT64_C(256)));
  t253 = gg_view(((*t252)).data, 8);
  t254 = ((int64_t)(((int64_t)UINT64_C(2))));
  t255 = ((int64_t)(v20));
  t256 = ((int64_t)((uint64_t)(t254) * (uint64_t)(t255)));
  t257 = ((gg_type_25 *)t253.data) + gg_index(t256, t253.len);
  (*t257) = v22;
  /* FG_DIRECT_SLICE_INDEX */
  t258 = ((int64_t)UINT64_C(0));
  t259 = ((gg_type_26 *)(((*(v3))).f16).data) + gg_index(t258, ((int64_t)UINT64_C(256)));
  t260 = gg_view(((*t259)).data, 8);
  t261 = ((int64_t)(((int64_t)UINT64_C(2))));
  t262 = ((int64_t)(v20));
  t263 = ((int64_t)(((int64_t)((uint64_t)(t261) * (uint64_t)(t262)))));
  t264 = ((int64_t)(((int64_t)UINT64_C(1))));
  t265 = ((int64_t)((uint64_t)(t263) + (uint64_t)(t264)));
  t266 = ((gg_type_25 *)t260.data) + gg_index(t265, t260.len);
  (*t266) = v23;
  }
label13:;
  t201++; goto label12;
label14:;
  }
  } else {
  {
  t267 = ((int64_t)(v0));
  t268 = ((int64_t)(((int64_t)UINT64_C(2))));
  if (!(((t267) == (t268)))) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t269 = ((int64_t)UINT64_C(0));
  t270 = ((float *)(((*(v3))).f17).data) + gg_index(t269, ((int64_t)UINT64_C(1)));
  t271 = v1;
  t272 = ((int64_t)(v2));
  t273 = ((int64_t)(((int64_t)UINT64_C(208))));
  t274 = ((int64_t)(((int64_t)((uint64_t)(t272) + (uint64_t)(t273)))));
  t275 = gg_function_33(t271, t274, base + 0);
  t276 = fg141_half(t275);
  (*t270) = ((float)(t276));
  t278 = ((int64_t)UINT64_C(16));
  t277 = 0;
label18:;
  if (t277 >= t278) goto label20;
  v25 = t277;
  {
  t279 = v3;
  t280 = ((int64_t)(v25));
  t281 = v1;
  t282 = ((int64_t)(v2));
  t283 = ((int64_t)(((int64_t)UINT64_C(192))));
  t284 = ((int64_t)(((int64_t)((uint64_t)(t282) + (uint64_t)(t283)))));
  t285 = ((int64_t)(v25));
  t286 = ((int64_t)((uint64_t)(t284) + (uint64_t)(t285)));
  t287 = ((uint8_t *)t281.data) + gg_index(t286, t281.len);
  t288 = ((int32_t)(((int32_t)(((int8_t)((*t287)))))));
  gg_function_31(t279, t280, t288, base + 0);
  }
label19:;
  t277++; goto label18;
label20:;
  t290 = ((int64_t)UINT64_C(2));
  t289 = 0;
label21:;
  if (t289 >= t290) goto label23;
  v26 = t289;
  {
  t292 = ((int64_t)UINT64_C(32));
  t291 = 0;
label24:;
  if (t291 >= t292) goto label26;
  v27 = t291;
  {
  t293 = v1;
  t294 = ((int64_t)(v2));
  t295 = ((int64_t)(((int64_t)UINT64_C(128))));
  t296 = ((int64_t)(((int64_t)((uint64_t)(t294) + (uint64_t)(t295)))));
  t297 = ((int64_t)(v26));
  t298 = ((int64_t)(((int64_t)UINT64_C(32))));
  t299 = ((int64_t)(((int64_t)((uint64_t)(t297) * (uint64_t)(t298)))));
  t300 = ((int64_t)(((int64_t)((uint64_t)(t296) + (uint64_t)(t299)))));
  t301 = ((int64_t)(v27));
  t302 = ((int64_t)((uint64_t)(t300) + (uint64_t)(t301)));
  t303 = ((uint8_t *)t293.data) + gg_index(t302, t293.len);
  v28 = ((uint8_t)((*t303)));
  t304 = v1;
  t305 = ((int64_t)(v2));
  t306 = ((int64_t)(v26));
  t307 = ((int64_t)(((int64_t)UINT64_C(64))));
  t308 = ((int64_t)(((int64_t)((uint64_t)(t306) * (uint64_t)(t307)))));
  t309 = ((int64_t)(((int64_t)((uint64_t)(t305) + (uint64_t)(t308)))));
  t310 = ((int64_t)(v27));
  t311 = ((int64_t)((uint64_t)(t309) + (uint64_t)(t310)));
  t312 = ((uint8_t *)t304.data) + gg_index(t311, t304.len);
  v29 = ((uint8_t)((*t312)));
  t313 = v1;
  t314 = ((int64_t)(v2));
  t315 = ((int64_t)(v26));
  t316 = ((int64_t)(((int64_t)UINT64_C(64))));
  t317 = ((int64_t)(((int64_t)((uint64_t)(t315) * (uint64_t)(t316)))));
  t318 = ((int64_t)(((int64_t)((uint64_t)(t314) + (uint64_t)(t317)))));
  t319 = ((int64_t)(((int64_t)UINT64_C(32))));
  t320 = ((int64_t)(((int64_t)((uint64_t)(t318) + (uint64_t)(t319)))));
  t321 = ((int64_t)(v27));
  t322 = ((int64_t)((uint64_t)(t320) + (uint64_t)(t321)));
  t323 = ((uint8_t *)t313.data) + gg_index(t322, t313.len);
  v30 = ((uint8_t)((*t323)));
  /* FG_DIRECT_SLICE_INDEX */
  t324 = ((int64_t)UINT64_C(0));
  t325 = ((gg_type_26 *)(((*(v3))).f16).data) + gg_index(t324, ((int64_t)UINT64_C(256)));
  t326 = gg_view(((*t325)).data, 8);
  t327 = ((int64_t)(v26));
  t328 = ((int64_t)(((int64_t)UINT64_C(4))));
  t329 = ((int64_t)((uint64_t)(t327) * (uint64_t)(t328)));
  t330 = ((gg_type_25 *)t326.data) + gg_index(t329, t326.len);
  t331 = gg_view(((*t330)).data, 32);
  t332 = v27;
  t333 = ((uint8_t *)t331.data) + gg_index(t332, t331.len);
  t334 = ((uint8_t)(v29));
  t335 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t336 = ((uint8_t)(((t334) & (t335))));
  t337 = ((uint8_t)(v28));
  t338 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t339 = ((uint8_t)(((t337) & (t338))));
  t340 = ((int64_t)(((int64_t)UINT64_C(4))));
  t341 = ((uint8_t)(((uint8_t)((uint64_t)(t339) << ((uint64_t)(t340) & 7)))));
  (*t333) = ((uint8_t)(((t336) | (t341))));
  /* FG_DIRECT_SLICE_INDEX */
  t342 = ((int64_t)UINT64_C(0));
  t343 = ((gg_type_26 *)(((*(v3))).f16).data) + gg_index(t342, ((int64_t)UINT64_C(256)));
  t344 = gg_view(((*t343)).data, 8);
  t345 = ((int64_t)(v26));
  t346 = ((int64_t)(((int64_t)UINT64_C(4))));
  t347 = ((int64_t)(((int64_t)((uint64_t)(t345) * (uint64_t)(t346)))));
  t348 = ((int64_t)(((int64_t)UINT64_C(1))));
  t349 = ((int64_t)((uint64_t)(t347) + (uint64_t)(t348)));
  t350 = ((gg_type_25 *)t344.data) + gg_index(t349, t344.len);
  t351 = gg_view(((*t350)).data, 32);
  t352 = v27;
  t353 = ((uint8_t *)t351.data) + gg_index(t352, t351.len);
  t354 = ((uint8_t)(v30));
  t355 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t356 = ((uint8_t)(((t354) & (t355))));
  t357 = ((uint8_t)(v28));
  t358 = ((uint8_t)(((int64_t)UINT64_C(12))));
  t359 = ((uint8_t)(((t357) & (t358))));
  t360 = ((int64_t)(((int64_t)UINT64_C(2))));
  t361 = ((uint8_t)(((uint8_t)((uint64_t)(t359) << ((uint64_t)(t360) & 7)))));
  (*t353) = ((uint8_t)(((t356) | (t361))));
  /* FG_DIRECT_SLICE_INDEX */
  t362 = ((int64_t)UINT64_C(0));
  t363 = ((gg_type_26 *)(((*(v3))).f16).data) + gg_index(t362, ((int64_t)UINT64_C(256)));
  t364 = gg_view(((*t363)).data, 8);
  t365 = ((int64_t)(v26));
  t366 = ((int64_t)(((int64_t)UINT64_C(4))));
  t367 = ((int64_t)(((int64_t)((uint64_t)(t365) * (uint64_t)(t366)))));
  t368 = ((int64_t)(((int64_t)UINT64_C(2))));
  t369 = ((int64_t)((uint64_t)(t367) + (uint64_t)(t368)));
  t370 = ((gg_type_25 *)t364.data) + gg_index(t369, t364.len);
  t371 = gg_view(((*t370)).data, 32);
  t372 = v27;
  t373 = ((uint8_t *)t371.data) + gg_index(t372, t371.len);
  t374 = ((uint8_t)(v29));
  t375 = ((int64_t)(((int64_t)UINT64_C(4))));
  t376 = ((uint8_t)(((t374) >> ((uint64_t)(t375) & 7))));
  t377 = ((uint8_t)(v28));
  t378 = ((uint8_t)(((int64_t)UINT64_C(48))));
  t379 = ((uint8_t)(((t377) & (t378))));
  (*t373) = ((uint8_t)(((t376) | (t379))));
  /* FG_DIRECT_SLICE_INDEX */
  t380 = ((int64_t)UINT64_C(0));
  t381 = ((gg_type_26 *)(((*(v3))).f16).data) + gg_index(t380, ((int64_t)UINT64_C(256)));
  t382 = gg_view(((*t381)).data, 8);
  t383 = ((int64_t)(v26));
  t384 = ((int64_t)(((int64_t)UINT64_C(4))));
  t385 = ((int64_t)(((int64_t)((uint64_t)(t383) * (uint64_t)(t384)))));
  t386 = ((int64_t)(((int64_t)UINT64_C(3))));
  t387 = ((int64_t)((uint64_t)(t385) + (uint64_t)(t386)));
  t388 = ((gg_type_25 *)t382.data) + gg_index(t387, t382.len);
  t389 = gg_view(((*t388)).data, 32);
  t390 = v27;
  t391 = ((uint8_t *)t389.data) + gg_index(t390, t389.len);
  t392 = ((uint8_t)(v30));
  t393 = ((int64_t)(((int64_t)UINT64_C(4))));
  t394 = ((uint8_t)(((t392) >> ((uint64_t)(t393) & 7))));
  t395 = ((uint8_t)(v28));
  t396 = ((uint8_t)(((int64_t)UINT64_C(192))));
  t397 = ((uint8_t)(((t395) & (t396))));
  t398 = ((int64_t)(((int64_t)UINT64_C(2))));
  t399 = ((uint8_t)(((t397) >> ((uint64_t)(t398) & 7))));
  (*t391) = ((uint8_t)(((t394) | (t399))));
  }
label25:;
  t291++; goto label24;
label26:;
  }
label22:;
  t289++; goto label21;
label23:;
  }
  }
  }
  }
gg_done:;
}
static int64_t gg_function_4(int64_t p0, int64_t base) {
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
  t10 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t10)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_29(int64_t p0, gg_slice p1, gg_slice p2, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
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
  gg_type_26 t27;
  gg_type_25 t28;
  uint8_t t29;
  uint8_t t30;
  int64_t t31;
  gg_type_25 t32;
  int64_t t33;
  gg_type_26 t34;
  int64_t t35;
  float t36;
  float t37;
  int64_t t38;
  float t39;
  float t40;
  int64_t t41;
  gg_type_28 t42;
  int32_t t43;
  int32_t t44;
  int64_t t45;
  gg_type_28 t46;
  int64_t t47;
  gg_type_28 t48;
  int32_t t49;
  int32_t t50;
  int64_t t51;
  gg_type_28 t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  gg_slice t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  gg_type_30 t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  gg_slice t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  gg_type_28 * t72;
  gg_slice t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int32_t * t77;
  gg_slice t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  gg_type_28 * t82;
  gg_slice t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int32_t * t87;
  int64_t t88;
  gg_type_26 * t89;
  gg_slice t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  gg_type_25 * t94;
  gg_slice t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint8_t * t99;
  int64_t t100;
  int64_t t101;
  gg_slice t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  float * t108;
  int64_t t109;
  float * t110;
  float t111;
  float t112;
  float t113;
  float t114;
  float t115;
  int64_t t116;
  float * t117;
  float t118;
  float t119;
  float t120;
  int64_t v0;
  gg_slice v1;
  gg_slice v2;
  int64_t v3;
  gg_type_19 v4;
  gg_type_24 v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  float v10;
  int64_t v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  v3 = gg_function_4(t1, base + 0);
  t2 = ((int64_t)(v3));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) < (t3));
  if (t4) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v3));
  v4 = gg_function_5(t5, base + 0);
  t6 = ((int64_t)((v4).f13));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) == (t7));
  t9 = t8;
  if (!t9) {
  t10 = ((int64_t)((v2).len));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t10) <= (t11));
  }
  t12 = t9;
  t13 = t12;
  if (!t13) {
  t14 = ((int64_t)((v2).len));
  t15 = ((int64_t)(((int64_t)UINT64_C(256))));
  t16 = ((int64_t)(((int64_t)gg_mod(t14, t15))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t16) != (t17));
  }
  t18 = t13;
  t19 = t18;
  if (!t19) {
  t20 = ((int64_t)((v1).len));
  t21 = ((int64_t)((v2).len));
  t22 = ((int64_t)(((int64_t)UINT64_C(256))));
  t23 = ((int64_t)(((int64_t)gg_div(t21, t22))));
  t24 = ((int64_t)((v4).f13));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t19 = ((t20) < (t25));
  }
  t26 = t19;
  if (t26) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  memset(&(v5), 0, sizeof(v5));
  memset(&t29, 0, sizeof(t29));
  t30 = ((uint8_t)(t29));
  for (t31=0; t31<32; t31++) (t28).data[t31] = t30;
  t32 = t28;
  for (t33=0; t33<8; t33++) (t27).data[t33] = t32;
  t34 = t27;
  for (t35=0; t35<256; t35++) ((v5).f16).data[t35] = t34;
  memset(&t36, 0, sizeof(t36));
  t37 = ((float)(t36));
  for (t38=0; t38<1; t38++) ((v5).f17).data[t38] = t37;
  memset(&t39, 0, sizeof(t39));
  t40 = ((float)(t39));
  for (t41=0; t41<1; t41++) ((v5).f18).data[t41] = t40;
  memset(&t43, 0, sizeof(t43));
  t44 = ((int32_t)(t43));
  for (t45=0; t45<8; t45++) (t42).data[t45] = t44;
  t46 = t42;
  for (t47=0; t47<512; t47++) ((v5).f19).data[t47] = t46;
  memset(&t49, 0, sizeof(t49));
  t50 = ((int32_t)(t49));
  for (t51=0; t51<8; t51++) (t48).data[t51] = t50;
  t52 = t48;
  for (t53=0; t53<512; t53++) ((v5).f20).data[t53] = t52;
  t55 = ((int64_t)((v2).len));
  t56 = ((int64_t)(((int64_t)UINT64_C(256))));
  t57 = ((int64_t)gg_div(t55, t56));
  t54 = 0;
label0:;
  if (t54 >= t57) goto label2;
  v6 = t54;
  {
  t58 = ((int64_t)(v3));
  t59 = v1;
  t60 = ((int64_t)(v6));
  t61 = ((int64_t)((v4).f13));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = &(v5);
  gg_function_30(t58, t59, t62, t63, base + 0);
  t65 = ((int64_t)UINT64_C(256));
  t64 = 0;
label3:;
  if (t64 >= t65) goto label5;
  v7 = t64;
  {
  t66 = ((int64_t)(v7));
  t67 = ((int64_t)((v4).f14));
  v8 = ((int64_t)(((int64_t)gg_div(t66, t67))));
  t68 = gg_view(((v5).f19).data, 512);
  t69 = ((int64_t)(v8));
  t70 = ((int64_t)(((int64_t)UINT64_C(8))));
  t71 = ((int64_t)gg_div(t69, t70));
  t72 = ((gg_type_28 *)t68.data) + gg_index(t71, t68.len);
  t73 = gg_view(((*t72)).data, 8);
  t74 = ((int64_t)(v8));
  t75 = ((int64_t)(((int64_t)UINT64_C(8))));
  t76 = ((int64_t)gg_mod(t74, t75));
  t77 = ((int32_t *)t73.data) + gg_index(t76, t73.len);
  v9 = ((float)(((float)((*t77)))));
  t78 = gg_view(((v5).f20).data, 512);
  t79 = ((int64_t)(v8));
  t80 = ((int64_t)(((int64_t)UINT64_C(8))));
  t81 = ((int64_t)gg_div(t79, t80));
  t82 = ((gg_type_28 *)t78.data) + gg_index(t81, t78.len);
  t83 = gg_view(((*t82)).data, 8);
  t84 = ((int64_t)(v8));
  t85 = ((int64_t)(((int64_t)UINT64_C(8))));
  t86 = ((int64_t)gg_mod(t84, t85));
  t87 = ((int32_t *)t83.data) + gg_index(t86, t83.len);
  v10 = ((float)(((float)((*t87)))));
  /* FG_DIRECT_SLICE_INDEX */
  t88 = ((int64_t)UINT64_C(0));
  t89 = ((gg_type_26 *)((v5).f16).data) + gg_index(t88, ((int64_t)UINT64_C(256)));
  t90 = gg_view(((*t89)).data, 8);
  t91 = ((int64_t)(v7));
  t92 = ((int64_t)(((int64_t)UINT64_C(32))));
  t93 = ((int64_t)gg_div(t91, t92));
  t94 = ((gg_type_25 *)t90.data) + gg_index(t93, t90.len);
  t95 = gg_view(((*t94)).data, 32);
  t96 = ((int64_t)(v7));
  t97 = ((int64_t)(((int64_t)UINT64_C(32))));
  t98 = ((int64_t)gg_mod(t96, t97));
  t99 = ((uint8_t *)t95.data) + gg_index(t98, t95.len);
  t100 = ((int64_t)(((int32_t)((*t99)))));
  t101 = ((int64_t)((v4).f15));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t100) - (uint64_t)(t101)))));
  t102 = v2;
  t103 = ((int64_t)(v6));
  t104 = ((int64_t)(((int64_t)UINT64_C(256))));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t104)))));
  t106 = ((int64_t)(v7));
  t107 = ((int64_t)((uint64_t)(t105) + (uint64_t)(t106)));
  t108 = ((float *)t102.data) + gg_index(t107, t102.len);
  /* FG_DIRECT_SLICE_INDEX */
  t109 = ((int64_t)UINT64_C(0));
  t110 = ((float *)((v5).f17).data) + gg_index(t109, ((int64_t)UINT64_C(1)));
  t111 = ((float)((*t110)));
  t112 = ((float)(v9));
  t113 = ((float)(((t111) * (t112))));
  t114 = ((float)(((float)(v11))));
  t115 = ((float)(((t113) * (t114))));
  /* FG_DIRECT_SLICE_INDEX */
  t116 = ((int64_t)UINT64_C(0));
  t117 = ((float *)((v5).f18).data) + gg_index(t116, ((int64_t)UINT64_C(1)));
  t118 = ((float)((*t117)));
  t119 = ((float)(v10));
  t120 = ((float)(((t118) * (t119))));
  (*t108) = ((float)(((t115) - (t120))));
  }
label4:;
  t64++; goto label3;
label5:;
  }
label1:;
  t54++; goto label0;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_69(int64_t p0, int64_t p1, int64_t base) {
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
static int64_t gg_function_3(int64_t p0, int64_t base) {
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
static void gg_function_16(gg_type_20 p0, int64_t p1, int64_t base) {
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
  gg_type_20 v0;
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
  v2 = gg_function_3(t6, base + 0);
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
  t35 = gg_function_6(t31, t34, base + 0);
  v3 = gg_function_7(t26, t35, base + 0);
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
static uint8_t gg_function_68(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t * t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  gg_type_20 t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t * t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  gg_type_20 t57;
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
  int64_t * t74;
  int64_t t75;
  int64_t * t76;
  int64_t t77;
  int64_t * t78;
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
  int64_t t119;
  int64_t t120;
  int64_t t121;
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
  int64_t * t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t * t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t * t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t * t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
  uint8_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  gg_slice t167;
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
  uint8_t * t179;
  gg_slice t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  uint8_t * t184;
  gg_slice t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  gg_slice t195;
  gg_slice t196;
  uint8_t t197;
  int64_t t198;
  int64_t t199;
  gg_type_20 t200;
  gg_slice t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  float * t206;
  float t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  int64_t v5;
  gg_type_19 v6;
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
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  gg_slice v23;
  int64_t v24;
  gg_slice v25;
  int64_t v26;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)((((*(v0))).f4).f2));
  v5 = gg_function_4(t1, base + 0);
  t2 = ((int64_t)(v5));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) < (t3));
  t5 = t4;
  if (!t5) {
  t6 = ((int64_t)((((*(v0))).f7).f2));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t6) != (t7));
  }
  t8 = t5;
  t9 = t8;
  if (!t9) {
  /* FG_DIRECT_SLICE_INDEX */
  t10 = ((int64_t)UINT64_C(0));
  t11 = ((int64_t *)((((*(v0))).f7).f1).data) + gg_index(t10, ((int64_t)UINT64_C(4)));
  t12 = ((int64_t)((*t11)));
  t13 = ((int64_t)(((int64_t)UINT64_C(4))));
  t9 = ((t12) != (t13));
  }
  t14 = t9;
  if (t14) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v5));
  v6 = gg_function_5(t15, base + 0);
  t16 = ((int64_t)((v6).f13));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) <= (t17));
  if (t18) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  v7 = ((int64_t)(((int64_t)UINT64_C(256))));
  t19 = &(((*(v0))).f7);
  v8 = gg_function_9(t19, base + 0);
  t20 = ((int64_t)(v8));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) <= (t21));
  t23 = t22;
  if (!t23) {
  t24 = ((int64_t)(v8));
  t25 = ((int64_t)(v7));
  t26 = ((int64_t)(((int64_t)gg_mod(t24, t25))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t26) != (t27));
  }
  t28 = t23;
  t29 = t28;
  if (!t29) {
  /* FG_DIRECT_SLICE_INDEX */
  t30 = ((int64_t)UINT64_C(0));
  t31 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t30, ((int64_t)UINT64_C(4)));
  t32 = ((int64_t)((*t31)));
  t33 = ((int64_t)(v7));
  t34 = ((int64_t)(((int64_t)gg_mod(t32, t33))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t34) != (t35));
  }
  t36 = t29;
  if (t36) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t37 = ((int64_t)((((*(v0))).f4).f3));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t37) <= (t38));
  t40 = t39;
  if (!t40) {
  t41 = ((int64_t)((((*(v0))).f4).f3));
  t42 = ((int64_t)((v1).len));
  t40 = ((t41) > (t42));
  }
  t43 = t40;
  if (t43) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t44 = ((int64_t)((v3).len));
  t45 = ((int64_t)(v7));
  t46 = ((int64_t)(((*(v0))).f10));
  t47 = gg_function_6(t45, t46, base + 0);
  t48 = ((int64_t)(t47));
  t49 = ((t44) < (t48));
  t50 = t49;
  if (!t50) {
  t51 = ((int64_t)((v4).len));
  t52 = ((int64_t)((v6).f13));
  t53 = ((int64_t)(((*(v0))).f10));
  t54 = gg_function_6(t52, t53, base + 0);
  t55 = ((int64_t)(t54));
  t50 = ((t51) < (t55));
  }
  t56 = t50;
  if (t56) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t57 = &(((*(v0))).f7);
  t58 = ((int64_t)((v2).len));
  gg_function_16(t57, t58, base + 0);
  t59 = ((int64_t)(v8));
  t60 = ((int64_t)(v7));
  v9 = ((int64_t)(((int64_t)gg_div(t59, t60))));
  t61 = ((int64_t)(v9));
  t62 = ((int64_t)(((*(v0))).f10));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) + (uint64_t)(t62)))));
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) - (uint64_t)(t64)))));
  t66 = ((int64_t)(((*(v0))).f10));
  v10 = ((int64_t)(((int64_t)gg_div(t65, t66))));
  t67 = ((int64_t)(v10));
  t68 = ((int64_t)(((*(v0))).f9));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t69 = ((int64_t)(v11));
  t70 = ((int64_t)(v10));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) + (uint64_t)(t70)))));
  t72 = ((int64_t)(v9));
  v12 = gg_function_69(t71, t72, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t73 = ((int64_t)UINT64_C(0));
  t74 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t73, ((int64_t)UINT64_C(4)));
  v13 = ((int64_t)((*t74)));
  /* FG_DIRECT_SLICE_INDEX */
  t75 = ((int64_t)UINT64_C(1));
  t76 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t75, ((int64_t)UINT64_C(4)));
  v14 = ((int64_t)((*t76)));
  /* FG_DIRECT_SLICE_INDEX */
  t77 = ((int64_t)UINT64_C(2));
  t78 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t77, ((int64_t)UINT64_C(4)));
  v15 = ((int64_t)((*t78)));
  v16 = ((int64_t)(v11));
label1:;
  t79 = ((int64_t)(v16));
  t80 = ((int64_t)(v12));
  if (!(((t79) < (t80)))) goto label2;
  {
  t81 = ((int64_t)(v16));
  t82 = ((int64_t)(v7));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t82)))));
  t83 = ((int64_t)(v17));
  t84 = ((int64_t)(v13));
  t85 = ((int64_t)(v14));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))));
  t87 = ((int64_t)(v15));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t87)))));
  v18 = ((int64_t)(((int64_t)gg_div(t83, t88))));
  t89 = ((int64_t)(v17));
  t90 = ((int64_t)(v18));
  t91 = ((int64_t)(v13));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
  t93 = ((int64_t)(v14));
  t94 = ((int64_t)(((int64_t)((uint64_t)(t92) * (uint64_t)(t93)))));
  t95 = ((int64_t)(v15));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t94) * (uint64_t)(t95)))));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t89) - (uint64_t)(t96)))));
  t98 = ((int64_t)(v13));
  t99 = ((int64_t)(v14));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  v19 = ((int64_t)(((int64_t)gg_div(t97, t100))));
  t101 = ((int64_t)(v17));
  t102 = ((int64_t)(v18));
  t103 = ((int64_t)(v13));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t102) * (uint64_t)(t103)))));
  t105 = ((int64_t)(v14));
  t106 = ((int64_t)(((int64_t)((uint64_t)(t104) * (uint64_t)(t105)))));
  t107 = ((int64_t)(v15));
  t108 = ((int64_t)(((int64_t)((uint64_t)(t106) * (uint64_t)(t107)))));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t101) - (uint64_t)(t108)))));
  t110 = ((int64_t)(v19));
  t111 = ((int64_t)(v13));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) * (uint64_t)(t111)))));
  t113 = ((int64_t)(v14));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t113)))));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t109) - (uint64_t)(t114)))));
  t116 = ((int64_t)(v13));
  v20 = ((int64_t)(((int64_t)gg_div(t115, t116))));
  t117 = ((int64_t)(v17));
  t118 = ((int64_t)(v18));
  t119 = ((int64_t)(v13));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t118) * (uint64_t)(t119)))));
  t121 = ((int64_t)(v14));
  t122 = ((int64_t)(((int64_t)((uint64_t)(t120) * (uint64_t)(t121)))));
  t123 = ((int64_t)(v15));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t122) * (uint64_t)(t123)))));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t117) - (uint64_t)(t124)))));
  t126 = ((int64_t)(v19));
  t127 = ((int64_t)(v13));
  t128 = ((int64_t)(((int64_t)((uint64_t)(t126) * (uint64_t)(t127)))));
  t129 = ((int64_t)(v14));
  t130 = ((int64_t)(((int64_t)((uint64_t)(t128) * (uint64_t)(t129)))));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t125) - (uint64_t)(t130)))));
  t132 = ((int64_t)(v20));
  t133 = ((int64_t)(v13));
  t134 = ((int64_t)(((int64_t)((uint64_t)(t132) * (uint64_t)(t133)))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t131) - (uint64_t)(t134)))));
  t135 = ((int64_t)(v21));
  t136 = ((int64_t)(v7));
  t137 = ((int64_t)(((int64_t)gg_div(t135, t136))));
  /* FG_DIRECT_SLICE_INDEX */
  t138 = ((int64_t)UINT64_C(0));
  t139 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t138, ((int64_t)UINT64_C(4)));
  t140 = ((int64_t)((*t139)));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t137) * (uint64_t)(t140)))));
  t142 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t143 = ((int64_t)UINT64_C(1));
  t144 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t143, ((int64_t)UINT64_C(4)));
  t145 = ((int64_t)((*t144)));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t142) * (uint64_t)(t145)))));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t141) + (uint64_t)(t146)))));
  t148 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t149 = ((int64_t)UINT64_C(2));
  t150 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t149, ((int64_t)UINT64_C(4)));
  t151 = ((int64_t)((*t150)));
  t152 = ((int64_t)(((int64_t)((uint64_t)(t148) * (uint64_t)(t151)))));
  t153 = ((int64_t)(((int64_t)((uint64_t)(t147) + (uint64_t)(t152)))));
  t154 = ((int64_t)(v18));
  /* FG_DIRECT_SLICE_INDEX */
  t155 = ((int64_t)UINT64_C(3));
  t156 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t155, ((int64_t)UINT64_C(4)));
  t157 = ((int64_t)((*t156)));
  t158 = ((int64_t)(((int64_t)((uint64_t)(t154) * (uint64_t)(t157)))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t158)))));
  t159 = ((int64_t)(v22));
  t160 = ((int64_t)(((int64_t)UINT64_C(0))));
  t161 = ((t159) >= (t160));
  t162 = t161;
  if (t162) {
  t163 = ((int64_t)(v22));
  t164 = ((int64_t)((v6).f13));
  t165 = ((int64_t)(((int64_t)((uint64_t)(t163) + (uint64_t)(t164)))));
  t166 = ((int64_t)((v1).len));
  t162 = ((t165) <= (t166));
  }
  if (!(t162)) gg_fail("assertion failed");
  t167 = v4;
  t168 = ((int64_t)(((*(v0))).f9));
  t169 = ((int64_t)((v6).f13));
  t170 = ((int64_t)((uint64_t)(t168) * (uint64_t)(t169)));
  t171 = ((int64_t)(((*(v0))).f9));
  t172 = ((int64_t)(((int64_t)UINT64_C(1))));
  t173 = ((int64_t)(((int64_t)((uint64_t)(t171) + (uint64_t)(t172)))));
  t174 = ((int64_t)((v6).f13));
  t175 = ((int64_t)((uint64_t)(t173) * (uint64_t)(t174)));
  v23 = gg_sub(t167, t170, t175, sizeof(uint8_t));
  t177 = (v6).f13;
  t176 = 0;
label3:;
  if (t176 >= t177) goto label5;
  v24 = t176;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t178 = v24;
  t179 = ((uint8_t *)(v23).data) + gg_index(t178, (v23).len);
  t180 = v1;
  t181 = ((int64_t)(v22));
  t182 = ((int64_t)(v24));
  t183 = ((int64_t)((uint64_t)(t181) + (uint64_t)(t182)));
  t184 = ((uint8_t *)t180.data) + gg_index(t183, t180.len);
  (*t179) = ((uint8_t)((*t184)));
  }
label4:;
  t176++; goto label3;
label5:;
  t185 = v3;
  t186 = ((int64_t)(((*(v0))).f9));
  t187 = ((int64_t)(v7));
  t188 = ((int64_t)((uint64_t)(t186) * (uint64_t)(t187)));
  t189 = ((int64_t)(((*(v0))).f9));
  t190 = ((int64_t)(((int64_t)UINT64_C(1))));
  t191 = ((int64_t)(((int64_t)((uint64_t)(t189) + (uint64_t)(t190)))));
  t192 = ((int64_t)(v7));
  t193 = ((int64_t)((uint64_t)(t191) * (uint64_t)(t192)));
  v25 = gg_sub(t185, t188, t193, sizeof(float));
  t194 = ((int64_t)((((*(v0))).f4).f2));
  t195 = v23;
  t196 = v25;
  t197 = gg_function_29(t194, t195, t196, base + 0);
  if (!(t197)) gg_fail("assertion failed");
  t199 = v7;
  t198 = 0;
label6:;
  if (t198 >= t199) goto label8;
  v26 = t198;
  {
  t200 = &(((*(v0))).f7);
  t201 = v2;
  t202 = ((int64_t)(v17));
  t203 = ((int64_t)(v26));
  t204 = ((int64_t)(((int64_t)((uint64_t)(t202) + (uint64_t)(t203)))));
  /* FG_DIRECT_SLICE_INDEX */
  t205 = v26;
  t206 = ((float *)(v25).data) + gg_index(t205, (v25).len);
  t207 = ((float)((*t206)));
  gg_function_35(t200, t201, t204, t207, base + 0);
  }
label7:;
  t198++; goto label6;
label8:;
  t208 = v16;
  t209 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)((uint64_t)(t208) + (uint64_t)(t209)));
  }
  goto label1;
label2:;
  t210 = ((int64_t)(((int64_t)UINT64_C(34))));
  t211 = ((int64_t)(v9));
  fg143_witness(t210, t211);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint16_t gg_function_45(gg_slice p0, int64_t p1, int64_t base) {
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
static uint32_t gg_function_12(gg_slice p0, int64_t p1, int64_t base) {
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
static float gg_function_43(gg_type_20 p0, gg_slice p1, int64_t p2, int64_t base) {
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
  gg_type_20 v0;
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
  t6 = gg_function_12(t4, t5, base + 0);
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
  t13 = gg_function_45(t11, t12, base + 0);
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
  t20 = gg_function_45(t18, t19, base + 0);
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
  t30 = gg_function_12(t28, t29, base + 0);
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
  t36 = gg_function_45(t34, t35, base + 0);
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
static int64_t gg_function_17(gg_type_12 p0, int64_t base) {
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
  gg_type_12 v0;
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
static uint8_t gg_function_67(gg_type_20 p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  gg_type_20 v0;
  v0 = p0;
  {
  t1 = ((int64_t)(((*(v0))).f2));
  t2 = gg_function_3(t1, base + 0);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = ((t3) > (t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_66(gg_type_12 p0, gg_slice p1, gg_slice p2, int64_t base) {
  uint8_t t0;
  gg_type_20 t1;
  uint8_t t2;
  uint8_t t3;
  uint8_t t4;
  uint8_t t5;
  gg_type_20 t6;
  uint8_t t7;
  uint8_t t8;
  uint8_t t9;
  gg_type_20 t10;
  int64_t t11;
  int64_t t12;
  gg_type_20 t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  gg_type_20 t17;
  int64_t t18;
  gg_type_20 t19;
  int64_t t20;
  gg_type_12 t21;
  gg_type_20 t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  gg_type_20 t29;
  int64_t t30;
  gg_type_20 t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  gg_slice t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t * t39;
  gg_slice t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  uint8_t * t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  gg_type_20 t49;
  gg_slice t50;
  int64_t t51;
  gg_type_20 t52;
  gg_slice t53;
  gg_type_20 t54;
  int64_t t55;
  int64_t t56;
  float t57;
  int64_t t58;
  int64_t t59;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = &(((*(v0))).f4);
  t2 = gg_function_67(t1, base + 0);
  t3 = t2;
  t4 = (!(t3));
  t5 = t4;
  if (!t5) {
  t6 = &(((*(v0))).f7);
  t7 = gg_function_67(t6, base + 0);
  t8 = t7;
  t5 = (!(t8));
  }
  t9 = t5;
  if (t9) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t10 = &(((*(v0))).f4);
  t11 = gg_function_9(t10, base + 0);
  t12 = ((int64_t)(t11));
  t13 = &(((*(v0))).f7);
  t14 = gg_function_9(t13, base + 0);
  t15 = ((int64_t)(t14));
  t16 = ((t12) != (t15));
  if (t16) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t17 = &(((*(v0))).f7);
  t18 = ((int64_t)((v2).len));
  gg_function_16(t17, t18, base + 0);
  t19 = &(((*(v0))).f4);
  t20 = ((int64_t)((v1).len));
  gg_function_16(t19, t20, base + 0);
  t21 = v0;
  v3 = gg_function_17(t21, base + 0);
  t22 = &(((*(v0))).f7);
  v4 = gg_function_9(t22, base + 0);
  t23 = ((int64_t)((((*(v0))).f4).f2));
  t24 = ((int64_t)((((*(v0))).f7).f2));
  t25 = ((t23) == (t24));
  if (t25) {
  {
  t26 = ((int64_t)((((*(v0))).f4).f2));
  v5 = gg_function_3(t26, base + 0);
label1:;
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(v4));
  if (!(((t27) < (t28)))) goto label2;
  {
  t29 = &(((*(v0))).f4);
  t30 = ((int64_t)(v3));
  v6 = gg_function_11(t29, t30, base + 0);
  t31 = &(((*(v0))).f7);
  t32 = ((int64_t)(v3));
  v7 = gg_function_11(t31, t32, base + 0);
  t34 = v5;
  t33 = 0;
label3:;
  if (t33 >= t34) goto label5;
  v8 = t33;
  {
  t35 = v2;
  t36 = ((int64_t)(v7));
  t37 = ((int64_t)(v8));
  t38 = ((int64_t)((uint64_t)(t36) + (uint64_t)(t37)));
  t39 = ((uint8_t *)t35.data) + gg_index(t38, t35.len);
  t40 = v1;
  t41 = ((int64_t)(v6));
  t42 = ((int64_t)(v8));
  t43 = ((int64_t)((uint64_t)(t41) + (uint64_t)(t42)));
  t44 = ((uint8_t *)t40.data) + gg_index(t43, t40.len);
  (*t39) = ((uint8_t)((*t44)));
  }
label4:;
  t33++; goto label3;
label5:;
  t45 = v3;
  t46 = ((int64_t)(((*(v0))).f10));
  v3 = ((int64_t)((uint64_t)(t45) + (uint64_t)(t46)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
label7:;
  t47 = ((int64_t)(v3));
  t48 = ((int64_t)(v4));
  if (!(((t47) < (t48)))) goto label8;
  {
  t49 = &(((*(v0))).f7);
  t50 = v2;
  t51 = ((int64_t)(v3));
  t52 = &(((*(v0))).f4);
  t53 = v1;
  t54 = &(((*(v0))).f4);
  t55 = ((int64_t)(v3));
  t56 = gg_function_11(t54, t55, base + 0);
  t57 = gg_function_43(t52, t53, t56, base + 0);
  gg_function_35(t49, t50, t51, t57, base + 0);
  t58 = v3;
  t59 = ((int64_t)(((*(v0))).f10));
  v3 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t59)));
  }
  goto label7;
label8:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_65(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t base) {
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
  gg_type_20 t12;
  int64_t t13;
  int64_t t14;
  gg_type_20 t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  gg_type_12 t19;
  gg_slice t20;
  gg_slice t21;
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
  uint8_t t32;
  gg_type_12 t33;
  gg_slice t34;
  gg_slice t35;
  gg_slice t36;
  gg_slice t37;
  gg_type_12 v0;
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
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t12 = &(((*(v0))).f4);
  t13 = gg_function_9(t12, base + 0);
  t14 = ((int64_t)(t13));
  t15 = &(((*(v0))).f7);
  t16 = gg_function_9(t15, base + 0);
  t17 = ((int64_t)(t16));
  t18 = ((t14) != (t17));
  if (t18) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t19 = v0;
  t20 = v1;
  t21 = v2;
  t22 = gg_function_66(t19, t20, t21, base + 0);
  t23 = t22;
  if (t23) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t24 = ((int64_t)((((*(v0))).f7).f2));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) == (t25));
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)((((*(v0))).f4).f2));
  t29 = gg_function_3(t28, base + 0);
  t30 = ((int64_t)(t29));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t30) == (t31));
  }
  t32 = t27;
  if (t32) {
  {
  t33 = v0;
  t34 = v1;
  t35 = v2;
  t36 = v3;
  t37 = v4;
  t0 = gg_function_68(t33, t34, t35, t36, t37, base + 0);
  goto gg_done;
  }
  }
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_15(gg_type_20 p0, int64_t base) {
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
  gg_type_20 v0;
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
  t7 = gg_function_6(t3, t6, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(3));
  t9 = ((int64_t *)(((*(v0))).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = ((int64_t)((*t9)));
  t0 = gg_function_6(t7, t10, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_62(float p0, int64_t base) {
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
static float gg_function_63(float p0, float p1, int64_t base) {
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
static float gg_function_64(float p0, float p1, int64_t base) {
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
static float gg_function_56(float p0, int64_t base) {
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
static float gg_function_61(gg_type_12 p0, float p1, int64_t p2, int64_t p3, int64_t base) {
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
  gg_type_12 v0;
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
  t27 = gg_function_56(t26, base + 0);
  t28 = ((float)(t27));
  t29 = ((float)(((t23) + (t28))));
  t0 = ((float)(((t22) / (t29))));
  goto gg_done;
  }
  }
  t30 = ((float)(v1));
  t0 = gg_function_62(t30, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_59(float p0, int64_t base) {
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
  float v0;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t3 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t4 = ((float)(gg_double_bits(UINT64_C(4610343945738431496))));
  t5 = ((float)((-(t4))));
  t6 = ((float)(v0));
  t7 = ((float)(((t5) * (t6))));
  t8 = expf(t7);
  t9 = ((float)(t8));
  t10 = ((float)(((t3) + (t9))));
  t11 = ((float)(((t2) / (t10))));
  t0 = ((float)(((t1) * (t11))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_55(gg_type_12 p0, float p1, int64_t p2, int64_t p3, int64_t base) {
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
  float t31;
  gg_type_12 v0;
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
  t24 = ((float)(gg_double_bits(UINT64_C(4610343945738431496))));
  t25 = ((float)((-(t24))));
  t26 = ((float)(v1));
  t27 = ((float)(((t25) * (t26))));
  t28 = gg_function_56(t27, base + 0);
  t29 = ((float)(t28));
  t30 = ((float)(((t23) + (t29))));
  t0 = ((float)(((t22) / (t30))));
  goto gg_done;
  }
  }
  t31 = ((float)(v1));
  t0 = gg_function_59(t31, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_53(float p0, int64_t base) {
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
static float gg_function_51(float p0, int64_t base) {
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
static float gg_function_50(gg_type_12 p0, float p1, int64_t base) {
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
  gg_type_12 v0;
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
  t22 = gg_function_51(t21, base + 0);
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
  t30 = gg_function_51(t29, base + 0);
  t31 = fg143_to_half(t30);
  t32 = ((uint16_t)(t31));
  t33 = fg143_half(t32);
  t0 = ((float)(t33));
  goto gg_done;
  }
  }
  t34 = ((float)(v1));
  t0 = gg_function_51(t34, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_49(gg_type_20 p0, gg_slice p1, int64_t p2, int64_t base) {
  float t0;
  gg_type_20 t1;
  gg_slice t2;
  gg_type_20 t3;
  int64_t t4;
  int64_t t5;
  gg_type_20 v0;
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
  t5 = gg_function_11(t3, t4, base + 0);
  t0 = gg_function_43(t1, t2, t5, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_48(gg_type_20 p0, gg_type_20 p1, int64_t base) {
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
  gg_type_20 v0;
  gg_type_20 v1;
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
static uint8_t gg_function_47(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t * t5;
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
  int64_t * t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t * t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  gg_type_20 t30;
  int64_t t31;
  int64_t t32;
  gg_type_20 t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
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
  uint8_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  uint8_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  uint8_t t58;
  gg_type_20 t59;
  gg_type_20 t60;
  uint8_t t61;
  uint8_t t62;
  uint8_t t63;
  uint8_t t64;
  gg_type_20 t65;
  gg_type_20 t66;
  uint8_t t67;
  uint8_t t68;
  uint8_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t * t72;
  int64_t t73;
  int64_t t74;
  uint8_t t75;
  int64_t t76;
  int64_t * t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  uint8_t t82;
  gg_type_20 t83;
  int64_t t84;
  gg_type_20 t85;
  int64_t t86;
  uint8_t t87;
  gg_type_20 t88;
  int64_t t89;
  gg_type_12 t90;
  int64_t t91;
  gg_type_20 t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  gg_type_20 t97;
  gg_slice t98;
  int64_t t99;
  int64_t t100;
  int64_t * t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
  uint8_t t108;
  uint8_t t109;
  uint8_t t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  gg_type_20 t114;
  gg_slice t115;
  int64_t t116;
  int64_t t117;
  int64_t * t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  gg_type_20 t123;
  gg_slice t124;
  int64_t t125;
  int64_t t126;
  int64_t * t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  uint8_t t138;
  float t139;
  float t140;
  uint8_t t141;
  float t142;
  float t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
  gg_type_12 t147;
  float t148;
  float t149;
  float t150;
  float t151;
  int64_t t152;
  int64_t t153;
  uint8_t t154;
  float t155;
  float t156;
  float t157;
  float t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
  gg_type_12 t162;
  float t163;
  int64_t t164;
  int64_t t165;
  float t166;
  float t167;
  float t168;
  int64_t t169;
  int64_t t170;
  uint8_t t171;
  gg_type_12 t172;
  float t173;
  int64_t t174;
  int64_t t175;
  float t176;
  float t177;
  float t178;
  int64_t t179;
  int64_t t180;
  uint8_t t181;
  float t182;
  int64_t t183;
  float * t184;
  float t185;
  float t186;
  int64_t t187;
  float * t188;
  float t189;
  float t190;
  int64_t t191;
  float * t192;
  float t193;
  float t194;
  float t195;
  float t196;
  int64_t t197;
  float * t198;
  float t199;
  float t200;
  float t201;
  float t202;
  float t203;
  float t204;
  float t205;
  float t206;
  float t207;
  float t208;
  float t209;
  int64_t t210;
  int64_t t211;
  float t212;
  int64_t t213;
  float * t214;
  float t215;
  float t216;
  int64_t t217;
  float * t218;
  float t219;
  float t220;
  float t221;
  int64_t t222;
  float * t223;
  float t224;
  float t225;
  float t226;
  float t227;
  float t228;
  gg_type_20 t229;
  gg_slice t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  float t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  gg_type_20 t240;
  int64_t t241;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  uint8_t v7;
  uint8_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  float v12;
  float v13;
  float v14;
  float v15;
  float v16;
  float v17;
  float v18;
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
  v5 = ((int64_t)((((*(v0))).f4).f2));
  t6 = ((int64_t)(v5));
  v6 = gg_function_3(t6, base + 0);
  t7 = ((int64_t)(v4));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) <= (t8));
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v6));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t11) == (t12));
  }
  t13 = t10;
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)((((*(v0))).f7).f2));
  t16 = ((int64_t)(v5));
  t14 = ((t15) != (t16));
  }
  t17 = t14;
  t18 = t17;
  if (!t18) {
  /* FG_DIRECT_SLICE_INDEX */
  t19 = ((int64_t)UINT64_C(0));
  t20 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t19, ((int64_t)UINT64_C(4)));
  t21 = ((int64_t)((*t20)));
  t22 = ((int64_t)(v6));
  t18 = ((t21) != (t22));
  }
  t23 = t18;
  t24 = t23;
  if (!t24) {
  /* FG_DIRECT_SLICE_INDEX */
  t25 = ((int64_t)UINT64_C(0));
  t26 = ((int64_t *)((((*(v0))).f7).f1).data) + gg_index(t25, ((int64_t)UINT64_C(4)));
  t27 = ((int64_t)((*t26)));
  t28 = ((int64_t)(v6));
  t24 = ((t27) != (t28));
  }
  t29 = t24;
  if (t29) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t30 = &(((*(v0))).f4);
  t31 = gg_function_15(t30, base + 0);
  t32 = ((int64_t)(t31));
  t33 = &(((*(v0))).f7);
  t34 = gg_function_15(t33, base + 0);
  t35 = ((int64_t)(t34));
  t36 = ((t32) != (t35));
  if (t36) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t37 = ((int64_t)UINT64_C(2));
  t38 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t37, ((int64_t)UINT64_C(16)));
  t39 = ((int64_t)((*t38)));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = ((t39) != (t40));
  /* FG_DIRECT_SLICE_INDEX */
  t41 = ((int64_t)UINT64_C(1));
  t42 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t41, ((int64_t)UINT64_C(16)));
  t43 = ((int64_t)((*t42)));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  v8 = ((t43) != (t44));
  /* FG_DIRECT_SLICE_INDEX */
  t45 = ((int64_t)UINT64_C(0));
  t46 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t45, ((int64_t)UINT64_C(16)));
  v9 = ((int64_t)((*t46)));
  t47 = ((int64_t)(v9));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((t47) < (t48));
  t50 = t49;
  if (!t50) {
  t51 = ((int64_t)(v9));
  t52 = ((int64_t)(((int64_t)UINT64_C(7))));
  t50 = ((t51) >= (t52));
  }
  t53 = t50;
  if (t53) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t54 = v7;
  if (t54) {
  {
  t55 = ((int64_t)((((*(v0))).f5).f2));
  t56 = ((int64_t)(v5));
  t57 = ((t55) != (t56));
  t58 = t57;
  if (!t58) {
  t59 = &(((*(v0))).f4);
  t60 = &(((*(v0))).f5);
  t61 = gg_function_48(t59, t60, base + 0);
  t62 = t61;
  t58 = (!(t62));
  }
  t63 = t58;
  t64 = t63;
  if (!t64) {
  t65 = &(((*(v0))).f4);
  t66 = &(((*(v0))).f7);
  t67 = gg_function_48(t65, t66, base + 0);
  t68 = t67;
  t64 = (!(t68));
  }
  t69 = t64;
  t70 = t69;
  if (!t70) {
  /* FG_DIRECT_SLICE_INDEX */
  t71 = ((int64_t)UINT64_C(0));
  t72 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t71, ((int64_t)UINT64_C(4)));
  t73 = ((int64_t)((*t72)));
  t74 = ((int64_t)(v6));
  t70 = ((t73) != (t74));
  }
  t75 = t70;
  if (t75) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t76 = ((int64_t)UINT64_C(0));
  t77 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t76, ((int64_t)UINT64_C(4)));
  t78 = ((int64_t)((*t77)));
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((int64_t)(v4));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  t82 = ((t78) != (t81));
  if (t82) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  }
  }
  t83 = &(((*(v0))).f7);
  t84 = ((int64_t)((v3).len));
  gg_function_16(t83, t84, base + 0);
  t85 = &(((*(v0))).f4);
  t86 = ((int64_t)((v1).len));
  gg_function_16(t85, t86, base + 0);
  t87 = v7;
  if (t87) {
  {
  t88 = &(((*(v0))).f5);
  t89 = ((int64_t)((v2).len));
  gg_function_16(t88, t89, base + 0);
  }
  }
  t90 = v0;
  v10 = gg_function_17(t90, base + 0);
label1:;
  t91 = ((int64_t)(v10));
  t92 = &(((*(v0))).f7);
  t93 = gg_function_15(t92, base + 0);
  t94 = ((int64_t)(t93));
  if (!(((t91) < (t94)))) goto label2;
  {
  t96 = v4;
  t95 = 0;
label3:;
  if (t95 >= t96) goto label5;
  v11 = t95;
  {
  t97 = &(((*(v0))).f4);
  t98 = v1;
  t99 = ((int64_t)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t100 = ((int64_t)UINT64_C(0));
  t101 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t100, ((int64_t)UINT64_C(4)));
  t102 = ((int64_t)((*t101)));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t102)))));
  t104 = ((int64_t)(v11));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t103) + (uint64_t)(t104)))));
  t107 = v7;
  t108 = t107;
  if (!t108) {
  t109 = v8;
  t108 = (!(t109));
  }
  t110 = t108;
  if (t110) {
  {
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t106 = ((int64_t)(v4));
  }
  }
  t111 = ((int64_t)(t106));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t105) + (uint64_t)(t111)))));
  v12 = gg_function_49(t97, t98, t112, base + 0);
  t113 = v7;
  if (t113) {
  {
  t114 = &(((*(v0))).f5);
  t115 = v2;
  t116 = ((int64_t)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t117 = ((int64_t)UINT64_C(0));
  t118 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t117, ((int64_t)UINT64_C(4)));
  t119 = ((int64_t)((*t118)));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t116) * (uint64_t)(t119)))));
  t121 = ((int64_t)(v11));
  t122 = ((int64_t)(((int64_t)((uint64_t)(t120) + (uint64_t)(t121)))));
  v13 = gg_function_49(t114, t115, t122, base + 0);
  }
  } else {
  {
  t123 = &(((*(v0))).f4);
  t124 = v1;
  t125 = ((int64_t)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t126 = ((int64_t)UINT64_C(0));
  t127 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t126, ((int64_t)UINT64_C(4)));
  t128 = ((int64_t)((*t127)));
  t129 = ((int64_t)(((int64_t)((uint64_t)(t125) * (uint64_t)(t128)))));
  t130 = ((int64_t)(v11));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t129) + (uint64_t)(t130)))));
  t133 = v8;
  if (t133) {
  {
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t132 = ((int64_t)(v4));
  }
  }
  t134 = ((int64_t)(t132));
  t135 = ((int64_t)(((int64_t)((uint64_t)(t131) + (uint64_t)(t134)))));
  v13 = gg_function_49(t123, t124, t135, base + 0);
  }
  }
  v14 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t136 = ((int64_t)(v9));
  t137 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = ((t136) == (t137));
  if (t138) {
  {
  t139 = ((float)(v12));
  t140 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t141 = ((t139) > (t140));
  if (t141) {
  {
  t142 = ((float)(v12));
  t143 = ((float)(v13));
  v14 = ((float)(((t142) * (t143))));
  }
  } else {
  {
  v14 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  }
  }
  } else {
  t144 = ((int64_t)(v9));
  t145 = ((int64_t)(((int64_t)UINT64_C(1))));
  t146 = ((t144) == (t145));
  if (t146) {
  {
  t147 = v0;
  t148 = ((float)(v12));
  t149 = gg_function_50(t147, t148, base + 0);
  t150 = ((float)(t149));
  t151 = ((float)(v13));
  v14 = ((float)(((t150) * (t151))));
  }
  } else {
  t152 = ((int64_t)(v9));
  t153 = ((int64_t)(((int64_t)UINT64_C(4))));
  t154 = ((t152) == (t153));
  if (t154) {
  {
  t155 = ((float)(v12));
  t156 = gg_function_53(t155, base + 0);
  t157 = ((float)(t156));
  t158 = ((float)(v13));
  v14 = ((float)(((t157) * (t158))));
  }
  } else {
  t159 = ((int64_t)(v9));
  t160 = ((int64_t)(((int64_t)UINT64_C(5))));
  t161 = ((t159) == (t160));
  if (t161) {
  {
  t162 = v0;
  t163 = ((float)(v12));
  t164 = ((int64_t)(v11));
  t165 = ((int64_t)(v4));
  t166 = gg_function_55(t162, t163, t164, t165, base + 0);
  t167 = ((float)(t166));
  t168 = ((float)(v13));
  v14 = ((float)(((t167) * (t168))));
  }
  } else {
  t169 = ((int64_t)(v9));
  t170 = ((int64_t)(((int64_t)UINT64_C(2))));
  t171 = ((t169) == (t170));
  if (t171) {
  {
  t172 = v0;
  t173 = ((float)(v12));
  t174 = ((int64_t)(v11));
  t175 = ((int64_t)(v4));
  t176 = gg_function_61(t172, t173, t174, t175, base + 0);
  t177 = ((float)(t176));
  t178 = ((float)(v13));
  v14 = ((float)(((t177) * (t178))));
  }
  } else {
  t179 = ((int64_t)(v9));
  t180 = ((int64_t)(((int64_t)UINT64_C(3))));
  t181 = ((t179) == (t180));
  if (t181) {
  {
  t182 = ((float)(v12));
  /* FG_DIRECT_SLICE_INDEX */
  t183 = ((int64_t)UINT64_C(1));
  t184 = ((float *)(((*(v0))).f12).data) + gg_index(t183, ((int64_t)UINT64_C(8)));
  t185 = ((float)((*t184)));
  v15 = gg_function_63(t182, t185, base + 0);
  t186 = ((float)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t187 = ((int64_t)UINT64_C(1));
  t188 = ((float *)(((*(v0))).f12).data) + gg_index(t187, ((int64_t)UINT64_C(8)));
  t189 = ((float)((*t188)));
  t190 = gg_function_63(t186, t189, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t191 = ((int64_t)UINT64_C(1));
  t192 = ((float *)(((*(v0))).f12).data) + gg_index(t191, ((int64_t)UINT64_C(8)));
  t193 = (*t192);
  t194 = ((float)((-(t193))));
  v16 = gg_function_64(t190, t194, base + 0);
  t195 = ((float)(v15));
  t196 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  /* FG_DIRECT_SLICE_INDEX */
  t197 = ((int64_t)UINT64_C(0));
  t198 = ((float *)(((*(v0))).f12).data) + gg_index(t197, ((int64_t)UINT64_C(8)));
  t199 = (*t198);
  t200 = ((float)((-(t199))));
  t201 = ((float)(v15));
  t202 = ((float)(((t200) * (t201))));
  t203 = expf(t202);
  t204 = ((float)(t203));
  t205 = ((float)(((t196) + (t204))));
  t206 = ((float)(((t195) / (t205))));
  t207 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t208 = ((float)(v16));
  t209 = ((float)(((t207) + (t208))));
  v14 = ((float)(((t206) * (t209))));
  }
  } else {
  {
  t210 = ((int64_t)(v9));
  t211 = ((int64_t)(((int64_t)UINT64_C(6))));
  if (!(((t210) == (t211)))) gg_fail("assertion failed");
  t212 = ((float)(v12));
  /* FG_DIRECT_SLICE_INDEX */
  t213 = ((int64_t)UINT64_C(1));
  t214 = ((float *)(((*(v0))).f12).data) + gg_index(t213, ((int64_t)UINT64_C(8)));
  t215 = ((float)((*t214)));
  v17 = gg_function_63(t212, t215, base + 0);
  t216 = ((float)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t217 = ((int64_t)UINT64_C(1));
  t218 = ((float *)(((*(v0))).f12).data) + gg_index(t217, ((int64_t)UINT64_C(8)));
  t219 = (*t218);
  t220 = ((float)((-(t219))));
  t221 = gg_function_64(t216, t220, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t222 = ((int64_t)UINT64_C(1));
  t223 = ((float *)(((*(v0))).f12).data) + gg_index(t222, ((int64_t)UINT64_C(8)));
  t224 = ((float)((*t223)));
  v18 = gg_function_63(t221, t224, base + 0);
  t225 = ((float)(v17));
  t226 = gg_function_62(t225, base + 0);
  t227 = ((float)(t226));
  t228 = ((float)(v18));
  v14 = ((float)(((t227) * (t228))));
  }
  }
  }
  }
  }
  }
  }
  t229 = &(((*(v0))).f7);
  t230 = v3;
  t231 = ((int64_t)(v10));
  t232 = ((int64_t)(v4));
  t233 = ((int64_t)(((int64_t)((uint64_t)(t231) * (uint64_t)(t232)))));
  t234 = ((int64_t)(v11));
  t235 = ((int64_t)(((int64_t)((uint64_t)(t233) + (uint64_t)(t234)))));
  t236 = ((float)(v14));
  gg_function_35(t229, t230, t235, t236, base + 0);
  }
label4:;
  t95++; goto label3;
label5:;
  t237 = v10;
  t238 = ((int64_t)(((*(v0))).f10));
  v10 = ((int64_t)((uint64_t)(t237) + (uint64_t)(t238)));
  }
  goto label1;
label2:;
  t239 = ((int64_t)(((int64_t)UINT64_C(100))));
  t240 = &(((*(v0))).f7);
  t241 = gg_function_15(t240, base + 0);
  fg143_witness(t239, t241);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(gg_type_20 p0, gg_slice p1, int64_t p2, int64_t base) {
  int64_t t0;
  gg_type_20 t1;
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
  gg_type_20 v0;
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
  v3 = gg_function_11(t1, t2, base + 0);
  t3 = ((int64_t)(((*(v0))).f2));
  t4 = ((int64_t)(((int64_t)UINT64_C(26))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  t6 = v1;
  t7 = ((int64_t)(v3));
  t8 = gg_function_12(t6, t7, base + 0);
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
static uint8_t gg_function_42(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
  uint8_t t0;
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
  int64_t t15;
  int64_t * t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t * t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t * t28;
  int64_t t29;
  int64_t t30;
  int64_t * t31;
  int64_t t32;
  uint8_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t * t36;
  int64_t t37;
  int64_t t38;
  int64_t * t39;
  int64_t t40;
  uint8_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t * t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  uint8_t t48;
  gg_type_20 t49;
  int64_t t50;
  int64_t t51;
  gg_type_20 t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t * t57;
  int64_t t58;
  int64_t t59;
  uint8_t t60;
  gg_type_20 t61;
  int64_t t62;
  gg_type_20 t63;
  int64_t t64;
  gg_type_20 t65;
  int64_t t66;
  gg_type_12 t67;
  int64_t t68;
  gg_type_20 t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t * t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t * t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t * t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t * t86;
  int64_t t87;
  int64_t t88;
  int64_t * t89;
  int64_t t90;
  int64_t t91;
  gg_type_20 t92;
  gg_slice t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  int64_t * t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t * t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t * t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t * t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  gg_type_20 t121;
  gg_slice t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  gg_type_20 t128;
  gg_slice t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  float t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  gg_type_20 t139;
  int64_t t140;
  gg_type_12 v0;
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
  int64_t v12;
  float v13;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)((((*(v0))).f7).f2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  if (t3) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t4 = ((int64_t)((((*(v0))).f4).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)((((*(v0))).f4).f2));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((t8) != (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)((((*(v0))).f4).f2));
  t13 = ((int64_t)(((int64_t)UINT64_C(30))));
  t11 = ((t12) != (t13));
  }
  t14 = t11;
  if (t14) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t15 = ((int64_t)UINT64_C(0));
  t16 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t15, ((int64_t)UINT64_C(4)));
  v4 = ((int64_t)((*t16)));
  t17 = ((int64_t)((((*(v0))).f4).f2));
  v5 = gg_function_3(t17, base + 0);
  t18 = ((int64_t)(v4));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t18) <= (t19));
  t21 = t20;
  if (!t21) {
  /* FG_DIRECT_SLICE_INDEX */
  t22 = ((int64_t)UINT64_C(0));
  t23 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t22, ((int64_t)UINT64_C(4)));
  t24 = ((int64_t)((*t23)));
  t25 = ((int64_t)(v4));
  t21 = ((t24) != (t25));
  }
  t26 = t21;
  if (t26) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t27 = ((int64_t)UINT64_C(2));
  t28 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t27, ((int64_t)UINT64_C(4)));
  t29 = ((int64_t)((*t28)));
  /* FG_DIRECT_SLICE_INDEX */
  t30 = ((int64_t)UINT64_C(1));
  t31 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t30, ((int64_t)UINT64_C(4)));
  t32 = ((int64_t)((*t31)));
  t33 = ((t29) != (t32));
  t34 = t33;
  if (!t34) {
  /* FG_DIRECT_SLICE_INDEX */
  t35 = ((int64_t)UINT64_C(3));
  t36 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t35, ((int64_t)UINT64_C(4)));
  t37 = ((int64_t)((*t36)));
  /* FG_DIRECT_SLICE_INDEX */
  t38 = ((int64_t)UINT64_C(2));
  t39 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t38, ((int64_t)UINT64_C(4)));
  t40 = ((int64_t)((*t39)));
  t34 = ((t37) != (t40));
  }
  t41 = t34;
  t42 = t41;
  if (!t42) {
  /* FG_DIRECT_SLICE_INDEX */
  t43 = ((int64_t)UINT64_C(3));
  t44 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t43, ((int64_t)UINT64_C(4)));
  t45 = ((int64_t)((*t44)));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t42 = ((t45) != (t46));
  }
  t47 = t42;
  t48 = t47;
  if (!t48) {
  t49 = &(((*(v0))).f5);
  t50 = gg_function_9(t49, base + 0);
  t51 = ((int64_t)(t50));
  t52 = &(((*(v0))).f7);
  t53 = gg_function_15(t52, base + 0);
  t54 = ((int64_t)(t53));
  t48 = ((t51) != (t54));
  }
  t55 = t48;
  if (t55) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t56 = ((int64_t)UINT64_C(0));
  t57 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t56, ((int64_t)UINT64_C(4)));
  t58 = ((int64_t)((*t57)));
  t59 = ((int64_t)(v5));
  t60 = ((t58) != (t59));
  if (t60) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t61 = &(((*(v0))).f7);
  t62 = ((int64_t)((v3).len));
  gg_function_16(t61, t62, base + 0);
  t63 = &(((*(v0))).f4);
  t64 = ((int64_t)((v1).len));
  gg_function_16(t63, t64, base + 0);
  t65 = &(((*(v0))).f5);
  t66 = ((int64_t)((v2).len));
  gg_function_16(t65, t66, base + 0);
  t67 = v0;
  v6 = gg_function_17(t67, base + 0);
label1:;
  t68 = ((int64_t)(v6));
  t69 = &(((*(v0))).f7);
  t70 = gg_function_15(t69, base + 0);
  t71 = ((int64_t)(t70));
  if (!(((t68) < (t71)))) goto label2;
  {
  t72 = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t73 = ((int64_t)UINT64_C(0));
  t74 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t73, ((int64_t)UINT64_C(4)));
  t75 = ((int64_t)((*t74)));
  v7 = ((int64_t)(((int64_t)gg_mod(t72, t75))));
  t76 = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t77 = ((int64_t)UINT64_C(0));
  t78 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t77, ((int64_t)UINT64_C(4)));
  t79 = ((int64_t)((*t78)));
  t80 = ((int64_t)(((int64_t)gg_div(t76, t79))));
  /* FG_DIRECT_SLICE_INDEX */
  t81 = ((int64_t)UINT64_C(1));
  t82 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t81, ((int64_t)UINT64_C(4)));
  t83 = ((int64_t)((*t82)));
  v8 = ((int64_t)(((int64_t)gg_mod(t80, t83))));
  t84 = ((int64_t)(v6));
  /* FG_DIRECT_SLICE_INDEX */
  t85 = ((int64_t)UINT64_C(0));
  t86 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t85, ((int64_t)UINT64_C(4)));
  t87 = ((int64_t)((*t86)));
  /* FG_DIRECT_SLICE_INDEX */
  t88 = ((int64_t)UINT64_C(1));
  t89 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t88, ((int64_t)UINT64_C(4)));
  t90 = ((int64_t)((*t89)));
  t91 = ((int64_t)(((int64_t)((uint64_t)(t87) * (uint64_t)(t90)))));
  v9 = ((int64_t)(((int64_t)gg_div(t84, t91))));
  t92 = &(((*(v0))).f5);
  t93 = v2;
  t94 = ((int64_t)(v6));
  v10 = gg_function_10(t92, t93, t94, base + 0);
  t95 = ((int64_t)(v10));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = ((t95) >= (t96));
  t98 = t97;
  if (t98) {
  t99 = ((int64_t)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t100 = ((int64_t)UINT64_C(1));
  t101 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t100, ((int64_t)UINT64_C(4)));
  t102 = ((int64_t)((*t101)));
  t98 = ((t99) < (t102));
  }
  if (!(t98)) gg_fail("assertion failed");
  t103 = ((int64_t)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t104 = ((int64_t)UINT64_C(1));
  t105 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t104, ((int64_t)UINT64_C(4)));
  t106 = ((int64_t)((*t105)));
  t107 = ((int64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t106)))));
  t108 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t109 = ((int64_t)UINT64_C(2));
  t110 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t109, ((int64_t)UINT64_C(4)));
  t111 = ((int64_t)((*t110)));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t111)))));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t107) + (uint64_t)(t112)))));
  t114 = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t115 = ((int64_t)UINT64_C(3));
  t116 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t115, ((int64_t)UINT64_C(4)));
  t117 = ((int64_t)((*t116)));
  t118 = ((int64_t)(((int64_t)((uint64_t)(t114) * (uint64_t)(t117)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t113) + (uint64_t)(t118)))));
  t120 = v4;
  t119 = 0;
label3:;
  if (t119 >= t120) goto label5;
  v12 = t119;
  {
  t121 = &(((*(v0))).f4);
  t122 = v1;
  t123 = ((int64_t)(v11));
  t124 = ((int64_t)(v12));
  t125 = ((int64_t)(v5));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t124) * (uint64_t)(t125)))));
  t127 = ((int64_t)(((int64_t)((uint64_t)(t123) + (uint64_t)(t126)))));
  v13 = gg_function_43(t121, t122, t127, base + 0);
  t128 = &(((*(v0))).f7);
  t129 = v3;
  t130 = ((int64_t)(v6));
  t131 = ((int64_t)(v4));
  t132 = ((int64_t)(((int64_t)((uint64_t)(t130) * (uint64_t)(t131)))));
  t133 = ((int64_t)(v12));
  t134 = ((int64_t)(((int64_t)((uint64_t)(t132) + (uint64_t)(t133)))));
  t135 = ((float)(v13));
  gg_function_35(t128, t129, t134, t135, base + 0);
  }
label4:;
  t119++; goto label3;
label5:;
  t136 = v6;
  t137 = ((int64_t)(((*(v0))).f10));
  v6 = ((int64_t)((uint64_t)(t136) + (uint64_t)(t137)));
  }
  goto label1;
label2:;
  t138 = ((int64_t)(((int64_t)UINT64_C(40))));
  t139 = &(((*(v0))).f7);
  t140 = gg_function_15(t139, base + 0);
  fg143_witness(t138, t140);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_22(gg_type_30 p0, int64_t p1, int32_t p2, int64_t base) {
  /* FG_SHARED_BODY:31 */
  gg_function_31(p0, p1, p2, base);
}
static uint16_t gg_function_24(gg_slice p0, int64_t p1, int64_t base) {
  /* FG_SHARED_BODY:33 */
  return gg_function_33(p0, p1, base);
}
static gg_type_25 gg_function_26(gg_slice p0, int64_t p1, int64_t base) {
  /* FG_SHARED_BODY:34 */
  return gg_function_34(p0, p1, base);
}
static void gg_function_23(gg_type_30 p0, int64_t p1, int32_t p2, int64_t base) {
  /* FG_SHARED_BODY:32 */
  gg_function_32(p0, p1, p2, base);
}
static void gg_function_21(int64_t p0, gg_slice p1, int64_t p2, gg_type_30 p3, int64_t base) {
  /* FG_SHARED_BODY:30 */
  gg_function_30(p0, p1, p2, p3, base);
}
static uint8_t gg_function_20(int64_t p0, gg_slice p1, gg_slice p2, int64_t base) {
  /* FG_SHARED_BODY:29 */
  return gg_function_29(p0, p1, p2, base);
}
static uint8_t gg_function_19(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t * t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t * t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t * t30;
  int64_t t31;
  int64_t t32;
  int64_t * t33;
  int64_t t34;
  uint8_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t * t38;
  int64_t t39;
  int64_t t40;
  int64_t * t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t * t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  uint8_t t50;
  gg_type_20 t51;
  int64_t t52;
  int64_t t53;
  gg_type_20 t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  uint8_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  uint8_t t74;
  uint8_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  gg_type_20 t82;
  int64_t t83;
  gg_type_20 t84;
  int64_t t85;
  gg_type_12 t86;
  int64_t t87;
  gg_type_20 t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t * t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t * t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t * t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t * t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  int64_t t110;
  gg_type_20 t111;
  gg_slice t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint8_t t116;
  uint8_t t117;
  int64_t t118;
  int64_t t119;
  int64_t * t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t * t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t * t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t * t135;
  int64_t t136;
  int64_t t137;
  gg_slice t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t * t148;
  int64_t t149;
  int64_t t150;
  uint8_t t151;
  int64_t t152;
  int64_t t153;
  uint8_t t154;
  uint8_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  gg_slice t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  gg_slice t166;
  gg_slice t167;
  uint8_t t168;
  gg_slice t169;
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
  int64_t * t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  uint8_t t188;
  uint8_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
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
  gg_slice t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  uint8_t * t207;
  int64_t t208;
  gg_slice t209;
  gg_slice t210;
  uint8_t t211;
  int64_t t212;
  int64_t t213;
  gg_type_20 t214;
  gg_slice t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  float * t222;
  float t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  gg_type_20 t227;
  int64_t t228;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  int64_t v6;
  gg_type_19 v7;
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
  gg_slice v18;
  gg_slice v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)((((*(v0))).f7).f2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  if (t3) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t4 = ((int64_t)((((*(v0))).f4).f2));
  v6 = gg_function_4(t4, base + 0);
  t5 = ((int64_t)(v6));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) < (t6));
  if (t7) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v6));
  v7 = gg_function_5(t8, base + 0);
  t9 = ((int64_t)((v7).f13));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) <= (t10));
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  v8 = ((int64_t)(((int64_t)UINT64_C(256))));
  /* FG_DIRECT_SLICE_INDEX */
  t12 = ((int64_t)UINT64_C(0));
  t13 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t12, ((int64_t)UINT64_C(4)));
  v9 = ((int64_t)((*t13)));
  t14 = ((int64_t)(v9));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) <= (t15));
  t17 = t16;
  if (!t17) {
  t18 = ((int64_t)(v9));
  t19 = ((int64_t)(v8));
  t20 = ((int64_t)(((int64_t)gg_mod(t18, t19))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t20) != (t21));
  }
  t22 = t17;
  t23 = t22;
  if (!t23) {
  /* FG_DIRECT_SLICE_INDEX */
  t24 = ((int64_t)UINT64_C(0));
  t25 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t24, ((int64_t)UINT64_C(4)));
  t26 = ((int64_t)((*t25)));
  t27 = ((int64_t)(v9));
  t23 = ((t26) != (t27));
  }
  t28 = t23;
  if (t28) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t29 = ((int64_t)UINT64_C(2));
  t30 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t29, ((int64_t)UINT64_C(4)));
  t31 = ((int64_t)((*t30)));
  /* FG_DIRECT_SLICE_INDEX */
  t32 = ((int64_t)UINT64_C(1));
  t33 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t32, ((int64_t)UINT64_C(4)));
  t34 = ((int64_t)((*t33)));
  t35 = ((t31) != (t34));
  t36 = t35;
  if (!t36) {
  /* FG_DIRECT_SLICE_INDEX */
  t37 = ((int64_t)UINT64_C(3));
  t38 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t37, ((int64_t)UINT64_C(4)));
  t39 = ((int64_t)((*t38)));
  /* FG_DIRECT_SLICE_INDEX */
  t40 = ((int64_t)UINT64_C(2));
  t41 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t40, ((int64_t)UINT64_C(4)));
  t42 = ((int64_t)((*t41)));
  t36 = ((t39) != (t42));
  }
  t43 = t36;
  t44 = t43;
  if (!t44) {
  /* FG_DIRECT_SLICE_INDEX */
  t45 = ((int64_t)UINT64_C(3));
  t46 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t45, ((int64_t)UINT64_C(4)));
  t47 = ((int64_t)((*t46)));
  t48 = ((int64_t)(((int64_t)UINT64_C(1))));
  t44 = ((t47) != (t48));
  }
  t49 = t44;
  t50 = t49;
  if (!t50) {
  t51 = &(((*(v0))).f5);
  t52 = gg_function_9(t51, base + 0);
  t53 = ((int64_t)(t52));
  t54 = &(((*(v0))).f7);
  t55 = gg_function_15(t54, base + 0);
  t56 = ((int64_t)(t55));
  t50 = ((t53) != (t56));
  }
  t57 = t50;
  if (t57) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t58 = ((int64_t)(v9));
  t59 = ((int64_t)(v8));
  v10 = ((int64_t)(((int64_t)gg_div(t58, t59))));
  t60 = ((int64_t)(v10));
  t61 = ((int64_t)((v7).f13));
  v11 = gg_function_6(t60, t61, base + 0);
  t62 = ((int64_t)((((*(v0))).f4).f3));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((t62) <= (t63));
  t65 = t64;
  if (!t65) {
  t66 = ((int64_t)((((*(v0))).f4).f3));
  t67 = ((int64_t)((v1).len));
  t65 = ((t66) > (t67));
  }
  t68 = t65;
  if (t68) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t69 = ((int64_t)((v4).len));
  t70 = ((int64_t)(v9));
  t71 = ((int64_t)(((*(v0))).f10));
  t72 = gg_function_6(t70, t71, base + 0);
  t73 = ((int64_t)(t72));
  t74 = ((t69) < (t73));
  t75 = t74;
  if (!t75) {
  t76 = ((int64_t)((v5).len));
  t77 = ((int64_t)(v11));
  t78 = ((int64_t)(((*(v0))).f10));
  t79 = gg_function_6(t77, t78, base + 0);
  t80 = ((int64_t)(t79));
  t75 = ((t76) < (t80));
  }
  t81 = t75;
  if (t81) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t82 = &(((*(v0))).f7);
  t83 = ((int64_t)((v3).len));
  gg_function_16(t82, t83, base + 0);
  t84 = &(((*(v0))).f5);
  t85 = ((int64_t)((v2).len));
  gg_function_16(t84, t85, base + 0);
  t86 = v0;
  v12 = gg_function_17(t86, base + 0);
label1:;
  t87 = ((int64_t)(v12));
  t88 = &(((*(v0))).f7);
  t89 = gg_function_15(t88, base + 0);
  t90 = ((int64_t)(t89));
  if (!(((t87) < (t90)))) goto label2;
  {
  t91 = ((int64_t)(v12));
  /* FG_DIRECT_SLICE_INDEX */
  t92 = ((int64_t)UINT64_C(0));
  t93 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t92, ((int64_t)UINT64_C(4)));
  t94 = ((int64_t)((*t93)));
  v13 = ((int64_t)(((int64_t)gg_mod(t91, t94))));
  t95 = ((int64_t)(v12));
  /* FG_DIRECT_SLICE_INDEX */
  t96 = ((int64_t)UINT64_C(0));
  t97 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t96, ((int64_t)UINT64_C(4)));
  t98 = ((int64_t)((*t97)));
  t99 = ((int64_t)(((int64_t)gg_div(t95, t98))));
  /* FG_DIRECT_SLICE_INDEX */
  t100 = ((int64_t)UINT64_C(1));
  t101 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t100, ((int64_t)UINT64_C(4)));
  t102 = ((int64_t)((*t101)));
  v14 = ((int64_t)(((int64_t)gg_mod(t99, t102))));
  t103 = ((int64_t)(v12));
  /* FG_DIRECT_SLICE_INDEX */
  t104 = ((int64_t)UINT64_C(0));
  t105 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t104, ((int64_t)UINT64_C(4)));
  t106 = ((int64_t)((*t105)));
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(1));
  t108 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = ((int64_t)((*t108)));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t106) * (uint64_t)(t109)))));
  v15 = ((int64_t)(((int64_t)gg_div(t103, t110))));
  t111 = &(((*(v0))).f5);
  t112 = v2;
  t113 = ((int64_t)(v12));
  v16 = gg_function_10(t111, t112, t113, base + 0);
  t114 = ((int64_t)(v16));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((t114) >= (t115));
  t117 = t116;
  if (t117) {
  t118 = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t119 = ((int64_t)UINT64_C(1));
  t120 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t119, ((int64_t)UINT64_C(4)));
  t121 = ((int64_t)((*t120)));
  t117 = ((t118) < (t121));
  }
  if (!(t117)) gg_fail("assertion failed");
  t122 = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t123 = ((int64_t)UINT64_C(1));
  t124 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t123, ((int64_t)UINT64_C(4)));
  t125 = ((int64_t)((*t124)));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t122) * (uint64_t)(t125)))));
  t127 = ((int64_t)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t128 = ((int64_t)UINT64_C(2));
  t129 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t128, ((int64_t)UINT64_C(4)));
  t130 = ((int64_t)((*t129)));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t127) * (uint64_t)(t130)))));
  t132 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t131)))));
  t133 = ((int64_t)(v15));
  /* FG_DIRECT_SLICE_INDEX */
  t134 = ((int64_t)UINT64_C(3));
  t135 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t134, ((int64_t)UINT64_C(4)));
  t136 = ((int64_t)((*t135)));
  t137 = ((int64_t)(((int64_t)((uint64_t)(t133) * (uint64_t)(t136)))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t132) + (uint64_t)(t137)))));
  t138 = v4;
  t139 = ((int64_t)(((*(v0))).f9));
  t140 = ((int64_t)(v9));
  t141 = ((int64_t)((uint64_t)(t139) * (uint64_t)(t140)));
  t142 = ((int64_t)(((*(v0))).f9));
  t143 = ((int64_t)(((int64_t)UINT64_C(1))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t142) + (uint64_t)(t143)))));
  t145 = ((int64_t)(v9));
  t146 = ((int64_t)((uint64_t)(t144) * (uint64_t)(t145)));
  v18 = gg_sub(t138, t141, t146, sizeof(float));
  /* FG_DIRECT_SLICE_INDEX */
  t147 = ((int64_t)UINT64_C(0));
  t148 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t147, ((int64_t)UINT64_C(4)));
  t149 = ((int64_t)((*t148)));
  t150 = ((int64_t)((v7).f13));
  t151 = ((t149) == (t150));
  if (t151) {
  {
  t152 = ((int64_t)(v17));
  t153 = ((int64_t)(((int64_t)UINT64_C(0))));
  t154 = ((t152) >= (t153));
  t155 = t154;
  if (t155) {
  t156 = ((int64_t)(v17));
  t157 = ((int64_t)(v11));
  t158 = ((int64_t)(((int64_t)((uint64_t)(t156) + (uint64_t)(t157)))));
  t159 = ((int64_t)((v1).len));
  t155 = ((t158) <= (t159));
  }
  if (!(t155)) gg_fail("assertion failed");
  t160 = ((int64_t)((((*(v0))).f4).f2));
  t161 = v1;
  t162 = v17;
  t163 = ((int64_t)(v17));
  t164 = ((int64_t)(v11));
  t165 = ((int64_t)((uint64_t)(t163) + (uint64_t)(t164)));
  t166 = gg_sub(t161, t162, t165, sizeof(uint8_t));
  t167 = v18;
  t168 = gg_function_29(t160, t166, t167, base + 0);
  if (!(t168)) gg_fail("assertion failed");
  }
  } else {
  {
  t169 = v5;
  t170 = ((int64_t)(((*(v0))).f9));
  t171 = ((int64_t)(v11));
  t172 = ((int64_t)((uint64_t)(t170) * (uint64_t)(t171)));
  t173 = ((int64_t)(((*(v0))).f9));
  t174 = ((int64_t)(((int64_t)UINT64_C(1))));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) + (uint64_t)(t174)))));
  t176 = ((int64_t)(v11));
  t177 = ((int64_t)((uint64_t)(t175) * (uint64_t)(t176)));
  v19 = gg_sub(t169, t172, t177, sizeof(uint8_t));
  t179 = v10;
  t178 = 0;
label3:;
  if (t178 >= t179) goto label5;
  v20 = t178;
  {
  t180 = ((int64_t)(v17));
  t181 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t182 = ((int64_t)UINT64_C(0));
  t183 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t182, ((int64_t)UINT64_C(4)));
  t184 = ((int64_t)((*t183)));
  t185 = ((int64_t)(((int64_t)((uint64_t)(t181) * (uint64_t)(t184)))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t180) + (uint64_t)(t185)))));
  t186 = ((int64_t)(v21));
  t187 = ((int64_t)(((int64_t)UINT64_C(0))));
  t188 = ((t186) >= (t187));
  t189 = t188;
  if (t189) {
  t190 = ((int64_t)(v21));
  t191 = ((int64_t)((v7).f13));
  t192 = ((int64_t)(((int64_t)((uint64_t)(t190) + (uint64_t)(t191)))));
  t193 = ((int64_t)((v1).len));
  t189 = ((t192) <= (t193));
  }
  if (!(t189)) gg_fail("assertion failed");
  t195 = (v7).f13;
  t194 = 0;
label6:;
  if (t194 >= t195) goto label8;
  v22 = t194;
  {
  t196 = v19;
  t197 = ((int64_t)(v20));
  t198 = ((int64_t)((v7).f13));
  t199 = ((int64_t)(((int64_t)((uint64_t)(t197) * (uint64_t)(t198)))));
  t200 = ((int64_t)(v22));
  t201 = ((int64_t)((uint64_t)(t199) + (uint64_t)(t200)));
  t202 = ((uint8_t *)t196.data) + gg_index(t201, t196.len);
  t203 = v1;
  t204 = ((int64_t)(v21));
  t205 = ((int64_t)(v22));
  t206 = ((int64_t)((uint64_t)(t204) + (uint64_t)(t205)));
  t207 = ((uint8_t *)t203.data) + gg_index(t206, t203.len);
  (*t202) = ((uint8_t)((*t207)));
  }
label7:;
  t194++; goto label6;
label8:;
  }
label4:;
  t178++; goto label3;
label5:;
  t208 = ((int64_t)((((*(v0))).f4).f2));
  t209 = v19;
  t210 = v18;
  t211 = gg_function_29(t208, t209, t210, base + 0);
  if (!(t211)) gg_fail("assertion failed");
  }
  }
  t213 = v9;
  t212 = 0;
label9:;
  if (t212 >= t213) goto label11;
  v23 = t212;
  {
  t214 = &(((*(v0))).f7);
  t215 = v3;
  t216 = ((int64_t)(v12));
  t217 = ((int64_t)(v9));
  t218 = ((int64_t)(((int64_t)((uint64_t)(t216) * (uint64_t)(t217)))));
  t219 = ((int64_t)(v23));
  t220 = ((int64_t)(((int64_t)((uint64_t)(t218) + (uint64_t)(t219)))));
  /* FG_DIRECT_SLICE_INDEX */
  t221 = v23;
  t222 = ((float *)(v18).data) + gg_index(t221, (v18).len);
  t223 = ((float)((*t222)));
  gg_function_35(t214, t215, t220, t223, base + 0);
  }
label10:;
  t212++; goto label9;
label11:;
  t224 = v12;
  t225 = ((int64_t)(((*(v0))).f10));
  v12 = ((int64_t)((uint64_t)(t224) + (uint64_t)(t225)));
  }
  goto label1;
label2:;
  t226 = ((int64_t)(((int64_t)UINT64_C(40))));
  t227 = &(((*(v0))).f7);
  t228 = gg_function_15(t227, base + 0);
  fg143_witness(t226, t228);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_14(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t base) {
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
  uint8_t t18;
  int64_t t19;
  int64_t * t20;
  int64_t t21;
  int64_t t22;
  int64_t * t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t * t28;
  int64_t t29;
  int64_t t30;
  int64_t * t31;
  int64_t t32;
  uint8_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t * t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
  gg_type_20 t41;
  int64_t t42;
  int64_t t43;
  gg_type_20 t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t * t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t * t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  gg_type_20 t59;
  int64_t t60;
  gg_type_20 t61;
  int64_t t62;
  gg_type_20 t63;
  int64_t t64;
  gg_type_12 t65;
  int64_t t66;
  gg_type_20 t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t * t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t * t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t * t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  int64_t t86;
  int64_t * t87;
  int64_t t88;
  int64_t t89;
  gg_type_20 t90;
  gg_slice t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint8_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  int64_t * t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t * t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t * t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  gg_type_20 t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  uint8_t t131;
  uint8_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  uint8_t t137;
  uint8_t t138;
  int64_t t139;
  int64_t t140;
  uint8_t t141;
  uint8_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  gg_slice t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  uint8_t * t153;
  gg_slice t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  uint8_t * t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  gg_type_20 t162;
  int64_t t163;
  gg_type_12 v0;
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
  int64_t v12;
  int64_t v13;
  int64_t v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)((((*(v0))).f7).f2));
  t2 = ((int64_t)(((int64_t)UINT64_C(26))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)((((*(v0))).f4).f2));
  t6 = ((int64_t)(((int64_t)UINT64_C(26))));
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
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t10) <= (t11));
  t13 = t12;
  if (!t13) {
  /* FG_DIRECT_SLICE_INDEX */
  t14 = ((int64_t)UINT64_C(0));
  t15 = ((int64_t *)((((*(v0))).f7).f0).data) + gg_index(t14, ((int64_t)UINT64_C(4)));
  t16 = ((int64_t)((*t15)));
  t17 = ((int64_t)(v4));
  t13 = ((t16) != (t17));
  }
  t18 = t13;
  if (t18) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t19 = ((int64_t)UINT64_C(2));
  t20 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t19, ((int64_t)UINT64_C(4)));
  t21 = ((int64_t)((*t20)));
  /* FG_DIRECT_SLICE_INDEX */
  t22 = ((int64_t)UINT64_C(1));
  t23 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t22, ((int64_t)UINT64_C(4)));
  t24 = ((int64_t)((*t23)));
  t25 = ((t21) != (t24));
  t26 = t25;
  if (!t26) {
  /* FG_DIRECT_SLICE_INDEX */
  t27 = ((int64_t)UINT64_C(3));
  t28 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t27, ((int64_t)UINT64_C(4)));
  t29 = ((int64_t)((*t28)));
  /* FG_DIRECT_SLICE_INDEX */
  t30 = ((int64_t)UINT64_C(2));
  t31 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t30, ((int64_t)UINT64_C(4)));
  t32 = ((int64_t)((*t31)));
  t26 = ((t29) != (t32));
  }
  t33 = t26;
  t34 = t33;
  if (!t34) {
  /* FG_DIRECT_SLICE_INDEX */
  t35 = ((int64_t)UINT64_C(3));
  t36 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t35, ((int64_t)UINT64_C(4)));
  t37 = ((int64_t)((*t36)));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = ((t37) != (t38));
  }
  t39 = t34;
  t40 = t39;
  if (!t40) {
  t41 = &(((*(v0))).f5);
  t42 = gg_function_9(t41, base + 0);
  t43 = ((int64_t)(t42));
  t44 = &(((*(v0))).f7);
  t45 = gg_function_15(t44, base + 0);
  t46 = ((int64_t)(t45));
  t40 = ((t43) != (t46));
  }
  t47 = t40;
  if (t47) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t48 = ((int64_t)UINT64_C(0));
  t49 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t48, ((int64_t)UINT64_C(4)));
  t50 = ((int64_t)((*t49)));
  t51 = ((int64_t)(((int64_t)UINT64_C(4))));
  t52 = ((t50) != (t51));
  t53 = t52;
  if (!t53) {
  /* FG_DIRECT_SLICE_INDEX */
  t54 = ((int64_t)UINT64_C(0));
  t55 = ((int64_t *)((((*(v0))).f7).f1).data) + gg_index(t54, ((int64_t)UINT64_C(4)));
  t56 = ((int64_t)((*t55)));
  t57 = ((int64_t)(((int64_t)UINT64_C(4))));
  t53 = ((t56) != (t57));
  }
  t58 = t53;
  if (t58) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t59 = &(((*(v0))).f7);
  t60 = ((int64_t)((v3).len));
  gg_function_16(t59, t60, base + 0);
  t61 = &(((*(v0))).f4);
  t62 = ((int64_t)((v1).len));
  gg_function_16(t61, t62, base + 0);
  t63 = &(((*(v0))).f5);
  t64 = ((int64_t)((v2).len));
  gg_function_16(t63, t64, base + 0);
  t65 = v0;
  v5 = gg_function_17(t65, base + 0);
label1:;
  t66 = ((int64_t)(v5));
  t67 = &(((*(v0))).f7);
  t68 = gg_function_15(t67, base + 0);
  t69 = ((int64_t)(t68));
  if (!(((t66) < (t69)))) goto label2;
  {
  t70 = ((int64_t)(v5));
  /* FG_DIRECT_SLICE_INDEX */
  t71 = ((int64_t)UINT64_C(0));
  t72 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t71, ((int64_t)UINT64_C(4)));
  t73 = ((int64_t)((*t72)));
  v6 = ((int64_t)(((int64_t)gg_mod(t70, t73))));
  t74 = ((int64_t)(v5));
  /* FG_DIRECT_SLICE_INDEX */
  t75 = ((int64_t)UINT64_C(0));
  t76 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t75, ((int64_t)UINT64_C(4)));
  t77 = ((int64_t)((*t76)));
  t78 = ((int64_t)(((int64_t)gg_div(t74, t77))));
  /* FG_DIRECT_SLICE_INDEX */
  t79 = ((int64_t)UINT64_C(1));
  t80 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t79, ((int64_t)UINT64_C(4)));
  t81 = ((int64_t)((*t80)));
  v7 = ((int64_t)(((int64_t)gg_mod(t78, t81))));
  t82 = ((int64_t)(v5));
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = ((int64_t)((*t84)));
  /* FG_DIRECT_SLICE_INDEX */
  t86 = ((int64_t)UINT64_C(1));
  t87 = ((int64_t *)((((*(v0))).f5).f0).data) + gg_index(t86, ((int64_t)UINT64_C(4)));
  t88 = ((int64_t)((*t87)));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t88)))));
  v8 = ((int64_t)(((int64_t)gg_div(t82, t89))));
  t90 = &(((*(v0))).f5);
  t91 = v2;
  t92 = ((int64_t)(v5));
  v9 = gg_function_10(t90, t91, t92, base + 0);
  t93 = ((int64_t)(v9));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((t93) >= (t94));
  t96 = t95;
  if (t96) {
  t97 = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t98 = ((int64_t)UINT64_C(1));
  t99 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t98, ((int64_t)UINT64_C(4)));
  t100 = ((int64_t)((*t99)));
  t96 = ((t97) < (t100));
  }
  if (!(t96)) gg_fail("assertion failed");
  t101 = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t102 = ((int64_t)UINT64_C(1));
  t103 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t102, ((int64_t)UINT64_C(4)));
  t104 = ((int64_t)((*t103)));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t101) * (uint64_t)(t104)))));
  t106 = ((int64_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(2));
  t108 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = ((int64_t)((*t108)));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t106) * (uint64_t)(t109)))));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t105) + (uint64_t)(t110)))));
  t112 = ((int64_t)(v8));
  /* FG_DIRECT_SLICE_INDEX */
  t113 = ((int64_t)UINT64_C(3));
  t114 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t113, ((int64_t)UINT64_C(4)));
  t115 = ((int64_t)((*t114)));
  t116 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t115)))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t111) + (uint64_t)(t116)))));
  t118 = v4;
  t117 = 0;
label3:;
  if (t117 >= t118) goto label5;
  v11 = t117;
  {
  t119 = ((int64_t)(v10));
  t120 = ((int64_t)(v11));
  t121 = ((int64_t)(((int64_t)UINT64_C(4))));
  t122 = ((int64_t)(((int64_t)((uint64_t)(t120) * (uint64_t)(t121)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t119) + (uint64_t)(t122)))));
  t123 = &(((*(v0))).f7);
  t124 = ((int64_t)(v5));
  t125 = ((int64_t)(v4));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t124) * (uint64_t)(t125)))));
  t127 = ((int64_t)(v11));
  t128 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t127)))));
  v13 = gg_function_11(t123, t128, base + 0);
  t129 = ((int64_t)(v12));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = ((t129) >= (t130));
  t132 = t131;
  if (t132) {
  t133 = ((int64_t)(v12));
  t134 = ((int64_t)(((int64_t)UINT64_C(4))));
  t135 = ((int64_t)(((int64_t)((uint64_t)(t133) + (uint64_t)(t134)))));
  t136 = ((int64_t)((v1).len));
  t132 = ((t135) <= (t136));
  }
  t137 = t132;
  t138 = t137;
  if (t138) {
  t139 = ((int64_t)(v13));
  t140 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = ((t139) >= (t140));
  }
  t141 = t138;
  t142 = t141;
  if (t142) {
  t143 = ((int64_t)(v13));
  t144 = ((int64_t)(((int64_t)UINT64_C(4))));
  t145 = ((int64_t)(((int64_t)((uint64_t)(t143) + (uint64_t)(t144)))));
  t146 = ((int64_t)((v3).len));
  t142 = ((t145) <= (t146));
  }
  if (!(t142)) gg_fail("assertion failed");
  t148 = ((int64_t)UINT64_C(4));
  t147 = 0;
label6:;
  if (t147 >= t148) goto label8;
  v14 = t147;
  {
  t149 = v3;
  t150 = ((int64_t)(v13));
  t151 = ((int64_t)(v14));
  t152 = ((int64_t)((uint64_t)(t150) + (uint64_t)(t151)));
  t153 = ((uint8_t *)t149.data) + gg_index(t152, t149.len);
  t154 = v1;
  t155 = ((int64_t)(v12));
  t156 = ((int64_t)(v14));
  t157 = ((int64_t)((uint64_t)(t155) + (uint64_t)(t156)));
  t158 = ((uint8_t *)t154.data) + gg_index(t157, t154.len);
  (*t153) = ((uint8_t)((*t158)));
  }
label7:;
  t147++; goto label6;
label8:;
  }
label4:;
  t117++; goto label3;
label5:;
  t159 = v5;
  t160 = ((int64_t)(((*(v0))).f10));
  v5 = ((int64_t)((uint64_t)(t159) + (uint64_t)(t160)));
  }
  goto label1;
label2:;
  t161 = ((int64_t)(((int64_t)UINT64_C(40))));
  t162 = &(((*(v0))).f7);
  t163 = gg_function_15(t162, base + 0);
  fg143_witness(t161, t163);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_13(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  gg_type_12 t4;
  gg_slice t5;
  gg_slice t6;
  gg_slice t7;
  uint8_t t8;
  uint8_t t9;
  gg_type_12 t10;
  gg_slice t11;
  gg_slice t12;
  gg_slice t13;
  gg_slice t14;
  gg_slice t15;
  uint8_t t16;
  uint8_t t17;
  gg_type_12 t18;
  gg_slice t19;
  gg_slice t20;
  gg_slice t21;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(40))));
  t3 = ((t1) != (t2));
  if (t3) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t4 = v0;
  t5 = v1;
  t6 = v2;
  t7 = v3;
  t8 = gg_function_14(t4, t5, t6, t7, base + 0);
  t9 = t8;
  if (t9) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t10 = v0;
  t11 = v1;
  t12 = v2;
  t13 = v3;
  t14 = v4;
  t15 = v5;
  t16 = gg_function_19(t10, t11, t12, t13, t14, t15, base + 0);
  t17 = t16;
  if (t17) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t18 = v0;
  t19 = v1;
  t20 = v2;
  t21 = v3;
  t0 = gg_function_42(t18, t19, t20, t21, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_8(gg_type_12 p0, gg_slice p1, int64_t base) {
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
  int64_t * t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  int64_t t14;
  gg_type_20 t15;
  int64_t t16;
  int64_t t17;
  gg_type_20 t18;
  gg_slice t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  int64_t * t27;
  int64_t t28;
  uint8_t t29;
  gg_type_12 v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(40))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)((((*(v0))).f5).f2));
  t6 = ((int64_t)(((int64_t)UINT64_C(26))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  /* FG_DIRECT_SLICE_INDEX */
  t9 = ((int64_t)UINT64_C(1));
  t10 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t9, ((int64_t)UINT64_C(4)));
  t11 = ((int64_t)((*t10)));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t11) <= (t12));
  }
  t13 = t8;
  if (t13) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t15 = &(((*(v0))).f5);
  t16 = gg_function_9(t15, base + 0);
  t17 = t16;
  t14 = 0;
label0:;
  if (t14 >= t17) goto label2;
  v2 = t14;
  {
  t18 = &(((*(v0))).f5);
  t19 = v1;
  t20 = ((int64_t)(v2));
  v3 = gg_function_10(t18, t19, t20, base + 0);
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) < (t22));
  t24 = t23;
  if (!t24) {
  t25 = ((int64_t)(v3));
  /* FG_DIRECT_SLICE_INDEX */
  t26 = ((int64_t)UINT64_C(1));
  t27 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t26, ((int64_t)UINT64_C(4)));
  t28 = ((int64_t)((*t27)));
  t24 = ((t25) >= (t28));
  }
  t29 = t24;
  if (t29) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  }
label1:;
  t14++; goto label0;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_2(gg_type_12 p0, int64_t p1, gg_slice p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t * t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t * t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t * t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t * t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  uint8_t t67;
  uint8_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
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
  int64_t * t96;
  int64_t t97;
  int64_t * t98;
  int64_t t99;
  int64_t * t100;
  int64_t t101;
  int64_t * t102;
  int64_t t103;
  int64_t t104;
  gg_type_12 v0;
  int64_t v1;
  gg_slice v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_type_19 v6;
  int64_t v7;
  gg_type_19 v8;
  int64_t v9;
  int64_t v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((v2).len));
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = ((t4) >= (t5));
  }
  if (!(t3)) gg_fail("assertion failed");
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((*(v0))).f8));
  t7 = ((int64_t)(((int64_t)UINT64_C(40))));
  t8 = ((t6) == (t7));
  t9 = t8;
  if (t9) {
  t10 = ((int64_t)((((*(v0))).f7).f2));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t10) == (t11));
  }
  t12 = t9;
  t13 = t12;
  if (t13) {
  t14 = ((int64_t)((((*(v0))).f4).f2));
  t15 = gg_function_3(t14, base + 0);
  t16 = ((int64_t)(t15));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t16) == (t17));
  }
  t18 = t13;
  if (t18) {
  {
  t19 = ((int64_t)((((*(v0))).f4).f2));
  v5 = gg_function_4(t19, base + 0);
  t20 = ((int64_t)(v5));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) >= (t21));
  t23 = t22;
  if (t23) {
  /* FG_DIRECT_SLICE_INDEX */
  t24 = ((int64_t)UINT64_C(0));
  t25 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t24, ((int64_t)UINT64_C(4)));
  t26 = ((int64_t)((*t25)));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t26) > (t27));
  }
  t28 = t23;
  t29 = t28;
  if (t29) {
  /* FG_DIRECT_SLICE_INDEX */
  t30 = ((int64_t)UINT64_C(0));
  t31 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t30, ((int64_t)UINT64_C(4)));
  t32 = ((int64_t)((*t31)));
  t33 = ((int64_t)(((int64_t)UINT64_C(256))));
  t34 = ((int64_t)(((int64_t)gg_mod(t32, t33))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t34) == (t35));
  }
  t36 = t29;
  if (t36) {
  {
  t37 = ((int64_t)(v5));
  v6 = gg_function_5(t37, base + 0);
  t38 = ((int64_t)((v6).f13));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) > (t39));
  if (t40) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t41 = ((int64_t)UINT64_C(0));
  t42 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t41, ((int64_t)UINT64_C(4)));
  t43 = ((int64_t)((*t42)));
  t44 = ((int64_t)(v1));
  v3 = gg_function_6(t43, t44, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t45 = ((int64_t)UINT64_C(0));
  t46 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t45, ((int64_t)UINT64_C(4)));
  t47 = ((int64_t)((*t46)));
  t48 = ((int64_t)(((int64_t)UINT64_C(256))));
  t49 = ((int64_t)(((int64_t)gg_div(t47, t48))));
  t50 = ((int64_t)((v6).f13));
  t51 = gg_function_6(t49, t50, base + 0);
  t52 = ((int64_t)(v1));
  v4 = gg_function_6(t51, t52, base + 0);
  }
  }
  }
  }
  }
  }
  t53 = ((int64_t)(((*(v0))).f8));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((t53) == (t54));
  t56 = t55;
  if (!t56) {
  t57 = ((int64_t)(((*(v0))).f8));
  t58 = ((int64_t)(((int64_t)UINT64_C(34))));
  t56 = ((t57) == (t58));
  }
  t59 = t56;
  t60 = t59;
  if (!t60) {
  t61 = ((int64_t)(((*(v0))).f8));
  t62 = ((int64_t)(((int64_t)UINT64_C(35))));
  t60 = ((t61) == (t62));
  }
  t63 = t60;
  t64 = t63;
  if (t64) {
  t65 = ((int64_t)((((*(v0))).f7).f2));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((t65) == (t66));
  }
  t67 = t64;
  t68 = t67;
  if (t68) {
  t69 = ((int64_t)((((*(v0))).f4).f2));
  t70 = gg_function_3(t69, base + 0);
  t71 = ((int64_t)(t70));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = ((t71) == (t72));
  }
  t73 = t68;
  if (t73) {
  {
  t74 = ((int64_t)((((*(v0))).f4).f2));
  v7 = gg_function_4(t74, base + 0);
  t75 = ((int64_t)(v7));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = ((t75) >= (t76));
  if (t77) {
  {
  t78 = ((int64_t)(v7));
  v8 = gg_function_5(t78, base + 0);
  t79 = ((int64_t)((v8).f13));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = ((t79) > (t80));
  if (t81) {
  {
  t82 = ((int64_t)(((int64_t)UINT64_C(256))));
  t83 = ((int64_t)(v1));
  v3 = gg_function_6(t82, t83, base + 0);
  t84 = ((int64_t)((v8).f13));
  t85 = ((int64_t)(v1));
  v4 = gg_function_6(t84, t85, base + 0);
  }
  }
  }
  }
  }
  }
  t86 = ((int64_t)(v3));
  t87 = ((int64_t)(((int64_t)UINT64_C(4))));
  v9 = gg_function_6(t86, t87, base + 0);
  t88 = ((int64_t)(v9));
  t89 = ((int64_t)(((int64_t)UINT64_C(7))));
  t90 = gg_function_7(t88, t89, base + 0);
  t91 = ((int64_t)(t90));
  t92 = ((int64_t)(((int64_t)UINT64_C(8))));
  t93 = ((int64_t)(((int64_t)gg_div(t91, t92))));
  t94 = ((int64_t)(((int64_t)UINT64_C(8))));
  v10 = gg_function_6(t93, t94, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t95 = ((int64_t)UINT64_C(0));
  t96 = ((int64_t *)(v2).data) + gg_index(t95, (v2).len);
  (*t96) = ((int64_t)(v3));
  /* FG_DIRECT_SLICE_INDEX */
  t97 = ((int64_t)UINT64_C(1));
  t98 = ((int64_t *)(v2).data) + gg_index(t97, (v2).len);
  (*t98) = ((int64_t)(v4));
  /* FG_DIRECT_SLICE_INDEX */
  t99 = ((int64_t)UINT64_C(2));
  t100 = ((int64_t *)(v2).data) + gg_index(t99, (v2).len);
  (*t100) = ((int64_t)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t101 = ((int64_t)UINT64_C(3));
  t102 = ((int64_t *)(v2).data) + gg_index(t101, (v2).len);
  t103 = ((int64_t)(v10));
  t104 = ((int64_t)(v4));
  (*t102) = gg_function_7(t103, t104, base + 0);
  }
gg_done:;
}
static uint8_t gg_function_1(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, int64_t base) {
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
  gg_type_12 t16;
  int64_t t17;
  gg_slice t18;
  int64_t t19;
  int64_t * t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  gg_type_12 t24;
  gg_slice t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  gg_type_12 t29;
  gg_slice t30;
  gg_slice t31;
  gg_slice t32;
  gg_slice t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  gg_type_12 t38;
  gg_slice t39;
  gg_slice t40;
  gg_slice t41;
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
  uint8_t t52;
  gg_type_12 t53;
  gg_slice t54;
  gg_slice t55;
  gg_slice t56;
  gg_slice t57;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
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
  t16 = v0;
  t17 = ((int64_t)(((*(v0))).f10));
  t18 = v7;
  gg_function_2(t16, t17, t18, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t19 = ((int64_t)UINT64_C(15));
  t20 = ((int64_t *)(((*(v0))).f11).data) + gg_index(t19, ((int64_t)UINT64_C(16)));
  t21 = ((int64_t)((*t20)));
  t22 = ((int64_t)(((int64_t)UINT64_C(1464816204))));
  t23 = ((t21) == (t22));
  if (t23) {
  {
  t24 = v0;
  t25 = v2;
  t0 = gg_function_8(t24, t25, base + 0);
  goto gg_done;
  }
  }
  t26 = ((int64_t)(((*(v0))).f8));
  t27 = ((int64_t)(((int64_t)UINT64_C(40))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t29 = v0;
  t30 = v1;
  t31 = v2;
  t32 = v4;
  t33 = v5;
  t34 = v6;
  t0 = gg_function_13(t29, t30, t31, t32, t33, t34, base + 0);
  goto gg_done;
  }
  }
  t35 = ((int64_t)(((*(v0))).f8));
  t36 = ((int64_t)(((int64_t)UINT64_C(100))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  t38 = v0;
  t39 = v1;
  t40 = v2;
  t41 = v4;
  t0 = gg_function_47(t38, t39, t40, t41, base + 0);
  goto gg_done;
  }
  }
  t42 = ((int64_t)(((*(v0))).f8));
  t43 = ((int64_t)(((int64_t)UINT64_C(1))));
  t44 = ((t42) == (t43));
  t45 = t44;
  if (!t45) {
  t46 = ((int64_t)(((*(v0))).f8));
  t47 = ((int64_t)(((int64_t)UINT64_C(34))));
  t45 = ((t46) == (t47));
  }
  t48 = t45;
  t49 = t48;
  if (!t49) {
  t50 = ((int64_t)(((*(v0))).f8));
  t51 = ((int64_t)(((int64_t)UINT64_C(35))));
  t49 = ((t50) == (t51));
  }
  t52 = t49;
  if (t52) {
  {
  t53 = v0;
  t54 = v1;
  t55 = v4;
  t56 = v5;
  t57 = v6;
  t0 = gg_function_65(t53, t54, t55, t56, t57, base + 0);
  goto gg_done;
  }
  }
  t0 = ((uint8_t)UINT64_C(0));
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
  uint8_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  gg_header t56;
  gg_type_12 t57;
  gg_slice t58;
  gg_slice t59;
  gg_slice t60;
  gg_slice t61;
  gg_slice t62;
  gg_slice t63;
  gg_slice t64;
  uint8_t t65;
  int64_t t66;
  gg_type_1 v0;
  gg_type_9 v1;
  gg_type_9 v2;
  gg_type_10 v3;
  gg_type_11 v4;
  gg_type_4 v5;
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
  t51 = ((uint8_t)(t50));
  for (t52=0; t52<1; t52++) (v4).data[t52] = t51;
  memset(&t53, 0, sizeof(t53));
  t54 = ((int64_t)(t53));
  for (t55=0; t55<4; t55++) (v5).data[t55] = t54;
  t56.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t56.len = 0;
  t57 = &(v0);
  t58 = gg_view((v1).data, 4);
  t59 = gg_view((v1).data, 4);
  t60 = gg_view((v1).data, 4);
  t61 = gg_view((v2).data, 4);
  t62 = gg_view((v3).data, 1);
  t63 = gg_view((v4).data, 1);
  t64 = gg_view((v5).data, 4);
  t65 = gg_function_1(t57, t58, t59, t60, t61, t62, t63, t64, base + 1);
  gg_bool(gg_ref(&t56, gg_get_stack(base + 0)), t65);
  gg_append(gg_ref(&t56, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t56).base, (t56).len));
  }
gg_done:;
  for (t66 = 0; t66 < 1; t66++) gg_stacks[base + t66].top = 0;
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
