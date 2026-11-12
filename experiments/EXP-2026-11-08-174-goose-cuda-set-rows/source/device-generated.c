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
#pragma pack(push, 1)
struct gg_type_1 {
  uint32_t f0;
  uint32_t f1;
  uint32_t f2;
};
#pragma pack(pop)
typedef struct gg_type_8 gg_type_8;
#pragma pack(push, 1)
struct gg_type_8 {
  int64_t f3;
  int64_t f4;
  int64_t f5;
  int64_t f6;
};
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, gg_type_1 value);
static uint8_t gg_eq_2(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint32_t value);
static uint8_t gg_eq_3(int64_t a, int64_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, int64_t value);
static uint8_t gg_eq_4(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint64_t value);
static uint8_t gg_eq_5(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, uint8_t value);
static uint8_t gg_eq_6(int32_t a, int32_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, int32_t value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(float a, float b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, float value);
static uint8_t gg_eq_10(double a, double b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, double value);
static uint8_t gg_eq_11(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((gg_type_1*)a.data)[i],((gg_type_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b) {
if(!gg_eq_2(a.f0,b.f0))return 0;
if(!gg_eq_2(a.f1,b.f1))return 0;
if(!gg_eq_2(a.f2,b.f2))return 0;
return 1;
}
static void gg_render_1(gg_reference out, gg_type_1 value) {
gg_append(out,"\106\141\163\164\104\151\166\040\173\040",10,1);
gg_render_2(out, value.f0);
gg_append(out,", ",2,1);
gg_render_2(out, value.f1);
gg_append(out,", ",2,1);
gg_render_2(out, value.f2);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
if(!gg_eq_3(a.f3,b.f3))return 0;
if(!gg_eq_3(a.f4,b.f4))return 0;
if(!gg_eq_3(a.f5,b.f5))return 0;
if(!gg_eq_3(a.f6,b.f6))return 0;
return 1;
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
gg_append(out,"\106\147\061\067\066\162\111\144\170\040\173\040",12,1);
gg_render_3(out, value.f3);
gg_append(out,", ",2,1);
gg_render_3(out, value.f4);
gg_append(out,", ",2,1);
gg_render_3(out, value.f5);
gg_append(out,", ",2,1);
gg_render_3(out, value.f6);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(float a, float b) {
return a == b;
}
static void gg_render_9(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(double a, double b) {
return a == b;
}
static void gg_render_10(gg_reference out, double value) {
gg_float(out,value);
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
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, gg_type_1 p4, gg_type_1 p5, gg_type_1 p6, gg_type_1 p7, gg_type_1 p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, uint8_t p19, uint8_t p20, int32_t p21, int32_t p22, int32_t p23, int64_t base);
static gg_type_8 gg_function_2(uint32_t p0, gg_type_1 p1, gg_type_1 p2, gg_type_1 p3, int64_t base);
static uint32_t gg_function_3(uint32_t p0, gg_type_1 p1, int64_t base);
static uint32_t gg_function_5(uint32_t p0, gg_type_1 p1, int64_t base);
static void gg_function_10(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base);
static void gg_function_16(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, gg_type_1 p4, gg_type_1 p5, gg_type_1 p6, gg_type_1 p7, gg_type_1 p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, uint8_t p19, uint8_t p20, int32_t p21, int32_t p22, int32_t p23, int64_t base);
static uint32_t gg_function_3(uint32_t p0, gg_type_1 p1, int64_t base) {
  uint32_t t0;
  uint32_t t1;
  uint32_t t2;
  uint32_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint32_t v0;
  gg_type_1 v1;
  uint32_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint32_t)(v0));
  t2 = ((uint32_t)((v1).f0));
  t3 = fg176r_mulhi(t1, t2);
  v2 = ((uint32_t)(t3));
  t4 = ((uint64_t)(((uint64_t)(v2))));
  t5 = ((uint64_t)(((uint64_t)(v0))));
  t6 = ((uint64_t)(((uint64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = ((uint64_t)(((uint64_t)((v1).f1))));
  t0 = ((uint32_t)(((uint32_t)(((t6) >> ((uint64_t)(t7) & 63))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint32_t gg_function_5(uint32_t p0, gg_type_1 p1, int64_t base) {
  uint32_t t0;
  uint32_t t1;
  uint32_t t2;
  gg_type_1 t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
  uint32_t t7;
  uint32_t v0;
  gg_type_1 v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint32_t)(v0));
  t2 = ((uint32_t)(v0));
  t3 = v1;
  t4 = gg_function_3(t2, t3, base + 0);
  t5 = ((uint32_t)(t4));
  t6 = ((uint32_t)((v1).f2));
  t7 = ((uint32_t)(((uint32_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t0 = ((uint32_t)(((uint32_t)((uint64_t)(t1) - (uint64_t)(t7)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_8 gg_function_2(uint32_t p0, gg_type_1 p1, gg_type_1 p2, gg_type_1 p3, int64_t base) {
  gg_type_8 t0;
  uint32_t t1;
  gg_type_1 t2;
  uint32_t t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
  uint32_t t7;
  gg_type_1 t8;
  uint32_t t9;
  uint32_t t10;
  uint32_t t11;
  uint32_t t12;
  uint32_t t13;
  gg_type_1 t14;
  uint32_t t15;
  uint32_t t16;
  uint32_t t17;
  uint32_t t18;
  uint32_t v0;
  gg_type_1 v1;
  gg_type_1 v2;
  gg_type_1 v3;
  uint32_t v4;
  uint32_t v5;
  uint32_t v6;
  uint32_t v7;
  uint32_t v8;
  uint32_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((uint32_t)(v0));
  t2 = v1;
  v4 = gg_function_3(t1, t2, base + 0);
  t3 = ((uint32_t)(v0));
  t4 = ((uint32_t)(v4));
  t5 = ((uint32_t)((v1).f2));
  t6 = ((uint32_t)(((uint32_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  v5 = ((uint32_t)(((uint32_t)((uint64_t)(t3) - (uint64_t)(t6)))));
  t7 = ((uint32_t)(v4));
  t8 = v2;
  v6 = gg_function_3(t7, t8, base + 0);
  t9 = ((uint32_t)(v4));
  t10 = ((uint32_t)(v6));
  t11 = ((uint32_t)((v2).f2));
  t12 = ((uint32_t)(((uint32_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  v7 = ((uint32_t)(((uint32_t)((uint64_t)(t9) - (uint64_t)(t12)))));
  t13 = ((uint32_t)(v6));
  t14 = v3;
  v8 = gg_function_3(t13, t14, base + 0);
  t15 = ((uint32_t)(v6));
  t16 = ((uint32_t)(v8));
  t17 = ((uint32_t)((v3).f2));
  t18 = ((uint32_t)(((uint32_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  v9 = ((uint32_t)(((uint32_t)((uint64_t)(t15) - (uint64_t)(t18)))));
  memset(&(t0), 0, sizeof(t0));
  (t0).f3 = ((int64_t)(((int64_t)(v5))));
  (t0).f4 = ((int64_t)(((int64_t)(v7))));
  (t0).f5 = ((int64_t)(((int64_t)(v9))));
  (t0).f6 = ((int64_t)(((int64_t)(v8))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_16(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, gg_type_1 p4, gg_type_1 p5, gg_type_1 p6, gg_type_1 p7, gg_type_1 p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, uint8_t p19, uint8_t p20, int32_t p21, int32_t p22, int32_t p23, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint32_t t7;
  gg_type_1 t8;
  gg_type_1 t9;
  gg_type_1 t10;
  uint32_t t11;
  gg_type_1 t12;
  uint32_t t13;
  uint32_t t14;
  gg_type_1 t15;
  uint32_t t16;
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
  uint8_t t27;
  uint64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  int64_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  int64_t t46;
  int64_t t47;
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
  uint64_t t75;
  uint64_t t76;
  uint64_t t77;
  float t78;
  float t79;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  gg_type_1 v4;
  gg_type_1 v5;
  gg_type_1 v6;
  gg_type_1 v7;
  gg_type_1 v8;
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
  uint8_t v19;
  uint8_t v20;
  int32_t v21;
  int32_t v22;
  int32_t v23;
  int64_t v24;
  gg_type_8 v25;
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
  int64_t v36;
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
  {
  t0 = ((int64_t)(((int64_t)(v23))));
  t1 = ((int64_t)(((int64_t)(v21))));
  t2 = ((int64_t)(((int64_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  t3 = ((int64_t)(((int64_t)(v22))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t4 = ((int64_t)(v24));
  t5 = ((int64_t)(v3));
  t6 = ((t4) >= (t5));
  if (t6) {
  {
  goto gg_done;
  }
  }
  t7 = ((uint32_t)(((uint32_t)(v24))));
  t8 = v4;
  t9 = v5;
  t10 = v6;
  v25 = gg_function_2(t7, t8, t9, t10, base + 0);
  v26 = ((int64_t)((v25).f3));
  v27 = ((int64_t)((v25).f4));
  v28 = ((int64_t)((v25).f5));
  v29 = ((int64_t)((v25).f6));
  t11 = ((uint32_t)(((uint32_t)(v29))));
  t12 = v8;
  t13 = gg_function_5(t11, t12, base + 0);
  v30 = ((int64_t)(((int64_t)(t13))));
  t14 = ((uint32_t)(((uint32_t)(v28))));
  t15 = v7;
  t16 = gg_function_5(t14, t15, base + 0);
  v31 = ((int64_t)(((int64_t)(t16))));
  v32 = ((int64_t)(v27));
  fg176r_pdl_sync();
  t17 = ((int64_t)(v32));
  t18 = ((int64_t)(v12));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(v31));
  t21 = ((int64_t)(v13));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t22)))));
  t24 = ((int64_t)(v30));
  t25 = ((int64_t)(v14));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t26)))));
  t27 = v20;
  if (t27) {
  {
  t28 = ((uint64_t)(v1));
  t29 = ((int64_t)(v33));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))))));
  t32 = fg176r_ld_i64(t28, t31);
  v34 = ((int64_t)(t32));
  }
  } else {
  {
  t33 = ((uint64_t)(v1));
  t34 = ((int64_t)(v33));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))))));
  t37 = fg176r_ld_i32(t33, t36);
  v34 = ((int64_t)(t37));
  }
  }
  fg176r_pdl_lc();
  t38 = v19;
  if (t38) {
  {
  t39 = ((int64_t)(v34));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t41 = ((t39) < (t40));
  t42 = t41;
  if (!t42) {
  t43 = ((int64_t)(v34));
  t44 = ((int64_t)(v18));
  t42 = ((t43) >= (t44));
  }
  t45 = t42;
  if (t45) {
  {
  fg176r_bad_index();
  goto gg_done;
  }
  }
  }
  }
  t46 = ((int64_t)(v27));
  t47 = ((int64_t)(v9));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)(v28));
  t50 = ((int64_t)(v10));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t51)))));
  t53 = ((int64_t)(v29));
  t54 = ((int64_t)(v11));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t57 = ((int64_t)(v26));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  t59 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t60 = ((int64_t)(v34));
  t61 = ((int64_t)(v15));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = ((int64_t)(v28));
  t64 = ((int64_t)(v16));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t65)))));
  t67 = ((int64_t)(v29));
  t68 = ((int64_t)(v17));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t69)))));
  t71 = ((int64_t)(v26));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  t74 = ((uint64_t)(v2));
  t75 = ((uint64_t)(((uint64_t)(v36))));
  t76 = ((uint64_t)(v0));
  t77 = ((uint64_t)(((uint64_t)(v35))));
  t78 = fg176r_ld_f32(t76, t77);
  t79 = ((float)(t78));
  fg176r_st_f32(t74, t75, t79);
  }
gg_done:;
}
static void gg_function_10(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base) {
  double t0;
  int32_t t1;
  int32_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  float t9;
  float t10;
  float t11;
  float t12;
  float t13;
  uint8_t t14;
  int32_t t15;
  int32_t t16;
  float t17;
  float t18;
  float t19;
  float t20;
  uint8_t t21;
  float t22;
  float t23;
  uint64_t t24;
  uint64_t t25;
  float t26;
  int32_t t27;
  int32_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  int32_t t34;
  int32_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint64_t t39;
  uint64_t t40;
  float t41;
  float t42;
  float t43;
  uint64_t t44;
  uint64_t t45;
  float t46;
  float t47;
  float t48;
  float t49;
  float t50;
  float t51;
  int32_t t52;
  float t53;
  float t54;
  float t55;
  int32_t t56;
  int32_t t57;
  int32_t t58;
  uint8_t t59;
  int32_t t60;
  int32_t t61;
  uint8_t t62;
  int32_t t63;
  int32_t t64;
  int32_t t65;
  int32_t t66;
  uint32_t t67;
  uint32_t t68;
  uint32_t t69;
  uint32_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint64_t t74;
  uint64_t t75;
  uint64_t t76;
  uint32_t t77;
  uint32_t t78;
  uint32_t t79;
  int32_t t80;
  int32_t t81;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  float v4;
  float v5;
  float v6;
  float v7;
  uint64_t v8;
  int32_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  float v14;
  float v15;
  int32_t v16;
  uint64_t v17;
  float v18;
  float v19;
  float v20;
  float v21;
  int32_t v22;
  uint64_t v23;
  uint64_t v24;
  float v25;
  float v26;
  int32_t v27;
  int32_t v28;
  int32_t v29;
  int32_t v30;
  uint32_t v31;
  uint32_t v32;
  uint32_t v33;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  v4 = ((float)(gg_double_bits(UINT64_C(0))));
  t0 = gg_double_bits(UINT64_C(4620693217682128896));
  v5 = ((float)((-(t0))));
  v6 = ((float)(gg_double_bits(UINT64_C(4620974692658839552))));
  v7 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  v8 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v9 = ((int32_t)(((int64_t)UINT64_C(32))));
  v10 = ((int32_t)(((int64_t)UINT64_C(16))));
  v11 = ((int32_t)(((int64_t)UINT64_C(16))));
  v12 = ((int32_t)(((int64_t)UINT64_C(15))));
  v13 = ((int32_t)(((int64_t)UINT64_C(255))));
  v14 = ((float)(v4));
  v15 = ((float)(v4));
  v16 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int32_t)(v16));
  t2 = ((int32_t)(v9));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((uint64_t)(v1));
  t4 = ((uint64_t)(((uint64_t)(((int64_t)(v16))))));
  t5 = ((uint64_t)(v8));
  t6 = ((uint64_t)(((uint64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  v17 = ((uint64_t)(((uint64_t)((uint64_t)(t3) + (uint64_t)(t6)))));
  t7 = ((uint64_t)(v0));
  t8 = ((uint64_t)(v17));
  t9 = fg176r_ld_f32(t7, t8);
  v18 = ((float)(t9));
  t10 = ((float)(v18));
  t11 = fg176r_fabs(t10);
  v19 = ((float)(t11));
  t12 = ((float)(v14));
  t13 = ((float)(v19));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  v14 = ((float)(v19));
  v15 = ((float)(v18));
  }
  }
  t15 = v16;
  t16 = ((int32_t)(((int64_t)UINT64_C(1))));
  v16 = ((int32_t)((uint64_t)(t15) + (uint64_t)(t16)));
  }
  goto label1;
label2:;
  t17 = ((float)(v15));
  t18 = ((float)(v5));
  v20 = ((float)(((t17) / (t18))));
  v21 = ((float)(v4));
  t19 = ((float)(v20));
  t20 = ((float)(v4));
  t21 = ((t19) != (t20));
  if (t21) {
  {
  t22 = ((float)(v7));
  t23 = ((float)(v20));
  v21 = ((float)(((t22) / (t23))));
  }
  }
  t24 = ((uint64_t)(v2));
  t25 = ((uint64_t)(v3));
  t26 = ((float)(v20));
  fg176r_st_f16(t24, t25, t26);
  v22 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t27 = ((int32_t)(v22));
  t28 = ((int32_t)(v11));
  if (!(((t27) < (t28)))) goto label5;
  {
  t29 = ((uint64_t)(v1));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)(v22))))));
  t31 = ((uint64_t)(v8));
  t32 = ((uint64_t)(((uint64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  v23 = ((uint64_t)(((uint64_t)((uint64_t)(t29) + (uint64_t)(t32)))));
  t33 = ((uint64_t)(v1));
  t34 = ((int32_t)(v22));
  t35 = ((int32_t)(v10));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)(((int32_t)((uint64_t)(t34) + (uint64_t)(t35)))))))));
  t37 = ((uint64_t)(v8));
  t38 = ((uint64_t)(((uint64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  v24 = ((uint64_t)(((uint64_t)((uint64_t)(t33) + (uint64_t)(t38)))));
  t39 = ((uint64_t)(v0));
  t40 = ((uint64_t)(v23));
  t41 = fg176r_ld_f32(t39, t40);
  t42 = ((float)(t41));
  t43 = ((float)(v21));
  v25 = ((float)(((t42) * (t43))));
  t44 = ((uint64_t)(v0));
  t45 = ((uint64_t)(v24));
  t46 = fg176r_ld_f32(t44, t45);
  t47 = ((float)(t46));
  t48 = ((float)(v21));
  v26 = ((float)(((t47) * (t48))));
  t49 = ((float)(v25));
  t50 = ((float)(v6));
  t51 = ((float)(((t49) + (t50))));
  t52 = fg176r_i8trunc(t51);
  v27 = ((int32_t)(t52));
  t53 = ((float)(v26));
  t54 = ((float)(v6));
  t55 = ((float)(((t53) + (t54))));
  t56 = fg176r_i8trunc(t55);
  v28 = ((int32_t)(t56));
  t57 = ((int32_t)(v27));
  t58 = ((int32_t)(v12));
  t59 = ((t57) > (t58));
  if (t59) {
  {
  v29 = ((int32_t)(v12));
  }
  } else {
  {
  v29 = ((int32_t)(v27));
  }
  }
  t60 = ((int32_t)(v28));
  t61 = ((int32_t)(v12));
  t62 = ((t60) > (t61));
  if (t62) {
  {
  v30 = ((int32_t)(v12));
  }
  } else {
  {
  v30 = ((int32_t)(v28));
  }
  }
  t63 = ((int32_t)(v29));
  t64 = ((int32_t)(v13));
  v31 = ((uint32_t)(((uint32_t)(((t63) & (t64))))));
  t65 = ((int32_t)(v30));
  t66 = ((int32_t)(v13));
  v32 = ((uint32_t)(((uint32_t)(((t65) & (t66))))));
  t67 = ((uint32_t)(v32));
  t68 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t69 = ((uint32_t)(((uint32_t)((uint64_t)(t67) << ((uint64_t)(t68) & 31)))));
  t70 = ((uint32_t)(((uint32_t)(v13))));
  v33 = ((uint32_t)(((t69) & (t70))));
  t71 = ((uint64_t)(v2));
  t72 = ((uint64_t)(v3));
  t73 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  t74 = ((uint64_t)(((uint64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  t75 = ((uint64_t)(((uint64_t)(((int64_t)(v22))))));
  t76 = ((uint64_t)(((uint64_t)((uint64_t)(t74) + (uint64_t)(t75)))));
  t77 = ((uint32_t)(v31));
  t78 = ((uint32_t)(v33));
  t79 = ((uint32_t)(((t77) | (t78))));
  fg176r_st_u8(t71, t76, t79);
  t80 = v22;
  t81 = ((int32_t)(((int64_t)UINT64_C(1))));
  v22 = ((int32_t)((uint64_t)(t80) + (uint64_t)(t81)));
  }
  goto label4;
label5:;
  }
gg_done:;
}
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, gg_type_1 p4, gg_type_1 p5, gg_type_1 p6, gg_type_1 p7, gg_type_1 p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, uint8_t p19, uint8_t p20, int32_t p21, int32_t p22, int32_t p23, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint32_t t9;
  gg_type_1 t10;
  gg_type_1 t11;
  gg_type_1 t12;
  uint32_t t13;
  gg_type_1 t14;
  uint32_t t15;
  uint32_t t16;
  gg_type_1 t17;
  uint32_t t18;
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
  uint8_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  int64_t t34;
  uint64_t t35;
  int64_t t36;
  int64_t t37;
  uint64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
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
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint64_t t83;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  gg_type_1 v4;
  gg_type_1 v5;
  gg_type_1 v6;
  gg_type_1 v7;
  gg_type_1 v8;
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
  uint8_t v19;
  uint8_t v20;
  int32_t v21;
  int32_t v22;
  int32_t v23;
  int64_t v24;
  gg_type_8 v25;
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
  int64_t v36;
  int64_t v37;
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
  {
  t0 = ((int64_t)(((int64_t)(v23))));
  t1 = ((int64_t)(((int64_t)(v21))));
  t2 = ((int64_t)(((int64_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  t3 = ((int64_t)(((int64_t)(v22))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t4 = ((int64_t)(v24));
  t5 = ((int64_t)(v3));
  t6 = ((t4) >= (t5));
  if (t6) {
  {
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v24));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t9 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))))));
  t10 = v4;
  t11 = v5;
  t12 = v6;
  v25 = gg_function_2(t9, t10, t11, t12, base + 0);
  v26 = ((int64_t)((v25).f3));
  v27 = ((int64_t)((v25).f4));
  v28 = ((int64_t)((v25).f5));
  v29 = ((int64_t)((v25).f6));
  t13 = ((uint32_t)(((uint32_t)(v29))));
  t14 = v8;
  t15 = gg_function_5(t13, t14, base + 0);
  v30 = ((int64_t)(((int64_t)(t15))));
  t16 = ((uint32_t)(((uint32_t)(v28))));
  t17 = v7;
  t18 = gg_function_5(t16, t17, base + 0);
  v31 = ((int64_t)(((int64_t)(t18))));
  v32 = ((int64_t)(v27));
  fg176r_pdl_sync();
  t19 = ((int64_t)(v32));
  t20 = ((int64_t)(v12));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(v31));
  t23 = ((int64_t)(v13));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t24)))));
  t26 = ((int64_t)(v30));
  t27 = ((int64_t)(v14));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t28)))));
  t29 = v20;
  if (t29) {
  {
  t30 = ((uint64_t)(v1));
  t31 = ((int64_t)(v33));
  t32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t33 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))))));
  t34 = fg176r_ld_i64(t30, t33);
  v34 = ((int64_t)(t34));
  }
  } else {
  {
  t35 = ((uint64_t)(v1));
  t36 = ((int64_t)(v33));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t38 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))))));
  t39 = fg176r_ld_i32(t35, t38);
  v34 = ((int64_t)(t39));
  }
  }
  t40 = v19;
  if (t40) {
  {
  t41 = ((int64_t)(v34));
  t42 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t43 = ((t41) < (t42));
  t44 = t43;
  if (!t44) {
  t45 = ((int64_t)(v34));
  t46 = ((int64_t)(v18));
  t44 = ((t45) >= (t46));
  }
  t47 = t44;
  if (t47) {
  {
  fg176r_bad_index();
  goto gg_done;
  }
  }
  }
  }
  t48 = ((int64_t)(v34));
  t49 = ((int64_t)(v15));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t49)))));
  t51 = ((int64_t)(v28));
  t52 = ((int64_t)(v16));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t53)))));
  t55 = ((int64_t)(v29));
  t56 = ((int64_t)(v17));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t57)))));
  t59 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(18))))));
  v35 = ((int64_t)(((int64_t)gg_div(t58, t59))));
  t60 = ((int64_t)(v35));
  t61 = ((int64_t)(v26));
  t62 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t63 = ((int64_t)(((int64_t)gg_div(t61, t62))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(18))))));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))));
  t66 = ((int64_t)(v27));
  t67 = ((int64_t)(v9));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  t69 = ((int64_t)(v28));
  t70 = ((int64_t)(v10));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t71)))));
  t73 = ((int64_t)(v29));
  t74 = ((int64_t)(v11));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t75)))));
  t77 = ((int64_t)(v26));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t77)))));
  t79 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v37 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  t80 = ((uint64_t)(v0));
  t81 = ((uint64_t)(((uint64_t)(v37))));
  t82 = ((uint64_t)(v2));
  t83 = ((uint64_t)(((uint64_t)(v36))));
  gg_function_10(t80, t81, t82, t83, base + 0);
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  gg_type_1 t4;
  gg_type_1 t5;
  gg_type_1 t6;
  gg_type_1 t7;
  gg_type_1 t8;
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
  uint8_t t19;
  uint8_t t20;
  int32_t t21;
  int32_t t22;
  int32_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  int64_t t27;
  gg_type_1 t28;
  gg_type_1 t29;
  gg_type_1 t30;
  gg_type_1 t31;
  gg_type_1 t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int32_t t45;
  int32_t t46;
  int32_t t47;
  gg_type_1 v0;
  {
  memset(&(v0), 0, sizeof(v0));
  (v0).f0 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(1))))));
  (v0).f1 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
  (v0).f2 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(1))))));
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t1 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = v0;
  t5 = v0;
  t6 = v0;
  t7 = v0;
  t8 = v0;
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t19 = ((uint8_t)UINT64_C(1));
  t20 = ((uint8_t)UINT64_C(0));
  t21 = ((int32_t)(((int64_t)UINT64_C(0))));
  t22 = ((int32_t)(((int64_t)UINT64_C(0))));
  t23 = ((int32_t)(((int64_t)UINT64_C(256))));
  gg_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, t20, t21, t22, t23, base + 0);
  t24 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t25 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t26 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t28 = v0;
  t29 = v0;
  t30 = v0;
  t31 = v0;
  t32 = v0;
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t34 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t38 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t42 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t43 = ((uint8_t)UINT64_C(1));
  t44 = ((uint8_t)UINT64_C(0));
  t45 = ((int32_t)(((int64_t)UINT64_C(0))));
  t46 = ((int32_t)(((int64_t)UINT64_C(0))));
  t47 = ((int32_t)(((int64_t)UINT64_C(256))));
  gg_function_16(t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, t34, t35, t36, t37, t38, t39, t40, t41, t42, t43, t44, t45, t46, t47, base + 0);
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
