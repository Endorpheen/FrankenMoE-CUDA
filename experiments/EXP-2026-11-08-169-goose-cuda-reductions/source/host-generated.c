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


typedef struct gg_type_5 gg_type_5;
#pragma pack(push, 1)
struct gg_type_5 {
  uint8_t f0;
  int32_t f1;
  int64_t f2;
  int64_t f3;
};
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(int64_t a, int64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, int64_t value);
static uint8_t gg_eq_2(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint8_t value);
static uint8_t gg_eq_3(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint64_t value);
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
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_type_5*)a.data)[i],((gg_type_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
if(!gg_eq_2(a.f0,b.f0))return 0;
if(!gg_eq_6(a.f1,b.f1))return 0;
if(!gg_eq_1(a.f2,b.f2))return 0;
if(!gg_eq_1(a.f3,b.f3))return 0;
return 1;
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
gg_append(out,"\114\141\165\156\143\150\040\173\040",9,1);
gg_render_2(out, value.f0);
gg_append(out,", ",2,1);
gg_render_6(out, value.f1);
gg_append(out,", ",2,1);
gg_render_1(out, value.f2);
gg_append(out,", ",2,1);
gg_render_1(out, value.f3);
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
static void gg_function_1(int64_t p0, int64_t base);
static void gg_function_4(uint8_t p0, int64_t p1, int64_t base);
static uint64_t gg_function_8(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t gg_function_9(int64_t base);
static uint64_t gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static uint64_t gg_function_12(uint64_t p0, uint64_t p1, int64_t base);
static gg_type_5 gg_function_20(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_21(int64_t p0, int64_t p1, int64_t base);
static int32_t gg_function_22(int64_t p0, int64_t base);
static int64_t gg_function_23(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_24(int64_t p0, int64_t base);
static int64_t gg_function_25(int32_t p0, int64_t base);
static int32_t gg_function_26(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int32_t gg_function_27(int32_t p0, int64_t base);
static int32_t gg_function_27(int32_t p0, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  uint8_t t3;
  int32_t v0;
  v0 = p0;
  {
  t1 = ((int32_t)(v0));
  t2 = ((int32_t)(((int64_t)UINT64_C(32))));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((int32_t)(((int64_t)UINT64_C(32))));
  goto gg_done;
  }
  }
  t0 = ((int32_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int32_t gg_function_26(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int32_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  t5 = ((t3) < (t4));
  if (t5) {
  {
  t0 = ((int32_t)(((int64_t)UINT64_C(512))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1024))));
  t8 = ((t6) < (t7));
  if (t8) {
  {
  t0 = ((int32_t)(((int64_t)UINT64_C(32))));
  goto gg_done;
  }
  }
  t0 = ((int32_t)(((int64_t)UINT64_C(128))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_4(uint8_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  uint8_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(v1));
  fg169_fail(t2);
  }
  }
  }
gg_done:;
}
static int64_t gg_function_25(int32_t p0, int64_t base) {
  int64_t t0;
  int32_t t1;
  int32_t t2;
  uint8_t t3;
  int32_t v0;
  v0 = p0;
  {
  t1 = ((int32_t)(v0));
  t2 = ((int32_t)(((int64_t)UINT64_C(32))));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_24(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_23(int64_t p0, int64_t p1, int64_t base) {
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
static int32_t gg_function_22(int64_t p0, int64_t base) {
  int32_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(1024))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(31))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = ((int64_t)(((int64_t)UINT64_C(32))));
  t6 = ((int64_t)(((int64_t)gg_div(t4, t5))));
  t7 = ((int64_t)(((int64_t)UINT64_C(32))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = gg_function_23(t1, t8, base + 0);
  t0 = ((int32_t)(((int32_t)(t9))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_21(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t14;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  if (t7) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t10 = ((t8) > (t9));
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t11 = ((t12) > (t13));
  }
  t14 = t11;
  if (t14) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_5 gg_function_20(int64_t p0, int64_t p1, int64_t base) {
  gg_type_5 t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int32_t t8;
  int64_t v0;
  int64_t v1;
  int32_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = gg_function_21(t1, t2, base + 0);
  t4 = t3;
  t5 = (!(t4));
  if (t5) {
  {
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((uint8_t)UINT64_C(0));
  (t0).f1 = ((int32_t)(((int64_t)UINT64_C(0))));
  (t0).f2 = ((int64_t)(((int64_t)UINT64_C(0))));
  (t0).f3 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  v2 = gg_function_22(t6, base + 0);
  memset(&(t0), 0, sizeof(t0));
  (t0).f0 = ((uint8_t)UINT64_C(1));
  (t0).f1 = ((int32_t)(v2));
  t7 = ((int64_t)(v1));
  (t0).f2 = gg_function_24(t7, base + 0);
  t8 = ((int32_t)(v2));
  (t0).f3 = gg_function_25(t8, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_9(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_8(uint64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  uint8_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  int64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((uint64_t)(v1));
  t6 = gg_function_9(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_function_4(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_12(uint64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint8_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = gg_function_9(base + 0);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(v1));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((t1) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_4(t6, t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  uint64_t t0;
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
  uint8_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
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
  uint8_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  uint8_t t46;
  uint64_t t47;
  uint64_t t48;
  uint64_t t49;
  uint64_t t50;
  uint8_t t51;
  int64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  uint64_t t58;
  uint64_t t59;
  uint64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint8_t t81;
  uint8_t t82;
  uint64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint64_t t86;
  uint64_t t87;
  uint8_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
  uint8_t t97;
  uint64_t t98;
  uint64_t t99;
  uint8_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint64_t t107;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  uint64_t v5;
  uint64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg169_ti(t5, t6, t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(v3));
  t4 = ((t9) == (t10));
  }
  t11 = t4;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg169_ti(t13, t14, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t17) != (t18));
  }
  t19 = t12;
  t20 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_4(t19, t20, base + 0);
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg169_ti(t21, t22, t23);
  v4 = ((uint64_t)(((uint64_t)(t24))));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((int64_t)(v2));
  t28 = ((uint64_t)(v4));
  fg169_hput(t25, t26, t27, t28);
  v5 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v6 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t29 = ((int64_t)(v7));
  t30 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t29) < (t30)))) goto label2;
  {
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  t33 = ((int64_t)(v7));
  t34 = fg169_ti(t31, t32, t33);
  v8 = ((int64_t)(t34));
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(((int64_t)UINT64_C(4))));
  t37 = ((int64_t)(v7));
  t38 = fg169_ti(t35, t36, t37);
  v9 = ((int64_t)(t38));
  t39 = ((int64_t)(v8));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((t39) > (t40));
  t42 = t41;
  if (t42) {
  t43 = ((int64_t)(v9));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t43) >= (t44));
  }
  t45 = t42;
  t46 = t45;
  if (t46) {
  t47 = ((uint64_t)(((uint64_t)(v9))));
  t48 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t49 = ((uint64_t)(((uint64_t)gg_umod(t47, t48))));
  t50 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t46 = ((t49) == (t50));
  }
  t51 = t46;
  t52 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_4(t51, t52, base + 0);
  t53 = ((uint64_t)(v5));
  t54 = ((uint64_t)(((uint64_t)(v8))));
  v5 = gg_function_8(t53, t54, base + 0);
  t55 = ((uint64_t)(v6));
  t56 = ((int64_t)(v8));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  t58 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t56) - (uint64_t)(t57)))))));
  t59 = ((uint64_t)(((uint64_t)(v9))));
  t60 = gg_function_8(t58, t59, base + 0);
  v6 = gg_function_12(t55, t60, base + 0);
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t63 = ((int64_t)(v2));
  t64 = ((int64_t)(((int64_t)UINT64_C(4))));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  t66 = ((int64_t)(v7));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = ((uint64_t)(((uint64_t)(v8))));
  fg169_hput(t61, t62, t67, t68);
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(((int64_t)UINT64_C(2))));
  t71 = ((int64_t)(v2));
  t72 = ((int64_t)(((int64_t)UINT64_C(4))));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) * (uint64_t)(t72)))));
  t74 = ((int64_t)(v7));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t74)))));
  t76 = ((uint64_t)(((uint64_t)(v9))));
  fg169_hput(t69, t70, t75, t76);
  t77 = v7;
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t77) + (uint64_t)(t78)));
  }
  goto label1;
label2:;
  t79 = ((uint64_t)(v6));
  t80 = ((uint64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t81 = ((t79) <= (t80));
  t82 = t81;
  if (t82) {
  t83 = ((uint64_t)(v4));
  t84 = gg_function_9(base + 0);
  t85 = ((uint64_t)(t84));
  t86 = ((uint64_t)(v6));
  t87 = ((uint64_t)(((uint64_t)((uint64_t)(t85) - (uint64_t)(t86)))));
  t82 = ((t83) <= (t87));
  }
  t88 = t82;
  t89 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_4(t88, t89, base + 0);
  t90 = ((int64_t)(v1));
  t91 = ((int64_t)(((int64_t)UINT64_C(9))));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = fg169_ti(t90, t91, t92);
  v10 = ((int64_t)(t93));
  t94 = ((int64_t)(v10));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = ((t94) > (t95));
  t97 = t96;
  if (t97) {
  t98 = ((uint64_t)(v6));
  t99 = ((uint64_t)(((uint64_t)(v10))));
  t97 = ((t98) <= (t99));
  }
  t100 = t97;
  t101 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_4(t100, t101, base + 0);
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(((int64_t)UINT64_C(3))));
  t104 = ((int64_t)(((int64_t)UINT64_C(12))));
  t105 = ((int64_t)(v2));
  t106 = ((int64_t)(((int64_t)((uint64_t)(t104) + (uint64_t)(t105)))));
  t107 = ((uint64_t)(v6));
  fg169_hput(t102, t103, t106, t107);
  t0 = ((uint64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_1(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  uint64_t t37;
  uint8_t t38;
  uint8_t t39;
  uint64_t t40;
  uint64_t t41;
  uint8_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  uint8_t t67;
  uint64_t t68;
  uint64_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint64_t t86;
  uint64_t t87;
  uint64_t t88;
  uint64_t t89;
  uint64_t t90;
  uint8_t t91;
  uint8_t t92;
  uint64_t t93;
  uint64_t t94;
  uint64_t t95;
  uint64_t t96;
  uint64_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  uint8_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
  int64_t t121;
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
  int64_t t138;
  uint8_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  uint8_t t143;
  uint8_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  uint64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  uint64_t t153;
  uint64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  uint64_t t158;
  uint64_t t159;
  uint64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  uint64_t t164;
  uint64_t t165;
  int64_t t166;
  int64_t t167;
  uint8_t t168;
  int64_t t169;
  int64_t t170;
  uint8_t t171;
  uint8_t t172;
  uint8_t t173;
  uint64_t t174;
  uint64_t t175;
  uint8_t t176;
  int64_t t177;
  uint64_t t178;
  uint64_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  uint8_t t184;
  int64_t t185;
  int64_t t186;
  uint8_t t187;
  uint8_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  uint8_t t192;
  uint8_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  uint64_t t198;
  int64_t t199;
  int64_t t200;
  uint8_t t201;
  uint8_t t202;
  uint64_t t203;
  uint64_t t204;
  uint64_t t205;
  uint8_t t206;
  uint8_t t207;
  uint64_t t208;
  uint64_t t209;
  uint8_t t210;
  uint8_t t211;
  uint64_t t212;
  uint64_t t213;
  uint8_t t214;
  uint8_t t215;
  uint64_t t216;
  uint64_t t217;
  uint8_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  uint64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  uint64_t t227;
  uint64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  uint64_t t232;
  uint64_t t233;
  uint64_t t234;
  uint64_t t235;
  uint8_t t236;
  uint8_t t237;
  uint64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  uint64_t t242;
  uint64_t t243;
  uint64_t t244;
  uint64_t t245;
  uint8_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  uint64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  uint64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  uint64_t t259;
  int64_t t260;
  int64_t t261;
  uint8_t t262;
  uint8_t t263;
  int64_t t264;
  int64_t t265;
  uint8_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  uint8_t t274;
  uint8_t t275;
  uint64_t t276;
  uint64_t t277;
  uint8_t t278;
  uint8_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  uint8_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  uint8_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  uint8_t t297;
  uint8_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  uint8_t t304;
  uint8_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  uint8_t t311;
  uint8_t t312;
  uint64_t t313;
  uint64_t t314;
  uint8_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  uint64_t t319;
  int64_t t320;
  uint64_t t321;
  uint64_t t322;
  uint64_t t323;
  uint8_t t324;
  uint8_t t325;
  uint64_t t326;
  uint64_t t327;
  uint8_t t328;
  int64_t t329;
  int64_t t330;
  uint64_t t331;
  uint64_t t332;
  uint64_t t333;
  uint64_t t334;
  uint8_t t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  uint64_t t340;
  uint64_t t341;
  uint64_t t342;
  uint8_t t343;
  int64_t t344;
  uint64_t t345;
  int64_t t346;
  int64_t t347;
  uint64_t t348;
  int64_t t349;
  int64_t t350;
  uint8_t t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  uint64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  uint64_t t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  uint64_t t363;
  int64_t t364;
  int64_t t365;
  uint64_t t366;
  uint64_t t367;
  uint64_t t368;
  uint8_t t369;
  int64_t t370;
  int64_t t371;
  int64_t t372;
  uint8_t t373;
  int64_t t374;
  int64_t t375;
  uint8_t t376;
  int64_t t377;
  int64_t t378;
  int64_t t379;
  int64_t t380;
  uint64_t t381;
  int64_t t382;
  int64_t t383;
  int64_t t384;
  uint64_t t385;
  int64_t t386;
  int64_t t387;
  int64_t t388;
  uint64_t t389;
  int64_t t390;
  int64_t t391;
  int64_t t392;
  int64_t t393;
  int64_t t394;
  uint8_t t395;
  int64_t t396;
  int64_t t397;
  int64_t t398;
  int64_t t399;
  int64_t t400;
  int64_t t401;
  int64_t t402;
  uint64_t t403;
  int64_t t404;
  int64_t t405;
  int64_t t406;
  int32_t t407;
  int32_t t408;
  uint64_t t409;
  uint64_t t410;
  uint64_t t411;
  int64_t t412;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
  uint64_t v8;
  uint64_t v9;
  uint64_t v10;
  uint64_t v11;
  int64_t v12;
  int64_t v13;
  uint64_t v14;
  uint64_t v15;
  uint64_t v16;
  uint8_t v17;
  uint64_t v18;
  uint8_t v19;
  uint64_t v20;
  uint64_t v21;
  gg_type_5 v22;
  int64_t v23;
  int32_t v24;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg169_arg(t0, t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = fg169_plan_ptr(t3);
  v2 = ((int64_t)(t4));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) >= (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(5))));
  t8 = ((t9) <= (t10));
  }
  t11 = t8;
  t12 = ((int64_t)(((int64_t)UINT64_C(7))));
  gg_function_4(t11, t12, base + 0);
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((uint64_t)(((uint64_t)(v1))));
  fg169_hput(t13, t14, t15, t16);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = fg169_constant(t17, t18);
  v3 = ((int64_t)(t19));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = fg169_constant(t20, t21);
  v4 = ((int64_t)(t22));
  v5 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v6 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v7 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(3))));
  t25 = ((t23) == (t24));
  t26 = t25;
  if (!t26) {
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(((int64_t)UINT64_C(5))));
  t26 = ((t27) == (t28));
  }
  t29 = t26;
  if (t29) {
  {
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(3))));
  t32 = fg169_arg(t30, t31);
  v8 = ((uint64_t)(((uint64_t)(t32))));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(4))));
  t35 = fg169_arg(t33, t34);
  v9 = ((uint64_t)(((uint64_t)(t35))));
  t36 = ((uint64_t)(v8));
  t37 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) != (t37));
  t39 = t38;
  if (t39) {
  t40 = ((uint64_t)(v9));
  t41 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t40) != (t41));
  }
  t42 = t39;
  t43 = t42;
  if (t43) {
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(5))));
  t46 = fg169_arg(t44, t45);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t47) > (t48));
  }
  t49 = t43;
  t50 = t49;
  if (t50) {
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(6))));
  t53 = fg169_arg(t51, t52);
  t54 = ((int64_t)(t53));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((t54) > (t55));
  }
  t56 = t50;
  t57 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_function_4(t56, t57, base + 0);
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(((int64_t)UINT64_C(5))));
  t60 = fg169_arg(t58, t59);
  v5 = ((uint64_t)(((uint64_t)(t60))));
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(6))));
  t63 = fg169_arg(t61, t62);
  v6 = ((uint64_t)(((uint64_t)(t63))));
  t64 = ((int64_t)(v1));
  t65 = ((int64_t)(((int64_t)UINT64_C(5))));
  t66 = ((t64) != (t65));
  t67 = t66;
  if (!t67) {
  t68 = ((uint64_t)(v6));
  t69 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t67 = ((t68) == (t69));
  }
  t70 = t67;
  t71 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_function_4(t70, t71, base + 0);
  t72 = ((int64_t)(v2));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((uint64_t)(v8));
  fg169_hput(t72, t73, t74, t75);
  t76 = ((int64_t)(v2));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  t79 = ((uint64_t)(v9));
  fg169_hput(t76, t77, t78, t79);
  t80 = ((uint64_t)(v5));
  t81 = ((uint64_t)(v6));
  t82 = gg_function_8(t80, t81, base + 0);
  t83 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  v10 = gg_function_8(t82, t83, base + 0);
  t84 = ((uint64_t)(v6));
  t85 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  v11 = gg_function_8(t84, t85, base + 0);
  t86 = ((uint64_t)(v8));
  t87 = gg_function_9(base + 0);
  t88 = ((uint64_t)(t87));
  t89 = ((uint64_t)(v10));
  t90 = ((uint64_t)(((uint64_t)((uint64_t)(t88) - (uint64_t)(t89)))));
  t91 = ((t86) <= (t90));
  t92 = t91;
  if (t92) {
  t93 = ((uint64_t)(v9));
  t94 = gg_function_9(base + 0);
  t95 = ((uint64_t)(t94));
  t96 = ((uint64_t)(v11));
  t97 = ((uint64_t)(((uint64_t)((uint64_t)(t95) - (uint64_t)(t96)))));
  t92 = ((t93) <= (t97));
  }
  t98 = t92;
  t99 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_4(t98, t99, base + 0);
  t100 = ((int64_t)(v2));
  t101 = ((int64_t)(((int64_t)UINT64_C(3))));
  t102 = ((int64_t)(((int64_t)UINT64_C(12))));
  t103 = ((uint64_t)(v10));
  fg169_hput(t100, t101, t102, t103);
  t104 = ((int64_t)(v2));
  t105 = ((int64_t)(((int64_t)UINT64_C(3))));
  t106 = ((int64_t)(((int64_t)UINT64_C(13))));
  t107 = ((uint64_t)(v11));
  fg169_hput(t104, t105, t106, t107);
  }
  } else {
  {
  t108 = ((int64_t)(v0));
  t109 = ((int64_t)(((int64_t)UINT64_C(1))));
  t110 = fg169_arg(t108, t109);
  t111 = ((int64_t)(t110));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = ((t111) != (t112));
  t114 = t113;
  if (t114) {
  t115 = ((int64_t)(v0));
  t116 = ((int64_t)(((int64_t)UINT64_C(2))));
  t117 = fg169_arg(t115, t116);
  t118 = ((int64_t)(t117));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((t118) != (t119));
  }
  t120 = t114;
  t121 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_4(t120, t121, base + 0);
  t122 = ((int64_t)(v0));
  t123 = ((int64_t)(((int64_t)UINT64_C(2))));
  t124 = fg169_arg(t122, t123);
  v12 = ((int64_t)(t124));
  t125 = ((int64_t)(v12));
  t126 = ((int64_t)(((int64_t)UINT64_C(0))));
  t127 = ((int64_t)(((int64_t)UINT64_C(0))));
  t128 = fg169_ti(t125, t126, t127);
  v13 = ((int64_t)(t128));
  t129 = ((int64_t)(v2));
  t130 = ((int64_t)(v13));
  t131 = ((int64_t)(((int64_t)UINT64_C(0))));
  t132 = ((int64_t)(v3));
  v7 = gg_function_11(t129, t130, t131, t132, base + 0);
  t133 = ((int64_t)(v2));
  t134 = ((int64_t)(v12));
  t135 = ((int64_t)(((int64_t)UINT64_C(1))));
  t137 = ((int64_t)(v1));
  t138 = ((int64_t)(((int64_t)UINT64_C(2))));
  t139 = ((t137) == (t138));
  if (t139) {
  {
  t136 = ((int64_t)(v4));
  }
  } else {
  {
  t136 = ((int64_t)(v3));
  }
  }
  t140 = ((int64_t)(t136));
  v14 = gg_function_11(t133, t134, t135, t140, base + 0);
  t141 = ((int64_t)(((int64_t)UINT64_C(0))));
  t142 = ((int64_t)(v12));
  t143 = fg169_pred(t141, t142);
  t144 = t143;
  t145 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_function_4(t144, t145, base + 0);
  t146 = ((int64_t)(v2));
  t147 = ((int64_t)(((int64_t)UINT64_C(1))));
  t148 = ((int64_t)(((int64_t)UINT64_C(0))));
  t149 = fg169_hget(t146, t147, t148);
  v5 = ((uint64_t)(t149));
  t150 = ((int64_t)(v2));
  t151 = ((int64_t)(((int64_t)UINT64_C(1))));
  t152 = ((int64_t)(((int64_t)UINT64_C(1))));
  t153 = fg169_hget(t150, t151, t152);
  t154 = ((uint64_t)(t153));
  t155 = ((int64_t)(v2));
  t156 = ((int64_t)(((int64_t)UINT64_C(1))));
  t157 = ((int64_t)(((int64_t)UINT64_C(2))));
  t158 = fg169_hget(t155, t156, t157);
  t159 = ((uint64_t)(t158));
  t160 = gg_function_8(t154, t159, base + 0);
  t161 = ((int64_t)(v2));
  t162 = ((int64_t)(((int64_t)UINT64_C(1))));
  t163 = ((int64_t)(((int64_t)UINT64_C(3))));
  t164 = fg169_hget(t161, t162, t163);
  t165 = ((uint64_t)(t164));
  v6 = gg_function_8(t160, t165, base + 0);
  t166 = ((int64_t)(v1));
  t167 = ((int64_t)(((int64_t)UINT64_C(4))));
  t168 = ((t166) == (t167));
  if (t168) {
  {
  t169 = ((int64_t)(((int64_t)UINT64_C(2))));
  t170 = ((int64_t)(v13));
  t171 = fg169_pred(t169, t170);
  t172 = t171;
  t173 = t172;
  if (t173) {
  t174 = ((uint64_t)(v14));
  t175 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t173 = ((t174) == (t175));
  }
  t176 = t173;
  t177 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_4(t176, t177, base + 0);
  v5 = ((uint64_t)(v7));
  v6 = ((uint64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t178 = ((uint64_t)(v14));
  t179 = ((uint64_t)(v6));
  t180 = ((t178) == (t179));
  t181 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_function_4(t180, t181, base + 0);
  t182 = ((int64_t)(v1));
  t183 = ((int64_t)(((int64_t)UINT64_C(2))));
  t184 = ((t182) == (t183));
  if (t184) {
  {
  t185 = ((int64_t)(((int64_t)UINT64_C(0))));
  t186 = ((int64_t)(v13));
  t187 = fg169_pred(t185, t186);
  t188 = t187;
  t189 = ((int64_t)(((int64_t)UINT64_C(15))));
  gg_function_4(t188, t189, base + 0);
  }
  } else {
  {
  t190 = ((int64_t)(((int64_t)UINT64_C(1))));
  t191 = ((int64_t)(v13));
  t192 = fg169_pred(t190, t191);
  t193 = t192;
  t194 = ((int64_t)(((int64_t)UINT64_C(16))));
  gg_function_4(t193, t194, base + 0);
  t195 = ((int64_t)(v2));
  t196 = ((int64_t)(((int64_t)UINT64_C(3))));
  t197 = ((int64_t)(((int64_t)UINT64_C(6))));
  t199 = ((int64_t)(((int64_t)UINT64_C(0))));
  t200 = ((int64_t)(v13));
  t201 = fg169_pred(t199, t200);
  t202 = t201;
  if (t202) {
  {
  t198 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t198 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t203 = ((uint64_t)(t198));
  fg169_hput(t195, t196, t197, t203);
  }
  }
  }
  }
  }
  }
  t204 = ((uint64_t)(v5));
  t205 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t206 = ((t204) > (t205));
  t207 = t206;
  if (t207) {
  t208 = ((uint64_t)(v6));
  t209 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t207 = ((t208) > (t209));
  }
  t210 = t207;
  t211 = t210;
  if (t211) {
  t212 = ((uint64_t)(v5));
  t213 = ((uint64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t211 = ((t212) <= (t213));
  }
  t214 = t211;
  t215 = t214;
  if (t215) {
  t216 = ((uint64_t)(v6));
  t217 = ((uint64_t)(((int64_t)UINT64_C(2147483647))));
  t215 = ((t216) <= (t217));
  }
  t218 = t215;
  t219 = ((int64_t)(((int64_t)UINT64_C(17))));
  gg_function_4(t218, t219, base + 0);
  t220 = ((int64_t)(v2));
  t221 = ((int64_t)(((int64_t)UINT64_C(0))));
  t222 = ((int64_t)(((int64_t)UINT64_C(0))));
  t223 = fg169_hget(t220, t221, t222);
  v15 = ((uint64_t)(t223));
  t224 = ((int64_t)(v2));
  t225 = ((int64_t)(((int64_t)UINT64_C(0))));
  t226 = ((int64_t)(((int64_t)UINT64_C(1))));
  t227 = fg169_hget(t224, t225, t226);
  v16 = ((uint64_t)(t227));
  t228 = ((uint64_t)(v15));
  t229 = ((int64_t)(v2));
  t230 = ((int64_t)(((int64_t)UINT64_C(3))));
  t231 = ((int64_t)(((int64_t)UINT64_C(12))));
  t232 = fg169_hget(t229, t230, t231);
  t233 = ((uint64_t)(t232));
  t234 = ((uint64_t)(((uint64_t)((uint64_t)(t228) + (uint64_t)(t233)))));
  t235 = ((uint64_t)(v16));
  t236 = ((t234) <= (t235));
  t237 = t236;
  if (!t237) {
  t238 = ((uint64_t)(v16));
  t239 = ((int64_t)(v2));
  t240 = ((int64_t)(((int64_t)UINT64_C(3))));
  t241 = ((int64_t)(((int64_t)UINT64_C(13))));
  t242 = fg169_hget(t239, t240, t241);
  t243 = ((uint64_t)(t242));
  t244 = ((uint64_t)(((uint64_t)((uint64_t)(t238) + (uint64_t)(t243)))));
  t245 = ((uint64_t)(v15));
  t237 = ((t244) <= (t245));
  }
  t246 = t237;
  t247 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_4(t246, t247, base + 0);
  t248 = ((int64_t)(v2));
  t249 = ((int64_t)(((int64_t)UINT64_C(3))));
  t250 = ((int64_t)(((int64_t)UINT64_C(1))));
  t251 = ((uint64_t)(v5));
  fg169_hput(t248, t249, t250, t251);
  t252 = ((int64_t)(v2));
  t253 = ((int64_t)(((int64_t)UINT64_C(3))));
  t254 = ((int64_t)(((int64_t)UINT64_C(2))));
  t255 = ((uint64_t)(v6));
  fg169_hput(t252, t253, t254, t255);
  t256 = ((int64_t)(v2));
  t257 = ((int64_t)(((int64_t)UINT64_C(3))));
  t258 = ((int64_t)(((int64_t)UINT64_C(4))));
  t259 = ((uint64_t)(v6));
  fg169_hput(t256, t257, t258, t259);
  v17 = ((uint8_t)UINT64_C(0));
  t260 = ((int64_t)(v1));
  t261 = ((int64_t)(((int64_t)UINT64_C(4))));
  t262 = ((t260) == (t261));
  t263 = t262;
  if (!t263) {
  t264 = ((int64_t)(v1));
  t265 = ((int64_t)(((int64_t)UINT64_C(5))));
  t263 = ((t264) == (t265));
  }
  t266 = t263;
  if (t266) {
  {
  t267 = ((int64_t)(v0));
  t268 = ((int64_t)(((int64_t)UINT64_C(3))));
  t269 = fg169_constant(t267, t268);
  t270 = ((int64_t)(t269));
  t271 = ((int64_t)(((int64_t)UINT64_C(0))));
  v17 = ((t270) != (t271));
  }
  }
  t272 = ((int64_t)(v1));
  t273 = ((int64_t)(((int64_t)UINT64_C(1))));
  t274 = ((t272) == (t273));
  t275 = t274;
  if (t275) {
  t276 = ((uint64_t)(v6));
  t277 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t275 = ((t276) == (t277));
  }
  t278 = t275;
  t279 = t278;
  if (t279) {
  t280 = ((int64_t)(v0));
  t281 = ((int64_t)(((int64_t)UINT64_C(3))));
  t282 = fg169_constant(t280, t281);
  t283 = ((int64_t)(t282));
  t284 = ((int64_t)(((int64_t)UINT64_C(0))));
  t279 = ((t283) != (t284));
  }
  t285 = t279;
  if (t285) {
  {
  v18 = ((uint64_t)(((int64_t)UINT64_C(65536))));
  t286 = ((int64_t)(v0));
  t287 = ((int64_t)(((int64_t)UINT64_C(4))));
  t288 = fg169_constant(t286, t287);
  t289 = ((int64_t)(t288));
  t290 = ((int64_t)(((int64_t)UINT64_C(0))));
  t291 = ((t289) != (t290));
  if (t291) {
  {
  t292 = ((int64_t)(v0));
  t293 = ((int64_t)(((int64_t)UINT64_C(5))));
  t294 = fg169_constant(t292, t293);
  t295 = ((int64_t)(t294));
  t296 = ((int64_t)(((int64_t)UINT64_C(0))));
  t297 = ((t295) == (t296));
  t298 = t297;
  if (t298) {
  t299 = ((int64_t)(v0));
  t300 = ((int64_t)(((int64_t)UINT64_C(6))));
  t301 = fg169_constant(t299, t300);
  t302 = ((int64_t)(t301));
  t303 = ((int64_t)(((int64_t)UINT64_C(0))));
  t298 = ((t302) == (t303));
  }
  t304 = t298;
  t305 = t304;
  if (!t305) {
  t306 = ((int64_t)(v0));
  t307 = ((int64_t)(((int64_t)UINT64_C(7))));
  t308 = fg169_constant(t306, t307);
  t309 = ((int64_t)(t308));
  t310 = ((int64_t)(((int64_t)UINT64_C(0))));
  t305 = ((t309) != (t310));
  }
  v19 = t305;
  t311 = v19;
  t312 = (!(t311));
  if (t312) {
  {
  v18 = ((uint64_t)(((int64_t)UINT64_C(32768))));
  }
  }
  }
  }
  t313 = ((uint64_t)(v5));
  t314 = ((uint64_t)(v18));
  v17 = ((t313) > (t314));
  }
  }
  t315 = v17;
  if (t315) {
  {
  t316 = ((int64_t)(v2));
  t317 = ((int64_t)(((int64_t)UINT64_C(3))));
  t318 = ((int64_t)(((int64_t)UINT64_C(7))));
  t319 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg169_hput(t316, t317, t318, t319);
  t320 = ((int64_t)(v0));
  t321 = fg169_cub_query(t320);
  v20 = ((uint64_t)(t321));
  t322 = ((uint64_t)(v20));
  t323 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t324 = ((t322) > (t323));
  t325 = t324;
  if (t325) {
  t326 = ((uint64_t)(v20));
  t327 = ((uint64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t325 = ((t326) <= (t327));
  }
  t328 = t325;
  t329 = ((int64_t)(((int64_t)UINT64_C(19))));
  gg_function_4(t328, t329, base + 0);
  t330 = ((int64_t)(v0));
  t331 = ((uint64_t)(v20));
  t332 = fg169_pool_alloc(t330, t331);
  v21 = ((uint64_t)(t332));
  t333 = ((uint64_t)(v21));
  t334 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t335 = ((t333) != (t334));
  t336 = ((int64_t)(((int64_t)UINT64_C(20))));
  gg_function_4(t335, t336, base + 0);
  t337 = ((int64_t)(v2));
  t338 = ((int64_t)(((int64_t)UINT64_C(3))));
  t339 = ((int64_t)(((int64_t)UINT64_C(8))));
  t340 = fg169_hget(t337, t338, t339);
  t341 = ((uint64_t)(t340));
  t342 = ((uint64_t)(v20));
  t343 = ((t341) < (t342));
  if (t343) {
  {
  t344 = ((int64_t)(v0));
  t345 = ((uint64_t)(v21));
  fg169_pool_free(t344, t345);
  t346 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg169_fail(t346);
  goto gg_done;
  }
  }
  t347 = ((int64_t)(v0));
  t348 = ((uint64_t)(v21));
  fg169_cub_run(t347, t348);
  t349 = ((int64_t)(v1));
  t350 = ((int64_t)(((int64_t)UINT64_C(1))));
  t351 = ((t349) == (t350));
  if (t351) {
  {
  t352 = ((int64_t)(v2));
  t353 = ((int64_t)(((int64_t)UINT64_C(3))));
  t354 = ((int64_t)(((int64_t)UINT64_C(7))));
  t355 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  fg169_hput(t352, t353, t354, t355);
  t356 = ((int64_t)(v2));
  t357 = ((int64_t)(((int64_t)UINT64_C(3))));
  t358 = ((int64_t)(((int64_t)UINT64_C(3))));
  t359 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32))))));
  fg169_hput(t356, t357, t358, t359);
  t360 = ((int64_t)(v2));
  t361 = ((int64_t)(((int64_t)UINT64_C(3))));
  t362 = ((int64_t)(((int64_t)UINT64_C(4))));
  t363 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg169_hput(t360, t361, t362, t363);
  t364 = ((int64_t)(v0));
  fg169_launch(t364);
  }
  }
  t365 = ((int64_t)(v0));
  t366 = ((uint64_t)(v21));
  fg169_pool_free(t365, t366);
  goto gg_done;
  }
  }
  t367 = ((uint64_t)(v5));
  t368 = ((uint64_t)(((int64_t)UINT64_C(2147483647))));
  t369 = ((t367) <= (t368));
  t370 = ((int64_t)(((int64_t)UINT64_C(22))));
  gg_function_4(t369, t370, base + 0);
  t371 = ((int64_t)(v1));
  t372 = ((int64_t)(((int64_t)UINT64_C(2))));
  t373 = ((t371) == (t372));
  if (t373) {
  {
  t374 = ((int64_t)(((int64_t)(v5))));
  t375 = ((int64_t)(((int64_t)(v6))));
  v22 = gg_function_20(t374, t375, base + 0);
  t376 = (v22).f0;
  t377 = ((int64_t)(((int64_t)UINT64_C(23))));
  gg_function_4(t376, t377, base + 0);
  t378 = ((int64_t)(v2));
  t379 = ((int64_t)(((int64_t)UINT64_C(3))));
  t380 = ((int64_t)(((int64_t)UINT64_C(3))));
  t381 = ((uint64_t)(((uint64_t)((v22).f1))));
  fg169_hput(t378, t379, t380, t381);
  t382 = ((int64_t)(v2));
  t383 = ((int64_t)(((int64_t)UINT64_C(3))));
  t384 = ((int64_t)(((int64_t)UINT64_C(4))));
  t385 = ((uint64_t)(((uint64_t)((v22).f2))));
  fg169_hput(t382, t383, t384, t385);
  t386 = ((int64_t)(v2));
  t387 = ((int64_t)(((int64_t)UINT64_C(3))));
  t388 = ((int64_t)(((int64_t)UINT64_C(5))));
  t389 = ((uint64_t)(((uint64_t)((v22).f3))));
  fg169_hput(t386, t387, t388, t389);
  }
  } else {
  {
  t390 = ((int64_t)(v0));
  t391 = ((int64_t)(((int64_t)UINT64_C(2))));
  t392 = fg169_constant(t390, t391);
  v23 = ((int64_t)(t392));
  t393 = ((int64_t)(v23));
  t394 = ((int64_t)(((int64_t)UINT64_C(0))));
  t395 = ((t393) > (t394));
  t396 = ((int64_t)(((int64_t)UINT64_C(24))));
  gg_function_4(t395, t396, base + 0);
  t397 = ((int64_t)(((int64_t)(v5))));
  t398 = ((int64_t)(((int64_t)(v6))));
  t399 = ((int64_t)(v23));
  v24 = gg_function_26(t397, t398, t399, base + 0);
  t400 = ((int64_t)(v2));
  t401 = ((int64_t)(((int64_t)UINT64_C(3))));
  t402 = ((int64_t)(((int64_t)UINT64_C(3))));
  t403 = ((uint64_t)(((uint64_t)(v24))));
  fg169_hput(t400, t401, t402, t403);
  t404 = ((int64_t)(v2));
  t405 = ((int64_t)(((int64_t)UINT64_C(3))));
  t406 = ((int64_t)(((int64_t)UINT64_C(5))));
  t407 = ((int32_t)(v24));
  t408 = gg_function_27(t407, base + 0);
  t409 = ((uint64_t)(((uint64_t)(t408))));
  t410 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t411 = ((uint64_t)(((uint64_t)((uint64_t)(t409) * (uint64_t)(t410)))));
  fg169_hput(t404, t405, t406, t411);
  }
  }
  t412 = ((int64_t)(v0));
  fg169_launch(t412);
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_1(t0, base + 0);
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
