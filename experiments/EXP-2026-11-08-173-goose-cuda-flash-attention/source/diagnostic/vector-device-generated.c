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
typedef struct { float data[2]; } gg_type_8;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[8]; } gg_type_9;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[4]; } gg_type_11;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_12;
#pragma pack(pop)
typedef struct gg_type_13 gg_type_13;
#pragma pack(push, 1)
struct gg_type_13 {
  float f0;
  float f1;
  float f2;
  float f3;
};
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, uint64_t value);
static uint8_t gg_eq_2(int64_t a, int64_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, int64_t value);
static uint8_t gg_eq_3(float a, float b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, float value);
static uint8_t gg_eq_4(double a, double b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, double value);
static uint8_t gg_eq_5(int32_t a, int32_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, int32_t value);
static uint8_t gg_eq_6(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint8_t value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, gg_type_9 value);
static uint8_t gg_eq_10(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, uint32_t value);
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, gg_type_11 value);
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_type_12 value);
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_type_13 value);
static uint8_t gg_eq_14(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(float a, float b) {
return a == b;
}
static void gg_render_3(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(double a, double b) {
return a == b;
}
static void gg_render_4(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
return gg_equal_elements_3(gg_view((a).data, 2),gg_view((b).data, 2));
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
int64_t i; gg_slice view=gg_view((value).data, 2);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((gg_type_9*)a.data)[i],((gg_type_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b) {
return gg_equal_elements_3(gg_view((a).data, 8),gg_view((b).data, 8));
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
int64_t i; gg_slice view=gg_view((value).data, 8);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_10(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_type_11*)a.data)[i],((gg_type_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b) {
return gg_equal_elements_5(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_11(gg_reference out, gg_type_11 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return gg_equal_elements_3(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_type_13*)a.data)[i],((gg_type_13*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_type_13 a, gg_type_13 b) {
if(!gg_eq_3(a.f0,b.f0))return 0;
if(!gg_eq_3(a.f1,b.f1))return 0;
if(!gg_eq_3(a.f2,b.f2))return 0;
if(!gg_eq_3(a.f3,b.f3))return 0;
return 1;
}
static void gg_render_13(gg_reference out, gg_type_13 value) {
gg_append(out,"\126\064\040\173\040",5,1);
gg_render_3(out, value.f0);
gg_append(out,", ",2,1);
gg_render_3(out, value.f1);
gg_append(out,", ",2,1);
gg_render_3(out, value.f2);
gg_append(out,", ",2,1);
gg_render_3(out, value.f3);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_14(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, uint64_t p7, float p8, float p9, float p10, float p11, int32_t p12, float p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t p18, int64_t p19, int64_t p20, int32_t p21, int32_t p22, int32_t p23, int32_t p24, int64_t p25, int64_t p26, int64_t p27, int64_t p28, int64_t p29, int64_t p30, int32_t p31, int32_t p32, int32_t p33, int64_t p34, int64_t p35, int64_t p36, int32_t p37, int32_t p38, int32_t p39, int32_t p40, int32_t p41, int32_t p42, int32_t p43, int32_t p44, uint64_t p45, int64_t base);
static float gg_function_3(float p0, int32_t p1, int32_t p2, float p3, float p4, int64_t base);
static float gg_function_5(int64_t base);
static void gg_function_9(uint64_t p0, float p1, int64_t p2, int64_t p3, int32_t p4, uint64_t p5, int64_t base);
static float gg_function_11(float p0, int64_t base);
static float gg_function_19(uint64_t p0, int32_t p1, int32_t p2, int32_t p3, float p4, float p5, float p6, float p7, int64_t base);
static int32_t gg_function_20(uint64_t p0, int32_t p1, int32_t p2, int64_t base);
static float gg_function_24(float p0, int64_t base);
static gg_type_13 gg_function_28(uint64_t p0, int64_t p1, int64_t base);
static float gg_function_31(float p0, int64_t base);
static void gg_function_33(uint64_t p0, uint64_t p1, uint64_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, uint64_t p10, int64_t base);
static void gg_function_33(uint64_t p0, uint64_t p1, uint64_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, uint64_t p10, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  int32_t t24;
  int32_t t25;
  int32_t t26;
  int32_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  int32_t t31;
  int32_t t32;
  uint64_t t33;
  float t34;
  float t35;
  int32_t t36;
  int32_t t37;
  uint64_t t38;
  uint64_t t39;
  float t40;
  int32_t t41;
  int32_t t42;
  float t43;
  uint64_t t44;
  int32_t t45;
  int32_t t46;
  uint64_t t47;
  float t48;
  float t49;
  float t50;
  int32_t t51;
  int32_t t52;
  int32_t t53;
  int32_t t54;
  uint64_t t55;
  int32_t t56;
  int32_t t57;
  uint64_t t58;
  float t59;
  float t60;
  float t61;
  float t62;
  float t63;
  float t64;
  float t65;
  uint64_t t66;
  int32_t t67;
  int32_t t68;
  int32_t t69;
  int32_t t70;
  int32_t t71;
  int32_t t72;
  uint64_t t73;
  float t74;
  float t75;
  float t76;
  float t77;
  float t78;
  uint64_t t79;
  int32_t t80;
  int32_t t81;
  int32_t t82;
  int32_t t83;
  uint64_t t84;
  float t85;
  float t86;
  float t87;
  int32_t t88;
  int32_t t89;
  uint64_t t90;
  int32_t t91;
  int32_t t92;
  uint64_t t93;
  float t94;
  float t95;
  float t96;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int32_t v3;
  int32_t v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  int32_t v8;
  int32_t v9;
  uint64_t v10;
  int32_t v11;
  int64_t v12;
  uint64_t v13;
  uint64_t v14;
  uint64_t v15;
  int32_t v16;
  float v17;
  int32_t v18;
  float v19;
  float v20;
  float v21;
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
  t0 = ((int32_t)(v6));
  t1 = ((int32_t)(v7));
  t2 = ((int32_t)(((int32_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  t3 = ((int32_t)(v4));
  t4 = ((int32_t)(((int32_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = ((int32_t)(v8));
  t6 = ((int32_t)(((int32_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int32_t)(v5));
  v11 = ((int32_t)(((int32_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  v12 = ((int64_t)(((int64_t)(v3))));
  t8 = ((uint64_t)(v0));
  t9 = ((int64_t)(((int64_t)(v11))));
  t10 = ((int64_t)(v12));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(((int64_t)UINT64_C(1024))));
  t13 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))))));
  v13 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t13)))));
  t14 = ((uint64_t)(v1));
  t15 = ((int64_t)(((int64_t)(v11))));
  t16 = ((int64_t)(v12));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))))));
  v14 = ((uint64_t)(((uint64_t)((uint64_t)(t14) + (uint64_t)(t19)))));
  t20 = ((uint64_t)(v2));
  t21 = ((int64_t)(((int64_t)(v11))));
  t22 = ((int64_t)(((int64_t)UINT64_C(1024))));
  t23 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))))));
  v15 = ((uint64_t)(((uint64_t)((uint64_t)(t20) + (uint64_t)(t23)))));
  v16 = ((int32_t)(v9));
label1:;
  t24 = ((int32_t)(v16));
  t25 = ((int32_t)(((int64_t)UINT64_C(2))));
  t26 = ((int32_t)(v3));
  t27 = ((int32_t)(((int32_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  if (!(((t24) < (t27)))) goto label2;
  {
  t28 = ((uint64_t)(v10));
  t29 = ((uint64_t)(((uint64_t)(v16))));
  t30 = ((uint64_t)(v14));
  t31 = ((int32_t)(v16));
  t32 = ((int32_t)(((int64_t)UINT64_C(4))));
  t33 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t31) * (uint64_t)(t32)))))));
  t34 = fg175v_ld_f32(t30, t33);
  t35 = ((float)(t34));
  fg175v_sm_st_f32(t28, t29, t35);
  t36 = v16;
  t37 = ((int32_t)(((int64_t)UINT64_C(256))));
  v16 = ((int32_t)((uint64_t)(t36) + (uint64_t)(t37)));
  }
  goto label1;
label2:;
  fg175v_barrier();
  t38 = ((uint64_t)(v10));
  t39 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t40 = fg175v_sm_f32(t38, t39);
  v17 = ((float)(t40));
  v18 = ((int32_t)(((int64_t)UINT64_C(1))));
label4:;
  t41 = ((int32_t)(v18));
  t42 = ((int32_t)(v3));
  if (!(((t41) < (t42)))) goto label5;
  {
  t43 = ((float)(v17));
  t44 = ((uint64_t)(v10));
  t45 = ((int32_t)(((int64_t)UINT64_C(2))));
  t46 = ((int32_t)(v18));
  t47 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t45) * (uint64_t)(t46)))))));
  t48 = fg175v_sm_f32(t44, t47);
  t49 = ((float)(t48));
  t50 = fg175v_fmaxf(t43, t49);
  v17 = ((float)(t50));
  t51 = v18;
  t52 = ((int32_t)(((int64_t)UINT64_C(1))));
  v18 = ((int32_t)((uint64_t)(t51) + (uint64_t)(t52)));
  }
  goto label4;
label5:;
  v19 = ((float)(gg_double_bits(UINT64_C(0))));
  v20 = ((float)(gg_double_bits(UINT64_C(0))));
  v18 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t53 = ((int32_t)(v18));
  t54 = ((int32_t)(v3));
  if (!(((t53) < (t54)))) goto label8;
  {
  t55 = ((uint64_t)(v10));
  t56 = ((int32_t)(((int64_t)UINT64_C(2))));
  t57 = ((int32_t)(v18));
  t58 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t56) * (uint64_t)(t57)))))));
  t59 = fg175v_sm_f32(t55, t58);
  t60 = ((float)(t59));
  t61 = ((float)(v17));
  t62 = ((float)(((t60) - (t61))));
  t63 = fg175v_expf(t62);
  v21 = ((float)(t63));
  t64 = v19;
  t65 = ((float)(v21));
  t66 = ((uint64_t)(v13));
  t67 = ((int32_t)(v18));
  t68 = ((int32_t)(((int64_t)UINT64_C(256))));
  t69 = ((int32_t)(((int32_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int32_t)(v9));
  t71 = ((int32_t)(((int32_t)((uint64_t)(t69) + (uint64_t)(t70)))));
  t72 = ((int32_t)(((int64_t)UINT64_C(4))));
  t73 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t71) * (uint64_t)(t72)))))));
  t74 = fg175v_ld_f32(t66, t73);
  t75 = ((float)(t74));
  t76 = ((float)(((t65) * (t75))));
  v19 = ((t64) + (t76));
  t77 = v20;
  t78 = ((float)(v21));
  t79 = ((uint64_t)(v10));
  t80 = ((int32_t)(((int64_t)UINT64_C(2))));
  t81 = ((int32_t)(v18));
  t82 = ((int32_t)(((int32_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  t83 = ((int32_t)(((int64_t)UINT64_C(1))));
  t84 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t82) + (uint64_t)(t83)))))));
  t85 = fg175v_sm_f32(t79, t84);
  t86 = ((float)(t85));
  t87 = ((float)(((t78) * (t86))));
  v20 = ((t77) + (t87));
  t88 = v18;
  t89 = ((int32_t)(((int64_t)UINT64_C(1))));
  v18 = ((int32_t)((uint64_t)(t88) + (uint64_t)(t89)));
  }
  goto label7;
label8:;
  t90 = ((uint64_t)(v15));
  t91 = ((int32_t)(v9));
  t92 = ((int32_t)(((int64_t)UINT64_C(4))));
  t93 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t91) * (uint64_t)(t92)))))));
  t94 = ((float)(v19));
  t95 = ((float)(v20));
  t96 = ((float)(((t94) / (t95))));
  fg175v_st_f32(t90, t93, t96);
  }
gg_done:;
}
static float gg_function_24(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  int32_t t3;
  float t4;
  float t5;
  float t6;
  float t7;
  int32_t t8;
  float t9;
  float t10;
  float t11;
  float t12;
  int32_t t13;
  float t14;
  float t15;
  float t16;
  float t17;
  int32_t t18;
  float t19;
  float t20;
  float t21;
  float t22;
  int32_t t23;
  float t24;
  float t25;
  float v0;
  float v1;
  v0 = p0;
  {
  v1 = ((float)(v0));
  t1 = v1;
  t2 = ((float)(v1));
  t3 = ((int32_t)(((int64_t)UINT64_C(16))));
  t4 = fg175v_shfl_xor(t2, t3);
  t5 = ((float)(t4));
  v1 = ((t1) + (t5));
  t6 = v1;
  t7 = ((float)(v1));
  t8 = ((int32_t)(((int64_t)UINT64_C(8))));
  t9 = fg175v_shfl_xor(t7, t8);
  t10 = ((float)(t9));
  v1 = ((t6) + (t10));
  t11 = v1;
  t12 = ((float)(v1));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
  t14 = fg175v_shfl_xor(t12, t13);
  t15 = ((float)(t14));
  v1 = ((t11) + (t15));
  t16 = v1;
  t17 = ((float)(v1));
  t18 = ((int32_t)(((int64_t)UINT64_C(2))));
  t19 = fg175v_shfl_xor(t17, t18);
  t20 = ((float)(t19));
  v1 = ((t16) + (t20));
  t21 = v1;
  t22 = ((float)(v1));
  t23 = ((int32_t)(((int64_t)UINT64_C(1))));
  t24 = fg175v_shfl_xor(t22, t23);
  t25 = ((float)(t24));
  v1 = ((t21) + (t25));
  t0 = ((float)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_31(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  int32_t t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float t8;
  int32_t t9;
  float t10;
  float t11;
  float t12;
  float t13;
  float t14;
  int32_t t15;
  float t16;
  float t17;
  float t18;
  float t19;
  float t20;
  int32_t t21;
  float t22;
  float t23;
  float t24;
  float t25;
  float t26;
  int32_t t27;
  float t28;
  float t29;
  float t30;
  float v0;
  float v1;
  v0 = p0;
  {
  v1 = ((float)(v0));
  t1 = ((float)(v1));
  t2 = ((float)(v1));
  t3 = ((int32_t)(((int64_t)UINT64_C(16))));
  t4 = fg175v_shfl_xor(t2, t3);
  t5 = ((float)(t4));
  t6 = fg175v_fmaxf(t1, t5);
  v1 = ((float)(t6));
  t7 = ((float)(v1));
  t8 = ((float)(v1));
  t9 = ((int32_t)(((int64_t)UINT64_C(8))));
  t10 = fg175v_shfl_xor(t8, t9);
  t11 = ((float)(t10));
  t12 = fg175v_fmaxf(t7, t11);
  v1 = ((float)(t12));
  t13 = ((float)(v1));
  t14 = ((float)(v1));
  t15 = ((int32_t)(((int64_t)UINT64_C(4))));
  t16 = fg175v_shfl_xor(t14, t15);
  t17 = ((float)(t16));
  t18 = fg175v_fmaxf(t13, t17);
  v1 = ((float)(t18));
  t19 = ((float)(v1));
  t20 = ((float)(v1));
  t21 = ((int32_t)(((int64_t)UINT64_C(2))));
  t22 = fg175v_shfl_xor(t20, t21);
  t23 = ((float)(t22));
  t24 = fg175v_fmaxf(t19, t23);
  v1 = ((float)(t24));
  t25 = ((float)(v1));
  t26 = ((float)(v1));
  t27 = ((int32_t)(((int64_t)UINT64_C(1))));
  t28 = fg175v_shfl_xor(t26, t27);
  t29 = ((float)(t28));
  t30 = fg175v_fmaxf(t25, t29);
  v1 = ((float)(t30));
  t0 = ((float)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_5(int64_t base) {
  float t0;
  double t1;
  {
  t1 = gg_double_bits(UINT64_C(5179139570939199488));
  t0 = ((float)((-(t1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_13 gg_function_28(uint64_t p0, int64_t p1, int64_t base) {
  gg_type_13 t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint32_t t13;
  uint32_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint32_t t19;
  uint32_t t20;
  uint32_t t21;
  uint32_t t22;
  uint32_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint32_t t30;
  uint32_t t31;
  uint32_t t32;
  uint64_t t33;
  uint64_t t34;
  uint32_t t35;
  uint32_t t36;
  float t37;
  float t38;
  uint32_t t39;
  uint32_t t40;
  float t41;
  float t42;
  float t43;
  float t44;
  uint32_t t45;
  uint32_t t46;
  uint32_t t47;
  uint32_t t48;
  float t49;
  float t50;
  float t51;
  float t52;
  uint32_t t53;
  uint32_t t54;
  uint32_t t55;
  uint32_t t56;
  float t57;
  float t58;
  float t59;
  float t60;
  uint32_t t61;
  uint32_t t62;
  uint32_t t63;
  uint32_t t64;
  float t65;
  float t66;
  float t67;
  uint64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint32_t v4;
  uint32_t v5;
  float v6;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(32))));
  t4 = ((int64_t)(((int64_t)gg_div(t2, t3))));
  t5 = ((int64_t)(((int64_t)UINT64_C(18))));
  t6 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))))));
  v2 = ((uint64_t)(((uint64_t)((uint64_t)(t1) + (uint64_t)(t6)))));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  v3 = ((uint64_t)(((uint64_t)(((int64_t)gg_mod(t7, t8))))));
  t9 = ((uint64_t)(v2));
  t10 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t11 = ((uint64_t)(v3));
  t12 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = fg175v_ld_u16(t9, t12);
  t14 = ((uint32_t)(t13));
  t15 = ((uint64_t)(v2));
  t16 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t17 = ((uint64_t)(v3));
  t18 = ((uint64_t)(((uint64_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  t19 = fg175v_ld_u16(t15, t18);
  t20 = ((uint32_t)(t19));
  t21 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  t22 = ((uint32_t)(((uint32_t)((uint64_t)(t20) << ((uint64_t)(t21) & 31)))));
  v4 = ((uint32_t)(((t14) | (t22))));
  t23 = ((uint32_t)(v4));
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(((int64_t)UINT64_C(32))));
  t26 = ((int64_t)(((int64_t)gg_mod(t24, t25))));
  t27 = ((int64_t)(((int64_t)UINT64_C(16))));
  t28 = ((int64_t)(((int64_t)gg_div(t26, t27))));
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))))));
  t31 = ((uint32_t)(((t23) >> ((uint64_t)(t30) & 31))));
  t32 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(252645135))))));
  v5 = ((uint32_t)(((t31) & (t32))));
  t33 = ((uint64_t)(v2));
  t34 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t35 = fg175v_ld_u16(t33, t34);
  t36 = ((uint32_t)(t35));
  t37 = fg175v_h2f(t36);
  v6 = ((float)(t37));
  memset(&(t0), 0, sizeof(t0));
  t38 = ((float)(v6));
  t39 = ((uint32_t)(v5));
  t40 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  t41 = ((float)(((float)(((t39) & (t40))))));
  t42 = ((float)(gg_double_bits(UINT64_C(4620693217682128896))));
  t43 = ((float)(((t41) - (t42))));
  (t0).f0 = ((float)(((t38) * (t43))));
  t44 = ((float)(v6));
  t45 = ((uint32_t)(v5));
  t46 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(8))))));
  t47 = ((uint32_t)(((t45) >> ((uint64_t)(t46) & 31))));
  t48 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  t49 = ((float)(((float)(((t47) & (t48))))));
  t50 = ((float)(gg_double_bits(UINT64_C(4620693217682128896))));
  t51 = ((float)(((t49) - (t50))));
  (t0).f1 = ((float)(((t44) * (t51))));
  t52 = ((float)(v6));
  t53 = ((uint32_t)(v5));
  t54 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  t55 = ((uint32_t)(((t53) >> ((uint64_t)(t54) & 31))));
  t56 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  t57 = ((float)(((float)(((t55) & (t56))))));
  t58 = ((float)(gg_double_bits(UINT64_C(4620693217682128896))));
  t59 = ((float)(((t57) - (t58))));
  (t0).f2 = ((float)(((t52) * (t59))));
  t60 = ((float)(v6));
  t61 = ((uint32_t)(v5));
  t62 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(24))))));
  t63 = ((uint32_t)(((t61) >> ((uint64_t)(t62) & 31))));
  t64 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  t65 = ((float)(((float)(((t63) & (t64))))));
  t66 = ((float)(gg_double_bits(UINT64_C(4620693217682128896))));
  t67 = ((float)(((t65) - (t66))));
  (t0).f3 = ((float)(((t60) * (t67))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_20(uint64_t p0, int32_t p1, int32_t p2, int64_t base) {
  int32_t t0;
  uint64_t t1;
  uint64_t t2;
  uint32_t t3;
  uint64_t t4;
  int32_t t5;
  int32_t t6;
  uint64_t t7;
  uint32_t t8;
  uint32_t t9;
  uint32_t t10;
  uint32_t t11;
  uint32_t t12;
  uint32_t t13;
  int32_t t14;
  int32_t t15;
  uint32_t t16;
  uint32_t t17;
  uint32_t t18;
  uint64_t v0;
  int32_t v1;
  int32_t v2;
  uint32_t v3;
  uint32_t v4;
  uint32_t v5;
  uint32_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(((uint64_t)(v1))));
  t3 = fg175v_ld_u16(t1, t2);
  v3 = ((uint32_t)(t3));
  t4 = ((uint64_t)(v0));
  t5 = ((int32_t)(v1));
  t6 = ((int32_t)(((int64_t)UINT64_C(2))));
  t7 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t5) + (uint64_t)(t6)))))));
  t8 = fg175v_ld_u16(t4, t7);
  v4 = ((uint32_t)(t8));
  t9 = ((uint32_t)(v3));
  t10 = ((uint32_t)(v4));
  t11 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  t12 = ((uint32_t)(((uint32_t)((uint64_t)(t10) << ((uint64_t)(t11) & 31)))));
  v5 = ((uint32_t)(((t9) | (t12))));
  t13 = ((uint32_t)(v5));
  t14 = ((int32_t)(v2));
  t15 = ((int32_t)(((int64_t)UINT64_C(4))));
  t16 = ((uint32_t)(((uint32_t)(((int32_t)((uint64_t)(t14) * (uint64_t)(t15)))))));
  t17 = ((uint32_t)(((t13) >> ((uint64_t)(t16) & 31))));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(252645135))))));
  v6 = ((uint32_t)(((t17) & (t18))));
  t0 = ((int32_t)(((int32_t)(v6))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_19(uint64_t p0, int32_t p1, int32_t p2, int32_t p3, float p4, float p5, float p6, float p7, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  uint64_t t7;
  int32_t t8;
  int32_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  uint64_t t19;
  int32_t t20;
  int32_t t21;
  int32_t t22;
  int32_t t23;
  int32_t t24;
  int32_t t25;
  int32_t t26;
  int32_t t27;
  int32_t t28;
  int32_t t29;
  uint8_t t30;
  int32_t t31;
  int32_t t32;
  int32_t t33;
  uint64_t t34;
  uint64_t t35;
  uint32_t t36;
  uint32_t t37;
  float t38;
  int32_t t39;
  int32_t t40;
  uint8_t t41;
  int32_t t42;
  int32_t t43;
  uint8_t t44;
  float t45;
  float t46;
  float t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
  int32_t t53;
  int32_t t54;
  uint64_t v0;
  int32_t v1;
  int32_t v2;
  int32_t v3;
  float v4;
  float v5;
  float v6;
  float v7;
  float v8;
  int32_t v9;
  int32_t v10;
  uint64_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  float v16;
  float v17;
  float v18;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  v8 = ((float)(gg_double_bits(UINT64_C(0))));
  v9 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int32_t)(v9));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((int32_t)(v9));
  t4 = ((int32_t)(((int64_t)UINT64_C(32))));
  t5 = ((int32_t)(((int32_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((int32_t)(v1));
  v10 = ((int32_t)(((int32_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t7 = ((uint64_t)(v0));
  t8 = ((int32_t)(v10));
  t9 = ((int32_t)(((int64_t)UINT64_C(8))));
  t10 = ((int64_t)(((int64_t)(((int32_t)gg_div(t8, t9))))));
  t11 = ((int64_t)(((int64_t)UINT64_C(18))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))))));
  v11 = ((uint64_t)(((uint64_t)((uint64_t)(t7) + (uint64_t)(t12)))));
  t13 = ((int32_t)(v10));
  t14 = ((int32_t)(((int64_t)UINT64_C(4))));
  v12 = ((int32_t)(((int32_t)gg_mod(t13, t14))));
  t15 = ((int32_t)(v10));
  t16 = ((int32_t)(((int64_t)UINT64_C(8))));
  t17 = ((int32_t)(((int32_t)gg_mod(t15, t16))));
  t18 = ((int32_t)(((int64_t)UINT64_C(4))));
  v13 = ((int32_t)(((int32_t)gg_div(t17, t18))));
  t19 = ((uint64_t)(v11));
  t20 = ((int32_t)(((int64_t)UINT64_C(2))));
  t21 = ((int32_t)(((int64_t)UINT64_C(4))));
  t22 = ((int32_t)(v12));
  t23 = ((int32_t)(((int32_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((int32_t)(((int32_t)((uint64_t)(t20) + (uint64_t)(t23)))));
  t25 = ((int32_t)(v13));
  v14 = gg_function_20(t19, t24, t25, base + 0);
  t26 = ((int32_t)(v14));
  t28 = ((int32_t)(v9));
  t29 = ((int32_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t27 = ((int32_t)(v2));
  }
  } else {
  {
  t27 = ((int32_t)(v3));
  }
  }
  t31 = ((int32_t)(t27));
  t32 = ((int32_t)(((int64_t)UINT64_C(0))));
  t33 = fg175v_dp4a(t26, t31, t32);
  v15 = ((int32_t)(t33));
  t34 = ((uint64_t)(v11));
  t35 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t36 = fg175v_ld_u16(t34, t35);
  t37 = ((uint32_t)(t36));
  t38 = fg175v_h2f(t37);
  v16 = ((float)(t38));
  t39 = ((int32_t)(v9));
  t40 = ((int32_t)(((int64_t)UINT64_C(0))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  v17 = ((float)(v4));
  }
  } else {
  {
  v17 = ((float)(v6));
  }
  }
  t42 = ((int32_t)(v9));
  t43 = ((int32_t)(((int64_t)UINT64_C(0))));
  t44 = ((t42) == (t43));
  if (t44) {
  {
  v18 = ((float)(v5));
  }
  } else {
  {
  v18 = ((float)(v7));
  }
  }
  t45 = v8;
  t46 = ((float)(v16));
  t47 = ((float)(((float)(v15))));
  t48 = ((float)(v17));
  t49 = ((float)(((t47) * (t48))));
  t50 = ((float)(v18));
  t51 = ((float)(((t49) - (t50))));
  t52 = ((float)(((t46) * (t51))));
  v8 = ((t45) + (t52));
  t53 = v9;
  t54 = ((int32_t)(((int64_t)UINT64_C(1))));
  v9 = ((int32_t)((uint64_t)(t53) + (uint64_t)(t54)));
  }
  goto label1;
label2:;
  t0 = ((float)(v8));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_11(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  float t4;
  float t5;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = ((float)(gg_double_bits(UINT64_C(0))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t4 = ((float)(gg_double_bits(UINT64_C(0))));
  t5 = ((float)(v0));
  t0 = ((float)(((t4) - (t5))));
  goto gg_done;
  }
  }
  t0 = ((float)(v0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_9(uint64_t p0, float p1, int64_t p2, int64_t p3, int32_t p4, uint64_t p5, int64_t base) {
  float t0;
  uint64_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  uint64_t t6;
  float t7;
  float t8;
  float t9;
  uint64_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  uint64_t t17;
  float t18;
  float t19;
  float t20;
  uint64_t t21;
  int32_t t22;
  int32_t t23;
  int32_t t24;
  int32_t t25;
  int32_t t26;
  int32_t t27;
  uint64_t t28;
  float t29;
  float t30;
  float t31;
  uint64_t t32;
  int32_t t33;
  int32_t t34;
  int32_t t35;
  int32_t t36;
  int32_t t37;
  int32_t t38;
  uint64_t t39;
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
  int32_t t63;
  float t64;
  float t65;
  float t66;
  float t67;
  float t68;
  int32_t t69;
  float t70;
  float t71;
  float t72;
  float t73;
  int32_t t74;
  float t75;
  float t76;
  float t77;
  float t78;
  float t79;
  int32_t t80;
  float t81;
  float t82;
  float t83;
  float t84;
  int32_t t85;
  float t86;
  float t87;
  float t88;
  float t89;
  float t90;
  int32_t t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  float t97;
  uint8_t t98;
  float t99;
  float t100;
  float t101;
  int32_t t102;
  float t103;
  float t104;
  float t105;
  int32_t t106;
  float t107;
  float t108;
  float t109;
  int32_t t110;
  float t111;
  float t112;
  float t113;
  int32_t t114;
  uint32_t t115;
  uint32_t t116;
  uint32_t t117;
  uint32_t t118;
  uint32_t t119;
  uint32_t t120;
  uint32_t t121;
  uint32_t t122;
  uint32_t t123;
  uint32_t t124;
  uint32_t t125;
  uint32_t t126;
  uint32_t t127;
  uint32_t t128;
  uint32_t t129;
  uint32_t t130;
  uint32_t t131;
  uint32_t t132;
  uint32_t t133;
  uint32_t t134;
  uint64_t t135;
  int64_t t136;
  int64_t t137;
  uint64_t t138;
  int32_t t139;
  int32_t t140;
  int32_t t141;
  int32_t t142;
  int32_t t143;
  uint8_t t144;
  uint64_t t145;
  int64_t t146;
  int32_t t147;
  int32_t t148;
  int32_t t149;
  int32_t t150;
  int64_t t151;
  uint64_t t152;
  float t153;
  uint64_t t154;
  int64_t t155;
  int32_t t156;
  int32_t t157;
  int32_t t158;
  int32_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  uint64_t t163;
  float t164;
  uint64_t v0;
  float v1;
  int64_t v2;
  int64_t v3;
  int32_t v4;
  uint64_t v5;
  float v6;
  float v7;
  float v8;
  float v9;
  float v10;
  float v11;
  float v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
  uint32_t v18;
  uint32_t v19;
  uint32_t v20;
  uint32_t v21;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((float)(v1));
  t1 = ((uint64_t)(v0));
  t2 = ((int32_t)(((int64_t)UINT64_C(4))));
  t3 = ((int32_t)(v4));
  t4 = ((int32_t)(((int32_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t5 = ((int32_t)(((int64_t)UINT64_C(4))));
  t6 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t4) * (uint64_t)(t5)))))));
  t7 = fg175v_ld_f32(t1, t6);
  t8 = ((float)(t7));
  v6 = ((float)(((t0) * (t8))));
  t9 = ((float)(v1));
  t10 = ((uint64_t)(v0));
  t11 = ((int32_t)(((int64_t)UINT64_C(4))));
  t12 = ((int32_t)(v4));
  t13 = ((int32_t)(((int32_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int32_t)(((int64_t)UINT64_C(1))));
  t15 = ((int32_t)(((int32_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int32_t)(((int64_t)UINT64_C(4))));
  t17 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t15) * (uint64_t)(t16)))))));
  t18 = fg175v_ld_f32(t10, t17);
  t19 = ((float)(t18));
  v7 = ((float)(((t9) * (t19))));
  t20 = ((float)(v1));
  t21 = ((uint64_t)(v0));
  t22 = ((int32_t)(((int64_t)UINT64_C(4))));
  t23 = ((int32_t)(v4));
  t24 = ((int32_t)(((int32_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((int32_t)(((int64_t)UINT64_C(2))));
  t26 = ((int32_t)(((int32_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = ((int32_t)(((int64_t)UINT64_C(4))));
  t28 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t26) * (uint64_t)(t27)))))));
  t29 = fg175v_ld_f32(t21, t28);
  t30 = ((float)(t29));
  v8 = ((float)(((t20) * (t30))));
  t31 = ((float)(v1));
  t32 = ((uint64_t)(v0));
  t33 = ((int32_t)(((int64_t)UINT64_C(4))));
  t34 = ((int32_t)(v4));
  t35 = ((int32_t)(((int32_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((int32_t)(((int64_t)UINT64_C(3))));
  t37 = ((int32_t)(((int32_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t38 = ((int32_t)(((int64_t)UINT64_C(4))));
  t39 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t37) * (uint64_t)(t38)))))));
  t40 = fg175v_ld_f32(t32, t39);
  t41 = ((float)(t40));
  v9 = ((float)(((t31) * (t41))));
  t42 = ((float)(v6));
  v10 = gg_function_11(t42, base + 0);
  v11 = ((float)(v6));
  t43 = ((float)(v10));
  t44 = ((float)(v7));
  t45 = gg_function_11(t44, base + 0);
  t46 = fg175v_fmaxf(t43, t45);
  v10 = ((float)(t46));
  t47 = v11;
  t48 = ((float)(v7));
  v11 = ((t47) + (t48));
  t49 = ((float)(v10));
  t50 = ((float)(v8));
  t51 = gg_function_11(t50, base + 0);
  t52 = fg175v_fmaxf(t49, t51);
  v10 = ((float)(t52));
  t53 = v11;
  t54 = ((float)(v8));
  v11 = ((t53) + (t54));
  t55 = ((float)(v10));
  t56 = ((float)(v9));
  t57 = gg_function_11(t56, base + 0);
  t58 = fg175v_fmaxf(t55, t57);
  v10 = ((float)(t58));
  t59 = v11;
  t60 = ((float)(v9));
  v11 = ((t59) + (t60));
  t61 = ((float)(v10));
  t62 = ((float)(v10));
  t63 = ((int32_t)(((int64_t)UINT64_C(4))));
  t64 = fg175v_shfl_xor(t62, t63);
  t65 = ((float)(t64));
  t66 = fg175v_fmaxf(t61, t65);
  v10 = ((float)(t66));
  t67 = v11;
  t68 = ((float)(v11));
  t69 = ((int32_t)(((int64_t)UINT64_C(4))));
  t70 = fg175v_shfl_xor(t68, t69);
  t71 = ((float)(t70));
  v11 = ((t67) + (t71));
  t72 = ((float)(v10));
  t73 = ((float)(v10));
  t74 = ((int32_t)(((int64_t)UINT64_C(2))));
  t75 = fg175v_shfl_xor(t73, t74);
  t76 = ((float)(t75));
  t77 = fg175v_fmaxf(t72, t76);
  v10 = ((float)(t77));
  t78 = v11;
  t79 = ((float)(v11));
  t80 = ((int32_t)(((int64_t)UINT64_C(2))));
  t81 = fg175v_shfl_xor(t79, t80);
  t82 = ((float)(t81));
  v11 = ((t78) + (t82));
  t83 = ((float)(v10));
  t84 = ((float)(v10));
  t85 = ((int32_t)(((int64_t)UINT64_C(1))));
  t86 = fg175v_shfl_xor(t84, t85);
  t87 = ((float)(t86));
  t88 = fg175v_fmaxf(t83, t87);
  v10 = ((float)(t88));
  t89 = v11;
  t90 = ((float)(v11));
  t91 = ((int32_t)(((int64_t)UINT64_C(1))));
  t92 = fg175v_shfl_xor(t90, t91);
  t93 = ((float)(t92));
  v11 = ((t89) + (t93));
  t94 = ((float)(v10));
  t95 = ((float)(gg_double_bits(UINT64_C(4638637247447433216))));
  v12 = ((float)(((t94) / (t95))));
  v13 = ((int32_t)(((int64_t)UINT64_C(0))));
  t96 = ((float)(v12));
  t97 = ((float)(gg_double_bits(UINT64_C(0))));
  t98 = ((t96) != (t97));
  if (t98) {
  {
  t99 = ((float)(v6));
  t100 = ((float)(v12));
  t101 = ((float)(((t99) / (t100))));
  t102 = fg175v_round_i8(t101);
  v14 = ((int32_t)(t102));
  t103 = ((float)(v7));
  t104 = ((float)(v12));
  t105 = ((float)(((t103) / (t104))));
  t106 = fg175v_round_i8(t105);
  v15 = ((int32_t)(t106));
  t107 = ((float)(v8));
  t108 = ((float)(v12));
  t109 = ((float)(((t107) / (t108))));
  t110 = fg175v_round_i8(t109);
  v16 = ((int32_t)(t110));
  t111 = ((float)(v9));
  t112 = ((float)(v12));
  t113 = ((float)(((t111) / (t112))));
  t114 = fg175v_round_i8(t113);
  v17 = ((int32_t)(t114));
  t115 = ((uint32_t)(((uint32_t)(v14))));
  t116 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v18 = ((uint32_t)(((t115) & (t116))));
  t117 = ((uint32_t)(((uint32_t)(v15))));
  t118 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v19 = ((uint32_t)(((t117) & (t118))));
  t119 = ((uint32_t)(((uint32_t)(v16))));
  t120 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v20 = ((uint32_t)(((t119) & (t120))));
  t121 = ((uint32_t)(((uint32_t)(v17))));
  t122 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v21 = ((uint32_t)(((t121) & (t122))));
  t123 = ((uint32_t)(v18));
  t124 = ((uint32_t)(v19));
  t125 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(8))))));
  t126 = ((uint32_t)(((uint32_t)((uint64_t)(t124) << ((uint64_t)(t125) & 31)))));
  t127 = ((uint32_t)(((t123) | (t126))));
  t128 = ((uint32_t)(v20));
  t129 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  t130 = ((uint32_t)(((uint32_t)((uint64_t)(t128) << ((uint64_t)(t129) & 31)))));
  t131 = ((uint32_t)(((t127) | (t130))));
  t132 = ((uint32_t)(v21));
  t133 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(24))))));
  t134 = ((uint32_t)(((uint32_t)((uint64_t)(t132) << ((uint64_t)(t133) & 31)))));
  v13 = ((int32_t)(((int32_t)(((t131) | (t134))))));
  }
  }
  t135 = ((uint64_t)(v5));
  t136 = ((int64_t)(v2));
  t137 = ((int64_t)(((int64_t)(v4))));
  t138 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t136) + (uint64_t)(t137)))))));
  t139 = ((int32_t)(v13));
  fg175v_sm_st_i32(t135, t138, t139);
  t140 = ((int32_t)(v4));
  t141 = ((int32_t)(((int64_t)UINT64_C(8))));
  t142 = ((int32_t)(((int32_t)gg_mod(t140, t141))));
  t143 = ((int32_t)(((int64_t)UINT64_C(0))));
  t144 = ((t142) == (t143));
  if (t144) {
  {
  t145 = ((uint64_t)(v5));
  t146 = ((int64_t)(v3));
  t147 = ((int32_t)(((int64_t)UINT64_C(2))));
  t148 = ((int32_t)(v4));
  t149 = ((int32_t)(((int64_t)UINT64_C(8))));
  t150 = ((int32_t)(((int32_t)gg_div(t148, t149))));
  t151 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t147) * (uint64_t)(t150)))))));
  t152 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t146) + (uint64_t)(t151)))))));
  t153 = ((float)(v12));
  fg175v_sm_st_f32(t145, t152, t153);
  t154 = ((uint64_t)(v5));
  t155 = ((int64_t)(v3));
  t156 = ((int32_t)(((int64_t)UINT64_C(2))));
  t157 = ((int32_t)(v4));
  t158 = ((int32_t)(((int64_t)UINT64_C(8))));
  t159 = ((int32_t)(((int32_t)gg_div(t157, t158))));
  t160 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t156) * (uint64_t)(t159)))))));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t155) + (uint64_t)(t160)))));
  t162 = ((int64_t)(((int64_t)UINT64_C(1))));
  t163 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t161) + (uint64_t)(t162)))))));
  t164 = ((float)(v11));
  fg175v_sm_st_f32(t154, t163, t164);
  }
  }
  }
gg_done:;
}
static float gg_function_3(float p0, int32_t p1, int32_t p2, float p3, float p4, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  int32_t t4;
  int32_t t5;
  uint8_t t6;
  int32_t t7;
  int32_t t8;
  uint8_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  float t18;
  int32_t t19;
  float t20;
  float v0;
  int32_t v1;
  int32_t v2;
  float v3;
  float v4;
  float v5;
  int32_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((float)(v0));
  t2 = ((float)(gg_double_bits(UINT64_C(0))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t0 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  goto gg_done;
  }
  }
  t4 = ((int32_t)(v1));
  t5 = ((int32_t)(v2));
  t6 = ((t4) < (t5));
  if (t6) {
  {
  v5 = ((float)(v3));
  }
  } else {
  {
  v5 = ((float)(v4));
  }
  }
  t7 = ((int32_t)(v1));
  t8 = ((int32_t)(v2));
  t9 = ((t7) < (t8));
  if (t9) {
  {
  t10 = ((int32_t)(v1));
  t11 = ((int32_t)(((int64_t)UINT64_C(1))));
  v6 = ((int32_t)(((int32_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  }
  } else {
  {
  t12 = ((int32_t)(((int64_t)UINT64_C(2))));
  t13 = ((int32_t)(v1));
  t14 = ((int32_t)(v2));
  t15 = ((int32_t)(((int32_t)((uint64_t)(t13) - (uint64_t)(t14)))));
  t16 = ((int32_t)(((int32_t)((uint64_t)(t12) * (uint64_t)(t15)))));
  t17 = ((int32_t)(((int64_t)UINT64_C(1))));
  v6 = ((int32_t)(((int32_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  }
  }
  t18 = ((float)(v5));
  t19 = ((int32_t)(v6));
  t20 = fg175v_powf(t18, t19);
  t0 = ((float)(t20));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, uint64_t p7, float p8, float p9, float p10, float p11, int32_t p12, float p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t p18, int64_t p19, int64_t p20, int32_t p21, int32_t p22, int32_t p23, int32_t p24, int64_t p25, int64_t p26, int64_t p27, int64_t p28, int64_t p29, int64_t p30, int32_t p31, int32_t p32, int32_t p33, int64_t p34, int64_t p35, int64_t p36, int32_t p37, int32_t p38, int32_t p39, int32_t p40, int32_t p41, int32_t p42, int32_t p43, int32_t p44, uint64_t p45, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  uint64_t t10;
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
  uint64_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int32_t t27;
  int32_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int32_t t37;
  int32_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  uint8_t t44;
  uint64_t t45;
  int64_t t46;
  int32_t t47;
  int32_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  float t55;
  int32_t t56;
  int32_t t57;
  float t58;
  float t59;
  int32_t t60;
  int32_t t61;
  int32_t t62;
  int32_t t63;
  float t64;
  float t65;
  int64_t t66;
  float t67;
  float t68;
  int64_t t69;
  float t70;
  float t71;
  int64_t t72;
  float t73;
  float t74;
  int64_t t75;
  float t76;
  float t77;
  int64_t t78;
  int32_t t79;
  int32_t t80;
  int32_t t81;
  float * t82;
  int32_t t83;
  float * t84;
  int32_t t85;
  int32_t t86;
  int32_t t87;
  int32_t t88;
  uint8_t t89;
  uint8_t t90;
  int32_t t91;
  int32_t t92;
  uint8_t t93;
  uint8_t t94;
  int32_t t95;
  int32_t t96;
  int32_t t97;
  int32_t t98;
  uint8_t t99;
  uint8_t t100;
  int32_t t101;
  int32_t t102;
  int32_t t103;
  int32_t t104;
  uint8_t t105;
  int32_t t106;
  int32_t t107;
  uint64_t t108;
  int32_t t109;
  int32_t t110;
  int32_t t111;
  int32_t t112;
  uint64_t t113;
  int32_t t114;
  int32_t t115;
  int32_t t116;
  int32_t t117;
  int32_t t118;
  uint8_t t119;
  uint64_t t120;
  int32_t t121;
  int32_t t122;
  int32_t t123;
  int32_t t124;
  int32_t t125;
  int32_t t126;
  uint64_t t127;
  float t128;
  uint64_t t129;
  int32_t t130;
  int32_t t131;
  int32_t t132;
  int32_t t133;
  int32_t t134;
  int32_t t135;
  int32_t t136;
  int32_t t137;
  uint64_t t138;
  float t139;
  uint64_t t140;
  int64_t t141;
  int64_t t142;
  uint64_t t143;
  int32_t t144;
  int32_t t145;
  uint64_t t146;
  int32_t t147;
  int32_t t148;
  uint64_t t149;
  uint64_t t150;
  float t151;
  int32_t t152;
  int32_t t153;
  int64_t t154;
  int32_t t155;
  int32_t t156;
  int32_t t157;
  int32_t t158;
  int32_t t159;
  int32_t t160;
  int32_t t161;
  int32_t t162;
  int64_t t163;
  int32_t t164;
  uint64_t t165;
  int32_t t166;
  int32_t t167;
  int32_t t168;
  int32_t t169;
  int64_t t170;
  float t171;
  float t172;
  int64_t t173;
  float t174;
  float t175;
  int64_t t176;
  int32_t t177;
  int32_t t178;
  int32_t t179;
  int32_t t180;
  int32_t t181;
  int32_t t182;
  gg_slice t183;
  int32_t t184;
  int32_t t185;
  int32_t t186;
  int32_t t187;
  int32_t t188;
  int32_t t189;
  int32_t t190;
  int32_t * t191;
  uint64_t t192;
  int32_t t193;
  int32_t t194;
  int32_t t195;
  int32_t t196;
  uint64_t t197;
  int32_t t198;
  int32_t t199;
  int32_t t200;
  int32_t t201;
  int32_t t202;
  int32_t t203;
  int32_t t204;
  int32_t t205;
  int32_t t206;
  int32_t t207;
  int32_t t208;
  gg_slice t209;
  int32_t t210;
  int32_t t211;
  int32_t t212;
  int32_t t213;
  int32_t t214;
  int32_t t215;
  int32_t t216;
  float * t217;
  uint64_t t218;
  uint64_t t219;
  float t220;
  gg_slice t221;
  int32_t t222;
  int32_t t223;
  int32_t t224;
  int32_t t225;
  int32_t t226;
  int32_t t227;
  int32_t t228;
  float * t229;
  uint64_t t230;
  uint64_t t231;
  uint64_t t232;
  uint64_t t233;
  float t234;
  int32_t t235;
  int32_t t236;
  int32_t t237;
  int32_t t238;
  uint64_t t239;
  uint64_t t240;
  uint8_t t241;
  uint64_t t242;
  int32_t t243;
  int32_t t244;
  int32_t t245;
  int32_t t246;
  int32_t t247;
  int32_t t248;
  uint64_t t249;
  int32_t t250;
  uint64_t t251;
  int64_t t252;
  int32_t t253;
  int32_t t254;
  int64_t t255;
  uint64_t t256;
  uint64_t t257;
  int64_t t258;
  int32_t t259;
  int32_t t260;
  int64_t t261;
  uint64_t t262;
  uint64_t t263;
  int32_t t264;
  int32_t t265;
  int32_t t266;
  int32_t t267;
  uint64_t t268;
  int32_t t269;
  int32_t t270;
  int64_t t271;
  int64_t t272;
  float t273;
  float t274;
  int64_t t275;
  float t276;
  float t277;
  int64_t t278;
  int32_t t279;
  int32_t t280;
  int32_t t281;
  float * t282;
  int32_t t283;
  float * t284;
  int32_t t285;
  int32_t t286;
  int32_t t287;
  int32_t t288;
  int32_t t289;
  int32_t t290;
  int32_t t291;
  int32_t t292;
  int32_t t293;
  int32_t t294;
  uint64_t t295;
  int64_t t296;
  int64_t t297;
  uint64_t t298;
  uint64_t t299;
  int32_t t300;
  gg_slice t301;
  int32_t t302;
  int32_t t303;
  int32_t t304;
  int32_t * t305;
  int32_t t306;
  gg_slice t307;
  int32_t t308;
  int32_t t309;
  int32_t t310;
  int32_t t311;
  int32_t t312;
  int32_t * t313;
  int32_t t314;
  gg_slice t315;
  int32_t t316;
  int32_t t317;
  int32_t t318;
  float * t319;
  float t320;
  gg_slice t321;
  int32_t t322;
  int32_t t323;
  int32_t t324;
  float * t325;
  float t326;
  gg_slice t327;
  int32_t t328;
  int32_t t329;
  int32_t t330;
  int32_t t331;
  int32_t t332;
  float * t333;
  float t334;
  gg_slice t335;
  int32_t t336;
  int32_t t337;
  int32_t t338;
  int32_t t339;
  int32_t t340;
  float * t341;
  float t342;
  float t343;
  uint64_t t344;
  uint64_t t345;
  uint8_t t346;
  uint8_t t347;
  int32_t t348;
  int32_t t349;
  uint8_t t350;
  uint8_t t351;
  int32_t t352;
  int32_t t353;
  int32_t t354;
  int32_t t355;
  uint8_t t356;
  float t357;
  float t358;
  uint64_t t359;
  int32_t t360;
  int32_t t361;
  int32_t t362;
  int32_t t363;
  int32_t t364;
  int32_t t365;
  uint64_t t366;
  uint32_t t367;
  uint32_t t368;
  float t369;
  float t370;
  float t371;
  int32_t t372;
  float * t373;
  int32_t t374;
  float * t375;
  float t376;
  float t377;
  float t378;
  float t379;
  float t380;
  float t381;
  float t382;
  int32_t t383;
  int32_t t384;
  uint8_t t385;
  int32_t t386;
  float * t387;
  int32_t t388;
  int32_t t389;
  int32_t t390;
  int32_t t391;
  int32_t t392;
  int32_t t393;
  int32_t t394;
  float * t395;
  float t396;
  int32_t t397;
  float * t398;
  float t399;
  float t400;
  float t401;
  int32_t t402;
  float * t403;
  int32_t t404;
  float * t405;
  int32_t t406;
  float * t407;
  int32_t t408;
  float * t409;
  float t410;
  int32_t t411;
  float * t412;
  float t413;
  float t414;
  float t415;
  int32_t t416;
  float * t417;
  int32_t t418;
  float * t419;
  float t420;
  float t421;
  float t422;
  int32_t t423;
  float * t424;
  float t425;
  uint64_t t426;
  int32_t t427;
  int32_t t428;
  int32_t t429;
  int32_t t430;
  uint64_t t431;
  int32_t t432;
  float * t433;
  float t434;
  int32_t t435;
  int32_t t436;
  gg_slice t437;
  int32_t t438;
  int32_t t439;
  int32_t t440;
  int32_t t441;
  int32_t t442;
  float * t443;
  gg_slice t444;
  int32_t t445;
  int32_t t446;
  int32_t t447;
  int32_t t448;
  int32_t t449;
  float * t450;
  float t451;
  float t452;
  float t453;
  gg_slice t454;
  int32_t t455;
  int32_t t456;
  int32_t t457;
  int32_t t458;
  int32_t t459;
  float * t460;
  gg_slice t461;
  int32_t t462;
  int32_t t463;
  int32_t t464;
  int32_t t465;
  int32_t t466;
  float * t467;
  float t468;
  float t469;
  float t470;
  int32_t t471;
  int32_t t472;
  int32_t t473;
  int32_t t474;
  int32_t t475;
  int32_t t476;
  int32_t t477;
  int32_t t478;
  int32_t t479;
  int32_t t480;
  int32_t t481;
  int32_t t482;
  int32_t t483;
  float * t484;
  uint64_t t485;
  int32_t t486;
  int32_t t487;
  int32_t t488;
  int32_t t489;
  uint64_t t490;
  float t491;
  int32_t t492;
  int32_t t493;
  int32_t t494;
  int32_t t495;
  uint64_t t496;
  int64_t t497;
  int64_t t498;
  uint64_t t499;
  uint64_t t500;
  int32_t t501;
  int32_t t502;
  int32_t t503;
  int32_t t504;
  int32_t t505;
  int32_t t506;
  int64_t t507;
  int32_t t508;
  int32_t t509;
  gg_slice t510;
  int32_t t511;
  int32_t t512;
  int32_t t513;
  int32_t t514;
  int32_t t515;
  int32_t t516;
  int32_t t517;
  float * t518;
  float t519;
  int32_t t520;
  float * t521;
  float t522;
  gg_slice t523;
  int32_t t524;
  int32_t t525;
  int32_t t526;
  int32_t t527;
  int32_t t528;
  int32_t t529;
  int32_t t530;
  float * t531;
  float t532;
  float t533;
  gg_slice t534;
  int32_t t535;
  int32_t t536;
  int32_t t537;
  int32_t t538;
  int32_t t539;
  int32_t t540;
  int32_t t541;
  float * t542;
  float t543;
  int32_t t544;
  float * t545;
  float t546;
  gg_slice t547;
  int32_t t548;
  int32_t t549;
  int32_t t550;
  int32_t t551;
  int32_t t552;
  int32_t t553;
  int32_t t554;
  float * t555;
  float t556;
  float t557;
  gg_slice t558;
  int32_t t559;
  int32_t t560;
  int32_t t561;
  int32_t t562;
  int32_t t563;
  int32_t t564;
  int32_t t565;
  int32_t t566;
  int32_t t567;
  float * t568;
  float t569;
  int32_t t570;
  float * t571;
  float t572;
  gg_slice t573;
  int32_t t574;
  int32_t t575;
  int32_t t576;
  int32_t t577;
  int32_t t578;
  int32_t t579;
  int32_t t580;
  int32_t t581;
  int32_t t582;
  float * t583;
  float t584;
  float t585;
  gg_slice t586;
  int32_t t587;
  int32_t t588;
  int32_t t589;
  int32_t t590;
  int32_t t591;
  int32_t t592;
  int32_t t593;
  int32_t t594;
  int32_t t595;
  float * t596;
  float t597;
  int32_t t598;
  float * t599;
  float t600;
  gg_slice t601;
  int32_t t602;
  int32_t t603;
  int32_t t604;
  int32_t t605;
  int32_t t606;
  int32_t t607;
  int32_t t608;
  int32_t t609;
  int32_t t610;
  float * t611;
  float t612;
  float t613;
  int32_t t614;
  int32_t t615;
  int32_t t616;
  int32_t t617;
  int32_t t618;
  int32_t t619;
  int64_t t620;
  int64_t t621;
  int64_t t622;
  int64_t t623;
  int32_t t624;
  int64_t t625;
  float * t626;
  float t627;
  int64_t t628;
  int64_t t629;
  int64_t t630;
  int64_t t631;
  int64_t t632;
  int64_t t633;
  int32_t t634;
  int64_t t635;
  float * t636;
  float t637;
  int32_t t638;
  int32_t t639;
  int64_t t640;
  int64_t t641;
  int64_t t642;
  int64_t t643;
  int64_t t644;
  int64_t t645;
  int64_t t646;
  int32_t t647;
  int32_t t648;
  int64_t t649;
  int32_t t650;
  float t651;
  int32_t t652;
  int32_t t653;
  uint8_t t654;
  int64_t t655;
  float * t656;
  int32_t t657;
  float * t658;
  float t659;
  int64_t t660;
  int64_t t661;
  int64_t t662;
  int64_t t663;
  int64_t t664;
  int64_t t665;
  int64_t t666;
  int32_t t667;
  int32_t t668;
  int64_t t669;
  int32_t t670;
  int32_t t671;
  float * t672;
  float t673;
  int32_t t674;
  int32_t t675;
  int64_t t676;
  int32_t t677;
  int32_t t678;
  int64_t t679;
  uint64_t t680;
  int32_t t681;
  int32_t t682;
  int64_t t683;
  int64_t t684;
  uint64_t t685;
  uint64_t t686;
  int32_t t687;
  int32_t t688;
  int64_t t689;
  int64_t t690;
  uint64_t t691;
  uint64_t t692;
  int32_t t693;
  int32_t t694;
  int32_t t695;
  int32_t t696;
  uint64_t t697;
  uint64_t t698;
  uint64_t t699;
  uint8_t t700;
  uint8_t t701;
  int32_t t702;
  int32_t t703;
  uint8_t t704;
  uint64_t t705;
  int32_t t706;
  int32_t t707;
  uint64_t t708;
  float t709;
  int32_t t710;
  int32_t t711;
  uint8_t t712;
  uint8_t t713;
  int32_t t714;
  int32_t t715;
  uint8_t t716;
  uint8_t t717;
  float t718;
  int32_t t719;
  float * t720;
  float t721;
  float t722;
  int32_t t723;
  float * t724;
  float t725;
  float t726;
  float t727;
  float t728;
  int32_t t729;
  float * t730;
  int32_t t731;
  float * t732;
  int32_t t733;
  float * t734;
  float t735;
  float t736;
  float t737;
  float t738;
  int32_t t739;
  int32_t t740;
  uint8_t t741;
  float t742;
  int32_t t743;
  float * t744;
  float t745;
  float t746;
  float t747;
  float t748;
  int32_t t749;
  int32_t t750;
  gg_slice t751;
  int32_t t752;
  int32_t t753;
  int32_t t754;
  int32_t t755;
  int32_t t756;
  float * t757;
  gg_slice t758;
  int32_t t759;
  int32_t t760;
  int32_t t761;
  int32_t t762;
  int32_t t763;
  float * t764;
  float t765;
  float t766;
  float t767;
  gg_slice t768;
  int32_t t769;
  int32_t t770;
  int32_t t771;
  int32_t t772;
  int32_t t773;
  float * t774;
  gg_slice t775;
  int32_t t776;
  int32_t t777;
  int32_t t778;
  int32_t t779;
  int32_t t780;
  float * t781;
  float t782;
  float t783;
  float t784;
  int32_t t785;
  int32_t t786;
  int32_t t787;
  int32_t t788;
  int32_t t789;
  int32_t t790;
  uint8_t t791;
  uint64_t t792;
  int32_t t793;
  int32_t t794;
  int32_t t795;
  int32_t t796;
  int32_t t797;
  int32_t t798;
  uint64_t t799;
  float t800;
  uint64_t t801;
  int32_t t802;
  int32_t t803;
  int32_t t804;
  int32_t t805;
  int32_t t806;
  int32_t t807;
  uint64_t t808;
  float t809;
  int32_t t810;
  int32_t t811;
  int32_t t812;
  int32_t t813;
  int32_t t814;
  int32_t t815;
  uint8_t t816;
  uint64_t t817;
  int32_t t818;
  int32_t t819;
  int32_t t820;
  int32_t t821;
  int32_t t822;
  int32_t t823;
  uint64_t t824;
  int32_t t825;
  float * t826;
  float t827;
  int32_t t828;
  int32_t t829;
  int32_t t830;
  int32_t t831;
  uint8_t t832;
  uint8_t t833;
  int32_t t834;
  int32_t t835;
  int32_t t836;
  int32_t t837;
  uint8_t t838;
  int32_t t839;
  int32_t t840;
  int32_t t841;
  int32_t t842;
  uint64_t t843;
  int32_t t844;
  int32_t t845;
  int32_t t846;
  int32_t t847;
  int32_t t848;
  int32_t t849;
  uint64_t t850;
  float t851;
  float t852;
  int32_t t853;
  float * t854;
  float t855;
  float t856;
  float t857;
  float t858;
  int32_t t859;
  float * t860;
  int32_t t861;
  int32_t t862;
  gg_slice t863;
  int32_t t864;
  int32_t t865;
  int32_t t866;
  int32_t t867;
  int32_t t868;
  float * t869;
  gg_slice t870;
  int32_t t871;
  int32_t t872;
  int32_t t873;
  int32_t t874;
  int32_t t875;
  float * t876;
  float t877;
  float t878;
  float t879;
  gg_slice t880;
  int32_t t881;
  int32_t t882;
  int32_t t883;
  int32_t t884;
  int32_t t885;
  float * t886;
  gg_slice t887;
  int32_t t888;
  int32_t t889;
  int32_t t890;
  int32_t t891;
  int32_t t892;
  float * t893;
  float t894;
  float t895;
  float t896;
  int32_t t897;
  int32_t t898;
  int32_t t899;
  int32_t t900;
  int32_t t901;
  int32_t t902;
  int32_t t903;
  int32_t t904;
  uint64_t t905;
  int32_t t906;
  int32_t t907;
  int32_t t908;
  int32_t t909;
  int32_t t910;
  int32_t t911;
  uint64_t t912;
  gg_slice t913;
  int32_t t914;
  int32_t t915;
  int32_t t916;
  int32_t t917;
  int32_t t918;
  int32_t t919;
  int32_t t920;
  float * t921;
  float t922;
  uint64_t t923;
  int32_t t924;
  int32_t t925;
  int32_t t926;
  int32_t t927;
  int32_t t928;
  int32_t t929;
  int32_t t930;
  int32_t t931;
  uint64_t t932;
  gg_slice t933;
  int32_t t934;
  int32_t t935;
  int32_t t936;
  int32_t t937;
  int32_t t938;
  int32_t t939;
  int32_t t940;
  float * t941;
  float t942;
  uint64_t t943;
  int32_t t944;
  int32_t t945;
  int32_t t946;
  int32_t t947;
  int32_t t948;
  int32_t t949;
  int32_t t950;
  int32_t t951;
  uint64_t t952;
  gg_slice t953;
  int32_t t954;
  int32_t t955;
  int32_t t956;
  int32_t t957;
  int32_t t958;
  int32_t t959;
  int32_t t960;
  int32_t t961;
  int32_t t962;
  float * t963;
  float t964;
  uint64_t t965;
  int32_t t966;
  int32_t t967;
  int32_t t968;
  int32_t t969;
  int32_t t970;
  int32_t t971;
  int32_t t972;
  int32_t t973;
  uint64_t t974;
  gg_slice t975;
  int32_t t976;
  int32_t t977;
  int32_t t978;
  int32_t t979;
  int32_t t980;
  int32_t t981;
  int32_t t982;
  int32_t t983;
  int32_t t984;
  float * t985;
  float t986;
  int32_t t987;
  int32_t t988;
  int32_t t989;
  float * t990;
  float t991;
  float t992;
  int32_t t993;
  float * t994;
  int32_t t995;
  float * t996;
  float t997;
  int32_t t998;
  int32_t t999;
  uint8_t t1000;
  uint64_t t1001;
  int32_t t1002;
  int32_t t1003;
  int32_t t1004;
  int32_t t1005;
  int32_t t1006;
  int32_t t1007;
  uint64_t t1008;
  int32_t t1009;
  float * t1010;
  float t1011;
  int32_t t1012;
  float * t1013;
  uint64_t t1014;
  int32_t t1015;
  int32_t t1016;
  int32_t t1017;
  int32_t t1018;
  int32_t t1019;
  int32_t t1020;
  uint64_t t1021;
  float t1022;
  int32_t t1023;
  float * t1024;
  int32_t t1025;
  float * t1026;
  float t1027;
  int32_t t1028;
  int32_t t1029;
  int32_t t1030;
  int32_t t1031;
  float t1032;
  uint64_t t1033;
  int32_t t1034;
  int32_t t1035;
  int32_t t1036;
  int32_t t1037;
  int32_t t1038;
  int32_t t1039;
  uint64_t t1040;
  float t1041;
  float t1042;
  int32_t t1043;
  int32_t t1044;
  int32_t t1045;
  int32_t t1046;
  uint8_t t1047;
  float t1048;
  int32_t t1049;
  float * t1050;
  float t1051;
  int64_t t1052;
  int64_t t1053;
  int64_t t1054;
  int32_t t1055;
  int32_t t1056;
  int64_t t1057;
  int64_t t1058;
  int64_t t1059;
  int64_t t1060;
  int64_t t1061;
  int64_t t1062;
  int64_t t1063;
  int64_t t1064;
  int64_t t1065;
  uint64_t t1066;
  int64_t t1067;
  int64_t t1068;
  int64_t t1069;
  int32_t t1070;
  int32_t t1071;
  int64_t t1072;
  int64_t t1073;
  int64_t t1074;
  uint64_t t1075;
  float t1076;
  int32_t t1077;
  int32_t t1078;
  int32_t t1079;
  int32_t t1080;
  int32_t t1081;
  int32_t t1082;
  uint8_t t1083;
  int32_t t1084;
  int32_t t1085;
  int32_t t1086;
  int32_t t1087;
  uint8_t t1088;
  uint8_t t1089;
  int32_t t1090;
  int32_t t1091;
  uint8_t t1092;
  uint8_t t1093;
  int32_t t1094;
  int32_t t1095;
  uint8_t t1096;
  uint8_t t1097;
  int32_t t1098;
  int32_t t1099;
  int32_t t1100;
  int32_t t1101;
  uint8_t t1102;
  int64_t t1103;
  int64_t t1104;
  int64_t t1105;
  int32_t t1106;
  int32_t t1107;
  int64_t t1108;
  int64_t t1109;
  int64_t t1110;
  int64_t t1111;
  int64_t t1112;
  int64_t t1113;
  int64_t t1114;
  int64_t t1115;
  int64_t t1116;
  uint64_t t1117;
  int64_t t1118;
  int64_t t1119;
  uint64_t t1120;
  int32_t t1121;
  float * t1122;
  float t1123;
  uint64_t t1124;
  int64_t t1125;
  int64_t t1126;
  int64_t t1127;
  int64_t t1128;
  uint64_t t1129;
  int32_t t1130;
  float * t1131;
  float t1132;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
  float v8;
  float v9;
  float v10;
  float v11;
  int32_t v12;
  float v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int32_t v21;
  int32_t v22;
  int32_t v23;
  int32_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  int32_t v31;
  int32_t v32;
  int32_t v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
  int32_t v37;
  int32_t v38;
  int32_t v39;
  int32_t v40;
  int32_t v41;
  int32_t v42;
  int32_t v43;
  int32_t v44;
  uint64_t v45;
  int32_t v46;
  int32_t v47;
  int32_t v48;
  int32_t v49;
  uint64_t v50;
  uint64_t v51;
  uint64_t v52;
  uint64_t v53;
  float v54;
  int32_t v55;
  gg_type_8 v56;
  gg_type_8 v57;
  gg_type_9 v58;
  gg_type_9 v59;
  gg_type_8 v60;
  int32_t v61;
  int32_t v62;
  int32_t v63;
  int32_t v64;
  uint64_t v65;
  int32_t v66;
  gg_type_11 v67;
  gg_type_12 v68;
  gg_type_12 v69;
  int32_t v70;
  int32_t v71;
  uint64_t v72;
  int32_t v73;
  uint64_t v74;
  uint64_t v75;
  uint64_t v76;
  int64_t v77;
  gg_type_8 v78;
  gg_type_8 v79;
  int32_t v80;
  int32_t v81;
  float v82;
  float v83;
  int32_t v84;
  int32_t v85;
  int32_t v86;
  int32_t v87;
  gg_type_13 v88;
  int32_t v89;
  float v90;
  int32_t v91;
  float v92;
  float v93;
  int32_t v94;
  int32_t v95;
  float v96;
  float v97;
  int32_t v98;
  int32_t v99;
  int32_t v100;
  int32_t v101;
  float v102;
  int32_t v103;
  int64_t v104;
  int64_t v105;
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
  v18 = p18;
  v19 = p19;
  v20 = p20;
  v21 = p21;
  v22 = p22;
  v23 = p23;
  v24 = p24;
  v25 = p25;
  v26 = p26;
  v27 = p27;
  v28 = p28;
  v29 = p29;
  v30 = p30;
  v31 = p31;
  v32 = p32;
  v33 = p33;
  v34 = p34;
  v35 = p35;
  v36 = p36;
  v37 = p37;
  v38 = p38;
  v39 = p39;
  v40 = p40;
  v41 = p41;
  v42 = p42;
  v43 = p43;
  v44 = p44;
  v45 = p45;
  {
  fg175v_lc();
  t0 = ((int32_t)(v38));
  t1 = ((int32_t)(v37));
  v46 = ((int32_t)(((int32_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  t2 = ((int32_t)(v40));
  t3 = ((int32_t)(v16));
  v47 = ((int32_t)(((int32_t)gg_div(t2, t3))));
  t4 = ((int32_t)(v40));
  t5 = ((int32_t)(v47));
  t6 = ((int32_t)(v16));
  t7 = ((int32_t)(((int32_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  v48 = ((int32_t)(((int32_t)((uint64_t)(t4) - (uint64_t)(t7)))));
  t8 = ((int32_t)(v16));
  t9 = ((int32_t)(v23));
  v49 = ((int32_t)(((int32_t)gg_div(t8, t9))));
  t10 = ((uint64_t)(v0));
  t11 = ((int64_t)(v20));
  t12 = ((int64_t)(((int64_t)(v47))));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(v19));
  t15 = ((int64_t)(((int64_t)(v48))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t16)))));
  t18 = ((int64_t)(v18));
  t19 = ((int64_t)(((int64_t)(v46))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t21 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t20)))))));
  v50 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t21)))));
  t22 = ((uint64_t)(v1));
  t23 = ((int64_t)(v27));
  t24 = ((int64_t)(((int64_t)(v47))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t26 = ((int64_t)(v26));
  t27 = ((int32_t)(v48));
  t28 = ((int32_t)(v49));
  t29 = ((int64_t)(((int64_t)(((int32_t)gg_div(t27, t28))))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t29)))));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t30)))))));
  v51 = ((uint64_t)(((uint64_t)((uint64_t)(t22) + (uint64_t)(t31)))));
  t32 = ((uint64_t)(v2));
  t33 = ((int64_t)(v30));
  t34 = ((int64_t)(((int64_t)(v47))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((int64_t)(v29));
  t37 = ((int32_t)(v48));
  t38 = ((int32_t)(v49));
  t39 = ((int64_t)(((int64_t)(((int32_t)gg_div(t37, t38))))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t39)))));
  t41 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t40)))))));
  v52 = ((uint64_t)(((uint64_t)((uint64_t)(t32) + (uint64_t)(t41)))));
  t42 = ((uint64_t)(v3));
  t43 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t44 = ((t42) != (t43));
  if (t44) {
  {
  t45 = ((uint64_t)(v3));
  t46 = ((int64_t)(v36));
  t47 = ((int32_t)(v47));
  t48 = ((int32_t)(v33));
  t49 = ((int64_t)(((int64_t)(((int32_t)gg_mod(t47, t48))))));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t49)))));
  t51 = ((int64_t)(v34));
  t52 = ((int64_t)(((int64_t)(v46))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t54 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t53)))))));
  v53 = ((uint64_t)(((uint64_t)((uint64_t)(t45) + (uint64_t)(t54)))));
  }
  } else {
  {
  v53 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t55 = ((float)(v9));
  t56 = ((int32_t)(v48));
  t57 = ((int32_t)(v12));
  t58 = ((float)(v10));
  t59 = ((float)(v11));
  v54 = gg_function_3(t55, t56, t57, t58, t59, base + 0);
  t60 = ((int32_t)(((int64_t)UINT64_C(32))));
  t61 = ((int32_t)(v44));
  t62 = ((int32_t)(((int32_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = ((int32_t)(v43));
  v55 = ((int32_t)(((int32_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  memset(&t64, 0, sizeof(t64));
  t65 = ((float)(t64));
  for (t66=0; t66<2; t66++) (v56).data[t66] = t65;
  memset(&t67, 0, sizeof(t67));
  t68 = ((float)(t67));
  for (t69=0; t69<2; t69++) (v57).data[t69] = t68;
  memset(&t70, 0, sizeof(t70));
  t71 = ((float)(t70));
  for (t72=0; t72<8; t72++) (v58).data[t72] = t71;
  memset(&t73, 0, sizeof(t73));
  t74 = ((float)(t73));
  for (t75=0; t75<8; t75++) (v59).data[t75] = t74;
  memset(&t76, 0, sizeof(t76));
  t77 = ((float)(t76));
  for (t78=0; t78<2; t78++) (v60).data[t78] = t77;
  v61 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t79 = ((int32_t)(v61));
  t80 = ((int32_t)(v37));
  if (!(((t79) < (t80)))) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t81 = v61;
  t82 = ((float *)(v56).data) + gg_index(t81, ((int64_t)UINT64_C(2)));
  (*t82) = gg_function_5(base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t83 = v61;
  t84 = ((float *)(v57).data) + gg_index(t83, ((int64_t)UINT64_C(2)));
  (*t84) = ((float)(gg_double_bits(UINT64_C(0))));
  t85 = v61;
  t86 = ((int32_t)(((int64_t)UINT64_C(1))));
  v61 = ((int32_t)((uint64_t)(t85) + (uint64_t)(t86)));
  }
  goto label1;
label2:;
  fg175v_sync();
  v62 = ((int32_t)(v44));
  t87 = ((int32_t)(((int64_t)UINT64_C(4))));
  t88 = ((int32_t)(v37));
  t89 = ((t87) > (t88));
  t90 = t89;
  if (t90) {
  t91 = ((int32_t)(v62));
  t92 = ((int32_t)(v37));
  t90 = ((t91) >= (t92));
  }
  t93 = t90;
  t94 = (!(t93));
  if (t94) {
  {
  t95 = ((int32_t)(v62));
  t96 = ((int32_t)(((int64_t)UINT64_C(256))));
  v63 = ((int32_t)(((int32_t)((uint64_t)(t95) * (uint64_t)(t96)))));
  t97 = ((int32_t)(v37));
  t98 = ((int32_t)(((int64_t)UINT64_C(1))));
  t99 = ((t97) > (t98));
  t100 = t99;
  if (t100) {
  t101 = ((int32_t)(v46));
  t102 = ((int32_t)(v62));
  t103 = ((int32_t)(((int32_t)((uint64_t)(t101) + (uint64_t)(t102)))));
  t104 = ((int32_t)(v15));
  t100 = ((t103) >= (t104));
  }
  t105 = t100;
  if (t105) {
  {
  v64 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t106 = ((int32_t)(v64));
  t107 = ((int32_t)(((int64_t)UINT64_C(64))));
  if (!(((t106) < (t107)))) goto label5;
  {
  t108 = ((uint64_t)(v45));
  t109 = ((int32_t)(v63));
  t110 = ((int32_t)(v64));
  t111 = ((int32_t)(((int32_t)((uint64_t)(t109) + (uint64_t)(t110)))));
  t112 = ((int32_t)(v43));
  t113 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t111) + (uint64_t)(t112)))))));
  t114 = ((int32_t)(((int64_t)UINT64_C(0))));
  fg175v_sm_st_i32(t108, t113, t114);
  t115 = v64;
  t116 = ((int32_t)(((int64_t)UINT64_C(32))));
  v64 = ((int32_t)((uint64_t)(t115) + (uint64_t)(t116)));
  }
  goto label4;
label5:;
  t117 = ((int32_t)(v43));
  t118 = ((int32_t)(((int64_t)UINT64_C(8))));
  t119 = ((t117) < (t118));
  if (t119) {
  {
  t120 = ((uint64_t)(v45));
  t121 = ((int32_t)(v63));
  t122 = ((int32_t)(((int64_t)UINT64_C(64))));
  t123 = ((int32_t)(((int32_t)((uint64_t)(t121) + (uint64_t)(t122)))));
  t124 = ((int32_t)(((int64_t)UINT64_C(2))));
  t125 = ((int32_t)(v43));
  t126 = ((int32_t)(((int32_t)((uint64_t)(t124) * (uint64_t)(t125)))));
  t127 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t123) + (uint64_t)(t126)))))));
  t128 = ((float)(gg_double_bits(UINT64_C(0))));
  fg175v_sm_st_f32(t120, t127, t128);
  t129 = ((uint64_t)(v45));
  t130 = ((int32_t)(v63));
  t131 = ((int32_t)(((int64_t)UINT64_C(64))));
  t132 = ((int32_t)(((int32_t)((uint64_t)(t130) + (uint64_t)(t131)))));
  t133 = ((int32_t)(((int64_t)UINT64_C(2))));
  t134 = ((int32_t)(v43));
  t135 = ((int32_t)(((int32_t)((uint64_t)(t133) * (uint64_t)(t134)))));
  t136 = ((int32_t)(((int32_t)((uint64_t)(t132) + (uint64_t)(t135)))));
  t137 = ((int32_t)(((int64_t)UINT64_C(1))));
  t138 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t136) + (uint64_t)(t137)))))));
  t139 = ((float)(gg_double_bits(UINT64_C(0))));
  fg175v_sm_st_f32(t129, t138, t139);
  }
  }
  }
  } else {
  {
  t140 = ((uint64_t)(v50));
  t141 = ((int64_t)(v18));
  t142 = ((int64_t)(((int64_t)(v62))));
  t143 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t141) * (uint64_t)(t142)))))));
  v65 = ((uint64_t)(((uint64_t)((uint64_t)(t140) + (uint64_t)(t143)))));
  v66 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t144 = ((int32_t)(v66));
  t145 = ((int32_t)(((int64_t)UINT64_C(64))));
  if (!(((t144) < (t145)))) goto label8;
  {
  t146 = ((uint64_t)(v65));
  t147 = ((int32_t)(v66));
  t148 = ((int32_t)(((int64_t)UINT64_C(16))));
  t149 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t147) * (uint64_t)(t148)))))));
  t150 = ((uint64_t)(((uint64_t)((uint64_t)(t146) + (uint64_t)(t149)))));
  t151 = ((float)(v8));
  t152 = ((int32_t)(v63));
  t153 = ((int32_t)(v66));
  t154 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t152) + (uint64_t)(t153)))))));
  t155 = ((int32_t)(v63));
  t156 = ((int32_t)(((int64_t)UINT64_C(64))));
  t157 = ((int32_t)(((int32_t)((uint64_t)(t155) + (uint64_t)(t156)))));
  t158 = ((int32_t)(((int64_t)UINT64_C(2))));
  t159 = ((int32_t)(v66));
  t160 = ((int32_t)(((int64_t)UINT64_C(8))));
  t161 = ((int32_t)(((int32_t)gg_div(t159, t160))));
  t162 = ((int32_t)(((int32_t)((uint64_t)(t158) * (uint64_t)(t161)))));
  t163 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t157) + (uint64_t)(t162)))))));
  t164 = ((int32_t)(v43));
  t165 = ((uint64_t)(v45));
  gg_function_9(t150, t151, t154, t163, t164, t165, base + 0);
  t166 = v66;
  t167 = ((int32_t)(((int64_t)UINT64_C(32))));
  v66 = ((int32_t)((uint64_t)(t166) + (uint64_t)(t167)));
  }
  goto label7;
label8:;
  }
  }
  }
  }
  fg175v_barrier();
  memset(&t168, 0, sizeof(t168));
  t169 = ((int32_t)(t168));
  for (t170=0; t170<4; t170++) (v67).data[t170] = t169;
  memset(&t171, 0, sizeof(t171));
  t172 = ((float)(t171));
  for (t173=0; t173<4; t173++) (v68).data[t173] = t172;
  memset(&t174, 0, sizeof(t174));
  t175 = ((float)(t174));
  for (t176=0; t176<4; t176++) (v69).data[t176] = t175;
  v61 = ((int32_t)(((int64_t)UINT64_C(0))));
label10:;
  t177 = ((int32_t)(v61));
  t178 = ((int32_t)(v37));
  if (!(((t177) < (t178)))) goto label11;
  {
  v70 = ((int32_t)(((int64_t)UINT64_C(0))));
label13:;
  t179 = ((int32_t)(v70));
  t180 = ((int32_t)(((int64_t)UINT64_C(64))));
  if (!(((t179) < (t180)))) goto label14;
  {
  t181 = ((int32_t)(v70));
  t182 = ((int32_t)(v43));
  v71 = ((int32_t)(((int32_t)((uint64_t)(t181) + (uint64_t)(t182)))));
  t183 = gg_view((v67).data, 4);
  t184 = ((int32_t)(v61));
  t185 = ((int32_t)(((int64_t)UINT64_C(2))));
  t186 = ((int32_t)(((int32_t)((uint64_t)(t184) * (uint64_t)(t185)))));
  t187 = ((int32_t)(v70));
  t188 = ((int32_t)(((int64_t)UINT64_C(32))));
  t189 = ((int32_t)(((int32_t)gg_div(t187, t188))));
  t190 = ((int32_t)((uint64_t)(t186) + (uint64_t)(t189)));
  t191 = ((int32_t *)t183.data) + gg_index(t190, t183.len);
  t192 = ((uint64_t)(v45));
  t193 = ((int32_t)(v61));
  t194 = ((int32_t)(((int64_t)UINT64_C(256))));
  t195 = ((int32_t)(((int32_t)((uint64_t)(t193) * (uint64_t)(t194)))));
  t196 = ((int32_t)(v71));
  t197 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t195) + (uint64_t)(t196)))))));
  t198 = fg175v_sm_i32(t192, t197);
  (*t191) = ((int32_t)(t198));
  t199 = ((int32_t)(v61));
  t200 = ((int32_t)(((int64_t)UINT64_C(256))));
  t201 = ((int32_t)(((int32_t)((uint64_t)(t199) * (uint64_t)(t200)))));
  t202 = ((int32_t)(((int64_t)UINT64_C(64))));
  t203 = ((int32_t)(((int32_t)((uint64_t)(t201) + (uint64_t)(t202)))));
  t204 = ((int32_t)(((int64_t)UINT64_C(2))));
  t205 = ((int32_t)(v71));
  t206 = ((int32_t)(((int64_t)UINT64_C(8))));
  t207 = ((int32_t)(((int32_t)gg_div(t205, t206))));
  t208 = ((int32_t)(((int32_t)((uint64_t)(t204) * (uint64_t)(t207)))));
  v72 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t203) + (uint64_t)(t208)))))));
  t209 = gg_view((v68).data, 4);
  t210 = ((int32_t)(v61));
  t211 = ((int32_t)(((int64_t)UINT64_C(2))));
  t212 = ((int32_t)(((int32_t)((uint64_t)(t210) * (uint64_t)(t211)))));
  t213 = ((int32_t)(v70));
  t214 = ((int32_t)(((int64_t)UINT64_C(32))));
  t215 = ((int32_t)(((int32_t)gg_div(t213, t214))));
  t216 = ((int32_t)((uint64_t)(t212) + (uint64_t)(t215)));
  t217 = ((float *)t209.data) + gg_index(t216, t209.len);
  t218 = ((uint64_t)(v45));
  t219 = ((uint64_t)(v72));
  t220 = fg175v_sm_f32(t218, t219);
  (*t217) = ((float)(t220));
  t221 = gg_view((v69).data, 4);
  t222 = ((int32_t)(v61));
  t223 = ((int32_t)(((int64_t)UINT64_C(2))));
  t224 = ((int32_t)(((int32_t)((uint64_t)(t222) * (uint64_t)(t223)))));
  t225 = ((int32_t)(v70));
  t226 = ((int32_t)(((int64_t)UINT64_C(32))));
  t227 = ((int32_t)(((int32_t)gg_div(t225, t226))));
  t228 = ((int32_t)((uint64_t)(t224) + (uint64_t)(t227)));
  t229 = ((float *)t221.data) + gg_index(t228, t221.len);
  t230 = ((uint64_t)(v45));
  t231 = ((uint64_t)(v72));
  t232 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t233 = ((uint64_t)(((uint64_t)((uint64_t)(t231) + (uint64_t)(t232)))));
  t234 = fg175v_sm_f32(t230, t233);
  (*t229) = ((float)(t234));
  t235 = v70;
  t236 = ((int32_t)(((int64_t)UINT64_C(32))));
  v70 = ((int32_t)((uint64_t)(t235) + (uint64_t)(t236)));
  }
  goto label13;
label14:;
  t237 = v61;
  t238 = ((int32_t)(((int64_t)UINT64_C(1))));
  v61 = ((int32_t)((uint64_t)(t237) + (uint64_t)(t238)));
  }
  goto label10;
label11:;
  fg175v_barrier();
  v73 = ((int32_t)(v22));
  t239 = ((uint64_t)(v5));
  t240 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t241 = ((t239) != (t240));
  if (t241) {
  {
  t242 = ((uint64_t)(v5));
  t243 = ((int32_t)(v47));
  t244 = ((int32_t)(v41));
  t245 = ((int32_t)(((int32_t)((uint64_t)(t243) * (uint64_t)(t244)))));
  t246 = ((int32_t)(v38));
  t247 = ((int32_t)(((int32_t)((uint64_t)(t245) + (uint64_t)(t246)))));
  t248 = ((int32_t)(((int64_t)UINT64_C(4))));
  t249 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t247) * (uint64_t)(t248)))))));
  t250 = fg175v_ld_i32(t242, t249);
  v73 = ((int32_t)(t250));
  }
  }
  t251 = ((uint64_t)(v51));
  t252 = ((int64_t)(v25));
  t253 = ((int32_t)(v39));
  t254 = ((int32_t)(((int64_t)UINT64_C(128))));
  t255 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t253) * (uint64_t)(t254)))))));
  t256 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t252) * (uint64_t)(t255)))))));
  v74 = ((uint64_t)(((uint64_t)((uint64_t)(t251) + (uint64_t)(t256)))));
  t257 = ((uint64_t)(v52));
  t258 = ((int64_t)(v28));
  t259 = ((int32_t)(v39));
  t260 = ((int32_t)(((int64_t)UINT64_C(128))));
  t261 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t259) * (uint64_t)(t260)))))));
  t262 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t258) * (uint64_t)(t261)))))));
  v75 = ((uint64_t)(((uint64_t)((uint64_t)(t257) + (uint64_t)(t262)))));
  t263 = ((uint64_t)(v53));
  t264 = ((int32_t)(v39));
  t265 = ((int32_t)(((int64_t)UINT64_C(128))));
  t266 = ((int32_t)(((int32_t)((uint64_t)(t264) * (uint64_t)(t265)))));
  t267 = ((int32_t)(((int64_t)UINT64_C(2))));
  t268 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t266) * (uint64_t)(t267)))))));
  v76 = ((uint64_t)(((uint64_t)((uint64_t)(t263) + (uint64_t)(t268)))));
  t269 = ((int32_t)(v39));
  t270 = ((int32_t)(((int64_t)UINT64_C(128))));
  v77 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t269) * (uint64_t)(t270)))))));
label16:;
  t271 = ((int64_t)(v77));
  t272 = ((int64_t)(((int64_t)(v73))));
  if (!(((t271) < (t272)))) goto label17;
  {
  memset(&t273, 0, sizeof(t273));
  t274 = ((float)(t273));
  for (t275=0; t275<2; t275++) (v78).data[t275] = t274;
  memset(&t276, 0, sizeof(t276));
  t277 = ((float)(t276));
  for (t278=0; t278<2; t278++) (v79).data[t278] = t277;
  v61 = ((int32_t)(((int64_t)UINT64_C(0))));
label19:;
  t279 = ((int32_t)(v61));
  t280 = ((int32_t)(v37));
  if (!(((t279) < (t280)))) goto label20;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t281 = v61;
  t282 = ((float *)(v79).data) + gg_index(t281, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t283 = v61;
  t284 = ((float *)(v56).data) + gg_index(t283, ((int64_t)UINT64_C(2)));
  (*t282) = ((float)((*t284)));
  t285 = v61;
  t286 = ((int32_t)(((int64_t)UINT64_C(1))));
  v61 = ((int32_t)((uint64_t)(t285) + (uint64_t)(t286)));
  }
  goto label19;
label20:;
  v80 = ((int32_t)(((int64_t)UINT64_C(0))));
label22:;
  t287 = ((int32_t)(v80));
  t288 = ((int32_t)(((int64_t)UINT64_C(32))));
  if (!(((t287) < (t288)))) goto label23;
  {
  t289 = ((int32_t)(v44));
  t290 = ((int32_t)(((int64_t)UINT64_C(32))));
  t291 = ((int32_t)(((int32_t)((uint64_t)(t289) * (uint64_t)(t290)))));
  t292 = ((int32_t)(v80));
  v81 = ((int32_t)(((int32_t)((uint64_t)(t291) + (uint64_t)(t292)))));
  v61 = ((int32_t)(((int64_t)UINT64_C(0))));
label25:;
  t293 = ((int32_t)(v61));
  t294 = ((int32_t)(v37));
  if (!(((t293) < (t294)))) goto label26;
  {
  t295 = ((uint64_t)(v74));
  t296 = ((int64_t)(v25));
  t297 = ((int64_t)(((int64_t)(v81))));
  t298 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t296) * (uint64_t)(t297)))))));
  t299 = ((uint64_t)(((uint64_t)((uint64_t)(t295) + (uint64_t)(t298)))));
  t300 = ((int32_t)(v43));
  t301 = gg_view((v67).data, 4);
  t302 = ((int32_t)(v61));
  t303 = ((int32_t)(((int64_t)UINT64_C(2))));
  t304 = ((int32_t)((uint64_t)(t302) * (uint64_t)(t303)));
  t305 = ((int32_t *)t301.data) + gg_index(t304, t301.len);
  t306 = ((int32_t)((*t305)));
  t307 = gg_view((v67).data, 4);
  t308 = ((int32_t)(v61));
  t309 = ((int32_t)(((int64_t)UINT64_C(2))));
  t310 = ((int32_t)(((int32_t)((uint64_t)(t308) * (uint64_t)(t309)))));
  t311 = ((int32_t)(((int64_t)UINT64_C(1))));
  t312 = ((int32_t)((uint64_t)(t310) + (uint64_t)(t311)));
  t313 = ((int32_t *)t307.data) + gg_index(t312, t307.len);
  t314 = ((int32_t)((*t313)));
  t315 = gg_view((v68).data, 4);
  t316 = ((int32_t)(v61));
  t317 = ((int32_t)(((int64_t)UINT64_C(2))));
  t318 = ((int32_t)((uint64_t)(t316) * (uint64_t)(t317)));
  t319 = ((float *)t315.data) + gg_index(t318, t315.len);
  t320 = ((float)((*t319)));
  t321 = gg_view((v69).data, 4);
  t322 = ((int32_t)(v61));
  t323 = ((int32_t)(((int64_t)UINT64_C(2))));
  t324 = ((int32_t)((uint64_t)(t322) * (uint64_t)(t323)));
  t325 = ((float *)t321.data) + gg_index(t324, t321.len);
  t326 = ((float)((*t325)));
  t327 = gg_view((v68).data, 4);
  t328 = ((int32_t)(v61));
  t329 = ((int32_t)(((int64_t)UINT64_C(2))));
  t330 = ((int32_t)(((int32_t)((uint64_t)(t328) * (uint64_t)(t329)))));
  t331 = ((int32_t)(((int64_t)UINT64_C(1))));
  t332 = ((int32_t)((uint64_t)(t330) + (uint64_t)(t331)));
  t333 = ((float *)t327.data) + gg_index(t332, t327.len);
  t334 = ((float)((*t333)));
  t335 = gg_view((v69).data, 4);
  t336 = ((int32_t)(v61));
  t337 = ((int32_t)(((int64_t)UINT64_C(2))));
  t338 = ((int32_t)(((int32_t)((uint64_t)(t336) * (uint64_t)(t337)))));
  t339 = ((int32_t)(((int64_t)UINT64_C(1))));
  t340 = ((int32_t)((uint64_t)(t338) + (uint64_t)(t339)));
  t341 = ((float *)t335.data) + gg_index(t340, t335.len);
  t342 = ((float)((*t341)));
  v82 = gg_function_19(t299, t300, t306, t314, t320, t326, t334, t342, base + 0);
  t343 = ((float)(v82));
  v82 = gg_function_24(t343, base + 0);
  t344 = ((uint64_t)(v3));
  t345 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t346 = ((t344) != (t345));
  t347 = t346;
  if (t347) {
  t348 = ((int32_t)(v37));
  t349 = ((int32_t)(((int64_t)UINT64_C(1))));
  t350 = ((t348) == (t349));
  t351 = t350;
  if (!t351) {
  t352 = ((int32_t)(v46));
  t353 = ((int32_t)(v61));
  t354 = ((int32_t)(((int32_t)((uint64_t)(t352) + (uint64_t)(t353)))));
  t355 = ((int32_t)(v15));
  t351 = ((t354) < (t355));
  }
  t347 = t351;
  }
  t356 = t347;
  if (t356) {
  {
  t357 = v82;
  t358 = ((float)(v54));
  t359 = ((uint64_t)(v76));
  t360 = ((int32_t)(v61));
  t361 = ((int32_t)(v22));
  t362 = ((int32_t)(((int32_t)((uint64_t)(t360) * (uint64_t)(t361)))));
  t363 = ((int32_t)(v81));
  t364 = ((int32_t)(((int32_t)((uint64_t)(t362) + (uint64_t)(t363)))));
  t365 = ((int32_t)(((int64_t)UINT64_C(2))));
  t366 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t364) * (uint64_t)(t365)))))));
  t367 = fg175v_ld_u16(t359, t366);
  t368 = ((uint32_t)(t367));
  t369 = fg175v_h2f(t368);
  t370 = ((float)(t369));
  t371 = ((float)(((t358) * (t370))));
  v82 = ((t357) + (t371));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t372 = v61;
  t373 = ((float *)(v79).data) + gg_index(t372, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t374 = v61;
  t375 = ((float *)(v79).data) + gg_index(t374, ((int64_t)UINT64_C(2)));
  t376 = ((float)((*t375)));
  t377 = ((float)(v82));
  t378 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t379 = ((float)(((float)(gg_double_bits(UINT64_C(4604418109348737398))))));
  t380 = ((float)(((t378) * (t379))));
  t381 = ((float)(((t377) + (t380))));
  t382 = fg175v_fmaxf(t376, t381);
  (*t373) = ((float)(t382));
  t383 = ((int32_t)(v43));
  t384 = ((int32_t)(v80));
  t385 = ((t383) == (t384));
  if (t385) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t386 = v61;
  t387 = ((float *)(v78).data) + gg_index(t386, ((int64_t)UINT64_C(2)));
  (*t387) = ((float)(v82));
  }
  }
  t388 = v61;
  t389 = ((int32_t)(((int64_t)UINT64_C(1))));
  v61 = ((int32_t)((uint64_t)(t388) + (uint64_t)(t389)));
  }
  goto label25;
label26:;
  t390 = v80;
  t391 = ((int32_t)(((int64_t)UINT64_C(1))));
  v80 = ((int32_t)((uint64_t)(t390) + (uint64_t)(t391)));
  }
  goto label22;
label23:;
  v61 = ((int32_t)(((int64_t)UINT64_C(0))));
label28:;
  t392 = ((int32_t)(v61));
  t393 = ((int32_t)(v37));
  if (!(((t392) < (t393)))) goto label29;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t394 = v61;
  t395 = ((float *)(v56).data) + gg_index(t394, ((int64_t)UINT64_C(2)));
  t396 = ((float)((*t395)));
  /* FG_DIRECT_SLICE_INDEX */
  t397 = v61;
  t398 = ((float *)(v79).data) + gg_index(t397, ((int64_t)UINT64_C(2)));
  t399 = ((float)((*t398)));
  t400 = ((float)(((t396) - (t399))));
  t401 = fg175v_expf(t400);
  v83 = ((float)(t401));
  /* FG_DIRECT_SLICE_INDEX */
  t402 = v61;
  t403 = ((float *)(v56).data) + gg_index(t402, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t404 = v61;
  t405 = ((float *)(v79).data) + gg_index(t404, ((int64_t)UINT64_C(2)));
  (*t403) = ((float)((*t405)));
  /* FG_DIRECT_SLICE_INDEX */
  t406 = v61;
  t407 = ((float *)(v78).data) + gg_index(t406, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t408 = v61;
  t409 = ((float *)(v78).data) + gg_index(t408, ((int64_t)UINT64_C(2)));
  t410 = ((float)((*t409)));
  /* FG_DIRECT_SLICE_INDEX */
  t411 = v61;
  t412 = ((float *)(v56).data) + gg_index(t411, ((int64_t)UINT64_C(2)));
  t413 = ((float)((*t412)));
  t414 = ((float)(((t410) - (t413))));
  t415 = fg175v_expf(t414);
  (*t407) = ((float)(t415));
  /* FG_DIRECT_SLICE_INDEX */
  t416 = v61;
  t417 = ((float *)(v57).data) + gg_index(t416, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t418 = v61;
  t419 = ((float *)(v57).data) + gg_index(t418, ((int64_t)UINT64_C(2)));
  t420 = ((float)((*t419)));
  t421 = ((float)(v83));
  t422 = ((float)(((t420) * (t421))));
  /* FG_DIRECT_SLICE_INDEX */
  t423 = v61;
  t424 = ((float *)(v78).data) + gg_index(t423, ((int64_t)UINT64_C(2)));
  t425 = ((float)((*t424)));
  (*t417) = ((float)(((t422) + (t425))));
  t426 = ((uint64_t)(v45));
  t427 = ((int32_t)(v61));
  t428 = ((int32_t)(((int64_t)UINT64_C(128))));
  t429 = ((int32_t)(((int32_t)((uint64_t)(t427) * (uint64_t)(t428)))));
  t430 = ((int32_t)(v55));
  t431 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t429) + (uint64_t)(t430)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t432 = v61;
  t433 = ((float *)(v78).data) + gg_index(t432, ((int64_t)UINT64_C(2)));
  t434 = ((float)((*t433)));
  fg175v_sm_st_f32(t426, t431, t434);
  v84 = ((int32_t)(((int64_t)UINT64_C(0))));
label31:;
  t435 = ((int32_t)(v84));
  t436 = ((int32_t)(((int64_t)UINT64_C(4))));
  if (!(((t435) < (t436)))) goto label32;
  {
  t437 = gg_view((v58).data, 8);
  t438 = ((int32_t)(v61));
  t439 = ((int32_t)(((int64_t)UINT64_C(4))));
  t440 = ((int32_t)(((int32_t)((uint64_t)(t438) * (uint64_t)(t439)))));
  t441 = ((int32_t)(v84));
  t442 = ((int32_t)((uint64_t)(t440) + (uint64_t)(t441)));
  t443 = ((float *)t437.data) + gg_index(t442, t437.len);
  t444 = gg_view((v58).data, 8);
  t445 = ((int32_t)(v61));
  t446 = ((int32_t)(((int64_t)UINT64_C(4))));
  t447 = ((int32_t)(((int32_t)((uint64_t)(t445) * (uint64_t)(t446)))));
  t448 = ((int32_t)(v84));
  t449 = ((int32_t)((uint64_t)(t447) + (uint64_t)(t448)));
  t450 = ((float *)t444.data) + gg_index(t449, t444.len);
  t451 = ((float)((*t450)));
  t452 = ((float)(v83));
  t453 = fg175v_mul_rn(t451, t452);
  (*t443) = ((float)(t453));
  t454 = gg_view((v59).data, 8);
  t455 = ((int32_t)(v61));
  t456 = ((int32_t)(((int64_t)UINT64_C(4))));
  t457 = ((int32_t)(((int32_t)((uint64_t)(t455) * (uint64_t)(t456)))));
  t458 = ((int32_t)(v84));
  t459 = ((int32_t)((uint64_t)(t457) + (uint64_t)(t458)));
  t460 = ((float *)t454.data) + gg_index(t459, t454.len);
  t461 = gg_view((v59).data, 8);
  t462 = ((int32_t)(v61));
  t463 = ((int32_t)(((int64_t)UINT64_C(4))));
  t464 = ((int32_t)(((int32_t)((uint64_t)(t462) * (uint64_t)(t463)))));
  t465 = ((int32_t)(v84));
  t466 = ((int32_t)((uint64_t)(t464) + (uint64_t)(t465)));
  t467 = ((float *)t461.data) + gg_index(t466, t461.len);
  t468 = ((float)((*t467)));
  t469 = ((float)(v83));
  t470 = fg175v_mul_rn(t468, t469);
  (*t460) = ((float)(t470));
  t471 = v84;
  t472 = ((int32_t)(((int64_t)UINT64_C(1))));
  v84 = ((int32_t)((uint64_t)(t471) + (uint64_t)(t472)));
  }
  goto label31;
label32:;
  t473 = v61;
  t474 = ((int32_t)(((int64_t)UINT64_C(1))));
  v61 = ((int32_t)((uint64_t)(t473) + (uint64_t)(t474)));
  }
  goto label28;
label29:;
  fg175v_syncwarp();
  v85 = ((int32_t)(((int64_t)UINT64_C(0))));
label34:;
  t475 = ((int32_t)(v85));
  t476 = ((int32_t)(((int64_t)UINT64_C(32))));
  if (!(((t475) < (t476)))) goto label35;
  {
  t477 = ((int32_t)(v44));
  t478 = ((int32_t)(((int64_t)UINT64_C(32))));
  t479 = ((int32_t)(((int32_t)((uint64_t)(t477) * (uint64_t)(t478)))));
  t480 = ((int32_t)(v85));
  v86 = ((int32_t)(((int32_t)((uint64_t)(t479) + (uint64_t)(t480)))));
  v61 = ((int32_t)(((int64_t)UINT64_C(0))));
label37:;
  t481 = ((int32_t)(v61));
  t482 = ((int32_t)(v37));
  if (!(((t481) < (t482)))) goto label38;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t483 = v61;
  t484 = ((float *)(v60).data) + gg_index(t483, ((int64_t)UINT64_C(2)));
  t485 = ((uint64_t)(v45));
  t486 = ((int32_t)(v61));
  t487 = ((int32_t)(((int64_t)UINT64_C(128))));
  t488 = ((int32_t)(((int32_t)((uint64_t)(t486) * (uint64_t)(t487)))));
  t489 = ((int32_t)(v86));
  t490 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t488) + (uint64_t)(t489)))))));
  t491 = fg175v_sm_f32(t485, t490);
  (*t484) = ((float)(t491));
  t492 = v61;
  t493 = ((int32_t)(((int64_t)UINT64_C(1))));
  v61 = ((int32_t)((uint64_t)(t492) + (uint64_t)(t493)));
  }
  goto label37;
label38:;
  v87 = ((int32_t)(((int64_t)UINT64_C(0))));
label40:;
  t494 = ((int32_t)(v87));
  t495 = ((int32_t)(((int64_t)UINT64_C(128))));
  if (!(((t494) < (t495)))) goto label41;
  {
  t496 = ((uint64_t)(v75));
  t497 = ((int64_t)(v28));
  t498 = ((int64_t)(((int64_t)(v86))));
  t499 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t497) * (uint64_t)(t498)))))));
  t500 = ((uint64_t)(((uint64_t)((uint64_t)(t496) + (uint64_t)(t499)))));
  t501 = ((int32_t)(((int64_t)UINT64_C(2))));
  t502 = ((int32_t)(v87));
  t503 = ((int32_t)(((int32_t)((uint64_t)(t501) * (uint64_t)(t502)))));
  t504 = ((int32_t)(v43));
  t505 = ((int32_t)(((int64_t)UINT64_C(4))));
  t506 = ((int32_t)(((int32_t)((uint64_t)(t504) * (uint64_t)(t505)))));
  t507 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t503) + (uint64_t)(t506)))))));
  v88 = gg_function_28(t500, t507, base + 0);
  v61 = ((int32_t)(((int64_t)UINT64_C(0))));
label43:;
  t508 = ((int32_t)(v61));
  t509 = ((int32_t)(v37));
  if (!(((t508) < (t509)))) goto label44;
  {
  t510 = gg_view((v58).data, 8);
  t511 = ((int32_t)(v61));
  t512 = ((int32_t)(((int64_t)UINT64_C(4))));
  t513 = ((int32_t)(((int32_t)((uint64_t)(t511) * (uint64_t)(t512)))));
  t514 = ((int32_t)(v87));
  t515 = ((int32_t)(((int64_t)UINT64_C(32))));
  t516 = ((int32_t)(((int32_t)gg_div(t514, t515))));
  t517 = ((int32_t)((uint64_t)(t513) + (uint64_t)(t516)));
  t518 = ((float *)t510.data) + gg_index(t517, t510.len);
  t519 = ((float)((v88).f0));
  /* FG_DIRECT_SLICE_INDEX */
  t520 = v61;
  t521 = ((float *)(v60).data) + gg_index(t520, ((int64_t)UINT64_C(2)));
  t522 = ((float)((*t521)));
  t523 = gg_view((v58).data, 8);
  t524 = ((int32_t)(v61));
  t525 = ((int32_t)(((int64_t)UINT64_C(4))));
  t526 = ((int32_t)(((int32_t)((uint64_t)(t524) * (uint64_t)(t525)))));
  t527 = ((int32_t)(v87));
  t528 = ((int32_t)(((int64_t)UINT64_C(32))));
  t529 = ((int32_t)(((int32_t)gg_div(t527, t528))));
  t530 = ((int32_t)((uint64_t)(t526) + (uint64_t)(t529)));
  t531 = ((float *)t523.data) + gg_index(t530, t523.len);
  t532 = ((float)((*t531)));
  t533 = fg175v_fma_rn(t519, t522, t532);
  (*t518) = ((float)(t533));
  t534 = gg_view((v59).data, 8);
  t535 = ((int32_t)(v61));
  t536 = ((int32_t)(((int64_t)UINT64_C(4))));
  t537 = ((int32_t)(((int32_t)((uint64_t)(t535) * (uint64_t)(t536)))));
  t538 = ((int32_t)(v87));
  t539 = ((int32_t)(((int64_t)UINT64_C(32))));
  t540 = ((int32_t)(((int32_t)gg_div(t538, t539))));
  t541 = ((int32_t)((uint64_t)(t537) + (uint64_t)(t540)));
  t542 = ((float *)t534.data) + gg_index(t541, t534.len);
  t543 = ((float)((v88).f1));
  /* FG_DIRECT_SLICE_INDEX */
  t544 = v61;
  t545 = ((float *)(v60).data) + gg_index(t544, ((int64_t)UINT64_C(2)));
  t546 = ((float)((*t545)));
  t547 = gg_view((v59).data, 8);
  t548 = ((int32_t)(v61));
  t549 = ((int32_t)(((int64_t)UINT64_C(4))));
  t550 = ((int32_t)(((int32_t)((uint64_t)(t548) * (uint64_t)(t549)))));
  t551 = ((int32_t)(v87));
  t552 = ((int32_t)(((int64_t)UINT64_C(32))));
  t553 = ((int32_t)(((int32_t)gg_div(t551, t552))));
  t554 = ((int32_t)((uint64_t)(t550) + (uint64_t)(t553)));
  t555 = ((float *)t547.data) + gg_index(t554, t547.len);
  t556 = ((float)((*t555)));
  t557 = fg175v_fma_rn(t543, t546, t556);
  (*t542) = ((float)(t557));
  t558 = gg_view((v58).data, 8);
  t559 = ((int32_t)(v61));
  t560 = ((int32_t)(((int64_t)UINT64_C(4))));
  t561 = ((int32_t)(((int32_t)((uint64_t)(t559) * (uint64_t)(t560)))));
  t562 = ((int32_t)(v87));
  t563 = ((int32_t)(((int64_t)UINT64_C(32))));
  t564 = ((int32_t)(((int32_t)gg_div(t562, t563))));
  t565 = ((int32_t)(((int32_t)((uint64_t)(t561) + (uint64_t)(t564)))));
  t566 = ((int32_t)(((int64_t)UINT64_C(1))));
  t567 = ((int32_t)((uint64_t)(t565) + (uint64_t)(t566)));
  t568 = ((float *)t558.data) + gg_index(t567, t558.len);
  t569 = ((float)((v88).f2));
  /* FG_DIRECT_SLICE_INDEX */
  t570 = v61;
  t571 = ((float *)(v60).data) + gg_index(t570, ((int64_t)UINT64_C(2)));
  t572 = ((float)((*t571)));
  t573 = gg_view((v58).data, 8);
  t574 = ((int32_t)(v61));
  t575 = ((int32_t)(((int64_t)UINT64_C(4))));
  t576 = ((int32_t)(((int32_t)((uint64_t)(t574) * (uint64_t)(t575)))));
  t577 = ((int32_t)(v87));
  t578 = ((int32_t)(((int64_t)UINT64_C(32))));
  t579 = ((int32_t)(((int32_t)gg_div(t577, t578))));
  t580 = ((int32_t)(((int32_t)((uint64_t)(t576) + (uint64_t)(t579)))));
  t581 = ((int32_t)(((int64_t)UINT64_C(1))));
  t582 = ((int32_t)((uint64_t)(t580) + (uint64_t)(t581)));
  t583 = ((float *)t573.data) + gg_index(t582, t573.len);
  t584 = ((float)((*t583)));
  t585 = fg175v_fma_rn(t569, t572, t584);
  (*t568) = ((float)(t585));
  t586 = gg_view((v59).data, 8);
  t587 = ((int32_t)(v61));
  t588 = ((int32_t)(((int64_t)UINT64_C(4))));
  t589 = ((int32_t)(((int32_t)((uint64_t)(t587) * (uint64_t)(t588)))));
  t590 = ((int32_t)(v87));
  t591 = ((int32_t)(((int64_t)UINT64_C(32))));
  t592 = ((int32_t)(((int32_t)gg_div(t590, t591))));
  t593 = ((int32_t)(((int32_t)((uint64_t)(t589) + (uint64_t)(t592)))));
  t594 = ((int32_t)(((int64_t)UINT64_C(1))));
  t595 = ((int32_t)((uint64_t)(t593) + (uint64_t)(t594)));
  t596 = ((float *)t586.data) + gg_index(t595, t586.len);
  t597 = ((float)((v88).f3));
  /* FG_DIRECT_SLICE_INDEX */
  t598 = v61;
  t599 = ((float *)(v60).data) + gg_index(t598, ((int64_t)UINT64_C(2)));
  t600 = ((float)((*t599)));
  t601 = gg_view((v59).data, 8);
  t602 = ((int32_t)(v61));
  t603 = ((int32_t)(((int64_t)UINT64_C(4))));
  t604 = ((int32_t)(((int32_t)((uint64_t)(t602) * (uint64_t)(t603)))));
  t605 = ((int32_t)(v87));
  t606 = ((int32_t)(((int64_t)UINT64_C(32))));
  t607 = ((int32_t)(((int32_t)gg_div(t605, t606))));
  t608 = ((int32_t)(((int32_t)((uint64_t)(t604) + (uint64_t)(t607)))));
  t609 = ((int32_t)(((int64_t)UINT64_C(1))));
  t610 = ((int32_t)((uint64_t)(t608) + (uint64_t)(t609)));
  t611 = ((float *)t601.data) + gg_index(t610, t601.len);
  t612 = ((float)((*t611)));
  t613 = fg175v_fma_rn(t597, t600, t612);
  (*t596) = ((float)(t613));
  t614 = v61;
  t615 = ((int32_t)(((int64_t)UINT64_C(1))));
  v61 = ((int32_t)((uint64_t)(t614) + (uint64_t)(t615)));
  }
  goto label43;
label44:;
  t616 = v87;
  t617 = ((int32_t)(((int64_t)UINT64_C(64))));
  v87 = ((int32_t)((uint64_t)(t616) + (uint64_t)(t617)));
  }
  goto label40;
label41:;
  t618 = v85;
  t619 = ((int32_t)(((int64_t)UINT64_C(1))));
  v85 = ((int32_t)((uint64_t)(t618) + (uint64_t)(t619)));
  }
  goto label34;
label35:;
  fg175v_syncwarp();
  t620 = ((int64_t)(v77));
  t621 = ((int64_t)(((int64_t)UINT64_C(128))));
  t622 = ((int64_t)(((int64_t)gg_div(t620, t621))));
  t623 = ((int64_t)(((int64_t)UINT64_C(10))));
  t624 = ((int32_t)(((int32_t)(((int64_t)((uint64_t)(t622) * (uint64_t)(t623)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t625 = ((int64_t)UINT64_C(0));
  t626 = ((float *)(v56).data) + gg_index(t625, ((int64_t)UINT64_C(2)));
  t627 = ((float)((*t626)));
  fg175v_trace(t624, t627);
  t628 = ((int64_t)(v77));
  t629 = ((int64_t)(((int64_t)UINT64_C(128))));
  t630 = ((int64_t)(((int64_t)gg_div(t628, t629))));
  t631 = ((int64_t)(((int64_t)UINT64_C(10))));
  t632 = ((int64_t)(((int64_t)((uint64_t)(t630) * (uint64_t)(t631)))));
  t633 = ((int64_t)(((int64_t)UINT64_C(1))));
  t634 = ((int32_t)(((int32_t)(((int64_t)((uint64_t)(t632) + (uint64_t)(t633)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t635 = ((int64_t)UINT64_C(0));
  t636 = ((float *)(v57).data) + gg_index(t635, ((int64_t)UINT64_C(2)));
  t637 = ((float)((*t636)));
  fg175v_trace(t634, t637);
  v89 = ((int32_t)(((int64_t)UINT64_C(0))));
label46:;
  t638 = ((int32_t)(v89));
  t639 = ((int32_t)(((int64_t)UINT64_C(4))));
  if (!(((t638) < (t639)))) goto label47;
  {
  t640 = ((int64_t)(v77));
  t641 = ((int64_t)(((int64_t)UINT64_C(128))));
  t642 = ((int64_t)(((int64_t)gg_div(t640, t641))));
  t643 = ((int64_t)(((int64_t)UINT64_C(10))));
  t644 = ((int64_t)(((int64_t)((uint64_t)(t642) * (uint64_t)(t643)))));
  t645 = ((int64_t)(((int64_t)UINT64_C(2))));
  t646 = ((int64_t)(((int64_t)((uint64_t)(t644) + (uint64_t)(t645)))));
  t647 = ((int32_t)(v89));
  t648 = ((int32_t)(((int64_t)UINT64_C(2))));
  t649 = ((int64_t)(((int32_t)((uint64_t)(t647) * (uint64_t)(t648)))));
  t650 = ((int32_t)(((int32_t)(((int64_t)((uint64_t)(t646) + (uint64_t)(t649)))))));
  t652 = ((int32_t)(v89));
  t653 = ((int32_t)(((int64_t)UINT64_C(0))));
  t654 = ((t652) == (t653));
  if (t654) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t655 = ((int64_t)UINT64_C(0));
  t656 = ((float *)(v78).data) + gg_index(t655, ((int64_t)UINT64_C(2)));
  t651 = ((float)((*t656)));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t657 = v89;
  t658 = ((float *)(v58).data) + gg_index(t657, ((int64_t)UINT64_C(8)));
  t651 = ((float)((*t658)));
  }
  }
  t659 = ((float)(t651));
  fg175v_trace(t650, t659);
  t660 = ((int64_t)(v77));
  t661 = ((int64_t)(((int64_t)UINT64_C(128))));
  t662 = ((int64_t)(((int64_t)gg_div(t660, t661))));
  t663 = ((int64_t)(((int64_t)UINT64_C(10))));
  t664 = ((int64_t)(((int64_t)((uint64_t)(t662) * (uint64_t)(t663)))));
  t665 = ((int64_t)(((int64_t)UINT64_C(3))));
  t666 = ((int64_t)(((int64_t)((uint64_t)(t664) + (uint64_t)(t665)))));
  t667 = ((int32_t)(v89));
  t668 = ((int32_t)(((int64_t)UINT64_C(2))));
  t669 = ((int64_t)(((int32_t)((uint64_t)(t667) * (uint64_t)(t668)))));
  t670 = ((int32_t)(((int32_t)(((int64_t)((uint64_t)(t666) + (uint64_t)(t669)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t671 = v89;
  t672 = ((float *)(v59).data) + gg_index(t671, ((int64_t)UINT64_C(8)));
  t673 = ((float)((*t672)));
  fg175v_trace(t670, t673);
  t674 = v89;
  t675 = ((int32_t)(((int64_t)UINT64_C(1))));
  v89 = ((int32_t)((uint64_t)(t674) + (uint64_t)(t675)));
  }
  goto label46;
label47:;
  t676 = v77;
  t677 = ((int32_t)(v42));
  t678 = ((int32_t)(((int64_t)UINT64_C(128))));
  t679 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t677) * (uint64_t)(t678)))))));
  v77 = ((int64_t)((uint64_t)(t676) + (uint64_t)(t679)));
  t680 = v74;
  t681 = ((int32_t)(v42));
  t682 = ((int32_t)(((int64_t)UINT64_C(128))));
  t683 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t681) * (uint64_t)(t682)))))));
  t684 = ((int64_t)(v25));
  t685 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t683) * (uint64_t)(t684)))))));
  v74 = ((uint64_t)((uint64_t)(t680) + (uint64_t)(t685)));
  t686 = v75;
  t687 = ((int32_t)(v42));
  t688 = ((int32_t)(((int64_t)UINT64_C(128))));
  t689 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t687) * (uint64_t)(t688)))))));
  t690 = ((int64_t)(v28));
  t691 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t689) * (uint64_t)(t690)))))));
  v75 = ((uint64_t)((uint64_t)(t686) + (uint64_t)(t691)));
  t692 = v76;
  t693 = ((int32_t)(v42));
  t694 = ((int32_t)(((int64_t)UINT64_C(128))));
  t695 = ((int32_t)(((int32_t)((uint64_t)(t693) * (uint64_t)(t694)))));
  t696 = ((int32_t)(((int64_t)UINT64_C(2))));
  t697 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t695) * (uint64_t)(t696)))))));
  v76 = ((uint64_t)((uint64_t)(t692) + (uint64_t)(t697)));
  }
  goto label16;
label17:;
  t698 = ((uint64_t)(v4));
  t699 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t700 = ((t698) != (t699));
  t701 = t700;
  if (t701) {
  t702 = ((int32_t)(v39));
  t703 = ((int32_t)(((int64_t)UINT64_C(0))));
  t701 = ((t702) == (t703));
  }
  t704 = t701;
  if (t704) {
  {
  t705 = ((uint64_t)(v4));
  t706 = ((int32_t)(v48));
  t707 = ((int32_t)(((int64_t)UINT64_C(4))));
  t708 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t706) * (uint64_t)(t707)))))));
  t709 = fg175v_ld_f32(t705, t708);
  v90 = ((float)(t709));
  v91 = ((int32_t)(v44));
  t710 = ((int32_t)(((int64_t)UINT64_C(4))));
  t711 = ((int32_t)(v37));
  t712 = ((t710) > (t711));
  t713 = t712;
  if (t713) {
  t714 = ((int32_t)(v91));
  t715 = ((int32_t)(v37));
  t713 = ((t714) >= (t715));
  }
  t716 = t713;
  t717 = (!(t716));
  if (t717) {
  {
  t718 = ((float)(v90));
  /* FG_DIRECT_SLICE_INDEX */
  t719 = v91;
  t720 = ((float *)(v56).data) + gg_index(t719, ((int64_t)UINT64_C(2)));
  t721 = ((float)((*t720)));
  t722 = fg175v_fmaxf(t718, t721);
  v92 = ((float)(t722));
  /* FG_DIRECT_SLICE_INDEX */
  t723 = v91;
  t724 = ((float *)(v56).data) + gg_index(t723, ((int64_t)UINT64_C(2)));
  t725 = ((float)((*t724)));
  t726 = ((float)(v92));
  t727 = ((float)(((t725) - (t726))));
  t728 = fg175v_expf(t727);
  v93 = ((float)(t728));
  /* FG_DIRECT_SLICE_INDEX */
  t729 = v91;
  t730 = ((float *)(v56).data) + gg_index(t729, ((int64_t)UINT64_C(2)));
  (*t730) = ((float)(v92));
  /* FG_DIRECT_SLICE_INDEX */
  t731 = v91;
  t732 = ((float *)(v57).data) + gg_index(t731, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t733 = v91;
  t734 = ((float *)(v57).data) + gg_index(t733, ((int64_t)UINT64_C(2)));
  t735 = ((float)((*t734)));
  t736 = ((float)(v93));
  t737 = ((float)(((t735) * (t736))));
  t739 = ((int32_t)(v43));
  t740 = ((int32_t)(((int64_t)UINT64_C(0))));
  t741 = ((t739) == (t740));
  if (t741) {
  {
  t742 = ((float)(v90));
  /* FG_DIRECT_SLICE_INDEX */
  t743 = v91;
  t744 = ((float *)(v56).data) + gg_index(t743, ((int64_t)UINT64_C(2)));
  t745 = ((float)((*t744)));
  t746 = ((float)(((t742) - (t745))));
  t747 = fg175v_expf(t746);
  t738 = ((float)(t747));
  }
  } else {
  {
  t738 = ((float)(gg_double_bits(UINT64_C(0))));
  }
  }
  t748 = ((float)(t738));
  (*t732) = ((float)(((t737) + (t748))));
  v94 = ((int32_t)(((int64_t)UINT64_C(0))));
label49:;
  t749 = ((int32_t)(v94));
  t750 = ((int32_t)(((int64_t)UINT64_C(4))));
  if (!(((t749) < (t750)))) goto label50;
  {
  t751 = gg_view((v58).data, 8);
  t752 = ((int32_t)(v91));
  t753 = ((int32_t)(((int64_t)UINT64_C(4))));
  t754 = ((int32_t)(((int32_t)((uint64_t)(t752) * (uint64_t)(t753)))));
  t755 = ((int32_t)(v94));
  t756 = ((int32_t)((uint64_t)(t754) + (uint64_t)(t755)));
  t757 = ((float *)t751.data) + gg_index(t756, t751.len);
  t758 = gg_view((v58).data, 8);
  t759 = ((int32_t)(v91));
  t760 = ((int32_t)(((int64_t)UINT64_C(4))));
  t761 = ((int32_t)(((int32_t)((uint64_t)(t759) * (uint64_t)(t760)))));
  t762 = ((int32_t)(v94));
  t763 = ((int32_t)((uint64_t)(t761) + (uint64_t)(t762)));
  t764 = ((float *)t758.data) + gg_index(t763, t758.len);
  t765 = ((float)((*t764)));
  t766 = ((float)(v93));
  t767 = fg175v_mul_rn(t765, t766);
  (*t757) = ((float)(t767));
  t768 = gg_view((v59).data, 8);
  t769 = ((int32_t)(v91));
  t770 = ((int32_t)(((int64_t)UINT64_C(4))));
  t771 = ((int32_t)(((int32_t)((uint64_t)(t769) * (uint64_t)(t770)))));
  t772 = ((int32_t)(v94));
  t773 = ((int32_t)((uint64_t)(t771) + (uint64_t)(t772)));
  t774 = ((float *)t768.data) + gg_index(t773, t768.len);
  t775 = gg_view((v59).data, 8);
  t776 = ((int32_t)(v91));
  t777 = ((int32_t)(((int64_t)UINT64_C(4))));
  t778 = ((int32_t)(((int32_t)((uint64_t)(t776) * (uint64_t)(t777)))));
  t779 = ((int32_t)(v94));
  t780 = ((int32_t)((uint64_t)(t778) + (uint64_t)(t779)));
  t781 = ((float *)t775.data) + gg_index(t780, t775.len);
  t782 = ((float)((*t781)));
  t783 = ((float)(v93));
  t784 = fg175v_mul_rn(t782, t783);
  (*t774) = ((float)(t784));
  t785 = v94;
  t786 = ((int32_t)(((int64_t)UINT64_C(1))));
  v94 = ((int32_t)((uint64_t)(t785) + (uint64_t)(t786)));
  }
  goto label49;
label50:;
  }
  }
  }
  }
  v61 = ((int32_t)(((int64_t)UINT64_C(0))));
label52:;
  t787 = ((int32_t)(v61));
  t788 = ((int32_t)(v37));
  if (!(((t787) < (t788)))) goto label53;
  {
  t789 = ((int32_t)(v44));
  t790 = ((int32_t)(((int64_t)UINT64_C(0))));
  t791 = ((t789) == (t790));
  if (t791) {
  {
  t792 = ((uint64_t)(v45));
  t793 = ((int32_t)(((int64_t)UINT64_C(1024))));
  t794 = ((int32_t)(v61));
  t795 = ((int32_t)(((int64_t)UINT64_C(32))));
  t796 = ((int32_t)(((int32_t)((uint64_t)(t794) * (uint64_t)(t795)))));
  t797 = ((int32_t)(((int32_t)((uint64_t)(t793) + (uint64_t)(t796)))));
  t798 = ((int32_t)(v43));
  t799 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t797) + (uint64_t)(t798)))))));
  t800 = gg_function_5(base + 0);
  fg175v_sm_st_f32(t792, t799, t800);
  t801 = ((uint64_t)(v45));
  t802 = ((int32_t)(((int64_t)UINT64_C(1088))));
  t803 = ((int32_t)(v61));
  t804 = ((int32_t)(((int64_t)UINT64_C(32))));
  t805 = ((int32_t)(((int32_t)((uint64_t)(t803) * (uint64_t)(t804)))));
  t806 = ((int32_t)(((int32_t)((uint64_t)(t802) + (uint64_t)(t805)))));
  t807 = ((int32_t)(v43));
  t808 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t806) + (uint64_t)(t807)))))));
  t809 = ((float)(gg_double_bits(UINT64_C(0))));
  fg175v_sm_st_f32(t801, t808, t809);
  }
  }
  t810 = v61;
  t811 = ((int32_t)(((int64_t)UINT64_C(1))));
  v61 = ((int32_t)((uint64_t)(t810) + (uint64_t)(t811)));
  }
  goto label52;
label53:;
  fg175v_barrier();
  v61 = ((int32_t)(((int64_t)UINT64_C(0))));
label55:;
  t812 = ((int32_t)(v61));
  t813 = ((int32_t)(v37));
  if (!(((t812) < (t813)))) goto label56;
  {
  t814 = ((int32_t)(v43));
  t815 = ((int32_t)(((int64_t)UINT64_C(0))));
  t816 = ((t814) == (t815));
  if (t816) {
  {
  t817 = ((uint64_t)(v45));
  t818 = ((int32_t)(((int64_t)UINT64_C(1024))));
  t819 = ((int32_t)(v61));
  t820 = ((int32_t)(((int64_t)UINT64_C(32))));
  t821 = ((int32_t)(((int32_t)((uint64_t)(t819) * (uint64_t)(t820)))));
  t822 = ((int32_t)(((int32_t)((uint64_t)(t818) + (uint64_t)(t821)))));
  t823 = ((int32_t)(v44));
  t824 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t822) + (uint64_t)(t823)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t825 = v61;
  t826 = ((float *)(v56).data) + gg_index(t825, ((int64_t)UINT64_C(2)));
  t827 = ((float)((*t826)));
  fg175v_sm_st_f32(t817, t824, t827);
  }
  }
  t828 = v61;
  t829 = ((int32_t)(((int64_t)UINT64_C(1))));
  v61 = ((int32_t)((uint64_t)(t828) + (uint64_t)(t829)));
  }
  goto label55;
label56:;
  fg175v_barrier();
  v95 = ((int32_t)(v37));
  t830 = ((int32_t)(v37));
  t831 = ((int32_t)(((int64_t)UINT64_C(1))));
  t832 = ((t830) > (t831));
  t833 = t832;
  if (t833) {
  t834 = ((int32_t)(v46));
  t835 = ((int32_t)(v37));
  t836 = ((int32_t)(((int32_t)((uint64_t)(t834) + (uint64_t)(t835)))));
  t837 = ((int32_t)(v15));
  t833 = ((t836) > (t837));
  }
  t838 = t833;
  if (t838) {
  {
  t839 = ((int32_t)(v15));
  t840 = ((int32_t)(v46));
  v95 = ((int32_t)(((int32_t)((uint64_t)(t839) - (uint64_t)(t840)))));
  }
  }
  v61 = ((int32_t)(((int64_t)UINT64_C(0))));
label58:;
  t841 = ((int32_t)(v61));
  t842 = ((int32_t)(v95));
  if (!(((t841) < (t842)))) goto label59;
  {
  t843 = ((uint64_t)(v45));
  t844 = ((int32_t)(((int64_t)UINT64_C(1024))));
  t845 = ((int32_t)(v61));
  t846 = ((int32_t)(((int64_t)UINT64_C(32))));
  t847 = ((int32_t)(((int32_t)((uint64_t)(t845) * (uint64_t)(t846)))));
  t848 = ((int32_t)(((int32_t)((uint64_t)(t844) + (uint64_t)(t847)))));
  t849 = ((int32_t)(v43));
  t850 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t848) + (uint64_t)(t849)))))));
  t851 = fg175v_sm_f32(t843, t850);
  v96 = ((float)(t851));
  t852 = ((float)(v96));
  v96 = gg_function_31(t852, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t853 = v61;
  t854 = ((float *)(v56).data) + gg_index(t853, ((int64_t)UINT64_C(2)));
  t855 = ((float)((*t854)));
  t856 = ((float)(v96));
  t857 = ((float)(((t855) - (t856))));
  t858 = fg175v_expf(t857);
  v97 = ((float)(t858));
  /* FG_DIRECT_SLICE_INDEX */
  t859 = v61;
  t860 = ((float *)(v56).data) + gg_index(t859, ((int64_t)UINT64_C(2)));
  (*t860) = ((float)(v96));
  v98 = ((int32_t)(((int64_t)UINT64_C(0))));
label61:;
  t861 = ((int32_t)(v98));
  t862 = ((int32_t)(((int64_t)UINT64_C(4))));
  if (!(((t861) < (t862)))) goto label62;
  {
  t863 = gg_view((v58).data, 8);
  t864 = ((int32_t)(v61));
  t865 = ((int32_t)(((int64_t)UINT64_C(4))));
  t866 = ((int32_t)(((int32_t)((uint64_t)(t864) * (uint64_t)(t865)))));
  t867 = ((int32_t)(v98));
  t868 = ((int32_t)((uint64_t)(t866) + (uint64_t)(t867)));
  t869 = ((float *)t863.data) + gg_index(t868, t863.len);
  t870 = gg_view((v58).data, 8);
  t871 = ((int32_t)(v61));
  t872 = ((int32_t)(((int64_t)UINT64_C(4))));
  t873 = ((int32_t)(((int32_t)((uint64_t)(t871) * (uint64_t)(t872)))));
  t874 = ((int32_t)(v98));
  t875 = ((int32_t)((uint64_t)(t873) + (uint64_t)(t874)));
  t876 = ((float *)t870.data) + gg_index(t875, t870.len);
  t877 = ((float)((*t876)));
  t878 = ((float)(v97));
  t879 = fg175v_mul_rn(t877, t878);
  (*t869) = ((float)(t879));
  t880 = gg_view((v59).data, 8);
  t881 = ((int32_t)(v61));
  t882 = ((int32_t)(((int64_t)UINT64_C(4))));
  t883 = ((int32_t)(((int32_t)((uint64_t)(t881) * (uint64_t)(t882)))));
  t884 = ((int32_t)(v98));
  t885 = ((int32_t)((uint64_t)(t883) + (uint64_t)(t884)));
  t886 = ((float *)t880.data) + gg_index(t885, t880.len);
  t887 = gg_view((v59).data, 8);
  t888 = ((int32_t)(v61));
  t889 = ((int32_t)(((int64_t)UINT64_C(4))));
  t890 = ((int32_t)(((int32_t)((uint64_t)(t888) * (uint64_t)(t889)))));
  t891 = ((int32_t)(v98));
  t892 = ((int32_t)((uint64_t)(t890) + (uint64_t)(t891)));
  t893 = ((float *)t887.data) + gg_index(t892, t887.len);
  t894 = ((float)((*t893)));
  t895 = ((float)(v97));
  t896 = fg175v_mul_rn(t894, t895);
  (*t886) = ((float)(t896));
  t897 = v98;
  t898 = ((int32_t)(((int64_t)UINT64_C(1))));
  v98 = ((int32_t)((uint64_t)(t897) + (uint64_t)(t898)));
  }
  goto label61;
label62:;
  v99 = ((int32_t)(((int64_t)UINT64_C(0))));
label64:;
  t899 = ((int32_t)(v99));
  t900 = ((int32_t)(((int64_t)UINT64_C(128))));
  if (!(((t899) < (t900)))) goto label65;
  {
  t901 = ((int32_t)(v99));
  t902 = ((int32_t)(v43));
  t903 = ((int32_t)(((int64_t)UINT64_C(2))));
  t904 = ((int32_t)(((int32_t)((uint64_t)(t902) * (uint64_t)(t903)))));
  v100 = ((int32_t)(((int32_t)((uint64_t)(t901) + (uint64_t)(t904)))));
  t905 = ((uint64_t)(v45));
  t906 = ((int32_t)(v44));
  t907 = ((int32_t)(((int64_t)UINT64_C(256))));
  t908 = ((int32_t)(((int32_t)((uint64_t)(t906) * (uint64_t)(t907)))));
  t909 = ((int32_t)(((int64_t)UINT64_C(2))));
  t910 = ((int32_t)(v100));
  t911 = ((int32_t)(((int32_t)((uint64_t)(t909) * (uint64_t)(t910)))));
  t912 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t908) + (uint64_t)(t911)))))));
  t913 = gg_view((v58).data, 8);
  t914 = ((int32_t)(v61));
  t915 = ((int32_t)(((int64_t)UINT64_C(4))));
  t916 = ((int32_t)(((int32_t)((uint64_t)(t914) * (uint64_t)(t915)))));
  t917 = ((int32_t)(v99));
  t918 = ((int32_t)(((int64_t)UINT64_C(32))));
  t919 = ((int32_t)(((int32_t)gg_div(t917, t918))));
  t920 = ((int32_t)((uint64_t)(t916) + (uint64_t)(t919)));
  t921 = ((float *)t913.data) + gg_index(t920, t913.len);
  t922 = ((float)((*t921)));
  fg175v_sm_st_f32(t905, t912, t922);
  t923 = ((uint64_t)(v45));
  t924 = ((int32_t)(v44));
  t925 = ((int32_t)(((int64_t)UINT64_C(256))));
  t926 = ((int32_t)(((int32_t)((uint64_t)(t924) * (uint64_t)(t925)))));
  t927 = ((int32_t)(((int64_t)UINT64_C(2))));
  t928 = ((int32_t)(v100));
  t929 = ((int32_t)(((int32_t)((uint64_t)(t927) * (uint64_t)(t928)))));
  t930 = ((int32_t)(((int32_t)((uint64_t)(t926) + (uint64_t)(t929)))));
  t931 = ((int32_t)(((int64_t)UINT64_C(1))));
  t932 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t930) + (uint64_t)(t931)))))));
  t933 = gg_view((v59).data, 8);
  t934 = ((int32_t)(v61));
  t935 = ((int32_t)(((int64_t)UINT64_C(4))));
  t936 = ((int32_t)(((int32_t)((uint64_t)(t934) * (uint64_t)(t935)))));
  t937 = ((int32_t)(v99));
  t938 = ((int32_t)(((int64_t)UINT64_C(32))));
  t939 = ((int32_t)(((int32_t)gg_div(t937, t938))));
  t940 = ((int32_t)((uint64_t)(t936) + (uint64_t)(t939)));
  t941 = ((float *)t933.data) + gg_index(t940, t933.len);
  t942 = ((float)((*t941)));
  fg175v_sm_st_f32(t923, t932, t942);
  t943 = ((uint64_t)(v45));
  t944 = ((int32_t)(v44));
  t945 = ((int32_t)(((int64_t)UINT64_C(256))));
  t946 = ((int32_t)(((int32_t)((uint64_t)(t944) * (uint64_t)(t945)))));
  t947 = ((int32_t)(((int64_t)UINT64_C(2))));
  t948 = ((int32_t)(v100));
  t949 = ((int32_t)(((int32_t)((uint64_t)(t947) * (uint64_t)(t948)))));
  t950 = ((int32_t)(((int32_t)((uint64_t)(t946) + (uint64_t)(t949)))));
  t951 = ((int32_t)(((int64_t)UINT64_C(2))));
  t952 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t950) + (uint64_t)(t951)))))));
  t953 = gg_view((v58).data, 8);
  t954 = ((int32_t)(v61));
  t955 = ((int32_t)(((int64_t)UINT64_C(4))));
  t956 = ((int32_t)(((int32_t)((uint64_t)(t954) * (uint64_t)(t955)))));
  t957 = ((int32_t)(v99));
  t958 = ((int32_t)(((int64_t)UINT64_C(32))));
  t959 = ((int32_t)(((int32_t)gg_div(t957, t958))));
  t960 = ((int32_t)(((int32_t)((uint64_t)(t956) + (uint64_t)(t959)))));
  t961 = ((int32_t)(((int64_t)UINT64_C(1))));
  t962 = ((int32_t)((uint64_t)(t960) + (uint64_t)(t961)));
  t963 = ((float *)t953.data) + gg_index(t962, t953.len);
  t964 = ((float)((*t963)));
  fg175v_sm_st_f32(t943, t952, t964);
  t965 = ((uint64_t)(v45));
  t966 = ((int32_t)(v44));
  t967 = ((int32_t)(((int64_t)UINT64_C(256))));
  t968 = ((int32_t)(((int32_t)((uint64_t)(t966) * (uint64_t)(t967)))));
  t969 = ((int32_t)(((int64_t)UINT64_C(2))));
  t970 = ((int32_t)(v100));
  t971 = ((int32_t)(((int32_t)((uint64_t)(t969) * (uint64_t)(t970)))));
  t972 = ((int32_t)(((int32_t)((uint64_t)(t968) + (uint64_t)(t971)))));
  t973 = ((int32_t)(((int64_t)UINT64_C(3))));
  t974 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t972) + (uint64_t)(t973)))))));
  t975 = gg_view((v59).data, 8);
  t976 = ((int32_t)(v61));
  t977 = ((int32_t)(((int64_t)UINT64_C(4))));
  t978 = ((int32_t)(((int32_t)((uint64_t)(t976) * (uint64_t)(t977)))));
  t979 = ((int32_t)(v99));
  t980 = ((int32_t)(((int64_t)UINT64_C(32))));
  t981 = ((int32_t)(((int32_t)gg_div(t979, t980))));
  t982 = ((int32_t)(((int32_t)((uint64_t)(t978) + (uint64_t)(t981)))));
  t983 = ((int32_t)(((int64_t)UINT64_C(1))));
  t984 = ((int32_t)((uint64_t)(t982) + (uint64_t)(t983)));
  t985 = ((float *)t975.data) + gg_index(t984, t975.len);
  t986 = ((float)((*t985)));
  fg175v_sm_st_f32(t965, t974, t986);
  t987 = v99;
  t988 = ((int32_t)(((int64_t)UINT64_C(64))));
  v99 = ((int32_t)((uint64_t)(t987) + (uint64_t)(t988)));
  }
  goto label64;
label65:;
  /* FG_DIRECT_SLICE_INDEX */
  t989 = v61;
  t990 = ((float *)(v57).data) + gg_index(t989, ((int64_t)UINT64_C(2)));
  t991 = (*t990);
  t992 = ((float)(v97));
  (*t990) = ((t991) * (t992));
  /* FG_DIRECT_SLICE_INDEX */
  t993 = v61;
  t994 = ((float *)(v57).data) + gg_index(t993, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t995 = v61;
  t996 = ((float *)(v57).data) + gg_index(t995, ((int64_t)UINT64_C(2)));
  t997 = ((float)((*t996)));
  (*t994) = gg_function_24(t997, base + 0);
  t998 = ((int32_t)(v43));
  t999 = ((int32_t)(((int64_t)UINT64_C(0))));
  t1000 = ((t998) == (t999));
  if (t1000) {
  {
  t1001 = ((uint64_t)(v45));
  t1002 = ((int32_t)(((int64_t)UINT64_C(1088))));
  t1003 = ((int32_t)(v61));
  t1004 = ((int32_t)(((int64_t)UINT64_C(32))));
  t1005 = ((int32_t)(((int32_t)((uint64_t)(t1003) * (uint64_t)(t1004)))));
  t1006 = ((int32_t)(((int32_t)((uint64_t)(t1002) + (uint64_t)(t1005)))));
  t1007 = ((int32_t)(v44));
  t1008 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t1006) + (uint64_t)(t1007)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t1009 = v61;
  t1010 = ((float *)(v57).data) + gg_index(t1009, ((int64_t)UINT64_C(2)));
  t1011 = ((float)((*t1010)));
  fg175v_sm_st_f32(t1001, t1008, t1011);
  }
  }
  fg175v_barrier();
  /* FG_DIRECT_SLICE_INDEX */
  t1012 = v61;
  t1013 = ((float *)(v57).data) + gg_index(t1012, ((int64_t)UINT64_C(2)));
  t1014 = ((uint64_t)(v45));
  t1015 = ((int32_t)(((int64_t)UINT64_C(1088))));
  t1016 = ((int32_t)(v61));
  t1017 = ((int32_t)(((int64_t)UINT64_C(32))));
  t1018 = ((int32_t)(((int32_t)((uint64_t)(t1016) * (uint64_t)(t1017)))));
  t1019 = ((int32_t)(((int32_t)((uint64_t)(t1015) + (uint64_t)(t1018)))));
  t1020 = ((int32_t)(v43));
  t1021 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t1019) + (uint64_t)(t1020)))))));
  t1022 = fg175v_sm_f32(t1014, t1021);
  (*t1013) = ((float)(t1022));
  /* FG_DIRECT_SLICE_INDEX */
  t1023 = v61;
  t1024 = ((float *)(v57).data) + gg_index(t1023, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t1025 = v61;
  t1026 = ((float *)(v57).data) + gg_index(t1025, ((int64_t)UINT64_C(2)));
  t1027 = ((float)((*t1026)));
  (*t1024) = gg_function_24(t1027, base + 0);
  v101 = ((int32_t)(((int64_t)UINT64_C(0))));
label67:;
  t1028 = ((int32_t)(v101));
  t1029 = ((int32_t)(((int64_t)UINT64_C(256))));
  if (!(((t1028) < (t1029)))) goto label68;
  {
  v102 = ((float)(gg_double_bits(UINT64_C(0))));
  v103 = ((int32_t)(((int64_t)UINT64_C(0))));
label70:;
  t1030 = ((int32_t)(v103));
  t1031 = ((int32_t)(((int64_t)UINT64_C(4))));
  if (!(((t1030) < (t1031)))) goto label71;
  {
  t1032 = v102;
  t1033 = ((uint64_t)(v45));
  t1034 = ((int32_t)(v103));
  t1035 = ((int32_t)(((int64_t)UINT64_C(256))));
  t1036 = ((int32_t)(((int32_t)((uint64_t)(t1034) * (uint64_t)(t1035)))));
  t1037 = ((int32_t)(v101));
  t1038 = ((int32_t)(((int32_t)((uint64_t)(t1036) + (uint64_t)(t1037)))));
  t1039 = ((int32_t)(v55));
  t1040 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t1038) + (uint64_t)(t1039)))))));
  t1041 = fg175v_sm_f32(t1033, t1040);
  t1042 = ((float)(t1041));
  v102 = ((t1032) + (t1042));
  t1043 = v103;
  t1044 = ((int32_t)(((int64_t)UINT64_C(1))));
  v103 = ((int32_t)((uint64_t)(t1043) + (uint64_t)(t1044)));
  }
  goto label70;
label71:;
  t1045 = ((int32_t)(v42));
  t1046 = ((int32_t)(((int64_t)UINT64_C(1))));
  t1047 = ((t1045) == (t1046));
  if (t1047) {
  {
  t1048 = v102;
  /* FG_DIRECT_SLICE_INDEX */
  t1049 = v61;
  t1050 = ((float *)(v57).data) + gg_index(t1049, ((int64_t)UINT64_C(2)));
  t1051 = ((float)((*t1050)));
  v102 = ((t1048) / (t1051));
  }
  }
  t1052 = ((int64_t)(((int64_t)(v47))));
  t1053 = ((int64_t)(((int64_t)(v15))));
  t1054 = ((int64_t)(((int64_t)((uint64_t)(t1052) * (uint64_t)(t1053)))));
  t1055 = ((int32_t)(v46));
  t1056 = ((int32_t)(v61));
  t1057 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t1055) + (uint64_t)(t1056)))))));
  t1058 = ((int64_t)(((int64_t)((uint64_t)(t1054) + (uint64_t)(t1057)))));
  t1059 = ((int64_t)(((int64_t)(v16))));
  t1060 = ((int64_t)(((int64_t)((uint64_t)(t1058) * (uint64_t)(t1059)))));
  t1061 = ((int64_t)(((int64_t)(v48))));
  t1062 = ((int64_t)(((int64_t)((uint64_t)(t1060) + (uint64_t)(t1061)))));
  t1063 = ((int64_t)(((int64_t)(v42))));
  t1064 = ((int64_t)(((int64_t)((uint64_t)(t1062) * (uint64_t)(t1063)))));
  t1065 = ((int64_t)(((int64_t)(v39))));
  v104 = ((int64_t)(((int64_t)((uint64_t)(t1064) + (uint64_t)(t1065)))));
  t1066 = ((uint64_t)(v6));
  t1067 = ((int64_t)(v104));
  t1068 = ((int64_t)(((int64_t)UINT64_C(256))));
  t1069 = ((int64_t)(((int64_t)((uint64_t)(t1067) * (uint64_t)(t1068)))));
  t1070 = ((int32_t)(v101));
  t1071 = ((int32_t)(v55));
  t1072 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t1070) + (uint64_t)(t1071)))))));
  t1073 = ((int64_t)(((int64_t)((uint64_t)(t1069) + (uint64_t)(t1072)))));
  t1074 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1075 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t1073) * (uint64_t)(t1074)))))));
  t1076 = ((float)(v102));
  fg175v_st_f32(t1066, t1075, t1076);
  t1077 = v101;
  t1078 = ((int32_t)(((int64_t)UINT64_C(128))));
  v101 = ((int32_t)((uint64_t)(t1077) + (uint64_t)(t1078)));
  }
  goto label67;
label68:;
  t1079 = ((int32_t)(v61));
  t1080 = ((int32_t)(v95));
  t1081 = ((int32_t)(((int64_t)UINT64_C(1))));
  t1082 = ((int32_t)(((int32_t)((uint64_t)(t1080) - (uint64_t)(t1081)))));
  t1083 = ((t1079) < (t1082));
  if (t1083) {
  {
  fg175v_barrier();
  }
  }
  t1084 = v61;
  t1085 = ((int32_t)(((int64_t)UINT64_C(1))));
  v61 = ((int32_t)((uint64_t)(t1084) + (uint64_t)(t1085)));
  }
  goto label58;
label59:;
  t1086 = ((int32_t)(v42));
  t1087 = ((int32_t)(((int64_t)UINT64_C(1))));
  t1088 = ((t1086) != (t1087));
  t1089 = t1088;
  if (t1089) {
  t1090 = ((int32_t)(v55));
  t1091 = ((int32_t)(v37));
  t1089 = ((t1090) < (t1091));
  }
  t1092 = t1089;
  t1093 = t1092;
  if (t1093) {
  t1094 = ((int32_t)(v37));
  t1095 = ((int32_t)(((int64_t)UINT64_C(1))));
  t1096 = ((t1094) == (t1095));
  t1097 = t1096;
  if (!t1097) {
  t1098 = ((int32_t)(v46));
  t1099 = ((int32_t)(v55));
  t1100 = ((int32_t)(((int32_t)((uint64_t)(t1098) + (uint64_t)(t1099)))));
  t1101 = ((int32_t)(v15));
  t1097 = ((t1100) < (t1101));
  }
  t1093 = t1097;
  }
  t1102 = t1093;
  if (t1102) {
  {
  t1103 = ((int64_t)(((int64_t)(v47))));
  t1104 = ((int64_t)(((int64_t)(v15))));
  t1105 = ((int64_t)(((int64_t)((uint64_t)(t1103) * (uint64_t)(t1104)))));
  t1106 = ((int32_t)(v46));
  t1107 = ((int32_t)(v55));
  t1108 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t1106) + (uint64_t)(t1107)))))));
  t1109 = ((int64_t)(((int64_t)((uint64_t)(t1105) + (uint64_t)(t1108)))));
  t1110 = ((int64_t)(((int64_t)(v16))));
  t1111 = ((int64_t)(((int64_t)((uint64_t)(t1109) * (uint64_t)(t1110)))));
  t1112 = ((int64_t)(((int64_t)(v48))));
  t1113 = ((int64_t)(((int64_t)((uint64_t)(t1111) + (uint64_t)(t1112)))));
  t1114 = ((int64_t)(((int64_t)(v42))));
  t1115 = ((int64_t)(((int64_t)((uint64_t)(t1113) * (uint64_t)(t1114)))));
  t1116 = ((int64_t)(((int64_t)(v39))));
  v105 = ((int64_t)(((int64_t)((uint64_t)(t1115) + (uint64_t)(t1116)))));
  t1117 = ((uint64_t)(v7));
  t1118 = ((int64_t)(v105));
  t1119 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1120 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t1118) * (uint64_t)(t1119)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t1121 = v55;
  t1122 = ((float *)(v56).data) + gg_index(t1121, ((int64_t)UINT64_C(2)));
  t1123 = ((float)((*t1122)));
  fg175v_st_f32(t1117, t1120, t1123);
  t1124 = ((uint64_t)(v7));
  t1125 = ((int64_t)(v105));
  t1126 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1127 = ((int64_t)(((int64_t)((uint64_t)(t1125) * (uint64_t)(t1126)))));
  t1128 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1129 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t1127) + (uint64_t)(t1128)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t1130 = v55;
  t1131 = ((float *)(v57).data) + gg_index(t1130, ((int64_t)UINT64_C(2)));
  t1132 = ((float)((*t1131)));
  fg175v_st_f32(t1124, t1129, t1132);
  }
  }
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  float t8;
  float t9;
  float t10;
  float t11;
  int32_t t12;
  float t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int32_t t21;
  int32_t t22;
  int32_t t23;
  int32_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int32_t t31;
  int32_t t32;
  int32_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int32_t t37;
  int32_t t38;
  int32_t t39;
  int32_t t40;
  int32_t t41;
  int32_t t42;
  int32_t t43;
  int32_t t44;
  uint64_t t45;
  uint64_t t46;
  uint64_t t47;
  uint64_t t48;
  int32_t t49;
  int32_t t50;
  int32_t t51;
  int32_t t52;
  int32_t t53;
  int32_t t54;
  int32_t t55;
  uint64_t t56;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t1 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t6 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t7 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((float)(gg_double_bits(UINT64_C(0))));
  t9 = ((float)(gg_double_bits(UINT64_C(0))));
  t10 = ((float)(gg_double_bits(UINT64_C(0))));
  t11 = ((float)(gg_double_bits(UINT64_C(0))));
  t12 = ((int32_t)(((int64_t)UINT64_C(0))));
  t13 = ((float)(gg_double_bits(UINT64_C(0))));
  t14 = ((int32_t)(((int64_t)UINT64_C(0))));
  t15 = ((int32_t)(((int64_t)UINT64_C(0))));
  t16 = ((int32_t)(((int64_t)UINT64_C(0))));
  t17 = ((int32_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t21 = ((int32_t)(((int64_t)UINT64_C(0))));
  t22 = ((int32_t)(((int64_t)UINT64_C(0))));
  t23 = ((int32_t)(((int64_t)UINT64_C(0))));
  t24 = ((int32_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t31 = ((int32_t)(((int64_t)UINT64_C(0))));
  t32 = ((int32_t)(((int64_t)UINT64_C(0))));
  t33 = ((int32_t)(((int64_t)UINT64_C(0))));
  t34 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t37 = ((int32_t)(((int64_t)UINT64_C(1))));
  t38 = ((int32_t)(((int64_t)UINT64_C(0))));
  t39 = ((int32_t)(((int64_t)UINT64_C(0))));
  t40 = ((int32_t)(((int64_t)UINT64_C(0))));
  t41 = ((int32_t)(((int64_t)UINT64_C(1))));
  t42 = ((int32_t)(((int64_t)UINT64_C(1))));
  t43 = ((int32_t)(((int64_t)UINT64_C(0))));
  t44 = ((int32_t)(((int64_t)UINT64_C(0))));
  t45 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  gg_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, t20, t21, t22, t23, t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, t34, t35, t36, t37, t38, t39, t40, t41, t42, t43, t44, t45, base + 0);
  t46 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t47 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t48 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t49 = ((int32_t)(((int64_t)UINT64_C(1))));
  t50 = ((int32_t)(((int64_t)UINT64_C(0))));
  t51 = ((int32_t)(((int64_t)UINT64_C(0))));
  t52 = ((int32_t)(((int64_t)UINT64_C(0))));
  t53 = ((int32_t)(((int64_t)UINT64_C(1))));
  t54 = ((int32_t)(((int64_t)UINT64_C(1))));
  t55 = ((int32_t)(((int64_t)UINT64_C(0))));
  t56 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  gg_function_33(t46, t47, t48, t49, t50, t51, t52, t53, t54, t55, t56, base + 0);
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
