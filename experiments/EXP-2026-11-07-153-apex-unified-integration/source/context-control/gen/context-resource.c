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
static int64_t gg_function_1(int64_t p0, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t base);
static int64_t gg_function_6(int64_t p0, int64_t base);
static void gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_13(int64_t p0, int64_t base);
static void gg_function_14(int64_t p0, int64_t base);
static void gg_function_15(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_16(int64_t p0, int64_t base);
static int64_t gg_function_17(int64_t p0, int64_t base);
static int64_t gg_function_18(int64_t p0, int64_t base);
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_21(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_23(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_24(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_25(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_27(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_28(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
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
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(67))));
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg107_r(t1, t2, t3, t4, t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) != (t8));
  if (t9) {
  {
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(52))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = fg107_r(t10, t11, t12, t13, t14);
  (void)(t15);
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(54))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg107_r(t16, t17, t18, t19, t20);
  t22 = ((int64_t)(t21));
  t23 = ((int64_t)(((int64_t)UINT64_C(2937339096))));
  t24 = ((t22) != (t23));
  if (t24) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(56))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = fg107_r(t25, t26, t27, t28, t29);
  v5 = ((int64_t)(t30));
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(63))));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg107_r(t31, t32, t33, t34, t35);
  t37 = ((int64_t)(t36));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t37) != (t38)))) gg_fail("assertion failed");
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(65))));
  t41 = ((int64_t)(v2));
  t42 = ((int64_t)(v3));
  t43 = ((int64_t)(v5));
  t44 = fg107_r(t39, t40, t41, t42, t43);
  (void)(t44);
  }
  } else {
  {
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(52))));
  t47 = ((int64_t)(v2));
  t48 = ((int64_t)(v3));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = fg107_r(t45, t46, t47, t48, t49);
  (void)(t50);
  }
  }
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(54))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = fg107_r(t51, t52, t53, t54, t55);
  t57 = ((int64_t)(t56));
  t58 = ((int64_t)(((int64_t)UINT64_C(2937339096))));
  t59 = ((t57) != (t58));
  if (t59) {
  {
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(62))));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = fg107_r(t60, t61, t62, t63, t64);
  (void)(t65);
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(32))));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = fg107_log(t66, t67, t68, t69);
  (void)(t70);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(((int64_t)UINT64_C(56))));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = fg107_r(t71, t72, t73, t74, t75);
  (void)(t76);
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(60))));
  t79 = ((int64_t)(v1));
  t80 = ((int64_t)(v4));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = fg107_r(t77, t78, t79, t80, t81);
  v6 = ((int64_t)(t82));
  t83 = ((int64_t)(v6));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((t83) < (t84));
  if (t85) {
  {
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(62))));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = fg107_r(t86, t87, t88, t89, t90);
  (void)(t91);
  t92 = ((int64_t)(v0));
  t93 = ((int64_t)(((int64_t)UINT64_C(32))));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = fg107_log(t92, t93, t94, t95);
  (void)(t96);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_28(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  t0 = gg_function_29(t1, t2, t3, t4, t5, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_27(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
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
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(67))));
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg107_r(t1, t2, t3, t4, t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) != (t8));
  if (t9) {
  {
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(64))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(v1));
  t15 = fg107_r(t10, t11, t12, t13, t14);
  (void)(t15);
  }
  } else {
  {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(51))));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(v3));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg107_r(t16, t17, t18, t19, t20);
  (void)(t21);
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(53))));
  t24 = ((int64_t)(((int64_t)UINT64_C(2937339096))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg107_r(t22, t23, t24, t25, t26);
  (void)(t27);
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(55))));
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = fg107_r(t28, t29, t30, t31, t32);
  (void)(t33);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(59))));
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(v4));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = fg107_r(t34, t35, t36, t37, t38);
  v5 = ((int64_t)(t39));
  t40 = ((int64_t)(v5));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t40) < (t41));
  if (t42) {
  {
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(31))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = fg107_log(t43, t44, t45, t46);
  (void)(t47);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  t0 = gg_function_27(t1, t2, t3, t4, t5, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_25(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(50))));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(67))));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg107_r(t3, t4, t5, t6, t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg107_r(t1, t2, t9, t10, t11);
  (void)(t12);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(53))));
  t15 = ((int64_t)(((int64_t)UINT64_C(2937339096))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg107_r(t13, t14, t15, t16, t17);
  (void)(t18);
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(55))));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg107_r(t19, t20, t21, t22, t23);
  (void)(t24);
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(59))));
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(v2));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = fg107_r(t25, t26, t27, t28, t29);
  v3 = ((int64_t)(t30));
  t31 = ((int64_t)(v3));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) < (t32));
  if (t33) {
  {
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(30))));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = fg107_log(t34, t35, t36, t37);
  (void)(t38);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_24(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  t0 = gg_function_25(t1, t2, t3, base + 0);
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(52))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg107_r(t1, t2, t3, t4, t5);
  (void)(t6);
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(58))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg107_r(t7, t8, t9, t10, t11);
  v3 = ((int64_t)(t12));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) < (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(62))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg107_r(t16, t17, t18, t19, t20);
  (void)(t21);
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(32))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg107_log(t22, t23, t24, t25);
  (void)(t26);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v3));
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
  t0 = gg_function_23(t1, t2, t3, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_21(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(51))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg107_r(t1, t2, t3, t4, t5);
  (void)(t6);
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(57))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg107_r(t7, t8, t9, t10, t11);
  v3 = ((int64_t)(t12));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) < (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(31))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg107_log(t16, t17, t18, t19);
  (void)(t20);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  t0 = gg_function_21(t1, t2, t3, base + 0);
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
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(50))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg107_r(t1, t2, t3, t4, t5);
  (void)(t6);
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(57))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg107_r(t7, t8, t9, t10, t11);
  v1 = ((int64_t)(t12));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) < (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(30))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg107_log(t16, t17, t18, t19);
  (void)(t20);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_17(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t0 = gg_function_18(t1, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
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
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(27))));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg107_r(t0, t1, t2, t3, t4);
  (void)(t5);
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(28))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg107_r(t6, t7, t8, t9, t10);
  (void)(t11);
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(18))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = fg107_rq(t12, t13, t14);
  t16 = ((int64_t)(t15));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(30))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg107_ctx(t20, t21, t22);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t24) != (t25));
  }
  t26 = t19;
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(v2));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t28) == (t29));
  }
  t30 = t27;
  t31 = t30;
  if (t31) {
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(11))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg107_rq(t32, t33, t34);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t36) == (t37));
  }
  t38 = t31;
  if (t38) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(29))));
  t41 = ((int64_t)(v3));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = fg107_r(t39, t40, t41, t42, t43);
  (void)(t44);
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(30))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = fg107_r(t45, t46, t47, t48, t49);
  (void)(t50);
  }
gg_done:;
}
static int64_t gg_function_6(int64_t p0, int64_t base) {
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
  uint8_t t39;
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
  uint8_t t65;
  int64_t t66;
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
  uint8_t t92;
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
  uint8_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
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
  uint8_t t193;
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
  uint8_t t210;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg107_rq(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg107_rq_set(t8, t9, t10);
  (void)(t11);
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = fg107_ctx(t12, t13, t14);
  t16 = ((int64_t)(t15));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  if (t18) {
  {
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(3))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg107_sched(t19, t20, t21, t22, t23);
  (void)(t24);
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(5))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = fg107_ctx(t25, t26, t27);
  v1 = ((int64_t)(t28));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = fg107_ctx(t29, t30, t31);
  v2 = ((int64_t)(t32));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(4))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg107_ctx(t33, t34, t35);
  v3 = ((int64_t)(t36));
  t37 = ((int64_t)(v2));
  t38 = ((int64_t)(v3));
  t39 = ((t37) < (t38));
  if (t39) {
  {
  v4 = ((int64_t)(v2));
  }
  } else {
  {
  v4 = ((int64_t)(v3));
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(34))));
  t42 = ((int64_t)(v4));
  t43 = fg107_ctx(t40, t41, t42);
  v5 = ((int64_t)(t43));
  t44 = ((int64_t)(v0));
  t45 = fg107_res_reset_all_prev(t44);
  (void)(t45);
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(18))));
  t48 = ((int64_t)(v5));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg107_r(t46, t47, t48, t49, t50);
  (void)(t51);
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(v5));
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(((int64_t)UINT64_C(15))));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = fg107_ctx(t54, t55, t56);
  t58 = ((int64_t)(t57));
  gg_function_10(t52, t53, t58, base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = fg107_ctx(t59, t60, t61);
  t63 = ((int64_t)(t62));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((t63) != (t64));
  if (t65) {
  {
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = fg107_mem(t66, t67, t68, t69, t70);
  v6 = ((int64_t)(t71));
  t72 = ((int64_t)(v6));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((t72) == (t73));
  if (t74) {
  {
  t75 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t75)))));
  goto gg_done;
  }
  }
  }
  }
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(((int64_t)UINT64_C(19))));
  t78 = ((int64_t)(v6));
  t79 = ((int64_t)(v1));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = fg107_r(t76, t77, t78, t79, t80);
  (void)(t81);
  t82 = ((int64_t)(v0));
  t83 = ((int64_t)(((int64_t)UINT64_C(30))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = fg107_ctx(t82, t83, t84);
  v7 = ((int64_t)(t85));
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = fg107_ctx(t86, t87, t88);
  t90 = ((int64_t)(t89));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((t90) == (t91));
  if (t92) {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t93 = ((int64_t)(v4));
  t94 = ((int64_t)(v0));
  t95 = ((int64_t)(((int64_t)UINT64_C(21))));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = fg107_ctx(t94, t95, t96);
  t98 = ((int64_t)(t97));
  t99 = ((t93) < (t98));
  if (t99) {
  {
  v8 = ((int64_t)(v4));
  }
  } else {
  {
  t100 = ((int64_t)(v0));
  t101 = ((int64_t)(((int64_t)UINT64_C(21))));
  t102 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = fg107_ctx(t100, t101, t102);
  v8 = ((int64_t)(t103));
  }
  }
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(v4));
  t106 = ((int64_t)(v1));
  t107 = ((int64_t)(v8));
  t108 = ((int64_t)(v6));
  t109 = ((int64_t)(v7));
  t110 = fg107_graph_reserve(t104, t105, t106, t107, t108, t109);
  v9 = ((int64_t)(t110));
  t111 = ((int64_t)(v9));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = ((t111) == (t112));
  if (t113) {
  {
  t114 = ((int64_t)(v0));
  t115 = ((int64_t)(((int64_t)UINT64_C(15))));
  t116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t117 = fg107_ctx(t114, t115, t116);
  t118 = ((int64_t)(t117));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((t118) != (t119));
  if (t120) {
  {
  t121 = ((int64_t)(v0));
  t122 = ((int64_t)(((int64_t)UINT64_C(2))));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = fg107_rq_set(t121, t122, t123);
  (void)(t124);
  t125 = ((int64_t)(v0));
  t126 = ((int64_t)(v5));
  t127 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_10(t125, t126, t127, base + 0);
  t128 = ((int64_t)(v0));
  t129 = ((int64_t)(v4));
  t130 = ((int64_t)(v1));
  t131 = ((int64_t)(v8));
  t132 = ((int64_t)(v6));
  t133 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = fg107_graph_reserve(t128, t129, t130, t131, t132, t133);
  v9 = ((int64_t)(t134));
  }
  }
  t135 = ((int64_t)(v9));
  t136 = ((int64_t)(((int64_t)UINT64_C(0))));
  t137 = ((t135) == (t136));
  if (t137) {
  {
  t138 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t138)))));
  goto gg_done;
  }
  }
  }
  }
  t139 = ((int64_t)(v0));
  t140 = ((int64_t)(((int64_t)UINT64_C(1))));
  t141 = ((int64_t)(v0));
  t142 = ((int64_t)(((int64_t)UINT64_C(33))));
  t143 = ((int64_t)(((int64_t)UINT64_C(0))));
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t145 = ((int64_t)(((int64_t)UINT64_C(0))));
  t146 = fg107_r(t141, t142, t143, t144, t145);
  t147 = ((int64_t)(t146));
  t148 = fg107_rq_set(t139, t140, t147);
  (void)(t148);
  t149 = ((int64_t)(v0));
  t150 = ((int64_t)(((int64_t)UINT64_C(3))));
  t151 = ((int64_t)(v0));
  t152 = ((int64_t)(((int64_t)UINT64_C(23))));
  t153 = ((int64_t)(v9));
  t154 = ((int64_t)(((int64_t)UINT64_C(0))));
  t155 = ((int64_t)(((int64_t)UINT64_C(0))));
  t156 = fg107_r(t151, t152, t153, t154, t155);
  t157 = ((int64_t)(t156));
  t158 = fg107_rq_set(t149, t150, t157);
  (void)(t158);
  t159 = ((int64_t)(v0));
  t160 = ((int64_t)(((int64_t)UINT64_C(3))));
  t161 = ((int64_t)(((int64_t)UINT64_C(0))));
  t162 = fg107_rq(t159, t160, t161);
  t163 = ((int64_t)(t162));
  t164 = ((int64_t)(((int64_t)UINT64_C(0))));
  t165 = ((t163) != (t164));
  if (t165) {
  {
  t166 = ((int64_t)(v0));
  t167 = ((int64_t)(((int64_t)UINT64_C(38))));
  t168 = ((int64_t)(((int64_t)UINT64_C(0))));
  t169 = ((int64_t)(((int64_t)UINT64_C(0))));
  t170 = ((int64_t)(((int64_t)UINT64_C(0))));
  t171 = fg107_r(t166, t167, t168, t169, t170);
  (void)(t171);
  }
  } else {
  {
  t172 = ((int64_t)(v0));
  t173 = ((int64_t)(v1));
  t174 = ((int64_t)(v1));
  t175 = ((int64_t)(v1));
  t176 = ((int64_t)(v6));
  t177 = ((int64_t)(v0));
  t178 = ((int64_t)(((int64_t)UINT64_C(4))));
  t179 = ((int64_t)(((int64_t)UINT64_C(0))));
  t180 = fg107_rq(t177, t178, t179);
  t181 = ((int64_t)(t180));
  t182 = fg107_graph_reserve(t172, t173, t174, t175, t176, t181);
  v10 = ((int64_t)(t182));
  t183 = ((int64_t)(v10));
  t184 = ((int64_t)(((int64_t)UINT64_C(0))));
  t185 = ((t183) == (t184));
  if (t185) {
  {
  t186 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t186)))));
  goto gg_done;
  }
  }
  }
  }
  t187 = ((int64_t)(v0));
  t188 = ((int64_t)(((int64_t)UINT64_C(19))));
  t189 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = fg107_rq(t187, t188, t189);
  t191 = ((int64_t)(t190));
  t192 = ((int64_t)(((int64_t)UINT64_C(0))));
  t193 = ((t191) != (t192));
  if (t193) {
  {
  t194 = ((int64_t)(v0));
  t195 = ((int64_t)(v4));
  t196 = ((int64_t)(((int64_t)UINT64_C(1))));
  t197 = ((int64_t)(v8));
  t198 = ((int64_t)(v6));
  t199 = ((int64_t)(v7));
  t200 = fg107_graph_reserve(t194, t195, t196, t197, t198, t199);
  v9 = ((int64_t)(t200));
  }
  } else {
  {
  t201 = ((int64_t)(v0));
  t202 = ((int64_t)(v4));
  t203 = ((int64_t)(v1));
  t204 = ((int64_t)(v8));
  t205 = ((int64_t)(v6));
  t206 = ((int64_t)(v7));
  t207 = fg107_graph_reserve(t201, t202, t203, t204, t205, t206);
  v9 = ((int64_t)(t207));
  }
  }
  t208 = ((int64_t)(v9));
  t209 = ((int64_t)(((int64_t)UINT64_C(0))));
  t210 = ((t208) == (t209));
  if (t210) {
  {
  t211 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t211)))));
  goto gg_done;
  }
  }
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t212 = ((int64_t)(v0));
  t213 = ((int64_t)(((int64_t)UINT64_C(37))));
  t214 = ((int64_t)(((int64_t)UINT64_C(0))));
  t215 = ((int64_t)(((int64_t)UINT64_C(0))));
  t216 = ((int64_t)(((int64_t)UINT64_C(0))));
  t217 = fg107_r(t212, t213, t214, t215, t216);
  v12 = ((int64_t)(t217));
label1:;
  t218 = ((int64_t)(v11));
  t219 = ((int64_t)(v12));
  if (!(((t218) < (t219)))) goto label2;
  {
  t220 = ((int64_t)(v0));
  t221 = ((int64_t)(((int64_t)UINT64_C(31))));
  t222 = ((int64_t)(v11));
  t223 = ((int64_t)(((int64_t)UINT64_C(0))));
  t224 = ((int64_t)(((int64_t)UINT64_C(0))));
  t225 = fg107_r(t220, t221, t222, t223, t224);
  v13 = ((int64_t)(t225));
  t226 = ((int64_t)(v0));
  t227 = ((int64_t)(((int64_t)UINT64_C(4))));
  t228 = ((int64_t)(((int64_t)UINT64_C(0))));
  t229 = fg107_rq(t226, t227, t228);
  t230 = ((int64_t)(t229));
  t231 = ((int64_t)(((int64_t)UINT64_C(0))));
  t232 = ((t230) == (t231));
  if (t232) {
  {
  t233 = ((int64_t)(v0));
  t234 = ((int64_t)(((int64_t)UINT64_C(26))));
  t235 = ((int64_t)(v11));
  t236 = ((int64_t)(v0));
  t237 = ((int64_t)(((int64_t)UINT64_C(25))));
  t238 = ((int64_t)(v13));
  t239 = ((int64_t)(((int64_t)UINT64_C(0))));
  t240 = ((int64_t)(((int64_t)UINT64_C(0))));
  t241 = fg107_r(t236, t237, t238, t239, t240);
  t242 = ((int64_t)(t241));
  t243 = ((int64_t)(((int64_t)UINT64_C(0))));
  t244 = fg107_r(t233, t234, t235, t242, t243);
  (void)(t244);
  }
  }
  t245 = ((int64_t)(v0));
  t246 = ((int64_t)(((int64_t)UINT64_C(38))));
  t247 = ((int64_t)(v11));
  t248 = ((int64_t)(((int64_t)UINT64_C(0))));
  t249 = ((int64_t)(((int64_t)UINT64_C(0))));
  t250 = fg107_r(t245, t246, t247, t248, t249);
  (void)(t250);
  t251 = v11;
  t252 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t251) + (uint64_t)(t252)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_16(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t0 = gg_function_6(t1, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_15(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  gg_function_10(t0, t1, t2, base + 0);
  }
gg_done:;
}
static void gg_function_14(int64_t p0, int64_t base) {
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
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg107_ctx(t0, t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(3))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg107_sched(t7, t8, t9, t10, t11);
  (void)(t12);
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg107_rq(t13, t14, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) == (t18));
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(34))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg107_r(t21, t22, t23, t24, t25);
  t27 = ((int64_t)(t26));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t27) == (t28));
  }
  t29 = t20;
  if (t29) {
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(37))));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg107_r(t30, t31, t32, t33, t34);
  v2 = ((int64_t)(t35));
label1:;
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(v2));
  if (!(((t36) < (t37)))) goto label2;
  {
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(36))));
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = fg107_r(t38, t39, t40, t41, t42);
  (void)(t43);
  t44 = v1;
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t44) + (uint64_t)(t45)));
  }
  goto label1;
label2:;
  }
  }
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(34))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg107_r(t46, t47, t48, t49, t50);
  (void)(t51);
  }
gg_done:;
}
static void gg_function_13(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  gg_function_14(t0, base + 0);
  }
gg_done:;
}
static int64_t gg_function_2(int64_t p0, int64_t base) {
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
  uint8_t t94;
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
  uint8_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint8_t t134;
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
  int64_t t172;
  int64_t t173;
  uint8_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  uint8_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  uint8_t t194;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg107_rq(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) != (t6));
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg107_rq(t8, t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) == (t13));
  if (t14) {
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = fg107_rq(t16, t17, t18);
  t20 = ((int64_t)(t19));
  if (!(((t15) < (t20)))) goto label2;
  {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg107_r(t21, t22, t23, t24, t25);
  v2 = ((int64_t)(t26));
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = fg107_r(t27, t28, t29, t30, t31);
  v3 = ((int64_t)(t32));
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t36 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t36)))));
  goto gg_done;
  }
  }
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(2))));
  t39 = ((int64_t)(v3));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = fg107_r(t37, t38, t39, t40, t41);
  (void)(t42);
  t43 = v1;
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  }
  goto label1;
label2:;
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(3))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg107_r(t46, t47, t48, t49, t50);
  t52 = ((int64_t)(t51));
  if (!(((t45) < (t52)))) goto label5;
  {
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(4))));
  t55 = ((int64_t)(v1));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = fg107_r(t53, t54, t55, t56, t57);
  v4 = ((int64_t)(t58));
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(5))));
  t61 = ((int64_t)(v4));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = fg107_r(t59, t60, t61, t62, t63);
  t65 = ((int64_t)(t64));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((t65) != (t66));
  if (t67) {
  {
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(1))));
  t70 = ((int64_t)(v4));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = fg107_r(t68, t69, t70, t71, t72);
  v5 = ((int64_t)(t73));
  t74 = ((int64_t)(v5));
  t75 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = ((t74) == (t75));
  if (t76) {
  {
  t77 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t77)))));
  goto gg_done;
  }
  }
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((int64_t)(v5));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = fg107_r(t78, t79, t80, t81, t82);
  (void)(t83);
  }
  }
  t84 = v1;
  t85 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t84) + (uint64_t)(t85)));
  }
  goto label4;
label5:;
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(6))));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = fg107_r(t86, t87, t88, t89, t90);
  v6 = ((int64_t)(t91));
  t92 = ((int64_t)(v6));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((t92) == (t93));
  if (t94) {
  {
  t95 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t95)))));
  goto gg_done;
  }
  }
  t96 = ((int64_t)(v0));
  t97 = ((int64_t)(((int64_t)UINT64_C(2))));
  t98 = ((int64_t)(v6));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = fg107_r(t96, t97, t98, t99, t100);
  (void)(t101);
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(((int64_t)UINT64_C(7))));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = fg107_r(t102, t103, t104, t105, t106);
  (void)(t107);
  t108 = ((int64_t)(v0));
  t109 = ((int64_t)(((int64_t)UINT64_C(8))));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = fg107_r(t108, t109, t110, t111, t112);
  (void)(t113);
  t114 = ((int64_t)(v0));
  t115 = ((int64_t)(((int64_t)UINT64_C(9))));
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(((int64_t)UINT64_C(5))));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = fg107_ctx(t116, t117, t118);
  t120 = ((int64_t)(t119));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = ((int64_t)(((int64_t)UINT64_C(0))));
  t123 = fg107_r(t114, t115, t120, t121, t122);
  t124 = ((int64_t)(t123));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((t124) == (t125));
  if (t126) {
  {
  t127 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t127)))));
  goto gg_done;
  }
  }
  }
  }
  t128 = ((int64_t)(v0));
  t129 = ((int64_t)(((int64_t)UINT64_C(0))));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = fg107_rq(t128, t129, t130);
  t132 = ((int64_t)(t131));
  t133 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = ((t132) == (t133));
  if (t134) {
  {
  t135 = ((int64_t)(v0));
  t136 = ((int64_t)(((int64_t)UINT64_C(10))));
  t137 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((int64_t)(((int64_t)UINT64_C(0))));
  t140 = fg107_r(t135, t136, t137, t138, t139);
  t141 = ((int64_t)(t140));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = ((t141) == (t142));
  if (t143) {
  {
  t144 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t144)))));
  goto gg_done;
  }
  }
  }
  }
  t145 = ((int64_t)(v0));
  t146 = ((int64_t)(((int64_t)UINT64_C(0))));
  t147 = ((int64_t)(((int64_t)UINT64_C(0))));
  t148 = fg107_rq(t145, t146, t147);
  t149 = ((int64_t)(t148));
  t150 = ((int64_t)(((int64_t)UINT64_C(0))));
  t151 = ((t149) == (t150));
  if (t151) {
  {
  t152 = ((int64_t)(v0));
  t153 = ((int64_t)(((int64_t)UINT64_C(11))));
  t154 = ((int64_t)(((int64_t)UINT64_C(0))));
  t155 = ((int64_t)(((int64_t)UINT64_C(0))));
  t156 = ((int64_t)(((int64_t)UINT64_C(0))));
  t157 = fg107_r(t152, t153, t154, t155, t156);
  (void)(t157);
  t158 = ((int64_t)(v0));
  t159 = ((int64_t)(((int64_t)UINT64_C(12))));
  t160 = ((int64_t)(((int64_t)UINT64_C(0))));
  t161 = ((int64_t)(((int64_t)UINT64_C(0))));
  t162 = ((int64_t)(((int64_t)UINT64_C(0))));
  t163 = fg107_r(t158, t159, t160, t161, t162);
  (void)(t163);
  t164 = ((int64_t)(v0));
  t165 = ((int64_t)(((int64_t)UINT64_C(13))));
  t166 = ((int64_t)(((int64_t)UINT64_C(0))));
  t167 = ((int64_t)(((int64_t)UINT64_C(0))));
  t168 = ((int64_t)(((int64_t)UINT64_C(0))));
  t169 = fg107_r(t164, t165, t166, t167, t168);
  (void)(t169);
  t170 = ((int64_t)(v0));
  t171 = gg_function_6(t170, base + 0);
  t172 = ((int64_t)(t171));
  t173 = ((int64_t)(((int64_t)UINT64_C(0))));
  t174 = ((t172) != (t173));
  if (t174) {
  {
  t175 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t175)))));
  goto gg_done;
  }
  }
  t176 = ((int64_t)(v0));
  t177 = ((int64_t)(((int64_t)UINT64_C(14))));
  t178 = ((int64_t)(((int64_t)UINT64_C(0))));
  t179 = ((int64_t)(((int64_t)UINT64_C(0))));
  t180 = ((int64_t)(((int64_t)UINT64_C(0))));
  t181 = fg107_r(t176, t177, t178, t179, t180);
  t182 = ((int64_t)(t181));
  t183 = ((int64_t)(((int64_t)UINT64_C(0))));
  t184 = ((t182) == (t183));
  if (t184) {
  {
  t185 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t185)))));
  goto gg_done;
  }
  }
  }
  }
  t186 = ((int64_t)(v0));
  t187 = ((int64_t)(((int64_t)UINT64_C(15))));
  t188 = ((int64_t)(((int64_t)UINT64_C(0))));
  t189 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = ((int64_t)(((int64_t)UINT64_C(0))));
  t191 = fg107_r(t186, t187, t188, t189, t190);
  t192 = ((int64_t)(t191));
  t193 = ((int64_t)(((int64_t)UINT64_C(0))));
  t194 = ((t192) != (t193));
  if (t194) {
  {
  t195 = ((int64_t)(v0));
  t196 = ((int64_t)(((int64_t)UINT64_C(16))));
  t197 = ((int64_t)(((int64_t)UINT64_C(0))));
  t198 = ((int64_t)(((int64_t)UINT64_C(0))));
  t199 = ((int64_t)(((int64_t)UINT64_C(0))));
  t200 = fg107_r(t195, t196, t197, t198, t199);
  (void)(t200);
  }
  }
  t201 = ((int64_t)(v0));
  t202 = ((int64_t)(((int64_t)UINT64_C(17))));
  t203 = ((int64_t)(((int64_t)UINT64_C(0))));
  t204 = ((int64_t)(((int64_t)UINT64_C(0))));
  t205 = ((int64_t)(((int64_t)UINT64_C(0))));
  t206 = fg107_r(t201, t202, t203, t204, t205);
  (void)(t206);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t0 = gg_function_2(t1, base + 0);
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
  gg_header t26;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  v0 = gg_function_1(t0, base + 0);
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_13(t1, base + 0);
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_15(t2, t3, t4, base + 0);
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  v1 = gg_function_16(t5, base + 0);
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  v2 = gg_function_17(t6, base + 0);
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  v3 = gg_function_20(t7, t8, t9, base + 0);
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  v4 = gg_function_22(t10, t11, t12, base + 0);
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = gg_function_24(t13, t14, t15, base + 0);
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  v6 = gg_function_26(t16, t17, t18, t19, t20, base + 0);
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = gg_function_28(t21, t22, t23, t24, t25, base + 0);
  t26.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t26.len = 0;
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = ((int64_t)(v2));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t32 = ((int64_t)(v3));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = ((int64_t)(v4));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
  t36 = ((int64_t)(v5));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t38 = ((int64_t)(v6));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t40 = ((int64_t)(v7));
  gg_integer(gg_ref(&t26, gg_get_stack(base + 0)), ((int64_t)((uint64_t)(t39) + (uint64_t)(t40))));
  gg_append(gg_ref(&t26, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t26).base, (t26).len));
  }
gg_done:;
  for (t41 = 0; t41 < 1; t41++) gg_stacks[base + t41].top = 0;
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
