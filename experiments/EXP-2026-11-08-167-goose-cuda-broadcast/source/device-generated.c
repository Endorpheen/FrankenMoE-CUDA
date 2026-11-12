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
static uint8_t gg_eq_3(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint8_t value);
static uint8_t gg_eq_5(float a, float b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, float value);
static uint8_t gg_eq_6(double a, double b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, double value);
static uint8_t gg_eq_7(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, uint32_t value);
static uint8_t gg_eq_8(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, uint8_t value);
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
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(float a, float b) {
return a == b;
}
static void gg_render_5(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(double a, double b) {
return a == b;
}
static void gg_render_6(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_7(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_8(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, uint64_t p7, uint64_t p8, uint64_t p9, uint64_t p10, int64_t base);
static uint32_t gg_function_7(int64_t p0, uint32_t p1, int64_t p2, int64_t base);
static void gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, uint64_t p6, uint64_t p7, uint64_t p8, uint64_t p9, int64_t base);
static uint32_t gg_function_11(int64_t p0, uint32_t p1, int64_t p2, int64_t base);
static float gg_function_12(int64_t p0, float p1, float p2, int64_t base);
static uint32_t gg_function_7(int64_t p0, uint32_t p1, int64_t p2, int64_t base) {
  uint32_t t0;
  uint32_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint32_t t5;
  uint32_t t6;
  uint32_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint32_t t14;
  uint64_t t15;
  int64_t v0;
  uint32_t v1;
  int64_t v2;
  uint32_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((uint32_t)(v1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg167_dmagic(t2, t3, t4);
  t6 = ((uint32_t)(t5));
  t7 = fg167_umulhi(t1, t6);
  v3 = ((uint32_t)(t7));
  t8 = ((uint64_t)(((uint64_t)(v3))));
  t9 = ((uint64_t)(((uint64_t)(v1))));
  t10 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = fg167_dmagic(t11, t12, t13);
  t15 = ((uint64_t)(((uint64_t)(t14))));
  t0 = ((uint32_t)(((uint32_t)(((t10) >> ((uint64_t)(t15) & 63))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_12(int64_t p0, float p1, float p2, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  float t7;
  float t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  float t12;
  float t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  float t17;
  float t18;
  float t19;
  float t20;
  int64_t v0;
  float v1;
  float v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((float)(v2));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t7 = ((float)(v1));
  t8 = ((float)(v2));
  t0 = ((float)(((t7) + (t8))));
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((float)(v1));
  t13 = ((float)(v2));
  t0 = ((float)(((t12) - (t13))));
  goto gg_done;
  }
  }
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = ((float)(v1));
  t18 = ((float)(v2));
  t0 = ((float)(((t17) * (t18))));
  goto gg_done;
  }
  }
  t19 = ((float)(v1));
  t20 = ((float)(v2));
  t0 = ((float)(((t19) / (t20))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint32_t gg_function_11(int64_t p0, uint32_t p1, int64_t p2, int64_t base) {
  uint32_t t0;
  uint32_t t1;
  int64_t t2;
  uint32_t t3;
  int64_t t4;
  uint32_t t5;
  uint32_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint32_t t10;
  uint32_t t11;
  uint32_t t12;
  int64_t v0;
  uint32_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((uint32_t)(v1));
  t2 = ((int64_t)(v0));
  t3 = ((uint32_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = gg_function_7(t2, t3, t4, base + 0);
  t6 = ((uint32_t)(t5));
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  t10 = fg167_dmagic(t7, t8, t9);
  t11 = ((uint32_t)(t10));
  t12 = ((uint32_t)(((uint32_t)((uint64_t)(t6) * (uint64_t)(t11)))));
  t0 = ((uint32_t)(((uint32_t)((uint64_t)(t1) - (uint64_t)(t12)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, uint64_t p6, uint64_t p7, uint64_t p8, uint64_t p9, int64_t base) {
  int64_t t0;
  uint32_t t1;
  int64_t t2;
  uint32_t t3;
  int64_t t4;
  uint32_t t5;
  int64_t t6;
  uint32_t t7;
  int64_t t8;
  uint32_t t9;
  int64_t t10;
  uint32_t t11;
  int64_t t12;
  uint32_t t13;
  int64_t t14;
  uint32_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
  uint64_t t50;
  uint64_t t51;
  uint64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  uint64_t t57;
  uint64_t t58;
  uint64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint64_t t63;
  uint64_t t64;
  uint64_t t65;
  uint64_t t66;
  uint64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint64_t t74;
  uint64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint64_t t83;
  uint64_t t84;
  uint64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  uint64_t t89;
  uint64_t t90;
  uint64_t t91;
  uint64_t t92;
  uint64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint64_t t97;
  uint64_t t98;
  uint64_t t99;
  uint64_t t100;
  uint64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint64_t t105;
  uint64_t t106;
  uint64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  uint64_t t111;
  uint64_t t112;
  uint64_t t113;
  uint8_t t114;
  int64_t t115;
  uint64_t t116;
  uint64_t t117;
  float t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  float t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  uint64_t t129;
  uint64_t t130;
  uint64_t t131;
  float t132;
  float t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  uint64_t t140;
  uint64_t t141;
  uint64_t t142;
  float t143;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  uint64_t v6;
  uint64_t v7;
  uint64_t v8;
  uint64_t v9;
  uint64_t v10;
  uint64_t v11;
  uint64_t v12;
  uint64_t v13;
  uint64_t v14;
  uint64_t v15;
  uint64_t v16;
  uint64_t v17;
  uint64_t v18;
  uint64_t v19;
  float v20;
  int64_t v21;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  v9 = p9;
  {
  t0 = ((int64_t)(v0));
  t1 = ((uint32_t)(((uint32_t)(v6))));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = gg_function_11(t0, t1, t2, base + 0);
  v10 = ((uint64_t)(((uint64_t)(t3))));
  t4 = ((int64_t)(v0));
  t5 = ((uint32_t)(((uint32_t)(v7))));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = gg_function_11(t4, t5, t6, base + 0);
  v11 = ((uint64_t)(((uint64_t)(t7))));
  t8 = ((int64_t)(v0));
  t9 = ((uint32_t)(((uint32_t)(v8))));
  t10 = ((int64_t)(((int64_t)UINT64_C(3))));
  t11 = gg_function_11(t8, t9, t10, base + 0);
  v12 = ((uint64_t)(((uint64_t)(t11))));
  t12 = ((int64_t)(v0));
  t13 = ((uint32_t)(((uint32_t)(v9))));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = gg_function_11(t12, t13, t14, base + 0);
  v13 = ((uint64_t)(((uint64_t)(t15))));
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  v14 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  }
  } else {
  {
  v14 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  }
  }
  t19 = ((int64_t)(v5));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  v15 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  }
  } else {
  {
  v15 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  }
  }
  t22 = ((uint64_t)(v6));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(5))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg167_dget(t23, t24, t25);
  t27 = ((uint64_t)(t26));
  t28 = ((uint64_t)(((uint64_t)((uint64_t)(t22) * (uint64_t)(t27)))));
  t29 = ((uint64_t)(v7));
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(5))));
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  t33 = fg167_dget(t30, t31, t32);
  t34 = ((uint64_t)(t33));
  t35 = ((uint64_t)(((uint64_t)((uint64_t)(t29) * (uint64_t)(t34)))));
  t36 = ((uint64_t)(((uint64_t)((uint64_t)(t28) + (uint64_t)(t35)))));
  t37 = ((uint64_t)(v8));
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(5))));
  t40 = ((int64_t)(((int64_t)UINT64_C(2))));
  t41 = fg167_dget(t38, t39, t40);
  t42 = ((uint64_t)(t41));
  t43 = ((uint64_t)(((uint64_t)((uint64_t)(t37) * (uint64_t)(t42)))));
  t44 = ((uint64_t)(((uint64_t)((uint64_t)(t36) + (uint64_t)(t43)))));
  t45 = ((uint64_t)(v9));
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(5))));
  t48 = ((int64_t)(((int64_t)UINT64_C(3))));
  t49 = fg167_dget(t46, t47, t48);
  t50 = ((uint64_t)(t49));
  t51 = ((uint64_t)(((uint64_t)((uint64_t)(t45) * (uint64_t)(t50)))));
  v16 = ((uint64_t)(((uint64_t)((uint64_t)(t44) + (uint64_t)(t51)))));
  t52 = ((uint64_t)(v10));
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(6))));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = fg167_dget(t53, t54, t55);
  t57 = ((uint64_t)(t56));
  t58 = ((uint64_t)(((uint64_t)((uint64_t)(t52) * (uint64_t)(t57)))));
  t59 = ((uint64_t)(v11));
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(6))));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t63 = fg167_dget(t60, t61, t62);
  t64 = ((uint64_t)(t63));
  t65 = ((uint64_t)(((uint64_t)((uint64_t)(t59) * (uint64_t)(t64)))));
  t66 = ((uint64_t)(((uint64_t)((uint64_t)(t58) + (uint64_t)(t65)))));
  t67 = ((uint64_t)(v12));
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(6))));
  t70 = ((int64_t)(((int64_t)UINT64_C(2))));
  t71 = fg167_dget(t68, t69, t70);
  t72 = ((uint64_t)(t71));
  t73 = ((uint64_t)(((uint64_t)((uint64_t)(t67) * (uint64_t)(t72)))));
  t74 = ((uint64_t)(((uint64_t)((uint64_t)(t66) + (uint64_t)(t73)))));
  t75 = ((uint64_t)(v13));
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(((int64_t)UINT64_C(6))));
  t78 = ((int64_t)(((int64_t)UINT64_C(3))));
  t79 = fg167_dget(t76, t77, t78);
  t80 = ((uint64_t)(t79));
  t81 = ((uint64_t)(((uint64_t)((uint64_t)(t75) * (uint64_t)(t80)))));
  v17 = ((uint64_t)(((uint64_t)((uint64_t)(t74) + (uint64_t)(t81)))));
  t82 = ((uint64_t)(v6));
  t83 = ((uint64_t)(v15));
  t84 = ((uint64_t)(((uint64_t)((uint64_t)(t82) * (uint64_t)(t83)))));
  t85 = ((uint64_t)(v7));
  t86 = ((int64_t)(v0));
  t87 = ((int64_t)(((int64_t)UINT64_C(4))));
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  t89 = fg167_dget(t86, t87, t88);
  t90 = ((uint64_t)(t89));
  t91 = ((uint64_t)(((uint64_t)((uint64_t)(t85) * (uint64_t)(t90)))));
  t92 = ((uint64_t)(((uint64_t)((uint64_t)(t84) + (uint64_t)(t91)))));
  t93 = ((uint64_t)(v8));
  t94 = ((int64_t)(v0));
  t95 = ((int64_t)(((int64_t)UINT64_C(4))));
  t96 = ((int64_t)(((int64_t)UINT64_C(2))));
  t97 = fg167_dget(t94, t95, t96);
  t98 = ((uint64_t)(t97));
  t99 = ((uint64_t)(((uint64_t)((uint64_t)(t93) * (uint64_t)(t98)))));
  t100 = ((uint64_t)(((uint64_t)((uint64_t)(t92) + (uint64_t)(t99)))));
  t101 = ((uint64_t)(v9));
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(((int64_t)UINT64_C(4))));
  t104 = ((int64_t)(((int64_t)UINT64_C(3))));
  t105 = fg167_dget(t102, t103, t104);
  t106 = ((uint64_t)(t105));
  t107 = ((uint64_t)(((uint64_t)((uint64_t)(t101) * (uint64_t)(t106)))));
  v18 = ((uint64_t)(((uint64_t)((uint64_t)(t100) + (uint64_t)(t107)))));
  t108 = ((int64_t)(v0));
  t109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = fg167_dget(t108, t109, t110);
  v19 = ((uint64_t)(t111));
  v20 = ((float)(gg_double_bits(UINT64_C(0))));
  t112 = ((uint64_t)(v19));
  t113 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t114 = ((t112) != (t113));
  if (t114) {
  {
  t115 = ((int64_t)(v3));
  t116 = ((uint64_t)(v19));
  t117 = ((uint64_t)(v16));
  t118 = fg167_load(t115, t116, t117);
  v20 = ((float)(t118));
  }
  }
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t119 = ((int64_t)(v21));
  t120 = ((int64_t)(v2));
  if (!(((t119) < (t120)))) goto label2;
  {
  t121 = ((int64_t)(v1));
  t122 = ((float)(v20));
  t123 = ((int64_t)(v4));
  t124 = ((int64_t)(v0));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((int64_t)(((int64_t)UINT64_C(1))));
  t127 = ((int64_t)(v21));
  t128 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t127)))));
  t129 = fg167_dget(t124, t125, t128);
  t130 = ((uint64_t)(t129));
  t131 = ((uint64_t)(v17));
  t132 = fg167_load(t123, t130, t131);
  t133 = ((float)(t132));
  v20 = gg_function_12(t121, t122, t133, base + 0);
  t134 = v21;
  t135 = ((int64_t)(((int64_t)UINT64_C(1))));
  v21 = ((int64_t)((uint64_t)(t134) + (uint64_t)(t135)));
  }
  goto label1;
label2:;
  t136 = ((int64_t)(v5));
  t137 = ((int64_t)(v0));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((int64_t)(((int64_t)UINT64_C(9))));
  t140 = fg167_dget(t137, t138, t139);
  t141 = ((uint64_t)(t140));
  t142 = ((uint64_t)(v18));
  t143 = ((float)(v20));
  fg167_store(t136, t141, t142, t143);
  }
gg_done:;
}
static void gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, uint64_t p7, uint64_t p8, uint64_t p9, uint64_t p10, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint8_t t25;
  uint64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
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
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint64_t t48;
  uint64_t t49;
  float t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  uint64_t t56;
  uint64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint64_t t61;
  uint64_t t62;
  uint64_t t63;
  uint64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint64_t t68;
  uint64_t t69;
  uint64_t t70;
  uint64_t t71;
  uint64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint64_t t76;
  uint64_t t77;
  uint64_t t78;
  uint64_t t79;
  uint64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint64_t t86;
  uint64_t t87;
  float t88;
  float t89;
  uint64_t t90;
  uint64_t t91;
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
  uint64_t t105;
  uint64_t t106;
  uint64_t t107;
  uint64_t t108;
  uint64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint64_t t114;
  uint64_t t115;
  uint64_t t116;
  uint64_t t117;
  uint64_t t118;
  float t119;
  int64_t t120;
  int64_t t121;
  uint8_t t122;
  int64_t t123;
  uint32_t t124;
  int64_t t125;
  uint32_t t126;
  uint64_t t127;
  uint64_t t128;
  uint64_t t129;
  uint64_t t130;
  uint64_t t131;
  uint64_t t132;
  uint8_t t133;
  uint8_t t134;
  uint64_t t135;
  uint64_t t136;
  uint8_t t137;
  uint8_t t138;
  uint64_t t139;
  uint64_t t140;
  uint8_t t141;
  uint8_t t142;
  uint64_t t143;
  uint64_t t144;
  uint8_t t145;
  uint64_t t146;
  uint64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  uint64_t t154;
  uint64_t t155;
  uint64_t t156;
  uint64_t t157;
  uint64_t t158;
  uint64_t t159;
  uint64_t t160;
  uint64_t t161;
  uint8_t t162;
  uint64_t t163;
  uint64_t t164;
  int64_t t165;
  uint32_t t166;
  int64_t t167;
  uint32_t t168;
  uint64_t t169;
  uint64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  uint32_t t174;
  uint64_t t175;
  uint64_t t176;
  int64_t t177;
  uint32_t t178;
  int64_t t179;
  uint32_t t180;
  uint64_t t181;
  uint64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  uint32_t t186;
  uint64_t t187;
  uint64_t t188;
  int64_t t189;
  uint32_t t190;
  int64_t t191;
  uint32_t t192;
  uint64_t t193;
  uint64_t t194;
  uint64_t t195;
  uint64_t t196;
  uint64_t t197;
  uint64_t t198;
  uint8_t t199;
  uint8_t t200;
  uint64_t t201;
  uint64_t t202;
  uint8_t t203;
  uint8_t t204;
  uint64_t t205;
  uint64_t t206;
  uint8_t t207;
  uint8_t t208;
  uint64_t t209;
  uint64_t t210;
  uint8_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  uint64_t t218;
  uint64_t t219;
  uint64_t t220;
  uint64_t t221;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  uint64_t v7;
  uint64_t v8;
  uint64_t v9;
  uint64_t v10;
  uint64_t v11;
  uint64_t v12;
  uint64_t v13;
  uint64_t v14;
  uint64_t v15;
  uint64_t v16;
  float v17;
  uint64_t v18;
  uint64_t v19;
  uint64_t v20;
  uint64_t v21;
  uint64_t v22;
  uint64_t v23;
  uint64_t v24;
  uint64_t v25;
  uint64_t v26;
  uint64_t v27;
  uint64_t v28;
  uint64_t v29;
  uint64_t v30;
  uint64_t v31;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  v9 = p9;
  v10 = p10;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg167_dget(t0, t1, t2);
  v11 = ((uint64_t)(t3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = fg167_dget(t4, t5, t6);
  v12 = ((uint64_t)(t7));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = fg167_dget(t8, t9, t10);
  v13 = ((uint64_t)(t11));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = fg167_dget(t12, t13, t14);
  v14 = ((uint64_t)(t15));
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t19 = ((uint64_t)(v9));
  t20 = ((uint64_t)(v13));
  v15 = ((uint64_t)(((uint64_t)gg_umod(t19, t20))));
  t21 = ((uint64_t)(v9));
  t22 = ((uint64_t)(v13));
  v16 = ((uint64_t)(((uint64_t)gg_udiv(t21, t22))));
  t23 = ((uint64_t)(v7));
  t24 = ((uint64_t)(v11));
  t25 = ((t23) >= (t24));
  if (t25) {
  {
  goto gg_done;
  }
  }
  v17 = ((float)(gg_double_bits(UINT64_C(0))));
  fg167_sync();
  v18 = ((uint64_t)(v16));
label1:;
  t26 = ((uint64_t)(v18));
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(((int64_t)UINT64_C(3))));
  t30 = fg167_dget(t27, t28, t29);
  t31 = ((uint64_t)(t30));
  if (!(((t26) < (t31)))) goto label2;
  {
  v19 = ((uint64_t)(v15));
label4:;
  t32 = ((uint64_t)(v19));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t36 = fg167_dget(t33, t34, t35);
  t37 = ((uint64_t)(t36));
  if (!(((t32) < (t37)))) goto label5;
  {
  v20 = ((uint64_t)(v8));
label7:;
  t38 = ((uint64_t)(v20));
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(2))));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  t42 = fg167_dget(t39, t40, t41);
  t43 = ((uint64_t)(t42));
  if (!(((t38) < (t43)))) goto label8;
  {
  v21 = ((uint64_t)(v7));
label10:;
  t44 = ((uint64_t)(v21));
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(2))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = fg167_dget(t45, t46, t47);
  t49 = ((uint64_t)(t48));
  if (!(((t44) < (t49)))) goto label11;
  {
  t50 = v17;
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = fg167_dget(t52, t53, t54);
  t56 = ((uint64_t)(t55));
  t57 = ((uint64_t)(v21));
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(((int64_t)UINT64_C(5))));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = fg167_dget(t58, t59, t60);
  t62 = ((uint64_t)(t61));
  t63 = ((uint64_t)(((uint64_t)((uint64_t)(t57) * (uint64_t)(t62)))));
  t64 = ((uint64_t)(v20));
  t65 = ((int64_t)(v0));
  t66 = ((int64_t)(((int64_t)UINT64_C(5))));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  t68 = fg167_dget(t65, t66, t67);
  t69 = ((uint64_t)(t68));
  t70 = ((uint64_t)(((uint64_t)((uint64_t)(t64) * (uint64_t)(t69)))));
  t71 = ((uint64_t)(((uint64_t)((uint64_t)(t63) + (uint64_t)(t70)))));
  t72 = ((uint64_t)(v19));
  t73 = ((int64_t)(v0));
  t74 = ((int64_t)(((int64_t)UINT64_C(5))));
  t75 = ((int64_t)(((int64_t)UINT64_C(2))));
  t76 = fg167_dget(t73, t74, t75);
  t77 = ((uint64_t)(t76));
  t78 = ((uint64_t)(((uint64_t)((uint64_t)(t72) * (uint64_t)(t77)))));
  t79 = ((uint64_t)(((uint64_t)((uint64_t)(t71) + (uint64_t)(t78)))));
  t80 = ((uint64_t)(v18));
  t81 = ((int64_t)(v0));
  t82 = ((int64_t)(((int64_t)UINT64_C(5))));
  t83 = ((int64_t)(((int64_t)UINT64_C(3))));
  t84 = fg167_dget(t81, t82, t83);
  t85 = ((uint64_t)(t84));
  t86 = ((uint64_t)(((uint64_t)((uint64_t)(t80) * (uint64_t)(t85)))));
  t87 = ((uint64_t)(((uint64_t)((uint64_t)(t79) + (uint64_t)(t86)))));
  t88 = fg167_load(t51, t56, t87);
  t89 = ((float)(t88));
  v17 = ((t50) + (t89));
  t90 = v21;
  t91 = ((uint64_t)(v11));
  v21 = ((uint64_t)((uint64_t)(t90) + (uint64_t)(t91)));
  }
  goto label10;
label11:;
  t92 = v20;
  t93 = ((uint64_t)(v12));
  v20 = ((uint64_t)((uint64_t)(t92) + (uint64_t)(t93)));
  }
  goto label7;
label8:;
  t94 = v19;
  t95 = ((uint64_t)(v13));
  v19 = ((uint64_t)((uint64_t)(t94) + (uint64_t)(t95)));
  }
  goto label4;
label5:;
  t96 = v18;
  t97 = ((uint64_t)(v14));
  v18 = ((uint64_t)((uint64_t)(t96) + (uint64_t)(t97)));
  }
  goto label1;
label2:;
  t98 = ((uint64_t)(v16));
  t99 = ((uint64_t)(v13));
  t100 = ((uint64_t)(((uint64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  t101 = ((uint64_t)(v15));
  t102 = ((uint64_t)(((uint64_t)((uint64_t)(t100) + (uint64_t)(t101)))));
  t103 = ((uint64_t)(v12));
  t104 = ((uint64_t)(((uint64_t)((uint64_t)(t102) * (uint64_t)(t103)))));
  t105 = ((uint64_t)(v8));
  t106 = ((uint64_t)(((uint64_t)((uint64_t)(t104) + (uint64_t)(t105)))));
  t107 = ((uint64_t)(v11));
  t108 = ((uint64_t)(((uint64_t)((uint64_t)(t106) * (uint64_t)(t107)))));
  t109 = ((uint64_t)(v7));
  v22 = ((uint64_t)(((uint64_t)((uint64_t)(t108) + (uint64_t)(t109)))));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((int64_t)(v0));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = ((int64_t)(((int64_t)UINT64_C(9))));
  t114 = fg167_dget(t111, t112, t113);
  t115 = ((uint64_t)(t114));
  t116 = ((uint64_t)(v22));
  t117 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t118 = ((uint64_t)(((uint64_t)((uint64_t)(t116) * (uint64_t)(t117)))));
  t119 = ((float)(v17));
  fg167_store(t110, t115, t118, t119);
  goto gg_done;
  }
  }
  t120 = ((int64_t)(v3));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = ((t120) == (t121));
  if (t122) {
  {
  fg167_lc();
  t123 = ((int64_t)(v0));
  t124 = ((uint32_t)(((uint32_t)(v9))));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = gg_function_7(t123, t124, t125, base + 0);
  v23 = ((uint64_t)(((uint64_t)(t126))));
  t127 = ((uint64_t)(v9));
  t128 = ((uint64_t)(v23));
  t129 = ((uint64_t)(v14));
  t130 = ((uint64_t)(((uint64_t)((uint64_t)(t128) * (uint64_t)(t129)))));
  v24 = ((uint64_t)(((uint64_t)((uint64_t)(t127) - (uint64_t)(t130)))));
  t131 = ((uint64_t)(v7));
  t132 = ((uint64_t)(v11));
  t133 = ((t131) >= (t132));
  t134 = t133;
  if (!t134) {
  t135 = ((uint64_t)(v8));
  t136 = ((uint64_t)(v12));
  t134 = ((t135) >= (t136));
  }
  t137 = t134;
  t138 = t137;
  if (!t138) {
  t139 = ((uint64_t)(v23));
  t140 = ((uint64_t)(v13));
  t138 = ((t139) >= (t140));
  }
  t141 = t138;
  t142 = t141;
  if (!t142) {
  t143 = ((uint64_t)(v24));
  t144 = ((uint64_t)(v14));
  t142 = ((t143) >= (t144));
  }
  t145 = t142;
  if (t145) {
  {
  goto gg_done;
  }
  }
  fg167_sync();
  v25 = ((uint64_t)(v7));
label13:;
  t146 = ((uint64_t)(v25));
  t147 = ((uint64_t)(v11));
  if (!(((t146) < (t147)))) goto label14;
  {
  t148 = ((int64_t)(v0));
  t149 = ((int64_t)(v1));
  t150 = ((int64_t)(v2));
  t151 = ((int64_t)(v4));
  t152 = ((int64_t)(v5));
  t153 = ((int64_t)(v6));
  t154 = ((uint64_t)(v25));
  t155 = ((uint64_t)(v8));
  t156 = ((uint64_t)(v23));
  t157 = ((uint64_t)(v24));
  gg_function_10(t148, t149, t150, t151, t152, t153, t154, t155, t156, t157, base + 0);
  t158 = ((uint64_t)(v11));
  t159 = ((uint64_t)(v25));
  t160 = ((uint64_t)(((uint64_t)((uint64_t)(t158) - (uint64_t)(t159)))));
  t161 = ((uint64_t)(v10));
  t162 = ((t160) <= (t161));
  if (t162) {
  {
  goto label14;
  }
  }
  t163 = v25;
  t164 = ((uint64_t)(v10));
  v25 = ((uint64_t)((uint64_t)(t163) + (uint64_t)(t164)));
  }
  goto label13;
label14:;
  goto gg_done;
  }
  }
  t165 = ((int64_t)(v0));
  t166 = ((uint32_t)(((uint32_t)(v7))));
  t167 = ((int64_t)(((int64_t)UINT64_C(7))));
  t168 = gg_function_7(t165, t166, t167, base + 0);
  v26 = ((uint64_t)(((uint64_t)(t168))));
  t169 = ((uint64_t)(v7));
  t170 = ((uint64_t)(v26));
  t171 = ((int64_t)(v0));
  t172 = ((int64_t)(((int64_t)UINT64_C(7))));
  t173 = ((int64_t)(((int64_t)UINT64_C(2))));
  t174 = fg167_dmagic(t171, t172, t173);
  t175 = ((uint64_t)(((uint64_t)(t174))));
  t176 = ((uint64_t)(((uint64_t)((uint64_t)(t170) * (uint64_t)(t175)))));
  v27 = ((uint64_t)(((uint64_t)((uint64_t)(t169) - (uint64_t)(t176)))));
  t177 = ((int64_t)(v0));
  t178 = ((uint32_t)(((uint32_t)(v27))));
  t179 = ((int64_t)(((int64_t)UINT64_C(6))));
  t180 = gg_function_7(t177, t178, t179, base + 0);
  v28 = ((uint64_t)(((uint64_t)(t180))));
  t181 = ((uint64_t)(v27));
  t182 = ((uint64_t)(v28));
  t183 = ((int64_t)(v0));
  t184 = ((int64_t)(((int64_t)UINT64_C(6))));
  t185 = ((int64_t)(((int64_t)UINT64_C(2))));
  t186 = fg167_dmagic(t183, t184, t185);
  t187 = ((uint64_t)(((uint64_t)(t186))));
  t188 = ((uint64_t)(((uint64_t)((uint64_t)(t182) * (uint64_t)(t187)))));
  v29 = ((uint64_t)(((uint64_t)((uint64_t)(t181) - (uint64_t)(t188)))));
  t189 = ((int64_t)(v0));
  t190 = ((uint32_t)(((uint32_t)(v29))));
  t191 = ((int64_t)(((int64_t)UINT64_C(5))));
  t192 = gg_function_7(t189, t190, t191, base + 0);
  v30 = ((uint64_t)(((uint64_t)(t192))));
  t193 = ((uint64_t)(v29));
  t194 = ((uint64_t)(v30));
  t195 = ((uint64_t)(v11));
  t196 = ((uint64_t)(((uint64_t)((uint64_t)(t194) * (uint64_t)(t195)))));
  v31 = ((uint64_t)(((uint64_t)((uint64_t)(t193) - (uint64_t)(t196)))));
  t197 = ((uint64_t)(v31));
  t198 = ((uint64_t)(v11));
  t199 = ((t197) >= (t198));
  t200 = t199;
  if (!t200) {
  t201 = ((uint64_t)(v30));
  t202 = ((uint64_t)(v12));
  t200 = ((t201) >= (t202));
  }
  t203 = t200;
  t204 = t203;
  if (!t204) {
  t205 = ((uint64_t)(v28));
  t206 = ((uint64_t)(v13));
  t204 = ((t205) >= (t206));
  }
  t207 = t204;
  t208 = t207;
  if (!t208) {
  t209 = ((uint64_t)(v26));
  t210 = ((uint64_t)(v14));
  t208 = ((t209) >= (t210));
  }
  t211 = t208;
  if (t211) {
  {
  goto gg_done;
  }
  }
  fg167_sync();
  t212 = ((int64_t)(v0));
  t213 = ((int64_t)(v1));
  t214 = ((int64_t)(v2));
  t215 = ((int64_t)(v4));
  t216 = ((int64_t)(v5));
  t217 = ((int64_t)(v6));
  t218 = ((uint64_t)(v31));
  t219 = ((uint64_t)(v30));
  t220 = ((uint64_t)(v28));
  t221 = ((uint64_t)(v26));
  gg_function_10(t212, t213, t214, t215, t216, t217, t218, t219, t220, t221, base + 0);
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  gg_header t11;
  int64_t t12;
  uint32_t t13;
  int64_t t14;
  uint32_t t15;
  int64_t t16;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t9 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t10 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  gg_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, base + 0);
  t11.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t11.len = 0;
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(1))))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = gg_function_7(t12, t13, t14, base + 1);
  gg_unsigned(gg_ref(&t11, gg_get_stack(base + 0)), t15);
  gg_append(gg_ref(&t11, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t11).base, (t11).len));
  }
gg_done:;
  for (t16 = 0; t16 < 1; t16++) gg_stacks[base + t16].top = 0;
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
