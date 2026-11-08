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
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(float a, float b) {
return a == b;
}
static void gg_render_4(gg_reference out, float value) {
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
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_9(int64_t p0, int64_t base);
static int64_t gg_function_10(int64_t p0, int64_t base);
static float gg_function_14(float p0, float p1, int64_t base);
static float gg_function_14(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float t8;
  float t9;
  float t10;
  float t11;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t0 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  goto gg_done;
  }
  }
  t4 = ((float)(((float)(gg_double_bits(UINT64_C(4591870180066957722))))));
  t5 = ((float)(v1));
  t6 = ((float)(((t4) * (t5))));
  t7 = ((float)(v0));
  t8 = logf(t7);
  t9 = ((float)(t8));
  t10 = ((float)(((t6) * (t9))));
  t11 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t0 = ((float)(((t10) + (t11))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(int64_t p0, int64_t base) {
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
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  float t36;
  float t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  float t41;
  float t42;
  float t43;
  float t44;
  uint8_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  float t49;
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
  float t61;
  float t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  float t66;
  float t67;
  float t68;
  float t69;
  uint8_t t70;
  float t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  float t75;
  float t76;
  float t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  float t81;
  float t82;
  float t83;
  float t84;
  uint8_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  float t89;
  float t90;
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
  uint8_t t101;
  uint8_t t102;
  float t103;
  float t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  float t109;
  float t110;
  float t111;
  float t112;
  float t113;
  float t114;
  float t115;
  float t116;
  float t117;
  float t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  float t127;
  float t128;
  float t129;
  float t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  float t137;
  float t138;
  float t139;
  float t140;
  float t141;
  float t142;
  float t143;
  float t144;
  float t145;
  float t146;
  float t147;
  float t148;
  float t149;
  float t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  float t157;
  float t158;
  float t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  float t163;
  float t164;
  float t165;
  float t166;
  float t167;
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
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  uint8_t t189;
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
  uint8_t t218;
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
  uint8_t t243;
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
  int64_t t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  uint8_t t277;
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
  uint8_t t337;
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
  uint8_t t372;
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
  uint8_t t386;
  uint8_t t387;
  int64_t t388;
  int64_t t389;
  int64_t t390;
  int64_t t391;
  uint8_t t392;
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
  uint8_t t406;
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
  uint8_t t419;
  int64_t t420;
  int64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  int64_t t425;
  int64_t t426;
  int64_t v0;
  float v1;
  float v2;
  float v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(400))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg107_cpi(t1, t2, t3, t4);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg107_cpc(t7, t8);
  t10 = ((int64_t)(t9));
  t11 = ((t6) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = ((int64_t)(((int64_t)UINT64_C(400))));
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((int64_t)(((int64_t)UINT64_C(200))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = fg107_cpi(t15, t16, t17, t18);
  t20 = ((int64_t)(t19));
  t21 = fg107_cpi(t12, t13, t14, t20);
  (void)(t21);
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(((int64_t)UINT64_C(400))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg107_cpi(t22, t23, t24, t25);
  t27 = ((int64_t)(t26));
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = fg107_cpc(t28, t29);
  t31 = ((int64_t)(t30));
  t32 = ((t27) == (t31));
  if (t32) {
  {
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = ((int64_t)(((int64_t)UINT64_C(500))));
  t36 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t37 = fg107_cpf(t33, t34, t35, t36);
  (void)(t37);
  }
  }
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((int64_t)(((int64_t)UINT64_C(501))));
  t41 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t42 = fg107_cpf(t38, t39, t40, t41);
  t43 = ((float)(t42));
  t44 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t45 = ((t43) < (t44));
  if (t45) {
  {
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(1))));
  t48 = ((int64_t)(((int64_t)UINT64_C(501))));
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((int64_t)(((int64_t)UINT64_C(400))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = fg107_cpi(t50, t51, t52, t53);
  t55 = ((int64_t)(t54));
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t58 = fg107_cpc(t56, t57);
  t59 = ((int64_t)(t58));
  t60 = ((t55) == (t59));
  if (t60) {
  {
  t49 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t49 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  }
  t61 = ((float)(t49));
  t62 = fg107_cpf(t46, t47, t48, t61);
  (void)(t62);
  }
  }
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((int64_t)(((int64_t)UINT64_C(501))));
  t66 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t67 = fg107_cpf(t63, t64, t65, t66);
  t68 = ((float)(t67));
  t69 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t70 = ((t68) != (t69));
  if (t70) {
  {
  t71 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((int64_t)(((int64_t)UINT64_C(500))));
  t75 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t76 = fg107_cpf(t72, t73, t74, t75);
  t77 = ((float)(t76));
  v1 = ((float)(((t71) / (t77))));
  v2 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = ((int64_t)(((int64_t)UINT64_C(302))));
  t81 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t82 = fg107_cpf(t78, t79, t80, t81);
  t83 = ((float)(t82));
  t84 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t85 = ((t83) != (t84));
  if (t85) {
  {
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((int64_t)(((int64_t)UINT64_C(302))));
  t89 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t90 = fg107_cpf(t86, t87, t88, t89);
  v3 = ((float)(t90));
  t91 = ((int64_t)(v0));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((int64_t)(((int64_t)UINT64_C(203))));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = fg107_cpi(t91, t92, t93, t94);
  t96 = ((int64_t)(t95));
  t97 = ((int64_t)(v0));
  t98 = ((int64_t)(((int64_t)UINT64_C(9))));
  t99 = fg107_cpc(t97, t98);
  t100 = ((int64_t)(t99));
  t101 = ((t96) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((float)(v3));
  t104 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t102 = ((t103) != (t104));
  }
  t105 = t102;
  if (t105) {
  {
  v2 = ((float)(v3));
  }
  }
  t106 = ((int64_t)(v0));
  t107 = ((int64_t)(((int64_t)UINT64_C(1))));
  t108 = ((int64_t)(((int64_t)UINT64_C(502))));
  t109 = ((float)(v1));
  t110 = ((float)(v2));
  t111 = gg_function_14(t109, t110, base + 0);
  t112 = ((float)(t111));
  t113 = ((float)(v1));
  t114 = ((float)(v3));
  t115 = gg_function_14(t113, t114, base + 0);
  t116 = ((float)(t115));
  t117 = ((float)(((t112) / (t116))));
  t118 = fg107_cpf(t106, t107, t108, t117);
  (void)(t118);
  t119 = ((int64_t)(v0));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = ((int64_t)(((int64_t)UINT64_C(0))));
  t123 = fg107_cplog(t119, t120, t121, t122);
  (void)(t123);
  }
  } else {
  {
  t124 = ((int64_t)(v0));
  t125 = ((int64_t)(((int64_t)UINT64_C(1))));
  t126 = ((int64_t)(((int64_t)UINT64_C(502))));
  t127 = ((float)(v1));
  t128 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t129 = gg_function_14(t127, t128, base + 0);
  t130 = fg107_cpf(t124, t125, t126, t129);
  (void)(t130);
  }
  }
  t131 = ((int64_t)(v0));
  t132 = ((int64_t)(((int64_t)UINT64_C(1))));
  t133 = ((int64_t)(((int64_t)UINT64_C(502))));
  t134 = ((int64_t)(v0));
  t135 = ((int64_t)(((int64_t)UINT64_C(0))));
  t136 = ((int64_t)(((int64_t)UINT64_C(502))));
  t137 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t138 = fg107_cpf(t134, t135, t136, t137);
  t139 = ((float)(t138));
  t140 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t141 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t142 = ((float)(((float)(gg_double_bits(UINT64_C(4591870180066957722))))));
  t143 = ((float)(v1));
  t144 = logf(t143);
  t145 = ((float)(t144));
  t146 = ((float)(((t142) * (t145))));
  t147 = ((float)(((t141) + (t146))));
  t148 = ((float)(((t140) / (t147))));
  t149 = ((float)(((t139) * (t148))));
  t150 = fg107_cpf(t131, t132, t133, t149);
  (void)(t150);
  }
  }
  t151 = ((int64_t)(v0));
  t152 = ((int64_t)(((int64_t)UINT64_C(1))));
  t153 = ((int64_t)(((int64_t)UINT64_C(502))));
  t154 = ((int64_t)(v0));
  t155 = ((int64_t)(((int64_t)UINT64_C(0))));
  t156 = ((int64_t)(((int64_t)UINT64_C(502))));
  t157 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t158 = fg107_cpf(t154, t155, t156, t157);
  t159 = ((float)(t158));
  t160 = ((int64_t)(v0));
  t161 = ((int64_t)(((int64_t)UINT64_C(0))));
  t162 = ((int64_t)(((int64_t)UINT64_C(303))));
  t163 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t164 = fg107_cpf(t160, t161, t162, t163);
  t165 = ((float)(t164));
  t166 = ((float)(((t159) * (t165))));
  t167 = fg107_cpf(t151, t152, t153, t166);
  (void)(t167);
  t168 = ((int64_t)(v0));
  t169 = ((int64_t)(((int64_t)UINT64_C(0))));
  t170 = ((int64_t)(((int64_t)UINT64_C(401))));
  t171 = ((int64_t)(((int64_t)UINT64_C(0))));
  t172 = fg107_cpi(t168, t169, t170, t171);
  t173 = ((int64_t)(t172));
  t174 = ((int64_t)(v0));
  t175 = ((int64_t)(((int64_t)UINT64_C(3))));
  t176 = fg107_cpc(t174, t175);
  t177 = ((int64_t)(t176));
  t178 = ((t173) == (t177));
  if (t178) {
  {
  t179 = ((int64_t)(v0));
  t180 = ((int64_t)(((int64_t)UINT64_C(0))));
  t181 = ((int64_t)(((int64_t)UINT64_C(201))));
  t182 = ((int64_t)(((int64_t)UINT64_C(0))));
  t183 = fg107_cpi(t179, t180, t181, t182);
  t184 = ((int64_t)(t183));
  t185 = ((int64_t)(v0));
  t186 = ((int64_t)(((int64_t)UINT64_C(3))));
  t187 = fg107_cpc(t185, t186);
  t188 = ((int64_t)(t187));
  t189 = ((t184) == (t188));
  if (t189) {
  {
  t190 = ((int64_t)(v0));
  t191 = ((int64_t)(((int64_t)UINT64_C(1))));
  t192 = ((int64_t)(((int64_t)UINT64_C(401))));
  t193 = ((int64_t)(v0));
  t194 = ((int64_t)(((int64_t)UINT64_C(4))));
  t195 = fg107_cpc(t193, t194);
  t196 = ((int64_t)(t195));
  t197 = fg107_cpi(t190, t191, t192, t196);
  (void)(t197);
  }
  } else {
  {
  t198 = ((int64_t)(v0));
  t199 = ((int64_t)(((int64_t)UINT64_C(1))));
  t200 = ((int64_t)(((int64_t)UINT64_C(401))));
  t201 = ((int64_t)(v0));
  t202 = ((int64_t)(((int64_t)UINT64_C(0))));
  t203 = ((int64_t)(((int64_t)UINT64_C(201))));
  t204 = ((int64_t)(((int64_t)UINT64_C(0))));
  t205 = fg107_cpi(t201, t202, t203, t204);
  t206 = ((int64_t)(t205));
  t207 = fg107_cpi(t198, t199, t200, t206);
  (void)(t207);
  }
  }
  }
  }
  t208 = ((int64_t)(v0));
  t209 = ((int64_t)(((int64_t)UINT64_C(0))));
  t210 = ((int64_t)(((int64_t)UINT64_C(1))));
  t211 = ((int64_t)(((int64_t)UINT64_C(0))));
  t212 = fg107_cpi(t208, t209, t210, t211);
  t213 = ((int64_t)(t212));
  t214 = ((int64_t)(v0));
  t215 = ((int64_t)(((int64_t)UINT64_C(5))));
  t216 = fg107_cpc(t214, t215);
  t217 = ((int64_t)(t216));
  t218 = ((t213) == (t217));
  if (t218) {
  {
  t219 = ((int64_t)(v0));
  t220 = ((int64_t)(((int64_t)UINT64_C(1))));
  t221 = ((int64_t)(((int64_t)UINT64_C(402))));
  t222 = ((int64_t)(v0));
  t223 = ((int64_t)(((int64_t)UINT64_C(0))));
  t224 = ((int64_t)(((int64_t)UINT64_C(202))));
  t225 = ((int64_t)(((int64_t)UINT64_C(0))));
  t226 = fg107_cpi(t222, t223, t224, t225);
  t227 = ((int64_t)(t226));
  t228 = fg107_cpi(t219, t220, t221, t227);
  (void)(t228);
  }
  } else {
  {
  t229 = ((int64_t)(v0));
  t230 = ((int64_t)(((int64_t)UINT64_C(1))));
  t231 = ((int64_t)(((int64_t)UINT64_C(402))));
  t233 = ((int64_t)(v0));
  t234 = ((int64_t)(((int64_t)UINT64_C(0))));
  t235 = ((int64_t)(((int64_t)UINT64_C(1))));
  t236 = ((int64_t)(((int64_t)UINT64_C(0))));
  t237 = fg107_cpi(t233, t234, t235, t236);
  t238 = ((int64_t)(t237));
  t239 = ((int64_t)(v0));
  t240 = ((int64_t)(((int64_t)UINT64_C(6))));
  t241 = fg107_cpc(t239, t240);
  t242 = ((int64_t)(t241));
  t243 = ((t238) == (t242));
  if (t243) {
  {
  t232 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t244 = ((int64_t)(t232));
  t245 = fg107_cpi(t229, t230, t231, t244);
  (void)(t245);
  }
  }
  t246 = ((int64_t)(v0));
  t247 = ((int64_t)(((int64_t)UINT64_C(1))));
  t248 = ((int64_t)(((int64_t)UINT64_C(403))));
  t250 = ((int64_t)(v0));
  t251 = ((int64_t)(((int64_t)UINT64_C(0))));
  t252 = ((int64_t)(((int64_t)UINT64_C(2))));
  t253 = ((int64_t)(((int64_t)UINT64_C(0))));
  t254 = fg107_cpi(t250, t251, t252, t253);
  t255 = ((int64_t)(t254));
  t256 = ((int64_t)(v0));
  t257 = ((int64_t)(((int64_t)UINT64_C(7))));
  t258 = fg107_cpc(t256, t257);
  t259 = ((int64_t)(t258));
  t260 = ((t255) != (t259));
  if (t260) {
  {
  t249 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t249 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t261 = ((int64_t)(t249));
  t262 = fg107_cpi(t246, t247, t248, t261);
  (void)(t262);
  t263 = ((int64_t)(v0));
  t264 = ((int64_t)(((int64_t)UINT64_C(1))));
  t265 = ((int64_t)(((int64_t)UINT64_C(404))));
  t267 = ((int64_t)(v0));
  t268 = ((int64_t)(((int64_t)UINT64_C(0))));
  t269 = ((int64_t)(((int64_t)UINT64_C(2))));
  t270 = ((int64_t)(((int64_t)UINT64_C(0))));
  t271 = fg107_cpi(t267, t268, t269, t270);
  t272 = ((int64_t)(t271));
  t273 = ((int64_t)(v0));
  t274 = ((int64_t)(((int64_t)UINT64_C(8))));
  t275 = fg107_cpc(t273, t274);
  t276 = ((int64_t)(t275));
  t277 = ((t272) == (t276));
  if (t277) {
  {
  t266 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t266 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t278 = ((int64_t)(t266));
  t279 = fg107_cpi(t263, t264, t265, t278);
  (void)(t279);
  t280 = ((int64_t)(v0));
  t281 = ((int64_t)(((int64_t)UINT64_C(1))));
  t282 = ((int64_t)(((int64_t)UINT64_C(405))));
  t283 = ((int64_t)(((int64_t)UINT64_C(1))));
  t284 = fg107_cpi(t280, t281, t282, t283);
  (void)(t284);
  t285 = ((int64_t)(v0));
  t286 = ((int64_t)(((int64_t)UINT64_C(1))));
  t287 = ((int64_t)(((int64_t)UINT64_C(406))));
  t288 = ((int64_t)(((int64_t)UINT64_C(1))));
  t289 = fg107_cpi(t285, t286, t287, t288);
  (void)(t289);
  t290 = ((int64_t)(v0));
  t291 = ((int64_t)(((int64_t)UINT64_C(1))));
  t292 = ((int64_t)(((int64_t)UINT64_C(407))));
  t293 = ((int64_t)(((int64_t)UINT64_C(0))));
  t294 = fg107_cpi(t290, t291, t292, t293);
  (void)(t294);
  t295 = ((int64_t)(v0));
  t296 = ((int64_t)(((int64_t)UINT64_C(1))));
  t297 = ((int64_t)(((int64_t)UINT64_C(408))));
  t298 = ((int64_t)(((int64_t)UINT64_C(1))));
  t299 = fg107_cpi(t295, t296, t297, t298);
  (void)(t299);
  t300 = ((int64_t)(v0));
  t301 = ((int64_t)(((int64_t)UINT64_C(1))));
  t302 = ((int64_t)(((int64_t)UINT64_C(409))));
  t303 = ((int64_t)(((int64_t)UINT64_C(0))));
  t304 = fg107_cpi(t300, t301, t302, t303);
  (void)(t304);
  t305 = ((int64_t)(v0));
  t306 = ((int64_t)(((int64_t)UINT64_C(1))));
  t307 = ((int64_t)(((int64_t)UINT64_C(410))));
  t308 = ((int64_t)(((int64_t)UINT64_C(1))));
  t309 = fg107_cpi(t305, t306, t307, t308);
  (void)(t309);
  t310 = ((int64_t)(v0));
  t311 = ((int64_t)(((int64_t)UINT64_C(1))));
  t312 = ((int64_t)(((int64_t)UINT64_C(411))));
  t313 = ((int64_t)(((int64_t)UINT64_C(1))));
  t314 = fg107_cpi(t310, t311, t312, t313);
  (void)(t314);
  t315 = ((int64_t)(v0));
  t316 = ((int64_t)(((int64_t)UINT64_C(1))));
  t317 = ((int64_t)(((int64_t)UINT64_C(412))));
  t318 = ((int64_t)(((int64_t)UINT64_C(1))));
  t319 = fg107_cpi(t315, t316, t317, t318);
  (void)(t319);
  t320 = ((int64_t)(v0));
  t321 = ((int64_t)(((int64_t)UINT64_C(1))));
  t322 = ((int64_t)(((int64_t)UINT64_C(413))));
  t323 = ((int64_t)(((int64_t)UINT64_C(1))));
  t324 = fg107_cpi(t320, t321, t322, t323);
  (void)(t324);
  t325 = ((int64_t)(v0));
  t326 = ((int64_t)(((int64_t)UINT64_C(0))));
  t327 = ((int64_t)(((int64_t)UINT64_C(418))));
  t328 = ((int64_t)(((int64_t)UINT64_C(0))));
  t329 = fg107_cpi(t325, t326, t327, t328);
  v4 = ((int64_t)(t329));
  t330 = ((int64_t)(v0));
  t331 = ((int64_t)(((int64_t)UINT64_C(0))));
  t332 = ((int64_t)(((int64_t)UINT64_C(402))));
  t333 = ((int64_t)(((int64_t)UINT64_C(0))));
  t334 = fg107_cpi(t330, t331, t332, t333);
  t335 = ((int64_t)(t334));
  t336 = ((int64_t)(((int64_t)UINT64_C(0))));
  t337 = ((t335) != (t336));
  if (t337) {
  {
  t338 = ((int64_t)(v4));
  t339 = ((int64_t)(v0));
  t340 = ((int64_t)(((int64_t)UINT64_C(0))));
  t341 = ((int64_t)(((int64_t)UINT64_C(3))));
  t342 = ((int64_t)(((int64_t)UINT64_C(0))));
  t343 = fg107_cpi(t339, t340, t341, t342);
  t344 = ((int64_t)(t343));
  t345 = ((t338) < (t344));
  if (t345) {
  {
  v5 = ((int64_t)(v4));
  }
  } else {
  {
  t346 = ((int64_t)(v0));
  t347 = ((int64_t)(((int64_t)UINT64_C(0))));
  t348 = ((int64_t)(((int64_t)UINT64_C(3))));
  t349 = ((int64_t)(((int64_t)UINT64_C(0))));
  t350 = fg107_cpi(t346, t347, t348, t349);
  v5 = ((int64_t)(t350));
  }
  }
  }
  } else {
  {
  t351 = ((int64_t)(v0));
  t352 = ((int64_t)(((int64_t)UINT64_C(0))));
  t353 = ((int64_t)(((int64_t)UINT64_C(3))));
  t354 = ((int64_t)(((int64_t)UINT64_C(0))));
  t355 = fg107_cpi(t351, t352, t353, t354);
  v5 = ((int64_t)(t355));
  }
  }
  t356 = ((int64_t)(v0));
  t357 = ((int64_t)(((int64_t)UINT64_C(1))));
  t358 = ((int64_t)(((int64_t)UINT64_C(414))));
  t359 = ((int64_t)(v5));
  t360 = fg107_cpi(t356, t357, t358, t359);
  (void)(t360);
  t361 = ((int64_t)(v0));
  t362 = ((int64_t)(((int64_t)UINT64_C(0))));
  t363 = ((int64_t)(((int64_t)UINT64_C(4))));
  t364 = ((int64_t)(((int64_t)UINT64_C(0))));
  t365 = fg107_cpi(t361, t362, t363, t364);
  v6 = ((int64_t)(t365));
  t366 = ((int64_t)(v0));
  t367 = ((int64_t)(((int64_t)UINT64_C(1))));
  t368 = ((int64_t)(((int64_t)UINT64_C(415))));
  t370 = ((int64_t)(v5));
  t371 = ((int64_t)(v6));
  t372 = ((t370) < (t371));
  if (t372) {
  {
  t369 = ((int64_t)(v5));
  }
  } else {
  {
  t369 = ((int64_t)(v6));
  }
  }
  t373 = ((int64_t)(t369));
  t374 = fg107_cpi(t366, t367, t368, t373);
  (void)(t374);
  t375 = ((int64_t)(v0));
  t376 = ((int64_t)(((int64_t)UINT64_C(0))));
  t377 = ((int64_t)(((int64_t)UINT64_C(5))));
  t378 = ((int64_t)(((int64_t)UINT64_C(0))));
  t379 = fg107_cpi(t375, t376, t377, t378);
  v7 = ((int64_t)(t379));
  t380 = ((int64_t)(v0));
  t381 = ((int64_t)(((int64_t)UINT64_C(1))));
  t382 = ((int64_t)(((int64_t)UINT64_C(416))));
  t384 = ((int64_t)(v7));
  t385 = ((int64_t)(((int64_t)UINT64_C(0))));
  t386 = ((t384) == (t385));
  t387 = t386;
  if (!t387) {
  t388 = ((int64_t)(v0));
  t389 = fg107_cpmodel(t388);
  t390 = ((int64_t)(t389));
  t391 = ((int64_t)(((int64_t)UINT64_C(0))));
  t387 = ((t390) != (t391));
  }
  t392 = t387;
  if (t392) {
  {
  t383 = ((int64_t)(v5));
  }
  } else {
  {
  t383 = ((int64_t)(v7));
  }
  }
  t393 = ((int64_t)(t383));
  t394 = fg107_cpi(t380, t381, t382, t393);
  (void)(t394);
  t395 = ((int64_t)(v0));
  t396 = ((int64_t)(((int64_t)UINT64_C(0))));
  t397 = ((int64_t)(((int64_t)UINT64_C(6))));
  t398 = ((int64_t)(((int64_t)UINT64_C(0))));
  t399 = fg107_cpi(t395, t396, t397, t398);
  v8 = ((int64_t)(t399));
  t400 = ((int64_t)(v0));
  t401 = ((int64_t)(((int64_t)UINT64_C(1))));
  t402 = ((int64_t)(((int64_t)UINT64_C(417))));
  t404 = ((int64_t)(v8));
  t405 = ((int64_t)(((int64_t)UINT64_C(0))));
  t406 = ((t404) == (t405));
  if (t406) {
  {
  t407 = ((int64_t)(v0));
  t408 = ((int64_t)(((int64_t)UINT64_C(0))));
  t409 = ((int64_t)(((int64_t)UINT64_C(416))));
  t410 = ((int64_t)(((int64_t)UINT64_C(0))));
  t411 = fg107_cpi(t407, t408, t409, t410);
  t403 = ((int64_t)(t411));
  }
  } else {
  {
  t412 = ((int64_t)(v8));
  t413 = ((int64_t)(v0));
  t414 = ((int64_t)(((int64_t)UINT64_C(0))));
  t415 = ((int64_t)(((int64_t)UINT64_C(416))));
  t416 = ((int64_t)(((int64_t)UINT64_C(0))));
  t417 = fg107_cpi(t413, t414, t415, t416);
  t418 = ((int64_t)(t417));
  t419 = ((t412) < (t418));
  if (t419) {
  {
  t403 = ((int64_t)(v8));
  }
  } else {
  {
  t420 = ((int64_t)(v0));
  t421 = ((int64_t)(((int64_t)UINT64_C(0))));
  t422 = ((int64_t)(((int64_t)UINT64_C(416))));
  t423 = ((int64_t)(((int64_t)UINT64_C(0))));
  t424 = fg107_cpi(t420, t421, t422, t423);
  t403 = ((int64_t)(t424));
  }
  }
  }
  }
  t425 = ((int64_t)(t403));
  t426 = fg107_cpi(t400, t401, t402, t425);
  (void)(t426);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_9(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t0 = gg_function_10(t1, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(15))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg107_pol(t7, t8, t9, t10, t11, t12);
  (void)(t13);
  }
  } else {
  {
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(15))));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg107_pol(t14, t15, t16, t17, t18, t19);
  (void)(t20);
  }
  }
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(16))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg107_pol(t21, t22, t23, t24, t25, t26);
  v5 = ((int64_t)(t27));
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(16))));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = fg107_pol(t28, t29, t30, t31, t32, t33);
  v6 = ((int64_t)(t34));
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(12))));
  t37 = ((int64_t)(((int64_t)UINT64_C(7))));
  t38 = ((int64_t)(v3));
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(v5));
  t41 = ((int64_t)(((int64_t)UINT64_C(1000000))));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) * (uint64_t)(t41)))));
  t43 = ((int64_t)(v6));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = fg107_pol(t35, t36, t37, t38, t39, t44);
  (void)(t45);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_7(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
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
  t0 = gg_function_8(t1, t2, t3, t4, t5, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
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
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)UINT64_C(1));
  t3 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t2)))));
  t4 = ((t1) == (t3));
  t5 = t4;
  if (!t5) {
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) == (t7));
  t9 = t8;
  if (t9) {
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t10) == (t11));
  }
  t5 = t9;
  }
  t12 = t5;
  if (t12) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(32))));
  t15 = ((t13) < (t14));
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t17) != (t18));
  }
  t19 = t16;
  if (t19) {
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
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
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
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(v5));
  t0 = gg_function_6(t1, t2, t3, t4, t5, t6, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  uint8_t t115;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg107_pol(t1, t2, t3, t4, t5, t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(14))));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg107_pol(t11, t12, t13, t14, t15, t16);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(v2));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(4))));
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg107_pol(t20, t21, t22, t23, t24, t25);
  v5 = ((int64_t)(t26));
  t27 = ((int64_t)(v5));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t27) == (t28));
  if (t29) {
  {
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(12))));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg107_pol(t30, t31, t32, t33, t34, t35);
  (void)(t36);
  t37 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t37)))));
  goto gg_done;
  }
  }
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(5))));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = fg107_pol(t38, t39, t40, t41, t42, t43);
  v6 = ((int64_t)(t44));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t45 = ((int64_t)(v8));
  t46 = ((int64_t)(v6));
  if (!(((t45) < (t46)))) goto label2;
  {
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(6))));
  t49 = ((int64_t)(v8));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = fg107_pol(t47, t48, t49, t50, t51, t52);
  t54 = ((int64_t)(t53));
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(13))));
  t57 = ((int64_t)(v1));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = fg107_pol(t55, t56, t57, t58, t59, t60);
  t62 = ((int64_t)(t61));
  t63 = ((t54) == (t62));
  if (t63) {
  {
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(((int64_t)UINT64_C(7))));
  t66 = ((int64_t)(v8));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = fg107_pol(t64, t65, t66, t67, t68, t69);
  v9 = ((int64_t)(t70));
  t71 = ((int64_t)(v9));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t71) >= (t72)))) gg_fail("assertion failed");
  t73 = ((int64_t)(v0));
  t74 = ((int64_t)(((int64_t)UINT64_C(8))));
  t75 = ((int64_t)(v8));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = fg107_pol(t73, t74, t75, t76, t77, t78);
  v10 = ((int64_t)(t79));
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(((int64_t)UINT64_C(9))));
  t82 = ((int64_t)(v10));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = fg107_pol(t80, t81, t82, t83, t84, t85);
  v11 = ((int64_t)(t86));
  t87 = ((int64_t)(v0));
  t88 = ((int64_t)(((int64_t)UINT64_C(10))));
  t89 = ((int64_t)(v11));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = fg107_pol(t87, t88, t89, t90, t91, t92);
  v12 = ((int64_t)(t93));
  t94 = ((int64_t)(v0));
  t95 = ((int64_t)(((int64_t)UINT64_C(11))));
  t96 = ((int64_t)(v9));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = fg107_pol(t94, t95, t96, t97, t98, t99);
  v13 = ((int64_t)(t100));
  t101 = ((int64_t)(v12));
  t102 = ((int64_t)(v13));
  t103 = ((t101) != (t102));
  if (t103) {
  {
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(((int64_t)UINT64_C(12))));
  t106 = ((int64_t)(((int64_t)UINT64_C(1))));
  t107 = ((int64_t)(v9));
  t108 = ((int64_t)(v12));
  t109 = ((int64_t)(v13));
  t110 = fg107_pol(t104, t105, t106, t107, t108, t109);
  (void)(t110);
  v7 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  }
  t111 = v8;
  t112 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  }
  goto label1;
label2:;
  t113 = ((int64_t)(v7));
  t114 = ((int64_t)(((int64_t)UINT64_C(0))));
  t115 = ((t113) != (t114));
  if (t115) {
  {
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(((int64_t)UINT64_C(1))));
  t118 = ((int64_t)(v1));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = fg107_pol(t116, t117, t118, t119, t120, t121);
  (void)(t122);
  t123 = ((int64_t)(v0));
  t124 = ((int64_t)(((int64_t)UINT64_C(12))));
  t125 = ((int64_t)(((int64_t)UINT64_C(3))));
  t126 = ((int64_t)(v1));
  t127 = ((int64_t)(((int64_t)UINT64_C(0))));
  t128 = ((int64_t)(((int64_t)UINT64_C(0))));
  t129 = fg107_pol(t123, t124, t125, t126, t127, t128);
  (void)(t129);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t130 = ((int64_t)(v0));
  t131 = ((int64_t)(((int64_t)UINT64_C(1))));
  t132 = ((int64_t)(v1));
  t133 = ((int64_t)(((int64_t)UINT64_C(1))));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((int64_t)(((int64_t)UINT64_C(0))));
  t136 = fg107_pol(t130, t131, t132, t133, t134, t135);
  (void)(t136);
  t137 = ((int64_t)(v0));
  t138 = ((int64_t)(((int64_t)UINT64_C(12))));
  t139 = ((int64_t)(((int64_t)UINT64_C(2))));
  t140 = ((int64_t)(v1));
  t141 = ((int64_t)(((int64_t)UINT64_C(0))));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = fg107_pol(t137, t138, t139, t140, t141, t142);
  (void)(t143);
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
  uint8_t t10;
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
  int64_t t50;
  uint8_t t51;
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
  uint8_t t111;
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
  uint8_t t126;
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
  uint8_t t144;
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
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg107_pol(t1, t2, t3, t4, t5, t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) != (t9));
  if (t10) {
  {
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(v1));
  t15 = gg_function_4(t11, t12, t13, t14, base + 0);
  t16 = ((int64_t)(t15));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  if (t18) {
  {
  t19 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t19)))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(3))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg107_pol(t20, t21, t22, t23, t24, t25);
  (void)(t26);
  }
  }
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = fg107_pol(t27, t28, t29, t30, t31, t32);
  t34 = ((int64_t)(t33));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) != (t35));
  if (t36) {
  {
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(12))));
  t39 = ((int64_t)(((int64_t)UINT64_C(4))));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = fg107_pol(t37, t38, t39, t40, t41, t42);
  (void)(t43);
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  t46 = ((int64_t)(v2));
  t47 = ((int64_t)(v1));
  t48 = gg_function_4(t44, t45, t46, t47, base + 0);
  t49 = ((int64_t)(t48));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((t49) != (t50));
  if (t51) {
  {
  t52 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t52)))));
  goto gg_done;
  }
  }
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(2))));
  t55 = ((int64_t)(v2));
  t56 = ((int64_t)(v1));
  t57 = gg_function_4(t53, t54, t55, t56, base + 0);
  t58 = ((int64_t)(t57));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((t58) != (t59));
  if (t60) {
  {
  t61 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t61)))));
  goto gg_done;
  }
  }
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(((int64_t)UINT64_C(3))));
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = fg107_pol(t62, t63, t64, t65, t66, t67);
  (void)(t68);
  }
  }
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(((int64_t)UINT64_C(2))));
  t71 = ((int64_t)(((int64_t)UINT64_C(2))));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = fg107_pol(t69, t70, t71, t72, t73, t74);
  t76 = ((int64_t)(t75));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t76) != (t77));
  if (t78) {
  {
  t79 = ((int64_t)(v0));
  t80 = ((int64_t)(((int64_t)UINT64_C(12))));
  t81 = ((int64_t)(((int64_t)UINT64_C(5))));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = fg107_pol(t79, t80, t81, t82, t83, t84);
  (void)(t85);
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(3))));
  t88 = ((int64_t)(v2));
  t89 = ((int64_t)(v1));
  t90 = gg_function_4(t86, t87, t88, t89, base + 0);
  t91 = ((int64_t)(t90));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t91) != (t92));
  if (t93) {
  {
  t94 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t94)))));
  goto gg_done;
  }
  }
  t95 = ((int64_t)(v0));
  t96 = ((int64_t)(((int64_t)UINT64_C(3))));
  t97 = ((int64_t)(((int64_t)UINT64_C(2))));
  t98 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = fg107_pol(t95, t96, t97, t98, t99, t100);
  (void)(t101);
  }
  }
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(((int64_t)UINT64_C(2))));
  t104 = ((int64_t)(((int64_t)UINT64_C(3))));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = fg107_pol(t102, t103, t104, t105, t106, t107);
  t109 = ((int64_t)(t108));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((t109) != (t110));
  if (t111) {
  {
  t112 = ((int64_t)(v0));
  t113 = ((int64_t)(((int64_t)UINT64_C(12))));
  t114 = ((int64_t)(((int64_t)UINT64_C(6))));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t118 = fg107_pol(t112, t113, t114, t115, t116, t117);
  (void)(t118);
  t119 = ((int64_t)(v0));
  t120 = ((int64_t)(((int64_t)UINT64_C(4))));
  t121 = ((int64_t)(v2));
  t122 = ((int64_t)(v1));
  t123 = gg_function_4(t119, t120, t121, t122, base + 0);
  t124 = ((int64_t)(t123));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((t124) != (t125));
  if (t126) {
  {
  t127 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t127)))));
  goto gg_done;
  }
  }
  t128 = ((int64_t)(v0));
  t129 = ((int64_t)(((int64_t)UINT64_C(5))));
  t130 = ((int64_t)(v2));
  t131 = ((int64_t)(v1));
  t132 = gg_function_4(t128, t129, t130, t131, base + 0);
  t133 = ((int64_t)(t132));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((t133) != (t134));
  if (t135) {
  {
  t136 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t136)))));
  goto gg_done;
  }
  }
  t137 = ((int64_t)(v0));
  t138 = ((int64_t)(((int64_t)UINT64_C(6))));
  t139 = ((int64_t)(v2));
  t140 = ((int64_t)(v1));
  t141 = gg_function_4(t137, t138, t139, t140, base + 0);
  t142 = ((int64_t)(t141));
  t143 = ((int64_t)(((int64_t)UINT64_C(0))));
  t144 = ((t142) != (t143));
  if (t144) {
  {
  t145 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t145)))));
  goto gg_done;
  }
  }
  t146 = ((int64_t)(v0));
  t147 = ((int64_t)(((int64_t)UINT64_C(3))));
  t148 = ((int64_t)(((int64_t)UINT64_C(3))));
  t149 = ((int64_t)(((int64_t)UINT64_C(0))));
  t150 = ((int64_t)(((int64_t)UINT64_C(0))));
  t151 = ((int64_t)(((int64_t)UINT64_C(0))));
  t152 = fg107_pol(t146, t147, t148, t149, t150, t151);
  (void)(t152);
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t0 = gg_function_2(t1, t2, t3, base + 0);
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
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  gg_header t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  v0 = gg_function_1(t0, t1, t2, base + 0);
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  v1 = gg_function_5(t3, t4, t5, t6, t7, t8, base + 0);
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  v2 = gg_function_7(t9, t10, t11, t12, t13, base + 0);
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  v3 = gg_function_9(t14, base + 0);
  t15.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t15.len = 0;
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
  t21 = ((int64_t)(v3));
  gg_integer(gg_ref(&t15, gg_get_stack(base + 0)), ((int64_t)((uint64_t)(t20) + (uint64_t)(t21))));
  gg_append(gg_ref(&t15, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t15).base, (t15).len));
  }
gg_done:;
  for (t22 = 0; t22 < 1; t22++) gg_stacks[base + t22].top = 0;
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
