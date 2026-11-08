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
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static int64_t gg_function_3(int64_t p0, int64_t p1, int64_t base);
static void gg_function_4(uint8_t p0, int64_t base);
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t base);
static void gg_function_9(int64_t p0, int64_t base);
static int64_t gg_function_10(int64_t p0, int64_t base);
static void gg_function_12(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_13(int64_t base);
static void gg_function_14(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_15(int64_t base);
static void gg_function_16(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_19(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_21(int64_t p0, int64_t base);
static int64_t gg_function_22(int64_t p0, int64_t base);
static void gg_function_23(int64_t p0, int64_t base);
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_33(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_34(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_35(int64_t p0, int64_t base);
static int64_t gg_function_37(int64_t p0, int64_t p1, int64_t base);
static void gg_function_38(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_39(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_40(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_41(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_42(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_43(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_44(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_51(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_53(int64_t p0, int64_t base);
static int64_t gg_function_54(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_55(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_56(int64_t p0, int64_t base);
static int64_t gg_function_57(int64_t p0, int64_t base);
static int64_t gg_function_58(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_59(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_60(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_62(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_63(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_64(int64_t p0, int64_t p1, int64_t base);
static void gg_function_65(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_66(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_67(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_68(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_69(int64_t p0, int64_t p1, int64_t base);
static void gg_function_70(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_71(int64_t p0, int64_t p1, int64_t base);
static void gg_function_72(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_80(int64_t p0, int64_t p1, int64_t base);
static void gg_function_82(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_83(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_84(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_85(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_91(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_92(int64_t p0, int64_t base);
static int64_t gg_function_93(int64_t p0, int64_t base);
static int64_t gg_function_94(int64_t p0, int64_t base);
static int64_t gg_function_95(int64_t p0, int64_t base);
static int64_t gg_function_96(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_97(int64_t p0, int64_t base);
static int64_t gg_function_98(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_99(int64_t p0, int64_t base);
static int64_t gg_function_100(int64_t p0, int64_t base);
static int64_t gg_function_101(int64_t p0, int64_t base);
static int64_t gg_function_102(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_105(int64_t p0, int64_t base);
static void gg_function_107(int64_t p0, int64_t base);
static int64_t gg_function_108(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_109(int64_t p0, int64_t base);
static int64_t gg_function_110(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_111(int64_t p0, int64_t base);
static int64_t gg_function_112(int64_t p0, int64_t base);
static int64_t gg_function_113(int64_t p0, int64_t base);
static int64_t gg_function_114(int64_t p0, int64_t base);
static int64_t gg_function_115(int64_t p0, int64_t base);
static void gg_function_116(int64_t p0, int64_t base);
static int64_t gg_function_117(int64_t base);
static void gg_function_124(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_128(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static void gg_function_140(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base);
static int64_t gg_function_141(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_148(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_150(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static int64_t gg_function_154(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static void gg_function_158(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_4(uint8_t p0, int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t v0;
  int64_t v1;
  v0 = p0;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg_kv_service(t2, t3, t4);
  v1 = ((int64_t)(t5));
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
gg_done:;
}
static int64_t gg_function_56(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg_kv_slot_len(t1, t2);
  t0 = ((int64_t)(t3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_19(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(100))));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(v2));
  fg_kv_put(t0, t3, t4, t5);
  }
gg_done:;
}
static int64_t gg_function_34(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(100))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg_kv_get(t1, t4, t5);
  t0 = ((int64_t)(t6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_16(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(1000))));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v3));
  fg_w2_put(t0, t3, t4, t5);
  }
gg_done:;
}
static int64_t gg_function_15(int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t v0;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(900))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_w2_get(t1, t2, t3);
  v0 = ((int64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) > (t6));
  gg_function_4(t7, base + 0);
  t0 = ((int64_t)(v0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_14(int64_t p0, int64_t p1, int64_t base) {
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
  v0 = p0;
  v1 = p1;
  {
  v2 = gg_function_15(base + 0);
  t1 = v2;
  t0 = 0;
label0:;
  if (t0 >= t1) goto label2;
  v3 = t0;
  {
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_16(t2, t3, t4, t5, base + 0);
  }
label1:;
  t0++; goto label0;
label2:;
  }
gg_done:;
}
static int64_t gg_function_13(int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(901))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_w2_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_12(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(10))));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)UINT64_C(1));
  t4 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t3)))));
  fg_w2_put(t0, t1, t2, t4);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(11))));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_w2_put(t5, t6, t7, t8);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(12))));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_w2_put(t9, t10, t11, t12);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(13))));
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_w2_put(t13, t14, t15, t16);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(14))));
  t19 = ((int64_t)(v1));
  t20 = gg_function_13(base + 0);
  fg_w2_put(t17, t18, t19, t20);
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(v1));
  gg_function_14(t21, t22, base + 0);
  }
gg_done:;
}
static int64_t gg_function_41(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1000))));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = ((int64_t)(v1));
  t6 = fg_w2_get(t1, t4, t5);
  t0 = ((int64_t)(t6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_60(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  v2 = gg_function_15(base + 0);
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = v2;
  t1 = 0;
label0:;
  if (t1 >= t2) goto label2;
  v4 = t1;
  {
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v4));
  t6 = gg_function_41(t3, t4, t5, base + 0);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) != (t8));
  if (t9) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
label1:;
  t1++; goto label0;
label2:;
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_69(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
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
  gg_function_4(t7, base + 0);
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) == (t9));
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)gg_div(t13, t14))));
  t11 = ((t12) <= (t15));
  }
  t16 = t11;
  gg_function_4(t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_68(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
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
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(2147483648))));
  t4 = ((t5) < (t6));
  }
  t7 = t4;
  gg_function_4(t7, base + 0);
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) >= (t9));
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(4294967296))));
  t11 = ((t12) < (t13));
  }
  t14 = t11;
  gg_function_4(t14, base + 0);
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(4294967296))));
  t17 = gg_function_69(t15, t16, base + 0);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
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
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_w2_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_38(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  gg_function_4(t2, base + 0);
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) >= (t4));
  t6 = t5;
  if (t6) {
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v0));
  t9 = gg_function_10(t8, base + 0);
  t10 = ((int64_t)(t9));
  t6 = ((t7) < (t10));
  }
  t11 = t6;
  gg_function_4(t11, base + 0);
  }
gg_done:;
}
static int64_t gg_function_66(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  gg_function_38(t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  t6 = gg_function_41(t3, t4, t5, base + 0);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) != (t8));
  if (t9) {
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
static int64_t gg_function_83(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(10))));
  t3 = ((int64_t)(v1));
  t4 = fg_w2_get(t1, t2, t3);
  v4 = ((int64_t)(t4));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(v2));
  t7 = ((t5) >= (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(v3));
  t8 = ((t9) < (t10));
  }
  t11 = t8;
  if (t11) {
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
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(10))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg_kv_get(t1, t4, t5);
  t0 = ((int64_t)(t6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_33(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_kv_get(t1, t2, t3);
  v2 = ((int64_t)(t4));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) >= (t9));
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v2));
  t11 = ((t12) < (t13));
  }
  t14 = t11;
  gg_function_4(t14, base + 0);
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_82(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
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
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  v4 = gg_function_33(t0, t1, base + 0);
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v4));
  v5 = gg_function_6(t2, t3, base + 0);
  t4 = ((int64_t)(v5));
  v6 = gg_function_10(t4, base + 0);
  v7 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t5 = ((int64_t)(v8));
  t6 = ((int64_t)(v6));
  if (!(((t5) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(v3));
  t11 = gg_function_83(t7, t8, t9, t10, base + 0);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) != (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(v8));
  t18 = ((int64_t)(v1));
  t19 = gg_function_66(t16, t17, t18, base + 0);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t20) != (t21));
  }
  t22 = t15;
  if (t22) {
  {
  t23 = ((int64_t)(v5));
  t24 = ((int64_t)(v8));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_16(t23, t24, t25, t26, base + 0);
  t27 = ((int64_t)(v5));
  t28 = ((int64_t)(((int64_t)UINT64_C(25))));
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = fg_w2_get(t27, t30, t31);
  t33 = ((int64_t)(t32));
  t34 = ((int64_t)(v5));
  t35 = ((int64_t)(((int64_t)UINT64_C(10))));
  t36 = ((int64_t)(v8));
  t37 = fg_w2_get(t34, t35, t36);
  t38 = ((int64_t)(t37));
  t39 = ((int64_t)(v8));
  t40 = gg_function_68(t38, t39, base + 0);
  t41 = fg_w2_set_erase(t33, t40);
  v9 = ((int64_t)(t41));
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(v8));
  t44 = gg_function_60(t42, t43, base + 0);
  t45 = ((int64_t)(t44));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  t48 = ((int64_t)(v5));
  t49 = ((int64_t)(v8));
  gg_function_12(t48, t49, base + 0);
  t50 = ((int64_t)(v5));
  t51 = ((int64_t)(((int64_t)UINT64_C(20))));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = fg_w2_get(t50, t51, t52);
  t54 = ((int64_t)(t53));
  t55 = ((int64_t)(v8));
  t56 = fg_w2_set_erase(t54, t55);
  v10 = ((int64_t)(t56));
  t57 = ((int64_t)(v7));
  t58 = ((int64_t)(v6));
  t59 = ((t57) == (t58));
  if (t59) {
  {
  v7 = ((int64_t)(v8));
  }
  }
  }
  }
  }
  }
  t60 = v8;
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t60) + (uint64_t)(t61)));
  }
  goto label1;
label2:;
  t62 = ((int64_t)(v7));
  t63 = ((int64_t)(v6));
  t64 = ((t62) != (t63));
  if (t64) {
  {
  t65 = ((int64_t)(v0));
  t66 = ((int64_t)(v4));
  v11 = gg_function_34(t65, t66, base + 0);
  t67 = ((int64_t)(v7));
  t68 = ((int64_t)(v11));
  t69 = ((t67) < (t68));
  if (t69) {
  {
  t70 = ((int64_t)(v0));
  t71 = ((int64_t)(v4));
  t72 = ((int64_t)(v7));
  gg_function_19(t70, t71, t72, base + 0);
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_9(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  v1 = gg_function_10(t0, base + 0);
  t2 = v1;
  t1 = 0;
label0:;
  if (t1 >= t2) goto label2;
  v2 = t1;
  {
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v2));
  gg_function_12(t3, t4, base + 0);
  }
label1:;
  t1++; goto label0;
label2:;
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_w2_put(t5, t6, t7, t8);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(20))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg_w2_get(t9, t10, t11);
  t13 = ((int64_t)(t12));
  fg_w2_set_destroy(t13);
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(20))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg_w2_set_create();
  t18 = ((int64_t)(t17));
  fg_w2_put(t14, t15, t16, t18);
  v3 = gg_function_15(base + 0);
  t20 = v3;
  t19 = 0;
label3:;
  if (t19 >= t20) goto label5;
  v4 = t19;
  {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(25))));
  t23 = ((int64_t)(v4));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg_w2_get(t21, t24, t25);
  t27 = ((int64_t)(t26));
  fg_w2_set_destroy(t27);
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(25))));
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = fg_w2_set_create();
  t34 = ((int64_t)(t33));
  fg_w2_put(t28, t31, t32, t34);
  }
label4:;
  t19++; goto label3;
label5:;
  }
gg_done:;
}
static int64_t gg_function_22(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_kv_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_158(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t29;
  int64_t t30;
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
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)UINT64_C(1));
  t2 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t1)))));
  t3 = ((t0) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v0));
  v4 = gg_function_22(t4, base + 0);
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v4));
  if (!(((t5) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v5));
  t9 = gg_function_6(t7, t8, base + 0);
  gg_function_9(t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_19(t10, t11, t12, base + 0);
  t13 = v5;
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  }
  goto label1;
label2:;
  t15 = ((int64_t)(v0));
  fg_kv_clear_data(t15);
  }
  } else {
  {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)UINT64_C(1));
  t19 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t18)))));
  t20 = ((int64_t)UINT64_C(1));
  t21 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t20)))));
  gg_function_82(t16, t17, t19, t21, base + 0);
  t22 = ((int64_t)(v3));
  t23 = gg_function_56(t22, base + 0);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) > (t25));
  if (t26) {
  {
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(v2));
  t29 = ((int64_t)(v3));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_zero_cells(t27, t28, t29, t30);
  }
  }
  }
  }
  }
gg_done:;
}
static int64_t gg_function_58(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(100))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = ((int64_t)(v2));
  t6 = fg_kv_get(t1, t4, t5);
  t0 = ((int64_t)(t6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_154(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  int64_t t21;
  uint8_t t22;
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
  uint8_t t33;
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
  uint8_t t54;
  uint8_t t55;
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
  int64_t t137;
  int64_t t138;
  uint8_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  uint8_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  uint8_t t157;
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
  uint8_t t168;
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
  uint8_t t212;
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
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  uint8_t t235;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v2));
  v5 = gg_function_6(t1, t2, base + 0);
  t3 = ((int64_t)(v1));
  t4 = fg153_i64_fg_state_read_u32(t3);
  v6 = ((int64_t)(t4));
  t5 = ((int64_t)(v1));
  t6 = fg153_i64_fg_state_read_u32(t5);
  v7 = ((int64_t)(t6));
  t7 = ((int64_t)(v1));
  t8 = fg153_i64_fg_state_read_u32(t7);
  v8 = ((int64_t)(t8));
  t9 = ((int64_t)(v1));
  t10 = fg153_i64_fg_state_read_u32(t9);
  v9 = ((int64_t)(t10));
  t11 = ((int64_t)(v7));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(7))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = fg_kv_get(t12, t13, t14);
  t16 = ((int64_t)(t15));
  t17 = ((t11) != (t16));
  if (t17) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(v5));
  t20 = gg_function_10(t19, base + 0);
  t21 = ((int64_t)(t20));
  t22 = ((t18) > (t21));
  if (t22) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(v0));
  t25 = fg_state_v_trans(t24);
  t26 = ((int64_t)(t25));
  t27 = ((t23) != (t26));
  if (t27) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(v8));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg_state_n_rot(t29, t30);
  t32 = ((int64_t)(t31));
  t33 = ((t28) != (t32));
  if (t33) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t34 = ((int64_t)(v9));
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  t37 = fg_state_n_rot(t35, t36);
  t38 = ((int64_t)(t37));
  t39 = ((t34) != (t38));
  if (t39) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t40 = ((int64_t)(((int64_t)UINT64_C(4))));
  t41 = fg_kv_obj(t40);
  v10 = ((int64_t)(t41));
  t42 = ((int64_t)(v3));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = ((t42) > (t43));
  if (t44) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t45 = ((int64_t)(v11));
  t46 = ((int64_t)(v3));
  if (!(((t45) < (t46)))) goto label2;
  {
  t47 = ((int64_t)(v4));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(v11));
  v12 = gg_function_58(t47, t48, t49, base + 0);
  t50 = ((int64_t)(v11));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t51)))));
label4:;
  t52 = ((int64_t)(v13));
  t53 = ((int64_t)(v3));
  t54 = ((t52) < (t53));
  t55 = t54;
  if (t55) {
  t56 = ((int64_t)(v4));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((int64_t)(v13));
  t59 = gg_function_58(t56, t57, t58, base + 0);
  t60 = ((int64_t)(t59));
  t61 = ((int64_t)(v4));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((int64_t)(v13));
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) - (uint64_t)(t64)))));
  t66 = gg_function_58(t61, t62, t65, base + 0);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  t55 = ((t60) == (t69));
  }
  if (!(t55)) goto label5;
  {
  t70 = v13;
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)((uint64_t)(t70) + (uint64_t)(t71)));
  }
  goto label4;
label5:;
  t72 = ((int64_t)(v10));
  t73 = ((int64_t)(v12));
  fg_kv_vec_push(t72, t73);
  t74 = ((int64_t)(v10));
  t75 = ((int64_t)(v4));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = ((int64_t)(v13));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) - (uint64_t)(t78)))));
  t80 = gg_function_58(t75, t76, t79, base + 0);
  t81 = ((int64_t)(t80));
  t82 = ((int64_t)(((int64_t)UINT64_C(1))));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t81) + (uint64_t)(t82)))));
  fg_kv_vec_push(t74, t83);
  v11 = ((int64_t)(v13));
  }
  goto label1;
label2:;
  }
  }
  t84 = ((int64_t)(v10));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = fg_kv_get(t84, t85, t86);
  v14 = ((int64_t)(t87));
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t88 = ((int64_t)(v15));
  t89 = ((int64_t)(v7));
  if (!(((t88) < (t89)))) goto label8;
  {
  t90 = ((int64_t)(v1));
  t91 = fg153_i64_fg_state_read_i32(t90);
  v16 = ((int64_t)(t91));
  t92 = ((int64_t)(v16));
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(v2));
  t95 = ((int64_t)(v15));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = fg_state_tensor_type(t93, t94, t95, t96);
  t98 = ((int64_t)(t97));
  t99 = ((t92) != (t98));
  if (t99) {
  {
  t100 = ((int64_t)(v10));
  fg_kv_destroy(t100);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t101 = ((int64_t)(v1));
  t102 = fg153_i64_fg_state_read_u64(t101);
  v17 = ((int64_t)(t102));
  t103 = ((int64_t)(v17));
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(v2));
  t106 = ((int64_t)(v15));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = fg_state_tensor_row_size(t104, t105, t106, t107);
  t109 = ((int64_t)(t108));
  t110 = ((t103) != (t109));
  if (t110) {
  {
  t111 = ((int64_t)(v10));
  fg_kv_destroy(t111);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t112 = ((int64_t)(v18));
  t113 = ((int64_t)(v14));
  if (!(((t112) < (t113)))) goto label11;
  {
  t114 = ((int64_t)(v0));
  t115 = ((int64_t)(v1));
  t116 = ((int64_t)(v2));
  t117 = ((int64_t)(v15));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((int64_t)(v10));
  t120 = ((int64_t)(((int64_t)UINT64_C(10))));
  t121 = ((int64_t)(v18));
  t122 = ((int64_t)(((int64_t)((uint64_t)(t120) + (uint64_t)(t121)))));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = fg_kv_get(t119, t122, t123);
  t125 = ((int64_t)(t124));
  t126 = ((int64_t)(v10));
  t127 = ((int64_t)(((int64_t)UINT64_C(11))));
  t128 = ((int64_t)(v18));
  t129 = ((int64_t)(((int64_t)((uint64_t)(t127) + (uint64_t)(t128)))));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = fg_kv_get(t126, t129, t130);
  t132 = ((int64_t)(t131));
  fg153_i64_fg_state_read_rows(t114, t115, t116, t117, t118, t125, t132);
  t133 = v18;
  t134 = ((int64_t)(((int64_t)UINT64_C(2))));
  v18 = ((int64_t)((uint64_t)(t133) + (uint64_t)(t134)));
  }
  goto label10;
label11:;
  t135 = v15;
  t136 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t135) + (uint64_t)(t136)));
  }
  goto label7;
label8:;
  t137 = ((int64_t)(v6));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((t137) == (t138));
  if (t139) {
  {
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t140 = ((int64_t)(v15));
  t141 = ((int64_t)(v7));
  if (!(((t140) < (t141)))) goto label14;
  {
  t142 = ((int64_t)(v0));
  t143 = ((int64_t)(v15));
  t144 = fg_state_layer_has_v(t142, t143);
  t145 = ((int64_t)(t144));
  t146 = ((int64_t)(((int64_t)UINT64_C(0))));
  t147 = ((t145) != (t146));
  if (t147) {
  {
  t148 = ((int64_t)(v1));
  t149 = fg153_i64_fg_state_read_i32(t148);
  v19 = ((int64_t)(t149));
  t150 = ((int64_t)(v19));
  t151 = ((int64_t)(v0));
  t152 = ((int64_t)(v2));
  t153 = ((int64_t)(v15));
  t154 = ((int64_t)(((int64_t)UINT64_C(1))));
  t155 = fg_state_tensor_type(t151, t152, t153, t154);
  t156 = ((int64_t)(t155));
  t157 = ((t150) != (t156));
  if (t157) {
  {
  t158 = ((int64_t)(v10));
  fg_kv_destroy(t158);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t159 = ((int64_t)(v1));
  t160 = fg153_i64_fg_state_read_u64(t159);
  v20 = ((int64_t)(t160));
  t161 = ((int64_t)(v20));
  t162 = ((int64_t)(v0));
  t163 = ((int64_t)(v2));
  t164 = ((int64_t)(v15));
  t165 = ((int64_t)(((int64_t)UINT64_C(1))));
  t166 = fg_state_tensor_row_size(t162, t163, t164, t165);
  t167 = ((int64_t)(t166));
  t168 = ((t161) != (t167));
  if (t168) {
  {
  t169 = ((int64_t)(v10));
  fg_kv_destroy(t169);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t170 = ((int64_t)(v21));
  t171 = ((int64_t)(v14));
  if (!(((t170) < (t171)))) goto label17;
  {
  t172 = ((int64_t)(v0));
  t173 = ((int64_t)(v1));
  t174 = ((int64_t)(v2));
  t175 = ((int64_t)(v15));
  t176 = ((int64_t)(((int64_t)UINT64_C(1))));
  t177 = ((int64_t)(v10));
  t178 = ((int64_t)(((int64_t)UINT64_C(10))));
  t179 = ((int64_t)(v21));
  t180 = ((int64_t)(((int64_t)((uint64_t)(t178) + (uint64_t)(t179)))));
  t181 = ((int64_t)(((int64_t)UINT64_C(0))));
  t182 = fg_kv_get(t177, t180, t181);
  t183 = ((int64_t)(t182));
  t184 = ((int64_t)(v10));
  t185 = ((int64_t)(((int64_t)UINT64_C(11))));
  t186 = ((int64_t)(v21));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t185) + (uint64_t)(t186)))));
  t188 = ((int64_t)(((int64_t)UINT64_C(0))));
  t189 = fg_kv_get(t184, t187, t188);
  t190 = ((int64_t)(t189));
  fg153_i64_fg_state_read_rows(t172, t173, t174, t175, t176, t183, t190);
  t191 = v21;
  t192 = ((int64_t)(((int64_t)UINT64_C(2))));
  v21 = ((int64_t)((uint64_t)(t191) + (uint64_t)(t192)));
  }
  goto label16;
label17:;
  }
  }
  t193 = v15;
  t194 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t193) + (uint64_t)(t194)));
  }
  goto label13;
label14:;
  }
  } else {
  {
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t195 = ((int64_t)(v15));
  t196 = ((int64_t)(v7));
  if (!(((t195) < (t196)))) goto label20;
  {
  t197 = ((int64_t)(v0));
  t198 = ((int64_t)(v15));
  t199 = fg_state_layer_has_v(t197, t198);
  t200 = ((int64_t)(t199));
  t201 = ((int64_t)(((int64_t)UINT64_C(0))));
  t202 = ((t200) != (t201));
  if (t202) {
  {
  t203 = ((int64_t)(v1));
  t204 = fg153_i64_fg_state_read_i32(t203);
  v22 = ((int64_t)(t204));
  t205 = ((int64_t)(v22));
  t206 = ((int64_t)(v0));
  t207 = ((int64_t)(v2));
  t208 = ((int64_t)(v15));
  t209 = ((int64_t)(((int64_t)UINT64_C(1))));
  t210 = fg_state_tensor_type(t206, t207, t208, t209);
  t211 = ((int64_t)(t210));
  t212 = ((t205) != (t211));
  if (t212) {
  {
  t213 = ((int64_t)(v10));
  fg_kv_destroy(t213);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t214 = ((int64_t)(v1));
  t215 = fg153_i64_fg_state_read_u32(t214);
  v23 = ((int64_t)(t215));
  t216 = ((int64_t)(v23));
  t217 = ((int64_t)(v0));
  t218 = ((int64_t)(v2));
  t219 = ((int64_t)(v15));
  t220 = ((int64_t)(((int64_t)UINT64_C(1))));
  t221 = fg_state_tensor_size_el(t217, t218, t219, t220);
  t222 = ((int64_t)(t221));
  t223 = ((t216) != (t222));
  if (t223) {
  {
  t224 = ((int64_t)(v10));
  fg_kv_destroy(t224);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t225 = ((int64_t)(v1));
  t226 = fg153_i64_fg_state_read_u32(t225);
  v24 = ((int64_t)(t226));
  t227 = ((int64_t)(v0));
  t228 = ((int64_t)(v15));
  t229 = fg_state_layer_il(t227, t228);
  v25 = ((int64_t)(t229));
  t230 = ((int64_t)(v24));
  t231 = ((int64_t)(v0));
  t232 = ((int64_t)(v25));
  t233 = fg_state_n_embd_v(t231, t232);
  t234 = ((int64_t)(t233));
  t235 = ((t230) != (t234));
  if (t235) {
  {
  t236 = ((int64_t)(v10));
  fg_kv_destroy(t236);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t237 = ((int64_t)(v26));
  t238 = ((int64_t)(v24));
  if (!(((t237) < (t238)))) goto label23;
  {
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
label25:;
  t239 = ((int64_t)(v27));
  t240 = ((int64_t)(v14));
  if (!(((t239) < (t240)))) goto label26;
  {
  t241 = ((int64_t)(v0));
  t242 = ((int64_t)(v1));
  t243 = ((int64_t)(v2));
  t244 = ((int64_t)(v15));
  t245 = ((int64_t)(v26));
  t246 = ((int64_t)(v10));
  t247 = ((int64_t)(((int64_t)UINT64_C(10))));
  t248 = ((int64_t)(v27));
  t249 = ((int64_t)(((int64_t)((uint64_t)(t247) + (uint64_t)(t248)))));
  t250 = ((int64_t)(((int64_t)UINT64_C(0))));
  t251 = fg_kv_get(t246, t249, t250);
  t252 = ((int64_t)(t251));
  t253 = ((int64_t)(v10));
  t254 = ((int64_t)(((int64_t)UINT64_C(11))));
  t255 = ((int64_t)(v27));
  t256 = ((int64_t)(((int64_t)((uint64_t)(t254) + (uint64_t)(t255)))));
  t257 = ((int64_t)(((int64_t)UINT64_C(0))));
  t258 = fg_kv_get(t253, t256, t257);
  t259 = ((int64_t)(t258));
  fg153_i64_fg_state_read_trans(t241, t242, t243, t244, t245, t252, t259);
  t260 = v27;
  t261 = ((int64_t)(((int64_t)UINT64_C(2))));
  v27 = ((int64_t)((uint64_t)(t260) + (uint64_t)(t261)));
  }
  goto label25;
label26:;
  t262 = v26;
  t263 = ((int64_t)(((int64_t)UINT64_C(1))));
  v26 = ((int64_t)((uint64_t)(t262) + (uint64_t)(t263)));
  }
  goto label22;
label23:;
  }
  }
  t264 = v15;
  t265 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t264) + (uint64_t)(t265)));
  }
  goto label19;
label20:;
  }
  }
  t266 = ((int64_t)(v10));
  fg_kv_destroy(t266);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_65(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  gg_function_38(t0, t1, base + 0);
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(12))));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  fg_w2_put(t2, t3, t4, t5);
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(13))));
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(v3));
  fg_w2_put(t6, t7, t8, t9);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(14))));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v4));
  fg_w2_put(t10, t11, t12, t13);
  }
gg_done:;
}
static int64_t gg_function_54(int64_t p0, int64_t p1, int64_t base) {
  /* FG_SHARED_BODY:6 */
  return gg_function_6(p0, p1, base);
}
static int64_t gg_function_80(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(25))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg_w2_get(t1, t4, t5);
  v2 = ((int64_t)(t6));
  t7 = ((int64_t)(v2));
  t8 = fg_w2_set_size(t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t12)))));
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(v2));
  t15 = fg_w2_set_first(t14);
  t16 = ((int64_t)(t15));
  t17 = fg_w2_set_key(t13, t16);
  v3 = ((int64_t)(t17));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(4294967296))));
  t0 = ((int64_t)(((int64_t)gg_div(t18, t19))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_67(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  uint8_t t5;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  gg_function_38(t0, t1, base + 0);
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) >= (t3));
  t5 = t4;
  if (t5) {
  t6 = ((int64_t)(v2));
  t7 = gg_function_15(base + 0);
  t8 = ((int64_t)(t7));
  t5 = ((t6) < (t8));
  }
  t9 = t5;
  gg_function_4(t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(10))));
  t12 = ((int64_t)(v1));
  t13 = fg_w2_get(t10, t11, t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)UINT64_C(1));
  t16 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t15)))));
  t17 = ((t14) != (t16));
  gg_function_4(t17, base + 0);
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(v2));
  t21 = gg_function_41(t18, t19, t20, base + 0);
  t22 = ((int64_t)(t21));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t22) == (t23));
  gg_function_4(t24, base + 0);
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_16(t25, t26, t27, t28, base + 0);
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(25))));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = fg_w2_get(t29, t32, t33);
  t35 = ((int64_t)(t34));
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(10))));
  t38 = ((int64_t)(v1));
  t39 = fg_w2_get(t36, t37, t38);
  t40 = ((int64_t)(t39));
  t41 = ((int64_t)(v1));
  t42 = gg_function_68(t40, t41, base + 0);
  t43 = fg_w2_set_insert(t35, t42);
  v3 = ((int64_t)(t43));
  }
gg_done:;
}
static void gg_function_59(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  gg_function_38(t0, t1, base + 0);
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  t4 = ((int64_t)(v1));
  t5 = fg_w2_get(t2, t3, t4);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)UINT64_C(1));
  t8 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t7)))));
  t9 = ((t6) == (t8));
  gg_function_4(t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(v1));
  t12 = gg_function_60(t10, t11, base + 0);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) == (t14));
  gg_function_4(t15, base + 0);
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(10))));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(v2));
  fg_w2_put(t16, t17, t18, t19);
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(20))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg_w2_get(t20, t21, t22);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(v1));
  t26 = fg_w2_set_insert(t24, t25);
  v3 = ((int64_t)(t26));
  }
gg_done:;
}
static void gg_function_72(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  {
  v2 = gg_function_15(base + 0);
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(10))));
  t2 = ((int64_t)(v1));
  t3 = fg_w2_get(t0, t1, t2);
  v3 = ((int64_t)(t3));
  t5 = v2;
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v4 = t4;
  {
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v4));
  t9 = gg_function_41(t6, t7, t8, base + 0);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t10) != (t11));
  if (t12) {
  {
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(25))));
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg_w2_get(t13, t16, t17);
  t19 = ((int64_t)(t18));
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(v1));
  t22 = gg_function_68(t20, t21, base + 0);
  t23 = fg_w2_set_erase(t19, t22);
  v5 = ((int64_t)(t23));
  }
  }
  }
label1:;
  t4++; goto label0;
label2:;
  }
gg_done:;
}
static void gg_function_71(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  gg_function_38(t0, t1, base + 0);
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  t4 = ((int64_t)(v1));
  t5 = fg_w2_get(t2, t3, t4);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)UINT64_C(1));
  t8 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t7)))));
  t9 = ((t6) != (t8));
  gg_function_4(t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(v1));
  gg_function_72(t10, t11, base + 0);
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(v1));
  gg_function_12(t12, t13, base + 0);
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(20))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg_w2_get(t14, t15, t16);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(v1));
  t20 = fg_w2_set_erase(t18, t19);
  v2 = ((int64_t)(t20));
  }
gg_done:;
}
static int64_t gg_function_42(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  gg_function_38(t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(10))));
  t5 = ((int64_t)(v1));
  t6 = fg_w2_get(t3, t4, t5);
  v2 = ((int64_t)(t6));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)UINT64_C(1));
  t9 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t8)))));
  t10 = ((t7) != (t9));
  gg_function_4(t10, base + 0);
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_40(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  v2 = gg_function_15(base + 0);
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = v2;
  t1 = 0;
label0:;
  if (t1 >= t2) goto label2;
  v4 = t1;
  {
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v4));
  t6 = gg_function_41(t3, t4, t5, base + 0);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) != (t8));
  if (t9) {
  {
  t10 = v3;
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t10) + (uint64_t)(t11)));
  }
  }
  }
label1:;
  t1++; goto label0;
label2:;
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_43(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t17;
  int64_t t18;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  gg_function_38(t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = gg_function_40(t3, t4, base + 0);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((t6) == (t7));
  gg_function_4(t8, base + 0);
  v2 = gg_function_15(base + 0);
  t10 = v2;
  t9 = 0;
label0:;
  if (t9 >= t10) goto label2;
  v3 = t9;
  {
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v3));
  t14 = gg_function_41(t11, t12, t13, base + 0);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) != (t16));
  if (t17) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  }
label1:;
  t9++; goto label0;
label2:;
  t18 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t18)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_39(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  gg_function_38(t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(10))));
  t5 = ((int64_t)(v1));
  t6 = fg_w2_get(t3, t4, t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)UINT64_C(1));
  t9 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t8)))));
  t10 = ((t7) != (t9));
  gg_function_4(t10, base + 0);
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(v1));
  t0 = gg_function_40(t11, t12, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_37(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  gg_function_38(t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(10))));
  t5 = ((int64_t)(v1));
  t6 = fg_w2_get(t3, t4, t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)UINT64_C(1));
  t9 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t8)))));
  t10 = ((t7) == (t9));
  if (t10) {
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
static int64_t gg_function_53(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_kv_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_70(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t48;
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
  uint8_t t92;
  uint8_t t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
  uint8_t t97;
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
  uint8_t t122;
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
  uint8_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
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
  uint8_t t185;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(62))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg_kv_get(t0, t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  if (t6) {
  {
  goto gg_done;
  }
  }
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  t8 = fg_kv_obj(t7);
  v3 = ((int64_t)(t8));
  v4 = gg_function_15(base + 0);
  t10 = v4;
  t9 = 0;
label0:;
  if (t9 >= t10) goto label2;
  v5 = t9;
  {
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)UINT64_C(1000))));
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)UINT64_C(1));
  t17 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t16)))));
  fg_kv_put(t11, t14, t15, t17);
  }
label1:;
  t9++; goto label0;
label2:;
  t18 = ((int64_t)(v0));
  v6 = gg_function_22(t18, base + 0);
  t19 = ((int64_t)(v1));
  v7 = gg_function_53(t19, base + 0);
  t20 = ((int64_t)(v1));
  v8 = gg_function_56(t20, base + 0);
  t21 = ((int64_t)(v2));
  t22 = fg_kv_ub_n_tokens(t21);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(v7));
  t25 = ((int64_t)(v8));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((t23) == (t26));
  gg_function_4(t27, base + 0);
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t28 = ((int64_t)(v9));
  t29 = ((int64_t)(v7));
  if (!(((t28) < (t29)))) goto label5;
  {
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(v9));
  v10 = gg_function_6(t30, t31, base + 0);
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(v10));
  v11 = gg_function_6(t32, t33, base + 0);
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t34 = ((int64_t)(v12));
  t35 = ((int64_t)(v8));
  if (!(((t34) < (t35)))) goto label8;
  {
  t36 = ((int64_t)(v9));
  t37 = ((int64_t)(v8));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int64_t)(v12));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(v9));
  t42 = ((int64_t)(v12));
  v14 = gg_function_58(t40, t41, t42, base + 0);
  t43 = ((int64_t)(v11));
  t44 = ((int64_t)(v14));
  t45 = gg_function_37(t43, t44, base + 0);
  t46 = ((int64_t)(t45));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((t46) == (t47));
  if (t48) {
  {
  t49 = ((int64_t)(v11));
  t50 = ((int64_t)(v14));
  t51 = gg_function_39(t49, t50, base + 0);
  t52 = ((int64_t)(t51));
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  t54 = ((t52) == (t53));
  gg_function_4(t54, base + 0);
  t55 = ((int64_t)(v11));
  t56 = ((int64_t)(v14));
  v15 = gg_function_43(t55, t56, base + 0);
  t57 = ((int64_t)(v11));
  t58 = ((int64_t)(v14));
  v16 = gg_function_42(t57, t58, base + 0);
  t59 = ((int64_t)(v16));
  t60 = ((int64_t)(v3));
  t61 = ((int64_t)(((int64_t)UINT64_C(1000))));
  t62 = ((int64_t)(v15));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) + (uint64_t)(t62)))));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = fg_kv_get(t60, t63, t64);
  t66 = ((int64_t)(t65));
  t67 = ((t59) > (t66));
  if (t67) {
  {
  t68 = ((int64_t)(v3));
  t69 = ((int64_t)(((int64_t)UINT64_C(1000))));
  t70 = ((int64_t)(v15));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) + (uint64_t)(t70)))));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((int64_t)(v16));
  fg_kv_put(t68, t71, t72, t73);
  }
  }
  t74 = ((int64_t)(v11));
  t75 = ((int64_t)(v14));
  gg_function_71(t74, t75, base + 0);
  }
  }
  t76 = ((int64_t)(v11));
  t77 = ((int64_t)(v14));
  t78 = ((int64_t)(v2));
  t79 = ((int64_t)(v13));
  t80 = fg_kv_ub_pos(t78, t79);
  t81 = ((int64_t)(t80));
  gg_function_59(t76, t77, t81, base + 0);
  t82 = ((int64_t)(v2));
  t83 = fg_kv_ub_has_token(t82);
  v17 = ((int64_t)(t83));
  t84 = ((int64_t)(v2));
  t85 = fg_kv_ub_is_pos_2d(t84);
  v18 = ((int64_t)(t85));
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(8))));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = fg_kv_get(t86, t87, t88);
  v19 = ((int64_t)(t89));
  t90 = ((int64_t)(v18));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((t90) != (t91));
  t93 = t92;
  if (!t93) {
  t94 = ((int64_t)(v17));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t94) != (t95));
  }
  t96 = t93;
  t97 = t96;
  if (!t97) {
  t98 = ((int64_t)(v19));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = ((t98) > (t99));
  }
  t100 = t97;
  if (t100) {
  {
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
  v22 = gg_function_13(base + 0);
  t101 = ((int64_t)(v18));
  t102 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = ((t101) != (t102));
  if (t103) {
  {
  t104 = ((int64_t)(v2));
  t105 = ((int64_t)(v13));
  t106 = ((int64_t)(v2));
  t107 = fg_kv_ub_n_tokens(t106);
  t108 = ((int64_t)(t107));
  t109 = ((int64_t)(((int64_t)UINT64_C(2))));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t105) + (uint64_t)(t110)))));
  t112 = fg_kv_ub_pos(t104, t111);
  v20 = ((int64_t)(t112));
  t113 = ((int64_t)(v2));
  t114 = ((int64_t)(v13));
  t115 = ((int64_t)(v2));
  t116 = fg_kv_ub_n_tokens(t115);
  t117 = ((int64_t)(t116));
  t118 = ((int64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t117)))));
  t119 = fg_kv_ub_pos(t113, t118);
  v21 = ((int64_t)(t119));
  }
  }
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((int64_t)(v17));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = ((t120) == (t121));
  if (t122) {
  {
  v23 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t123 = ((int64_t)(v2));
  t124 = fg_kv_ub_is_mixed(t123);
  t125 = ((int64_t)(t124));
  t126 = ((int64_t)(((int64_t)UINT64_C(0))));
  t127 = ((t125) != (t126));
  if (t127) {
  {
  t128 = ((int64_t)(v2));
  t129 = ((int64_t)(v13));
  t130 = fg_kv_ub_type(t128, t129);
  v23 = ((int64_t)(t130));
  }
  }
  }
  }
  t131 = ((int64_t)(v23));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t133 = ((t131) == (t132));
  if (t133) {
  {
  t134 = ((int64_t)(v2));
  t135 = ((int64_t)(v13));
  t136 = fg_kv_ub_token(t134, t135);
  v22 = ((int64_t)(t136));
  }
  } else {
  {
  t137 = ((int64_t)(v19));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((t137) > (t138));
  if (t139) {
  {
  t140 = ((int64_t)(v0));
  t141 = ((int64_t)(((int64_t)UINT64_C(9))));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = fg_kv_get(t140, t141, t142);
  v24 = ((int64_t)(t143));
  t144 = ((int64_t)(v24));
  t145 = ((int64_t)(((int64_t)UINT64_C(0))));
  t146 = ((t144) != (t145));
  if (t146) {
  {
  v22 = ((int64_t)(v24));
  }
  } else {
  {
  t147 = ((int64_t)(v0));
  t148 = ((int64_t)(((int64_t)UINT64_C(60))));
  t149 = ((int64_t)(((int64_t)UINT64_C(0))));
  t150 = fg_kv_get(t147, t148, t149);
  v22 = ((int64_t)(t150));
  }
  }
  }
  }
  }
  }
  t151 = ((int64_t)(v11));
  t152 = ((int64_t)(v14));
  t153 = ((int64_t)(v20));
  t154 = ((int64_t)(v21));
  t155 = ((int64_t)(v22));
  gg_function_65(t151, t152, t153, t154, t155, base + 0);
  }
  }
  t156 = ((int64_t)(v2));
  t157 = ((int64_t)(v13));
  t158 = fg_kv_ub_n_seq_id(t156, t157);
  v25 = ((int64_t)(t158));
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t159 = ((int64_t)(v26));
  t160 = ((int64_t)(v25));
  if (!(((t159) < (t160)))) goto label11;
  {
  t161 = ((int64_t)(v11));
  t162 = ((int64_t)(v14));
  t163 = ((int64_t)(v2));
  t164 = ((int64_t)(v13));
  t165 = ((int64_t)(v26));
  t166 = fg_kv_ub_seq_id(t163, t164, t165);
  t167 = ((int64_t)(t166));
  gg_function_67(t161, t162, t167, base + 0);
  t168 = v26;
  t169 = ((int64_t)(((int64_t)UINT64_C(1))));
  v26 = ((int64_t)((uint64_t)(t168) + (uint64_t)(t169)));
  }
  goto label10;
label11:;
  t170 = v12;
  t171 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t170) + (uint64_t)(t171)));
  }
  goto label7;
label8:;
  t172 = v9;
  t173 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t172) + (uint64_t)(t173)));
  }
  goto label4;
label5:;
  v27 = gg_function_15(base + 0);
  t175 = v27;
  t174 = 0;
label12:;
  if (t174 >= t175) goto label14;
  v28 = t174;
  {
  t176 = ((int64_t)(v3));
  t177 = ((int64_t)(((int64_t)UINT64_C(1000))));
  t178 = ((int64_t)(v28));
  t179 = ((int64_t)(((int64_t)((uint64_t)(t177) + (uint64_t)(t178)))));
  t180 = ((int64_t)(((int64_t)UINT64_C(0))));
  t181 = fg_kv_get(t176, t179, t180);
  v29 = ((int64_t)(t181));
  t182 = ((int64_t)(v29));
  t183 = ((int64_t)UINT64_C(1));
  t184 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t183)))));
  t185 = ((t182) != (t184));
  if (t185) {
  {
  t186 = ((int64_t)(v0));
  t187 = ((int64_t)(v28));
  v30 = gg_function_33(t186, t187, base + 0);
  t188 = ((int64_t)(v0));
  t189 = ((int64_t)(v30));
  v31 = gg_function_6(t188, t189, base + 0);
  t190 = ((int64_t)(v31));
  t191 = ((int64_t)(v28));
  v32 = gg_function_80(t190, t191, base + 0);
  t192 = ((int64_t)(v32));
  t193 = ((int64_t)(v29));
  t194 = ((t192) <= (t193));
  if (t194) {
  {
  t195 = ((int64_t)(v0));
  t196 = ((int64_t)(v28));
  t197 = ((int64_t)(v32));
  t198 = ((int64_t)(v29));
  t199 = ((int64_t)(((int64_t)UINT64_C(1))));
  t200 = ((int64_t)(((int64_t)((uint64_t)(t198) + (uint64_t)(t199)))));
  gg_function_82(t195, t196, t197, t200, base + 0);
  }
  }
  }
  }
  }
label13:;
  t174++; goto label12;
label14:;
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t201 = ((int64_t)(v9));
  t202 = ((int64_t)(v7));
  if (!(((t201) < (t202)))) goto label17;
  {
  t203 = ((int64_t)(v1));
  t204 = ((int64_t)(v9));
  v33 = gg_function_6(t203, t204, base + 0);
  t205 = ((int64_t)(v1));
  t206 = ((int64_t)(v9));
  t207 = fg_kv_slot_len(t205, t206);
  v34 = ((int64_t)(t207));
  t208 = ((int64_t)(v0));
  t209 = ((int64_t)(v33));
  t210 = ((int64_t)(v1));
  t211 = ((int64_t)(v9));
  t212 = ((int64_t)(v34));
  t213 = ((int64_t)(((int64_t)UINT64_C(1))));
  t214 = ((int64_t)(((int64_t)((uint64_t)(t212) - (uint64_t)(t213)))));
  t215 = gg_function_58(t210, t211, t214, base + 0);
  t216 = ((int64_t)(t215));
  t217 = ((int64_t)(((int64_t)UINT64_C(1))));
  t218 = ((int64_t)(((int64_t)((uint64_t)(t216) + (uint64_t)(t217)))));
  gg_function_19(t208, t209, t218, base + 0);
  t219 = v9;
  t220 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t219) + (uint64_t)(t220)));
  }
  goto label16;
label17:;
  t221 = ((int64_t)(v3));
  fg_kv_destroy(t221);
  }
gg_done:;
}
static int64_t gg_function_44(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(25))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg_w2_get(t1, t4, t5);
  v2 = ((int64_t)(t6));
  t7 = ((int64_t)(v2));
  t8 = fg_w2_set_size(t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t12)))));
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(v2));
  t15 = fg_w2_set_last(t14);
  t16 = ((int64_t)(t15));
  t17 = fg_w2_set_key(t13, t16);
  v3 = ((int64_t)(t17));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(4294967296))));
  t0 = ((int64_t)(((int64_t)gg_div(t18, t19))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_35(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(20))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_w2_get(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = fg_w2_set_size(t5);
  t0 = ((int64_t)(t6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t52;
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
  uint8_t t70;
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
  uint8_t t81;
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
  uint8_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
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
  uint8_t t136;
  uint8_t t137;
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
  uint8_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  uint8_t t157;
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
  uint8_t t175;
  int64_t t176;
  int64_t t177;
  uint8_t t178;
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
  uint8_t t192;
  int64_t t193;
  int64_t t194;
  uint8_t t195;
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
  uint8_t t207;
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
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  v3 = gg_function_22(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = fg_kv_ub_n_tokens(t2);
  v4 = ((int64_t)(t3));
  v5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((t4) > (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v1));
  t8 = fg_kv_ub_n_seqs_unq(t7);
  v6 = ((int64_t)(t8));
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) > (t10));
  gg_function_4(t11, base + 0);
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(v6));
  t14 = ((int64_t)(((int64_t)gg_mod(t12, t13))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) == (t15));
  gg_function_4(t16, base + 0);
  v5 = ((int64_t)(v6));
  t17 = ((int64_t)(v4));
  t18 = ((int64_t)(v6));
  v4 = ((int64_t)(((int64_t)gg_div(t17, t18))));
  }
  }
  t19 = ((int64_t)(((int64_t)UINT64_C(3))));
  t20 = fg_kv_obj(t19);
  v7 = ((int64_t)(t20));
  t21 = ((int64_t)(v7));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  fg_kv_put(t21, t22, t23, t24);
  t25 = ((int64_t)(v7));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_put(t25, t26, t27, t28);
  t29 = ((int64_t)(v7));
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((int64_t)(v4));
  fg_kv_put(t29, t30, t31, t32);
  t33 = ((int64_t)(v7));
  t34 = ((int64_t)(((int64_t)UINT64_C(3))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((int64_t)(v5));
  fg_kv_put(t33, t34, t35, t36);
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t37 = ((int64_t)(v8));
  t38 = ((int64_t)(v5));
  if (!(((t37) < (t38)))) goto label2;
  {
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(v8));
  t41 = fg_kv_ub_seq_id_unq(t39, t40);
  v9 = ((int64_t)(t41));
  t42 = ((int64_t)(v3));
  t43 = ((int64_t)(((int64_t)UINT64_C(1))));
  t44 = ((t42) > (t43));
  if (t44) {
  {
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(v8));
  t47 = ((int64_t)(v4));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = fg_kv_ub_n_seq_id(t45, t48);
  t50 = ((int64_t)(t49));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  t52 = ((t50) == (t51));
  gg_function_4(t52, base + 0);
  t53 = ((int64_t)(v1));
  t54 = ((int64_t)(v8));
  t55 = ((int64_t)(v4));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = fg_kv_ub_seq_id(t53, t56, t57);
  t59 = ((int64_t)(t58));
  t60 = ((int64_t)(v9));
  t61 = ((t59) == (t60));
  gg_function_4(t61, base + 0);
  }
  }
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(v9));
  v10 = gg_function_33(t62, t63, base + 0);
  t64 = ((int64_t)(v10));
  t65 = ((int64_t)(v7));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = fg_kv_get(t65, t66, t67);
  t69 = ((int64_t)(t68));
  t70 = ((t64) < (t69));
  if (t70) {
  {
  t71 = ((int64_t)(v7));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((int64_t)(v10));
  fg_kv_put(t71, t72, t73, t74);
  }
  }
  t75 = ((int64_t)(v10));
  t76 = ((int64_t)(v7));
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = fg_kv_get(t76, t77, t78);
  t80 = ((int64_t)(t79));
  t81 = ((t75) > (t80));
  if (t81) {
  {
  t82 = ((int64_t)(v7));
  t83 = ((int64_t)(((int64_t)UINT64_C(1))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((int64_t)(v10));
  fg_kv_put(t82, t83, t84, t85);
  }
  }
  t86 = ((int64_t)(v7));
  t87 = ((int64_t)(((int64_t)UINT64_C(10))));
  t88 = ((int64_t)(v8));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t87) + (uint64_t)(t88)))));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = ((int64_t)(v10));
  fg_kv_put(t86, t89, t90, t91);
  t92 = ((int64_t)(v0));
  t93 = ((int64_t)(v10));
  v11 = gg_function_6(t92, t93, base + 0);
  t94 = ((int64_t)(v0));
  t95 = ((int64_t)(v10));
  v12 = gg_function_34(t94, t95, base + 0);
  t96 = ((int64_t)(v12));
  t97 = ((int64_t)(v11));
  t98 = gg_function_35(t97, base + 0);
  t99 = ((int64_t)(t98));
  t100 = ((int64_t)(((int64_t)UINT64_C(2))));
  t101 = ((int64_t)(v4));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t100) * (uint64_t)(t101)))));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t99) + (uint64_t)(t102)))));
  t104 = ((t96) > (t103));
  if (t104) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t105 = ((int64_t)(v4));
  t106 = ((int64_t)(v11));
  t107 = gg_function_10(t106, base + 0);
  t108 = ((int64_t)(t107));
  t109 = ((t105) > (t108));
  if (t109) {
  {
  t110 = ((int64_t)(v7));
  fg_kv_destroy(t110);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
  v14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t111 = ((int64_t)(v2));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = ((t111) != (t112));
  if (t113) {
  {
  v14 = ((int64_t)(v4));
  }
  }
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t114 = ((int64_t)(v15));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t114) == (t115)))) goto label5;
  {
  t116 = ((int64_t)(v11));
  v16 = gg_function_10(t116, base + 0);
  t117 = ((int64_t)(v12));
  t118 = ((int64_t)(v14));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) + (uint64_t)(t118)))));
  t120 = ((int64_t)(v16));
  t121 = ((t119) > (t120));
  if (t121) {
  {
  t122 = ((int64_t)(v13));
  t123 = ((int64_t)(v16));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t122) + (uint64_t)(t123)))));
  t125 = ((int64_t)(v12));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t124) - (uint64_t)(t125)))));
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t126 = ((int64_t)(v17));
  t127 = ((int64_t)(v14));
  if (!(((t126) < (t127)))) goto label8;
  {
  v18 = ((int64_t)(v12));
  t128 = v12;
  t129 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t128) + (uint64_t)(t129)));
  t130 = v13;
  t131 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)((uint64_t)(t130) + (uint64_t)(t131)));
  t132 = ((int64_t)(v11));
  t133 = ((int64_t)(v18));
  v19 = gg_function_37(t132, t133, base + 0);
  t134 = ((int64_t)(v19));
  t135 = ((int64_t)(((int64_t)UINT64_C(0))));
  t136 = ((t134) == (t135));
  t137 = t136;
  if (t137) {
  t138 = ((int64_t)(v11));
  t139 = ((int64_t)(v18));
  t140 = gg_function_39(t138, t139, base + 0);
  t141 = ((int64_t)(t140));
  t142 = ((int64_t)(((int64_t)UINT64_C(1))));
  t137 = ((t141) == (t142));
  }
  t143 = t137;
  if (t143) {
  {
  t144 = ((int64_t)(v11));
  t145 = ((int64_t)(v18));
  v20 = gg_function_42(t144, t145, base + 0);
  t146 = ((int64_t)(v19));
  t147 = ((int64_t)(((int64_t)UINT64_C(0))));
  t148 = ((t146) == (t147));
  if (t148) {
  {
  t149 = ((int64_t)(v11));
  t150 = ((int64_t)(v18));
  v21 = gg_function_43(t149, t150, base + 0);
  t151 = ((int64_t)(v0));
  t152 = ((int64_t)(((int64_t)UINT64_C(3))));
  t153 = ((int64_t)(((int64_t)UINT64_C(0))));
  t154 = fg_kv_get(t151, t152, t153);
  v22 = ((int64_t)(t154));
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t155 = ((int64_t)(v22));
  t156 = ((int64_t)(((int64_t)UINT64_C(0))));
  t157 = ((t155) > (t156));
  if (t157) {
  {
  t158 = ((int64_t)(v0));
  t159 = ((int64_t)(v22));
  t160 = ((int64_t)(v0));
  t161 = ((int64_t)(((int64_t)UINT64_C(4))));
  t162 = ((int64_t)(((int64_t)UINT64_C(0))));
  t163 = fg_kv_get(t160, t161, t162);
  t164 = ((int64_t)(t163));
  t165 = ((int64_t)(v20));
  t166 = ((int64_t)(v11));
  t167 = ((int64_t)(v21));
  t168 = gg_function_44(t166, t167, base + 0);
  t169 = ((int64_t)(t168));
  t170 = ((int64_t)(((int64_t)UINT64_C(1))));
  t171 = ((int64_t)(((int64_t)((uint64_t)(t169) + (uint64_t)(t170)))));
  t172 = fg_kv_swa_masked(t158, t159, t164, t165, t171);
  v23 = ((int64_t)(t172));
  }
  }
  t173 = ((int64_t)(v23));
  t174 = ((int64_t)(((int64_t)UINT64_C(0))));
  t175 = ((t173) != (t174));
  if (t175) {
  {
  v19 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  }
  }
  }
  t176 = ((int64_t)(v19));
  t177 = ((int64_t)(((int64_t)UINT64_C(0))));
  t178 = ((t176) != (t177));
  if (t178) {
  {
  t179 = ((int64_t)(v7));
  t180 = ((int64_t)(v8));
  t181 = ((int64_t)(v18));
  fg_kv_slot_push(t179, t180, t181);
  }
  } else {
  {
  t182 = ((int64_t)(v2));
  t183 = ((int64_t)(((int64_t)UINT64_C(0))));
  t184 = ((t182) != (t183));
  if (t184) {
  {
  goto label8;
  }
  }
  }
  }
  t185 = v17;
  t186 = ((int64_t)(((int64_t)UINT64_C(1))));
  v17 = ((int64_t)((uint64_t)(t185) + (uint64_t)(t186)));
  }
  goto label7;
label8:;
  t187 = ((int64_t)(v7));
  t188 = ((int64_t)(v8));
  t189 = fg_kv_slot_len(t187, t188);
  t190 = ((int64_t)(t189));
  t191 = ((int64_t)(v4));
  t192 = ((t190) == (t191));
  if (t192) {
  {
  v15 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t193 = ((int64_t)(v2));
  t194 = ((int64_t)(((int64_t)UINT64_C(0))));
  t195 = ((t193) != (t194));
  if (t195) {
  {
  t196 = ((int64_t)(v7));
  t197 = ((int64_t)(v8));
  fg_kv_slot_clear(t196, t197);
  }
  }
  t198 = ((int64_t)(v13));
  t199 = ((int64_t)(v16));
  t200 = ((t198) >= (t199));
  if (t200) {
  {
  t201 = ((int64_t)(v7));
  fg_kv_destroy(t201);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  }
  goto label4;
label5:;
  t202 = ((int64_t)(v7));
  t203 = ((int64_t)(v8));
  t204 = fg_kv_slot_len(t202, t203);
  t205 = ((int64_t)(t204));
  t206 = ((int64_t)(v4));
  t207 = ((t205) < (t206));
  if (t207) {
  {
  t208 = ((int64_t)(v7));
  fg_kv_destroy(t208);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t209 = v8;
  t210 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t209) + (uint64_t)(t210)));
  }
  goto label1;
label2:;
  t211 = ((int64_t)(v7));
  t212 = ((int64_t)(((int64_t)UINT64_C(1))));
  t213 = ((int64_t)(((int64_t)UINT64_C(0))));
  t214 = fg_kv_get(t211, t212, t213);
  t215 = ((int64_t)(t214));
  t216 = ((int64_t)(v7));
  t217 = ((int64_t)(((int64_t)UINT64_C(0))));
  t218 = ((int64_t)(((int64_t)UINT64_C(0))));
  t219 = fg_kv_get(t216, t217, t218);
  t220 = ((int64_t)(t219));
  t221 = ((t215) >= (t220));
  gg_function_4(t221, base + 0);
  t0 = ((int64_t)(v7));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_150(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
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
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
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
  int64_t t82;
  int64_t t83;
  uint8_t t84;
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
  uint8_t t113;
  uint8_t t114;
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
  uint8_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  uint8_t t139;
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
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v3));
  v9 = gg_function_6(t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = fg_state_has_cell_ext(t3);
  v10 = ((int64_t)(t4));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(v9));
  t7 = gg_function_10(t6, base + 0);
  t8 = ((int64_t)(t7));
  t9 = ((t5) > (t8));
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)UINT64_C(1));
  t13 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t12)))));
  t14 = ((int64_t)UINT64_C(1));
  t15 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t14)))));
  gg_function_82(t10, t11, t13, t15, base + 0);
  t16 = ((int64_t)(v4));
  t17 = fg_kv_test_ubatch(t16);
  v11 = ((int64_t)(t17));
  t18 = ((int64_t)(v11));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((int64_t)(v2));
  fg_kv_test_ub_set_seq_unq(t18, t19, t20);
  t21 = ((int64_t)(((int64_t)UINT64_C(4))));
  t22 = fg_kv_obj(t21);
  v12 = ((int64_t)(t22));
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t23 = ((int64_t)(v13));
  t24 = ((int64_t)(v4));
  if (!(((t23) < (t24)))) goto label2;
  {
  t25 = ((int64_t)(v1));
  t26 = fg153_i64_fg_state_read_i32(t25);
  v14 = ((int64_t)(t26));
  t27 = ((int64_t)(v1));
  t28 = fg153_i64_fg_state_read_u32(t27);
  v15 = ((int64_t)(t28));
  t29 = ((int64_t)(v15));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((t29) != (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v11));
  fg_kv_test_ubatch_free(t32);
  t33 = ((int64_t)(v12));
  fg_kv_destroy(t33);
  t34 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t34)))));
  goto gg_done;
  }
  }
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t35) != (t36));
  if (t37) {
  {
  t38 = ((int64_t)(v1));
  t39 = fg153_i64_fg_state_read_i32(t38);
  v16 = ((int64_t)(t39));
  t40 = ((int64_t)(v1));
  t41 = fg153_i64_fg_state_read_i32(t40);
  v17 = ((int64_t)(t41));
  t42 = ((int64_t)(v1));
  t43 = fg153_i64_fg_state_read_i32(t42);
  v18 = ((int64_t)(t43));
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v16));
  fg_kv_vec_push(t44, t45);
  t46 = ((int64_t)(v12));
  t47 = ((int64_t)(v17));
  fg_kv_vec_push(t46, t47);
  t48 = ((int64_t)(v12));
  t49 = ((int64_t)(v18));
  fg_kv_vec_push(t48, t49);
  t50 = ((int64_t)(v11));
  t51 = ((int64_t)(v13));
  t52 = ((int64_t)(v18));
  fg_kv_test_ub_set_token(t50, t51, t52);
  }
  }
  t53 = ((int64_t)(v1));
  t54 = fg153_i64_fg_state_read_i32(t53);
  v19 = ((int64_t)(t54));
  t55 = ((int64_t)(v11));
  t56 = ((int64_t)(v13));
  t57 = ((int64_t)(v14));
  fg_kv_test_ub_set_pos(t55, t56, t57);
  t58 = ((int64_t)(v11));
  t59 = ((int64_t)(v13));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((int64_t)(v2));
  fg_kv_test_ub_set_seq(t58, t59, t60, t61);
  t62 = v13;
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  }
  goto label1;
label2:;
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((int64_t)(v6));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((t64) != (t65));
  if (t66) {
  {
  t67 = ((int64_t)(v7));
  t68 = ((int64_t)(v8));
  t69 = fg153_i64_fg_sinfos_in_empty(t67, t68);
  t70 = ((int64_t)(t69));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((t70) != (t71));
  if (t72) {
  {
  t73 = ((int64_t)(v11));
  fg_kv_test_ubatch_free(t73);
  t74 = ((int64_t)(v12));
  fg_kv_destroy(t74);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t75 = ((int64_t)(((int64_t)UINT64_C(3))));
  t76 = fg_kv_obj(t75);
  v20 = ((int64_t)(t76));
  t77 = ((int64_t)(v7));
  t78 = ((int64_t)(v8));
  t79 = ((int64_t)(v20));
  fg153_i64_fg_sinfos_in_to_slot(t77, t78, t79);
  t80 = ((int64_t)(v20));
  t81 = gg_function_56(t80, base + 0);
  t82 = ((int64_t)(t81));
  t83 = ((int64_t)(v4));
  t84 = ((t82) != (t83));
  if (t84) {
  {
  t85 = ((int64_t)(v11));
  fg_kv_test_ubatch_free(t85);
  t86 = ((int64_t)(v12));
  fg_kv_destroy(t86);
  t87 = ((int64_t)(v20));
  fg_kv_destroy(t87);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t88 = ((int64_t)(v20));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = ((int64_t)(v3));
  fg_kv_put(t88, t89, t90, t91);
  t92 = ((int64_t)(v20));
  t93 = ((int64_t)(((int64_t)UINT64_C(1))));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((int64_t)(v3));
  fg_kv_put(t92, t93, t94, t95);
  t96 = ((int64_t)(v20));
  t97 = ((int64_t)(((int64_t)UINT64_C(3))));
  t98 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg_kv_put(t96, t97, t98, t99);
  t100 = ((int64_t)(v20));
  t101 = ((int64_t)(((int64_t)UINT64_C(10))));
  t102 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = ((int64_t)(v3));
  fg_kv_put(t100, t101, t102, t103);
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t104 = ((int64_t)(v21));
  t105 = ((int64_t)(v4));
  if (!(((t104) < (t105)))) goto label5;
  {
  t106 = ((int64_t)(v20));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = ((int64_t)(v21));
  v22 = gg_function_58(t106, t107, t108, base + 0);
  t109 = ((int64_t)(v22));
  t110 = ((int64_t)(v9));
  t111 = gg_function_10(t110, base + 0);
  t112 = ((int64_t)(t111));
  t113 = ((t109) >= (t112));
  t114 = t113;
  if (!t114) {
  t115 = ((int64_t)(v9));
  t116 = ((int64_t)(v22));
  t117 = gg_function_37(t115, t116, base + 0);
  t118 = ((int64_t)(t117));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((t118) == (t119));
  }
  t120 = t114;
  if (t120) {
  {
  t121 = ((int64_t)(v11));
  fg_kv_test_ubatch_free(t121);
  t122 = ((int64_t)(v12));
  fg_kv_destroy(t122);
  t123 = ((int64_t)(v20));
  fg_kv_destroy(t123);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t124 = v21;
  t125 = ((int64_t)(((int64_t)UINT64_C(1))));
  v21 = ((int64_t)((uint64_t)(t124) + (uint64_t)(t125)));
  }
  goto label4;
label5:;
  }
  } else {
  {
  t126 = ((int64_t)(v0));
  t127 = ((int64_t)(v11));
  t128 = ((int64_t)(((int64_t)UINT64_C(0))));
  v20 = gg_function_26(t126, t127, t128, base + 0);
  t129 = ((int64_t)(v20));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = ((t129) == (t130));
  if (t131) {
  {
  t132 = ((int64_t)(v11));
  fg_kv_test_ubatch_free(t132);
  t133 = ((int64_t)(v12));
  fg_kv_destroy(t133);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  t134 = ((int64_t)(v0));
  t135 = ((int64_t)(v20));
  t136 = ((int64_t)(v11));
  gg_function_70(t134, t135, t136, base + 0);
  t137 = ((int64_t)(v10));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((t137) != (t138));
  if (t139) {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t140 = ((int64_t)(v13));
  t141 = ((int64_t)(v4));
  if (!(((t140) < (t141)))) goto label8;
  {
  t142 = ((int64_t)(v20));
  t143 = ((int64_t)(((int64_t)UINT64_C(0))));
  t144 = ((int64_t)(v13));
  v23 = gg_function_58(t142, t143, t144, base + 0);
  t145 = ((int64_t)(v9));
  t146 = ((int64_t)(v23));
  t147 = ((int64_t)(v12));
  t148 = ((int64_t)(((int64_t)UINT64_C(10))));
  t149 = ((int64_t)(((int64_t)UINT64_C(3))));
  t150 = ((int64_t)(v13));
  t151 = ((int64_t)(((int64_t)((uint64_t)(t149) * (uint64_t)(t150)))));
  t152 = ((int64_t)(((int64_t)((uint64_t)(t148) + (uint64_t)(t151)))));
  t153 = ((int64_t)(((int64_t)UINT64_C(0))));
  t154 = fg_kv_get(t147, t152, t153);
  t155 = ((int64_t)(t154));
  t156 = ((int64_t)(v12));
  t157 = ((int64_t)(((int64_t)UINT64_C(11))));
  t158 = ((int64_t)(((int64_t)UINT64_C(3))));
  t159 = ((int64_t)(v13));
  t160 = ((int64_t)(((int64_t)((uint64_t)(t158) * (uint64_t)(t159)))));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t157) + (uint64_t)(t160)))));
  t162 = ((int64_t)(((int64_t)UINT64_C(0))));
  t163 = fg_kv_get(t156, t161, t162);
  t164 = ((int64_t)(t163));
  t165 = ((int64_t)(v12));
  t166 = ((int64_t)(((int64_t)UINT64_C(12))));
  t167 = ((int64_t)(((int64_t)UINT64_C(3))));
  t168 = ((int64_t)(v13));
  t169 = ((int64_t)(((int64_t)((uint64_t)(t167) * (uint64_t)(t168)))));
  t170 = ((int64_t)(((int64_t)((uint64_t)(t166) + (uint64_t)(t169)))));
  t171 = ((int64_t)(((int64_t)UINT64_C(0))));
  t172 = fg_kv_get(t165, t170, t171);
  t173 = ((int64_t)(t172));
  gg_function_65(t145, t146, t155, t164, t173, base + 0);
  t174 = v13;
  t175 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)((uint64_t)(t174) + (uint64_t)(t175)));
  }
  goto label7;
label8:;
  }
  }
  t176 = ((int64_t)(v11));
  fg_kv_test_ubatch_free(t176);
  t177 = ((int64_t)(v12));
  fg_kv_destroy(t177);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_148(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
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
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v2));
  v5 = gg_function_6(t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = fg_state_has_cell_ext(t3);
  v6 = ((int64_t)(t4));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg_kv_get(t5, t6, t7);
  v7 = ((int64_t)(t8));
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(v5));
  t11 = gg_function_10(t10, base + 0);
  t12 = ((int64_t)(t11));
  t13 = ((t9) > (t12));
  if (t13) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t14 = ((int64_t)(v8));
  t15 = ((int64_t)(v3));
  if (!(((t14) < (t15)))) goto label2;
  {
  t16 = ((int64_t)(v1));
  t17 = fg153_i64_fg_state_read_i32(t16);
  v9 = ((int64_t)(t17));
  t18 = ((int64_t)(v1));
  t19 = fg153_i64_fg_state_read_u32(t18);
  v10 = ((int64_t)(t19));
  t20 = ((int64_t)(v5));
  t21 = ((int64_t)(v8));
  t22 = ((int64_t)(v9));
  gg_function_59(t20, t21, t22, base + 0);
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) != (t24));
  if (t25) {
  {
  t26 = ((int64_t)(v1));
  t27 = fg153_i64_fg_state_read_i32(t26);
  v11 = ((int64_t)(t27));
  t28 = ((int64_t)(v1));
  t29 = fg153_i64_fg_state_read_i32(t28);
  v12 = ((int64_t)(t29));
  t30 = ((int64_t)(v1));
  t31 = fg153_i64_fg_state_read_i32(t30);
  v13 = ((int64_t)(t31));
  t32 = ((int64_t)(v5));
  t33 = ((int64_t)(v8));
  t34 = ((int64_t)(v11));
  t35 = ((int64_t)(v12));
  t36 = ((int64_t)(v13));
  gg_function_65(t32, t33, t34, t35, t36, base + 0);
  }
  }
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t37 = ((int64_t)(v14));
  t38 = ((int64_t)(v10));
  if (!(((t37) < (t38)))) goto label5;
  {
  t39 = ((int64_t)(v1));
  t40 = fg153_i64_fg_state_read_i32(t39);
  v15 = ((int64_t)(t40));
  t41 = ((int64_t)(v15));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t41) < (t42));
  t44 = t43;
  if (!t44) {
  t45 = ((int64_t)(v15));
  t46 = ((int64_t)(v7));
  t44 = ((t45) >= (t46));
  }
  t47 = t44;
  if (t47) {
  {
  t48 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t48)))));
  goto gg_done;
  }
  }
  t49 = ((int64_t)(v5));
  t50 = ((int64_t)(v8));
  t51 = ((int64_t)(v15));
  gg_function_67(t49, t50, t51, base + 0);
  t52 = v14;
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t52) + (uint64_t)(t53)));
  }
  goto label4;
label5:;
  t54 = v8;
  t55 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t54) + (uint64_t)(t55)));
  }
  goto label1;
label2:;
  t56 = ((int64_t)(v4));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(v2));
  fg_kv_put(t56, t57, t58, t59);
  t60 = ((int64_t)(v4));
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((int64_t)(v2));
  fg_kv_put(t60, t61, t62, t63);
  t64 = ((int64_t)(v4));
  t65 = ((int64_t)(((int64_t)UINT64_C(3))));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg_kv_put(t64, t65, t66, t67);
  t68 = ((int64_t)(v4));
  t69 = ((int64_t)(((int64_t)UINT64_C(10))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((int64_t)(v2));
  fg_kv_put(t68, t69, t70, t71);
  t72 = ((int64_t)(v4));
  t73 = ((int64_t)(((int64_t)UINT64_C(2))));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((int64_t)(v3));
  fg_kv_put(t72, t73, t74, t75);
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t76 = ((int64_t)(v8));
  t77 = ((int64_t)(v3));
  if (!(((t76) < (t77)))) goto label8;
  {
  t78 = ((int64_t)(v4));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = ((int64_t)(v8));
  fg_kv_slot_push(t78, t79, t80);
  t81 = v8;
  t82 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t81) + (uint64_t)(t82)));
  }
  goto label7;
label8:;
  t83 = ((int64_t)(v0));
  t84 = ((int64_t)(v2));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_19(t83, t84, t85, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_141(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t13;
  uint8_t t14;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  v2 = gg_function_22(t4, base + 0);
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)(v1));
  t9 = gg_function_15(base + 0);
  t10 = ((int64_t)(t9));
  t11 = ((t8) < (t10));
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v2));
  t14 = ((t12) < (t13));
  if (t14) {
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
static void gg_function_140(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base) {
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
  uint8_t t12;
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
  uint8_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
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
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
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
  uint8_t t73;
  int64_t t74;
  int64_t t75;
  uint8_t t76;
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
  uint8_t t97;
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
  uint8_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  uint8_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  uint8_t t130;
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
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(62))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg_kv_get(t0, t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  if (t6) {
  {
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  t9 = ((int64_t)(((t7) & (t8))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) != (t10));
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg_kv_get(t13, t14, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t17) == (t18));
  }
  t19 = t12;
  if (t19) {
  {
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)UINT64_C(1));
  t22 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t21)))));
  t23 = ((t20) == (t22));
  t24 = t23;
  if (!t24) {
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(v2));
  t27 = gg_function_141(t25, t26, base + 0);
  t28 = ((int64_t)(t27));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t28) != (t29));
  }
  t30 = t24;
  gg_function_4(t30, base + 0);
  t31 = ((int64_t)(v0));
  v8 = gg_function_22(t31, base + 0);
  t32 = ((int64_t)(v5));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((t32) != (t33));
  if (t34) {
  {
  t35 = ((int64_t)(v4));
  t36 = ((int64_t)(v8));
  fg153_i64_fg_sinfos_init(t35, t36);
  }
  }
  t37 = ((int64_t)(v7));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t37) != (t38));
  if (t39) {
  {
  t40 = ((int64_t)(v6));
  t41 = fg153_i64_fg_sinfos_count(t40);
  t42 = ((int64_t)(t41));
  t43 = ((int64_t)(v8));
  t44 = ((t42) != (t43));
  if (t44) {
  {
  t45 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg_state_fail(t45);
  }
  }
  }
  }
  t46 = ((int64_t)(v1));
  t47 = fg153_i64_fg_state_read_u32(t46);
  v9 = ((int64_t)(t47));
  t48 = ((int64_t)(v9));
  t49 = ((int64_t)(v8));
  t50 = ((t48) != (t49));
  if (t50) {
  {
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg_state_fail(t51);
  }
  }
  t52 = ((int64_t)(v2));
  t53 = ((int64_t)UINT64_C(1));
  t54 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t53)))));
  t55 = ((t52) == (t54));
  if (t55) {
  {
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t56 = ((int64_t)(v10));
  t57 = ((int64_t)(v8));
  if (!(((t56) < (t57)))) goto label2;
  {
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(v10));
  t60 = gg_function_6(t58, t59, base + 0);
  gg_function_9(t60, base + 0);
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(v10));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_19(t61, t62, t63, base + 0);
  t64 = v10;
  t65 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t64) + (uint64_t)(t65)));
  }
  goto label1;
label2:;
  t66 = ((int64_t)(v0));
  fg_kv_clear_data(t66);
  }
  }
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t67 = ((int64_t)(v11));
  t68 = ((int64_t)(v8));
  if (!(((t67) < (t68)))) goto label5;
  {
  t69 = ((int64_t)(v1));
  t70 = fg153_i64_fg_state_read_u32(t69);
  v12 = ((int64_t)(t70));
  t71 = ((int64_t)(v12));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((t71) == (t72));
  if (t73) {
  {
  t74 = ((int64_t)(v7));
  t75 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = ((t74) != (t75));
  if (t76) {
  {
  t77 = ((int64_t)(v6));
  t78 = ((int64_t)(v11));
  t79 = fg153_i64_fg_sinfos_in_empty(t77, t78);
  t80 = ((int64_t)(t79));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = ((t80) == (t81));
  if (t82) {
  {
  t83 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg_state_fail(t83);
  }
  }
  }
  }
  t84 = v11;
  t85 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t84) + (uint64_t)(t85)));
  }
  } else {
  {
  v13 = ((int64_t)(v11));
  t86 = ((int64_t)(v2));
  t87 = ((int64_t)UINT64_C(1));
  t88 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t87)))));
  t89 = ((t86) != (t88));
  if (t89) {
  {
  t90 = ((int64_t)(v0));
  t91 = ((int64_t)(v2));
  v13 = gg_function_33(t90, t91, base + 0);
  }
  }
  t92 = ((int64_t)(((int64_t)UINT64_C(3))));
  t93 = fg_kv_obj(t92);
  v14 = ((int64_t)(t93));
  v15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t94 = ((int64_t)(v2));
  t95 = ((int64_t)UINT64_C(1));
  t96 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t95)))));
  t97 = ((t94) == (t96));
  if (t97) {
  {
  t98 = ((int64_t)(v0));
  t99 = ((int64_t)(v1));
  t100 = ((int64_t)(v13));
  t101 = ((int64_t)(v12));
  t102 = ((int64_t)(v14));
  v15 = gg_function_148(t98, t99, t100, t101, t102, base + 0);
  }
  } else {
  {
  t103 = ((int64_t)(v0));
  t104 = ((int64_t)(v1));
  t105 = ((int64_t)(v2));
  t106 = ((int64_t)(v13));
  t107 = ((int64_t)(v12));
  t108 = ((int64_t)(v14));
  t109 = ((int64_t)(v7));
  t110 = ((int64_t)(v6));
  t111 = ((int64_t)(v11));
  v15 = gg_function_150(t103, t104, t105, t106, t107, t108, t109, t110, t111, base + 0);
  }
  }
  t112 = ((int64_t)(v15));
  t113 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((t112) > (t113));
  if (t114) {
  {
  t115 = ((int64_t)(v0));
  t116 = ((int64_t)(v1));
  t117 = ((int64_t)(v13));
  t118 = ((int64_t)(v12));
  t119 = ((int64_t)(v14));
  v15 = gg_function_154(t115, t116, t117, t118, t119, base + 0);
  }
  }
  t120 = ((int64_t)(v15));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = ((t120) <= (t121));
  if (t122) {
  {
  t123 = ((int64_t)(v0));
  t124 = ((int64_t)(v2));
  t125 = ((int64_t)(v13));
  t126 = ((int64_t)(v14));
  gg_function_158(t123, t124, t125, t126, base + 0);
  t127 = ((int64_t)(((int64_t)UINT64_C(4))));
  fg_state_fail(t127);
  }
  }
  t128 = ((int64_t)(v5));
  t129 = ((int64_t)(((int64_t)UINT64_C(0))));
  t130 = ((t128) != (t129));
  if (t130) {
  {
  t131 = ((int64_t)(v4));
  t132 = ((int64_t)(v11));
  t133 = ((int64_t)(v14));
  fg153_i64_fg_sinfos_set_out(t131, t132, t133);
  }
  }
  t134 = ((int64_t)(v14));
  fg_kv_destroy(t134);
  t135 = v11;
  t136 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t135) + (uint64_t)(t136)));
  }
  }
  }
  goto label4;
label5:;
  }
gg_done:;
}
static void gg_function_128(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  v4 = p4;
  {
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg_kv_get(t0, t1, t2);
  v5 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v0));
  t6 = fg_state_v_trans(t5);
  t7 = ((int64_t)(t6));
  fg153_i64_fg_state_write_u32(t4, t7);
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(v3));
  fg153_i64_fg_state_write_u32(t8, t9);
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg_state_n_rot(t11, t12);
  t14 = ((int64_t)(t13));
  fg153_i64_fg_state_write_u32(t10, t14);
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = fg_state_n_rot(t16, t17);
  t19 = ((int64_t)(t18));
  fg153_i64_fg_state_write_u32(t15, t19);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(v3));
  if (!(((t20) < (t21)))) goto label2;
  {
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(v6));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg_state_tensor_type(t23, t24, t25, t26);
  t28 = ((int64_t)(t27));
  fg153_i64_fg_state_write_i32(t22, t28);
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(v6));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = fg_state_tensor_row_size(t30, t31, t32, t33);
  t35 = ((int64_t)(t34));
  fg153_i64_fg_state_write_u64(t29, t35);
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t36 = ((int64_t)(v7));
  t37 = ((int64_t)(v5));
  if (!(((t36) < (t37)))) goto label5;
  {
  t38 = ((int64_t)(v4));
  t39 = ((int64_t)(((int64_t)UINT64_C(10))));
  t40 = ((int64_t)(v7));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = fg_kv_get(t38, t41, t42);
  v8 = ((int64_t)(t43));
  t44 = ((int64_t)(v4));
  t45 = ((int64_t)(((int64_t)UINT64_C(11))));
  t46 = ((int64_t)(v7));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = fg_kv_get(t44, t47, t48);
  v9 = ((int64_t)(t49));
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(v2));
  t53 = ((int64_t)(v6));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((int64_t)(v8));
  t56 = ((int64_t)(v9));
  fg153_i64_fg_state_write_rows(t50, t51, t52, t53, t54, t55, t56);
  t57 = v7;
  t58 = ((int64_t)(((int64_t)UINT64_C(2))));
  v7 = ((int64_t)((uint64_t)(t57) + (uint64_t)(t58)));
  }
  goto label4;
label5:;
  t59 = v6;
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t59) + (uint64_t)(t60)));
  }
  goto label1;
label2:;
  t61 = ((int64_t)(v0));
  t62 = fg_state_v_trans(t61);
  t63 = ((int64_t)(t62));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((t63) == (t64));
  if (t65) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t66 = ((int64_t)(v6));
  t67 = ((int64_t)(v3));
  if (!(((t66) < (t67)))) goto label8;
  {
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(v6));
  t70 = fg_state_layer_has_v(t68, t69);
  t71 = ((int64_t)(t70));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((t71) != (t72));
  if (t73) {
  {
  t74 = ((int64_t)(v1));
  t75 = ((int64_t)(v0));
  t76 = ((int64_t)(v2));
  t77 = ((int64_t)(v6));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  t79 = fg_state_tensor_type(t75, t76, t77, t78);
  t80 = ((int64_t)(t79));
  fg153_i64_fg_state_write_i32(t74, t80);
  t81 = ((int64_t)(v1));
  t82 = ((int64_t)(v0));
  t83 = ((int64_t)(v2));
  t84 = ((int64_t)(v6));
  t85 = ((int64_t)(((int64_t)UINT64_C(1))));
  t86 = fg_state_tensor_row_size(t82, t83, t84, t85);
  t87 = ((int64_t)(t86));
  fg153_i64_fg_state_write_u64(t81, t87);
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t88 = ((int64_t)(v10));
  t89 = ((int64_t)(v5));
  if (!(((t88) < (t89)))) goto label11;
  {
  t90 = ((int64_t)(v4));
  t91 = ((int64_t)(((int64_t)UINT64_C(10))));
  t92 = ((int64_t)(v10));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) + (uint64_t)(t92)))));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = fg_kv_get(t90, t93, t94);
  v11 = ((int64_t)(t95));
  t96 = ((int64_t)(v4));
  t97 = ((int64_t)(((int64_t)UINT64_C(11))));
  t98 = ((int64_t)(v10));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t97) + (uint64_t)(t98)))));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = fg_kv_get(t96, t99, t100);
  v12 = ((int64_t)(t101));
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(v1));
  t104 = ((int64_t)(v2));
  t105 = ((int64_t)(v6));
  t106 = ((int64_t)(((int64_t)UINT64_C(1))));
  t107 = ((int64_t)(v11));
  t108 = ((int64_t)(v12));
  fg153_i64_fg_state_write_rows(t102, t103, t104, t105, t106, t107, t108);
  t109 = v10;
  t110 = ((int64_t)(((int64_t)UINT64_C(2))));
  v10 = ((int64_t)((uint64_t)(t109) + (uint64_t)(t110)));
  }
  goto label10;
label11:;
  }
  }
  t111 = v6;
  t112 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t111) + (uint64_t)(t112)));
  }
  goto label7;
label8:;
  }
  } else {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t113 = ((int64_t)(v6));
  t114 = ((int64_t)(v3));
  if (!(((t113) < (t114)))) goto label14;
  {
  t115 = ((int64_t)(v0));
  t116 = ((int64_t)(v6));
  t117 = fg_state_layer_has_v(t115, t116);
  t118 = ((int64_t)(t117));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((t118) != (t119));
  if (t120) {
  {
  t121 = ((int64_t)(v0));
  t122 = ((int64_t)(v6));
  t123 = fg_state_layer_il(t121, t122);
  v13 = ((int64_t)(t123));
  t124 = ((int64_t)(v1));
  t125 = ((int64_t)(v0));
  t126 = ((int64_t)(v2));
  t127 = ((int64_t)(v6));
  t128 = ((int64_t)(((int64_t)UINT64_C(1))));
  t129 = fg_state_tensor_type(t125, t126, t127, t128);
  t130 = ((int64_t)(t129));
  fg153_i64_fg_state_write_i32(t124, t130);
  t131 = ((int64_t)(v1));
  t132 = ((int64_t)(v0));
  t133 = ((int64_t)(v2));
  t134 = ((int64_t)(v6));
  t135 = ((int64_t)(((int64_t)UINT64_C(1))));
  t136 = fg_state_tensor_size_el(t132, t133, t134, t135);
  t137 = ((int64_t)(t136));
  fg153_i64_fg_state_write_u32(t131, t137);
  t138 = ((int64_t)(v0));
  t139 = ((int64_t)(v13));
  t140 = fg_state_n_embd_v(t138, t139);
  v14 = ((int64_t)(t140));
  t141 = ((int64_t)(v1));
  t142 = ((int64_t)(v14));
  fg153_i64_fg_state_write_u32(t141, t142);
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t143 = ((int64_t)(v15));
  t144 = ((int64_t)(v14));
  if (!(((t143) < (t144)))) goto label17;
  {
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t145 = ((int64_t)(v16));
  t146 = ((int64_t)(v5));
  if (!(((t145) < (t146)))) goto label20;
  {
  t147 = ((int64_t)(v4));
  t148 = ((int64_t)(((int64_t)UINT64_C(10))));
  t149 = ((int64_t)(v16));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) + (uint64_t)(t149)))));
  t151 = ((int64_t)(((int64_t)UINT64_C(0))));
  t152 = fg_kv_get(t147, t150, t151);
  v17 = ((int64_t)(t152));
  t153 = ((int64_t)(v4));
  t154 = ((int64_t)(((int64_t)UINT64_C(11))));
  t155 = ((int64_t)(v16));
  t156 = ((int64_t)(((int64_t)((uint64_t)(t154) + (uint64_t)(t155)))));
  t157 = ((int64_t)(((int64_t)UINT64_C(0))));
  t158 = fg_kv_get(t153, t156, t157);
  v18 = ((int64_t)(t158));
  t159 = ((int64_t)(v0));
  t160 = ((int64_t)(v1));
  t161 = ((int64_t)(v2));
  t162 = ((int64_t)(v6));
  t163 = ((int64_t)(v15));
  t164 = ((int64_t)(v17));
  t165 = ((int64_t)(v18));
  fg153_i64_fg_state_write_trans(t159, t160, t161, t162, t163, t164, t165);
  t166 = v16;
  t167 = ((int64_t)(((int64_t)UINT64_C(2))));
  v16 = ((int64_t)((uint64_t)(t166) + (uint64_t)(t167)));
  }
  goto label19;
label20:;
  t168 = v15;
  t169 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t168) + (uint64_t)(t169)));
  }
  goto label16;
label17:;
  }
  }
  t170 = v6;
  t171 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t170) + (uint64_t)(t171)));
  }
  goto label13;
label14:;
  }
  }
  }
gg_done:;
}
static int64_t gg_function_64(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(14))));
  t3 = ((int64_t)(v1));
  t4 = fg_w2_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_63(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(13))));
  t3 = ((int64_t)(v1));
  t4 = fg_w2_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_62(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(12))));
  t3 = ((int64_t)(v1));
  t4 = fg_w2_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_124(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t12;
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
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  uint8_t t63;
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
  uint8_t t91;
  int64_t t92;
  int64_t t93;
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint8_t t99;
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
  uint8_t t112;
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
  uint8_t t152;
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
  uint8_t t190;
  uint8_t t191;
  int64_t t192;
  int64_t t193;
  uint8_t t194;
  uint8_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
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
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(62))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg_kv_get(t0, t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  if (t6) {
  {
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  t9 = ((int64_t)(((t7) & (t8))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) != (t10));
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg_kv_get(t13, t14, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t17) == (t18));
  }
  t19 = t12;
  if (t19) {
  {
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  v4 = gg_function_22(t20, base + 0);
  t21 = ((int64_t)(v0));
  t22 = fg_state_has_cell_ext(t21);
  v5 = ((int64_t)(t22));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg_kv_get(t23, t24, t25);
  v6 = ((int64_t)(t26));
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(7))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = fg_kv_get(t27, t28, t29);
  v7 = ((int64_t)(t30));
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(v4));
  fg153_i64_fg_state_write_u32(t31, t32);
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t33 = ((int64_t)(v8));
  t34 = ((int64_t)(v4));
  if (!(((t33) < (t34)))) goto label2;
  {
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(v8));
  v9 = gg_function_6(t35, t36, base + 0);
  t37 = ((int64_t)(v9));
  v10 = gg_function_10(t37, base + 0);
  t38 = ((int64_t)(((int64_t)UINT64_C(4))));
  t39 = fg_kv_obj(t38);
  v11 = ((int64_t)(t39));
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((int64_t)UINT64_C(1));
  v13 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t40)))));
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t41 = ((int64_t)(v14));
  t42 = ((int64_t)(v10));
  if (!(((t41) < (t42)))) goto label5;
  {
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((int64_t)(v9));
  t44 = ((int64_t)(v14));
  t45 = gg_function_37(t43, t44, base + 0);
  t46 = ((int64_t)(t45));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((t46) == (t47));
  if (t48) {
  {
  t49 = ((int64_t)(v2));
  t50 = ((int64_t)UINT64_C(1));
  t51 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t50)))));
  t52 = ((t49) == (t51));
  if (t52) {
  {
  v15 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t53 = ((int64_t)(v9));
  t54 = ((int64_t)(v14));
  t55 = ((int64_t)(v2));
  t56 = gg_function_66(t53, t54, t55, base + 0);
  t57 = ((int64_t)(t56));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((t57) != (t58));
  if (t59) {
  {
  v15 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  }
  }
  }
  t60 = ((int64_t)(v15));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((t60) != (t61));
  t63 = t62;
  if (t63) {
  t64 = ((int64_t)(v2));
  t65 = ((int64_t)UINT64_C(1));
  t66 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t65)))));
  t63 = ((t64) != (t66));
  }
  t67 = t63;
  if (t67) {
  {
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(3))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = fg_kv_get(t68, t69, t70);
  v16 = ((int64_t)(t71));
  t72 = ((int64_t)(v16));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((t72) > (t73));
  if (t74) {
  {
  t75 = ((int64_t)(v0));
  t76 = ((int64_t)(v16));
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(4))));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = fg_kv_get(t77, t78, t79);
  t81 = ((int64_t)(t80));
  t82 = ((int64_t)(v9));
  t83 = ((int64_t)(v14));
  t84 = gg_function_42(t82, t83, base + 0);
  t85 = ((int64_t)(v9));
  t86 = ((int64_t)(v2));
  t87 = gg_function_44(t85, t86, base + 0);
  t88 = fg_kv_swa_masked(t75, t76, t81, t84, t87);
  t89 = ((int64_t)(t88));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = ((t89) != (t90));
  if (t91) {
  {
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  }
  }
  }
  }
  t92 = ((int64_t)(v15));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((t92) != (t93));
  if (t94) {
  {
  t95 = v12;
  t96 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  t97 = ((int64_t)(v13));
  t98 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((t97) < (t98));
  if (t99) {
  {
  v13 = ((int64_t)(v14));
  }
  }
  }
  } else {
  {
  t100 = ((int64_t)(v13));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t100) >= (t101));
  if (t102) {
  {
  t103 = ((int64_t)(v11));
  t104 = ((int64_t)(v13));
  fg_kv_vec_push(t103, t104);
  t105 = ((int64_t)(v11));
  t106 = ((int64_t)(v14));
  fg_kv_vec_push(t105, t106);
  t107 = ((int64_t)UINT64_C(1));
  v13 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t107)))));
  }
  }
  }
  }
  t108 = v14;
  t109 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t108) + (uint64_t)(t109)));
  }
  goto label4;
label5:;
  t110 = ((int64_t)(v13));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = ((t110) >= (t111));
  if (t112) {
  {
  t113 = ((int64_t)(v11));
  t114 = ((int64_t)(v13));
  fg_kv_vec_push(t113, t114);
  t115 = ((int64_t)(v11));
  t116 = ((int64_t)(v10));
  fg_kv_vec_push(t115, t116);
  }
  }
  t117 = ((int64_t)(v1));
  t118 = ((int64_t)(v12));
  fg153_i64_fg_state_write_u32(t117, t118);
  t119 = ((int64_t)(v12));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = ((t119) != (t120));
  if (t121) {
  {
  t122 = ((int64_t)(v11));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t125 = fg_kv_get(t122, t123, t124);
  v17 = ((int64_t)(t125));
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t126 = ((int64_t)(v18));
  t127 = ((int64_t)(v17));
  if (!(((t126) < (t127)))) goto label8;
  {
  t128 = ((int64_t)(v11));
  t129 = ((int64_t)(((int64_t)UINT64_C(10))));
  t130 = ((int64_t)(v18));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t129) + (uint64_t)(t130)))));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t133 = fg_kv_get(t128, t131, t132);
  v19 = ((int64_t)(t133));
  t134 = ((int64_t)(v11));
  t135 = ((int64_t)(((int64_t)UINT64_C(11))));
  t136 = ((int64_t)(v18));
  t137 = ((int64_t)(((int64_t)((uint64_t)(t135) + (uint64_t)(t136)))));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = fg_kv_get(t134, t137, t138);
  v20 = ((int64_t)(t139));
  v21 = ((int64_t)(v19));
label10:;
  t140 = ((int64_t)(v21));
  t141 = ((int64_t)(v20));
  if (!(((t140) < (t141)))) goto label11;
  {
  t142 = ((int64_t)(v1));
  t143 = ((int64_t)(v9));
  t144 = ((int64_t)(v21));
  t145 = gg_function_42(t143, t144, base + 0);
  fg153_i64_fg_state_write_i32(t142, t145);
  v22 = ((int64_t)(((int64_t)UINT64_C(0))));
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t146 = ((int64_t)(v23));
  t147 = ((int64_t)(v6));
  if (!(((t146) < (t147)))) goto label14;
  {
  t148 = ((int64_t)(v2));
  t149 = ((int64_t)UINT64_C(1));
  t150 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t149)))));
  t151 = ((t148) == (t150));
  t152 = t151;
  if (!t152) {
  t153 = ((int64_t)(v23));
  t154 = ((int64_t)(v2));
  t152 = ((t153) == (t154));
  }
  t155 = t152;
  t156 = t155;
  if (t156) {
  t157 = ((int64_t)(v9));
  t158 = ((int64_t)(v21));
  t159 = ((int64_t)(v23));
  t160 = gg_function_66(t157, t158, t159, base + 0);
  t161 = ((int64_t)(t160));
  t162 = ((int64_t)(((int64_t)UINT64_C(0))));
  t156 = ((t161) != (t162));
  }
  t163 = t156;
  if (t163) {
  {
  t164 = v22;
  t165 = ((int64_t)(((int64_t)UINT64_C(1))));
  v22 = ((int64_t)((uint64_t)(t164) + (uint64_t)(t165)));
  }
  }
  t166 = v23;
  t167 = ((int64_t)(((int64_t)UINT64_C(1))));
  v23 = ((int64_t)((uint64_t)(t166) + (uint64_t)(t167)));
  }
  goto label13;
label14:;
  t168 = ((int64_t)(v1));
  t169 = ((int64_t)(v22));
  fg153_i64_fg_state_write_u32(t168, t169);
  t170 = ((int64_t)(v5));
  t171 = ((int64_t)(((int64_t)UINT64_C(0))));
  t172 = ((t170) != (t171));
  if (t172) {
  {
  t173 = ((int64_t)(v1));
  t174 = ((int64_t)(v9));
  t175 = ((int64_t)(v21));
  t176 = gg_function_62(t174, t175, base + 0);
  fg153_i64_fg_state_write_i32(t173, t176);
  t177 = ((int64_t)(v1));
  t178 = ((int64_t)(v9));
  t179 = ((int64_t)(v21));
  t180 = gg_function_63(t178, t179, base + 0);
  fg153_i64_fg_state_write_i32(t177, t180);
  t181 = ((int64_t)(v1));
  t182 = ((int64_t)(v9));
  t183 = ((int64_t)(v21));
  t184 = gg_function_64(t182, t183, base + 0);
  fg153_i64_fg_state_write_i32(t181, t184);
  }
  }
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t185 = ((int64_t)(v23));
  t186 = ((int64_t)(v6));
  if (!(((t185) < (t186)))) goto label17;
  {
  t187 = ((int64_t)(v2));
  t188 = ((int64_t)UINT64_C(1));
  t189 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t188)))));
  t190 = ((t187) == (t189));
  t191 = t190;
  if (!t191) {
  t192 = ((int64_t)(v23));
  t193 = ((int64_t)(v2));
  t191 = ((t192) == (t193));
  }
  t194 = t191;
  t195 = t194;
  if (t195) {
  t196 = ((int64_t)(v9));
  t197 = ((int64_t)(v21));
  t198 = ((int64_t)(v23));
  t199 = gg_function_66(t196, t197, t198, base + 0);
  t200 = ((int64_t)(t199));
  t201 = ((int64_t)(((int64_t)UINT64_C(0))));
  t195 = ((t200) != (t201));
  }
  t202 = t195;
  if (t202) {
  {
  t203 = ((int64_t)(v1));
  t204 = ((int64_t)(v23));
  fg153_i64_fg_state_write_i32(t203, t204);
  }
  }
  t205 = v23;
  t206 = ((int64_t)(((int64_t)UINT64_C(1))));
  v23 = ((int64_t)((uint64_t)(t205) + (uint64_t)(t206)));
  }
  goto label16;
label17:;
  t207 = v21;
  t208 = ((int64_t)(((int64_t)UINT64_C(1))));
  v21 = ((int64_t)((uint64_t)(t207) + (uint64_t)(t208)));
  }
  goto label10;
label11:;
  t209 = v18;
  t210 = ((int64_t)(((int64_t)UINT64_C(2))));
  v18 = ((int64_t)((uint64_t)(t209) + (uint64_t)(t210)));
  }
  goto label7;
label8:;
  t211 = ((int64_t)(v0));
  t212 = ((int64_t)(v1));
  t213 = ((int64_t)(v8));
  t214 = ((int64_t)(v7));
  t215 = ((int64_t)(v11));
  gg_function_128(t211, t212, t213, t214, t215, base + 0);
  }
  }
  t216 = ((int64_t)(v11));
  fg_kv_destroy(t216);
  t217 = v8;
  t218 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t217) + (uint64_t)(t218)));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_107(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  v1 = gg_function_10(t0, base + 0);
  t2 = v1;
  t1 = 0;
label0:;
  if (t1 >= t2) goto label2;
  v2 = t1;
  {
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(11))));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_w2_put(t3, t4, t5, t6);
  }
label1:;
  t1++; goto label0;
label2:;
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_w2_put(t7, t8, t9, t10);
  }
gg_done:;
}
static int64_t gg_function_105(int64_t p0, int64_t base) {
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
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(5))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_kv_get(t1, t2, t3);
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
  t9 = ((int64_t)(((int64_t)UINT64_C(6))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg_kv_get(t8, t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = ((t12) > (t13));
  if (t14) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_102(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
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
  uint8_t t41;
  int64_t t42;
  int64_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
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
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
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
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint8_t t114;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(62))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_kv_get(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) != (t6));
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(v0));
  v6 = gg_function_22(t8, base + 0);
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) > (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v6));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = ((t12) > (t13));
  gg_function_4(t14, base + 0);
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) != (t16));
  gg_function_4(t17, base + 0);
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(v1));
  fg_kv_sync(t18, t19);
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(7))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg_kv_get(t20, t21, t22);
  v7 = ((int64_t)(t23));
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t24 = ((int64_t)(v8));
  t25 = ((int64_t)(v3));
  if (!(((t24) < (t25)))) goto label2;
  {
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(((int64_t)UINT64_C(300))));
  t28 = ((int64_t)(v8));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg_kv_get(t26, t29, t30);
  v9 = ((int64_t)(t31));
  t32 = ((int64_t)(v4));
  t33 = ((int64_t)(((int64_t)UINT64_C(400))));
  t34 = ((int64_t)(v8));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = fg_kv_get(t32, t35, t36);
  v10 = ((int64_t)(t37));
  t38 = ((int64_t)(v9));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) >= (t39));
  t41 = t40;
  if (t41) {
  t42 = ((int64_t)(v9));
  t43 = ((int64_t)(v6));
  t41 = ((t42) < (t43));
  }
  t44 = t41;
  gg_function_4(t44, base + 0);
  t45 = ((int64_t)(v10));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((t45) >= (t46));
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(v10));
  t50 = ((int64_t)(v6));
  t48 = ((t49) < (t50));
  }
  t51 = t48;
  gg_function_4(t51, base + 0);
  t52 = ((int64_t)(v9));
  t53 = ((int64_t)(v10));
  t54 = ((t52) != (t53));
  gg_function_4(t54, base + 0);
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t55 = ((int64_t)(v11));
  t56 = ((int64_t)(v7));
  if (!(((t55) < (t56)))) goto label5;
  {
  t57 = ((int64_t)(v0));
  t58 = ((int64_t)(v9));
  t59 = ((int64_t)(v10));
  t60 = ((int64_t)(v11));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_tensor_copy(t57, t58, t59, t60, t61);
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(((int64_t)UINT64_C(400))));
  t64 = ((int64_t)(v11));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t64)))));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = fg_kv_get(t62, t65, t66);
  t68 = ((int64_t)(t67));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = ((t68) != (t69));
  if (t70) {
  {
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(v9));
  t73 = ((int64_t)(v10));
  t74 = ((int64_t)(v11));
  t75 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg_kv_tensor_copy(t71, t72, t73, t74, t75);
  }
  }
  t76 = v11;
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t76) + (uint64_t)(t77)));
  }
  goto label4;
label5:;
  t78 = v8;
  t79 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t78) + (uint64_t)(t79)));
  }
  goto label1;
label2:;
  }
  }
  t80 = ((int64_t)(v2));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = ((t80) != (t81));
  if (t82) {
  {
  t83 = ((int64_t)(v0));
  t84 = gg_function_105(t83, base + 0);
  t85 = ((int64_t)(t84));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = ((t85) != (t86));
  gg_function_4(t87, base + 0);
  t88 = ((int64_t)(v0));
  t89 = ((int64_t)(((int64_t)UINT64_C(61))));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = fg_kv_get(t88, t89, t90);
  t92 = ((int64_t)(t91));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((t92) == (t93));
  if (t94) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((int64_t)(v0));
  t96 = ((int64_t)(((int64_t)UINT64_C(7))));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = fg_kv_get(t95, t96, t97);
  v13 = ((int64_t)(t98));
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t99 = ((int64_t)(v14));
  t100 = ((int64_t)(v13));
  if (!(((t99) < (t100)))) goto label8;
  {
  t101 = ((int64_t)(v0));
  t102 = ((int64_t)(((int64_t)UINT64_C(500))));
  t103 = ((int64_t)(v14));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t103)))));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = fg_kv_get(t101, t104, t105);
  t107 = ((int64_t)(t106));
  t108 = ((int64_t)(((int64_t)UINT64_C(0))));
  t109 = ((t107) != (t108));
  if (t109) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t110 = v14;
  t111 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t110) + (uint64_t)(t111)));
  }
  goto label7;
label8:;
  t112 = ((int64_t)(v12));
  t113 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((t112) != (t113));
  if (t114) {
  {
  t115 = ((int64_t)(v0));
  t116 = ((int64_t)(v1));
  t117 = fg_kv_shift_compute(t115, t116);
  v15 = ((int64_t)(t117));
  t118 = ((int64_t)(v15));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((t118) != (t119));
  if (t120) {
  {
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
  }
  v5 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  }
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t121 = ((int64_t)(v16));
  t122 = ((int64_t)(v6));
  if (!(((t121) < (t122)))) goto label11;
  {
  t123 = ((int64_t)(v0));
  t124 = ((int64_t)(v16));
  t125 = gg_function_6(t123, t124, base + 0);
  gg_function_107(t125, base + 0);
  t126 = v16;
  t127 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)((uint64_t)(t126) + (uint64_t)(t127)));
  }
  goto label10;
label11:;
  }
  }
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_92(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = fg_kv_obj(t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(v0));
  fg_kv_put(t3, t4, t5, t6);
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_put(t7, t8, t9, t10);
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(3))));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_put(t11, t12, t13, t14);
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(4))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_put(t15, t16, t17, t18);
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(6))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_put(t19, t20, t21, t22);
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(7))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_put(t23, t24, t25, t26);
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_91(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  v3 = gg_function_92(t1, base + 0);
  t2 = ((int64_t)(v3));
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(v0));
  fg_kv_put(t2, t3, t4, t5);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg_kv_get(t6, t7, t8);
  v4 = ((int64_t)(t9));
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg_kv_get(t10, t11, t12);
  v5 = ((int64_t)(t13));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)UINT64_C(7))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((int64_t)(v4));
  fg_kv_put(t14, t15, t16, t17);
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(6))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(v5));
  fg_kv_put(t18, t19, t20, t21);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t22 = ((int64_t)(v6));
  t23 = ((int64_t)(v5));
  if (!(((t22) < (t23)))) goto label2;
  {
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(100))));
  t26 = ((int64_t)(v6));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(((int64_t)UINT64_C(10))));
  t31 = ((int64_t)(v6));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = fg_kv_get(t29, t32, t33);
  t35 = ((int64_t)(t34));
  fg_kv_put(t24, t27, t28, t35);
  t36 = v6;
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t36) + (uint64_t)(t37)));
  }
  goto label1;
label2:;
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t38 = ((int64_t)(v6));
  t39 = ((int64_t)(v4));
  if (!(((t38) < (t39)))) goto label5;
  {
  t40 = ((int64_t)(v3));
  t41 = ((int64_t)(((int64_t)UINT64_C(200))));
  t42 = ((int64_t)(v6));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t42)))));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(((int64_t)UINT64_C(10))));
  t47 = ((int64_t)(v6));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = fg_kv_get(t45, t48, t49);
  t51 = ((int64_t)(t50));
  fg_kv_put(t40, t43, t44, t51);
  t52 = v6;
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t52) + (uint64_t)(t53)));
  }
  goto label4;
label5:;
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_23(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  if (t2) {
  {
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(20))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg_w2_get(t3, t4, t5);
  t7 = ((int64_t)(t6));
  fg_w2_set_destroy(t7);
  v1 = gg_function_15(base + 0);
  t9 = v1;
  t8 = 0;
label0:;
  if (t8 >= t9) goto label2;
  v2 = t8;
  {
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(25))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = fg_w2_get(t10, t13, t14);
  t16 = ((int64_t)(t15));
  fg_w2_set_destroy(t16);
  }
label1:;
  t8++; goto label0;
label2:;
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg_w2_service(t17, t18, t19);
  v3 = ((int64_t)(t20));
  }
  }
  }
gg_done:;
}
static void gg_function_21(int64_t p0, int64_t base) {
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
  int64_t t11;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  if (t2) {
  {
  t3 = ((int64_t)(v0));
  v1 = gg_function_22(t3, base + 0);
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v1));
  if (!(((t4) < (t5)))) goto label2;
  {
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v2));
  t8 = gg_function_6(t6, t7, base + 0);
  gg_function_23(t8, base + 0);
  t9 = v2;
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t9) + (uint64_t)(t10)));
  }
  goto label1;
label2:;
  t11 = ((int64_t)(v0));
  fg_kv_destroy(t11);
  }
  }
  }
gg_done:;
}
static void gg_function_116(int64_t p0, int64_t base) {
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
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  if (t2) {
  {
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(6))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg_kv_get(t3, t4, t5);
  v1 = ((int64_t)(t6));
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(v1));
  if (!(((t7) < (t8)))) goto label2;
  {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(100))));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg_kv_get(t9, t12, t13);
  v3 = ((int64_t)(t14));
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) != (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v3));
  fg_kv_ub_destroy(t18);
  }
  }
  t19 = v2;
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  }
  goto label1;
label2:;
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(7))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg_kv_get(t21, t22, t23);
  v4 = ((int64_t)(t24));
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(v4));
  if (!(((t25) < (t26)))) goto label5;
  {
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(200))));
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = fg_kv_get(t27, t30, t31);
  v5 = ((int64_t)(t32));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) != (t34));
  if (t35) {
  {
  t36 = ((int64_t)(v5));
  fg_kv_destroy(t36);
  }
  }
  t37 = v2;
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t37) + (uint64_t)(t38)));
  }
  goto label4;
label5:;
  t39 = ((int64_t)(v0));
  fg_kv_destroy(t39);
  }
  }
  }
gg_done:;
}
static int64_t gg_function_100(int64_t p0, int64_t base) {
  /* FG_SHARED_BODY:22 */
  return gg_function_22(p0, base);
}
static int64_t gg_function_111(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t0 = gg_function_22(t1, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_84(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t12;
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
  uint8_t t26;
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
  uint8_t t51;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  v4 = gg_function_56(t0, base + 0);
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v5));
  t2 = ((int64_t)(v4));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v5));
  v6 = gg_function_58(t3, t4, t5, base + 0);
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(11))));
  t8 = ((int64_t)(v6));
  t9 = fg_w2_get(t6, t7, t8);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t10) == (t11));
  gg_function_4(t12, base + 0);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v6));
  t15 = gg_function_37(t13, t14, base + 0);
  t16 = ((int64_t)(t15));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(v6));
  gg_function_71(t19, t20, base + 0);
  }
  }
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v5));
  t23 = gg_function_37(t21, t22, base + 0);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) == (t25));
  if (t26) {
  {
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(v6));
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(v5));
  t31 = gg_function_42(t29, t30, base + 0);
  gg_function_59(t27, t28, t31, base + 0);
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(v6));
  t34 = ((int64_t)(v1));
  t35 = ((int64_t)(v5));
  t36 = gg_function_62(t34, t35, base + 0);
  t37 = ((int64_t)(v1));
  t38 = ((int64_t)(v5));
  t39 = gg_function_63(t37, t38, base + 0);
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(v5));
  t42 = gg_function_64(t40, t41, base + 0);
  gg_function_65(t32, t33, t36, t39, t42, base + 0);
  v7 = gg_function_15(base + 0);
  t44 = v7;
  t43 = 0;
label3:;
  if (t43 >= t44) goto label5;
  v8 = t43;
  {
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(v5));
  t47 = ((int64_t)(v8));
  t48 = gg_function_66(t45, t46, t47, base + 0);
  t49 = ((int64_t)(t48));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((t49) != (t50));
  if (t51) {
  {
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(v6));
  t54 = ((int64_t)(v8));
  gg_function_67(t52, t53, t54, base + 0);
  }
  }
  }
label4:;
  t43++; goto label3;
label5:;
  }
  }
  t55 = v5;
  t56 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t55) + (uint64_t)(t56)));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_57(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_w2_service(t1, t2, t3);
  v1 = ((int64_t)(t4));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) != (t6));
  gg_function_4(t7, base + 0);
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(v0));
  fg_w2_put(t8, t9, t10, t11);
  t12 = ((int64_t)(v1));
  gg_function_9(t12, base + 0);
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_55(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t45;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  v3 = gg_function_56(t1, base + 0);
  t2 = ((int64_t)(v3));
  v4 = gg_function_57(t2, base + 0);
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v5));
  t4 = ((int64_t)(v3));
  if (!(((t3) < (t4)))) goto label2;
  {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(v5));
  v6 = gg_function_58(t5, t6, t7, base + 0);
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(11))));
  t10 = ((int64_t)(v6));
  t11 = fg_w2_get(t8, t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) == (t13));
  gg_function_4(t14, base + 0);
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(v6));
  t17 = gg_function_37(t15, t16, base + 0);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t18) == (t19));
  if (t20) {
  {
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(v6));
  t25 = gg_function_42(t23, t24, base + 0);
  gg_function_59(t21, t22, t25, base + 0);
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(v5));
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(v6));
  t30 = gg_function_62(t28, t29, base + 0);
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(v6));
  t33 = gg_function_63(t31, t32, base + 0);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(v6));
  t36 = gg_function_64(t34, t35, base + 0);
  gg_function_65(t26, t27, t30, t33, t36, base + 0);
  v7 = gg_function_15(base + 0);
  t38 = v7;
  t37 = 0;
label3:;
  if (t37 >= t38) goto label5;
  v8 = t37;
  {
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(v6));
  t41 = ((int64_t)(v8));
  t42 = gg_function_66(t39, t40, t41, base + 0);
  t43 = ((int64_t)(t42));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = ((t43) != (t44));
  if (t45) {
  {
  t46 = ((int64_t)(v4));
  t47 = ((int64_t)(v5));
  t48 = ((int64_t)(v8));
  gg_function_67(t46, t47, t48, base + 0);
  }
  }
  }
label4:;
  t37++; goto label3;
label5:;
  }
  }
  t49 = v5;
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t49) + (uint64_t)(t50)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_51(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
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
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(4))));
  t2 = fg_kv_obj(t1);
  v2 = ((int64_t)(t2));
  t3 = ((int64_t)(((int64_t)UINT64_C(4))));
  t4 = fg_kv_obj(t3);
  v3 = ((int64_t)(t4));
  v4 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg_kv_get(t5, t6, t7);
  v6 = ((int64_t)(t8));
label1:;
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(v6));
  if (!(((t9) < (t10)))) goto label2;
  {
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(10))));
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = fg_kv_get(t11, t14, t15);
  v7 = ((int64_t)(t16));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(v7));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  v8 = gg_function_26(t17, t18, t19, base + 0);
  t20 = ((int64_t)(v8));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = ((int64_t)(v6));
  }
  } else {
  {
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(v8));
  fg_kv_vec_push(t23, t24);
  t25 = ((int64_t)(((int64_t)UINT64_C(5))));
  t26 = fg_kv_obj(t25);
  v9 = ((int64_t)(t26));
  t27 = ((int64_t)(v9));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((int64_t)(v8));
  fg_kv_put(t27, t28, t29, t30);
  t31 = ((int64_t)(v8));
  v10 = gg_function_53(t31, base + 0);
  t32 = ((int64_t)(v0));
  v11 = gg_function_22(t32, base + 0);
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t33 = ((int64_t)(v12));
  t34 = ((int64_t)(v11));
  if (!(((t33) < (t34)))) goto label5;
  {
  t35 = ((int64_t)(v9));
  t36 = ((int64_t)(((int64_t)UINT64_C(10))));
  t37 = ((int64_t)(v12));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(v12));
  t42 = gg_function_34(t40, t41, base + 0);
  fg_kv_put(t35, t38, t39, t42);
  t43 = v12;
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  }
  goto label4;
label5:;
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t45 = ((int64_t)(v12));
  t46 = ((int64_t)(v10));
  if (!(((t45) < (t46)))) goto label8;
  {
  t47 = ((int64_t)(v8));
  t48 = ((int64_t)(v12));
  v13 = gg_function_6(t47, t48, base + 0);
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(v13));
  v14 = gg_function_6(t49, t50, base + 0);
  t51 = ((int64_t)(v14));
  t52 = ((int64_t)(v8));
  t53 = ((int64_t)(v12));
  v15 = gg_function_55(t51, t52, t53, base + 0);
  t54 = ((int64_t)(v9));
  t55 = ((int64_t)(((int64_t)UINT64_C(100))));
  t56 = ((int64_t)(v12));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(v15));
  fg_kv_put(t54, t57, t58, t59);
  t60 = v12;
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t60) + (uint64_t)(t61)));
  }
  goto label7;
label8:;
  t62 = ((int64_t)(v3));
  t63 = ((int64_t)(v9));
  fg_kv_vec_push(t62, t63);
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(v8));
  t66 = ((int64_t)(v7));
  gg_function_70(t64, t65, t66, base + 0);
  t67 = v5;
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t67) + (uint64_t)(t68)));
  }
  }
  }
  goto label1;
label2:;
  t69 = ((int64_t)(v3));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = fg_kv_get(t69, t70, t71);
  v16 = ((int64_t)(t72));
label10:;
  t73 = ((int64_t)(v16));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t73) > (t74)))) goto label11;
  {
  t75 = v16;
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)((uint64_t)(t75) - (uint64_t)(t76)));
  t77 = ((int64_t)(v3));
  t78 = ((int64_t)(((int64_t)UINT64_C(10))));
  t79 = ((int64_t)(v16));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t79)))));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = fg_kv_get(t77, t80, t81);
  v17 = ((int64_t)(t82));
  t83 = ((int64_t)(v17));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = fg_kv_get(t83, t84, t85);
  v18 = ((int64_t)(t86));
  t87 = ((int64_t)(v18));
  v19 = gg_function_53(t87, base + 0);
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t88 = ((int64_t)(v20));
  t89 = ((int64_t)(v19));
  if (!(((t88) < (t89)))) goto label14;
  {
  t90 = ((int64_t)(v18));
  t91 = ((int64_t)(v20));
  v21 = gg_function_6(t90, t91, base + 0);
  t92 = ((int64_t)(v0));
  t93 = ((int64_t)(v21));
  v22 = gg_function_6(t92, t93, base + 0);
  t94 = ((int64_t)(v17));
  t95 = ((int64_t)(((int64_t)UINT64_C(100))));
  t96 = ((int64_t)(v20));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t95) + (uint64_t)(t96)))));
  t98 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = fg_kv_get(t94, t97, t98);
  v23 = ((int64_t)(t99));
  t100 = ((int64_t)(v22));
  t101 = ((int64_t)(v23));
  t102 = ((int64_t)(v18));
  t103 = ((int64_t)(v20));
  gg_function_84(t100, t101, t102, t103, base + 0);
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(v21));
  t106 = ((int64_t)(v17));
  t107 = ((int64_t)(((int64_t)UINT64_C(10))));
  t108 = ((int64_t)(v20));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t107) + (uint64_t)(t108)))));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = fg_kv_get(t106, t109, t110);
  t112 = ((int64_t)(t111));
  gg_function_19(t104, t105, t112, base + 0);
  t113 = v20;
  t114 = ((int64_t)(((int64_t)UINT64_C(1))));
  v20 = ((int64_t)((uint64_t)(t113) + (uint64_t)(t114)));
  }
  goto label13;
label14:;
  t115 = ((int64_t)(v17));
  fg_kv_destroy(t115);
  }
  goto label10;
label11:;
  t116 = ((int64_t)(v4));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t118 = ((t116) == (t117));
  if (t118) {
  {
  t119 = ((int64_t)(v2));
  fg_kv_destroy(t119);
  t120 = ((int64_t)(v3));
  fg_kv_destroy(t120);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t121 = ((int64_t)(v3));
  fg_kv_destroy(t121);
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_85(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
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
  v3 = p3;
  {
  t1 = ((int64_t)(v1));
  fg_kv_balloc_reset(t1);
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = fg_kv_obj(t2);
  v4 = ((int64_t)(t3));
  v5 = ((int64_t)(((int64_t)UINT64_C(2))));
  t4 = ((int64_t)(v0));
  t5 = gg_function_22(t4, base + 0);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((t6) == (t7));
  if (t8) {
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) != (t10));
  if (t11) {
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  }
  v6 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t12 = ((int64_t)(v6));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  if (!(((t12) == (t13)))) goto label2;
  {
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(v5));
  t17 = fg_kv_balloc_split(t14, t15, t16);
  v7 = ((int64_t)(t17));
  t18 = ((int64_t)(v7));
  t19 = fg_kv_ub_n_tokens(t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  t23 = ((int64_t)(v7));
  fg_kv_ub_destroy(t23);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(v7));
  fg_kv_vec_push(t24, t25);
  }
  }
  }
  goto label1;
label2:;
  v8 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((int64_t)(v1));
  t27 = fg_kv_balloc_n_used(t26);
  t28 = ((int64_t)(t27));
  t29 = ((int64_t)(v1));
  t30 = fg_kv_balloc_n_tokens(t29);
  t31 = ((int64_t)(t30));
  t32 = ((t28) < (t31));
  if (t32) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t33 = ((int64_t)(v8));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) != (t34));
  if (t35) {
  {
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(v4));
  v9 = gg_function_51(t36, t37, base + 0);
  t38 = ((int64_t)(v9));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) != (t39));
  if (t40) {
  {
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(v9));
  t43 = ((int64_t)(v4));
  v10 = gg_function_91(t41, t42, t43, base + 0);
  t44 = ((int64_t)(v9));
  fg_kv_destroy(t44);
  t45 = ((int64_t)(v4));
  fg_kv_destroy(t45);
  t0 = ((int64_t)(v10));
  goto gg_done;
  }
  }
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t46 = ((int64_t)(v4));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = fg_kv_get(t46, t47, t48);
  v11 = ((int64_t)(t49));
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t50 = ((int64_t)(v12));
  t51 = ((int64_t)(v11));
  if (!(((t50) < (t51)))) goto label5;
  {
  t52 = ((int64_t)(v4));
  t53 = ((int64_t)(((int64_t)UINT64_C(10))));
  t54 = ((int64_t)(v12));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = fg_kv_get(t52, t55, t56);
  t58 = ((int64_t)(t57));
  fg_kv_ub_destroy(t58);
  t59 = v12;
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t59) + (uint64_t)(t60)));
  }
  goto label4;
label5:;
  t61 = ((int64_t)(v4));
  fg_kv_destroy(t61);
  t62 = ((int64_t)(((int64_t)UINT64_C(2))));
  t0 = gg_function_92(t62, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_98(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t56;
  uint8_t t57;
  int64_t t58;
  int64_t t59;
  uint8_t t60;
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
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  v3 = gg_function_92(t1, base + 0);
  t2 = ((int64_t)(v3));
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(v0));
  fg_kv_put(t2, t3, t4, t5);
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(v1));
  fg_kv_put(t6, t7, t8, t9);
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(5))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((int64_t)(v2));
  fg_kv_put(t10, t11, t12, t13);
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(65))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg_kv_get(t14, t15, t16);
  v4 = ((int64_t)(t17));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(8))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(v4));
  fg_kv_put(t18, t19, t20, t21);
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(v4));
  if (!(((t22) < (t23)))) goto label2;
  {
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(300))));
  t26 = ((int64_t)(v5));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(200))));
  t31 = ((int64_t)(v5));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = fg_kv_get(t29, t32, t33);
  t35 = ((int64_t)(t34));
  fg_kv_put(t24, t27, t28, t35);
  t36 = ((int64_t)(v3));
  t37 = ((int64_t)(((int64_t)UINT64_C(400))));
  t38 = ((int64_t)(v5));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(300))));
  t43 = ((int64_t)(v5));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = fg_kv_get(t41, t44, t45);
  t47 = ((int64_t)(t46));
  fg_kv_put(t36, t39, t40, t47);
  t48 = v5;
  t49 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t48) + (uint64_t)(t49)));
  }
  goto label1;
label2:;
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(65))));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_put(t50, t51, t52, t53);
  t54 = ((int64_t)(v2));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = ((t54) == (t55));
  t57 = t56;
  if (t57) {
  t58 = ((int64_t)(v4));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t58) == (t59));
  }
  t60 = t57;
  if (t60) {
  {
  t61 = ((int64_t)(v3));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg_kv_put(t61, t62, t63, t64);
  }
  }
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_97(int64_t p0, int64_t base) {
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
  uint8_t t12;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  v1 = gg_function_22(t1, base + 0);
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = v1;
  t2 = 0;
label0:;
  if (t2 >= t3) goto label2;
  v3 = t2;
  {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v3));
  t6 = gg_function_6(t4, t5, base + 0);
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg_w2_get(t6, t7, t8);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t10) != (t11));
  if (t12) {
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
label1:;
  t2++; goto label0;
label2:;
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_96(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v0));
  t4 = gg_function_97(t3, base + 0);
  t0 = gg_function_98(t1, t2, t4, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_113(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_kv_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_95(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = gg_function_6(t1, t2, base + 0);
  t0 = gg_function_10(t3, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_94(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  v1 = gg_function_92(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(v0));
  fg_kv_put(t2, t3, t4, t5);
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(v0));
  t10 = gg_function_95(t9, base + 0);
  fg_kv_put(t6, t7, t8, t10);
  t11 = ((int64_t)(v0));
  v2 = gg_function_22(t11, base + 0);
  t12 = ((int64_t)(((int64_t)UINT64_C(3))));
  t13 = fg_kv_obj(t12);
  v3 = ((int64_t)(t13));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_put(t14, t15, t16, t17);
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) - (uint64_t)(t22)))));
  fg_kv_put(t18, t19, t20, t23);
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(2))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg_kv_put(t24, t25, t26, t27);
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)UINT64_C(3))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(v2));
  fg_kv_put(t28, t29, t30, t31);
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t32 = ((int64_t)(v4));
  t33 = ((int64_t)(v2));
  if (!(((t32) < (t33)))) goto label2;
  {
  t34 = ((int64_t)(v3));
  t35 = ((int64_t)(((int64_t)UINT64_C(10))));
  t36 = ((int64_t)(v4));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((int64_t)(v4));
  fg_kv_put(t34, t37, t38, t39);
  t40 = ((int64_t)(v3));
  t41 = ((int64_t)(v4));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_slot_push(t40, t41, t42);
  t43 = v4;
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  }
  goto label1;
label2:;
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(((int64_t)UINT64_C(7))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg_kv_put(t45, t46, t47, t48);
  t49 = ((int64_t)(v1));
  t50 = ((int64_t)(((int64_t)UINT64_C(200))));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((int64_t)(v3));
  fg_kv_put(t49, t50, t51, t52);
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_93(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t0 = gg_function_94(t1, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_3(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((t1) >= (t2));
  gg_function_4(t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = fg_kv_create(t4, t5);
  v2 = ((int64_t)(t6));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(v0));
  if (!(((t7) < (t8)))) goto label2;
  {
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(v3));
  v4 = gg_function_6(t9, t10, base + 0);
  t11 = ((int64_t)(v4));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((int64_t)(v1));
  fg_w2_put(t11, t12, t13, t14);
  t15 = ((int64_t)(v4));
  gg_function_9(t15, base + 0);
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_19(t16, t17, t18, base + 0);
  t19 = v3;
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_110(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t10;
  int64_t t11;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  gg_function_4(t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)gg_div(t8, t9))));
  t11 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_109(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(20))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_w2_get(t1, t2, t3);
  v1 = ((int64_t)(t4));
  t5 = ((int64_t)(v1));
  t6 = fg_w2_set_size(t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(v1));
  t12 = fg_w2_set_last(t11);
  t13 = ((int64_t)(t12));
  t14 = fg_w2_set_key(t10, t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_108(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
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
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_kv_get(t1, t2, t3);
  v2 = ((int64_t)(t4));
  v3 = ((int64_t)(v2));
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)UINT64_C(256))));
  t7 = ((t5) < (t6));
  if (t7) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(256))));
  }
  }
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(v1));
  v5 = gg_function_53(t8, base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(v5));
  if (!(((t9) < (t10)))) goto label2;
  {
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v6));
  t14 = gg_function_6(t12, t13, base + 0);
  v7 = gg_function_6(t11, t14, base + 0);
  t15 = ((int64_t)(v7));
  v8 = gg_function_10(t15, base + 0);
  t16 = ((int64_t)(v7));
  t17 = gg_function_109(t16, base + 0);
  t18 = ((int64_t)(v3));
  v9 = gg_function_110(t17, t18, base + 0);
  t19 = ((int64_t)(v9));
  t20 = ((int64_t)(v3));
  t21 = ((t19) < (t20));
  if (t21) {
  {
  v9 = ((int64_t)(v3));
  }
  }
  v10 = ((int64_t)(v8));
  t22 = ((int64_t)(v9));
  t23 = ((int64_t)(v10));
  t24 = ((t22) < (t23));
  if (t24) {
  {
  v10 = ((int64_t)(v9));
  }
  }
  t25 = ((int64_t)(v10));
  t26 = ((int64_t)(v4));
  t27 = ((t25) > (t26));
  if (t27) {
  {
  v4 = ((int64_t)(v10));
  }
  }
  t28 = v6;
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t28) + (uint64_t)(t29)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_101(int64_t p0, int64_t base) {
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
  v1 = gg_function_22(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(2))));
  t4 = ((t2) == (t3));
  t5 = t4;
  if (!t5) {
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(3))));
  t5 = ((t6) == (t7));
  }
  t8 = t5;
  t9 = (!(t8));
  gg_function_4(t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg_kv_get(t10, t11, t12);
  v2 = ((int64_t)(t13));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(6))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg_kv_get(t14, t15, t16);
  v3 = ((int64_t)(t17));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t18) == (t19));
  if (t20) {
  {
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = fg_kv_get(t22, t23, t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(5))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = fg_kv_get(t27, t28, t29);
  t31 = ((int64_t)(t30));
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(8))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg_kv_get(t32, t33, t34);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(v0));
  t38 = gg_function_102(t21, t26, t31, t36, t37, base + 0);
  (void)(t38);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(3))));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = fg_kv_get(t39, t40, t41);
  v4 = ((int64_t)(t42));
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(200))));
  t45 = ((int64_t)(v4));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = fg_kv_get(t43, t46, t47);
  v5 = ((int64_t)(t48));
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(100))));
  t51 = ((int64_t)(v4));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t51)))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = fg_kv_get(t49, t52, t53);
  v6 = ((int64_t)(t54));
  t55 = ((int64_t)(v2));
  t56 = ((int64_t)(v5));
  t57 = ((int64_t)(v6));
  gg_function_70(t55, t56, t57, base + 0);
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(((int64_t)UINT64_C(4))));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((int64_t)(v2));
  t62 = ((int64_t)(v5));
  t63 = gg_function_108(t61, t62, base + 0);
  fg_kv_put(t58, t59, t60, t63);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_117(int64_t base) {
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
  uint8_t t20;
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
  uint8_t t42;
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
  uint8_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
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
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  v0 = gg_function_3(t1, t2, base + 0);
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = fg_kv_test_ubatch(t3);
  v1 = ((int64_t)(t4));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_test_ub_set_seq(t5, t6, t7, t8);
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_test_ub_set_seq_unq(t9, t10, t11);
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_test_ub_set_pos(t12, t13, t14);
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  v2 = gg_function_26(t15, t16, t17, base + 0);
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t18) != (t19));
  gg_function_4(t20, base + 0);
  t21 = ((int64_t)(v2));
  t22 = gg_function_56(t21, base + 0);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) == (t24));
  gg_function_4(t25, base + 0);
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = gg_function_58(t26, t27, t28, base + 0);
  t30 = ((int64_t)(t29));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((t30) == (t31));
  gg_function_4(t32, base + 0);
  t33 = ((int64_t)(v2));
  fg_kv_destroy(t33);
  t34 = ((int64_t)(((int64_t)UINT64_C(4))));
  t35 = fg_kv_obj(t34);
  v3 = ((int64_t)(t35));
  t36 = ((int64_t)(v3));
  t37 = ((int64_t)(v1));
  fg_kv_vec_push(t36, t37);
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(v3));
  v4 = gg_function_51(t38, t39, base + 0);
  t40 = ((int64_t)(v4));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t40) != (t41));
  gg_function_4(t42, base + 0);
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = gg_function_6(t43, t44, base + 0);
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = gg_function_37(t45, t46, base + 0);
  t48 = ((int64_t)(t47));
  t49 = ((int64_t)(((int64_t)UINT64_C(1))));
  t50 = ((t48) == (t49));
  gg_function_4(t50, base + 0);
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(v4));
  t53 = ((int64_t)(v3));
  v5 = gg_function_91(t51, t52, t53, base + 0);
  t54 = ((int64_t)(v4));
  fg_kv_destroy(t54);
  t55 = ((int64_t)(v3));
  fg_kv_destroy(t55);
  t56 = ((int64_t)(v5));
  t57 = gg_function_111(t56, base + 0);
  t58 = ((int64_t)(t57));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((t58) == (t59));
  gg_function_4(t60, base + 0);
  t61 = ((int64_t)(v5));
  t62 = gg_function_101(t61, base + 0);
  t63 = ((int64_t)(t62));
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  t65 = ((t63) == (t64));
  gg_function_4(t65, base + 0);
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = gg_function_6(t66, t67, base + 0);
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = gg_function_37(t68, t69, base + 0);
  t71 = ((int64_t)(t70));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((t71) == (t72));
  gg_function_4(t73, base + 0);
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = gg_function_6(t74, t75, base + 0);
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = gg_function_42(t76, t77, base + 0);
  t79 = ((int64_t)(t78));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = ((t79) == (t80));
  gg_function_4(t81, base + 0);
  t82 = ((int64_t)(v0));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = gg_function_6(t82, t83, base + 0);
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = gg_function_66(t84, t85, t86, base + 0);
  t88 = ((int64_t)(t87));
  t89 = ((int64_t)(((int64_t)UINT64_C(1))));
  t90 = ((t88) == (t89));
  gg_function_4(t90, base + 0);
  t91 = ((int64_t)(((int64_t)UINT64_C(1))));
  t92 = ((int64_t)(((int64_t)UINT64_C(2))));
  v6 = gg_function_3(t91, t92, base + 0);
  t93 = ((int64_t)(v6));
  v7 = gg_function_93(t93, base + 0);
  t94 = ((int64_t)(v7));
  t95 = gg_function_113(t94, base + 0);
  t96 = ((int64_t)(t95));
  t97 = ((int64_t)(((int64_t)UINT64_C(2))));
  t98 = ((t96) == (t97));
  gg_function_4(t98, base + 0);
  t99 = ((int64_t)(v6));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  v8 = gg_function_96(t99, t100, base + 0);
  t101 = ((int64_t)(v8));
  t102 = gg_function_111(t101, base + 0);
  t103 = ((int64_t)(t102));
  t104 = ((int64_t)(((int64_t)UINT64_C(1))));
  t105 = ((t103) == (t104));
  gg_function_4(t105, base + 0);
  t106 = ((int64_t)(((int64_t)UINT64_C(1))));
  t107 = fg_kv_test_ubatch(t106);
  v9 = ((int64_t)(t107));
  t108 = ((int64_t)(v9));
  t109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_test_ub_set_seq(t108, t109, t110, t111);
  t112 = ((int64_t)(v9));
  t113 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_test_ub_set_seq_unq(t112, t113, t114);
  t115 = ((int64_t)(v9));
  t116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg_kv_test_ub_set_pos(t115, t116, t117);
  t118 = ((int64_t)(v9));
  t119 = fg_kv_test_balloc(t118);
  v10 = ((int64_t)(t119));
  t120 = ((int64_t)(v6));
  t121 = ((int64_t)(v10));
  t122 = ((int64_t)(((int64_t)UINT64_C(1))));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = gg_function_85(t120, t121, t122, t123, base + 0);
  t124 = ((int64_t)(v11));
  t125 = gg_function_111(t124, base + 0);
  t126 = ((int64_t)(t125));
  t127 = ((int64_t)(((int64_t)UINT64_C(0))));
  t128 = ((t126) == (t127));
  gg_function_4(t128, base + 0);
  t129 = ((int64_t)(v11));
  gg_function_116(t129, base + 0);
  t130 = ((int64_t)(v10));
  fg_kv_balloc_destroy(t130);
  t131 = ((int64_t)(v8));
  gg_function_116(t131, base + 0);
  t132 = ((int64_t)(v7));
  gg_function_116(t132, base + 0);
  t133 = ((int64_t)(v5));
  gg_function_116(t133, base + 0);
  t134 = ((int64_t)(v6));
  gg_function_21(t134, base + 0);
  t135 = ((int64_t)(v0));
  gg_function_21(t135, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_115(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_kv_get(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(64))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg_kv_get(t5, t6, t7);
  t0 = ((int64_t)(t8));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_114(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_kv_get(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(63))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg_kv_get(t5, t6, t7);
  t0 = ((int64_t)(t8));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_112(int64_t p0, int64_t base) {
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
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_22(t1, base + 0);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) == (t4));
  gg_function_4(t5, base + 0);
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(100))));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(3))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg_kv_get(t8, t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = fg_kv_get(t6, t13, t14);
  t0 = ((int64_t)(t15));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_99(int64_t p0, int64_t base) {
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
  uint8_t t22;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_22(t1, base + 0);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) == (t4));
  gg_function_4(t5, base + 0);
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(3))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg_kv_get(t6, t7, t8);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(3))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(v1));
  fg_kv_put(t12, t13, t14, t15);
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(6))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg_kv_get(t17, t18, t19);
  t21 = ((int64_t)(t20));
  t22 = ((t16) >= (t21));
  if (t22) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
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
  uint8_t t12;
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
  uint8_t t27;
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
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  uint8_t t44;
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
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
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
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint8_t t103;
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
  uint8_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  uint8_t t122;
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
  int64_t t136;
  uint8_t t137;
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
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(100))));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg_kv_service(t1, t2, t3);
  (void)(t4);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(v2));
  t0 = gg_function_3(t8, t9, base + 0);
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t13 = ((int64_t)(v1));
  gg_function_21(t13, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
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
  t0 = gg_function_26(t17, t18, t19, base + 0);
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
  t0 = gg_function_51(t23, t24, base + 0);
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(4))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(v3));
  gg_function_70(t28, t29, t30, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(5))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t34 = ((int64_t)(v1));
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)(v3));
  t37 = ((int64_t)(v4));
  t0 = gg_function_85(t34, t35, t36, t37, base + 0);
  goto gg_done;
  }
  }
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(6))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  t41 = ((int64_t)(v1));
  t0 = gg_function_93(t41, base + 0);
  goto gg_done;
  }
  }
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(7))));
  t44 = ((t42) == (t43));
  if (t44) {
  {
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(v2));
  t0 = gg_function_96(t45, t46, base + 0);
  goto gg_done;
  }
  }
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(8))));
  t49 = ((t47) == (t48));
  if (t49) {
  {
  t50 = ((int64_t)(v1));
  t0 = gg_function_99(t50, base + 0);
  goto gg_done;
  }
  }
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(9))));
  t53 = ((t51) == (t52));
  if (t53) {
  {
  t54 = ((int64_t)(v1));
  t0 = gg_function_101(t54, base + 0);
  goto gg_done;
  }
  }
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(10))));
  t57 = ((t55) == (t56));
  if (t57) {
  {
  t58 = ((int64_t)(v1));
  t0 = gg_function_111(t58, base + 0);
  goto gg_done;
  }
  }
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(11))));
  t61 = ((t59) == (t60));
  if (t61) {
  {
  t62 = ((int64_t)(v1));
  t0 = gg_function_112(t62, base + 0);
  goto gg_done;
  }
  }
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(((int64_t)UINT64_C(12))));
  t65 = ((t63) == (t64));
  if (t65) {
  {
  t66 = ((int64_t)(v1));
  t0 = gg_function_113(t66, base + 0);
  goto gg_done;
  }
  }
  t67 = ((int64_t)(v0));
  t68 = ((int64_t)(((int64_t)UINT64_C(13))));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  t70 = ((int64_t)(v1));
  t0 = gg_function_114(t70, base + 0);
  goto gg_done;
  }
  }
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(((int64_t)UINT64_C(14))));
  t73 = ((t71) == (t72));
  if (t73) {
  {
  t74 = ((int64_t)(v1));
  t0 = gg_function_115(t74, base + 0);
  goto gg_done;
  }
  }
  t75 = ((int64_t)(v0));
  t76 = ((int64_t)(((int64_t)UINT64_C(15))));
  t77 = ((t75) == (t76));
  if (t77) {
  {
  t78 = ((int64_t)(v1));
  gg_function_116(t78, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t79 = ((int64_t)(v0));
  t80 = ((int64_t)(((int64_t)UINT64_C(16))));
  t81 = ((t79) == (t80));
  if (t81) {
  {
  t82 = ((int64_t)(v1));
  t83 = ((int64_t)(v2));
  t84 = ((int64_t)(v3));
  t85 = ((int64_t)(v4));
  t86 = ((int64_t)(v5));
  t0 = gg_function_102(t82, t83, t84, t85, t86, base + 0);
  goto gg_done;
  }
  }
  t87 = ((int64_t)(v0));
  t88 = ((int64_t)(((int64_t)UINT64_C(17))));
  t89 = ((t87) == (t88));
  if (t89) {
  {
  t90 = ((int64_t)(v1));
  t91 = ((int64_t)(v2));
  t0 = gg_function_108(t90, t91, base + 0);
  goto gg_done;
  }
  }
  t92 = ((int64_t)(v0));
  t93 = ((int64_t)(((int64_t)UINT64_C(18))));
  t94 = ((t92) == (t93));
  if (t94) {
  {
  t0 = gg_function_117(base + 0);
  goto gg_done;
  }
  }
  t95 = ((int64_t)(v0));
  t96 = ((int64_t)(((int64_t)UINT64_C(19))));
  t97 = ((t95) == (t96));
  if (t97) {
  {
  t98 = ((int64_t)(v1));
  t99 = ((int64_t)(v2));
  t100 = ((int64_t)(v3));
  t0 = gg_function_91(t98, t99, t100, base + 0);
  goto gg_done;
  }
  }
  t101 = ((int64_t)(v0));
  t102 = ((int64_t)(((int64_t)UINT64_C(20))));
  t103 = ((t101) == (t102));
  if (t103) {
  {
  t104 = ((int64_t)(v1));
  t0 = gg_function_92(t104, base + 0);
  goto gg_done;
  }
  }
  t105 = ((int64_t)(v0));
  t106 = ((int64_t)(((int64_t)UINT64_C(21))));
  t107 = ((t105) == (t106));
  if (t107) {
  {
  t108 = ((int64_t)(v1));
  t109 = ((int64_t)(v2));
  t110 = ((int64_t)(v3));
  t111 = ((int64_t)(v4));
  t112 = ((int64_t)(v5));
  t0 = gg_function_102(t108, t109, t110, t111, t112, base + 0);
  goto gg_done;
  }
  }
  t113 = ((int64_t)(v0));
  t114 = ((int64_t)(((int64_t)UINT64_C(22))));
  t115 = ((t113) == (t114));
  if (t115) {
  {
  t116 = ((int64_t)(v1));
  t117 = ((int64_t)(v2));
  t118 = ((int64_t)(v3));
  t119 = ((int64_t)(v4));
  gg_function_124(t116, t117, t118, t119, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t120 = ((int64_t)(v0));
  t121 = ((int64_t)(((int64_t)UINT64_C(23))));
  t122 = ((t120) == (t121));
  if (t122) {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t123 = ((int64_t)(v5));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t125 = ((t123) != (t124));
  if (t125) {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((int64_t)(v6));
  t127 = ((int64_t)(((int64_t)UINT64_C(0))));
  t128 = ((t126) != (t127));
  if (t128) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t129 = ((int64_t)(v1));
  t130 = ((int64_t)(v2));
  t131 = ((int64_t)(v3));
  t132 = ((int64_t)(v4));
  t133 = ((int64_t)(v5));
  t134 = ((int64_t)(v7));
  t135 = ((int64_t)(v6));
  t136 = ((int64_t)(v8));
  gg_function_140(t129, t130, t131, t132, t133, t134, t135, t136, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t137 = ((uint8_t)UINT64_C(0));
  gg_function_4(t137, base + 0);
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
  int64_t t7;
  int64_t t8;
  int64_t t9;
  {
  t0.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(18))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = gg_function_1(t1, t2, t3, t4, t5, t6, t7, base + 1);
  gg_integer(gg_ref(&t0, gg_get_stack(base + 0)), t8);
  gg_append(gg_ref(&t0, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t0).base, (t0).len));
  }
gg_done:;
  for (t9 = 0; t9 < 1; t9++) gg_stacks[base + t9].top = 0;
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
