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
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_13(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_18(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_19(int64_t p0, int64_t base);
static int64_t gg_function_20(int64_t p0, int64_t base);
static int64_t gg_function_21(int64_t p0, int64_t base);
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_23(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_28(int64_t p0, int64_t base);
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_30(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_31(int64_t p0, int64_t base);
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t base) {
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
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(7))));
  t2 = ((int64_t)(v0));
  t3 = fgc_i64_vec_data(t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(8))));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t7)))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fgcio_raw(t1, t8, t9, t10, t11);
  t0 = ((int64_t)(t12));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_19(int64_t p0, int64_t base) {
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
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(6))));
  t3 = fgc_record_get(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = fgc_vec_len(t4);
  v2 = ((int64_t)(t5));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(v2));
  if (!(((t6) < (t7)))) goto label2;
  {
  t8 = ((int64_t)(((int64_t)UINT64_C(7))));
  t9 = ((int64_t)(v1));
  t10 = fgc_i64_vec_data(t9);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(8))));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = fgcio_raw(t8, t15, t16, t17, t18);
  v4 = ((int64_t)(t19));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) != (t21));
  if (t22) {
  {
  t23 = ((int64_t)(v4));
  t24 = fgc_i64_record_free(t23);
  v5 = ((int64_t)(t24));
  }
  }
  t25 = ((int64_t)(v3));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  }
  goto label1;
label2:;
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = fgc_vec_resize(t27, t28);
  v6 = ((int64_t)(t29));
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = fgc_record_get(t30, t31);
  t33 = ((int64_t)(t32));
  t34 = ((int64_t)(((int64_t)UINT64_C(6))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(2))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = fgc_record_set(t36, t37, t38);
  v7 = ((int64_t)(t39));
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t10;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fgc_vec_len(t1);
  v2 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = fgc_vec_resize(t3, t6);
  v3 = ((int64_t)(t7));
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) != (t9));
  if (t10) {
  {
  t11 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t11)))));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(((int64_t)UINT64_C(8))));
  t13 = ((int64_t)(v0));
  t14 = fgc_i64_vec_data(t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(((int64_t)UINT64_C(8))));
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t18)))));
  t20 = ((int64_t)(v1));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fgcio_raw(t12, t19, t20, t21, t22);
  v4 = ((int64_t)(t23));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_30(int64_t p0, int64_t p1, int64_t base) {
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
  t2 = ((int64_t)(((int64_t)UINT64_C(7))));
  t3 = fgc_record_get(t1, t2);
  v2 = ((int64_t)(t3));
  t4 = ((int64_t)(v2));
  t5 = fgc_vec_len(t4);
  v3 = ((int64_t)(t5));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t6 = ((int64_t)(v4));
  t7 = ((int64_t)(v3));
  if (!(((t6) < (t7)))) goto label2;
  {
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(v4));
  v6 = gg_function_22(t8, t9, base + 0);
  t10 = ((int64_t)(v6));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fgc_record_get(t10, t11);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(v1));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  v5 = ((int64_t)(v6));
  }
  }
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v5));
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
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fgc_vec_len(t1);
  v2 = ((int64_t)(t2));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v2));
  if (!(((t3) < (t4)))) goto label2;
  {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(v3));
  t7 = gg_function_22(t5, t6, base + 0);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(v1));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  }
  goto label1;
label2:;
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v1));
  t0 = gg_function_26(t13, t14, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_28(int64_t p0, int64_t base) {
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
  uint8_t t41;
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
  uint8_t t69;
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
  uint8_t t99;
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
  uint8_t t135;
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
  uint8_t t151;
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
  uint8_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  uint8_t t177;
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
  uint8_t t232;
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
  uint8_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  uint8_t t286;
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
  int64_t t297;
  int64_t t298;
  int64_t t299;
  uint8_t t300;
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
  int64_t t317;
  int64_t t318;
  uint8_t t319;
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
  uint8_t t340;
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
  uint8_t t370;
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
  int64_t t383;
  int64_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  int64_t t388;
  int64_t t389;
  int64_t t390;
  int64_t t391;
  int64_t t392;
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
  int64_t v57;
  int64_t v58;
  int64_t v59;
  int64_t v60;
  int64_t v61;
  int64_t v62;
  int64_t v63;
  int64_t v64;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(6))));
  t3 = fgc_record_get(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = fgc_vec_len(t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(((int64_t)UINT64_C(25))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fgcio_raw(t6, t7, t8, t9, t10);
  v3 = ((int64_t)(t11));
  t12 = ((int64_t)(((int64_t)UINT64_C(8))));
  t13 = fgc_vec_new(t12);
  v4 = ((int64_t)(t13));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t14 = ((int64_t)(v5));
  t15 = ((int64_t)(v2));
  if (!(((t14) < (t15)))) goto label2;
  {
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(v5));
  v6 = gg_function_22(t16, t17, base + 0);
  t18 = ((int64_t)(v4));
  t19 = ((int64_t)(((int64_t)UINT64_C(12))));
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = fgc_record_get(t20, t21);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fgcio_raw(t19, t23, t24, t25, t26);
  t28 = ((int64_t)(t27));
  v7 = gg_function_29(t18, t28, base + 0);
  t29 = ((int64_t)(v5));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  }
  goto label1;
label2:;
  t31 = ((int64_t)(v4));
  t32 = fgc_vec_len(t31);
  v8 = ((int64_t)(t32));
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t33 = ((int64_t)(v9));
  t34 = ((int64_t)(v8));
  if (!(((t33) < (t34)))) goto label5;
  {
  t35 = ((int64_t)(v4));
  t36 = ((int64_t)(v9));
  v10 = gg_function_22(t35, t36, base + 0);
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(v10));
  v11 = gg_function_30(t37, t38, base + 0);
  t39 = ((int64_t)(v11));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  t42 = ((int64_t)(((int64_t)UINT64_C(34))));
  t43 = ((int64_t)(((int64_t)UINT64_C(1))));
  t44 = ((int64_t)(v10));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = fgcio_raw(t42, t43, t44, t45, t46);
  (void)(t47);
  }
  }
  t48 = ((int64_t)(v11));
  t49 = ((int64_t)(((int64_t)UINT64_C(1))));
  t50 = fgc_record_get(t48, t49);
  v12 = ((int64_t)(t50));
  t51 = ((int64_t)(v11));
  t52 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = fgc_record_get(t51, t52);
  v13 = ((int64_t)(t53));
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t54 = ((int64_t)(v15));
  t55 = ((int64_t)(v2));
  if (!(((t54) < (t55)))) goto label8;
  {
  t56 = ((int64_t)(((int64_t)UINT64_C(12))));
  t57 = ((int64_t)(v1));
  t58 = ((int64_t)(v15));
  t59 = gg_function_22(t57, t58, base + 0);
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = fgc_record_get(t59, t60);
  t62 = ((int64_t)(t61));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = fgcio_raw(t56, t62, t63, t64, t65);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(v10));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  t70 = ((int64_t)(v14));
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  }
  }
  t72 = ((int64_t)(v15));
  t73 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  }
  goto label7;
label8:;
  t74 = ((int64_t)(((int64_t)UINT64_C(21))));
  t75 = ((int64_t)(v14));
  t76 = ((int64_t)(v3));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t75) * (uint64_t)(t76)))));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = fgcio_raw(t74, t77, t78, t79, t80);
  v16 = ((int64_t)(t81));
  t82 = ((int64_t)(((int64_t)UINT64_C(8))));
  t83 = fgc_vec_new(t82);
  v17 = ((int64_t)(t83));
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t84 = ((int64_t)(v18));
  t85 = ((int64_t)(v2));
  if (!(((t84) < (t85)))) goto label11;
  {
  t86 = ((int64_t)(v1));
  t87 = ((int64_t)(v18));
  v19 = gg_function_22(t86, t87, base + 0);
  t88 = ((int64_t)(v19));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = fgc_record_get(t88, t89);
  v20 = ((int64_t)(t90));
  t91 = ((int64_t)(((int64_t)UINT64_C(12))));
  t92 = ((int64_t)(v20));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = fgcio_raw(t91, t92, t93, t94, t95);
  t97 = ((int64_t)(t96));
  t98 = ((int64_t)(v10));
  t99 = ((t97) == (t98));
  if (t99) {
  {
  t100 = ((int64_t)(v19));
  t101 = ((int64_t)(((int64_t)UINT64_C(2))));
  t102 = fgc_record_get(t100, t101);
  v21 = ((int64_t)(t102));
  t103 = ((int64_t)(((int64_t)UINT64_C(13))));
  t104 = ((int64_t)(v20));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = fgcio_raw(t103, t104, t105, t106, t107);
  v22 = ((int64_t)(t108));
  t109 = ((int64_t)(v21));
  t110 = ((int64_t)(v22));
  v23 = ((int64_t)(((int64_t)gg_div(t109, t110))));
  t111 = ((int64_t)(v19));
  t112 = ((int64_t)(((int64_t)UINT64_C(3))));
  t113 = fgc_record_get(t111, t112);
  v24 = ((int64_t)(t113));
  t114 = ((int64_t)(((int64_t)UINT64_C(23))));
  t115 = ((int64_t)(v16));
  t116 = ((int64_t)(v20));
  t117 = ((int64_t)(v23));
  t118 = ((int64_t)(v24));
  t119 = fgcio_raw(t114, t115, t116, t117, t118);
  v25 = ((int64_t)(t119));
  t120 = ((int64_t)(((int64_t)UINT64_C(19))));
  t121 = ((int64_t)(v25));
  t122 = ((int64_t)(((int64_t)UINT64_C(0))));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t125 = fgcio_raw(t120, t121, t122, t123, t124);
  v26 = ((int64_t)(t125));
  t126 = ((int64_t)(v17));
  t127 = ((int64_t)(v25));
  v27 = gg_function_26(t126, t127, base + 0);
  }
  }
  t128 = ((int64_t)(v18));
  t129 = ((int64_t)(((int64_t)UINT64_C(1))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t128) + (uint64_t)(t129)))));
  }
  goto label10;
label11:;
  t130 = ((int64_t)(v11));
  t131 = ((int64_t)(((int64_t)UINT64_C(4))));
  t132 = fgc_record_get(t130, t131);
  v28 = ((int64_t)(t132));
  t133 = ((int64_t)(v28));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((t133) == (t134));
  if (t135) {
  {
  t136 = ((int64_t)(((int64_t)UINT64_C(34))));
  t137 = ((int64_t)(((int64_t)UINT64_C(2))));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((int64_t)(((int64_t)UINT64_C(0))));
  t140 = ((int64_t)(((int64_t)UINT64_C(0))));
  t141 = fgcio_raw(t136, t137, t138, t139, t140);
  (void)(t141);
  }
  }
  t142 = ((int64_t)(v11));
  t143 = ((int64_t)(((int64_t)UINT64_C(6))));
  t144 = fgc_record_get(t142, t143);
  v29 = ((int64_t)(t144));
  t145 = ((int64_t)(v29));
  t146 = fgc_vec_len(t145);
  v30 = ((int64_t)(t146));
  t147 = ((int64_t)(v17));
  t148 = fgc_vec_len(t147);
  v31 = ((int64_t)(t148));
  v32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t149 = ((int64_t)(v12));
  t150 = ((int64_t)(v31));
  t151 = ((t149) == (t150));
  if (t151) {
  {
  v32 = ((int64_t)(((int64_t)UINT64_C(1))));
  v33 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t152 = ((int64_t)(v33));
  t153 = ((int64_t)(v31));
  if (!(((t152) < (t153)))) goto label14;
  {
  t154 = ((int64_t)(((int64_t)UINT64_C(15))));
  t155 = ((int64_t)(v29));
  t156 = ((int64_t)(v33));
  t157 = gg_function_22(t155, t156, base + 0);
  t158 = ((int64_t)(((int64_t)UINT64_C(0))));
  t159 = ((int64_t)(((int64_t)UINT64_C(0))));
  t160 = ((int64_t)(((int64_t)UINT64_C(0))));
  t161 = fgcio_raw(t154, t157, t158, t159, t160);
  t162 = ((int64_t)(t161));
  t163 = ((int64_t)(((int64_t)UINT64_C(15))));
  t164 = ((int64_t)(v17));
  t165 = ((int64_t)(v33));
  t166 = gg_function_22(t164, t165, base + 0);
  t167 = ((int64_t)(((int64_t)UINT64_C(0))));
  t168 = ((int64_t)(((int64_t)UINT64_C(0))));
  t169 = ((int64_t)(((int64_t)UINT64_C(0))));
  t170 = fgcio_raw(t163, t166, t167, t168, t169);
  t171 = ((int64_t)(t170));
  t172 = ((t162) != (t171));
  if (t172) {
  {
  v32 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t173 = ((int64_t)(v33));
  t174 = ((int64_t)(((int64_t)UINT64_C(1))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t173) + (uint64_t)(t174)))));
  }
  goto label13;
label14:;
  }
  }
  t175 = ((int64_t)(v32));
  t176 = ((int64_t)(((int64_t)UINT64_C(0))));
  t177 = ((t175) != (t176));
  if (t177) {
  {
  v34 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t178 = ((int64_t)(v34));
  t179 = ((int64_t)(v31));
  if (!(((t178) < (t179)))) goto label17;
  {
  t180 = ((int64_t)(((int64_t)UINT64_C(20))));
  t181 = ((int64_t)(v29));
  t182 = ((int64_t)(v34));
  t183 = gg_function_22(t181, t182, base + 0);
  t184 = ((int64_t)(v17));
  t185 = ((int64_t)(v34));
  t186 = gg_function_22(t184, t185, base + 0);
  t187 = ((int64_t)(((int64_t)UINT64_C(0))));
  t188 = ((int64_t)(((int64_t)UINT64_C(0))));
  t189 = fgcio_raw(t180, t183, t186, t187, t188);
  v35 = ((int64_t)(t189));
  t190 = ((int64_t)(v34));
  t191 = ((int64_t)(((int64_t)UINT64_C(1))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t190) + (uint64_t)(t191)))));
  }
  goto label16;
label17:;
  }
  } else {
  {
  t192 = ((int64_t)(((int64_t)UINT64_C(21))));
  t193 = ((int64_t)(((int64_t)UINT64_C(2))));
  t194 = ((int64_t)(v30));
  t195 = ((int64_t)(v31));
  t196 = ((int64_t)(((int64_t)((uint64_t)(t194) + (uint64_t)(t195)))));
  t197 = ((int64_t)(((int64_t)((uint64_t)(t193) * (uint64_t)(t196)))));
  t198 = ((int64_t)(v3));
  t199 = ((int64_t)(((int64_t)((uint64_t)(t197) * (uint64_t)(t198)))));
  t200 = ((int64_t)(((int64_t)UINT64_C(0))));
  t201 = ((int64_t)(((int64_t)UINT64_C(0))));
  t202 = ((int64_t)(((int64_t)UINT64_C(0))));
  t203 = fgcio_raw(t192, t199, t200, t201, t202);
  v36 = ((int64_t)(t203));
  v37 = ((int64_t)(((int64_t)UINT64_C(0))));
  v38 = ((int64_t)(((int64_t)UINT64_C(0))));
  v39 = ((int64_t)(((int64_t)UINT64_C(0))));
  v40 = ((int64_t)(((int64_t)UINT64_C(0))));
  v41 = ((int64_t)(((int64_t)UINT64_C(0))));
  v42 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t204 = ((int64_t)(v37));
  t205 = ((int64_t)(v13));
  if (!(((t204) < (t205)))) goto label20;
  {
  t206 = ((int64_t)(v29));
  t207 = ((int64_t)(v39));
  v43 = gg_function_22(t206, t207, base + 0);
  t208 = ((int64_t)(v17));
  t209 = ((int64_t)(v40));
  v44 = gg_function_22(t208, t209, base + 0);
  t210 = ((int64_t)(((int64_t)UINT64_C(15))));
  t211 = ((int64_t)(v43));
  t212 = ((int64_t)(((int64_t)UINT64_C(0))));
  t213 = ((int64_t)(((int64_t)UINT64_C(0))));
  t214 = ((int64_t)(((int64_t)UINT64_C(0))));
  t215 = fgcio_raw(t210, t211, t212, t213, t214);
  v45 = ((int64_t)(t215));
  t216 = ((int64_t)(((int64_t)UINT64_C(15))));
  t217 = ((int64_t)(v44));
  t218 = ((int64_t)(((int64_t)UINT64_C(0))));
  t219 = ((int64_t)(((int64_t)UINT64_C(0))));
  t220 = ((int64_t)(((int64_t)UINT64_C(0))));
  t221 = fgcio_raw(t216, t217, t218, t219, t220);
  v46 = ((int64_t)(t221));
  t222 = ((int64_t)(v37));
  t223 = ((int64_t)(v41));
  v47 = ((int64_t)(((int64_t)((uint64_t)(t222) - (uint64_t)(t223)))));
  t224 = ((int64_t)(v38));
  t225 = ((int64_t)(v42));
  v48 = ((int64_t)(((int64_t)((uint64_t)(t224) - (uint64_t)(t225)))));
  t226 = ((int64_t)(v45));
  t227 = ((int64_t)(v47));
  v49 = ((int64_t)(((int64_t)((uint64_t)(t226) - (uint64_t)(t227)))));
  t228 = ((int64_t)(v46));
  t229 = ((int64_t)(v48));
  v50 = ((int64_t)(((int64_t)((uint64_t)(t228) - (uint64_t)(t229)))));
  t230 = ((int64_t)(v50));
  t231 = ((int64_t)(v49));
  t232 = ((t230) < (t231));
  if (t232) {
  {
  v49 = ((int64_t)(v50));
  }
  }
  t233 = ((int64_t)(((int64_t)UINT64_C(13))));
  t234 = ((int64_t)(v43));
  t235 = ((int64_t)(((int64_t)UINT64_C(0))));
  t236 = ((int64_t)(((int64_t)UINT64_C(0))));
  t237 = ((int64_t)(((int64_t)UINT64_C(0))));
  t238 = fgcio_raw(t233, t234, t235, t236, t237);
  v51 = ((int64_t)(t238));
  t239 = ((int64_t)(v49));
  t240 = ((int64_t)(v51));
  v52 = ((int64_t)(((int64_t)gg_div(t239, t240))));
  t241 = ((int64_t)(((int64_t)UINT64_C(23))));
  t242 = ((int64_t)(v36));
  t243 = ((int64_t)(v43));
  t244 = ((int64_t)(v52));
  t245 = ((int64_t)(v47));
  t246 = fgcio_raw(t241, t242, t243, t244, t245);
  v53 = ((int64_t)(t246));
  t247 = ((int64_t)(((int64_t)UINT64_C(19))));
  t248 = ((int64_t)(v53));
  t249 = ((int64_t)(((int64_t)UINT64_C(0))));
  t250 = ((int64_t)(((int64_t)UINT64_C(0))));
  t251 = ((int64_t)(((int64_t)UINT64_C(0))));
  t252 = fgcio_raw(t247, t248, t249, t250, t251);
  v54 = ((int64_t)(t252));
  t253 = ((int64_t)(((int64_t)UINT64_C(23))));
  t254 = ((int64_t)(v36));
  t255 = ((int64_t)(v44));
  t256 = ((int64_t)(v52));
  t257 = ((int64_t)(v48));
  t258 = fgcio_raw(t253, t254, t255, t256, t257);
  v55 = ((int64_t)(t258));
  t259 = ((int64_t)(((int64_t)UINT64_C(19))));
  t260 = ((int64_t)(v55));
  t261 = ((int64_t)(((int64_t)UINT64_C(0))));
  t262 = ((int64_t)(((int64_t)UINT64_C(0))));
  t263 = ((int64_t)(((int64_t)UINT64_C(0))));
  t264 = fgcio_raw(t259, t260, t261, t262, t263);
  v56 = ((int64_t)(t264));
  t265 = ((int64_t)(((int64_t)UINT64_C(20))));
  t266 = ((int64_t)(v53));
  t267 = ((int64_t)(v55));
  t268 = ((int64_t)(((int64_t)UINT64_C(0))));
  t269 = ((int64_t)(((int64_t)UINT64_C(0))));
  t270 = fgcio_raw(t265, t266, t267, t268, t269);
  v57 = ((int64_t)(t270));
  t271 = ((int64_t)(v37));
  t272 = ((int64_t)(v49));
  v37 = ((int64_t)(((int64_t)((uint64_t)(t271) + (uint64_t)(t272)))));
  t273 = ((int64_t)(v38));
  t274 = ((int64_t)(v49));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t273) + (uint64_t)(t274)))));
  t275 = ((int64_t)(v37));
  t276 = ((int64_t)(v41));
  t277 = ((int64_t)(((int64_t)((uint64_t)(t275) - (uint64_t)(t276)))));
  t278 = ((int64_t)(v45));
  t279 = ((t277) == (t278));
  if (t279) {
  {
  v41 = ((int64_t)(v37));
  t280 = ((int64_t)(v39));
  t281 = ((int64_t)(((int64_t)UINT64_C(1))));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t280) + (uint64_t)(t281)))));
  }
  }
  t282 = ((int64_t)(v38));
  t283 = ((int64_t)(v42));
  t284 = ((int64_t)(((int64_t)((uint64_t)(t282) - (uint64_t)(t283)))));
  t285 = ((int64_t)(v46));
  t286 = ((t284) == (t285));
  if (t286) {
  {
  v42 = ((int64_t)(v38));
  t287 = ((int64_t)(v40));
  t288 = ((int64_t)(((int64_t)UINT64_C(1))));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t287) + (uint64_t)(t288)))));
  }
  }
  }
  goto label19;
label20:;
  t289 = ((int64_t)(v37));
  t290 = ((int64_t)(v13));
  t291 = ((t289) != (t290));
  if (t291) {
  {
  t292 = ((int64_t)(((int64_t)UINT64_C(34))));
  t293 = ((int64_t)(((int64_t)UINT64_C(3))));
  t294 = ((int64_t)(v37));
  t295 = ((int64_t)(v13));
  t296 = ((int64_t)(((int64_t)UINT64_C(0))));
  t297 = fgcio_raw(t292, t293, t294, t295, t296);
  (void)(t297);
  }
  }
  t298 = ((int64_t)(v38));
  t299 = ((int64_t)(v13));
  t300 = ((t298) != (t299));
  if (t300) {
  {
  t301 = ((int64_t)(((int64_t)UINT64_C(34))));
  t302 = ((int64_t)(((int64_t)UINT64_C(4))));
  t303 = ((int64_t)(v38));
  t304 = ((int64_t)(v13));
  t305 = ((int64_t)(((int64_t)UINT64_C(0))));
  t306 = fgcio_raw(t301, t302, t303, t304, t305);
  (void)(t306);
  }
  }
label22:;
  t307 = ((int64_t)(v39));
  t308 = ((int64_t)(v30));
  if (!(((t307) < (t308)))) goto label23;
  {
  t309 = ((int64_t)(((int64_t)UINT64_C(15))));
  t310 = ((int64_t)(v29));
  t311 = ((int64_t)(v39));
  t312 = gg_function_22(t310, t311, base + 0);
  t313 = ((int64_t)(((int64_t)UINT64_C(0))));
  t314 = ((int64_t)(((int64_t)UINT64_C(0))));
  t315 = ((int64_t)(((int64_t)UINT64_C(0))));
  t316 = fgcio_raw(t309, t312, t313, t314, t315);
  t317 = ((int64_t)(t316));
  t318 = ((int64_t)(((int64_t)UINT64_C(0))));
  t319 = ((t317) != (t318));
  if (t319) {
  {
  t320 = ((int64_t)(((int64_t)UINT64_C(34))));
  t321 = ((int64_t)(((int64_t)UINT64_C(5))));
  t322 = ((int64_t)(v39));
  t323 = ((int64_t)(((int64_t)UINT64_C(0))));
  t324 = ((int64_t)(((int64_t)UINT64_C(0))));
  t325 = fgcio_raw(t320, t321, t322, t323, t324);
  (void)(t325);
  }
  }
  t326 = ((int64_t)(v39));
  t327 = ((int64_t)(((int64_t)UINT64_C(1))));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t326) + (uint64_t)(t327)))));
  }
  goto label22;
label23:;
label25:;
  t328 = ((int64_t)(v40));
  t329 = ((int64_t)(v31));
  if (!(((t328) < (t329)))) goto label26;
  {
  t330 = ((int64_t)(((int64_t)UINT64_C(15))));
  t331 = ((int64_t)(v17));
  t332 = ((int64_t)(v40));
  t333 = gg_function_22(t331, t332, base + 0);
  t334 = ((int64_t)(((int64_t)UINT64_C(0))));
  t335 = ((int64_t)(((int64_t)UINT64_C(0))));
  t336 = ((int64_t)(((int64_t)UINT64_C(0))));
  t337 = fgcio_raw(t330, t333, t334, t335, t336);
  t338 = ((int64_t)(t337));
  t339 = ((int64_t)(((int64_t)UINT64_C(0))));
  t340 = ((t338) != (t339));
  if (t340) {
  {
  t341 = ((int64_t)(((int64_t)UINT64_C(34))));
  t342 = ((int64_t)(((int64_t)UINT64_C(6))));
  t343 = ((int64_t)(v40));
  t344 = ((int64_t)(((int64_t)UINT64_C(0))));
  t345 = ((int64_t)(((int64_t)UINT64_C(0))));
  t346 = fgcio_raw(t341, t342, t343, t344, t345);
  (void)(t346);
  }
  }
  t347 = ((int64_t)(v40));
  t348 = ((int64_t)(((int64_t)UINT64_C(1))));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t347) + (uint64_t)(t348)))));
  }
  goto label25;
label26:;
  t349 = ((int64_t)(((int64_t)UINT64_C(22))));
  t350 = ((int64_t)(v36));
  t351 = ((int64_t)(((int64_t)UINT64_C(0))));
  t352 = ((int64_t)(((int64_t)UINT64_C(0))));
  t353 = ((int64_t)(((int64_t)UINT64_C(0))));
  t354 = fgcio_raw(t349, t350, t351, t352, t353);
  v58 = ((int64_t)(t354));
  }
  }
  t355 = ((int64_t)(((int64_t)UINT64_C(22))));
  t356 = ((int64_t)(v16));
  t357 = ((int64_t)(((int64_t)UINT64_C(0))));
  t358 = ((int64_t)(((int64_t)UINT64_C(0))));
  t359 = ((int64_t)(((int64_t)UINT64_C(0))));
  t360 = fgcio_raw(t355, t356, t357, t358, t359);
  v59 = ((int64_t)(t360));
  t361 = ((int64_t)(v17));
  t362 = fgc_i64_vec_free(t361);
  v60 = ((int64_t)(t362));
  t363 = ((int64_t)(v9));
  t364 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t363) + (uint64_t)(t364)))));
  }
  goto label4;
label5:;
  t365 = ((int64_t)(v0));
  t366 = ((int64_t)(((int64_t)UINT64_C(2))));
  t367 = fgc_record_get(t365, t366);
  t368 = ((int64_t)(t367));
  t369 = ((int64_t)(((int64_t)UINT64_C(0))));
  t370 = ((t368) != (t369));
  if (t370) {
  {
  t371 = ((int64_t)(((int64_t)UINT64_C(34))));
  t372 = ((int64_t)(((int64_t)UINT64_C(7))));
  t373 = ((int64_t)(v0));
  t374 = ((int64_t)(((int64_t)UINT64_C(2))));
  t375 = fgc_record_get(t373, t374);
  t376 = ((int64_t)(t375));
  t377 = ((int64_t)(((int64_t)UINT64_C(0))));
  t378 = ((int64_t)(((int64_t)UINT64_C(0))));
  t379 = fgcio_raw(t371, t372, t376, t377, t378);
  (void)(t379);
  }
  }
  v61 = ((int64_t)(((int64_t)UINT64_C(0))));
label28:;
  t380 = ((int64_t)(v61));
  t381 = ((int64_t)(v2));
  if (!(((t380) < (t381)))) goto label29;
  {
  t382 = ((int64_t)(v1));
  t383 = ((int64_t)(v61));
  v62 = gg_function_22(t382, t383, base + 0);
  t384 = ((int64_t)(v62));
  t385 = fgc_i64_record_free(t384);
  v63 = ((int64_t)(t385));
  t386 = ((int64_t)(v61));
  t387 = ((int64_t)(((int64_t)UINT64_C(1))));
  v61 = ((int64_t)(((int64_t)((uint64_t)(t386) + (uint64_t)(t387)))));
  }
  goto label28;
label29:;
  t388 = ((int64_t)(v1));
  t389 = ((int64_t)(((int64_t)UINT64_C(0))));
  t390 = fgc_vec_resize(t388, t389);
  v64 = ((int64_t)(t390));
  t391 = ((int64_t)(v4));
  t392 = fgc_i64_vec_free(t391);
  (void)(t392);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_23(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t69;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(12))));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fgcio_raw(t1, t2, t3, t4, t5);
  v3 = ((int64_t)(t6));
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(8))));
  t9 = fgc_record_get(t7, t8);
  v4 = ((int64_t)(t9));
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(7))));
  t12 = fgc_record_get(t10, t11);
  v5 = ((int64_t)(t12));
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(v3));
  t15 = fgc_set_contains(t13, t14);
  t16 = ((int64_t)(t15));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(v3));
  t21 = fgc_set_insert(t19, t20);
  v6 = ((int64_t)(t21));
  t22 = ((int64_t)(((int64_t)UINT64_C(3103))));
  t23 = fgc_record_new(t22);
  v7 = ((int64_t)(t23));
  t24 = ((int64_t)(v7));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(v3));
  t27 = fgc_record_set(t24, t25, t26);
  v8 = ((int64_t)(t27));
  t28 = ((int64_t)(v7));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fgc_record_set(t28, t29, t30);
  v9 = ((int64_t)(t31));
  t32 = ((int64_t)(v7));
  t33 = ((int64_t)(((int64_t)UINT64_C(2))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fgc_record_set(t32, t33, t34);
  v10 = ((int64_t)(t35));
  t36 = ((int64_t)(v7));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = fgc_record_set(t36, t37, t38);
  v11 = ((int64_t)(t39));
  t40 = ((int64_t)(v7));
  t41 = ((int64_t)(((int64_t)UINT64_C(4))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = fgc_record_set(t40, t41, t42);
  v12 = ((int64_t)(t43));
  t44 = ((int64_t)(((int64_t)UINT64_C(8))));
  t45 = fgc_vec_new(t44);
  v13 = ((int64_t)(t45));
  t46 = ((int64_t)(((int64_t)UINT64_C(8))));
  t47 = fgc_vec_new(t46);
  v14 = ((int64_t)(t47));
  t48 = ((int64_t)(v7));
  t49 = ((int64_t)(((int64_t)UINT64_C(5))));
  t50 = ((int64_t)(v13));
  t51 = fgc_record_set(t48, t49, t50);
  v15 = ((int64_t)(t51));
  t52 = ((int64_t)(v7));
  t53 = ((int64_t)(((int64_t)UINT64_C(6))));
  t54 = ((int64_t)(v14));
  t55 = fgc_record_set(t52, t53, t54);
  v16 = ((int64_t)(t55));
  t56 = ((int64_t)(v5));
  t57 = ((int64_t)(v7));
  v17 = gg_function_26(t56, t57, base + 0);
  }
  }
  t58 = ((int64_t)(v5));
  t59 = fgc_vec_len(t58);
  v18 = ((int64_t)(t59));
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t60 = ((int64_t)(v20));
  t61 = ((int64_t)(v18));
  if (!(((t60) < (t61)))) goto label2;
  {
  t62 = ((int64_t)(v5));
  t63 = ((int64_t)(v20));
  v21 = gg_function_22(t62, t63, base + 0);
  t64 = ((int64_t)(v21));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = fgc_record_get(t64, t65);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(v3));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  v19 = ((int64_t)(v20));
  }
  }
  t70 = ((int64_t)(v20));
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  }
  goto label1;
label2:;
  t72 = ((int64_t)(v5));
  t73 = ((int64_t)(v19));
  v22 = gg_function_22(t72, t73, base + 0);
  t74 = ((int64_t)(v22));
  t75 = ((int64_t)(((int64_t)UINT64_C(7))));
  t76 = ((int64_t)(v22));
  t77 = ((int64_t)(((int64_t)UINT64_C(7))));
  t78 = fgc_record_get(t76, t77);
  t79 = ((int64_t)(t78));
  t80 = ((int64_t)(((int64_t)UINT64_C(1))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  t82 = fgc_record_set(t74, t75, t81);
  v23 = ((int64_t)(t82));
  t83 = ((int64_t)(v22));
  t84 = ((int64_t)(((int64_t)UINT64_C(8))));
  t85 = ((int64_t)(v22));
  t86 = ((int64_t)(((int64_t)UINT64_C(8))));
  t87 = fgc_record_get(t85, t86);
  t88 = ((int64_t)(t87));
  t89 = ((int64_t)(v2));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))));
  t91 = fgc_record_set(t83, t84, t90);
  v24 = ((int64_t)(t91));
  t0 = ((int64_t)(v22));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_21(int64_t p0, int64_t base) {
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
  uint8_t t63;
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
  uint8_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
  int64_t t147;
  int64_t t148;
  uint8_t t149;
  int64_t t150;
  int64_t t151;
  uint8_t t152;
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
  uint8_t t170;
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
  uint8_t t185;
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
  uint8_t t200;
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
  uint8_t t221;
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
  uint8_t t242;
  uint8_t t243;
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
  uint8_t t255;
  int64_t t256;
  int64_t t257;
  uint8_t t258;
  int64_t t259;
  int64_t t260;
  uint8_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  uint8_t t267;
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
  uint8_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  uint8_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
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
  uint8_t t340;
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
  uint8_t t363;
  int64_t t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  uint8_t t368;
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
  int64_t t383;
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
  int64_t t453;
  int64_t t454;
  int64_t t455;
  int64_t t456;
  int64_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  int64_t t461;
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
  int64_t v57;
  int64_t v58;
  int64_t v59;
  int64_t v60;
  int64_t v61;
  int64_t v62;
  int64_t v63;
  int64_t v64;
  int64_t v65;
  int64_t v66;
  int64_t v67;
  int64_t v68;
  int64_t v69;
  int64_t v70;
  int64_t v71;
  int64_t v72;
  int64_t v73;
  int64_t v74;
  int64_t v75;
  int64_t v76;
  int64_t v77;
  int64_t v78;
  int64_t v79;
  int64_t v80;
  int64_t v81;
  int64_t v82;
  int64_t v83;
  int64_t v84;
  int64_t v85;
  int64_t v86;
  int64_t v87;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(6))));
  t3 = fgc_record_get(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = fgc_vec_len(t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(7))));
  t8 = fgc_record_get(t6, t7);
  v3 = ((int64_t)(t8));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(v3));
  t11 = fgc_vec_len(t10);
  t12 = ((int64_t)(t11));
  if (!(((t9) < (t12)))) goto label2;
  {
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(v4));
  v5 = gg_function_22(t13, t14, base + 0);
  t15 = ((int64_t)(v5));
  t16 = ((int64_t)(((int64_t)UINT64_C(7))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fgc_record_set(t15, t16, t17);
  (void)(t18);
  t19 = ((int64_t)(v5));
  t20 = ((int64_t)(((int64_t)UINT64_C(8))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = fgc_record_set(t19, t20, t21);
  (void)(t22);
  t23 = ((int64_t)(v4));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))));
  }
  goto label1;
label2:;
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t25 = ((int64_t)(v6));
  t26 = ((int64_t)(v2));
  if (!(((t25) < (t26)))) goto label5;
  {
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(v6));
  v7 = gg_function_22(t27, t28, base + 0);
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(v7));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = fgc_record_get(t30, t31);
  t33 = ((int64_t)(t32));
  t34 = ((int64_t)(v7));
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t36 = fgc_record_get(t34, t35);
  t37 = ((int64_t)(t36));
  v8 = gg_function_23(t29, t33, t37, base + 0);
  t38 = ((int64_t)(v6));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  }
  goto label4;
label5:;
  t40 = ((int64_t)(((int64_t)UINT64_C(25))));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = fgcio_raw(t40, t41, t42, t43, t44);
  v9 = ((int64_t)(t45));
  t46 = ((int64_t)(v3));
  t47 = fgc_vec_len(t46);
  v10 = ((int64_t)(t47));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t48 = ((int64_t)(v11));
  t49 = ((int64_t)(v10));
  if (!(((t48) < (t49)))) goto label8;
  {
  t50 = ((int64_t)(v3));
  t51 = ((int64_t)(v11));
  v12 = gg_function_22(t50, t51, base + 0);
  t52 = ((int64_t)(v12));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = fgc_record_get(t52, t53);
  v13 = ((int64_t)(t54));
  t55 = ((int64_t)(v12));
  t56 = ((int64_t)(((int64_t)UINT64_C(7))));
  t57 = fgc_record_get(t55, t56);
  v14 = ((int64_t)(t57));
  t58 = ((int64_t)(v12));
  t59 = ((int64_t)(((int64_t)UINT64_C(8))));
  t60 = fgc_record_get(t58, t59);
  v15 = ((int64_t)(t60));
  t61 = ((int64_t)(v14));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((t61) > (t62));
  if (t63) {
  {
  t64 = ((int64_t)(((int64_t)UINT64_C(21))));
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t66 = ((int64_t)(v14));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  t68 = ((int64_t)(v9));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = fgcio_raw(t64, t69, t70, t71, t72);
  v16 = ((int64_t)(t73));
  t74 = ((int64_t)(((int64_t)UINT64_C(8))));
  t75 = fgc_vec_new(t74);
  v17 = ((int64_t)(t75));
  t76 = ((int64_t)(((int64_t)UINT64_C(8))));
  t77 = fgc_vec_new(t76);
  v18 = ((int64_t)(t77));
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t78 = ((int64_t)(v19));
  t79 = ((int64_t)(v2));
  if (!(((t78) < (t79)))) goto label11;
  {
  t80 = ((int64_t)(v1));
  t81 = ((int64_t)(v19));
  v20 = gg_function_22(t80, t81, base + 0);
  t82 = ((int64_t)(v20));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = fgc_record_get(t82, t83);
  v21 = ((int64_t)(t84));
  t85 = ((int64_t)(((int64_t)UINT64_C(12))));
  t86 = ((int64_t)(v21));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = fgcio_raw(t85, t86, t87, t88, t89);
  t91 = ((int64_t)(t90));
  t92 = ((int64_t)(v13));
  t93 = ((t91) == (t92));
  if (t93) {
  {
  t94 = ((int64_t)(v20));
  t95 = ((int64_t)(((int64_t)UINT64_C(2))));
  t96 = fgc_record_get(t94, t95);
  v22 = ((int64_t)(t96));
  t97 = ((int64_t)(((int64_t)UINT64_C(13))));
  t98 = ((int64_t)(v21));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = fgcio_raw(t97, t98, t99, t100, t101);
  v23 = ((int64_t)(t102));
  t103 = ((int64_t)(v22));
  t104 = ((int64_t)(v23));
  v24 = ((int64_t)(((int64_t)gg_div(t103, t104))));
  t105 = ((int64_t)(v20));
  t106 = ((int64_t)(((int64_t)UINT64_C(3))));
  t107 = fgc_record_get(t105, t106);
  v25 = ((int64_t)(t107));
  t108 = ((int64_t)(((int64_t)UINT64_C(23))));
  t109 = ((int64_t)(v16));
  t110 = ((int64_t)(v21));
  t111 = ((int64_t)(v24));
  t112 = ((int64_t)(v25));
  t113 = fgcio_raw(t108, t109, t110, t111, t112);
  v26 = ((int64_t)(t113));
  t114 = ((int64_t)(((int64_t)UINT64_C(24))));
  t115 = ((int64_t)(v16));
  t116 = ((int64_t)(((int64_t)UINT64_C(14))));
  t117 = ((int64_t)(v21));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = fgcio_raw(t116, t117, t118, t119, t120);
  t122 = ((int64_t)(t121));
  t123 = ((int64_t)(v24));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t125 = fgcio_raw(t114, t115, t122, t123, t124);
  v27 = ((int64_t)(t125));
  t126 = ((int64_t)(v17));
  t127 = ((int64_t)(v26));
  v28 = gg_function_26(t126, t127, base + 0);
  t128 = ((int64_t)(v18));
  t129 = ((int64_t)(v27));
  v29 = gg_function_26(t128, t129, base + 0);
  }
  }
  t130 = ((int64_t)(v19));
  t131 = ((int64_t)(((int64_t)UINT64_C(1))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t130) + (uint64_t)(t131)))));
  }
  goto label10;
label11:;
  t132 = ((int64_t)(v12));
  t133 = ((int64_t)(((int64_t)UINT64_C(4))));
  t134 = fgc_record_get(t132, t133);
  v30 = ((int64_t)(t134));
  t135 = ((int64_t)(v12));
  t136 = ((int64_t)(((int64_t)UINT64_C(1))));
  t137 = fgc_record_get(t135, t136);
  v31 = ((int64_t)(t137));
  t138 = ((int64_t)(v12));
  t139 = ((int64_t)(((int64_t)UINT64_C(2))));
  t140 = fgc_record_get(t138, t139);
  v32 = ((int64_t)(t140));
  v33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t141 = ((int64_t)(v30));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = ((t141) == (t142));
  if (t143) {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t144 = ((int64_t)(v31));
  t145 = ((int64_t)(v14));
  t146 = ((t144) != (t145));
  if (t146) {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t147 = ((int64_t)(v32));
  t148 = ((int64_t)(v15));
  t149 = ((t147) != (t148));
  if (t149) {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t150 = ((int64_t)(v33));
  t151 = ((int64_t)(((int64_t)UINT64_C(0))));
  t152 = ((t150) == (t151));
  if (t152) {
  {
  t153 = ((int64_t)(v12));
  t154 = ((int64_t)(((int64_t)UINT64_C(5))));
  t155 = fgc_record_get(t153, t154);
  v34 = ((int64_t)(t155));
  v35 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t156 = ((int64_t)(v35));
  t157 = ((int64_t)(v14));
  if (!(((t156) < (t157)))) goto label14;
  {
  t158 = ((int64_t)(v34));
  t159 = ((int64_t)(v35));
  v36 = gg_function_22(t158, t159, base + 0);
  t160 = ((int64_t)(v17));
  t161 = ((int64_t)(v35));
  v37 = gg_function_22(t160, t161, base + 0);
  t162 = ((int64_t)(((int64_t)UINT64_C(16))));
  t163 = ((int64_t)(v36));
  t164 = ((int64_t)(v37));
  t165 = ((int64_t)(((int64_t)UINT64_C(0))));
  t166 = ((int64_t)(((int64_t)UINT64_C(0))));
  t167 = fgcio_raw(t162, t163, t164, t165, t166);
  t168 = ((int64_t)(t167));
  t169 = ((int64_t)(((int64_t)UINT64_C(0))));
  t170 = ((t168) == (t169));
  if (t170) {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t171 = ((int64_t)(((int64_t)UINT64_C(17))));
  t172 = ((int64_t)(v36));
  t173 = ((int64_t)(((int64_t)UINT64_C(0))));
  t174 = ((int64_t)(((int64_t)UINT64_C(0))));
  t175 = ((int64_t)(((int64_t)UINT64_C(0))));
  t176 = fgcio_raw(t171, t172, t173, t174, t175);
  t177 = ((int64_t)(t176));
  t178 = ((int64_t)(((int64_t)UINT64_C(17))));
  t179 = ((int64_t)(v37));
  t180 = ((int64_t)(((int64_t)UINT64_C(0))));
  t181 = ((int64_t)(((int64_t)UINT64_C(0))));
  t182 = ((int64_t)(((int64_t)UINT64_C(0))));
  t183 = fgcio_raw(t178, t179, t180, t181, t182);
  t184 = ((int64_t)(t183));
  t185 = ((t177) != (t184));
  if (t185) {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t186 = ((int64_t)(((int64_t)UINT64_C(18))));
  t187 = ((int64_t)(v36));
  t188 = ((int64_t)(((int64_t)UINT64_C(0))));
  t189 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = ((int64_t)(((int64_t)UINT64_C(0))));
  t191 = fgcio_raw(t186, t187, t188, t189, t190);
  t192 = ((int64_t)(t191));
  t193 = ((int64_t)(((int64_t)UINT64_C(18))));
  t194 = ((int64_t)(v37));
  t195 = ((int64_t)(((int64_t)UINT64_C(0))));
  t196 = ((int64_t)(((int64_t)UINT64_C(0))));
  t197 = ((int64_t)(((int64_t)UINT64_C(0))));
  t198 = fgcio_raw(t193, t194, t195, t196, t197);
  t199 = ((int64_t)(t198));
  t200 = ((t192) != (t199));
  if (t200) {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  }
  t201 = ((int64_t)(v35));
  t202 = ((int64_t)(((int64_t)UINT64_C(1))));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t201) + (uint64_t)(t202)))));
  }
  goto label13;
label14:;
  }
  }
  t203 = ((int64_t)(((int64_t)UINT64_C(27))));
  t204 = ((int64_t)(v13));
  t205 = ((int64_t)(((int64_t)UINT64_C(0))));
  t206 = ((int64_t)(((int64_t)UINT64_C(0))));
  t207 = ((int64_t)(((int64_t)UINT64_C(0))));
  t208 = fgcio_raw(t203, t204, t205, t206, t207);
  v38 = ((int64_t)(t208));
  v39 = ((int64_t)(((int64_t)UINT64_C(0))));
  v40 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t209 = ((int64_t)(v40));
  t210 = ((int64_t)(v14));
  if (!(((t209) < (t210)))) goto label17;
  {
  t211 = ((int64_t)(((int64_t)UINT64_C(28))));
  t212 = ((int64_t)(v13));
  t213 = ((int64_t)(v18));
  t214 = ((int64_t)(v40));
  t215 = gg_function_22(t213, t214, base + 0);
  t216 = ((int64_t)(((int64_t)UINT64_C(0))));
  t217 = ((int64_t)(((int64_t)UINT64_C(0))));
  t218 = fgcio_raw(t211, t212, t215, t216, t217);
  v41 = ((int64_t)(t218));
  t219 = ((int64_t)(v41));
  t220 = ((int64_t)(((int64_t)UINT64_C(0))));
  t221 = ((t219) < (t220));
  if (t221) {
  {
  t222 = ((int64_t)(((int64_t)UINT64_C(34))));
  t223 = ((int64_t)(((int64_t)UINT64_C(9))));
  t224 = ((int64_t)(v41));
  t225 = ((int64_t)(((int64_t)UINT64_C(0))));
  t226 = ((int64_t)(((int64_t)UINT64_C(0))));
  t227 = fgcio_raw(t222, t223, t224, t225, t226);
  (void)(t227);
  }
  }
  t228 = ((int64_t)(v41));
  t229 = ((int64_t)(v38));
  t230 = ((int64_t)(((int64_t)((uint64_t)(t228) + (uint64_t)(t229)))));
  t231 = ((int64_t)(((int64_t)UINT64_C(1))));
  t232 = ((int64_t)(((int64_t)((uint64_t)(t230) - (uint64_t)(t231)))));
  t233 = ((int64_t)(v38));
  t234 = ((int64_t)(((int64_t)gg_div(t232, t233))));
  t235 = ((int64_t)(v38));
  v42 = ((int64_t)(((int64_t)((uint64_t)(t234) * (uint64_t)(t235)))));
  t236 = ((int64_t)(v39));
  t237 = ((int64_t)(v42));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t236) + (uint64_t)(t237)))));
  t238 = ((int64_t)(v40));
  t239 = ((int64_t)(((int64_t)UINT64_C(1))));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t238) + (uint64_t)(t239)))));
  }
  goto label16;
label17:;
  v43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t240 = ((int64_t)(v30));
  t241 = ((int64_t)(((int64_t)UINT64_C(0))));
  t242 = ((t240) == (t241));
  t243 = t242;
  if (!t243) {
  t244 = ((int64_t)(v32));
  t245 = ((int64_t)(v15));
  t243 = ((t244) != (t245));
  }
  t246 = t243;
  if (t246) {
  {
  v43 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t247 = ((int64_t)(((int64_t)UINT64_C(29))));
  t248 = ((int64_t)(v30));
  t249 = ((int64_t)(((int64_t)UINT64_C(0))));
  t250 = ((int64_t)(((int64_t)UINT64_C(0))));
  t251 = ((int64_t)(((int64_t)UINT64_C(0))));
  t252 = fgcio_raw(t247, t248, t249, t250, t251);
  t253 = ((int64_t)(t252));
  t254 = ((int64_t)(v39));
  t255 = ((t253) < (t254));
  if (t255) {
  {
  v43 = ((int64_t)(((int64_t)UINT64_C(1))));
  v33 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  }
  t256 = ((int64_t)(v33));
  t257 = ((int64_t)(((int64_t)UINT64_C(0))));
  t258 = ((t256) != (t257));
  if (t258) {
  {
  t259 = ((int64_t)(v43));
  t260 = ((int64_t)(((int64_t)UINT64_C(0))));
  t261 = ((t259) != (t260));
  if (t261) {
  {
  t262 = ((int64_t)(v12));
  t263 = ((int64_t)(((int64_t)UINT64_C(4))));
  t264 = fgc_record_get(t262, t263);
  v44 = ((int64_t)(t264));
  t265 = ((int64_t)(v44));
  t266 = ((int64_t)(((int64_t)UINT64_C(0))));
  t267 = ((t265) != (t266));
  if (t267) {
  {
  t268 = ((int64_t)(((int64_t)UINT64_C(38))));
  t269 = ((int64_t)(v44));
  t270 = ((int64_t)(((int64_t)UINT64_C(0))));
  t271 = ((int64_t)(((int64_t)UINT64_C(0))));
  t272 = ((int64_t)(((int64_t)UINT64_C(0))));
  t273 = fgcio_raw(t268, t269, t270, t271, t272);
  (void)(t273);
  }
  }
  t274 = ((int64_t)(v12));
  t275 = ((int64_t)(((int64_t)UINT64_C(3))));
  t276 = fgc_record_get(t274, t275);
  v45 = ((int64_t)(t276));
  t277 = ((int64_t)(v45));
  t278 = ((int64_t)(((int64_t)UINT64_C(0))));
  t279 = ((t277) != (t278));
  if (t279) {
  {
  t280 = ((int64_t)(((int64_t)UINT64_C(22))));
  t281 = ((int64_t)(v45));
  t282 = ((int64_t)(((int64_t)UINT64_C(0))));
  t283 = ((int64_t)(((int64_t)UINT64_C(0))));
  t284 = ((int64_t)(((int64_t)UINT64_C(0))));
  t285 = fgcio_raw(t280, t281, t282, t283, t284);
  v46 = ((int64_t)(t285));
  }
  }
  t286 = ((int64_t)(v12));
  t287 = ((int64_t)(((int64_t)UINT64_C(3))));
  t288 = ((int64_t)(v16));
  t289 = fgc_record_set(t286, t287, t288);
  v47 = ((int64_t)(t289));
  t290 = ((int64_t)(((int64_t)UINT64_C(26))));
  t291 = ((int64_t)(v16));
  t292 = ((int64_t)(v13));
  t293 = ((int64_t)(((int64_t)UINT64_C(0))));
  t294 = ((int64_t)(((int64_t)UINT64_C(0))));
  t295 = fgcio_raw(t290, t291, t292, t293, t294);
  v48 = ((int64_t)(t295));
  t296 = ((int64_t)(v12));
  t297 = ((int64_t)(((int64_t)UINT64_C(4))));
  t298 = ((int64_t)(v48));
  t299 = fgc_record_set(t296, t297, t298);
  v49 = ((int64_t)(t299));
  t300 = ((int64_t)(v12));
  t301 = ((int64_t)(((int64_t)UINT64_C(5))));
  t302 = fgc_record_get(t300, t301);
  v50 = ((int64_t)(t302));
  t303 = ((int64_t)(v12));
  t304 = ((int64_t)(((int64_t)UINT64_C(6))));
  t305 = fgc_record_get(t303, t304);
  v51 = ((int64_t)(t305));
  t306 = ((int64_t)(v50));
  t307 = ((int64_t)(((int64_t)UINT64_C(0))));
  t308 = ((t306) != (t307));
  if (t308) {
  {
  t309 = ((int64_t)(v50));
  t310 = fgc_i64_vec_free(t309);
  v52 = ((int64_t)(t310));
  }
  }
  t311 = ((int64_t)(v51));
  t312 = ((int64_t)(((int64_t)UINT64_C(0))));
  t313 = ((t311) != (t312));
  if (t313) {
  {
  t314 = ((int64_t)(v51));
  t315 = fgc_i64_vec_free(t314);
  v53 = ((int64_t)(t315));
  }
  }
  t316 = ((int64_t)(v12));
  t317 = ((int64_t)(((int64_t)UINT64_C(5))));
  t318 = ((int64_t)(v17));
  t319 = fgc_record_set(t316, t317, t318);
  v54 = ((int64_t)(t319));
  t320 = ((int64_t)(v12));
  t321 = ((int64_t)(((int64_t)UINT64_C(6))));
  t322 = ((int64_t)(v18));
  t323 = fgc_record_set(t320, t321, t322);
  v55 = ((int64_t)(t323));
  t324 = ((int64_t)(v12));
  t325 = ((int64_t)(((int64_t)UINT64_C(1))));
  t326 = ((int64_t)(v14));
  t327 = fgc_record_set(t324, t325, t326);
  v56 = ((int64_t)(t327));
  t328 = ((int64_t)(v12));
  t329 = ((int64_t)(((int64_t)UINT64_C(2))));
  t330 = ((int64_t)(v15));
  t331 = fgc_record_set(t328, t329, t330);
  v57 = ((int64_t)(t331));
  }
  } else {
  {
  t332 = ((int64_t)(v12));
  t333 = ((int64_t)(((int64_t)UINT64_C(4))));
  t334 = fgc_record_get(t332, t333);
  v58 = ((int64_t)(t334));
  t335 = ((int64_t)(v12));
  t336 = ((int64_t)(((int64_t)UINT64_C(3))));
  t337 = fgc_record_get(t335, t336);
  v59 = ((int64_t)(t337));
  t338 = ((int64_t)(v59));
  t339 = ((int64_t)(((int64_t)UINT64_C(0))));
  t340 = ((t338) != (t339));
  if (t340) {
  {
  t341 = ((int64_t)(((int64_t)UINT64_C(22))));
  t342 = ((int64_t)(v59));
  t343 = ((int64_t)(((int64_t)UINT64_C(0))));
  t344 = ((int64_t)(((int64_t)UINT64_C(0))));
  t345 = ((int64_t)(((int64_t)UINT64_C(0))));
  t346 = fgcio_raw(t341, t342, t343, t344, t345);
  v60 = ((int64_t)(t346));
  }
  }
  t347 = ((int64_t)(v12));
  t348 = ((int64_t)(((int64_t)UINT64_C(3))));
  t349 = ((int64_t)(v16));
  t350 = fgc_record_set(t347, t348, t349);
  v61 = ((int64_t)(t350));
  t351 = ((int64_t)(v12));
  t352 = ((int64_t)(((int64_t)UINT64_C(4))));
  t353 = ((int64_t)(v58));
  t354 = fgc_record_set(t351, t352, t353);
  v62 = ((int64_t)(t354));
  t355 = ((int64_t)(v12));
  t356 = ((int64_t)(((int64_t)UINT64_C(5))));
  t357 = fgc_record_get(t355, t356);
  v63 = ((int64_t)(t357));
  t358 = ((int64_t)(v12));
  t359 = ((int64_t)(((int64_t)UINT64_C(6))));
  t360 = fgc_record_get(t358, t359);
  v64 = ((int64_t)(t360));
  t361 = ((int64_t)(v63));
  t362 = ((int64_t)(((int64_t)UINT64_C(0))));
  t363 = ((t361) != (t362));
  if (t363) {
  {
  t364 = ((int64_t)(v63));
  t365 = fgc_i64_vec_free(t364);
  v65 = ((int64_t)(t365));
  }
  }
  t366 = ((int64_t)(v64));
  t367 = ((int64_t)(((int64_t)UINT64_C(0))));
  t368 = ((t366) != (t367));
  if (t368) {
  {
  t369 = ((int64_t)(v64));
  t370 = fgc_i64_vec_free(t369);
  v66 = ((int64_t)(t370));
  }
  }
  t371 = ((int64_t)(v12));
  t372 = ((int64_t)(((int64_t)UINT64_C(5))));
  t373 = ((int64_t)(v17));
  t374 = fgc_record_set(t371, t372, t373);
  v67 = ((int64_t)(t374));
  t375 = ((int64_t)(v12));
  t376 = ((int64_t)(((int64_t)UINT64_C(6))));
  t377 = ((int64_t)(v18));
  t378 = fgc_record_set(t375, t376, t377);
  v68 = ((int64_t)(t378));
  t379 = ((int64_t)(v12));
  t380 = ((int64_t)(((int64_t)UINT64_C(1))));
  t381 = ((int64_t)(v14));
  t382 = fgc_record_set(t379, t380, t381);
  v69 = ((int64_t)(t382));
  t383 = ((int64_t)(v12));
  t384 = ((int64_t)(((int64_t)UINT64_C(2))));
  t385 = ((int64_t)(v15));
  t386 = fgc_record_set(t383, t384, t385);
  v70 = ((int64_t)(t386));
  t387 = ((int64_t)(((int64_t)UINT64_C(31))));
  t388 = ((int64_t)(v58));
  t389 = ((int64_t)(((int64_t)UINT64_C(0))));
  t390 = ((int64_t)(((int64_t)UINT64_C(0))));
  t391 = ((int64_t)(((int64_t)UINT64_C(0))));
  t392 = fgcio_raw(t387, t388, t389, t390, t391);
  v71 = ((int64_t)(t392));
  v72 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t393 = ((int64_t)(v72));
  t394 = ((int64_t)(v14));
  if (!(((t393) < (t394)))) goto label20;
  {
  t395 = ((int64_t)(v17));
  t396 = ((int64_t)(v72));
  v73 = gg_function_22(t395, t396, base + 0);
  t397 = ((int64_t)(v18));
  t398 = ((int64_t)(v72));
  v74 = gg_function_22(t397, t398, base + 0);
  t399 = ((int64_t)(((int64_t)UINT64_C(19))));
  t400 = ((int64_t)(v73));
  t401 = ((int64_t)(((int64_t)UINT64_C(0))));
  t402 = ((int64_t)(((int64_t)UINT64_C(0))));
  t403 = ((int64_t)(((int64_t)UINT64_C(0))));
  t404 = fgcio_raw(t399, t400, t401, t402, t403);
  v75 = ((int64_t)(t404));
  t405 = ((int64_t)(((int64_t)UINT64_C(32))));
  t406 = ((int64_t)(v71));
  t407 = ((int64_t)(v74));
  t408 = ((int64_t)(((int64_t)UINT64_C(0))));
  t409 = ((int64_t)(((int64_t)UINT64_C(0))));
  t410 = fgcio_raw(t405, t406, t407, t408, t409);
  v76 = ((int64_t)(t410));
  t411 = ((int64_t)(v72));
  t412 = ((int64_t)(((int64_t)UINT64_C(1))));
  v72 = ((int64_t)(((int64_t)((uint64_t)(t411) + (uint64_t)(t412)))));
  }
  goto label19;
label20:;
  t413 = ((int64_t)(((int64_t)UINT64_C(33))));
  t414 = ((int64_t)(v71));
  t415 = ((int64_t)(((int64_t)UINT64_C(0))));
  t416 = ((int64_t)(((int64_t)UINT64_C(0))));
  t417 = ((int64_t)(((int64_t)UINT64_C(0))));
  t418 = fgcio_raw(t413, t414, t415, t416, t417);
  v77 = ((int64_t)(t418));
  }
  }
  }
  } else {
  {
  t419 = ((int64_t)(((int64_t)UINT64_C(22))));
  t420 = ((int64_t)(v16));
  t421 = ((int64_t)(((int64_t)UINT64_C(0))));
  t422 = ((int64_t)(((int64_t)UINT64_C(0))));
  t423 = ((int64_t)(((int64_t)UINT64_C(0))));
  t424 = fgcio_raw(t419, t420, t421, t422, t423);
  v78 = ((int64_t)(t424));
  t425 = ((int64_t)(v17));
  t426 = fgc_i64_vec_free(t425);
  v79 = ((int64_t)(t426));
  t427 = ((int64_t)(v18));
  t428 = fgc_i64_vec_free(t427);
  v80 = ((int64_t)(t428));
  }
  }
  t429 = ((int64_t)(v12));
  t430 = ((int64_t)(((int64_t)UINT64_C(5))));
  t431 = fgc_record_get(t429, t430);
  v81 = ((int64_t)(t431));
  t432 = ((int64_t)(v12));
  t433 = ((int64_t)(((int64_t)UINT64_C(6))));
  t434 = fgc_record_get(t432, t433);
  v82 = ((int64_t)(t434));
  v83 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t435 = ((int64_t)(v83));
  t436 = ((int64_t)(v14));
  if (!(((t435) < (t436)))) goto label23;
  {
  t437 = ((int64_t)(((int64_t)UINT64_C(20))));
  t438 = ((int64_t)(v81));
  t439 = ((int64_t)(v83));
  t440 = gg_function_22(t438, t439, base + 0);
  t441 = ((int64_t)(v82));
  t442 = ((int64_t)(v83));
  t443 = gg_function_22(t441, t442, base + 0);
  t444 = ((int64_t)(((int64_t)UINT64_C(0))));
  t445 = ((int64_t)(((int64_t)UINT64_C(0))));
  t446 = fgcio_raw(t437, t440, t443, t444, t445);
  v84 = ((int64_t)(t446));
  t447 = ((int64_t)(v83));
  t448 = ((int64_t)(((int64_t)UINT64_C(1))));
  v83 = ((int64_t)(((int64_t)((uint64_t)(t447) + (uint64_t)(t448)))));
  }
  goto label22;
label23:;
  }
  }
  t449 = ((int64_t)(v11));
  t450 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t449) + (uint64_t)(t450)))));
  }
  goto label7;
label8:;
  v85 = ((int64_t)(((int64_t)UINT64_C(0))));
label25:;
  t451 = ((int64_t)(v85));
  t452 = ((int64_t)(v2));
  if (!(((t451) < (t452)))) goto label26;
  {
  t453 = ((int64_t)(v1));
  t454 = ((int64_t)(v85));
  v86 = gg_function_22(t453, t454, base + 0);
  t455 = ((int64_t)(v86));
  t456 = fgc_i64_record_free(t455);
  (void)(t456);
  t457 = ((int64_t)(v85));
  t458 = ((int64_t)(((int64_t)UINT64_C(1))));
  v85 = ((int64_t)(((int64_t)((uint64_t)(t457) + (uint64_t)(t458)))));
  }
  goto label25;
label26:;
  t459 = ((int64_t)(v1));
  t460 = ((int64_t)(((int64_t)UINT64_C(0))));
  t461 = fgc_vec_resize(t459, t460);
  v87 = ((int64_t)(t461));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_20(int64_t p0, int64_t base) {
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
  uint8_t t10;
  int64_t t11;
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
  uint8_t t42;
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
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fgc_record_get(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(5))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v0));
  t0 = gg_function_21(t7, base + 0);
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(6))));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t11 = ((int64_t)(v0));
  t0 = gg_function_28(t11, base + 0);
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(3))));
  t14 = ((t12) == (t13));
  t15 = t14;
  if (!t15) {
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = ((t16) == (t17));
  }
  t18 = t15;
  if (t18) {
  {
  t19 = ((int64_t)(v0));
  t20 = gg_function_19(t19, base + 0);
  (void)(t20);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(6))));
  t23 = fgc_record_get(t21, t22);
  v2 = ((int64_t)(t23));
  t24 = ((int64_t)(v2));
  t25 = fgc_vec_len(t24);
  v3 = ((int64_t)(t25));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(v3));
  if (!(((t26) < (t27)))) goto label2;
  {
  t28 = ((int64_t)(v2));
  t29 = fgc_i64_vec_data(t28);
  t30 = ((int64_t)(t29));
  t31 = ((int64_t)(((int64_t)UINT64_C(8))));
  t32 = ((int64_t)(v4));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t33)))));
  t34 = ((int64_t)(((int64_t)UINT64_C(7))));
  t35 = ((int64_t)(v5));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = fgcio_raw(t34, t35, t36, t37, t38);
  v6 = ((int64_t)(t39));
  t40 = ((int64_t)(v6));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t40) != (t41));
  if (t42) {
  {
  t43 = ((int64_t)(v6));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = fgc_record_get(t43, t44);
  v7 = ((int64_t)(t45));
  t46 = ((int64_t)(v6));
  t47 = ((int64_t)(((int64_t)UINT64_C(1))));
  t48 = fgc_record_get(t46, t47);
  v8 = ((int64_t)(t48));
  t49 = ((int64_t)(v6));
  t50 = ((int64_t)(((int64_t)UINT64_C(2))));
  t51 = fgc_record_get(t49, t50);
  v9 = ((int64_t)(t51));
  t52 = ((int64_t)(v6));
  t53 = ((int64_t)(((int64_t)UINT64_C(3))));
  t54 = fgc_record_get(t52, t53);
  v10 = ((int64_t)(t54));
  t55 = ((int64_t)(v1));
  t56 = ((int64_t)(((int64_t)UINT64_C(1))));
  t57 = ((t55) == (t56));
  if (t57) {
  {
  t58 = ((int64_t)(((int64_t)UINT64_C(5))));
  t59 = ((int64_t)(v7));
  t60 = ((int64_t)(v8));
  t61 = ((int64_t)(v10));
  t62 = ((int64_t)(v9));
  t63 = fgcio_raw(t58, t59, t60, t61, t62);
  v11 = ((int64_t)(t63));
  }
  } else {
  {
  t64 = ((int64_t)(((int64_t)UINT64_C(6))));
  t65 = ((int64_t)(v7));
  t66 = ((int64_t)(v8));
  t67 = ((int64_t)(v10));
  t68 = ((int64_t)(v9));
  t69 = fgcio_raw(t64, t65, t66, t67, t68);
  v12 = ((int64_t)(t69));
  }
  }
  t70 = ((int64_t)(v6));
  t71 = fgc_i64_record_free(t70);
  v13 = ((int64_t)(t71));
  }
  }
  t72 = ((int64_t)(v4));
  t73 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  }
  goto label1;
label2:;
  t74 = ((int64_t)(v2));
  t75 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = fgc_vec_resize(t74, t75);
  v14 = ((int64_t)(t76));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_31(int64_t p0, int64_t base) {
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
  uint8_t t10;
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
  int64_t t33;
  int64_t t34;
  int64_t t35;
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
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
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
  uint8_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
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
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fgc_record_get(t4, t5);
  v1 = ((int64_t)(t6));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)UINT64_C(5))));
  t9 = ((t7) == (t8));
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(6))));
  t10 = ((t11) == (t12));
  }
  t13 = t10;
  if (t13) {
  {
  t14 = ((int64_t)(v0));
  v2 = gg_function_19(t14, base + 0);
  }
  } else {
  {
  t15 = ((int64_t)(v0));
  v3 = gg_function_20(t15, base + 0);
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(6))));
  t18 = fgc_record_get(t16, t17);
  v4 = ((int64_t)(t18));
  t19 = ((int64_t)(v0));
  v5 = gg_function_19(t19, base + 0);
  t20 = ((int64_t)(v4));
  t21 = fgc_i64_vec_free(t20);
  v6 = ((int64_t)(t21));
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(10))));
  t24 = fgc_record_get(t22, t23);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(7))));
  t30 = fgc_record_get(t28, t29);
  v7 = ((int64_t)(t30));
  t31 = ((int64_t)(v7));
  t32 = fgc_vec_len(t31);
  v8 = ((int64_t)(t32));
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t33 = ((int64_t)(v9));
  t34 = ((int64_t)(v8));
  if (!(((t33) < (t34)))) goto label2;
  {
  t35 = ((int64_t)(v7));
  t36 = ((int64_t)(v9));
  v10 = gg_function_22(t35, t36, base + 0);
  t37 = ((int64_t)(v10));
  t38 = ((int64_t)(((int64_t)UINT64_C(4))));
  t39 = fgc_record_get(t37, t38);
  v11 = ((int64_t)(t39));
  t40 = ((int64_t)(v11));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t40) != (t41));
  if (t42) {
  {
  t43 = ((int64_t)(((int64_t)UINT64_C(38))));
  t44 = ((int64_t)(v11));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = fgcio_raw(t43, t44, t45, t46, t47);
  v12 = ((int64_t)(t48));
  }
  }
  t49 = ((int64_t)(v10));
  t50 = ((int64_t)(((int64_t)UINT64_C(3))));
  t51 = fgc_record_get(t49, t50);
  v13 = ((int64_t)(t51));
  t52 = ((int64_t)(v13));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = ((t52) != (t53));
  if (t54) {
  {
  t55 = ((int64_t)(((int64_t)UINT64_C(22))));
  t56 = ((int64_t)(v13));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = fgcio_raw(t55, t56, t57, t58, t59);
  v14 = ((int64_t)(t60));
  }
  }
  t61 = ((int64_t)(v10));
  t62 = ((int64_t)(((int64_t)UINT64_C(5))));
  t63 = fgc_record_get(t61, t62);
  v15 = ((int64_t)(t63));
  t64 = ((int64_t)(v15));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((t64) != (t65));
  if (t66) {
  {
  t67 = ((int64_t)(v15));
  t68 = fgc_i64_vec_free(t67);
  v16 = ((int64_t)(t68));
  }
  }
  t69 = ((int64_t)(v10));
  t70 = ((int64_t)(((int64_t)UINT64_C(6))));
  t71 = fgc_record_get(t69, t70);
  v17 = ((int64_t)(t71));
  t72 = ((int64_t)(v17));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((t72) != (t73));
  if (t74) {
  {
  t75 = ((int64_t)(v17));
  t76 = fgc_i64_vec_free(t75);
  v18 = ((int64_t)(t76));
  }
  }
  t77 = ((int64_t)(v10));
  t78 = fgc_i64_record_free(t77);
  v19 = ((int64_t)(t78));
  t79 = ((int64_t)(v9));
  t80 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  }
  goto label1;
label2:;
  t81 = ((int64_t)(v7));
  t82 = fgc_i64_vec_free(t81);
  v20 = ((int64_t)(t82));
  }
  }
  t83 = ((int64_t)(v0));
  t84 = ((int64_t)(((int64_t)UINT64_C(8))));
  t85 = fgc_record_get(t83, t84);
  v21 = ((int64_t)(t85));
  t86 = ((int64_t)(v21));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((t86) != (t87));
  if (t88) {
  {
  t89 = ((int64_t)(v21));
  t90 = fgc_i64_set_free(t89);
  v22 = ((int64_t)(t90));
  }
  }
  t91 = ((int64_t)(v0));
  t92 = fgc_i64_record_free(t91);
  v23 = ((int64_t)(t92));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(3))));
  t5 = fgc_record_get(t3, t4);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t9 = fgc_record_set(t1, t2, t8);
  v2 = ((int64_t)(t9));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_13(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(3102))));
  t2 = fgc_record_new(t1);
  v5 = ((int64_t)(t2));
  t3 = ((int64_t)(v5));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  t6 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t6)))));
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(v1));
  t10 = fgc_record_set(t7, t8, t9);
  v6 = ((int64_t)(t10));
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = ((int64_t)(v2));
  t14 = fgc_record_set(t11, t12, t13);
  v7 = ((int64_t)(t14));
  t15 = ((int64_t)(v5));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((int64_t)(v3));
  t18 = fgc_record_set(t15, t16, t17);
  v8 = ((int64_t)(t18));
  t19 = ((int64_t)(v5));
  t20 = ((int64_t)(((int64_t)UINT64_C(3))));
  t21 = ((int64_t)(v4));
  t22 = fgc_record_set(t19, t20, t21);
  v9 = ((int64_t)(t22));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(6))));
  t25 = fgc_record_get(t23, t24);
  v10 = ((int64_t)(t25));
  t26 = ((int64_t)(v10));
  t27 = fgc_vec_len(t26);
  v11 = ((int64_t)(t27));
  t28 = ((int64_t)(v10));
  t29 = ((int64_t)(v11));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t32 = fgc_vec_resize(t28, t31);
  v12 = ((int64_t)(t32));
  t33 = ((int64_t)(v12));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) != (t34));
  if (t35) {
  {
  t36 = ((int64_t)(v5));
  t37 = fgc_i64_record_free(t36);
  v13 = ((int64_t)(t37));
  t38 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t38)))));
  goto gg_done;
  }
  }
  t39 = ((int64_t)(v10));
  t40 = fgc_i64_vec_data(t39);
  v14 = ((int64_t)(t40));
  t41 = ((int64_t)(((int64_t)UINT64_C(8))));
  t42 = ((int64_t)(v14));
  t43 = ((int64_t)(((int64_t)UINT64_C(8))));
  t44 = ((int64_t)(v11));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t45)))));
  t47 = ((int64_t)(v5));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = fgcio_raw(t41, t46, t47, t48, t49);
  v15 = ((int64_t)(t50));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_18(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t18;
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
  uint8_t t50;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fgc_record_get(t1, t2);
  v4 = ((int64_t)(t3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(5))));
  t9 = fgc_record_get(t7, t8);
  v5 = ((int64_t)(t9));
  t10 = ((int64_t)(((int64_t)UINT64_C(11))));
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(v3));
  t15 = fgcio_raw(t10, t11, t12, t13, t14);
  v6 = ((int64_t)(t15));
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t19 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t19)))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(v2));
  v7 = gg_function_9(t20, t21, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)UINT64_C(6))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = fgc_record_get(t27, t28);
  t30 = ((int64_t)(t29));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(v3));
  v8 = gg_function_13(t25, t26, t30, t31, t32, base + 0);
  t0 = ((int64_t)(v8));
  goto gg_done;
  }
  }
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = fgc_record_get(t33, t34);
  v9 = ((int64_t)(t35));
  t36 = ((int64_t)(v2));
  t37 = ((int64_t)(v9));
  t38 = ((t36) > (t37));
  if (t38) {
  {
  t39 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t39)))));
  goto gg_done;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  t42 = fgc_record_get(t40, t41);
  v10 = ((int64_t)(t42));
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(v10));
  t46 = ((int64_t)(v2));
  t47 = ((int64_t)(v3));
  v11 = gg_function_13(t43, t44, t45, t46, t47, base + 0);
  t48 = ((int64_t)(v11));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((t48) != (t49));
  if (t50) {
  {
  t0 = ((int64_t)(v11));
  goto gg_done;
  }
  }
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(v2));
  v12 = gg_function_9(t51, t52, base + 0);
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((int64_t)(v10));
  t56 = ((int64_t)(v2));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
  t58 = fgc_record_set(t53, t54, t57);
  v13 = ((int64_t)(t58));
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(2))));
  t61 = ((int64_t)(v9));
  t62 = ((int64_t)(v2));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) - (uint64_t)(t62)))));
  t64 = fgc_record_set(t59, t60, t63);
  v14 = ((int64_t)(t64));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t22;
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
  uint8_t t61;
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
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fgc_record_get(t1, t2);
  v4 = ((int64_t)(t3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(4))));
  t9 = fgc_record_get(t7, t8);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t10) != (t11));
  if (t12) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v2));
  v5 = gg_function_9(t13, t14, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)UINT64_C(3))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(5))));
  t20 = fgc_record_get(t18, t19);
  v6 = ((int64_t)(t20));
  t21 = ((int64_t)(((int64_t)UINT64_C(10))));
  t22 = ((int64_t)(v6));
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(v3));
  t26 = fgcio_raw(t21, t22, t23, t24, t25);
  v7 = ((int64_t)(t26));
  t27 = ((int64_t)(v7));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t27) < (t28));
  if (t29) {
  {
  t30 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t30)))));
  goto gg_done;
  }
  }
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(v2));
  v8 = gg_function_9(t31, t32, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)UINT64_C(5))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(v1));
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = fgc_record_get(t38, t39);
  t41 = ((int64_t)(t40));
  t42 = ((int64_t)(v2));
  t43 = ((int64_t)(v3));
  v9 = gg_function_13(t36, t37, t41, t42, t43, base + 0);
  t0 = ((int64_t)(v9));
  goto gg_done;
  }
  }
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(2))));
  t46 = fgc_record_get(t44, t45);
  v10 = ((int64_t)(t46));
  t47 = ((int64_t)(v2));
  t48 = ((int64_t)(v10));
  t49 = ((t47) > (t48));
  if (t49) {
  {
  t50 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t50)))));
  goto gg_done;
  }
  }
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = fgc_record_get(t51, t52);
  v11 = ((int64_t)(t53));
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(v1));
  t56 = ((int64_t)(v11));
  t57 = ((int64_t)(v2));
  t58 = ((int64_t)(v3));
  v12 = gg_function_13(t54, t55, t56, t57, t58, base + 0);
  t59 = ((int64_t)(v12));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((t59) != (t60));
  if (t61) {
  {
  t0 = ((int64_t)(v12));
  goto gg_done;
  }
  }
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(v2));
  v13 = gg_function_9(t62, t63, base + 0);
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(((int64_t)UINT64_C(1))));
  t66 = ((int64_t)(v11));
  t67 = ((int64_t)(v2));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t69 = fgc_record_set(t64, t65, t68);
  v14 = ((int64_t)(t69));
  t70 = ((int64_t)(v0));
  t71 = ((int64_t)(((int64_t)UINT64_C(2))));
  t72 = ((int64_t)(v10));
  t73 = ((int64_t)(v2));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) - (uint64_t)(t73)))));
  t75 = fgc_record_set(t70, t71, t74);
  v15 = ((int64_t)(t75));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t18;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fgc_record_get(t1, t2);
  v3 = ((int64_t)(t3));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(5))));
  t9 = fgc_record_get(t7, t8);
  v4 = ((int64_t)(t9));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  t11 = ((int64_t)(v4));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = fgcio_raw(t10, t11, t12, t13, t14);
  v5 = ((int64_t)(t15));
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t19 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t19)))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(v2));
  v6 = gg_function_9(t20, t21, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  t24 = fgc_record_get(t22, t23);
  v7 = ((int64_t)(t24));
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(v7));
  t27 = ((t25) > (t26));
  if (t27) {
  {
  t28 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t28)))));
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = fgc_record_get(t29, t30);
  v8 = ((int64_t)(t31));
  t32 = ((int64_t)(((int64_t)UINT64_C(2))));
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(v8));
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = fgcio_raw(t32, t33, t34, t35, t36);
  v9 = ((int64_t)(t37));
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(v2));
  v10 = gg_function_9(t38, t39, base + 0);
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  t42 = ((int64_t)(v8));
  t43 = ((int64_t)(v2));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = fgc_record_set(t40, t41, t44);
  v11 = ((int64_t)(t45));
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(2))));
  t48 = ((int64_t)(v7));
  t49 = ((int64_t)(v2));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) - (uint64_t)(t49)))));
  t51 = fgc_record_set(t46, t47, t50);
  v12 = ((int64_t)(t51));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t23;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fgc_record_get(t1, t2);
  v3 = ((int64_t)(t3));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v2));
  v4 = gg_function_9(t7, t8, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(3))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(5))));
  t14 = fgc_record_get(t12, t13);
  v5 = ((int64_t)(t14));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fgcio_raw(t15, t16, t17, t18, t19);
  v6 = ((int64_t)(t20));
  t21 = ((int64_t)(v6));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) < (t22));
  if (t23) {
  {
  t24 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t24)))));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(v2));
  v7 = gg_function_9(t25, t26, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = fgc_record_get(t27, t28);
  v8 = ((int64_t)(t29));
  t30 = ((int64_t)(v2));
  t31 = ((int64_t)(v8));
  t32 = ((t30) > (t31));
  if (t32) {
  {
  t33 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t33)))));
  goto gg_done;
  }
  }
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  t36 = fgc_record_get(t34, t35);
  v9 = ((int64_t)(t36));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  t38 = ((int64_t)(v9));
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(v2));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = fgcio_raw(t37, t38, t39, t40, t41);
  v10 = ((int64_t)(t42));
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(v2));
  v11 = gg_function_9(t43, t44, base + 0);
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t47 = ((int64_t)(v9));
  t48 = ((int64_t)(v2));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t50 = fgc_record_set(t45, t46, t49);
  v12 = ((int64_t)(t50));
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((int64_t)(v8));
  t54 = ((int64_t)(v2));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) - (uint64_t)(t54)))));
  t56 = fgc_record_set(t51, t52, t55);
  v13 = ((int64_t)(t56));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t10;
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
  uint8_t t21;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(3101))));
  t2 = fgc_record_new(t1);
  v4 = ((int64_t)(t2));
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(((int64_t)UINT64_C(8))));
  t7 = fgc_vec_new(t6);
  v5 = ((int64_t)(t7));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(6))));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) != (t12));
  if (t13) {
  {
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)UINT64_C(7))));
  t16 = fgc_record_get(t14, t15);
  v6 = ((int64_t)(t16));
  v7 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t17 = ((int64_t)(((int64_t)UINT64_C(8))));
  t18 = fgc_vec_new(t17);
  v6 = ((int64_t)(t18));
  }
  }
  }
  }
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = ((int64_t)(((int64_t)UINT64_C(8))));
  t23 = fgc_vec_new(t22);
  v6 = ((int64_t)(t23));
  }
  }
  t24 = fgc_set_new();
  v8 = ((int64_t)(t24));
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((int64_t)(v0));
  t28 = fgc_record_set(t25, t26, t27);
  v9 = ((int64_t)(t28));
  t29 = ((int64_t)(v4));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((int64_t)(v1));
  t32 = fgc_record_set(t29, t30, t31);
  v10 = ((int64_t)(t32));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = ((int64_t)(v2));
  t36 = fgc_record_set(t33, t34, t35);
  v11 = ((int64_t)(t36));
  t37 = ((int64_t)(v4));
  t38 = ((int64_t)(((int64_t)UINT64_C(3))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = fgc_record_set(t37, t38, t39);
  v12 = ((int64_t)(t40));
  t41 = ((int64_t)(v4));
  t42 = ((int64_t)(((int64_t)UINT64_C(4))));
  t43 = ((int64_t)(v3));
  t44 = fgc_record_set(t41, t42, t43);
  v13 = ((int64_t)(t44));
  t45 = ((int64_t)(v4));
  t46 = ((int64_t)(((int64_t)UINT64_C(5))));
  t47 = ((int64_t)(v1));
  t48 = fgc_record_set(t45, t46, t47);
  v14 = ((int64_t)(t48));
  t49 = ((int64_t)(v4));
  t50 = ((int64_t)(((int64_t)UINT64_C(6))));
  t51 = ((int64_t)(v5));
  t52 = fgc_record_set(t49, t50, t51);
  v15 = ((int64_t)(t52));
  t53 = ((int64_t)(v4));
  t54 = ((int64_t)(((int64_t)UINT64_C(7))));
  t55 = ((int64_t)(v6));
  t56 = fgc_record_set(t53, t54, t55);
  v16 = ((int64_t)(t56));
  t57 = ((int64_t)(v4));
  t58 = ((int64_t)(((int64_t)UINT64_C(8))));
  t59 = ((int64_t)(v8));
  t60 = fgc_record_set(t57, t58, t59);
  v17 = ((int64_t)(t60));
  t61 = ((int64_t)(v4));
  t62 = ((int64_t)(((int64_t)UINT64_C(10))));
  t63 = ((int64_t)(v7));
  t64 = fgc_record_set(t61, t62, t63);
  v18 = ((int64_t)(t64));
  t0 = ((int64_t)(v4));
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
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
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
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
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
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(v4));
  t0 = gg_function_2(t4, t5, t6, t7, base + 0);
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(v3));
  t0 = gg_function_8(t11, t12, t13, base + 0);
  goto gg_done;
  }
  }
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(v3));
  t0 = gg_function_11(t17, t18, t19, base + 0);
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
  t0 = gg_function_12(t23, t24, t25, t26, base + 0);
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
  t32 = ((int64_t)(v3));
  t33 = ((int64_t)(v4));
  t0 = gg_function_18(t30, t31, t32, t33, base + 0);
  goto gg_done;
  }
  }
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(5))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  t37 = ((int64_t)(v1));
  t38 = ((int64_t)(((int64_t)UINT64_C(3))));
  t39 = fgc_record_get(t37, t38);
  t0 = ((int64_t)(t39));
  goto gg_done;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(6))));
  t42 = ((t40) == (t41));
  if (t42) {
  {
  t43 = ((int64_t)(v1));
  t0 = gg_function_19(t43, base + 0);
  goto gg_done;
  }
  }
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(7))));
  t46 = ((t44) == (t45));
  if (t46) {
  {
  t47 = ((int64_t)(v1));
  t0 = gg_function_20(t47, base + 0);
  goto gg_done;
  }
  }
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(8))));
  t50 = ((t48) == (t49));
  if (t50) {
  {
  t51 = ((int64_t)(v1));
  t0 = gg_function_31(t51, base + 0);
  goto gg_done;
  }
  }
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(((int64_t)UINT64_C(9))));
  t54 = ((t52) == (t53));
  if (t54) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  goto gg_done;
  }
  }
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = fgcio_raw(t55, t56, t57, t58, t59);
  (void)(t60);
  t61 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t61)))));
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
  t1 = ((int64_t)(((int64_t)UINT64_C(9))));
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
