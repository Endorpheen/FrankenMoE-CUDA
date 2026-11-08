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
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t base);
static void gg_function_9(int64_t p0, int64_t base);
static uint8_t gg_function_13(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_19(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_28(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_31(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_33(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static void gg_function_35(int64_t p0, int64_t base);
static void gg_function_36(int64_t p0, int64_t base);
static int64_t gg_function_38(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static uint8_t gg_function_39(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static uint8_t gg_function_39(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  uint8_t t18;
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
  uint8_t t30;
  uint8_t t31;
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
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  uint8_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint8_t t76;
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
  uint8_t t148;
  uint8_t t149;
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
  uint8_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  uint8_t t185;
  uint8_t t186;
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
  uint8_t t201;
  uint8_t t202;
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
  uint8_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  uint8_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
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
  int64_t t246;
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
  uint8_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  uint8_t t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(v4));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg107_moe(t1, t2, t3, t4, t5, t6, t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) != (t10));
  if (t11) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = fg107_graph(t12, t13, t14);
  t16 = ((int64_t)(t15));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = fg107_graph(t19, t20, t21);
  v5 = ((int64_t)(t22));
  t23 = ((int64_t)(v5));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg107_tensor(t23, t24, t25);
  t27 = ((int64_t)(t26));
  t28 = fg107_op_mul_mat_id();
  t29 = ((int64_t)(t28));
  t30 = ((t27) != (t29));
  t31 = t30;
  if (!t31) {
  t32 = ((int64_t)(v5));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg107_tensor(t32, t33, t34);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(v3));
  t31 = ((t36) != (t37));
  }
  t38 = t31;
  if (t38) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t39 = ((int64_t)(v5));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  t41 = ((int64_t)(((int64_t)UINT64_C(2))));
  t42 = fg107_tensor(t39, t40, t41);
  v6 = ((int64_t)(t42));
  t43 = ((int64_t)(v6));
  t44 = ((int64_t)(((int64_t)UINT64_C(4))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = fg107_tensor(t43, t44, t45);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((t47) == (t48));
  if (t49) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t50 = ((int64_t)(v2));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = fg107_tensor(t50, t51, t52);
  v7 = ((int64_t)(t53));
  t54 = ((int64_t)(v2));
  t55 = ((int64_t)(((int64_t)UINT64_C(3))));
  t56 = ((int64_t)(((int64_t)UINT64_C(2))));
  t57 = fg107_tensor(t54, t55, t56);
  v8 = ((int64_t)(t57));
  t58 = ((int64_t)(v6));
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(14))));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = fg107_exec(t59, t60, t61, t62, t63);
  t65 = ((int64_t)(t64));
  t66 = ((t58) != (t65));
  t67 = t66;
  if (!t67) {
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(19))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = fg107_exec(t68, t69, t70, t71, t72);
  t74 = ((int64_t)(t73));
  t75 = ((int64_t)(v7));
  t67 = ((t74) != (t75));
  }
  t76 = t67;
  if (t76) {
  {
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(16))));
  t79 = ((int64_t)(v6));
  t80 = ((int64_t)(((int64_t)UINT64_C(5))));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = fg107_tensor(t79, t80, t81);
  t83 = ((int64_t)(t82));
  t84 = ((int64_t)(((int64_t)UINT64_C(4))));
  t85 = ((int64_t)(((int64_t)gg_div(t83, t84))));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = fg107_exec(t77, t78, t85, t86, t87);
  (void)(t88);
  t89 = ((int64_t)(v1));
  t90 = ((int64_t)(v6));
  t91 = ((int64_t)(v0));
  t92 = ((int64_t)(((int64_t)UINT64_C(17))));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = fg107_exec(t91, t92, t93, t94, t95);
  t97 = ((int64_t)(t96));
  t98 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((int64_t)(v6));
  t100 = ((int64_t)(((int64_t)UINT64_C(5))));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = fg107_tensor(t99, t100, t101);
  t103 = ((int64_t)(t102));
  t104 = fg107_tensor_get_async(t89, t90, t97, t98, t103);
  (void)(t104);
  t105 = ((int64_t)(v1));
  t106 = fg107_backend_synchronize(t105);
  (void)(t106);
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(((int64_t)UINT64_C(20))));
  t109 = ((int64_t)(v7));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = fg107_exec(t107, t108, t109, t110, t111);
  (void)(t112);
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t113 = ((int64_t)(v9));
  t114 = ((int64_t)(v6));
  t115 = ((int64_t)(((int64_t)UINT64_C(2))));
  t116 = ((int64_t)(((int64_t)UINT64_C(1))));
  t117 = fg107_tensor(t114, t115, t116);
  t118 = ((int64_t)(t117));
  if (!(((t113) < (t118)))) goto label2;
  {
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t119 = ((int64_t)(v10));
  t120 = ((int64_t)(v6));
  t121 = ((int64_t)(((int64_t)UINT64_C(2))));
  t122 = ((int64_t)(((int64_t)UINT64_C(0))));
  t123 = fg107_tensor(t120, t121, t122);
  t124 = ((int64_t)(t123));
  if (!(((t119) < (t124)))) goto label5;
  {
  t125 = ((int64_t)(v0));
  t126 = ((int64_t)(v9));
  t127 = ((int64_t)(v6));
  t128 = ((int64_t)(((int64_t)UINT64_C(3))));
  t129 = ((int64_t)(((int64_t)UINT64_C(1))));
  t130 = fg107_tensor(t127, t128, t129);
  t131 = ((int64_t)(t130));
  t132 = ((int64_t)(((int64_t)((uint64_t)(t126) * (uint64_t)(t131)))));
  t133 = ((int64_t)(((int64_t)UINT64_C(4))));
  t134 = ((int64_t)(((int64_t)gg_div(t132, t133))));
  t135 = ((int64_t)(v10));
  t136 = ((int64_t)(v6));
  t137 = ((int64_t)(((int64_t)UINT64_C(3))));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = fg107_tensor(t136, t137, t138);
  t140 = ((int64_t)(t139));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t135) * (uint64_t)(t140)))));
  t142 = ((int64_t)(((int64_t)UINT64_C(4))));
  t143 = ((int64_t)(((int64_t)gg_div(t141, t142))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t134) + (uint64_t)(t143)))));
  t145 = fg107_exec_copy_id_at(t125, t144);
  v11 = ((int64_t)(t145));
  t146 = ((int64_t)(v11));
  t147 = ((int64_t)(((int64_t)UINT64_C(0))));
  t148 = ((t146) >= (t147));
  t149 = t148;
  if (t149) {
  t150 = ((int64_t)(v11));
  t151 = ((int64_t)(v7));
  t149 = ((t150) < (t151));
  }
  if (!(t149)) gg_fail("assertion failed");
  t152 = ((int64_t)(v0));
  t153 = ((int64_t)(((int64_t)UINT64_C(22))));
  t154 = ((int64_t)(v11));
  t155 = ((int64_t)(((int64_t)UINT64_C(1))));
  t156 = ((int64_t)(((int64_t)UINT64_C(0))));
  t157 = fg107_exec(t152, t153, t154, t155, t156);
  (void)(t157);
  t158 = v10;
  t159 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t158) + (uint64_t)(t159)));
  }
  goto label4;
label5:;
  t160 = v9;
  t161 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t160) + (uint64_t)(t161)));
  }
  goto label1;
label2:;
  t162 = ((int64_t)(v0));
  t163 = ((int64_t)(((int64_t)UINT64_C(15))));
  t164 = ((int64_t)(v6));
  t165 = ((int64_t)(((int64_t)UINT64_C(0))));
  t166 = ((int64_t)(((int64_t)UINT64_C(0))));
  t167 = fg107_exec(t162, t163, t164, t165, t166);
  (void)(t167);
  }
  }
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t168 = ((int64_t)(v12));
  t169 = ((int64_t)(v7));
  if (!(((t168) < (t169)))) goto label8;
  {
  t170 = ((int64_t)(v0));
  t171 = ((int64_t)(((int64_t)UINT64_C(21))));
  t172 = ((int64_t)(v12));
  t173 = ((int64_t)(((int64_t)UINT64_C(0))));
  t174 = ((int64_t)(((int64_t)UINT64_C(0))));
  t175 = fg107_exec(t170, t171, t172, t173, t174);
  t176 = ((int64_t)(t175));
  t177 = ((int64_t)(((int64_t)UINT64_C(0))));
  t178 = ((t176) == (t177));
  if (t178) {
  {
  t179 = v12;
  t180 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t179) + (uint64_t)(t180)));
  }
  } else {
  {
  v13 = ((int64_t)(v12));
label10:;
  t181 = ((int64_t)(v13));
  t182 = ((int64_t)(((int64_t)UINT64_C(1))));
  t183 = ((int64_t)(((int64_t)((uint64_t)(t181) + (uint64_t)(t182)))));
  t184 = ((int64_t)(v7));
  t185 = ((t183) < (t184));
  t186 = t185;
  if (t186) {
  t187 = ((int64_t)(v0));
  t188 = ((int64_t)(((int64_t)UINT64_C(21))));
  t189 = ((int64_t)(v13));
  t190 = ((int64_t)(((int64_t)UINT64_C(1))));
  t191 = ((int64_t)(((int64_t)((uint64_t)(t189) + (uint64_t)(t190)))));
  t192 = ((int64_t)(((int64_t)UINT64_C(0))));
  t193 = ((int64_t)(((int64_t)UINT64_C(0))));
  t194 = fg107_exec(t187, t188, t191, t192, t193);
  t195 = ((int64_t)(t194));
  t196 = ((int64_t)(((int64_t)UINT64_C(0))));
  t186 = ((t195) != (t196));
  }
  if (!(t186)) goto label11;
  {
  t197 = v13;
  t198 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)((uint64_t)(t197) + (uint64_t)(t198)));
  }
  goto label10;
label11:;
  v14 = ((int64_t)(v12));
  v15 = ((int64_t)(v12));
label13:;
  t199 = ((int64_t)(v15));
  t200 = ((int64_t)(v13));
  t201 = ((t199) <= (t200));
  t202 = t201;
  if (t202) {
  t203 = ((int64_t)(v0));
  t204 = ((int64_t)(((int64_t)UINT64_C(0))));
  t205 = ((int64_t)(((int64_t)UINT64_C(0))));
  t206 = ((int64_t)(((int64_t)UINT64_C(0))));
  t207 = ((int64_t)(((int64_t)UINT64_C(0))));
  t208 = ((int64_t)(((int64_t)UINT64_C(0))));
  t209 = ((int64_t)(((int64_t)UINT64_C(0))));
  t210 = fg107_moe(t203, t204, t205, t206, t207, t208, t209);
  t211 = ((int64_t)(t210));
  t212 = ((int64_t)(((int64_t)UINT64_C(0))));
  t202 = ((t211) != (t212));
  }
  if (!(t202)) goto label14;
  {
  t213 = ((int64_t)(v0));
  t214 = ((int64_t)(((int64_t)UINT64_C(2))));
  t215 = ((int64_t)(v1));
  t216 = ((int64_t)(v2));
  t217 = ((int64_t)(v3));
  t218 = ((int64_t)(v15));
  t219 = ((int64_t)(v13));
  t220 = fg107_moe(t213, t214, t215, t216, t217, t218, t219);
  v16 = ((int64_t)(t220));
  t221 = ((int64_t)(v16));
  t222 = ((int64_t)(((int64_t)UINT64_C(0))));
  t223 = ((t221) == (t222));
  if (t223) {
  {
  t224 = v15;
  t225 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t224) + (uint64_t)(t225)));
  }
  } else {
  {
  t226 = ((int64_t)(v14));
  t227 = ((int64_t)(v15));
  t228 = ((t226) < (t227));
  if (t228) {
  {
  t229 = ((int64_t)(v1));
  t230 = ((int64_t)(v3));
  t231 = ((int64_t)(v2));
  t232 = ((int64_t)(((int64_t)UINT64_C(7))));
  t233 = ((int64_t)(((int64_t)UINT64_C(0))));
  t234 = fg107_tensor(t231, t232, t233);
  t235 = ((int64_t)(t234));
  t236 = ((int64_t)(v14));
  t237 = ((int64_t)(v8));
  t238 = ((int64_t)(((int64_t)((uint64_t)(t236) * (uint64_t)(t237)))));
  t239 = ((int64_t)(((int64_t)((uint64_t)(t235) + (uint64_t)(t238)))));
  t240 = ((int64_t)(v14));
  t241 = ((int64_t)(v8));
  t242 = ((int64_t)(((int64_t)((uint64_t)(t240) * (uint64_t)(t241)))));
  t243 = ((int64_t)(v15));
  t244 = ((int64_t)(v14));
  t245 = ((int64_t)(((int64_t)((uint64_t)(t243) - (uint64_t)(t244)))));
  t246 = ((int64_t)(v8));
  t247 = ((int64_t)(((int64_t)((uint64_t)(t245) * (uint64_t)(t246)))));
  t248 = fg107_tensor_set_async(t229, t230, t239, t242, t247);
  (void)(t248);
  }
  }
  t249 = v15;
  t250 = ((int64_t)(v16));
  v15 = ((int64_t)((uint64_t)(t249) + (uint64_t)(t250)));
  v14 = ((int64_t)(v15));
  }
  }
  }
  goto label13;
label14:;
  t251 = ((int64_t)(v14));
  t252 = ((int64_t)(v8));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t251) * (uint64_t)(t252)))));
  t253 = ((int64_t)(v13));
  t254 = ((int64_t)(v7));
  t255 = ((int64_t)(((int64_t)UINT64_C(1))));
  t256 = ((int64_t)(((int64_t)((uint64_t)(t254) - (uint64_t)(t255)))));
  t257 = ((t253) < (t256));
  if (t257) {
  {
  t258 = ((int64_t)(v8));
  t259 = ((int64_t)(((int64_t)UINT64_C(512))));
  t260 = ((t258) < (t259));
  if (t260) {
  {
  v18 = ((int64_t)(v8));
  }
  } else {
  {
  v18 = ((int64_t)(((int64_t)UINT64_C(512))));
  }
  }
  }
  } else {
  {
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t261 = ((int64_t)(v13));
  t262 = ((int64_t)(((int64_t)UINT64_C(1))));
  t263 = ((int64_t)(((int64_t)((uint64_t)(t261) + (uint64_t)(t262)))));
  t264 = ((int64_t)(v14));
  t265 = ((int64_t)(((int64_t)((uint64_t)(t263) - (uint64_t)(t264)))));
  t266 = ((int64_t)(v8));
  t267 = ((int64_t)(((int64_t)((uint64_t)(t265) * (uint64_t)(t266)))));
  t268 = ((int64_t)(v18));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t267) + (uint64_t)(t268)))));
  t269 = ((int64_t)(v19));
  t270 = ((int64_t)(((int64_t)UINT64_C(0))));
  t271 = ((t269) > (t270));
  if (t271) {
  {
  t272 = ((int64_t)(v1));
  t273 = ((int64_t)(v3));
  t274 = ((int64_t)(v2));
  t275 = ((int64_t)(((int64_t)UINT64_C(7))));
  t276 = ((int64_t)(((int64_t)UINT64_C(0))));
  t277 = fg107_tensor(t274, t275, t276);
  t278 = ((int64_t)(t277));
  t279 = ((int64_t)(v17));
  t280 = ((int64_t)(((int64_t)((uint64_t)(t278) + (uint64_t)(t279)))));
  t281 = ((int64_t)(v17));
  t282 = ((int64_t)(v19));
  t283 = fg107_tensor_set_async(t272, t273, t280, t281, t282);
  (void)(t283);
  }
  }
  t284 = ((int64_t)(v13));
  t285 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t284) + (uint64_t)(t285)))));
  }
  }
  }
  goto label7;
label8:;
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_38(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
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
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v4));
  t6 = gg_function_39(t1, t2, t3, t4, t5, base + 0);
  t7 = t6;
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_36(int64_t p0, int64_t base) {
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
  uint8_t t67;
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
  uint8_t t94;
  uint8_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  uint8_t t102;
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
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  uint8_t t127;
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
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
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
  int64_t t246;
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
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  uint8_t t264;
  uint8_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  int64_t t275;
  uint8_t t276;
  uint8_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  uint8_t t284;
  int64_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
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
  int64_t t301;
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
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  uint8_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
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
  int64_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  int64_t t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
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
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(6))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg107_ctx(t0, t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(7))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg107_ctx(t4, t5, t6);
  v2 = ((int64_t)(t7));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg107_ctx(t8, t9, t10);
  v3 = ((int64_t)(t11));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(5))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg107_exec(t12, t13, t14, t15, t16);
  v4 = ((int64_t)(t17));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t18 = ((int64_t)(v5));
  t19 = ((int64_t)(v4));
  if (!(((t18) < (t19)))) goto label2;
  {
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(6))));
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = fg107_exec(t20, t21, t22, t23, t24);
  v6 = ((int64_t)(t25));
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(6))));
  t28 = ((int64_t)(v5));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg107_exec(t26, t27, t28, t29, t30);
  v7 = ((int64_t)(t31));
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = fg107_out(t32, t33, t34, t35, t36);
  t38 = ((int64_t)(t37));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) > (t39));
  if (t40) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t41 = ((int64_t)(v8));
  t42 = ((int64_t)(v1));
  if (!(((t41) < (t42)))) goto label5;
  {
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((int64_t)(v6));
  t47 = ((int64_t)(v1));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)(v8));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  t51 = ((int64_t)(v7));
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t54 = ((int64_t)(v8));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t54)))));
  t56 = fg107_out_view_swap(t43, t44, t45, t50, t55);
  (void)(t56);
  t57 = v8;
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t57) + (uint64_t)(t58)));
  }
  goto label4;
label5:;
  }
  }
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(6))));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = fg107_out(t59, t60, t61, t62, t63);
  t65 = ((int64_t)(t64));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((t65) > (t66));
  if (t67) {
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t68 = ((int64_t)(v9));
  t69 = ((int64_t)(v3));
  if (!(((t68) < (t69)))) goto label8;
  {
  t70 = ((int64_t)(v0));
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((int64_t)(v6));
  t74 = ((int64_t)(v3));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))));
  t76 = ((int64_t)(v9));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t76)))));
  t78 = ((int64_t)(v7));
  t79 = ((int64_t)(v3));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  t81 = ((int64_t)(v9));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t81)))));
  t83 = fg107_out_view_swap(t70, t71, t72, t77, t82);
  (void)(t83);
  t84 = v9;
  t85 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t84) + (uint64_t)(t85)));
  }
  goto label7;
label8:;
  }
  }
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(8))));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = fg107_out(t86, t87, t88, t89, t90);
  t92 = ((int64_t)(t91));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((t92) > (t93));
  t95 = t94;
  if (t95) {
  t96 = ((int64_t)(v0));
  t97 = ((int64_t)(((int64_t)UINT64_C(12))));
  t98 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = fg107_ctx(t96, t97, t98);
  t100 = ((int64_t)(t99));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((t100) != (t101));
  }
  t102 = t95;
  if (t102) {
  {
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t103 = ((int64_t)(v10));
  t104 = ((int64_t)(v3));
  if (!(((t103) < (t104)))) goto label11;
  {
  t105 = ((int64_t)(v0));
  t106 = ((int64_t)(((int64_t)UINT64_C(2))));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = ((int64_t)(v6));
  t109 = ((int64_t)(v3));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = ((int64_t)(v10));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  t113 = ((int64_t)(v7));
  t114 = ((int64_t)(v3));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t113) * (uint64_t)(t114)))));
  t116 = ((int64_t)(v10));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = fg107_out_view_swap(t105, t106, t107, t112, t117);
  (void)(t118);
  t119 = v10;
  t120 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t119) + (uint64_t)(t120)));
  }
  goto label10;
label11:;
  }
  }
  t121 = ((int64_t)(v0));
  t122 = ((int64_t)(((int64_t)UINT64_C(23))));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = fg107_ctx(t121, t122, t123);
  t125 = ((int64_t)(t124));
  t126 = ((int64_t)(((int64_t)UINT64_C(0))));
  t127 = ((t125) != (t126));
  if (t127) {
  {
  t128 = ((int64_t)(v0));
  t129 = ((int64_t)(((int64_t)UINT64_C(19))));
  t130 = ((int64_t)(((int64_t)UINT64_C(4))));
  t131 = ((int64_t)(((int64_t)UINT64_C(0))));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t133 = fg107_out(t128, t129, t130, t131, t132);
  t134 = ((int64_t)(t133));
  t135 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t134) > (t135)))) gg_fail("assertion failed");
  t136 = ((int64_t)(v0));
  t137 = ((int64_t)(((int64_t)UINT64_C(19))));
  t138 = ((int64_t)(((int64_t)UINT64_C(5))));
  t139 = ((int64_t)(((int64_t)UINT64_C(0))));
  t140 = ((int64_t)(((int64_t)UINT64_C(0))));
  t141 = fg107_out(t136, t137, t138, t139, t140);
  t142 = ((int64_t)(t141));
  t143 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t142) > (t143)))) gg_fail("assertion failed");
  t144 = ((int64_t)(v0));
  t145 = ((int64_t)(((int64_t)UINT64_C(19))));
  t146 = ((int64_t)(((int64_t)UINT64_C(6))));
  t147 = ((int64_t)(((int64_t)UINT64_C(0))));
  t148 = ((int64_t)(((int64_t)UINT64_C(0))));
  t149 = fg107_out(t144, t145, t146, t147, t148);
  t150 = ((int64_t)(t149));
  t151 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t150) > (t151)))) gg_fail("assertion failed");
  t152 = ((int64_t)(v0));
  t153 = ((int64_t)(((int64_t)UINT64_C(19))));
  t154 = ((int64_t)(((int64_t)UINT64_C(7))));
  t155 = ((int64_t)(((int64_t)UINT64_C(0))));
  t156 = ((int64_t)(((int64_t)UINT64_C(0))));
  t157 = fg107_out(t152, t153, t154, t155, t156);
  t158 = ((int64_t)(t157));
  t159 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t158) > (t159)))) gg_fail("assertion failed");
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t160 = ((int64_t)(v11));
  t161 = ((int64_t)(v1));
  if (!(((t160) < (t161)))) goto label14;
  {
  t162 = ((int64_t)(v0));
  t163 = ((int64_t)(((int64_t)UINT64_C(4))));
  t164 = ((int64_t)(((int64_t)UINT64_C(0))));
  t165 = ((int64_t)(v6));
  t166 = ((int64_t)(v1));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t165) * (uint64_t)(t166)))));
  t168 = ((int64_t)(v11));
  t169 = ((int64_t)(((int64_t)((uint64_t)(t167) + (uint64_t)(t168)))));
  t170 = ((int64_t)(v7));
  t171 = ((int64_t)(v1));
  t172 = ((int64_t)(((int64_t)((uint64_t)(t170) * (uint64_t)(t171)))));
  t173 = ((int64_t)(v11));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t172) + (uint64_t)(t173)))));
  t175 = fg107_out_view_swap(t162, t163, t164, t169, t174);
  (void)(t175);
  t176 = v11;
  t177 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t176) + (uint64_t)(t177)));
  }
  goto label13;
label14:;
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t178 = ((int64_t)(v12));
  t179 = ((int64_t)(v1));
  if (!(((t178) < (t179)))) goto label17;
  {
  t180 = ((int64_t)(v0));
  t181 = ((int64_t)(((int64_t)UINT64_C(5))));
  t182 = ((int64_t)(((int64_t)UINT64_C(0))));
  t183 = ((int64_t)(v6));
  t184 = ((int64_t)(v1));
  t185 = ((int64_t)(((int64_t)((uint64_t)(t183) * (uint64_t)(t184)))));
  t186 = ((int64_t)(v12));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t185) + (uint64_t)(t186)))));
  t188 = ((int64_t)(v7));
  t189 = ((int64_t)(v1));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t188) * (uint64_t)(t189)))));
  t191 = ((int64_t)(v12));
  t192 = ((int64_t)(((int64_t)((uint64_t)(t190) + (uint64_t)(t191)))));
  t193 = fg107_out_view_swap(t180, t181, t182, t187, t192);
  (void)(t193);
  t194 = v12;
  t195 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t194) + (uint64_t)(t195)));
  }
  goto label16;
label17:;
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t196 = ((int64_t)(v13));
  t197 = ((int64_t)(v1));
  if (!(((t196) < (t197)))) goto label20;
  {
  t198 = ((int64_t)(v0));
  t199 = ((int64_t)(((int64_t)UINT64_C(6))));
  t200 = ((int64_t)(((int64_t)UINT64_C(0))));
  t201 = ((int64_t)(v6));
  t202 = ((int64_t)(v1));
  t203 = ((int64_t)(((int64_t)((uint64_t)(t201) * (uint64_t)(t202)))));
  t204 = ((int64_t)(v13));
  t205 = ((int64_t)(((int64_t)((uint64_t)(t203) + (uint64_t)(t204)))));
  t206 = ((int64_t)(v7));
  t207 = ((int64_t)(v1));
  t208 = ((int64_t)(((int64_t)((uint64_t)(t206) * (uint64_t)(t207)))));
  t209 = ((int64_t)(v13));
  t210 = ((int64_t)(((int64_t)((uint64_t)(t208) + (uint64_t)(t209)))));
  t211 = fg107_out_view_swap(t198, t199, t200, t205, t210);
  (void)(t211);
  t212 = v13;
  t213 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)((uint64_t)(t212) + (uint64_t)(t213)));
  }
  goto label19;
label20:;
  t214 = ((int64_t)(v0));
  t215 = ((int64_t)(((int64_t)UINT64_C(23))));
  t216 = ((int64_t)(((int64_t)UINT64_C(0))));
  t217 = ((int64_t)(v6));
  t218 = ((int64_t)(v7));
  t219 = fg107_out(t214, t215, t216, t217, t218);
  (void)(t219);
  t220 = ((int64_t)(v0));
  t221 = ((int64_t)(((int64_t)UINT64_C(23))));
  t222 = ((int64_t)(((int64_t)UINT64_C(1))));
  t223 = ((int64_t)(v6));
  t224 = ((int64_t)(v7));
  t225 = fg107_out(t220, t221, t222, t223, t224);
  (void)(t225);
  t226 = ((int64_t)(v0));
  t227 = ((int64_t)(((int64_t)UINT64_C(23))));
  t228 = ((int64_t)(((int64_t)UINT64_C(2))));
  t229 = ((int64_t)(v6));
  t230 = ((int64_t)(v7));
  t231 = fg107_out(t226, t227, t228, t229, t230);
  (void)(t231);
  t232 = ((int64_t)(v0));
  t233 = ((int64_t)(((int64_t)UINT64_C(23))));
  t234 = ((int64_t)(((int64_t)UINT64_C(3))));
  t235 = ((int64_t)(v6));
  t236 = ((int64_t)(v7));
  t237 = fg107_out(t232, t233, t234, t235, t236);
  (void)(t237);
  }
  }
  t238 = v5;
  t239 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t238) + (uint64_t)(t239)));
  }
  goto label1;
label2:;
  t240 = ((int64_t)(v0));
  t241 = ((int64_t)(((int64_t)UINT64_C(8))));
  t242 = ((int64_t)(((int64_t)UINT64_C(0))));
  t243 = ((int64_t)(((int64_t)UINT64_C(0))));
  t244 = ((int64_t)(((int64_t)UINT64_C(0))));
  t245 = fg107_exec(t240, t241, t242, t243, t244);
  (void)(t245);
  t246 = ((int64_t)(v0));
  t247 = ((int64_t)(((int64_t)UINT64_C(9))));
  t248 = ((int64_t)(((int64_t)UINT64_C(0))));
  t249 = ((int64_t)(((int64_t)UINT64_C(0))));
  t250 = ((int64_t)(((int64_t)UINT64_C(0))));
  t251 = fg107_exec(t246, t247, t248, t249, t250);
  v14 = ((int64_t)(t251));
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t252 = ((int64_t)(v15));
  t253 = ((int64_t)(v14));
  if (!(((t252) < (t253)))) goto label23;
  {
  t254 = ((int64_t)(v0));
  t255 = ((int64_t)(((int64_t)UINT64_C(10))));
  t256 = ((int64_t)(v15));
  t257 = ((int64_t)(((int64_t)UINT64_C(0))));
  t258 = ((int64_t)(((int64_t)UINT64_C(0))));
  t259 = fg107_exec(t254, t255, t256, t257, t258);
  v16 = ((int64_t)(t259));
label25:;
  t260 = ((int64_t)(v16));
  t261 = ((int64_t)(v15));
  if (!(((t260) != (t261)))) goto label26;
  {
  v17 = ((int64_t)(v16));
  t262 = ((int64_t)(v17));
  t263 = ((int64_t)(((int64_t)UINT64_C(0))));
  t264 = ((t262) >= (t263));
  t265 = t264;
  if (t265) {
  t266 = ((int64_t)(v17));
  t267 = ((int64_t)(v14));
  t265 = ((t266) < (t267));
  }
  if (!(t265)) gg_fail("assertion failed");
  t268 = ((int64_t)(v0));
  t269 = ((int64_t)(((int64_t)UINT64_C(8))));
  t270 = ((int64_t)(((int64_t)UINT64_C(0))));
  t271 = ((int64_t)(((int64_t)UINT64_C(0))));
  t272 = ((int64_t)(((int64_t)UINT64_C(0))));
  t273 = fg107_out(t268, t269, t270, t271, t272);
  t274 = ((int64_t)(t273));
  t275 = ((int64_t)(((int64_t)UINT64_C(0))));
  t276 = ((t274) > (t275));
  t277 = t276;
  if (t277) {
  t278 = ((int64_t)(v0));
  t279 = ((int64_t)(((int64_t)UINT64_C(12))));
  t280 = ((int64_t)(((int64_t)UINT64_C(0))));
  t281 = fg107_ctx(t278, t279, t280);
  t282 = ((int64_t)(t281));
  t283 = ((int64_t)(((int64_t)UINT64_C(0))));
  t277 = ((t282) == (t283));
  }
  t284 = t277;
  if (t284) {
  {
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
label28:;
  t285 = ((int64_t)(v18));
  t286 = ((int64_t)(v3));
  if (!(((t285) < (t286)))) goto label29;
  {
  t287 = ((int64_t)(v0));
  t288 = ((int64_t)(((int64_t)UINT64_C(2))));
  t289 = ((int64_t)(((int64_t)UINT64_C(0))));
  t290 = ((int64_t)(v15));
  t291 = ((int64_t)(v3));
  t292 = ((int64_t)(((int64_t)((uint64_t)(t290) * (uint64_t)(t291)))));
  t293 = ((int64_t)(v18));
  t294 = ((int64_t)(((int64_t)((uint64_t)(t292) + (uint64_t)(t293)))));
  t295 = ((int64_t)(v17));
  t296 = ((int64_t)(v3));
  t297 = ((int64_t)(((int64_t)((uint64_t)(t295) * (uint64_t)(t296)))));
  t298 = ((int64_t)(v18));
  t299 = ((int64_t)(((int64_t)((uint64_t)(t297) + (uint64_t)(t298)))));
  t300 = fg107_out_view_swap(t287, t288, t289, t294, t299);
  (void)(t300);
  t301 = v18;
  t302 = ((int64_t)(((int64_t)UINT64_C(1))));
  v18 = ((int64_t)((uint64_t)(t301) + (uint64_t)(t302)));
  }
  goto label28;
label29:;
  }
  }
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
label31:;
  t303 = ((int64_t)(v19));
  t304 = ((int64_t)(v0));
  t305 = ((int64_t)(((int64_t)UINT64_C(24))));
  t306 = ((int64_t)(((int64_t)UINT64_C(0))));
  t307 = fg107_ctx(t304, t305, t306);
  t308 = ((int64_t)(t307));
  if (!(((t303) < (t308)))) goto label32;
  {
  t309 = ((int64_t)(v0));
  t310 = ((int64_t)(((int64_t)UINT64_C(11))));
  t311 = ((int64_t)(v19));
  t312 = ((int64_t)(((int64_t)UINT64_C(0))));
  t313 = ((int64_t)(((int64_t)UINT64_C(0))));
  t314 = fg107_out(t309, t310, t311, t312, t313);
  t315 = ((int64_t)(t314));
  t316 = ((int64_t)(((int64_t)UINT64_C(0))));
  t317 = ((t315) != (t316));
  if (t317) {
  {
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
label34:;
  t318 = ((int64_t)(v20));
  t319 = ((int64_t)(v2));
  if (!(((t318) < (t319)))) goto label35;
  {
  t320 = ((int64_t)(v0));
  t321 = ((int64_t)(((int64_t)UINT64_C(3))));
  t322 = ((int64_t)(v19));
  t323 = ((int64_t)(v15));
  t324 = ((int64_t)(v2));
  t325 = ((int64_t)(((int64_t)((uint64_t)(t323) * (uint64_t)(t324)))));
  t326 = ((int64_t)(v20));
  t327 = ((int64_t)(((int64_t)((uint64_t)(t325) + (uint64_t)(t326)))));
  t328 = ((int64_t)(v17));
  t329 = ((int64_t)(v2));
  t330 = ((int64_t)(((int64_t)((uint64_t)(t328) * (uint64_t)(t329)))));
  t331 = ((int64_t)(v20));
  t332 = ((int64_t)(((int64_t)((uint64_t)(t330) + (uint64_t)(t331)))));
  t333 = fg107_out_view_swap(t320, t321, t322, t327, t332);
  (void)(t333);
  t334 = v20;
  t335 = ((int64_t)(((int64_t)UINT64_C(1))));
  v20 = ((int64_t)((uint64_t)(t334) + (uint64_t)(t335)));
  }
  goto label34;
label35:;
  }
  }
  t336 = v19;
  t337 = ((int64_t)(((int64_t)UINT64_C(1))));
  v19 = ((int64_t)((uint64_t)(t336) + (uint64_t)(t337)));
  }
  goto label31;
label32:;
  t338 = ((int64_t)(v0));
  t339 = ((int64_t)(((int64_t)UINT64_C(13))));
  t340 = ((int64_t)(v15));
  t341 = ((int64_t)(v17));
  t342 = ((int64_t)(((int64_t)UINT64_C(0))));
  t343 = fg107_exec(t338, t339, t340, t341, t342);
  (void)(t343);
  t344 = ((int64_t)(v0));
  t345 = ((int64_t)(((int64_t)UINT64_C(10))));
  t346 = ((int64_t)(v15));
  t347 = ((int64_t)(((int64_t)UINT64_C(0))));
  t348 = ((int64_t)(((int64_t)UINT64_C(0))));
  t349 = fg107_exec(t344, t345, t346, t347, t348);
  v16 = ((int64_t)(t349));
  }
  goto label25;
label26:;
  t350 = v15;
  t351 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t350) + (uint64_t)(t351)));
  }
  goto label22;
label23:;
  t352 = ((int64_t)(v0));
  t353 = ((int64_t)(((int64_t)UINT64_C(12))));
  t354 = ((int64_t)(((int64_t)UINT64_C(0))));
  t355 = ((int64_t)(((int64_t)UINT64_C(0))));
  t356 = ((int64_t)(((int64_t)UINT64_C(0))));
  t357 = fg107_exec(t352, t353, t354, t355, t356);
  (void)(t357);
  }
gg_done:;
}
static void gg_function_35(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  gg_function_36(t0, base + 0);
  }
gg_done:;
}
static void gg_function_33(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
  int64_t t0;
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
  uint8_t t85;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(18))));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg107_out(t0, t1, t2, t3, t4);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) == (t7));
  if (t8) {
  {
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)UINT64_C(1));
  t12 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t11)))));
  t13 = fg107_res_sampling_tensor(t9, t10, t12);
  v6 = ((int64_t)(t13));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(22))));
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = fg107_out(t14, t15, t16, t17, t18);
  v7 = ((int64_t)(t19));
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t20 = ((int64_t)(v8));
  t21 = ((int64_t)(v6));
  if (!(((t20) < (t21)))) goto label2;
  {
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(v8));
  t25 = fg107_res_sampling_tensor(t22, t23, t24);
  v9 = ((int64_t)(t25));
  t26 = ((int64_t)(v9));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t26) != (t27));
  if (t28) {
  {
  t29 = ((int64_t)(v4));
  t30 = ((int64_t)(v8));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t31 = ((int64_t)(v9));
  t32 = ((int64_t)(((int64_t)UINT64_C(4))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = fg107_tensor(t31, t32, t33);
  v11 = ((int64_t)(t34));
  t35 = ((int64_t)(v9));
  t36 = ((int64_t)(((int64_t)UINT64_C(6))));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = fg107_tensor(t35, t36, t37);
  t39 = ((int64_t)(t38));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t39) != (t40)))) gg_fail("assertion failed");
  t41 = ((int64_t)(v11));
  t42 = ((int64_t)(v3));
  if (!(((t41) <= (t42)))) gg_fail("assertion failed");
  t43 = ((int64_t)(v10));
  t44 = ((int64_t)(v3));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((int64_t)(v11));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(19))));
  t50 = ((int64_t)(v2));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = fg107_out(t48, t49, t50, t51, t52);
  t54 = ((int64_t)(t53));
  if (!(((t47) <= (t54)))) gg_fail("assertion failed");
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(4))));
  t57 = ((int64_t)(v9));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = fg107_sched(t55, t56, t57, t58, t59);
  v12 = ((int64_t)(t60));
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(18))));
  t63 = ((int64_t)(v2));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = fg107_out(t61, t62, t63, t64, t65);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(v10));
  t69 = ((int64_t)(v3));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t69)))));
  t71 = ((int64_t)(v7));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t72)))));
  t73 = ((int64_t)(v12));
  t74 = ((int64_t)(v9));
  t75 = ((int64_t)(v13));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = ((int64_t)(v9));
  t78 = ((int64_t)(((int64_t)UINT64_C(5))));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = fg107_tensor(t77, t78, t79);
  t81 = ((int64_t)(t80));
  t82 = fg107_tensor_get_async(t73, t74, t75, t76, t81);
  (void)(t82);
  t83 = ((int64_t)(v5));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((t83) != (t84));
  if (t85) {
  {
  t86 = ((int64_t)(v10));
  t87 = ((int64_t)(v0));
  t88 = ((int64_t)(((int64_t)UINT64_C(20))));
  t89 = ((int64_t)(v2));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = fg107_out(t87, t88, t89, t90, t91);
  t93 = ((int64_t)(t92));
  if (!(((t86) < (t93)))) gg_fail("assertion failed");
  t94 = ((int64_t)(v0));
  t95 = ((int64_t)(((int64_t)UINT64_C(21))));
  t96 = ((int64_t)(v2));
  t97 = ((int64_t)(v10));
  t98 = ((int64_t)(v11));
  t99 = fg107_out(t94, t95, t96, t97, t98);
  (void)(t99);
  }
  }
  }
  }
  t100 = v8;
  t101 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t100) + (uint64_t)(t101)));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static uint8_t gg_function_31(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t13;
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
  uint8_t t30;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint8_t v4;
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
  v3 = p3;
  {
  v4 = ((uint8_t)UINT64_C(0));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v5));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(24))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg107_ctx(t2, t3, t4);
  t6 = ((int64_t)(t5));
  if (!(((t1) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(35))));
  t9 = ((int64_t)(v5));
  t10 = fg107_ctx(t7, t8, t9);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) != (t12));
  if (t13) {
  {
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(11))));
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = fg107_out(t14, t15, t16, t17, t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(((int64_t)UINT64_C(8))));
  t26 = ((int64_t)(v5));
  t27 = fg107_res(t23, t24, t25, t26);
  v6 = ((int64_t)(t27));
  t28 = ((int64_t)(v6));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  t31 = ((int64_t)(v6));
  t32 = ((int64_t)(((int64_t)UINT64_C(5))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = fg107_tensor(t31, t32, t33);
  v7 = ((int64_t)(t34));
  t35 = ((int64_t)(v7));
  t36 = ((int64_t)(((int64_t)UINT64_C(4))));
  v8 = ((int64_t)(((int64_t)gg_div(t35, t36))));
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t37) > (t38)))) gg_fail("assertion failed");
  t39 = ((int64_t)(v8));
  t40 = ((int64_t)(v3));
  t41 = ((int64_t)(((int64_t)gg_mod(t39, t40))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t41) == (t42)))) gg_fail("assertion failed");
  t43 = ((int64_t)(v8));
  t44 = ((int64_t)(v3));
  v9 = ((int64_t)(((int64_t)gg_div(t43, t44))));
  t45 = ((int64_t)(v9));
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(7))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = fg107_ctx(t46, t47, t48);
  t50 = ((int64_t)(t49));
  if (!(((t45) == (t50)))) gg_fail("assertion failed");
  t51 = ((int64_t)(v2));
  t52 = ((int64_t)(v9));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t53 = ((int64_t)(v10));
  t54 = ((int64_t)(v8));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t54)))));
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(((int64_t)UINT64_C(10))));
  t58 = ((int64_t)(v5));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = fg107_out(t56, t57, t58, t59, t60);
  t62 = ((int64_t)(t61));
  if (!(((t55) <= (t62)))) gg_fail("assertion failed");
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(((int64_t)UINT64_C(4))));
  t65 = ((int64_t)(v6));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = fg107_sched(t63, t64, t65, t66, t67);
  v11 = ((int64_t)(t68));
  t69 = ((int64_t)(v11));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t69) != (t70)))) gg_fail("assertion failed");
  t71 = ((int64_t)(v11));
  t72 = ((int64_t)(v6));
  t73 = ((int64_t)(v0));
  t74 = ((int64_t)(((int64_t)UINT64_C(9))));
  t75 = ((int64_t)(v5));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = fg107_out(t73, t74, t75, t76, t77);
  t79 = ((int64_t)(t78));
  t80 = ((int64_t)(v10));
  t81 = ((int64_t)(((int64_t)UINT64_C(4))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t82)))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((int64_t)(v7));
  t86 = fg107_tensor_get_async(t71, t72, t83, t84, t85);
  (void)(t86);
  v4 = ((uint8_t)UINT64_C(1));
  }
  }
  t87 = v5;
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  }
  goto label1;
label2:;
  t0 = v4;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg107_ub(t2, t3, t4, t5);
  t7 = ((int64_t)(t6));
  if (!(((t1) < (t7)))) goto label2;
  {
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg107_ub(t8, t9, t10, t11);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) != (t14));
  if (t15) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(2))));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg107_ub(t17, t18, t19, t20);
  t22 = ((int64_t)(t21));
  if (!(((t16) < (t22)))) goto label5;
  {
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(3))));
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(v3));
  t27 = fg107_ub(t23, t24, t25, t26);
  v4 = ((int64_t)(t27));
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(36))));
  t30 = ((int64_t)(v4));
  t31 = fg107_ctx(t28, t29, t30);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((t32) == (t33));
  if (t34) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t35 = v3;
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  }
  goto label4;
label5:;
  }
  }
  t37 = v2;
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t37) + (uint64_t)(t38)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_28(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
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
  t2 = ((int64_t)(((int64_t)UINT64_C(11))));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg107_sched(t1, t2, t3, t4, t5);
  v3 = ((int64_t)(t6));
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(12))));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg107_sched(t7, t8, t9, t10, t11);
  v4 = ((int64_t)(t12));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(9))));
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg107_sched(t13, t14, t15, t16, t17);
  (void)(t18);
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(10))));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg107_sched(t19, t20, t21, t22, t23);
  (void)(t24);
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(8))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = fg107_sched(t25, t26, t27, t28, t29);
  (void)(t30);
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(6))));
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg107_sched(t31, t32, t33, t34, t35);
  v5 = ((int64_t)(t36));
  t37 = ((int64_t)(v5));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t37) != (t38));
  if (t39) {
  {
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = fg107_log(t40, t41, t42, t43);
  (void)(t44);
  }
  }
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
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
  uint8_t t102;
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
  int64_t t120;
  uint8_t t121;
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
  uint8_t t138;
  uint8_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
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
  uint8_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  uint8_t t196;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg107_mctx(t5, t6, t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t9) == (t10));
  }
  t11 = t4;
  if (t11) {
  {
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg107_log(t12, t13, t14, t15);
  (void)(t16);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = ((int64_t)UINT64_C(1));
  t20 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t19)))));
  t21 = fg107_ctx_set(t17, t18, t20);
  (void)(t21);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = fg107_res_prev(t22);
  v4 = ((int64_t)(t23));
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = fg107_res(t24, t25, t26, t27);
  v5 = ((int64_t)(t28));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(v3));
  t33 = ((int64_t)(v2));
  t34 = fg107_graph_params(t29, t30, t31, t32, t33);
  v6 = ((int64_t)(t34));
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(16))));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = fg107_ctx(t35, t36, t37);
  t39 = ((int64_t)(t38));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((t39) == (t40));
  t42 = t41;
  if (t42) {
  t43 = ((int64_t)(v0));
  t44 = fg107_res_prev_active(t43);
  t45 = ((int64_t)(t44));
  t46 = ((int64_t)(v4));
  t42 = ((t45) == (t46));
  }
  t47 = t42;
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(v4));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  t52 = ((int64_t)(v6));
  t53 = fg107_res(t49, t50, t51, t52);
  t54 = ((int64_t)(t53));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((t54) != (t55));
  }
  t56 = t48;
  if (t56) {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t57 = ((int64_t)(v7));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((t57) != (t58));
  if (t59) {
  {
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(15))));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = fg107_ctx(t60, t61, t62);
  t64 = ((int64_t)(t63));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((t64) != (t65));
  if (t66) {
  {
  t67 = ((int64_t)(v0));
  t68 = ((int64_t)(((int64_t)UINT64_C(3))));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = fg107_sched(t67, t68, t69, t70, t71);
  (void)(t72);
  }
  }
  t73 = ((int64_t)(v0));
  t74 = ((int64_t)(((int64_t)UINT64_C(1))));
  t75 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = fg107_ctx_set(t73, t74, t75);
  (void)(t76);
  }
  } else {
  {
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = fg107_res_set_prev_active(t77, t78);
  (void)(t79);
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(v4));
  t82 = ((int64_t)(((int64_t)UINT64_C(2))));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = fg107_res(t80, t81, t82, t83);
  (void)(t84);
  t85 = ((int64_t)(v0));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = fg107_sched(t85, t86, t87, t88, t89);
  (void)(t90);
  t91 = ((int64_t)(v0));
  t92 = ((int64_t)(((int64_t)UINT64_C(1))));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = fg107_sched(t91, t92, t93, t94, t95);
  (void)(t96);
  t97 = ((int64_t)(v0));
  t98 = ((int64_t)(v6));
  t99 = fg107_model_build_graph(t97, t98);
  v5 = ((int64_t)(t99));
  t100 = ((int64_t)(v5));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t100) == (t101));
  if (t102) {
  {
  t103 = ((int64_t)(v0));
  t104 = ((int64_t)(((int64_t)UINT64_C(3))));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = fg107_log(t103, t104, t105, t106);
  (void)(t107);
  t108 = ((int64_t)(v0));
  t109 = ((int64_t)(((int64_t)UINT64_C(4))));
  t110 = ((int64_t)UINT64_C(1));
  t111 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t110)))));
  t112 = fg107_ctx_set(t108, t109, t111);
  (void)(t112);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t113 = ((int64_t)(v0));
  t114 = ((int64_t)(((int64_t)UINT64_C(2))));
  t115 = ((int64_t)(v5));
  t116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t118 = fg107_sched(t113, t114, t115, t116, t117);
  t119 = ((int64_t)(t118));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = ((t119) == (t120));
  if (t121) {
  {
  t122 = ((int64_t)(v0));
  t123 = ((int64_t)(((int64_t)UINT64_C(4))));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = fg107_log(t122, t123, t124, t125);
  (void)(t126);
  t127 = ((int64_t)(v0));
  t128 = ((int64_t)(((int64_t)UINT64_C(4))));
  t129 = ((int64_t)UINT64_C(2));
  t130 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t129)))));
  t131 = fg107_ctx_set(t127, t128, t130);
  (void)(t131);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t132 = ((int64_t)(v0));
  t133 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = fg107_ctx(t132, t133, t134);
  t136 = ((int64_t)(t135));
  t137 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = ((t136) != (t137));
  t139 = t138;
  if (t139) {
  t140 = ((int64_t)(v0));
  t141 = ((int64_t)(((int64_t)UINT64_C(30))));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = fg107_ctx(t140, t141, t142);
  t144 = ((int64_t)(t143));
  t145 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((t144) != (t145));
  }
  t146 = t139;
  if (t146) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t147 = ((int64_t)(v0));
  t148 = ((int64_t)(((int64_t)UINT64_C(5))));
  t149 = ((int64_t)(((int64_t)UINT64_C(0))));
  t150 = ((int64_t)(((int64_t)UINT64_C(0))));
  t151 = ((int64_t)(((int64_t)UINT64_C(0))));
  t152 = fg107_sched(t147, t148, t149, t150, t151);
  v9 = ((int64_t)(t152));
label1:;
  t153 = ((int64_t)(v8));
  t154 = ((int64_t)(v9));
  if (!(((t153) < (t154)))) goto label2;
  {
  t155 = ((int64_t)(v0));
  t156 = ((int64_t)(((int64_t)UINT64_C(15))));
  t157 = ((int64_t)(v8));
  t158 = ((int64_t)(((int64_t)UINT64_C(0))));
  t159 = ((int64_t)(((int64_t)UINT64_C(0))));
  t160 = fg107_sched(t155, t156, t157, t158, t159);
  v10 = ((int64_t)(t160));
  t161 = ((int64_t)(v0));
  t162 = ((int64_t)(((int64_t)UINT64_C(14))));
  t163 = ((int64_t)(v10));
  t164 = ((int64_t)(((int64_t)UINT64_C(0))));
  t165 = ((int64_t)(((int64_t)UINT64_C(0))));
  t166 = fg107_sched(t161, t162, t163, t164, t165);
  (void)(t166);
  t167 = v8;
  t168 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t167) + (uint64_t)(t168)));
  }
  goto label1;
label2:;
  }
  }
  t169 = ((int64_t)(v0));
  t170 = ((int64_t)(v4));
  t171 = fg107_res_set_prev_active(t169, t170);
  (void)(t171);
  }
  }
  t172 = ((int64_t)(v0));
  t173 = ((int64_t)(v4));
  t174 = ((int64_t)(((int64_t)UINT64_C(3))));
  t175 = ((int64_t)(v1));
  t176 = fg107_res(t172, t173, t174, t175);
  (void)(t176);
  t177 = ((int64_t)(v0));
  t178 = ((int64_t)(v0));
  t179 = ((int64_t)(v4));
  t180 = ((int64_t)(((int64_t)UINT64_C(0))));
  t181 = ((int64_t)(((int64_t)UINT64_C(0))));
  t182 = fg107_res(t178, t179, t180, t181);
  t183 = ((int64_t)(t182));
  t185 = ((int64_t)(v1));
  t186 = ((int64_t)(((int64_t)UINT64_C(0))));
  t187 = ((int64_t)(((int64_t)UINT64_C(0))));
  t188 = ((int64_t)(((int64_t)UINT64_C(0))));
  t189 = fg107_ub(t185, t186, t187, t188);
  t190 = ((int64_t)(t189));
  t191 = ((int64_t)(((int64_t)UINT64_C(1))));
  t192 = ((t190) > (t191));
  if (t192) {
  {
  t184 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t184 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t193 = ((int64_t)(t184));
  v11 = gg_function_28(t177, t183, t193, base + 0);
  t194 = ((int64_t)(v11));
  t195 = ((int64_t)(((int64_t)UINT64_C(0))));
  t196 = ((t194) != (t195));
  if (t196) {
  {
  t197 = ((int64_t)(v0));
  t198 = ((int64_t)(((int64_t)UINT64_C(5))));
  t199 = ((int64_t)(v11));
  t200 = ((int64_t)(((int64_t)UINT64_C(0))));
  t201 = fg107_log(t197, t198, t199, t200);
  (void)(t201);
  t202 = ((int64_t)(v0));
  t203 = ((int64_t)(((int64_t)UINT64_C(4))));
  t204 = ((int64_t)(v11));
  t205 = fg107_ctx_set(t202, t203, t204);
  (void)(t205);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t206 = ((int64_t)(v0));
  t207 = ((int64_t)(((int64_t)UINT64_C(4))));
  t208 = ((int64_t)(((int64_t)UINT64_C(0))));
  t209 = fg107_ctx_set(t206, t207, t208);
  (void)(t209);
  t0 = ((int64_t)(v4));
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
  uint8_t t7;
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
  uint8_t t30;
  uint8_t t31;
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
  uint8_t t53;
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
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint8_t t75;
  uint8_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  uint8_t t83;
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
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
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
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint8_t t124;
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
  uint8_t t153;
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
  uint8_t t201;
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
  uint8_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
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
  int64_t t246;
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
  int64_t t257;
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
  int64_t t268;
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
  int64_t t279;
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
  int64_t t290;
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
  int64_t t301;
  int64_t t302;
  int64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(5))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg107_ctx(t1, t2, t3);
  v2 = ((int64_t)(t4));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v2));
  t7 = ((t5) > (t6));
  if (t7) {
  {
  v3 = ((int64_t)(v1));
  }
  } else {
  {
  v3 = ((int64_t)(v2));
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(3))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg107_ctx(t8, t9, t10);
  v4 = ((int64_t)(t11));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(6))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = fg107_ctx(t12, t13, t14);
  v5 = ((int64_t)(t15));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(7))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = fg107_ctx(t16, t17, t18);
  v6 = ((int64_t)(t19));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(8))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg107_ctx(t20, t21, t22);
  v7 = ((int64_t)(t23));
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(10))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg107_ctx(t24, t25, t26);
  t28 = ((int64_t)(t27));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) == (t29));
  t31 = t30;
  if (!t31) {
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(20))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg107_ctx(t32, t33, t34);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t36) != (t37));
  }
  t38 = t31;
  if (t38) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(10))));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = fg107_ctx(t39, t40, t41);
  v9 = ((int64_t)(t42));
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(11))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = fg107_ctx(t43, t44, t45);
  v10 = ((int64_t)(t46));
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(19))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = fg107_ctx(t47, t48, t49);
  t51 = ((int64_t)(t50));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((t51) != (t52));
  if (t53) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(((int64_t)UINT64_C(23))));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = fg107_ctx(t54, t55, t56);
  v11 = ((int64_t)(t57));
  t58 = ((int64_t)(v8));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((t58) != (t59));
  if (t60) {
  {
  t61 = ((int64_t)(v5));
  t62 = ((int64_t)(v3));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t61) * (uint64_t)(t62)))));
  }
  } else {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t63 = ((int64_t)(v9));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((t63) != (t64));
  if (t65) {
  {
  t66 = ((int64_t)(v7));
  t67 = ((int64_t)(v3));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  }
  } else {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t68 = ((int64_t)(v10));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = ((t68) != (t69));
  if (t70) {
  {
  t71 = ((int64_t)(v7));
  t72 = ((int64_t)(v3));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t71) * (uint64_t)(t72)))));
  }
  } else {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t73 = ((int64_t)(v10));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((t73) != (t74));
  t76 = t75;
  if (t76) {
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(12))));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = fg107_ctx(t77, t78, t79);
  t81 = ((int64_t)(t80));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = ((t81) == (t82));
  }
  t83 = t76;
  if (t83) {
  {
  t84 = ((int64_t)(v7));
  t85 = ((int64_t)(v4));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))));
  }
  }
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t86 = ((int64_t)(v16));
  t87 = ((int64_t)(v0));
  t88 = ((int64_t)(((int64_t)UINT64_C(24))));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = fg107_ctx(t87, t88, t89);
  t91 = ((int64_t)(t90));
  if (!(((t86) < (t91)))) goto label2;
  {
  t92 = ((int64_t)(v0));
  t93 = ((int64_t)(((int64_t)UINT64_C(35))));
  t94 = ((int64_t)(v16));
  t95 = fg107_ctx(t92, t93, t94);
  t96 = ((int64_t)(t95));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((t96) != (t97));
  if (t98) {
  {
  t99 = v15;
  t100 = ((int64_t)(v6));
  t101 = ((int64_t)(v4));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t100) * (uint64_t)(t101)))));
  v15 = ((int64_t)((uint64_t)(t99) + (uint64_t)(t102)));
  }
  }
  t103 = v16;
  t104 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)((uint64_t)(t103) + (uint64_t)(t104)));
  }
  goto label1;
label2:;
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t105 = ((int64_t)(v11));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = ((t105) != (t106));
  if (t107) {
  {
  t108 = ((int64_t)(((int64_t)UINT64_C(2))));
  t109 = ((int64_t)(v5));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = ((int64_t)(v3));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t110) * (uint64_t)(t111)))));
  t112 = ((int64_t)(((int64_t)UINT64_C(1))));
  t113 = ((int64_t)(v5));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) + (uint64_t)(t113)))));
  t115 = ((int64_t)(v3));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t114) * (uint64_t)(t115)))));
  }
  }
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(((int64_t)UINT64_C(1))));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = fg107_exec(t116, t117, t118, t119, t120);
  t122 = ((int64_t)(t121));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = ((t122) == (t123));
  if (t124) {
  {
  t125 = ((int64_t)(v0));
  t126 = ((int64_t)(((int64_t)UINT64_C(0))));
  t127 = ((int64_t)(v4));
  t128 = ((int64_t)(((int64_t)UINT64_C(0))));
  t129 = ((int64_t)(((int64_t)UINT64_C(0))));
  t130 = fg107_exec(t125, t126, t127, t128, t129);
  (void)(t130);
  }
  }
  t131 = ((int64_t)(v12));
  t132 = ((int64_t)(v13));
  t133 = ((int64_t)(((int64_t)((uint64_t)(t131) + (uint64_t)(t132)))));
  t134 = ((int64_t)(v14));
  t135 = ((int64_t)(((int64_t)((uint64_t)(t133) + (uint64_t)(t134)))));
  t136 = ((int64_t)(v15));
  t137 = ((int64_t)(((int64_t)((uint64_t)(t135) + (uint64_t)(t136)))));
  t138 = ((int64_t)(v17));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t137) + (uint64_t)(t138)))));
  t140 = ((int64_t)(((int64_t)UINT64_C(4))));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t139) * (uint64_t)(t140)))));
  t142 = ((int64_t)(v18));
  t143 = ((int64_t)(((int64_t)UINT64_C(4))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t142) * (uint64_t)(t143)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t141) + (uint64_t)(t144)))));
  t145 = ((int64_t)(v0));
  t146 = ((int64_t)(((int64_t)UINT64_C(1))));
  t147 = ((int64_t)(v19));
  t148 = ((int64_t)(((int64_t)UINT64_C(0))));
  t149 = ((int64_t)(((int64_t)UINT64_C(0))));
  t150 = fg107_out(t145, t146, t147, t148, t149);
  t151 = ((int64_t)(t150));
  t152 = ((int64_t)(((int64_t)UINT64_C(0))));
  t153 = ((t151) == (t152));
  if (t153) {
  {
  t154 = ((int64_t)(v0));
  t155 = ((int64_t)(((int64_t)UINT64_C(6))));
  t156 = ((int64_t)(v19));
  t157 = ((int64_t)(((int64_t)UINT64_C(0))));
  t158 = fg107_log(t154, t155, t156, t157);
  (void)(t158);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t159 = ((int64_t)(v0));
  t160 = ((int64_t)(((int64_t)UINT64_C(0))));
  t161 = ((int64_t)(((int64_t)UINT64_C(0))));
  t162 = ((int64_t)(v20));
  t163 = ((int64_t)(v12));
  t164 = fg107_out_bind(t159, t160, t161, t162, t163);
  (void)(t164);
  t165 = v20;
  t166 = ((int64_t)(v12));
  t167 = ((int64_t)(((int64_t)UINT64_C(4))));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))));
  v20 = ((int64_t)((uint64_t)(t165) + (uint64_t)(t168)));
  t169 = ((int64_t)(v0));
  t170 = ((int64_t)(((int64_t)UINT64_C(1))));
  t171 = ((int64_t)(((int64_t)UINT64_C(0))));
  t172 = ((int64_t)(v20));
  t173 = ((int64_t)(v13));
  t174 = fg107_out_bind(t169, t170, t171, t172, t173);
  (void)(t174);
  t175 = v20;
  t176 = ((int64_t)(v13));
  t177 = ((int64_t)(((int64_t)UINT64_C(4))));
  t178 = ((int64_t)(((int64_t)((uint64_t)(t176) * (uint64_t)(t177)))));
  v20 = ((int64_t)((uint64_t)(t175) + (uint64_t)(t178)));
  t179 = ((int64_t)(v0));
  t180 = ((int64_t)(((int64_t)UINT64_C(2))));
  t181 = ((int64_t)(((int64_t)UINT64_C(0))));
  t182 = ((int64_t)(v20));
  t183 = ((int64_t)(v14));
  t184 = fg107_out_bind(t179, t180, t181, t182, t183);
  (void)(t184);
  t185 = v20;
  t186 = ((int64_t)(v14));
  t187 = ((int64_t)(((int64_t)UINT64_C(4))));
  t188 = ((int64_t)(((int64_t)((uint64_t)(t186) * (uint64_t)(t187)))));
  v20 = ((int64_t)((uint64_t)(t185) + (uint64_t)(t188)));
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t189 = ((int64_t)(v16));
  t190 = ((int64_t)(v0));
  t191 = ((int64_t)(((int64_t)UINT64_C(24))));
  t192 = ((int64_t)(((int64_t)UINT64_C(0))));
  t193 = fg107_ctx(t190, t191, t192);
  t194 = ((int64_t)(t193));
  if (!(((t189) < (t194)))) goto label5;
  {
  t195 = ((int64_t)(v0));
  t196 = ((int64_t)(((int64_t)UINT64_C(35))));
  t197 = ((int64_t)(v16));
  t198 = fg107_ctx(t195, t196, t197);
  t199 = ((int64_t)(t198));
  t200 = ((int64_t)(((int64_t)UINT64_C(0))));
  t201 = ((t199) != (t200));
  if (t201) {
  {
  t202 = ((int64_t)(v0));
  t203 = ((int64_t)(((int64_t)UINT64_C(3))));
  t204 = ((int64_t)(v16));
  t205 = ((int64_t)(v20));
  t206 = ((int64_t)(v6));
  t207 = ((int64_t)(v4));
  t208 = ((int64_t)(((int64_t)((uint64_t)(t206) * (uint64_t)(t207)))));
  t209 = fg107_out_bind(t202, t203, t204, t205, t208);
  (void)(t209);
  t210 = v20;
  t211 = ((int64_t)(v6));
  t212 = ((int64_t)(v4));
  t213 = ((int64_t)(((int64_t)((uint64_t)(t211) * (uint64_t)(t212)))));
  t214 = ((int64_t)(((int64_t)UINT64_C(4))));
  t215 = ((int64_t)(((int64_t)((uint64_t)(t213) * (uint64_t)(t214)))));
  v20 = ((int64_t)((uint64_t)(t210) + (uint64_t)(t215)));
  }
  } else {
  {
  t216 = ((int64_t)(v0));
  t217 = ((int64_t)(((int64_t)UINT64_C(3))));
  t218 = ((int64_t)(v16));
  t219 = ((int64_t)(((int64_t)UINT64_C(0))));
  t220 = ((int64_t)(((int64_t)UINT64_C(0))));
  t221 = fg107_out_bind(t216, t217, t218, t219, t220);
  (void)(t221);
  }
  }
  t222 = v16;
  t223 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)((uint64_t)(t222) + (uint64_t)(t223)));
  }
  goto label4;
label5:;
  t224 = ((int64_t)(v11));
  t225 = ((int64_t)(((int64_t)UINT64_C(0))));
  t226 = ((t224) != (t225));
  if (t226) {
  {
  t227 = ((int64_t)(v0));
  t228 = ((int64_t)(((int64_t)UINT64_C(4))));
  t229 = ((int64_t)(((int64_t)UINT64_C(0))));
  t230 = ((int64_t)(v20));
  t231 = ((int64_t)(v5));
  t232 = ((int64_t)(v3));
  t233 = ((int64_t)(((int64_t)((uint64_t)(t231) * (uint64_t)(t232)))));
  t234 = fg107_out_bind(t227, t228, t229, t230, t233);
  (void)(t234);
  t235 = v20;
  t236 = ((int64_t)(v5));
  t237 = ((int64_t)(v3));
  t238 = ((int64_t)(((int64_t)((uint64_t)(t236) * (uint64_t)(t237)))));
  t239 = ((int64_t)(((int64_t)UINT64_C(4))));
  t240 = ((int64_t)(((int64_t)((uint64_t)(t238) * (uint64_t)(t239)))));
  v20 = ((int64_t)((uint64_t)(t235) + (uint64_t)(t240)));
  t241 = ((int64_t)(v0));
  t242 = ((int64_t)(((int64_t)UINT64_C(5))));
  t243 = ((int64_t)(((int64_t)UINT64_C(0))));
  t244 = ((int64_t)(v20));
  t245 = ((int64_t)(v5));
  t246 = ((int64_t)(v3));
  t247 = ((int64_t)(((int64_t)((uint64_t)(t245) * (uint64_t)(t246)))));
  t248 = fg107_out_bind(t241, t242, t243, t244, t247);
  (void)(t248);
  t249 = v20;
  t250 = ((int64_t)(v5));
  t251 = ((int64_t)(v3));
  t252 = ((int64_t)(((int64_t)((uint64_t)(t250) * (uint64_t)(t251)))));
  t253 = ((int64_t)(((int64_t)UINT64_C(4))));
  t254 = ((int64_t)(((int64_t)((uint64_t)(t252) * (uint64_t)(t253)))));
  v20 = ((int64_t)((uint64_t)(t249) + (uint64_t)(t254)));
  t255 = ((int64_t)(v0));
  t256 = ((int64_t)(((int64_t)UINT64_C(6))));
  t257 = ((int64_t)(((int64_t)UINT64_C(0))));
  t258 = ((int64_t)(v20));
  t259 = ((int64_t)(v3));
  t260 = fg107_out_bind(t255, t256, t257, t258, t259);
  (void)(t260);
  t261 = v20;
  t262 = ((int64_t)(v3));
  t263 = ((int64_t)(((int64_t)UINT64_C(4))));
  t264 = ((int64_t)(((int64_t)((uint64_t)(t262) * (uint64_t)(t263)))));
  v20 = ((int64_t)((uint64_t)(t261) + (uint64_t)(t264)));
  t265 = ((int64_t)(v0));
  t266 = ((int64_t)(((int64_t)UINT64_C(7))));
  t267 = ((int64_t)(((int64_t)UINT64_C(0))));
  t268 = ((int64_t)(v20));
  t269 = ((int64_t)(v5));
  t270 = ((int64_t)(v3));
  t271 = ((int64_t)(((int64_t)((uint64_t)(t269) * (uint64_t)(t270)))));
  t272 = fg107_out_bind(t265, t266, t267, t268, t271);
  (void)(t272);
  t273 = v20;
  t274 = ((int64_t)(v5));
  t275 = ((int64_t)(v3));
  t276 = ((int64_t)(((int64_t)((uint64_t)(t274) * (uint64_t)(t275)))));
  t277 = ((int64_t)(((int64_t)UINT64_C(4))));
  t278 = ((int64_t)(((int64_t)((uint64_t)(t276) * (uint64_t)(t277)))));
  v20 = ((int64_t)((uint64_t)(t273) + (uint64_t)(t278)));
  t279 = ((int64_t)(v0));
  t280 = ((int64_t)(((int64_t)UINT64_C(16))));
  t281 = ((int64_t)(v3));
  t282 = ((int64_t)(((int64_t)UINT64_C(0))));
  t283 = ((int64_t)(((int64_t)UINT64_C(0))));
  t284 = fg107_out(t279, t280, t281, t282, t283);
  (void)(t284);
  }
  } else {
  {
  t285 = ((int64_t)(v0));
  t286 = ((int64_t)(((int64_t)UINT64_C(17))));
  t287 = ((int64_t)(((int64_t)UINT64_C(0))));
  t288 = ((int64_t)(((int64_t)UINT64_C(0))));
  t289 = ((int64_t)(((int64_t)UINT64_C(0))));
  t290 = fg107_out(t285, t286, t287, t288, t289);
  (void)(t290);
  }
  }
  t291 = ((int64_t)(v0));
  t292 = ((int64_t)(((int64_t)UINT64_C(4))));
  t293 = ((int64_t)UINT64_C(1));
  t294 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t293)))));
  t295 = ((int64_t)(((int64_t)UINT64_C(0))));
  t296 = ((int64_t)(((int64_t)UINT64_C(0))));
  t297 = fg107_exec(t291, t292, t294, t295, t296);
  (void)(t297);
  t298 = ((int64_t)(v0));
  t299 = ((int64_t)(((int64_t)UINT64_C(0))));
  t300 = ((int64_t)(((int64_t)UINT64_C(0))));
  t301 = fg107_ctx_set(t298, t299, t300);
  (void)(t301);
  t302 = ((int64_t)(v3));
  t303 = ((int64_t)(v0));
  t304 = ((int64_t)(((int64_t)UINT64_C(21))));
  t305 = ((int64_t)(((int64_t)UINT64_C(0))));
  t306 = fg107_ctx(t303, t304, t305);
  t307 = ((int64_t)(t306));
  if (!(((t302) <= (t307)))) gg_fail("assertion failed");
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_13(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
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
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
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
  uint8_t t44;
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
  uint8_t t58;
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
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg107_ctx(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg107_mem(t8, t9, t10, t11, t12);
  v2 = ((int64_t)(t13));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg107_mctx(t14, t15, t16);
  v3 = ((int64_t)(t17));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((t18) == (t19));
  if (t20) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(((int64_t)UINT64_C(2))));
  t23 = ((t21) == (t22));
  t24 = t23;
  if (!t24) {
  t25 = ((int64_t)(v3));
  t26 = ((int64_t)(((int64_t)UINT64_C(3))));
  t24 = ((t25) == (t26));
  }
  t27 = t24;
  if (t27) {
  {
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(7))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = fg107_log(t28, t29, t30, t31);
  (void)(t32);
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t33 = ((int64_t)(v0));
  t34 = fg107_res_reset_all_prev(t33);
  (void)(t34);
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = fg107_res_set_prev_active(t35, t36);
  (void)(t37);
  t38 = ((int64_t)(v2));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = fg107_mctx(t38, t39, t40);
  t42 = ((int64_t)(t41));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = ((t42) == (t43));
  if (t44) {
  {
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(8))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = fg107_log(t45, t46, t47, t48);
  (void)(t49);
  }
  }
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = fg107_mem(t50, t51, t52, t53, t54);
  v4 = ((int64_t)(t55));
  t56 = ((int64_t)(v4));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((t56) == (t57));
  if (t58) {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(5))));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = fg107_ctx(t59, t60, t61);
  v5 = ((int64_t)(t62));
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(((int64_t)UINT64_C(2))));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = fg107_ctx(t63, t64, t65);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(4))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = fg107_ctx(t68, t69, t70);
  t72 = ((int64_t)(t71));
  t73 = ((t67) < (t72));
  if (t73) {
  {
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(2))));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = fg107_ctx(t74, t75, t76);
  v6 = ((int64_t)(t77));
  }
  } else {
  {
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(((int64_t)UINT64_C(4))));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = fg107_ctx(t78, t79, t80);
  v6 = ((int64_t)(t81));
  }
  }
  t82 = ((int64_t)(v6));
  t83 = ((int64_t)(v0));
  t84 = ((int64_t)(((int64_t)UINT64_C(21))));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = fg107_ctx(t83, t84, t85);
  t87 = ((int64_t)(t86));
  t88 = ((t82) < (t87));
  if (t88) {
  {
  v7 = ((int64_t)(v6));
  }
  } else {
  {
  t89 = ((int64_t)(v0));
  t90 = ((int64_t)(((int64_t)UINT64_C(21))));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = fg107_ctx(t89, t90, t91);
  v7 = ((int64_t)(t92));
  }
  }
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(((int64_t)UINT64_C(30))));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = fg107_ctx(t93, t94, t95);
  v8 = ((int64_t)(t96));
  t97 = ((int64_t)(v0));
  t98 = ((int64_t)(v6));
  t99 = ((int64_t)(v5));
  t100 = ((int64_t)(v7));
  t101 = ((int64_t)(v4));
  t102 = ((int64_t)(v8));
  t103 = fg107_graph_reserve(t97, t98, t99, t100, t101, t102);
  v9 = ((int64_t)(t103));
  t104 = ((int64_t)(v9));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((t104) == (t105));
  if (t106) {
  {
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(((int64_t)UINT64_C(9))));
  t109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = fg107_log(t107, t108, t109, t110);
  (void)(t111);
  }
  }
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_9(int64_t p0, int64_t base) {
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
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  uint8_t t74;
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
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint8_t t112;
  uint8_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg107_ctx(t0, t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(3))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg107_sched(t7, t8, t9, t10, t11);
  (void)(t12);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg107_perf(t13, t14, t15, t16);
  v1 = ((int64_t)(t17));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((t18) == (t19));
  if (t20) {
  {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(9))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = fg107_perf(t21, t22, t23, t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((int64_t)(((int64_t)UINT64_C(4))));
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((int64_t)(((int64_t)UINT64_C(4))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg107_perf(t32, t33, t34, t35);
  t37 = ((int64_t)(t36));
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(33))));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = fg107_ctx(t38, t39, t40);
  t42 = ((int64_t)(t41));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t42)))));
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((int64_t)(((int64_t)UINT64_C(2))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = fg107_perf(t44, t45, t46, t47);
  t49 = ((int64_t)(t48));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t43) - (uint64_t)(t49)))));
  t51 = fg107_perf(t29, t30, t31, t50);
  (void)(t51);
  }
  }
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  t54 = ((int64_t)(((int64_t)UINT64_C(3))));
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((int64_t)(((int64_t)UINT64_C(3))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = fg107_perf(t55, t56, t57, t58);
  t60 = ((int64_t)(t59));
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  t63 = fg107_perf(t52, t53, t54, t62);
  (void)(t63);
  }
  } else {
  t64 = ((int64_t)(v1));
  t65 = ((int64_t)(((int64_t)UINT64_C(1))));
  t66 = ((t64) > (t65));
  if (t66) {
  {
  t67 = ((int64_t)(v0));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((int64_t)(((int64_t)UINT64_C(9))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = fg107_perf(t67, t68, t69, t70);
  t72 = ((int64_t)(t71));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((t72) == (t73));
  if (t74) {
  {
  t75 = ((int64_t)(v0));
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  t77 = ((int64_t)(((int64_t)UINT64_C(6))));
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = ((int64_t)(((int64_t)UINT64_C(6))));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = fg107_perf(t78, t79, t80, t81);
  t83 = ((int64_t)(t82));
  t84 = ((int64_t)(v0));
  t85 = ((int64_t)(((int64_t)UINT64_C(33))));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = fg107_ctx(t84, t85, t86);
  t88 = ((int64_t)(t87));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t83) + (uint64_t)(t88)))));
  t90 = ((int64_t)(v0));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((int64_t)(((int64_t)UINT64_C(2))));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = fg107_perf(t90, t91, t92, t93);
  t95 = ((int64_t)(t94));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t89) - (uint64_t)(t95)))));
  t97 = fg107_perf(t75, t76, t77, t96);
  (void)(t97);
  }
  }
  t98 = ((int64_t)(v0));
  t99 = ((int64_t)(((int64_t)UINT64_C(1))));
  t100 = ((int64_t)(((int64_t)UINT64_C(5))));
  t101 = ((int64_t)(v0));
  t102 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = ((int64_t)(((int64_t)UINT64_C(5))));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t105 = fg107_perf(t101, t102, t103, t104);
  t106 = ((int64_t)(t105));
  t107 = ((int64_t)(v1));
  t108 = ((int64_t)(((int64_t)((uint64_t)(t106) + (uint64_t)(t107)))));
  t109 = fg107_perf(t98, t99, t100, t108);
  (void)(t109);
  }
  }
  }
  t110 = ((int64_t)(v1));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = ((t110) > (t111));
  t113 = t112;
  if (t113) {
  t114 = ((int64_t)(v0));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((int64_t)(((int64_t)UINT64_C(7))));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t118 = fg107_perf(t114, t115, t116, t117);
  t119 = ((int64_t)(t118));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = ((t119) == (t120));
  }
  t121 = t113;
  if (t121) {
  {
  t122 = ((int64_t)(v0));
  t123 = ((int64_t)(((int64_t)UINT64_C(1))));
  t124 = ((int64_t)(((int64_t)UINT64_C(8))));
  t125 = ((int64_t)(v0));
  t126 = ((int64_t)(((int64_t)UINT64_C(33))));
  t127 = ((int64_t)(((int64_t)UINT64_C(0))));
  t128 = fg107_ctx(t125, t126, t127);
  t129 = ((int64_t)(t128));
  t130 = ((int64_t)(v0));
  t131 = ((int64_t)(((int64_t)UINT64_C(0))));
  t132 = ((int64_t)(((int64_t)UINT64_C(1))));
  t133 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = fg107_perf(t130, t131, t132, t133);
  t135 = ((int64_t)(t134));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t129) - (uint64_t)(t135)))));
  t137 = fg107_perf(t122, t123, t124, t136);
  (void)(t137);
  t138 = ((int64_t)(v0));
  t139 = ((int64_t)(((int64_t)UINT64_C(1))));
  t140 = ((int64_t)(((int64_t)UINT64_C(7))));
  t141 = ((int64_t)(((int64_t)UINT64_C(1))));
  t142 = fg107_perf(t138, t139, t140, t141);
  (void)(t142);
  }
  }
  t143 = ((int64_t)(v0));
  t144 = ((int64_t)(((int64_t)UINT64_C(1))));
  t145 = ((int64_t)(((int64_t)UINT64_C(0))));
  t146 = ((int64_t)(((int64_t)UINT64_C(0))));
  t147 = fg107_perf(t143, t144, t145, t146);
  (void)(t147);
  t148 = ((int64_t)(v0));
  t149 = ((int64_t)(((int64_t)UINT64_C(1))));
  t150 = ((int64_t)(((int64_t)UINT64_C(2))));
  t151 = ((int64_t)(((int64_t)UINT64_C(0))));
  t152 = fg107_perf(t148, t149, t150, t151);
  (void)(t152);
  }
gg_done:;
}
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
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
  uint8_t t44;
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
  uint8_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
  uint8_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint8_t t78;
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
  uint8_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint8_t t114;
  uint8_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  uint8_t t123;
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
  uint8_t t138;
  uint8_t t139;
  int64_t t140;
  int64_t t141;
  uint8_t t142;
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
  uint8_t t153;
  uint8_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
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
  uint8_t t179;
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
  uint8_t t198;
  uint8_t t199;
  int64_t t200;
  int64_t t201;
  uint8_t t202;
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
  uint8_t t221;
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
  int64_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  uint8_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  uint8_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
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
  int64_t t268;
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
  int64_t t279;
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
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  uint8_t t294;
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
  uint8_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  uint8_t t317;
  int64_t t318;
  int64_t t319;
  uint8_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  uint8_t t329;
  int64_t t330;
  int64_t t331;
  uint8_t t332;
  int64_t t333;
  int64_t t334;
  uint8_t t335;
  uint8_t t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  int64_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  int64_t t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
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
  int64_t t365;
  int64_t t366;
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
  int64_t t377;
  int64_t t378;
  int64_t t379;
  int64_t t380;
  int64_t t381;
  int64_t t382;
  uint8_t t383;
  int64_t t384;
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
  int64_t t395;
  int64_t t396;
  int64_t t397;
  int64_t t398;
  int64_t t399;
  int64_t t400;
  int64_t t401;
  int64_t t402;
  int64_t t403;
  int64_t t404;
  int64_t t405;
  int64_t t406;
  uint8_t t407;
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
  uint8_t t433;
  int64_t t434;
  int64_t t435;
  int64_t t436;
  int64_t t437;
  int64_t t438;
  int64_t t439;
  int64_t t440;
  int64_t t441;
  int64_t t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  int64_t t446;
  int64_t t447;
  int64_t t448;
  int64_t t449;
  int64_t t450;
  int64_t t451;
  int64_t t452;
  uint8_t t453;
  uint8_t t454;
  int64_t t455;
  int64_t t456;
  int64_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  int64_t t461;
  uint8_t t462;
  int64_t t463;
  int64_t t464;
  int64_t t465;
  int64_t t466;
  int64_t t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
  int64_t t472;
  int64_t t473;
  int64_t t474;
  int64_t t475;
  uint8_t t476;
  int64_t t477;
  int64_t t478;
  int64_t t479;
  int64_t t480;
  int64_t t481;
  int64_t t482;
  int64_t t483;
  int64_t t484;
  int64_t t485;
  int64_t t486;
  int64_t t487;
  int64_t t488;
  int64_t t489;
  int64_t t490;
  int64_t t491;
  int64_t t492;
  int64_t t493;
  int64_t t494;
  int64_t t495;
  int64_t t496;
  uint8_t t497;
  int64_t t498;
  int64_t t499;
  int64_t t500;
  uint8_t t501;
  int64_t t502;
  int64_t t503;
  int64_t t504;
  int64_t t505;
  uint8_t t506;
  int64_t t507;
  int64_t t508;
  int64_t t509;
  int64_t t510;
  int64_t t511;
  int64_t t512;
  int64_t t513;
  int64_t t514;
  int64_t t515;
  int64_t t516;
  int64_t t517;
  int64_t t518;
  uint8_t t519;
  int64_t t520;
  int64_t t521;
  int64_t t522;
  int64_t t523;
  int64_t t524;
  int64_t t525;
  int64_t t526;
  int64_t t527;
  int64_t t528;
  int64_t t529;
  int64_t t530;
  uint8_t t531;
  int64_t t532;
  int64_t t533;
  int64_t t534;
  int64_t t535;
  int64_t t536;
  int64_t t537;
  int64_t t538;
  uint8_t t539;
  uint8_t t540;
  int64_t t541;
  int64_t t542;
  int64_t t543;
  int64_t t544;
  int64_t t545;
  int64_t t546;
  int64_t t547;
  uint8_t t548;
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
  int64_t t563;
  int64_t t564;
  int64_t t565;
  uint8_t t566;
  uint8_t t567;
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
  int64_t t578;
  int64_t t579;
  int64_t t580;
  uint8_t t581;
  int64_t t582;
  int64_t t583;
  int64_t t584;
  int64_t t585;
  int64_t t586;
  int64_t t587;
  int64_t t588;
  int64_t t589;
  int64_t t590;
  int64_t t591;
  int64_t t592;
  int64_t t593;
  int64_t t594;
  int64_t t595;
  int64_t t596;
  int64_t t597;
  int64_t t598;
  int64_t t599;
  int64_t t600;
  int64_t t601;
  int64_t t602;
  int64_t t603;
  int64_t t604;
  int64_t t605;
  int64_t t606;
  int64_t t607;
  int64_t t608;
  int64_t t609;
  int64_t t610;
  int64_t t611;
  int64_t t612;
  int64_t t613;
  int64_t t614;
  int64_t t615;
  int64_t t616;
  int64_t t617;
  int64_t t618;
  int64_t t619;
  int64_t t620;
  int64_t t621;
  int64_t t622;
  int64_t t623;
  int64_t t624;
  int64_t t625;
  int64_t t626;
  int64_t t627;
  int64_t t628;
  int64_t t629;
  int64_t t630;
  int64_t t631;
  int64_t t632;
  int64_t t633;
  int64_t t634;
  int64_t t635;
  int64_t t636;
  int64_t t637;
  int64_t t638;
  int64_t t639;
  int64_t t640;
  int64_t t641;
  int64_t t642;
  int64_t t643;
  int64_t t644;
  int64_t t645;
  int64_t t646;
  int64_t t647;
  uint8_t t648;
  uint8_t t649;
  int64_t t650;
  int64_t t651;
  uint8_t t652;
  uint8_t t653;
  int64_t t654;
  int64_t t655;
  uint8_t t656;
  int64_t t657;
  int64_t t658;
  int64_t t659;
  int64_t t660;
  int64_t t661;
  int64_t t662;
  int64_t t663;
  int64_t t664;
  int64_t t665;
  int64_t t666;
  int64_t t667;
  int64_t t668;
  int64_t t669;
  int64_t t670;
  uint8_t t671;
  int64_t t672;
  int64_t t673;
  int64_t t674;
  int64_t t675;
  int64_t t676;
  int64_t t677;
  int64_t t678;
  int64_t t679;
  int64_t t680;
  int64_t t681;
  int64_t t682;
  int64_t t683;
  int64_t t684;
  int64_t t685;
  int64_t t686;
  int64_t t687;
  int64_t t688;
  int64_t t689;
  int64_t t690;
  int64_t t691;
  int64_t t692;
  int64_t t693;
  int64_t t694;
  int64_t t695;
  int64_t t696;
  int64_t t697;
  int64_t t698;
  int64_t t699;
  int64_t t700;
  int64_t t701;
  int64_t t702;
  int64_t t703;
  int64_t t704;
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
  uint8_t t716;
  uint8_t t717;
  int64_t t718;
  int64_t t719;
  uint8_t t720;
  uint8_t t721;
  int64_t t722;
  int64_t t723;
  uint8_t t724;
  int64_t t725;
  int64_t t726;
  int64_t t727;
  int64_t t728;
  int64_t t729;
  int64_t t730;
  int64_t t731;
  int64_t t732;
  int64_t t733;
  int64_t t734;
  int64_t t735;
  int64_t t736;
  int64_t t737;
  int64_t t738;
  int64_t t739;
  int64_t t740;
  int64_t t741;
  int64_t t742;
  int64_t t743;
  int64_t t744;
  int64_t t745;
  int64_t t746;
  int64_t t747;
  int64_t t748;
  int64_t t749;
  int64_t t750;
  int64_t t751;
  int64_t t752;
  int64_t t753;
  int64_t t754;
  int64_t t755;
  int64_t t756;
  int64_t t757;
  int64_t t758;
  int64_t t759;
  int64_t t760;
  int64_t t761;
  int64_t t762;
  int64_t t763;
  int64_t t764;
  int64_t t765;
  int64_t t766;
  int64_t t767;
  int64_t t768;
  int64_t t769;
  int64_t t770;
  int64_t t771;
  int64_t t772;
  int64_t t773;
  int64_t t774;
  int64_t t775;
  uint8_t t776;
  int64_t t777;
  int64_t t778;
  int64_t t779;
  int64_t t780;
  int64_t t781;
  int64_t t782;
  int64_t t783;
  int64_t t784;
  int64_t t785;
  int64_t t786;
  int64_t t787;
  int64_t t788;
  int64_t t789;
  int64_t t790;
  int64_t t791;
  int64_t t792;
  int64_t t793;
  int64_t t794;
  int64_t t795;
  int64_t t796;
  int64_t t797;
  int64_t t798;
  int64_t t799;
  int64_t t800;
  int64_t t801;
  int64_t t802;
  int64_t t803;
  int64_t t804;
  int64_t t805;
  int64_t t806;
  int64_t t807;
  int64_t t808;
  int64_t t809;
  int64_t t810;
  int64_t t811;
  int64_t t812;
  int64_t t813;
  int64_t t814;
  int64_t t815;
  int64_t t816;
  int64_t t817;
  int64_t t818;
  int64_t t819;
  int64_t t820;
  int64_t t821;
  int64_t t822;
  int64_t t823;
  int64_t t824;
  int64_t t825;
  int64_t t826;
  int64_t t827;
  int64_t t828;
  int64_t t829;
  uint8_t t830;
  uint8_t t831;
  int64_t t832;
  int64_t t833;
  int64_t t834;
  int64_t t835;
  int64_t t836;
  int64_t t837;
  uint8_t t838;
  int64_t t839;
  int64_t t840;
  int64_t t841;
  int64_t t842;
  int64_t t843;
  int64_t t844;
  uint8_t t845;
  int64_t t846;
  int64_t t847;
  int64_t t848;
  int64_t t849;
  int64_t t850;
  int64_t t851;
  int64_t t852;
  int64_t t853;
  uint8_t t854;
  uint8_t t855;
  int64_t t856;
  int64_t t857;
  uint8_t t858;
  uint8_t t859;
  int64_t t860;
  int64_t t861;
  uint8_t t862;
  uint8_t t863;
  int64_t t864;
  int64_t t865;
  int64_t t866;
  int64_t t867;
  int64_t t868;
  int64_t t869;
  uint8_t t870;
  int64_t t871;
  int64_t t872;
  int64_t t873;
  int64_t t874;
  int64_t t875;
  int64_t t876;
  int64_t t877;
  int64_t t878;
  int64_t t879;
  int64_t t880;
  int64_t t881;
  int64_t t882;
  int64_t t883;
  int64_t t884;
  int64_t t885;
  int64_t t886;
  int64_t t887;
  int64_t t888;
  int64_t t889;
  int64_t t890;
  int64_t t891;
  int64_t t892;
  int64_t t893;
  int64_t t894;
  int64_t t895;
  int64_t t896;
  int64_t t897;
  int64_t t898;
  int64_t t899;
  int64_t t900;
  int64_t t901;
  int64_t t902;
  int64_t t903;
  int64_t t904;
  int64_t t905;
  int64_t t906;
  int64_t t907;
  int64_t t908;
  int64_t t909;
  int64_t t910;
  int64_t t911;
  int64_t t912;
  int64_t t913;
  int64_t t914;
  int64_t t915;
  int64_t t916;
  int64_t t917;
  int64_t t918;
  uint8_t t919;
  int64_t t920;
  int64_t t921;
  uint8_t t922;
  int64_t t923;
  int64_t t924;
  int64_t t925;
  int64_t t926;
  int64_t t927;
  int64_t t928;
  int64_t t929;
  int64_t t930;
  int64_t t931;
  int64_t t932;
  int64_t t933;
  int64_t t934;
  int64_t t935;
  int64_t t936;
  int64_t t937;
  int64_t t938;
  int64_t t939;
  int64_t t940;
  int64_t t941;
  int64_t t942;
  int64_t t943;
  int64_t t944;
  int64_t t945;
  uint8_t t946;
  int64_t t947;
  int64_t t948;
  int64_t t949;
  int64_t t950;
  int64_t t951;
  int64_t t952;
  int64_t t953;
  int64_t t954;
  int64_t t955;
  int64_t t956;
  int64_t t957;
  int64_t t958;
  int64_t t959;
  int64_t t960;
  int64_t t961;
  int64_t t962;
  int64_t t963;
  int64_t t964;
  int64_t t965;
  int64_t t966;
  int64_t t967;
  int64_t t968;
  int64_t t969;
  int64_t t970;
  int64_t t971;
  int64_t t972;
  int64_t t973;
  int64_t t974;
  int64_t t975;
  int64_t t976;
  int64_t t977;
  int64_t t978;
  int64_t t979;
  int64_t t980;
  int64_t t981;
  int64_t t982;
  int64_t t983;
  int64_t t984;
  int64_t t985;
  int64_t t986;
  int64_t t987;
  int64_t t988;
  int64_t t989;
  int64_t t990;
  int64_t t991;
  int64_t t992;
  uint8_t t993;
  int64_t t994;
  int64_t t995;
  int64_t t996;
  int64_t t997;
  int64_t t998;
  int64_t t999;
  uint8_t t1000;
  int64_t t1001;
  int64_t t1002;
  int64_t t1003;
  int64_t t1004;
  int64_t t1005;
  int64_t t1006;
  int64_t t1007;
  int64_t t1008;
  int64_t t1009;
  int64_t t1010;
  int64_t t1011;
  int64_t t1012;
  int64_t t1013;
  int64_t t1014;
  int64_t t1015;
  int64_t t1016;
  int64_t t1017;
  int64_t t1018;
  int64_t t1019;
  int64_t t1020;
  int64_t t1021;
  int64_t t1022;
  uint8_t t1023;
  int64_t t1024;
  int64_t t1025;
  int64_t t1026;
  int64_t t1027;
  uint8_t t1028;
  uint8_t t1029;
  int64_t t1030;
  int64_t t1031;
  uint8_t t1032;
  int64_t t1033;
  int64_t t1034;
  int64_t t1035;
  int64_t t1036;
  int64_t t1037;
  int64_t t1038;
  int64_t t1039;
  int64_t t1040;
  int64_t t1041;
  int64_t t1042;
  int64_t t1043;
  int64_t t1044;
  int64_t t1045;
  int64_t t1046;
  int64_t t1047;
  int64_t t1048;
  int64_t t1049;
  int64_t t1050;
  int64_t t1051;
  int64_t t1052;
  uint8_t t1053;
  int64_t t1054;
  int64_t t1055;
  int64_t t1056;
  int64_t t1057;
  uint8_t t1058;
  int64_t t1059;
  int64_t t1060;
  int64_t t1061;
  int64_t t1062;
  int64_t t1063;
  int64_t t1064;
  int64_t t1065;
  int64_t t1066;
  int64_t t1067;
  int64_t t1068;
  int64_t t1069;
  int64_t t1070;
  int64_t t1071;
  int64_t t1072;
  int64_t t1073;
  int64_t t1074;
  int64_t t1075;
  int64_t t1076;
  int64_t t1077;
  int64_t t1078;
  int64_t t1079;
  int64_t t1080;
  int64_t t1081;
  int64_t t1082;
  int64_t t1083;
  int64_t t1084;
  int64_t t1085;
  int64_t t1086;
  int64_t t1087;
  int64_t t1088;
  int64_t t1089;
  int64_t t1090;
  int64_t t1091;
  int64_t t1092;
  int64_t t1093;
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
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
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
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
  int64_t v42;
  int64_t v43;
  int64_t v44;
  int64_t v45;
  int64_t v46;
  int64_t v47;
  int64_t v48;
  int64_t v49;
  int64_t v50;
  int64_t v51;
  int64_t v52;
  int64_t v53;
  int64_t v54;
  int64_t v55;
  int64_t v56;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg107_ctx(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)UINT64_C(100));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t8)))));
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg107_batch(t9, t10, t11, t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(10))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg107_log(t17, t18, t19, t20);
  (void)(t21);
  t22 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t22)))));
  goto gg_done;
  }
  }
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg107_batch(t23, t24, t25, t26);
  t28 = ((int64_t)(t27));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) > (t29));
  t31 = t30;
  if (t31) {
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg107_batch(t32, t33, t34, t35);
  t37 = ((int64_t)(t36));
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(((int64_t)UINT64_C(2))));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = fg107_batch(t38, t39, t40, t41);
  t43 = ((int64_t)(t42));
  t31 = ((t37) != (t43));
  }
  t44 = t31;
  if (t44) {
  {
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(11))));
  t47 = ((int64_t)(v1));
  t48 = ((int64_t)(((int64_t)UINT64_C(1))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg107_batch(t47, t48, t49, t50);
  t52 = ((int64_t)(t51));
  t53 = ((int64_t)(v1));
  t54 = ((int64_t)(((int64_t)UINT64_C(2))));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = fg107_batch(t53, t54, t55, t56);
  t58 = ((int64_t)(t57));
  t59 = fg107_log(t45, t46, t52, t58);
  (void)(t59);
  t60 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t60)))));
  goto gg_done;
  }
  }
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(10))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = fg107_ctx(t61, t62, t63);
  t65 = ((int64_t)(t64));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((t65) != (t66));
  if (t67) {
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t68 = ((int64_t)(v2));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = ((t68) != (t69));
  t71 = t70;
  if (!t71) {
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(((int64_t)UINT64_C(13))));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = fg107_ctx(t72, t73, t74);
  t76 = ((int64_t)(t75));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((t76) == (t77));
  }
  t78 = t71;
  if (t78) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t79 = ((int64_t)(v0));
  t80 = ((int64_t)(((int64_t)UINT64_C(23))));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = fg107_ctx(t79, t80, t81);
  v4 = ((int64_t)(t82));
  t83 = ((int64_t)(v0));
  t84 = ((int64_t)(((int64_t)UINT64_C(14))));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = fg107_ctx(t83, t84, t85);
  t87 = ((int64_t)(t86));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((t87) != (t88));
  if (t89) {
  {
  t90 = ((int64_t)(v0));
  t91 = ((int64_t)(((int64_t)UINT64_C(31))));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = fg107_ctx(t90, t91, t92);
  v5 = ((int64_t)(t93));
  }
  } else {
  {
  t94 = ((int64_t)(v0));
  t95 = ((int64_t)(((int64_t)UINT64_C(5))));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = fg107_ctx(t94, t95, t96);
  v5 = ((int64_t)(t97));
  }
  }
  t98 = ((int64_t)(v0));
  t99 = ((int64_t)(((int64_t)UINT64_C(5))));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = fg107_ctx_set(t98, t99, t100);
  (void)(t101);
  t102 = ((int64_t)(v4));
  t103 = ((int64_t)(((int64_t)UINT64_C(0))));
  t104 = ((t102) != (t103));
  if (t104) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t105 = ((int64_t)(v6));
  t106 = ((int64_t)(v1));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = ((int64_t)(((int64_t)UINT64_C(0))));
  t109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = fg107_batch(t106, t107, t108, t109);
  t111 = ((int64_t)(t110));
  if (!(((t105) < (t111)))) goto label2;
  {
  t112 = ((int64_t)(v2));
  t113 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((t112) != (t113));
  t115 = t114;
  if (!t115) {
  t116 = ((int64_t)(v1));
  t117 = ((int64_t)(((int64_t)UINT64_C(3))));
  t118 = ((int64_t)(v6));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = fg107_batch(t116, t117, t118, t119);
  t121 = ((int64_t)(t120));
  t122 = ((int64_t)(((int64_t)UINT64_C(0))));
  t115 = ((t121) != (t122));
  }
  t123 = t115;
  if (t123) {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t124 = ((int64_t)(v7));
  t125 = ((int64_t)(v1));
  t126 = ((int64_t)(((int64_t)UINT64_C(4))));
  t127 = ((int64_t)(v6));
  t128 = ((int64_t)(((int64_t)UINT64_C(0))));
  t129 = fg107_batch(t125, t126, t127, t128);
  t130 = ((int64_t)(t129));
  if (!(((t124) < (t130)))) goto label5;
  {
  t131 = ((int64_t)(v1));
  t132 = ((int64_t)(((int64_t)UINT64_C(5))));
  t133 = ((int64_t)(v6));
  t134 = ((int64_t)(v7));
  t135 = fg107_batch(t131, t132, t133, t134);
  v8 = ((int64_t)(t135));
  t136 = ((int64_t)(v8));
  t137 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = ((t136) >= (t137));
  t139 = t138;
  if (t139) {
  t140 = ((int64_t)(v8));
  t141 = ((int64_t)(v5));
  t139 = ((t140) < (t141));
  }
  t142 = t139;
  if (t142) {
  {
  t143 = ((int64_t)(v0));
  t144 = ((int64_t)(((int64_t)UINT64_C(37))));
  t145 = ((int64_t)(v8));
  t146 = fg107_ctx(t143, t144, t145);
  v9 = ((int64_t)(t146));
  t147 = ((int64_t)(v0));
  t148 = ((int64_t)(((int64_t)UINT64_C(36))));
  t149 = ((int64_t)(v8));
  t150 = fg107_ctx(t147, t148, t149);
  t151 = ((int64_t)(t150));
  t152 = ((int64_t)(((int64_t)UINT64_C(0))));
  t153 = ((t151) != (t152));
  t154 = t153;
  if (t154) {
  t155 = ((int64_t)(v9));
  t156 = ((int64_t)(v0));
  t157 = ((int64_t)(((int64_t)UINT64_C(22))));
  t158 = ((int64_t)(((int64_t)UINT64_C(0))));
  t159 = fg107_ctx(t156, t157, t158);
  t160 = ((int64_t)(t159));
  t154 = ((t155) > (t160));
  }
  t161 = t154;
  if (t161) {
  {
  t162 = ((int64_t)(v0));
  t163 = ((int64_t)(((int64_t)UINT64_C(12))));
  t164 = ((int64_t)(v8));
  t165 = ((int64_t)(v9));
  t166 = fg107_log(t162, t163, t164, t165);
  (void)(t166);
  t167 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t167)))));
  goto gg_done;
  }
  }
  }
  }
  t168 = v7;
  t169 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t168) + (uint64_t)(t169)));
  }
  goto label4;
label5:;
  }
  }
  t170 = v6;
  t171 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t170) + (uint64_t)(t171)));
  }
  goto label1;
label2:;
  }
  }
  t172 = ((int64_t)(v0));
  t173 = ((int64_t)(((int64_t)UINT64_C(0))));
  t174 = ((int64_t)(v1));
  t175 = ((int64_t)(v2));
  t176 = fg107_balloc(t172, t173, t174, t175);
  t177 = ((int64_t)(t176));
  t178 = ((int64_t)(((int64_t)UINT64_C(0))));
  t179 = ((t177) == (t178));
  if (t179) {
  {
  t180 = ((int64_t)(v0));
  t181 = ((int64_t)(((int64_t)UINT64_C(13))));
  t182 = ((int64_t)(((int64_t)UINT64_C(0))));
  t183 = ((int64_t)(((int64_t)UINT64_C(0))));
  t184 = fg107_log(t180, t181, t182, t183);
  (void)(t184);
  t185 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t185)))));
  goto gg_done;
  }
  }
  t186 = ((int64_t)(v0));
  t187 = ((int64_t)(((int64_t)UINT64_C(1))));
  t188 = ((int64_t)(((int64_t)UINT64_C(0))));
  t189 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = fg107_balloc(t186, t187, t188, t189);
  v10 = ((int64_t)(t190));
  t191 = ((int64_t)(v0));
  t192 = ((int64_t)(((int64_t)UINT64_C(2))));
  t193 = ((int64_t)(((int64_t)UINT64_C(0))));
  t194 = ((int64_t)(((int64_t)UINT64_C(0))));
  t195 = fg107_balloc(t191, t192, t193, t194);
  v11 = ((int64_t)(t195));
  t196 = ((int64_t)(v2));
  t197 = ((int64_t)(((int64_t)UINT64_C(0))));
  t198 = ((t196) != (t197));
  t199 = t198;
  if (t199) {
  t200 = ((int64_t)(v11));
  t201 = ((int64_t)(v10));
  t199 = ((t200) != (t201));
  }
  t202 = t199;
  if (t202) {
  {
  t203 = ((int64_t)(v0));
  t204 = ((int64_t)(((int64_t)UINT64_C(14))));
  t205 = ((int64_t)(v11));
  t206 = ((int64_t)(v10));
  t207 = fg107_log(t203, t204, t205, t206);
  (void)(t207);
  t208 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t208)))));
  goto gg_done;
  }
  }
  t209 = ((int64_t)(v10));
  t210 = ((int64_t)(v0));
  t211 = ((int64_t)(((int64_t)UINT64_C(3))));
  t212 = ((int64_t)(((int64_t)UINT64_C(0))));
  t213 = fg107_ctx(t210, t211, t212);
  t214 = ((int64_t)(t213));
  if (!(((t209) <= (t214)))) gg_fail("assertion failed");
  t215 = ((int64_t)(v0));
  t216 = ((int64_t)(((int64_t)UINT64_C(13))));
  t217 = ((int64_t)(((int64_t)UINT64_C(0))));
  t218 = fg107_ctx(t215, t216, t217);
  t219 = ((int64_t)(t218));
  t220 = ((int64_t)(((int64_t)UINT64_C(0))));
  t221 = ((t219) != (t220));
  t222 = t221;
  if (!t222) {
  t223 = ((int64_t)(v0));
  t224 = ((int64_t)(((int64_t)UINT64_C(4))));
  t225 = ((int64_t)(((int64_t)UINT64_C(0))));
  t226 = fg107_ctx(t223, t224, t225);
  t227 = ((int64_t)(t226));
  t228 = ((int64_t)(v10));
  t222 = ((t227) >= (t228));
  }
  if (!(t222)) gg_fail("assertion failed");
  t229 = ((int64_t)(v0));
  t230 = ((int64_t)(((int64_t)UINT64_C(12))));
  t231 = ((int64_t)(((int64_t)UINT64_C(0))));
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((int64_t)(((int64_t)UINT64_C(0))));
  t234 = fg107_out(t229, t230, t231, t232, t233);
  t235 = ((int64_t)(t234));
  t236 = ((int64_t)(((int64_t)UINT64_C(0))));
  t237 = ((t235) > (t236));
  if (t237) {
  {
  t238 = ((int64_t)(v0));
  gg_function_9(t238, base + 0);
  }
  }
  t239 = ((int64_t)(v0));
  t240 = ((int64_t)(((int64_t)UINT64_C(13))));
  t241 = ((int64_t)(((int64_t)UINT64_C(0))));
  t242 = ((int64_t)(((int64_t)UINT64_C(0))));
  t243 = ((int64_t)(((int64_t)UINT64_C(0))));
  t244 = fg107_out(t239, t240, t241, t242, t243);
  (void)(t244);
  t245 = ((int64_t)(v0));
  t246 = ((int64_t)(((int64_t)UINT64_C(0))));
  t247 = ((int64_t)(((int64_t)UINT64_C(2))));
  t248 = ((int64_t)(((int64_t)UINT64_C(0))));
  t249 = fg107_perf(t245, t246, t247, t248);
  t250 = ((int64_t)(t249));
  t251 = ((int64_t)(((int64_t)UINT64_C(0))));
  t252 = ((t250) == (t251));
  if (t252) {
  {
  t253 = ((int64_t)(v0));
  t254 = ((int64_t)(((int64_t)UINT64_C(1))));
  t255 = ((int64_t)(((int64_t)UINT64_C(2))));
  t256 = ((int64_t)(v0));
  t257 = ((int64_t)(((int64_t)UINT64_C(33))));
  t258 = ((int64_t)(((int64_t)UINT64_C(0))));
  t259 = fg107_ctx(t256, t257, t258);
  t260 = ((int64_t)(t259));
  t261 = fg107_perf(t253, t254, t255, t260);
  (void)(t261);
  }
  }
  t262 = ((int64_t)(v0));
  t263 = ((int64_t)(((int64_t)UINT64_C(1))));
  t264 = ((int64_t)(((int64_t)UINT64_C(0))));
  t265 = ((int64_t)(v0));
  t266 = ((int64_t)(((int64_t)UINT64_C(0))));
  t267 = ((int64_t)(((int64_t)UINT64_C(0))));
  t268 = ((int64_t)(((int64_t)UINT64_C(0))));
  t269 = fg107_perf(t265, t266, t267, t268);
  t270 = ((int64_t)(t269));
  t271 = ((int64_t)(v10));
  t272 = ((int64_t)(((int64_t)((uint64_t)(t270) + (uint64_t)(t271)))));
  t273 = fg107_perf(t262, t263, t264, t272);
  (void)(t273);
  t274 = ((int64_t)(v0));
  t275 = ((int64_t)(((int64_t)UINT64_C(8))));
  t276 = ((int64_t)(((int64_t)UINT64_C(0))));
  t277 = ((int64_t)(((int64_t)UINT64_C(0))));
  t278 = ((int64_t)(((int64_t)UINT64_C(0))));
  t279 = fg107_exec(t274, t275, t276, t277, t278);
  (void)(t279);
  t280 = ((int64_t)(v0));
  t281 = ((int64_t)(((int64_t)UINT64_C(12))));
  t282 = ((int64_t)(((int64_t)UINT64_C(0))));
  t283 = ((int64_t)(((int64_t)UINT64_C(0))));
  t284 = ((int64_t)(((int64_t)UINT64_C(0))));
  t285 = fg107_exec(t280, t281, t282, t283, t284);
  (void)(t285);
  t286 = ((int64_t)(v0));
  t287 = ((int64_t)(((int64_t)UINT64_C(7))));
  t288 = ((int64_t)(((int64_t)UINT64_C(0))));
  t289 = ((int64_t)(((int64_t)UINT64_C(0))));
  t290 = ((int64_t)(((int64_t)UINT64_C(0))));
  t291 = fg107_sched(t286, t287, t288, t289, t290);
  (void)(t291);
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t292 = ((int64_t)(v0));
  t293 = ((int64_t)(((int64_t)UINT64_C(0))));
  t294 = gg_function_13(t292, t293, base + 0);
  (void)(t294);
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t295 = ((int64_t)(v14));
  t296 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t295) == (t296)))) goto label8;
  {
  t297 = ((int64_t)(v0));
  t298 = ((int64_t)(((int64_t)UINT64_C(0))));
  t299 = ((int64_t)(v0));
  t300 = ((int64_t)(((int64_t)UINT64_C(4))));
  t301 = ((int64_t)(((int64_t)UINT64_C(0))));
  t302 = fg107_ctx(t299, t300, t301);
  t303 = ((int64_t)(t302));
  t304 = ((int64_t)(v3));
  t305 = ((int64_t)(((int64_t)UINT64_C(0))));
  t306 = fg107_mem(t297, t298, t303, t304, t305);
  v13 = ((int64_t)(t306));
  t307 = ((int64_t)(v13));
  t308 = ((int64_t)(((int64_t)UINT64_C(0))));
  t309 = ((t307) == (t308));
  if (t309) {
  {
  t310 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t310)))));
  goto gg_done;
  }
  }
  t311 = ((int64_t)(v13));
  t312 = ((int64_t)(((int64_t)UINT64_C(0))));
  t313 = ((int64_t)(((int64_t)UINT64_C(0))));
  t314 = fg107_mctx(t311, t312, t313);
  v15 = ((int64_t)(t314));
  t315 = ((int64_t)(v15));
  t316 = ((int64_t)(((int64_t)UINT64_C(0))));
  t317 = ((t315) == (t316));
  if (t317) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  t318 = ((int64_t)(v15));
  t319 = ((int64_t)(((int64_t)UINT64_C(1))));
  t320 = ((t318) == (t319));
  if (t320) {
  {
  t321 = ((int64_t)(v0));
  t322 = ((int64_t)(((int64_t)UINT64_C(15))));
  t323 = ((int64_t)(v15));
  t324 = ((int64_t)(((int64_t)UINT64_C(0))));
  t325 = fg107_log(t321, t322, t323, t324);
  (void)(t325);
  t326 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t326)))));
  goto gg_done;
  }
  } else {
  t327 = ((int64_t)(v15));
  t328 = ((int64_t)(((int64_t)UINT64_C(2))));
  t329 = ((t327) == (t328));
  if (t329) {
  {
  t330 = ((int64_t)(v12));
  t331 = ((int64_t)(((int64_t)UINT64_C(0))));
  t332 = ((t330) == (t331));
  if (t332) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t333 = ((int64_t)(v0));
  t334 = ((int64_t)(((int64_t)UINT64_C(1))));
  t335 = gg_function_13(t333, t334, base + 0);
  t336 = t335;
  if (t336) {
  {
  t337 = ((int64_t)(v0));
  t338 = ((int64_t)(((int64_t)UINT64_C(16))));
  t339 = ((int64_t)(v0));
  t340 = ((int64_t)(((int64_t)UINT64_C(1))));
  t341 = ((int64_t)(((int64_t)UINT64_C(0))));
  t342 = ((int64_t)(((int64_t)UINT64_C(0))));
  t343 = fg107_balloc(t339, t340, t341, t342);
  t344 = ((int64_t)(t343));
  t345 = ((int64_t)(((int64_t)UINT64_C(0))));
  t346 = fg107_log(t337, t338, t344, t345);
  (void)(t346);
  }
  } else {
  {
  t347 = ((int64_t)(v0));
  t348 = ((int64_t)(((int64_t)UINT64_C(17))));
  t349 = ((int64_t)(v0));
  t350 = ((int64_t)(((int64_t)UINT64_C(1))));
  t351 = ((int64_t)(((int64_t)UINT64_C(0))));
  t352 = ((int64_t)(((int64_t)UINT64_C(0))));
  t353 = fg107_balloc(t349, t350, t351, t352);
  t354 = ((int64_t)(t353));
  t355 = ((int64_t)(((int64_t)UINT64_C(0))));
  t356 = fg107_log(t347, t348, t354, t355);
  (void)(t356);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  }
  } else {
  {
  t357 = ((int64_t)(v0));
  t358 = ((int64_t)(((int64_t)UINT64_C(17))));
  t359 = ((int64_t)(v0));
  t360 = ((int64_t)(((int64_t)UINT64_C(1))));
  t361 = ((int64_t)(((int64_t)UINT64_C(0))));
  t362 = ((int64_t)(((int64_t)UINT64_C(0))));
  t363 = fg107_balloc(t359, t360, t361, t362);
  t364 = ((int64_t)(t363));
  t365 = ((int64_t)(((int64_t)UINT64_C(0))));
  t366 = fg107_log(t357, t358, t364, t365);
  (void)(t366);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  }
  } else {
  {
  t367 = ((int64_t)(v0));
  t368 = ((int64_t)(((int64_t)UINT64_C(18))));
  t369 = ((int64_t)(v0));
  t370 = ((int64_t)(((int64_t)UINT64_C(1))));
  t371 = ((int64_t)(((int64_t)UINT64_C(0))));
  t372 = ((int64_t)(((int64_t)UINT64_C(0))));
  t373 = fg107_balloc(t369, t370, t371, t372);
  t374 = ((int64_t)(t373));
  t375 = ((int64_t)(((int64_t)UINT64_C(0))));
  t376 = fg107_log(t367, t368, t374, t375);
  (void)(t376);
  t377 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t377)))));
  goto gg_done;
  }
  }
  }
  }
  }
  goto label7;
label8:;
  t378 = ((int64_t)(v0));
  t379 = ((int64_t)(v11));
  t380 = gg_function_19(t378, t379, base + 0);
  t381 = ((int64_t)(t380));
  t382 = ((int64_t)(v11));
  t383 = ((t381) < (t382));
  if (t383) {
  {
  t384 = ((int64_t)(v0));
  t385 = ((int64_t)(((int64_t)UINT64_C(19))));
  t386 = ((int64_t)(v11));
  t387 = ((int64_t)(((int64_t)UINT64_C(0))));
  t388 = fg107_log(t384, t385, t386, t387);
  (void)(t388);
  t389 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t389)))));
  goto gg_done;
  }
  }
  t390 = ((int64_t)(v0));
  t391 = ((int64_t)(((int64_t)UINT64_C(3))));
  t392 = ((int64_t)(((int64_t)UINT64_C(0))));
  t393 = fg107_ctx_set(t390, t391, t392);
  (void)(t393);
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
  v18 = ((int64_t)(((int64_t)UINT64_C(1))));
label10:;
  t394 = ((int64_t)(v18));
  t395 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t394) != (t395)))) goto label11;
  {
  t396 = ((int64_t)(v13));
  t397 = ((int64_t)(((int64_t)UINT64_C(3))));
  t398 = ((int64_t)(((int64_t)UINT64_C(0))));
  t399 = fg107_mctx(t396, t397, t398);
  v19 = ((int64_t)(t399));
  t400 = ((int64_t)(v19));
  t401 = ((int64_t)(((int64_t)UINT64_C(0))));
  t402 = ((int64_t)(((int64_t)UINT64_C(0))));
  t403 = ((int64_t)(((int64_t)UINT64_C(0))));
  t404 = fg107_ub(t400, t401, t402, t403);
  v20 = ((int64_t)(t404));
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t405 = ((int64_t)(v11));
  t406 = ((int64_t)(v10));
  t407 = ((t405) == (t406));
  if (t407) {
  {
  v21 = ((int64_t)(v20));
  }
  } else {
  {
  v22 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t408 = ((int64_t)(v22));
  t409 = ((int64_t)(v20));
  if (!(((t408) < (t409)))) goto label14;
  {
  t410 = v21;
  t411 = ((int64_t)(v19));
  t412 = ((int64_t)(((int64_t)UINT64_C(1))));
  t413 = ((int64_t)(v22));
  t414 = ((int64_t)(((int64_t)UINT64_C(0))));
  t415 = fg107_ub(t411, t412, t413, t414);
  t416 = ((int64_t)(t415));
  v21 = ((int64_t)((uint64_t)(t410) + (uint64_t)(t416)));
  t417 = v22;
  t418 = ((int64_t)(((int64_t)UINT64_C(1))));
  v22 = ((int64_t)((uint64_t)(t417) + (uint64_t)(t418)));
  }
  goto label13;
label14:;
  }
  }
  t419 = ((int64_t)(v0));
  t420 = ((int64_t)(((int64_t)UINT64_C(0))));
  t421 = ((int64_t)(v21));
  t422 = fg107_ctx_set(t419, t420, t421);
  (void)(t422);
  t423 = ((int64_t)(v0));
  t424 = ((int64_t)(v19));
  t425 = ((int64_t)(v0));
  t426 = ((int64_t)(((int64_t)UINT64_C(17))));
  t427 = ((int64_t)(((int64_t)UINT64_C(0))));
  t428 = fg107_ctx(t425, t426, t427);
  t429 = ((int64_t)(t428));
  t430 = ((int64_t)(v13));
  v23 = gg_function_22(t423, t424, t429, t430, base + 0);
  t431 = ((int64_t)(v23));
  t432 = ((int64_t)(((int64_t)UINT64_C(0))));
  t433 = ((t431) == (t432));
  if (t433) {
  {
  v24 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t434 = ((int64_t)(v24));
  t435 = ((int64_t)(v0));
  t436 = ((int64_t)(((int64_t)UINT64_C(31))));
  t437 = ((int64_t)(((int64_t)UINT64_C(0))));
  t438 = fg107_ctx(t435, t436, t437);
  t439 = ((int64_t)(t438));
  if (!(((t434) < (t439)))) goto label17;
  {
  t440 = ((int64_t)(v0));
  t441 = ((int64_t)(((int64_t)UINT64_C(32))));
  t442 = ((int64_t)(((int64_t)UINT64_C(0))));
  t443 = fg107_ctx(t440, t441, t442);
  v25 = ((int64_t)(t443));
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t444 = ((int64_t)(v26));
  t445 = ((int64_t)(v20));
  if (!(((t444) < (t445)))) goto label20;
  {
  t446 = ((int64_t)(v19));
  t447 = ((int64_t)(((int64_t)UINT64_C(3))));
  t448 = ((int64_t)(v26));
  t449 = ((int64_t)(((int64_t)UINT64_C(0))));
  t450 = fg107_ub(t446, t447, t448, t449);
  t451 = ((int64_t)(t450));
  t452 = ((int64_t)(v24));
  t453 = ((t451) == (t452));
  t454 = t453;
  if (t454) {
  t455 = ((int64_t)(v19));
  t456 = ((int64_t)(((int64_t)UINT64_C(4))));
  t457 = ((int64_t)(v26));
  t458 = ((int64_t)(((int64_t)UINT64_C(0))));
  t459 = fg107_ub(t455, t456, t457, t458);
  t460 = ((int64_t)(t459));
  t461 = ((int64_t)(v25));
  t454 = ((t460) < (t461));
  }
  t462 = t454;
  if (t462) {
  {
  t463 = ((int64_t)(v19));
  t464 = ((int64_t)(((int64_t)UINT64_C(4))));
  t465 = ((int64_t)(v26));
  t466 = ((int64_t)(((int64_t)UINT64_C(0))));
  t467 = fg107_ub(t463, t464, t465, t466);
  v25 = ((int64_t)(t467));
  }
  }
  t468 = v26;
  t469 = ((int64_t)(((int64_t)UINT64_C(1))));
  v26 = ((int64_t)((uint64_t)(t468) + (uint64_t)(t469)));
  }
  goto label19;
label20:;
  t470 = ((int64_t)(v25));
  t471 = ((int64_t)(v0));
  t472 = ((int64_t)(((int64_t)UINT64_C(32))));
  t473 = ((int64_t)(((int64_t)UINT64_C(0))));
  t474 = fg107_ctx(t471, t472, t473);
  t475 = ((int64_t)(t474));
  t476 = ((t470) != (t475));
  if (t476) {
  {
  t477 = ((int64_t)(v0));
  t478 = ((int64_t)(((int64_t)UINT64_C(20))));
  t479 = ((int64_t)(v24));
  t480 = ((int64_t)(v25));
  t481 = fg107_log(t477, t478, t479, t480);
  (void)(t481);
  t482 = ((int64_t)(v0));
  t483 = ((int64_t)(((int64_t)UINT64_C(3))));
  t484 = ((int64_t)(v24));
  t485 = ((int64_t)(v25));
  t486 = ((int64_t)UINT64_C(1));
  t487 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t486)))));
  t488 = fg107_mem(t482, t483, t484, t485, t487);
  (void)(t488);
  }
  }
  t489 = v24;
  t490 = ((int64_t)(((int64_t)UINT64_C(1))));
  v24 = ((int64_t)((uint64_t)(t489) + (uint64_t)(t490)));
  }
  goto label16;
label17:;
  t491 = ((int64_t)(v0));
  t492 = ((int64_t)(((int64_t)UINT64_C(39))));
  t493 = ((int64_t)(((int64_t)UINT64_C(0))));
  t494 = fg107_ctx(t491, t492, t493);
  v27 = ((int64_t)(t494));
  t495 = ((int64_t)(v27));
  t496 = ((int64_t)(((int64_t)UINT64_C(1))));
  t497 = ((t495) == (t496));
  if (t497) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  goto gg_done;
  }
  } else {
  t498 = ((int64_t)(v27));
  t499 = ((int64_t)UINT64_C(2));
  t500 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t499)))));
  t501 = ((t498) == (t500));
  if (t501) {
  {
  t502 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t502)))));
  goto gg_done;
  }
  } else {
  t503 = ((int64_t)(v27));
  t504 = ((int64_t)UINT64_C(1));
  t505 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t504)))));
  t506 = ((t503) == (t505));
  if (t506) {
  {
  t507 = ((int64_t)UINT64_C(3));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t507)))));
  goto gg_done;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  t508 = ((int64_t)(v0));
  t509 = ((int64_t)(v23));
  t510 = ((int64_t)(((int64_t)UINT64_C(4))));
  t511 = ((int64_t)(((int64_t)UINT64_C(0))));
  t512 = fg107_res(t508, t509, t510, t511);
  v28 = ((int64_t)(t512));
  v29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t513 = ((int64_t)(v0));
  t514 = ((int64_t)(((int64_t)UINT64_C(10))));
  t515 = ((int64_t)(((int64_t)UINT64_C(0))));
  t516 = fg107_ctx(t513, t514, t515);
  t517 = ((int64_t)(t516));
  t518 = ((int64_t)(((int64_t)UINT64_C(0))));
  t519 = ((t517) != (t518));
  if (t519) {
  {
  t520 = ((int64_t)(v0));
  t521 = ((int64_t)(v23));
  t522 = ((int64_t)(((int64_t)UINT64_C(5))));
  t523 = ((int64_t)(((int64_t)UINT64_C(0))));
  t524 = fg107_res(t520, t521, t522, t523);
  v29 = ((int64_t)(t524));
  }
  }
  v30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t525 = ((int64_t)(v0));
  t526 = ((int64_t)(((int64_t)UINT64_C(11))));
  t527 = ((int64_t)(((int64_t)UINT64_C(0))));
  t528 = fg107_ctx(t525, t526, t527);
  t529 = ((int64_t)(t528));
  t530 = ((int64_t)(((int64_t)UINT64_C(0))));
  t531 = ((t529) != (t530));
  if (t531) {
  {
  t532 = ((int64_t)(v0));
  t533 = ((int64_t)(v23));
  t534 = ((int64_t)(((int64_t)UINT64_C(7))));
  t535 = ((int64_t)(((int64_t)UINT64_C(0))));
  t536 = fg107_res(t532, t533, t534, t535);
  v30 = ((int64_t)(t536));
  }
  }
  t537 = ((int64_t)(v29));
  t538 = ((int64_t)(((int64_t)UINT64_C(0))));
  t539 = ((t537) != (t538));
  t540 = t539;
  if (t540) {
  t541 = ((int64_t)(v0));
  t542 = ((int64_t)(v23));
  t543 = ((int64_t)(((int64_t)UINT64_C(6))));
  t544 = ((int64_t)(((int64_t)UINT64_C(0))));
  t545 = fg107_res(t541, t542, t543, t544);
  t546 = ((int64_t)(t545));
  t547 = ((int64_t)(((int64_t)UINT64_C(0))));
  t540 = ((t546) != (t547));
  }
  t548 = t540;
  if (t548) {
  {
  t549 = ((int64_t)(v0));
  t550 = ((int64_t)(v23));
  t551 = ((int64_t)(((int64_t)UINT64_C(6))));
  t552 = ((int64_t)(((int64_t)UINT64_C(0))));
  t553 = fg107_res(t549, t550, t551, t552);
  v29 = ((int64_t)(t553));
  }
  }
  t554 = ((int64_t)(v0));
  t555 = ((int64_t)(((int64_t)UINT64_C(25))));
  t556 = ((int64_t)(((int64_t)UINT64_C(0))));
  t557 = fg107_ctx(t554, t555, t556);
  v31 = ((int64_t)(t557));
  t558 = ((int64_t)(v0));
  t559 = ((int64_t)(((int64_t)UINT64_C(3))));
  t560 = ((int64_t)(((int64_t)UINT64_C(0))));
  t561 = ((int64_t)(((int64_t)UINT64_C(0))));
  t562 = ((int64_t)(((int64_t)UINT64_C(0))));
  t563 = fg107_out(t558, t559, t560, t561, t562);
  t564 = ((int64_t)(t563));
  t565 = ((int64_t)(((int64_t)UINT64_C(0))));
  t566 = ((t564) != (t565));
  t567 = t566;
  if (t567) {
  t568 = ((int64_t)(v28));
  t569 = ((int64_t)(((int64_t)UINT64_C(0))));
  t567 = ((t568) != (t569));
  }
  t570 = t567;
  t571 = t570;
  if (t571) {
  t572 = ((int64_t)(v31));
  t573 = ((int64_t)(((int64_t)UINT64_C(0))));
  t571 = ((t572) > (t573));
  }
  t574 = t571;
  t575 = t574;
  if (t575) {
  t576 = ((int64_t)(v0));
  t577 = ((int64_t)(v19));
  t578 = gg_function_29(t576, t577, base + 0);
  t579 = ((int64_t)(t578));
  t580 = ((int64_t)(((int64_t)UINT64_C(0))));
  t575 = ((t579) != (t580));
  }
  t581 = t575;
  if (t581) {
  {
  t582 = ((int64_t)(v0));
  t583 = ((int64_t)(((int64_t)UINT64_C(4))));
  t584 = ((int64_t)(v28));
  t585 = ((int64_t)(((int64_t)UINT64_C(0))));
  t586 = ((int64_t)(((int64_t)UINT64_C(0))));
  t587 = fg107_sched(t582, t583, t584, t585, t586);
  v32 = ((int64_t)(t587));
  t588 = ((int64_t)(v32));
  t589 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t588) != (t589)))) gg_fail("assertion failed");
  t590 = ((int64_t)(v16));
  t591 = ((int64_t)(v31));
  t592 = ((int64_t)(((int64_t)((uint64_t)(t590) + (uint64_t)(t591)))));
  t593 = ((int64_t)(v11));
  if (!(((t592) <= (t593)))) gg_fail("assertion failed");
  t594 = ((int64_t)(v16));
  t595 = ((int64_t)(v31));
  t596 = ((int64_t)(((int64_t)((uint64_t)(t594) + (uint64_t)(t595)))));
  t597 = ((int64_t)(v0));
  t598 = ((int64_t)(((int64_t)UINT64_C(6))));
  t599 = ((int64_t)(((int64_t)UINT64_C(0))));
  t600 = fg107_ctx(t597, t598, t599);
  t601 = ((int64_t)(t600));
  t602 = ((int64_t)(((int64_t)((uint64_t)(t596) * (uint64_t)(t601)))));
  t603 = ((int64_t)(v0));
  t604 = ((int64_t)(((int64_t)UINT64_C(4))));
  t605 = ((int64_t)(((int64_t)UINT64_C(0))));
  t606 = ((int64_t)(((int64_t)UINT64_C(0))));
  t607 = ((int64_t)(((int64_t)UINT64_C(0))));
  t608 = fg107_out(t603, t604, t605, t606, t607);
  t609 = ((int64_t)(t608));
  if (!(((t602) <= (t609)))) gg_fail("assertion failed");
  t610 = ((int64_t)(v32));
  t611 = ((int64_t)(v28));
  t612 = ((int64_t)(v0));
  t613 = ((int64_t)(((int64_t)UINT64_C(3))));
  t614 = ((int64_t)(((int64_t)UINT64_C(0))));
  t615 = ((int64_t)(((int64_t)UINT64_C(0))));
  t616 = ((int64_t)(((int64_t)UINT64_C(0))));
  t617 = fg107_out(t612, t613, t614, t615, t616);
  t618 = ((int64_t)(t617));
  t619 = ((int64_t)(v16));
  t620 = ((int64_t)(v0));
  t621 = ((int64_t)(((int64_t)UINT64_C(6))));
  t622 = ((int64_t)(((int64_t)UINT64_C(0))));
  t623 = fg107_ctx(t620, t621, t622);
  t624 = ((int64_t)(t623));
  t625 = ((int64_t)(((int64_t)((uint64_t)(t619) * (uint64_t)(t624)))));
  t626 = ((int64_t)(((int64_t)UINT64_C(4))));
  t627 = ((int64_t)(((int64_t)((uint64_t)(t625) * (uint64_t)(t626)))));
  t628 = ((int64_t)(((int64_t)((uint64_t)(t618) + (uint64_t)(t627)))));
  t629 = ((int64_t)(((int64_t)UINT64_C(0))));
  t630 = ((int64_t)(v31));
  t631 = ((int64_t)(v0));
  t632 = ((int64_t)(((int64_t)UINT64_C(6))));
  t633 = ((int64_t)(((int64_t)UINT64_C(0))));
  t634 = fg107_ctx(t631, t632, t633);
  t635 = ((int64_t)(t634));
  t636 = ((int64_t)(((int64_t)((uint64_t)(t630) * (uint64_t)(t635)))));
  t637 = ((int64_t)(((int64_t)UINT64_C(4))));
  t638 = ((int64_t)(((int64_t)((uint64_t)(t636) * (uint64_t)(t637)))));
  t639 = fg107_tensor_get_async(t610, t611, t628, t629, t638);
  (void)(t639);
  }
  }
  t640 = ((int64_t)(v0));
  t641 = ((int64_t)(((int64_t)UINT64_C(5))));
  t642 = ((int64_t)(((int64_t)UINT64_C(0))));
  t643 = ((int64_t)(((int64_t)UINT64_C(0))));
  t644 = ((int64_t)(((int64_t)UINT64_C(0))));
  t645 = fg107_out(t640, t641, t642, t643, t644);
  t646 = ((int64_t)(t645));
  t647 = ((int64_t)(((int64_t)UINT64_C(0))));
  t648 = ((t646) != (t647));
  t649 = t648;
  if (t649) {
  t650 = ((int64_t)(v29));
  t651 = ((int64_t)(((int64_t)UINT64_C(0))));
  t649 = ((t650) != (t651));
  }
  t652 = t649;
  t653 = t652;
  if (t653) {
  t654 = ((int64_t)(v31));
  t655 = ((int64_t)(((int64_t)UINT64_C(0))));
  t653 = ((t654) > (t655));
  }
  t656 = t653;
  if (t656) {
  {
  t657 = ((int64_t)(v0));
  t658 = ((int64_t)(((int64_t)UINT64_C(4))));
  t659 = ((int64_t)(v29));
  t660 = ((int64_t)(((int64_t)UINT64_C(0))));
  t661 = ((int64_t)(((int64_t)UINT64_C(0))));
  t662 = fg107_sched(t657, t658, t659, t660, t661);
  v33 = ((int64_t)(t662));
  t663 = ((int64_t)(v33));
  t664 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t663) != (t664)))) gg_fail("assertion failed");
  t665 = ((int64_t)(v0));
  t666 = ((int64_t)(((int64_t)UINT64_C(18))));
  t667 = ((int64_t)(((int64_t)UINT64_C(0))));
  t668 = fg107_ctx(t665, t666, t667);
  v34 = ((int64_t)(t668));
  t669 = ((int64_t)(v34));
  t670 = ((int64_t)(((int64_t)UINT64_C(0))));
  t671 = ((t669) == (t670));
  if (t671) {
  {
  t672 = ((int64_t)(v0));
  t673 = ((int64_t)(((int64_t)UINT64_C(8))));
  t674 = ((int64_t)(((int64_t)UINT64_C(0))));
  t675 = fg107_ctx(t672, t673, t674);
  v35 = ((int64_t)(t675));
  t676 = ((int64_t)(v16));
  t677 = ((int64_t)(v31));
  t678 = ((int64_t)(((int64_t)((uint64_t)(t676) + (uint64_t)(t677)))));
  t679 = ((int64_t)(v11));
  if (!(((t678) <= (t679)))) gg_fail("assertion failed");
  t680 = ((int64_t)(v16));
  t681 = ((int64_t)(v31));
  t682 = ((int64_t)(((int64_t)((uint64_t)(t680) + (uint64_t)(t681)))));
  t683 = ((int64_t)(v35));
  t684 = ((int64_t)(((int64_t)((uint64_t)(t682) * (uint64_t)(t683)))));
  t685 = ((int64_t)(v0));
  t686 = ((int64_t)(((int64_t)UINT64_C(6))));
  t687 = ((int64_t)(((int64_t)UINT64_C(0))));
  t688 = ((int64_t)(((int64_t)UINT64_C(0))));
  t689 = ((int64_t)(((int64_t)UINT64_C(0))));
  t690 = fg107_out(t685, t686, t687, t688, t689);
  t691 = ((int64_t)(t690));
  if (!(((t684) <= (t691)))) gg_fail("assertion failed");
  t692 = ((int64_t)(v33));
  t693 = ((int64_t)(v29));
  t694 = ((int64_t)(v0));
  t695 = ((int64_t)(((int64_t)UINT64_C(5))));
  t696 = ((int64_t)(((int64_t)UINT64_C(0))));
  t697 = ((int64_t)(((int64_t)UINT64_C(0))));
  t698 = ((int64_t)(((int64_t)UINT64_C(0))));
  t699 = fg107_out(t694, t695, t696, t697, t698);
  t700 = ((int64_t)(t699));
  t701 = ((int64_t)(v16));
  t702 = ((int64_t)(v35));
  t703 = ((int64_t)(((int64_t)((uint64_t)(t701) * (uint64_t)(t702)))));
  t704 = ((int64_t)(((int64_t)UINT64_C(4))));
  t705 = ((int64_t)(((int64_t)((uint64_t)(t703) * (uint64_t)(t704)))));
  t706 = ((int64_t)(((int64_t)((uint64_t)(t700) + (uint64_t)(t705)))));
  t707 = ((int64_t)(((int64_t)UINT64_C(0))));
  t708 = ((int64_t)(v31));
  t709 = ((int64_t)(v35));
  t710 = ((int64_t)(((int64_t)((uint64_t)(t708) * (uint64_t)(t709)))));
  t711 = ((int64_t)(((int64_t)UINT64_C(4))));
  t712 = ((int64_t)(((int64_t)((uint64_t)(t710) * (uint64_t)(t711)))));
  t713 = fg107_tensor_get_async(t692, t693, t706, t707, t712);
  (void)(t713);
  }
  } else {
  t714 = ((int64_t)(v34));
  t715 = ((int64_t)(((int64_t)UINT64_C(1))));
  t716 = ((t714) == (t715));
  t717 = t716;
  if (!t717) {
  t718 = ((int64_t)(v34));
  t719 = ((int64_t)(((int64_t)UINT64_C(2))));
  t717 = ((t718) == (t719));
  }
  t720 = t717;
  t721 = t720;
  if (!t721) {
  t722 = ((int64_t)(v34));
  t723 = ((int64_t)(((int64_t)UINT64_C(3))));
  t721 = ((t722) == (t723));
  }
  t724 = t721;
  if (t724) {
  {
  t725 = ((int64_t)(v0));
  t726 = ((int64_t)(((int64_t)UINT64_C(8))));
  t727 = ((int64_t)(((int64_t)UINT64_C(0))));
  t728 = fg107_ctx(t725, t726, t727);
  v36 = ((int64_t)(t728));
  v37 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t729 = ((int64_t)(v37));
  t730 = ((int64_t)(v19));
  t731 = ((int64_t)(((int64_t)UINT64_C(5))));
  t732 = ((int64_t)(((int64_t)UINT64_C(0))));
  t733 = ((int64_t)(((int64_t)UINT64_C(0))));
  t734 = fg107_ub(t730, t731, t732, t733);
  t735 = ((int64_t)(t734));
  if (!(((t729) < (t735)))) goto label23;
  {
  t736 = ((int64_t)(v19));
  t737 = ((int64_t)(((int64_t)UINT64_C(6))));
  t738 = ((int64_t)(v37));
  t739 = ((int64_t)(((int64_t)UINT64_C(0))));
  t740 = fg107_ub(t736, t737, t738, t739);
  v38 = ((int64_t)(t740));
  t741 = ((int64_t)(v19));
  t742 = ((int64_t)(((int64_t)UINT64_C(7))));
  t743 = ((int64_t)(v38));
  t744 = ((int64_t)(((int64_t)UINT64_C(0))));
  t745 = fg107_ub(t741, t742, t743, t744);
  v39 = ((int64_t)(t745));
  t746 = ((int64_t)(v0));
  t747 = ((int64_t)(((int64_t)UINT64_C(14))));
  t748 = ((int64_t)(v38));
  t749 = ((int64_t)(v36));
  t750 = ((int64_t)(((int64_t)UINT64_C(0))));
  t751 = fg107_out(t746, t747, t748, t749, t750);
  (void)(t751);
  t752 = ((int64_t)(v33));
  t753 = ((int64_t)(v29));
  t754 = ((int64_t)(v0));
  t755 = ((int64_t)(((int64_t)UINT64_C(15))));
  t756 = ((int64_t)(v38));
  t757 = ((int64_t)(((int64_t)UINT64_C(0))));
  t758 = ((int64_t)(((int64_t)UINT64_C(0))));
  t759 = fg107_out(t754, t755, t756, t757, t758);
  t760 = ((int64_t)(t759));
  t761 = ((int64_t)(v36));
  t762 = ((int64_t)(v39));
  t763 = ((int64_t)(((int64_t)((uint64_t)(t761) * (uint64_t)(t762)))));
  t764 = ((int64_t)(((int64_t)UINT64_C(4))));
  t765 = ((int64_t)(((int64_t)((uint64_t)(t763) * (uint64_t)(t764)))));
  t766 = ((int64_t)(((int64_t)((uint64_t)(t760) + (uint64_t)(t765)))));
  t767 = ((int64_t)(((int64_t)UINT64_C(0))));
  t768 = ((int64_t)(v36));
  t769 = ((int64_t)(((int64_t)UINT64_C(4))));
  t770 = ((int64_t)(((int64_t)((uint64_t)(t768) * (uint64_t)(t769)))));
  t771 = fg107_tensor_get_async(t752, t753, t766, t767, t770);
  (void)(t771);
  t772 = v37;
  t773 = ((int64_t)(((int64_t)UINT64_C(1))));
  v37 = ((int64_t)((uint64_t)(t772) + (uint64_t)(t773)));
  }
  goto label22;
label23:;
  }
  } else {
  t774 = ((int64_t)(v34));
  t775 = ((int64_t)(((int64_t)UINT64_C(4))));
  t776 = ((t774) == (t775));
  if (t776) {
  {
  t777 = ((int64_t)(v0));
  t778 = ((int64_t)(((int64_t)UINT64_C(9))));
  t779 = ((int64_t)(((int64_t)UINT64_C(0))));
  t780 = fg107_ctx(t777, t778, t779);
  v40 = ((int64_t)(t780));
  v41 = ((int64_t)(((int64_t)UINT64_C(0))));
label25:;
  t781 = ((int64_t)(v41));
  t782 = ((int64_t)(v19));
  t783 = ((int64_t)(((int64_t)UINT64_C(5))));
  t784 = ((int64_t)(((int64_t)UINT64_C(0))));
  t785 = ((int64_t)(((int64_t)UINT64_C(0))));
  t786 = fg107_ub(t782, t783, t784, t785);
  t787 = ((int64_t)(t786));
  if (!(((t781) < (t787)))) goto label26;
  {
  t788 = ((int64_t)(v19));
  t789 = ((int64_t)(((int64_t)UINT64_C(6))));
  t790 = ((int64_t)(v41));
  t791 = ((int64_t)(((int64_t)UINT64_C(0))));
  t792 = fg107_ub(t788, t789, t790, t791);
  v42 = ((int64_t)(t792));
  t793 = ((int64_t)(v19));
  t794 = ((int64_t)(((int64_t)UINT64_C(7))));
  t795 = ((int64_t)(v42));
  t796 = ((int64_t)(((int64_t)UINT64_C(0))));
  t797 = fg107_ub(t793, t794, t795, t796);
  v43 = ((int64_t)(t797));
  t798 = ((int64_t)(v0));
  t799 = ((int64_t)(((int64_t)UINT64_C(14))));
  t800 = ((int64_t)(v42));
  t801 = ((int64_t)(v40));
  t802 = ((int64_t)(((int64_t)UINT64_C(0))));
  t803 = fg107_out(t798, t799, t800, t801, t802);
  (void)(t803);
  t804 = ((int64_t)(v33));
  t805 = ((int64_t)(v29));
  t806 = ((int64_t)(v0));
  t807 = ((int64_t)(((int64_t)UINT64_C(15))));
  t808 = ((int64_t)(v42));
  t809 = ((int64_t)(((int64_t)UINT64_C(0))));
  t810 = ((int64_t)(((int64_t)UINT64_C(0))));
  t811 = fg107_out(t806, t807, t808, t809, t810);
  t812 = ((int64_t)(t811));
  t813 = ((int64_t)(v40));
  t814 = ((int64_t)(v43));
  t815 = ((int64_t)(((int64_t)((uint64_t)(t813) * (uint64_t)(t814)))));
  t816 = ((int64_t)(((int64_t)UINT64_C(4))));
  t817 = ((int64_t)(((int64_t)((uint64_t)(t815) * (uint64_t)(t816)))));
  t818 = ((int64_t)(((int64_t)((uint64_t)(t812) + (uint64_t)(t817)))));
  t819 = ((int64_t)(((int64_t)UINT64_C(0))));
  t820 = ((int64_t)(v40));
  t821 = ((int64_t)(((int64_t)UINT64_C(4))));
  t822 = ((int64_t)(((int64_t)((uint64_t)(t820) * (uint64_t)(t821)))));
  t823 = fg107_tensor_get_async(t804, t805, t818, t819, t822);
  (void)(t823);
  t824 = v41;
  t825 = ((int64_t)(((int64_t)UINT64_C(1))));
  v41 = ((int64_t)((uint64_t)(t824) + (uint64_t)(t825)));
  }
  goto label25;
label26:;
  }
  } else {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
  }
  }
  }
  v44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t826 = ((int64_t)(v0));
  t827 = ((int64_t)(v23));
  t828 = ((int64_t)(v17));
  t829 = ((int64_t)(v20));
  t830 = gg_function_31(t826, t827, t828, t829, base + 0);
  t831 = t830;
  if (t831) {
  {
  v44 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  {
  t832 = ((int64_t)(v0));
  t833 = ((int64_t)(((int64_t)UINT64_C(12))));
  t834 = ((int64_t)(((int64_t)UINT64_C(0))));
  t835 = fg107_ctx(t832, t833, t834);
  v45 = ((int64_t)(t835));
  t836 = ((int64_t)(v45));
  t837 = ((int64_t)(((int64_t)UINT64_C(0))));
  t838 = ((t836) != (t837));
  if (t838) {
  {
  t839 = ((int64_t)(v0));
  t840 = ((int64_t)(((int64_t)UINT64_C(25))));
  t841 = ((int64_t)(((int64_t)UINT64_C(0))));
  t842 = fg107_ctx(t839, t840, t841);
  v46 = ((int64_t)(t842));
  }
  } else {
  {
  v46 = ((int64_t)(v20));
  }
  }
  t843 = ((int64_t)(v45));
  t844 = ((int64_t)(((int64_t)UINT64_C(0))));
  t845 = ((t843) != (t844));
  if (t845) {
  {
  v47 = ((int64_t)(v16));
  }
  } else {
  {
  v47 = ((int64_t)(v17));
  }
  }
  t846 = ((int64_t)(v0));
  t847 = ((int64_t)(((int64_t)UINT64_C(7))));
  t848 = ((int64_t)(((int64_t)UINT64_C(0))));
  t849 = ((int64_t)(((int64_t)UINT64_C(0))));
  t850 = ((int64_t)(((int64_t)UINT64_C(0))));
  t851 = fg107_out(t846, t847, t848, t849, t850);
  t852 = ((int64_t)(t851));
  t853 = ((int64_t)(((int64_t)UINT64_C(0))));
  t854 = ((t852) != (t853));
  t855 = t854;
  if (t855) {
  t856 = ((int64_t)(v30));
  t857 = ((int64_t)(((int64_t)UINT64_C(0))));
  t855 = ((t856) != (t857));
  }
  t858 = t855;
  t859 = t858;
  if (t859) {
  t860 = ((int64_t)(v46));
  t861 = ((int64_t)(((int64_t)UINT64_C(0))));
  t859 = ((t860) > (t861));
  }
  t862 = t859;
  t863 = t862;
  if (t863) {
  t864 = ((int64_t)(v0));
  t865 = ((int64_t)(((int64_t)UINT64_C(18))));
  t866 = ((int64_t)(((int64_t)UINT64_C(0))));
  t867 = fg107_ctx(t864, t865, t866);
  t868 = ((int64_t)(t867));
  t869 = ((int64_t)(((int64_t)UINT64_C(0))));
  t863 = ((t868) == (t869));
  }
  t870 = t863;
  if (t870) {
  {
  t871 = ((int64_t)(v0));
  t872 = ((int64_t)(((int64_t)UINT64_C(4))));
  t873 = ((int64_t)(v30));
  t874 = ((int64_t)(((int64_t)UINT64_C(0))));
  t875 = ((int64_t)(((int64_t)UINT64_C(0))));
  t876 = fg107_sched(t871, t872, t873, t874, t875);
  v48 = ((int64_t)(t876));
  t877 = ((int64_t)(v48));
  t878 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t877) != (t878)))) gg_fail("assertion failed");
  t879 = ((int64_t)(v0));
  t880 = ((int64_t)(((int64_t)UINT64_C(8))));
  t881 = ((int64_t)(((int64_t)UINT64_C(0))));
  t882 = fg107_ctx(t879, t880, t881);
  v49 = ((int64_t)(t882));
  t883 = ((int64_t)(v47));
  t884 = ((int64_t)(v46));
  t885 = ((int64_t)(((int64_t)((uint64_t)(t883) + (uint64_t)(t884)))));
  t886 = ((int64_t)(v49));
  t887 = ((int64_t)(((int64_t)((uint64_t)(t885) * (uint64_t)(t886)))));
  t888 = ((int64_t)(v0));
  t889 = ((int64_t)(((int64_t)UINT64_C(8))));
  t890 = ((int64_t)(((int64_t)UINT64_C(0))));
  t891 = ((int64_t)(((int64_t)UINT64_C(0))));
  t892 = ((int64_t)(((int64_t)UINT64_C(0))));
  t893 = fg107_out(t888, t889, t890, t891, t892);
  t894 = ((int64_t)(t893));
  if (!(((t887) <= (t894)))) gg_fail("assertion failed");
  t895 = ((int64_t)(v48));
  t896 = ((int64_t)(v30));
  t897 = ((int64_t)(v0));
  t898 = ((int64_t)(((int64_t)UINT64_C(7))));
  t899 = ((int64_t)(((int64_t)UINT64_C(0))));
  t900 = ((int64_t)(((int64_t)UINT64_C(0))));
  t901 = ((int64_t)(((int64_t)UINT64_C(0))));
  t902 = fg107_out(t897, t898, t899, t900, t901);
  t903 = ((int64_t)(t902));
  t904 = ((int64_t)(v47));
  t905 = ((int64_t)(v49));
  t906 = ((int64_t)(((int64_t)((uint64_t)(t904) * (uint64_t)(t905)))));
  t907 = ((int64_t)(((int64_t)UINT64_C(4))));
  t908 = ((int64_t)(((int64_t)((uint64_t)(t906) * (uint64_t)(t907)))));
  t909 = ((int64_t)(((int64_t)((uint64_t)(t903) + (uint64_t)(t908)))));
  t910 = ((int64_t)(((int64_t)UINT64_C(0))));
  t911 = ((int64_t)(v46));
  t912 = ((int64_t)(v49));
  t913 = ((int64_t)(((int64_t)((uint64_t)(t911) * (uint64_t)(t912)))));
  t914 = ((int64_t)(((int64_t)UINT64_C(4))));
  t915 = ((int64_t)(((int64_t)((uint64_t)(t913) * (uint64_t)(t914)))));
  t916 = fg107_tensor_get_async(t895, t896, t909, t910, t915);
  (void)(t916);
  t917 = ((int64_t)(v45));
  t918 = ((int64_t)(((int64_t)UINT64_C(0))));
  t919 = ((t917) == (t918));
  if (t919) {
  {
  v44 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  }
  }
  t920 = ((int64_t)(v44));
  t921 = ((int64_t)(((int64_t)UINT64_C(0))));
  t922 = ((t920) != (t921));
  if (t922) {
  {
  t923 = ((int64_t)(v19));
  t924 = ((int64_t)(((int64_t)UINT64_C(8))));
  t925 = ((int64_t)(((int64_t)UINT64_C(0))));
  t926 = ((int64_t)(((int64_t)UINT64_C(0))));
  t927 = fg107_ub(t923, t924, t925, t926);
  t928 = ((int64_t)(t927));
  t929 = ((int64_t)(v20));
  if (!(((t928) == (t929)))) gg_fail("assertion failed");
  t930 = ((int64_t)(v0));
  t931 = ((int64_t)(((int64_t)UINT64_C(9))));
  t932 = ((int64_t)(((int64_t)UINT64_C(0))));
  t933 = ((int64_t)(((int64_t)UINT64_C(0))));
  t934 = ((int64_t)(((int64_t)UINT64_C(0))));
  t935 = fg107_exec(t930, t931, t932, t933, t934);
  t936 = ((int64_t)(t935));
  t937 = ((int64_t)(v17));
  if (!(((t936) == (t937)))) gg_fail("assertion failed");
  t938 = ((int64_t)(v0));
  t939 = ((int64_t)(((int64_t)UINT64_C(11))));
  t940 = ((int64_t)(v19));
  t941 = ((int64_t)(((int64_t)UINT64_C(0))));
  t942 = ((int64_t)(((int64_t)UINT64_C(0))));
  t943 = fg107_exec(t938, t939, t940, t941, t942);
  (void)(t943);
  }
  }
  t944 = ((int64_t)(v4));
  t945 = ((int64_t)(((int64_t)UINT64_C(0))));
  t946 = ((t944) != (t945));
  if (t946) {
  {
  t947 = ((int64_t)(v0));
  t948 = ((int64_t)(v23));
  t949 = ((int64_t)(((int64_t)UINT64_C(0))));
  t950 = ((int64_t)(((int64_t)UINT64_C(1))));
  t951 = ((int64_t)(v16));
  t952 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_33(t947, t948, t949, t950, t951, t952, base + 0);
  t953 = ((int64_t)(v0));
  t954 = ((int64_t)(v23));
  t955 = ((int64_t)(((int64_t)UINT64_C(1))));
  t956 = ((int64_t)(v0));
  t957 = ((int64_t)(((int64_t)UINT64_C(6))));
  t958 = ((int64_t)(((int64_t)UINT64_C(0))));
  t959 = fg107_ctx(t956, t957, t958);
  t960 = ((int64_t)(t959));
  t961 = ((int64_t)(v16));
  t962 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_33(t953, t954, t955, t960, t961, t962, base + 0);
  t963 = ((int64_t)(v0));
  t964 = ((int64_t)(v23));
  t965 = ((int64_t)(((int64_t)UINT64_C(2))));
  t966 = ((int64_t)(v0));
  t967 = ((int64_t)(((int64_t)UINT64_C(6))));
  t968 = ((int64_t)(((int64_t)UINT64_C(0))));
  t969 = fg107_ctx(t966, t967, t968);
  t970 = ((int64_t)(t969));
  t971 = ((int64_t)(v16));
  t972 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_33(t963, t964, t965, t970, t971, t972, base + 0);
  t973 = ((int64_t)(v0));
  t974 = ((int64_t)(v23));
  t975 = ((int64_t)(((int64_t)UINT64_C(3))));
  t976 = ((int64_t)(v0));
  t977 = ((int64_t)(((int64_t)UINT64_C(6))));
  t978 = ((int64_t)(((int64_t)UINT64_C(0))));
  t979 = fg107_ctx(t976, t977, t978);
  t980 = ((int64_t)(t979));
  t981 = ((int64_t)(v16));
  t982 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_33(t973, t974, t975, t980, t981, t982, base + 0);
  }
  }
  t983 = v16;
  t984 = ((int64_t)(v31));
  v16 = ((int64_t)((uint64_t)(t983) + (uint64_t)(t984)));
  t985 = v17;
  t986 = ((int64_t)(v20));
  v17 = ((int64_t)((uint64_t)(t985) + (uint64_t)(t986)));
  t987 = ((int64_t)(v13));
  t988 = ((int64_t)(((int64_t)UINT64_C(2))));
  t989 = ((int64_t)(((int64_t)UINT64_C(0))));
  t990 = fg107_mctx(t987, t988, t989);
  t991 = ((int64_t)(t990));
  t992 = ((int64_t)(((int64_t)UINT64_C(0))));
  t993 = ((t991) != (t992));
  if (t993) {
  {
  v18 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  }
  goto label10;
label11:;
  t994 = ((int64_t)(v0));
  t995 = ((int64_t)(((int64_t)UINT64_C(0))));
  t996 = ((int64_t)(v11));
  t997 = fg107_ctx_set(t994, t995, t996);
  (void)(t997);
  t998 = ((int64_t)(v11));
  t999 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1000 = ((t998) > (t999));
  if (t1000) {
  {
  t1001 = ((int64_t)(v0));
  t1002 = ((int64_t)(((int64_t)UINT64_C(3))));
  t1003 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1004 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1005 = fg107_balloc(t1001, t1002, t1003, t1004);
  t1006 = ((int64_t)(t1005));
  t1007 = ((int64_t)(v11));
  if (!(((t1006) == (t1007)))) gg_fail("assertion failed");
  v50 = ((int64_t)(((int64_t)UINT64_C(1))));
  v51 = ((int64_t)(((int64_t)UINT64_C(0))));
label28:;
  t1008 = ((int64_t)(v51));
  t1009 = ((int64_t)(v11));
  if (!(((t1008) < (t1009)))) goto label29;
  {
  t1010 = ((int64_t)(v0));
  t1011 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1012 = ((int64_t)(v51));
  t1013 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1014 = fg107_balloc(t1010, t1011, t1012, t1013);
  v52 = ((int64_t)(t1014));
  t1015 = ((int64_t)(v0));
  t1016 = ((int64_t)(((int64_t)UINT64_C(3))));
  t1017 = ((int64_t)(v52));
  t1018 = ((int64_t)(v51));
  t1019 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1020 = fg107_exec(t1015, t1016, t1017, t1018, t1019);
  (void)(t1020);
  t1021 = ((int64_t)(v52));
  t1022 = ((int64_t)(v51));
  t1023 = ((t1021) != (t1022));
  if (t1023) {
  {
  v50 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t1024 = v51;
  t1025 = ((int64_t)(((int64_t)UINT64_C(1))));
  v51 = ((int64_t)((uint64_t)(t1024) + (uint64_t)(t1025)));
  }
  goto label28;
label29:;
  t1026 = ((int64_t)(v50));
  t1027 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1028 = ((t1026) == (t1027));
  t1029 = t1028;
  if (t1029) {
  t1030 = ((int64_t)(v11));
  t1031 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1029 = ((t1030) > (t1031));
  }
  t1032 = t1029;
  if (t1032) {
  {
  v53 = ((int64_t)(((int64_t)UINT64_C(0))));
label31:;
  t1033 = ((int64_t)(v53));
  t1034 = ((int64_t)(v11));
  t1035 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1036 = ((int64_t)(((int64_t)((uint64_t)(t1034) - (uint64_t)(t1035)))));
  if (!(((t1033) < (t1036)))) goto label32;
  {
  v54 = ((int64_t)(v53));
  t1037 = ((int64_t)(v53));
  t1038 = ((int64_t)(((int64_t)UINT64_C(1))));
  v55 = ((int64_t)(((int64_t)((uint64_t)(t1037) + (uint64_t)(t1038)))));
label34:;
  t1039 = ((int64_t)(v55));
  t1040 = ((int64_t)(v11));
  if (!(((t1039) < (t1040)))) goto label35;
  {
  t1041 = ((int64_t)(v0));
  t1042 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1043 = ((int64_t)(v55));
  t1044 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1045 = fg107_balloc(t1041, t1042, t1043, t1044);
  t1046 = ((int64_t)(t1045));
  t1047 = ((int64_t)(v0));
  t1048 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1049 = ((int64_t)(v54));
  t1050 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1051 = fg107_balloc(t1047, t1048, t1049, t1050);
  t1052 = ((int64_t)(t1051));
  t1053 = ((t1046) < (t1052));
  if (t1053) {
  {
  v54 = ((int64_t)(v55));
  }
  }
  t1054 = v55;
  t1055 = ((int64_t)(((int64_t)UINT64_C(1))));
  v55 = ((int64_t)((uint64_t)(t1054) + (uint64_t)(t1055)));
  }
  goto label34;
label35:;
  t1056 = ((int64_t)(v54));
  t1057 = ((int64_t)(v53));
  t1058 = ((t1056) != (t1057));
  if (t1058) {
  {
  t1059 = ((int64_t)(v0));
  t1060 = ((int64_t)(((int64_t)UINT64_C(5))));
  t1061 = ((int64_t)(v53));
  t1062 = ((int64_t)(v54));
  t1063 = fg107_balloc(t1059, t1060, t1061, t1062);
  (void)(t1063);
  t1064 = ((int64_t)(v0));
  t1065 = ((int64_t)(((int64_t)UINT64_C(7))));
  t1066 = ((int64_t)(v53));
  t1067 = ((int64_t)(v54));
  t1068 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1069 = fg107_exec(t1064, t1065, t1066, t1067, t1068);
  (void)(t1069);
  }
  }
  t1070 = v53;
  t1071 = ((int64_t)(((int64_t)UINT64_C(1))));
  v53 = ((int64_t)((uint64_t)(t1070) + (uint64_t)(t1071)));
  }
  goto label31;
label32:;
  t1072 = ((int64_t)(v0));
  t1073 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1074 = ((int64_t)UINT64_C(1));
  t1075 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t1074)))));
  t1076 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1077 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1078 = fg107_exec(t1072, t1073, t1075, t1076, t1077);
  (void)(t1078);
  v56 = ((int64_t)(((int64_t)UINT64_C(0))));
label37:;
  t1079 = ((int64_t)(v56));
  t1080 = ((int64_t)(v11));
  if (!(((t1079) < (t1080)))) goto label38;
  {
  t1081 = ((int64_t)(v0));
  t1082 = ((int64_t)(((int64_t)UINT64_C(3))));
  t1083 = ((int64_t)(v0));
  t1084 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1085 = ((int64_t)(v56));
  t1086 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1087 = fg107_balloc(t1083, t1084, t1085, t1086);
  t1088 = ((int64_t)(t1087));
  t1089 = ((int64_t)(v56));
  t1090 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1091 = fg107_exec(t1081, t1082, t1088, t1089, t1090);
  (void)(t1091);
  t1092 = v56;
  t1093 = ((int64_t)(((int64_t)UINT64_C(1))));
  v56 = ((int64_t)((uint64_t)(t1092) + (uint64_t)(t1093)));
  }
  goto label37;
label38:;
  }
  }
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t0 = gg_function_2(t1, t2, base + 0);
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
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  gg_header t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t v0;
  int64_t v1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  v0 = gg_function_1(t0, t1, base + 0);
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_35(t2, base + 0);
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  v1 = gg_function_38(t3, t4, t5, t6, t7, base + 0);
  t8.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t8.len = 0;
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(v1));
  gg_integer(gg_ref(&t8, gg_get_stack(base + 0)), ((int64_t)((uint64_t)(t9) + (uint64_t)(t10))));
  gg_append(gg_ref(&t8, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t8).base, (t8).len));
  }
gg_done:;
  for (t11 = 0; t11 < 1; t11++) gg_stacks[base + t11].top = 0;
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
