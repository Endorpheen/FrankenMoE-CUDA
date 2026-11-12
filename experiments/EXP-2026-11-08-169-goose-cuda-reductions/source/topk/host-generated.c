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
static uint8_t gg_eq_6(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint64_t value);
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
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
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
static uint8_t gg_function_1(int64_t p0, int64_t base);
static uint8_t gg_function_3(int64_t p0, int64_t base);
static uint8_t gg_function_5(int64_t p0, int64_t base);
static void gg_function_9(uint8_t p0, int64_t p1, int64_t base);
static void gg_function_11(int64_t p0, int64_t base);
static uint64_t gg_function_13(int64_t p0, int64_t p1, int64_t base);
static uint64_t gg_function_14(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t gg_function_15(int64_t base);
static uint64_t gg_function_16(uint64_t p0, uint64_t p1, int64_t base);
static uint8_t gg_function_17(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base);
static void gg_function_9(uint8_t p0, int64_t p1, int64_t base) {
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
  fg169t_fail(t2);
  }
  }
  }
gg_done:;
}
static uint8_t gg_function_17(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base) {
  uint8_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint8_t t5;
  uint8_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v2));
  t3 = ((uint64_t)(v3));
  t4 = ((uint64_t)(((uint64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = ((t1) < (t4));
  t6 = t5;
  if (t6) {
  t7 = ((uint64_t)(v2));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t10 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t6 = ((t7) < (t10));
  }
  t0 = t6;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_15(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_14(uint64_t p0, uint64_t p1, int64_t base) {
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
  t6 = gg_function_15(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_function_9(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_16(uint64_t p0, uint64_t p1, int64_t base) {
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
  t2 = gg_function_15(base + 0);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(v1));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((t1) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_9(t6, t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_13(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t20;
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
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  uint64_t t57;
  uint64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint64_t t65;
  uint64_t t66;
  uint8_t t67;
  uint8_t t68;
  uint64_t t69;
  uint64_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint8_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  uint8_t t82;
  uint8_t t83;
  uint64_t t84;
  uint64_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  uint64_t v6;
  int64_t v7;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg169t_ti(t5, t6, t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(v1));
  t4 = ((t9) == (t10));
  }
  t11 = t4;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg169t_ti(t13, t14, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t17) != (t18));
  }
  t19 = t12;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(4))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg169t_ti(t21, t22, t23);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = ((t25) == (t26));
  }
  t27 = t20;
  t28 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_9(t27, t28, base + 0);
  v2 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t29 = ((int64_t)(v3));
  t30 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t29) < (t30)))) goto label2;
  {
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  t33 = ((int64_t)(v3));
  t34 = fg169t_ti(t31, t32, t33);
  v4 = ((int64_t)(t34));
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(4))));
  t37 = ((int64_t)(v3));
  t38 = fg169t_ti(t35, t36, t37);
  v5 = ((int64_t)(t38));
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((t39) > (t40));
  t42 = t41;
  if (t42) {
  t43 = ((int64_t)(v5));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t43) >= (t44));
  }
  t45 = t42;
  t46 = t45;
  if (t46) {
  t47 = ((uint64_t)(((uint64_t)(v5))));
  t48 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t49 = ((uint64_t)(((uint64_t)gg_umod(t47, t48))));
  t50 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t46 = ((t49) == (t50));
  }
  t51 = t46;
  t52 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_9(t51, t52, base + 0);
  t53 = ((uint64_t)(v2));
  t54 = ((int64_t)(v4));
  t55 = ((int64_t)(((int64_t)UINT64_C(1))));
  t56 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t54) - (uint64_t)(t55)))))));
  t57 = ((uint64_t)(((uint64_t)(v5))));
  t58 = gg_function_14(t56, t57, base + 0);
  v2 = gg_function_16(t53, t58, base + 0);
  t59 = v3;
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t59) + (uint64_t)(t60)));
  }
  goto label1;
label2:;
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = fg169t_ti(t61, t62, t63);
  v6 = ((uint64_t)(((uint64_t)(t64))));
  t65 = ((uint64_t)(v2));
  t66 = ((uint64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t67 = ((t65) <= (t66));
  t68 = t67;
  if (t68) {
  t69 = ((uint64_t)(v6));
  t70 = gg_function_15(base + 0);
  t71 = ((uint64_t)(t70));
  t72 = ((uint64_t)(v2));
  t73 = ((uint64_t)(((uint64_t)((uint64_t)(t71) - (uint64_t)(t72)))));
  t68 = ((t69) <= (t73));
  }
  t74 = t68;
  t75 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_9(t74, t75, base + 0);
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(((int64_t)UINT64_C(9))));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = fg169t_ti(t76, t77, t78);
  v7 = ((int64_t)(t79));
  t80 = ((int64_t)(v7));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = ((t80) > (t81));
  t83 = t82;
  if (t83) {
  t84 = ((uint64_t)(v2));
  t85 = ((uint64_t)(((uint64_t)(v7))));
  t83 = ((t84) <= (t85));
  }
  t86 = t83;
  t87 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_9(t86, t87, base + 0);
  t0 = ((uint64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_5(int64_t p0, int64_t base) {
  uint8_t t0;
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
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t4 = ((t5) == (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  t8 = ((t9) == (t10));
  }
  t11 = t8;
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(8))));
  t12 = ((t13) == (t14));
  }
  t15 = t12;
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(16))));
  t16 = ((t17) == (t18));
  }
  t19 = t16;
  t20 = t19;
  if (!t20) {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(32))));
  t20 = ((t21) == (t22));
  }
  t23 = t20;
  t24 = t23;
  if (!t24) {
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(64))));
  t24 = ((t25) == (t26));
  }
  t27 = t24;
  t28 = t27;
  if (!t28) {
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(128))));
  t28 = ((t29) == (t30));
  }
  t31 = t28;
  t32 = t31;
  if (!t32) {
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(256))));
  t32 = ((t33) == (t34));
  }
  t35 = t32;
  t36 = t35;
  if (!t36) {
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(288))));
  t36 = ((t37) == (t38));
  }
  t39 = t36;
  t40 = t39;
  if (!t40) {
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(512))));
  t40 = ((t41) == (t42));
  }
  t43 = t40;
  t44 = t43;
  if (!t44) {
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(576))));
  t44 = ((t45) == (t46));
  }
  t0 = t44;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_11(int64_t p0, int64_t base) {
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
  uint8_t t25;
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
  uint8_t t39;
  uint8_t t40;
  int64_t t41;
  uint8_t t42;
  uint8_t t43;
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
  uint8_t t58;
  uint8_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  uint8_t t67;
  uint8_t t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
  uint8_t t72;
  int64_t t73;
  int64_t t74;
  uint8_t t75;
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
  uint64_t t87;
  uint64_t t88;
  uint64_t t89;
  uint64_t t90;
  uint64_t t91;
  uint64_t t92;
  uint64_t t93;
  uint64_t t94;
  int64_t t95;
  int64_t t96;
  uint64_t t97;
  uint64_t t98;
  uint64_t t99;
  uint64_t t100;
  uint64_t t101;
  uint64_t t102;
  uint64_t t103;
  uint64_t t104;
  uint8_t t105;
  uint8_t t106;
  uint64_t t107;
  uint64_t t108;
  uint8_t t109;
  uint8_t t110;
  uint64_t t111;
  uint64_t t112;
  uint8_t t113;
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
  uint64_t t127;
  uint64_t t128;
  uint64_t t129;
  uint64_t t130;
  uint8_t t131;
  uint8_t t132;
  uint8_t t133;
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
  uint8_t t144;
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
  int64_t t156;
  int64_t t157;
  uint8_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  uint8_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint8_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  uint8_t t178;
  uint8_t t179;
  uint8_t t180;
  uint8_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  uint8_t t188;
  uint64_t t189;
  uint64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  uint8_t t196;
  uint64_t t197;
  uint64_t t198;
  int64_t t199;
  int64_t t200;
  uint8_t t201;
  uint64_t t202;
  uint64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  uint8_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  float t216;
  float t217;
  uint8_t t218;
  uint64_t t219;
  uint64_t t220;
  int64_t t221;
  int64_t t222;
  uint8_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  uint8_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  float t234;
  int64_t t235;
  int64_t t236;
  float t237;
  int64_t t238;
  int64_t t239;
  uint8_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  uint8_t t244;
  uint8_t t245;
  uint8_t t246;
  uint64_t t247;
  uint64_t t248;
  uint64_t t249;
  uint64_t t250;
  uint64_t t251;
  uint8_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  uint64_t t258;
  uint64_t t259;
  uint64_t t260;
  uint64_t t261;
  uint8_t t262;
  uint8_t t263;
  uint8_t t264;
  uint8_t t265;
  uint64_t t266;
  uint64_t t267;
  uint64_t t268;
  uint64_t t269;
  uint8_t t270;
  uint8_t t271;
  uint8_t t272;
  int64_t t273;
  uint64_t t274;
  uint64_t t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  uint64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  uint64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  uint64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  uint64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  uint64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  uint64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  uint64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  uint64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  uint64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  uint64_t t315;
  int64_t t316;
  int64_t t317;
  uint8_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  uint64_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  uint64_t t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  uint64_t t332;
  uint64_t t333;
  uint64_t t334;
  uint64_t t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  uint8_t t340;
  uint64_t t341;
  uint64_t t342;
  int64_t t343;
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
  uint64_t v10;
  uint64_t v11;
  uint64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  uint64_t v16;
  uint64_t v17;
  uint64_t v18;
  uint64_t v19;
  uint64_t v20;
  uint64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  uint8_t v25;
  uint64_t v26;
  float v27;
  uint64_t v28;
  uint64_t v29;
  int64_t v30;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = fg169t_plan_ptr(t0);
  v1 = ((int64_t)(t1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(2))));
  t4 = fg169t_arg(t2, t3);
  v2 = ((int64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(3))));
  t7 = fg169t_arg(t5, t6);
  v3 = ((int64_t)(t7));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(4))));
  t10 = fg169t_arg(t8, t9);
  v4 = ((int64_t)(t10));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(5))));
  t13 = fg169t_arg(t11, t12);
  v5 = ((int64_t)(t13));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(6))));
  t16 = fg169t_arg(t14, t15);
  v6 = ((int64_t)(t16));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(7))));
  t19 = fg169t_arg(t17, t18);
  v7 = ((int64_t)(t19));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = fg169t_arg(t20, t21);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) != (t24));
  t26 = ((int64_t)(((int64_t)UINT64_C(7))));
  gg_function_9(t25, t26, base + 0);
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = fg169t_constant(t27);
  v8 = ((int64_t)(t28));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = fg169t_constant(t29);
  v9 = ((int64_t)(t30));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(v8));
  v10 = gg_function_13(t31, t32, base + 0);
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(v8));
  v11 = gg_function_13(t33, t34, base + 0);
  t35 = ((int64_t)(v4));
  t36 = ((int64_t)(v9));
  v12 = gg_function_13(t35, t36, base + 0);
  t37 = ((int64_t)(v2));
  t38 = fg169t_contiguous(t37);
  t39 = t38;
  t40 = t39;
  if (t40) {
  t41 = ((int64_t)(v3));
  t42 = fg169t_contiguous(t41);
  t40 = t42;
  }
  t43 = t40;
  t44 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_function_9(t43, t44, base + 0);
  t45 = ((int64_t)(v2));
  t46 = ((int64_t)(((int64_t)UINT64_C(3))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = fg169t_ti(t45, t46, t47);
  v13 = ((int64_t)(t48));
  t49 = ((int64_t)(v2));
  t50 = ((int64_t)(((int64_t)UINT64_C(3))));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  t52 = fg169t_ti(t49, t50, t51);
  v14 = ((int64_t)(t52));
  t53 = ((int64_t)(v3));
  t54 = ((int64_t)(((int64_t)UINT64_C(3))));
  t55 = ((int64_t)(((int64_t)UINT64_C(1))));
  t56 = fg169t_ti(t53, t54, t55);
  v15 = ((int64_t)(t56));
  t57 = ((int64_t)(v13));
  t58 = gg_function_5(t57, base + 0);
  t59 = t58;
  t60 = t59;
  if (t60) {
  t61 = ((int64_t)(v14));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((t61) > (t62));
  }
  t63 = t60;
  t64 = t63;
  if (t64) {
  t65 = ((int64_t)(v14));
  t66 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t64 = ((t65) <= (t66));
  }
  t67 = t64;
  t68 = t67;
  if (t68) {
  t69 = ((int64_t)(v15));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = ((t69) > (t70));
  }
  t71 = t68;
  t72 = t71;
  if (t72) {
  t73 = ((int64_t)(v15));
  t74 = ((int64_t)(v13));
  t72 = ((t73) <= (t74));
  }
  t75 = t72;
  t76 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_function_9(t75, t76, base + 0);
  t77 = ((int64_t)(v4));
  t78 = ((int64_t)(((int64_t)UINT64_C(4))));
  t79 = ((int64_t)(((int64_t)UINT64_C(1))));
  t80 = fg169t_ti(t77, t78, t79);
  t81 = ((int64_t)(t80));
  t82 = ((int64_t)(v13));
  t83 = ((int64_t)(((int64_t)UINT64_C(4))));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) * (uint64_t)(t83)))));
  t85 = ((t81) == (t84));
  t86 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_9(t85, t86, base + 0);
  t87 = ((uint64_t)(((uint64_t)(v13))));
  t88 = ((uint64_t)(((uint64_t)(v14))));
  t89 = gg_function_14(t87, t88, base + 0);
  t90 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  v16 = gg_function_14(t89, t90, base + 0);
  t91 = ((uint64_t)(((uint64_t)(v15))));
  t92 = ((uint64_t)(((uint64_t)(v14))));
  t93 = gg_function_14(t91, t92, base + 0);
  t94 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  v17 = gg_function_14(t93, t94, base + 0);
  t95 = ((int64_t)(v14));
  t96 = ((int64_t)(((int64_t)UINT64_C(1))));
  t97 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t95) - (uint64_t)(t96)))))));
  t98 = ((uint64_t)(((uint64_t)(v13))));
  t99 = gg_function_14(t97, t98, base + 0);
  t100 = ((uint64_t)(((uint64_t)(v15))));
  t101 = gg_function_16(t99, t100, base + 0);
  t102 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  v18 = gg_function_14(t101, t102, base + 0);
  t103 = ((uint64_t)(v10));
  t104 = ((uint64_t)(v16));
  t105 = ((t103) >= (t104));
  t106 = t105;
  if (t106) {
  t107 = ((uint64_t)(v11));
  t108 = ((uint64_t)(v17));
  t106 = ((t107) >= (t108));
  }
  t109 = t106;
  t110 = t109;
  if (t110) {
  t111 = ((uint64_t)(v12));
  t112 = ((uint64_t)(v18));
  t110 = ((t111) >= (t112));
  }
  t113 = t110;
  t114 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_9(t113, t114, base + 0);
  t115 = ((int64_t)(v2));
  t116 = ((int64_t)(((int64_t)UINT64_C(1))));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t118 = fg169t_ti(t115, t116, t117);
  v19 = ((uint64_t)(((uint64_t)(t118))));
  t119 = ((int64_t)(v3));
  t120 = ((int64_t)(((int64_t)UINT64_C(1))));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = fg169t_ti(t119, t120, t121);
  v20 = ((uint64_t)(((uint64_t)(t122))));
  t123 = ((int64_t)(v4));
  t124 = ((int64_t)(((int64_t)UINT64_C(1))));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = fg169t_ti(t123, t124, t125);
  v21 = ((uint64_t)(((uint64_t)(t126))));
  t127 = ((uint64_t)(v20));
  t128 = ((uint64_t)(v17));
  t129 = ((uint64_t)(v21));
  t130 = ((uint64_t)(v18));
  t131 = gg_function_17(t127, t128, t129, t130, base + 0);
  t132 = t131;
  t133 = (!(t132));
  t134 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_function_9(t133, t134, base + 0);
  t135 = ((int64_t)(v0));
  t136 = ((int64_t)(((int64_t)UINT64_C(3))));
  t137 = fg169t_cfg(t135, t136);
  v22 = ((int64_t)(t137));
  t138 = ((int64_t)(v0));
  t139 = ((int64_t)(((int64_t)UINT64_C(4))));
  t140 = fg169t_cfg(t138, t139);
  v23 = ((int64_t)(t140));
  t141 = ((int64_t)(v22));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = ((t141) >= (t142));
  t144 = t143;
  if (t144) {
  t145 = ((int64_t)(v22));
  t146 = ((int64_t)(((int64_t)UINT64_C(32))));
  t144 = ((t145) <= (t146));
  }
  t147 = t144;
  t148 = t147;
  if (t148) {
  t149 = ((int64_t)(v22));
  t150 = ((int64_t)(v13));
  t148 = ((t149) <= (t150));
  }
  t151 = t148;
  t152 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_9(t151, t152, base + 0);
  t153 = ((int64_t)(v22));
  t154 = ((int64_t)(((int64_t)UINT64_C(0))));
  t155 = ((t153) > (t154));
  if (t155) {
  {
  t156 = ((int64_t)(v23));
  t157 = ((int64_t)(((int64_t)UINT64_C(1))));
  t158 = ((t156) >= (t157));
  t159 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_function_9(t158, t159, base + 0);
  t160 = ((int64_t)(v23));
  t161 = ((int64_t)(v22));
  t162 = ((t160) < (t161));
  if (t162) {
  {
  v24 = ((int64_t)(v23));
  }
  } else {
  {
  v24 = ((int64_t)(v22));
  }
  }
  t163 = ((int64_t)(v15));
  t164 = ((int64_t)(v24));
  t165 = ((int64_t)(v13));
  t166 = ((int64_t)(v22));
  t167 = ((int64_t)(((int64_t)gg_div(t165, t166))));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t164) * (uint64_t)(t167)))));
  t169 = ((t163) <= (t168));
  t170 = ((int64_t)(((int64_t)UINT64_C(15))));
  gg_function_9(t169, t170, base + 0);
  }
  }
  t171 = ((int64_t)(v0));
  t172 = ((int64_t)(((int64_t)UINT64_C(2))));
  t173 = fg169t_cfg(t171, t172);
  t174 = ((int64_t)(t173));
  t175 = ((int64_t)(((int64_t)UINT64_C(0))));
  v25 = ((t174) != (t175));
  t176 = ((int64_t)(v5));
  t177 = ((int64_t)(((int64_t)UINT64_C(0))));
  t178 = ((t176) != (t177));
  t179 = t178;
  if (t179) {
  t179 = v25;
  }
  t180 = t179;
  t181 = (!(t180));
  t182 = ((int64_t)(((int64_t)UINT64_C(16))));
  gg_function_9(t181, t182, base + 0);
  v26 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t183 = ((int64_t)(v0));
  t184 = ((int64_t)(((int64_t)UINT64_C(0))));
  t185 = fg169t_cfg(t183, t184);
  t186 = ((int64_t)(t185));
  t187 = ((int64_t)(((int64_t)UINT64_C(0))));
  t188 = ((t186) != (t187));
  if (t188) {
  {
  t189 = v26;
  t190 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v26 = ((uint64_t)((uint64_t)(t189) + (uint64_t)(t190)));
  }
  }
  t191 = ((int64_t)(v0));
  t192 = ((int64_t)(((int64_t)UINT64_C(1))));
  t193 = fg169t_cfg(t191, t192);
  t194 = ((int64_t)(t193));
  t195 = ((int64_t)(((int64_t)UINT64_C(0))));
  t196 = ((t194) != (t195));
  if (t196) {
  {
  t197 = v26;
  t198 = ((uint64_t)(((int64_t)UINT64_C(2))));
  v26 = ((uint64_t)((uint64_t)(t197) + (uint64_t)(t198)));
  }
  }
  t199 = ((int64_t)(v5));
  t200 = ((int64_t)(((int64_t)UINT64_C(0))));
  t201 = ((t199) != (t200));
  if (t201) {
  {
  t202 = v26;
  t203 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v26 = ((uint64_t)((uint64_t)(t202) + (uint64_t)(t203)));
  t204 = ((int64_t)(v5));
  t205 = ((int64_t)(((int64_t)UINT64_C(2))));
  t206 = ((int64_t)(((int64_t)UINT64_C(0))));
  t207 = fg169t_ti(t204, t205, t206);
  t208 = ((int64_t)(t207));
  t209 = ((int64_t)(v8));
  t210 = ((t208) == (t209));
  t211 = ((int64_t)(((int64_t)UINT64_C(17))));
  gg_function_9(t210, t211, base + 0);
  t212 = ((int64_t)(v1));
  t213 = ((int64_t)(((int64_t)UINT64_C(0))));
  t214 = ((int64_t)(v5));
  t215 = ((int64_t)(((int64_t)UINT64_C(0))));
  t216 = fg169t_tf(t214, t215);
  t217 = ((float)(t216));
  fg169t_hfloat(t212, t213, t217);
  }
  }
  t218 = v25;
  if (t218) {
  {
  t219 = v26;
  t220 = ((uint64_t)(((int64_t)UINT64_C(8))));
  v26 = ((uint64_t)((uint64_t)(t219) + (uint64_t)(t220)));
  }
  }
  v27 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t221 = ((int64_t)(v6));
  t222 = ((int64_t)(((int64_t)UINT64_C(0))));
  t223 = ((t221) != (t222));
  if (t223) {
  {
  t224 = ((int64_t)(v6));
  t225 = ((int64_t)(((int64_t)UINT64_C(2))));
  t226 = ((int64_t)(((int64_t)UINT64_C(0))));
  t227 = fg169t_ti(t224, t225, t226);
  t228 = ((int64_t)(t227));
  t229 = ((int64_t)(v8));
  t230 = ((t228) == (t229));
  t231 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_9(t230, t231, base + 0);
  t232 = ((int64_t)(v6));
  t233 = ((int64_t)(((int64_t)UINT64_C(0))));
  t234 = fg169t_tf(t232, t233);
  v27 = ((float)(t234));
  }
  }
  t235 = ((int64_t)(v1));
  t236 = ((int64_t)(((int64_t)UINT64_C(1))));
  t237 = ((float)(v27));
  fg169t_hfloat(t235, t236, t237);
  t238 = ((int64_t)(v7));
  t239 = ((int64_t)(((int64_t)UINT64_C(0))));
  t240 = ((t238) != (t239));
  if (t240) {
  {
  t241 = ((int64_t)(v7));
  t242 = ((int64_t)(v8));
  v28 = gg_function_13(t241, t242, base + 0);
  t243 = ((int64_t)(v7));
  t244 = fg169t_contiguous(t243);
  t245 = t244;
  t246 = t245;
  if (t246) {
  t247 = ((uint64_t)(v28));
  t248 = ((uint64_t)(((uint64_t)(v13))));
  t249 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t250 = gg_function_14(t248, t249, base + 0);
  t251 = ((uint64_t)(t250));
  t246 = ((t247) >= (t251));
  }
  t252 = t246;
  t253 = ((int64_t)(((int64_t)UINT64_C(19))));
  gg_function_9(t252, t253, base + 0);
  t254 = ((int64_t)(v7));
  t255 = ((int64_t)(((int64_t)UINT64_C(1))));
  t256 = ((int64_t)(((int64_t)UINT64_C(0))));
  t257 = fg169t_ti(t254, t255, t256);
  v29 = ((uint64_t)(((uint64_t)(t257))));
  t258 = ((uint64_t)(v29));
  t259 = ((uint64_t)(v28));
  t260 = ((uint64_t)(v20));
  t261 = ((uint64_t)(v17));
  t262 = gg_function_17(t258, t259, t260, t261, base + 0);
  t263 = t262;
  t264 = (!(t263));
  t265 = t264;
  if (t265) {
  t266 = ((uint64_t)(v29));
  t267 = ((uint64_t)(v28));
  t268 = ((uint64_t)(v21));
  t269 = ((uint64_t)(v18));
  t270 = gg_function_17(t266, t267, t268, t269, base + 0);
  t271 = t270;
  t265 = (!(t271));
  }
  t272 = t265;
  t273 = ((int64_t)(((int64_t)UINT64_C(20))));
  gg_function_9(t272, t273, base + 0);
  t274 = v26;
  t275 = ((uint64_t)(((int64_t)UINT64_C(16))));
  v26 = ((uint64_t)((uint64_t)(t274) + (uint64_t)(t275)));
  t276 = ((int64_t)(v1));
  t277 = ((int64_t)(((int64_t)UINT64_C(0))));
  t278 = ((int64_t)(((int64_t)UINT64_C(3))));
  t279 = ((uint64_t)(v29));
  fg169t_hput(t276, t277, t278, t279);
  }
  }
  t280 = ((int64_t)(v1));
  t281 = ((int64_t)(((int64_t)UINT64_C(0))));
  t282 = ((int64_t)(((int64_t)UINT64_C(0))));
  t283 = ((uint64_t)(v19));
  fg169t_hput(t280, t281, t282, t283);
  t284 = ((int64_t)(v1));
  t285 = ((int64_t)(((int64_t)UINT64_C(0))));
  t286 = ((int64_t)(((int64_t)UINT64_C(1))));
  t287 = ((uint64_t)(v20));
  fg169t_hput(t284, t285, t286, t287);
  t288 = ((int64_t)(v1));
  t289 = ((int64_t)(((int64_t)UINT64_C(0))));
  t290 = ((int64_t)(((int64_t)UINT64_C(2))));
  t291 = ((uint64_t)(v21));
  fg169t_hput(t288, t289, t290, t291);
  t292 = ((int64_t)(v1));
  t293 = ((int64_t)(((int64_t)UINT64_C(3))));
  t294 = ((int64_t)(((int64_t)UINT64_C(0))));
  t295 = ((uint64_t)(((uint64_t)(v13))));
  fg169t_hput(t292, t293, t294, t295);
  t296 = ((int64_t)(v1));
  t297 = ((int64_t)(((int64_t)UINT64_C(3))));
  t298 = ((int64_t)(((int64_t)UINT64_C(1))));
  t299 = ((uint64_t)(((uint64_t)(v14))));
  fg169t_hput(t296, t297, t298, t299);
  t300 = ((int64_t)(v1));
  t301 = ((int64_t)(((int64_t)UINT64_C(3))));
  t302 = ((int64_t)(((int64_t)UINT64_C(2))));
  t303 = ((uint64_t)(((uint64_t)(v15))));
  fg169t_hput(t300, t301, t302, t303);
  t304 = ((int64_t)(v1));
  t305 = ((int64_t)(((int64_t)UINT64_C(3))));
  t306 = ((int64_t)(((int64_t)UINT64_C(3))));
  t307 = ((uint64_t)(v26));
  fg169t_hput(t304, t305, t306, t307);
  t308 = ((int64_t)(v1));
  t309 = ((int64_t)(((int64_t)UINT64_C(3))));
  t310 = ((int64_t)(((int64_t)UINT64_C(4))));
  t311 = ((uint64_t)(((uint64_t)(v22))));
  fg169t_hput(t308, t309, t310, t311);
  t312 = ((int64_t)(v1));
  t313 = ((int64_t)(((int64_t)UINT64_C(3))));
  t314 = ((int64_t)(((int64_t)UINT64_C(5))));
  t315 = ((uint64_t)(((uint64_t)(v23))));
  fg169t_hput(t312, t313, t314, t315);
  t316 = ((int64_t)(v13));
  t317 = ((int64_t)(((int64_t)UINT64_C(32))));
  t318 = ((t316) > (t317));
  if (t318) {
  {
  t319 = ((int64_t)(v13));
  t320 = ((int64_t)(((int64_t)UINT64_C(32))));
  v30 = ((int64_t)(((int64_t)gg_div(t319, t320))));
  }
  } else {
  {
  v30 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t321 = ((int64_t)(v1));
  t322 = ((int64_t)(((int64_t)UINT64_C(3))));
  t323 = ((int64_t)(((int64_t)UINT64_C(6))));
  t324 = ((uint64_t)(((uint64_t)(v30))));
  fg169t_hput(t321, t322, t323, t324);
  t325 = ((int64_t)(v1));
  t326 = ((int64_t)(((int64_t)UINT64_C(3))));
  t327 = ((int64_t)(((int64_t)UINT64_C(7))));
  t328 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  fg169t_hput(t325, t326, t327, t328);
  t329 = ((int64_t)(v1));
  t330 = ((int64_t)(((int64_t)UINT64_C(3))));
  t331 = ((int64_t)(((int64_t)UINT64_C(8))));
  t332 = ((uint64_t)(((uint64_t)(v14))));
  t333 = ((uint64_t)(((int64_t)UINT64_C(8))));
  t334 = ((uint64_t)(((uint64_t)gg_udiv(t332, t333))));
  t336 = ((int64_t)(v14));
  t337 = ((int64_t)(((int64_t)UINT64_C(8))));
  t338 = ((int64_t)(((int64_t)gg_mod(t336, t337))));
  t339 = ((int64_t)(((int64_t)UINT64_C(0))));
  t340 = ((t338) != (t339));
  if (t340) {
  {
  t335 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t335 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t341 = ((uint64_t)(t335));
  t342 = ((uint64_t)(((uint64_t)((uint64_t)(t334) + (uint64_t)(t341)))));
  fg169t_hput(t329, t330, t331, t342);
  t343 = ((int64_t)(v0));
  fg169t_launch(t343);
  }
gg_done:;
}
static uint8_t gg_function_3(int64_t p0, int64_t base) {
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
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
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
  uint8_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  uint8_t t47;
  uint8_t t48;
  int64_t t49;
  uint8_t t50;
  uint8_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  uint8_t t55;
  uint8_t t56;
  uint8_t t57;
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
  uint8_t t73;
  uint8_t t74;
  int64_t t75;
  uint8_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  float t81;
  float t82;
  float t83;
  uint8_t t84;
  uint8_t t85;
  int64_t t86;
  int64_t t87;
  float t88;
  float t89;
  float t90;
  uint8_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  uint8_t t98;
  uint8_t t99;
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
  uint8_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
  uint8_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint8_t t126;
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
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(9))));
  t3 = fg169t_arg(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(3))));
  t6 = fg169t_arg(t4, t5);
  v2 = ((int64_t)(t6));
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  t9 = fg169t_arg(t7, t8);
  v3 = ((int64_t)(t9));
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(4))));
  t12 = fg169t_arg(t10, t11);
  v4 = ((int64_t)(t12));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) == (t14));
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t17) == (t18));
  }
  t19 = t16;
  t20 = t19;
  if (!t20) {
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t21) == (t22));
  }
  t23 = t20;
  t24 = t23;
  if (!t24) {
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t25) == (t26));
  }
  t27 = t24;
  if (t27) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg169t_ti(t28, t29, t30);
  v5 = ((int64_t)(t31));
  t32 = ((int64_t)(v4));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = fg169t_ti(t32, t33, t34);
  v6 = ((int64_t)(t35));
  t36 = ((int64_t)(v5));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) <= (t37));
  t39 = t38;
  if (!t39) {
  t40 = ((int64_t)(v6));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t40) < (t41));
  }
  t42 = t39;
  if (t42) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t43 = ((int64_t)(v6));
  t44 = ((int64_t)(v5));
  v7 = ((int64_t)(((int64_t)gg_div(t43, t44))));
  t45 = ((int64_t)(v7));
  t46 = gg_function_5(t45, base + 0);
  t47 = t46;
  t48 = (!(t47));
  if (t48) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t49 = ((int64_t)(v2));
  t50 = fg169t_contiguous(t49);
  t51 = t50;
  t52 = (!(t51));
  t53 = t52;
  if (!t53) {
  t54 = ((int64_t)(v3));
  t55 = fg169t_contiguous(t54);
  t56 = t55;
  t53 = (!(t56));
  }
  t57 = t53;
  if (t57) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t58 = ((int64_t)(v1));
  t59 = ((int64_t)(((int64_t)UINT64_C(6))));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = fg169t_ti(t58, t59, t60);
  v8 = ((int64_t)(t61));
  t62 = ((int64_t)(v8));
  t63 = ((int64_t)(((int64_t)UINT64_C(2))));
  t64 = fg169t_constant(t63);
  t65 = ((int64_t)(t64));
  t66 = ((t62) == (t65));
  if (t66) {
  {
  t67 = ((int64_t)(v1));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = fg169t_ti(t67, t68, t69);
  v9 = ((int64_t)(t70));
  t71 = ((int64_t)(v9));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((t71) == (t72));
  t74 = t73;
  if (!t74) {
  t75 = ((int64_t)(v9));
  t76 = fg169t_contiguous(t75);
  t77 = t76;
  t74 = (!(t77));
  }
  t78 = t74;
  if (t78) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t79 = ((int64_t)(v1));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = fg169t_tf(t79, t80);
  t82 = ((float)(t81));
  t83 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t84 = ((t82) != (t83));
  t85 = t84;
  if (!t85) {
  t86 = ((int64_t)(v1));
  t87 = ((int64_t)(((int64_t)UINT64_C(1))));
  t88 = fg169t_tf(t86, t87);
  t89 = ((float)(t88));
  t90 = ((float)(gg_double_bits(UINT64_C(0))));
  t85 = ((t89) != (t90));
  }
  t91 = t85;
  if (t91) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t92 = ((int64_t)(v1));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((int64_t)(((int64_t)UINT64_C(1))));
  t95 = fg169t_ti(t92, t93, t94);
  t96 = ((int64_t)(t95));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((t96) != (t97));
  t99 = t98;
  if (!t99) {
  t100 = ((int64_t)(v1));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((int64_t)(((int64_t)UINT64_C(2))));
  t103 = fg169t_ti(t100, t101, t102);
  t104 = ((int64_t)(t103));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((t104) != (t105));
  }
  t106 = t99;
  if (t106) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  }
  } else {
  {
  t107 = ((int64_t)(v8));
  t108 = ((int64_t)(((int64_t)UINT64_C(3))));
  t109 = fg169t_constant(t108);
  t110 = ((int64_t)(t109));
  t111 = ((t107) == (t110));
  if (t111) {
  {
  t112 = ((int64_t)(v1));
  t113 = ((int64_t)(((int64_t)UINT64_C(5))));
  t114 = ((int64_t)(((int64_t)UINT64_C(0))));
  t115 = fg169t_ti(t112, t113, t114);
  v10 = ((int64_t)(t115));
  t116 = ((int64_t)(v10));
  t117 = ((int64_t)(((int64_t)UINT64_C(4))));
  t118 = fg169t_constant(t117);
  t119 = ((int64_t)(t118));
  t120 = ((t116) != (t119));
  t121 = t120;
  if (t121) {
  t122 = ((int64_t)(v10));
  t123 = ((int64_t)(((int64_t)UINT64_C(5))));
  t124 = fg169t_constant(t123);
  t125 = ((int64_t)(t124));
  t121 = ((t122) != (t125));
  }
  t126 = t121;
  if (t126) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_1(int64_t p0, int64_t base) {
  uint8_t t0;
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg169t_arg(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v0));
  t0 = gg_function_3(t7, base + 0);
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) == (t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(21))));
  gg_function_9(t10, t11, base + 0);
  t12 = ((int64_t)(v0));
  gg_function_11(t12, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  int64_t t0;
  uint8_t t1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = gg_function_1(t0, base + 0);
  (void)(t1);
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
