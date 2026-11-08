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
static uint8_t gg_eq_3(int32_t a, int32_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, int32_t value);
static uint8_t gg_eq_4(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint8_t value);
static uint8_t gg_eq_6(float a, float b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, float value);
static uint8_t gg_eq_7(double a, double b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, double value);
static uint8_t gg_eq_8(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, uint32_t value);
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
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(float a, float b) {
return a == b;
}
static void gg_render_6(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(double a, double b) {
return a == b;
}
static void gg_render_7(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_8(gg_reference out, uint32_t value) {
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
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static void gg_function_2(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base);
static void gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static int64_t gg_function_5(uint64_t p0, uint64_t p1, int64_t base);
static float gg_function_12(float p0, float p1, int64_t base);
static float gg_function_13(float p0, int32_t p1, int64_t base);
static float gg_function_15(float p0, int32_t p1, int64_t base);
static void gg_function_22(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static void gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  fg180q_fail(t2);
  }
  }
  }
gg_done:;
}
static int64_t gg_function_5(uint64_t p0, uint64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
  t3 = fg180q_ld_i32(t1, t2);
  t0 = ((int64_t)(t3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_15(float p0, int32_t p1, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  uint8_t t3;
  float t4;
  float t5;
  uint32_t t6;
  int32_t t7;
  float t8;
  float t9;
  float t10;
  float t11;
  uint32_t t12;
  int32_t t13;
  float t14;
  float t15;
  float t16;
  float t17;
  uint32_t t18;
  int32_t t19;
  float t20;
  float t21;
  float t22;
  float t23;
  uint32_t t24;
  int32_t t25;
  float t26;
  float t27;
  float t28;
  float t29;
  uint32_t t30;
  int32_t t31;
  float t32;
  float t33;
  float t34;
  float t35;
  uint32_t t36;
  int32_t t37;
  float t38;
  float t39;
  float t40;
  float t41;
  uint32_t t42;
  int32_t t43;
  float t44;
  float t45;
  float v0;
  int32_t v1;
  float v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v2));
  t5 = ((float)(v2));
  t6 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t7 = ((int32_t)(((int64_t)UINT64_C(8))));
  t8 = fg180q_shfl_xor(t5, t6, t7);
  t9 = ((float)(t8));
  v2 = ((float)(((t4) + (t9))));
  t10 = ((float)(v2));
  t11 = ((float)(v2));
  t12 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
  t14 = fg180q_shfl_xor(t11, t12, t13);
  t15 = ((float)(t14));
  v2 = ((float)(((t10) + (t15))));
  t16 = ((float)(v2));
  t17 = ((float)(v2));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t19 = ((int32_t)(((int64_t)UINT64_C(2))));
  t20 = fg180q_shfl_xor(t17, t18, t19);
  t21 = ((float)(t20));
  v2 = ((float)(((t16) + (t21))));
  t22 = ((float)(v2));
  t23 = ((float)(v2));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t25 = ((int32_t)(((int64_t)UINT64_C(1))));
  t26 = fg180q_shfl_xor(t23, t24, t25);
  t27 = ((float)(t26));
  v2 = ((float)(((t22) + (t27))));
  }
  } else {
  {
  t28 = ((float)(v2));
  t29 = ((float)(v2));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t31 = ((int32_t)(((int64_t)UINT64_C(4))));
  t32 = fg180q_shfl_xor(t29, t30, t31);
  t33 = ((float)(t32));
  v2 = ((float)(((t28) + (t33))));
  t34 = ((float)(v2));
  t35 = ((float)(v2));
  t36 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t37 = ((int32_t)(((int64_t)UINT64_C(2))));
  t38 = fg180q_shfl_xor(t35, t36, t37);
  t39 = ((float)(t38));
  v2 = ((float)(((t34) + (t39))));
  t40 = ((float)(v2));
  t41 = ((float)(v2));
  t42 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
  t44 = fg180q_shfl_xor(t41, t42, t43);
  t45 = ((float)(t44));
  v2 = ((float)(((t40) + (t45))));
  }
  }
  t0 = ((float)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_12(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((float)(v1));
  }
  } else {
  {
  t0 = ((float)(v0));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_13(float p0, int32_t p1, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  uint8_t t3;
  float t4;
  float t5;
  uint32_t t6;
  int32_t t7;
  float t8;
  float t9;
  float t10;
  float t11;
  uint32_t t12;
  int32_t t13;
  float t14;
  float t15;
  float t16;
  float t17;
  uint32_t t18;
  int32_t t19;
  float t20;
  float t21;
  float t22;
  float t23;
  uint32_t t24;
  int32_t t25;
  float t26;
  float t27;
  float t28;
  float t29;
  uint32_t t30;
  int32_t t31;
  float t32;
  float t33;
  float t34;
  float t35;
  uint32_t t36;
  int32_t t37;
  float t38;
  float t39;
  float t40;
  float t41;
  uint32_t t42;
  int32_t t43;
  float t44;
  float t45;
  float v0;
  int32_t v1;
  float v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v2));
  t5 = ((float)(v2));
  t6 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t7 = ((int32_t)(((int64_t)UINT64_C(8))));
  t8 = fg180q_shfl_xor(t5, t6, t7);
  t9 = ((float)(t8));
  v2 = gg_function_12(t4, t9, base + 0);
  t10 = ((float)(v2));
  t11 = ((float)(v2));
  t12 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
  t14 = fg180q_shfl_xor(t11, t12, t13);
  t15 = ((float)(t14));
  v2 = gg_function_12(t10, t15, base + 0);
  t16 = ((float)(v2));
  t17 = ((float)(v2));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t19 = ((int32_t)(((int64_t)UINT64_C(2))));
  t20 = fg180q_shfl_xor(t17, t18, t19);
  t21 = ((float)(t20));
  v2 = gg_function_12(t16, t21, base + 0);
  t22 = ((float)(v2));
  t23 = ((float)(v2));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t25 = ((int32_t)(((int64_t)UINT64_C(1))));
  t26 = fg180q_shfl_xor(t23, t24, t25);
  t27 = ((float)(t26));
  v2 = gg_function_12(t22, t27, base + 0);
  }
  } else {
  {
  t28 = ((float)(v2));
  t29 = ((float)(v2));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t31 = ((int32_t)(((int64_t)UINT64_C(4))));
  t32 = fg180q_shfl_xor(t29, t30, t31);
  t33 = ((float)(t32));
  v2 = gg_function_12(t28, t33, base + 0);
  t34 = ((float)(v2));
  t35 = ((float)(v2));
  t36 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t37 = ((int32_t)(((int64_t)UINT64_C(2))));
  t38 = fg180q_shfl_xor(t35, t36, t37);
  t39 = ((float)(t38));
  v2 = gg_function_12(t34, t39, base + 0);
  t40 = ((float)(v2));
  t41 = ((float)(v2));
  t42 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
  t44 = fg180q_shfl_xor(t41, t42, t43);
  t45 = ((float)(t44));
  v2 = gg_function_12(t40, t45, base + 0);
  }
  }
  t0 = ((float)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_2(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint8_t t2;
  int32_t t3;
  int32_t t4;
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
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int32_t t31;
  int32_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint8_t t42;
  uint64_t t43;
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
  uint64_t t70;
  uint64_t t71;
  float t72;
  uint64_t t73;
  uint64_t t74;
  float t75;
  uint64_t t76;
  uint64_t t77;
  float t78;
  uint64_t t79;
  uint64_t t80;
  float t81;
  float t82;
  float t83;
  float t84;
  float t85;
  float t86;
  float t87;
  float t88;
  float t89;
  float t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  int32_t t97;
  int32_t t98;
  int32_t t99;
  uint8_t t100;
  float t101;
  float t102;
  float t103;
  float t104;
  float t105;
  float t106;
  float t107;
  int32_t t108;
  float t109;
  float t110;
  float t111;
  float t112;
  float t113;
  float t114;
  float t115;
  int32_t t116;
  float t117;
  float t118;
  float t119;
  float t120;
  float t121;
  int32_t t122;
  float t123;
  float t124;
  float t125;
  float t126;
  float t127;
  int32_t t128;
  float t129;
  float t130;
  float t131;
  float t132;
  float t133;
  int32_t t134;
  int32_t t135;
  int32_t t136;
  int32_t t137;
  int32_t t138;
  int32_t t139;
  int32_t t140;
  int32_t t141;
  int32_t t142;
  int32_t t143;
  int32_t t144;
  int32_t t145;
  int32_t t146;
  int32_t t147;
  int32_t t148;
  int32_t t149;
  int32_t t150;
  int32_t t151;
  int32_t t152;
  int32_t t153;
  int32_t t154;
  float t155;
  float t156;
  int32_t t157;
  int32_t t158;
  uint8_t t159;
  int32_t t160;
  int32_t t161;
  int32_t t162;
  int32_t t163;
  uint8_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint64_t t169;
  int64_t t170;
  int64_t t171;
  uint64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int32_t t177;
  int32_t t178;
  int32_t t179;
  int32_t t180;
  int32_t t181;
  int32_t t182;
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
  uint8_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  uint64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  uint64_t t202;
  int32_t t203;
  int32_t t204;
  int32_t t205;
  uint8_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  uint8_t t211;
  uint8_t t212;
  int64_t t213;
  int64_t t214;
  uint8_t t215;
  uint64_t t216;
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
  uint64_t t227;
  float t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  uint8_t t233;
  uint64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  uint64_t t243;
  float t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  uint8_t t249;
  int32_t t250;
  int32_t t251;
  uint8_t t252;
  uint64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  uint64_t t260;
  float t261;
  float t262;
  uint64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  uint64_t t270;
  float t271;
  int32_t t272;
  int32_t t273;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
  int32_t v18;
  int32_t v19;
  int32_t v20;
  int32_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int32_t v27;
  int64_t v28;
  int64_t v29;
  float v30;
  float v31;
  float v32;
  float v33;
  int64_t v34;
  uint64_t v35;
  float v36;
  float v37;
  float v38;
  int32_t v39;
  int32_t v40;
  int32_t v41;
  int32_t v42;
  int32_t v43;
  int32_t v44;
  float v45;
  int32_t v46;
  int32_t v47;
  int64_t v48;
  int64_t v49;
  int64_t v50;
  int32_t v51;
  int64_t v52;
  int64_t v53;
  int64_t v54;
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
  {
  t0 = ((int32_t)(v11));
  t1 = ((int32_t)(((int64_t)UINT64_C(2))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v20 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(64))))));
  }
  } else {
  {
  v20 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  }
  }
  t3 = ((int32_t)(v11));
  t4 = ((int32_t)(((int64_t)UINT64_C(2))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  v21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  }
  } else {
  {
  v21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  }
  }
  t6 = ((int64_t)(((int64_t)(v17))));
  t7 = ((int64_t)(((int64_t)(v14))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = ((int64_t)(((int64_t)(v16))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t12 = ((int64_t)(v22));
  t13 = ((int64_t)(v7));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) > (t16));
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(v8));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t19) > (t20));
  }
  t21 = t18;
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(v9));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t23) > (t24));
  }
  t25 = t22;
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t27) > (t28));
  }
  t29 = t26;
  t30 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_3(t29, t30, base + 0);
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int32_t)(v12));
  t32 = ((int32_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  if (t33) {
  {
  t34 = ((int64_t)(((int64_t)(v13))));
  t35 = ((int64_t)(v5));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  }
  } else {
  {
  t36 = ((int64_t)(((int64_t)(v15))));
  t37 = ((int64_t)(v9));
  v24 = ((int64_t)(((int64_t)gg_mod(t36, t37))));
  t38 = ((int64_t)(((int64_t)(v15))));
  t39 = ((int64_t)(v9));
  v25 = ((int64_t)(((int64_t)gg_div(t38, t39))));
  v26 = ((int64_t)(((int64_t)(v13))));
  t40 = ((uint64_t)(v1));
  t41 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t40) != (t41));
  if (t42) {
  {
  t43 = ((uint64_t)(v1));
  t44 = ((int64_t)(((int64_t)(v13))));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t46 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))))));
  v26 = gg_function_5(t43, t46, base + 0);
  }
  }
  t47 = ((int64_t)(v25));
  t48 = ((int64_t)(v6));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  t50 = ((int64_t)(v24));
  t51 = ((int64_t)(v5));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t52)))));
  t54 = ((int64_t)(v26));
  t55 = ((int64_t)(v4));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t56)))));
  }
  }
  t57 = ((int64_t)(v22));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(128))))));
  v27 = ((int32_t)(((int32_t)(((int64_t)gg_div(t57, t58))))));
  v28 = ((int64_t)(((int64_t)(v27))));
  t59 = ((int64_t)(v22));
  t60 = ((int64_t)(v28));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(128))))));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t59) - (uint64_t)(t62)))));
  v30 = ((float)(gg_double_bits(UINT64_C(0))));
  v31 = ((float)(gg_double_bits(UINT64_C(0))));
  v32 = ((float)(gg_double_bits(UINT64_C(0))));
  v33 = ((float)(gg_double_bits(UINT64_C(0))));
  t63 = ((int64_t)(v22));
  t64 = ((int64_t)(v3));
  t65 = ((t63) < (t64));
  if (t65) {
  {
  t66 = ((int64_t)(v23));
  t67 = ((int64_t)(v22));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t68 = ((int64_t)(v34));
  t69 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v35 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t69)))))));
  t70 = ((uint64_t)(v0));
  t71 = ((uint64_t)(v35));
  t72 = fg180q_ld_f4x(t70, t71);
  v30 = ((float)(t72));
  t73 = ((uint64_t)(v0));
  t74 = ((uint64_t)(v35));
  t75 = fg180q_ld_f4y(t73, t74);
  v31 = ((float)(t75));
  t76 = ((uint64_t)(v0));
  t77 = ((uint64_t)(v35));
  t78 = fg180q_ld_f4z(t76, t77);
  v32 = ((float)(t78));
  t79 = ((uint64_t)(v0));
  t80 = ((uint64_t)(v35));
  t81 = fg180q_ld_f4w(t79, t80);
  v33 = ((float)(t81));
  }
  }
  t82 = ((float)(v30));
  t83 = fg180q_fabs(t82);
  v36 = ((float)(t83));
  t84 = ((float)(v36));
  t85 = ((float)(v31));
  t86 = fg180q_fabs(t85);
  t87 = ((float)(t86));
  v36 = gg_function_12(t84, t87, base + 0);
  t88 = ((float)(v36));
  t89 = ((float)(v32));
  t90 = fg180q_fabs(t89);
  t91 = ((float)(t90));
  v36 = gg_function_12(t88, t91, base + 0);
  t92 = ((float)(v36));
  t93 = ((float)(v33));
  t94 = fg180q_fabs(t93);
  t95 = ((float)(t94));
  v36 = gg_function_12(t92, t95, base + 0);
  t96 = ((float)(v36));
  t97 = ((int32_t)(v11));
  v36 = gg_function_13(t96, t97, base + 0);
  v37 = ((float)(gg_double_bits(UINT64_C(0))));
  t98 = ((int32_t)(v11));
  t99 = ((int32_t)(((int64_t)UINT64_C(0))));
  t100 = ((t98) != (t99));
  if (t100) {
  {
  t101 = ((float)(v30));
  t102 = ((float)(v31));
  v37 = ((float)(((t101) + (t102))));
  t103 = ((float)(v37));
  t104 = ((float)(v32));
  v37 = ((float)(((t103) + (t104))));
  t105 = ((float)(v37));
  t106 = ((float)(v33));
  v37 = ((float)(((t105) + (t106))));
  t107 = ((float)(v37));
  t108 = ((int32_t)(v11));
  v37 = gg_function_15(t107, t108, base + 0);
  }
  }
  t109 = ((float)(gg_double_bits(UINT64_C(4638637247447433216))));
  t110 = ((float)(v36));
  v38 = ((float)(((t109) / (t110))));
  fg180q_pdl_sync();
  t111 = ((float)(v30));
  t112 = ((float)(v38));
  t113 = ((float)(((t111) * (t112))));
  t114 = fg180q_round(t113);
  t115 = ((float)(t114));
  t116 = fg180q_i32(t115);
  v39 = ((int32_t)(t116));
  t117 = ((float)(v31));
  t118 = ((float)(v38));
  t119 = ((float)(((t117) * (t118))));
  t120 = fg180q_round(t119);
  t121 = ((float)(t120));
  t122 = fg180q_i32(t121);
  v40 = ((int32_t)(t122));
  t123 = ((float)(v32));
  t124 = ((float)(v38));
  t125 = ((float)(((t123) * (t124))));
  t126 = fg180q_round(t125);
  t127 = ((float)(t126));
  t128 = fg180q_i32(t127);
  v41 = ((int32_t)(t128));
  t129 = ((float)(v33));
  t130 = ((float)(v38));
  t131 = ((float)(((t129) * (t130))));
  t132 = fg180q_round(t131);
  t133 = ((float)(t132));
  t134 = fg180q_i32(t133);
  v42 = ((int32_t)(t134));
  v43 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(255))))));
  t135 = ((int32_t)(v39));
  t136 = ((int32_t)(v43));
  t137 = ((int32_t)(((t135) & (t136))));
  t138 = ((int32_t)(v40));
  t139 = ((int32_t)(v43));
  t140 = ((int32_t)(((t138) & (t139))));
  t141 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  t142 = ((int32_t)(((int32_t)((uint64_t)(t140) << ((uint64_t)(t141) & 31)))));
  t143 = ((int32_t)(((t137) | (t142))));
  t144 = ((int32_t)(v41));
  t145 = ((int32_t)(v43));
  t146 = ((int32_t)(((t144) & (t145))));
  t147 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  t148 = ((int32_t)(((int32_t)((uint64_t)(t146) << ((uint64_t)(t147) & 31)))));
  t149 = ((int32_t)(((t143) | (t148))));
  t150 = ((int32_t)(v42));
  t151 = ((int32_t)(v43));
  t152 = ((int32_t)(((t150) & (t151))));
  t153 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(24))))));
  t154 = ((int32_t)(((int32_t)((uint64_t)(t152) << ((uint64_t)(t153) & 31)))));
  v44 = ((int32_t)(((t149) | (t154))));
  t155 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t156 = ((float)(v38));
  v45 = ((float)(((t155) / (t156))));
  t157 = ((int32_t)(v12));
  t158 = ((int32_t)(((int64_t)UINT64_C(0))));
  t159 = ((t157) != (t158));
  if (t159) {
  {
  v46 = ((int32_t)(v10));
  }
  } else {
  {
  v46 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  }
  }
  v47 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t160 = ((int32_t)(v47));
  t161 = ((int32_t)(v46));
  if (!(((t160) < (t161)))) goto label2;
  {
  v48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t162 = ((int32_t)(v12));
  t163 = ((int32_t)(((int64_t)UINT64_C(0))));
  t164 = ((t162) != (t163));
  if (t164) {
  {
  t165 = ((int64_t)(((int64_t)(v13))));
  t166 = ((int64_t)(((int64_t)(v10))));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t165) * (uint64_t)(t166)))));
  t168 = ((int64_t)(((int64_t)(v47))));
  v49 = ((int64_t)(((int64_t)((uint64_t)(t167) + (uint64_t)(t168)))));
  t169 = ((uint64_t)(v1));
  t170 = ((int64_t)(v49));
  t171 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t172 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t170) * (uint64_t)(t171)))))));
  v50 = gg_function_5(t169, t172, base + 0);
  t173 = ((int64_t)(v28));
  t174 = ((int64_t)(v8));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) * (uint64_t)(t174)))));
  t176 = ((int64_t)(v50));
  v48 = ((int64_t)(((int64_t)((uint64_t)(t175) + (uint64_t)(t176)))));
  }
  } else {
  {
  t177 = ((int32_t)(v18));
  t178 = ((int32_t)(v19));
  t179 = ((int32_t)(((int32_t)((uint64_t)(t177) * (uint64_t)(t178)))));
  t180 = ((int32_t)(v17));
  t181 = ((int32_t)(((int32_t)((uint64_t)(t179) * (uint64_t)(t180)))));
  t182 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(128))))));
  v51 = ((int32_t)(((int32_t)gg_div(t181, t182))));
  v52 = ((int64_t)(((int64_t)(v51))));
  t183 = ((int64_t)(((int64_t)(v15))));
  t184 = ((int64_t)(v52));
  v53 = ((int64_t)(((int64_t)((uint64_t)(t183) * (uint64_t)(t184)))));
  t185 = ((int64_t)(v53));
  t186 = ((int64_t)(v28));
  t187 = ((int64_t)(v8));
  t188 = ((int64_t)(((int64_t)((uint64_t)(t186) * (uint64_t)(t187)))));
  t189 = ((int64_t)(((int64_t)((uint64_t)(t185) + (uint64_t)(t188)))));
  t190 = ((int64_t)(((int64_t)(v13))));
  v48 = ((int64_t)(((int64_t)((uint64_t)(t189) + (uint64_t)(t190)))));
  }
  }
  t191 = ((int64_t)(v48));
  t192 = ((int64_t)(((int64_t)UINT64_C(0))));
  t193 = ((t191) >= (t192));
  t194 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_3(t193, t194, base + 0);
  t195 = ((int64_t)(v48));
  t196 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(144))))));
  v54 = ((int64_t)(((int64_t)((uint64_t)(t195) * (uint64_t)(t196)))));
  t197 = ((uint64_t)(v2));
  t198 = ((int64_t)(v54));
  t199 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t200 = ((int64_t)(((int64_t)((uint64_t)(t198) + (uint64_t)(t199)))));
  t201 = ((int64_t)(v29));
  t202 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t200) + (uint64_t)(t201)))))));
  t203 = ((int32_t)(v44));
  fg180q_st_i32(t197, t202, t203);
  t204 = ((int32_t)(v11));
  t205 = ((int32_t)(((int64_t)UINT64_C(2))));
  t206 = ((t204) == (t205));
  if (t206) {
  {
  t207 = ((int64_t)(v29));
  t208 = ((int64_t)(((int64_t)UINT64_C(16))));
  t209 = ((int64_t)(((int64_t)gg_mod(t207, t208))));
  t210 = ((int64_t)(((int64_t)UINT64_C(0))));
  t211 = ((t209) == (t210));
  t212 = t211;
  if (t212) {
  t213 = ((int64_t)(v29));
  t214 = ((int64_t)(((int64_t)UINT64_C(96))));
  t212 = ((t213) < (t214));
  }
  t215 = t212;
  if (t215) {
  {
  t216 = ((uint64_t)(v2));
  t217 = ((int64_t)(v54));
  t218 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t219 = ((int64_t)(((int64_t)((uint64_t)(t217) + (uint64_t)(t218)))));
  t220 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t221 = ((int64_t)(v29));
  t222 = ((int64_t)(((int64_t)UINT64_C(16))));
  t223 = ((int64_t)(((int64_t)gg_div(t221, t222))));
  t224 = ((int64_t)(((int64_t)((uint64_t)(t220) + (uint64_t)(t223)))));
  t225 = ((int64_t)(((int64_t)UINT64_C(2))));
  t226 = ((int64_t)(((int64_t)((uint64_t)(t224) * (uint64_t)(t225)))));
  t227 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t219) + (uint64_t)(t226)))))));
  t228 = ((float)(v37));
  fg180q_st_f32(t216, t227, t228);
  t229 = ((int64_t)(v29));
  t230 = ((int64_t)(((int64_t)UINT64_C(64))));
  t231 = ((int64_t)(((int64_t)gg_mod(t229, t230))));
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((t231) == (t232));
  if (t233) {
  {
  t234 = ((uint64_t)(v2));
  t235 = ((int64_t)(v54));
  t236 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t237 = ((int64_t)(((int64_t)((uint64_t)(t235) + (uint64_t)(t236)))));
  t238 = ((int64_t)(v29));
  t239 = ((int64_t)(((int64_t)UINT64_C(64))));
  t240 = ((int64_t)(((int64_t)gg_div(t238, t239))));
  t241 = ((int64_t)(((int64_t)UINT64_C(2))));
  t242 = ((int64_t)(((int64_t)((uint64_t)(t240) * (uint64_t)(t241)))));
  t243 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t237) + (uint64_t)(t242)))))));
  t244 = ((float)(v45));
  fg180q_st_f32(t234, t243, t244);
  }
  }
  }
  }
  }
  } else {
  {
  t245 = ((int64_t)(v29));
  t246 = ((int64_t)(((int64_t)UINT64_C(32))));
  t247 = ((int64_t)(((int64_t)gg_mod(t245, t246))));
  t248 = ((int64_t)(((int64_t)UINT64_C(0))));
  t249 = ((t247) == (t248));
  if (t249) {
  {
  t250 = ((int32_t)(v11));
  t251 = ((int32_t)(((int64_t)UINT64_C(1))));
  t252 = ((t250) == (t251));
  if (t252) {
  {
  t253 = ((uint64_t)(v2));
  t254 = ((int64_t)(v54));
  t255 = ((int64_t)(v29));
  t256 = ((int64_t)(((int64_t)UINT64_C(32))));
  t257 = ((int64_t)(((int64_t)gg_div(t255, t256))));
  t258 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t259 = ((int64_t)(((int64_t)((uint64_t)(t257) * (uint64_t)(t258)))));
  t260 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t254) + (uint64_t)(t259)))))));
  t261 = ((float)(v45));
  t262 = ((float)(v37));
  fg180q_st_half2(t253, t260, t261, t262);
  }
  } else {
  {
  t263 = ((uint64_t)(v2));
  t264 = ((int64_t)(v54));
  t265 = ((int64_t)(v29));
  t266 = ((int64_t)(((int64_t)UINT64_C(32))));
  t267 = ((int64_t)(((int64_t)gg_div(t265, t266))));
  t268 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t267) * (uint64_t)(t268)))));
  t270 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t264) + (uint64_t)(t269)))))));
  t271 = ((float)(v45));
  fg180q_st_f32(t263, t270, t271);
  }
  }
  }
  }
  }
  }
  t272 = v47;
  t273 = ((int32_t)(((int64_t)UINT64_C(1))));
  v47 = ((int32_t)((uint64_t)(t272) + (uint64_t)(t273)));
  }
  goto label1;
label2:;
  }
  }
  }
gg_done:;
}
static void gg_function_22(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
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
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
gg_done:;
}
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
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
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int32_t t28;
  int32_t t29;
  int32_t t30;
  int32_t t31;
  int32_t t32;
  int32_t t33;
  int32_t t34;
  int32_t t35;
  uint64_t v0;
  {
  v0 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v0));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t10 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t11 = ((int32_t)(((int64_t)UINT64_C(0))));
  t12 = ((int32_t)(((int64_t)UINT64_C(0))));
  t13 = ((int32_t)(((int64_t)UINT64_C(0))));
  t14 = ((int32_t)(((int64_t)UINT64_C(0))));
  t15 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t16 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t17 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  gg_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, base + 0);
  t18 = ((uint64_t)(v0));
  t19 = ((uint64_t)(v0));
  t20 = ((uint64_t)(v0));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t28 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t29 = ((int32_t)(((int64_t)UINT64_C(0))));
  t30 = ((int32_t)(((int64_t)UINT64_C(0))));
  t31 = ((int32_t)(((int64_t)UINT64_C(0))));
  t32 = ((int32_t)(((int64_t)UINT64_C(0))));
  t33 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t34 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t35 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  gg_function_22(t18, t19, t20, t21, t22, t23, t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, t34, t35, base + 0);
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
