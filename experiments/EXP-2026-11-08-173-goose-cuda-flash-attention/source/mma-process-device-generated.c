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
  uint32_t f0;
  uint32_t f1;
  uint32_t f2;
  uint32_t f3;
};
#pragma pack(pop)
typedef struct gg_type_10 gg_type_10;
#pragma pack(push, 1)
struct gg_type_10 {
  float f4;
  float f5;
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
static uint8_t gg_eq_7(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, uint32_t value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, gg_type_10 value);
static uint8_t gg_eq_11(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, uint8_t value);
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
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_7(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
if(!gg_eq_7(a.f0,b.f0))return 0;
if(!gg_eq_7(a.f1,b.f1))return 0;
if(!gg_eq_7(a.f2,b.f2))return 0;
if(!gg_eq_7(a.f3,b.f3))return 0;
return 1;
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
gg_append(out,"\125\064\040\173\040",5,1);
gg_render_7(out, value.f0);
gg_append(out,", ",2,1);
gg_render_7(out, value.f1);
gg_append(out,", ",2,1);
gg_render_7(out, value.f2);
gg_append(out,", ",2,1);
gg_render_7(out, value.f3);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
if(!gg_eq_3(a.f4,b.f4))return 0;
if(!gg_eq_3(a.f5,b.f5))return 0;
return 1;
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
gg_append(out,"\106\062\040\173\040",5,1);
gg_render_3(out, value.f4);
gg_append(out,", ",2,1);
gg_render_3(out, value.f5);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_11(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, float p7, float p8, int64_t p9, int32_t p10, int32_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int32_t p17, int32_t p18, int32_t p19, int32_t p20, int32_t p21, int32_t p22, uint8_t p23, uint8_t p24, int32_t p25, int32_t p26, int32_t p27, int32_t p28, uint64_t p29, uint64_t p30, uint64_t p31, uint64_t p32, uint64_t p33, int64_t base);
static int32_t gg_function_14(int32_t p0, int32_t p1, int64_t base);
static float gg_function_15(float p0, float p1, int64_t base);
static int32_t gg_function_18(int32_t p0, int32_t p1, int64_t base);
static int32_t gg_function_21(int32_t p0, int32_t p1, int64_t base);
static int32_t gg_function_21(int32_t p0, int32_t p1, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t v0;
  int32_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int32_t)(v0));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((int32_t)(((int32_t)gg_div(t1, t2))));
  t4 = ((int32_t)(((int64_t)UINT64_C(4))));
  t5 = ((int32_t)(((int32_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((int32_t)(v1));
  t7 = ((int32_t)(((int64_t)UINT64_C(4))));
  t8 = ((int32_t)(((int32_t)gg_mod(t6, t7))));
  t0 = ((int32_t)(((int32_t)((uint64_t)(t5) + (uint64_t)(t8)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_18(int32_t p0, int32_t p1, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t v0;
  int32_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int32_t)(v0));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((int32_t)(((int32_t)gg_mod(t1, t2))));
  t4 = ((int32_t)(((int64_t)UINT64_C(8))));
  t5 = ((int32_t)(((int32_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((int32_t)(v1));
  t7 = ((int32_t)(((int64_t)UINT64_C(4))));
  t8 = ((int32_t)(((int32_t)gg_div(t6, t7))));
  t0 = ((int32_t)(((int32_t)((uint64_t)(t5) + (uint64_t)(t8)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_15(float p0, float p1, int64_t base) {
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
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((float)(v0));
  }
  } else {
  {
  t0 = ((float)(v1));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_14(int32_t p0, int32_t p1, int64_t base) {
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
  int32_t t10;
  int32_t v0;
  int32_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int32_t)(v0));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((int32_t)(((int32_t)gg_div(t1, t2))));
  t4 = ((int32_t)(((int64_t)UINT64_C(2))));
  t5 = ((int32_t)(((int32_t)gg_mod(t3, t4))));
  t6 = ((int32_t)(((int64_t)UINT64_C(8))));
  t7 = ((int32_t)(((int32_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((int32_t)(v1));
  t9 = ((int32_t)(((int64_t)UINT64_C(4))));
  t10 = ((int32_t)(((int32_t)gg_div(t8, t9))));
  t0 = ((int32_t)(((int32_t)((uint64_t)(t7) + (uint64_t)(t10)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, float p7, float p8, int64_t p9, int32_t p10, int32_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int32_t p17, int32_t p18, int32_t p19, int32_t p20, int32_t p21, int32_t p22, uint8_t p23, uint8_t p24, int32_t p25, int32_t p26, int32_t p27, int32_t p28, uint64_t p29, uint64_t p30, uint64_t p31, uint64_t p32, uint64_t p33, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint64_t t2;
  uint64_t t3;
  float t4;
  float t5;
  float t6;
  uint64_t t7;
  uint64_t t8;
  float t9;
  float t10;
  float t11;
  uint64_t t12;
  uint64_t t13;
  float t14;
  uint64_t t15;
  uint64_t t16;
  float t17;
  int32_t t18;
  int32_t t19;
  uint64_t t20;
  uint64_t t21;
  float t22;
  float t23;
  uint32_t t24;
  uint32_t t25;
  int32_t t26;
  int32_t t27;
  float t28;
  float t29;
  uint32_t t30;
  int32_t t31;
  int32_t t32;
  int32_t t33;
  int32_t t34;
  int32_t t35;
  int32_t t36;
  int32_t t37;
  int32_t t38;
  int32_t t39;
  int32_t t40;
  uint8_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  uint8_t t50;
  int32_t t51;
  int32_t t52;
  int32_t t53;
  int32_t t54;
  int32_t t55;
  int32_t t56;
  int32_t t57;
  int32_t t58;
  int32_t t59;
  int32_t t60;
  uint8_t t61;
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
  uint64_t t74;
  int32_t t75;
  int32_t t76;
  int32_t t77;
  int32_t t78;
  uint64_t t79;
  uint64_t t80;
  int64_t t81;
  int64_t t82;
  uint64_t t83;
  float t84;
  float t85;
  uint64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  float t92;
  float t93;
  uint32_t t94;
  uint32_t t95;
  uint32_t t96;
  uint32_t t97;
  uint32_t t98;
  uint64_t t99;
  int32_t t100;
  int32_t t101;
  int32_t t102;
  int32_t t103;
  uint64_t t104;
  float t105;
  float t106;
  uint32_t t107;
  uint32_t t108;
  int32_t t109;
  int32_t t110;
  int32_t t111;
  int32_t t112;
  int32_t t113;
  int32_t t114;
  int32_t t115;
  int32_t t116;
  int32_t t117;
  int32_t t118;
  int32_t t119;
  int32_t t120;
  int32_t t121;
  int32_t t122;
  int32_t t123;
  int32_t t124;
  int32_t t125;
  int32_t t126;
  int32_t t127;
  int32_t t128;
  int32_t t129;
  int32_t t130;
  int32_t t131;
  int32_t t132;
  int32_t t133;
  int32_t t134;
  uint64_t t135;
  uint64_t t136;
  uint64_t t137;
  uint64_t t138;
  uint64_t t139;
  gg_type_8 t140;
  uint64_t t141;
  int32_t t142;
  int32_t t143;
  int32_t t144;
  int32_t t145;
  uint64_t t146;
  uint32_t t147;
  uint64_t t148;
  int32_t t149;
  int32_t t150;
  int32_t t151;
  int32_t t152;
  int32_t t153;
  int32_t t154;
  uint64_t t155;
  uint32_t t156;
  uint64_t t157;
  int32_t t158;
  int32_t t159;
  int32_t t160;
  int32_t t161;
  int32_t t162;
  int32_t t163;
  uint64_t t164;
  uint32_t t165;
  uint64_t t166;
  int32_t t167;
  int32_t t168;
  int32_t t169;
  int32_t t170;
  int32_t t171;
  int32_t t172;
  uint64_t t173;
  uint32_t t174;
  int32_t t175;
  int32_t t176;
  uint64_t t177;
  uint64_t t178;
  uint64_t t179;
  uint64_t t180;
  int64_t t181;
  int32_t t182;
  int32_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int32_t t189;
  int32_t t190;
  int32_t t191;
  uint64_t t192;
  uint64_t t193;
  int64_t t194;
  int32_t t195;
  int32_t t196;
  int64_t t197;
  int32_t t198;
  int32_t t199;
  int32_t t200;
  int32_t t201;
  int32_t t202;
  int32_t t203;
  uint64_t t204;
  uint64_t t205;
  uint64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  float t210;
  int32_t t211;
  int32_t t212;
  int64_t t213;
  int32_t t214;
  uint64_t t215;
  uint64_t t216;
  uint64_t t217;
  uint64_t t218;
  uint64_t t219;
  int32_t t220;
  int32_t t221;
  int32_t t222;
  uint8_t t223;
  int32_t t224;
  int32_t t225;
  uint64_t t226;
  uint64_t t227;
  uint64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  float t232;
  int32_t t233;
  int32_t t234;
  int64_t t235;
  int32_t t236;
  uint64_t t237;
  uint64_t t238;
  uint64_t t239;
  uint64_t t240;
  uint64_t t241;
  int32_t t242;
  int32_t t243;
  int32_t t244;
  uint8_t t245;
  int32_t t246;
  int32_t t247;
  uint64_t t248;
  uint64_t t249;
  float t250;
  int32_t t251;
  int32_t t252;
  float t253;
  float t254;
  int32_t t255;
  float t256;
  float t257;
  int32_t t258;
  int64_t t259;
  uint64_t t260;
  uint64_t t261;
  float t262;
  int32_t t263;
  int32_t t264;
  uint8_t t265;
  uint8_t t266;
  uint8_t t267;
  int32_t t268;
  int32_t t269;
  uint8_t t270;
  uint8_t t271;
  int32_t t272;
  int32_t t273;
  int32_t t274;
  int32_t t275;
  uint8_t t276;
  uint8_t t277;
  uint64_t t278;
  uint64_t t279;
  uint8_t t280;
  int32_t t281;
  int32_t t282;
  int32_t t283;
  int32_t t284;
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
  int32_t t295;
  int32_t t296;
  int32_t t297;
  int32_t t298;
  uint64_t t299;
  int32_t t300;
  int32_t t301;
  uint64_t t302;
  float t303;
  uint64_t t304;
  int32_t t305;
  int32_t t306;
  uint64_t t307;
  float t308;
  uint64_t t309;
  uint64_t t310;
  float t311;
  float t312;
  float t313;
  uint64_t t314;
  uint64_t t315;
  float t316;
  float t317;
  float t318;
  float t319;
  float t320;
  uint64_t t321;
  uint64_t t322;
  float t323;
  float t324;
  double t325;
  float t326;
  uint8_t t327;
  uint64_t t328;
  uint64_t t329;
  float t330;
  uint64_t t331;
  uint64_t t332;
  float t333;
  float t334;
  float t335;
  float t336;
  float t337;
  float t338;
  float t339;
  float t340;
  float t341;
  uint64_t t342;
  uint64_t t343;
  float t344;
  float t345;
  float t346;
  uint64_t t347;
  uint64_t t348;
  float t349;
  float t350;
  float t351;
  float t352;
  float t353;
  uint64_t t354;
  uint64_t t355;
  float t356;
  float t357;
  double t358;
  float t359;
  uint8_t t360;
  uint64_t t361;
  uint64_t t362;
  float t363;
  uint64_t t364;
  uint64_t t365;
  float t366;
  float t367;
  float t368;
  float t369;
  float t370;
  float t371;
  float t372;
  float t373;
  float t374;
  float t375;
  float t376;
  uint32_t t377;
  float t378;
  float t379;
  uint32_t t380;
  int32_t t381;
  int32_t t382;
  uint64_t t383;
  int32_t t384;
  int32_t t385;
  int32_t t386;
  int32_t t387;
  uint64_t t388;
  uint64_t t389;
  int32_t t390;
  int32_t t391;
  int32_t t392;
  int32_t t393;
  uint64_t t394;
  uint32_t t395;
  uint32_t t396;
  uint32_t t397;
  uint32_t t398;
  uint32_t t399;
  uint64_t t400;
  int32_t t401;
  int32_t t402;
  int32_t t403;
  int32_t t404;
  uint64_t t405;
  uint64_t t406;
  int32_t t407;
  int32_t t408;
  int32_t t409;
  int32_t t410;
  uint64_t t411;
  uint32_t t412;
  uint32_t t413;
  uint32_t t414;
  uint32_t t415;
  uint32_t t416;
  uint64_t t417;
  int32_t t418;
  int32_t t419;
  int32_t t420;
  int32_t t421;
  uint64_t t422;
  uint64_t t423;
  int32_t t424;
  int32_t t425;
  int32_t t426;
  int32_t t427;
  uint64_t t428;
  uint32_t t429;
  uint32_t t430;
  uint32_t t431;
  uint32_t t432;
  uint32_t t433;
  uint64_t t434;
  int32_t t435;
  int32_t t436;
  int32_t t437;
  int32_t t438;
  uint64_t t439;
  uint64_t t440;
  int32_t t441;
  int32_t t442;
  int32_t t443;
  int32_t t444;
  uint64_t t445;
  uint32_t t446;
  uint32_t t447;
  uint32_t t448;
  uint32_t t449;
  uint32_t t450;
  int32_t t451;
  int32_t t452;
  int32_t t453;
  int32_t t454;
  int32_t t455;
  int32_t t456;
  int32_t t457;
  int32_t t458;
  int32_t t459;
  int32_t t460;
  int32_t t461;
  uint64_t t462;
  int32_t t463;
  int32_t t464;
  uint64_t t465;
  float t466;
  uint64_t t467;
  int32_t t468;
  int32_t t469;
  uint64_t t470;
  float t471;
  int32_t t472;
  int32_t t473;
  int32_t t474;
  int32_t t475;
  uint8_t t476;
  uint8_t t477;
  uint8_t t478;
  uint8_t t479;
  uint8_t t480;
  uint8_t t481;
  int32_t t482;
  int32_t t483;
  uint8_t t484;
  uint8_t t485;
  uint8_t t486;
  uint64_t t487;
  int32_t t488;
  int32_t t489;
  int32_t t490;
  int32_t t491;
  uint64_t t492;
  float t493;
  uint64_t t494;
  int32_t t495;
  int32_t t496;
  int32_t t497;
  int32_t t498;
  uint64_t t499;
  float t500;
  int32_t t501;
  int32_t t502;
  uint8_t t503;
  uint8_t t504;
  uint8_t t505;
  uint8_t t506;
  int64_t t507;
  int64_t t508;
  int64_t t509;
  int64_t t510;
  int64_t t511;
  int64_t t512;
  uint64_t t513;
  uint64_t t514;
  float t515;
  uint64_t t516;
  int64_t t517;
  int64_t t518;
  uint64_t t519;
  float t520;
  uint8_t t521;
  uint8_t t522;
  uint8_t t523;
  int64_t t524;
  int64_t t525;
  int64_t t526;
  int64_t t527;
  int64_t t528;
  int64_t t529;
  int64_t t530;
  int64_t t531;
  uint64_t t532;
  uint64_t t533;
  float t534;
  uint64_t t535;
  int64_t t536;
  int64_t t537;
  uint64_t t538;
  float t539;
  int32_t t540;
  int32_t t541;
  uint8_t t542;
  int32_t t543;
  int32_t t544;
  int32_t t545;
  int32_t t546;
  int32_t t547;
  int32_t t548;
  int32_t t549;
  int32_t t550;
  int32_t t551;
  int32_t t552;
  int32_t t553;
  int32_t t554;
  uint8_t t555;
  uint64_t t556;
  uint64_t t557;
  float t558;
  uint64_t t559;
  int32_t t560;
  int32_t t561;
  uint64_t t562;
  float t563;
  int32_t t564;
  int32_t t565;
  int32_t t566;
  int32_t t567;
  int32_t t568;
  int32_t t569;
  int32_t t570;
  int32_t t571;
  uint8_t t572;
  float t573;
  float t574;
  int32_t t575;
  float t576;
  float t577;
  int32_t t578;
  int64_t t579;
  float t580;
  float t581;
  float t582;
  float t583;
  float t584;
  float t585;
  int32_t t586;
  int32_t t587;
  int32_t t588;
  int32_t t589;
  int32_t t590;
  int32_t t591;
  int32_t t592;
  int32_t t593;
  uint8_t t594;
  float t595;
  float t596;
  int32_t t597;
  float t598;
  float t599;
  int32_t t600;
  int64_t t601;
  int32_t t602;
  int32_t t603;
  int32_t t604;
  int32_t t605;
  uint8_t t606;
  uint64_t t607;
  uint64_t t608;
  float t609;
  uint64_t t610;
  int32_t t611;
  int32_t t612;
  uint64_t t613;
  float t614;
  uint8_t t615;
  uint8_t t616;
  int32_t t617;
  int32_t t618;
  uint8_t t619;
  int64_t t620;
  int64_t t621;
  int64_t t622;
  int32_t t623;
  int32_t t624;
  int32_t t625;
  int32_t t626;
  int32_t t627;
  int32_t t628;
  int64_t t629;
  int64_t t630;
  int64_t t631;
  uint64_t t632;
  uint64_t t633;
  float t634;
  uint64_t t635;
  int64_t t636;
  int64_t t637;
  uint64_t t638;
  float t639;
  uint8_t t640;
  uint8_t t641;
  int32_t t642;
  int32_t t643;
  uint8_t t644;
  int64_t t645;
  int64_t t646;
  int64_t t647;
  int64_t t648;
  int64_t t649;
  int32_t t650;
  int32_t t651;
  int32_t t652;
  int32_t t653;
  int32_t t654;
  int32_t t655;
  int64_t t656;
  int64_t t657;
  int64_t t658;
  uint64_t t659;
  uint64_t t660;
  float t661;
  uint64_t t662;
  int64_t t663;
  int64_t t664;
  uint64_t t665;
  float t666;
  int32_t t667;
  int32_t t668;
  int32_t t669;
  int32_t t670;
  int32_t t671;
  int32_t t672;
  uint64_t t673;
  int32_t t674;
  int32_t t675;
  int32_t t676;
  int32_t t677;
  int32_t t678;
  int32_t t679;
  int32_t t680;
  int32_t t681;
  int32_t t682;
  int32_t t683;
  int32_t t684;
  int32_t t685;
  int32_t t686;
  int32_t t687;
  uint64_t t688;
  uint64_t t689;
  int32_t t690;
  int32_t t691;
  int32_t t692;
  int32_t t693;
  int32_t t694;
  int32_t t695;
  uint64_t t696;
  uint32_t t697;
  uint32_t t698;
  int32_t t699;
  int32_t t700;
  int32_t t701;
  int32_t t702;
  int32_t t703;
  int32_t t704;
  uint8_t t705;
  uint8_t t706;
  int32_t t707;
  int32_t t708;
  int32_t t709;
  int32_t t710;
  uint8_t t711;
  int64_t t712;
  int64_t t713;
  int64_t t714;
  int64_t t715;
  int64_t t716;
  int64_t t717;
  int64_t t718;
  int64_t t719;
  int64_t t720;
  int64_t t721;
  int64_t t722;
  int64_t t723;
  int32_t t724;
  int32_t t725;
  int32_t t726;
  int32_t t727;
  int32_t t728;
  int32_t t729;
  int32_t t730;
  int32_t t731;
  int32_t t732;
  int32_t t733;
  int32_t t734;
  int32_t t735;
  int32_t t736;
  int32_t t737;
  int32_t t738;
  int32_t t739;
  int32_t t740;
  int32_t t741;
  int32_t t742;
  int32_t t743;
  uint8_t t744;
  uint8_t t745;
  uint8_t t746;
  int32_t t747;
  int32_t t748;
  uint8_t t749;
  uint8_t t750;
  int64_t t751;
  int64_t t752;
  int64_t t753;
  int64_t t754;
  int64_t t755;
  int64_t t756;
  uint8_t t757;
  uint8_t t758;
  int32_t t759;
  int32_t t760;
  int32_t t761;
  int32_t t762;
  int32_t t763;
  int32_t t764;
  uint8_t t765;
  uint8_t t766;
  int32_t t767;
  int32_t t768;
  int32_t t769;
  int32_t t770;
  int32_t t771;
  int32_t t772;
  int32_t t773;
  int32_t t774;
  int32_t t775;
  int32_t t776;
  int32_t t777;
  int32_t t778;
  uint8_t t779;
  uint64_t t780;
  int32_t t781;
  int32_t t782;
  int32_t t783;
  int32_t t784;
  int32_t t785;
  int32_t t786;
  uint64_t t787;
  float t788;
  uint64_t t789;
  int32_t t790;
  int32_t t791;
  int32_t t792;
  int32_t t793;
  int32_t t794;
  int32_t t795;
  int32_t t796;
  int32_t t797;
  uint64_t t798;
  uint32_t t799;
  float t800;
  uint32_t t801;
  float t802;
  float t803;
  float t804;
  float t805;
  float t806;
  uint32_t t807;
  float t808;
  float t809;
  float t810;
  float t811;
  int32_t t812;
  int32_t t813;
  uint8_t t814;
  uint8_t t815;
  uint8_t t816;
  uint8_t t817;
  uint8_t t818;
  uint64_t t819;
  int32_t t820;
  int32_t t821;
  uint64_t t822;
  float t823;
  float t824;
  float t825;
  float t826;
  float t827;
  uint8_t t828;
  int64_t t829;
  int64_t t830;
  int64_t t831;
  int64_t t832;
  int64_t t833;
  int64_t t834;
  int64_t t835;
  int64_t t836;
  uint64_t t837;
  uint64_t t838;
  float t839;
  uint64_t t840;
  int64_t t841;
  int64_t t842;
  uint64_t t843;
  float t844;
  int64_t t845;
  int64_t t846;
  int64_t t847;
  int64_t t848;
  int64_t t849;
  int64_t t850;
  int64_t t851;
  int64_t t852;
  int64_t t853;
  int64_t t854;
  int64_t t855;
  int64_t t856;
  int64_t t857;
  int64_t t858;
  uint64_t t859;
  uint64_t t860;
  float t861;
  uint64_t t862;
  int64_t t863;
  int64_t t864;
  uint64_t t865;
  float t866;
  int32_t t867;
  int32_t t868;
  int32_t t869;
  int32_t t870;
  int32_t t871;
  int32_t t872;
  int32_t t873;
  int32_t t874;
  uint8_t t875;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint64_t v5;
  uint64_t v6;
  float v7;
  float v8;
  int64_t v9;
  int32_t v10;
  int32_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int32_t v17;
  int32_t v18;
  int32_t v19;
  int32_t v20;
  int32_t v21;
  int32_t v22;
  uint8_t v23;
  uint8_t v24;
  int32_t v25;
  int32_t v26;
  int32_t v27;
  int32_t v28;
  uint64_t v29;
  uint64_t v30;
  uint64_t v31;
  uint64_t v32;
  uint64_t v33;
  int32_t v34;
  float v35;
  int32_t v36;
  uint32_t v37;
  int32_t v38;
  int32_t v39;
  int32_t v40;
  int32_t v41;
  uint8_t v42;
  int32_t v43;
  int32_t v44;
  int64_t v45;
  int32_t v46;
  int32_t v47;
  int32_t v48;
  gg_type_8 v49;
  int32_t v50;
  int32_t v51;
  float v52;
  int32_t v53;
  float v54;
  float v55;
  int32_t v56;
  int32_t v57;
  float v58;
  float v59;
  float v60;
  float v61;
  float v62;
  float v63;
  uint32_t v64;
  uint32_t v65;
  int32_t v66;
  int32_t v67;
  float v68;
  float v69;
  uint8_t v70;
  int64_t v71;
  int64_t v72;
  float v73;
  float v74;
  float v75;
  int32_t v76;
  int32_t v77;
  float v78;
  float v79;
  int32_t v80;
  int64_t v81;
  int64_t v82;
  int32_t v83;
  int32_t v84;
  int32_t v85;
  int64_t v86;
  int32_t v87;
  int32_t v88;
  int32_t v89;
  int32_t v90;
  int32_t v91;
  uint8_t v92;
  int32_t v93;
  int32_t v94;
  int32_t v95;
  float v96;
  float v97;
  int32_t v98;
  float v99;
  uint32_t v100;
  float v101;
  int64_t v102;
  int64_t v103;
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
  {
  t0 = ((int32_t)(v21));
  t1 = ((int32_t)(((int64_t)UINT64_C(8))));
  v34 = ((int32_t)(((int32_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  v35 = ((float)(gg_double_bits(UINT64_C(5179139570939199488))));
  t2 = ((uint64_t)(v31));
  t3 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t4 = ((float)(gg_double_bits(UINT64_C(0))));
  t5 = ((float)(v35));
  t6 = ((float)(((t4) - (t5))));
  fg175p_gstore(t2, t3, t6);
  t7 = ((uint64_t)(v31));
  t8 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t9 = ((float)(gg_double_bits(UINT64_C(0))));
  t10 = ((float)(v35));
  t11 = ((float)(((t9) - (t10))));
  fg175p_gstore(t7, t8, t11);
  t12 = ((uint64_t)(v32));
  t13 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t14 = ((float)(gg_double_bits(UINT64_C(0))));
  fg175p_gstore(t12, t13, t14);
  t15 = ((uint64_t)(v32));
  t16 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t17 = ((float)(gg_double_bits(UINT64_C(0))));
  fg175p_gstore(t15, t16, t17);
  v36 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t18 = ((int32_t)(v36));
  t19 = ((int32_t)(((int64_t)UINT64_C(64))));
  if (!(((t18) < (t19)))) goto label2;
  {
  t20 = ((uint64_t)(v30));
  t21 = ((uint64_t)(((uint64_t)(v36))));
  t22 = ((float)(gg_double_bits(UINT64_C(0))));
  t23 = ((float)(gg_double_bits(UINT64_C(0))));
  t24 = fg175p_h2pack(t22, t23);
  t25 = ((uint32_t)(t24));
  fg175p_stu32(t20, t21, t25);
  t26 = v36;
  t27 = ((int32_t)(((int64_t)UINT64_C(1))));
  v36 = ((int32_t)((uint64_t)(t26) + (uint64_t)(t27)));
  }
  goto label1;
label2:;
  t28 = ((float)(v7));
  t29 = ((float)(v7));
  t30 = fg175p_h2pack(t28, t29);
  v37 = ((uint32_t)(t30));
  v38 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t31 = ((int32_t)(v38));
  t32 = ((int32_t)(v34));
  if (!(((t31) < (t32)))) goto label5;
  {
  t33 = ((int32_t)(v38));
  t34 = ((int32_t)(v28));
  v39 = ((int32_t)(((int32_t)((uint64_t)(t33) + (uint64_t)(t34)))));
  t35 = ((int32_t)(v39));
  t36 = ((int32_t)(((int64_t)UINT64_C(8))));
  v40 = ((int32_t)(((int32_t)gg_div(t35, t36))));
  t37 = ((int32_t)(v39));
  t38 = ((int32_t)(((int64_t)UINT64_C(8))));
  v41 = ((int32_t)(((int32_t)gg_mod(t37, t38))));
  t39 = ((int32_t)(v21));
  t40 = ((int32_t)(((int64_t)UINT64_C(1))));
  t41 = ((t39) == (t40));
  t42 = t41;
  if (!t42) {
  t43 = ((int64_t)(((int64_t)(v17))));
  t44 = ((int64_t)(((int64_t)(v21))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((int64_t)(((int64_t)(v40))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  t48 = ((int64_t)(v9));
  t42 = ((t47) < (t48));
  }
  t49 = t42;
  t50 = t49;
  if (t50) {
  t51 = ((int32_t)(v18));
  t52 = ((int32_t)(((int64_t)UINT64_C(8))));
  t53 = ((int32_t)(((int32_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t54 = ((int32_t)(v41));
  t55 = ((int32_t)(((int32_t)((uint64_t)(t53) + (uint64_t)(t54)))));
  t56 = ((int32_t)(v11));
  t50 = ((t55) < (t56));
  }
  v42 = t50;
  v43 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t57 = ((int32_t)(v43));
  t58 = ((int32_t)(((int64_t)UINT64_C(128))));
  if (!(((t57) < (t58)))) goto label8;
  {
  t59 = ((int32_t)(v43));
  t60 = ((int32_t)(v27));
  v44 = ((int32_t)(((int32_t)((uint64_t)(t59) + (uint64_t)(t60)))));
  t61 = v42;
  if (t61) {
  {
  t62 = ((int64_t)(((int64_t)(v17))));
  t63 = ((int64_t)(((int64_t)(v21))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)(v40))));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
  t67 = ((int64_t)(v12));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  t69 = ((int64_t)(((int64_t)(v41))));
  t70 = ((int64_t)(v13));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)(v44))));
  v45 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  t74 = ((uint64_t)(v33));
  t75 = ((int32_t)(v39));
  t76 = ((int32_t)(((int64_t)UINT64_C(132))));
  t77 = ((int32_t)(((int32_t)((uint64_t)(t75) * (uint64_t)(t76)))));
  t78 = ((int32_t)(v44));
  t79 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t77) + (uint64_t)(t78)))))));
  t80 = ((uint64_t)(v0));
  t81 = ((int64_t)(v45));
  t82 = ((int64_t)(((int64_t)UINT64_C(2))));
  t83 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t82)))))));
  t84 = fg175p_gload(t80, t83);
  t85 = ((float)(t84));
  t86 = ((uint64_t)(v0));
  t87 = ((int64_t)(v45));
  t88 = ((int64_t)(((int64_t)UINT64_C(2))));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t87) * (uint64_t)(t88)))));
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  t91 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t89) + (uint64_t)(t90)))))));
  t92 = fg175p_gload(t86, t91);
  t93 = ((float)(t92));
  t94 = fg175p_h2pack(t85, t93);
  t95 = ((uint32_t)(t94));
  t96 = ((uint32_t)(v37));
  t97 = fg175p_h2mul(t95, t96);
  t98 = ((uint32_t)(t97));
  fg175p_smst(t74, t79, t98);
  }
  } else {
  {
  t99 = ((uint64_t)(v33));
  t100 = ((int32_t)(v39));
  t101 = ((int32_t)(((int64_t)UINT64_C(132))));
  t102 = ((int32_t)(((int32_t)((uint64_t)(t100) * (uint64_t)(t101)))));
  t103 = ((int32_t)(v44));
  t104 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t102) + (uint64_t)(t103)))))));
  t105 = ((float)(gg_double_bits(UINT64_C(0))));
  t106 = ((float)(gg_double_bits(UINT64_C(0))));
  t107 = fg175p_h2pack(t105, t106);
  t108 = ((uint32_t)(t107));
  fg175p_smst(t99, t104, t108);
  }
  }
  t109 = v43;
  t110 = ((int32_t)(((int64_t)UINT64_C(32))));
  v43 = ((int32_t)((uint64_t)(t109) + (uint64_t)(t110)));
  }
  goto label7;
label8:;
  t111 = v38;
  t112 = ((int32_t)(((int64_t)UINT64_C(4))));
  v38 = ((int32_t)((uint64_t)(t111) + (uint64_t)(t112)));
  }
  goto label4;
label5:;
  fg175p_barrier();
  t113 = ((int32_t)(v28));
  t114 = ((int32_t)(v22));
  t115 = ((int32_t)(((int32_t)gg_div(t113, t114))));
  t116 = ((int32_t)(((int64_t)UINT64_C(16))));
  v46 = ((int32_t)(((int32_t)((uint64_t)(t115) * (uint64_t)(t116)))));
  v47 = ((int32_t)(((int64_t)UINT64_C(0))));
label10:;
  t117 = ((int32_t)(v47));
  t118 = ((int32_t)(((int64_t)UINT64_C(128))));
  if (!(((t117) < (t118)))) goto label11;
  {
  t119 = ((int32_t)(v46));
  t120 = ((int32_t)(((int64_t)UINT64_C(132))));
  t121 = ((int32_t)(((int32_t)((uint64_t)(t119) * (uint64_t)(t120)))));
  t122 = ((int32_t)(v47));
  t123 = ((int32_t)(((int32_t)((uint64_t)(t121) + (uint64_t)(t122)))));
  t124 = ((int32_t)(v27));
  t125 = ((int32_t)(((int64_t)UINT64_C(16))));
  t126 = ((int32_t)(((int32_t)gg_mod(t124, t125))));
  t127 = ((int32_t)(((int64_t)UINT64_C(132))));
  t128 = ((int32_t)(((int32_t)((uint64_t)(t126) * (uint64_t)(t127)))));
  t129 = ((int32_t)(((int32_t)((uint64_t)(t123) + (uint64_t)(t128)))));
  t130 = ((int32_t)(v27));
  t131 = ((int32_t)(((int64_t)UINT64_C(16))));
  t132 = ((int32_t)(((int32_t)gg_div(t130, t131))));
  t133 = ((int32_t)(((int64_t)UINT64_C(4))));
  t134 = ((int32_t)(((int32_t)((uint64_t)(t132) * (uint64_t)(t133)))));
  v48 = ((int32_t)(((int32_t)((uint64_t)(t129) + (uint64_t)(t134)))));
  t135 = ((uint64_t)(v33));
  t136 = ((uint64_t)(((uint64_t)(v48))));
  t137 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t138 = ((uint64_t)(((uint64_t)((uint64_t)(t136) * (uint64_t)(t137)))));
  t139 = ((uint64_t)(((uint64_t)((uint64_t)(t135) + (uint64_t)(t138)))));
  t140 = fg175p_ldm4(t139);
  v49 = t140;
  t141 = ((uint64_t)(v29));
  t142 = ((int32_t)(((int64_t)UINT64_C(4))));
  t143 = ((int32_t)(v47));
  t144 = ((int32_t)(((int64_t)UINT64_C(8))));
  t145 = ((int32_t)(((int32_t)gg_div(t143, t144))));
  t146 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t142) * (uint64_t)(t145)))))));
  t147 = ((uint32_t)((v49).f0));
  fg175p_stu32(t141, t146, t147);
  t148 = ((uint64_t)(v29));
  t149 = ((int32_t)(((int64_t)UINT64_C(4))));
  t150 = ((int32_t)(v47));
  t151 = ((int32_t)(((int64_t)UINT64_C(8))));
  t152 = ((int32_t)(((int32_t)gg_div(t150, t151))));
  t153 = ((int32_t)(((int32_t)((uint64_t)(t149) * (uint64_t)(t152)))));
  t154 = ((int32_t)(((int64_t)UINT64_C(1))));
  t155 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t153) + (uint64_t)(t154)))))));
  t156 = ((uint32_t)((v49).f1));
  fg175p_stu32(t148, t155, t156);
  t157 = ((uint64_t)(v29));
  t158 = ((int32_t)(((int64_t)UINT64_C(4))));
  t159 = ((int32_t)(v47));
  t160 = ((int32_t)(((int64_t)UINT64_C(8))));
  t161 = ((int32_t)(((int32_t)gg_div(t159, t160))));
  t162 = ((int32_t)(((int32_t)((uint64_t)(t158) * (uint64_t)(t161)))));
  t163 = ((int32_t)(((int64_t)UINT64_C(2))));
  t164 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t162) + (uint64_t)(t163)))))));
  t165 = ((uint32_t)((v49).f2));
  fg175p_stu32(t157, t164, t165);
  t166 = ((uint64_t)(v29));
  t167 = ((int32_t)(((int64_t)UINT64_C(4))));
  t168 = ((int32_t)(v47));
  t169 = ((int32_t)(((int64_t)UINT64_C(8))));
  t170 = ((int32_t)(((int32_t)gg_div(t168, t169))));
  t171 = ((int32_t)(((int32_t)((uint64_t)(t167) * (uint64_t)(t170)))));
  t172 = ((int32_t)(((int64_t)UINT64_C(3))));
  t173 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t171) + (uint64_t)(t172)))))));
  t174 = ((uint32_t)((v49).f3));
  fg175p_stu32(t166, t173, t174);
  t175 = v47;
  t176 = ((int32_t)(((int64_t)UINT64_C(8))));
  v47 = ((int32_t)((uint64_t)(t175) + (uint64_t)(t176)));
  }
  goto label10;
label11:;
  fg175p_barrier();
  t177 = ((uint64_t)(v3));
  t178 = ((uint64_t)(v33));
  t179 = ((uint64_t)(((int64_t)UINT64_C(32768))));
  t180 = ((uint64_t)(((uint64_t)((uint64_t)(t178) + (uint64_t)(t179)))));
  t181 = ((int64_t)(v16));
  t182 = ((int32_t)(v19));
  t183 = ((int32_t)(((int64_t)UINT64_C(32))));
  t184 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t182) * (uint64_t)(t183)))))));
  t185 = ((int64_t)(((int64_t)(v17))));
  t186 = ((int64_t)(((int64_t)(v21))));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t185) * (uint64_t)(t186)))));
  t188 = ((int64_t)(v9));
  t189 = ((int32_t)(v21));
  t190 = ((int32_t)(v27));
  t191 = ((int32_t)(v28));
  fg175l_load_mask(t177, t180, t181, t184, t187, t188, t189, t190, t191);
  t192 = ((uint64_t)(v1));
  t193 = ((uint64_t)(v33));
  t194 = ((int64_t)(v14));
  t195 = ((int32_t)(v19));
  t196 = ((int32_t)(((int64_t)UINT64_C(32))));
  t197 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t195) * (uint64_t)(t196)))))));
  t198 = ((int32_t)(v27));
  t199 = ((int32_t)(v28));
  fg175l_load_kv(t192, t193, t194, t197, t198, t199);
  v50 = ((int32_t)(v19));
label13:;
  t200 = ((int32_t)(v50));
  t201 = ((int32_t)(v20));
  t202 = ((int32_t)(((int64_t)UINT64_C(1))));
  t203 = ((int32_t)(((int32_t)((uint64_t)(t201) - (uint64_t)(t202)))));
  if (!(((t200) < (t203)))) goto label14;
  {
  t204 = ((uint64_t)(v1));
  t205 = ((uint64_t)(v2));
  t206 = ((uint64_t)(v3));
  t207 = ((int64_t)(v14));
  t208 = ((int64_t)(v15));
  t209 = ((int64_t)(v16));
  t210 = ((float)(v8));
  t211 = ((int32_t)(v17));
  t212 = ((int32_t)(v50));
  t213 = ((int64_t)(v9));
  t214 = ((int32_t)(v21));
  t215 = ((uint64_t)(v29));
  t216 = ((uint64_t)(v30));
  t217 = ((uint64_t)(v31));
  t218 = ((uint64_t)(v32));
  t219 = ((uint64_t)(v33));
  t220 = ((int32_t)(v27));
  t221 = ((int32_t)(v28));
  t222 = ((int32_t)(v22));
  t223 = ((uint8_t)UINT64_C(0));
  fg175m_iter(t204, t205, t206, t207, t208, t209, t210, t211, t212, t213, t214, t215, t216, t217, t218, t219, t220, t221, t222, t223);
  t224 = v50;
  t225 = ((int32_t)(((int64_t)UINT64_C(1))));
  v50 = ((int32_t)((uint64_t)(t224) + (uint64_t)(t225)));
  }
  goto label13;
label14:;
  t226 = ((uint64_t)(v1));
  t227 = ((uint64_t)(v2));
  t228 = ((uint64_t)(v3));
  t229 = ((int64_t)(v14));
  t230 = ((int64_t)(v15));
  t231 = ((int64_t)(v16));
  t232 = ((float)(v8));
  t233 = ((int32_t)(v17));
  t234 = ((int32_t)(v50));
  t235 = ((int64_t)(v9));
  t236 = ((int32_t)(v21));
  t237 = ((uint64_t)(v29));
  t238 = ((uint64_t)(v30));
  t239 = ((uint64_t)(v31));
  t240 = ((uint64_t)(v32));
  t241 = ((uint64_t)(v33));
  t242 = ((int32_t)(v27));
  t243 = ((int32_t)(v28));
  t244 = ((int32_t)(v22));
  t245 = ((uint8_t)UINT64_C(1));
  fg175m_iter(t226, t227, t228, t229, t230, t231, t232, t233, t234, t235, t236, t237, t238, t239, t240, t241, t242, t243, t244, t245);
  fg175p_barrier();
  v51 = ((int32_t)(((int64_t)UINT64_C(0))));
label16:;
  t246 = ((int32_t)(v51));
  t247 = ((int32_t)(((int64_t)UINT64_C(2))));
  if (!(((t246) < (t247)))) goto label17;
  {
  t248 = ((uint64_t)(v32));
  t249 = ((uint64_t)(((uint64_t)(v51))));
  t250 = fg175p_gload(t248, t249);
  v52 = ((float)(t250));
  v53 = ((int32_t)(((int64_t)UINT64_C(2))));
label19:;
  t251 = ((int32_t)(v53));
  t252 = ((int32_t)(((int64_t)UINT64_C(1))));
  if (!(((t251) >= (t252)))) goto label20;
  {
  t253 = v52;
  t254 = ((float)(v52));
  t255 = ((int32_t)(v53));
  t256 = fg175p_shflx(t254, t255);
  t257 = ((float)(t256));
  v52 = ((t253) + (t257));
  t258 = ((int32_t)(v53));
  t259 = ((int64_t)(((int64_t)UINT64_C(1))));
  v53 = ((int32_t)(((t258) >> ((uint64_t)(t259) & 31))));
  }
  goto label19;
label20:;
  t260 = ((uint64_t)(v32));
  t261 = ((uint64_t)(((uint64_t)(v51))));
  t262 = ((float)(v52));
  fg175p_gstore(t260, t261, t262);
  t263 = v51;
  t264 = ((int32_t)(((int64_t)UINT64_C(1))));
  v51 = ((int32_t)((uint64_t)(t263) + (uint64_t)(t264)));
  }
  goto label16;
label17:;
  v54 = ((float)(gg_double_bits(UINT64_C(0))));
  v55 = ((float)(gg_double_bits(UINT64_C(0))));
  t265 = v24;
  t266 = (!(t265));
  t267 = t266;
  if (t267) {
  t268 = ((int32_t)(v22));
  t269 = ((int32_t)(((int64_t)UINT64_C(1))));
  t270 = ((t268) == (t269));
  t271 = t270;
  if (!t271) {
  t272 = ((int32_t)(v28));
  t273 = ((int32_t)(v22));
  t274 = ((int32_t)(((int32_t)gg_mod(t272, t273))));
  t275 = ((int32_t)(((int64_t)UINT64_C(0))));
  t271 = ((t274) == (t275));
  }
  t267 = t271;
  }
  t276 = t267;
  t277 = t276;
  if (t277) {
  t278 = ((uint64_t)(v4));
  t279 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t277 = ((t278) != (t279));
  }
  t280 = t277;
  if (t280) {
  {
  t281 = ((int32_t)(v28));
  t282 = ((int32_t)(v22));
  t283 = ((int32_t)(((int32_t)gg_div(t281, t282))));
  t284 = ((int32_t)(((int64_t)UINT64_C(16))));
  t285 = ((int32_t)(((int32_t)((uint64_t)(t283) * (uint64_t)(t284)))));
  t286 = ((int32_t)(((int64_t)UINT64_C(0))));
  t287 = ((int32_t)(v27));
  t288 = gg_function_14(t286, t287, base + 0);
  t289 = ((int32_t)(t288));
  v56 = ((int32_t)(((int32_t)((uint64_t)(t285) + (uint64_t)(t289)))));
  t290 = ((int32_t)(v28));
  t291 = ((int32_t)(v22));
  t292 = ((int32_t)(((int32_t)gg_div(t290, t291))));
  t293 = ((int32_t)(((int64_t)UINT64_C(16))));
  t294 = ((int32_t)(((int32_t)((uint64_t)(t292) * (uint64_t)(t293)))));
  t295 = ((int32_t)(((int64_t)UINT64_C(2))));
  t296 = ((int32_t)(v27));
  t297 = gg_function_14(t295, t296, base + 0);
  t298 = ((int32_t)(t297));
  v57 = ((int32_t)(((int32_t)((uint64_t)(t294) + (uint64_t)(t298)))));
  t299 = ((uint64_t)(v4));
  t300 = ((int32_t)(v56));
  t301 = ((int32_t)(((int64_t)UINT64_C(8))));
  t302 = ((uint64_t)(((uint64_t)(((int32_t)gg_mod(t300, t301))))));
  t303 = fg175p_gload(t299, t302);
  v58 = ((float)(t303));
  t304 = ((uint64_t)(v4));
  t305 = ((int32_t)(v57));
  t306 = ((int32_t)(((int64_t)UINT64_C(8))));
  t307 = ((uint64_t)(((uint64_t)(((int32_t)gg_mod(t305, t306))))));
  t308 = fg175p_gload(t304, t307);
  v59 = ((float)(t308));
  t309 = ((uint64_t)(v31));
  t310 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t311 = fg175p_gload(t309, t310);
  t312 = ((float)(t311));
  t313 = ((float)(v58));
  v60 = gg_function_15(t312, t313, base + 0);
  t314 = ((uint64_t)(v31));
  t315 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t316 = fg175p_gload(t314, t315);
  t317 = ((float)(t316));
  t318 = ((float)(v60));
  v61 = ((float)(((t317) - (t318))));
  t319 = ((float)(v61));
  t320 = fg175p_expf(t319);
  v54 = ((float)(t320));
  t321 = ((uint64_t)(v31));
  t322 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t323 = ((float)(v60));
  fg175p_gstore(t321, t322, t323);
  t324 = ((float)(v61));
  t325 = gg_double_bits(UINT64_C(4626322717216342016));
  t326 = ((float)((-(t325))));
  t327 = ((t324) < (t326));
  if (t327) {
  {
  v54 = ((float)(gg_double_bits(UINT64_C(0))));
  }
  }
  t328 = ((uint64_t)(v32));
  t329 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t330 = ((float)(v54));
  t331 = ((uint64_t)(v32));
  t332 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t333 = fg175p_gload(t331, t332);
  t334 = ((float)(t333));
  t335 = ((float)(((t330) * (t334))));
  t336 = ((float)(v58));
  t337 = ((float)(v60));
  t338 = ((float)(((t336) - (t337))));
  t339 = fg175p_expf(t338);
  t340 = ((float)(t339));
  t341 = ((float)(((t335) + (t340))));
  fg175p_gstore(t328, t329, t341);
  t342 = ((uint64_t)(v31));
  t343 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t344 = fg175p_gload(t342, t343);
  t345 = ((float)(t344));
  t346 = ((float)(v59));
  v62 = gg_function_15(t345, t346, base + 0);
  t347 = ((uint64_t)(v31));
  t348 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t349 = fg175p_gload(t347, t348);
  t350 = ((float)(t349));
  t351 = ((float)(v62));
  v63 = ((float)(((t350) - (t351))));
  t352 = ((float)(v63));
  t353 = fg175p_expf(t352);
  v55 = ((float)(t353));
  t354 = ((uint64_t)(v31));
  t355 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t356 = ((float)(v62));
  fg175p_gstore(t354, t355, t356);
  t357 = ((float)(v63));
  t358 = gg_double_bits(UINT64_C(4626322717216342016));
  t359 = ((float)((-(t358))));
  t360 = ((t357) < (t359));
  if (t360) {
  {
  v55 = ((float)(gg_double_bits(UINT64_C(0))));
  }
  }
  t361 = ((uint64_t)(v32));
  t362 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t363 = ((float)(v55));
  t364 = ((uint64_t)(v32));
  t365 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t366 = fg175p_gload(t364, t365);
  t367 = ((float)(t366));
  t368 = ((float)(((t363) * (t367))));
  t369 = ((float)(v59));
  t370 = ((float)(v62));
  t371 = ((float)(((t369) - (t370))));
  t372 = fg175p_expf(t371);
  t373 = ((float)(t372));
  t374 = ((float)(((t368) + (t373))));
  fg175p_gstore(t361, t362, t374);
  t375 = ((float)(v54));
  t376 = ((float)(v54));
  t377 = fg175p_h2pack(t375, t376);
  v64 = ((uint32_t)(t377));
  t378 = ((float)(v55));
  t379 = ((float)(v55));
  t380 = fg175p_h2pack(t378, t379);
  v65 = ((uint32_t)(t380));
  v66 = ((int32_t)(((int64_t)UINT64_C(0))));
label22:;
  t381 = ((int32_t)(v66));
  t382 = ((int32_t)(((int64_t)UINT64_C(16))));
  if (!(((t381) < (t382)))) goto label23;
  {
  t383 = ((uint64_t)(v30));
  t384 = ((int32_t)(((int64_t)UINT64_C(4))));
  t385 = ((int32_t)(v66));
  t386 = ((int32_t)(((int32_t)((uint64_t)(t384) * (uint64_t)(t385)))));
  t387 = ((int32_t)(((int64_t)UINT64_C(0))));
  t388 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t386) + (uint64_t)(t387)))))));
  t389 = ((uint64_t)(v30));
  t390 = ((int32_t)(((int64_t)UINT64_C(4))));
  t391 = ((int32_t)(v66));
  t392 = ((int32_t)(((int32_t)((uint64_t)(t390) * (uint64_t)(t391)))));
  t393 = ((int32_t)(((int64_t)UINT64_C(0))));
  t394 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t392) + (uint64_t)(t393)))))));
  t395 = fg175p_ldu32(t389, t394);
  t396 = ((uint32_t)(t395));
  t397 = ((uint32_t)(v64));
  t398 = fg175p_h2mul(t396, t397);
  t399 = ((uint32_t)(t398));
  fg175p_stu32(t383, t388, t399);
  t400 = ((uint64_t)(v30));
  t401 = ((int32_t)(((int64_t)UINT64_C(4))));
  t402 = ((int32_t)(v66));
  t403 = ((int32_t)(((int32_t)((uint64_t)(t401) * (uint64_t)(t402)))));
  t404 = ((int32_t)(((int64_t)UINT64_C(2))));
  t405 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t403) + (uint64_t)(t404)))))));
  t406 = ((uint64_t)(v30));
  t407 = ((int32_t)(((int64_t)UINT64_C(4))));
  t408 = ((int32_t)(v66));
  t409 = ((int32_t)(((int32_t)((uint64_t)(t407) * (uint64_t)(t408)))));
  t410 = ((int32_t)(((int64_t)UINT64_C(2))));
  t411 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t409) + (uint64_t)(t410)))))));
  t412 = fg175p_ldu32(t406, t411);
  t413 = ((uint32_t)(t412));
  t414 = ((uint32_t)(v64));
  t415 = fg175p_h2mul(t413, t414);
  t416 = ((uint32_t)(t415));
  fg175p_stu32(t400, t405, t416);
  t417 = ((uint64_t)(v30));
  t418 = ((int32_t)(((int64_t)UINT64_C(4))));
  t419 = ((int32_t)(v66));
  t420 = ((int32_t)(((int32_t)((uint64_t)(t418) * (uint64_t)(t419)))));
  t421 = ((int32_t)(((int64_t)UINT64_C(1))));
  t422 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t420) + (uint64_t)(t421)))))));
  t423 = ((uint64_t)(v30));
  t424 = ((int32_t)(((int64_t)UINT64_C(4))));
  t425 = ((int32_t)(v66));
  t426 = ((int32_t)(((int32_t)((uint64_t)(t424) * (uint64_t)(t425)))));
  t427 = ((int32_t)(((int64_t)UINT64_C(1))));
  t428 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t426) + (uint64_t)(t427)))))));
  t429 = fg175p_ldu32(t423, t428);
  t430 = ((uint32_t)(t429));
  t431 = ((uint32_t)(v65));
  t432 = fg175p_h2mul(t430, t431);
  t433 = ((uint32_t)(t432));
  fg175p_stu32(t417, t422, t433);
  t434 = ((uint64_t)(v30));
  t435 = ((int32_t)(((int64_t)UINT64_C(4))));
  t436 = ((int32_t)(v66));
  t437 = ((int32_t)(((int32_t)((uint64_t)(t435) * (uint64_t)(t436)))));
  t438 = ((int32_t)(((int64_t)UINT64_C(3))));
  t439 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t437) + (uint64_t)(t438)))))));
  t440 = ((uint64_t)(v30));
  t441 = ((int32_t)(((int64_t)UINT64_C(4))));
  t442 = ((int32_t)(v66));
  t443 = ((int32_t)(((int32_t)((uint64_t)(t441) * (uint64_t)(t442)))));
  t444 = ((int32_t)(((int64_t)UINT64_C(3))));
  t445 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t443) + (uint64_t)(t444)))))));
  t446 = fg175p_ldu32(t440, t445);
  t447 = ((uint32_t)(t446));
  t448 = ((uint32_t)(v65));
  t449 = fg175p_h2mul(t447, t448);
  t450 = ((uint32_t)(t449));
  fg175p_stu32(t434, t439, t450);
  t451 = v66;
  t452 = ((int32_t)(((int64_t)UINT64_C(1))));
  v66 = ((int32_t)((uint64_t)(t451) + (uint64_t)(t452)));
  }
  goto label22;
label23:;
  }
  }
  fg175p_barrier();
  t453 = ((int32_t)(v28));
  t454 = ((int32_t)(((int64_t)UINT64_C(16))));
  t455 = ((int32_t)(((int32_t)((uint64_t)(t453) * (uint64_t)(t454)))));
  t456 = ((int32_t)(v27));
  t457 = ((int32_t)(((int64_t)UINT64_C(4))));
  t458 = ((int32_t)(((int32_t)gg_mod(t456, t457))));
  t459 = ((int32_t)(v27));
  t460 = gg_function_18(t458, t459, base + 0);
  t461 = ((int32_t)(t460));
  v67 = ((int32_t)(((int32_t)((uint64_t)(t455) + (uint64_t)(t461)))));
  t462 = ((uint64_t)(v31));
  t463 = ((int32_t)(v27));
  t464 = ((int32_t)(((int64_t)UINT64_C(2))));
  t465 = ((uint64_t)(((uint64_t)(((int32_t)gg_mod(t463, t464))))));
  t466 = fg175p_gload(t462, t465);
  v68 = ((float)(t466));
  t467 = ((uint64_t)(v32));
  t468 = ((int32_t)(v27));
  t469 = ((int32_t)(((int64_t)UINT64_C(2))));
  t470 = ((uint64_t)(((uint64_t)(((int32_t)gg_mod(t468, t469))))));
  t471 = fg175p_gload(t467, t470);
  v69 = ((float)(t471));
  t472 = ((int32_t)(v27));
  t473 = ((int32_t)(((int64_t)UINT64_C(4))));
  t474 = ((int32_t)(((int32_t)gg_mod(t472, t473))));
  t475 = ((int32_t)(((int64_t)UINT64_C(2))));
  v70 = ((t474) < (t475));
  t476 = v23;
  t477 = (!(t476));
  t478 = t477;
  if (t478) {
  t479 = v24;
  t478 = (!(t479));
  }
  t480 = t478;
  t481 = t480;
  if (!t481) {
  t482 = ((int32_t)(v22));
  t483 = ((int32_t)(((int64_t)UINT64_C(1))));
  t481 = ((t482) > (t483));
  }
  t484 = t481;
  t485 = t484;
  if (t485) {
  t485 = v70;
  }
  t486 = t485;
  if (t486) {
  {
  t487 = ((uint64_t)(v33));
  t488 = ((int32_t)(v67));
  t489 = ((int32_t)(((int64_t)UINT64_C(132))));
  t490 = ((int32_t)(((int32_t)((uint64_t)(t488) * (uint64_t)(t489)))));
  t491 = ((int32_t)(((int64_t)UINT64_C(128))));
  t492 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t490) + (uint64_t)(t491)))))));
  t493 = ((float)(v68));
  fg175p_smstf(t487, t492, t493);
  t494 = ((uint64_t)(v33));
  t495 = ((int32_t)(v67));
  t496 = ((int32_t)(((int64_t)UINT64_C(132))));
  t497 = ((int32_t)(((int32_t)((uint64_t)(t495) * (uint64_t)(t496)))));
  t498 = ((int32_t)(((int64_t)UINT64_C(129))));
  t499 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t497) + (uint64_t)(t498)))))));
  t500 = ((float)(v69));
  fg175p_smstf(t494, t499, t500);
  }
  }
  fg175p_barrier();
  t501 = ((int32_t)(v22));
  t502 = ((int32_t)(((int64_t)UINT64_C(1))));
  t503 = ((t501) == (t502));
  if (t503) {
  {
  t504 = v23;
  t505 = t504;
  if (t505) {
  t505 = v70;
  }
  t506 = t505;
  if (t506) {
  {
  t507 = ((int64_t)(((int64_t)(v25))));
  t508 = ((int64_t)(((int64_t)(v34))));
  t509 = ((int64_t)(((int64_t)((uint64_t)(t507) * (uint64_t)(t508)))));
  t510 = ((int64_t)(((int64_t)(v67))));
  t511 = ((int64_t)(((int64_t)((uint64_t)(t509) + (uint64_t)(t510)))));
  t512 = ((int64_t)(((int64_t)UINT64_C(2))));
  v71 = ((int64_t)(((int64_t)((uint64_t)(t511) * (uint64_t)(t512)))));
  t513 = ((uint64_t)(v6));
  t514 = ((uint64_t)(((uint64_t)(v71))));
  t515 = ((float)(v68));
  fg175p_gstore(t513, t514, t515);
  t516 = ((uint64_t)(v6));
  t517 = ((int64_t)(v71));
  t518 = ((int64_t)(((int64_t)UINT64_C(1))));
  t519 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t517) + (uint64_t)(t518)))))));
  t520 = ((float)(v69));
  fg175p_gstore(t516, t519, t520);
  }
  }
  t521 = v24;
  t522 = t521;
  if (t522) {
  t522 = v70;
  }
  t523 = t522;
  if (t523) {
  {
  t524 = ((int64_t)(((int64_t)(v26))));
  t525 = ((int64_t)(((int64_t)(v25))));
  t526 = ((int64_t)(((int64_t)((uint64_t)(t524) + (uint64_t)(t525)))));
  t527 = ((int64_t)(((int64_t)(v34))));
  t528 = ((int64_t)(((int64_t)((uint64_t)(t526) * (uint64_t)(t527)))));
  t529 = ((int64_t)(((int64_t)(v67))));
  t530 = ((int64_t)(((int64_t)((uint64_t)(t528) + (uint64_t)(t529)))));
  t531 = ((int64_t)(((int64_t)UINT64_C(2))));
  v72 = ((int64_t)(((int64_t)((uint64_t)(t530) * (uint64_t)(t531)))));
  t532 = ((uint64_t)(v6));
  t533 = ((uint64_t)(((uint64_t)(v72))));
  t534 = ((float)(v68));
  fg175p_gstore(t532, t533, t534);
  t535 = ((uint64_t)(v6));
  t536 = ((int64_t)(v72));
  t537 = ((int64_t)(((int64_t)UINT64_C(1))));
  t538 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t536) + (uint64_t)(t537)))))));
  t539 = ((float)(v69));
  fg175p_gstore(t535, t538, t539);
  }
  }
  }
  }
  v73 = ((float)(gg_double_bits(UINT64_C(0))));
  v74 = ((float)(gg_double_bits(UINT64_C(0))));
  v75 = ((float)(gg_double_bits(UINT64_C(0))));
  t540 = ((int32_t)(v22));
  t541 = ((int32_t)(((int64_t)UINT64_C(1))));
  t542 = ((t540) > (t541));
  if (t542) {
  {
  t543 = ((int32_t)(v28));
  t544 = ((int32_t)(((int64_t)UINT64_C(16))));
  t545 = ((int32_t)(((int32_t)((uint64_t)(t543) * (uint64_t)(t544)))));
  t546 = ((int32_t)(v27));
  v76 = ((int32_t)(((int32_t)((uint64_t)(t545) + (uint64_t)(t546)))));
  t547 = ((int32_t)(v76));
  t548 = ((int32_t)(((int64_t)UINT64_C(132))));
  t549 = ((int32_t)(((int32_t)((uint64_t)(t547) * (uint64_t)(t548)))));
  t550 = ((int32_t)(((int64_t)UINT64_C(128))));
  v77 = ((int32_t)(((int32_t)((uint64_t)(t549) + (uint64_t)(t550)))));
  t551 = ((int32_t)(v28));
  t552 = ((int32_t)(v22));
  t553 = ((int32_t)(((int32_t)gg_mod(t551, t552))));
  t554 = ((int32_t)(((int64_t)UINT64_C(0))));
  t555 = ((t553) == (t554));
  if (t555) {
  {
  t556 = ((uint64_t)(v33));
  t557 = ((uint64_t)(((uint64_t)(v77))));
  t558 = fg175p_smldf(t556, t557);
  v78 = ((float)(t558));
  t559 = ((uint64_t)(v33));
  t560 = ((int32_t)(v77));
  t561 = ((int32_t)(((int64_t)UINT64_C(1))));
  t562 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t560) + (uint64_t)(t561)))))));
  t563 = fg175p_smldf(t559, t562);
  v79 = ((float)(t563));
  v73 = ((float)(v78));
  t564 = ((int32_t)(v22));
  t565 = ((int32_t)(((int64_t)UINT64_C(16))));
  t566 = ((int32_t)(((int32_t)((uint64_t)(t564) * (uint64_t)(t565)))));
  t567 = ((int32_t)(((int64_t)UINT64_C(2))));
  v80 = ((int32_t)(((int32_t)gg_div(t566, t567))));
label25:;
  t568 = ((int32_t)(v80));
  t569 = ((int32_t)(((int64_t)UINT64_C(16))));
  if (!(((t568) >= (t569)))) goto label26;
  {
  t570 = ((int32_t)(v80));
  t571 = ((int32_t)(((int64_t)UINT64_C(32))));
  t572 = ((t570) < (t571));
  if (t572) {
  {
  t573 = ((float)(v73));
  t574 = ((float)(v73));
  t575 = ((int32_t)(v80));
  t576 = fg175p_shflx(t574, t575);
  t577 = ((float)(t576));
  v73 = gg_function_15(t573, t577, base + 0);
  }
  }
  t578 = ((int32_t)(v80));
  t579 = ((int64_t)(((int64_t)UINT64_C(1))));
  v80 = ((int32_t)(((t578) >> ((uint64_t)(t579) & 31))));
  }
  goto label25;
label26:;
  t580 = ((float)(v78));
  t581 = ((float)(v73));
  t582 = ((float)(((t580) - (t581))));
  t583 = fg175p_expf(t582);
  v74 = ((float)(t583));
  t584 = ((float)(v74));
  t585 = ((float)(v79));
  v75 = ((float)(((t584) * (t585))));
  t586 = ((int32_t)(v22));
  t587 = ((int32_t)(((int64_t)UINT64_C(16))));
  t588 = ((int32_t)(((int32_t)((uint64_t)(t586) * (uint64_t)(t587)))));
  t589 = ((int32_t)(((int64_t)UINT64_C(2))));
  v80 = ((int32_t)(((int32_t)gg_div(t588, t589))));
label28:;
  t590 = ((int32_t)(v80));
  t591 = ((int32_t)(((int64_t)UINT64_C(16))));
  if (!(((t590) >= (t591)))) goto label29;
  {
  t592 = ((int32_t)(v80));
  t593 = ((int32_t)(((int64_t)UINT64_C(32))));
  t594 = ((t592) < (t593));
  if (t594) {
  {
  t595 = v75;
  t596 = ((float)(v75));
  t597 = ((int32_t)(v80));
  t598 = fg175p_shflx(t596, t597);
  t599 = ((float)(t598));
  v75 = ((t595) + (t599));
  }
  }
  t600 = ((int32_t)(v80));
  t601 = ((int64_t)(((int64_t)UINT64_C(1))));
  v80 = ((int32_t)(((t600) >> ((uint64_t)(t601) & 31))));
  }
  goto label28;
label29:;
  }
  }
  fg175p_barrier();
  t602 = ((int32_t)(v28));
  t603 = ((int32_t)(v22));
  t604 = ((int32_t)(((int32_t)gg_mod(t602, t603))));
  t605 = ((int32_t)(((int64_t)UINT64_C(0))));
  t606 = ((t604) == (t605));
  if (t606) {
  {
  t607 = ((uint64_t)(v33));
  t608 = ((uint64_t)(((uint64_t)(v77))));
  t609 = ((float)(v74));
  fg175p_smstf(t607, t608, t609);
  t610 = ((uint64_t)(v33));
  t611 = ((int32_t)(v77));
  t612 = ((int32_t)(((int64_t)UINT64_C(1))));
  t613 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t611) + (uint64_t)(t612)))))));
  t614 = ((float)(v75));
  fg175p_smstf(t610, t613, t614);
  t615 = v23;
  t616 = t615;
  if (t616) {
  t617 = ((int32_t)(v27));
  t618 = ((int32_t)(((int64_t)UINT64_C(16))));
  t616 = ((t617) < (t618));
  }
  t619 = t616;
  if (t619) {
  {
  t620 = ((int64_t)(((int64_t)(v25))));
  t621 = ((int64_t)(((int64_t)(v34))));
  t622 = ((int64_t)(((int64_t)((uint64_t)(t620) * (uint64_t)(t621)))));
  t623 = ((int32_t)(v28));
  t624 = ((int32_t)(v22));
  t625 = ((int32_t)(((int32_t)gg_div(t623, t624))));
  t626 = ((int32_t)(((int64_t)UINT64_C(16))));
  t627 = ((int32_t)(((int32_t)((uint64_t)(t625) * (uint64_t)(t626)))));
  t628 = ((int32_t)(v27));
  t629 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t627) + (uint64_t)(t628)))))));
  t630 = ((int64_t)(((int64_t)((uint64_t)(t622) + (uint64_t)(t629)))));
  t631 = ((int64_t)(((int64_t)UINT64_C(2))));
  v81 = ((int64_t)(((int64_t)((uint64_t)(t630) * (uint64_t)(t631)))));
  t632 = ((uint64_t)(v6));
  t633 = ((uint64_t)(((uint64_t)(v81))));
  t634 = ((float)(v73));
  fg175p_gstore(t632, t633, t634);
  t635 = ((uint64_t)(v6));
  t636 = ((int64_t)(v81));
  t637 = ((int64_t)(((int64_t)UINT64_C(1))));
  t638 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t636) + (uint64_t)(t637)))))));
  t639 = ((float)(v75));
  fg175p_gstore(t635, t638, t639);
  }
  }
  t640 = v24;
  t641 = t640;
  if (t641) {
  t642 = ((int32_t)(v27));
  t643 = ((int32_t)(((int64_t)UINT64_C(16))));
  t641 = ((t642) < (t643));
  }
  t644 = t641;
  if (t644) {
  {
  t645 = ((int64_t)(((int64_t)(v26))));
  t646 = ((int64_t)(((int64_t)(v25))));
  t647 = ((int64_t)(((int64_t)((uint64_t)(t645) + (uint64_t)(t646)))));
  t648 = ((int64_t)(((int64_t)(v34))));
  t649 = ((int64_t)(((int64_t)((uint64_t)(t647) * (uint64_t)(t648)))));
  t650 = ((int32_t)(v28));
  t651 = ((int32_t)(v22));
  t652 = ((int32_t)(((int32_t)gg_div(t650, t651))));
  t653 = ((int32_t)(((int64_t)UINT64_C(16))));
  t654 = ((int32_t)(((int32_t)((uint64_t)(t652) * (uint64_t)(t653)))));
  t655 = ((int32_t)(v27));
  t656 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t654) + (uint64_t)(t655)))))));
  t657 = ((int64_t)(((int64_t)((uint64_t)(t649) + (uint64_t)(t656)))));
  t658 = ((int64_t)(((int64_t)UINT64_C(2))));
  v82 = ((int64_t)(((int64_t)((uint64_t)(t657) * (uint64_t)(t658)))));
  t659 = ((uint64_t)(v6));
  t660 = ((uint64_t)(((uint64_t)(v82))));
  t661 = ((float)(v73));
  fg175p_gstore(t659, t660, t661);
  t662 = ((uint64_t)(v6));
  t663 = ((int64_t)(v82));
  t664 = ((int64_t)(((int64_t)UINT64_C(1))));
  t665 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t663) + (uint64_t)(t664)))))));
  t666 = ((float)(v75));
  fg175p_gstore(t662, t665, t666);
  }
  }
  }
  }
  }
  }
  t667 = ((int32_t)(v28));
  t668 = ((int32_t)(((int64_t)UINT64_C(16))));
  v83 = ((int32_t)(((int32_t)((uint64_t)(t667) * (uint64_t)(t668)))));
  v84 = ((int32_t)(((int64_t)UINT64_C(0))));
label31:;
  t669 = ((int32_t)(v84));
  t670 = ((int32_t)(((int64_t)UINT64_C(128))));
  if (!(((t669) < (t670)))) goto label32;
  {
  v85 = ((int32_t)(((int64_t)UINT64_C(0))));
label34:;
  t671 = ((int32_t)(v85));
  t672 = ((int32_t)(((int64_t)UINT64_C(4))));
  if (!(((t671) < (t672)))) goto label35;
  {
  t673 = ((uint64_t)(v33));
  t674 = ((int32_t)(v83));
  t675 = ((int32_t)(v85));
  t676 = ((int32_t)(v27));
  t677 = gg_function_18(t675, t676, base + 0);
  t678 = ((int32_t)(t677));
  t679 = ((int32_t)(((int32_t)((uint64_t)(t674) + (uint64_t)(t678)))));
  t680 = ((int32_t)(((int64_t)UINT64_C(132))));
  t681 = ((int32_t)(((int32_t)((uint64_t)(t679) * (uint64_t)(t680)))));
  t682 = ((int32_t)(v84));
  t683 = ((int32_t)(((int32_t)((uint64_t)(t681) + (uint64_t)(t682)))));
  t684 = ((int32_t)(v85));
  t685 = ((int32_t)(v27));
  t686 = gg_function_21(t684, t685, base + 0);
  t687 = ((int32_t)(t686));
  t688 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t683) + (uint64_t)(t687)))))));
  t689 = ((uint64_t)(v30));
  t690 = ((int32_t)(((int64_t)UINT64_C(4))));
  t691 = ((int32_t)(v84));
  t692 = ((int32_t)(((int64_t)UINT64_C(8))));
  t693 = ((int32_t)(((int32_t)gg_div(t691, t692))));
  t694 = ((int32_t)(((int32_t)((uint64_t)(t690) * (uint64_t)(t693)))));
  t695 = ((int32_t)(v85));
  t696 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t694) + (uint64_t)(t695)))))));
  t697 = fg175p_ldu32(t689, t696);
  t698 = ((uint32_t)(t697));
  fg175p_smst(t673, t688, t698);
  t699 = v85;
  t700 = ((int32_t)(((int64_t)UINT64_C(1))));
  v85 = ((int32_t)((uint64_t)(t699) + (uint64_t)(t700)));
  }
  goto label34;
label35:;
  t701 = v84;
  t702 = ((int32_t)(((int64_t)UINT64_C(8))));
  v84 = ((int32_t)((uint64_t)(t701) + (uint64_t)(t702)));
  }
  goto label31;
label32:;
  fg175p_barrier();
  t703 = ((int32_t)(v22));
  t704 = ((int32_t)(((int64_t)UINT64_C(1))));
  t705 = ((t703) == (t704));
  t706 = t705;
  if (!t706) {
  t707 = ((int32_t)(v28));
  t708 = ((int32_t)(v22));
  t709 = ((int32_t)(((int32_t)gg_mod(t707, t708))));
  t710 = ((int32_t)(((int64_t)UINT64_C(0))));
  t706 = ((t709) == (t710));
  }
  t711 = t706;
  if (t711) {
  {
  t712 = ((int64_t)(((int64_t)(v26))));
  t713 = ((int64_t)(((int64_t)UINT64_C(2))));
  t714 = ((int64_t)(((int64_t)((uint64_t)(t712) * (uint64_t)(t713)))));
  t715 = ((int64_t)(((int64_t)(v34))));
  t716 = ((int64_t)(((int64_t)((uint64_t)(t714) * (uint64_t)(t715)))));
  t717 = ((int64_t)(((int64_t)(v25))));
  t718 = ((int64_t)(((int64_t)(v34))));
  t719 = ((int64_t)(((int64_t)((uint64_t)(t717) * (uint64_t)(t718)))));
  t720 = ((int64_t)(((int64_t)UINT64_C(128))));
  t721 = ((int64_t)(((int64_t)((uint64_t)(t719) * (uint64_t)(t720)))));
  t722 = ((int64_t)(((int64_t)((uint64_t)(t716) + (uint64_t)(t721)))));
  t723 = ((int64_t)(((int64_t)UINT64_C(2))));
  v86 = ((int64_t)(((int64_t)((uint64_t)(t722) * (uint64_t)(t723)))));
  v87 = ((int32_t)(((int64_t)UINT64_C(0))));
label37:;
  t724 = ((int32_t)(v87));
  t725 = ((int32_t)(v34));
  if (!(((t724) < (t725)))) goto label38;
  {
  t726 = ((int32_t)(v87));
  t727 = ((int32_t)(v28));
  t728 = ((int32_t)(v22));
  t729 = ((int32_t)(((int32_t)gg_div(t727, t728))));
  v88 = ((int32_t)(((int32_t)((uint64_t)(t726) + (uint64_t)(t729)))));
  t730 = ((int32_t)(v88));
  t731 = ((int32_t)(((int64_t)UINT64_C(16))));
  t732 = ((int32_t)(((int32_t)gg_div(t730, t731))));
  t733 = ((int32_t)(v22));
  t734 = ((int32_t)(((int64_t)UINT64_C(16))));
  t735 = ((int32_t)(((int32_t)((uint64_t)(t733) * (uint64_t)(t734)))));
  t736 = ((int32_t)(((int32_t)((uint64_t)(t732) * (uint64_t)(t735)))));
  t737 = ((int32_t)(v88));
  t738 = ((int32_t)(((int64_t)UINT64_C(16))));
  t739 = ((int32_t)(((int32_t)gg_mod(t737, t738))));
  v89 = ((int32_t)(((int32_t)((uint64_t)(t736) + (uint64_t)(t739)))));
  t740 = ((int32_t)(v88));
  t741 = ((int32_t)(((int64_t)UINT64_C(8))));
  v90 = ((int32_t)(((int32_t)gg_div(t740, t741))));
  t742 = ((int32_t)(v88));
  t743 = ((int32_t)(((int64_t)UINT64_C(8))));
  v91 = ((int32_t)(((int32_t)gg_mod(t742, t743))));
  t744 = v24;
  t745 = (!(t744));
  t746 = t745;
  if (t746) {
  t747 = ((int32_t)(v21));
  t748 = ((int32_t)(((int64_t)UINT64_C(1))));
  t749 = ((t747) > (t748));
  t750 = t749;
  if (t750) {
  t751 = ((int64_t)(((int64_t)(v17))));
  t752 = ((int64_t)(((int64_t)(v21))));
  t753 = ((int64_t)(((int64_t)((uint64_t)(t751) * (uint64_t)(t752)))));
  t754 = ((int64_t)(((int64_t)(v90))));
  t755 = ((int64_t)(((int64_t)((uint64_t)(t753) + (uint64_t)(t754)))));
  t756 = ((int64_t)(v9));
  t750 = ((t755) >= (t756));
  }
  t757 = t750;
  t758 = t757;
  if (!t758) {
  t759 = ((int32_t)(v18));
  t760 = ((int32_t)(((int64_t)UINT64_C(8))));
  t761 = ((int32_t)(((int32_t)((uint64_t)(t759) * (uint64_t)(t760)))));
  t762 = ((int32_t)(v91));
  t763 = ((int32_t)(((int32_t)((uint64_t)(t761) + (uint64_t)(t762)))));
  t764 = ((int32_t)(v11));
  t758 = ((t763) >= (t764));
  }
  t746 = t758;
  }
  v92 = t746;
  t765 = v92;
  t766 = (!(t765));
  if (t766) {
  {
  t767 = ((int32_t)(v89));
  t768 = ((int32_t)(((int64_t)UINT64_C(132))));
  t769 = ((int32_t)(((int32_t)((uint64_t)(t767) * (uint64_t)(t768)))));
  t770 = ((int32_t)(((int64_t)UINT64_C(128))));
  v93 = ((int32_t)(((int32_t)((uint64_t)(t769) + (uint64_t)(t770)))));
  v94 = ((int32_t)(((int64_t)UINT64_C(0))));
label40:;
  t771 = ((int32_t)(v94));
  t772 = ((int32_t)(((int64_t)UINT64_C(128))));
  if (!(((t771) < (t772)))) goto label41;
  {
  t773 = ((int32_t)(v94));
  t774 = ((int32_t)(v27));
  v95 = ((int32_t)(((int32_t)((uint64_t)(t773) + (uint64_t)(t774)))));
  v96 = ((float)(gg_double_bits(UINT64_C(0))));
  v97 = ((float)(gg_double_bits(UINT64_C(0))));
  v98 = ((int32_t)(((int64_t)UINT64_C(0))));
label43:;
  t775 = ((int32_t)(v98));
  t776 = ((int32_t)(v22));
  if (!(((t775) < (t776)))) goto label44;
  {
  t777 = ((int32_t)(v22));
  t778 = ((int32_t)(((int64_t)UINT64_C(1))));
  t779 = ((t777) == (t778));
  if (t779) {
  {
  v99 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  {
  t780 = ((uint64_t)(v33));
  t781 = ((int32_t)(v93));
  t782 = ((int32_t)(v98));
  t783 = ((int32_t)(((int64_t)UINT64_C(16))));
  t784 = ((int32_t)(((int32_t)((uint64_t)(t782) * (uint64_t)(t783)))));
  t785 = ((int32_t)(((int64_t)UINT64_C(132))));
  t786 = ((int32_t)(((int32_t)((uint64_t)(t784) * (uint64_t)(t785)))));
  t787 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t781) + (uint64_t)(t786)))))));
  t788 = fg175p_smldf(t780, t787);
  v99 = ((float)(t788));
  }
  }
  t789 = ((uint64_t)(v33));
  t790 = ((int32_t)(v89));
  t791 = ((int32_t)(v98));
  t792 = ((int32_t)(((int64_t)UINT64_C(16))));
  t793 = ((int32_t)(((int32_t)((uint64_t)(t791) * (uint64_t)(t792)))));
  t794 = ((int32_t)(((int32_t)((uint64_t)(t790) + (uint64_t)(t793)))));
  t795 = ((int32_t)(((int64_t)UINT64_C(132))));
  t796 = ((int32_t)(((int32_t)((uint64_t)(t794) * (uint64_t)(t795)))));
  t797 = ((int32_t)(v95));
  t798 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t796) + (uint64_t)(t797)))))));
  t799 = fg175p_smld(t789, t798);
  v100 = ((uint32_t)(t799));
  t800 = v96;
  t801 = ((uint32_t)(v100));
  t802 = fg175p_hlo(t801);
  t803 = ((float)(t802));
  t804 = ((float)(v99));
  t805 = ((float)(((t803) * (t804))));
  v96 = ((t800) + (t805));
  t806 = v97;
  t807 = ((uint32_t)(v100));
  t808 = fg175p_hhi(t807);
  t809 = ((float)(t808));
  t810 = ((float)(v99));
  t811 = ((float)(((t809) * (t810))));
  v97 = ((t806) + (t811));
  t812 = v98;
  t813 = ((int32_t)(((int64_t)UINT64_C(1))));
  v98 = ((int32_t)((uint64_t)(t812) + (uint64_t)(t813)));
  }
  goto label43;
label44:;
  t814 = v23;
  t815 = (!(t814));
  t816 = t815;
  if (t816) {
  t817 = v24;
  t816 = (!(t817));
  }
  t818 = t816;
  if (t818) {
  {
  t819 = ((uint64_t)(v33));
  t820 = ((int32_t)(v93));
  t821 = ((int32_t)(((int64_t)UINT64_C(1))));
  t822 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t820) + (uint64_t)(t821)))))));
  t823 = fg175p_smldf(t819, t822);
  v101 = ((float)(t823));
  t824 = ((float)(v96));
  t825 = ((float)(v101));
  v96 = ((float)(((t824) / (t825))));
  t826 = ((float)(v97));
  t827 = ((float)(v101));
  v97 = ((float)(((t826) / (t827))));
  }
  }
  t828 = v24;
  if (t828) {
  {
  t829 = ((int64_t)(v86));
  t830 = ((int64_t)(((int64_t)(v88))));
  t831 = ((int64_t)(((int64_t)UINT64_C(128))));
  t832 = ((int64_t)(((int64_t)((uint64_t)(t830) * (uint64_t)(t831)))));
  t833 = ((int64_t)(((int64_t)(v95))));
  t834 = ((int64_t)(((int64_t)((uint64_t)(t832) + (uint64_t)(t833)))));
  t835 = ((int64_t)(((int64_t)UINT64_C(2))));
  t836 = ((int64_t)(((int64_t)((uint64_t)(t834) * (uint64_t)(t835)))));
  v102 = ((int64_t)(((int64_t)((uint64_t)(t829) + (uint64_t)(t836)))));
  t837 = ((uint64_t)(v6));
  t838 = ((uint64_t)(((uint64_t)(v102))));
  t839 = ((float)(v96));
  fg175p_gstore(t837, t838, t839);
  t840 = ((uint64_t)(v6));
  t841 = ((int64_t)(v102));
  t842 = ((int64_t)(((int64_t)UINT64_C(1))));
  t843 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t841) + (uint64_t)(t842)))))));
  t844 = ((float)(v97));
  fg175p_gstore(t840, t843, t844);
  }
  } else {
  {
  t845 = ((int64_t)(((int64_t)(v17))));
  t846 = ((int64_t)(((int64_t)(v21))));
  t847 = ((int64_t)(((int64_t)((uint64_t)(t845) * (uint64_t)(t846)))));
  t848 = ((int64_t)(((int64_t)(v90))));
  t849 = ((int64_t)(((int64_t)((uint64_t)(t847) + (uint64_t)(t848)))));
  t850 = ((int64_t)(((int64_t)(v10))));
  t851 = ((int64_t)(((int64_t)((uint64_t)(t849) * (uint64_t)(t850)))));
  t852 = ((int64_t)(((int64_t)(v91))));
  t853 = ((int64_t)(((int64_t)((uint64_t)(t851) + (uint64_t)(t852)))));
  t854 = ((int64_t)(((int64_t)UINT64_C(128))));
  t855 = ((int64_t)(((int64_t)((uint64_t)(t853) * (uint64_t)(t854)))));
  t856 = ((int64_t)(((int64_t)(v95))));
  t857 = ((int64_t)(((int64_t)((uint64_t)(t855) + (uint64_t)(t856)))));
  t858 = ((int64_t)(((int64_t)UINT64_C(2))));
  v103 = ((int64_t)(((int64_t)((uint64_t)(t857) * (uint64_t)(t858)))));
  t859 = ((uint64_t)(v5));
  t860 = ((uint64_t)(((uint64_t)(v103))));
  t861 = ((float)(v96));
  fg175p_gstore(t859, t860, t861);
  t862 = ((uint64_t)(v5));
  t863 = ((int64_t)(v103));
  t864 = ((int64_t)(((int64_t)UINT64_C(1))));
  t865 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t863) + (uint64_t)(t864)))))));
  t866 = ((float)(v97));
  fg175p_gstore(t862, t865, t866);
  }
  }
  t867 = v94;
  t868 = ((int32_t)(((int64_t)UINT64_C(32))));
  v94 = ((int32_t)((uint64_t)(t867) + (uint64_t)(t868)));
  }
  goto label40;
label41:;
  }
  }
  t869 = v87;
  t870 = ((int32_t)(((int64_t)UINT64_C(4))));
  t871 = ((int32_t)(v22));
  t872 = ((int32_t)(((int32_t)gg_div(t870, t871))));
  v87 = ((int32_t)((uint64_t)(t869) + (uint64_t)(t872)));
  }
  goto label37;
label38:;
  }
  }
  t873 = ((int32_t)(v22));
  t874 = ((int32_t)(((int64_t)UINT64_C(1))));
  t875 = ((t873) > (t874));
  if (t875) {
  {
  fg175p_barrier();
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
  float t7;
  float t8;
  int64_t t9;
  int32_t t10;
  int32_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int32_t t17;
  int32_t t18;
  int32_t t19;
  int32_t t20;
  int32_t t21;
  int32_t t22;
  uint8_t t23;
  uint8_t t24;
  int32_t t25;
  int32_t t26;
  int32_t t27;
  int32_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t1 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t6 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t7 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t8 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t10 = ((int32_t)(((int64_t)UINT64_C(0))));
  t11 = ((int32_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t17 = ((int32_t)(((int64_t)UINT64_C(0))));
  t18 = ((int32_t)(((int64_t)UINT64_C(0))));
  t19 = ((int32_t)(((int64_t)UINT64_C(0))));
  t20 = ((int32_t)(((int64_t)UINT64_C(0))));
  t21 = ((int32_t)(((int64_t)UINT64_C(4))));
  t22 = ((int32_t)(((int64_t)UINT64_C(2))));
  t23 = ((uint8_t)UINT64_C(0));
  t24 = ((uint8_t)UINT64_C(0));
  t25 = ((int32_t)(((int64_t)UINT64_C(0))));
  t26 = ((int32_t)(((int64_t)UINT64_C(0))));
  t27 = ((int32_t)(((int64_t)UINT64_C(0))));
  t28 = ((int32_t)(((int64_t)UINT64_C(0))));
  t29 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t33 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  gg_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, t20, t21, t22, t23, t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, base + 0);
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
