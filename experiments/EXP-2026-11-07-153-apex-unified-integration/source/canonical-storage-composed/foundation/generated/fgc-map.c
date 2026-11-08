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
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_3(int64_t base);
static int64_t gg_function_5(int64_t base);
static void gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_9(int64_t base);
static int64_t gg_function_10(int64_t p0, int64_t p1, int64_t base);
static void gg_function_11(uint8_t p0, int64_t base);
static int64_t gg_function_15(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_18(int64_t p0, int64_t base);
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint8_t gg_function_21(int64_t p0, int64_t base);
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_23(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_24(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_25(int64_t p0, int64_t p1, int64_t base);
static void gg_function_26(int64_t p0, int64_t base);
static int64_t gg_function_28(int64_t p0, int64_t base);
static int64_t gg_function_29(int64_t p0, int64_t base);
static void gg_function_11(uint8_t p0, int64_t base) {
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
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  fgc_abort(t2);
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
gg_done:;
}
static void gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v2));
  fgc_store_i64(t0, t1, t2);
  }
gg_done:;
}
static int64_t gg_function_15(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = fgc_load_i64(t1, t2);
  t0 = ((int64_t)(t3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_5(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1296126001))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_21(int64_t p0, int64_t base) {
  uint8_t t0;
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
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = gg_function_15(t5, t6, base + 0);
  t8 = ((int64_t)(t7));
  t9 = gg_function_5(base + 0);
  t10 = ((int64_t)(t9));
  t4 = ((t8) == (t10));
  }
  t0 = t4;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_29(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
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
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_21(t1, base + 0);
  t3 = t2;
  t4 = (!(t3));
  if (t4) {
  {
  t5 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t5)))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = gg_function_15(t6, t7, base + 0);
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(6))));
  t10 = gg_function_15(t8, t9, base + 0);
  t11 = ((int64_t)(v1));
  t12 = fgc_memzero(t10, t11);
  v2 = ((int64_t)(t12));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t13, t14, t15, base + 0);
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(3))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t16, t17, t18, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_28(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_21(t1, base + 0);
  t3 = t2;
  t4 = (!(t3));
  if (t4) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t0 = gg_function_15(t5, t6, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_9(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) == (t10));
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v1));
  t14 = gg_function_9(base + 0);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)gg_div(t15, t16))));
  t12 = ((t13) <= (t17));
  }
  t8 = t12;
  }
  t18 = t8;
  gg_function_11(t18, base + 0);
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_26(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
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
  int64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  goto gg_done;
  }
  }
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = gg_function_5(base + 0);
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fgc_cas_i64(t3, t4, t5, t6);
  v1 = ((int64_t)(t7));
  t8 = ((int64_t)(v1));
  t9 = gg_function_5(base + 0);
  t10 = ((int64_t)(t9));
  t11 = ((t8) != (t10));
  if (t11) {
  {
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = gg_function_15(t12, t13, base + 0);
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(4))));
  v3 = gg_function_15(t14, t15, base + 0);
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(5))));
  v4 = gg_function_15(t16, t17, base + 0);
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(6))));
  v5 = gg_function_15(t18, t19, base + 0);
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) != (t21));
  if (t22) {
  {
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(((int64_t)UINT64_C(8))));
  t26 = gg_function_10(t24, t25, base + 0);
  fgc_free_ptr(t23, t26);
  }
  }
  t27 = ((int64_t)(v4));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t27) != (t28));
  if (t29) {
  {
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(((int64_t)UINT64_C(8))));
  t33 = gg_function_10(t31, t32, base + 0);
  fgc_free_ptr(t30, t33);
  }
  }
  t34 = ((int64_t)(v5));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) != (t35));
  if (t36) {
  {
  t37 = ((int64_t)(v5));
  t38 = ((int64_t)(v2));
  fgc_free_ptr(t37, t38);
  }
  }
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(56))));
  fgc_free_ptr(t39, t40);
  t41 = ((int64_t)(((int64_t)UINT64_C(22))));
  fgc_witness(t41);
  }
gg_done:;
}
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v0));
  t10 = gg_function_9(base + 0);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) - (uint64_t)(t12)))));
  t8 = ((t9) <= (t13));
  }
  t14 = t8;
  gg_function_11(t14, base + 0);
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
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
  int64_t t4;
  int64_t t5;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((t1) >= (t2));
  gg_function_11(t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t4) - (uint64_t)(t5)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_18(int64_t p0, int64_t base) {
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
  int64_t v1;
  v0 = p0;
  {
  v1 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(33))));
  t4 = ((int64_t)(((t2) >> ((uint64_t)(t3) & 63))));
  v1 = ((int64_t)(((t1) ^ (t4))));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(4503599627370495))));
  v1 = ((int64_t)(((t5) & (t6))));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(11))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) << ((uint64_t)(t9) & 63)))));
  v1 = ((int64_t)(((t7) ^ (t10))));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  v1 = ((int64_t)(((t11) & (t12))));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(((int64_t)UINT64_C(29))));
  t16 = ((int64_t)(((t14) >> ((uint64_t)(t15) & 63))));
  v1 = ((int64_t)(((t13) ^ (t16))));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  v1 = ((int64_t)(((t17) & (t18))));
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_25(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
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
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_21(t1, base + 0);
  t3 = t2;
  t4 = (!(t3));
  if (t4) {
  {
  t5 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t5)))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = gg_function_15(t6, t7, base + 0);
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t8) - (uint64_t)(t9)))));
  t10 = ((int64_t)(v1));
  t11 = gg_function_18(t10, base + 0);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(v3));
  v4 = ((int64_t)(((t12) & (t13))));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t14 = ((int64_t)(v5));
  t15 = ((int64_t)(v2));
  if (!(((t14) < (t15)))) goto label2;
  {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(6))));
  t18 = gg_function_15(t16, t17, base + 0);
  t19 = ((int64_t)(v4));
  t20 = fgc_load_u8(t18, t19);
  v6 = ((int64_t)(t20));
  t21 = ((int64_t)(v6));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) == (t22));
  if (t23) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t24) == (t25));
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  t30 = gg_function_15(t28, t29, base + 0);
  t31 = ((int64_t)(v4));
  t32 = fgc_load_i64(t30, t31);
  t33 = ((int64_t)(t32));
  t34 = ((int64_t)(v1));
  t27 = ((t33) == (t34));
  }
  t35 = t27;
  if (t35) {
  {
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(6))));
  t38 = gg_function_15(t36, t37, base + 0);
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(((int64_t)UINT64_C(2))));
  fgc_store_u8(t38, t39, t40);
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(2))));
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(2))));
  t45 = gg_function_15(t43, t44, base + 0);
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t47 = gg_function_23(t45, t46, base + 0);
  gg_function_6(t41, t42, t47, base + 0);
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(3))));
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(3))));
  t52 = gg_function_15(t50, t51, base + 0);
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  t54 = gg_function_22(t52, t53, base + 0);
  gg_function_6(t48, t49, t54, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t55 = ((int64_t)(v4));
  t56 = ((int64_t)(((int64_t)UINT64_C(1))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
  t58 = ((int64_t)(v3));
  v4 = ((int64_t)(((t57) & (t58))));
  t59 = v5;
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t59) + (uint64_t)(t60)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_24(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
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
  uint8_t t26;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_21(t1, base + 0);
  t3 = t2;
  t4 = (!(t3));
  t5 = t4;
  if (!t5) {
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t6) == (t7));
  }
  t8 = t5;
  if (t8) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = gg_function_15(t9, t10, base + 0);
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t11) - (uint64_t)(t12)))));
  t13 = ((int64_t)(v1));
  t14 = gg_function_18(t13, base + 0);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(v4));
  v5 = ((int64_t)(((t15) & (t16))));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(v3));
  if (!(((t17) < (t18)))) goto label2;
  {
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(6))));
  t21 = gg_function_15(t19, t20, base + 0);
  t22 = ((int64_t)(v5));
  t23 = fgc_load_u8(t21, t22);
  v7 = ((int64_t)(t23));
  t24 = ((int64_t)(v7));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) == (t25));
  if (t26) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t27 = ((int64_t)(v7));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = ((t27) == (t28));
  t30 = t29;
  if (t30) {
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(4))));
  t33 = gg_function_15(t31, t32, base + 0);
  t34 = ((int64_t)(v5));
  t35 = fgc_load_i64(t33, t34);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(v1));
  t30 = ((t36) == (t37));
  }
  t38 = t30;
  if (t38) {
  {
  t39 = ((int64_t)(v2));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(5))));
  t43 = gg_function_15(t41, t42, base + 0);
  t44 = ((int64_t)(v5));
  t45 = fgc_load_i64(t43, t44);
  t46 = ((int64_t)(t45));
  fgc_store_i64(t39, t40, t46);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t47 = ((int64_t)(v5));
  t48 = ((int64_t)(((int64_t)UINT64_C(1))));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t50 = ((int64_t)(v4));
  v5 = ((int64_t)(((t49) & (t50))));
  t51 = v6;
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t51) + (uint64_t)(t52)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
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
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
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
  uint8_t t60;
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
  uint8_t t93;
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
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
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
  v0 = p0;
  v1 = p1;
  {
  v2 = ((int64_t)(v1));
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(8))));
  }
  }
  t4 = gg_function_9(base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(8))));
  v3 = ((int64_t)(((int64_t)gg_div(t5, t6))));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(v3));
  t9 = ((t7) > (t8));
  if (t9) {
  {
  t10 = ((int64_t)UINT64_C(3));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t10)))));
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = gg_function_10(t11, t12, base + 0);
  t13 = ((int64_t)(v4));
  t14 = fgc_alloc(t13);
  v5 = ((int64_t)(t14));
  t15 = ((int64_t)(v5));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t18 = ((int64_t)(((int64_t)UINT64_C(20))));
  fgc_witness(t18);
  t19 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t19)))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v4));
  t21 = fgc_alloc(t20);
  v6 = ((int64_t)(t21));
  t22 = ((int64_t)(v6));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(v4));
  fgc_free_ptr(t25, t26);
  t27 = ((int64_t)(((int64_t)UINT64_C(20))));
  fgc_witness(t27);
  t28 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t28)))));
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v2));
  t30 = fgc_alloc(t29);
  v7 = ((int64_t)(t30));
  t31 = ((int64_t)(v7));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t34 = ((int64_t)(v5));
  t35 = ((int64_t)(v4));
  fgc_free_ptr(t34, t35);
  t36 = ((int64_t)(v6));
  t37 = ((int64_t)(v4));
  fgc_free_ptr(t36, t37);
  t38 = ((int64_t)(((int64_t)UINT64_C(20))));
  fgc_witness(t38);
  t39 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t39)))));
  goto gg_done;
  }
  }
  t40 = ((int64_t)(v7));
  t41 = ((int64_t)(v2));
  t42 = fgc_memzero(t40, t41);
  v8 = ((int64_t)(t42));
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = gg_function_15(t43, t44, base + 0);
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(4))));
  v10 = gg_function_15(t45, t46, base + 0);
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(5))));
  v11 = gg_function_15(t47, t48, base + 0);
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(6))));
  v12 = gg_function_15(t49, t50, base + 0);
  t51 = ((int64_t)(v2));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t51) - (uint64_t)(t52)))));
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t53 = ((int64_t)(v14));
  t54 = ((int64_t)(v9));
  if (!(((t53) < (t54)))) goto label2;
  {
  t55 = ((int64_t)(v12));
  t56 = ((int64_t)(v14));
  t57 = fgc_load_u8(t55, t56);
  t58 = ((int64_t)(t57));
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((t58) == (t59));
  if (t60) {
  {
  t61 = ((int64_t)(v10));
  t62 = ((int64_t)(v14));
  t63 = fgc_load_i64(t61, t62);
  v15 = ((int64_t)(t63));
  t64 = ((int64_t)(v11));
  t65 = ((int64_t)(v14));
  t66 = fgc_load_i64(t64, t65);
  v16 = ((int64_t)(t66));
  t67 = ((int64_t)(v15));
  t68 = gg_function_18(t67, base + 0);
  t69 = ((int64_t)(t68));
  t70 = ((int64_t)(v13));
  v17 = ((int64_t)(((t69) & (t70))));
label4:;
  t71 = ((int64_t)(v7));
  t72 = ((int64_t)(v17));
  t73 = fgc_load_u8(t71, t72);
  t74 = ((int64_t)(t73));
  t75 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t74) != (t75)))) goto label5;
  {
  t76 = ((int64_t)(v17));
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t77)))));
  t79 = ((int64_t)(v13));
  v17 = ((int64_t)(((t78) & (t79))));
  }
  goto label4;
label5:;
  t80 = ((int64_t)(v5));
  t81 = ((int64_t)(v17));
  t82 = ((int64_t)(v15));
  fgc_store_i64(t80, t81, t82);
  t83 = ((int64_t)(v6));
  t84 = ((int64_t)(v17));
  t85 = ((int64_t)(v16));
  fgc_store_i64(t83, t84, t85);
  t86 = ((int64_t)(v7));
  t87 = ((int64_t)(v17));
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  fgc_store_u8(t86, t87, t88);
  }
  }
  t89 = v14;
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t89) + (uint64_t)(t90)));
  }
  goto label1;
label2:;
  t91 = ((int64_t)(v10));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t91) != (t92));
  if (t93) {
  {
  t94 = ((int64_t)(v10));
  t95 = ((int64_t)(v9));
  t96 = ((int64_t)(((int64_t)UINT64_C(8))));
  t97 = gg_function_10(t95, t96, base + 0);
  fgc_free_ptr(t94, t97);
  t98 = ((int64_t)(v11));
  t99 = ((int64_t)(v9));
  t100 = ((int64_t)(((int64_t)UINT64_C(8))));
  t101 = gg_function_10(t99, t100, base + 0);
  fgc_free_ptr(t98, t101);
  t102 = ((int64_t)(v12));
  t103 = ((int64_t)(v9));
  fgc_free_ptr(t102, t103);
  }
  }
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(((int64_t)UINT64_C(4))));
  t106 = ((int64_t)(v5));
  gg_function_6(t104, t105, t106, base + 0);
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(((int64_t)UINT64_C(5))));
  t109 = ((int64_t)(v6));
  gg_function_6(t107, t108, t109, base + 0);
  t110 = ((int64_t)(v0));
  t111 = ((int64_t)(((int64_t)UINT64_C(6))));
  t112 = ((int64_t)(v7));
  gg_function_6(t110, t111, t112, base + 0);
  t113 = ((int64_t)(v0));
  t114 = ((int64_t)(((int64_t)UINT64_C(1))));
  t115 = ((int64_t)(v2));
  gg_function_6(t113, t114, t115, base + 0);
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(((int64_t)UINT64_C(3))));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t116, t117, t118, base + 0);
  t119 = ((int64_t)(((int64_t)UINT64_C(21))));
  fgc_witness(t119);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
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
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
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
  uint8_t t54;
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
  uint8_t t85;
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
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  uint8_t t110;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_21(t1, base + 0);
  t3 = t2;
  t4 = (!(t3));
  if (t4) {
  {
  t5 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t5)))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = gg_function_15(t6, t7, base + 0);
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  v4 = gg_function_15(t8, t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(3))));
  v5 = gg_function_15(t10, t11, base + 0);
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(v5));
  t14 = gg_function_22(t12, t13, base + 0);
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = gg_function_22(t14, t15, base + 0);
  t17 = ((int64_t)(((int64_t)UINT64_C(10))));
  t18 = gg_function_10(t16, t17, base + 0);
  t19 = ((int64_t)(t18));
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)UINT64_C(7))));
  t22 = gg_function_10(t20, t21, base + 0);
  t23 = ((int64_t)(t22));
  t24 = ((t19) >= (t23));
  if (t24) {
  {
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(v3));
  t27 = ((int64_t)(((int64_t)UINT64_C(2))));
  t28 = gg_function_10(t26, t27, base + 0);
  v6 = gg_function_8(t25, t28, base + 0);
  t29 = ((int64_t)(v6));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t29) != (t30));
  if (t31) {
  {
  t0 = ((int64_t)(v6));
  goto gg_done;
  }
  }
  }
  }
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = gg_function_15(t32, t33, base + 0);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  v8 = gg_function_15(t34, t35, base + 0);
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  v9 = gg_function_15(t36, t37, base + 0);
  t38 = ((int64_t)(v7));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t38) - (uint64_t)(t39)))));
  t40 = ((int64_t)(v1));
  t41 = gg_function_18(t40, base + 0);
  t42 = ((int64_t)(t41));
  t43 = ((int64_t)(v10));
  v11 = ((int64_t)(((t42) & (t43))));
  t44 = ((int64_t)UINT64_C(1));
  v12 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t44)))));
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t45 = ((int64_t)(v13));
  t46 = ((int64_t)(v7));
  if (!(((t45) < (t46)))) goto label2;
  {
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(6))));
  t49 = gg_function_15(t47, t48, base + 0);
  t50 = ((int64_t)(v11));
  t51 = fgc_load_u8(t49, t50);
  v14 = ((int64_t)(t51));
  t52 = ((int64_t)(v14));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = ((t52) == (t53));
  if (t54) {
  {
  v15 = ((int64_t)(v11));
  t55 = ((int64_t)(v12));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t55) >= (t56));
  if (t57) {
  {
  v15 = ((int64_t)(v12));
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(((int64_t)UINT64_C(3))));
  t60 = ((int64_t)(v9));
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  t62 = gg_function_23(t60, t61, base + 0);
  gg_function_6(t58, t59, t62, base + 0);
  }
  }
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(((int64_t)UINT64_C(4))));
  t65 = gg_function_15(t63, t64, base + 0);
  t66 = ((int64_t)(v15));
  t67 = ((int64_t)(v1));
  fgc_store_i64(t65, t66, t67);
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(5))));
  t70 = gg_function_15(t68, t69, base + 0);
  t71 = ((int64_t)(v15));
  t72 = ((int64_t)(v2));
  fgc_store_i64(t70, t71, t72);
  t73 = ((int64_t)(v0));
  t74 = ((int64_t)(((int64_t)UINT64_C(6))));
  t75 = gg_function_15(t73, t74, base + 0);
  t76 = ((int64_t)(v15));
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  fgc_store_u8(t75, t76, t77);
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((int64_t)(v8));
  t81 = ((int64_t)(((int64_t)UINT64_C(1))));
  t82 = gg_function_22(t80, t81, base + 0);
  gg_function_6(t78, t79, t82, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t83 = ((int64_t)(v14));
  t84 = ((int64_t)(((int64_t)UINT64_C(2))));
  t85 = ((t83) == (t84));
  if (t85) {
  {
  t86 = ((int64_t)(v12));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((t86) < (t87));
  if (t88) {
  {
  v12 = ((int64_t)(v11));
  }
  }
  }
  } else {
  {
  t89 = ((int64_t)(v0));
  t90 = ((int64_t)(((int64_t)UINT64_C(4))));
  t91 = gg_function_15(t89, t90, base + 0);
  t92 = ((int64_t)(v11));
  t93 = fgc_load_i64(t91, t92);
  t94 = ((int64_t)(t93));
  t95 = ((int64_t)(v1));
  t96 = ((t94) == (t95));
  if (t96) {
  {
  t97 = ((int64_t)(v0));
  t98 = ((int64_t)(((int64_t)UINT64_C(5))));
  t99 = gg_function_15(t97, t98, base + 0);
  t100 = ((int64_t)(v11));
  t101 = ((int64_t)(v2));
  fgc_store_i64(t99, t100, t101);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  t102 = ((int64_t)(v11));
  t103 = ((int64_t)(((int64_t)UINT64_C(1))));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t103)))));
  t105 = ((int64_t)(v10));
  v11 = ((int64_t)(((t104) & (t105))));
  t106 = v13;
  t107 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)((uint64_t)(t106) + (uint64_t)(t107)));
  }
  goto label1;
label2:;
  t108 = ((int64_t)(v12));
  t109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = ((t108) >= (t109));
  if (t110) {
  {
  t111 = ((int64_t)(v0));
  t112 = ((int64_t)(((int64_t)UINT64_C(4))));
  t113 = gg_function_15(t111, t112, base + 0);
  t114 = ((int64_t)(v12));
  t115 = ((int64_t)(v1));
  fgc_store_i64(t113, t114, t115);
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(((int64_t)UINT64_C(5))));
  t118 = gg_function_15(t116, t117, base + 0);
  t119 = ((int64_t)(v12));
  t120 = ((int64_t)(v2));
  fgc_store_i64(t118, t119, t120);
  t121 = ((int64_t)(v0));
  t122 = ((int64_t)(((int64_t)UINT64_C(6))));
  t123 = gg_function_15(t121, t122, base + 0);
  t124 = ((int64_t)(v12));
  t125 = ((int64_t)(((int64_t)UINT64_C(1))));
  fgc_store_u8(t123, t124, t125);
  t126 = ((int64_t)(v0));
  t127 = ((int64_t)(((int64_t)UINT64_C(3))));
  t128 = ((int64_t)(v9));
  t129 = ((int64_t)(((int64_t)UINT64_C(1))));
  t130 = gg_function_23(t128, t129, base + 0);
  gg_function_6(t126, t127, t130, base + 0);
  t131 = ((int64_t)(v0));
  t132 = ((int64_t)(((int64_t)UINT64_C(2))));
  t133 = ((int64_t)(v8));
  t134 = ((int64_t)(((int64_t)UINT64_C(1))));
  t135 = gg_function_22(t133, t134, base + 0);
  gg_function_6(t131, t132, t135, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t136 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t136)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_3(int64_t base) {
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
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t v0;
  int64_t v1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(56))));
  t2 = fgc_alloc(t1);
  v0 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = gg_function_5(base + 0);
  gg_function_6(t6, t7, t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t9, t10, t11, base + 0);
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t12, t13, t14, base + 0);
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(3))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t15, t16, t17, base + 0);
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t18, t19, t20, base + 0);
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(5))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t21, t22, t23, base + 0);
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(6))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t24, t25, t26, base + 0);
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(8))));
  v1 = gg_function_8(t27, t28, base + 0);
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t29) != (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(56))));
  fgc_free_ptr(t32, t33);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  fgc_witness(t34);
  t0 = ((int64_t)(v0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
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
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  fgc_witness(t1);
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) == (t3));
  if (t4) {
  {
  t0 = gg_function_3(base + 0);
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(v3));
  t0 = gg_function_20(t8, t9, t10, base + 0);
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(2))));
  t13 = ((t11) == (t12));
  if (t13) {
  {
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(v3));
  t0 = gg_function_24(t14, t15, t16, base + 0);
  goto gg_done;
  }
  }
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(3))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t20 = ((int64_t)(v1));
  t21 = ((int64_t)(v2));
  t0 = gg_function_25(t20, t21, base + 0);
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(4))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t25 = ((int64_t)(v1));
  gg_function_26(t25, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(5))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t29 = ((int64_t)(v1));
  t0 = gg_function_28(t29, base + 0);
  goto gg_done;
  }
  }
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(6))));
  t32 = ((t30) == (t31));
  if (t32) {
  {
  t33 = ((int64_t)(v1));
  t0 = gg_function_29(t33, base + 0);
  goto gg_done;
  }
  }
  t34 = ((uint8_t)UINT64_C(0));
  gg_function_11(t34, base + 0);
  t35 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t35)))));
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
  t1 = ((int64_t)(((int64_t)UINT64_C(5))));
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
