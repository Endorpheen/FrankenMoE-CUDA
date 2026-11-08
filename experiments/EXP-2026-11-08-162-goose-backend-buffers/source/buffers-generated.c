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
static uint8_t gg_eq_2(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint64_t value);
static uint8_t gg_eq_4(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint8_t value);
static uint8_t gg_eq_5(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static uint64_t gg_function_1(int64_t p0, int64_t base);
static int64_t gg_function_3(uint64_t p0, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t p4, int64_t base);
static void gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, int64_t base);
static void gg_function_13(int64_t p0, int64_t base);
static void gg_function_15(uint8_t p0, int64_t p1, int64_t base);
static uint64_t gg_function_18(int64_t p0, int64_t base);
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t base);
static void gg_function_21(int64_t p0, uint64_t p1, int64_t base);
static void gg_function_22(int64_t p0, int64_t p1, int64_t base);
static uint8_t gg_function_23(int64_t p0, int64_t base);
static void gg_function_25(int64_t p0, int64_t p1, int64_t base);
static void gg_function_28(int64_t p0, int64_t base);
static int64_t gg_function_30(int64_t p0, uint64_t p1, int64_t p2, int64_t base);
static uint64_t gg_function_31(int64_t p0, int64_t base);
static uint64_t gg_function_32(int64_t p0, int64_t base);
static uint64_t gg_function_33(int64_t base);
static uint64_t gg_function_34(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_35(uint8_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_36(int64_t p0, int64_t base);
static uint8_t gg_function_38(int64_t p0, int64_t base);
static uint8_t gg_function_40(int64_t p0, int64_t base);
static uint64_t gg_function_42(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint8_t p4, int64_t base);
static int64_t gg_function_43(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static uint8_t gg_function_45(uint64_t p0, int64_t base);
static int64_t gg_function_49(int64_t p0, int64_t base);
static uint64_t gg_function_50(uint64_t p0, uint64_t p1, int64_t base);
static int64_t gg_function_52(int64_t p0, int64_t p1, int64_t base);
static void gg_function_54(int64_t p0, int64_t base);
static void gg_function_55(int64_t p0, uint64_t p1, int64_t base);
static int64_t gg_function_56(int64_t p0, int64_t p1, uint64_t p2, int64_t base);
static uint8_t gg_function_40(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(74))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(29))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(30))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(16))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(69))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(70))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(18))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_3(uint64_t p0, int64_t base) {
  int64_t t0;
  uint64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(((int64_t)(v0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_15(uint8_t p0, int64_t p1, int64_t base) {
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
  fg162_fail(t2);
  }
  }
  }
gg_done:;
}
static int64_t gg_function_20(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  int64_t t18;
  uint64_t t19;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_15(t3, t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = fg162_callback(t5, t6, t7);
  t9 = ((uint64_t)(t8));
  t10 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) != (t10));
  if (t11) {
  {
  t12 = ((int64_t)(((int64_t)UINT64_C(2))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v1));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t16 = fg162_buffer_call(t12, t13, t14, t15);
  t17 = ((uint64_t)(t16));
  t0 = gg_function_3(t17, base + 0);
  }
  } else {
  {
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = fg162_constant(t18);
  t0 = ((int64_t)(((int64_t)(t19))));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_18(int64_t p0, int64_t base) {
  uint64_t t0;
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
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t v0;
  uint64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_function_15(t3, t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = fg162_is_meta(t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  t10 = t9;
  if (t10) {
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = fg162_get(t11, t12, t13, t14);
  t16 = ((uint64_t)(t15));
  t17 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t16) == (t17));
  }
  t18 = t10;
  if (t18) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = fg162_callback(t19, t20, t21);
  t23 = ((uint64_t)(t22));
  t24 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) == (t24));
  if (t25) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t30 = fg162_buffer_call(t26, t27, t28, t29);
  v1 = ((uint64_t)(t30));
  t31 = ((uint64_t)(v1));
  t32 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  t34 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_15(t33, t34, base + 0);
  t0 = ((uint64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_36(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(3))));
  t1 = ((int64_t)(v0));
  fg162_drop(t0, t1);
  }
gg_done:;
}
static void gg_function_35(uint8_t p0, int64_t p1, int64_t p2, int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(v1));
  gg_function_36(t2, base + 0);
  t3 = ((int64_t)(v2));
  fg162_fail(t3);
  }
  }
  }
gg_done:;
}
static uint8_t gg_function_38(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(36))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(39))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(37))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(38))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
  }
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_34(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint8_t t16;
  int64_t t17;
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint8_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  uint64_t t40;
  int64_t t41;
  uint8_t t42;
  uint8_t t43;
  uint8_t t44;
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
  uint64_t t55;
  uint64_t t56;
  int64_t t57;
  uint8_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_35(t7, t8, t9, base + 0);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(5))));
  t13 = fg162_callback(t10, t11, t12);
  t14 = ((uint64_t)(t13));
  t15 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v1));
  t18 = fg162_nbytes(t17);
  t0 = ((uint64_t)(t18));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(((int64_t)UINT64_C(5))));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(v1));
  t22 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t23 = fg162_buft_call(t19, t20, t21, t22);
  v3 = ((uint64_t)(t23));
  t24 = ((int64_t)(v1));
  t25 = fg162_nbytes(t24);
  v4 = ((uint64_t)(t25));
  t26 = ((uint64_t)(v3));
  t27 = ((uint64_t)(v4));
  t28 = ((t26) >= (t27));
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_35(t28, t29, t30, base + 0);
  t31 = ((uint64_t)(v3));
  t32 = ((uint64_t)(v4));
  t33 = ((t31) <= (t32));
  t34 = t33;
  if (!t34) {
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(((int64_t)UINT64_C(2))));
  t37 = ((int64_t)(((int64_t)UINT64_C(2))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = fg162_get(t35, t36, t37, t38);
  t40 = ((uint64_t)(t39));
  t41 = gg_function_3(t40, base + 0);
  t42 = gg_function_38(t41, base + 0);
  t34 = t42;
  }
  t43 = t34;
  t44 = t43;
  if (!t44) {
  t45 = ((int64_t)(v1));
  t46 = fg162_quantized(t45);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = ((t47) != (t48));
  }
  t49 = t44;
  t50 = t49;
  if (!t50) {
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((int64_t)(((int64_t)UINT64_C(2))));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = fg162_get(t51, t52, t53, t54);
  t56 = ((uint64_t)(t55));
  t57 = gg_function_3(t56, base + 0);
  t58 = gg_function_40(t57, base + 0);
  t50 = t58;
  }
  t59 = t50;
  t60 = ((int64_t)(v2));
  t61 = ((int64_t)(((int64_t)UINT64_C(7))));
  gg_function_35(t59, t60, t61, base + 0);
  t0 = ((uint64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_56(int64_t p0, int64_t p1, uint64_t p2, int64_t base) {
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
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint8_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint8_t t39;
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
  uint64_t t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  uint64_t t60;
  uint64_t t61;
  uint64_t t62;
  uint64_t t63;
  uint8_t t64;
  uint8_t t65;
  uint64_t t66;
  uint64_t t67;
  uint64_t t68;
  uint64_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  uint64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  uint64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint64_t v5;
  uint64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg162_get(t9, t10, t11, t12);
  t14 = ((uint64_t)(t13));
  t15 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t14) == (t15));
  }
  t16 = t8;
  t17 = t16;
  if (t17) {
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = fg162_get(t18, t19, t20, t21);
  t23 = ((uint64_t)(t22));
  t24 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t23) == (t24));
  }
  t25 = t17;
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(((int64_t)UINT64_C(3))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg162_get(t27, t28, t29, t30);
  t32 = ((uint64_t)(t31));
  t33 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t32) == (t33));
  }
  t34 = t26;
  t35 = ((int64_t)(((int64_t)UINT64_C(32))));
  gg_function_15(t34, t35, base + 0);
  t36 = ((int64_t)(v0));
  v3 = gg_function_18(t36, base + 0);
  t37 = ((uint64_t)(v2));
  t38 = ((uint64_t)(v3));
  t39 = ((t37) >= (t38));
  t40 = ((int64_t)(((int64_t)UINT64_C(33))));
  gg_function_15(t39, t40, base + 0);
  t41 = ((int64_t)(v0));
  t42 = fg162_is_meta(t41);
  t43 = ((int64_t)(t42));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = ((t43) == (t44));
  if (t45) {
  {
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = fg162_get(t46, t47, t48, t49);
  t51 = ((uint64_t)(t50));
  t52 = gg_function_3(t51, base + 0);
  t53 = ((int64_t)(v1));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  v4 = gg_function_34(t52, t53, t54, base + 0);
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = fg162_get(t55, t56, t57, t58);
  v5 = ((uint64_t)(t59));
  t60 = ((uint64_t)(v2));
  t61 = ((uint64_t)(v3));
  v6 = ((uint64_t)(((uint64_t)((uint64_t)(t60) - (uint64_t)(t61)))));
  t62 = ((uint64_t)(v6));
  t63 = ((uint64_t)(v5));
  t64 = ((t62) <= (t63));
  t65 = t64;
  if (t65) {
  t66 = ((uint64_t)(v4));
  t67 = ((uint64_t)(v5));
  t68 = ((uint64_t)(v6));
  t69 = ((uint64_t)(((uint64_t)((uint64_t)(t67) - (uint64_t)(t68)))));
  t65 = ((t66) <= (t69));
  }
  t70 = t65;
  t71 = ((int64_t)(((int64_t)UINT64_C(34))));
  gg_function_15(t70, t71, base + 0);
  t72 = ((int64_t)(v0));
  t73 = gg_function_18(t72, base + 0);
  (void)(t73);
  }
  }
  t74 = ((int64_t)(v1));
  t75 = ((int64_t)(((int64_t)UINT64_C(2))));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((uint64_t)(((uint64_t)(v0))));
  fg162_put(t74, t75, t76, t77, t78);
  t79 = ((int64_t)(v1));
  t80 = ((int64_t)(((int64_t)UINT64_C(2))));
  t81 = ((int64_t)(((int64_t)UINT64_C(1))));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = ((uint64_t)(v2));
  fg162_put(t79, t80, t81, t82, t83);
  t84 = ((int64_t)(v0));
  t85 = ((int64_t)(v1));
  t0 = gg_function_20(t84, t85, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_33(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_50(uint64_t p0, uint64_t p1, int64_t base) {
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
  t2 = gg_function_33(base + 0);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(v1));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((t1) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_15(t6, t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_49(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint8_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  uint64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint8_t t41;
  uint8_t t42;
  uint64_t t43;
  uint64_t t44;
  uint8_t t45;
  int64_t t46;
  uint64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint64_t t52;
  uint64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  uint64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint64_t t63;
  uint64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg162_get(t5, t6, t7, t8);
  t10 = ((uint64_t)(t9));
  t11 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t10) == (t11));
  }
  t12 = t4;
  t13 = t12;
  if (t13) {
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  t16 = ((int64_t)(((int64_t)UINT64_C(3))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg162_get(t14, t15, t16, t17);
  t19 = ((uint64_t)(t18));
  t20 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t19) != (t20));
  }
  t21 = t13;
  t22 = ((int64_t)(((int64_t)UINT64_C(30))));
  gg_function_15(t21, t22, base + 0);
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(2))));
  t25 = ((int64_t)(((int64_t)UINT64_C(3))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg162_get(t23, t24, t25, t26);
  t28 = ((uint64_t)(t27));
  v1 = gg_function_3(t28, base + 0);
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = fg162_get(t29, t30, t31, t32);
  v2 = ((uint64_t)(t33));
  t34 = ((int64_t)(v1));
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = fg162_get(t34, t35, t36, t37);
  v3 = ((uint64_t)(t38));
  t39 = ((uint64_t)(v2));
  t40 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t41 = ((t39) != (t40));
  t42 = t41;
  if (t42) {
  t43 = ((uint64_t)(v3));
  t44 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t43) != (t44));
  }
  t45 = t42;
  t46 = ((int64_t)(((int64_t)UINT64_C(31))));
  gg_function_15(t45, t46, base + 0);
  t47 = ((uint64_t)(v3));
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(2))));
  t50 = ((int64_t)(((int64_t)UINT64_C(4))));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = fg162_get(t48, t49, t50, t51);
  t53 = ((uint64_t)(t52));
  v4 = gg_function_50(t47, t53, base + 0);
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((uint64_t)(v2));
  fg162_put(t54, t55, t56, t57, t58);
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(2))));
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((uint64_t)(v4));
  fg162_put(t59, t60, t61, t62, t63);
  t64 = ((uint64_t)(v2));
  t65 = gg_function_3(t64, base + 0);
  t66 = ((int64_t)(v0));
  t0 = gg_function_20(t65, t66, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_23(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_function_15(t3, t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg162_callback(t5, t6, t7);
  t9 = ((uint64_t)(t8));
  t10 = fg162_multi_free_marker();
  t11 = ((uint64_t)(t10));
  t0 = ((t9) == (t11));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_22(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  int64_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_function_15(t2, t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(3))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((uint64_t)(((uint64_t)(v1))));
  fg162_put(t4, t5, t6, t7, t8);
  t9 = ((int64_t)(v0));
  t10 = gg_function_23(t9, base + 0);
  t11 = t10;
  if (t11) {
  {
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(v1));
  gg_function_25(t12, t13, base + 0);
  }
  } else {
  {
  t14 = ((int64_t)(v0));
  t15 = fg162_is_meta(t14);
  t16 = ((int64_t)(t15));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  if (t18) {
  {
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(v1));
  fg162_meta_usage(t19, t20);
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_25(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  uint8_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  uint64_t t25;
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
  {
  t0 = ((int64_t)(v0));
  t1 = gg_function_23(t0, base + 0);
  t2 = ((int64_t)(((int64_t)UINT64_C(29))));
  gg_function_15(t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg162_get(t3, t4, t5, t6);
  t8 = ((uint64_t)(t7));
  v2 = gg_function_3(t8, base + 0);
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg162_get(t9, t10, t11, t12);
  t14 = ((uint64_t)(t13));
  v3 = gg_function_3(t14, base + 0);
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t15 = ((uint64_t)(((uint64_t)(v4))));
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg162_get(t16, t17, t18, t19);
  t21 = ((uint64_t)(t20));
  if (!(((t15) < (t21)))) goto label2;
  {
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(v4));
  t24 = fg162_ptr(t22, t23);
  t25 = ((uint64_t)(t24));
  t26 = gg_function_3(t25, base + 0);
  t27 = ((int64_t)(v1));
  gg_function_22(t26, t27, base + 0);
  t28 = v4;
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t28) + (uint64_t)(t29)));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_21(int64_t p0, uint64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  uint64_t t24;
  int64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_function_15(t2, t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg162_get(t4, t5, t6, t7);
  t9 = ((uint64_t)(t8));
  t10 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = fg162_callback(t12, t13, t14);
  t16 = ((uint64_t)(t15));
  t17 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_15(t18, t19, base + 0);
  t20 = ((int64_t)(((int64_t)UINT64_C(3))));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((uint64_t)(v1));
  t24 = fg162_buffer_call(t20, t21, t22, t23);
  (void)(t24);
  }
gg_done:;
}
static void gg_function_55(int64_t p0, uint64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  int64_t t27;
  uint64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(28))));
  gg_function_15(t2, t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg162_get(t4, t5, t6, t7);
  t9 = ((uint64_t)(t8));
  v2 = gg_function_3(t9, base + 0);
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg162_get(t10, t11, t12, t13);
  t15 = ((uint64_t)(t14));
  v3 = gg_function_3(t15, base + 0);
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t16 = ((uint64_t)(((uint64_t)(v4))));
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg162_get(t17, t18, t19, t20);
  t22 = ((uint64_t)(t21));
  if (!(((t16) < (t22)))) goto label2;
  {
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(v4));
  t25 = fg162_ptr(t23, t24);
  t26 = ((uint64_t)(t25));
  t27 = gg_function_3(t26, base + 0);
  t28 = ((uint64_t)(v1));
  gg_function_21(t27, t28, base + 0);
  t29 = v4;
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t29) + (uint64_t)(t30)));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_13(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  goto gg_done;
  }
  }
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(1))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg162_callback(t3, t4, t5);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) != (t8));
  if (t9) {
  {
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t14 = fg162_buffer_call(t10, t11, t12, t13);
  (void)(t14);
  }
  }
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(v0));
  fg162_drop(t15, t16);
  }
gg_done:;
}
static void gg_function_54(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  uint64_t t27;
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
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(27))));
  gg_function_15(t2, t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg162_get(t4, t5, t6, t7);
  t9 = ((uint64_t)(t8));
  v1 = gg_function_3(t9, base + 0);
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg162_get(t10, t11, t12, t13);
  t15 = ((uint64_t)(t14));
  v2 = gg_function_3(t15, base + 0);
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg162_get(t16, t17, t18, t19);
  t21 = ((uint64_t)(t20));
  v3 = gg_function_3(t21, base + 0);
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(v3));
  if (!(((t22) < (t23)))) goto label2;
  {
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(v4));
  t26 = fg162_ptr(t24, t25);
  t27 = ((uint64_t)(t26));
  t28 = gg_function_3(t27, base + 0);
  gg_function_13(t28, base + 0);
  t29 = v4;
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t29) + (uint64_t)(t30)));
  }
  goto label1;
label2:;
  t31 = ((int64_t)(((int64_t)UINT64_C(2))));
  t32 = ((int64_t)(v2));
  fg162_drop(t31, t32);
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = ((int64_t)(v1));
  fg162_drop(t33, t34);
  }
gg_done:;
}
static void gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v2));
  fg162_iface_copy(t0, t1);
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((uint64_t)(((uint64_t)(v1))));
  fg162_put(t2, t3, t4, t5, t6);
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((uint64_t)(((uint64_t)(v3))));
  fg162_put(t7, t8, t9, t10, t11);
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((int64_t)(((int64_t)UINT64_C(2))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((uint64_t)(v4));
  fg162_put(t12, t13, t14, t15, t16);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(3))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = fg162_constant(t21);
  t23 = ((uint64_t)(t22));
  fg162_put(t17, t18, t19, t20, t23);
  }
gg_done:;
}
static int64_t gg_function_52(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
  uint8_t t46;
  uint64_t t47;
  uint64_t t48;
  int64_t t49;
  int64_t t50;
  uint64_t t51;
  uint64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  int64_t t64;
  uint64_t t65;
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
  uint64_t t78;
  uint64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  uint64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  int64_t t92;
  uint64_t t93;
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
  uint64_t t104;
  uint64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  uint64_t t110;
  uint64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  uint64_t t117;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  uint64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(41))));
  fg162_mark(t1);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) <= (t3));
  t5 = t4;
  if (!t5) {
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t6) == (t7));
  }
  t8 = t5;
  if (t8) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t9 = ((uint64_t)(((uint64_t)(v1))));
  t10 = gg_function_33(base + 0);
  t11 = ((uint64_t)(t10));
  t12 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t13 = ((uint64_t)(((uint64_t)gg_udiv(t11, t12))));
  t14 = ((uint64_t)(((int64_t)UINT64_C(8))));
  t15 = ((uint64_t)(((uint64_t)gg_udiv(t13, t14))));
  t16 = ((t9) > (t15));
  if (t16) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v2 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(v1));
  if (!(((t17) < (t18)))) goto label2;
  {
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(v3));
  t21 = fg162_ptr(t19, t20);
  t22 = ((uint64_t)(t21));
  v4 = gg_function_3(t22, base + 0);
  t23 = ((int64_t)(v4));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) == (t24));
  if (t25) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t26 = ((int64_t)(v5));
  t27 = ((int64_t)(v3));
  if (!(((t26) < (t27)))) goto label5;
  {
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(v5));
  t30 = fg162_ptr(t28, t29);
  t31 = ((uint64_t)(t30));
  t32 = ((uint64_t)(((uint64_t)(v4))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t34 = v5;
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  }
  goto label4;
label5:;
  t36 = ((int64_t)(v4));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((int64_t)(((int64_t)UINT64_C(2))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = fg162_get(t36, t37, t38, t39);
  v6 = ((uint64_t)(t40));
  t41 = ((uint64_t)(v2));
  t42 = gg_function_33(base + 0);
  t43 = ((uint64_t)(t42));
  t44 = ((uint64_t)(v6));
  t45 = ((uint64_t)(((uint64_t)((uint64_t)(t43) - (uint64_t)(t44)))));
  t46 = ((t41) > (t45));
  if (t46) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t47 = v2;
  t48 = ((uint64_t)(v6));
  v2 = ((uint64_t)((uint64_t)(t47) + (uint64_t)(t48)));
  t49 = v3;
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t49) + (uint64_t)(t50)));
  }
  goto label1;
label2:;
  t51 = ((uint64_t)(((uint64_t)(v1))));
  t52 = gg_function_33(base + 0);
  t53 = ((uint64_t)(t52));
  t54 = ((uint64_t)(((int64_t)UINT64_C(8))));
  t55 = ((uint64_t)(((uint64_t)gg_udiv(t53, t54))));
  t56 = ((t51) <= (t55));
  t57 = ((int64_t)(((int64_t)UINT64_C(26))));
  gg_function_15(t56, t57, base + 0);
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t59 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t60 = fg162_heap(t58, t59);
  v7 = ((int64_t)(t60));
  t61 = ((int64_t)(v7));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((t61) == (t62));
  if (t63) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t64 = ((int64_t)(((int64_t)UINT64_C(2))));
  t65 = ((uint64_t)(((uint64_t)(v1))));
  t66 = fg162_heap(t64, t65);
  v8 = ((int64_t)(t66));
  t67 = ((int64_t)(v8));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  t70 = ((int64_t)(((int64_t)UINT64_C(1))));
  t71 = ((int64_t)(v7));
  fg162_drop(t70, t71);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t72 = ((int64_t)(v3));
  t73 = ((int64_t)(v1));
  if (!(((t72) < (t73)))) goto label8;
  {
  t74 = ((int64_t)(v8));
  t75 = ((int64_t)(v3));
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(v3));
  t78 = fg162_ptr(t76, t77);
  t79 = ((uint64_t)(t78));
  fg162_ptr_put(t74, t75, t79);
  t80 = v3;
  t81 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t80) + (uint64_t)(t81)));
  }
  goto label7;
label8:;
  t82 = ((int64_t)(v7));
  t83 = ((int64_t)(((int64_t)UINT64_C(1))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = ((uint64_t)(((uint64_t)(v8))));
  fg162_put(t82, t83, t84, t85, t86);
  t87 = ((int64_t)(v7));
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  t89 = ((int64_t)(((int64_t)UINT64_C(1))));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = ((uint64_t)(((uint64_t)(v1))));
  fg162_put(t87, t88, t89, t90, t91);
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t94 = fg162_heap(t92, t93);
  v9 = ((int64_t)(t94));
  t95 = ((int64_t)(v9));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = ((t95) == (t96));
  if (t97) {
  {
  t98 = ((int64_t)(((int64_t)UINT64_C(2))));
  t99 = ((int64_t)(v8));
  fg162_drop(t98, t99);
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((int64_t)(v7));
  fg162_drop(t100, t101);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(((int64_t)UINT64_C(0))));
  t104 = fg162_ptr(t102, t103);
  t105 = ((uint64_t)(t104));
  t106 = gg_function_3(t105, base + 0);
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = ((int64_t)(((int64_t)UINT64_C(0))));
  t109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = fg162_get(t106, t107, t108, t109);
  t111 = ((uint64_t)(t110));
  v10 = gg_function_3(t111, base + 0);
  t112 = ((int64_t)(v9));
  t113 = ((int64_t)(v10));
  t114 = fg162_multi_iface();
  t115 = ((int64_t)(t114));
  t116 = ((int64_t)(v7));
  t117 = ((uint64_t)(v2));
  gg_function_6(t112, t113, t115, t116, t117, base + 0);
  t0 = ((int64_t)(v9));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_4(int64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t3 = fg162_heap(t1, t2);
  v5 = ((int64_t)(t3));
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  if (t6) {
  {
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v2));
  t11 = ((uint64_t)(v3));
  gg_function_6(t7, t8, t9, t10, t11, base + 0);
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg162_workspace(t12, t13);
  v6 = ((int64_t)(t14));
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(v2));
  t19 = ((uint64_t)(v3));
  gg_function_6(t15, t16, t17, t18, t19, base + 0);
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg162_callback(t20, t21, t22);
  t24 = ((uint64_t)(t23));
  t25 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) != (t25));
  if (t26) {
  {
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((int64_t)(v6));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t31 = fg162_buffer_call(t27, t28, t29, t30);
  (void)(t31);
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_30(int64_t p0, uint64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(16))));
  gg_function_15(t3, t4, base + 0);
  t5 = ((uint64_t)(v1));
  t6 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = fg162_workspace(t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t15 = ((int64_t)(v2));
  t0 = gg_function_4(t8, t12, t13, t14, t15, base + 0);
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = fg162_callback(t16, t17, t18);
  t20 = ((uint64_t)(t19));
  t21 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) != (t21));
  t23 = ((int64_t)(((int64_t)UINT64_C(17))));
  gg_function_15(t22, t23, base + 0);
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((uint64_t)(v1));
  t28 = fg162_buft_call(t24, t25, t26, t27);
  t29 = ((uint64_t)(t28));
  t0 = gg_function_3(t29, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_45(uint64_t p0, int64_t base) {
  uint8_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  uint8_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t v0;
  v0 = p0;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(v0));
  t7 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t8 = ((uint64_t)(((uint64_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  t9 = ((uint64_t)(((t5) & (t8))));
  t10 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t9) == (t10));
  }
  t0 = t4;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_32(int64_t p0, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  uint64_t t16;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_15(t3, t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  t8 = fg162_callback(t5, t6, t7);
  t9 = ((uint64_t)(t8));
  t10 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) != (t10));
  if (t11) {
  {
  t12 = ((int64_t)(((int64_t)UINT64_C(4))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t16 = fg162_buft_call(t12, t13, t14, t15);
  t0 = ((uint64_t)(t16));
  }
  } else {
  {
  t0 = gg_function_33(base + 0);
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_31(int64_t p0, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  uint64_t t17;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(3))));
  t8 = fg162_callback(t5, t6, t7);
  t9 = ((uint64_t)(t8));
  t10 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t9) != (t10));
  }
  t11 = t4;
  t12 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_15(t11, t12, base + 0);
  t13 = ((int64_t)(((int64_t)UINT64_C(3))));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t17 = fg162_buft_call(t13, t14, t15, t16);
  t0 = ((uint64_t)(t17));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_43(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  uint64_t t66;
  uint64_t t67;
  uint64_t t68;
  uint8_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t t75;
  uint64_t t76;
  uint64_t t77;
  uint8_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  uint64_t t82;
  uint64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint64_t t86;
  uint64_t t87;
  uint64_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  uint64_t t92;
  uint64_t t93;
  uint64_t t94;
  uint64_t t95;
  uint64_t t96;
  uint64_t t97;
  uint64_t t98;
  uint64_t t99;
  uint64_t t100;
  uint64_t t101;
  uint64_t t102;
  uint64_t t103;
  uint64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  uint64_t t108;
  uint64_t t109;
  uint8_t t110;
  uint8_t t111;
  uint64_t t112;
  uint64_t t113;
  uint64_t t114;
  uint64_t t115;
  uint8_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  uint64_t t131;
  uint64_t t132;
  uint64_t t133;
  uint64_t t134;
  uint64_t t135;
  uint64_t t136;
  uint8_t t137;
  int64_t t138;
  int64_t t139;
  uint64_t t140;
  uint64_t t141;
  int64_t t142;
  int64_t t143;
  uint64_t t144;
  uint64_t t145;
  int64_t t146;
  int64_t t147;
  uint64_t t148;
  uint64_t t149;
  uint8_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  uint64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  uint64_t t165;
  uint64_t t166;
  uint64_t t167;
  uint64_t t168;
  uint64_t t169;
  uint64_t t170;
  uint8_t t171;
  int64_t t172;
  int64_t t173;
  uint64_t t174;
  uint64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  uint64_t t180;
  int64_t t181;
  int64_t t182;
  uint64_t t183;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  uint64_t v5;
  int64_t v6;
  uint64_t v7;
  uint64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  uint64_t v13;
  uint64_t v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t8 = ((t9) <= (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) == (t14));
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t17) != (t18));
  }
  t12 = t16;
  }
  t19 = t12;
  t20 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_15(t19, t20, base + 0);
  t21 = ((int64_t)(((int64_t)UINT64_C(40))));
  fg162_mark(t21);
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(((int64_t)UINT64_C(8))));
  t24 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg162_arg_put(t22, t23, t24);
  t25 = ((int64_t)(v3));
  t26 = ((int64_t)(((int64_t)UINT64_C(9))));
  t27 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg162_arg_put(t25, t26, t27);
  t28 = ((int64_t)(v0));
  v4 = gg_function_31(t28, base + 0);
  t29 = ((int64_t)(v0));
  v5 = gg_function_32(t29, base + 0);
  t30 = ((uint64_t)(v4));
  t31 = gg_function_45(t30, base + 0);
  t32 = ((int64_t)(((int64_t)UINT64_C(19))));
  gg_function_15(t31, t32, base + 0);
  t33 = ((int64_t)(v2));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t36 = ((uint64_t)(((uint64_t)(v2))));
  t37 = gg_function_33(base + 0);
  t38 = ((uint64_t)(t37));
  t39 = ((int64_t)(((int64_t)UINT64_C(2))));
  t40 = fg162_constant(t39);
  t41 = ((uint64_t)(t40));
  t42 = ((uint64_t)(((uint64_t)gg_udiv(t38, t41))));
  t43 = ((t36) <= (t42));
  t44 = ((int64_t)(((int64_t)UINT64_C(20))));
  gg_function_15(t43, t44, base + 0);
  t45 = ((int64_t)(((int64_t)UINT64_C(3))));
  t46 = ((uint64_t)(((uint64_t)(v2))));
  t47 = fg162_heap(t45, t46);
  v6 = ((int64_t)(t47));
  t48 = ((int64_t)(v6));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((t48) == (t49));
  if (t50) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v7 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v8 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t51 = ((int64_t)(v11));
  t52 = ((int64_t)(v2));
  if (!(((t51) < (t52)))) goto label2;
  {
  t53 = ((int64_t)(v1));
  t54 = ((int64_t)(v11));
  t55 = fg162_ptr(t53, t54);
  t56 = ((uint64_t)(t55));
  v12 = gg_function_3(t56, base + 0);
  t57 = ((int64_t)(v12));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((t57) != (t58));
  t60 = ((int64_t)(v6));
  t61 = ((int64_t)(((int64_t)UINT64_C(21))));
  gg_function_35(t59, t60, t61, base + 0);
  v13 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t62 = ((int64_t)(v12));
  t63 = ((int64_t)(((int64_t)UINT64_C(2))));
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = fg162_get(t62, t63, t64, t65);
  t67 = ((uint64_t)(t66));
  t68 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t69 = ((t67) == (t68));
  t70 = t69;
  if (t70) {
  t71 = ((int64_t)(v12));
  t72 = ((int64_t)(((int64_t)UINT64_C(2))));
  t73 = ((int64_t)(((int64_t)UINT64_C(3))));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = fg162_get(t71, t72, t73, t74);
  t76 = ((uint64_t)(t75));
  t77 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t70 = ((t76) == (t77));
  }
  t78 = t70;
  if (t78) {
  {
  t79 = ((int64_t)(v0));
  t80 = ((int64_t)(v12));
  t81 = ((int64_t)(v6));
  v14 = gg_function_34(t79, t80, t81, base + 0);
  t82 = ((uint64_t)(v14));
  t83 = gg_function_33(base + 0);
  t84 = ((uint64_t)(t83));
  t85 = ((uint64_t)(v4));
  t86 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t87 = ((uint64_t)(((uint64_t)((uint64_t)(t85) - (uint64_t)(t86)))));
  t88 = ((uint64_t)(((uint64_t)((uint64_t)(t84) - (uint64_t)(t87)))));
  t89 = ((t82) <= (t88));
  t90 = ((int64_t)(v6));
  t91 = ((int64_t)(((int64_t)UINT64_C(22))));
  gg_function_35(t89, t90, t91, base + 0);
  t92 = ((uint64_t)(v14));
  t93 = ((uint64_t)(v4));
  t94 = ((uint64_t)(((uint64_t)((uint64_t)(t92) + (uint64_t)(t93)))));
  t95 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t96 = ((uint64_t)(((uint64_t)((uint64_t)(t94) - (uint64_t)(t95)))));
  t97 = ((uint64_t)(v4));
  t98 = ((uint64_t)(((uint64_t)gg_udiv(t96, t97))));
  t99 = ((uint64_t)(v4));
  v13 = ((uint64_t)(((uint64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  }
  }
  t100 = ((uint64_t)(v7));
  t101 = gg_function_33(base + 0);
  t102 = ((uint64_t)(t101));
  t103 = ((uint64_t)(v13));
  t104 = ((uint64_t)(((uint64_t)((uint64_t)(t102) - (uint64_t)(t103)))));
  t105 = ((t100) <= (t104));
  t106 = ((int64_t)(v6));
  t107 = ((int64_t)(((int64_t)UINT64_C(23))));
  gg_function_35(t105, t106, t107, base + 0);
  t108 = ((uint64_t)(v7));
  t109 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t110 = ((t108) > (t109));
  t111 = t110;
  if (t111) {
  t112 = ((uint64_t)(v7));
  t113 = ((uint64_t)(v13));
  t114 = ((uint64_t)(((uint64_t)((uint64_t)(t112) + (uint64_t)(t113)))));
  t115 = ((uint64_t)(v5));
  t111 = ((t114) > (t115));
  }
  t116 = t111;
  if (t116) {
  {
  t117 = ((int64_t)(v6));
  t118 = ((int64_t)(((int64_t)UINT64_C(3))));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((int64_t)(v10));
  t121 = ((uint64_t)(v7));
  fg162_put(t117, t118, t119, t120, t121);
  t122 = ((int64_t)(v6));
  t123 = ((int64_t)(((int64_t)UINT64_C(3))));
  t124 = ((int64_t)(((int64_t)UINT64_C(1))));
  t125 = ((int64_t)(v10));
  t126 = ((uint64_t)(((uint64_t)(v9))));
  fg162_put(t122, t123, t124, t125, t126);
  t127 = ((int64_t)(v6));
  t128 = ((int64_t)(((int64_t)UINT64_C(3))));
  t129 = ((int64_t)(((int64_t)UINT64_C(2))));
  t130 = ((int64_t)(v10));
  t131 = ((uint64_t)(((uint64_t)(v11))));
  fg162_put(t127, t128, t129, t130, t131);
  t132 = ((uint64_t)(v8));
  t133 = gg_function_33(base + 0);
  t134 = ((uint64_t)(t133));
  t135 = ((uint64_t)(v7));
  t136 = ((uint64_t)(((uint64_t)((uint64_t)(t134) - (uint64_t)(t135)))));
  t137 = ((t132) <= (t136));
  t138 = ((int64_t)(v6));
  t139 = ((int64_t)(((int64_t)UINT64_C(24))));
  gg_function_35(t137, t138, t139, base + 0);
  t140 = v8;
  t141 = ((uint64_t)(v7));
  v8 = ((uint64_t)((uint64_t)(t140) + (uint64_t)(t141)));
  t142 = v10;
  t143 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t142) + (uint64_t)(t143)));
  v7 = ((uint64_t)(v13));
  v9 = ((int64_t)(v11));
  }
  } else {
  {
  t144 = v7;
  t145 = ((uint64_t)(v13));
  v7 = ((uint64_t)((uint64_t)(t144) + (uint64_t)(t145)));
  }
  }
  t146 = v11;
  t147 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t146) + (uint64_t)(t147)));
  }
  goto label1;
label2:;
  t148 = ((uint64_t)(v7));
  t149 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t150 = ((t148) > (t149));
  if (t150) {
  {
  t151 = ((int64_t)(v6));
  t152 = ((int64_t)(((int64_t)UINT64_C(3))));
  t153 = ((int64_t)(((int64_t)UINT64_C(0))));
  t154 = ((int64_t)(v10));
  t155 = ((uint64_t)(v7));
  fg162_put(t151, t152, t153, t154, t155);
  t156 = ((int64_t)(v6));
  t157 = ((int64_t)(((int64_t)UINT64_C(3))));
  t158 = ((int64_t)(((int64_t)UINT64_C(1))));
  t159 = ((int64_t)(v10));
  t160 = ((uint64_t)(((uint64_t)(v9))));
  fg162_put(t156, t157, t158, t159, t160);
  t161 = ((int64_t)(v6));
  t162 = ((int64_t)(((int64_t)UINT64_C(3))));
  t163 = ((int64_t)(((int64_t)UINT64_C(2))));
  t164 = ((int64_t)(v10));
  t165 = ((uint64_t)(((uint64_t)(v2))));
  fg162_put(t161, t162, t163, t164, t165);
  t166 = ((uint64_t)(v8));
  t167 = gg_function_33(base + 0);
  t168 = ((uint64_t)(t167));
  t169 = ((uint64_t)(v7));
  t170 = ((uint64_t)(((uint64_t)((uint64_t)(t168) - (uint64_t)(t169)))));
  t171 = ((t166) <= (t170));
  t172 = ((int64_t)(v6));
  t173 = ((int64_t)(((int64_t)UINT64_C(25))));
  gg_function_35(t171, t172, t173, base + 0);
  t174 = v8;
  t175 = ((uint64_t)(v7));
  v8 = ((uint64_t)((uint64_t)(t174) + (uint64_t)(t175)));
  t176 = v10;
  t177 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t176) + (uint64_t)(t177)));
  }
  }
  t178 = ((int64_t)(v3));
  t179 = ((int64_t)(((int64_t)UINT64_C(8))));
  t180 = ((uint64_t)(((uint64_t)(v10))));
  fg162_arg_put(t178, t179, t180);
  t181 = ((int64_t)(v3));
  t182 = ((int64_t)(((int64_t)UINT64_C(9))));
  t183 = ((uint64_t)(v8));
  fg162_arg_put(t181, t182, t183);
  t0 = ((int64_t)(v6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_42(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint8_t p4, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  int64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  uint64_t t38;
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
  uint64_t t52;
  uint64_t t53;
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
  uint64_t t65;
  uint64_t t66;
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
  uint64_t t77;
  uint64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  uint64_t t83;
  uint64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  uint64_t t89;
  uint64_t t90;
  int64_t t91;
  uint64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint64_t t97;
  uint64_t t98;
  uint64_t t99;
  uint8_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint64_t t105;
  uint64_t t106;
  uint64_t t107;
  uint8_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint64_t t116;
  uint64_t t117;
  uint64_t t118;
  uint8_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  uint64_t t125;
  uint64_t t126;
  uint64_t t127;
  uint8_t t128;
  uint8_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint64_t t134;
  uint64_t t135;
  uint64_t t136;
  uint8_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  uint64_t t141;
  int64_t t142;
  uint8_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  uint64_t t151;
  uint64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  uint64_t t159;
  int64_t t160;
  int64_t t161;
  uint8_t t162;
  uint8_t t163;
  int64_t t164;
  int64_t t165;
  uint8_t t166;
  int64_t t167;
  int64_t t168;
  uint64_t t169;
  uint64_t t170;
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
  uint64_t t182;
  uint64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  uint64_t t193;
  uint64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint8_t v4;
  int64_t v5;
  int64_t v6;
  uint64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  uint8_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  uint8_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
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
  v5 = gg_function_43(t1, t2, t3, t4, base + 0);
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  t10 = fg162_arg(t8, t9);
  t11 = ((uint64_t)(t10));
  v6 = gg_function_3(t11, base + 0);
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(9))));
  t14 = fg162_arg(t12, t13);
  v7 = ((uint64_t)(t14));
  t15 = v4;
  if (t15) {
  {
  t16 = ((int64_t)(v5));
  gg_function_36(t16, base + 0);
  t0 = ((uint64_t)(v7));
  goto gg_done;
  }
  }
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t20 = ((int64_t)(v5));
  gg_function_36(t20, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t21 = ((int64_t)(((int64_t)UINT64_C(2))));
  t22 = ((uint64_t)(((uint64_t)(v6))));
  t23 = fg162_heap(t21, t22);
  v8 = ((int64_t)(t23));
  t24 = ((int64_t)(v8));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) == (t25));
  if (t26) {
  {
  t27 = ((int64_t)(v5));
  gg_function_36(t27, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t28 = ((uint64_t)(((uint64_t)(v2))));
  t29 = gg_function_33(base + 0);
  t30 = ((uint64_t)(t29));
  t31 = ((int64_t)(((int64_t)UINT64_C(3))));
  t32 = fg162_constant(t31);
  t33 = ((uint64_t)(t32));
  t34 = ((uint64_t)(((uint64_t)gg_udiv(t30, t33))));
  t35 = ((t28) <= (t34));
  t36 = ((int64_t)(((int64_t)UINT64_C(35))));
  gg_function_15(t35, t36, base + 0);
  t37 = ((int64_t)(((int64_t)UINT64_C(4))));
  t38 = ((uint64_t)(((uint64_t)(v2))));
  t39 = fg162_heap(t37, t38);
  v9 = ((int64_t)(t39));
  t40 = ((int64_t)(v9));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t40) == (t41));
  if (t42) {
  {
  t43 = ((int64_t)(((int64_t)UINT64_C(2))));
  t44 = ((int64_t)(v8));
  fg162_drop(t43, t44);
  t45 = ((int64_t)(v5));
  gg_function_36(t45, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t46 = ((int64_t)(v10));
  t47 = ((int64_t)(v2));
  if (!(((t46) < (t47)))) goto label2;
  {
  t48 = ((int64_t)(v9));
  t49 = ((int64_t)(v10));
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(v10));
  t52 = fg162_ptr(t50, t51);
  t53 = ((uint64_t)(t52));
  t54 = gg_function_3(t53, base + 0);
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg162_snapshot(t48, t49, t54, t55);
  t56 = v10;
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t56) + (uint64_t)(t57)));
  }
  goto label1;
label2:;
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  v12 = ((uint8_t)UINT64_C(0));
label4:;
  t58 = ((int64_t)(v11));
  t59 = ((int64_t)(v6));
  if (!(((t58) < (t59)))) goto label5;
  {
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(v5));
  t62 = ((int64_t)(((int64_t)UINT64_C(3))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((int64_t)(v11));
  t65 = fg162_get(t61, t62, t63, t64);
  t66 = ((uint64_t)(t65));
  t67 = ((int64_t)(v3));
  v13 = gg_function_30(t60, t66, t67, base + 0);
  t68 = ((int64_t)(v13));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = ((t68) == (t69));
  if (t70) {
  {
  v12 = ((uint8_t)UINT64_C(1));
  goto label5;
  }
  }
  t71 = ((int64_t)(v3));
  t72 = ((int64_t)(v13));
  fg162_tallocr_init(t71, t72);
  t73 = ((int64_t)(v5));
  t74 = ((int64_t)(((int64_t)UINT64_C(3))));
  t75 = ((int64_t)(((int64_t)UINT64_C(1))));
  t76 = ((int64_t)(v11));
  t77 = fg162_get(t73, t74, t75, t76);
  t78 = ((uint64_t)(t77));
  v14 = gg_function_3(t78, base + 0);
  t79 = ((int64_t)(v5));
  t80 = ((int64_t)(((int64_t)UINT64_C(3))));
  t81 = ((int64_t)(((int64_t)UINT64_C(2))));
  t82 = ((int64_t)(v11));
  t83 = fg162_get(t79, t80, t81, t82);
  t84 = ((uint64_t)(t83));
  v15 = gg_function_3(t84, base + 0);
  v16 = ((uint8_t)UINT64_C(0));
label7:;
  t85 = ((int64_t)(v14));
  t86 = ((int64_t)(v15));
  if (!(((t85) < (t86)))) goto label8;
  {
  t87 = ((int64_t)(v1));
  t88 = ((int64_t)(v14));
  t89 = fg162_ptr(t87, t88);
  t90 = ((uint64_t)(t89));
  v17 = gg_function_3(t90, base + 0);
  t91 = ((int64_t)(((int64_t)UINT64_C(1))));
  t92 = fg162_constant(t91);
  v18 = ((int64_t)(((int64_t)(t92))));
  t93 = ((int64_t)(v17));
  t94 = ((int64_t)(((int64_t)UINT64_C(2))));
  t95 = ((int64_t)(((int64_t)UINT64_C(1))));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = fg162_get(t93, t94, t95, t96);
  t98 = ((uint64_t)(t97));
  t99 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t100 = ((t98) == (t99));
  if (t100) {
  {
  t101 = ((int64_t)(v17));
  t102 = ((int64_t)(((int64_t)UINT64_C(2))));
  t103 = ((int64_t)(((int64_t)UINT64_C(3))));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t105 = fg162_get(t101, t102, t103, t104);
  t106 = ((uint64_t)(t105));
  t107 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t108 = ((t106) == (t107));
  if (t108) {
  {
  t109 = ((int64_t)(v3));
  t110 = ((int64_t)(v17));
  t111 = fg162_tallocr_step(t109, t110);
  v18 = ((int64_t)(t111));
  }
  } else {
  {
  t112 = ((int64_t)(v17));
  t113 = ((int64_t)(((int64_t)UINT64_C(2))));
  t114 = ((int64_t)(((int64_t)UINT64_C(0))));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = fg162_get(t112, t113, t114, t115);
  t117 = ((uint64_t)(t116));
  t118 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t119 = ((t117) == (t118));
  if (t119) {
  {
  t120 = ((int64_t)(v17));
  v18 = gg_function_49(t120, base + 0);
  }
  }
  }
  }
  }
  } else {
  {
  t121 = ((int64_t)(v17));
  t122 = ((int64_t)(((int64_t)UINT64_C(2))));
  t123 = ((int64_t)(((int64_t)UINT64_C(3))));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t125 = fg162_get(t121, t122, t123, t124);
  t126 = ((uint64_t)(t125));
  t127 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t128 = ((t126) != (t127));
  t129 = t128;
  if (t129) {
  t130 = ((int64_t)(v17));
  t131 = ((int64_t)(((int64_t)UINT64_C(2))));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t133 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = fg162_get(t130, t131, t132, t133);
  t135 = ((uint64_t)(t134));
  t136 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t129 = ((t135) == (t136));
  }
  t137 = t129;
  if (t137) {
  {
  t138 = ((int64_t)(v17));
  v18 = gg_function_49(t138, base + 0);
  }
  }
  }
  }
  t139 = ((int64_t)(v18));
  t140 = ((int64_t)(((int64_t)UINT64_C(1))));
  t141 = fg162_constant(t140);
  t142 = ((int64_t)(((int64_t)(t141))));
  t143 = ((t139) != (t142));
  if (t143) {
  {
  v16 = ((uint8_t)UINT64_C(1));
  goto label8;
  }
  }
  t144 = v14;
  t145 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t144) + (uint64_t)(t145)));
  }
  goto label7;
label8:;
  t146 = v16;
  if (t146) {
  {
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t147 = ((int64_t)(v19));
  t148 = ((int64_t)(v11));
  if (!(((t147) < (t148)))) goto label11;
  {
  t149 = ((int64_t)(v8));
  t150 = ((int64_t)(v19));
  t151 = fg162_ptr(t149, t150);
  t152 = ((uint64_t)(t151));
  t153 = gg_function_3(t152, base + 0);
  gg_function_13(t153, base + 0);
  t154 = v19;
  t155 = ((int64_t)(((int64_t)UINT64_C(1))));
  v19 = ((int64_t)((uint64_t)(t154) + (uint64_t)(t155)));
  }
  goto label10;
label11:;
  t156 = ((int64_t)(v13));
  gg_function_13(t156, base + 0);
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  v12 = ((uint8_t)UINT64_C(1));
  goto label5;
  }
  }
  t157 = ((int64_t)(v8));
  t158 = ((int64_t)(v11));
  t159 = ((uint64_t)(((uint64_t)(v13))));
  fg162_ptr_put(t157, t158, t159);
  t160 = v11;
  t161 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t160) + (uint64_t)(t161)));
  }
  goto label4;
label5:;
  v20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t162 = v12;
  t163 = (!(t162));
  if (t163) {
  {
  t164 = ((int64_t)(v6));
  t165 = ((int64_t)(((int64_t)UINT64_C(1))));
  t166 = ((t164) == (t165));
  if (t166) {
  {
  t167 = ((int64_t)(v8));
  t168 = ((int64_t)(((int64_t)UINT64_C(0))));
  t169 = fg162_ptr(t167, t168);
  t170 = ((uint64_t)(t169));
  v20 = gg_function_3(t170, base + 0);
  }
  } else {
  {
  t171 = ((int64_t)(v8));
  t172 = ((int64_t)(v6));
  v20 = gg_function_52(t171, t172, base + 0);
  t173 = ((int64_t)(v20));
  t174 = ((int64_t)(((int64_t)UINT64_C(0))));
  t175 = ((t173) == (t174));
  if (t175) {
  {
  v12 = ((uint8_t)UINT64_C(1));
  }
  }
  }
  }
  }
  }
  t176 = v12;
  if (t176) {
  {
  t177 = ((int64_t)(((int64_t)UINT64_C(42))));
  fg162_mark(t177);
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t178 = ((int64_t)(v21));
  t179 = ((int64_t)(v11));
  if (!(((t178) < (t179)))) goto label14;
  {
  t180 = ((int64_t)(v8));
  t181 = ((int64_t)(v21));
  t182 = fg162_ptr(t180, t181);
  t183 = ((uint64_t)(t182));
  t184 = gg_function_3(t183, base + 0);
  gg_function_13(t184, base + 0);
  t185 = v21;
  t186 = ((int64_t)(((int64_t)UINT64_C(1))));
  v21 = ((int64_t)((uint64_t)(t185) + (uint64_t)(t186)));
  }
  goto label13;
label14:;
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t187 = ((int64_t)(v10));
  t188 = ((int64_t)(v2));
  if (!(((t187) < (t188)))) goto label17;
  {
  t189 = ((int64_t)(v9));
  t190 = ((int64_t)(v10));
  t191 = ((int64_t)(v1));
  t192 = ((int64_t)(v10));
  t193 = fg162_ptr(t191, t192);
  t194 = ((uint64_t)(t193));
  t195 = gg_function_3(t194, base + 0);
  t196 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg162_snapshot(t189, t190, t195, t196);
  t197 = v10;
  t198 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t197) + (uint64_t)(t198)));
  }
  goto label16;
label17:;
  }
  }
  t199 = ((int64_t)(((int64_t)UINT64_C(4))));
  t200 = ((int64_t)(v9));
  fg162_drop(t199, t200);
  t201 = ((int64_t)(((int64_t)UINT64_C(2))));
  t202 = ((int64_t)(v8));
  fg162_drop(t201, t202);
  t203 = ((int64_t)(v5));
  gg_function_36(t203, base + 0);
  t0 = ((uint64_t)(((uint64_t)(v20))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_28(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(15))));
  gg_function_15(t2, t3, base + 0);
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)UINT64_C(4))));
  t7 = fg162_callback(t4, t5, t6);
  t8 = ((uint64_t)(t7));
  t9 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) != (t9));
  if (t10) {
  {
  t11 = ((int64_t)(((int64_t)UINT64_C(4))));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t15 = fg162_buffer_call(t11, t12, t13, t14);
  (void)(t15);
  }
  }
  }
gg_done:;
}
static uint64_t gg_function_1(int64_t p0, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  int64_t t25;
  uint64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
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
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
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
  uint64_t t76;
  uint64_t t77;
  int64_t t78;
  int64_t t79;
  uint8_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint8_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
  uint8_t t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  uint64_t t102;
  uint64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint64_t t112;
  uint64_t t113;
  int64_t t114;
  int64_t t115;
  uint8_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint64_t t121;
  uint64_t t122;
  uint64_t t123;
  uint8_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint64_t t128;
  uint64_t t129;
  int64_t t130;
  int64_t t131;
  uint8_t t132;
  int64_t t133;
  int64_t t134;
  uint8_t t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  uint64_t t140;
  uint64_t t141;
  uint64_t t142;
  uint8_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  uint64_t t148;
  uint64_t t149;
  int64_t t150;
  int64_t t151;
  uint8_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  uint64_t t156;
  uint64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  uint8_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  uint8_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  uint8_t t170;
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
  uint8_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  uint8_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  uint64_t t191;
  uint64_t t192;
  uint64_t t193;
  uint8_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  uint8_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  uint64_t t203;
  uint64_t t204;
  int64_t t205;
  int64_t t206;
  uint64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  uint8_t t212;
  int64_t t213;
  int64_t t214;
  uint8_t t215;
  int64_t t216;
  int64_t t217;
  uint8_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  uint64_t t223;
  uint64_t t224;
  uint64_t t225;
  uint8_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  uint64_t t230;
  uint64_t t231;
  int64_t t232;
  int64_t t233;
  uint8_t t234;
  int64_t t235;
  int64_t t236;
  uint8_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  uint64_t t243;
  int64_t t244;
  int64_t t245;
  uint8_t t246;
  uint8_t t247;
  int64_t t248;
  int64_t t249;
  uint8_t t250;
  int64_t t251;
  int64_t t252;
  uint64_t t253;
  int64_t t254;
  int64_t t255;
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
  uint8_t t271;
  int64_t t272;
  int64_t t273;
  int64_t t274;
  uint64_t t275;
  uint64_t t276;
  int64_t t277;
  int64_t t278;
  uint8_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  uint8_t t284;
  int64_t t285;
  uint8_t t286;
  uint8_t t287;
  int64_t t288;
  int64_t t289;
  uint8_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  uint8_t t295;
  int64_t t296;
  int64_t t297;
  uint64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  uint8_t t302;
  int64_t t303;
  uint8_t t304;
  uint8_t t305;
  int64_t t306;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  uint64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg162_arg(t1, t2);
  t4 = ((uint64_t)(t3));
  v1 = gg_function_3(t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = fg162_arg(t5, t6);
  t8 = ((uint64_t)(t7));
  v2 = gg_function_3(t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = fg162_arg(t9, t10);
  t12 = ((uint64_t)(t11));
  v3 = gg_function_3(t12, base + 0);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = fg162_arg(t13, t14);
  v4 = ((uint64_t)(t15));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  t18 = fg162_arg(t16, t17);
  v5 = ((uint64_t)(t18));
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(v3));
  t24 = ((uint64_t)(v4));
  t25 = gg_function_3(t24, base + 0);
  t26 = ((uint64_t)(v5));
  t27 = ((int64_t)(v0));
  t28 = gg_function_4(t22, t23, t25, t26, t27, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t28))));
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v2));
  gg_function_13(t32, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = ((t33) == (t34));
  t36 = t35;
  if (!t36) {
  t37 = ((int64_t)(v1));
  t38 = ((int64_t)(((int64_t)UINT64_C(7))));
  t36 = ((t37) == (t38));
  }
  t39 = t36;
  t40 = t39;
  if (!t40) {
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(((int64_t)UINT64_C(8))));
  t40 = ((t41) == (t42));
  }
  t43 = t40;
  if (t43) {
  {
  t44 = ((int64_t)(v2));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((t44) != (t45));
  t47 = ((int64_t)(((int64_t)UINT64_C(36))));
  gg_function_15(t46, t47, base + 0);
  t48 = ((int64_t)(v2));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((t51) == (t52));
  if (t53) {
  {
  t50 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  t54 = ((int64_t)(v1));
  t55 = ((int64_t)(((int64_t)UINT64_C(7))));
  t56 = ((t54) == (t55));
  if (t56) {
  {
  t50 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  } else {
  {
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  }
  }
  t57 = ((int64_t)(t50));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = fg162_get(t48, t49, t57, t58);
  t0 = ((uint64_t)(t59));
  goto gg_done;
  }
  }
  t60 = ((int64_t)(v1));
  t61 = ((int64_t)(((int64_t)UINT64_C(3))));
  t62 = ((t60) == (t61));
  if (t62) {
  {
  t63 = ((int64_t)(v2));
  t0 = gg_function_18(t63, base + 0);
  goto gg_done;
  }
  }
  t64 = ((int64_t)(v1));
  t65 = ((int64_t)(((int64_t)UINT64_C(4))));
  t66 = ((t64) == (t65));
  if (t66) {
  {
  t67 = ((int64_t)(v2));
  t68 = ((int64_t)(v3));
  t69 = gg_function_20(t67, t68, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t69))));
  goto gg_done;
  }
  }
  t70 = ((int64_t)(v1));
  t71 = ((int64_t)(((int64_t)UINT64_C(5))));
  t72 = ((t70) == (t71));
  if (t72) {
  {
  t73 = ((int64_t)(v2));
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(2))));
  t76 = fg162_arg(t74, t75);
  t77 = ((uint64_t)(t76));
  gg_function_21(t73, t77, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t78 = ((int64_t)(v1));
  t79 = ((int64_t)(((int64_t)UINT64_C(6))));
  t80 = ((t78) == (t79));
  if (t80) {
  {
  t81 = ((int64_t)(v2));
  t82 = ((int64_t)(v3));
  gg_function_22(t81, t82, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t83 = ((int64_t)(v1));
  t84 = ((int64_t)(((int64_t)UINT64_C(9))));
  t85 = ((t83) == (t84));
  if (t85) {
  {
  t86 = ((int64_t)(v2));
  gg_function_28(t86, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t87 = ((int64_t)(v1));
  t88 = ((int64_t)(((int64_t)UINT64_C(10))));
  t89 = ((t87) == (t88));
  if (t89) {
  {
  t90 = ((int64_t)(v2));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((t90) != (t91));
  t93 = t92;
  if (t93) {
  t94 = ((int64_t)(v3));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t94) != (t95));
  }
  t96 = t93;
  t97 = ((int64_t)(((int64_t)UINT64_C(37))));
  gg_function_15(t96, t97, base + 0);
  t98 = ((int64_t)(v3));
  t99 = ((int64_t)(((int64_t)UINT64_C(2))));
  t100 = ((int64_t)(((int64_t)UINT64_C(3))));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = fg162_get(t98, t99, t100, t101);
  t103 = ((uint64_t)(t102));
  v6 = gg_function_3(t103, base + 0);
  t105 = ((int64_t)(v6));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = ((t105) != (t106));
  if (t107) {
  {
  t104 = ((int64_t)(v6));
  }
  } else {
  {
  t104 = ((int64_t)(v3));
  }
  }
  t108 = ((int64_t)(t104));
  t109 = ((int64_t)(((int64_t)UINT64_C(2))));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = fg162_get(t108, t109, t110, t111);
  t113 = ((uint64_t)(t112));
  v7 = gg_function_3(t113, base + 0);
  t114 = ((int64_t)(v7));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((t114) != (t115));
  t117 = ((int64_t)(((int64_t)UINT64_C(38))));
  gg_function_15(t116, t117, base + 0);
  t118 = ((int64_t)(v7));
  t119 = ((int64_t)(((int64_t)UINT64_C(1))));
  t120 = ((int64_t)(((int64_t)UINT64_C(5))));
  t121 = fg162_callback(t118, t119, t120);
  t122 = ((uint64_t)(t121));
  t123 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t124 = ((t122) != (t123));
  if (t124) {
  {
  t125 = ((int64_t)(((int64_t)UINT64_C(5))));
  t126 = ((int64_t)(v7));
  t127 = ((int64_t)(v2));
  t128 = ((uint64_t)(((uint64_t)(v3))));
  t129 = fg162_buffer_call(t125, t126, t127, t128);
  t0 = ((uint64_t)(t129));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto gg_done;
  }
  }
  t130 = ((int64_t)(v1));
  t131 = ((int64_t)(((int64_t)UINT64_C(11))));
  t132 = ((t130) == (t131));
  if (t132) {
  {
  t133 = ((int64_t)(v2));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((t133) != (t134));
  t136 = t135;
  if (t136) {
  t137 = ((int64_t)(v2));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((int64_t)(((int64_t)UINT64_C(0))));
  t140 = fg162_callback(t137, t138, t139);
  t141 = ((uint64_t)(t140));
  t142 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t136 = ((t141) != (t142));
  }
  t143 = t136;
  t144 = ((int64_t)(((int64_t)UINT64_C(39))));
  gg_function_15(t143, t144, base + 0);
  t145 = ((int64_t)(((int64_t)UINT64_C(0))));
  t146 = ((int64_t)(v2));
  t147 = ((int64_t)(((int64_t)UINT64_C(0))));
  t148 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t149 = fg162_buft_call(t145, t146, t147, t148);
  t0 = ((uint64_t)(t149));
  goto gg_done;
  }
  }
  t150 = ((int64_t)(v1));
  t151 = ((int64_t)(((int64_t)UINT64_C(12))));
  t152 = ((t150) == (t151));
  if (t152) {
  {
  t153 = ((int64_t)(v2));
  t154 = ((int64_t)(v0));
  t155 = ((int64_t)(((int64_t)UINT64_C(2))));
  t156 = fg162_arg(t154, t155);
  t157 = ((uint64_t)(t156));
  t158 = ((int64_t)(v0));
  t159 = gg_function_30(t153, t157, t158, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t159))));
  goto gg_done;
  }
  }
  t160 = ((int64_t)(v1));
  t161 = ((int64_t)(((int64_t)UINT64_C(13))));
  t162 = ((t160) == (t161));
  if (t162) {
  {
  t163 = ((int64_t)(v2));
  t0 = gg_function_31(t163, base + 0);
  goto gg_done;
  }
  }
  t164 = ((int64_t)(v1));
  t165 = ((int64_t)(((int64_t)UINT64_C(14))));
  t166 = ((t164) == (t165));
  if (t166) {
  {
  t167 = ((int64_t)(v2));
  t0 = gg_function_32(t167, base + 0);
  goto gg_done;
  }
  }
  t168 = ((int64_t)(v1));
  t169 = ((int64_t)(((int64_t)UINT64_C(15))));
  t170 = ((t168) == (t169));
  if (t170) {
  {
  t171 = ((int64_t)(v2));
  t172 = ((int64_t)(v3));
  t173 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = gg_function_34(t171, t172, t173, base + 0);
  goto gg_done;
  }
  }
  t174 = ((int64_t)(v1));
  t175 = ((int64_t)(((int64_t)UINT64_C(16))));
  t176 = ((t174) == (t175));
  t177 = t176;
  if (!t177) {
  t178 = ((int64_t)(v1));
  t179 = ((int64_t)(((int64_t)UINT64_C(17))));
  t177 = ((t178) == (t179));
  }
  t180 = t177;
  if (t180) {
  {
  t181 = ((int64_t)(v2));
  t182 = ((int64_t)(((int64_t)UINT64_C(0))));
  t183 = ((t181) != (t182));
  t184 = ((int64_t)(((int64_t)UINT64_C(40))));
  gg_function_15(t183, t184, base + 0);
  t185 = ((int64_t)(v1));
  t186 = ((int64_t)(((int64_t)UINT64_C(16))));
  t187 = ((t185) == (t186));
  if (t187) {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  v8 = ((int64_t)(((int64_t)UINT64_C(6))));
  }
  }
  t188 = ((int64_t)(v2));
  t189 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = ((int64_t)(v8));
  t191 = fg162_callback(t188, t189, t190);
  t192 = ((uint64_t)(t191));
  t193 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t194 = ((t192) != (t193));
  if (t194) {
  {
  t196 = ((int64_t)(v1));
  t197 = ((int64_t)(((int64_t)UINT64_C(16))));
  t198 = ((t196) == (t197));
  if (t198) {
  {
  t195 = ((int64_t)(((int64_t)UINT64_C(43))));
  }
  } else {
  {
  t195 = ((int64_t)(((int64_t)UINT64_C(44))));
  }
  }
  t199 = ((int64_t)(t195));
  fg162_mark(t199);
  t200 = ((int64_t)(v8));
  t201 = ((int64_t)(v2));
  t202 = ((int64_t)(v3));
  t203 = ((uint64_t)(v4));
  t204 = fg162_buft_call(t200, t201, t202, t203);
  t0 = ((uint64_t)(t204));
  goto gg_done;
  }
  }
  t205 = ((int64_t)(v2));
  t206 = ((int64_t)(v3));
  t207 = ((uint64_t)(v4));
  t208 = gg_function_3(t207, base + 0);
  t209 = ((int64_t)(v0));
  t210 = ((int64_t)(v1));
  t211 = ((int64_t)(((int64_t)UINT64_C(17))));
  t212 = ((t210) == (t211));
  t0 = gg_function_42(t205, t206, t208, t209, t212, base + 0);
  goto gg_done;
  }
  }
  t213 = ((int64_t)(v1));
  t214 = ((int64_t)(((int64_t)UINT64_C(18))));
  t215 = ((t213) == (t214));
  if (t215) {
  {
  t216 = ((int64_t)(v2));
  t217 = ((int64_t)(((int64_t)UINT64_C(0))));
  t218 = ((t216) != (t217));
  t219 = ((int64_t)(((int64_t)UINT64_C(41))));
  gg_function_15(t218, t219, base + 0);
  t220 = ((int64_t)(v2));
  t221 = ((int64_t)(((int64_t)UINT64_C(0))));
  t222 = ((int64_t)(((int64_t)UINT64_C(7))));
  t223 = fg162_callback(t220, t221, t222);
  t224 = ((uint64_t)(t223));
  t225 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t226 = ((t224) != (t225));
  if (t226) {
  {
  t227 = ((int64_t)(((int64_t)UINT64_C(7))));
  t228 = ((int64_t)(v2));
  t229 = ((int64_t)(((int64_t)UINT64_C(0))));
  t230 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t231 = fg162_buft_call(t227, t228, t229, t230);
  t0 = ((uint64_t)(t231));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto gg_done;
  }
  }
  t232 = ((int64_t)(v1));
  t233 = ((int64_t)(((int64_t)UINT64_C(19))));
  t234 = ((t232) == (t233));
  if (t234) {
  {
  t235 = ((int64_t)(v2));
  t236 = ((int64_t)(((int64_t)UINT64_C(0))));
  t237 = ((t235) != (t236));
  t238 = ((int64_t)(((int64_t)UINT64_C(42))));
  gg_function_15(t237, t238, base + 0);
  t239 = ((int64_t)(v2));
  t240 = ((int64_t)(((int64_t)UINT64_C(4))));
  t241 = ((int64_t)(((int64_t)UINT64_C(0))));
  t242 = ((int64_t)(((int64_t)UINT64_C(0))));
  t243 = fg162_get(t239, t240, t241, t242);
  t0 = ((uint64_t)(t243));
  goto gg_done;
  }
  }
  t244 = ((int64_t)(v1));
  t245 = ((int64_t)(((int64_t)UINT64_C(20))));
  t246 = ((t244) == (t245));
  t247 = t246;
  if (!t247) {
  t248 = ((int64_t)(v1));
  t249 = ((int64_t)(((int64_t)UINT64_C(21))));
  t247 = ((t248) == (t249));
  }
  t250 = t247;
  if (t250) {
  {
  t251 = ((int64_t)(v2));
  t252 = ((int64_t)(v3));
  t253 = ((uint64_t)(v4));
  t254 = gg_function_3(t253, base + 0);
  t255 = ((int64_t)(v0));
  t256 = ((int64_t)(v1));
  t257 = ((int64_t)(((int64_t)UINT64_C(21))));
  t258 = ((t256) == (t257));
  t0 = gg_function_42(t251, t252, t254, t255, t258, base + 0);
  goto gg_done;
  }
  }
  t259 = ((int64_t)(v1));
  t260 = ((int64_t)(((int64_t)UINT64_C(22))));
  t261 = ((t259) == (t260));
  if (t261) {
  {
  t262 = ((int64_t)(v2));
  t263 = ((int64_t)(v3));
  t264 = gg_function_52(t262, t263, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t264))));
  goto gg_done;
  }
  }
  t265 = ((int64_t)(v1));
  t266 = ((int64_t)(((int64_t)UINT64_C(23))));
  t267 = ((t265) == (t266));
  if (t267) {
  {
  t268 = ((int64_t)(v2));
  gg_function_54(t268, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t269 = ((int64_t)(v1));
  t270 = ((int64_t)(((int64_t)UINT64_C(24))));
  t271 = ((t269) == (t270));
  if (t271) {
  {
  t272 = ((int64_t)(v2));
  t273 = ((int64_t)(v0));
  t274 = ((int64_t)(((int64_t)UINT64_C(2))));
  t275 = fg162_arg(t273, t274);
  t276 = ((uint64_t)(t275));
  gg_function_55(t272, t276, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t277 = ((int64_t)(v1));
  t278 = ((int64_t)(((int64_t)UINT64_C(25))));
  t279 = ((t277) == (t278));
  if (t279) {
  {
  t280 = ((int64_t)(v2));
  t281 = ((int64_t)(v3));
  gg_function_25(t280, t281, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t282 = ((int64_t)(v1));
  t283 = ((int64_t)(((int64_t)UINT64_C(26))));
  t284 = ((t282) == (t283));
  if (t284) {
  {
  t285 = ((int64_t)(v2));
  t286 = gg_function_23(t285, base + 0);
  t287 = t286;
  if (t287) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto gg_done;
  }
  }
  t288 = ((int64_t)(v1));
  t289 = ((int64_t)(((int64_t)UINT64_C(27))));
  t290 = ((t288) == (t289));
  if (t290) {
  {
  t291 = ((int64_t)(v2));
  t292 = gg_function_49(t291, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t292))));
  goto gg_done;
  }
  }
  t293 = ((int64_t)(v1));
  t294 = ((int64_t)(((int64_t)UINT64_C(28))));
  t295 = ((t293) == (t294));
  if (t295) {
  {
  t296 = ((int64_t)(v2));
  t297 = ((int64_t)(v3));
  t298 = ((uint64_t)(v4));
  t299 = gg_function_56(t296, t297, t298, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t299))));
  goto gg_done;
  }
  }
  t300 = ((int64_t)(v1));
  t301 = ((int64_t)(((int64_t)UINT64_C(29))));
  t302 = ((t300) == (t301));
  if (t302) {
  {
  t303 = ((int64_t)(v2));
  t304 = gg_function_40(t303, base + 0);
  t305 = t304;
  if (t305) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto gg_done;
  }
  }
  t306 = ((int64_t)(((int64_t)UINT64_C(99))));
  fg162_fail(t306);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  gg_header t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
  {
  t0.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = gg_function_1(t1, base + 1);
  gg_unsigned(gg_ref(&t0, gg_get_stack(base + 0)), t2);
  gg_append(gg_ref(&t0, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t0).base, (t0).len));
  }
gg_done:;
  for (t3 = 0; t3 < 1; t3++) gg_stacks[base + t3].top = 0;
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
