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
static uint8_t gg_eq_1(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, uint64_t value);
static uint8_t gg_eq_2(int64_t a, int64_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, int64_t value);
static uint8_t gg_eq_3(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint8_t value);
static uint8_t gg_eq_4(int32_t a, int32_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, int32_t value);
static uint8_t gg_eq_5(float a, float b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, float value);
static uint8_t gg_eq_6(double a, double b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, double value);
static uint8_t gg_eq_7(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, uint32_t value);
static uint8_t gg_eq_9(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, int32_t value) {
gg_integer(out,value);
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
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_9(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, uint8_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t p26, int64_t base);
static void gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static float gg_function_5(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static float gg_function_6(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static float gg_function_10(int32_t p0, int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, float p10, float p11, float p12, float p13, int64_t base);
static float gg_function_15(float p0, int64_t base);
static float gg_function_15(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  int32_t t3;
  int32_t t4;
  float t5;
  float t6;
  float t7;
  float t8;
  int32_t t9;
  int32_t t10;
  float t11;
  float t12;
  float t13;
  float t14;
  int32_t t15;
  int32_t t16;
  float t17;
  float t18;
  float t19;
  float t20;
  int32_t t21;
  int32_t t22;
  float t23;
  float t24;
  float t25;
  float t26;
  int32_t t27;
  int32_t t28;
  float t29;
  float t30;
  float v0;
  float v1;
  v0 = p0;
  {
  v1 = ((float)(v0));
  t1 = ((float)(v1));
  t2 = ((float)(v1));
  t3 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  t4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t5 = fg185q_shfl_xor(t2, t3, t4);
  t6 = ((float)(t5));
  v1 = ((float)(((t1) + (t6))));
  t7 = ((float)(v1));
  t8 = ((float)(v1));
  t9 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  t10 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t11 = fg185q_shfl_xor(t8, t9, t10);
  t12 = ((float)(t11));
  v1 = ((float)(((t7) + (t12))));
  t13 = ((float)(v1));
  t14 = ((float)(v1));
  t15 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(4))))));
  t16 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t17 = fg185q_shfl_xor(t14, t15, t16);
  t18 = ((float)(t17));
  v1 = ((float)(((t13) + (t18))));
  t19 = ((float)(v1));
  t20 = ((float)(v1));
  t21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(2))))));
  t22 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t23 = fg185q_shfl_xor(t20, t21, t22);
  t24 = ((float)(t23));
  v1 = ((float)(((t19) + (t24))));
  t25 = ((float)(v1));
  t26 = ((float)(v1));
  t27 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t28 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t29 = fg185q_shfl_xor(t26, t27, t28);
  t30 = ((float)(t29));
  v1 = ((float)(((t25) + (t30))));
  t0 = ((float)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_10(int32_t p0, int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, float p10, float p11, float p12, float p13, int64_t base) {
  float t0;
  int32_t t1;
  int64_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  int64_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  int32_t t19;
  int32_t t20;
  int32_t t21;
  int32_t t22;
  int32_t t23;
  int32_t t24;
  float t25;
  float t26;
  int32_t t27;
  int32_t t28;
  float t29;
  float t30;
  float t31;
  float t32;
  int32_t t33;
  int32_t t34;
  float t35;
  float t36;
  int32_t t37;
  int64_t t38;
  int32_t t39;
  int32_t t40;
  int32_t t41;
  int64_t t42;
  int32_t t43;
  int32_t t44;
  int32_t t45;
  int32_t t46;
  int32_t t47;
  int32_t t48;
  int32_t t49;
  int32_t t50;
  int32_t t51;
  int32_t t52;
  int32_t t53;
  int32_t t54;
  int32_t t55;
  int32_t t56;
  int32_t t57;
  int32_t t58;
  int32_t t59;
  int32_t t60;
  float t61;
  float t62;
  int32_t t63;
  int32_t t64;
  float t65;
  float t66;
  float t67;
  float t68;
  int32_t t69;
  int32_t t70;
  float t71;
  float t72;
  float t73;
  float t74;
  float t75;
  float t76;
  float t77;
  float t78;
  int32_t v0;
  int32_t v1;
  int32_t v2;
  int32_t v3;
  int32_t v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  int32_t v8;
  int32_t v9;
  float v10;
  float v11;
  float v12;
  float v13;
  float v14;
  float v15;
  int32_t v16;
  int32_t v17;
  int32_t v18;
  int32_t v19;
  int32_t v20;
  int32_t v21;
  int32_t v22;
  int32_t v23;
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
  v11 = p11;
  v12 = p12;
  v13 = p13;
  {
  v14 = ((float)(gg_double_bits(UINT64_C(0))));
  v15 = ((float)(gg_double_bits(UINT64_C(0))));
  t1 = ((int32_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int32_t)(((t1) >> ((uint64_t)(t2) & 31))));
  t4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(252645135))))));
  v16 = ((int32_t)(((t3) & (t4))));
  t5 = ((int32_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int32_t)(((t5) >> ((uint64_t)(t6) & 31))));
  t8 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(252645135))))));
  v17 = ((int32_t)(((t7) & (t8))));
  t9 = ((int32_t)(v17));
  t10 = ((int32_t)(v3));
  t11 = ((int32_t)(v16));
  t12 = ((int32_t)(v2));
  t13 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t14 = fg185q_dp4a(t11, t12, t13);
  t15 = ((int32_t)(t14));
  t16 = fg185q_dp4a(t9, t10, t15);
  v18 = ((int32_t)(t16));
  t17 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16843009))))));
  t18 = ((int32_t)(v3));
  t19 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16843009))))));
  t20 = ((int32_t)(v2));
  t21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t22 = fg185q_dp4a(t19, t20, t21);
  t23 = ((int32_t)(t22));
  t24 = fg185q_dp4a(t17, t18, t23);
  v19 = ((int32_t)(t24));
  t25 = ((float)(v14));
  t26 = ((float)(v12));
  t27 = ((int32_t)(v18));
  t28 = ((int32_t)(v6));
  t29 = ((float)(((float)(((int32_t)((uint64_t)(t27) * (uint64_t)(t28)))))));
  t30 = ((float)(((t26) * (t29))));
  v14 = ((float)(((t25) + (t30))));
  t31 = ((float)(v15));
  t32 = ((float)(v12));
  t33 = ((int32_t)(v19));
  t34 = ((int32_t)(v8));
  t35 = ((float)(((float)(((int32_t)((uint64_t)(t33) * (uint64_t)(t34)))))));
  t36 = ((float)(((t32) * (t35))));
  v15 = ((float)(((t31) + (t36))));
  t37 = ((int32_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(4))));
  t39 = ((int32_t)(((t37) >> ((uint64_t)(t38) & 31))));
  t40 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(252645135))))));
  v20 = ((int32_t)(((t39) & (t40))));
  t41 = ((int32_t)(v1));
  t42 = ((int64_t)(((int64_t)UINT64_C(4))));
  t43 = ((int32_t)(((t41) >> ((uint64_t)(t42) & 31))));
  t44 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(252645135))))));
  v21 = ((int32_t)(((t43) & (t44))));
  t45 = ((int32_t)(v21));
  t46 = ((int32_t)(v5));
  t47 = ((int32_t)(v20));
  t48 = ((int32_t)(v4));
  t49 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t50 = fg185q_dp4a(t47, t48, t49);
  t51 = ((int32_t)(t50));
  t52 = fg185q_dp4a(t45, t46, t51);
  v22 = ((int32_t)(t52));
  t53 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16843009))))));
  t54 = ((int32_t)(v5));
  t55 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16843009))))));
  t56 = ((int32_t)(v4));
  t57 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t58 = fg185q_dp4a(t55, t56, t57);
  t59 = ((int32_t)(t58));
  t60 = fg185q_dp4a(t53, t54, t59);
  v23 = ((int32_t)(t60));
  t61 = ((float)(v14));
  t62 = ((float)(v13));
  t63 = ((int32_t)(v22));
  t64 = ((int32_t)(v7));
  t65 = ((float)(((float)(((int32_t)((uint64_t)(t63) * (uint64_t)(t64)))))));
  t66 = ((float)(((t62) * (t65))));
  v14 = ((float)(((t61) + (t66))));
  t67 = ((float)(v15));
  t68 = ((float)(v13));
  t69 = ((int32_t)(v23));
  t70 = ((int32_t)(v9));
  t71 = ((float)(((float)(((int32_t)((uint64_t)(t69) * (uint64_t)(t70)))))));
  t72 = ((float)(((t68) * (t71))));
  v15 = ((float)(((t67) + (t72))));
  t73 = ((float)(v10));
  t74 = ((float)(v14));
  t75 = ((float)(((t73) * (t74))));
  t76 = ((float)(v11));
  t77 = ((float)(v15));
  t78 = ((float)(((t76) * (t77))));
  t0 = ((float)(((t75) - (t78))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_6(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  float t0;
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
  uint64_t t25;
  uint64_t t26;
  int32_t t27;
  uint64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  int32_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  uint64_t t44;
  uint32_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  uint64_t t53;
  uint32_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  uint64_t t62;
  uint32_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  uint32_t t67;
  uint32_t t68;
  uint32_t t69;
  uint32_t t70;
  uint32_t t71;
  uint32_t t72;
  uint32_t t73;
  uint32_t t74;
  uint32_t t75;
  uint32_t t76;
  uint32_t t77;
  uint32_t t78;
  int64_t t79;
  uint32_t t80;
  uint32_t t81;
  uint32_t t82;
  uint32_t t83;
  uint32_t t84;
  uint32_t t85;
  uint32_t t86;
  uint32_t t87;
  uint32_t t88;
  uint32_t t89;
  uint32_t t90;
  int64_t t91;
  uint32_t t92;
  uint32_t t93;
  uint32_t t94;
  uint32_t t95;
  uint32_t t96;
  uint32_t t97;
  int64_t t98;
  uint32_t t99;
  uint32_t t100;
  uint32_t t101;
  uint32_t t102;
  uint32_t t103;
  uint32_t t104;
  uint32_t t105;
  int64_t t106;
  uint32_t t107;
  uint32_t t108;
  uint32_t t109;
  uint32_t t110;
  uint32_t t111;
  int64_t t112;
  uint32_t t113;
  uint32_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint64_t t121;
  uint64_t t122;
  float t123;
  uint64_t t124;
  int64_t t125;
  int64_t t126;
  uint64_t t127;
  float t128;
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
  uint64_t t139;
  uint64_t t140;
  int32_t t141;
  uint64_t t142;
  int64_t t143;
  int64_t t144;
  uint64_t t145;
  int32_t t146;
  int64_t t147;
  int64_t t148;
  uint64_t t149;
  uint64_t t150;
  int32_t t151;
  uint64_t t152;
  int64_t t153;
  int64_t t154;
  uint64_t t155;
  int32_t t156;
  uint64_t t157;
  uint64_t t158;
  float t159;
  uint64_t t160;
  uint64_t t161;
  float t162;
  int32_t t163;
  int32_t t164;
  int32_t t165;
  int32_t t166;
  int32_t t167;
  int32_t t168;
  int32_t t169;
  int32_t t170;
  int32_t t171;
  int32_t t172;
  float t173;
  float t174;
  float t175;
  float t176;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int32_t v10;
  int32_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  uint32_t v15;
  uint32_t v16;
  uint32_t v17;
  uint32_t v18;
  uint32_t v19;
  uint32_t v20;
  int32_t v21;
  int32_t v22;
  int32_t v23;
  int32_t v24;
  int64_t v25;
  float v26;
  float v27;
  int64_t v28;
  int32_t v29;
  int32_t v30;
  int64_t v31;
  int32_t v32;
  int32_t v33;
  float v34;
  float v35;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(v4));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(144))))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t5 = ((int64_t)(((int64_t)UINT64_C(2))));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(((int64_t)gg_div(t6, t7))));
  t9 = ((int64_t)(((int64_t)UINT64_C(4))));
  t10 = ((int64_t)(((int64_t)gg_div(t8, t9))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t10)))));
  t11 = ((int64_t)(v7));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)UINT64_C(16))));
  t15 = ((int64_t)(v8));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)UINT64_C(2))));
  t21 = ((int64_t)(((int64_t)gg_div(t19, t20))));
  t22 = ((int64_t)(((int64_t)UINT64_C(4))));
  t23 = ((int64_t)(((int64_t)gg_mod(t21, t22))));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t23)))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t24)))));
  t25 = ((uint64_t)(v0));
  t26 = ((uint64_t)(((uint64_t)(v9))));
  t27 = fg185q_ld_i32(t25, t26);
  v10 = ((int32_t)(t27));
  t28 = ((uint64_t)(v0));
  t29 = ((int64_t)(v9));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))))));
  t32 = fg185q_ld_i32(t28, t31);
  v11 = ((int32_t)(t32));
  t33 = ((int64_t)(v8));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  v12 = ((int64_t)(((int64_t)gg_div(t33, t34))));
  t35 = ((int64_t)(v12));
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)(((t35) & (t36))));
  t37 = ((int64_t)(v7));
  t38 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t39 = ((uint64_t)(v0));
  t40 = ((int64_t)(v14));
  t41 = ((int64_t)(((int64_t)UINT64_C(2))));
  t42 = ((int64_t)(v13));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t44 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))))));
  t45 = fg185q_ld_u16(t39, t44);
  v15 = ((uint32_t)(t45));
  t46 = ((uint64_t)(v0));
  t47 = ((int64_t)(v14));
  t48 = ((int64_t)(((int64_t)UINT64_C(2))));
  t49 = ((int64_t)(v13));
  t50 = ((int64_t)(((int64_t)UINT64_C(2))));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t50)))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t51)))));
  t53 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t52)))))));
  t54 = fg185q_ld_u16(t46, t53);
  v16 = ((uint32_t)(t54));
  t55 = ((uint64_t)(v0));
  t56 = ((int64_t)(v14));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  t58 = ((int64_t)(v13));
  t59 = ((int64_t)(((int64_t)UINT64_C(4))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t59)))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t60)))));
  t62 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t61)))))));
  t63 = fg185q_ld_u16(t55, t62);
  v17 = ((uint32_t)(t63));
  v18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
  t64 = ((int64_t)(v12));
  t65 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t66 = ((t64) >= (t65));
  if (t66) {
  {
  v18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  }
  }
  t67 = ((uint32_t)(v15));
  t68 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16191))))));
  t69 = ((uint32_t)(((t67) & (t68))));
  t70 = v18;
  t71 = ((uint32_t)((~(t70))));
  t72 = ((uint32_t)(((t69) & (t71))));
  t73 = ((uint32_t)(v17));
  t74 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(3855))))));
  t75 = ((uint32_t)(((t73) & (t74))));
  t76 = ((uint32_t)(v15));
  t77 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(49344))))));
  t78 = ((uint32_t)(((t76) & (t77))));
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((uint32_t)(((t78) >> ((uint64_t)(t79) & 31))));
  t81 = ((uint32_t)(((t75) | (t80))));
  t82 = ((uint32_t)(v18));
  t83 = ((uint32_t)(((t81) & (t82))));
  v19 = ((uint32_t)(((t72) | (t83))));
  t84 = ((uint32_t)(v16));
  t85 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16191))))));
  t86 = ((uint32_t)(((t84) & (t85))));
  t87 = v18;
  t88 = ((uint32_t)((~(t87))));
  t89 = ((uint32_t)(((t86) & (t88))));
  t90 = ((uint32_t)(v17));
  t91 = ((int64_t)(((int64_t)UINT64_C(4))));
  t92 = ((uint32_t)(((t90) >> ((uint64_t)(t91) & 31))));
  t93 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(3855))))));
  t94 = ((uint32_t)(((t92) & (t93))));
  t95 = ((uint32_t)(v16));
  t96 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(49344))))));
  t97 = ((uint32_t)(((t95) & (t96))));
  t98 = ((int64_t)(((int64_t)UINT64_C(2))));
  t99 = ((uint32_t)(((t97) >> ((uint64_t)(t98) & 31))));
  t100 = ((uint32_t)(((t94) | (t99))));
  t101 = ((uint32_t)(v18));
  t102 = ((uint32_t)(((t100) & (t101))));
  v20 = ((uint32_t)(((t89) | (t102))));
  t103 = ((uint32_t)(v19));
  t104 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v21 = ((int32_t)(((int32_t)(((t103) & (t104))))));
  t105 = ((uint32_t)(v19));
  t106 = ((int64_t)(((int64_t)UINT64_C(8))));
  t107 = ((uint32_t)(((t105) >> ((uint64_t)(t106) & 31))));
  t108 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v22 = ((int32_t)(((int32_t)(((t107) & (t108))))));
  t109 = ((uint32_t)(v20));
  t110 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v23 = ((int32_t)(((int32_t)(((t109) & (t110))))));
  t111 = ((uint32_t)(v20));
  t112 = ((int64_t)(((int64_t)UINT64_C(8))));
  t113 = ((uint32_t)(((t111) >> ((uint64_t)(t112) & 31))));
  t114 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v24 = ((int32_t)(((int32_t)(((t113) & (t114))))));
  t115 = ((int64_t)(v2));
  t116 = ((int64_t)(v5));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = ((int64_t)(v8));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) + (uint64_t)(t118)))));
  t120 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(36))))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t119) * (uint64_t)(t120)))));
  t121 = ((uint64_t)(v1));
  t122 = ((uint64_t)(((uint64_t)(v25))));
  t123 = fg185q_ld_h2(t121, t122);
  v26 = ((float)(t123));
  t124 = ((uint64_t)(v1));
  t125 = ((int64_t)(v25));
  t126 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(36))))));
  t127 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t125) + (uint64_t)(t126)))))));
  t128 = fg185q_ld_h2(t124, t127);
  v27 = ((float)(t128));
  t129 = ((int64_t)(v25));
  t130 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t129) + (uint64_t)(t130)))));
  t132 = ((int64_t)(((int64_t)UINT64_C(4))));
  t133 = ((int64_t)(v6));
  t134 = ((int64_t)(((int64_t)UINT64_C(2))));
  t135 = ((int64_t)(((int64_t)gg_div(t133, t134))));
  t136 = ((int64_t)(((int64_t)UINT64_C(4))));
  t137 = ((int64_t)(((int64_t)gg_mod(t135, t136))));
  t138 = ((int64_t)(((int64_t)((uint64_t)(t132) * (uint64_t)(t137)))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t131) + (uint64_t)(t138)))));
  t139 = ((uint64_t)(v1));
  t140 = ((uint64_t)(((uint64_t)(v28))));
  t141 = fg185q_ld_i32(t139, t140);
  v29 = ((int32_t)(t141));
  t142 = ((uint64_t)(v1));
  t143 = ((int64_t)(v28));
  t144 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t145 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t143) + (uint64_t)(t144)))))));
  t146 = fg185q_ld_i32(t142, t145);
  v30 = ((int32_t)(t146));
  t147 = ((int64_t)(v28));
  t148 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(36))))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t147) + (uint64_t)(t148)))));
  t149 = ((uint64_t)(v1));
  t150 = ((uint64_t)(((uint64_t)(v31))));
  t151 = fg185q_ld_i32(t149, t150);
  v32 = ((int32_t)(t151));
  t152 = ((uint64_t)(v1));
  t153 = ((int64_t)(v31));
  t154 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t155 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t154)))))));
  t156 = fg185q_ld_i32(t152, t155);
  v33 = ((int32_t)(t156));
  t157 = ((uint64_t)(v0));
  t158 = ((uint64_t)(((uint64_t)(v7))));
  t159 = fg185q_ld_h2(t157, t158);
  v34 = ((float)(t159));
  t160 = ((uint64_t)(v0));
  t161 = ((uint64_t)(((uint64_t)(v7))));
  t162 = fg185q_ld_h2b(t160, t161);
  v35 = ((float)(t162));
  t163 = ((int32_t)(v10));
  t164 = ((int32_t)(v11));
  t165 = ((int32_t)(v29));
  t166 = ((int32_t)(v30));
  t167 = ((int32_t)(v32));
  t168 = ((int32_t)(v33));
  t169 = ((int32_t)(v21));
  t170 = ((int32_t)(v22));
  t171 = ((int32_t)(v23));
  t172 = ((int32_t)(v24));
  t173 = ((float)(v34));
  t174 = ((float)(v35));
  t175 = ((float)(v26));
  t176 = ((float)(v27));
  t0 = gg_function_10(t163, t164, t165, t166, t167, t168, t169, t170, t171, t172, t173, t174, t175, t176, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_5(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  float t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t v0;
  uint64_t v1;
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
  v7 = p7;
  v8 = p8;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(v8));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t7)))));
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(v6));
  t11 = ((int64_t)(v7));
  t0 = gg_function_6(t1, t2, t3, t8, t9, t10, t11, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  fg185q_fail(t2);
  }
  }
  }
gg_done:;
}
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, uint8_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t p26, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
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
  uint64_t t31;
  uint64_t t32;
  int32_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
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
  float t74;
  uint64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  float t84;
  float t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
  int64_t t91;
  int64_t t92;
  int32_t t93;
  int32_t t94;
  float t95;
  int64_t t96;
  int64_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  float t103;
  int32_t t104;
  int32_t t105;
  float t106;
  float t107;
  int64_t t108;
  int64_t t109;
  float t110;
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
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint64_t t124;
  uint64_t t125;
  float t126;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
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
  uint8_t v19;
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
  float v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
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
  v11 = p11;
  v12 = p12;
  v13 = p13;
  v14 = p14;
  v15 = p15;
  v16 = p16;
  v17 = p17;
  v18 = p18;
  v19 = p19;
  v20 = p20;
  v21 = p21;
  v22 = p22;
  v23 = p23;
  v24 = p24;
  v25 = p25;
  v26 = p26;
  {
  t0 = ((int64_t)(v21));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t2 = ((t0) == (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v23));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) >= (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v23));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t7 = ((t8) < (t9));
  }
  t10 = t7;
  t11 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_2(t10, t11, base + 0);
  t12 = ((int64_t)(v6));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) > (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) > (t17));
  }
  t18 = t15;
  t19 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_function_2(t18, t19, base + 0);
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t21 = ((int64_t)(v24));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(v23));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))));
  v28 = ((int64_t)(v25));
  v29 = ((int64_t)(v25));
  v30 = ((int64_t)(v26));
  t24 = v19;
  if (t24) {
  {
  t25 = ((int64_t)(v25));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t27 = ((int64_t)(v20));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t28)))));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t31 = ((uint64_t)(v2));
  t32 = ((uint64_t)(((uint64_t)(v31))));
  t33 = fg185q_ld_i32(t31, t32);
  v28 = ((int64_t)(((int64_t)(t33))));
  t34 = ((int64_t)(v25));
  t35 = ((int64_t)(v16));
  v29 = ((int64_t)(((int64_t)gg_mod(t34, t35))));
  }
  } else {
  {
  t36 = ((int64_t)(v17));
  t37 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) > (t37));
  t39 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_2(t38, t39, base + 0);
  t40 = ((int64_t)(v25));
  t41 = ((int64_t)(v17));
  v28 = ((int64_t)(((int64_t)gg_div(t40, t41))));
  v29 = ((int64_t)(v25));
  }
  }
  v32 = ((int64_t)(v30));
  t42 = v19;
  t43 = (!(t42));
  if (t43) {
  {
  t44 = ((int64_t)(v18));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((t44) > (t45));
  t47 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_function_2(t46, t47, base + 0);
  t48 = ((int64_t)(v30));
  t49 = ((int64_t)(v18));
  v32 = ((int64_t)(((int64_t)gg_div(t48, t49))));
  }
  }
  v33 = ((int64_t)(v30));
  t50 = ((int64_t)(v32));
  t51 = ((int64_t)(v13));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(v28));
  t54 = ((int64_t)(v10));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t56 = ((int64_t)(v33));
  t57 = ((int64_t)(v14));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  t59 = ((int64_t)(v29));
  t60 = ((int64_t)(v11));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t61)))));
  v36 = ((float)(gg_double_bits(UINT64_C(0))));
  t62 = ((int64_t)(v27));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  v37 = ((int64_t)(((int64_t)gg_div(t62, t63))));
label1:;
  t64 = ((int64_t)(v37));
  t65 = ((int64_t)(v5));
  if (!(((t64) < (t65)))) goto label2;
  {
  t66 = ((int64_t)(v37));
  t67 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t69 = ((int64_t)(((int64_t)gg_div(t67, t68))));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t69)))));
  t70 = ((int64_t)(((int64_t)UINT64_C(2))));
  t71 = ((int64_t)(v27));
  t72 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t73 = ((int64_t)(((int64_t)gg_mod(t71, t72))));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t73)))));
  t74 = ((float)(v36));
  t75 = ((uint64_t)(v0));
  t76 = ((uint64_t)(v1));
  t77 = ((int64_t)(v35));
  t78 = ((int64_t)(v34));
  t79 = ((int64_t)(v22));
  t80 = ((int64_t)(v37));
  t81 = ((int64_t)(v38));
  t82 = ((int64_t)(v39));
  t83 = ((int64_t)(v7));
  t84 = gg_function_5(t75, t76, t77, t78, t79, t80, t81, t82, t83, base + 0);
  t85 = ((float)(t84));
  v36 = ((float)(((t74) + (t85))));
  t86 = ((int64_t)(v37));
  t87 = ((int64_t)(v6));
  v37 = ((int64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))));
  }
  goto label1;
label2:;
  t88 = ((int64_t)(v24));
  t89 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t90 = ((t88) > (t89));
  if (t90) {
  {
  t91 = ((int64_t)(v24));
  t92 = ((int64_t)(((int64_t)UINT64_C(1))));
  t93 = ((int32_t)(((int32_t)(((int64_t)((uint64_t)(t91) - (uint64_t)(t92)))))));
  t94 = ((int32_t)(((int32_t)(v23))));
  t95 = ((float)(v36));
  fg185q_shm_put(t93, t94, t95);
  }
  }
  fg185q_barrier();
  t96 = ((int64_t)(v24));
  t97 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t98 = ((t96) > (t97));
  if (t98) {
  {
  v36 = ((float)(v36));
  }
  } else {
  {
  v40 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t99 = ((int64_t)(v40));
  t100 = ((int64_t)(v21));
  t101 = ((int64_t)(((int64_t)UINT64_C(1))));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t100) - (uint64_t)(t101)))));
  if (!(((t99) < (t102)))) goto label5;
  {
  t103 = ((float)(v36));
  t104 = ((int32_t)(((int32_t)(v40))));
  t105 = ((int32_t)(((int32_t)(v23))));
  t106 = fg185q_shm_get(t104, t105);
  t107 = ((float)(t106));
  v36 = ((float)(((t103) + (t107))));
  t108 = ((int64_t)(v40));
  t109 = ((int64_t)(((int64_t)UINT64_C(1))));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t108) + (uint64_t)(t109)))));
  }
  goto label4;
label5:;
  t110 = ((float)(v36));
  v36 = gg_function_15(t110, base + 0);
  t111 = ((int64_t)(v23));
  t112 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t113 = ((t111) == (t112));
  if (t113) {
  {
  t114 = ((int64_t)(v30));
  t115 = ((int64_t)(v15));
  t116 = ((int64_t)(((int64_t)((uint64_t)(t114) * (uint64_t)(t115)))));
  t117 = ((int64_t)(v25));
  t118 = ((int64_t)(v12));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) * (uint64_t)(t118)))));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t116) + (uint64_t)(t119)))));
  t121 = ((int64_t)(v22));
  t122 = ((int64_t)(((int64_t)((uint64_t)(t120) + (uint64_t)(t121)))));
  t123 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v41 = ((int64_t)(((int64_t)((uint64_t)(t122) * (uint64_t)(t123)))));
  t124 = ((uint64_t)(v3));
  t125 = ((uint64_t)(((uint64_t)(v41))));
  t126 = ((float)(v36));
  fg185q_st_f32(t124, t125, t126);
  }
  }
  }
  }
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
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
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  {
  t0 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t1 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t2 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((uint8_t)UINT64_C(0));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(((int64_t)UINT64_C(4))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, t20, t21, t22, t23, t24, t25, t26, base + 0);
  }
gg_done:;
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
