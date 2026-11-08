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
static void gg_function_4(int64_t p0, int64_t base);
static void gg_function_10(int64_t p0, int64_t base);
static void gg_function_13(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_15(int64_t p0, int64_t p1, int64_t base);
static uint64_t gg_function_17(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t gg_function_18(int64_t base);
static void gg_function_19(uint8_t p0, int64_t p1, int64_t base);
static void gg_function_21(int64_t p0, uint8_t p1, uint64_t p2, int64_t base);
static uint64_t gg_function_22(uint64_t p0, uint64_t p1, int64_t base);
static void gg_function_26(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_27(int64_t p0, uint64_t p1, int64_t base);
static int64_t gg_function_28(int64_t p0, int64_t p1, uint8_t p2, int64_t base);
static int64_t gg_function_29(int64_t p0, uint64_t p1, int64_t base);
static uint8_t gg_function_30(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_33(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_34(int64_t p0, int64_t p1, uint8_t p2, int64_t base);
static void gg_function_37(int64_t p0, int64_t p1, int64_t base);
static void gg_function_38(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_39(int64_t p0, int64_t p1, uint8_t p2, int64_t p3, int64_t base);
static int64_t gg_function_40(int64_t base);
static int64_t gg_function_41(int64_t p0, int64_t base);
static uint64_t gg_function_42(int64_t p0, int64_t base);
static int64_t gg_function_44(int64_t base);
static int64_t gg_function_45(int64_t p0, int64_t p1, uint8_t p2, int64_t base);
static int64_t gg_function_46(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static uint8_t gg_function_47(int64_t p0, int64_t p1, int64_t base);
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
static void gg_function_19(uint8_t p0, int64_t p1, int64_t base) {
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
  fg165_fail(t2);
  }
  }
  }
gg_done:;
}
static uint64_t gg_function_18(int64_t base) {
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
  t6 = gg_function_18(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_function_19(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_21(int64_t p0, uint8_t p1, uint64_t p2, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint8_t t8;
  uint64_t t9;
  uint64_t t10;
  uint8_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint8_t t16;
  uint8_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint64_t t51;
  int64_t v0;
  uint8_t v1;
  uint64_t v2;
  int64_t v3;
  uint64_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
  int64_t v8;
  int64_t v9;
  uint64_t v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = v1;
  if (t0) {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  } else {
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v3));
  t3 = ((int64_t)(((int64_t)UINT64_C(2))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = fg165_get(t1, t4);
  v4 = ((uint64_t)(t5));
  t6 = ((uint64_t)(v2));
  t7 = ((uint64_t)(v4));
  t8 = ((t6) <= (t7));
  if (t8) {
  {
  goto gg_done;
  }
  }
  t9 = ((uint64_t)(v4));
  t10 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  v5 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  }
  } else {
  {
  t12 = ((uint64_t)(v4));
  t13 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  v5 = gg_function_22(t12, t13, base + 0);
  }
  }
  t14 = ((uint64_t)(v5));
  t15 = ((uint64_t)(v2));
  t16 = ((t14) < (t15));
  if (t16) {
  {
  v6 = ((uint64_t)(v2));
  }
  } else {
  {
  v6 = ((uint64_t)(v5));
  }
  }
  t17 = v1;
  if (t17) {
  {
  v7 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  }
  } else {
  {
  v7 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(16))))));
  }
  }
  t18 = ((uint64_t)(v6));
  t19 = ((uint64_t)(v7));
  t20 = gg_function_22(t18, t19, base + 0);
  t21 = fg165_alloc(t20);
  v8 = ((int64_t)(t21));
  t22 = ((int64_t)(v8));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t22) != (t23));
  t25 = ((int64_t)(((int64_t)UINT64_C(7))));
  gg_function_19(t24, t25, base + 0);
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(v3));
  t28 = fg165_get(t26, t27);
  t29 = ((uint64_t)(t28));
  v9 = gg_function_3(t29, base + 0);
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(v3));
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = fg165_get(t30, t33);
  v10 = ((uint64_t)(t34));
  t35 = ((uint64_t)(v10));
  t36 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t35) != (t36));
  if (t37) {
  {
  t38 = ((int64_t)(v8));
  t39 = ((int64_t)(v9));
  t40 = ((uint64_t)(v10));
  t41 = ((uint64_t)(v7));
  t42 = gg_function_22(t40, t41, base + 0);
  fg165_copy(t38, t39, t42);
  }
  }
  t43 = ((int64_t)(v9));
  fg165_free(t43);
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(v3));
  t46 = ((uint64_t)(((uint64_t)(v8))));
  fg165_put(t44, t45, t46);
  t47 = ((int64_t)(v0));
  t48 = ((int64_t)(v3));
  t49 = ((int64_t)(((int64_t)UINT64_C(2))));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  t51 = ((uint64_t)(v6));
  fg165_put(t47, t50, t51);
  }
gg_done:;
}
static uint64_t gg_function_17(uint64_t p0, uint64_t p1, int64_t base) {
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
  t2 = gg_function_18(base + 0);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(v1));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((t1) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_19(t6, t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_26(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
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
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint8_t t18;
  uint64_t t19;
  uint64_t t20;
  int64_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
  uint64_t t46;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) != (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_function_19(t2, t3, base + 0);
  v2 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t4 = ((uint64_t)(v2));
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(4))));
  t7 = fg165_get(t5, t6);
  t8 = ((uint64_t)(t7));
  if (!(((t4) < (t8)))) goto label2;
  {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(3))));
  t11 = fg165_get(t9, t10);
  t12 = ((uint64_t)(t11));
  t13 = gg_function_3(t12, base + 0);
  t14 = ((uint64_t)(v2));
  t15 = fg165_ptr(t13, t14);
  t16 = ((uint64_t)(t15));
  t17 = ((uint64_t)(((uint64_t)(v1))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  goto gg_done;
  }
  }
  t19 = v2;
  t20 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v2 = ((uint64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  }
  goto label1;
label2:;
  t21 = ((int64_t)(v0));
  t22 = ((uint8_t)UINT64_C(1));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(4))));
  t25 = fg165_get(t23, t24);
  t26 = ((uint64_t)(t25));
  t27 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t28 = gg_function_17(t26, t27, base + 0);
  gg_function_21(t21, t22, t28, base + 0);
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(3))));
  t31 = fg165_get(t29, t30);
  t32 = ((uint64_t)(t31));
  t33 = gg_function_3(t32, base + 0);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(4))));
  t36 = fg165_get(t34, t35);
  t37 = ((uint64_t)(t36));
  t38 = ((uint64_t)(((uint64_t)(v1))));
  fg165_ptr_put(t33, t37, t38);
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(4))));
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(4))));
  t43 = fg165_get(t41, t42);
  t44 = ((uint64_t)(t43));
  t45 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t46 = ((uint64_t)(((uint64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  fg165_put(t39, t40, t46);
  }
gg_done:;
}
static int64_t gg_function_15(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint8_t t17;
  uint64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((uint64_t)(v2));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = fg165_get(t2, t3);
  t5 = ((uint64_t)(t4));
  if (!(((t1) < (t5)))) goto label2;
  {
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg165_get(t6, t7);
  t9 = ((uint64_t)(t8));
  t10 = gg_function_3(t9, base + 0);
  t11 = ((uint64_t)(v2));
  t12 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t13 = ((uint64_t)(((uint64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = fg165_ptr(t10, t13);
  t15 = ((uint64_t)(t14));
  t16 = ((uint64_t)(((uint64_t)(v1))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t0 = ((int64_t)(((int64_t)(v2))));
  goto gg_done;
  }
  }
  t18 = v2;
  t19 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v2 = ((uint64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  }
  goto label1;
label2:;
  t20 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t20)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_13(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
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
  uint64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  uint64_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  int64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  int64_t t45;
  uint64_t t46;
  uint64_t t47;
  uint64_t t48;
  uint64_t t49;
  uint64_t t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint64_t t56;
  uint64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  uint64_t t62;
  uint64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint64_t t69;
  uint64_t t70;
  int64_t t71;
  uint64_t t72;
  uint64_t t73;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  uint64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  if (t5) {
  {
  t6 = ((int64_t)(((int64_t)UINT64_C(12))));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = fg165_effect(t6, t7, t8, t9);
  (void)(t10);
  }
  }
  goto gg_done;
  }
  }
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(v1));
  t13 = gg_function_15(t11, t12, base + 0);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) >= (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) != (t18));
  if (t19) {
  {
  t20 = ((int64_t)(((int64_t)UINT64_C(12))));
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = fg165_effect(t20, t21, t22, t23);
  (void)(t24);
  }
  }
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((uint8_t)UINT64_C(0));
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = fg165_get(t27, t28);
  t30 = ((uint64_t)(t29));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t32 = gg_function_17(t30, t31, base + 0);
  gg_function_21(t25, t26, t32, base + 0);
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg165_get(t33, t34);
  t36 = ((uint64_t)(t35));
  v3 = gg_function_3(t36, base + 0);
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t39 = fg165_get(t37, t38);
  v4 = ((uint64_t)(t39));
  t40 = ((int64_t)(v3));
  t41 = ((uint64_t)(v4));
  t42 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t43 = ((uint64_t)(((uint64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t44 = ((uint64_t)(((uint64_t)(v1))));
  fg165_ptr_put(t40, t43, t44);
  t45 = ((int64_t)(v3));
  t46 = ((uint64_t)(v4));
  t47 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t48 = ((uint64_t)(((uint64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t50 = ((uint64_t)(((uint64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  t51 = ((uint64_t)(((uint64_t)(v2))));
  fg165_ptr_put(t45, t50, t51);
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  t54 = ((uint64_t)(v4));
  t55 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t56 = ((uint64_t)(((uint64_t)((uint64_t)(t54) + (uint64_t)(t55)))));
  fg165_put(t52, t53, t56);
  v5 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t57 = ((uint64_t)(v5));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t59 = ((int64_t)(v1));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = fg165_effect(t58, t59, t60, t61);
  t63 = ((uint64_t)(t62));
  if (!(((t57) < (t63)))) goto label2;
  {
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t66 = ((int64_t)(v1));
  t67 = ((int64_t)(((int64_t)(v5))));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = fg165_effect(t65, t66, t67, t68);
  t70 = ((uint64_t)(t69));
  t71 = gg_function_3(t70, base + 0);
  gg_function_26(t64, t71, base + 0);
  t72 = v5;
  t73 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v5 = ((uint64_t)((uint64_t)(t72) + (uint64_t)(t73)));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_34(int64_t p0, int64_t p1, uint8_t p2, int64_t base) {
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
  uint64_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
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
  uint64_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint8_t t39;
  uint8_t t40;
  uint8_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
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
  uint8_t t60;
  uint8_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  uint8_t t79;
  uint8_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint64_t t85;
  uint64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  uint64_t t92;
  uint8_t t93;
  uint8_t t94;
  uint8_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t v0;
  int64_t v1;
  uint8_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_function_19(t3, t4, base + 0);
  t5 = ((int64_t)(((int64_t)UINT64_C(42))));
  fg165_mark(t5);
  t6 = ((int64_t)(((int64_t)UINT64_C(10))));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = fg165_effect(t6, t7, t8, t9);
  t11 = ((uint64_t)(t10));
  v3 = gg_function_3(t11, base + 0);
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) == (t13));
  if (t14) {
  {
  t15 = v2;
  t16 = (!(t15));
  if (t16) {
  {
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg165_log(t17, t18, t19);
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(((int64_t)UINT64_C(11))));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg165_literal(t22);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg165_effect(t20, t21, t24, t25);
  t27 = ((uint64_t)(t26));
  v4 = gg_function_3(t27, base + 0);
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) != (t29));
  t31 = t30;
  if (t31) {
  t32 = ((int64_t)(((int64_t)UINT64_C(13))));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg165_effect(t32, t33, t34, t35);
  t37 = ((uint64_t)(t36));
  t38 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t37) == (t38));
  }
  t39 = t31;
  if (t39) {
  {
  t40 = v2;
  t41 = (!(t40));
  if (t41) {
  {
  t42 = ((int64_t)(((int64_t)UINT64_C(1))));
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg165_log(t42, t43, t44);
  }
  }
  t45 = ((int64_t)(((int64_t)UINT64_C(12))));
  t46 = ((int64_t)(v3));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = fg165_effect(t45, t46, t47, t48);
  (void)(t49);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t50 = ((int64_t)(((int64_t)UINT64_C(11))));
  t51 = ((int64_t)(v3));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = fg165_literal(t52);
  t54 = ((int64_t)(t53));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = fg165_effect(t50, t51, t54, t55);
  t57 = ((uint64_t)(t56));
  v5 = gg_function_3(t57, base + 0);
  t58 = ((int64_t)(v5));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((t58) == (t59));
  if (t60) {
  {
  t61 = v2;
  t62 = (!(t61));
  if (t62) {
  {
  t63 = ((int64_t)(((int64_t)UINT64_C(2))));
  t64 = ((int64_t)(v1));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg165_log(t63, t64, t65);
  }
  }
  t66 = ((int64_t)(((int64_t)UINT64_C(12))));
  t67 = ((int64_t)(v3));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = fg165_effect(t66, t67, t68, t69);
  (void)(t70);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t71 = ((int64_t)(((int64_t)UINT64_C(14))));
  t72 = ((int64_t)(v5));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = fg165_effect(t71, t72, t73, t74);
  t76 = ((uint64_t)(t75));
  v6 = gg_function_3(t76, base + 0);
  t77 = ((int64_t)(v6));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = ((t77) == (t78));
  t80 = t79;
  if (!t80) {
  t81 = ((int64_t)(((int64_t)UINT64_C(6))));
  t82 = ((int64_t)(v6));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = fg165_effect(t81, t82, t83, t84);
  t86 = ((uint64_t)(t85));
  t87 = ((int64_t)(((int64_t)UINT64_C(7))));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = fg165_effect(t87, t88, t89, t90);
  t92 = ((uint64_t)(t91));
  t80 = ((t86) != (t92));
  }
  t93 = t80;
  if (t93) {
  {
  t94 = v2;
  t95 = (!(t94));
  if (t95) {
  {
  t96 = ((int64_t)(((int64_t)UINT64_C(3))));
  t97 = ((int64_t)(v1));
  t98 = ((int64_t)(v6));
  fg165_log(t96, t97, t98);
  }
  }
  t99 = ((int64_t)(((int64_t)UINT64_C(12))));
  t100 = ((int64_t)(v3));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = fg165_effect(t99, t100, t101, t102);
  (void)(t103);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t104 = ((int64_t)(((int64_t)UINT64_C(4))));
  t105 = ((int64_t)(v1));
  t106 = ((int64_t)(v6));
  fg165_log(t104, t105, t106);
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(v6));
  t109 = ((int64_t)(v3));
  gg_function_13(t107, t108, t109, base + 0);
  t0 = ((int64_t)(v6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_42(int64_t p0, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  int64_t v0;
  uint64_t v1;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_19(t3, t4, base + 0);
  v1 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t5 = ((int64_t)(v0));
  t6 = ((uint64_t)(v1));
  t7 = fg165_byte(t5, t6);
  t8 = ((uint64_t)(t7));
  t9 = ((uint64_t)(((int64_t)UINT64_C(0))));
  if (!(((t8) != (t9)))) goto label2;
  {
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  v1 = gg_function_17(t10, t11, base + 0);
  }
  goto label1;
label2:;
  t0 = ((uint64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_45(int64_t p0, int64_t p1, uint8_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  uint64_t t5;
  uint64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint8_t t20;
  uint64_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint8_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  int64_t v0;
  int64_t v1;
  uint8_t v2;
  uint64_t v3;
  uint64_t v4;
  uint8_t v5;
  uint64_t v6;
  int64_t v7;
  uint64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  v3 = gg_function_42(t1, base + 0);
  t2 = ((int64_t)(v1));
  v4 = gg_function_42(t2, base + 0);
  t3 = v2;
  t4 = t3;
  if (t4) {
  t5 = ((uint64_t)(v3));
  t6 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) != (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v0));
  t10 = ((uint64_t)(v3));
  t11 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t12 = ((uint64_t)(((uint64_t)((uint64_t)(t10) - (uint64_t)(t11)))));
  t13 = fg165_byte(t9, t12);
  t14 = ((uint64_t)(t13));
  t15 = ((uint64_t)(((int64_t)UINT64_C(47))));
  t8 = ((t14) != (t15));
  }
  v5 = t8;
  t16 = ((uint64_t)(v3));
  t17 = ((uint64_t)(v4));
  t18 = gg_function_17(t16, t17, base + 0);
  t20 = v5;
  if (t20) {
  {
  t19 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  }
  } else {
  {
  t19 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t21 = ((uint64_t)(t19));
  v6 = gg_function_17(t18, t21, base + 0);
  t22 = ((uint64_t)(v6));
  t23 = fg165_alloc(t22);
  v7 = ((int64_t)(t23));
  t24 = ((int64_t)(v7));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t24) != (t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_19(t26, t27, base + 0);
  t28 = ((int64_t)(v7));
  t29 = ((int64_t)(v0));
  t30 = ((uint64_t)(v3));
  fg165_copy(t28, t29, t30);
  v8 = ((uint64_t)(v3));
  t31 = v5;
  if (t31) {
  {
  t32 = ((int64_t)(v7));
  t33 = ((uint64_t)(v8));
  t34 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(47))))));
  fg165_byte_put(t32, t33, t34);
  t35 = v8;
  t36 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v8 = ((uint64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  }
  }
  t37 = ((int64_t)(v7));
  t38 = ((int64_t)(((int64_t)(v8))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t40 = ((int64_t)(v1));
  t41 = ((uint64_t)(v4));
  t42 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t43 = ((uint64_t)(((uint64_t)((uint64_t)(t41) + (uint64_t)(t42)))));
  fg165_copy(t39, t40, t43);
  t0 = ((int64_t)(v7));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_46(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
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
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(v1));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto gg_done;
  }
  }
  t7 = ((int64_t)(((int64_t)UINT64_C(3))));
  t8 = fg165_literal(t7);
  v4 = ((int64_t)(t8));
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) != (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) == (t13));
  if (t14) {
  {
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
  }
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((t18) == (t19));
  if (t20) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) == (t22));
  if (t23) {
  {
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
  }
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t24) == (t25));
  if (t26) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_41(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t v0;
  uint64_t v1;
  int64_t v2;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = gg_function_42(t1, base + 0);
  t3 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  v1 = gg_function_17(t2, t3, base + 0);
  t4 = ((uint64_t)(v1));
  t5 = fg165_alloc(t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) != (t7));
  t9 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_19(t8, t9, base + 0);
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(v0));
  t12 = ((uint64_t)(v1));
  fg165_copy(t10, t11, t12);
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_47(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint8_t t7;
  uint64_t t8;
  uint64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  int64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint8_t t18;
  uint64_t t19;
  uint64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint8_t t28;
  uint8_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint8_t t37;
  uint8_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  v2 = gg_function_42(t1, base + 0);
  t2 = ((int64_t)(v1));
  v3 = gg_function_42(t2, base + 0);
  t3 = ((uint64_t)(v2));
  t4 = ((uint64_t)(v3));
  t5 = ((uint64_t)(((int64_t)UINT64_C(3))));
  t6 = ((uint64_t)(((uint64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = ((t3) < (t6));
  if (t7) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  v4 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t8 = ((uint64_t)(v4));
  t9 = ((uint64_t)(v3));
  if (!(((t8) < (t9)))) goto label2;
  {
  t10 = ((int64_t)(v0));
  t11 = ((uint64_t)(v4));
  t12 = fg165_byte(t10, t11);
  t13 = ((uint64_t)(t12));
  t14 = ((int64_t)(v1));
  t15 = ((uint64_t)(v4));
  t16 = fg165_byte(t14, t15);
  t17 = ((uint64_t)(t16));
  t18 = ((t13) != (t17));
  if (t18) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t19 = v4;
  t20 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v4 = ((uint64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  }
  goto label1;
label2:;
  t21 = ((int64_t)(v0));
  t22 = ((uint64_t)(v2));
  t23 = ((uint64_t)(((int64_t)UINT64_C(3))));
  t24 = ((uint64_t)(((uint64_t)((uint64_t)(t22) - (uint64_t)(t23)))));
  t25 = fg165_byte(t21, t24);
  t26 = ((uint64_t)(t25));
  t27 = ((uint64_t)(((int64_t)UINT64_C(46))));
  t28 = ((t26) == (t27));
  t29 = t28;
  if (t29) {
  t30 = ((int64_t)(v0));
  t31 = ((uint64_t)(v2));
  t32 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t33 = ((uint64_t)(((uint64_t)((uint64_t)(t31) - (uint64_t)(t32)))));
  t34 = fg165_byte(t30, t33);
  t35 = ((uint64_t)(t34));
  t36 = ((uint64_t)(((int64_t)UINT64_C(115))));
  t29 = ((t35) == (t36));
  }
  t37 = t29;
  t38 = t37;
  if (t38) {
  t39 = ((int64_t)(v0));
  t40 = ((uint64_t)(v2));
  t41 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t42 = ((uint64_t)(((uint64_t)((uint64_t)(t40) - (uint64_t)(t41)))));
  t43 = fg165_byte(t39, t42);
  t44 = ((uint64_t)(t43));
  t45 = ((uint64_t)(((int64_t)UINT64_C(111))));
  t38 = ((t44) == (t45));
  }
  t0 = t38;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_44(int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  {
  v0 = ((uint64_t)(((int64_t)UINT64_C(1024))));
label1:;
  if (!(((uint8_t)UINT64_C(1)))) goto label2;
  {
  t1 = ((uint64_t)(v0));
  t2 = fg165_alloc(t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_function_19(t5, t6, base + 0);
  t7 = ((int64_t)(((int64_t)UINT64_C(21))));
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)(v0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg165_effect(t7, t8, t9, t10);
  t12 = ((uint64_t)(t11));
  v2 = gg_function_3(t12, base + 0);
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) > (t14));
  if (t15) {
  {
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
  }
  t16 = ((int64_t)(v1));
  fg165_free(t16);
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t20 = ((uint64_t)(v0));
  t21 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  v0 = gg_function_22(t20, t21, base + 0);
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_40(int64_t base) {
  int64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  int64_t t4;
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
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint8_t t24;
  int64_t t25;
  uint64_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  uint64_t t42;
  int64_t t43;
  uint64_t t44;
  uint64_t t45;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  {
  v0 = ((uint64_t)(((int64_t)UINT64_C(1024))));
label1:;
  if (!(((uint8_t)UINT64_C(1)))) goto label2;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t3 = gg_function_17(t1, t2, base + 0);
  t4 = fg165_alloc(t3);
  v1 = ((int64_t)(t4));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t5) != (t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_19(t7, t8, base + 0);
  t9 = ((int64_t)(((int64_t)UINT64_C(20))));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)(v0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg165_effect(t9, t10, t11, t12);
  t14 = ((uint64_t)(t13));
  v2 = gg_function_3(t14, base + 0);
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) < (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v1));
  fg165_free(t18);
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = fg165_literal(t19);
  t21 = ((int64_t)(t20));
  t0 = gg_function_41(t21, base + 0);
  goto gg_done;
  }
  }
  t22 = ((uint64_t)(((uint64_t)(v2))));
  t23 = ((uint64_t)(v0));
  t24 = ((t22) < (t23));
  if (t24) {
  {
  t25 = ((int64_t)(v1));
  t26 = ((uint64_t)(((uint64_t)(v2))));
  t27 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg165_byte_put(t25, t26, t27);
  v3 = ((int64_t)(v2));
label4:;
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t28) > (t29)))) goto label5;
  {
  t30 = v3;
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t30) - (uint64_t)(t31)));
  t32 = ((int64_t)(v1));
  t33 = ((uint64_t)(((uint64_t)(v3))));
  t34 = fg165_byte(t32, t33);
  t35 = ((uint64_t)(t34));
  t36 = ((uint64_t)(((int64_t)UINT64_C(47))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(v3));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  t41 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))))));
  t42 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg165_byte_put(t38, t41, t42);
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
  }
  }
  goto label4;
label5:;
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
  }
  t43 = ((int64_t)(v1));
  fg165_free(t43);
  t44 = ((uint64_t)(v0));
  t45 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  v0 = gg_function_22(t44, t45, base + 0);
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_39(int64_t p0, int64_t p1, uint8_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
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
  uint8_t t14;
  int64_t t15;
  int64_t t16;
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
  uint64_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  uint64_t t38;
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
  uint64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  uint64_t t60;
  uint64_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  uint64_t t71;
  uint64_t t72;
  int64_t t73;
  int64_t t74;
  uint8_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint64_t t80;
  uint64_t t81;
  int64_t t82;
  int64_t t83;
  uint8_t t84;
  int64_t t85;
  int64_t t86;
  uint8_t t87;
  uint8_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint64_t t96;
  uint8_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint64_t t107;
  uint64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  uint64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  uint8_t t123;
  int64_t t124;
  int64_t t125;
  uint8_t t126;
  uint8_t t127;
  uint8_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  uint8_t t135;
  int64_t t136;
  int64_t t137;
  uint8_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  uint64_t t143;
  uint64_t t144;
  uint64_t t145;
  uint8_t t146;
  int64_t t147;
  int64_t t148;
  uint8_t t149;
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
  int64_t v0;
  int64_t v1;
  uint8_t v2;
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
  uint8_t v20;
  int64_t v21;
  int64_t v22;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(43))));
  fg165_mark(t1);
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(v3));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) == (t3));
  if (t4) {
  {
  v4 = gg_function_40(base + 0);
  v5 = gg_function_44(base + 0);
  }
  }
  t5 = ((int64_t)(((int64_t)UINT64_C(4))));
  t6 = fg165_literal(t5);
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(v1));
  t9 = ((uint8_t)UINT64_C(0));
  v6 = gg_function_45(t7, t8, t9, base + 0);
  t10 = ((int64_t)(v6));
  t11 = ((int64_t)(((int64_t)UINT64_C(5))));
  t12 = fg165_literal(t11);
  t13 = ((int64_t)(t12));
  t14 = ((uint8_t)UINT64_C(0));
  v7 = gg_function_45(t10, t13, t14, base + 0);
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t15 = ((int64_t)(v10));
  t16 = ((int64_t)(((int64_t)UINT64_C(3))));
  if (!(((t15) < (t16)))) goto label2;
  {
  t17 = ((int64_t)(v10));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(v5));
  v11 = gg_function_46(t17, t18, t19, t20, base + 0);
  t21 = ((int64_t)(v11));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) != (t22));
  if (t23) {
  {
  t24 = ((int64_t)(((int64_t)UINT64_C(22))));
  t25 = ((int64_t)(v11));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = fg165_effect(t24, t25, t26, t27);
  t29 = ((uint64_t)(t28));
  v12 = gg_function_3(t29, base + 0);
  t30 = ((int64_t)(v12));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((t30) != (t31));
  if (t32) {
  {
  t33 = ((int64_t)(((int64_t)UINT64_C(23))));
  t34 = ((int64_t)(v12));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = fg165_effect(t33, t34, t35, t36);
  t38 = ((uint64_t)(t37));
  v13 = gg_function_3(t38, base + 0);
label4:;
  t39 = ((int64_t)(v13));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t39) != (t40)))) goto label5;
  {
  t41 = ((int64_t)(v13));
  t42 = ((int64_t)(v7));
  t43 = gg_function_47(t41, t42, base + 0);
  t44 = t43;
  if (t44) {
  {
  t45 = ((int64_t)(v11));
  t46 = ((int64_t)(v13));
  t47 = ((uint8_t)UINT64_C(1));
  v14 = gg_function_45(t45, t46, t47, base + 0);
  t48 = ((int64_t)(((int64_t)UINT64_C(25))));
  t49 = ((int64_t)(v14));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = fg165_effect(t48, t49, t50, t51);
  t53 = ((uint64_t)(t52));
  t54 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t55 = ((t53) != (t54));
  if (t55) {
  {
  t56 = ((int64_t)(((int64_t)UINT64_C(10))));
  t57 = ((int64_t)(v14));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = fg165_effect(t56, t57, t58, t59);
  t61 = ((uint64_t)(t60));
  v15 = gg_function_3(t61, base + 0);
  t62 = ((int64_t)(v15));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((t62) != (t63));
  if (t64) {
  {
  t65 = ((int64_t)(((int64_t)UINT64_C(11))));
  t66 = ((int64_t)(v15));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = fg165_literal(t67);
  t69 = ((int64_t)(t68));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = fg165_effect(t65, t66, t69, t70);
  t72 = ((uint64_t)(t71));
  v16 = gg_function_3(t72, base + 0);
  t73 = ((int64_t)(v16));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((t73) != (t74));
  if (t75) {
  {
  t76 = ((int64_t)(((int64_t)UINT64_C(13))));
  t77 = ((int64_t)(v16));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = fg165_effect(t76, t77, t78, t79);
  t81 = ((uint64_t)(t80));
  v17 = gg_function_3(t81, base + 0);
  t82 = ((int64_t)(v17));
  t83 = ((int64_t)(v8));
  t84 = ((t82) > (t83));
  if (t84) {
  {
  v8 = ((int64_t)(v17));
  t85 = ((int64_t)(v9));
  fg165_free(t85);
  t86 = ((int64_t)(v14));
  v9 = gg_function_41(t86, base + 0);
  }
  }
  }
  } else {
  {
  t87 = v2;
  t88 = (!(t87));
  if (t88) {
  {
  t89 = ((int64_t)(((int64_t)UINT64_C(5))));
  t90 = ((int64_t)(v14));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg165_log(t89, t90, t91);
  }
  }
  }
  }
  t92 = ((int64_t)(((int64_t)UINT64_C(12))));
  t93 = ((int64_t)(v15));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = fg165_effect(t92, t93, t94, t95);
  (void)(t96);
  }
  } else {
  {
  t97 = v2;
  t98 = (!(t97));
  if (t98) {
  {
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = ((int64_t)(v14));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg165_log(t99, t100, t101);
  }
  }
  }
  }
  }
  }
  t102 = ((int64_t)(v14));
  fg165_free(t102);
  }
  }
  t103 = ((int64_t)(((int64_t)UINT64_C(23))));
  t104 = ((int64_t)(v12));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = fg165_effect(t103, t104, t105, t106);
  t108 = ((uint64_t)(t107));
  v13 = gg_function_3(t108, base + 0);
  }
  goto label4;
label5:;
  t109 = ((int64_t)(((int64_t)UINT64_C(24))));
  t110 = ((int64_t)(v12));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = fg165_effect(t109, t110, t111, t112);
  (void)(t113);
  }
  }
  }
  }
  t114 = v10;
  t115 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t114) + (uint64_t)(t115)));
  }
  goto label1;
label2:;
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((int64_t)(v8));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t118 = ((t116) == (t117));
  if (t118) {
  {
  t119 = ((int64_t)(v6));
  t120 = ((int64_t)(((int64_t)UINT64_C(6))));
  t121 = fg165_literal(t120);
  t122 = ((int64_t)(t121));
  t123 = ((uint8_t)UINT64_C(0));
  v19 = gg_function_45(t119, t122, t123, base + 0);
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  v20 = ((uint8_t)UINT64_C(0));
label7:;
  t124 = ((int64_t)(v10));
  t125 = ((int64_t)(((int64_t)UINT64_C(3))));
  t126 = ((t124) < (t125));
  t127 = t126;
  if (t127) {
  t128 = v20;
  t127 = (!(t128));
  }
  if (!(t127)) goto label8;
  {
  t129 = ((int64_t)(v10));
  t130 = ((int64_t)(v3));
  t131 = ((int64_t)(v4));
  t132 = ((int64_t)(v5));
  v21 = gg_function_46(t129, t130, t131, t132, base + 0);
  t133 = ((int64_t)(v21));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((t133) != (t134));
  if (t135) {
  {
  t136 = ((int64_t)(v21));
  t137 = ((int64_t)(v19));
  t138 = ((uint8_t)UINT64_C(1));
  v22 = gg_function_45(t136, t137, t138, base + 0);
  t139 = ((int64_t)(((int64_t)UINT64_C(26))));
  t140 = ((int64_t)(v22));
  t141 = ((int64_t)(((int64_t)UINT64_C(0))));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = fg165_effect(t139, t140, t141, t142);
  t144 = ((uint64_t)(t143));
  t145 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t146 = ((t144) != (t145));
  if (t146) {
  {
  t147 = ((int64_t)(v0));
  t148 = ((int64_t)(v22));
  t149 = v2;
  v18 = gg_function_34(t147, t148, t149, base + 0);
  v20 = ((uint8_t)UINT64_C(1));
  }
  }
  t150 = ((int64_t)(v22));
  fg165_free(t150);
  }
  }
  t151 = v10;
  t152 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t151) + (uint64_t)(t152)));
  }
  goto label7;
label8:;
  t153 = ((int64_t)(v19));
  fg165_free(t153);
  }
  } else {
  {
  t154 = ((int64_t)(v0));
  t155 = ((int64_t)(v9));
  t156 = v2;
  v18 = gg_function_34(t154, t155, t156, base + 0);
  }
  }
  t157 = ((int64_t)(v9));
  fg165_free(t157);
  t158 = ((int64_t)(v7));
  fg165_free(t158);
  t159 = ((int64_t)(v6));
  fg165_free(t159);
  t160 = ((int64_t)(v4));
  fg165_free(t160);
  t161 = ((int64_t)(v5));
  fg165_free(t161);
  t0 = ((int64_t)(v18));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_38(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)UINT64_C(15))));
  if (!(((t0) < (t1)))) goto label2;
  {
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t4)))));
  t6 = fg165_literal(t5);
  t7 = ((int64_t)(t6));
  t8 = ((uint8_t)UINT64_C(1));
  t9 = ((int64_t)(v1));
  t10 = gg_function_39(t2, t7, t8, t9, base + 0);
  (void)(t10);
  t11 = v2;
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t11) + (uint64_t)(t12)));
  }
  goto label1;
label2:;
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg165_env(t13);
  v3 = ((int64_t)(t14));
  t15 = ((int64_t)(v3));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) != (t16));
  if (t17) {
  {
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(v3));
  t20 = ((uint8_t)UINT64_C(0));
  t21 = gg_function_34(t18, t19, t20, base + 0);
  (void)(t21);
  }
  }
  }
gg_done:;
}
static void gg_function_37(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  uint64_t t25;
  int64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint8_t t37;
  int64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  uint64_t t48;
  uint64_t t49;
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  uint64_t t53;
  uint64_t t54;
  int64_t t55;
  uint64_t t56;
  uint64_t t57;
  uint64_t t58;
  int64_t t59;
  uint64_t t60;
  uint64_t t61;
  uint64_t t62;
  uint64_t t63;
  uint64_t t64;
  uint64_t t65;
  uint64_t t66;
  int64_t t67;
  uint64_t t68;
  uint64_t t69;
  uint64_t t70;
  uint64_t t71;
  uint64_t t72;
  int64_t t73;
  uint64_t t74;
  uint64_t t75;
  uint64_t t76;
  uint64_t t77;
  uint64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint64_t t83;
  uint64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  uint64_t t89;
  uint64_t t90;
  uint64_t t91;
  uint64_t t92;
  int64_t t93;
  int64_t t94;
  uint8_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint64_t t100;
  int64_t t101;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  uint64_t v6;
  uint64_t v7;
  int64_t v8;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  v2 = gg_function_15(t0, t1, base + 0);
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t2) < (t3));
  if (t4) {
  {
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg165_get(t5, t6);
  t8 = ((uint64_t)(t7));
  v3 = gg_function_3(t8, base + 0);
  t9 = ((int64_t)(v3));
  t10 = ((uint64_t)(((uint64_t)(v2))));
  t11 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t12 = ((uint64_t)(((uint64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t14 = ((uint64_t)(((uint64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  t15 = fg165_ptr(t9, t14);
  t16 = ((uint64_t)(t15));
  v4 = gg_function_3(t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(3))));
  t19 = fg165_get(t17, t18);
  t20 = ((uint64_t)(t19));
  v5 = gg_function_3(t20, base + 0);
  v6 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v7 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t21 = ((uint64_t)(v7));
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(4))));
  t24 = fg165_get(t22, t23);
  t25 = ((uint64_t)(t24));
  if (!(((t21) < (t25)))) goto label2;
  {
  t26 = ((int64_t)(v5));
  t27 = ((uint64_t)(v7));
  t28 = fg165_ptr(t26, t27);
  t29 = ((uint64_t)(t28));
  v8 = gg_function_3(t29, base + 0);
  t30 = ((int64_t)(((int64_t)UINT64_C(5))));
  t31 = ((int64_t)(v8));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = fg165_effect(t30, t31, t32, t33);
  t35 = ((uint64_t)(t34));
  t36 = ((uint64_t)(((uint64_t)(v1))));
  t37 = ((t35) != (t36));
  if (t37) {
  {
  t38 = ((int64_t)(v5));
  t39 = ((uint64_t)(v6));
  t40 = ((uint64_t)(((uint64_t)(v8))));
  fg165_ptr_put(t38, t39, t40);
  t41 = v6;
  t42 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v6 = ((uint64_t)((uint64_t)(t41) + (uint64_t)(t42)));
  }
  }
  t43 = v7;
  t44 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v7 = ((uint64_t)((uint64_t)(t43) + (uint64_t)(t44)));
  }
  goto label1;
label2:;
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(4))));
  t47 = ((uint64_t)(v6));
  fg165_put(t45, t46, t47);
  v7 = ((uint64_t)(((uint64_t)(v2))));
label4:;
  t48 = ((uint64_t)(v7));
  t49 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t50 = ((uint64_t)(((uint64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = fg165_get(t51, t52);
  t54 = ((uint64_t)(t53));
  if (!(((t50) < (t54)))) goto label5;
  {
  t55 = ((int64_t)(v3));
  t56 = ((uint64_t)(v7));
  t57 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t58 = ((uint64_t)(((uint64_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  t59 = ((int64_t)(v3));
  t60 = ((uint64_t)(v7));
  t61 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t62 = ((uint64_t)(((uint64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  t63 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t64 = ((uint64_t)(((uint64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t65 = fg165_ptr(t59, t64);
  t66 = ((uint64_t)(t65));
  fg165_ptr_put(t55, t58, t66);
  t67 = ((int64_t)(v3));
  t68 = ((uint64_t)(v7));
  t69 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t70 = ((uint64_t)(((uint64_t)((uint64_t)(t68) * (uint64_t)(t69)))));
  t71 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t72 = ((uint64_t)(((uint64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  t73 = ((int64_t)(v3));
  t74 = ((uint64_t)(v7));
  t75 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t76 = ((uint64_t)(((uint64_t)((uint64_t)(t74) + (uint64_t)(t75)))));
  t77 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t78 = ((uint64_t)(((uint64_t)((uint64_t)(t76) * (uint64_t)(t77)))));
  t79 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t80 = ((uint64_t)(((uint64_t)((uint64_t)(t78) + (uint64_t)(t79)))));
  t81 = fg165_ptr(t73, t80);
  t82 = ((uint64_t)(t81));
  fg165_ptr_put(t67, t72, t82);
  t83 = v7;
  t84 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v7 = ((uint64_t)((uint64_t)(t83) + (uint64_t)(t84)));
  }
  goto label4;
label5:;
  t85 = ((int64_t)(v0));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((int64_t)(v0));
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  t89 = fg165_get(t87, t88);
  t90 = ((uint64_t)(t89));
  t91 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t92 = ((uint64_t)(((uint64_t)((uint64_t)(t90) - (uint64_t)(t91)))));
  fg165_put(t85, t86, t92);
  t93 = ((int64_t)(v4));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((t93) != (t94));
  if (t95) {
  {
  t96 = ((int64_t)(((int64_t)UINT64_C(12))));
  t97 = ((int64_t)(v4));
  t98 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = fg165_effect(t96, t97, t98, t99);
  (void)(t100);
  }
  }
  t101 = ((int64_t)(((int64_t)UINT64_C(41))));
  fg165_mark(t101);
  }
gg_done:;
}
static int64_t gg_function_29(int64_t p0, uint64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  int64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(4))));
  t4 = fg165_get(t2, t3);
  t5 = ((uint64_t)(t4));
  t6 = ((t1) < (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_19(t6, t7, base + 0);
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(3))));
  t10 = fg165_get(t8, t9);
  t11 = ((uint64_t)(t10));
  t12 = gg_function_3(t11, base + 0);
  t13 = ((uint64_t)(v1));
  t14 = fg165_ptr(t12, t13);
  t15 = ((uint64_t)(t14));
  t0 = gg_function_3(t15, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_33(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint8_t t15;
  uint64_t t16;
  uint64_t t17;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((uint64_t)(v2));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(4))));
  t4 = fg165_get(t2, t3);
  t5 = ((uint64_t)(t4));
  if (!(((t1) < (t5)))) goto label2;
  {
  t6 = ((int64_t)(v0));
  t7 = ((uint64_t)(v2));
  v3 = gg_function_29(t6, t7, base + 0);
  t8 = ((int64_t)(((int64_t)UINT64_C(4))));
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg165_effect(t8, t9, t10, t11);
  t13 = ((uint64_t)(t12));
  t14 = ((uint64_t)(((uint64_t)(v1))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  t16 = v2;
  t17 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v2 = ((uint64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_30(int64_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint8_t t33;
  uint64_t t34;
  uint64_t t35;
  int64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint64_t t39;
  int64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  v0 = p0;
  v1 = p1;
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
  t8 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_19(t7, t8, base + 0);
  v2 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t9 = ((int64_t)(v0));
  t10 = ((uint64_t)(v2));
  t11 = fg165_byte(t9, t10);
  t12 = ((uint64_t)(t11));
  t13 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) != (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v1));
  t17 = ((uint64_t)(v2));
  t18 = fg165_byte(t16, t17);
  t19 = ((uint64_t)(t18));
  t20 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t19) != (t20));
  }
  if (!(t15)) goto label2;
  {
  t21 = ((int64_t)(v0));
  t22 = ((uint64_t)(v2));
  t23 = fg165_byte(t21, t22);
  t24 = ((uint64_t)(t23));
  t25 = fg165_lower(t24);
  t26 = ((uint64_t)(t25));
  t27 = ((int64_t)(v1));
  t28 = ((uint64_t)(v2));
  t29 = fg165_byte(t27, t28);
  t30 = ((uint64_t)(t29));
  t31 = fg165_lower(t30);
  t32 = ((uint64_t)(t31));
  t33 = ((t26) != (t32));
  if (t33) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t34 = v2;
  t35 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v2 = ((uint64_t)((uint64_t)(t34) + (uint64_t)(t35)));
  }
  goto label1;
label2:;
  t36 = ((int64_t)(v0));
  t37 = ((uint64_t)(v2));
  t38 = fg165_byte(t36, t37);
  t39 = ((uint64_t)(t38));
  t40 = ((int64_t)(v1));
  t41 = ((uint64_t)(v2));
  t42 = fg165_byte(t40, t41);
  t43 = ((uint64_t)(t42));
  t0 = ((t39) == (t43));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_27(int64_t p0, uint64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  int64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(1))));
  t4 = fg165_get(t2, t3);
  t5 = ((uint64_t)(t4));
  t6 = ((t1) < (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_function_19(t6, t7, base + 0);
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = fg165_get(t8, t9);
  t11 = ((uint64_t)(t10));
  t12 = gg_function_3(t11, base + 0);
  t13 = ((uint64_t)(v1));
  t14 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t15 = ((uint64_t)(((uint64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = fg165_ptr(t12, t15);
  t17 = ((uint64_t)(t16));
  t0 = gg_function_3(t17, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_28(int64_t p0, int64_t p1, uint8_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint8_t t12;
  int64_t t13;
  uint64_t t14;
  int64_t t15;
  uint64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  uint8_t t28;
  uint64_t t29;
  uint64_t t30;
  int64_t v0;
  int64_t v1;
  uint8_t v2;
  uint64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_19(t3, t4, base + 0);
  v3 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t5 = ((uint64_t)(v3));
  t6 = ((int64_t)(v0));
  t8 = v2;
  if (t8) {
  {
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  } else {
  {
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t9 = ((int64_t)(t7));
  t10 = fg165_get(t6, t9);
  t11 = ((uint64_t)(t10));
  if (!(((t5) < (t11)))) goto label2;
  {
  t12 = v2;
  if (t12) {
  {
  t13 = ((int64_t)(v0));
  t14 = ((uint64_t)(v3));
  v4 = gg_function_29(t13, t14, base + 0);
  }
  } else {
  {
  t15 = ((int64_t)(v0));
  t16 = ((uint64_t)(v3));
  v4 = gg_function_27(t15, t16, base + 0);
  }
  }
  t18 = v2;
  if (t18) {
  {
  t17 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  } else {
  {
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t19 = ((int64_t)(t17));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = fg165_effect(t19, t20, t21, t22);
  t24 = ((uint64_t)(t23));
  t25 = gg_function_3(t24, base + 0);
  t26 = ((int64_t)(v1));
  t27 = gg_function_30(t25, t26, base + 0);
  t28 = t27;
  if (t28) {
  {
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
  }
  t29 = v3;
  t30 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v3 = ((uint64_t)((uint64_t)(t29) + (uint64_t)(t30)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_10(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
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
  uint64_t t29;
  int64_t t30;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(6))));
  t2 = fg165_get(t0, t1);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) != (t4));
  if (t5) {
  {
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(6))));
  t8 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg165_put(t6, t7, t8);
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(19))));
  if (!(((t9) < (t10)))) goto label2;
  {
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(3))));
  t13 = ((t11) != (t12));
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = fg165_env(t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t17) == (t18));
  }
  t19 = t14;
  if (t19) {
  {
  t20 = ((int64_t)(v1));
  t21 = fg165_builtin(t20);
  v2 = ((int64_t)(t21));
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_13(t22, t23, t24, base + 0);
  }
  }
  t25 = v1;
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  v1 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  }
  goto label1;
label2:;
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(6))));
  t29 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  fg165_put(t27, t28, t29);
  t30 = ((int64_t)(((int64_t)UINT64_C(40))));
  fg165_mark(t30);
  }
gg_done:;
}
static void gg_function_4(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg165_get(t0, t1);
  t3 = ((uint64_t)(t2));
  t4 = gg_function_3(t3, base + 0);
  fg165_free(t4);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(3))));
  t7 = fg165_get(t5, t6);
  t8 = ((uint64_t)(t7));
  t9 = gg_function_3(t8, base + 0);
  fg165_free(t9);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg165_put(t10, t11, t12);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg165_put(t13, t14, t15);
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t18 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg165_put(t16, t17, t18);
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(3))));
  t21 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg165_put(t19, t20, t21);
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(4))));
  t24 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg165_put(t22, t23, t24);
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(5))));
  t27 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg165_put(t25, t26, t27);
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(6))));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg165_put(t28, t29, t30);
  t31 = ((int64_t)(((int64_t)UINT64_C(44))));
  fg165_mark(t31);
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
  uint8_t t15;
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
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  uint64_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  uint64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  uint64_t t57;
  int64_t t58;
  int64_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
  uint64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  int64_t t70;
  int64_t t71;
  uint8_t t72;
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
  uint8_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  uint64_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  uint64_t t111;
  uint64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint8_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  uint64_t t122;
  uint64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  uint8_t t127;
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
  uint8_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  uint64_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
  int64_t t147;
  int64_t t148;
  uint8_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  uint8_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  uint8_t t158;
  uint8_t t159;
  int64_t t160;
  int64_t t161;
  uint8_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  uint8_t t167;
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
  uint64_t t179;
  uint64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
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
  t3 = fg165_arg(t1, t2);
  t4 = ((uint64_t)(t3));
  v1 = gg_function_3(t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = fg165_arg(t5, t6);
  t8 = ((uint64_t)(t7));
  v2 = gg_function_3(t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = fg165_arg(t9, t10);
  t12 = ((uint64_t)(t11));
  v3 = gg_function_3(t12, base + 0);
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(30))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v2));
  gg_function_4(t16, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t17 = fg165_state();
  v4 = ((int64_t)(t17));
  t18 = ((int64_t)(v4));
  gg_function_10(t18, base + 0);
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_13(t22, t23, t24, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(v2));
  gg_function_26(t28, t29, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(((int64_t)UINT64_C(2))));
  t32 = ((t30) == (t31));
  if (t32) {
  {
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)UINT64_C(1))));
  t35 = fg165_get(t33, t34);
  t0 = ((uint64_t)(t35));
  goto gg_done;
  }
  }
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  t42 = fg165_arg(t40, t41);
  t43 = ((uint64_t)(t42));
  t44 = gg_function_27(t39, t43, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t44))));
  goto gg_done;
  }
  }
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(((int64_t)UINT64_C(4))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  t48 = ((int64_t)(v4));
  t49 = ((int64_t)(v2));
  t50 = ((uint8_t)UINT64_C(0));
  t51 = gg_function_28(t48, t49, t50, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t51))));
  goto gg_done;
  }
  }
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(((int64_t)UINT64_C(5))));
  t54 = ((t52) == (t53));
  if (t54) {
  {
  t55 = ((int64_t)(v4));
  t56 = ((int64_t)(((int64_t)UINT64_C(4))));
  t57 = fg165_get(t55, t56);
  t0 = ((uint64_t)(t57));
  goto gg_done;
  }
  }
  t58 = ((int64_t)(v1));
  t59 = ((int64_t)(((int64_t)UINT64_C(6))));
  t60 = ((t58) == (t59));
  if (t60) {
  {
  t61 = ((int64_t)(v4));
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  t64 = fg165_arg(t62, t63);
  t65 = ((uint64_t)(t64));
  t66 = gg_function_29(t61, t65, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t66))));
  goto gg_done;
  }
  }
  t67 = ((int64_t)(v1));
  t68 = ((int64_t)(((int64_t)UINT64_C(7))));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  t70 = ((int64_t)(v4));
  t71 = ((int64_t)(v2));
  t72 = ((uint8_t)UINT64_C(1));
  t73 = gg_function_28(t70, t71, t72, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t73))));
  goto gg_done;
  }
  }
  t74 = ((int64_t)(v1));
  t75 = ((int64_t)(((int64_t)UINT64_C(8))));
  t76 = ((t74) == (t75));
  if (t76) {
  {
  t77 = ((int64_t)(v4));
  t78 = ((int64_t)(v2));
  t79 = gg_function_33(t77, t78, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t79))));
  goto gg_done;
  }
  }
  t80 = ((int64_t)(v1));
  t81 = ((int64_t)(((int64_t)UINT64_C(9))));
  t82 = ((t80) == (t81));
  t83 = t82;
  if (!t83) {
  t84 = ((int64_t)(v1));
  t85 = ((int64_t)(((int64_t)UINT64_C(10))));
  t83 = ((t84) == (t85));
  }
  t86 = t83;
  if (t86) {
  {
  t87 = ((int64_t)(v1));
  t88 = ((int64_t)(((int64_t)UINT64_C(9))));
  t89 = ((t87) == (t88));
  if (t89) {
  {
  t90 = ((int64_t)(v4));
  t91 = ((int64_t)(v2));
  t92 = ((uint8_t)UINT64_C(1));
  v5 = gg_function_28(t90, t91, t92, base + 0);
  }
  } else {
  {
  t93 = ((int64_t)(v4));
  t94 = ((int64_t)(v2));
  v5 = gg_function_33(t93, t94, base + 0);
  }
  }
  t95 = ((int64_t)(v5));
  t96 = ((int64_t)(((int64_t)UINT64_C(0))));
  t97 = ((t95) == (t96));
  if (t97) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t98 = ((int64_t)(((int64_t)UINT64_C(8))));
  t99 = ((int64_t)(v5));
  t100 = ((int64_t)(v3));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = fg165_effect(t98, t99, t100, t101);
  t0 = ((uint64_t)(t102));
  }
  }
  goto gg_done;
  }
  }
  t103 = ((int64_t)(v1));
  t104 = ((int64_t)(((int64_t)UINT64_C(11))));
  t105 = ((t103) == (t104));
  if (t105) {
  {
  t106 = ((int64_t)(v4));
  t107 = ((int64_t)(((int64_t)UINT64_C(30))));
  t108 = ((int64_t)(((int64_t)UINT64_C(0))));
  t109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = fg165_effect(t107, t108, t109, t110);
  t112 = ((uint64_t)(t111));
  t113 = gg_function_3(t112, base + 0);
  v6 = gg_function_33(t106, t113, base + 0);
  t114 = ((int64_t)(v6));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((t114) == (t115));
  if (t116) {
  {
  t117 = ((int64_t)(v4));
  t118 = ((int64_t)(((int64_t)UINT64_C(31))));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = fg165_effect(t118, t119, t120, t121);
  t123 = ((uint64_t)(t122));
  t124 = gg_function_3(t123, base + 0);
  v6 = gg_function_33(t117, t124, base + 0);
  }
  }
  t125 = ((int64_t)(v6));
  t126 = ((int64_t)(((int64_t)UINT64_C(0))));
  t127 = ((t125) == (t126));
  if (t127) {
  {
  t128 = ((int64_t)(v4));
  t129 = ((int64_t)(((int64_t)UINT64_C(32))));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = ((int64_t)(((int64_t)UINT64_C(0))));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t133 = fg165_effect(t129, t130, t131, t132);
  t134 = ((uint64_t)(t133));
  t135 = gg_function_3(t134, base + 0);
  v6 = gg_function_33(t128, t135, base + 0);
  }
  }
  t136 = ((int64_t)(v6));
  t137 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = ((t136) == (t137));
  if (t138) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t139 = ((int64_t)(((int64_t)UINT64_C(8))));
  t140 = ((int64_t)(v6));
  t141 = ((int64_t)(((int64_t)UINT64_C(0))));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = fg165_effect(t139, t140, t141, t142);
  t0 = ((uint64_t)(t143));
  }
  }
  goto gg_done;
  }
  }
  t144 = ((int64_t)(v1));
  t145 = ((int64_t)(((int64_t)UINT64_C(12))));
  t146 = ((t144) == (t145));
  if (t146) {
  {
  t147 = ((int64_t)(v4));
  t148 = ((int64_t)(v2));
  t149 = ((uint8_t)UINT64_C(0));
  t150 = gg_function_34(t147, t148, t149, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t150))));
  goto gg_done;
  }
  }
  t151 = ((int64_t)(v1));
  t152 = ((int64_t)(((int64_t)UINT64_C(13))));
  t153 = ((t151) == (t152));
  if (t153) {
  {
  t154 = ((int64_t)(v4));
  t155 = ((int64_t)(v2));
  gg_function_37(t154, t155, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t156 = ((int64_t)(v1));
  t157 = ((int64_t)(((int64_t)UINT64_C(14))));
  t158 = ((t156) == (t157));
  t159 = t158;
  if (!t159) {
  t160 = ((int64_t)(v1));
  t161 = ((int64_t)(((int64_t)UINT64_C(15))));
  t159 = ((t160) == (t161));
  }
  t162 = t159;
  if (t162) {
  {
  t163 = ((int64_t)(v4));
  t165 = ((int64_t)(v1));
  t166 = ((int64_t)(((int64_t)UINT64_C(14))));
  t167 = ((t165) == (t166));
  if (t167) {
  {
  t164 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t164 = ((int64_t)(v2));
  }
  }
  t168 = ((int64_t)(t164));
  gg_function_38(t163, t168, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t169 = ((int64_t)(v1));
  t170 = ((int64_t)(((int64_t)UINT64_C(20))));
  t171 = ((t169) == (t170));
  if (t171) {
  {
  t172 = ((int64_t)(v4));
  t173 = ((int64_t)(v2));
  t174 = ((int64_t)(v3));
  t175 = ((int64_t)(((int64_t)UINT64_C(0))));
  t176 = ((t174) != (t175));
  t177 = ((int64_t)(v0));
  t178 = ((int64_t)(((int64_t)UINT64_C(3))));
  t179 = fg165_arg(t177, t178);
  t180 = ((uint64_t)(t179));
  t181 = gg_function_3(t180, base + 0);
  t182 = gg_function_39(t172, t173, t176, t181, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t182))));
  goto gg_done;
  }
  }
  t183 = ((int64_t)(((int64_t)UINT64_C(99))));
  fg165_fail(t183);
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
