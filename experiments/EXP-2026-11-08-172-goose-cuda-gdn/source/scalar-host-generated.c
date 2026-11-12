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
static uint8_t gg_eq_3(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint64_t value);
static uint8_t gg_eq_5(float a, float b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, float value);
static uint8_t gg_eq_6(double a, double b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, double value);
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
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(float a, float b) {
return a == b;
}
static void gg_render_5(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(double a, double b) {
return a == b;
}
static void gg_render_6(gg_reference out, double value) {
gg_float(out,value);
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
static void gg_function_1(int64_t p0, int64_t base);
static void gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static void gg_function_8(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint64_t gg_function_10(int64_t p0, int64_t base);
static uint64_t gg_function_11(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t gg_function_12(int64_t base);
static uint64_t gg_function_13(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t gg_function_18(uint64_t p0, uint64_t p1, int64_t base);
static void gg_function_21(int64_t p0, int64_t p1, uint64_t p2, int64_t base);
static uint64_t gg_function_22(uint64_t p0, uint64_t p1, int64_t base);
static void gg_function_23(int64_t p0, uint64_t p1, int64_t p2, int64_t base);
static uint8_t gg_function_24(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base);
static void gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  fg174s_fail(t2);
  }
  }
  }
gg_done:;
}
static uint8_t gg_function_24(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base) {
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
static uint64_t gg_function_12(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_11(uint64_t p0, uint64_t p1, int64_t base) {
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
  t6 = gg_function_12(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_3(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_13(uint64_t p0, uint64_t p1, int64_t base) {
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
  t2 = gg_function_12(base + 0);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(v1));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((t1) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_3(t6, t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_10(int64_t p0, int64_t base) {
  uint64_t t0;
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
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint8_t t33;
  uint8_t t34;
  uint64_t t35;
  uint64_t t36;
  uint8_t t37;
  uint8_t t38;
  uint64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  uint64_t t54;
  uint64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint64_t v5;
  int64_t v6;
  v0 = p0;
  {
  v1 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(3))));
  t5 = ((int64_t)(v2));
  t6 = fg174s_ti(t3, t4, t5);
  v3 = ((int64_t)(t6));
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(4))));
  t9 = ((int64_t)(v2));
  t10 = fg174s_ti(t7, t8, t9);
  v4 = ((int64_t)(t10));
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) > (t12));
  t14 = t13;
  if (t14) {
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t15) >= (t16));
  }
  t17 = t14;
  t18 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_3(t17, t18, base + 0);
  t19 = ((uint64_t)(v1));
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t20) - (uint64_t)(t21)))))));
  t23 = ((uint64_t)(((uint64_t)(v4))));
  t24 = gg_function_11(t22, t23, base + 0);
  v1 = gg_function_13(t19, t24, base + 0);
  t25 = v2;
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  }
  goto label1;
label2:;
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = fg174s_ti(t27, t28, t29);
  v5 = ((uint64_t)(((uint64_t)(t30))));
  t31 = ((uint64_t)(v5));
  t32 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  t34 = t33;
  if (t34) {
  t35 = ((uint64_t)(v1));
  t36 = ((uint64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t34 = ((t35) <= (t36));
  }
  t37 = t34;
  t38 = t37;
  if (t38) {
  t39 = ((uint64_t)(v5));
  t40 = gg_function_12(base + 0);
  t41 = ((uint64_t)(t40));
  t42 = ((uint64_t)(v1));
  t43 = ((uint64_t)(((uint64_t)((uint64_t)(t41) - (uint64_t)(t42)))));
  t38 = ((t39) <= (t43));
  }
  t44 = t38;
  t45 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_3(t44, t45, base + 0);
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(9))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = fg174s_ti(t46, t47, t48);
  v6 = ((int64_t)(t49));
  t50 = ((int64_t)(v6));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((t50) > (t51));
  t53 = t52;
  if (t53) {
  t54 = ((uint64_t)(v1));
  t55 = ((uint64_t)(((uint64_t)(v6))));
  t53 = ((t54) <= (t55));
  }
  t56 = t53;
  t57 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_3(t56, t57, base + 0);
  t0 = ((uint64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_23(int64_t p0, uint64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = gg_function_10(t0, base + 0);
  t2 = ((uint64_t)(t1));
  t3 = ((uint64_t)(v1));
  t4 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t5 = gg_function_11(t3, t4, base + 0);
  t6 = ((uint64_t)(t5));
  t7 = ((t2) >= (t6));
  t8 = ((int64_t)(v2));
  gg_function_3(t7, t8, base + 0);
  }
gg_done:;
}
static uint64_t gg_function_22(uint64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((uint64_t)(v0));
  }
  } else {
  {
  t0 = ((uint64_t)(v1));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_21(int64_t p0, int64_t p1, uint64_t p2, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint8_t t2;
  uint8_t t3;
  uint64_t t4;
  uint64_t t5;
  uint8_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  uint8_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  int64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((uint64_t)(v2));
  t1 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((uint64_t)(v2));
  t5 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t3 = ((t4) <= (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_3(t6, t7, base + 0);
  v3 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t8 = ((uint64_t)(v3));
  t9 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t10 = ((t8) < (t9));
  t11 = t10;
  if (t11) {
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t13 = ((uint64_t)(v3));
  t14 = ((uint64_t)(((uint64_t)((uint64_t)(t12) << ((uint64_t)(t13) & 63)))));
  t15 = ((uint64_t)(v2));
  t11 = ((t14) < (t15));
  }
  if (!(t11)) goto label2;
  {
  t16 = v3;
  t17 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v3 = ((uint64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  }
  goto label1;
label2:;
  t18 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t19 = ((int64_t)(((int64_t)UINT64_C(32))));
  t20 = ((uint64_t)(((uint64_t)((uint64_t)(t18) << ((uint64_t)(t19) & 63)))));
  t21 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t22 = ((uint64_t)(v3));
  t23 = ((uint64_t)(((uint64_t)((uint64_t)(t21) << ((uint64_t)(t22) & 63)))));
  t24 = ((uint64_t)(v2));
  t25 = ((uint64_t)(((uint64_t)((uint64_t)(t23) - (uint64_t)(t24)))));
  t26 = ((uint64_t)(((uint64_t)((uint64_t)(t20) * (uint64_t)(t25)))));
  t27 = ((uint64_t)(v2));
  t28 = ((uint64_t)(((uint64_t)gg_udiv(t26, t27))));
  t29 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v4 = ((uint64_t)(((uint64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t30 = ((uint64_t)(v4));
  t31 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t32 = ((t30) <= (t31));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_3(t32, t33, base + 0);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(4))));
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((uint64_t)(v4));
  fg174s_hput(t34, t35, t38, t39);
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(4))));
  t42 = ((int64_t)(v1));
  t43 = ((int64_t)(((int64_t)UINT64_C(3))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((uint64_t)(v3));
  fg174s_hput(t40, t41, t46, t47);
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(((int64_t)UINT64_C(3))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(((int64_t)UINT64_C(2))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((uint64_t)(v2));
  fg174s_hput(t48, t49, t54, t55);
  }
gg_done:;
}
static uint64_t gg_function_18(uint64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  int64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint8_t t13;
  uint64_t t14;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_function_3(t3, t4, base + 0);
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(v1));
  t7 = ((uint64_t)(((uint64_t)gg_udiv(t5, t6))));
  t9 = ((uint64_t)(v0));
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(((uint64_t)gg_umod(t9, t10))));
  t12 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) != (t12));
  if (t13) {
  {
  t8 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t8 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t14 = ((uint64_t)(t8));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t7) + (uint64_t)(t14)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_8(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t19;
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
  uint64_t t36;
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
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  int64_t t62;
  uint64_t t63;
  uint64_t t64;
  uint64_t t65;
  uint64_t t66;
  uint8_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint64_t t90;
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
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_3(t2, t3, base + 0);
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(2))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg174s_ti(t4, t5, t6);
  t8 = ((int64_t)(t7));
  t9 = fg174s_f32_type();
  t10 = ((int64_t)(t9));
  t11 = ((t8) == (t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_3(t11, t12, base + 0);
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg174s_ti(t13, t14, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) != (t18));
  t20 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_3(t19, t20, base + 0);
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(4))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg174s_ti(t21, t22, t23);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(((int64_t)UINT64_C(4))));
  t27 = ((t25) == (t26));
  t28 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_3(t27, t28, base + 0);
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg174s_ti(t32, t33, t34);
  t36 = ((uint64_t)(((uint64_t)(t35))));
  fg174s_hput(t29, t30, t31, t36);
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t37) < (t38)))) goto label2;
  {
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(((int64_t)UINT64_C(3))));
  t41 = ((int64_t)(v3));
  t42 = fg174s_ti(t39, t40, t41);
  v4 = ((int64_t)(t42));
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(((int64_t)UINT64_C(4))));
  t45 = ((int64_t)(v3));
  t46 = fg174s_ti(t43, t44, t45);
  v5 = ((int64_t)(t46));
  t47 = ((int64_t)(v4));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((t47) > (t48));
  t50 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_3(t49, t50, base + 0);
  t51 = ((int64_t)(v4));
  t52 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t53 = ((t51) <= (t52));
  t54 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_3(t53, t54, base + 0);
  t55 = ((int64_t)(v5));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t55) >= (t56));
  t58 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_3(t57, t58, base + 0);
  t59 = ((int64_t)(v5));
  t60 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t61 = ((t59) <= (t60));
  t62 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_3(t61, t62, base + 0);
  t63 = ((uint64_t)(((uint64_t)(v5))));
  t64 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t65 = ((uint64_t)(((uint64_t)gg_umod(t63, t64))));
  t66 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t67 = ((t65) == (t66));
  t68 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_3(t67, t68, base + 0);
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(((int64_t)UINT64_C(1))));
  t71 = ((int64_t)(v2));
  t72 = ((int64_t)(((int64_t)UINT64_C(4))));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) * (uint64_t)(t72)))));
  t74 = ((int64_t)(v3));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t74)))));
  t76 = ((uint64_t)(((uint64_t)(v4))));
  fg174s_hput(t69, t70, t75, t76);
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(2))));
  t79 = ((int64_t)(v2));
  t80 = ((int64_t)(((int64_t)UINT64_C(4))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  t82 = ((int64_t)(v3));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t81) + (uint64_t)(t82)))));
  t84 = ((uint64_t)(((uint64_t)(v5))));
  t85 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t86 = ((uint64_t)(((uint64_t)gg_udiv(t84, t85))));
  fg174s_hput(t77, t78, t83, t86);
  t87 = v3;
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t87) + (uint64_t)(t88)));
  }
  goto label1;
label2:;
  t89 = ((int64_t)(v1));
  t90 = gg_function_10(t89, base + 0);
  (void)(t90);
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
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
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
  uint8_t t49;
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
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint8_t t85;
  uint8_t t86;
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
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
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
  uint64_t t133;
  uint64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  uint64_t t138;
  uint64_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  uint64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  uint64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  uint64_t t153;
  uint64_t t154;
  uint64_t t155;
  uint8_t t156;
  int64_t t157;
  uint64_t t158;
  uint64_t t159;
  uint8_t t160;
  int64_t t161;
  uint64_t t162;
  uint64_t t163;
  uint64_t t164;
  uint64_t t165;
  uint8_t t166;
  int64_t t167;
  uint64_t t168;
  uint64_t t169;
  uint8_t t170;
  int64_t t171;
  uint64_t t172;
  uint64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  uint64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  uint64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  uint64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  uint64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  uint64_t t193;
  uint64_t t194;
  uint64_t t195;
  uint8_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  uint64_t t201;
  uint64_t t202;
  uint64_t t203;
  uint8_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  uint64_t t209;
  uint64_t t210;
  uint64_t t211;
  uint8_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  uint64_t t219;
  uint64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  uint64_t t226;
  uint64_t t227;
  uint8_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  uint64_t t235;
  uint64_t t236;
  uint64_t t237;
  uint64_t t238;
  uint64_t t239;
  uint8_t t240;
  uint8_t t241;
  uint8_t t242;
  int64_t t243;
  int64_t t244;
  uint8_t t245;
  uint8_t t246;
  int64_t t247;
  int64_t t248;
  uint8_t t249;
  uint8_t t250;
  int64_t t251;
  int64_t t252;
  uint8_t t253;
  uint8_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  uint64_t t259;
  uint64_t t260;
  uint64_t t261;
  uint8_t t262;
  uint8_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  uint64_t t267;
  uint64_t t268;
  uint64_t t269;
  uint8_t t270;
  uint8_t t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  uint64_t t275;
  uint64_t t276;
  uint64_t t277;
  uint8_t t278;
  uint8_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  uint64_t t283;
  uint64_t t284;
  uint64_t t285;
  uint8_t t286;
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
  uint64_t t297;
  uint64_t t298;
  uint64_t t299;
  uint8_t t300;
  uint8_t t301;
  int64_t t302;
  int64_t t303;
  int64_t t304;
  uint64_t t305;
  uint64_t t306;
  uint64_t t307;
  uint8_t t308;
  uint8_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  uint64_t t313;
  uint64_t t314;
  uint64_t t315;
  uint8_t t316;
  uint8_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  uint64_t t321;
  uint64_t t322;
  uint64_t t323;
  uint8_t t324;
  int64_t t325;
  uint64_t t326;
  uint64_t t327;
  uint8_t t328;
  uint8_t t329;
  uint64_t t330;
  uint64_t t331;
  uint8_t t332;
  uint8_t t333;
  uint64_t t334;
  uint64_t t335;
  uint8_t t336;
  uint8_t t337;
  uint64_t t338;
  uint64_t t339;
  uint8_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  uint8_t t345;
  uint8_t t346;
  int64_t t347;
  int64_t t348;
  uint8_t t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  uint8_t t354;
  int64_t t355;
  uint64_t t356;
  uint64_t t357;
  uint8_t t358;
  uint64_t t359;
  uint64_t t360;
  uint64_t t361;
  uint64_t t362;
  uint64_t t363;
  uint64_t t364;
  uint64_t t365;
  uint64_t t366;
  uint64_t t367;
  uint64_t t368;
  uint64_t t369;
  uint64_t t370;
  uint8_t t371;
  uint64_t t372;
  uint64_t t373;
  uint64_t t374;
  uint64_t t375;
  uint64_t t376;
  uint64_t t377;
  uint64_t t378;
  uint64_t t379;
  uint8_t t380;
  int64_t t381;
  uint64_t t382;
  uint64_t t383;
  uint8_t t384;
  uint64_t t385;
  uint64_t t386;
  uint64_t t387;
  uint64_t t388;
  uint8_t t389;
  uint8_t t390;
  uint64_t t391;
  uint64_t t392;
  uint64_t t393;
  uint64_t t394;
  uint8_t t395;
  int64_t t396;
  uint64_t t397;
  uint64_t t398;
  uint64_t t399;
  uint64_t t400;
  uint8_t t401;
  int64_t t402;
  uint64_t t403;
  uint64_t t404;
  uint8_t t405;
  int64_t t406;
  int64_t t407;
  int64_t t408;
  float t409;
  float t410;
  float t411;
  float t412;
  float t413;
  int64_t t414;
  int64_t t415;
  uint64_t t416;
  int64_t t417;
  int64_t t418;
  uint64_t t419;
  uint64_t t420;
  uint64_t t421;
  uint64_t t422;
  uint64_t t423;
  uint64_t t424;
  uint64_t t425;
  uint64_t t426;
  uint64_t t427;
  uint64_t t428;
  uint64_t t429;
  uint64_t t430;
  uint64_t t431;
  int64_t t432;
  int64_t t433;
  uint8_t t434;
  int64_t t435;
  int64_t t436;
  uint8_t t437;
  int64_t t438;
  uint8_t t439;
  uint64_t t440;
  uint64_t t441;
  uint8_t t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  uint8_t t446;
  uint8_t t447;
  uint64_t t448;
  uint64_t t449;
  int64_t t450;
  int64_t t451;
  uint8_t t452;
  int64_t t453;
  int64_t t454;
  int64_t t455;
  int64_t t456;
  uint64_t t457;
  uint64_t t458;
  uint64_t t459;
  uint64_t t460;
  uint64_t t461;
  uint64_t t462;
  uint64_t t463;
  uint64_t t464;
  uint64_t t465;
  uint64_t t466;
  uint64_t t467;
  uint64_t t468;
  uint64_t t469;
  uint64_t t470;
  uint64_t t471;
  uint64_t t472;
  uint64_t t473;
  uint8_t t474;
  int64_t t475;
  int64_t t476;
  uint64_t t477;
  uint64_t t478;
  uint64_t t479;
  uint64_t t480;
  uint64_t t481;
  uint64_t t482;
  uint8_t t483;
  int64_t t484;
  int64_t t485;
  int64_t t486;
  uint8_t t487;
  int64_t t488;
  uint64_t t489;
  uint64_t t490;
  uint64_t t491;
  uint64_t t492;
  uint64_t t493;
  uint64_t t494;
  uint64_t t495;
  uint64_t t496;
  uint64_t t497;
  uint64_t t498;
  uint8_t t499;
  int64_t t500;
  uint64_t t501;
  uint64_t t502;
  uint8_t t503;
  uint64_t t504;
  uint64_t t505;
  uint64_t t506;
  uint64_t t507;
  uint64_t t508;
  uint64_t t509;
  uint64_t t510;
  uint64_t t511;
  uint64_t t512;
  int64_t t513;
  int64_t t514;
  int64_t t515;
  uint64_t t516;
  uint64_t t517;
  uint64_t t518;
  uint64_t t519;
  uint64_t t520;
  uint64_t t521;
  int64_t t522;
  int64_t t523;
  int64_t t524;
  uint64_t t525;
  uint64_t t526;
  uint64_t t527;
  uint64_t t528;
  uint64_t t529;
  int64_t t530;
  int64_t t531;
  int64_t t532;
  uint64_t t533;
  uint64_t t534;
  uint64_t t535;
  uint64_t t536;
  uint64_t t537;
  int64_t t538;
  uint64_t t539;
  int64_t t540;
  int64_t t541;
  uint64_t t542;
  int64_t t543;
  uint64_t t544;
  uint64_t t545;
  uint64_t t546;
  int64_t t547;
  int64_t t548;
  int64_t t549;
  uint64_t t550;
  uint64_t t551;
  uint64_t t552;
  uint64_t t553;
  uint64_t t554;
  uint64_t t555;
  int64_t t556;
  int64_t t557;
  int64_t t558;
  uint64_t t559;
  uint64_t t560;
  uint64_t t561;
  uint64_t t562;
  uint64_t t563;
  uint64_t t564;
  uint64_t t565;
  int64_t t566;
  int64_t t567;
  int64_t t568;
  uint64_t t569;
  uint64_t t570;
  uint64_t t571;
  uint64_t t572;
  uint64_t t573;
  int64_t t574;
  uint64_t t575;
  int64_t t576;
  uint64_t t577;
  uint64_t t578;
  uint64_t t579;
  int64_t t580;
  int64_t t581;
  int64_t t582;
  uint64_t t583;
  uint64_t t584;
  uint64_t t585;
  uint64_t t586;
  uint64_t t587;
  uint64_t t588;
  int64_t t589;
  int64_t t590;
  int64_t t591;
  uint64_t t592;
  uint64_t t593;
  uint64_t t594;
  uint64_t t595;
  uint64_t t596;
  uint64_t t597;
  uint64_t t598;
  int64_t t599;
  int64_t t600;
  int64_t t601;
  uint64_t t602;
  uint64_t t603;
  uint64_t t604;
  uint64_t t605;
  uint64_t t606;
  int64_t t607;
  uint64_t t608;
  int64_t t609;
  uint8_t t610;
  uint64_t t611;
  uint64_t t612;
  int64_t t613;
  uint64_t t614;
  int64_t t615;
  int64_t t616;
  uint64_t t617;
  uint64_t t618;
  uint64_t t619;
  uint64_t t620;
  uint64_t t621;
  uint64_t t622;
  uint64_t t623;
  int64_t t624;
  int64_t t625;
  int64_t t626;
  int64_t t627;
  int64_t t628;
  uint64_t t629;
  uint64_t t630;
  uint64_t t631;
  uint64_t t632;
  uint64_t t633;
  uint64_t t634;
  uint8_t t635;
  uint8_t t636;
  uint8_t t637;
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
  int64_t t648;
  int64_t t649;
  uint64_t t650;
  uint64_t t651;
  uint64_t t652;
  uint64_t t653;
  uint8_t t654;
  uint8_t t655;
  uint8_t t656;
  int64_t t657;
  int64_t t658;
  int64_t t659;
  uint8_t t660;
  uint8_t t661;
  uint64_t t662;
  uint64_t t663;
  uint8_t t664;
  uint64_t t665;
  uint64_t t666;
  uint64_t t667;
  uint64_t t668;
  uint64_t t669;
  uint8_t t670;
  int64_t t671;
  uint64_t t672;
  uint64_t t673;
  uint64_t t674;
  uint64_t t675;
  uint8_t t676;
  uint8_t t677;
  uint8_t t678;
  int64_t t679;
  int64_t t680;
  int64_t t681;
  uint64_t t682;
  uint64_t t683;
  uint8_t t684;
  uint8_t t685;
  int64_t t686;
  int64_t t687;
  uint8_t t688;
  uint8_t t689;
  uint64_t t690;
  uint64_t t691;
  uint8_t t692;
  uint8_t t693;
  int64_t t694;
  int64_t t695;
  int64_t t696;
  uint64_t t697;
  uint64_t t698;
  uint64_t t699;
  int64_t t700;
  int64_t t701;
  int64_t t702;
  uint64_t t703;
  int64_t t704;
  int64_t t705;
  int64_t t706;
  uint64_t t707;
  int64_t t708;
  int64_t t709;
  int64_t t710;
  uint64_t t711;
  int64_t t712;
  int64_t t713;
  int64_t t714;
  uint64_t t715;
  int64_t t716;
  int64_t t717;
  int64_t t718;
  uint64_t t719;
  int64_t t720;
  int64_t t721;
  int64_t t722;
  uint64_t t723;
  int64_t t724;
  int64_t t725;
  int64_t t726;
  uint64_t t727;
  int64_t t728;
  int64_t t729;
  int64_t t730;
  uint64_t t731;
  uint8_t t732;
  uint64_t t733;
  int64_t t734;
  int64_t t735;
  int64_t t736;
  uint64_t t737;
  int64_t t738;
  int64_t t739;
  int64_t t740;
  uint64_t t741;
  int64_t t742;
  int64_t t743;
  int64_t t744;
  uint64_t t745;
  int64_t t746;
  int64_t t747;
  int64_t t748;
  uint64_t t749;
  int64_t t750;
  int64_t t751;
  int64_t t752;
  uint64_t t753;
  int64_t t754;
  int64_t t755;
  int64_t t756;
  uint64_t t757;
  int64_t t758;
  int64_t t759;
  int64_t t760;
  uint64_t t761;
  int64_t t762;
  int64_t t763;
  int64_t t764;
  uint64_t t765;
  int64_t t766;
  int64_t t767;
  int64_t t768;
  uint64_t t769;
  uint8_t t770;
  uint64_t t771;
  int64_t t772;
  int64_t t773;
  int64_t t774;
  uint64_t t775;
  uint8_t t776;
  uint64_t t777;
  int64_t t778;
  int64_t t779;
  int64_t t780;
  uint64_t t781;
  int64_t t782;
  int64_t t783;
  int64_t t784;
  uint64_t t785;
  uint8_t t786;
  int64_t t787;
  uint8_t t788;
  uint8_t t789;
  int64_t t790;
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
  uint64_t v12;
  uint64_t v13;
  uint64_t v14;
  uint64_t v15;
  uint64_t v16;
  uint64_t v17;
  uint64_t v18;
  uint64_t v19;
  uint64_t v20;
  int64_t v21;
  uint64_t v22;
  uint8_t v23;
  int64_t v24;
  uint8_t v25;
  int64_t v26;
  int64_t v27;
  uint64_t v28;
  uint8_t v29;
  uint64_t v30;
  uint64_t v31;
  uint64_t v32;
  uint64_t v33;
  uint64_t v34;
  uint64_t v35;
  uint64_t v36;
  uint64_t v37;
  uint64_t v38;
  uint64_t v39;
  uint64_t v40;
  uint64_t v41;
  uint64_t v42;
  uint64_t v43;
  uint64_t v44;
  uint64_t v45;
  uint64_t v46;
  uint64_t v47;
  uint64_t v48;
  uint64_t v49;
  int64_t v50;
  int64_t v51;
  uint64_t v52;
  uint64_t v53;
  uint8_t v54;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg174s_arg(t0, t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  t5 = fg174s_arg(t3, t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(3))));
  t8 = fg174s_arg(t6, t7);
  v3 = ((int64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  t11 = fg174s_arg(t9, t10);
  v4 = ((int64_t)(t11));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = fg174s_arg(t12, t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) != (t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_3(t17, t18, base + 0);
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) == (t20));
  t22 = t21;
  if (!t22) {
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = ((t23) == (t24));
  }
  t25 = t22;
  t26 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_3(t25, t26, base + 0);
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t27) != (t28));
  t30 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_3(t29, t30, base + 0);
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t34 = ((int64_t)(v3));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) == (t35));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_3(t36, t37, base + 0);
  }
  } else {
  {
  t38 = ((int64_t)(v3));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) != (t39));
  t41 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_3(t40, t41, base + 0);
  }
  }
  t42 = ((int64_t)(v2));
  t43 = ((int64_t)(((int64_t)UINT64_C(2))));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = fg174s_ti(t42, t43, t44);
  t46 = ((int64_t)(t45));
  t47 = fg174s_f32_type();
  t48 = ((int64_t)(t47));
  t49 = ((t46) == (t48));
  t50 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_3(t49, t50, base + 0);
  t51 = ((int64_t)(v2));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = fg174s_ti(t51, t52, t53);
  t55 = ((int64_t)(t54));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t55) != (t56));
  t58 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_3(t57, t58, base + 0);
  t59 = ((int64_t)(v2));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = fg174s_ti(t59, t60, t61);
  v5 = ((int64_t)(t62));
  t63 = ((int64_t)(v2));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((int64_t)(((int64_t)UINT64_C(1))));
  t66 = fg174s_ti(t63, t64, t65);
  v6 = ((int64_t)(t66));
  t67 = ((int64_t)(v2));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((int64_t)(((int64_t)UINT64_C(2))));
  t70 = fg174s_ti(t67, t68, t69);
  v7 = ((int64_t)(t70));
  t71 = ((int64_t)(v2));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((int64_t)(((int64_t)UINT64_C(3))));
  t74 = fg174s_ti(t71, t72, t73);
  v8 = ((int64_t)(t74));
  t75 = ((int64_t)(v2));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = ((int64_t)(((int64_t)UINT64_C(4))));
  t78 = fg174s_ti(t75, t76, t77);
  v9 = ((int64_t)(t78));
  t79 = ((int64_t)(v2));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = ((int64_t)(((int64_t)UINT64_C(5))));
  t82 = fg174s_ti(t79, t80, t81);
  v10 = ((int64_t)(t82));
  t83 = ((int64_t)(v5));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((t83) != (t84));
  t86 = t85;
  if (t86) {
  t87 = ((int64_t)(v6));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = ((t87) != (t88));
  }
  t89 = t86;
  t90 = t89;
  if (t90) {
  t91 = ((int64_t)(v7));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((t91) != (t92));
  }
  t93 = t90;
  t94 = t93;
  if (t94) {
  t95 = ((int64_t)(v8));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((t95) != (t96));
  }
  t97 = t94;
  t98 = t97;
  if (t98) {
  t99 = ((int64_t)(v9));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((t99) != (t100));
  }
  t101 = t98;
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)(v10));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) != (t104));
  }
  t105 = t102;
  t106 = ((int64_t)(((int64_t)UINT64_C(7))));
  gg_function_3(t105, t106, base + 0);
  t107 = ((int64_t)(v0));
  t108 = fg174s_plan_ptr(t107);
  v11 = ((int64_t)(t108));
  t109 = ((int64_t)(v11));
  t110 = ((int64_t)(v5));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_8(t109, t110, t111, base + 0);
  t112 = ((int64_t)(v11));
  t113 = ((int64_t)(v6));
  t114 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_8(t112, t113, t114, base + 0);
  t115 = ((int64_t)(v11));
  t116 = ((int64_t)(v7));
  t117 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_function_8(t115, t116, t117, base + 0);
  t118 = ((int64_t)(v11));
  t119 = ((int64_t)(v8));
  t120 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_8(t118, t119, t120, base + 0);
  t121 = ((int64_t)(v11));
  t122 = ((int64_t)(v9));
  t123 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_8(t121, t122, t123, base + 0);
  t124 = ((int64_t)(v11));
  t125 = ((int64_t)(v10));
  t126 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_8(t124, t125, t126, base + 0);
  t127 = ((int64_t)(v11));
  t128 = ((int64_t)(v2));
  t129 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_8(t127, t128, t129, base + 0);
  t130 = ((int64_t)(v11));
  t131 = ((int64_t)(((int64_t)UINT64_C(1))));
  t132 = ((int64_t)(((int64_t)UINT64_C(1))));
  t133 = fg174s_hget(t130, t131, t132);
  t134 = ((uint64_t)(t133));
  t135 = ((int64_t)(v11));
  t136 = ((int64_t)(((int64_t)UINT64_C(1))));
  t137 = ((int64_t)(((int64_t)UINT64_C(5))));
  t138 = fg174s_hget(t135, t136, t137);
  t139 = ((uint64_t)(t138));
  t140 = ((t134) == (t139));
  t141 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_function_3(t140, t141, base + 0);
  t142 = ((int64_t)(v11));
  t143 = ((int64_t)(((int64_t)UINT64_C(1))));
  t144 = ((int64_t)(((int64_t)UINT64_C(3))));
  t145 = fg174s_hget(t142, t143, t144);
  v12 = ((uint64_t)(t145));
  t146 = ((int64_t)(v11));
  t147 = ((int64_t)(((int64_t)UINT64_C(1))));
  t148 = ((int64_t)(((int64_t)UINT64_C(7))));
  t149 = fg174s_hget(t146, t147, t148);
  v13 = ((uint64_t)(t149));
  t150 = ((int64_t)(v11));
  t151 = ((int64_t)(((int64_t)UINT64_C(1))));
  t152 = ((int64_t)(((int64_t)UINT64_C(11))));
  t153 = fg174s_hget(t150, t151, t152);
  v14 = ((uint64_t)(t153));
  t154 = ((uint64_t)(v12));
  t155 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t156 = ((t154) > (t155));
  t157 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_function_3(t156, t157, base + 0);
  t158 = ((uint64_t)(v14));
  t159 = ((uint64_t)(v12));
  t160 = ((t158) >= (t159));
  t161 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_function_3(t160, t161, base + 0);
  t162 = ((uint64_t)(v14));
  t163 = ((uint64_t)(v12));
  t164 = ((uint64_t)(((uint64_t)gg_umod(t162, t163))));
  t165 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t166 = ((t164) == (t165));
  t167 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_function_3(t166, t167, base + 0);
  t168 = ((uint64_t)(v13));
  t169 = ((uint64_t)(v12));
  t170 = ((t168) == (t169));
  t171 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_function_3(t170, t171, base + 0);
  t172 = ((uint64_t)(v14));
  t173 = ((uint64_t)(v12));
  v15 = ((uint64_t)(((uint64_t)gg_udiv(t172, t173))));
  t174 = ((int64_t)(v11));
  t175 = ((int64_t)(((int64_t)UINT64_C(1))));
  t176 = ((int64_t)(((int64_t)UINT64_C(1))));
  t177 = fg174s_hget(t174, t175, t176);
  v16 = ((uint64_t)(t177));
  t178 = ((int64_t)(v11));
  t179 = ((int64_t)(((int64_t)UINT64_C(1))));
  t180 = ((int64_t)(((int64_t)UINT64_C(8))));
  t181 = fg174s_hget(t178, t179, t180);
  v17 = ((uint64_t)(t181));
  t182 = ((int64_t)(v11));
  t183 = ((int64_t)(((int64_t)UINT64_C(1))));
  t184 = ((int64_t)(((int64_t)UINT64_C(9))));
  t185 = fg174s_hget(t182, t183, t184);
  v18 = ((uint64_t)(t185));
  t186 = ((int64_t)(v11));
  t187 = ((int64_t)(((int64_t)UINT64_C(1))));
  t188 = ((int64_t)(((int64_t)UINT64_C(10))));
  t189 = fg174s_hget(t186, t187, t188);
  v19 = ((uint64_t)(t189));
  v20 = ((uint64_t)(v14));
  t190 = ((int64_t)(v11));
  t191 = ((int64_t)(((int64_t)UINT64_C(2))));
  t192 = ((int64_t)(((int64_t)UINT64_C(0))));
  t193 = fg174s_hget(t190, t191, t192);
  t194 = ((uint64_t)(t193));
  t195 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t196 = ((t194) == (t195));
  t197 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_3(t196, t197, base + 0);
  t198 = ((int64_t)(v11));
  t199 = ((int64_t)(((int64_t)UINT64_C(2))));
  t200 = ((int64_t)(((int64_t)UINT64_C(4))));
  t201 = fg174s_hget(t198, t199, t200);
  t202 = ((uint64_t)(t201));
  t203 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t204 = ((t202) == (t203));
  t205 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_3(t204, t205, base + 0);
  t206 = ((int64_t)(v11));
  t207 = ((int64_t)(((int64_t)UINT64_C(2))));
  t208 = ((int64_t)(((int64_t)UINT64_C(8))));
  t209 = fg174s_hget(t206, t207, t208);
  t210 = ((uint64_t)(t209));
  t211 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t212 = ((t210) == (t211));
  t213 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_3(t212, t213, base + 0);
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t214 = ((int64_t)(v21));
  t215 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t214) < (t215)))) goto label2;
  {
  t216 = ((int64_t)(v11));
  t217 = ((int64_t)(((int64_t)UINT64_C(2))));
  t218 = ((int64_t)(v21));
  t219 = fg174s_hget(t216, t217, t218);
  t220 = ((uint64_t)(t219));
  t221 = ((int64_t)(v11));
  t222 = ((int64_t)(((int64_t)UINT64_C(2))));
  t223 = ((int64_t)(((int64_t)UINT64_C(4))));
  t224 = ((int64_t)(v21));
  t225 = ((int64_t)(((int64_t)((uint64_t)(t223) + (uint64_t)(t224)))));
  t226 = fg174s_hget(t221, t222, t225);
  t227 = ((uint64_t)(t226));
  t228 = ((t220) == (t227));
  t229 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_3(t228, t229, base + 0);
  t230 = v21;
  t231 = ((int64_t)(((int64_t)UINT64_C(1))));
  v21 = ((int64_t)((uint64_t)(t230) + (uint64_t)(t231)));
  }
  goto label1;
label2:;
  t232 = ((int64_t)(v11));
  t233 = ((int64_t)(((int64_t)UINT64_C(1))));
  t234 = ((int64_t)(((int64_t)UINT64_C(12))));
  t235 = fg174s_hget(t232, t233, t234);
  v22 = ((uint64_t)(t235));
  t236 = ((uint64_t)(v22));
  t237 = ((uint64_t)(v17));
  v23 = ((t236) == (t237));
  t238 = ((uint64_t)(v22));
  t239 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t240 = ((t238) == (t239));
  t241 = t240;
  if (!t241) {
  t241 = v23;
  }
  t242 = t241;
  t243 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_function_3(t242, t243, base + 0);
  t244 = ((int64_t)(v8));
  t245 = fg174s_contiguous(t244);
  t246 = t245;
  t247 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_3(t246, t247, base + 0);
  t248 = ((int64_t)(v9));
  t249 = fg174s_contiguous(t248);
  t250 = t249;
  t251 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_3(t250, t251, base + 0);
  t252 = ((int64_t)(v10));
  t253 = fg174s_contiguous(t252);
  t254 = t253;
  t255 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_3(t254, t255, base + 0);
  t256 = ((int64_t)(v11));
  t257 = ((int64_t)(((int64_t)UINT64_C(1))));
  t258 = ((int64_t)(((int64_t)UINT64_C(20))));
  t259 = fg174s_hget(t256, t257, t258);
  t260 = ((uint64_t)(t259));
  t261 = ((uint64_t)(v17));
  t262 = ((t260) == (t261));
  t263 = t262;
  if (t263) {
  t264 = ((int64_t)(v11));
  t265 = ((int64_t)(((int64_t)UINT64_C(1))));
  t266 = ((int64_t)(((int64_t)UINT64_C(21))));
  t267 = fg174s_hget(t264, t265, t266);
  t268 = ((uint64_t)(t267));
  t269 = ((uint64_t)(v17));
  t263 = ((t268) == (t269));
  }
  t270 = t263;
  t271 = t270;
  if (t271) {
  t272 = ((int64_t)(v11));
  t273 = ((int64_t)(((int64_t)UINT64_C(1))));
  t274 = ((int64_t)(((int64_t)UINT64_C(22))));
  t275 = fg174s_hget(t272, t273, t274);
  t276 = ((uint64_t)(t275));
  t277 = ((uint64_t)(v18));
  t271 = ((t276) == (t277));
  }
  t278 = t271;
  t279 = t278;
  if (t279) {
  t280 = ((int64_t)(v11));
  t281 = ((int64_t)(((int64_t)UINT64_C(1))));
  t282 = ((int64_t)(((int64_t)UINT64_C(23))));
  t283 = fg174s_hget(t280, t281, t282);
  t284 = ((uint64_t)(t283));
  t285 = ((uint64_t)(v20));
  t279 = ((t284) == (t285));
  }
  t286 = t279;
  t287 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_function_3(t286, t287, base + 0);
  t288 = ((int64_t)(v2));
  t289 = ((int64_t)(((int64_t)UINT64_C(8))));
  t290 = ((int64_t)(((int64_t)UINT64_C(0))));
  t291 = fg174s_ti(t288, t289, t290);
  v24 = ((int64_t)(t291));
  t292 = ((int64_t)(v24));
  t293 = ((int64_t)(((int64_t)UINT64_C(1))));
  v25 = ((t292) > (t293));
  t294 = ((int64_t)(v11));
  t295 = ((int64_t)(((int64_t)UINT64_C(1))));
  t296 = ((int64_t)(((int64_t)UINT64_C(0))));
  t297 = fg174s_hget(t294, t295, t296);
  t298 = ((uint64_t)(t297));
  t299 = ((uint64_t)(v17));
  t300 = ((t298) == (t299));
  t301 = t300;
  if (t301) {
  t302 = ((int64_t)(v11));
  t303 = ((int64_t)(((int64_t)UINT64_C(1))));
  t304 = ((int64_t)(((int64_t)UINT64_C(4))));
  t305 = fg174s_hget(t302, t303, t304);
  t306 = ((uint64_t)(t305));
  t307 = ((uint64_t)(v17));
  t301 = ((t306) == (t307));
  }
  t308 = t301;
  t309 = t308;
  if (t309) {
  t310 = ((int64_t)(v11));
  t311 = ((int64_t)(((int64_t)UINT64_C(1))));
  t312 = ((int64_t)(((int64_t)UINT64_C(2))));
  t313 = fg174s_hget(t310, t311, t312);
  t314 = ((uint64_t)(t313));
  t315 = ((uint64_t)(v19));
  t309 = ((t314) >= (t315));
  }
  t316 = t309;
  t317 = t316;
  if (t317) {
  t318 = ((int64_t)(v11));
  t319 = ((int64_t)(((int64_t)UINT64_C(1))));
  t320 = ((int64_t)(((int64_t)UINT64_C(6))));
  t321 = fg174s_hget(t318, t319, t320);
  t322 = ((uint64_t)(t321));
  t323 = ((uint64_t)(v19));
  t317 = ((t322) >= (t323));
  }
  t324 = t317;
  t325 = ((int64_t)(((int64_t)UINT64_C(22))));
  gg_function_3(t324, t325, base + 0);
  t326 = ((uint64_t)(v17));
  t327 = ((uint64_t)(((int64_t)UINT64_C(16))));
  t328 = ((t326) == (t327));
  t329 = t328;
  if (!t329) {
  t330 = ((uint64_t)(v17));
  t331 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t329 = ((t330) == (t331));
  }
  t332 = t329;
  t333 = t332;
  if (!t333) {
  t334 = ((uint64_t)(v17));
  t335 = ((uint64_t)(((int64_t)UINT64_C(64))));
  t333 = ((t334) == (t335));
  }
  t336 = t333;
  t337 = t336;
  if (!t337) {
  t338 = ((uint64_t)(v17));
  t339 = ((uint64_t)(((int64_t)UINT64_C(128))));
  t337 = ((t338) == (t339));
  }
  t340 = t337;
  t341 = ((int64_t)(((int64_t)UINT64_C(20))));
  gg_function_3(t340, t341, base + 0);
  t342 = fg174s_warp_size();
  v26 = ((int64_t)(t342));
  t343 = ((int64_t)(v26));
  t344 = ((int64_t)(((int64_t)UINT64_C(32))));
  t345 = ((t343) == (t344));
  t346 = t345;
  if (!t346) {
  t347 = ((int64_t)(v26));
  t348 = ((int64_t)(((int64_t)UINT64_C(64))));
  t346 = ((t347) == (t348));
  }
  t349 = t346;
  t350 = ((int64_t)(((int64_t)UINT64_C(21))));
  gg_function_3(t349, t350, base + 0);
  t351 = fg174s_nsm();
  v27 = ((int64_t)(t351));
  t352 = ((int64_t)(v27));
  t353 = ((int64_t)(((int64_t)UINT64_C(0))));
  t354 = ((t352) > (t353));
  t355 = ((int64_t)(((int64_t)UINT64_C(21))));
  gg_function_3(t354, t355, base + 0);
  v28 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v29 = ((uint8_t)UINT64_C(1));
label4:;
  if (!(v29)) goto label5;
  {
  t356 = ((uint64_t)(v28));
  t357 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t358 = ((t356) > (t357));
  if (t358) {
  {
  t359 = ((uint64_t)(v17));
  t360 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t361 = ((uint64_t)(v28));
  t362 = gg_function_11(t360, t361, base + 0);
  t363 = ((uint64_t)(t362));
  v30 = ((uint64_t)(((uint64_t)gg_udiv(t359, t363))));
  t364 = ((uint64_t)(v18));
  t365 = ((uint64_t)(v20));
  t366 = gg_function_11(t364, t365, base + 0);
  t367 = ((uint64_t)(v30));
  t368 = gg_function_11(t366, t367, base + 0);
  t369 = ((uint64_t)(t368));
  t370 = ((uint64_t)(((uint64_t)(v27))));
  t371 = ((t369) < (t370));
  if (t371) {
  {
  t372 = ((uint64_t)(v28));
  t373 = ((uint64_t)(((int64_t)UINT64_C(2))));
  v28 = ((uint64_t)(((uint64_t)gg_udiv(t372, t373))));
  }
  } else {
  {
  v29 = ((uint8_t)UINT64_C(0));
  }
  }
  }
  } else {
  {
  v29 = ((uint8_t)UINT64_C(0));
  }
  }
  }
  goto label4;
label5:;
  t374 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t375 = ((uint64_t)(v28));
  v31 = gg_function_11(t374, t375, base + 0);
  t376 = ((uint64_t)(v17));
  t377 = ((uint64_t)(v31));
  t378 = ((uint64_t)(((uint64_t)gg_umod(t376, t377))));
  t379 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t380 = ((t378) == (t379));
  t381 = ((int64_t)(((int64_t)UINT64_C(15))));
  gg_function_3(t380, t381, base + 0);
  t382 = ((uint64_t)(((uint64_t)(v26))));
  t383 = ((uint64_t)(v17));
  t384 = ((t382) < (t383));
  if (t384) {
  {
  v32 = ((uint64_t)(((uint64_t)(v26))));
  }
  } else {
  {
  v32 = ((uint64_t)(v17));
  }
  }
  t385 = ((uint64_t)(v32));
  t386 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  v33 = ((uint64_t)(((uint64_t)gg_udiv(t385, t386))));
  t387 = ((uint64_t)(v33));
  t388 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t389 = ((t387) > (t388));
  t390 = t389;
  if (t390) {
  t391 = ((uint64_t)(v17));
  t392 = ((uint64_t)(v33));
  t393 = ((uint64_t)(((uint64_t)gg_umod(t391, t392))));
  t394 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t390 = ((t393) == (t394));
  }
  t395 = t390;
  t396 = ((int64_t)(((int64_t)UINT64_C(15))));
  gg_function_3(t395, t396, base + 0);
  t397 = ((uint64_t)(v17));
  t398 = ((uint64_t)(v31));
  v34 = gg_function_18(t397, t398, base + 0);
  t399 = ((uint64_t)(v20));
  t400 = ((uint64_t)(((int64_t)UINT64_C(65535))));
  t401 = ((t399) <= (t400));
  t402 = ((int64_t)(((int64_t)UINT64_C(16))));
  gg_function_3(t401, t402, base + 0);
  t403 = ((uint64_t)(v34));
  t404 = ((uint64_t)(((int64_t)UINT64_C(65535))));
  t405 = ((t403) <= (t404));
  t406 = ((int64_t)(((int64_t)UINT64_C(16))));
  gg_function_3(t405, t406, base + 0);
  t407 = ((int64_t)(v11));
  t408 = ((int64_t)(((int64_t)UINT64_C(0))));
  t409 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t410 = ((float)(((float)(v17))));
  t411 = fg174s_sqrt(t410);
  t412 = ((float)(t411));
  t413 = ((float)(((t409) / (t412))));
  fg174s_hfloat(t407, t408, t413);
  t414 = ((int64_t)(v11));
  t415 = ((int64_t)(((int64_t)UINT64_C(0))));
  t416 = ((uint64_t)(v16));
  gg_function_21(t414, t415, t416, base + 0);
  t417 = ((int64_t)(v11));
  t418 = ((int64_t)(((int64_t)UINT64_C(1))));
  t419 = ((uint64_t)(v15));
  gg_function_21(t417, t418, t419, base + 0);
  t420 = ((uint64_t)(v17));
  t421 = ((uint64_t)(v18));
  t422 = gg_function_11(t420, t421, base + 0);
  t423 = ((uint64_t)(v19));
  t424 = gg_function_11(t422, t423, base + 0);
  t425 = ((uint64_t)(v20));
  v35 = gg_function_11(t424, t425, base + 0);
  t426 = ((uint64_t)(v17));
  t427 = ((uint64_t)(v17));
  t428 = gg_function_11(t426, t427, base + 0);
  t429 = ((uint64_t)(v18));
  t430 = gg_function_11(t428, t429, base + 0);
  t431 = ((uint64_t)(v20));
  v36 = gg_function_11(t430, t431, base + 0);
  t432 = ((int64_t)(v1));
  t433 = ((int64_t)(((int64_t)UINT64_C(1))));
  t434 = ((t432) == (t433));
  if (t434) {
  {
  t435 = ((int64_t)(v4));
  t436 = ((int64_t)(((int64_t)UINT64_C(0))));
  t437 = ((t435) >= (t436));
  t438 = ((int64_t)(((int64_t)UINT64_C(23))));
  gg_function_3(t437, t438, base + 0);
  t439 = v25;
  if (t439) {
  {
  t440 = ((uint64_t)(((uint64_t)(v4))));
  t441 = ((uint64_t)(v36));
  t442 = ((t440) >= (t441));
  t443 = ((int64_t)(((int64_t)UINT64_C(23))));
  gg_function_3(t442, t443, base + 0);
  }
  }
  }
  }
  t444 = ((int64_t)(v1));
  t445 = ((int64_t)(((int64_t)UINT64_C(1))));
  t446 = ((t444) == (t445));
  if (t446) {
  {
  v37 = ((uint64_t)(((uint64_t)(v4))));
  }
  } else {
  {
  v37 = ((uint64_t)(v36));
  }
  }
  t447 = v25;
  if (t447) {
  {
  t448 = ((uint64_t)(((uint64_t)(v24))));
  t449 = ((uint64_t)(v19));
  v38 = gg_function_22(t448, t449, base + 0);
  }
  } else {
  {
  v38 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t450 = ((int64_t)(v1));
  t451 = ((int64_t)(((int64_t)UINT64_C(1))));
  t452 = ((t450) == (t451));
  if (t452) {
  {
  v39 = ((uint64_t)(((uint64_t)(v3))));
  }
  } else {
  {
  t453 = ((int64_t)(v2));
  t454 = ((int64_t)(((int64_t)UINT64_C(1))));
  t455 = ((int64_t)(((int64_t)UINT64_C(0))));
  t456 = fg174s_ti(t453, t454, t455);
  t457 = ((uint64_t)(((uint64_t)(t456))));
  t458 = ((uint64_t)(v35));
  t459 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t460 = gg_function_11(t458, t459, base + 0);
  v39 = gg_function_13(t457, t460, base + 0);
  }
  }
  t461 = ((uint64_t)(v38));
  t462 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t463 = ((uint64_t)(((uint64_t)((uint64_t)(t461) - (uint64_t)(t462)))));
  t464 = ((uint64_t)(v37));
  t465 = gg_function_11(t463, t464, base + 0);
  t466 = ((uint64_t)(v36));
  t467 = gg_function_13(t465, t466, base + 0);
  t468 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v40 = gg_function_11(t467, t468, base + 0);
  t469 = ((uint64_t)(v39));
  t470 = gg_function_12(base + 0);
  t471 = ((uint64_t)(t470));
  t472 = ((uint64_t)(v40));
  t473 = ((uint64_t)(((uint64_t)((uint64_t)(t471) - (uint64_t)(t472)))));
  t474 = ((t469) <= (t473));
  t475 = ((int64_t)(((int64_t)UINT64_C(24))));
  gg_function_3(t474, t475, base + 0);
  t476 = ((int64_t)(v2));
  t477 = gg_function_10(t476, base + 0);
  t478 = ((uint64_t)(t477));
  t479 = ((uint64_t)(v35));
  t480 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t481 = gg_function_11(t479, t480, base + 0);
  t482 = ((uint64_t)(t481));
  t483 = ((t478) >= (t482));
  t484 = ((int64_t)(((int64_t)UINT64_C(17))));
  gg_function_3(t483, t484, base + 0);
  t485 = ((int64_t)(v1));
  t486 = ((int64_t)(((int64_t)UINT64_C(0))));
  t487 = ((t485) == (t486));
  if (t487) {
  {
  t488 = ((int64_t)(v2));
  t489 = gg_function_10(t488, base + 0);
  t490 = ((uint64_t)(t489));
  t491 = ((uint64_t)(v35));
  t492 = ((uint64_t)(v38));
  t493 = ((uint64_t)(v37));
  t494 = gg_function_11(t492, t493, base + 0);
  t495 = gg_function_13(t491, t494, base + 0);
  t496 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t497 = gg_function_11(t495, t496, base + 0);
  t498 = ((uint64_t)(t497));
  t499 = ((t490) >= (t498));
  t500 = ((int64_t)(((int64_t)UINT64_C(17))));
  gg_function_3(t499, t500, base + 0);
  }
  }
  t501 = ((uint64_t)(v18));
  t502 = ((uint64_t)(v16));
  t503 = ((t501) < (t502));
  if (t503) {
  {
  t504 = ((uint64_t)(v18));
  t505 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v41 = ((uint64_t)(((uint64_t)((uint64_t)(t504) - (uint64_t)(t505)))));
  }
  } else {
  {
  t506 = ((uint64_t)(v16));
  t507 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v41 = ((uint64_t)(((uint64_t)((uint64_t)(t506) - (uint64_t)(t507)))));
  }
  }
  t508 = ((uint64_t)(v20));
  t509 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t510 = ((uint64_t)(((uint64_t)((uint64_t)(t508) - (uint64_t)(t509)))));
  t511 = ((uint64_t)(v15));
  v42 = ((uint64_t)(((uint64_t)gg_udiv(t510, t511))));
  t512 = ((uint64_t)(v42));
  t513 = ((int64_t)(v11));
  t514 = ((int64_t)(((int64_t)UINT64_C(2))));
  t515 = ((int64_t)(((int64_t)UINT64_C(3))));
  t516 = fg174s_hget(t513, t514, t515);
  t517 = ((uint64_t)(t516));
  t518 = gg_function_11(t512, t517, base + 0);
  t519 = ((uint64_t)(v19));
  t520 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t521 = ((uint64_t)(((uint64_t)((uint64_t)(t519) - (uint64_t)(t520)))));
  t522 = ((int64_t)(v11));
  t523 = ((int64_t)(((int64_t)UINT64_C(2))));
  t524 = ((int64_t)(((int64_t)UINT64_C(2))));
  t525 = fg174s_hget(t522, t523, t524);
  t526 = ((uint64_t)(t525));
  t527 = gg_function_11(t521, t526, base + 0);
  t528 = gg_function_13(t518, t527, base + 0);
  t529 = ((uint64_t)(v41));
  t530 = ((int64_t)(v11));
  t531 = ((int64_t)(((int64_t)UINT64_C(2))));
  t532 = ((int64_t)(((int64_t)UINT64_C(1))));
  t533 = fg174s_hget(t530, t531, t532);
  t534 = ((uint64_t)(t533));
  t535 = gg_function_11(t529, t534, base + 0);
  t536 = ((uint64_t)(v17));
  t537 = gg_function_13(t535, t536, base + 0);
  v43 = gg_function_13(t528, t537, base + 0);
  t538 = ((int64_t)(v5));
  t539 = ((uint64_t)(v43));
  t540 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_23(t538, t539, t540, base + 0);
  t541 = ((int64_t)(v6));
  t542 = ((uint64_t)(v43));
  t543 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_23(t541, t542, t543, base + 0);
  t544 = ((uint64_t)(v20));
  t545 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t546 = ((uint64_t)(((uint64_t)((uint64_t)(t544) - (uint64_t)(t545)))));
  t547 = ((int64_t)(v11));
  t548 = ((int64_t)(((int64_t)UINT64_C(2))));
  t549 = ((int64_t)(((int64_t)UINT64_C(11))));
  t550 = fg174s_hget(t547, t548, t549);
  t551 = ((uint64_t)(t550));
  t552 = gg_function_11(t546, t551, base + 0);
  t553 = ((uint64_t)(v19));
  t554 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t555 = ((uint64_t)(((uint64_t)((uint64_t)(t553) - (uint64_t)(t554)))));
  t556 = ((int64_t)(v11));
  t557 = ((int64_t)(((int64_t)UINT64_C(2))));
  t558 = ((int64_t)(((int64_t)UINT64_C(10))));
  t559 = fg174s_hget(t556, t557, t558);
  t560 = ((uint64_t)(t559));
  t561 = gg_function_11(t555, t560, base + 0);
  t562 = gg_function_13(t552, t561, base + 0);
  t563 = ((uint64_t)(v18));
  t564 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t565 = ((uint64_t)(((uint64_t)((uint64_t)(t563) - (uint64_t)(t564)))));
  t566 = ((int64_t)(v11));
  t567 = ((int64_t)(((int64_t)UINT64_C(2))));
  t568 = ((int64_t)(((int64_t)UINT64_C(9))));
  t569 = fg174s_hget(t566, t567, t568);
  t570 = ((uint64_t)(t569));
  t571 = gg_function_11(t565, t570, base + 0);
  t572 = gg_function_13(t562, t571, base + 0);
  t573 = ((uint64_t)(v17));
  v44 = gg_function_13(t572, t573, base + 0);
  t574 = ((int64_t)(v7));
  t575 = ((uint64_t)(v44));
  t576 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_23(t574, t575, t576, base + 0);
  t577 = ((uint64_t)(v20));
  t578 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t579 = ((uint64_t)(((uint64_t)((uint64_t)(t577) - (uint64_t)(t578)))));
  t580 = ((int64_t)(v11));
  t581 = ((int64_t)(((int64_t)UINT64_C(2))));
  t582 = ((int64_t)(((int64_t)UINT64_C(19))));
  t583 = fg174s_hget(t580, t581, t582);
  t584 = ((uint64_t)(t583));
  t585 = gg_function_11(t579, t584, base + 0);
  t586 = ((uint64_t)(v19));
  t587 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t588 = ((uint64_t)(((uint64_t)((uint64_t)(t586) - (uint64_t)(t587)))));
  t589 = ((int64_t)(v11));
  t590 = ((int64_t)(((int64_t)UINT64_C(2))));
  t591 = ((int64_t)(((int64_t)UINT64_C(18))));
  t592 = fg174s_hget(t589, t590, t591);
  t593 = ((uint64_t)(t592));
  t594 = gg_function_11(t588, t593, base + 0);
  t595 = gg_function_13(t585, t594, base + 0);
  t596 = ((uint64_t)(v18));
  t597 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t598 = ((uint64_t)(((uint64_t)((uint64_t)(t596) - (uint64_t)(t597)))));
  t599 = ((int64_t)(v11));
  t600 = ((int64_t)(((int64_t)UINT64_C(2))));
  t601 = ((int64_t)(((int64_t)UINT64_C(17))));
  t602 = fg174s_hget(t599, t600, t601);
  t603 = ((uint64_t)(t602));
  t604 = gg_function_11(t598, t603, base + 0);
  v45 = gg_function_13(t595, t604, base + 0);
  t605 = ((uint64_t)(v45));
  t606 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v46 = gg_function_13(t605, t606, base + 0);
  t607 = ((int64_t)(v9));
  t608 = ((uint64_t)(v46));
  t609 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_23(t607, t608, t609, base + 0);
  t610 = v23;
  if (t610) {
  {
  t611 = ((uint64_t)(v46));
  t612 = ((uint64_t)(v17));
  v47 = gg_function_11(t611, t612, base + 0);
  }
  } else {
  {
  v47 = ((uint64_t)(v46));
  }
  }
  t613 = ((int64_t)(v8));
  t614 = ((uint64_t)(v47));
  t615 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_23(t613, t614, t615, base + 0);
  t616 = ((int64_t)(v10));
  t617 = ((uint64_t)(v20));
  t618 = ((uint64_t)(v18));
  t619 = gg_function_11(t617, t618, base + 0);
  t620 = ((uint64_t)(v17));
  t621 = gg_function_11(t619, t620, base + 0);
  t622 = ((uint64_t)(v17));
  t623 = gg_function_11(t621, t622, base + 0);
  t624 = ((int64_t)(((int64_t)UINT64_C(19))));
  gg_function_23(t616, t623, t624, base + 0);
  t625 = ((int64_t)(v2));
  t626 = ((int64_t)(((int64_t)UINT64_C(1))));
  t627 = ((int64_t)(((int64_t)UINT64_C(0))));
  t628 = fg174s_ti(t625, t626, t627);
  v48 = ((uint64_t)(((uint64_t)(t628))));
  t629 = ((uint64_t)(v35));
  t630 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v49 = gg_function_11(t629, t630, base + 0);
  t631 = ((uint64_t)(v48));
  t632 = ((uint64_t)(v49));
  t633 = ((uint64_t)(v39));
  t634 = ((uint64_t)(v40));
  t635 = gg_function_24(t631, t632, t633, t634, base + 0);
  t636 = t635;
  t637 = (!(t636));
  t638 = ((int64_t)(((int64_t)UINT64_C(25))));
  gg_function_3(t637, t638, base + 0);
  v50 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t639 = ((int64_t)(v50));
  t640 = ((int64_t)(((int64_t)UINT64_C(6))));
  if (!(((t639) < (t640)))) goto label8;
  {
  t641 = ((int64_t)(v2));
  t642 = ((int64_t)(((int64_t)UINT64_C(0))));
  t643 = ((int64_t)(v50));
  t644 = fg174s_ti(t641, t642, t643);
  v51 = ((int64_t)(t644));
  t645 = ((int64_t)(v51));
  t646 = ((int64_t)(((int64_t)UINT64_C(1))));
  t647 = ((int64_t)(((int64_t)UINT64_C(0))));
  t648 = fg174s_ti(t645, t646, t647);
  v52 = ((uint64_t)(((uint64_t)(t648))));
  t649 = ((int64_t)(v51));
  v53 = gg_function_10(t649, base + 0);
  t650 = ((uint64_t)(v48));
  t651 = ((uint64_t)(v49));
  t652 = ((uint64_t)(v52));
  t653 = ((uint64_t)(v53));
  t654 = gg_function_24(t650, t651, t652, t653, base + 0);
  t655 = t654;
  t656 = (!(t655));
  t657 = ((int64_t)(((int64_t)UINT64_C(25))));
  gg_function_3(t656, t657, base + 0);
  t658 = ((int64_t)(v50));
  t659 = ((int64_t)(((int64_t)UINT64_C(5))));
  t660 = ((t658) == (t659));
  t661 = t660;
  if (t661) {
  t662 = ((uint64_t)(v52));
  t663 = ((uint64_t)(v39));
  t661 = ((t662) == (t663));
  }
  t664 = t661;
  if (t664) {
  {
  t665 = ((uint64_t)(v53));
  t666 = ((uint64_t)(v36));
  t667 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t668 = gg_function_11(t666, t667, base + 0);
  t669 = ((uint64_t)(t668));
  t670 = ((t665) == (t669));
  t671 = ((int64_t)(((int64_t)UINT64_C(25))));
  gg_function_3(t670, t671, base + 0);
  }
  } else {
  {
  t672 = ((uint64_t)(v39));
  t673 = ((uint64_t)(v40));
  t674 = ((uint64_t)(v52));
  t675 = ((uint64_t)(v53));
  t676 = gg_function_24(t672, t673, t674, t675, base + 0);
  t677 = t676;
  t678 = (!(t677));
  t679 = ((int64_t)(((int64_t)UINT64_C(25))));
  gg_function_3(t678, t679, base + 0);
  }
  }
  t680 = v50;
  t681 = ((int64_t)(((int64_t)UINT64_C(1))));
  v50 = ((int64_t)((uint64_t)(t680) + (uint64_t)(t681)));
  }
  goto label7;
label8:;
  t682 = ((uint64_t)(v22));
  t683 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t684 = ((t682) == (t683));
  t685 = t684;
  if (t685) {
  t686 = ((int64_t)(v24));
  t687 = ((int64_t)(((int64_t)(v19))));
  t685 = ((t686) < (t687));
  }
  t688 = t685;
  t689 = t688;
  if (t689) {
  t690 = ((uint64_t)(v17));
  t691 = ((uint64_t)(((int64_t)UINT64_C(128))));
  t689 = ((t690) == (t691));
  }
  t692 = t689;
  t693 = t692;
  if (t693) {
  t694 = ((int64_t)(v11));
  t695 = ((int64_t)(((int64_t)UINT64_C(1))));
  t696 = ((int64_t)(((int64_t)UINT64_C(0))));
  t697 = fg174s_hget(t694, t695, t696);
  t698 = ((uint64_t)(t697));
  t699 = ((uint64_t)(((int64_t)UINT64_C(128))));
  t693 = ((t698) == (t699));
  }
  v54 = t693;
  t700 = ((int64_t)(v11));
  t701 = ((int64_t)(((int64_t)UINT64_C(3))));
  t702 = ((int64_t)(((int64_t)UINT64_C(0))));
  t703 = ((uint64_t)(v17));
  fg174s_hput(t700, t701, t702, t703);
  t704 = ((int64_t)(v11));
  t705 = ((int64_t)(((int64_t)UINT64_C(3))));
  t706 = ((int64_t)(((int64_t)UINT64_C(1))));
  t707 = ((uint64_t)(v18));
  fg174s_hput(t704, t705, t706, t707);
  t708 = ((int64_t)(v11));
  t709 = ((int64_t)(((int64_t)UINT64_C(3))));
  t710 = ((int64_t)(((int64_t)UINT64_C(2))));
  t711 = ((uint64_t)(v19));
  fg174s_hput(t708, t709, t710, t711);
  t712 = ((int64_t)(v11));
  t713 = ((int64_t)(((int64_t)UINT64_C(3))));
  t714 = ((int64_t)(((int64_t)UINT64_C(3))));
  t715 = ((uint64_t)(v20));
  fg174s_hput(t712, t713, t714, t715);
  t716 = ((int64_t)(v11));
  t717 = ((int64_t)(((int64_t)UINT64_C(3))));
  t718 = ((int64_t)(((int64_t)UINT64_C(4))));
  t719 = ((uint64_t)(v16));
  fg174s_hput(t716, t717, t718, t719);
  t720 = ((int64_t)(v11));
  t721 = ((int64_t)(((int64_t)UINT64_C(3))));
  t722 = ((int64_t)(((int64_t)UINT64_C(5))));
  t723 = ((uint64_t)(v15));
  fg174s_hput(t720, t721, t722, t723);
  t724 = ((int64_t)(v11));
  t725 = ((int64_t)(((int64_t)UINT64_C(3))));
  t726 = ((int64_t)(((int64_t)UINT64_C(6))));
  t727 = ((uint64_t)(((uint64_t)(v24))));
  fg174s_hput(t724, t725, t726, t727);
  t728 = ((int64_t)(v11));
  t729 = ((int64_t)(((int64_t)UINT64_C(3))));
  t730 = ((int64_t)(((int64_t)UINT64_C(7))));
  t732 = v25;
  if (t732) {
  {
  t731 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t731 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t733 = ((uint64_t)(t731));
  fg174s_hput(t728, t729, t730, t733);
  t734 = ((int64_t)(v11));
  t735 = ((int64_t)(((int64_t)UINT64_C(3))));
  t736 = ((int64_t)(((int64_t)UINT64_C(8))));
  t737 = ((uint64_t)(v18));
  fg174s_hput(t734, t735, t736, t737);
  t738 = ((int64_t)(v11));
  t739 = ((int64_t)(((int64_t)UINT64_C(3))));
  t740 = ((int64_t)(((int64_t)UINT64_C(9))));
  t741 = ((uint64_t)(v20));
  fg174s_hput(t738, t739, t740, t741);
  t742 = ((int64_t)(v11));
  t743 = ((int64_t)(((int64_t)UINT64_C(3))));
  t744 = ((int64_t)(((int64_t)UINT64_C(10))));
  t745 = ((uint64_t)(v34));
  fg174s_hput(t742, t743, t744, t745);
  t746 = ((int64_t)(v11));
  t747 = ((int64_t)(((int64_t)UINT64_C(3))));
  t748 = ((int64_t)(((int64_t)UINT64_C(11))));
  t749 = ((uint64_t)(v32));
  fg174s_hput(t746, t747, t748, t749);
  t750 = ((int64_t)(v11));
  t751 = ((int64_t)(((int64_t)UINT64_C(3))));
  t752 = ((int64_t)(((int64_t)UINT64_C(12))));
  t753 = ((uint64_t)(v28));
  fg174s_hput(t750, t751, t752, t753);
  t754 = ((int64_t)(v11));
  t755 = ((int64_t)(((int64_t)UINT64_C(3))));
  t756 = ((int64_t)(((int64_t)UINT64_C(13))));
  t757 = ((uint64_t)(((uint64_t)(v26))));
  fg174s_hput(t754, t755, t756, t757);
  t758 = ((int64_t)(v11));
  t759 = ((int64_t)(((int64_t)UINT64_C(3))));
  t760 = ((int64_t)(((int64_t)UINT64_C(14))));
  t761 = ((uint64_t)(((uint64_t)(v27))));
  fg174s_hput(t758, t759, t760, t761);
  t762 = ((int64_t)(v11));
  t763 = ((int64_t)(((int64_t)UINT64_C(3))));
  t764 = ((int64_t)(((int64_t)UINT64_C(15))));
  t765 = ((uint64_t)(v37));
  fg174s_hput(t762, t763, t764, t765);
  t766 = ((int64_t)(v11));
  t767 = ((int64_t)(((int64_t)UINT64_C(3))));
  t768 = ((int64_t)(((int64_t)UINT64_C(16))));
  t770 = v23;
  if (t770) {
  {
  t769 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t769 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t771 = ((uint64_t)(t769));
  fg174s_hput(t766, t767, t768, t771);
  t772 = ((int64_t)(v11));
  t773 = ((int64_t)(((int64_t)UINT64_C(3))));
  t774 = ((int64_t)(((int64_t)UINT64_C(17))));
  t776 = v54;
  if (t776) {
  {
  t775 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t775 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t777 = ((uint64_t)(t775));
  fg174s_hput(t772, t773, t774, t777);
  t778 = ((int64_t)(v11));
  t779 = ((int64_t)(((int64_t)UINT64_C(3))));
  t780 = ((int64_t)(((int64_t)UINT64_C(18))));
  t781 = ((uint64_t)(v16));
  fg174s_hput(t778, t779, t780, t781);
  t782 = ((int64_t)(v11));
  t783 = ((int64_t)(((int64_t)UINT64_C(0))));
  t784 = ((int64_t)(((int64_t)UINT64_C(7))));
  t785 = ((uint64_t)(v39));
  fg174s_hput(t782, t783, t784, t785);
  t786 = v54;
  if (t786) {
  {
  t787 = ((int64_t)(v0));
  t788 = fg174m_launch(t787);
  t789 = t788;
  if (t789) {
  {
  goto gg_done;
  }
  }
  }
  }
  t790 = ((int64_t)(v0));
  fg174s_launch(t790);
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
