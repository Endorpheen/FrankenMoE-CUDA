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
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static int64_t gg_function_3(int64_t base);
static void gg_function_5(uint8_t p0, int64_t base);
static void gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_9(int64_t p0, int64_t base);
static int64_t gg_function_11(int64_t base);
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t base);
static void gg_function_14(int64_t p0, int64_t base);
static void gg_function_15(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_16(int64_t p0, int64_t base);
static void gg_function_18(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_23(int64_t p0, int64_t p1, int64_t base);
static void gg_function_26(int64_t p0, int64_t base);
static void gg_function_28(int64_t p0, int64_t base);
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_30(int64_t base);
static void gg_function_31(int64_t p0, int64_t p1, int64_t base);
static void gg_function_33(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_34(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_36(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_37(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_38(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_39(int64_t p0, int64_t p1, int64_t base);
static void gg_function_41(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_43(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_44(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_45(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_46(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_47(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_48(int64_t base);
static int64_t gg_function_49(int64_t base);
static void gg_function_50(int64_t p0, int64_t base);
static void gg_function_51(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_52(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_53(int64_t p0, int64_t p1, int64_t base);
static void gg_function_54(int64_t p0, int64_t p1, int64_t base);
static void gg_function_55(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_56(int64_t p0, int64_t base);
static void gg_function_57(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_58(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_60(int64_t p0, int64_t p1, int64_t base);
static void gg_function_5(uint8_t p0, int64_t base) {
  uint8_t t0;
  uint8_t t1;
  uint8_t v0;
  v0 = p0;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
gg_done:;
}
static int64_t gg_function_12(int64_t p0, int64_t p1, int64_t base) {
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
  t3 = fgc_record_get(t1, t2);
  t0 = ((int64_t)(t3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t13;
  int64_t t14;
  int64_t t15;
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
  t13 = t8;
  gg_function_5(t13, base + 0);
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
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
  uint8_t t17;
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
  t14 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)gg_div(t14, t15))));
  t12 = ((t13) <= (t16));
  }
  t8 = t12;
  }
  t17 = t8;
  gg_function_5(t17, base + 0);
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_54(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v1));
  t1 = fgc_vec_len(t0);
  v2 = ((int64_t)(t1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v2));
  t4 = fgc_vec_resize(t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) >= (t6));
  gg_function_5(t7, base + 0);
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(v2));
  if (!(((t8) < (t9)))) goto label2;
  {
  t10 = ((int64_t)(v0));
  t11 = fgc_i64_vec_data(t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = gg_function_23(t13, t14, base + 0);
  t16 = fgc_raw_ptr_add(t12, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(v1));
  t19 = fgc_i64_vec_data(t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(((int64_t)UINT64_C(4))));
  t23 = gg_function_23(t21, t22, base + 0);
  t24 = fgc_raw_ptr_add(t20, t23);
  t25 = ((int64_t)(t24));
  t26 = fgc_raw_load_i32(t25);
  t27 = ((int64_t)(t26));
  fgc_raw_store_i32(t17, t27);
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = gg_function_20(t28, t29, base + 0);
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_55(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v1));
  t1 = fgc_vec_len(t0);
  v2 = ((int64_t)(t1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v2));
  t4 = fgc_vec_resize(t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) >= (t6));
  gg_function_5(t7, base + 0);
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(v2));
  if (!(((t8) < (t9)))) goto label2;
  {
  t10 = ((int64_t)(v0));
  t11 = fgc_i64_vec_data(t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(v3));
  t14 = fgc_raw_ptr_add(t12, t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(v1));
  t17 = fgc_i64_vec_data(t16);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(v3));
  t20 = fgc_raw_ptr_add(t18, t19);
  t21 = ((int64_t)(t20));
  t22 = fgc_raw_load_i8(t21);
  t23 = ((int64_t)(t22));
  fgc_raw_store_i8(t15, t23);
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = gg_function_20(t24, t25, base + 0);
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_57(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t11;
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
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v1));
  t1 = fgc_vec_len(t0);
  v2 = ((int64_t)(t1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v2));
  t4 = fgc_vec_resize(t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) >= (t6));
  gg_function_5(t7, base + 0);
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) > (t9));
  if (t10) {
  {
  t11 = ((int64_t)(v0));
  t12 = fgc_i64_vec_data(t11);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(v1));
  t15 = fgc_i64_vec_data(t14);
  t16 = ((int64_t)(t15));
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = gg_function_23(t17, t18, base + 0);
  fgc_raw_memcpy(t13, t16, t19);
  }
  }
  }
gg_done:;
}
static void gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
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
  t2 = ((int64_t)(v2));
  t3 = fgc_record_set(t0, t1, t2);
  v3 = ((int64_t)(t3));
  }
gg_done:;
}
static int64_t gg_function_11(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1111577649))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_49(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1430410289))));
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fgc_record_kind(t1);
  t3 = ((int64_t)(t2));
  t4 = gg_function_49(base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((t3) == (t5));
  gg_function_5(t6, base + 0);
  t7 = ((int64_t)(v1));
  t8 = fgc_record_kind(t7);
  t9 = ((int64_t)(t8));
  t10 = gg_function_11(base + 0);
  t11 = ((int64_t)(t10));
  t12 = ((t9) == (t11));
  gg_function_5(t12, base + 0);
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = gg_function_12(t15, t16, base + 0);
  gg_function_6(t13, t14, t17, base + 0);
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(2))));
  t22 = gg_function_12(t20, t21, base + 0);
  gg_function_6(t18, t19, t22, base + 0);
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(4))));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(4))));
  t27 = gg_function_12(t25, t26, base + 0);
  gg_function_6(t23, t24, t27, base + 0);
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)UINT64_C(21))));
  t30 = gg_function_12(t28, t29, base + 0);
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(20))));
  t33 = gg_function_12(t31, t32, base + 0);
  gg_function_54(t30, t33, base + 0);
  t34 = ((int64_t)(v1));
  t35 = ((int64_t)(((int64_t)UINT64_C(22))));
  t36 = gg_function_12(t34, t35, base + 0);
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(21))));
  t39 = gg_function_12(t37, t38, base + 0);
  gg_function_57(t36, t39, base + 0);
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(((int64_t)UINT64_C(23))));
  t42 = gg_function_12(t40, t41, base + 0);
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(22))));
  t45 = gg_function_12(t43, t44, base + 0);
  gg_function_57(t42, t45, base + 0);
  t46 = ((int64_t)(v1));
  t47 = ((int64_t)(((int64_t)UINT64_C(24))));
  t48 = gg_function_12(t46, t47, base + 0);
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(23))));
  t51 = gg_function_12(t49, t50, base + 0);
  gg_function_54(t48, t51, base + 0);
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(((int64_t)UINT64_C(25))));
  t54 = gg_function_12(t52, t53, base + 0);
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(24))));
  t57 = gg_function_12(t55, t56, base + 0);
  gg_function_54(t54, t57, base + 0);
  t58 = ((int64_t)(v1));
  t59 = ((int64_t)(((int64_t)UINT64_C(26))));
  t60 = gg_function_12(t58, t59, base + 0);
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(25))));
  t63 = gg_function_12(t61, t62, base + 0);
  gg_function_54(t60, t63, base + 0);
  t64 = ((int64_t)(v1));
  t65 = ((int64_t)(((int64_t)UINT64_C(27))));
  t66 = gg_function_12(t64, t65, base + 0);
  t67 = ((int64_t)(v0));
  t68 = ((int64_t)(((int64_t)UINT64_C(26))));
  t69 = gg_function_12(t67, t68, base + 0);
  gg_function_54(t66, t69, base + 0);
  t70 = ((int64_t)(v1));
  t71 = ((int64_t)(((int64_t)UINT64_C(28))));
  t72 = gg_function_12(t70, t71, base + 0);
  t73 = ((int64_t)(v0));
  t74 = ((int64_t)(((int64_t)UINT64_C(29))));
  t75 = gg_function_12(t73, t74, base + 0);
  gg_function_55(t72, t75, base + 0);
  t76 = ((int64_t)(v1));
  t77 = ((int64_t)(((int64_t)UINT64_C(29))));
  t78 = gg_function_12(t76, t77, base + 0);
  t79 = ((int64_t)(v0));
  t80 = ((int64_t)(((int64_t)UINT64_C(31))));
  t81 = gg_function_12(t79, t80, base + 0);
  gg_function_54(t78, t81, base + 0);
  t82 = ((int64_t)(v1));
  t83 = ((int64_t)(((int64_t)UINT64_C(31))));
  t84 = gg_function_12(t82, t83, base + 0);
  t85 = ((int64_t)(v0));
  t86 = ((int64_t)(((int64_t)UINT64_C(32))));
  t87 = gg_function_12(t85, t86, base + 0);
  gg_function_54(t84, t87, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_34(int64_t p0, int64_t p1, int64_t base) {
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
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v0));
  t7 = fgc_vec_len(t6);
  t8 = ((int64_t)(t7));
  t4 = ((t5) < (t8));
  }
  t9 = t4;
  gg_function_5(t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = fgc_i64_vec_data(t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = gg_function_23(t13, t14, base + 0);
  t16 = fgc_raw_ptr_add(t12, t15);
  t17 = ((int64_t)(t16));
  t18 = fgc_raw_load_i32(t17);
  t0 = ((int64_t)(t18));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_58(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fgc_record_kind(t1);
  t3 = ((int64_t)(t2));
  t4 = gg_function_49(base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((t3) == (t5));
  gg_function_5(t6, base + 0);
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  v2 = gg_function_12(t7, t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(33))));
  t11 = gg_function_12(t9, t10, base + 0);
  t12 = ((int64_t)(v2));
  t13 = fgc_vec_resize(t11, t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) >= (t15));
  gg_function_5(t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(25))));
  t19 = gg_function_12(t17, t18, base + 0);
  t20 = fgc_i64_vec_data(t19);
  v3 = ((int64_t)(t20));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(v2));
  if (!(((t21) < (t22)))) goto label2;
  {
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(26))));
  t25 = gg_function_12(t23, t24, base + 0);
  t26 = ((int64_t)(v4));
  v5 = gg_function_34(t25, t26, base + 0);
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(33))));
  t29 = gg_function_12(t27, t28, base + 0);
  t30 = fgc_i64_vec_data(t29);
  t31 = ((int64_t)(t30));
  t32 = ((int64_t)(v4));
  t33 = ((int64_t)(((int64_t)UINT64_C(8))));
  t34 = gg_function_23(t32, t33, base + 0);
  t35 = fgc_raw_ptr_add(t31, t34);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(v5));
  t39 = ((int64_t)(((int64_t)UINT64_C(4))));
  t40 = gg_function_23(t38, t39, base + 0);
  t41 = fgc_raw_ptr_add(t37, t40);
  t42 = ((int64_t)(t41));
  fgc_raw_store_i64(t36, t42);
  t43 = ((int64_t)(v4));
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = gg_function_20(t43, t44, base + 0);
  }
  goto label1;
label2:;
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = gg_function_12(t46, t47, base + 0);
  fgc_raw_store_i64(t45, t48);
  t49 = ((int64_t)(v1));
  t50 = ((int64_t)(((int64_t)UINT64_C(8))));
  t51 = fgc_raw_ptr_add(t49, t50);
  t52 = ((int64_t)(t51));
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = gg_function_12(t53, t54, base + 0);
  fgc_raw_store_i64(t52, t55);
  t56 = ((int64_t)(v1));
  t57 = ((int64_t)(((int64_t)UINT64_C(16))));
  t58 = fgc_raw_ptr_add(t56, t57);
  t59 = ((int64_t)(t58));
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(2))));
  t62 = gg_function_12(t60, t61, base + 0);
  fgc_raw_store_i64(t59, t62);
  t63 = ((int64_t)(v1));
  t64 = ((int64_t)(((int64_t)UINT64_C(24))));
  t65 = fgc_raw_ptr_add(t63, t64);
  t66 = ((int64_t)(t65));
  t67 = ((int64_t)(v0));
  t68 = ((int64_t)(((int64_t)UINT64_C(3))));
  t69 = gg_function_12(t67, t68, base + 0);
  fgc_raw_store_i64(t66, t69);
  t70 = ((int64_t)(v1));
  t71 = ((int64_t)(((int64_t)UINT64_C(32))));
  t72 = fgc_raw_ptr_add(t70, t71);
  t73 = ((int64_t)(t72));
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(4))));
  t76 = gg_function_12(t74, t75, base + 0);
  fgc_raw_store_i64(t73, t76);
  t77 = ((int64_t)(v1));
  t78 = ((int64_t)(((int64_t)UINT64_C(40))));
  t79 = fgc_raw_ptr_add(t77, t78);
  t80 = ((int64_t)(t79));
  t81 = ((int64_t)(v0));
  t82 = ((int64_t)(((int64_t)UINT64_C(5))));
  t83 = gg_function_12(t81, t82, base + 0);
  fgc_raw_store_i32(t80, t83);
  t84 = ((int64_t)(v1));
  t85 = ((int64_t)(((int64_t)UINT64_C(48))));
  t86 = fgc_raw_ptr_add(t84, t85);
  t87 = ((int64_t)(t86));
  t88 = ((int64_t)(v0));
  t89 = ((int64_t)(((int64_t)UINT64_C(20))));
  t90 = gg_function_12(t88, t89, base + 0);
  t91 = fgc_i64_vec_data(t90);
  t92 = ((int64_t)(t91));
  fgc_raw_store_i64(t87, t92);
  t93 = ((int64_t)(v1));
  t94 = ((int64_t)(((int64_t)UINT64_C(56))));
  t95 = fgc_raw_ptr_add(t93, t94);
  t96 = ((int64_t)(t95));
  t97 = ((int64_t)(v0));
  t98 = ((int64_t)(((int64_t)UINT64_C(21))));
  t99 = gg_function_12(t97, t98, base + 0);
  t100 = fgc_i64_vec_data(t99);
  t101 = ((int64_t)(t100));
  fgc_raw_store_i64(t96, t101);
  t102 = ((int64_t)(v1));
  t103 = ((int64_t)(((int64_t)UINT64_C(64))));
  t104 = fgc_raw_ptr_add(t102, t103);
  t105 = ((int64_t)(t104));
  t106 = ((int64_t)(v0));
  t107 = ((int64_t)(((int64_t)UINT64_C(22))));
  t108 = gg_function_12(t106, t107, base + 0);
  t109 = fgc_i64_vec_data(t108);
  t110 = ((int64_t)(t109));
  fgc_raw_store_i64(t105, t110);
  t111 = ((int64_t)(v1));
  t112 = ((int64_t)(((int64_t)UINT64_C(72))));
  t113 = fgc_raw_ptr_add(t111, t112);
  t114 = ((int64_t)(t113));
  t115 = ((int64_t)(v0));
  t116 = ((int64_t)(((int64_t)UINT64_C(23))));
  t117 = gg_function_12(t115, t116, base + 0);
  t118 = fgc_i64_vec_data(t117);
  t119 = ((int64_t)(t118));
  fgc_raw_store_i64(t114, t119);
  t120 = ((int64_t)(v1));
  t121 = ((int64_t)(((int64_t)UINT64_C(80))));
  t122 = fgc_raw_ptr_add(t120, t121);
  t123 = ((int64_t)(t122));
  t124 = ((int64_t)(v0));
  t125 = ((int64_t)(((int64_t)UINT64_C(24))));
  t126 = gg_function_12(t124, t125, base + 0);
  t127 = fgc_i64_vec_data(t126);
  t128 = ((int64_t)(t127));
  fgc_raw_store_i64(t123, t128);
  t129 = ((int64_t)(v1));
  t130 = ((int64_t)(((int64_t)UINT64_C(88))));
  t131 = fgc_raw_ptr_add(t129, t130);
  t132 = ((int64_t)(t131));
  t133 = ((int64_t)(v0));
  t134 = ((int64_t)(((int64_t)UINT64_C(33))));
  t135 = gg_function_12(t133, t134, base + 0);
  t136 = fgc_i64_vec_data(t135);
  t137 = ((int64_t)(t136));
  fgc_raw_store_i64(t132, t137);
  t138 = ((int64_t)(v1));
  t139 = ((int64_t)(((int64_t)UINT64_C(96))));
  t140 = fgc_raw_ptr_add(t138, t139);
  t141 = ((int64_t)(t140));
  t142 = ((int64_t)(v0));
  t143 = ((int64_t)(((int64_t)UINT64_C(27))));
  t144 = gg_function_12(t142, t143, base + 0);
  t145 = fgc_i64_vec_data(t144);
  t146 = ((int64_t)(t145));
  fgc_raw_store_i64(t141, t146);
  t147 = ((int64_t)(v1));
  t148 = ((int64_t)(((int64_t)UINT64_C(104))));
  t149 = fgc_raw_ptr_add(t147, t148);
  t150 = ((int64_t)(t149));
  t151 = ((int64_t)(v0));
  t152 = ((int64_t)(((int64_t)UINT64_C(28))));
  t153 = gg_function_12(t151, t152, base + 0);
  t154 = fgc_i64_vec_data(t153);
  t155 = ((int64_t)(t154));
  fgc_raw_store_i64(t150, t155);
  t156 = ((int64_t)(v1));
  t157 = ((int64_t)(((int64_t)UINT64_C(112))));
  t158 = fgc_raw_ptr_add(t156, t157);
  t159 = ((int64_t)(t158));
  t160 = ((int64_t)(v0));
  t161 = ((int64_t)(((int64_t)UINT64_C(29))));
  t162 = gg_function_12(t160, t161, base + 0);
  t163 = fgc_i64_vec_data(t162);
  t164 = ((int64_t)(t163));
  fgc_raw_store_i64(t159, t164);
  t165 = ((int64_t)(v1));
  t166 = ((int64_t)(((int64_t)UINT64_C(120))));
  t167 = fgc_raw_ptr_add(t165, t166);
  t168 = ((int64_t)(t167));
  t169 = ((int64_t)(v0));
  t170 = ((int64_t)(((int64_t)UINT64_C(30))));
  t171 = gg_function_12(t169, t170, base + 0);
  t172 = fgc_i64_vec_data(t171);
  t173 = ((int64_t)(t172));
  fgc_raw_store_i64(t168, t173);
  t174 = ((int64_t)(v1));
  t175 = ((int64_t)(((int64_t)UINT64_C(128))));
  t176 = fgc_raw_ptr_add(t174, t175);
  t177 = ((int64_t)(t176));
  t178 = ((int64_t)(v0));
  t179 = ((int64_t)(((int64_t)UINT64_C(31))));
  t180 = gg_function_12(t178, t179, base + 0);
  t181 = fgc_i64_vec_data(t180);
  t182 = ((int64_t)(t181));
  fgc_raw_store_i64(t177, t182);
  t183 = ((int64_t)(v1));
  t184 = ((int64_t)(((int64_t)UINT64_C(136))));
  t185 = fgc_raw_ptr_add(t183, t184);
  t186 = ((int64_t)(t185));
  t187 = ((int64_t)(v0));
  t188 = ((int64_t)(((int64_t)UINT64_C(32))));
  t189 = gg_function_12(t187, t188, base + 0);
  t190 = fgc_i64_vec_data(t189);
  t191 = ((int64_t)(t190));
  fgc_raw_store_i64(t186, t191);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_18(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = fgc_vec_len(t0);
  v2 = ((int64_t)(t1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(1))));
  t5 = gg_function_20(t3, t4, base + 0);
  t6 = fgc_vec_resize(t2, t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) >= (t8));
  gg_function_5(t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = fgc_i64_vec_data(t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = gg_function_23(t13, t14, base + 0);
  t16 = fgc_raw_ptr_add(t12, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(v1));
  fgc_raw_store_i32(t17, t18);
  }
gg_done:;
}
static int64_t gg_function_16(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = fgc_vec_new(t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  gg_function_5(t5, base + 0);
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_51(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v2));
  v3 = gg_function_16(t0, base + 0);
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v3));
  gg_function_6(t1, t2, t3, base + 0);
  }
gg_done:;
}
static void gg_function_50(int64_t p0, int64_t base) {
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
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(20))));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t0, t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(21))));
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t3, t4, t5, base + 0);
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(22))));
  t8 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t6, t7, t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(23))));
  t11 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t9, t10, t11, base + 0);
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(24))));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t12, t13, t14, base + 0);
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(25))));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t15, t16, t17, base + 0);
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(26))));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t18, t19, t20, base + 0);
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(27))));
  t23 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t21, t22, t23, base + 0);
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(28))));
  t26 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t24, t25, t26, base + 0);
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(29))));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_51(t27, t28, t29, base + 0);
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(30))));
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_51(t30, t31, t32, base + 0);
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(31))));
  t35 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t33, t34, t35, base + 0);
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(32))));
  t38 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t36, t37, t38, base + 0);
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(33))));
  t41 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_function_51(t39, t40, t41, base + 0);
  }
gg_done:;
}
static int64_t gg_function_48(int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t v0;
  {
  t1 = gg_function_49(base + 0);
  t2 = fgc_record_new(t1);
  v0 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  gg_function_5(t5, base + 0);
  t6 = ((int64_t)(v0));
  gg_function_50(t6, base + 0);
  t0 = ((int64_t)(v0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_56(int64_t p0, int64_t base) {
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
  uint8_t t130;
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
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  uint8_t t156;
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
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = fgc_record_kind(t1);
  t3 = ((int64_t)(t2));
  t4 = gg_function_11(base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((t3) == (t5));
  gg_function_5(t6, base + 0);
  v1 = gg_function_48(base + 0);
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = gg_function_12(t7, t8, base + 0);
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(v2));
  gg_function_6(t9, t10, t11, base + 0);
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) > (t16));
  if (t17) {
  {
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t18 = ((int64_t)(t14));
  gg_function_6(t12, t13, t18, base + 0);
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(v2));
  gg_function_6(t19, t20, t21, base + 0);
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)UINT64_C(3))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t22, t23, t24, base + 0);
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(4))));
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  t29 = gg_function_12(t27, t28, base + 0);
  gg_function_6(t25, t26, t29, base + 0);
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(((int64_t)UINT64_C(5))));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t30, t31, t32, base + 0);
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)UINT64_C(6))));
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(10))));
  t37 = gg_function_12(t35, t36, base + 0);
  gg_function_6(t33, t34, t37, base + 0);
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(((int64_t)UINT64_C(20))));
  v3 = gg_function_12(t38, t39, base + 0);
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(((int64_t)UINT64_C(21))));
  v4 = gg_function_12(t40, t41, base + 0);
  t42 = ((int64_t)(v1));
  t43 = ((int64_t)(((int64_t)UINT64_C(22))));
  v5 = gg_function_12(t42, t43, base + 0);
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(((int64_t)UINT64_C(23))));
  v6 = gg_function_12(t44, t45, base + 0);
  t46 = ((int64_t)(v1));
  t47 = ((int64_t)(((int64_t)UINT64_C(24))));
  v7 = gg_function_12(t46, t47, base + 0);
  t48 = ((int64_t)(v1));
  t49 = ((int64_t)(((int64_t)UINT64_C(25))));
  v8 = gg_function_12(t48, t49, base + 0);
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(((int64_t)UINT64_C(26))));
  v9 = gg_function_12(t50, t51, base + 0);
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(((int64_t)UINT64_C(27))));
  v10 = gg_function_12(t52, t53, base + 0);
  t54 = ((int64_t)(v1));
  t55 = ((int64_t)(((int64_t)UINT64_C(28))));
  v11 = gg_function_12(t54, t55, base + 0);
  t56 = ((int64_t)(v1));
  t57 = ((int64_t)(((int64_t)UINT64_C(29))));
  v12 = gg_function_12(t56, t57, base + 0);
  t58 = ((int64_t)(v1));
  t59 = ((int64_t)(((int64_t)UINT64_C(31))));
  v13 = gg_function_12(t58, t59, base + 0);
  t60 = ((int64_t)(v1));
  t61 = ((int64_t)(((int64_t)UINT64_C(32))));
  v14 = gg_function_12(t60, t61, base + 0);
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(((int64_t)UINT64_C(21))));
  v15 = gg_function_12(t62, t63, base + 0);
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(((int64_t)UINT64_C(22))));
  v16 = gg_function_12(t64, t65, base + 0);
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(23))));
  v17 = gg_function_12(t66, t67, base + 0);
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(24))));
  v18 = gg_function_12(t68, t69, base + 0);
  t70 = ((int64_t)(v0));
  t71 = ((int64_t)(((int64_t)UINT64_C(25))));
  v19 = gg_function_12(t70, t71, base + 0);
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(((int64_t)UINT64_C(26))));
  v20 = gg_function_12(t72, t73, base + 0);
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(27))));
  v21 = gg_function_12(t74, t75, base + 0);
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(((int64_t)UINT64_C(28))));
  v22 = gg_function_12(t76, t77, base + 0);
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(((int64_t)UINT64_C(29))));
  v23 = gg_function_12(t78, t79, base + 0);
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(((int64_t)UINT64_C(31))));
  v24 = gg_function_12(t80, t81, base + 0);
  t82 = ((int64_t)(v3));
  t83 = ((int64_t)(v15));
  gg_function_54(t82, t83, base + 0);
  t84 = ((int64_t)(v4));
  t85 = ((int64_t)(v16));
  gg_function_57(t84, t85, base + 0);
  t86 = ((int64_t)(v5));
  t87 = ((int64_t)(v17));
  gg_function_57(t86, t87, base + 0);
  t88 = ((int64_t)(v6));
  t89 = ((int64_t)(v18));
  gg_function_54(t88, t89, base + 0);
  t90 = ((int64_t)(v7));
  t91 = ((int64_t)(v19));
  gg_function_54(t90, t91, base + 0);
  t92 = ((int64_t)(v8));
  t93 = ((int64_t)(v20));
  gg_function_54(t92, t93, base + 0);
  t94 = ((int64_t)(v9));
  t95 = ((int64_t)(v21));
  gg_function_54(t94, t95, base + 0);
  t96 = ((int64_t)(v12));
  t97 = ((int64_t)(v22));
  gg_function_55(t96, t97, base + 0);
  t98 = ((int64_t)(v13));
  t99 = ((int64_t)(v23));
  gg_function_54(t98, t99, base + 0);
  t100 = ((int64_t)(v14));
  t101 = ((int64_t)(v24));
  gg_function_54(t100, t101, base + 0);
  v25 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t102 = ((int64_t)(v25));
  t103 = ((int64_t)(v2));
  if (!(((t102) < (t103)))) goto label2;
  {
  t104 = ((int64_t)(v21));
  t105 = ((int64_t)(v25));
  v26 = gg_function_34(t104, t105, base + 0);
  t106 = ((int64_t)(v19));
  t107 = ((int64_t)(v25));
  v27 = gg_function_34(t106, t107, base + 0);
  v28 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t108 = ((int64_t)(v28));
  t109 = ((int64_t)(v27));
  if (!(((t108) < (t109)))) goto label5;
  {
  t110 = ((int64_t)(v20));
  t111 = fgc_i64_vec_data(t110);
  t112 = ((int64_t)(t111));
  t113 = ((int64_t)(v26));
  t114 = ((int64_t)(v28));
  t115 = gg_function_20(t113, t114, base + 0);
  t116 = ((int64_t)(((int64_t)UINT64_C(4))));
  t117 = gg_function_23(t115, t116, base + 0);
  t118 = fgc_raw_ptr_add(t112, t117);
  t119 = ((int64_t)(t118));
  t120 = fgc_raw_load_i32(t119);
  v29 = ((int64_t)(t120));
  v30 = ((int64_t)(((int64_t)UINT64_C(0))));
  v31 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t121 = ((int64_t)(v31));
  t122 = ((int64_t)(v10));
  t123 = fgc_vec_len(t122);
  t124 = ((int64_t)(t123));
  if (!(((t121) < (t124)))) goto label8;
  {
  t125 = ((int64_t)(v10));
  t126 = ((int64_t)(v31));
  t127 = gg_function_34(t125, t126, base + 0);
  t128 = ((int64_t)(t127));
  t129 = ((int64_t)(v29));
  t130 = ((t128) == (t129));
  if (t130) {
  {
  v30 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto label8;
  }
  }
  t131 = ((int64_t)(v31));
  t132 = ((int64_t)(((int64_t)UINT64_C(1))));
  v31 = gg_function_20(t131, t132, base + 0);
  }
  goto label7;
label8:;
  t133 = ((int64_t)(v30));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((t133) == (t134));
  if (t135) {
  {
  t136 = ((int64_t)(v10));
  t137 = ((int64_t)(v29));
  gg_function_18(t136, t137, base + 0);
  }
  }
  t138 = ((int64_t)(v28));
  t139 = ((int64_t)(((int64_t)UINT64_C(1))));
  v28 = gg_function_20(t138, t139, base + 0);
  }
  goto label4;
label5:;
  t140 = ((int64_t)(v25));
  t141 = ((int64_t)(((int64_t)UINT64_C(1))));
  v25 = gg_function_20(t140, t141, base + 0);
  }
  goto label1;
label2:;
  t142 = ((int64_t)(v1));
  t143 = ((int64_t)(((int64_t)UINT64_C(3))));
  t144 = ((int64_t)(v10));
  t145 = fgc_vec_len(t144);
  t146 = ((int64_t)(t145));
  gg_function_6(t142, t143, t146, base + 0);
  t147 = ((int64_t)(v0));
  t148 = ((int64_t)(((int64_t)UINT64_C(0))));
  v32 = gg_function_12(t147, t148, base + 0);
  t149 = ((int64_t)(v32));
  t150 = ((int64_t)(((int64_t)UINT64_C(3))));
  v33 = gg_function_12(t149, t150, base + 0);
  t151 = ((int64_t)(v11));
  t152 = ((int64_t)(v33));
  t153 = fgc_vec_resize(t151, t152);
  t154 = ((int64_t)(t153));
  t155 = ((int64_t)(((int64_t)UINT64_C(0))));
  t156 = ((t154) >= (t155));
  gg_function_5(t156, base + 0);
  v34 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t157 = ((int64_t)(v34));
  t158 = ((int64_t)(v33));
  if (!(((t157) < (t158)))) goto label11;
  {
  t159 = ((int64_t)(v11));
  t160 = fgc_i64_vec_data(t159);
  t161 = ((int64_t)(t160));
  t162 = ((int64_t)(v34));
  t163 = ((int64_t)(((int64_t)UINT64_C(4))));
  t164 = gg_function_23(t162, t163, base + 0);
  t165 = fgc_raw_ptr_add(t161, t164);
  t166 = ((int64_t)(t165));
  t167 = ((int64_t)UINT64_C(1));
  t168 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t167)))));
  fgc_raw_store_i32(t166, t168);
  t169 = ((int64_t)(v34));
  t170 = ((int64_t)(((int64_t)UINT64_C(1))));
  v34 = gg_function_20(t169, t170, base + 0);
  }
  goto label10;
label11:;
  v35 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t171 = ((int64_t)(v35));
  t172 = ((int64_t)(v10));
  t173 = fgc_vec_len(t172);
  t174 = ((int64_t)(t173));
  if (!(((t171) < (t174)))) goto label14;
  {
  t175 = ((int64_t)(v10));
  t176 = ((int64_t)(v35));
  v36 = gg_function_34(t175, t176, base + 0);
  t177 = ((int64_t)(v11));
  t178 = fgc_i64_vec_data(t177);
  t179 = ((int64_t)(t178));
  t180 = ((int64_t)(v36));
  t181 = ((int64_t)(((int64_t)UINT64_C(4))));
  t182 = gg_function_23(t180, t181, base + 0);
  t183 = fgc_raw_ptr_add(t179, t182);
  t184 = ((int64_t)(t183));
  t185 = ((int64_t)(v35));
  fgc_raw_store_i32(t184, t185);
  t186 = ((int64_t)(v35));
  t187 = ((int64_t)(((int64_t)UINT64_C(1))));
  v35 = gg_function_20(t186, t187, base + 0);
  }
  goto label13;
label14:;
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_53(int64_t p0, int64_t p1, int64_t base) {
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
  gg_function_5(t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t4) - (uint64_t)(t5)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_52(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  gg_function_5(t7, base + 0);
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)gg_div(t8, t9))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_28(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = fgc_record_kind(t0);
  t2 = ((int64_t)(t1));
  t3 = gg_function_11(base + 0);
  t4 = ((int64_t)(t3));
  t5 = ((t2) == (t4));
  gg_function_5(t5, base + 0);
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t6, t7, t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t9, t10, t11, base + 0);
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(8))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t12, t13, t14, base + 0);
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(9))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t15, t16, t17, base + 0);
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(10))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t18, t19, t20, base + 0);
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(11))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t21, t22, t23, base + 0);
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(12))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t24, t25, t26, base + 0);
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(13))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t27, t28, t29, base + 0);
  v1 = ((int64_t)(((int64_t)UINT64_C(20))));
label1:;
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(((int64_t)UINT64_C(33))));
  if (!(((t30) <= (t31)))) goto label2;
  {
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(v1));
  v2 = gg_function_12(t32, t33, base + 0);
  t34 = ((int64_t)(v2));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) != (t35));
  if (t36) {
  {
  t37 = ((int64_t)(v2));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = fgc_vec_resize(t37, t38);
  v3 = ((int64_t)(t39));
  }
  }
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = gg_function_20(t40, t41, base + 0);
  }
  goto label1;
label2:;
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(27))));
  t44 = gg_function_12(t42, t43, base + 0);
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_18(t44, t45, base + 0);
  }
gg_done:;
}
static int64_t gg_function_47(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
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
  uint8_t t247;
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
  t2 = fgc_record_kind(t1);
  t3 = ((int64_t)(t2));
  t4 = gg_function_11(base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((t3) == (t5));
  gg_function_5(t6, base + 0);
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) <= (t8));
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  gg_function_28(t10, base + 0);
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(v2));
  v3 = gg_function_23(t11, t12, base + 0);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(v3));
  gg_function_6(t13, t14, t15, base + 0);
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(10))));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_6(t16, t17, t18, base + 0);
  v4 = gg_function_48(base + 0);
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(v3));
  gg_function_6(t19, t20, t21, base + 0);
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)(v1));
  gg_function_6(t22, t23, t24, base + 0);
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(((int64_t)UINT64_C(2))));
  t27 = ((int64_t)(v2));
  gg_function_6(t25, t26, t27, base + 0);
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(((int64_t)UINT64_C(3))));
  t30 = ((int64_t)(v2));
  gg_function_6(t28, t29, t30, base + 0);
  t31 = ((int64_t)(v4));
  t32 = ((int64_t)(((int64_t)UINT64_C(4))));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(4))));
  t35 = gg_function_12(t33, t34, base + 0);
  gg_function_6(t31, t32, t35, base + 0);
  t36 = ((int64_t)(v4));
  t37 = ((int64_t)(((int64_t)UINT64_C(5))));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_6(t36, t37, t38, base + 0);
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(((int64_t)UINT64_C(6))));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_6(t39, t40, t41, base + 0);
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(21))));
  t44 = gg_function_12(t42, t43, base + 0);
  t45 = ((int64_t)(v3));
  t46 = fgc_vec_resize(t44, t45);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((t47) >= (t48));
  gg_function_5(t49, base + 0);
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(25))));
  t52 = gg_function_12(t50, t51, base + 0);
  t53 = ((int64_t)(v3));
  t54 = fgc_vec_resize(t52, t53);
  t55 = ((int64_t)(t54));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t55) >= (t56));
  gg_function_5(t57, base + 0);
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(((int64_t)UINT64_C(26))));
  t60 = gg_function_12(t58, t59, base + 0);
  t61 = ((int64_t)(v3));
  t62 = fgc_vec_resize(t60, t61);
  t63 = ((int64_t)(t62));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((t63) >= (t64));
  gg_function_5(t65, base + 0);
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(28))));
  t68 = gg_function_12(t66, t67, base + 0);
  t69 = ((int64_t)(v3));
  t70 = fgc_vec_resize(t68, t69);
  t71 = ((int64_t)(t70));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((t71) >= (t72));
  gg_function_5(t73, base + 0);
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(31))));
  t76 = gg_function_12(t74, t75, base + 0);
  t77 = ((int64_t)(v3));
  t78 = fgc_vec_resize(t76, t77);
  t79 = ((int64_t)(t78));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = ((t79) >= (t80));
  gg_function_5(t81, base + 0);
  t82 = ((int64_t)(v0));
  t83 = ((int64_t)(((int64_t)UINT64_C(24))));
  t84 = gg_function_12(t82, t83, base + 0);
  t85 = ((int64_t)(v3));
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(4))));
  t88 = gg_function_12(t86, t87, base + 0);
  t89 = gg_function_23(t85, t88, base + 0);
  t90 = fgc_vec_resize(t84, t89);
  t91 = ((int64_t)(t90));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t91) >= (t92));
  gg_function_5(t93, base + 0);
  t94 = ((int64_t)(v0));
  t95 = ((int64_t)(((int64_t)UINT64_C(27))));
  t96 = gg_function_12(t94, t95, base + 0);
  t97 = ((int64_t)(v3));
  t98 = ((int64_t)(((int64_t)UINT64_C(1))));
  t99 = gg_function_20(t97, t98, base + 0);
  t100 = fgc_vec_resize(t96, t99);
  t101 = ((int64_t)(t100));
  t102 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = ((t101) >= (t102));
  gg_function_5(t103, base + 0);
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t104 = ((int64_t)(v5));
  t105 = ((int64_t)(v3));
  t106 = ((int64_t)(v0));
  t107 = ((int64_t)(((int64_t)UINT64_C(4))));
  t108 = gg_function_12(t106, t107, base + 0);
  t109 = gg_function_23(t105, t108, base + 0);
  t110 = ((int64_t)(t109));
  if (!(((t104) < (t110)))) goto label2;
  {
  t111 = ((int64_t)(v0));
  t112 = ((int64_t)(((int64_t)UINT64_C(24))));
  t113 = gg_function_12(t111, t112, base + 0);
  t114 = fgc_i64_vec_data(t113);
  t115 = ((int64_t)(t114));
  t116 = ((int64_t)(v5));
  t117 = ((int64_t)(((int64_t)UINT64_C(4))));
  t118 = gg_function_23(t116, t117, base + 0);
  t119 = fgc_raw_ptr_add(t115, t118);
  t120 = ((int64_t)(t119));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  fgc_raw_store_i32(t120, t121);
  t122 = ((int64_t)(v5));
  t123 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = gg_function_20(t122, t123, base + 0);
  }
  goto label1;
label2:;
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t124 = ((int64_t)(v6));
  t125 = ((int64_t)(v3));
  if (!(((t124) < (t125)))) goto label5;
  {
  t126 = ((int64_t)(v6));
  t127 = ((int64_t)(v6));
  t128 = ((int64_t)(v2));
  t129 = gg_function_52(t127, t128, base + 0);
  t130 = ((int64_t)(v2));
  t131 = gg_function_23(t129, t130, base + 0);
  v7 = gg_function_53(t126, t131, base + 0);
  t132 = ((int64_t)(v0));
  t133 = ((int64_t)(((int64_t)UINT64_C(21))));
  t134 = gg_function_12(t132, t133, base + 0);
  t135 = fgc_i64_vec_data(t134);
  t136 = ((int64_t)(t135));
  t137 = ((int64_t)(v6));
  t138 = ((int64_t)(((int64_t)UINT64_C(4))));
  t139 = gg_function_23(t137, t138, base + 0);
  t140 = fgc_raw_ptr_add(t136, t139);
  t141 = ((int64_t)(t140));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  fgc_raw_store_i32(t141, t142);
  t143 = ((int64_t)(v0));
  t144 = ((int64_t)(((int64_t)UINT64_C(25))));
  t145 = gg_function_12(t143, t144, base + 0);
  t146 = fgc_i64_vec_data(t145);
  t147 = ((int64_t)(t146));
  t148 = ((int64_t)(v6));
  t149 = ((int64_t)(((int64_t)UINT64_C(4))));
  t150 = gg_function_23(t148, t149, base + 0);
  t151 = fgc_raw_ptr_add(t147, t150);
  t152 = ((int64_t)(t151));
  t153 = ((int64_t)(((int64_t)UINT64_C(1))));
  fgc_raw_store_i32(t152, t153);
  t154 = ((int64_t)(v0));
  t155 = ((int64_t)(((int64_t)UINT64_C(27))));
  t156 = gg_function_12(t154, t155, base + 0);
  t157 = fgc_i64_vec_data(t156);
  t158 = ((int64_t)(t157));
  t159 = ((int64_t)(v6));
  t160 = ((int64_t)(((int64_t)UINT64_C(4))));
  t161 = gg_function_23(t159, t160, base + 0);
  t162 = fgc_raw_ptr_add(t158, t161);
  t163 = ((int64_t)(t162));
  t164 = ((int64_t)(v6));
  fgc_raw_store_i32(t163, t164);
  t165 = ((int64_t)(v0));
  t166 = ((int64_t)(((int64_t)UINT64_C(26))));
  t167 = gg_function_12(t165, t166, base + 0);
  t168 = fgc_i64_vec_data(t167);
  t169 = ((int64_t)(t168));
  t170 = ((int64_t)(v6));
  t171 = ((int64_t)(((int64_t)UINT64_C(4))));
  t172 = gg_function_23(t170, t171, base + 0);
  t173 = fgc_raw_ptr_add(t169, t172);
  t174 = ((int64_t)(t173));
  t175 = ((int64_t)(v7));
  fgc_raw_store_i32(t174, t175);
  t176 = ((int64_t)(v0));
  t177 = ((int64_t)(((int64_t)UINT64_C(28))));
  t178 = gg_function_12(t176, t177, base + 0);
  t179 = fgc_i64_vec_data(t178);
  t180 = ((int64_t)(t179));
  t181 = ((int64_t)(v6));
  t182 = fgc_raw_ptr_add(t180, t181);
  t183 = ((int64_t)(t182));
  t184 = ((int64_t)(((int64_t)UINT64_C(0))));
  fgc_raw_store_i8(t183, t184);
  t185 = ((int64_t)(v6));
  t186 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = gg_function_20(t185, t186, base + 0);
  }
  goto label4;
label5:;
  t187 = ((int64_t)(v0));
  t188 = ((int64_t)(((int64_t)UINT64_C(27))));
  t189 = gg_function_12(t187, t188, base + 0);
  t190 = fgc_i64_vec_data(t189);
  t191 = ((int64_t)(t190));
  t192 = ((int64_t)(v3));
  t193 = ((int64_t)(((int64_t)UINT64_C(4))));
  t194 = gg_function_23(t192, t193, base + 0);
  t195 = fgc_raw_ptr_add(t191, t194);
  t196 = ((int64_t)(t195));
  t197 = ((int64_t)(v3));
  fgc_raw_store_i32(t196, t197);
  t198 = ((int64_t)(v4));
  t199 = ((int64_t)(((int64_t)UINT64_C(20))));
  t200 = gg_function_12(t198, t199, base + 0);
  t201 = ((int64_t)(v0));
  t202 = ((int64_t)(((int64_t)UINT64_C(21))));
  t203 = gg_function_12(t201, t202, base + 0);
  gg_function_54(t200, t203, base + 0);
  t204 = ((int64_t)(v4));
  t205 = ((int64_t)(((int64_t)UINT64_C(24))));
  t206 = gg_function_12(t204, t205, base + 0);
  t207 = ((int64_t)(v0));
  t208 = ((int64_t)(((int64_t)UINT64_C(25))));
  t209 = gg_function_12(t207, t208, base + 0);
  gg_function_54(t206, t209, base + 0);
  t210 = ((int64_t)(v4));
  t211 = ((int64_t)(((int64_t)UINT64_C(25))));
  t212 = gg_function_12(t210, t211, base + 0);
  t213 = ((int64_t)(v0));
  t214 = ((int64_t)(((int64_t)UINT64_C(26))));
  t215 = gg_function_12(t213, t214, base + 0);
  gg_function_54(t212, t215, base + 0);
  t216 = ((int64_t)(v4));
  t217 = ((int64_t)(((int64_t)UINT64_C(26))));
  t218 = gg_function_12(t216, t217, base + 0);
  t219 = ((int64_t)(v0));
  t220 = ((int64_t)(((int64_t)UINT64_C(27))));
  t221 = gg_function_12(t219, t220, base + 0);
  gg_function_54(t218, t221, base + 0);
  t222 = ((int64_t)(v4));
  t223 = ((int64_t)(((int64_t)UINT64_C(29))));
  t224 = gg_function_12(t222, t223, base + 0);
  t225 = ((int64_t)(v0));
  t226 = ((int64_t)(((int64_t)UINT64_C(28))));
  t227 = gg_function_12(t225, t226, base + 0);
  gg_function_55(t224, t227, base + 0);
  t228 = ((int64_t)(v4));
  t229 = ((int64_t)(((int64_t)UINT64_C(27))));
  t230 = gg_function_12(t228, t229, base + 0);
  t231 = ((int64_t)(v2));
  t232 = fgc_vec_resize(t230, t231);
  t233 = ((int64_t)(t232));
  t234 = ((int64_t)(((int64_t)UINT64_C(0))));
  t235 = ((t233) >= (t234));
  gg_function_5(t235, base + 0);
  t236 = ((int64_t)(v4));
  t237 = ((int64_t)(((int64_t)UINT64_C(28))));
  t238 = gg_function_12(t236, t237, base + 0);
  t239 = ((int64_t)(v0));
  t240 = ((int64_t)(((int64_t)UINT64_C(0))));
  t241 = gg_function_12(t239, t240, base + 0);
  t242 = ((int64_t)(((int64_t)UINT64_C(3))));
  t243 = gg_function_12(t241, t242, base + 0);
  t244 = fgc_vec_resize(t238, t243);
  t245 = ((int64_t)(t244));
  t246 = ((int64_t)(((int64_t)UINT64_C(0))));
  t247 = ((t245) >= (t246));
  gg_function_5(t247, base + 0);
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t248 = ((int64_t)(v8));
  t249 = ((int64_t)(v2));
  if (!(((t248) < (t249)))) goto label8;
  {
  t250 = ((int64_t)(v4));
  t251 = ((int64_t)(((int64_t)UINT64_C(27))));
  t252 = gg_function_12(t250, t251, base + 0);
  t253 = fgc_i64_vec_data(t252);
  t254 = ((int64_t)(t253));
  t255 = ((int64_t)(v8));
  t256 = ((int64_t)(((int64_t)UINT64_C(4))));
  t257 = gg_function_23(t255, t256, base + 0);
  t258 = fgc_raw_ptr_add(t254, t257);
  t259 = ((int64_t)(t258));
  t260 = ((int64_t)(v8));
  fgc_raw_store_i32(t259, t260);
  t261 = ((int64_t)(v4));
  t262 = ((int64_t)(((int64_t)UINT64_C(28))));
  t263 = gg_function_12(t261, t262, base + 0);
  t264 = fgc_i64_vec_data(t263);
  t265 = ((int64_t)(t264));
  t266 = ((int64_t)(v8));
  t267 = ((int64_t)(((int64_t)UINT64_C(4))));
  t268 = gg_function_23(t266, t267, base + 0);
  t269 = fgc_raw_ptr_add(t265, t268);
  t270 = ((int64_t)(t269));
  t271 = ((int64_t)(v8));
  fgc_raw_store_i32(t270, t271);
  t272 = ((int64_t)(v8));
  t273 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = gg_function_20(t272, t273, base + 0);
  }
  goto label7;
label8:;
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_46(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) < (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = gg_function_12(t6, t7, base + 0);
  t9 = ((int64_t)(t8));
  t4 = ((t5) >= (t9));
  }
  t10 = t4;
  if (t10) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(29))));
  t13 = gg_function_12(t11, t12, base + 0);
  t14 = fgc_i64_vec_data(t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  t18 = gg_function_23(t16, t17, base + 0);
  t19 = fgc_raw_ptr_add(t15, t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(v2));
  fgc_raw_store_i32(t20, t21);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_45(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t22;
  int64_t t23;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) < (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = gg_function_12(t6, t7, base + 0);
  t9 = ((int64_t)(t8));
  t4 = ((t5) >= (t9));
  }
  t10 = t4;
  if (t10) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(28))));
  t13 = gg_function_12(t11, t12, base + 0);
  t14 = fgc_i64_vec_data(t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(v1));
  t17 = fgc_raw_ptr_add(t15, t16);
  t18 = ((int64_t)(t17));
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) != (t21));
  if (t22) {
  {
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t23 = ((int64_t)(t19));
  fgc_raw_store_i8(t18, t23);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_30(int64_t base) {
  int64_t t0;
  int64_t t1;
  {
  t1 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t1)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_44(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) < (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = gg_function_12(t6, t7, base + 0);
  t9 = ((int64_t)(t8));
  t4 = ((t5) >= (t9));
  }
  t10 = t4;
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) == (t13));
  }
  t14 = t11;
  if (t14) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(4))));
  v3 = gg_function_12(t15, t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(21))));
  t19 = gg_function_12(t17, t18, base + 0);
  t20 = ((int64_t)(v1));
  t21 = gg_function_34(t19, t20, base + 0);
  t22 = ((int64_t)(t21));
  t23 = gg_function_30(base + 0);
  t24 = ((int64_t)(t23));
  t25 = ((t22) == (t24));
  if (t25) {
  {
  v4 = ((int64_t)(v3));
  }
  } else {
  {
  v4 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(24))));
  t28 = gg_function_12(t26, t27, base + 0);
  t29 = fgc_i64_vec_data(t28);
  t30 = ((int64_t)(t29));
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(v3));
  t33 = gg_function_23(t31, t32, base + 0);
  t34 = ((int64_t)(((int64_t)UINT64_C(4))));
  t35 = gg_function_23(t33, t34, base + 0);
  t36 = fgc_raw_ptr_add(t30, t35);
  v5 = ((int64_t)(t36));
  t37 = ((int64_t)(v5));
  t38 = ((int64_t)(v2));
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(((int64_t)UINT64_C(4))));
  t41 = gg_function_23(t39, t40, base + 0);
  fgc_raw_memcpy(t37, t38, t41);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_41(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = fgc_vec_len(t0);
  v3 = ((int64_t)(t1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v2));
  t5 = gg_function_20(t3, t4, base + 0);
  t6 = fgc_vec_resize(t2, t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) >= (t8));
  gg_function_5(t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = fgc_i64_vec_data(t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = gg_function_23(t13, t14, base + 0);
  t16 = fgc_raw_ptr_add(t12, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = gg_function_23(t19, t20, base + 0);
  fgc_raw_memcpy(t17, t18, t21);
  }
gg_done:;
}
static int64_t gg_function_39(int64_t p0, int64_t p1, int64_t base) {
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
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v0));
  t7 = fgc_vec_len(t6);
  t8 = ((int64_t)(t7));
  t4 = ((t5) < (t8));
  }
  t9 = t4;
  gg_function_5(t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = fgc_i64_vec_data(t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(v1));
  t14 = fgc_raw_ptr_add(t12, t13);
  t15 = ((int64_t)(t14));
  t16 = fgc_raw_load_i8(t15);
  t0 = ((int64_t)(t16));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_43(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  int64_t t20;
  uint8_t t21;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) < (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = gg_function_12(t6, t7, base + 0);
  t9 = ((int64_t)(t8));
  t4 = ((t5) >= (t9));
  }
  t10 = t4;
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) == (t13));
  }
  t14 = t11;
  if (t14) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = gg_function_12(t15, t16, base + 0);
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = gg_function_12(t17, t18, base + 0);
  t19 = ((int64_t)(v5));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(v4));
  v6 = gg_function_23(t22, t23, base + 0);
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(v5));
  t26 = ((t24) != (t25));
  if (t26) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(33))));
  t29 = gg_function_12(t27, t28, base + 0);
  t30 = ((int64_t)(v1));
  t31 = gg_function_39(t29, t30, base + 0);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((t32) != (t33));
  if (t34) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(23))));
  t37 = gg_function_12(t35, t36, base + 0);
  t38 = ((int64_t)(v2));
  t39 = ((int64_t)(v6));
  gg_function_41(t37, t38, t39, base + 0);
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(33))));
  t42 = gg_function_12(t40, t41, base + 0);
  t43 = fgc_i64_vec_data(t42);
  t44 = ((int64_t)(t43));
  t45 = ((int64_t)(v1));
  t46 = fgc_raw_ptr_add(t44, t45);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)UINT64_C(1))));
  fgc_raw_store_i8(t47, t48);
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(12))));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_6(t49, t50, t51, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_38(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
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
  uint8_t t46;
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
  int64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) < (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = gg_function_12(t6, t7, base + 0);
  t9 = ((int64_t)(t8));
  t4 = ((t5) >= (t9));
  }
  t10 = t4;
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) == (t13));
  }
  t14 = t11;
  if (t14) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(v4));
  v5 = gg_function_23(t15, t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(2))));
  v6 = gg_function_12(t17, t18, base + 0);
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = gg_function_12(t22, t23, base + 0);
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = gg_function_12(t24, t25, base + 0);
  t26 = ((int64_t)(v7));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t26) > (t27));
  t29 = t28;
  if (t29) {
  t30 = ((int64_t)(v5));
  t31 = ((int64_t)(v7));
  t29 = ((t30) != (t31));
  }
  t32 = t29;
  if (t32) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = ((int64_t)(v5));
  gg_function_6(t33, t34, t35, base + 0);
  }
  } else {
  {
  t36 = ((int64_t)(v5));
  t37 = ((int64_t)(v6));
  t38 = ((t36) != (t37));
  if (t38) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(32))));
  t41 = gg_function_12(t39, t40, base + 0);
  t42 = ((int64_t)(v1));
  t43 = gg_function_39(t41, t42, base + 0);
  t44 = ((int64_t)(t43));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((t44) != (t45));
  if (t46) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(((int64_t)UINT64_C(22))));
  t49 = gg_function_12(t47, t48, base + 0);
  t50 = ((int64_t)(v2));
  t51 = ((int64_t)(v5));
  gg_function_41(t49, t50, t51, base + 0);
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(((int64_t)UINT64_C(32))));
  t54 = gg_function_12(t52, t53, base + 0);
  t55 = fgc_i64_vec_data(t54);
  t56 = ((int64_t)(t55));
  t57 = ((int64_t)(v1));
  t58 = fgc_raw_ptr_add(t56, t57);
  t59 = ((int64_t)(t58));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  fgc_raw_store_i8(t59, t60);
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(11))));
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_6(t61, t62, t63, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_37(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) < (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = gg_function_12(t6, t7, base + 0);
  t9 = ((int64_t)(t8));
  t4 = ((t5) >= (t9));
  }
  t10 = t4;
  if (t10) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) < (t12));
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = gg_function_12(t16, t17, base + 0);
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = gg_function_12(t18, t19, base + 0);
  t21 = ((int64_t)(t20));
  t14 = ((t15) >= (t21));
  }
  t22 = t14;
  if (t22) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(21))));
  t25 = gg_function_12(t23, t24, base + 0);
  t26 = fgc_i64_vec_data(t25);
  t27 = ((int64_t)(t26));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  t30 = gg_function_23(t28, t29, base + 0);
  t31 = fgc_raw_ptr_add(t27, t30);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(v2));
  fgc_raw_store_i32(t32, t33);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_33(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(26))));
  v3 = gg_function_12(t0, t1, base + 0);
  t2 = ((int64_t)(v3));
  t3 = ((int64_t)(v2));
  gg_function_18(t2, t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(25))));
  v4 = gg_function_12(t4, t5, base + 0);
  t6 = ((int64_t)(v4));
  t7 = ((int64_t)(v1));
  v5 = gg_function_34(t6, t7, base + 0);
  t8 = ((int64_t)(v4));
  t9 = fgc_i64_vec_data(t8);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(4))));
  t13 = gg_function_23(t11, t12, base + 0);
  t14 = fgc_raw_ptr_add(t10, t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = gg_function_20(t16, t17, base + 0);
  fgc_raw_store_i32(t15, t18);
  }
gg_done:;
}
static int64_t gg_function_36(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = fgc_record_kind(t1);
  t3 = ((int64_t)(t2));
  t4 = gg_function_11(base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((t3) == (t5));
  gg_function_5(t6, base + 0);
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  v3 = gg_function_12(t7, t8, base + 0);
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) < (t10));
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = gg_function_12(t14, t15, base + 0);
  t17 = ((int64_t)(t16));
  t12 = ((t13) >= (t17));
  }
  t18 = t12;
  if (t18) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) < (t20));
  t22 = t21;
  if (!t22) {
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(3))));
  t26 = gg_function_12(t24, t25, base + 0);
  t27 = ((int64_t)(t26));
  t22 = ((t23) >= (t27));
  }
  t28 = t22;
  if (t28) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(v2));
  gg_function_33(t29, t30, t31, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_31(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = fgc_vec_len(t0);
  v2 = ((int64_t)(t1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(1))));
  t5 = gg_function_20(t3, t4, base + 0);
  t6 = fgc_vec_resize(t2, t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) >= (t8));
  gg_function_5(t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = fgc_i64_vec_data(t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(v2));
  t14 = fgc_raw_ptr_add(t12, t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(v1));
  fgc_raw_store_i8(t15, t16);
  }
gg_done:;
}
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v16;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fgc_record_kind(t1);
  t3 = ((int64_t)(t2));
  t4 = gg_function_11(base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((t3) == (t5));
  gg_function_5(t6, base + 0);
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  v2 = gg_function_12(t7, t8, base + 0);
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) < (t10));
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
  t16 = gg_function_12(t14, t15, base + 0);
  t17 = ((int64_t)(t16));
  t12 = ((t13) >= (t17));
  }
  t18 = t12;
  if (t18) {
  {
  t19 = ((int64_t)UINT64_C(3));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t19)))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = gg_function_12(t20, t21, base + 0);
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = gg_function_20(t24, t25, base + 0);
  gg_function_6(t22, t23, t26, base + 0);
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(21))));
  v4 = gg_function_12(t27, t28, base + 0);
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(20))));
  v5 = gg_function_12(t29, t30, base + 0);
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(28))));
  v6 = gg_function_12(t31, t32, base + 0);
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(29))));
  v7 = gg_function_12(t33, t34, base + 0);
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(32))));
  v8 = gg_function_12(t35, t36, base + 0);
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(33))));
  v9 = gg_function_12(t37, t38, base + 0);
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(25))));
  v10 = gg_function_12(t39, t40, base + 0);
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(27))));
  v11 = gg_function_12(t41, t42, base + 0);
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(26))));
  v12 = gg_function_12(t43, t44, base + 0);
  t45 = ((int64_t)(v4));
  t46 = gg_function_30(base + 0);
  gg_function_18(t45, t46, base + 0);
  t47 = ((int64_t)(v5));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_31(t47, t48, base + 0);
  t49 = ((int64_t)(v6));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_31(t49, t50, base + 0);
  t51 = ((int64_t)(v7));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_18(t51, t52, base + 0);
  t53 = ((int64_t)(v8));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_31(t53, t54, base + 0);
  t55 = ((int64_t)(v9));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_31(t55, t56, base + 0);
  t57 = ((int64_t)(v10));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_18(t57, t58, base + 0);
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(4))));
  v13 = gg_function_12(t59, t60, base + 0);
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(24))));
  v14 = gg_function_12(t61, t62, base + 0);
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t63 = ((int64_t)(v15));
  t64 = ((int64_t)(v13));
  if (!(((t63) < (t64)))) goto label2;
  {
  t65 = ((int64_t)(v14));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_18(t65, t66, base + 0);
  t67 = ((int64_t)(v15));
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = gg_function_20(t67, t68, base + 0);
  }
  goto label1;
label2:;
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(v3));
  t71 = ((int64_t)(v1));
  gg_function_33(t69, t70, t71, base + 0);
  t72 = ((int64_t)(v12));
  t73 = fgc_vec_len(t72);
  v16 = ((int64_t)(t73));
  t74 = ((int64_t)(v11));
  t75 = ((int64_t)(v16));
  gg_function_18(t74, t75, base + 0);
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_26(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(20))));
label1:;
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(33))));
  if (!(((t0) <= (t1)))) goto label2;
  {
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v1));
  v2 = gg_function_12(t2, t3, base + 0);
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v2));
  t8 = fgc_i64_vec_free(t7);
  v3 = ((int64_t)(t8));
  }
  }
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = gg_function_20(t9, t10, base + 0);
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_15(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  /* FG_SHARED_BODY:51 */
  gg_function_51(p0, p1, p2, base);
}
static void gg_function_14(int64_t p0, int64_t base) {
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
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(20))));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_51(t0, t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(21))));
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t3, t4, t5, base + 0);
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(22))));
  t8 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t6, t7, t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(23))));
  t11 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t9, t10, t11, base + 0);
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(24))));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t12, t13, t14, base + 0);
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(25))));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t15, t16, t17, base + 0);
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(26))));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t18, t19, t20, base + 0);
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(27))));
  t23 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t21, t22, t23, base + 0);
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(28))));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_51(t24, t25, t26, base + 0);
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(29))));
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t27, t28, t29, base + 0);
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(30))));
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_51(t30, t31, t32, base + 0);
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(31))));
  t35 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_51(t33, t34, t35, base + 0);
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(32))));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_51(t36, t37, t38, base + 0);
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(33))));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_51(t39, t40, t41, base + 0);
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(27))));
  t44 = gg_function_12(t42, t43, base + 0);
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_18(t44, t45, base + 0);
  }
gg_done:;
}
static int64_t gg_function_3(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1129601073))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_9(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = fgc_record_kind(t1);
  t3 = ((int64_t)(t2));
  t4 = gg_function_3(base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((t3) == (t5));
  gg_function_5(t6, base + 0);
  t7 = gg_function_11(base + 0);
  t8 = fgc_record_new(t7);
  v1 = ((int64_t)(t8));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) != (t10));
  gg_function_5(t11, base + 0);
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((int64_t)(v0));
  gg_function_6(t12, t13, t14, base + 0);
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t15, t16, t17, base + 0);
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t18, t19, t20, base + 0);
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(3))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t21, t22, t23, base + 0);
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(((int64_t)UINT64_C(4))));
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(2))));
  t28 = gg_function_12(t26, t27, base + 0);
  gg_function_6(t24, t25, t28, base + 0);
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(5))));
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(3))));
  t33 = gg_function_12(t31, t32, base + 0);
  gg_function_6(t29, t30, t33, base + 0);
  t34 = ((int64_t)(v1));
  t35 = ((int64_t)(((int64_t)UINT64_C(6))));
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(4))));
  t38 = gg_function_12(t36, t37, base + 0);
  gg_function_6(t34, t35, t38, base + 0);
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(((int64_t)UINT64_C(7))));
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(6))));
  t43 = gg_function_12(t41, t42, base + 0);
  gg_function_6(t39, t40, t43, base + 0);
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(((int64_t)UINT64_C(8))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t44, t45, t46, base + 0);
  t47 = ((int64_t)(v1));
  t48 = ((int64_t)(((int64_t)UINT64_C(9))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t47, t48, t49, base + 0);
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(((int64_t)UINT64_C(10))));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t50, t51, t52, base + 0);
  t53 = ((int64_t)(v1));
  t54 = ((int64_t)(((int64_t)UINT64_C(11))));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t53, t54, t55, base + 0);
  t56 = ((int64_t)(v1));
  t57 = ((int64_t)(((int64_t)UINT64_C(12))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t56, t57, t58, base + 0);
  t59 = ((int64_t)(v1));
  t60 = ((int64_t)(((int64_t)UINT64_C(13))));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t59, t60, t61, base + 0);
  t62 = ((int64_t)(v1));
  gg_function_14(t62, base + 0);
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  v5 = p5;
  {
  t1 = gg_function_3(base + 0);
  t2 = fgc_record_new(t1);
  v6 = ((int64_t)(t2));
  t3 = ((int64_t)(v6));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  gg_function_5(t5, base + 0);
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(v0));
  gg_function_6(t6, t7, t8, base + 0);
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((int64_t)(v1));
  gg_function_6(t9, t10, t11, base + 0);
  t12 = ((int64_t)(v6));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(v2));
  gg_function_6(t12, t13, t14, base + 0);
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(((int64_t)UINT64_C(3))));
  t17 = ((int64_t)(v3));
  gg_function_6(t15, t16, t17, base + 0);
  t18 = ((int64_t)(v6));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = ((int64_t)(v4));
  gg_function_6(t18, t19, t20, base + 0);
  t21 = ((int64_t)(v6));
  t22 = ((int64_t)(((int64_t)UINT64_C(5))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_6(t21, t22, t23, base + 0);
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)UINT64_C(6))));
  t26 = ((int64_t)(v5));
  gg_function_6(t24, t25, t26, base + 0);
  t0 = ((int64_t)(v6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
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
  int64_t t46;
  int64_t t47;
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
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
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
  uint8_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
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
  uint8_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  uint8_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint8_t t124;
  int64_t t125;
  int64_t t126;
  uint8_t t127;
  int64_t t128;
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
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
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
  t8 = ((int64_t)(v5));
  t9 = ((int64_t)(v6));
  t0 = gg_function_2(t4, t5, t6, t7, t8, t9, base + 0);
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t13 = ((int64_t)(v1));
  t14 = fgc_i64_record_free(t13);
  t0 = ((int64_t)(t14));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v1));
  t0 = gg_function_9(t18, base + 0);
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(3))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = ((int64_t)(v1));
  gg_function_26(t22, base + 0);
  t23 = ((int64_t)(v1));
  t24 = fgc_i64_record_free(t23);
  t0 = ((int64_t)(t24));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(4))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v1));
  gg_function_28(t28, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(5))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(v2));
  t0 = gg_function_29(t32, t33, base + 0);
  goto gg_done;
  }
  }
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(6))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  t37 = ((int64_t)(v1));
  t38 = ((int64_t)(v2));
  t39 = ((int64_t)(v3));
  t0 = gg_function_36(t37, t38, t39, base + 0);
  goto gg_done;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(7))));
  t42 = ((t40) == (t41));
  if (t42) {
  {
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(v2));
  t45 = ((int64_t)(v3));
  t0 = gg_function_37(t43, t44, t45, base + 0);
  goto gg_done;
  }
  }
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(8))));
  t48 = ((t46) == (t47));
  if (t48) {
  {
  t49 = ((int64_t)(v1));
  t50 = ((int64_t)(v2));
  t51 = ((int64_t)(v3));
  t52 = ((int64_t)(v4));
  t53 = ((int64_t)(v5));
  t0 = gg_function_38(t49, t50, t51, t52, t53, base + 0);
  goto gg_done;
  }
  }
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(((int64_t)UINT64_C(9))));
  t56 = ((t54) == (t55));
  if (t56) {
  {
  t57 = ((int64_t)(v1));
  t58 = ((int64_t)(v2));
  t59 = ((int64_t)(v3));
  t60 = ((int64_t)(v4));
  t61 = ((int64_t)(v5));
  t0 = gg_function_43(t57, t58, t59, t60, t61, base + 0);
  goto gg_done;
  }
  }
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(((int64_t)UINT64_C(10))));
  t64 = ((t62) == (t63));
  if (t64) {
  {
  t65 = ((int64_t)(v1));
  t66 = ((int64_t)(v2));
  t67 = ((int64_t)(v3));
  t0 = gg_function_44(t65, t66, t67, base + 0);
  goto gg_done;
  }
  }
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(11))));
  t70 = ((t68) == (t69));
  if (t70) {
  {
  t71 = ((int64_t)(v1));
  t72 = ((int64_t)(v2));
  t73 = ((int64_t)(v3));
  t0 = gg_function_45(t71, t72, t73, base + 0);
  goto gg_done;
  }
  }
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(12))));
  t76 = ((t74) == (t75));
  if (t76) {
  {
  t77 = ((int64_t)(v1));
  t78 = ((int64_t)(v2));
  t79 = ((int64_t)(v3));
  t0 = gg_function_46(t77, t78, t79, base + 0);
  goto gg_done;
  }
  }
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(((int64_t)UINT64_C(13))));
  t82 = ((t80) == (t81));
  if (t82) {
  {
  t83 = ((int64_t)(v1));
  t84 = ((int64_t)(v2));
  t85 = ((int64_t)(v3));
  t0 = gg_function_47(t83, t84, t85, base + 0);
  goto gg_done;
  }
  }
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(14))));
  t88 = ((t86) == (t87));
  if (t88) {
  {
  t89 = ((int64_t)(v1));
  t0 = gg_function_56(t89, base + 0);
  goto gg_done;
  }
  }
  t90 = ((int64_t)(v0));
  t91 = ((int64_t)(((int64_t)UINT64_C(15))));
  t92 = ((t90) == (t91));
  if (t92) {
  {
  t93 = ((int64_t)(v1));
  t94 = fgc_record_kind(t93);
  t95 = ((int64_t)(t94));
  t96 = gg_function_49(base + 0);
  t97 = ((int64_t)(t96));
  t98 = ((t95) == (t97));
  gg_function_5(t98, base + 0);
  v7 = ((int64_t)(((int64_t)UINT64_C(20))));
label1:;
  t99 = ((int64_t)(v7));
  t100 = ((int64_t)(((int64_t)UINT64_C(33))));
  if (!(((t99) <= (t100)))) goto label2;
  {
  t101 = ((int64_t)(v1));
  t102 = ((int64_t)(v7));
  v8 = gg_function_12(t101, t102, base + 0);
  t103 = ((int64_t)(v8));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t105 = ((t103) != (t104));
  if (t105) {
  {
  t106 = ((int64_t)(v8));
  t107 = fgc_i64_vec_free(t106);
  v9 = ((int64_t)(t107));
  }
  }
  t108 = ((int64_t)(v7));
  t109 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = gg_function_20(t108, t109, base + 0);
  }
  goto label1;
label2:;
  t110 = ((int64_t)(v1));
  t111 = fgc_i64_record_free(t110);
  t0 = ((int64_t)(t111));
  goto gg_done;
  }
  }
  t112 = ((int64_t)(v0));
  t113 = ((int64_t)(((int64_t)UINT64_C(16))));
  t114 = ((t112) == (t113));
  if (t114) {
  {
  t115 = ((int64_t)(v1));
  t116 = ((int64_t)(v2));
  t0 = gg_function_58(t115, t116, base + 0);
  goto gg_done;
  }
  }
  t117 = ((int64_t)(v0));
  t118 = ((int64_t)(((int64_t)UINT64_C(17))));
  t119 = ((t117) == (t118));
  if (t119) {
  {
  t120 = ((int64_t)(v1));
  t121 = ((int64_t)(v2));
  t0 = gg_function_60(t120, t121, base + 0);
  goto gg_done;
  }
  }
  t122 = ((int64_t)(v0));
  t123 = ((int64_t)(((int64_t)UINT64_C(18))));
  t124 = ((t122) == (t123));
  if (t124) {
  {
  t125 = ((int64_t)(v1));
  t126 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = gg_function_12(t125, t126, base + 0);
  goto gg_done;
  }
  }
  t127 = ((uint8_t)UINT64_C(0));
  gg_function_5(t127, base + 0);
  t128 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t128)))));
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
