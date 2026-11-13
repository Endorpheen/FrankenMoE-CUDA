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
typedef struct { uint64_t data[1]; } gg_type_2;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[1]; } gg_type_4;
#pragma pack(pop)
typedef struct gg_type_13 gg_type_13;
#pragma pack(push, 1)
typedef struct { uint8_t data[7]; } gg_type_16;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_13 {
  int32_t f0;
  int32_t f1;
  int32_t f2;
  uint32_t f3;
  int32_t f4;
  uint32_t f5;
  uint64_t f6;
  uint8_t f7;
  gg_type_16 f8;
};
#pragma pack(pop)
typedef gg_type_13 *gg_type_14;
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, uint64_t value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(int32_t a, int32_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, int32_t value);
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_type_4 value);
static uint8_t gg_eq_5(int64_t a, int64_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, int64_t value);
static uint8_t gg_eq_6(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, gg_slice value);
static uint8_t gg_eq_7(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_slice value);
static uint8_t gg_eq_8(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_slice value);
static uint8_t gg_eq_9(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, gg_slice value);
static uint8_t gg_eq_10(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, uint32_t value);
static uint8_t gg_eq_11(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, uint8_t value);
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_type_13 value);
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, gg_type_14 value);
static uint8_t gg_eq_15(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, uint8_t value);
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b);
static uint8_t gg_equal_elements_16(gg_slice a, gg_slice b);
static void gg_render_16(gg_reference out, gg_type_16 value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
return gg_equal_elements_1(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((uint64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_type_4*)a.data)[i],((gg_type_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b) {
return gg_equal_elements_3(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_6(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((uint64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_7(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((uint64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_8(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_9(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_10(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_11(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
if(!gg_eq_3(a.f0,b.f0))return 0;
if(!gg_eq_3(a.f1,b.f1))return 0;
if(!gg_eq_3(a.f2,b.f2))return 0;
if(!gg_eq_10(a.f3,b.f3))return 0;
if(!gg_eq_3(a.f4,b.f4))return 0;
if(!gg_eq_10(a.f5,b.f5))return 0;
if(!gg_eq_1(a.f6,b.f6))return 0;
if(!gg_eq_11(a.f7,b.f7))return 0;
if(!gg_eq_16(a.f8,b.f8))return 0;
return 1;
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
gg_append(out,"\116\157\144\145\123\164\141\164\145\040\173\040",12,1);
gg_render_3(out, value.f0);
gg_append(out,", ",2,1);
gg_render_3(out, value.f1);
gg_append(out,", ",2,1);
gg_render_3(out, value.f2);
gg_append(out,", ",2,1);
gg_render_10(out, value.f3);
gg_append(out,", ",2,1);
gg_render_3(out, value.f4);
gg_append(out,", ",2,1);
gg_render_10(out, value.f5);
gg_append(out,", ",2,1);
gg_render_1(out, value.f6);
gg_append(out,", ",2,1);
gg_render_11(out, value.f7);
gg_append(out,", ",2,1);
gg_quote(out, gg_view((value.f8).data, 7));
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_type_14*)a.data)[i],((gg_type_14*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_type_14 a, gg_type_14 b) {
return a == b;
}
static void gg_render_14(gg_reference out, gg_type_14 value) {
gg_render_13(out,*value);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_15(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_16(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_16(((gg_type_16*)a.data)[i],((gg_type_16*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_16(gg_type_16 a, gg_type_16 b) {
return gg_equal_elements_15(gg_view((a).data, 7),gg_view((b).data, 7));
}
static void gg_render_16(gg_reference out, gg_type_16 value) {
gg_text(out,gg_view((value).data, 7));
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(uint64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t p5, uint32_t p6, int32_t p7, uint8_t p8, int64_t base);
static int32_t gg_function_2(gg_slice p0, int64_t p1, int64_t base);
static void gg_function_7(uint64_t p0, uint64_t p1, uint8_t p2, uint8_t p3, int64_t base);
static int32_t gg_function_12(uint64_t p0, uint64_t p1, uint8_t p2, int64_t base);
static uint8_t gg_function_13(uint64_t p0, uint64_t p1, uint8_t p2, int64_t base);
static uint8_t gg_function_13(uint64_t p0, uint64_t p1, uint8_t p2, int64_t base) {
  uint8_t t0;
  uint64_t t1;
  uint64_t t2;
  gg_type_14 t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  uint8_t t8;
  uint8_t t9;
  int32_t t10;
  int32_t t11;
  uint8_t t12;
  uint8_t t13;
  uint8_t t14;
  gg_type_14 t15;
  int32_t t16;
  int32_t t17;
  uint8_t t18;
  int32_t t19;
  int64_t t20;
  uint8_t t21;
  int32_t t22;
  uint8_t t23;
  uint64_t v0;
  uint64_t v1;
  uint8_t v2;
  gg_type_14 v3;
  int32_t v4;
  int32_t v5;
  uint8_t v6;
  uint8_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
  t3 = fg_graph_hash(t1, t2);
  v3 = t3;
  v4 = ((int32_t)(((*(v3))).f0));
  v5 = ((int32_t)(((*(v3))).f1));
  v6 = ((*(v3))).f7;
  t4 = ((*(v3))).f1;
  t5 = ((int32_t)(((int64_t)UINT64_C(1))));
  ((*(v3))).f1 = ((int32_t)((uint64_t)(t4) - (uint64_t)(t5)));
  t6 = ((int32_t)(((*(v3))).f1));
  t7 = ((int32_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) == (t7));
  t9 = t8;
  if (t9) {
  t10 = ((int32_t)(((*(v3))).f0));
  t11 = ((int32_t)(((int64_t)UINT64_C(0))));
  t9 = ((t10) == (t11));
  }
  t12 = t9;
  t13 = t12;
  if (t13) {
  t13 = ((*(v3))).f7;
  }
  v7 = t13;
  t14 = v2;
  if (t14) {
  {
  t15 = v3;
  t16 = ((int32_t)(v4));
  t17 = ((int32_t)(v5));
  t18 = v6;
  t19 = ((int32_t)(((int64_t)UINT64_C(3))));
  t21 = v7;
  if (t21) {
  {
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t22 = ((int32_t)(((int32_t)(t20))));
  t23 = ((uint8_t)UINT64_C(1));
  fg_graph_verify(t15, t16, t17, t18, t19, t22, t23);
  }
  }
  t0 = v7;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_12(uint64_t p0, uint64_t p1, uint8_t p2, int64_t base) {
  int32_t t0;
  uint64_t t1;
  uint64_t t2;
  gg_type_14 t3;
  uint64_t t4;
  uint8_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  uint8_t t10;
  uint8_t t11;
  int32_t t12;
  int32_t t13;
  uint8_t t14;
  uint8_t t15;
  uint8_t t16;
  uint8_t t17;
  gg_type_14 t18;
  int32_t t19;
  int32_t t20;
  uint8_t t21;
  int32_t t22;
  int32_t t23;
  uint8_t t24;
  uint64_t v0;
  uint64_t v1;
  uint8_t v2;
  gg_type_14 v3;
  int32_t v4;
  int32_t v5;
  uint8_t v6;
  uint8_t v7;
  int32_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
  t3 = fg_graph_hash(t1, t2);
  v3 = t3;
  v4 = ((int32_t)(((*(v3))).f0));
  v5 = ((int32_t)(((*(v3))).f1));
  v6 = ((*(v3))).f7;
  t4 = ((uint64_t)(v1));
  t5 = fg_graph_is_view(t4);
  v7 = t5;
  t6 = ((*(v3))).f0;
  t7 = ((int32_t)(((int64_t)UINT64_C(1))));
  ((*(v3))).f0 = ((int32_t)((uint64_t)(t6) - (uint64_t)(t7)));
  v8 = ((int32_t)(((int64_t)UINT64_C(0))));
  t8 = ((int32_t)(((*(v3))).f0));
  t9 = ((int32_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) == (t9));
  t11 = t10;
  if (t11) {
  t12 = ((int32_t)(((*(v3))).f1));
  t13 = ((int32_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) == (t13));
  }
  t14 = t11;
  if (t14) {
  {
  t15 = v7;
  if (t15) {
  {
  v8 = ((int32_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  t16 = ((*(v3))).f7;
  if (t16) {
  {
  v8 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  }
  }
  t17 = v2;
  if (t17) {
  {
  t18 = v3;
  t19 = ((int32_t)(v4));
  t20 = ((int32_t)(v5));
  t21 = v6;
  t22 = ((int32_t)(((int64_t)UINT64_C(2))));
  t23 = ((int32_t)(v8));
  t24 = v7;
  fg_graph_verify(t18, t19, t20, t21, t22, t23, t24);
  }
  }
  t0 = ((int32_t)(v8));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_2(gg_slice p0, int64_t p1, int64_t base) {
  int32_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int32_t * t5;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)((v0).len));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int32_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t4 = v1;
  t5 = ((int32_t *)(v0).data) + gg_index(t4, (v0).len);
  t0 = ((int32_t)((*t5)));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_7(uint64_t p0, uint64_t p1, uint8_t p2, uint8_t p3, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  gg_type_14 t2;
  uint8_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  uint8_t t8;
  gg_type_14 t9;
  int32_t t10;
  int32_t t11;
  uint8_t t12;
  int64_t t13;
  uint8_t t14;
  int32_t t15;
  int32_t t16;
  uint8_t t17;
  uint64_t v0;
  uint64_t v1;
  uint8_t v2;
  uint8_t v3;
  gg_type_14 v4;
  int32_t v5;
  int32_t v6;
  uint8_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = fg_graph_hash(t0, t1);
  v4 = t2;
  v5 = ((int32_t)(((*(v4))).f0));
  v6 = ((int32_t)(((*(v4))).f1));
  v7 = ((*(v4))).f7;
  t3 = v2;
  if (t3) {
  {
  t4 = ((*(v4))).f1;
  t5 = ((int32_t)(((int64_t)UINT64_C(1))));
  ((*(v4))).f1 = ((int32_t)((uint64_t)(t4) + (uint64_t)(t5)));
  }
  } else {
  {
  t6 = ((*(v4))).f0;
  t7 = ((int32_t)(((int64_t)UINT64_C(1))));
  ((*(v4))).f0 = ((int32_t)((uint64_t)(t6) + (uint64_t)(t7)));
  }
  }
  t8 = v3;
  if (t8) {
  {
  t9 = v4;
  t10 = ((int32_t)(v5));
  t11 = ((int32_t)(v6));
  t12 = v7;
  t14 = v2;
  if (t14) {
  {
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t15 = ((int32_t)(((int32_t)(t13))));
  t16 = ((int32_t)(((int64_t)UINT64_C(0))));
  t17 = v2;
  fg_graph_verify(t9, t10, t11, t12, t15, t16, t17);
  }
  }
  }
gg_done:;
}
static void gg_function_1(uint64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t p5, uint32_t p6, int32_t p7, uint8_t p8, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  int64_t t17;
  uint64_t * t18;
  uint64_t t19;
  gg_slice t20;
  int64_t t21;
  int32_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t * t26;
  gg_slice t27;
  int64_t t28;
  uint64_t t29;
  uint8_t t30;
  uint8_t t31;
  uint8_t t32;
  uint64_t t33;
  int32_t t34;
  int32_t t35;
  int32_t t36;
  uint8_t t37;
  uint64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint8_t t42;
  uint8_t t43;
  uint64_t t44;
  uint32_t t45;
  uint32_t t46;
  uint32_t t47;
  uint32_t t48;
  uint32_t t49;
  uint8_t t50;
  uint64_t t51;
  uint64_t t52;
  int32_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  int64_t t57;
  uint64_t t58;
  uint64_t t59;
  uint64_t t60;
  uint8_t t61;
  uint64_t t62;
  uint64_t t63;
  uint8_t t64;
  uint8_t t65;
  uint64_t t66;
  uint32_t t67;
  uint32_t t68;
  uint32_t t69;
  uint32_t t70;
  uint32_t t71;
  uint8_t t72;
  uint64_t t73;
  uint64_t t74;
  int32_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t * t79;
  gg_slice t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  int64_t t85;
  uint64_t t86;
  uint64_t t87;
  uint64_t t88;
  uint8_t t89;
  uint64_t t90;
  uint64_t t91;
  int32_t t92;
  uint64_t t93;
  uint64_t t94;
  int32_t t95;
  int64_t t96;
  int64_t t97;
  uint64_t t98;
  int64_t t99;
  uint64_t t100;
  uint64_t t101;
  uint64_t t102;
  uint8_t t103;
  uint64_t t104;
  uint64_t t105;
  uint8_t t106;
  int32_t t107;
  int32_t t108;
  uint8_t t109;
  uint64_t t110;
  uint64_t t111;
  uint64_t t112;
  uint64_t t113;
  uint8_t t114;
  uint8_t t115;
  uint8_t t116;
  uint64_t t117;
  uint64_t t118;
  int32_t t119;
  int32_t t120;
  uint8_t t121;
  uint64_t t122;
  uint64_t t123;
  uint64_t v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  int64_t v5;
  uint32_t v6;
  int32_t v7;
  uint8_t v8;
  int64_t v9;
  int64_t v10;
  uint64_t v11;
  int32_t v12;
  int64_t v13;
  uint64_t v14;
  int64_t v15;
  uint64_t v16;
  int32_t v17;
  int64_t v18;
  uint64_t v19;
  int64_t v20;
  uint64_t v21;
  int32_t v22;
  uint64_t v23;
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
  t0 = ((int64_t)(v5));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t0) >= (t1)))) gg_fail("assertion failed");
  t2 = ((int64_t)((v3).len));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) == (t3));
  t5 = t4;
  if (!t5) {
  t6 = ((int64_t)((v3).len));
  t7 = ((int64_t)((v1).len));
  t5 = ((t6) >= (t7));
  }
  if (!(t5)) gg_fail("assertion failed");
  t8 = ((int64_t)((v4).len));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) == (t9));
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)((v4).len));
  t13 = ((int64_t)((v2).len));
  t11 = ((t12) >= (t13));
  }
  if (!(t11)) gg_fail("assertion failed");
  t15 = (v2).len;
  t14 = 0;
label0:;
  if (t14 >= t15) goto label2;
  v9 = t14;
  {
  t16 = ((uint64_t)(v0));
  /* FG_DIRECT_SLICE_INDEX */
  t17 = v9;
  t18 = ((uint64_t *)(v2).data) + gg_index(t17, (v2).len);
  t19 = ((uint64_t)((*t18)));
  t20 = v4;
  t21 = ((int64_t)(v9));
  t22 = gg_function_2(t20, t21, base + 0);
  fg_graph_allocate(t16, t19, t22);
  }
label1:;
  t14++; goto label0;
label2:;
  t24 = (v1).len;
  t23 = 0;
label3:;
  if (t23 >= t24) goto label5;
  v10 = t23;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v10;
  t26 = ((uint64_t *)(v1).data) + gg_index(t25, (v1).len);
  v11 = ((uint64_t)((*t26)));
  t27 = v3;
  t28 = ((int64_t)(v10));
  v12 = gg_function_2(t27, t28, base + 0);
  t29 = ((uint64_t)(v11));
  t30 = fg_graph_is_view(t29);
  t31 = t30;
  t32 = t31;
  if (t32) {
  t33 = ((uint64_t)(v11));
  t34 = fg_graph_op(t33);
  t35 = ((int32_t)(t34));
  t36 = ((int32_t)(v7));
  t32 = ((t35) != (t36));
  }
  t37 = t32;
  if (t37) {
  {
  t38 = ((uint64_t)(v0));
  t39 = ((uint64_t)(v11));
  t40 = fg_graph_owner(t39);
  t41 = ((uint64_t)(t40));
  t42 = ((uint8_t)UINT64_C(1));
  t43 = v8;
  gg_function_7(t38, t41, t42, t43, base + 0);
  }
  }
  t44 = ((uint64_t)(v11));
  t45 = fg_graph_flags(t44);
  t46 = ((uint32_t)(t45));
  t47 = ((uint32_t)(v6));
  t48 = ((uint32_t)(((t46) & (t47))));
  t49 = ((uint32_t)(((int64_t)UINT64_C(0))));
  t50 = ((t48) != (t49));
  if (t50) {
  {
  t51 = ((uint64_t)(v0));
  t52 = ((uint64_t)(v11));
  t53 = ((int32_t)(v12));
  fg_graph_allocate(t51, t52, t53);
  }
  }
  t55 = v5;
  t54 = 0;
label6:;
  if (t54 >= t55) goto label8;
  v13 = t54;
  {
  t56 = ((uint64_t)(v11));
  t57 = ((int64_t)(v13));
  t58 = fg_graph_source(t56, t57);
  v14 = ((uint64_t)(t58));
  t59 = ((uint64_t)(v14));
  t60 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t61 = ((t59) != (t60));
  if (t61) {
  {
  t62 = ((uint64_t)(v0));
  t63 = ((uint64_t)(v14));
  t64 = ((uint8_t)UINT64_C(0));
  t65 = v8;
  gg_function_7(t62, t63, t64, t65, base + 0);
  t66 = ((uint64_t)(v14));
  t67 = fg_graph_flags(t66);
  t68 = ((uint32_t)(t67));
  t69 = ((uint32_t)(v6));
  t70 = ((uint32_t)(((t68) & (t69))));
  t71 = ((uint32_t)(((int64_t)UINT64_C(0))));
  t72 = ((t70) != (t71));
  if (t72) {
  {
  t73 = ((uint64_t)(v0));
  t74 = ((uint64_t)(v14));
  t75 = ((int32_t)(v12));
  fg_graph_allocate(t73, t74, t75);
  }
  }
  }
  }
  }
label7:;
  t54++; goto label6;
label8:;
  }
label4:;
  t23++; goto label3;
label5:;
  t77 = (v1).len;
  t76 = 0;
label9:;
  if (t76 >= t77) goto label11;
  v15 = t76;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t78 = v15;
  t79 = ((uint64_t *)(v1).data) + gg_index(t78, (v1).len);
  v16 = ((uint64_t)((*t79)));
  t80 = v3;
  t81 = ((int64_t)(v15));
  v17 = gg_function_2(t80, t81, base + 0);
  t83 = v5;
  t82 = 0;
label12:;
  if (t82 >= t83) goto label14;
  v18 = t82;
  {
  t84 = ((uint64_t)(v16));
  t85 = ((int64_t)(v18));
  t86 = fg_graph_source(t84, t85);
  v19 = ((uint64_t)(t86));
  t87 = ((uint64_t)(v19));
  t88 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t89 = ((t87) != (t88));
  if (t89) {
  {
  t90 = ((uint64_t)(v0));
  t91 = ((uint64_t)(v19));
  t92 = ((int32_t)(v17));
  fg_graph_allocate(t90, t91, t92);
  }
  }
  }
label13:;
  t82++; goto label12;
label14:;
  t93 = ((uint64_t)(v0));
  t94 = ((uint64_t)(v16));
  t95 = ((int32_t)(v17));
  fg_graph_allocate(t93, t94, t95);
  t97 = v5;
  t96 = 0;
label15:;
  if (t96 >= t97) goto label17;
  v20 = t96;
  {
  t98 = ((uint64_t)(v16));
  t99 = ((int64_t)(v20));
  t100 = fg_graph_source(t98, t99);
  v21 = ((uint64_t)(t100));
  t101 = ((uint64_t)(v21));
  t102 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t103 = ((t101) != (t102));
  if (t103) {
  {
  t104 = ((uint64_t)(v0));
  t105 = ((uint64_t)(v21));
  t106 = v8;
  v22 = gg_function_12(t104, t105, t106, base + 0);
  t107 = ((int32_t)(v22));
  t108 = ((int32_t)(((int64_t)UINT64_C(2))));
  t109 = ((t107) == (t108));
  if (t109) {
  {
  t110 = ((uint64_t)(v21));
  t111 = fg_graph_owner(t110);
  v23 = ((uint64_t)(t111));
  t112 = ((uint64_t)(v0));
  t113 = ((uint64_t)(v23));
  t114 = v8;
  t115 = gg_function_13(t112, t113, t114, base + 0);
  t116 = t115;
  if (t116) {
  {
  t117 = ((uint64_t)(v0));
  t118 = ((uint64_t)(v23));
  fg_graph_release(t117, t118);
  }
  }
  }
  } else {
  t119 = ((int32_t)(v22));
  t120 = ((int32_t)(((int64_t)UINT64_C(1))));
  t121 = ((t119) == (t120));
  if (t121) {
  {
  t122 = ((uint64_t)(v0));
  t123 = ((uint64_t)(v21));
  fg_graph_release(t122, t123);
  }
  }
  }
  }
  }
  }
label16:;
  t96++; goto label15;
label17:;
  }
label10:;
  t76++; goto label9;
label11:;
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  uint64_t t0;
  uint64_t t1;
  int64_t t2;
  int32_t t3;
  int32_t t4;
  int64_t t5;
  uint64_t t6;
  gg_slice t7;
  int64_t t8;
  gg_slice t9;
  gg_slice t10;
  int64_t t11;
  gg_slice t12;
  gg_slice t13;
  int64_t t14;
  gg_slice t15;
  gg_slice t16;
  int64_t t17;
  gg_slice t18;
  int64_t t19;
  uint32_t t20;
  int32_t t21;
  uint8_t t22;
  gg_type_2 v0;
  gg_type_4 v1;
  {
  memset(&t0, 0, sizeof(t0));
  t1 = ((uint64_t)(t0));
  for (t2=0; t2<1; t2++) (v0).data[t2] = t1;
  memset(&t3, 0, sizeof(t3));
  t4 = ((int32_t)(t3));
  for (t5=0; t5<1; t5++) (v1).data[t5] = t4;
  t6 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t7 = gg_view((v0).data, 1);
  t8 = ((int64_t)UINT64_C(0));
  t9 = gg_sub(t7, 0, t8, sizeof(uint64_t));
  t10 = gg_view((v0).data, 1);
  t11 = ((int64_t)UINT64_C(0));
  t12 = gg_sub(t10, 0, t11, sizeof(uint64_t));
  t13 = gg_view((v1).data, 1);
  t14 = ((int64_t)UINT64_C(0));
  t15 = gg_sub(t13, 0, t14, sizeof(int32_t));
  t16 = gg_view((v1).data, 1);
  t17 = ((int64_t)UINT64_C(0));
  t18 = gg_sub(t16, 0, t17, sizeof(int32_t));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((uint32_t)(((int64_t)UINT64_C(1))));
  t21 = ((int32_t)(((int64_t)UINT64_C(0))));
  t22 = ((uint8_t)UINT64_C(0));
  gg_function_1(t6, t9, t12, t15, t18, t19, t20, t21, t22, base + 0);
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
