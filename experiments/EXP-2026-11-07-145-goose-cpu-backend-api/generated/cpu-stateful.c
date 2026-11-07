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
typedef struct { gg_type_2 data[7]; } gg_type_5;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_7;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_1 {
  gg_type_5 f4;
  gg_type_2 f5;
  int64_t f6;
  int64_t f7;
  int64_t f8;
  int64_t f9;
  gg_type_7 f10;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[1]; } gg_type_9;
#pragma pack(pop)
typedef gg_type_1 *gg_type_10;
typedef gg_type_2 *gg_type_15;
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_17;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_17 data[4]; } gg_type_18;
#pragma pack(pop)
typedef struct gg_type_24 gg_type_24;
#pragma pack(push, 1)
struct gg_type_24 {
  gg_type_5 f11;
  int64_t f12;
  int64_t f13;
  int64_t f14;
};
#pragma pack(pop)
typedef gg_type_24 *gg_type_25;
typedef struct gg_type_26 gg_type_26;
#pragma pack(push, 1)
typedef struct { int64_t data[16]; } gg_type_27;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_26 {
  gg_type_2 f15;
  gg_type_2 f16;
  gg_type_2 f17;
  gg_type_2 f18;
  int64_t f19;
  int64_t f20;
  int64_t f21;
  gg_type_27 f22;
  gg_type_17 f23;
};
#pragma pack(pop)
typedef gg_type_26 *gg_type_28;
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
static uint8_t gg_eq_11(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, gg_slice value);
static uint8_t gg_eq_12(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_slice value);
static uint8_t gg_eq_13(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, uint8_t value);
static uint8_t gg_eq_15(gg_type_15 a, gg_type_15 b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, gg_type_15 value);
static uint8_t gg_eq_16(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, uint32_t value);
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_type_17 value);
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, gg_type_18 value);
static uint8_t gg_eq_19(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, uint16_t value);
static uint8_t gg_eq_20(int32_t a, int32_t b);
static uint8_t gg_equal_elements_20(gg_slice a, gg_slice b);
static void gg_render_20(gg_reference out, int32_t value);
static uint8_t gg_eq_21(int16_t a, int16_t b);
static uint8_t gg_equal_elements_21(gg_slice a, gg_slice b);
static void gg_render_21(gg_reference out, int16_t value);
static uint8_t gg_eq_22(int8_t a, int8_t b);
static uint8_t gg_equal_elements_22(gg_slice a, gg_slice b);
static void gg_render_22(gg_reference out, int8_t value);
static uint8_t gg_eq_23(double a, double b);
static uint8_t gg_equal_elements_23(gg_slice a, gg_slice b);
static void gg_render_23(gg_reference out, double value);
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
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((gg_type_1*)a.data)[i],((gg_type_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b) {
if(!gg_eq_5(a.f4,b.f4))return 0;
if(!gg_eq_2(a.f5,b.f5))return 0;
if(!gg_eq_3(a.f6,b.f6))return 0;
if(!gg_eq_3(a.f7,b.f7))return 0;
if(!gg_eq_3(a.f8,b.f8))return 0;
if(!gg_eq_3(a.f9,b.f9))return 0;
if(!gg_eq_7(a.f10,b.f10))return 0;
return 1;
}
static void gg_render_1(gg_reference out, gg_type_1 value) {
gg_append(out,"\123\164\141\164\145\120\154\141\156\040\173\040",12,1);
gg_render_5(out, value.f4);
gg_append(out,", ",2,1);
gg_render_2(out, value.f5);
gg_append(out,", ",2,1);
gg_render_3(out, value.f6);
gg_append(out,", ",2,1);
gg_render_3(out, value.f7);
gg_append(out,", ",2,1);
gg_render_3(out, value.f8);
gg_append(out,", ",2,1);
gg_render_3(out, value.f9);
gg_append(out,", ",2,1);
gg_render_7(out, value.f10);
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
return gg_equal_elements_2(gg_view((a).data, 7),gg_view((b).data, 7));
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
int64_t i; gg_slice view=gg_view((value).data, 7);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_2(out, ((gg_type_2*)view.data)[i]);}gg_append(out,"]",1,1);
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
return gg_equal_elements_6(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
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
return gg_equal_elements_8(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
gg_text(out,gg_view((value).data, 1));
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
return a == b;
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_11(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_12(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_13(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((gg_type_15*)a.data)[i],((gg_type_15*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(gg_type_15 a, gg_type_15 b) {
return a == b;
}
static void gg_render_15(gg_reference out, gg_type_15 value) {
gg_render_2(out,*value);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_16(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_type_17*)a.data)[i],((gg_type_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b) {
return gg_equal_elements_6(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_17(gg_reference out, gg_type_17 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_6(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_type_18*)a.data)[i],((gg_type_18*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_type_18 a, gg_type_18 b) {
return gg_equal_elements_17(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_18(gg_reference out, gg_type_18 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_17(out, ((gg_type_17*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_19(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_20(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_21(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_22(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(double a, double b) {
return a == b;
}
static void gg_render_23(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_24(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_24(((gg_type_24*)a.data)[i],((gg_type_24*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_24(gg_type_24 a, gg_type_24 b) {
if(!gg_eq_5(a.f11,b.f11))return 0;
if(!gg_eq_3(a.f12,b.f12))return 0;
if(!gg_eq_3(a.f13,b.f13))return 0;
if(!gg_eq_3(a.f14,b.f14))return 0;
return 1;
}
static void gg_render_24(gg_reference out, gg_type_24 value) {
gg_append(out,"\123\143\141\156\120\154\141\156\040\173\040",11,1);
gg_render_5(out, value.f11);
gg_append(out,", ",2,1);
gg_render_3(out, value.f12);
gg_append(out,", ",2,1);
gg_render_3(out, value.f13);
gg_append(out,", ",2,1);
gg_render_3(out, value.f14);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_25(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_25(((gg_type_25*)a.data)[i],((gg_type_25*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_25(gg_type_25 a, gg_type_25 b) {
return a == b;
}
static void gg_render_25(gg_reference out, gg_type_25 value) {
gg_render_24(out,*value);
}
static uint8_t gg_equal_elements_26(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_26(((gg_type_26*)a.data)[i],((gg_type_26*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_26(gg_type_26 a, gg_type_26 b) {
if(!gg_eq_2(a.f15,b.f15))return 0;
if(!gg_eq_2(a.f16,b.f16))return 0;
if(!gg_eq_2(a.f17,b.f17))return 0;
if(!gg_eq_2(a.f18,b.f18))return 0;
if(!gg_eq_3(a.f19,b.f19))return 0;
if(!gg_eq_3(a.f20,b.f20))return 0;
if(!gg_eq_3(a.f21,b.f21))return 0;
if(!gg_eq_27(a.f22,b.f22))return 0;
if(!gg_eq_17(a.f23,b.f23))return 0;
return 1;
}
static void gg_render_26(gg_reference out, gg_type_26 value) {
gg_append(out,"\116\157\144\145\040\173\040",7,1);
gg_render_2(out, value.f15);
gg_append(out,", ",2,1);
gg_render_2(out, value.f16);
gg_append(out,", ",2,1);
gg_render_2(out, value.f17);
gg_append(out,", ",2,1);
gg_render_2(out, value.f18);
gg_append(out,", ",2,1);
gg_render_3(out, value.f19);
gg_append(out,", ",2,1);
gg_render_3(out, value.f20);
gg_append(out,", ",2,1);
gg_render_3(out, value.f21);
gg_append(out,", ",2,1);
gg_render_27(out, value.f22);
gg_append(out,", ",2,1);
gg_render_17(out, value.f23);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_27(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_27(((gg_type_27*)a.data)[i],((gg_type_27*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_27(gg_type_27 a, gg_type_27 b) {
return gg_equal_elements_3(gg_view((a).data, 16),gg_view((b).data, 16));
}
static void gg_render_27(gg_reference out, gg_type_27 value) {
int64_t i; gg_slice view=gg_view((value).data, 16);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_28(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_28(((gg_type_28*)a.data)[i],((gg_type_28*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_28(gg_type_28 a, gg_type_28 b) {
return a == b;
}
static void gg_render_28(gg_reference out, gg_type_28 value) {
gg_render_26(out,*value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static uint8_t gg_function_1(gg_type_10 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, int64_t base);
static uint8_t gg_function_2(gg_type_10 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, int64_t base);
static void gg_function_3(gg_type_15 p0, int64_t p1, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t base);
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_7(gg_type_15 p0, int64_t base);
static float gg_function_8(gg_slice p0, int64_t p1, int64_t base);
static uint32_t gg_function_9(gg_slice p0, int64_t p1, int64_t base);
static void gg_function_11(gg_slice p0, int64_t p1, float p2, int64_t base);
static void gg_function_13(gg_slice p0, int64_t p1, uint32_t p2, int64_t base);
static float gg_function_14(gg_type_15 p0, gg_slice p1, int64_t p2, int64_t p3, gg_type_15 p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static float gg_function_15(gg_type_15 p0, gg_slice p1, int64_t p2, int64_t base);
static uint16_t gg_function_16(gg_slice p0, int64_t p1, int64_t base);
static float gg_function_18(gg_type_15 p0, gg_slice p1, int64_t p2, int64_t base);
static float gg_function_20(gg_slice p0, int64_t p1, int64_t base);
static float gg_function_21(gg_type_15 p0, gg_slice p1, int64_t p2, int64_t p3, gg_type_15 p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static uint8_t gg_function_23(gg_type_10 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, int64_t base);
static uint8_t gg_function_26(gg_type_25 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, int64_t base);
static int64_t gg_function_27(gg_type_15 p0, int64_t p1, int64_t base);
static int64_t gg_function_28(gg_type_15 p0, int64_t base);
static float gg_function_29(gg_type_15 p0, gg_slice p1, int64_t p2, int64_t base);
static float gg_function_30(float p0, int64_t base);
static void gg_function_13(gg_slice p0, int64_t p1, uint32_t p2, int64_t base) {
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
static void gg_function_11(gg_slice p0, int64_t p1, float p2, int64_t base) {
  gg_slice t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  float t4;
  uint32_t t5;
  uint32_t t6;
  gg_slice v0;
  int64_t v1;
  float v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = v0;
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = gg_function_5(t1, t2, base + 0);
  t4 = ((float)(v2));
  t5 = fg143_bits(t4);
  t6 = ((uint32_t)(t5));
  gg_function_13(t0, t3, t6, base + 0);
  }
gg_done:;
}
static uint32_t gg_function_9(gg_slice p0, int64_t p1, int64_t base) {
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
static float gg_function_20(gg_slice p0, int64_t p1, int64_t base) {
  float t0;
  gg_slice t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint32_t t5;
  float t6;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(4))));
  t4 = gg_function_5(t2, t3, base + 0);
  t5 = gg_function_9(t1, t4, base + 0);
  t6 = fg143_from_bits(t5);
  t0 = ((float)(t6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint16_t gg_function_16(gg_slice p0, int64_t p1, int64_t base) {
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
static float gg_function_29(gg_type_15 p0, gg_slice p1, int64_t p2, int64_t base) {
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
  gg_type_15 v0;
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
  t6 = gg_function_9(t4, t5, base + 0);
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
  t13 = gg_function_16(t11, t12, base + 0);
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
  t20 = gg_function_16(t18, t19, base + 0);
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
  t30 = gg_function_9(t28, t29, base + 0);
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
  t36 = gg_function_16(t34, t35, base + 0);
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
static float gg_function_30(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
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
  t2 = ((float)(((float)(((int64_t)UINT64_C(20))))));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((float)(v0));
  }
  } else {
  {
  t4 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t5 = ((float)(v0));
  t6 = expf(t5);
  t7 = ((float)(t6));
  t8 = ((float)(((t4) + (t7))));
  t9 = logf(t8);
  t0 = ((float)(t9));
  }
  }
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
static int64_t gg_function_28(gg_type_15 p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t * t5;
  int64_t t6;
  gg_type_15 v0;
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
  v1 = gg_function_5(t3, t6, base + 0);
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
static int64_t gg_function_27(gg_type_15 p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  gg_type_15 t6;
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
  gg_type_15 v0;
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
  t7 = gg_function_28(t6, base + 0);
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
  t24 = gg_function_5(t20, t23, base + 0);
  v3 = gg_function_6(t15, t24, base + 0);
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
static uint8_t gg_function_26(gg_type_25 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, int64_t base) {
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
  int64_t t15;
  int64_t t16;
  int64_t t17;
  gg_type_2 * t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  gg_type_2 * t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  gg_type_2 * t26;
  gg_slice t27;
  int64_t t28;
  int64_t * t29;
  int64_t t30;
  gg_type_2 * t31;
  gg_slice t32;
  int64_t t33;
  int64_t * t34;
  int64_t t35;
  gg_type_2 * t36;
  gg_slice t37;
  int64_t t38;
  int64_t * t39;
  int64_t t40;
  gg_type_2 * t41;
  gg_slice t42;
  int64_t t43;
  int64_t * t44;
  int64_t t45;
  gg_type_2 * t46;
  gg_slice t47;
  int64_t t48;
  int64_t * t49;
  int64_t t50;
  gg_type_2 * t51;
  gg_slice t52;
  int64_t t53;
  int64_t * t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  uint8_t t58;
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
  int64_t t70;
  int64_t t71;
  gg_type_2 * t72;
  gg_slice t73;
  int64_t t74;
  int64_t * t75;
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
  gg_slice t107;
  int64_t t108;
  gg_type_2 * t109;
  gg_type_15 t110;
  int64_t t111;
  int64_t t112;
  uint32_t t113;
  int32_t t114;
  int32_t t115;
  uint8_t t116;
  uint8_t t117;
  int64_t t118;
  int64_t t119;
  gg_type_2 * t120;
  gg_slice t121;
  int64_t t122;
  int64_t * t123;
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
  gg_type_2 * t138;
  gg_type_15 t139;
  gg_slice t140;
  int64_t t141;
  int64_t t142;
  gg_type_2 * t143;
  gg_slice t144;
  int64_t t145;
  int64_t * t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  gg_type_2 * t151;
  gg_slice t152;
  int64_t t153;
  int64_t * t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  gg_type_2 * t160;
  gg_slice t161;
  int64_t t162;
  int64_t * t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  float t167;
  int64_t t168;
  gg_type_2 * t169;
  gg_type_15 t170;
  gg_slice t171;
  int64_t t172;
  int64_t t173;
  gg_type_2 * t174;
  gg_slice t175;
  int64_t t176;
  int64_t * t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  gg_type_2 * t181;
  gg_slice t182;
  int64_t t183;
  int64_t * t184;
  int64_t t185;
  int64_t t186;
  float t187;
  float t188;
  float t189;
  float t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  gg_type_2 * t194;
  gg_type_15 t195;
  gg_slice t196;
  int64_t t197;
  int64_t t198;
  gg_type_2 * t199;
  gg_slice t200;
  int64_t t201;
  int64_t * t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  gg_type_2 * t207;
  gg_slice t208;
  int64_t t209;
  int64_t * t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  gg_type_2 * t216;
  gg_slice t217;
  int64_t t218;
  int64_t * t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  gg_type_2 * t225;
  gg_slice t226;
  int64_t t227;
  int64_t * t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  float t232;
  float t233;
  int64_t t234;
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
  uint8_t t252;
  int64_t t253;
  gg_type_2 * t254;
  gg_type_15 t255;
  gg_slice t256;
  int64_t t257;
  int64_t t258;
  gg_type_2 * t259;
  gg_slice t260;
  int64_t t261;
  int64_t * t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  gg_type_2 * t267;
  gg_slice t268;
  int64_t t269;
  int64_t * t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  int64_t t275;
  gg_type_2 * t276;
  gg_slice t277;
  int64_t t278;
  int64_t * t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  gg_type_2 * t285;
  gg_slice t286;
  int64_t t287;
  int64_t * t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  gg_slice t292;
  int64_t t293;
  int64_t t294;
  gg_type_2 * t295;
  gg_type_15 t296;
  gg_slice t297;
  int64_t t298;
  int64_t t299;
  gg_type_2 * t300;
  gg_slice t301;
  int64_t t302;
  int64_t * t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  gg_type_2 * t308;
  gg_slice t309;
  int64_t t310;
  int64_t * t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  gg_type_2 * t317;
  gg_slice t318;
  int64_t t319;
  int64_t * t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  gg_type_2 * t326;
  gg_slice t327;
  int64_t t328;
  int64_t * t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  gg_type_2 * t334;
  gg_type_15 t335;
  gg_slice t336;
  int64_t t337;
  int64_t t338;
  gg_type_2 * t339;
  gg_slice t340;
  int64_t t341;
  int64_t * t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  int64_t t346;
  gg_type_2 * t347;
  gg_slice t348;
  int64_t t349;
  int64_t * t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  gg_type_2 * t356;
  gg_slice t357;
  int64_t t358;
  int64_t * t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t t364;
  gg_type_2 * t365;
  gg_slice t366;
  int64_t t367;
  int64_t * t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  uint8_t t372;
  float t373;
  int64_t t374;
  gg_type_2 * t375;
  gg_type_15 t376;
  gg_slice t377;
  int64_t t378;
  int64_t t379;
  gg_type_2 * t380;
  gg_slice t381;
  int64_t t382;
  int64_t * t383;
  int64_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  gg_type_2 * t388;
  gg_slice t389;
  int64_t t390;
  int64_t * t391;
  int64_t t392;
  int64_t t393;
  int64_t t394;
  float t395;
  float t396;
  float t397;
  float t398;
  float t399;
  float t400;
  float t401;
  float t402;
  float t403;
  float t404;
  gg_slice t405;
  int64_t t406;
  float t407;
  float t408;
  float t409;
  float t410;
  float t411;
  gg_slice t412;
  int64_t t413;
  int64_t t414;
  int64_t t415;
  int64_t t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  int64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  int64_t t425;
  float t426;
  int64_t t427;
  int64_t t428;
  int64_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  uint8_t t433;
  uint8_t t434;
  int64_t t435;
  int64_t t436;
  uint8_t t437;
  uint8_t t438;
  int64_t t439;
  int64_t t440;
  uint8_t t441;
  int64_t t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  int64_t t446;
  int64_t t447;
  int64_t t448;
  int64_t t449;
  int64_t t450;
  int64_t t451;
  gg_slice t452;
  int64_t t453;
  int64_t t454;
  int64_t t455;
  int64_t t456;
  int64_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  int64_t t461;
  int64_t t462;
  int64_t t463;
  gg_slice t464;
  int64_t t465;
  int64_t t466;
  int64_t t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
  float t472;
  int64_t t473;
  int64_t t474;
  gg_type_25 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
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
  int32_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  float v26;
  float v27;
  uint8_t v28;
  float v29;
  int64_t v30;
  float v31;
  float v32;
  float v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
  float v37;
  float v38;
  float v39;
  float v40;
  float v41;
  int64_t v42;
  int64_t v43;
  int64_t v44;
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
  t1 = ((int64_t)(((*(v0))).f13));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(((*(v0))).f12));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(((*(v0))).f12));
  t10 = ((int64_t)(((*(v0))).f13));
  t8 = ((t9) < (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(((*(v0))).f14));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t13) > (t14));
  }
  if (!(t12)) gg_fail("assertion failed");
  t16 = ((int64_t)UINT64_C(6));
  t15 = 0;
label0:;
  if (t15 >= t16) goto label2;
  v9 = t15;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t17 = v9;
  t18 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t17, ((int64_t)UINT64_C(7)));
  t19 = ((int64_t)(((*t18)).f2));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t19) == (t20)))) gg_fail("assertion failed");
  }
label1:;
  t15++; goto label0;
label2:;
  /* FG_DIRECT_SLICE_INDEX */
  t21 = ((int64_t)UINT64_C(6));
  t22 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t21, ((int64_t)UINT64_C(7)));
  t23 = ((int64_t)(((*t22)).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(26))));
  if (!(((t23) == (t24)))) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t25 = ((int64_t)UINT64_C(0));
  t26 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t25, ((int64_t)UINT64_C(7)));
  t27 = gg_view((((*t26)).f0).data, 4);
  t28 = ((int64_t)UINT64_C(0));
  t29 = ((int64_t *)t27.data) + gg_index(t28, t27.len);
  v10 = ((int64_t)((*t29)));
  /* FG_DIRECT_SLICE_INDEX */
  t30 = ((int64_t)UINT64_C(0));
  t31 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t30, ((int64_t)UINT64_C(7)));
  t32 = gg_view((((*t31)).f0).data, 4);
  t33 = ((int64_t)UINT64_C(1));
  t34 = ((int64_t *)t32.data) + gg_index(t33, t32.len);
  v11 = ((int64_t)((*t34)));
  /* FG_DIRECT_SLICE_INDEX */
  t35 = ((int64_t)UINT64_C(1));
  t36 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t35, ((int64_t)UINT64_C(7)));
  t37 = gg_view((((*t36)).f0).data, 4);
  t38 = ((int64_t)UINT64_C(1));
  t39 = ((int64_t *)t37.data) + gg_index(t38, t37.len);
  v12 = ((int64_t)((*t39)));
  /* FG_DIRECT_SLICE_INDEX */
  t40 = ((int64_t)UINT64_C(1));
  t41 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t40, ((int64_t)UINT64_C(7)));
  t42 = gg_view((((*t41)).f0).data, 4);
  t43 = ((int64_t)UINT64_C(2));
  t44 = ((int64_t *)t42.data) + gg_index(t43, t42.len);
  v13 = ((int64_t)((*t44)));
  /* FG_DIRECT_SLICE_INDEX */
  t45 = ((int64_t)UINT64_C(1));
  t46 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t45, ((int64_t)UINT64_C(7)));
  t47 = gg_view((((*t46)).f0).data, 4);
  t48 = ((int64_t)UINT64_C(3));
  t49 = ((int64_t *)t47.data) + gg_index(t48, t47.len);
  v14 = ((int64_t)((*t49)));
  /* FG_DIRECT_SLICE_INDEX */
  t50 = ((int64_t)UINT64_C(4));
  t51 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t50, ((int64_t)UINT64_C(7)));
  t52 = gg_view((((*t51)).f0).data, 4);
  t53 = ((int64_t)UINT64_C(1));
  t54 = ((int64_t *)t52.data) + gg_index(t53, t52.len);
  v15 = ((int64_t)((*t54)));
  t55 = ((int64_t)(v10));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t55) > (t56));
  t58 = t57;
  if (t58) {
  t59 = ((int64_t)(v11));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((t59) > (t60));
  }
  t61 = t58;
  t62 = t61;
  if (t62) {
  t63 = ((int64_t)(v15));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((t63) > (t64));
  }
  t65 = t62;
  t66 = t65;
  if (t66) {
  t67 = ((int64_t)(v12));
  t68 = ((int64_t)(v15));
  t69 = ((int64_t)(((int64_t)gg_mod(t67, t68))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((t69) == (t70));
  }
  if (!(t66)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t71 = ((int64_t)UINT64_C(0));
  t72 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t71, ((int64_t)UINT64_C(7)));
  t73 = gg_view((((*t72)).f1).data, 4);
  t74 = ((int64_t)UINT64_C(3));
  t75 = ((int64_t *)t73.data) + gg_index(t74, t73.len);
  t76 = ((int64_t)((*t75)));
  t77 = ((int64_t)(((int64_t)UINT64_C(4))));
  v16 = ((int64_t)(((int64_t)gg_div(t76, t77))));
  t78 = ((int64_t)(v11));
  t79 = ((int64_t)(v12));
  t80 = gg_function_5(t78, t79, base + 0);
  t81 = ((int64_t)(v13));
  t82 = gg_function_5(t80, t81, base + 0);
  t83 = ((int64_t)(v14));
  v17 = gg_function_5(t82, t83, base + 0);
  t84 = ((int64_t)((v8).len));
  t85 = ((int64_t)(v17));
  t86 = ((int64_t)(v16));
  t87 = ((int64_t)(v14));
  t88 = gg_function_5(t86, t87, base + 0);
  t89 = ((int64_t)(((*(v0))).f14));
  t90 = gg_function_5(t88, t89, base + 0);
  t91 = gg_function_6(t85, t90, base + 0);
  t92 = ((int64_t)(((int64_t)UINT64_C(4))));
  t93 = gg_function_5(t91, t92, base + 0);
  t94 = ((int64_t)(t93));
  if (!(((t84) >= (t94)))) gg_fail("assertion failed");
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(((*(v0))).f12));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t95) * (uint64_t)(t96)))));
  t98 = ((int64_t)(((*(v0))).f13));
  v18 = ((int64_t)(((int64_t)gg_div(t97, t98))));
  t99 = ((int64_t)(v12));
  t100 = ((int64_t)(((*(v0))).f12));
  t101 = ((int64_t)(((int64_t)UINT64_C(1))));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t101)))));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t102)))));
  t104 = ((int64_t)(((*(v0))).f13));
  v19 = ((int64_t)(((int64_t)gg_div(t103, t104))));
  t106 = v14;
  t105 = 0;
label3:;
  if (t105 >= t106) goto label5;
  v20 = t105;
  {
  t107 = v7;
  /* FG_DIRECT_SLICE_INDEX */
  t108 = ((int64_t)UINT64_C(6));
  t109 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t108, ((int64_t)UINT64_C(7)));
  t110 = t109;
  t111 = ((int64_t)(v20));
  t112 = gg_function_27(t110, t111, base + 0);
  t113 = gg_function_9(t107, t112, base + 0);
  v21 = ((int32_t)(((int32_t)(t113))));
  t114 = ((int32_t)(v21));
  t115 = ((int32_t)(((int64_t)UINT64_C(0))));
  t116 = ((t114) >= (t115));
  t117 = t116;
  if (t117) {
  t118 = ((int64_t)(((int64_t)(v21))));
  /* FG_DIRECT_SLICE_INDEX */
  t119 = ((int64_t)UINT64_C(0));
  t120 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t119, ((int64_t)UINT64_C(7)));
  t121 = gg_view((((*t120)).f0).data, 4);
  t122 = ((int64_t)UINT64_C(3));
  t123 = ((int64_t *)t121.data) + gg_index(t122, t121.len);
  t124 = ((int64_t)((*t123)));
  t117 = ((t118) < (t124));
  }
  if (!(t117)) gg_fail("assertion failed");
  t126 = v13;
  t125 = 0;
label6:;
  if (t125 >= t126) goto label8;
  v22 = t125;
  {
  t128 = ((int64_t)(v19));
  t129 = ((int64_t)(v18));
  t130 = ((int64_t)((uint64_t)(t128) - (uint64_t)(t129)));
  t127 = 0;
label9:;
  if (t127 >= t130) goto label11;
  v23 = t127;
  {
  t131 = ((int64_t)(v18));
  t132 = ((int64_t)(v23));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t131) + (uint64_t)(t132)))));
  t133 = ((int64_t)(v24));
  t134 = ((int64_t)(v12));
  t135 = ((int64_t)(v15));
  t136 = ((int64_t)(((int64_t)gg_div(t134, t135))));
  v25 = ((int64_t)(((int64_t)gg_div(t133, t136))));
  /* FG_DIRECT_SLICE_INDEX */
  t137 = ((int64_t)UINT64_C(2));
  t138 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t137, ((int64_t)UINT64_C(7)));
  t139 = t138;
  t140 = v3;
  t141 = ((int64_t)(v24));
  /* FG_DIRECT_SLICE_INDEX */
  t142 = ((int64_t)UINT64_C(2));
  t143 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t142, ((int64_t)UINT64_C(7)));
  t144 = gg_view((((*t143)).f1).data, 4);
  t145 = ((int64_t)UINT64_C(0));
  t146 = ((int64_t *)t144.data) + gg_index(t145, t144.len);
  t147 = ((int64_t)((*t146)));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t141) * (uint64_t)(t147)))));
  t149 = ((int64_t)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t150 = ((int64_t)UINT64_C(2));
  t151 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t150, ((int64_t)UINT64_C(7)));
  t152 = gg_view((((*t151)).f1).data, 4);
  t153 = ((int64_t)UINT64_C(1));
  t154 = ((int64_t *)t152.data) + gg_index(t153, t152.len);
  t155 = ((int64_t)((*t154)));
  t156 = ((int64_t)(((int64_t)((uint64_t)(t149) * (uint64_t)(t155)))));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t148) + (uint64_t)(t156)))));
  t158 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t159 = ((int64_t)UINT64_C(2));
  t160 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t159, ((int64_t)UINT64_C(7)));
  t161 = gg_view((((*t160)).f1).data, 4);
  t162 = ((int64_t)UINT64_C(2));
  t163 = ((int64_t *)t161.data) + gg_index(t162, t161.len);
  t164 = ((int64_t)((*t163)));
  t165 = ((int64_t)(((int64_t)((uint64_t)(t158) * (uint64_t)(t164)))));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t157) + (uint64_t)(t165)))));
  t167 = gg_function_29(t139, t140, t166, base + 0);
  v26 = gg_function_30(t167, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t168 = ((int64_t)UINT64_C(3));
  t169 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t168, ((int64_t)UINT64_C(7)));
  t170 = t169;
  t171 = v4;
  t172 = ((int64_t)(v24));
  /* FG_DIRECT_SLICE_INDEX */
  t173 = ((int64_t)UINT64_C(3));
  t174 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t173, ((int64_t)UINT64_C(7)));
  t175 = gg_view((((*t174)).f1).data, 4);
  t176 = ((int64_t)UINT64_C(1));
  t177 = ((int64_t *)t175.data) + gg_index(t176, t175.len);
  t178 = ((int64_t)((*t177)));
  t179 = ((int64_t)(((int64_t)((uint64_t)(t172) * (uint64_t)(t178)))));
  v27 = gg_function_29(t170, t171, t179, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t180 = ((int64_t)UINT64_C(3));
  t181 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t180, ((int64_t)UINT64_C(7)));
  t182 = gg_view((((*t181)).f0).data, 4);
  t183 = ((int64_t)UINT64_C(0));
  t184 = ((int64_t *)t182.data) + gg_index(t183, t182.len);
  t185 = ((int64_t)((*t184)));
  t186 = ((int64_t)(((int64_t)UINT64_C(1))));
  v28 = ((t185) == (t186));
  t187 = ((float)(v26));
  t188 = ((float)(v27));
  t189 = ((float)(((t187) * (t188))));
  t190 = expf(t189);
  v29 = ((float)(t190));
  t192 = v11;
  t191 = 0;
label12:;
  if (t191 >= t192) goto label14;
  v30 = t191;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t193 = ((int64_t)UINT64_C(1));
  t194 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t193, ((int64_t)UINT64_C(7)));
  t195 = t194;
  t196 = v2;
  t197 = ((int64_t)(v30));
  /* FG_DIRECT_SLICE_INDEX */
  t198 = ((int64_t)UINT64_C(1));
  t199 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t198, ((int64_t)UINT64_C(7)));
  t200 = gg_view((((*t199)).f1).data, 4);
  t201 = ((int64_t)UINT64_C(0));
  t202 = ((int64_t *)t200.data) + gg_index(t201, t200.len);
  t203 = ((int64_t)((*t202)));
  t204 = ((int64_t)(((int64_t)((uint64_t)(t197) * (uint64_t)(t203)))));
  t205 = ((int64_t)(v24));
  /* FG_DIRECT_SLICE_INDEX */
  t206 = ((int64_t)UINT64_C(1));
  t207 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t206, ((int64_t)UINT64_C(7)));
  t208 = gg_view((((*t207)).f1).data, 4);
  t209 = ((int64_t)UINT64_C(1));
  t210 = ((int64_t *)t208.data) + gg_index(t209, t208.len);
  t211 = ((int64_t)((*t210)));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t205) * (uint64_t)(t211)))));
  t213 = ((int64_t)(((int64_t)((uint64_t)(t204) + (uint64_t)(t212)))));
  t214 = ((int64_t)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t215 = ((int64_t)UINT64_C(1));
  t216 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t215, ((int64_t)UINT64_C(7)));
  t217 = gg_view((((*t216)).f1).data, 4);
  t218 = ((int64_t)UINT64_C(2));
  t219 = ((int64_t *)t217.data) + gg_index(t218, t217.len);
  t220 = ((int64_t)((*t219)));
  t221 = ((int64_t)(((int64_t)((uint64_t)(t214) * (uint64_t)(t220)))));
  t222 = ((int64_t)(((int64_t)((uint64_t)(t213) + (uint64_t)(t221)))));
  t223 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t224 = ((int64_t)UINT64_C(1));
  t225 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t224, ((int64_t)UINT64_C(7)));
  t226 = gg_view((((*t225)).f1).data, 4);
  t227 = ((int64_t)UINT64_C(3));
  t228 = ((int64_t *)t226.data) + gg_index(t227, t226.len);
  t229 = ((int64_t)((*t228)));
  t230 = ((int64_t)(((int64_t)((uint64_t)(t223) * (uint64_t)(t229)))));
  t231 = ((int64_t)(((int64_t)((uint64_t)(t222) + (uint64_t)(t230)))));
  v31 = gg_function_29(t195, t196, t231, base + 0);
  t232 = ((float)(v31));
  t233 = ((float)(v26));
  v32 = ((float)(((t232) * (t233))));
  v33 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t235 = v10;
  t234 = 0;
label15:;
  if (t234 >= t235) goto label17;
  v34 = t234;
  {
  t236 = ((int64_t)(v34));
  t237 = ((int64_t)(v30));
  t238 = ((int64_t)(v24));
  t239 = ((int64_t)(v11));
  t240 = ((int64_t)(((int64_t)((uint64_t)(t238) * (uint64_t)(t239)))));
  t241 = ((int64_t)(((int64_t)((uint64_t)(t237) + (uint64_t)(t240)))));
  t242 = ((int64_t)(v10));
  t243 = ((int64_t)(((int64_t)((uint64_t)(t241) * (uint64_t)(t242)))));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t236) + (uint64_t)(t243)))));
  t244 = ((int64_t)(v17));
  t245 = ((int64_t)(v20));
  t246 = ((int64_t)(v16));
  t247 = ((int64_t)(((int64_t)((uint64_t)(t245) * (uint64_t)(t246)))));
  t248 = ((int64_t)(((int64_t)((uint64_t)(t244) + (uint64_t)(t247)))));
  t249 = ((int64_t)(v35));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t248) + (uint64_t)(t249)))));
  t250 = ((int64_t)(v22));
  t251 = ((int64_t)(((int64_t)UINT64_C(0))));
  t252 = ((t250) == (t251));
  if (t252) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t253 = ((int64_t)UINT64_C(0));
  t254 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t253, ((int64_t)UINT64_C(7)));
  t255 = t254;
  t256 = v1;
  t257 = ((int64_t)(v34));
  /* FG_DIRECT_SLICE_INDEX */
  t258 = ((int64_t)UINT64_C(0));
  t259 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t258, ((int64_t)UINT64_C(7)));
  t260 = gg_view((((*t259)).f1).data, 4);
  t261 = ((int64_t)UINT64_C(0));
  t262 = ((int64_t *)t260.data) + gg_index(t261, t260.len);
  t263 = ((int64_t)((*t262)));
  t264 = ((int64_t)(((int64_t)((uint64_t)(t257) * (uint64_t)(t263)))));
  t265 = ((int64_t)(v30));
  /* FG_DIRECT_SLICE_INDEX */
  t266 = ((int64_t)UINT64_C(0));
  t267 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t266, ((int64_t)UINT64_C(7)));
  t268 = gg_view((((*t267)).f1).data, 4);
  t269 = ((int64_t)UINT64_C(1));
  t270 = ((int64_t *)t268.data) + gg_index(t269, t268.len);
  t271 = ((int64_t)((*t270)));
  t272 = ((int64_t)(((int64_t)((uint64_t)(t265) * (uint64_t)(t271)))));
  t273 = ((int64_t)(((int64_t)((uint64_t)(t264) + (uint64_t)(t272)))));
  t274 = ((int64_t)(v24));
  /* FG_DIRECT_SLICE_INDEX */
  t275 = ((int64_t)UINT64_C(0));
  t276 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t275, ((int64_t)UINT64_C(7)));
  t277 = gg_view((((*t276)).f1).data, 4);
  t278 = ((int64_t)UINT64_C(2));
  t279 = ((int64_t *)t277.data) + gg_index(t278, t277.len);
  t280 = ((int64_t)((*t279)));
  t281 = ((int64_t)(((int64_t)((uint64_t)(t274) * (uint64_t)(t280)))));
  t282 = ((int64_t)(((int64_t)((uint64_t)(t273) + (uint64_t)(t281)))));
  t283 = ((int64_t)(((int64_t)(v21))));
  /* FG_DIRECT_SLICE_INDEX */
  t284 = ((int64_t)UINT64_C(0));
  t285 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t284, ((int64_t)UINT64_C(7)));
  t286 = gg_view((((*t285)).f1).data, 4);
  t287 = ((int64_t)UINT64_C(3));
  t288 = ((int64_t *)t286.data) + gg_index(t287, t286.len);
  t289 = ((int64_t)((*t288)));
  t290 = ((int64_t)(((int64_t)((uint64_t)(t283) * (uint64_t)(t289)))));
  t291 = ((int64_t)(((int64_t)((uint64_t)(t282) + (uint64_t)(t290)))));
  v37 = gg_function_29(t255, t256, t291, base + 0);
  }
  } else {
  {
  t292 = v8;
  t293 = ((int64_t)(v36));
  v37 = gg_function_20(t292, t293, base + 0);
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t294 = ((int64_t)UINT64_C(4));
  t295 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t294, ((int64_t)UINT64_C(7)));
  t296 = t295;
  t297 = v5;
  t298 = ((int64_t)(v34));
  /* FG_DIRECT_SLICE_INDEX */
  t299 = ((int64_t)UINT64_C(4));
  t300 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t299, ((int64_t)UINT64_C(7)));
  t301 = gg_view((((*t300)).f1).data, 4);
  t302 = ((int64_t)UINT64_C(0));
  t303 = ((int64_t *)t301.data) + gg_index(t302, t301.len);
  t304 = ((int64_t)((*t303)));
  t305 = ((int64_t)(((int64_t)((uint64_t)(t298) * (uint64_t)(t304)))));
  t306 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t307 = ((int64_t)UINT64_C(4));
  t308 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t307, ((int64_t)UINT64_C(7)));
  t309 = gg_view((((*t308)).f1).data, 4);
  t310 = ((int64_t)UINT64_C(1));
  t311 = ((int64_t *)t309.data) + gg_index(t310, t309.len);
  t312 = ((int64_t)((*t311)));
  t313 = ((int64_t)(((int64_t)((uint64_t)(t306) * (uint64_t)(t312)))));
  t314 = ((int64_t)(((int64_t)((uint64_t)(t305) + (uint64_t)(t313)))));
  t315 = ((int64_t)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t316 = ((int64_t)UINT64_C(4));
  t317 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t316, ((int64_t)UINT64_C(7)));
  t318 = gg_view((((*t317)).f1).data, 4);
  t319 = ((int64_t)UINT64_C(2));
  t320 = ((int64_t *)t318.data) + gg_index(t319, t318.len);
  t321 = ((int64_t)((*t320)));
  t322 = ((int64_t)(((int64_t)((uint64_t)(t315) * (uint64_t)(t321)))));
  t323 = ((int64_t)(((int64_t)((uint64_t)(t314) + (uint64_t)(t322)))));
  t324 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t325 = ((int64_t)UINT64_C(4));
  t326 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t325, ((int64_t)UINT64_C(7)));
  t327 = gg_view((((*t326)).f1).data, 4);
  t328 = ((int64_t)UINT64_C(3));
  t329 = ((int64_t *)t327.data) + gg_index(t328, t327.len);
  t330 = ((int64_t)((*t329)));
  t331 = ((int64_t)(((int64_t)((uint64_t)(t324) * (uint64_t)(t330)))));
  t332 = ((int64_t)(((int64_t)((uint64_t)(t323) + (uint64_t)(t331)))));
  v38 = gg_function_29(t296, t297, t332, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t333 = ((int64_t)UINT64_C(5));
  t334 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t333, ((int64_t)UINT64_C(7)));
  t335 = t334;
  t336 = v6;
  t337 = ((int64_t)(v34));
  /* FG_DIRECT_SLICE_INDEX */
  t338 = ((int64_t)UINT64_C(5));
  t339 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t338, ((int64_t)UINT64_C(7)));
  t340 = gg_view((((*t339)).f1).data, 4);
  t341 = ((int64_t)UINT64_C(0));
  t342 = ((int64_t *)t340.data) + gg_index(t341, t340.len);
  t343 = ((int64_t)((*t342)));
  t344 = ((int64_t)(((int64_t)((uint64_t)(t337) * (uint64_t)(t343)))));
  t345 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t346 = ((int64_t)UINT64_C(5));
  t347 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t346, ((int64_t)UINT64_C(7)));
  t348 = gg_view((((*t347)).f1).data, 4);
  t349 = ((int64_t)UINT64_C(1));
  t350 = ((int64_t *)t348.data) + gg_index(t349, t348.len);
  t351 = ((int64_t)((*t350)));
  t352 = ((int64_t)(((int64_t)((uint64_t)(t345) * (uint64_t)(t351)))));
  t353 = ((int64_t)(((int64_t)((uint64_t)(t344) + (uint64_t)(t352)))));
  t354 = ((int64_t)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t355 = ((int64_t)UINT64_C(5));
  t356 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t355, ((int64_t)UINT64_C(7)));
  t357 = gg_view((((*t356)).f1).data, 4);
  t358 = ((int64_t)UINT64_C(2));
  t359 = ((int64_t *)t357.data) + gg_index(t358, t357.len);
  t360 = ((int64_t)((*t359)));
  t361 = ((int64_t)(((int64_t)((uint64_t)(t354) * (uint64_t)(t360)))));
  t362 = ((int64_t)(((int64_t)((uint64_t)(t353) + (uint64_t)(t361)))));
  t363 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t364 = ((int64_t)UINT64_C(5));
  t365 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t364, ((int64_t)UINT64_C(7)));
  t366 = gg_view((((*t365)).f1).data, 4);
  t367 = ((int64_t)UINT64_C(3));
  t368 = ((int64_t *)t366.data) + gg_index(t367, t366.len);
  t369 = ((int64_t)((*t368)));
  t370 = ((int64_t)(((int64_t)((uint64_t)(t363) * (uint64_t)(t369)))));
  t371 = ((int64_t)(((int64_t)((uint64_t)(t362) + (uint64_t)(t370)))));
  v39 = gg_function_29(t335, t336, t371, base + 0);
  t372 = v28;
  if (t372) {
  {
  v40 = ((float)(v29));
  }
  } else {
  {
  t373 = ((float)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t374 = ((int64_t)UINT64_C(3));
  t375 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t374, ((int64_t)UINT64_C(7)));
  t376 = t375;
  t377 = v4;
  t378 = ((int64_t)(v34));
  /* FG_DIRECT_SLICE_INDEX */
  t379 = ((int64_t)UINT64_C(3));
  t380 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t379, ((int64_t)UINT64_C(7)));
  t381 = gg_view((((*t380)).f1).data, 4);
  t382 = ((int64_t)UINT64_C(0));
  t383 = ((int64_t *)t381.data) + gg_index(t382, t381.len);
  t384 = ((int64_t)((*t383)));
  t385 = ((int64_t)(((int64_t)((uint64_t)(t378) * (uint64_t)(t384)))));
  t386 = ((int64_t)(v24));
  /* FG_DIRECT_SLICE_INDEX */
  t387 = ((int64_t)UINT64_C(3));
  t388 = ((gg_type_2 *)(((*(v0))).f11).data) + gg_index(t387, ((int64_t)UINT64_C(7)));
  t389 = gg_view((((*t388)).f1).data, 4);
  t390 = ((int64_t)UINT64_C(1));
  t391 = ((int64_t *)t389.data) + gg_index(t390, t389.len);
  t392 = ((int64_t)((*t391)));
  t393 = ((int64_t)(((int64_t)((uint64_t)(t386) * (uint64_t)(t392)))));
  t394 = ((int64_t)(((int64_t)((uint64_t)(t385) + (uint64_t)(t393)))));
  t395 = gg_function_29(t376, t377, t394, base + 0);
  t396 = ((float)(t395));
  t397 = ((float)(((t373) * (t396))));
  t398 = expf(t397);
  v40 = ((float)(t398));
  }
  }
  t399 = ((float)(v37));
  t400 = ((float)(v40));
  t401 = ((float)(((t399) * (t400))));
  t402 = ((float)(v38));
  t403 = ((float)(v32));
  t404 = ((float)(((t402) * (t403))));
  v41 = ((float)(((t401) + (t404))));
  t405 = v8;
  t406 = ((int64_t)(v36));
  t407 = ((float)(v41));
  gg_function_11(t405, t406, t407, base + 0);
  t408 = v33;
  t409 = ((float)(v41));
  t410 = ((float)(v39));
  t411 = ((float)(((t409) * (t410))));
  v33 = ((t408) + (t411));
  }
label16:;
  t234++; goto label15;
label17:;
  t412 = v8;
  t413 = ((int64_t)(v30));
  t414 = ((int64_t)(v11));
  t415 = ((int64_t)(v24));
  t416 = ((int64_t)(v12));
  t417 = ((int64_t)(v22));
  t418 = ((int64_t)(v13));
  t419 = ((int64_t)(v20));
  t420 = ((int64_t)(((int64_t)((uint64_t)(t418) * (uint64_t)(t419)))));
  t421 = ((int64_t)(((int64_t)((uint64_t)(t417) + (uint64_t)(t420)))));
  t422 = ((int64_t)(((int64_t)((uint64_t)(t416) * (uint64_t)(t421)))));
  t423 = ((int64_t)(((int64_t)((uint64_t)(t415) + (uint64_t)(t422)))));
  t424 = ((int64_t)(((int64_t)((uint64_t)(t414) * (uint64_t)(t423)))));
  t425 = ((int64_t)(((int64_t)((uint64_t)(t413) + (uint64_t)(t424)))));
  t426 = ((float)(v33));
  gg_function_11(t412, t425, t426, base + 0);
  }
label13:;
  t191++; goto label12;
label14:;
  t427 = ((int64_t)(v13));
  t428 = ((int64_t)(((int64_t)UINT64_C(1))));
  t429 = ((int64_t)(((int64_t)((uint64_t)(t427) - (uint64_t)(t428)))));
  t430 = ((int64_t)(v22));
  v42 = ((int64_t)(((int64_t)((uint64_t)(t429) - (uint64_t)(t430)))));
  t431 = ((int64_t)(((*(v0))).f14));
  t432 = ((int64_t)(((int64_t)UINT64_C(1))));
  t433 = ((t431) > (t432));
  t434 = t433;
  if (t434) {
  t435 = ((int64_t)(v42));
  t436 = ((int64_t)(((int64_t)UINT64_C(0))));
  t434 = ((t435) > (t436));
  }
  t437 = t434;
  t438 = t437;
  if (t438) {
  t439 = ((int64_t)(v42));
  t440 = ((int64_t)(((*(v0))).f14));
  t438 = ((t439) < (t440));
  }
  t441 = t438;
  if (t441) {
  {
  t443 = ((int64_t)(v10));
  t444 = ((int64_t)(v11));
  t445 = ((int64_t)((uint64_t)(t443) * (uint64_t)(t444)));
  t442 = 0;
label18:;
  if (t442 >= t445) goto label20;
  v43 = t442;
  {
  t446 = ((int64_t)(v43));
  t447 = ((int64_t)(v24));
  t448 = ((int64_t)(v10));
  t449 = ((int64_t)(((int64_t)((uint64_t)(t447) * (uint64_t)(t448)))));
  t450 = ((int64_t)(v11));
  t451 = ((int64_t)(((int64_t)((uint64_t)(t449) * (uint64_t)(t450)))));
  v44 = ((int64_t)(((int64_t)((uint64_t)(t446) + (uint64_t)(t451)))));
  t452 = v8;
  t453 = ((int64_t)(v17));
  t454 = ((int64_t)(v42));
  t455 = ((int64_t)(v14));
  t456 = ((int64_t)(((int64_t)((uint64_t)(t454) * (uint64_t)(t455)))));
  t457 = ((int64_t)(v20));
  t458 = ((int64_t)(((int64_t)((uint64_t)(t456) + (uint64_t)(t457)))));
  t459 = ((int64_t)(v16));
  t460 = ((int64_t)(((int64_t)((uint64_t)(t458) * (uint64_t)(t459)))));
  t461 = ((int64_t)(((int64_t)((uint64_t)(t453) + (uint64_t)(t460)))));
  t462 = ((int64_t)(v44));
  t463 = ((int64_t)(((int64_t)((uint64_t)(t461) + (uint64_t)(t462)))));
  t464 = v8;
  t465 = ((int64_t)(v17));
  t466 = ((int64_t)(v20));
  t467 = ((int64_t)(v16));
  t468 = ((int64_t)(((int64_t)((uint64_t)(t466) * (uint64_t)(t467)))));
  t469 = ((int64_t)(((int64_t)((uint64_t)(t465) + (uint64_t)(t468)))));
  t470 = ((int64_t)(v44));
  t471 = ((int64_t)(((int64_t)((uint64_t)(t469) + (uint64_t)(t470)))));
  t472 = gg_function_20(t464, t471, base + 0);
  gg_function_11(t452, t463, t472, base + 0);
  }
label19:;
  t442++; goto label18;
label20:;
  }
  }
  }
label10:;
  t127++; goto label9;
label11:;
  }
label7:;
  t125++; goto label6;
label8:;
  }
label4:;
  t105++; goto label3;
label5:;
  t473 = ((int64_t)(((int64_t)UINT64_C(77))));
  t474 = ((int64_t)(v17));
  fg143_witness(t473, t474);
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
static void gg_function_3(gg_type_15 p0, int64_t p1, int64_t base) {
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
  gg_type_15 v0;
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
  t35 = gg_function_5(t31, t34, base + 0);
  v3 = gg_function_6(t26, t35, base + 0);
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
static float gg_function_15(gg_type_15 p0, gg_slice p1, int64_t p2, int64_t base) {
  /* FG_SHARED_BODY:29 */
  return gg_function_29(p0, p1, p2, base);
}
static float gg_function_18(gg_type_15 p0, gg_slice p1, int64_t p2, int64_t base) {
  /* FG_SHARED_BODY:29 */
  return gg_function_29(p0, p1, p2, base);
}
static float gg_function_21(gg_type_15 p0, gg_slice p1, int64_t p2, int64_t p3, gg_type_15 p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  gg_type_17 t3;
  float t4;
  float t5;
  int64_t t6;
  gg_type_17 t7;
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
  gg_type_15 t25;
  gg_slice t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  gg_type_15 t32;
  gg_slice t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  gg_type_17 * t40;
  gg_slice t41;
  int64_t t42;
  float * t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  int64_t t47;
  gg_type_17 * t48;
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
  gg_type_17 * t59;
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
  gg_type_17 * t75;
  gg_slice t76;
  int64_t t77;
  float * t78;
  float t79;
  int64_t t80;
  gg_type_17 * t81;
  gg_slice t82;
  int64_t t83;
  float * t84;
  float t85;
  float t86;
  int64_t t87;
  gg_type_17 * t88;
  gg_slice t89;
  int64_t t90;
  float * t91;
  float t92;
  int64_t t93;
  gg_type_17 * t94;
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
  gg_type_15 t136;
  gg_slice t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  float t143;
  float t144;
  gg_type_15 t145;
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
  gg_type_15 t175;
  gg_slice t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  float t182;
  float t183;
  gg_type_15 t184;
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
  gg_type_15 v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  gg_type_15 v4;
  gg_slice v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  gg_type_18 v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  float v15;
  float v16;
  gg_type_17 v17;
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
  v15 = gg_function_29(t25, t26, t31, base + 0);
  t32 = v4;
  t33 = v5;
  t34 = ((int64_t)(v6));
  t35 = ((int64_t)(v14));
  t36 = ((int64_t)(v7));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t37)))));
  v16 = gg_function_29(t32, t33, t38, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t39 = v12;
  t40 = ((gg_type_17 *)(v9).data) + gg_index(t39, ((int64_t)UINT64_C(4)));
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
  t48 = ((gg_type_17 *)(v9).data) + gg_index(t47, ((int64_t)UINT64_C(4)));
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
  t59 = ((gg_type_17 *)(v9).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
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
  t75 = ((gg_type_17 *)(v9).data) + gg_index(t74, ((int64_t)UINT64_C(4)));
  t76 = gg_view(((*t75)).data, 8);
  t77 = v18;
  t78 = ((float *)t76.data) + gg_index(t77, t76.len);
  t79 = ((float)((*t78)));
  /* FG_DIRECT_SLICE_INDEX */
  t80 = ((int64_t)UINT64_C(2));
  t81 = ((gg_type_17 *)(v9).data) + gg_index(t80, ((int64_t)UINT64_C(4)));
  t82 = gg_view(((*t81)).data, 8);
  t83 = v18;
  t84 = ((float *)t82.data) + gg_index(t83, t82.len);
  t85 = ((float)((*t84)));
  t86 = ((float)(((t79) + (t85))));
  /* FG_DIRECT_SLICE_INDEX */
  t87 = ((int64_t)UINT64_C(1));
  t88 = ((gg_type_17 *)(v9).data) + gg_index(t87, ((int64_t)UINT64_C(4)));
  t89 = gg_view(((*t88)).data, 8);
  t90 = v18;
  t91 = ((float *)t89.data) + gg_index(t90, t89.len);
  t92 = ((float)((*t91)));
  /* FG_DIRECT_SLICE_INDEX */
  t93 = ((int64_t)UINT64_C(3));
  t94 = ((gg_type_17 *)(v9).data) + gg_index(t93, ((int64_t)UINT64_C(4)));
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
  t143 = gg_function_29(t136, t137, t142, base + 0);
  t144 = ((float)(t143));
  t145 = v4;
  t146 = v5;
  t147 = ((int64_t)(v6));
  t148 = ((int64_t)(v11));
  t149 = ((int64_t)(v7));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) * (uint64_t)(t149)))));
  t151 = ((int64_t)(((int64_t)((uint64_t)(t147) + (uint64_t)(t150)))));
  t152 = gg_function_29(t145, t146, t151, base + 0);
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
  t182 = gg_function_29(t175, t176, t181, base + 0);
  t183 = ((float)(t182));
  t184 = v4;
  t185 = v5;
  t186 = ((int64_t)(v6));
  t187 = ((int64_t)(v11));
  t188 = ((int64_t)(v7));
  t189 = ((int64_t)(((int64_t)((uint64_t)(t187) * (uint64_t)(t188)))));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t186) + (uint64_t)(t189)))));
  t191 = gg_function_29(t184, t185, t190, base + 0);
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
static uint8_t gg_function_23(gg_type_10 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, int64_t base) {
  uint8_t t0;
  int64_t t1;
  gg_type_2 * t2;
  gg_slice t3;
  int64_t t4;
  int64_t * t5;
  int64_t t6;
  gg_type_2 * t7;
  gg_slice t8;
  int64_t t9;
  int64_t * t10;
  int64_t t11;
  gg_type_2 * t12;
  gg_slice t13;
  int64_t t14;
  int64_t * t15;
  int64_t t16;
  gg_type_2 * t17;
  gg_slice t18;
  int64_t t19;
  int64_t * t20;
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
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  gg_type_2 * t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  gg_type_2 * t60;
  gg_slice t61;
  int64_t t62;
  int64_t * t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  uint8_t t67;
  int64_t t68;
  gg_type_2 * t69;
  gg_slice t70;
  int64_t t71;
  int64_t * t72;
  int64_t t73;
  int64_t t74;
  uint8_t t75;
  uint8_t t76;
  int64_t t77;
  gg_type_2 * t78;
  gg_slice t79;
  int64_t t80;
  int64_t * t81;
  int64_t t82;
  int64_t t83;
  uint8_t t84;
  uint8_t t85;
  int64_t t86;
  gg_type_2 * t87;
  gg_slice t88;
  int64_t t89;
  int64_t * t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  gg_type_2 * t97;
  gg_slice t98;
  int64_t t99;
  int64_t * t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  uint8_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  gg_type_2 * t108;
  gg_slice t109;
  int64_t t110;
  int64_t * t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  uint8_t t115;
  uint8_t t116;
  int64_t t117;
  gg_type_2 * t118;
  gg_slice t119;
  int64_t t120;
  int64_t * t121;
  int64_t t122;
  int64_t t123;
  uint8_t t124;
  uint8_t t125;
  int64_t t126;
  gg_type_2 * t127;
  gg_slice t128;
  int64_t t129;
  int64_t * t130;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  uint8_t t134;
  int64_t t135;
  int64_t t136;
  gg_type_2 * t137;
  gg_slice t138;
  int64_t t139;
  int64_t * t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  uint8_t t144;
  uint8_t t145;
  int64_t t146;
  int64_t t147;
  gg_type_2 * t148;
  gg_slice t149;
  int64_t t150;
  int64_t * t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  gg_type_2 * t156;
  gg_slice t157;
  int64_t t158;
  int64_t * t159;
  int64_t t160;
  int64_t t161;
  uint8_t t162;
  uint8_t t163;
  int64_t t164;
  gg_type_2 * t165;
  gg_slice t166;
  int64_t t167;
  int64_t * t168;
  int64_t t169;
  int64_t t170;
  uint8_t t171;
  uint8_t t172;
  int64_t t173;
  gg_type_2 * t174;
  gg_slice t175;
  int64_t t176;
  int64_t * t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  gg_type_2 * t181;
  gg_slice t182;
  int64_t t183;
  int64_t * t184;
  int64_t t185;
  int64_t t186;
  uint8_t t187;
  uint8_t t188;
  int64_t t189;
  gg_type_2 * t190;
  gg_slice t191;
  int64_t t192;
  int64_t * t193;
  int64_t t194;
  int64_t t195;
  uint8_t t196;
  uint8_t t197;
  int64_t t198;
  gg_type_2 * t199;
  gg_slice t200;
  int64_t t201;
  int64_t * t202;
  int64_t t203;
  int64_t t204;
  uint8_t t205;
  uint8_t t206;
  int64_t t207;
  gg_type_2 * t208;
  gg_slice t209;
  int64_t t210;
  int64_t * t211;
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
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  float t235;
  float t236;
  float t237;
  float t238;
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
  gg_type_2 * t250;
  gg_slice t251;
  int64_t t252;
  int64_t * t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  gg_type_2 * t257;
  gg_slice t258;
  int64_t t259;
  int64_t * t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  gg_type_2 * t265;
  gg_slice t266;
  int64_t t267;
  int64_t * t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  gg_type_2 * t274;
  gg_slice t275;
  int64_t t276;
  int64_t * t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  uint8_t t286;
  uint8_t t287;
  int64_t t288;
  int64_t t289;
  uint8_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  gg_type_2 * t297;
  gg_slice t298;
  int64_t t299;
  int64_t * t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  int64_t t304;
  gg_type_2 * t305;
  gg_slice t306;
  int64_t t307;
  int64_t * t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  gg_type_2 * t314;
  gg_slice t315;
  int64_t t316;
  int64_t * t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  gg_type_2 * t323;
  gg_slice t324;
  int64_t t325;
  int64_t * t326;
  int64_t t327;
  int64_t t328;
  gg_slice t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  int64_t t336;
  int64_t t337;
  gg_type_2 * t338;
  gg_type_15 t339;
  gg_slice t340;
  int64_t t341;
  float t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  int64_t t346;
  gg_type_2 * t347;
  gg_slice t348;
  int64_t t349;
  int64_t * t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  gg_type_2 * t355;
  gg_slice t356;
  int64_t t357;
  int64_t * t358;
  int64_t t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  gg_type_2 * t364;
  gg_slice t365;
  int64_t t366;
  int64_t * t367;
  int64_t t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  gg_type_2 * t372;
  gg_slice t373;
  int64_t t374;
  int64_t * t375;
  int64_t t376;
  int64_t t377;
  int64_t t378;
  int64_t t379;
  gg_type_2 * t380;
  gg_slice t381;
  int64_t t382;
  int64_t * t383;
  int64_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  int64_t t388;
  gg_type_2 * t389;
  gg_slice t390;
  int64_t t391;
  int64_t * t392;
  int64_t t393;
  int64_t t394;
  int64_t t395;
  int64_t t396;
  gg_type_2 * t397;
  gg_slice t398;
  int64_t t399;
  int64_t * t400;
  int64_t t401;
  int64_t t402;
  int64_t t403;
  int64_t t404;
  gg_type_2 * t405;
  gg_slice t406;
  int64_t t407;
  int64_t * t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  int64_t t413;
  gg_type_2 * t414;
  gg_slice t415;
  int64_t t416;
  int64_t * t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  int64_t t421;
  gg_type_2 * t422;
  gg_slice t423;
  int64_t t424;
  int64_t * t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  int64_t t429;
  gg_type_2 * t430;
  gg_slice t431;
  int64_t t432;
  int64_t * t433;
  int64_t t434;
  int64_t t435;
  int64_t t436;
  int64_t t437;
  int64_t t438;
  gg_type_2 * t439;
  gg_slice t440;
  int64_t t441;
  int64_t * t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  int64_t t446;
  gg_type_2 * t447;
  gg_slice t448;
  int64_t t449;
  int64_t * t450;
  int64_t t451;
  int64_t t452;
  int64_t t453;
  int64_t t454;
  gg_type_2 * t455;
  gg_slice t456;
  int64_t t457;
  int64_t * t458;
  int64_t t459;
  int64_t t460;
  int64_t t461;
  int64_t t462;
  int64_t t463;
  gg_type_2 * t464;
  gg_slice t465;
  int64_t t466;
  int64_t * t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  gg_type_2 * t471;
  gg_type_15 t472;
  gg_slice t473;
  int64_t t474;
  int64_t t475;
  int64_t t476;
  int64_t t477;
  int64_t t478;
  int64_t t479;
  gg_type_2 * t480;
  gg_type_15 t481;
  gg_slice t482;
  int64_t t483;
  int64_t t484;
  int64_t t485;
  gg_type_2 * t486;
  gg_slice t487;
  int64_t t488;
  int64_t * t489;
  int64_t t490;
  int64_t t491;
  uint8_t t492;
  int64_t t493;
  int64_t t494;
  gg_type_2 * t495;
  gg_slice t496;
  int64_t t497;
  int64_t * t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  float t502;
  float t503;
  int64_t t504;
  int64_t t505;
  int64_t t506;
  int64_t t507;
  int64_t t508;
  int64_t t509;
  gg_slice t510;
  int64_t t511;
  gg_slice t512;
  int64_t t513;
  float t514;
  float t515;
  float t516;
  float t517;
  int64_t t518;
  int64_t t519;
  gg_type_15 t520;
  gg_slice t521;
  int64_t t522;
  int64_t t523;
  int64_t t524;
  int64_t t525;
  int64_t t526;
  int64_t t527;
  int64_t t528;
  int64_t t529;
  int64_t t530;
  gg_type_2 * t531;
  gg_type_15 t532;
  gg_slice t533;
  int64_t t534;
  int64_t t535;
  gg_type_2 * t536;
  gg_slice t537;
  int64_t t538;
  int64_t * t539;
  int64_t t540;
  int64_t t541;
  int64_t t542;
  gg_type_2 * t543;
  gg_type_15 t544;
  gg_slice t545;
  int64_t t546;
  int64_t t547;
  int64_t t548;
  gg_type_2 * t549;
  gg_slice t550;
  int64_t t551;
  int64_t * t552;
  int64_t t553;
  int64_t t554;
  int64_t t555;
  float t556;
  float t557;
  float t558;
  float t559;
  float t560;
  int64_t t561;
  int64_t t562;
  int64_t t563;
  int64_t t564;
  int64_t t565;
  int64_t t566;
  int64_t t567;
  int64_t t568;
  int64_t t569;
  gg_type_2 * t570;
  gg_type_15 t571;
  gg_slice t572;
  int64_t t573;
  int64_t t574;
  int64_t t575;
  gg_type_2 * t576;
  gg_slice t577;
  int64_t t578;
  int64_t * t579;
  int64_t t580;
  int64_t t581;
  int64_t t582;
  gg_slice t583;
  int64_t t584;
  float t585;
  float t586;
  gg_slice t587;
  int64_t t588;
  float t589;
  float t590;
  float t591;
  int64_t t592;
  int64_t t593;
  gg_type_15 t594;
  gg_slice t595;
  int64_t t596;
  int64_t t597;
  int64_t t598;
  int64_t t599;
  int64_t t600;
  int64_t t601;
  int64_t t602;
  int64_t t603;
  int64_t t604;
  gg_type_2 * t605;
  gg_type_15 t606;
  gg_slice t607;
  int64_t t608;
  int64_t t609;
  gg_type_2 * t610;
  gg_slice t611;
  int64_t t612;
  int64_t * t613;
  int64_t t614;
  int64_t t615;
  gg_slice t616;
  int64_t t617;
  int64_t t618;
  int64_t t619;
  int64_t t620;
  int64_t t621;
  int64_t t622;
  int64_t t623;
  int64_t t624;
  int64_t t625;
  int64_t t626;
  int64_t t627;
  int64_t t628;
  int64_t t629;
  int64_t t630;
  int64_t t631;
  float t632;
  float t633;
  float t634;
  int64_t t635;
  int64_t t636;
  int64_t t637;
  int64_t t638;
  int64_t t639;
  int64_t t640;
  uint8_t t641;
  uint8_t t642;
  int64_t t643;
  int64_t t644;
  uint8_t t645;
  uint8_t t646;
  int64_t t647;
  int64_t t648;
  uint8_t t649;
  int64_t t650;
  int64_t t651;
  gg_slice t652;
  int64_t t653;
  int64_t t654;
  int64_t t655;
  int64_t t656;
  int64_t t657;
  int64_t t658;
  int64_t t659;
  int64_t t660;
  int64_t t661;
  int64_t t662;
  int64_t t663;
  gg_slice t664;
  int64_t t665;
  int64_t t666;
  int64_t t667;
  float t668;
  int64_t t669;
  int64_t t670;
  int64_t t671;
  int64_t t672;
  gg_type_10 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  float v17;
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
  float v35;
  int64_t v36;
  int64_t v37;
  float v38;
  int64_t v39;
  int64_t v40;
  float v41;
  float v42;
  int64_t v43;
  int64_t v44;
  float v45;
  int64_t v46;
  float v47;
  int64_t v48;
  int64_t v49;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1 = ((int64_t)UINT64_C(2));
  t2 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t1, ((int64_t)UINT64_C(7)));
  t3 = gg_view((((*t2)).f0).data, 4);
  t4 = ((int64_t)UINT64_C(0));
  t5 = ((int64_t *)t3.data) + gg_index(t4, t3.len);
  v8 = ((int64_t)((*t5)));
  /* FG_DIRECT_SLICE_INDEX */
  t6 = ((int64_t)UINT64_C(2));
  t7 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t6, ((int64_t)UINT64_C(7)));
  t8 = gg_view((((*t7)).f0).data, 4);
  t9 = ((int64_t)UINT64_C(1));
  t10 = ((int64_t *)t8.data) + gg_index(t9, t8.len);
  v9 = ((int64_t)((*t10)));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = ((int64_t)UINT64_C(2));
  t12 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t11, ((int64_t)UINT64_C(7)));
  t13 = gg_view((((*t12)).f0).data, 4);
  t14 = ((int64_t)UINT64_C(2));
  t15 = ((int64_t *)t13.data) + gg_index(t14, t13.len);
  v10 = ((int64_t)((*t15)));
  /* FG_DIRECT_SLICE_INDEX */
  t16 = ((int64_t)UINT64_C(2));
  t17 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t16, ((int64_t)UINT64_C(7)));
  t18 = gg_view((((*t17)).f0).data, 4);
  t19 = ((int64_t)UINT64_C(3));
  t20 = ((int64_t *)t18.data) + gg_index(t19, t18.len);
  v11 = ((int64_t)((*t20)));
  v12 = ((int64_t)(((*(v0))).f9));
  t21 = ((int64_t)(v8));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) > (t22));
  t24 = t23;
  if (t24) {
  t25 = ((int64_t)(v9));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t25) > (t26));
  }
  t27 = t24;
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(v10));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t29) >= (t30));
  }
  t31 = t28;
  t32 = t31;
  if (t32) {
  t33 = ((int64_t)(v11));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((t33) > (t34));
  }
  t35 = t32;
  t36 = t35;
  if (t36) {
  t37 = ((int64_t)(v12));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t36 = ((t37) >= (t38));
  }
  t39 = t36;
  t40 = t39;
  if (t40) {
  t41 = ((int64_t)(((*(v0))).f7));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t41) > (t42));
  }
  t43 = t40;
  t44 = t43;
  if (t44) {
  t45 = ((int64_t)(((*(v0))).f6));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = ((t45) >= (t46));
  }
  t47 = t44;
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(((*(v0))).f6));
  t50 = ((int64_t)(((*(v0))).f7));
  t48 = ((t49) < (t50));
  }
  if (!(t48)) gg_fail("assertion failed");
  t51 = ((int64_t)((((*(v0))).f5).f2));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t51) == (t52)))) gg_fail("assertion failed");
  t54 = ((int64_t)UINT64_C(6));
  t53 = 0;
label0:;
  if (t53 >= t54) goto label2;
  v13 = t53;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t55 = v13;
  t56 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t55, ((int64_t)UINT64_C(7)));
  t57 = ((int64_t)(((*t56)).f2));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t57) == (t58)))) gg_fail("assertion failed");
  }
label1:;
  t53++; goto label0;
label2:;
  /* FG_DIRECT_SLICE_INDEX */
  t59 = ((int64_t)UINT64_C(0));
  t60 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t59, ((int64_t)UINT64_C(7)));
  t61 = gg_view((((*t60)).f0).data, 4);
  t62 = ((int64_t)UINT64_C(0));
  t63 = ((int64_t *)t61.data) + gg_index(t62, t61.len);
  t64 = ((int64_t)((*t63)));
  t65 = ((int64_t)(v8));
  t66 = ((t64) == (t65));
  t67 = t66;
  if (t67) {
  /* FG_DIRECT_SLICE_INDEX */
  t68 = ((int64_t)UINT64_C(1));
  t69 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t68, ((int64_t)UINT64_C(7)));
  t70 = gg_view((((*t69)).f0).data, 4);
  t71 = ((int64_t)UINT64_C(0));
  t72 = ((int64_t *)t70.data) + gg_index(t71, t70.len);
  t73 = ((int64_t)((*t72)));
  t74 = ((int64_t)(v8));
  t67 = ((t73) == (t74));
  }
  t75 = t67;
  t76 = t75;
  if (t76) {
  /* FG_DIRECT_SLICE_INDEX */
  t77 = ((int64_t)UINT64_C(0));
  t78 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t77, ((int64_t)UINT64_C(7)));
  t79 = gg_view((((*t78)).f0).data, 4);
  t80 = ((int64_t)UINT64_C(1));
  t81 = ((int64_t *)t79.data) + gg_index(t80, t79.len);
  t82 = ((int64_t)((*t81)));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = ((t82) > (t83));
  }
  t84 = t76;
  t85 = t84;
  if (t85) {
  /* FG_DIRECT_SLICE_INDEX */
  t86 = ((int64_t)UINT64_C(1));
  t87 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t86, ((int64_t)UINT64_C(7)));
  t88 = gg_view((((*t87)).f0).data, 4);
  t89 = ((int64_t)UINT64_C(1));
  t90 = ((int64_t *)t88.data) + gg_index(t89, t88.len);
  t91 = ((int64_t)((*t90)));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((t91) > (t92));
  }
  t93 = t85;
  t94 = t93;
  if (t94) {
  t95 = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t96 = ((int64_t)UINT64_C(0));
  t97 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t96, ((int64_t)UINT64_C(7)));
  t98 = gg_view((((*t97)).f0).data, 4);
  t99 = ((int64_t)UINT64_C(1));
  t100 = ((int64_t *)t98.data) + gg_index(t99, t98.len);
  t101 = ((int64_t)((*t100)));
  t102 = ((int64_t)(((int64_t)gg_mod(t95, t101))));
  t103 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((t102) == (t103));
  }
  t104 = t94;
  t105 = t104;
  if (t105) {
  t106 = ((int64_t)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(1));
  t108 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t107, ((int64_t)UINT64_C(7)));
  t109 = gg_view((((*t108)).f0).data, 4);
  t110 = ((int64_t)UINT64_C(1));
  t111 = ((int64_t *)t109.data) + gg_index(t110, t109.len);
  t112 = ((int64_t)((*t111)));
  t113 = ((int64_t)(((int64_t)gg_mod(t106, t112))));
  t114 = ((int64_t)(((int64_t)UINT64_C(0))));
  t105 = ((t113) == (t114));
  }
  t115 = t105;
  t116 = t115;
  if (t116) {
  /* FG_DIRECT_SLICE_INDEX */
  t117 = ((int64_t)UINT64_C(0));
  t118 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t117, ((int64_t)UINT64_C(7)));
  t119 = gg_view((((*t118)).f0).data, 4);
  t120 = ((int64_t)UINT64_C(3));
  t121 = ((int64_t *)t119.data) + gg_index(t120, t119.len);
  t122 = ((int64_t)((*t121)));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((t122) > (t123));
  }
  t124 = t116;
  t125 = t124;
  if (t125) {
  /* FG_DIRECT_SLICE_INDEX */
  t126 = ((int64_t)UINT64_C(1));
  t127 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t126, ((int64_t)UINT64_C(7)));
  t128 = gg_view((((*t127)).f0).data, 4);
  t129 = ((int64_t)UINT64_C(3));
  t130 = ((int64_t *)t128.data) + gg_index(t129, t128.len);
  t131 = ((int64_t)((*t130)));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t125 = ((t131) > (t132));
  }
  t133 = t125;
  t134 = t133;
  if (t134) {
  t135 = ((int64_t)(v11));
  /* FG_DIRECT_SLICE_INDEX */
  t136 = ((int64_t)UINT64_C(0));
  t137 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t136, ((int64_t)UINT64_C(7)));
  t138 = gg_view((((*t137)).f0).data, 4);
  t139 = ((int64_t)UINT64_C(3));
  t140 = ((int64_t *)t138.data) + gg_index(t139, t138.len);
  t141 = ((int64_t)((*t140)));
  t142 = ((int64_t)(((int64_t)gg_mod(t135, t141))));
  t143 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = ((t142) == (t143));
  }
  t144 = t134;
  t145 = t144;
  if (t145) {
  t146 = ((int64_t)(v11));
  /* FG_DIRECT_SLICE_INDEX */
  t147 = ((int64_t)UINT64_C(1));
  t148 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t147, ((int64_t)UINT64_C(7)));
  t149 = gg_view((((*t148)).f0).data, 4);
  t150 = ((int64_t)UINT64_C(3));
  t151 = ((int64_t *)t149.data) + gg_index(t150, t149.len);
  t152 = ((int64_t)((*t151)));
  t153 = ((int64_t)(((int64_t)gg_mod(t146, t152))));
  t154 = ((int64_t)(((int64_t)UINT64_C(0))));
  t145 = ((t153) == (t154));
  }
  if (!(t145)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t155 = ((int64_t)UINT64_C(3));
  t156 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t155, ((int64_t)UINT64_C(7)));
  t157 = gg_view((((*t156)).f0).data, 4);
  t158 = ((int64_t)UINT64_C(0));
  t159 = ((int64_t *)t157.data) + gg_index(t158, t157.len);
  t160 = ((int64_t)((*t159)));
  t161 = ((int64_t)(((int64_t)UINT64_C(1))));
  t162 = ((t160) == (t161));
  t163 = t162;
  if (!t163) {
  /* FG_DIRECT_SLICE_INDEX */
  t164 = ((int64_t)UINT64_C(3));
  t165 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t164, ((int64_t)UINT64_C(7)));
  t166 = gg_view((((*t165)).f0).data, 4);
  t167 = ((int64_t)UINT64_C(0));
  t168 = ((int64_t *)t166.data) + gg_index(t167, t166.len);
  t169 = ((int64_t)((*t168)));
  t170 = ((int64_t)(v8));
  t163 = ((t169) == (t170));
  }
  t171 = t163;
  t172 = t171;
  if (t172) {
  /* FG_DIRECT_SLICE_INDEX */
  t173 = ((int64_t)UINT64_C(4));
  t174 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t173, ((int64_t)UINT64_C(7)));
  t175 = gg_view((((*t174)).f0).data, 4);
  t176 = ((int64_t)UINT64_C(0));
  t177 = ((int64_t *)t175.data) + gg_index(t176, t175.len);
  t178 = ((int64_t)((*t177)));
  t179 = ((int64_t)(((int64_t)UINT64_C(1))));
  t172 = ((t178) == (t179));
  }
  if (!(t172)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t180 = ((int64_t)UINT64_C(5));
  t181 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t180, ((int64_t)UINT64_C(7)));
  t182 = gg_view((((*t181)).f0).data, 4);
  t183 = ((int64_t)UINT64_C(0));
  t184 = ((int64_t *)t182.data) + gg_index(t183, t182.len);
  t185 = ((int64_t)((*t184)));
  t186 = ((int64_t)(v8));
  t187 = ((t185) == (t186));
  t188 = t187;
  if (t188) {
  /* FG_DIRECT_SLICE_INDEX */
  t189 = ((int64_t)UINT64_C(5));
  t190 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t189, ((int64_t)UINT64_C(7)));
  t191 = gg_view((((*t190)).f0).data, 4);
  t192 = ((int64_t)UINT64_C(1));
  t193 = ((int64_t *)t191.data) + gg_index(t192, t191.len);
  t194 = ((int64_t)((*t193)));
  t195 = ((int64_t)(v8));
  t188 = ((t194) == (t195));
  }
  t196 = t188;
  t197 = t196;
  if (t197) {
  /* FG_DIRECT_SLICE_INDEX */
  t198 = ((int64_t)UINT64_C(5));
  t199 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t198, ((int64_t)UINT64_C(7)));
  t200 = gg_view((((*t199)).f0).data, 4);
  t201 = ((int64_t)UINT64_C(2));
  t202 = ((int64_t *)t200.data) + gg_index(t201, t200.len);
  t203 = ((int64_t)((*t202)));
  t204 = ((int64_t)(v9));
  t197 = ((t203) == (t204));
  }
  t205 = t197;
  t206 = t205;
  if (t206) {
  /* FG_DIRECT_SLICE_INDEX */
  t207 = ((int64_t)UINT64_C(5));
  t208 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t207, ((int64_t)UINT64_C(7)));
  t209 = gg_view((((*t208)).f0).data, 4);
  t210 = ((int64_t)UINT64_C(3));
  t211 = ((int64_t *)t209.data) + gg_index(t210, t209.len);
  t212 = ((int64_t)((*t211)));
  t213 = ((int64_t)(v11));
  t206 = ((t212) >= (t213));
  }
  if (!(t206)) gg_fail("assertion failed");
  t214 = ((int64_t)(v8));
  t215 = ((int64_t)(v8));
  v14 = gg_function_5(t214, t215, base + 0);
  t216 = ((int64_t)(v8));
  t217 = ((int64_t)(v9));
  t218 = gg_function_5(t216, t217, base + 0);
  t219 = ((int64_t)(v10));
  t220 = gg_function_5(t218, t219, base + 0);
  t221 = ((int64_t)(v11));
  v15 = gg_function_5(t220, t221, base + 0);
  t222 = ((int64_t)(v14));
  t223 = ((int64_t)(v9));
  t224 = gg_function_5(t222, t223, base + 0);
  t225 = ((int64_t)(v11));
  v16 = gg_function_5(t224, t225, base + 0);
  t226 = ((int64_t)(v15));
  t227 = ((int64_t)(v16));
  t228 = ((int64_t)(v12));
  t229 = gg_function_5(t227, t228, base + 0);
  t230 = gg_function_6(t226, t229, base + 0);
  t231 = ((int64_t)(((int64_t)UINT64_C(4))));
  t232 = gg_function_5(t230, t231, base + 0);
  t233 = ((int64_t)(t232));
  t234 = ((int64_t)((v7).len));
  if (!(((t233) <= (t234)))) gg_fail("assertion failed");
  t235 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t236 = ((float)(((float)(v8))));
  t237 = sqrtf(t236);
  t238 = ((float)(t237));
  v17 = ((float)(((t235) / (t238))));
  v18 = ((int64_t)(((*(v0))).f6));
label4:;
  t239 = ((int64_t)(v18));
  t240 = ((int64_t)(v9));
  t241 = ((int64_t)(v11));
  t242 = gg_function_5(t240, t241, base + 0);
  t243 = ((int64_t)(t242));
  if (!(((t239) < (t243)))) goto label5;
  {
  t244 = ((int64_t)(v18));
  t245 = ((int64_t)(v9));
  v19 = ((int64_t)(((int64_t)gg_mod(t244, t245))));
  t246 = ((int64_t)(v18));
  t247 = ((int64_t)(v9));
  v20 = ((int64_t)(((int64_t)gg_div(t246, t247))));
  t248 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t249 = ((int64_t)UINT64_C(0));
  t250 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t249, ((int64_t)UINT64_C(7)));
  t251 = gg_view((((*t250)).f0).data, 4);
  t252 = ((int64_t)UINT64_C(1));
  t253 = ((int64_t *)t251.data) + gg_index(t252, t251.len);
  t254 = ((int64_t)((*t253)));
  v21 = ((int64_t)(((int64_t)gg_mod(t248, t254))));
  t255 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t256 = ((int64_t)UINT64_C(1));
  t257 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t256, ((int64_t)UINT64_C(7)));
  t258 = gg_view((((*t257)).f0).data, 4);
  t259 = ((int64_t)UINT64_C(1));
  t260 = ((int64_t *)t258.data) + gg_index(t259, t258.len);
  t261 = ((int64_t)((*t260)));
  v22 = ((int64_t)(((int64_t)gg_mod(t255, t261))));
  t262 = ((int64_t)(v20));
  t263 = ((int64_t)(v11));
  /* FG_DIRECT_SLICE_INDEX */
  t264 = ((int64_t)UINT64_C(0));
  t265 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t264, ((int64_t)UINT64_C(7)));
  t266 = gg_view((((*t265)).f0).data, 4);
  t267 = ((int64_t)UINT64_C(3));
  t268 = ((int64_t *)t266.data) + gg_index(t267, t266.len);
  t269 = ((int64_t)((*t268)));
  t270 = ((int64_t)(((int64_t)gg_div(t263, t269))));
  v23 = ((int64_t)(((int64_t)gg_div(t262, t270))));
  t271 = ((int64_t)(v20));
  t272 = ((int64_t)(v11));
  /* FG_DIRECT_SLICE_INDEX */
  t273 = ((int64_t)UINT64_C(1));
  t274 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t273, ((int64_t)UINT64_C(7)));
  t275 = gg_view((((*t274)).f0).data, 4);
  t276 = ((int64_t)UINT64_C(3));
  t277 = ((int64_t *)t275.data) + gg_index(t276, t275.len);
  t278 = ((int64_t)((*t277)));
  t279 = ((int64_t)(((int64_t)gg_div(t272, t278))));
  v24 = ((int64_t)(((int64_t)gg_div(t271, t279))));
  t280 = ((int64_t)(v15));
  t281 = ((int64_t)(v18));
  t282 = ((int64_t)(v14));
  t283 = ((int64_t)(((int64_t)((uint64_t)(t281) * (uint64_t)(t282)))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t280) + (uint64_t)(t283)))));
  t284 = ((int64_t)(v10));
  t285 = ((int64_t)(((int64_t)UINT64_C(0))));
  t286 = ((t284) > (t285));
  t287 = t286;
  if (!t287) {
  t288 = ((int64_t)(v12));
  t289 = ((int64_t)(((int64_t)UINT64_C(1))));
  t287 = ((t288) == (t289));
  }
  t290 = t287;
  if (t290) {
  {
  t292 = v8;
  t291 = 0;
label6:;
  if (t291 >= t292) goto label8;
  v26 = t291;
  {
  t294 = v8;
  t293 = 0;
label9:;
  if (t293 >= t294) goto label11;
  v27 = t293;
  {
  t295 = ((int64_t)(v27));
  /* FG_DIRECT_SLICE_INDEX */
  t296 = ((int64_t)UINT64_C(5));
  t297 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t296, ((int64_t)UINT64_C(7)));
  t298 = gg_view((((*t297)).f1).data, 4);
  t299 = ((int64_t)UINT64_C(0));
  t300 = ((int64_t *)t298.data) + gg_index(t299, t298.len);
  t301 = ((int64_t)((*t300)));
  t302 = ((int64_t)(((int64_t)((uint64_t)(t295) * (uint64_t)(t301)))));
  t303 = ((int64_t)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t304 = ((int64_t)UINT64_C(5));
  t305 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t304, ((int64_t)UINT64_C(7)));
  t306 = gg_view((((*t305)).f1).data, 4);
  t307 = ((int64_t)UINT64_C(1));
  t308 = ((int64_t *)t306.data) + gg_index(t307, t306.len);
  t309 = ((int64_t)((*t308)));
  t310 = ((int64_t)(((int64_t)((uint64_t)(t303) * (uint64_t)(t309)))));
  t311 = ((int64_t)(((int64_t)((uint64_t)(t302) + (uint64_t)(t310)))));
  t312 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t313 = ((int64_t)UINT64_C(5));
  t314 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t313, ((int64_t)UINT64_C(7)));
  t315 = gg_view((((*t314)).f1).data, 4);
  t316 = ((int64_t)UINT64_C(2));
  t317 = ((int64_t *)t315.data) + gg_index(t316, t315.len);
  t318 = ((int64_t)((*t317)));
  t319 = ((int64_t)(((int64_t)((uint64_t)(t312) * (uint64_t)(t318)))));
  t320 = ((int64_t)(((int64_t)((uint64_t)(t311) + (uint64_t)(t319)))));
  t321 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t322 = ((int64_t)UINT64_C(5));
  t323 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t322, ((int64_t)UINT64_C(7)));
  t324 = gg_view((((*t323)).f1).data, 4);
  t325 = ((int64_t)UINT64_C(3));
  t326 = ((int64_t *)t324.data) + gg_index(t325, t324.len);
  t327 = ((int64_t)((*t326)));
  t328 = ((int64_t)(((int64_t)((uint64_t)(t321) * (uint64_t)(t327)))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t320) + (uint64_t)(t328)))));
  t329 = v7;
  t330 = ((int64_t)(v25));
  t331 = ((int64_t)(v26));
  t332 = ((int64_t)(v8));
  t333 = ((int64_t)(((int64_t)((uint64_t)(t331) * (uint64_t)(t332)))));
  t334 = ((int64_t)(((int64_t)((uint64_t)(t330) + (uint64_t)(t333)))));
  t335 = ((int64_t)(v27));
  t336 = ((int64_t)(((int64_t)((uint64_t)(t334) + (uint64_t)(t335)))));
  /* FG_DIRECT_SLICE_INDEX */
  t337 = ((int64_t)UINT64_C(5));
  t338 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t337, ((int64_t)UINT64_C(7)));
  t339 = t338;
  t340 = v6;
  t341 = ((int64_t)(v28));
  t342 = gg_function_29(t339, t340, t341, base + 0);
  gg_function_11(t329, t336, t342, base + 0);
  }
label10:;
  t293++; goto label9;
label11:;
  }
label7:;
  t291++; goto label6;
label8:;
  }
  }
  t344 = v10;
  t343 = 0;
label12:;
  if (t343 >= t344) goto label14;
  v29 = t343;
  {
  t345 = ((int64_t)(v21));
  /* FG_DIRECT_SLICE_INDEX */
  t346 = ((int64_t)UINT64_C(0));
  t347 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t346, ((int64_t)UINT64_C(7)));
  t348 = gg_view((((*t347)).f1).data, 4);
  t349 = ((int64_t)UINT64_C(1));
  t350 = ((int64_t *)t348.data) + gg_index(t349, t348.len);
  t351 = ((int64_t)((*t350)));
  t352 = ((int64_t)(((int64_t)((uint64_t)(t345) * (uint64_t)(t351)))));
  t353 = ((int64_t)(v29));
  /* FG_DIRECT_SLICE_INDEX */
  t354 = ((int64_t)UINT64_C(0));
  t355 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t354, ((int64_t)UINT64_C(7)));
  t356 = gg_view((((*t355)).f1).data, 4);
  t357 = ((int64_t)UINT64_C(2));
  t358 = ((int64_t *)t356.data) + gg_index(t357, t356.len);
  t359 = ((int64_t)((*t358)));
  t360 = ((int64_t)(((int64_t)((uint64_t)(t353) * (uint64_t)(t359)))));
  t361 = ((int64_t)(((int64_t)((uint64_t)(t352) + (uint64_t)(t360)))));
  t362 = ((int64_t)(v23));
  /* FG_DIRECT_SLICE_INDEX */
  t363 = ((int64_t)UINT64_C(0));
  t364 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t363, ((int64_t)UINT64_C(7)));
  t365 = gg_view((((*t364)).f1).data, 4);
  t366 = ((int64_t)UINT64_C(3));
  t367 = ((int64_t *)t365.data) + gg_index(t366, t365.len);
  t368 = ((int64_t)((*t367)));
  t369 = ((int64_t)(((int64_t)((uint64_t)(t362) * (uint64_t)(t368)))));
  v30 = ((int64_t)(((int64_t)((uint64_t)(t361) + (uint64_t)(t369)))));
  t370 = ((int64_t)(v22));
  /* FG_DIRECT_SLICE_INDEX */
  t371 = ((int64_t)UINT64_C(1));
  t372 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t371, ((int64_t)UINT64_C(7)));
  t373 = gg_view((((*t372)).f1).data, 4);
  t374 = ((int64_t)UINT64_C(1));
  t375 = ((int64_t *)t373.data) + gg_index(t374, t373.len);
  t376 = ((int64_t)((*t375)));
  t377 = ((int64_t)(((int64_t)((uint64_t)(t370) * (uint64_t)(t376)))));
  t378 = ((int64_t)(v29));
  /* FG_DIRECT_SLICE_INDEX */
  t379 = ((int64_t)UINT64_C(1));
  t380 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t379, ((int64_t)UINT64_C(7)));
  t381 = gg_view((((*t380)).f1).data, 4);
  t382 = ((int64_t)UINT64_C(2));
  t383 = ((int64_t *)t381.data) + gg_index(t382, t381.len);
  t384 = ((int64_t)((*t383)));
  t385 = ((int64_t)(((int64_t)((uint64_t)(t378) * (uint64_t)(t384)))));
  t386 = ((int64_t)(((int64_t)((uint64_t)(t377) + (uint64_t)(t385)))));
  t387 = ((int64_t)(v24));
  /* FG_DIRECT_SLICE_INDEX */
  t388 = ((int64_t)UINT64_C(1));
  t389 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t388, ((int64_t)UINT64_C(7)));
  t390 = gg_view((((*t389)).f1).data, 4);
  t391 = ((int64_t)UINT64_C(3));
  t392 = ((int64_t *)t390.data) + gg_index(t391, t390.len);
  t393 = ((int64_t)((*t392)));
  t394 = ((int64_t)(((int64_t)((uint64_t)(t387) * (uint64_t)(t393)))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t386) + (uint64_t)(t394)))));
  t395 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t396 = ((int64_t)UINT64_C(2));
  t397 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t396, ((int64_t)UINT64_C(7)));
  t398 = gg_view((((*t397)).f1).data, 4);
  t399 = ((int64_t)UINT64_C(1));
  t400 = ((int64_t *)t398.data) + gg_index(t399, t398.len);
  t401 = ((int64_t)((*t400)));
  t402 = ((int64_t)(((int64_t)((uint64_t)(t395) * (uint64_t)(t401)))));
  t403 = ((int64_t)(v29));
  /* FG_DIRECT_SLICE_INDEX */
  t404 = ((int64_t)UINT64_C(2));
  t405 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t404, ((int64_t)UINT64_C(7)));
  t406 = gg_view((((*t405)).f1).data, 4);
  t407 = ((int64_t)UINT64_C(2));
  t408 = ((int64_t *)t406.data) + gg_index(t407, t406.len);
  t409 = ((int64_t)((*t408)));
  t410 = ((int64_t)(((int64_t)((uint64_t)(t403) * (uint64_t)(t409)))));
  t411 = ((int64_t)(((int64_t)((uint64_t)(t402) + (uint64_t)(t410)))));
  t412 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t413 = ((int64_t)UINT64_C(2));
  t414 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t413, ((int64_t)UINT64_C(7)));
  t415 = gg_view((((*t414)).f1).data, 4);
  t416 = ((int64_t)UINT64_C(3));
  t417 = ((int64_t *)t415.data) + gg_index(t416, t415.len);
  t418 = ((int64_t)((*t417)));
  t419 = ((int64_t)(((int64_t)((uint64_t)(t412) * (uint64_t)(t418)))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t411) + (uint64_t)(t419)))));
  t420 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t421 = ((int64_t)UINT64_C(3));
  t422 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t421, ((int64_t)UINT64_C(7)));
  t423 = gg_view((((*t422)).f1).data, 4);
  t424 = ((int64_t)UINT64_C(1));
  t425 = ((int64_t *)t423.data) + gg_index(t424, t423.len);
  t426 = ((int64_t)((*t425)));
  t427 = ((int64_t)(((int64_t)((uint64_t)(t420) * (uint64_t)(t426)))));
  t428 = ((int64_t)(v29));
  /* FG_DIRECT_SLICE_INDEX */
  t429 = ((int64_t)UINT64_C(3));
  t430 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t429, ((int64_t)UINT64_C(7)));
  t431 = gg_view((((*t430)).f1).data, 4);
  t432 = ((int64_t)UINT64_C(2));
  t433 = ((int64_t *)t431.data) + gg_index(t432, t431.len);
  t434 = ((int64_t)((*t433)));
  t435 = ((int64_t)(((int64_t)((uint64_t)(t428) * (uint64_t)(t434)))));
  t436 = ((int64_t)(((int64_t)((uint64_t)(t427) + (uint64_t)(t435)))));
  t437 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t438 = ((int64_t)UINT64_C(3));
  t439 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t438, ((int64_t)UINT64_C(7)));
  t440 = gg_view((((*t439)).f1).data, 4);
  t441 = ((int64_t)UINT64_C(3));
  t442 = ((int64_t *)t440.data) + gg_index(t441, t440.len);
  t443 = ((int64_t)((*t442)));
  t444 = ((int64_t)(((int64_t)((uint64_t)(t437) * (uint64_t)(t443)))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t436) + (uint64_t)(t444)))));
  t445 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t446 = ((int64_t)UINT64_C(4));
  t447 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t446, ((int64_t)UINT64_C(7)));
  t448 = gg_view((((*t447)).f1).data, 4);
  t449 = ((int64_t)UINT64_C(1));
  t450 = ((int64_t *)t448.data) + gg_index(t449, t448.len);
  t451 = ((int64_t)((*t450)));
  t452 = ((int64_t)(((int64_t)((uint64_t)(t445) * (uint64_t)(t451)))));
  t453 = ((int64_t)(v29));
  /* FG_DIRECT_SLICE_INDEX */
  t454 = ((int64_t)UINT64_C(4));
  t455 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t454, ((int64_t)UINT64_C(7)));
  t456 = gg_view((((*t455)).f1).data, 4);
  t457 = ((int64_t)UINT64_C(2));
  t458 = ((int64_t *)t456.data) + gg_index(t457, t456.len);
  t459 = ((int64_t)((*t458)));
  t460 = ((int64_t)(((int64_t)((uint64_t)(t453) * (uint64_t)(t459)))));
  t461 = ((int64_t)(((int64_t)((uint64_t)(t452) + (uint64_t)(t460)))));
  t462 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t463 = ((int64_t)UINT64_C(4));
  t464 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t463, ((int64_t)UINT64_C(7)));
  t465 = gg_view((((*t464)).f1).data, 4);
  t466 = ((int64_t)UINT64_C(3));
  t467 = ((int64_t *)t465.data) + gg_index(t466, t465.len);
  t468 = ((int64_t)((*t467)));
  t469 = ((int64_t)(((int64_t)((uint64_t)(t462) * (uint64_t)(t468)))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t461) + (uint64_t)(t469)))));
  /* FG_DIRECT_SLICE_INDEX */
  t470 = ((int64_t)UINT64_C(4));
  t471 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t470, ((int64_t)UINT64_C(7)));
  t472 = t471;
  t473 = v5;
  t474 = ((int64_t)(v34));
  v35 = gg_function_29(t472, t473, t474, base + 0);
  t476 = v8;
  t475 = 0;
label15:;
  if (t475 >= t476) goto label17;
  v36 = t475;
  {
  t478 = v8;
  t477 = 0;
label18:;
  if (t477 >= t478) goto label20;
  v37 = t477;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t479 = ((int64_t)UINT64_C(3));
  t480 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t479, ((int64_t)UINT64_C(7)));
  t481 = t480;
  t482 = v4;
  t483 = ((int64_t)(v33));
  /* FG_DIRECT_SLICE_INDEX */
  t485 = ((int64_t)UINT64_C(3));
  t486 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t485, ((int64_t)UINT64_C(7)));
  t487 = gg_view((((*t486)).f0).data, 4);
  t488 = ((int64_t)UINT64_C(0));
  t489 = ((int64_t *)t487.data) + gg_index(t488, t487.len);
  t490 = ((int64_t)((*t489)));
  t491 = ((int64_t)(((int64_t)UINT64_C(1))));
  t492 = ((t490) == (t491));
  if (t492) {
  {
  t484 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t493 = ((int64_t)(v37));
  /* FG_DIRECT_SLICE_INDEX */
  t494 = ((int64_t)UINT64_C(3));
  t495 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t494, ((int64_t)UINT64_C(7)));
  t496 = gg_view((((*t495)).f1).data, 4);
  t497 = ((int64_t)UINT64_C(0));
  t498 = ((int64_t *)t496.data) + gg_index(t497, t496.len);
  t499 = ((int64_t)((*t498)));
  t484 = ((int64_t)(((int64_t)((uint64_t)(t493) * (uint64_t)(t499)))));
  }
  }
  t500 = ((int64_t)(t484));
  t501 = ((int64_t)(((int64_t)((uint64_t)(t483) + (uint64_t)(t500)))));
  t502 = gg_function_29(t481, t482, t501, base + 0);
  t503 = expf(t502);
  v38 = ((float)(t503));
  t504 = ((int64_t)(v25));
  t505 = ((int64_t)(v36));
  t506 = ((int64_t)(v8));
  t507 = ((int64_t)(((int64_t)((uint64_t)(t505) * (uint64_t)(t506)))));
  t508 = ((int64_t)(((int64_t)((uint64_t)(t504) + (uint64_t)(t507)))));
  t509 = ((int64_t)(v37));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t508) + (uint64_t)(t509)))));
  t510 = v7;
  t511 = ((int64_t)(v39));
  t512 = v7;
  t513 = ((int64_t)(v39));
  t514 = gg_function_20(t512, t513, base + 0);
  t515 = ((float)(t514));
  t516 = ((float)(v38));
  t517 = ((float)(((t515) * (t516))));
  gg_function_11(t510, t511, t517, base + 0);
  }
label19:;
  t477++; goto label18;
label20:;
  }
label16:;
  t475++; goto label15;
label17:;
  t519 = v8;
  t518 = 0;
label21:;
  if (t518 >= t519) goto label23;
  v40 = t518;
  {
  t520 = &(((*(v0))).f5);
  t521 = v7;
  t522 = ((int64_t)(v25));
  t523 = ((int64_t)(v40));
  t524 = ((int64_t)(v8));
  t525 = ((int64_t)(((int64_t)((uint64_t)(t523) * (uint64_t)(t524)))));
  t526 = ((int64_t)(((int64_t)((uint64_t)(t522) + (uint64_t)(t525)))));
  t527 = ((int64_t)(((int64_t)UINT64_C(4))));
  t528 = ((int64_t)(((int64_t)((uint64_t)(t526) * (uint64_t)(t527)))));
  t529 = ((int64_t)(((int64_t)UINT64_C(4))));
  /* FG_DIRECT_SLICE_INDEX */
  t530 = ((int64_t)UINT64_C(1));
  t531 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t530, ((int64_t)UINT64_C(7)));
  t532 = t531;
  t533 = v2;
  t534 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t535 = ((int64_t)UINT64_C(1));
  t536 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t535, ((int64_t)UINT64_C(7)));
  t537 = gg_view((((*t536)).f1).data, 4);
  t538 = ((int64_t)UINT64_C(0));
  t539 = ((int64_t *)t537.data) + gg_index(t538, t537.len);
  t540 = ((int64_t)((*t539)));
  t541 = ((int64_t)(v8));
  v41 = gg_function_21(t520, t521, t528, t529, t532, t533, t534, t540, t541, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t542 = ((int64_t)UINT64_C(2));
  t543 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t542, ((int64_t)UINT64_C(7)));
  t544 = t543;
  t545 = v3;
  t546 = ((int64_t)(v32));
  t547 = ((int64_t)(v40));
  /* FG_DIRECT_SLICE_INDEX */
  t548 = ((int64_t)UINT64_C(2));
  t549 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t548, ((int64_t)UINT64_C(7)));
  t550 = gg_view((((*t549)).f1).data, 4);
  t551 = ((int64_t)UINT64_C(0));
  t552 = ((int64_t *)t550.data) + gg_index(t551, t550.len);
  t553 = ((int64_t)((*t552)));
  t554 = ((int64_t)(((int64_t)((uint64_t)(t547) * (uint64_t)(t553)))));
  t555 = ((int64_t)(((int64_t)((uint64_t)(t546) + (uint64_t)(t554)))));
  t556 = gg_function_29(t544, t545, t555, base + 0);
  t557 = ((float)(t556));
  t558 = ((float)(v41));
  t559 = ((float)(((t557) - (t558))));
  t560 = ((float)(v35));
  v42 = ((float)(((t559) * (t560))));
  t562 = v8;
  t561 = 0;
label24:;
  if (t561 >= t562) goto label26;
  v43 = t561;
  {
  t563 = ((int64_t)(v25));
  t564 = ((int64_t)(v40));
  t565 = ((int64_t)(v8));
  t566 = ((int64_t)(((int64_t)((uint64_t)(t564) * (uint64_t)(t565)))));
  t567 = ((int64_t)(((int64_t)((uint64_t)(t563) + (uint64_t)(t566)))));
  t568 = ((int64_t)(v43));
  v44 = ((int64_t)(((int64_t)((uint64_t)(t567) + (uint64_t)(t568)))));
  /* FG_DIRECT_SLICE_INDEX */
  t569 = ((int64_t)UINT64_C(1));
  t570 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t569, ((int64_t)UINT64_C(7)));
  t571 = t570;
  t572 = v2;
  t573 = ((int64_t)(v31));
  t574 = ((int64_t)(v43));
  /* FG_DIRECT_SLICE_INDEX */
  t575 = ((int64_t)UINT64_C(1));
  t576 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t575, ((int64_t)UINT64_C(7)));
  t577 = gg_view((((*t576)).f1).data, 4);
  t578 = ((int64_t)UINT64_C(0));
  t579 = ((int64_t *)t577.data) + gg_index(t578, t577.len);
  t580 = ((int64_t)((*t579)));
  t581 = ((int64_t)(((int64_t)((uint64_t)(t574) * (uint64_t)(t580)))));
  t582 = ((int64_t)(((int64_t)((uint64_t)(t573) + (uint64_t)(t581)))));
  v45 = gg_function_29(t571, t572, t582, base + 0);
  t583 = v7;
  t584 = ((int64_t)(v44));
  t585 = ((float)(v45));
  t586 = ((float)(v42));
  t587 = v7;
  t588 = ((int64_t)(v44));
  t589 = gg_function_20(t587, t588, base + 0);
  t590 = fmaf(t585, t586, t589);
  t591 = ((float)(t590));
  gg_function_11(t583, t584, t591, base + 0);
  }
label25:;
  t561++; goto label24;
label26:;
  }
label22:;
  t518++; goto label21;
label23:;
  t593 = v8;
  t592 = 0;
label27:;
  if (t592 >= t593) goto label29;
  v46 = t592;
  {
  t594 = &(((*(v0))).f5);
  t595 = v7;
  t596 = ((int64_t)(v25));
  t597 = ((int64_t)(v46));
  t598 = ((int64_t)(v8));
  t599 = ((int64_t)(((int64_t)((uint64_t)(t597) * (uint64_t)(t598)))));
  t600 = ((int64_t)(((int64_t)((uint64_t)(t596) + (uint64_t)(t599)))));
  t601 = ((int64_t)(((int64_t)UINT64_C(4))));
  t602 = ((int64_t)(((int64_t)((uint64_t)(t600) * (uint64_t)(t601)))));
  t603 = ((int64_t)(((int64_t)UINT64_C(4))));
  /* FG_DIRECT_SLICE_INDEX */
  t604 = ((int64_t)UINT64_C(0));
  t605 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t604, ((int64_t)UINT64_C(7)));
  t606 = t605;
  t607 = v1;
  t608 = ((int64_t)(v30));
  /* FG_DIRECT_SLICE_INDEX */
  t609 = ((int64_t)UINT64_C(0));
  t610 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t609, ((int64_t)UINT64_C(7)));
  t611 = gg_view((((*t610)).f1).data, 4);
  t612 = ((int64_t)UINT64_C(0));
  t613 = ((int64_t *)t611.data) + gg_index(t612, t611.len);
  t614 = ((int64_t)((*t613)));
  t615 = ((int64_t)(v8));
  v47 = gg_function_21(t594, t595, t602, t603, t606, t607, t608, t614, t615, base + 0);
  t616 = v7;
  t617 = ((int64_t)(v20));
  t618 = ((int64_t)(v10));
  t619 = ((int64_t)(((int64_t)((uint64_t)(t617) * (uint64_t)(t618)))));
  t620 = ((int64_t)(v9));
  t621 = ((int64_t)(((int64_t)((uint64_t)(t619) * (uint64_t)(t620)))));
  t622 = ((int64_t)(v29));
  t623 = ((int64_t)(v9));
  t624 = ((int64_t)(((int64_t)((uint64_t)(t622) * (uint64_t)(t623)))));
  t625 = ((int64_t)(((int64_t)((uint64_t)(t621) + (uint64_t)(t624)))));
  t626 = ((int64_t)(v19));
  t627 = ((int64_t)(((int64_t)((uint64_t)(t625) + (uint64_t)(t626)))));
  t628 = ((int64_t)(v8));
  t629 = ((int64_t)(((int64_t)((uint64_t)(t627) * (uint64_t)(t628)))));
  t630 = ((int64_t)(v46));
  t631 = ((int64_t)(((int64_t)((uint64_t)(t629) + (uint64_t)(t630)))));
  t632 = ((float)(v47));
  t633 = ((float)(v17));
  t634 = ((float)(((t632) * (t633))));
  gg_function_11(t616, t631, t634, base + 0);
  }
label28:;
  t592++; goto label27;
label29:;
  t635 = ((int64_t)(v10));
  t636 = ((int64_t)(((int64_t)UINT64_C(1))));
  t637 = ((int64_t)(((int64_t)((uint64_t)(t635) - (uint64_t)(t636)))));
  t638 = ((int64_t)(v29));
  v48 = ((int64_t)(((int64_t)((uint64_t)(t637) - (uint64_t)(t638)))));
  t639 = ((int64_t)(v12));
  t640 = ((int64_t)(((int64_t)UINT64_C(1))));
  t641 = ((t639) > (t640));
  t642 = t641;
  if (t642) {
  t643 = ((int64_t)(v48));
  t644 = ((int64_t)(((int64_t)UINT64_C(0))));
  t642 = ((t643) > (t644));
  }
  t645 = t642;
  t646 = t645;
  if (t646) {
  t647 = ((int64_t)(v48));
  t648 = ((int64_t)(v12));
  t646 = ((t647) < (t648));
  }
  t649 = t646;
  if (t649) {
  {
  t651 = v14;
  t650 = 0;
label30:;
  if (t650 >= t651) goto label32;
  v49 = t650;
  {
  t652 = v7;
  t653 = ((int64_t)(v15));
  t654 = ((int64_t)(v48));
  t655 = ((int64_t)(v16));
  t656 = ((int64_t)(((int64_t)((uint64_t)(t654) * (uint64_t)(t655)))));
  t657 = ((int64_t)(((int64_t)((uint64_t)(t653) + (uint64_t)(t656)))));
  t658 = ((int64_t)(v18));
  t659 = ((int64_t)(v14));
  t660 = ((int64_t)(((int64_t)((uint64_t)(t658) * (uint64_t)(t659)))));
  t661 = ((int64_t)(((int64_t)((uint64_t)(t657) + (uint64_t)(t660)))));
  t662 = ((int64_t)(v49));
  t663 = ((int64_t)(((int64_t)((uint64_t)(t661) + (uint64_t)(t662)))));
  t664 = v7;
  t665 = ((int64_t)(v25));
  t666 = ((int64_t)(v49));
  t667 = ((int64_t)(((int64_t)((uint64_t)(t665) + (uint64_t)(t666)))));
  t668 = gg_function_20(t664, t667, base + 0);
  gg_function_11(t652, t663, t668, base + 0);
  }
label31:;
  t650++; goto label30;
label32:;
  }
  }
  }
label13:;
  t343++; goto label12;
label14:;
  t669 = v18;
  t670 = ((int64_t)(((*(v0))).f7));
  v18 = ((int64_t)((uint64_t)(t669) + (uint64_t)(t670)));
  }
  goto label4;
label5:;
  t671 = ((int64_t)(((int64_t)UINT64_C(86))));
  t672 = ((int64_t)(v15));
  fg143_witness(t671, t672);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_8(gg_slice p0, int64_t p1, int64_t base) {
  /* FG_SHARED_BODY:20 */
  return gg_function_20(p0, p1, base);
}
static float gg_function_14(gg_type_15 p0, gg_slice p1, int64_t p2, int64_t p3, gg_type_15 p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  /* FG_SHARED_BODY:21 */
  return gg_function_21(p0, p1, p2, p3, p4, p5, p6, p7, p8, base);
}
static int64_t gg_function_7(gg_type_15 p0, int64_t base) {
  /* FG_SHARED_BODY:28 */
  return gg_function_28(p0, base);
}
static uint8_t gg_function_2(gg_type_10 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  uint8_t t7;
  uint8_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  gg_type_2 * t17;
  gg_slice t18;
  int64_t t19;
  int64_t * t20;
  int64_t t21;
  gg_type_2 * t22;
  gg_slice t23;
  int64_t t24;
  int64_t * t25;
  int64_t t26;
  gg_type_2 * t27;
  gg_slice t28;
  int64_t t29;
  int64_t * t30;
  int64_t t31;
  gg_type_2 * t32;
  gg_slice t33;
  int64_t t34;
  int64_t * t35;
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
  uint8_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  uint8_t t57;
  int64_t t58;
  int64_t t59;
  uint8_t t60;
  uint8_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  gg_type_15 t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  gg_type_2 * t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t * t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t * t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  gg_type_2 * t93;
  gg_type_15 t94;
  int64_t t95;
  int64_t t96;
  gg_type_2 * t97;
  gg_type_15 t98;
  int64_t t99;
  int64_t t100;
  gg_type_2 * t101;
  gg_type_15 t102;
  int64_t t103;
  int64_t t104;
  gg_type_2 * t105;
  gg_type_15 t106;
  int64_t t107;
  int64_t t108;
  gg_type_2 * t109;
  gg_type_15 t110;
  int64_t t111;
  uint8_t t112;
  uint8_t t113;
  int64_t t114;
  gg_type_2 * t115;
  gg_type_15 t116;
  int64_t t117;
  uint8_t t118;
  int64_t t119;
  gg_type_2 * t120;
  gg_type_15 t121;
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
  gg_type_2 * t138;
  gg_type_15 t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  uint8_t t143;
  uint8_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  uint8_t t154;
  uint8_t t155;
  uint8_t t156;
  int64_t t157;
  int64_t t158;
  uint8_t t159;
  uint8_t t160;
  uint8_t t161;
  uint8_t t162;
  int64_t t163;
  gg_type_2 * t164;
  gg_type_15 t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  gg_type_2 * t170;
  gg_type_15 t171;
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
  int64_t t187;
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
  int64_t t200;
  int64_t t201;
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
  uint8_t t227;
  int64_t t228;
  int64_t t229;
  uint8_t t230;
  gg_slice t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  uint8_t t235;
  gg_slice t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  gg_slice t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  gg_slice t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  float t248;
  uint8_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  gg_type_2 * t257;
  gg_type_15 t258;
  gg_slice t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  gg_type_15 t264;
  gg_slice t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  gg_slice t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  float t279;
  gg_slice t280;
  int64_t t281;
  float t282;
  float t283;
  gg_slice t284;
  int64_t t285;
  int64_t t286;
  int64_t t287;
  float t288;
  gg_slice t289;
  int64_t t290;
  float t291;
  float t292;
  float t293;
  gg_slice t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  float t298;
  gg_slice t299;
  int64_t t300;
  float t301;
  float t302;
  float t303;
  float t304;
  gg_slice t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  gg_type_15 t309;
  gg_slice t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  gg_type_2 * t316;
  gg_type_15 t317;
  gg_slice t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  float t324;
  int64_t t325;
  int64_t t326;
  gg_slice t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  float t331;
  int64_t t332;
  int64_t t333;
  gg_slice t334;
  int64_t t335;
  int64_t t336;
  int64_t t337;
  gg_slice t338;
  int64_t t339;
  int64_t t340;
  int64_t t341;
  float t342;
  float t343;
  float t344;
  uint8_t t345;
  int64_t t346;
  float * t347;
  float t348;
  uint8_t t349;
  gg_slice t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  gg_slice t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  uint8_t t358;
  gg_slice t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  int64_t t372;
  gg_slice t373;
  int64_t t374;
  int64_t t375;
  int64_t t376;
  float t377;
  float t378;
  float t379;
  gg_slice t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  uint8_t t384;
  float t385;
  float t386;
  float t387;
  float t388;
  float t389;
  float t390;
  float t391;
  float t392;
  uint8_t t393;
  int64_t t394;
  int64_t t395;
  uint8_t t396;
  float t397;
  float t398;
  float t399;
  float t400;
  float t401;
  float t402;
  float t403;
  float t404;
  int64_t t405;
  int64_t t406;
  uint8_t t407;
  float t408;
  float t409;
  gg_slice t410;
  int64_t t411;
  int64_t t412;
  int64_t t413;
  float t414;
  float t415;
  gg_slice t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  float t420;
  float t421;
  float t422;
  float t423;
  float t424;
  gg_slice t425;
  int64_t t426;
  float t427;
  gg_slice t428;
  int64_t t429;
  int64_t t430;
  int64_t t431;
  float t432;
  int64_t t433;
  int64_t t434;
  gg_type_10 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  uint8_t v9;
  uint8_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  gg_type_2 v17;
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
  float v36;
  int64_t v37;
  int64_t v38;
  float v39;
  float v40;
  int64_t v41;
  int64_t v42;
  float v43;
  float v44;
  int64_t v45;
  int64_t v46;
  float v47;
  float v48;
  float v49;
  float v50;
  int64_t v51;
  int64_t v52;
  float v53;
  float v54;
  float v55;
  float v56;
  float v57;
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
  t1 = ((int64_t)(((*(v0))).f8));
  t2 = ((int64_t)(((int64_t)UINT64_C(84))));
  v9 = ((t1) == (t2));
  t3 = ((int64_t)(((*(v0))).f8));
  t4 = ((int64_t)(((int64_t)UINT64_C(83))));
  v10 = ((t3) == (t4));
  t5 = v9;
  t6 = (!(t5));
  t7 = t6;
  if (t7) {
  t8 = v10;
  t7 = (!(t8));
  }
  t9 = t7;
  t10 = t9;
  if (t10) {
  t11 = ((int64_t)(((*(v0))).f8));
  t12 = ((int64_t)(((int64_t)UINT64_C(82))));
  t10 = ((t11) != (t12));
  }
  t13 = t10;
  if (t13) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t14 = v9;
  if (t14) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(6))));
  }
  } else {
  {
  t15 = v10;
  if (t15) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  } else {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(5))));
  }
  }
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t16 = ((int64_t)UINT64_C(1));
  t17 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t16, ((int64_t)UINT64_C(7)));
  t18 = gg_view((((*t17)).f0).data, 4);
  t19 = ((int64_t)UINT64_C(0));
  t20 = ((int64_t *)t18.data) + gg_index(t19, t18.len);
  v12 = ((int64_t)((*t20)));
  /* FG_DIRECT_SLICE_INDEX */
  t21 = ((int64_t)UINT64_C(1));
  t22 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t21, ((int64_t)UINT64_C(7)));
  t23 = gg_view((((*t22)).f0).data, 4);
  t24 = ((int64_t)UINT64_C(1));
  t25 = ((int64_t *)t23.data) + gg_index(t24, t23.len);
  v13 = ((int64_t)((*t25)));
  /* FG_DIRECT_SLICE_INDEX */
  t26 = ((int64_t)UINT64_C(1));
  t27 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t26, ((int64_t)UINT64_C(7)));
  t28 = gg_view((((*t27)).f0).data, 4);
  t29 = ((int64_t)UINT64_C(2));
  t30 = ((int64_t *)t28.data) + gg_index(t29, t28.len);
  v14 = ((int64_t)((*t30)));
  /* FG_DIRECT_SLICE_INDEX */
  t31 = v11;
  t32 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t31, ((int64_t)UINT64_C(7)));
  t33 = gg_view((((*t32)).f0).data, 4);
  t34 = ((int64_t)UINT64_C(1));
  t35 = ((int64_t *)t33.data) + gg_index(t34, t33.len);
  v15 = ((int64_t)((*t35)));
  t36 = ((int64_t)(v12));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) > (t37));
  t39 = t38;
  if (t39) {
  t40 = ((int64_t)(v13));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t40) > (t41));
  }
  t42 = t39;
  t43 = t42;
  if (t43) {
  t44 = ((int64_t)(v14));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t44) >= (t45));
  }
  t46 = t43;
  t47 = t46;
  if (t47) {
  t48 = ((int64_t)(v15));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((t48) > (t49));
  }
  t50 = t47;
  t51 = t50;
  if (t51) {
  t52 = ((int64_t)(v14));
  t53 = ((int64_t)(v15));
  t54 = ((int64_t)(((int64_t)gg_mod(t52, t53))));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((t54) == (t55));
  }
  t56 = t51;
  t57 = t56;
  if (t57) {
  t58 = ((int64_t)(((*(v0))).f7));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t58) > (t59));
  }
  t60 = t57;
  t61 = t60;
  if (t61) {
  t62 = ((int64_t)(((*(v0))).f6));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((t62) >= (t63));
  }
  t64 = t61;
  t65 = t64;
  if (t65) {
  t66 = ((int64_t)(((*(v0))).f6));
  t67 = ((int64_t)(((*(v0))).f7));
  t65 = ((t66) < (t67));
  }
  if (!(t65)) gg_fail("assertion failed");
  t68 = ((int64_t)((((*(v0))).f5).f2));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t68) == (t69)))) gg_fail("assertion failed");
  t70 = &(((*(v0))).f5);
  t71 = ((int64_t)((v8).len));
  gg_function_3(t70, t71, base + 0);
  t73 = ((int64_t)(v11));
  t74 = ((int64_t)(((int64_t)UINT64_C(1))));
  t75 = ((int64_t)((uint64_t)(t73) + (uint64_t)(t74)));
  t72 = 0;
label0:;
  if (t72 >= t75) goto label2;
  v16 = t72;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t76 = v16;
  t77 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t76, ((int64_t)UINT64_C(7)));
  v17 = (*t77);
  t78 = ((int64_t)((v17).f2));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t78) == (t79)))) gg_fail("assertion failed");
  v18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t81 = ((int64_t)UINT64_C(4));
  t80 = 0;
label3:;
  if (t80 >= t81) goto label5;
  v19 = t80;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t82 = v19;
  t83 = ((int64_t *)((v17).f1).data) + gg_index(t82, ((int64_t)UINT64_C(4)));
  t84 = ((int64_t)((*t83)));
  t85 = ((int64_t)(v18));
  if (!(((t84) == (t85)))) gg_fail("assertion failed");
  t86 = ((int64_t)(v18));
  /* FG_DIRECT_SLICE_INDEX */
  t87 = v19;
  t88 = ((int64_t *)((v17).f0).data) + gg_index(t87, ((int64_t)UINT64_C(4)));
  t89 = ((int64_t)((*t88)));
  v18 = gg_function_5(t86, t89, base + 0);
  }
label4:;
  t80++; goto label3;
label5:;
  t90 = ((int64_t)((v17).f3));
  t91 = ((int64_t)(v18));
  if (!(((t90) >= (t91)))) gg_fail("assertion failed");
  }
label1:;
  t72++; goto label0;
label2:;
  /* FG_DIRECT_SLICE_INDEX */
  t92 = ((int64_t)UINT64_C(0));
  t93 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t92, ((int64_t)UINT64_C(7)));
  t94 = t93;
  t95 = ((int64_t)((v1).len));
  gg_function_3(t94, t95, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t96 = ((int64_t)UINT64_C(1));
  t97 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t96, ((int64_t)UINT64_C(7)));
  t98 = t97;
  t99 = ((int64_t)((v2).len));
  gg_function_3(t98, t99, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t100 = ((int64_t)UINT64_C(2));
  t101 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t100, ((int64_t)UINT64_C(7)));
  t102 = t101;
  t103 = ((int64_t)((v3).len));
  gg_function_3(t102, t103, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t104 = ((int64_t)UINT64_C(3));
  t105 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t104, ((int64_t)UINT64_C(7)));
  t106 = t105;
  t107 = ((int64_t)((v4).len));
  gg_function_3(t106, t107, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t108 = ((int64_t)UINT64_C(4));
  t109 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t108, ((int64_t)UINT64_C(7)));
  t110 = t109;
  t111 = ((int64_t)((v5).len));
  gg_function_3(t110, t111, base + 0);
  t112 = v10;
  t113 = (!(t112));
  if (t113) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t114 = ((int64_t)UINT64_C(5));
  t115 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t114, ((int64_t)UINT64_C(7)));
  t116 = t115;
  t117 = ((int64_t)((v6).len));
  gg_function_3(t116, t117, base + 0);
  }
  }
  t118 = v9;
  if (t118) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t119 = ((int64_t)UINT64_C(6));
  t120 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t119, ((int64_t)UINT64_C(7)));
  t121 = t120;
  t122 = ((int64_t)((v7).len));
  gg_function_3(t121, t122, base + 0);
  t123 = ((int64_t)(v12));
  t124 = ((int64_t)(((int64_t)UINT64_C(32))));
  t125 = ((int64_t)(((int64_t)gg_mod(t123, t124))));
  t126 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t125) == (t126)))) gg_fail("assertion failed");
  }
  }
  t127 = ((int64_t)(v12));
  t128 = ((int64_t)(v13));
  v20 = gg_function_5(t127, t128, base + 0);
  t129 = ((int64_t)(v20));
  t130 = ((int64_t)(v14));
  v21 = gg_function_5(t129, t130, base + 0);
  t131 = ((int64_t)(v12));
  t132 = ((int64_t)(v12));
  v22 = gg_function_5(t131, t132, base + 0);
  t133 = ((int64_t)(v22));
  t134 = ((int64_t)(v13));
  t135 = gg_function_5(t133, t134, base + 0);
  t136 = ((int64_t)(v15));
  v23 = gg_function_5(t135, t136, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t137 = v11;
  t138 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t137, ((int64_t)UINT64_C(7)));
  t139 = t138;
  t140 = gg_function_28(t139, base + 0);
  t141 = ((int64_t)(t140));
  t142 = ((int64_t)(v23));
  t143 = ((t141) == (t142));
  t144 = t143;
  if (t144) {
  t145 = ((int64_t)(v21));
  t146 = ((int64_t)(v23));
  t147 = gg_function_6(t145, t146, base + 0);
  t148 = ((int64_t)(((int64_t)UINT64_C(4))));
  t149 = gg_function_5(t147, t148, base + 0);
  t150 = ((int64_t)(t149));
  t151 = ((int64_t)((v8).len));
  t144 = ((t150) <= (t151));
  }
  if (!(t144)) gg_fail("assertion failed");
  t153 = v11;
  t152 = 0;
label6:;
  if (t152 >= t153) goto label8;
  v24 = t152;
  {
  t154 = v9;
  t155 = (!(t154));
  t156 = t155;
  if (t156) {
  t157 = ((int64_t)(v24));
  t158 = ((int64_t)(((int64_t)UINT64_C(3))));
  t156 = ((t157) == (t158));
  }
  t159 = t156;
  t160 = t159;
  if (t160) {
  t161 = v10;
  t160 = (!(t161));
  }
  t162 = t160;
  if (t162) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t163 = v24;
  t164 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t163, ((int64_t)UINT64_C(7)));
  t165 = t164;
  t166 = gg_function_28(t165, base + 0);
  t167 = ((int64_t)(t166));
  t168 = ((int64_t)(v20));
  if (!(((t167) >= (t168)))) gg_fail("assertion failed");
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t169 = v24;
  t170 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t169, ((int64_t)UINT64_C(7)));
  t171 = t170;
  t172 = gg_function_28(t171, base + 0);
  t173 = ((int64_t)(t172));
  t174 = ((int64_t)(v21));
  if (!(((t173) >= (t174)))) gg_fail("assertion failed");
  }
  }
  }
label7:;
  t152++; goto label6;
label8:;
  t175 = ((int64_t)(v13));
  t176 = ((int64_t)(((*(v0))).f6));
  t177 = ((int64_t)(((int64_t)((uint64_t)(t175) * (uint64_t)(t176)))));
  t178 = ((int64_t)(((*(v0))).f7));
  v25 = ((int64_t)(((int64_t)gg_div(t177, t178))));
  t179 = ((int64_t)(v13));
  t180 = ((int64_t)(((*(v0))).f6));
  t181 = ((int64_t)(((int64_t)UINT64_C(1))));
  t182 = ((int64_t)(((int64_t)((uint64_t)(t180) + (uint64_t)(t181)))));
  t183 = ((int64_t)(((int64_t)((uint64_t)(t179) * (uint64_t)(t182)))));
  t184 = ((int64_t)(((*(v0))).f7));
  v26 = ((int64_t)(((int64_t)gg_div(t183, t184))));
  t185 = ((int64_t)(v12));
  t186 = ((int64_t)(((int64_t)UINT64_C(8))));
  t187 = ((int64_t)(((int64_t)gg_div(t185, t186))));
  t188 = ((int64_t)(((int64_t)UINT64_C(8))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t187) * (uint64_t)(t188)))));
  t190 = ((int64_t)(v26));
  t191 = ((int64_t)(v25));
  t192 = ((int64_t)((uint64_t)(t190) - (uint64_t)(t191)));
  t189 = 0;
label9:;
  if (t189 >= t192) goto label11;
  v28 = t189;
  {
  t193 = ((int64_t)(v25));
  t194 = ((int64_t)(v28));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t193) + (uint64_t)(t194)))));
  t196 = v14;
  t195 = 0;
label12:;
  if (t195 >= t196) goto label14;
  v30 = t195;
  {
  t197 = ((int64_t)(v30));
  t198 = ((int64_t)(v14));
  t199 = ((int64_t)(v15));
  t200 = ((int64_t)(((int64_t)gg_div(t198, t199))));
  v31 = ((int64_t)(((int64_t)gg_div(t197, t200))));
  t201 = ((int64_t)(v30));
  t202 = ((int64_t)(v20));
  t203 = ((int64_t)(((int64_t)((uint64_t)(t201) * (uint64_t)(t202)))));
  t204 = ((int64_t)(v29));
  t205 = ((int64_t)(v12));
  t206 = ((int64_t)(((int64_t)((uint64_t)(t204) * (uint64_t)(t205)))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t203) + (uint64_t)(t206)))));
  t207 = ((int64_t)(v21));
  t208 = ((int64_t)(v31));
  t209 = ((int64_t)(v13));
  t210 = ((int64_t)(((int64_t)((uint64_t)(t208) * (uint64_t)(t209)))));
  t211 = ((int64_t)(v29));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t210) + (uint64_t)(t211)))));
  t213 = ((int64_t)(v22));
  t214 = ((int64_t)(((int64_t)((uint64_t)(t212) * (uint64_t)(t213)))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t207) + (uint64_t)(t214)))));
  t215 = ((int64_t)(v31));
  t216 = ((int64_t)(v13));
  t217 = ((int64_t)(((int64_t)((uint64_t)(t215) * (uint64_t)(t216)))));
  t218 = ((int64_t)(v29));
  t219 = ((int64_t)(((int64_t)((uint64_t)(t217) + (uint64_t)(t218)))));
  t220 = ((int64_t)(v22));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t219) * (uint64_t)(t220)))));
  t221 = ((int64_t)(v30));
  t222 = ((int64_t)(v14));
  t223 = ((int64_t)(v15));
  t224 = ((int64_t)(((int64_t)gg_div(t222, t223))));
  t225 = ((int64_t)(((int64_t)gg_mod(t221, t224))));
  t226 = ((int64_t)(((int64_t)UINT64_C(0))));
  t227 = ((t225) == (t226));
  if (t227) {
  {
  t229 = v22;
  t228 = 0;
label15:;
  if (t228 >= t229) goto label17;
  v35 = t228;
  {
  t230 = v9;
  if (t230) {
  {
  t231 = v7;
  t232 = ((int64_t)(v34));
  t233 = ((int64_t)(v35));
  t234 = ((int64_t)(((int64_t)((uint64_t)(t232) + (uint64_t)(t233)))));
  v36 = gg_function_20(t231, t234, base + 0);
  }
  } else {
  {
  t235 = v10;
  if (t235) {
  {
  t236 = v5;
  t237 = ((int64_t)(v34));
  t238 = ((int64_t)(v35));
  t239 = ((int64_t)(((int64_t)((uint64_t)(t237) + (uint64_t)(t238)))));
  v36 = gg_function_20(t236, t239, base + 0);
  }
  } else {
  {
  t240 = v6;
  t241 = ((int64_t)(v34));
  t242 = ((int64_t)(v35));
  t243 = ((int64_t)(((int64_t)((uint64_t)(t241) + (uint64_t)(t242)))));
  v36 = gg_function_20(t240, t243, base + 0);
  }
  }
  }
  }
  t244 = v8;
  t245 = ((int64_t)(v33));
  t246 = ((int64_t)(v35));
  t247 = ((int64_t)(((int64_t)((uint64_t)(t245) + (uint64_t)(t246)))));
  t248 = ((float)(v36));
  gg_function_11(t244, t247, t248, base + 0);
  }
label16:;
  t228++; goto label15;
label17:;
  }
  }
  t249 = v9;
  if (t249) {
  {
  t251 = v12;
  t250 = 0;
label18:;
  if (t250 >= t251) goto label20;
  v37 = t250;
  {
  t252 = ((int64_t)(v33));
  t253 = ((int64_t)(v37));
  t254 = ((int64_t)(v12));
  t255 = ((int64_t)(((int64_t)((uint64_t)(t253) * (uint64_t)(t254)))));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t252) + (uint64_t)(t255)))));
  /* FG_DIRECT_SLICE_INDEX */
  t256 = ((int64_t)UINT64_C(4));
  t257 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t256, ((int64_t)UINT64_C(7)));
  t258 = t257;
  t259 = v5;
  t260 = ((int64_t)(v32));
  t261 = ((int64_t)(((int64_t)UINT64_C(4))));
  t262 = ((int64_t)(((int64_t)((uint64_t)(t260) * (uint64_t)(t261)))));
  t263 = ((int64_t)(((int64_t)UINT64_C(4))));
  t264 = &(((*(v0))).f5);
  t265 = v8;
  t266 = ((int64_t)(v38));
  t267 = ((int64_t)(((int64_t)UINT64_C(4))));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t266) * (uint64_t)(t267)))));
  t269 = ((int64_t)(((int64_t)UINT64_C(4))));
  t270 = ((int64_t)(v12));
  v39 = gg_function_21(t258, t259, t262, t263, t264, t265, t268, t269, t270, base + 0);
  t271 = v4;
  t272 = ((int64_t)(v32));
  t273 = ((int64_t)(v37));
  t274 = ((int64_t)(((int64_t)((uint64_t)(t272) + (uint64_t)(t273)))));
  v40 = gg_function_20(t271, t274, base + 0);
  t276 = v12;
  t275 = 0;
label21:;
  if (t275 >= t276) goto label23;
  v41 = t275;
  {
  t277 = ((int64_t)(v32));
  t278 = ((int64_t)(v41));
  v42 = ((int64_t)(((int64_t)((uint64_t)(t277) + (uint64_t)(t278)))));
  t279 = ((float)(v40));
  t280 = v3;
  t281 = ((int64_t)(v42));
  t282 = gg_function_20(t280, t281, base + 0);
  t283 = ((float)(t282));
  v43 = ((float)(((t279) * (t283))));
  t284 = v8;
  t285 = ((int64_t)(v38));
  t286 = ((int64_t)(v41));
  t287 = ((int64_t)(((int64_t)((uint64_t)(t285) + (uint64_t)(t286)))));
  t288 = gg_function_20(t284, t287, base + 0);
  t289 = v2;
  t290 = ((int64_t)(v42));
  t291 = gg_function_20(t289, t290, base + 0);
  t292 = ((float)(v43));
  t293 = fmaf(t288, t291, t292);
  v44 = ((float)(t293));
  t294 = v8;
  t295 = ((int64_t)(v38));
  t296 = ((int64_t)(v41));
  t297 = ((int64_t)(((int64_t)((uint64_t)(t295) + (uint64_t)(t296)))));
  t298 = ((float)(v39));
  t299 = v6;
  t300 = ((int64_t)(v42));
  t301 = gg_function_20(t299, t300, base + 0);
  t302 = ((float)(v44));
  t303 = fmaf(t298, t301, t302);
  t304 = ((float)(t303));
  gg_function_11(t294, t297, t304, base + 0);
  }
label22:;
  t275++; goto label21;
label23:;
  t305 = v8;
  t306 = ((int64_t)(v32));
  t307 = ((int64_t)(v37));
  t308 = ((int64_t)(((int64_t)((uint64_t)(t306) + (uint64_t)(t307)))));
  t309 = &(((*(v0))).f5);
  t310 = v8;
  t311 = ((int64_t)(v38));
  t312 = ((int64_t)(((int64_t)UINT64_C(4))));
  t313 = ((int64_t)(((int64_t)((uint64_t)(t311) * (uint64_t)(t312)))));
  t314 = ((int64_t)(((int64_t)UINT64_C(4))));
  /* FG_DIRECT_SLICE_INDEX */
  t315 = ((int64_t)UINT64_C(0));
  t316 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t315, ((int64_t)UINT64_C(7)));
  t317 = t316;
  t318 = v1;
  t319 = ((int64_t)(v32));
  t320 = ((int64_t)(((int64_t)UINT64_C(4))));
  t321 = ((int64_t)(((int64_t)((uint64_t)(t319) * (uint64_t)(t320)))));
  t322 = ((int64_t)(((int64_t)UINT64_C(4))));
  t323 = ((int64_t)(v12));
  t324 = gg_function_21(t309, t310, t313, t314, t317, t318, t321, t322, t323, base + 0);
  gg_function_11(t305, t308, t324, base + 0);
  }
label19:;
  t250++; goto label18;
label20:;
  }
  } else {
  {
  t326 = v12;
  t325 = 0;
label24:;
  if (t325 >= t326) goto label26;
  v45 = t325;
  {
  t327 = v8;
  t328 = ((int64_t)(v32));
  t329 = ((int64_t)(v45));
  t330 = ((int64_t)(((int64_t)((uint64_t)(t328) + (uint64_t)(t329)))));
  t331 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  gg_function_11(t327, t330, t331, base + 0);
  }
label25:;
  t325++; goto label24;
label26:;
  t333 = v12;
  t332 = 0;
label27:;
  if (t332 >= t333) goto label29;
  v46 = t332;
  {
  t334 = v1;
  t335 = ((int64_t)(v32));
  t336 = ((int64_t)(v46));
  t337 = ((int64_t)(((int64_t)((uint64_t)(t335) + (uint64_t)(t336)))));
  v47 = gg_function_20(t334, t337, base + 0);
  t338 = v3;
  t339 = ((int64_t)(v32));
  t340 = ((int64_t)(v46));
  t341 = ((int64_t)(((int64_t)((uint64_t)(t339) + (uint64_t)(t340)))));
  t342 = gg_function_20(t338, t341, base + 0);
  t343 = ((float)(t342));
  t345 = v10;
  if (t345) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t346 = ((int64_t)UINT64_C(0));
  t347 = ((float *)(((*(v0))).f10).data) + gg_index(t346, ((int64_t)UINT64_C(4)));
  t344 = ((float)((*t347)));
  }
  } else {
  {
  t344 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  }
  }
  t348 = ((float)(t344));
  v48 = ((float)(((t343) * (t348))));
  t349 = v10;
  if (t349) {
  {
  t350 = v4;
  t351 = ((int64_t)(v32));
  t352 = ((int64_t)(v46));
  t353 = ((int64_t)(((int64_t)((uint64_t)(t351) + (uint64_t)(t352)))));
  v49 = gg_function_20(t350, t353, base + 0);
  }
  } else {
  {
  t354 = v5;
  t355 = ((int64_t)(v32));
  t356 = ((int64_t)(v46));
  t357 = ((int64_t)(((int64_t)((uint64_t)(t355) + (uint64_t)(t356)))));
  v49 = gg_function_20(t354, t357, base + 0);
  }
  }
  t358 = v10;
  if (t358) {
  {
  v50 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t359 = v4;
  t360 = ((int64_t)(v29));
  t361 = ((int64_t)(v12));
  t362 = ((int64_t)(((int64_t)((uint64_t)(t360) * (uint64_t)(t361)))));
  t363 = ((int64_t)(v46));
  t364 = ((int64_t)(((int64_t)((uint64_t)(t362) + (uint64_t)(t363)))));
  v50 = gg_function_20(t359, t364, base + 0);
  }
  }
  t366 = v12;
  t365 = 0;
label30:;
  if (t365 >= t366) goto label32;
  v51 = t365;
  {
  t367 = ((int64_t)(v33));
  t368 = ((int64_t)(v46));
  t369 = ((int64_t)(v12));
  t370 = ((int64_t)(((int64_t)((uint64_t)(t368) * (uint64_t)(t369)))));
  t371 = ((int64_t)(((int64_t)((uint64_t)(t367) + (uint64_t)(t370)))));
  t372 = ((int64_t)(v51));
  v52 = ((int64_t)(((int64_t)((uint64_t)(t371) + (uint64_t)(t372)))));
  t373 = v2;
  t374 = ((int64_t)(v32));
  t375 = ((int64_t)(v51));
  t376 = ((int64_t)(((int64_t)((uint64_t)(t374) + (uint64_t)(t375)))));
  t377 = gg_function_20(t373, t376, base + 0);
  t378 = ((float)(t377));
  t379 = ((float)(v47));
  v53 = ((float)(((t378) * (t379))));
  t380 = v8;
  t381 = ((int64_t)(v52));
  v54 = gg_function_20(t380, t381, base + 0);
  t382 = ((int64_t)(v51));
  t383 = ((int64_t)(v27));
  t384 = ((t382) < (t383));
  if (t384) {
  {
  t385 = ((float)(v54));
  t386 = ((float)(v49));
  t387 = ((float)(v53));
  t388 = fmaf(t385, t386, t387);
  v55 = ((float)(t388));
  }
  } else {
  {
  t389 = ((float)(v54));
  t390 = ((float)(v49));
  t391 = ((float)(((t389) * (t390))));
  t392 = ((float)(v53));
  v55 = ((float)(((t391) + (t392))));
  }
  }
  t393 = v10;
  if (t393) {
  {
  v56 = ((float)(v55));
  }
  } else {
  {
  t394 = ((int64_t)(v51));
  t395 = ((int64_t)(v27));
  t396 = ((t394) < (t395));
  if (t396) {
  {
  t397 = ((float)(v53));
  t398 = ((float)(v50));
  t399 = ((float)(v54));
  t400 = fmaf(t397, t398, t399);
  v56 = ((float)(t400));
  }
  } else {
  {
  t401 = ((float)(v53));
  t402 = ((float)(v50));
  t403 = ((float)(((t401) * (t402))));
  t404 = ((float)(v54));
  v56 = ((float)(((t403) + (t404))));
  }
  }
  }
  }
  t405 = ((int64_t)(v51));
  t406 = ((int64_t)(v27));
  t407 = ((t405) < (t406));
  if (t407) {
  {
  t408 = ((float)(v56));
  t409 = ((float)(v48));
  t410 = v8;
  t411 = ((int64_t)(v32));
  t412 = ((int64_t)(v51));
  t413 = ((int64_t)(((int64_t)((uint64_t)(t411) + (uint64_t)(t412)))));
  t414 = gg_function_20(t410, t413, base + 0);
  t415 = fmaf(t408, t409, t414);
  v57 = ((float)(t415));
  }
  } else {
  {
  t416 = v8;
  t417 = ((int64_t)(v32));
  t418 = ((int64_t)(v51));
  t419 = ((int64_t)(((int64_t)((uint64_t)(t417) + (uint64_t)(t418)))));
  t420 = gg_function_20(t416, t419, base + 0);
  t421 = ((float)(t420));
  t422 = ((float)(v56));
  t423 = ((float)(v48));
  t424 = ((float)(((t422) * (t423))));
  v57 = ((float)(((t421) + (t424))));
  }
  }
  t425 = v8;
  t426 = ((int64_t)(v52));
  t427 = ((float)(v55));
  gg_function_11(t425, t426, t427, base + 0);
  t428 = v8;
  t429 = ((int64_t)(v32));
  t430 = ((int64_t)(v51));
  t431 = ((int64_t)(((int64_t)((uint64_t)(t429) + (uint64_t)(t430)))));
  t432 = ((float)(v57));
  gg_function_11(t428, t431, t432, base + 0);
  }
label31:;
  t365++; goto label30;
label32:;
  }
label28:;
  t332++; goto label27;
label29:;
  }
  }
  }
label13:;
  t195++; goto label12;
label14:;
  }
label10:;
  t189++; goto label9;
label11:;
  t433 = ((int64_t)(((*(v0))).f8));
  t434 = ((int64_t)(v21));
  fg143_witness(t433, t434);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_1(gg_type_10 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, int64_t base) {
  uint8_t t0;
  gg_type_10 t1;
  gg_slice t2;
  gg_slice t3;
  gg_slice t4;
  gg_slice t5;
  gg_slice t6;
  gg_slice t7;
  gg_slice t8;
  gg_slice t9;
  uint8_t t10;
  uint8_t t11;
  gg_type_15 t12;
  int64_t t13;
  int64_t t14;
  gg_type_2 * t15;
  gg_type_15 t16;
  int64_t t17;
  int64_t t18;
  gg_type_2 * t19;
  gg_type_15 t20;
  int64_t t21;
  int64_t t22;
  gg_type_2 * t23;
  gg_type_15 t24;
  int64_t t25;
  int64_t t26;
  gg_type_2 * t27;
  gg_type_15 t28;
  int64_t t29;
  int64_t t30;
  gg_type_2 * t31;
  gg_type_15 t32;
  int64_t t33;
  int64_t t34;
  gg_type_2 * t35;
  gg_type_15 t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  gg_type_10 t41;
  gg_slice t42;
  gg_slice t43;
  gg_slice t44;
  gg_slice t45;
  gg_slice t46;
  gg_slice t47;
  gg_slice t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  int64_t t52;
  gg_type_2 * t53;
  gg_type_15 t54;
  int64_t t55;
  gg_type_2 t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  gg_type_2 t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  gg_type_2 * t71;
  gg_slice t72;
  int64_t t73;
  int64_t * t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  gg_type_25 t81;
  gg_slice t82;
  gg_slice t83;
  gg_slice t84;
  gg_slice t85;
  gg_slice t86;
  gg_slice t87;
  gg_slice t88;
  gg_slice t89;
  gg_type_10 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_type_24 v9;
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
  t1 = v0;
  t2 = v1;
  t3 = v2;
  t4 = v3;
  t5 = v4;
  t6 = v5;
  t7 = v6;
  t8 = v7;
  t9 = v8;
  t10 = gg_function_2(t1, t2, t3, t4, t5, t6, t7, t8, t9, base + 0);
  t11 = t10;
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t12 = &(((*(v0))).f5);
  t13 = ((int64_t)((v8).len));
  gg_function_3(t12, t13, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t14 = ((int64_t)UINT64_C(0));
  t15 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t14, ((int64_t)UINT64_C(7)));
  t16 = t15;
  t17 = ((int64_t)((v1).len));
  gg_function_3(t16, t17, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t18 = ((int64_t)UINT64_C(1));
  t19 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t18, ((int64_t)UINT64_C(7)));
  t20 = t19;
  t21 = ((int64_t)((v2).len));
  gg_function_3(t20, t21, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t22 = ((int64_t)UINT64_C(2));
  t23 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t22, ((int64_t)UINT64_C(7)));
  t24 = t23;
  t25 = ((int64_t)((v3).len));
  gg_function_3(t24, t25, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t26 = ((int64_t)UINT64_C(3));
  t27 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t26, ((int64_t)UINT64_C(7)));
  t28 = t27;
  t29 = ((int64_t)((v4).len));
  gg_function_3(t28, t29, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t30 = ((int64_t)UINT64_C(4));
  t31 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t30, ((int64_t)UINT64_C(7)));
  t32 = t31;
  t33 = ((int64_t)((v5).len));
  gg_function_3(t32, t33, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t34 = ((int64_t)UINT64_C(5));
  t35 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t34, ((int64_t)UINT64_C(7)));
  t36 = t35;
  t37 = ((int64_t)((v6).len));
  gg_function_3(t36, t37, base + 0);
  t38 = ((int64_t)(((*(v0))).f8));
  t39 = ((int64_t)(((int64_t)UINT64_C(86))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  t41 = v0;
  t42 = v1;
  t43 = v2;
  t44 = v3;
  t45 = v4;
  t46 = v5;
  t47 = v6;
  t48 = v8;
  t0 = gg_function_23(t41, t42, t43, t44, t45, t46, t47, t48, base + 0);
  goto gg_done;
  }
  }
  t49 = ((int64_t)(((*(v0))).f8));
  t50 = ((int64_t)(((int64_t)UINT64_C(77))));
  t51 = ((t49) == (t50));
  if (t51) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t52 = ((int64_t)UINT64_C(6));
  t53 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t52, ((int64_t)UINT64_C(7)));
  t54 = t53;
  t55 = ((int64_t)((v7).len));
  gg_function_3(t54, t55, base + 0);
  memset(&(v9), 0, sizeof(v9));
  memset(&(t56), 0, sizeof(t56));
  memset(&t57, 0, sizeof(t57));
  t58 = ((int64_t)(t57));
  for (t59=0; t59<4; t59++) ((t56).f0).data[t59] = t58;
  memset(&t60, 0, sizeof(t60));
  t61 = ((int64_t)(t60));
  for (t62=0; t62<4; t62++) ((t56).f1).data[t62] = t61;
  memset(&t63, 0, sizeof(t63));
  (t56).f2 = ((int64_t)(t63));
  memset(&t64, 0, sizeof(t64));
  (t56).f3 = ((int64_t)(t64));
  t65 = t56;
  for (t66=0; t66<7; t66++) ((v9).f11).data[t66] = t65;
  memset(&t67, 0, sizeof(t67));
  (v9).f12 = ((int64_t)(t67));
  memset(&t68, 0, sizeof(t68));
  (v9).f13 = ((int64_t)(t68));
  memset(&t69, 0, sizeof(t69));
  (v9).f14 = ((int64_t)(t69));
  (v9).f11 = ((*(v0))).f4;
  (v9).f12 = ((int64_t)(((*(v0))).f6));
  (v9).f13 = ((int64_t)(((*(v0))).f7));
  (v9).f14 = ((int64_t)(((*(v0))).f9));
  /* FG_DIRECT_SLICE_INDEX */
  t70 = ((int64_t)UINT64_C(3));
  t71 = ((gg_type_2 *)(((*(v0))).f4).data) + gg_index(t70, ((int64_t)UINT64_C(7)));
  t72 = gg_view((((*t71)).f0).data, 4);
  t73 = ((int64_t)UINT64_C(0));
  t74 = ((int64_t *)t72.data) + gg_index(t73, t72.len);
  t75 = ((int64_t)((*t74)));
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (!t78) {
  t79 = ((int64_t)(((*(v0))).f9));
  t80 = ((int64_t)(((int64_t)UINT64_C(1))));
  t78 = ((t79) == (t80));
  }
  if (!(t78)) gg_fail("assertion failed");
  t81 = &(v9);
  t82 = v1;
  t83 = v2;
  t84 = v3;
  t85 = v4;
  t86 = v5;
  t87 = v6;
  t88 = v7;
  t89 = v8;
  t0 = gg_function_26(t81, t82, t83, t84, t85, t86, t87, t88, t89, base + 0);
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
  gg_type_2 t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  gg_type_2 t9;
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
  float t23;
  float t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  uint8_t t29;
  uint8_t t30;
  int64_t t31;
  gg_type_10 t32;
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
  gg_type_1 v0;
  gg_type_9 v1;
  gg_type_9 v2;
  uint8_t v3;
  {
  memset(&(v0), 0, sizeof(v0));
  memset(&(t0), 0, sizeof(t0));
  memset(&t1, 0, sizeof(t1));
  t2 = ((int64_t)(t1));
  for (t3=0; t3<4; t3++) ((t0).f0).data[t3] = t2;
  memset(&t4, 0, sizeof(t4));
  t5 = ((int64_t)(t4));
  for (t6=0; t6<4; t6++) ((t0).f1).data[t6] = t5;
  memset(&t7, 0, sizeof(t7));
  (t0).f2 = ((int64_t)(t7));
  memset(&t8, 0, sizeof(t8));
  (t0).f3 = ((int64_t)(t8));
  t9 = t0;
  for (t10=0; t10<7; t10++) ((v0).f4).data[t10] = t9;
  memset(&((v0).f5), 0, sizeof((v0).f5));
  memset(&t11, 0, sizeof(t11));
  t12 = ((int64_t)(t11));
  for (t13=0; t13<4; t13++) (((v0).f5).f0).data[t13] = t12;
  memset(&t14, 0, sizeof(t14));
  t15 = ((int64_t)(t14));
  for (t16=0; t16<4; t16++) (((v0).f5).f1).data[t16] = t15;
  memset(&t17, 0, sizeof(t17));
  ((v0).f5).f2 = ((int64_t)(t17));
  memset(&t18, 0, sizeof(t18));
  ((v0).f5).f3 = ((int64_t)(t18));
  memset(&t19, 0, sizeof(t19));
  (v0).f6 = ((int64_t)(t19));
  memset(&t20, 0, sizeof(t20));
  (v0).f7 = ((int64_t)(t20));
  memset(&t21, 0, sizeof(t21));
  (v0).f8 = ((int64_t)(t21));
  memset(&t22, 0, sizeof(t22));
  (v0).f9 = ((int64_t)(t22));
  memset(&t23, 0, sizeof(t23));
  t24 = ((float)(t23));
  for (t25=0; t25<4; t25++) ((v0).f10).data[t25] = t24;
  memset(&t26, 0, sizeof(t26));
  t27 = ((uint8_t)(t26));
  for (t28=0; t28<1; t28++) (v1).data[t28] = t27;
  memset(&t29, 0, sizeof(t29));
  t30 = ((uint8_t)(t29));
  for (t31=0; t31<1; t31++) (v2).data[t31] = t30;
  t32 = &(v0);
  t33 = gg_view((v1).data, 1);
  t34 = gg_view((v1).data, 1);
  t35 = gg_view((v1).data, 1);
  t36 = gg_view((v1).data, 1);
  t37 = gg_view((v1).data, 1);
  t38 = gg_view((v1).data, 1);
  t39 = gg_view((v1).data, 1);
  t40 = gg_view((v2).data, 1);
  v3 = gg_function_1(t32, t33, t34, t35, t36, t37, t38, t39, t40, base + 0);
  t41.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t41.len = 0;
  gg_bool(gg_ref(&t41, gg_get_stack(base + 0)), v3);
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
