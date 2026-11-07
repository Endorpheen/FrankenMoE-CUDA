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
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_6;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_1 {
  gg_type_2 f3;
  gg_type_2 f4;
  gg_type_2 f5;
  int64_t f6;
  int64_t f7;
  int64_t f8;
  int64_t f9;
  gg_type_6 f10;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[1]; } gg_type_7;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint16_t data[1]; } gg_type_9;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_10;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_10 data[1]; } gg_type_11;
#pragma pack(pop)
typedef gg_type_1 *gg_type_12;
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
static uint8_t gg_eq_5(float a, float b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, float value);
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, gg_type_6 value);
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
static uint8_t gg_eq_17(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_slice value);
static uint8_t gg_eq_18(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, gg_slice value);
static uint8_t gg_eq_19(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, uint8_t value);
static uint8_t gg_eq_21(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_21(gg_slice a, gg_slice b);
static void gg_render_21(gg_reference out, uint32_t value);
static uint8_t gg_eq_22(double a, double b);
static uint8_t gg_equal_elements_22(gg_slice a, gg_slice b);
static void gg_render_22(gg_reference out, double value);
static uint8_t gg_eq_23(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_23(gg_slice a, gg_slice b);
static void gg_render_23(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((gg_type_1*)a.data)[i],((gg_type_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b) {
if(!gg_eq_2(a.f3,b.f3))return 0;
if(!gg_eq_2(a.f4,b.f4))return 0;
if(!gg_eq_2(a.f5,b.f5))return 0;
if(!gg_eq_3(a.f6,b.f6))return 0;
if(!gg_eq_3(a.f7,b.f7))return 0;
if(!gg_eq_3(a.f8,b.f8))return 0;
if(!gg_eq_3(a.f9,b.f9))return 0;
if(!gg_eq_6(a.f10,b.f10))return 0;
return 1;
}
static void gg_render_1(gg_reference out, gg_type_1 value) {
gg_append(out,"\120\154\141\156\040\173\040",7,1);
gg_render_2(out, value.f3);
gg_append(out,", ",2,1);
gg_render_2(out, value.f4);
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
gg_render_6(out, value.f10);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
if(!gg_eq_4(a.f0,b.f0))return 0;
if(!gg_eq_4(a.f1,b.f1))return 0;
if(!gg_eq_3(a.f2,b.f2))return 0;
return 1;
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
gg_append(out,"\123\150\141\160\145\040\173\040",8,1);
gg_render_4(out, value.f0);
gg_append(out,", ",2,1);
gg_render_4(out, value.f1);
gg_append(out,", ",2,1);
gg_render_3(out, value.f2);
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
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(float a, float b) {
return a == b;
}
static void gg_render_5(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_type_6*)a.data)[i],((gg_type_6*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b) {
return gg_equal_elements_5(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_6(gg_reference out, gg_type_6 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_5(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
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
return gg_equal_elements_8(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((uint16_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
return gg_equal_elements_5(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
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
return a == b;
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_slice a, gg_slice b) {
return gg_equal_elements_5(a,b);
}
static void gg_render_13(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_14(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((uint16_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(gg_slice a, gg_slice b) {
return gg_equal_elements_5(a,b);
}
static void gg_render_15(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_16(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((uint16_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_slice a, gg_slice b) {
return gg_equal_elements_10(a,b);
}
static void gg_render_17(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((gg_type_10*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_slice a, gg_slice b) {
return gg_equal_elements_10(a,b);
}
static void gg_render_18(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((gg_type_10*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_19(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_21(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_21(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_21(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_21(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_22(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_22(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_22(double a, double b) {
return a == b;
}
static void gg_render_22(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_23(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_23(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_23(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_23(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t base);
static void gg_function_3(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static void gg_function_4(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static void gg_function_5(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static void gg_function_6(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static void gg_function_7(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static float gg_function_11(uint16_t p0, int64_t base);
static uint16_t gg_function_13(float p0, int64_t base);
static void gg_function_15(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_16(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_17(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_18(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_20(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_22(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_23(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_25(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_28(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_29(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_30(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_31(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_33(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_35(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_37(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_39(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_40(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_41(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_43(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_45(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_47(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_49(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_51(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_52(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_53(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static void gg_function_54(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static float gg_function_11(uint16_t p0, int64_t base) {
  float t0;
  uint32_t t1;
  int64_t t2;
  uint32_t t3;
  float t4;
  uint16_t v0;
  v0 = p0;
  {
  t1 = ((uint32_t)(((uint32_t)(v0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(16))));
  t3 = ((uint32_t)(((uint32_t)((uint64_t)(t1) << ((uint64_t)(t2) & 31)))));
  t4 = fg_el_from_bits(t3);
  t0 = ((float)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint16_t gg_function_13(float p0, int64_t base) {
  uint16_t t0;
  float t1;
  uint32_t t2;
  uint32_t t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
  uint8_t t7;
  uint32_t t8;
  int64_t t9;
  uint32_t t10;
  uint32_t t11;
  uint32_t t12;
  uint32_t t13;
  uint32_t t14;
  int64_t t15;
  uint32_t t16;
  uint32_t t17;
  uint32_t t18;
  uint32_t t19;
  uint32_t t20;
  int64_t t21;
  float v0;
  uint32_t v1;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = fg_el_bits(t1);
  v1 = ((uint32_t)(t2));
  t3 = ((uint32_t)(v1));
  t4 = ((uint32_t)(((int64_t)UINT64_C(2147483647))));
  t5 = ((uint32_t)(((t3) & (t4))));
  t6 = ((uint32_t)(((int64_t)UINT64_C(2139095040))));
  t7 = ((t5) > (t6));
  if (t7) {
  {
  t8 = ((uint32_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(16))));
  t10 = ((uint32_t)(((t8) >> ((uint64_t)(t9) & 31))));
  t11 = ((uint32_t)(((int64_t)UINT64_C(64))));
  t0 = ((uint16_t)(((uint16_t)(((t10) | (t11))))));
  goto gg_done;
  }
  }
  t12 = ((uint32_t)(v1));
  t13 = ((uint32_t)(((int64_t)UINT64_C(32767))));
  t14 = ((uint32_t)(v1));
  t15 = ((int64_t)(((int64_t)UINT64_C(16))));
  t16 = ((uint32_t)(((t14) >> ((uint64_t)(t15) & 31))));
  t17 = ((uint32_t)(((int64_t)UINT64_C(1))));
  t18 = ((uint32_t)(((t16) & (t17))));
  t19 = ((uint32_t)(((uint32_t)((uint64_t)(t13) + (uint64_t)(t18)))));
  t20 = ((uint32_t)(((uint32_t)((uint64_t)(t12) + (uint64_t)(t19)))));
  t21 = ((int64_t)(((int64_t)UINT64_C(16))));
  t0 = ((uint16_t)(((uint16_t)(((t20) >> ((uint64_t)(t21) & 31))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_54(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  int64_t * t13;
  int64_t t14;
  int64_t t15;
  int64_t * t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t * t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t * t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t * t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t * t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  float * t46;
  float t47;
  int64_t t48;
  float * t49;
  float t50;
  int64_t t51;
  float * t52;
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
  int64_t t64;
  int64_t t65;
  int64_t * t66;
  int64_t t67;
  int64_t t68;
  int64_t * t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t * t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t * t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t * t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t * t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint16_t * t99;
  uint16_t t100;
  float t101;
  float t102;
  int64_t t103;
  uint16_t * t104;
  uint16_t t105;
  float t106;
  float t107;
  int64_t t108;
  uint16_t * t109;
  float t110;
  uint16_t t111;
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
  int64_t t123;
  int64_t t124;
  int64_t * t125;
  int64_t t126;
  int64_t t127;
  int64_t * t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t * t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t * t138;
  int64_t t139;
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
  int64_t t150;
  int64_t t151;
  int64_t * t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  uint16_t * t158;
  uint16_t t159;
  float t160;
  float t161;
  int64_t t162;
  uint16_t * t163;
  uint16_t t164;
  float t165;
  float t166;
  int64_t t167;
  uint16_t * t168;
  float t169;
  int64_t t170;
  int64_t t171;
  uint8_t t172;
  uint8_t t173;
  int64_t t174;
  int64_t t175;
  uint8_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  int64_t * t183;
  int64_t t184;
  int64_t t185;
  int64_t * t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t * t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t * t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t * t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t * t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  uint16_t * t216;
  uint16_t t217;
  float t218;
  float t219;
  int64_t t220;
  float * t221;
  float t222;
  int64_t t223;
  uint16_t * t224;
  float t225;
  int64_t t226;
  int64_t t227;
  uint8_t t228;
  uint8_t t229;
  int64_t t230;
  int64_t t231;
  uint8_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  uint8_t t236;
  int64_t t237;
  int64_t t238;
  int64_t * t239;
  int64_t t240;
  int64_t t241;
  int64_t * t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t * t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t * t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t * t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t * t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  uint16_t * t272;
  uint16_t t273;
  float t274;
  float t275;
  int64_t t276;
  float * t277;
  float t278;
  int64_t t279;
  float * t280;
  int64_t t281;
  int64_t t282;
  uint8_t t283;
  uint8_t t284;
  int64_t t285;
  int64_t t286;
  uint8_t t287;
  uint8_t t288;
  int64_t t289;
  int64_t t290;
  uint8_t t291;
  int64_t t292;
  int64_t t293;
  int64_t * t294;
  int64_t t295;
  int64_t t296;
  int64_t * t297;
  int64_t t298;
  int64_t t299;
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
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t * t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t * t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  uint16_t * t327;
  uint16_t t328;
  float t329;
  float t330;
  int64_t t331;
  float * t332;
  float t333;
  int64_t t334;
  uint16_t * t335;
  float t336;
  uint16_t t337;
  int64_t t338;
  int64_t t339;
  uint8_t t340;
  uint8_t t341;
  int64_t t342;
  int64_t t343;
  uint8_t t344;
  uint8_t t345;
  int64_t t346;
  int64_t t347;
  uint8_t t348;
  int64_t t349;
  int64_t t350;
  int64_t * t351;
  int64_t t352;
  int64_t t353;
  int64_t * t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t * t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t * t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  int64_t * t372;
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
  uint16_t * t384;
  uint16_t t385;
  float t386;
  float t387;
  int64_t t388;
  float * t389;
  float t390;
  int64_t t391;
  float * t392;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  float v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  float v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  float v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  float v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
  float v42;
  int64_t v43;
  int64_t v44;
  int64_t v45;
  int64_t v46;
  int64_t v47;
  float v48;
  int64_t v49;
  int64_t v50;
  int64_t v51;
  int64_t v52;
  int64_t v53;
  float v54;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f4).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)((((*(v0))).f5).f2));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) == (t9));
  }
  t10 = t7;
  if (t10) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t12 = ((int64_t)UINT64_C(0));
  t13 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t12, ((int64_t)UINT64_C(4)));
  t14 = ((int64_t)((*t13)));
  /* FG_DIRECT_SLICE_INDEX */
  t15 = ((int64_t)UINT64_C(0));
  t16 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t15, ((int64_t)UINT64_C(4)));
  t17 = ((int64_t)((*t16)));
  t18 = ((int64_t)gg_div(t14, t17));
  t11 = 0;
label0:;
  if (t11 >= t18) goto label2;
  v13 = t11;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t20 = ((int64_t)UINT64_C(0));
  t21 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t20, ((int64_t)UINT64_C(4)));
  t22 = (*t21);
  t19 = 0;
label3:;
  if (t19 >= t22) goto label5;
  v14 = t19;
  {
  t23 = ((int64_t)(v10));
  t24 = ((int64_t)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t25 = ((int64_t)UINT64_C(0));
  t26 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t25, ((int64_t)UINT64_C(4)));
  t27 = ((int64_t)((*t26)));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t27)))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t28)))));
  t30 = ((int64_t)(v14));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t31 = ((int64_t)(v11));
  t32 = ((int64_t)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t33 = ((int64_t)UINT64_C(0));
  t34 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t33, ((int64_t)UINT64_C(4)));
  t35 = ((int64_t)((*t34)));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t35)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t36)))));
  t37 = ((int64_t)(v12));
  t38 = ((int64_t)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t39 = ((int64_t)UINT64_C(0));
  t40 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t39, ((int64_t)UINT64_C(4)));
  t41 = ((int64_t)((*t40)));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t41)))));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t42)))));
  t44 = ((int64_t)(v14));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t44)))));
  /* FG_DIRECT_SLICE_INDEX */
  t45 = v15;
  t46 = ((float *)(v1).data) + gg_index(t45, (v1).len);
  t47 = ((float)((*t46)));
  /* FG_DIRECT_SLICE_INDEX */
  t48 = v16;
  t49 = ((float *)(v3).data) + gg_index(t48, (v3).len);
  t50 = ((float)((*t49)));
  v18 = ((float)(((t47) / (t50))));
  /* FG_DIRECT_SLICE_INDEX */
  t51 = v17;
  t52 = ((float *)(v5).data) + gg_index(t51, (v5).len);
  (*t52) = ((float)(v18));
  }
label4:;
  t19++; goto label3;
label5:;
  }
label1:;
  t11++; goto label0;
label2:;
  }
  } else {
  t53 = ((int64_t)((((*(v0))).f3).f2));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((t53) == (t54));
  t56 = t55;
  if (t56) {
  t57 = ((int64_t)((((*(v0))).f4).f2));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t56 = ((t57) == (t58));
  }
  t59 = t56;
  t60 = t59;
  if (t60) {
  t61 = ((int64_t)((((*(v0))).f5).f2));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((t61) == (t62));
  }
  t63 = t60;
  if (t63) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t65 = ((int64_t)UINT64_C(0));
  t66 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t65, ((int64_t)UINT64_C(4)));
  t67 = ((int64_t)((*t66)));
  /* FG_DIRECT_SLICE_INDEX */
  t68 = ((int64_t)UINT64_C(0));
  t69 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t68, ((int64_t)UINT64_C(4)));
  t70 = ((int64_t)((*t69)));
  t71 = ((int64_t)gg_div(t67, t70));
  t64 = 0;
label6:;
  if (t64 >= t71) goto label8;
  v19 = t64;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t73 = ((int64_t)UINT64_C(0));
  t74 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t73, ((int64_t)UINT64_C(4)));
  t75 = (*t74);
  t72 = 0;
label9:;
  if (t72 >= t75) goto label11;
  v20 = t72;
  {
  t76 = ((int64_t)(v10));
  t77 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t78 = ((int64_t)UINT64_C(0));
  t79 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t78, ((int64_t)UINT64_C(4)));
  t80 = ((int64_t)((*t79)));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t81)))));
  t83 = ((int64_t)(v20));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  t84 = ((int64_t)(v11));
  t85 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t86 = ((int64_t)UINT64_C(0));
  t87 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t86, ((int64_t)UINT64_C(4)));
  t88 = ((int64_t)((*t87)));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t88)))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t89)))));
  t90 = ((int64_t)(v12));
  t91 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t92 = ((int64_t)UINT64_C(0));
  t93 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t92, ((int64_t)UINT64_C(4)));
  t94 = ((int64_t)((*t93)));
  t95 = ((int64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t94)))));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t95)))));
  t97 = ((int64_t)(v20));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  /* FG_DIRECT_SLICE_INDEX */
  t98 = v21;
  t99 = ((uint16_t *)(v2).data) + gg_index(t98, (v2).len);
  t100 = ((uint16_t)((*t99)));
  t101 = fg_el_half(t100);
  t102 = ((float)(t101));
  /* FG_DIRECT_SLICE_INDEX */
  t103 = v22;
  t104 = ((uint16_t *)(v4).data) + gg_index(t103, (v4).len);
  t105 = ((uint16_t)((*t104)));
  t106 = fg_el_half(t105);
  t107 = ((float)(t106));
  v24 = ((float)(((t102) / (t107))));
  /* FG_DIRECT_SLICE_INDEX */
  t108 = v23;
  t109 = ((uint16_t *)(v6).data) + gg_index(t108, (v6).len);
  t110 = ((float)(v24));
  t111 = fg_el_to_half(t110);
  (*t109) = ((uint16_t)(t111));
  }
label10:;
  t72++; goto label9;
label11:;
  }
label7:;
  t64++; goto label6;
label8:;
  }
  } else {
  t112 = ((int64_t)((((*(v0))).f3).f2));
  t113 = ((int64_t)(((int64_t)UINT64_C(2))));
  t114 = ((t112) == (t113));
  t115 = t114;
  if (t115) {
  t116 = ((int64_t)((((*(v0))).f4).f2));
  t117 = ((int64_t)(((int64_t)UINT64_C(2))));
  t115 = ((t116) == (t117));
  }
  t118 = t115;
  t119 = t118;
  if (t119) {
  t120 = ((int64_t)((((*(v0))).f5).f2));
  t121 = ((int64_t)(((int64_t)UINT64_C(2))));
  t119 = ((t120) == (t121));
  }
  t122 = t119;
  if (t122) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t124 = ((int64_t)UINT64_C(0));
  t125 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t124, ((int64_t)UINT64_C(4)));
  t126 = ((int64_t)((*t125)));
  /* FG_DIRECT_SLICE_INDEX */
  t127 = ((int64_t)UINT64_C(0));
  t128 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t127, ((int64_t)UINT64_C(4)));
  t129 = ((int64_t)((*t128)));
  t130 = ((int64_t)gg_div(t126, t129));
  t123 = 0;
label12:;
  if (t123 >= t130) goto label14;
  v25 = t123;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t132 = ((int64_t)UINT64_C(0));
  t133 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t132, ((int64_t)UINT64_C(4)));
  t134 = (*t133);
  t131 = 0;
label15:;
  if (t131 >= t134) goto label17;
  v26 = t131;
  {
  t135 = ((int64_t)(v10));
  t136 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t137 = ((int64_t)UINT64_C(0));
  t138 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t137, ((int64_t)UINT64_C(4)));
  t139 = ((int64_t)((*t138)));
  t140 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t139)))));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t135) + (uint64_t)(t140)))));
  t142 = ((int64_t)(v26));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t141) + (uint64_t)(t142)))));
  t143 = ((int64_t)(v11));
  t144 = ((int64_t)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t145 = ((int64_t)UINT64_C(0));
  t146 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t145, ((int64_t)UINT64_C(4)));
  t147 = ((int64_t)((*t146)));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t147)))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t143) + (uint64_t)(t148)))));
  t149 = ((int64_t)(v12));
  t150 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t151 = ((int64_t)UINT64_C(0));
  t152 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t151, ((int64_t)UINT64_C(4)));
  t153 = ((int64_t)((*t152)));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t150) * (uint64_t)(t153)))));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t149) + (uint64_t)(t154)))));
  t156 = ((int64_t)(v26));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t155) + (uint64_t)(t156)))));
  /* FG_DIRECT_SLICE_INDEX */
  t157 = v27;
  t158 = ((uint16_t *)(v2).data) + gg_index(t157, (v2).len);
  t159 = ((uint16_t)((*t158)));
  t160 = gg_function_11(t159, base + 0);
  t161 = ((float)(t160));
  /* FG_DIRECT_SLICE_INDEX */
  t162 = v28;
  t163 = ((uint16_t *)(v4).data) + gg_index(t162, (v4).len);
  t164 = ((uint16_t)((*t163)));
  t165 = gg_function_11(t164, base + 0);
  t166 = ((float)(t165));
  v30 = ((float)(((t161) / (t166))));
  /* FG_DIRECT_SLICE_INDEX */
  t167 = v29;
  t168 = ((uint16_t *)(v6).data) + gg_index(t167, (v6).len);
  t169 = ((float)(v30));
  (*t168) = gg_function_13(t169, base + 0);
  }
label16:;
  t131++; goto label15;
label17:;
  }
label13:;
  t123++; goto label12;
label14:;
  }
  } else {
  t170 = ((int64_t)((((*(v0))).f3).f2));
  t171 = ((int64_t)(((int64_t)UINT64_C(2))));
  t172 = ((t170) == (t171));
  t173 = t172;
  if (t173) {
  t174 = ((int64_t)((((*(v0))).f4).f2));
  t175 = ((int64_t)(((int64_t)UINT64_C(0))));
  t173 = ((t174) == (t175));
  }
  t176 = t173;
  t177 = t176;
  if (t177) {
  t178 = ((int64_t)((((*(v0))).f5).f2));
  t179 = ((int64_t)(((int64_t)UINT64_C(2))));
  t177 = ((t178) == (t179));
  }
  t180 = t177;
  if (t180) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t182 = ((int64_t)UINT64_C(0));
  t183 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t182, ((int64_t)UINT64_C(4)));
  t184 = ((int64_t)((*t183)));
  /* FG_DIRECT_SLICE_INDEX */
  t185 = ((int64_t)UINT64_C(0));
  t186 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t185, ((int64_t)UINT64_C(4)));
  t187 = ((int64_t)((*t186)));
  t188 = ((int64_t)gg_div(t184, t187));
  t181 = 0;
label18:;
  if (t181 >= t188) goto label20;
  v31 = t181;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t190 = ((int64_t)UINT64_C(0));
  t191 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t190, ((int64_t)UINT64_C(4)));
  t192 = (*t191);
  t189 = 0;
label21:;
  if (t189 >= t192) goto label23;
  v32 = t189;
  {
  t193 = ((int64_t)(v10));
  t194 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t195 = ((int64_t)UINT64_C(0));
  t196 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t195, ((int64_t)UINT64_C(4)));
  t197 = ((int64_t)((*t196)));
  t198 = ((int64_t)(((int64_t)((uint64_t)(t194) * (uint64_t)(t197)))));
  t199 = ((int64_t)(((int64_t)((uint64_t)(t193) + (uint64_t)(t198)))));
  t200 = ((int64_t)(v32));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t199) + (uint64_t)(t200)))));
  t201 = ((int64_t)(v11));
  t202 = ((int64_t)(v32));
  /* FG_DIRECT_SLICE_INDEX */
  t203 = ((int64_t)UINT64_C(0));
  t204 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t203, ((int64_t)UINT64_C(4)));
  t205 = ((int64_t)((*t204)));
  t206 = ((int64_t)(((int64_t)((uint64_t)(t202) * (uint64_t)(t205)))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t201) + (uint64_t)(t206)))));
  t207 = ((int64_t)(v12));
  t208 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t209 = ((int64_t)UINT64_C(0));
  t210 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t209, ((int64_t)UINT64_C(4)));
  t211 = ((int64_t)((*t210)));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t208) * (uint64_t)(t211)))));
  t213 = ((int64_t)(((int64_t)((uint64_t)(t207) + (uint64_t)(t212)))));
  t214 = ((int64_t)(v32));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t213) + (uint64_t)(t214)))));
  /* FG_DIRECT_SLICE_INDEX */
  t215 = v33;
  t216 = ((uint16_t *)(v2).data) + gg_index(t215, (v2).len);
  t217 = ((uint16_t)((*t216)));
  t218 = gg_function_11(t217, base + 0);
  t219 = ((float)(t218));
  /* FG_DIRECT_SLICE_INDEX */
  t220 = v34;
  t221 = ((float *)(v3).data) + gg_index(t220, (v3).len);
  t222 = ((float)((*t221)));
  v36 = ((float)(((t219) / (t222))));
  /* FG_DIRECT_SLICE_INDEX */
  t223 = v35;
  t224 = ((uint16_t *)(v6).data) + gg_index(t223, (v6).len);
  t225 = ((float)(v36));
  (*t224) = gg_function_13(t225, base + 0);
  }
label22:;
  t189++; goto label21;
label23:;
  }
label19:;
  t181++; goto label18;
label20:;
  }
  } else {
  t226 = ((int64_t)((((*(v0))).f3).f2));
  t227 = ((int64_t)(((int64_t)UINT64_C(2))));
  t228 = ((t226) == (t227));
  t229 = t228;
  if (t229) {
  t230 = ((int64_t)((((*(v0))).f4).f2));
  t231 = ((int64_t)(((int64_t)UINT64_C(0))));
  t229 = ((t230) == (t231));
  }
  t232 = t229;
  t233 = t232;
  if (t233) {
  t234 = ((int64_t)((((*(v0))).f5).f2));
  t235 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((t234) == (t235));
  }
  t236 = t233;
  if (t236) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t238 = ((int64_t)UINT64_C(0));
  t239 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t238, ((int64_t)UINT64_C(4)));
  t240 = ((int64_t)((*t239)));
  /* FG_DIRECT_SLICE_INDEX */
  t241 = ((int64_t)UINT64_C(0));
  t242 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t241, ((int64_t)UINT64_C(4)));
  t243 = ((int64_t)((*t242)));
  t244 = ((int64_t)gg_div(t240, t243));
  t237 = 0;
label24:;
  if (t237 >= t244) goto label26;
  v37 = t237;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t246 = ((int64_t)UINT64_C(0));
  t247 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t246, ((int64_t)UINT64_C(4)));
  t248 = (*t247);
  t245 = 0;
label27:;
  if (t245 >= t248) goto label29;
  v38 = t245;
  {
  t249 = ((int64_t)(v10));
  t250 = ((int64_t)(v37));
  /* FG_DIRECT_SLICE_INDEX */
  t251 = ((int64_t)UINT64_C(0));
  t252 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t251, ((int64_t)UINT64_C(4)));
  t253 = ((int64_t)((*t252)));
  t254 = ((int64_t)(((int64_t)((uint64_t)(t250) * (uint64_t)(t253)))));
  t255 = ((int64_t)(((int64_t)((uint64_t)(t249) + (uint64_t)(t254)))));
  t256 = ((int64_t)(v38));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t255) + (uint64_t)(t256)))));
  t257 = ((int64_t)(v11));
  t258 = ((int64_t)(v38));
  /* FG_DIRECT_SLICE_INDEX */
  t259 = ((int64_t)UINT64_C(0));
  t260 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t259, ((int64_t)UINT64_C(4)));
  t261 = ((int64_t)((*t260)));
  t262 = ((int64_t)(((int64_t)((uint64_t)(t258) * (uint64_t)(t261)))));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t257) + (uint64_t)(t262)))));
  t263 = ((int64_t)(v12));
  t264 = ((int64_t)(v37));
  /* FG_DIRECT_SLICE_INDEX */
  t265 = ((int64_t)UINT64_C(0));
  t266 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t265, ((int64_t)UINT64_C(4)));
  t267 = ((int64_t)((*t266)));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t264) * (uint64_t)(t267)))));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t263) + (uint64_t)(t268)))));
  t270 = ((int64_t)(v38));
  v41 = ((int64_t)(((int64_t)((uint64_t)(t269) + (uint64_t)(t270)))));
  /* FG_DIRECT_SLICE_INDEX */
  t271 = v39;
  t272 = ((uint16_t *)(v2).data) + gg_index(t271, (v2).len);
  t273 = ((uint16_t)((*t272)));
  t274 = gg_function_11(t273, base + 0);
  t275 = ((float)(t274));
  /* FG_DIRECT_SLICE_INDEX */
  t276 = v40;
  t277 = ((float *)(v3).data) + gg_index(t276, (v3).len);
  t278 = ((float)((*t277)));
  v42 = ((float)(((t275) / (t278))));
  /* FG_DIRECT_SLICE_INDEX */
  t279 = v41;
  t280 = ((float *)(v5).data) + gg_index(t279, (v5).len);
  (*t280) = ((float)(v42));
  }
label28:;
  t245++; goto label27;
label29:;
  }
label25:;
  t237++; goto label24;
label26:;
  }
  } else {
  t281 = ((int64_t)((((*(v0))).f3).f2));
  t282 = ((int64_t)(((int64_t)UINT64_C(1))));
  t283 = ((t281) == (t282));
  t284 = t283;
  if (t284) {
  t285 = ((int64_t)((((*(v0))).f4).f2));
  t286 = ((int64_t)(((int64_t)UINT64_C(0))));
  t284 = ((t285) == (t286));
  }
  t287 = t284;
  t288 = t287;
  if (t288) {
  t289 = ((int64_t)((((*(v0))).f5).f2));
  t290 = ((int64_t)(((int64_t)UINT64_C(1))));
  t288 = ((t289) == (t290));
  }
  t291 = t288;
  if (t291) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t293 = ((int64_t)UINT64_C(0));
  t294 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t293, ((int64_t)UINT64_C(4)));
  t295 = ((int64_t)((*t294)));
  /* FG_DIRECT_SLICE_INDEX */
  t296 = ((int64_t)UINT64_C(0));
  t297 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t296, ((int64_t)UINT64_C(4)));
  t298 = ((int64_t)((*t297)));
  t299 = ((int64_t)gg_div(t295, t298));
  t292 = 0;
label30:;
  if (t292 >= t299) goto label32;
  v43 = t292;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t301 = ((int64_t)UINT64_C(0));
  t302 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t301, ((int64_t)UINT64_C(4)));
  t303 = (*t302);
  t300 = 0;
label33:;
  if (t300 >= t303) goto label35;
  v44 = t300;
  {
  t304 = ((int64_t)(v10));
  t305 = ((int64_t)(v43));
  /* FG_DIRECT_SLICE_INDEX */
  t306 = ((int64_t)UINT64_C(0));
  t307 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t306, ((int64_t)UINT64_C(4)));
  t308 = ((int64_t)((*t307)));
  t309 = ((int64_t)(((int64_t)((uint64_t)(t305) * (uint64_t)(t308)))));
  t310 = ((int64_t)(((int64_t)((uint64_t)(t304) + (uint64_t)(t309)))));
  t311 = ((int64_t)(v44));
  v45 = ((int64_t)(((int64_t)((uint64_t)(t310) + (uint64_t)(t311)))));
  t312 = ((int64_t)(v11));
  t313 = ((int64_t)(v44));
  /* FG_DIRECT_SLICE_INDEX */
  t314 = ((int64_t)UINT64_C(0));
  t315 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t314, ((int64_t)UINT64_C(4)));
  t316 = ((int64_t)((*t315)));
  t317 = ((int64_t)(((int64_t)((uint64_t)(t313) * (uint64_t)(t316)))));
  v46 = ((int64_t)(((int64_t)((uint64_t)(t312) + (uint64_t)(t317)))));
  t318 = ((int64_t)(v12));
  t319 = ((int64_t)(v43));
  /* FG_DIRECT_SLICE_INDEX */
  t320 = ((int64_t)UINT64_C(0));
  t321 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t320, ((int64_t)UINT64_C(4)));
  t322 = ((int64_t)((*t321)));
  t323 = ((int64_t)(((int64_t)((uint64_t)(t319) * (uint64_t)(t322)))));
  t324 = ((int64_t)(((int64_t)((uint64_t)(t318) + (uint64_t)(t323)))));
  t325 = ((int64_t)(v44));
  v47 = ((int64_t)(((int64_t)((uint64_t)(t324) + (uint64_t)(t325)))));
  /* FG_DIRECT_SLICE_INDEX */
  t326 = v45;
  t327 = ((uint16_t *)(v2).data) + gg_index(t326, (v2).len);
  t328 = ((uint16_t)((*t327)));
  t329 = fg_el_half(t328);
  t330 = ((float)(t329));
  /* FG_DIRECT_SLICE_INDEX */
  t331 = v46;
  t332 = ((float *)(v3).data) + gg_index(t331, (v3).len);
  t333 = ((float)((*t332)));
  v48 = ((float)(((t330) / (t333))));
  /* FG_DIRECT_SLICE_INDEX */
  t334 = v47;
  t335 = ((uint16_t *)(v6).data) + gg_index(t334, (v6).len);
  t336 = ((float)(v48));
  t337 = fg_el_to_half(t336);
  (*t335) = ((uint16_t)(t337));
  }
label34:;
  t300++; goto label33;
label35:;
  }
label31:;
  t292++; goto label30;
label32:;
  }
  } else {
  t338 = ((int64_t)((((*(v0))).f3).f2));
  t339 = ((int64_t)(((int64_t)UINT64_C(1))));
  t340 = ((t338) == (t339));
  t341 = t340;
  if (t341) {
  t342 = ((int64_t)((((*(v0))).f4).f2));
  t343 = ((int64_t)(((int64_t)UINT64_C(0))));
  t341 = ((t342) == (t343));
  }
  t344 = t341;
  t345 = t344;
  if (t345) {
  t346 = ((int64_t)((((*(v0))).f5).f2));
  t347 = ((int64_t)(((int64_t)UINT64_C(0))));
  t345 = ((t346) == (t347));
  }
  t348 = t345;
  if (t348) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t350 = ((int64_t)UINT64_C(0));
  t351 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t350, ((int64_t)UINT64_C(4)));
  t352 = ((int64_t)((*t351)));
  /* FG_DIRECT_SLICE_INDEX */
  t353 = ((int64_t)UINT64_C(0));
  t354 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t353, ((int64_t)UINT64_C(4)));
  t355 = ((int64_t)((*t354)));
  t356 = ((int64_t)gg_div(t352, t355));
  t349 = 0;
label36:;
  if (t349 >= t356) goto label38;
  v49 = t349;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t358 = ((int64_t)UINT64_C(0));
  t359 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t358, ((int64_t)UINT64_C(4)));
  t360 = (*t359);
  t357 = 0;
label39:;
  if (t357 >= t360) goto label41;
  v50 = t357;
  {
  t361 = ((int64_t)(v10));
  t362 = ((int64_t)(v49));
  /* FG_DIRECT_SLICE_INDEX */
  t363 = ((int64_t)UINT64_C(0));
  t364 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t363, ((int64_t)UINT64_C(4)));
  t365 = ((int64_t)((*t364)));
  t366 = ((int64_t)(((int64_t)((uint64_t)(t362) * (uint64_t)(t365)))));
  t367 = ((int64_t)(((int64_t)((uint64_t)(t361) + (uint64_t)(t366)))));
  t368 = ((int64_t)(v50));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t367) + (uint64_t)(t368)))));
  t369 = ((int64_t)(v11));
  t370 = ((int64_t)(v50));
  /* FG_DIRECT_SLICE_INDEX */
  t371 = ((int64_t)UINT64_C(0));
  t372 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t371, ((int64_t)UINT64_C(4)));
  t373 = ((int64_t)((*t372)));
  t374 = ((int64_t)(((int64_t)((uint64_t)(t370) * (uint64_t)(t373)))));
  v52 = ((int64_t)(((int64_t)((uint64_t)(t369) + (uint64_t)(t374)))));
  t375 = ((int64_t)(v12));
  t376 = ((int64_t)(v49));
  /* FG_DIRECT_SLICE_INDEX */
  t377 = ((int64_t)UINT64_C(0));
  t378 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t377, ((int64_t)UINT64_C(4)));
  t379 = ((int64_t)((*t378)));
  t380 = ((int64_t)(((int64_t)((uint64_t)(t376) * (uint64_t)(t379)))));
  t381 = ((int64_t)(((int64_t)((uint64_t)(t375) + (uint64_t)(t380)))));
  t382 = ((int64_t)(v50));
  v53 = ((int64_t)(((int64_t)((uint64_t)(t381) + (uint64_t)(t382)))));
  /* FG_DIRECT_SLICE_INDEX */
  t383 = v51;
  t384 = ((uint16_t *)(v2).data) + gg_index(t383, (v2).len);
  t385 = ((uint16_t)((*t384)));
  t386 = fg_el_half(t385);
  t387 = ((float)(t386));
  /* FG_DIRECT_SLICE_INDEX */
  t388 = v52;
  t389 = ((float *)(v3).data) + gg_index(t388, (v3).len);
  t390 = ((float)((*t389)));
  v54 = ((float)(((t387) / (t390))));
  /* FG_DIRECT_SLICE_INDEX */
  t391 = v53;
  t392 = ((float *)(v5).data) + gg_index(t391, (v5).len);
  (*t392) = ((float)(v54));
  }
label40:;
  t357++; goto label39;
label41:;
  }
label37:;
  t349++; goto label36;
label38:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
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
static void gg_function_53(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  int64_t * t13;
  int64_t t14;
  int64_t t15;
  int64_t * t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t * t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t * t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t * t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t * t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  float * t46;
  float t47;
  int64_t t48;
  float * t49;
  float t50;
  int64_t t51;
  float * t52;
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
  int64_t t64;
  int64_t t65;
  int64_t * t66;
  int64_t t67;
  int64_t t68;
  int64_t * t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t * t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t * t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t * t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t * t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint16_t * t99;
  uint16_t t100;
  float t101;
  float t102;
  int64_t t103;
  uint16_t * t104;
  uint16_t t105;
  float t106;
  float t107;
  int64_t t108;
  uint16_t * t109;
  float t110;
  uint16_t t111;
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
  int64_t t123;
  int64_t t124;
  int64_t * t125;
  int64_t t126;
  int64_t t127;
  int64_t * t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t * t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t * t138;
  int64_t t139;
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
  int64_t t150;
  int64_t t151;
  int64_t * t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  uint16_t * t158;
  uint16_t t159;
  float t160;
  float t161;
  int64_t t162;
  uint16_t * t163;
  uint16_t t164;
  float t165;
  float t166;
  int64_t t167;
  uint16_t * t168;
  float t169;
  int64_t t170;
  int64_t t171;
  uint8_t t172;
  uint8_t t173;
  int64_t t174;
  int64_t t175;
  uint8_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  int64_t * t183;
  int64_t t184;
  int64_t t185;
  int64_t * t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t * t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t * t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t * t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t * t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  uint16_t * t216;
  uint16_t t217;
  float t218;
  float t219;
  int64_t t220;
  float * t221;
  float t222;
  int64_t t223;
  uint16_t * t224;
  float t225;
  int64_t t226;
  int64_t t227;
  uint8_t t228;
  uint8_t t229;
  int64_t t230;
  int64_t t231;
  uint8_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  uint8_t t236;
  int64_t t237;
  int64_t t238;
  int64_t * t239;
  int64_t t240;
  int64_t t241;
  int64_t * t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t * t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t * t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t * t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t * t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  uint16_t * t272;
  uint16_t t273;
  float t274;
  float t275;
  int64_t t276;
  float * t277;
  float t278;
  int64_t t279;
  float * t280;
  int64_t t281;
  int64_t t282;
  uint8_t t283;
  uint8_t t284;
  int64_t t285;
  int64_t t286;
  uint8_t t287;
  uint8_t t288;
  int64_t t289;
  int64_t t290;
  uint8_t t291;
  int64_t t292;
  int64_t t293;
  int64_t * t294;
  int64_t t295;
  int64_t t296;
  int64_t * t297;
  int64_t t298;
  int64_t t299;
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
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t * t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t * t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  uint16_t * t327;
  uint16_t t328;
  float t329;
  float t330;
  int64_t t331;
  float * t332;
  float t333;
  int64_t t334;
  uint16_t * t335;
  float t336;
  uint16_t t337;
  int64_t t338;
  int64_t t339;
  uint8_t t340;
  uint8_t t341;
  int64_t t342;
  int64_t t343;
  uint8_t t344;
  uint8_t t345;
  int64_t t346;
  int64_t t347;
  uint8_t t348;
  int64_t t349;
  int64_t t350;
  int64_t * t351;
  int64_t t352;
  int64_t t353;
  int64_t * t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t * t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t * t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  int64_t * t372;
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
  uint16_t * t384;
  uint16_t t385;
  float t386;
  float t387;
  int64_t t388;
  float * t389;
  float t390;
  int64_t t391;
  float * t392;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  float v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  float v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  float v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  float v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
  float v42;
  int64_t v43;
  int64_t v44;
  int64_t v45;
  int64_t v46;
  int64_t v47;
  float v48;
  int64_t v49;
  int64_t v50;
  int64_t v51;
  int64_t v52;
  int64_t v53;
  float v54;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f4).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)((((*(v0))).f5).f2));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) == (t9));
  }
  t10 = t7;
  if (t10) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t12 = ((int64_t)UINT64_C(0));
  t13 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t12, ((int64_t)UINT64_C(4)));
  t14 = ((int64_t)((*t13)));
  /* FG_DIRECT_SLICE_INDEX */
  t15 = ((int64_t)UINT64_C(0));
  t16 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t15, ((int64_t)UINT64_C(4)));
  t17 = ((int64_t)((*t16)));
  t18 = ((int64_t)gg_div(t14, t17));
  t11 = 0;
label0:;
  if (t11 >= t18) goto label2;
  v13 = t11;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t20 = ((int64_t)UINT64_C(0));
  t21 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t20, ((int64_t)UINT64_C(4)));
  t22 = (*t21);
  t19 = 0;
label3:;
  if (t19 >= t22) goto label5;
  v14 = t19;
  {
  t23 = ((int64_t)(v10));
  t24 = ((int64_t)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t25 = ((int64_t)UINT64_C(0));
  t26 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t25, ((int64_t)UINT64_C(4)));
  t27 = ((int64_t)((*t26)));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t27)))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t28)))));
  t30 = ((int64_t)(v14));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t31 = ((int64_t)(v11));
  t32 = ((int64_t)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t33 = ((int64_t)UINT64_C(0));
  t34 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t33, ((int64_t)UINT64_C(4)));
  t35 = ((int64_t)((*t34)));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t35)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t36)))));
  t37 = ((int64_t)(v12));
  t38 = ((int64_t)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t39 = ((int64_t)UINT64_C(0));
  t40 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t39, ((int64_t)UINT64_C(4)));
  t41 = ((int64_t)((*t40)));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t41)))));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t42)))));
  t44 = ((int64_t)(v14));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t44)))));
  /* FG_DIRECT_SLICE_INDEX */
  t45 = v15;
  t46 = ((float *)(v1).data) + gg_index(t45, (v1).len);
  t47 = ((float)((*t46)));
  /* FG_DIRECT_SLICE_INDEX */
  t48 = v16;
  t49 = ((float *)(v3).data) + gg_index(t48, (v3).len);
  t50 = ((float)((*t49)));
  v18 = ((float)(((t47) * (t50))));
  /* FG_DIRECT_SLICE_INDEX */
  t51 = v17;
  t52 = ((float *)(v5).data) + gg_index(t51, (v5).len);
  (*t52) = ((float)(v18));
  }
label4:;
  t19++; goto label3;
label5:;
  }
label1:;
  t11++; goto label0;
label2:;
  }
  } else {
  t53 = ((int64_t)((((*(v0))).f3).f2));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((t53) == (t54));
  t56 = t55;
  if (t56) {
  t57 = ((int64_t)((((*(v0))).f4).f2));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t56 = ((t57) == (t58));
  }
  t59 = t56;
  t60 = t59;
  if (t60) {
  t61 = ((int64_t)((((*(v0))).f5).f2));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((t61) == (t62));
  }
  t63 = t60;
  if (t63) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t65 = ((int64_t)UINT64_C(0));
  t66 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t65, ((int64_t)UINT64_C(4)));
  t67 = ((int64_t)((*t66)));
  /* FG_DIRECT_SLICE_INDEX */
  t68 = ((int64_t)UINT64_C(0));
  t69 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t68, ((int64_t)UINT64_C(4)));
  t70 = ((int64_t)((*t69)));
  t71 = ((int64_t)gg_div(t67, t70));
  t64 = 0;
label6:;
  if (t64 >= t71) goto label8;
  v19 = t64;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t73 = ((int64_t)UINT64_C(0));
  t74 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t73, ((int64_t)UINT64_C(4)));
  t75 = (*t74);
  t72 = 0;
label9:;
  if (t72 >= t75) goto label11;
  v20 = t72;
  {
  t76 = ((int64_t)(v10));
  t77 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t78 = ((int64_t)UINT64_C(0));
  t79 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t78, ((int64_t)UINT64_C(4)));
  t80 = ((int64_t)((*t79)));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t81)))));
  t83 = ((int64_t)(v20));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  t84 = ((int64_t)(v11));
  t85 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t86 = ((int64_t)UINT64_C(0));
  t87 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t86, ((int64_t)UINT64_C(4)));
  t88 = ((int64_t)((*t87)));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t88)))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t89)))));
  t90 = ((int64_t)(v12));
  t91 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t92 = ((int64_t)UINT64_C(0));
  t93 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t92, ((int64_t)UINT64_C(4)));
  t94 = ((int64_t)((*t93)));
  t95 = ((int64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t94)))));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t95)))));
  t97 = ((int64_t)(v20));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  /* FG_DIRECT_SLICE_INDEX */
  t98 = v21;
  t99 = ((uint16_t *)(v2).data) + gg_index(t98, (v2).len);
  t100 = ((uint16_t)((*t99)));
  t101 = fg_el_half(t100);
  t102 = ((float)(t101));
  /* FG_DIRECT_SLICE_INDEX */
  t103 = v22;
  t104 = ((uint16_t *)(v4).data) + gg_index(t103, (v4).len);
  t105 = ((uint16_t)((*t104)));
  t106 = fg_el_half(t105);
  t107 = ((float)(t106));
  v24 = ((float)(((t102) * (t107))));
  /* FG_DIRECT_SLICE_INDEX */
  t108 = v23;
  t109 = ((uint16_t *)(v6).data) + gg_index(t108, (v6).len);
  t110 = ((float)(v24));
  t111 = fg_el_to_half(t110);
  (*t109) = ((uint16_t)(t111));
  }
label10:;
  t72++; goto label9;
label11:;
  }
label7:;
  t64++; goto label6;
label8:;
  }
  } else {
  t112 = ((int64_t)((((*(v0))).f3).f2));
  t113 = ((int64_t)(((int64_t)UINT64_C(2))));
  t114 = ((t112) == (t113));
  t115 = t114;
  if (t115) {
  t116 = ((int64_t)((((*(v0))).f4).f2));
  t117 = ((int64_t)(((int64_t)UINT64_C(2))));
  t115 = ((t116) == (t117));
  }
  t118 = t115;
  t119 = t118;
  if (t119) {
  t120 = ((int64_t)((((*(v0))).f5).f2));
  t121 = ((int64_t)(((int64_t)UINT64_C(2))));
  t119 = ((t120) == (t121));
  }
  t122 = t119;
  if (t122) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t124 = ((int64_t)UINT64_C(0));
  t125 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t124, ((int64_t)UINT64_C(4)));
  t126 = ((int64_t)((*t125)));
  /* FG_DIRECT_SLICE_INDEX */
  t127 = ((int64_t)UINT64_C(0));
  t128 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t127, ((int64_t)UINT64_C(4)));
  t129 = ((int64_t)((*t128)));
  t130 = ((int64_t)gg_div(t126, t129));
  t123 = 0;
label12:;
  if (t123 >= t130) goto label14;
  v25 = t123;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t132 = ((int64_t)UINT64_C(0));
  t133 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t132, ((int64_t)UINT64_C(4)));
  t134 = (*t133);
  t131 = 0;
label15:;
  if (t131 >= t134) goto label17;
  v26 = t131;
  {
  t135 = ((int64_t)(v10));
  t136 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t137 = ((int64_t)UINT64_C(0));
  t138 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t137, ((int64_t)UINT64_C(4)));
  t139 = ((int64_t)((*t138)));
  t140 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t139)))));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t135) + (uint64_t)(t140)))));
  t142 = ((int64_t)(v26));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t141) + (uint64_t)(t142)))));
  t143 = ((int64_t)(v11));
  t144 = ((int64_t)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t145 = ((int64_t)UINT64_C(0));
  t146 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t145, ((int64_t)UINT64_C(4)));
  t147 = ((int64_t)((*t146)));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t147)))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t143) + (uint64_t)(t148)))));
  t149 = ((int64_t)(v12));
  t150 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t151 = ((int64_t)UINT64_C(0));
  t152 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t151, ((int64_t)UINT64_C(4)));
  t153 = ((int64_t)((*t152)));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t150) * (uint64_t)(t153)))));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t149) + (uint64_t)(t154)))));
  t156 = ((int64_t)(v26));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t155) + (uint64_t)(t156)))));
  /* FG_DIRECT_SLICE_INDEX */
  t157 = v27;
  t158 = ((uint16_t *)(v2).data) + gg_index(t157, (v2).len);
  t159 = ((uint16_t)((*t158)));
  t160 = gg_function_11(t159, base + 0);
  t161 = ((float)(t160));
  /* FG_DIRECT_SLICE_INDEX */
  t162 = v28;
  t163 = ((uint16_t *)(v4).data) + gg_index(t162, (v4).len);
  t164 = ((uint16_t)((*t163)));
  t165 = gg_function_11(t164, base + 0);
  t166 = ((float)(t165));
  v30 = ((float)(((t161) * (t166))));
  /* FG_DIRECT_SLICE_INDEX */
  t167 = v29;
  t168 = ((uint16_t *)(v6).data) + gg_index(t167, (v6).len);
  t169 = ((float)(v30));
  (*t168) = gg_function_13(t169, base + 0);
  }
label16:;
  t131++; goto label15;
label17:;
  }
label13:;
  t123++; goto label12;
label14:;
  }
  } else {
  t170 = ((int64_t)((((*(v0))).f3).f2));
  t171 = ((int64_t)(((int64_t)UINT64_C(2))));
  t172 = ((t170) == (t171));
  t173 = t172;
  if (t173) {
  t174 = ((int64_t)((((*(v0))).f4).f2));
  t175 = ((int64_t)(((int64_t)UINT64_C(0))));
  t173 = ((t174) == (t175));
  }
  t176 = t173;
  t177 = t176;
  if (t177) {
  t178 = ((int64_t)((((*(v0))).f5).f2));
  t179 = ((int64_t)(((int64_t)UINT64_C(2))));
  t177 = ((t178) == (t179));
  }
  t180 = t177;
  if (t180) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t182 = ((int64_t)UINT64_C(0));
  t183 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t182, ((int64_t)UINT64_C(4)));
  t184 = ((int64_t)((*t183)));
  /* FG_DIRECT_SLICE_INDEX */
  t185 = ((int64_t)UINT64_C(0));
  t186 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t185, ((int64_t)UINT64_C(4)));
  t187 = ((int64_t)((*t186)));
  t188 = ((int64_t)gg_div(t184, t187));
  t181 = 0;
label18:;
  if (t181 >= t188) goto label20;
  v31 = t181;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t190 = ((int64_t)UINT64_C(0));
  t191 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t190, ((int64_t)UINT64_C(4)));
  t192 = (*t191);
  t189 = 0;
label21:;
  if (t189 >= t192) goto label23;
  v32 = t189;
  {
  t193 = ((int64_t)(v10));
  t194 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t195 = ((int64_t)UINT64_C(0));
  t196 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t195, ((int64_t)UINT64_C(4)));
  t197 = ((int64_t)((*t196)));
  t198 = ((int64_t)(((int64_t)((uint64_t)(t194) * (uint64_t)(t197)))));
  t199 = ((int64_t)(((int64_t)((uint64_t)(t193) + (uint64_t)(t198)))));
  t200 = ((int64_t)(v32));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t199) + (uint64_t)(t200)))));
  t201 = ((int64_t)(v11));
  t202 = ((int64_t)(v32));
  /* FG_DIRECT_SLICE_INDEX */
  t203 = ((int64_t)UINT64_C(0));
  t204 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t203, ((int64_t)UINT64_C(4)));
  t205 = ((int64_t)((*t204)));
  t206 = ((int64_t)(((int64_t)((uint64_t)(t202) * (uint64_t)(t205)))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t201) + (uint64_t)(t206)))));
  t207 = ((int64_t)(v12));
  t208 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t209 = ((int64_t)UINT64_C(0));
  t210 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t209, ((int64_t)UINT64_C(4)));
  t211 = ((int64_t)((*t210)));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t208) * (uint64_t)(t211)))));
  t213 = ((int64_t)(((int64_t)((uint64_t)(t207) + (uint64_t)(t212)))));
  t214 = ((int64_t)(v32));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t213) + (uint64_t)(t214)))));
  /* FG_DIRECT_SLICE_INDEX */
  t215 = v33;
  t216 = ((uint16_t *)(v2).data) + gg_index(t215, (v2).len);
  t217 = ((uint16_t)((*t216)));
  t218 = gg_function_11(t217, base + 0);
  t219 = ((float)(t218));
  /* FG_DIRECT_SLICE_INDEX */
  t220 = v34;
  t221 = ((float *)(v3).data) + gg_index(t220, (v3).len);
  t222 = ((float)((*t221)));
  v36 = ((float)(((t219) * (t222))));
  /* FG_DIRECT_SLICE_INDEX */
  t223 = v35;
  t224 = ((uint16_t *)(v6).data) + gg_index(t223, (v6).len);
  t225 = ((float)(v36));
  (*t224) = gg_function_13(t225, base + 0);
  }
label22:;
  t189++; goto label21;
label23:;
  }
label19:;
  t181++; goto label18;
label20:;
  }
  } else {
  t226 = ((int64_t)((((*(v0))).f3).f2));
  t227 = ((int64_t)(((int64_t)UINT64_C(2))));
  t228 = ((t226) == (t227));
  t229 = t228;
  if (t229) {
  t230 = ((int64_t)((((*(v0))).f4).f2));
  t231 = ((int64_t)(((int64_t)UINT64_C(0))));
  t229 = ((t230) == (t231));
  }
  t232 = t229;
  t233 = t232;
  if (t233) {
  t234 = ((int64_t)((((*(v0))).f5).f2));
  t235 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((t234) == (t235));
  }
  t236 = t233;
  if (t236) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t238 = ((int64_t)UINT64_C(0));
  t239 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t238, ((int64_t)UINT64_C(4)));
  t240 = ((int64_t)((*t239)));
  /* FG_DIRECT_SLICE_INDEX */
  t241 = ((int64_t)UINT64_C(0));
  t242 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t241, ((int64_t)UINT64_C(4)));
  t243 = ((int64_t)((*t242)));
  t244 = ((int64_t)gg_div(t240, t243));
  t237 = 0;
label24:;
  if (t237 >= t244) goto label26;
  v37 = t237;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t246 = ((int64_t)UINT64_C(0));
  t247 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t246, ((int64_t)UINT64_C(4)));
  t248 = (*t247);
  t245 = 0;
label27:;
  if (t245 >= t248) goto label29;
  v38 = t245;
  {
  t249 = ((int64_t)(v10));
  t250 = ((int64_t)(v37));
  /* FG_DIRECT_SLICE_INDEX */
  t251 = ((int64_t)UINT64_C(0));
  t252 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t251, ((int64_t)UINT64_C(4)));
  t253 = ((int64_t)((*t252)));
  t254 = ((int64_t)(((int64_t)((uint64_t)(t250) * (uint64_t)(t253)))));
  t255 = ((int64_t)(((int64_t)((uint64_t)(t249) + (uint64_t)(t254)))));
  t256 = ((int64_t)(v38));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t255) + (uint64_t)(t256)))));
  t257 = ((int64_t)(v11));
  t258 = ((int64_t)(v38));
  /* FG_DIRECT_SLICE_INDEX */
  t259 = ((int64_t)UINT64_C(0));
  t260 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t259, ((int64_t)UINT64_C(4)));
  t261 = ((int64_t)((*t260)));
  t262 = ((int64_t)(((int64_t)((uint64_t)(t258) * (uint64_t)(t261)))));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t257) + (uint64_t)(t262)))));
  t263 = ((int64_t)(v12));
  t264 = ((int64_t)(v37));
  /* FG_DIRECT_SLICE_INDEX */
  t265 = ((int64_t)UINT64_C(0));
  t266 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t265, ((int64_t)UINT64_C(4)));
  t267 = ((int64_t)((*t266)));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t264) * (uint64_t)(t267)))));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t263) + (uint64_t)(t268)))));
  t270 = ((int64_t)(v38));
  v41 = ((int64_t)(((int64_t)((uint64_t)(t269) + (uint64_t)(t270)))));
  /* FG_DIRECT_SLICE_INDEX */
  t271 = v39;
  t272 = ((uint16_t *)(v2).data) + gg_index(t271, (v2).len);
  t273 = ((uint16_t)((*t272)));
  t274 = gg_function_11(t273, base + 0);
  t275 = ((float)(t274));
  /* FG_DIRECT_SLICE_INDEX */
  t276 = v40;
  t277 = ((float *)(v3).data) + gg_index(t276, (v3).len);
  t278 = ((float)((*t277)));
  v42 = ((float)(((t275) * (t278))));
  /* FG_DIRECT_SLICE_INDEX */
  t279 = v41;
  t280 = ((float *)(v5).data) + gg_index(t279, (v5).len);
  (*t280) = ((float)(v42));
  }
label28:;
  t245++; goto label27;
label29:;
  }
label25:;
  t237++; goto label24;
label26:;
  }
  } else {
  t281 = ((int64_t)((((*(v0))).f3).f2));
  t282 = ((int64_t)(((int64_t)UINT64_C(1))));
  t283 = ((t281) == (t282));
  t284 = t283;
  if (t284) {
  t285 = ((int64_t)((((*(v0))).f4).f2));
  t286 = ((int64_t)(((int64_t)UINT64_C(0))));
  t284 = ((t285) == (t286));
  }
  t287 = t284;
  t288 = t287;
  if (t288) {
  t289 = ((int64_t)((((*(v0))).f5).f2));
  t290 = ((int64_t)(((int64_t)UINT64_C(1))));
  t288 = ((t289) == (t290));
  }
  t291 = t288;
  if (t291) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t293 = ((int64_t)UINT64_C(0));
  t294 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t293, ((int64_t)UINT64_C(4)));
  t295 = ((int64_t)((*t294)));
  /* FG_DIRECT_SLICE_INDEX */
  t296 = ((int64_t)UINT64_C(0));
  t297 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t296, ((int64_t)UINT64_C(4)));
  t298 = ((int64_t)((*t297)));
  t299 = ((int64_t)gg_div(t295, t298));
  t292 = 0;
label30:;
  if (t292 >= t299) goto label32;
  v43 = t292;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t301 = ((int64_t)UINT64_C(0));
  t302 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t301, ((int64_t)UINT64_C(4)));
  t303 = (*t302);
  t300 = 0;
label33:;
  if (t300 >= t303) goto label35;
  v44 = t300;
  {
  t304 = ((int64_t)(v10));
  t305 = ((int64_t)(v43));
  /* FG_DIRECT_SLICE_INDEX */
  t306 = ((int64_t)UINT64_C(0));
  t307 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t306, ((int64_t)UINT64_C(4)));
  t308 = ((int64_t)((*t307)));
  t309 = ((int64_t)(((int64_t)((uint64_t)(t305) * (uint64_t)(t308)))));
  t310 = ((int64_t)(((int64_t)((uint64_t)(t304) + (uint64_t)(t309)))));
  t311 = ((int64_t)(v44));
  v45 = ((int64_t)(((int64_t)((uint64_t)(t310) + (uint64_t)(t311)))));
  t312 = ((int64_t)(v11));
  t313 = ((int64_t)(v44));
  /* FG_DIRECT_SLICE_INDEX */
  t314 = ((int64_t)UINT64_C(0));
  t315 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t314, ((int64_t)UINT64_C(4)));
  t316 = ((int64_t)((*t315)));
  t317 = ((int64_t)(((int64_t)((uint64_t)(t313) * (uint64_t)(t316)))));
  v46 = ((int64_t)(((int64_t)((uint64_t)(t312) + (uint64_t)(t317)))));
  t318 = ((int64_t)(v12));
  t319 = ((int64_t)(v43));
  /* FG_DIRECT_SLICE_INDEX */
  t320 = ((int64_t)UINT64_C(0));
  t321 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t320, ((int64_t)UINT64_C(4)));
  t322 = ((int64_t)((*t321)));
  t323 = ((int64_t)(((int64_t)((uint64_t)(t319) * (uint64_t)(t322)))));
  t324 = ((int64_t)(((int64_t)((uint64_t)(t318) + (uint64_t)(t323)))));
  t325 = ((int64_t)(v44));
  v47 = ((int64_t)(((int64_t)((uint64_t)(t324) + (uint64_t)(t325)))));
  /* FG_DIRECT_SLICE_INDEX */
  t326 = v45;
  t327 = ((uint16_t *)(v2).data) + gg_index(t326, (v2).len);
  t328 = ((uint16_t)((*t327)));
  t329 = fg_el_half(t328);
  t330 = ((float)(t329));
  /* FG_DIRECT_SLICE_INDEX */
  t331 = v46;
  t332 = ((float *)(v3).data) + gg_index(t331, (v3).len);
  t333 = ((float)((*t332)));
  v48 = ((float)(((t330) * (t333))));
  /* FG_DIRECT_SLICE_INDEX */
  t334 = v47;
  t335 = ((uint16_t *)(v6).data) + gg_index(t334, (v6).len);
  t336 = ((float)(v48));
  t337 = fg_el_to_half(t336);
  (*t335) = ((uint16_t)(t337));
  }
label34:;
  t300++; goto label33;
label35:;
  }
label31:;
  t292++; goto label30;
label32:;
  }
  } else {
  t338 = ((int64_t)((((*(v0))).f3).f2));
  t339 = ((int64_t)(((int64_t)UINT64_C(1))));
  t340 = ((t338) == (t339));
  t341 = t340;
  if (t341) {
  t342 = ((int64_t)((((*(v0))).f4).f2));
  t343 = ((int64_t)(((int64_t)UINT64_C(0))));
  t341 = ((t342) == (t343));
  }
  t344 = t341;
  t345 = t344;
  if (t345) {
  t346 = ((int64_t)((((*(v0))).f5).f2));
  t347 = ((int64_t)(((int64_t)UINT64_C(0))));
  t345 = ((t346) == (t347));
  }
  t348 = t345;
  if (t348) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t350 = ((int64_t)UINT64_C(0));
  t351 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t350, ((int64_t)UINT64_C(4)));
  t352 = ((int64_t)((*t351)));
  /* FG_DIRECT_SLICE_INDEX */
  t353 = ((int64_t)UINT64_C(0));
  t354 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t353, ((int64_t)UINT64_C(4)));
  t355 = ((int64_t)((*t354)));
  t356 = ((int64_t)gg_div(t352, t355));
  t349 = 0;
label36:;
  if (t349 >= t356) goto label38;
  v49 = t349;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t358 = ((int64_t)UINT64_C(0));
  t359 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t358, ((int64_t)UINT64_C(4)));
  t360 = (*t359);
  t357 = 0;
label39:;
  if (t357 >= t360) goto label41;
  v50 = t357;
  {
  t361 = ((int64_t)(v10));
  t362 = ((int64_t)(v49));
  /* FG_DIRECT_SLICE_INDEX */
  t363 = ((int64_t)UINT64_C(0));
  t364 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t363, ((int64_t)UINT64_C(4)));
  t365 = ((int64_t)((*t364)));
  t366 = ((int64_t)(((int64_t)((uint64_t)(t362) * (uint64_t)(t365)))));
  t367 = ((int64_t)(((int64_t)((uint64_t)(t361) + (uint64_t)(t366)))));
  t368 = ((int64_t)(v50));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t367) + (uint64_t)(t368)))));
  t369 = ((int64_t)(v11));
  t370 = ((int64_t)(v50));
  /* FG_DIRECT_SLICE_INDEX */
  t371 = ((int64_t)UINT64_C(0));
  t372 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t371, ((int64_t)UINT64_C(4)));
  t373 = ((int64_t)((*t372)));
  t374 = ((int64_t)(((int64_t)((uint64_t)(t370) * (uint64_t)(t373)))));
  v52 = ((int64_t)(((int64_t)((uint64_t)(t369) + (uint64_t)(t374)))));
  t375 = ((int64_t)(v12));
  t376 = ((int64_t)(v49));
  /* FG_DIRECT_SLICE_INDEX */
  t377 = ((int64_t)UINT64_C(0));
  t378 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t377, ((int64_t)UINT64_C(4)));
  t379 = ((int64_t)((*t378)));
  t380 = ((int64_t)(((int64_t)((uint64_t)(t376) * (uint64_t)(t379)))));
  t381 = ((int64_t)(((int64_t)((uint64_t)(t375) + (uint64_t)(t380)))));
  t382 = ((int64_t)(v50));
  v53 = ((int64_t)(((int64_t)((uint64_t)(t381) + (uint64_t)(t382)))));
  /* FG_DIRECT_SLICE_INDEX */
  t383 = v51;
  t384 = ((uint16_t *)(v2).data) + gg_index(t383, (v2).len);
  t385 = ((uint16_t)((*t384)));
  t386 = fg_el_half(t385);
  t387 = ((float)(t386));
  /* FG_DIRECT_SLICE_INDEX */
  t388 = v52;
  t389 = ((float *)(v3).data) + gg_index(t388, (v3).len);
  t390 = ((float)((*t389)));
  v54 = ((float)(((t387) * (t390))));
  /* FG_DIRECT_SLICE_INDEX */
  t391 = v53;
  t392 = ((float *)(v5).data) + gg_index(t391, (v5).len);
  (*t392) = ((float)(v54));
  }
label40:;
  t357++; goto label39;
label41:;
  }
label37:;
  t349++; goto label36;
label38:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
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
static void gg_function_52(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  int64_t * t13;
  int64_t t14;
  int64_t t15;
  int64_t * t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t * t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t * t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t * t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t * t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  float * t46;
  float t47;
  int64_t t48;
  float * t49;
  float t50;
  int64_t t51;
  float * t52;
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
  int64_t t64;
  int64_t t65;
  int64_t * t66;
  int64_t t67;
  int64_t t68;
  int64_t * t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t * t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t * t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t * t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t * t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint16_t * t99;
  uint16_t t100;
  float t101;
  float t102;
  int64_t t103;
  uint16_t * t104;
  uint16_t t105;
  float t106;
  float t107;
  int64_t t108;
  uint16_t * t109;
  float t110;
  uint16_t t111;
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
  int64_t t123;
  int64_t t124;
  int64_t * t125;
  int64_t t126;
  int64_t t127;
  int64_t * t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t * t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t * t138;
  int64_t t139;
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
  int64_t t150;
  int64_t t151;
  int64_t * t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  uint16_t * t158;
  uint16_t t159;
  float t160;
  float t161;
  int64_t t162;
  uint16_t * t163;
  uint16_t t164;
  float t165;
  float t166;
  int64_t t167;
  uint16_t * t168;
  float t169;
  int64_t t170;
  int64_t t171;
  uint8_t t172;
  uint8_t t173;
  int64_t t174;
  int64_t t175;
  uint8_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  int64_t * t183;
  int64_t t184;
  int64_t t185;
  int64_t * t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t * t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t * t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t * t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t * t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  uint16_t * t216;
  uint16_t t217;
  float t218;
  float t219;
  int64_t t220;
  float * t221;
  float t222;
  int64_t t223;
  uint16_t * t224;
  float t225;
  int64_t t226;
  int64_t t227;
  uint8_t t228;
  uint8_t t229;
  int64_t t230;
  int64_t t231;
  uint8_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  uint8_t t236;
  int64_t t237;
  int64_t t238;
  int64_t * t239;
  int64_t t240;
  int64_t t241;
  int64_t * t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t * t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t * t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t * t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t * t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  uint16_t * t272;
  uint16_t t273;
  float t274;
  float t275;
  int64_t t276;
  float * t277;
  float t278;
  int64_t t279;
  float * t280;
  int64_t t281;
  int64_t t282;
  uint8_t t283;
  uint8_t t284;
  int64_t t285;
  int64_t t286;
  uint8_t t287;
  uint8_t t288;
  int64_t t289;
  int64_t t290;
  uint8_t t291;
  int64_t t292;
  int64_t t293;
  int64_t * t294;
  int64_t t295;
  int64_t t296;
  int64_t * t297;
  int64_t t298;
  int64_t t299;
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
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t * t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t * t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  uint16_t * t327;
  uint16_t t328;
  float t329;
  float t330;
  int64_t t331;
  float * t332;
  float t333;
  int64_t t334;
  uint16_t * t335;
  float t336;
  uint16_t t337;
  int64_t t338;
  int64_t t339;
  uint8_t t340;
  uint8_t t341;
  int64_t t342;
  int64_t t343;
  uint8_t t344;
  uint8_t t345;
  int64_t t346;
  int64_t t347;
  uint8_t t348;
  int64_t t349;
  int64_t t350;
  int64_t * t351;
  int64_t t352;
  int64_t t353;
  int64_t * t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t * t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t * t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  int64_t * t372;
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
  uint16_t * t384;
  uint16_t t385;
  float t386;
  float t387;
  int64_t t388;
  float * t389;
  float t390;
  int64_t t391;
  float * t392;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  float v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  float v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  float v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  float v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
  float v42;
  int64_t v43;
  int64_t v44;
  int64_t v45;
  int64_t v46;
  int64_t v47;
  float v48;
  int64_t v49;
  int64_t v50;
  int64_t v51;
  int64_t v52;
  int64_t v53;
  float v54;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f4).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)((((*(v0))).f5).f2));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) == (t9));
  }
  t10 = t7;
  if (t10) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t12 = ((int64_t)UINT64_C(0));
  t13 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t12, ((int64_t)UINT64_C(4)));
  t14 = ((int64_t)((*t13)));
  /* FG_DIRECT_SLICE_INDEX */
  t15 = ((int64_t)UINT64_C(0));
  t16 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t15, ((int64_t)UINT64_C(4)));
  t17 = ((int64_t)((*t16)));
  t18 = ((int64_t)gg_div(t14, t17));
  t11 = 0;
label0:;
  if (t11 >= t18) goto label2;
  v13 = t11;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t20 = ((int64_t)UINT64_C(0));
  t21 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t20, ((int64_t)UINT64_C(4)));
  t22 = (*t21);
  t19 = 0;
label3:;
  if (t19 >= t22) goto label5;
  v14 = t19;
  {
  t23 = ((int64_t)(v10));
  t24 = ((int64_t)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t25 = ((int64_t)UINT64_C(0));
  t26 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t25, ((int64_t)UINT64_C(4)));
  t27 = ((int64_t)((*t26)));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t27)))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t28)))));
  t30 = ((int64_t)(v14));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t31 = ((int64_t)(v11));
  t32 = ((int64_t)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t33 = ((int64_t)UINT64_C(0));
  t34 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t33, ((int64_t)UINT64_C(4)));
  t35 = ((int64_t)((*t34)));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t35)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t36)))));
  t37 = ((int64_t)(v12));
  t38 = ((int64_t)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t39 = ((int64_t)UINT64_C(0));
  t40 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t39, ((int64_t)UINT64_C(4)));
  t41 = ((int64_t)((*t40)));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t41)))));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t42)))));
  t44 = ((int64_t)(v14));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t44)))));
  /* FG_DIRECT_SLICE_INDEX */
  t45 = v15;
  t46 = ((float *)(v1).data) + gg_index(t45, (v1).len);
  t47 = ((float)((*t46)));
  /* FG_DIRECT_SLICE_INDEX */
  t48 = v16;
  t49 = ((float *)(v3).data) + gg_index(t48, (v3).len);
  t50 = ((float)((*t49)));
  v18 = ((float)(((t47) - (t50))));
  /* FG_DIRECT_SLICE_INDEX */
  t51 = v17;
  t52 = ((float *)(v5).data) + gg_index(t51, (v5).len);
  (*t52) = ((float)(v18));
  }
label4:;
  t19++; goto label3;
label5:;
  }
label1:;
  t11++; goto label0;
label2:;
  }
  } else {
  t53 = ((int64_t)((((*(v0))).f3).f2));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((t53) == (t54));
  t56 = t55;
  if (t56) {
  t57 = ((int64_t)((((*(v0))).f4).f2));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t56 = ((t57) == (t58));
  }
  t59 = t56;
  t60 = t59;
  if (t60) {
  t61 = ((int64_t)((((*(v0))).f5).f2));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((t61) == (t62));
  }
  t63 = t60;
  if (t63) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t65 = ((int64_t)UINT64_C(0));
  t66 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t65, ((int64_t)UINT64_C(4)));
  t67 = ((int64_t)((*t66)));
  /* FG_DIRECT_SLICE_INDEX */
  t68 = ((int64_t)UINT64_C(0));
  t69 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t68, ((int64_t)UINT64_C(4)));
  t70 = ((int64_t)((*t69)));
  t71 = ((int64_t)gg_div(t67, t70));
  t64 = 0;
label6:;
  if (t64 >= t71) goto label8;
  v19 = t64;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t73 = ((int64_t)UINT64_C(0));
  t74 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t73, ((int64_t)UINT64_C(4)));
  t75 = (*t74);
  t72 = 0;
label9:;
  if (t72 >= t75) goto label11;
  v20 = t72;
  {
  t76 = ((int64_t)(v10));
  t77 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t78 = ((int64_t)UINT64_C(0));
  t79 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t78, ((int64_t)UINT64_C(4)));
  t80 = ((int64_t)((*t79)));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t81)))));
  t83 = ((int64_t)(v20));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  t84 = ((int64_t)(v11));
  t85 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t86 = ((int64_t)UINT64_C(0));
  t87 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t86, ((int64_t)UINT64_C(4)));
  t88 = ((int64_t)((*t87)));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t88)))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t89)))));
  t90 = ((int64_t)(v12));
  t91 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t92 = ((int64_t)UINT64_C(0));
  t93 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t92, ((int64_t)UINT64_C(4)));
  t94 = ((int64_t)((*t93)));
  t95 = ((int64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t94)))));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t95)))));
  t97 = ((int64_t)(v20));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  /* FG_DIRECT_SLICE_INDEX */
  t98 = v21;
  t99 = ((uint16_t *)(v2).data) + gg_index(t98, (v2).len);
  t100 = ((uint16_t)((*t99)));
  t101 = fg_el_half(t100);
  t102 = ((float)(t101));
  /* FG_DIRECT_SLICE_INDEX */
  t103 = v22;
  t104 = ((uint16_t *)(v4).data) + gg_index(t103, (v4).len);
  t105 = ((uint16_t)((*t104)));
  t106 = fg_el_half(t105);
  t107 = ((float)(t106));
  v24 = ((float)(((t102) - (t107))));
  /* FG_DIRECT_SLICE_INDEX */
  t108 = v23;
  t109 = ((uint16_t *)(v6).data) + gg_index(t108, (v6).len);
  t110 = ((float)(v24));
  t111 = fg_el_to_half(t110);
  (*t109) = ((uint16_t)(t111));
  }
label10:;
  t72++; goto label9;
label11:;
  }
label7:;
  t64++; goto label6;
label8:;
  }
  } else {
  t112 = ((int64_t)((((*(v0))).f3).f2));
  t113 = ((int64_t)(((int64_t)UINT64_C(2))));
  t114 = ((t112) == (t113));
  t115 = t114;
  if (t115) {
  t116 = ((int64_t)((((*(v0))).f4).f2));
  t117 = ((int64_t)(((int64_t)UINT64_C(2))));
  t115 = ((t116) == (t117));
  }
  t118 = t115;
  t119 = t118;
  if (t119) {
  t120 = ((int64_t)((((*(v0))).f5).f2));
  t121 = ((int64_t)(((int64_t)UINT64_C(2))));
  t119 = ((t120) == (t121));
  }
  t122 = t119;
  if (t122) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t124 = ((int64_t)UINT64_C(0));
  t125 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t124, ((int64_t)UINT64_C(4)));
  t126 = ((int64_t)((*t125)));
  /* FG_DIRECT_SLICE_INDEX */
  t127 = ((int64_t)UINT64_C(0));
  t128 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t127, ((int64_t)UINT64_C(4)));
  t129 = ((int64_t)((*t128)));
  t130 = ((int64_t)gg_div(t126, t129));
  t123 = 0;
label12:;
  if (t123 >= t130) goto label14;
  v25 = t123;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t132 = ((int64_t)UINT64_C(0));
  t133 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t132, ((int64_t)UINT64_C(4)));
  t134 = (*t133);
  t131 = 0;
label15:;
  if (t131 >= t134) goto label17;
  v26 = t131;
  {
  t135 = ((int64_t)(v10));
  t136 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t137 = ((int64_t)UINT64_C(0));
  t138 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t137, ((int64_t)UINT64_C(4)));
  t139 = ((int64_t)((*t138)));
  t140 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t139)))));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t135) + (uint64_t)(t140)))));
  t142 = ((int64_t)(v26));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t141) + (uint64_t)(t142)))));
  t143 = ((int64_t)(v11));
  t144 = ((int64_t)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t145 = ((int64_t)UINT64_C(0));
  t146 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t145, ((int64_t)UINT64_C(4)));
  t147 = ((int64_t)((*t146)));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t147)))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t143) + (uint64_t)(t148)))));
  t149 = ((int64_t)(v12));
  t150 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t151 = ((int64_t)UINT64_C(0));
  t152 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t151, ((int64_t)UINT64_C(4)));
  t153 = ((int64_t)((*t152)));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t150) * (uint64_t)(t153)))));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t149) + (uint64_t)(t154)))));
  t156 = ((int64_t)(v26));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t155) + (uint64_t)(t156)))));
  /* FG_DIRECT_SLICE_INDEX */
  t157 = v27;
  t158 = ((uint16_t *)(v2).data) + gg_index(t157, (v2).len);
  t159 = ((uint16_t)((*t158)));
  t160 = gg_function_11(t159, base + 0);
  t161 = ((float)(t160));
  /* FG_DIRECT_SLICE_INDEX */
  t162 = v28;
  t163 = ((uint16_t *)(v4).data) + gg_index(t162, (v4).len);
  t164 = ((uint16_t)((*t163)));
  t165 = gg_function_11(t164, base + 0);
  t166 = ((float)(t165));
  v30 = ((float)(((t161) - (t166))));
  /* FG_DIRECT_SLICE_INDEX */
  t167 = v29;
  t168 = ((uint16_t *)(v6).data) + gg_index(t167, (v6).len);
  t169 = ((float)(v30));
  (*t168) = gg_function_13(t169, base + 0);
  }
label16:;
  t131++; goto label15;
label17:;
  }
label13:;
  t123++; goto label12;
label14:;
  }
  } else {
  t170 = ((int64_t)((((*(v0))).f3).f2));
  t171 = ((int64_t)(((int64_t)UINT64_C(2))));
  t172 = ((t170) == (t171));
  t173 = t172;
  if (t173) {
  t174 = ((int64_t)((((*(v0))).f4).f2));
  t175 = ((int64_t)(((int64_t)UINT64_C(0))));
  t173 = ((t174) == (t175));
  }
  t176 = t173;
  t177 = t176;
  if (t177) {
  t178 = ((int64_t)((((*(v0))).f5).f2));
  t179 = ((int64_t)(((int64_t)UINT64_C(2))));
  t177 = ((t178) == (t179));
  }
  t180 = t177;
  if (t180) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t182 = ((int64_t)UINT64_C(0));
  t183 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t182, ((int64_t)UINT64_C(4)));
  t184 = ((int64_t)((*t183)));
  /* FG_DIRECT_SLICE_INDEX */
  t185 = ((int64_t)UINT64_C(0));
  t186 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t185, ((int64_t)UINT64_C(4)));
  t187 = ((int64_t)((*t186)));
  t188 = ((int64_t)gg_div(t184, t187));
  t181 = 0;
label18:;
  if (t181 >= t188) goto label20;
  v31 = t181;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t190 = ((int64_t)UINT64_C(0));
  t191 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t190, ((int64_t)UINT64_C(4)));
  t192 = (*t191);
  t189 = 0;
label21:;
  if (t189 >= t192) goto label23;
  v32 = t189;
  {
  t193 = ((int64_t)(v10));
  t194 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t195 = ((int64_t)UINT64_C(0));
  t196 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t195, ((int64_t)UINT64_C(4)));
  t197 = ((int64_t)((*t196)));
  t198 = ((int64_t)(((int64_t)((uint64_t)(t194) * (uint64_t)(t197)))));
  t199 = ((int64_t)(((int64_t)((uint64_t)(t193) + (uint64_t)(t198)))));
  t200 = ((int64_t)(v32));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t199) + (uint64_t)(t200)))));
  t201 = ((int64_t)(v11));
  t202 = ((int64_t)(v32));
  /* FG_DIRECT_SLICE_INDEX */
  t203 = ((int64_t)UINT64_C(0));
  t204 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t203, ((int64_t)UINT64_C(4)));
  t205 = ((int64_t)((*t204)));
  t206 = ((int64_t)(((int64_t)((uint64_t)(t202) * (uint64_t)(t205)))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t201) + (uint64_t)(t206)))));
  t207 = ((int64_t)(v12));
  t208 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t209 = ((int64_t)UINT64_C(0));
  t210 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t209, ((int64_t)UINT64_C(4)));
  t211 = ((int64_t)((*t210)));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t208) * (uint64_t)(t211)))));
  t213 = ((int64_t)(((int64_t)((uint64_t)(t207) + (uint64_t)(t212)))));
  t214 = ((int64_t)(v32));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t213) + (uint64_t)(t214)))));
  /* FG_DIRECT_SLICE_INDEX */
  t215 = v33;
  t216 = ((uint16_t *)(v2).data) + gg_index(t215, (v2).len);
  t217 = ((uint16_t)((*t216)));
  t218 = gg_function_11(t217, base + 0);
  t219 = ((float)(t218));
  /* FG_DIRECT_SLICE_INDEX */
  t220 = v34;
  t221 = ((float *)(v3).data) + gg_index(t220, (v3).len);
  t222 = ((float)((*t221)));
  v36 = ((float)(((t219) - (t222))));
  /* FG_DIRECT_SLICE_INDEX */
  t223 = v35;
  t224 = ((uint16_t *)(v6).data) + gg_index(t223, (v6).len);
  t225 = ((float)(v36));
  (*t224) = gg_function_13(t225, base + 0);
  }
label22:;
  t189++; goto label21;
label23:;
  }
label19:;
  t181++; goto label18;
label20:;
  }
  } else {
  t226 = ((int64_t)((((*(v0))).f3).f2));
  t227 = ((int64_t)(((int64_t)UINT64_C(2))));
  t228 = ((t226) == (t227));
  t229 = t228;
  if (t229) {
  t230 = ((int64_t)((((*(v0))).f4).f2));
  t231 = ((int64_t)(((int64_t)UINT64_C(0))));
  t229 = ((t230) == (t231));
  }
  t232 = t229;
  t233 = t232;
  if (t233) {
  t234 = ((int64_t)((((*(v0))).f5).f2));
  t235 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((t234) == (t235));
  }
  t236 = t233;
  if (t236) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t238 = ((int64_t)UINT64_C(0));
  t239 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t238, ((int64_t)UINT64_C(4)));
  t240 = ((int64_t)((*t239)));
  /* FG_DIRECT_SLICE_INDEX */
  t241 = ((int64_t)UINT64_C(0));
  t242 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t241, ((int64_t)UINT64_C(4)));
  t243 = ((int64_t)((*t242)));
  t244 = ((int64_t)gg_div(t240, t243));
  t237 = 0;
label24:;
  if (t237 >= t244) goto label26;
  v37 = t237;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t246 = ((int64_t)UINT64_C(0));
  t247 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t246, ((int64_t)UINT64_C(4)));
  t248 = (*t247);
  t245 = 0;
label27:;
  if (t245 >= t248) goto label29;
  v38 = t245;
  {
  t249 = ((int64_t)(v10));
  t250 = ((int64_t)(v37));
  /* FG_DIRECT_SLICE_INDEX */
  t251 = ((int64_t)UINT64_C(0));
  t252 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t251, ((int64_t)UINT64_C(4)));
  t253 = ((int64_t)((*t252)));
  t254 = ((int64_t)(((int64_t)((uint64_t)(t250) * (uint64_t)(t253)))));
  t255 = ((int64_t)(((int64_t)((uint64_t)(t249) + (uint64_t)(t254)))));
  t256 = ((int64_t)(v38));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t255) + (uint64_t)(t256)))));
  t257 = ((int64_t)(v11));
  t258 = ((int64_t)(v38));
  /* FG_DIRECT_SLICE_INDEX */
  t259 = ((int64_t)UINT64_C(0));
  t260 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t259, ((int64_t)UINT64_C(4)));
  t261 = ((int64_t)((*t260)));
  t262 = ((int64_t)(((int64_t)((uint64_t)(t258) * (uint64_t)(t261)))));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t257) + (uint64_t)(t262)))));
  t263 = ((int64_t)(v12));
  t264 = ((int64_t)(v37));
  /* FG_DIRECT_SLICE_INDEX */
  t265 = ((int64_t)UINT64_C(0));
  t266 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t265, ((int64_t)UINT64_C(4)));
  t267 = ((int64_t)((*t266)));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t264) * (uint64_t)(t267)))));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t263) + (uint64_t)(t268)))));
  t270 = ((int64_t)(v38));
  v41 = ((int64_t)(((int64_t)((uint64_t)(t269) + (uint64_t)(t270)))));
  /* FG_DIRECT_SLICE_INDEX */
  t271 = v39;
  t272 = ((uint16_t *)(v2).data) + gg_index(t271, (v2).len);
  t273 = ((uint16_t)((*t272)));
  t274 = gg_function_11(t273, base + 0);
  t275 = ((float)(t274));
  /* FG_DIRECT_SLICE_INDEX */
  t276 = v40;
  t277 = ((float *)(v3).data) + gg_index(t276, (v3).len);
  t278 = ((float)((*t277)));
  v42 = ((float)(((t275) - (t278))));
  /* FG_DIRECT_SLICE_INDEX */
  t279 = v41;
  t280 = ((float *)(v5).data) + gg_index(t279, (v5).len);
  (*t280) = ((float)(v42));
  }
label28:;
  t245++; goto label27;
label29:;
  }
label25:;
  t237++; goto label24;
label26:;
  }
  } else {
  t281 = ((int64_t)((((*(v0))).f3).f2));
  t282 = ((int64_t)(((int64_t)UINT64_C(1))));
  t283 = ((t281) == (t282));
  t284 = t283;
  if (t284) {
  t285 = ((int64_t)((((*(v0))).f4).f2));
  t286 = ((int64_t)(((int64_t)UINT64_C(0))));
  t284 = ((t285) == (t286));
  }
  t287 = t284;
  t288 = t287;
  if (t288) {
  t289 = ((int64_t)((((*(v0))).f5).f2));
  t290 = ((int64_t)(((int64_t)UINT64_C(1))));
  t288 = ((t289) == (t290));
  }
  t291 = t288;
  if (t291) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t293 = ((int64_t)UINT64_C(0));
  t294 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t293, ((int64_t)UINT64_C(4)));
  t295 = ((int64_t)((*t294)));
  /* FG_DIRECT_SLICE_INDEX */
  t296 = ((int64_t)UINT64_C(0));
  t297 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t296, ((int64_t)UINT64_C(4)));
  t298 = ((int64_t)((*t297)));
  t299 = ((int64_t)gg_div(t295, t298));
  t292 = 0;
label30:;
  if (t292 >= t299) goto label32;
  v43 = t292;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t301 = ((int64_t)UINT64_C(0));
  t302 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t301, ((int64_t)UINT64_C(4)));
  t303 = (*t302);
  t300 = 0;
label33:;
  if (t300 >= t303) goto label35;
  v44 = t300;
  {
  t304 = ((int64_t)(v10));
  t305 = ((int64_t)(v43));
  /* FG_DIRECT_SLICE_INDEX */
  t306 = ((int64_t)UINT64_C(0));
  t307 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t306, ((int64_t)UINT64_C(4)));
  t308 = ((int64_t)((*t307)));
  t309 = ((int64_t)(((int64_t)((uint64_t)(t305) * (uint64_t)(t308)))));
  t310 = ((int64_t)(((int64_t)((uint64_t)(t304) + (uint64_t)(t309)))));
  t311 = ((int64_t)(v44));
  v45 = ((int64_t)(((int64_t)((uint64_t)(t310) + (uint64_t)(t311)))));
  t312 = ((int64_t)(v11));
  t313 = ((int64_t)(v44));
  /* FG_DIRECT_SLICE_INDEX */
  t314 = ((int64_t)UINT64_C(0));
  t315 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t314, ((int64_t)UINT64_C(4)));
  t316 = ((int64_t)((*t315)));
  t317 = ((int64_t)(((int64_t)((uint64_t)(t313) * (uint64_t)(t316)))));
  v46 = ((int64_t)(((int64_t)((uint64_t)(t312) + (uint64_t)(t317)))));
  t318 = ((int64_t)(v12));
  t319 = ((int64_t)(v43));
  /* FG_DIRECT_SLICE_INDEX */
  t320 = ((int64_t)UINT64_C(0));
  t321 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t320, ((int64_t)UINT64_C(4)));
  t322 = ((int64_t)((*t321)));
  t323 = ((int64_t)(((int64_t)((uint64_t)(t319) * (uint64_t)(t322)))));
  t324 = ((int64_t)(((int64_t)((uint64_t)(t318) + (uint64_t)(t323)))));
  t325 = ((int64_t)(v44));
  v47 = ((int64_t)(((int64_t)((uint64_t)(t324) + (uint64_t)(t325)))));
  /* FG_DIRECT_SLICE_INDEX */
  t326 = v45;
  t327 = ((uint16_t *)(v2).data) + gg_index(t326, (v2).len);
  t328 = ((uint16_t)((*t327)));
  t329 = fg_el_half(t328);
  t330 = ((float)(t329));
  /* FG_DIRECT_SLICE_INDEX */
  t331 = v46;
  t332 = ((float *)(v3).data) + gg_index(t331, (v3).len);
  t333 = ((float)((*t332)));
  v48 = ((float)(((t330) - (t333))));
  /* FG_DIRECT_SLICE_INDEX */
  t334 = v47;
  t335 = ((uint16_t *)(v6).data) + gg_index(t334, (v6).len);
  t336 = ((float)(v48));
  t337 = fg_el_to_half(t336);
  (*t335) = ((uint16_t)(t337));
  }
label34:;
  t300++; goto label33;
label35:;
  }
label31:;
  t292++; goto label30;
label32:;
  }
  } else {
  t338 = ((int64_t)((((*(v0))).f3).f2));
  t339 = ((int64_t)(((int64_t)UINT64_C(1))));
  t340 = ((t338) == (t339));
  t341 = t340;
  if (t341) {
  t342 = ((int64_t)((((*(v0))).f4).f2));
  t343 = ((int64_t)(((int64_t)UINT64_C(0))));
  t341 = ((t342) == (t343));
  }
  t344 = t341;
  t345 = t344;
  if (t345) {
  t346 = ((int64_t)((((*(v0))).f5).f2));
  t347 = ((int64_t)(((int64_t)UINT64_C(0))));
  t345 = ((t346) == (t347));
  }
  t348 = t345;
  if (t348) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t350 = ((int64_t)UINT64_C(0));
  t351 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t350, ((int64_t)UINT64_C(4)));
  t352 = ((int64_t)((*t351)));
  /* FG_DIRECT_SLICE_INDEX */
  t353 = ((int64_t)UINT64_C(0));
  t354 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t353, ((int64_t)UINT64_C(4)));
  t355 = ((int64_t)((*t354)));
  t356 = ((int64_t)gg_div(t352, t355));
  t349 = 0;
label36:;
  if (t349 >= t356) goto label38;
  v49 = t349;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t358 = ((int64_t)UINT64_C(0));
  t359 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t358, ((int64_t)UINT64_C(4)));
  t360 = (*t359);
  t357 = 0;
label39:;
  if (t357 >= t360) goto label41;
  v50 = t357;
  {
  t361 = ((int64_t)(v10));
  t362 = ((int64_t)(v49));
  /* FG_DIRECT_SLICE_INDEX */
  t363 = ((int64_t)UINT64_C(0));
  t364 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t363, ((int64_t)UINT64_C(4)));
  t365 = ((int64_t)((*t364)));
  t366 = ((int64_t)(((int64_t)((uint64_t)(t362) * (uint64_t)(t365)))));
  t367 = ((int64_t)(((int64_t)((uint64_t)(t361) + (uint64_t)(t366)))));
  t368 = ((int64_t)(v50));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t367) + (uint64_t)(t368)))));
  t369 = ((int64_t)(v11));
  t370 = ((int64_t)(v50));
  /* FG_DIRECT_SLICE_INDEX */
  t371 = ((int64_t)UINT64_C(0));
  t372 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t371, ((int64_t)UINT64_C(4)));
  t373 = ((int64_t)((*t372)));
  t374 = ((int64_t)(((int64_t)((uint64_t)(t370) * (uint64_t)(t373)))));
  v52 = ((int64_t)(((int64_t)((uint64_t)(t369) + (uint64_t)(t374)))));
  t375 = ((int64_t)(v12));
  t376 = ((int64_t)(v49));
  /* FG_DIRECT_SLICE_INDEX */
  t377 = ((int64_t)UINT64_C(0));
  t378 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t377, ((int64_t)UINT64_C(4)));
  t379 = ((int64_t)((*t378)));
  t380 = ((int64_t)(((int64_t)((uint64_t)(t376) * (uint64_t)(t379)))));
  t381 = ((int64_t)(((int64_t)((uint64_t)(t375) + (uint64_t)(t380)))));
  t382 = ((int64_t)(v50));
  v53 = ((int64_t)(((int64_t)((uint64_t)(t381) + (uint64_t)(t382)))));
  /* FG_DIRECT_SLICE_INDEX */
  t383 = v51;
  t384 = ((uint16_t *)(v2).data) + gg_index(t383, (v2).len);
  t385 = ((uint16_t)((*t384)));
  t386 = fg_el_half(t385);
  t387 = ((float)(t386));
  /* FG_DIRECT_SLICE_INDEX */
  t388 = v52;
  t389 = ((float *)(v3).data) + gg_index(t388, (v3).len);
  t390 = ((float)((*t389)));
  v54 = ((float)(((t387) - (t390))));
  /* FG_DIRECT_SLICE_INDEX */
  t391 = v53;
  t392 = ((float *)(v5).data) + gg_index(t391, (v5).len);
  (*t392) = ((float)(v54));
  }
label40:;
  t357++; goto label39;
label41:;
  }
label37:;
  t349++; goto label36;
label38:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
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
static void gg_function_51(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  int64_t * t13;
  int64_t t14;
  int64_t t15;
  int64_t * t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t * t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t * t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t * t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t * t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  float * t46;
  float t47;
  int64_t t48;
  float * t49;
  float t50;
  int64_t t51;
  float * t52;
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
  int64_t t64;
  int64_t t65;
  int64_t * t66;
  int64_t t67;
  int64_t t68;
  int64_t * t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t * t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t * t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t * t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t * t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint16_t * t99;
  uint16_t t100;
  float t101;
  float t102;
  int64_t t103;
  uint16_t * t104;
  uint16_t t105;
  float t106;
  float t107;
  int64_t t108;
  uint16_t * t109;
  float t110;
  uint16_t t111;
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
  int64_t t123;
  int64_t t124;
  int64_t * t125;
  int64_t t126;
  int64_t t127;
  int64_t * t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t * t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t * t138;
  int64_t t139;
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
  int64_t t150;
  int64_t t151;
  int64_t * t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  uint16_t * t158;
  uint16_t t159;
  float t160;
  float t161;
  int64_t t162;
  uint16_t * t163;
  uint16_t t164;
  float t165;
  float t166;
  int64_t t167;
  uint16_t * t168;
  float t169;
  int64_t t170;
  int64_t t171;
  uint8_t t172;
  uint8_t t173;
  int64_t t174;
  int64_t t175;
  uint8_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  int64_t * t183;
  int64_t t184;
  int64_t t185;
  int64_t * t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t * t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t * t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t * t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t * t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  uint16_t * t216;
  uint16_t t217;
  float t218;
  float t219;
  int64_t t220;
  float * t221;
  float t222;
  int64_t t223;
  uint16_t * t224;
  float t225;
  int64_t t226;
  int64_t t227;
  uint8_t t228;
  uint8_t t229;
  int64_t t230;
  int64_t t231;
  uint8_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  uint8_t t236;
  int64_t t237;
  int64_t t238;
  int64_t * t239;
  int64_t t240;
  int64_t t241;
  int64_t * t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t * t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t * t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t * t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t * t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  uint16_t * t272;
  uint16_t t273;
  float t274;
  float t275;
  int64_t t276;
  float * t277;
  float t278;
  int64_t t279;
  float * t280;
  int64_t t281;
  int64_t t282;
  uint8_t t283;
  uint8_t t284;
  int64_t t285;
  int64_t t286;
  uint8_t t287;
  uint8_t t288;
  int64_t t289;
  int64_t t290;
  uint8_t t291;
  int64_t t292;
  int64_t t293;
  int64_t * t294;
  int64_t t295;
  int64_t t296;
  int64_t * t297;
  int64_t t298;
  int64_t t299;
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
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t * t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t * t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  uint16_t * t327;
  uint16_t t328;
  float t329;
  float t330;
  int64_t t331;
  float * t332;
  float t333;
  int64_t t334;
  uint16_t * t335;
  float t336;
  uint16_t t337;
  int64_t t338;
  int64_t t339;
  uint8_t t340;
  uint8_t t341;
  int64_t t342;
  int64_t t343;
  uint8_t t344;
  uint8_t t345;
  int64_t t346;
  int64_t t347;
  uint8_t t348;
  int64_t t349;
  int64_t t350;
  int64_t * t351;
  int64_t t352;
  int64_t t353;
  int64_t * t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t * t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t * t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  int64_t * t372;
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
  uint16_t * t384;
  uint16_t t385;
  float t386;
  float t387;
  int64_t t388;
  float * t389;
  float t390;
  int64_t t391;
  float * t392;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  float v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  float v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  float v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  float v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
  float v42;
  int64_t v43;
  int64_t v44;
  int64_t v45;
  int64_t v46;
  int64_t v47;
  float v48;
  int64_t v49;
  int64_t v50;
  int64_t v51;
  int64_t v52;
  int64_t v53;
  float v54;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f4).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)((((*(v0))).f5).f2));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) == (t9));
  }
  t10 = t7;
  if (t10) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t12 = ((int64_t)UINT64_C(0));
  t13 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t12, ((int64_t)UINT64_C(4)));
  t14 = ((int64_t)((*t13)));
  /* FG_DIRECT_SLICE_INDEX */
  t15 = ((int64_t)UINT64_C(0));
  t16 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t15, ((int64_t)UINT64_C(4)));
  t17 = ((int64_t)((*t16)));
  t18 = ((int64_t)gg_div(t14, t17));
  t11 = 0;
label0:;
  if (t11 >= t18) goto label2;
  v13 = t11;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t20 = ((int64_t)UINT64_C(0));
  t21 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t20, ((int64_t)UINT64_C(4)));
  t22 = (*t21);
  t19 = 0;
label3:;
  if (t19 >= t22) goto label5;
  v14 = t19;
  {
  t23 = ((int64_t)(v10));
  t24 = ((int64_t)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t25 = ((int64_t)UINT64_C(0));
  t26 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t25, ((int64_t)UINT64_C(4)));
  t27 = ((int64_t)((*t26)));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t27)))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t28)))));
  t30 = ((int64_t)(v14));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t31 = ((int64_t)(v11));
  t32 = ((int64_t)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t33 = ((int64_t)UINT64_C(0));
  t34 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t33, ((int64_t)UINT64_C(4)));
  t35 = ((int64_t)((*t34)));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t35)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t36)))));
  t37 = ((int64_t)(v12));
  t38 = ((int64_t)(v13));
  /* FG_DIRECT_SLICE_INDEX */
  t39 = ((int64_t)UINT64_C(0));
  t40 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t39, ((int64_t)UINT64_C(4)));
  t41 = ((int64_t)((*t40)));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t41)))));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t42)))));
  t44 = ((int64_t)(v14));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t44)))));
  /* FG_DIRECT_SLICE_INDEX */
  t45 = v15;
  t46 = ((float *)(v1).data) + gg_index(t45, (v1).len);
  t47 = ((float)((*t46)));
  /* FG_DIRECT_SLICE_INDEX */
  t48 = v16;
  t49 = ((float *)(v3).data) + gg_index(t48, (v3).len);
  t50 = ((float)((*t49)));
  v18 = ((float)(((t47) + (t50))));
  /* FG_DIRECT_SLICE_INDEX */
  t51 = v17;
  t52 = ((float *)(v5).data) + gg_index(t51, (v5).len);
  (*t52) = ((float)(v18));
  }
label4:;
  t19++; goto label3;
label5:;
  }
label1:;
  t11++; goto label0;
label2:;
  }
  } else {
  t53 = ((int64_t)((((*(v0))).f3).f2));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((t53) == (t54));
  t56 = t55;
  if (t56) {
  t57 = ((int64_t)((((*(v0))).f4).f2));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t56 = ((t57) == (t58));
  }
  t59 = t56;
  t60 = t59;
  if (t60) {
  t61 = ((int64_t)((((*(v0))).f5).f2));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((t61) == (t62));
  }
  t63 = t60;
  if (t63) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t65 = ((int64_t)UINT64_C(0));
  t66 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t65, ((int64_t)UINT64_C(4)));
  t67 = ((int64_t)((*t66)));
  /* FG_DIRECT_SLICE_INDEX */
  t68 = ((int64_t)UINT64_C(0));
  t69 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t68, ((int64_t)UINT64_C(4)));
  t70 = ((int64_t)((*t69)));
  t71 = ((int64_t)gg_div(t67, t70));
  t64 = 0;
label6:;
  if (t64 >= t71) goto label8;
  v19 = t64;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t73 = ((int64_t)UINT64_C(0));
  t74 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t73, ((int64_t)UINT64_C(4)));
  t75 = (*t74);
  t72 = 0;
label9:;
  if (t72 >= t75) goto label11;
  v20 = t72;
  {
  t76 = ((int64_t)(v10));
  t77 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t78 = ((int64_t)UINT64_C(0));
  t79 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t78, ((int64_t)UINT64_C(4)));
  t80 = ((int64_t)((*t79)));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t81)))));
  t83 = ((int64_t)(v20));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  t84 = ((int64_t)(v11));
  t85 = ((int64_t)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t86 = ((int64_t)UINT64_C(0));
  t87 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t86, ((int64_t)UINT64_C(4)));
  t88 = ((int64_t)((*t87)));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t88)))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t89)))));
  t90 = ((int64_t)(v12));
  t91 = ((int64_t)(v19));
  /* FG_DIRECT_SLICE_INDEX */
  t92 = ((int64_t)UINT64_C(0));
  t93 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t92, ((int64_t)UINT64_C(4)));
  t94 = ((int64_t)((*t93)));
  t95 = ((int64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t94)))));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t95)))));
  t97 = ((int64_t)(v20));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  /* FG_DIRECT_SLICE_INDEX */
  t98 = v21;
  t99 = ((uint16_t *)(v2).data) + gg_index(t98, (v2).len);
  t100 = ((uint16_t)((*t99)));
  t101 = fg_el_half(t100);
  t102 = ((float)(t101));
  /* FG_DIRECT_SLICE_INDEX */
  t103 = v22;
  t104 = ((uint16_t *)(v4).data) + gg_index(t103, (v4).len);
  t105 = ((uint16_t)((*t104)));
  t106 = fg_el_half(t105);
  t107 = ((float)(t106));
  v24 = ((float)(((t102) + (t107))));
  /* FG_DIRECT_SLICE_INDEX */
  t108 = v23;
  t109 = ((uint16_t *)(v6).data) + gg_index(t108, (v6).len);
  t110 = ((float)(v24));
  t111 = fg_el_to_half(t110);
  (*t109) = ((uint16_t)(t111));
  }
label10:;
  t72++; goto label9;
label11:;
  }
label7:;
  t64++; goto label6;
label8:;
  }
  } else {
  t112 = ((int64_t)((((*(v0))).f3).f2));
  t113 = ((int64_t)(((int64_t)UINT64_C(2))));
  t114 = ((t112) == (t113));
  t115 = t114;
  if (t115) {
  t116 = ((int64_t)((((*(v0))).f4).f2));
  t117 = ((int64_t)(((int64_t)UINT64_C(2))));
  t115 = ((t116) == (t117));
  }
  t118 = t115;
  t119 = t118;
  if (t119) {
  t120 = ((int64_t)((((*(v0))).f5).f2));
  t121 = ((int64_t)(((int64_t)UINT64_C(2))));
  t119 = ((t120) == (t121));
  }
  t122 = t119;
  if (t122) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t124 = ((int64_t)UINT64_C(0));
  t125 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t124, ((int64_t)UINT64_C(4)));
  t126 = ((int64_t)((*t125)));
  /* FG_DIRECT_SLICE_INDEX */
  t127 = ((int64_t)UINT64_C(0));
  t128 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t127, ((int64_t)UINT64_C(4)));
  t129 = ((int64_t)((*t128)));
  t130 = ((int64_t)gg_div(t126, t129));
  t123 = 0;
label12:;
  if (t123 >= t130) goto label14;
  v25 = t123;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t132 = ((int64_t)UINT64_C(0));
  t133 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t132, ((int64_t)UINT64_C(4)));
  t134 = (*t133);
  t131 = 0;
label15:;
  if (t131 >= t134) goto label17;
  v26 = t131;
  {
  t135 = ((int64_t)(v10));
  t136 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t137 = ((int64_t)UINT64_C(0));
  t138 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t137, ((int64_t)UINT64_C(4)));
  t139 = ((int64_t)((*t138)));
  t140 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t139)))));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t135) + (uint64_t)(t140)))));
  t142 = ((int64_t)(v26));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t141) + (uint64_t)(t142)))));
  t143 = ((int64_t)(v11));
  t144 = ((int64_t)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t145 = ((int64_t)UINT64_C(0));
  t146 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t145, ((int64_t)UINT64_C(4)));
  t147 = ((int64_t)((*t146)));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t147)))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t143) + (uint64_t)(t148)))));
  t149 = ((int64_t)(v12));
  t150 = ((int64_t)(v25));
  /* FG_DIRECT_SLICE_INDEX */
  t151 = ((int64_t)UINT64_C(0));
  t152 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t151, ((int64_t)UINT64_C(4)));
  t153 = ((int64_t)((*t152)));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t150) * (uint64_t)(t153)))));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t149) + (uint64_t)(t154)))));
  t156 = ((int64_t)(v26));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t155) + (uint64_t)(t156)))));
  /* FG_DIRECT_SLICE_INDEX */
  t157 = v27;
  t158 = ((uint16_t *)(v2).data) + gg_index(t157, (v2).len);
  t159 = ((uint16_t)((*t158)));
  t160 = gg_function_11(t159, base + 0);
  t161 = ((float)(t160));
  /* FG_DIRECT_SLICE_INDEX */
  t162 = v28;
  t163 = ((uint16_t *)(v4).data) + gg_index(t162, (v4).len);
  t164 = ((uint16_t)((*t163)));
  t165 = gg_function_11(t164, base + 0);
  t166 = ((float)(t165));
  v30 = ((float)(((t161) + (t166))));
  /* FG_DIRECT_SLICE_INDEX */
  t167 = v29;
  t168 = ((uint16_t *)(v6).data) + gg_index(t167, (v6).len);
  t169 = ((float)(v30));
  (*t168) = gg_function_13(t169, base + 0);
  }
label16:;
  t131++; goto label15;
label17:;
  }
label13:;
  t123++; goto label12;
label14:;
  }
  } else {
  t170 = ((int64_t)((((*(v0))).f3).f2));
  t171 = ((int64_t)(((int64_t)UINT64_C(2))));
  t172 = ((t170) == (t171));
  t173 = t172;
  if (t173) {
  t174 = ((int64_t)((((*(v0))).f4).f2));
  t175 = ((int64_t)(((int64_t)UINT64_C(0))));
  t173 = ((t174) == (t175));
  }
  t176 = t173;
  t177 = t176;
  if (t177) {
  t178 = ((int64_t)((((*(v0))).f5).f2));
  t179 = ((int64_t)(((int64_t)UINT64_C(2))));
  t177 = ((t178) == (t179));
  }
  t180 = t177;
  if (t180) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t182 = ((int64_t)UINT64_C(0));
  t183 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t182, ((int64_t)UINT64_C(4)));
  t184 = ((int64_t)((*t183)));
  /* FG_DIRECT_SLICE_INDEX */
  t185 = ((int64_t)UINT64_C(0));
  t186 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t185, ((int64_t)UINT64_C(4)));
  t187 = ((int64_t)((*t186)));
  t188 = ((int64_t)gg_div(t184, t187));
  t181 = 0;
label18:;
  if (t181 >= t188) goto label20;
  v31 = t181;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t190 = ((int64_t)UINT64_C(0));
  t191 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t190, ((int64_t)UINT64_C(4)));
  t192 = (*t191);
  t189 = 0;
label21:;
  if (t189 >= t192) goto label23;
  v32 = t189;
  {
  t193 = ((int64_t)(v10));
  t194 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t195 = ((int64_t)UINT64_C(0));
  t196 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t195, ((int64_t)UINT64_C(4)));
  t197 = ((int64_t)((*t196)));
  t198 = ((int64_t)(((int64_t)((uint64_t)(t194) * (uint64_t)(t197)))));
  t199 = ((int64_t)(((int64_t)((uint64_t)(t193) + (uint64_t)(t198)))));
  t200 = ((int64_t)(v32));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t199) + (uint64_t)(t200)))));
  t201 = ((int64_t)(v11));
  t202 = ((int64_t)(v32));
  /* FG_DIRECT_SLICE_INDEX */
  t203 = ((int64_t)UINT64_C(0));
  t204 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t203, ((int64_t)UINT64_C(4)));
  t205 = ((int64_t)((*t204)));
  t206 = ((int64_t)(((int64_t)((uint64_t)(t202) * (uint64_t)(t205)))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t201) + (uint64_t)(t206)))));
  t207 = ((int64_t)(v12));
  t208 = ((int64_t)(v31));
  /* FG_DIRECT_SLICE_INDEX */
  t209 = ((int64_t)UINT64_C(0));
  t210 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t209, ((int64_t)UINT64_C(4)));
  t211 = ((int64_t)((*t210)));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t208) * (uint64_t)(t211)))));
  t213 = ((int64_t)(((int64_t)((uint64_t)(t207) + (uint64_t)(t212)))));
  t214 = ((int64_t)(v32));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t213) + (uint64_t)(t214)))));
  /* FG_DIRECT_SLICE_INDEX */
  t215 = v33;
  t216 = ((uint16_t *)(v2).data) + gg_index(t215, (v2).len);
  t217 = ((uint16_t)((*t216)));
  t218 = gg_function_11(t217, base + 0);
  t219 = ((float)(t218));
  /* FG_DIRECT_SLICE_INDEX */
  t220 = v34;
  t221 = ((float *)(v3).data) + gg_index(t220, (v3).len);
  t222 = ((float)((*t221)));
  v36 = ((float)(((t219) + (t222))));
  /* FG_DIRECT_SLICE_INDEX */
  t223 = v35;
  t224 = ((uint16_t *)(v6).data) + gg_index(t223, (v6).len);
  t225 = ((float)(v36));
  (*t224) = gg_function_13(t225, base + 0);
  }
label22:;
  t189++; goto label21;
label23:;
  }
label19:;
  t181++; goto label18;
label20:;
  }
  } else {
  t226 = ((int64_t)((((*(v0))).f3).f2));
  t227 = ((int64_t)(((int64_t)UINT64_C(2))));
  t228 = ((t226) == (t227));
  t229 = t228;
  if (t229) {
  t230 = ((int64_t)((((*(v0))).f4).f2));
  t231 = ((int64_t)(((int64_t)UINT64_C(0))));
  t229 = ((t230) == (t231));
  }
  t232 = t229;
  t233 = t232;
  if (t233) {
  t234 = ((int64_t)((((*(v0))).f5).f2));
  t235 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((t234) == (t235));
  }
  t236 = t233;
  if (t236) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t238 = ((int64_t)UINT64_C(0));
  t239 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t238, ((int64_t)UINT64_C(4)));
  t240 = ((int64_t)((*t239)));
  /* FG_DIRECT_SLICE_INDEX */
  t241 = ((int64_t)UINT64_C(0));
  t242 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t241, ((int64_t)UINT64_C(4)));
  t243 = ((int64_t)((*t242)));
  t244 = ((int64_t)gg_div(t240, t243));
  t237 = 0;
label24:;
  if (t237 >= t244) goto label26;
  v37 = t237;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t246 = ((int64_t)UINT64_C(0));
  t247 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t246, ((int64_t)UINT64_C(4)));
  t248 = (*t247);
  t245 = 0;
label27:;
  if (t245 >= t248) goto label29;
  v38 = t245;
  {
  t249 = ((int64_t)(v10));
  t250 = ((int64_t)(v37));
  /* FG_DIRECT_SLICE_INDEX */
  t251 = ((int64_t)UINT64_C(0));
  t252 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t251, ((int64_t)UINT64_C(4)));
  t253 = ((int64_t)((*t252)));
  t254 = ((int64_t)(((int64_t)((uint64_t)(t250) * (uint64_t)(t253)))));
  t255 = ((int64_t)(((int64_t)((uint64_t)(t249) + (uint64_t)(t254)))));
  t256 = ((int64_t)(v38));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t255) + (uint64_t)(t256)))));
  t257 = ((int64_t)(v11));
  t258 = ((int64_t)(v38));
  /* FG_DIRECT_SLICE_INDEX */
  t259 = ((int64_t)UINT64_C(0));
  t260 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t259, ((int64_t)UINT64_C(4)));
  t261 = ((int64_t)((*t260)));
  t262 = ((int64_t)(((int64_t)((uint64_t)(t258) * (uint64_t)(t261)))));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t257) + (uint64_t)(t262)))));
  t263 = ((int64_t)(v12));
  t264 = ((int64_t)(v37));
  /* FG_DIRECT_SLICE_INDEX */
  t265 = ((int64_t)UINT64_C(0));
  t266 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t265, ((int64_t)UINT64_C(4)));
  t267 = ((int64_t)((*t266)));
  t268 = ((int64_t)(((int64_t)((uint64_t)(t264) * (uint64_t)(t267)))));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t263) + (uint64_t)(t268)))));
  t270 = ((int64_t)(v38));
  v41 = ((int64_t)(((int64_t)((uint64_t)(t269) + (uint64_t)(t270)))));
  /* FG_DIRECT_SLICE_INDEX */
  t271 = v39;
  t272 = ((uint16_t *)(v2).data) + gg_index(t271, (v2).len);
  t273 = ((uint16_t)((*t272)));
  t274 = gg_function_11(t273, base + 0);
  t275 = ((float)(t274));
  /* FG_DIRECT_SLICE_INDEX */
  t276 = v40;
  t277 = ((float *)(v3).data) + gg_index(t276, (v3).len);
  t278 = ((float)((*t277)));
  v42 = ((float)(((t275) + (t278))));
  /* FG_DIRECT_SLICE_INDEX */
  t279 = v41;
  t280 = ((float *)(v5).data) + gg_index(t279, (v5).len);
  (*t280) = ((float)(v42));
  }
label28:;
  t245++; goto label27;
label29:;
  }
label25:;
  t237++; goto label24;
label26:;
  }
  } else {
  t281 = ((int64_t)((((*(v0))).f3).f2));
  t282 = ((int64_t)(((int64_t)UINT64_C(1))));
  t283 = ((t281) == (t282));
  t284 = t283;
  if (t284) {
  t285 = ((int64_t)((((*(v0))).f4).f2));
  t286 = ((int64_t)(((int64_t)UINT64_C(0))));
  t284 = ((t285) == (t286));
  }
  t287 = t284;
  t288 = t287;
  if (t288) {
  t289 = ((int64_t)((((*(v0))).f5).f2));
  t290 = ((int64_t)(((int64_t)UINT64_C(1))));
  t288 = ((t289) == (t290));
  }
  t291 = t288;
  if (t291) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t293 = ((int64_t)UINT64_C(0));
  t294 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t293, ((int64_t)UINT64_C(4)));
  t295 = ((int64_t)((*t294)));
  /* FG_DIRECT_SLICE_INDEX */
  t296 = ((int64_t)UINT64_C(0));
  t297 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t296, ((int64_t)UINT64_C(4)));
  t298 = ((int64_t)((*t297)));
  t299 = ((int64_t)gg_div(t295, t298));
  t292 = 0;
label30:;
  if (t292 >= t299) goto label32;
  v43 = t292;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t301 = ((int64_t)UINT64_C(0));
  t302 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t301, ((int64_t)UINT64_C(4)));
  t303 = (*t302);
  t300 = 0;
label33:;
  if (t300 >= t303) goto label35;
  v44 = t300;
  {
  t304 = ((int64_t)(v10));
  t305 = ((int64_t)(v43));
  /* FG_DIRECT_SLICE_INDEX */
  t306 = ((int64_t)UINT64_C(0));
  t307 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t306, ((int64_t)UINT64_C(4)));
  t308 = ((int64_t)((*t307)));
  t309 = ((int64_t)(((int64_t)((uint64_t)(t305) * (uint64_t)(t308)))));
  t310 = ((int64_t)(((int64_t)((uint64_t)(t304) + (uint64_t)(t309)))));
  t311 = ((int64_t)(v44));
  v45 = ((int64_t)(((int64_t)((uint64_t)(t310) + (uint64_t)(t311)))));
  t312 = ((int64_t)(v11));
  t313 = ((int64_t)(v44));
  /* FG_DIRECT_SLICE_INDEX */
  t314 = ((int64_t)UINT64_C(0));
  t315 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t314, ((int64_t)UINT64_C(4)));
  t316 = ((int64_t)((*t315)));
  t317 = ((int64_t)(((int64_t)((uint64_t)(t313) * (uint64_t)(t316)))));
  v46 = ((int64_t)(((int64_t)((uint64_t)(t312) + (uint64_t)(t317)))));
  t318 = ((int64_t)(v12));
  t319 = ((int64_t)(v43));
  /* FG_DIRECT_SLICE_INDEX */
  t320 = ((int64_t)UINT64_C(0));
  t321 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t320, ((int64_t)UINT64_C(4)));
  t322 = ((int64_t)((*t321)));
  t323 = ((int64_t)(((int64_t)((uint64_t)(t319) * (uint64_t)(t322)))));
  t324 = ((int64_t)(((int64_t)((uint64_t)(t318) + (uint64_t)(t323)))));
  t325 = ((int64_t)(v44));
  v47 = ((int64_t)(((int64_t)((uint64_t)(t324) + (uint64_t)(t325)))));
  /* FG_DIRECT_SLICE_INDEX */
  t326 = v45;
  t327 = ((uint16_t *)(v2).data) + gg_index(t326, (v2).len);
  t328 = ((uint16_t)((*t327)));
  t329 = fg_el_half(t328);
  t330 = ((float)(t329));
  /* FG_DIRECT_SLICE_INDEX */
  t331 = v46;
  t332 = ((float *)(v3).data) + gg_index(t331, (v3).len);
  t333 = ((float)((*t332)));
  v48 = ((float)(((t330) + (t333))));
  /* FG_DIRECT_SLICE_INDEX */
  t334 = v47;
  t335 = ((uint16_t *)(v6).data) + gg_index(t334, (v6).len);
  t336 = ((float)(v48));
  t337 = fg_el_to_half(t336);
  (*t335) = ((uint16_t)(t337));
  }
label34:;
  t300++; goto label33;
label35:;
  }
label31:;
  t292++; goto label30;
label32:;
  }
  } else {
  t338 = ((int64_t)((((*(v0))).f3).f2));
  t339 = ((int64_t)(((int64_t)UINT64_C(1))));
  t340 = ((t338) == (t339));
  t341 = t340;
  if (t341) {
  t342 = ((int64_t)((((*(v0))).f4).f2));
  t343 = ((int64_t)(((int64_t)UINT64_C(0))));
  t341 = ((t342) == (t343));
  }
  t344 = t341;
  t345 = t344;
  if (t345) {
  t346 = ((int64_t)((((*(v0))).f5).f2));
  t347 = ((int64_t)(((int64_t)UINT64_C(0))));
  t345 = ((t346) == (t347));
  }
  t348 = t345;
  if (t348) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t350 = ((int64_t)UINT64_C(0));
  t351 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t350, ((int64_t)UINT64_C(4)));
  t352 = ((int64_t)((*t351)));
  /* FG_DIRECT_SLICE_INDEX */
  t353 = ((int64_t)UINT64_C(0));
  t354 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t353, ((int64_t)UINT64_C(4)));
  t355 = ((int64_t)((*t354)));
  t356 = ((int64_t)gg_div(t352, t355));
  t349 = 0;
label36:;
  if (t349 >= t356) goto label38;
  v49 = t349;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t358 = ((int64_t)UINT64_C(0));
  t359 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t358, ((int64_t)UINT64_C(4)));
  t360 = (*t359);
  t357 = 0;
label39:;
  if (t357 >= t360) goto label41;
  v50 = t357;
  {
  t361 = ((int64_t)(v10));
  t362 = ((int64_t)(v49));
  /* FG_DIRECT_SLICE_INDEX */
  t363 = ((int64_t)UINT64_C(0));
  t364 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t363, ((int64_t)UINT64_C(4)));
  t365 = ((int64_t)((*t364)));
  t366 = ((int64_t)(((int64_t)((uint64_t)(t362) * (uint64_t)(t365)))));
  t367 = ((int64_t)(((int64_t)((uint64_t)(t361) + (uint64_t)(t366)))));
  t368 = ((int64_t)(v50));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t367) + (uint64_t)(t368)))));
  t369 = ((int64_t)(v11));
  t370 = ((int64_t)(v50));
  /* FG_DIRECT_SLICE_INDEX */
  t371 = ((int64_t)UINT64_C(0));
  t372 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t371, ((int64_t)UINT64_C(4)));
  t373 = ((int64_t)((*t372)));
  t374 = ((int64_t)(((int64_t)((uint64_t)(t370) * (uint64_t)(t373)))));
  v52 = ((int64_t)(((int64_t)((uint64_t)(t369) + (uint64_t)(t374)))));
  t375 = ((int64_t)(v12));
  t376 = ((int64_t)(v49));
  /* FG_DIRECT_SLICE_INDEX */
  t377 = ((int64_t)UINT64_C(0));
  t378 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t377, ((int64_t)UINT64_C(4)));
  t379 = ((int64_t)((*t378)));
  t380 = ((int64_t)(((int64_t)((uint64_t)(t376) * (uint64_t)(t379)))));
  t381 = ((int64_t)(((int64_t)((uint64_t)(t375) + (uint64_t)(t380)))));
  t382 = ((int64_t)(v50));
  v53 = ((int64_t)(((int64_t)((uint64_t)(t381) + (uint64_t)(t382)))));
  /* FG_DIRECT_SLICE_INDEX */
  t383 = v51;
  t384 = ((uint16_t *)(v2).data) + gg_index(t383, (v2).len);
  t385 = ((uint16_t)((*t384)));
  t386 = fg_el_half(t385);
  t387 = ((float)(t386));
  /* FG_DIRECT_SLICE_INDEX */
  t388 = v52;
  t389 = ((float *)(v3).data) + gg_index(t388, (v3).len);
  t390 = ((float)((*t389)));
  v54 = ((float)(((t387) + (t390))));
  /* FG_DIRECT_SLICE_INDEX */
  t391 = v53;
  t392 = ((float *)(v5).data) + gg_index(t391, (v5).len);
  (*t392) = ((float)(v54));
  }
label40:;
  t357++; goto label39;
label41:;
  }
label37:;
  t349++; goto label36;
label38:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
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
static void gg_function_49(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  uint8_t t18;
  int64_t t19;
  float * t20;
  float t21;
  float t22;
  float t23;
  float t24;
  int64_t t25;
  float * t26;
  float t27;
  float t28;
  float t29;
  float t30;
  float t31;
  int64_t t32;
  float * t33;
  float t34;
  float t35;
  float t36;
  float t37;
  float t38;
  float t39;
  float t40;
  int64_t t41;
  float * t42;
  float t43;
  int64_t t44;
  float * t45;
  float t46;
  float t47;
  float t48;
  float t49;
  gg_slice t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  float * t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  uint8_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  int64_t t62;
  int64_t t63;
  int64_t * t64;
  int64_t t65;
  gg_slice t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint16_t * t70;
  uint16_t t71;
  float t72;
  float t73;
  float t74;
  uint8_t t75;
  int64_t t76;
  float * t77;
  float t78;
  float t79;
  float t80;
  float t81;
  int64_t t82;
  float * t83;
  float t84;
  float t85;
  float t86;
  float t87;
  float t88;
  int64_t t89;
  float * t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  float t97;
  int64_t t98;
  float * t99;
  float t100;
  int64_t t101;
  float * t102;
  float t103;
  float t104;
  float t105;
  float t106;
  gg_slice t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  uint16_t * t111;
  float t112;
  uint16_t t113;
  int64_t t114;
  int64_t t115;
  uint8_t t116;
  uint8_t t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
  int64_t t121;
  int64_t t122;
  int64_t * t123;
  int64_t t124;
  gg_slice t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  uint16_t * t129;
  uint16_t t130;
  float t131;
  float t132;
  uint8_t t133;
  int64_t t134;
  float * t135;
  float t136;
  float t137;
  float t138;
  float t139;
  int64_t t140;
  float * t141;
  float t142;
  float t143;
  float t144;
  float t145;
  float t146;
  int64_t t147;
  float * t148;
  float t149;
  float t150;
  float t151;
  float t152;
  float t153;
  float t154;
  float t155;
  int64_t t156;
  float * t157;
  float t158;
  int64_t t159;
  float * t160;
  float t161;
  float t162;
  float t163;
  float t164;
  gg_slice t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint16_t * t169;
  float t170;
  int64_t t171;
  int64_t t172;
  uint8_t t173;
  uint8_t t174;
  int64_t t175;
  int64_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  int64_t * t180;
  int64_t t181;
  gg_slice t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  uint16_t * t186;
  uint16_t t187;
  float t188;
  float t189;
  uint8_t t190;
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
  int64_t t204;
  float * t205;
  float t206;
  float t207;
  float t208;
  float t209;
  float t210;
  float t211;
  float t212;
  int64_t t213;
  float * t214;
  float t215;
  int64_t t216;
  float * t217;
  float t218;
  float t219;
  float t220;
  float t221;
  gg_slice t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  float * t226;
  int64_t t227;
  int64_t t228;
  uint8_t t229;
  uint8_t t230;
  int64_t t231;
  int64_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  int64_t * t236;
  int64_t t237;
  gg_slice t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  uint16_t * t242;
  uint16_t t243;
  float t244;
  float t245;
  float t246;
  uint8_t t247;
  int64_t t248;
  float * t249;
  float t250;
  float t251;
  float t252;
  float t253;
  int64_t t254;
  float * t255;
  float t256;
  float t257;
  float t258;
  float t259;
  float t260;
  int64_t t261;
  float * t262;
  float t263;
  float t264;
  float t265;
  float t266;
  float t267;
  float t268;
  float t269;
  int64_t t270;
  float * t271;
  float t272;
  int64_t t273;
  float * t274;
  float t275;
  float t276;
  float t277;
  float t278;
  gg_slice t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  float * t283;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t18 = ((t16) > (t17));
  if (t18) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t19 = ((int64_t)UINT64_C(1));
  t20 = ((float *)(((*(v0))).f10).data) + gg_index(t19, ((int64_t)UINT64_C(4)));
  t21 = ((float)((*t20)));
  t22 = ((float)(v14));
  t23 = ((float)(((t21) * (t22))));
  t24 = ((float)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t25 = ((int64_t)UINT64_C(2));
  t26 = ((float *)(((*(v0))).f10).data) + gg_index(t25, ((int64_t)UINT64_C(4)));
  t27 = ((float)((*t26)));
  t28 = ((float)(v14));
  t29 = ((float)(((t27) * (t28))));
  t30 = fmaf(t23, t24, t29);
  v15 = ((float)(t30));
  }
  } else {
  {
  t31 = ((float)(v14));
  /* FG_DIRECT_SLICE_INDEX */
  t32 = ((int64_t)UINT64_C(3));
  t33 = ((float *)(((*(v0))).f10).data) + gg_index(t32, ((int64_t)UINT64_C(4)));
  t34 = ((float)((*t33)));
  t35 = fminf(t31, t34);
  t36 = ((float)(t35));
  t37 = expm1f(t36);
  t38 = ((float)(t37));
  t39 = ((float)(v14));
  t40 = ((float)(((t38) - (t39))));
  /* FG_DIRECT_SLICE_INDEX */
  t41 = ((int64_t)UINT64_C(0));
  t42 = ((float *)(((*(v0))).f10).data) + gg_index(t41, ((int64_t)UINT64_C(4)));
  t43 = ((float)((*t42)));
  /* FG_DIRECT_SLICE_INDEX */
  t44 = ((int64_t)UINT64_C(2));
  t45 = ((float *)(((*(v0))).f10).data) + gg_index(t44, ((int64_t)UINT64_C(4)));
  t46 = ((float)((*t45)));
  t47 = ((float)(v14));
  t48 = ((float)(((t46) * (t47))));
  t49 = fmaf(t40, t43, t48);
  v15 = ((float)(t49));
  }
  }
  t50 = v5;
  t51 = ((int64_t)(v12));
  t52 = ((int64_t)(v13));
  t53 = ((int64_t)((uint64_t)(t51) + (uint64_t)(t52)));
  t54 = ((float *)t50.data) + gg_index(t53, t50.len);
  (*t54) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t55 = ((int64_t)((((*(v0))).f3).f2));
  t56 = ((int64_t)(((int64_t)UINT64_C(1))));
  t57 = ((t55) == (t56));
  t58 = t57;
  if (t58) {
  t59 = ((int64_t)((((*(v0))).f5).f2));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  t58 = ((t59) == (t60));
  }
  t61 = t58;
  if (t61) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t63 = ((int64_t)UINT64_C(0));
  t64 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t63, ((int64_t)UINT64_C(4)));
  t65 = (*t64);
  t62 = 0;
label3:;
  if (t62 >= t65) goto label5;
  v16 = t62;
  {
  t66 = v2;
  t67 = ((int64_t)(v10));
  t68 = ((int64_t)(v16));
  t69 = ((int64_t)((uint64_t)(t67) + (uint64_t)(t68)));
  t70 = ((uint16_t *)t66.data) + gg_index(t69, t66.len);
  t71 = ((uint16_t)((*t70)));
  t72 = fg_el_half(t71);
  v17 = ((float)(t72));
  t73 = ((float)(v17));
  t74 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t75 = ((t73) > (t74));
  if (t75) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t76 = ((int64_t)UINT64_C(1));
  t77 = ((float *)(((*(v0))).f10).data) + gg_index(t76, ((int64_t)UINT64_C(4)));
  t78 = ((float)((*t77)));
  t79 = ((float)(v17));
  t80 = ((float)(((t78) * (t79))));
  t81 = ((float)(v17));
  /* FG_DIRECT_SLICE_INDEX */
  t82 = ((int64_t)UINT64_C(2));
  t83 = ((float *)(((*(v0))).f10).data) + gg_index(t82, ((int64_t)UINT64_C(4)));
  t84 = ((float)((*t83)));
  t85 = ((float)(v17));
  t86 = ((float)(((t84) * (t85))));
  t87 = fmaf(t80, t81, t86);
  v18 = ((float)(t87));
  }
  } else {
  {
  t88 = ((float)(v17));
  /* FG_DIRECT_SLICE_INDEX */
  t89 = ((int64_t)UINT64_C(3));
  t90 = ((float *)(((*(v0))).f10).data) + gg_index(t89, ((int64_t)UINT64_C(4)));
  t91 = ((float)((*t90)));
  t92 = fminf(t88, t91);
  t93 = ((float)(t92));
  t94 = expm1f(t93);
  t95 = ((float)(t94));
  t96 = ((float)(v17));
  t97 = ((float)(((t95) - (t96))));
  /* FG_DIRECT_SLICE_INDEX */
  t98 = ((int64_t)UINT64_C(0));
  t99 = ((float *)(((*(v0))).f10).data) + gg_index(t98, ((int64_t)UINT64_C(4)));
  t100 = ((float)((*t99)));
  /* FG_DIRECT_SLICE_INDEX */
  t101 = ((int64_t)UINT64_C(2));
  t102 = ((float *)(((*(v0))).f10).data) + gg_index(t101, ((int64_t)UINT64_C(4)));
  t103 = ((float)((*t102)));
  t104 = ((float)(v17));
  t105 = ((float)(((t103) * (t104))));
  t106 = fmaf(t97, t100, t105);
  v18 = ((float)(t106));
  }
  }
  t107 = v6;
  t108 = ((int64_t)(v12));
  t109 = ((int64_t)(v16));
  t110 = ((int64_t)((uint64_t)(t108) + (uint64_t)(t109)));
  t111 = ((uint16_t *)t107.data) + gg_index(t110, t107.len);
  t112 = ((float)(v18));
  t113 = fg_el_to_half(t112);
  (*t111) = ((uint16_t)(t113));
  }
label4:;
  t62++; goto label3;
label5:;
  }
  } else {
  t114 = ((int64_t)((((*(v0))).f3).f2));
  t115 = ((int64_t)(((int64_t)UINT64_C(2))));
  t116 = ((t114) == (t115));
  t117 = t116;
  if (t117) {
  t118 = ((int64_t)((((*(v0))).f5).f2));
  t119 = ((int64_t)(((int64_t)UINT64_C(2))));
  t117 = ((t118) == (t119));
  }
  t120 = t117;
  if (t120) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t122 = ((int64_t)UINT64_C(0));
  t123 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t122, ((int64_t)UINT64_C(4)));
  t124 = (*t123);
  t121 = 0;
label6:;
  if (t121 >= t124) goto label8;
  v19 = t121;
  {
  t125 = v2;
  t126 = ((int64_t)(v10));
  t127 = ((int64_t)(v19));
  t128 = ((int64_t)((uint64_t)(t126) + (uint64_t)(t127)));
  t129 = ((uint16_t *)t125.data) + gg_index(t128, t125.len);
  t130 = ((uint16_t)((*t129)));
  v20 = gg_function_11(t130, base + 0);
  t131 = ((float)(v20));
  t132 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t133 = ((t131) > (t132));
  if (t133) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t134 = ((int64_t)UINT64_C(1));
  t135 = ((float *)(((*(v0))).f10).data) + gg_index(t134, ((int64_t)UINT64_C(4)));
  t136 = ((float)((*t135)));
  t137 = ((float)(v20));
  t138 = ((float)(((t136) * (t137))));
  t139 = ((float)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t140 = ((int64_t)UINT64_C(2));
  t141 = ((float *)(((*(v0))).f10).data) + gg_index(t140, ((int64_t)UINT64_C(4)));
  t142 = ((float)((*t141)));
  t143 = ((float)(v20));
  t144 = ((float)(((t142) * (t143))));
  t145 = fmaf(t138, t139, t144);
  v21 = ((float)(t145));
  }
  } else {
  {
  t146 = ((float)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t147 = ((int64_t)UINT64_C(3));
  t148 = ((float *)(((*(v0))).f10).data) + gg_index(t147, ((int64_t)UINT64_C(4)));
  t149 = ((float)((*t148)));
  t150 = fminf(t146, t149);
  t151 = ((float)(t150));
  t152 = expm1f(t151);
  t153 = ((float)(t152));
  t154 = ((float)(v20));
  t155 = ((float)(((t153) - (t154))));
  /* FG_DIRECT_SLICE_INDEX */
  t156 = ((int64_t)UINT64_C(0));
  t157 = ((float *)(((*(v0))).f10).data) + gg_index(t156, ((int64_t)UINT64_C(4)));
  t158 = ((float)((*t157)));
  /* FG_DIRECT_SLICE_INDEX */
  t159 = ((int64_t)UINT64_C(2));
  t160 = ((float *)(((*(v0))).f10).data) + gg_index(t159, ((int64_t)UINT64_C(4)));
  t161 = ((float)((*t160)));
  t162 = ((float)(v20));
  t163 = ((float)(((t161) * (t162))));
  t164 = fmaf(t155, t158, t163);
  v21 = ((float)(t164));
  }
  }
  t165 = v6;
  t166 = ((int64_t)(v12));
  t167 = ((int64_t)(v19));
  t168 = ((int64_t)((uint64_t)(t166) + (uint64_t)(t167)));
  t169 = ((uint16_t *)t165.data) + gg_index(t168, t165.len);
  t170 = ((float)(v21));
  (*t169) = gg_function_13(t170, base + 0);
  }
label7:;
  t121++; goto label6;
label8:;
  }
  } else {
  t171 = ((int64_t)((((*(v0))).f3).f2));
  t172 = ((int64_t)(((int64_t)UINT64_C(2))));
  t173 = ((t171) == (t172));
  t174 = t173;
  if (t174) {
  t175 = ((int64_t)((((*(v0))).f5).f2));
  t176 = ((int64_t)(((int64_t)UINT64_C(0))));
  t174 = ((t175) == (t176));
  }
  t177 = t174;
  if (t177) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t179 = ((int64_t)UINT64_C(0));
  t180 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t179, ((int64_t)UINT64_C(4)));
  t181 = (*t180);
  t178 = 0;
label9:;
  if (t178 >= t181) goto label11;
  v22 = t178;
  {
  t182 = v2;
  t183 = ((int64_t)(v10));
  t184 = ((int64_t)(v22));
  t185 = ((int64_t)((uint64_t)(t183) + (uint64_t)(t184)));
  t186 = ((uint16_t *)t182.data) + gg_index(t185, t182.len);
  t187 = ((uint16_t)((*t186)));
  v23 = gg_function_11(t187, base + 0);
  t188 = ((float)(v23));
  t189 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t190 = ((t188) > (t189));
  if (t190) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t191 = ((int64_t)UINT64_C(1));
  t192 = ((float *)(((*(v0))).f10).data) + gg_index(t191, ((int64_t)UINT64_C(4)));
  t193 = ((float)((*t192)));
  t194 = ((float)(v23));
  t195 = ((float)(((t193) * (t194))));
  t196 = ((float)(v23));
  /* FG_DIRECT_SLICE_INDEX */
  t197 = ((int64_t)UINT64_C(2));
  t198 = ((float *)(((*(v0))).f10).data) + gg_index(t197, ((int64_t)UINT64_C(4)));
  t199 = ((float)((*t198)));
  t200 = ((float)(v23));
  t201 = ((float)(((t199) * (t200))));
  t202 = fmaf(t195, t196, t201);
  v24 = ((float)(t202));
  }
  } else {
  {
  t203 = ((float)(v23));
  /* FG_DIRECT_SLICE_INDEX */
  t204 = ((int64_t)UINT64_C(3));
  t205 = ((float *)(((*(v0))).f10).data) + gg_index(t204, ((int64_t)UINT64_C(4)));
  t206 = ((float)((*t205)));
  t207 = fminf(t203, t206);
  t208 = ((float)(t207));
  t209 = expm1f(t208);
  t210 = ((float)(t209));
  t211 = ((float)(v23));
  t212 = ((float)(((t210) - (t211))));
  /* FG_DIRECT_SLICE_INDEX */
  t213 = ((int64_t)UINT64_C(0));
  t214 = ((float *)(((*(v0))).f10).data) + gg_index(t213, ((int64_t)UINT64_C(4)));
  t215 = ((float)((*t214)));
  /* FG_DIRECT_SLICE_INDEX */
  t216 = ((int64_t)UINT64_C(2));
  t217 = ((float *)(((*(v0))).f10).data) + gg_index(t216, ((int64_t)UINT64_C(4)));
  t218 = ((float)((*t217)));
  t219 = ((float)(v23));
  t220 = ((float)(((t218) * (t219))));
  t221 = fmaf(t212, t215, t220);
  v24 = ((float)(t221));
  }
  }
  t222 = v5;
  t223 = ((int64_t)(v12));
  t224 = ((int64_t)(v22));
  t225 = ((int64_t)((uint64_t)(t223) + (uint64_t)(t224)));
  t226 = ((float *)t222.data) + gg_index(t225, t222.len);
  (*t226) = ((float)(v24));
  }
label10:;
  t178++; goto label9;
label11:;
  }
  } else {
  t227 = ((int64_t)((((*(v0))).f3).f2));
  t228 = ((int64_t)(((int64_t)UINT64_C(1))));
  t229 = ((t227) == (t228));
  t230 = t229;
  if (t230) {
  t231 = ((int64_t)((((*(v0))).f5).f2));
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  t230 = ((t231) == (t232));
  }
  t233 = t230;
  if (t233) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t235 = ((int64_t)UINT64_C(0));
  t236 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t235, ((int64_t)UINT64_C(4)));
  t237 = (*t236);
  t234 = 0;
label12:;
  if (t234 >= t237) goto label14;
  v25 = t234;
  {
  t238 = v2;
  t239 = ((int64_t)(v10));
  t240 = ((int64_t)(v25));
  t241 = ((int64_t)((uint64_t)(t239) + (uint64_t)(t240)));
  t242 = ((uint16_t *)t238.data) + gg_index(t241, t238.len);
  t243 = ((uint16_t)((*t242)));
  t244 = fg_el_half(t243);
  v26 = ((float)(t244));
  t245 = ((float)(v26));
  t246 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t247 = ((t245) > (t246));
  if (t247) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t248 = ((int64_t)UINT64_C(1));
  t249 = ((float *)(((*(v0))).f10).data) + gg_index(t248, ((int64_t)UINT64_C(4)));
  t250 = ((float)((*t249)));
  t251 = ((float)(v26));
  t252 = ((float)(((t250) * (t251))));
  t253 = ((float)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t254 = ((int64_t)UINT64_C(2));
  t255 = ((float *)(((*(v0))).f10).data) + gg_index(t254, ((int64_t)UINT64_C(4)));
  t256 = ((float)((*t255)));
  t257 = ((float)(v26));
  t258 = ((float)(((t256) * (t257))));
  t259 = fmaf(t252, t253, t258);
  v27 = ((float)(t259));
  }
  } else {
  {
  t260 = ((float)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t261 = ((int64_t)UINT64_C(3));
  t262 = ((float *)(((*(v0))).f10).data) + gg_index(t261, ((int64_t)UINT64_C(4)));
  t263 = ((float)((*t262)));
  t264 = fminf(t260, t263);
  t265 = ((float)(t264));
  t266 = expm1f(t265);
  t267 = ((float)(t266));
  t268 = ((float)(v26));
  t269 = ((float)(((t267) - (t268))));
  /* FG_DIRECT_SLICE_INDEX */
  t270 = ((int64_t)UINT64_C(0));
  t271 = ((float *)(((*(v0))).f10).data) + gg_index(t270, ((int64_t)UINT64_C(4)));
  t272 = ((float)((*t271)));
  /* FG_DIRECT_SLICE_INDEX */
  t273 = ((int64_t)UINT64_C(2));
  t274 = ((float *)(((*(v0))).f10).data) + gg_index(t273, ((int64_t)UINT64_C(4)));
  t275 = ((float)((*t274)));
  t276 = ((float)(v26));
  t277 = ((float)(((t275) * (t276))));
  t278 = fmaf(t269, t272, t277);
  v27 = ((float)(t278));
  }
  }
  t279 = v5;
  t280 = ((int64_t)(v12));
  t281 = ((int64_t)(v25));
  t282 = ((int64_t)((uint64_t)(t280) + (uint64_t)(t281)));
  t283 = ((float *)t279.data) + gg_index(t282, t279.len);
  (*t283) = ((float)(v27));
  }
label13:;
  t234++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_47(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = truncf(t16);
  v15 = ((float)(t17));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = truncf(t41);
  v18 = ((float)(t42));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = truncf(t67);
  v21 = ((float)(t68));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = truncf(t92);
  v24 = ((float)(t93));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = truncf(t117);
  v27 = ((float)(t118));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_45(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = roundf(t16);
  v15 = ((float)(t17));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = roundf(t41);
  v18 = ((float)(t42));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = roundf(t67);
  v21 = ((float)(t68));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = roundf(t92);
  v24 = ((float)(t93));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = roundf(t117);
  v27 = ((float)(t118));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_43(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = ceilf(t16);
  v15 = ((float)(t17));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = ceilf(t41);
  v18 = ((float)(t42));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = ceilf(t67);
  v21 = ((float)(t68));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = ceilf(t92);
  v24 = ((float)(t93));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = ceilf(t117);
  v27 = ((float)(t118));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_41(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = floorf(t16);
  v15 = ((float)(t17));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = floorf(t41);
  v18 = ((float)(t42));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = floorf(t67);
  v21 = ((float)(t68));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = floorf(t92);
  v24 = ((float)(t93));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = floorf(t117);
  v27 = ((float)(t118));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_40(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  uint8_t t18;
  float t19;
  float t20;
  float t21;
  float t22;
  float t23;
  float t24;
  gg_slice t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  float * t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t * t39;
  int64_t t40;
  gg_slice t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint16_t * t45;
  uint16_t t46;
  float t47;
  float t48;
  float t49;
  uint8_t t50;
  float t51;
  float t52;
  float t53;
  float t54;
  float t55;
  float t56;
  gg_slice t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint16_t * t61;
  float t62;
  uint16_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  uint8_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  int64_t * t73;
  int64_t t74;
  gg_slice t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint16_t * t79;
  uint16_t t80;
  float t81;
  float t82;
  uint8_t t83;
  float t84;
  float t85;
  float t86;
  float t87;
  float t88;
  float t89;
  gg_slice t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  uint16_t * t94;
  float t95;
  int64_t t96;
  int64_t t97;
  uint8_t t98;
  uint8_t t99;
  int64_t t100;
  int64_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  int64_t * t105;
  int64_t t106;
  gg_slice t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  uint16_t * t111;
  uint16_t t112;
  float t113;
  float t114;
  uint8_t t115;
  float t116;
  float t117;
  float t118;
  float t119;
  float t120;
  float t121;
  gg_slice t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  float * t126;
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
  gg_slice t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  uint16_t * t142;
  uint16_t t143;
  float t144;
  float t145;
  float t146;
  uint8_t t147;
  float t148;
  float t149;
  float t150;
  float t151;
  float t152;
  float t153;
  gg_slice t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  float * t158;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = ((float)(((float)(gg_double_bits(UINT64_C(4626322717216342016))))));
  t18 = ((t16) > (t17));
  if (t18) {
  {
  v15 = ((float)(v14));
  }
  } else {
  {
  t19 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t20 = ((float)(v14));
  t21 = expf(t20);
  t22 = ((float)(t21));
  t23 = ((float)(((t19) + (t22))));
  t24 = logf(t23);
  v15 = ((float)(t24));
  }
  }
  t25 = v5;
  t26 = ((int64_t)(v12));
  t27 = ((int64_t)(v13));
  t28 = ((int64_t)((uint64_t)(t26) + (uint64_t)(t27)));
  t29 = ((float *)t25.data) + gg_index(t28, t25.len);
  (*t29) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t30 = ((int64_t)((((*(v0))).f3).f2));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((t30) == (t31));
  t33 = t32;
  if (t33) {
  t34 = ((int64_t)((((*(v0))).f5).f2));
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  t33 = ((t34) == (t35));
  }
  t36 = t33;
  if (t36) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t38 = ((int64_t)UINT64_C(0));
  t39 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t38, ((int64_t)UINT64_C(4)));
  t40 = (*t39);
  t37 = 0;
label3:;
  if (t37 >= t40) goto label5;
  v16 = t37;
  {
  t41 = v2;
  t42 = ((int64_t)(v10));
  t43 = ((int64_t)(v16));
  t44 = ((int64_t)((uint64_t)(t42) + (uint64_t)(t43)));
  t45 = ((uint16_t *)t41.data) + gg_index(t44, t41.len);
  t46 = ((uint16_t)((*t45)));
  t47 = fg_el_half(t46);
  v17 = ((float)(t47));
  t48 = ((float)(v17));
  t49 = ((float)(((float)(gg_double_bits(UINT64_C(4626322717216342016))))));
  t50 = ((t48) > (t49));
  if (t50) {
  {
  v18 = ((float)(v17));
  }
  } else {
  {
  t51 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t52 = ((float)(v17));
  t53 = expf(t52);
  t54 = ((float)(t53));
  t55 = ((float)(((t51) + (t54))));
  t56 = logf(t55);
  v18 = ((float)(t56));
  }
  }
  t57 = v6;
  t58 = ((int64_t)(v12));
  t59 = ((int64_t)(v16));
  t60 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t59)));
  t61 = ((uint16_t *)t57.data) + gg_index(t60, t57.len);
  t62 = ((float)(v18));
  t63 = fg_el_to_half(t62);
  (*t61) = ((uint16_t)(t63));
  }
label4:;
  t37++; goto label3;
label5:;
  }
  } else {
  t64 = ((int64_t)((((*(v0))).f3).f2));
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t66 = ((t64) == (t65));
  t67 = t66;
  if (t67) {
  t68 = ((int64_t)((((*(v0))).f5).f2));
  t69 = ((int64_t)(((int64_t)UINT64_C(2))));
  t67 = ((t68) == (t69));
  }
  t70 = t67;
  if (t70) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t72 = ((int64_t)UINT64_C(0));
  t73 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t72, ((int64_t)UINT64_C(4)));
  t74 = (*t73);
  t71 = 0;
label6:;
  if (t71 >= t74) goto label8;
  v19 = t71;
  {
  t75 = v2;
  t76 = ((int64_t)(v10));
  t77 = ((int64_t)(v19));
  t78 = ((int64_t)((uint64_t)(t76) + (uint64_t)(t77)));
  t79 = ((uint16_t *)t75.data) + gg_index(t78, t75.len);
  t80 = ((uint16_t)((*t79)));
  v20 = gg_function_11(t80, base + 0);
  t81 = ((float)(v20));
  t82 = ((float)(((float)(gg_double_bits(UINT64_C(4626322717216342016))))));
  t83 = ((t81) > (t82));
  if (t83) {
  {
  v21 = ((float)(v20));
  }
  } else {
  {
  t84 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t85 = ((float)(v20));
  t86 = expf(t85);
  t87 = ((float)(t86));
  t88 = ((float)(((t84) + (t87))));
  t89 = logf(t88);
  v21 = ((float)(t89));
  }
  }
  t90 = v6;
  t91 = ((int64_t)(v12));
  t92 = ((int64_t)(v19));
  t93 = ((int64_t)((uint64_t)(t91) + (uint64_t)(t92)));
  t94 = ((uint16_t *)t90.data) + gg_index(t93, t90.len);
  t95 = ((float)(v21));
  (*t94) = gg_function_13(t95, base + 0);
  }
label7:;
  t71++; goto label6;
label8:;
  }
  } else {
  t96 = ((int64_t)((((*(v0))).f3).f2));
  t97 = ((int64_t)(((int64_t)UINT64_C(2))));
  t98 = ((t96) == (t97));
  t99 = t98;
  if (t99) {
  t100 = ((int64_t)((((*(v0))).f5).f2));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((t100) == (t101));
  }
  t102 = t99;
  if (t102) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t104 = ((int64_t)UINT64_C(0));
  t105 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t104, ((int64_t)UINT64_C(4)));
  t106 = (*t105);
  t103 = 0;
label9:;
  if (t103 >= t106) goto label11;
  v22 = t103;
  {
  t107 = v2;
  t108 = ((int64_t)(v10));
  t109 = ((int64_t)(v22));
  t110 = ((int64_t)((uint64_t)(t108) + (uint64_t)(t109)));
  t111 = ((uint16_t *)t107.data) + gg_index(t110, t107.len);
  t112 = ((uint16_t)((*t111)));
  v23 = gg_function_11(t112, base + 0);
  t113 = ((float)(v23));
  t114 = ((float)(((float)(gg_double_bits(UINT64_C(4626322717216342016))))));
  t115 = ((t113) > (t114));
  if (t115) {
  {
  v24 = ((float)(v23));
  }
  } else {
  {
  t116 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t117 = ((float)(v23));
  t118 = expf(t117);
  t119 = ((float)(t118));
  t120 = ((float)(((t116) + (t119))));
  t121 = logf(t120);
  v24 = ((float)(t121));
  }
  }
  t122 = v5;
  t123 = ((int64_t)(v12));
  t124 = ((int64_t)(v22));
  t125 = ((int64_t)((uint64_t)(t123) + (uint64_t)(t124)));
  t126 = ((float *)t122.data) + gg_index(t125, t122.len);
  (*t126) = ((float)(v24));
  }
label10:;
  t103++; goto label9;
label11:;
  }
  } else {
  t127 = ((int64_t)((((*(v0))).f3).f2));
  t128 = ((int64_t)(((int64_t)UINT64_C(1))));
  t129 = ((t127) == (t128));
  t130 = t129;
  if (t130) {
  t131 = ((int64_t)((((*(v0))).f5).f2));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t130 = ((t131) == (t132));
  }
  t133 = t130;
  if (t133) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t135 = ((int64_t)UINT64_C(0));
  t136 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t135, ((int64_t)UINT64_C(4)));
  t137 = (*t136);
  t134 = 0;
label12:;
  if (t134 >= t137) goto label14;
  v25 = t134;
  {
  t138 = v2;
  t139 = ((int64_t)(v10));
  t140 = ((int64_t)(v25));
  t141 = ((int64_t)((uint64_t)(t139) + (uint64_t)(t140)));
  t142 = ((uint16_t *)t138.data) + gg_index(t141, t138.len);
  t143 = ((uint16_t)((*t142)));
  t144 = fg_el_half(t143);
  v26 = ((float)(t144));
  t145 = ((float)(v26));
  t146 = ((float)(((float)(gg_double_bits(UINT64_C(4626322717216342016))))));
  t147 = ((t145) > (t146));
  if (t147) {
  {
  v27 = ((float)(v26));
  }
  } else {
  {
  t148 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t149 = ((float)(v26));
  t150 = expf(t149);
  t151 = ((float)(t150));
  t152 = ((float)(((t148) + (t151))));
  t153 = logf(t152);
  v27 = ((float)(t153));
  }
  }
  t154 = v5;
  t155 = ((int64_t)(v12));
  t156 = ((int64_t)(v25));
  t157 = ((int64_t)((uint64_t)(t155) + (uint64_t)(t156)));
  t158 = ((float *)t154.data) + gg_index(t157, t154.len);
  (*t158) = ((float)(v27));
  }
label13:;
  t134++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_39(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  float t18;
  float t19;
  gg_slice t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  float * t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t * t34;
  int64_t t35;
  gg_slice t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint16_t * t40;
  uint16_t t41;
  float t42;
  float t43;
  float t44;
  float t45;
  float t46;
  gg_slice t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint16_t * t51;
  float t52;
  uint16_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  uint8_t t57;
  int64_t t58;
  int64_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  int64_t * t63;
  int64_t t64;
  gg_slice t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint16_t * t69;
  uint16_t t70;
  float t71;
  float t72;
  float t73;
  float t74;
  gg_slice t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint16_t * t79;
  float t80;
  int64_t t81;
  int64_t t82;
  uint8_t t83;
  uint8_t t84;
  int64_t t85;
  int64_t t86;
  uint8_t t87;
  int64_t t88;
  int64_t t89;
  int64_t * t90;
  int64_t t91;
  gg_slice t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint16_t * t96;
  uint16_t t97;
  float t98;
  float t99;
  float t100;
  float t101;
  gg_slice t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  float * t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  uint8_t t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  int64_t t114;
  int64_t t115;
  int64_t * t116;
  int64_t t117;
  gg_slice t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  uint16_t * t122;
  uint16_t t123;
  float t124;
  float t125;
  float t126;
  float t127;
  float t128;
  gg_slice t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  float * t133;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = expf(t16);
  t18 = ((float)(t17));
  t19 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  v15 = ((float)(((t18) - (t19))));
  t20 = v5;
  t21 = ((int64_t)(v12));
  t22 = ((int64_t)(v13));
  t23 = ((int64_t)((uint64_t)(t21) + (uint64_t)(t22)));
  t24 = ((float *)t20.data) + gg_index(t23, t20.len);
  (*t24) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t25 = ((int64_t)((((*(v0))).f3).f2));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((t25) == (t26));
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)((((*(v0))).f5).f2));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = ((t29) == (t30));
  }
  t31 = t28;
  if (t31) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t33 = ((int64_t)UINT64_C(0));
  t34 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t33, ((int64_t)UINT64_C(4)));
  t35 = (*t34);
  t32 = 0;
label3:;
  if (t32 >= t35) goto label5;
  v16 = t32;
  {
  t36 = v2;
  t37 = ((int64_t)(v10));
  t38 = ((int64_t)(v16));
  t39 = ((int64_t)((uint64_t)(t37) + (uint64_t)(t38)));
  t40 = ((uint16_t *)t36.data) + gg_index(t39, t36.len);
  t41 = ((uint16_t)((*t40)));
  t42 = fg_el_half(t41);
  v17 = ((float)(t42));
  t43 = ((float)(v17));
  t44 = expf(t43);
  t45 = ((float)(t44));
  t46 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  v18 = ((float)(((t45) - (t46))));
  t47 = v6;
  t48 = ((int64_t)(v12));
  t49 = ((int64_t)(v16));
  t50 = ((int64_t)((uint64_t)(t48) + (uint64_t)(t49)));
  t51 = ((uint16_t *)t47.data) + gg_index(t50, t47.len);
  t52 = ((float)(v18));
  t53 = fg_el_to_half(t52);
  (*t51) = ((uint16_t)(t53));
  }
label4:;
  t32++; goto label3;
label5:;
  }
  } else {
  t54 = ((int64_t)((((*(v0))).f3).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t56 = ((t54) == (t55));
  t57 = t56;
  if (t57) {
  t58 = ((int64_t)((((*(v0))).f5).f2));
  t59 = ((int64_t)(((int64_t)UINT64_C(2))));
  t57 = ((t58) == (t59));
  }
  t60 = t57;
  if (t60) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t62 = ((int64_t)UINT64_C(0));
  t63 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t62, ((int64_t)UINT64_C(4)));
  t64 = (*t63);
  t61 = 0;
label6:;
  if (t61 >= t64) goto label8;
  v19 = t61;
  {
  t65 = v2;
  t66 = ((int64_t)(v10));
  t67 = ((int64_t)(v19));
  t68 = ((int64_t)((uint64_t)(t66) + (uint64_t)(t67)));
  t69 = ((uint16_t *)t65.data) + gg_index(t68, t65.len);
  t70 = ((uint16_t)((*t69)));
  v20 = gg_function_11(t70, base + 0);
  t71 = ((float)(v20));
  t72 = expf(t71);
  t73 = ((float)(t72));
  t74 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  v21 = ((float)(((t73) - (t74))));
  t75 = v6;
  t76 = ((int64_t)(v12));
  t77 = ((int64_t)(v19));
  t78 = ((int64_t)((uint64_t)(t76) + (uint64_t)(t77)));
  t79 = ((uint16_t *)t75.data) + gg_index(t78, t75.len);
  t80 = ((float)(v21));
  (*t79) = gg_function_13(t80, base + 0);
  }
label7:;
  t61++; goto label6;
label8:;
  }
  } else {
  t81 = ((int64_t)((((*(v0))).f3).f2));
  t82 = ((int64_t)(((int64_t)UINT64_C(2))));
  t83 = ((t81) == (t82));
  t84 = t83;
  if (t84) {
  t85 = ((int64_t)((((*(v0))).f5).f2));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = ((t85) == (t86));
  }
  t87 = t84;
  if (t87) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t89 = ((int64_t)UINT64_C(0));
  t90 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t89, ((int64_t)UINT64_C(4)));
  t91 = (*t90);
  t88 = 0;
label9:;
  if (t88 >= t91) goto label11;
  v22 = t88;
  {
  t92 = v2;
  t93 = ((int64_t)(v10));
  t94 = ((int64_t)(v22));
  t95 = ((int64_t)((uint64_t)(t93) + (uint64_t)(t94)));
  t96 = ((uint16_t *)t92.data) + gg_index(t95, t92.len);
  t97 = ((uint16_t)((*t96)));
  v23 = gg_function_11(t97, base + 0);
  t98 = ((float)(v23));
  t99 = expf(t98);
  t100 = ((float)(t99));
  t101 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  v24 = ((float)(((t100) - (t101))));
  t102 = v5;
  t103 = ((int64_t)(v12));
  t104 = ((int64_t)(v22));
  t105 = ((int64_t)((uint64_t)(t103) + (uint64_t)(t104)));
  t106 = ((float *)t102.data) + gg_index(t105, t102.len);
  (*t106) = ((float)(v24));
  }
label10:;
  t88++; goto label9;
label11:;
  }
  } else {
  t107 = ((int64_t)((((*(v0))).f3).f2));
  t108 = ((int64_t)(((int64_t)UINT64_C(1))));
  t109 = ((t107) == (t108));
  t110 = t109;
  if (t110) {
  t111 = ((int64_t)((((*(v0))).f5).f2));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = ((t111) == (t112));
  }
  t113 = t110;
  if (t113) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t115 = ((int64_t)UINT64_C(0));
  t116 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t115, ((int64_t)UINT64_C(4)));
  t117 = (*t116);
  t114 = 0;
label12:;
  if (t114 >= t117) goto label14;
  v25 = t114;
  {
  t118 = v2;
  t119 = ((int64_t)(v10));
  t120 = ((int64_t)(v25));
  t121 = ((int64_t)((uint64_t)(t119) + (uint64_t)(t120)));
  t122 = ((uint16_t *)t118.data) + gg_index(t121, t118.len);
  t123 = ((uint16_t)((*t122)));
  t124 = fg_el_half(t123);
  v26 = ((float)(t124));
  t125 = ((float)(v26));
  t126 = expf(t125);
  t127 = ((float)(t126));
  t128 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  v27 = ((float)(((t127) - (t128))));
  t129 = v5;
  t130 = ((int64_t)(v12));
  t131 = ((int64_t)(v25));
  t132 = ((int64_t)((uint64_t)(t130) + (uint64_t)(t131)));
  t133 = ((float *)t129.data) + gg_index(t132, t129.len);
  (*t133) = ((float)(v27));
  }
label13:;
  t114++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_37(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = logf(t16);
  v15 = ((float)(t17));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = logf(t41);
  v18 = ((float)(t42));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = logf(t67);
  v21 = ((float)(t68));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = logf(t92);
  v24 = ((float)(t93));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = logf(t117);
  v27 = ((float)(t118));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_35(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = cosf(t16);
  v15 = ((float)(t17));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = cosf(t41);
  v18 = ((float)(t42));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = cosf(t67);
  v21 = ((float)(t68));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = cosf(t92);
  v24 = ((float)(t93));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = cosf(t117);
  v27 = ((float)(t118));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_33(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = sinf(t16);
  v15 = ((float)(t17));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = sinf(t41);
  v18 = ((float)(t42));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = sinf(t67);
  v21 = ((float)(t68));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = sinf(t92);
  v24 = ((float)(t93));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = sinf(t117);
  v27 = ((float)(t118));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_31(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = sqrtf(t16);
  v15 = ((float)(t17));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = sqrtf(t41);
  v18 = ((float)(t42));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = sqrtf(t67);
  v21 = ((float)(t68));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = sqrtf(t92);
  v24 = ((float)(t93));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = sqrtf(t117);
  v27 = ((float)(t118));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_30(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = ((float)(v14));
  v15 = ((float)(((t16) * (t17))));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = ((float)(v17));
  v18 = ((float)(((t41) * (t42))));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = ((float)(v20));
  v21 = ((float)(((t67) * (t68))));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = ((float)(v23));
  v24 = ((float)(((t92) * (t93))));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = ((float)(v26));
  v27 = ((float)(((t117) * (t118))));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_29(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  float t18;
  float t19;
  float t20;
  float t21;
  float t22;
  float t23;
  float t24;
  float t25;
  float t26;
  float t27;
  gg_slice t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  float * t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  int64_t * t42;
  int64_t t43;
  gg_slice t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint16_t * t48;
  uint16_t t49;
  float t50;
  float t51;
  float t52;
  float t53;
  float t54;
  float t55;
  float t56;
  float t57;
  float t58;
  float t59;
  float t60;
  float t61;
  float t62;
  gg_slice t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  uint16_t * t67;
  float t68;
  uint16_t t69;
  int64_t t70;
  int64_t t71;
  uint8_t t72;
  uint8_t t73;
  int64_t t74;
  int64_t t75;
  uint8_t t76;
  int64_t t77;
  int64_t t78;
  int64_t * t79;
  int64_t t80;
  gg_slice t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint16_t * t85;
  uint16_t t86;
  float t87;
  float t88;
  float t89;
  float t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  float t97;
  float t98;
  gg_slice t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint16_t * t103;
  float t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
  uint8_t t108;
  int64_t t109;
  int64_t t110;
  uint8_t t111;
  int64_t t112;
  int64_t t113;
  int64_t * t114;
  int64_t t115;
  gg_slice t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  uint16_t * t120;
  uint16_t t121;
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
  gg_slice t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  float * t138;
  int64_t t139;
  int64_t t140;
  uint8_t t141;
  uint8_t t142;
  int64_t t143;
  int64_t t144;
  uint8_t t145;
  int64_t t146;
  int64_t t147;
  int64_t * t148;
  int64_t t149;
  gg_slice t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  uint16_t * t154;
  uint16_t t155;
  float t156;
  float t157;
  float t158;
  float t159;
  float t160;
  float t161;
  float t162;
  float t163;
  float t164;
  float t165;
  float t166;
  float t167;
  float t168;
  gg_slice t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  float * t173;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t18 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t19 = ((float)(v14));
  t20 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t21 = ((float)(((t19) + (t20))));
  t22 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t23 = ((float)(((t21) / (t22))));
  t24 = fmaxf(t18, t23);
  t25 = ((float)(t24));
  t26 = fminf(t17, t25);
  t27 = ((float)(t26));
  v15 = ((float)(((t16) * (t27))));
  t28 = v5;
  t29 = ((int64_t)(v12));
  t30 = ((int64_t)(v13));
  t31 = ((int64_t)((uint64_t)(t29) + (uint64_t)(t30)));
  t32 = ((float *)t28.data) + gg_index(t31, t28.len);
  (*t32) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t33 = ((int64_t)((((*(v0))).f3).f2));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = ((t33) == (t34));
  t36 = t35;
  if (t36) {
  t37 = ((int64_t)((((*(v0))).f5).f2));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t36 = ((t37) == (t38));
  }
  t39 = t36;
  if (t39) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t41 = ((int64_t)UINT64_C(0));
  t42 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t41, ((int64_t)UINT64_C(4)));
  t43 = (*t42);
  t40 = 0;
label3:;
  if (t40 >= t43) goto label5;
  v16 = t40;
  {
  t44 = v2;
  t45 = ((int64_t)(v10));
  t46 = ((int64_t)(v16));
  t47 = ((int64_t)((uint64_t)(t45) + (uint64_t)(t46)));
  t48 = ((uint16_t *)t44.data) + gg_index(t47, t44.len);
  t49 = ((uint16_t)((*t48)));
  t50 = fg_el_half(t49);
  v17 = ((float)(t50));
  t51 = ((float)(v17));
  t52 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t53 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t54 = ((float)(v17));
  t55 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t56 = ((float)(((t54) + (t55))));
  t57 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t58 = ((float)(((t56) / (t57))));
  t59 = fmaxf(t53, t58);
  t60 = ((float)(t59));
  t61 = fminf(t52, t60);
  t62 = ((float)(t61));
  v18 = ((float)(((t51) * (t62))));
  t63 = v6;
  t64 = ((int64_t)(v12));
  t65 = ((int64_t)(v16));
  t66 = ((int64_t)((uint64_t)(t64) + (uint64_t)(t65)));
  t67 = ((uint16_t *)t63.data) + gg_index(t66, t63.len);
  t68 = ((float)(v18));
  t69 = fg_el_to_half(t68);
  (*t67) = ((uint16_t)(t69));
  }
label4:;
  t40++; goto label3;
label5:;
  }
  } else {
  t70 = ((int64_t)((((*(v0))).f3).f2));
  t71 = ((int64_t)(((int64_t)UINT64_C(2))));
  t72 = ((t70) == (t71));
  t73 = t72;
  if (t73) {
  t74 = ((int64_t)((((*(v0))).f5).f2));
  t75 = ((int64_t)(((int64_t)UINT64_C(2))));
  t73 = ((t74) == (t75));
  }
  t76 = t73;
  if (t76) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t78 = ((int64_t)UINT64_C(0));
  t79 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t78, ((int64_t)UINT64_C(4)));
  t80 = (*t79);
  t77 = 0;
label6:;
  if (t77 >= t80) goto label8;
  v19 = t77;
  {
  t81 = v2;
  t82 = ((int64_t)(v10));
  t83 = ((int64_t)(v19));
  t84 = ((int64_t)((uint64_t)(t82) + (uint64_t)(t83)));
  t85 = ((uint16_t *)t81.data) + gg_index(t84, t81.len);
  t86 = ((uint16_t)((*t85)));
  v20 = gg_function_11(t86, base + 0);
  t87 = ((float)(v20));
  t88 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t89 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t90 = ((float)(v20));
  t91 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t92 = ((float)(((t90) + (t91))));
  t93 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t94 = ((float)(((t92) / (t93))));
  t95 = fmaxf(t89, t94);
  t96 = ((float)(t95));
  t97 = fminf(t88, t96);
  t98 = ((float)(t97));
  v21 = ((float)(((t87) * (t98))));
  t99 = v6;
  t100 = ((int64_t)(v12));
  t101 = ((int64_t)(v19));
  t102 = ((int64_t)((uint64_t)(t100) + (uint64_t)(t101)));
  t103 = ((uint16_t *)t99.data) + gg_index(t102, t99.len);
  t104 = ((float)(v21));
  (*t103) = gg_function_13(t104, base + 0);
  }
label7:;
  t77++; goto label6;
label8:;
  }
  } else {
  t105 = ((int64_t)((((*(v0))).f3).f2));
  t106 = ((int64_t)(((int64_t)UINT64_C(2))));
  t107 = ((t105) == (t106));
  t108 = t107;
  if (t108) {
  t109 = ((int64_t)((((*(v0))).f5).f2));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = ((t109) == (t110));
  }
  t111 = t108;
  if (t111) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t113 = ((int64_t)UINT64_C(0));
  t114 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t113, ((int64_t)UINT64_C(4)));
  t115 = (*t114);
  t112 = 0;
label9:;
  if (t112 >= t115) goto label11;
  v22 = t112;
  {
  t116 = v2;
  t117 = ((int64_t)(v10));
  t118 = ((int64_t)(v22));
  t119 = ((int64_t)((uint64_t)(t117) + (uint64_t)(t118)));
  t120 = ((uint16_t *)t116.data) + gg_index(t119, t116.len);
  t121 = ((uint16_t)((*t120)));
  v23 = gg_function_11(t121, base + 0);
  t122 = ((float)(v23));
  t123 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t124 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t125 = ((float)(v23));
  t126 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t127 = ((float)(((t125) + (t126))));
  t128 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t129 = ((float)(((t127) / (t128))));
  t130 = fmaxf(t124, t129);
  t131 = ((float)(t130));
  t132 = fminf(t123, t131);
  t133 = ((float)(t132));
  v24 = ((float)(((t122) * (t133))));
  t134 = v5;
  t135 = ((int64_t)(v12));
  t136 = ((int64_t)(v22));
  t137 = ((int64_t)((uint64_t)(t135) + (uint64_t)(t136)));
  t138 = ((float *)t134.data) + gg_index(t137, t134.len);
  (*t138) = ((float)(v24));
  }
label10:;
  t112++; goto label9;
label11:;
  }
  } else {
  t139 = ((int64_t)((((*(v0))).f3).f2));
  t140 = ((int64_t)(((int64_t)UINT64_C(1))));
  t141 = ((t139) == (t140));
  t142 = t141;
  if (t142) {
  t143 = ((int64_t)((((*(v0))).f5).f2));
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t142 = ((t143) == (t144));
  }
  t145 = t142;
  if (t145) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t147 = ((int64_t)UINT64_C(0));
  t148 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t147, ((int64_t)UINT64_C(4)));
  t149 = (*t148);
  t146 = 0;
label12:;
  if (t146 >= t149) goto label14;
  v25 = t146;
  {
  t150 = v2;
  t151 = ((int64_t)(v10));
  t152 = ((int64_t)(v25));
  t153 = ((int64_t)((uint64_t)(t151) + (uint64_t)(t152)));
  t154 = ((uint16_t *)t150.data) + gg_index(t153, t150.len);
  t155 = ((uint16_t)((*t154)));
  t156 = fg_el_half(t155);
  v26 = ((float)(t156));
  t157 = ((float)(v26));
  t158 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t159 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t160 = ((float)(v26));
  t161 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t162 = ((float)(((t160) + (t161))));
  t163 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t164 = ((float)(((t162) / (t163))));
  t165 = fmaxf(t159, t164);
  t166 = ((float)(t165));
  t167 = fminf(t158, t166);
  t168 = ((float)(t167));
  v27 = ((float)(((t157) * (t168))));
  t169 = v5;
  t170 = ((int64_t)(v12));
  t171 = ((int64_t)(v25));
  t172 = ((int64_t)((uint64_t)(t170) + (uint64_t)(t171)));
  t173 = ((float *)t169.data) + gg_index(t172, t169.len);
  (*t173) = ((float)(v27));
  }
label13:;
  t146++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_28(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = expf(t16);
  v15 = ((float)(t17));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = expf(t41);
  v18 = ((float)(t42));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = expf(t67);
  v21 = ((float)(t68));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = expf(t92);
  v24 = ((float)(t93));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = expf(t117);
  v27 = ((float)(t118));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_25(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  float t18;
  float t19;
  float t20;
  float t21;
  float t22;
  float t23;
  float t24;
  float t25;
  gg_slice t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  float * t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  int64_t * t40;
  int64_t t41;
  gg_slice t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint16_t * t46;
  uint16_t t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
  float t53;
  float t54;
  float t55;
  float t56;
  float t57;
  float t58;
  gg_slice t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint16_t * t63;
  float t64;
  uint16_t t65;
  int64_t t66;
  int64_t t67;
  uint8_t t68;
  uint8_t t69;
  int64_t t70;
  int64_t t71;
  uint8_t t72;
  int64_t t73;
  int64_t t74;
  int64_t * t75;
  int64_t t76;
  gg_slice t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint16_t * t81;
  uint16_t t82;
  float t83;
  float t84;
  float t85;
  float t86;
  float t87;
  float t88;
  float t89;
  float t90;
  float t91;
  float t92;
  gg_slice t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint16_t * t97;
  float t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  float t119;
  float t120;
  float t121;
  float t122;
  float t123;
  float t124;
  float t125;
  gg_slice t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  float * t130;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  uint8_t t134;
  int64_t t135;
  int64_t t136;
  uint8_t t137;
  int64_t t138;
  int64_t t139;
  int64_t * t140;
  int64_t t141;
  gg_slice t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint16_t * t146;
  uint16_t t147;
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
  gg_slice t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  float * t163;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t17 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t18 = ((float)(v14));
  t19 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t20 = ((float)(((t18) + (t19))));
  t21 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t22 = ((float)(((t20) / (t21))));
  t23 = fmaxf(t17, t22);
  t24 = ((float)(t23));
  t25 = fminf(t16, t24);
  v15 = ((float)(t25));
  t26 = v5;
  t27 = ((int64_t)(v12));
  t28 = ((int64_t)(v13));
  t29 = ((int64_t)((uint64_t)(t27) + (uint64_t)(t28)));
  t30 = ((float *)t26.data) + gg_index(t29, t26.len);
  (*t30) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t31 = ((int64_t)((((*(v0))).f3).f2));
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  t33 = ((t31) == (t32));
  t34 = t33;
  if (t34) {
  t35 = ((int64_t)((((*(v0))).f5).f2));
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = ((t35) == (t36));
  }
  t37 = t34;
  if (t37) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t39 = ((int64_t)UINT64_C(0));
  t40 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t39, ((int64_t)UINT64_C(4)));
  t41 = (*t40);
  t38 = 0;
label3:;
  if (t38 >= t41) goto label5;
  v16 = t38;
  {
  t42 = v2;
  t43 = ((int64_t)(v10));
  t44 = ((int64_t)(v16));
  t45 = ((int64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  t46 = ((uint16_t *)t42.data) + gg_index(t45, t42.len);
  t47 = ((uint16_t)((*t46)));
  t48 = fg_el_half(t47);
  v17 = ((float)(t48));
  t49 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t50 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t51 = ((float)(v17));
  t52 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t53 = ((float)(((t51) + (t52))));
  t54 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t55 = ((float)(((t53) / (t54))));
  t56 = fmaxf(t50, t55);
  t57 = ((float)(t56));
  t58 = fminf(t49, t57);
  v18 = ((float)(t58));
  t59 = v6;
  t60 = ((int64_t)(v12));
  t61 = ((int64_t)(v16));
  t62 = ((int64_t)((uint64_t)(t60) + (uint64_t)(t61)));
  t63 = ((uint16_t *)t59.data) + gg_index(t62, t59.len);
  t64 = ((float)(v18));
  t65 = fg_el_to_half(t64);
  (*t63) = ((uint16_t)(t65));
  }
label4:;
  t38++; goto label3;
label5:;
  }
  } else {
  t66 = ((int64_t)((((*(v0))).f3).f2));
  t67 = ((int64_t)(((int64_t)UINT64_C(2))));
  t68 = ((t66) == (t67));
  t69 = t68;
  if (t69) {
  t70 = ((int64_t)((((*(v0))).f5).f2));
  t71 = ((int64_t)(((int64_t)UINT64_C(2))));
  t69 = ((t70) == (t71));
  }
  t72 = t69;
  if (t72) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t74 = ((int64_t)UINT64_C(0));
  t75 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t74, ((int64_t)UINT64_C(4)));
  t76 = (*t75);
  t73 = 0;
label6:;
  if (t73 >= t76) goto label8;
  v19 = t73;
  {
  t77 = v2;
  t78 = ((int64_t)(v10));
  t79 = ((int64_t)(v19));
  t80 = ((int64_t)((uint64_t)(t78) + (uint64_t)(t79)));
  t81 = ((uint16_t *)t77.data) + gg_index(t80, t77.len);
  t82 = ((uint16_t)((*t81)));
  v20 = gg_function_11(t82, base + 0);
  t83 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t84 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t85 = ((float)(v20));
  t86 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t87 = ((float)(((t85) + (t86))));
  t88 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t89 = ((float)(((t87) / (t88))));
  t90 = fmaxf(t84, t89);
  t91 = ((float)(t90));
  t92 = fminf(t83, t91);
  v21 = ((float)(t92));
  t93 = v6;
  t94 = ((int64_t)(v12));
  t95 = ((int64_t)(v19));
  t96 = ((int64_t)((uint64_t)(t94) + (uint64_t)(t95)));
  t97 = ((uint16_t *)t93.data) + gg_index(t96, t93.len);
  t98 = ((float)(v21));
  (*t97) = gg_function_13(t98, base + 0);
  }
label7:;
  t73++; goto label6;
label8:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(2))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label9:;
  if (t106 >= t109) goto label11;
  v22 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v22));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  v23 = gg_function_11(t115, base + 0);
  t116 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t117 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t118 = ((float)(v23));
  t119 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t120 = ((float)(((t118) + (t119))));
  t121 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t122 = ((float)(((t120) / (t121))));
  t123 = fmaxf(t117, t122);
  t124 = ((float)(t123));
  t125 = fminf(t116, t124);
  v24 = ((float)(t125));
  t126 = v5;
  t127 = ((int64_t)(v12));
  t128 = ((int64_t)(v22));
  t129 = ((int64_t)((uint64_t)(t127) + (uint64_t)(t128)));
  t130 = ((float *)t126.data) + gg_index(t129, t126.len);
  (*t130) = ((float)(v24));
  }
label10:;
  t106++; goto label9;
label11:;
  }
  } else {
  t131 = ((int64_t)((((*(v0))).f3).f2));
  t132 = ((int64_t)(((int64_t)UINT64_C(1))));
  t133 = ((t131) == (t132));
  t134 = t133;
  if (t134) {
  t135 = ((int64_t)((((*(v0))).f5).f2));
  t136 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = ((t135) == (t136));
  }
  t137 = t134;
  if (t137) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t139 = ((int64_t)UINT64_C(0));
  t140 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t139, ((int64_t)UINT64_C(4)));
  t141 = (*t140);
  t138 = 0;
label12:;
  if (t138 >= t141) goto label14;
  v25 = t138;
  {
  t142 = v2;
  t143 = ((int64_t)(v10));
  t144 = ((int64_t)(v25));
  t145 = ((int64_t)((uint64_t)(t143) + (uint64_t)(t144)));
  t146 = ((uint16_t *)t142.data) + gg_index(t145, t142.len);
  t147 = ((uint16_t)((*t146)));
  t148 = fg_el_half(t147);
  v26 = ((float)(t148));
  t149 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t150 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t151 = ((float)(v26));
  t152 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t153 = ((float)(((t151) + (t152))));
  t154 = ((float)(((float)(gg_double_bits(UINT64_C(4618441417868443648))))));
  t155 = ((float)(((t153) / (t154))));
  t156 = fmaxf(t150, t155);
  t157 = ((float)(t156));
  t158 = fminf(t149, t157);
  v27 = ((float)(t158));
  t159 = v5;
  t160 = ((int64_t)(v12));
  t161 = ((int64_t)(v25));
  t162 = ((int64_t)((uint64_t)(t160) + (uint64_t)(t161)));
  t163 = ((float *)t159.data) + gg_index(t162, t159.len);
  (*t163) = ((float)(v27));
  }
label13:;
  t138++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_23(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  float t18;
  float t19;
  float t20;
  float t21;
  float t22;
  gg_slice t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  float * t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t * t37;
  int64_t t38;
  gg_slice t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint16_t * t43;
  uint16_t t44;
  float t45;
  float t46;
  float t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
  gg_slice t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint16_t * t57;
  float t58;
  uint16_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  uint8_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  int64_t * t69;
  int64_t t70;
  gg_slice t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint16_t * t75;
  uint16_t t76;
  float t77;
  float t78;
  float t79;
  float t80;
  float t81;
  float t82;
  float t83;
  gg_slice t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  uint16_t * t88;
  float t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
  uint8_t t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  int64_t * t99;
  int64_t t100;
  gg_slice t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint16_t * t105;
  uint16_t t106;
  float t107;
  float t108;
  float t109;
  float t110;
  float t111;
  float t112;
  float t113;
  gg_slice t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  float * t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
  uint8_t t122;
  int64_t t123;
  int64_t t124;
  uint8_t t125;
  int64_t t126;
  int64_t t127;
  int64_t * t128;
  int64_t t129;
  gg_slice t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint16_t * t134;
  uint16_t t135;
  float t136;
  float t137;
  float t138;
  float t139;
  float t140;
  float t141;
  float t142;
  float t143;
  gg_slice t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  float * t148;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t17 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t18 = v14;
  t19 = ((float)((-(t18))));
  t20 = expf(t19);
  t21 = ((float)(t20));
  t22 = ((float)(((t17) + (t21))));
  v15 = ((float)(((t16) / (t22))));
  t23 = v5;
  t24 = ((int64_t)(v12));
  t25 = ((int64_t)(v13));
  t26 = ((int64_t)((uint64_t)(t24) + (uint64_t)(t25)));
  t27 = ((float *)t23.data) + gg_index(t26, t23.len);
  (*t27) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t28 = ((int64_t)((((*(v0))).f3).f2));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((t28) == (t29));
  t31 = t30;
  if (t31) {
  t32 = ((int64_t)((((*(v0))).f5).f2));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((t32) == (t33));
  }
  t34 = t31;
  if (t34) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t36 = ((int64_t)UINT64_C(0));
  t37 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t36, ((int64_t)UINT64_C(4)));
  t38 = (*t37);
  t35 = 0;
label3:;
  if (t35 >= t38) goto label5;
  v16 = t35;
  {
  t39 = v2;
  t40 = ((int64_t)(v10));
  t41 = ((int64_t)(v16));
  t42 = ((int64_t)((uint64_t)(t40) + (uint64_t)(t41)));
  t43 = ((uint16_t *)t39.data) + gg_index(t42, t39.len);
  t44 = ((uint16_t)((*t43)));
  t45 = fg_el_half(t44);
  v17 = ((float)(t45));
  t46 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t47 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t48 = v17;
  t49 = ((float)((-(t48))));
  t50 = expf(t49);
  t51 = ((float)(t50));
  t52 = ((float)(((t47) + (t51))));
  v18 = ((float)(((t46) / (t52))));
  t53 = v6;
  t54 = ((int64_t)(v12));
  t55 = ((int64_t)(v16));
  t56 = ((int64_t)((uint64_t)(t54) + (uint64_t)(t55)));
  t57 = ((uint16_t *)t53.data) + gg_index(t56, t53.len);
  t58 = ((float)(v18));
  t59 = fg_el_to_half(t58);
  (*t57) = ((uint16_t)(t59));
  }
label4:;
  t35++; goto label3;
label5:;
  }
  } else {
  t60 = ((int64_t)((((*(v0))).f3).f2));
  t61 = ((int64_t)(((int64_t)UINT64_C(2))));
  t62 = ((t60) == (t61));
  t63 = t62;
  if (t63) {
  t64 = ((int64_t)((((*(v0))).f5).f2));
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t63 = ((t64) == (t65));
  }
  t66 = t63;
  if (t66) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t68 = ((int64_t)UINT64_C(0));
  t69 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t68, ((int64_t)UINT64_C(4)));
  t70 = (*t69);
  t67 = 0;
label6:;
  if (t67 >= t70) goto label8;
  v19 = t67;
  {
  t71 = v2;
  t72 = ((int64_t)(v10));
  t73 = ((int64_t)(v19));
  t74 = ((int64_t)((uint64_t)(t72) + (uint64_t)(t73)));
  t75 = ((uint16_t *)t71.data) + gg_index(t74, t71.len);
  t76 = ((uint16_t)((*t75)));
  v20 = gg_function_11(t76, base + 0);
  t77 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t78 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t79 = v20;
  t80 = ((float)((-(t79))));
  t81 = expf(t80);
  t82 = ((float)(t81));
  t83 = ((float)(((t78) + (t82))));
  v21 = ((float)(((t77) / (t83))));
  t84 = v6;
  t85 = ((int64_t)(v12));
  t86 = ((int64_t)(v19));
  t87 = ((int64_t)((uint64_t)(t85) + (uint64_t)(t86)));
  t88 = ((uint16_t *)t84.data) + gg_index(t87, t84.len);
  t89 = ((float)(v21));
  (*t88) = gg_function_13(t89, base + 0);
  }
label7:;
  t67++; goto label6;
label8:;
  }
  } else {
  t90 = ((int64_t)((((*(v0))).f3).f2));
  t91 = ((int64_t)(((int64_t)UINT64_C(2))));
  t92 = ((t90) == (t91));
  t93 = t92;
  if (t93) {
  t94 = ((int64_t)((((*(v0))).f5).f2));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t94) == (t95));
  }
  t96 = t93;
  if (t96) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t98 = ((int64_t)UINT64_C(0));
  t99 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t98, ((int64_t)UINT64_C(4)));
  t100 = (*t99);
  t97 = 0;
label9:;
  if (t97 >= t100) goto label11;
  v22 = t97;
  {
  t101 = v2;
  t102 = ((int64_t)(v10));
  t103 = ((int64_t)(v22));
  t104 = ((int64_t)((uint64_t)(t102) + (uint64_t)(t103)));
  t105 = ((uint16_t *)t101.data) + gg_index(t104, t101.len);
  t106 = ((uint16_t)((*t105)));
  v23 = gg_function_11(t106, base + 0);
  t107 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t108 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t109 = v23;
  t110 = ((float)((-(t109))));
  t111 = expf(t110);
  t112 = ((float)(t111));
  t113 = ((float)(((t108) + (t112))));
  v24 = ((float)(((t107) / (t113))));
  t114 = v5;
  t115 = ((int64_t)(v12));
  t116 = ((int64_t)(v22));
  t117 = ((int64_t)((uint64_t)(t115) + (uint64_t)(t116)));
  t118 = ((float *)t114.data) + gg_index(t117, t114.len);
  (*t118) = ((float)(v24));
  }
label10:;
  t97++; goto label9;
label11:;
  }
  } else {
  t119 = ((int64_t)((((*(v0))).f3).f2));
  t120 = ((int64_t)(((int64_t)UINT64_C(1))));
  t121 = ((t119) == (t120));
  t122 = t121;
  if (t122) {
  t123 = ((int64_t)((((*(v0))).f5).f2));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = ((t123) == (t124));
  }
  t125 = t122;
  if (t125) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t127 = ((int64_t)UINT64_C(0));
  t128 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t127, ((int64_t)UINT64_C(4)));
  t129 = (*t128);
  t126 = 0;
label12:;
  if (t126 >= t129) goto label14;
  v25 = t126;
  {
  t130 = v2;
  t131 = ((int64_t)(v10));
  t132 = ((int64_t)(v25));
  t133 = ((int64_t)((uint64_t)(t131) + (uint64_t)(t132)));
  t134 = ((uint16_t *)t130.data) + gg_index(t133, t130.len);
  t135 = ((uint16_t)((*t134)));
  t136 = fg_el_half(t135);
  v26 = ((float)(t136));
  t137 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t138 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  t139 = v26;
  t140 = ((float)((-(t139))));
  t141 = expf(t140);
  t142 = ((float)(t141));
  t143 = ((float)(((t138) + (t142))));
  v27 = ((float)(((t137) / (t143))));
  t144 = v5;
  t145 = ((int64_t)(v12));
  t146 = ((int64_t)(v25));
  t147 = ((int64_t)((uint64_t)(t145) + (uint64_t)(t146)));
  t148 = ((float *)t144.data) + gg_index(t147, t144.len);
  (*t148) = ((float)(v27));
  }
label13:;
  t126++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_22(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  uint8_t t18;
  gg_slice t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  float * t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  int64_t * t33;
  int64_t t34;
  gg_slice t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint16_t * t39;
  uint16_t t40;
  float t41;
  float t42;
  float t43;
  uint8_t t44;
  gg_slice t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint16_t * t49;
  float t50;
  uint16_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  int64_t t59;
  int64_t t60;
  int64_t * t61;
  int64_t t62;
  gg_slice t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  uint16_t * t67;
  uint16_t t68;
  float t69;
  float t70;
  uint8_t t71;
  gg_slice t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint16_t * t76;
  float t77;
  int64_t t78;
  int64_t t79;
  uint8_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  uint8_t t84;
  int64_t t85;
  int64_t t86;
  int64_t * t87;
  int64_t t88;
  gg_slice t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint16_t * t93;
  uint16_t t94;
  float t95;
  float t96;
  uint8_t t97;
  gg_slice t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  float * t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  int64_t * t112;
  int64_t t113;
  gg_slice t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  uint16_t * t118;
  uint16_t t119;
  float t120;
  float t121;
  float t122;
  uint8_t t123;
  gg_slice t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  float * t128;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t18 = ((t16) > (t17));
  if (t18) {
  {
  v15 = ((float)(v14));
  }
  } else {
  {
  v15 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t19 = v5;
  t20 = ((int64_t)(v12));
  t21 = ((int64_t)(v13));
  t22 = ((int64_t)((uint64_t)(t20) + (uint64_t)(t21)));
  t23 = ((float *)t19.data) + gg_index(t22, t19.len);
  (*t23) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t24 = ((int64_t)((((*(v0))).f3).f2));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t24) == (t25));
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)((((*(v0))).f5).f2));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((t28) == (t29));
  }
  t30 = t27;
  if (t30) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t32 = ((int64_t)UINT64_C(0));
  t33 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t32, ((int64_t)UINT64_C(4)));
  t34 = (*t33);
  t31 = 0;
label3:;
  if (t31 >= t34) goto label5;
  v16 = t31;
  {
  t35 = v2;
  t36 = ((int64_t)(v10));
  t37 = ((int64_t)(v16));
  t38 = ((int64_t)((uint64_t)(t36) + (uint64_t)(t37)));
  t39 = ((uint16_t *)t35.data) + gg_index(t38, t35.len);
  t40 = ((uint16_t)((*t39)));
  t41 = fg_el_half(t40);
  v17 = ((float)(t41));
  t42 = ((float)(v17));
  t43 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t44 = ((t42) > (t43));
  if (t44) {
  {
  v18 = ((float)(v17));
  }
  } else {
  {
  v18 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t45 = v6;
  t46 = ((int64_t)(v12));
  t47 = ((int64_t)(v16));
  t48 = ((int64_t)((uint64_t)(t46) + (uint64_t)(t47)));
  t49 = ((uint16_t *)t45.data) + gg_index(t48, t45.len);
  t50 = ((float)(v18));
  t51 = fg_el_to_half(t50);
  (*t49) = ((uint16_t)(t51));
  }
label4:;
  t31++; goto label3;
label5:;
  }
  } else {
  t52 = ((int64_t)((((*(v0))).f3).f2));
  t53 = ((int64_t)(((int64_t)UINT64_C(2))));
  t54 = ((t52) == (t53));
  t55 = t54;
  if (t55) {
  t56 = ((int64_t)((((*(v0))).f5).f2));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t55 = ((t56) == (t57));
  }
  t58 = t55;
  if (t58) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t60 = ((int64_t)UINT64_C(0));
  t61 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t60, ((int64_t)UINT64_C(4)));
  t62 = (*t61);
  t59 = 0;
label6:;
  if (t59 >= t62) goto label8;
  v19 = t59;
  {
  t63 = v2;
  t64 = ((int64_t)(v10));
  t65 = ((int64_t)(v19));
  t66 = ((int64_t)((uint64_t)(t64) + (uint64_t)(t65)));
  t67 = ((uint16_t *)t63.data) + gg_index(t66, t63.len);
  t68 = ((uint16_t)((*t67)));
  v20 = gg_function_11(t68, base + 0);
  t69 = ((float)(v20));
  t70 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t71 = ((t69) > (t70));
  if (t71) {
  {
  v21 = ((float)(v20));
  }
  } else {
  {
  v21 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t72 = v6;
  t73 = ((int64_t)(v12));
  t74 = ((int64_t)(v19));
  t75 = ((int64_t)((uint64_t)(t73) + (uint64_t)(t74)));
  t76 = ((uint16_t *)t72.data) + gg_index(t75, t72.len);
  t77 = ((float)(v21));
  (*t76) = gg_function_13(t77, base + 0);
  }
label7:;
  t59++; goto label6;
label8:;
  }
  } else {
  t78 = ((int64_t)((((*(v0))).f3).f2));
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((t78) == (t79));
  t81 = t80;
  if (t81) {
  t82 = ((int64_t)((((*(v0))).f5).f2));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = ((t82) == (t83));
  }
  t84 = t81;
  if (t84) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t86 = ((int64_t)UINT64_C(0));
  t87 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t86, ((int64_t)UINT64_C(4)));
  t88 = (*t87);
  t85 = 0;
label9:;
  if (t85 >= t88) goto label11;
  v22 = t85;
  {
  t89 = v2;
  t90 = ((int64_t)(v10));
  t91 = ((int64_t)(v22));
  t92 = ((int64_t)((uint64_t)(t90) + (uint64_t)(t91)));
  t93 = ((uint16_t *)t89.data) + gg_index(t92, t89.len);
  t94 = ((uint16_t)((*t93)));
  v23 = gg_function_11(t94, base + 0);
  t95 = ((float)(v23));
  t96 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t97 = ((t95) > (t96));
  if (t97) {
  {
  v24 = ((float)(v23));
  }
  } else {
  {
  v24 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t98 = v5;
  t99 = ((int64_t)(v12));
  t100 = ((int64_t)(v22));
  t101 = ((int64_t)((uint64_t)(t99) + (uint64_t)(t100)));
  t102 = ((float *)t98.data) + gg_index(t101, t98.len);
  (*t102) = ((float)(v24));
  }
label10:;
  t85++; goto label9;
label11:;
  }
  } else {
  t103 = ((int64_t)((((*(v0))).f3).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(1))));
  t105 = ((t103) == (t104));
  t106 = t105;
  if (t106) {
  t107 = ((int64_t)((((*(v0))).f5).f2));
  t108 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((t107) == (t108));
  }
  t109 = t106;
  if (t109) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t111 = ((int64_t)UINT64_C(0));
  t112 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t111, ((int64_t)UINT64_C(4)));
  t113 = (*t112);
  t110 = 0;
label12:;
  if (t110 >= t113) goto label14;
  v25 = t110;
  {
  t114 = v2;
  t115 = ((int64_t)(v10));
  t116 = ((int64_t)(v25));
  t117 = ((int64_t)((uint64_t)(t115) + (uint64_t)(t116)));
  t118 = ((uint16_t *)t114.data) + gg_index(t117, t114.len);
  t119 = ((uint16_t)((*t118)));
  t120 = fg_el_half(t119);
  v26 = ((float)(t120));
  t121 = ((float)(v26));
  t122 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t123 = ((t121) > (t122));
  if (t123) {
  {
  v27 = ((float)(v26));
  }
  } else {
  {
  v27 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t124 = v5;
  t125 = ((int64_t)(v12));
  t126 = ((int64_t)(v25));
  t127 = ((int64_t)((uint64_t)(t125) + (uint64_t)(t126)));
  t128 = ((float *)t124.data) + gg_index(t127, t124.len);
  (*t128) = ((float)(v27));
  }
label13:;
  t110++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_20(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  uint8_t t18;
  float t19;
  float t20;
  gg_slice t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  float * t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  int64_t * t35;
  int64_t t36;
  gg_slice t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint16_t * t41;
  uint16_t t42;
  float t43;
  float t44;
  float t45;
  uint8_t t46;
  float t47;
  float t48;
  gg_slice t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  uint16_t * t53;
  float t54;
  uint16_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  int64_t * t65;
  int64_t t66;
  gg_slice t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  uint16_t * t71;
  uint16_t t72;
  float t73;
  float t74;
  uint8_t t75;
  float t76;
  float t77;
  gg_slice t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  uint16_t * t82;
  float t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  uint8_t t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
  int64_t t91;
  int64_t t92;
  int64_t * t93;
  int64_t t94;
  gg_slice t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint16_t * t99;
  uint16_t t100;
  float t101;
  float t102;
  uint8_t t103;
  float t104;
  float t105;
  gg_slice t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  float * t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  uint8_t t114;
  int64_t t115;
  int64_t t116;
  uint8_t t117;
  int64_t t118;
  int64_t t119;
  int64_t * t120;
  int64_t t121;
  gg_slice t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint16_t * t126;
  uint16_t t127;
  float t128;
  float t129;
  float t130;
  uint8_t t131;
  float t132;
  float t133;
  gg_slice t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  float * t138;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t18 = ((t16) > (t17));
  if (t18) {
  {
  v15 = ((float)(v14));
  }
  } else {
  {
  t19 = ((float)(v14));
  t20 = expm1f(t19);
  v15 = ((float)(t20));
  }
  }
  t21 = v5;
  t22 = ((int64_t)(v12));
  t23 = ((int64_t)(v13));
  t24 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  t25 = ((float *)t21.data) + gg_index(t24, t21.len);
  (*t25) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t26 = ((int64_t)((((*(v0))).f3).f2));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = ((t26) == (t27));
  t29 = t28;
  if (t29) {
  t30 = ((int64_t)((((*(v0))).f5).f2));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = ((t30) == (t31));
  }
  t32 = t29;
  if (t32) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t34 = ((int64_t)UINT64_C(0));
  t35 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t34, ((int64_t)UINT64_C(4)));
  t36 = (*t35);
  t33 = 0;
label3:;
  if (t33 >= t36) goto label5;
  v16 = t33;
  {
  t37 = v2;
  t38 = ((int64_t)(v10));
  t39 = ((int64_t)(v16));
  t40 = ((int64_t)((uint64_t)(t38) + (uint64_t)(t39)));
  t41 = ((uint16_t *)t37.data) + gg_index(t40, t37.len);
  t42 = ((uint16_t)((*t41)));
  t43 = fg_el_half(t42);
  v17 = ((float)(t43));
  t44 = ((float)(v17));
  t45 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t46 = ((t44) > (t45));
  if (t46) {
  {
  v18 = ((float)(v17));
  }
  } else {
  {
  t47 = ((float)(v17));
  t48 = expm1f(t47);
  v18 = ((float)(t48));
  }
  }
  t49 = v6;
  t50 = ((int64_t)(v12));
  t51 = ((int64_t)(v16));
  t52 = ((int64_t)((uint64_t)(t50) + (uint64_t)(t51)));
  t53 = ((uint16_t *)t49.data) + gg_index(t52, t49.len);
  t54 = ((float)(v18));
  t55 = fg_el_to_half(t54);
  (*t53) = ((uint16_t)(t55));
  }
label4:;
  t33++; goto label3;
label5:;
  }
  } else {
  t56 = ((int64_t)((((*(v0))).f3).f2));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t58 = ((t56) == (t57));
  t59 = t58;
  if (t59) {
  t60 = ((int64_t)((((*(v0))).f5).f2));
  t61 = ((int64_t)(((int64_t)UINT64_C(2))));
  t59 = ((t60) == (t61));
  }
  t62 = t59;
  if (t62) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t64 = ((int64_t)UINT64_C(0));
  t65 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t64, ((int64_t)UINT64_C(4)));
  t66 = (*t65);
  t63 = 0;
label6:;
  if (t63 >= t66) goto label8;
  v19 = t63;
  {
  t67 = v2;
  t68 = ((int64_t)(v10));
  t69 = ((int64_t)(v19));
  t70 = ((int64_t)((uint64_t)(t68) + (uint64_t)(t69)));
  t71 = ((uint16_t *)t67.data) + gg_index(t70, t67.len);
  t72 = ((uint16_t)((*t71)));
  v20 = gg_function_11(t72, base + 0);
  t73 = ((float)(v20));
  t74 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t75 = ((t73) > (t74));
  if (t75) {
  {
  v21 = ((float)(v20));
  }
  } else {
  {
  t76 = ((float)(v20));
  t77 = expm1f(t76);
  v21 = ((float)(t77));
  }
  }
  t78 = v6;
  t79 = ((int64_t)(v12));
  t80 = ((int64_t)(v19));
  t81 = ((int64_t)((uint64_t)(t79) + (uint64_t)(t80)));
  t82 = ((uint16_t *)t78.data) + gg_index(t81, t78.len);
  t83 = ((float)(v21));
  (*t82) = gg_function_13(t83, base + 0);
  }
label7:;
  t63++; goto label6;
label8:;
  }
  } else {
  t84 = ((int64_t)((((*(v0))).f3).f2));
  t85 = ((int64_t)(((int64_t)UINT64_C(2))));
  t86 = ((t84) == (t85));
  t87 = t86;
  if (t87) {
  t88 = ((int64_t)((((*(v0))).f5).f2));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = ((t88) == (t89));
  }
  t90 = t87;
  if (t90) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t92 = ((int64_t)UINT64_C(0));
  t93 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t92, ((int64_t)UINT64_C(4)));
  t94 = (*t93);
  t91 = 0;
label9:;
  if (t91 >= t94) goto label11;
  v22 = t91;
  {
  t95 = v2;
  t96 = ((int64_t)(v10));
  t97 = ((int64_t)(v22));
  t98 = ((int64_t)((uint64_t)(t96) + (uint64_t)(t97)));
  t99 = ((uint16_t *)t95.data) + gg_index(t98, t95.len);
  t100 = ((uint16_t)((*t99)));
  v23 = gg_function_11(t100, base + 0);
  t101 = ((float)(v23));
  t102 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t103 = ((t101) > (t102));
  if (t103) {
  {
  v24 = ((float)(v23));
  }
  } else {
  {
  t104 = ((float)(v23));
  t105 = expm1f(t104);
  v24 = ((float)(t105));
  }
  }
  t106 = v5;
  t107 = ((int64_t)(v12));
  t108 = ((int64_t)(v22));
  t109 = ((int64_t)((uint64_t)(t107) + (uint64_t)(t108)));
  t110 = ((float *)t106.data) + gg_index(t109, t106.len);
  (*t110) = ((float)(v24));
  }
label10:;
  t91++; goto label9;
label11:;
  }
  } else {
  t111 = ((int64_t)((((*(v0))).f3).f2));
  t112 = ((int64_t)(((int64_t)UINT64_C(1))));
  t113 = ((t111) == (t112));
  t114 = t113;
  if (t114) {
  t115 = ((int64_t)((((*(v0))).f5).f2));
  t116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((t115) == (t116));
  }
  t117 = t114;
  if (t117) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t119 = ((int64_t)UINT64_C(0));
  t120 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t119, ((int64_t)UINT64_C(4)));
  t121 = (*t120);
  t118 = 0;
label12:;
  if (t118 >= t121) goto label14;
  v25 = t118;
  {
  t122 = v2;
  t123 = ((int64_t)(v10));
  t124 = ((int64_t)(v25));
  t125 = ((int64_t)((uint64_t)(t123) + (uint64_t)(t124)));
  t126 = ((uint16_t *)t122.data) + gg_index(t125, t122.len);
  t127 = ((uint16_t)((*t126)));
  t128 = fg_el_half(t127);
  v26 = ((float)(t128));
  t129 = ((float)(v26));
  t130 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t131 = ((t129) > (t130));
  if (t131) {
  {
  v27 = ((float)(v26));
  }
  } else {
  {
  t132 = ((float)(v26));
  t133 = expm1f(t132);
  v27 = ((float)(t133));
  }
  }
  t134 = v5;
  t135 = ((int64_t)(v12));
  t136 = ((int64_t)(v25));
  t137 = ((int64_t)((uint64_t)(t135) + (uint64_t)(t136)));
  t138 = ((float *)t134.data) + gg_index(t137, t134.len);
  (*t138) = ((float)(v27));
  }
label13:;
  t118++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_18(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = tanhf(t16);
  v15 = ((float)(t17));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = tanhf(t41);
  v18 = ((float)(t42));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = tanhf(t67);
  v21 = ((float)(t68));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = tanhf(t92);
  v24 = ((float)(t93));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = tanhf(t117);
  v27 = ((float)(t118));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_17(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  uint8_t t18;
  gg_slice t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  float * t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  int64_t * t33;
  int64_t t34;
  gg_slice t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint16_t * t39;
  uint16_t t40;
  float t41;
  float t42;
  float t43;
  uint8_t t44;
  gg_slice t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint16_t * t49;
  float t50;
  uint16_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  int64_t t59;
  int64_t t60;
  int64_t * t61;
  int64_t t62;
  gg_slice t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  uint16_t * t67;
  uint16_t t68;
  float t69;
  float t70;
  uint8_t t71;
  gg_slice t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint16_t * t76;
  float t77;
  int64_t t78;
  int64_t t79;
  uint8_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  uint8_t t84;
  int64_t t85;
  int64_t t86;
  int64_t * t87;
  int64_t t88;
  gg_slice t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint16_t * t93;
  uint16_t t94;
  float t95;
  float t96;
  uint8_t t97;
  gg_slice t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  float * t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  int64_t * t112;
  int64_t t113;
  gg_slice t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  uint16_t * t118;
  uint16_t t119;
  float t120;
  float t121;
  float t122;
  uint8_t t123;
  gg_slice t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  float * t128;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t18 = ((t16) > (t17));
  if (t18) {
  {
  v15 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  {
  v15 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t19 = v5;
  t20 = ((int64_t)(v12));
  t21 = ((int64_t)(v13));
  t22 = ((int64_t)((uint64_t)(t20) + (uint64_t)(t21)));
  t23 = ((float *)t19.data) + gg_index(t22, t19.len);
  (*t23) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t24 = ((int64_t)((((*(v0))).f3).f2));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t24) == (t25));
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)((((*(v0))).f5).f2));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((t28) == (t29));
  }
  t30 = t27;
  if (t30) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t32 = ((int64_t)UINT64_C(0));
  t33 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t32, ((int64_t)UINT64_C(4)));
  t34 = (*t33);
  t31 = 0;
label3:;
  if (t31 >= t34) goto label5;
  v16 = t31;
  {
  t35 = v2;
  t36 = ((int64_t)(v10));
  t37 = ((int64_t)(v16));
  t38 = ((int64_t)((uint64_t)(t36) + (uint64_t)(t37)));
  t39 = ((uint16_t *)t35.data) + gg_index(t38, t35.len);
  t40 = ((uint16_t)((*t39)));
  t41 = fg_el_half(t40);
  v17 = ((float)(t41));
  t42 = ((float)(v17));
  t43 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t44 = ((t42) > (t43));
  if (t44) {
  {
  v18 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  {
  v18 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t45 = v6;
  t46 = ((int64_t)(v12));
  t47 = ((int64_t)(v16));
  t48 = ((int64_t)((uint64_t)(t46) + (uint64_t)(t47)));
  t49 = ((uint16_t *)t45.data) + gg_index(t48, t45.len);
  t50 = ((float)(v18));
  t51 = fg_el_to_half(t50);
  (*t49) = ((uint16_t)(t51));
  }
label4:;
  t31++; goto label3;
label5:;
  }
  } else {
  t52 = ((int64_t)((((*(v0))).f3).f2));
  t53 = ((int64_t)(((int64_t)UINT64_C(2))));
  t54 = ((t52) == (t53));
  t55 = t54;
  if (t55) {
  t56 = ((int64_t)((((*(v0))).f5).f2));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t55 = ((t56) == (t57));
  }
  t58 = t55;
  if (t58) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t60 = ((int64_t)UINT64_C(0));
  t61 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t60, ((int64_t)UINT64_C(4)));
  t62 = (*t61);
  t59 = 0;
label6:;
  if (t59 >= t62) goto label8;
  v19 = t59;
  {
  t63 = v2;
  t64 = ((int64_t)(v10));
  t65 = ((int64_t)(v19));
  t66 = ((int64_t)((uint64_t)(t64) + (uint64_t)(t65)));
  t67 = ((uint16_t *)t63.data) + gg_index(t66, t63.len);
  t68 = ((uint16_t)((*t67)));
  v20 = gg_function_11(t68, base + 0);
  t69 = ((float)(v20));
  t70 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t71 = ((t69) > (t70));
  if (t71) {
  {
  v21 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  {
  v21 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t72 = v6;
  t73 = ((int64_t)(v12));
  t74 = ((int64_t)(v19));
  t75 = ((int64_t)((uint64_t)(t73) + (uint64_t)(t74)));
  t76 = ((uint16_t *)t72.data) + gg_index(t75, t72.len);
  t77 = ((float)(v21));
  (*t76) = gg_function_13(t77, base + 0);
  }
label7:;
  t59++; goto label6;
label8:;
  }
  } else {
  t78 = ((int64_t)((((*(v0))).f3).f2));
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((t78) == (t79));
  t81 = t80;
  if (t81) {
  t82 = ((int64_t)((((*(v0))).f5).f2));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = ((t82) == (t83));
  }
  t84 = t81;
  if (t84) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t86 = ((int64_t)UINT64_C(0));
  t87 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t86, ((int64_t)UINT64_C(4)));
  t88 = (*t87);
  t85 = 0;
label9:;
  if (t85 >= t88) goto label11;
  v22 = t85;
  {
  t89 = v2;
  t90 = ((int64_t)(v10));
  t91 = ((int64_t)(v22));
  t92 = ((int64_t)((uint64_t)(t90) + (uint64_t)(t91)));
  t93 = ((uint16_t *)t89.data) + gg_index(t92, t89.len);
  t94 = ((uint16_t)((*t93)));
  v23 = gg_function_11(t94, base + 0);
  t95 = ((float)(v23));
  t96 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t97 = ((t95) > (t96));
  if (t97) {
  {
  v24 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  {
  v24 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t98 = v5;
  t99 = ((int64_t)(v12));
  t100 = ((int64_t)(v22));
  t101 = ((int64_t)((uint64_t)(t99) + (uint64_t)(t100)));
  t102 = ((float *)t98.data) + gg_index(t101, t98.len);
  (*t102) = ((float)(v24));
  }
label10:;
  t85++; goto label9;
label11:;
  }
  } else {
  t103 = ((int64_t)((((*(v0))).f3).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(1))));
  t105 = ((t103) == (t104));
  t106 = t105;
  if (t106) {
  t107 = ((int64_t)((((*(v0))).f5).f2));
  t108 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((t107) == (t108));
  }
  t109 = t106;
  if (t109) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t111 = ((int64_t)UINT64_C(0));
  t112 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t111, ((int64_t)UINT64_C(4)));
  t113 = (*t112);
  t110 = 0;
label12:;
  if (t110 >= t113) goto label14;
  v25 = t110;
  {
  t114 = v2;
  t115 = ((int64_t)(v10));
  t116 = ((int64_t)(v25));
  t117 = ((int64_t)((uint64_t)(t115) + (uint64_t)(t116)));
  t118 = ((uint16_t *)t114.data) + gg_index(t117, t114.len);
  t119 = ((uint16_t)((*t118)));
  t120 = fg_el_half(t119);
  v26 = ((float)(t120));
  t121 = ((float)(v26));
  t122 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t123 = ((t121) > (t122));
  if (t123) {
  {
  v27 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  {
  v27 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t124 = v5;
  t125 = ((int64_t)(v12));
  t126 = ((int64_t)(v25));
  t127 = ((int64_t)((uint64_t)(t125) + (uint64_t)(t126)));
  t128 = ((float *)t124.data) + gg_index(t127, t124.len);
  (*t128) = ((float)(v27));
  }
label13:;
  t110++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_16(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  gg_slice t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  float * t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t * t31;
  int64_t t32;
  gg_slice t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint16_t * t37;
  uint16_t t38;
  float t39;
  float t40;
  gg_slice t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint16_t * t45;
  float t46;
  uint16_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  int64_t * t57;
  int64_t t58;
  gg_slice t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint16_t * t63;
  uint16_t t64;
  float t65;
  gg_slice t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint16_t * t70;
  float t71;
  int64_t t72;
  int64_t t73;
  uint8_t t74;
  uint8_t t75;
  int64_t t76;
  int64_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  int64_t * t81;
  int64_t t82;
  gg_slice t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  uint16_t * t87;
  uint16_t t88;
  float t89;
  gg_slice t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  float * t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  int64_t t102;
  int64_t t103;
  int64_t * t104;
  int64_t t105;
  gg_slice t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  uint16_t * t110;
  uint16_t t111;
  float t112;
  float t113;
  gg_slice t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  float * t118;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = v14;
  v15 = ((float)((-(t16))));
  t17 = v5;
  t18 = ((int64_t)(v12));
  t19 = ((int64_t)(v13));
  t20 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  t21 = ((float *)t17.data) + gg_index(t20, t17.len);
  (*t21) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t22 = ((int64_t)((((*(v0))).f3).f2));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((t22) == (t23));
  t25 = t24;
  if (t25) {
  t26 = ((int64_t)((((*(v0))).f5).f2));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t26) == (t27));
  }
  t28 = t25;
  if (t28) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t30 = ((int64_t)UINT64_C(0));
  t31 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t30, ((int64_t)UINT64_C(4)));
  t32 = (*t31);
  t29 = 0;
label3:;
  if (t29 >= t32) goto label5;
  v16 = t29;
  {
  t33 = v2;
  t34 = ((int64_t)(v10));
  t35 = ((int64_t)(v16));
  t36 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  t37 = ((uint16_t *)t33.data) + gg_index(t36, t33.len);
  t38 = ((uint16_t)((*t37)));
  t39 = fg_el_half(t38);
  v17 = ((float)(t39));
  t40 = v17;
  v18 = ((float)((-(t40))));
  t41 = v6;
  t42 = ((int64_t)(v12));
  t43 = ((int64_t)(v16));
  t44 = ((int64_t)((uint64_t)(t42) + (uint64_t)(t43)));
  t45 = ((uint16_t *)t41.data) + gg_index(t44, t41.len);
  t46 = ((float)(v18));
  t47 = fg_el_to_half(t46);
  (*t45) = ((uint16_t)(t47));
  }
label4:;
  t29++; goto label3;
label5:;
  }
  } else {
  t48 = ((int64_t)((((*(v0))).f3).f2));
  t49 = ((int64_t)(((int64_t)UINT64_C(2))));
  t50 = ((t48) == (t49));
  t51 = t50;
  if (t51) {
  t52 = ((int64_t)((((*(v0))).f5).f2));
  t53 = ((int64_t)(((int64_t)UINT64_C(2))));
  t51 = ((t52) == (t53));
  }
  t54 = t51;
  if (t54) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t56 = ((int64_t)UINT64_C(0));
  t57 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t56, ((int64_t)UINT64_C(4)));
  t58 = (*t57);
  t55 = 0;
label6:;
  if (t55 >= t58) goto label8;
  v19 = t55;
  {
  t59 = v2;
  t60 = ((int64_t)(v10));
  t61 = ((int64_t)(v19));
  t62 = ((int64_t)((uint64_t)(t60) + (uint64_t)(t61)));
  t63 = ((uint16_t *)t59.data) + gg_index(t62, t59.len);
  t64 = ((uint16_t)((*t63)));
  v20 = gg_function_11(t64, base + 0);
  t65 = v20;
  v21 = ((float)((-(t65))));
  t66 = v6;
  t67 = ((int64_t)(v12));
  t68 = ((int64_t)(v19));
  t69 = ((int64_t)((uint64_t)(t67) + (uint64_t)(t68)));
  t70 = ((uint16_t *)t66.data) + gg_index(t69, t66.len);
  t71 = ((float)(v21));
  (*t70) = gg_function_13(t71, base + 0);
  }
label7:;
  t55++; goto label6;
label8:;
  }
  } else {
  t72 = ((int64_t)((((*(v0))).f3).f2));
  t73 = ((int64_t)(((int64_t)UINT64_C(2))));
  t74 = ((t72) == (t73));
  t75 = t74;
  if (t75) {
  t76 = ((int64_t)((((*(v0))).f5).f2));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((t76) == (t77));
  }
  t78 = t75;
  if (t78) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t80 = ((int64_t)UINT64_C(0));
  t81 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t80, ((int64_t)UINT64_C(4)));
  t82 = (*t81);
  t79 = 0;
label9:;
  if (t79 >= t82) goto label11;
  v22 = t79;
  {
  t83 = v2;
  t84 = ((int64_t)(v10));
  t85 = ((int64_t)(v22));
  t86 = ((int64_t)((uint64_t)(t84) + (uint64_t)(t85)));
  t87 = ((uint16_t *)t83.data) + gg_index(t86, t83.len);
  t88 = ((uint16_t)((*t87)));
  v23 = gg_function_11(t88, base + 0);
  t89 = v23;
  v24 = ((float)((-(t89))));
  t90 = v5;
  t91 = ((int64_t)(v12));
  t92 = ((int64_t)(v22));
  t93 = ((int64_t)((uint64_t)(t91) + (uint64_t)(t92)));
  t94 = ((float *)t90.data) + gg_index(t93, t90.len);
  (*t94) = ((float)(v24));
  }
label10:;
  t79++; goto label9;
label11:;
  }
  } else {
  t95 = ((int64_t)((((*(v0))).f3).f2));
  t96 = ((int64_t)(((int64_t)UINT64_C(1))));
  t97 = ((t95) == (t96));
  t98 = t97;
  if (t98) {
  t99 = ((int64_t)((((*(v0))).f5).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((t99) == (t100));
  }
  t101 = t98;
  if (t101) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t103 = ((int64_t)UINT64_C(0));
  t104 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t103, ((int64_t)UINT64_C(4)));
  t105 = (*t104);
  t102 = 0;
label12:;
  if (t102 >= t105) goto label14;
  v25 = t102;
  {
  t106 = v2;
  t107 = ((int64_t)(v10));
  t108 = ((int64_t)(v25));
  t109 = ((int64_t)((uint64_t)(t107) + (uint64_t)(t108)));
  t110 = ((uint16_t *)t106.data) + gg_index(t109, t106.len);
  t111 = ((uint16_t)((*t110)));
  t112 = fg_el_half(t111);
  v26 = ((float)(t112));
  t113 = v26;
  v27 = ((float)((-(t113))));
  t114 = v5;
  t115 = ((int64_t)(v12));
  t116 = ((int64_t)(v25));
  t117 = ((int64_t)((uint64_t)(t115) + (uint64_t)(t116)));
  t118 = ((float *)t114.data) + gg_index(t117, t114.len);
  (*t118) = ((float)(v27));
  }
label13:;
  t102++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_15(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  uint8_t t18;
  float t19;
  float t20;
  uint8_t t21;
  float t22;
  gg_slice t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  float * t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t * t37;
  int64_t t38;
  gg_slice t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint16_t * t43;
  uint16_t t44;
  float t45;
  float t46;
  float t47;
  uint8_t t48;
  float t49;
  float t50;
  uint8_t t51;
  float t52;
  gg_slice t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint16_t * t57;
  float t58;
  uint16_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  uint8_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  int64_t * t69;
  int64_t t70;
  gg_slice t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint16_t * t75;
  uint16_t t76;
  float t77;
  float t78;
  uint8_t t79;
  float t80;
  float t81;
  uint8_t t82;
  float t83;
  gg_slice t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  uint16_t * t88;
  float t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
  uint8_t t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  int64_t * t99;
  int64_t t100;
  gg_slice t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint16_t * t105;
  uint16_t t106;
  float t107;
  float t108;
  uint8_t t109;
  float t110;
  float t111;
  uint8_t t112;
  float t113;
  gg_slice t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  float * t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
  uint8_t t122;
  int64_t t123;
  int64_t t124;
  uint8_t t125;
  int64_t t126;
  int64_t t127;
  int64_t * t128;
  int64_t t129;
  gg_slice t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint16_t * t134;
  uint16_t t135;
  float t136;
  float t137;
  float t138;
  uint8_t t139;
  float t140;
  float t141;
  uint8_t t142;
  float t143;
  gg_slice t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  float * t148;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t18 = ((t16) > (t17));
  if (t18) {
  {
  v15 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  t19 = ((float)(v14));
  t20 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t21 = ((t19) < (t20));
  if (t21) {
  {
  t22 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  v15 = ((float)((-(t22))));
  }
  } else {
  {
  v15 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  }
  t23 = v5;
  t24 = ((int64_t)(v12));
  t25 = ((int64_t)(v13));
  t26 = ((int64_t)((uint64_t)(t24) + (uint64_t)(t25)));
  t27 = ((float *)t23.data) + gg_index(t26, t23.len);
  (*t27) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t28 = ((int64_t)((((*(v0))).f3).f2));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((t28) == (t29));
  t31 = t30;
  if (t31) {
  t32 = ((int64_t)((((*(v0))).f5).f2));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((t32) == (t33));
  }
  t34 = t31;
  if (t34) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t36 = ((int64_t)UINT64_C(0));
  t37 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t36, ((int64_t)UINT64_C(4)));
  t38 = (*t37);
  t35 = 0;
label3:;
  if (t35 >= t38) goto label5;
  v16 = t35;
  {
  t39 = v2;
  t40 = ((int64_t)(v10));
  t41 = ((int64_t)(v16));
  t42 = ((int64_t)((uint64_t)(t40) + (uint64_t)(t41)));
  t43 = ((uint16_t *)t39.data) + gg_index(t42, t39.len);
  t44 = ((uint16_t)((*t43)));
  t45 = fg_el_half(t44);
  v17 = ((float)(t45));
  t46 = ((float)(v17));
  t47 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t48 = ((t46) > (t47));
  if (t48) {
  {
  v18 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  t49 = ((float)(v17));
  t50 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t51 = ((t49) < (t50));
  if (t51) {
  {
  t52 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  v18 = ((float)((-(t52))));
  }
  } else {
  {
  v18 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  }
  t53 = v6;
  t54 = ((int64_t)(v12));
  t55 = ((int64_t)(v16));
  t56 = ((int64_t)((uint64_t)(t54) + (uint64_t)(t55)));
  t57 = ((uint16_t *)t53.data) + gg_index(t56, t53.len);
  t58 = ((float)(v18));
  t59 = fg_el_to_half(t58);
  (*t57) = ((uint16_t)(t59));
  }
label4:;
  t35++; goto label3;
label5:;
  }
  } else {
  t60 = ((int64_t)((((*(v0))).f3).f2));
  t61 = ((int64_t)(((int64_t)UINT64_C(2))));
  t62 = ((t60) == (t61));
  t63 = t62;
  if (t63) {
  t64 = ((int64_t)((((*(v0))).f5).f2));
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t63 = ((t64) == (t65));
  }
  t66 = t63;
  if (t66) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t68 = ((int64_t)UINT64_C(0));
  t69 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t68, ((int64_t)UINT64_C(4)));
  t70 = (*t69);
  t67 = 0;
label6:;
  if (t67 >= t70) goto label8;
  v19 = t67;
  {
  t71 = v2;
  t72 = ((int64_t)(v10));
  t73 = ((int64_t)(v19));
  t74 = ((int64_t)((uint64_t)(t72) + (uint64_t)(t73)));
  t75 = ((uint16_t *)t71.data) + gg_index(t74, t71.len);
  t76 = ((uint16_t)((*t75)));
  v20 = gg_function_11(t76, base + 0);
  t77 = ((float)(v20));
  t78 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t79 = ((t77) > (t78));
  if (t79) {
  {
  v21 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  t80 = ((float)(v20));
  t81 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t82 = ((t80) < (t81));
  if (t82) {
  {
  t83 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  v21 = ((float)((-(t83))));
  }
  } else {
  {
  v21 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  }
  t84 = v6;
  t85 = ((int64_t)(v12));
  t86 = ((int64_t)(v19));
  t87 = ((int64_t)((uint64_t)(t85) + (uint64_t)(t86)));
  t88 = ((uint16_t *)t84.data) + gg_index(t87, t84.len);
  t89 = ((float)(v21));
  (*t88) = gg_function_13(t89, base + 0);
  }
label7:;
  t67++; goto label6;
label8:;
  }
  } else {
  t90 = ((int64_t)((((*(v0))).f3).f2));
  t91 = ((int64_t)(((int64_t)UINT64_C(2))));
  t92 = ((t90) == (t91));
  t93 = t92;
  if (t93) {
  t94 = ((int64_t)((((*(v0))).f5).f2));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t94) == (t95));
  }
  t96 = t93;
  if (t96) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t98 = ((int64_t)UINT64_C(0));
  t99 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t98, ((int64_t)UINT64_C(4)));
  t100 = (*t99);
  t97 = 0;
label9:;
  if (t97 >= t100) goto label11;
  v22 = t97;
  {
  t101 = v2;
  t102 = ((int64_t)(v10));
  t103 = ((int64_t)(v22));
  t104 = ((int64_t)((uint64_t)(t102) + (uint64_t)(t103)));
  t105 = ((uint16_t *)t101.data) + gg_index(t104, t101.len);
  t106 = ((uint16_t)((*t105)));
  v23 = gg_function_11(t106, base + 0);
  t107 = ((float)(v23));
  t108 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t109 = ((t107) > (t108));
  if (t109) {
  {
  v24 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  t110 = ((float)(v23));
  t111 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t112 = ((t110) < (t111));
  if (t112) {
  {
  t113 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  v24 = ((float)((-(t113))));
  }
  } else {
  {
  v24 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  }
  t114 = v5;
  t115 = ((int64_t)(v12));
  t116 = ((int64_t)(v22));
  t117 = ((int64_t)((uint64_t)(t115) + (uint64_t)(t116)));
  t118 = ((float *)t114.data) + gg_index(t117, t114.len);
  (*t118) = ((float)(v24));
  }
label10:;
  t97++; goto label9;
label11:;
  }
  } else {
  t119 = ((int64_t)((((*(v0))).f3).f2));
  t120 = ((int64_t)(((int64_t)UINT64_C(1))));
  t121 = ((t119) == (t120));
  t122 = t121;
  if (t122) {
  t123 = ((int64_t)((((*(v0))).f5).f2));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = ((t123) == (t124));
  }
  t125 = t122;
  if (t125) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t127 = ((int64_t)UINT64_C(0));
  t128 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t127, ((int64_t)UINT64_C(4)));
  t129 = (*t128);
  t126 = 0;
label12:;
  if (t126 >= t129) goto label14;
  v25 = t126;
  {
  t130 = v2;
  t131 = ((int64_t)(v10));
  t132 = ((int64_t)(v25));
  t133 = ((int64_t)((uint64_t)(t131) + (uint64_t)(t132)));
  t134 = ((uint16_t *)t130.data) + gg_index(t133, t130.len);
  t135 = ((uint16_t)((*t134)));
  t136 = fg_el_half(t135);
  v26 = ((float)(t136));
  t137 = ((float)(v26));
  t138 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t139 = ((t137) > (t138));
  if (t139) {
  {
  v27 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  t140 = ((float)(v26));
  t141 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t142 = ((t140) < (t141));
  if (t142) {
  {
  t143 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  v27 = ((float)((-(t143))));
  }
  } else {
  {
  v27 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  }
  t144 = v5;
  t145 = ((int64_t)(v12));
  t146 = ((int64_t)(v25));
  t147 = ((int64_t)((uint64_t)(t145) + (uint64_t)(t146)));
  t148 = ((float *)t144.data) + gg_index(t147, t144.len);
  (*t148) = ((float)(v27));
  }
label13:;
  t126++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_7(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  float * t15;
  float t16;
  float t17;
  gg_slice t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  float * t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t * t32;
  int64_t t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t * t38;
  uint16_t t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint16_t * t47;
  float t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint16_t * t65;
  uint16_t t66;
  float t67;
  float t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint16_t * t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t * t84;
  int64_t t85;
  gg_slice t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint16_t * t90;
  uint16_t t91;
  float t92;
  float t93;
  gg_slice t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  float * t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t * t108;
  int64_t t109;
  gg_slice t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint16_t * t114;
  uint16_t t115;
  float t116;
  float t117;
  float t118;
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float * t123;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  float v15;
  int64_t v16;
  float v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  int64_t v22;
  float v23;
  float v24;
  int64_t v25;
  float v26;
  float v27;
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
  t0 = ((int64_t)((((*(v0))).f3).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((((*(v0))).f5).f2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) == (t5));
  }
  t6 = t3;
  if (t6) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = (*t9);
  t7 = 0;
label0:;
  if (t7 >= t10) goto label2;
  v13 = t7;
  {
  t11 = v1;
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((float *)t11.data) + gg_index(t14, t11.len);
  v14 = ((float)((*t15)));
  t16 = ((float)(v14));
  t17 = fabsf(t16);
  v15 = ((float)(t17));
  t18 = v5;
  t19 = ((int64_t)(v12));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t22 = ((float *)t18.data) + gg_index(t21, t18.len);
  (*t22) = ((float)(v15));
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  t23 = ((int64_t)((((*(v0))).f3).f2));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)((((*(v0))).f5).f2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t31 = ((int64_t)UINT64_C(0));
  t32 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t31, ((int64_t)UINT64_C(4)));
  t33 = (*t32);
  t30 = 0;
label3:;
  if (t30 >= t33) goto label5;
  v16 = t30;
  {
  t34 = v2;
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(v16));
  t37 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  t38 = ((uint16_t *)t34.data) + gg_index(t37, t34.len);
  t39 = ((uint16_t)((*t38)));
  t40 = fg_el_half(t39);
  v17 = ((float)(t40));
  t41 = ((float)(v17));
  t42 = fabsf(t41);
  v18 = ((float)(t42));
  t43 = v6;
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  t46 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  t47 = ((uint16_t *)t43.data) + gg_index(t46, t43.len);
  t48 = ((float)(v18));
  t49 = fg_el_to_half(t48);
  (*t47) = ((uint16_t)(t49));
  }
label4:;
  t30++; goto label3;
label5:;
  }
  } else {
  t50 = ((int64_t)((((*(v0))).f3).f2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (t53) {
  t54 = ((int64_t)((((*(v0))).f5).f2));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t54) == (t55));
  }
  t56 = t53;
  if (t56) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(0));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = (*t59);
  t57 = 0;
label6:;
  if (t57 >= t60) goto label8;
  v19 = t57;
  {
  t61 = v2;
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v19));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint16_t *)t61.data) + gg_index(t64, t61.len);
  t66 = ((uint16_t)((*t65)));
  v20 = gg_function_11(t66, base + 0);
  t67 = ((float)(v20));
  t68 = fabsf(t67);
  v21 = ((float)(t68));
  t69 = v6;
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  t73 = ((uint16_t *)t69.data) + gg_index(t72, t69.len);
  t74 = ((float)(v21));
  (*t73) = gg_function_13(t74, base + 0);
  }
label7:;
  t57++; goto label6;
label8:;
  }
  } else {
  t75 = ((int64_t)((((*(v0))).f3).f2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)((((*(v0))).f5).f2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t79) == (t80));
  }
  t81 = t78;
  if (t81) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t83 = ((int64_t)UINT64_C(0));
  t84 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t83, ((int64_t)UINT64_C(4)));
  t85 = (*t84);
  t82 = 0;
label9:;
  if (t82 >= t85) goto label11;
  v22 = t82;
  {
  t86 = v2;
  t87 = ((int64_t)(v10));
  t88 = ((int64_t)(v22));
  t89 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  t90 = ((uint16_t *)t86.data) + gg_index(t89, t86.len);
  t91 = ((uint16_t)((*t90)));
  v23 = gg_function_11(t91, base + 0);
  t92 = ((float)(v23));
  t93 = fabsf(t92);
  v24 = ((float)(t93));
  t94 = v5;
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v22));
  t97 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t98 = ((float *)t94.data) + gg_index(t97, t94.len);
  (*t98) = ((float)(v24));
  }
label10:;
  t82++; goto label9;
label11:;
  }
  } else {
  t99 = ((int64_t)((((*(v0))).f3).f2));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)((((*(v0))).f5).f2));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  if (t105) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t107 = ((int64_t)UINT64_C(0));
  t108 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = (*t108);
  t106 = 0;
label12:;
  if (t106 >= t109) goto label14;
  v25 = t106;
  {
  t110 = v2;
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(v25));
  t113 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  t114 = ((uint16_t *)t110.data) + gg_index(t113, t110.len);
  t115 = ((uint16_t)((*t114)));
  t116 = fg_el_half(t115);
  v26 = ((float)(t116));
  t117 = ((float)(v26));
  t118 = fabsf(t117);
  v27 = ((float)(t118));
  t119 = v5;
  t120 = ((int64_t)(v12));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)((uint64_t)(t120) + (uint64_t)(t121)));
  t123 = ((float *)t119.data) + gg_index(t122, t119.len);
  (*t123) = ((float)(v27));
  }
label13:;
  t106++; goto label12;
label14:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_6(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  int64_t t0;
  int64_t * t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  float * t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  gg_slice t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  gg_type_10 * t19;
  float t20;
  float t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  float * t26;
  int64_t t27;
  float * t28;
  float t29;
  float t30;
  gg_slice t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  gg_type_10 * t37;
  int64_t t38;
  int64_t * t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t * t44;
  int64_t t45;
  int64_t t46;
  int64_t * t47;
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
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  gg_type_10 * t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  gg_type_10 * t75;
  float t76;
  float t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  float * t82;
  int64_t t83;
  float * t84;
  float t85;
  int64_t t86;
  float * t87;
  float t88;
  gg_slice t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  gg_type_10 * t93;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  int64_t v10;
  gg_type_10 v11;
  gg_type_10 v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  gg_type_10 v19;
  gg_type_10 v20;
  gg_type_10 v21;
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
  {
  /* FG_DIRECT_SLICE_INDEX */
  t0 = ((int64_t)UINT64_C(0));
  t1 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t0, ((int64_t)UINT64_C(4)));
  t2 = ((int64_t)((*t1)));
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = ((t2) == (t3));
  if (t4) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t5 = v7;
  t6 = ((float *)(v3).data) + gg_index(t5, (v3).len);
  v9 = ((float)((*t6)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = ((int64_t)((*t9)));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  t12 = ((int64_t)gg_div(t10, t11));
  t7 = 0;
label0:;
  if (t7 >= t12) goto label2;
  v10 = t7;
  {
  t13 = v2;
  t14 = ((int64_t)(v6));
  t15 = ((int64_t)(((int64_t)UINT64_C(8))));
  t16 = ((int64_t)(((int64_t)gg_div(t14, t15))));
  t17 = ((int64_t)(v10));
  t18 = ((int64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  t19 = ((gg_type_10 *)t13.data) + gg_index(t18, t13.len);
  v11 = (*t19);
  memset(&t20, 0, sizeof(t20));
  t21 = ((float)(t20));
  for (t22=0; t22<8; t22++) (v12).data[t22] = t21;
  t24 = ((int64_t)UINT64_C(8));
  t23 = 0;
label3:;
  if (t23 >= t24) goto label5;
  v13 = t23;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v13;
  t26 = ((float *)(v12).data) + gg_index(t25, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = v13;
  t28 = ((float *)(v11).data) + gg_index(t27, ((int64_t)UINT64_C(8)));
  t29 = ((float)((*t28)));
  t30 = ((float)(v9));
  (*t26) = ((float)(((t29) / (t30))));
  }
label4:;
  t23++; goto label3;
label5:;
  t31 = v5;
  t32 = ((int64_t)(v8));
  t33 = ((int64_t)(((int64_t)UINT64_C(8))));
  t34 = ((int64_t)(((int64_t)gg_div(t32, t33))));
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  t37 = ((gg_type_10 *)t31.data) + gg_index(t36, t31.len);
  (*t37) = v12;
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t38 = ((int64_t)UINT64_C(0));
  t39 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t38, ((int64_t)UINT64_C(4)));
  t40 = ((int64_t)((*t39)));
  t41 = ((int64_t)(((int64_t)UINT64_C(8))));
  v14 = ((int64_t)(((int64_t)gg_div(t40, t41))));
  /* FG_DIRECT_SLICE_INDEX */
  t43 = ((int64_t)UINT64_C(0));
  t44 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t43, ((int64_t)UINT64_C(4)));
  t45 = ((int64_t)((*t44)));
  /* FG_DIRECT_SLICE_INDEX */
  t46 = ((int64_t)UINT64_C(0));
  t47 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t46, ((int64_t)UINT64_C(4)));
  t48 = ((int64_t)((*t47)));
  t49 = ((int64_t)gg_div(t45, t48));
  t42 = 0;
label6:;
  if (t42 >= t49) goto label8;
  v15 = t42;
  {
  t50 = ((int64_t)(v6));
  t51 = ((int64_t)(((int64_t)UINT64_C(8))));
  t52 = ((int64_t)(((int64_t)gg_div(t50, t51))));
  t53 = ((int64_t)(v15));
  t54 = ((int64_t)(v14));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t56 = ((int64_t)(v8));
  t57 = ((int64_t)(((int64_t)UINT64_C(8))));
  t58 = ((int64_t)(((int64_t)gg_div(t56, t57))));
  t59 = ((int64_t)(v15));
  t60 = ((int64_t)(v14));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t61)))));
  t63 = v14;
  t62 = 0;
label9:;
  if (t62 >= t63) goto label11;
  v18 = t62;
  {
  t64 = v2;
  t65 = ((int64_t)(v16));
  t66 = ((int64_t)(v18));
  t67 = ((int64_t)((uint64_t)(t65) + (uint64_t)(t66)));
  t68 = ((gg_type_10 *)t64.data) + gg_index(t67, t64.len);
  v19 = (*t68);
  t69 = v4;
  t70 = ((int64_t)(v7));
  t71 = ((int64_t)(((int64_t)UINT64_C(8))));
  t72 = ((int64_t)(((int64_t)gg_div(t70, t71))));
  t73 = ((int64_t)(v18));
  t74 = ((int64_t)((uint64_t)(t72) + (uint64_t)(t73)));
  t75 = ((gg_type_10 *)t69.data) + gg_index(t74, t69.len);
  v20 = (*t75);
  memset(&t76, 0, sizeof(t76));
  t77 = ((float)(t76));
  for (t78=0; t78<8; t78++) (v21).data[t78] = t77;
  t80 = ((int64_t)UINT64_C(8));
  t79 = 0;
label12:;
  if (t79 >= t80) goto label14;
  v22 = t79;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t81 = v22;
  t82 = ((float *)(v21).data) + gg_index(t81, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t83 = v22;
  t84 = ((float *)(v19).data) + gg_index(t83, ((int64_t)UINT64_C(8)));
  t85 = ((float)((*t84)));
  /* FG_DIRECT_SLICE_INDEX */
  t86 = v22;
  t87 = ((float *)(v20).data) + gg_index(t86, ((int64_t)UINT64_C(8)));
  t88 = ((float)((*t87)));
  (*t82) = ((float)(((t85) / (t88))));
  }
label13:;
  t79++; goto label12;
label14:;
  t89 = v5;
  t90 = ((int64_t)(v17));
  t91 = ((int64_t)(v18));
  t92 = ((int64_t)((uint64_t)(t90) + (uint64_t)(t91)));
  t93 = ((gg_type_10 *)t89.data) + gg_index(t92, t89.len);
  (*t93) = v21;
  }
label10:;
  t62++; goto label9;
label11:;
  }
label7:;
  t42++; goto label6;
label8:;
  }
  }
  }
gg_done:;
}
static void gg_function_5(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  int64_t t0;
  int64_t * t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  float * t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  gg_slice t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  gg_type_10 * t19;
  float t20;
  float t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  float * t26;
  int64_t t27;
  float * t28;
  float t29;
  float t30;
  gg_slice t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  gg_type_10 * t37;
  int64_t t38;
  int64_t * t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t * t44;
  int64_t t45;
  int64_t t46;
  int64_t * t47;
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
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  gg_type_10 * t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  gg_type_10 * t75;
  float t76;
  float t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  float * t82;
  int64_t t83;
  float * t84;
  float t85;
  int64_t t86;
  float * t87;
  float t88;
  gg_slice t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  gg_type_10 * t93;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  int64_t v10;
  gg_type_10 v11;
  gg_type_10 v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  gg_type_10 v19;
  gg_type_10 v20;
  gg_type_10 v21;
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
  {
  /* FG_DIRECT_SLICE_INDEX */
  t0 = ((int64_t)UINT64_C(0));
  t1 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t0, ((int64_t)UINT64_C(4)));
  t2 = ((int64_t)((*t1)));
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = ((t2) == (t3));
  if (t4) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t5 = v7;
  t6 = ((float *)(v3).data) + gg_index(t5, (v3).len);
  v9 = ((float)((*t6)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = ((int64_t)((*t9)));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  t12 = ((int64_t)gg_div(t10, t11));
  t7 = 0;
label0:;
  if (t7 >= t12) goto label2;
  v10 = t7;
  {
  t13 = v2;
  t14 = ((int64_t)(v6));
  t15 = ((int64_t)(((int64_t)UINT64_C(8))));
  t16 = ((int64_t)(((int64_t)gg_div(t14, t15))));
  t17 = ((int64_t)(v10));
  t18 = ((int64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  t19 = ((gg_type_10 *)t13.data) + gg_index(t18, t13.len);
  v11 = (*t19);
  memset(&t20, 0, sizeof(t20));
  t21 = ((float)(t20));
  for (t22=0; t22<8; t22++) (v12).data[t22] = t21;
  t24 = ((int64_t)UINT64_C(8));
  t23 = 0;
label3:;
  if (t23 >= t24) goto label5;
  v13 = t23;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v13;
  t26 = ((float *)(v12).data) + gg_index(t25, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = v13;
  t28 = ((float *)(v11).data) + gg_index(t27, ((int64_t)UINT64_C(8)));
  t29 = ((float)((*t28)));
  t30 = ((float)(v9));
  (*t26) = ((float)(((t29) * (t30))));
  }
label4:;
  t23++; goto label3;
label5:;
  t31 = v5;
  t32 = ((int64_t)(v8));
  t33 = ((int64_t)(((int64_t)UINT64_C(8))));
  t34 = ((int64_t)(((int64_t)gg_div(t32, t33))));
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  t37 = ((gg_type_10 *)t31.data) + gg_index(t36, t31.len);
  (*t37) = v12;
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t38 = ((int64_t)UINT64_C(0));
  t39 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t38, ((int64_t)UINT64_C(4)));
  t40 = ((int64_t)((*t39)));
  t41 = ((int64_t)(((int64_t)UINT64_C(8))));
  v14 = ((int64_t)(((int64_t)gg_div(t40, t41))));
  /* FG_DIRECT_SLICE_INDEX */
  t43 = ((int64_t)UINT64_C(0));
  t44 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t43, ((int64_t)UINT64_C(4)));
  t45 = ((int64_t)((*t44)));
  /* FG_DIRECT_SLICE_INDEX */
  t46 = ((int64_t)UINT64_C(0));
  t47 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t46, ((int64_t)UINT64_C(4)));
  t48 = ((int64_t)((*t47)));
  t49 = ((int64_t)gg_div(t45, t48));
  t42 = 0;
label6:;
  if (t42 >= t49) goto label8;
  v15 = t42;
  {
  t50 = ((int64_t)(v6));
  t51 = ((int64_t)(((int64_t)UINT64_C(8))));
  t52 = ((int64_t)(((int64_t)gg_div(t50, t51))));
  t53 = ((int64_t)(v15));
  t54 = ((int64_t)(v14));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t56 = ((int64_t)(v8));
  t57 = ((int64_t)(((int64_t)UINT64_C(8))));
  t58 = ((int64_t)(((int64_t)gg_div(t56, t57))));
  t59 = ((int64_t)(v15));
  t60 = ((int64_t)(v14));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t61)))));
  t63 = v14;
  t62 = 0;
label9:;
  if (t62 >= t63) goto label11;
  v18 = t62;
  {
  t64 = v2;
  t65 = ((int64_t)(v16));
  t66 = ((int64_t)(v18));
  t67 = ((int64_t)((uint64_t)(t65) + (uint64_t)(t66)));
  t68 = ((gg_type_10 *)t64.data) + gg_index(t67, t64.len);
  v19 = (*t68);
  t69 = v4;
  t70 = ((int64_t)(v7));
  t71 = ((int64_t)(((int64_t)UINT64_C(8))));
  t72 = ((int64_t)(((int64_t)gg_div(t70, t71))));
  t73 = ((int64_t)(v18));
  t74 = ((int64_t)((uint64_t)(t72) + (uint64_t)(t73)));
  t75 = ((gg_type_10 *)t69.data) + gg_index(t74, t69.len);
  v20 = (*t75);
  memset(&t76, 0, sizeof(t76));
  t77 = ((float)(t76));
  for (t78=0; t78<8; t78++) (v21).data[t78] = t77;
  t80 = ((int64_t)UINT64_C(8));
  t79 = 0;
label12:;
  if (t79 >= t80) goto label14;
  v22 = t79;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t81 = v22;
  t82 = ((float *)(v21).data) + gg_index(t81, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t83 = v22;
  t84 = ((float *)(v19).data) + gg_index(t83, ((int64_t)UINT64_C(8)));
  t85 = ((float)((*t84)));
  /* FG_DIRECT_SLICE_INDEX */
  t86 = v22;
  t87 = ((float *)(v20).data) + gg_index(t86, ((int64_t)UINT64_C(8)));
  t88 = ((float)((*t87)));
  (*t82) = ((float)(((t85) * (t88))));
  }
label13:;
  t79++; goto label12;
label14:;
  t89 = v5;
  t90 = ((int64_t)(v17));
  t91 = ((int64_t)(v18));
  t92 = ((int64_t)((uint64_t)(t90) + (uint64_t)(t91)));
  t93 = ((gg_type_10 *)t89.data) + gg_index(t92, t89.len);
  (*t93) = v21;
  }
label10:;
  t62++; goto label9;
label11:;
  }
label7:;
  t42++; goto label6;
label8:;
  }
  }
  }
gg_done:;
}
static void gg_function_4(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  int64_t t0;
  int64_t * t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  float * t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  gg_slice t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  gg_type_10 * t19;
  float t20;
  float t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  float * t26;
  int64_t t27;
  float * t28;
  float t29;
  float t30;
  gg_slice t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  gg_type_10 * t37;
  int64_t t38;
  int64_t * t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t * t44;
  int64_t t45;
  int64_t t46;
  int64_t * t47;
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
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  gg_type_10 * t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  gg_type_10 * t75;
  float t76;
  float t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  float * t82;
  int64_t t83;
  float * t84;
  float t85;
  int64_t t86;
  float * t87;
  float t88;
  gg_slice t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  gg_type_10 * t93;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  int64_t v10;
  gg_type_10 v11;
  gg_type_10 v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  gg_type_10 v19;
  gg_type_10 v20;
  gg_type_10 v21;
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
  {
  /* FG_DIRECT_SLICE_INDEX */
  t0 = ((int64_t)UINT64_C(0));
  t1 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t0, ((int64_t)UINT64_C(4)));
  t2 = ((int64_t)((*t1)));
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = ((t2) == (t3));
  if (t4) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t5 = v7;
  t6 = ((float *)(v3).data) + gg_index(t5, (v3).len);
  v9 = ((float)((*t6)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = ((int64_t)((*t9)));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  t12 = ((int64_t)gg_div(t10, t11));
  t7 = 0;
label0:;
  if (t7 >= t12) goto label2;
  v10 = t7;
  {
  t13 = v2;
  t14 = ((int64_t)(v6));
  t15 = ((int64_t)(((int64_t)UINT64_C(8))));
  t16 = ((int64_t)(((int64_t)gg_div(t14, t15))));
  t17 = ((int64_t)(v10));
  t18 = ((int64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  t19 = ((gg_type_10 *)t13.data) + gg_index(t18, t13.len);
  v11 = (*t19);
  memset(&t20, 0, sizeof(t20));
  t21 = ((float)(t20));
  for (t22=0; t22<8; t22++) (v12).data[t22] = t21;
  t24 = ((int64_t)UINT64_C(8));
  t23 = 0;
label3:;
  if (t23 >= t24) goto label5;
  v13 = t23;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v13;
  t26 = ((float *)(v12).data) + gg_index(t25, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = v13;
  t28 = ((float *)(v11).data) + gg_index(t27, ((int64_t)UINT64_C(8)));
  t29 = ((float)((*t28)));
  t30 = ((float)(v9));
  (*t26) = ((float)(((t29) - (t30))));
  }
label4:;
  t23++; goto label3;
label5:;
  t31 = v5;
  t32 = ((int64_t)(v8));
  t33 = ((int64_t)(((int64_t)UINT64_C(8))));
  t34 = ((int64_t)(((int64_t)gg_div(t32, t33))));
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  t37 = ((gg_type_10 *)t31.data) + gg_index(t36, t31.len);
  (*t37) = v12;
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t38 = ((int64_t)UINT64_C(0));
  t39 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t38, ((int64_t)UINT64_C(4)));
  t40 = ((int64_t)((*t39)));
  t41 = ((int64_t)(((int64_t)UINT64_C(8))));
  v14 = ((int64_t)(((int64_t)gg_div(t40, t41))));
  /* FG_DIRECT_SLICE_INDEX */
  t43 = ((int64_t)UINT64_C(0));
  t44 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t43, ((int64_t)UINT64_C(4)));
  t45 = ((int64_t)((*t44)));
  /* FG_DIRECT_SLICE_INDEX */
  t46 = ((int64_t)UINT64_C(0));
  t47 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t46, ((int64_t)UINT64_C(4)));
  t48 = ((int64_t)((*t47)));
  t49 = ((int64_t)gg_div(t45, t48));
  t42 = 0;
label6:;
  if (t42 >= t49) goto label8;
  v15 = t42;
  {
  t50 = ((int64_t)(v6));
  t51 = ((int64_t)(((int64_t)UINT64_C(8))));
  t52 = ((int64_t)(((int64_t)gg_div(t50, t51))));
  t53 = ((int64_t)(v15));
  t54 = ((int64_t)(v14));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t56 = ((int64_t)(v8));
  t57 = ((int64_t)(((int64_t)UINT64_C(8))));
  t58 = ((int64_t)(((int64_t)gg_div(t56, t57))));
  t59 = ((int64_t)(v15));
  t60 = ((int64_t)(v14));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t61)))));
  t63 = v14;
  t62 = 0;
label9:;
  if (t62 >= t63) goto label11;
  v18 = t62;
  {
  t64 = v2;
  t65 = ((int64_t)(v16));
  t66 = ((int64_t)(v18));
  t67 = ((int64_t)((uint64_t)(t65) + (uint64_t)(t66)));
  t68 = ((gg_type_10 *)t64.data) + gg_index(t67, t64.len);
  v19 = (*t68);
  t69 = v4;
  t70 = ((int64_t)(v7));
  t71 = ((int64_t)(((int64_t)UINT64_C(8))));
  t72 = ((int64_t)(((int64_t)gg_div(t70, t71))));
  t73 = ((int64_t)(v18));
  t74 = ((int64_t)((uint64_t)(t72) + (uint64_t)(t73)));
  t75 = ((gg_type_10 *)t69.data) + gg_index(t74, t69.len);
  v20 = (*t75);
  memset(&t76, 0, sizeof(t76));
  t77 = ((float)(t76));
  for (t78=0; t78<8; t78++) (v21).data[t78] = t77;
  t80 = ((int64_t)UINT64_C(8));
  t79 = 0;
label12:;
  if (t79 >= t80) goto label14;
  v22 = t79;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t81 = v22;
  t82 = ((float *)(v21).data) + gg_index(t81, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t83 = v22;
  t84 = ((float *)(v19).data) + gg_index(t83, ((int64_t)UINT64_C(8)));
  t85 = ((float)((*t84)));
  /* FG_DIRECT_SLICE_INDEX */
  t86 = v22;
  t87 = ((float *)(v20).data) + gg_index(t86, ((int64_t)UINT64_C(8)));
  t88 = ((float)((*t87)));
  (*t82) = ((float)(((t85) - (t88))));
  }
label13:;
  t79++; goto label12;
label14:;
  t89 = v5;
  t90 = ((int64_t)(v17));
  t91 = ((int64_t)(v18));
  t92 = ((int64_t)((uint64_t)(t90) + (uint64_t)(t91)));
  t93 = ((gg_type_10 *)t89.data) + gg_index(t92, t89.len);
  (*t93) = v21;
  }
label10:;
  t62++; goto label9;
label11:;
  }
label7:;
  t42++; goto label6;
label8:;
  }
  }
  }
gg_done:;
}
static void gg_function_3(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  int64_t t0;
  int64_t * t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  float * t6;
  int64_t t7;
  int64_t t8;
  int64_t * t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  gg_slice t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  gg_type_10 * t19;
  float t20;
  float t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  float * t26;
  int64_t t27;
  float * t28;
  float t29;
  float t30;
  gg_slice t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  gg_type_10 * t37;
  int64_t t38;
  int64_t * t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t * t44;
  int64_t t45;
  int64_t t46;
  int64_t * t47;
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
  gg_slice t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  gg_type_10 * t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  gg_type_10 * t75;
  float t76;
  float t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  float * t82;
  int64_t t83;
  float * t84;
  float t85;
  int64_t t86;
  float * t87;
  float t88;
  gg_slice t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  gg_type_10 * t93;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  int64_t v10;
  gg_type_10 v11;
  gg_type_10 v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  gg_type_10 v19;
  gg_type_10 v20;
  gg_type_10 v21;
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
  {
  /* FG_DIRECT_SLICE_INDEX */
  t0 = ((int64_t)UINT64_C(0));
  t1 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t0, ((int64_t)UINT64_C(4)));
  t2 = ((int64_t)((*t1)));
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = ((t2) == (t3));
  if (t4) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t5 = v7;
  t6 = ((float *)(v3).data) + gg_index(t5, (v3).len);
  v9 = ((float)((*t6)));
  /* FG_DIRECT_SLICE_INDEX */
  t8 = ((int64_t)UINT64_C(0));
  t9 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t8, ((int64_t)UINT64_C(4)));
  t10 = ((int64_t)((*t9)));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  t12 = ((int64_t)gg_div(t10, t11));
  t7 = 0;
label0:;
  if (t7 >= t12) goto label2;
  v10 = t7;
  {
  t13 = v2;
  t14 = ((int64_t)(v6));
  t15 = ((int64_t)(((int64_t)UINT64_C(8))));
  t16 = ((int64_t)(((int64_t)gg_div(t14, t15))));
  t17 = ((int64_t)(v10));
  t18 = ((int64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  t19 = ((gg_type_10 *)t13.data) + gg_index(t18, t13.len);
  v11 = (*t19);
  memset(&t20, 0, sizeof(t20));
  t21 = ((float)(t20));
  for (t22=0; t22<8; t22++) (v12).data[t22] = t21;
  t24 = ((int64_t)UINT64_C(8));
  t23 = 0;
label3:;
  if (t23 >= t24) goto label5;
  v13 = t23;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v13;
  t26 = ((float *)(v12).data) + gg_index(t25, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = v13;
  t28 = ((float *)(v11).data) + gg_index(t27, ((int64_t)UINT64_C(8)));
  t29 = ((float)((*t28)));
  t30 = ((float)(v9));
  (*t26) = ((float)(((t29) + (t30))));
  }
label4:;
  t23++; goto label3;
label5:;
  t31 = v5;
  t32 = ((int64_t)(v8));
  t33 = ((int64_t)(((int64_t)UINT64_C(8))));
  t34 = ((int64_t)(((int64_t)gg_div(t32, t33))));
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  t37 = ((gg_type_10 *)t31.data) + gg_index(t36, t31.len);
  (*t37) = v12;
  }
label1:;
  t7++; goto label0;
label2:;
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t38 = ((int64_t)UINT64_C(0));
  t39 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t38, ((int64_t)UINT64_C(4)));
  t40 = ((int64_t)((*t39)));
  t41 = ((int64_t)(((int64_t)UINT64_C(8))));
  v14 = ((int64_t)(((int64_t)gg_div(t40, t41))));
  /* FG_DIRECT_SLICE_INDEX */
  t43 = ((int64_t)UINT64_C(0));
  t44 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t43, ((int64_t)UINT64_C(4)));
  t45 = ((int64_t)((*t44)));
  /* FG_DIRECT_SLICE_INDEX */
  t46 = ((int64_t)UINT64_C(0));
  t47 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t46, ((int64_t)UINT64_C(4)));
  t48 = ((int64_t)((*t47)));
  t49 = ((int64_t)gg_div(t45, t48));
  t42 = 0;
label6:;
  if (t42 >= t49) goto label8;
  v15 = t42;
  {
  t50 = ((int64_t)(v6));
  t51 = ((int64_t)(((int64_t)UINT64_C(8))));
  t52 = ((int64_t)(((int64_t)gg_div(t50, t51))));
  t53 = ((int64_t)(v15));
  t54 = ((int64_t)(v14));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t56 = ((int64_t)(v8));
  t57 = ((int64_t)(((int64_t)UINT64_C(8))));
  t58 = ((int64_t)(((int64_t)gg_div(t56, t57))));
  t59 = ((int64_t)(v15));
  t60 = ((int64_t)(v14));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t61)))));
  t63 = v14;
  t62 = 0;
label9:;
  if (t62 >= t63) goto label11;
  v18 = t62;
  {
  t64 = v2;
  t65 = ((int64_t)(v16));
  t66 = ((int64_t)(v18));
  t67 = ((int64_t)((uint64_t)(t65) + (uint64_t)(t66)));
  t68 = ((gg_type_10 *)t64.data) + gg_index(t67, t64.len);
  v19 = (*t68);
  t69 = v4;
  t70 = ((int64_t)(v7));
  t71 = ((int64_t)(((int64_t)UINT64_C(8))));
  t72 = ((int64_t)(((int64_t)gg_div(t70, t71))));
  t73 = ((int64_t)(v18));
  t74 = ((int64_t)((uint64_t)(t72) + (uint64_t)(t73)));
  t75 = ((gg_type_10 *)t69.data) + gg_index(t74, t69.len);
  v20 = (*t75);
  memset(&t76, 0, sizeof(t76));
  t77 = ((float)(t76));
  for (t78=0; t78<8; t78++) (v21).data[t78] = t77;
  t80 = ((int64_t)UINT64_C(8));
  t79 = 0;
label12:;
  if (t79 >= t80) goto label14;
  v22 = t79;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t81 = v22;
  t82 = ((float *)(v21).data) + gg_index(t81, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t83 = v22;
  t84 = ((float *)(v19).data) + gg_index(t83, ((int64_t)UINT64_C(8)));
  t85 = ((float)((*t84)));
  /* FG_DIRECT_SLICE_INDEX */
  t86 = v22;
  t87 = ((float *)(v20).data) + gg_index(t86, ((int64_t)UINT64_C(8)));
  t88 = ((float)((*t87)));
  (*t82) = ((float)(((t85) + (t88))));
  }
label13:;
  t79++; goto label12;
label14:;
  t89 = v5;
  t90 = ((int64_t)(v17));
  t91 = ((int64_t)(v18));
  t92 = ((int64_t)((uint64_t)(t90) + (uint64_t)(t91)));
  t93 = ((gg_type_10 *)t89.data) + gg_index(t92, t89.len);
  (*t93) = v21;
  }
label10:;
  t62++; goto label9;
label11:;
  }
label7:;
  t42++; goto label6;
label8:;
  }
  }
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
static void gg_function_1(gg_type_12 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9, int64_t base) {
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
  int64_t t10;
  int64_t * t11;
  int64_t t12;
  int64_t t13;
  int64_t * t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t * t18;
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
  int64_t * t36;
  int64_t t37;
  int64_t t38;
  int64_t * t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t * t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t * t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t * t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t * t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t * t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t * t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t * t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t * t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t * t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t * t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t * t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t * t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  int64_t * t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t * t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t * t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t * t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t * t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t * t132;
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
  uint8_t t145;
  uint8_t t146;
  int64_t t147;
  int64_t t148;
  uint8_t t149;
  uint8_t t150;
  int64_t t151;
  int64_t * t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  uint8_t t157;
  uint8_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  uint8_t t163;
  uint8_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint8_t t169;
  uint8_t t170;
  int64_t t171;
  int64_t * t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  uint8_t t176;
  int64_t t177;
  int64_t * t178;
  int64_t t179;
  int64_t t180;
  uint8_t t181;
  uint8_t t182;
  int64_t t183;
  int64_t * t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  uint8_t t189;
  uint8_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  uint8_t t195;
  int64_t t196;
  int64_t t197;
  uint8_t t198;
  gg_type_12 t199;
  gg_slice t200;
  gg_slice t201;
  gg_slice t202;
  gg_slice t203;
  gg_slice t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  uint8_t t210;
  gg_type_12 t211;
  gg_slice t212;
  gg_slice t213;
  gg_slice t214;
  gg_slice t215;
  gg_slice t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  uint8_t t222;
  gg_type_12 t223;
  gg_slice t224;
  gg_slice t225;
  gg_slice t226;
  gg_slice t227;
  gg_slice t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  uint8_t t234;
  gg_type_12 t235;
  gg_slice t236;
  gg_slice t237;
  gg_slice t238;
  gg_slice t239;
  gg_slice t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  uint8_t t246;
  int64_t t247;
  int64_t t248;
  uint8_t t249;
  gg_type_12 t250;
  gg_slice t251;
  gg_slice t252;
  gg_slice t253;
  gg_slice t254;
  gg_slice t255;
  gg_slice t256;
  gg_slice t257;
  gg_slice t258;
  gg_slice t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  uint8_t t265;
  gg_type_12 t266;
  gg_slice t267;
  gg_slice t268;
  gg_slice t269;
  gg_slice t270;
  gg_slice t271;
  gg_slice t272;
  gg_slice t273;
  gg_slice t274;
  gg_slice t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  uint8_t t281;
  gg_type_12 t282;
  gg_slice t283;
  gg_slice t284;
  gg_slice t285;
  gg_slice t286;
  gg_slice t287;
  gg_slice t288;
  gg_slice t289;
  gg_slice t290;
  gg_slice t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  uint8_t t297;
  gg_type_12 t298;
  gg_slice t299;
  gg_slice t300;
  gg_slice t301;
  gg_slice t302;
  gg_slice t303;
  gg_slice t304;
  gg_slice t305;
  gg_slice t306;
  gg_slice t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  uint8_t t313;
  gg_type_12 t314;
  gg_slice t315;
  gg_slice t316;
  gg_slice t317;
  gg_slice t318;
  gg_slice t319;
  gg_slice t320;
  gg_slice t321;
  gg_slice t322;
  gg_slice t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  uint8_t t329;
  gg_type_12 t330;
  gg_slice t331;
  gg_slice t332;
  gg_slice t333;
  gg_slice t334;
  gg_slice t335;
  gg_slice t336;
  gg_slice t337;
  gg_slice t338;
  gg_slice t339;
  int64_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  uint8_t t345;
  gg_type_12 t346;
  gg_slice t347;
  gg_slice t348;
  gg_slice t349;
  gg_slice t350;
  gg_slice t351;
  gg_slice t352;
  gg_slice t353;
  gg_slice t354;
  gg_slice t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t t359;
  int64_t t360;
  uint8_t t361;
  gg_type_12 t362;
  gg_slice t363;
  gg_slice t364;
  gg_slice t365;
  gg_slice t366;
  gg_slice t367;
  gg_slice t368;
  gg_slice t369;
  gg_slice t370;
  gg_slice t371;
  int64_t t372;
  int64_t t373;
  int64_t t374;
  int64_t t375;
  int64_t t376;
  uint8_t t377;
  gg_type_12 t378;
  gg_slice t379;
  gg_slice t380;
  gg_slice t381;
  gg_slice t382;
  gg_slice t383;
  gg_slice t384;
  gg_slice t385;
  gg_slice t386;
  gg_slice t387;
  int64_t t388;
  int64_t t389;
  int64_t t390;
  int64_t t391;
  int64_t t392;
  uint8_t t393;
  gg_type_12 t394;
  gg_slice t395;
  gg_slice t396;
  gg_slice t397;
  gg_slice t398;
  gg_slice t399;
  gg_slice t400;
  gg_slice t401;
  gg_slice t402;
  gg_slice t403;
  int64_t t404;
  int64_t t405;
  int64_t t406;
  int64_t t407;
  int64_t t408;
  uint8_t t409;
  gg_type_12 t410;
  gg_slice t411;
  gg_slice t412;
  gg_slice t413;
  gg_slice t414;
  gg_slice t415;
  gg_slice t416;
  gg_slice t417;
  gg_slice t418;
  gg_slice t419;
  int64_t t420;
  int64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  uint8_t t425;
  gg_type_12 t426;
  gg_slice t427;
  gg_slice t428;
  gg_slice t429;
  gg_slice t430;
  gg_slice t431;
  gg_slice t432;
  gg_slice t433;
  gg_slice t434;
  gg_slice t435;
  int64_t t436;
  int64_t t437;
  int64_t t438;
  int64_t t439;
  int64_t t440;
  uint8_t t441;
  gg_type_12 t442;
  gg_slice t443;
  gg_slice t444;
  gg_slice t445;
  gg_slice t446;
  gg_slice t447;
  gg_slice t448;
  gg_slice t449;
  gg_slice t450;
  gg_slice t451;
  int64_t t452;
  int64_t t453;
  int64_t t454;
  int64_t t455;
  int64_t t456;
  uint8_t t457;
  gg_type_12 t458;
  gg_slice t459;
  gg_slice t460;
  gg_slice t461;
  gg_slice t462;
  gg_slice t463;
  gg_slice t464;
  gg_slice t465;
  gg_slice t466;
  gg_slice t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
  int64_t t472;
  uint8_t t473;
  gg_type_12 t474;
  gg_slice t475;
  gg_slice t476;
  gg_slice t477;
  gg_slice t478;
  gg_slice t479;
  gg_slice t480;
  gg_slice t481;
  gg_slice t482;
  gg_slice t483;
  int64_t t484;
  int64_t t485;
  int64_t t486;
  int64_t t487;
  int64_t t488;
  uint8_t t489;
  gg_type_12 t490;
  gg_slice t491;
  gg_slice t492;
  gg_slice t493;
  gg_slice t494;
  gg_slice t495;
  gg_slice t496;
  gg_slice t497;
  gg_slice t498;
  gg_slice t499;
  int64_t t500;
  int64_t t501;
  int64_t t502;
  int64_t t503;
  int64_t t504;
  uint8_t t505;
  gg_type_12 t506;
  gg_slice t507;
  gg_slice t508;
  gg_slice t509;
  gg_slice t510;
  gg_slice t511;
  gg_slice t512;
  gg_slice t513;
  gg_slice t514;
  gg_slice t515;
  int64_t t516;
  int64_t t517;
  int64_t t518;
  int64_t t519;
  int64_t t520;
  uint8_t t521;
  gg_type_12 t522;
  gg_slice t523;
  gg_slice t524;
  gg_slice t525;
  gg_slice t526;
  gg_slice t527;
  gg_slice t528;
  gg_slice t529;
  gg_slice t530;
  gg_slice t531;
  int64_t t532;
  int64_t t533;
  int64_t t534;
  int64_t t535;
  int64_t t536;
  uint8_t t537;
  gg_type_12 t538;
  gg_slice t539;
  gg_slice t540;
  gg_slice t541;
  gg_slice t542;
  gg_slice t543;
  gg_slice t544;
  gg_slice t545;
  gg_slice t546;
  gg_slice t547;
  int64_t t548;
  int64_t t549;
  int64_t t550;
  int64_t t551;
  int64_t t552;
  uint8_t t553;
  gg_type_12 t554;
  gg_slice t555;
  gg_slice t556;
  gg_slice t557;
  gg_slice t558;
  gg_slice t559;
  gg_slice t560;
  gg_slice t561;
  gg_slice t562;
  gg_slice t563;
  int64_t t564;
  int64_t t565;
  int64_t t566;
  int64_t t567;
  int64_t t568;
  uint8_t t569;
  gg_type_12 t570;
  gg_slice t571;
  gg_slice t572;
  gg_slice t573;
  gg_slice t574;
  gg_slice t575;
  gg_slice t576;
  gg_slice t577;
  gg_slice t578;
  gg_slice t579;
  int64_t t580;
  int64_t t581;
  int64_t t582;
  int64_t t583;
  int64_t t584;
  uint8_t t585;
  gg_type_12 t586;
  gg_slice t587;
  gg_slice t588;
  gg_slice t589;
  gg_slice t590;
  gg_slice t591;
  gg_slice t592;
  gg_slice t593;
  gg_slice t594;
  gg_slice t595;
  int64_t t596;
  int64_t t597;
  int64_t t598;
  int64_t t599;
  int64_t t600;
  uint8_t t601;
  gg_type_12 t602;
  gg_slice t603;
  gg_slice t604;
  gg_slice t605;
  gg_slice t606;
  gg_slice t607;
  gg_slice t608;
  gg_slice t609;
  gg_slice t610;
  gg_slice t611;
  int64_t t612;
  int64_t t613;
  int64_t t614;
  int64_t t615;
  int64_t t616;
  uint8_t t617;
  gg_type_12 t618;
  gg_slice t619;
  gg_slice t620;
  gg_slice t621;
  gg_slice t622;
  gg_slice t623;
  gg_slice t624;
  gg_slice t625;
  gg_slice t626;
  gg_slice t627;
  int64_t t628;
  int64_t t629;
  int64_t t630;
  int64_t t631;
  int64_t t632;
  uint8_t t633;
  gg_type_12 t634;
  gg_slice t635;
  gg_slice t636;
  gg_slice t637;
  gg_slice t638;
  gg_slice t639;
  gg_slice t640;
  gg_slice t641;
  gg_slice t642;
  gg_slice t643;
  int64_t t644;
  int64_t t645;
  int64_t t646;
  int64_t t647;
  int64_t t648;
  uint8_t t649;
  gg_type_12 t650;
  gg_slice t651;
  gg_slice t652;
  gg_slice t653;
  gg_slice t654;
  gg_slice t655;
  gg_slice t656;
  gg_slice t657;
  gg_slice t658;
  gg_slice t659;
  int64_t t660;
  int64_t t661;
  int64_t t662;
  int64_t t663;
  int64_t t664;
  uint8_t t665;
  gg_type_12 t666;
  gg_slice t667;
  gg_slice t668;
  gg_slice t669;
  gg_slice t670;
  gg_slice t671;
  gg_slice t672;
  gg_slice t673;
  gg_slice t674;
  gg_slice t675;
  int64_t t676;
  int64_t t677;
  int64_t t678;
  int64_t t679;
  int64_t t680;
  int64_t * t681;
  int64_t t682;
  int64_t t683;
  int64_t t684;
  int64_t t685;
  int64_t t686;
  uint8_t t687;
  int64_t t688;
  int64_t t689;
  int64_t t690;
  gg_type_12 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  gg_slice v8;
  gg_slice v9;
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
  t0 = ((int64_t)(((*(v0))).f7));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(((*(v0))).f6));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) >= (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(((*(v0))).f6));
  t9 = ((int64_t)(((*(v0))).f7));
  t7 = ((t8) < (t9));
  }
  if (!(t7)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t10 = ((int64_t)UINT64_C(1));
  t11 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t10, ((int64_t)UINT64_C(4)));
  t12 = ((int64_t)((*t11)));
  /* FG_DIRECT_SLICE_INDEX */
  t13 = ((int64_t)UINT64_C(2));
  t14 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t13, ((int64_t)UINT64_C(4)));
  t15 = ((int64_t)((*t14)));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t15)))));
  /* FG_DIRECT_SLICE_INDEX */
  t17 = ((int64_t)UINT64_C(3));
  t18 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t17, ((int64_t)UINT64_C(4)));
  t19 = ((int64_t)((*t18)));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t19)))));
  t20 = ((int64_t)(v10));
  t21 = ((int64_t)(((*(v0))).f7));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) - (uint64_t)(t23)))));
  t25 = ((int64_t)(((*(v0))).f7));
  v11 = ((int64_t)(((int64_t)gg_div(t24, t25))));
  t26 = ((int64_t)(v11));
  t27 = ((int64_t)(((*(v0))).f6));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t28 = ((int64_t)(v12));
  t29 = ((int64_t)(v11));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(v10));
  v13 = gg_function_2(t30, t31, base + 0);
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  v15 = ((int64_t)(v12));
label1:;
  t32 = ((int64_t)(v15));
  t33 = ((int64_t)(v13));
  if (!(((t32) < (t33)))) goto label2;
  {
  t34 = ((int64_t)(v15));
  /* FG_DIRECT_SLICE_INDEX */
  t35 = ((int64_t)UINT64_C(2));
  t36 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t35, ((int64_t)UINT64_C(4)));
  t37 = ((int64_t)((*t36)));
  /* FG_DIRECT_SLICE_INDEX */
  t38 = ((int64_t)UINT64_C(1));
  t39 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t38, ((int64_t)UINT64_C(4)));
  t40 = ((int64_t)((*t39)));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t40)))));
  v16 = ((int64_t)(((int64_t)gg_div(t34, t41))));
  t42 = ((int64_t)(v15));
  t43 = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t44 = ((int64_t)UINT64_C(2));
  t45 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t44, ((int64_t)UINT64_C(4)));
  t46 = ((int64_t)((*t45)));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t46)))));
  /* FG_DIRECT_SLICE_INDEX */
  t48 = ((int64_t)UINT64_C(1));
  t49 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t48, ((int64_t)UINT64_C(4)));
  t50 = ((int64_t)((*t49)));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t50)))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t42) - (uint64_t)(t51)))));
  /* FG_DIRECT_SLICE_INDEX */
  t53 = ((int64_t)UINT64_C(1));
  t54 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t53, ((int64_t)UINT64_C(4)));
  t55 = ((int64_t)((*t54)));
  v17 = ((int64_t)(((int64_t)gg_div(t52, t55))));
  t56 = ((int64_t)(v15));
  t57 = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t58 = ((int64_t)UINT64_C(2));
  t59 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t58, ((int64_t)UINT64_C(4)));
  t60 = ((int64_t)((*t59)));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t60)))));
  /* FG_DIRECT_SLICE_INDEX */
  t62 = ((int64_t)UINT64_C(1));
  t63 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t62, ((int64_t)UINT64_C(4)));
  t64 = ((int64_t)((*t63)));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t61) * (uint64_t)(t64)))));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t56) - (uint64_t)(t65)))));
  t67 = ((int64_t)(v17));
  /* FG_DIRECT_SLICE_INDEX */
  t68 = ((int64_t)UINT64_C(1));
  t69 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t68, ((int64_t)UINT64_C(4)));
  t70 = ((int64_t)((*t69)));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t70)))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t66) - (uint64_t)(t71)))));
  t72 = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t73 = ((int64_t)UINT64_C(3));
  t74 = ((int64_t *)((((*(v0))).f3).f1).data) + gg_index(t73, ((int64_t)UINT64_C(4)));
  t75 = ((int64_t)((*t74)));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t75)))));
  t77 = ((int64_t)(v17));
  /* FG_DIRECT_SLICE_INDEX */
  t78 = ((int64_t)UINT64_C(2));
  t79 = ((int64_t *)((((*(v0))).f3).f1).data) + gg_index(t78, ((int64_t)UINT64_C(4)));
  t80 = ((int64_t)((*t79)));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t81)))));
  t83 = ((int64_t)(v18));
  /* FG_DIRECT_SLICE_INDEX */
  t84 = ((int64_t)UINT64_C(1));
  t85 = ((int64_t *)((((*(v0))).f3).f1).data) + gg_index(t84, ((int64_t)UINT64_C(4)));
  t86 = ((int64_t)((*t85)));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t83) * (uint64_t)(t86)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t87)))));
  t88 = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t89 = ((int64_t)UINT64_C(3));
  t90 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t89, ((int64_t)UINT64_C(4)));
  t91 = ((int64_t)((*t90)));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t88) * (uint64_t)(t91)))));
  t93 = ((int64_t)(v17));
  /* FG_DIRECT_SLICE_INDEX */
  t94 = ((int64_t)UINT64_C(2));
  t95 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t94, ((int64_t)UINT64_C(4)));
  t96 = ((int64_t)((*t95)));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t93) * (uint64_t)(t96)))));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t92) + (uint64_t)(t97)))));
  t99 = ((int64_t)(v18));
  /* FG_DIRECT_SLICE_INDEX */
  t100 = ((int64_t)UINT64_C(1));
  t101 = ((int64_t *)((((*(v0))).f5).f1).data) + gg_index(t100, ((int64_t)UINT64_C(4)));
  t102 = ((int64_t)((*t101)));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t102)))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t98) + (uint64_t)(t103)))));
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t104 = ((int64_t)(((*(v0))).f8));
  t105 = ((int64_t)(((int64_t)UINT64_C(1))));
  t106 = ((t104) == (t105));
  if (t106) {
  {
  t107 = ((int64_t)(v16));
  /* FG_DIRECT_SLICE_INDEX */
  t108 = ((int64_t)UINT64_C(3));
  t109 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t108, ((int64_t)UINT64_C(4)));
  t110 = ((int64_t)((*t109)));
  t111 = ((int64_t)(((int64_t)gg_mod(t107, t110))));
  /* FG_DIRECT_SLICE_INDEX */
  t112 = ((int64_t)UINT64_C(3));
  t113 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t112, ((int64_t)UINT64_C(4)));
  t114 = ((int64_t)((*t113)));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t111) * (uint64_t)(t114)))));
  t116 = ((int64_t)(v17));
  /* FG_DIRECT_SLICE_INDEX */
  t117 = ((int64_t)UINT64_C(2));
  t118 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t117, ((int64_t)UINT64_C(4)));
  t119 = ((int64_t)((*t118)));
  t120 = ((int64_t)(((int64_t)gg_mod(t116, t119))));
  /* FG_DIRECT_SLICE_INDEX */
  t121 = ((int64_t)UINT64_C(2));
  t122 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t121, ((int64_t)UINT64_C(4)));
  t123 = ((int64_t)((*t122)));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t120) * (uint64_t)(t123)))));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t124)))));
  t126 = ((int64_t)(v18));
  /* FG_DIRECT_SLICE_INDEX */
  t127 = ((int64_t)UINT64_C(1));
  t128 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t127, ((int64_t)UINT64_C(4)));
  t129 = ((int64_t)((*t128)));
  t130 = ((int64_t)(((int64_t)gg_mod(t126, t129))));
  /* FG_DIRECT_SLICE_INDEX */
  t131 = ((int64_t)UINT64_C(1));
  t132 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t131, ((int64_t)UINT64_C(4)));
  t133 = ((int64_t)((*t132)));
  t134 = ((int64_t)(((int64_t)((uint64_t)(t130) * (uint64_t)(t133)))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t125) + (uint64_t)(t134)))));
  }
  }
  t135 = ((int64_t)(((*(v0))).f8));
  t136 = ((int64_t)(((int64_t)UINT64_C(1))));
  t137 = ((t135) == (t136));
  t138 = t137;
  if (t138) {
  t139 = ((int64_t)((((*(v0))).f3).f2));
  t140 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = ((t139) == (t140));
  }
  t141 = t138;
  t142 = t141;
  if (t142) {
  t143 = ((int64_t)((((*(v0))).f4).f2));
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t142 = ((t143) == (t144));
  }
  t145 = t142;
  t146 = t145;
  if (t146) {
  t147 = ((int64_t)((((*(v0))).f5).f2));
  t148 = ((int64_t)(((int64_t)UINT64_C(0))));
  t146 = ((t147) == (t148));
  }
  t149 = t146;
  t150 = t149;
  if (t150) {
  /* FG_DIRECT_SLICE_INDEX */
  t151 = ((int64_t)UINT64_C(0));
  t152 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t151, ((int64_t)UINT64_C(4)));
  t153 = ((int64_t)((*t152)));
  t154 = ((int64_t)(((int64_t)UINT64_C(8))));
  t155 = ((int64_t)(((int64_t)gg_mod(t153, t154))));
  t156 = ((int64_t)(((int64_t)UINT64_C(0))));
  t150 = ((t155) == (t156));
  }
  t157 = t150;
  t158 = t157;
  if (t158) {
  t159 = ((int64_t)(v19));
  t160 = ((int64_t)(((int64_t)UINT64_C(8))));
  t161 = ((int64_t)(((int64_t)gg_mod(t159, t160))));
  t162 = ((int64_t)(((int64_t)UINT64_C(0))));
  t158 = ((t161) == (t162));
  }
  t163 = t158;
  t164 = t163;
  if (t164) {
  t165 = ((int64_t)(v20));
  t166 = ((int64_t)(((int64_t)UINT64_C(8))));
  t167 = ((int64_t)(((int64_t)gg_mod(t165, t166))));
  t168 = ((int64_t)(((int64_t)UINT64_C(0))));
  t164 = ((t167) == (t168));
  }
  t169 = t164;
  t170 = t169;
  if (t170) {
  /* FG_DIRECT_SLICE_INDEX */
  t171 = ((int64_t)UINT64_C(0));
  t172 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t171, ((int64_t)UINT64_C(4)));
  t173 = ((int64_t)((*t172)));
  t174 = ((int64_t)(((int64_t)UINT64_C(1))));
  t175 = ((t173) == (t174));
  t176 = t175;
  if (!t176) {
  /* FG_DIRECT_SLICE_INDEX */
  t177 = ((int64_t)UINT64_C(0));
  t178 = ((int64_t *)((((*(v0))).f4).f1).data) + gg_index(t177, ((int64_t)UINT64_C(4)));
  t179 = ((int64_t)((*t178)));
  t180 = ((int64_t)(((int64_t)UINT64_C(1))));
  t181 = ((t179) == (t180));
  t182 = t181;
  if (t182) {
  /* FG_DIRECT_SLICE_INDEX */
  t183 = ((int64_t)UINT64_C(0));
  t184 = ((int64_t *)((((*(v0))).f4).f0).data) + gg_index(t183, ((int64_t)UINT64_C(4)));
  t185 = ((int64_t)((*t184)));
  t186 = ((int64_t)(((int64_t)UINT64_C(8))));
  t187 = ((int64_t)(((int64_t)gg_mod(t185, t186))));
  t188 = ((int64_t)(((int64_t)UINT64_C(0))));
  t182 = ((t187) == (t188));
  }
  t189 = t182;
  t190 = t189;
  if (t190) {
  t191 = ((int64_t)(v21));
  t192 = ((int64_t)(((int64_t)UINT64_C(8))));
  t193 = ((int64_t)(((int64_t)gg_mod(t191, t192))));
  t194 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = ((t193) == (t194));
  }
  t176 = t190;
  }
  t170 = t176;
  }
  t195 = t170;
  if (t195) {
  {
  t196 = ((int64_t)(((*(v0))).f9));
  t197 = ((int64_t)(((int64_t)UINT64_C(0))));
  t198 = ((t196) == (t197));
  if (t198) {
  {
  t199 = v0;
  t200 = v1;
  t201 = v7;
  t202 = v3;
  t203 = v8;
  t204 = v9;
  t205 = ((int64_t)(v19));
  t206 = ((int64_t)(v21));
  t207 = ((int64_t)(v20));
  gg_function_3(t199, t200, t201, t202, t203, t204, t205, t206, t207, base + 0);
  }
  } else {
  t208 = ((int64_t)(((*(v0))).f9));
  t209 = ((int64_t)(((int64_t)UINT64_C(1))));
  t210 = ((t208) == (t209));
  if (t210) {
  {
  t211 = v0;
  t212 = v1;
  t213 = v7;
  t214 = v3;
  t215 = v8;
  t216 = v9;
  t217 = ((int64_t)(v19));
  t218 = ((int64_t)(v21));
  t219 = ((int64_t)(v20));
  gg_function_4(t211, t212, t213, t214, t215, t216, t217, t218, t219, base + 0);
  }
  } else {
  t220 = ((int64_t)(((*(v0))).f9));
  t221 = ((int64_t)(((int64_t)UINT64_C(2))));
  t222 = ((t220) == (t221));
  if (t222) {
  {
  t223 = v0;
  t224 = v1;
  t225 = v7;
  t226 = v3;
  t227 = v8;
  t228 = v9;
  t229 = ((int64_t)(v19));
  t230 = ((int64_t)(v21));
  t231 = ((int64_t)(v20));
  gg_function_5(t223, t224, t225, t226, t227, t228, t229, t230, t231, base + 0);
  }
  } else {
  t232 = ((int64_t)(((*(v0))).f9));
  t233 = ((int64_t)(((int64_t)UINT64_C(3))));
  t234 = ((t232) == (t233));
  if (t234) {
  {
  t235 = v0;
  t236 = v1;
  t237 = v7;
  t238 = v3;
  t239 = v8;
  t240 = v9;
  t241 = ((int64_t)(v19));
  t242 = ((int64_t)(v21));
  t243 = ((int64_t)(v20));
  gg_function_6(t235, t236, t237, t238, t239, t240, t241, t242, t243, base + 0);
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  } else {
  t244 = ((int64_t)(((*(v0))).f8));
  t245 = ((int64_t)(((int64_t)UINT64_C(0))));
  t246 = ((t244) == (t245));
  if (t246) {
  {
  t247 = ((int64_t)(((*(v0))).f9));
  t248 = ((int64_t)(((int64_t)UINT64_C(0))));
  t249 = ((t247) == (t248));
  if (t249) {
  {
  t250 = v0;
  t251 = v1;
  t252 = v2;
  t253 = v3;
  t254 = v4;
  t255 = v5;
  t256 = v6;
  t257 = v7;
  t258 = v8;
  t259 = v9;
  t260 = ((int64_t)(v19));
  t261 = ((int64_t)(v21));
  t262 = ((int64_t)(v20));
  gg_function_7(t250, t251, t252, t253, t254, t255, t256, t257, t258, t259, t260, t261, t262, base + 0);
  }
  } else {
  t263 = ((int64_t)(((*(v0))).f9));
  t264 = ((int64_t)(((int64_t)UINT64_C(1))));
  t265 = ((t263) == (t264));
  if (t265) {
  {
  t266 = v0;
  t267 = v1;
  t268 = v2;
  t269 = v3;
  t270 = v4;
  t271 = v5;
  t272 = v6;
  t273 = v7;
  t274 = v8;
  t275 = v9;
  t276 = ((int64_t)(v19));
  t277 = ((int64_t)(v21));
  t278 = ((int64_t)(v20));
  gg_function_15(t266, t267, t268, t269, t270, t271, t272, t273, t274, t275, t276, t277, t278, base + 0);
  }
  } else {
  t279 = ((int64_t)(((*(v0))).f9));
  t280 = ((int64_t)(((int64_t)UINT64_C(2))));
  t281 = ((t279) == (t280));
  if (t281) {
  {
  t282 = v0;
  t283 = v1;
  t284 = v2;
  t285 = v3;
  t286 = v4;
  t287 = v5;
  t288 = v6;
  t289 = v7;
  t290 = v8;
  t291 = v9;
  t292 = ((int64_t)(v19));
  t293 = ((int64_t)(v21));
  t294 = ((int64_t)(v20));
  gg_function_16(t282, t283, t284, t285, t286, t287, t288, t289, t290, t291, t292, t293, t294, base + 0);
  }
  } else {
  t295 = ((int64_t)(((*(v0))).f9));
  t296 = ((int64_t)(((int64_t)UINT64_C(3))));
  t297 = ((t295) == (t296));
  if (t297) {
  {
  t298 = v0;
  t299 = v1;
  t300 = v2;
  t301 = v3;
  t302 = v4;
  t303 = v5;
  t304 = v6;
  t305 = v7;
  t306 = v8;
  t307 = v9;
  t308 = ((int64_t)(v19));
  t309 = ((int64_t)(v21));
  t310 = ((int64_t)(v20));
  gg_function_17(t298, t299, t300, t301, t302, t303, t304, t305, t306, t307, t308, t309, t310, base + 0);
  }
  } else {
  t311 = ((int64_t)(((*(v0))).f9));
  t312 = ((int64_t)(((int64_t)UINT64_C(4))));
  t313 = ((t311) == (t312));
  if (t313) {
  {
  t314 = v0;
  t315 = v1;
  t316 = v2;
  t317 = v3;
  t318 = v4;
  t319 = v5;
  t320 = v6;
  t321 = v7;
  t322 = v8;
  t323 = v9;
  t324 = ((int64_t)(v19));
  t325 = ((int64_t)(v21));
  t326 = ((int64_t)(v20));
  gg_function_18(t314, t315, t316, t317, t318, t319, t320, t321, t322, t323, t324, t325, t326, base + 0);
  }
  } else {
  t327 = ((int64_t)(((*(v0))).f9));
  t328 = ((int64_t)(((int64_t)UINT64_C(5))));
  t329 = ((t327) == (t328));
  if (t329) {
  {
  t330 = v0;
  t331 = v1;
  t332 = v2;
  t333 = v3;
  t334 = v4;
  t335 = v5;
  t336 = v6;
  t337 = v7;
  t338 = v8;
  t339 = v9;
  t340 = ((int64_t)(v19));
  t341 = ((int64_t)(v21));
  t342 = ((int64_t)(v20));
  gg_function_20(t330, t331, t332, t333, t334, t335, t336, t337, t338, t339, t340, t341, t342, base + 0);
  }
  } else {
  t343 = ((int64_t)(((*(v0))).f9));
  t344 = ((int64_t)(((int64_t)UINT64_C(6))));
  t345 = ((t343) == (t344));
  if (t345) {
  {
  t346 = v0;
  t347 = v1;
  t348 = v2;
  t349 = v3;
  t350 = v4;
  t351 = v5;
  t352 = v6;
  t353 = v7;
  t354 = v8;
  t355 = v9;
  t356 = ((int64_t)(v19));
  t357 = ((int64_t)(v21));
  t358 = ((int64_t)(v20));
  gg_function_22(t346, t347, t348, t349, t350, t351, t352, t353, t354, t355, t356, t357, t358, base + 0);
  }
  } else {
  t359 = ((int64_t)(((*(v0))).f9));
  t360 = ((int64_t)(((int64_t)UINT64_C(7))));
  t361 = ((t359) == (t360));
  if (t361) {
  {
  t362 = v0;
  t363 = v1;
  t364 = v2;
  t365 = v3;
  t366 = v4;
  t367 = v5;
  t368 = v6;
  t369 = v7;
  t370 = v8;
  t371 = v9;
  t372 = ((int64_t)(v19));
  t373 = ((int64_t)(v21));
  t374 = ((int64_t)(v20));
  gg_function_23(t362, t363, t364, t365, t366, t367, t368, t369, t370, t371, t372, t373, t374, base + 0);
  }
  } else {
  t375 = ((int64_t)(((*(v0))).f9));
  t376 = ((int64_t)(((int64_t)UINT64_C(8))));
  t377 = ((t375) == (t376));
  if (t377) {
  {
  t378 = v0;
  t379 = v1;
  t380 = v2;
  t381 = v3;
  t382 = v4;
  t383 = v5;
  t384 = v6;
  t385 = v7;
  t386 = v8;
  t387 = v9;
  t388 = ((int64_t)(v19));
  t389 = ((int64_t)(v21));
  t390 = ((int64_t)(v20));
  gg_function_25(t378, t379, t380, t381, t382, t383, t384, t385, t386, t387, t388, t389, t390, base + 0);
  }
  } else {
  t391 = ((int64_t)(((*(v0))).f9));
  t392 = ((int64_t)(((int64_t)UINT64_C(9))));
  t393 = ((t391) == (t392));
  if (t393) {
  {
  t394 = v0;
  t395 = v1;
  t396 = v2;
  t397 = v3;
  t398 = v4;
  t399 = v5;
  t400 = v6;
  t401 = v7;
  t402 = v8;
  t403 = v9;
  t404 = ((int64_t)(v19));
  t405 = ((int64_t)(v21));
  t406 = ((int64_t)(v20));
  gg_function_28(t394, t395, t396, t397, t398, t399, t400, t401, t402, t403, t404, t405, t406, base + 0);
  }
  } else {
  t407 = ((int64_t)(((*(v0))).f9));
  t408 = ((int64_t)(((int64_t)UINT64_C(10))));
  t409 = ((t407) == (t408));
  if (t409) {
  {
  t410 = v0;
  t411 = v1;
  t412 = v2;
  t413 = v3;
  t414 = v4;
  t415 = v5;
  t416 = v6;
  t417 = v7;
  t418 = v8;
  t419 = v9;
  t420 = ((int64_t)(v19));
  t421 = ((int64_t)(v21));
  t422 = ((int64_t)(v20));
  gg_function_29(t410, t411, t412, t413, t414, t415, t416, t417, t418, t419, t420, t421, t422, base + 0);
  }
  } else {
  t423 = ((int64_t)(((*(v0))).f9));
  t424 = ((int64_t)(((int64_t)UINT64_C(11))));
  t425 = ((t423) == (t424));
  if (t425) {
  {
  t426 = v0;
  t427 = v1;
  t428 = v2;
  t429 = v3;
  t430 = v4;
  t431 = v5;
  t432 = v6;
  t433 = v7;
  t434 = v8;
  t435 = v9;
  t436 = ((int64_t)(v19));
  t437 = ((int64_t)(v21));
  t438 = ((int64_t)(v20));
  gg_function_30(t426, t427, t428, t429, t430, t431, t432, t433, t434, t435, t436, t437, t438, base + 0);
  }
  } else {
  t439 = ((int64_t)(((*(v0))).f9));
  t440 = ((int64_t)(((int64_t)UINT64_C(12))));
  t441 = ((t439) == (t440));
  if (t441) {
  {
  t442 = v0;
  t443 = v1;
  t444 = v2;
  t445 = v3;
  t446 = v4;
  t447 = v5;
  t448 = v6;
  t449 = v7;
  t450 = v8;
  t451 = v9;
  t452 = ((int64_t)(v19));
  t453 = ((int64_t)(v21));
  t454 = ((int64_t)(v20));
  gg_function_31(t442, t443, t444, t445, t446, t447, t448, t449, t450, t451, t452, t453, t454, base + 0);
  }
  } else {
  t455 = ((int64_t)(((*(v0))).f9));
  t456 = ((int64_t)(((int64_t)UINT64_C(13))));
  t457 = ((t455) == (t456));
  if (t457) {
  {
  t458 = v0;
  t459 = v1;
  t460 = v2;
  t461 = v3;
  t462 = v4;
  t463 = v5;
  t464 = v6;
  t465 = v7;
  t466 = v8;
  t467 = v9;
  t468 = ((int64_t)(v19));
  t469 = ((int64_t)(v21));
  t470 = ((int64_t)(v20));
  gg_function_33(t458, t459, t460, t461, t462, t463, t464, t465, t466, t467, t468, t469, t470, base + 0);
  }
  } else {
  t471 = ((int64_t)(((*(v0))).f9));
  t472 = ((int64_t)(((int64_t)UINT64_C(14))));
  t473 = ((t471) == (t472));
  if (t473) {
  {
  t474 = v0;
  t475 = v1;
  t476 = v2;
  t477 = v3;
  t478 = v4;
  t479 = v5;
  t480 = v6;
  t481 = v7;
  t482 = v8;
  t483 = v9;
  t484 = ((int64_t)(v19));
  t485 = ((int64_t)(v21));
  t486 = ((int64_t)(v20));
  gg_function_35(t474, t475, t476, t477, t478, t479, t480, t481, t482, t483, t484, t485, t486, base + 0);
  }
  } else {
  t487 = ((int64_t)(((*(v0))).f9));
  t488 = ((int64_t)(((int64_t)UINT64_C(15))));
  t489 = ((t487) == (t488));
  if (t489) {
  {
  t490 = v0;
  t491 = v1;
  t492 = v2;
  t493 = v3;
  t494 = v4;
  t495 = v5;
  t496 = v6;
  t497 = v7;
  t498 = v8;
  t499 = v9;
  t500 = ((int64_t)(v19));
  t501 = ((int64_t)(v21));
  t502 = ((int64_t)(v20));
  gg_function_37(t490, t491, t492, t493, t494, t495, t496, t497, t498, t499, t500, t501, t502, base + 0);
  }
  } else {
  t503 = ((int64_t)(((*(v0))).f9));
  t504 = ((int64_t)(((int64_t)UINT64_C(16))));
  t505 = ((t503) == (t504));
  if (t505) {
  {
  t506 = v0;
  t507 = v1;
  t508 = v2;
  t509 = v3;
  t510 = v4;
  t511 = v5;
  t512 = v6;
  t513 = v7;
  t514 = v8;
  t515 = v9;
  t516 = ((int64_t)(v19));
  t517 = ((int64_t)(v21));
  t518 = ((int64_t)(v20));
  gg_function_39(t506, t507, t508, t509, t510, t511, t512, t513, t514, t515, t516, t517, t518, base + 0);
  }
  } else {
  t519 = ((int64_t)(((*(v0))).f9));
  t520 = ((int64_t)(((int64_t)UINT64_C(17))));
  t521 = ((t519) == (t520));
  if (t521) {
  {
  t522 = v0;
  t523 = v1;
  t524 = v2;
  t525 = v3;
  t526 = v4;
  t527 = v5;
  t528 = v6;
  t529 = v7;
  t530 = v8;
  t531 = v9;
  t532 = ((int64_t)(v19));
  t533 = ((int64_t)(v21));
  t534 = ((int64_t)(v20));
  gg_function_40(t522, t523, t524, t525, t526, t527, t528, t529, t530, t531, t532, t533, t534, base + 0);
  }
  } else {
  t535 = ((int64_t)(((*(v0))).f9));
  t536 = ((int64_t)(((int64_t)UINT64_C(18))));
  t537 = ((t535) == (t536));
  if (t537) {
  {
  t538 = v0;
  t539 = v1;
  t540 = v2;
  t541 = v3;
  t542 = v4;
  t543 = v5;
  t544 = v6;
  t545 = v7;
  t546 = v8;
  t547 = v9;
  t548 = ((int64_t)(v19));
  t549 = ((int64_t)(v21));
  t550 = ((int64_t)(v20));
  gg_function_41(t538, t539, t540, t541, t542, t543, t544, t545, t546, t547, t548, t549, t550, base + 0);
  }
  } else {
  t551 = ((int64_t)(((*(v0))).f9));
  t552 = ((int64_t)(((int64_t)UINT64_C(19))));
  t553 = ((t551) == (t552));
  if (t553) {
  {
  t554 = v0;
  t555 = v1;
  t556 = v2;
  t557 = v3;
  t558 = v4;
  t559 = v5;
  t560 = v6;
  t561 = v7;
  t562 = v8;
  t563 = v9;
  t564 = ((int64_t)(v19));
  t565 = ((int64_t)(v21));
  t566 = ((int64_t)(v20));
  gg_function_43(t554, t555, t556, t557, t558, t559, t560, t561, t562, t563, t564, t565, t566, base + 0);
  }
  } else {
  t567 = ((int64_t)(((*(v0))).f9));
  t568 = ((int64_t)(((int64_t)UINT64_C(20))));
  t569 = ((t567) == (t568));
  if (t569) {
  {
  t570 = v0;
  t571 = v1;
  t572 = v2;
  t573 = v3;
  t574 = v4;
  t575 = v5;
  t576 = v6;
  t577 = v7;
  t578 = v8;
  t579 = v9;
  t580 = ((int64_t)(v19));
  t581 = ((int64_t)(v21));
  t582 = ((int64_t)(v20));
  gg_function_45(t570, t571, t572, t573, t574, t575, t576, t577, t578, t579, t580, t581, t582, base + 0);
  }
  } else {
  t583 = ((int64_t)(((*(v0))).f9));
  t584 = ((int64_t)(((int64_t)UINT64_C(21))));
  t585 = ((t583) == (t584));
  if (t585) {
  {
  t586 = v0;
  t587 = v1;
  t588 = v2;
  t589 = v3;
  t590 = v4;
  t591 = v5;
  t592 = v6;
  t593 = v7;
  t594 = v8;
  t595 = v9;
  t596 = ((int64_t)(v19));
  t597 = ((int64_t)(v21));
  t598 = ((int64_t)(v20));
  gg_function_47(t586, t587, t588, t589, t590, t591, t592, t593, t594, t595, t596, t597, t598, base + 0);
  }
  } else {
  t599 = ((int64_t)(((*(v0))).f9));
  t600 = ((int64_t)(((int64_t)UINT64_C(22))));
  t601 = ((t599) == (t600));
  if (t601) {
  {
  t602 = v0;
  t603 = v1;
  t604 = v2;
  t605 = v3;
  t606 = v4;
  t607 = v5;
  t608 = v6;
  t609 = v7;
  t610 = v8;
  t611 = v9;
  t612 = ((int64_t)(v19));
  t613 = ((int64_t)(v21));
  t614 = ((int64_t)(v20));
  gg_function_49(t602, t603, t604, t605, t606, t607, t608, t609, t610, t611, t612, t613, t614, base + 0);
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
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
  }
  } else {
  {
  t615 = ((int64_t)(((*(v0))).f9));
  t616 = ((int64_t)(((int64_t)UINT64_C(0))));
  t617 = ((t615) == (t616));
  if (t617) {
  {
  t618 = v0;
  t619 = v1;
  t620 = v2;
  t621 = v3;
  t622 = v4;
  t623 = v5;
  t624 = v6;
  t625 = v7;
  t626 = v8;
  t627 = v9;
  t628 = ((int64_t)(v19));
  t629 = ((int64_t)(v21));
  t630 = ((int64_t)(v20));
  gg_function_51(t618, t619, t620, t621, t622, t623, t624, t625, t626, t627, t628, t629, t630, base + 0);
  }
  } else {
  t631 = ((int64_t)(((*(v0))).f9));
  t632 = ((int64_t)(((int64_t)UINT64_C(1))));
  t633 = ((t631) == (t632));
  if (t633) {
  {
  t634 = v0;
  t635 = v1;
  t636 = v2;
  t637 = v3;
  t638 = v4;
  t639 = v5;
  t640 = v6;
  t641 = v7;
  t642 = v8;
  t643 = v9;
  t644 = ((int64_t)(v19));
  t645 = ((int64_t)(v21));
  t646 = ((int64_t)(v20));
  gg_function_52(t634, t635, t636, t637, t638, t639, t640, t641, t642, t643, t644, t645, t646, base + 0);
  }
  } else {
  t647 = ((int64_t)(((*(v0))).f9));
  t648 = ((int64_t)(((int64_t)UINT64_C(2))));
  t649 = ((t647) == (t648));
  if (t649) {
  {
  t650 = v0;
  t651 = v1;
  t652 = v2;
  t653 = v3;
  t654 = v4;
  t655 = v5;
  t656 = v6;
  t657 = v7;
  t658 = v8;
  t659 = v9;
  t660 = ((int64_t)(v19));
  t661 = ((int64_t)(v21));
  t662 = ((int64_t)(v20));
  gg_function_53(t650, t651, t652, t653, t654, t655, t656, t657, t658, t659, t660, t661, t662, base + 0);
  }
  } else {
  t663 = ((int64_t)(((*(v0))).f9));
  t664 = ((int64_t)(((int64_t)UINT64_C(3))));
  t665 = ((t663) == (t664));
  if (t665) {
  {
  t666 = v0;
  t667 = v1;
  t668 = v2;
  t669 = v3;
  t670 = v4;
  t671 = v5;
  t672 = v6;
  t673 = v7;
  t674 = v8;
  t675 = v9;
  t676 = ((int64_t)(v19));
  t677 = ((int64_t)(v21));
  t678 = ((int64_t)(v20));
  gg_function_54(t666, t667, t668, t669, t670, t671, t672, t673, t674, t675, t676, t677, t678, base + 0);
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  }
  }
  t679 = v14;
  /* FG_DIRECT_SLICE_INDEX */
  t680 = ((int64_t)UINT64_C(0));
  t681 = ((int64_t *)((((*(v0))).f3).f0).data) + gg_index(t680, ((int64_t)UINT64_C(4)));
  t682 = ((int64_t)((*t681)));
  v14 = ((int64_t)((uint64_t)(t679) + (uint64_t)(t682)));
  t683 = v15;
  t684 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t683) + (uint64_t)(t684)));
  }
  goto label1;
label2:;
  t685 = ((int64_t)(v14));
  t686 = ((int64_t)(((int64_t)UINT64_C(0))));
  t687 = ((t685) > (t686));
  if (t687) {
  {
  t688 = ((int64_t)(((*(v0))).f8));
  t689 = ((int64_t)(((*(v0))).f9));
  t690 = ((int64_t)(v14));
  fg_element_witness(t688, t689, t690);
  }
  }
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
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  float t25;
  float t26;
  int64_t t27;
  float t28;
  float t29;
  int64_t t30;
  uint16_t t31;
  uint16_t t32;
  int64_t t33;
  gg_type_10 t34;
  float t35;
  float t36;
  int64_t t37;
  gg_type_10 t38;
  int64_t t39;
  gg_type_12 t40;
  gg_slice t41;
  gg_slice t42;
  gg_slice t43;
  gg_slice t44;
  gg_slice t45;
  gg_slice t46;
  gg_slice t47;
  gg_slice t48;
  gg_slice t49;
  gg_type_1 v0;
  gg_type_7 v1;
  gg_type_9 v2;
  gg_type_11 v3;
  {
  memset(&(v0), 0, sizeof(v0));
  memset(&((v0).f3), 0, sizeof((v0).f3));
  memset(&t0, 0, sizeof(t0));
  t1 = ((int64_t)(t0));
  for (t2=0; t2<4; t2++) (((v0).f3).f0).data[t2] = t1;
  memset(&t3, 0, sizeof(t3));
  t4 = ((int64_t)(t3));
  for (t5=0; t5<4; t5++) (((v0).f3).f1).data[t5] = t4;
  memset(&t6, 0, sizeof(t6));
  ((v0).f3).f2 = ((int64_t)(t6));
  memset(&((v0).f4), 0, sizeof((v0).f4));
  memset(&t7, 0, sizeof(t7));
  t8 = ((int64_t)(t7));
  for (t9=0; t9<4; t9++) (((v0).f4).f0).data[t9] = t8;
  memset(&t10, 0, sizeof(t10));
  t11 = ((int64_t)(t10));
  for (t12=0; t12<4; t12++) (((v0).f4).f1).data[t12] = t11;
  memset(&t13, 0, sizeof(t13));
  ((v0).f4).f2 = ((int64_t)(t13));
  memset(&((v0).f5), 0, sizeof((v0).f5));
  memset(&t14, 0, sizeof(t14));
  t15 = ((int64_t)(t14));
  for (t16=0; t16<4; t16++) (((v0).f5).f0).data[t16] = t15;
  memset(&t17, 0, sizeof(t17));
  t18 = ((int64_t)(t17));
  for (t19=0; t19<4; t19++) (((v0).f5).f1).data[t19] = t18;
  memset(&t20, 0, sizeof(t20));
  ((v0).f5).f2 = ((int64_t)(t20));
  memset(&t21, 0, sizeof(t21));
  (v0).f6 = ((int64_t)(t21));
  memset(&t22, 0, sizeof(t22));
  (v0).f7 = ((int64_t)(t22));
  memset(&t23, 0, sizeof(t23));
  (v0).f8 = ((int64_t)(t23));
  memset(&t24, 0, sizeof(t24));
  (v0).f9 = ((int64_t)(t24));
  memset(&t25, 0, sizeof(t25));
  t26 = ((float)(t25));
  for (t27=0; t27<4; t27++) ((v0).f10).data[t27] = t26;
  memset(&t28, 0, sizeof(t28));
  t29 = ((float)(t28));
  for (t30=0; t30<1; t30++) (v1).data[t30] = t29;
  memset(&t31, 0, sizeof(t31));
  t32 = ((uint16_t)(t31));
  for (t33=0; t33<1; t33++) (v2).data[t33] = t32;
  memset(&t35, 0, sizeof(t35));
  t36 = ((float)(t35));
  for (t37=0; t37<8; t37++) (t34).data[t37] = t36;
  t38 = t34;
  for (t39=0; t39<1; t39++) (v3).data[t39] = t38;
  t40 = &(v0);
  t41 = gg_view((v1).data, 1);
  t42 = gg_view((v2).data, 1);
  t43 = gg_view((v1).data, 1);
  t44 = gg_view((v2).data, 1);
  t45 = gg_view((v1).data, 1);
  t46 = gg_view((v2).data, 1);
  t47 = gg_view((v3).data, 1);
  t48 = gg_view((v3).data, 1);
  t49 = gg_view((v3).data, 1);
  gg_function_1(t40, t41, t42, t43, t44, t45, t46, t47, t48, t49, base + 0);
  }
gg_done:;
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
