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


static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(int64_t a, int64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, int64_t value);
static uint8_t gg_eq_2(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint8_t value);
static uint8_t gg_eq_3(float a, float b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, float value);
static uint8_t gg_eq_5(double a, double b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, double value);
static uint8_t gg_eq_6(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint8_t value);
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
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(float a, float b) {
return a == b;
}
static void gg_render_3(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(double a, double b) {
return a == b;
}
static void gg_render_5(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t p1, uint8_t p2, int64_t base);
static float gg_function_9(int64_t p0, float p1, int64_t base);
static float gg_function_9(int64_t p0, float p1, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  float t4;
  float t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  float t9;
  float t10;
  uint8_t t11;
  float t12;
  float t13;
  uint8_t t14;
  float t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  float t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  float t23;
  float t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
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
  float t42;
  float t43;
  float t44;
  float t45;
  float t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
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
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  float t63;
  float t64;
  float t65;
  float t66;
  float t67;
  float t68;
  float t69;
  float t70;
  float t71;
  float t72;
  float t73;
  int64_t t74;
  int64_t t75;
  uint8_t t76;
  float t77;
  float t78;
  float t79;
  float t80;
  float t81;
  float t82;
  float t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  float t87;
  float t88;
  int64_t t89;
  int64_t t90;
  uint8_t t91;
  float t92;
  float t93;
  float t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  float t98;
  float t99;
  float t100;
  float t101;
  float t102;
  float t103;
  float t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
  float t108;
  float t109;
  float t110;
  float t111;
  float t112;
  float t113;
  float t114;
  float t115;
  float t116;
  float t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
  float t121;
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
  int64_t t133;
  int64_t t134;
  uint8_t t135;
  float t136;
  float t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
  float t141;
  float t142;
  int64_t t143;
  int64_t t144;
  uint8_t t145;
  float t146;
  float t147;
  int64_t t148;
  int64_t t149;
  uint8_t t150;
  float t151;
  float t152;
  int64_t t153;
  int64_t t154;
  uint8_t t155;
  float t156;
  float t157;
  int64_t t158;
  int64_t t159;
  uint8_t t160;
  float t161;
  float t162;
  int64_t t163;
  int64_t t164;
  uint8_t t165;
  float t166;
  float t167;
  uint8_t t168;
  float t169;
  float t170;
  int64_t t171;
  int64_t t172;
  uint8_t t173;
  float t174;
  float t175;
  int64_t t176;
  int64_t t177;
  uint8_t t178;
  float t179;
  float t180;
  int64_t t181;
  int64_t t182;
  uint8_t t183;
  float t184;
  float t185;
  int64_t t186;
  int64_t t187;
  uint8_t t188;
  double t189;
  double t190;
  int64_t t191;
  int64_t t192;
  uint8_t t193;
  float t194;
  float t195;
  int64_t t196;
  int64_t t197;
  uint8_t t198;
  float t199;
  float t200;
  uint8_t t201;
  float t202;
  float t203;
  float t204;
  float t205;
  float t206;
  float t207;
  float t208;
  float t209;
  float t210;
  float t211;
  float t212;
  int64_t v0;
  float v1;
  float v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v1));
  t5 = fabsf(t4);
  t0 = ((float)(t5));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((t6) == (t7));
  if (t8) {
  {
  t9 = ((float)(v1));
  t10 = ((float)(gg_double_bits(UINT64_C(0))));
  t11 = ((t9) > (t10));
  if (t11) {
  {
  t0 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  {
  t12 = ((float)(v1));
  t13 = ((float)(gg_double_bits(UINT64_C(0))));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t15 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t0 = ((float)((-(t15))));
  }
  } else {
  {
  t0 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  }
  }
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t19 = v1;
  t0 = ((float)((-(t19))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(3))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  t23 = ((float)(v1));
  t24 = ((float)(gg_double_bits(UINT64_C(0))));
  t25 = ((t23) > (t24));
  if (t25) {
  {
  t0 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  {
  t0 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  goto gg_done;
  }
  }
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(4))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t29 = ((float)(gg_double_bits(UINT64_C(4602678819172646912))));
  t30 = ((float)(v1));
  t31 = ((float)(((t29) * (t30))));
  t32 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t33 = ((float)(((float)(gg_double_bits(UINT64_C(4605361924766709329))))));
  t34 = ((float)(v1));
  t35 = ((float)(((t33) * (t34))));
  t36 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t37 = ((float)(((float)(gg_double_bits(UINT64_C(4586604931670606327))))));
  t38 = ((float)(v1));
  t39 = ((float)(((t37) * (t38))));
  t40 = ((float)(v1));
  t41 = ((float)(((t39) * (t40))));
  t42 = ((float)(((t36) + (t41))));
  t43 = ((float)(((t35) * (t42))));
  t44 = tanhf(t43);
  t45 = ((float)(t44));
  t46 = ((float)(((t32) + (t45))));
  t0 = ((float)(((t31) * (t46))));
  goto gg_done;
  }
  }
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(5))));
  t49 = ((t47) == (t48));
  if (t49) {
  {
  t50 = ((float)(gg_double_bits(UINT64_C(4602678819172646912))));
  t51 = ((float)(v1));
  t52 = ((float)(((t50) * (t51))));
  t53 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t54 = ((float)(v1));
  t55 = ((float)(((float)(gg_double_bits(UINT64_C(4604544271217802189))))));
  t56 = ((float)(((t54) * (t55))));
  t57 = erff(t56);
  t58 = ((float)(t57));
  t59 = ((float)(((t53) + (t58))));
  t0 = ((float)(((t52) * (t59))));
  goto gg_done;
  }
  }
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(6))));
  t62 = ((t60) == (t61));
  if (t62) {
  {
  t63 = ((float)(v1));
  t64 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t65 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t66 = ((float)(gg_double_bits(UINT64_C(4610343945738431496))));
  t67 = ((float)((-(t66))));
  t68 = ((float)(v1));
  t69 = ((float)(((t67) * (t68))));
  t70 = expf(t69);
  t71 = ((float)(t70));
  t72 = ((float)(((t65) + (t71))));
  t73 = ((float)(((t64) / (t72))));
  t0 = ((float)(((t63) * (t73))));
  goto gg_done;
  }
  }
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(7))));
  t76 = ((t74) == (t75));
  if (t76) {
  {
  t77 = ((float)(v1));
  t78 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t79 = v1;
  t80 = ((float)((-(t79))));
  t81 = expf(t80);
  t82 = ((float)(t81));
  t83 = ((float)(((t78) + (t82))));
  t0 = ((float)(((t77) / (t83))));
  goto gg_done;
  }
  }
  t84 = ((int64_t)(v0));
  t85 = ((int64_t)(((int64_t)UINT64_C(8))));
  t86 = ((t84) == (t85));
  if (t86) {
  {
  t87 = ((float)(v1));
  t88 = tanhf(t87);
  t0 = ((float)(t88));
  goto gg_done;
  }
  }
  t89 = ((int64_t)(v0));
  t90 = ((int64_t)(((int64_t)UINT64_C(9))));
  t91 = ((t89) == (t90));
  if (t91) {
  {
  t92 = ((float)(v1));
  t93 = ((float)(gg_double_bits(UINT64_C(0))));
  t94 = fmaxf(t92, t93);
  t0 = ((float)(t94));
  goto gg_done;
  }
  }
  t95 = ((int64_t)(v0));
  t96 = ((int64_t)(((int64_t)UINT64_C(10))));
  t97 = ((t95) == (t96));
  if (t97) {
  {
  t98 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t99 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t100 = v1;
  t101 = ((float)((-(t100))));
  t102 = expf(t101);
  t103 = ((float)(t102));
  t104 = ((float)(((t99) + (t103))));
  t0 = ((float)(((t98) / (t104))));
  goto gg_done;
  }
  }
  t105 = ((int64_t)(v0));
  t106 = ((int64_t)(((int64_t)UINT64_C(11))));
  t107 = ((t105) == (t106));
  if (t107) {
  {
  t108 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t109 = ((float)(gg_double_bits(UINT64_C(0))));
  t110 = ((float)(v1));
  t111 = ((float)(gg_double_bits(UINT64_C(4613937818241073152))));
  t112 = ((float)(((t110) + (t111))));
  t113 = ((float)(gg_double_bits(UINT64_C(4618441417868443648))));
  t114 = ((float)(((t112) / (t113))));
  t115 = fmaxf(t109, t114);
  t116 = ((float)(t115));
  t117 = fminf(t108, t116);
  t0 = ((float)(t117));
  goto gg_done;
  }
  }
  t118 = ((int64_t)(v0));
  t119 = ((int64_t)(((int64_t)UINT64_C(12))));
  t120 = ((t118) == (t119));
  if (t120) {
  {
  t121 = ((float)(v1));
  t122 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t123 = ((float)(gg_double_bits(UINT64_C(0))));
  t124 = ((float)(v1));
  t125 = ((float)(gg_double_bits(UINT64_C(4613937818241073152))));
  t126 = ((float)(((t124) + (t125))));
  t127 = ((float)(gg_double_bits(UINT64_C(4618441417868443648))));
  t128 = ((float)(((t126) / (t127))));
  t129 = fmaxf(t123, t128);
  t130 = ((float)(t129));
  t131 = fminf(t122, t130);
  t132 = ((float)(t131));
  t0 = ((float)(((t121) * (t132))));
  goto gg_done;
  }
  }
  t133 = ((int64_t)(v0));
  t134 = ((int64_t)(((int64_t)UINT64_C(13))));
  t135 = ((t133) == (t134));
  if (t135) {
  {
  t136 = ((float)(v1));
  t137 = expf(t136);
  t0 = ((float)(t137));
  goto gg_done;
  }
  }
  t138 = ((int64_t)(v0));
  t139 = ((int64_t)(((int64_t)UINT64_C(14))));
  t140 = ((t138) == (t139));
  if (t140) {
  {
  t141 = ((float)(v1));
  t142 = ((float)(v1));
  t0 = ((float)(((t141) * (t142))));
  goto gg_done;
  }
  }
  t143 = ((int64_t)(v0));
  t144 = ((int64_t)(((int64_t)UINT64_C(15))));
  t145 = ((t143) == (t144));
  if (t145) {
  {
  t146 = ((float)(v1));
  t147 = sqrtf(t146);
  t0 = ((float)(t147));
  goto gg_done;
  }
  }
  t148 = ((int64_t)(v0));
  t149 = ((int64_t)(((int64_t)UINT64_C(16))));
  t150 = ((t148) == (t149));
  if (t150) {
  {
  t151 = ((float)(v1));
  t152 = sinf(t151);
  t0 = ((float)(t152));
  goto gg_done;
  }
  }
  t153 = ((int64_t)(v0));
  t154 = ((int64_t)(((int64_t)UINT64_C(17))));
  t155 = ((t153) == (t154));
  if (t155) {
  {
  t156 = ((float)(v1));
  t157 = cosf(t156);
  t0 = ((float)(t157));
  goto gg_done;
  }
  }
  t158 = ((int64_t)(v0));
  t159 = ((int64_t)(((int64_t)UINT64_C(18))));
  t160 = ((t158) == (t159));
  if (t160) {
  {
  t161 = ((float)(v1));
  t162 = logf(t161);
  t0 = ((float)(t162));
  goto gg_done;
  }
  }
  t163 = ((int64_t)(v0));
  t164 = ((int64_t)(((int64_t)UINT64_C(19))));
  t165 = ((t163) == (t164));
  if (t165) {
  {
  t166 = ((float)(v1));
  t167 = ((float)(gg_double_bits(UINT64_C(0))));
  t168 = ((t166) > (t167));
  if (t168) {
  {
  t0 = ((float)(v1));
  }
  } else {
  {
  t169 = ((float)(v1));
  t170 = expm1f(t169);
  t0 = ((float)(t170));
  }
  }
  goto gg_done;
  }
  }
  t171 = ((int64_t)(v0));
  t172 = ((int64_t)(((int64_t)UINT64_C(20))));
  t173 = ((t171) == (t172));
  if (t173) {
  {
  t174 = ((float)(v1));
  t175 = floorf(t174);
  t0 = ((float)(t175));
  goto gg_done;
  }
  }
  t176 = ((int64_t)(v0));
  t177 = ((int64_t)(((int64_t)UINT64_C(21))));
  t178 = ((t176) == (t177));
  if (t178) {
  {
  t179 = ((float)(v1));
  t180 = ceilf(t179);
  t0 = ((float)(t180));
  goto gg_done;
  }
  }
  t181 = ((int64_t)(v0));
  t182 = ((int64_t)(((int64_t)UINT64_C(22))));
  t183 = ((t181) == (t182));
  if (t183) {
  {
  t184 = ((float)(v1));
  t185 = roundf(t184);
  t0 = ((float)(t185));
  goto gg_done;
  }
  }
  t186 = ((int64_t)(v0));
  t187 = ((int64_t)(((int64_t)UINT64_C(23))));
  t188 = ((t186) == (t187));
  if (t188) {
  {
  t189 = ((double)(((double)(v1))));
  t190 = trunc(t189);
  t0 = ((float)(((float)(t190))));
  goto gg_done;
  }
  }
  t191 = ((int64_t)(v0));
  t192 = ((int64_t)(((int64_t)UINT64_C(24))));
  t193 = ((t191) == (t192));
  if (t193) {
  {
  t194 = ((float)(v1));
  t195 = expm1f(t194);
  t0 = ((float)(t195));
  goto gg_done;
  }
  }
  t196 = ((int64_t)(v0));
  t197 = ((int64_t)(((int64_t)UINT64_C(25))));
  t198 = ((t196) == (t197));
  if (t198) {
  {
  t199 = ((float)(v1));
  t200 = ((float)(gg_double_bits(UINT64_C(4626322717216342016))));
  t201 = ((t199) > (t200));
  if (t201) {
  {
  t0 = ((float)(v1));
  }
  } else {
  {
  t202 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t203 = ((float)(v1));
  t204 = expf(t203);
  t205 = ((float)(t204));
  t206 = ((float)(((t202) + (t205))));
  t207 = logf(t206);
  t0 = ((float)(t207));
  }
  }
  goto gg_done;
  }
  }
  t208 = ((float)(v1));
  t209 = ((float)(gg_double_bits(UINT64_C(0))));
  t210 = fmaxf(t208, t209);
  v2 = ((float)(t210));
  t211 = ((float)(v2));
  t212 = ((float)(v2));
  t0 = ((float)(((t211) * (t212))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_8(int64_t p0, int64_t p1, uint8_t p2, int64_t base) {
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
  uint8_t t22;
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
  int64_t v0;
  int64_t v1;
  uint8_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(7))));
  t3 = fg166_di(t1, t2);
  v3 = ((int64_t)(t3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(8))));
  t6 = fg166_di(t4, t5);
  v4 = ((int64_t)(t6));
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(9))));
  t9 = fg166_di(t7, t8);
  v5 = ((int64_t)(t9));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(v3));
  v6 = ((int64_t)(((int64_t)gg_mod(t10, t11))));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v3));
  v7 = ((int64_t)(((int64_t)gg_div(t12, t13))));
  t14 = ((int64_t)(v7));
  t15 = ((int64_t)(v4));
  v8 = ((int64_t)(((int64_t)gg_mod(t14, t15))));
  t16 = ((int64_t)(v7));
  t17 = ((int64_t)(v4));
  v9 = ((int64_t)(((int64_t)gg_div(t16, t17))));
  t18 = ((int64_t)(v9));
  t19 = ((int64_t)(v5));
  v10 = ((int64_t)(((int64_t)gg_mod(t18, t19))));
  t20 = ((int64_t)(v9));
  t21 = ((int64_t)(v5));
  v11 = ((int64_t)(((int64_t)gg_div(t20, t21))));
  t22 = v2;
  if (t22) {
  {
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(17))));
  t26 = fg166_di(t24, t25);
  t27 = ((int64_t)(t26));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t27)))));
  t29 = ((int64_t)(v8));
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(14))));
  t32 = fg166_di(t30, t31);
  t33 = ((int64_t)(t32));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t34)))));
  t36 = ((int64_t)(v10));
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(15))));
  t39 = fg166_di(t37, t38);
  t40 = ((int64_t)(t39));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t40)))));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t41)))));
  t43 = ((int64_t)(v11));
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(16))));
  t46 = fg166_di(t44, t45);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t47)))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t48)))));
  goto gg_done;
  }
  }
  t49 = ((int64_t)(v6));
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(10))));
  t52 = fg166_di(t50, t51);
  t53 = ((int64_t)(t52));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t53)))));
  t55 = ((int64_t)(v8));
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(((int64_t)UINT64_C(11))));
  t58 = fg166_di(t56, t57);
  t59 = ((int64_t)(t58));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t59)))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t60)))));
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(((int64_t)UINT64_C(12))));
  t65 = fg166_di(t63, t64);
  t66 = ((int64_t)(t65));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t61) + (uint64_t)(t67)))));
  t69 = ((int64_t)(v11));
  t70 = ((int64_t)(v0));
  t71 = ((int64_t)(((int64_t)UINT64_C(13))));
  t72 = fg166_di(t70, t71);
  t73 = ((int64_t)(t72));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t73)))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t74)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  float t27;
  float t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  float t34;
  float t35;
  float t36;
  int64_t t37;
  int64_t t38;
  float t39;
  float t40;
  float t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  int64_t t58;
  int64_t t59;
  uint8_t t60;
  uint8_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  uint8_t t68;
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
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  uint8_t t88;
  int64_t t89;
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
  uint8_t t103;
  int64_t t104;
  int64_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  uint8_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  float t116;
  int64_t t117;
  int64_t t118;
  uint8_t t119;
  uint8_t t120;
  int64_t t121;
  int64_t t122;
  uint8_t t123;
  int64_t t124;
  float t125;
  int64_t t126;
  int64_t t127;
  uint8_t t128;
  int64_t t129;
  float t130;
  float t131;
  float t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  float t136;
  float t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
  float t141;
  int64_t t142;
  int64_t t143;
  float t144;
  float t145;
  float t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  float t150;
  float t151;
  int64_t t152;
  int64_t t153;
  float t154;
  float t155;
  float t156;
  float t157;
  int64_t t158;
  int64_t t159;
  float t160;
  float t161;
  float t162;
  float t163;
  float t164;
  float t165;
  float t166;
  float t167;
  int64_t t168;
  int64_t t169;
  float t170;
  float t171;
  float t172;
  float t173;
  float t174;
  float t175;
  float t176;
  float t177;
  float t178;
  float t179;
  int64_t t180;
  int64_t t181;
  uint8_t t182;
  float t183;
  int64_t t184;
  int64_t t185;
  float t186;
  float t187;
  float t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  float t192;
  float t193;
  int64_t t194;
  int64_t t195;
  float t196;
  float t197;
  float t198;
  float t199;
  int64_t t200;
  int64_t t201;
  float t202;
  float t203;
  float t204;
  float t205;
  int64_t t206;
  float t207;
  float t208;
  float t209;
  float t210;
  int64_t t211;
  int64_t t212;
  uint8_t t213;
  float t214;
  float t215;
  uint8_t t216;
  float t217;
  int64_t t218;
  int64_t t219;
  float t220;
  float t221;
  int64_t t222;
  int64_t t223;
  float t224;
  float t225;
  float t226;
  float t227;
  float t228;
  float t229;
  float t230;
  float t231;
  int64_t t232;
  int64_t t233;
  float t234;
  float t235;
  float t236;
  int64_t t237;
  int64_t t238;
  float t239;
  float t240;
  float t241;
  float t242;
  float t243;
  float t244;
  float t245;
  float t246;
  int64_t t247;
  int64_t t248;
  float t249;
  float t250;
  float t251;
  float t252;
  float t253;
  float t254;
  float t255;
  float t256;
  float t257;
  float t258;
  float t259;
  float t260;
  int64_t t261;
  int64_t t262;
  uint8_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  float t267;
  float t268;
  float t269;
  float t270;
  float t271;
  float t272;
  float t273;
  float t274;
  float t275;
  float t276;
  float t277;
  float t278;
  float t279;
  float t280;
  float t281;
  float t282;
  float t283;
  float t284;
  int64_t t285;
  int64_t t286;
  uint8_t t287;
  float t288;
  int64_t t289;
  int64_t t290;
  float t291;
  float t292;
  float t293;
  float t294;
  int64_t t295;
  int64_t t296;
  float t297;
  float t298;
  float t299;
  int64_t t300;
  int64_t t301;
  uint8_t t302;
  float t303;
  float t304;
  float t305;
  float t306;
  float t307;
  float t308;
  float t309;
  float t310;
  int64_t t311;
  int64_t t312;
  float t313;
  float t314;
  float t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  float t319;
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
  float v20;
  float v21;
  float v22;
  float v23;
  float v24;
  float v25;
  float v26;
  float v27;
  float v28;
  float v29;
  float v30;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg166_di(t0, t1);
  v6 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(1))));
  t5 = fg166_di(t3, t4);
  v7 = ((int64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = fg166_di(t6, t7);
  v8 = ((int64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(3))));
  t11 = fg166_di(t9, t10);
  v9 = ((int64_t)(t11));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(17))));
  t14 = fg166_di(t12, t13);
  v10 = ((int64_t)(t14));
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(7))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  fg166_pdl_lc();
  fg166_pdl_sync();
  v11 = ((int64_t)(v4));
label1:;
  t18 = ((int64_t)(v11));
  t19 = ((int64_t)(v9));
  if (!(((t18) < (t19)))) goto label2;
  {
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(v8));
  t22 = ((int64_t)(v11));
  t23 = ((int64_t)(v10));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg166_df(t25, t26);
  t28 = ((float)(t27));
  t29 = ((int64_t)(v3));
  t30 = ((int64_t)(v6));
  t31 = ((int64_t)(v11));
  t32 = ((int64_t)(v10));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t34 = fg166_load(t29, t30, t33);
  t35 = ((float)(t34));
  t36 = ((float)(((t28) * (t35))));
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t39 = fg166_df(t37, t38);
  t40 = ((float)(t39));
  t41 = ((float)(((t36) + (t40))));
  fg166_store(t20, t21, t24, t41);
  t42 = v11;
  t43 = ((int64_t)(v5));
  v11 = ((int64_t)((uint64_t)(t42) + (uint64_t)(t43)));
  }
  goto label1;
label2:;
  goto gg_done;
  }
  }
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(((int64_t)UINT64_C(2))));
  t46 = ((t44) <= (t45));
  if (t46) {
  {
  fg166_pdl_lc();
  }
  }
  t47 = ((int64_t)(v4));
  t48 = ((int64_t)(v9));
  t49 = ((t47) >= (t48));
  if (t49) {
  {
  goto gg_done;
  }
  }
  t50 = ((int64_t)(v4));
  t51 = ((int64_t)(v10));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  v13 = ((int64_t)(v12));
  v14 = ((int64_t)(v12));
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  t54 = ((t52) == (t53));
  if (t54) {
  {
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(v4));
  t57 = ((uint8_t)UINT64_C(0));
  v12 = gg_function_8(t55, t56, t57, base + 0);
  }
  }
  t58 = ((int64_t)(v1));
  t59 = ((int64_t)(((int64_t)UINT64_C(2))));
  t60 = ((t58) == (t59));
  t61 = t60;
  if (!t61) {
  t62 = ((int64_t)(v1));
  t63 = ((int64_t)(((int64_t)UINT64_C(3))));
  t61 = ((t62) == (t63));
  }
  t64 = t61;
  t65 = t64;
  if (!t65) {
  t66 = ((int64_t)(v1));
  t67 = ((int64_t)(((int64_t)UINT64_C(4))));
  t65 = ((t66) == (t67));
  }
  t68 = t65;
  if (t68) {
  {
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(((int64_t)UINT64_C(4))));
  t71 = fg166_di(t69, t70);
  v15 = ((int64_t)(t71));
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(((int64_t)UINT64_C(5))));
  t74 = fg166_di(t72, t73);
  v16 = ((int64_t)(t74));
  t75 = ((int64_t)(v0));
  t76 = ((int64_t)(((int64_t)UINT64_C(6))));
  t77 = fg166_di(t75, t76);
  v17 = ((int64_t)(t77));
  t78 = ((int64_t)(v4));
  t79 = ((int64_t)(v15));
  t80 = ((int64_t)(((int64_t)gg_div(t78, t79))));
  t81 = ((int64_t)(v16));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  t83 = ((int64_t)(v4));
  t84 = ((int64_t)(v15));
  t85 = ((int64_t)(((int64_t)gg_mod(t83, t84))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t85)))));
  t86 = ((int64_t)(v16));
  t87 = ((int64_t)(v17));
  t88 = ((t86) == (t87));
  if (t88) {
  {
  v19 = ((int64_t)(v18));
  }
  } else {
  {
  t89 = ((int64_t)(v4));
  t90 = ((int64_t)(v15));
  t91 = ((int64_t)(((int64_t)gg_div(t89, t90))));
  t92 = ((int64_t)(v17));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t92)))));
  t94 = ((int64_t)(v4));
  t95 = ((int64_t)(v15));
  t96 = ((int64_t)(((int64_t)gg_mod(t94, t95))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t93) + (uint64_t)(t96)))));
  }
  }
  t97 = ((int64_t)(v18));
  t98 = ((int64_t)(v10));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t97) * (uint64_t)(t98)))));
  t99 = ((int64_t)(v19));
  t100 = ((int64_t)(v10));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t100)))));
  }
  }
  t101 = ((int64_t)(v1));
  t102 = ((int64_t)(((int64_t)UINT64_C(8))));
  t103 = ((t101) == (t102));
  if (t103) {
  {
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(v4));
  t106 = ((uint8_t)UINT64_C(0));
  v12 = gg_function_8(t104, t105, t106, base + 0);
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(v4));
  t109 = ((uint8_t)UINT64_C(1));
  v14 = gg_function_8(t107, t108, t109, base + 0);
  }
  }
  t110 = ((int64_t)(v1));
  t111 = ((int64_t)(((int64_t)UINT64_C(2))));
  t112 = ((t110) <= (t111));
  if (t112) {
  {
  fg166_pdl_sync();
  }
  }
  t113 = ((int64_t)(v3));
  t114 = ((int64_t)(v6));
  t115 = ((int64_t)(v12));
  t116 = fg166_load(t113, t114, t115);
  v20 = ((float)(t116));
  v21 = ((float)(gg_double_bits(UINT64_C(0))));
  t117 = ((int64_t)(v1));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((t117) == (t118));
  t120 = t119;
  if (!t120) {
  t121 = ((int64_t)(v1));
  t122 = ((int64_t)(((int64_t)UINT64_C(1))));
  t120 = ((t121) == (t122));
  }
  t123 = t120;
  if (t123) {
  {
  t124 = ((int64_t)(v2));
  t125 = ((float)(v20));
  v21 = gg_function_9(t124, t125, base + 0);
  }
  }
  t126 = ((int64_t)(v1));
  t127 = ((int64_t)(((int64_t)UINT64_C(2))));
  t128 = ((t126) == (t127));
  if (t128) {
  {
  t129 = ((int64_t)(v2));
  t130 = ((float)(v20));
  t131 = gg_function_9(t129, t130, base + 0);
  t132 = ((float)(t131));
  t133 = ((int64_t)(v3));
  t134 = ((int64_t)(v7));
  t135 = ((int64_t)(v13));
  t136 = fg166_load(t133, t134, t135);
  t137 = ((float)(t136));
  v21 = ((float)(((t132) * (t137))));
  }
  }
  t138 = ((int64_t)(v1));
  t139 = ((int64_t)(((int64_t)UINT64_C(3))));
  t140 = ((t138) == (t139));
  if (t140) {
  {
  t141 = ((float)(v20));
  t142 = ((int64_t)(v0));
  t143 = ((int64_t)(((int64_t)UINT64_C(5))));
  t144 = fg166_df(t142, t143);
  t145 = ((float)(t144));
  t146 = fminf(t141, t145);
  v22 = ((float)(t146));
  t147 = ((int64_t)(v3));
  t148 = ((int64_t)(v7));
  t149 = ((int64_t)(v13));
  t150 = fg166_load(t147, t148, t149);
  t151 = ((float)(t150));
  t152 = ((int64_t)(v0));
  t153 = ((int64_t)(((int64_t)UINT64_C(5))));
  t154 = fg166_df(t152, t153);
  t155 = ((float)(t154));
  t156 = fminf(t151, t155);
  t157 = ((float)(t156));
  t158 = ((int64_t)(v0));
  t159 = ((int64_t)(((int64_t)UINT64_C(5))));
  t160 = fg166_df(t158, t159);
  t161 = t160;
  t162 = ((float)((-(t161))));
  t163 = fmaxf(t157, t162);
  v23 = ((float)(t163));
  t164 = ((float)(v22));
  t165 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t166 = v22;
  t167 = ((float)((-(t166))));
  t168 = ((int64_t)(v0));
  t169 = ((int64_t)(((int64_t)UINT64_C(4))));
  t170 = fg166_df(t168, t169);
  t171 = ((float)(t170));
  t172 = ((float)(((t167) * (t171))));
  t173 = expf(t172);
  t174 = ((float)(t173));
  t175 = ((float)(((t165) + (t174))));
  t176 = ((float)(((t164) / (t175))));
  t177 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t178 = ((float)(v23));
  t179 = ((float)(((t177) + (t178))));
  v21 = ((float)(((t176) * (t179))));
  }
  }
  t180 = ((int64_t)(v1));
  t181 = ((int64_t)(((int64_t)UINT64_C(4))));
  t182 = ((t180) == (t181));
  if (t182) {
  {
  t183 = ((float)(v20));
  t184 = ((int64_t)(v0));
  t185 = ((int64_t)(((int64_t)UINT64_C(5))));
  t186 = fg166_df(t184, t185);
  t187 = ((float)(t186));
  t188 = fminf(t183, t187);
  v24 = ((float)(t188));
  t189 = ((int64_t)(v3));
  t190 = ((int64_t)(v7));
  t191 = ((int64_t)(v13));
  t192 = fg166_load(t189, t190, t191);
  t193 = ((float)(t192));
  t194 = ((int64_t)(v0));
  t195 = ((int64_t)(((int64_t)UINT64_C(5))));
  t196 = fg166_df(t194, t195);
  t197 = ((float)(t196));
  t198 = fminf(t193, t197);
  t199 = ((float)(t198));
  t200 = ((int64_t)(v0));
  t201 = ((int64_t)(((int64_t)UINT64_C(5))));
  t202 = fg166_df(t200, t201);
  t203 = t202;
  t204 = ((float)((-(t203))));
  t205 = fmaxf(t199, t204);
  v25 = ((float)(t205));
  t206 = ((int64_t)(((int64_t)UINT64_C(7))));
  t207 = ((float)(v24));
  t208 = gg_function_9(t206, t207, base + 0);
  t209 = ((float)(t208));
  t210 = ((float)(v25));
  v21 = ((float)(((t209) * (t210))));
  }
  }
  t211 = ((int64_t)(v1));
  t212 = ((int64_t)(((int64_t)UINT64_C(5))));
  t213 = ((t211) == (t212));
  if (t213) {
  {
  t214 = ((float)(v20));
  t215 = ((float)(gg_double_bits(UINT64_C(0))));
  t216 = ((t214) > (t215));
  if (t216) {
  {
  v26 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  }
  } else {
  {
  v26 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  t217 = ((float)(v20));
  t218 = ((int64_t)(v0));
  t219 = ((int64_t)(((int64_t)UINT64_C(2))));
  t220 = fg166_df(t218, t219);
  t221 = ((float)(t220));
  t222 = ((int64_t)(v0));
  t223 = ((int64_t)(((int64_t)UINT64_C(1))));
  t224 = fg166_df(t222, t223);
  t225 = ((float)(t224));
  t226 = ((float)(v20));
  t227 = ((float)(((t225) * (t226))));
  t228 = ((float)(v20));
  t229 = ((float)(((t227) * (t228))));
  t230 = fmaf(t217, t221, t229);
  v27 = ((float)(t230));
  t231 = ((float)(v20));
  t232 = ((int64_t)(v0));
  t233 = ((int64_t)(((int64_t)UINT64_C(2))));
  t234 = fg166_df(t232, t233);
  t235 = ((float)(t234));
  t236 = ((float)(v20));
  t237 = ((int64_t)(v0));
  t238 = ((int64_t)(((int64_t)UINT64_C(3))));
  t239 = fg166_df(t237, t238);
  t240 = ((float)(t239));
  t241 = fminf(t236, t240);
  t242 = ((float)(t241));
  t243 = expm1f(t242);
  t244 = ((float)(t243));
  t245 = ((float)(v20));
  t246 = ((float)(((t244) - (t245))));
  t247 = ((int64_t)(v0));
  t248 = ((int64_t)(((int64_t)UINT64_C(0))));
  t249 = fg166_df(t247, t248);
  t250 = ((float)(t249));
  t251 = ((float)(((t246) * (t250))));
  t252 = fmaf(t231, t235, t251);
  v28 = ((float)(t252));
  t253 = ((float)(v27));
  t254 = ((float)(v26));
  t255 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t256 = ((float)(v26));
  t257 = ((float)(((t255) - (t256))));
  t258 = ((float)(v28));
  t259 = ((float)(((t257) * (t258))));
  t260 = fmaf(t253, t254, t259);
  v21 = ((float)(t260));
  }
  }
  t261 = ((int64_t)(v1));
  t262 = ((int64_t)(((int64_t)UINT64_C(6))));
  t263 = ((t261) == (t262));
  if (t263) {
  {
  t264 = ((int64_t)(v3));
  t265 = ((int64_t)(v7));
  t266 = ((int64_t)(v13));
  t267 = fg166_load(t264, t265, t266);
  v29 = ((float)(t267));
  t268 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t269 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t270 = v29;
  t271 = ((float)((-(t270))));
  t272 = expf(t271);
  t273 = ((float)(t272));
  t274 = ((float)(((t269) + (t273))));
  v30 = ((float)(((t268) / (t274))));
  t275 = ((float)(v20));
  t276 = ((float)(v30));
  t277 = ((float)(((t275) * (t276))));
  t278 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t279 = ((float)(v29));
  t280 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t281 = ((float)(v30));
  t282 = ((float)(((t280) - (t281))));
  t283 = ((float)(((t279) * (t282))));
  t284 = ((float)(((t278) + (t283))));
  v21 = ((float)(((t277) * (t284))));
  }
  }
  t285 = ((int64_t)(v1));
  t286 = ((int64_t)(((int64_t)UINT64_C(8))));
  t287 = ((t285) == (t286));
  if (t287) {
  {
  t288 = ((float)(v20));
  t289 = ((int64_t)(v0));
  t290 = ((int64_t)(((int64_t)UINT64_C(4))));
  t291 = fg166_df(t289, t290);
  t292 = ((float)(t291));
  t293 = fmaxf(t288, t292);
  t294 = ((float)(t293));
  t295 = ((int64_t)(v0));
  t296 = ((int64_t)(((int64_t)UINT64_C(5))));
  t297 = fg166_df(t295, t296);
  t298 = ((float)(t297));
  t299 = fminf(t294, t298);
  v21 = ((float)(t299));
  }
  }
  t300 = ((int64_t)(v1));
  t301 = ((int64_t)(((int64_t)UINT64_C(9))));
  t302 = ((t300) == (t301));
  if (t302) {
  {
  t303 = ((float)(v20));
  t304 = ((float)(gg_double_bits(UINT64_C(0))));
  t305 = fmaxf(t303, t304);
  t306 = ((float)(t305));
  t307 = ((float)(v20));
  t308 = ((float)(gg_double_bits(UINT64_C(0))));
  t309 = fminf(t307, t308);
  t310 = ((float)(t309));
  t311 = ((int64_t)(v0));
  t312 = ((int64_t)(((int64_t)UINT64_C(6))));
  t313 = fg166_df(t311, t312);
  t314 = ((float)(t313));
  t315 = ((float)(((t310) * (t314))));
  v21 = ((float)(((t306) + (t315))));
  }
  }
  t316 = ((int64_t)(v3));
  t317 = ((int64_t)(v8));
  t318 = ((int64_t)(v14));
  t319 = ((float)(v21));
  fg166_store(t316, t317, t318, t319);
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
  gg_header t6;
  int64_t t7;
  float t8;
  float t9;
  int64_t t10;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_1(t0, t1, t2, t3, t4, t5, base + 0);
  t6.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t6.len = 0;
  t7 = ((int64_t)(((int64_t)UINT64_C(7))));
  t8 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t9 = gg_function_9(t7, t8, base + 1);
  gg_float32(gg_ref(&t6, gg_get_stack(base + 0)), t9);
  gg_append(gg_ref(&t6, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t6).base, (t6).len));
  }
gg_done:;
  for (t10 = 0; t10 < 1; t10++) gg_stacks[base + t10].top = 0;
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
