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
static uint8_t gg_eq_4(float a, float b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, float value);
static uint8_t gg_eq_5(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, uint8_t value);
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
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(float a, float b) {
return a == b;
}
static void gg_render_4(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t base);
static void gg_function_9(uint8_t p0, int64_t base);
static void gg_function_9(uint8_t p0, int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  uint8_t v0;
  v0 = p0;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gx_assert(t2);
  }
  }
  }
gg_done:;
}
static int64_t gg_function_8(int64_t p0, int64_t base) {
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
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(37))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg153_gh_get(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg153_gh_get(t8, t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg153_gh_get(t15, t16, t17);
  t19 = ((int64_t)(t18));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) < (t20));
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(26))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = fg153_gh_get(t22, t23, t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t26) < (t27));
  if (t28) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
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
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
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
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
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
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  float t228;
  float t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  float t234;
  float t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  float t240;
  float t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  float t246;
  float t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  float t252;
  float t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  float t258;
  float t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  float t264;
  float t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  float t270;
  float t271;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(29))));
  t3 = ((int64_t)(((int64_t)UINT64_C(8))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t5 = ((int64_t)(((int64_t)UINT64_C(8))));
  t6 = ((int64_t)(((int64_t)UINT64_C(4))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t7)))));
  fg153_gc_clear(t1, t8);
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg153_gh_get(t11, t12, t13);
  t15 = ((int64_t)(t14));
  fg153_gc_put(t9, t10, t15);
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg153_gh_get(t18, t19, t20);
  t22 = ((int64_t)(t21));
  fg153_gc_put(t16, t17, t22);
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(2))));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(2))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = fg153_gh_get(t25, t26, t27);
  t29 = ((int64_t)(t28));
  fg153_gc_put(t23, t24, t29);
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(((int64_t)UINT64_C(3))));
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(3))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg153_gh_get(t32, t33, t34);
  t36 = ((int64_t)(t35));
  fg153_gc_put(t30, t31, t36);
  t37 = ((int64_t)(v1));
  t38 = ((int64_t)(((int64_t)UINT64_C(4))));
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(4))));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = fg153_gh_get(t39, t40, t41);
  t43 = ((int64_t)(t42));
  fg153_gc_put(t37, t38, t43);
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(((int64_t)UINT64_C(5))));
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(16))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = fg153_gh_get(t46, t47, t48);
  t50 = ((int64_t)(t49));
  fg153_gc_put(t44, t45, t50);
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(((int64_t)UINT64_C(6))));
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(5))));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = fg153_gh_get(t53, t54, t55);
  t57 = ((int64_t)(t56));
  fg153_gc_put(t51, t52, t57);
  t58 = ((int64_t)(v1));
  t59 = ((int64_t)(((int64_t)UINT64_C(7))));
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(6))));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = fg153_gh_get(t60, t61, t62);
  t64 = ((int64_t)(t63));
  fg153_gc_put(t58, t59, t64);
  t65 = ((int64_t)(v1));
  t66 = ((int64_t)(((int64_t)UINT64_C(8))));
  t67 = ((int64_t)(v0));
  t68 = ((int64_t)(((int64_t)UINT64_C(7))));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = fg153_gh_get(t67, t68, t69);
  t71 = ((int64_t)(t70));
  fg153_gc_put(t65, t66, t71);
  t72 = ((int64_t)(v1));
  t73 = ((int64_t)(((int64_t)UINT64_C(9))));
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(8))));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = fg153_gh_get(t74, t75, t76);
  t78 = ((int64_t)(t77));
  fg153_gc_put(t72, t73, t78);
  t79 = ((int64_t)(v1));
  t80 = ((int64_t)(((int64_t)UINT64_C(10))));
  t81 = ((int64_t)(v0));
  t82 = ((int64_t)(((int64_t)UINT64_C(9))));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = fg153_gh_get(t81, t82, t83);
  t85 = ((int64_t)(t84));
  fg153_gc_put(t79, t80, t85);
  t86 = ((int64_t)(v1));
  t87 = ((int64_t)(((int64_t)UINT64_C(11))));
  t88 = ((int64_t)(v0));
  t89 = ((int64_t)(((int64_t)UINT64_C(10))));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = fg153_gh_get(t88, t89, t90);
  t92 = ((int64_t)(t91));
  fg153_gc_put(t86, t87, t92);
  t93 = ((int64_t)(v1));
  t94 = ((int64_t)(((int64_t)UINT64_C(12))));
  t95 = ((int64_t)(v0));
  t96 = ((int64_t)(((int64_t)UINT64_C(11))));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = fg153_gh_get(t95, t96, t97);
  t99 = ((int64_t)(t98));
  fg153_gc_put(t93, t94, t99);
  t100 = ((int64_t)(v1));
  t101 = ((int64_t)(((int64_t)UINT64_C(13))));
  t103 = ((int64_t)(v0));
  t104 = ((int64_t)(((int64_t)UINT64_C(25))));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = fg153_gh_get(t103, t104, t105);
  t107 = ((int64_t)(t106));
  t108 = ((int64_t)(((int64_t)UINT64_C(0))));
  t109 = ((t107) != (t108));
  if (t109) {
  {
  t110 = ((int64_t)(v0));
  t111 = ((int64_t)(((int64_t)UINT64_C(11))));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = fg153_gh_get(t110, t111, t112);
  t102 = ((int64_t)(t113));
  }
  } else {
  {
  t114 = ((int64_t)(v0));
  t115 = ((int64_t)(((int64_t)UINT64_C(12))));
  t116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t117 = fg153_gh_get(t114, t115, t116);
  t102 = ((int64_t)(t117));
  }
  }
  t118 = ((int64_t)(t102));
  fg153_gc_put(t100, t101, t118);
  t119 = ((int64_t)(v1));
  t120 = ((int64_t)(((int64_t)UINT64_C(14))));
  t121 = ((int64_t)(v0));
  t122 = ((int64_t)(((int64_t)UINT64_C(26))));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = fg153_gh_get(t121, t122, t123);
  t125 = ((int64_t)(t124));
  fg153_gc_put(t119, t120, t125);
  t126 = ((int64_t)(v1));
  t127 = ((int64_t)(((int64_t)UINT64_C(15))));
  t128 = ((int64_t)(v0));
  t129 = ((int64_t)(((int64_t)UINT64_C(27))));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = fg153_gh_get(t128, t129, t130);
  t132 = ((int64_t)(t131));
  fg153_gc_put(t126, t127, t132);
  t133 = ((int64_t)(v1));
  t134 = ((int64_t)(((int64_t)UINT64_C(16))));
  t135 = ((int64_t)(v0));
  t136 = ((int64_t)(((int64_t)UINT64_C(23))));
  t137 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = fg153_gh_get(t135, t136, t137);
  t139 = ((int64_t)(t138));
  fg153_gc_put(t133, t134, t139);
  t140 = ((int64_t)(v1));
  t141 = ((int64_t)(((int64_t)UINT64_C(17))));
  t142 = ((int64_t)(v0));
  t143 = ((int64_t)(((int64_t)UINT64_C(24))));
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t145 = fg153_gh_get(t142, t143, t144);
  t146 = ((int64_t)(t145));
  fg153_gc_put(t140, t141, t146);
  t147 = ((int64_t)(v1));
  t148 = ((int64_t)(((int64_t)UINT64_C(18))));
  t149 = ((int64_t)(v0));
  t150 = ((int64_t)(((int64_t)UINT64_C(15))));
  t151 = ((int64_t)(((int64_t)UINT64_C(0))));
  t152 = fg153_gh_get(t149, t150, t151);
  t153 = ((int64_t)(t152));
  fg153_gc_put(t147, t148, t153);
  t154 = ((int64_t)(v1));
  t155 = ((int64_t)(((int64_t)UINT64_C(19))));
  t156 = ((int64_t)(v0));
  t157 = ((int64_t)(((int64_t)UINT64_C(28))));
  t158 = ((int64_t)(((int64_t)UINT64_C(0))));
  t159 = fg153_gh_get(t156, t157, t158);
  t160 = ((int64_t)(t159));
  fg153_gc_put(t154, t155, t160);
  t161 = ((int64_t)(v1));
  t162 = ((int64_t)(((int64_t)UINT64_C(20))));
  t163 = ((int64_t)(v0));
  t164 = ((int64_t)(((int64_t)UINT64_C(29))));
  t165 = ((int64_t)(((int64_t)UINT64_C(0))));
  t166 = fg153_gh_get(t163, t164, t165);
  t167 = ((int64_t)(t166));
  fg153_gc_put(t161, t162, t167);
  t168 = ((int64_t)(v1));
  t169 = ((int64_t)(((int64_t)UINT64_C(21))));
  t170 = ((int64_t)(v0));
  t171 = ((int64_t)(((int64_t)UINT64_C(30))));
  t172 = ((int64_t)(((int64_t)UINT64_C(0))));
  t173 = fg153_gh_get(t170, t171, t172);
  t174 = ((int64_t)(t173));
  fg153_gc_put(t168, t169, t174);
  t175 = ((int64_t)(v1));
  t176 = ((int64_t)(((int64_t)UINT64_C(22))));
  t177 = ((int64_t)(v0));
  t178 = ((int64_t)(((int64_t)UINT64_C(31))));
  t179 = ((int64_t)(((int64_t)UINT64_C(0))));
  t180 = fg153_gh_get(t177, t178, t179);
  t181 = ((int64_t)(t180));
  fg153_gc_put(t175, t176, t181);
  t182 = ((int64_t)(v1));
  t183 = ((int64_t)(((int64_t)UINT64_C(23))));
  t184 = ((int64_t)(v0));
  t185 = ((int64_t)(((int64_t)UINT64_C(32))));
  t186 = ((int64_t)(((int64_t)UINT64_C(0))));
  t187 = fg153_gh_get(t184, t185, t186);
  t188 = ((int64_t)(t187));
  fg153_gc_put(t182, t183, t188);
  t189 = ((int64_t)(v1));
  t190 = ((int64_t)(((int64_t)UINT64_C(24))));
  t191 = ((int64_t)(v0));
  t192 = ((int64_t)(((int64_t)UINT64_C(33))));
  t193 = ((int64_t)(((int64_t)UINT64_C(0))));
  t194 = fg153_gh_get(t191, t192, t193);
  t195 = ((int64_t)(t194));
  fg153_gc_put(t189, t190, t195);
  t196 = ((int64_t)(v1));
  t197 = ((int64_t)(((int64_t)UINT64_C(25))));
  t198 = ((int64_t)(v0));
  t199 = ((int64_t)(((int64_t)UINT64_C(34))));
  t200 = ((int64_t)(((int64_t)UINT64_C(0))));
  t201 = fg153_gh_get(t198, t199, t200);
  t202 = ((int64_t)(t201));
  fg153_gc_put(t196, t197, t202);
  t203 = ((int64_t)(v1));
  t204 = ((int64_t)(((int64_t)UINT64_C(26))));
  t205 = ((int64_t)(v0));
  t206 = ((int64_t)(((int64_t)UINT64_C(35))));
  t207 = ((int64_t)(((int64_t)UINT64_C(0))));
  t208 = fg153_gh_get(t205, t206, t207);
  t209 = ((int64_t)(t208));
  fg153_gc_put(t203, t204, t209);
  t210 = ((int64_t)(v1));
  t211 = ((int64_t)(((int64_t)UINT64_C(27))));
  t212 = ((int64_t)(v0));
  t213 = ((int64_t)(((int64_t)UINT64_C(36))));
  t214 = ((int64_t)(((int64_t)UINT64_C(0))));
  t215 = fg153_gh_get(t212, t213, t214);
  t216 = ((int64_t)(t215));
  fg153_gc_put(t210, t211, t216);
  t217 = ((int64_t)(v1));
  t218 = ((int64_t)(((int64_t)UINT64_C(28))));
  t219 = ((int64_t)(v0));
  t220 = ((int64_t)(((int64_t)UINT64_C(37))));
  t221 = ((int64_t)(((int64_t)UINT64_C(0))));
  t222 = fg153_gh_get(t219, t220, t221);
  t223 = ((int64_t)(t222));
  fg153_gc_put(t217, t218, t223);
  t224 = ((int64_t)(v1));
  t225 = ((int64_t)(((int64_t)UINT64_C(0))));
  t226 = ((int64_t)(v0));
  t227 = ((int64_t)(((int64_t)UINT64_C(0))));
  t228 = fg153_gh_fget(t226, t227);
  t229 = ((float)(t228));
  fg153_gc_putf(t224, t225, t229);
  t230 = ((int64_t)(v1));
  t231 = ((int64_t)(((int64_t)UINT64_C(1))));
  t232 = ((int64_t)(v0));
  t233 = ((int64_t)(((int64_t)UINT64_C(1))));
  t234 = fg153_gh_fget(t232, t233);
  t235 = ((float)(t234));
  fg153_gc_putf(t230, t231, t235);
  t236 = ((int64_t)(v1));
  t237 = ((int64_t)(((int64_t)UINT64_C(2))));
  t238 = ((int64_t)(v0));
  t239 = ((int64_t)(((int64_t)UINT64_C(2))));
  t240 = fg153_gh_fget(t238, t239);
  t241 = ((float)(t240));
  fg153_gc_putf(t236, t237, t241);
  t242 = ((int64_t)(v1));
  t243 = ((int64_t)(((int64_t)UINT64_C(3))));
  t244 = ((int64_t)(v0));
  t245 = ((int64_t)(((int64_t)UINT64_C(3))));
  t246 = fg153_gh_fget(t244, t245);
  t247 = ((float)(t246));
  fg153_gc_putf(t242, t243, t247);
  t248 = ((int64_t)(v1));
  t249 = ((int64_t)(((int64_t)UINT64_C(4))));
  t250 = ((int64_t)(v0));
  t251 = ((int64_t)(((int64_t)UINT64_C(4))));
  t252 = fg153_gh_fget(t250, t251);
  t253 = ((float)(t252));
  fg153_gc_putf(t248, t249, t253);
  t254 = ((int64_t)(v1));
  t255 = ((int64_t)(((int64_t)UINT64_C(5))));
  t256 = ((int64_t)(v0));
  t257 = ((int64_t)(((int64_t)UINT64_C(5))));
  t258 = fg153_gh_fget(t256, t257);
  t259 = ((float)(t258));
  fg153_gc_putf(t254, t255, t259);
  t260 = ((int64_t)(v1));
  t261 = ((int64_t)(((int64_t)UINT64_C(6))));
  t262 = ((int64_t)(v0));
  t263 = ((int64_t)(((int64_t)UINT64_C(6))));
  t264 = fg153_gh_fget(t262, t263);
  t265 = ((float)(t264));
  fg153_gc_putf(t260, t261, t265);
  t266 = ((int64_t)(v1));
  t267 = ((int64_t)(((int64_t)UINT64_C(7))));
  t268 = ((int64_t)(v0));
  t269 = ((int64_t)(((int64_t)UINT64_C(7))));
  t270 = fg153_gh_fget(t268, t269);
  t271 = ((float)(t270));
  fg153_gc_putf(t266, t267, t271);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
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
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  int64_t t53;
  uint8_t t54;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(99))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v2));
  t0 = gg_function_2(t7, t8, base + 0);
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) == (t13));
  if (t14) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(9))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg153_gh_get(t15, t16, t17);
  t19 = ((int64_t)(t18));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)UINT64_C(10))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = fg153_gh_get(t22, t23, t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t26) != (t27));
  if (t28) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v3));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((t32) == (t33));
  if (t34) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(((int64_t)UINT64_C(9))));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = fg153_gh_get(t35, t36, t37);
  t39 = ((int64_t)(t38));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((t39) != (t40));
  t42 = t41;
  if (t42) {
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(((int64_t)UINT64_C(10))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = fg153_gh_get(t43, t44, t45);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t47) == (t48));
  }
  t49 = t42;
  if (t49) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(3))));
  t52 = ((t50) == (t51));
  if (t52) {
  {
  t53 = ((int64_t)(v1));
  t0 = gg_function_8(t53, base + 0);
  goto gg_done;
  }
  }
  t54 = ((uint8_t)UINT64_C(0));
  gg_function_9(t54, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  gg_header t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  {
  t0.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(99))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = gg_function_1(t1, t2, t3, t4, t5, t6, base + 1);
  gg_integer(gg_ref(&t0, gg_get_stack(base + 0)), t7);
  gg_append(gg_ref(&t0, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t0).base, (t0).len));
  }
gg_done:;
  for (t8 = 0; t8 < 1; t8++) gg_stacks[base + t8].top = 0;
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
