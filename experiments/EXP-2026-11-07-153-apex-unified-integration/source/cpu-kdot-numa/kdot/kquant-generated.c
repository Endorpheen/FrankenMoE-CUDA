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
typedef struct { uint8_t data[1]; } gg_type_2;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_7;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_8;
#pragma pack(pop)
typedef struct gg_type_12 gg_type_12;
#pragma pack(push, 1)
typedef struct { uint8_t data[8]; } gg_type_13;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_12 {
  gg_type_13 f0;
  gg_type_13 f1;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint32_t data[4]; } gg_type_14;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[256]; } gg_type_16;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_17;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[8]; } gg_type_19;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, uint8_t value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(int64_t a, int64_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, int64_t value);
static uint8_t gg_eq_4(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_slice value);
static uint8_t gg_eq_5(float a, float b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, float value);
static uint8_t gg_eq_6(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint8_t value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, uint32_t value);
static uint8_t gg_eq_11(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, uint16_t value);
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_type_12 value);
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_type_13 value);
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, gg_type_14 value);
static uint8_t gg_eq_15(int32_t a, int32_t b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, int32_t value);
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, gg_type_16 value);
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_type_17 value);
static uint8_t gg_eq_18(int16_t a, int16_t b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, int16_t value);
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b);
static uint8_t gg_equal_elements_19(gg_slice a, gg_slice b);
static void gg_render_19(gg_reference out, gg_type_19 value);
static uint8_t gg_eq_20(int8_t a, int8_t b);
static uint8_t gg_equal_elements_20(gg_slice a, gg_slice b);
static void gg_render_20(gg_reference out, int8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
return gg_equal_elements_1(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
gg_text(out,gg_view((value).data, 1));
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_4(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(float a, float b) {
return a == b;
}
static void gg_render_5(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_5(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
return gg_equal_elements_5(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_9(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_11(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
if(!gg_eq_13(a.f0,b.f0))return 0;
if(!gg_eq_13(a.f1,b.f1))return 0;
return 1;
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
gg_append(out,"\121\064\123\143\141\154\145\163\040\173\040",11,1);
gg_quote(out, gg_view((value.f0).data, 8));
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f1).data, 8));
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
return gg_equal_elements_1(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
gg_text(out,gg_view((value).data, 8));
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_type_14*)a.data)[i],((gg_type_14*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b) {
return gg_equal_elements_9(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_14(gg_reference out, gg_type_14 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_9(out, ((uint32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_15(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((gg_type_16*)a.data)[i],((gg_type_16*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b) {
return gg_equal_elements_15(gg_view((a).data, 256),gg_view((b).data, 256));
}
static void gg_render_16(gg_reference out, gg_type_16 value) {
int64_t i; gg_slice view=gg_view((value).data, 256);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_15(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_type_17*)a.data)[i],((gg_type_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b) {
return gg_equal_elements_15(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_17(gg_reference out, gg_type_17 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_15(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((int16_t*)a.data)[i],((int16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(int16_t a, int16_t b) {
return a == b;
}
static void gg_render_18(gg_reference out, int16_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_19(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_19(((gg_type_19*)a.data)[i],((gg_type_19*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_19(gg_type_19 a, gg_type_19 b) {
return gg_equal_elements_15(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_19(gg_reference out, gg_type_19 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_15(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_20(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_20(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_20(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_20(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static float gg_function_1(int64_t p0, int64_t p1, gg_slice p2, gg_slice p3, int64_t base);
static float gg_function_2(int64_t p0, gg_slice p1, gg_slice p2, int64_t base);
static uint32_t gg_function_3(gg_slice p0, int64_t p1, int64_t base);
static uint16_t gg_function_5(gg_slice p0, int64_t p1, int64_t base);
static gg_type_12 gg_function_7(gg_slice p0, int64_t p1, int64_t base);
static float gg_function_10(gg_type_7 p0, int64_t base);
static float gg_function_11(int64_t p0, gg_slice p1, gg_slice p2, int64_t base);
static float gg_function_10(gg_type_7 p0, int64_t base) {
  float t0;
  int64_t t1;
  float * t2;
  float t3;
  int64_t t4;
  float * t5;
  float t6;
  int64_t t7;
  float * t8;
  float t9;
  int64_t t10;
  float * t11;
  float t12;
  int64_t t13;
  float * t14;
  float t15;
  int64_t t16;
  float * t17;
  float t18;
  int64_t t19;
  float * t20;
  float t21;
  int64_t t22;
  float * t23;
  float t24;
  float t25;
  float t26;
  float t27;
  float t28;
  float t29;
  float t30;
  gg_type_7 v0;
  float v1;
  float v2;
  float v3;
  float v4;
  v0 = p0;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t1 = ((int64_t)UINT64_C(0));
  t2 = ((float *)(v0).data) + gg_index(t1, ((int64_t)UINT64_C(8)));
  t3 = ((float)((*t2)));
  /* FG_DIRECT_SLICE_INDEX */
  t4 = ((int64_t)UINT64_C(4));
  t5 = ((float *)(v0).data) + gg_index(t4, ((int64_t)UINT64_C(8)));
  t6 = ((float)((*t5)));
  v1 = ((float)(((t3) + (t6))));
  /* FG_DIRECT_SLICE_INDEX */
  t7 = ((int64_t)UINT64_C(1));
  t8 = ((float *)(v0).data) + gg_index(t7, ((int64_t)UINT64_C(8)));
  t9 = ((float)((*t8)));
  /* FG_DIRECT_SLICE_INDEX */
  t10 = ((int64_t)UINT64_C(5));
  t11 = ((float *)(v0).data) + gg_index(t10, ((int64_t)UINT64_C(8)));
  t12 = ((float)((*t11)));
  v2 = ((float)(((t9) + (t12))));
  /* FG_DIRECT_SLICE_INDEX */
  t13 = ((int64_t)UINT64_C(2));
  t14 = ((float *)(v0).data) + gg_index(t13, ((int64_t)UINT64_C(8)));
  t15 = ((float)((*t14)));
  /* FG_DIRECT_SLICE_INDEX */
  t16 = ((int64_t)UINT64_C(6));
  t17 = ((float *)(v0).data) + gg_index(t16, ((int64_t)UINT64_C(8)));
  t18 = ((float)((*t17)));
  v3 = ((float)(((t15) + (t18))));
  /* FG_DIRECT_SLICE_INDEX */
  t19 = ((int64_t)UINT64_C(3));
  t20 = ((float *)(v0).data) + gg_index(t19, ((int64_t)UINT64_C(8)));
  t21 = ((float)((*t20)));
  /* FG_DIRECT_SLICE_INDEX */
  t22 = ((int64_t)UINT64_C(7));
  t23 = ((float *)(v0).data) + gg_index(t22, ((int64_t)UINT64_C(8)));
  t24 = ((float)((*t23)));
  v4 = ((float)(((t21) + (t24))));
  t25 = ((float)(v1));
  t26 = ((float)(v3));
  t27 = ((float)(((t25) + (t26))));
  t28 = ((float)(v2));
  t29 = ((float)(v4));
  t30 = ((float)(((t28) + (t29))));
  t0 = ((float)(((t27) + (t30))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint16_t gg_function_5(gg_slice p0, int64_t p1, int64_t base) {
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
static uint32_t gg_function_3(gg_slice p0, int64_t p1, int64_t base) {
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
static float gg_function_11(int64_t p0, gg_slice p1, gg_slice p2, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  float t11;
  float t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  gg_slice t20;
  int64_t t21;
  uint32_t t22;
  float t23;
  gg_slice t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint16_t t28;
  float t29;
  float t30;
  float t31;
  int32_t t32;
  int32_t t33;
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
  gg_slice t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  uint8_t * t53;
  gg_slice t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int32_t * t58;
  gg_slice t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t * t65;
  uint8_t t66;
  uint8_t t67;
  int32_t t68;
  uint8_t t69;
  int64_t t70;
  uint8_t t71;
  uint8_t t72;
  int32_t t73;
  int64_t t74;
  int32_t t75;
  gg_slice t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int32_t * t82;
  gg_slice t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint8_t * t91;
  uint8_t t92;
  uint8_t t93;
  int32_t t94;
  uint8_t t95;
  int64_t t96;
  uint8_t t97;
  uint8_t t98;
  int32_t t99;
  int64_t t100;
  int32_t t101;
  gg_slice t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int32_t * t108;
  gg_slice t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  uint8_t * t115;
  uint8_t t116;
  int64_t t117;
  int32_t t118;
  uint8_t t119;
  int64_t t120;
  uint8_t t121;
  uint8_t t122;
  int32_t t123;
  int64_t t124;
  int32_t t125;
  gg_slice t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int32_t * t132;
  gg_slice t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  uint8_t * t141;
  uint8_t t142;
  int64_t t143;
  int32_t t144;
  uint8_t t145;
  int64_t t146;
  uint8_t t147;
  uint8_t t148;
  int32_t t149;
  int64_t t150;
  int32_t t151;
  int32_t t152;
  int32_t t153;
  int64_t t154;
  int64_t t155;
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
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  uint8_t * t171;
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
  int32_t t188;
  gg_slice t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  uint8_t * t195;
  int32_t t196;
  int64_t t197;
  int32_t * t198;
  int32_t t199;
  int32_t t200;
  int64_t t201;
  int32_t * t202;
  int32_t t203;
  int32_t t204;
  int32_t t205;
  int32_t t206;
  int64_t t207;
  int64_t t208;
  gg_slice t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  uint16_t t219;
  gg_slice t220;
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
  uint16_t t232;
  gg_slice t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  uint8_t * t241;
  gg_slice t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  uint8_t * t252;
  int64_t t253;
  int32_t * t254;
  int32_t t255;
  int32_t t256;
  int32_t t257;
  int32_t t258;
  int32_t t259;
  int32_t t260;
  int32_t t261;
  int32_t t262;
  int32_t t263;
  int32_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  float * t268;
  float t269;
  int64_t t270;
  int32_t * t271;
  float t272;
  int64_t t273;
  float * t274;
  float t275;
  float t276;
  int64_t t277;
  int64_t t278;
  gg_type_7 t279;
  int64_t v0;
  gg_slice v1;
  gg_slice v2;
  int64_t v3;
  gg_type_7 v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  float v8;
  float v9;
  gg_type_16 v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  uint8_t v16;
  gg_type_19 v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int32_t v21;
  int32_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int32_t v26;
  int32_t v27;
  int32_t v28;
  int32_t v29;
  int64_t v30;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(256))));
  t7 = ((int64_t)(((int64_t)gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t7) == (t8));
  }
  if (!(t4)) gg_fail("assertion failed");
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(256))));
  v3 = ((int64_t)(((int64_t)gg_div(t9, t10))));
  memset(&t11, 0, sizeof(t11));
  t12 = ((float)(t11));
  for (t13=0; t13<8; t13++) (v4).data[t13] = t12;
  t15 = v3;
  t14 = 0;
label0:;
  if (t14 >= t15) goto label2;
  v5 = t14;
  {
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(((int64_t)UINT64_C(210))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t18 = ((int64_t)(v5));
  t19 = ((int64_t)(((int64_t)UINT64_C(292))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t20 = v2;
  t21 = ((int64_t)(v7));
  t22 = gg_function_3(t20, t21, base + 0);
  t23 = fg143_from_bits(t22);
  v8 = ((float)(t23));
  t24 = v1;
  t25 = ((int64_t)(v6));
  t26 = ((int64_t)(((int64_t)UINT64_C(208))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  t28 = gg_function_5(t24, t27, base + 0);
  t29 = fg143_half(t28);
  t30 = ((float)(t29));
  t31 = ((float)(v8));
  v9 = ((float)(((t30) * (t31))));
  memset(&t32, 0, sizeof(t32));
  t33 = ((int32_t)(t32));
  for (t34=0; t34<256; t34++) (v10).data[t34] = t33;
  t36 = ((int64_t)UINT64_C(2));
  t35 = 0;
label3:;
  if (t35 >= t36) goto label5;
  v11 = t35;
  {
  t37 = ((int64_t)(v11));
  t38 = ((int64_t)(((int64_t)UINT64_C(64))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t39 = ((int64_t)(v11));
  t40 = ((int64_t)(((int64_t)UINT64_C(32))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t41 = ((int64_t)(v11));
  t42 = ((int64_t)(((int64_t)UINT64_C(128))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t44 = ((int64_t)UINT64_C(32));
  t43 = 0;
label6:;
  if (t43 >= t44) goto label8;
  v15 = t43;
  {
  t45 = v1;
  t46 = ((int64_t)(v6));
  t47 = ((int64_t)(((int64_t)UINT64_C(128))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  t49 = ((int64_t)(v13));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  t51 = ((int64_t)(v15));
  t52 = ((int64_t)((uint64_t)(t50) + (uint64_t)(t51)));
  t53 = ((uint8_t *)t45.data) + gg_index(t52, t45.len);
  v16 = ((uint8_t)((*t53)));
  t54 = gg_view((v10).data, 256);
  t55 = ((int64_t)(v14));
  t56 = ((int64_t)(v15));
  t57 = ((int64_t)((uint64_t)(t55) + (uint64_t)(t56)));
  t58 = ((int32_t *)t54.data) + gg_index(t57, t54.len);
  t59 = v1;
  t60 = ((int64_t)(v6));
  t61 = ((int64_t)(v12));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  t63 = ((int64_t)(v15));
  t64 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  t65 = ((uint8_t *)t59.data) + gg_index(t64, t59.len);
  t66 = ((uint8_t)((*t65)));
  t67 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t68 = ((int32_t)(((int32_t)(((t66) & (t67))))));
  t69 = ((uint8_t)(v16));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((uint8_t)(((t69) >> ((uint64_t)(t70) & 7))));
  t72 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t73 = ((int32_t)(((int32_t)(((t71) & (t72))))));
  t74 = ((int64_t)(((int64_t)UINT64_C(4))));
  t75 = ((int32_t)(((int32_t)((uint64_t)(t73) << ((uint64_t)(t74) & 31)))));
  (*t58) = ((int32_t)(((t68) | (t75))));
  t76 = gg_view((v10).data, 256);
  t77 = ((int64_t)(v14));
  t78 = ((int64_t)(((int64_t)UINT64_C(32))));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  t80 = ((int64_t)(v15));
  t81 = ((int64_t)((uint64_t)(t79) + (uint64_t)(t80)));
  t82 = ((int32_t *)t76.data) + gg_index(t81, t76.len);
  t83 = v1;
  t84 = ((int64_t)(v6));
  t85 = ((int64_t)(v12));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t85)))));
  t87 = ((int64_t)(((int64_t)UINT64_C(32))));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))));
  t89 = ((int64_t)(v15));
  t90 = ((int64_t)((uint64_t)(t88) + (uint64_t)(t89)));
  t91 = ((uint8_t *)t83.data) + gg_index(t90, t83.len);
  t92 = ((uint8_t)((*t91)));
  t93 = ((uint8_t)(((int64_t)UINT64_C(15))));
  t94 = ((int32_t)(((int32_t)(((t92) & (t93))))));
  t95 = ((uint8_t)(v16));
  t96 = ((int64_t)(((int64_t)UINT64_C(2))));
  t97 = ((uint8_t)(((t95) >> ((uint64_t)(t96) & 7))));
  t98 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t99 = ((int32_t)(((int32_t)(((t97) & (t98))))));
  t100 = ((int64_t)(((int64_t)UINT64_C(4))));
  t101 = ((int32_t)(((int32_t)((uint64_t)(t99) << ((uint64_t)(t100) & 31)))));
  (*t82) = ((int32_t)(((t94) | (t101))));
  t102 = gg_view((v10).data, 256);
  t103 = ((int64_t)(v14));
  t104 = ((int64_t)(((int64_t)UINT64_C(64))));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t103) + (uint64_t)(t104)))));
  t106 = ((int64_t)(v15));
  t107 = ((int64_t)((uint64_t)(t105) + (uint64_t)(t106)));
  t108 = ((int32_t *)t102.data) + gg_index(t107, t102.len);
  t109 = v1;
  t110 = ((int64_t)(v6));
  t111 = ((int64_t)(v12));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  t113 = ((int64_t)(v15));
  t114 = ((int64_t)((uint64_t)(t112) + (uint64_t)(t113)));
  t115 = ((uint8_t *)t109.data) + gg_index(t114, t109.len);
  t116 = ((uint8_t)((*t115)));
  t117 = ((int64_t)(((int64_t)UINT64_C(4))));
  t118 = ((int32_t)(((int32_t)(((t116) >> ((uint64_t)(t117) & 7))))));
  t119 = ((uint8_t)(v16));
  t120 = ((int64_t)(((int64_t)UINT64_C(4))));
  t121 = ((uint8_t)(((t119) >> ((uint64_t)(t120) & 7))));
  t122 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t123 = ((int32_t)(((int32_t)(((t121) & (t122))))));
  t124 = ((int64_t)(((int64_t)UINT64_C(4))));
  t125 = ((int32_t)(((int32_t)((uint64_t)(t123) << ((uint64_t)(t124) & 31)))));
  (*t108) = ((int32_t)(((t118) | (t125))));
  t126 = gg_view((v10).data, 256);
  t127 = ((int64_t)(v14));
  t128 = ((int64_t)(((int64_t)UINT64_C(96))));
  t129 = ((int64_t)(((int64_t)((uint64_t)(t127) + (uint64_t)(t128)))));
  t130 = ((int64_t)(v15));
  t131 = ((int64_t)((uint64_t)(t129) + (uint64_t)(t130)));
  t132 = ((int32_t *)t126.data) + gg_index(t131, t126.len);
  t133 = v1;
  t134 = ((int64_t)(v6));
  t135 = ((int64_t)(v12));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t134) + (uint64_t)(t135)))));
  t137 = ((int64_t)(((int64_t)UINT64_C(32))));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t136) + (uint64_t)(t137)))));
  t139 = ((int64_t)(v15));
  t140 = ((int64_t)((uint64_t)(t138) + (uint64_t)(t139)));
  t141 = ((uint8_t *)t133.data) + gg_index(t140, t133.len);
  t142 = ((uint8_t)((*t141)));
  t143 = ((int64_t)(((int64_t)UINT64_C(4))));
  t144 = ((int32_t)(((int32_t)(((t142) >> ((uint64_t)(t143) & 7))))));
  t145 = ((uint8_t)(v16));
  t146 = ((int64_t)(((int64_t)UINT64_C(6))));
  t147 = ((uint8_t)(((t145) >> ((uint64_t)(t146) & 7))));
  t148 = ((uint8_t)(((int64_t)UINT64_C(3))));
  t149 = ((int32_t)(((int32_t)(((t147) & (t148))))));
  t150 = ((int64_t)(((int64_t)UINT64_C(4))));
  t151 = ((int32_t)(((int32_t)((uint64_t)(t149) << ((uint64_t)(t150) & 31)))));
  (*t132) = ((int32_t)(((t144) | (t151))));
  }
label7:;
  t43++; goto label6;
label8:;
  }
label4:;
  t35++; goto label3;
label5:;
  memset(&t152, 0, sizeof(t152));
  t153 = ((int32_t)(t152));
  for (t154=0; t154<8; t154++) (v17).data[t154] = t153;
  t156 = ((int64_t)UINT64_C(8));
  t155 = 0;
label9:;
  if (t155 >= t156) goto label11;
  v18 = t155;
  {
  t158 = ((int64_t)UINT64_C(8));
  t157 = 0;
label12:;
  if (t157 >= t158) goto label14;
  v19 = t157;
  {
  t159 = ((int64_t)(v19));
  t160 = ((int64_t)(((int64_t)UINT64_C(4))));
  v20 = ((int64_t)(((int64_t)gg_div(t159, t160))));
  t161 = v1;
  t162 = ((int64_t)(v6));
  t163 = ((int64_t)(((int64_t)UINT64_C(192))));
  t164 = ((int64_t)(((int64_t)((uint64_t)(t162) + (uint64_t)(t163)))));
  t165 = ((int64_t)(((int64_t)UINT64_C(2))));
  t166 = ((int64_t)(v18));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t165) * (uint64_t)(t166)))));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t167)))));
  t169 = ((int64_t)(v20));
  t170 = ((int64_t)((uint64_t)(t168) + (uint64_t)(t169)));
  t171 = ((uint8_t *)t161.data) + gg_index(t170, t161.len);
  v21 = ((int32_t)(((int32_t)(((int8_t)((*t171)))))));
  v22 = ((int32_t)(((int64_t)UINT64_C(0))));
  t173 = ((int64_t)UINT64_C(4));
  t172 = 0;
label15:;
  if (t172 >= t173) goto label17;
  v23 = t172;
  {
  t174 = ((int64_t)(v18));
  t175 = ((int64_t)(((int64_t)UINT64_C(32))));
  t176 = ((int64_t)(((int64_t)((uint64_t)(t174) * (uint64_t)(t175)))));
  t177 = ((int64_t)(v20));
  t178 = ((int64_t)(((int64_t)UINT64_C(16))));
  t179 = ((int64_t)(((int64_t)((uint64_t)(t177) * (uint64_t)(t178)))));
  t180 = ((int64_t)(((int64_t)((uint64_t)(t176) + (uint64_t)(t179)))));
  t181 = ((int64_t)(((int64_t)UINT64_C(4))));
  t182 = ((int64_t)(v19));
  t183 = ((int64_t)(((int64_t)UINT64_C(4))));
  t184 = ((int64_t)(((int64_t)gg_mod(t182, t183))));
  t185 = ((int64_t)(((int64_t)((uint64_t)(t181) * (uint64_t)(t184)))));
  t186 = ((int64_t)(((int64_t)((uint64_t)(t180) + (uint64_t)(t185)))));
  t187 = ((int64_t)(v23));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t186) + (uint64_t)(t187)))));
  t188 = v22;
  t189 = v2;
  t190 = ((int64_t)(v7));
  t191 = ((int64_t)(((int64_t)UINT64_C(4))));
  t192 = ((int64_t)(((int64_t)((uint64_t)(t190) + (uint64_t)(t191)))));
  t193 = ((int64_t)(v24));
  t194 = ((int64_t)((uint64_t)(t192) + (uint64_t)(t193)));
  t195 = ((uint8_t *)t189.data) + gg_index(t194, t189.len);
  t196 = ((int32_t)(((int32_t)(((int8_t)((*t195)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t197 = v24;
  t198 = ((int32_t *)(v10).data) + gg_index(t197, ((int64_t)UINT64_C(256)));
  t199 = ((int32_t)((*t198)));
  t200 = ((int32_t)(((int32_t)((uint64_t)(t196) * (uint64_t)(t199)))));
  v22 = ((int32_t)((uint64_t)(t188) + (uint64_t)(t200)));
  }
label16:;
  t172++; goto label15;
label17:;
  /* FG_DIRECT_SLICE_INDEX */
  t201 = v19;
  t202 = ((int32_t *)(v17).data) + gg_index(t201, ((int64_t)UINT64_C(8)));
  t203 = (*t202);
  t204 = ((int32_t)(v21));
  t205 = ((int32_t)(v22));
  t206 = ((int32_t)(((int32_t)((uint64_t)(t204) * (uint64_t)(t205)))));
  (*t202) = ((int32_t)((uint64_t)(t203) + (uint64_t)(t206)));
  }
label13:;
  t157++; goto label12;
label14:;
  }
label10:;
  t155++; goto label9;
label11:;
  t208 = ((int64_t)UINT64_C(8));
  t207 = 0;
label18:;
  if (t207 >= t208) goto label20;
  v25 = t207;
  {
  t209 = v2;
  t210 = ((int64_t)(v7));
  t211 = ((int64_t)(((int64_t)UINT64_C(260))));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t210) + (uint64_t)(t211)))));
  t213 = ((int64_t)(((int64_t)UINT64_C(2))));
  t214 = ((int64_t)(((int64_t)UINT64_C(2))));
  t215 = ((int64_t)(v25));
  t216 = ((int64_t)(((int64_t)((uint64_t)(t214) * (uint64_t)(t215)))));
  t217 = ((int64_t)(((int64_t)((uint64_t)(t213) * (uint64_t)(t216)))));
  t218 = ((int64_t)(((int64_t)((uint64_t)(t212) + (uint64_t)(t217)))));
  t219 = gg_function_5(t209, t218, base + 0);
  v26 = ((int32_t)(((int32_t)(((int16_t)(t219))))));
  t220 = v2;
  t221 = ((int64_t)(v7));
  t222 = ((int64_t)(((int64_t)UINT64_C(260))));
  t223 = ((int64_t)(((int64_t)((uint64_t)(t221) + (uint64_t)(t222)))));
  t224 = ((int64_t)(((int64_t)UINT64_C(2))));
  t225 = ((int64_t)(((int64_t)UINT64_C(2))));
  t226 = ((int64_t)(v25));
  t227 = ((int64_t)(((int64_t)((uint64_t)(t225) * (uint64_t)(t226)))));
  t228 = ((int64_t)(((int64_t)UINT64_C(1))));
  t229 = ((int64_t)(((int64_t)((uint64_t)(t227) + (uint64_t)(t228)))));
  t230 = ((int64_t)(((int64_t)((uint64_t)(t224) * (uint64_t)(t229)))));
  t231 = ((int64_t)(((int64_t)((uint64_t)(t223) + (uint64_t)(t230)))));
  t232 = gg_function_5(t220, t231, base + 0);
  v27 = ((int32_t)(((int32_t)(((int16_t)(t232))))));
  t233 = v1;
  t234 = ((int64_t)(v6));
  t235 = ((int64_t)(((int64_t)UINT64_C(192))));
  t236 = ((int64_t)(((int64_t)((uint64_t)(t234) + (uint64_t)(t235)))));
  t237 = ((int64_t)(((int64_t)UINT64_C(2))));
  t238 = ((int64_t)(v25));
  t239 = ((int64_t)(((int64_t)((uint64_t)(t237) * (uint64_t)(t238)))));
  t240 = ((int64_t)((uint64_t)(t236) + (uint64_t)(t239)));
  t241 = ((uint8_t *)t233.data) + gg_index(t240, t233.len);
  v28 = ((int32_t)(((int32_t)(((int8_t)((*t241)))))));
  t242 = v1;
  t243 = ((int64_t)(v6));
  t244 = ((int64_t)(((int64_t)UINT64_C(192))));
  t245 = ((int64_t)(((int64_t)((uint64_t)(t243) + (uint64_t)(t244)))));
  t246 = ((int64_t)(((int64_t)UINT64_C(2))));
  t247 = ((int64_t)(v25));
  t248 = ((int64_t)(((int64_t)((uint64_t)(t246) * (uint64_t)(t247)))));
  t249 = ((int64_t)(((int64_t)((uint64_t)(t245) + (uint64_t)(t248)))));
  t250 = ((int64_t)(((int64_t)UINT64_C(1))));
  t251 = ((int64_t)((uint64_t)(t249) + (uint64_t)(t250)));
  t252 = ((uint8_t *)t242.data) + gg_index(t251, t242.len);
  v29 = ((int32_t)(((int32_t)(((int8_t)((*t252)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t253 = v25;
  t254 = ((int32_t *)(v17).data) + gg_index(t253, ((int64_t)UINT64_C(8)));
  t255 = (*t254);
  t256 = ((int32_t)(((int64_t)UINT64_C(32))));
  t257 = ((int32_t)(v26));
  t258 = ((int32_t)(v28));
  t259 = ((int32_t)(((int32_t)((uint64_t)(t257) * (uint64_t)(t258)))));
  t260 = ((int32_t)(v27));
  t261 = ((int32_t)(v29));
  t262 = ((int32_t)(((int32_t)((uint64_t)(t260) * (uint64_t)(t261)))));
  t263 = ((int32_t)(((int32_t)((uint64_t)(t259) + (uint64_t)(t262)))));
  t264 = ((int32_t)(((int32_t)((uint64_t)(t256) * (uint64_t)(t263)))));
  (*t254) = ((int32_t)((uint64_t)(t255) - (uint64_t)(t264)));
  }
label19:;
  t207++; goto label18;
label20:;
  t266 = ((int64_t)UINT64_C(8));
  t265 = 0;
label21:;
  if (t265 >= t266) goto label23;
  v30 = t265;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t267 = v30;
  t268 = ((float *)(v4).data) + gg_index(t267, ((int64_t)UINT64_C(8)));
  t269 = ((float)(v9));
  /* FG_DIRECT_SLICE_INDEX */
  t270 = v30;
  t271 = ((int32_t *)(v17).data) + gg_index(t270, ((int64_t)UINT64_C(8)));
  t272 = ((float)(((float)((*t271)))));
  /* FG_DIRECT_SLICE_INDEX */
  t273 = v30;
  t274 = ((float *)(v4).data) + gg_index(t273, ((int64_t)UINT64_C(8)));
  t275 = ((float)((*t274)));
  t276 = fmaf(t269, t272, t275);
  (*t268) = ((float)(t276));
  }
label22:;
  t265++; goto label21;
label23:;
  }
label1:;
  t14++; goto label0;
label2:;
  t277 = ((int64_t)(((int64_t)UINT64_C(1))));
  t278 = ((int64_t)(v0));
  fg143_witness(t277, t278);
  t279 = v4;
  t0 = gg_function_10(t279, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_12 gg_function_7(gg_slice p0, int64_t p1, int64_t base) {
  gg_type_12 t0;
  uint32_t t1;
  uint32_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint32_t * t7;
  gg_slice t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint32_t * t15;
  int64_t t16;
  uint32_t * t17;
  uint32_t t18;
  int64_t t19;
  uint32_t t20;
  uint32_t t21;
  uint32_t t22;
  int64_t t23;
  uint32_t * t24;
  uint32_t t25;
  int64_t t26;
  uint32_t t27;
  uint32_t t28;
  uint32_t t29;
  int64_t t30;
  uint32_t t31;
  int64_t t32;
  uint32_t * t33;
  uint32_t t34;
  uint32_t t35;
  int64_t t36;
  uint32_t * t37;
  int64_t t38;
  uint32_t * t39;
  uint32_t t40;
  uint32_t t41;
  uint32_t t42;
  int64_t t43;
  uint32_t * t44;
  uint32_t t45;
  int64_t t46;
  uint32_t t47;
  uint32_t t48;
  uint32_t t49;
  int64_t t50;
  uint32_t t51;
  int64_t t52;
  uint32_t * t53;
  int64_t t54;
  uint32_t * t55;
  int64_t t56;
  uint32_t * t57;
  uint32_t t58;
  uint32_t t59;
  uint8_t t60;
  uint8_t t61;
  int64_t t62;
  uint8_t t63;
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t * t69;
  int64_t t70;
  uint32_t * t71;
  uint32_t t72;
  int64_t t73;
  int64_t t74;
  uint32_t t75;
  gg_slice t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint8_t * t80;
  int64_t t81;
  uint32_t * t82;
  uint32_t t83;
  int64_t t84;
  int64_t t85;
  uint32_t t86;
  int64_t t87;
  uint8_t * t88;
  int64_t t89;
  uint32_t * t90;
  uint32_t t91;
  int64_t t92;
  int64_t t93;
  uint32_t t94;
  gg_slice t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint8_t * t99;
  int64_t t100;
  uint32_t * t101;
  uint32_t t102;
  int64_t t103;
  int64_t t104;
  uint32_t t105;
  gg_slice v0;
  int64_t v1;
  gg_type_14 v2;
  int64_t v3;
  uint32_t v4;
  uint32_t v5;
  uint32_t v6;
  uint32_t v7;
  gg_type_12 v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  {
  memset(&t1, 0, sizeof(t1));
  t2 = ((uint32_t)(t1));
  for (t3=0; t3<4; t3++) (v2).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(4));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v3 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v3;
  t7 = ((uint32_t *)(v2).data) + gg_index(t6, ((int64_t)UINT64_C(4)));
  t8 = v0;
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t12)))));
  (*t7) = gg_function_3(t8, t13, base + 0);
  }
label1:;
  t4++; goto label0;
label2:;
  v4 = ((uint32_t)(((int64_t)UINT64_C(1061109567))));
  v5 = ((uint32_t)(((int64_t)UINT64_C(252645135))));
  v6 = ((uint32_t)(((int64_t)UINT64_C(50529027))));
  /* FG_DIRECT_SLICE_INDEX */
  t14 = ((int64_t)UINT64_C(3));
  t15 = ((uint32_t *)(v2).data) + gg_index(t14, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t16 = ((int64_t)UINT64_C(2));
  t17 = ((uint32_t *)(v2).data) + gg_index(t16, ((int64_t)UINT64_C(4)));
  t18 = ((uint32_t)((*t17)));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = ((uint32_t)(((t18) >> ((uint64_t)(t19) & 31))));
  t21 = ((uint32_t)(v5));
  t22 = ((uint32_t)(((t20) & (t21))));
  /* FG_DIRECT_SLICE_INDEX */
  t23 = ((int64_t)UINT64_C(1));
  t24 = ((uint32_t *)(v2).data) + gg_index(t23, ((int64_t)UINT64_C(4)));
  t25 = ((uint32_t)((*t24)));
  t26 = ((int64_t)(((int64_t)UINT64_C(6))));
  t27 = ((uint32_t)(((t25) >> ((uint64_t)(t26) & 31))));
  t28 = ((uint32_t)(v6));
  t29 = ((uint32_t)(((t27) & (t28))));
  t30 = ((int64_t)(((int64_t)UINT64_C(4))));
  t31 = ((uint32_t)(((uint32_t)((uint64_t)(t29) << ((uint64_t)(t30) & 31)))));
  (*t15) = ((uint32_t)(((t22) | (t31))));
  /* FG_DIRECT_SLICE_INDEX */
  t32 = ((int64_t)UINT64_C(1));
  t33 = ((uint32_t *)(v2).data) + gg_index(t32, ((int64_t)UINT64_C(4)));
  t34 = ((uint32_t)((*t33)));
  t35 = ((uint32_t)(v4));
  v7 = ((uint32_t)(((t34) & (t35))));
  /* FG_DIRECT_SLICE_INDEX */
  t36 = ((int64_t)UINT64_C(1));
  t37 = ((uint32_t *)(v2).data) + gg_index(t36, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t38 = ((int64_t)UINT64_C(2));
  t39 = ((uint32_t *)(v2).data) + gg_index(t38, ((int64_t)UINT64_C(4)));
  t40 = ((uint32_t)((*t39)));
  t41 = ((uint32_t)(v5));
  t42 = ((uint32_t)(((t40) & (t41))));
  /* FG_DIRECT_SLICE_INDEX */
  t43 = ((int64_t)UINT64_C(0));
  t44 = ((uint32_t *)(v2).data) + gg_index(t43, ((int64_t)UINT64_C(4)));
  t45 = ((uint32_t)((*t44)));
  t46 = ((int64_t)(((int64_t)UINT64_C(6))));
  t47 = ((uint32_t)(((t45) >> ((uint64_t)(t46) & 31))));
  t48 = ((uint32_t)(v6));
  t49 = ((uint32_t)(((t47) & (t48))));
  t50 = ((int64_t)(((int64_t)UINT64_C(4))));
  t51 = ((uint32_t)(((uint32_t)((uint64_t)(t49) << ((uint64_t)(t50) & 31)))));
  (*t37) = ((uint32_t)(((t42) | (t51))));
  /* FG_DIRECT_SLICE_INDEX */
  t52 = ((int64_t)UINT64_C(2));
  t53 = ((uint32_t *)(v2).data) + gg_index(t52, ((int64_t)UINT64_C(4)));
  (*t53) = ((uint32_t)(v7));
  /* FG_DIRECT_SLICE_INDEX */
  t54 = ((int64_t)UINT64_C(0));
  t55 = ((uint32_t *)(v2).data) + gg_index(t54, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t56 = ((int64_t)UINT64_C(0));
  t57 = ((uint32_t *)(v2).data) + gg_index(t56, ((int64_t)UINT64_C(4)));
  t58 = ((uint32_t)((*t57)));
  t59 = ((uint32_t)(v4));
  (*t55) = ((uint32_t)(((t58) & (t59))));
  memset(&(v8), 0, sizeof(v8));
  memset(&t60, 0, sizeof(t60));
  t61 = ((uint8_t)(t60));
  for (t62=0; t62<8; t62++) ((v8).f0).data[t62] = t61;
  memset(&t63, 0, sizeof(t63));
  t64 = ((uint8_t)(t63));
  for (t65=0; t65<8; t65++) ((v8).f1).data[t65] = t64;
  t67 = ((int64_t)UINT64_C(4));
  t66 = 0;
label3:;
  if (t66 >= t67) goto label5;
  v9 = t66;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t68 = v9;
  t69 = ((uint8_t *)((v8).f0).data) + gg_index(t68, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t70 = ((int64_t)UINT64_C(0));
  t71 = ((uint32_t *)(v2).data) + gg_index(t70, ((int64_t)UINT64_C(4)));
  t72 = ((uint32_t)((*t71)));
  t73 = ((int64_t)(((int64_t)UINT64_C(8))));
  t74 = ((int64_t)(v9));
  t75 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))))));
  (*t69) = ((uint8_t)(((uint8_t)(((t72) >> ((uint64_t)(t75) & 31))))));
  t76 = gg_view(((v8).f0).data, 8);
  t77 = ((int64_t)(((int64_t)UINT64_C(4))));
  t78 = ((int64_t)(v9));
  t79 = ((int64_t)((uint64_t)(t77) + (uint64_t)(t78)));
  t80 = ((uint8_t *)t76.data) + gg_index(t79, t76.len);
  /* FG_DIRECT_SLICE_INDEX */
  t81 = ((int64_t)UINT64_C(1));
  t82 = ((uint32_t *)(v2).data) + gg_index(t81, ((int64_t)UINT64_C(4)));
  t83 = ((uint32_t)((*t82)));
  t84 = ((int64_t)(((int64_t)UINT64_C(8))));
  t85 = ((int64_t)(v9));
  t86 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))))));
  (*t80) = ((uint8_t)(((uint8_t)(((t83) >> ((uint64_t)(t86) & 31))))));
  /* FG_DIRECT_SLICE_INDEX */
  t87 = v9;
  t88 = ((uint8_t *)((v8).f1).data) + gg_index(t87, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t89 = ((int64_t)UINT64_C(2));
  t90 = ((uint32_t *)(v2).data) + gg_index(t89, ((int64_t)UINT64_C(4)));
  t91 = ((uint32_t)((*t90)));
  t92 = ((int64_t)(((int64_t)UINT64_C(8))));
  t93 = ((int64_t)(v9));
  t94 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t92) * (uint64_t)(t93)))))));
  (*t88) = ((uint8_t)(((uint8_t)(((t91) >> ((uint64_t)(t94) & 31))))));
  t95 = gg_view(((v8).f1).data, 8);
  t96 = ((int64_t)(((int64_t)UINT64_C(4))));
  t97 = ((int64_t)(v9));
  t98 = ((int64_t)((uint64_t)(t96) + (uint64_t)(t97)));
  t99 = ((uint8_t *)t95.data) + gg_index(t98, t95.len);
  /* FG_DIRECT_SLICE_INDEX */
  t100 = ((int64_t)UINT64_C(3));
  t101 = ((uint32_t *)(v2).data) + gg_index(t100, ((int64_t)UINT64_C(4)));
  t102 = ((uint32_t)((*t101)));
  t103 = ((int64_t)(((int64_t)UINT64_C(8))));
  t104 = ((int64_t)(v9));
  t105 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t104)))))));
  (*t99) = ((uint8_t)(((uint8_t)(((t102) >> ((uint64_t)(t105) & 31))))));
  }
label4:;
  t66++; goto label3;
label5:;
  t0 = v8;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_2(int64_t p0, gg_slice p1, gg_slice p2, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  float t11;
  float t12;
  int64_t t13;
  float t14;
  float t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  gg_slice t23;
  int64_t t24;
  uint32_t t25;
  float t26;
  gg_slice t27;
  int64_t t28;
  uint16_t t29;
  float t30;
  float t31;
  float t32;
  float t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint16_t t38;
  float t39;
  float t40;
  float t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int32_t t47;
  int32_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  gg_slice t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t * t64;
  gg_slice t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int32_t * t71;
  uint8_t t72;
  uint8_t t73;
  gg_slice t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int32_t * t82;
  uint8_t t83;
  int64_t t84;
  int32_t t85;
  int32_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  gg_slice t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  uint16_t t102;
  gg_slice t103;
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
  uint16_t t115;
  gg_slice t116;
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
  uint16_t t128;
  gg_slice t129;
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
  uint16_t t141;
  int64_t t142;
  int32_t * t143;
  int64_t t144;
  uint8_t * t145;
  int32_t t146;
  int32_t t147;
  int32_t t148;
  int32_t t149;
  int32_t t150;
  gg_slice t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint8_t * t155;
  int32_t t156;
  int32_t t157;
  int32_t t158;
  int32_t t159;
  int32_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  float * t164;
  float t165;
  int64_t t166;
  int32_t * t167;
  float t168;
  int64_t t169;
  float * t170;
  float t171;
  float t172;
  int32_t t173;
  int32_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  uint8_t * t179;
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
  int32_t t192;
  gg_slice t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  uint8_t * t199;
  int32_t t200;
  int64_t t201;
  int32_t * t202;
  int32_t t203;
  int32_t t204;
  int64_t t205;
  int32_t * t206;
  int32_t t207;
  int32_t t208;
  int32_t t209;
  int32_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  float * t214;
  float t215;
  int64_t t216;
  int32_t * t217;
  float t218;
  int64_t t219;
  float * t220;
  float t221;
  float t222;
  int64_t t223;
  int64_t t224;
  gg_type_7 t225;
  float t226;
  float t227;
  int64_t t228;
  float * t229;
  float t230;
  int64_t t231;
  float * t232;
  float t233;
  float t234;
  int64_t t235;
  float * t236;
  float t237;
  int64_t t238;
  float * t239;
  float t240;
  float t241;
  float t242;
  int64_t v0;
  gg_slice v1;
  gg_slice v2;
  int64_t v3;
  gg_type_7 v4;
  gg_type_8 v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  float v10;
  float v11;
  gg_type_12 v12;
  gg_type_16 v13;
  int64_t v14;
  int64_t v15;
  uint8_t v16;
  gg_type_17 v17;
  int64_t v18;
  int64_t v19;
  int32_t v20;
  int32_t v21;
  int32_t v22;
  int32_t v23;
  int64_t v24;
  gg_type_19 v25;
  int64_t v26;
  int32_t v27;
  int64_t v28;
  int32_t v29;
  int64_t v30;
  int64_t v31;
  int64_t v32;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(256))));
  t7 = ((int64_t)(((int64_t)gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t7) == (t8));
  }
  if (!(t4)) gg_fail("assertion failed");
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(256))));
  v3 = ((int64_t)(((int64_t)gg_div(t9, t10))));
  memset(&t11, 0, sizeof(t11));
  t12 = ((float)(t11));
  for (t13=0; t13<8; t13++) (v4).data[t13] = t12;
  memset(&t14, 0, sizeof(t14));
  t15 = ((float)(t14));
  for (t16=0; t16<4; t16++) (v5).data[t16] = t15;
  t18 = v3;
  t17 = 0;
label0:;
  if (t17 >= t18) goto label2;
  v6 = t17;
  {
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)UINT64_C(144))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t21 = ((int64_t)(v6));
  t22 = ((int64_t)(((int64_t)UINT64_C(292))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t23 = v2;
  t24 = ((int64_t)(v8));
  t25 = gg_function_3(t23, t24, base + 0);
  t26 = fg143_from_bits(t25);
  v9 = ((float)(t26));
  t27 = v1;
  t28 = ((int64_t)(v7));
  t29 = gg_function_5(t27, t28, base + 0);
  t30 = fg143_half(t29);
  t31 = ((float)(t30));
  t32 = ((float)(v9));
  v10 = ((float)(((t31) * (t32))));
  t33 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t34 = v1;
  t35 = ((int64_t)(v7));
  t36 = ((int64_t)(((int64_t)UINT64_C(2))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t38 = gg_function_5(t34, t37, base + 0);
  t39 = fg143_half(t38);
  t40 = ((float)(t39));
  t41 = ((float)(v9));
  t42 = ((float)(((t40) * (t41))));
  v11 = ((float)(((t33) - (t42))));
  t43 = v1;
  t44 = ((int64_t)(v7));
  t45 = ((int64_t)(((int64_t)UINT64_C(4))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  v12 = gg_function_7(t43, t46, base + 0);
  memset(&t47, 0, sizeof(t47));
  t48 = ((int32_t)(t47));
  for (t49=0; t49<256; t49++) (v13).data[t49] = t48;
  t51 = ((int64_t)UINT64_C(4));
  t50 = 0;
label3:;
  if (t50 >= t51) goto label5;
  v14 = t50;
  {
  t53 = ((int64_t)UINT64_C(32));
  t52 = 0;
label6:;
  if (t52 >= t53) goto label8;
  v15 = t52;
  {
  t54 = v1;
  t55 = ((int64_t)(v7));
  t56 = ((int64_t)(((int64_t)UINT64_C(16))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
  t58 = ((int64_t)(v14));
  t59 = ((int64_t)(((int64_t)UINT64_C(32))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t60)))));
  t62 = ((int64_t)(v15));
  t63 = ((int64_t)((uint64_t)(t61) + (uint64_t)(t62)));
  t64 = ((uint8_t *)t54.data) + gg_index(t63, t54.len);
  v16 = ((uint8_t)((*t64)));
  t65 = gg_view((v13).data, 256);
  t66 = ((int64_t)(v14));
  t67 = ((int64_t)(((int64_t)UINT64_C(64))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  t69 = ((int64_t)(v15));
  t70 = ((int64_t)((uint64_t)(t68) + (uint64_t)(t69)));
  t71 = ((int32_t *)t65.data) + gg_index(t70, t65.len);
  t72 = ((uint8_t)(v16));
  t73 = ((uint8_t)(((int64_t)UINT64_C(15))));
  (*t71) = ((int32_t)(((int32_t)(((t72) & (t73))))));
  t74 = gg_view((v13).data, 256);
  t75 = ((int64_t)(v14));
  t76 = ((int64_t)(((int64_t)UINT64_C(64))));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t75) * (uint64_t)(t76)))));
  t78 = ((int64_t)(((int64_t)UINT64_C(32))));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  t80 = ((int64_t)(v15));
  t81 = ((int64_t)((uint64_t)(t79) + (uint64_t)(t80)));
  t82 = ((int32_t *)t74.data) + gg_index(t81, t74.len);
  t83 = ((uint8_t)(v16));
  t84 = ((int64_t)(((int64_t)UINT64_C(4))));
  (*t82) = ((int32_t)(((int32_t)(((t83) >> ((uint64_t)(t84) & 7))))));
  }
label7:;
  t52++; goto label6;
label8:;
  }
label4:;
  t50++; goto label3;
label5:;
  memset(&t85, 0, sizeof(t85));
  t86 = ((int32_t)(t85));
  for (t87=0; t87<4; t87++) (v17).data[t87] = t86;
  t89 = ((int64_t)UINT64_C(4));
  t88 = 0;
label9:;
  if (t88 >= t89) goto label11;
  v18 = t88;
  {
  t90 = ((int64_t)(((int64_t)UINT64_C(2))));
  t91 = ((int64_t)(v18));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
  t92 = v2;
  t93 = ((int64_t)(v8));
  t94 = ((int64_t)(((int64_t)UINT64_C(260))));
  t95 = ((int64_t)(((int64_t)((uint64_t)(t93) + (uint64_t)(t94)))));
  t96 = ((int64_t)(((int64_t)UINT64_C(2))));
  t97 = ((int64_t)(((int64_t)UINT64_C(4))));
  t98 = ((int64_t)(v18));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t97) * (uint64_t)(t98)))));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t96) * (uint64_t)(t99)))));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t95) + (uint64_t)(t100)))));
  t102 = gg_function_5(t92, t101, base + 0);
  v20 = ((int32_t)(((int32_t)(((int16_t)(t102))))));
  t103 = v2;
  t104 = ((int64_t)(v8));
  t105 = ((int64_t)(((int64_t)UINT64_C(260))));
  t106 = ((int64_t)(((int64_t)((uint64_t)(t104) + (uint64_t)(t105)))));
  t107 = ((int64_t)(((int64_t)UINT64_C(2))));
  t108 = ((int64_t)(((int64_t)UINT64_C(4))));
  t109 = ((int64_t)(v18));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = ((int64_t)(((int64_t)UINT64_C(1))));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t107) * (uint64_t)(t112)))));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t106) + (uint64_t)(t113)))));
  t115 = gg_function_5(t103, t114, base + 0);
  v21 = ((int32_t)(((int32_t)(((int16_t)(t115))))));
  t116 = v2;
  t117 = ((int64_t)(v8));
  t118 = ((int64_t)(((int64_t)UINT64_C(260))));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) + (uint64_t)(t118)))));
  t120 = ((int64_t)(((int64_t)UINT64_C(2))));
  t121 = ((int64_t)(((int64_t)UINT64_C(4))));
  t122 = ((int64_t)(v18));
  t123 = ((int64_t)(((int64_t)((uint64_t)(t121) * (uint64_t)(t122)))));
  t124 = ((int64_t)(((int64_t)UINT64_C(2))));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) + (uint64_t)(t124)))));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t120) * (uint64_t)(t125)))));
  t127 = ((int64_t)(((int64_t)((uint64_t)(t119) + (uint64_t)(t126)))));
  t128 = gg_function_5(t116, t127, base + 0);
  v22 = ((int32_t)(((int32_t)(((int16_t)(t128))))));
  t129 = v2;
  t130 = ((int64_t)(v8));
  t131 = ((int64_t)(((int64_t)UINT64_C(260))));
  t132 = ((int64_t)(((int64_t)((uint64_t)(t130) + (uint64_t)(t131)))));
  t133 = ((int64_t)(((int64_t)UINT64_C(2))));
  t134 = ((int64_t)(((int64_t)UINT64_C(4))));
  t135 = ((int64_t)(v18));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t134) * (uint64_t)(t135)))));
  t137 = ((int64_t)(((int64_t)UINT64_C(3))));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t136) + (uint64_t)(t137)))));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t133) * (uint64_t)(t138)))));
  t140 = ((int64_t)(((int64_t)((uint64_t)(t132) + (uint64_t)(t139)))));
  t141 = gg_function_5(t129, t140, base + 0);
  v23 = ((int32_t)(((int32_t)(((int16_t)(t141))))));
  /* FG_DIRECT_SLICE_INDEX */
  t142 = v18;
  t143 = ((int32_t *)(v17).data) + gg_index(t142, ((int64_t)UINT64_C(4)));
  /* FG_DIRECT_SLICE_INDEX */
  t144 = v19;
  t145 = ((uint8_t *)((v12).f1).data) + gg_index(t144, ((int64_t)UINT64_C(8)));
  t146 = ((int32_t)(((int32_t)((*t145)))));
  t147 = ((int32_t)(v20));
  t148 = ((int32_t)(v21));
  t149 = ((int32_t)(((int32_t)((uint64_t)(t147) + (uint64_t)(t148)))));
  t150 = ((int32_t)(((int32_t)((uint64_t)(t146) * (uint64_t)(t149)))));
  t151 = gg_view(((v12).f1).data, 8);
  t152 = ((int64_t)(v19));
  t153 = ((int64_t)(((int64_t)UINT64_C(1))));
  t154 = ((int64_t)((uint64_t)(t152) + (uint64_t)(t153)));
  t155 = ((uint8_t *)t151.data) + gg_index(t154, t151.len);
  t156 = ((int32_t)(((int32_t)((*t155)))));
  t157 = ((int32_t)(v22));
  t158 = ((int32_t)(v23));
  t159 = ((int32_t)(((int32_t)((uint64_t)(t157) + (uint64_t)(t158)))));
  t160 = ((int32_t)(((int32_t)((uint64_t)(t156) * (uint64_t)(t159)))));
  (*t143) = ((int32_t)(((int32_t)((uint64_t)(t150) + (uint64_t)(t160)))));
  }
label10:;
  t88++; goto label9;
label11:;
  t162 = ((int64_t)UINT64_C(4));
  t161 = 0;
label12:;
  if (t161 >= t162) goto label14;
  v24 = t161;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t163 = v24;
  t164 = ((float *)(v5).data) + gg_index(t163, ((int64_t)UINT64_C(4)));
  t165 = ((float)(v11));
  /* FG_DIRECT_SLICE_INDEX */
  t166 = v24;
  t167 = ((int32_t *)(v17).data) + gg_index(t166, ((int64_t)UINT64_C(4)));
  t168 = ((float)(((float)((*t167)))));
  /* FG_DIRECT_SLICE_INDEX */
  t169 = v24;
  t170 = ((float *)(v5).data) + gg_index(t169, ((int64_t)UINT64_C(4)));
  t171 = ((float)((*t170)));
  t172 = fmaf(t165, t168, t171);
  (*t164) = ((float)(t172));
  }
label13:;
  t161++; goto label12;
label14:;
  memset(&t173, 0, sizeof(t173));
  t174 = ((int32_t)(t173));
  for (t175=0; t175<8; t175++) (v25).data[t175] = t174;
  t177 = ((int64_t)UINT64_C(8));
  t176 = 0;
label15:;
  if (t176 >= t177) goto label17;
  v26 = t176;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t178 = v26;
  t179 = ((uint8_t *)((v12).f0).data) + gg_index(t178, ((int64_t)UINT64_C(8)));
  v27 = ((int32_t)(((int32_t)((*t179)))));
  t181 = ((int64_t)UINT64_C(8));
  t180 = 0;
label18:;
  if (t180 >= t181) goto label20;
  v28 = t180;
  {
  v29 = ((int32_t)(((int64_t)UINT64_C(0))));
  t183 = ((int64_t)UINT64_C(4));
  t182 = 0;
label21:;
  if (t182 >= t183) goto label23;
  v30 = t182;
  {
  t184 = ((int64_t)(v26));
  t185 = ((int64_t)(((int64_t)UINT64_C(32))));
  t186 = ((int64_t)(((int64_t)((uint64_t)(t184) * (uint64_t)(t185)))));
  t187 = ((int64_t)(((int64_t)UINT64_C(4))));
  t188 = ((int64_t)(v28));
  t189 = ((int64_t)(((int64_t)((uint64_t)(t187) * (uint64_t)(t188)))));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t186) + (uint64_t)(t189)))));
  t191 = ((int64_t)(v30));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t190) + (uint64_t)(t191)))));
  t192 = v29;
  t193 = v2;
  t194 = ((int64_t)(v8));
  t195 = ((int64_t)(((int64_t)UINT64_C(4))));
  t196 = ((int64_t)(((int64_t)((uint64_t)(t194) + (uint64_t)(t195)))));
  t197 = ((int64_t)(v31));
  t198 = ((int64_t)((uint64_t)(t196) + (uint64_t)(t197)));
  t199 = ((uint8_t *)t193.data) + gg_index(t198, t193.len);
  t200 = ((int32_t)(((int32_t)(((int8_t)((*t199)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t201 = v31;
  t202 = ((int32_t *)(v13).data) + gg_index(t201, ((int64_t)UINT64_C(256)));
  t203 = ((int32_t)((*t202)));
  t204 = ((int32_t)(((int32_t)((uint64_t)(t200) * (uint64_t)(t203)))));
  v29 = ((int32_t)((uint64_t)(t192) + (uint64_t)(t204)));
  }
label22:;
  t182++; goto label21;
label23:;
  /* FG_DIRECT_SLICE_INDEX */
  t205 = v28;
  t206 = ((int32_t *)(v25).data) + gg_index(t205, ((int64_t)UINT64_C(8)));
  t207 = (*t206);
  t208 = ((int32_t)(v27));
  t209 = ((int32_t)(v29));
  t210 = ((int32_t)(((int32_t)((uint64_t)(t208) * (uint64_t)(t209)))));
  (*t206) = ((int32_t)((uint64_t)(t207) + (uint64_t)(t210)));
  }
label19:;
  t180++; goto label18;
label20:;
  }
label16:;
  t176++; goto label15;
label17:;
  t212 = ((int64_t)UINT64_C(8));
  t211 = 0;
label24:;
  if (t211 >= t212) goto label26;
  v32 = t211;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t213 = v32;
  t214 = ((float *)(v4).data) + gg_index(t213, ((int64_t)UINT64_C(8)));
  t215 = ((float)(v10));
  /* FG_DIRECT_SLICE_INDEX */
  t216 = v32;
  t217 = ((int32_t *)(v25).data) + gg_index(t216, ((int64_t)UINT64_C(8)));
  t218 = ((float)(((float)((*t217)))));
  /* FG_DIRECT_SLICE_INDEX */
  t219 = v32;
  t220 = ((float *)(v4).data) + gg_index(t219, ((int64_t)UINT64_C(8)));
  t221 = ((float)((*t220)));
  t222 = fmaf(t215, t218, t221);
  (*t214) = ((float)(t222));
  }
label25:;
  t211++; goto label24;
label26:;
  }
label1:;
  t17++; goto label0;
label2:;
  t223 = ((int64_t)(((int64_t)UINT64_C(0))));
  t224 = ((int64_t)(v0));
  fg143_witness(t223, t224);
  t225 = v4;
  t226 = gg_function_10(t225, base + 0);
  t227 = ((float)(t226));
  /* FG_DIRECT_SLICE_INDEX */
  t228 = ((int64_t)UINT64_C(0));
  t229 = ((float *)(v5).data) + gg_index(t228, ((int64_t)UINT64_C(4)));
  t230 = ((float)((*t229)));
  /* FG_DIRECT_SLICE_INDEX */
  t231 = ((int64_t)UINT64_C(2));
  t232 = ((float *)(v5).data) + gg_index(t231, ((int64_t)UINT64_C(4)));
  t233 = ((float)((*t232)));
  t234 = ((float)(((t230) + (t233))));
  /* FG_DIRECT_SLICE_INDEX */
  t235 = ((int64_t)UINT64_C(1));
  t236 = ((float *)(v5).data) + gg_index(t235, ((int64_t)UINT64_C(4)));
  t237 = ((float)((*t236)));
  /* FG_DIRECT_SLICE_INDEX */
  t238 = ((int64_t)UINT64_C(3));
  t239 = ((float *)(v5).data) + gg_index(t238, ((int64_t)UINT64_C(4)));
  t240 = ((float)((*t239)));
  t241 = ((float)(((t237) + (t240))));
  t242 = ((float)(((t234) + (t241))));
  t0 = ((float)(((t227) + (t242))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_1(int64_t p0, int64_t p1, gg_slice p2, gg_slice p3, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  gg_slice t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  gg_slice t10;
  gg_slice t11;
  int64_t v0;
  int64_t v1;
  gg_slice v2;
  gg_slice v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t5 = v2;
  t6 = v3;
  t0 = gg_function_2(t4, t5, t6, base + 0);
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  if (!(((t7) == (t8)))) gg_fail("assertion failed");
  t9 = ((int64_t)(v1));
  t10 = v2;
  t11 = v3;
  t0 = gg_function_11(t9, t10, t11, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  gg_header t6;
  int64_t t7;
  int64_t t8;
  gg_slice t9;
  gg_slice t10;
  float t11;
  int64_t t12;
  gg_type_2 v0;
  gg_type_2 v1;
  {
  memset(&t0, 0, sizeof(t0));
  t1 = ((uint8_t)(t0));
  for (t2=0; t2<1; t2++) (v0).data[t2] = t1;
  memset(&t3, 0, sizeof(t3));
  t4 = ((uint8_t)(t3));
  for (t5=0; t5<1; t5++) (v1).data[t5] = t4;
  t6.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t6.len = 0;
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(256))));
  t9 = gg_view((v0).data, 1);
  t10 = gg_view((v1).data, 1);
  t11 = gg_function_1(t7, t8, t9, t10, base + 1);
  gg_float32(gg_ref(&t6, gg_get_stack(base + 0)), t11);
  gg_append(gg_ref(&t6, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t6).base, (t6).len));
  }
gg_done:;
  for (t12 = 0; t12 < 1; t12++) gg_stacks[base + t12].top = 0;
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
