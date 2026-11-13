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
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_3(uint64_t p0, int64_t base);
static int64_t gg_function_4(int64_t p0, int64_t p1, uint64_t p2, int64_t base);
static uint64_t gg_function_6(int64_t base);
static void gg_function_7(uint8_t p0, int64_t p1, int64_t base);
static uint64_t gg_function_11(uint64_t p0, uint64_t p1, int64_t base);
static int64_t gg_function_17(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, uint64_t p5, int64_t base);
static uint64_t gg_function_22(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t gg_function_25(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_27(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_32(int64_t p0, int64_t base);
static int64_t gg_function_33(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, int64_t base);
static int64_t gg_function_34(int64_t p0, int64_t base);
static int64_t gg_function_35(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_36(int64_t p0, int64_t p1, int64_t p2, int64_t base);
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
static void gg_function_7(uint8_t p0, int64_t p1, int64_t base) {
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
  fg156_fail(t2);
  }
  }
  }
gg_done:;
}
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(v1));
  t4 = fg156_tget(t1, t2, t3);
  t0 = ((int64_t)(((int64_t)(t4))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_6(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_22(uint64_t p0, uint64_t p1, int64_t base) {
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
  t6 = gg_function_6(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_function_7(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_25(int64_t p0, int64_t p1, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((int64_t)(v1));
  t4 = fg156_tget(t1, t2, t3);
  t0 = ((uint64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_11(uint64_t p0, uint64_t p1, int64_t base) {
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
  t1 = ((uint64_t)(v1));
  t2 = gg_function_6(base + 0);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(v0));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((t1) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_7(t6, t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_4(int64_t p0, int64_t p1, uint64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  int64_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint8_t t42;
  int64_t t43;
  uint64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  uint64_t t48;
  uint64_t t49;
  uint64_t t50;
  uint64_t t51;
  uint64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint8_t t56;
  int64_t t57;
  uint64_t t58;
  int64_t t59;
  int64_t t60;
  uint64_t t61;
  uint64_t t62;
  uint64_t t63;
  uint64_t t64;
  uint64_t t65;
  int64_t t66;
  int64_t t67;
  uint64_t t68;
  uint64_t t69;
  uint8_t t70;
  int64_t t71;
  uint64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  uint64_t t80;
  int64_t t81;
  uint64_t t82;
  int64_t t83;
  int64_t t84;
  uint64_t t85;
  int64_t t86;
  int64_t t87;
  uint64_t t88;
  uint64_t t89;
  uint64_t t90;
  int64_t t91;
  int64_t t92;
  uint64_t t93;
  int64_t t94;
  int64_t t95;
  uint64_t t96;
  int64_t t97;
  int64_t t98;
  uint64_t t99;
  int64_t t100;
  int64_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  uint64_t t105;
  int64_t t106;
  int64_t t107;
  uint64_t t108;
  int64_t t109;
  int64_t t110;
  uint64_t t111;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  int64_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
  uint64_t v8;
  uint64_t v9;
  int64_t v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg156_constant(t1);
  v3 = ((uint64_t)(t2));
  t3 = ((uint64_t)(v2));
  t4 = gg_function_6(base + 0);
  t5 = ((uint64_t)(t4));
  t6 = ((uint64_t)(v3));
  t7 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t8 = ((uint64_t)(((uint64_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  t9 = ((uint64_t)(((uint64_t)((uint64_t)(t5) - (uint64_t)(t8)))));
  t10 = ((t3) <= (t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_7(t10, t11, base + 0);
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(3))));
  t14 = fg156_ctx(t12, t13);
  t15 = ((uint64_t)(t14));
  v4 = gg_function_3(t15, base + 0);
  v5 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  if (t18) {
  {
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = fg156_obj(t19, t20);
  t22 = ((uint64_t)(t21));
  t23 = ((int64_t)(v4));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = fg156_obj(t23, t24);
  t26 = ((uint64_t)(t25));
  v5 = gg_function_11(t22, t26, base + 0);
  }
  }
  t27 = ((uint64_t)(v2));
  t28 = ((uint64_t)(v3));
  t29 = ((uint64_t)(((uint64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t31 = ((uint64_t)(((uint64_t)((uint64_t)(t29) - (uint64_t)(t30)))));
  t32 = ((uint64_t)(v3));
  t33 = ((uint64_t)(((uint64_t)gg_udiv(t31, t32))));
  t34 = ((uint64_t)(v3));
  v6 = ((uint64_t)(((uint64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  t36 = fg156_constant(t35);
  v7 = ((uint64_t)(t36));
  t37 = ((uint64_t)(v6));
  t38 = gg_function_6(base + 0);
  t39 = ((uint64_t)(t38));
  t40 = ((uint64_t)(v5));
  t41 = ((uint64_t)(((uint64_t)((uint64_t)(t39) - (uint64_t)(t40)))));
  t42 = ((t37) > (t41));
  if (t42) {
  {
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = ((uint64_t)(v6));
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = fg156_ctx(t45, t46);
  t48 = ((uint64_t)(t47));
  fg156_warn_arena(t43, t44, t48);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t49 = ((uint64_t)(v5));
  t50 = ((uint64_t)(v6));
  v8 = ((uint64_t)(((uint64_t)((uint64_t)(t49) + (uint64_t)(t50)))));
  t51 = ((uint64_t)(v7));
  t52 = gg_function_6(base + 0);
  t53 = ((uint64_t)(t52));
  t54 = ((uint64_t)(v8));
  t55 = ((uint64_t)(((uint64_t)((uint64_t)(t53) - (uint64_t)(t54)))));
  t56 = ((t51) > (t55));
  if (t56) {
  {
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((uint64_t)(v6));
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = fg156_ctx(t59, t60);
  t62 = ((uint64_t)(t61));
  fg156_warn_arena(t57, t58, t62);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t63 = ((uint64_t)(v8));
  t64 = ((uint64_t)(v7));
  v9 = ((uint64_t)(((uint64_t)((uint64_t)(t63) + (uint64_t)(t64)))));
  t65 = ((uint64_t)(v9));
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = fg156_ctx(t66, t67);
  t69 = ((uint64_t)(t68));
  t70 = ((t65) > (t69));
  if (t70) {
  {
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  t72 = ((uint64_t)(v9));
  t73 = ((int64_t)(v0));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = fg156_ctx(t73, t74);
  t76 = ((uint64_t)(t75));
  fg156_warn_arena(t71, t72, t76);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  t79 = fg156_ctx(t77, t78);
  t80 = ((uint64_t)(t79));
  t81 = gg_function_3(t80, base + 0);
  t82 = ((uint64_t)(v5));
  t83 = fg156_ptr_add(t81, t82);
  v10 = ((int64_t)(t83));
  t84 = ((int64_t)(v10));
  t85 = ((uint64_t)(v7));
  fg156_zero(t84, t85);
  t86 = ((int64_t)(v10));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((uint64_t)(v5));
  t89 = ((uint64_t)(v7));
  t90 = ((uint64_t)(((uint64_t)((uint64_t)(t88) + (uint64_t)(t89)))));
  fg156_obj_set(t86, t87, t90);
  t91 = ((int64_t)(v10));
  t92 = ((int64_t)(((int64_t)UINT64_C(1))));
  t93 = ((uint64_t)(v6));
  fg156_obj_set(t91, t92, t93);
  t94 = ((int64_t)(v10));
  t95 = ((int64_t)(((int64_t)UINT64_C(2))));
  t96 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg156_obj_set(t94, t95, t96);
  t97 = ((int64_t)(v10));
  t98 = ((int64_t)(((int64_t)UINT64_C(3))));
  t99 = ((uint64_t)(((uint64_t)(v1))));
  fg156_obj_set(t97, t98, t99);
  t100 = ((int64_t)(v4));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t100) != (t101));
  if (t102) {
  {
  t103 = ((int64_t)(v4));
  t104 = ((int64_t)(((int64_t)UINT64_C(2))));
  t105 = ((uint64_t)(((uint64_t)(v10))));
  fg156_obj_set(t103, t104, t105);
  }
  } else {
  {
  t106 = ((int64_t)(v0));
  t107 = ((int64_t)(((int64_t)UINT64_C(2))));
  t108 = ((uint64_t)(((uint64_t)(v10))));
  fg156_ctx_set(t106, t107, t108);
  }
  }
  t109 = ((int64_t)(v0));
  t110 = ((int64_t)(((int64_t)UINT64_C(3))));
  t111 = ((uint64_t)(((uint64_t)(v10))));
  fg156_ctx_set(t109, t110, t111);
  t0 = ((int64_t)(v10));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_17(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, uint64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  uint64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint8_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint64_t t45;
  uint64_t t46;
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
  uint8_t t65;
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
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint64_t t83;
  uint64_t t84;
  uint64_t t85;
  int64_t t86;
  int64_t t87;
  uint64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  uint64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  uint8_t t98;
  uint64_t t99;
  uint64_t t100;
  uint8_t t101;
  uint64_t t102;
  uint64_t t103;
  uint64_t t104;
  uint64_t t105;
  int64_t t106;
  uint64_t t107;
  uint64_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  uint8_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  uint64_t t118;
  uint64_t t119;
  uint64_t t120;
  uint8_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  uint64_t t125;
  uint64_t t126;
  uint64_t t127;
  uint64_t t128;
  int64_t t129;
  int64_t t130;
  uint8_t t131;
  uint8_t t132;
  int64_t t133;
  int64_t t134;
  uint64_t t135;
  uint64_t t136;
  uint64_t t137;
  uint8_t t138;
  int64_t t139;
  uint64_t t140;
  uint64_t t141;
  uint64_t t142;
  int64_t t143;
  int64_t t144;
  uint64_t t145;
  int64_t t146;
  uint64_t t147;
  int64_t t148;
  int64_t t149;
  uint8_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  uint64_t t154;
  uint64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  uint64_t t159;
  uint64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  uint64_t t164;
  uint64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  uint64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  uint64_t t177;
  uint64_t t178;
  uint64_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  uint64_t t183;
  uint64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  uint64_t t189;
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
  uint64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  uint64_t t208;
  uint64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  uint64_t t214;
  uint64_t t215;
  int64_t t216;
  int64_t t217;
  uint64_t t218;
  uint64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  uint64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  uint64_t t235;
  uint64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  uint64_t t241;
  uint64_t t242;
  uint64_t t243;
  uint8_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  uint64_t t250;
  uint64_t t251;
  uint64_t t252;
  uint64_t t253;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint64_t v5;
  int64_t v6;
  int64_t v7;
  uint64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  uint64_t v13;
  int64_t v14;
  uint64_t v15;
  uint64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((uint64_t)(((uint64_t)(v1))));
  t6 = ((int64_t)(((int64_t)UINT64_C(3))));
  t7 = fg156_constant(t6);
  t8 = ((uint64_t)(t7));
  t4 = ((t5) < (t8));
  }
  t9 = t4;
  t10 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_7(t9, t10, base + 0);
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = ((t11) >= (t12));
  t14 = t13;
  if (t14) {
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(4))));
  t14 = ((t15) <= (t16));
  }
  t17 = t14;
  t18 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_7(t17, t18, base + 0);
  t19 = ((int64_t)(v1));
  t20 = fg156_type_block(t19);
  v6 = ((int64_t)(t20));
  t21 = ((int64_t)(v6));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) > (t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_function_7(t23, t24, base + 0);
  v7 = ((int64_t)(v4));
  v8 = ((uint64_t)(v5));
  t25 = ((int64_t)(v7));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t25) != (t26));
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(v7));
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = fg156_tget(t29, t30, t31);
  t33 = ((uint64_t)(t32));
  t34 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t33) != (t34));
  }
  t35 = t28;
  if (t35) {
  {
  t36 = ((uint64_t)(v8));
  t37 = ((int64_t)(v7));
  t38 = ((int64_t)(((int64_t)UINT64_C(3))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = fg156_tget(t37, t38, t39);
  t41 = ((uint64_t)(t40));
  v8 = gg_function_11(t36, t41, base + 0);
  t42 = ((int64_t)(v7));
  t43 = ((int64_t)(((int64_t)UINT64_C(2))));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = fg156_tget(t42, t43, t44);
  t46 = ((uint64_t)(t45));
  v7 = gg_function_3(t46, base + 0);
  }
  }
  t47 = ((int64_t)(v3));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = fg156_dim(t47, t48);
  v9 = ((int64_t)(t49));
  t50 = ((int64_t)(v9));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((t50) >= (t51));
  t53 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_7(t52, t53, base + 0);
  v10 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t54 = ((int64_t)(v10));
  t55 = ((int64_t)(v2));
  if (!(((t54) < (t55)))) goto label2;
  {
  t56 = ((int64_t)(v3));
  t57 = ((int64_t)(v10));
  t58 = fg156_dim(t56, t57);
  v11 = ((int64_t)(t58));
  t59 = ((int64_t)(v11));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((t59) >= (t60));
  t62 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_7(t61, t62, base + 0);
  t63 = ((int64_t)(v11));
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  t65 = ((t63) > (t64));
  if (t65) {
  {
  t66 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t67 = ((int64_t)(v11));
  t68 = ((int64_t)(((int64_t)gg_div(t66, t67))));
  t69 = ((int64_t)(v9));
  t70 = ((t68) > (t69));
  t71 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_function_7(t70, t71, base + 0);
  t72 = v9;
  t73 = ((int64_t)(v11));
  v9 = ((int64_t)((uint64_t)(t72) * (uint64_t)(t73)));
  }
  }
  t74 = v10;
  t75 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t74) + (uint64_t)(t75)));
  }
  goto label1;
label2:;
  t76 = ((int64_t)(v3));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = fg156_dim(t76, t77);
  v12 = ((int64_t)(t78));
  t79 = ((int64_t)(v1));
  t80 = fg156_type_size(t79);
  t81 = ((uint64_t)(t80));
  t82 = ((uint64_t)(((uint64_t)(v12))));
  t83 = gg_function_22(t81, t82, base + 0);
  t84 = ((uint64_t)(t83));
  t85 = ((uint64_t)(((uint64_t)(v6))));
  v13 = ((uint64_t)(((uint64_t)gg_udiv(t84, t85))));
  v10 = ((int64_t)(((int64_t)UINT64_C(1))));
label4:;
  t86 = ((int64_t)(v10));
  t87 = ((int64_t)(v2));
  if (!(((t86) < (t87)))) goto label5;
  {
  t88 = ((uint64_t)(v13));
  t89 = ((int64_t)(v3));
  t90 = ((int64_t)(v10));
  t91 = fg156_dim(t89, t90);
  t92 = ((uint64_t)(((uint64_t)(t91))));
  v13 = gg_function_22(t88, t92, base + 0);
  t93 = v10;
  t94 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t93) + (uint64_t)(t94)));
  }
  goto label4;
label5:;
  t95 = ((int64_t)(v7));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = ((t95) != (t96));
  t98 = t97;
  if (t98) {
  t99 = ((uint64_t)(v13));
  t100 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t98 = ((t99) != (t100));
  }
  t101 = t98;
  if (t101) {
  {
  t102 = ((uint64_t)(v13));
  t103 = ((uint64_t)(v8));
  t104 = gg_function_11(t102, t103, base + 0);
  t105 = ((uint64_t)(t104));
  t106 = ((int64_t)(v7));
  t107 = fg156_span(t106);
  t108 = ((uint64_t)(t107));
  t109 = ((t105) <= (t108));
  t110 = ((int64_t)(((int64_t)UINT64_C(16))));
  gg_function_7(t109, t110, base + 0);
  }
  }
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((int64_t)(v7));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = ((t111) != (t112));
  t114 = t113;
  if (t114) {
  t115 = ((int64_t)(v7));
  t116 = ((int64_t)(((int64_t)UINT64_C(4))));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t118 = fg156_tget(t115, t116, t117);
  t119 = ((uint64_t)(t118));
  t120 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t114 = ((t119) != (t120));
  }
  t121 = t114;
  if (t121) {
  {
  t122 = ((int64_t)(v7));
  t123 = ((int64_t)(((int64_t)UINT64_C(4))));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t125 = fg156_tget(t122, t123, t124);
  t126 = ((uint64_t)(t125));
  t127 = ((uint64_t)(v8));
  t128 = gg_function_11(t126, t127, base + 0);
  v14 = gg_function_3(t128, base + 0);
  }
  }
  v15 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t129 = ((int64_t)(v7));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = ((t129) == (t130));
  t132 = t131;
  if (t132) {
  t133 = ((int64_t)(v0));
  t134 = ((int64_t)(((int64_t)UINT64_C(4))));
  t135 = fg156_ctx(t133, t134);
  t136 = ((uint64_t)(t135));
  t137 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t132 = ((t136) == (t137));
  }
  t138 = t132;
  if (t138) {
  {
  v15 = ((uint64_t)(v13));
  }
  }
  t139 = ((int64_t)(((int64_t)UINT64_C(2))));
  t140 = fg156_constant(t139);
  t141 = ((uint64_t)(t140));
  t142 = ((uint64_t)(v15));
  v16 = gg_function_11(t141, t142, base + 0);
  t143 = ((int64_t)(v0));
  t144 = ((int64_t)(((int64_t)UINT64_C(4))));
  t145 = fg156_constant(t144);
  t146 = ((int64_t)(((int64_t)(t145))));
  t147 = ((uint64_t)(v16));
  v17 = gg_function_4(t143, t146, t147, base + 0);
  t148 = ((int64_t)(v17));
  t149 = ((int64_t)(((int64_t)UINT64_C(0))));
  t150 = ((t148) != (t149));
  t151 = ((int64_t)(((int64_t)UINT64_C(17))));
  gg_function_7(t150, t151, base + 0);
  t152 = ((int64_t)(v0));
  t153 = ((int64_t)(((int64_t)UINT64_C(1))));
  t154 = fg156_ctx(t152, t153);
  t155 = ((uint64_t)(t154));
  t156 = gg_function_3(t155, base + 0);
  t157 = ((int64_t)(v17));
  t158 = ((int64_t)(((int64_t)UINT64_C(0))));
  t159 = fg156_obj(t157, t158);
  t160 = ((uint64_t)(t159));
  t161 = fg156_ptr_add(t156, t160);
  v18 = ((int64_t)(t161));
  t162 = ((int64_t)(v18));
  t163 = ((int64_t)(((int64_t)UINT64_C(2))));
  t164 = fg156_constant(t163);
  t165 = ((uint64_t)(t164));
  fg156_zero(t162, t165);
  t166 = ((int64_t)(v18));
  t167 = ((int64_t)(((int64_t)UINT64_C(5))));
  t168 = ((int64_t)(((int64_t)UINT64_C(0))));
  t169 = ((uint64_t)(((uint64_t)(v1))));
  fg156_tset(t166, t167, t168, t169);
  t170 = ((int64_t)(v18));
  t171 = ((int64_t)(((int64_t)UINT64_C(2))));
  t172 = ((int64_t)(((int64_t)UINT64_C(0))));
  t173 = ((uint64_t)(((uint64_t)(v7))));
  fg156_tset(t170, t171, t172, t173);
  t174 = ((int64_t)(v18));
  t175 = ((int64_t)(((int64_t)UINT64_C(3))));
  t176 = ((int64_t)(((int64_t)UINT64_C(0))));
  t177 = ((uint64_t)(v8));
  fg156_tset(t174, t175, t176, t177);
  t178 = ((uint64_t)(v15));
  t179 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t180 = ((t178) > (t179));
  if (t180) {
  {
  t181 = ((int64_t)(v18));
  t182 = ((int64_t)(((int64_t)UINT64_C(5))));
  t183 = fg156_constant(t182);
  t184 = ((uint64_t)(t183));
  t185 = fg156_ptr_add(t181, t184);
  v14 = ((int64_t)(t185));
  }
  }
  t186 = ((int64_t)(v18));
  t187 = ((int64_t)(((int64_t)UINT64_C(4))));
  t188 = ((int64_t)(((int64_t)UINT64_C(0))));
  t189 = ((uint64_t)(((uint64_t)(v14))));
  fg156_tset(t186, t187, t188, t189);
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t190 = ((int64_t)(v10));
  t191 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t190) < (t191)))) goto label8;
  {
  v19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t192 = ((int64_t)(v10));
  t193 = ((int64_t)(v2));
  t194 = ((t192) < (t193));
  if (t194) {
  {
  t195 = ((int64_t)(v3));
  t196 = ((int64_t)(v10));
  t197 = fg156_dim(t195, t196);
  v19 = ((int64_t)(t197));
  }
  }
  t198 = ((int64_t)(v18));
  t199 = ((int64_t)(((int64_t)UINT64_C(0))));
  t200 = ((int64_t)(v10));
  t201 = ((uint64_t)(((uint64_t)(v19))));
  fg156_tset(t198, t199, t200, t201);
  t202 = v10;
  t203 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t202) + (uint64_t)(t203)));
  }
  goto label7;
label8:;
  t204 = ((int64_t)(v18));
  t205 = ((int64_t)(((int64_t)UINT64_C(1))));
  t206 = ((int64_t)(((int64_t)UINT64_C(0))));
  t207 = ((int64_t)(v1));
  t208 = fg156_type_size(t207);
  t209 = ((uint64_t)(t208));
  fg156_tset(t204, t205, t206, t209);
  t210 = ((int64_t)(v18));
  t211 = ((int64_t)(((int64_t)UINT64_C(1))));
  t212 = ((int64_t)(((int64_t)UINT64_C(1))));
  t213 = ((int64_t)(v1));
  t214 = fg156_type_size(t213);
  t215 = ((uint64_t)(t214));
  t216 = ((int64_t)(v12));
  t217 = ((int64_t)(v6));
  t218 = ((uint64_t)(((uint64_t)(((int64_t)gg_div(t216, t217))))));
  t219 = gg_function_22(t215, t218, base + 0);
  fg156_tset(t210, t211, t212, t219);
  v10 = ((int64_t)(((int64_t)UINT64_C(2))));
label10:;
  t220 = ((int64_t)(v10));
  t221 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t220) < (t221)))) goto label11;
  {
  t222 = ((int64_t)(v18));
  t223 = ((int64_t)(((int64_t)UINT64_C(1))));
  t224 = ((int64_t)(v10));
  t225 = ((int64_t)(v18));
  t226 = ((int64_t)(v10));
  t227 = ((int64_t)(((int64_t)UINT64_C(1))));
  t228 = ((int64_t)(((int64_t)((uint64_t)(t226) - (uint64_t)(t227)))));
  t229 = gg_function_25(t225, t228, base + 0);
  t230 = ((int64_t)(v18));
  t231 = ((int64_t)(v10));
  t232 = ((int64_t)(((int64_t)UINT64_C(1))));
  t233 = ((int64_t)(((int64_t)((uint64_t)(t231) - (uint64_t)(t232)))));
  t234 = gg_function_26(t230, t233, base + 0);
  t235 = ((uint64_t)(((uint64_t)(t234))));
  t236 = gg_function_22(t229, t235, base + 0);
  fg156_tset(t222, t223, t224, t236);
  t237 = v10;
  t238 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t237) + (uint64_t)(t238)));
  }
  goto label10;
label11:;
  t239 = ((int64_t)(v0));
  t240 = ((int64_t)(((int64_t)UINT64_C(5))));
  t241 = fg156_ctx(t239, t240);
  t242 = ((uint64_t)(t241));
  t243 = ((uint64_t)(((int64_t)UINT64_C(2147483647))));
  t244 = ((t242) < (t243));
  t245 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_7(t244, t245, base + 0);
  t246 = ((int64_t)(v0));
  t247 = ((int64_t)(((int64_t)UINT64_C(5))));
  t248 = ((int64_t)(v0));
  t249 = ((int64_t)(((int64_t)UINT64_C(5))));
  t250 = fg156_ctx(t248, t249);
  t251 = ((uint64_t)(t250));
  t252 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t253 = ((uint64_t)(((uint64_t)((uint64_t)(t251) + (uint64_t)(t252)))));
  fg156_ctx_set(t246, t247, t253);
  t0 = ((int64_t)(v18));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_33(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
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
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(5))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg156_tget(t2, t3, t4);
  t6 = ((uint64_t)(t5));
  t7 = gg_function_3(t6, base + 0);
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(v1));
  t11 = ((uint64_t)(v4));
  v5 = gg_function_17(t1, t7, t8, t9, t10, t11, base + 0);
  t12 = ((int64_t)(v5));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg156_name(t12, t13, t14);
  t15 = ((int64_t)(v5));
  t16 = ((int64_t)(((int64_t)UINT64_C(9))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((uint64_t)(v4));
  fg156_tset(t15, t16, t17, t18);
  t19 = ((int64_t)(v5));
  t20 = ((int64_t)(((int64_t)UINT64_C(7))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(7))));
  t23 = fg156_constant(t22);
  t24 = ((uint64_t)(t23));
  fg156_tset(t19, t20, t21, t24);
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(((int64_t)UINT64_C(8))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((uint64_t)(((uint64_t)(v1))));
  fg156_tset(t25, t26, t27, t28);
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_36(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  uint64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  uint64_t t19;
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
  uint64_t t33;
  uint64_t t34;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((int64_t)(v1));
label1:;
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) != (t2)))) goto label2;
  {
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(((int64_t)UINT64_C(3))));
  t5 = fg156_obj(t3, t4);
  t6 = ((uint64_t)(t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  t8 = fg156_constant(t7);
  t9 = ((uint64_t)(t8));
  t10 = ((t6) == (t9));
  if (t10) {
  {
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = fg156_ctx(t11, t12);
  t14 = ((uint64_t)(t13));
  t15 = gg_function_3(t14, base + 0);
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg156_obj(t16, t17);
  t19 = ((uint64_t)(t18));
  t20 = fg156_ptr_add(t15, t19);
  v4 = ((int64_t)(t20));
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) == (t22));
  t24 = t23;
  if (!t24) {
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(v2));
  t27 = fg156_name_equal(t25, t26);
  t28 = ((int64_t)(t27));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t28) != (t29));
  }
  t30 = t24;
  if (t30) {
  {
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
  }
  }
  }
  t31 = ((int64_t)(v3));
  t32 = ((int64_t)(((int64_t)UINT64_C(2))));
  t33 = fg156_obj(t31, t32);
  t34 = ((uint64_t)(t33));
  v3 = gg_function_3(t34, base + 0);
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_27(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  uint64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  uint64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
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
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(5))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg156_tget(t2, t3, t4);
  t6 = ((uint64_t)(t5));
  t7 = gg_function_3(t6, base + 0);
  t8 = ((int64_t)(((int64_t)UINT64_C(4))));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(6))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg156_tget(t9, t10, t11);
  t13 = ((uint64_t)(t12));
  t14 = gg_function_3(t13, base + 0);
  t15 = ((int64_t)(v1));
  t16 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  v2 = gg_function_17(t1, t7, t8, t14, t15, t16, base + 0);
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg156_name(t17, t18, t19);
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t20) < (t21)))) goto label2;
  {
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(v3));
  t27 = gg_function_25(t25, t26, base + 0);
  fg156_tset(t22, t23, t24, t27);
  t28 = v3;
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t28) + (uint64_t)(t29)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_35(int64_t p0, int64_t p1, int64_t base) {
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
  uint64_t t12;
  uint64_t t13;
  int64_t t14;
  int64_t t15;
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
  int64_t t26;
  uint64_t t27;
  int64_t t28;
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
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint64_t t43;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  v2 = gg_function_27(t1, t2, base + 0);
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg156_name(t3, t4, t5);
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = fg156_tget(t9, t10, t11);
  t13 = ((uint64_t)(t12));
  fg156_tset(t6, t7, t8, t13);
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg156_tget(t17, t18, t19);
  t21 = ((uint64_t)(t20));
  fg156_tset(t14, t15, t16, t21);
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = gg_function_25(t25, t26, base + 0);
  fg156_tset(t22, t23, t24, t27);
  t28 = ((int64_t)(v2));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  t30 = ((int64_t)(((int64_t)UINT64_C(1))));
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = gg_function_25(t31, t32, base + 0);
  fg156_tset(t28, t29, t30, t33);
  t34 = ((int64_t)(v2));
  t35 = ((int64_t)(((int64_t)UINT64_C(7))));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((int64_t)(((int64_t)UINT64_C(9))));
  t38 = fg156_constant(t37);
  t39 = ((uint64_t)(t38));
  fg156_tset(t34, t35, t36, t39);
  t40 = ((int64_t)(v2));
  t41 = ((int64_t)(((int64_t)UINT64_C(8))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((uint64_t)(((uint64_t)(v1))));
  fg156_tset(t40, t41, t42, t43);
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_34(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  uint8_t t12;
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
  uint64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  uint64_t t40;
  int64_t t41;
  int64_t t42;
  uint64_t t43;
  uint64_t t44;
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
  uint64_t t56;
  uint64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
  uint64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  uint64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  uint64_t t82;
  uint64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  uint64_t t87;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(2))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = fg156_arg(t3, t6);
  t8 = ((uint64_t)(t7));
  v2 = gg_function_3(t8, base + 0);
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) >= (t10));
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t12 = ((t13) < (t14));
  }
  t15 = t12;
  t16 = ((int64_t)(((int64_t)UINT64_C(30))));
  gg_function_7(t15, t16, base + 0);
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(v1));
  if (!(((t17) < (t18)))) goto label5;
  {
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)UINT64_C(2))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t21)))));
  t23 = fg156_arg(t19, t22);
  t24 = ((uint64_t)(t23));
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(((int64_t)UINT64_C(2))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  t29 = fg156_arg(t25, t28);
  t30 = ((uint64_t)(t29));
  t31 = ((t24) != (t30));
  t32 = ((int64_t)(((int64_t)UINT64_C(31))));
  gg_function_7(t31, t32, base + 0);
  t33 = v3;
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t33) + (uint64_t)(t34)));
  }
  goto label4;
label5:;
  t35 = v1;
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  }
  goto label1;
label2:;
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t39 = fg156_arg(t37, t38);
  t40 = ((uint64_t)(t39));
  v4 = gg_function_3(t40, base + 0);
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = fg156_arg(t41, t42);
  t44 = ((uint64_t)(t43));
  t45 = gg_function_3(t44, base + 0);
  t46 = ((int64_t)(v4));
  v5 = gg_function_27(t45, t46, base + 0);
  t47 = ((int64_t)(v5));
  t48 = ((int64_t)(v4));
  t49 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg156_name(t47, t48, t49);
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t50) < (t51)))) goto label8;
  {
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(v1));
  t54 = ((int64_t)(((int64_t)UINT64_C(2))));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t54)))));
  t56 = fg156_arg(t52, t55);
  t57 = ((uint64_t)(t56));
  v6 = gg_function_3(t57, base + 0);
  t58 = ((int64_t)(v5));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((int64_t)(v6));
  t61 = ((int64_t)(v4));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((int64_t)(v1));
  t64 = fg156_tget(t61, t62, t63);
  t65 = ((uint64_t)(t64));
  fg156_tset(t58, t59, t60, t65);
  t66 = ((int64_t)(v5));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  t68 = ((int64_t)(v6));
  t69 = ((int64_t)(v4));
  t70 = ((int64_t)(v1));
  t71 = gg_function_25(t69, t70, base + 0);
  fg156_tset(t66, t67, t68, t71);
  t72 = ((int64_t)(v5));
  t73 = ((int64_t)(((int64_t)UINT64_C(10))));
  t74 = ((int64_t)(v1));
  t75 = ((uint64_t)(((uint64_t)(v6))));
  fg156_tset(t72, t73, t74, t75);
  t76 = v1;
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t76) + (uint64_t)(t77)));
  }
  goto label7;
label8:;
  t78 = ((int64_t)(v5));
  t79 = ((int64_t)(((int64_t)UINT64_C(7))));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = ((int64_t)(((int64_t)UINT64_C(8))));
  t82 = fg156_constant(t81);
  t83 = ((uint64_t)(t82));
  fg156_tset(t78, t79, t80, t83);
  t84 = ((int64_t)(v5));
  t85 = ((int64_t)(((int64_t)UINT64_C(8))));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = ((uint64_t)(((uint64_t)(v4))));
  fg156_tset(t84, t85, t86, t87);
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_32(int64_t p0, int64_t base) {
  int64_t t0;
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
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  uint64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint64_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint64_t t61;
  uint64_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint64_t t73;
  int64_t t74;
  int64_t t75;
  uint8_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint64_t t80;
  int64_t t81;
  int64_t t82;
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  uint64_t t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  uint64_t t94;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
  int64_t v8;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg156_arg(t1, t2);
  t4 = ((uint64_t)(t3));
  v1 = gg_function_3(t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = fg156_arg(t5, t6);
  t8 = ((uint64_t)(t7));
  v2 = gg_function_3(t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = fg156_arg(t9, t10);
  t12 = ((uint64_t)(t11));
  v3 = gg_function_3(t12, base + 0);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = fg156_arg(t13, t14);
  t16 = ((uint64_t)(t15));
  v4 = gg_function_3(t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = fg156_arg(t17, t18);
  v5 = ((uint64_t)(t19));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(6))));
  t22 = fg156_arg(t20, t21);
  v6 = ((uint64_t)(t22));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(7))));
  t25 = fg156_arg(t23, t24);
  v7 = ((uint64_t)(t25));
  t26 = ((int64_t)(v3));
  t27 = ((int64_t)(((int64_t)UINT64_C(2))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(5))));
  t31 = fg156_arg(t29, t30);
  t32 = ((uint64_t)(t31));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = fg156_dim(t33, t34);
  t36 = ((uint64_t)(((uint64_t)(t35))));
  v6 = gg_function_22(t32, t36, base + 0);
  v7 = ((uint64_t)(v6));
  }
  }
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(((int64_t)UINT64_C(3))));
  t39 = ((t37) == (t38));
  if (t39) {
  {
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(6))));
  t42 = fg156_arg(t40, t41);
  t43 = ((uint64_t)(t42));
  t44 = ((int64_t)(v4));
  t45 = ((int64_t)(((int64_t)UINT64_C(2))));
  t46 = fg156_dim(t44, t45);
  t47 = ((uint64_t)(((uint64_t)(t46))));
  v7 = gg_function_22(t43, t47, base + 0);
  }
  }
  t48 = ((int64_t)(v1));
  t49 = ((int64_t)(v2));
  t50 = ((int64_t)(v3));
  t51 = ((int64_t)(v4));
  t52 = ((uint64_t)(v5));
  v8 = gg_function_33(t48, t49, t50, t51, t52, base + 0);
  t53 = ((int64_t)(v3));
  t54 = ((int64_t)(((int64_t)UINT64_C(2))));
  t55 = ((t53) >= (t54));
  if (t55) {
  {
  t56 = ((int64_t)(v8));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(5))));
  t61 = fg156_arg(t59, t60);
  t62 = ((uint64_t)(t61));
  fg156_tset(t56, t57, t58, t62);
  }
  }
  t63 = ((int64_t)(v3));
  t64 = ((int64_t)(((int64_t)UINT64_C(2))));
  t65 = ((t63) == (t64));
  if (t65) {
  {
  t66 = ((int64_t)(v8));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  t68 = ((int64_t)(((int64_t)UINT64_C(2))));
  t69 = ((uint64_t)(v6));
  fg156_tset(t66, t67, t68, t69);
  t70 = ((int64_t)(v8));
  t71 = ((int64_t)(((int64_t)UINT64_C(1))));
  t72 = ((int64_t)(((int64_t)UINT64_C(3))));
  t73 = ((uint64_t)(v7));
  fg156_tset(t70, t71, t72, t73);
  }
  }
  t74 = ((int64_t)(v3));
  t75 = ((int64_t)(((int64_t)UINT64_C(3))));
  t76 = ((t74) >= (t75));
  if (t76) {
  {
  t77 = ((int64_t)(v8));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((uint64_t)(v6));
  fg156_tset(t77, t78, t79, t80);
  }
  }
  t81 = ((int64_t)(v3));
  t82 = ((int64_t)(((int64_t)UINT64_C(3))));
  t83 = ((t81) == (t82));
  if (t83) {
  {
  t84 = ((int64_t)(v8));
  t85 = ((int64_t)(((int64_t)UINT64_C(1))));
  t86 = ((int64_t)(((int64_t)UINT64_C(3))));
  t87 = ((uint64_t)(v7));
  fg156_tset(t84, t85, t86, t87);
  }
  }
  t88 = ((int64_t)(v3));
  t89 = ((int64_t)(((int64_t)UINT64_C(4))));
  t90 = ((t88) == (t89));
  if (t90) {
  {
  t91 = ((int64_t)(v8));
  t92 = ((int64_t)(((int64_t)UINT64_C(1))));
  t93 = ((int64_t)(((int64_t)UINT64_C(3))));
  t94 = ((uint64_t)(v7));
  fg156_tset(t91, t92, t93, t94);
  }
  }
  t0 = ((int64_t)(v8));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_29(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
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
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
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
  {
  t1 = ((int64_t)(v1));
  t2 = fg156_contiguous(t1);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(20))));
  gg_function_7(t5, t6, base + 0);
  v4 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(v2));
  if (!(((t7) < (t8)))) goto label2;
  {
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(v5));
  t11 = fg156_dim(t9, t10);
  v6 = ((int64_t)(t11));
  t12 = ((int64_t)(v6));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) >= (t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_7(t14, t15, base + 0);
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  t19 = t18;
  if (!t19) {
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t22 = ((int64_t)(v6));
  t23 = ((int64_t)(((int64_t)gg_div(t21, t22))));
  t19 = ((t20) <= (t23));
  }
  t24 = t19;
  t25 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_function_7(t24, t25, base + 0);
  t26 = v4;
  t27 = ((int64_t)(v6));
  v4 = ((int64_t)((uint64_t)(t26) * (uint64_t)(t27)));
  t28 = v5;
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t28) + (uint64_t)(t29)));
  }
  goto label1;
label2:;
  t30 = ((int64_t)(v1));
  t31 = fg156_elements(t30);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(v4));
  t34 = ((t32) == (t33));
  t35 = ((int64_t)(((int64_t)UINT64_C(21))));
  gg_function_7(t34, t35, base + 0);
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(v1));
  t38 = ((int64_t)(((int64_t)UINT64_C(5))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = fg156_tget(t37, t38, t39);
  t41 = ((uint64_t)(t40));
  t42 = gg_function_3(t41, base + 0);
  t43 = ((int64_t)(v2));
  t44 = ((int64_t)(v3));
  t45 = ((int64_t)(v1));
  t46 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  v7 = gg_function_17(t36, t42, t43, t44, t45, t46, base + 0);
  t47 = ((int64_t)(v7));
  t48 = ((int64_t)(v1));
  t49 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg156_name(t47, t48, t49);
  t50 = ((int64_t)(v7));
  t51 = ((int64_t)(((int64_t)UINT64_C(7))));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((int64_t)(((int64_t)UINT64_C(6))));
  t54 = fg156_constant(t53);
  t55 = ((uint64_t)(t54));
  fg156_tset(t50, t51, t52, t55);
  t56 = ((int64_t)(v7));
  t57 = ((int64_t)(((int64_t)UINT64_C(8))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((uint64_t)(((uint64_t)(v1))));
  fg156_tset(t56, t57, t58, t59);
  t0 = ((int64_t)(v7));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  uint64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint64_t t38;
  uint64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint64_t t43;
  uint64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint64_t t48;
  uint64_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  uint64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint64_t t65;
  uint64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  uint64_t t72;
  uint64_t t73;
  int64_t t74;
  int64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  uint8_t t79;
  int64_t t80;
  int64_t t81;
  uint64_t t82;
  uint64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  uint64_t t87;
  uint64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
  int64_t t93;
  int64_t t94;
  uint64_t t95;
  uint64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint64_t t100;
  uint64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint64_t t105;
  uint64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  uint64_t t110;
  uint64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  uint8_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  uint8_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  uint8_t t123;
  int64_t t124;
  int64_t t125;
  uint64_t t126;
  uint64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  uint64_t t131;
  uint64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  uint64_t t139;
  uint64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  uint64_t t144;
  uint64_t t145;
  int64_t t146;
  int64_t t147;
  uint64_t t148;
  uint64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  uint8_t t154;
  int64_t t155;
  int64_t t156;
  uint64_t t157;
  uint64_t t158;
  int64_t t159;
  uint64_t t160;
  int64_t t161;
  uint64_t t162;
  uint64_t t163;
  uint64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint64_t t168;
  uint64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  uint64_t t173;
  uint64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  uint8_t t179;
  int64_t t180;
  int64_t t181;
  uint64_t t182;
  uint64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  uint64_t t187;
  uint64_t t188;
  int64_t t189;
  int64_t t190;
  uint64_t t191;
  uint64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  uint64_t t196;
  uint64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  uint8_t t201;
  int64_t t202;
  int64_t t203;
  uint64_t t204;
  uint64_t t205;
  int64_t t206;
  int64_t t207;
  uint64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  uint64_t t212;
  uint64_t t213;
  int64_t t214;
  int64_t t215;
  uint8_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  uint64_t t220;
  uint64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  uint64_t t225;
  uint64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  uint8_t t230;
  int64_t t231;
  int64_t t232;
  uint64_t t233;
  uint64_t t234;
  int64_t t235;
  int64_t t236;
  uint64_t t237;
  uint64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  uint64_t t243;
  uint64_t t244;
  int64_t t245;
  uint64_t t246;
  uint64_t t247;
  uint8_t t248;
  int64_t t249;
  int64_t t250;
  uint64_t t251;
  uint64_t t252;
  int64_t t253;
  int64_t t254;
  uint64_t t255;
  uint64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  uint64_t t260;
  uint64_t t261;
  int64_t t262;
  int64_t t263;
  uint64_t t264;
  uint64_t t265;
  uint64_t t266;
  uint8_t t267;
  int64_t t268;
  int64_t t269;
  uint64_t t270;
  uint64_t t271;
  int64_t t272;
  int64_t t273;
  uint8_t t274;
  int64_t t275;
  int64_t t276;
  uint64_t t277;
  uint64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  uint64_t t282;
  uint64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  uint64_t t287;
  uint64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  uint64_t t292;
  uint64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  uint64_t t297;
  uint64_t t298;
  int64_t t299;
  int64_t t300;
  uint8_t t301;
  int64_t t302;
  int64_t t303;
  uint64_t t304;
  uint64_t t305;
  int64_t t306;
  int64_t t307;
  uint64_t t308;
  uint64_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  uint8_t t318;
  uint8_t t319;
  int64_t t320;
  int64_t t321;
  uint8_t t322;
  uint8_t t323;
  int64_t t324;
  int64_t t325;
  uint8_t t326;
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
  int64_t t339;
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
  uint64_t t382;
  uint64_t t383;
  int64_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  uint64_t t388;
  uint64_t t389;
  int64_t t390;
  int64_t t391;
  int64_t t392;
  int64_t t393;
  uint64_t t394;
  uint64_t t395;
  int64_t t396;
  int64_t t397;
  int64_t t398;
  int64_t t399;
  uint64_t t400;
  uint64_t t401;
  int64_t t402;
  int64_t t403;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  uint64_t v7;
  uint64_t v8;
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
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = fg156_arg(t4, t5);
  t7 = ((uint64_t)(t6));
  t8 = gg_function_3(t7, base + 0);
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = fg156_arg(t9, t10);
  t12 = ((uint64_t)(t11));
  t13 = gg_function_3(t12, base + 0);
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  t16 = fg156_arg(t14, t15);
  t17 = ((uint64_t)(t16));
  t0 = gg_function_4(t8, t13, t17, base + 0);
  goto gg_done;
  }
  }
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((t18) == (t19));
  if (t20) {
  {
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg156_arg(t21, t22);
  t24 = ((uint64_t)(t23));
  t25 = gg_function_3(t24, base + 0);
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = fg156_arg(t26, t27);
  t29 = ((uint64_t)(t28));
  t30 = gg_function_3(t29, base + 0);
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(2))));
  t33 = fg156_arg(t31, t32);
  t34 = ((uint64_t)(t33));
  t35 = gg_function_3(t34, base + 0);
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  t38 = fg156_arg(t36, t37);
  t39 = ((uint64_t)(t38));
  t40 = gg_function_3(t39, base + 0);
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(((int64_t)UINT64_C(4))));
  t43 = fg156_arg(t41, t42);
  t44 = ((uint64_t)(t43));
  t45 = gg_function_3(t44, base + 0);
  t46 = ((int64_t)(v1));
  t47 = ((int64_t)(((int64_t)UINT64_C(5))));
  t48 = fg156_arg(t46, t47);
  t49 = ((uint64_t)(t48));
  t0 = gg_function_17(t25, t30, t35, t40, t45, t49, base + 0);
  goto gg_done;
  }
  }
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((t50) == (t51));
  if (t52) {
  {
  t53 = ((int64_t)(v1));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = fg156_arg(t53, t54);
  t56 = ((uint64_t)(t55));
  v2 = gg_function_3(t56, base + 0);
  t57 = ((int64_t)(v1));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = fg156_arg(t57, t58);
  t60 = ((uint64_t)(t59));
  t61 = gg_function_3(t60, base + 0);
  t62 = ((int64_t)(v2));
  t63 = ((int64_t)(((int64_t)UINT64_C(5))));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = fg156_tget(t62, t63, t64);
  t66 = ((uint64_t)(t65));
  t67 = gg_function_3(t66, base + 0);
  t68 = ((int64_t)(((int64_t)UINT64_C(4))));
  t69 = ((int64_t)(v2));
  t70 = ((int64_t)(((int64_t)UINT64_C(6))));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = fg156_tget(t69, t70, t71);
  t73 = ((uint64_t)(t72));
  t74 = gg_function_3(t73, base + 0);
  t75 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t0 = gg_function_17(t61, t67, t68, t74, t75, t76, base + 0);
  goto gg_done;
  }
  }
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(3))));
  t79 = ((t77) == (t78));
  if (t79) {
  {
  t80 = ((int64_t)(v1));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = fg156_arg(t80, t81);
  t83 = ((uint64_t)(t82));
  t84 = gg_function_3(t83, base + 0);
  t85 = ((int64_t)(v1));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = fg156_arg(t85, t86);
  t88 = ((uint64_t)(t87));
  t89 = gg_function_3(t88, base + 0);
  t0 = gg_function_27(t84, t89, base + 0);
  goto gg_done;
  }
  }
  t90 = ((int64_t)(v0));
  t91 = ((int64_t)(((int64_t)UINT64_C(4))));
  t92 = ((t90) == (t91));
  if (t92) {
  {
  t93 = ((int64_t)(v1));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = fg156_arg(t93, t94);
  t96 = ((uint64_t)(t95));
  t97 = gg_function_3(t96, base + 0);
  t98 = ((int64_t)(v1));
  t99 = ((int64_t)(((int64_t)UINT64_C(1))));
  t100 = fg156_arg(t98, t99);
  t101 = ((uint64_t)(t100));
  t102 = gg_function_3(t101, base + 0);
  t103 = ((int64_t)(v1));
  t104 = ((int64_t)(((int64_t)UINT64_C(2))));
  t105 = fg156_arg(t103, t104);
  t106 = ((uint64_t)(t105));
  t107 = gg_function_3(t106, base + 0);
  t108 = ((int64_t)(v1));
  t109 = ((int64_t)(((int64_t)UINT64_C(3))));
  t110 = fg156_arg(t108, t109);
  t111 = ((uint64_t)(t110));
  t112 = gg_function_3(t111, base + 0);
  t0 = gg_function_29(t97, t102, t107, t112, base + 0);
  goto gg_done;
  }
  }
  t113 = ((int64_t)(v0));
  t114 = ((int64_t)(((int64_t)UINT64_C(5))));
  t115 = ((t113) == (t114));
  if (t115) {
  {
  t116 = ((int64_t)(v1));
  t0 = gg_function_32(t116, base + 0);
  goto gg_done;
  }
  }
  t117 = ((int64_t)(v0));
  t118 = ((int64_t)(((int64_t)UINT64_C(6))));
  t119 = ((t117) == (t118));
  if (t119) {
  {
  t120 = ((int64_t)(v1));
  t0 = gg_function_34(t120, base + 0);
  goto gg_done;
  }
  }
  t121 = ((int64_t)(v0));
  t122 = ((int64_t)(((int64_t)UINT64_C(7))));
  t123 = ((t121) == (t122));
  if (t123) {
  {
  t124 = ((int64_t)(v1));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = fg156_arg(t124, t125);
  t127 = ((uint64_t)(t126));
  t128 = gg_function_3(t127, base + 0);
  t129 = ((int64_t)(v1));
  t130 = ((int64_t)(((int64_t)UINT64_C(1))));
  t131 = fg156_arg(t129, t130);
  t132 = ((uint64_t)(t131));
  t133 = gg_function_3(t132, base + 0);
  t0 = gg_function_35(t128, t133, base + 0);
  goto gg_done;
  }
  }
  t134 = ((int64_t)(v0));
  t135 = ((int64_t)(((int64_t)UINT64_C(8))));
  t136 = ((t134) == (t135));
  if (t136) {
  {
  t137 = ((int64_t)(v1));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = fg156_arg(t137, t138);
  t140 = ((uint64_t)(t139));
  t141 = gg_function_3(t140, base + 0);
  t142 = ((int64_t)(v1));
  t143 = ((int64_t)(((int64_t)UINT64_C(0))));
  t144 = fg156_arg(t142, t143);
  t145 = ((uint64_t)(t144));
  t146 = gg_function_3(t145, base + 0);
  t147 = ((int64_t)(((int64_t)UINT64_C(2))));
  t148 = fg156_ctx(t146, t147);
  t149 = ((uint64_t)(t148));
  t150 = gg_function_3(t149, base + 0);
  t151 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = gg_function_36(t141, t150, t151, base + 0);
  goto gg_done;
  }
  }
  t152 = ((int64_t)(v0));
  t153 = ((int64_t)(((int64_t)UINT64_C(9))));
  t154 = ((t152) == (t153));
  if (t154) {
  {
  t155 = ((int64_t)(v1));
  t156 = ((int64_t)(((int64_t)UINT64_C(1))));
  t157 = fg156_arg(t155, t156);
  t158 = ((uint64_t)(t157));
  t159 = gg_function_3(t158, base + 0);
  t160 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t161 = ((int64_t)(((int64_t)UINT64_C(1))));
  t162 = fg156_constant(t161);
  t163 = ((uint64_t)(t162));
  t164 = ((uint64_t)(((uint64_t)((uint64_t)(t160) - (uint64_t)(t163)))));
  t165 = fg156_ptr_add(t159, t164);
  v3 = ((int64_t)(t165));
  t166 = ((int64_t)(v1));
  t167 = ((int64_t)(((int64_t)UINT64_C(0))));
  t168 = fg156_arg(t166, t167);
  t169 = ((uint64_t)(t168));
  t170 = gg_function_3(t169, base + 0);
  t171 = ((int64_t)(v3));
  t172 = ((int64_t)(((int64_t)UINT64_C(2))));
  t173 = fg156_obj(t171, t172);
  t174 = ((uint64_t)(t173));
  t175 = gg_function_3(t174, base + 0);
  t176 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = gg_function_36(t170, t175, t176, base + 0);
  goto gg_done;
  }
  }
  t177 = ((int64_t)(v0));
  t178 = ((int64_t)(((int64_t)UINT64_C(10))));
  t179 = ((t177) == (t178));
  if (t179) {
  {
  t180 = ((int64_t)(v1));
  t181 = ((int64_t)(((int64_t)UINT64_C(0))));
  t182 = fg156_arg(t180, t181);
  t183 = ((uint64_t)(t182));
  t184 = gg_function_3(t183, base + 0);
  t185 = ((int64_t)(v1));
  t186 = ((int64_t)(((int64_t)UINT64_C(0))));
  t187 = fg156_arg(t185, t186);
  t188 = ((uint64_t)(t187));
  t189 = gg_function_3(t188, base + 0);
  t190 = ((int64_t)(((int64_t)UINT64_C(2))));
  t191 = fg156_ctx(t189, t190);
  t192 = ((uint64_t)(t191));
  t193 = gg_function_3(t192, base + 0);
  t194 = ((int64_t)(v1));
  t195 = ((int64_t)(((int64_t)UINT64_C(1))));
  t196 = fg156_arg(t194, t195);
  t197 = ((uint64_t)(t196));
  t198 = gg_function_3(t197, base + 0);
  t0 = gg_function_36(t184, t193, t198, base + 0);
  goto gg_done;
  }
  }
  t199 = ((int64_t)(v0));
  t200 = ((int64_t)(((int64_t)UINT64_C(11))));
  t201 = ((t199) == (t200));
  if (t201) {
  {
  t202 = ((int64_t)(v1));
  t203 = ((int64_t)(((int64_t)UINT64_C(0))));
  t204 = fg156_arg(t202, t203);
  t205 = ((uint64_t)(t204));
  v4 = gg_function_3(t205, base + 0);
  t206 = ((int64_t)(v4));
  t207 = ((int64_t)(((int64_t)UINT64_C(10))));
  t208 = fg156_constant(t207);
  t209 = ((int64_t)(((int64_t)(t208))));
  t210 = ((int64_t)(v1));
  t211 = ((int64_t)(((int64_t)UINT64_C(1))));
  t212 = fg156_arg(t210, t211);
  t213 = ((uint64_t)(t212));
  v5 = gg_function_4(t206, t209, t213, base + 0);
  t214 = ((int64_t)(v5));
  t215 = ((int64_t)(((int64_t)UINT64_C(0))));
  t216 = ((t214) != (t215));
  t217 = ((int64_t)(((int64_t)UINT64_C(17))));
  gg_function_7(t216, t217, base + 0);
  t218 = ((int64_t)(v4));
  t219 = ((int64_t)(((int64_t)UINT64_C(1))));
  t220 = fg156_ctx(t218, t219);
  t221 = ((uint64_t)(t220));
  t222 = gg_function_3(t221, base + 0);
  t223 = ((int64_t)(v5));
  t224 = ((int64_t)(((int64_t)UINT64_C(0))));
  t225 = fg156_obj(t223, t224);
  t226 = ((uint64_t)(t225));
  t227 = fg156_ptr_add(t222, t226);
  t0 = ((int64_t)(t227));
  goto gg_done;
  }
  }
  t228 = ((int64_t)(v0));
  t229 = ((int64_t)(((int64_t)UINT64_C(12))));
  t230 = ((t228) == (t229));
  if (t230) {
  {
  t231 = ((int64_t)(v1));
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = fg156_arg(t231, t232);
  t234 = ((uint64_t)(t233));
  t235 = gg_function_3(t234, base + 0);
  t236 = ((int64_t)(((int64_t)UINT64_C(2))));
  t237 = fg156_ctx(t235, t236);
  t238 = ((uint64_t)(t237));
  v6 = gg_function_3(t238, base + 0);
  v7 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t239 = ((int64_t)(v6));
  t240 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t239) != (t240)))) goto label2;
  {
  t241 = ((int64_t)(v6));
  t242 = ((int64_t)(((int64_t)UINT64_C(3))));
  t243 = fg156_obj(t241, t242);
  t244 = ((uint64_t)(t243));
  t245 = ((int64_t)(((int64_t)UINT64_C(4))));
  t246 = fg156_constant(t245);
  t247 = ((uint64_t)(t246));
  t248 = ((t244) == (t247));
  if (t248) {
  {
  t249 = ((int64_t)(v1));
  t250 = ((int64_t)(((int64_t)UINT64_C(0))));
  t251 = fg156_arg(t249, t250);
  t252 = ((uint64_t)(t251));
  t253 = gg_function_3(t252, base + 0);
  t254 = ((int64_t)(((int64_t)UINT64_C(1))));
  t255 = fg156_ctx(t253, t254);
  t256 = ((uint64_t)(t255));
  t257 = gg_function_3(t256, base + 0);
  t258 = ((int64_t)(v6));
  t259 = ((int64_t)(((int64_t)UINT64_C(0))));
  t260 = fg156_obj(t258, t259);
  t261 = ((uint64_t)(t260));
  t262 = fg156_ptr_add(t257, t261);
  t263 = ((int64_t)(t262));
  t264 = fg156_span(t263);
  v8 = ((uint64_t)(t264));
  t265 = ((uint64_t)(v8));
  t266 = ((uint64_t)(v7));
  t267 = ((t265) > (t266));
  if (t267) {
  {
  v7 = ((uint64_t)(v8));
  }
  }
  }
  }
  t268 = ((int64_t)(v6));
  t269 = ((int64_t)(((int64_t)UINT64_C(2))));
  t270 = fg156_obj(t268, t269);
  t271 = ((uint64_t)(t270));
  v6 = gg_function_3(t271, base + 0);
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)(v7))));
  goto gg_done;
  }
  }
  t272 = ((int64_t)(v0));
  t273 = ((int64_t)(((int64_t)UINT64_C(14))));
  t274 = ((t272) == (t273));
  if (t274) {
  {
  t275 = ((int64_t)(v1));
  t276 = ((int64_t)(((int64_t)UINT64_C(0))));
  t277 = fg156_arg(t275, t276);
  t278 = ((uint64_t)(t277));
  t279 = gg_function_3(t278, base + 0);
  t280 = ((int64_t)(v1));
  t281 = ((int64_t)(((int64_t)UINT64_C(1))));
  t282 = fg156_arg(t280, t281);
  t283 = ((uint64_t)(t282));
  t284 = gg_function_3(t283, base + 0);
  t285 = ((int64_t)(v1));
  t286 = ((int64_t)(((int64_t)UINT64_C(2))));
  t287 = fg156_arg(t285, t286);
  t288 = ((uint64_t)(t287));
  t289 = gg_function_3(t288, base + 0);
  t290 = ((int64_t)(v1));
  t291 = ((int64_t)(((int64_t)UINT64_C(3))));
  t292 = fg156_arg(t290, t291);
  t293 = ((uint64_t)(t292));
  t294 = gg_function_3(t293, base + 0);
  t295 = ((int64_t)(v1));
  t296 = ((int64_t)(((int64_t)UINT64_C(4))));
  t297 = fg156_arg(t295, t296);
  t298 = ((uint64_t)(t297));
  t0 = gg_function_33(t279, t284, t289, t294, t298, base + 0);
  goto gg_done;
  }
  }
  t299 = ((int64_t)(v0));
  t300 = ((int64_t)(((int64_t)UINT64_C(13))));
  t301 = ((t299) == (t300));
  if (t301) {
  {
  t302 = ((int64_t)(v1));
  t303 = ((int64_t)(((int64_t)UINT64_C(0))));
  t304 = fg156_arg(t302, t303);
  t305 = ((uint64_t)(t304));
  v9 = gg_function_3(t305, base + 0);
  t306 = ((int64_t)(v1));
  t307 = ((int64_t)(((int64_t)UINT64_C(1))));
  t308 = fg156_arg(t306, t307);
  t309 = ((uint64_t)(t308));
  v10 = gg_function_3(t309, base + 0);
  t310 = ((int64_t)(v9));
  t311 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = gg_function_26(t310, t311, base + 0);
  t312 = ((int64_t)(v9));
  t313 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = gg_function_26(t312, t313, base + 0);
  t314 = ((int64_t)(v9));
  t315 = ((int64_t)(((int64_t)UINT64_C(2))));
  v13 = gg_function_26(t314, t315, base + 0);
  t316 = ((int64_t)(v11));
  t317 = ((int64_t)(((int64_t)UINT64_C(0))));
  t318 = ((t316) > (t317));
  t319 = t318;
  if (t319) {
  t320 = ((int64_t)(v12));
  t321 = ((int64_t)(((int64_t)UINT64_C(0))));
  t319 = ((t320) > (t321));
  }
  t322 = t319;
  t323 = t322;
  if (t323) {
  t324 = ((int64_t)(v13));
  t325 = ((int64_t)(((int64_t)UINT64_C(0))));
  t323 = ((t324) > (t325));
  }
  t326 = t323;
  t327 = ((int64_t)(((int64_t)UINT64_C(40))));
  gg_function_7(t326, t327, base + 0);
  t328 = ((int64_t)(v10));
  t329 = ((int64_t)(v13));
  t330 = ((int64_t)(v12));
  t331 = ((int64_t)(((int64_t)((uint64_t)(t329) * (uint64_t)(t330)))));
  t332 = ((int64_t)(v11));
  t333 = ((int64_t)(((int64_t)((uint64_t)(t331) * (uint64_t)(t332)))));
  v14 = ((int64_t)(((int64_t)gg_div(t328, t333))));
  t334 = ((int64_t)(v10));
  t335 = ((int64_t)(v14));
  t336 = ((int64_t)(v13));
  t337 = ((int64_t)(((int64_t)((uint64_t)(t335) * (uint64_t)(t336)))));
  t338 = ((int64_t)(v12));
  t339 = ((int64_t)(((int64_t)((uint64_t)(t337) * (uint64_t)(t338)))));
  t340 = ((int64_t)(v11));
  t341 = ((int64_t)(((int64_t)((uint64_t)(t339) * (uint64_t)(t340)))));
  t342 = ((int64_t)(((int64_t)((uint64_t)(t334) - (uint64_t)(t341)))));
  t343 = ((int64_t)(v12));
  t344 = ((int64_t)(v11));
  t345 = ((int64_t)(((int64_t)((uint64_t)(t343) * (uint64_t)(t344)))));
  v15 = ((int64_t)(((int64_t)gg_div(t342, t345))));
  t346 = ((int64_t)(v10));
  t347 = ((int64_t)(v14));
  t348 = ((int64_t)(v13));
  t349 = ((int64_t)(((int64_t)((uint64_t)(t347) * (uint64_t)(t348)))));
  t350 = ((int64_t)(v12));
  t351 = ((int64_t)(((int64_t)((uint64_t)(t349) * (uint64_t)(t350)))));
  t352 = ((int64_t)(v11));
  t353 = ((int64_t)(((int64_t)((uint64_t)(t351) * (uint64_t)(t352)))));
  t354 = ((int64_t)(((int64_t)((uint64_t)(t346) - (uint64_t)(t353)))));
  t355 = ((int64_t)(v15));
  t356 = ((int64_t)(v12));
  t357 = ((int64_t)(((int64_t)((uint64_t)(t355) * (uint64_t)(t356)))));
  t358 = ((int64_t)(v11));
  t359 = ((int64_t)(((int64_t)((uint64_t)(t357) * (uint64_t)(t358)))));
  t360 = ((int64_t)(((int64_t)((uint64_t)(t354) - (uint64_t)(t359)))));
  t361 = ((int64_t)(v11));
  v16 = ((int64_t)(((int64_t)gg_div(t360, t361))));
  t362 = ((int64_t)(v10));
  t363 = ((int64_t)(v14));
  t364 = ((int64_t)(v13));
  t365 = ((int64_t)(((int64_t)((uint64_t)(t363) * (uint64_t)(t364)))));
  t366 = ((int64_t)(v12));
  t367 = ((int64_t)(((int64_t)((uint64_t)(t365) * (uint64_t)(t366)))));
  t368 = ((int64_t)(v11));
  t369 = ((int64_t)(((int64_t)((uint64_t)(t367) * (uint64_t)(t368)))));
  t370 = ((int64_t)(((int64_t)((uint64_t)(t362) - (uint64_t)(t369)))));
  t371 = ((int64_t)(v15));
  t372 = ((int64_t)(v12));
  t373 = ((int64_t)(((int64_t)((uint64_t)(t371) * (uint64_t)(t372)))));
  t374 = ((int64_t)(v11));
  t375 = ((int64_t)(((int64_t)((uint64_t)(t373) * (uint64_t)(t374)))));
  t376 = ((int64_t)(((int64_t)((uint64_t)(t370) - (uint64_t)(t375)))));
  t377 = ((int64_t)(v16));
  t378 = ((int64_t)(v11));
  t379 = ((int64_t)(((int64_t)((uint64_t)(t377) * (uint64_t)(t378)))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t376) - (uint64_t)(t379)))));
  t380 = ((int64_t)(v1));
  t381 = ((int64_t)(((int64_t)UINT64_C(2))));
  t382 = fg156_arg(t380, t381);
  t383 = ((uint64_t)(t382));
  t384 = gg_function_3(t383, base + 0);
  t385 = ((int64_t)(v17));
  fg156_store_index(t384, t385);
  t386 = ((int64_t)(v1));
  t387 = ((int64_t)(((int64_t)UINT64_C(3))));
  t388 = fg156_arg(t386, t387);
  t389 = ((uint64_t)(t388));
  t390 = gg_function_3(t389, base + 0);
  t391 = ((int64_t)(v16));
  fg156_store_index(t390, t391);
  t392 = ((int64_t)(v1));
  t393 = ((int64_t)(((int64_t)UINT64_C(4))));
  t394 = fg156_arg(t392, t393);
  t395 = ((uint64_t)(t394));
  t396 = gg_function_3(t395, base + 0);
  t397 = ((int64_t)(v15));
  fg156_store_index(t396, t397);
  t398 = ((int64_t)(v1));
  t399 = ((int64_t)(((int64_t)UINT64_C(5))));
  t400 = fg156_arg(t398, t399);
  t401 = ((uint64_t)(t400));
  t402 = gg_function_3(t401, base + 0);
  t403 = ((int64_t)(v14));
  fg156_store_index(t402, t403);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
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
  {
  t0.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(99))));
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
