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
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t base);
static int64_t gg_function_6(int64_t base);
static int64_t gg_function_7(int64_t base);
static int64_t gg_function_9(int64_t base);
static int64_t gg_function_10(int64_t base);
static int64_t gg_function_12(int64_t base);
static int64_t gg_function_14(int64_t base);
static int64_t gg_function_15(int64_t base);
static int64_t gg_function_18(int64_t base);
static int64_t gg_function_19(int64_t base);
static int64_t gg_function_23(int64_t base);
static int64_t gg_function_24(int64_t base);
static int64_t gg_function_25(int64_t base);
static int64_t gg_function_26(int64_t base);
static uint8_t gg_function_27(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_29(int64_t p0, int64_t base);
static int64_t gg_function_34(int64_t base);
static int64_t gg_function_35(int64_t p0, int64_t base);
static int64_t gg_function_38(int64_t base);
static int64_t gg_function_39(int64_t base);
static int64_t gg_function_41(int64_t base);
static int64_t gg_function_42(int64_t p0, int64_t base);
static int64_t gg_function_45(int64_t base);
static int64_t gg_function_46(int64_t p0, int64_t base);
static int64_t gg_function_50(int64_t base);
static int64_t gg_function_51(int64_t p0, int64_t base);
static int64_t gg_function_55(int64_t base);
static int64_t gg_function_56(int64_t base);
static int64_t gg_function_58(int64_t base);
static int64_t gg_function_59(int64_t base);
static int64_t gg_function_60(int64_t base);
static int64_t gg_function_61(int64_t base);
static int64_t gg_function_63(int64_t base);
static int64_t gg_function_64(int64_t base);
static int64_t gg_function_65(int64_t base);
static int64_t gg_function_67(int64_t p0, int64_t base);
static int64_t gg_function_68(int64_t base);
static int64_t gg_function_69(int64_t base);
static int64_t gg_function_70(int64_t base);
static int64_t gg_function_71(int64_t base);
static int64_t gg_function_72(int64_t base);
static int64_t gg_function_73(int64_t base);
static int64_t gg_function_74(int64_t base);
static int64_t gg_function_75(int64_t base);
static int64_t gg_function_76(int64_t base);
static int64_t gg_function_77(int64_t base);
static int64_t gg_function_78(int64_t base);
static int64_t gg_function_79(int64_t base);
static int64_t gg_function_80(int64_t base);
static int64_t gg_function_81(int64_t base);
static int64_t gg_function_82(int64_t base);
static int64_t gg_function_83(int64_t base);
static int64_t gg_function_84(int64_t base);
static int64_t gg_function_85(int64_t base);
static int64_t gg_function_86(int64_t base);
static int64_t gg_function_87(int64_t base);
static int64_t gg_function_88(int64_t base);
static int64_t gg_function_89(int64_t base);
static int64_t gg_function_90(int64_t base);
static int64_t gg_function_91(int64_t base);
static int64_t gg_function_92(int64_t base);
static int64_t gg_function_93(int64_t base);
static int64_t gg_function_94(int64_t base);
static int64_t gg_function_95(int64_t base);
static int64_t gg_function_96(int64_t base);
static int64_t gg_function_97(int64_t base);
static int64_t gg_function_98(int64_t base);
static int64_t gg_function_99(int64_t base);
static int64_t gg_function_100(int64_t base);
static int64_t gg_function_101(int64_t base);
static int64_t gg_function_102(int64_t base);
static int64_t gg_function_103(int64_t base);
static int64_t gg_function_104(int64_t base);
static int64_t gg_function_105(int64_t base);
static int64_t gg_function_106(int64_t base);
static int64_t gg_function_107(int64_t base);
static int64_t gg_function_108(int64_t base);
static int64_t gg_function_109(int64_t base);
static int64_t gg_function_110(int64_t base);
static int64_t gg_function_111(int64_t base);
static int64_t gg_function_112(int64_t base);
static int64_t gg_function_113(int64_t base);
static int64_t gg_function_114(int64_t base);
static int64_t gg_function_115(int64_t base);
static int64_t gg_function_116(int64_t base);
static int64_t gg_function_117(int64_t base);
static int64_t gg_function_118(int64_t base);
static int64_t gg_function_119(int64_t base);
static int64_t gg_function_120(int64_t base);
static int64_t gg_function_121(int64_t base);
static int64_t gg_function_122(int64_t base);
static int64_t gg_function_123(int64_t base);
static int64_t gg_function_124(int64_t base);
static int64_t gg_function_125(int64_t base);
static int64_t gg_function_126(int64_t base);
static int64_t gg_function_127(int64_t base);
static int64_t gg_function_128(int64_t base);
static int64_t gg_function_129(int64_t base);
static int64_t gg_function_130(int64_t base);
static int64_t gg_function_131(int64_t base);
static int64_t gg_function_132(int64_t base);
static int64_t gg_function_133(int64_t base);
static int64_t gg_function_134(int64_t base);
static int64_t gg_function_135(int64_t base);
static int64_t gg_function_136(int64_t base);
static int64_t gg_function_137(int64_t base);
static int64_t gg_function_138(int64_t base);
static int64_t gg_function_141(int64_t base);
static int64_t gg_function_142(int64_t base);
static int64_t gg_function_145(int64_t base);
static int64_t gg_function_146(int64_t base);
static int64_t gg_function_147(int64_t base);
static int64_t gg_function_150(int64_t base);
static int64_t gg_function_153(int64_t base);
static int64_t gg_function_156(int64_t p0, int64_t base);
static int64_t gg_function_6(int64_t base) {
  int64_t t0;
  int64_t t1;
  {
  t1 = ((int64_t)UINT64_C(7));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t1)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_138(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_137(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(18))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_136(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_135(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(22))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_27(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = apx028_tmpl_has(t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = ((t4) != (t5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_134(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(47))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_133(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(46))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_132(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(45))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_113(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(32))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_131(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(21))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_130(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(44))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_129(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(43))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_128(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(20))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_127(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(42))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_126(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(41))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_125(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(40))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_124(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(19))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_123(int64_t base) {
  /* FG_SHARED_BODY:137 */
  return gg_function_137(base);
}
static int64_t gg_function_122(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(39))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_121(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(38))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_120(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(17))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_119(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(37))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_118(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(36))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_117(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(35))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_116(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(34))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_115(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(16))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_114(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(33))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_112(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(31))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_111(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(15))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_110(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(30))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_109(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(29))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_108(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(28))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_107(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(14))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_103(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(25))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_106(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(27))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_105(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(13))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_104(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(26))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_102(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(12))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_101(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(24))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_100(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(23))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_99(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(11))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_98(int64_t base) {
  /* FG_SHARED_BODY:135 */
  return gg_function_135(base);
}
static int64_t gg_function_97(int64_t base) {
  /* FG_SHARED_BODY:131 */
  return gg_function_131(base);
}
static int64_t gg_function_96(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(10))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_95(int64_t base) {
  /* FG_SHARED_BODY:128 */
  return gg_function_128(base);
}
static int64_t gg_function_94(int64_t base) {
  /* FG_SHARED_BODY:124 */
  return gg_function_124(base);
}
static int64_t gg_function_93(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(9))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_92(int64_t base) {
  /* FG_SHARED_BODY:137 */
  return gg_function_137(base);
}
static int64_t gg_function_91(int64_t base) {
  /* FG_SHARED_BODY:120 */
  return gg_function_120(base);
}
static int64_t gg_function_90(int64_t base) {
  /* FG_SHARED_BODY:115 */
  return gg_function_115(base);
}
static int64_t gg_function_89(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(8))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_88(int64_t base) {
  /* FG_SHARED_BODY:111 */
  return gg_function_111(base);
}
static int64_t gg_function_87(int64_t base) {
  /* FG_SHARED_BODY:107 */
  return gg_function_107(base);
}
static int64_t gg_function_86(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(7))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_85(int64_t base) {
  /* FG_SHARED_BODY:105 */
  return gg_function_105(base);
}
static int64_t gg_function_84(int64_t base) {
  /* FG_SHARED_BODY:102 */
  return gg_function_102(base);
}
static int64_t gg_function_83(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(6))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_80(int64_t base) {
  /* FG_SHARED_BODY:96 */
  return gg_function_96(base);
}
static int64_t gg_function_82(int64_t base) {
  /* FG_SHARED_BODY:99 */
  return gg_function_99(base);
}
static int64_t gg_function_81(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(5))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_79(int64_t base) {
  /* FG_SHARED_BODY:93 */
  return gg_function_93(base);
}
static int64_t gg_function_78(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(4))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_34(int64_t base) {
  /* FG_SHARED_BODY:83 */
  return gg_function_83(base);
}
static int64_t gg_function_77(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(3))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_76(int64_t base) {
  /* FG_SHARED_BODY:89 */
  return gg_function_89(base);
}
static int64_t gg_function_75(int64_t base) {
  /* FG_SHARED_BODY:86 */
  return gg_function_86(base);
}
static int64_t gg_function_74(int64_t base) {
  /* FG_SHARED_BODY:138 */
  return gg_function_138(base);
}
static int64_t gg_function_73(int64_t base) {
  /* FG_SHARED_BODY:81 */
  return gg_function_81(base);
}
static int64_t gg_function_72(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_71(int64_t base) {
  /* FG_SHARED_BODY:78 */
  return gg_function_78(base);
}
static int64_t gg_function_70(int64_t base) {
  /* FG_SHARED_BODY:77 */
  return gg_function_77(base);
}
static int64_t gg_function_69(int64_t base) {
  /* FG_SHARED_BODY:138 */
  return gg_function_138(base);
}
static int64_t gg_function_68(int64_t base) {
  /* FG_SHARED_BODY:72 */
  return gg_function_72(base);
}
static int64_t gg_function_67(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
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
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  uint8_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  uint8_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  uint8_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  uint8_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  uint8_t t68;
  uint8_t t69;
  int64_t t70;
  int64_t t71;
  uint8_t t72;
  uint8_t t73;
  uint8_t t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  uint8_t t82;
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  uint8_t t87;
  uint8_t t88;
  int64_t t89;
  int64_t t90;
  uint8_t t91;
  uint8_t t92;
  int64_t t93;
  int64_t t94;
  uint8_t t95;
  uint8_t t96;
  uint8_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t t100;
  uint8_t t101;
  int64_t t102;
  int64_t t103;
  uint8_t t104;
  uint8_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  uint8_t t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  uint8_t t114;
  uint8_t t115;
  int64_t t116;
  int64_t t117;
  uint8_t t118;
  uint8_t t119;
  int64_t t120;
  int64_t t121;
  uint8_t t122;
  uint8_t t123;
  uint8_t t124;
  int64_t t125;
  int64_t t126;
  uint8_t t127;
  uint8_t t128;
  int64_t t129;
  int64_t t130;
  uint8_t t131;
  uint8_t t132;
  uint8_t t133;
  int64_t t134;
  int64_t t135;
  uint8_t t136;
  uint8_t t137;
  uint8_t t138;
  int64_t t139;
  int64_t t140;
  uint8_t t141;
  uint8_t t142;
  uint8_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
  uint8_t t147;
  uint8_t t148;
  int64_t t149;
  int64_t t150;
  uint8_t t151;
  uint8_t t152;
  uint8_t t153;
  int64_t t154;
  int64_t t155;
  uint8_t t156;
  uint8_t t157;
  uint8_t t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
  uint8_t t162;
  uint8_t t163;
  int64_t t164;
  int64_t t165;
  uint8_t t166;
  uint8_t t167;
  int64_t t168;
  int64_t t169;
  uint8_t t170;
  uint8_t t171;
  uint8_t t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  uint8_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  uint8_t t180;
  uint8_t t181;
  uint8_t t182;
  int64_t t183;
  int64_t t184;
  uint8_t t185;
  uint8_t t186;
  int64_t t187;
  int64_t t188;
  uint8_t t189;
  uint8_t t190;
  int64_t t191;
  int64_t t192;
  uint8_t t193;
  uint8_t t194;
  uint8_t t195;
  int64_t t196;
  int64_t t197;
  uint8_t t198;
  uint8_t t199;
  uint8_t t200;
  int64_t t201;
  int64_t t202;
  uint8_t t203;
  uint8_t t204;
  uint8_t t205;
  int64_t t206;
  int64_t t207;
  uint8_t t208;
  uint8_t t209;
  int64_t t210;
  int64_t t211;
  uint8_t t212;
  uint8_t t213;
  uint8_t t214;
  int64_t t215;
  int64_t t216;
  uint8_t t217;
  uint8_t t218;
  int64_t t219;
  int64_t t220;
  uint8_t t221;
  uint8_t t222;
  uint8_t t223;
  int64_t t224;
  int64_t t225;
  uint8_t t226;
  uint8_t t227;
  uint8_t t228;
  int64_t t229;
  int64_t t230;
  uint8_t t231;
  uint8_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  uint8_t t236;
  uint8_t t237;
  uint8_t t238;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_72(base + 0);
  t3 = gg_function_27(t1, t2, base + 0);
  t4 = t3;
  t5 = t4;
  if (t5) {
  t6 = ((int64_t)(v0));
  t7 = gg_function_138(base + 0);
  t8 = gg_function_27(t6, t7, base + 0);
  t5 = t8;
  }
  t9 = t5;
  t10 = t9;
  if (t10) {
  t11 = ((int64_t)(v0));
  t12 = gg_function_77(base + 0);
  t13 = gg_function_27(t11, t12, base + 0);
  t10 = t13;
  }
  t14 = t10;
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v0));
  t17 = gg_function_78(base + 0);
  t18 = gg_function_27(t16, t17, base + 0);
  t19 = t18;
  t15 = (!(t19));
  }
  t20 = t15;
  if (t20) {
  {
  t0 = gg_function_72(base + 0);
  goto gg_done;
  }
  }
  t21 = ((int64_t)(v0));
  t22 = gg_function_81(base + 0);
  t23 = gg_function_27(t21, t22, base + 0);
  t24 = t23;
  if (t24) {
  {
  t0 = gg_function_138(base + 0);
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = gg_function_83(base + 0);
  t27 = gg_function_27(t25, t26, base + 0);
  t28 = t27;
  t29 = t28;
  if (t29) {
  t30 = ((int64_t)(v0));
  t31 = gg_function_86(base + 0);
  t32 = gg_function_27(t30, t31, base + 0);
  t29 = t32;
  }
  t33 = t29;
  t34 = t33;
  if (t34) {
  t35 = ((int64_t)(v0));
  t36 = gg_function_89(base + 0);
  t37 = gg_function_27(t35, t36, base + 0);
  t34 = t37;
  }
  t38 = t34;
  if (t38) {
  {
  t0 = gg_function_77(base + 0);
  goto gg_done;
  }
  }
  t39 = ((int64_t)(v0));
  t40 = gg_function_83(base + 0);
  t41 = gg_function_27(t39, t40, base + 0);
  t42 = t41;
  if (t42) {
  {
  t0 = gg_function_78(base + 0);
  goto gg_done;
  }
  }
  t43 = ((int64_t)(v0));
  t44 = gg_function_93(base + 0);
  t45 = gg_function_27(t43, t44, base + 0);
  t46 = t45;
  t47 = t46;
  if (t47) {
  t48 = ((int64_t)(v0));
  t49 = gg_function_96(base + 0);
  t50 = gg_function_27(t48, t49, base + 0);
  t47 = t50;
  }
  t51 = t47;
  if (t51) {
  {
  t0 = gg_function_81(base + 0);
  goto gg_done;
  }
  }
  t52 = ((int64_t)(v0));
  t53 = gg_function_99(base + 0);
  t54 = gg_function_27(t52, t53, base + 0);
  t55 = t54;
  t56 = t55;
  if (t56) {
  t57 = ((int64_t)(v0));
  t58 = gg_function_96(base + 0);
  t59 = gg_function_27(t57, t58, base + 0);
  t56 = t59;
  }
  t60 = t56;
  if (t60) {
  {
  t0 = gg_function_83(base + 0);
  goto gg_done;
  }
  }
  t61 = ((int64_t)(v0));
  t62 = gg_function_102(base + 0);
  t63 = gg_function_27(t61, t62, base + 0);
  t64 = t63;
  t65 = t64;
  if (t65) {
  t66 = ((int64_t)(v0));
  t67 = gg_function_105(base + 0);
  t68 = gg_function_27(t66, t67, base + 0);
  t65 = t68;
  }
  t69 = t65;
  if (t69) {
  {
  t0 = gg_function_86(base + 0);
  goto gg_done;
  }
  }
  t70 = ((int64_t)(v0));
  t71 = gg_function_107(base + 0);
  t72 = gg_function_27(t70, t71, base + 0);
  t73 = t72;
  t74 = t73;
  if (t74) {
  t75 = ((int64_t)(v0));
  t76 = gg_function_111(base + 0);
  t77 = gg_function_27(t75, t76, base + 0);
  t74 = t77;
  }
  t78 = t74;
  if (t78) {
  {
  t0 = gg_function_89(base + 0);
  goto gg_done;
  }
  }
  t79 = ((int64_t)(v0));
  t80 = gg_function_115(base + 0);
  t81 = gg_function_27(t79, t80, base + 0);
  t82 = t81;
  t83 = t82;
  if (t83) {
  t84 = ((int64_t)(v0));
  t85 = gg_function_120(base + 0);
  t86 = gg_function_27(t84, t85, base + 0);
  t83 = t86;
  }
  t87 = t83;
  t88 = t87;
  if (t88) {
  t89 = ((int64_t)(v0));
  t90 = gg_function_137(base + 0);
  t91 = gg_function_27(t89, t90, base + 0);
  t88 = t91;
  }
  t92 = t88;
  if (t92) {
  {
  t0 = gg_function_93(base + 0);
  goto gg_done;
  }
  }
  t93 = ((int64_t)(v0));
  t94 = gg_function_124(base + 0);
  t95 = gg_function_27(t93, t94, base + 0);
  t96 = t95;
  t97 = t96;
  if (t97) {
  t98 = ((int64_t)(v0));
  t99 = gg_function_128(base + 0);
  t100 = gg_function_27(t98, t99, base + 0);
  t97 = t100;
  }
  t101 = t97;
  if (t101) {
  {
  t0 = gg_function_96(base + 0);
  goto gg_done;
  }
  }
  t102 = ((int64_t)(v0));
  t103 = gg_function_131(base + 0);
  t104 = gg_function_27(t102, t103, base + 0);
  t105 = t104;
  t106 = t105;
  if (t106) {
  t107 = ((int64_t)(v0));
  t108 = gg_function_135(base + 0);
  t109 = gg_function_27(t107, t108, base + 0);
  t106 = t109;
  }
  t110 = t106;
  if (t110) {
  {
  t0 = gg_function_99(base + 0);
  goto gg_done;
  }
  }
  t111 = ((int64_t)(v0));
  t112 = gg_function_100(base + 0);
  t113 = gg_function_27(t111, t112, base + 0);
  t114 = t113;
  t115 = t114;
  if (t115) {
  t116 = ((int64_t)(v0));
  t117 = gg_function_101(base + 0);
  t118 = gg_function_27(t116, t117, base + 0);
  t115 = t118;
  }
  t119 = t115;
  if (t119) {
  {
  t0 = gg_function_102(base + 0);
  goto gg_done;
  }
  }
  t120 = ((int64_t)(v0));
  t121 = gg_function_103(base + 0);
  t122 = gg_function_27(t120, t121, base + 0);
  t123 = t122;
  t124 = t123;
  if (t124) {
  t125 = ((int64_t)(v0));
  t126 = gg_function_104(base + 0);
  t127 = gg_function_27(t125, t126, base + 0);
  t124 = t127;
  }
  t128 = t124;
  if (t128) {
  {
  t0 = gg_function_105(base + 0);
  goto gg_done;
  }
  }
  t129 = ((int64_t)(v0));
  t130 = gg_function_106(base + 0);
  t131 = gg_function_27(t129, t130, base + 0);
  t132 = t131;
  t133 = t132;
  if (t133) {
  t134 = ((int64_t)(v0));
  t135 = gg_function_103(base + 0);
  t136 = gg_function_27(t134, t135, base + 0);
  t137 = t136;
  t133 = (!(t137));
  }
  t138 = t133;
  if (t138) {
  {
  t0 = gg_function_107(base + 0);
  goto gg_done;
  }
  }
  t139 = ((int64_t)(v0));
  t140 = gg_function_108(base + 0);
  t141 = gg_function_27(t139, t140, base + 0);
  t142 = t141;
  t143 = t142;
  if (t143) {
  t144 = ((int64_t)(v0));
  t145 = gg_function_109(base + 0);
  t146 = gg_function_27(t144, t145, base + 0);
  t143 = t146;
  }
  t147 = t143;
  t148 = t147;
  if (t148) {
  t149 = ((int64_t)(v0));
  t150 = gg_function_110(base + 0);
  t151 = gg_function_27(t149, t150, base + 0);
  t152 = t151;
  t148 = (!(t152));
  }
  t153 = t148;
  if (t153) {
  {
  t0 = gg_function_111(base + 0);
  goto gg_done;
  }
  }
  t154 = ((int64_t)(v0));
  t155 = gg_function_112(base + 0);
  t156 = gg_function_27(t154, t155, base + 0);
  t157 = t156;
  t158 = t157;
  if (t158) {
  t159 = ((int64_t)(v0));
  t160 = gg_function_113(base + 0);
  t161 = gg_function_27(t159, t160, base + 0);
  t158 = t161;
  }
  t162 = t158;
  t163 = t162;
  if (t163) {
  t164 = ((int64_t)(v0));
  t165 = gg_function_114(base + 0);
  t166 = gg_function_27(t164, t165, base + 0);
  t163 = t166;
  }
  t167 = t163;
  if (t167) {
  {
  t0 = gg_function_115(base + 0);
  goto gg_done;
  }
  }
  t168 = ((int64_t)(v0));
  t169 = gg_function_116(base + 0);
  t170 = gg_function_27(t168, t169, base + 0);
  t171 = t170;
  t172 = t171;
  if (t172) {
  t173 = ((int64_t)(v0));
  t174 = gg_function_117(base + 0);
  t175 = gg_function_27(t173, t174, base + 0);
  t172 = t175;
  }
  t176 = t172;
  t177 = t176;
  if (t177) {
  t178 = ((int64_t)(v0));
  t179 = gg_function_118(base + 0);
  t180 = gg_function_27(t178, t179, base + 0);
  t181 = t180;
  t182 = t181;
  if (!t182) {
  t183 = ((int64_t)(v0));
  t184 = gg_function_119(base + 0);
  t185 = gg_function_27(t183, t184, base + 0);
  t182 = t185;
  }
  t177 = t182;
  }
  t186 = t177;
  if (t186) {
  {
  t0 = gg_function_120(base + 0);
  goto gg_done;
  }
  }
  t187 = ((int64_t)(v0));
  t188 = gg_function_121(base + 0);
  t189 = gg_function_27(t187, t188, base + 0);
  t190 = t189;
  if (t190) {
  {
  t191 = ((int64_t)(v0));
  t192 = gg_function_122(base + 0);
  t193 = gg_function_27(t191, t192, base + 0);
  t194 = t193;
  t195 = (!(t194));
  if (t195) {
  {
  t0 = gg_function_137(base + 0);
  goto gg_done;
  }
  }
  t0 = gg_function_124(base + 0);
  goto gg_done;
  }
  }
  t196 = ((int64_t)(v0));
  t197 = gg_function_125(base + 0);
  t198 = gg_function_27(t196, t197, base + 0);
  t199 = t198;
  t200 = t199;
  if (t200) {
  t201 = ((int64_t)(v0));
  t202 = gg_function_126(base + 0);
  t203 = gg_function_27(t201, t202, base + 0);
  t200 = t203;
  }
  t204 = t200;
  t205 = t204;
  if (t205) {
  t206 = ((int64_t)(v0));
  t207 = gg_function_127(base + 0);
  t208 = gg_function_27(t206, t207, base + 0);
  t205 = t208;
  }
  t209 = t205;
  if (t209) {
  {
  t0 = gg_function_128(base + 0);
  goto gg_done;
  }
  }
  t210 = ((int64_t)(v0));
  t211 = gg_function_129(base + 0);
  t212 = gg_function_27(t210, t211, base + 0);
  t213 = t212;
  t214 = t213;
  if (t214) {
  t215 = ((int64_t)(v0));
  t216 = gg_function_130(base + 0);
  t217 = gg_function_27(t215, t216, base + 0);
  t214 = t217;
  }
  t218 = t214;
  if (t218) {
  {
  t0 = gg_function_131(base + 0);
  goto gg_done;
  }
  }
  t219 = ((int64_t)(v0));
  t220 = gg_function_113(base + 0);
  t221 = gg_function_27(t219, t220, base + 0);
  t222 = t221;
  t223 = t222;
  if (t223) {
  t224 = ((int64_t)(v0));
  t225 = gg_function_132(base + 0);
  t226 = gg_function_27(t224, t225, base + 0);
  t223 = t226;
  }
  t227 = t223;
  t228 = t227;
  if (t228) {
  t229 = ((int64_t)(v0));
  t230 = gg_function_133(base + 0);
  t231 = gg_function_27(t229, t230, base + 0);
  t228 = t231;
  }
  t232 = t228;
  t233 = t232;
  if (t233) {
  t234 = ((int64_t)(v0));
  t235 = gg_function_134(base + 0);
  t236 = gg_function_27(t234, t235, base + 0);
  t237 = t236;
  t233 = (!(t237));
  }
  t238 = t233;
  if (t238) {
  {
  t0 = gg_function_135(base + 0);
  goto gg_done;
  }
  }
  t0 = gg_function_136(base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(int64_t base) {
  /* FG_SHARED_BODY:72 */
  return gg_function_72(base);
}
static int64_t gg_function_9(int64_t base) {
  /* FG_SHARED_BODY:107 */
  return gg_function_107(base);
}
static int64_t gg_function_7(int64_t base) {
  /* FG_SHARED_BODY:105 */
  return gg_function_105(base);
}
static int64_t gg_function_156(int64_t p0, int64_t base) {
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
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
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
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint8_t t36;
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
  int64_t v0;
  uint8_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  apx028_plan_clear_result(t1);
  t2 = ((int64_t)(v0));
  t3 = apx028_plan_tools_count(t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  v1 = ((t4) > (t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = apx028_plan_template(t6, t7);
  v2 = ((int64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = apx028_plan_template(t9, t10);
  v3 = ((int64_t)(t11));
  t12 = v1;
  t13 = t12;
  if (t13) {
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t14) != (t15));
  }
  t16 = t13;
  if (t16) {
  {
  v4 = ((int64_t)(v3));
  }
  } else {
  {
  v4 = ((int64_t)(v2));
  }
  }
  t17 = ((int64_t)(v4));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t0 = gg_function_6(base + 0);
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = gg_function_105(base + 0);
  t22 = ((int64_t)(v4));
  apx028_plan_set(t20, t21, t22);
  t23 = ((int64_t)(v0));
  t24 = gg_function_107(base + 0);
  t26 = v1;
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t28) != (t29));
  }
  t30 = t27;
  if (t30) {
  {
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t31 = ((int64_t)(t25));
  apx028_plan_set(t23, t24, t31);
  t32 = ((int64_t)(v0));
  t33 = gg_function_72(base + 0);
  t35 = v1;
  t36 = t35;
  if (t36) {
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t37) != (t38));
  }
  t39 = t36;
  if (t39) {
  {
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t40 = ((int64_t)(t34));
  apx028_res_set(t32, t33, t40);
  t41 = ((int64_t)(v4));
  v5 = gg_function_67(t41, base + 0);
  t42 = ((int64_t)(v0));
  t43 = gg_function_137(base + 0);
  t44 = ((int64_t)(v5));
  apx028_plan_set(t42, t43, t44);
  t45 = ((int64_t)(v0));
  t46 = gg_function_138(base + 0);
  t47 = ((int64_t)(v5));
  apx028_res_set(t45, t46, t47);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_153(int64_t base) {
  /* FG_SHARED_BODY:86 */
  return gg_function_86(base);
}
static int64_t gg_function_150(int64_t base) {
  /* FG_SHARED_BODY:83 */
  return gg_function_83(base);
}
static int64_t gg_function_147(int64_t base) {
  /* FG_SHARED_BODY:81 */
  return gg_function_81(base);
}
static int64_t gg_function_146(int64_t base) {
  /* FG_SHARED_BODY:138 */
  return gg_function_138(base);
}
static int64_t gg_function_145(int64_t base) {
  /* FG_SHARED_BODY:72 */
  return gg_function_72(base);
}
static int64_t gg_function_64(int64_t base) {
  /* FG_SHARED_BODY:138 */
  return gg_function_138(base);
}
static int64_t gg_function_65(int64_t base) {
  /* FG_SHARED_BODY:136 */
  return gg_function_136(base);
}
static int64_t gg_function_63(int64_t base) {
  /* FG_SHARED_BODY:120 */
  return gg_function_120(base);
}
static int64_t gg_function_60(int64_t base) {
  /* FG_SHARED_BODY:102 */
  return gg_function_102(base);
}
static int64_t gg_function_58(int64_t base) {
  /* FG_SHARED_BODY:101 */
  return gg_function_101(base);
}
static int64_t gg_function_142(int64_t base) {
  /* FG_SHARED_BODY:105 */
  return gg_function_105(base);
}
static int64_t gg_function_141(int64_t base) {
  int64_t t0;
  int64_t t1;
  {
  t1 = ((int64_t)UINT64_C(3));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t1)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_61(int64_t base) {
  /* FG_SHARED_BODY:103 */
  return gg_function_103(base);
}
static int64_t gg_function_59(int64_t base) {
  int64_t t0;
  int64_t t1;
  {
  t1 = ((int64_t)UINT64_C(2));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t1)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_56(int64_t base) {
  /* FG_SHARED_BODY:138 */
  return gg_function_138(base);
}
static int64_t gg_function_55(int64_t base) {
  /* FG_SHARED_BODY:93 */
  return gg_function_93(base);
}
static int64_t gg_function_51(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = apx028_plan_msgs(t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = apx028_plan_msgs_count(t3);
  v2 = ((int64_t)(t4));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(v2));
  if (!(((t5) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v3));
  t9 = apx028_msg_at(t7, t8);
  t10 = ((int64_t)(t9));
  t11 = apx028_msg_parse_args(t10);
  v4 = ((int64_t)(t11));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
  }
  t15 = v3;
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t15) + (uint64_t)(t16)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_50(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(64))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_46(int64_t p0, int64_t base) {
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
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = apx028_plan_msgs(t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = apx028_plan_msgs_count(t3);
  v2 = ((int64_t)(t4));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(v2));
  if (!(((t5) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v3));
  t9 = apx028_msg_at(t7, t8);
  v4 = ((int64_t)(t9));
  t10 = ((int64_t)(v4));
  t11 = apx028_msg_has_tool_calls(t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) != (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v4));
  t17 = apx028_msg_has_content(t16);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t18) == (t19));
  }
  t20 = t15;
  if (t20) {
  {
  t21 = ((int64_t)(v4));
  apx028_msg_set_empty_content(t21);
  }
  }
  t22 = v3;
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_45(int64_t base) {
  /* FG_SHARED_BODY:138 */
  return gg_function_138(base);
}
static int64_t gg_function_39(int64_t base) {
  /* FG_SHARED_BODY:72 */
  return gg_function_72(base);
}
static int64_t gg_function_42(int64_t p0, int64_t base) {
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
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = apx028_plan_msgs(t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = apx028_plan_msgs_count(t3);
  v2 = ((int64_t)(t4));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) <= (t6));
  if (t7) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = apx028_msg_at(t8, t9);
  t11 = ((int64_t)(t10));
  t12 = apx028_msg_role(t11);
  t13 = ((int64_t)(t12));
  t14 = gg_function_72(base + 0);
  t15 = ((int64_t)(t14));
  t16 = ((t13) != (t15));
  if (t16) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((t17) > (t18));
  if (t19) {
  {
  t20 = ((int64_t)(v1));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = apx028_msgs_merge_content(t20, t21, t22);
  v3 = ((int64_t)(t23));
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) < (t25));
  if (t26) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  }
  }
  t27 = ((int64_t)(v1));
  t28 = apx028_msgs_erase_first(t27);
  t0 = ((int64_t)(t28));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_41(int64_t base) {
  /* FG_SHARED_BODY:78 */
  return gg_function_78(base);
}
static int64_t gg_function_38(int64_t base) {
  /* FG_SHARED_BODY:81 */
  return gg_function_81(base);
}
static int64_t gg_function_35(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = apx028_plan_msgs(t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v1));
  t4 = cf_fgc_vec_len(t3);
  v2 = ((int64_t)(t4));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(v2));
  if (!(((t5) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v3));
  t9 = apx028_msg_at(t7, t8);
  v4 = ((int64_t)(t9));
  t10 = ((int64_t)(v4));
  t11 = apx028_msg_role(t10);
  t12 = ((int64_t)(t11));
  t13 = gg_function_81(base + 0);
  t14 = ((int64_t)(t13));
  t15 = ((t12) == (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v4));
  t17 = gg_function_72(base + 0);
  apx028_msg_set_role(t16, t17);
  }
  }
  t18 = v3;
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_29(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = apx028_plan_msgs(t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = apx028_plan_msgs_count(t3);
  v2 = ((int64_t)(t4));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(v2));
  if (!(((t5) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v3));
  t9 = apx028_msg_at(t7, t8);
  t10 = ((int64_t)(t9));
  t11 = apx028_msg_trim(t10);
  v4 = ((int64_t)(t11));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
  }
  t15 = v3;
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t15) + (uint64_t)(t16)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_26(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(48))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_25(int64_t base) {
  /* FG_SHARED_BODY:78 */
  return gg_function_78(base);
}
static int64_t gg_function_24(int64_t base) {
  /* FG_SHARED_BODY:77 */
  return gg_function_77(base);
}
static int64_t gg_function_14(int64_t base) {
  /* FG_SHARED_BODY:100 */
  return gg_function_100(base);
}
static int64_t gg_function_12(int64_t base) {
  /* FG_SHARED_BODY:89 */
  return gg_function_89(base);
}
static int64_t gg_function_23(int64_t base) {
  /* FG_SHARED_BODY:138 */
  return gg_function_138(base);
}
static int64_t gg_function_19(int64_t base) {
  /* FG_SHARED_BODY:77 */
  return gg_function_77(base);
}
static int64_t gg_function_18(int64_t base) {
  /* FG_SHARED_BODY:72 */
  return gg_function_72(base);
}
static int64_t gg_function_15(int64_t base) {
  /* FG_SHARED_BODY:136 */
  return gg_function_136(base);
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
  uint8_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
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
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint8_t t36;
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
  uint8_t t61;
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
  uint8_t t72;
  uint8_t t73;
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
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  uint8_t t104;
  uint8_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  uint8_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint8_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint8_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  uint8_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  uint8_t t145;
  uint8_t t146;
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
  uint8_t t159;
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
  uint8_t t201;
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
  uint8_t t220;
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
  int64_t t252;
  int64_t t253;
  int64_t t254;
  uint8_t t255;
  uint8_t t256;
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
  uint8_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  uint8_t t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
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
  uint8_t t289;
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
  uint8_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  uint8_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  uint8_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  uint8_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t v0;
  uint8_t v1;
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
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  apx028_plan_clear_result(t1);
  t2 = ((int64_t)(v0));
  t3 = apx028_plan_tools_count(t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  v1 = ((t4) > (t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = apx028_plan_template(t6, t7);
  v2 = ((int64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = apx028_plan_template(t9, t10);
  v3 = ((int64_t)(t11));
  t12 = v1;
  t13 = t12;
  if (t13) {
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t14) != (t15));
  }
  t16 = t13;
  if (t16) {
  {
  v4 = ((int64_t)(v3));
  }
  } else {
  {
  v4 = ((int64_t)(v2));
  }
  }
  t17 = ((int64_t)(v4));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t0 = gg_function_6(base + 0);
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = gg_function_105(base + 0);
  t22 = ((int64_t)(v4));
  apx028_plan_set(t20, t21, t22);
  t23 = ((int64_t)(v0));
  t24 = gg_function_107(base + 0);
  t26 = v1;
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t28) != (t29));
  }
  t30 = t27;
  if (t30) {
  {
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t31 = ((int64_t)(t25));
  apx028_plan_set(t23, t24, t31);
  t32 = ((int64_t)(v0));
  t33 = gg_function_72(base + 0);
  t35 = v1;
  t36 = t35;
  if (t36) {
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t37) != (t38));
  }
  t39 = t36;
  if (t39) {
  {
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t40 = ((int64_t)(t34));
  apx028_res_set(t32, t33, t40);
  t41 = ((int64_t)(v0));
  t42 = gg_function_89(base + 0);
  t43 = apx028_plan_get(t41, t42);
  v5 = ((int64_t)(t43));
  t44 = ((int64_t)(v0));
  t45 = gg_function_100(base + 0);
  t46 = apx028_plan_get(t44, t45);
  v6 = ((int64_t)(t46));
  t47 = ((int64_t)(v5));
  t48 = gg_function_136(base + 0);
  t49 = ((int64_t)(t48));
  t50 = ((t47) != (t49));
  if (t50) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((int64_t)(v0));
  t52 = apx028_plan_msgs_count(t51);
  t53 = ((int64_t)(t52));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((t53) > (t54));
  if (t55) {
  {
  t56 = ((int64_t)(v0));
  t57 = apx028_plan_pop_continuation(t56);
  v7 = ((int64_t)(t57));
  t58 = ((int64_t)(v5));
  t59 = gg_function_72(base + 0);
  t60 = ((int64_t)(t59));
  t61 = ((t58) == (t60));
  if (t61) {
  {
  v5 = gg_function_77(base + 0);
  t62 = ((int64_t)(v0));
  t63 = apx028_plan_cont_reasoning_len(t62);
  t64 = ((int64_t)(t63));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((t64) > (t65));
  t67 = t66;
  if (t67) {
  t68 = ((int64_t)(v0));
  t69 = apx028_plan_cont_content_len(t68);
  t70 = ((int64_t)(t69));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((t70) == (t71));
  }
  t72 = t67;
  t73 = t72;
  if (t73) {
  t74 = ((int64_t)(v0));
  t75 = apx028_plan_cont_parts_len(t74);
  t76 = ((int64_t)(t75));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((t76) == (t77));
  }
  t78 = t73;
  if (t78) {
  {
  v5 = gg_function_138(base + 0);
  }
  }
  }
  }
  }
  }
  }
  }
  t79 = ((int64_t)(v0));
  t80 = gg_function_89(base + 0);
  t81 = ((int64_t)(v5));
  apx028_plan_set(t79, t80, t81);
  t82 = ((int64_t)(v0));
  t83 = gg_function_100(base + 0);
  t84 = ((int64_t)(v6));
  apx028_plan_set(t82, t83, t84);
  t85 = ((int64_t)(v0));
  t86 = gg_function_77(base + 0);
  t87 = ((int64_t)(v5));
  apx028_res_set(t85, t86, t87);
  t88 = ((int64_t)(v0));
  t89 = gg_function_78(base + 0);
  t90 = ((int64_t)(v6));
  apx028_res_set(t88, t89, t90);
  t91 = ((int64_t)(v4));
  t92 = gg_function_26(base + 0);
  t93 = gg_function_27(t91, t92, base + 0);
  t94 = t93;
  if (t94) {
  {
  t95 = ((int64_t)(v0));
  v8 = gg_function_29(t95, base + 0);
  t96 = ((int64_t)(v8));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((t96) < (t97));
  if (t98) {
  {
  t0 = ((int64_t)(v8));
  goto gg_done;
  }
  }
  }
  }
  t99 = ((int64_t)(v0));
  t100 = ((int64_t)(v4));
  t101 = apx028_plan_caps(t99, t100);
  v9 = ((int64_t)(t101));
  t102 = ((int64_t)(v4));
  t103 = gg_function_83(base + 0);
  t104 = gg_function_27(t102, t103, base + 0);
  t105 = t104;
  t106 = (!(t105));
  if (t106) {
  {
  t107 = ((int64_t)(v0));
  v10 = gg_function_35(t107, base + 0);
  t108 = ((int64_t)(v10));
  t109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = ((t108) < (t109));
  if (t110) {
  {
  t0 = ((int64_t)(v10));
  goto gg_done;
  }
  }
  }
  }
  t111 = ((int64_t)(v9));
  t112 = gg_function_78(base + 0);
  t113 = ((int64_t)(t112));
  t114 = ((int64_t)(((t111) & (t113))));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((t114) == (t115));
  if (t116) {
  {
  t117 = ((int64_t)(v0));
  v11 = gg_function_42(t117, base + 0);
  t118 = ((int64_t)(v11));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((t118) < (t119));
  if (t120) {
  {
  t0 = ((int64_t)(v11));
  goto gg_done;
  }
  }
  }
  }
  t121 = ((int64_t)(v9));
  t122 = gg_function_138(base + 0);
  t123 = ((int64_t)(t122));
  t124 = ((int64_t)(((t121) & (t123))));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((t124) != (t125));
  if (t126) {
  {
  t127 = ((int64_t)(v0));
  v12 = gg_function_46(t127, base + 0);
  t128 = ((int64_t)(v12));
  t129 = ((int64_t)(((int64_t)UINT64_C(0))));
  t130 = ((t128) < (t129));
  if (t130) {
  {
  t0 = ((int64_t)(v12));
  goto gg_done;
  }
  }
  }
  }
  t131 = ((int64_t)(v9));
  t132 = gg_function_50(base + 0);
  t133 = ((int64_t)(t132));
  t134 = ((int64_t)(((t131) & (t133))));
  t135 = ((int64_t)(((int64_t)UINT64_C(0))));
  t136 = ((t134) != (t135));
  if (t136) {
  {
  t137 = ((int64_t)(v0));
  v13 = gg_function_51(t137, base + 0);
  t138 = ((int64_t)(v13));
  t139 = ((int64_t)(((int64_t)UINT64_C(0))));
  t140 = ((t138) < (t139));
  if (t140) {
  {
  t0 = ((int64_t)(v13));
  goto gg_done;
  }
  }
  }
  }
  t141 = ((int64_t)(v0));
  t142 = apx028_plan_extra_context(t141);
  v14 = ((int64_t)(t142));
  t143 = ((int64_t)(v0));
  t144 = apx028_plan_parse_json_schema(t143);
  v15 = ((int64_t)(t144));
  t145 = v1;
  t146 = t145;
  if (t146) {
  t147 = ((int64_t)(v0));
  t148 = gg_function_93(base + 0);
  t149 = apx028_plan_get(t147, t148);
  t150 = ((int64_t)(t149));
  t151 = gg_function_138(base + 0);
  t152 = ((int64_t)(t151));
  t146 = ((t150) != (t152));
  }
  t153 = t146;
  t154 = t153;
  if (t154) {
  t155 = ((int64_t)(v0));
  t156 = apx028_plan_grammar_len(t155);
  t157 = ((int64_t)(t156));
  t158 = ((int64_t)(((int64_t)UINT64_C(0))));
  t154 = ((t157) > (t158));
  }
  t159 = t154;
  if (t159) {
  {
  t160 = ((int64_t)(v0));
  t161 = gg_function_101(base + 0);
  t162 = gg_function_59(base + 0);
  apx028_plan_set(t160, t161, t162);
  t163 = ((int64_t)(v0));
  t164 = gg_function_102(base + 0);
  t165 = gg_function_59(base + 0);
  apx028_res_set(t163, t164, t165);
  t0 = gg_function_59(base + 0);
  goto gg_done;
  }
  }
  t166 = ((int64_t)(v0));
  t167 = gg_function_103(base + 0);
  t168 = apx028_plan_get(t166, t167);
  t169 = ((int64_t)(t168));
  t170 = ((int64_t)(((int64_t)UINT64_C(0))));
  t171 = ((t169) != (t170));
  if (t171) {
  {
  t172 = ((int64_t)(v0));
  t173 = apx028_plan_render_pure(t172);
  v16 = ((int64_t)(t173));
  t174 = ((int64_t)(v16));
  t175 = ((int64_t)(((int64_t)UINT64_C(0))));
  t176 = ((t174) < (t175));
  if (t176) {
  {
  t177 = ((int64_t)(v0));
  t178 = gg_function_101(base + 0);
  t179 = ((int64_t)(v16));
  apx028_plan_set(t177, t178, t179);
  t180 = ((int64_t)(v0));
  t181 = gg_function_102(base + 0);
  t182 = ((int64_t)(v16));
  apx028_res_set(t180, t181, t182);
  t0 = ((int64_t)(v16));
  goto gg_done;
  }
  }
  t183 = ((int64_t)(v0));
  t184 = gg_function_120(base + 0);
  t185 = gg_function_138(base + 0);
  apx028_plan_set(t183, t184, t185);
  t186 = ((int64_t)(v0));
  t187 = gg_function_136(base + 0);
  t188 = gg_function_138(base + 0);
  apx028_res_set(t186, t187, t188);
  t189 = ((int64_t)(v0));
  t190 = apx028_plan_store_messages_json(t189);
  v17 = ((int64_t)(t190));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t191 = ((int64_t)(v4));
  v18 = gg_function_67(t191, base + 0);
  t192 = ((int64_t)(v0));
  t193 = gg_function_137(base + 0);
  t194 = ((int64_t)(v18));
  apx028_plan_set(t192, t193, t194);
  t195 = ((int64_t)(v0));
  t196 = gg_function_138(base + 0);
  t197 = ((int64_t)(v18));
  apx028_res_set(t195, t196, t197);
  t198 = ((int64_t)(v18));
  t199 = gg_function_136(base + 0);
  t200 = ((int64_t)(t199));
  t201 = ((t198) != (t200));
  if (t201) {
  {
  t202 = ((int64_t)(v0));
  t203 = ((int64_t)(v18));
  t204 = apx028_native_specialized_apply(t202, t203);
  v19 = ((int64_t)(t204));
  t205 = ((int64_t)(v19));
  t206 = ((int64_t)(((int64_t)UINT64_C(0))));
  t207 = ((t205) < (t206));
  if (t207) {
  {
  t208 = ((int64_t)(v0));
  t209 = gg_function_101(base + 0);
  t210 = ((int64_t)(v19));
  apx028_plan_set(t208, t209, t210);
  t211 = ((int64_t)(v0));
  t212 = gg_function_102(base + 0);
  t213 = ((int64_t)(v19));
  apx028_res_set(t211, t212, t213);
  t0 = ((int64_t)(v19));
  goto gg_done;
  }
  }
  t214 = ((int64_t)(v0));
  t215 = apx028_plan_store_messages_json(t214);
  v20 = ((int64_t)(t215));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t216 = ((int64_t)(v0));
  t217 = apx028_plan_analysis_ready(t216);
  t218 = ((int64_t)(t217));
  t219 = ((int64_t)(((int64_t)UINT64_C(0))));
  t220 = ((t218) == (t219));
  if (t220) {
  {
  t221 = ((int64_t)(v0));
  t222 = gg_function_101(base + 0);
  t223 = gg_function_141(base + 0);
  apx028_plan_set(t221, t222, t223);
  t224 = ((int64_t)(v0));
  t225 = gg_function_102(base + 0);
  t226 = gg_function_141(base + 0);
  apx028_res_set(t224, t225, t226);
  t227 = ((int64_t)(v0));
  t228 = gg_function_105(base + 0);
  t229 = ((int64_t)(((int64_t)UINT64_C(0))));
  apx028_res_set(t227, t228, t229);
  t0 = gg_function_141(base + 0);
  goto gg_done;
  }
  }
  t230 = ((int64_t)(v0));
  t231 = gg_function_105(base + 0);
  t232 = ((int64_t)(((int64_t)UINT64_C(1))));
  apx028_res_set(t230, t231, t232);
  t233 = ((int64_t)(v0));
  t234 = apx028_native_autoparser_generate(t233);
  v21 = ((int64_t)(t234));
  t235 = ((int64_t)(v21));
  t236 = ((int64_t)(((int64_t)UINT64_C(0))));
  t237 = ((t235) < (t236));
  if (t237) {
  {
  t238 = ((int64_t)(v0));
  t239 = gg_function_101(base + 0);
  t240 = ((int64_t)(v21));
  apx028_plan_set(t238, t239, t240);
  t241 = ((int64_t)(v0));
  t242 = gg_function_102(base + 0);
  t243 = ((int64_t)(v21));
  apx028_res_set(t241, t242, t243);
  t0 = ((int64_t)(v21));
  goto gg_done;
  }
  }
  t244 = ((int64_t)(v0));
  t245 = gg_function_120(base + 0);
  t246 = gg_function_138(base + 0);
  apx028_plan_set(t244, t245, t246);
  t247 = ((int64_t)(v0));
  t248 = gg_function_136(base + 0);
  t249 = gg_function_138(base + 0);
  apx028_res_set(t247, t248, t249);
  t250 = ((int64_t)(v0));
  t251 = apx028_plan_reason_mode(t250);
  v22 = ((int64_t)(t251));
  t252 = ((int64_t)(v22));
  t253 = gg_function_72(base + 0);
  t254 = ((int64_t)(t253));
  t255 = ((t252) == (t254));
  t256 = t255;
  if (!t256) {
  t257 = ((int64_t)(v22));
  t258 = gg_function_138(base + 0);
  t259 = ((int64_t)(t258));
  t256 = ((t257) == (t259));
  }
  t260 = t256;
  if (t260) {
  {
  t261 = ((int64_t)(v0));
  t262 = gg_function_81(base + 0);
  t263 = ((int64_t)(((int64_t)UINT64_C(1))));
  apx028_res_set(t261, t262, t263);
  t264 = ((int64_t)(v0));
  t265 = ((int64_t)(((int64_t)UINT64_C(2))));
  t266 = apx028_plan_analysis_str(t264, t265);
  v23 = ((int64_t)(t266));
  t267 = ((int64_t)(v23));
  t268 = ((int64_t)(((int64_t)UINT64_C(0))));
  t269 = ((t267) != (t268));
  if (t269) {
  {
  t270 = ((int64_t)(v23));
  t271 = apx028_str_trim_dup(t270);
  v24 = ((int64_t)(t271));
  t272 = ((int64_t)(v24));
  t273 = ((int64_t)(((int64_t)UINT64_C(0))));
  t274 = ((t272) != (t273));
  if (t274) {
  {
  t275 = ((int64_t)(v0));
  t276 = gg_function_83(base + 0);
  t277 = ((int64_t)(v24));
  t278 = apx028_res_str_take(t275, t276, t277);
  v25 = ((int64_t)(t278));
  }
  }
  }
  }
  t279 = ((int64_t)(v0));
  t280 = ((int64_t)(((int64_t)UINT64_C(3))));
  t281 = apx028_plan_analysis_str(t279, t280);
  v26 = ((int64_t)(t281));
  t282 = ((int64_t)(v26));
  t283 = ((int64_t)(((int64_t)UINT64_C(0))));
  t284 = ((t282) != (t283));
  if (t284) {
  {
  t285 = ((int64_t)(v26));
  t286 = apx028_str_trim_dup(t285);
  v27 = ((int64_t)(t286));
  t287 = ((int64_t)(v27));
  t288 = ((int64_t)(((int64_t)UINT64_C(0))));
  t289 = ((t287) != (t288));
  if (t289) {
  {
  t290 = ((int64_t)(v27));
  t291 = apx028_str_len(t290);
  t292 = ((int64_t)(t291));
  t293 = ((int64_t)(((int64_t)UINT64_C(0))));
  t294 = ((t292) > (t293));
  if (t294) {
  {
  t295 = ((int64_t)(v0));
  t296 = gg_function_86(base + 0);
  t297 = ((int64_t)(v27));
  t298 = apx028_res_str_take(t295, t296, t297);
  v28 = ((int64_t)(t298));
  }
  } else {
  {
  t299 = ((int64_t)(v27));
  apx028_str_free(t299);
  }
  }
  }
  }
  }
  }
  }
  }
  t300 = ((int64_t)(v0));
  t301 = ((int64_t)(((int64_t)UINT64_C(0))));
  t302 = apx028_plan_analysis_str(t300, t301);
  v29 = ((int64_t)(t302));
  t303 = ((int64_t)(v29));
  t304 = ((int64_t)(((int64_t)UINT64_C(0))));
  t305 = ((t303) != (t304));
  if (t305) {
  {
  t306 = ((int64_t)(v29));
  t307 = apx028_str_len(t306);
  t308 = ((int64_t)(t307));
  t309 = ((int64_t)(((int64_t)UINT64_C(0))));
  t310 = ((t308) > (t309));
  if (t310) {
  {
  t311 = ((int64_t)(v0));
  t312 = ((int64_t)(((int64_t)UINT64_C(2))));
  t313 = ((int64_t)(v29));
  t314 = apx028_plan_add_delimiter(t311, t312, t313);
  v30 = ((int64_t)(t314));
  }
  }
  }
  }
  t315 = ((int64_t)(v0));
  t316 = ((int64_t)(((int64_t)UINT64_C(1))));
  t317 = apx028_plan_analysis_str(t315, t316);
  v31 = ((int64_t)(t317));
  t318 = ((int64_t)(v31));
  t319 = ((int64_t)(((int64_t)UINT64_C(0))));
  t320 = ((t318) != (t319));
  if (t320) {
  {
  t321 = ((int64_t)(v31));
  t322 = apx028_str_len(t321);
  t323 = ((int64_t)(t322));
  t324 = ((int64_t)(((int64_t)UINT64_C(0))));
  t325 = ((t323) > (t324));
  if (t325) {
  {
  t326 = ((int64_t)(v0));
  t327 = ((int64_t)(((int64_t)UINT64_C(1))));
  t328 = ((int64_t)(v31));
  t329 = apx028_plan_add_delimiter(t326, t327, t328);
  v32 = ((int64_t)(t329));
  }
  }
  }
  }
  t330 = ((int64_t)(v0));
  t331 = apx028_plan_store_messages_json(t330);
  v33 = ((int64_t)(t331));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  uint8_t t10;
  int64_t t11;
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
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v1));
  t0 = gg_function_2(t7, base + 0);
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t11 = ((int64_t)(v1));
  t0 = gg_function_156(t11, base + 0);
  goto gg_done;
  }
  }
  t0 = gg_function_6(base + 0);
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
  {
  t0.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = gg_function_1(t1, t2, t3, t4, t5, base + 1);
  gg_integer(gg_ref(&t0, gg_get_stack(base + 0)), t6);
  gg_append(gg_ref(&t0, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t0).base, (t0).len));
  }
gg_done:;
  for (t7 = 0; t7 < 1; t7++) gg_stacks[base + t7].top = 0;
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
