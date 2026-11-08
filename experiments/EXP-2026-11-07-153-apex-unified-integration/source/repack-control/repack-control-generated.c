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


typedef struct gg_type_2 gg_type_2;
#pragma pack(push, 1)
struct gg_type_2 {
  int64_t f0;
  int64_t f1;
  int64_t f2;
};
#pragma pack(pop)
typedef struct gg_type_5 gg_type_5;
#pragma pack(push, 1)
struct gg_type_5 {
  uint8_t f3;
  int64_t f4;
  int64_t f5;
  int64_t f6;
  int64_t f7;
};
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(int64_t a, int64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, int64_t value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint8_t value);
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_type_5 value);
static uint8_t gg_eq_6(int32_t a, int32_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, int32_t value);
static uint8_t gg_eq_7(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
if(!gg_eq_1(a.f0,b.f0))return 0;
if(!gg_eq_1(a.f1,b.f1))return 0;
if(!gg_eq_1(a.f2,b.f2))return 0;
return 1;
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
gg_append(out,"\120\154\141\156\040\173\040",7,1);
gg_render_1(out, value.f0);
gg_append(out,", ",2,1);
gg_render_1(out, value.f1);
gg_append(out,", ",2,1);
gg_render_1(out, value.f2);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_type_5*)a.data)[i],((gg_type_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
if(!gg_eq_3(a.f3,b.f3))return 0;
if(!gg_eq_1(a.f4,b.f4))return 0;
if(!gg_eq_1(a.f5,b.f5))return 0;
if(!gg_eq_1(a.f6,b.f6))return 0;
if(!gg_eq_1(a.f7,b.f7))return 0;
return 1;
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
gg_append(out,"\102\157\165\156\144\163\040\173\040",9,1);
gg_render_3(out, value.f3);
gg_append(out,", ",2,1);
gg_render_1(out, value.f4);
gg_append(out,", ",2,1);
gg_render_1(out, value.f5);
gg_append(out,", ",2,1);
gg_render_1(out, value.f6);
gg_append(out,", ",2,1);
gg_render_1(out, value.f7);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_7(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static gg_type_2 gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t base);
static gg_type_5 gg_function_3(gg_type_2 p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t base);
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t base);
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static uint8_t gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static uint8_t gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static int64_t gg_function_15(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base);
static void gg_function_22(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static void gg_function_32(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t base) {
  int64_t t0;
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
  int64_t t11;
  int64_t t12;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
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
  t11 = ((int64_t)(((int64_t)gg_div(t9, t10))));
  t12 = ((int64_t)(((int64_t)UINT64_C(292))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) >= (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t13) >= (t14));
  }
  t15 = t12;
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(256))));
  t19 = ((int64_t)(((int64_t)gg_mod(t17, t18))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t19) == (t20));
  }
  if (!(t16)) gg_fail("assertion failed");
  t21 = ((int64_t)(v5));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) == (t22));
  if (t23) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v6 = ((int64_t)(v5));
  }
  }
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t29 = ((int64_t)(v6));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t30 = ((int64_t)(v7));
  v8 = gg_function_8(t30, base + 0);
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t0 = ((int64_t)(v8));
  goto gg_done;
  }
  }
  t34 = ((int64_t)(v8));
  t35 = ((int64_t)(((int64_t)UINT64_C(8))));
  t36 = ((int64_t)(((int64_t)gg_mod(t34, t35))));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) != (t37));
  if (t38) {
  {
  t39 = ((int64_t)(v8));
  t40 = ((int64_t)(((int64_t)UINT64_C(8))));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  t42 = ((int64_t)(v8));
  t43 = ((int64_t)(((int64_t)UINT64_C(8))));
  t44 = ((int64_t)(((int64_t)gg_mod(t42, t43))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t41) - (uint64_t)(t44)))));
  }
  } else {
  {
  v9 = ((int64_t)(v8));
  }
  }
  t45 = ((int64_t)(v9));
  t46 = ((int64_t)(((int64_t)UINT64_C(8))));
  t47 = ((int64_t)(v4));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)(v3));
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t50)))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t51)))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t52)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
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
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) >= (t2)))) gg_fail("assertion failed");
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  t6 = t5;
  if (t6) {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(8))));
  t9 = ((int64_t)(((int64_t)gg_mod(t7, t8))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t9) == (t10));
  }
  t11 = t6;
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) != (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) != (t17));
  }
  t18 = t15;
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(8))));
  t22 = ((int64_t)(((int64_t)gg_mod(t20, t21))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t22) == (t23));
  }
  t24 = t19;
  if (t24) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t25) != (t26));
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(v4));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t29) != (t30));
  }
  t31 = t28;
  t32 = t31;
  if (t32) {
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(8))));
  t35 = ((int64_t)(((int64_t)gg_mod(t33, t34))));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((t35) == (t36));
  }
  t37 = t32;
  if (t37) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_32(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
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
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) >= (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) >= (t13));
  }
  t14 = t11;
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) > (t17));
  }
  if (!(t15)) gg_fail("assertion failed");
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t18 = ((int64_t)(v5));
  t19 = ((int64_t)(v3));
  if (!(((t18) < (t19)))) goto label2;
  {
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(4))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
label4:;
  t22 = ((int64_t)(v6));
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(((int64_t)UINT64_C(4))));
  t26 = ((int64_t)(((int64_t)gg_mod(t24, t25))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t23) - (uint64_t)(t26)))));
  if (!(((t22) < (t27)))) goto label5;
  {
  t28 = ((int64_t)(v5));
  t29 = ((int64_t)(v6));
  t30 = ((int64_t)(v4));
  fg006_rc_quant4(t28, t29, t30);
  t31 = v6;
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  v6 = ((int64_t)((uint64_t)(t31) + (uint64_t)(t34)));
  }
  goto label4;
label5:;
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)(v2));
  t37 = ((int64_t)(((int64_t)UINT64_C(4))));
  t38 = ((int64_t)(((int64_t)gg_mod(t36, t37))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t35) - (uint64_t)(t38)))));
  t39 = ((int64_t)(v7));
  t40 = ((int64_t)(v0));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
label7:;
  t41 = ((int64_t)(v8));
  t42 = ((int64_t)(v2));
  if (!(((t41) < (t42)))) goto label8;
  {
  t43 = ((int64_t)(v5));
  t44 = ((int64_t)(v8));
  t45 = ((int64_t)(v4));
  fg006_rc_quant1(t43, t44, t45);
  t46 = v8;
  t47 = ((int64_t)(v1));
  v8 = ((int64_t)((uint64_t)(t46) + (uint64_t)(t47)));
  }
  goto label7;
label8:;
  t48 = v5;
  t49 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t48) + (uint64_t)(t49)));
  }
  goto label1;
label2:;
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  t51 = ((int64_t)(v3));
  t52 = ((int64_t)(v2));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  fg006_rc_witness(t50, t53);
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
static int64_t gg_function_4(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)(((int64_t)gg_mod(t1, t2))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  if (t5) {
  {
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(8))));
  t11 = ((int64_t)(((int64_t)gg_mod(t9, t10))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t8) - (uint64_t)(t11)))));
  }
  } else {
  {
  t0 = ((int64_t)(v0));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_5 gg_function_3(gg_type_2 p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  gg_type_5 t0;
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
  gg_type_2 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)((v0).f0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)((v0).f0));
  t11 = ((int64_t)((v0).f1));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t8 = ((t9) < (t12));
  }
  if (!(t8)) gg_fail("assertion failed");
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)((v0).f0));
  v4 = ((int64_t)(((int64_t)gg_mod(t13, t14))));
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)((v0).f0));
  v5 = ((int64_t)(((int64_t)gg_div(t15, t16))));
  t17 = ((int64_t)((v0).f2));
  t18 = ((int64_t)(v4));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)((v0).f2));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t22 = ((int64_t)(v2));
  v7 = gg_function_2(t21, t22, base + 0);
  t23 = ((int64_t)(v5));
  t24 = ((int64_t)(v3));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  t28 = ((int64_t)(v3));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t29 = ((int64_t)(v6));
  v6 = gg_function_4(t29, base + 0);
  t30 = ((int64_t)(v7));
  v7 = gg_function_4(t30, base + 0);
  t31 = ((int64_t)(v7));
  t32 = ((int64_t)(v2));
  v7 = gg_function_2(t31, t32, base + 0);
  memset(&(t0), 0, sizeof(t0));
  t33 = ((int64_t)(v6));
  t34 = ((int64_t)(v7));
  (t0).f3 = ((t33) < (t34));
  (t0).f4 = ((int64_t)(v6));
  (t0).f5 = ((int64_t)(v7));
  (t0).f6 = ((int64_t)(v8));
  (t0).f7 = ((int64_t)(v9));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_2 gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  gg_type_2 t0;
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
  uint8_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
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
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) > (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) > (t10));
  }
  if (!(t8)) gg_fail("assertion failed");
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(4))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) - (uint64_t)(t16)))));
  t18 = ((int64_t)(v4));
  v5 = ((int64_t)(((int64_t)gg_div(t17, t18))));
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(v5));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) - (uint64_t)(t22)))));
  t24 = ((int64_t)(v5));
  v6 = ((int64_t)(((int64_t)gg_div(t23, t24))));
  v7 = ((int64_t)(((int64_t)UINT64_C(8))));
  t25 = ((int64_t)(v6));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t25) > (t26));
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(v6));
  t31 = ((int64_t)(((int64_t)gg_div(t29, t30))));
  t32 = ((int64_t)(v7));
  t28 = ((t31) < (t32));
  }
  t33 = t28;
  t34 = t33;
  if (t34) {
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(v7));
  t34 = ((t35) >= (t36));
  }
  t37 = t34;
  if (t37) {
  {
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(v7));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) - (uint64_t)(t41)))));
  t43 = ((int64_t)(v7));
  v6 = ((int64_t)(((int64_t)gg_div(t42, t43))));
  }
  }
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(v6));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)UINT64_C(1))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) - (uint64_t)(t47)))));
  t49 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)gg_div(t48, t49))));
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (!t53) {
  t54 = ((int64_t)(v6));
  t55 = ((int64_t)(v1));
  t56 = ((t54) < (t55));
  t57 = t56;
  if (!t57) {
  t58 = ((int64_t)(v3));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t58) != (t59));
  }
  t60 = t57;
  t61 = t60;
  if (t61) {
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(v1));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)UINT64_C(1))));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) - (uint64_t)(t65)))));
  t67 = ((int64_t)(v1));
  t68 = ((int64_t)(((int64_t)gg_div(t66, t67))));
  t69 = ((int64_t)(v7));
  t61 = ((t68) >= (t69));
  }
  t53 = t61;
  }
  t70 = t53;
  if (t70) {
  {
  v6 = ((int64_t)(v1));
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(v6));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
  t74 = ((int64_t)(((int64_t)UINT64_C(1))));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) - (uint64_t)(t74)))));
  t76 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)gg_div(t75, t76))));
  }
  }
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(v7));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  t80 = ((int64_t)(((int64_t)UINT64_C(1))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) - (uint64_t)(t80)))));
  t82 = ((int64_t)(v7));
  v9 = ((int64_t)(((int64_t)gg_div(t81, t82))));
  t83 = ((int64_t)(v6));
  t84 = ((int64_t)(v9));
  v6 = gg_function_2(t83, t84, base + 0);
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((int64_t)(v6));
  (t0).f1 = ((int64_t)(v2));
  (t0).f2 = ((int64_t)(v8));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_22(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) >= (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) >= (t13));
  }
  t14 = t11;
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) > (t17));
  }
  if (!(t15)) gg_fail("assertion failed");
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t18 = ((int64_t)(v5));
  t19 = ((int64_t)(v3));
  if (!(((t18) < (t19)))) goto label2;
  {
  v6 = ((int64_t)(v0));
label4:;
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(v2));
  if (!(((t20) < (t21)))) goto label5;
  {
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(v4));
  fg006_rc_quant1(t22, t23, t24);
  t25 = v6;
  t26 = ((int64_t)(v1));
  v6 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  }
  goto label4;
label5:;
  t27 = v5;
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t27) + (uint64_t)(t28)));
  }
  goto label1;
label2:;
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((int64_t)(v3));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  fg006_rc_witness(t29, t32);
  }
gg_done:;
}
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base) {
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
  uint8_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
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
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int32_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  uint8_t t51;
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
  uint8_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
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
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
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
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(v1));
  t4 = ((t5) < (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) > (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t13) > (t14));
  }
  t15 = t12;
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(v4));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t17) > (t18));
  }
  t19 = t16;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v5));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t21) > (t22));
  }
  t23 = t20;
  t24 = t23;
  if (t24) {
  t25 = ((int64_t)(v6));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t25) > (t26));
  }
  t27 = t24;
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(v7));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t29) >= (t30));
  }
  if (!(t28)) gg_fail("assertion failed");
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(v4));
  t35 = fg006_rc_k();
  t36 = ((int64_t)(t35));
  gg_function_22(t31, t32, t33, t34, t36, base + 0);
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t37) == (t38));
  if (t39) {
  {
  t40 = ((int64_t)(v5));
  fg006_rc_rows_clear(t40);
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t41 = ((int64_t)(v8));
  t42 = ((int64_t)(v7));
  if (!(((t41) < (t42)))) goto label2;
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t43 = ((int64_t)(v9));
  t44 = ((int64_t)(v6));
  if (!(((t43) < (t44)))) goto label5;
  {
  t45 = ((int64_t)(v8));
  t46 = ((int64_t)(v9));
  t47 = fg006_rc_ids_get(t45, t46);
  v10 = ((int64_t)(((int64_t)(t47))));
  t48 = ((int64_t)(v10));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((t48) >= (t49));
  t51 = t50;
  if (t51) {
  t52 = ((int64_t)(v10));
  t53 = ((int64_t)(v5));
  t51 = ((t52) < (t53));
  }
  if (!(t51)) gg_fail("assertion failed");
  t54 = ((int64_t)(v10));
  t55 = fg006_rc_row_count(t54);
  v11 = ((int64_t)(t55));
  t56 = ((int64_t)(v10));
  t57 = ((int64_t)(v11));
  t58 = ((int64_t)(v9));
  t59 = ((int64_t)(v8));
  fg006_rc_row_set(t56, t57, t58, t59);
  t60 = ((int64_t)(v10));
  fg006_rc_row_inc(t60);
  t61 = v9;
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t61) + (uint64_t)(t62)));
  }
  goto label4;
label5:;
  t63 = v8;
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t63) + (uint64_t)(t64)));
  }
  goto label1;
label2:;
  }
  }
  fg006_rc_barrier();
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t65 = ((int64_t)(v12));
  t66 = ((int64_t)(v5));
  if (!(((t65) < (t66)))) goto label8;
  {
  t67 = ((int64_t)(v12));
  t68 = fg006_rc_row_count(t67);
  v14 = ((int64_t)(t68));
  t69 = ((int64_t)(v14));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((t69) > (t70));
  if (t71) {
  {
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(v2));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  t75 = ((int64_t)(v1));
  v15 = ((int64_t)(((int64_t)gg_div(t74, t75))));
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t77)))));
  t79 = ((int64_t)(v2));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  t81 = ((int64_t)(v1));
  v16 = ((int64_t)(((int64_t)gg_div(t80, t81))));
  t82 = ((int64_t)(v15));
  v15 = gg_function_4(t82, base + 0);
  t83 = ((int64_t)(v16));
  v16 = gg_function_4(t83, base + 0);
  t84 = ((int64_t)(v16));
  t85 = ((int64_t)(v2));
  t86 = ((t84) > (t85));
  if (t86) {
  {
  v16 = ((int64_t)(v2));
  }
  }
  t87 = ((int64_t)(v15));
  t88 = ((int64_t)(v16));
  t89 = ((t87) < (t88));
  if (t89) {
  {
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t90 = ((int64_t)(v17));
  t91 = ((int64_t)(v14));
  if (!(((t90) < (t91)))) goto label11;
  {
  t92 = ((int64_t)(v12));
  t93 = ((int64_t)(v17));
  t94 = fg006_rc_row_i1(t92, t93);
  v18 = ((int64_t)(t94));
  t95 = ((int64_t)(v12));
  t96 = ((int64_t)(v17));
  t97 = fg006_rc_row_i2(t95, t96);
  v19 = ((int64_t)(t97));
  t98 = ((int64_t)(v18));
  t99 = ((int64_t)(v3));
  v20 = ((int64_t)(((int64_t)gg_mod(t98, t99))));
  t100 = ((int64_t)(v12));
  t101 = ((int64_t)(v18));
  t102 = ((int64_t)(v19));
  t103 = ((int64_t)(v20));
  t104 = ((int64_t)(v15));
  t105 = ((int64_t)(v16));
  fg006_rc_mmid_gemv(t100, t101, t102, t103, t104, t105);
  t106 = v13;
  t107 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)((uint64_t)(t106) + (uint64_t)(t107)));
  t108 = v17;
  t109 = ((int64_t)(((int64_t)UINT64_C(1))));
  v17 = ((int64_t)((uint64_t)(t108) + (uint64_t)(t109)));
  }
  goto label10;
label11:;
  }
  }
  }
  }
  t110 = v12;
  t111 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t110) + (uint64_t)(t111)));
  }
  goto label7;
label8:;
  t112 = ((int64_t)(((int64_t)UINT64_C(3))));
  t113 = ((int64_t)(v13));
  fg006_rc_witness(t112, t113);
  t0 = ((int64_t)(v13));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_15(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(v2));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t1) - (uint64_t)(t2)))));
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  t6 = ((int64_t)(((int64_t)gg_mod(t4, t5))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t3) - (uint64_t)(t6)))));
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) > (t8));
  if (t9) {
  {
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(v5));
  fg006_rc_chunk_gemm(t10, t11, t12, t13);
  }
  }
  v6 = ((int64_t)(v5));
label1:;
  t14 = ((int64_t)(v6));
  t15 = ((int64_t)(v4));
  if (!(((t14) < (t15)))) goto label2;
  {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(v6));
  fg006_rc_chunk_gemv(t16, t17, t18, t19);
  t20 = v6;
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t20) + (uint64_t)(t21)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  gg_type_2 t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  gg_type_2 v7;
  int64_t v8;
  int64_t v9;
  gg_type_5 v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v5));
  t4 = ((int64_t)(v6));
  v7 = gg_function_1(t1, t2, t3, t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)(v1));
  fg006_rc_chunk_set(t8);
  }
  }
  fg006_rc_barrier();
  v8 = ((int64_t)(v0));
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t9 = ((int64_t)(v8));
  t10 = ((int64_t)((v7).f0));
  t11 = ((int64_t)((v7).f1));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  if (!(((t9) < (t12)))) goto label2;
  {
  t13 = v7;
  t14 = ((int64_t)(v8));
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(v4));
  v10 = gg_function_3(t13, t14, t15, t16, base + 0);
  t17 = (v10).f3;
  if (t17) {
  {
  t18 = ((int64_t)((v10).f4));
  t19 = ((int64_t)((v10).f5));
  t20 = ((int64_t)((v10).f6));
  t21 = ((int64_t)((v10).f7));
  t22 = gg_function_15(t18, t19, t20, t21, base + 0);
  (void)(t22);
  t23 = v9;
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t23) + (uint64_t)(t24)));
  }
  }
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = fg006_rc_chunk_add(t25);
  v8 = ((int64_t)(t26));
  }
  goto label1;
label2:;
  t27 = ((int64_t)(((int64_t)UINT64_C(2))));
  t28 = ((int64_t)(v9));
  fg006_rc_witness(t27, t28);
  t0 = ((int64_t)(v9));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  uint8_t t0;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) == (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) != (t9));
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) != (t13));
  }
  t0 = t11;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
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
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) == (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) != (t10));
  }
  t11 = t8;
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) != (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = ((t16) == (t17));
  }
  t18 = t15;
  if (t18) {
  {
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = ((t19) != (t20));
  goto gg_done;
  }
  }
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) != (t22));
  t24 = t23;
  if (t24) {
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)UINT64_C(3))));
  t24 = ((t25) == (t26));
  }
  t27 = t24;
  if (t27) {
  {
  t28 = ((int64_t)(v6));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = ((t28) != (t29));
  goto gg_done;
  }
  }
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  int64_t t15;
  int64_t t16;
  uint8_t t17;
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
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((t8) == (t9));
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) != (t13));
  }
  t14 = t11;
  if (t14) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((t15) == (t16));
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(v3));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t19) != (t20));
  }
  t21 = t18;
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(3))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(3))));
  t24 = ((t22) == (t23));
  t25 = t24;
  if (t25) {
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t26) != (t27));
  }
  t28 = t25;
  if (t28) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(4))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
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
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  int64_t t78;
  int64_t t79;
  uint8_t t80;
  int64_t t81;
  int64_t t82;
  uint8_t t83;
  gg_type_2 t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
  uint8_t t91;
  int64_t t92;
  int64_t t93;
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  gg_type_2 v9;
  gg_type_2 v10;
  gg_type_5 v11;
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
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(v4));
  t8 = ((int64_t)(v5));
  t0 = gg_function_6(t4, t5, t6, t7, t8, base + 0);
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(v6));
  t0 = gg_function_7(t12, t13, t14, t15, t16, t17, base + 0);
  goto gg_done;
  }
  }
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((t18) == (t19));
  if (t20) {
  {
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(v5));
  t0 = gg_function_9(t21, t22, t23, t24, t25, base + 0);
  goto gg_done;
  }
  }
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(3))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(v2));
  t31 = ((int64_t)(v3));
  t32 = ((int64_t)(v4));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(v6));
  t35 = ((int64_t)(v7));
  t36 = gg_function_10(t29, t30, t31, t32, t33, t34, t35, base + 0);
  t37 = t36;
  if (t37) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
  }
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(4))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(v2));
  t43 = ((int64_t)(v3));
  t44 = ((int64_t)(v4));
  t45 = gg_function_11(t41, t42, t43, t44, base + 0);
  t46 = t45;
  if (t46) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
  }
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(5))));
  t49 = ((t47) == (t48));
  if (t49) {
  {
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(v2));
  t52 = ((int64_t)(v3));
  t53 = ((int64_t)(v4));
  t54 = ((int64_t)(v5));
  t55 = ((int64_t)(v6));
  t56 = ((int64_t)(v7));
  t0 = gg_function_12(t50, t51, t52, t53, t54, t55, t56, base + 0);
  goto gg_done;
  }
  }
  t57 = ((int64_t)(v0));
  t58 = ((int64_t)(((int64_t)UINT64_C(6))));
  t59 = ((t57) == (t58));
  if (t59) {
  {
  t60 = ((int64_t)(v1));
  t61 = ((int64_t)(v2));
  t62 = ((int64_t)(v3));
  t63 = ((int64_t)(v4));
  t64 = ((int64_t)(v5));
  t65 = ((int64_t)(v6));
  t66 = ((int64_t)(v7));
  t67 = ((int64_t)(v8));
  t0 = gg_function_20(t60, t61, t62, t63, t64, t65, t66, t67, base + 0);
  goto gg_done;
  }
  }
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(7))));
  t70 = ((t68) == (t69));
  if (t70) {
  {
  t71 = ((int64_t)(v1));
  t72 = ((int64_t)(v2));
  t73 = ((int64_t)(v3));
  t74 = ((int64_t)(v4));
  v9 = gg_function_1(t71, t72, t73, t74, base + 0);
  t75 = ((int64_t)(v5));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = ((t75) == (t76));
  if (t77) {
  {
  t0 = ((int64_t)((v9).f0));
  goto gg_done;
  }
  }
  t78 = ((int64_t)(v5));
  t79 = ((int64_t)(((int64_t)UINT64_C(1))));
  t80 = ((t78) == (t79));
  if (t80) {
  {
  t0 = ((int64_t)((v9).f1));
  goto gg_done;
  }
  }
  t0 = ((int64_t)((v9).f2));
  goto gg_done;
  }
  }
  t81 = ((int64_t)(v0));
  t82 = ((int64_t)(((int64_t)UINT64_C(8))));
  t83 = ((t81) == (t82));
  if (t83) {
  {
  memset(&(v10), 0, sizeof(v10));
  (v10).f0 = ((int64_t)(v1));
  (v10).f1 = ((int64_t)(v2));
  (v10).f2 = ((int64_t)(v3));
  t84 = v10;
  t85 = ((int64_t)(v4));
  t86 = ((int64_t)(v5));
  t87 = ((int64_t)(v6));
  v11 = gg_function_3(t84, t85, t86, t87, base + 0);
  t88 = ((int64_t)(v7));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((t88) == (t89));
  if (t90) {
  {
  t91 = (v11).f3;
  if (t91) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
  }
  t92 = ((int64_t)(v7));
  t93 = ((int64_t)(((int64_t)UINT64_C(1))));
  t94 = ((t92) == (t93));
  if (t94) {
  {
  t0 = ((int64_t)((v11).f4));
  goto gg_done;
  }
  }
  t95 = ((int64_t)(v7));
  t96 = ((int64_t)(((int64_t)UINT64_C(2))));
  t97 = ((t95) == (t96));
  if (t97) {
  {
  t0 = ((int64_t)((v11).f5));
  goto gg_done;
  }
  }
  t98 = ((int64_t)(v7));
  t99 = ((int64_t)(((int64_t)UINT64_C(3))));
  t100 = ((t98) == (t99));
  if (t100) {
  {
  t0 = ((int64_t)((v11).f6));
  goto gg_done;
  }
  }
  t0 = ((int64_t)((v11).f7));
  goto gg_done;
  }
  }
  t101 = ((int64_t)(v0));
  t102 = ((int64_t)(((int64_t)UINT64_C(9))));
  if (!(((t101) == (t102)))) gg_fail("assertion failed");
  t103 = ((int64_t)(v1));
  t104 = ((int64_t)(v2));
  t105 = ((int64_t)(v3));
  t106 = ((int64_t)(v4));
  t107 = ((int64_t)(v5));
  gg_function_32(t103, t104, t105, t106, t107, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
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
  gg_type_2 t4;
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
  gg_header t17;
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
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  gg_type_2 v0;
  gg_type_5 v1;
  int64_t v2;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  v0 = gg_function_1(t0, t1, t2, t3, base + 0);
  t4 = v0;
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(8))));
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  v1 = gg_function_3(t4, t5, t6, t7, base + 0);
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  v2 = gg_function_5(t8, t9, t10, t11, t12, t13, t14, t15, t16, base + 0);
  t17.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t17.len = 0;
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = gg_function_6(t18, t19, t20, t21, t22, base + 1);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(((int64_t)UINT64_C(512))));
  t27 = ((int64_t)(((int64_t)UINT64_C(4))));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = gg_function_7(t25, t26, t27, t28, t29, t30, base + 1);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t32)))));
  t35 = (v1).f3;
  if (t35) {
  {
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t36 = ((int64_t)(t34));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t36)))));
  t38 = ((int64_t)(v2));
  gg_integer(gg_ref(&t17, gg_get_stack(base + 0)), ((int64_t)((uint64_t)(t37) + (uint64_t)(t38))));
  gg_append(gg_ref(&t17, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t17).base, (t17).len));
  }
gg_done:;
  for (t39 = 0; t39 < 1; t39++) gg_stacks[base + t39].top = 0;
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
