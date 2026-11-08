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
static gg_type_13 gg_function_27(uint64_t p0, int64_t p1, int64_t base);
static float gg_function_30(float p0, int64_t base);
static void gg_function_32(uint64_t p0, uint64_t p1, uint64_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, uint64_t p10, int64_t base);
static void gg_function_32(uint64_t p0, uint64_t p1, uint64_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, uint64_t p10, int64_t base) {
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
static float gg_function_30(float p0, int64_t base) {
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
static gg_type_13 gg_function_27(uint64_t p0, int64_t p1, int64_t base) {
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
  float t79;
  float t80;
  int64_t t81;
  int32_t t82;
  int32_t t83;
  int32_t t84;
  float * t85;
  int32_t t86;
  float * t87;
  int32_t t88;
  int32_t t89;
  int32_t t90;
  int32_t t91;
  uint8_t t92;
  uint8_t t93;
  int32_t t94;
  int32_t t95;
  uint8_t t96;
  uint8_t t97;
  int32_t t98;
  int32_t t99;
  int32_t t100;
  int32_t t101;
  uint8_t t102;
  uint8_t t103;
  int32_t t104;
  int32_t t105;
  int32_t t106;
  int32_t t107;
  uint8_t t108;
  int32_t t109;
  int32_t t110;
  uint64_t t111;
  int32_t t112;
  int32_t t113;
  int32_t t114;
  int32_t t115;
  uint64_t t116;
  int32_t t117;
  int32_t t118;
  int32_t t119;
  int32_t t120;
  int32_t t121;
  uint8_t t122;
  uint64_t t123;
  int32_t t124;
  int32_t t125;
  int32_t t126;
  int32_t t127;
  int32_t t128;
  int32_t t129;
  uint64_t t130;
  float t131;
  uint64_t t132;
  int32_t t133;
  int32_t t134;
  int32_t t135;
  int32_t t136;
  int32_t t137;
  int32_t t138;
  int32_t t139;
  int32_t t140;
  uint64_t t141;
  float t142;
  uint64_t t143;
  int64_t t144;
  int64_t t145;
  uint64_t t146;
  int32_t t147;
  int32_t t148;
  uint64_t t149;
  int32_t t150;
  int32_t t151;
  uint64_t t152;
  uint64_t t153;
  float t154;
  int32_t t155;
  int32_t t156;
  int64_t t157;
  int32_t t158;
  int32_t t159;
  int32_t t160;
  int32_t t161;
  int32_t t162;
  int32_t t163;
  int32_t t164;
  int32_t t165;
  int64_t t166;
  int32_t t167;
  uint64_t t168;
  int32_t t169;
  int32_t t170;
  int32_t t171;
  int32_t t172;
  int64_t t173;
  float t174;
  float t175;
  int64_t t176;
  float t177;
  float t178;
  int64_t t179;
  int32_t t180;
  int32_t t181;
  int32_t t182;
  int32_t t183;
  int32_t t184;
  int32_t t185;
  gg_slice t186;
  int32_t t187;
  int32_t t188;
  int32_t t189;
  int32_t t190;
  int32_t t191;
  int32_t t192;
  int32_t t193;
  int32_t * t194;
  uint64_t t195;
  int32_t t196;
  int32_t t197;
  int32_t t198;
  int32_t t199;
  uint64_t t200;
  int32_t t201;
  int32_t t202;
  int32_t t203;
  int32_t t204;
  int32_t t205;
  int32_t t206;
  int32_t t207;
  int32_t t208;
  int32_t t209;
  int32_t t210;
  int32_t t211;
  gg_slice t212;
  int32_t t213;
  int32_t t214;
  int32_t t215;
  int32_t t216;
  int32_t t217;
  int32_t t218;
  int32_t t219;
  float * t220;
  uint64_t t221;
  uint64_t t222;
  float t223;
  gg_slice t224;
  int32_t t225;
  int32_t t226;
  int32_t t227;
  int32_t t228;
  int32_t t229;
  int32_t t230;
  int32_t t231;
  float * t232;
  uint64_t t233;
  uint64_t t234;
  uint64_t t235;
  uint64_t t236;
  float t237;
  int32_t t238;
  int32_t t239;
  int32_t t240;
  int32_t t241;
  uint64_t t242;
  uint64_t t243;
  uint8_t t244;
  uint64_t t245;
  int32_t t246;
  int32_t t247;
  int32_t t248;
  int32_t t249;
  int32_t t250;
  int32_t t251;
  uint64_t t252;
  int32_t t253;
  uint64_t t254;
  int64_t t255;
  int32_t t256;
  int32_t t257;
  int64_t t258;
  uint64_t t259;
  uint64_t t260;
  int64_t t261;
  int32_t t262;
  int32_t t263;
  int64_t t264;
  uint64_t t265;
  uint64_t t266;
  int32_t t267;
  int32_t t268;
  int32_t t269;
  int32_t t270;
  uint64_t t271;
  int32_t t272;
  int32_t t273;
  int64_t t274;
  int64_t t275;
  float t276;
  float t277;
  int64_t t278;
  float t279;
  float t280;
  int64_t t281;
  int32_t t282;
  int32_t t283;
  int32_t t284;
  float * t285;
  int32_t t286;
  float * t287;
  int32_t t288;
  int32_t t289;
  int32_t t290;
  int32_t t291;
  int32_t t292;
  int32_t t293;
  int32_t t294;
  int32_t t295;
  int32_t t296;
  int32_t t297;
  uint64_t t298;
  int64_t t299;
  int64_t t300;
  uint64_t t301;
  uint64_t t302;
  int32_t t303;
  gg_slice t304;
  int32_t t305;
  int32_t t306;
  int32_t t307;
  int32_t * t308;
  int32_t t309;
  gg_slice t310;
  int32_t t311;
  int32_t t312;
  int32_t t313;
  int32_t t314;
  int32_t t315;
  int32_t * t316;
  int32_t t317;
  gg_slice t318;
  int32_t t319;
  int32_t t320;
  int32_t t321;
  float * t322;
  float t323;
  gg_slice t324;
  int32_t t325;
  int32_t t326;
  int32_t t327;
  float * t328;
  float t329;
  gg_slice t330;
  int32_t t331;
  int32_t t332;
  int32_t t333;
  int32_t t334;
  int32_t t335;
  float * t336;
  float t337;
  gg_slice t338;
  int32_t t339;
  int32_t t340;
  int32_t t341;
  int32_t t342;
  int32_t t343;
  float * t344;
  float t345;
  float t346;
  uint64_t t347;
  uint64_t t348;
  uint8_t t349;
  uint8_t t350;
  int32_t t351;
  int32_t t352;
  uint8_t t353;
  uint8_t t354;
  int32_t t355;
  int32_t t356;
  int32_t t357;
  int32_t t358;
  uint8_t t359;
  float t360;
  float t361;
  uint64_t t362;
  int32_t t363;
  int32_t t364;
  int32_t t365;
  int32_t t366;
  int32_t t367;
  int32_t t368;
  uint64_t t369;
  uint32_t t370;
  uint32_t t371;
  float t372;
  float t373;
  float t374;
  int32_t t375;
  float * t376;
  int32_t t377;
  float * t378;
  float t379;
  float t380;
  float t381;
  float t382;
  float t383;
  float t384;
  float t385;
  int32_t t386;
  int32_t t387;
  uint8_t t388;
  int32_t t389;
  float * t390;
  int32_t t391;
  int32_t t392;
  int32_t t393;
  int32_t t394;
  int32_t t395;
  int32_t t396;
  int32_t t397;
  float * t398;
  float t399;
  int32_t t400;
  float * t401;
  float t402;
  float t403;
  float t404;
  int32_t t405;
  float * t406;
  int32_t t407;
  float * t408;
  int32_t t409;
  float * t410;
  int32_t t411;
  float * t412;
  float t413;
  int32_t t414;
  float * t415;
  float t416;
  float t417;
  float t418;
  int32_t t419;
  float * t420;
  int32_t t421;
  float * t422;
  float t423;
  float t424;
  float t425;
  int32_t t426;
  float * t427;
  float t428;
  uint64_t t429;
  int32_t t430;
  int32_t t431;
  int32_t t432;
  int32_t t433;
  uint64_t t434;
  int32_t t435;
  float * t436;
  float t437;
  int32_t t438;
  float * t439;
  int32_t t440;
  int32_t t441;
  int32_t t442;
  int32_t t443;
  int32_t t444;
  int32_t t445;
  int32_t t446;
  int32_t t447;
  int32_t t448;
  int32_t t449;
  int32_t t450;
  float * t451;
  uint64_t t452;
  int32_t t453;
  int32_t t454;
  int32_t t455;
  int32_t t456;
  uint64_t t457;
  float t458;
  int32_t t459;
  int32_t t460;
  int32_t t461;
  int32_t t462;
  uint64_t t463;
  int64_t t464;
  int64_t t465;
  uint64_t t466;
  uint64_t t467;
  int32_t t468;
  int32_t t469;
  int32_t t470;
  int32_t t471;
  int32_t t472;
  int32_t t473;
  int64_t t474;
  int32_t t475;
  int32_t t476;
  gg_slice t477;
  int32_t t478;
  int32_t t479;
  int32_t t480;
  int32_t t481;
  int32_t t482;
  int32_t t483;
  int32_t t484;
  float * t485;
  int32_t t486;
  int32_t t487;
  uint8_t t488;
  int32_t t489;
  float * t490;
  float t491;
  gg_slice t492;
  int32_t t493;
  int32_t t494;
  int32_t t495;
  int32_t t496;
  int32_t t497;
  int32_t t498;
  int32_t t499;
  float * t500;
  float t501;
  float t502;
  int32_t t503;
  float * t504;
  float t505;
  float t506;
  float t507;
  float t508;
  float t509;
  int32_t t510;
  float * t511;
  float t512;
  gg_slice t513;
  int32_t t514;
  int32_t t515;
  int32_t t516;
  int32_t t517;
  int32_t t518;
  int32_t t519;
  int32_t t520;
  float * t521;
  float t522;
  float t523;
  gg_slice t524;
  int32_t t525;
  int32_t t526;
  int32_t t527;
  int32_t t528;
  int32_t t529;
  int32_t t530;
  int32_t t531;
  float * t532;
  int32_t t533;
  int32_t t534;
  uint8_t t535;
  int32_t t536;
  float * t537;
  float t538;
  gg_slice t539;
  int32_t t540;
  int32_t t541;
  int32_t t542;
  int32_t t543;
  int32_t t544;
  int32_t t545;
  int32_t t546;
  float * t547;
  float t548;
  float t549;
  int32_t t550;
  float * t551;
  float t552;
  float t553;
  float t554;
  float t555;
  float t556;
  int32_t t557;
  float * t558;
  float t559;
  gg_slice t560;
  int32_t t561;
  int32_t t562;
  int32_t t563;
  int32_t t564;
  int32_t t565;
  int32_t t566;
  int32_t t567;
  float * t568;
  float t569;
  float t570;
  gg_slice t571;
  int32_t t572;
  int32_t t573;
  int32_t t574;
  int32_t t575;
  int32_t t576;
  int32_t t577;
  int32_t t578;
  int32_t t579;
  int32_t t580;
  float * t581;
  int32_t t582;
  int32_t t583;
  uint8_t t584;
  int32_t t585;
  float * t586;
  float t587;
  gg_slice t588;
  int32_t t589;
  int32_t t590;
  int32_t t591;
  int32_t t592;
  int32_t t593;
  int32_t t594;
  int32_t t595;
  int32_t t596;
  int32_t t597;
  float * t598;
  float t599;
  float t600;
  int32_t t601;
  float * t602;
  float t603;
  float t604;
  float t605;
  float t606;
  float t607;
  int32_t t608;
  float * t609;
  float t610;
  gg_slice t611;
  int32_t t612;
  int32_t t613;
  int32_t t614;
  int32_t t615;
  int32_t t616;
  int32_t t617;
  int32_t t618;
  int32_t t619;
  int32_t t620;
  float * t621;
  float t622;
  float t623;
  gg_slice t624;
  int32_t t625;
  int32_t t626;
  int32_t t627;
  int32_t t628;
  int32_t t629;
  int32_t t630;
  int32_t t631;
  int32_t t632;
  int32_t t633;
  float * t634;
  int32_t t635;
  int32_t t636;
  uint8_t t637;
  int32_t t638;
  float * t639;
  float t640;
  gg_slice t641;
  int32_t t642;
  int32_t t643;
  int32_t t644;
  int32_t t645;
  int32_t t646;
  int32_t t647;
  int32_t t648;
  int32_t t649;
  int32_t t650;
  float * t651;
  float t652;
  float t653;
  int32_t t654;
  float * t655;
  float t656;
  float t657;
  float t658;
  float t659;
  float t660;
  int32_t t661;
  float * t662;
  float t663;
  gg_slice t664;
  int32_t t665;
  int32_t t666;
  int32_t t667;
  int32_t t668;
  int32_t t669;
  int32_t t670;
  int32_t t671;
  int32_t t672;
  int32_t t673;
  float * t674;
  float t675;
  float t676;
  int32_t t677;
  int32_t t678;
  int32_t t679;
  int32_t t680;
  int32_t t681;
  int32_t t682;
  int64_t t683;
  int32_t t684;
  int32_t t685;
  int64_t t686;
  uint64_t t687;
  int32_t t688;
  int32_t t689;
  int64_t t690;
  int64_t t691;
  uint64_t t692;
  uint64_t t693;
  int32_t t694;
  int32_t t695;
  int64_t t696;
  int64_t t697;
  uint64_t t698;
  uint64_t t699;
  int32_t t700;
  int32_t t701;
  int32_t t702;
  int32_t t703;
  uint64_t t704;
  uint64_t t705;
  uint64_t t706;
  uint8_t t707;
  uint8_t t708;
  int32_t t709;
  int32_t t710;
  uint8_t t711;
  uint64_t t712;
  int32_t t713;
  int32_t t714;
  uint64_t t715;
  float t716;
  int32_t t717;
  int32_t t718;
  uint8_t t719;
  uint8_t t720;
  int32_t t721;
  int32_t t722;
  uint8_t t723;
  uint8_t t724;
  float t725;
  int32_t t726;
  float * t727;
  float t728;
  float t729;
  int32_t t730;
  float * t731;
  float t732;
  float t733;
  float t734;
  float t735;
  int32_t t736;
  float * t737;
  int32_t t738;
  float * t739;
  int32_t t740;
  float * t741;
  float t742;
  float t743;
  float t744;
  float t745;
  int32_t t746;
  int32_t t747;
  uint8_t t748;
  float t749;
  int32_t t750;
  float * t751;
  float t752;
  float t753;
  float t754;
  float t755;
  int32_t t756;
  int32_t t757;
  gg_slice t758;
  int32_t t759;
  int32_t t760;
  int32_t t761;
  int32_t t762;
  int32_t t763;
  float * t764;
  gg_slice t765;
  int32_t t766;
  int32_t t767;
  int32_t t768;
  int32_t t769;
  int32_t t770;
  float * t771;
  float t772;
  float t773;
  float t774;
  gg_slice t775;
  int32_t t776;
  int32_t t777;
  int32_t t778;
  int32_t t779;
  int32_t t780;
  float * t781;
  gg_slice t782;
  int32_t t783;
  int32_t t784;
  int32_t t785;
  int32_t t786;
  int32_t t787;
  float * t788;
  float t789;
  float t790;
  float t791;
  int32_t t792;
  int32_t t793;
  int32_t t794;
  int32_t t795;
  int32_t t796;
  int32_t t797;
  uint8_t t798;
  uint64_t t799;
  int32_t t800;
  int32_t t801;
  int32_t t802;
  int32_t t803;
  int32_t t804;
  int32_t t805;
  uint64_t t806;
  float t807;
  uint64_t t808;
  int32_t t809;
  int32_t t810;
  int32_t t811;
  int32_t t812;
  int32_t t813;
  int32_t t814;
  uint64_t t815;
  float t816;
  int32_t t817;
  int32_t t818;
  int32_t t819;
  int32_t t820;
  int32_t t821;
  int32_t t822;
  uint8_t t823;
  uint64_t t824;
  int32_t t825;
  int32_t t826;
  int32_t t827;
  int32_t t828;
  int32_t t829;
  int32_t t830;
  uint64_t t831;
  int32_t t832;
  float * t833;
  float t834;
  int32_t t835;
  int32_t t836;
  int32_t t837;
  int32_t t838;
  uint8_t t839;
  uint8_t t840;
  int32_t t841;
  int32_t t842;
  int32_t t843;
  int32_t t844;
  uint8_t t845;
  int32_t t846;
  int32_t t847;
  int32_t t848;
  int32_t t849;
  uint64_t t850;
  int32_t t851;
  int32_t t852;
  int32_t t853;
  int32_t t854;
  int32_t t855;
  int32_t t856;
  uint64_t t857;
  float t858;
  float t859;
  int32_t t860;
  float * t861;
  float t862;
  float t863;
  float t864;
  float t865;
  int32_t t866;
  float * t867;
  int32_t t868;
  int32_t t869;
  gg_slice t870;
  int32_t t871;
  int32_t t872;
  int32_t t873;
  int32_t t874;
  int32_t t875;
  float * t876;
  gg_slice t877;
  int32_t t878;
  int32_t t879;
  int32_t t880;
  int32_t t881;
  int32_t t882;
  float * t883;
  float t884;
  float t885;
  float t886;
  gg_slice t887;
  int32_t t888;
  int32_t t889;
  int32_t t890;
  int32_t t891;
  int32_t t892;
  float * t893;
  gg_slice t894;
  int32_t t895;
  int32_t t896;
  int32_t t897;
  int32_t t898;
  int32_t t899;
  float * t900;
  float t901;
  float t902;
  float t903;
  int32_t t904;
  int32_t t905;
  int32_t t906;
  int32_t t907;
  int32_t t908;
  int32_t t909;
  int32_t t910;
  int32_t t911;
  uint64_t t912;
  int32_t t913;
  int32_t t914;
  int32_t t915;
  int32_t t916;
  int32_t t917;
  int32_t t918;
  uint64_t t919;
  gg_slice t920;
  int32_t t921;
  int32_t t922;
  int32_t t923;
  int32_t t924;
  int32_t t925;
  int32_t t926;
  int32_t t927;
  float * t928;
  float t929;
  uint64_t t930;
  int32_t t931;
  int32_t t932;
  int32_t t933;
  int32_t t934;
  int32_t t935;
  int32_t t936;
  int32_t t937;
  int32_t t938;
  uint64_t t939;
  gg_slice t940;
  int32_t t941;
  int32_t t942;
  int32_t t943;
  int32_t t944;
  int32_t t945;
  int32_t t946;
  int32_t t947;
  float * t948;
  float t949;
  uint64_t t950;
  int32_t t951;
  int32_t t952;
  int32_t t953;
  int32_t t954;
  int32_t t955;
  int32_t t956;
  int32_t t957;
  int32_t t958;
  uint64_t t959;
  gg_slice t960;
  int32_t t961;
  int32_t t962;
  int32_t t963;
  int32_t t964;
  int32_t t965;
  int32_t t966;
  int32_t t967;
  int32_t t968;
  int32_t t969;
  float * t970;
  float t971;
  uint64_t t972;
  int32_t t973;
  int32_t t974;
  int32_t t975;
  int32_t t976;
  int32_t t977;
  int32_t t978;
  int32_t t979;
  int32_t t980;
  uint64_t t981;
  gg_slice t982;
  int32_t t983;
  int32_t t984;
  int32_t t985;
  int32_t t986;
  int32_t t987;
  int32_t t988;
  int32_t t989;
  int32_t t990;
  int32_t t991;
  float * t992;
  float t993;
  int32_t t994;
  int32_t t995;
  int32_t t996;
  float * t997;
  float t998;
  float t999;
  int32_t t1000;
  float * t1001;
  int32_t t1002;
  float * t1003;
  float t1004;
  int32_t t1005;
  int32_t t1006;
  uint8_t t1007;
  uint64_t t1008;
  int32_t t1009;
  int32_t t1010;
  int32_t t1011;
  int32_t t1012;
  int32_t t1013;
  int32_t t1014;
  uint64_t t1015;
  int32_t t1016;
  float * t1017;
  float t1018;
  int32_t t1019;
  float * t1020;
  uint64_t t1021;
  int32_t t1022;
  int32_t t1023;
  int32_t t1024;
  int32_t t1025;
  int32_t t1026;
  int32_t t1027;
  uint64_t t1028;
  float t1029;
  int32_t t1030;
  float * t1031;
  int32_t t1032;
  float * t1033;
  float t1034;
  int32_t t1035;
  int32_t t1036;
  int32_t t1037;
  int32_t t1038;
  float t1039;
  uint64_t t1040;
  int32_t t1041;
  int32_t t1042;
  int32_t t1043;
  int32_t t1044;
  int32_t t1045;
  int32_t t1046;
  uint64_t t1047;
  float t1048;
  float t1049;
  int32_t t1050;
  int32_t t1051;
  int32_t t1052;
  int32_t t1053;
  uint8_t t1054;
  float t1055;
  int32_t t1056;
  float * t1057;
  float t1058;
  int64_t t1059;
  int64_t t1060;
  int64_t t1061;
  int32_t t1062;
  int32_t t1063;
  int64_t t1064;
  int64_t t1065;
  int64_t t1066;
  int64_t t1067;
  int64_t t1068;
  int64_t t1069;
  int64_t t1070;
  int64_t t1071;
  int64_t t1072;
  uint64_t t1073;
  int64_t t1074;
  int64_t t1075;
  int64_t t1076;
  int32_t t1077;
  int32_t t1078;
  int64_t t1079;
  int64_t t1080;
  int64_t t1081;
  uint64_t t1082;
  float t1083;
  int32_t t1084;
  int32_t t1085;
  int32_t t1086;
  int32_t t1087;
  int32_t t1088;
  int32_t t1089;
  uint8_t t1090;
  int32_t t1091;
  int32_t t1092;
  int32_t t1093;
  int32_t t1094;
  uint8_t t1095;
  uint8_t t1096;
  int32_t t1097;
  int32_t t1098;
  uint8_t t1099;
  uint8_t t1100;
  int32_t t1101;
  int32_t t1102;
  uint8_t t1103;
  uint8_t t1104;
  int32_t t1105;
  int32_t t1106;
  int32_t t1107;
  int32_t t1108;
  uint8_t t1109;
  int64_t t1110;
  int64_t t1111;
  int64_t t1112;
  int32_t t1113;
  int32_t t1114;
  int64_t t1115;
  int64_t t1116;
  int64_t t1117;
  int64_t t1118;
  int64_t t1119;
  int64_t t1120;
  int64_t t1121;
  int64_t t1122;
  int64_t t1123;
  uint64_t t1124;
  int64_t t1125;
  int64_t t1126;
  uint64_t t1127;
  int32_t t1128;
  float * t1129;
  float t1130;
  uint64_t t1131;
  int64_t t1132;
  int64_t t1133;
  int64_t t1134;
  int64_t t1135;
  uint64_t t1136;
  int32_t t1137;
  float * t1138;
  float t1139;
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
  gg_type_8 v61;
  int32_t v62;
  int32_t v63;
  int32_t v64;
  int32_t v65;
  uint64_t v66;
  int32_t v67;
  gg_type_11 v68;
  gg_type_12 v69;
  gg_type_12 v70;
  int32_t v71;
  int32_t v72;
  uint64_t v73;
  int32_t v74;
  uint64_t v75;
  uint64_t v76;
  uint64_t v77;
  int64_t v78;
  gg_type_8 v79;
  gg_type_8 v80;
  int32_t v81;
  int32_t v82;
  float v83;
  float v84;
  int32_t v85;
  int32_t v86;
  int32_t v87;
  gg_type_13 v88;
  float v89;
  int32_t v90;
  float v91;
  float v92;
  int32_t v93;
  int32_t v94;
  float v95;
  float v96;
  int32_t v97;
  int32_t v98;
  int32_t v99;
  int32_t v100;
  float v101;
  int32_t v102;
  int64_t v103;
  int64_t v104;
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
  memset(&t79, 0, sizeof(t79));
  t80 = ((float)(t79));
  for (t81=0; t81<2; t81++) (v61).data[t81] = t80;
  v62 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t82 = ((int32_t)(v62));
  t83 = ((int32_t)(v37));
  if (!(((t82) < (t83)))) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t84 = v62;
  t85 = ((float *)(v56).data) + gg_index(t84, ((int64_t)UINT64_C(2)));
  (*t85) = gg_function_5(base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t86 = v62;
  t87 = ((float *)(v57).data) + gg_index(t86, ((int64_t)UINT64_C(2)));
  (*t87) = ((float)(gg_double_bits(UINT64_C(0))));
  t88 = v62;
  t89 = ((int32_t)(((int64_t)UINT64_C(1))));
  v62 = ((int32_t)((uint64_t)(t88) + (uint64_t)(t89)));
  }
  goto label1;
label2:;
  fg175v_sync();
  v63 = ((int32_t)(v44));
  t90 = ((int32_t)(((int64_t)UINT64_C(4))));
  t91 = ((int32_t)(v37));
  t92 = ((t90) > (t91));
  t93 = t92;
  if (t93) {
  t94 = ((int32_t)(v63));
  t95 = ((int32_t)(v37));
  t93 = ((t94) >= (t95));
  }
  t96 = t93;
  t97 = (!(t96));
  if (t97) {
  {
  t98 = ((int32_t)(v63));
  t99 = ((int32_t)(((int64_t)UINT64_C(256))));
  v64 = ((int32_t)(((int32_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  t100 = ((int32_t)(v37));
  t101 = ((int32_t)(((int64_t)UINT64_C(1))));
  t102 = ((t100) > (t101));
  t103 = t102;
  if (t103) {
  t104 = ((int32_t)(v46));
  t105 = ((int32_t)(v63));
  t106 = ((int32_t)(((int32_t)((uint64_t)(t104) + (uint64_t)(t105)))));
  t107 = ((int32_t)(v15));
  t103 = ((t106) >= (t107));
  }
  t108 = t103;
  if (t108) {
  {
  v65 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t109 = ((int32_t)(v65));
  t110 = ((int32_t)(((int64_t)UINT64_C(64))));
  if (!(((t109) < (t110)))) goto label5;
  {
  t111 = ((uint64_t)(v45));
  t112 = ((int32_t)(v64));
  t113 = ((int32_t)(v65));
  t114 = ((int32_t)(((int32_t)((uint64_t)(t112) + (uint64_t)(t113)))));
  t115 = ((int32_t)(v43));
  t116 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t114) + (uint64_t)(t115)))))));
  t117 = ((int32_t)(((int64_t)UINT64_C(0))));
  fg175v_sm_st_i32(t111, t116, t117);
  t118 = v65;
  t119 = ((int32_t)(((int64_t)UINT64_C(32))));
  v65 = ((int32_t)((uint64_t)(t118) + (uint64_t)(t119)));
  }
  goto label4;
label5:;
  t120 = ((int32_t)(v43));
  t121 = ((int32_t)(((int64_t)UINT64_C(8))));
  t122 = ((t120) < (t121));
  if (t122) {
  {
  t123 = ((uint64_t)(v45));
  t124 = ((int32_t)(v64));
  t125 = ((int32_t)(((int64_t)UINT64_C(64))));
  t126 = ((int32_t)(((int32_t)((uint64_t)(t124) + (uint64_t)(t125)))));
  t127 = ((int32_t)(((int64_t)UINT64_C(2))));
  t128 = ((int32_t)(v43));
  t129 = ((int32_t)(((int32_t)((uint64_t)(t127) * (uint64_t)(t128)))));
  t130 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t126) + (uint64_t)(t129)))))));
  t131 = ((float)(gg_double_bits(UINT64_C(0))));
  fg175v_sm_st_f32(t123, t130, t131);
  t132 = ((uint64_t)(v45));
  t133 = ((int32_t)(v64));
  t134 = ((int32_t)(((int64_t)UINT64_C(64))));
  t135 = ((int32_t)(((int32_t)((uint64_t)(t133) + (uint64_t)(t134)))));
  t136 = ((int32_t)(((int64_t)UINT64_C(2))));
  t137 = ((int32_t)(v43));
  t138 = ((int32_t)(((int32_t)((uint64_t)(t136) * (uint64_t)(t137)))));
  t139 = ((int32_t)(((int32_t)((uint64_t)(t135) + (uint64_t)(t138)))));
  t140 = ((int32_t)(((int64_t)UINT64_C(1))));
  t141 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t139) + (uint64_t)(t140)))))));
  t142 = ((float)(gg_double_bits(UINT64_C(0))));
  fg175v_sm_st_f32(t132, t141, t142);
  }
  }
  }
  } else {
  {
  t143 = ((uint64_t)(v50));
  t144 = ((int64_t)(v18));
  t145 = ((int64_t)(((int64_t)(v63))));
  t146 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t145)))))));
  v66 = ((uint64_t)(((uint64_t)((uint64_t)(t143) + (uint64_t)(t146)))));
  v67 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t147 = ((int32_t)(v67));
  t148 = ((int32_t)(((int64_t)UINT64_C(64))));
  if (!(((t147) < (t148)))) goto label8;
  {
  t149 = ((uint64_t)(v66));
  t150 = ((int32_t)(v67));
  t151 = ((int32_t)(((int64_t)UINT64_C(16))));
  t152 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t150) * (uint64_t)(t151)))))));
  t153 = ((uint64_t)(((uint64_t)((uint64_t)(t149) + (uint64_t)(t152)))));
  t154 = ((float)(v8));
  t155 = ((int32_t)(v64));
  t156 = ((int32_t)(v67));
  t157 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t155) + (uint64_t)(t156)))))));
  t158 = ((int32_t)(v64));
  t159 = ((int32_t)(((int64_t)UINT64_C(64))));
  t160 = ((int32_t)(((int32_t)((uint64_t)(t158) + (uint64_t)(t159)))));
  t161 = ((int32_t)(((int64_t)UINT64_C(2))));
  t162 = ((int32_t)(v67));
  t163 = ((int32_t)(((int64_t)UINT64_C(8))));
  t164 = ((int32_t)(((int32_t)gg_div(t162, t163))));
  t165 = ((int32_t)(((int32_t)((uint64_t)(t161) * (uint64_t)(t164)))));
  t166 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t160) + (uint64_t)(t165)))))));
  t167 = ((int32_t)(v43));
  t168 = ((uint64_t)(v45));
  gg_function_9(t153, t154, t157, t166, t167, t168, base + 0);
  t169 = v67;
  t170 = ((int32_t)(((int64_t)UINT64_C(32))));
  v67 = ((int32_t)((uint64_t)(t169) + (uint64_t)(t170)));
  }
  goto label7;
label8:;
  }
  }
  }
  }
  fg175v_barrier();
  memset(&t171, 0, sizeof(t171));
  t172 = ((int32_t)(t171));
  for (t173=0; t173<4; t173++) (v68).data[t173] = t172;
  memset(&t174, 0, sizeof(t174));
  t175 = ((float)(t174));
  for (t176=0; t176<4; t176++) (v69).data[t176] = t175;
  memset(&t177, 0, sizeof(t177));
  t178 = ((float)(t177));
  for (t179=0; t179<4; t179++) (v70).data[t179] = t178;
  v62 = ((int32_t)(((int64_t)UINT64_C(0))));
label10:;
  t180 = ((int32_t)(v62));
  t181 = ((int32_t)(v37));
  if (!(((t180) < (t181)))) goto label11;
  {
  v71 = ((int32_t)(((int64_t)UINT64_C(0))));
label13:;
  t182 = ((int32_t)(v71));
  t183 = ((int32_t)(((int64_t)UINT64_C(64))));
  if (!(((t182) < (t183)))) goto label14;
  {
  t184 = ((int32_t)(v71));
  t185 = ((int32_t)(v43));
  v72 = ((int32_t)(((int32_t)((uint64_t)(t184) + (uint64_t)(t185)))));
  t186 = gg_view((v68).data, 4);
  t187 = ((int32_t)(v62));
  t188 = ((int32_t)(((int64_t)UINT64_C(2))));
  t189 = ((int32_t)(((int32_t)((uint64_t)(t187) * (uint64_t)(t188)))));
  t190 = ((int32_t)(v71));
  t191 = ((int32_t)(((int64_t)UINT64_C(32))));
  t192 = ((int32_t)(((int32_t)gg_div(t190, t191))));
  t193 = ((int32_t)((uint64_t)(t189) + (uint64_t)(t192)));
  t194 = ((int32_t *)t186.data) + gg_index(t193, t186.len);
  t195 = ((uint64_t)(v45));
  t196 = ((int32_t)(v62));
  t197 = ((int32_t)(((int64_t)UINT64_C(256))));
  t198 = ((int32_t)(((int32_t)((uint64_t)(t196) * (uint64_t)(t197)))));
  t199 = ((int32_t)(v72));
  t200 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t198) + (uint64_t)(t199)))))));
  t201 = fg175v_sm_i32(t195, t200);
  (*t194) = ((int32_t)(t201));
  t202 = ((int32_t)(v62));
  t203 = ((int32_t)(((int64_t)UINT64_C(256))));
  t204 = ((int32_t)(((int32_t)((uint64_t)(t202) * (uint64_t)(t203)))));
  t205 = ((int32_t)(((int64_t)UINT64_C(64))));
  t206 = ((int32_t)(((int32_t)((uint64_t)(t204) + (uint64_t)(t205)))));
  t207 = ((int32_t)(((int64_t)UINT64_C(2))));
  t208 = ((int32_t)(v72));
  t209 = ((int32_t)(((int64_t)UINT64_C(8))));
  t210 = ((int32_t)(((int32_t)gg_div(t208, t209))));
  t211 = ((int32_t)(((int32_t)((uint64_t)(t207) * (uint64_t)(t210)))));
  v73 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t206) + (uint64_t)(t211)))))));
  t212 = gg_view((v69).data, 4);
  t213 = ((int32_t)(v62));
  t214 = ((int32_t)(((int64_t)UINT64_C(2))));
  t215 = ((int32_t)(((int32_t)((uint64_t)(t213) * (uint64_t)(t214)))));
  t216 = ((int32_t)(v71));
  t217 = ((int32_t)(((int64_t)UINT64_C(32))));
  t218 = ((int32_t)(((int32_t)gg_div(t216, t217))));
  t219 = ((int32_t)((uint64_t)(t215) + (uint64_t)(t218)));
  t220 = ((float *)t212.data) + gg_index(t219, t212.len);
  t221 = ((uint64_t)(v45));
  t222 = ((uint64_t)(v73));
  t223 = fg175v_sm_f32(t221, t222);
  (*t220) = ((float)(t223));
  t224 = gg_view((v70).data, 4);
  t225 = ((int32_t)(v62));
  t226 = ((int32_t)(((int64_t)UINT64_C(2))));
  t227 = ((int32_t)(((int32_t)((uint64_t)(t225) * (uint64_t)(t226)))));
  t228 = ((int32_t)(v71));
  t229 = ((int32_t)(((int64_t)UINT64_C(32))));
  t230 = ((int32_t)(((int32_t)gg_div(t228, t229))));
  t231 = ((int32_t)((uint64_t)(t227) + (uint64_t)(t230)));
  t232 = ((float *)t224.data) + gg_index(t231, t224.len);
  t233 = ((uint64_t)(v45));
  t234 = ((uint64_t)(v73));
  t235 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t236 = ((uint64_t)(((uint64_t)((uint64_t)(t234) + (uint64_t)(t235)))));
  t237 = fg175v_sm_f32(t233, t236);
  (*t232) = ((float)(t237));
  t238 = v71;
  t239 = ((int32_t)(((int64_t)UINT64_C(32))));
  v71 = ((int32_t)((uint64_t)(t238) + (uint64_t)(t239)));
  }
  goto label13;
label14:;
  t240 = v62;
  t241 = ((int32_t)(((int64_t)UINT64_C(1))));
  v62 = ((int32_t)((uint64_t)(t240) + (uint64_t)(t241)));
  }
  goto label10;
label11:;
  fg175v_barrier();
  v74 = ((int32_t)(v22));
  t242 = ((uint64_t)(v5));
  t243 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t244 = ((t242) != (t243));
  if (t244) {
  {
  t245 = ((uint64_t)(v5));
  t246 = ((int32_t)(v47));
  t247 = ((int32_t)(v41));
  t248 = ((int32_t)(((int32_t)((uint64_t)(t246) * (uint64_t)(t247)))));
  t249 = ((int32_t)(v38));
  t250 = ((int32_t)(((int32_t)((uint64_t)(t248) + (uint64_t)(t249)))));
  t251 = ((int32_t)(((int64_t)UINT64_C(4))));
  t252 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t250) * (uint64_t)(t251)))))));
  t253 = fg175v_ld_i32(t245, t252);
  v74 = ((int32_t)(t253));
  }
  }
  t254 = ((uint64_t)(v51));
  t255 = ((int64_t)(v25));
  t256 = ((int32_t)(v39));
  t257 = ((int32_t)(((int64_t)UINT64_C(128))));
  t258 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t256) * (uint64_t)(t257)))))));
  t259 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t255) * (uint64_t)(t258)))))));
  v75 = ((uint64_t)(((uint64_t)((uint64_t)(t254) + (uint64_t)(t259)))));
  t260 = ((uint64_t)(v52));
  t261 = ((int64_t)(v28));
  t262 = ((int32_t)(v39));
  t263 = ((int32_t)(((int64_t)UINT64_C(128))));
  t264 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t262) * (uint64_t)(t263)))))));
  t265 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t261) * (uint64_t)(t264)))))));
  v76 = ((uint64_t)(((uint64_t)((uint64_t)(t260) + (uint64_t)(t265)))));
  t266 = ((uint64_t)(v53));
  t267 = ((int32_t)(v39));
  t268 = ((int32_t)(((int64_t)UINT64_C(128))));
  t269 = ((int32_t)(((int32_t)((uint64_t)(t267) * (uint64_t)(t268)))));
  t270 = ((int32_t)(((int64_t)UINT64_C(2))));
  t271 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t269) * (uint64_t)(t270)))))));
  v77 = ((uint64_t)(((uint64_t)((uint64_t)(t266) + (uint64_t)(t271)))));
  t272 = ((int32_t)(v39));
  t273 = ((int32_t)(((int64_t)UINT64_C(128))));
  v78 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t272) * (uint64_t)(t273)))))));
label16:;
  t274 = ((int64_t)(v78));
  t275 = ((int64_t)(((int64_t)(v74))));
  if (!(((t274) < (t275)))) goto label17;
  {
  memset(&t276, 0, sizeof(t276));
  t277 = ((float)(t276));
  for (t278=0; t278<2; t278++) (v79).data[t278] = t277;
  memset(&t279, 0, sizeof(t279));
  t280 = ((float)(t279));
  for (t281=0; t281<2; t281++) (v80).data[t281] = t280;
  v62 = ((int32_t)(((int64_t)UINT64_C(0))));
label19:;
  t282 = ((int32_t)(v62));
  t283 = ((int32_t)(v37));
  if (!(((t282) < (t283)))) goto label20;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t284 = v62;
  t285 = ((float *)(v80).data) + gg_index(t284, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t286 = v62;
  t287 = ((float *)(v56).data) + gg_index(t286, ((int64_t)UINT64_C(2)));
  (*t285) = ((float)((*t287)));
  t288 = v62;
  t289 = ((int32_t)(((int64_t)UINT64_C(1))));
  v62 = ((int32_t)((uint64_t)(t288) + (uint64_t)(t289)));
  }
  goto label19;
label20:;
  v81 = ((int32_t)(((int64_t)UINT64_C(0))));
label22:;
  t290 = ((int32_t)(v81));
  t291 = ((int32_t)(((int64_t)UINT64_C(32))));
  if (!(((t290) < (t291)))) goto label23;
  {
  t292 = ((int32_t)(v44));
  t293 = ((int32_t)(((int64_t)UINT64_C(32))));
  t294 = ((int32_t)(((int32_t)((uint64_t)(t292) * (uint64_t)(t293)))));
  t295 = ((int32_t)(v81));
  v82 = ((int32_t)(((int32_t)((uint64_t)(t294) + (uint64_t)(t295)))));
  v62 = ((int32_t)(((int64_t)UINT64_C(0))));
label25:;
  t296 = ((int32_t)(v62));
  t297 = ((int32_t)(v37));
  if (!(((t296) < (t297)))) goto label26;
  {
  t298 = ((uint64_t)(v75));
  t299 = ((int64_t)(v25));
  t300 = ((int64_t)(((int64_t)(v82))));
  t301 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t299) * (uint64_t)(t300)))))));
  t302 = ((uint64_t)(((uint64_t)((uint64_t)(t298) + (uint64_t)(t301)))));
  t303 = ((int32_t)(v43));
  t304 = gg_view((v68).data, 4);
  t305 = ((int32_t)(v62));
  t306 = ((int32_t)(((int64_t)UINT64_C(2))));
  t307 = ((int32_t)((uint64_t)(t305) * (uint64_t)(t306)));
  t308 = ((int32_t *)t304.data) + gg_index(t307, t304.len);
  t309 = ((int32_t)((*t308)));
  t310 = gg_view((v68).data, 4);
  t311 = ((int32_t)(v62));
  t312 = ((int32_t)(((int64_t)UINT64_C(2))));
  t313 = ((int32_t)(((int32_t)((uint64_t)(t311) * (uint64_t)(t312)))));
  t314 = ((int32_t)(((int64_t)UINT64_C(1))));
  t315 = ((int32_t)((uint64_t)(t313) + (uint64_t)(t314)));
  t316 = ((int32_t *)t310.data) + gg_index(t315, t310.len);
  t317 = ((int32_t)((*t316)));
  t318 = gg_view((v69).data, 4);
  t319 = ((int32_t)(v62));
  t320 = ((int32_t)(((int64_t)UINT64_C(2))));
  t321 = ((int32_t)((uint64_t)(t319) * (uint64_t)(t320)));
  t322 = ((float *)t318.data) + gg_index(t321, t318.len);
  t323 = ((float)((*t322)));
  t324 = gg_view((v70).data, 4);
  t325 = ((int32_t)(v62));
  t326 = ((int32_t)(((int64_t)UINT64_C(2))));
  t327 = ((int32_t)((uint64_t)(t325) * (uint64_t)(t326)));
  t328 = ((float *)t324.data) + gg_index(t327, t324.len);
  t329 = ((float)((*t328)));
  t330 = gg_view((v69).data, 4);
  t331 = ((int32_t)(v62));
  t332 = ((int32_t)(((int64_t)UINT64_C(2))));
  t333 = ((int32_t)(((int32_t)((uint64_t)(t331) * (uint64_t)(t332)))));
  t334 = ((int32_t)(((int64_t)UINT64_C(1))));
  t335 = ((int32_t)((uint64_t)(t333) + (uint64_t)(t334)));
  t336 = ((float *)t330.data) + gg_index(t335, t330.len);
  t337 = ((float)((*t336)));
  t338 = gg_view((v70).data, 4);
  t339 = ((int32_t)(v62));
  t340 = ((int32_t)(((int64_t)UINT64_C(2))));
  t341 = ((int32_t)(((int32_t)((uint64_t)(t339) * (uint64_t)(t340)))));
  t342 = ((int32_t)(((int64_t)UINT64_C(1))));
  t343 = ((int32_t)((uint64_t)(t341) + (uint64_t)(t342)));
  t344 = ((float *)t338.data) + gg_index(t343, t338.len);
  t345 = ((float)((*t344)));
  v83 = gg_function_19(t302, t303, t309, t317, t323, t329, t337, t345, base + 0);
  t346 = ((float)(v83));
  v83 = gg_function_24(t346, base + 0);
  t347 = ((uint64_t)(v3));
  t348 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t349 = ((t347) != (t348));
  t350 = t349;
  if (t350) {
  t351 = ((int32_t)(v37));
  t352 = ((int32_t)(((int64_t)UINT64_C(1))));
  t353 = ((t351) == (t352));
  t354 = t353;
  if (!t354) {
  t355 = ((int32_t)(v46));
  t356 = ((int32_t)(v62));
  t357 = ((int32_t)(((int32_t)((uint64_t)(t355) + (uint64_t)(t356)))));
  t358 = ((int32_t)(v15));
  t354 = ((t357) < (t358));
  }
  t350 = t354;
  }
  t359 = t350;
  if (t359) {
  {
  t360 = v83;
  t361 = ((float)(v54));
  t362 = ((uint64_t)(v77));
  t363 = ((int32_t)(v62));
  t364 = ((int32_t)(v22));
  t365 = ((int32_t)(((int32_t)((uint64_t)(t363) * (uint64_t)(t364)))));
  t366 = ((int32_t)(v82));
  t367 = ((int32_t)(((int32_t)((uint64_t)(t365) + (uint64_t)(t366)))));
  t368 = ((int32_t)(((int64_t)UINT64_C(2))));
  t369 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t367) * (uint64_t)(t368)))))));
  t370 = fg175v_ld_u16(t362, t369);
  t371 = ((uint32_t)(t370));
  t372 = fg175v_h2f(t371);
  t373 = ((float)(t372));
  t374 = ((float)(((t361) * (t373))));
  v83 = ((t360) + (t374));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t375 = v62;
  t376 = ((float *)(v80).data) + gg_index(t375, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t377 = v62;
  t378 = ((float *)(v80).data) + gg_index(t377, ((int64_t)UINT64_C(2)));
  t379 = ((float)((*t378)));
  t380 = ((float)(v83));
  t381 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t382 = ((float)(((float)(gg_double_bits(UINT64_C(4604418109348737398))))));
  t383 = ((float)(((t381) * (t382))));
  t384 = ((float)(((t380) + (t383))));
  t385 = fg175v_fmaxf(t379, t384);
  (*t376) = ((float)(t385));
  t386 = ((int32_t)(v43));
  t387 = ((int32_t)(v81));
  t388 = ((t386) == (t387));
  if (t388) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t389 = v62;
  t390 = ((float *)(v79).data) + gg_index(t389, ((int64_t)UINT64_C(2)));
  (*t390) = ((float)(v83));
  }
  }
  t391 = v62;
  t392 = ((int32_t)(((int64_t)UINT64_C(1))));
  v62 = ((int32_t)((uint64_t)(t391) + (uint64_t)(t392)));
  }
  goto label25;
label26:;
  t393 = v81;
  t394 = ((int32_t)(((int64_t)UINT64_C(1))));
  v81 = ((int32_t)((uint64_t)(t393) + (uint64_t)(t394)));
  }
  goto label22;
label23:;
  v62 = ((int32_t)(((int64_t)UINT64_C(0))));
label28:;
  t395 = ((int32_t)(v62));
  t396 = ((int32_t)(v37));
  if (!(((t395) < (t396)))) goto label29;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t397 = v62;
  t398 = ((float *)(v56).data) + gg_index(t397, ((int64_t)UINT64_C(2)));
  t399 = ((float)((*t398)));
  /* FG_DIRECT_SLICE_INDEX */
  t400 = v62;
  t401 = ((float *)(v80).data) + gg_index(t400, ((int64_t)UINT64_C(2)));
  t402 = ((float)((*t401)));
  t403 = ((float)(((t399) - (t402))));
  t404 = fg175v_expf(t403);
  v84 = ((float)(t404));
  /* FG_DIRECT_SLICE_INDEX */
  t405 = v62;
  t406 = ((float *)(v56).data) + gg_index(t405, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t407 = v62;
  t408 = ((float *)(v80).data) + gg_index(t407, ((int64_t)UINT64_C(2)));
  (*t406) = ((float)((*t408)));
  /* FG_DIRECT_SLICE_INDEX */
  t409 = v62;
  t410 = ((float *)(v79).data) + gg_index(t409, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t411 = v62;
  t412 = ((float *)(v79).data) + gg_index(t411, ((int64_t)UINT64_C(2)));
  t413 = ((float)((*t412)));
  /* FG_DIRECT_SLICE_INDEX */
  t414 = v62;
  t415 = ((float *)(v56).data) + gg_index(t414, ((int64_t)UINT64_C(2)));
  t416 = ((float)((*t415)));
  t417 = ((float)(((t413) - (t416))));
  t418 = fg175v_expf(t417);
  (*t410) = ((float)(t418));
  /* FG_DIRECT_SLICE_INDEX */
  t419 = v62;
  t420 = ((float *)(v57).data) + gg_index(t419, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t421 = v62;
  t422 = ((float *)(v57).data) + gg_index(t421, ((int64_t)UINT64_C(2)));
  t423 = ((float)((*t422)));
  t424 = ((float)(v84));
  t425 = ((float)(((t423) * (t424))));
  /* FG_DIRECT_SLICE_INDEX */
  t426 = v62;
  t427 = ((float *)(v79).data) + gg_index(t426, ((int64_t)UINT64_C(2)));
  t428 = ((float)((*t427)));
  (*t420) = ((float)(((t425) + (t428))));
  t429 = ((uint64_t)(v45));
  t430 = ((int32_t)(v62));
  t431 = ((int32_t)(((int64_t)UINT64_C(128))));
  t432 = ((int32_t)(((int32_t)((uint64_t)(t430) * (uint64_t)(t431)))));
  t433 = ((int32_t)(v55));
  t434 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t432) + (uint64_t)(t433)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t435 = v62;
  t436 = ((float *)(v79).data) + gg_index(t435, ((int64_t)UINT64_C(2)));
  t437 = ((float)((*t436)));
  fg175v_sm_st_f32(t429, t434, t437);
  /* FG_DIRECT_SLICE_INDEX */
  t438 = v62;
  t439 = ((float *)(v61).data) + gg_index(t438, ((int64_t)UINT64_C(2)));
  (*t439) = ((float)(v84));
  t440 = v62;
  t441 = ((int32_t)(((int64_t)UINT64_C(1))));
  v62 = ((int32_t)((uint64_t)(t440) + (uint64_t)(t441)));
  }
  goto label28;
label29:;
  fg175v_syncwarp();
  v85 = ((int32_t)(((int64_t)UINT64_C(0))));
label31:;
  t442 = ((int32_t)(v85));
  t443 = ((int32_t)(((int64_t)UINT64_C(32))));
  if (!(((t442) < (t443)))) goto label32;
  {
  t444 = ((int32_t)(v44));
  t445 = ((int32_t)(((int64_t)UINT64_C(32))));
  t446 = ((int32_t)(((int32_t)((uint64_t)(t444) * (uint64_t)(t445)))));
  t447 = ((int32_t)(v85));
  v86 = ((int32_t)(((int32_t)((uint64_t)(t446) + (uint64_t)(t447)))));
  v62 = ((int32_t)(((int64_t)UINT64_C(0))));
label34:;
  t448 = ((int32_t)(v62));
  t449 = ((int32_t)(v37));
  if (!(((t448) < (t449)))) goto label35;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t450 = v62;
  t451 = ((float *)(v60).data) + gg_index(t450, ((int64_t)UINT64_C(2)));
  t452 = ((uint64_t)(v45));
  t453 = ((int32_t)(v62));
  t454 = ((int32_t)(((int64_t)UINT64_C(128))));
  t455 = ((int32_t)(((int32_t)((uint64_t)(t453) * (uint64_t)(t454)))));
  t456 = ((int32_t)(v86));
  t457 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t455) + (uint64_t)(t456)))))));
  t458 = fg175v_sm_f32(t452, t457);
  (*t451) = ((float)(t458));
  t459 = v62;
  t460 = ((int32_t)(((int64_t)UINT64_C(1))));
  v62 = ((int32_t)((uint64_t)(t459) + (uint64_t)(t460)));
  }
  goto label34;
label35:;
  v87 = ((int32_t)(((int64_t)UINT64_C(0))));
label37:;
  t461 = ((int32_t)(v87));
  t462 = ((int32_t)(((int64_t)UINT64_C(128))));
  if (!(((t461) < (t462)))) goto label38;
  {
  t463 = ((uint64_t)(v76));
  t464 = ((int64_t)(v28));
  t465 = ((int64_t)(((int64_t)(v86))));
  t466 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t464) * (uint64_t)(t465)))))));
  t467 = ((uint64_t)(((uint64_t)((uint64_t)(t463) + (uint64_t)(t466)))));
  t468 = ((int32_t)(((int64_t)UINT64_C(2))));
  t469 = ((int32_t)(v87));
  t470 = ((int32_t)(((int32_t)((uint64_t)(t468) * (uint64_t)(t469)))));
  t471 = ((int32_t)(v43));
  t472 = ((int32_t)(((int64_t)UINT64_C(4))));
  t473 = ((int32_t)(((int32_t)((uint64_t)(t471) * (uint64_t)(t472)))));
  t474 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t470) + (uint64_t)(t473)))))));
  v88 = gg_function_27(t467, t474, base + 0);
  v62 = ((int32_t)(((int64_t)UINT64_C(0))));
label40:;
  t475 = ((int32_t)(v62));
  t476 = ((int32_t)(v37));
  if (!(((t475) < (t476)))) goto label41;
  {
  t477 = gg_view((v58).data, 8);
  t478 = ((int32_t)(v62));
  t479 = ((int32_t)(((int64_t)UINT64_C(4))));
  t480 = ((int32_t)(((int32_t)((uint64_t)(t478) * (uint64_t)(t479)))));
  t481 = ((int32_t)(v87));
  t482 = ((int32_t)(((int64_t)UINT64_C(32))));
  t483 = ((int32_t)(((int32_t)gg_div(t481, t482))));
  t484 = ((int32_t)((uint64_t)(t480) + (uint64_t)(t483)));
  t485 = ((float *)t477.data) + gg_index(t484, t477.len);
  t486 = ((int32_t)(v85));
  t487 = ((int32_t)(((int64_t)UINT64_C(0))));
  t488 = ((t486) == (t487));
  if (t488) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t489 = v62;
  t490 = ((float *)(v61).data) + gg_index(t489, ((int64_t)UINT64_C(2)));
  t491 = ((float)((*t490)));
  t492 = gg_view((v58).data, 8);
  t493 = ((int32_t)(v62));
  t494 = ((int32_t)(((int64_t)UINT64_C(4))));
  t495 = ((int32_t)(((int32_t)((uint64_t)(t493) * (uint64_t)(t494)))));
  t496 = ((int32_t)(v87));
  t497 = ((int32_t)(((int64_t)UINT64_C(32))));
  t498 = ((int32_t)(((int32_t)gg_div(t496, t497))));
  t499 = ((int32_t)((uint64_t)(t495) + (uint64_t)(t498)));
  t500 = ((float *)t492.data) + gg_index(t499, t492.len);
  t501 = ((float)((*t500)));
  t502 = ((float)((v88).f0));
  /* FG_DIRECT_SLICE_INDEX */
  t503 = v62;
  t504 = ((float *)(v60).data) + gg_index(t503, ((int64_t)UINT64_C(2)));
  t505 = ((float)((*t504)));
  t506 = fg175v_mul_rn(t502, t505);
  t507 = ((float)(t506));
  t508 = fg175v_fma_rn(t491, t501, t507);
  (*t485) = ((float)(t508));
  }
  } else {
  {
  t509 = ((float)((v88).f0));
  /* FG_DIRECT_SLICE_INDEX */
  t510 = v62;
  t511 = ((float *)(v60).data) + gg_index(t510, ((int64_t)UINT64_C(2)));
  t512 = ((float)((*t511)));
  t513 = gg_view((v58).data, 8);
  t514 = ((int32_t)(v62));
  t515 = ((int32_t)(((int64_t)UINT64_C(4))));
  t516 = ((int32_t)(((int32_t)((uint64_t)(t514) * (uint64_t)(t515)))));
  t517 = ((int32_t)(v87));
  t518 = ((int32_t)(((int64_t)UINT64_C(32))));
  t519 = ((int32_t)(((int32_t)gg_div(t517, t518))));
  t520 = ((int32_t)((uint64_t)(t516) + (uint64_t)(t519)));
  t521 = ((float *)t513.data) + gg_index(t520, t513.len);
  t522 = ((float)((*t521)));
  t523 = fg175v_fma_rn(t509, t512, t522);
  (*t485) = ((float)(t523));
  }
  }
  t524 = gg_view((v59).data, 8);
  t525 = ((int32_t)(v62));
  t526 = ((int32_t)(((int64_t)UINT64_C(4))));
  t527 = ((int32_t)(((int32_t)((uint64_t)(t525) * (uint64_t)(t526)))));
  t528 = ((int32_t)(v87));
  t529 = ((int32_t)(((int64_t)UINT64_C(32))));
  t530 = ((int32_t)(((int32_t)gg_div(t528, t529))));
  t531 = ((int32_t)((uint64_t)(t527) + (uint64_t)(t530)));
  t532 = ((float *)t524.data) + gg_index(t531, t524.len);
  t533 = ((int32_t)(v85));
  t534 = ((int32_t)(((int64_t)UINT64_C(0))));
  t535 = ((t533) == (t534));
  if (t535) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t536 = v62;
  t537 = ((float *)(v61).data) + gg_index(t536, ((int64_t)UINT64_C(2)));
  t538 = ((float)((*t537)));
  t539 = gg_view((v59).data, 8);
  t540 = ((int32_t)(v62));
  t541 = ((int32_t)(((int64_t)UINT64_C(4))));
  t542 = ((int32_t)(((int32_t)((uint64_t)(t540) * (uint64_t)(t541)))));
  t543 = ((int32_t)(v87));
  t544 = ((int32_t)(((int64_t)UINT64_C(32))));
  t545 = ((int32_t)(((int32_t)gg_div(t543, t544))));
  t546 = ((int32_t)((uint64_t)(t542) + (uint64_t)(t545)));
  t547 = ((float *)t539.data) + gg_index(t546, t539.len);
  t548 = ((float)((*t547)));
  t549 = ((float)((v88).f1));
  /* FG_DIRECT_SLICE_INDEX */
  t550 = v62;
  t551 = ((float *)(v60).data) + gg_index(t550, ((int64_t)UINT64_C(2)));
  t552 = ((float)((*t551)));
  t553 = fg175v_mul_rn(t549, t552);
  t554 = ((float)(t553));
  t555 = fg175v_fma_rn(t538, t548, t554);
  (*t532) = ((float)(t555));
  }
  } else {
  {
  t556 = ((float)((v88).f1));
  /* FG_DIRECT_SLICE_INDEX */
  t557 = v62;
  t558 = ((float *)(v60).data) + gg_index(t557, ((int64_t)UINT64_C(2)));
  t559 = ((float)((*t558)));
  t560 = gg_view((v59).data, 8);
  t561 = ((int32_t)(v62));
  t562 = ((int32_t)(((int64_t)UINT64_C(4))));
  t563 = ((int32_t)(((int32_t)((uint64_t)(t561) * (uint64_t)(t562)))));
  t564 = ((int32_t)(v87));
  t565 = ((int32_t)(((int64_t)UINT64_C(32))));
  t566 = ((int32_t)(((int32_t)gg_div(t564, t565))));
  t567 = ((int32_t)((uint64_t)(t563) + (uint64_t)(t566)));
  t568 = ((float *)t560.data) + gg_index(t567, t560.len);
  t569 = ((float)((*t568)));
  t570 = fg175v_fma_rn(t556, t559, t569);
  (*t532) = ((float)(t570));
  }
  }
  t571 = gg_view((v58).data, 8);
  t572 = ((int32_t)(v62));
  t573 = ((int32_t)(((int64_t)UINT64_C(4))));
  t574 = ((int32_t)(((int32_t)((uint64_t)(t572) * (uint64_t)(t573)))));
  t575 = ((int32_t)(v87));
  t576 = ((int32_t)(((int64_t)UINT64_C(32))));
  t577 = ((int32_t)(((int32_t)gg_div(t575, t576))));
  t578 = ((int32_t)(((int32_t)((uint64_t)(t574) + (uint64_t)(t577)))));
  t579 = ((int32_t)(((int64_t)UINT64_C(1))));
  t580 = ((int32_t)((uint64_t)(t578) + (uint64_t)(t579)));
  t581 = ((float *)t571.data) + gg_index(t580, t571.len);
  t582 = ((int32_t)(v85));
  t583 = ((int32_t)(((int64_t)UINT64_C(0))));
  t584 = ((t582) == (t583));
  if (t584) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t585 = v62;
  t586 = ((float *)(v61).data) + gg_index(t585, ((int64_t)UINT64_C(2)));
  t587 = ((float)((*t586)));
  t588 = gg_view((v58).data, 8);
  t589 = ((int32_t)(v62));
  t590 = ((int32_t)(((int64_t)UINT64_C(4))));
  t591 = ((int32_t)(((int32_t)((uint64_t)(t589) * (uint64_t)(t590)))));
  t592 = ((int32_t)(v87));
  t593 = ((int32_t)(((int64_t)UINT64_C(32))));
  t594 = ((int32_t)(((int32_t)gg_div(t592, t593))));
  t595 = ((int32_t)(((int32_t)((uint64_t)(t591) + (uint64_t)(t594)))));
  t596 = ((int32_t)(((int64_t)UINT64_C(1))));
  t597 = ((int32_t)((uint64_t)(t595) + (uint64_t)(t596)));
  t598 = ((float *)t588.data) + gg_index(t597, t588.len);
  t599 = ((float)((*t598)));
  t600 = ((float)((v88).f2));
  /* FG_DIRECT_SLICE_INDEX */
  t601 = v62;
  t602 = ((float *)(v60).data) + gg_index(t601, ((int64_t)UINT64_C(2)));
  t603 = ((float)((*t602)));
  t604 = fg175v_mul_rn(t600, t603);
  t605 = ((float)(t604));
  t606 = fg175v_fma_rn(t587, t599, t605);
  (*t581) = ((float)(t606));
  }
  } else {
  {
  t607 = ((float)((v88).f2));
  /* FG_DIRECT_SLICE_INDEX */
  t608 = v62;
  t609 = ((float *)(v60).data) + gg_index(t608, ((int64_t)UINT64_C(2)));
  t610 = ((float)((*t609)));
  t611 = gg_view((v58).data, 8);
  t612 = ((int32_t)(v62));
  t613 = ((int32_t)(((int64_t)UINT64_C(4))));
  t614 = ((int32_t)(((int32_t)((uint64_t)(t612) * (uint64_t)(t613)))));
  t615 = ((int32_t)(v87));
  t616 = ((int32_t)(((int64_t)UINT64_C(32))));
  t617 = ((int32_t)(((int32_t)gg_div(t615, t616))));
  t618 = ((int32_t)(((int32_t)((uint64_t)(t614) + (uint64_t)(t617)))));
  t619 = ((int32_t)(((int64_t)UINT64_C(1))));
  t620 = ((int32_t)((uint64_t)(t618) + (uint64_t)(t619)));
  t621 = ((float *)t611.data) + gg_index(t620, t611.len);
  t622 = ((float)((*t621)));
  t623 = fg175v_fma_rn(t607, t610, t622);
  (*t581) = ((float)(t623));
  }
  }
  t624 = gg_view((v59).data, 8);
  t625 = ((int32_t)(v62));
  t626 = ((int32_t)(((int64_t)UINT64_C(4))));
  t627 = ((int32_t)(((int32_t)((uint64_t)(t625) * (uint64_t)(t626)))));
  t628 = ((int32_t)(v87));
  t629 = ((int32_t)(((int64_t)UINT64_C(32))));
  t630 = ((int32_t)(((int32_t)gg_div(t628, t629))));
  t631 = ((int32_t)(((int32_t)((uint64_t)(t627) + (uint64_t)(t630)))));
  t632 = ((int32_t)(((int64_t)UINT64_C(1))));
  t633 = ((int32_t)((uint64_t)(t631) + (uint64_t)(t632)));
  t634 = ((float *)t624.data) + gg_index(t633, t624.len);
  t635 = ((int32_t)(v85));
  t636 = ((int32_t)(((int64_t)UINT64_C(0))));
  t637 = ((t635) == (t636));
  if (t637) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t638 = v62;
  t639 = ((float *)(v61).data) + gg_index(t638, ((int64_t)UINT64_C(2)));
  t640 = ((float)((*t639)));
  t641 = gg_view((v59).data, 8);
  t642 = ((int32_t)(v62));
  t643 = ((int32_t)(((int64_t)UINT64_C(4))));
  t644 = ((int32_t)(((int32_t)((uint64_t)(t642) * (uint64_t)(t643)))));
  t645 = ((int32_t)(v87));
  t646 = ((int32_t)(((int64_t)UINT64_C(32))));
  t647 = ((int32_t)(((int32_t)gg_div(t645, t646))));
  t648 = ((int32_t)(((int32_t)((uint64_t)(t644) + (uint64_t)(t647)))));
  t649 = ((int32_t)(((int64_t)UINT64_C(1))));
  t650 = ((int32_t)((uint64_t)(t648) + (uint64_t)(t649)));
  t651 = ((float *)t641.data) + gg_index(t650, t641.len);
  t652 = ((float)((*t651)));
  t653 = ((float)((v88).f3));
  /* FG_DIRECT_SLICE_INDEX */
  t654 = v62;
  t655 = ((float *)(v60).data) + gg_index(t654, ((int64_t)UINT64_C(2)));
  t656 = ((float)((*t655)));
  t657 = fg175v_mul_rn(t653, t656);
  t658 = ((float)(t657));
  t659 = fg175v_fma_rn(t640, t652, t658);
  (*t634) = ((float)(t659));
  }
  } else {
  {
  t660 = ((float)((v88).f3));
  /* FG_DIRECT_SLICE_INDEX */
  t661 = v62;
  t662 = ((float *)(v60).data) + gg_index(t661, ((int64_t)UINT64_C(2)));
  t663 = ((float)((*t662)));
  t664 = gg_view((v59).data, 8);
  t665 = ((int32_t)(v62));
  t666 = ((int32_t)(((int64_t)UINT64_C(4))));
  t667 = ((int32_t)(((int32_t)((uint64_t)(t665) * (uint64_t)(t666)))));
  t668 = ((int32_t)(v87));
  t669 = ((int32_t)(((int64_t)UINT64_C(32))));
  t670 = ((int32_t)(((int32_t)gg_div(t668, t669))));
  t671 = ((int32_t)(((int32_t)((uint64_t)(t667) + (uint64_t)(t670)))));
  t672 = ((int32_t)(((int64_t)UINT64_C(1))));
  t673 = ((int32_t)((uint64_t)(t671) + (uint64_t)(t672)));
  t674 = ((float *)t664.data) + gg_index(t673, t664.len);
  t675 = ((float)((*t674)));
  t676 = fg175v_fma_rn(t660, t663, t675);
  (*t634) = ((float)(t676));
  }
  }
  t677 = v62;
  t678 = ((int32_t)(((int64_t)UINT64_C(1))));
  v62 = ((int32_t)((uint64_t)(t677) + (uint64_t)(t678)));
  }
  goto label40;
label41:;
  t679 = v87;
  t680 = ((int32_t)(((int64_t)UINT64_C(64))));
  v87 = ((int32_t)((uint64_t)(t679) + (uint64_t)(t680)));
  }
  goto label37;
label38:;
  t681 = v85;
  t682 = ((int32_t)(((int64_t)UINT64_C(1))));
  v85 = ((int32_t)((uint64_t)(t681) + (uint64_t)(t682)));
  }
  goto label31;
label32:;
  fg175v_syncwarp();
  t683 = v78;
  t684 = ((int32_t)(v42));
  t685 = ((int32_t)(((int64_t)UINT64_C(128))));
  t686 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t684) * (uint64_t)(t685)))))));
  v78 = ((int64_t)((uint64_t)(t683) + (uint64_t)(t686)));
  t687 = v75;
  t688 = ((int32_t)(v42));
  t689 = ((int32_t)(((int64_t)UINT64_C(128))));
  t690 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t688) * (uint64_t)(t689)))))));
  t691 = ((int64_t)(v25));
  t692 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t690) * (uint64_t)(t691)))))));
  v75 = ((uint64_t)((uint64_t)(t687) + (uint64_t)(t692)));
  t693 = v76;
  t694 = ((int32_t)(v42));
  t695 = ((int32_t)(((int64_t)UINT64_C(128))));
  t696 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t694) * (uint64_t)(t695)))))));
  t697 = ((int64_t)(v28));
  t698 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t696) * (uint64_t)(t697)))))));
  v76 = ((uint64_t)((uint64_t)(t693) + (uint64_t)(t698)));
  t699 = v77;
  t700 = ((int32_t)(v42));
  t701 = ((int32_t)(((int64_t)UINT64_C(128))));
  t702 = ((int32_t)(((int32_t)((uint64_t)(t700) * (uint64_t)(t701)))));
  t703 = ((int32_t)(((int64_t)UINT64_C(2))));
  t704 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t702) * (uint64_t)(t703)))))));
  v77 = ((uint64_t)((uint64_t)(t699) + (uint64_t)(t704)));
  }
  goto label16;
label17:;
  t705 = ((uint64_t)(v4));
  t706 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t707 = ((t705) != (t706));
  t708 = t707;
  if (t708) {
  t709 = ((int32_t)(v39));
  t710 = ((int32_t)(((int64_t)UINT64_C(0))));
  t708 = ((t709) == (t710));
  }
  t711 = t708;
  if (t711) {
  {
  t712 = ((uint64_t)(v4));
  t713 = ((int32_t)(v48));
  t714 = ((int32_t)(((int64_t)UINT64_C(4))));
  t715 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t713) * (uint64_t)(t714)))))));
  t716 = fg175v_ld_f32(t712, t715);
  v89 = ((float)(t716));
  v90 = ((int32_t)(v44));
  t717 = ((int32_t)(((int64_t)UINT64_C(4))));
  t718 = ((int32_t)(v37));
  t719 = ((t717) > (t718));
  t720 = t719;
  if (t720) {
  t721 = ((int32_t)(v90));
  t722 = ((int32_t)(v37));
  t720 = ((t721) >= (t722));
  }
  t723 = t720;
  t724 = (!(t723));
  if (t724) {
  {
  t725 = ((float)(v89));
  /* FG_DIRECT_SLICE_INDEX */
  t726 = v90;
  t727 = ((float *)(v56).data) + gg_index(t726, ((int64_t)UINT64_C(2)));
  t728 = ((float)((*t727)));
  t729 = fg175v_fmaxf(t725, t728);
  v91 = ((float)(t729));
  /* FG_DIRECT_SLICE_INDEX */
  t730 = v90;
  t731 = ((float *)(v56).data) + gg_index(t730, ((int64_t)UINT64_C(2)));
  t732 = ((float)((*t731)));
  t733 = ((float)(v91));
  t734 = ((float)(((t732) - (t733))));
  t735 = fg175v_expf(t734);
  v92 = ((float)(t735));
  /* FG_DIRECT_SLICE_INDEX */
  t736 = v90;
  t737 = ((float *)(v56).data) + gg_index(t736, ((int64_t)UINT64_C(2)));
  (*t737) = ((float)(v91));
  /* FG_DIRECT_SLICE_INDEX */
  t738 = v90;
  t739 = ((float *)(v57).data) + gg_index(t738, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t740 = v90;
  t741 = ((float *)(v57).data) + gg_index(t740, ((int64_t)UINT64_C(2)));
  t742 = ((float)((*t741)));
  t743 = ((float)(v92));
  t744 = ((float)(((t742) * (t743))));
  t746 = ((int32_t)(v43));
  t747 = ((int32_t)(((int64_t)UINT64_C(0))));
  t748 = ((t746) == (t747));
  if (t748) {
  {
  t749 = ((float)(v89));
  /* FG_DIRECT_SLICE_INDEX */
  t750 = v90;
  t751 = ((float *)(v56).data) + gg_index(t750, ((int64_t)UINT64_C(2)));
  t752 = ((float)((*t751)));
  t753 = ((float)(((t749) - (t752))));
  t754 = fg175v_expf(t753);
  t745 = ((float)(t754));
  }
  } else {
  {
  t745 = ((float)(gg_double_bits(UINT64_C(0))));
  }
  }
  t755 = ((float)(t745));
  (*t739) = ((float)(((t744) + (t755))));
  v93 = ((int32_t)(((int64_t)UINT64_C(0))));
label43:;
  t756 = ((int32_t)(v93));
  t757 = ((int32_t)(((int64_t)UINT64_C(4))));
  if (!(((t756) < (t757)))) goto label44;
  {
  t758 = gg_view((v58).data, 8);
  t759 = ((int32_t)(v90));
  t760 = ((int32_t)(((int64_t)UINT64_C(4))));
  t761 = ((int32_t)(((int32_t)((uint64_t)(t759) * (uint64_t)(t760)))));
  t762 = ((int32_t)(v93));
  t763 = ((int32_t)((uint64_t)(t761) + (uint64_t)(t762)));
  t764 = ((float *)t758.data) + gg_index(t763, t758.len);
  t765 = gg_view((v58).data, 8);
  t766 = ((int32_t)(v90));
  t767 = ((int32_t)(((int64_t)UINT64_C(4))));
  t768 = ((int32_t)(((int32_t)((uint64_t)(t766) * (uint64_t)(t767)))));
  t769 = ((int32_t)(v93));
  t770 = ((int32_t)((uint64_t)(t768) + (uint64_t)(t769)));
  t771 = ((float *)t765.data) + gg_index(t770, t765.len);
  t772 = ((float)((*t771)));
  t773 = ((float)(v92));
  t774 = fg175v_mul_rn(t772, t773);
  (*t764) = ((float)(t774));
  t775 = gg_view((v59).data, 8);
  t776 = ((int32_t)(v90));
  t777 = ((int32_t)(((int64_t)UINT64_C(4))));
  t778 = ((int32_t)(((int32_t)((uint64_t)(t776) * (uint64_t)(t777)))));
  t779 = ((int32_t)(v93));
  t780 = ((int32_t)((uint64_t)(t778) + (uint64_t)(t779)));
  t781 = ((float *)t775.data) + gg_index(t780, t775.len);
  t782 = gg_view((v59).data, 8);
  t783 = ((int32_t)(v90));
  t784 = ((int32_t)(((int64_t)UINT64_C(4))));
  t785 = ((int32_t)(((int32_t)((uint64_t)(t783) * (uint64_t)(t784)))));
  t786 = ((int32_t)(v93));
  t787 = ((int32_t)((uint64_t)(t785) + (uint64_t)(t786)));
  t788 = ((float *)t782.data) + gg_index(t787, t782.len);
  t789 = ((float)((*t788)));
  t790 = ((float)(v92));
  t791 = fg175v_mul_rn(t789, t790);
  (*t781) = ((float)(t791));
  t792 = v93;
  t793 = ((int32_t)(((int64_t)UINT64_C(1))));
  v93 = ((int32_t)((uint64_t)(t792) + (uint64_t)(t793)));
  }
  goto label43;
label44:;
  }
  }
  }
  }
  v62 = ((int32_t)(((int64_t)UINT64_C(0))));
label46:;
  t794 = ((int32_t)(v62));
  t795 = ((int32_t)(v37));
  if (!(((t794) < (t795)))) goto label47;
  {
  t796 = ((int32_t)(v44));
  t797 = ((int32_t)(((int64_t)UINT64_C(0))));
  t798 = ((t796) == (t797));
  if (t798) {
  {
  t799 = ((uint64_t)(v45));
  t800 = ((int32_t)(((int64_t)UINT64_C(1024))));
  t801 = ((int32_t)(v62));
  t802 = ((int32_t)(((int64_t)UINT64_C(32))));
  t803 = ((int32_t)(((int32_t)((uint64_t)(t801) * (uint64_t)(t802)))));
  t804 = ((int32_t)(((int32_t)((uint64_t)(t800) + (uint64_t)(t803)))));
  t805 = ((int32_t)(v43));
  t806 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t804) + (uint64_t)(t805)))))));
  t807 = gg_function_5(base + 0);
  fg175v_sm_st_f32(t799, t806, t807);
  t808 = ((uint64_t)(v45));
  t809 = ((int32_t)(((int64_t)UINT64_C(1088))));
  t810 = ((int32_t)(v62));
  t811 = ((int32_t)(((int64_t)UINT64_C(32))));
  t812 = ((int32_t)(((int32_t)((uint64_t)(t810) * (uint64_t)(t811)))));
  t813 = ((int32_t)(((int32_t)((uint64_t)(t809) + (uint64_t)(t812)))));
  t814 = ((int32_t)(v43));
  t815 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t813) + (uint64_t)(t814)))))));
  t816 = ((float)(gg_double_bits(UINT64_C(0))));
  fg175v_sm_st_f32(t808, t815, t816);
  }
  }
  t817 = v62;
  t818 = ((int32_t)(((int64_t)UINT64_C(1))));
  v62 = ((int32_t)((uint64_t)(t817) + (uint64_t)(t818)));
  }
  goto label46;
label47:;
  fg175v_barrier();
  v62 = ((int32_t)(((int64_t)UINT64_C(0))));
label49:;
  t819 = ((int32_t)(v62));
  t820 = ((int32_t)(v37));
  if (!(((t819) < (t820)))) goto label50;
  {
  t821 = ((int32_t)(v43));
  t822 = ((int32_t)(((int64_t)UINT64_C(0))));
  t823 = ((t821) == (t822));
  if (t823) {
  {
  t824 = ((uint64_t)(v45));
  t825 = ((int32_t)(((int64_t)UINT64_C(1024))));
  t826 = ((int32_t)(v62));
  t827 = ((int32_t)(((int64_t)UINT64_C(32))));
  t828 = ((int32_t)(((int32_t)((uint64_t)(t826) * (uint64_t)(t827)))));
  t829 = ((int32_t)(((int32_t)((uint64_t)(t825) + (uint64_t)(t828)))));
  t830 = ((int32_t)(v44));
  t831 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t829) + (uint64_t)(t830)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t832 = v62;
  t833 = ((float *)(v56).data) + gg_index(t832, ((int64_t)UINT64_C(2)));
  t834 = ((float)((*t833)));
  fg175v_sm_st_f32(t824, t831, t834);
  }
  }
  t835 = v62;
  t836 = ((int32_t)(((int64_t)UINT64_C(1))));
  v62 = ((int32_t)((uint64_t)(t835) + (uint64_t)(t836)));
  }
  goto label49;
label50:;
  fg175v_barrier();
  v94 = ((int32_t)(v37));
  t837 = ((int32_t)(v37));
  t838 = ((int32_t)(((int64_t)UINT64_C(1))));
  t839 = ((t837) > (t838));
  t840 = t839;
  if (t840) {
  t841 = ((int32_t)(v46));
  t842 = ((int32_t)(v37));
  t843 = ((int32_t)(((int32_t)((uint64_t)(t841) + (uint64_t)(t842)))));
  t844 = ((int32_t)(v15));
  t840 = ((t843) > (t844));
  }
  t845 = t840;
  if (t845) {
  {
  t846 = ((int32_t)(v15));
  t847 = ((int32_t)(v46));
  v94 = ((int32_t)(((int32_t)((uint64_t)(t846) - (uint64_t)(t847)))));
  }
  }
  v62 = ((int32_t)(((int64_t)UINT64_C(0))));
label52:;
  t848 = ((int32_t)(v62));
  t849 = ((int32_t)(v94));
  if (!(((t848) < (t849)))) goto label53;
  {
  t850 = ((uint64_t)(v45));
  t851 = ((int32_t)(((int64_t)UINT64_C(1024))));
  t852 = ((int32_t)(v62));
  t853 = ((int32_t)(((int64_t)UINT64_C(32))));
  t854 = ((int32_t)(((int32_t)((uint64_t)(t852) * (uint64_t)(t853)))));
  t855 = ((int32_t)(((int32_t)((uint64_t)(t851) + (uint64_t)(t854)))));
  t856 = ((int32_t)(v43));
  t857 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t855) + (uint64_t)(t856)))))));
  t858 = fg175v_sm_f32(t850, t857);
  v95 = ((float)(t858));
  t859 = ((float)(v95));
  v95 = gg_function_30(t859, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t860 = v62;
  t861 = ((float *)(v56).data) + gg_index(t860, ((int64_t)UINT64_C(2)));
  t862 = ((float)((*t861)));
  t863 = ((float)(v95));
  t864 = ((float)(((t862) - (t863))));
  t865 = fg175v_expf(t864);
  v96 = ((float)(t865));
  /* FG_DIRECT_SLICE_INDEX */
  t866 = v62;
  t867 = ((float *)(v56).data) + gg_index(t866, ((int64_t)UINT64_C(2)));
  (*t867) = ((float)(v95));
  v97 = ((int32_t)(((int64_t)UINT64_C(0))));
label55:;
  t868 = ((int32_t)(v97));
  t869 = ((int32_t)(((int64_t)UINT64_C(4))));
  if (!(((t868) < (t869)))) goto label56;
  {
  t870 = gg_view((v58).data, 8);
  t871 = ((int32_t)(v62));
  t872 = ((int32_t)(((int64_t)UINT64_C(4))));
  t873 = ((int32_t)(((int32_t)((uint64_t)(t871) * (uint64_t)(t872)))));
  t874 = ((int32_t)(v97));
  t875 = ((int32_t)((uint64_t)(t873) + (uint64_t)(t874)));
  t876 = ((float *)t870.data) + gg_index(t875, t870.len);
  t877 = gg_view((v58).data, 8);
  t878 = ((int32_t)(v62));
  t879 = ((int32_t)(((int64_t)UINT64_C(4))));
  t880 = ((int32_t)(((int32_t)((uint64_t)(t878) * (uint64_t)(t879)))));
  t881 = ((int32_t)(v97));
  t882 = ((int32_t)((uint64_t)(t880) + (uint64_t)(t881)));
  t883 = ((float *)t877.data) + gg_index(t882, t877.len);
  t884 = ((float)((*t883)));
  t885 = ((float)(v96));
  t886 = fg175v_mul_rn(t884, t885);
  (*t876) = ((float)(t886));
  t887 = gg_view((v59).data, 8);
  t888 = ((int32_t)(v62));
  t889 = ((int32_t)(((int64_t)UINT64_C(4))));
  t890 = ((int32_t)(((int32_t)((uint64_t)(t888) * (uint64_t)(t889)))));
  t891 = ((int32_t)(v97));
  t892 = ((int32_t)((uint64_t)(t890) + (uint64_t)(t891)));
  t893 = ((float *)t887.data) + gg_index(t892, t887.len);
  t894 = gg_view((v59).data, 8);
  t895 = ((int32_t)(v62));
  t896 = ((int32_t)(((int64_t)UINT64_C(4))));
  t897 = ((int32_t)(((int32_t)((uint64_t)(t895) * (uint64_t)(t896)))));
  t898 = ((int32_t)(v97));
  t899 = ((int32_t)((uint64_t)(t897) + (uint64_t)(t898)));
  t900 = ((float *)t894.data) + gg_index(t899, t894.len);
  t901 = ((float)((*t900)));
  t902 = ((float)(v96));
  t903 = fg175v_mul_rn(t901, t902);
  (*t893) = ((float)(t903));
  t904 = v97;
  t905 = ((int32_t)(((int64_t)UINT64_C(1))));
  v97 = ((int32_t)((uint64_t)(t904) + (uint64_t)(t905)));
  }
  goto label55;
label56:;
  v98 = ((int32_t)(((int64_t)UINT64_C(0))));
label58:;
  t906 = ((int32_t)(v98));
  t907 = ((int32_t)(((int64_t)UINT64_C(128))));
  if (!(((t906) < (t907)))) goto label59;
  {
  t908 = ((int32_t)(v98));
  t909 = ((int32_t)(v43));
  t910 = ((int32_t)(((int64_t)UINT64_C(2))));
  t911 = ((int32_t)(((int32_t)((uint64_t)(t909) * (uint64_t)(t910)))));
  v99 = ((int32_t)(((int32_t)((uint64_t)(t908) + (uint64_t)(t911)))));
  t912 = ((uint64_t)(v45));
  t913 = ((int32_t)(v44));
  t914 = ((int32_t)(((int64_t)UINT64_C(256))));
  t915 = ((int32_t)(((int32_t)((uint64_t)(t913) * (uint64_t)(t914)))));
  t916 = ((int32_t)(((int64_t)UINT64_C(2))));
  t917 = ((int32_t)(v99));
  t918 = ((int32_t)(((int32_t)((uint64_t)(t916) * (uint64_t)(t917)))));
  t919 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t915) + (uint64_t)(t918)))))));
  t920 = gg_view((v58).data, 8);
  t921 = ((int32_t)(v62));
  t922 = ((int32_t)(((int64_t)UINT64_C(4))));
  t923 = ((int32_t)(((int32_t)((uint64_t)(t921) * (uint64_t)(t922)))));
  t924 = ((int32_t)(v98));
  t925 = ((int32_t)(((int64_t)UINT64_C(32))));
  t926 = ((int32_t)(((int32_t)gg_div(t924, t925))));
  t927 = ((int32_t)((uint64_t)(t923) + (uint64_t)(t926)));
  t928 = ((float *)t920.data) + gg_index(t927, t920.len);
  t929 = ((float)((*t928)));
  fg175v_sm_st_f32(t912, t919, t929);
  t930 = ((uint64_t)(v45));
  t931 = ((int32_t)(v44));
  t932 = ((int32_t)(((int64_t)UINT64_C(256))));
  t933 = ((int32_t)(((int32_t)((uint64_t)(t931) * (uint64_t)(t932)))));
  t934 = ((int32_t)(((int64_t)UINT64_C(2))));
  t935 = ((int32_t)(v99));
  t936 = ((int32_t)(((int32_t)((uint64_t)(t934) * (uint64_t)(t935)))));
  t937 = ((int32_t)(((int32_t)((uint64_t)(t933) + (uint64_t)(t936)))));
  t938 = ((int32_t)(((int64_t)UINT64_C(1))));
  t939 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t937) + (uint64_t)(t938)))))));
  t940 = gg_view((v59).data, 8);
  t941 = ((int32_t)(v62));
  t942 = ((int32_t)(((int64_t)UINT64_C(4))));
  t943 = ((int32_t)(((int32_t)((uint64_t)(t941) * (uint64_t)(t942)))));
  t944 = ((int32_t)(v98));
  t945 = ((int32_t)(((int64_t)UINT64_C(32))));
  t946 = ((int32_t)(((int32_t)gg_div(t944, t945))));
  t947 = ((int32_t)((uint64_t)(t943) + (uint64_t)(t946)));
  t948 = ((float *)t940.data) + gg_index(t947, t940.len);
  t949 = ((float)((*t948)));
  fg175v_sm_st_f32(t930, t939, t949);
  t950 = ((uint64_t)(v45));
  t951 = ((int32_t)(v44));
  t952 = ((int32_t)(((int64_t)UINT64_C(256))));
  t953 = ((int32_t)(((int32_t)((uint64_t)(t951) * (uint64_t)(t952)))));
  t954 = ((int32_t)(((int64_t)UINT64_C(2))));
  t955 = ((int32_t)(v99));
  t956 = ((int32_t)(((int32_t)((uint64_t)(t954) * (uint64_t)(t955)))));
  t957 = ((int32_t)(((int32_t)((uint64_t)(t953) + (uint64_t)(t956)))));
  t958 = ((int32_t)(((int64_t)UINT64_C(2))));
  t959 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t957) + (uint64_t)(t958)))))));
  t960 = gg_view((v58).data, 8);
  t961 = ((int32_t)(v62));
  t962 = ((int32_t)(((int64_t)UINT64_C(4))));
  t963 = ((int32_t)(((int32_t)((uint64_t)(t961) * (uint64_t)(t962)))));
  t964 = ((int32_t)(v98));
  t965 = ((int32_t)(((int64_t)UINT64_C(32))));
  t966 = ((int32_t)(((int32_t)gg_div(t964, t965))));
  t967 = ((int32_t)(((int32_t)((uint64_t)(t963) + (uint64_t)(t966)))));
  t968 = ((int32_t)(((int64_t)UINT64_C(1))));
  t969 = ((int32_t)((uint64_t)(t967) + (uint64_t)(t968)));
  t970 = ((float *)t960.data) + gg_index(t969, t960.len);
  t971 = ((float)((*t970)));
  fg175v_sm_st_f32(t950, t959, t971);
  t972 = ((uint64_t)(v45));
  t973 = ((int32_t)(v44));
  t974 = ((int32_t)(((int64_t)UINT64_C(256))));
  t975 = ((int32_t)(((int32_t)((uint64_t)(t973) * (uint64_t)(t974)))));
  t976 = ((int32_t)(((int64_t)UINT64_C(2))));
  t977 = ((int32_t)(v99));
  t978 = ((int32_t)(((int32_t)((uint64_t)(t976) * (uint64_t)(t977)))));
  t979 = ((int32_t)(((int32_t)((uint64_t)(t975) + (uint64_t)(t978)))));
  t980 = ((int32_t)(((int64_t)UINT64_C(3))));
  t981 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t979) + (uint64_t)(t980)))))));
  t982 = gg_view((v59).data, 8);
  t983 = ((int32_t)(v62));
  t984 = ((int32_t)(((int64_t)UINT64_C(4))));
  t985 = ((int32_t)(((int32_t)((uint64_t)(t983) * (uint64_t)(t984)))));
  t986 = ((int32_t)(v98));
  t987 = ((int32_t)(((int64_t)UINT64_C(32))));
  t988 = ((int32_t)(((int32_t)gg_div(t986, t987))));
  t989 = ((int32_t)(((int32_t)((uint64_t)(t985) + (uint64_t)(t988)))));
  t990 = ((int32_t)(((int64_t)UINT64_C(1))));
  t991 = ((int32_t)((uint64_t)(t989) + (uint64_t)(t990)));
  t992 = ((float *)t982.data) + gg_index(t991, t982.len);
  t993 = ((float)((*t992)));
  fg175v_sm_st_f32(t972, t981, t993);
  t994 = v98;
  t995 = ((int32_t)(((int64_t)UINT64_C(64))));
  v98 = ((int32_t)((uint64_t)(t994) + (uint64_t)(t995)));
  }
  goto label58;
label59:;
  /* FG_DIRECT_SLICE_INDEX */
  t996 = v62;
  t997 = ((float *)(v57).data) + gg_index(t996, ((int64_t)UINT64_C(2)));
  t998 = (*t997);
  t999 = ((float)(v96));
  (*t997) = ((t998) * (t999));
  /* FG_DIRECT_SLICE_INDEX */
  t1000 = v62;
  t1001 = ((float *)(v57).data) + gg_index(t1000, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t1002 = v62;
  t1003 = ((float *)(v57).data) + gg_index(t1002, ((int64_t)UINT64_C(2)));
  t1004 = ((float)((*t1003)));
  (*t1001) = gg_function_24(t1004, base + 0);
  t1005 = ((int32_t)(v43));
  t1006 = ((int32_t)(((int64_t)UINT64_C(0))));
  t1007 = ((t1005) == (t1006));
  if (t1007) {
  {
  t1008 = ((uint64_t)(v45));
  t1009 = ((int32_t)(((int64_t)UINT64_C(1088))));
  t1010 = ((int32_t)(v62));
  t1011 = ((int32_t)(((int64_t)UINT64_C(32))));
  t1012 = ((int32_t)(((int32_t)((uint64_t)(t1010) * (uint64_t)(t1011)))));
  t1013 = ((int32_t)(((int32_t)((uint64_t)(t1009) + (uint64_t)(t1012)))));
  t1014 = ((int32_t)(v44));
  t1015 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t1013) + (uint64_t)(t1014)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t1016 = v62;
  t1017 = ((float *)(v57).data) + gg_index(t1016, ((int64_t)UINT64_C(2)));
  t1018 = ((float)((*t1017)));
  fg175v_sm_st_f32(t1008, t1015, t1018);
  }
  }
  fg175v_barrier();
  /* FG_DIRECT_SLICE_INDEX */
  t1019 = v62;
  t1020 = ((float *)(v57).data) + gg_index(t1019, ((int64_t)UINT64_C(2)));
  t1021 = ((uint64_t)(v45));
  t1022 = ((int32_t)(((int64_t)UINT64_C(1088))));
  t1023 = ((int32_t)(v62));
  t1024 = ((int32_t)(((int64_t)UINT64_C(32))));
  t1025 = ((int32_t)(((int32_t)((uint64_t)(t1023) * (uint64_t)(t1024)))));
  t1026 = ((int32_t)(((int32_t)((uint64_t)(t1022) + (uint64_t)(t1025)))));
  t1027 = ((int32_t)(v43));
  t1028 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t1026) + (uint64_t)(t1027)))))));
  t1029 = fg175v_sm_f32(t1021, t1028);
  (*t1020) = ((float)(t1029));
  /* FG_DIRECT_SLICE_INDEX */
  t1030 = v62;
  t1031 = ((float *)(v57).data) + gg_index(t1030, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t1032 = v62;
  t1033 = ((float *)(v57).data) + gg_index(t1032, ((int64_t)UINT64_C(2)));
  t1034 = ((float)((*t1033)));
  (*t1031) = gg_function_24(t1034, base + 0);
  v100 = ((int32_t)(((int64_t)UINT64_C(0))));
label61:;
  t1035 = ((int32_t)(v100));
  t1036 = ((int32_t)(((int64_t)UINT64_C(256))));
  if (!(((t1035) < (t1036)))) goto label62;
  {
  v101 = ((float)(gg_double_bits(UINT64_C(0))));
  v102 = ((int32_t)(((int64_t)UINT64_C(0))));
label64:;
  t1037 = ((int32_t)(v102));
  t1038 = ((int32_t)(((int64_t)UINT64_C(4))));
  if (!(((t1037) < (t1038)))) goto label65;
  {
  t1039 = v101;
  t1040 = ((uint64_t)(v45));
  t1041 = ((int32_t)(v102));
  t1042 = ((int32_t)(((int64_t)UINT64_C(256))));
  t1043 = ((int32_t)(((int32_t)((uint64_t)(t1041) * (uint64_t)(t1042)))));
  t1044 = ((int32_t)(v100));
  t1045 = ((int32_t)(((int32_t)((uint64_t)(t1043) + (uint64_t)(t1044)))));
  t1046 = ((int32_t)(v55));
  t1047 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t1045) + (uint64_t)(t1046)))))));
  t1048 = fg175v_sm_f32(t1040, t1047);
  t1049 = ((float)(t1048));
  v101 = ((t1039) + (t1049));
  t1050 = v102;
  t1051 = ((int32_t)(((int64_t)UINT64_C(1))));
  v102 = ((int32_t)((uint64_t)(t1050) + (uint64_t)(t1051)));
  }
  goto label64;
label65:;
  t1052 = ((int32_t)(v42));
  t1053 = ((int32_t)(((int64_t)UINT64_C(1))));
  t1054 = ((t1052) == (t1053));
  if (t1054) {
  {
  t1055 = v101;
  /* FG_DIRECT_SLICE_INDEX */
  t1056 = v62;
  t1057 = ((float *)(v57).data) + gg_index(t1056, ((int64_t)UINT64_C(2)));
  t1058 = ((float)((*t1057)));
  v101 = ((t1055) / (t1058));
  }
  }
  t1059 = ((int64_t)(((int64_t)(v47))));
  t1060 = ((int64_t)(((int64_t)(v15))));
  t1061 = ((int64_t)(((int64_t)((uint64_t)(t1059) * (uint64_t)(t1060)))));
  t1062 = ((int32_t)(v46));
  t1063 = ((int32_t)(v62));
  t1064 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t1062) + (uint64_t)(t1063)))))));
  t1065 = ((int64_t)(((int64_t)((uint64_t)(t1061) + (uint64_t)(t1064)))));
  t1066 = ((int64_t)(((int64_t)(v16))));
  t1067 = ((int64_t)(((int64_t)((uint64_t)(t1065) * (uint64_t)(t1066)))));
  t1068 = ((int64_t)(((int64_t)(v48))));
  t1069 = ((int64_t)(((int64_t)((uint64_t)(t1067) + (uint64_t)(t1068)))));
  t1070 = ((int64_t)(((int64_t)(v42))));
  t1071 = ((int64_t)(((int64_t)((uint64_t)(t1069) * (uint64_t)(t1070)))));
  t1072 = ((int64_t)(((int64_t)(v39))));
  v103 = ((int64_t)(((int64_t)((uint64_t)(t1071) + (uint64_t)(t1072)))));
  t1073 = ((uint64_t)(v6));
  t1074 = ((int64_t)(v103));
  t1075 = ((int64_t)(((int64_t)UINT64_C(256))));
  t1076 = ((int64_t)(((int64_t)((uint64_t)(t1074) * (uint64_t)(t1075)))));
  t1077 = ((int32_t)(v100));
  t1078 = ((int32_t)(v55));
  t1079 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t1077) + (uint64_t)(t1078)))))));
  t1080 = ((int64_t)(((int64_t)((uint64_t)(t1076) + (uint64_t)(t1079)))));
  t1081 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1082 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t1080) * (uint64_t)(t1081)))))));
  t1083 = ((float)(v101));
  fg175v_st_f32(t1073, t1082, t1083);
  t1084 = v100;
  t1085 = ((int32_t)(((int64_t)UINT64_C(128))));
  v100 = ((int32_t)((uint64_t)(t1084) + (uint64_t)(t1085)));
  }
  goto label61;
label62:;
  t1086 = ((int32_t)(v62));
  t1087 = ((int32_t)(v94));
  t1088 = ((int32_t)(((int64_t)UINT64_C(1))));
  t1089 = ((int32_t)(((int32_t)((uint64_t)(t1087) - (uint64_t)(t1088)))));
  t1090 = ((t1086) < (t1089));
  if (t1090) {
  {
  fg175v_barrier();
  }
  }
  t1091 = v62;
  t1092 = ((int32_t)(((int64_t)UINT64_C(1))));
  v62 = ((int32_t)((uint64_t)(t1091) + (uint64_t)(t1092)));
  }
  goto label52;
label53:;
  t1093 = ((int32_t)(v42));
  t1094 = ((int32_t)(((int64_t)UINT64_C(1))));
  t1095 = ((t1093) != (t1094));
  t1096 = t1095;
  if (t1096) {
  t1097 = ((int32_t)(v55));
  t1098 = ((int32_t)(v37));
  t1096 = ((t1097) < (t1098));
  }
  t1099 = t1096;
  t1100 = t1099;
  if (t1100) {
  t1101 = ((int32_t)(v37));
  t1102 = ((int32_t)(((int64_t)UINT64_C(1))));
  t1103 = ((t1101) == (t1102));
  t1104 = t1103;
  if (!t1104) {
  t1105 = ((int32_t)(v46));
  t1106 = ((int32_t)(v55));
  t1107 = ((int32_t)(((int32_t)((uint64_t)(t1105) + (uint64_t)(t1106)))));
  t1108 = ((int32_t)(v15));
  t1104 = ((t1107) < (t1108));
  }
  t1100 = t1104;
  }
  t1109 = t1100;
  if (t1109) {
  {
  t1110 = ((int64_t)(((int64_t)(v47))));
  t1111 = ((int64_t)(((int64_t)(v15))));
  t1112 = ((int64_t)(((int64_t)((uint64_t)(t1110) * (uint64_t)(t1111)))));
  t1113 = ((int32_t)(v46));
  t1114 = ((int32_t)(v55));
  t1115 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t1113) + (uint64_t)(t1114)))))));
  t1116 = ((int64_t)(((int64_t)((uint64_t)(t1112) + (uint64_t)(t1115)))));
  t1117 = ((int64_t)(((int64_t)(v16))));
  t1118 = ((int64_t)(((int64_t)((uint64_t)(t1116) * (uint64_t)(t1117)))));
  t1119 = ((int64_t)(((int64_t)(v48))));
  t1120 = ((int64_t)(((int64_t)((uint64_t)(t1118) + (uint64_t)(t1119)))));
  t1121 = ((int64_t)(((int64_t)(v42))));
  t1122 = ((int64_t)(((int64_t)((uint64_t)(t1120) * (uint64_t)(t1121)))));
  t1123 = ((int64_t)(((int64_t)(v39))));
  v104 = ((int64_t)(((int64_t)((uint64_t)(t1122) + (uint64_t)(t1123)))));
  t1124 = ((uint64_t)(v7));
  t1125 = ((int64_t)(v104));
  t1126 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1127 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t1125) * (uint64_t)(t1126)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t1128 = v55;
  t1129 = ((float *)(v56).data) + gg_index(t1128, ((int64_t)UINT64_C(2)));
  t1130 = ((float)((*t1129)));
  fg175v_st_f32(t1124, t1127, t1130);
  t1131 = ((uint64_t)(v7));
  t1132 = ((int64_t)(v104));
  t1133 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1134 = ((int64_t)(((int64_t)((uint64_t)(t1132) * (uint64_t)(t1133)))));
  t1135 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1136 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t1134) + (uint64_t)(t1135)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t1137 = v55;
  t1138 = ((float *)(v57).data) + gg_index(t1137, ((int64_t)UINT64_C(2)));
  t1139 = ((float)((*t1138)));
  fg175v_st_f32(t1131, t1136, t1139);
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
  gg_function_32(t46, t47, t48, t49, t50, t51, t52, t53, t54, t55, t56, base + 0);
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
