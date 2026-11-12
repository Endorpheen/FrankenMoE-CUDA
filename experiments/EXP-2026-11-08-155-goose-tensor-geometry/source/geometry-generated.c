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
static uint8_t gg_eq_4(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint64_t value);
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
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t base);
static uint64_t gg_function_5(int64_t p0, int64_t base);
static int64_t gg_function_13(uint8_t p0, int64_t base);
static uint8_t gg_function_14(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint8_t gg_function_15(int64_t p0, int64_t base);
static uint8_t gg_function_16(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_17(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_18(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_13(uint8_t p0, int64_t base) {
  int64_t t0;
  uint8_t t1;
  uint8_t v0;
  v0 = p0;
  {
  t1 = v0;
  if (t1) {
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
gg_done:;
  return t0;
}
static uint8_t gg_function_15(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg155_ne(t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  t7 = t6;
  if (!t7) {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = fg155_ne(t8, t9);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t11) == (t12));
  }
  t13 = t7;
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = fg155_ne(t15, t16);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t18) == (t19));
  }
  t20 = t14;
  t21 = t20;
  if (!t21) {
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(3))));
  t24 = fg155_ne(t22, t23);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t25) == (t26));
  }
  t0 = t21;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_18(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
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
  uint8_t t40;
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_15(t1, base + 0);
  t3 = t2;
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t0 = gg_function_15(t4, base + 0);
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg155_ne(t5, t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg155_ne(t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)gg_mod(t8, t12))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) == (t14));
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = fg155_ne(t17, t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  t23 = fg155_ne(t21, t22);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(((int64_t)gg_mod(t20, t24))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t25) == (t26));
  }
  t27 = t16;
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = fg155_ne(t29, t30);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = fg155_ne(t33, t34);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(((int64_t)gg_mod(t32, t36))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t37) == (t38));
  }
  t39 = t28;
  t40 = t39;
  if (t40) {
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(((int64_t)UINT64_C(3))));
  t43 = fg155_ne(t41, t42);
  t44 = ((int64_t)(t43));
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(3))));
  t47 = fg155_ne(t45, t46);
  t48 = ((int64_t)(t47));
  t49 = ((int64_t)(((int64_t)gg_mod(t44, t48))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t49) == (t50));
  }
  t0 = t40;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_17(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint8_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  uint64_t t38;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg155_nb(t1, t2);
  t4 = ((uint64_t)(t3));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg155_nb(t5, t6);
  t8 = ((uint64_t)(t7));
  t9 = ((t4) == (t8));
  t10 = t9;
  if (t10) {
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = fg155_nb(t11, t12);
  t14 = ((uint64_t)(t13));
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = fg155_nb(t15, t16);
  t18 = ((uint64_t)(t17));
  t10 = ((t14) == (t18));
  }
  t19 = t10;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(2))));
  t23 = fg155_nb(t21, t22);
  t24 = ((uint64_t)(t23));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(2))));
  t27 = fg155_nb(t25, t26);
  t28 = ((uint64_t)(t27));
  t20 = ((t24) == (t28));
  }
  t29 = t20;
  t30 = t29;
  if (t30) {
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  t33 = fg155_nb(t31, t32);
  t34 = ((uint64_t)(t33));
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(((int64_t)UINT64_C(3))));
  t37 = fg155_nb(t35, t36);
  t38 = ((uint64_t)(t37));
  t30 = ((t34) == (t38));
  }
  t0 = t30;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_16(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
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
  int64_t t21;
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
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg155_ne(t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg155_ne(t5, t6);
  t8 = ((int64_t)(t7));
  t9 = ((t4) == (t8));
  t10 = t9;
  if (t10) {
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = fg155_ne(t11, t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = fg155_ne(t15, t16);
  t18 = ((int64_t)(t17));
  t10 = ((t14) == (t18));
  }
  t19 = t10;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(2))));
  t23 = fg155_ne(t21, t22);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(2))));
  t27 = fg155_ne(t25, t26);
  t28 = ((int64_t)(t27));
  t20 = ((t24) == (t28));
  }
  t29 = t20;
  t30 = t29;
  if (t30) {
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  t33 = fg155_ne(t31, t32);
  t34 = ((int64_t)(t33));
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(((int64_t)UINT64_C(3))));
  t37 = fg155_ne(t35, t36);
  t38 = ((int64_t)(t37));
  t30 = ((t34) == (t38));
  }
  t0 = t30;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_2(int64_t p0, int64_t base) {
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
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg155_ne(t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = fg155_ne(t5, t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t8)))));
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  t12 = fg155_ne(t10, t11);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t13)))));
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(3))));
  t17 = fg155_ne(t15, t16);
  t18 = ((int64_t)(t17));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t18)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_5(int64_t p0, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  uint64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = fg155_ne(t3, t4);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) <= (t7));
  if (t8) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t9 = v1;
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t9) + (uint64_t)(t10)));
  }
  goto label1;
label2:;
  t11 = ((int64_t)(v0));
  t12 = fg155_tensor_block(t11);
  v2 = ((int64_t)(t12));
  v3 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v0));
  t17 = fg155_tensor_bytes(t16);
  v3 = ((uint64_t)(t17));
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg155_ne(t18, t19);
  t21 = ((uint64_t)(((uint64_t)(t20))));
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg155_nb(t22, t23);
  t25 = ((uint64_t)(t24));
  t26 = ((uint64_t)(((uint64_t)((uint64_t)(t21) * (uint64_t)(t25)))));
  t27 = ((uint64_t)(((uint64_t)(v2))));
  v3 = ((uint64_t)(((uint64_t)gg_udiv(t26, t27))));
  v1 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
label4:;
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t28) < (t29)))) goto label5;
  {
  t30 = v3;
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(v1));
  t33 = fg155_ne(t31, t32);
  t34 = ((int64_t)(t33));
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) - (uint64_t)(t35)))))));
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(v1));
  t39 = fg155_nb(t37, t38);
  t40 = ((uint64_t)(t39));
  t41 = ((uint64_t)(((uint64_t)((uint64_t)(t36) * (uint64_t)(t40)))));
  v3 = ((uint64_t)((uint64_t)(t30) + (uint64_t)(t41)));
  t42 = v1;
  t43 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t42) + (uint64_t)(t43)));
  }
  goto label4;
label5:;
  t0 = ((uint64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_14(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  uint8_t t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint8_t t17;
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
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
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint8_t t42;
  uint64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = fg155_tensor_bytes(t1);
  v3 = ((uint64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = fg155_tensor_block(t3);
  v4 = ((int64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg155_ne(t5, t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(v4));
  t10 = ((t8) != (t9));
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg155_nb(t12, t13);
  t15 = ((uint64_t)(t14));
  t16 = ((uint64_t)(v3));
  t11 = ((t15) != (t16));
  }
  t17 = t11;
  if (t17) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t18 = v3;
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg155_ne(t19, t20);
  t22 = ((int64_t)(t21));
  t23 = ((int64_t)(v4));
  t24 = ((uint64_t)(((uint64_t)(((int64_t)gg_div(t22, t23))))));
  v3 = ((uint64_t)((uint64_t)(t18) * (uint64_t)(t24)));
  v5 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(v2));
  if (!(((t25) < (t26)))) goto label2;
  {
  t27 = ((int64_t)(v5));
  t28 = ((int64_t)(v1));
  t29 = ((t27) > (t28));
  if (t29) {
  {
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(v5));
  t32 = fg155_ne(t30, t31);
  t33 = ((int64_t)(t32));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = ((t33) != (t34));
  t36 = t35;
  if (t36) {
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(v5));
  t39 = fg155_nb(t37, t38);
  t40 = ((uint64_t)(t39));
  t41 = ((uint64_t)(v3));
  t36 = ((t40) != (t41));
  }
  t42 = t36;
  if (t42) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t43 = v3;
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(v5));
  t46 = fg155_ne(t44, t45);
  t47 = ((uint64_t)(((uint64_t)(t46))));
  v3 = ((uint64_t)((uint64_t)(t43) * (uint64_t)(t47)));
  }
  } else {
  {
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(v5));
  t50 = fg155_ne(t48, t49);
  t51 = ((uint64_t)(((uint64_t)(t50))));
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(v5));
  t54 = fg155_nb(t52, t53);
  t55 = ((uint64_t)(t54));
  v3 = ((uint64_t)(((uint64_t)((uint64_t)(t51) * (uint64_t)(t55)))));
  }
  }
  t56 = v5;
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t56) + (uint64_t)(t57)));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
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
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = fg155_ne(t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = fg155_ne(t5, t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t8)))));
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(3))));
  t12 = fg155_ne(t10, t11);
  t13 = ((int64_t)(t12));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t13)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  uint64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  uint64_t t17;
  int64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  int64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
  uint64_t t46;
  uint64_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  uint8_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
  uint8_t t71;
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
  int64_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  uint8_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t t100;
  int64_t t101;
  int64_t t102;
  uint8_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  uint8_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint8_t t116;
  int64_t t117;
  int64_t t118;
  uint8_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  uint8_t t125;
  int64_t t126;
  int64_t t127;
  uint8_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  uint8_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  uint8_t t147;
  int64_t t148;
  int64_t t149;
  uint8_t t150;
  int64_t t151;
  uint64_t t152;
  uint64_t t153;
  int64_t t154;
  int64_t t155;
  uint64_t t156;
  int64_t t157;
  uint64_t t158;
  uint64_t t159;
  uint64_t t160;
  int64_t t161;
  int64_t t162;
  uint64_t t163;
  uint64_t t164;
  uint8_t t165;
  int64_t t166;
  int64_t t167;
  uint8_t t168;
  int64_t t169;
  int64_t t170;
  uint64_t t171;
  uint64_t t172;
  int64_t t173;
  int64_t t174;
  uint64_t t175;
  uint64_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  uint64_t t183;
  uint64_t t184;
  int64_t t185;
  int64_t t186;
  uint64_t t187;
  uint64_t t188;
  uint8_t t189;
  uint8_t t190;
  int64_t t191;
  int64_t t192;
  uint64_t t193;
  uint64_t t194;
  int64_t t195;
  int64_t t196;
  uint64_t t197;
  uint64_t t198;
  uint8_t t199;
  uint8_t t200;
  int64_t t201;
  int64_t t202;
  uint64_t t203;
  uint64_t t204;
  int64_t t205;
  int64_t t206;
  uint64_t t207;
  uint64_t t208;
  uint8_t t209;
  int64_t t210;
  int64_t t211;
  uint8_t t212;
  int64_t t213;
  int64_t t214;
  uint64_t t215;
  uint64_t t216;
  int64_t t217;
  int64_t t218;
  uint64_t t219;
  uint64_t t220;
  uint8_t t221;
  uint8_t t222;
  int64_t t223;
  int64_t t224;
  uint64_t t225;
  uint64_t t226;
  int64_t t227;
  int64_t t228;
  uint64_t t229;
  uint64_t t230;
  uint8_t t231;
  uint8_t t232;
  int64_t t233;
  int64_t t234;
  uint64_t t235;
  uint64_t t236;
  int64_t t237;
  uint64_t t238;
  uint64_t t239;
  uint8_t t240;
  int64_t t241;
  int64_t t242;
  uint8_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  uint8_t t251;
  uint8_t t252;
  int64_t t253;
  int64_t t254;
  uint64_t t255;
  uint64_t t256;
  int64_t t257;
  uint64_t t258;
  uint64_t t259;
  uint8_t t260;
  int64_t t261;
  int64_t t262;
  uint8_t t263;
  int64_t t264;
  int64_t t265;
  uint64_t t266;
  uint64_t t267;
  int64_t t268;
  uint64_t t269;
  uint64_t t270;
  uint8_t t271;
  uint8_t t272;
  int64_t t273;
  int64_t t274;
  uint64_t t275;
  uint64_t t276;
  int64_t t277;
  int64_t t278;
  uint64_t t279;
  uint64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  uint64_t t284;
  uint64_t t285;
  uint8_t t286;
  uint8_t t287;
  int64_t t288;
  int64_t t289;
  uint64_t t290;
  uint64_t t291;
  int64_t t292;
  int64_t t293;
  uint64_t t294;
  uint64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  uint64_t t299;
  uint64_t t300;
  uint8_t t301;
  int64_t t302;
  int64_t t303;
  uint8_t t304;
  int64_t t305;
  uint8_t t306;
  int64_t t307;
  int64_t t308;
  uint8_t t309;
  int64_t t310;
  int64_t t311;
  uint8_t t312;
  int64_t t313;
  int64_t t314;
  uint8_t t315;
  int64_t t316;
  int64_t t317;
  uint8_t t318;
  int64_t t319;
  int64_t t320;
  uint8_t t321;
  int64_t t322;
  int64_t t323;
  uint8_t t324;
  int64_t t325;
  int64_t t326;
  uint8_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  uint8_t t336;
  uint8_t t337;
  int64_t t338;
  int64_t t339;
  uint8_t t340;
  uint8_t t341;
  int64_t t342;
  int64_t t343;
  uint8_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  int64_t t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  uint8_t t353;
  uint8_t t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t t364;
  uint8_t t365;
  uint8_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  int64_t t370;
  int64_t t371;
  int64_t t372;
  int64_t t373;
  int64_t t374;
  int64_t t375;
  int64_t t376;
  uint8_t t377;
  int64_t t378;
  int64_t t379;
  uint8_t t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  int64_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  int64_t t388;
  uint8_t t389;
  uint8_t t390;
  int64_t t391;
  int64_t t392;
  int64_t t393;
  int64_t t394;
  int64_t t395;
  int64_t t396;
  int64_t t397;
  int64_t t398;
  int64_t t399;
  int64_t t400;
  uint8_t t401;
  uint8_t t402;
  int64_t t403;
  int64_t t404;
  int64_t t405;
  int64_t t406;
  int64_t t407;
  int64_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  uint8_t t413;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t0 = gg_function_2(t4, base + 0);
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)(v1));
  t0 = gg_function_4(t8, base + 0);
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v1));
  t13 = gg_function_5(t12, base + 0);
  t0 = ((int64_t)(((int64_t)(t13))));
  goto gg_done;
  }
  }
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = fg155_alignment();
  v4 = ((uint64_t)(t17));
  t18 = ((int64_t)(v1));
  t19 = gg_function_5(t18, base + 0);
  t20 = ((uint64_t)(t19));
  t21 = ((uint64_t)(v4));
  t22 = ((uint64_t)(((uint64_t)((uint64_t)(t20) + (uint64_t)(t21)))));
  t23 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t24 = ((uint64_t)(((uint64_t)((uint64_t)(t22) - (uint64_t)(t23)))));
  t25 = ((uint64_t)(v4));
  t26 = ((uint64_t)(((uint64_t)gg_udiv(t24, t25))));
  t27 = ((uint64_t)(v4));
  t0 = ((int64_t)(((int64_t)(((uint64_t)((uint64_t)(t26) * (uint64_t)(t27)))))));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t31 = ((int64_t)(v1));
  t32 = fg155_type_block(t31);
  v5 = ((int64_t)(t32));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) <= (t34));
  t36 = t35;
  if (!t36) {
  t37 = ((int64_t)(v2));
  t38 = ((int64_t)(v5));
  t39 = ((int64_t)(((int64_t)gg_mod(t37, t38))));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t39) != (t40));
  }
  t41 = t36;
  if (t41) {
  {
  fg155_fail();
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t42 = ((int64_t)(v1));
  t43 = fg155_type_bytes(t42);
  t44 = ((uint64_t)(t43));
  t45 = ((uint64_t)(((uint64_t)(v2))));
  t46 = ((uint64_t)(((uint64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t47 = ((uint64_t)(((uint64_t)(v5))));
  t0 = ((int64_t)(((int64_t)(((uint64_t)gg_udiv(t46, t47))))));
  goto gg_done;
  }
  }
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(5))));
  t50 = ((t48) == (t49));
  if (t50) {
  {
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = fg155_ne(t51, t52);
  t54 = ((int64_t)(t53));
  t55 = ((int64_t)(((int64_t)UINT64_C(1))));
  t56 = ((t54) == (t55));
  t57 = t56;
  if (t57) {
  t58 = ((int64_t)(v1));
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = fg155_ne(t58, t59);
  t61 = ((int64_t)(t60));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t57 = ((t61) == (t62));
  }
  t63 = t57;
  t64 = t63;
  if (t64) {
  t65 = ((int64_t)(v1));
  t66 = ((int64_t)(((int64_t)UINT64_C(2))));
  t67 = fg155_ne(t65, t66);
  t68 = ((int64_t)(t67));
  t69 = ((int64_t)(((int64_t)UINT64_C(1))));
  t64 = ((t68) == (t69));
  }
  t70 = t64;
  t71 = t70;
  if (t71) {
  t72 = ((int64_t)(v1));
  t73 = ((int64_t)(((int64_t)UINT64_C(3))));
  t74 = fg155_ne(t72, t73);
  t75 = ((int64_t)(t74));
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  t71 = ((t75) == (t76));
  }
  t77 = t71;
  t0 = gg_function_13(t77, base + 0);
  goto gg_done;
  }
  }
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(((int64_t)UINT64_C(6))));
  t80 = ((t78) == (t79));
  if (t80) {
  {
  t81 = ((int64_t)(v1));
  t82 = ((int64_t)(((int64_t)UINT64_C(1))));
  t83 = fg155_ne(t81, t82);
  t84 = ((int64_t)(t83));
  t85 = ((int64_t)(((int64_t)UINT64_C(1))));
  t86 = ((t84) == (t85));
  t87 = t86;
  if (t87) {
  t88 = ((int64_t)(v1));
  t89 = ((int64_t)(((int64_t)UINT64_C(2))));
  t90 = fg155_ne(t88, t89);
  t91 = ((int64_t)(t90));
  t92 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((t91) == (t92));
  }
  t93 = t87;
  t94 = t93;
  if (t94) {
  t95 = ((int64_t)(v1));
  t96 = ((int64_t)(((int64_t)UINT64_C(3))));
  t97 = fg155_ne(t95, t96);
  t98 = ((int64_t)(t97));
  t99 = ((int64_t)(((int64_t)UINT64_C(1))));
  t94 = ((t98) == (t99));
  }
  t100 = t94;
  t0 = gg_function_13(t100, base + 0);
  goto gg_done;
  }
  }
  t101 = ((int64_t)(v0));
  t102 = ((int64_t)(((int64_t)UINT64_C(7))));
  t103 = ((t101) == (t102));
  if (t103) {
  {
  t104 = ((int64_t)(v1));
  t105 = ((int64_t)(((int64_t)UINT64_C(2))));
  t106 = fg155_ne(t104, t105);
  t107 = ((int64_t)(t106));
  t108 = ((int64_t)(((int64_t)UINT64_C(1))));
  t109 = ((t107) == (t108));
  t110 = t109;
  if (t110) {
  t111 = ((int64_t)(v1));
  t112 = ((int64_t)(((int64_t)UINT64_C(3))));
  t113 = fg155_ne(t111, t112);
  t114 = ((int64_t)(t113));
  t115 = ((int64_t)(((int64_t)UINT64_C(1))));
  t110 = ((t114) == (t115));
  }
  t116 = t110;
  t0 = gg_function_13(t116, base + 0);
  goto gg_done;
  }
  }
  t117 = ((int64_t)(v0));
  t118 = ((int64_t)(((int64_t)UINT64_C(8))));
  t119 = ((t117) == (t118));
  if (t119) {
  {
  t120 = ((int64_t)(v1));
  t121 = ((int64_t)(((int64_t)UINT64_C(3))));
  t122 = fg155_ne(t120, t121);
  t123 = ((int64_t)(t122));
  t124 = ((int64_t)(((int64_t)UINT64_C(1))));
  t125 = ((t123) == (t124));
  t0 = gg_function_13(t125, base + 0);
  goto gg_done;
  }
  }
  t126 = ((int64_t)(v0));
  t127 = ((int64_t)(((int64_t)UINT64_C(9))));
  t128 = ((t126) == (t127));
  if (t128) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(3))));
label1:;
  t129 = ((int64_t)(v6));
  t130 = ((int64_t)(((int64_t)UINT64_C(1))));
  if (!(((t129) >= (t130)))) goto label2;
  {
  t131 = ((int64_t)(v1));
  t132 = ((int64_t)(v6));
  t133 = fg155_ne(t131, t132);
  t134 = ((int64_t)(t133));
  t135 = ((int64_t)(((int64_t)UINT64_C(1))));
  t136 = ((t134) > (t135));
  if (t136) {
  {
  t137 = ((int64_t)(v6));
  t138 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t137) + (uint64_t)(t138)))));
  goto gg_done;
  }
  }
  t139 = v6;
  t140 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t139) - (uint64_t)(t140)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t141 = ((int64_t)(v0));
  t142 = ((int64_t)(((int64_t)UINT64_C(10))));
  t143 = ((t141) == (t142));
  if (t143) {
  {
  t144 = ((int64_t)(v1));
  t145 = ((int64_t)(v2));
  t146 = ((int64_t)(v3));
  t147 = gg_function_14(t144, t145, t146, base + 0);
  t0 = gg_function_13(t147, base + 0);
  goto gg_done;
  }
  }
  t148 = ((int64_t)(v0));
  t149 = ((int64_t)(((int64_t)UINT64_C(11))));
  t150 = ((t148) == (t149));
  if (t150) {
  {
  t151 = ((int64_t)(v1));
  t152 = gg_function_5(t151, base + 0);
  t153 = ((uint64_t)(t152));
  t154 = ((int64_t)(v1));
  t155 = gg_function_2(t154, base + 0);
  t156 = ((uint64_t)(((uint64_t)(t155))));
  t157 = ((int64_t)(v1));
  t158 = fg155_tensor_bytes(t157);
  t159 = ((uint64_t)(t158));
  t160 = ((uint64_t)(((uint64_t)((uint64_t)(t156) * (uint64_t)(t159)))));
  t161 = ((int64_t)(v1));
  t162 = fg155_tensor_block(t161);
  t163 = ((uint64_t)(((uint64_t)(t162))));
  t164 = ((uint64_t)(((uint64_t)gg_udiv(t160, t163))));
  t165 = ((t153) == (t164));
  t0 = gg_function_13(t165, base + 0);
  goto gg_done;
  }
  }
  t166 = ((int64_t)(v0));
  t167 = ((int64_t)(((int64_t)UINT64_C(12))));
  t168 = ((t166) == (t167));
  if (t168) {
  {
  t169 = ((int64_t)(v1));
  t170 = ((int64_t)(((int64_t)UINT64_C(0))));
  t171 = fg155_nb(t169, t170);
  t172 = ((uint64_t)(t171));
  t173 = ((int64_t)(v1));
  t174 = ((int64_t)(((int64_t)UINT64_C(1))));
  t175 = fg155_nb(t173, t174);
  t176 = ((uint64_t)(t175));
  t177 = ((t172) > (t176));
  t0 = gg_function_13(t177, base + 0);
  goto gg_done;
  }
  }
  t178 = ((int64_t)(v0));
  t179 = ((int64_t)(((int64_t)UINT64_C(13))));
  t180 = ((t178) == (t179));
  if (t180) {
  {
  t181 = ((int64_t)(v1));
  t182 = ((int64_t)(((int64_t)UINT64_C(0))));
  t183 = fg155_nb(t181, t182);
  t184 = ((uint64_t)(t183));
  t185 = ((int64_t)(v1));
  t186 = ((int64_t)(((int64_t)UINT64_C(1))));
  t187 = fg155_nb(t185, t186);
  t188 = ((uint64_t)(t187));
  t189 = ((t184) > (t188));
  t190 = t189;
  if (!t190) {
  t191 = ((int64_t)(v1));
  t192 = ((int64_t)(((int64_t)UINT64_C(1))));
  t193 = fg155_nb(t191, t192);
  t194 = ((uint64_t)(t193));
  t195 = ((int64_t)(v1));
  t196 = ((int64_t)(((int64_t)UINT64_C(2))));
  t197 = fg155_nb(t195, t196);
  t198 = ((uint64_t)(t197));
  t190 = ((t194) > (t198));
  }
  t199 = t190;
  t200 = t199;
  if (!t200) {
  t201 = ((int64_t)(v1));
  t202 = ((int64_t)(((int64_t)UINT64_C(2))));
  t203 = fg155_nb(t201, t202);
  t204 = ((uint64_t)(t203));
  t205 = ((int64_t)(v1));
  t206 = ((int64_t)(((int64_t)UINT64_C(3))));
  t207 = fg155_nb(t205, t206);
  t208 = ((uint64_t)(t207));
  t200 = ((t204) > (t208));
  }
  t209 = t200;
  t0 = gg_function_13(t209, base + 0);
  goto gg_done;
  }
  }
  t210 = ((int64_t)(v0));
  t211 = ((int64_t)(((int64_t)UINT64_C(14))));
  t212 = ((t210) == (t211));
  if (t212) {
  {
  t213 = ((int64_t)(v1));
  t214 = ((int64_t)(((int64_t)UINT64_C(0))));
  t215 = fg155_nb(t213, t214);
  t216 = ((uint64_t)(t215));
  t217 = ((int64_t)(v1));
  t218 = ((int64_t)(((int64_t)UINT64_C(2))));
  t219 = fg155_nb(t217, t218);
  t220 = ((uint64_t)(t219));
  t221 = ((t216) > (t220));
  t222 = t221;
  if (t222) {
  t223 = ((int64_t)(v1));
  t224 = ((int64_t)(((int64_t)UINT64_C(1))));
  t225 = fg155_nb(t223, t224);
  t226 = ((uint64_t)(t225));
  t227 = ((int64_t)(v1));
  t228 = ((int64_t)(((int64_t)UINT64_C(0))));
  t229 = fg155_nb(t227, t228);
  t230 = ((uint64_t)(t229));
  t222 = ((t226) > (t230));
  }
  t231 = t222;
  t232 = t231;
  if (t232) {
  t233 = ((int64_t)(v1));
  t234 = ((int64_t)(((int64_t)UINT64_C(2))));
  t235 = fg155_nb(t233, t234);
  t236 = ((uint64_t)(t235));
  t237 = ((int64_t)(v1));
  t238 = fg155_tensor_bytes(t237);
  t239 = ((uint64_t)(t238));
  t232 = ((t236) == (t239));
  }
  t240 = t232;
  t0 = gg_function_13(t240, base + 0);
  goto gg_done;
  }
  }
  t241 = ((int64_t)(v0));
  t242 = ((int64_t)(((int64_t)UINT64_C(15))));
  t243 = ((t241) == (t242));
  if (t243) {
  {
  t244 = ((int64_t)(v1));
  t245 = ((int64_t)(((int64_t)UINT64_C(0))));
  t246 = fg155_ne(t244, t245);
  t247 = ((int64_t)(t246));
  t248 = ((int64_t)(v1));
  t249 = fg155_tensor_block(t248);
  t250 = ((int64_t)(t249));
  t251 = ((t247) == (t250));
  t252 = t251;
  if (!t252) {
  t253 = ((int64_t)(v1));
  t254 = ((int64_t)(((int64_t)UINT64_C(0))));
  t255 = fg155_nb(t253, t254);
  t256 = ((uint64_t)(t255));
  t257 = ((int64_t)(v1));
  t258 = fg155_tensor_bytes(t257);
  t259 = ((uint64_t)(t258));
  t252 = ((t256) == (t259));
  }
  t260 = t252;
  t0 = gg_function_13(t260, base + 0);
  goto gg_done;
  }
  }
  t261 = ((int64_t)(v0));
  t262 = ((int64_t)(((int64_t)UINT64_C(16))));
  t263 = ((t261) == (t262));
  if (t263) {
  {
  t264 = ((int64_t)(v1));
  t265 = ((int64_t)(((int64_t)UINT64_C(0))));
  t266 = fg155_nb(t264, t265);
  t267 = ((uint64_t)(t266));
  t268 = ((int64_t)(v1));
  t269 = fg155_tensor_bytes(t268);
  t270 = ((uint64_t)(t269));
  t271 = ((t267) == (t270));
  t272 = t271;
  if (t272) {
  t273 = ((int64_t)(v1));
  t274 = ((int64_t)(((int64_t)UINT64_C(2))));
  t275 = fg155_nb(t273, t274);
  t276 = ((uint64_t)(t275));
  t277 = ((int64_t)(v1));
  t278 = ((int64_t)(((int64_t)UINT64_C(1))));
  t279 = fg155_nb(t277, t278);
  t280 = ((uint64_t)(t279));
  t281 = ((int64_t)(v1));
  t282 = ((int64_t)(((int64_t)UINT64_C(1))));
  t283 = fg155_ne(t281, t282);
  t284 = ((uint64_t)(((uint64_t)(t283))));
  t285 = ((uint64_t)(((uint64_t)((uint64_t)(t280) * (uint64_t)(t284)))));
  t272 = ((t276) == (t285));
  }
  t286 = t272;
  t287 = t286;
  if (t287) {
  t288 = ((int64_t)(v1));
  t289 = ((int64_t)(((int64_t)UINT64_C(3))));
  t290 = fg155_nb(t288, t289);
  t291 = ((uint64_t)(t290));
  t292 = ((int64_t)(v1));
  t293 = ((int64_t)(((int64_t)UINT64_C(2))));
  t294 = fg155_nb(t292, t293);
  t295 = ((uint64_t)(t294));
  t296 = ((int64_t)(v1));
  t297 = ((int64_t)(((int64_t)UINT64_C(2))));
  t298 = fg155_ne(t296, t297);
  t299 = ((uint64_t)(((uint64_t)(t298))));
  t300 = ((uint64_t)(((uint64_t)((uint64_t)(t295) * (uint64_t)(t299)))));
  t287 = ((t291) == (t300));
  }
  t301 = t287;
  t0 = gg_function_13(t301, base + 0);
  goto gg_done;
  }
  }
  t302 = ((int64_t)(v0));
  t303 = ((int64_t)(((int64_t)UINT64_C(17))));
  t304 = ((t302) == (t303));
  if (t304) {
  {
  t305 = ((int64_t)(v1));
  t306 = gg_function_15(t305, base + 0);
  t0 = gg_function_13(t306, base + 0);
  goto gg_done;
  }
  }
  t307 = ((int64_t)(v0));
  t308 = ((int64_t)(((int64_t)UINT64_C(18))));
  t309 = ((t307) == (t308));
  if (t309) {
  {
  t310 = ((int64_t)(v1));
  t311 = ((int64_t)(v2));
  t312 = gg_function_16(t310, t311, base + 0);
  t0 = gg_function_13(t312, base + 0);
  goto gg_done;
  }
  }
  t313 = ((int64_t)(v0));
  t314 = ((int64_t)(((int64_t)UINT64_C(19))));
  t315 = ((t313) == (t314));
  if (t315) {
  {
  t316 = ((int64_t)(v1));
  t317 = ((int64_t)(v2));
  t318 = gg_function_17(t316, t317, base + 0);
  t0 = gg_function_13(t318, base + 0);
  goto gg_done;
  }
  }
  t319 = ((int64_t)(v0));
  t320 = ((int64_t)(((int64_t)UINT64_C(20))));
  t321 = ((t319) == (t320));
  if (t321) {
  {
  t322 = ((int64_t)(v1));
  t323 = ((int64_t)(v2));
  t324 = gg_function_18(t322, t323, base + 0);
  t0 = gg_function_13(t324, base + 0);
  goto gg_done;
  }
  }
  t325 = ((int64_t)(v0));
  t326 = ((int64_t)(((int64_t)UINT64_C(21))));
  t327 = ((t325) == (t326));
  if (t327) {
  {
  t328 = ((int64_t)(v1));
  t329 = ((int64_t)(((int64_t)UINT64_C(0))));
  t330 = fg155_ne(t328, t329);
  t331 = ((int64_t)(t330));
  t332 = ((int64_t)(v2));
  t333 = ((int64_t)(((int64_t)UINT64_C(0))));
  t334 = fg155_ne(t332, t333);
  t335 = ((int64_t)(t334));
  t336 = ((t331) == (t335));
  t337 = t336;
  if (t337) {
  t338 = ((int64_t)(v1));
  t339 = ((int64_t)(v2));
  t340 = gg_function_18(t338, t339, base + 0);
  t337 = t340;
  }
  t341 = t337;
  t0 = gg_function_13(t341, base + 0);
  goto gg_done;
  }
  }
  t342 = ((int64_t)(v0));
  t343 = ((int64_t)(((int64_t)UINT64_C(22))));
  t344 = ((t342) == (t343));
  if (t344) {
  {
  t345 = ((int64_t)(v1));
  t346 = ((int64_t)(((int64_t)UINT64_C(0))));
  t347 = fg155_ne(t345, t346);
  t348 = ((int64_t)(t347));
  t349 = ((int64_t)(v2));
  t350 = ((int64_t)(((int64_t)UINT64_C(0))));
  t351 = fg155_ne(t349, t350);
  t352 = ((int64_t)(t351));
  t353 = ((t348) == (t352));
  t354 = t353;
  if (t354) {
  t355 = ((int64_t)(v2));
  t356 = ((int64_t)(((int64_t)UINT64_C(2))));
  t357 = fg155_ne(t355, t356);
  t358 = ((int64_t)(t357));
  t359 = ((int64_t)(v1));
  t360 = ((int64_t)(((int64_t)UINT64_C(2))));
  t361 = fg155_ne(t359, t360);
  t362 = ((int64_t)(t361));
  t363 = ((int64_t)(((int64_t)gg_mod(t358, t362))));
  t364 = ((int64_t)(((int64_t)UINT64_C(0))));
  t354 = ((t363) == (t364));
  }
  t365 = t354;
  t366 = t365;
  if (t366) {
  t367 = ((int64_t)(v2));
  t368 = ((int64_t)(((int64_t)UINT64_C(3))));
  t369 = fg155_ne(t367, t368);
  t370 = ((int64_t)(t369));
  t371 = ((int64_t)(v1));
  t372 = ((int64_t)(((int64_t)UINT64_C(3))));
  t373 = fg155_ne(t371, t372);
  t374 = ((int64_t)(t373));
  t375 = ((int64_t)(((int64_t)gg_mod(t370, t374))));
  t376 = ((int64_t)(((int64_t)UINT64_C(0))));
  t366 = ((t375) == (t376));
  }
  t377 = t366;
  t0 = gg_function_13(t377, base + 0);
  goto gg_done;
  }
  }
  t378 = ((int64_t)(v0));
  t379 = ((int64_t)(((int64_t)UINT64_C(23))));
  t380 = ((t378) == (t379));
  if (t380) {
  {
  t381 = ((int64_t)(v1));
  t382 = ((int64_t)(((int64_t)UINT64_C(1))));
  t383 = fg155_ne(t381, t382);
  t384 = ((int64_t)(t383));
  t385 = ((int64_t)(v2));
  t386 = ((int64_t)(((int64_t)UINT64_C(1))));
  t387 = fg155_ne(t385, t386);
  t388 = ((int64_t)(t387));
  t389 = ((t384) == (t388));
  t390 = t389;
  if (t390) {
  t391 = ((int64_t)(v2));
  t392 = ((int64_t)(((int64_t)UINT64_C(2))));
  t393 = fg155_ne(t391, t392);
  t394 = ((int64_t)(t393));
  t395 = ((int64_t)(v1));
  t396 = ((int64_t)(((int64_t)UINT64_C(2))));
  t397 = fg155_ne(t395, t396);
  t398 = ((int64_t)(t397));
  t399 = ((int64_t)(((int64_t)gg_mod(t394, t398))));
  t400 = ((int64_t)(((int64_t)UINT64_C(0))));
  t390 = ((t399) == (t400));
  }
  t401 = t390;
  t402 = t401;
  if (t402) {
  t403 = ((int64_t)(v2));
  t404 = ((int64_t)(((int64_t)UINT64_C(3))));
  t405 = fg155_ne(t403, t404);
  t406 = ((int64_t)(t405));
  t407 = ((int64_t)(v1));
  t408 = ((int64_t)(((int64_t)UINT64_C(3))));
  t409 = fg155_ne(t407, t408);
  t410 = ((int64_t)(t409));
  t411 = ((int64_t)(((int64_t)gg_mod(t406, t410))));
  t412 = ((int64_t)(((int64_t)UINT64_C(0))));
  t402 = ((t411) == (t412));
  }
  t413 = t402;
  t0 = gg_function_13(t413, base + 0);
  goto gg_done;
  }
  }
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
  {
  t0.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(999))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = gg_function_1(t1, t2, t3, t4, base + 1);
  gg_integer(gg_ref(&t0, gg_get_stack(base + 0)), t5);
  gg_append(gg_ref(&t0, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t0).base, (t0).len));
  }
gg_done:;
  for (t6 = 0; t6 < 1; t6++) gg_stacks[base + t6].top = 0;
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
