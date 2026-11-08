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
static uint8_t gg_eq_4(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint8_t value);
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
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_15(uint8_t p0, int64_t base);
static int64_t gg_function_19(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_23(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_31(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static uint8_t gg_function_32(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_36(int64_t p0, int64_t p1, int64_t base);
static void gg_function_15(uint8_t p0, int64_t base) {
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
static uint8_t gg_function_36(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
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
  uint8_t t11;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(7))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg153_gp_get(t1, t2, t3);
  v2 = ((int64_t)(t4));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(7))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg153_gp_get(t6, t7, t8);
  t10 = ((int64_t)(t9));
  t11 = ((t5) != (t10));
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(v2));
  if (!(((t12) < (t13)))) goto label2;
  {
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(v3));
  t16 = fg153_gp_sampler_key(t14, t15);
  v4 = ((int64_t)(t16));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(v3));
  t19 = fg153_gp_sampler_val(t17, t18);
  v5 = ((int64_t)(t19));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(v2));
  if (!(((t20) < (t21)))) goto label5;
  {
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(v6));
  t24 = fg153_gp_sampler_key(t22, t23);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(v4));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v6));
  t30 = fg153_gp_sampler_val(t28, t29);
  t31 = ((int64_t)(t30));
  t32 = ((int64_t)(v5));
  t33 = ((t31) != (t32));
  if (t33) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  v7 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto label5;
  }
  }
  t34 = v6;
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  }
  goto label4;
label5:;
  t36 = ((int64_t)(v7));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t39 = v3;
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t39) + (uint64_t)(t40)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_32(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
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
  uint8_t t26;
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
  uint8_t t48;
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
  uint8_t t89;
  uint8_t t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint8_t t103;
  uint8_t t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
  uint8_t t108;
  int64_t t109;
  int64_t t110;
  uint8_t t111;
  uint8_t t112;
  int64_t t113;
  int64_t t114;
  uint8_t t115;
  uint8_t t116;
  uint8_t t117;
  uint8_t t118;
  uint8_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint8_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  uint8_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  uint8_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  uint8_t t148;
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
  uint8_t t165;
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
  uint8_t t178;
  int64_t t179;
  int64_t t180;
  uint8_t t181;
  uint8_t t182;
  uint8_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  uint8_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  uint8_t t197;
  uint8_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  uint8_t t205;
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
  uint8_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  uint8_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  uint8_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  uint8_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  uint8_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  uint8_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  uint8_t t301;
  int64_t t302;
  int64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  uint8_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  uint8_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  uint8_t t334;
  int64_t t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  int64_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  uint8_t t345;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  uint8_t v8;
  uint8_t v9;
  int64_t v10;
  int64_t v11;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fg153_gp_ubatch(t1);
  v2 = ((int64_t)(t2));
  t3 = ((int64_t)(v1));
  t4 = fg153_gp_ubatch(t3);
  v3 = ((int64_t)(t4));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(5))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg153_gu_get(t5, t6, t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(5))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg153_gu_get(t10, t11, t12);
  t14 = ((int64_t)(t13));
  t15 = ((t9) != (t14));
  if (t15) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = fg153_gu_get(t16, t17, t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg153_gu_get(t21, t22, t23);
  t25 = ((int64_t)(t24));
  t26 = ((t20) != (t25));
  if (t26) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = fg153_gu_get(t27, t28, t29);
  t31 = ((int64_t)(t30));
  t32 = ((int64_t)(v3));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg153_gu_get(t32, t33, t34);
  t36 = ((int64_t)(t35));
  t37 = ((t31) != (t36));
  if (t37) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t38 = ((int64_t)(v2));
  t39 = ((int64_t)(((int64_t)UINT64_C(2))));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = fg153_gu_get(t38, t39, t40);
  t42 = ((int64_t)(t41));
  t43 = ((int64_t)(v3));
  t44 = ((int64_t)(((int64_t)UINT64_C(2))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = fg153_gu_get(t43, t44, t45);
  t47 = ((int64_t)(t46));
  t48 = ((t42) != (t47));
  if (t48) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t49 = ((int64_t)(v2));
  t50 = ((int64_t)(((int64_t)UINT64_C(3))));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = fg153_gu_get(t49, t50, t51);
  t53 = ((int64_t)(t52));
  t54 = ((int64_t)(v3));
  t55 = ((int64_t)(((int64_t)UINT64_C(3))));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = fg153_gu_get(t54, t55, t56);
  t58 = ((int64_t)(t57));
  t59 = ((t53) != (t58));
  if (t59) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t60 = ((int64_t)(v2));
  t61 = ((int64_t)(((int64_t)UINT64_C(6))));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = fg153_gu_get(t60, t61, t62);
  t64 = ((int64_t)(t63));
  t65 = ((int64_t)(v3));
  t66 = ((int64_t)(((int64_t)UINT64_C(6))));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = fg153_gu_get(t65, t66, t67);
  t69 = ((int64_t)(t68));
  t70 = ((t64) != (t69));
  if (t70) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t71 = ((int64_t)(v2));
  t72 = ((int64_t)(((int64_t)UINT64_C(7))));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = fg153_gu_get(t71, t72, t73);
  v4 = ((int64_t)(t74));
  t75 = ((int64_t)(v3));
  t76 = ((int64_t)(((int64_t)UINT64_C(7))));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = fg153_gu_get(t75, t76, t77);
  v5 = ((int64_t)(t78));
  t79 = ((int64_t)(v2));
  t80 = ((int64_t)(((int64_t)UINT64_C(8))));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = fg153_gu_get(t79, t80, t81);
  v6 = ((int64_t)(t82));
  t83 = ((int64_t)(v3));
  t84 = ((int64_t)(((int64_t)UINT64_C(8))));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = fg153_gu_get(t83, t84, t85);
  v7 = ((int64_t)(t86));
  t87 = ((int64_t)(v4));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((t87) == (t88));
  t90 = t89;
  if (t90) {
  t91 = ((int64_t)(v5));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((t91) == (t92));
  }
  t93 = t90;
  t94 = t93;
  if (!t94) {
  t95 = ((int64_t)(v6));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = ((t95) == (t96));
  t98 = t97;
  if (t98) {
  t99 = ((int64_t)(v7));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((t99) == (t100));
  }
  t94 = t98;
  }
  v8 = t94;
  t101 = ((int64_t)(v4));
  t102 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = ((t101) != (t102));
  t104 = t103;
  if (t104) {
  t105 = ((int64_t)(v5));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t104 = ((t105) != (t106));
  }
  t107 = t104;
  t108 = t107;
  if (t108) {
  t109 = ((int64_t)(v6));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = ((t109) != (t110));
  }
  t111 = t108;
  t112 = t111;
  if (t112) {
  t113 = ((int64_t)(v7));
  t114 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = ((t113) != (t114));
  }
  v9 = t112;
  t115 = v8;
  t116 = (!(t115));
  t117 = t116;
  if (t117) {
  t118 = v9;
  t117 = (!(t118));
  }
  t119 = t117;
  if (t119) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t120 = ((int64_t)(v2));
  t121 = ((int64_t)(((int64_t)UINT64_C(9))));
  t122 = ((int64_t)(((int64_t)UINT64_C(0))));
  t123 = fg153_gu_get(t120, t121, t122);
  t124 = ((int64_t)(t123));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((t124) == (t125));
  t127 = ((int64_t)(v3));
  t128 = ((int64_t)(((int64_t)UINT64_C(9))));
  t129 = ((int64_t)(((int64_t)UINT64_C(0))));
  t130 = fg153_gu_get(t127, t128, t129);
  t131 = ((int64_t)(t130));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t133 = ((t131) == (t132));
  t134 = ((t126) != (t133));
  if (t134) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t135 = ((int64_t)(v2));
  t136 = ((int64_t)(((int64_t)UINT64_C(5))));
  t137 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = fg153_gu_get(t135, t136, t137);
  t139 = ((int64_t)(t138));
  t140 = ((int64_t)(((int64_t)UINT64_C(0))));
  t141 = ((t139) != (t140));
  if (t141) {
  {
  t142 = ((int64_t)(v2));
  t143 = ((int64_t)(((int64_t)UINT64_C(12))));
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t145 = fg153_gu_get(t142, t143, t144);
  t146 = ((int64_t)(t145));
  t147 = ((int64_t)(((int64_t)UINT64_C(0))));
  t148 = ((t146) == (t147));
  if (t148) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t149 = ((int64_t)(v10));
  t150 = ((int64_t)(v2));
  t151 = ((int64_t)(((int64_t)UINT64_C(3))));
  t152 = ((int64_t)(((int64_t)UINT64_C(0))));
  t153 = fg153_gu_get(t150, t151, t152);
  t154 = ((int64_t)(t153));
  if (!(((t149) < (t154)))) goto label2;
  {
  t155 = ((int64_t)(v2));
  t156 = ((int64_t)(((int64_t)UINT64_C(18))));
  t157 = ((int64_t)(v10));
  t158 = fg153_gu_get(t155, t156, t157);
  t159 = ((int64_t)(t158));
  t160 = ((int64_t)(v3));
  t161 = ((int64_t)(((int64_t)UINT64_C(18))));
  t162 = ((int64_t)(v10));
  t163 = fg153_gu_get(t160, t161, t162);
  t164 = ((int64_t)(t163));
  t165 = ((t159) != (t164));
  if (t165) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t166 = v10;
  t167 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t166) + (uint64_t)(t167)));
  }
  goto label1;
label2:;
  }
  }
  t168 = ((int64_t)(v0));
  t169 = ((int64_t)(((int64_t)UINT64_C(1))));
  t170 = ((int64_t)(((int64_t)UINT64_C(0))));
  t171 = fg153_gp_get(t168, t169, t170);
  t172 = ((int64_t)(t171));
  t173 = ((int64_t)(v1));
  t174 = ((int64_t)(((int64_t)UINT64_C(1))));
  t175 = ((int64_t)(((int64_t)UINT64_C(0))));
  t176 = fg153_gp_get(t173, t174, t175);
  t177 = ((int64_t)(t176));
  t178 = ((t172) != (t177));
  if (t178) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t179 = ((int64_t)(v0));
  t180 = ((int64_t)(v1));
  t181 = gg_function_36(t179, t180, base + 0);
  t182 = t181;
  t183 = (!(t182));
  if (t183) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t184 = ((int64_t)(v0));
  t185 = ((int64_t)(((int64_t)UINT64_C(7))));
  t186 = ((int64_t)(((int64_t)UINT64_C(0))));
  t187 = fg153_gp_get(t184, t185, t186);
  t188 = ((int64_t)(t187));
  t189 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = ((t188) > (t189));
  if (t190) {
  {
  t191 = ((int64_t)(v2));
  t192 = ((int64_t)(((int64_t)UINT64_C(12))));
  t193 = ((int64_t)(((int64_t)UINT64_C(0))));
  t194 = fg153_gu_get(t191, t192, t193);
  t195 = ((int64_t)(t194));
  t196 = ((int64_t)(((int64_t)UINT64_C(0))));
  t197 = ((t195) == (t196));
  t198 = t197;
  if (!t198) {
  t199 = ((int64_t)(v3));
  t200 = ((int64_t)(((int64_t)UINT64_C(12))));
  t201 = ((int64_t)(((int64_t)UINT64_C(0))));
  t202 = fg153_gu_get(t199, t200, t201);
  t203 = ((int64_t)(t202));
  t204 = ((int64_t)(((int64_t)UINT64_C(0))));
  t198 = ((t203) == (t204));
  }
  t205 = t198;
  if (t205) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t206 = ((int64_t)(v11));
  t207 = ((int64_t)(v2));
  t208 = ((int64_t)(((int64_t)UINT64_C(0))));
  t209 = ((int64_t)(((int64_t)UINT64_C(0))));
  t210 = fg153_gu_get(t207, t208, t209);
  t211 = ((int64_t)(t210));
  if (!(((t206) < (t211)))) goto label5;
  {
  t212 = ((int64_t)(v2));
  t213 = ((int64_t)(((int64_t)UINT64_C(16))));
  t214 = ((int64_t)(v11));
  t215 = fg153_gu_get(t212, t213, t214);
  t216 = ((int64_t)(t215));
  t217 = ((int64_t)(v3));
  t218 = ((int64_t)(((int64_t)UINT64_C(16))));
  t219 = ((int64_t)(v11));
  t220 = fg153_gu_get(t217, t218, t219);
  t221 = ((int64_t)(t220));
  t222 = ((t216) != (t221));
  if (t222) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t223 = ((int64_t)(v2));
  t224 = ((int64_t)(v11));
  t225 = ((int64_t)(((int64_t)UINT64_C(0))));
  t226 = fg153_gu_seq_id(t223, t224, t225);
  t227 = ((int64_t)(t226));
  t228 = ((int64_t)(v3));
  t229 = ((int64_t)(v11));
  t230 = ((int64_t)(((int64_t)UINT64_C(0))));
  t231 = fg153_gu_seq_id(t228, t229, t230);
  t232 = ((int64_t)(t231));
  t233 = ((t227) != (t232));
  if (t233) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t234 = v11;
  t235 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t234) + (uint64_t)(t235)));
  }
  goto label4;
label5:;
  }
  }
  t236 = ((int64_t)(v0));
  t237 = ((int64_t)(((int64_t)UINT64_C(12))));
  t238 = ((int64_t)(((int64_t)UINT64_C(0))));
  t239 = fg153_gp_get(t236, t237, t238);
  t240 = ((int64_t)(t239));
  t241 = ((int64_t)(v1));
  t242 = ((int64_t)(((int64_t)UINT64_C(12))));
  t243 = ((int64_t)(((int64_t)UINT64_C(0))));
  t244 = fg153_gp_get(t241, t242, t243);
  t245 = ((int64_t)(t244));
  t246 = ((t240) != (t245));
  if (t246) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t247 = ((int64_t)(v0));
  t248 = ((int64_t)(((int64_t)UINT64_C(8))));
  t249 = ((int64_t)(((int64_t)UINT64_C(0))));
  t250 = fg153_gp_get(t247, t248, t249);
  t251 = ((int64_t)(t250));
  t252 = ((int64_t)(v1));
  t253 = ((int64_t)(((int64_t)UINT64_C(8))));
  t254 = ((int64_t)(((int64_t)UINT64_C(0))));
  t255 = fg153_gp_get(t252, t253, t254);
  t256 = ((int64_t)(t255));
  t257 = ((t251) != (t256));
  if (t257) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t258 = ((int64_t)(v0));
  t259 = ((int64_t)(((int64_t)UINT64_C(9))));
  t260 = ((int64_t)(((int64_t)UINT64_C(0))));
  t261 = fg153_gp_get(t258, t259, t260);
  t262 = ((int64_t)(t261));
  t263 = ((int64_t)(v1));
  t264 = ((int64_t)(((int64_t)UINT64_C(9))));
  t265 = ((int64_t)(((int64_t)UINT64_C(0))));
  t266 = fg153_gp_get(t263, t264, t265);
  t267 = ((int64_t)(t266));
  t268 = ((t262) != (t267));
  if (t268) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t269 = ((int64_t)(v0));
  t270 = ((int64_t)(((int64_t)UINT64_C(10))));
  t271 = ((int64_t)(((int64_t)UINT64_C(0))));
  t272 = fg153_gp_get(t269, t270, t271);
  t273 = ((int64_t)(t272));
  t274 = ((int64_t)(v1));
  t275 = ((int64_t)(((int64_t)UINT64_C(10))));
  t276 = ((int64_t)(((int64_t)UINT64_C(0))));
  t277 = fg153_gp_get(t274, t275, t276);
  t278 = ((int64_t)(t277));
  t279 = ((t273) != (t278));
  if (t279) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t280 = ((int64_t)(v0));
  t281 = ((int64_t)(((int64_t)UINT64_C(11))));
  t282 = ((int64_t)(((int64_t)UINT64_C(0))));
  t283 = fg153_gp_get(t280, t281, t282);
  t284 = ((int64_t)(t283));
  t285 = ((int64_t)(v1));
  t286 = ((int64_t)(((int64_t)UINT64_C(11))));
  t287 = ((int64_t)(((int64_t)UINT64_C(0))));
  t288 = fg153_gp_get(t285, t286, t287);
  t289 = ((int64_t)(t288));
  t290 = ((t284) != (t289));
  if (t290) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t291 = ((int64_t)(v0));
  t292 = ((int64_t)(((int64_t)UINT64_C(0))));
  t293 = ((int64_t)(((int64_t)UINT64_C(0))));
  t294 = fg153_gp_get(t291, t292, t293);
  t295 = ((int64_t)(t294));
  t296 = ((int64_t)(v1));
  t297 = ((int64_t)(((int64_t)UINT64_C(0))));
  t298 = ((int64_t)(((int64_t)UINT64_C(0))));
  t299 = fg153_gp_get(t296, t297, t298);
  t300 = ((int64_t)(t299));
  t301 = ((t295) != (t300));
  if (t301) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t302 = ((int64_t)(v0));
  t303 = ((int64_t)(((int64_t)UINT64_C(2))));
  t304 = ((int64_t)(((int64_t)UINT64_C(0))));
  t305 = fg153_gp_get(t302, t303, t304);
  t306 = ((int64_t)(t305));
  t307 = ((int64_t)(v1));
  t308 = ((int64_t)(((int64_t)UINT64_C(2))));
  t309 = ((int64_t)(((int64_t)UINT64_C(0))));
  t310 = fg153_gp_get(t307, t308, t309);
  t311 = ((int64_t)(t310));
  t312 = ((t306) != (t311));
  if (t312) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t313 = ((int64_t)(v0));
  t314 = ((int64_t)(((int64_t)UINT64_C(3))));
  t315 = ((int64_t)(((int64_t)UINT64_C(0))));
  t316 = fg153_gp_get(t313, t314, t315);
  t317 = ((int64_t)(t316));
  t318 = ((int64_t)(v1));
  t319 = ((int64_t)(((int64_t)UINT64_C(3))));
  t320 = ((int64_t)(((int64_t)UINT64_C(0))));
  t321 = fg153_gp_get(t318, t319, t320);
  t322 = ((int64_t)(t321));
  t323 = ((t317) != (t322));
  if (t323) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t324 = ((int64_t)(v0));
  t325 = ((int64_t)(((int64_t)UINT64_C(4))));
  t326 = ((int64_t)(((int64_t)UINT64_C(0))));
  t327 = fg153_gp_get(t324, t325, t326);
  t328 = ((int64_t)(t327));
  t329 = ((int64_t)(v1));
  t330 = ((int64_t)(((int64_t)UINT64_C(4))));
  t331 = ((int64_t)(((int64_t)UINT64_C(0))));
  t332 = fg153_gp_get(t329, t330, t331);
  t333 = ((int64_t)(t332));
  t334 = ((t328) != (t333));
  if (t334) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t335 = ((int64_t)(v0));
  t336 = ((int64_t)(((int64_t)UINT64_C(5))));
  t337 = ((int64_t)(((int64_t)UINT64_C(0))));
  t338 = fg153_gp_get(t335, t336, t337);
  t339 = ((int64_t)(t338));
  t340 = ((int64_t)(v1));
  t341 = ((int64_t)(((int64_t)UINT64_C(5))));
  t342 = ((int64_t)(((int64_t)UINT64_C(0))));
  t343 = fg153_gp_get(t340, t341, t342);
  t344 = ((int64_t)(t343));
  t345 = ((t339) != (t344));
  if (t345) {
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
static int64_t gg_function_31(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
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
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(v1));
  t3 = gg_function_32(t1, t2, base + 0);
  t4 = t3;
  t5 = (!(t4));
  if (t5) {
  {
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((t6) > (t7));
  if (t8) {
  {
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gx_log(t9, t10, t11);
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = ((t12) > (t13));
  if (t14) {
  {
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((int64_t)(v0));
  t17 = fg153_gr_inputs_len(t16);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gx_log(t15, t18, t19);
  }
  }
  t20 = ((int64_t)(v0));
  t21 = fg153_gr_inputs_len(t20);
  v4 = ((int64_t)(t21));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  v6 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(v4));
  if (!(((t22) < (t23)))) goto label2;
  {
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(v5));
  t26 = fg153_gr_input_at(t24, t25);
  t27 = ((int64_t)(t26));
  t28 = ((int64_t)(v1));
  t29 = fg153_gi_can_reuse(t27, t28);
  v7 = ((int64_t)(t29));
  t30 = ((int64_t)(v3));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = ((t30) > (t31));
  if (t32) {
  {
  t33 = ((int64_t)(((int64_t)UINT64_C(2))));
  t34 = ((int64_t)(v7));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gx_log(t33, t34, t35);
  }
  }
  t36 = ((int64_t)(v7));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t39 = v5;
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t39) + (uint64_t)(t40)));
  }
  goto label1;
label2:;
  t41 = ((int64_t)(v3));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t41) > (t42));
  if (t43) {
  {
  t44 = ((int64_t)(((int64_t)UINT64_C(3))));
  t45 = ((int64_t)(v6));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gx_log(t44, t45, t46);
  }
  }
  t0 = ((int64_t)(v6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_23(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
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
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = fg153_gr_tensor_get(t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  t9 = fg153_gr_tensor_get(t7, t8);
  t10 = ((int64_t)(t9));
  fg153_gx_set_output(t10);
  }
  }
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(3))));
  t13 = fg153_gr_tensor_get(t11, t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) != (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(3))));
  t19 = fg153_gr_tensor_get(t17, t18);
  t20 = ((int64_t)(t19));
  fg153_gx_set_output(t20);
  }
  }
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(4))));
  t23 = fg153_gr_tensor_get(t21, t22);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) != (t25));
  if (t26) {
  {
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  t29 = fg153_gr_tensor_get(t27, t28);
  t30 = ((int64_t)(t29));
  fg153_gx_set_output(t30);
  }
  }
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(5))));
  t33 = fg153_gr_tensor_get(t31, t32);
  t34 = ((int64_t)(t33));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) != (t35));
  if (t36) {
  {
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(5))));
  t39 = fg153_gr_tensor_get(t37, t38);
  t40 = ((int64_t)(t39));
  fg153_gx_set_output(t40);
  }
  }
  t41 = ((int64_t)(v1));
  t42 = fg153_gp_layer_inp_size(t41);
  v2 = ((int64_t)(t42));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t43 = ((int64_t)(v3));
  t44 = ((int64_t)(v2));
  if (!(((t43) < (t44)))) goto label2;
  {
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(v3));
  t47 = fg153_gp_layer_inp_at(t45, t46);
  t48 = ((int64_t)(t47));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((t48) != (t49));
  if (t50) {
  {
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(v3));
  t53 = fg153_gr_layer_inp_get(t51, t52);
  v4 = ((int64_t)(t53));
  t54 = ((int64_t)(v4));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = ((t54) != (t55));
  gg_function_15(t56, base + 0);
  t57 = ((int64_t)(v4));
  fg153_gx_set_output(t57);
  }
  }
  t58 = v3;
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t59)));
  }
  goto label1;
label2:;
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t60 = ((int64_t)(v5));
  t61 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t60) < (t61)))) goto label5;
  {
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(v5));
  t64 = fg153_gr_sampling_len(t62, t63);
  v6 = ((int64_t)(t64));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t65 = ((int64_t)(v7));
  t66 = ((int64_t)(v6));
  if (!(((t65) < (t66)))) goto label8;
  {
  t67 = ((int64_t)(v0));
  t68 = ((int64_t)(v5));
  t69 = ((int64_t)(v7));
  t70 = fg153_gr_sampling_get(t67, t68, t69);
  v8 = ((int64_t)(t70));
  t71 = ((int64_t)(v8));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((t71) != (t72));
  if (t73) {
  {
  t74 = ((int64_t)(v8));
  fg153_gx_set_output(t74);
  }
  }
  t75 = v7;
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t75) + (uint64_t)(t76)));
  }
  goto label7;
label8:;
  t77 = v5;
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t77) + (uint64_t)(t78)));
  }
  goto label4;
label5:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_19(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fg153_gr_inputs_len(t1);
  v2 = ((int64_t)(t2));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v2));
  if (!(((t3) < (t4)))) goto label2;
  {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(v3));
  t7 = fg153_gr_input_at(t5, t6);
  v4 = ((int64_t)(t7));
  t8 = ((int64_t)(v4));
  t9 = ((int64_t)(v1));
  t10 = fg153_gi_set_input(t8, t9);
  v5 = ((int64_t)(t10));
  t11 = v3;
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t11) + (uint64_t)(t12)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gr_tensor_set(t1, t2, t3);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gr_tensor_set(t4, t5, t6);
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gr_tensor_set(t7, t8, t9);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(3))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gr_tensor_set(t10, t11, t12);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gr_tensor_set(t13, t14, t15);
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(5))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gr_tensor_set(t16, t17, t18);
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg153_gr_const(t20);
  t22 = ((int64_t)(t21));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))));
  fg153_gr_layer_inp_reset(t19, t24);
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg153_gr_sampling_clear(t25, t26);
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg153_gr_sampling_clear(t27, t28);
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg153_gr_sampling_clear(t29, t30);
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg153_gr_sampling_clear(t31, t32);
  t33 = ((int64_t)(v2));
  fg153_gr_params_clear(t33);
  t34 = ((int64_t)(v0));
  fg153_gr_inputs_clear(t34);
  t35 = ((int64_t)(v0));
  fg153_gr_fused_clear(t35);
  t36 = ((int64_t)(v0));
  fg153_gr_hdmd_clear(t36);
  t37 = fg153_gr_tensor_overhead();
  t38 = ((int64_t)(t37));
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  t41 = ((int64_t)(v1));
  t42 = fg153_gr_graph_overhead(t41);
  t43 = ((int64_t)(t42));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))));
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(v3));
  fg153_gr_meta_resize(t44, t45);
  t46 = ((int64_t)(v0));
  t47 = fg153_gr_ctx_init(t46);
  v4 = ((int64_t)(t47));
  t48 = ((int64_t)(v4));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((t48) != (t49));
  gg_function_15(t50, base + 0);
  t51 = ((int64_t)(v4));
  t52 = ((int64_t)(v1));
  t53 = fg153_gr_new_graph(t51, t52);
  v5 = ((int64_t)(t53));
  t54 = ((int64_t)(v5));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = ((t54) != (t55));
  gg_function_15(t56, base + 0);
  t57 = ((int64_t)(v0));
  t58 = ((int64_t)(v5));
  fg153_gr_gf_set(t57, t58);
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
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
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
  uint8_t t29;
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
  int64_t t40;
  int64_t t41;
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
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
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
  t9 = ((int64_t)(v3));
  t0 = gg_function_2(t7, t8, t9, base + 0);
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v2));
  t0 = gg_function_19(t13, t14, base + 0);
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(v2));
  t0 = gg_function_23(t18, t19, base + 0);
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(3))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(v3));
  t26 = ((int64_t)(v4));
  t0 = gg_function_31(t23, t24, t25, t26, base + 0);
  goto gg_done;
  }
  }
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  t29 = ((t27) == (t28));
  if (t29) {
  {
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(v2));
  t32 = fg153_gr_inputs_add(t30, t31);
  t0 = ((int64_t)(t32));
  goto gg_done;
  }
  }
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(5))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(v2));
  t38 = ((int64_t)(v3));
  t39 = ((int64_t)(v4));
  fg153_gr_fused_push(t36, t37, t38, t39);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(6))));
  t42 = ((t40) == (t41));
  if (t42) {
  {
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(v2));
  t45 = ((int64_t)(v3));
  t46 = fg153_gr_hdmd_get(t43, t44, t45);
  t0 = ((int64_t)(t46));
  goto gg_done;
  }
  }
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(7))));
  t49 = ((t47) == (t48));
  if (t49) {
  {
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(v2));
  t52 = ((int64_t)(v3));
  t53 = ((int64_t)(v4));
  fg153_gr_hdmd_set(t50, t51, t52, t53);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(((int64_t)UINT64_C(8))));
  t56 = ((t54) == (t55));
  if (t56) {
  {
  t57 = ((int64_t)(v3));
  t58 = ((int64_t)(v2));
  fg153_gr_params_copy(t57, t58);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(9))));
  t61 = ((t59) == (t60));
  if (t61) {
  {
  t62 = ((int64_t)(v1));
  t63 = fg153_gr_layer_inp_len(t62);
  t0 = ((int64_t)(t63));
  goto gg_done;
  }
  }
  t64 = ((uint8_t)UINT64_C(0));
  gg_function_15(t64, base + 0);
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
