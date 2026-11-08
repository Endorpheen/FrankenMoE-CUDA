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
static uint64_t gg_function_4(uint64_t p0, int64_t base);
static uint64_t gg_function_5(uint64_t p0, int64_t base);
static void gg_function_7(int64_t p0, uint64_t p1, int64_t base);
static uint64_t gg_function_8(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t gg_function_9(int64_t base);
static void gg_function_10(uint8_t p0, int64_t p1, int64_t base);
static uint64_t gg_function_11(uint64_t p0, int64_t base);
static void gg_function_15(int64_t p0, int64_t base);
static uint8_t gg_function_18(int64_t p0, uint64_t p1, int64_t base);
static void gg_function_20(int64_t p0, uint64_t p1, uint8_t p2, int64_t base);
static uint64_t gg_function_22(int64_t p0, uint64_t p1, int64_t base);
static uint64_t gg_function_24(int64_t p0, uint64_t p1, uint8_t p2, int64_t base);
static uint64_t gg_function_26(uint64_t p0, uint8_t p1, int64_t base);
static uint64_t gg_function_28(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t gg_function_29(uint64_t p0, uint64_t p1, int64_t base);
static int64_t gg_function_30(int64_t p0, uint64_t p1, uint8_t p2, int64_t base);
static void gg_function_34(int64_t p0, int64_t p1, uint8_t p2, uint8_t p3, int64_t base);
static void gg_function_35(int64_t p0, int64_t base);
static uint64_t gg_function_36(int64_t p0, int64_t p1, uint8_t p2, int64_t base);
static void gg_function_37(int64_t p0, int64_t p1, uint8_t p2, int64_t p3, uint64_t p4, uint64_t p5, int64_t base);
static void gg_function_38(uint8_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_42(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void gg_function_43(int64_t p0, int64_t p1, int64_t base);
static uint64_t gg_function_45(int64_t p0, int64_t p1, uint8_t p2, int64_t base);
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
static uint8_t gg_function_18(int64_t p0, uint64_t p1, int64_t base) {
  uint8_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  int64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((uint64_t)(v1));
  t3 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t4 = ((uint64_t)(((uint64_t)gg_udiv(t2, t3))));
  t5 = fg159_u32(t1, t4);
  t6 = ((uint64_t)(t5));
  t7 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t8 = ((uint64_t)(v1));
  t9 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t10 = ((uint64_t)(((uint64_t)gg_umod(t8, t9))));
  t11 = ((uint64_t)(((uint64_t)((uint64_t)(t7) << ((uint64_t)(t10) & 63)))));
  t12 = ((uint64_t)(((t6) & (t11))));
  t13 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t0 = ((t12) != (t13));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_9(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_10(uint8_t p0, int64_t p1, int64_t base) {
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
  fg159_fail(t2);
  }
  }
  }
gg_done:;
}
static uint64_t gg_function_22(int64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  int64_t t17;
  uint64_t t18;
  uint64_t t19;
  int64_t t20;
  uint64_t t21;
  uint8_t t22;
  uint8_t t23;
  uint8_t t24;
  int64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint8_t t36;
  int64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  uint64_t v5;
  uint64_t v6;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg159_hash(t1, t2);
  v2 = ((uint64_t)(t3));
  t4 = ((uint64_t)(v2));
  t5 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) > (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_function_10(t6, t7, base + 0);
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = fg159_hash(t8, t9);
  t11 = ((uint64_t)(t10));
  v3 = gg_function_3(t11, base + 0);
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = fg159_hash(t12, t13);
  t15 = ((uint64_t)(t14));
  v4 = gg_function_3(t15, base + 0);
  t16 = ((uint64_t)(v1));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  t18 = ((uint64_t)(((t16) >> ((uint64_t)(t17) & 63))));
  t19 = ((uint64_t)(v2));
  v5 = ((uint64_t)(((uint64_t)gg_umod(t18, t19))));
  v6 = ((uint64_t)(v5));
label1:;
  t20 = ((int64_t)(v3));
  t21 = ((uint64_t)(v6));
  t22 = gg_function_18(t20, t21, base + 0);
  t23 = t22;
  t24 = t23;
  if (t24) {
  t25 = ((int64_t)(v4));
  t26 = ((uint64_t)(v6));
  t27 = fg159_ptr(t25, t26);
  t28 = ((uint64_t)(t27));
  t29 = ((uint64_t)(v1));
  t24 = ((t28) != (t29));
  }
  if (!(t24)) goto label2;
  {
  t30 = ((uint64_t)(v6));
  t31 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t32 = ((uint64_t)(((uint64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  t33 = ((uint64_t)(v2));
  v6 = ((uint64_t)(((uint64_t)gg_umod(t32, t33))));
  t34 = ((uint64_t)(v6));
  t35 = ((uint64_t)(v5));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  t0 = gg_function_9(base + 0);
  goto gg_done;
  }
  }
  }
  goto label1;
label2:;
  t0 = ((uint64_t)(v6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_45(int64_t p0, int64_t p1, uint8_t p2, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  int64_t t16;
  uint64_t t17;
  uint8_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  int64_t v0;
  int64_t v1;
  uint8_t v2;
  int64_t v3;
  uint64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(10))));
  t3 = fg159_graph(t1, t2);
  t4 = ((uint64_t)(t3));
  v3 = gg_function_3(t4, base + 0);
  t5 = ((int64_t)(v3));
  t6 = ((uint64_t)(((uint64_t)(v1))));
  v4 = gg_function_22(t5, t6, base + 0);
  t7 = ((uint64_t)(v4));
  t8 = gg_function_9(base + 0);
  t9 = ((uint64_t)(t8));
  t10 = ((t7) == (t9));
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = fg159_hash(t12, t13);
  t15 = ((uint64_t)(t14));
  t16 = gg_function_3(t15, base + 0);
  t17 = ((uint64_t)(v4));
  t18 = gg_function_18(t16, t17, base + 0);
  t19 = t18;
  t11 = (!(t19));
  }
  t20 = t11;
  if (t20) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t21 = ((int64_t)(v0));
  t23 = v2;
  if (t23) {
  {
  t22 = ((int64_t)(((int64_t)UINT64_C(7))));
  }
  } else {
  {
  t22 = ((int64_t)(((int64_t)UINT64_C(6))));
  }
  }
  t24 = ((int64_t)(t22));
  t25 = fg159_graph(t21, t24);
  t26 = ((uint64_t)(t25));
  v5 = gg_function_3(t26, base + 0);
  t27 = ((int64_t)(v5));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t29 = ((t27) != (t28));
  if (t29) {
  {
  t30 = ((int64_t)(v5));
  t31 = ((uint64_t)(v4));
  t32 = fg159_ptr(t30, t31);
  t0 = ((uint64_t)(t32));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_8(uint64_t p0, uint64_t p1, int64_t base) {
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
  t6 = gg_function_9(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_function_10(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_11(uint64_t p0, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint8_t t9;
  uint64_t t10;
  uint64_t v0;
  v0 = p0;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t3 = ((uint64_t)(((uint64_t)gg_udiv(t1, t2))));
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t7 = ((uint64_t)(((uint64_t)gg_umod(t5, t6))));
  t8 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) != (t8));
  if (t9) {
  {
  t4 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t4 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t10 = ((uint64_t)(t4));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t3) + (uint64_t)(t10)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_15(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = fg159_hash(t0, t1);
  t3 = ((uint64_t)(t2));
  t4 = gg_function_3(t3, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = fg159_hash(t5, t6);
  t8 = ((uint64_t)(t7));
  t9 = gg_function_11(t8, base + 0);
  t10 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t11 = gg_function_8(t9, t10, base + 0);
  fg159_zero(t4, t11);
  }
gg_done:;
}
static void gg_function_35(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t v0;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg159_graph_put(t0, t1, t2);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  t5 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg159_graph_put(t3, t4, t5);
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  t8 = fg159_graph(t6, t7);
  t9 = ((uint64_t)(t8));
  t10 = gg_function_3(t9, base + 0);
  gg_function_15(t10, base + 0);
  }
gg_done:;
}
static void gg_function_20(int64_t p0, uint64_t p1, uint8_t p2, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  int64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  int64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint8_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  int64_t v0;
  uint64_t v1;
  uint8_t v2;
  uint64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t3 = ((uint64_t)(((uint64_t)gg_umod(t1, t2))));
  v3 = ((uint64_t)(((uint64_t)((uint64_t)(t0) << ((uint64_t)(t3) & 63)))));
  t4 = ((int64_t)(v0));
  t5 = ((uint64_t)(v1));
  t6 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t7 = ((uint64_t)(((uint64_t)gg_udiv(t5, t6))));
  t8 = fg159_u32(t4, t7);
  v4 = ((uint64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t12 = ((uint64_t)(((uint64_t)gg_udiv(t10, t11))));
  t14 = v2;
  if (t14) {
  {
  t15 = ((uint64_t)(v4));
  t16 = ((uint64_t)(v3));
  t13 = ((uint64_t)(((t15) | (t16))));
  }
  } else {
  {
  t17 = ((uint64_t)(v4));
  t18 = v3;
  t19 = ((uint64_t)((~(t18))));
  t13 = ((uint64_t)(((t17) & (t19))));
  }
  }
  t20 = ((uint64_t)(t13));
  fg159_u32_put(t9, t12, t20);
  }
gg_done:;
}
static uint64_t gg_function_24(int64_t p0, uint64_t p1, uint8_t p2, int64_t base) {
  uint64_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
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
  uint8_t t14;
  uint8_t t15;
  uint8_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  int64_t t20;
  uint64_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  int64_t v0;
  uint64_t v1;
  uint8_t v2;
  uint64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((uint64_t)(v1));
  v3 = gg_function_22(t1, t2, base + 0);
  t3 = ((uint64_t)(v3));
  t4 = gg_function_9(base + 0);
  t5 = ((uint64_t)(t4));
  t6 = ((t3) != (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_function_10(t6, t7, base + 0);
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = fg159_hash(t8, t9);
  t11 = ((uint64_t)(t10));
  v4 = gg_function_3(t11, base + 0);
  t12 = ((int64_t)(v4));
  t13 = ((uint64_t)(v3));
  t14 = gg_function_18(t12, t13, base + 0);
  t15 = t14;
  if (t15) {
  {
  t16 = v2;
  if (t16) {
  {
  t0 = ((uint64_t)(v3));
  }
  } else {
  {
  t17 = gg_function_9(base + 0);
  t18 = ((uint64_t)(t17));
  t19 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t18) - (uint64_t)(t19)))));
  }
  }
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v4));
  t21 = ((uint64_t)(v3));
  t22 = ((uint8_t)UINT64_C(1));
  gg_function_20(t20, t21, t22, base + 0);
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(2))));
  t25 = fg159_hash(t23, t24);
  t26 = ((uint64_t)(t25));
  t27 = gg_function_3(t26, base + 0);
  t28 = ((uint64_t)(v3));
  t29 = ((uint64_t)(v1));
  fg159_ptr_put(t27, t28, t29);
  t0 = ((uint64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_43(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint8_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  uint64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  uint64_t t47;
  uint64_t t48;
  uint8_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint8_t t56;
  int64_t t57;
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
  uint64_t t68;
  uint64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  uint64_t t74;
  uint64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  uint64_t t80;
  int64_t t81;
  int64_t t82;
  uint64_t t83;
  uint64_t t84;
  int64_t t85;
  uint64_t t86;
  int64_t t87;
  int64_t t88;
  uint64_t t89;
  uint64_t t90;
  int64_t t91;
  uint64_t t92;
  uint64_t t93;
  uint64_t t94;
  uint64_t t95;
  uint64_t t96;
  uint64_t t97;
  int64_t t98;
  int64_t t99;
  uint64_t t100;
  uint64_t t101;
  int64_t t102;
  int64_t t103;
  uint64_t t104;
  uint64_t t105;
  int64_t t106;
  uint64_t t107;
  int64_t t108;
  int64_t t109;
  uint64_t t110;
  uint64_t t111;
  int64_t t112;
  uint64_t t113;
  uint64_t t114;
  uint64_t t115;
  uint64_t t116;
  uint64_t t117;
  uint64_t t118;
  int64_t t119;
  int64_t t120;
  uint64_t t121;
  uint64_t t122;
  int64_t t123;
  int64_t t124;
  uint64_t t125;
  uint64_t t126;
  int64_t t127;
  uint64_t t128;
  uint8_t t129;
  uint8_t t130;
  int64_t t131;
  int64_t t132;
  uint64_t t133;
  uint64_t t134;
  int64_t t135;
  uint64_t t136;
  uint64_t t137;
  int64_t t138;
  uint64_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  uint64_t t143;
  uint64_t t144;
  int64_t t145;
  uint64_t t146;
  int64_t t147;
  int64_t t148;
  uint64_t t149;
  uint64_t t150;
  int64_t t151;
  uint64_t t152;
  uint64_t t153;
  uint64_t t154;
  uint64_t t155;
  uint64_t t156;
  int64_t t157;
  int64_t t158;
  uint64_t t159;
  uint64_t t160;
  uint64_t t161;
  uint8_t t162;
  int64_t t163;
  int64_t t164;
  uint64_t t165;
  uint64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  uint64_t t170;
  uint64_t t171;
  uint64_t t172;
  uint64_t t173;
  int64_t t174;
  int64_t t175;
  uint64_t t176;
  uint64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  uint64_t t181;
  uint64_t t182;
  uint64_t t183;
  uint64_t t184;
  int64_t t185;
  int64_t t186;
  uint64_t t187;
  uint64_t t188;
  uint64_t t189;
  uint8_t t190;
  uint64_t t191;
  int64_t t192;
  int64_t t193;
  uint64_t t194;
  uint64_t t195;
  int64_t t196;
  int64_t t197;
  uint64_t t198;
  uint64_t t199;
  int64_t t200;
  uint64_t t201;
  uint64_t t202;
  int64_t t203;
  uint64_t t204;
  int64_t t205;
  uint64_t t206;
  uint64_t t207;
  uint64_t t208;
  uint64_t t209;
  uint8_t t210;
  uint8_t t211;
  uint64_t t212;
  uint64_t t213;
  uint64_t t214;
  uint8_t t215;
  uint8_t t216;
  int64_t t217;
  int64_t t218;
  uint64_t t219;
  uint64_t t220;
  int64_t t221;
  uint64_t t222;
  uint8_t t223;
  uint8_t t224;
  uint8_t t225;
  int64_t t226;
  int64_t t227;
  uint64_t t228;
  uint64_t t229;
  int64_t t230;
  uint64_t t231;
  uint8_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  uint64_t t237;
  uint64_t t238;
  int64_t t239;
  uint64_t t240;
  int64_t t241;
  int64_t t242;
  uint64_t t243;
  uint64_t t244;
  int64_t t245;
  uint64_t t246;
  uint64_t t247;
  uint64_t t248;
  int64_t t249;
  int64_t t250;
  uint64_t t251;
  uint64_t t252;
  int64_t t253;
  uint64_t t254;
  int64_t t255;
  int64_t t256;
  uint64_t t257;
  uint64_t t258;
  int64_t t259;
  uint64_t t260;
  uint64_t t261;
  uint64_t t262;
  uint64_t t263;
  uint64_t t264;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
  uint64_t v8;
  uint64_t v9;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg159_graph(t0, t1);
  t3 = ((uint64_t)(t2));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = fg159_graph(t4, t5);
  t7 = ((uint64_t)(t6));
  t8 = ((t3) >= (t7));
  t9 = t8;
  if (t9) {
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = fg159_graph(t10, t11);
  t13 = ((uint64_t)(t12));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  t16 = fg159_graph(t14, t15);
  t17 = ((uint64_t)(t16));
  t9 = ((t13) >= (t17));
  }
  t18 = t9;
  t19 = ((int64_t)(((int64_t)UINT64_C(17))));
  gg_function_10(t18, t19, base + 0);
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(10))));
  t22 = fg159_graph(t20, t21);
  t23 = ((uint64_t)(t22));
  v2 = gg_function_3(t23, base + 0);
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(((int64_t)UINT64_C(10))));
  t26 = fg159_graph(t24, t25);
  t27 = ((uint64_t)(t26));
  v3 = gg_function_3(t27, base + 0);
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = fg159_hash(t28, t29);
  t31 = ((uint64_t)(t30));
  t32 = ((int64_t)(v2));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = fg159_hash(t32, t33);
  t35 = ((uint64_t)(t34));
  t36 = ((t31) >= (t35));
  t37 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_function_10(t36, t37, base + 0);
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(6))));
  t40 = fg159_graph(t38, t39);
  t41 = ((uint64_t)(t40));
  t42 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t41) != (t42));
  if (t43) {
  {
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(((int64_t)UINT64_C(6))));
  t46 = fg159_graph(t44, t45);
  t47 = ((uint64_t)(t46));
  t48 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t49 = ((t47) != (t48));
  t50 = t49;
  if (t50) {
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(((int64_t)UINT64_C(7))));
  t53 = fg159_graph(t51, t52);
  t54 = ((uint64_t)(t53));
  t55 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t50 = ((t54) != (t55));
  }
  t56 = t50;
  t57 = ((int64_t)(((int64_t)UINT64_C(19))));
  gg_function_10(t56, t57, base + 0);
  }
  }
  t58 = ((int64_t)(v1));
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  t62 = fg159_graph(t60, t61);
  t63 = ((uint64_t)(t62));
  fg159_graph_put(t58, t59, t63);
  t64 = ((int64_t)(v1));
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(2))));
  t68 = fg159_graph(t66, t67);
  t69 = ((uint64_t)(t68));
  fg159_graph_put(t64, t65, t69);
  t70 = ((int64_t)(v1));
  t71 = ((int64_t)(((int64_t)UINT64_C(8))));
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(((int64_t)UINT64_C(8))));
  t74 = fg159_graph(t72, t73);
  t75 = ((uint64_t)(t74));
  fg159_graph_put(t70, t71, t75);
  v4 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t76 = ((uint64_t)(v4));
  t77 = ((int64_t)(v0));
  t78 = ((int64_t)(((int64_t)UINT64_C(2))));
  t79 = fg159_graph(t77, t78);
  t80 = ((uint64_t)(t79));
  if (!(((t76) < (t80)))) goto label2;
  {
  t81 = ((int64_t)(v1));
  t82 = ((int64_t)(((int64_t)UINT64_C(4))));
  t83 = fg159_graph(t81, t82);
  t84 = ((uint64_t)(t83));
  t85 = gg_function_3(t84, base + 0);
  t86 = ((uint64_t)(v4));
  t87 = ((int64_t)(v0));
  t88 = ((int64_t)(((int64_t)UINT64_C(4))));
  t89 = fg159_graph(t87, t88);
  t90 = ((uint64_t)(t89));
  t91 = gg_function_3(t90, base + 0);
  t92 = ((uint64_t)(v4));
  t93 = fg159_ptr(t91, t92);
  t94 = ((uint64_t)(t93));
  fg159_ptr_put(t85, t86, t94);
  t95 = v4;
  t96 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v4 = ((uint64_t)((uint64_t)(t95) + (uint64_t)(t96)));
  }
  goto label1;
label2:;
  v4 = ((uint64_t)(((int64_t)UINT64_C(0))));
label4:;
  t97 = ((uint64_t)(v4));
  t98 = ((int64_t)(v0));
  t99 = ((int64_t)(((int64_t)UINT64_C(1))));
  t100 = fg159_graph(t98, t99);
  t101 = ((uint64_t)(t100));
  if (!(((t97) < (t101)))) goto label5;
  {
  t102 = ((int64_t)(v1));
  t103 = ((int64_t)(((int64_t)UINT64_C(3))));
  t104 = fg159_graph(t102, t103);
  t105 = ((uint64_t)(t104));
  t106 = gg_function_3(t105, base + 0);
  t107 = ((uint64_t)(v4));
  t108 = ((int64_t)(v0));
  t109 = ((int64_t)(((int64_t)UINT64_C(3))));
  t110 = fg159_graph(t108, t109);
  t111 = ((uint64_t)(t110));
  t112 = gg_function_3(t111, base + 0);
  t113 = ((uint64_t)(v4));
  t114 = fg159_ptr(t112, t113);
  t115 = ((uint64_t)(t114));
  fg159_ptr_put(t106, t107, t115);
  t116 = v4;
  t117 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v4 = ((uint64_t)((uint64_t)(t116) + (uint64_t)(t117)));
  }
  goto label4;
label5:;
  v4 = ((uint64_t)(((int64_t)UINT64_C(0))));
label7:;
  t118 = ((uint64_t)(v4));
  t119 = ((int64_t)(v2));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = fg159_hash(t119, t120);
  t122 = ((uint64_t)(t121));
  if (!(((t118) < (t122)))) goto label8;
  {
  t123 = ((int64_t)(v2));
  t124 = ((int64_t)(((int64_t)UINT64_C(1))));
  t125 = fg159_hash(t123, t124);
  t126 = ((uint64_t)(t125));
  t127 = gg_function_3(t126, base + 0);
  t128 = ((uint64_t)(v4));
  t129 = gg_function_18(t127, t128, base + 0);
  t130 = t129;
  if (t130) {
  {
  t131 = ((int64_t)(v2));
  t132 = ((int64_t)(((int64_t)UINT64_C(2))));
  t133 = fg159_hash(t131, t132);
  t134 = ((uint64_t)(t133));
  t135 = gg_function_3(t134, base + 0);
  t136 = ((uint64_t)(v4));
  t137 = fg159_ptr(t135, t136);
  v5 = ((uint64_t)(t137));
  t138 = ((int64_t)(v3));
  t139 = ((uint64_t)(v5));
  t140 = ((uint8_t)UINT64_C(1));
  v6 = gg_function_24(t138, t139, t140, base + 0);
  t141 = ((int64_t)(v1));
  t142 = ((int64_t)(((int64_t)UINT64_C(5))));
  t143 = fg159_graph(t141, t142);
  t144 = ((uint64_t)(t143));
  t145 = gg_function_3(t144, base + 0);
  t146 = ((uint64_t)(v6));
  t147 = ((int64_t)(v0));
  t148 = ((int64_t)(((int64_t)UINT64_C(5))));
  t149 = fg159_graph(t147, t148);
  t150 = ((uint64_t)(t149));
  t151 = gg_function_3(t150, base + 0);
  t152 = ((uint64_t)(v4));
  t153 = fg159_u32(t151, t152);
  t154 = ((uint64_t)(t153));
  fg159_u32_put(t145, t146, t154);
  }
  }
  t155 = v4;
  t156 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v4 = ((uint64_t)((uint64_t)(t155) + (uint64_t)(t156)));
  }
  goto label7;
label8:;
  t157 = ((int64_t)(v1));
  t158 = ((int64_t)(((int64_t)UINT64_C(6))));
  t159 = fg159_graph(t157, t158);
  t160 = ((uint64_t)(t159));
  t161 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t162 = ((t160) != (t161));
  if (t162) {
  {
  t163 = ((int64_t)(v1));
  t164 = ((int64_t)(((int64_t)UINT64_C(6))));
  t165 = fg159_graph(t163, t164);
  t166 = ((uint64_t)(t165));
  t167 = gg_function_3(t166, base + 0);
  t168 = ((int64_t)(v3));
  t169 = ((int64_t)(((int64_t)UINT64_C(0))));
  t170 = fg159_hash(t168, t169);
  t171 = ((uint64_t)(t170));
  t172 = ((uint64_t)(((int64_t)UINT64_C(8))));
  t173 = ((uint64_t)(((uint64_t)((uint64_t)(t171) * (uint64_t)(t172)))));
  fg159_zero(t167, t173);
  t174 = ((int64_t)(v1));
  t175 = ((int64_t)(((int64_t)UINT64_C(7))));
  t176 = fg159_graph(t174, t175);
  t177 = ((uint64_t)(t176));
  t178 = gg_function_3(t177, base + 0);
  t179 = ((int64_t)(v3));
  t180 = ((int64_t)(((int64_t)UINT64_C(0))));
  t181 = fg159_hash(t179, t180);
  t182 = ((uint64_t)(t181));
  t183 = ((uint64_t)(((int64_t)UINT64_C(8))));
  t184 = ((uint64_t)(((uint64_t)((uint64_t)(t182) * (uint64_t)(t183)))));
  fg159_zero(t178, t184);
  }
  }
  t185 = ((int64_t)(v0));
  t186 = ((int64_t)(((int64_t)UINT64_C(6))));
  t187 = fg159_graph(t185, t186);
  t188 = ((uint64_t)(t187));
  t189 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t190 = ((t188) != (t189));
  if (t190) {
  {
  v4 = ((uint64_t)(((int64_t)UINT64_C(0))));
label10:;
  t191 = ((uint64_t)(v4));
  t192 = ((int64_t)(v0));
  t193 = ((int64_t)(((int64_t)UINT64_C(1))));
  t194 = fg159_graph(t192, t193);
  t195 = ((uint64_t)(t194));
  if (!(((t191) < (t195)))) goto label11;
  {
  t196 = ((int64_t)(v0));
  t197 = ((int64_t)(((int64_t)UINT64_C(3))));
  t198 = fg159_graph(t196, t197);
  t199 = ((uint64_t)(t198));
  t200 = gg_function_3(t199, base + 0);
  t201 = ((uint64_t)(v4));
  t202 = fg159_ptr(t200, t201);
  v7 = ((uint64_t)(t202));
  t203 = ((int64_t)(v2));
  t204 = ((uint64_t)(v7));
  v8 = gg_function_22(t203, t204, base + 0);
  t205 = ((int64_t)(v3));
  t206 = ((uint64_t)(v7));
  v9 = gg_function_22(t205, t206, base + 0);
  t207 = ((uint64_t)(v8));
  t208 = gg_function_9(base + 0);
  t209 = ((uint64_t)(t208));
  t210 = ((t207) != (t209));
  t211 = t210;
  if (t211) {
  t212 = ((uint64_t)(v9));
  t213 = gg_function_9(base + 0);
  t214 = ((uint64_t)(t213));
  t211 = ((t212) != (t214));
  }
  t215 = t211;
  t216 = t215;
  if (t216) {
  t217 = ((int64_t)(v2));
  t218 = ((int64_t)(((int64_t)UINT64_C(1))));
  t219 = fg159_hash(t217, t218);
  t220 = ((uint64_t)(t219));
  t221 = gg_function_3(t220, base + 0);
  t222 = ((uint64_t)(v8));
  t223 = gg_function_18(t221, t222, base + 0);
  t216 = t223;
  }
  t224 = t216;
  t225 = t224;
  if (t225) {
  t226 = ((int64_t)(v3));
  t227 = ((int64_t)(((int64_t)UINT64_C(1))));
  t228 = fg159_hash(t226, t227);
  t229 = ((uint64_t)(t228));
  t230 = gg_function_3(t229, base + 0);
  t231 = ((uint64_t)(v9));
  t232 = gg_function_18(t230, t231, base + 0);
  t225 = t232;
  }
  t233 = t225;
  t234 = ((int64_t)(((int64_t)UINT64_C(20))));
  gg_function_10(t233, t234, base + 0);
  t235 = ((int64_t)(v1));
  t236 = ((int64_t)(((int64_t)UINT64_C(6))));
  t237 = fg159_graph(t235, t236);
  t238 = ((uint64_t)(t237));
  t239 = gg_function_3(t238, base + 0);
  t240 = ((uint64_t)(v9));
  t241 = ((int64_t)(v0));
  t242 = ((int64_t)(((int64_t)UINT64_C(6))));
  t243 = fg159_graph(t241, t242);
  t244 = ((uint64_t)(t243));
  t245 = gg_function_3(t244, base + 0);
  t246 = ((uint64_t)(v8));
  t247 = fg159_ptr(t245, t246);
  t248 = ((uint64_t)(t247));
  fg159_ptr_put(t239, t240, t248);
  t249 = ((int64_t)(v1));
  t250 = ((int64_t)(((int64_t)UINT64_C(7))));
  t251 = fg159_graph(t249, t250);
  t252 = ((uint64_t)(t251));
  t253 = gg_function_3(t252, base + 0);
  t254 = ((uint64_t)(v9));
  t255 = ((int64_t)(v0));
  t256 = ((int64_t)(((int64_t)UINT64_C(7))));
  t257 = fg159_graph(t255, t256);
  t258 = ((uint64_t)(t257));
  t259 = gg_function_3(t258, base + 0);
  t260 = ((uint64_t)(v8));
  t261 = fg159_ptr(t259, t260);
  t262 = ((uint64_t)(t261));
  fg159_ptr_put(t253, t254, t262);
  t263 = v4;
  t264 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v4 = ((uint64_t)((uint64_t)(t263) + (uint64_t)(t264)));
  }
  goto label10;
label11:;
  }
  }
  }
gg_done:;
}
static uint64_t gg_function_29(uint64_t p0, uint64_t p1, int64_t base) {
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
  t2 = gg_function_9(base + 0);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(v1));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((t1) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_10(t6, t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_28(uint64_t p0, uint64_t p1, int64_t base) {
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
  uint64_t t10;
  uint8_t t11;
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = t3;
  if (t4) {
  t5 = ((uint64_t)(v1));
  t6 = ((uint64_t)(v1));
  t7 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t8 = ((uint64_t)(((uint64_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  t9 = ((uint64_t)(((t5) & (t8))));
  t10 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t9) == (t10));
  }
  t11 = t4;
  t12 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_10(t11, t12, base + 0);
  t13 = ((uint64_t)(v0));
  t14 = ((uint64_t)(v1));
  t15 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t16 = ((uint64_t)(((uint64_t)((uint64_t)(t14) - (uint64_t)(t15)))));
  t17 = gg_function_29(t13, t16, base + 0);
  t18 = ((uint64_t)(t17));
  t19 = ((uint64_t)(v1));
  t20 = ((uint64_t)(((uint64_t)gg_udiv(t18, t19))));
  t21 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_5(uint64_t p0, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  uint64_t t4;
  uint64_t t5;
  uint8_t t6;
  uint64_t t7;
  uint64_t t8;
  uint8_t t9;
  uint64_t t10;
  uint64_t t11;
  uint8_t t12;
  uint64_t t13;
  uint64_t t14;
  uint8_t t15;
  uint64_t t16;
  uint64_t t17;
  uint8_t t18;
  uint64_t t19;
  uint64_t t20;
  uint8_t t21;
  uint64_t t22;
  uint64_t t23;
  uint8_t t24;
  uint64_t t25;
  uint64_t t26;
  uint8_t t27;
  uint64_t t28;
  uint64_t t29;
  uint8_t t30;
  uint64_t t31;
  uint64_t t32;
  uint8_t t33;
  uint64_t t34;
  uint64_t t35;
  uint8_t t36;
  uint64_t t37;
  uint64_t t38;
  uint8_t t39;
  uint64_t t40;
  uint64_t t41;
  uint8_t t42;
  uint64_t t43;
  uint64_t t44;
  uint8_t t45;
  uint64_t t46;
  uint64_t t47;
  uint8_t t48;
  uint64_t t49;
  uint64_t t50;
  uint8_t t51;
  uint64_t t52;
  uint64_t t53;
  uint8_t t54;
  uint64_t t55;
  uint64_t t56;
  uint8_t t57;
  uint64_t t58;
  uint64_t t59;
  uint8_t t60;
  uint64_t t61;
  uint64_t t62;
  uint8_t t63;
  uint64_t t64;
  uint64_t t65;
  uint8_t t66;
  uint64_t t67;
  uint64_t t68;
  uint8_t t69;
  uint64_t t70;
  uint64_t t71;
  uint8_t t72;
  uint64_t t73;
  uint64_t t74;
  uint8_t t75;
  uint64_t t76;
  uint64_t t77;
  uint8_t t78;
  uint64_t t79;
  uint64_t t80;
  uint8_t t81;
  uint64_t t82;
  uint64_t t83;
  uint8_t t84;
  uint64_t t85;
  uint64_t t86;
  uint8_t t87;
  uint64_t t88;
  uint64_t t89;
  uint8_t t90;
  uint64_t t91;
  uint64_t t92;
  uint8_t t93;
  uint64_t t94;
  uint64_t t95;
  uint8_t t96;
  int64_t t97;
  uint64_t v0;
  v0 = p0;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  goto gg_done;
  }
  }
  t4 = ((uint64_t)(v0));
  t5 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(3))))));
  goto gg_done;
  }
  }
  t7 = ((uint64_t)(v0));
  t8 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(5))))));
  goto gg_done;
  }
  }
  t10 = ((uint64_t)(v0));
  t11 = ((uint64_t)(((int64_t)UINT64_C(3))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(11))))));
  goto gg_done;
  }
  }
  t13 = ((uint64_t)(v0));
  t14 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(17))))));
  goto gg_done;
  }
  }
  t16 = ((uint64_t)(v0));
  t17 = ((uint64_t)(((int64_t)UINT64_C(5))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(37))))));
  goto gg_done;
  }
  }
  t19 = ((uint64_t)(v0));
  t20 = ((uint64_t)(((int64_t)UINT64_C(6))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(67))))));
  goto gg_done;
  }
  }
  t22 = ((uint64_t)(v0));
  t23 = ((uint64_t)(((int64_t)UINT64_C(7))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(131))))));
  goto gg_done;
  }
  }
  t25 = ((uint64_t)(v0));
  t26 = ((uint64_t)(((int64_t)UINT64_C(8))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(257))))));
  goto gg_done;
  }
  }
  t28 = ((uint64_t)(v0));
  t29 = ((uint64_t)(((int64_t)UINT64_C(9))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(521))))));
  goto gg_done;
  }
  }
  t31 = ((uint64_t)(v0));
  t32 = ((uint64_t)(((int64_t)UINT64_C(10))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1031))))));
  goto gg_done;
  }
  }
  t34 = ((uint64_t)(v0));
  t35 = ((uint64_t)(((int64_t)UINT64_C(11))));
  t36 = ((t34) == (t35));
  if (t36) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2053))))));
  goto gg_done;
  }
  }
  t37 = ((uint64_t)(v0));
  t38 = ((uint64_t)(((int64_t)UINT64_C(12))));
  t39 = ((t37) == (t38));
  if (t39) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4099))))));
  goto gg_done;
  }
  }
  t40 = ((uint64_t)(v0));
  t41 = ((uint64_t)(((int64_t)UINT64_C(13))));
  t42 = ((t40) == (t41));
  if (t42) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8209))))));
  goto gg_done;
  }
  }
  t43 = ((uint64_t)(v0));
  t44 = ((uint64_t)(((int64_t)UINT64_C(14))));
  t45 = ((t43) == (t44));
  if (t45) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(16411))))));
  goto gg_done;
  }
  }
  t46 = ((uint64_t)(v0));
  t47 = ((uint64_t)(((int64_t)UINT64_C(15))));
  t48 = ((t46) == (t47));
  if (t48) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32771))))));
  goto gg_done;
  }
  }
  t49 = ((uint64_t)(v0));
  t50 = ((uint64_t)(((int64_t)UINT64_C(16))));
  t51 = ((t49) == (t50));
  if (t51) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(65537))))));
  goto gg_done;
  }
  }
  t52 = ((uint64_t)(v0));
  t53 = ((uint64_t)(((int64_t)UINT64_C(17))));
  t54 = ((t52) == (t53));
  if (t54) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(131101))))));
  goto gg_done;
  }
  }
  t55 = ((uint64_t)(v0));
  t56 = ((uint64_t)(((int64_t)UINT64_C(18))));
  t57 = ((t55) == (t56));
  if (t57) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(262147))))));
  goto gg_done;
  }
  }
  t58 = ((uint64_t)(v0));
  t59 = ((uint64_t)(((int64_t)UINT64_C(19))));
  t60 = ((t58) == (t59));
  if (t60) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(524309))))));
  goto gg_done;
  }
  }
  t61 = ((uint64_t)(v0));
  t62 = ((uint64_t)(((int64_t)UINT64_C(20))));
  t63 = ((t61) == (t62));
  if (t63) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1048583))))));
  goto gg_done;
  }
  }
  t64 = ((uint64_t)(v0));
  t65 = ((uint64_t)(((int64_t)UINT64_C(21))));
  t66 = ((t64) == (t65));
  if (t66) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2097169))))));
  goto gg_done;
  }
  }
  t67 = ((uint64_t)(v0));
  t68 = ((uint64_t)(((int64_t)UINT64_C(22))));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4194319))))));
  goto gg_done;
  }
  }
  t70 = ((uint64_t)(v0));
  t71 = ((uint64_t)(((int64_t)UINT64_C(23))));
  t72 = ((t70) == (t71));
  if (t72) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8388617))))));
  goto gg_done;
  }
  }
  t73 = ((uint64_t)(v0));
  t74 = ((uint64_t)(((int64_t)UINT64_C(24))));
  t75 = ((t73) == (t74));
  if (t75) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(16777259))))));
  goto gg_done;
  }
  }
  t76 = ((uint64_t)(v0));
  t77 = ((uint64_t)(((int64_t)UINT64_C(25))));
  t78 = ((t76) == (t77));
  if (t78) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(33554467))))));
  goto gg_done;
  }
  }
  t79 = ((uint64_t)(v0));
  t80 = ((uint64_t)(((int64_t)UINT64_C(26))));
  t81 = ((t79) == (t80));
  if (t81) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(67108879))))));
  goto gg_done;
  }
  }
  t82 = ((uint64_t)(v0));
  t83 = ((uint64_t)(((int64_t)UINT64_C(27))));
  t84 = ((t82) == (t83));
  if (t84) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(134217757))))));
  goto gg_done;
  }
  }
  t85 = ((uint64_t)(v0));
  t86 = ((uint64_t)(((int64_t)UINT64_C(28))));
  t87 = ((t85) == (t86));
  if (t87) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(268435459))))));
  goto gg_done;
  }
  }
  t88 = ((uint64_t)(v0));
  t89 = ((uint64_t)(((int64_t)UINT64_C(29))));
  t90 = ((t88) == (t89));
  if (t90) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(536870923))))));
  goto gg_done;
  }
  }
  t91 = ((uint64_t)(v0));
  t92 = ((uint64_t)(((int64_t)UINT64_C(30))));
  t93 = ((t91) == (t92));
  if (t93) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1073741827))))));
  goto gg_done;
  }
  }
  t94 = ((uint64_t)(v0));
  t95 = ((uint64_t)(((int64_t)UINT64_C(31))));
  t96 = ((t94) == (t95));
  if (t96) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2147483659))))));
  goto gg_done;
  }
  }
  t97 = ((int64_t)(((int64_t)UINT64_C(30))));
  fg159_fail(t97);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_4(uint64_t p0, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint8_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint8_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  v0 = p0;
  {
  v1 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v2 = ((uint64_t)(((int64_t)UINT64_C(32))));
label1:;
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((uint64_t)(v1));
  t4 = ((uint64_t)(v2));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) + (uint64_t)(t4)))));
  t6 = ((uint64_t)(((int64_t)UINT64_C(2))));
  v3 = ((uint64_t)(((uint64_t)gg_udiv(t5, t6))));
  t7 = ((uint64_t)(v3));
  t8 = gg_function_5(t7, base + 0);
  t9 = ((uint64_t)(t8));
  t10 = ((uint64_t)(v0));
  t11 = ((t9) < (t10));
  if (t11) {
  {
  t12 = ((uint64_t)(v3));
  t13 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v1 = ((uint64_t)(((uint64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  }
  } else {
  {
  v2 = ((uint64_t)(v3));
  }
  }
  }
  goto label1;
label2:;
  t14 = ((uint64_t)(v1));
  t15 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t16 = ((t14) < (t15));
  if (t16) {
  {
  t17 = ((uint64_t)(v1));
  t0 = gg_function_5(t17, base + 0);
  }
  } else {
  {
  t18 = ((uint64_t)(v0));
  t19 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t0 = ((uint64_t)(((t18) | (t19))));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_26(uint64_t p0, uint8_t p1, int64_t base) {
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
  int64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
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
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
  uint64_t t46;
  uint64_t t47;
  uint64_t v0;
  uint8_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint64_t v5;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(((int64_t)UINT64_C(2147483647))));
  t3 = ((t1) <= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_function_10(t3, t4, base + 0);
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  t7 = gg_function_8(t5, t6, base + 0);
  v2 = gg_function_4(t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  v3 = gg_function_8(t8, t9, base + 0);
  t10 = ((uint64_t)(v2));
  t11 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  v4 = gg_function_8(t10, t11, base + 0);
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg159_constant(t12);
  v5 = ((uint64_t)(t13));
  t14 = ((uint64_t)(v5));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t16 = gg_function_28(t14, t15, base + 0);
  t17 = ((uint64_t)(v3));
  v5 = gg_function_29(t16, t17, base + 0);
  t18 = ((uint64_t)(v5));
  t19 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t20 = gg_function_28(t18, t19, base + 0);
  t21 = ((uint64_t)(v3));
  v5 = gg_function_29(t20, t21, base + 0);
  t22 = ((uint64_t)(v5));
  t23 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t24 = gg_function_28(t22, t23, base + 0);
  t25 = ((uint64_t)(v2));
  t26 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t27 = gg_function_8(t25, t26, base + 0);
  v5 = gg_function_29(t24, t27, base + 0);
  t28 = ((uint64_t)(v5));
  t29 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t30 = gg_function_28(t28, t29, base + 0);
  t31 = ((uint64_t)(v4));
  v5 = gg_function_29(t30, t31, base + 0);
  t32 = v1;
  if (t32) {
  {
  t33 = ((uint64_t)(v5));
  t34 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t35 = gg_function_28(t33, t34, base + 0);
  t36 = ((uint64_t)(v4));
  v5 = gg_function_29(t35, t36, base + 0);
  t37 = ((uint64_t)(v5));
  t38 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t39 = gg_function_28(t37, t38, base + 0);
  t40 = ((uint64_t)(v4));
  v5 = gg_function_29(t39, t40, base + 0);
  }
  }
  t41 = ((uint64_t)(v5));
  t42 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t43 = gg_function_28(t41, t42, base + 0);
  t44 = ((uint64_t)(v2));
  t45 = gg_function_11(t44, base + 0);
  t46 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t47 = gg_function_8(t45, t46, base + 0);
  t0 = gg_function_29(t43, t47, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_30(int64_t p0, uint64_t p1, uint8_t p2, int64_t base) {
  int64_t t0;
  uint64_t t1;
  uint8_t t2;
  int64_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  uint64_t t47;
  uint64_t t48;
  uint64_t t49;
  uint64_t t50;
  uint64_t t51;
  uint64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  uint64_t t60;
  int64_t t61;
  int64_t t62;
  uint64_t t63;
  int64_t t64;
  int64_t t65;
  uint64_t t66;
  uint64_t t67;
  uint64_t t68;
  uint64_t t69;
  uint64_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint64_t t74;
  uint8_t t75;
  int64_t t76;
  int64_t t77;
  uint64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint64_t t86;
  int64_t t87;
  int64_t t88;
  uint64_t t89;
  uint64_t t90;
  uint64_t t91;
  uint64_t t92;
  uint64_t t93;
  uint64_t t94;
  uint64_t t95;
  uint64_t t96;
  uint64_t t97;
  int64_t t98;
  int64_t t99;
  uint64_t t100;
  uint64_t t101;
  int64_t t102;
  uint64_t t103;
  int64_t t104;
  int64_t t105;
  uint64_t t106;
  uint64_t t107;
  int64_t t108;
  uint64_t t109;
  int64_t t110;
  int64_t t111;
  uint64_t t112;
  uint64_t t113;
  uint64_t t114;
  uint64_t t115;
  uint64_t t116;
  uint64_t t117;
  uint64_t t118;
  uint64_t t119;
  uint64_t t120;
  uint64_t t121;
  uint64_t t122;
  uint64_t t123;
  uint64_t t124;
  uint64_t t125;
  uint64_t t126;
  uint8_t t127;
  int64_t t128;
  int64_t t129;
  int64_t v0;
  uint64_t v1;
  uint8_t v2;
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
  t1 = ((uint64_t)(v1));
  t2 = v2;
  v3 = gg_function_26(t1, t2, base + 0);
  t3 = ((int64_t)(v0));
  t4 = ((uint64_t)(v3));
  t5 = fg159_arena(t3, t4);
  v4 = ((int64_t)(t5));
  t6 = ((int64_t)(v4));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg159_constant(t7);
  t9 = ((uint64_t)(t8));
  fg159_zero(t6, t9);
  v5 = ((uint64_t)(((uint64_t)(v4))));
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t12 = ((uint64_t)(((uint64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  v6 = gg_function_4(t12, base + 0);
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg159_constant(t13);
  v7 = ((uint64_t)(t14));
  t15 = ((uint64_t)(v1));
  t16 = ((uint64_t)(((int64_t)UINT64_C(8))));
  v8 = ((uint64_t)(((uint64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t17 = ((uint64_t)(v6));
  t18 = ((uint64_t)(((int64_t)UINT64_C(8))));
  v9 = ((uint64_t)(((uint64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((uint64_t)(v1));
  fg159_graph_put(t19, t20, t21);
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)UINT64_C(3))));
  t24 = ((uint64_t)(v5));
  t25 = ((uint64_t)(v7));
  t26 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t27 = gg_function_28(t25, t26, base + 0);
  t28 = gg_function_29(t24, t27, base + 0);
  fg159_graph_put(t22, t23, t28);
  t29 = ((uint64_t)(v7));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t31 = gg_function_28(t29, t30, base + 0);
  t32 = ((uint64_t)(v8));
  v7 = gg_function_29(t31, t32, base + 0);
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)UINT64_C(4))));
  t35 = ((uint64_t)(v5));
  t36 = ((uint64_t)(v7));
  t37 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t38 = gg_function_28(t36, t37, base + 0);
  t39 = gg_function_29(t35, t38, base + 0);
  fg159_graph_put(t33, t34, t39);
  t40 = ((uint64_t)(v7));
  t41 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t42 = gg_function_28(t40, t41, base + 0);
  t43 = ((uint64_t)(v8));
  v7 = gg_function_29(t42, t43, base + 0);
  t44 = ((int64_t)(v4));
  t45 = ((int64_t)(((int64_t)UINT64_C(5))));
  t46 = ((uint64_t)(v5));
  t47 = ((uint64_t)(v7));
  t48 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t49 = gg_function_28(t47, t48, base + 0);
  t50 = gg_function_29(t46, t49, base + 0);
  fg159_graph_put(t44, t45, t50);
  t51 = ((uint64_t)(v7));
  t52 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t53 = gg_function_28(t51, t52, base + 0);
  t54 = ((uint64_t)(v6));
  t55 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t56 = ((uint64_t)(((uint64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  v7 = gg_function_29(t53, t56, base + 0);
  t57 = ((int64_t)(v4));
  t58 = ((int64_t)(((int64_t)UINT64_C(10))));
  t59 = fg159_graph(t57, t58);
  t60 = ((uint64_t)(t59));
  v10 = gg_function_3(t60, base + 0);
  t61 = ((int64_t)(v10));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t63 = ((uint64_t)(v6));
  fg159_hash_put(t61, t62, t63);
  t64 = ((int64_t)(v10));
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  t66 = ((uint64_t)(v5));
  t67 = ((uint64_t)(v7));
  t68 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t69 = gg_function_28(t67, t68, base + 0);
  t70 = gg_function_29(t66, t69, base + 0);
  fg159_hash_put(t64, t65, t70);
  t71 = ((uint64_t)(v7));
  t72 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t73 = gg_function_28(t71, t72, base + 0);
  t74 = ((uint64_t)(v9));
  v7 = gg_function_29(t73, t74, base + 0);
  t75 = v2;
  if (t75) {
  {
  t76 = ((int64_t)(v4));
  t77 = ((int64_t)(((int64_t)UINT64_C(6))));
  t78 = ((uint64_t)(v5));
  t79 = ((uint64_t)(v7));
  t80 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t81 = gg_function_28(t79, t80, base + 0);
  t82 = gg_function_29(t78, t81, base + 0);
  fg159_graph_put(t76, t77, t82);
  t83 = ((uint64_t)(v7));
  t84 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t85 = gg_function_28(t83, t84, base + 0);
  t86 = ((uint64_t)(v9));
  v7 = gg_function_29(t85, t86, base + 0);
  t87 = ((int64_t)(v4));
  t88 = ((int64_t)(((int64_t)UINT64_C(7))));
  t89 = ((uint64_t)(v5));
  t90 = ((uint64_t)(v7));
  t91 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t92 = gg_function_28(t90, t91, base + 0);
  t93 = gg_function_29(t89, t92, base + 0);
  fg159_graph_put(t87, t88, t93);
  t94 = ((uint64_t)(v7));
  t95 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t96 = gg_function_28(t94, t95, base + 0);
  t97 = ((uint64_t)(v9));
  v7 = gg_function_29(t96, t97, base + 0);
  t98 = ((int64_t)(v4));
  t99 = ((int64_t)(((int64_t)UINT64_C(6))));
  t100 = fg159_graph(t98, t99);
  t101 = ((uint64_t)(t100));
  t102 = gg_function_3(t101, base + 0);
  t103 = ((uint64_t)(v9));
  fg159_zero(t102, t103);
  t104 = ((int64_t)(v4));
  t105 = ((int64_t)(((int64_t)UINT64_C(7))));
  t106 = fg159_graph(t104, t105);
  t107 = ((uint64_t)(t106));
  t108 = gg_function_3(t107, base + 0);
  t109 = ((uint64_t)(v9));
  fg159_zero(t108, t109);
  }
  }
  t110 = ((int64_t)(v10));
  t111 = ((int64_t)(((int64_t)UINT64_C(1))));
  t112 = ((uint64_t)(v5));
  t113 = ((uint64_t)(v7));
  t114 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t115 = gg_function_28(t113, t114, base + 0);
  t116 = gg_function_29(t112, t115, base + 0);
  fg159_hash_put(t110, t111, t116);
  t117 = ((uint64_t)(v7));
  t118 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t119 = gg_function_28(t117, t118, base + 0);
  t120 = ((uint64_t)(v6));
  t121 = gg_function_11(t120, base + 0);
  t122 = ((uint64_t)(t121));
  t123 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t124 = ((uint64_t)(((uint64_t)((uint64_t)(t122) * (uint64_t)(t123)))));
  v7 = gg_function_29(t119, t124, base + 0);
  t125 = ((uint64_t)(v7));
  t126 = ((uint64_t)(v3));
  t127 = ((t125) == (t126));
  t128 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_function_10(t127, t128, base + 0);
  t129 = ((int64_t)(v10));
  gg_function_15(t129, base + 0);
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_42(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  uint64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
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
  int64_t t43;
  uint64_t t44;
  uint64_t t45;
  int64_t t46;
  int64_t t47;
  uint64_t t48;
  uint64_t t49;
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
  uint64_t t60;
  uint64_t t61;
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
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v2));
  t3 = ((t4) >= (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((uint64_t)(((uint64_t)(v3))));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  t11 = fg159_graph(t9, t10);
  t12 = ((uint64_t)(t11));
  t7 = ((t8) <= (t12));
  }
  t13 = t7;
  t14 = ((int64_t)(((int64_t)UINT64_C(21))));
  gg_function_10(t13, t14, base + 0);
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = fg159_constant(t16);
  t18 = ((uint64_t)(t17));
  fg159_zero(t15, t18);
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(v2));
  t23 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t21) - (uint64_t)(t22)))))));
  fg159_graph_put(t19, t20, t23);
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(3))));
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(((int64_t)UINT64_C(3))));
  t28 = fg159_graph(t26, t27);
  t29 = ((uint64_t)(t28));
  t30 = ((uint64_t)(((uint64_t)(v2))));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  t32 = gg_function_8(t30, t31, base + 0);
  t33 = gg_function_29(t29, t32, base + 0);
  fg159_graph_put(t24, t25, t33);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(5))));
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(((int64_t)UINT64_C(5))));
  t38 = fg159_graph(t36, t37);
  t39 = ((uint64_t)(t38));
  fg159_graph_put(t34, t35, t39);
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(8))));
  t42 = ((int64_t)(v1));
  t43 = ((int64_t)(((int64_t)UINT64_C(8))));
  t44 = fg159_graph(t42, t43);
  t45 = ((uint64_t)(t44));
  fg159_graph_put(t40, t41, t45);
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(10))));
  t48 = fg159_graph(t46, t47);
  t49 = ((uint64_t)(t48));
  v4 = gg_function_3(t49, base + 0);
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(((int64_t)UINT64_C(10))));
  t52 = fg159_graph(t50, t51);
  t53 = ((uint64_t)(t52));
  v5 = gg_function_3(t53, base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t54 = ((int64_t)(v6));
  t55 = ((int64_t)(((int64_t)UINT64_C(3))));
  if (!(((t54) < (t55)))) goto label2;
  {
  t56 = ((int64_t)(v4));
  t57 = ((int64_t)(v6));
  t58 = ((int64_t)(v5));
  t59 = ((int64_t)(v6));
  t60 = fg159_hash(t58, t59);
  t61 = ((uint64_t)(t60));
  fg159_hash_put(t56, t57, t61);
  t62 = v6;
  t63 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t62) + (uint64_t)(t63)));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_38(uint8_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  fg159_free(t2);
  t3 = ((int64_t)(v2));
  fg159_fail(t3);
  }
  }
  }
gg_done:;
}
static void gg_function_37(int64_t p0, int64_t p1, uint8_t p2, int64_t p3, uint64_t p4, uint64_t p5, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint8_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint64_t t20;
  uint64_t t21;
  int64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  int64_t t42;
  uint64_t t43;
  uint8_t t44;
  uint8_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
  uint64_t t50;
  int64_t t51;
  uint64_t t52;
  uint64_t t53;
  int64_t t54;
  uint64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  uint64_t t60;
  int64_t t61;
  uint64_t t62;
  uint64_t t63;
  int64_t t64;
  uint64_t t65;
  uint64_t t66;
  uint64_t t67;
  uint64_t t68;
  int64_t t69;
  uint64_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint64_t t74;
  uint64_t t75;
  int64_t t76;
  uint64_t t77;
  uint64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint8_t t83;
  uint8_t t84;
  uint8_t t85;
  uint8_t t86;
  uint8_t t87;
  int64_t t88;
  uint64_t t89;
  uint64_t t90;
  int64_t t91;
  uint64_t t92;
  uint64_t t93;
  uint64_t t94;
  uint64_t t95;
  uint64_t t96;
  uint64_t t97;
  uint8_t t98;
  uint64_t t99;
  int64_t v0;
  int64_t v1;
  uint8_t v2;
  int64_t v3;
  uint64_t v4;
  uint64_t v5;
  uint64_t v6;
  int64_t v7;
  uint64_t v8;
  int64_t v9;
  uint8_t v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((uint64_t)(v4));
  t1 = ((uint64_t)(v5));
  t2 = ((t0) < (t1));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_38(t2, t3, t4, base + 0);
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg159_tensor(t5, t6, t7);
  v6 = ((uint64_t)(t8));
  t9 = ((uint64_t)(v6));
  t10 = ((int64_t)(((int64_t)UINT64_C(8))));
  t11 = fg159_constant(t10);
  t12 = ((uint64_t)(t11));
  t13 = ((t9) != (t12));
  t14 = t13;
  if (t14) {
  t14 = v2;
  }
  t15 = t14;
  if (t15) {
  {
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg159_tensor(t17, t18, t19);
  t21 = ((uint64_t)(t20));
  t22 = ((int64_t)(((int64_t)UINT64_C(7))));
  t23 = fg159_constant(t22);
  t24 = ((uint64_t)(t23));
  t25 = ((uint64_t)(((t21) | (t24))));
  fg159_flags(t16, t25);
  }
  }
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(10))));
  t28 = fg159_graph(t26, t27);
  t29 = ((uint64_t)(t28));
  v7 = gg_function_3(t29, base + 0);
  t30 = ((int64_t)(v7));
  t31 = ((uint64_t)(((uint64_t)(v1))));
  v8 = gg_function_22(t30, t31, base + 0);
  t32 = ((uint64_t)(v8));
  t33 = gg_function_9(base + 0);
  t34 = ((uint64_t)(t33));
  t35 = ((t32) != (t34));
  t36 = ((int64_t)(v3));
  t37 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_38(t35, t36, t37, base + 0);
  t38 = ((int64_t)(v7));
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = fg159_hash(t38, t39);
  t41 = ((uint64_t)(t40));
  v9 = gg_function_3(t41, base + 0);
  t42 = ((int64_t)(v9));
  t43 = ((uint64_t)(v8));
  t44 = gg_function_18(t42, t43, base + 0);
  t45 = t44;
  v10 = (!(t45));
  t46 = v10;
  if (t46) {
  {
  t47 = ((int64_t)(v7));
  t48 = ((int64_t)(((int64_t)UINT64_C(2))));
  t49 = fg159_hash(t47, t48);
  t50 = ((uint64_t)(t49));
  t51 = gg_function_3(t50, base + 0);
  t52 = ((uint64_t)(v8));
  t53 = ((uint64_t)(((uint64_t)(v1))));
  fg159_ptr_put(t51, t52, t53);
  t54 = ((int64_t)(v9));
  t55 = ((uint64_t)(v8));
  t56 = ((uint8_t)UINT64_C(1));
  gg_function_20(t54, t55, t56, base + 0);
  t57 = ((int64_t)(v0));
  t58 = ((int64_t)(((int64_t)UINT64_C(5))));
  t59 = fg159_graph(t57, t58);
  t60 = ((uint64_t)(t59));
  t61 = gg_function_3(t60, base + 0);
  t62 = ((uint64_t)(v8));
  t63 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg159_u32_put(t61, t62, t63);
  }
  }
  t64 = ((int64_t)(v3));
  t65 = ((uint64_t)(v4));
  t66 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t67 = ((uint64_t)(((uint64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  t68 = ((uint64_t)(((uint64_t)(v1))));
  fg159_ptr_put(t64, t67, t68);
  t69 = ((int64_t)(v3));
  t70 = ((uint64_t)(v4));
  t71 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t72 = ((uint64_t)(((uint64_t)((uint64_t)(t70) * (uint64_t)(t71)))));
  t73 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t74 = ((uint64_t)(((uint64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  t75 = ((uint64_t)(v8));
  fg159_ptr_put(t69, t74, t75);
  t76 = ((int64_t)(v3));
  t77 = ((uint64_t)(v4));
  t78 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t79 = ((uint64_t)(((uint64_t)((uint64_t)(t77) * (uint64_t)(t78)))));
  t80 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t81 = ((uint64_t)(((uint64_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  t83 = v10;
  t84 = (!(t83));
  t85 = t84;
  if (t85) {
  t86 = v2;
  t85 = (!(t86));
  }
  t87 = t85;
  if (t87) {
  {
  t88 = ((int64_t)(((int64_t)UINT64_C(5))));
  t89 = fg159_constant(t88);
  t82 = ((uint64_t)(t89));
  }
  } else {
  {
  t82 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t90 = ((uint64_t)(t82));
  fg159_ptr_put(t76, t81, t90);
  t91 = ((int64_t)(v3));
  t92 = ((uint64_t)(v4));
  t93 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t94 = ((uint64_t)(((uint64_t)((uint64_t)(t92) * (uint64_t)(t93)))));
  t95 = ((uint64_t)(((int64_t)UINT64_C(3))));
  t96 = ((uint64_t)(((uint64_t)((uint64_t)(t94) + (uint64_t)(t95)))));
  t98 = v10;
  if (t98) {
  {
  t97 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t97 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t99 = ((uint64_t)(t97));
  fg159_ptr_put(t91, t96, t99);
  }
gg_done:;
}
static uint64_t gg_function_36(int64_t p0, int64_t p1, uint8_t p2, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  int64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  int64_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
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
  uint64_t t52;
  int64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint8_t t56;
  int64_t t57;
  uint64_t t58;
  uint64_t t59;
  uint64_t t60;
  uint64_t t61;
  uint64_t t62;
  uint64_t t63;
  uint8_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  uint64_t t68;
  uint64_t t69;
  uint64_t t70;
  uint8_t t71;
  int64_t t72;
  uint64_t t73;
  uint64_t t74;
  uint64_t t75;
  uint64_t t76;
  uint64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint64_t t81;
  uint64_t t82;
  int64_t t83;
  int64_t t84;
  uint8_t t85;
  uint8_t t86;
  uint8_t t87;
  uint8_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  uint64_t t92;
  uint64_t t93;
  int64_t t94;
  uint64_t t95;
  uint64_t t96;
  uint64_t t97;
  uint64_t t98;
  uint8_t t99;
  uint64_t t100;
  uint64_t t101;
  int64_t t102;
  int64_t t103;
  uint8_t t104;
  int64_t t105;
  uint64_t t106;
  uint64_t t107;
  uint8_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint64_t t112;
  uint64_t t113;
  int64_t t114;
  uint64_t t115;
  uint64_t t116;
  uint8_t t117;
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  uint64_t t122;
  uint64_t t123;
  int64_t t124;
  uint64_t t125;
  uint64_t t126;
  uint64_t t127;
  uint64_t t128;
  int64_t t129;
  int64_t t130;
  uint8_t t131;
  int64_t t132;
  uint64_t t133;
  uint64_t t134;
  int64_t t135;
  int64_t t136;
  uint64_t t137;
  uint64_t t138;
  uint8_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  uint64_t t145;
  uint64_t t146;
  uint64_t t147;
  uint8_t t148;
  int64_t t149;
  int64_t t150;
  uint8_t t151;
  int64_t t152;
  uint64_t t153;
  int64_t t154;
  int64_t t155;
  uint8_t t156;
  int64_t t157;
  uint64_t t158;
  uint64_t t159;
  int64_t t160;
  uint64_t t161;
  uint64_t t162;
  int64_t t163;
  int64_t t164;
  uint8_t t165;
  int64_t t166;
  uint64_t t167;
  uint64_t t168;
  uint64_t t169;
  uint64_t t170;
  uint64_t t171;
  uint8_t t172;
  uint64_t t173;
  uint64_t t174;
  int64_t t175;
  uint64_t t176;
  uint64_t t177;
  uint64_t t178;
  uint64_t t179;
  uint64_t t180;
  uint64_t t181;
  uint64_t t182;
  uint64_t t183;
  uint8_t t184;
  int64_t t185;
  int64_t t186;
  uint64_t t187;
  uint64_t t188;
  int64_t t189;
  uint64_t t190;
  uint64_t t191;
  uint64_t t192;
  uint64_t t193;
  uint8_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  uint64_t t198;
  uint64_t t199;
  uint64_t t200;
  uint64_t t201;
  int64_t t202;
  int64_t v0;
  int64_t v1;
  uint8_t v2;
  int64_t v3;
  uint64_t v4;
  uint64_t v5;
  int64_t v6;
  uint64_t v7;
  uint64_t v8;
  uint64_t v9;
  int64_t v10;
  uint64_t v11;
  uint64_t v12;
  uint8_t v13;
  uint64_t v14;
  int64_t v15;
  uint8_t v16;
  uint64_t v17;
  int64_t v18;
  uint64_t v19;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_function_10(t3, t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(10))));
  t7 = fg159_graph(t5, t6);
  t8 = ((uint64_t)(t7));
  v3 = gg_function_3(t8, base + 0);
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg159_hash(t9, t10);
  t12 = ((uint64_t)(t11));
  t13 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  v4 = gg_function_29(t12, t13, base + 0);
  t14 = ((uint64_t)(v4));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32))))));
  v5 = gg_function_8(t14, t15, base + 0);
  t16 = ((uint64_t)(v5));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg159_alloc(t16, t17);
  v6 = ((int64_t)(t18));
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) != (t20));
  t22 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_10(t21, t22, base + 0);
  v7 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(v1));
  t25 = v2;
  t26 = ((int64_t)(v6));
  t27 = ((uint64_t)(v7));
  t28 = ((uint64_t)(v4));
  gg_function_37(t23, t24, t25, t26, t27, t28, base + 0);
  v8 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  if (!(((uint8_t)UINT64_C(1)))) goto label2;
  {
  t29 = ((uint64_t)(v7));
  t30 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v9 = ((uint64_t)(((uint64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t31 = ((int64_t)(v6));
  t32 = ((uint64_t)(v9));
  t33 = fg159_ptr(t31, t32);
  t34 = ((uint64_t)(t33));
  v10 = gg_function_3(t34, base + 0);
  t35 = ((int64_t)(v6));
  t36 = ((uint64_t)(v9));
  t37 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t38 = ((uint64_t)(((uint64_t)((uint64_t)(t36) + (uint64_t)(t37)))));
  t39 = fg159_ptr(t35, t38);
  v11 = ((uint64_t)(t39));
  t40 = ((int64_t)(v6));
  t41 = ((uint64_t)(v9));
  t42 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t43 = ((uint64_t)(((uint64_t)((uint64_t)(t41) + (uint64_t)(t42)))));
  t44 = fg159_ptr(t40, t43);
  v12 = ((uint64_t)(t44));
  t45 = ((int64_t)(v6));
  t46 = ((uint64_t)(v9));
  t47 = ((uint64_t)(((int64_t)UINT64_C(3))));
  t48 = ((uint64_t)(((uint64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  t49 = fg159_ptr(t45, t48);
  t50 = ((uint64_t)(t49));
  t51 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v13 = ((t50) != (t51));
  t52 = ((uint64_t)(v12));
  t53 = ((int64_t)(((int64_t)UINT64_C(5))));
  t54 = fg159_constant(t53);
  t55 = ((uint64_t)(t54));
  t56 = ((t52) < (t55));
  if (t56) {
  {
  t57 = ((int64_t)(v6));
  t58 = ((uint64_t)(v9));
  t59 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t60 = ((uint64_t)(((uint64_t)((uint64_t)(t58) + (uint64_t)(t59)))));
  t61 = ((uint64_t)(v12));
  t62 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t63 = ((uint64_t)(((uint64_t)((uint64_t)(t61) + (uint64_t)(t62)))));
  fg159_ptr_put(t57, t60, t63);
  t64 = v13;
  t65 = t64;
  if (t65) {
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(8))));
  t68 = fg159_graph(t66, t67);
  t69 = ((uint64_t)(t68));
  t70 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t65 = ((t69) == (t70));
  }
  t71 = t65;
  if (t71) {
  {
  t72 = ((int64_t)(((int64_t)UINT64_C(5))));
  t73 = fg159_constant(t72);
  t74 = ((uint64_t)(t73));
  t75 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t76 = ((uint64_t)(((uint64_t)((uint64_t)(t74) - (uint64_t)(t75)))));
  t77 = ((uint64_t)(v12));
  v14 = ((uint64_t)(((uint64_t)((uint64_t)(t76) - (uint64_t)(t77)))));
  }
  } else {
  {
  v14 = ((uint64_t)(v12));
  }
  }
  t78 = ((int64_t)(v10));
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((int64_t)(((int64_t)(v14))));
  t81 = fg159_tensor(t78, t79, t80);
  t82 = ((uint64_t)(t81));
  v15 = gg_function_3(t82, base + 0);
  t83 = ((int64_t)(v15));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((t83) != (t84));
  t86 = t85;
  if (t86) {
  t87 = v13;
  t88 = t87;
  if (!t88) {
  t89 = ((int64_t)(v15));
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = fg159_tensor(t89, t90, t91);
  t93 = ((uint64_t)(t92));
  t94 = ((int64_t)(((int64_t)UINT64_C(7))));
  t95 = fg159_constant(t94);
  t96 = ((uint64_t)(t95));
  t97 = ((uint64_t)(((t93) & (t96))));
  t98 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t88 = ((t97) == (t98));
  }
  t86 = t88;
  }
  t99 = t86;
  if (t99) {
  {
  t100 = v7;
  t101 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v7 = ((uint64_t)((uint64_t)(t100) + (uint64_t)(t101)));
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(v15));
  t104 = v2;
  t105 = ((int64_t)(v6));
  t106 = ((uint64_t)(v7));
  t107 = ((uint64_t)(v4));
  gg_function_37(t102, t103, t104, t105, t106, t107, base + 0);
  }
  }
  goto label1;
  }
  }
  t108 = v13;
  if (t108) {
  {
  t109 = ((int64_t)(v10));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = fg159_tensor(t109, t110, t111);
  t113 = ((uint64_t)(t112));
  t114 = ((int64_t)(((int64_t)UINT64_C(8))));
  t115 = fg159_constant(t114);
  t116 = ((uint64_t)(t115));
  t117 = ((t113) == (t116));
  t118 = t117;
  if (t118) {
  t119 = ((int64_t)(v10));
  t120 = ((int64_t)(((int64_t)UINT64_C(1))));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = fg159_tensor(t119, t120, t121);
  t123 = ((uint64_t)(t122));
  t124 = ((int64_t)(((int64_t)UINT64_C(6))));
  t125 = fg159_constant(t124);
  t126 = ((uint64_t)(t125));
  t127 = ((uint64_t)(((t123) & (t126))));
  t128 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t118 = ((t127) == (t128));
  }
  v16 = t118;
  t129 = ((int64_t)(v0));
  t131 = v16;
  if (t131) {
  {
  t130 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  t130 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t132 = ((int64_t)(t130));
  t133 = fg159_graph(t129, t132);
  v17 = ((uint64_t)(t133));
  t134 = ((uint64_t)(v17));
  t135 = ((int64_t)(v0));
  t136 = ((int64_t)(((int64_t)UINT64_C(0))));
  t137 = fg159_graph(t135, t136);
  t138 = ((uint64_t)(t137));
  t139 = ((t134) < (t138));
  t140 = ((int64_t)(v6));
  t141 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_function_38(t139, t140, t141, base + 0);
  t142 = ((int64_t)(v10));
  t143 = ((int64_t)(((int64_t)UINT64_C(3))));
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t145 = fg159_tensor(t142, t143, t144);
  t146 = ((uint64_t)(t145));
  t147 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t148 = ((t146) == (t147));
  if (t148) {
  {
  t149 = ((int64_t)(v10));
  t151 = v16;
  if (t151) {
  {
  t150 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t150 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t152 = ((int64_t)(t150));
  t153 = ((uint64_t)(v17));
  fg159_name(t149, t152, t153);
  }
  }
  t154 = ((int64_t)(v0));
  t156 = v16;
  if (t156) {
  {
  t155 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  } else {
  {
  t155 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  }
  t157 = ((int64_t)(t155));
  t158 = fg159_graph(t154, t157);
  t159 = ((uint64_t)(t158));
  t160 = gg_function_3(t159, base + 0);
  t161 = ((uint64_t)(v17));
  t162 = ((uint64_t)(((uint64_t)(v10))));
  fg159_ptr_put(t160, t161, t162);
  t163 = ((int64_t)(v0));
  t165 = v16;
  if (t165) {
  {
  t164 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  t164 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t166 = ((int64_t)(t164));
  t167 = ((uint64_t)(v17));
  t168 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t169 = ((uint64_t)(((uint64_t)((uint64_t)(t167) + (uint64_t)(t168)))));
  fg159_graph_put(t163, t166, t169);
  }
  }
  v8 = ((uint64_t)(v11));
  t170 = ((uint64_t)(v7));
  t171 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t172 = ((t170) == (t171));
  if (t172) {
  {
  goto label2;
  }
  }
  t173 = v7;
  t174 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v7 = ((uint64_t)((uint64_t)(t173) - (uint64_t)(t174)));
  t175 = ((int64_t)(v6));
  t176 = ((uint64_t)(v7));
  t177 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t178 = ((uint64_t)(((uint64_t)((uint64_t)(t176) * (uint64_t)(t177)))));
  t179 = ((uint64_t)(((int64_t)UINT64_C(3))));
  t180 = ((uint64_t)(((uint64_t)((uint64_t)(t178) + (uint64_t)(t179)))));
  t181 = fg159_ptr(t175, t180);
  t182 = ((uint64_t)(t181));
  t183 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t184 = ((t182) != (t183));
  if (t184) {
  {
  t185 = ((int64_t)(v0));
  t186 = ((int64_t)(((int64_t)UINT64_C(5))));
  t187 = fg159_graph(t185, t186);
  t188 = ((uint64_t)(t187));
  v18 = gg_function_3(t188, base + 0);
  t189 = ((int64_t)(v18));
  t190 = ((uint64_t)(v11));
  t191 = fg159_u32(t189, t190);
  v19 = ((uint64_t)(t191));
  t192 = ((uint64_t)(v19));
  t193 = ((uint64_t)(((int64_t)UINT64_C(2147483647))));
  t194 = ((t192) < (t193));
  t195 = ((int64_t)(v6));
  t196 = ((int64_t)(((int64_t)UINT64_C(15))));
  gg_function_38(t194, t195, t196, base + 0);
  t197 = ((int64_t)(v18));
  t198 = ((uint64_t)(v11));
  t199 = ((uint64_t)(v19));
  t200 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t201 = ((uint64_t)(((uint64_t)((uint64_t)(t199) + (uint64_t)(t200)))));
  fg159_u32_put(t197, t198, t201);
  }
  }
  }
  goto label1;
label2:;
  t202 = ((int64_t)(v6));
  fg159_free(t202);
  t0 = ((uint64_t)(v8));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_34(int64_t p0, int64_t p1, uint8_t p2, uint8_t p3, int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  uint64_t t19;
  int64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t v0;
  int64_t v1;
  uint8_t v2;
  uint8_t v3;
  uint64_t v4;
  uint64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = v2;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(v0));
  gg_function_35(t2, base + 0);
  }
  }
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(1))));
  t5 = fg159_graph(t3, t4);
  v4 = ((uint64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v1));
  t8 = v3;
  t9 = gg_function_36(t6, t7, t8, base + 0);
  (void)(t9);
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = fg159_graph(t10, t11);
  v5 = ((uint64_t)(t12));
  t13 = ((uint64_t)(v5));
  t14 = ((uint64_t)(v4));
  t15 = ((t13) > (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(3))));
  t18 = fg159_graph(t16, t17);
  t19 = ((uint64_t)(t18));
  t20 = gg_function_3(t19, base + 0);
  t21 = ((uint64_t)(v5));
  t22 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t23 = ((uint64_t)(((uint64_t)((uint64_t)(t21) - (uint64_t)(t22)))));
  t24 = fg159_ptr(t20, t23);
  t25 = ((uint64_t)(t24));
  t26 = ((uint64_t)(((uint64_t)(v1))));
  t27 = ((t25) == (t26));
  t28 = ((int64_t)(((int64_t)UINT64_C(16))));
  gg_function_10(t27, t28, base + 0);
  }
  }
  }
gg_done:;
}
static void gg_function_7(int64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  uint64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  int64_t t27;
  int64_t t28;
  uint64_t t29;
  int64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((uint64_t)(v1));
  v2 = gg_function_4(t0, base + 0);
  t1 = ((uint64_t)(v2));
  t2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  v3 = gg_function_8(t1, t2, base + 0);
  t3 = ((uint64_t)(v2));
  t4 = gg_function_11(t3, base + 0);
  t5 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  v4 = gg_function_8(t4, t5, base + 0);
  t6 = ((uint64_t)(v3));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = fg159_alloc(t6, t7);
  v5 = ((int64_t)(t8));
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) != (t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_function_10(t11, t12, base + 0);
  t13 = ((uint64_t)(v4));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = fg159_alloc(t13, t14);
  v6 = ((int64_t)(t15));
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t19 = ((int64_t)(v5));
  fg159_free(t19);
  t20 = ((int64_t)(((int64_t)UINT64_C(7))));
  fg159_fail(t20);
  goto gg_done;
  }
  }
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((uint64_t)(v2));
  fg159_hash_put(t21, t22, t23);
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((uint64_t)(((uint64_t)(v6))));
  fg159_hash_put(t24, t25, t26);
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((uint64_t)(((uint64_t)(v5))));
  fg159_hash_put(t27, t28, t29);
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
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  uint64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  uint64_t t62;
  uint64_t t63;
  uint8_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  uint8_t t68;
  uint8_t t69;
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
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  uint64_t t86;
  uint64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint8_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint64_t t95;
  uint64_t t96;
  int64_t t97;
  int64_t t98;
  uint8_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint64_t t103;
  uint64_t t104;
  uint64_t t105;
  uint64_t t106;
  uint64_t t107;
  uint8_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  uint64_t t112;
  uint64_t t113;
  int64_t t114;
  uint64_t t115;
  uint8_t t116;
  uint8_t t117;
  int64_t t118;
  int64_t t119;
  uint8_t t120;
  uint8_t t121;
  int64_t t122;
  int64_t t123;
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
  int64_t t136;
  int64_t t137;
  uint64_t t138;
  uint64_t t139;
  int64_t t140;
  int64_t t141;
  uint8_t t142;
  int64_t t143;
  int64_t t144;
  uint8_t t145;
  int64_t t146;
  uint64_t t147;
  uint64_t t148;
  int64_t t149;
  int64_t t150;
  uint64_t t151;
  uint64_t t152;
  int64_t t153;
  int64_t t154;
  uint8_t t155;
  uint64_t t156;
  int64_t t157;
  uint64_t t158;
  uint64_t t159;
  uint64_t t160;
  int64_t t161;
  int64_t t162;
  uint8_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  uint64_t t167;
  uint64_t t168;
  int64_t t169;
  int64_t t170;
  uint8_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  uint64_t t183;
  uint64_t t184;
  uint64_t t185;
  uint8_t t186;
  int64_t t187;
  int64_t t188;
  uint8_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  uint8_t t194;
  int64_t t195;
  int64_t t196;
  uint8_t t197;
  int64_t t198;
  int64_t t199;
  uint64_t t200;
  uint64_t t201;
  int64_t t202;
  int64_t t203;
  uint64_t t204;
  uint64_t t205;
  int64_t t206;
  int64_t t207;
  uint8_t t208;
  uint8_t t209;
  int64_t t210;
  int64_t t211;
  uint8_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  uint64_t t218;
  uint64_t t219;
  uint64_t t220;
  int64_t t221;
  uint8_t t222;
  int64_t t223;
  int64_t t224;
  uint8_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  uint64_t t229;
  uint64_t t230;
  int64_t t231;
  int64_t t232;
  uint8_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  uint64_t t239;
  uint64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  uint8_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  uint8_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  uint64_t t253;
  uint64_t t254;
  int64_t t255;
  int64_t t256;
  uint64_t t257;
  uint64_t t258;
  uint64_t t259;
  uint8_t t260;
  uint8_t t261;
  int64_t t262;
  int64_t t263;
  uint8_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  uint8_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  uint8_t t273;
  int64_t t274;
  int64_t t275;
  uint64_t t276;
  int64_t t277;
  int64_t t278;
  uint8_t t279;
  int64_t t280;
  int64_t t281;
  uint64_t t282;
  int64_t t283;
  int64_t t284;
  uint8_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  uint8_t t290;
  uint8_t t291;
  uint64_t t292;
  uint64_t t293;
  uint8_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  uint64_t t298;
  uint64_t t299;
  int64_t t300;
  uint64_t t301;
  uint64_t t302;
  int64_t t303;
  int64_t t304;
  uint8_t t305;
  int64_t t306;
  int64_t t307;
  uint64_t t308;
  int64_t t309;
  int64_t t310;
  uint8_t t311;
  int64_t t312;
  int64_t t313;
  uint64_t t314;
  int64_t t315;
  int64_t t316;
  uint8_t t317;
  int64_t t318;
  int64_t t319;
  uint64_t t320;
  uint64_t t321;
  int64_t t322;
  int64_t t323;
  uint64_t t324;
  uint64_t t325;
  uint8_t t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  uint64_t t330;
  uint64_t t331;
  int64_t t332;
  uint64_t t333;
  uint64_t t334;
  int64_t t335;
  int64_t t336;
  uint64_t t337;
  uint64_t t338;
  uint64_t t339;
  int64_t t340;
  int64_t t341;
  uint8_t t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  uint8_t t349;
  int64_t t350;
  uint64_t t351;
  int64_t t352;
  int64_t t353;
  int64_t t354;
  int64_t t355;
  uint8_t t356;
  int64_t t357;
  uint64_t t358;
  uint64_t t359;
  uint64_t t360;
  uint64_t t361;
  int64_t t362;
  uint64_t t363;
  uint64_t t364;
  uint64_t t365;
  int64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  int64_t t370;
  uint8_t t371;
  uint64_t t372;
  uint64_t t373;
  int64_t t374;
  int64_t t375;
  int64_t t376;
  int64_t t377;
  uint8_t t378;
  uint8_t t379;
  int64_t t380;
  int64_t t381;
  uint8_t t382;
  int64_t t383;
  int64_t t384;
  int64_t t385;
  int64_t t386;
  uint8_t t387;
  int64_t t388;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  uint64_t v10;
  int64_t v11;
  uint64_t v12;
  int64_t v13;
  uint64_t v14;
  int64_t v15;
  uint64_t v16;
  int64_t v17;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg159_arg(t1, t2);
  t4 = ((uint64_t)(t3));
  v1 = gg_function_3(t4, base + 0);
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = fg159_arg(t5, t6);
  t8 = ((uint64_t)(t7));
  v2 = gg_function_3(t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = fg159_arg(t9, t10);
  t12 = ((uint64_t)(t11));
  v3 = gg_function_3(t12, base + 0);
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = fg159_arg(t13, t14);
  t16 = ((uint64_t)(t15));
  v4 = gg_function_3(t16, base + 0);
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = fg159_arg(t20, t21);
  t23 = ((uint64_t)(t22));
  t0 = gg_function_4(t23, base + 0);
  goto gg_done;
  }
  }
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((t24) == (t25));
  if (t26) {
  {
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(2))));
  t30 = fg159_arg(t28, t29);
  t31 = ((uint64_t)(t30));
  gg_function_7(t27, t31, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t32 = ((int64_t)(v1));
  t33 = ((int64_t)(((int64_t)UINT64_C(2))));
  t34 = ((t32) == (t33));
  if (t34) {
  {
  t35 = ((int64_t)(v2));
  gg_function_15(t35, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  t39 = ((int64_t)(v2));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  t41 = fg159_hash(t39, t40);
  t42 = ((uint64_t)(t41));
  t43 = gg_function_3(t42, base + 0);
  fg159_free(t43);
  t44 = ((int64_t)(v2));
  t45 = ((int64_t)(((int64_t)UINT64_C(2))));
  t46 = fg159_hash(t44, t45);
  t47 = ((uint64_t)(t46));
  t48 = gg_function_3(t47, base + 0);
  fg159_free(t48);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t49 = ((int64_t)(v1));
  t50 = ((int64_t)(((int64_t)UINT64_C(4))));
  t51 = ((t49) == (t50));
  if (t51) {
  {
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  t54 = fg159_arg(t52, t53);
  t55 = ((uint64_t)(t54));
  t0 = gg_function_11(t55, base + 0);
  goto gg_done;
  }
  }
  t56 = ((int64_t)(v1));
  t57 = ((int64_t)(((int64_t)UINT64_C(5))));
  t58 = ((t56) == (t57));
  if (t58) {
  {
  t59 = ((int64_t)(v2));
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(2))));
  t62 = fg159_arg(t60, t61);
  t63 = ((uint64_t)(t62));
  t64 = gg_function_18(t59, t63, base + 0);
  t65 = t64;
  if (t65) {
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
  t66 = ((int64_t)(v1));
  t67 = ((int64_t)(((int64_t)UINT64_C(6))));
  t68 = ((t66) == (t67));
  t69 = t68;
  if (!t69) {
  t70 = ((int64_t)(v1));
  t71 = ((int64_t)(((int64_t)UINT64_C(7))));
  t69 = ((t70) == (t71));
  }
  t72 = t69;
  if (t72) {
  {
  t73 = ((int64_t)(v2));
  t74 = ((int64_t)(v0));
  t75 = ((int64_t)(((int64_t)UINT64_C(2))));
  t76 = fg159_arg(t74, t75);
  t77 = ((uint64_t)(t76));
  t78 = ((int64_t)(v1));
  t79 = ((int64_t)(((int64_t)UINT64_C(6))));
  t80 = ((t78) == (t79));
  gg_function_20(t73, t77, t80, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t81 = ((int64_t)(v1));
  t82 = ((int64_t)(((int64_t)UINT64_C(8))));
  t83 = ((t81) == (t82));
  if (t83) {
  {
  t84 = ((int64_t)(v0));
  t85 = ((int64_t)(((int64_t)UINT64_C(1))));
  t86 = fg159_arg(t84, t85);
  t87 = ((uint64_t)(t86));
  t88 = ((int64_t)(((int64_t)UINT64_C(4))));
  t0 = ((uint64_t)(((t87) >> ((uint64_t)(t88) & 63))));
  goto gg_done;
  }
  }
  t89 = ((int64_t)(v1));
  t90 = ((int64_t)(((int64_t)UINT64_C(9))));
  t91 = ((t89) == (t90));
  if (t91) {
  {
  t92 = ((int64_t)(v2));
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(((int64_t)UINT64_C(2))));
  t95 = fg159_arg(t93, t94);
  t96 = ((uint64_t)(t95));
  t0 = gg_function_22(t92, t96, base + 0);
  goto gg_done;
  }
  }
  t97 = ((int64_t)(v1));
  t98 = ((int64_t)(((int64_t)UINT64_C(10))));
  t99 = ((t97) == (t98));
  if (t99) {
  {
  t100 = ((int64_t)(v2));
  t101 = ((int64_t)(v0));
  t102 = ((int64_t)(((int64_t)UINT64_C(2))));
  t103 = fg159_arg(t101, t102);
  t104 = ((uint64_t)(t103));
  v5 = gg_function_22(t100, t104, base + 0);
  t105 = ((uint64_t)(v5));
  t106 = gg_function_9(base + 0);
  t107 = ((uint64_t)(t106));
  t108 = ((t105) != (t107));
  t109 = t108;
  if (t109) {
  t110 = ((int64_t)(v2));
  t111 = ((int64_t)(((int64_t)UINT64_C(1))));
  t112 = fg159_hash(t110, t111);
  t113 = ((uint64_t)(t112));
  t114 = gg_function_3(t113, base + 0);
  t115 = ((uint64_t)(v5));
  t116 = gg_function_18(t114, t115, base + 0);
  t109 = t116;
  }
  t117 = t109;
  if (t117) {
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
  t118 = ((int64_t)(v1));
  t119 = ((int64_t)(((int64_t)UINT64_C(11))));
  t120 = ((t118) == (t119));
  t121 = t120;
  if (!t121) {
  t122 = ((int64_t)(v1));
  t123 = ((int64_t)(((int64_t)UINT64_C(12))));
  t121 = ((t122) == (t123));
  }
  t124 = t121;
  if (t124) {
  {
  t125 = ((int64_t)(v2));
  t126 = ((int64_t)(v0));
  t127 = ((int64_t)(((int64_t)UINT64_C(2))));
  t128 = fg159_arg(t126, t127);
  t129 = ((uint64_t)(t128));
  t130 = ((int64_t)(v1));
  t131 = ((int64_t)(((int64_t)UINT64_C(12))));
  t132 = ((t130) == (t131));
  t0 = gg_function_24(t125, t129, t132, base + 0);
  goto gg_done;
  }
  }
  t133 = ((int64_t)(v1));
  t134 = ((int64_t)(((int64_t)UINT64_C(13))));
  t135 = ((t133) == (t134));
  if (t135) {
  {
  t136 = ((int64_t)(v0));
  t137 = ((int64_t)(((int64_t)UINT64_C(1))));
  t138 = fg159_arg(t136, t137);
  t139 = ((uint64_t)(t138));
  t140 = ((int64_t)(v3));
  t141 = ((int64_t)(((int64_t)UINT64_C(0))));
  t142 = ((t140) != (t141));
  t0 = gg_function_26(t139, t142, base + 0);
  goto gg_done;
  }
  }
  t143 = ((int64_t)(v1));
  t144 = ((int64_t)(((int64_t)UINT64_C(14))));
  t145 = ((t143) == (t144));
  if (t145) {
  {
  t146 = ((int64_t)(((int64_t)UINT64_C(2))));
  t147 = fg159_constant(t146);
  t148 = ((uint64_t)(t147));
  t149 = ((int64_t)(v0));
  t150 = ((int64_t)(((int64_t)UINT64_C(1))));
  t151 = fg159_arg(t149, t150);
  t152 = ((uint64_t)(t151));
  t153 = ((int64_t)(v3));
  t154 = ((int64_t)(((int64_t)UINT64_C(0))));
  t155 = ((t153) != (t154));
  t156 = gg_function_26(t152, t155, base + 0);
  t157 = ((int64_t)(((int64_t)UINT64_C(1))));
  t158 = fg159_constant(t157);
  t159 = ((uint64_t)(t158));
  t160 = gg_function_28(t156, t159, base + 0);
  t0 = gg_function_29(t148, t160, base + 0);
  goto gg_done;
  }
  }
  t161 = ((int64_t)(v1));
  t162 = ((int64_t)(((int64_t)UINT64_C(15))));
  t163 = ((t161) == (t162));
  if (t163) {
  {
  t164 = ((int64_t)(v2));
  t165 = ((int64_t)(v0));
  t166 = ((int64_t)(((int64_t)UINT64_C(2))));
  t167 = fg159_arg(t165, t166);
  t168 = ((uint64_t)(t167));
  t169 = ((int64_t)(v4));
  t170 = ((int64_t)(((int64_t)UINT64_C(0))));
  t171 = ((t169) != (t170));
  t172 = gg_function_30(t164, t168, t171, base + 0);
  t0 = ((uint64_t)(((uint64_t)(t172))));
  goto gg_done;
  }
  }
  t173 = ((int64_t)(v1));
  t174 = ((int64_t)(((int64_t)UINT64_C(16))));
  t175 = ((t173) == (t174));
  if (t175) {
  {
  t176 = ((int64_t)(v2));
  t177 = ((int64_t)(v3));
  t178 = ((int64_t)(v4));
  t179 = ((int64_t)(((int64_t)UINT64_C(0))));
  t180 = ((t178) != (t179));
  t181 = ((int64_t)(v0));
  t182 = ((int64_t)(((int64_t)UINT64_C(4))));
  t183 = fg159_arg(t181, t182);
  t184 = ((uint64_t)(t183));
  t185 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t186 = ((t184) != (t185));
  gg_function_34(t176, t177, t180, t186, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t187 = ((int64_t)(v1));
  t188 = ((int64_t)(((int64_t)UINT64_C(17))));
  t189 = ((t187) == (t188));
  if (t189) {
  {
  t190 = ((int64_t)(v2));
  t191 = ((int64_t)(v3));
  t192 = ((int64_t)(v4));
  t193 = ((int64_t)(((int64_t)UINT64_C(0))));
  t194 = ((t192) != (t193));
  t0 = gg_function_36(t190, t191, t194, base + 0);
  goto gg_done;
  }
  }
  t195 = ((int64_t)(v1));
  t196 = ((int64_t)(((int64_t)UINT64_C(18))));
  t197 = ((t195) == (t196));
  if (t197) {
  {
  t198 = ((int64_t)(v0));
  t199 = ((int64_t)(((int64_t)UINT64_C(3))));
  t200 = fg159_arg(t198, t199);
  t201 = ((uint64_t)(t200));
  v6 = gg_function_3(t201, base + 0);
  t202 = ((int64_t)(v0));
  t203 = ((int64_t)(((int64_t)UINT64_C(4))));
  t204 = fg159_arg(t202, t203);
  t205 = ((uint64_t)(t204));
  v7 = gg_function_3(t205, base + 0);
  t206 = ((int64_t)(v7));
  t207 = ((int64_t)(((int64_t)UINT64_C(0))));
  t208 = ((t206) >= (t207));
  t209 = t208;
  if (t209) {
  t210 = ((int64_t)(v7));
  t211 = ((int64_t)(v6));
  t209 = ((t210) < (t211));
  }
  t212 = t209;
  t213 = ((int64_t)(((int64_t)UINT64_C(22))));
  gg_function_10(t212, t213, base + 0);
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t214 = ((int64_t)(v8));
  t215 = ((int64_t)(v6));
  if (!(((t214) < (t215)))) goto label2;
  {
  t216 = ((int64_t)(v2));
  t217 = ((int64_t)(v3));
  t218 = ((uint64_t)(((uint64_t)(v8))));
  t219 = fg159_ptr(t217, t218);
  t220 = ((uint64_t)(t219));
  t221 = gg_function_3(t220, base + 0);
  t222 = ((uint8_t)UINT64_C(1));
  t223 = ((int64_t)(v8));
  t224 = ((int64_t)(v7));
  t225 = ((t223) == (t224));
  gg_function_34(t216, t221, t222, t225, base + 0);
  t226 = v8;
  t227 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t226) + (uint64_t)(t227)));
  }
  goto label1;
label2:;
  t228 = ((int64_t)(v3));
  t229 = ((uint64_t)(((uint64_t)(v7))));
  t230 = fg159_ptr(t228, t229);
  t0 = ((uint64_t)(t230));
  goto gg_done;
  }
  }
  t231 = ((int64_t)(v1));
  t232 = ((int64_t)(((int64_t)UINT64_C(19))));
  t233 = ((t231) == (t232));
  if (t233) {
  {
  t234 = ((int64_t)(v2));
  t235 = ((int64_t)(v3));
  t236 = ((int64_t)(v4));
  t237 = ((int64_t)(v0));
  t238 = ((int64_t)(((int64_t)UINT64_C(4))));
  t239 = fg159_arg(t237, t238);
  t240 = ((uint64_t)(t239));
  t241 = gg_function_3(t240, base + 0);
  gg_function_42(t234, t235, t236, t241, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t242 = ((int64_t)(v1));
  t243 = ((int64_t)(((int64_t)UINT64_C(20))));
  t244 = ((t242) == (t243));
  if (t244) {
  {
  t245 = ((int64_t)(v2));
  t246 = ((int64_t)(v3));
  gg_function_43(t245, t246, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t247 = ((int64_t)(v1));
  t248 = ((int64_t)(((int64_t)UINT64_C(21))));
  t249 = ((t247) == (t248));
  if (t249) {
  {
  t250 = ((int64_t)(v2));
  t251 = ((int64_t)(v3));
  t252 = ((int64_t)(((int64_t)UINT64_C(0))));
  t253 = fg159_graph(t251, t252);
  t254 = ((uint64_t)(t253));
  t255 = ((int64_t)(v3));
  t256 = ((int64_t)(((int64_t)UINT64_C(6))));
  t257 = fg159_graph(t255, t256);
  t258 = ((uint64_t)(t257));
  t259 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t260 = ((t258) != (t259));
  t261 = t260;
  if (!t261) {
  t262 = ((int64_t)(v4));
  t263 = ((int64_t)(((int64_t)UINT64_C(0))));
  t261 = ((t262) != (t263));
  }
  t264 = t261;
  v9 = gg_function_30(t250, t254, t264, base + 0);
  t265 = ((int64_t)(v3));
  t266 = ((int64_t)(v9));
  gg_function_43(t265, t266, base + 0);
  t0 = ((uint64_t)(((uint64_t)(v9))));
  goto gg_done;
  }
  }
  t267 = ((int64_t)(v1));
  t268 = ((int64_t)(((int64_t)UINT64_C(22))));
  t269 = ((t267) == (t268));
  if (t269) {
  {
  t270 = ((int64_t)(v2));
  gg_function_35(t270, base + 0);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t271 = ((int64_t)(v1));
  t272 = ((int64_t)(((int64_t)UINT64_C(23))));
  t273 = ((t271) == (t272));
  if (t273) {
  {
  t274 = ((int64_t)(v2));
  t275 = ((int64_t)(((int64_t)UINT64_C(0))));
  t276 = fg159_graph(t274, t275);
  t0 = ((uint64_t)(t276));
  goto gg_done;
  }
  }
  t277 = ((int64_t)(v1));
  t278 = ((int64_t)(((int64_t)UINT64_C(24))));
  t279 = ((t277) == (t278));
  if (t279) {
  {
  t280 = ((int64_t)(v2));
  t281 = ((int64_t)(((int64_t)UINT64_C(1))));
  t282 = fg159_graph(t280, t281);
  v10 = ((uint64_t)(t282));
  v11 = ((int64_t)(v3));
  t283 = ((int64_t)(v11));
  t284 = ((int64_t)(((int64_t)UINT64_C(0))));
  t285 = ((t283) < (t284));
  if (t285) {
  {
  t286 = v11;
  t287 = ((int64_t)(((int64_t)(v10))));
  v11 = ((int64_t)((uint64_t)(t286) + (uint64_t)(t287)));
  }
  }
  t288 = ((int64_t)(v11));
  t289 = ((int64_t)(((int64_t)UINT64_C(0))));
  t290 = ((t288) >= (t289));
  t291 = t290;
  if (t291) {
  t292 = ((uint64_t)(((uint64_t)(v11))));
  t293 = ((uint64_t)(v10));
  t291 = ((t292) < (t293));
  }
  t294 = t291;
  t295 = ((int64_t)(((int64_t)UINT64_C(23))));
  gg_function_10(t294, t295, base + 0);
  t296 = ((int64_t)(v2));
  t297 = ((int64_t)(((int64_t)UINT64_C(3))));
  t298 = fg159_graph(t296, t297);
  t299 = ((uint64_t)(t298));
  t300 = gg_function_3(t299, base + 0);
  t301 = ((uint64_t)(((uint64_t)(v11))));
  t302 = fg159_ptr(t300, t301);
  t0 = ((uint64_t)(t302));
  goto gg_done;
  }
  }
  t303 = ((int64_t)(v1));
  t304 = ((int64_t)(((int64_t)UINT64_C(25))));
  t305 = ((t303) == (t304));
  if (t305) {
  {
  t306 = ((int64_t)(v2));
  t307 = ((int64_t)(((int64_t)UINT64_C(3))));
  t308 = fg159_graph(t306, t307);
  t0 = ((uint64_t)(t308));
  goto gg_done;
  }
  }
  t309 = ((int64_t)(v1));
  t310 = ((int64_t)(((int64_t)UINT64_C(26))));
  t311 = ((t309) == (t310));
  if (t311) {
  {
  t312 = ((int64_t)(v2));
  t313 = ((int64_t)(((int64_t)UINT64_C(1))));
  t314 = fg159_graph(t312, t313);
  t0 = ((uint64_t)(t314));
  goto gg_done;
  }
  }
  t315 = ((int64_t)(v1));
  t316 = ((int64_t)(((int64_t)UINT64_C(27))));
  t317 = ((t315) == (t316));
  if (t317) {
  {
  t318 = ((int64_t)(v2));
  t319 = ((int64_t)(((int64_t)UINT64_C(1))));
  t320 = fg159_graph(t318, t319);
  v12 = ((uint64_t)(t320));
  t321 = ((uint64_t)(v12));
  t322 = ((int64_t)(v2));
  t323 = ((int64_t)(((int64_t)UINT64_C(0))));
  t324 = fg159_graph(t322, t323);
  t325 = ((uint64_t)(t324));
  t326 = ((t321) < (t325));
  t327 = ((int64_t)(((int64_t)UINT64_C(24))));
  gg_function_10(t326, t327, base + 0);
  t328 = ((int64_t)(v2));
  t329 = ((int64_t)(((int64_t)UINT64_C(3))));
  t330 = fg159_graph(t328, t329);
  t331 = ((uint64_t)(t330));
  t332 = gg_function_3(t331, base + 0);
  t333 = ((uint64_t)(v12));
  t334 = ((uint64_t)(((uint64_t)(v3))));
  fg159_ptr_put(t332, t333, t334);
  t335 = ((int64_t)(v2));
  t336 = ((int64_t)(((int64_t)UINT64_C(1))));
  t337 = ((uint64_t)(v12));
  t338 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t339 = ((uint64_t)(((uint64_t)((uint64_t)(t337) + (uint64_t)(t338)))));
  fg159_graph_put(t335, t336, t339);
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t340 = ((int64_t)(v1));
  t341 = ((int64_t)(((int64_t)UINT64_C(28))));
  t342 = ((t340) == (t341));
  if (t342) {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t343 = ((int64_t)(v13));
  t344 = ((int64_t)(((int64_t)UINT64_C(2))));
  if (!(((t343) < (t344)))) goto label5;
  {
  t345 = ((int64_t)(v2));
  t347 = ((int64_t)(v13));
  t348 = ((int64_t)(((int64_t)UINT64_C(0))));
  t349 = ((t347) == (t348));
  if (t349) {
  {
  t346 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  t346 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t350 = ((int64_t)(t346));
  t351 = fg159_graph(t345, t350);
  v14 = ((uint64_t)(t351));
  t352 = ((int64_t)(v2));
  t354 = ((int64_t)(v13));
  t355 = ((int64_t)(((int64_t)UINT64_C(0))));
  t356 = ((t354) == (t355));
  if (t356) {
  {
  t353 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  } else {
  {
  t353 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  }
  t357 = ((int64_t)(t353));
  t358 = fg159_graph(t352, t357);
  t359 = ((uint64_t)(t358));
  v15 = gg_function_3(t359, base + 0);
  v16 = ((uint64_t)(((int64_t)UINT64_C(0))));
label7:;
  t360 = ((uint64_t)(v16));
  t361 = ((uint64_t)(v14));
  if (!(((t360) < (t361)))) goto label8;
  {
  t362 = ((int64_t)(v15));
  t363 = ((uint64_t)(v16));
  t364 = fg159_ptr(t362, t363);
  t365 = ((uint64_t)(t364));
  v17 = gg_function_3(t365, base + 0);
  t366 = ((int64_t)(v17));
  t367 = ((int64_t)(v3));
  t368 = fg159_equal_name(t366, t367);
  t369 = ((int64_t)(t368));
  t370 = ((int64_t)(((int64_t)UINT64_C(0))));
  t371 = ((t369) != (t370));
  if (t371) {
  {
  t0 = ((uint64_t)(((uint64_t)(v17))));
  goto gg_done;
  }
  }
  t372 = v16;
  t373 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v16 = ((uint64_t)((uint64_t)(t372) + (uint64_t)(t373)));
  }
  goto label7;
label8:;
  t374 = v13;
  t375 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)((uint64_t)(t374) + (uint64_t)(t375)));
  }
  goto label4;
label5:;
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
  }
  t376 = ((int64_t)(v1));
  t377 = ((int64_t)(((int64_t)UINT64_C(29))));
  t378 = ((t376) == (t377));
  t379 = t378;
  if (!t379) {
  t380 = ((int64_t)(v1));
  t381 = ((int64_t)(((int64_t)UINT64_C(30))));
  t379 = ((t380) == (t381));
  }
  t382 = t379;
  if (t382) {
  {
  t383 = ((int64_t)(v2));
  t384 = ((int64_t)(v3));
  t385 = ((int64_t)(v1));
  t386 = ((int64_t)(((int64_t)UINT64_C(30))));
  t387 = ((t385) == (t386));
  t0 = gg_function_45(t383, t384, t387, base + 0);
  goto gg_done;
  }
  }
  t388 = ((int64_t)(((int64_t)UINT64_C(99))));
  fg159_fail(t388);
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
