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
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static int64_t gg_function_12(int64_t p0, int64_t base);
static int64_t gg_function_14(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_16(int64_t p0, int64_t base);
static int64_t gg_function_19(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_22(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_23(int64_t p0, int64_t base);
static int64_t gg_function_27(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_30(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_32(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_34(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_35(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_37(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_40(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_43(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_45(int64_t p0, int64_t base);
static int64_t gg_function_48(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_49(int64_t p0, int64_t base);
static int64_t gg_function_51(int64_t p0, int64_t base);
static int64_t gg_function_53(int64_t p0, int64_t base);
static int64_t gg_function_59(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_60(int64_t p0, int64_t base);
static int64_t gg_function_62(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_64(int64_t p0, int64_t base);
static int64_t gg_function_65(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_68(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_70(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_71(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_77(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_78(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_80(int64_t p0, int64_t base);
static int64_t gg_function_84(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_86(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_87(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_89(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_92(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_93(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_94(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_95(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_96(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_98(int64_t p0, int64_t base);
static int64_t gg_function_106(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_109(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_32(int64_t p0, int64_t p1, int64_t base) {
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
  t3 = fg14_s_t_op_f32_bits(t1, t2);
  t0 = ((int64_t)(t3));
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(27))));
  t3 = ((int64_t)(v1));
  t4 = fg14_s_t_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
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
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg14_s_t_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_14(int64_t p0, int64_t p1, int64_t base) {
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
  t3 = fg14_s_t_src(t1, t2);
  t0 = ((int64_t)(t3));
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = ((int64_t)(v1));
  t4 = fg14_s_t_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_48(int64_t p0, int64_t p1, int64_t base) {
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
  t2 = ((int64_t)(((int64_t)UINT64_C(7))));
  t3 = ((int64_t)(v1));
  t4 = fg14_s_t_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_16(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg14_s_t_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_45(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_16(t1, base + 0);
  t3 = fg14_s_t_is_quant_type(t2);
  t0 = ((int64_t)(t3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_11(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
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
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
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
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  uint8_t t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  int64_t t78;
  int64_t t79;
  uint8_t t80;
  int64_t t81;
  int64_t t82;
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
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
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  uint8_t t108;
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
  uint8_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  uint8_t t125;
  uint8_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  uint8_t t132;
  uint8_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  uint8_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
  uint8_t t147;
  int64_t t148;
  int64_t t149;
  uint8_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint8_t t155;
  uint8_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
  int64_t t162;
  int64_t t163;
  uint8_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  uint8_t t170;
  uint8_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  uint8_t t177;
  uint8_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  uint8_t t184;
  uint8_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  uint8_t t191;
  uint8_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  uint8_t t198;
  uint8_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  uint8_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  uint8_t t214;
  uint8_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  uint8_t t224;
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
  uint8_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  uint8_t t246;
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
  uint8_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  uint8_t t266;
  int64_t t267;
  int64_t t268;
  uint8_t t269;
  uint8_t t270;
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
  uint8_t t284;
  uint8_t t285;
  int64_t t286;
  int64_t t287;
  uint8_t t288;
  uint8_t t289;
  int64_t t290;
  int64_t t291;
  uint8_t t292;
  uint8_t t293;
  int64_t t294;
  int64_t t295;
  uint8_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  uint8_t t302;
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
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  if (t7) {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t8 = ((int64_t)(v5));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) != (t9));
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v6));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) != (t13));
  }
  t14 = t11;
  if (t14) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) != (t16));
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(v3));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) == (t20));
  t22 = t21;
  if (!t22) {
  t23 = ((int64_t)(v4));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t23) == (t24));
  }
  t18 = t22;
  }
  t25 = t18;
  if (t25) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t26 = ((int64_t)(v8));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t26) != (t27));
  t29 = t28;
  if (t29) {
  t30 = ((int64_t)(v5));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((t30) == (t31));
  t33 = t32;
  if (!t33) {
  t34 = ((int64_t)(v6));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t34) == (t35));
  }
  t29 = t33;
  }
  t36 = t29;
  if (t36) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t37 = ((int64_t)(v0));
  t38 = gg_function_12(t37, base + 0);
  t39 = ((int64_t)(t38));
  t40 = ((int64_t)(((int64_t)UINT64_C(29))));
  t41 = ((t39) == (t40));
  t42 = t41;
  if (t42) {
  t43 = ((int64_t)(v1));
  t44 = gg_function_12(t43, base + 0);
  t45 = ((int64_t)(t44));
  t46 = ((int64_t)(((int64_t)UINT64_C(29))));
  t42 = ((t45) == (t46));
  }
  t47 = t42;
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(v2));
  t50 = gg_function_12(t49, base + 0);
  t51 = ((int64_t)(t50));
  t52 = ((int64_t)(((int64_t)UINT64_C(100))));
  t48 = ((t51) == (t52));
  }
  t53 = t48;
  if (t53) {
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t54 = ((int64_t)(v0));
  t55 = gg_function_12(t54, base + 0);
  t56 = ((int64_t)(t55));
  t57 = ((int64_t)(((int64_t)UINT64_C(30))));
  t58 = ((t56) == (t57));
  t59 = t58;
  if (t59) {
  t60 = ((int64_t)(v1));
  t61 = gg_function_12(t60, base + 0);
  t62 = ((int64_t)(t61));
  t63 = ((int64_t)(((int64_t)UINT64_C(30))));
  t59 = ((t62) == (t63));
  }
  t64 = t59;
  t65 = t64;
  if (t65) {
  t66 = ((int64_t)(v2));
  t67 = gg_function_12(t66, base + 0);
  t68 = ((int64_t)(t67));
  t69 = ((int64_t)(((int64_t)UINT64_C(100))));
  t65 = ((t68) == (t69));
  }
  t70 = t65;
  if (t70) {
  {
  v10 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t71 = ((int64_t)(v9));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((t71) == (t72));
  t74 = t73;
  if (t74) {
  t75 = ((int64_t)(v10));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((t75) == (t76));
  }
  t77 = t74;
  if (t77) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t78 = ((int64_t)(v9));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = ((t78) != (t79));
  if (t80) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  }
  t81 = ((int64_t)(v8));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = ((t81) != (t82));
  if (t83) {
  {
  v12 = ((int64_t)(v5));
  }
  } else {
  {
  v12 = ((int64_t)(v0));
  }
  }
  t84 = ((int64_t)(v8));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = ((t84) != (t85));
  if (t86) {
  {
  v13 = ((int64_t)(v6));
  }
  } else {
  {
  v13 = ((int64_t)(v1));
  }
  }
  t87 = ((int64_t)(v7));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((t87) != (t88));
  if (t89) {
  {
  v14 = ((int64_t)(v3));
  }
  } else {
  {
  v14 = ((int64_t)(v12));
  }
  }
  t90 = ((int64_t)(v7));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((t90) != (t91));
  if (t92) {
  {
  v15 = ((int64_t)(v4));
  }
  } else {
  {
  v15 = ((int64_t)(v13));
  }
  }
  t93 = ((int64_t)(v2));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = gg_function_14(t93, t94, base + 0);
  t96 = ((int64_t)(t95));
  t97 = ((int64_t)(v15));
  t98 = ((t96) != (t97));
  t99 = t98;
  if (!t99) {
  t100 = ((int64_t)(v2));
  t101 = ((int64_t)(((int64_t)UINT64_C(1))));
  t102 = gg_function_14(t100, t101, base + 0);
  t103 = ((int64_t)(t102));
  t104 = ((int64_t)(v14));
  t99 = ((t103) != (t104));
  }
  t105 = t99;
  if (t105) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t106 = ((int64_t)(v8));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = ((t106) != (t107));
  if (t108) {
  {
  t109 = ((int64_t)(v5));
  t110 = gg_function_12(t109, base + 0);
  t111 = ((int64_t)(t110));
  t112 = ((int64_t)(((int64_t)UINT64_C(7))));
  t113 = ((t111) != (t112));
  t114 = t113;
  if (!t114) {
  t115 = ((int64_t)(v6));
  t116 = gg_function_12(t115, base + 0);
  t117 = ((int64_t)(t116));
  t118 = ((int64_t)(((int64_t)UINT64_C(7))));
  t114 = ((t117) != (t118));
  }
  t119 = t114;
  if (t119) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t120 = ((int64_t)(v5));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = gg_function_14(t120, t121, base + 0);
  t123 = ((int64_t)(t122));
  t124 = ((int64_t)(v0));
  t125 = ((t123) == (t124));
  t126 = t125;
  if (!t126) {
  t127 = ((int64_t)(v5));
  t128 = ((int64_t)(((int64_t)UINT64_C(1))));
  t129 = gg_function_14(t127, t128, base + 0);
  t130 = ((int64_t)(t129));
  t131 = ((int64_t)(v0));
  t126 = ((t130) == (t131));
  }
  t132 = t126;
  t133 = (!(t132));
  if (t133) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t134 = ((int64_t)(v6));
  t135 = ((int64_t)(((int64_t)UINT64_C(0))));
  t136 = gg_function_14(t134, t135, base + 0);
  t137 = ((int64_t)(t136));
  t138 = ((int64_t)(v1));
  t139 = ((t137) == (t138));
  t140 = t139;
  if (!t140) {
  t141 = ((int64_t)(v6));
  t142 = ((int64_t)(((int64_t)UINT64_C(1))));
  t143 = gg_function_14(t141, t142, base + 0);
  t144 = ((int64_t)(t143));
  t145 = ((int64_t)(v1));
  t140 = ((t144) == (t145));
  }
  t146 = t140;
  t147 = (!(t146));
  if (t147) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  t148 = ((int64_t)(v7));
  t149 = ((int64_t)(((int64_t)UINT64_C(0))));
  t150 = ((t148) != (t149));
  if (t150) {
  {
  t151 = ((int64_t)(v3));
  t152 = gg_function_12(t151, base + 0);
  t153 = ((int64_t)(t152));
  t154 = ((int64_t)(v11));
  t155 = ((t153) != (t154));
  t156 = t155;
  if (!t156) {
  t157 = ((int64_t)(v4));
  t158 = gg_function_12(t157, base + 0);
  t159 = ((int64_t)(t158));
  t160 = ((int64_t)(v11));
  t156 = ((t159) != (t160));
  }
  t161 = t156;
  if (t161) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t162 = ((int64_t)(v11));
  t163 = ((int64_t)(((int64_t)UINT64_C(2))));
  t164 = ((t162) == (t163));
  if (t164) {
  {
  t165 = ((int64_t)(v3));
  t166 = ((int64_t)(((int64_t)UINT64_C(0))));
  t167 = gg_function_14(t165, t166, base + 0);
  t168 = ((int64_t)(t167));
  t169 = ((int64_t)(v12));
  t170 = ((t168) == (t169));
  t171 = t170;
  if (!t171) {
  t172 = ((int64_t)(v3));
  t173 = ((int64_t)(((int64_t)UINT64_C(1))));
  t174 = gg_function_14(t172, t173, base + 0);
  t175 = ((int64_t)(t174));
  t176 = ((int64_t)(v12));
  t171 = ((t175) == (t176));
  }
  t177 = t171;
  t178 = (!(t177));
  if (t178) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t179 = ((int64_t)(v4));
  t180 = ((int64_t)(((int64_t)UINT64_C(0))));
  t181 = gg_function_14(t179, t180, base + 0);
  t182 = ((int64_t)(t181));
  t183 = ((int64_t)(v13));
  t184 = ((t182) == (t183));
  t185 = t184;
  if (!t185) {
  t186 = ((int64_t)(v4));
  t187 = ((int64_t)(((int64_t)UINT64_C(1))));
  t188 = gg_function_14(t186, t187, base + 0);
  t189 = ((int64_t)(t188));
  t190 = ((int64_t)(v13));
  t185 = ((t189) == (t190));
  }
  t191 = t185;
  t192 = (!(t191));
  if (t192) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  } else {
  {
  t193 = ((int64_t)(v3));
  t194 = ((int64_t)(((int64_t)UINT64_C(0))));
  t195 = gg_function_14(t193, t194, base + 0);
  t196 = ((int64_t)(t195));
  t197 = ((int64_t)(v12));
  t198 = ((t196) != (t197));
  t199 = t198;
  if (!t199) {
  t200 = ((int64_t)(v4));
  t201 = ((int64_t)(((int64_t)UINT64_C(0))));
  t202 = gg_function_14(t200, t201, base + 0);
  t203 = ((int64_t)(t202));
  t204 = ((int64_t)(v13));
  t199 = ((t203) != (t204));
  }
  t205 = t199;
  if (t205) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t206 = ((int64_t)(v3));
  t207 = ((int64_t)(((int64_t)UINT64_C(2))));
  t208 = gg_function_14(t206, t207, base + 0);
  t209 = ((int64_t)(t208));
  t210 = ((int64_t)(v0));
  t211 = ((int64_t)(((int64_t)UINT64_C(2))));
  t212 = gg_function_14(t210, t211, base + 0);
  t213 = ((int64_t)(t212));
  t214 = ((t209) != (t213));
  t215 = t214;
  if (!t215) {
  t216 = ((int64_t)(v4));
  t217 = ((int64_t)(((int64_t)UINT64_C(2))));
  t218 = gg_function_14(t216, t217, base + 0);
  t219 = ((int64_t)(t218));
  t220 = ((int64_t)(v1));
  t221 = ((int64_t)(((int64_t)UINT64_C(2))));
  t222 = gg_function_14(t220, t221, base + 0);
  t223 = ((int64_t)(t222));
  t215 = ((t219) != (t223));
  }
  t224 = t215;
  if (t224) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t225 = ((int64_t)(v0));
  t226 = ((int64_t)(((int64_t)UINT64_C(0))));
  t227 = gg_function_14(t225, t226, base + 0);
  t228 = gg_function_16(t227, base + 0);
  t229 = ((int64_t)(t228));
  t230 = ((int64_t)(v1));
  t231 = ((int64_t)(((int64_t)UINT64_C(0))));
  t232 = gg_function_14(t230, t231, base + 0);
  t233 = gg_function_16(t232, base + 0);
  t234 = ((int64_t)(t233));
  t235 = ((t229) != (t234));
  t236 = t235;
  if (!t236) {
  t237 = ((int64_t)(v0));
  t238 = ((int64_t)(((int64_t)UINT64_C(0))));
  t239 = gg_function_14(t237, t238, base + 0);
  t240 = ((int64_t)(v1));
  t241 = ((int64_t)(((int64_t)UINT64_C(0))));
  t242 = gg_function_14(t240, t241, base + 0);
  t243 = fg14_s_t_same_shape(t239, t242);
  t244 = ((int64_t)(t243));
  t245 = ((int64_t)(((int64_t)UINT64_C(0))));
  t236 = ((t244) == (t245));
  }
  t246 = t236;
  t247 = t246;
  if (!t247) {
  t248 = ((int64_t)(v0));
  t249 = ((int64_t)(((int64_t)UINT64_C(0))));
  t250 = gg_function_14(t248, t249, base + 0);
  t251 = ((int64_t)(v1));
  t252 = ((int64_t)(((int64_t)UINT64_C(0))));
  t253 = gg_function_14(t251, t252, base + 0);
  t254 = fg14_s_t_same_stride(t250, t253);
  t255 = ((int64_t)(t254));
  t256 = ((int64_t)(((int64_t)UINT64_C(0))));
  t247 = ((t255) == (t256));
  }
  t257 = t247;
  if (t257) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t258 = ((int64_t)(v0));
  t259 = ((int64_t)(((int64_t)UINT64_C(1))));
  t260 = gg_function_14(t258, t259, base + 0);
  t261 = ((int64_t)(t260));
  t262 = ((int64_t)(v1));
  t263 = ((int64_t)(((int64_t)UINT64_C(1))));
  t264 = gg_function_14(t262, t263, base + 0);
  t265 = ((int64_t)(t264));
  t266 = ((t261) != (t265));
  if (t266) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t267 = ((int64_t)(v10));
  t268 = ((int64_t)(((int64_t)UINT64_C(0))));
  t269 = ((t267) != (t268));
  t270 = t269;
  if (t270) {
  t271 = ((int64_t)(v0));
  t272 = ((int64_t)(((int64_t)UINT64_C(2))));
  t273 = gg_function_14(t271, t272, base + 0);
  t274 = ((int64_t)(t273));
  t275 = ((int64_t)(v1));
  t276 = ((int64_t)(((int64_t)UINT64_C(2))));
  t277 = gg_function_14(t275, t276, base + 0);
  t278 = ((int64_t)(t277));
  t270 = ((t274) != (t278));
  }
  t279 = t270;
  if (t279) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t280 = ((int64_t)(v2));
  t281 = ((int64_t)(((int64_t)UINT64_C(0))));
  v16 = gg_function_19(t280, t281, base + 0);
  t282 = ((int64_t)(v16));
  t283 = ((int64_t)(((int64_t)UINT64_C(2))));
  t284 = ((t282) != (t283));
  t285 = t284;
  if (t285) {
  t286 = ((int64_t)(v16));
  t287 = ((int64_t)(((int64_t)UINT64_C(1))));
  t285 = ((t286) != (t287));
  }
  t288 = t285;
  t289 = t288;
  if (t289) {
  t290 = ((int64_t)(v16));
  t291 = ((int64_t)(((int64_t)UINT64_C(3))));
  t289 = ((t290) != (t291));
  }
  t292 = t289;
  t293 = t292;
  if (t293) {
  t294 = ((int64_t)(v16));
  t295 = ((int64_t)(((int64_t)UINT64_C(6))));
  t293 = ((t294) != (t295));
  }
  t296 = t293;
  if (t296) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t297 = ((int64_t)(v2));
  t298 = ((int64_t)(((int64_t)UINT64_C(1))));
  t299 = gg_function_19(t297, t298, base + 0);
  t300 = ((int64_t)(t299));
  t301 = ((int64_t)(((int64_t)UINT64_C(0))));
  t302 = ((t300) != (t301));
  if (t302) {
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
static int64_t gg_function_9(int64_t p0, int64_t p1, int64_t base) {
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
  t3 = fg14_s_g_node(t1, t2);
  t0 = ((int64_t)(t3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_43(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
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
  uint8_t t31;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v1));
  t4 = gg_function_9(t2, t3, base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((t1) == (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = gg_function_9(t9, t12, base + 0);
  t14 = ((int64_t)(t13));
  t7 = ((t8) == (t14));
  }
  t15 = t7;
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(v1));
  t20 = gg_function_9(t18, t19, base + 0);
  t21 = ((int64_t)(t20));
  t22 = ((t17) == (t21));
  t23 = t22;
  if (t23) {
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  t29 = gg_function_9(t25, t28, base + 0);
  t30 = ((int64_t)(t29));
  t23 = ((t24) == (t30));
  }
  t16 = t23;
  }
  t31 = t16;
  if (t31) {
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
static int64_t gg_function_40(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
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
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  uint8_t t62;
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
  uint8_t t85;
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
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t t100;
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
  uint8_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint8_t t128;
  uint8_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  uint8_t t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  uint8_t t142;
  uint8_t t143;
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
  uint8_t t166;
  uint8_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  uint8_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  uint8_t t186;
  uint8_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  uint8_t t196;
  uint8_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  uint8_t t203;
  uint8_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  uint8_t t209;
  uint8_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  uint8_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  uint8_t t224;
  uint8_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  uint8_t t231;
  uint8_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  uint8_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  uint8_t t243;
  uint8_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  uint8_t t251;
  uint8_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  uint8_t t261;
  uint8_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  uint8_t t271;
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
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t4 = ((int64_t)(v0));
  t5 = fg14_s_g_n(t4);
  t6 = ((int64_t)(t5));
  t7 = ((t3) >= (t6));
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t12 = ((int64_t)(v0));
  t13 = fg14_s_g_n(t12);
  t14 = ((int64_t)(t13));
  t8 = ((t11) >= (t14));
  }
  t15 = t8;
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(3))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
  t16 = ((t17) < (t20));
  }
  t21 = t16;
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(v2));
  t25 = fg14_s_g_can_fuse_shared(t22, t23, t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(((int64_t)UINT64_C(2))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  v3 = gg_function_9(t29, t32, base + 0);
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(v2));
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  v4 = gg_function_9(t33, t36, base + 0);
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = gg_function_14(t37, t38, base + 0);
  t39 = ((int64_t)(v3));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = gg_function_14(t39, t40, base + 0);
  t41 = ((int64_t)(v4));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = gg_function_14(t41, t42, base + 0);
  t43 = ((int64_t)(v4));
  t44 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = gg_function_14(t43, t44, base + 0);
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(v1));
  t47 = ((int64_t)(v5));
  t48 = ((int64_t)(v6));
  t49 = gg_function_43(t45, t46, t47, t48, base + 0);
  t50 = ((int64_t)(t49));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((t50) == (t51));
  t53 = t52;
  if (!t53) {
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(v2));
  t56 = ((int64_t)(v7));
  t57 = ((int64_t)(v8));
  t58 = gg_function_43(t54, t55, t56, t57, base + 0);
  t59 = ((int64_t)(t58));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((t59) == (t60));
  }
  t61 = t53;
  t62 = t61;
  if (!t62) {
  t63 = ((int64_t)(v6));
  t64 = ((int64_t)(v5));
  t65 = ((int64_t)(v3));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = gg_function_11(t63, t64, t65, t66, t67, t68, t69, base + 0);
  t71 = ((int64_t)(t70));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((t71) == (t72));
  }
  t73 = t62;
  t74 = t73;
  if (!t74) {
  t75 = ((int64_t)(v8));
  t76 = ((int64_t)(v7));
  t77 = ((int64_t)(v4));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = gg_function_11(t75, t76, t77, t78, t79, t80, t81, base + 0);
  t83 = ((int64_t)(t82));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((t83) == (t84));
  }
  t85 = t74;
  if (t85) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t86 = ((int64_t)(v6));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = gg_function_14(t86, t87, base + 0);
  t89 = fg14_s_t_has_buffer(t88);
  t90 = ((int64_t)(t89));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((t90) == (t91));
  t93 = t92;
  if (!t93) {
  t94 = ((int64_t)(v6));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = gg_function_14(t94, t95, base + 0);
  t97 = gg_function_45(t96, base + 0);
  t98 = ((int64_t)(t97));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t98) == (t99));
  }
  t100 = t93;
  if (t100) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t101 = ((int64_t)(v6));
  t102 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = gg_function_14(t101, t102, base + 0);
  t103 = ((int64_t)(v6));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  v10 = gg_function_14(t103, t104, base + 0);
  t105 = ((int64_t)(v8));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = gg_function_14(t105, t106, base + 0);
  t107 = ((int64_t)(v9));
  t108 = gg_function_12(t107, base + 0);
  t109 = ((int64_t)(t108));
  t110 = ((int64_t)(((int64_t)UINT64_C(36))));
  t111 = ((t109) != (t110));
  t112 = t111;
  if (!t112) {
  t113 = ((int64_t)(v9));
  t114 = ((int64_t)(((int64_t)UINT64_C(0))));
  t115 = gg_function_14(t113, t114, base + 0);
  t116 = ((int64_t)(t115));
  t117 = ((int64_t)(v8));
  t118 = ((int64_t)(((int64_t)UINT64_C(1))));
  t119 = gg_function_14(t117, t118, base + 0);
  t120 = ((int64_t)(t119));
  t112 = ((t116) != (t120));
  }
  t121 = t112;
  t122 = t121;
  if (!t122) {
  t123 = ((int64_t)(v9));
  t124 = ((int64_t)(((int64_t)UINT64_C(1))));
  t125 = gg_function_29(t123, t124, base + 0);
  t126 = ((int64_t)(t125));
  t127 = ((int64_t)(((int64_t)UINT64_C(1))));
  t122 = ((t126) != (t127));
  }
  t128 = t122;
  t129 = t128;
  if (!t129) {
  t130 = ((int64_t)(v9));
  t131 = ((int64_t)(((int64_t)UINT64_C(3))));
  t132 = gg_function_29(t130, t131, base + 0);
  t133 = ((int64_t)(t132));
  t134 = ((int64_t)(((int64_t)UINT64_C(1))));
  t129 = ((t133) != (t134));
  }
  t135 = t129;
  t136 = t135;
  if (!t136) {
  t137 = ((int64_t)(v9));
  t138 = ((int64_t)(((int64_t)UINT64_C(12))));
  t139 = fg14_s_contig(t137, t138);
  t140 = ((int64_t)(t139));
  t141 = ((int64_t)(((int64_t)UINT64_C(0))));
  t136 = ((t140) == (t141));
  }
  t142 = t136;
  t143 = t142;
  if (!t143) {
  t144 = ((int64_t)(v8));
  t145 = ((int64_t)(((int64_t)UINT64_C(1))));
  t146 = gg_function_14(t144, t145, base + 0);
  t147 = ((int64_t)(((int64_t)UINT64_C(12))));
  t148 = fg14_s_contig(t146, t147);
  t149 = ((int64_t)(t148));
  t150 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = ((t149) == (t150));
  }
  t151 = t143;
  t152 = t151;
  if (!t152) {
  t153 = ((int64_t)(v8));
  t154 = ((int64_t)(((int64_t)UINT64_C(1))));
  t155 = gg_function_14(t153, t154, base + 0);
  t156 = fg14_s_t_is_matrix(t155);
  t157 = ((int64_t)(t156));
  t158 = ((int64_t)(((int64_t)UINT64_C(0))));
  t152 = ((t157) == (t158));
  }
  t159 = t152;
  if (t159) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t160 = ((int64_t)(v10));
  t161 = gg_function_16(t160, base + 0);
  t162 = ((int64_t)(t161));
  t163 = ((int64_t)(v11));
  t164 = gg_function_16(t163, base + 0);
  t165 = ((int64_t)(t164));
  t166 = ((t162) != (t165));
  t167 = t166;
  if (!t167) {
  t168 = ((int64_t)(v10));
  t169 = ((int64_t)(((int64_t)UINT64_C(0))));
  t170 = gg_function_29(t168, t169, base + 0);
  t171 = ((int64_t)(t170));
  t172 = ((int64_t)(v11));
  t173 = ((int64_t)(((int64_t)UINT64_C(0))));
  t174 = gg_function_29(t172, t173, base + 0);
  t175 = ((int64_t)(t174));
  t167 = ((t171) != (t175));
  }
  t176 = t167;
  t177 = t176;
  if (!t177) {
  t178 = ((int64_t)(v10));
  t179 = ((int64_t)(((int64_t)UINT64_C(1))));
  t180 = gg_function_29(t178, t179, base + 0);
  t181 = ((int64_t)(t180));
  t182 = ((int64_t)(v11));
  t183 = ((int64_t)(((int64_t)UINT64_C(1))));
  t184 = gg_function_29(t182, t183, base + 0);
  t185 = ((int64_t)(t184));
  t177 = ((t181) != (t185));
  }
  t186 = t177;
  t187 = t186;
  if (!t187) {
  t188 = ((int64_t)(v10));
  t189 = ((int64_t)(((int64_t)UINT64_C(1))));
  t190 = gg_function_48(t188, t189, base + 0);
  t191 = ((int64_t)(t190));
  t192 = ((int64_t)(v11));
  t193 = ((int64_t)(((int64_t)UINT64_C(1))));
  t194 = gg_function_48(t192, t193, base + 0);
  t195 = ((int64_t)(t194));
  t187 = ((t191) != (t195));
  }
  t196 = t187;
  t197 = t196;
  if (!t197) {
  t198 = ((int64_t)(v10));
  t199 = ((int64_t)(((int64_t)UINT64_C(3))));
  t200 = gg_function_29(t198, t199, base + 0);
  t201 = ((int64_t)(t200));
  t202 = ((int64_t)(((int64_t)UINT64_C(1))));
  t197 = ((t201) != (t202));
  }
  t203 = t197;
  t204 = t203;
  if (!t204) {
  t205 = ((int64_t)(v11));
  t206 = fg14_s_t_is_matrix(t205);
  t207 = ((int64_t)(t206));
  t208 = ((int64_t)(((int64_t)UINT64_C(0))));
  t204 = ((t207) == (t208));
  }
  t209 = t204;
  t210 = t209;
  if (!t210) {
  t211 = ((int64_t)(v11));
  t212 = ((int64_t)(((int64_t)UINT64_C(12))));
  t213 = fg14_s_contig(t211, t212);
  t214 = ((int64_t)(t213));
  t215 = ((int64_t)(((int64_t)UINT64_C(0))));
  t210 = ((t214) == (t215));
  }
  t216 = t210;
  if (t216) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t217 = ((int64_t)(v7));
  t218 = ((int64_t)(((int64_t)UINT64_C(0))));
  t219 = gg_function_14(t217, t218, base + 0);
  t220 = ((int64_t)(((int64_t)UINT64_C(12))));
  t221 = fg14_s_contig(t219, t220);
  t222 = ((int64_t)(t221));
  t223 = ((int64_t)(((int64_t)UINT64_C(0))));
  t224 = ((t222) == (t223));
  t225 = t224;
  if (!t225) {
  t226 = ((int64_t)(v3));
  t227 = ((int64_t)(((int64_t)UINT64_C(12))));
  t228 = fg14_s_contig(t226, t227);
  t229 = ((int64_t)(t228));
  t230 = ((int64_t)(((int64_t)UINT64_C(0))));
  t225 = ((t229) == (t230));
  }
  t231 = t225;
  t232 = t231;
  if (!t232) {
  t233 = ((int64_t)(v4));
  t234 = ((int64_t)(((int64_t)UINT64_C(12))));
  t235 = fg14_s_contig(t233, t234);
  t236 = ((int64_t)(t235));
  t237 = ((int64_t)(((int64_t)UINT64_C(0))));
  t232 = ((t236) == (t237));
  }
  t238 = t232;
  if (t238) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t239 = ((int64_t)(v11));
  t240 = gg_function_12(t239, base + 0);
  t241 = ((int64_t)(t240));
  t242 = ((int64_t)(((int64_t)UINT64_C(0))));
  t243 = ((t241) != (t242));
  t244 = t243;
  if (!t244) {
  t245 = ((int64_t)(v7));
  t246 = ((int64_t)(((int64_t)UINT64_C(0))));
  t247 = gg_function_14(t245, t246, base + 0);
  t248 = gg_function_12(t247, base + 0);
  t249 = ((int64_t)(t248));
  t250 = ((int64_t)(((int64_t)UINT64_C(0))));
  t244 = ((t249) != (t250));
  }
  t251 = t244;
  t252 = t251;
  if (!t252) {
  t253 = ((int64_t)(v3));
  t254 = ((int64_t)(((int64_t)UINT64_C(0))));
  t255 = gg_function_19(t253, t254, base + 0);
  t256 = ((int64_t)(t255));
  t257 = ((int64_t)(v4));
  t258 = ((int64_t)(((int64_t)UINT64_C(0))));
  t259 = gg_function_19(t257, t258, base + 0);
  t260 = ((int64_t)(t259));
  t252 = ((t256) != (t260));
  }
  t261 = t252;
  t262 = t261;
  if (!t262) {
  t263 = ((int64_t)(v3));
  t264 = ((int64_t)(((int64_t)UINT64_C(3))));
  t265 = gg_function_32(t263, t264, base + 0);
  t266 = ((int64_t)(t265));
  t267 = ((int64_t)(v4));
  t268 = ((int64_t)(((int64_t)UINT64_C(3))));
  t269 = gg_function_32(t267, t268, base + 0);
  t270 = ((int64_t)(t269));
  t262 = ((t266) != (t270));
  }
  t271 = t262;
  if (t271) {
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
static int64_t gg_function_84(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(8))));
  t2 = fg14_s_vec_new(t1);
  v3 = ((int64_t)(t2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(((int64_t)UINT64_C(3))));
  t5 = fg14_s_vec_resize(t3, t4);
  (void)(t5);
  t6 = ((int64_t)(v3));
  t7 = fg14_s_vec_data(t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(v0));
  t11 = fg14_s_frame_set(t8, t9, t10);
  (void)(t11);
  t12 = ((int64_t)(v3));
  t13 = fg14_s_vec_data(t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((int64_t)(v1));
  t17 = fg14_s_frame_set(t14, t15, t16);
  (void)(t17);
  t18 = ((int64_t)(v3));
  t19 = fg14_s_vec_data(t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(2))));
  t22 = ((int64_t)(v2));
  t23 = fg14_s_frame_set(t20, t21, t22);
  (void)(t23);
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_109(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  t1 = ((int64_t)(((int64_t)UINT64_C(8))));
  t2 = fg14_s_vec_new(t1);
  v4 = ((int64_t)(t2));
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(((int64_t)UINT64_C(4))));
  t5 = fg14_s_vec_resize(t3, t4);
  (void)(t5);
  t6 = ((int64_t)(v4));
  t7 = fg14_s_vec_data(t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(v0));
  t11 = fg14_s_frame_set(t8, t9, t10);
  (void)(t11);
  t12 = ((int64_t)(v4));
  t13 = fg14_s_vec_data(t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((int64_t)(v1));
  t17 = fg14_s_frame_set(t14, t15, t16);
  (void)(t17);
  t18 = ((int64_t)(v4));
  t19 = fg14_s_vec_data(t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(2))));
  t22 = ((int64_t)(v2));
  t23 = fg14_s_frame_set(t20, t21, t22);
  (void)(t23);
  t24 = ((int64_t)(v4));
  t25 = fg14_s_vec_data(t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(3))));
  t28 = ((int64_t)(v3));
  t29 = fg14_s_frame_set(t26, t27, t28);
  (void)(t29);
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_93(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(8))));
  t2 = fg14_s_vec_new(t1);
  v2 = ((int64_t)(t2));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  t5 = fg14_s_vec_resize(t3, t4);
  (void)(t5);
  t6 = ((int64_t)(v2));
  t7 = fg14_s_vec_data(t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(v0));
  t11 = fg14_s_frame_set(t8, t9, t10);
  (void)(t11);
  t12 = ((int64_t)(v2));
  t13 = fg14_s_vec_data(t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((int64_t)(v1));
  t17 = fg14_s_frame_set(t14, t15, t16);
  (void)(t17);
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_106(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(91))));
  t4 = ((int64_t)(((int64_t)UINT64_C(7))));
  t5 = gg_function_93(t3, t4, base + 0);
  t6 = fg14_s_vec_data(t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  t9 = fg14_s_g_can_fuse(t1, t2, t7, t8);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v1));
  t15 = gg_function_9(t13, t14, base + 0);
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  v2 = gg_function_19(t15, t16, base + 0);
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(10))));
  t19 = ((t17) == (t18));
  t20 = t19;
  if (!t20) {
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(((int64_t)UINT64_C(7))));
  t20 = ((t21) == (t22));
  }
  t23 = t20;
  t24 = t23;
  if (!t24) {
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)UINT64_C(15))));
  t24 = ((t25) == (t26));
  }
  t27 = t24;
  if (t27) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
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
static int64_t gg_function_87(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  t1 = ((int64_t)(((int64_t)UINT64_C(8))));
  t2 = fg14_s_vec_new(t1);
  v5 = ((int64_t)(t2));
  t3 = ((int64_t)(v5));
  t4 = ((int64_t)(((int64_t)UINT64_C(5))));
  t5 = fg14_s_vec_resize(t3, t4);
  (void)(t5);
  t6 = ((int64_t)(v5));
  t7 = fg14_s_vec_data(t6);
  t8 = ((int64_t)(t7));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(v0));
  t11 = fg14_s_frame_set(t8, t9, t10);
  (void)(t11);
  t12 = ((int64_t)(v5));
  t13 = fg14_s_vec_data(t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((int64_t)(v1));
  t17 = fg14_s_frame_set(t14, t15, t16);
  (void)(t17);
  t18 = ((int64_t)(v5));
  t19 = fg14_s_vec_data(t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(2))));
  t22 = ((int64_t)(v2));
  t23 = fg14_s_frame_set(t20, t21, t22);
  (void)(t23);
  t24 = ((int64_t)(v5));
  t25 = fg14_s_vec_data(t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(3))));
  t28 = ((int64_t)(v3));
  t29 = fg14_s_frame_set(t26, t27, t28);
  (void)(t29);
  t30 = ((int64_t)(v5));
  t31 = fg14_s_vec_data(t30);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  t34 = ((int64_t)(v4));
  t35 = fg14_s_frame_set(t32, t33, t34);
  (void)(t35);
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_53(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = fg14_s_t_view_src(t1);
  t0 = ((int64_t)(t2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_51(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = fg14_s_t_nbytes(t1);
  t0 = ((int64_t)(t2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_49(int64_t p0, int64_t base) {
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
  uint8_t t49;
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
  uint8_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  uint8_t t98;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = gg_function_14(t1, t2, base + 0);
  t4 = fg14_s_t_buffer_usage(t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = ((t5) == (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = gg_function_14(t9, t10, base + 0);
  t12 = gg_function_51(t11, base + 0);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = gg_function_14(t14, t15, base + 0);
  t17 = fg14_s_t_alloc_size(t16);
  t18 = ((int64_t)(t17));
  t8 = ((t13) != (t18));
  }
  t19 = t8;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = gg_function_14(t21, t22, base + 0);
  t24 = gg_function_53(t23, base + 0);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t25) != (t26));
  }
  t27 = t20;
  if (t27) {
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) != (t29));
  t31 = t30;
  if (!t31) {
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = gg_function_14(t32, t33, base + 0);
  t35 = gg_function_16(t34, base + 0);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t36) != (t37));
  }
  t38 = t31;
  t39 = t38;
  if (!t39) {
  t40 = ((int64_t)(v0));
  t41 = gg_function_16(t40, base + 0);
  t42 = ((int64_t)(t41));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t42) != (t43));
  }
  t44 = t39;
  if (t44) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = fg14_s_info_cc(t45);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)UINT64_C(600))));
  t49 = ((t47) <= (t48));
  if (t49) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t50 = ((int64_t)(v0));
  t51 = gg_function_12(t50, base + 0);
  t52 = ((int64_t)(t51));
  t53 = ((int64_t)(((int64_t)UINT64_C(29))));
  t54 = ((t52) == (t53));
  t55 = t54;
  if (t55) {
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  t58 = gg_function_29(t56, t57, base + 0);
  t59 = ((int64_t)(t58));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((t59) != (t60));
  }
  t61 = t55;
  if (t61) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t62 = ((int64_t)(v0));
  t63 = gg_function_12(t62, base + 0);
  t64 = ((int64_t)(t63));
  t65 = ((int64_t)(((int64_t)UINT64_C(30))));
  t66 = ((t64) == (t65));
  t67 = t66;
  if (t67) {
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(2))));
  t70 = gg_function_29(t68, t69, base + 0);
  t71 = ((int64_t)(t70));
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = gg_function_14(t72, t73, base + 0);
  t75 = gg_function_16(t74, base + 0);
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = fg14_s_info_cc(t76);
  t78 = ((int64_t)(t77));
  t79 = fg14_s_mmvq_mmid_max_batch(t75, t78);
  t80 = ((int64_t)(t79));
  t67 = ((t71) > (t80));
  }
  t81 = t67;
  if (t81) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t82 = ((int64_t)(v0));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = gg_function_14(t82, t83, base + 0);
  t85 = gg_function_45(t84, base + 0);
  t86 = ((int64_t)(t85));
  t87 = ((int64_t)(((int64_t)UINT64_C(1))));
  t88 = ((t86) == (t87));
  t89 = t88;
  if (t89) {
  t90 = ((int64_t)(v0));
  t91 = ((int64_t)(((int64_t)UINT64_C(1))));
  t92 = gg_function_14(t90, t91, base + 0);
  t93 = ((int64_t)(((int64_t)UINT64_C(1))));
  t94 = gg_function_29(t92, t93, base + 0);
  t95 = ((int64_t)(t94));
  t96 = fg14_s_mmvq_max_batch();
  t97 = ((int64_t)(t96));
  t89 = ((t95) <= (t97));
  }
  t98 = t89;
  if (t98) {
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
static int64_t gg_function_98(int64_t p0, int64_t base) {
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
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  uint8_t t25;
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
  uint8_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = gg_function_14(t1, t2, base + 0);
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  v1 = gg_function_29(t3, t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = gg_function_12(t5, base + 0);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(29))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = gg_function_14(t10, t11, base + 0);
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = gg_function_29(t12, t13, base + 0);
  }
  }
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(v1));
  t16 = fg14_s_should_use_mmvf_t(t14, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = gg_function_12(t20, base + 0);
  t22 = ((int64_t)(t21));
  t23 = ((int64_t)(((int64_t)UINT64_C(29))));
  t24 = ((t22) == (t23));
  t25 = t24;
  if (t25) {
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = gg_function_29(t26, t27, base + 0);
  t29 = ((int64_t)(t28));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t29) != (t30));
  }
  t31 = t25;
  if (t31) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t32 = ((int64_t)(v0));
  t33 = gg_function_12(t32, base + 0);
  t34 = ((int64_t)(t33));
  t35 = ((int64_t)(((int64_t)UINT64_C(30))));
  t36 = ((t34) == (t35));
  t37 = t36;
  if (t37) {
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(2))));
  t40 = gg_function_29(t38, t39, base + 0);
  t41 = ((int64_t)(t40));
  t42 = ((int64_t)(((int64_t)UINT64_C(1))));
  t37 = ((t41) != (t42));
  }
  t43 = t37;
  if (t43) {
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
static int64_t gg_function_96(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = gg_function_14(t4, t5, base + 0);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(v1));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = gg_function_14(t10, t11, base + 0);
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = gg_function_14(t12, t13, base + 0);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(v1));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = gg_function_14(t18, t19, base + 0);
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = gg_function_14(t20, t21, base + 0);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(v1));
  t25 = ((t23) != (t24));
  if (t25) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = gg_function_14(t26, t27, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_95(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
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
  uint8_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = gg_function_14(t1, t2, base + 0);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(v1));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  t9 = gg_function_14(t7, t8, base + 0);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(v1));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) == (t14));
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t17) == (t18));
  }
  t19 = t16;
  if (t19) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  v4 = gg_function_14(t20, t21, base + 0);
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t22) != (t23));
  if (t24) {
  {
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = gg_function_14(t25, t26, base + 0);
  }
  }
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = gg_function_14(t27, t28, base + 0);
  t30 = gg_function_16(t29, base + 0);
  t31 = ((int64_t)(t30));
  t32 = ((int64_t)(((int64_t)UINT64_C(40))));
  t33 = ((t31) != (t32));
  t34 = t33;
  if (!t34) {
  t35 = ((int64_t)(v0));
  t36 = gg_function_16(t35, base + 0);
  t37 = ((int64_t)(t36));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((t37) != (t38));
  }
  t39 = t34;
  t40 = t39;
  if (!t40) {
  t41 = ((int64_t)(v4));
  t42 = gg_function_16(t41, base + 0);
  t43 = ((int64_t)(t42));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t43) != (t44));
  }
  t45 = t40;
  t46 = t45;
  if (!t46) {
  t47 = ((int64_t)(v4));
  t48 = ((int64_t)(((int64_t)UINT64_C(12))));
  t49 = fg14_s_contig(t47, t48);
  t50 = ((int64_t)(t49));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((t50) == (t51));
  }
  t52 = t46;
  t53 = t52;
  if (!t53) {
  t54 = ((int64_t)(v4));
  t55 = fg14_s_t_nelements(t54);
  t56 = ((int64_t)(t55));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = ((t56) != (t57));
  }
  t58 = t53;
  t59 = t58;
  if (!t59) {
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(v1));
  t62 = fg14_s_t_same_shape(t60, t61);
  t63 = ((int64_t)(t62));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((t63) == (t64));
  }
  t65 = t59;
  if (t65) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_94(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  v4 = gg_function_9(t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  v5 = gg_function_9(t3, t6, base + 0);
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((int64_t)UINT64_C(29))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  v5 = gg_function_9(t10, t13, base + 0);
  }
  }
  v6 = ((int64_t)(v5));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)UINT64_C(29))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(2))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  v6 = gg_function_9(t20, t23, base + 0);
  }
  } else {
  {
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(5))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  v6 = gg_function_9(t24, t27, base + 0);
  }
  }
  }
  }
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)UINT64_C(29))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t31 = ((int64_t)(v5));
  t32 = ((int64_t)(v4));
  v7 = gg_function_95(t31, t32, base + 0);
  }
  }
  t33 = ((int64_t)(v7));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t36 = ((int64_t)(v2));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(3))));
  t39 = ((int64_t)(v3));
  t40 = ((int64_t)(((int64_t)UINT64_C(29))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  }
  t42 = ((int64_t)(v6));
  t43 = ((int64_t)(v5));
  t44 = ((int64_t)(v8));
  v9 = gg_function_96(t42, t43, t44, base + 0);
  t45 = ((int64_t)(v9));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_92(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
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
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
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
  uint8_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint8_t t79;
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
  uint8_t t101;
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
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  uint8_t t142;
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
  int64_t t157;
  uint8_t t158;
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
  uint8_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
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
  t2 = ((int64_t)(v2));
  v3 = gg_function_9(t1, t2, base + 0);
  t3 = ((int64_t)(v3));
  t4 = gg_function_12(t3, base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(29))));
  t7 = ((t5) != (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v3));
  t10 = gg_function_12(t9, base + 0);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(30))));
  t8 = ((t11) != (t12));
  }
  t13 = t8;
  if (t13) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  if (!(((t14) < (t15)))) goto label2;
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(30))));
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(29))));
  }
  }
  t19 = ((int64_t)(v3));
  t20 = gg_function_12(t19, base + 0);
  t21 = ((int64_t)(t20));
  t22 = ((int64_t)(v5));
  t23 = ((t21) == (t22));
  if (t23) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(3))));
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) == (t25));
  if (t26) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  }
  t27 = ((int64_t)(v3));
  t28 = gg_function_12(t27, base + 0);
  t29 = ((int64_t)(t28));
  t30 = ((int64_t)(((int64_t)UINT64_C(29))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(v2));
  t34 = ((int64_t)(((int64_t)UINT64_C(29))));
  t35 = ((int64_t)(((int64_t)UINT64_C(7))));
  t36 = gg_function_93(t34, t35, base + 0);
  t37 = fg14_s_vec_data(t36);
  t38 = ((int64_t)(t37));
  t39 = ((int64_t)(((int64_t)UINT64_C(2))));
  t40 = fg14_s_g_can_fuse(t32, t33, t38, t39);
  t41 = ((int64_t)(t40));
  t42 = ((int64_t)(((int64_t)UINT64_C(1))));
  t43 = ((t41) == (t42));
  if (t43) {
  {
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(v2));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((int64_t)(((int64_t)UINT64_C(29))));
  t48 = gg_function_94(t44, t45, t46, t47, base + 0);
  t49 = ((int64_t)(t48));
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  t51 = ((t49) == (t50));
  if (t51) {
  {
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(v1));
  t54 = ((int64_t)(v2));
  t55 = ((int64_t)(v4));
  t56 = ((int64_t)(((int64_t)UINT64_C(2))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = fg14_s_launch_mm_scale_bias(t52, t53, t54, t55, t56, t57);
  t0 = ((int64_t)(t58));
  goto gg_done;
  }
  }
  }
  }
  t59 = ((int64_t)(v1));
  t60 = ((int64_t)(v2));
  t61 = ((int64_t)(((int64_t)UINT64_C(29))));
  t62 = ((int64_t)(((int64_t)UINT64_C(7))));
  t63 = ((int64_t)(((int64_t)UINT64_C(2))));
  t64 = gg_function_84(t61, t62, t63, base + 0);
  t65 = fg14_s_vec_data(t64);
  t66 = ((int64_t)(t65));
  t67 = ((int64_t)(((int64_t)UINT64_C(3))));
  t68 = fg14_s_g_can_fuse(t59, t60, t66, t67);
  t69 = ((int64_t)(t68));
  t70 = ((int64_t)(((int64_t)UINT64_C(1))));
  t71 = ((t69) == (t70));
  if (t71) {
  {
  t72 = ((int64_t)(v1));
  t73 = ((int64_t)(v2));
  t74 = ((int64_t)(((int64_t)UINT64_C(1))));
  t75 = ((int64_t)(((int64_t)UINT64_C(29))));
  t76 = gg_function_94(t72, t73, t74, t75, base + 0);
  t77 = ((int64_t)(t76));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  t79 = ((t77) == (t78));
  if (t79) {
  {
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(v1));
  t82 = ((int64_t)(v2));
  t83 = ((int64_t)(v4));
  t84 = ((int64_t)(((int64_t)UINT64_C(3))));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = fg14_s_launch_mm_scale_bias(t80, t81, t82, t83, t84, t85);
  t0 = ((int64_t)(t86));
  goto gg_done;
  }
  }
  }
  }
  }
  } else {
  {
  t87 = ((int64_t)(v1));
  t88 = ((int64_t)(v2));
  t89 = ((int64_t)(((int64_t)UINT64_C(30))));
  t90 = ((int64_t)(((int64_t)UINT64_C(36))));
  t91 = ((int64_t)(((int64_t)UINT64_C(38))));
  t92 = ((int64_t)(((int64_t)UINT64_C(40))));
  t93 = ((int64_t)(((int64_t)UINT64_C(7))));
  t94 = gg_function_87(t89, t90, t91, t92, t93, base + 0);
  t95 = fg14_s_vec_data(t94);
  t96 = ((int64_t)(t95));
  t97 = ((int64_t)(((int64_t)UINT64_C(5))));
  t98 = fg14_s_g_can_fuse(t87, t88, t96, t97);
  t99 = ((int64_t)(t98));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  if (t101) {
  {
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(v1));
  t104 = ((int64_t)(v2));
  t105 = ((int64_t)(v4));
  t106 = ((int64_t)(((int64_t)UINT64_C(5))));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = fg14_s_launch_mm_scale_bias(t102, t103, t104, t105, t106, t107);
  t0 = ((int64_t)(t108));
  goto gg_done;
  }
  }
  t109 = ((int64_t)(v1));
  t110 = ((int64_t)(v2));
  t111 = ((int64_t)(((int64_t)UINT64_C(30))));
  t112 = ((int64_t)(((int64_t)UINT64_C(36))));
  t113 = ((int64_t)(((int64_t)UINT64_C(38))));
  t114 = ((int64_t)(((int64_t)UINT64_C(40))));
  t115 = ((int64_t)(((int64_t)UINT64_C(7))));
  t116 = gg_function_87(t111, t112, t113, t114, t115, base + 0);
  t117 = fg14_s_vec_data(t116);
  t118 = ((int64_t)(t117));
  t119 = ((int64_t)(((int64_t)UINT64_C(6))));
  t120 = fg14_s_g_can_fuse(t109, t110, t118, t119);
  t121 = ((int64_t)(t120));
  t122 = ((int64_t)(((int64_t)UINT64_C(1))));
  t123 = ((t121) == (t122));
  if (t123) {
  {
  t124 = ((int64_t)(v0));
  t125 = ((int64_t)(v1));
  t126 = ((int64_t)(v2));
  t127 = ((int64_t)(v4));
  t128 = ((int64_t)(((int64_t)UINT64_C(6))));
  t129 = ((int64_t)(((int64_t)UINT64_C(0))));
  t130 = fg14_s_launch_mm_scale_bias(t124, t125, t126, t127, t128, t129);
  t0 = ((int64_t)(t130));
  goto gg_done;
  }
  }
  }
  }
  t131 = ((int64_t)(v5));
  t132 = ((int64_t)(v6));
  v7 = gg_function_93(t131, t132, base + 0);
  t133 = ((int64_t)(v1));
  t134 = ((int64_t)(v2));
  t135 = ((int64_t)(v7));
  t136 = fg14_s_vec_data(t135);
  t137 = ((int64_t)(t136));
  t138 = ((int64_t)(((int64_t)UINT64_C(2))));
  t139 = fg14_s_g_can_fuse(t133, t134, t137, t138);
  t140 = ((int64_t)(t139));
  t141 = ((int64_t)(((int64_t)UINT64_C(1))));
  t142 = ((t140) == (t141));
  if (t142) {
  {
  t143 = ((int64_t)(v3));
  t144 = gg_function_98(t143, base + 0);
  t145 = ((int64_t)(t144));
  t146 = ((int64_t)(((int64_t)UINT64_C(1))));
  t147 = ((t145) == (t146));
  if (t147) {
  {
  t148 = ((int64_t)(v0));
  t149 = ((int64_t)(v1));
  t150 = ((int64_t)(v2));
  t151 = ((int64_t)(v4));
  t152 = ((int64_t)(((int64_t)UINT64_C(1))));
  t153 = fg14_s_launch_mm_bias(t148, t149, t150, t151, t152);
  t0 = ((int64_t)(t153));
  goto gg_done;
  }
  }
  t154 = ((int64_t)(v3));
  t155 = gg_function_49(t154, base + 0);
  t156 = ((int64_t)(t155));
  t157 = ((int64_t)(((int64_t)UINT64_C(1))));
  t158 = ((t156) == (t157));
  if (t158) {
  {
  t159 = ((int64_t)(v0));
  t160 = ((int64_t)(v1));
  t161 = ((int64_t)(v2));
  t162 = ((int64_t)(v4));
  t163 = ((int64_t)(((int64_t)UINT64_C(2))));
  t164 = fg14_s_launch_mm_bias(t159, t160, t161, t162, t163);
  t0 = ((int64_t)(t164));
  goto gg_done;
  }
  }
  }
  }
  t165 = ((int64_t)(v7));
  t166 = fg14_s_vec_free(t165);
  (void)(t166);
  t167 = ((int64_t)(v5));
  t168 = ((int64_t)(((int64_t)UINT64_C(29))));
  t169 = ((t167) == (t168));
  if (t169) {
  {
  t170 = ((int64_t)(((int64_t)UINT64_C(29))));
  t171 = ((int64_t)(((int64_t)UINT64_C(29))));
  t172 = ((int64_t)(((int64_t)UINT64_C(100))));
  t173 = ((int64_t)(((int64_t)UINT64_C(0))));
  t174 = ((int64_t)(((int64_t)UINT64_C(0))));
  v8 = gg_function_87(t170, t171, t172, t173, t174, base + 0);
  t175 = ((int64_t)(v8));
  t176 = fg14_s_vec_free(t175);
  (void)(t176);
  }
  }
  }
  }
  t177 = v4;
  t178 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t177) + (uint64_t)(t178)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_89(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
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
  uint8_t t44;
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
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = gg_function_9(t1, t2, base + 0);
  v3 = gg_function_12(t3, base + 0);
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)UINT64_C(6))));
  if (!(((t4) <= (t5)))) goto label2;
  {
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v4));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  v5 = gg_function_9(t6, t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  v6 = gg_function_9(t10, t15, base + 0);
  t16 = ((int64_t)(v5));
  t17 = gg_function_12(t16, base + 0);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(v3));
  t20 = ((t18) != (t19));
  t21 = t20;
  if (!t21) {
  t22 = ((int64_t)(v6));
  t23 = gg_function_12(t22, base + 0);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(v3));
  t21 = ((t24) != (t25));
  }
  t26 = t21;
  t27 = t26;
  if (!t27) {
  t28 = ((int64_t)(v5));
  t29 = ((int64_t)(v6));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = gg_function_14(t29, t30, base + 0);
  t32 = ((int64_t)(t31));
  t27 = ((t28) != (t32));
  }
  t33 = t27;
  t34 = t33;
  if (!t34) {
  t35 = ((int64_t)(v5));
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  t37 = gg_function_14(t35, t36, base + 0);
  t38 = ((int64_t)(v6));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = gg_function_14(t38, t39, base + 0);
  t41 = fg14_s_t_same_layout(t37, t40);
  t42 = ((int64_t)(t41));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((t42) == (t43));
  }
  t44 = t34;
  if (t44) {
  {
  t45 = ((int64_t)(v2));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  goto gg_done;
  }
  }
  t47 = v4;
  t48 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t47) + (uint64_t)(t48)));
  t49 = v2;
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t49) + (uint64_t)(t50)));
  }
  goto label1;
label2:;
  t51 = ((int64_t)(v2));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t51) + (uint64_t)(t52)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_86(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t51;
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
  uint8_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint8_t t79;
  uint8_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  uint8_t t87;
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
  int64_t t98;
  int64_t t99;
  uint8_t t100;
  uint8_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
  uint8_t t108;
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
  uint8_t t120;
  uint8_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  uint8_t t127;
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
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
  uint8_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  uint8_t t152;
  uint8_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  uint8_t t158;
  uint8_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  uint8_t t164;
  uint8_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  uint8_t t170;
  uint8_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  uint8_t t178;
  uint8_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  uint8_t t186;
  uint8_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
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
  uint8_t t203;
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
  uint8_t t216;
  uint8_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  uint8_t t223;
  uint8_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  uint8_t t230;
  uint8_t t231;
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
  uint8_t t242;
  uint8_t t243;
  int64_t t244;
  int64_t t245;
  uint8_t t246;
  uint8_t t247;
  int64_t t248;
  int64_t t249;
  uint8_t t250;
  uint8_t t251;
  int64_t t252;
  int64_t t253;
  uint8_t t254;
  uint8_t t255;
  int64_t t256;
  int64_t t257;
  uint8_t t258;
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
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(7))));
  t2 = ((int64_t)(((int64_t)UINT64_C(12))));
  t3 = ((int64_t)(((int64_t)UINT64_C(9))));
  t4 = ((int64_t)(((int64_t)UINT64_C(7))));
  t5 = ((int64_t)(((int64_t)UINT64_C(2))));
  v2 = gg_function_87(t1, t2, t3, t4, t5, base + 0);
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v2));
  t9 = fg14_s_vec_data(t8);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(5))));
  t12 = fg14_s_g_can_fuse_sub1(t6, t7, t10, t11);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v2));
  t17 = fg14_s_vec_free(t16);
  (void)(t17);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(v1));
  v3 = gg_function_9(t18, t19, base + 0);
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(2))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  v4 = gg_function_9(t20, t23, base + 0);
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(3))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  v5 = gg_function_9(t24, t27, base + 0);
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(4))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  v6 = gg_function_9(t28, t31, base + 0);
  t32 = ((int64_t)(v3));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = gg_function_14(t32, t33, base + 0);
  t34 = ((int64_t)(v3));
  t35 = ((int64_t)(v3));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = gg_function_14(t35, t36, base + 0);
  t38 = fg14_s_t_same_shape(t34, t37);
  t39 = ((int64_t)(t38));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  t42 = ((int64_t)(v3));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = gg_function_14(t42, t43, base + 0);
  }
  }
  t44 = ((int64_t)(v3));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  v8 = gg_function_14(t44, t45, base + 0);
  t46 = ((int64_t)(v7));
  t47 = ((int64_t)(v3));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = gg_function_14(t47, t48, base + 0);
  t50 = ((int64_t)(t49));
  t51 = ((t46) == (t50));
  if (t51) {
  {
  t52 = ((int64_t)(v3));
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = gg_function_14(t52, t53, base + 0);
  }
  }
  t54 = ((int64_t)(v5));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  v9 = gg_function_14(t54, t55, base + 0);
  t56 = ((int64_t)(v5));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = gg_function_14(t56, t57, base + 0);
  t59 = ((int64_t)(t58));
  t60 = ((int64_t)(v4));
  t61 = ((t59) == (t60));
  if (t61) {
  {
  t62 = ((int64_t)(v5));
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = gg_function_14(t62, t63, base + 0);
  }
  }
  t64 = ((int64_t)(v6));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  v10 = gg_function_14(t64, t65, base + 0);
  t66 = ((int64_t)(v6));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = gg_function_14(t66, t67, base + 0);
  t69 = ((int64_t)(t68));
  t70 = ((int64_t)(v5));
  t71 = ((t69) == (t70));
  if (t71) {
  {
  t72 = ((int64_t)(v6));
  t73 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = gg_function_14(t72, t73, base + 0);
  }
  }
  t74 = ((int64_t)(v7));
  t75 = ((int64_t)(((int64_t)UINT64_C(2))));
  t76 = gg_function_29(t74, t75, base + 0);
  t77 = ((int64_t)(t76));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  t79 = ((t77) == (t78));
  t80 = t79;
  if (t80) {
  t81 = ((int64_t)(v7));
  t82 = ((int64_t)(((int64_t)UINT64_C(3))));
  t83 = gg_function_29(t81, t82, base + 0);
  t84 = ((int64_t)(t83));
  t85 = ((int64_t)(((int64_t)UINT64_C(1))));
  t80 = ((t84) == (t85));
  }
  t86 = t80;
  t87 = t86;
  if (t87) {
  t88 = ((int64_t)(v6));
  t89 = ((int64_t)(((int64_t)UINT64_C(2))));
  t90 = gg_function_29(t88, t89, base + 0);
  t91 = ((int64_t)(t90));
  t92 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((t91) == (t92));
  }
  t93 = t87;
  t94 = t93;
  if (t94) {
  t95 = ((int64_t)(v6));
  t96 = ((int64_t)(((int64_t)UINT64_C(3))));
  t97 = gg_function_29(t95, t96, base + 0);
  t98 = ((int64_t)(t97));
  t99 = ((int64_t)(((int64_t)UINT64_C(1))));
  t94 = ((t98) == (t99));
  }
  t100 = t94;
  t101 = t100;
  if (t101) {
  t102 = ((int64_t)(v8));
  t103 = ((int64_t)(((int64_t)UINT64_C(2))));
  t104 = gg_function_29(t102, t103, base + 0);
  t105 = ((int64_t)(t104));
  t106 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t105) == (t106));
  }
  t107 = t101;
  t108 = t107;
  if (t108) {
  t109 = ((int64_t)(v8));
  t110 = ((int64_t)(((int64_t)UINT64_C(3))));
  t111 = gg_function_29(t109, t110, base + 0);
  t112 = ((int64_t)(t111));
  t113 = ((int64_t)(((int64_t)UINT64_C(1))));
  t108 = ((t112) == (t113));
  }
  t114 = t108;
  if (t114) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t115 = ((int64_t)(v8));
  t116 = ((int64_t)(v9));
  t117 = fg14_s_t_same_shape(t115, t116);
  t118 = ((int64_t)(t117));
  t119 = ((int64_t)(((int64_t)UINT64_C(1))));
  t120 = ((t118) == (t119));
  t121 = t120;
  if (t121) {
  t122 = ((int64_t)(v8));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = gg_function_29(t122, t123, base + 0);
  t125 = ((int64_t)(t124));
  t126 = ((int64_t)(((int64_t)UINT64_C(1))));
  t121 = ((t125) == (t126));
  }
  t127 = t121;
  t128 = t127;
  if (t128) {
  t129 = ((int64_t)(v8));
  t130 = ((int64_t)(((int64_t)UINT64_C(1))));
  t131 = gg_function_29(t129, t130, base + 0);
  t132 = ((int64_t)(t131));
  t133 = ((int64_t)(v7));
  t134 = ((int64_t)(((int64_t)UINT64_C(1))));
  t135 = gg_function_29(t133, t134, base + 0);
  t136 = ((int64_t)(t135));
  t128 = ((t132) == (t136));
  }
  t137 = t128;
  if (t137) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t138 = ((int64_t)(v0));
  t139 = ((int64_t)(v1));
  t140 = ((int64_t)(((int64_t)UINT64_C(1))));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t139) + (uint64_t)(t140)))));
  v13 = gg_function_9(t138, t141, base + 0);
  t142 = ((int64_t)(v7));
  t143 = gg_function_16(t142, base + 0);
  t144 = ((int64_t)(t143));
  t145 = ((int64_t)(((int64_t)UINT64_C(0))));
  t146 = ((t144) == (t145));
  t147 = t146;
  if (!t147) {
  t148 = ((int64_t)(v7));
  t149 = gg_function_16(t148, base + 0);
  t150 = ((int64_t)(t149));
  t151 = ((int64_t)(((int64_t)UINT64_C(1))));
  t147 = ((t150) == (t151));
  }
  t152 = t147;
  t153 = t152;
  if (!t153) {
  t154 = ((int64_t)(v7));
  t155 = gg_function_16(t154, base + 0);
  t156 = ((int64_t)(t155));
  t157 = ((int64_t)(((int64_t)UINT64_C(30))));
  t153 = ((t156) == (t157));
  }
  t158 = t153;
  t159 = t158;
  if (t159) {
  t160 = ((int64_t)(v8));
  t161 = gg_function_16(t160, base + 0);
  t162 = ((int64_t)(t161));
  t163 = ((int64_t)(((int64_t)UINT64_C(0))));
  t159 = ((t162) == (t163));
  }
  t164 = t159;
  t165 = t164;
  if (t165) {
  t166 = ((int64_t)(v9));
  t167 = gg_function_16(t166, base + 0);
  t168 = ((int64_t)(t167));
  t169 = ((int64_t)(((int64_t)UINT64_C(0))));
  t165 = ((t168) == (t169));
  }
  t170 = t165;
  t171 = t170;
  if (t171) {
  t172 = ((int64_t)(v3));
  t173 = gg_function_16(t172, base + 0);
  t174 = ((int64_t)(t173));
  t175 = ((int64_t)(v7));
  t176 = gg_function_16(t175, base + 0);
  t177 = ((int64_t)(t176));
  t171 = ((t174) == (t177));
  }
  t178 = t171;
  t179 = t178;
  if (t179) {
  t180 = ((int64_t)(v13));
  t181 = gg_function_16(t180, base + 0);
  t182 = ((int64_t)(t181));
  t183 = ((int64_t)(v7));
  t184 = gg_function_16(t183, base + 0);
  t185 = ((int64_t)(t184));
  t179 = ((t182) == (t185));
  }
  t186 = t179;
  t187 = t186;
  if (t187) {
  t188 = ((int64_t)(v4));
  t189 = gg_function_16(t188, base + 0);
  t190 = ((int64_t)(t189));
  t191 = ((int64_t)(v7));
  t192 = gg_function_16(t191, base + 0);
  t193 = ((int64_t)(t192));
  t187 = ((t190) == (t193));
  }
  t194 = t187;
  t195 = t194;
  if (t195) {
  t196 = ((int64_t)(v5));
  t197 = gg_function_16(t196, base + 0);
  t198 = ((int64_t)(t197));
  t199 = ((int64_t)(v7));
  t200 = gg_function_16(t199, base + 0);
  t201 = ((int64_t)(t200));
  t195 = ((t198) == (t201));
  }
  t202 = t195;
  t203 = t202;
  if (t203) {
  t204 = ((int64_t)(v6));
  t205 = gg_function_16(t204, base + 0);
  t206 = ((int64_t)(t205));
  t207 = ((int64_t)(v7));
  t208 = gg_function_16(t207, base + 0);
  t209 = ((int64_t)(t208));
  t203 = ((t206) == (t209));
  }
  t210 = t203;
  if (t210) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t211 = ((int64_t)(v7));
  t212 = ((int64_t)(((int64_t)UINT64_C(12))));
  t213 = fg14_s_contig(t211, t212);
  t214 = ((int64_t)(t213));
  t215 = ((int64_t)(((int64_t)UINT64_C(1))));
  t216 = ((t214) == (t215));
  t217 = t216;
  if (t217) {
  t218 = ((int64_t)(v6));
  t219 = ((int64_t)(((int64_t)UINT64_C(12))));
  t220 = fg14_s_contig(t218, t219);
  t221 = ((int64_t)(t220));
  t222 = ((int64_t)(((int64_t)UINT64_C(1))));
  t217 = ((t221) == (t222));
  }
  t223 = t217;
  t224 = t223;
  if (t224) {
  t225 = ((int64_t)(v8));
  t226 = ((int64_t)(((int64_t)UINT64_C(12))));
  t227 = fg14_s_contig(t225, t226);
  t228 = ((int64_t)(t227));
  t229 = ((int64_t)(((int64_t)UINT64_C(1))));
  t224 = ((t228) == (t229));
  }
  t230 = t224;
  t231 = t230;
  if (t231) {
  t232 = ((int64_t)(v9));
  t233 = ((int64_t)(((int64_t)UINT64_C(12))));
  t234 = fg14_s_contig(t232, t233);
  t235 = ((int64_t)(t234));
  t236 = ((int64_t)(((int64_t)UINT64_C(1))));
  t231 = ((t235) == (t236));
  }
  t237 = t231;
  if (t237) {
  {
  v15 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t238 = ((int64_t)(v2));
  t239 = fg14_s_vec_free(t238);
  (void)(t239);
  t240 = ((int64_t)(v14));
  t241 = ((int64_t)(((int64_t)UINT64_C(1))));
  t242 = ((t240) == (t241));
  t243 = t242;
  if (t243) {
  t244 = ((int64_t)(v12));
  t245 = ((int64_t)(((int64_t)UINT64_C(1))));
  t243 = ((t244) == (t245));
  }
  t246 = t243;
  t247 = t246;
  if (t247) {
  t248 = ((int64_t)(v11));
  t249 = ((int64_t)(((int64_t)UINT64_C(1))));
  t247 = ((t248) == (t249));
  }
  t250 = t247;
  t251 = t250;
  if (t251) {
  t252 = ((int64_t)(v15));
  t253 = ((int64_t)(((int64_t)UINT64_C(1))));
  t251 = ((t252) == (t253));
  }
  t254 = t251;
  t255 = t254;
  if (t255) {
  t256 = ((int64_t)(v10));
  t257 = ((int64_t)(v7));
  t255 = ((t256) == (t257));
  }
  t258 = t255;
  if (t258) {
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
static int64_t gg_function_23(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = fg14_s_t_data(t1);
  t0 = ((int64_t)(t2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
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
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
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
  v2 = gg_function_23(t1, base + 0);
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(v0));
  t4 = fg14_s_t_alloc_size(t3);
  t5 = ((int64_t)(t4));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t5)))));
  t6 = ((int64_t)(v1));
  v4 = gg_function_23(t6, base + 0);
  t7 = ((int64_t)(v4));
  t8 = ((int64_t)(v1));
  t9 = fg14_s_t_alloc_size(t8);
  t10 = ((int64_t)(t9));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t10)))));
  t11 = ((int64_t)(v4));
  t12 = ((int64_t)(v2));
  t13 = ((t11) <= (t12));
  t14 = t13;
  if (t14) {
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(v5));
  t14 = ((t15) < (t16));
  }
  t17 = t14;
  t18 = t17;
  if (!t18) {
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(v4));
  t21 = ((t19) <= (t20));
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(v4));
  t24 = ((int64_t)(v3));
  t22 = ((t23) < (t24));
  }
  t18 = t22;
  }
  t25 = t18;
  if (t25) {
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
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  uint8_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
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
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
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
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(v4));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(v1));
  t7 = gg_function_9(t5, t6, base + 0);
  t8 = fg14_s_t_nrows(t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(32))));
  t4 = ((t9) <= (t10));
  }
  t11 = t4;
  if (t11) {
  {
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(v1));
  t14 = gg_function_9(t12, t13, base + 0);
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = gg_function_14(t14, t15, base + 0);
  }
  }
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(v3));
  v7 = gg_function_9(t16, t17, base + 0);
  v8 = ((int64_t)(v1));
label1:;
  t18 = ((int64_t)(v8));
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  if (!(((t18) < (t21)))) goto label2;
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t22 = ((int64_t)(v9));
  t23 = ((int64_t)(((int64_t)UINT64_C(10))));
  if (!(((t22) < (t23)))) goto label5;
  {
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(v8));
  t26 = gg_function_9(t24, t25, base + 0);
  t27 = ((int64_t)(v9));
  v10 = gg_function_14(t26, t27, base + 0);
  t28 = ((int64_t)(v10));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) != (t29));
  t31 = t30;
  if (t31) {
  t32 = ((int64_t)(v10));
  t33 = gg_function_12(t32, base + 0);
  t34 = ((int64_t)(t33));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t34) != (t35));
  }
  t36 = t31;
  t37 = t36;
  if (t37) {
  t38 = ((int64_t)(v10));
  t39 = ((int64_t)(v5));
  t37 = ((t38) != (t39));
  }
  t40 = t37;
  if (t40) {
  {
  t41 = ((int64_t)(v7));
  t42 = ((int64_t)(v10));
  t43 = gg_function_22(t41, t42, base + 0);
  t44 = ((int64_t)(t43));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  t46 = ((t44) == (t45));
  if (t46) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  v12 = ((int64_t)(v1));
label7:;
  t47 = ((int64_t)(v12));
  t48 = ((int64_t)(v8));
  if (!(((t47) < (t48)))) goto label8;
  {
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(v12));
  t51 = gg_function_9(t49, t50, base + 0);
  t52 = ((int64_t)(t51));
  t53 = ((int64_t)(v10));
  t54 = ((t52) == (t53));
  if (t54) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t55 = v12;
  t56 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t55) + (uint64_t)(t56)));
  }
  goto label7;
label8:;
  t57 = ((int64_t)(v11));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((t57) == (t58));
  if (t59) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t60 = v9;
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t60) + (uint64_t)(t61)));
  }
  goto label4;
label5:;
  t62 = v8;
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_78(int64_t p0, int64_t p1, int64_t base) {
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
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fg14_s_vec_len(t1);
  v2 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = fg14_s_vec_resize(t3, t6);
  (void)(t7);
  t8 = ((int64_t)(v0));
  t9 = fg14_s_vec_data(t8);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(v1));
  t13 = fg14_s_frame_set(t10, t11, t12);
  (void)(t13);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_80(int64_t p0, int64_t base) {
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
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(36))));
  t3 = gg_function_78(t1, t2, base + 0);
  (void)(t3);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(36))));
  t6 = gg_function_78(t4, t5, base + 0);
  (void)(t6);
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(69))));
  t9 = gg_function_78(t7, t8, base + 0);
  (void)(t9);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(37))));
  t12 = gg_function_78(t10, t11, base + 0);
  (void)(t12);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(40))));
  t15 = gg_function_78(t13, t14, base + 0);
  (void)(t15);
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(36))));
  t18 = gg_function_78(t16, t17, base + 0);
  (void)(t18);
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(15))));
  t21 = gg_function_78(t19, t20, base + 0);
  (void)(t21);
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(36))));
  t24 = gg_function_78(t22, t23, base + 0);
  (void)(t24);
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(69))));
  t27 = gg_function_78(t25, t26, base + 0);
  (void)(t27);
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(37))));
  t30 = gg_function_78(t28, t29, base + 0);
  (void)(t30);
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(((int64_t)UINT64_C(40))));
  t33 = gg_function_78(t31, t32, base + 0);
  (void)(t33);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(73))));
  t36 = gg_function_78(t34, t35, base + 0);
  (void)(t36);
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(42))));
  t39 = gg_function_78(t37, t38, base + 0);
  (void)(t39);
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(36))));
  t42 = gg_function_78(t40, t41, base + 0);
  (void)(t42);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_77(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t13;
  int64_t t14;
  uint8_t t15;
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
  uint8_t t30;
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
  uint8_t t46;
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
  uint8_t t72;
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
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  uint8_t t141;
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
  uint8_t t155;
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
  uint8_t t176;
  uint8_t t177;
  int64_t t178;
  int64_t t179;
  uint8_t t180;
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
  uint8_t t242;
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
  {
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)(((t1) & (t2))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  if (t5) {
  {
  v4 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((int64_t)(((t6) & (t7))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) != (t9));
  if (t10) {
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)UINT64_C(2))));
  t13 = ((int64_t)(((t11) & (t12))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) != (t14));
  if (t15) {
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  t18 = ((int64_t)(((t16) & (t17))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t18) != (t19));
  if (t20) {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(((int64_t)UINT64_C(16))));
  t23 = ((int64_t)(((t21) & (t22))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) != (t24));
  if (t25) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(v3));
  t27 = ((int64_t)(((int64_t)UINT64_C(32))));
  t28 = ((int64_t)(((t26) & (t27))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) != (t29));
  if (t30) {
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(v3));
  t32 = ((int64_t)(((int64_t)UINT64_C(64))));
  t33 = ((int64_t)(((t31) & (t32))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) != (t34));
  if (t35) {
  {
  v10 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t36 = ((int64_t)(v3));
  t37 = ((int64_t)(((int64_t)UINT64_C(8))));
  t38 = ((int64_t)(((t36) >> ((uint64_t)(t37) & 63))));
  t39 = ((int64_t)(((int64_t)UINT64_C(255))));
  v11 = ((int64_t)(((t38) & (t39))));
  t40 = ((int64_t)(v3));
  t41 = ((int64_t)(((int64_t)UINT64_C(16))));
  t42 = ((int64_t)(((t40) >> ((uint64_t)(t41) & 63))));
  t43 = ((int64_t)(((int64_t)UINT64_C(255))));
  v12 = ((int64_t)(((t42) & (t43))));
  t44 = ((int64_t)(v4));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((t44) == (t45));
  if (t46) {
  {
  t47 = ((int64_t)(((int64_t)UINT64_C(8))));
  t48 = fg14_s_vec_new(t47);
  v13 = ((int64_t)(t48));
  t49 = ((int64_t)(v5));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((t49) != (t50));
  if (t51) {
  {
  t52 = ((int64_t)(v13));
  t53 = ((int64_t)(((int64_t)UINT64_C(91))));
  t54 = gg_function_78(t52, t53, base + 0);
  (void)(t54);
  }
  } else {
  {
  t55 = ((int64_t)(v6));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t55) != (t56));
  if (t57) {
  {
  t58 = ((int64_t)(v13));
  t59 = ((int64_t)(((int64_t)UINT64_C(91))));
  t60 = gg_function_78(t58, t59, base + 0);
  (void)(t60);
  t61 = ((int64_t)(v13));
  t62 = ((int64_t)(((int64_t)UINT64_C(10))));
  t63 = gg_function_78(t61, t62, base + 0);
  (void)(t63);
  }
  } else {
  {
  t64 = ((int64_t)(v13));
  t65 = ((int64_t)(((int64_t)UINT64_C(46))));
  t66 = gg_function_78(t64, t65, base + 0);
  (void)(t66);
  }
  }
  }
  }
  t67 = ((int64_t)(v13));
  t68 = ((int64_t)(((int64_t)UINT64_C(36))));
  t69 = gg_function_78(t67, t68, base + 0);
  (void)(t69);
  t70 = ((int64_t)(v8));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((t70) != (t71));
  if (t72) {
  {
  t73 = ((int64_t)(v13));
  t74 = ((int64_t)(((int64_t)UINT64_C(2))));
  t75 = gg_function_78(t73, t74, base + 0);
  (void)(t75);
  }
  }
  t76 = ((int64_t)(v11));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((t76) > (t77));
  if (t78) {
  {
  t79 = ((int64_t)(v13));
  t80 = gg_function_80(t79, base + 0);
  (void)(t80);
  }
  }
  t81 = ((int64_t)(v13));
  t82 = ((int64_t)(((int64_t)UINT64_C(69))));
  t83 = gg_function_78(t81, t82, base + 0);
  (void)(t83);
  t84 = ((int64_t)(v13));
  t85 = ((int64_t)(((int64_t)UINT64_C(37))));
  t86 = gg_function_78(t84, t85, base + 0);
  (void)(t86);
  t87 = ((int64_t)(v13));
  t88 = ((int64_t)(((int64_t)UINT64_C(40))));
  t89 = gg_function_78(t87, t88, base + 0);
  (void)(t89);
  t90 = ((int64_t)(v2));
  t91 = ((int64_t)(v13));
  t92 = fg14_s_vec_len(t91);
  t93 = ((int64_t)(t92));
  t94 = ((int64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t93)))));
  t95 = ((int64_t)(((int64_t)UINT64_C(2))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t94) - (uint64_t)(t95)))));
  t96 = ((int64_t)(v9));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((t96) != (t97));
  if (t98) {
  {
  t99 = ((int64_t)(v13));
  t100 = ((int64_t)(((int64_t)UINT64_C(36))));
  t101 = gg_function_78(t99, t100, base + 0);
  (void)(t101);
  t102 = ((int64_t)(v13));
  t103 = ((int64_t)(((int64_t)UINT64_C(15))));
  t104 = gg_function_78(t102, t103, base + 0);
  (void)(t104);
  t105 = ((int64_t)(v13));
  t106 = ((int64_t)(((int64_t)UINT64_C(50))));
  t107 = gg_function_78(t105, t106, base + 0);
  (void)(t107);
  t108 = ((int64_t)(v13));
  t109 = ((int64_t)(((int64_t)UINT64_C(8))));
  t110 = gg_function_78(t108, t109, base + 0);
  (void)(t110);
  t111 = ((int64_t)(v13));
  t112 = ((int64_t)(((int64_t)UINT64_C(36))));
  t113 = gg_function_78(t111, t112, base + 0);
  (void)(t113);
  }
  }
  t114 = ((int64_t)(v10));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((t114) != (t115));
  if (t116) {
  {
  t117 = ((int64_t)(v13));
  t118 = ((int64_t)(((int64_t)UINT64_C(32))));
  t119 = gg_function_78(t117, t118, base + 0);
  (void)(t119);
  }
  }
  t120 = ((int64_t)(v2));
  t121 = ((int64_t)(v13));
  t122 = fg14_s_vec_len(t121);
  t123 = ((int64_t)(t122));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t120) + (uint64_t)(t123)))));
  t125 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t124) - (uint64_t)(t125)))));
  t126 = ((int64_t)(v13));
  t127 = fg14_s_vec_len(t126);
  v16 = ((int64_t)(t127));
  t128 = ((int64_t)(v1));
  t129 = ((int64_t)(v2));
  t130 = ((int64_t)(v13));
  t131 = fg14_s_vec_data(t130);
  t132 = ((int64_t)(t131));
  t133 = ((int64_t)(v16));
  t134 = ((int64_t)(v14));
  t135 = ((int64_t)(v15));
  t136 = fg14_s_g_can_fuse_sub2(t128, t129, t132, t133, t134, t135);
  v17 = ((int64_t)(t136));
  t137 = ((int64_t)(v1));
  t138 = ((int64_t)(v2));
  v18 = gg_function_9(t137, t138, base + 0);
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((int64_t)(v17));
  t140 = ((int64_t)(((int64_t)UINT64_C(1))));
  t141 = ((t139) == (t140));
  if (t141) {
  {
  t142 = ((int64_t)(v18));
  t143 = ((int64_t)(v18));
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t145 = gg_function_14(t143, t144, base + 0);
  t146 = ((int64_t)(v1));
  t147 = ((int64_t)(v15));
  t148 = gg_function_9(t146, t147, base + 0);
  t149 = ((int64_t)(v1));
  t150 = ((int64_t)(v14));
  t151 = gg_function_9(t149, t150, base + 0);
  t152 = fg14_s_should_use_topk_moe(t142, t145, t148, t151);
  v19 = ((int64_t)(t152));
  }
  }
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t153 = ((int64_t)(v19));
  t154 = ((int64_t)(((int64_t)UINT64_C(1))));
  t155 = ((t153) == (t154));
  if (t155) {
  {
  t156 = ((int64_t)(v1));
  t157 = ((int64_t)(v2));
  t158 = ((int64_t)(v16));
  t159 = ((int64_t)(v15));
  t160 = ((int64_t)(((int64_t)UINT64_C(1))));
  v20 = gg_function_20(t156, t157, t158, t159, t160, base + 0);
  }
  }
  t161 = ((int64_t)(v13));
  t162 = fg14_s_vec_free(t161);
  (void)(t162);
  t163 = ((int64_t)(v20));
  t164 = ((int64_t)(((int64_t)UINT64_C(1))));
  t165 = ((t163) == (t164));
  if (t165) {
  {
  t166 = ((int64_t)(v0));
  t167 = ((int64_t)(v1));
  t168 = ((int64_t)(v2));
  t169 = ((int64_t)(v3));
  t170 = ((int64_t)(v16));
  t171 = ((int64_t)(v14));
  t172 = ((int64_t)(v15));
  t173 = fg14_s_launch_topk_moe(t166, t167, t168, t169, t170, t171, t172);
  t0 = ((int64_t)(t173));
  goto gg_done;
  }
  }
  }
  } else {
  {
  t174 = ((int64_t)(v9));
  t175 = ((int64_t)(((int64_t)UINT64_C(0))));
  t176 = ((t174) != (t175));
  t177 = t176;
  if (!t177) {
  t178 = ((int64_t)(v8));
  t179 = ((int64_t)(((int64_t)UINT64_C(0))));
  t177 = ((t178) != (t179));
  }
  t180 = t177;
  if (t180) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t181 = ((int64_t)(((int64_t)UINT64_C(8))));
  t182 = fg14_s_vec_new(t181);
  v21 = ((int64_t)(t182));
  t183 = ((int64_t)(v21));
  t184 = ((int64_t)(((int64_t)UINT64_C(69))));
  t185 = gg_function_78(t183, t184, base + 0);
  (void)(t185);
  t186 = ((int64_t)(v21));
  t187 = ((int64_t)(((int64_t)UINT64_C(37))));
  t188 = gg_function_78(t186, t187, base + 0);
  (void)(t188);
  t189 = ((int64_t)(v21));
  t190 = ((int64_t)(((int64_t)UINT64_C(40))));
  t191 = gg_function_78(t189, t190, base + 0);
  (void)(t191);
  t192 = ((int64_t)(v21));
  t193 = ((int64_t)(((int64_t)UINT64_C(36))));
  t194 = gg_function_78(t192, t193, base + 0);
  (void)(t194);
  t195 = ((int64_t)(v21));
  t196 = ((int64_t)(((int64_t)UINT64_C(46))));
  t197 = gg_function_78(t195, t196, base + 0);
  (void)(t197);
  t198 = ((int64_t)(v21));
  t199 = ((int64_t)(((int64_t)UINT64_C(36))));
  t200 = gg_function_78(t198, t199, base + 0);
  (void)(t200);
  t201 = ((int64_t)(v21));
  t202 = fg14_s_vec_len(t201);
  v22 = ((int64_t)(t202));
  t203 = ((int64_t)(v2));
  t204 = ((int64_t)(((int64_t)UINT64_C(1))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t203) + (uint64_t)(t204)))));
  t205 = ((int64_t)(v2));
  t206 = ((int64_t)(((int64_t)UINT64_C(5))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t205) + (uint64_t)(t206)))));
  t207 = ((int64_t)(v1));
  t208 = ((int64_t)(v2));
  t209 = ((int64_t)(v21));
  t210 = fg14_s_vec_data(t209);
  t211 = ((int64_t)(t210));
  t212 = ((int64_t)(v22));
  t213 = ((int64_t)(v23));
  t214 = ((int64_t)(v24));
  t215 = fg14_s_g_can_fuse_sub2(t207, t208, t211, t212, t213, t214);
  v25 = ((int64_t)(t215));
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t216 = ((int64_t)(v25));
  t217 = ((int64_t)(((int64_t)UINT64_C(1))));
  t218 = ((t216) == (t217));
  if (t218) {
  {
  t219 = ((int64_t)(v1));
  t220 = ((int64_t)(v2));
  t221 = ((int64_t)(((int64_t)UINT64_C(4))));
  t222 = ((int64_t)(((int64_t)((uint64_t)(t220) + (uint64_t)(t221)))));
  t223 = gg_function_9(t219, t222, base + 0);
  t224 = ((int64_t)(v1));
  t225 = ((int64_t)(v2));
  t226 = gg_function_9(t224, t225, base + 0);
  t227 = ((int64_t)(((int64_t)UINT64_C(0))));
  t228 = gg_function_14(t226, t227, base + 0);
  t229 = ((int64_t)(v1));
  t230 = ((int64_t)(v2));
  t231 = ((int64_t)(((int64_t)UINT64_C(5))));
  t232 = ((int64_t)(((int64_t)((uint64_t)(t230) + (uint64_t)(t231)))));
  t233 = gg_function_9(t229, t232, base + 0);
  t234 = ((int64_t)(v1));
  t235 = ((int64_t)(v2));
  t236 = ((int64_t)(((int64_t)UINT64_C(1))));
  t237 = ((int64_t)(((int64_t)((uint64_t)(t235) + (uint64_t)(t236)))));
  t238 = gg_function_9(t234, t237, base + 0);
  t239 = fg14_s_should_use_topk_moe(t223, t228, t233, t238);
  v26 = ((int64_t)(t239));
  }
  }
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t240 = ((int64_t)(v26));
  t241 = ((int64_t)(((int64_t)UINT64_C(1))));
  t242 = ((t240) == (t241));
  if (t242) {
  {
  t243 = ((int64_t)(v1));
  t244 = ((int64_t)(v2));
  t245 = ((int64_t)(v22));
  t246 = ((int64_t)(v24));
  t247 = ((int64_t)(((int64_t)UINT64_C(1))));
  v27 = gg_function_20(t243, t244, t245, t246, t247, base + 0);
  }
  }
  t248 = ((int64_t)(v21));
  t249 = fg14_s_vec_free(t248);
  (void)(t249);
  t250 = ((int64_t)(v27));
  t251 = ((int64_t)(((int64_t)UINT64_C(1))));
  t252 = ((t250) == (t251));
  if (t252) {
  {
  t253 = ((int64_t)(v0));
  t254 = ((int64_t)(v1));
  t255 = ((int64_t)(v2));
  t256 = ((int64_t)(v3));
  t257 = ((int64_t)(v22));
  t258 = ((int64_t)(v23));
  t259 = ((int64_t)(v24));
  t260 = fg14_s_launch_topk_moe(t253, t254, t255, t256, t257, t258, t259);
  t0 = ((int64_t)(t260));
  goto gg_done;
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
static int64_t gg_function_71(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t288;
  uint8_t t289;
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
  uint8_t t303;
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
  uint8_t t329;
  int64_t t330;
  int64_t t331;
  uint8_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  uint8_t t339;
  int64_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  uint8_t t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  uint8_t t359;
  int64_t t360;
  int64_t t361;
  uint8_t t362;
  int64_t t363;
  int64_t t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  uint8_t t369;
  int64_t t370;
  int64_t t371;
  int64_t t372;
  int64_t t373;
  int64_t t374;
  int64_t t375;
  int64_t t376;
  int64_t t377;
  int64_t t378;
  uint8_t t379;
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
  uint8_t t407;
  uint8_t t408;
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
  uint8_t t423;
  uint8_t t424;
  int64_t t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  int64_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  uint8_t t434;
  uint8_t t435;
  int64_t t436;
  int64_t t437;
  uint8_t t438;
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
  t2 = fg14_s_g_n(t1);
  v3 = ((int64_t)(t2));
  t3 = ((int64_t)(((int64_t)UINT64_C(8))));
  t4 = fg14_s_vec_new(t3);
  v4 = ((int64_t)(t4));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(14))));
  t7 = fg14_s_vec_resize(t5, t6);
  (void)(t7);
  t8 = ((int64_t)(((int64_t)UINT64_C(8))));
  t9 = fg14_s_vec_new(t8);
  v5 = ((int64_t)(t9));
  t10 = ((int64_t)(v5));
  t11 = ((int64_t)(((int64_t)UINT64_C(14))));
  t12 = fg14_s_vec_resize(t10, t11);
  (void)(t12);
  t13 = ((int64_t)(((int64_t)UINT64_C(8))));
  t14 = fg14_s_vec_new(t13);
  v6 = ((int64_t)(t14));
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(((int64_t)UINT64_C(28))));
  t17 = fg14_s_vec_resize(t15, t16);
  (void)(t17);
  t18 = ((int64_t)(v5));
  t19 = fg14_s_vec_data(t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(36))));
  t23 = fg14_s_frame_set(t20, t21, t22);
  (void)(t23);
  t24 = ((int64_t)(v5));
  t25 = fg14_s_vec_data(t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = ((int64_t)(((int64_t)UINT64_C(36))));
  t29 = fg14_s_frame_set(t26, t27, t28);
  (void)(t29);
  t30 = ((int64_t)(v5));
  t31 = fg14_s_vec_data(t30);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(((int64_t)UINT64_C(2))));
  t34 = ((int64_t)(((int64_t)UINT64_C(69))));
  t35 = fg14_s_frame_set(t32, t33, t34);
  (void)(t35);
  t36 = ((int64_t)(v5));
  t37 = fg14_s_vec_data(t36);
  t38 = ((int64_t)(t37));
  t39 = ((int64_t)(((int64_t)UINT64_C(3))));
  t40 = ((int64_t)(((int64_t)UINT64_C(37))));
  t41 = fg14_s_frame_set(t38, t39, t40);
  (void)(t41);
  t42 = ((int64_t)(v5));
  t43 = fg14_s_vec_data(t42);
  t44 = ((int64_t)(t43));
  t45 = ((int64_t)(((int64_t)UINT64_C(4))));
  t46 = ((int64_t)(((int64_t)UINT64_C(40))));
  t47 = fg14_s_frame_set(t44, t45, t46);
  (void)(t47);
  t48 = ((int64_t)(v5));
  t49 = fg14_s_vec_data(t48);
  t50 = ((int64_t)(t49));
  t51 = ((int64_t)(((int64_t)UINT64_C(5))));
  t52 = ((int64_t)(((int64_t)UINT64_C(36))));
  t53 = fg14_s_frame_set(t50, t51, t52);
  (void)(t53);
  t54 = ((int64_t)(v5));
  t55 = fg14_s_vec_data(t54);
  t56 = ((int64_t)(t55));
  t57 = ((int64_t)(((int64_t)UINT64_C(6))));
  t58 = ((int64_t)(((int64_t)UINT64_C(15))));
  t59 = fg14_s_frame_set(t56, t57, t58);
  (void)(t59);
  t60 = ((int64_t)(v5));
  t61 = fg14_s_vec_data(t60);
  t62 = ((int64_t)(t61));
  t63 = ((int64_t)(((int64_t)UINT64_C(7))));
  t64 = ((int64_t)(((int64_t)UINT64_C(36))));
  t65 = fg14_s_frame_set(t62, t63, t64);
  (void)(t65);
  t66 = ((int64_t)(v5));
  t67 = fg14_s_vec_data(t66);
  t68 = ((int64_t)(t67));
  t69 = ((int64_t)(((int64_t)UINT64_C(8))));
  t70 = ((int64_t)(((int64_t)UINT64_C(69))));
  t71 = fg14_s_frame_set(t68, t69, t70);
  (void)(t71);
  t72 = ((int64_t)(v5));
  t73 = fg14_s_vec_data(t72);
  t74 = ((int64_t)(t73));
  t75 = ((int64_t)(((int64_t)UINT64_C(9))));
  t76 = ((int64_t)(((int64_t)UINT64_C(37))));
  t77 = fg14_s_frame_set(t74, t75, t76);
  (void)(t77);
  t78 = ((int64_t)(v5));
  t79 = fg14_s_vec_data(t78);
  t80 = ((int64_t)(t79));
  t81 = ((int64_t)(((int64_t)UINT64_C(10))));
  t82 = ((int64_t)(((int64_t)UINT64_C(40))));
  t83 = fg14_s_frame_set(t80, t81, t82);
  (void)(t83);
  t84 = ((int64_t)(v5));
  t85 = fg14_s_vec_data(t84);
  t86 = ((int64_t)(t85));
  t87 = ((int64_t)(((int64_t)UINT64_C(11))));
  t88 = ((int64_t)(((int64_t)UINT64_C(73))));
  t89 = fg14_s_frame_set(t86, t87, t88);
  (void)(t89);
  t90 = ((int64_t)(v5));
  t91 = fg14_s_vec_data(t90);
  t92 = ((int64_t)(t91));
  t93 = ((int64_t)(((int64_t)UINT64_C(12))));
  t94 = ((int64_t)(((int64_t)UINT64_C(42))));
  t95 = fg14_s_frame_set(t92, t93, t94);
  (void)(t95);
  t96 = ((int64_t)(v5));
  t97 = fg14_s_vec_data(t96);
  t98 = ((int64_t)(t97));
  t99 = ((int64_t)(((int64_t)UINT64_C(13))));
  t100 = ((int64_t)(((int64_t)UINT64_C(36))));
  t101 = fg14_s_frame_set(t98, t99, t100);
  (void)(t101);
  t102 = ((int64_t)(v6));
  t103 = fg14_s_vec_data(t102);
  t104 = ((int64_t)(t103));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((int64_t)UINT64_C(1));
  t107 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t106)))));
  t108 = fg14_s_frame_set(t104, t105, t107);
  (void)(t108);
  t109 = ((int64_t)(v6));
  t110 = fg14_s_vec_data(t109);
  t111 = ((int64_t)(t110));
  t112 = ((int64_t)(((int64_t)UINT64_C(1))));
  t113 = ((int64_t)UINT64_C(2));
  t114 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t113)))));
  t115 = fg14_s_frame_set(t111, t112, t114);
  (void)(t115);
  t116 = ((int64_t)(v6));
  t117 = fg14_s_vec_data(t116);
  t118 = ((int64_t)(t117));
  t119 = ((int64_t)(((int64_t)UINT64_C(2))));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = fg14_s_frame_set(t118, t119, t120);
  (void)(t121);
  t122 = ((int64_t)(v6));
  t123 = fg14_s_vec_data(t122);
  t124 = ((int64_t)(t123));
  t125 = ((int64_t)(((int64_t)UINT64_C(3))));
  t126 = ((int64_t)UINT64_C(2));
  t127 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t126)))));
  t128 = fg14_s_frame_set(t124, t125, t127);
  (void)(t128);
  t129 = ((int64_t)(v6));
  t130 = fg14_s_vec_data(t129);
  t131 = ((int64_t)(t130));
  t132 = ((int64_t)(((int64_t)UINT64_C(4))));
  t133 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = fg14_s_frame_set(t131, t132, t133);
  (void)(t134);
  t135 = ((int64_t)(v6));
  t136 = fg14_s_vec_data(t135);
  t137 = ((int64_t)(t136));
  t138 = ((int64_t)(((int64_t)UINT64_C(5))));
  t139 = ((int64_t)UINT64_C(2));
  t140 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t139)))));
  t141 = fg14_s_frame_set(t137, t138, t140);
  (void)(t141);
  t142 = ((int64_t)(v6));
  t143 = fg14_s_vec_data(t142);
  t144 = ((int64_t)(t143));
  t145 = ((int64_t)(((int64_t)UINT64_C(6))));
  t146 = ((int64_t)(((int64_t)UINT64_C(2))));
  t147 = fg14_s_frame_set(t144, t145, t146);
  (void)(t147);
  t148 = ((int64_t)(v6));
  t149 = fg14_s_vec_data(t148);
  t150 = ((int64_t)(t149));
  t151 = ((int64_t)(((int64_t)UINT64_C(7))));
  t152 = ((int64_t)UINT64_C(2));
  t153 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t152)))));
  t154 = fg14_s_frame_set(t150, t151, t153);
  (void)(t154);
  t155 = ((int64_t)(v6));
  t156 = fg14_s_vec_data(t155);
  t157 = ((int64_t)(t156));
  t158 = ((int64_t)(((int64_t)UINT64_C(8))));
  t159 = ((int64_t)(((int64_t)UINT64_C(1))));
  t160 = fg14_s_frame_set(t157, t158, t159);
  (void)(t160);
  t161 = ((int64_t)(v6));
  t162 = fg14_s_vec_data(t161);
  t163 = ((int64_t)(t162));
  t164 = ((int64_t)(((int64_t)UINT64_C(9))));
  t165 = ((int64_t)(((int64_t)UINT64_C(3))));
  t166 = fg14_s_frame_set(t163, t164, t165);
  (void)(t166);
  t167 = ((int64_t)(v6));
  t168 = fg14_s_vec_data(t167);
  t169 = ((int64_t)(t168));
  t170 = ((int64_t)(((int64_t)UINT64_C(10))));
  t171 = ((int64_t)(((int64_t)UINT64_C(4))));
  t172 = fg14_s_frame_set(t169, t170, t171);
  (void)(t172);
  t173 = ((int64_t)(v6));
  t174 = fg14_s_vec_data(t173);
  t175 = ((int64_t)(t174));
  t176 = ((int64_t)(((int64_t)UINT64_C(11))));
  t177 = ((int64_t)UINT64_C(2));
  t178 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t177)))));
  t179 = fg14_s_frame_set(t175, t176, t178);
  (void)(t179);
  t180 = ((int64_t)(v6));
  t181 = fg14_s_vec_data(t180);
  t182 = ((int64_t)(t181));
  t183 = ((int64_t)(((int64_t)UINT64_C(12))));
  t184 = ((int64_t)(((int64_t)UINT64_C(5))));
  t185 = fg14_s_frame_set(t182, t183, t184);
  (void)(t185);
  t186 = ((int64_t)(v6));
  t187 = fg14_s_vec_data(t186);
  t188 = ((int64_t)(t187));
  t189 = ((int64_t)(((int64_t)UINT64_C(13))));
  t190 = ((int64_t)UINT64_C(2));
  t191 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t190)))));
  t192 = fg14_s_frame_set(t188, t189, t191);
  (void)(t192);
  t193 = ((int64_t)(v6));
  t194 = fg14_s_vec_data(t193);
  t195 = ((int64_t)(t194));
  t196 = ((int64_t)(((int64_t)UINT64_C(14))));
  t197 = ((int64_t)(((int64_t)UINT64_C(6))));
  t198 = fg14_s_frame_set(t195, t196, t197);
  (void)(t198);
  t199 = ((int64_t)(v6));
  t200 = fg14_s_vec_data(t199);
  t201 = ((int64_t)(t200));
  t202 = ((int64_t)(((int64_t)UINT64_C(15))));
  t203 = ((int64_t)UINT64_C(2));
  t204 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t203)))));
  t205 = fg14_s_frame_set(t201, t202, t204);
  (void)(t205);
  t206 = ((int64_t)(v6));
  t207 = fg14_s_vec_data(t206);
  t208 = ((int64_t)(t207));
  t209 = ((int64_t)(((int64_t)UINT64_C(16))));
  t210 = ((int64_t)(((int64_t)UINT64_C(7))));
  t211 = fg14_s_frame_set(t208, t209, t210);
  (void)(t211);
  t212 = ((int64_t)(v6));
  t213 = fg14_s_vec_data(t212);
  t214 = ((int64_t)(t213));
  t215 = ((int64_t)(((int64_t)UINT64_C(17))));
  t216 = ((int64_t)UINT64_C(2));
  t217 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t216)))));
  t218 = fg14_s_frame_set(t214, t215, t217);
  (void)(t218);
  t219 = ((int64_t)(v6));
  t220 = fg14_s_vec_data(t219);
  t221 = ((int64_t)(t220));
  t222 = ((int64_t)(((int64_t)UINT64_C(18))));
  t223 = ((int64_t)(((int64_t)UINT64_C(8))));
  t224 = fg14_s_frame_set(t221, t222, t223);
  (void)(t224);
  t225 = ((int64_t)(v6));
  t226 = fg14_s_vec_data(t225);
  t227 = ((int64_t)(t226));
  t228 = ((int64_t)(((int64_t)UINT64_C(19))));
  t229 = ((int64_t)UINT64_C(2));
  t230 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t229)))));
  t231 = fg14_s_frame_set(t227, t228, t230);
  (void)(t231);
  t232 = ((int64_t)(v6));
  t233 = fg14_s_vec_data(t232);
  t234 = ((int64_t)(t233));
  t235 = ((int64_t)(((int64_t)UINT64_C(20))));
  t236 = ((int64_t)(((int64_t)UINT64_C(0))));
  t237 = fg14_s_frame_set(t234, t235, t236);
  (void)(t237);
  t238 = ((int64_t)(v6));
  t239 = fg14_s_vec_data(t238);
  t240 = ((int64_t)(t239));
  t241 = ((int64_t)(((int64_t)UINT64_C(21))));
  t242 = ((int64_t)(((int64_t)UINT64_C(9))));
  t243 = fg14_s_frame_set(t240, t241, t242);
  (void)(t243);
  t244 = ((int64_t)(v6));
  t245 = fg14_s_vec_data(t244);
  t246 = ((int64_t)(t245));
  t247 = ((int64_t)(((int64_t)UINT64_C(22))));
  t248 = ((int64_t)(((int64_t)UINT64_C(0))));
  t249 = fg14_s_frame_set(t246, t247, t248);
  (void)(t249);
  t250 = ((int64_t)(v6));
  t251 = fg14_s_vec_data(t250);
  t252 = ((int64_t)(t251));
  t253 = ((int64_t)(((int64_t)UINT64_C(23))));
  t254 = ((int64_t)UINT64_C(2));
  t255 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t254)))));
  t256 = fg14_s_frame_set(t252, t253, t255);
  (void)(t256);
  t257 = ((int64_t)(v6));
  t258 = fg14_s_vec_data(t257);
  t259 = ((int64_t)(t258));
  t260 = ((int64_t)(((int64_t)UINT64_C(24))));
  t261 = ((int64_t)(((int64_t)UINT64_C(10))));
  t262 = fg14_s_frame_set(t259, t260, t261);
  (void)(t262);
  t263 = ((int64_t)(v6));
  t264 = fg14_s_vec_data(t263);
  t265 = ((int64_t)(t264));
  t266 = ((int64_t)(((int64_t)UINT64_C(25))));
  t267 = ((int64_t)(((int64_t)UINT64_C(9))));
  t268 = fg14_s_frame_set(t265, t266, t267);
  (void)(t268);
  t269 = ((int64_t)(v6));
  t270 = fg14_s_vec_data(t269);
  t271 = ((int64_t)(t270));
  t272 = ((int64_t)(((int64_t)UINT64_C(26))));
  t273 = ((int64_t)(((int64_t)UINT64_C(12))));
  t274 = fg14_s_frame_set(t271, t272, t273);
  (void)(t274);
  t275 = ((int64_t)(v6));
  t276 = fg14_s_vec_data(t275);
  t277 = ((int64_t)(t276));
  t278 = ((int64_t)(((int64_t)UINT64_C(27))));
  t279 = ((int64_t)UINT64_C(2));
  t280 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t279)))));
  t281 = fg14_s_frame_set(t277, t278, t280);
  (void)(t281);
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t282 = ((int64_t)(v7));
  t283 = ((int64_t)(((int64_t)UINT64_C(14))));
  if (!(((t282) < (t283)))) goto label2;
  {
  t284 = ((int64_t)(v1));
  t285 = ((int64_t)(v7));
  t286 = ((int64_t)(((int64_t)((uint64_t)(t284) + (uint64_t)(t285)))));
  t287 = ((int64_t)(v3));
  t288 = ((t286) >= (t287));
  t289 = t288;
  if (!t289) {
  t290 = ((int64_t)(v0));
  t291 = ((int64_t)(v1));
  t292 = ((int64_t)(v7));
  t293 = ((int64_t)(((int64_t)((uint64_t)(t291) + (uint64_t)(t292)))));
  t294 = gg_function_9(t290, t293, base + 0);
  t295 = gg_function_12(t294, base + 0);
  t296 = ((int64_t)(t295));
  t297 = ((int64_t)(v5));
  t298 = fg14_s_vec_data(t297);
  t299 = ((int64_t)(t298));
  t300 = ((int64_t)(v7));
  t301 = fg14_s_frame_get(t299, t300);
  t302 = ((int64_t)(t301));
  t289 = ((t296) != (t302));
  }
  t303 = t289;
  if (t303) {
  {
  t304 = ((int64_t)(v4));
  t305 = fg14_s_vec_free(t304);
  (void)(t305);
  t306 = ((int64_t)(v5));
  t307 = fg14_s_vec_free(t306);
  (void)(t307);
  t308 = ((int64_t)(v6));
  t309 = fg14_s_vec_free(t308);
  (void)(t309);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t310 = ((int64_t)(v6));
  t311 = fg14_s_vec_data(t310);
  t312 = ((int64_t)(t311));
  t313 = ((int64_t)(v7));
  t314 = ((int64_t)(((int64_t)UINT64_C(2))));
  t315 = ((int64_t)(((int64_t)((uint64_t)(t313) * (uint64_t)(t314)))));
  t316 = fg14_s_frame_get(t312, t315);
  v8 = ((int64_t)(t316));
  t317 = ((int64_t)(v6));
  t318 = fg14_s_vec_data(t317);
  t319 = ((int64_t)(t318));
  t320 = ((int64_t)(v7));
  t321 = ((int64_t)(((int64_t)UINT64_C(2))));
  t322 = ((int64_t)(((int64_t)((uint64_t)(t320) * (uint64_t)(t321)))));
  t323 = ((int64_t)(((int64_t)UINT64_C(1))));
  t324 = ((int64_t)(((int64_t)((uint64_t)(t322) + (uint64_t)(t323)))));
  t325 = fg14_s_frame_get(t319, t324);
  v9 = ((int64_t)(t325));
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t326 = ((int64_t)(v8));
  t327 = ((int64_t)UINT64_C(1));
  t328 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t327)))));
  t329 = ((t326) == (t328));
  if (t329) {
  {
  v10 = ((int64_t)(v2));
  }
  } else {
  {
  t330 = ((int64_t)(v8));
  t331 = ((int64_t)(((int64_t)UINT64_C(0))));
  t332 = ((t330) >= (t331));
  if (t332) {
  {
  t333 = ((int64_t)(v0));
  t334 = ((int64_t)(v1));
  t335 = ((int64_t)(v8));
  t336 = ((int64_t)(((int64_t)((uint64_t)(t334) + (uint64_t)(t335)))));
  v10 = gg_function_9(t333, t336, base + 0);
  }
  }
  }
  }
  t337 = ((int64_t)(v8));
  t338 = ((int64_t)(((int64_t)UINT64_C(0))));
  t339 = ((t337) >= (t338));
  if (t339) {
  {
  t340 = ((int64_t)(v0));
  t341 = ((int64_t)(v1));
  t342 = ((int64_t)(v7));
  t343 = ((int64_t)(((int64_t)((uint64_t)(t341) + (uint64_t)(t342)))));
  t344 = gg_function_9(t340, t343, base + 0);
  t345 = ((int64_t)(((int64_t)UINT64_C(0))));
  t346 = gg_function_14(t344, t345, base + 0);
  t347 = ((int64_t)(t346));
  t348 = ((int64_t)(v10));
  t349 = ((t347) != (t348));
  if (t349) {
  {
  t350 = ((int64_t)(v4));
  t351 = fg14_s_vec_free(t350);
  (void)(t351);
  t352 = ((int64_t)(v5));
  t353 = fg14_s_vec_free(t352);
  (void)(t353);
  t354 = ((int64_t)(v6));
  t355 = fg14_s_vec_free(t354);
  (void)(t355);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t356 = ((int64_t)(v9));
  t357 = ((int64_t)UINT64_C(1));
  t358 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t357)))));
  t359 = ((t356) == (t358));
  if (t359) {
  {
  v11 = ((int64_t)(v2));
  }
  } else {
  {
  t360 = ((int64_t)(v9));
  t361 = ((int64_t)(((int64_t)UINT64_C(0))));
  t362 = ((t360) >= (t361));
  if (t362) {
  {
  t363 = ((int64_t)(v0));
  t364 = ((int64_t)(v1));
  t365 = ((int64_t)(v9));
  t366 = ((int64_t)(((int64_t)((uint64_t)(t364) + (uint64_t)(t365)))));
  v11 = gg_function_9(t363, t366, base + 0);
  }
  }
  }
  }
  t367 = ((int64_t)(v9));
  t368 = ((int64_t)(((int64_t)UINT64_C(0))));
  t369 = ((t367) >= (t368));
  if (t369) {
  {
  t370 = ((int64_t)(v0));
  t371 = ((int64_t)(v1));
  t372 = ((int64_t)(v7));
  t373 = ((int64_t)(((int64_t)((uint64_t)(t371) + (uint64_t)(t372)))));
  t374 = gg_function_9(t370, t373, base + 0);
  t375 = ((int64_t)(((int64_t)UINT64_C(1))));
  t376 = gg_function_14(t374, t375, base + 0);
  t377 = ((int64_t)(t376));
  t378 = ((int64_t)(v11));
  t379 = ((t377) != (t378));
  if (t379) {
  {
  t380 = ((int64_t)(v4));
  t381 = fg14_s_vec_free(t380);
  (void)(t381);
  t382 = ((int64_t)(v5));
  t383 = fg14_s_vec_free(t382);
  (void)(t383);
  t384 = ((int64_t)(v6));
  t385 = fg14_s_vec_free(t384);
  (void)(t385);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  t386 = v7;
  t387 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t386) + (uint64_t)(t387)));
  }
  goto label1;
label2:;
  t388 = ((int64_t)(v0));
  t389 = ((int64_t)(v1));
  t390 = gg_function_9(t388, t389, base + 0);
  t391 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = gg_function_29(t390, t391, base + 0);
  t392 = ((int64_t)(v0));
  t393 = ((int64_t)(v1));
  t394 = ((int64_t)(((int64_t)UINT64_C(9))));
  t395 = ((int64_t)(((int64_t)((uint64_t)(t393) + (uint64_t)(t394)))));
  t396 = gg_function_9(t392, t395, base + 0);
  t397 = ((int64_t)(((int64_t)UINT64_C(0))));
  v13 = gg_function_29(t396, t397, base + 0);
  t398 = ((int64_t)(v0));
  t399 = ((int64_t)(v1));
  t400 = ((int64_t)(((int64_t)UINT64_C(3))));
  t401 = ((int64_t)(((int64_t)((uint64_t)(t399) + (uint64_t)(t400)))));
  t402 = gg_function_9(t398, t401, base + 0);
  t403 = ((int64_t)(((int64_t)UINT64_C(0))));
  t404 = gg_function_29(t402, t403, base + 0);
  t405 = ((int64_t)(t404));
  t406 = ((int64_t)(((int64_t)UINT64_C(2))));
  t407 = ((t405) != (t406));
  t408 = t407;
  if (!t408) {
  t409 = ((int64_t)(v0));
  t410 = ((int64_t)(v1));
  t411 = ((int64_t)(((int64_t)UINT64_C(12))));
  t412 = ((int64_t)(((int64_t)((uint64_t)(t410) + (uint64_t)(t411)))));
  t413 = gg_function_9(t409, t412, base + 0);
  t414 = ((int64_t)(((int64_t)UINT64_C(2))));
  t415 = gg_function_14(t413, t414, base + 0);
  t416 = ((int64_t)(t415));
  t417 = ((int64_t)(v0));
  t418 = ((int64_t)(v1));
  t419 = ((int64_t)(((int64_t)UINT64_C(11))));
  t420 = ((int64_t)(((int64_t)((uint64_t)(t418) + (uint64_t)(t419)))));
  t421 = gg_function_9(t417, t420, base + 0);
  t422 = ((int64_t)(t421));
  t408 = ((t416) != (t422));
  }
  t423 = t408;
  t424 = t423;
  if (!t424) {
  t425 = ((int64_t)(v0));
  t426 = ((int64_t)(v1));
  t427 = ((int64_t)(((int64_t)UINT64_C(11))));
  t428 = ((int64_t)(((int64_t)((uint64_t)(t426) + (uint64_t)(t427)))));
  t429 = gg_function_9(t425, t428, base + 0);
  t430 = ((int64_t)(((int64_t)UINT64_C(0))));
  t431 = gg_function_32(t429, t430, base + 0);
  t432 = ((int64_t)(t431));
  t433 = ((int64_t)(((int64_t)UINT64_C(4286578688))));
  t424 = ((t432) != (t433));
  }
  t434 = t424;
  t435 = t434;
  if (!t435) {
  t436 = ((int64_t)(v12));
  t437 = ((int64_t)(((int64_t)UINT64_C(32))));
  t435 = ((t436) > (t437));
  }
  t438 = t435;
  if (t438) {
  {
  t439 = ((int64_t)(v4));
  t440 = fg14_s_vec_free(t439);
  (void)(t440);
  t441 = ((int64_t)(v5));
  t442 = fg14_s_vec_free(t441);
  (void)(t442);
  t443 = ((int64_t)(v6));
  t444 = fg14_s_vec_free(t443);
  (void)(t444);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t445 = ((int64_t)(v4));
  t446 = fg14_s_vec_free(t445);
  (void)(t446);
  t447 = ((int64_t)(v5));
  t448 = fg14_s_vec_free(t447);
  (void)(t448);
  t449 = ((int64_t)(v6));
  t450 = fg14_s_vec_free(t449);
  (void)(t450);
  t451 = ((int64_t)(v13));
  t452 = ((int64_t)(((int64_t)UINT64_C(8))));
  t453 = ((int64_t)(((int64_t)((uint64_t)(t451) << ((uint64_t)(t452) & 63)))));
  t454 = ((int64_t)(v12));
  t0 = ((int64_t)(((t453) | (t454))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_70(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t16;
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
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  uint8_t t33;
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
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
  int64_t t72;
  int64_t t73;
  uint8_t t74;
  uint8_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  uint8_t t82;
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
  uint8_t t96;
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
  uint8_t t152;
  uint8_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  uint8_t t160;
  uint8_t t161;
  int64_t t162;
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
  uint8_t t174;
  uint8_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  uint8_t t182;
  uint8_t t183;
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
  uint8_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  uint8_t t203;
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
  uint8_t t231;
  uint8_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  uint8_t t239;
  uint8_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  uint8_t t248;
  uint8_t t249;
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
  uint8_t t262;
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
  uint8_t t285;
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
  uint8_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  uint8_t t303;
  uint8_t t304;
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
  uint8_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  uint8_t t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  uint8_t t331;
  uint8_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  uint8_t t339;
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
  uint8_t t353;
  int64_t t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  uint8_t t358;
  uint8_t t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t t364;
  int64_t t365;
  uint8_t t366;
  uint8_t t367;
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
  uint8_t t380;
  int64_t t381;
  int64_t t382;
  int64_t t383;
  int64_t t384;
  uint8_t t385;
  uint8_t t386;
  int64_t t387;
  int64_t t388;
  int64_t t389;
  int64_t t390;
  int64_t t391;
  int64_t t392;
  uint8_t t393;
  uint8_t t394;
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
  uint8_t t412;
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
  uint8_t t461;
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
  uint8_t t474;
  uint8_t t475;
  int64_t t476;
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
  uint8_t t488;
  int64_t t489;
  int64_t t490;
  int64_t t491;
  int64_t t492;
  int64_t t493;
  int64_t t494;
  int64_t t495;
  int64_t t496;
  int64_t t497;
  int64_t t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  uint8_t t502;
  uint8_t t503;
  int64_t t504;
  int64_t t505;
  int64_t t506;
  int64_t t507;
  int64_t t508;
  int64_t t509;
  uint8_t t510;
  uint8_t t511;
  int64_t t512;
  int64_t t513;
  int64_t t514;
  int64_t t515;
  int64_t t516;
  int64_t t517;
  int64_t t518;
  int64_t t519;
  int64_t t520;
  int64_t t521;
  int64_t t522;
  int64_t t523;
  uint8_t t524;
  int64_t t525;
  int64_t t526;
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
  {
  t1 = ((int64_t)(v0));
  t2 = fg14_s_g_n(t1);
  v2 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  v3 = gg_function_9(t3, t4, base + 0);
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = ((int64_t)(v1));
  t5 = ((int64_t)(v3));
  t6 = gg_function_12(t5, base + 0);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(46))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)UINT64_C(4))));
  v4 = ((int64_t)(((t10) | (t11))));
  }
  }
  t12 = ((int64_t)(v3));
  t13 = gg_function_12(t12, base + 0);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(91))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  v6 = gg_function_19(t17, t18, base + 0);
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)UINT64_C(7))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((t22) | (t23))));
  }
  } else {
  {
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)UINT64_C(15))));
  t26 = ((t24) == (t25));
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(v2));
  t27 = ((t30) < (t31));
  }
  t32 = t27;
  t33 = t32;
  if (t33) {
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t38 = gg_function_9(t34, t37, base + 0);
  t39 = gg_function_12(t38, base + 0);
  t40 = ((int64_t)(t39));
  t41 = ((int64_t)(((int64_t)UINT64_C(10))));
  t33 = ((t40) == (t41));
  }
  t42 = t33;
  t43 = t42;
  if (t43) {
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  t48 = gg_function_9(t44, t47, base + 0);
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = gg_function_14(t48, t49, base + 0);
  t51 = ((int64_t)(t50));
  t52 = ((int64_t)(v3));
  t43 = ((t51) == (t52));
  }
  t53 = t43;
  if (t53) {
  {
  t54 = ((int64_t)(v4));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  v4 = ((int64_t)(((t54) | (t55))));
  t56 = ((int64_t)(v1));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t58 = ((int64_t)(v3));
  t59 = gg_function_12(t58, base + 0);
  t60 = ((int64_t)(t59));
  t61 = ((int64_t)(((int64_t)UINT64_C(69))));
  t62 = ((t60) == (t61));
  if (t62) {
  {
  t63 = ((int64_t)(v4));
  t64 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((t63) | (t64))));
  }
  }
  t65 = v5;
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t65) + (uint64_t)(t66)));
  t67 = ((int64_t)(v4));
  t68 = ((int64_t)(((int64_t)UINT64_C(7))));
  t69 = ((int64_t)(((t67) & (t68))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((t69) != (t70));
  if (t71) {
  {
  t72 = ((int64_t)(v5));
  t73 = ((int64_t)(v2));
  t74 = ((t72) >= (t73));
  t75 = t74;
  if (!t75) {
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(v5));
  t78 = gg_function_9(t76, t77, base + 0);
  t79 = gg_function_12(t78, base + 0);
  t80 = ((int64_t)(t79));
  t81 = ((int64_t)(((int64_t)UINT64_C(36))));
  t75 = ((t80) != (t81));
  }
  t82 = t75;
  t83 = t82;
  if (!t83) {
  t84 = ((int64_t)(v0));
  t85 = ((int64_t)(v5));
  t86 = gg_function_9(t84, t85, base + 0);
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = gg_function_14(t86, t87, base + 0);
  t89 = ((int64_t)(t88));
  t90 = ((int64_t)(v0));
  t91 = ((int64_t)(v5));
  t92 = ((int64_t)(((int64_t)UINT64_C(1))));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) - (uint64_t)(t92)))));
  t94 = gg_function_9(t90, t93, base + 0);
  t95 = ((int64_t)(t94));
  t83 = ((t89) != (t95));
  }
  t96 = t83;
  if (t96) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t97 = ((int64_t)(v0));
  t98 = ((int64_t)(v5));
  v7 = gg_function_9(t97, t98, base + 0);
  t99 = v5;
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t99) + (uint64_t)(t100)));
  t101 = ((int64_t)(v5));
  t102 = ((int64_t)(v2));
  t103 = ((t101) >= (t102));
  if (t103) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(v5));
  t106 = ((int64_t)(((int64_t)UINT64_C(2))));
  t107 = ((int64_t)(((int64_t)((uint64_t)(t105) - (uint64_t)(t106)))));
  v8 = gg_function_9(t104, t107, base + 0);
  t108 = ((int64_t)(v0));
  t109 = ((int64_t)(v5));
  t110 = gg_function_9(t108, t109, base + 0);
  t111 = gg_function_12(t110, base + 0);
  t112 = ((int64_t)(t111));
  t113 = ((int64_t)(((int64_t)UINT64_C(2))));
  t114 = ((t112) == (t113));
  t115 = t114;
  if (t115) {
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(v5));
  t118 = gg_function_9(t116, t117, base + 0);
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = gg_function_14(t118, t119, base + 0);
  t121 = ((int64_t)(t120));
  t122 = ((int64_t)(v8));
  t115 = ((t121) == (t122));
  }
  t123 = t115;
  if (t123) {
  {
  t124 = ((int64_t)(v4));
  t125 = ((int64_t)(((int64_t)UINT64_C(16))));
  v4 = ((int64_t)(((t124) | (t125))));
  t126 = ((int64_t)(v0));
  t127 = ((int64_t)(v5));
  v8 = gg_function_9(t126, t127, base + 0);
  t128 = v5;
  t129 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t128) + (uint64_t)(t129)));
  }
  }
  t130 = ((int64_t)(v0));
  t131 = ((int64_t)(v5));
  t132 = ((int64_t)(v8));
  v9 = gg_function_71(t130, t131, t132, base + 0);
  t133 = ((int64_t)(v9));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((t133) > (t134));
  if (t135) {
  {
  t136 = ((int64_t)(v4));
  t137 = ((int64_t)(v9));
  t138 = ((int64_t)(((int64_t)UINT64_C(255))));
  t139 = ((int64_t)(((t137) & (t138))));
  t140 = ((int64_t)(((int64_t)UINT64_C(8))));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t139) << ((uint64_t)(t140) & 63)))));
  t142 = ((int64_t)(((t136) | (t141))));
  t143 = ((int64_t)(v9));
  t144 = ((int64_t)(((int64_t)UINT64_C(8))));
  t145 = ((int64_t)(((t143) >> ((uint64_t)(t144) & 63))));
  t146 = ((int64_t)(((int64_t)UINT64_C(16))));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t145) << ((uint64_t)(t146) & 63)))));
  v4 = ((int64_t)(((t142) | (t147))));
  t148 = v5;
  t149 = ((int64_t)(v9));
  v5 = ((int64_t)((uint64_t)(t148) + (uint64_t)(t149)));
  }
  }
  t150 = ((int64_t)(v5));
  t151 = ((int64_t)(v2));
  t152 = ((t150) >= (t151));
  t153 = t152;
  if (!t153) {
  t154 = ((int64_t)(v0));
  t155 = ((int64_t)(v5));
  t156 = gg_function_9(t154, t155, base + 0);
  t157 = gg_function_12(t156, base + 0);
  t158 = ((int64_t)(t157));
  t159 = ((int64_t)(((int64_t)UINT64_C(69))));
  t153 = ((t158) != (t159));
  }
  t160 = t153;
  t161 = t160;
  if (!t161) {
  t162 = ((int64_t)(v0));
  t163 = ((int64_t)(v5));
  t164 = gg_function_9(t162, t163, base + 0);
  t165 = ((int64_t)(((int64_t)UINT64_C(0))));
  t166 = gg_function_14(t164, t165, base + 0);
  t167 = ((int64_t)(t166));
  t168 = ((int64_t)(v8));
  t161 = ((t167) != (t168));
  }
  t169 = t161;
  if (t169) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t170 = v5;
  t171 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t170) + (uint64_t)(t171)));
  t172 = ((int64_t)(v5));
  t173 = ((int64_t)(v2));
  t174 = ((t172) >= (t173));
  t175 = t174;
  if (!t175) {
  t176 = ((int64_t)(v0));
  t177 = ((int64_t)(v5));
  t178 = gg_function_9(t176, t177, base + 0);
  t179 = gg_function_12(t178, base + 0);
  t180 = ((int64_t)(t179));
  t181 = ((int64_t)(((int64_t)UINT64_C(37))));
  t175 = ((t180) != (t181));
  }
  t182 = t175;
  t183 = t182;
  if (!t183) {
  t184 = ((int64_t)(v0));
  t185 = ((int64_t)(v5));
  t186 = gg_function_9(t184, t185, base + 0);
  t187 = ((int64_t)(((int64_t)UINT64_C(0))));
  t188 = gg_function_14(t186, t187, base + 0);
  t189 = ((int64_t)(t188));
  t190 = ((int64_t)(v0));
  t191 = ((int64_t)(v5));
  t192 = ((int64_t)(((int64_t)UINT64_C(1))));
  t193 = ((int64_t)(((int64_t)((uint64_t)(t191) - (uint64_t)(t192)))));
  t194 = gg_function_9(t190, t193, base + 0);
  t195 = ((int64_t)(t194));
  t183 = ((t189) != (t195));
  }
  t196 = t183;
  if (t196) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t197 = ((int64_t)(v4));
  t198 = ((int64_t)(((int64_t)UINT64_C(8))));
  t199 = ((int64_t)(((t197) >> ((uint64_t)(t198) & 63))));
  t200 = ((int64_t)(((int64_t)UINT64_C(255))));
  t201 = ((int64_t)(((t199) & (t200))));
  t202 = ((int64_t)(((int64_t)UINT64_C(0))));
  t203 = ((t201) > (t202));
  if (t203) {
  {
  t204 = ((int64_t)(v4));
  t205 = ((int64_t)(((int64_t)UINT64_C(8))));
  t206 = ((int64_t)(((t204) >> ((uint64_t)(t205) & 63))));
  t207 = ((int64_t)(((int64_t)UINT64_C(255))));
  v10 = ((int64_t)(((t206) & (t207))));
  t208 = ((int64_t)(v4));
  t209 = ((int64_t)(((int64_t)UINT64_C(16))));
  t210 = ((int64_t)(((t208) >> ((uint64_t)(t209) & 63))));
  t211 = ((int64_t)(((int64_t)UINT64_C(255))));
  v11 = ((int64_t)(((t210) & (t211))));
  t212 = ((int64_t)(v0));
  t213 = ((int64_t)(v5));
  t214 = gg_function_9(t212, t213, base + 0);
  t215 = ((int64_t)(((int64_t)UINT64_C(0))));
  t216 = gg_function_29(t214, t215, base + 0);
  t217 = ((int64_t)(t216));
  t218 = ((int64_t)(v11));
  t219 = ((int64_t)(v7));
  t220 = ((int64_t)(((int64_t)UINT64_C(1))));
  t221 = gg_function_29(t219, t220, base + 0);
  t222 = ((int64_t)(t221));
  t223 = ((int64_t)(v10));
  t224 = ((int64_t)(((int64_t)gg_div(t222, t223))));
  t225 = ((int64_t)(((int64_t)((uint64_t)(t218) * (uint64_t)(t224)))));
  t226 = ((t217) > (t225));
  if (t226) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  t227 = v5;
  t228 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t227) + (uint64_t)(t228)));
  t229 = ((int64_t)(v5));
  t230 = ((int64_t)(v2));
  t231 = ((t229) >= (t230));
  t232 = t231;
  if (!t232) {
  t233 = ((int64_t)(v0));
  t234 = ((int64_t)(v5));
  t235 = gg_function_9(t233, t234, base + 0);
  t236 = gg_function_12(t235, base + 0);
  t237 = ((int64_t)(t236));
  t238 = ((int64_t)(((int64_t)UINT64_C(40))));
  t232 = ((t237) != (t238));
  }
  t239 = t232;
  t240 = t239;
  if (!t240) {
  t241 = ((int64_t)(v0));
  t242 = ((int64_t)(v5));
  t243 = gg_function_9(t241, t242, base + 0);
  t244 = ((int64_t)(((int64_t)UINT64_C(0))));
  t245 = gg_function_14(t243, t244, base + 0);
  t246 = ((int64_t)(t245));
  t247 = ((int64_t)(v7));
  t240 = ((t246) != (t247));
  }
  t248 = t240;
  t249 = t248;
  if (!t249) {
  t250 = ((int64_t)(v0));
  t251 = ((int64_t)(v5));
  t252 = gg_function_9(t250, t251, base + 0);
  t253 = ((int64_t)(((int64_t)UINT64_C(1))));
  t254 = gg_function_14(t252, t253, base + 0);
  t255 = ((int64_t)(t254));
  t256 = ((int64_t)(v0));
  t257 = ((int64_t)(v5));
  t258 = ((int64_t)(((int64_t)UINT64_C(1))));
  t259 = ((int64_t)(((int64_t)((uint64_t)(t257) - (uint64_t)(t258)))));
  t260 = gg_function_9(t256, t259, base + 0);
  t261 = ((int64_t)(t260));
  t249 = ((t255) != (t261));
  }
  t262 = t249;
  if (t262) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t263 = v5;
  t264 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t263) + (uint64_t)(t264)));
  }
  } else {
  {
  t265 = ((int64_t)(v5));
  t266 = ((int64_t)(((int64_t)UINT64_C(2))));
  t267 = ((int64_t)(((int64_t)((uint64_t)(t265) - (uint64_t)(t266)))));
  t268 = ((int64_t)(((int64_t)UINT64_C(0))));
  t269 = ((t267) < (t268));
  if (t269) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t270 = ((int64_t)(v0));
  t271 = ((int64_t)(v5));
  t272 = ((int64_t)(((int64_t)UINT64_C(2))));
  t273 = ((int64_t)(((int64_t)((uint64_t)(t271) - (uint64_t)(t272)))));
  v12 = gg_function_9(t270, t273, base + 0);
  t274 = ((int64_t)(v5));
  t275 = ((int64_t)(v2));
  t276 = ((t274) >= (t275));
  t277 = t276;
  if (!t277) {
  t278 = ((int64_t)(v0));
  t279 = ((int64_t)(v5));
  t280 = gg_function_9(t278, t279, base + 0);
  t281 = gg_function_12(t280, base + 0);
  t282 = ((int64_t)(t281));
  t283 = ((int64_t)(((int64_t)UINT64_C(37))));
  t277 = ((t282) != (t283));
  }
  t284 = t277;
  t285 = t284;
  if (!t285) {
  t286 = ((int64_t)(v0));
  t287 = ((int64_t)(v5));
  t288 = gg_function_9(t286, t287, base + 0);
  t289 = ((int64_t)(((int64_t)UINT64_C(0))));
  t290 = gg_function_14(t288, t289, base + 0);
  t291 = ((int64_t)(t290));
  t292 = ((int64_t)(v0));
  t293 = ((int64_t)(v5));
  t294 = ((int64_t)(((int64_t)UINT64_C(1))));
  t295 = ((int64_t)(((int64_t)((uint64_t)(t293) - (uint64_t)(t294)))));
  t296 = gg_function_9(t292, t295, base + 0);
  t297 = ((int64_t)(t296));
  t285 = ((t291) != (t297));
  }
  t298 = t285;
  if (t298) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t299 = v5;
  t300 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t299) + (uint64_t)(t300)));
  t301 = ((int64_t)(v5));
  t302 = ((int64_t)(v2));
  t303 = ((t301) >= (t302));
  t304 = t303;
  if (!t304) {
  t305 = ((int64_t)(v0));
  t306 = ((int64_t)(v5));
  t307 = gg_function_9(t305, t306, base + 0);
  t308 = ((int64_t)(((int64_t)UINT64_C(1))));
  t309 = gg_function_14(t307, t308, base + 0);
  t310 = ((int64_t)(t309));
  t311 = ((int64_t)(v0));
  t312 = ((int64_t)(v5));
  t313 = ((int64_t)(((int64_t)UINT64_C(1))));
  t314 = ((int64_t)(((int64_t)((uint64_t)(t312) - (uint64_t)(t313)))));
  t315 = gg_function_9(t311, t314, base + 0);
  t316 = ((int64_t)(t315));
  t304 = ((t310) != (t316));
  }
  t317 = t304;
  t318 = t317;
  if (!t318) {
  t319 = ((int64_t)(v0));
  t320 = ((int64_t)(v5));
  t321 = gg_function_9(t319, t320, base + 0);
  t322 = ((int64_t)(((int64_t)UINT64_C(0))));
  t323 = gg_function_14(t321, t322, base + 0);
  t324 = ((int64_t)(t323));
  t325 = ((int64_t)(v12));
  t318 = ((t324) != (t325));
  }
  t326 = t318;
  if (t326) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t327 = v5;
  t328 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t327) + (uint64_t)(t328)));
  t329 = ((int64_t)(v5));
  t330 = ((int64_t)(v2));
  t331 = ((t329) >= (t330));
  t332 = t331;
  if (!t332) {
  t333 = ((int64_t)(v0));
  t334 = ((int64_t)(v5));
  t335 = gg_function_9(t333, t334, base + 0);
  t336 = gg_function_12(t335, base + 0);
  t337 = ((int64_t)(t336));
  t338 = ((int64_t)(((int64_t)UINT64_C(36))));
  t332 = ((t337) != (t338));
  }
  t339 = t332;
  t340 = t339;
  if (!t340) {
  t341 = ((int64_t)(v0));
  t342 = ((int64_t)(v5));
  t343 = gg_function_9(t341, t342, base + 0);
  t344 = ((int64_t)(((int64_t)UINT64_C(0))));
  t345 = gg_function_14(t343, t344, base + 0);
  t346 = ((int64_t)(t345));
  t347 = ((int64_t)(v0));
  t348 = ((int64_t)(v5));
  t349 = ((int64_t)(((int64_t)UINT64_C(1))));
  t350 = ((int64_t)(((int64_t)((uint64_t)(t348) - (uint64_t)(t349)))));
  t351 = gg_function_9(t347, t350, base + 0);
  t352 = ((int64_t)(t351));
  t340 = ((t346) != (t352));
  }
  t353 = t340;
  if (t353) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t354 = v5;
  t355 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t354) + (uint64_t)(t355)));
  t356 = ((int64_t)(v5));
  t357 = ((int64_t)(v2));
  t358 = ((t356) >= (t357));
  t359 = t358;
  if (!t359) {
  t360 = ((int64_t)(v0));
  t361 = ((int64_t)(v5));
  t362 = gg_function_9(t360, t361, base + 0);
  t363 = gg_function_12(t362, base + 0);
  t364 = ((int64_t)(t363));
  t365 = ((int64_t)(((int64_t)UINT64_C(46))));
  t359 = ((t364) != (t365));
  }
  t366 = t359;
  t367 = t366;
  if (!t367) {
  t368 = ((int64_t)(v0));
  t369 = ((int64_t)(v5));
  t370 = gg_function_9(t368, t369, base + 0);
  t371 = ((int64_t)(((int64_t)UINT64_C(0))));
  t372 = gg_function_14(t370, t371, base + 0);
  t373 = ((int64_t)(t372));
  t374 = ((int64_t)(v0));
  t375 = ((int64_t)(v5));
  t376 = ((int64_t)(((int64_t)UINT64_C(1))));
  t377 = ((int64_t)(((int64_t)((uint64_t)(t375) - (uint64_t)(t376)))));
  t378 = gg_function_9(t374, t377, base + 0);
  t379 = ((int64_t)(t378));
  t367 = ((t373) != (t379));
  }
  t380 = t367;
  if (t380) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t381 = v5;
  t382 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t381) + (uint64_t)(t382)));
  t383 = ((int64_t)(v5));
  t384 = ((int64_t)(v2));
  t385 = ((t383) >= (t384));
  t386 = t385;
  if (!t386) {
  t387 = ((int64_t)(v0));
  t388 = ((int64_t)(v5));
  t389 = gg_function_9(t387, t388, base + 0);
  t390 = gg_function_12(t389, base + 0);
  t391 = ((int64_t)(t390));
  t392 = ((int64_t)(((int64_t)UINT64_C(36))));
  t386 = ((t391) != (t392));
  }
  t393 = t386;
  t394 = t393;
  if (!t394) {
  t395 = ((int64_t)(v0));
  t396 = ((int64_t)(v5));
  t397 = gg_function_9(t395, t396, base + 0);
  t398 = ((int64_t)(((int64_t)UINT64_C(0))));
  t399 = gg_function_14(t397, t398, base + 0);
  t400 = ((int64_t)(t399));
  t401 = ((int64_t)(v0));
  t402 = ((int64_t)(v5));
  t403 = ((int64_t)(((int64_t)UINT64_C(1))));
  t404 = ((int64_t)(((int64_t)((uint64_t)(t402) - (uint64_t)(t403)))));
  t405 = gg_function_9(t401, t404, base + 0);
  t406 = ((int64_t)(t405));
  t394 = ((t400) != (t406));
  }
  t407 = t394;
  if (t407) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t408 = v5;
  t409 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t408) + (uint64_t)(t409)));
  }
  }
  t410 = ((int64_t)(v5));
  t411 = ((int64_t)(v2));
  t412 = ((t410) >= (t411));
  if (t412) {
  {
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
  }
  t413 = ((int64_t)(v0));
  t414 = ((int64_t)(v5));
  t415 = gg_function_9(t413, t414, base + 0);
  t416 = gg_function_12(t415, base + 0);
  t417 = ((int64_t)(t416));
  t418 = ((int64_t)(((int64_t)UINT64_C(36))));
  t419 = ((t417) == (t418));
  if (t419) {
  {
  t420 = ((int64_t)(v4));
  t421 = ((int64_t)(((int64_t)UINT64_C(32))));
  v4 = ((int64_t)(((t420) | (t421))));
  t422 = ((int64_t)(((int64_t)UINT64_C(8))));
  t423 = fg14_s_vec_new(t422);
  v13 = ((int64_t)(t423));
  t424 = ((int64_t)(v13));
  t425 = ((int64_t)(((int64_t)UINT64_C(5))));
  t426 = fg14_s_vec_resize(t424, t425);
  (void)(t426);
  t427 = ((int64_t)(v13));
  t428 = fg14_s_vec_data(t427);
  t429 = ((int64_t)(t428));
  t430 = ((int64_t)(((int64_t)UINT64_C(0))));
  t431 = ((int64_t)(((int64_t)UINT64_C(36))));
  t432 = fg14_s_frame_set(t429, t430, t431);
  (void)(t432);
  t433 = ((int64_t)(v13));
  t434 = fg14_s_vec_data(t433);
  t435 = ((int64_t)(t434));
  t436 = ((int64_t)(((int64_t)UINT64_C(1))));
  t437 = ((int64_t)(((int64_t)UINT64_C(15))));
  t438 = fg14_s_frame_set(t435, t436, t437);
  (void)(t438);
  t439 = ((int64_t)(v13));
  t440 = fg14_s_vec_data(t439);
  t441 = ((int64_t)(t440));
  t442 = ((int64_t)(((int64_t)UINT64_C(2))));
  t443 = ((int64_t)(((int64_t)UINT64_C(50))));
  t444 = fg14_s_frame_set(t441, t442, t443);
  (void)(t444);
  t445 = ((int64_t)(v13));
  t446 = fg14_s_vec_data(t445);
  t447 = ((int64_t)(t446));
  t448 = ((int64_t)(((int64_t)UINT64_C(3))));
  t449 = ((int64_t)(((int64_t)UINT64_C(8))));
  t450 = fg14_s_frame_set(t447, t448, t449);
  (void)(t450);
  t451 = ((int64_t)(v13));
  t452 = fg14_s_vec_data(t451);
  t453 = ((int64_t)(t452));
  t454 = ((int64_t)(((int64_t)UINT64_C(4))));
  t455 = ((int64_t)(((int64_t)UINT64_C(36))));
  t456 = fg14_s_frame_set(t453, t454, t455);
  (void)(t456);
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t457 = ((int64_t)(v14));
  t458 = ((int64_t)(((int64_t)UINT64_C(5))));
  if (!(((t457) < (t458)))) goto label2;
  {
  t459 = ((int64_t)(v5));
  t460 = ((int64_t)(v2));
  t461 = ((t459) >= (t460));
  t462 = t461;
  if (!t462) {
  t463 = ((int64_t)(v0));
  t464 = ((int64_t)(v5));
  t465 = gg_function_9(t463, t464, base + 0);
  t466 = gg_function_12(t465, base + 0);
  t467 = ((int64_t)(t466));
  t468 = ((int64_t)(v13));
  t469 = fg14_s_vec_data(t468);
  t470 = ((int64_t)(t469));
  t471 = ((int64_t)(v14));
  t472 = fg14_s_frame_get(t470, t471);
  t473 = ((int64_t)(t472));
  t462 = ((t467) != (t473));
  }
  t474 = t462;
  t475 = t474;
  if (!t475) {
  t476 = ((int64_t)(v0));
  t477 = ((int64_t)(v5));
  t478 = gg_function_9(t476, t477, base + 0);
  t479 = ((int64_t)(((int64_t)UINT64_C(0))));
  t480 = gg_function_14(t478, t479, base + 0);
  t481 = ((int64_t)(t480));
  t482 = ((int64_t)(v0));
  t483 = ((int64_t)(v5));
  t484 = ((int64_t)(((int64_t)UINT64_C(1))));
  t485 = ((int64_t)(((int64_t)((uint64_t)(t483) - (uint64_t)(t484)))));
  t486 = gg_function_9(t482, t485, base + 0);
  t487 = ((int64_t)(t486));
  t475 = ((t481) != (t487));
  }
  t488 = t475;
  if (t488) {
  {
  t489 = ((int64_t)(v4));
  t490 = ((int64_t)UINT64_C(32));
  t491 = ((int64_t)((~(t490))));
  v4 = ((int64_t)(((t489) & (t491))));
  t492 = ((int64_t)(v13));
  t493 = fg14_s_vec_free(t492);
  (void)(t493);
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
  }
  t494 = v5;
  t495 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t494) + (uint64_t)(t495)));
  t496 = v14;
  t497 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t496) + (uint64_t)(t497)));
  }
  goto label1;
label2:;
  t498 = ((int64_t)(v13));
  t499 = fg14_s_vec_free(t498);
  (void)(t499);
  }
  }
  t500 = ((int64_t)(v5));
  t501 = ((int64_t)(v2));
  t502 = ((t500) < (t501));
  t503 = t502;
  if (t503) {
  t504 = ((int64_t)(v0));
  t505 = ((int64_t)(v5));
  t506 = gg_function_9(t504, t505, base + 0);
  t507 = gg_function_12(t506, base + 0);
  t508 = ((int64_t)(t507));
  t509 = ((int64_t)(((int64_t)UINT64_C(32))));
  t503 = ((t508) == (t509));
  }
  t510 = t503;
  t511 = t510;
  if (t511) {
  t512 = ((int64_t)(v0));
  t513 = ((int64_t)(v5));
  t514 = gg_function_9(t512, t513, base + 0);
  t515 = ((int64_t)(((int64_t)UINT64_C(0))));
  t516 = gg_function_14(t514, t515, base + 0);
  t517 = ((int64_t)(t516));
  t518 = ((int64_t)(v0));
  t519 = ((int64_t)(v5));
  t520 = ((int64_t)(((int64_t)UINT64_C(1))));
  t521 = ((int64_t)(((int64_t)((uint64_t)(t519) - (uint64_t)(t520)))));
  t522 = gg_function_9(t518, t521, base + 0);
  t523 = ((int64_t)(t522));
  t511 = ((t517) == (t523));
  }
  t524 = t511;
  if (t524) {
  {
  t525 = ((int64_t)(v4));
  t526 = ((int64_t)(((int64_t)UINT64_C(64))));
  v4 = ((int64_t)(((t525) | (t526))));
  }
  }
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_60(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(22))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg14_s_t_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_64(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg14_s_t_get(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_68(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
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
  int64_t t65;
  int64_t t66;
  uint8_t t67;
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
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  uint8_t t88;
  uint8_t t89;
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
  uint8_t t100;
  uint8_t t101;
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
  uint8_t t112;
  uint8_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  uint8_t t118;
  uint8_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint8_t t124;
  uint8_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  uint8_t t131;
  uint8_t t132;
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
  uint8_t t145;
  uint8_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  uint8_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  uint8_t t158;
  uint8_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint8_t t168;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  v3 = gg_function_9(t1, t2, base + 0);
  t3 = ((int64_t)(v3));
  t4 = gg_function_12(t3, base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(77))));
  t7 = ((t5) != (t6));
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v3));
  t10 = gg_function_16(t9, base + 0);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t11) != (t12));
  }
  t13 = t8;
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v3));
  t16 = gg_function_64(t15, base + 0);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(32))));
  t19 = ((int64_t)(((t17) & (t18))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t19) != (t20));
  }
  t21 = t14;
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  v4 = gg_function_19(t22, t23, base + 0);
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = gg_function_14(t24, t25, base + 0);
  t26 = ((int64_t)(v3));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = gg_function_14(t26, t27, base + 0);
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)UINT64_C(3))));
  v7 = gg_function_14(t28, t29, base + 0);
  t30 = ((int64_t)(v5));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  v8 = gg_function_29(t30, t31, base + 0);
  t32 = ((int64_t)(v8));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = gg_function_29(t33, t34, base + 0);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t36)))));
  t38 = ((int64_t)(v6));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = gg_function_29(t38, t39, base + 0);
  t41 = ((int64_t)(t40));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t41)))));
  t42 = ((int64_t)(v6));
  t43 = ((int64_t)(((int64_t)UINT64_C(2))));
  v10 = gg_function_29(t42, t43, base + 0);
  t44 = ((int64_t)(v6));
  t45 = ((int64_t)(((int64_t)UINT64_C(3))));
  v11 = gg_function_29(t44, t45, base + 0);
  t46 = ((int64_t)(v7));
  t47 = ((int64_t)(((int64_t)UINT64_C(1))));
  t48 = gg_function_48(t46, t47, base + 0);
  t49 = ((int64_t)(t48));
  t50 = ((int64_t)(((int64_t)UINT64_C(4))));
  t51 = ((t49) != (t50));
  t52 = t51;
  if (!t52) {
  t53 = ((int64_t)(v8));
  t54 = ((int64_t)(((int64_t)UINT64_C(96))));
  t55 = ((t53) != (t54));
  t56 = t55;
  if (t56) {
  t57 = ((int64_t)(v8));
  t58 = ((int64_t)(((int64_t)UINT64_C(128))));
  t56 = ((t57) != (t58));
  }
  t59 = t56;
  t60 = t59;
  if (t60) {
  t61 = ((int64_t)(v8));
  t62 = ((int64_t)(((int64_t)UINT64_C(256))));
  t60 = ((t61) != (t62));
  }
  t52 = t60;
  }
  t63 = t52;
  t64 = t63;
  if (!t64) {
  t65 = ((int64_t)(v11));
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  t64 = ((t65) != (t66));
  }
  t67 = t64;
  if (t67) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v12 = ((int64_t)(v10));
  t68 = ((int64_t)(v4));
  t69 = ((int64_t)(v12));
  t70 = ((t68) < (t69));
  if (t70) {
  {
  v12 = ((int64_t)(v4));
  }
  }
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((int64_t)(v6));
  t73 = fg14_s_t_nelements(t72);
  t74 = ((int64_t)(t73));
  t75 = fg14_s_ggml_row_size(t71, t74);
  v13 = ((int64_t)(t75));
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(v2));
  t78 = gg_function_9(t76, t77, base + 0);
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  v14 = gg_function_14(t78, t79, base + 0);
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(v2));
  t82 = gg_function_9(t80, t81, base + 0);
  t83 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = gg_function_14(t82, t83, base + 0);
  t84 = ((int64_t)(v14));
  t85 = gg_function_12(t84, base + 0);
  t86 = ((int64_t)(t85));
  t87 = ((int64_t)(((int64_t)UINT64_C(37))));
  t88 = ((t86) != (t87));
  t89 = t88;
  if (!t89) {
  t90 = ((int64_t)(v14));
  t91 = gg_function_53(t90, base + 0);
  t92 = ((int64_t)(t91));
  t93 = ((int64_t)(v3));
  t89 = ((t92) != (t93));
  }
  t94 = t89;
  t95 = t94;
  if (!t95) {
  t96 = ((int64_t)(v14));
  t97 = gg_function_60(t96, base + 0);
  t98 = ((int64_t)(t97));
  t99 = ((int64_t)(v13));
  t95 = ((t98) != (t99));
  }
  t100 = t95;
  t101 = t100;
  if (!t101) {
  t102 = ((int64_t)(v14));
  t103 = ((int64_t)(((int64_t)UINT64_C(12))));
  t104 = fg14_s_contig(t102, t103);
  t105 = ((int64_t)(t104));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = ((t105) == (t106));
  }
  t107 = t101;
  if (t107) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t108 = ((int64_t)(v15));
  t109 = gg_function_12(t108, base + 0);
  t110 = ((int64_t)(t109));
  t111 = ((int64_t)(((int64_t)UINT64_C(37))));
  t112 = ((t110) != (t111));
  t113 = t112;
  if (!t113) {
  t114 = ((int64_t)(v15));
  t115 = gg_function_16(t114, base + 0);
  t116 = ((int64_t)(t115));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = ((t116) != (t117));
  }
  t118 = t113;
  t119 = t118;
  if (!t119) {
  t120 = ((int64_t)(v15));
  t121 = gg_function_23(t120, base + 0);
  t122 = ((int64_t)(t121));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((t122) == (t123));
  }
  t124 = t119;
  t125 = t124;
  if (!t125) {
  t126 = ((int64_t)(v15));
  t127 = ((int64_t)(((int64_t)UINT64_C(0))));
  t128 = gg_function_29(t126, t127, base + 0);
  t129 = ((int64_t)(t128));
  t130 = ((int64_t)(v9));
  t125 = ((t129) != (t130));
  }
  t131 = t125;
  t132 = t131;
  if (!t132) {
  t133 = ((int64_t)(v15));
  t134 = ((int64_t)(((int64_t)UINT64_C(1))));
  t135 = gg_function_29(t133, t134, base + 0);
  t136 = ((int64_t)(t135));
  t137 = ((int64_t)(v11));
  t132 = ((t136) != (t137));
  }
  t138 = t132;
  t139 = t138;
  if (!t139) {
  t140 = ((int64_t)(v15));
  t141 = ((int64_t)(((int64_t)UINT64_C(2))));
  t142 = gg_function_29(t140, t141, base + 0);
  t143 = ((int64_t)(t142));
  t144 = ((int64_t)(v12));
  t139 = ((t143) != (t144));
  }
  t145 = t139;
  t146 = t145;
  if (!t146) {
  t147 = ((int64_t)(v15));
  t148 = ((int64_t)(((int64_t)UINT64_C(3))));
  t149 = gg_function_29(t147, t148, base + 0);
  t150 = ((int64_t)(t149));
  t151 = ((int64_t)(((int64_t)UINT64_C(1))));
  t146 = ((t150) != (t151));
  }
  t152 = t146;
  if (t152) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t153 = ((int64_t)(v15));
  t154 = ((int64_t)(((int64_t)UINT64_C(0))));
  t155 = gg_function_48(t153, t154, base + 0);
  t156 = ((int64_t)(t155));
  t157 = ((int64_t)(((int64_t)UINT64_C(4))));
  t158 = ((t156) != (t157));
  t159 = t158;
  if (!t159) {
  t160 = ((int64_t)(v15));
  t161 = ((int64_t)(((int64_t)UINT64_C(1))));
  t162 = gg_function_48(t160, t161, base + 0);
  t163 = ((int64_t)(t162));
  t164 = ((int64_t)(((int64_t)UINT64_C(0))));
  t165 = ((int64_t)(v9));
  t166 = fg14_s_ggml_row_size(t164, t165);
  t167 = ((int64_t)(t166));
  t159 = ((t163) != (t167));
  }
  t168 = t159;
  if (t168) {
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
static int64_t gg_function_62(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
label1:;
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v0));
  t5 = fg14_s_g_n(t4);
  t6 = ((int64_t)(t5));
  if (!(((t3) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v2));
  v3 = gg_function_9(t7, t8, base + 0);
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v3));
  t13 = fg14_s_node_is_view_or_noop(t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v3));
  t18 = gg_function_12(t17, base + 0);
  t19 = ((int64_t)(t18));
  t20 = ((int64_t)(((int64_t)UINT64_C(34))));
  t21 = ((t19) != (t20));
  t22 = t21;
  if (!t22) {
  t23 = ((int64_t)(v3));
  t24 = gg_function_64(t23, base + 0);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(((int64_t)UINT64_C(32))));
  t27 = ((int64_t)(((t25) & (t26))));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t27) != (t28));
  }
  t29 = t22;
  if (t29) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
  }
  t30 = v2;
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t30) + (uint64_t)(t31)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_65(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t62;
  uint8_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint8_t t68;
  uint8_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  uint8_t t74;
  uint8_t t75;
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
  uint8_t t86;
  uint8_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
  uint8_t t93;
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
  uint8_t t105;
  uint8_t t106;
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
  uint8_t t119;
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
  int64_t t130;
  int64_t t131;
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
  uint8_t t142;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  v3 = gg_function_9(t1, t2, base + 0);
  t3 = ((int64_t)(v3));
  t4 = gg_function_12(t3, base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(86))));
  t7 = ((t5) != (t6));
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v3));
  t10 = gg_function_16(t9, base + 0);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t11) != (t12));
  }
  t13 = t8;
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v3));
  t16 = gg_function_64(t15, base + 0);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(32))));
  t19 = ((int64_t)(((t17) & (t18))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t19) != (t20));
  }
  t21 = t14;
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  v4 = gg_function_14(t22, t23, base + 0);
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = gg_function_29(t24, t25, base + 0);
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = gg_function_29(t26, t27, base + 0);
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(((int64_t)UINT64_C(2))));
  v7 = gg_function_29(t28, t29, base + 0);
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(((int64_t)UINT64_C(3))));
  v8 = gg_function_29(t30, t31, base + 0);
  t32 = ((int64_t)(v5));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((int64_t)(v6));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  t36 = ((int64_t)(v3));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  v10 = gg_function_19(t36, t37, base + 0);
  v11 = ((int64_t)(v7));
  t38 = ((int64_t)(v10));
  t39 = ((int64_t)(v11));
  t40 = ((t38) < (t39));
  if (t40) {
  {
  v11 = ((int64_t)(v10));
  }
  }
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(v6));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(v7));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t47 = ((int64_t)(v8));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = fg14_s_ggml_row_size(t41, t48);
  v12 = ((int64_t)(t49));
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(v2));
  t52 = gg_function_9(t50, t51, base + 0);
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  v13 = gg_function_14(t52, t53, base + 0);
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(v2));
  t56 = gg_function_9(t54, t55, base + 0);
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = gg_function_14(t56, t57, base + 0);
  t58 = ((int64_t)(v13));
  t59 = gg_function_12(t58, base + 0);
  t60 = ((int64_t)(t59));
  t61 = ((int64_t)(((int64_t)UINT64_C(37))));
  t62 = ((t60) != (t61));
  t63 = t62;
  if (!t63) {
  t64 = ((int64_t)(v13));
  t65 = gg_function_53(t64, base + 0);
  t66 = ((int64_t)(t65));
  t67 = ((int64_t)(v3));
  t63 = ((t66) != (t67));
  }
  t68 = t63;
  t69 = t68;
  if (!t69) {
  t70 = ((int64_t)(v13));
  t71 = gg_function_60(t70, base + 0);
  t72 = ((int64_t)(t71));
  t73 = ((int64_t)(v12));
  t69 = ((t72) != (t73));
  }
  t74 = t69;
  t75 = t74;
  if (!t75) {
  t76 = ((int64_t)(v13));
  t77 = ((int64_t)(((int64_t)UINT64_C(12))));
  t78 = fg14_s_contig(t76, t77);
  t79 = ((int64_t)(t78));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((t79) == (t80));
  }
  t81 = t75;
  if (t81) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t82 = ((int64_t)(v14));
  t83 = gg_function_12(t82, base + 0);
  t84 = ((int64_t)(t83));
  t85 = ((int64_t)(((int64_t)UINT64_C(37))));
  t86 = ((t84) != (t85));
  t87 = t86;
  if (!t87) {
  t88 = ((int64_t)(v14));
  t89 = gg_function_16(t88, base + 0);
  t90 = ((int64_t)(t89));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = ((t90) != (t91));
  }
  t92 = t87;
  t93 = t92;
  if (!t93) {
  t94 = ((int64_t)(v14));
  t95 = gg_function_23(t94, base + 0);
  t96 = ((int64_t)(t95));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t96) == (t97));
  }
  t98 = t93;
  t99 = t98;
  if (!t99) {
  t100 = ((int64_t)(v14));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = gg_function_29(t100, t101, base + 0);
  t103 = ((int64_t)(t102));
  t104 = ((int64_t)(v9));
  t99 = ((t103) != (t104));
  }
  t105 = t99;
  t106 = t105;
  if (!t106) {
  t107 = ((int64_t)(v14));
  t108 = ((int64_t)(((int64_t)UINT64_C(1))));
  t109 = gg_function_29(t107, t108, base + 0);
  t110 = ((int64_t)(t109));
  t111 = ((int64_t)(v8));
  t106 = ((t110) != (t111));
  }
  t112 = t106;
  t113 = t112;
  if (!t113) {
  t114 = ((int64_t)(v14));
  t115 = ((int64_t)(((int64_t)UINT64_C(2))));
  t116 = gg_function_29(t114, t115, base + 0);
  t117 = ((int64_t)(t116));
  t118 = ((int64_t)(v11));
  t113 = ((t117) != (t118));
  }
  t119 = t113;
  t120 = t119;
  if (!t120) {
  t121 = ((int64_t)(v14));
  t122 = ((int64_t)(((int64_t)UINT64_C(3))));
  t123 = gg_function_29(t121, t122, base + 0);
  t124 = ((int64_t)(t123));
  t125 = ((int64_t)(((int64_t)UINT64_C(1))));
  t120 = ((t124) != (t125));
  }
  t126 = t120;
  if (t126) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t127 = ((int64_t)(v14));
  t128 = ((int64_t)(((int64_t)UINT64_C(0))));
  t129 = gg_function_48(t127, t128, base + 0);
  t130 = ((int64_t)(t129));
  t131 = ((int64_t)(((int64_t)UINT64_C(4))));
  t132 = ((t130) != (t131));
  t133 = t132;
  if (!t133) {
  t134 = ((int64_t)(v14));
  t135 = ((int64_t)(((int64_t)UINT64_C(1))));
  t136 = gg_function_48(t134, t135, base + 0);
  t137 = ((int64_t)(t136));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((int64_t)(v9));
  t140 = fg14_s_ggml_row_size(t138, t139);
  t141 = ((int64_t)(t140));
  t133 = ((t137) != (t141));
  }
  t142 = t133;
  if (t142) {
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
static int64_t gg_function_59(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  uint8_t t13;
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
  uint8_t t25;
  uint8_t t26;
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
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  uint8_t t57;
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
  uint8_t t68;
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
  uint8_t t81;
  int64_t t82;
  int64_t t83;
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
  t2 = ((int64_t)(v1));
  v2 = gg_function_9(t1, t2, base + 0);
  t3 = ((int64_t)(v2));
  t4 = gg_function_12(t3, base + 0);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(7))));
  t7 = ((t5) != (t6));
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v2));
  t10 = gg_function_16(t9, base + 0);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t11) != (t12));
  }
  t13 = t8;
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(12))));
  t17 = fg14_s_contig(t15, t16);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t18) == (t19));
  }
  t20 = t14;
  if (t20) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = gg_function_29(t21, t22, base + 0);
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(((int64_t)UINT64_C(2))));
  t25 = ((t23) < (t24));
  t26 = t25;
  if (!t26) {
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(15))));
  t26 = ((t27) > (t28));
  }
  t29 = t26;
  if (t29) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = ((int64_t)(v3));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = ((int64_t)(v0));
  t36 = fg14_s_g_n(t35);
  t37 = ((int64_t)(t36));
  t38 = ((t34) > (t37));
  if (t38) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v5 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t39 = ((int64_t)(v5));
  t40 = ((int64_t)(v3));
  if (!(((t39) < (t40)))) goto label2;
  {
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(v1));
  t43 = ((int64_t)(v5));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  v6 = gg_function_9(t41, t44, base + 0);
  t45 = ((int64_t)(v6));
  t46 = gg_function_12(t45, base + 0);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)UINT64_C(37))));
  t49 = ((t47) != (t48));
  t50 = t49;
  if (!t50) {
  t51 = ((int64_t)(v6));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = gg_function_14(t51, t52, base + 0);
  t54 = ((int64_t)(t53));
  t55 = ((int64_t)(v2));
  t50 = ((t54) != (t55));
  }
  t56 = t50;
  t57 = t56;
  if (!t57) {
  t58 = ((int64_t)(v6));
  t59 = gg_function_53(t58, base + 0);
  t60 = ((int64_t)(t59));
  t61 = ((int64_t)(v2));
  t57 = ((t60) != (t61));
  }
  t62 = t57;
  t63 = t62;
  if (!t63) {
  t64 = ((int64_t)(v6));
  t65 = gg_function_16(t64, base + 0);
  t66 = ((int64_t)(t65));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((t66) != (t67));
  }
  t68 = t63;
  t69 = t68;
  if (!t69) {
  t70 = ((int64_t)(v6));
  t71 = gg_function_60(t70, base + 0);
  t72 = ((int64_t)(t71));
  t73 = ((int64_t)(v5));
  t74 = ((int64_t)(((int64_t)UINT64_C(1))));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) - (uint64_t)(t74)))));
  t76 = ((int64_t)(v2));
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  t78 = gg_function_48(t76, t77, base + 0);
  t79 = ((int64_t)(t78));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t75) * (uint64_t)(t79)))));
  t69 = ((t72) != (t80));
  }
  t81 = t69;
  if (t81) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t82 = v5;
  t83 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t82) + (uint64_t)(t83)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_37(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
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
  uint8_t t17;
  int64_t t18;
  int64_t t19;
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
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
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
  int64_t t114;
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
  uint8_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  uint8_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  uint8_t t142;
  uint8_t t143;
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
  uint8_t t166;
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
  uint8_t t189;
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
  int64_t t202;
  int64_t t203;
  uint8_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  uint8_t t209;
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
  int64_t t252;
  int64_t t253;
  int64_t t254;
  uint8_t t255;
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
  uint8_t t273;
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
  uint8_t t289;
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
  uint8_t t305;
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
  uint8_t t322;
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
  uint8_t t339;
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
  uint8_t t350;
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
  uint8_t t366;
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
  int64_t t383;
  uint8_t t384;
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
  uint8_t t401;
  int64_t t402;
  int64_t t403;
  int64_t t404;
  int64_t t405;
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
  t1 = fg14_s_fusion_disabled();
  t2 = ((int64_t)(t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) != (t3));
  if (t4) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v2));
  v3 = gg_function_9(t5, t6, base + 0);
  t7 = ((int64_t)(v3));
  t8 = gg_function_12(t7, base + 0);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(30))));
  t11 = ((t9) == (t10));
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v0));
  t14 = fg14_res_stream_ctx_size(t13);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t15) == (t16));
  }
  t17 = t12;
  if (t17) {
  {
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(((int64_t)UINT64_C(3))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t21)))));
  t23 = gg_function_40(t18, t19, t22, base + 0);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t24) == (t25));
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t32 = gg_function_9(t28, t31, base + 0);
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = gg_function_14(t32, t33, base + 0);
  t35 = gg_function_49(t34, base + 0);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((t36) == (t37));
  }
  t38 = t27;
  if (t38) {
  {
  t39 = ((int64_t)(v1));
  t40 = ((int64_t)(v2));
  t41 = ((int64_t)(((int64_t)UINT64_C(6))));
  t42 = ((int64_t)(v2));
  t43 = ((int64_t)(((int64_t)UINT64_C(5))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = gg_function_20(t39, t40, t41, t44, t45, base + 0);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)UINT64_C(1))));
  t49 = ((t47) == (t48));
  if (t49) {
  {
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(v2));
  t53 = ((int64_t)(v2));
  t54 = ((int64_t)(((int64_t)UINT64_C(2))));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t54)))));
  t56 = ((int64_t)(v2));
  t57 = ((int64_t)(((int64_t)UINT64_C(5))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  t59 = fg14_s_launch_shared_expert(t50, t51, t52, t55, t58);
  t0 = ((int64_t)(t59));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t60 = ((int64_t)(v3));
  t61 = gg_function_12(t60, base + 0);
  t62 = ((int64_t)(t61));
  t63 = ((int64_t)(((int64_t)UINT64_C(7))));
  t64 = ((t62) == (t63));
  if (t64) {
  {
  t65 = ((int64_t)(v1));
  t66 = ((int64_t)(v2));
  t67 = gg_function_59(t65, t66, base + 0);
  t68 = ((int64_t)(t67));
  t69 = ((int64_t)(((int64_t)UINT64_C(2))));
  t70 = ((t68) >= (t69));
  if (t70) {
  {
  t71 = ((int64_t)(v1));
  t72 = ((int64_t)(v2));
  v4 = gg_function_59(t71, t72, base + 0);
  t73 = ((int64_t)(v2));
  t74 = ((int64_t)(v4));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t74)))));
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t75) - (uint64_t)(t76)))));
  t77 = ((int64_t)(v1));
  t78 = ((int64_t)(v2));
  t79 = ((int64_t)(v4));
  t80 = ((int64_t)(v5));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = gg_function_20(t77, t78, t79, t80, t81, base + 0);
  t83 = ((int64_t)(t82));
  t84 = ((int64_t)(((int64_t)UINT64_C(1))));
  t85 = ((t83) == (t84));
  if (t85) {
  {
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(v1));
  t88 = ((int64_t)(v2));
  t89 = ((int64_t)(v4));
  t90 = ((int64_t)(v5));
  t91 = fg14_s_launch_weighted_reduction(t86, t87, t88, t89, t90);
  t0 = ((int64_t)(t91));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t92 = ((int64_t)(v3));
  t93 = gg_function_12(t92, base + 0);
  t94 = ((int64_t)(t93));
  t95 = ((int64_t)(((int64_t)UINT64_C(86))));
  t96 = ((t94) == (t95));
  if (t96) {
  {
  t97 = ((int64_t)(v1));
  t98 = ((int64_t)(v2));
  v6 = gg_function_62(t97, t98, base + 0);
  t99 = ((int64_t)(v6));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = ((t99) > (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)(v1));
  t104 = ((int64_t)(v2));
  t105 = ((int64_t)(v6));
  t106 = gg_function_65(t103, t104, t105, base + 0);
  t107 = ((int64_t)(t106));
  t108 = ((int64_t)(((int64_t)UINT64_C(1))));
  t102 = ((t107) == (t108));
  }
  t109 = t102;
  if (t109) {
  {
  t110 = ((int64_t)(v0));
  t111 = ((int64_t)(v1));
  t112 = ((int64_t)(v2));
  t113 = ((int64_t)(v6));
  t114 = fg14_s_launch_gdn_cache(t110, t111, t112, t113);
  t0 = ((int64_t)(t114));
  goto gg_done;
  }
  }
  }
  }
  t115 = ((int64_t)(v3));
  t116 = gg_function_12(t115, base + 0);
  t117 = ((int64_t)(t116));
  t118 = ((int64_t)(((int64_t)UINT64_C(77))));
  t119 = ((t117) == (t118));
  if (t119) {
  {
  t120 = ((int64_t)(v1));
  t121 = ((int64_t)(v2));
  v7 = gg_function_62(t120, t121, base + 0);
  t122 = ((int64_t)(v7));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = ((t122) > (t123));
  t125 = t124;
  if (t125) {
  t126 = ((int64_t)(v1));
  t127 = ((int64_t)(v2));
  t128 = ((int64_t)(v7));
  t129 = gg_function_68(t126, t127, t128, base + 0);
  t130 = ((int64_t)(t129));
  t131 = ((int64_t)(((int64_t)UINT64_C(1))));
  t125 = ((t130) == (t131));
  }
  t132 = t125;
  if (t132) {
  {
  t133 = ((int64_t)(v0));
  t134 = ((int64_t)(v1));
  t135 = ((int64_t)(v2));
  t136 = ((int64_t)(v7));
  t137 = fg14_s_launch_ssm_cache(t133, t134, t135, t136);
  t0 = ((int64_t)(t137));
  goto gg_done;
  }
  }
  }
  }
  t138 = ((int64_t)(v3));
  t139 = gg_function_12(t138, base + 0);
  t140 = ((int64_t)(t139));
  t141 = ((int64_t)(((int64_t)UINT64_C(91))));
  t142 = ((t140) == (t141));
  t143 = t142;
  if (!t143) {
  t144 = ((int64_t)(v3));
  t145 = gg_function_12(t144, base + 0);
  t146 = ((int64_t)(t145));
  t147 = ((int64_t)(((int64_t)UINT64_C(46))));
  t143 = ((t146) == (t147));
  }
  t148 = t143;
  t149 = t148;
  if (!t149) {
  t150 = ((int64_t)(v3));
  t151 = gg_function_12(t150, base + 0);
  t152 = ((int64_t)(t151));
  t153 = ((int64_t)(((int64_t)UINT64_C(69))));
  t149 = ((t152) == (t153));
  }
  t154 = t149;
  if (t154) {
  {
  t155 = ((int64_t)(v1));
  t156 = ((int64_t)(v2));
  v8 = gg_function_70(t155, t156, base + 0);
  t157 = ((int64_t)(v8));
  t158 = ((int64_t)(((int64_t)UINT64_C(0))));
  t159 = ((t157) > (t158));
  if (t159) {
  {
  t160 = ((int64_t)(v0));
  t161 = ((int64_t)(v1));
  t162 = ((int64_t)(v2));
  t163 = ((int64_t)(v8));
  v9 = gg_function_77(t160, t161, t162, t163, base + 0);
  t164 = ((int64_t)(v9));
  t165 = ((int64_t)(((int64_t)UINT64_C(0))));
  t166 = ((t164) != (t165));
  if (t166) {
  {
  t0 = ((int64_t)(v9));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t167 = ((int64_t)(((int64_t)UINT64_C(48))));
  t168 = ((int64_t)(((int64_t)UINT64_C(37))));
  t169 = ((int64_t)(((int64_t)UINT64_C(42))));
  v10 = gg_function_84(t167, t168, t169, base + 0);
  t170 = ((int64_t)(v1));
  t171 = ((int64_t)(v2));
  t172 = ((int64_t)(v10));
  t173 = fg14_s_vec_data(t172);
  t174 = ((int64_t)(t173));
  t175 = ((int64_t)(((int64_t)UINT64_C(3))));
  t176 = fg14_s_g_can_fuse(t170, t171, t174, t175);
  t177 = ((int64_t)(t176));
  t178 = ((int64_t)(((int64_t)UINT64_C(1))));
  t179 = ((t177) == (t178));
  if (t179) {
  {
  t180 = ((int64_t)(v0));
  t181 = ((int64_t)(v1));
  t182 = ((int64_t)(v2));
  t183 = fg14_s_launch_rope_set_rows(t180, t181, t182);
  t0 = ((int64_t)(t183));
  goto gg_done;
  }
  }
  t184 = ((int64_t)(v1));
  t185 = ((int64_t)(v2));
  t186 = gg_function_86(t184, t185, base + 0);
  t187 = ((int64_t)(t186));
  t188 = ((int64_t)(((int64_t)UINT64_C(1))));
  t189 = ((t187) == (t188));
  if (t189) {
  {
  t190 = ((int64_t)(v0));
  t191 = ((int64_t)(v1));
  t192 = ((int64_t)(v2));
  t193 = fg14_s_launch_snake(t190, t191, t192);
  t0 = ((int64_t)(t193));
  goto gg_done;
  }
  }
  t194 = ((int64_t)(v3));
  t195 = gg_function_12(t194, base + 0);
  t196 = ((int64_t)(t195));
  t197 = ((int64_t)(((int64_t)UINT64_C(2))));
  t198 = ((t196) == (t197));
  t199 = t198;
  if (!t199) {
  t200 = ((int64_t)(v3));
  t201 = gg_function_12(t200, base + 0);
  t202 = ((int64_t)(t201));
  t203 = ((int64_t)(((int64_t)UINT64_C(7))));
  t199 = ((t202) == (t203));
  }
  t204 = t199;
  if (t204) {
  {
  t205 = ((int64_t)(v1));
  t206 = ((int64_t)(v2));
  v11 = gg_function_89(t205, t206, base + 0);
  t207 = ((int64_t)(v11));
  t208 = ((int64_t)(((int64_t)UINT64_C(1))));
  t209 = ((t207) > (t208));
  if (t209) {
  {
  t210 = ((int64_t)(v0));
  t211 = ((int64_t)(v1));
  t212 = ((int64_t)(v2));
  t213 = ((int64_t)(v11));
  t214 = ((int64_t)(v3));
  t215 = gg_function_12(t214, base + 0);
  t216 = fg14_s_launch_multi(t210, t211, t212, t213, t215);
  t0 = ((int64_t)(t216));
  goto gg_done;
  }
  }
  }
  }
  t217 = ((int64_t)(v0));
  t218 = ((int64_t)(v1));
  t219 = ((int64_t)(v2));
  v12 = gg_function_92(t217, t218, t219, base + 0);
  t220 = ((int64_t)(v12));
  t221 = ((int64_t)(((int64_t)UINT64_C(0))));
  t222 = ((t220) != (t221));
  if (t222) {
  {
  t0 = ((int64_t)(v12));
  goto gg_done;
  }
  }
  t223 = ((int64_t)(v1));
  t224 = ((int64_t)(v2));
  t225 = ((int64_t)(((int64_t)UINT64_C(44))));
  t226 = ((int64_t)(((int64_t)UINT64_C(7))));
  t227 = ((int64_t)(((int64_t)UINT64_C(48))));
  t228 = ((int64_t)(((int64_t)UINT64_C(37))));
  t229 = ((int64_t)(((int64_t)UINT64_C(42))));
  t230 = gg_function_87(t225, t226, t227, t228, t229, base + 0);
  t231 = fg14_s_vec_data(t230);
  t232 = ((int64_t)(t231));
  t233 = ((int64_t)(((int64_t)UINT64_C(5))));
  t234 = fg14_s_g_can_fuse(t223, t224, t232, t233);
  t235 = ((int64_t)(t234));
  t236 = ((int64_t)(((int64_t)UINT64_C(1))));
  t237 = ((t235) == (t236));
  if (t237) {
  {
  t238 = ((int64_t)(v0));
  t239 = ((int64_t)(v1));
  t240 = ((int64_t)(v2));
  t241 = ((int64_t)(((int64_t)UINT64_C(1))));
  t242 = fg14_s_launch_rms_rope(t238, t239, t240, t241);
  t0 = ((int64_t)(t242));
  goto gg_done;
  }
  }
  t243 = ((int64_t)(v1));
  t244 = ((int64_t)(v2));
  t245 = ((int64_t)(((int64_t)UINT64_C(44))));
  t246 = ((int64_t)(((int64_t)UINT64_C(7))));
  t247 = ((int64_t)(((int64_t)UINT64_C(48))));
  t248 = gg_function_84(t245, t246, t247, base + 0);
  t249 = fg14_s_vec_data(t248);
  t250 = ((int64_t)(t249));
  t251 = ((int64_t)(((int64_t)UINT64_C(3))));
  t252 = fg14_s_g_can_fuse(t243, t244, t250, t251);
  t253 = ((int64_t)(t252));
  t254 = ((int64_t)(((int64_t)UINT64_C(1))));
  t255 = ((t253) == (t254));
  if (t255) {
  {
  t256 = ((int64_t)(v0));
  t257 = ((int64_t)(v1));
  t258 = ((int64_t)(v2));
  t259 = ((int64_t)(((int64_t)UINT64_C(0))));
  t260 = fg14_s_launch_rms_rope(t256, t257, t258, t259);
  t0 = ((int64_t)(t260));
  goto gg_done;
  }
  }
  t261 = ((int64_t)(v1));
  t262 = ((int64_t)(v2));
  t263 = ((int64_t)(((int64_t)UINT64_C(44))));
  t264 = ((int64_t)(((int64_t)UINT64_C(7))));
  t265 = ((int64_t)(((int64_t)UINT64_C(2))));
  t266 = gg_function_84(t263, t264, t265, base + 0);
  t267 = fg14_s_vec_data(t266);
  t268 = ((int64_t)(t267));
  t269 = ((int64_t)(((int64_t)UINT64_C(3))));
  t270 = fg14_s_g_can_fuse(t261, t262, t268, t269);
  t271 = ((int64_t)(t270));
  t272 = ((int64_t)(((int64_t)UINT64_C(1))));
  t273 = ((t271) == (t272));
  if (t273) {
  {
  t274 = ((int64_t)(v0));
  t275 = ((int64_t)(v1));
  t276 = ((int64_t)(v2));
  t277 = fg14_s_launch_rms_add(t274, t275, t276);
  t0 = ((int64_t)(t277));
  goto gg_done;
  }
  }
  t278 = ((int64_t)(v1));
  t279 = ((int64_t)(v2));
  t280 = ((int64_t)(((int64_t)UINT64_C(44))));
  t281 = ((int64_t)(((int64_t)UINT64_C(7))));
  t282 = gg_function_93(t280, t281, base + 0);
  t283 = fg14_s_vec_data(t282);
  t284 = ((int64_t)(t283));
  t285 = ((int64_t)(((int64_t)UINT64_C(2))));
  t286 = fg14_s_g_can_fuse(t278, t279, t284, t285);
  t287 = ((int64_t)(t286));
  t288 = ((int64_t)(((int64_t)UINT64_C(1))));
  t289 = ((t287) == (t288));
  if (t289) {
  {
  t290 = ((int64_t)(v0));
  t291 = ((int64_t)(v1));
  t292 = ((int64_t)(v2));
  t293 = fg14_s_launch_rms_mul(t290, t291, t292);
  t0 = ((int64_t)(t293));
  goto gg_done;
  }
  }
  t294 = ((int64_t)(v1));
  t295 = ((int64_t)(v2));
  t296 = ((int64_t)(((int64_t)UINT64_C(44))));
  t297 = ((int64_t)(((int64_t)UINT64_C(32))));
  t298 = gg_function_93(t296, t297, base + 0);
  t299 = fg14_s_vec_data(t298);
  t300 = ((int64_t)(t299));
  t301 = ((int64_t)(((int64_t)UINT64_C(2))));
  t302 = fg14_s_g_can_fuse(t294, t295, t300, t301);
  t303 = ((int64_t)(t302));
  t304 = ((int64_t)(((int64_t)UINT64_C(1))));
  t305 = ((t303) == (t304));
  if (t305) {
  {
  t306 = ((int64_t)(v0));
  t307 = ((int64_t)(v1));
  t308 = ((int64_t)(v2));
  t309 = fg14_s_launch_rms_scale(t306, t307, t308);
  t0 = ((int64_t)(t309));
  goto gg_done;
  }
  }
  t310 = ((int64_t)(v1));
  t311 = ((int64_t)(v2));
  t312 = ((int64_t)(((int64_t)UINT64_C(76))));
  t313 = ((int64_t)(((int64_t)UINT64_C(2))));
  t314 = ((int64_t)(((int64_t)UINT64_C(91))));
  t315 = gg_function_84(t312, t313, t314, base + 0);
  t316 = fg14_s_vec_data(t315);
  t317 = ((int64_t)(t316));
  t318 = ((int64_t)(((int64_t)UINT64_C(3))));
  t319 = fg14_s_g_can_fuse(t310, t311, t317, t318);
  t320 = ((int64_t)(t319));
  t321 = ((int64_t)(((int64_t)UINT64_C(1))));
  t322 = ((t320) == (t321));
  if (t322) {
  {
  t323 = ((int64_t)(v0));
  t324 = ((int64_t)(v1));
  t325 = ((int64_t)(v2));
  t326 = ((int64_t)(((int64_t)UINT64_C(1))));
  t327 = fg14_s_launch_ssm_conv(t323, t324, t325, t326);
  t0 = ((int64_t)(t327));
  goto gg_done;
  }
  }
  t328 = ((int64_t)(v1));
  t329 = ((int64_t)(v2));
  t330 = ((int64_t)(((int64_t)UINT64_C(76))));
  t331 = ((int64_t)(((int64_t)UINT64_C(91))));
  t332 = gg_function_93(t330, t331, base + 0);
  t333 = fg14_s_vec_data(t332);
  t334 = ((int64_t)(t333));
  t335 = ((int64_t)(((int64_t)UINT64_C(2))));
  t336 = fg14_s_g_can_fuse(t328, t329, t334, t335);
  t337 = ((int64_t)(t336));
  t338 = ((int64_t)(((int64_t)UINT64_C(1))));
  t339 = ((t337) == (t338));
  if (t339) {
  {
  t340 = ((int64_t)(v0));
  t341 = ((int64_t)(v1));
  t342 = ((int64_t)(v2));
  t343 = ((int64_t)(((int64_t)UINT64_C(0))));
  t344 = fg14_s_launch_ssm_conv(t340, t341, t342, t343);
  t0 = ((int64_t)(t344));
  goto gg_done;
  }
  }
  t345 = ((int64_t)(v1));
  t346 = ((int64_t)(v2));
  t347 = gg_function_106(t345, t346, base + 0);
  t348 = ((int64_t)(t347));
  t349 = ((int64_t)(((int64_t)UINT64_C(1))));
  t350 = ((t348) == (t349));
  if (t350) {
  {
  t351 = ((int64_t)(v0));
  t352 = ((int64_t)(v1));
  t353 = ((int64_t)(v2));
  t354 = fg14_s_launch_unary_mul(t351, t352, t353);
  t0 = ((int64_t)(t354));
  goto gg_done;
  }
  }
  t355 = ((int64_t)(v1));
  t356 = ((int64_t)(v2));
  t357 = ((int64_t)(((int64_t)UINT64_C(91))));
  t358 = ((int64_t)(((int64_t)UINT64_C(9))));
  t359 = gg_function_93(t357, t358, base + 0);
  t360 = fg14_s_vec_data(t359);
  t361 = ((int64_t)(t360));
  t362 = ((int64_t)(((int64_t)UINT64_C(2))));
  t363 = fg14_s_g_can_fuse(t355, t356, t361, t362);
  t364 = ((int64_t)(t363));
  t365 = ((int64_t)(((int64_t)UINT64_C(1))));
  t366 = ((t364) == (t365));
  if (t366) {
  {
  t367 = ((int64_t)(v0));
  t368 = ((int64_t)(v1));
  t369 = ((int64_t)(v2));
  t370 = fg14_s_launch_relu_sqr(t367, t368, t369);
  t0 = ((int64_t)(t370));
  goto gg_done;
  }
  }
  t371 = ((int64_t)(v1));
  t372 = ((int64_t)(v2));
  t373 = ((int64_t)(((int64_t)UINT64_C(32))));
  t374 = ((int64_t)(((int64_t)UINT64_C(91))));
  t375 = ((int64_t)(((int64_t)UINT64_C(32))));
  t376 = ((int64_t)(((int64_t)UINT64_C(90))));
  t377 = gg_function_109(t373, t374, t375, t376, base + 0);
  t378 = fg14_s_vec_data(t377);
  t379 = ((int64_t)(t378));
  t380 = ((int64_t)(((int64_t)UINT64_C(4))));
  t381 = fg14_s_g_can_fuse(t371, t372, t379, t380);
  t382 = ((int64_t)(t381));
  t383 = ((int64_t)(((int64_t)UINT64_C(1))));
  t384 = ((t382) == (t383));
  if (t384) {
  {
  t385 = ((int64_t)(v0));
  t386 = ((int64_t)(v1));
  t387 = ((int64_t)(v2));
  t388 = fg14_s_launch_dsv4_hc_post(t385, t386, t387);
  t0 = ((int64_t)(t388));
  goto gg_done;
  }
  }
  t389 = ((int64_t)(v1));
  t390 = ((int64_t)(v2));
  t391 = ((int64_t)(((int64_t)UINT64_C(32))));
  t392 = ((int64_t)(((int64_t)UINT64_C(91))));
  t393 = ((int64_t)(((int64_t)UINT64_C(32))));
  t394 = gg_function_84(t391, t392, t393, base + 0);
  t395 = fg14_s_vec_data(t394);
  t396 = ((int64_t)(t395));
  t397 = ((int64_t)(((int64_t)UINT64_C(3))));
  t398 = fg14_s_g_can_fuse(t389, t390, t396, t397);
  t399 = ((int64_t)(t398));
  t400 = ((int64_t)(((int64_t)UINT64_C(1))));
  t401 = ((t399) == (t400));
  if (t401) {
  {
  t402 = ((int64_t)(v0));
  t403 = ((int64_t)(v1));
  t404 = ((int64_t)(v2));
  t405 = fg14_s_launch_softcap(t402, t403, t404);
  t0 = ((int64_t)(t405));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_34(int64_t p0, int64_t p1, int64_t base) {
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
  t3 = fg14_s_frame_get(t1, t2);
  t0 = ((int64_t)(t3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_6(int64_t p0, int64_t p1, int64_t base) {
  /* FG_SHARED_BODY:34 */
  return gg_function_34(p0, p1, base);
}
static int64_t gg_function_35(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_12(t1, base + 0);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(91))));
  t5 = ((t3) == (t4));
  t6 = t5;
  if (t6) {
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = gg_function_19(t7, t8, base + 0);
  t10 = ((int64_t)(t9));
  t11 = ((int64_t)(v1));
  t6 = ((t10) == (t11));
  }
  t12 = t6;
  if (t12) {
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
static int64_t gg_function_30(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
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
  uint8_t t17;
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
  uint8_t t30;
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
  int64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
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
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_12(t1, base + 0);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(48))));
  t5 = ((t3) != (t4));
  t6 = t5;
  if (!t6) {
  t7 = ((int64_t)(v1));
  t8 = gg_function_12(t7, base + 0);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(37))));
  t6 = ((t9) != (t10));
  }
  t11 = t6;
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v2));
  t14 = gg_function_12(t13, base + 0);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(((int64_t)UINT64_C(42))));
  t12 = ((t15) != (t16));
  }
  t17 = t12;
  if (t17) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = gg_function_14(t18, t19, base + 0);
  t21 = ((int64_t)(((int64_t)UINT64_C(3))));
  t22 = gg_function_29(t20, t21, base + 0);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) != (t24));
  if (t25) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t26 = ((int64_t)(v2));
  t27 = gg_function_16(t26, base + 0);
  t28 = ((int64_t)(t27));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) != (t29));
  t31 = t30;
  if (t31) {
  t32 = ((int64_t)(v2));
  t33 = gg_function_16(t32, base + 0);
  t34 = ((int64_t)(t33));
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((t34) != (t35));
  }
  t36 = t31;
  if (t36) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t37 = ((int64_t)(v2));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t39 = gg_function_14(t37, t38, base + 0);
  t40 = gg_function_16(t39, base + 0);
  t41 = ((int64_t)(t40));
  t42 = ((int64_t)(((int64_t)UINT64_C(27))));
  t43 = ((t41) != (t42));
  if (t43) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(((int64_t)UINT64_C(12))));
  t46 = fg14_s_contig(t44, t45);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((t47) == (t48));
  t50 = t49;
  if (!t50) {
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = gg_function_29(t51, t52, base + 0);
  t54 = ((int64_t)(t53));
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = gg_function_29(t55, t56, base + 0);
  t58 = ((int64_t)(t57));
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  t61 = gg_function_29(t59, t60, base + 0);
  t62 = ((int64_t)(t61));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t62)))));
  t50 = ((t54) != (t63));
  }
  t64 = t50;
  if (t64) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t65 = ((int64_t)(v0));
  t66 = ((int64_t)(((int64_t)UINT64_C(2))));
  v3 = gg_function_19(t65, t66, base + 0);
  t67 = ((int64_t)(v3));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((t67) != (t68));
  t70 = t69;
  if (t70) {
  t71 = ((int64_t)(v3));
  t72 = ((int64_t)(((int64_t)UINT64_C(2))));
  t70 = ((t71) != (t72));
  }
  t73 = t70;
  if (t73) {
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
static int64_t gg_function_27(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
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
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  uint8_t t25;
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
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  uint8_t t53;
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
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
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
  int64_t t78;
  uint8_t t79;
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
  uint8_t t93;
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  uint8_t t102;
  uint8_t t103;
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
  uint8_t t116;
  uint8_t t117;
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
  uint8_t t128;
  uint8_t t129;
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
  uint8_t t145;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_12(t1, base + 0);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(44))));
  t5 = ((t3) != (t4));
  t6 = t5;
  if (!t6) {
  t7 = ((int64_t)(v1));
  t8 = gg_function_12(t7, base + 0);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(7))));
  t6 = ((t9) != (t10));
  }
  t11 = t6;
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v2));
  t14 = gg_function_12(t13, base + 0);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(((int64_t)UINT64_C(48))));
  t12 = ((t15) != (t16));
  }
  t17 = t12;
  if (t17) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = gg_function_14(t18, t19, base + 0);
  t21 = gg_function_16(t20, base + 0);
  t22 = ((int64_t)(t21));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t22) != (t23));
  t25 = t24;
  if (!t25) {
  t26 = ((int64_t)(v0));
  t27 = gg_function_16(t26, base + 0);
  t28 = ((int64_t)(t27));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t28) != (t29));
  }
  t30 = t25;
  t31 = t30;
  if (!t31) {
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = gg_function_14(t32, t33, base + 0);
  t35 = gg_function_16(t34, base + 0);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t36) != (t37));
  }
  t38 = t31;
  t39 = t38;
  if (!t39) {
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  t42 = gg_function_14(t40, t41, base + 0);
  t43 = gg_function_16(t42, base + 0);
  t44 = ((int64_t)(t43));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((t44) != (t45));
  }
  t46 = t39;
  t47 = t46;
  if (!t47) {
  t48 = ((int64_t)(v1));
  t49 = gg_function_16(t48, base + 0);
  t50 = ((int64_t)(t49));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((t50) != (t51));
  }
  t52 = t47;
  t53 = t52;
  if (!t53) {
  t54 = ((int64_t)(v2));
  t55 = gg_function_16(t54, base + 0);
  t56 = ((int64_t)(t55));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((t56) != (t57));
  }
  t58 = t53;
  if (t58) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t59 = ((int64_t)(v2));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = gg_function_14(t59, t60, base + 0);
  t62 = ((int64_t)(t61));
  t63 = ((int64_t)(v1));
  t64 = ((t62) != (t63));
  if (t64) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t65 = ((int64_t)(v0));
  t66 = ((int64_t)(v1));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  t68 = gg_function_14(t66, t67, base + 0);
  t69 = ((int64_t)(t68));
  t70 = ((t65) == (t69));
  t71 = t70;
  if (t71) {
  t72 = ((int64_t)(v1));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = gg_function_14(t72, t73, base + 0);
  t75 = ((int64_t)(v0));
  t76 = fg14_s_t_same_shape(t74, t75);
  t77 = ((int64_t)(t76));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((t77) == (t78));
  }
  t79 = t71;
  if (t79) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(v1));
  t82 = fg14_s_t_same_shape(t80, t81);
  t83 = ((int64_t)(t82));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((t83) == (t84));
  if (t85) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = gg_function_14(t86, t87, base + 0);
  t89 = ((int64_t)(((int64_t)UINT64_C(15))));
  t90 = fg14_s_contig(t88, t89);
  t91 = ((int64_t)(t90));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t91) == (t92));
  t94 = t93;
  if (!t94) {
  t95 = ((int64_t)(v1));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = gg_function_14(t95, t96, base + 0);
  t98 = ((int64_t)(((int64_t)UINT64_C(15))));
  t99 = fg14_s_contig(t97, t98);
  t100 = ((int64_t)(t99));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((t100) == (t101));
  }
  t102 = t94;
  t103 = t102;
  if (!t103) {
  t104 = ((int64_t)(v1));
  t105 = ((int64_t)(((int64_t)UINT64_C(1))));
  t106 = gg_function_14(t104, t105, base + 0);
  t107 = ((int64_t)(((int64_t)UINT64_C(15))));
  t108 = fg14_s_contig(t106, t107);
  t109 = ((int64_t)(t108));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = ((t109) == (t110));
  }
  t111 = t103;
  if (t111) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t112 = ((int64_t)(v2));
  t113 = ((int64_t)(((int64_t)UINT64_C(2))));
  v3 = gg_function_19(t112, t113, base + 0);
  t114 = ((int64_t)(v3));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((t114) != (t115));
  t117 = t116;
  if (t117) {
  t118 = ((int64_t)(v3));
  t119 = ((int64_t)(((int64_t)UINT64_C(2))));
  t117 = ((t118) != (t119));
  }
  t120 = t117;
  if (t120) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t121 = ((int64_t)(v2));
  t122 = ((int64_t)(((int64_t)UINT64_C(1))));
  t123 = gg_function_19(t121, t122, base + 0);
  t124 = ((int64_t)(t123));
  t125 = ((int64_t)(((int64_t)UINT64_C(2))));
  t126 = ((int64_t)(((int64_t)gg_mod(t124, t125))));
  t127 = ((int64_t)(((int64_t)UINT64_C(0))));
  t128 = ((t126) != (t127));
  t129 = t128;
  if (!t129) {
  t130 = ((int64_t)(v2));
  t131 = ((int64_t)(((int64_t)UINT64_C(0))));
  t132 = gg_function_14(t130, t131, base + 0);
  t133 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = gg_function_29(t132, t133, base + 0);
  t135 = ((int64_t)(t134));
  t136 = ((int64_t)(((int64_t)UINT64_C(2))));
  t137 = ((int64_t)(((int64_t)gg_mod(t135, t136))));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t129 = ((t137) != (t138));
  }
  t139 = t129;
  if (t139) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t140 = ((int64_t)(v2));
  t141 = ((int64_t)(((int64_t)UINT64_C(15))));
  t142 = gg_function_19(t140, t141, base + 0);
  t143 = ((int64_t)(t142));
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t145 = ((t143) != (t144));
  if (t145) {
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
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = gg_function_34(t1, t2, base + 0);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(v1));
  t6 = ((t4) == (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = gg_function_34(t8, t9, base + 0);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(v2));
  t7 = ((t11) == (t12));
  }
  t13 = t7;
  t14 = t13;
  if (t14) {
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = gg_function_34(t15, t16, base + 0);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(v3));
  t14 = ((t18) == (t19));
  }
  t20 = t14;
  if (t20) {
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
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
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
  t5 = gg_function_5(t1, t2, t3, t4, base + 0);
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) != (t7));
  t9 = t8;
  if (t9) {
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(3))));
  t12 = gg_function_34(t10, t11, base + 0);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(v4));
  t9 = ((t13) == (t14));
  }
  t15 = t9;
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = gg_function_34(t17, t18, base + 0);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(v5));
  t16 = ((t20) == (t21));
  }
  t22 = t16;
  if (t22) {
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
static int64_t gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
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
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
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
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint8_t t91;
  uint8_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t t100;
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
  uint8_t t125;
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
  uint8_t t147;
  uint8_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  uint8_t t154;
  uint8_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
  uint8_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint8_t t168;
  uint8_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  uint8_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  uint8_t t182;
  uint8_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  uint8_t t191;
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
  uint8_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  uint8_t t228;
  uint8_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  uint8_t t238;
  uint8_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  uint8_t t248;
  uint8_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  uint8_t t256;
  uint8_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  uint8_t t264;
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
  uint8_t t275;
  int64_t t276;
  int64_t t277;
  uint8_t t278;
  uint8_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  uint8_t t285;
  uint8_t t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  uint8_t t292;
  uint8_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  uint8_t t299;
  uint8_t t300;
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
  uint8_t t325;
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
  uint8_t t336;
  int64_t t337;
  int64_t t338;
  uint8_t t339;
  uint8_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  uint8_t t346;
  uint8_t t347;
  int64_t t348;
  int64_t t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  uint8_t t353;
  uint8_t t354;
  int64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  int64_t t359;
  uint8_t t360;
  uint8_t t361;
  int64_t t362;
  int64_t t363;
  int64_t t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  uint8_t t369;
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
  uint8_t t386;
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
  uint8_t t397;
  int64_t t398;
  int64_t t399;
  uint8_t t400;
  uint8_t t401;
  int64_t t402;
  int64_t t403;
  int64_t t404;
  int64_t t405;
  int64_t t406;
  uint8_t t407;
  uint8_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  int64_t t413;
  uint8_t t414;
  uint8_t t415;
  int64_t t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  uint8_t t421;
  uint8_t t422;
  int64_t t423;
  int64_t t424;
  int64_t t425;
  int64_t t426;
  int64_t t427;
  uint8_t t428;
  uint8_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  int64_t t434;
  int64_t t435;
  int64_t t436;
  uint8_t t437;
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
  uint8_t t456;
  uint8_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  int64_t t461;
  int64_t t462;
  int64_t t463;
  int64_t t464;
  int64_t t465;
  uint8_t t466;
  uint8_t t467;
  int64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
  int64_t t472;
  int64_t t473;
  int64_t t474;
  int64_t t475;
  uint8_t t476;
  uint8_t t477;
  int64_t t478;
  int64_t t479;
  int64_t t480;
  int64_t t481;
  int64_t t482;
  uint8_t t483;
  uint8_t t484;
  int64_t t485;
  int64_t t486;
  int64_t t487;
  int64_t t488;
  int64_t t489;
  uint8_t t490;
  uint8_t t491;
  int64_t t492;
  int64_t t493;
  int64_t t494;
  int64_t t495;
  int64_t t496;
  uint8_t t497;
  uint8_t t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  int64_t t502;
  int64_t t503;
  int64_t t504;
  uint8_t t505;
  uint8_t t506;
  int64_t t507;
  int64_t t508;
  int64_t t509;
  int64_t t510;
  uint8_t t511;
  uint8_t t512;
  int64_t t513;
  int64_t t514;
  int64_t t515;
  int64_t t516;
  int64_t t517;
  int64_t t518;
  int64_t t519;
  uint8_t t520;
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
  int64_t t539;
  int64_t t540;
  int64_t t541;
  int64_t t542;
  uint8_t t543;
  int64_t t544;
  int64_t t545;
  uint8_t t546;
  uint8_t t547;
  int64_t t548;
  int64_t t549;
  uint8_t t550;
  int64_t t551;
  int64_t t552;
  int64_t t553;
  int64_t t554;
  int64_t t555;
  uint8_t t556;
  uint8_t t557;
  int64_t t558;
  int64_t t559;
  int64_t t560;
  int64_t t561;
  int64_t t562;
  uint8_t t563;
  int64_t t564;
  int64_t t565;
  int64_t t566;
  int64_t t567;
  int64_t t568;
  int64_t t569;
  int64_t t570;
  int64_t t571;
  uint8_t t572;
  uint8_t t573;
  int64_t t574;
  int64_t t575;
  int64_t t576;
  int64_t t577;
  int64_t t578;
  uint8_t t579;
  int64_t t580;
  int64_t t581;
  int64_t t582;
  int64_t t583;
  int64_t t584;
  int64_t t585;
  uint8_t t586;
  uint8_t t587;
  int64_t t588;
  int64_t t589;
  int64_t t590;
  int64_t t591;
  int64_t t592;
  int64_t t593;
  uint8_t t594;
  uint8_t t595;
  int64_t t596;
  int64_t t597;
  int64_t t598;
  int64_t t599;
  int64_t t600;
  int64_t t601;
  uint8_t t602;
  uint8_t t603;
  int64_t t604;
  int64_t t605;
  int64_t t606;
  int64_t t607;
  uint8_t t608;
  int64_t t609;
  int64_t t610;
  int64_t t611;
  int64_t t612;
  int64_t t613;
  int64_t t614;
  uint8_t t615;
  uint8_t t616;
  int64_t t617;
  int64_t t618;
  int64_t t619;
  int64_t t620;
  int64_t t621;
  int64_t t622;
  uint8_t t623;
  uint8_t t624;
  int64_t t625;
  int64_t t626;
  int64_t t627;
  int64_t t628;
  uint8_t t629;
  int64_t t630;
  int64_t t631;
  int64_t t632;
  int64_t t633;
  int64_t t634;
  uint8_t t635;
  uint8_t t636;
  int64_t t637;
  int64_t t638;
  int64_t t639;
  int64_t t640;
  int64_t t641;
  int64_t t642;
  int64_t t643;
  uint8_t t644;
  int64_t t645;
  int64_t t646;
  int64_t t647;
  int64_t t648;
  int64_t t649;
  int64_t t650;
  int64_t t651;
  uint8_t t652;
  uint8_t t653;
  int64_t t654;
  int64_t t655;
  int64_t t656;
  int64_t t657;
  int64_t t658;
  int64_t t659;
  int64_t t660;
  uint8_t t661;
  int64_t t662;
  int64_t t663;
  uint8_t t664;
  uint8_t t665;
  int64_t t666;
  int64_t t667;
  int64_t t668;
  int64_t t669;
  int64_t t670;
  int64_t t671;
  int64_t t672;
  uint8_t t673;
  uint8_t t674;
  int64_t t675;
  int64_t t676;
  int64_t t677;
  int64_t t678;
  int64_t t679;
  int64_t t680;
  int64_t t681;
  uint8_t t682;
  int64_t t683;
  int64_t t684;
  uint8_t t685;
  uint8_t t686;
  int64_t t687;
  int64_t t688;
  int64_t t689;
  int64_t t690;
  int64_t t691;
  uint8_t t692;
  uint8_t t693;
  int64_t t694;
  int64_t t695;
  int64_t t696;
  int64_t t697;
  int64_t t698;
  uint8_t t699;
  int64_t t700;
  int64_t t701;
  int64_t t702;
  int64_t t703;
  int64_t t704;
  int64_t t705;
  int64_t t706;
  int64_t t707;
  int64_t t708;
  uint8_t t709;
  uint8_t t710;
  int64_t t711;
  int64_t t712;
  int64_t t713;
  int64_t t714;
  uint8_t t715;
  int64_t t716;
  int64_t t717;
  uint8_t t718;
  uint8_t t719;
  int64_t t720;
  int64_t t721;
  int64_t t722;
  int64_t t723;
  int64_t t724;
  uint8_t t725;
  uint8_t t726;
  int64_t t727;
  int64_t t728;
  int64_t t729;
  int64_t t730;
  int64_t t731;
  uint8_t t732;
  uint8_t t733;
  int64_t t734;
  int64_t t735;
  uint8_t t736;
  uint8_t t737;
  int64_t t738;
  int64_t t739;
  int64_t t740;
  int64_t t741;
  int64_t t742;
  uint8_t t743;
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
  uint8_t t755;
  int64_t t756;
  int64_t t757;
  int64_t t758;
  int64_t t759;
  uint8_t t760;
  uint8_t t761;
  int64_t t762;
  int64_t t763;
  int64_t t764;
  int64_t t765;
  uint8_t t766;
  int64_t t767;
  int64_t t768;
  uint8_t t769;
  uint8_t t770;
  int64_t t771;
  int64_t t772;
  int64_t t773;
  int64_t t774;
  int64_t t775;
  uint8_t t776;
  uint8_t t777;
  int64_t t778;
  int64_t t779;
  int64_t t780;
  int64_t t781;
  int64_t t782;
  uint8_t t783;
  uint8_t t784;
  int64_t t785;
  int64_t t786;
  int64_t t787;
  int64_t t788;
  int64_t t789;
  uint8_t t790;
  uint8_t t791;
  int64_t t792;
  int64_t t793;
  uint8_t t794;
  uint8_t t795;
  int64_t t796;
  int64_t t797;
  int64_t t798;
  int64_t t799;
  int64_t t800;
  uint8_t t801;
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
  uint8_t t817;
  int64_t t818;
  int64_t t819;
  int64_t t820;
  int64_t t821;
  uint8_t t822;
  uint8_t t823;
  int64_t t824;
  int64_t t825;
  int64_t t826;
  int64_t t827;
  uint8_t t828;
  uint8_t t829;
  int64_t t830;
  int64_t t831;
  int64_t t832;
  int64_t t833;
  uint8_t t834;
  int64_t t835;
  int64_t t836;
  int64_t t837;
  int64_t t838;
  int64_t t839;
  int64_t t840;
  int64_t t841;
  uint8_t t842;
  int64_t t843;
  int64_t t844;
  int64_t t845;
  int64_t t846;
  int64_t t847;
  int64_t t848;
  uint8_t t849;
  uint8_t t850;
  int64_t t851;
  int64_t t852;
  int64_t t853;
  int64_t t854;
  int64_t t855;
  uint8_t t856;
  int64_t t857;
  int64_t t858;
  int64_t t859;
  int64_t t860;
  int64_t t861;
  int64_t t862;
  int64_t t863;
  uint8_t t864;
  uint8_t t865;
  int64_t t866;
  int64_t t867;
  int64_t t868;
  int64_t t869;
  int64_t t870;
  int64_t t871;
  int64_t t872;
  int64_t t873;
  uint8_t t874;
  int64_t t875;
  int64_t t876;
  uint8_t t877;
  uint8_t t878;
  int64_t t879;
  int64_t t880;
  int64_t t881;
  int64_t t882;
  int64_t t883;
  uint8_t t884;
  uint8_t t885;
  int64_t t886;
  int64_t t887;
  int64_t t888;
  int64_t t889;
  int64_t t890;
  uint8_t t891;
  uint8_t t892;
  int64_t t893;
  int64_t t894;
  uint8_t t895;
  int64_t t896;
  int64_t t897;
  int64_t t898;
  int64_t t899;
  uint8_t t900;
  uint8_t t901;
  int64_t t902;
  int64_t t903;
  uint8_t t904;
  uint8_t t905;
  int64_t t906;
  int64_t t907;
  uint8_t t908;
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
  int64_t t919;
  uint8_t t920;
  int64_t t921;
  int64_t t922;
  int64_t t923;
  uint8_t t924;
  uint8_t t925;
  int64_t t926;
  int64_t t927;
  uint8_t t928;
  uint8_t t929;
  int64_t t930;
  int64_t t931;
  uint8_t t932;
  int64_t t933;
  int64_t t934;
  int64_t t935;
  int64_t t936;
  uint8_t t937;
  int64_t t938;
  int64_t t939;
  int64_t t940;
  int64_t t941;
  int64_t t942;
  int64_t t943;
  int64_t t944;
  uint8_t t945;
  int64_t t946;
  int64_t t947;
  int64_t t948;
  int64_t t949;
  int64_t t950;
  int64_t t951;
  uint8_t t952;
  int64_t t953;
  int64_t t954;
  int64_t t955;
  int64_t t956;
  int64_t t957;
  uint8_t t958;
  uint8_t t959;
  int64_t t960;
  int64_t t961;
  int64_t t962;
  int64_t t963;
  int64_t t964;
  int64_t t965;
  int64_t t966;
  uint8_t t967;
  uint8_t t968;
  int64_t t969;
  int64_t t970;
  int64_t t971;
  int64_t t972;
  int64_t t973;
  uint8_t t974;
  int64_t t975;
  int64_t t976;
  uint8_t t977;
  uint8_t t978;
  int64_t t979;
  int64_t t980;
  int64_t t981;
  int64_t t982;
  int64_t t983;
  uint8_t t984;
  uint8_t t985;
  int64_t t986;
  int64_t t987;
  int64_t t988;
  int64_t t989;
  int64_t t990;
  uint8_t t991;
  uint8_t t992;
  int64_t t993;
  int64_t t994;
  uint8_t t995;
  uint8_t t996;
  int64_t t997;
  int64_t t998;
  int64_t t999;
  int64_t t1000;
  int64_t t1001;
  uint8_t t1002;
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
  uint8_t t1014;
  int64_t t1015;
  int64_t t1016;
  int64_t t1017;
  int64_t t1018;
  uint8_t t1019;
  uint8_t t1020;
  int64_t t1021;
  int64_t t1022;
  int64_t t1023;
  int64_t t1024;
  uint8_t t1025;
  int64_t t1026;
  int64_t t1027;
  int64_t t1028;
  int64_t t1029;
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
  uint8_t t1040;
  int64_t t1041;
  int64_t t1042;
  uint8_t t1043;
  uint8_t t1044;
  int64_t t1045;
  int64_t t1046;
  int64_t t1047;
  int64_t t1048;
  int64_t t1049;
  uint8_t t1050;
  uint8_t t1051;
  int64_t t1052;
  int64_t t1053;
  int64_t t1054;
  int64_t t1055;
  int64_t t1056;
  uint8_t t1057;
  uint8_t t1058;
  int64_t t1059;
  int64_t t1060;
  int64_t t1061;
  int64_t t1062;
  int64_t t1063;
  uint8_t t1064;
  uint8_t t1065;
  int64_t t1066;
  int64_t t1067;
  uint8_t t1068;
  uint8_t t1069;
  int64_t t1070;
  int64_t t1071;
  int64_t t1072;
  int64_t t1073;
  int64_t t1074;
  uint8_t t1075;
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
  uint8_t t1092;
  uint8_t t1093;
  int64_t t1094;
  int64_t t1095;
  int64_t t1096;
  int64_t t1097;
  uint8_t t1098;
  int64_t t1099;
  int64_t t1100;
  int64_t t1101;
  int64_t t1102;
  int64_t t1103;
  uint8_t t1104;
  int64_t t1105;
  int64_t t1106;
  int64_t t1107;
  int64_t t1108;
  int64_t t1109;
  uint8_t t1110;
  uint8_t t1111;
  int64_t t1112;
  int64_t t1113;
  int64_t t1114;
  int64_t t1115;
  int64_t t1116;
  uint8_t t1117;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(200))));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v3));
  t4 = fg14_s_log(t1, t2, t3);
  (void)(t4);
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(29))));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(((int64_t)UINT64_C(29))));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  t10 = ((int64_t)(((int64_t)UINT64_C(100))));
  t11 = gg_function_4(t5, t6, t7, t8, t9, t10, base + 0);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) != (t13));
  t15 = t14;
  if (!t15) {
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)UINT64_C(30))));
  t18 = ((int64_t)(((int64_t)UINT64_C(3))));
  t19 = ((int64_t)(((int64_t)UINT64_C(30))));
  t20 = ((int64_t)(((int64_t)UINT64_C(3))));
  t21 = ((int64_t)(((int64_t)UINT64_C(100))));
  t22 = gg_function_4(t16, t17, t18, t19, t20, t21, base + 0);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t23) != (t24));
  }
  t25 = t15;
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(((int64_t)UINT64_C(5))));
  t31 = fg14_s_g_can_fuse_sub1(t27, t28, t29, t30);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t32) == (t33));
  }
  t34 = t26;
  if (t34) {
  {
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(v1));
  v6 = gg_function_9(t35, t36, base + 0);
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  v7 = gg_function_9(t37, t40, base + 0);
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(v1));
  t43 = ((int64_t)(((int64_t)UINT64_C(2))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  v8 = gg_function_9(t41, t44, base + 0);
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(v1));
  t47 = ((int64_t)(((int64_t)UINT64_C(3))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  v9 = gg_function_9(t45, t48, base + 0);
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(((int64_t)UINT64_C(4))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t51)))));
  v10 = gg_function_9(t49, t52, base + 0);
  t53 = ((int64_t)(v8));
  t54 = ((int64_t)(v6));
  t55 = ((int64_t)(v10));
  t56 = ((int64_t)(v9));
  t57 = ((int64_t)(v7));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = gg_function_11(t53, t54, t55, t56, t57, t58, t59, base + 0);
  t61 = ((int64_t)(t60));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t63 = ((t61) == (t62));
  if (t63) {
  {
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(v1));
  t66 = ((int64_t)(((int64_t)UINT64_C(5))));
  t67 = ((int64_t)(v1));
  t68 = ((int64_t)(((int64_t)UINT64_C(4))));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = gg_function_20(t64, t65, t66, t69, t70, base + 0);
  t72 = ((int64_t)(t71));
  t73 = ((int64_t)(((int64_t)UINT64_C(1))));
  t74 = ((t72) == (t73));
  if (t74) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t75 = ((int64_t)(v2));
  t76 = ((int64_t)(((int64_t)UINT64_C(30))));
  t77 = ((int64_t)(((int64_t)UINT64_C(30))));
  t78 = ((int64_t)(((int64_t)UINT64_C(100))));
  t79 = gg_function_5(t75, t76, t77, t78, base + 0);
  t80 = ((int64_t)(t79));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = ((t80) != (t81));
  t83 = t82;
  if (!t83) {
  t84 = ((int64_t)(v2));
  t85 = ((int64_t)(((int64_t)UINT64_C(29))));
  t86 = ((int64_t)(((int64_t)UINT64_C(29))));
  t87 = ((int64_t)(((int64_t)UINT64_C(100))));
  t88 = gg_function_5(t84, t85, t86, t87, base + 0);
  t89 = ((int64_t)(t88));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = ((t89) != (t90));
  }
  t91 = t83;
  t92 = t91;
  if (t92) {
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(v1));
  t95 = ((int64_t)(v2));
  t96 = ((int64_t)(((int64_t)UINT64_C(3))));
  t97 = fg14_s_g_can_fuse_sub1(t93, t94, t95, t96);
  t98 = ((int64_t)(t97));
  t99 = ((int64_t)(((int64_t)UINT64_C(1))));
  t92 = ((t98) == (t99));
  }
  t100 = t92;
  if (t100) {
  {
  t101 = ((int64_t)(((int64_t)UINT64_C(201))));
  t102 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = ((int64_t)(((int64_t)UINT64_C(0))));
  t104 = fg14_s_log(t101, t102, t103);
  (void)(t104);
  t105 = ((int64_t)(v0));
  t106 = ((int64_t)(v1));
  v11 = gg_function_9(t105, t106, base + 0);
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(v1));
  t109 = ((int64_t)(((int64_t)UINT64_C(1))));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) + (uint64_t)(t109)))));
  v12 = gg_function_9(t107, t110, base + 0);
  t111 = ((int64_t)(v0));
  t112 = ((int64_t)(v1));
  t113 = ((int64_t)(((int64_t)UINT64_C(2))));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) + (uint64_t)(t113)))));
  v13 = gg_function_9(t111, t114, base + 0);
  t115 = ((int64_t)(v12));
  t116 = ((int64_t)(v11));
  t117 = ((int64_t)(v13));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = gg_function_11(t115, t116, t117, t118, t119, t120, t121, base + 0);
  t123 = ((int64_t)(t122));
  t124 = ((int64_t)(((int64_t)UINT64_C(1))));
  t125 = ((t123) == (t124));
  if (t125) {
  {
  t126 = ((int64_t)(v0));
  t127 = ((int64_t)(v1));
  t128 = ((int64_t)(((int64_t)UINT64_C(3))));
  t129 = ((int64_t)(v1));
  t130 = ((int64_t)(((int64_t)UINT64_C(2))));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t129) + (uint64_t)(t130)))));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t133 = gg_function_20(t126, t127, t128, t131, t132, base + 0);
  t134 = ((int64_t)(t133));
  t135 = ((int64_t)(((int64_t)UINT64_C(1))));
  t136 = ((t134) == (t135));
  if (t136) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  }
  }
  t137 = ((int64_t)(((int64_t)UINT64_C(202))));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((int64_t)(((int64_t)UINT64_C(0))));
  t140 = fg14_s_log(t137, t138, t139);
  (void)(t140);
  }
  }
  t141 = ((int64_t)(((int64_t)UINT64_C(203))));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = ((int64_t)(((int64_t)UINT64_C(0))));
  t144 = fg14_s_log(t141, t142, t143);
  (void)(t144);
  t145 = ((int64_t)(v3));
  t146 = ((int64_t)(((int64_t)UINT64_C(5))));
  t147 = ((t145) == (t146));
  t148 = t147;
  if (t148) {
  t149 = ((int64_t)(v2));
  t150 = ((int64_t)(((int64_t)UINT64_C(0))));
  t151 = gg_function_34(t149, t150, base + 0);
  t152 = ((int64_t)(t151));
  t153 = ((int64_t)(((int64_t)UINT64_C(44))));
  t148 = ((t152) == (t153));
  }
  t154 = t148;
  t155 = t154;
  if (t155) {
  t156 = ((int64_t)(v2));
  t157 = ((int64_t)(((int64_t)UINT64_C(1))));
  t158 = gg_function_34(t156, t157, base + 0);
  t159 = ((int64_t)(t158));
  t160 = ((int64_t)(((int64_t)UINT64_C(7))));
  t155 = ((t159) == (t160));
  }
  t161 = t155;
  t162 = t161;
  if (t162) {
  t163 = ((int64_t)(v2));
  t164 = ((int64_t)(((int64_t)UINT64_C(2))));
  t165 = gg_function_34(t163, t164, base + 0);
  t166 = ((int64_t)(t165));
  t167 = ((int64_t)(((int64_t)UINT64_C(48))));
  t162 = ((t166) == (t167));
  }
  t168 = t162;
  t169 = t168;
  if (t169) {
  t170 = ((int64_t)(v2));
  t171 = ((int64_t)(((int64_t)UINT64_C(3))));
  t172 = gg_function_34(t170, t171, base + 0);
  t173 = ((int64_t)(t172));
  t174 = ((int64_t)(((int64_t)UINT64_C(37))));
  t169 = ((t173) == (t174));
  }
  t175 = t169;
  t176 = t175;
  if (t176) {
  t177 = ((int64_t)(v2));
  t178 = ((int64_t)(((int64_t)UINT64_C(4))));
  t179 = gg_function_34(t177, t178, base + 0);
  t180 = ((int64_t)(t179));
  t181 = ((int64_t)(((int64_t)UINT64_C(42))));
  t176 = ((t180) == (t181));
  }
  t182 = t176;
  t183 = t182;
  if (t183) {
  t184 = ((int64_t)(v0));
  t185 = ((int64_t)(v1));
  t186 = ((int64_t)(v2));
  t187 = ((int64_t)(((int64_t)UINT64_C(5))));
  t188 = fg14_s_g_can_fuse_sub1(t184, t185, t186, t187);
  t189 = ((int64_t)(t188));
  t190 = ((int64_t)(((int64_t)UINT64_C(1))));
  t183 = ((t189) == (t190));
  }
  t191 = t183;
  if (t191) {
  {
  t192 = ((int64_t)(v0));
  t193 = ((int64_t)(v1));
  v14 = gg_function_9(t192, t193, base + 0);
  t194 = ((int64_t)(v0));
  t195 = ((int64_t)(v1));
  t196 = ((int64_t)(((int64_t)UINT64_C(1))));
  t197 = ((int64_t)(((int64_t)((uint64_t)(t195) + (uint64_t)(t196)))));
  v15 = gg_function_9(t194, t197, base + 0);
  t198 = ((int64_t)(v0));
  t199 = ((int64_t)(v1));
  t200 = ((int64_t)(((int64_t)UINT64_C(2))));
  t201 = ((int64_t)(((int64_t)((uint64_t)(t199) + (uint64_t)(t200)))));
  v16 = gg_function_9(t198, t201, base + 0);
  t202 = ((int64_t)(v0));
  t203 = ((int64_t)(v1));
  t204 = ((int64_t)(((int64_t)UINT64_C(3))));
  t205 = ((int64_t)(((int64_t)((uint64_t)(t203) + (uint64_t)(t204)))));
  v17 = gg_function_9(t202, t205, base + 0);
  t206 = ((int64_t)(v0));
  t207 = ((int64_t)(v1));
  t208 = ((int64_t)(((int64_t)UINT64_C(4))));
  t209 = ((int64_t)(((int64_t)((uint64_t)(t207) + (uint64_t)(t208)))));
  v18 = gg_function_9(t206, t209, base + 0);
  t210 = ((int64_t)(v0));
  t211 = ((int64_t)(v1));
  t212 = ((int64_t)(((int64_t)UINT64_C(1))));
  t213 = ((int64_t)(((int64_t)UINT64_C(0))));
  t214 = ((int64_t)(((int64_t)UINT64_C(0))));
  t215 = fg14_s_g_edge(t210, t211, t212, t213, t214);
  t216 = ((int64_t)(t215));
  t217 = ((int64_t)(((int64_t)UINT64_C(1))));
  t218 = ((t216) == (t217));
  t219 = t218;
  if (t219) {
  t220 = ((int64_t)(v0));
  t221 = ((int64_t)(v1));
  t222 = ((int64_t)(((int64_t)UINT64_C(2))));
  t223 = ((int64_t)(((int64_t)UINT64_C(0))));
  t224 = ((int64_t)(((int64_t)UINT64_C(1))));
  t225 = fg14_s_g_edge(t220, t221, t222, t223, t224);
  t226 = ((int64_t)(t225));
  t227 = ((int64_t)(((int64_t)UINT64_C(1))));
  t219 = ((t226) == (t227));
  }
  t228 = t219;
  t229 = t228;
  if (t229) {
  t230 = ((int64_t)(v0));
  t231 = ((int64_t)(v1));
  t232 = ((int64_t)(((int64_t)UINT64_C(3))));
  t233 = ((int64_t)(((int64_t)UINT64_C(0))));
  t234 = ((int64_t)(((int64_t)UINT64_C(2))));
  t235 = fg14_s_g_edge(t230, t231, t232, t233, t234);
  t236 = ((int64_t)(t235));
  t237 = ((int64_t)(((int64_t)UINT64_C(1))));
  t229 = ((t236) == (t237));
  }
  t238 = t229;
  t239 = t238;
  if (t239) {
  t240 = ((int64_t)(v0));
  t241 = ((int64_t)(v1));
  t242 = ((int64_t)(((int64_t)UINT64_C(4))));
  t243 = ((int64_t)(((int64_t)UINT64_C(0))));
  t244 = ((int64_t)(((int64_t)UINT64_C(3))));
  t245 = fg14_s_g_edge(t240, t241, t242, t243, t244);
  t246 = ((int64_t)(t245));
  t247 = ((int64_t)(((int64_t)UINT64_C(1))));
  t239 = ((t246) == (t247));
  }
  t248 = t239;
  t249 = t248;
  if (t249) {
  t250 = ((int64_t)(v14));
  t251 = ((int64_t)(v15));
  t252 = ((int64_t)(v16));
  t253 = gg_function_27(t250, t251, t252, base + 0);
  t254 = ((int64_t)(t253));
  t255 = ((int64_t)(((int64_t)UINT64_C(1))));
  t249 = ((t254) == (t255));
  }
  t256 = t249;
  t257 = t256;
  if (t257) {
  t258 = ((int64_t)(v16));
  t259 = ((int64_t)(v17));
  t260 = ((int64_t)(v18));
  t261 = gg_function_30(t258, t259, t260, base + 0);
  t262 = ((int64_t)(t261));
  t263 = ((int64_t)(((int64_t)UINT64_C(1))));
  t257 = ((t262) == (t263));
  }
  t264 = t257;
  if (t264) {
  {
  t265 = ((int64_t)(v0));
  t266 = ((int64_t)(v1));
  t267 = ((int64_t)(((int64_t)UINT64_C(5))));
  t268 = ((int64_t)(v1));
  t269 = ((int64_t)(((int64_t)UINT64_C(4))));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t268) + (uint64_t)(t269)))));
  t271 = ((int64_t)(((int64_t)UINT64_C(0))));
  t272 = gg_function_20(t265, t266, t267, t270, t271, base + 0);
  t273 = ((int64_t)(t272));
  t274 = ((int64_t)(((int64_t)UINT64_C(1))));
  t275 = ((t273) == (t274));
  if (t275) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t276 = ((int64_t)(v3));
  t277 = ((int64_t)(((int64_t)UINT64_C(3))));
  t278 = ((t276) == (t277));
  t279 = t278;
  if (t279) {
  t280 = ((int64_t)(v2));
  t281 = ((int64_t)(((int64_t)UINT64_C(0))));
  t282 = gg_function_34(t280, t281, base + 0);
  t283 = ((int64_t)(t282));
  t284 = ((int64_t)(((int64_t)UINT64_C(44))));
  t279 = ((t283) == (t284));
  }
  t285 = t279;
  t286 = t285;
  if (t286) {
  t287 = ((int64_t)(v2));
  t288 = ((int64_t)(((int64_t)UINT64_C(1))));
  t289 = gg_function_34(t287, t288, base + 0);
  t290 = ((int64_t)(t289));
  t291 = ((int64_t)(((int64_t)UINT64_C(7))));
  t286 = ((t290) == (t291));
  }
  t292 = t286;
  t293 = t292;
  if (t293) {
  t294 = ((int64_t)(v2));
  t295 = ((int64_t)(((int64_t)UINT64_C(2))));
  t296 = gg_function_34(t294, t295, base + 0);
  t297 = ((int64_t)(t296));
  t298 = ((int64_t)(((int64_t)UINT64_C(48))));
  t293 = ((t297) == (t298));
  }
  t299 = t293;
  t300 = t299;
  if (t300) {
  t301 = ((int64_t)(v0));
  t302 = ((int64_t)(v1));
  t303 = ((int64_t)(v2));
  t304 = ((int64_t)(v3));
  t305 = fg14_s_g_can_fuse(t301, t302, t303, t304);
  t306 = ((int64_t)(t305));
  t307 = ((int64_t)(((int64_t)UINT64_C(1))));
  t300 = ((t306) == (t307));
  }
  t308 = t300;
  if (t308) {
  {
  t309 = ((int64_t)(v0));
  t310 = ((int64_t)(v1));
  v19 = gg_function_9(t309, t310, base + 0);
  t311 = ((int64_t)(v0));
  t312 = ((int64_t)(v1));
  t313 = ((int64_t)(((int64_t)UINT64_C(1))));
  t314 = ((int64_t)(((int64_t)((uint64_t)(t312) + (uint64_t)(t313)))));
  v20 = gg_function_9(t311, t314, base + 0);
  t315 = ((int64_t)(v0));
  t316 = ((int64_t)(v1));
  t317 = ((int64_t)(((int64_t)UINT64_C(2))));
  t318 = ((int64_t)(((int64_t)((uint64_t)(t316) + (uint64_t)(t317)))));
  v21 = gg_function_9(t315, t318, base + 0);
  t319 = ((int64_t)(v19));
  t320 = ((int64_t)(v20));
  t321 = ((int64_t)(v21));
  t322 = gg_function_27(t319, t320, t321, base + 0);
  t323 = ((int64_t)(t322));
  t324 = ((int64_t)(((int64_t)UINT64_C(1))));
  t325 = ((t323) == (t324));
  if (t325) {
  {
  t326 = ((int64_t)(v0));
  t327 = ((int64_t)(v1));
  t328 = ((int64_t)(((int64_t)UINT64_C(3))));
  t329 = ((int64_t)(v1));
  t330 = ((int64_t)(((int64_t)UINT64_C(2))));
  t331 = ((int64_t)(((int64_t)((uint64_t)(t329) + (uint64_t)(t330)))));
  t332 = ((int64_t)(((int64_t)UINT64_C(0))));
  t333 = gg_function_20(t326, t327, t328, t331, t332, base + 0);
  t334 = ((int64_t)(t333));
  t335 = ((int64_t)(((int64_t)UINT64_C(1))));
  t336 = ((t334) == (t335));
  if (t336) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t337 = ((int64_t)(v3));
  t338 = ((int64_t)(((int64_t)UINT64_C(3))));
  t339 = ((t337) == (t338));
  t340 = t339;
  if (t340) {
  t341 = ((int64_t)(v2));
  t342 = ((int64_t)(((int64_t)UINT64_C(0))));
  t343 = gg_function_34(t341, t342, base + 0);
  t344 = ((int64_t)(t343));
  t345 = ((int64_t)(((int64_t)UINT64_C(48))));
  t340 = ((t344) == (t345));
  }
  t346 = t340;
  t347 = t346;
  if (t347) {
  t348 = ((int64_t)(v2));
  t349 = ((int64_t)(((int64_t)UINT64_C(1))));
  t350 = gg_function_34(t348, t349, base + 0);
  t351 = ((int64_t)(t350));
  t352 = ((int64_t)(((int64_t)UINT64_C(37))));
  t347 = ((t351) == (t352));
  }
  t353 = t347;
  t354 = t353;
  if (t354) {
  t355 = ((int64_t)(v2));
  t356 = ((int64_t)(((int64_t)UINT64_C(2))));
  t357 = gg_function_34(t355, t356, base + 0);
  t358 = ((int64_t)(t357));
  t359 = ((int64_t)(((int64_t)UINT64_C(42))));
  t354 = ((t358) == (t359));
  }
  t360 = t354;
  t361 = t360;
  if (t361) {
  t362 = ((int64_t)(v0));
  t363 = ((int64_t)(v1));
  t364 = ((int64_t)(v2));
  t365 = ((int64_t)(((int64_t)UINT64_C(3))));
  t366 = fg14_s_g_can_fuse_sub1(t362, t363, t364, t365);
  t367 = ((int64_t)(t366));
  t368 = ((int64_t)(((int64_t)UINT64_C(1))));
  t361 = ((t367) == (t368));
  }
  t369 = t361;
  if (t369) {
  {
  t370 = ((int64_t)(v0));
  t371 = ((int64_t)(v1));
  t372 = gg_function_9(t370, t371, base + 0);
  t373 = ((int64_t)(v0));
  t374 = ((int64_t)(v1));
  t375 = ((int64_t)(((int64_t)UINT64_C(1))));
  t376 = ((int64_t)(((int64_t)((uint64_t)(t374) + (uint64_t)(t375)))));
  t377 = gg_function_9(t373, t376, base + 0);
  t378 = ((int64_t)(v0));
  t379 = ((int64_t)(v1));
  t380 = ((int64_t)(((int64_t)UINT64_C(2))));
  t381 = ((int64_t)(((int64_t)((uint64_t)(t379) + (uint64_t)(t380)))));
  t382 = gg_function_9(t378, t381, base + 0);
  t383 = gg_function_30(t372, t377, t382, base + 0);
  t384 = ((int64_t)(t383));
  t385 = ((int64_t)(((int64_t)UINT64_C(1))));
  t386 = ((t384) == (t385));
  if (t386) {
  {
  t387 = ((int64_t)(v0));
  t388 = ((int64_t)(v1));
  t389 = ((int64_t)(((int64_t)UINT64_C(3))));
  t390 = ((int64_t)(v1));
  t391 = ((int64_t)(((int64_t)UINT64_C(2))));
  t392 = ((int64_t)(((int64_t)((uint64_t)(t390) + (uint64_t)(t391)))));
  t393 = ((int64_t)(((int64_t)UINT64_C(0))));
  t394 = gg_function_20(t387, t388, t389, t392, t393, base + 0);
  t395 = ((int64_t)(t394));
  t396 = ((int64_t)(((int64_t)UINT64_C(1))));
  t397 = ((t395) == (t396));
  if (t397) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  }
  }
  }
  }
  t398 = ((int64_t)(v3));
  t399 = ((int64_t)(((int64_t)UINT64_C(4))));
  t400 = ((t398) == (t399));
  t401 = t400;
  if (t401) {
  t402 = ((int64_t)(v2));
  t403 = ((int64_t)(((int64_t)UINT64_C(0))));
  t404 = gg_function_34(t402, t403, base + 0);
  t405 = ((int64_t)(t404));
  t406 = ((int64_t)(((int64_t)UINT64_C(32))));
  t401 = ((t405) == (t406));
  }
  t407 = t401;
  t408 = t407;
  if (t408) {
  t409 = ((int64_t)(v2));
  t410 = ((int64_t)(((int64_t)UINT64_C(1))));
  t411 = gg_function_34(t409, t410, base + 0);
  t412 = ((int64_t)(t411));
  t413 = ((int64_t)(((int64_t)UINT64_C(91))));
  t408 = ((t412) == (t413));
  }
  t414 = t408;
  t415 = t414;
  if (t415) {
  t416 = ((int64_t)(v2));
  t417 = ((int64_t)(((int64_t)UINT64_C(2))));
  t418 = gg_function_34(t416, t417, base + 0);
  t419 = ((int64_t)(t418));
  t420 = ((int64_t)(((int64_t)UINT64_C(32))));
  t415 = ((t419) == (t420));
  }
  t421 = t415;
  t422 = t421;
  if (t422) {
  t423 = ((int64_t)(v2));
  t424 = ((int64_t)(((int64_t)UINT64_C(3))));
  t425 = gg_function_34(t423, t424, base + 0);
  t426 = ((int64_t)(t425));
  t427 = ((int64_t)(((int64_t)UINT64_C(90))));
  t422 = ((t426) == (t427));
  }
  t428 = t422;
  t429 = t428;
  if (t429) {
  t430 = ((int64_t)(v0));
  t431 = ((int64_t)(v1));
  t432 = ((int64_t)(v2));
  t433 = ((int64_t)(((int64_t)UINT64_C(4))));
  t434 = fg14_s_g_can_fuse_sub1(t430, t431, t432, t433);
  t435 = ((int64_t)(t434));
  t436 = ((int64_t)(((int64_t)UINT64_C(1))));
  t429 = ((t435) == (t436));
  }
  t437 = t429;
  if (t437) {
  {
  t438 = ((int64_t)(v0));
  t439 = ((int64_t)(v1));
  v22 = gg_function_9(t438, t439, base + 0);
  t440 = ((int64_t)(v0));
  t441 = ((int64_t)(v1));
  t442 = ((int64_t)(((int64_t)UINT64_C(1))));
  t443 = ((int64_t)(((int64_t)((uint64_t)(t441) + (uint64_t)(t442)))));
  v23 = gg_function_9(t440, t443, base + 0);
  t444 = ((int64_t)(v0));
  t445 = ((int64_t)(v1));
  t446 = ((int64_t)(((int64_t)UINT64_C(2))));
  t447 = ((int64_t)(((int64_t)((uint64_t)(t445) + (uint64_t)(t446)))));
  v24 = gg_function_9(t444, t447, base + 0);
  t448 = ((int64_t)(v0));
  t449 = ((int64_t)(v1));
  t450 = ((int64_t)(((int64_t)UINT64_C(1))));
  t451 = ((int64_t)(((int64_t)UINT64_C(0))));
  t452 = ((int64_t)(((int64_t)UINT64_C(0))));
  t453 = fg14_s_g_edge(t448, t449, t450, t451, t452);
  t454 = ((int64_t)(t453));
  t455 = ((int64_t)(((int64_t)UINT64_C(1))));
  t456 = ((t454) == (t455));
  t457 = t456;
  if (t457) {
  t458 = ((int64_t)(v0));
  t459 = ((int64_t)(v1));
  t460 = ((int64_t)(((int64_t)UINT64_C(2))));
  t461 = ((int64_t)(((int64_t)UINT64_C(0))));
  t462 = ((int64_t)(((int64_t)UINT64_C(1))));
  t463 = fg14_s_g_edge(t458, t459, t460, t461, t462);
  t464 = ((int64_t)(t463));
  t465 = ((int64_t)(((int64_t)UINT64_C(1))));
  t457 = ((t464) == (t465));
  }
  t466 = t457;
  t467 = t466;
  if (t467) {
  t468 = ((int64_t)(v0));
  t469 = ((int64_t)(v1));
  t470 = ((int64_t)(((int64_t)UINT64_C(3))));
  t471 = ((int64_t)(((int64_t)UINT64_C(2))));
  t472 = ((int64_t)(((int64_t)UINT64_C(2))));
  t473 = fg14_s_g_edge(t468, t469, t470, t471, t472);
  t474 = ((int64_t)(t473));
  t475 = ((int64_t)(((int64_t)UINT64_C(1))));
  t467 = ((t474) == (t475));
  }
  t476 = t467;
  t477 = t476;
  if (t477) {
  t478 = ((int64_t)(v23));
  t479 = ((int64_t)(((int64_t)UINT64_C(0))));
  t480 = gg_function_19(t478, t479, base + 0);
  t481 = ((int64_t)(t480));
  t482 = ((int64_t)(((int64_t)UINT64_C(7))));
  t477 = ((t481) == (t482));
  }
  t483 = t477;
  t484 = t483;
  if (t484) {
  t485 = ((int64_t)(v22));
  t486 = ((int64_t)(((int64_t)UINT64_C(1))));
  t487 = gg_function_32(t485, t486, base + 0);
  t488 = ((int64_t)(t487));
  t489 = ((int64_t)(((int64_t)UINT64_C(0))));
  t484 = ((t488) == (t489));
  }
  t490 = t484;
  t491 = t490;
  if (t491) {
  t492 = ((int64_t)(v24));
  t493 = ((int64_t)(((int64_t)UINT64_C(1))));
  t494 = gg_function_32(t492, t493, base + 0);
  t495 = ((int64_t)(t494));
  t496 = ((int64_t)(((int64_t)UINT64_C(0))));
  t491 = ((t495) == (t496));
  }
  t497 = t491;
  t498 = t497;
  if (t498) {
  t499 = ((int64_t)(v22));
  t500 = ((int64_t)(((int64_t)UINT64_C(0))));
  t501 = gg_function_14(t499, t500, base + 0);
  t502 = gg_function_16(t501, base + 0);
  t503 = ((int64_t)(t502));
  t504 = ((int64_t)(((int64_t)UINT64_C(0))));
  t498 = ((t503) == (t504));
  }
  t505 = t498;
  t506 = t505;
  if (t506) {
  t507 = ((int64_t)(v24));
  t508 = gg_function_16(t507, base + 0);
  t509 = ((int64_t)(t508));
  t510 = ((int64_t)(((int64_t)UINT64_C(0))));
  t506 = ((t509) == (t510));
  }
  t511 = t506;
  t512 = t511;
  if (t512) {
  t513 = ((int64_t)(v22));
  t514 = ((int64_t)(((int64_t)UINT64_C(0))));
  t515 = gg_function_14(t513, t514, base + 0);
  t516 = ((int64_t)(v24));
  t517 = fg14_s_t_same_shape(t515, t516);
  t518 = ((int64_t)(t517));
  t519 = ((int64_t)(((int64_t)UINT64_C(1))));
  t512 = ((t518) == (t519));
  }
  t520 = t512;
  if (t520) {
  {
  t521 = ((int64_t)(v0));
  t522 = ((int64_t)(v1));
  t523 = ((int64_t)(((int64_t)UINT64_C(4))));
  t524 = ((int64_t)(v1));
  t525 = ((int64_t)(((int64_t)UINT64_C(3))));
  t526 = ((int64_t)(((int64_t)((uint64_t)(t524) + (uint64_t)(t525)))));
  t527 = ((int64_t)(((int64_t)UINT64_C(0))));
  t528 = gg_function_20(t521, t522, t523, t526, t527, base + 0);
  t529 = ((int64_t)(t528));
  t530 = ((int64_t)(((int64_t)UINT64_C(1))));
  t531 = ((t529) == (t530));
  if (t531) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t532 = ((int64_t)(((int64_t)UINT64_C(207))));
  t533 = ((int64_t)(((int64_t)UINT64_C(0))));
  t534 = ((int64_t)(((int64_t)UINT64_C(0))));
  t535 = fg14_s_log(t532, t533, t534);
  (void)(t535);
  t536 = ((int64_t)(v0));
  t537 = ((int64_t)(v1));
  t538 = ((int64_t)(v2));
  t539 = ((int64_t)(v3));
  t540 = fg14_s_g_can_fuse(t536, t537, t538, t539);
  t541 = ((int64_t)(t540));
  t542 = ((int64_t)(((int64_t)UINT64_C(0))));
  t543 = ((t541) == (t542));
  if (t543) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t544 = ((int64_t)(v3));
  t545 = ((int64_t)(((int64_t)UINT64_C(2))));
  t546 = ((t544) == (t545));
  t547 = t546;
  if (!t547) {
  t548 = ((int64_t)(v3));
  t549 = ((int64_t)(((int64_t)UINT64_C(3))));
  t547 = ((t548) == (t549));
  }
  t550 = t547;
  if (t550) {
  {
  t551 = ((int64_t)(v2));
  t552 = ((int64_t)(((int64_t)UINT64_C(0))));
  t553 = gg_function_34(t551, t552, base + 0);
  t554 = ((int64_t)(t553));
  t555 = ((int64_t)(((int64_t)UINT64_C(44))));
  t556 = ((t554) == (t555));
  t557 = t556;
  if (t557) {
  t558 = ((int64_t)(v2));
  t559 = ((int64_t)(((int64_t)UINT64_C(1))));
  t560 = gg_function_34(t558, t559, base + 0);
  t561 = ((int64_t)(t560));
  t562 = ((int64_t)(((int64_t)UINT64_C(7))));
  t557 = ((t561) == (t562));
  }
  t563 = t557;
  if (t563) {
  {
  t564 = ((int64_t)(v0));
  t565 = ((int64_t)(v1));
  v25 = gg_function_9(t564, t565, base + 0);
  t566 = ((int64_t)(v0));
  t567 = ((int64_t)(v1));
  t568 = ((int64_t)(((int64_t)UINT64_C(1))));
  t569 = ((int64_t)(((int64_t)((uint64_t)(t567) + (uint64_t)(t568)))));
  v26 = gg_function_9(t566, t569, base + 0);
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t570 = ((int64_t)(v3));
  t571 = ((int64_t)(((int64_t)UINT64_C(3))));
  t572 = ((t570) == (t571));
  t573 = t572;
  if (t573) {
  t574 = ((int64_t)(v2));
  t575 = ((int64_t)(((int64_t)UINT64_C(2))));
  t576 = gg_function_34(t574, t575, base + 0);
  t577 = ((int64_t)(t576));
  t578 = ((int64_t)(((int64_t)UINT64_C(2))));
  t573 = ((t577) == (t578));
  }
  t579 = t573;
  if (t579) {
  {
  t580 = ((int64_t)(v0));
  t581 = ((int64_t)(v1));
  t582 = ((int64_t)(((int64_t)UINT64_C(2))));
  t583 = ((int64_t)(((int64_t)((uint64_t)(t581) + (uint64_t)(t582)))));
  v27 = gg_function_9(t580, t583, base + 0);
  }
  }
  t584 = ((int64_t)(v27));
  t585 = ((int64_t)(((int64_t)UINT64_C(0))));
  t586 = ((t584) != (t585));
  t587 = t586;
  if (t587) {
  t588 = ((int64_t)(v27));
  t589 = ((int64_t)(((int64_t)UINT64_C(0))));
  t590 = gg_function_14(t588, t589, base + 0);
  t591 = gg_function_16(t590, base + 0);
  t592 = ((int64_t)(t591));
  t593 = ((int64_t)(((int64_t)UINT64_C(0))));
  t594 = ((t592) != (t593));
  t595 = t594;
  if (!t595) {
  t596 = ((int64_t)(v27));
  t597 = ((int64_t)(((int64_t)UINT64_C(1))));
  t598 = gg_function_14(t596, t597, base + 0);
  t599 = gg_function_16(t598, base + 0);
  t600 = ((int64_t)(t599));
  t601 = ((int64_t)(((int64_t)UINT64_C(0))));
  t595 = ((t600) != (t601));
  }
  t602 = t595;
  t603 = t602;
  if (!t603) {
  t604 = ((int64_t)(v27));
  t605 = gg_function_16(t604, base + 0);
  t606 = ((int64_t)(t605));
  t607 = ((int64_t)(((int64_t)UINT64_C(0))));
  t603 = ((t606) != (t607));
  }
  t587 = t603;
  }
  t608 = t587;
  if (t608) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t609 = ((int64_t)(v26));
  t610 = ((int64_t)(((int64_t)UINT64_C(0))));
  t611 = gg_function_14(t609, t610, base + 0);
  t612 = gg_function_16(t611, base + 0);
  t613 = ((int64_t)(t612));
  t614 = ((int64_t)(((int64_t)UINT64_C(0))));
  t615 = ((t613) != (t614));
  t616 = t615;
  if (!t616) {
  t617 = ((int64_t)(v26));
  t618 = ((int64_t)(((int64_t)UINT64_C(1))));
  t619 = gg_function_14(t617, t618, base + 0);
  t620 = gg_function_16(t619, base + 0);
  t621 = ((int64_t)(t620));
  t622 = ((int64_t)(((int64_t)UINT64_C(0))));
  t616 = ((t621) != (t622));
  }
  t623 = t616;
  t624 = t623;
  if (!t624) {
  t625 = ((int64_t)(v26));
  t626 = gg_function_16(t625, base + 0);
  t627 = ((int64_t)(t626));
  t628 = ((int64_t)(((int64_t)UINT64_C(0))));
  t624 = ((t627) != (t628));
  }
  t629 = t624;
  if (t629) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t630 = ((int64_t)(v25));
  t631 = ((int64_t)(v26));
  t632 = ((int64_t)(((int64_t)UINT64_C(1))));
  t633 = gg_function_14(t631, t632, base + 0);
  t634 = ((int64_t)(t633));
  t635 = ((t630) == (t634));
  t636 = t635;
  if (t636) {
  t637 = ((int64_t)(v26));
  t638 = ((int64_t)(((int64_t)UINT64_C(0))));
  t639 = gg_function_14(t637, t638, base + 0);
  t640 = ((int64_t)(v25));
  t641 = fg14_s_t_same_shape(t639, t640);
  t642 = ((int64_t)(t641));
  t643 = ((int64_t)(((int64_t)UINT64_C(0))));
  t636 = ((t642) == (t643));
  }
  t644 = t636;
  if (t644) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t645 = ((int64_t)(v26));
  t646 = ((int64_t)(((int64_t)UINT64_C(0))));
  t647 = gg_function_14(t645, t646, base + 0);
  t648 = ((int64_t)(((int64_t)UINT64_C(15))));
  t649 = fg14_s_contig(t647, t648);
  t650 = ((int64_t)(t649));
  t651 = ((int64_t)(((int64_t)UINT64_C(0))));
  t652 = ((t650) == (t651));
  t653 = t652;
  if (!t653) {
  t654 = ((int64_t)(v26));
  t655 = ((int64_t)(((int64_t)UINT64_C(1))));
  t656 = gg_function_14(t654, t655, base + 0);
  t657 = ((int64_t)(((int64_t)UINT64_C(15))));
  t658 = fg14_s_contig(t656, t657);
  t659 = ((int64_t)(t658));
  t660 = ((int64_t)(((int64_t)UINT64_C(0))));
  t653 = ((t659) == (t660));
  }
  t661 = t653;
  if (t661) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t662 = ((int64_t)(v27));
  t663 = ((int64_t)(((int64_t)UINT64_C(0))));
  t664 = ((t662) != (t663));
  t665 = t664;
  if (t665) {
  t666 = ((int64_t)(v27));
  t667 = ((int64_t)(((int64_t)UINT64_C(0))));
  t668 = gg_function_14(t666, t667, base + 0);
  t669 = ((int64_t)(((int64_t)UINT64_C(12))));
  t670 = fg14_s_contig(t668, t669);
  t671 = ((int64_t)(t670));
  t672 = ((int64_t)(((int64_t)UINT64_C(0))));
  t673 = ((t671) == (t672));
  t674 = t673;
  if (!t674) {
  t675 = ((int64_t)(v27));
  t676 = ((int64_t)(((int64_t)UINT64_C(1))));
  t677 = gg_function_14(t675, t676, base + 0);
  t678 = ((int64_t)(((int64_t)UINT64_C(15))));
  t679 = fg14_s_contig(t677, t678);
  t680 = ((int64_t)(t679));
  t681 = ((int64_t)(((int64_t)UINT64_C(0))));
  t674 = ((t680) == (t681));
  }
  t665 = t674;
  }
  t682 = t665;
  if (t682) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t683 = ((int64_t)(v3));
  t684 = ((int64_t)(((int64_t)UINT64_C(2))));
  t685 = ((t683) == (t684));
  t686 = t685;
  if (t686) {
  t687 = ((int64_t)(v2));
  t688 = ((int64_t)(((int64_t)UINT64_C(0))));
  t689 = gg_function_34(t687, t688, base + 0);
  t690 = ((int64_t)(t689));
  t691 = ((int64_t)(((int64_t)UINT64_C(44))));
  t686 = ((t690) == (t691));
  }
  t692 = t686;
  t693 = t692;
  if (t693) {
  t694 = ((int64_t)(v2));
  t695 = ((int64_t)(((int64_t)UINT64_C(1))));
  t696 = gg_function_34(t694, t695, base + 0);
  t697 = ((int64_t)(t696));
  t698 = ((int64_t)(((int64_t)UINT64_C(32))));
  t693 = ((t697) == (t698));
  }
  t699 = t693;
  if (t699) {
  {
  t700 = ((int64_t)(v0));
  t701 = ((int64_t)(v1));
  t702 = ((int64_t)(((int64_t)UINT64_C(1))));
  t703 = ((int64_t)(((int64_t)((uint64_t)(t701) + (uint64_t)(t702)))));
  v28 = gg_function_9(t700, t703, base + 0);
  t704 = ((int64_t)(v28));
  t705 = ((int64_t)(((int64_t)UINT64_C(1))));
  t706 = gg_function_32(t704, t705, base + 0);
  t707 = ((int64_t)(t706));
  t708 = ((int64_t)(((int64_t)UINT64_C(0))));
  t709 = ((t707) == (t708));
  t710 = t709;
  if (t710) {
  t711 = ((int64_t)(v28));
  t712 = gg_function_16(t711, base + 0);
  t713 = ((int64_t)(t712));
  t714 = ((int64_t)(((int64_t)UINT64_C(0))));
  t710 = ((t713) == (t714));
  }
  t715 = t710;
  if (t715) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t716 = ((int64_t)(v3));
  t717 = ((int64_t)(((int64_t)UINT64_C(2))));
  t718 = ((t716) == (t717));
  t719 = t718;
  if (t719) {
  t720 = ((int64_t)(v2));
  t721 = ((int64_t)(((int64_t)UINT64_C(0))));
  t722 = gg_function_34(t720, t721, base + 0);
  t723 = ((int64_t)(t722));
  t724 = ((int64_t)(((int64_t)UINT64_C(76))));
  t719 = ((t723) == (t724));
  }
  t725 = t719;
  t726 = t725;
  if (t726) {
  t727 = ((int64_t)(v2));
  t728 = ((int64_t)(((int64_t)UINT64_C(1))));
  t729 = gg_function_34(t727, t728, base + 0);
  t730 = ((int64_t)(t729));
  t731 = ((int64_t)(((int64_t)UINT64_C(91))));
  t726 = ((t730) == (t731));
  }
  t732 = t726;
  t733 = t732;
  if (t733) {
  t734 = ((int64_t)(v5));
  t735 = ((int64_t)(((int64_t)UINT64_C(1))));
  t733 = ((t734) == (t735));
  }
  t736 = t733;
  t737 = t736;
  if (t737) {
  t738 = ((int64_t)(v4));
  t739 = ((int64_t)(((int64_t)UINT64_C(0))));
  t740 = gg_function_34(t738, t739, base + 0);
  t741 = ((int64_t)(t740));
  t742 = ((int64_t)(((int64_t)UINT64_C(10))));
  t737 = ((t741) == (t742));
  }
  t743 = t737;
  if (t743) {
  {
  t744 = ((int64_t)(v0));
  t745 = ((int64_t)(v1));
  v29 = gg_function_9(t744, t745, base + 0);
  t746 = ((int64_t)(v0));
  t747 = ((int64_t)(v1));
  t748 = ((int64_t)(((int64_t)UINT64_C(1))));
  t749 = ((int64_t)(((int64_t)((uint64_t)(t747) + (uint64_t)(t748)))));
  v30 = gg_function_9(t746, t749, base + 0);
  t750 = ((int64_t)(v30));
  t751 = ((int64_t)(((int64_t)UINT64_C(10))));
  t752 = gg_function_35(t750, t751, base + 0);
  t753 = ((int64_t)(t752));
  t754 = ((int64_t)(((int64_t)UINT64_C(0))));
  t755 = ((t753) == (t754));
  if (t755) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t756 = ((int64_t)(v29));
  t757 = gg_function_16(t756, base + 0);
  t758 = ((int64_t)(t757));
  t759 = ((int64_t)(((int64_t)UINT64_C(0))));
  t760 = ((t758) != (t759));
  t761 = t760;
  if (!t761) {
  t762 = ((int64_t)(v30));
  t763 = gg_function_16(t762, base + 0);
  t764 = ((int64_t)(t763));
  t765 = ((int64_t)(((int64_t)UINT64_C(0))));
  t761 = ((t764) != (t765));
  }
  t766 = t761;
  if (t766) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t767 = ((int64_t)(v3));
  t768 = ((int64_t)(((int64_t)UINT64_C(3))));
  t769 = ((t767) == (t768));
  t770 = t769;
  if (t770) {
  t771 = ((int64_t)(v2));
  t772 = ((int64_t)(((int64_t)UINT64_C(0))));
  t773 = gg_function_34(t771, t772, base + 0);
  t774 = ((int64_t)(t773));
  t775 = ((int64_t)(((int64_t)UINT64_C(76))));
  t770 = ((t774) == (t775));
  }
  t776 = t770;
  t777 = t776;
  if (t777) {
  t778 = ((int64_t)(v2));
  t779 = ((int64_t)(((int64_t)UINT64_C(1))));
  t780 = gg_function_34(t778, t779, base + 0);
  t781 = ((int64_t)(t780));
  t782 = ((int64_t)(((int64_t)UINT64_C(2))));
  t777 = ((t781) == (t782));
  }
  t783 = t777;
  t784 = t783;
  if (t784) {
  t785 = ((int64_t)(v2));
  t786 = ((int64_t)(((int64_t)UINT64_C(2))));
  t787 = gg_function_34(t785, t786, base + 0);
  t788 = ((int64_t)(t787));
  t789 = ((int64_t)(((int64_t)UINT64_C(91))));
  t784 = ((t788) == (t789));
  }
  t790 = t784;
  t791 = t790;
  if (t791) {
  t792 = ((int64_t)(v5));
  t793 = ((int64_t)(((int64_t)UINT64_C(1))));
  t791 = ((t792) == (t793));
  }
  t794 = t791;
  t795 = t794;
  if (t795) {
  t796 = ((int64_t)(v4));
  t797 = ((int64_t)(((int64_t)UINT64_C(0))));
  t798 = gg_function_34(t796, t797, base + 0);
  t799 = ((int64_t)(t798));
  t800 = ((int64_t)(((int64_t)UINT64_C(10))));
  t795 = ((t799) == (t800));
  }
  t801 = t795;
  if (t801) {
  {
  t802 = ((int64_t)(v0));
  t803 = ((int64_t)(v1));
  v31 = gg_function_9(t802, t803, base + 0);
  t804 = ((int64_t)(v0));
  t805 = ((int64_t)(v1));
  t806 = ((int64_t)(((int64_t)UINT64_C(1))));
  t807 = ((int64_t)(((int64_t)((uint64_t)(t805) + (uint64_t)(t806)))));
  v32 = gg_function_9(t804, t807, base + 0);
  t808 = ((int64_t)(v0));
  t809 = ((int64_t)(v1));
  t810 = ((int64_t)(((int64_t)UINT64_C(2))));
  t811 = ((int64_t)(((int64_t)((uint64_t)(t809) + (uint64_t)(t810)))));
  v33 = gg_function_9(t808, t811, base + 0);
  t812 = ((int64_t)(v33));
  t813 = ((int64_t)(((int64_t)UINT64_C(10))));
  t814 = gg_function_35(t812, t813, base + 0);
  t815 = ((int64_t)(t814));
  t816 = ((int64_t)(((int64_t)UINT64_C(0))));
  t817 = ((t815) == (t816));
  if (t817) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t818 = ((int64_t)(v31));
  t819 = gg_function_16(t818, base + 0);
  t820 = ((int64_t)(t819));
  t821 = ((int64_t)(((int64_t)UINT64_C(0))));
  t822 = ((t820) != (t821));
  t823 = t822;
  if (!t823) {
  t824 = ((int64_t)(v32));
  t825 = gg_function_16(t824, base + 0);
  t826 = ((int64_t)(t825));
  t827 = ((int64_t)(((int64_t)UINT64_C(0))));
  t823 = ((t826) != (t827));
  }
  t828 = t823;
  t829 = t828;
  if (!t829) {
  t830 = ((int64_t)(v33));
  t831 = gg_function_16(t830, base + 0);
  t832 = ((int64_t)(t831));
  t833 = ((int64_t)(((int64_t)UINT64_C(0))));
  t829 = ((t832) != (t833));
  }
  t834 = t829;
  if (t834) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t835 = ((int64_t)(v32));
  t836 = ((int64_t)(((int64_t)UINT64_C(0))));
  v34 = gg_function_14(t835, t836, base + 0);
  t837 = ((int64_t)(v32));
  t838 = ((int64_t)(((int64_t)UINT64_C(0))));
  t839 = gg_function_14(t837, t838, base + 0);
  t840 = ((int64_t)(t839));
  t841 = ((int64_t)(v31));
  t842 = ((t840) == (t841));
  if (t842) {
  {
  t843 = ((int64_t)(v32));
  t844 = ((int64_t)(((int64_t)UINT64_C(1))));
  v34 = gg_function_14(t843, t844, base + 0);
  }
  }
  t845 = ((int64_t)(v34));
  t846 = gg_function_16(t845, base + 0);
  t847 = ((int64_t)(t846));
  t848 = ((int64_t)(((int64_t)UINT64_C(0))));
  t849 = ((t847) != (t848));
  t850 = t849;
  if (!t850) {
  t851 = ((int64_t)(v34));
  t852 = ((int64_t)(((int64_t)UINT64_C(12))));
  t853 = fg14_s_contig(t851, t852);
  t854 = ((int64_t)(t853));
  t855 = ((int64_t)(((int64_t)UINT64_C(0))));
  t850 = ((t854) == (t855));
  }
  t856 = t850;
  if (t856) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t857 = ((int64_t)(v34));
  t858 = fg14_s_t_nelements(t857);
  t859 = ((int64_t)(t858));
  t860 = ((int64_t)(v31));
  t861 = ((int64_t)(((int64_t)UINT64_C(0))));
  t862 = gg_function_29(t860, t861, base + 0);
  t863 = ((int64_t)(t862));
  t864 = ((t859) != (t863));
  t865 = t864;
  if (!t865) {
  t866 = ((int64_t)(v34));
  t867 = ((int64_t)(((int64_t)UINT64_C(0))));
  t868 = gg_function_29(t866, t867, base + 0);
  t869 = ((int64_t)(t868));
  t870 = ((int64_t)(v31));
  t871 = ((int64_t)(((int64_t)UINT64_C(0))));
  t872 = gg_function_29(t870, t871, base + 0);
  t873 = ((int64_t)(t872));
  t865 = ((t869) != (t873));
  }
  t874 = t865;
  if (t874) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t875 = ((int64_t)(v3));
  t876 = ((int64_t)(((int64_t)UINT64_C(2))));
  t877 = ((t875) == (t876));
  t878 = t877;
  if (t878) {
  t879 = ((int64_t)(v2));
  t880 = ((int64_t)(((int64_t)UINT64_C(0))));
  t881 = gg_function_34(t879, t880, base + 0);
  t882 = ((int64_t)(t881));
  t883 = ((int64_t)(((int64_t)UINT64_C(91))));
  t878 = ((t882) == (t883));
  }
  t884 = t878;
  t885 = t884;
  if (t885) {
  t886 = ((int64_t)(v2));
  t887 = ((int64_t)(((int64_t)UINT64_C(1))));
  t888 = gg_function_34(t886, t887, base + 0);
  t889 = ((int64_t)(t888));
  t890 = ((int64_t)(((int64_t)UINT64_C(7))));
  t885 = ((t889) == (t890));
  }
  t891 = t885;
  t892 = t891;
  if (t892) {
  t893 = ((int64_t)(v5));
  t894 = ((int64_t)(((int64_t)UINT64_C(1))));
  t892 = ((t893) == (t894));
  }
  t895 = t892;
  if (t895) {
  {
  t896 = ((int64_t)(v4));
  t897 = ((int64_t)(((int64_t)UINT64_C(0))));
  v35 = gg_function_34(t896, t897, base + 0);
  t898 = ((int64_t)(v35));
  t899 = ((int64_t)(((int64_t)UINT64_C(10))));
  t900 = ((t898) != (t899));
  t901 = t900;
  if (t901) {
  t902 = ((int64_t)(v35));
  t903 = ((int64_t)(((int64_t)UINT64_C(7))));
  t901 = ((t902) != (t903));
  }
  t904 = t901;
  t905 = t904;
  if (t905) {
  t906 = ((int64_t)(v35));
  t907 = ((int64_t)(((int64_t)UINT64_C(15))));
  t905 = ((t906) != (t907));
  }
  t908 = t905;
  if (t908) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t909 = ((int64_t)(v0));
  t910 = ((int64_t)(v1));
  v36 = gg_function_9(t909, t910, base + 0);
  t911 = ((int64_t)(v0));
  t912 = ((int64_t)(v1));
  t913 = ((int64_t)(((int64_t)UINT64_C(1))));
  t914 = ((int64_t)(((int64_t)((uint64_t)(t912) + (uint64_t)(t913)))));
  v37 = gg_function_9(t911, t914, base + 0);
  t915 = ((int64_t)(v36));
  t916 = ((int64_t)(((int64_t)UINT64_C(0))));
  t917 = gg_function_19(t915, t916, base + 0);
  t918 = ((int64_t)(t917));
  t919 = ((int64_t)(v35));
  t920 = ((t918) != (t919));
  if (t920) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t921 = ((int64_t)(v36));
  v38 = gg_function_16(t921, base + 0);
  t922 = ((int64_t)(v38));
  t923 = ((int64_t)(((int64_t)UINT64_C(0))));
  t924 = ((t922) != (t923));
  t925 = t924;
  if (t925) {
  t926 = ((int64_t)(v38));
  t927 = ((int64_t)(((int64_t)UINT64_C(1))));
  t925 = ((t926) != (t927));
  }
  t928 = t925;
  t929 = t928;
  if (t929) {
  t930 = ((int64_t)(v38));
  t931 = ((int64_t)(((int64_t)UINT64_C(30))));
  t929 = ((t930) != (t931));
  }
  t932 = t929;
  if (t932) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t933 = ((int64_t)(v38));
  t934 = ((int64_t)(v37));
  t935 = gg_function_16(t934, base + 0);
  t936 = ((int64_t)(t935));
  t937 = ((t933) != (t936));
  if (t937) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t938 = ((int64_t)(v37));
  t939 = ((int64_t)(((int64_t)UINT64_C(0))));
  v39 = gg_function_14(t938, t939, base + 0);
  t940 = ((int64_t)(v37));
  t941 = ((int64_t)(((int64_t)UINT64_C(0))));
  t942 = gg_function_14(t940, t941, base + 0);
  t943 = ((int64_t)(t942));
  t944 = ((int64_t)(v36));
  t945 = ((t943) == (t944));
  if (t945) {
  {
  t946 = ((int64_t)(v37));
  t947 = ((int64_t)(((int64_t)UINT64_C(1))));
  v39 = gg_function_14(t946, t947, base + 0);
  }
  }
  t948 = ((int64_t)(v39));
  t949 = gg_function_16(t948, base + 0);
  t950 = ((int64_t)(t949));
  t951 = ((int64_t)(v38));
  t952 = ((t950) != (t951));
  if (t952) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t953 = ((int64_t)(v39));
  t954 = ((int64_t)(((int64_t)UINT64_C(13))));
  t955 = fg14_s_contig(t953, t954);
  t956 = ((int64_t)(t955));
  t957 = ((int64_t)(((int64_t)UINT64_C(0))));
  t958 = ((t956) == (t957));
  t959 = t958;
  if (!t959) {
  t960 = ((int64_t)(v36));
  t961 = ((int64_t)(((int64_t)UINT64_C(0))));
  t962 = gg_function_14(t960, t961, base + 0);
  t963 = ((int64_t)(((int64_t)UINT64_C(13))));
  t964 = fg14_s_contig(t962, t963);
  t965 = ((int64_t)(t964));
  t966 = ((int64_t)(((int64_t)UINT64_C(0))));
  t959 = ((t965) == (t966));
  }
  t967 = t959;
  t968 = t967;
  if (!t968) {
  t969 = ((int64_t)(v39));
  t970 = ((int64_t)(v36));
  t971 = fg14_s_t_same_shape(t969, t970);
  t972 = ((int64_t)(t971));
  t973 = ((int64_t)(((int64_t)UINT64_C(0))));
  t968 = ((t972) == (t973));
  }
  t974 = t968;
  if (t974) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t975 = ((int64_t)(v3));
  t976 = ((int64_t)(((int64_t)UINT64_C(2))));
  t977 = ((t975) == (t976));
  t978 = t977;
  if (t978) {
  t979 = ((int64_t)(v2));
  t980 = ((int64_t)(((int64_t)UINT64_C(0))));
  t981 = gg_function_34(t979, t980, base + 0);
  t982 = ((int64_t)(t981));
  t983 = ((int64_t)(((int64_t)UINT64_C(91))));
  t978 = ((t982) == (t983));
  }
  t984 = t978;
  t985 = t984;
  if (t985) {
  t986 = ((int64_t)(v2));
  t987 = ((int64_t)(((int64_t)UINT64_C(1))));
  t988 = gg_function_34(t986, t987, base + 0);
  t989 = ((int64_t)(t988));
  t990 = ((int64_t)(((int64_t)UINT64_C(9))));
  t985 = ((t989) == (t990));
  }
  t991 = t985;
  t992 = t991;
  if (t992) {
  t993 = ((int64_t)(v5));
  t994 = ((int64_t)(((int64_t)UINT64_C(1))));
  t992 = ((t993) == (t994));
  }
  t995 = t992;
  t996 = t995;
  if (t996) {
  t997 = ((int64_t)(v4));
  t998 = ((int64_t)(((int64_t)UINT64_C(0))));
  t999 = gg_function_34(t997, t998, base + 0);
  t1000 = ((int64_t)(t999));
  t1001 = ((int64_t)(((int64_t)UINT64_C(6))));
  t996 = ((t1000) == (t1001));
  }
  t1002 = t996;
  if (t1002) {
  {
  t1003 = ((int64_t)(v0));
  t1004 = ((int64_t)(v1));
  v40 = gg_function_9(t1003, t1004, base + 0);
  t1005 = ((int64_t)(v0));
  t1006 = ((int64_t)(v1));
  t1007 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1008 = ((int64_t)(((int64_t)((uint64_t)(t1006) + (uint64_t)(t1007)))));
  v41 = gg_function_9(t1005, t1008, base + 0);
  t1009 = ((int64_t)(v40));
  t1010 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1011 = gg_function_19(t1009, t1010, base + 0);
  t1012 = ((int64_t)(t1011));
  t1013 = ((int64_t)(((int64_t)UINT64_C(6))));
  t1014 = ((t1012) != (t1013));
  if (t1014) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t1015 = ((int64_t)(v40));
  t1016 = gg_function_16(t1015, base + 0);
  t1017 = ((int64_t)(t1016));
  t1018 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1019 = ((t1017) != (t1018));
  t1020 = t1019;
  if (t1020) {
  t1021 = ((int64_t)(v40));
  t1022 = gg_function_16(t1021, base + 0);
  t1023 = ((int64_t)(t1022));
  t1024 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1020 = ((t1023) != (t1024));
  }
  t1025 = t1020;
  if (t1025) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t1026 = ((int64_t)(v40));
  t1027 = gg_function_16(t1026, base + 0);
  t1028 = ((int64_t)(t1027));
  t1029 = ((int64_t)(v41));
  t1030 = gg_function_16(t1029, base + 0);
  t1031 = ((int64_t)(t1030));
  t1032 = ((t1028) != (t1031));
  if (t1032) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t1033 = ((int64_t)(v40));
  t1034 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1035 = gg_function_14(t1033, t1034, base + 0);
  t1036 = ((int64_t)(((int64_t)UINT64_C(12))));
  t1037 = fg14_s_contig(t1035, t1036);
  t1038 = ((int64_t)(t1037));
  t1039 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1040 = ((t1038) == (t1039));
  if (t1040) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t1041 = ((int64_t)(v3));
  t1042 = ((int64_t)(((int64_t)UINT64_C(3))));
  t1043 = ((t1041) == (t1042));
  t1044 = t1043;
  if (t1044) {
  t1045 = ((int64_t)(v2));
  t1046 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1047 = gg_function_34(t1045, t1046, base + 0);
  t1048 = ((int64_t)(t1047));
  t1049 = ((int64_t)(((int64_t)UINT64_C(32))));
  t1044 = ((t1048) == (t1049));
  }
  t1050 = t1044;
  t1051 = t1050;
  if (t1051) {
  t1052 = ((int64_t)(v2));
  t1053 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1054 = gg_function_34(t1052, t1053, base + 0);
  t1055 = ((int64_t)(t1054));
  t1056 = ((int64_t)(((int64_t)UINT64_C(91))));
  t1051 = ((t1055) == (t1056));
  }
  t1057 = t1051;
  t1058 = t1057;
  if (t1058) {
  t1059 = ((int64_t)(v2));
  t1060 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1061 = gg_function_34(t1059, t1060, base + 0);
  t1062 = ((int64_t)(t1061));
  t1063 = ((int64_t)(((int64_t)UINT64_C(32))));
  t1058 = ((t1062) == (t1063));
  }
  t1064 = t1058;
  t1065 = t1064;
  if (t1065) {
  t1066 = ((int64_t)(v5));
  t1067 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1065 = ((t1066) == (t1067));
  }
  t1068 = t1065;
  t1069 = t1068;
  if (t1069) {
  t1070 = ((int64_t)(v4));
  t1071 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1072 = gg_function_34(t1070, t1071, base + 0);
  t1073 = ((int64_t)(t1072));
  t1074 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1069 = ((t1073) == (t1074));
  }
  t1075 = t1069;
  if (t1075) {
  {
  t1076 = ((int64_t)(v0));
  t1077 = ((int64_t)(v1));
  v42 = gg_function_9(t1076, t1077, base + 0);
  t1078 = ((int64_t)(v0));
  t1079 = ((int64_t)(v1));
  t1080 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1081 = ((int64_t)(((int64_t)((uint64_t)(t1079) + (uint64_t)(t1080)))));
  v43 = gg_function_9(t1078, t1081, base + 0);
  t1082 = ((int64_t)(v0));
  t1083 = ((int64_t)(v1));
  t1084 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1085 = ((int64_t)(((int64_t)((uint64_t)(t1083) + (uint64_t)(t1084)))));
  v44 = gg_function_9(t1082, t1085, base + 0);
  t1086 = ((int64_t)(v42));
  t1087 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1088 = gg_function_14(t1086, t1087, base + 0);
  t1089 = gg_function_16(t1088, base + 0);
  t1090 = ((int64_t)(t1089));
  t1091 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1092 = ((t1090) != (t1091));
  t1093 = t1092;
  if (!t1093) {
  t1094 = ((int64_t)(v42));
  t1095 = gg_function_16(t1094, base + 0);
  t1096 = ((int64_t)(t1095));
  t1097 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1093 = ((t1096) != (t1097));
  }
  t1098 = t1093;
  if (t1098) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t1099 = ((int64_t)(v43));
  t1100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1101 = gg_function_19(t1099, t1100, base + 0);
  t1102 = ((int64_t)(t1101));
  t1103 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1104 = ((t1102) != (t1103));
  if (t1104) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t1105 = ((int64_t)(v42));
  t1106 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1107 = gg_function_32(t1105, t1106, base + 0);
  t1108 = ((int64_t)(t1107));
  t1109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1110 = ((t1108) != (t1109));
  t1111 = t1110;
  if (!t1111) {
  t1112 = ((int64_t)(v44));
  t1113 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1114 = gg_function_32(t1112, t1113, base + 0);
  t1115 = ((int64_t)(t1114));
  t1116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1111 = ((t1115) != (t1116));
  }
  t1117 = t1111;
  if (t1117) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
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
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
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
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(v3));
  t0 = gg_function_37(t13, t14, t15, base + 0);
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(v3));
  t0 = gg_function_40(t19, t20, t21, base + 0);
  goto gg_done;
  }
  }
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
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
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
