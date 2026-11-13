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
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(float a, float b) {
return a == b;
}
static void gg_render_3(gg_reference out, float value) {
gg_float32(out,value);
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
static void gg_function_1(int64_t p0, int64_t base);
static void gg_function_4(uint8_t p0, int64_t p1, int64_t base);
static void gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t base);
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
  fg166_fail(t2);
  }
  }
  }
gg_done:;
}
static void gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(3))));
  if (!(((t0) < (t1)))) goto label2;
  {
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(7))));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t4)))));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(3))));
  t8 = ((int64_t)(v3));
  t9 = fg166_ti(t6, t7, t8);
  t10 = ((int64_t)(t9));
  fg166_hi(t2, t5, t10);
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(14))));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(4))));
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = fg166_ti(t15, t16, t19);
  t21 = ((int64_t)(t20));
  fg166_hi(t11, t14, t21);
  t22 = v3;
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  }
  goto label1;
label2:;
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t24) < (t25)))) goto label5;
  {
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(10))));
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(((int64_t)UINT64_C(4))));
  t32 = ((int64_t)(v3));
  t33 = fg166_ti(t30, t31, t32);
  t34 = ((int64_t)(t33));
  fg166_hi(t26, t29, t34);
  t35 = v3;
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  }
  goto label4;
label5:;
  }
gg_done:;
}
static void gg_function_1(int64_t p0, int64_t base) {
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
  uint8_t t14;
  uint8_t t15;
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
  int64_t t28;
  uint8_t t29;
  uint8_t t30;
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
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
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
  uint8_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  uint8_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  uint8_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint8_t t106;
  uint8_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint8_t t112;
  uint8_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  uint8_t t118;
  int64_t t119;
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
  uint8_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  uint8_t t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  uint8_t t147;
  uint8_t t148;
  int64_t t149;
  int64_t t150;
  uint8_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint8_t t155;
  uint8_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  uint8_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  uint8_t t167;
  uint8_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  uint8_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  uint8_t t183;
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
  uint8_t t215;
  int64_t t216;
  int64_t t217;
  uint8_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  uint8_t t226;
  int64_t t227;
  int64_t t228;
  uint8_t t229;
  uint8_t t230;
  int64_t t231;
  int64_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  uint8_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  uint8_t t247;
  uint8_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  uint8_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  uint8_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  uint8_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  uint8_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  uint8_t t277;
  uint8_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  uint8_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  uint8_t t289;
  uint8_t t290;
  int64_t t291;
  int64_t t292;
  uint8_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  uint8_t t310;
  uint8_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  uint8_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  uint8_t t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  int64_t t334;
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
  int64_t t346;
  int64_t t347;
  int64_t t348;
  int64_t t349;
  int64_t t350;
  int64_t t351;
  uint8_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  uint8_t t359;
  uint8_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t t364;
  int64_t t365;
  int64_t t366;
  uint8_t t367;
  uint8_t t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  int64_t t372;
  int64_t t373;
  int64_t t374;
  uint8_t t375;
  uint8_t t376;
  int64_t t377;
  int64_t t378;
  int64_t t379;
  int64_t t380;
  int64_t t381;
  int64_t t382;
  uint8_t t383;
  uint8_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  int64_t t388;
  int64_t t389;
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
  uint8_t t403;
  int64_t t404;
  int64_t t405;
  int64_t t406;
  int64_t t407;
  int64_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  int64_t t413;
  int64_t t414;
  int64_t t415;
  int64_t t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  int64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  int64_t t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  int64_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  int64_t t434;
  uint8_t t435;
  int64_t t436;
  int64_t t437;
  uint8_t t438;
  int64_t t439;
  int64_t t440;
  int64_t t441;
  int64_t t442;
  int64_t t443;
  float t444;
  float t445;
  int64_t t446;
  int64_t t447;
  int64_t t448;
  int64_t t449;
  float t450;
  float t451;
  int64_t t452;
  int64_t t453;
  uint8_t t454;
  int64_t t455;
  int64_t t456;
  uint8_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  int64_t t461;
  int64_t t462;
  float t463;
  float t464;
  int64_t t465;
  int64_t t466;
  uint8_t t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
  int64_t t472;
  int64_t t473;
  uint8_t t474;
  uint8_t t475;
  int64_t t476;
  int64_t t477;
  uint8_t t478;
  int64_t t479;
  int64_t t480;
  int64_t t481;
  int64_t t482;
  int64_t t483;
  int64_t t484;
  int64_t t485;
  int64_t t486;
  int64_t t487;
  float t488;
  float t489;
  int64_t t490;
  int64_t t491;
  int64_t t492;
  int64_t t493;
  int64_t t494;
  int64_t t495;
  uint8_t t496;
  int64_t t497;
  int64_t t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  int64_t t502;
  uint8_t t503;
  uint8_t t504;
  int64_t t505;
  int64_t t506;
  uint8_t t507;
  uint8_t t508;
  int64_t t509;
  int64_t t510;
  uint8_t t511;
  int64_t t512;
  int64_t t513;
  int64_t t514;
  int64_t t515;
  int64_t t516;
  int64_t t517;
  int64_t t518;
  uint8_t t519;
  uint8_t t520;
  int64_t t521;
  int64_t t522;
  int64_t t523;
  int64_t t524;
  int64_t t525;
  int64_t t526;
  uint8_t t527;
  uint8_t t528;
  int64_t t529;
  int64_t t530;
  int64_t t531;
  int64_t t532;
  int64_t t533;
  int64_t t534;
  uint8_t t535;
  uint8_t t536;
  int64_t t537;
  int64_t t538;
  int64_t t539;
  int64_t t540;
  int64_t t541;
  int64_t t542;
  uint8_t t543;
  int64_t t544;
  int64_t t545;
  int64_t t546;
  int64_t t547;
  int64_t t548;
  int64_t t549;
  int64_t t550;
  int64_t t551;
  int64_t t552;
  int64_t t553;
  int64_t t554;
  int64_t t555;
  int64_t t556;
  int64_t t557;
  int64_t t558;
  int64_t t559;
  int64_t t560;
  int64_t t561;
  int64_t t562;
  uint8_t t563;
  int64_t t564;
  int64_t t565;
  int64_t t566;
  int64_t t567;
  int64_t t568;
  int64_t t569;
  uint8_t t570;
  uint8_t t571;
  int64_t t572;
  int64_t t573;
  uint8_t t574;
  uint8_t t575;
  int64_t t576;
  int64_t t577;
  uint8_t t578;
  int64_t t579;
  int64_t t580;
  int64_t t581;
  int64_t t582;
  int64_t t583;
  float t584;
  float t585;
  int64_t t586;
  int64_t t587;
  int64_t t588;
  int64_t t589;
  uint8_t t590;
  int64_t t591;
  int64_t t592;
  uint8_t t593;
  int64_t t594;
  int64_t t595;
  int64_t t596;
  int64_t t597;
  int64_t t598;
  float t599;
  float t600;
  int64_t t601;
  int64_t t602;
  int64_t t603;
  int64_t t604;
  float t605;
  float t606;
  int64_t t607;
  int64_t t608;
  int64_t t609;
  int64_t t610;
  uint8_t t611;
  int64_t t612;
  int64_t t613;
  uint8_t t614;
  uint8_t t615;
  int64_t t616;
  int64_t t617;
  uint8_t t618;
  uint8_t t619;
  int64_t t620;
  int64_t t621;
  int64_t t622;
  int64_t t623;
  int64_t t624;
  int64_t t625;
  uint8_t t626;
  uint8_t t627;
  int64_t t628;
  int64_t t629;
  int64_t t630;
  int64_t t631;
  int64_t t632;
  int64_t t633;
  uint8_t t634;
  int64_t t635;
  int64_t t636;
  int64_t t637;
  float t638;
  int64_t t639;
  int64_t t640;
  float t641;
  int64_t t642;
  int64_t t643;
  float t644;
  int64_t t645;
  int64_t t646;
  uint8_t t647;
  float t648;
  float t649;
  float t650;
  int64_t t651;
  int64_t t652;
  float t653;
  int64_t t654;
  int64_t t655;
  uint8_t t656;
  float t657;
  float t658;
  float t659;
  int64_t t660;
  int64_t t661;
  int64_t t662;
  int64_t t663;
  uint8_t t664;
  int64_t t665;
  int64_t t666;
  int64_t t667;
  int64_t t668;
  int64_t t669;
  int64_t t670;
  uint8_t t671;
  uint8_t t672;
  int64_t t673;
  int64_t t674;
  uint8_t t675;
  uint8_t t676;
  int64_t t677;
  int64_t t678;
  uint8_t t679;
  int64_t t680;
  int64_t t681;
  int64_t t682;
  uint8_t t683;
  int64_t t684;
  int64_t t685;
  int64_t t686;
  int64_t t687;
  int64_t t688;
  int64_t t689;
  int64_t t690;
  int64_t t691;
  int64_t t692;
  uint8_t t693;
  int64_t t694;
  int64_t t695;
  int64_t t696;
  int64_t t697;
  int64_t t698;
  int64_t t699;
  uint8_t t700;
  uint8_t t701;
  int64_t t702;
  int64_t t703;
  uint8_t t704;
  int64_t t705;
  int64_t t706;
  int64_t t707;
  int64_t t708;
  int64_t t709;
  int64_t t710;
  int64_t t711;
  int64_t t712;
  int64_t t713;
  int64_t t714;
  int64_t t715;
  int64_t t716;
  int64_t t717;
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
  uint8_t v21;
  int64_t v22;
  int64_t v23;
  float v24;
  float v25;
  int64_t v26;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg166_arg(t0, t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  t5 = fg166_arg(t3, t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(3))));
  t8 = fg166_arg(t6, t7);
  v3 = ((int64_t)(t8));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = fg166_info(t9, t10);
  v4 = ((int64_t)(t11));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) != (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = fg166_arg(t16, t17);
  t19 = ((int64_t)(t18));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t19) != (t20));
  }
  t21 = t15;
  t22 = ((int64_t)(((int64_t)UINT64_C(21))));
  gg_function_4(t21, t22, base + 0);
  v5 = ((int64_t)(v2));
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg166_ti(t23, t24, t25);
  v6 = ((int64_t)(t26));
  t27 = ((int64_t)(v6));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t27) != (t28));
  t30 = t29;
  if (t30) {
  t31 = ((int64_t)(v4));
  t32 = ((int64_t)(((int64_t)UINT64_C(7))));
  t33 = ((t31) < (t32));
  t34 = t33;
  if (!t34) {
  t35 = ((int64_t)(v3));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((t35) != (t36));
  }
  t30 = t34;
  }
  t37 = t30;
  t38 = ((int64_t)(((int64_t)UINT64_C(22))));
  gg_function_4(t37, t38, base + 0);
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = fg166_info(t39, t40);
  v8 = ((int64_t)(t41));
  t42 = ((int64_t)(v1));
  t43 = ((int64_t)(((int64_t)UINT64_C(1))));
  t44 = fg166_info(t42, t43);
  v9 = ((int64_t)(t44));
  t45 = ((int64_t)(v4));
  t46 = ((int64_t)(((int64_t)UINT64_C(8))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  v5 = ((int64_t)(v3));
  }
  }
  t48 = ((int64_t)(v4));
  t49 = ((int64_t)(((int64_t)UINT64_C(7))));
  t50 = ((t48) == (t49));
  if (t50) {
  {
  v5 = ((int64_t)(v3));
  t51 = ((int64_t)(v3));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = fg166_ti(t51, t52, t53);
  t55 = ((int64_t)(t54));
  t56 = ((int64_t)(v2));
  t57 = ((t55) == (t56));
  if (t57) {
  {
  t58 = ((int64_t)(v3));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  t61 = fg166_ti(t58, t59, t60);
  v7 = ((int64_t)(t61));
  }
  } else {
  {
  t62 = ((int64_t)(v3));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = fg166_ti(t62, t63, t64);
  v7 = ((int64_t)(t65));
  }
  }
  t66 = ((int64_t)(((int64_t)UINT64_C(3))));
  t67 = ((int64_t)(v6));
  t68 = ((int64_t)(v7));
  t69 = fg166_pred(t66, t67, t68);
  t70 = ((int64_t)(t69));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((t70) != (t71));
  t73 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_4(t72, t73, base + 0);
  t74 = ((int64_t)(v2));
  t75 = ((int64_t)(((int64_t)UINT64_C(7))));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = fg166_ti(t74, t75, t76);
  v10 = ((int64_t)(t77));
  t78 = ((int64_t)(v10));
  t79 = ((int64_t)(((int64_t)UINT64_C(8))));
  t80 = fg166_constant(t79);
  t81 = ((int64_t)(t80));
  t82 = ((t78) == (t81));
  if (t82) {
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(7))));
  }
  } else {
  {
  t83 = ((int64_t)(v10));
  t84 = ((int64_t)(((int64_t)UINT64_C(9))));
  t85 = fg166_constant(t84);
  t86 = ((int64_t)(t85));
  t87 = ((t83) == (t86));
  if (t87) {
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(10))));
  }
  } else {
  {
  t88 = ((int64_t)(v10));
  t89 = ((int64_t)(((int64_t)UINT64_C(10))));
  t90 = fg166_constant(t89);
  t91 = ((int64_t)(t90));
  t92 = ((t88) == (t91));
  if (t92) {
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(25))));
  }
  } else {
  {
  t93 = ((int64_t)UINT64_C(1));
  v9 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t93)))));
  }
  }
  }
  }
  }
  }
  t94 = ((int64_t)(v9));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = ((t94) >= (t95));
  t97 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_function_4(t96, t97, base + 0);
  }
  }
  t98 = ((int64_t)(v6));
  t99 = ((int64_t)(((int64_t)UINT64_C(2))));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = fg166_ti(t98, t99, t100);
  v11 = ((int64_t)(t101));
  t102 = ((int64_t)(v11));
  t103 = ((int64_t)(((int64_t)UINT64_C(0))));
  t104 = fg166_constant(t103);
  t105 = ((int64_t)(t104));
  t106 = ((t102) == (t105));
  t107 = t106;
  if (!t107) {
  t108 = ((int64_t)(v11));
  t109 = ((int64_t)(((int64_t)UINT64_C(1))));
  t110 = fg166_constant(t109);
  t111 = ((int64_t)(t110));
  t107 = ((t108) == (t111));
  }
  t112 = t107;
  t113 = t112;
  if (!t113) {
  t114 = ((int64_t)(v11));
  t115 = ((int64_t)(((int64_t)UINT64_C(2))));
  t116 = fg166_constant(t115);
  t117 = ((int64_t)(t116));
  t113 = ((t114) == (t117));
  }
  t118 = t113;
  t119 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_4(t118, t119, base + 0);
  t120 = ((int64_t)(v5));
  t121 = ((int64_t)(((int64_t)UINT64_C(2))));
  t122 = ((int64_t)(((int64_t)UINT64_C(0))));
  t123 = fg166_ti(t120, t121, t122);
  t124 = ((int64_t)(t123));
  t125 = ((int64_t)(v11));
  t126 = ((t124) == (t125));
  t127 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_4(t126, t127, base + 0);
  t128 = ((int64_t)(v11));
  t129 = ((int64_t)(((int64_t)UINT64_C(0))));
  t130 = fg166_constant(t129);
  t131 = ((int64_t)(t130));
  t132 = ((t128) == (t131));
  if (t132) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t133 = ((int64_t)(v11));
  t134 = ((int64_t)(((int64_t)UINT64_C(1))));
  t135 = fg166_constant(t134);
  t136 = ((int64_t)(t135));
  t137 = ((t133) == (t136));
  if (t137) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  }
  }
  }
  t138 = ((int64_t)(v12));
  t139 = ((int64_t)(((int64_t)UINT64_C(0))));
  t140 = ((t138) == (t139));
  if (t140) {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  } else {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  }
  t141 = ((int64_t)(v5));
  t142 = ((int64_t)(((int64_t)UINT64_C(5))));
  t143 = ((int64_t)(((int64_t)UINT64_C(0))));
  t144 = fg166_ti(t141, t142, t143);
  v14 = ((int64_t)(t144));
  t145 = ((int64_t)(v14));
  t146 = ((int64_t)(((int64_t)UINT64_C(0))));
  t147 = ((t145) >= (t146));
  t148 = t147;
  if (t148) {
  t149 = ((int64_t)(v14));
  t150 = ((int64_t)(((int64_t)UINT64_C(9223372036854775552))));
  t148 = ((t149) <= (t150));
  }
  t151 = t148;
  t152 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_4(t151, t152, base + 0);
  t153 = ((int64_t)(v4));
  t154 = ((int64_t)(((int64_t)UINT64_C(1))));
  t155 = ((t153) == (t154));
  t156 = t155;
  if (!t156) {
  t157 = ((int64_t)(v14));
  t158 = ((int64_t)(v6));
  t159 = ((int64_t)(((int64_t)UINT64_C(5))));
  t160 = ((int64_t)(((int64_t)UINT64_C(0))));
  t161 = fg166_ti(t158, t159, t160);
  t162 = ((int64_t)(t161));
  t156 = ((t157) == (t162));
  }
  t163 = t156;
  t164 = ((int64_t)(((int64_t)UINT64_C(23))));
  gg_function_4(t163, t164, base + 0);
  t165 = ((int64_t)(v14));
  t166 = ((int64_t)(((int64_t)UINT64_C(0))));
  t167 = ((t165) == (t166));
  t168 = t167;
  if (!t168) {
  t169 = ((int64_t)(v6));
  t170 = ((int64_t)(((int64_t)UINT64_C(1))));
  t171 = ((int64_t)(((int64_t)UINT64_C(0))));
  t172 = fg166_ti(t169, t170, t171);
  t173 = ((int64_t)(t172));
  t174 = ((int64_t)(((int64_t)UINT64_C(0))));
  t175 = ((t173) != (t174));
  t176 = t175;
  if (t176) {
  t177 = ((int64_t)(v5));
  t178 = ((int64_t)(((int64_t)UINT64_C(1))));
  t179 = ((int64_t)(((int64_t)UINT64_C(0))));
  t180 = fg166_ti(t177, t178, t179);
  t181 = ((int64_t)(t180));
  t182 = ((int64_t)(((int64_t)UINT64_C(0))));
  t176 = ((t181) != (t182));
  }
  t168 = t176;
  }
  t183 = t168;
  t184 = ((int64_t)(((int64_t)UINT64_C(24))));
  gg_function_4(t183, t184, base + 0);
  t185 = ((int64_t)(v0));
  t186 = ((int64_t)(((int64_t)UINT64_C(0))));
  t187 = ((int64_t)(v6));
  t188 = ((int64_t)(((int64_t)UINT64_C(1))));
  t189 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = fg166_ti(t187, t188, t189);
  t191 = ((int64_t)(t190));
  fg166_hi(t185, t186, t191);
  t192 = ((int64_t)(v0));
  t193 = ((int64_t)(((int64_t)UINT64_C(2))));
  t194 = ((int64_t)(v5));
  t195 = ((int64_t)(((int64_t)UINT64_C(1))));
  t196 = ((int64_t)(((int64_t)UINT64_C(0))));
  t197 = fg166_ti(t194, t195, t196);
  t198 = ((int64_t)(t197));
  fg166_hi(t192, t193, t198);
  t199 = ((int64_t)(v0));
  t200 = ((int64_t)(((int64_t)UINT64_C(3))));
  t201 = ((int64_t)(v14));
  fg166_hi(t199, t200, t201);
  t202 = ((int64_t)(v0));
  t203 = ((int64_t)(((int64_t)UINT64_C(17))));
  t204 = ((int64_t)(v13));
  fg166_hi(t202, t203, t204);
  t205 = ((int64_t)(v0));
  t206 = ((int64_t)(((int64_t)UINT64_C(22))));
  t207 = ((int64_t)(v12));
  fg166_hi(t205, t206, t207);
  t208 = ((int64_t)(v0));
  t209 = ((int64_t)(v6));
  t210 = ((int64_t)(v5));
  gg_function_10(t208, t209, t210, base + 0);
  t211 = ((int64_t)(((int64_t)UINT64_C(3))));
  t212 = fg166_constant(t211);
  v15 = ((int64_t)(t212));
  t213 = ((int64_t)(v4));
  t214 = ((int64_t)(((int64_t)UINT64_C(0))));
  t215 = ((t213) == (t214));
  if (t215) {
  {
  t216 = ((int64_t)(v14));
  t217 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t218 = ((t216) <= (t217));
  t219 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_4(t218, t219, base + 0);
  t220 = ((int64_t)(((int64_t)UINT64_C(0))));
  t221 = ((int64_t)(v6));
  t222 = ((int64_t)(((int64_t)UINT64_C(0))));
  t223 = fg166_pred(t220, t221, t222);
  t224 = ((int64_t)(t223));
  t225 = ((int64_t)(((int64_t)UINT64_C(0))));
  t226 = ((t224) == (t225));
  if (t226) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  }
  t227 = ((int64_t)(v4));
  t228 = ((int64_t)(((int64_t)UINT64_C(1))));
  t229 = ((t227) == (t228));
  t230 = t229;
  if (!t230) {
  t231 = ((int64_t)(v4));
  t232 = ((int64_t)(((int64_t)UINT64_C(7))));
  t230 = ((t231) == (t232));
  }
  t233 = t230;
  if (t233) {
  {
  t234 = ((int64_t)(v4));
  t235 = ((int64_t)(((int64_t)UINT64_C(1))));
  t236 = ((t234) == (t235));
  if (t236) {
  {
  t237 = ((int64_t)(v2));
  t238 = ((int64_t)(((int64_t)UINT64_C(0))));
  t239 = ((int64_t)(((int64_t)UINT64_C(1))));
  t240 = fg166_ti(t237, t238, t239);
  v7 = ((int64_t)(t240));
  }
  }
  t241 = ((int64_t)(((int64_t)UINT64_C(1))));
  t242 = ((int64_t)(v6));
  t243 = ((int64_t)(((int64_t)UINT64_C(0))));
  t244 = fg166_pred(t241, t242, t243);
  t245 = ((int64_t)(t244));
  t246 = ((int64_t)(((int64_t)UINT64_C(0))));
  t247 = ((t245) != (t246));
  t248 = t247;
  if (t248) {
  t249 = ((int64_t)(v6));
  t250 = ((int64_t)(((int64_t)UINT64_C(4))));
  t251 = ((int64_t)(((int64_t)UINT64_C(0))));
  t252 = fg166_ti(t249, t250, t251);
  t253 = ((int64_t)(t252));
  t254 = ((int64_t)(v13));
  t248 = ((t253) == (t254));
  }
  t255 = t248;
  t256 = ((int64_t)(((int64_t)UINT64_C(7))));
  gg_function_4(t255, t256, base + 0);
  t257 = ((int64_t)(v4));
  t258 = ((int64_t)(((int64_t)UINT64_C(1))));
  t259 = ((t257) == (t258));
  if (t259) {
  {
  t260 = ((int64_t)(((int64_t)UINT64_C(0))));
  t261 = ((int64_t)(v5));
  t262 = ((int64_t)(((int64_t)UINT64_C(0))));
  t263 = fg166_pred(t260, t261, t262);
  t264 = ((int64_t)(t263));
  t265 = ((int64_t)(((int64_t)UINT64_C(0))));
  t266 = ((t264) != (t265));
  t267 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_function_4(t266, t267, base + 0);
  }
  }
  t268 = ((int64_t)(v4));
  t269 = ((int64_t)(((int64_t)UINT64_C(7))));
  t270 = ((t268) == (t269));
  if (t270) {
  {
  t271 = ((int64_t)(((int64_t)UINT64_C(1))));
  t272 = ((int64_t)(v7));
  t273 = ((int64_t)(((int64_t)UINT64_C(0))));
  t274 = fg166_pred(t271, t272, t273);
  t275 = ((int64_t)(t274));
  t276 = ((int64_t)(((int64_t)UINT64_C(0))));
  t277 = ((t275) != (t276));
  t278 = t277;
  if (t278) {
  t279 = ((int64_t)(v7));
  t280 = ((int64_t)(((int64_t)UINT64_C(4))));
  t281 = ((int64_t)(((int64_t)UINT64_C(0))));
  t282 = fg166_ti(t279, t280, t281);
  t283 = ((int64_t)(t282));
  t284 = ((int64_t)(v13));
  t278 = ((t283) == (t284));
  }
  t285 = t278;
  t286 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_function_4(t285, t286, base + 0);
  }
  }
  t287 = ((int64_t)(v4));
  t288 = ((int64_t)(((int64_t)UINT64_C(7))));
  t289 = ((t287) == (t288));
  t290 = t289;
  if (!t290) {
  t291 = ((int64_t)(v7));
  t292 = ((int64_t)(((int64_t)UINT64_C(0))));
  t290 = ((t291) != (t292));
  }
  t293 = t290;
  if (t293) {
  {
  t294 = ((int64_t)(v6));
  t295 = ((int64_t)(((int64_t)UINT64_C(3))));
  t296 = ((int64_t)(((int64_t)UINT64_C(0))));
  t297 = fg166_ti(t294, t295, t296);
  v16 = ((int64_t)(t297));
  }
  } else {
  {
  t298 = ((int64_t)(v6));
  t299 = ((int64_t)(((int64_t)UINT64_C(3))));
  t300 = ((int64_t)(((int64_t)UINT64_C(0))));
  t301 = fg166_ti(t298, t299, t300);
  t302 = ((int64_t)(t301));
  t303 = ((int64_t)(((int64_t)UINT64_C(2))));
  v16 = ((int64_t)(((int64_t)gg_div(t302, t303))));
  }
  }
  t304 = ((int64_t)(v5));
  t305 = ((int64_t)(((int64_t)UINT64_C(3))));
  t306 = ((int64_t)(((int64_t)UINT64_C(0))));
  t307 = fg166_ti(t304, t305, t306);
  t308 = ((int64_t)(t307));
  t309 = ((int64_t)(v16));
  t310 = ((t308) == (t309));
  t311 = t310;
  if (t311) {
  t312 = ((int64_t)(v5));
  t313 = ((int64_t)(((int64_t)UINT64_C(6))));
  t314 = ((int64_t)(((int64_t)UINT64_C(0))));
  t315 = fg166_ti(t312, t313, t314);
  t316 = ((int64_t)(t315));
  t317 = ((int64_t)(v6));
  t318 = ((int64_t)(((int64_t)UINT64_C(6))));
  t319 = ((int64_t)(((int64_t)UINT64_C(0))));
  t320 = fg166_ti(t317, t318, t319);
  t321 = ((int64_t)(t320));
  t311 = ((t316) == (t321));
  }
  t322 = t311;
  t323 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_4(t322, t323, base + 0);
  t324 = ((int64_t)(v7));
  t325 = ((int64_t)(((int64_t)UINT64_C(0))));
  t326 = ((t324) != (t325));
  if (t326) {
  {
  t327 = ((int64_t)(v7));
  t328 = ((int64_t)(((int64_t)UINT64_C(1))));
  t329 = ((int64_t)(((int64_t)UINT64_C(0))));
  t330 = fg166_ti(t327, t328, t329);
  v17 = ((int64_t)(t330));
  }
  } else {
  {
  t331 = ((int64_t)(v6));
  t332 = ((int64_t)(((int64_t)UINT64_C(1))));
  t333 = ((int64_t)(((int64_t)UINT64_C(0))));
  t334 = fg166_ti(t331, t332, t333);
  v17 = ((int64_t)(t334));
  }
  }
  t335 = ((int64_t)(v6));
  t336 = ((int64_t)(((int64_t)UINT64_C(1))));
  t337 = ((int64_t)(((int64_t)UINT64_C(0))));
  t338 = fg166_ti(t335, t336, t337);
  v18 = ((int64_t)(t338));
  t339 = ((int64_t)(v6));
  t340 = ((int64_t)(((int64_t)UINT64_C(4))));
  t341 = ((int64_t)(((int64_t)UINT64_C(1))));
  t342 = fg166_ti(t339, t340, t341);
  v19 = ((int64_t)(t342));
  t343 = ((int64_t)(v7));
  t344 = ((int64_t)(((int64_t)UINT64_C(0))));
  t345 = ((t343) != (t344));
  if (t345) {
  {
  t346 = ((int64_t)(v7));
  t347 = ((int64_t)(((int64_t)UINT64_C(4))));
  t348 = ((int64_t)(((int64_t)UINT64_C(1))));
  t349 = fg166_ti(t346, t347, t348);
  v20 = ((int64_t)(t349));
  }
  } else {
  {
  v20 = ((int64_t)(v19));
  }
  }
  t350 = ((int64_t)(v7));
  t351 = ((int64_t)(((int64_t)UINT64_C(0))));
  t352 = ((t350) != (t351));
  if (t352) {
  {
  t353 = ((int64_t)(((int64_t)UINT64_C(1))));
  t354 = ((int64_t)(v7));
  t355 = ((int64_t)(((int64_t)UINT64_C(0))));
  t356 = fg166_pred(t353, t354, t355);
  t357 = ((int64_t)(t356));
  t358 = ((int64_t)(((int64_t)UINT64_C(0))));
  t359 = ((t357) != (t358));
  t360 = t359;
  if (t360) {
  t361 = ((int64_t)(v7));
  t362 = ((int64_t)(((int64_t)UINT64_C(4))));
  t363 = ((int64_t)(((int64_t)UINT64_C(0))));
  t364 = fg166_ti(t361, t362, t363);
  t365 = ((int64_t)(t364));
  t366 = ((int64_t)(v13));
  t360 = ((t365) == (t366));
  }
  t367 = t360;
  t368 = t367;
  if (t368) {
  t369 = ((int64_t)(v7));
  t370 = ((int64_t)(((int64_t)UINT64_C(3))));
  t371 = ((int64_t)(((int64_t)UINT64_C(0))));
  t372 = fg166_ti(t369, t370, t371);
  t373 = ((int64_t)(t372));
  t374 = ((int64_t)(v16));
  t368 = ((t373) == (t374));
  }
  t375 = t368;
  t376 = t375;
  if (t376) {
  t377 = ((int64_t)(v7));
  t378 = ((int64_t)(((int64_t)UINT64_C(2))));
  t379 = ((int64_t)(((int64_t)UINT64_C(0))));
  t380 = fg166_ti(t377, t378, t379);
  t381 = ((int64_t)(t380));
  t382 = ((int64_t)(v11));
  t376 = ((t381) == (t382));
  }
  t383 = t376;
  t384 = t383;
  if (t384) {
  t385 = ((int64_t)(v7));
  t386 = ((int64_t)(((int64_t)UINT64_C(6))));
  t387 = ((int64_t)(((int64_t)UINT64_C(0))));
  t388 = fg166_ti(t385, t386, t387);
  t389 = ((int64_t)(t388));
  t390 = ((int64_t)(v5));
  t391 = ((int64_t)(((int64_t)UINT64_C(6))));
  t392 = ((int64_t)(((int64_t)UINT64_C(0))));
  t393 = fg166_ti(t390, t391, t392);
  t394 = ((int64_t)(t393));
  t384 = ((t389) == (t394));
  }
  t395 = t384;
  t396 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_4(t395, t396, base + 0);
  }
  } else {
  {
  t397 = ((int64_t)(v5));
  t398 = ((int64_t)(((int64_t)UINT64_C(8))));
  t399 = ((int64_t)(((int64_t)UINT64_C(1))));
  t400 = fg166_ti(t397, t398, t399);
  t401 = ((int64_t)(t400));
  t402 = ((int64_t)(((int64_t)UINT64_C(0))));
  v21 = ((t401) != (t402));
  t403 = v21;
  if (t403) {
  {
  t404 = v18;
  t405 = ((int64_t)(v16));
  t406 = ((int64_t)(v13));
  t407 = ((int64_t)(((int64_t)((uint64_t)(t405) * (uint64_t)(t406)))));
  v18 = ((int64_t)((uint64_t)(t404) + (uint64_t)(t407)));
  }
  } else {
  {
  t408 = v17;
  t409 = ((int64_t)(v16));
  t410 = ((int64_t)(v13));
  t411 = ((int64_t)(((int64_t)((uint64_t)(t409) * (uint64_t)(t410)))));
  v17 = ((int64_t)((uint64_t)(t408) + (uint64_t)(t411)));
  }
  }
  }
  }
  t412 = ((int64_t)(v0));
  t413 = ((int64_t)(((int64_t)UINT64_C(0))));
  t414 = ((int64_t)(v18));
  fg166_hi(t412, t413, t414);
  t415 = ((int64_t)(v0));
  t416 = ((int64_t)(((int64_t)UINT64_C(1))));
  t417 = ((int64_t)(v17));
  fg166_hi(t415, t416, t417);
  t418 = ((int64_t)(v0));
  t419 = ((int64_t)(((int64_t)UINT64_C(4))));
  t420 = ((int64_t)(v16));
  fg166_hi(t418, t419, t420);
  t421 = ((int64_t)(v0));
  t422 = ((int64_t)(((int64_t)UINT64_C(5))));
  t423 = ((int64_t)(v19));
  t424 = ((int64_t)(v13));
  t425 = ((int64_t)(((int64_t)gg_div(t423, t424))));
  fg166_hi(t421, t422, t425);
  t426 = ((int64_t)(v0));
  t427 = ((int64_t)(((int64_t)UINT64_C(6))));
  t428 = ((int64_t)(v20));
  t429 = ((int64_t)(v13));
  t430 = ((int64_t)(((int64_t)gg_div(t428, t429))));
  fg166_hi(t426, t427, t430);
  t431 = ((int64_t)(((int64_t)UINT64_C(4))));
  t432 = fg166_constant(t431);
  v15 = ((int64_t)(t432));
  t433 = ((int64_t)(v8));
  t434 = ((int64_t)(((int64_t)UINT64_C(3))));
  t435 = ((t433) == (t434));
  if (t435) {
  {
  t436 = ((int64_t)(v12));
  t437 = ((int64_t)(((int64_t)UINT64_C(0))));
  t438 = ((t436) == (t437));
  t439 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_function_4(t438, t439, base + 0);
  t440 = ((int64_t)(v0));
  t441 = ((int64_t)(((int64_t)UINT64_C(4))));
  t442 = ((int64_t)(v5));
  t443 = ((int64_t)(((int64_t)UINT64_C(2))));
  t444 = fg166_tf(t442, t443);
  t445 = ((float)(t444));
  fg166_hf(t440, t441, t445);
  t446 = ((int64_t)(v0));
  t447 = ((int64_t)(((int64_t)UINT64_C(5))));
  t448 = ((int64_t)(v5));
  t449 = ((int64_t)(((int64_t)UINT64_C(3))));
  t450 = fg166_tf(t448, t449);
  t451 = ((float)(t450));
  fg166_hf(t446, t447, t451);
  }
  }
  t452 = ((int64_t)(v8));
  t453 = ((int64_t)(((int64_t)UINT64_C(4))));
  t454 = ((t452) == (t453));
  if (t454) {
  {
  t455 = ((int64_t)(v12));
  t456 = ((int64_t)(((int64_t)UINT64_C(2))));
  t457 = ((t455) != (t456));
  t458 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_4(t457, t458, base + 0);
  t459 = ((int64_t)(v0));
  t460 = ((int64_t)(((int64_t)UINT64_C(5))));
  t461 = ((int64_t)(v5));
  t462 = ((int64_t)(((int64_t)UINT64_C(3))));
  t463 = fg166_tf(t461, t462);
  t464 = ((float)(t463));
  fg166_hf(t459, t460, t464);
  }
  }
  }
  }
  t465 = ((int64_t)(v4));
  t466 = ((int64_t)(((int64_t)UINT64_C(2))));
  t467 = ((t465) == (t466));
  if (t467) {
  {
  t468 = ((int64_t)(((int64_t)UINT64_C(0))));
  t469 = ((int64_t)(v6));
  t470 = ((int64_t)(((int64_t)UINT64_C(0))));
  t471 = fg166_pred(t468, t469, t470);
  t472 = ((int64_t)(t471));
  t473 = ((int64_t)(((int64_t)UINT64_C(0))));
  t474 = ((t472) != (t473));
  t475 = t474;
  if (t475) {
  t476 = ((int64_t)(v14));
  t477 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t475 = ((t476) <= (t477));
  }
  t478 = t475;
  t479 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_function_4(t478, t479, base + 0);
  v22 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t480 = ((int64_t)(v22));
  t481 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t480) < (t481)))) goto label2;
  {
  t482 = ((int64_t)(v0));
  t483 = ((int64_t)(v22));
  t484 = ((int64_t)(v5));
  t485 = ((int64_t)(v22));
  t486 = ((int64_t)(((int64_t)UINT64_C(1))));
  t487 = ((int64_t)(((int64_t)((uint64_t)(t485) + (uint64_t)(t486)))));
  t488 = fg166_tf(t484, t487);
  t489 = ((float)(t488));
  fg166_hf(t482, t483, t489);
  t490 = v22;
  t491 = ((int64_t)(((int64_t)UINT64_C(1))));
  v22 = ((int64_t)((uint64_t)(t490) + (uint64_t)(t491)));
  }
  goto label1;
label2:;
  t492 = ((int64_t)(((int64_t)UINT64_C(5))));
  t493 = fg166_constant(t492);
  v15 = ((int64_t)(t493));
  }
  }
  t494 = ((int64_t)(v4));
  t495 = ((int64_t)(((int64_t)UINT64_C(3))));
  t496 = ((t494) == (t495));
  if (t496) {
  {
  t497 = ((int64_t)(((int64_t)UINT64_C(0))));
  t498 = ((int64_t)(v6));
  t499 = ((int64_t)(((int64_t)UINT64_C(0))));
  t500 = fg166_pred(t497, t498, t499);
  t501 = ((int64_t)(t500));
  t502 = ((int64_t)(((int64_t)UINT64_C(0))));
  t503 = ((t501) != (t502));
  t504 = t503;
  if (t504) {
  t505 = ((int64_t)(v12));
  t506 = ((int64_t)(((int64_t)UINT64_C(2))));
  t504 = ((t505) != (t506));
  }
  t507 = t504;
  t508 = t507;
  if (t508) {
  t509 = ((int64_t)(v14));
  t510 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t508 = ((t509) <= (t510));
  }
  t511 = t508;
  t512 = ((int64_t)(((int64_t)UINT64_C(15))));
  gg_function_4(t511, t512, base + 0);
  t513 = ((int64_t)(v2));
  t514 = ((int64_t)(((int64_t)UINT64_C(0))));
  t515 = ((int64_t)(((int64_t)UINT64_C(1))));
  t516 = fg166_ti(t513, t514, t515);
  v23 = ((int64_t)(t516));
  t517 = ((int64_t)(v23));
  t518 = ((int64_t)(((int64_t)UINT64_C(0))));
  t519 = ((t517) != (t518));
  t520 = t519;
  if (t520) {
  t521 = ((int64_t)(v23));
  t522 = ((int64_t)(((int64_t)UINT64_C(2))));
  t523 = ((int64_t)(((int64_t)UINT64_C(0))));
  t524 = fg166_ti(t521, t522, t523);
  t525 = ((int64_t)(t524));
  t526 = ((int64_t)(v11));
  t520 = ((t525) == (t526));
  }
  t527 = t520;
  t528 = t527;
  if (t528) {
  t529 = ((int64_t)(v23));
  t530 = ((int64_t)(((int64_t)UINT64_C(5))));
  t531 = ((int64_t)(((int64_t)UINT64_C(0))));
  t532 = fg166_ti(t529, t530, t531);
  t533 = ((int64_t)(t532));
  t534 = ((int64_t)(v14));
  t528 = ((t533) == (t534));
  }
  t535 = t528;
  t536 = t535;
  if (t536) {
  t537 = ((int64_t)(((int64_t)UINT64_C(0))));
  t538 = ((int64_t)(v23));
  t539 = ((int64_t)(((int64_t)UINT64_C(0))));
  t540 = fg166_pred(t537, t538, t539);
  t541 = ((int64_t)(t540));
  t542 = ((int64_t)(((int64_t)UINT64_C(0))));
  t536 = ((t541) != (t542));
  }
  t543 = t536;
  t544 = ((int64_t)(((int64_t)UINT64_C(25))));
  gg_function_4(t543, t544, base + 0);
  t545 = ((int64_t)(v0));
  t546 = ((int64_t)(((int64_t)UINT64_C(0))));
  t547 = ((int64_t)(v23));
  t548 = ((int64_t)(((int64_t)UINT64_C(1))));
  t549 = ((int64_t)(((int64_t)UINT64_C(0))));
  t550 = fg166_ti(t547, t548, t549);
  t551 = ((int64_t)(t550));
  fg166_hi(t545, t546, t551);
  t552 = ((int64_t)(v0));
  t553 = ((int64_t)(((int64_t)UINT64_C(1))));
  t554 = ((int64_t)(v6));
  t555 = ((int64_t)(((int64_t)UINT64_C(1))));
  t556 = ((int64_t)(((int64_t)UINT64_C(0))));
  t557 = fg166_ti(t554, t555, t556);
  t558 = ((int64_t)(t557));
  fg166_hi(t552, t553, t558);
  t559 = ((int64_t)(((int64_t)UINT64_C(6))));
  t560 = fg166_constant(t559);
  v15 = ((int64_t)(t560));
  }
  }
  t561 = ((int64_t)(v4));
  t562 = ((int64_t)(((int64_t)UINT64_C(4))));
  t563 = ((t561) == (t562));
  if (t563) {
  {
  t564 = ((int64_t)(((int64_t)UINT64_C(0))));
  t565 = ((int64_t)(v6));
  t566 = ((int64_t)(((int64_t)UINT64_C(0))));
  t567 = fg166_pred(t564, t565, t566);
  t568 = ((int64_t)(t567));
  t569 = ((int64_t)(((int64_t)UINT64_C(0))));
  t570 = ((t568) != (t569));
  t571 = t570;
  if (t571) {
  t572 = ((int64_t)(v12));
  t573 = ((int64_t)(((int64_t)UINT64_C(2))));
  t571 = ((t572) != (t573));
  }
  t574 = t571;
  t575 = t574;
  if (t575) {
  t576 = ((int64_t)(v14));
  t577 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t575 = ((t576) <= (t577));
  }
  t578 = t575;
  t579 = ((int64_t)(((int64_t)UINT64_C(16))));
  gg_function_4(t578, t579, base + 0);
  t580 = ((int64_t)(v0));
  t581 = ((int64_t)(((int64_t)UINT64_C(6))));
  t582 = ((int64_t)(v5));
  t583 = ((int64_t)(((int64_t)UINT64_C(0))));
  t584 = fg166_tf(t582, t583);
  t585 = ((float)(t584));
  fg166_hf(t580, t581, t585);
  t586 = ((int64_t)(((int64_t)UINT64_C(7))));
  t587 = fg166_constant(t586);
  v15 = ((int64_t)(t587));
  }
  }
  t588 = ((int64_t)(v4));
  t589 = ((int64_t)(((int64_t)UINT64_C(5))));
  t590 = ((t588) == (t589));
  if (t590) {
  {
  t591 = ((int64_t)(v12));
  t592 = ((int64_t)(((int64_t)UINT64_C(1))));
  t593 = ((t591) != (t592));
  t594 = ((int64_t)(((int64_t)UINT64_C(17))));
  gg_function_4(t593, t594, base + 0);
  t595 = ((int64_t)(v0));
  t596 = ((int64_t)(((int64_t)UINT64_C(0))));
  t597 = ((int64_t)(v5));
  t598 = ((int64_t)(((int64_t)UINT64_C(0))));
  t599 = fg166_tf(t597, t598);
  t600 = ((float)(t599));
  fg166_hf(t595, t596, t600);
  t601 = ((int64_t)(v0));
  t602 = ((int64_t)(((int64_t)UINT64_C(1))));
  t603 = ((int64_t)(v5));
  t604 = ((int64_t)(((int64_t)UINT64_C(1))));
  t605 = fg166_tf(t603, t604);
  t606 = ((float)(t605));
  fg166_hf(t601, t602, t606);
  t607 = ((int64_t)(((int64_t)UINT64_C(11))));
  t608 = fg166_constant(t607);
  v15 = ((int64_t)(t608));
  }
  }
  t609 = ((int64_t)(v4));
  t610 = ((int64_t)(((int64_t)UINT64_C(6))));
  t611 = ((t609) == (t610));
  if (t611) {
  {
  t612 = ((int64_t)(v12));
  t613 = ((int64_t)(((int64_t)UINT64_C(2))));
  t614 = ((t612) != (t613));
  t615 = t614;
  if (t615) {
  t616 = ((int64_t)(v14));
  t617 = ((int64_t)(((int64_t)UINT64_C(4294967295))));
  t615 = ((t616) <= (t617));
  }
  t618 = t615;
  t619 = t618;
  if (t619) {
  t620 = ((int64_t)(((int64_t)UINT64_C(2))));
  t621 = ((int64_t)(v6));
  t622 = ((int64_t)(((int64_t)UINT64_C(0))));
  t623 = fg166_pred(t620, t621, t622);
  t624 = ((int64_t)(t623));
  t625 = ((int64_t)(((int64_t)UINT64_C(0))));
  t619 = ((t624) != (t625));
  }
  t626 = t619;
  t627 = t626;
  if (t627) {
  t628 = ((int64_t)(((int64_t)UINT64_C(2))));
  t629 = ((int64_t)(v5));
  t630 = ((int64_t)(((int64_t)UINT64_C(0))));
  t631 = fg166_pred(t628, t629, t630);
  t632 = ((int64_t)(t631));
  t633 = ((int64_t)(((int64_t)UINT64_C(0))));
  t627 = ((t632) != (t633));
  }
  t634 = t627;
  t635 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_4(t634, t635, base + 0);
  t636 = ((int64_t)(v5));
  t637 = ((int64_t)(((int64_t)UINT64_C(0))));
  t638 = fg166_tf(t636, t637);
  v24 = ((float)(t638));
  t639 = ((int64_t)(v5));
  t640 = ((int64_t)(((int64_t)UINT64_C(1))));
  t641 = fg166_tf(t639, t640);
  v25 = ((float)(t641));
  t642 = ((int64_t)(v0));
  t643 = ((int64_t)(((int64_t)UINT64_C(4))));
  t645 = ((int64_t)(v12));
  t646 = ((int64_t)(((int64_t)UINT64_C(1))));
  t647 = ((t645) == (t646));
  if (t647) {
  {
  t648 = ((float)(v24));
  t649 = fg166_half_round(t648);
  t644 = ((float)(t649));
  }
  } else {
  {
  t644 = ((float)(v24));
  }
  }
  t650 = ((float)(t644));
  fg166_hf(t642, t643, t650);
  t651 = ((int64_t)(v0));
  t652 = ((int64_t)(((int64_t)UINT64_C(5))));
  t654 = ((int64_t)(v12));
  t655 = ((int64_t)(((int64_t)UINT64_C(1))));
  t656 = ((t654) == (t655));
  if (t656) {
  {
  t657 = ((float)(v25));
  t658 = fg166_half_round(t657);
  t653 = ((float)(t658));
  }
  } else {
  {
  t653 = ((float)(v25));
  }
  }
  t659 = ((float)(t653));
  fg166_hf(t651, t652, t659);
  t660 = ((int64_t)(((int64_t)UINT64_C(12))));
  t661 = fg166_constant(t660);
  v15 = ((int64_t)(t661));
  }
  }
  t662 = ((int64_t)(v4));
  t663 = ((int64_t)(((int64_t)UINT64_C(8))));
  t664 = ((t662) == (t663));
  if (t664) {
  {
  t665 = ((int64_t)(((int64_t)UINT64_C(0))));
  t666 = ((int64_t)(v6));
  t667 = ((int64_t)(((int64_t)UINT64_C(0))));
  t668 = fg166_pred(t665, t666, t667);
  t669 = ((int64_t)(t668));
  t670 = ((int64_t)(((int64_t)UINT64_C(0))));
  t671 = ((t669) != (t670));
  t672 = t671;
  if (t672) {
  t673 = ((int64_t)(v12));
  t674 = ((int64_t)(((int64_t)UINT64_C(2))));
  t672 = ((t673) != (t674));
  }
  t675 = t672;
  t676 = t675;
  if (t676) {
  t677 = ((int64_t)(v14));
  t678 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t676 = ((t677) <= (t678));
  }
  t679 = t676;
  t680 = ((int64_t)(((int64_t)UINT64_C(19))));
  gg_function_4(t679, t680, base + 0);
  }
  }
  t681 = ((int64_t)(v15));
  t682 = ((int64_t)(((int64_t)UINT64_C(0))));
  t683 = ((t681) > (t682));
  t684 = ((int64_t)(((int64_t)UINT64_C(20))));
  gg_function_4(t683, t684, base + 0);
  t685 = ((int64_t)(v14));
  t686 = ((int64_t)(v15));
  t687 = ((int64_t)(((int64_t)((uint64_t)(t685) + (uint64_t)(t686)))));
  t688 = ((int64_t)(((int64_t)UINT64_C(1))));
  t689 = ((int64_t)(((int64_t)((uint64_t)(t687) - (uint64_t)(t688)))));
  t690 = ((int64_t)(v15));
  v26 = ((int64_t)(((int64_t)gg_div(t689, t690))));
  t691 = ((int64_t)(v4));
  t692 = ((int64_t)(((int64_t)UINT64_C(2))));
  t693 = ((t691) == (t692));
  if (t693) {
  {
  t694 = ((int64_t)(v14));
  t695 = ((int64_t)(v15));
  t696 = ((int64_t)(((int64_t)((uint64_t)(t694) + (uint64_t)(t695)))));
  t697 = ((int64_t)(v15));
  v26 = ((int64_t)(((int64_t)gg_div(t696, t697))));
  }
  }
  t698 = ((int64_t)(v4));
  t699 = ((int64_t)(((int64_t)UINT64_C(5))));
  t700 = ((t698) == (t699));
  t701 = t700;
  if (t701) {
  t702 = ((int64_t)(v26));
  t703 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t701 = ((t702) > (t703));
  }
  t704 = t701;
  if (t704) {
  {
  v26 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  }
  }
  t705 = ((int64_t)(v0));
  t706 = ((int64_t)(((int64_t)UINT64_C(18))));
  t707 = ((int64_t)(v26));
  fg166_hi(t705, t706, t707);
  t708 = ((int64_t)(v0));
  t709 = ((int64_t)(((int64_t)UINT64_C(19))));
  t710 = ((int64_t)(v15));
  fg166_hi(t708, t709, t710);
  t711 = ((int64_t)(v0));
  t712 = ((int64_t)(((int64_t)UINT64_C(20))));
  t713 = ((int64_t)(v8));
  fg166_hi(t711, t712, t713);
  t714 = ((int64_t)(v0));
  t715 = ((int64_t)(((int64_t)UINT64_C(21))));
  t716 = ((int64_t)(v9));
  fg166_hi(t714, t715, t716);
  t717 = ((int64_t)(v0));
  fg166_launch(t717);
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
