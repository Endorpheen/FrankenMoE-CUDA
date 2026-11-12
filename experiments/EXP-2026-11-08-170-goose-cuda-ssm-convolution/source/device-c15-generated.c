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


#pragma pack(push, 1)
typedef struct { float data[15]; } gg_type_6;
#pragma pack(pop)
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
static uint8_t gg_eq_5(float a, float b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, float value);
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, gg_type_6 value);
static uint8_t gg_eq_7(double a, double b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, double value);
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
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(float a, float b) {
return a == b;
}
static void gg_render_5(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_type_6*)a.data)[i],((gg_type_6*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b) {
return gg_equal_elements_5(gg_view((a).data, 15),gg_view((b).data, 15));
}
static void gg_render_6(gg_reference out, gg_type_6 value) {
int64_t i; gg_slice view=gg_view((value).data, 15);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(double a, double b) {
return a == b;
}
static void gg_render_7(gg_reference out, double value) {
gg_float(out,value);
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
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, uint8_t p13, uint8_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static float gg_function_5(float p0, int64_t base);
static void gg_function_8(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, uint8_t p14, uint8_t p15, uint8_t p16, int32_t p17, int32_t p18, int32_t p19, int32_t p20, int64_t base);
static float gg_function_5(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t3 = v0;
  t4 = ((float)((-(t3))));
  t5 = fg170s_expf(t4);
  t6 = ((float)(t5));
  t7 = ((float)(((t2) + (t6))));
  t0 = ((float)(((t1) / (t7))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_8(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, uint8_t p14, uint8_t p15, uint8_t p16, int32_t p17, int32_t p18, int32_t p19, int32_t p20, int64_t base) {
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
  uint64_t t11;
  uint64_t t12;
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
  uint64_t t32;
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
  int32_t t44;
  int32_t t45;
  int32_t t46;
  int32_t t47;
  int32_t t48;
  int32_t t49;
  uint8_t t50;
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
  int32_t t61;
  int32_t t62;
  int32_t t63;
  int32_t t64;
  int32_t t65;
  int32_t t66;
  int32_t t67;
  int32_t t68;
  int32_t t69;
  int32_t t70;
  int32_t t71;
  int32_t t72;
  uint8_t t73;
  uint8_t t74;
  int32_t t75;
  int32_t t76;
  uint8_t t77;
  uint64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint64_t t86;
  uint64_t t87;
  uint64_t t88;
  uint64_t t89;
  float t90;
  float t91;
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
  float t104;
  float t105;
  int32_t t106;
  int32_t t107;
  int32_t t108;
  int32_t t109;
  int32_t t110;
  int32_t t111;
  int32_t t112;
  int32_t t113;
  int32_t t114;
  int32_t t115;
  float t116;
  float t117;
  int64_t t118;
  int32_t t119;
  int32_t t120;
  int32_t t121;
  float * t122;
  uint64_t t123;
  uint64_t t124;
  uint64_t t125;
  uint64_t t126;
  uint64_t t127;
  uint64_t t128;
  float t129;
  int32_t t130;
  int32_t t131;
  uint8_t t132;
  uint64_t t133;
  uint64_t t134;
  uint64_t t135;
  uint64_t t136;
  uint64_t t137;
  uint64_t t138;
  float t139;
  int32_t t140;
  int32_t t141;
  int32_t t142;
  int32_t t143;
  float t144;
  uint64_t t145;
  uint64_t t146;
  uint64_t t147;
  uint64_t t148;
  int32_t t149;
  int32_t t150;
  uint64_t t151;
  uint64_t t152;
  float t153;
  float t154;
  int32_t t155;
  float * t156;
  float t157;
  float t158;
  int32_t t159;
  int32_t t160;
  float t161;
  float t162;
  uint8_t t163;
  float t164;
  uint64_t t165;
  uint64_t t166;
  uint64_t t167;
  uint64_t t168;
  uint64_t t169;
  uint64_t t170;
  float t171;
  int32_t t172;
  int32_t t173;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  int32_t v8;
  int32_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  uint8_t v14;
  uint8_t v15;
  uint8_t v16;
  int32_t v17;
  int32_t v18;
  int32_t v19;
  int32_t v20;
  uint64_t v21;
  uint64_t v22;
  uint64_t v23;
  uint64_t v24;
  uint64_t v25;
  uint64_t v26;
  int32_t v27;
  int32_t v28;
  int32_t v29;
  int32_t v30;
  int32_t v31;
  int32_t v32;
  int32_t v33;
  gg_type_6 v34;
  int32_t v35;
  float v36;
  int32_t v37;
  float v38;
  float v39;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(((uint64_t)(v18))));
  t2 = ((uint64_t)(((uint64_t)(v7))));
  t3 = ((uint64_t)(((uint64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((uint64_t)(((uint64_t)((uint64_t)(t0) + (uint64_t)(t3)))));
  t5 = ((uint64_t)(((uint64_t)(v19))));
  t6 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t7 = ((uint64_t)(((uint64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((uint64_t)(((uint64_t)(v6))));
  t9 = ((uint64_t)(((uint64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((uint64_t)(((uint64_t)((uint64_t)(t4) + (uint64_t)(t9)))));
  t11 = ((uint64_t)(((uint64_t)(v20))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32))))));
  t13 = ((uint64_t)(((uint64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((uint64_t)(((uint64_t)(v5))));
  t15 = ((uint64_t)(((uint64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  v21 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t16 = ((uint64_t)(v1));
  t17 = ((uint64_t)(((uint64_t)(v19))));
  t18 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t19 = ((uint64_t)(((uint64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((uint64_t)(((uint64_t)(v8))));
  t21 = ((uint64_t)(((uint64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  v22 = ((uint64_t)(((uint64_t)((uint64_t)(t16) + (uint64_t)(t21)))));
  t22 = ((uint64_t)(v3));
  t23 = ((uint64_t)(((uint64_t)(v18))));
  t24 = ((uint64_t)(((uint64_t)(v11))));
  t25 = ((uint64_t)(((uint64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t26 = ((uint64_t)(((uint64_t)((uint64_t)(t22) + (uint64_t)(t25)))));
  t27 = ((uint64_t)(((uint64_t)(v20))));
  t28 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32))))));
  t29 = ((uint64_t)(((uint64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((uint64_t)(((uint64_t)(v10))));
  t31 = ((uint64_t)(((uint64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t32 = ((uint64_t)(((uint64_t)((uint64_t)(t26) + (uint64_t)(t31)))));
  t33 = ((uint64_t)(((uint64_t)(v19))));
  t34 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t35 = ((uint64_t)(((uint64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((uint64_t)(((uint64_t)(v9))));
  t37 = ((uint64_t)(((uint64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  v23 = ((uint64_t)(((uint64_t)((uint64_t)(t32) + (uint64_t)(t37)))));
  t38 = ((uint64_t)(((uint64_t)(v6))));
  t39 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v24 = ((uint64_t)(((uint64_t)gg_udiv(t38, t39))));
  t40 = ((uint64_t)(((uint64_t)(v8))));
  t41 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v25 = ((uint64_t)(((uint64_t)gg_udiv(t40, t41))));
  t42 = ((uint64_t)(((uint64_t)(v10))));
  t43 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v26 = ((uint64_t)(((uint64_t)gg_udiv(t42, t43))));
  t44 = ((int32_t)(v12));
  t45 = ((int32_t)(v20));
  t46 = ((int32_t)(((int64_t)UINT64_C(32))));
  t47 = ((int32_t)(((int32_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = ((int32_t)(((int32_t)((uint64_t)(t44) - (uint64_t)(t47)))));
  t49 = ((int32_t)(((int64_t)UINT64_C(32))));
  t50 = ((t48) < (t49));
  if (t50) {
  {
  t51 = ((int32_t)(v12));
  t52 = ((int32_t)(v20));
  t53 = ((int32_t)(((int64_t)UINT64_C(32))));
  t54 = ((int32_t)(((int32_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  v27 = ((int32_t)(((int32_t)((uint64_t)(t51) - (uint64_t)(t54)))));
  }
  } else {
  {
  v27 = ((int32_t)(((int64_t)UINT64_C(32))));
  }
  }
  t55 = ((int32_t)(v13));
  t56 = ((int32_t)(((int64_t)UINT64_C(1))));
  t57 = ((int32_t)(((int32_t)((uint64_t)(t55) - (uint64_t)(t56)))));
  t58 = ((int32_t)(((int64_t)UINT64_C(32))));
  v28 = ((int32_t)(((int32_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t59 = ((int32_t)(((int64_t)UINT64_C(128))));
  t60 = ((int32_t)(v28));
  v29 = ((int32_t)(((int32_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  t61 = ((int32_t)(v27));
  t62 = ((int32_t)(v13));
  t63 = ((int32_t)(((int32_t)((uint64_t)(t61) + (uint64_t)(t62)))));
  t64 = ((int32_t)(((int64_t)UINT64_C(1))));
  v30 = ((int32_t)(((int32_t)((uint64_t)(t63) - (uint64_t)(t64)))));
  t65 = ((int32_t)(v17));
  t66 = ((int32_t)(v28));
  v31 = ((int32_t)(((int32_t)gg_div(t65, t66))));
  t67 = ((int32_t)(v17));
  t68 = ((int32_t)(v28));
  v32 = ((int32_t)(((int32_t)gg_mod(t67, t68))));
  v33 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t69 = ((int32_t)(v33));
  t70 = ((int32_t)(v29));
  if (!(((t69) < (t70)))) goto label2;
  {
  t71 = ((int32_t)(v31));
  t72 = ((int32_t)(((int64_t)UINT64_C(128))));
  t73 = ((t71) < (t72));
  if (t73) {
  {
  t74 = v16;
  if (t74) {
  {
  t75 = ((int32_t)(v32));
  t76 = ((int32_t)(v30));
  t77 = ((t75) < (t76));
  if (t77) {
  {
  t78 = ((uint64_t)(v4));
  t79 = ((uint64_t)(((uint64_t)(v31))));
  t80 = ((uint64_t)(((uint64_t)(v28))));
  t81 = ((uint64_t)(((uint64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  t82 = ((uint64_t)(((uint64_t)(v32))));
  t83 = ((uint64_t)(((uint64_t)((uint64_t)(t81) + (uint64_t)(t82)))));
  t84 = ((uint64_t)(v21));
  t85 = ((uint64_t)(((uint64_t)(v31))));
  t86 = ((uint64_t)(v24));
  t87 = ((uint64_t)(((uint64_t)((uint64_t)(t85) * (uint64_t)(t86)))));
  t88 = ((uint64_t)(((uint64_t)(v32))));
  t89 = ((uint64_t)(((uint64_t)((uint64_t)(t87) + (uint64_t)(t88)))));
  t90 = fg170s_load(t84, t89);
  t91 = ((float)(t90));
  fg170s_store(t78, t83, t91);
  }
  }
  }
  } else {
  {
  t92 = ((uint64_t)(v4));
  t93 = ((uint64_t)(((uint64_t)(v31))));
  t94 = ((uint64_t)(((uint64_t)(v28))));
  t95 = ((uint64_t)(((uint64_t)((uint64_t)(t93) * (uint64_t)(t94)))));
  t96 = ((uint64_t)(((uint64_t)(v32))));
  t97 = ((uint64_t)(((uint64_t)((uint64_t)(t95) + (uint64_t)(t96)))));
  t98 = ((uint64_t)(v21));
  t99 = ((uint64_t)(((uint64_t)(v31))));
  t100 = ((uint64_t)(v24));
  t101 = ((uint64_t)(((uint64_t)((uint64_t)(t99) * (uint64_t)(t100)))));
  t102 = ((uint64_t)(((uint64_t)(v32))));
  t103 = ((uint64_t)(((uint64_t)((uint64_t)(t101) + (uint64_t)(t102)))));
  t104 = fg170s_load(t98, t103);
  t105 = ((float)(t104));
  fg170s_store(t92, t97, t105);
  }
  }
  }
  }
  t106 = v32;
  t107 = ((int32_t)(((int64_t)UINT64_C(128))));
  v32 = ((int32_t)((uint64_t)(t106) + (uint64_t)(t107)));
  t108 = v31;
  t109 = ((int32_t)(v32));
  t110 = ((int32_t)(v28));
  t111 = ((int32_t)(((int32_t)gg_div(t109, t110))));
  v31 = ((int32_t)((uint64_t)(t108) + (uint64_t)(t111)));
  t112 = ((int32_t)(v32));
  t113 = ((int32_t)(v28));
  v32 = ((int32_t)(((int32_t)gg_mod(t112, t113))));
  t114 = v33;
  t115 = ((int32_t)(((int64_t)UINT64_C(128))));
  v33 = ((int32_t)((uint64_t)(t114) + (uint64_t)(t115)));
  }
  goto label1;
label2:;
  fg170s_barrier();
  memset(&t116, 0, sizeof(t116));
  t117 = ((float)(t116));
  for (t118=0; t118<15; t118++) (v34).data[t118] = t117;
  v35 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t119 = ((int32_t)(v35));
  t120 = ((int32_t)(v13));
  if (!(((t119) < (t120)))) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t121 = v35;
  t122 = ((float *)(v34).data) + gg_index(t121, ((int64_t)UINT64_C(15)));
  t123 = ((uint64_t)(v22));
  t124 = ((uint64_t)(((uint64_t)(v17))));
  t125 = ((uint64_t)(v25));
  t126 = ((uint64_t)(((uint64_t)((uint64_t)(t124) * (uint64_t)(t125)))));
  t127 = ((uint64_t)(((uint64_t)(v35))));
  t128 = ((uint64_t)(((uint64_t)((uint64_t)(t126) + (uint64_t)(t127)))));
  t129 = fg170s_load(t123, t128);
  (*t122) = ((float)(t129));
  t130 = v35;
  t131 = ((int32_t)(((int64_t)UINT64_C(1))));
  v35 = ((int32_t)((uint64_t)(t130) + (uint64_t)(t131)));
  }
  goto label4;
label5:;
  v36 = ((float)(gg_double_bits(UINT64_C(0))));
  t132 = v15;
  if (t132) {
  {
  t133 = ((uint64_t)(v2));
  t134 = ((uint64_t)(((uint64_t)(v19))));
  t135 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t136 = ((uint64_t)(((uint64_t)((uint64_t)(t134) * (uint64_t)(t135)))));
  t137 = ((uint64_t)(((uint64_t)(v17))));
  t138 = ((uint64_t)(((uint64_t)((uint64_t)(t136) + (uint64_t)(t137)))));
  t139 = fg170s_load(t133, t138);
  v36 = ((float)(t139));
  }
  }
  v37 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t140 = ((int32_t)(v37));
  t141 = ((int32_t)(v27));
  if (!(((t140) < (t141)))) goto label8;
  {
  v38 = ((float)(gg_double_bits(UINT64_C(0))));
  v35 = ((int32_t)(((int64_t)UINT64_C(0))));
label10:;
  t142 = ((int32_t)(v35));
  t143 = ((int32_t)(v13));
  if (!(((t142) < (t143)))) goto label11;
  {
  t144 = v38;
  t145 = ((uint64_t)(v4));
  t146 = ((uint64_t)(((uint64_t)(v17))));
  t147 = ((uint64_t)(((uint64_t)(v28))));
  t148 = ((uint64_t)(((uint64_t)((uint64_t)(t146) * (uint64_t)(t147)))));
  t149 = ((int32_t)(v37));
  t150 = ((int32_t)(v35));
  t151 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t149) + (uint64_t)(t150)))))));
  t152 = ((uint64_t)(((uint64_t)((uint64_t)(t148) + (uint64_t)(t151)))));
  t153 = fg170s_load(t145, t152);
  t154 = ((float)(t153));
  /* FG_DIRECT_SLICE_INDEX */
  t155 = v35;
  t156 = ((float *)(v34).data) + gg_index(t155, ((int64_t)UINT64_C(15)));
  t157 = ((float)((*t156)));
  t158 = ((float)(((t154) * (t157))));
  v38 = ((t144) + (t158));
  t159 = v35;
  t160 = ((int32_t)(((int64_t)UINT64_C(1))));
  v35 = ((int32_t)((uint64_t)(t159) + (uint64_t)(t160)));
  }
  goto label10;
label11:;
  t161 = v38;
  t162 = ((float)(v36));
  v38 = ((t161) + (t162));
  t163 = v14;
  if (t163) {
  {
  t164 = ((float)(v38));
  v39 = gg_function_5(t164, base + 0);
  }
  } else {
  {
  v39 = ((float)(v38));
  }
  }
  t165 = ((uint64_t)(v23));
  t166 = ((uint64_t)(((uint64_t)(v37))));
  t167 = ((uint64_t)(v26));
  t168 = ((uint64_t)(((uint64_t)((uint64_t)(t166) * (uint64_t)(t167)))));
  t169 = ((uint64_t)(((uint64_t)(v17))));
  t170 = ((uint64_t)(((uint64_t)((uint64_t)(t168) + (uint64_t)(t169)))));
  t171 = ((float)(v39));
  fg170s_store(t165, t170, t171);
  t172 = v37;
  t173 = ((int32_t)(((int64_t)UINT64_C(1))));
  v37 = ((int32_t)((uint64_t)(t172) + (uint64_t)(t173)));
  }
  goto label7;
label8:;
  }
gg_done:;
}
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, uint8_t p13, uint8_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
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
  uint64_t t11;
  uint64_t t12;
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
  float t32;
  float t33;
  int64_t t34;
  float t35;
  float t36;
  int64_t t37;
  int32_t t38;
  int32_t t39;
  int32_t t40;
  float * t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
  uint64_t t46;
  uint64_t t47;
  float t48;
  int32_t t49;
  int32_t t50;
  uint8_t t51;
  uint64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint64_t t56;
  uint64_t t57;
  float t58;
  int32_t t59;
  int32_t t60;
  int32_t t61;
  int32_t t62;
  uint8_t t63;
  int32_t t64;
  int32_t t65;
  int32_t t66;
  float * t67;
  uint64_t t68;
  uint64_t t69;
  uint64_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  float t74;
  int32_t t75;
  int32_t t76;
  gg_slice t77;
  int32_t t78;
  int32_t t79;
  int32_t t80;
  int32_t t81;
  int32_t t82;
  float * t83;
  uint64_t t84;
  uint64_t t85;
  uint64_t t86;
  uint64_t t87;
  int32_t t88;
  int32_t t89;
  int32_t t90;
  int32_t t91;
  uint64_t t92;
  uint64_t t93;
  float t94;
  int32_t t95;
  int32_t t96;
  float t97;
  gg_slice t98;
  int32_t t99;
  int32_t t100;
  int32_t t101;
  int32_t t102;
  int32_t t103;
  float * t104;
  float t105;
  int32_t t106;
  float * t107;
  float t108;
  float t109;
  int32_t t110;
  int32_t t111;
  float t112;
  float t113;
  uint8_t t114;
  float t115;
  uint64_t t116;
  uint64_t t117;
  uint64_t t118;
  uint64_t t119;
  uint64_t t120;
  uint64_t t121;
  float t122;
  int32_t t123;
  int32_t t124;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int32_t v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  int32_t v8;
  int32_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  uint8_t v13;
  uint8_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
  uint64_t v18;
  uint64_t v19;
  uint64_t v20;
  uint64_t v21;
  uint64_t v22;
  uint64_t v23;
  gg_type_6 v24;
  gg_type_6 v25;
  int32_t v26;
  float v27;
  int32_t v28;
  float v29;
  float v30;
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
  fg170s_lc();
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(((uint64_t)(v16))));
  t2 = ((uint64_t)(((uint64_t)(v6))));
  t3 = ((uint64_t)(((uint64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((uint64_t)(((uint64_t)((uint64_t)(t0) + (uint64_t)(t3)))));
  t5 = ((uint64_t)(((uint64_t)(v17))));
  t6 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t7 = ((uint64_t)(((uint64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((uint64_t)(((uint64_t)(v5))));
  t9 = ((uint64_t)(((uint64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  v18 = ((uint64_t)(((uint64_t)((uint64_t)(t4) + (uint64_t)(t9)))));
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(((uint64_t)(v17))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t13 = ((uint64_t)(((uint64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((uint64_t)(((uint64_t)(v7))));
  t15 = ((uint64_t)(((uint64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  v19 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t16 = ((uint64_t)(v3));
  t17 = ((uint64_t)(((uint64_t)(v16))));
  t18 = ((uint64_t)(((uint64_t)(v10))));
  t19 = ((uint64_t)(((uint64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((uint64_t)(((uint64_t)((uint64_t)(t16) + (uint64_t)(t19)))));
  t21 = ((uint64_t)(((uint64_t)(v17))));
  t22 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t23 = ((uint64_t)(((uint64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((uint64_t)(((uint64_t)(v8))));
  t25 = ((uint64_t)(((uint64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  v20 = ((uint64_t)(((uint64_t)((uint64_t)(t20) + (uint64_t)(t25)))));
  t26 = ((uint64_t)(((uint64_t)(v5))));
  t27 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v21 = ((uint64_t)(((uint64_t)gg_udiv(t26, t27))));
  t28 = ((uint64_t)(((uint64_t)(v7))));
  t29 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v22 = ((uint64_t)(((uint64_t)gg_udiv(t28, t29))));
  t30 = ((uint64_t)(((uint64_t)(v9))));
  t31 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v23 = ((uint64_t)(((uint64_t)gg_udiv(t30, t31))));
  memset(&t32, 0, sizeof(t32));
  t33 = ((float)(t32));
  for (t34=0; t34<15; t34++) (v24).data[t34] = t33;
  memset(&t35, 0, sizeof(t35));
  t36 = ((float)(t35));
  for (t37=0; t37<15; t37++) (v25).data[t37] = t36;
  fg170s_sync();
  v26 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t38 = ((int32_t)(v26));
  t39 = ((int32_t)(v12));
  if (!(((t38) < (t39)))) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t40 = v26;
  t41 = ((float *)(v25).data) + gg_index(t40, ((int64_t)UINT64_C(15)));
  t42 = ((uint64_t)(v19));
  t43 = ((uint64_t)(((uint64_t)(v15))));
  t44 = ((uint64_t)(v22));
  t45 = ((uint64_t)(((uint64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((uint64_t)(((uint64_t)(v26))));
  t47 = ((uint64_t)(((uint64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  t48 = fg170s_load(t42, t47);
  (*t41) = ((float)(t48));
  t49 = v26;
  t50 = ((int32_t)(((int64_t)UINT64_C(1))));
  v26 = ((int32_t)((uint64_t)(t49) + (uint64_t)(t50)));
  }
  goto label1;
label2:;
  v27 = ((float)(gg_double_bits(UINT64_C(0))));
  t51 = v14;
  if (t51) {
  {
  t52 = ((uint64_t)(v2));
  t53 = ((uint64_t)(((uint64_t)(v17))));
  t54 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t55 = ((uint64_t)(((uint64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((uint64_t)(((uint64_t)(v15))));
  t57 = ((uint64_t)(((uint64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
  t58 = fg170s_load(t52, t57);
  v27 = ((float)(t58));
  }
  }
  v28 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t59 = ((int32_t)(v28));
  t60 = ((int32_t)(v11));
  if (!(((t59) < (t60)))) goto label5;
  {
  v29 = ((float)(gg_double_bits(UINT64_C(0))));
  t61 = ((int32_t)(v28));
  t62 = ((int32_t)(((int64_t)UINT64_C(0))));
  t63 = ((t61) == (t62));
  if (t63) {
  {
  v26 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t64 = ((int32_t)(v26));
  t65 = ((int32_t)(v12));
  if (!(((t64) < (t65)))) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t66 = v26;
  t67 = ((float *)(v24).data) + gg_index(t66, ((int64_t)UINT64_C(15)));
  t68 = ((uint64_t)(v18));
  t69 = ((uint64_t)(((uint64_t)(v15))));
  t70 = ((uint64_t)(v21));
  t71 = ((uint64_t)(((uint64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((uint64_t)(((uint64_t)(v26))));
  t73 = ((uint64_t)(((uint64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
  t74 = fg170s_load(t68, t73);
  (*t67) = ((float)(t74));
  t75 = v26;
  t76 = ((int32_t)(((int64_t)UINT64_C(1))));
  v26 = ((int32_t)((uint64_t)(t75) + (uint64_t)(t76)));
  }
  goto label7;
label8:;
  }
  } else {
  {
  t77 = gg_view((v24).data, 15);
  t78 = ((int32_t)(v28));
  t79 = ((int32_t)(((int64_t)UINT64_C(1))));
  t80 = ((int32_t)(((int32_t)((uint64_t)(t78) - (uint64_t)(t79)))));
  t81 = ((int32_t)(v12));
  t82 = ((int32_t)gg_mod(t80, t81));
  t83 = ((float *)t77.data) + gg_index(t82, t77.len);
  t84 = ((uint64_t)(v18));
  t85 = ((uint64_t)(((uint64_t)(v15))));
  t86 = ((uint64_t)(v21));
  t87 = ((uint64_t)(((uint64_t)((uint64_t)(t85) * (uint64_t)(t86)))));
  t88 = ((int32_t)(v28));
  t89 = ((int32_t)(v12));
  t90 = ((int32_t)(((int32_t)((uint64_t)(t88) + (uint64_t)(t89)))));
  t91 = ((int32_t)(((int64_t)UINT64_C(1))));
  t92 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t90) - (uint64_t)(t91)))))));
  t93 = ((uint64_t)(((uint64_t)((uint64_t)(t87) + (uint64_t)(t92)))));
  t94 = fg170s_load(t84, t93);
  (*t83) = ((float)(t94));
  }
  }
  v26 = ((int32_t)(((int64_t)UINT64_C(0))));
label10:;
  t95 = ((int32_t)(v26));
  t96 = ((int32_t)(v12));
  if (!(((t95) < (t96)))) goto label11;
  {
  t97 = v29;
  t98 = gg_view((v24).data, 15);
  t99 = ((int32_t)(v28));
  t100 = ((int32_t)(v26));
  t101 = ((int32_t)(((int32_t)((uint64_t)(t99) + (uint64_t)(t100)))));
  t102 = ((int32_t)(v12));
  t103 = ((int32_t)gg_mod(t101, t102));
  t104 = ((float *)t98.data) + gg_index(t103, t98.len);
  t105 = ((float)((*t104)));
  /* FG_DIRECT_SLICE_INDEX */
  t106 = v26;
  t107 = ((float *)(v25).data) + gg_index(t106, ((int64_t)UINT64_C(15)));
  t108 = ((float)((*t107)));
  t109 = ((float)(((t105) * (t108))));
  v29 = ((t97) + (t109));
  t110 = v26;
  t111 = ((int32_t)(((int64_t)UINT64_C(1))));
  v26 = ((int32_t)((uint64_t)(t110) + (uint64_t)(t111)));
  }
  goto label10;
label11:;
  t112 = v29;
  t113 = ((float)(v27));
  v29 = ((t112) + (t113));
  t114 = v13;
  if (t114) {
  {
  t115 = ((float)(v29));
  v30 = gg_function_5(t115, base + 0);
  }
  } else {
  {
  v30 = ((float)(v29));
  }
  }
  t116 = ((uint64_t)(v20));
  t117 = ((uint64_t)(((uint64_t)(v28))));
  t118 = ((uint64_t)(v23));
  t119 = ((uint64_t)(((uint64_t)((uint64_t)(t117) * (uint64_t)(t118)))));
  t120 = ((uint64_t)(((uint64_t)(v15))));
  t121 = ((uint64_t)(((uint64_t)((uint64_t)(t119) + (uint64_t)(t120)))));
  t122 = ((float)(v30));
  fg170s_store(t116, t121, t122);
  t123 = v28;
  t124 = ((int32_t)(((int64_t)UINT64_C(1))));
  v28 = ((int32_t)((uint64_t)(t123) + (uint64_t)(t124)));
  }
  goto label4;
label5:;
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  uint8_t t13;
  uint8_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  int32_t t23;
  int32_t t24;
  int32_t t25;
  int32_t t26;
  int32_t t27;
  int32_t t28;
  int32_t t29;
  int32_t t30;
  int32_t t31;
  uint8_t t32;
  uint8_t t33;
  uint8_t t34;
  int32_t t35;
  int32_t t36;
  int32_t t37;
  int32_t t38;
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t1 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((int32_t)(((int64_t)UINT64_C(4))));
  t5 = ((int32_t)(((int64_t)UINT64_C(4))));
  t6 = ((int32_t)(((int64_t)UINT64_C(4))));
  t7 = ((int32_t)(((int64_t)UINT64_C(4))));
  t8 = ((int32_t)(((int64_t)UINT64_C(4))));
  t9 = ((int32_t)(((int64_t)UINT64_C(4))));
  t10 = ((int32_t)(((int64_t)UINT64_C(4))));
  t11 = ((int32_t)(((int64_t)UINT64_C(0))));
  t12 = ((int32_t)(((int64_t)UINT64_C(3))));
  t13 = ((uint8_t)UINT64_C(0));
  t14 = ((uint8_t)UINT64_C(0));
  t15 = ((int32_t)(((int64_t)UINT64_C(0))));
  t16 = ((int32_t)(((int64_t)UINT64_C(0))));
  t17 = ((int32_t)(((int64_t)UINT64_C(0))));
  gg_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, base + 0);
  t18 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t19 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t20 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t21 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t22 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t23 = ((int32_t)(((int64_t)UINT64_C(4))));
  t24 = ((int32_t)(((int64_t)UINT64_C(4))));
  t25 = ((int32_t)(((int64_t)UINT64_C(4))));
  t26 = ((int32_t)(((int64_t)UINT64_C(4))));
  t27 = ((int32_t)(((int64_t)UINT64_C(4))));
  t28 = ((int32_t)(((int64_t)UINT64_C(4))));
  t29 = ((int32_t)(((int64_t)UINT64_C(4))));
  t30 = ((int32_t)(((int64_t)UINT64_C(0))));
  t31 = ((int32_t)(((int64_t)UINT64_C(3))));
  t32 = ((uint8_t)UINT64_C(0));
  t33 = ((uint8_t)UINT64_C(0));
  t34 = ((uint8_t)UINT64_C(0));
  t35 = ((int32_t)(((int64_t)UINT64_C(0))));
  t36 = ((int32_t)(((int64_t)UINT64_C(0))));
  t37 = ((int32_t)(((int64_t)UINT64_C(0))));
  t38 = ((int32_t)(((int64_t)UINT64_C(0))));
  gg_function_8(t18, t19, t20, t21, t22, t23, t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, t34, t35, t36, t37, t38, base + 0);
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
