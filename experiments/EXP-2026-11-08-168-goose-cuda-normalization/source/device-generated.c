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


typedef struct gg_type_8 gg_type_8;
#pragma pack(push, 1)
struct gg_type_8 {
  float f0;
  float f1;
};
#pragma pack(pop)
typedef struct gg_type_10 gg_type_10;
#pragma pack(push, 1)
struct gg_type_10 {
  float f2;
  float f3;
  float f4;
  float f5;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_10 data[4]; } gg_type_11;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(int64_t a, int64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, int64_t value);
static uint8_t gg_eq_2(int32_t a, int32_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, int32_t value);
static uint8_t gg_eq_3(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint8_t value);
static uint8_t gg_eq_4(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint64_t value);
static uint8_t gg_eq_5(float a, float b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, float value);
static uint8_t gg_eq_6(double a, double b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, double value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, uint32_t value);
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, gg_type_10 value);
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, gg_type_11 value);
static uint8_t gg_eq_12(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(float a, float b) {
return a == b;
}
static void gg_render_5(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(double a, double b) {
return a == b;
}
static void gg_render_6(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
if(!gg_eq_5(a.f0,b.f0))return 0;
if(!gg_eq_5(a.f1,b.f1))return 0;
return 1;
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
gg_append(out,"\106\062\040\173\040",5,1);
gg_render_5(out, value.f0);
gg_append(out,", ",2,1);
gg_render_5(out, value.f1);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_9(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
if(!gg_eq_5(a.f2,b.f2))return 0;
if(!gg_eq_5(a.f3,b.f3))return 0;
if(!gg_eq_5(a.f4,b.f4))return 0;
if(!gg_eq_5(a.f5,b.f5))return 0;
return 1;
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
gg_append(out,"\106\064\040\173\040",5,1);
gg_render_5(out, value.f2);
gg_append(out,", ",2,1);
gg_render_5(out, value.f3);
gg_append(out,", ",2,1);
gg_render_5(out, value.f4);
gg_append(out,", ",2,1);
gg_render_5(out, value.f5);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_type_11*)a.data)[i],((gg_type_11*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_type_11 a, gg_type_11 b) {
return gg_equal_elements_10(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_11(gg_reference out, gg_type_11 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_10(out, ((gg_type_10*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_12(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(int64_t p0, int64_t p1, int32_t p2, int32_t p3, uint8_t p4, uint8_t p5, uint8_t p6, uint8_t p7, int32_t p8, uint64_t p9, uint64_t p10, uint64_t p11, uint64_t p12, uint64_t p13, uint64_t p14, int64_t base);
static float gg_function_6(float p0, uint64_t p1, int32_t p2, int32_t p3, int64_t base);
static float gg_function_7(float p0, int64_t base);
static uint64_t gg_function_12(int64_t p0, int64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t base);
static gg_type_8 gg_function_13(gg_type_8 p0, uint64_t p1, int32_t p2, int32_t p3, int64_t base);
static uint64_t gg_function_15(int64_t p0, int64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t base);
static uint32_t gg_function_16(int64_t p0, uint32_t p1, int64_t p2, int64_t base);
static void gg_function_19(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, uint8_t p9, uint8_t p10, uint8_t p11, uint8_t p12, int32_t p13, int32_t p14, float p15, float p16, int64_t base);
static float gg_function_7(float p0, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  float t3;
  float t4;
  int32_t t5;
  float t6;
  float t7;
  int32_t t8;
  int32_t t9;
  float v0;
  float v1;
  int32_t v2;
  v0 = p0;
  {
  v1 = ((float)(v0));
  v2 = ((int32_t)(((int64_t)UINT64_C(16))));
label1:;
  t1 = ((int32_t)(v2));
  t2 = ((int32_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) goto label2;
  {
  t3 = v1;
  t4 = ((float)(v1));
  t5 = ((int32_t)(v2));
  t6 = fg168_shuffle(t4, t5);
  t7 = ((float)(t6));
  v1 = ((t3) + (t7));
  t8 = v2;
  t9 = ((int32_t)(((int64_t)UINT64_C(1))));
  v2 = ((t8) >> ((uint64_t)(t9) & 31));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_6(float p0, uint64_t p1, int32_t p2, int32_t p3, int64_t base) {
  float t0;
  float t1;
  int32_t t2;
  int32_t t3;
  uint8_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  uint8_t t9;
  uint64_t t10;
  int32_t t11;
  int32_t t12;
  uint64_t t13;
  float t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  uint8_t t19;
  uint64_t t20;
  uint64_t t21;
  float t22;
  float t23;
  float v0;
  uint64_t v1;
  int32_t v2;
  int32_t v3;
  float v4;
  int32_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((float)(v0));
  v4 = gg_function_7(t1, base + 0);
  t2 = ((int32_t)(v3));
  t3 = ((int32_t)(((int64_t)UINT64_C(32))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int32_t)(v2));
  t6 = ((int32_t)(((int64_t)UINT64_C(32))));
  v5 = ((int32_t)(((int32_t)gg_mod(t5, t6))));
  t7 = ((int32_t)(v5));
  t8 = ((int32_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v1));
  t11 = ((int32_t)(v2));
  t12 = ((int32_t)(((int64_t)UINT64_C(32))));
  t13 = ((uint64_t)(((uint64_t)(((int32_t)gg_div(t11, t12))))));
  t14 = ((float)(v4));
  fg168_store(t10, t13, t14);
  }
  }
  fg168_barrier();
  v4 = ((float)(gg_double_bits(UINT64_C(0))));
  t15 = ((int32_t)(v5));
  t16 = ((int32_t)(v3));
  t17 = ((int32_t)(((int64_t)UINT64_C(32))));
  t18 = ((int32_t)(((int32_t)gg_div(t16, t17))));
  t19 = ((t15) < (t18));
  if (t19) {
  {
  t20 = ((uint64_t)(v1));
  t21 = ((uint64_t)(((uint64_t)(v5))));
  t22 = fg168_load(t20, t21);
  v4 = ((float)(t22));
  }
  }
  t23 = ((float)(v4));
  v4 = gg_function_7(t23, base + 0);
  }
  }
  t0 = ((float)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_8 gg_function_13(gg_type_8 p0, uint64_t p1, int32_t p2, int32_t p3, int64_t base) {
  gg_type_8 t0;
  float t1;
  float t2;
  int32_t t3;
  int32_t t4;
  uint8_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  uint8_t t10;
  uint64_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  uint64_t t16;
  float t17;
  uint64_t t18;
  int32_t t19;
  int32_t t20;
  int32_t t21;
  int32_t t22;
  int32_t t23;
  int32_t t24;
  uint64_t t25;
  float t26;
  int32_t t27;
  int32_t t28;
  int32_t t29;
  int32_t t30;
  uint8_t t31;
  uint64_t t32;
  int32_t t33;
  int32_t t34;
  uint64_t t35;
  float t36;
  uint64_t t37;
  int32_t t38;
  int32_t t39;
  int32_t t40;
  int32_t t41;
  uint64_t t42;
  float t43;
  float t44;
  float t45;
  gg_type_8 v0;
  uint64_t v1;
  int32_t v2;
  int32_t v3;
  gg_type_8 v4;
  int32_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  memset(&(v4), 0, sizeof(v4));
  t1 = ((float)((v0).f0));
  (v4).f0 = gg_function_7(t1, base + 0);
  t2 = ((float)((v0).f1));
  (v4).f1 = gg_function_7(t2, base + 0);
  t3 = ((int32_t)(v3));
  t4 = ((int32_t)(((int64_t)UINT64_C(32))));
  t5 = ((t3) > (t4));
  if (t5) {
  {
  t6 = ((int32_t)(v2));
  t7 = ((int32_t)(((int64_t)UINT64_C(32))));
  v5 = ((int32_t)(((int32_t)gg_mod(t6, t7))));
  t8 = ((int32_t)(v5));
  t9 = ((int32_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t11 = ((uint64_t)(v1));
  t12 = ((int32_t)(v2));
  t13 = ((int32_t)(((int64_t)UINT64_C(32))));
  t14 = ((int32_t)(((int32_t)gg_div(t12, t13))));
  t15 = ((int32_t)(((int64_t)UINT64_C(2))));
  t16 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t14) * (uint64_t)(t15)))))));
  t17 = ((float)((v4).f0));
  fg168_store(t11, t16, t17);
  t18 = ((uint64_t)(v1));
  t19 = ((int32_t)(v2));
  t20 = ((int32_t)(((int64_t)UINT64_C(32))));
  t21 = ((int32_t)(((int32_t)gg_div(t19, t20))));
  t22 = ((int32_t)(((int64_t)UINT64_C(2))));
  t23 = ((int32_t)(((int32_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((int32_t)(((int64_t)UINT64_C(1))));
  t25 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t23) + (uint64_t)(t24)))))));
  t26 = ((float)((v4).f1));
  fg168_store(t18, t25, t26);
  }
  }
  fg168_barrier();
  memset(&(v4), 0, sizeof(v4));
  (v4).f0 = ((float)(gg_double_bits(UINT64_C(0))));
  (v4).f1 = ((float)(gg_double_bits(UINT64_C(0))));
  t27 = ((int32_t)(v5));
  t28 = ((int32_t)(v3));
  t29 = ((int32_t)(((int64_t)UINT64_C(32))));
  t30 = ((int32_t)(((int32_t)gg_div(t28, t29))));
  t31 = ((t27) < (t30));
  if (t31) {
  {
  t32 = ((uint64_t)(v1));
  t33 = ((int32_t)(v5));
  t34 = ((int32_t)(((int64_t)UINT64_C(2))));
  t35 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t33) * (uint64_t)(t34)))))));
  t36 = fg168_load(t32, t35);
  (v4).f0 = ((float)(t36));
  t37 = ((uint64_t)(v1));
  t38 = ((int32_t)(v5));
  t39 = ((int32_t)(((int64_t)UINT64_C(2))));
  t40 = ((int32_t)(((int32_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  t41 = ((int32_t)(((int64_t)UINT64_C(1))));
  t42 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t40) + (uint64_t)(t41)))))));
  t43 = fg168_load(t37, t42);
  (v4).f1 = ((float)(t43));
  }
  }
  t44 = ((float)((v4).f0));
  (v4).f0 = gg_function_7(t44, base + 0);
  t45 = ((float)((v4).f1));
  (v4).f1 = gg_function_7(t45, base + 0);
  }
  }
  t0 = v4;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_19(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, uint8_t p9, uint8_t p10, uint8_t p11, uint8_t p12, int32_t p13, int32_t p14, float p15, float p16, int64_t base) {
  uint8_t t0;
  gg_type_10 t1;
  float t2;
  float t3;
  float t4;
  float t5;
  gg_type_10 t6;
  int64_t t7;
  int32_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  uint8_t t18;
  int32_t t19;
  gg_type_10 * t20;
  uint64_t t21;
  int32_t t22;
  gg_type_10 t23;
  int32_t t24;
  gg_type_10 * t25;
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
  int32_t t42;
  int32_t t43;
  float t44;
  uint64_t t45;
  int32_t t46;
  int32_t t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
  float t53;
  int32_t t54;
  int32_t t55;
  int32_t t56;
  int32_t t57;
  int32_t t58;
  int32_t t59;
  int32_t t60;
  int32_t t61;
  int32_t t62;
  int32_t t63;
  uint8_t t64;
  int32_t t65;
  gg_type_10 * t66;
  float t67;
  float t68;
  float t69;
  float t70;
  float t71;
  float t72;
  float t73;
  float t74;
  uint8_t t75;
  uint64_t t76;
  int32_t t77;
  gg_type_10 t78;
  float t79;
  float t80;
  float t81;
  float t82;
  float t83;
  float t84;
  float t85;
  float t86;
  uint8_t t87;
  uint64_t t88;
  int32_t t89;
  gg_type_10 t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  float t97;
  float t98;
  uint8_t t99;
  float t100;
  float t101;
  float t102;
  float t103;
  float t104;
  float t105;
  float t106;
  float t107;
  uint64_t t108;
  int32_t t109;
  gg_type_10 t110;
  int32_t t111;
  int32_t t112;
  int64_t t113;
  int64_t t114;
  uint64_t t115;
  uint64_t t116;
  float t117;
  float t118;
  float t119;
  float t120;
  float t121;
  int64_t t122;
  int64_t t123;
  float t124;
  uint64_t t125;
  int32_t t126;
  int32_t t127;
  float t128;
  float t129;
  float t130;
  float t131;
  float t132;
  float t133;
  int64_t t134;
  int64_t t135;
  uint8_t t136;
  uint8_t t137;
  uint64_t t138;
  uint64_t t139;
  float t140;
  uint64_t t141;
  uint64_t t142;
  float t143;
  float t144;
  float t145;
  uint64_t t146;
  int64_t t147;
  int64_t t148;
  uint64_t t149;
  float t150;
  float t151;
  uint64_t t152;
  int64_t t153;
  int64_t t154;
  uint64_t t155;
  float t156;
  float t157;
  float t158;
  float t159;
  uint64_t t160;
  uint64_t t161;
  float t162;
  uint64_t t163;
  uint64_t t164;
  float t165;
  float t166;
  float t167;
  uint64_t t168;
  int64_t t169;
  int64_t t170;
  uint64_t t171;
  float t172;
  float t173;
  float t174;
  uint8_t t175;
  uint64_t t176;
  uint64_t t177;
  float t178;
  float t179;
  uint64_t t180;
  uint64_t t181;
  float t182;
  float t183;
  float t184;
  float t185;
  uint64_t t186;
  uint64_t t187;
  float t188;
  uint64_t t189;
  uint64_t t190;
  float t191;
  float t192;
  float t193;
  int64_t t194;
  int64_t t195;
  int32_t t196;
  int32_t t197;
  uint8_t t198;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  int32_t v8;
  uint8_t v9;
  uint8_t v10;
  uint8_t v11;
  uint8_t v12;
  int32_t v13;
  int32_t v14;
  float v15;
  float v16;
  float v17;
  gg_type_11 v18;
  int32_t v19;
  int32_t v20;
  gg_type_10 v21;
  float v22;
  int32_t v23;
  gg_type_10 v24;
  gg_type_10 v25;
  gg_type_10 v26;
  gg_type_10 v27;
  int64_t v28;
  float v29;
  float v30;
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
  {
  v17 = ((float)(gg_double_bits(UINT64_C(0))));
  t0 = v9;
  if (t0) {
  {
  memset(&(t1), 0, sizeof(t1));
  memset(&t2, 0, sizeof(t2));
  (t1).f2 = ((float)(t2));
  memset(&t3, 0, sizeof(t3));
  (t1).f3 = ((float)(t3));
  memset(&t4, 0, sizeof(t4));
  (t1).f4 = ((float)(t4));
  memset(&t5, 0, sizeof(t5));
  (t1).f5 = ((float)(t5));
  t6 = t1;
  for (t7=0; t7<4; t7++) (v18).data[t7] = t6;
  v19 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t8 = ((int32_t)(v19));
  t9 = ((int32_t)(v8));
  if (!(((t8) < (t9)))) goto label2;
  {
  t10 = ((int32_t)(v6));
  t11 = ((int32_t)(v19));
  t12 = ((int32_t)(v7));
  t13 = ((int32_t)(((int32_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  v20 = ((int32_t)(((int32_t)((uint64_t)(t10) + (uint64_t)(t13)))));
  t14 = ((int32_t)(v20));
  t15 = ((int32_t)(v5));
  t16 = ((int32_t)(((int64_t)UINT64_C(4))));
  t17 = ((int32_t)(((int32_t)gg_div(t15, t16))));
  t18 = ((t14) < (t17));
  if (t18) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t19 = v19;
  t20 = ((gg_type_10 *)(v18).data) + gg_index(t19, ((int64_t)UINT64_C(4)));
  t21 = ((uint64_t)(v0));
  t22 = ((int32_t)(v20));
  t23 = fg168_load4(t21, t22);
  (*t20) = t23;
  /* FG_DIRECT_SLICE_INDEX */
  t24 = v19;
  t25 = ((gg_type_10 *)(v18).data) + gg_index(t24, ((int64_t)UINT64_C(4)));
  v21 = (*t25);
  t26 = v17;
  t27 = ((float)((v21).f2));
  t28 = ((float)((v21).f2));
  t29 = ((float)(((t27) * (t28))));
  t30 = ((float)((v21).f3));
  t31 = ((float)((v21).f3));
  t32 = ((float)(((t30) * (t31))));
  t33 = ((float)(((t29) + (t32))));
  t34 = ((float)((v21).f4));
  t35 = ((float)((v21).f4));
  t36 = ((float)(((t34) * (t35))));
  t37 = ((float)(((t33) + (t36))));
  t38 = ((float)((v21).f5));
  t39 = ((float)((v21).f5));
  t40 = ((float)(((t38) * (t39))));
  t41 = ((float)(((t37) + (t40))));
  v17 = ((t26) + (t41));
  }
  }
  t42 = v19;
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
  v19 = ((int32_t)((uint64_t)(t42) + (uint64_t)(t43)));
  }
  goto label1;
label2:;
  t44 = ((float)(v17));
  t45 = ((uint64_t)(v2));
  t46 = ((int32_t)(v6));
  t47 = ((int32_t)(v7));
  v17 = gg_function_6(t44, t45, t46, t47, base + 0);
  t48 = ((float)(v17));
  t49 = ((float)(((float)(v5))));
  t50 = ((float)(((t48) / (t49))));
  t51 = ((float)(v15));
  t52 = ((float)(((t50) + (t51))));
  t53 = fg168_rsqrt(t52);
  v22 = ((float)(t53));
  v19 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t54 = ((int32_t)(v19));
  t55 = ((int32_t)(v8));
  if (!(((t54) < (t55)))) goto label5;
  {
  t56 = ((int32_t)(v6));
  t57 = ((int32_t)(v19));
  t58 = ((int32_t)(v7));
  t59 = ((int32_t)(((int32_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  v23 = ((int32_t)(((int32_t)((uint64_t)(t56) + (uint64_t)(t59)))));
  t60 = ((int32_t)(v23));
  t61 = ((int32_t)(v5));
  t62 = ((int32_t)(((int64_t)UINT64_C(4))));
  t63 = ((int32_t)(((int32_t)gg_div(t61, t62))));
  t64 = ((t60) < (t63));
  if (t64) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t65 = v19;
  t66 = ((gg_type_10 *)(v18).data) + gg_index(t65, ((int64_t)UINT64_C(4)));
  v24 = (*t66);
  memset(&(v25), 0, sizeof(v25));
  t67 = ((float)(v22));
  t68 = ((float)((v24).f2));
  (v25).f2 = ((float)(((t67) * (t68))));
  t69 = ((float)(v22));
  t70 = ((float)((v24).f3));
  (v25).f3 = ((float)(((t69) * (t70))));
  t71 = ((float)(v22));
  t72 = ((float)((v24).f4));
  (v25).f4 = ((float)(((t71) * (t72))));
  t73 = ((float)(v22));
  t74 = ((float)((v24).f5));
  (v25).f5 = ((float)(((t73) * (t74))));
  t75 = v10;
  if (t75) {
  {
  t76 = ((uint64_t)(v3));
  t77 = ((int32_t)(v23));
  t78 = fg168_load4(t76, t77);
  v26 = t78;
  t79 = (v25).f2;
  t80 = ((float)((v26).f2));
  (v25).f2 = ((t79) * (t80));
  t81 = (v25).f3;
  t82 = ((float)((v26).f3));
  (v25).f3 = ((t81) * (t82));
  t83 = (v25).f4;
  t84 = ((float)((v26).f4));
  (v25).f4 = ((t83) * (t84));
  t85 = (v25).f5;
  t86 = ((float)((v26).f5));
  (v25).f5 = ((t85) * (t86));
  }
  }
  t87 = v11;
  if (t87) {
  {
  t88 = ((uint64_t)(v4));
  t89 = ((int32_t)(v23));
  t90 = fg168_load4(t88, t89);
  v27 = t90;
  t91 = (v25).f2;
  t92 = ((float)((v27).f2));
  (v25).f2 = ((t91) + (t92));
  t93 = (v25).f3;
  t94 = ((float)((v27).f3));
  (v25).f3 = ((t93) + (t94));
  t95 = (v25).f4;
  t96 = ((float)((v27).f4));
  (v25).f4 = ((t95) + (t96));
  t97 = (v25).f5;
  t98 = ((float)((v27).f5));
  (v25).f5 = ((t97) + (t98));
  }
  }
  t99 = v12;
  if (t99) {
  {
  t100 = (v25).f2;
  t101 = ((float)(v16));
  (v25).f2 = ((t100) * (t101));
  t102 = (v25).f3;
  t103 = ((float)(v16));
  (v25).f3 = ((t102) * (t103));
  t104 = (v25).f4;
  t105 = ((float)(v16));
  (v25).f4 = ((t104) * (t105));
  t106 = (v25).f5;
  t107 = ((float)(v16));
  (v25).f5 = ((t106) * (t107));
  }
  }
  t108 = ((uint64_t)(v1));
  t109 = ((int32_t)(v23));
  t110 = v25;
  fg168_store4(t108, t109, t110);
  }
  }
  t111 = v19;
  t112 = ((int32_t)(((int64_t)UINT64_C(1))));
  v19 = ((int32_t)((uint64_t)(t111) + (uint64_t)(t112)));
  }
  goto label4;
label5:;
  }
  } else {
  {
  v28 = ((int64_t)(((int64_t)(v6))));
label7:;
  t113 = ((int64_t)(v28));
  t114 = ((int64_t)(((int64_t)(v5))));
  if (!(((t113) < (t114)))) goto label8;
  {
  t115 = ((uint64_t)(v0));
  t116 = ((uint64_t)(((uint64_t)(v28))));
  t117 = fg168_load(t115, t116);
  v29 = ((float)(t117));
  t118 = v17;
  t119 = ((float)(v29));
  t120 = ((float)(v29));
  t121 = ((float)(((t119) * (t120))));
  v17 = ((t118) + (t121));
  t122 = v28;
  t123 = ((int64_t)(((int64_t)(v7))));
  v28 = ((int64_t)((uint64_t)(t122) + (uint64_t)(t123)));
  }
  goto label7;
label8:;
  t124 = ((float)(v17));
  t125 = ((uint64_t)(v2));
  t126 = ((int32_t)(v6));
  t127 = ((int32_t)(v7));
  v17 = gg_function_6(t124, t125, t126, t127, base + 0);
  t128 = ((float)(v17));
  t129 = ((float)(((float)(v5))));
  t130 = ((float)(((t128) / (t129))));
  t131 = ((float)(v15));
  t132 = ((float)(((t130) + (t131))));
  t133 = fg168_rsqrt(t132);
  v30 = ((float)(t133));
  v28 = ((int64_t)(((int64_t)(v6))));
label10:;
  t134 = ((int64_t)(v28));
  t135 = ((int64_t)(((int64_t)(v5))));
  if (!(((t134) < (t135)))) goto label11;
  {
  t136 = v10;
  if (t136) {
  {
  t137 = v11;
  if (t137) {
  {
  t138 = ((uint64_t)(v1));
  t139 = ((uint64_t)(((uint64_t)(v28))));
  t140 = ((float)(v30));
  t141 = ((uint64_t)(v0));
  t142 = ((uint64_t)(((uint64_t)(v28))));
  t143 = fg168_load(t141, t142);
  t144 = ((float)(t143));
  t145 = ((float)(((t140) * (t144))));
  t146 = ((uint64_t)(v3));
  t147 = ((int64_t)(v28));
  t148 = ((int64_t)(v13));
  t149 = ((uint64_t)(((uint64_t)(((int64_t)gg_mod(t147, t148))))));
  t150 = fg168_load(t146, t149);
  t151 = ((float)(t150));
  t152 = ((uint64_t)(v4));
  t153 = ((int64_t)(v28));
  t154 = ((int64_t)(v14));
  t155 = ((uint64_t)(((uint64_t)(((int64_t)gg_mod(t153, t154))))));
  t156 = fg168_load(t152, t155);
  t157 = ((float)(t156));
  t158 = fg168_fma(t145, t151, t157);
  t159 = ((float)(t158));
  fg168_store(t138, t139, t159);
  }
  } else {
  {
  t160 = ((uint64_t)(v1));
  t161 = ((uint64_t)(((uint64_t)(v28))));
  t162 = ((float)(v30));
  t163 = ((uint64_t)(v0));
  t164 = ((uint64_t)(((uint64_t)(v28))));
  t165 = fg168_load(t163, t164);
  t166 = ((float)(t165));
  t167 = ((float)(((t162) * (t166))));
  t168 = ((uint64_t)(v3));
  t169 = ((int64_t)(v28));
  t170 = ((int64_t)(v13));
  t171 = ((uint64_t)(((uint64_t)(((int64_t)gg_mod(t169, t170))))));
  t172 = fg168_load(t168, t171);
  t173 = ((float)(t172));
  t174 = ((float)(((t167) * (t173))));
  fg168_store(t160, t161, t174);
  }
  }
  }
  } else {
  {
  t175 = v12;
  if (t175) {
  {
  t176 = ((uint64_t)(v1));
  t177 = ((uint64_t)(((uint64_t)(v28))));
  t178 = ((float)(v16));
  t179 = ((float)(v30));
  t180 = ((uint64_t)(v0));
  t181 = ((uint64_t)(((uint64_t)(v28))));
  t182 = fg168_load(t180, t181);
  t183 = ((float)(t182));
  t184 = ((float)(((t179) * (t183))));
  t185 = ((float)(((t178) * (t184))));
  fg168_store(t176, t177, t185);
  }
  } else {
  {
  t186 = ((uint64_t)(v1));
  t187 = ((uint64_t)(((uint64_t)(v28))));
  t188 = ((float)(v30));
  t189 = ((uint64_t)(v0));
  t190 = ((uint64_t)(((uint64_t)(v28))));
  t191 = fg168_load(t189, t190);
  t192 = ((float)(t191));
  t193 = ((float)(((t188) * (t192))));
  fg168_store(t186, t187, t193);
  }
  }
  }
  }
  t194 = v28;
  t195 = ((int64_t)(((int64_t)(v7))));
  v28 = ((int64_t)((uint64_t)(t194) + (uint64_t)(t195)));
  }
  goto label10;
label11:;
  }
  }
  t196 = ((int32_t)(v7));
  t197 = ((int32_t)(((int64_t)UINT64_C(32))));
  t198 = ((t196) > (t197));
  if (t198) {
  {
  fg168_barrier();
  }
  }
  }
gg_done:;
}
static uint64_t gg_function_12(int64_t p0, int64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((uint64_t)(v2));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(2))));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t9 = fg168_di(t2, t3, t8);
  t10 = ((uint64_t)(t9));
  t11 = ((uint64_t)(((uint64_t)((uint64_t)(t1) * (uint64_t)(t10)))));
  t12 = ((uint64_t)(v3));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(4))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(2))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = fg168_di(t13, t14, t19);
  t21 = ((uint64_t)(t20));
  t22 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t21)))));
  t23 = ((uint64_t)(((uint64_t)((uint64_t)(t11) + (uint64_t)(t22)))));
  t24 = ((uint64_t)(v4));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(2))));
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(((int64_t)UINT64_C(3))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t32 = fg168_di(t25, t26, t31);
  t33 = ((uint64_t)(t32));
  t34 = ((uint64_t)(((uint64_t)((uint64_t)(t24) * (uint64_t)(t33)))));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t23) + (uint64_t)(t34)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint32_t gg_function_16(int64_t p0, uint32_t p1, int64_t p2, int64_t base) {
  uint32_t t0;
  uint32_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint32_t t5;
  uint32_t t6;
  uint32_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint32_t t14;
  uint64_t t15;
  uint32_t t16;
  uint32_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint32_t t21;
  uint32_t t22;
  uint32_t t23;
  int64_t v0;
  uint32_t v1;
  int64_t v2;
  uint32_t v3;
  uint32_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((uint32_t)(v1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg168_dm(t2, t3, t4);
  t6 = ((uint32_t)(t5));
  t7 = fg168_umulhi(t1, t6);
  v3 = ((uint32_t)(t7));
  t8 = ((uint64_t)(((uint64_t)(v3))));
  t9 = ((uint64_t)(((uint64_t)(v1))));
  t10 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = fg168_dm(t11, t12, t13);
  t15 = ((uint64_t)(((uint64_t)(t14))));
  v4 = ((uint32_t)(((uint32_t)(((t10) >> ((uint64_t)(t15) & 63))))));
  t16 = ((uint32_t)(v1));
  t17 = ((uint32_t)(v4));
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = fg168_dm(t18, t19, t20);
  t22 = ((uint32_t)(t21));
  t23 = ((uint32_t)(((uint32_t)((uint64_t)(t17) * (uint64_t)(t22)))));
  t0 = ((uint32_t)(((uint32_t)((uint64_t)(t16) - (uint64_t)(t23)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_15(int64_t p0, int64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint32_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint32_t t10;
  int64_t t11;
  uint32_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint32_t t16;
  int64_t t17;
  uint32_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint32_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  int64_t v5;
  uint64_t v6;
  uint64_t v7;
  uint64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) - (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)UINT64_C(4))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t5 = ((int64_t)(v0));
  t6 = ((uint32_t)(((uint32_t)(v2))));
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = gg_function_16(t5, t6, t9, base + 0);
  v6 = ((uint64_t)(((uint64_t)(t10))));
  t11 = ((int64_t)(v0));
  t12 = ((uint32_t)(((uint32_t)(v3))));
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = gg_function_16(t11, t12, t15, base + 0);
  v7 = ((uint64_t)(((uint64_t)(t16))));
  t17 = ((int64_t)(v0));
  t18 = ((uint32_t)(((uint32_t)(v4))));
  t19 = ((int64_t)(v5));
  t20 = ((int64_t)(((int64_t)UINT64_C(3))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t22 = gg_function_16(t17, t18, t21, base + 0);
  v8 = ((uint64_t)(((uint64_t)(t22))));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(v1));
  t25 = ((uint64_t)(v6));
  t26 = ((uint64_t)(v7));
  t27 = ((uint64_t)(v8));
  t0 = gg_function_12(t23, t24, t25, t26, t27, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_1(int64_t p0, int64_t p1, int32_t p2, int32_t p3, uint8_t p4, uint8_t p5, uint8_t p6, uint8_t p7, int32_t p8, uint64_t p9, uint64_t p10, uint64_t p11, uint64_t p12, uint64_t p13, uint64_t p14, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  float t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint8_t t36;
  uint64_t t37;
  uint64_t t38;
  uint64_t t39;
  uint64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  uint64_t t44;
  uint64_t t45;
  uint64_t t46;
  float t47;
  uint64_t t48;
  uint64_t t49;
  float t50;
  float t51;
  uint64_t t52;
  uint64_t t53;
  float t54;
  uint64_t t55;
  int32_t t56;
  int32_t t57;
  float t58;
  float t59;
  uint64_t t60;
  uint64_t t61;
  uint64_t t62;
  uint64_t t63;
  float t64;
  float t65;
  float t66;
  uint64_t t67;
  uint64_t t68;
  float t69;
  float t70;
  float t71;
  float t72;
  float t73;
  uint64_t t74;
  uint64_t t75;
  float t76;
  uint64_t t77;
  uint64_t t78;
  uint64_t t79;
  int32_t t80;
  int32_t t81;
  float t82;
  float t83;
  float t84;
  float t85;
  float t86;
  float t87;
  uint64_t t88;
  uint64_t t89;
  uint64_t t90;
  uint64_t t91;
  uint64_t t92;
  uint64_t t93;
  float t94;
  float t95;
  float t96;
  float t97;
  uint64_t t98;
  uint64_t t99;
  int64_t t100;
  int64_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  uint64_t t110;
  uint64_t t111;
  uint64_t t112;
  uint64_t t113;
  uint64_t t114;
  uint64_t t115;
  uint64_t t116;
  uint64_t t117;
  uint64_t t118;
  uint64_t t119;
  uint64_t t120;
  uint64_t t121;
  int64_t t122;
  int64_t t123;
  uint64_t t124;
  uint64_t t125;
  uint64_t t126;
  uint64_t t127;
  uint64_t t128;
  uint64_t t129;
  uint64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint64_t t134;
  uint64_t t135;
  int64_t t136;
  int64_t t137;
  uint64_t t138;
  uint64_t t139;
  uint64_t t140;
  uint64_t t141;
  uint64_t t142;
  uint64_t t143;
  uint64_t t144;
  uint64_t t145;
  int64_t t146;
  int64_t t147;
  uint64_t t148;
  uint64_t t149;
  uint64_t t150;
  uint64_t t151;
  uint64_t t152;
  uint64_t t153;
  uint64_t t154;
  int64_t t155;
  int64_t t156;
  uint64_t t157;
  uint64_t t158;
  float t159;
  float t160;
  float t161;
  float t162;
  float t163;
  float t164;
  float t165;
  uint64_t t166;
  uint64_t t167;
  float t168;
  float t169;
  float t170;
  int64_t t171;
  int64_t t172;
  gg_type_8 t173;
  uint64_t t174;
  int32_t t175;
  int32_t t176;
  float t177;
  float t178;
  float t179;
  float t180;
  float t181;
  float t182;
  float t183;
  float t184;
  float t185;
  float t186;
  float t187;
  float t188;
  float t189;
  float t190;
  float t191;
  int64_t t192;
  int64_t t193;
  uint64_t t194;
  uint64_t t195;
  float t196;
  uint64_t t197;
  uint64_t t198;
  float t199;
  float t200;
  float t201;
  float t202;
  uint64_t t203;
  uint64_t t204;
  float t205;
  float t206;
  float t207;
  float t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  uint8_t t213;
  uint8_t t214;
  int64_t t215;
  int64_t t216;
  uint8_t t217;
  uint64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  uint64_t t222;
  uint64_t t223;
  uint64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  uint64_t t228;
  uint64_t t229;
  uint64_t t230;
  int64_t t231;
  int64_t t232;
  uint64_t t233;
  uint64_t t234;
  uint64_t t235;
  uint64_t t236;
  uint64_t t237;
  uint64_t t238;
  uint64_t t239;
  uint64_t t240;
  int64_t t241;
  int64_t t242;
  uint64_t t243;
  uint64_t t244;
  uint64_t t245;
  uint64_t t246;
  uint64_t t247;
  uint64_t t248;
  uint64_t t249;
  int64_t t250;
  int64_t t251;
  uint8_t t252;
  uint8_t t253;
  int64_t t254;
  int64_t t255;
  uint8_t t256;
  uint8_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  uint64_t t261;
  uint64_t t262;
  int64_t t263;
  int64_t t264;
  uint64_t t265;
  uint64_t t266;
  uint64_t t267;
  uint64_t t268;
  uint64_t t269;
  uint64_t t270;
  uint64_t t271;
  uint8_t t272;
  int64_t t273;
  int64_t t274;
  int64_t t275;
  uint64_t t276;
  uint64_t t277;
  int64_t t278;
  int64_t t279;
  uint64_t t280;
  uint64_t t281;
  uint64_t t282;
  uint64_t t283;
  uint64_t t284;
  uint64_t t285;
  uint64_t t286;
  uint64_t t287;
  uint64_t t288;
  uint64_t t289;
  uint64_t t290;
  uint64_t t291;
  int32_t t292;
  int32_t t293;
  int32_t t294;
  int32_t t295;
  uint8_t t296;
  uint8_t t297;
  uint8_t t298;
  uint8_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  uint64_t t303;
  int32_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  uint64_t t308;
  int32_t t309;
  float t310;
  int64_t t311;
  int64_t t312;
  float t313;
  float t314;
  int64_t t315;
  int64_t t316;
  uint64_t t317;
  uint64_t t318;
  float t319;
  int64_t t320;
  int64_t t321;
  uint8_t t322;
  float t323;
  float t324;
  float t325;
  float t326;
  float t327;
  float t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  uint8_t t333;
  gg_type_8 t334;
  uint64_t t335;
  int32_t t336;
  int32_t t337;
  float t338;
  float t339;
  float t340;
  float t341;
  float t342;
  float t343;
  float t344;
  float t345;
  float t346;
  float t347;
  float t348;
  float t349;
  float t350;
  uint64_t t351;
  int32_t t352;
  int32_t t353;
  float t354;
  float t355;
  float t356;
  float t357;
  float t358;
  float t359;
  float t360;
  int64_t t361;
  int64_t t362;
  uint64_t t363;
  uint64_t t364;
  float t365;
  int64_t t366;
  int64_t t367;
  uint8_t t368;
  uint64_t t369;
  uint64_t t370;
  float t371;
  float t372;
  float t373;
  float t374;
  float t375;
  float t376;
  uint64_t t377;
  uint64_t t378;
  float t379;
  float t380;
  float t381;
  int64_t t382;
  int64_t t383;
  int32_t t384;
  int32_t t385;
  uint8_t t386;
  uint64_t t387;
  uint64_t t388;
  uint64_t t389;
  uint64_t t390;
  int64_t v0;
  int64_t v1;
  int32_t v2;
  int32_t v3;
  uint8_t v4;
  uint8_t v5;
  uint8_t v6;
  uint8_t v7;
  int32_t v8;
  uint64_t v9;
  uint64_t v10;
  uint64_t v11;
  uint64_t v12;
  uint64_t v13;
  uint64_t v14;
  uint64_t v15;
  uint64_t v16;
  int32_t v17;
  float v18;
  uint64_t v19;
  uint64_t v20;
  uint64_t v21;
  float v22;
  uint64_t v23;
  float v24;
  float v25;
  float v26;
  uint64_t v27;
  uint64_t v28;
  uint64_t v29;
  uint64_t v30;
  uint64_t v31;
  uint64_t v32;
  uint64_t v33;
  uint64_t v34;
  gg_type_8 v35;
  int64_t v36;
  float v37;
  float v38;
  float v39;
  float v40;
  float v41;
  uint64_t v42;
  uint64_t v43;
  uint64_t v44;
  uint64_t v45;
  uint64_t v46;
  uint64_t v47;
  gg_type_8 v48;
  int64_t v49;
  float v50;
  float v51;
  float v52;
  float v53;
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
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg168_di(t0, t1, t2);
  v15 = ((uint64_t)(t3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = fg168_di(t4, t5, t6);
  v16 = ((uint64_t)(t7));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(3))));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = fg168_di(t8, t9, t10);
  v17 = ((int32_t)(((int32_t)(t11))));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg168_df(t12, t13);
  v18 = ((float)(t14));
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(3))));
  t20 = ((int64_t)(((int64_t)UINT64_C(15))));
  t21 = fg168_di(t18, t19, t20);
  v19 = ((uint64_t)(t21));
  t22 = ((uint64_t)(v9));
  t23 = ((uint64_t)(v19));
  t24 = ((uint64_t)(((uint64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((uint64_t)(((uint64_t)(v8))));
  v20 = ((uint64_t)(((uint64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t26 = ((uint64_t)(v9));
  t27 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t28 = ((uint64_t)(((uint64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  t29 = ((uint64_t)(v19));
  t30 = ((uint64_t)(((uint64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  t33 = ((int64_t)(((int64_t)UINT64_C(16))));
  t34 = fg168_di(t31, t32, t33);
  t35 = ((uint64_t)(t34));
  t36 = ((t30) < (t35));
  if (t36) {
  {
  t37 = ((uint64_t)(v9));
  t38 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t39 = ((uint64_t)(((uint64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t40 = ((uint64_t)(v19));
  v21 = ((uint64_t)(((uint64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  }
  } else {
  {
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(3))));
  t43 = ((int64_t)(((int64_t)UINT64_C(16))));
  t44 = fg168_di(t41, t42, t43);
  v21 = ((uint64_t)(t44));
  }
  }
  v22 = ((float)(gg_double_bits(UINT64_C(0))));
  fg168_sync();
  v23 = ((uint64_t)(v20));
label1:;
  t45 = ((uint64_t)(v23));
  t46 = ((uint64_t)(v21));
  if (!(((t45) < (t46)))) goto label2;
  {
  t47 = v22;
  t48 = ((uint64_t)(v15));
  t49 = ((uint64_t)(v23));
  t50 = fg168_load(t48, t49);
  t51 = ((float)(t50));
  v22 = ((t47) + (t51));
  t52 = v23;
  t53 = ((uint64_t)(((uint64_t)(v2))));
  v23 = ((uint64_t)((uint64_t)(t52) + (uint64_t)(t53)));
  }
  goto label1;
label2:;
  t54 = ((float)(v22));
  t55 = ((uint64_t)(v14));
  t56 = ((int32_t)(v8));
  t57 = ((int32_t)(v2));
  v22 = gg_function_6(t54, t55, t56, t57, base + 0);
  t58 = ((float)(v22));
  t59 = ((float)(((float)(v19))));
  v24 = ((float)(((t58) / (t59))));
  v22 = ((float)(gg_double_bits(UINT64_C(0))));
  v23 = ((uint64_t)(v20));
label4:;
  t60 = ((uint64_t)(v23));
  t61 = ((uint64_t)(v21));
  if (!(((t60) < (t61)))) goto label5;
  {
  t62 = ((uint64_t)(v15));
  t63 = ((uint64_t)(v23));
  t64 = fg168_load(t62, t63);
  t65 = ((float)(t64));
  t66 = ((float)(v24));
  v25 = ((float)(((t65) - (t66))));
  t67 = ((uint64_t)(v16));
  t68 = ((uint64_t)(v23));
  t69 = ((float)(v25));
  fg168_store(t67, t68, t69);
  t70 = v22;
  t71 = ((float)(v25));
  t72 = ((float)(v25));
  t73 = ((float)(((t71) * (t72))));
  v22 = ((t70) + (t73));
  t74 = v23;
  t75 = ((uint64_t)(((uint64_t)(v2))));
  v23 = ((uint64_t)((uint64_t)(t74) + (uint64_t)(t75)));
  }
  goto label4;
label5:;
  t76 = ((float)(v22));
  t77 = ((uint64_t)(v14));
  t78 = ((uint64_t)(((int64_t)UINT64_C(128))));
  t79 = ((uint64_t)(((uint64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  t80 = ((int32_t)(v8));
  t81 = ((int32_t)(v2));
  v22 = gg_function_6(t76, t79, t80, t81, base + 0);
  t82 = ((float)(v22));
  t83 = ((float)(((float)(v19))));
  t84 = ((float)(((t82) / (t83))));
  t85 = ((float)(v18));
  t86 = ((float)(((t84) + (t85))));
  t87 = fg168_rsqrt(t86);
  v26 = ((float)(t87));
  v23 = ((uint64_t)(v20));
label7:;
  t88 = ((uint64_t)(v23));
  t89 = ((uint64_t)(v21));
  if (!(((t88) < (t89)))) goto label8;
  {
  t90 = ((uint64_t)(v16));
  t91 = ((uint64_t)(v23));
  t92 = ((uint64_t)(v16));
  t93 = ((uint64_t)(v23));
  t94 = fg168_load(t92, t93);
  t95 = ((float)(t94));
  t96 = ((float)(v26));
  t97 = ((float)(((t95) * (t96))));
  fg168_store(t90, t91, t97);
  t98 = v23;
  t99 = ((uint64_t)(((uint64_t)(v2))));
  v23 = ((uint64_t)((uint64_t)(t98) + (uint64_t)(t99)));
  }
  goto label7;
label8:;
  goto gg_done;
  }
  }
  t100 = ((int64_t)(v1));
  t101 = ((int64_t)(((int64_t)UINT64_C(6))));
  t102 = ((t100) == (t101));
  if (t102) {
  {
  t103 = ((int64_t)(v0));
  t104 = ((int64_t)(((int64_t)UINT64_C(3))));
  t105 = ((int64_t)(((int64_t)UINT64_C(2))));
  t106 = fg168_di(t103, t104, t105);
  v27 = ((uint64_t)(t106));
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(((int64_t)UINT64_C(3))));
  t109 = ((int64_t)(((int64_t)UINT64_C(3))));
  t110 = fg168_di(t107, t108, t109);
  v28 = ((uint64_t)(t110));
  t111 = ((uint64_t)(v9));
  t112 = ((uint64_t)(v27));
  v29 = ((uint64_t)(((uint64_t)gg_umod(t111, t112))));
  t113 = ((uint64_t)(v9));
  t114 = ((uint64_t)(v27));
  t115 = ((uint64_t)(((uint64_t)gg_udiv(t113, t114))));
  t116 = ((uint64_t)(v28));
  v30 = ((uint64_t)(((uint64_t)gg_umod(t115, t116))));
  t117 = ((uint64_t)(v9));
  t118 = ((uint64_t)(v27));
  t119 = ((uint64_t)(v28));
  t120 = ((uint64_t)(((uint64_t)((uint64_t)(t118) * (uint64_t)(t119)))));
  v31 = ((uint64_t)(((uint64_t)gg_udiv(t117, t120))));
  t121 = ((uint64_t)(v15));
  t122 = ((int64_t)(v0));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = ((uint64_t)(v29));
  t125 = ((uint64_t)(v30));
  t126 = ((uint64_t)(v31));
  t127 = gg_function_12(t122, t123, t124, t125, t126, base + 0);
  t128 = ((uint64_t)(t127));
  t129 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t130 = ((uint64_t)(((uint64_t)((uint64_t)(t128) * (uint64_t)(t129)))));
  v32 = ((uint64_t)(((uint64_t)((uint64_t)(t121) + (uint64_t)(t130)))));
  t131 = ((int64_t)(v0));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t133 = ((int64_t)(((int64_t)UINT64_C(4))));
  t134 = fg168_di(t131, t132, t133);
  t135 = ((uint64_t)(t134));
  t136 = ((int64_t)(v0));
  t137 = ((int64_t)(((int64_t)UINT64_C(4))));
  t138 = ((uint64_t)(v29));
  t139 = ((uint64_t)(v30));
  t140 = ((uint64_t)(v31));
  t141 = gg_function_12(t136, t137, t138, t139, t140, base + 0);
  t142 = ((uint64_t)(t141));
  t143 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t144 = ((uint64_t)(((uint64_t)((uint64_t)(t142) * (uint64_t)(t143)))));
  v33 = ((uint64_t)(((uint64_t)((uint64_t)(t135) + (uint64_t)(t144)))));
  t145 = ((uint64_t)(v16));
  t146 = ((int64_t)(v0));
  t147 = ((int64_t)(((int64_t)UINT64_C(1))));
  t148 = ((uint64_t)(v29));
  t149 = ((uint64_t)(v30));
  t150 = ((uint64_t)(v31));
  t151 = gg_function_12(t146, t147, t148, t149, t150, base + 0);
  t152 = ((uint64_t)(t151));
  t153 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t154 = ((uint64_t)(((uint64_t)((uint64_t)(t152) * (uint64_t)(t153)))));
  v34 = ((uint64_t)(((uint64_t)((uint64_t)(t145) + (uint64_t)(t154)))));
  memset(&(v35), 0, sizeof(v35));
  (v35).f0 = ((float)(gg_double_bits(UINT64_C(0))));
  (v35).f1 = ((float)(gg_double_bits(UINT64_C(0))));
  v36 = ((int64_t)(((int64_t)(v8))));
  fg168_sync();
label10:;
  t155 = ((int64_t)(v36));
  t156 = ((int64_t)(((int64_t)(v17))));
  if (!(((t155) < (t156)))) goto label11;
  {
  t157 = ((uint64_t)(v32));
  t158 = ((uint64_t)(((uint64_t)(v36))));
  t159 = fg168_load(t157, t158);
  v37 = ((float)(t159));
  t160 = (v35).f0;
  t161 = ((float)(v37));
  t162 = ((float)(v37));
  t163 = ((float)(((t161) * (t162))));
  (v35).f0 = ((t160) + (t163));
  t164 = (v35).f1;
  t165 = ((float)(v37));
  t166 = ((uint64_t)(v33));
  t167 = ((uint64_t)(((uint64_t)(v36))));
  t168 = fg168_load(t166, t167);
  t169 = ((float)(t168));
  t170 = ((float)(((t165) * (t169))));
  (v35).f1 = ((t164) + (t170));
  t171 = v36;
  t172 = ((int64_t)(((int64_t)(v2))));
  v36 = ((int64_t)((uint64_t)(t171) + (uint64_t)(t172)));
  }
  goto label10;
label11:;
  t173 = v35;
  t174 = ((uint64_t)(v14));
  t175 = ((int32_t)(v8));
  t176 = ((int32_t)(v2));
  v35 = gg_function_13(t173, t174, t175, t176, base + 0);
  t177 = ((float)((v35).f0));
  t178 = ((float)(((float)(v17))));
  t179 = ((float)(((t177) / (t178))));
  t180 = ((float)(v18));
  v38 = ((float)(((t179) + (t180))));
  t181 = ((float)((v35).f0));
  t182 = ((float)(((float)(v17))));
  t183 = ((float)(v18));
  t184 = ((float)(((t182) * (t183))));
  v39 = ((float)(((t181) + (t184))));
  t185 = ((float)(v38));
  t186 = fg168_rsqrt(t185);
  v40 = ((float)(t186));
  t187 = v40;
  t188 = ((float)((-(t187))));
  t189 = ((float)((v35).f1));
  t190 = ((float)(((t188) * (t189))));
  t191 = ((float)(v39));
  v41 = ((float)(((t190) / (t191))));
  v36 = ((int64_t)(((int64_t)(v8))));
label13:;
  t192 = ((int64_t)(v36));
  t193 = ((int64_t)(((int64_t)(v17))));
  if (!(((t192) < (t193)))) goto label14;
  {
  t194 = ((uint64_t)(v34));
  t195 = ((uint64_t)(((uint64_t)(v36))));
  t196 = ((float)(v40));
  t197 = ((uint64_t)(v33));
  t198 = ((uint64_t)(((uint64_t)(v36))));
  t199 = fg168_load(t197, t198);
  t200 = ((float)(t199));
  t201 = ((float)(((t196) * (t200))));
  t202 = ((float)(v41));
  t203 = ((uint64_t)(v32));
  t204 = ((uint64_t)(((uint64_t)(v36))));
  t205 = fg168_load(t203, t204);
  t206 = ((float)(t205));
  t207 = ((float)(((t202) * (t206))));
  t208 = ((float)(((t201) + (t207))));
  fg168_store(t194, t195, t208);
  t209 = v36;
  t210 = ((int64_t)(((int64_t)(v2))));
  v36 = ((int64_t)((uint64_t)(t209) + (uint64_t)(t210)));
  }
  goto label13;
label14:;
  goto gg_done;
  }
  }
  t211 = ((int64_t)(v1));
  t212 = ((int64_t)(((int64_t)UINT64_C(2))));
  t213 = ((t211) >= (t212));
  t214 = t213;
  if (t214) {
  t215 = ((int64_t)(v1));
  t216 = ((int64_t)(((int64_t)UINT64_C(5))));
  t214 = ((t215) <= (t216));
  }
  t217 = t214;
  if (t217) {
  {
  fg168_lc();
  }
  }
  fg168_sync();
  v42 = ((uint64_t)(v11));
label16:;
  t218 = ((uint64_t)(v42));
  t219 = ((int64_t)(v0));
  t220 = ((int64_t)(((int64_t)UINT64_C(3))));
  t221 = ((int64_t)(((int64_t)UINT64_C(4))));
  t222 = fg168_di(t219, t220, t221);
  t223 = ((uint64_t)(t222));
  if (!(((t218) < (t223)))) goto label17;
  {
  v43 = ((uint64_t)(v10));
label19:;
  t224 = ((uint64_t)(v43));
  t225 = ((int64_t)(v0));
  t226 = ((int64_t)(((int64_t)UINT64_C(3))));
  t227 = ((int64_t)(((int64_t)UINT64_C(3))));
  t228 = fg168_di(t225, t226, t227);
  t229 = ((uint64_t)(t228));
  if (!(((t224) < (t229)))) goto label20;
  {
  t230 = ((uint64_t)(v15));
  t231 = ((int64_t)(v0));
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((uint64_t)(v9));
  t234 = ((uint64_t)(v43));
  t235 = ((uint64_t)(v42));
  t236 = gg_function_12(t231, t232, t233, t234, t235, base + 0);
  t237 = ((uint64_t)(t236));
  t238 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t239 = ((uint64_t)(((uint64_t)((uint64_t)(t237) * (uint64_t)(t238)))));
  v44 = ((uint64_t)(((uint64_t)((uint64_t)(t230) + (uint64_t)(t239)))));
  t240 = ((uint64_t)(v16));
  t241 = ((int64_t)(v0));
  t242 = ((int64_t)(((int64_t)UINT64_C(1))));
  t243 = ((uint64_t)(v9));
  t244 = ((uint64_t)(v43));
  t245 = ((uint64_t)(v42));
  t246 = gg_function_12(t241, t242, t243, t244, t245, base + 0);
  t247 = ((uint64_t)(t246));
  t248 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t249 = ((uint64_t)(((uint64_t)((uint64_t)(t247) * (uint64_t)(t248)))));
  v45 = ((uint64_t)(((uint64_t)((uint64_t)(t240) + (uint64_t)(t249)))));
  t250 = ((int64_t)(v1));
  t251 = ((int64_t)(((int64_t)UINT64_C(2))));
  t252 = ((t250) >= (t251));
  t253 = t252;
  if (t253) {
  t254 = ((int64_t)(v1));
  t255 = ((int64_t)(((int64_t)UINT64_C(5))));
  t253 = ((t254) <= (t255));
  }
  t256 = t253;
  if (t256) {
  {
  v46 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v47 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t257 = v5;
  if (t257) {
  {
  t258 = ((int64_t)(v0));
  t259 = ((int64_t)(((int64_t)UINT64_C(0))));
  t260 = ((int64_t)(((int64_t)UINT64_C(2))));
  t261 = fg168_di(t258, t259, t260);
  t262 = ((uint64_t)(t261));
  t263 = ((int64_t)(v0));
  t264 = ((int64_t)(((int64_t)UINT64_C(2))));
  t265 = ((uint64_t)(v9));
  t266 = ((uint64_t)(v43));
  t267 = ((uint64_t)(v42));
  t268 = gg_function_15(t263, t264, t265, t266, t267, base + 0);
  t269 = ((uint64_t)(t268));
  t270 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t271 = ((uint64_t)(((uint64_t)((uint64_t)(t269) * (uint64_t)(t270)))));
  v46 = ((uint64_t)(((uint64_t)((uint64_t)(t262) + (uint64_t)(t271)))));
  }
  }
  t272 = v6;
  if (t272) {
  {
  t273 = ((int64_t)(v0));
  t274 = ((int64_t)(((int64_t)UINT64_C(0))));
  t275 = ((int64_t)(((int64_t)UINT64_C(3))));
  t276 = fg168_di(t273, t274, t275);
  t277 = ((uint64_t)(t276));
  t278 = ((int64_t)(v0));
  t279 = ((int64_t)(((int64_t)UINT64_C(3))));
  t280 = ((uint64_t)(v9));
  t281 = ((uint64_t)(v43));
  t282 = ((uint64_t)(v42));
  t283 = gg_function_15(t278, t279, t280, t281, t282, base + 0);
  t284 = ((uint64_t)(t283));
  t285 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t286 = ((uint64_t)(((uint64_t)((uint64_t)(t284) * (uint64_t)(t285)))));
  v47 = ((uint64_t)(((uint64_t)((uint64_t)(t277) + (uint64_t)(t286)))));
  }
  }
  t287 = ((uint64_t)(v44));
  t288 = ((uint64_t)(v45));
  t289 = ((uint64_t)(v14));
  t290 = ((uint64_t)(v46));
  t291 = ((uint64_t)(v47));
  t292 = ((int32_t)(v17));
  t293 = ((int32_t)(v8));
  t294 = ((int32_t)(v2));
  t295 = ((int32_t)(v3));
  t296 = v4;
  t297 = v5;
  t298 = v6;
  t299 = v7;
  t300 = ((int64_t)(v0));
  t301 = ((int64_t)(((int64_t)UINT64_C(1))));
  t302 = ((int64_t)(((int64_t)UINT64_C(8))));
  t303 = fg168_di(t300, t301, t302);
  t304 = ((int32_t)(((int32_t)(t303))));
  t305 = ((int64_t)(v0));
  t306 = ((int64_t)(((int64_t)UINT64_C(1))));
  t307 = ((int64_t)(((int64_t)UINT64_C(12))));
  t308 = fg168_di(t305, t306, t307);
  t309 = ((int32_t)(((int32_t)(t308))));
  t310 = ((float)(v18));
  t311 = ((int64_t)(v0));
  t312 = ((int64_t)(((int64_t)UINT64_C(1))));
  t313 = fg168_df(t311, t312);
  t314 = ((float)(t313));
  gg_function_19(t287, t288, t289, t290, t291, t292, t293, t294, t295, t296, t297, t298, t299, t304, t309, t310, t314, base + 0);
  }
  } else {
  {
  memset(&(v48), 0, sizeof(v48));
  (v48).f0 = ((float)(gg_double_bits(UINT64_C(0))));
  (v48).f1 = ((float)(gg_double_bits(UINT64_C(0))));
  v49 = ((int64_t)(((int64_t)(v8))));
label22:;
  t315 = ((int64_t)(v49));
  t316 = ((int64_t)(((int64_t)(v17))));
  if (!(((t315) < (t316)))) goto label23;
  {
  t317 = ((uint64_t)(v44));
  t318 = ((uint64_t)(((uint64_t)(v49))));
  t319 = fg168_load(t317, t318);
  v50 = ((float)(t319));
  t320 = ((int64_t)(v1));
  t321 = ((int64_t)(((int64_t)UINT64_C(0))));
  t322 = ((t320) == (t321));
  if (t322) {
  {
  t323 = (v48).f0;
  t324 = ((float)(v50));
  (v48).f0 = ((t323) + (t324));
  }
  }
  t325 = (v48).f1;
  t326 = ((float)(v50));
  t327 = ((float)(v50));
  t328 = ((float)(((t326) * (t327))));
  (v48).f1 = ((t325) + (t328));
  t329 = v49;
  t330 = ((int64_t)(((int64_t)(v2))));
  v49 = ((int64_t)((uint64_t)(t329) + (uint64_t)(t330)));
  }
  goto label22;
label23:;
  v51 = ((float)(gg_double_bits(UINT64_C(0))));
  v52 = ((float)(gg_double_bits(UINT64_C(0))));
  t331 = ((int64_t)(v1));
  t332 = ((int64_t)(((int64_t)UINT64_C(0))));
  t333 = ((t331) == (t332));
  if (t333) {
  {
  t334 = v48;
  t335 = ((uint64_t)(v14));
  t336 = ((int32_t)(v8));
  t337 = ((int32_t)(v2));
  v48 = gg_function_13(t334, t335, t336, t337, base + 0);
  t338 = ((float)((v48).f0));
  t339 = ((float)(((float)(v17))));
  v51 = ((float)(((t338) / (t339))));
  t340 = ((float)((v48).f1));
  t341 = ((float)(((float)(v17))));
  t342 = ((float)(((t340) / (t341))));
  t343 = ((float)(v51));
  t344 = ((float)(v51));
  t345 = ((float)(((t343) * (t344))));
  v53 = ((float)(((t342) - (t345))));
  t346 = ((float)(v53));
  t347 = ((float)(v18));
  t348 = ((float)(((t346) + (t347))));
  t349 = fg168_rsqrt(t348);
  v52 = ((float)(t349));
  }
  } else {
  {
  t350 = ((float)((v48).f1));
  t351 = ((uint64_t)(v14));
  t352 = ((int32_t)(v8));
  t353 = ((int32_t)(v2));
  (v48).f1 = gg_function_6(t350, t351, t352, t353, base + 0);
  t354 = ((float)((v48).f1));
  t355 = ((float)(v18));
  t356 = ((float)(v18));
  t357 = ((float)(((t355) * (t356))));
  t358 = fg168_max(t354, t357);
  t359 = ((float)(t358));
  t360 = fg168_rsqrt(t359);
  v52 = ((float)(t360));
  }
  }
  v49 = ((int64_t)(((int64_t)(v8))));
label25:;
  t361 = ((int64_t)(v49));
  t362 = ((int64_t)(((int64_t)(v17))));
  if (!(((t361) < (t362)))) goto label26;
  {
  t363 = ((uint64_t)(v45));
  t364 = ((uint64_t)(((uint64_t)(v49))));
  t366 = ((int64_t)(v1));
  t367 = ((int64_t)(((int64_t)UINT64_C(0))));
  t368 = ((t366) == (t367));
  if (t368) {
  {
  t369 = ((uint64_t)(v44));
  t370 = ((uint64_t)(((uint64_t)(v49))));
  t371 = fg168_load(t369, t370);
  t372 = ((float)(t371));
  t373 = ((float)(v51));
  t374 = ((float)(((t372) - (t373))));
  t375 = ((float)(v52));
  t365 = ((float)(((t374) * (t375))));
  }
  } else {
  {
  t376 = ((float)(v52));
  t377 = ((uint64_t)(v44));
  t378 = ((uint64_t)(((uint64_t)(v49))));
  t379 = fg168_load(t377, t378);
  t380 = ((float)(t379));
  t365 = ((float)(((t376) * (t380))));
  }
  }
  t381 = ((float)(t365));
  fg168_store(t363, t364, t381);
  t382 = v49;
  t383 = ((int64_t)(((int64_t)(v2))));
  v49 = ((int64_t)((uint64_t)(t382) + (uint64_t)(t383)));
  }
  goto label25;
label26:;
  t384 = ((int32_t)(v2));
  t385 = ((int32_t)(((int64_t)UINT64_C(32))));
  t386 = ((t384) > (t385));
  if (t386) {
  {
  fg168_barrier();
  }
  }
  }
  }
  t387 = v43;
  t388 = ((uint64_t)(v12));
  v43 = ((uint64_t)((uint64_t)(t387) + (uint64_t)(t388)));
  }
  goto label19;
label20:;
  t389 = v42;
  t390 = ((uint64_t)(v13));
  v42 = ((uint64_t)((uint64_t)(t389) + (uint64_t)(t390)));
  }
  goto label16;
label17:;
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  int64_t t0;
  int64_t t1;
  int32_t t2;
  int32_t t3;
  uint8_t t4;
  uint8_t t5;
  uint8_t t6;
  uint8_t t7;
  int32_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int32_t)(((int64_t)UINT64_C(32))));
  t3 = ((int32_t)(((int64_t)UINT64_C(1))));
  t4 = ((uint8_t)UINT64_C(0));
  t5 = ((uint8_t)UINT64_C(0));
  t6 = ((uint8_t)UINT64_C(0));
  t7 = ((uint8_t)UINT64_C(0));
  t8 = ((int32_t)(((int64_t)UINT64_C(0))));
  t9 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t10 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t13 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t14 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  gg_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, base + 0);
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
