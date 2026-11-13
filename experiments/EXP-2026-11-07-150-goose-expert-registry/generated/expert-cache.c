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
static uint8_t gg_eq_4(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint32_t value);
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
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
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
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t base);
static void gg_function_3(int64_t p0, int64_t base);
static void gg_function_7(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_8(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_10(int64_t p0, int64_t base);
static int64_t gg_function_11(int64_t p0, int64_t base);
static int64_t gg_function_12(int64_t p0, int64_t base);
static int64_t gg_function_13(int64_t p0, int64_t base);
static int64_t gg_function_14(int64_t p0, int64_t base);
static int64_t gg_function_19(double p0, int64_t base);
static int64_t gg_function_21(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_23(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_23(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t14;
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
  t10 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) - (uint64_t)(t11)))));
  t8 = ((t9) <= (t12));
  }
  if (!(t8)) gg_fail("assertion failed");
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_21(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
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
  if (!(t4)) gg_fail("assertion failed");
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)gg_div(t12, t13))));
  t10 = ((t11) <= (t14));
  }
  if (!(t10)) gg_fail("assertion failed");
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t12;
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
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(20))));
  t3 = ((int64_t)(v1));
  t4 = fg150_budget_get(t1, t2, t3);
  v3 = ((int64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(12))));
  t7 = ((int64_t)(v3));
  t8 = fg150_budget_get(t5, t6, t7);
  v4 = ((int64_t)(t8));
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) > (t10));
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) - (uint64_t)(t15)))));
  t17 = ((int64_t)(((t13) & (t16))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t17) == (t18));
  }
  if (!(t12)) gg_fail("assertion failed");
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(24))));
  t21 = ((int64_t)(v1));
  t22 = fg150_budget_get(t19, t20, t21);
  v6 = ((int64_t)(t22));
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(25))));
  t26 = ((int64_t)(v1));
  t27 = fg150_budget_get(t24, t25, t26);
  t28 = t27;
  t23 = 0;
label0:;
  if (t23 >= t28) goto label2;
  v7 = t23;
  {
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(30))));
  t31 = ((int64_t)(v6));
  t32 = ((int64_t)(v7));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = fg150_budget_get(t29, t30, t33);
  t35 = ((int64_t)(t34));
  t36 = ((int64_t)(v2));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  t38 = gg_function_23(t36, t37, base + 0);
  v8 = gg_function_21(t35, t38, base + 0);
  t39 = ((int64_t)(v5));
  t40 = ((int64_t)(v8));
  t41 = ((int64_t)(v4));
  t42 = ((int64_t)(((int64_t)UINT64_C(1))));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) - (uint64_t)(t42)))));
  t44 = gg_function_23(t40, t43, base + 0);
  t45 = ((int64_t)(t44));
  t46 = ((int64_t)(v4));
  t47 = ((int64_t)(((int64_t)gg_div(t45, t46))));
  t48 = ((int64_t)(v4));
  t49 = gg_function_21(t47, t48, base + 0);
  v5 = gg_function_23(t39, t49, base + 0);
  }
label1:;
  t23++; goto label0;
label2:;
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_19(double p0, int64_t base) {
  int64_t t0;
  double t1;
  double t2;
  uint8_t t3;
  uint8_t t4;
  double t5;
  double t6;
  double t7;
  double t8;
  uint8_t t9;
  double t10;
  uint64_t t11;
  uint64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  int64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint8_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  int64_t t34;
  uint64_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  uint64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  uint64_t t48;
  double v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  uint64_t v5;
  v0 = p0;
  {
  t1 = ((double)(v0));
  t2 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((double)(v0));
  t6 = ((double)(((double)(((int64_t)UINT64_C(9223372036854775807))))));
  t4 = ((t5) < (t6));
  }
  if (!(t4)) gg_fail("assertion failed");
  t7 = ((double)(v0));
  t8 = ((double)(((double)(((int64_t)UINT64_C(1))))));
  t9 = ((t7) < (t8));
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t10 = ((double)(v0));
  t11 = fg150_budget_double_bits(t10);
  v1 = ((uint64_t)(t11));
  t12 = ((uint64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(52))));
  t14 = ((uint64_t)(((t12) >> ((uint64_t)(t13) & 63))));
  t15 = ((uint64_t)(((int64_t)UINT64_C(2047))));
  v2 = ((int64_t)(((int64_t)(((t14) & (t15))))));
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)UINT64_C(1023))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t16) - (uint64_t)(t17)))));
  t18 = ((uint64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(63))));
  t20 = ((uint64_t)(((t18) >> ((uint64_t)(t19) & 63))));
  t21 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) == (t21));
  t23 = t22;
  if (t23) {
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t24) >= (t25));
  }
  t26 = t23;
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)UINT64_C(63))));
  t27 = ((t28) < (t29));
  }
  if (!(t27)) gg_fail("assertion failed");
  t30 = ((uint64_t)(v1));
  t31 = ((uint64_t)(((int64_t)UINT64_C(4503599627370495))));
  t32 = ((uint64_t)(((t30) & (t31))));
  t33 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t34 = ((int64_t)(((int64_t)UINT64_C(52))));
  t35 = ((uint64_t)(((uint64_t)((uint64_t)(t33) << ((uint64_t)(t34) & 63)))));
  v4 = ((uint64_t)(((t32) | (t35))));
  t36 = ((int64_t)(v3));
  t37 = ((int64_t)(((int64_t)UINT64_C(52))));
  t38 = ((t36) >= (t37));
  if (t38) {
  {
  t39 = ((uint64_t)(v4));
  t40 = ((int64_t)(v3));
  t41 = ((int64_t)(((int64_t)UINT64_C(52))));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) - (uint64_t)(t41)))));
  v5 = ((uint64_t)(((uint64_t)((uint64_t)(t39) << ((uint64_t)(t42) & 63)))));
  }
  } else {
  {
  t43 = ((uint64_t)(v4));
  t44 = ((int64_t)(((int64_t)UINT64_C(52))));
  t45 = ((int64_t)(v3));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) - (uint64_t)(t45)))));
  v5 = ((uint64_t)(((t43) >> ((uint64_t)(t46) & 63))));
  }
  }
  t47 = ((uint64_t)(v5));
  t48 = ((uint64_t)(((int64_t)UINT64_C(9223372036854775807))));
  if (!(((t47) <= (t48)))) gg_fail("assertion failed");
  t0 = ((int64_t)(((int64_t)(v5))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_14(int64_t p0, int64_t base) {
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
  int64_t t29;
  int64_t t30;
  int64_t t31;
  double t32;
  double t33;
  double t34;
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
  uint8_t t48;
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
  uint8_t t60;
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
  double t71;
  double t72;
  double t73;
  int64_t t74;
  int64_t t75;
  double t76;
  double t77;
  double t78;
  double t79;
  double t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
  int64_t t91;
  int64_t t92;
  double t93;
  double t94;
  double t95;
  double t96;
  double t97;
  uint8_t t98;
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
  uint8_t t110;
  uint8_t t111;
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
  double t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  double t126;
  double t127;
  double t128;
  double t129;
  double t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  double t135;
  double t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  double t141;
  double t142;
  double t143;
  uint8_t t144;
  uint8_t t145;
  double t146;
  double t147;
  double t148;
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
  uint8_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  uint8_t t176;
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
  uint8_t t207;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint8_t v5;
  int64_t v6;
  double v7;
  int64_t v8;
  double v9;
  int64_t v10;
  int64_t v11;
  double v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  double v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg150_budget_get(t1, t2, t3);
  v1 = ((int64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(3))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg150_budget_get(t5, t6, t7);
  v2 = ((int64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg150_budget_get(t9, t10, t11);
  v3 = ((int64_t)(t12));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg150_budget_get(t13, t14, t15);
  v4 = ((int64_t)(t16));
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) >= (t18));
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t21) > (t22));
  }
  if (!(t20)) gg_fail("assertion failed");
  v5 = ((uint8_t)UINT64_C(1));
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(4))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg150_budget_get(t24, t25, t26);
  t28 = t27;
  t23 = 0;
label0:;
  if (t23 >= t28) goto label2;
  v6 = t23;
  {
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(v6));
  t32 = fg150_budget_float(t29, t30, t31);
  t33 = ((double)(t32));
  t34 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t35 = ((t33) != (t34));
  if (t35) {
  {
  v5 = ((uint8_t)UINT64_C(0));
  }
  }
  }
label1:;
  t23++; goto label0;
label2:;
  v7 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t37 = v1;
  t36 = 0;
label3:;
  if (t36 >= t37) goto label5;
  v8 = t36;
  {
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = ((int64_t)(v8));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg150_budget_put(t38, t39, t40, t41);
  v9 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(10))));
  t44 = ((int64_t)(v8));
  t45 = fg150_budget_get(t42, t43, t44);
  t46 = ((int64_t)(t45));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((t46) > (t47));
  if (t48) {
  {
  t49 = v5;
  if (t49) {
  {
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(11))));
  t52 = ((int64_t)(v8));
  t53 = fg150_budget_get(t50, t51, t52);
  v9 = ((double)(((double)(t53))));
  }
  } else {
  {
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(((int64_t)UINT64_C(13))));
  t56 = ((int64_t)(v8));
  t57 = fg150_budget_get(t54, t55, t56);
  v10 = ((int64_t)(t57));
  t58 = ((int64_t)(v10));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((t58) >= (t59));
  t61 = t60;
  if (t61) {
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(((int64_t)UINT64_C(4))));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = fg150_budget_get(t63, t64, t65);
  t67 = ((int64_t)(t66));
  t61 = ((t62) < (t67));
  }
  if (!(t61)) gg_fail("assertion failed");
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = ((int64_t)(v10));
  t71 = fg150_budget_float(t68, t69, t70);
  v9 = ((double)(t71));
  }
  }
  }
  }
  t72 = ((double)(v9));
  t73 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  if (!(((t72) >= (t73)))) gg_fail("assertion failed");
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(v8));
  t76 = ((double)(v9));
  fg150_budget_put_float(t74, t75, t76);
  t77 = v7;
  t78 = ((double)(v9));
  v7 = ((t77) + (t78));
  }
label4:;
  t36++; goto label3;
label5:;
  t79 = ((double)(v7));
  t80 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t81 = ((t79) == (t80));
  if (t81) {
  {
  t83 = v1;
  t82 = 0;
label6:;
  if (t82 >= t83) goto label8;
  v11 = t82;
  {
  t84 = ((int64_t)(v0));
  t85 = ((int64_t)(((int64_t)UINT64_C(10))));
  t86 = ((int64_t)(v11));
  t87 = fg150_budget_get(t84, t85, t86);
  t88 = ((int64_t)(t87));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((t88) > (t89));
  if (t90) {
  {
  v12 = ((double)(((double)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  v12 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  }
  }
  t91 = ((int64_t)(v0));
  t92 = ((int64_t)(v11));
  t93 = ((double)(v12));
  fg150_budget_put_float(t91, t92, t93);
  t94 = v7;
  t95 = ((double)(v12));
  v7 = ((t94) + (t95));
  }
label7:;
  t82++; goto label6;
label8:;
  }
  }
  t96 = ((double)(v7));
  t97 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t98 = ((t96) > (t97));
  t99 = t98;
  if (!t99) {
  t100 = ((int64_t)(v2));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((t100) == (t101));
  }
  if (!(t99)) gg_fail("assertion failed");
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = v2;
  t102 = 0;
label9:;
  if (t102 >= t103) goto label11;
  v14 = t102;
  {
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(((int64_t)UINT64_C(20))));
  t106 = ((int64_t)(v14));
  t107 = fg150_budget_get(t104, t105, t106);
  v15 = ((int64_t)(t107));
  t108 = ((int64_t)(v15));
  t109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = ((t108) >= (t109));
  t111 = t110;
  if (t111) {
  t112 = ((int64_t)(v15));
  t113 = ((int64_t)(v1));
  t111 = ((t112) < (t113));
  }
  t114 = t111;
  t115 = t114;
  if (t115) {
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(((int64_t)UINT64_C(10))));
  t118 = ((int64_t)(v15));
  t119 = fg150_budget_get(t116, t117, t118);
  t120 = ((int64_t)(t119));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t115 = ((t120) > (t121));
  }
  if (!(t115)) gg_fail("assertion failed");
  t122 = ((double)(((double)(v3))));
  t123 = ((int64_t)(v0));
  t124 = ((int64_t)(((int64_t)UINT64_C(1))));
  t125 = ((int64_t)(v15));
  t126 = fg150_budget_float(t123, t124, t125);
  t127 = ((double)(t126));
  t128 = ((double)(((t122) * (t127))));
  t129 = ((double)(v7));
  t130 = ((double)(((t128) / (t129))));
  t131 = ((int64_t)(v0));
  t132 = ((int64_t)(((int64_t)UINT64_C(21))));
  t133 = ((int64_t)(v14));
  t134 = fg150_budget_get(t131, t132, t133);
  t135 = ((double)(((double)(t134))));
  t136 = ((double)(((t130) * (t135))));
  t137 = ((int64_t)(v0));
  t138 = ((int64_t)(((int64_t)UINT64_C(10))));
  t139 = ((int64_t)(v15));
  t140 = fg150_budget_get(t137, t138, t139);
  t141 = ((double)(((double)(t140))));
  v16 = ((double)(((t136) / (t141))));
  t142 = ((double)(v16));
  t143 = ((double)(((double)(((int64_t)UINT64_C(0))))));
  t144 = ((t142) >= (t143));
  t145 = t144;
  if (t145) {
  t146 = ((double)(v16));
  t147 = ((double)(((double)(((int64_t)UINT64_C(9223372036854775807))))));
  t145 = ((t146) < (t147));
  }
  if (!(t145)) gg_fail("assertion failed");
  t148 = ((double)(v16));
  v17 = gg_function_19(t148, base + 0);
  t149 = ((int64_t)(v0));
  t150 = ((int64_t)(((int64_t)UINT64_C(22))));
  t151 = ((int64_t)(v14));
  t152 = fg150_budget_get(t149, t150, t151);
  v18 = ((int64_t)(t152));
  t153 = ((int64_t)(v18));
  t154 = ((int64_t)(v0));
  t155 = ((int64_t)(((int64_t)UINT64_C(23))));
  t156 = ((int64_t)(v14));
  t157 = fg150_budget_get(t154, t155, t156);
  t158 = ((int64_t)(t157));
  v19 = gg_function_21(t153, t158, base + 0);
  t159 = ((int64_t)(v19));
  t160 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  if (!(((t159) <= (t160)))) gg_fail("assertion failed");
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t161 = ((int64_t)(v20));
  t162 = ((int64_t)(v19));
  if (!(((t161) < (t162)))) goto label14;
  {
  t163 = ((int64_t)(v0));
  t164 = ((int64_t)(v14));
  t165 = ((int64_t)(v20));
  t166 = ((int64_t)(((int64_t)UINT64_C(1))));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t165) + (uint64_t)(t166)))));
  t168 = gg_function_22(t163, t164, t167, base + 0);
  t169 = ((int64_t)(t168));
  t170 = ((int64_t)(v17));
  t171 = ((t169) > (t170));
  if (t171) {
  {
  goto label14;
  }
  }
  t172 = v20;
  t173 = ((int64_t)(((int64_t)UINT64_C(1))));
  v20 = ((int64_t)((uint64_t)(t172) + (uint64_t)(t173)));
  }
  goto label13;
label14:;
  t174 = ((int64_t)(v20));
  t175 = ((int64_t)(v4));
  t176 = ((t174) < (t175));
  if (t176) {
  {
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t177 = ((int64_t)(v0));
  t178 = ((int64_t)(((int64_t)UINT64_C(25))));
  t179 = ((int64_t)(v14));
  t180 = fg150_budget_get(t177, t178, t179);
  t181 = ((int64_t)(t180));
  t182 = ((int64_t)(((int64_t)UINT64_C(1))));
  t183 = ((int64_t)(v18));
  t184 = gg_function_23(t182, t183, base + 0);
  v21 = gg_function_21(t181, t184, base + 0);
  t185 = ((int64_t)(v0));
  t186 = ((int64_t)(((int64_t)UINT64_C(1))));
  t187 = ((int64_t)(v15));
  t188 = ((int64_t)(v0));
  t189 = ((int64_t)(((int64_t)UINT64_C(40))));
  t190 = ((int64_t)(v15));
  t191 = fg150_budget_get(t188, t189, t190);
  t192 = ((int64_t)(t191));
  t193 = ((int64_t)(v21));
  t194 = gg_function_23(t192, t193, base + 0);
  fg150_budget_put(t185, t186, t187, t194);
  t195 = ((int64_t)(v13));
  t196 = ((int64_t)(v18));
  v13 = gg_function_23(t195, t196, base + 0);
  }
  }
  t197 = ((int64_t)(v0));
  t198 = ((int64_t)(((int64_t)UINT64_C(0))));
  t199 = ((int64_t)(v14));
  t200 = ((int64_t)(v20));
  fg150_budget_put(t197, t198, t199, t200);
  }
label10:;
  t102++; goto label9;
label11:;
  t201 = ((int64_t)(v0));
  t202 = ((int64_t)(((int64_t)UINT64_C(2))));
  t203 = ((int64_t)(((int64_t)UINT64_C(0))));
  t204 = ((int64_t)(v13));
  fg150_budget_put(t201, t202, t203, t204);
  t205 = ((int64_t)(v13));
  t206 = ((int64_t)(((int64_t)UINT64_C(0))));
  t207 = ((t205) > (t206));
  if (t207) {
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
static int64_t gg_function_13(int64_t p0, int64_t base) {
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
  uint8_t t12;
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
  uint8_t t33;
  uint8_t t34;
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
  uint8_t t47;
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
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(40))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg150_cache_get(t1, t2, t3);
  v1 = ((int64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(34))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg150_cache_get(t5, t6, t7);
  v2 = ((int64_t)(t8));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) >= (t10));
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v2));
  t12 = ((t13) < (t14));
  }
  if (!(t12)) gg_fail("assertion failed");
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(18))));
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = fg150_cache_get(t15, t16, t19);
  v3 = ((int64_t)(t20));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(18))));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  t28 = fg150_cache_get(t21, t22, t27);
  v4 = ((int64_t)(t28));
  v5 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(v5));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t32 = ((int64_t)(v2));
  t33 = ((t31) < (t32));
  t34 = t33;
  if (t34) {
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(18))));
  t37 = ((int64_t)(((int64_t)UINT64_C(2))));
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(v5));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t40)))));
  t42 = fg150_cache_get(t35, t36, t41);
  t43 = ((int64_t)(t42));
  t44 = ((int64_t)(v3));
  t45 = ((int64_t)(v5));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t34 = ((t43) == (t46));
  }
  t47 = t34;
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(18))));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(v5));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)UINT64_C(1))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
  t58 = fg150_cache_get(t49, t50, t57);
  t59 = ((int64_t)(t58));
  t60 = ((int64_t)(v4));
  t61 = ((int64_t)(v5));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  t48 = ((t59) == (t62));
  }
  if (!(t48)) goto label2;
  {
  t63 = v5;
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t63) + (uint64_t)(t64)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_12(int64_t p0, int64_t base) {
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
  uint8_t t43;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(40))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg150_cache_get(t1, t2, t3);
  v1 = ((int64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(41))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg150_cache_get(t5, t6, t7);
  v2 = ((int64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(34))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg150_cache_get(t9, t10, t11);
  v3 = ((int64_t)(t12));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) == (t14));
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t17) < (t18));
  }
  t19 = t16;
  t20 = t19;
  if (!t20) {
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(v1));
  t20 = ((t21) < (t22));
  }
  t23 = t20;
  t24 = t23;
  if (!t24) {
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(v3));
  t24 = ((t25) >= (t26));
  }
  t27 = t24;
  t28 = t27;
  if (!t28) {
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(v3));
  t28 = ((t29) >= (t30));
  }
  t31 = t28;
  if (t31) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(18))));
  t34 = ((int64_t)(v1));
  t35 = fg150_cache_get(t32, t33, t34);
  v4 = ((int64_t)(t35));
  t36 = ((int64_t)(v4));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) < (t37));
  if (t38) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v5 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(v5));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  t42 = ((int64_t)(v2));
  t43 = ((t41) <= (t42));
  t44 = t43;
  if (t44) {
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(18))));
  t47 = ((int64_t)(v1));
  t48 = ((int64_t)(v5));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t50 = fg150_cache_get(t45, t46, t49);
  t51 = ((int64_t)(t50));
  t52 = ((int64_t)(v4));
  t53 = ((int64_t)(v5));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t44 = ((t51) == (t54));
  }
  if (!(t44)) goto label2;
  {
  t55 = v5;
  t56 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t55) + (uint64_t)(t56)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_11(int64_t p0, int64_t base) {
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
  uint8_t t12;
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
  uint8_t t23;
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
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
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
  uint8_t t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
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
  t2 = ((int64_t)(((int64_t)UINT64_C(40))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg150_cache_get(t1, t2, t3);
  v1 = ((int64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(41))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg150_cache_get(t5, t6, t7);
  v2 = ((int64_t)(t8));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) < (t10));
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v2));
  t12 = ((t13) >= (t14));
  }
  t15 = t12;
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(42))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg150_cache_get(t17, t18, t19);
  t21 = ((int64_t)(t20));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t21) < (t22));
  }
  t23 = t16;
  if (t23) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(43))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg150_cache_get(t24, t25, t26);
  v3 = ((int64_t)(t27));
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(44))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg150_cache_get(t28, t29, t30);
  v4 = ((int64_t)(t31));
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(46))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg150_cache_get(t32, t33, t34);
  v5 = ((int64_t)(t35));
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(45))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = fg150_cache_get(t36, t37, t38);
  v6 = ((int64_t)(t39));
  t40 = ((int64_t)(v3));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t40) <= (t41));
  t43 = t42;
  if (!t43) {
  t44 = ((int64_t)(v3));
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(47))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = fg150_cache_get(t45, t46, t47);
  t49 = ((int64_t)(t48));
  t43 = ((t44) > (t49));
  }
  t50 = t43;
  t51 = t50;
  if (!t51) {
  t52 = ((int64_t)(v4));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((t52) < (t53));
  }
  t54 = t51;
  if (t54) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t55 = ((int64_t)(v4));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t55) == (t56));
  t58 = t57;
  if (!t58) {
  t59 = ((int64_t)(v3));
  t60 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t61 = ((int64_t)(v4));
  t62 = ((int64_t)(((int64_t)gg_div(t60, t61))));
  t58 = ((t59) <= (t62));
  }
  if (!(t58)) gg_fail("assertion failed");
  t63 = ((int64_t)(v3));
  t64 = ((int64_t)(v4));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  t66 = ((int64_t)(v7));
  t67 = ((int64_t)(v5));
  t68 = ((t66) < (t67));
  if (t68) {
  {
  t65 = ((int64_t)(v7));
  }
  } else {
  {
  t65 = ((int64_t)(v5));
  }
  }
  t69 = ((int64_t)(t65));
  t70 = ((int64_t)(v6));
  t71 = ((t69) <= (t70));
  if (t71) {
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
static void gg_function_7(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
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
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(13))));
  t2 = ((int64_t)(v1));
  t3 = fg150_cache_get(t0, t1, t2);
  v3 = ((int64_t)(t3));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v2));
  t6 = ((t4) == (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(17))));
  t11 = ((int64_t)(v3));
  t12 = fg150_cache_get(t9, t10, t11);
  t13 = ((int64_t)(t12));
  t7 = ((t8) == (t13));
  }
  t14 = t7;
  if (t14) {
  {
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(11))));
  t17 = ((int64_t)(v1));
  t18 = fg150_cache_get(t15, t16, t17);
  v4 = ((int64_t)(t18));
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(12))));
  t21 = ((int64_t)(v1));
  t22 = fg150_cache_get(t19, t20, t21);
  v5 = ((int64_t)(t22));
  t23 = ((int64_t)(v4));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) >= (t24));
  if (t25) {
  {
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(12))));
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(v5));
  fg150_cache_put(t26, t27, t28, t29);
  }
  } else {
  {
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(16))));
  t32 = ((int64_t)(v3));
  t33 = ((int64_t)(v5));
  fg150_cache_put(t30, t31, t32, t33);
  }
  }
  t34 = ((int64_t)(v5));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) >= (t35));
  if (t36) {
  {
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(11))));
  t39 = ((int64_t)(v5));
  t40 = ((int64_t)(v4));
  fg150_cache_put(t37, t38, t39, t40);
  }
  } else {
  {
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(17))));
  t43 = ((int64_t)(v3));
  t44 = ((int64_t)(v4));
  fg150_cache_put(t41, t42, t43, t44);
  }
  }
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(17))));
  t47 = ((int64_t)(v2));
  t48 = fg150_cache_get(t45, t46, t47);
  v6 = ((int64_t)(t48));
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(11))));
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(v6));
  fg150_cache_put(t49, t50, t51, t52);
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(12))));
  t55 = ((int64_t)(v1));
  t56 = ((int64_t)UINT64_C(1));
  t57 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t56)))));
  fg150_cache_put(t53, t54, t55, t57);
  t58 = ((int64_t)(v6));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((t58) >= (t59));
  if (t60) {
  {
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(12))));
  t63 = ((int64_t)(v6));
  t64 = ((int64_t)(v1));
  fg150_cache_put(t61, t62, t63, t64);
  }
  } else {
  {
  t65 = ((int64_t)(v0));
  t66 = ((int64_t)(((int64_t)UINT64_C(16))));
  t67 = ((int64_t)(v2));
  t68 = ((int64_t)(v1));
  fg150_cache_put(t65, t66, t67, t68);
  }
  }
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(((int64_t)UINT64_C(17))));
  t71 = ((int64_t)(v2));
  t72 = ((int64_t)(v1));
  fg150_cache_put(t69, t70, t71, t72);
  t73 = ((int64_t)(v0));
  t74 = ((int64_t)(((int64_t)UINT64_C(13))));
  t75 = ((int64_t)(v1));
  t76 = ((int64_t)(v2));
  fg150_cache_put(t73, t74, t75, t76);
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(3))));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(((int64_t)UINT64_C(3))));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = fg150_cache_get(t80, t81, t82);
  t84 = ((int64_t)(t83));
  t85 = ((int64_t)(v2));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t85)))));
  t87 = ((int64_t)(v3));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) - (uint64_t)(t87)))));
  fg150_cache_put(t77, t78, t79, t88);
  }
gg_done:;
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
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  uint8_t t38;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg150_cache_get(t1, t2, t3);
  v2 = ((int64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(4))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg150_cache_get(t5, t6, t7);
  v3 = ((int64_t)(t8));
  t10 = ((int64_t)UINT64_C(2));
  t9 = 0;
label0:;
  if (t9 >= t10) goto label2;
  v4 = t9;
  {
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(16))));
  t13 = ((int64_t)(v4));
  t14 = fg150_cache_get(t11, t12, t13);
  v5 = ((int64_t)(t14));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t15 = ((int64_t)(v5));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t15) >= (t16)))) goto label5;
  {
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg150_cache_get(t18, t19, t20);
  t22 = ((int64_t)(t21));
  if (!(((t17) < (t22)))) gg_fail("assertion failed");
  t23 = v6;
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t23) + (uint64_t)(t24)));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(10))));
  t27 = ((int64_t)(v5));
  t28 = fg150_cache_get(t25, t26, t27);
  v7 = ((int64_t)(t28));
  t29 = ((int64_t)(v7));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t29) < (t30));
  t32 = t31;
  if (!t32) {
  t33 = ((int64_t)(v7));
  t34 = ((int64_t)(v2));
  t35 = ((int64_t)(((int64_t)gg_div(t33, t34))));
  t36 = ((int64_t)(v1));
  t32 = ((t35) != (t36));
  }
  t37 = t32;
  t38 = t37;
  if (!t38) {
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(14))));
  t41 = ((int64_t)(v7));
  t42 = ((int64_t)(v2));
  t43 = ((int64_t)(((int64_t)gg_mod(t41, t42))));
  t44 = fg150_cache_get(t39, t40, t43);
  t45 = ((int64_t)(t44));
  t46 = ((int64_t)(v3));
  t38 = ((t45) != (t46));
  }
  t47 = t38;
  if (t47) {
  {
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
  }
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(12))));
  t50 = ((int64_t)(v5));
  t51 = fg150_cache_get(t48, t49, t50);
  v5 = ((int64_t)(t51));
  }
  goto label4;
label5:;
  }
label1:;
  t9++; goto label0;
label2:;
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  t52 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t52)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_8(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_7(t0, t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(3))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg150_cache_get(t3, t4, t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg150_cache_get(t8, t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(((int64_t)gg_div(t12, t13))));
  t15 = ((t7) > (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(16))));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = fg150_cache_get(t17, t18, t19);
  t21 = ((int64_t)(t20));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_7(t16, t21, t22, base + 0);
  }
  }
  }
gg_done:;
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
  int64_t t29;
  int64_t t30;
  uint32_t t31;
  uint32_t t32;
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
  int64_t t61;
  uint8_t t62;
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
  uint8_t t116;
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
  uint8_t t137;
  uint8_t t138;
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
  uint8_t t181;
  int64_t t182;
  int64_t t183;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg150_cache_action(t1, t2, t3, t4);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg150_cache_action(t5, t6, t7, t8);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg150_cache_get(t9, t10, t11);
  v1 = ((int64_t)(t12));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(33))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg150_cache_get(t13, t14, t15);
  v2 = ((int64_t)(t16));
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) >= (t18));
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = fg150_cache_get(t22, t23, t24);
  t26 = ((int64_t)(t25));
  t20 = ((t21) < (t26));
  }
  if (!(t20)) gg_fail("assertion failed");
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = fg150_cache_get(t27, t28, t29);
  t31 = ((uint32_t)(((uint32_t)(t30))));
  t32 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(1))))));
  v3 = ((int64_t)(((int64_t)(((uint32_t)((uint64_t)(t31) + (uint64_t)(t32)))))));
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t37 = v1;
  t36 = 0;
label0:;
  if (t36 >= t37) goto label2;
  v4 = t36;
  {
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(14))));
  t40 = ((int64_t)(v4));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg150_cache_put(t38, t39, t40, t41);
  }
label1:;
  t36++; goto label0;
label2:;
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(4))));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg150_cache_put(t42, t43, t44, t45);
  }
  } else {
  {
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(4))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(v3));
  fg150_cache_put(t46, t47, t48, t49);
  }
  }
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(34))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = fg150_cache_get(t51, t52, t53);
  t55 = t54;
  t50 = 0;
label3:;
  if (t50 >= t55) goto label5;
  v5 = t50;
  {
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(((int64_t)UINT64_C(18))));
  t58 = ((int64_t)(v5));
  t59 = fg150_cache_get(t56, t57, t58);
  v6 = ((int64_t)(t59));
  t60 = ((int64_t)(v6));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((t60) >= (t61));
  t63 = t62;
  if (t63) {
  t64 = ((int64_t)(v6));
  t65 = ((int64_t)(v1));
  t63 = ((t64) < (t65));
  }
  if (!(t63)) gg_fail("assertion failed");
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(14))));
  t68 = ((int64_t)(v6));
  t69 = fg150_cache_get(t66, t67, t68);
  t70 = ((int64_t)(t69));
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(((int64_t)UINT64_C(4))));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = fg150_cache_get(t71, t72, t73);
  t75 = ((int64_t)(t74));
  t76 = ((t70) != (t75));
  if (t76) {
  {
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(14))));
  t79 = ((int64_t)(v6));
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(((int64_t)UINT64_C(4))));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = fg150_cache_get(t80, t81, t82);
  t84 = ((int64_t)(t83));
  fg150_cache_put(t77, t78, t79, t84);
  t85 = ((int64_t)(v0));
  t86 = ((int64_t)(((int64_t)UINT64_C(2))));
  t87 = ((int64_t)(v6));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg150_cache_action(t85, t86, t87, t88);
  }
  }
  }
label4:;
  t50++; goto label3;
label5:;
  t89 = ((int64_t)(v0));
  t90 = ((int64_t)(((int64_t)UINT64_C(5))));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = fg150_cache_get(t89, t90, t91);
  v7 = ((int64_t)(t92));
  t93 = ((int64_t)(v7));
  t94 = ((int64_t)(v0));
  t95 = ((int64_t)(((int64_t)UINT64_C(1))));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = fg150_cache_get(t94, t95, t96);
  t98 = ((int64_t)(t97));
  t99 = ((t93) > (t98));
  if (t99) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t101 = v7;
  t100 = 0;
label6:;
  if (t100 >= t101) goto label8;
  v8 = t100;
  {
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(((int64_t)UINT64_C(15))));
  t104 = ((int64_t)(v8));
  t105 = fg150_cache_get(t102, t103, t104);
  v9 = ((int64_t)(t105));
  t106 = ((int64_t)(v0));
  t107 = ((int64_t)(((int64_t)UINT64_C(19))));
  t108 = ((int64_t)(v2));
  t109 = ((int64_t)(v1));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = ((int64_t)(v9));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  t113 = fg150_cache_get(t106, t107, t112);
  v10 = ((int64_t)(t113));
  t114 = ((int64_t)(v10));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((t114) >= (t115));
  if (t116) {
  {
  t117 = ((int64_t)(v0));
  t118 = ((int64_t)(v10));
  gg_function_8(t117, t118, base + 0);
  t119 = ((int64_t)(v0));
  t120 = ((int64_t)(((int64_t)UINT64_C(6))));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = ((int64_t)(v0));
  t123 = ((int64_t)(((int64_t)UINT64_C(6))));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t125 = fg150_cache_get(t122, t123, t124);
  t126 = ((int64_t)(t125));
  t127 = ((int64_t)(((int64_t)UINT64_C(1))));
  t128 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t127)))));
  fg150_cache_put(t119, t120, t121, t128);
  }
  }
  }
label7:;
  t100++; goto label6;
label8:;
  v11 = ((int64_t)(((int64_t)UINT64_C(1))));
label10:;
  t129 = ((int64_t)(v11));
  t130 = ((int64_t)(v7));
  if (!(((t129) < (t130)))) goto label11;
  {
  t131 = ((int64_t)(v0));
  t132 = ((int64_t)(((int64_t)UINT64_C(15))));
  t133 = ((int64_t)(v11));
  t134 = fg150_cache_get(t131, t132, t133);
  v12 = ((int64_t)(t134));
  v13 = ((int64_t)(v11));
label13:;
  t135 = ((int64_t)(v13));
  t136 = ((int64_t)(((int64_t)UINT64_C(0))));
  t137 = ((t135) > (t136));
  t138 = t137;
  if (t138) {
  t139 = ((int64_t)(v0));
  t140 = ((int64_t)(((int64_t)UINT64_C(15))));
  t141 = ((int64_t)(v13));
  t142 = ((int64_t)(((int64_t)UINT64_C(1))));
  t143 = ((int64_t)(((int64_t)((uint64_t)(t141) - (uint64_t)(t142)))));
  t144 = fg150_cache_get(t139, t140, t143);
  t145 = ((int64_t)(t144));
  t146 = ((int64_t)(v12));
  t138 = ((t145) > (t146));
  }
  if (!(t138)) goto label14;
  {
  t147 = ((int64_t)(v0));
  t148 = ((int64_t)(((int64_t)UINT64_C(15))));
  t149 = ((int64_t)(v13));
  t150 = ((int64_t)(v0));
  t151 = ((int64_t)(((int64_t)UINT64_C(15))));
  t152 = ((int64_t)(v13));
  t153 = ((int64_t)(((int64_t)UINT64_C(1))));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t152) - (uint64_t)(t153)))));
  t155 = fg150_cache_get(t150, t151, t154);
  t156 = ((int64_t)(t155));
  fg150_cache_put(t147, t148, t149, t156);
  t157 = v13;
  t158 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)((uint64_t)(t157) - (uint64_t)(t158)));
  }
  goto label13;
label14:;
  t159 = ((int64_t)(v0));
  t160 = ((int64_t)(((int64_t)UINT64_C(15))));
  t161 = ((int64_t)(v13));
  t162 = ((int64_t)(v12));
  fg150_cache_put(t159, t160, t161, t162);
  t163 = v11;
  t164 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t163) + (uint64_t)(t164)));
  }
  goto label10;
label11:;
  t166 = v7;
  t165 = 0;
label15:;
  if (t165 >= t166) goto label17;
  v14 = t165;
  {
  t167 = ((int64_t)(v0));
  t168 = ((int64_t)(((int64_t)UINT64_C(15))));
  t169 = ((int64_t)(v14));
  t170 = fg150_cache_get(t167, t168, t169);
  v15 = ((int64_t)(t170));
  t171 = ((int64_t)(v0));
  t172 = ((int64_t)(((int64_t)UINT64_C(19))));
  t173 = ((int64_t)(v2));
  t174 = ((int64_t)(v1));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) * (uint64_t)(t174)))));
  t176 = ((int64_t)(v15));
  t177 = ((int64_t)(((int64_t)((uint64_t)(t175) + (uint64_t)(t176)))));
  t178 = fg150_cache_get(t171, t172, t177);
  t179 = ((int64_t)(t178));
  t180 = ((int64_t)(((int64_t)UINT64_C(0))));
  t181 = ((t179) < (t180));
  if (t181) {
  {
  t182 = ((int64_t)(v0));
  t183 = ((int64_t)(v2));
  v16 = gg_function_9(t182, t183, base + 0);
  t184 = ((int64_t)(v0));
  t185 = ((int64_t)(((int64_t)UINT64_C(10))));
  t186 = ((int64_t)(v16));
  t187 = fg150_cache_get(t184, t185, t186);
  v17 = ((int64_t)(t187));
  t188 = ((int64_t)(v17));
  t189 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = ((t188) >= (t189));
  if (t190) {
  {
  t191 = ((int64_t)(v0));
  t192 = ((int64_t)(((int64_t)UINT64_C(19))));
  t193 = ((int64_t)(v17));
  t194 = ((int64_t)UINT64_C(1));
  t195 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t194)))));
  fg150_cache_put(t191, t192, t193, t195);
  }
  }
  t196 = ((int64_t)(v0));
  t197 = ((int64_t)(((int64_t)UINT64_C(10))));
  t198 = ((int64_t)(v16));
  t199 = ((int64_t)(v2));
  t200 = ((int64_t)(v1));
  t201 = ((int64_t)(((int64_t)((uint64_t)(t199) * (uint64_t)(t200)))));
  t202 = ((int64_t)(v15));
  t203 = ((int64_t)(((int64_t)((uint64_t)(t201) + (uint64_t)(t202)))));
  fg150_cache_put(t196, t197, t198, t203);
  t204 = ((int64_t)(v0));
  t205 = ((int64_t)(((int64_t)UINT64_C(19))));
  t206 = ((int64_t)(v2));
  t207 = ((int64_t)(v1));
  t208 = ((int64_t)(((int64_t)((uint64_t)(t206) * (uint64_t)(t207)))));
  t209 = ((int64_t)(v15));
  t210 = ((int64_t)(((int64_t)((uint64_t)(t208) + (uint64_t)(t209)))));
  t211 = ((int64_t)(v16));
  fg150_cache_put(t204, t205, t210, t211);
  t212 = ((int64_t)(v0));
  t213 = ((int64_t)(v16));
  t214 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_7(t212, t213, t214, base + 0);
  t215 = ((int64_t)(v0));
  t216 = ((int64_t)(((int64_t)UINT64_C(4))));
  t217 = ((int64_t)(v15));
  t218 = ((int64_t)(v16));
  fg150_cache_action(t215, t216, t217, t218);
  }
  }
  }
label16:;
  t165++; goto label15;
label17:;
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_3(int64_t p0, int64_t base) {
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
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  uint8_t t23;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(30))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg150_cache_get(t0, t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(31))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg150_cache_get(t4, t5, t6);
  v2 = ((int64_t)(t7));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(32))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg150_cache_get(t8, t9, t10);
  v3 = ((int64_t)(t11));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) >= (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) > (t17));
  }
  t18 = t15;
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t20) > (t21));
  }
  t22 = t19;
  t23 = t22;
  if (t23) {
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)(((int64_t)gg_div(t25, t26))));
  t23 = ((t24) <= (t27));
  }
  if (!(t23)) gg_fail("assertion failed");
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg150_cache_action(t28, t29, t30, t31);
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(v2));
  fg150_cache_put(t32, t33, t34, t35);
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((int64_t)(v3));
  fg150_cache_put(t36, t37, t38, t39);
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(3))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg150_cache_put(t40, t41, t42, t43);
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(4))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg150_cache_put(t44, t45, t46, t47);
  t49 = v3;
  t48 = 0;
label0:;
  if (t48 >= t49) goto label2;
  v4 = t48;
  {
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(10))));
  t52 = ((int64_t)(v4));
  t53 = ((int64_t)UINT64_C(1));
  t54 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t53)))));
  fg150_cache_put(t50, t51, t52, t54);
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(11))));
  t57 = ((int64_t)(v4));
  t58 = ((int64_t)(v4));
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) - (uint64_t)(t59)))));
  fg150_cache_put(t55, t56, t57, t60);
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(12))));
  t63 = ((int64_t)(v4));
  t65 = ((int64_t)(v4));
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = ((int64_t)(v3));
  t69 = ((t67) < (t68));
  if (t69) {
  {
  t70 = ((int64_t)(v4));
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  }
  } else {
  {
  t72 = ((int64_t)UINT64_C(1));
  t64 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t72)))));
  }
  }
  t73 = ((int64_t)(t64));
  fg150_cache_put(t61, t62, t63, t73);
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(13))));
  t76 = ((int64_t)(v4));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg150_cache_put(t74, t75, t76, t77);
  }
label1:;
  t48++; goto label0;
label2:;
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(((int64_t)UINT64_C(16))));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg150_cache_put(t78, t79, t80, t81);
  t82 = ((int64_t)(v0));
  t83 = ((int64_t)(((int64_t)UINT64_C(17))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((int64_t)(v3));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t85) - (uint64_t)(t86)))));
  fg150_cache_put(t82, t83, t84, t87);
  t88 = ((int64_t)(v0));
  t89 = ((int64_t)(((int64_t)UINT64_C(16))));
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  t91 = ((int64_t)UINT64_C(1));
  t92 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t91)))));
  fg150_cache_put(t88, t89, t90, t92);
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(((int64_t)UINT64_C(17))));
  t95 = ((int64_t)(((int64_t)UINT64_C(1))));
  t96 = ((int64_t)UINT64_C(1));
  t97 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t96)))));
  fg150_cache_put(t93, t94, t95, t97);
  t99 = v2;
  t98 = 0;
label3:;
  if (t98 >= t99) goto label5;
  v5 = t98;
  {
  t100 = ((int64_t)(v0));
  t101 = ((int64_t)(((int64_t)UINT64_C(14))));
  t102 = ((int64_t)(v5));
  t103 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg150_cache_put(t100, t101, t102, t103);
  }
label4:;
  t98++; goto label3;
label5:;
  }
gg_done:;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
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
  uint8_t t22;
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
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  fg150_cache_witness(t1);
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) == (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v1));
  gg_function_3(t5, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((t6) == (t7));
  if (t8) {
  {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(35))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg150_cache_get(t10, t11, t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(36))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg150_cache_get(t15, t16, t17);
  t19 = ((int64_t)(t18));
  gg_function_7(t9, t14, t19, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(2))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(((int64_t)UINT64_C(35))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg150_cache_get(t24, t25, t26);
  t28 = ((int64_t)(t27));
  gg_function_8(t23, t28, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(3))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)UINT64_C(33))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg150_cache_get(t33, t34, t35);
  t37 = ((int64_t)(t36));
  t0 = gg_function_9(t32, t37, base + 0);
  goto gg_done;
  }
  }
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(4))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  t41 = ((int64_t)(v1));
  t0 = gg_function_10(t41, base + 0);
  goto gg_done;
  }
  }
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(5))));
  t44 = ((t42) == (t43));
  if (t44) {
  {
  t45 = ((int64_t)(v1));
  t0 = gg_function_11(t45, base + 0);
  goto gg_done;
  }
  }
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(6))));
  t48 = ((t46) == (t47));
  if (t48) {
  {
  t49 = ((int64_t)(v1));
  t0 = gg_function_12(t49, base + 0);
  goto gg_done;
  }
  }
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(7))));
  t52 = ((t50) == (t51));
  if (t52) {
  {
  t53 = ((int64_t)(v1));
  t0 = gg_function_13(t53, base + 0);
  goto gg_done;
  }
  }
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t54) == (t55)))) gg_fail("assertion failed");
  t56 = ((int64_t)(v1));
  t0 = gg_function_14(t56, base + 0);
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
  {
  t0.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(5))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = gg_function_1(t1, t2, base + 1);
  gg_integer(gg_ref(&t0, gg_get_stack(base + 0)), t3);
  gg_append(gg_ref(&t0, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t0).base, (t0).len));
  }
gg_done:;
  for (t4 = 0; t4 < 1; t4++) gg_stacks[base + t4].top = 0;
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
