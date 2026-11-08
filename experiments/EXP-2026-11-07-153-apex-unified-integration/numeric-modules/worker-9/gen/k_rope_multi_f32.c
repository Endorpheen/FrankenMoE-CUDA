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
typedef struct { float data[4096]; } gg_type_2;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[64]; } gg_type_4;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[2]; } gg_type_12;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(float a, float b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, float value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint8_t value);
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_type_4 value);
static uint8_t gg_eq_5(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_slice value);
static uint8_t gg_eq_6(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, gg_slice value);
static uint8_t gg_eq_7(int64_t a, int64_t b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, int64_t value);
static uint8_t gg_eq_8(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, uint8_t value);
static uint8_t gg_eq_10(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, uint32_t value);
static uint8_t gg_eq_11(int32_t a, int32_t b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, int32_t value);
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_type_12 value);
static uint8_t gg_eq_13(double a, double b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, double value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(float a, float b) {
return a == b;
}
static void gg_render_1(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
return gg_equal_elements_1(gg_view((a).data, 4096),gg_view((b).data, 4096));
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
int64_t i; gg_slice view=gg_view((value).data, 4096);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_type_4*)a.data)[i],((gg_type_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b) {
return gg_equal_elements_3(gg_view((a).data, 64),gg_view((b).data, 64));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
gg_text(out,gg_view((value).data, 64));
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_5(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_6(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_7(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_8(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_10(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_11(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_type_12*)a.data)[i],((gg_type_12*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_type_12 a, gg_type_12 b) {
return gg_equal_elements_1(gg_view((a).data, 2),gg_view((b).data, 2));
}
static void gg_render_12(gg_reference out, gg_type_12 value) {
int64_t i; gg_slice view=gg_view((value).data, 2);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(double a, double b) {
return a == b;
}
static void gg_render_13(gg_reference out, double value) {
gg_float(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, float p14, float p15, float p16, float p17, float p18, float p19, int64_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t base);
static int64_t gg_function_8(gg_slice p0, int64_t p1, int64_t base);
static gg_type_12 gg_function_10(float p0, float p1, float p2, float p3, int64_t p4, float p5, float p6, int64_t base);
static float gg_function_11(float p0, float p1, int64_t p2, int64_t base);
static float gg_function_11(float p0, float p1, int64_t p2, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float t8;
  float t9;
  float t10;
  float t11;
  float t12;
  float t13;
  float t14;
  float t15;
  float t16;
  float t17;
  float t18;
  float t19;
  float v0;
  float v1;
  int64_t v2;
  float v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((float)(((float)(((int64_t)gg_div(t1, t2))))));
  t4 = ((float)(v0));
  t5 = ((float)(((t3) - (t4))));
  t6 = ((float)(((float)(gg_double_bits(UINT64_C(4562254508917369340))))));
  t7 = ((float)(v1));
  t8 = ((float)(v0));
  t9 = ((float)(((t7) - (t8))));
  t10 = apex_fmaxf(t6, t9);
  t11 = ((float)(t10));
  v3 = ((float)(((t5) / (t11))));
  t12 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t13 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t14 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t15 = ((float)(v3));
  t16 = apex_fmaxf(t14, t15);
  t17 = ((float)(t16));
  t18 = apex_fminf(t13, t17);
  t19 = ((float)(t18));
  t0 = ((float)(((t12) - (t19))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static gg_type_12 gg_function_10(float p0, float p1, float p2, float p3, int64_t p4, float p5, float p6, int64_t base) {
  gg_type_12 t0;
  float t1;
  float t2;
  float t3;
  float t4;
  uint8_t t5;
  float t6;
  float t7;
  int64_t t8;
  float t9;
  float t10;
  float t11;
  float t12;
  float t13;
  float t14;
  float t15;
  float t16;
  float t17;
  float t18;
  float t19;
  float t20;
  float t21;
  float t22;
  float t23;
  float t24;
  float t25;
  float t26;
  float t27;
  float t28;
  float t29;
  float t30;
  float t31;
  int64_t t32;
  int64_t t33;
  float * t34;
  float t35;
  float t36;
  float t37;
  float t38;
  int64_t t39;
  float * t40;
  float t41;
  float t42;
  float t43;
  float t44;
  float v0;
  float v1;
  float v2;
  float v3;
  int64_t v4;
  float v5;
  float v6;
  float v7;
  float v8;
  float v9;
  float v10;
  gg_type_12 v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((float)(v1));
  t2 = ((float)(v0));
  v7 = ((float)(((t1) * (t2))));
  v8 = ((float)(v7));
  v9 = ((float)(v6));
  t3 = ((float)(v5));
  t4 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t5 = ((t3) != (t4));
  if (t5) {
  {
  t6 = ((float)(v2));
  t7 = ((float)(v3));
  t8 = ((int64_t)(v4));
  t9 = gg_function_11(t6, t7, t8, base + 0);
  t10 = ((float)(t9));
  t11 = ((float)(v5));
  v10 = ((float)(((t10) * (t11))));
  t12 = ((float)(v7));
  t13 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t14 = ((float)(v10));
  t15 = ((float)(((t13) - (t14))));
  t16 = ((float)(((t12) * (t15))));
  t17 = ((float)(v0));
  t18 = ((float)(v10));
  t19 = ((float)(((t17) * (t18))));
  v8 = ((float)(((t16) + (t19))));
  t20 = ((float)(v9));
  t21 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t22 = ((float)(((float)(gg_double_bits(UINT64_C(4591870180066957722))))));
  t23 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t24 = ((float)(v1));
  t25 = ((float)(((t23) / (t24))));
  t26 = apex_logf(t25);
  t27 = ((float)(t26));
  t28 = ((float)(((t22) * (t27))));
  t29 = ((float)(((t21) + (t28))));
  v9 = ((float)(((t20) * (t29))));
  }
  }
  memset(&t30, 0, sizeof(t30));
  t31 = ((float)(t30));
  for (t32=0; t32<2; t32++) (v11).data[t32] = t31;
  /* FG_DIRECT_SLICE_INDEX */
  t33 = ((int64_t)UINT64_C(0));
  t34 = ((float *)(v11).data) + gg_index(t33, ((int64_t)UINT64_C(2)));
  t35 = ((float)(v8));
  t36 = apex_cosf(t35);
  t37 = ((float)(t36));
  t38 = ((float)(v9));
  (*t34) = ((float)(((t37) * (t38))));
  /* FG_DIRECT_SLICE_INDEX */
  t39 = ((int64_t)UINT64_C(1));
  t40 = ((float *)(v11).data) + gg_index(t39, ((int64_t)UINT64_C(2)));
  t41 = ((float)(v8));
  t42 = apex_sinf(t41);
  t43 = ((float)(t42));
  t44 = ((float)(v9));
  (*t40) = ((float)(((t43) * (t44))));
  t0 = v11;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_8(gg_slice p0, int64_t p1, int64_t base) {
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
  int64_t t11;
  uint8_t * t12;
  uint32_t t13;
  gg_slice t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t * t18;
  uint32_t t19;
  int64_t t20;
  uint32_t t21;
  uint32_t t22;
  gg_slice t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t * t27;
  uint32_t t28;
  int64_t t29;
  uint32_t t30;
  uint32_t t31;
  gg_slice t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t * t36;
  uint32_t t37;
  int64_t t38;
  uint32_t t39;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  uint32_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)((v0).len));
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  t4 = ((t5) <= (t8));
  }
  if (!(t4)) gg_fail("assertion failed");
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v2;
  t12 = ((uint8_t *)(v0).data) + gg_index(t11, (v0).len);
  t13 = ((uint32_t)(((uint32_t)((*t12)))));
  t14 = v0;
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = ((int64_t)((uint64_t)(t15) + (uint64_t)(t16)));
  t18 = ((uint8_t *)t14.data) + gg_index(t17, t14.len);
  t19 = ((uint32_t)(((uint32_t)((*t18)))));
  t20 = ((int64_t)(((int64_t)UINT64_C(8))));
  t21 = ((uint32_t)(((uint32_t)((uint64_t)(t19) << ((uint64_t)(t20) & 31)))));
  t22 = ((uint32_t)(((t13) | (t21))));
  t23 = v0;
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(((int64_t)UINT64_C(2))));
  t26 = ((int64_t)((uint64_t)(t24) + (uint64_t)(t25)));
  t27 = ((uint8_t *)t23.data) + gg_index(t26, t23.len);
  t28 = ((uint32_t)(((uint32_t)((*t27)))));
  t29 = ((int64_t)(((int64_t)UINT64_C(16))));
  t30 = ((uint32_t)(((uint32_t)((uint64_t)(t28) << ((uint64_t)(t29) & 31)))));
  t31 = ((uint32_t)(((t22) | (t30))));
  t32 = v0;
  t33 = ((int64_t)(v2));
  t34 = ((int64_t)(((int64_t)UINT64_C(3))));
  t35 = ((int64_t)((uint64_t)(t33) + (uint64_t)(t34)));
  t36 = ((uint8_t *)t32.data) + gg_index(t35, t32.len);
  t37 = ((uint32_t)(((uint32_t)((*t36)))));
  t38 = ((int64_t)(((int64_t)UINT64_C(24))));
  t39 = ((uint32_t)(((uint32_t)((uint64_t)(t37) << ((uint64_t)(t38) & 31)))));
  v3 = ((uint32_t)(((t31) | (t39))));
  t0 = ((int64_t)(((int64_t)(((int32_t)(v3))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_1(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, float p14, float p15, float p16, float p17, float p18, float p19, int64_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t base) {
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
  uint8_t t71;
  uint8_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  gg_slice t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  float * t84;
  gg_slice t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  float * t91;
  gg_slice t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  float * t100;
  gg_slice t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  float * t109;
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
  uint8_t t126;
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
  uint8_t t137;
  gg_slice t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  float t143;
  float t144;
  float t145;
  float t146;
  float t147;
  float t148;
  float t149;
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
  uint8_t t160;
  gg_slice t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  float t168;
  float t169;
  float t170;
  float t171;
  float t172;
  float t173;
  float t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  uint8_t t179;
  uint8_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  uint8_t t185;
  gg_slice t186;
  int64_t t187;
  int64_t t188;
  float t189;
  float t190;
  float t191;
  float t192;
  float t193;
  float t194;
  float t195;
  gg_slice t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  float t203;
  float t204;
  float t205;
  float t206;
  float t207;
  float t208;
  float t209;
  int64_t t210;
  int64_t t211;
  uint8_t t212;
  gg_slice t213;
  int64_t t214;
  int64_t t215;
  float t216;
  float t217;
  float t218;
  float t219;
  float t220;
  float t221;
  float t222;
  int64_t t223;
  int64_t t224;
  uint8_t t225;
  uint8_t t226;
  int64_t t227;
  int64_t t228;
  uint8_t t229;
  gg_slice t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  float t235;
  float t236;
  float t237;
  float t238;
  float t239;
  float t240;
  float t241;
  int64_t t242;
  int64_t t243;
  uint8_t t244;
  uint8_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  uint8_t t250;
  gg_slice t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  float t258;
  float t259;
  float t260;
  float t261;
  float t262;
  float t263;
  float t264;
  gg_slice t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  float t272;
  float t273;
  float t274;
  float t275;
  float t276;
  float t277;
  float t278;
  float t279;
  float t280;
  float t281;
  float t282;
  int64_t t283;
  float t284;
  float t285;
  gg_slice t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  float * t292;
  gg_slice t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  float * t303;
  gg_slice t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  float * t310;
  float t311;
  int64_t t312;
  float * t313;
  float t314;
  float t315;
  float t316;
  int64_t t317;
  float * t318;
  float t319;
  float t320;
  gg_slice t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  float * t331;
  float t332;
  int64_t t333;
  float * t334;
  float t335;
  float t336;
  float t337;
  int64_t t338;
  float * t339;
  float t340;
  float t341;
  gg_slice v0;
  gg_slice v1;
  gg_slice v2;
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
  float v14;
  float v15;
  float v16;
  float v17;
  float v18;
  float v19;
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
  gg_type_12 v37;
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
  v21 = p21;
  v22 = p22;
  v23 = p23;
  v24 = p24;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  t1 = apex_bdim_y();
  t2 = ((int64_t)(t1));
  t3 = apex_bid_y();
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t4)))));
  t6 = apex_tid_y();
  t7 = ((int64_t)(t6));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t7)))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t0) * (uint64_t)(t8)))));
  t9 = ((int64_t)(v25));
  t10 = ((int64_t)(v3));
  t11 = ((t9) >= (t10));
  if (t11) {
  {
  goto gg_done;
  }
  }
  t12 = apex_bdim_x();
  t13 = ((int64_t)(t12));
  t14 = apex_bid_x();
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t15)))));
  t17 = apex_tid_x();
  t18 = ((int64_t)(t17));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t18)))));
  t19 = ((int64_t)(v26));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(v5));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  v27 = ((int64_t)(((int64_t)gg_div(t19, t22))));
  t23 = ((int64_t)(v26));
  t24 = ((int64_t)(v27));
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(v5));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t23) - (uint64_t)(t28)))));
  t30 = ((int64_t)(v4));
  v28 = ((int64_t)(((int64_t)gg_div(t29, t30))));
  t31 = ((int64_t)(v26));
  t32 = ((int64_t)(v27));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((int64_t)(v5));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t31) - (uint64_t)(t36)))));
  t38 = ((int64_t)(v28));
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t37) - (uint64_t)(t40)))));
  t41 = ((int64_t)(v25));
  t42 = ((int64_t)(((int64_t)UINT64_C(2))));
  t43 = ((int64_t)(((int64_t)gg_div(t41, t42))));
  t44 = ((int64_t)(v29));
  t45 = ((int64_t)(v9));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t46)))));
  t48 = ((int64_t)(v28));
  t49 = ((int64_t)(v10));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t49)))));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t50)))));
  t52 = ((int64_t)(v27));
  t53 = ((int64_t)(v11));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  v30 = ((int64_t)(((int64_t)((uint64_t)(t51) + (uint64_t)(t54)))));
  t55 = ((int64_t)(v25));
  t56 = ((int64_t)(((int64_t)UINT64_C(2))));
  t57 = ((int64_t)(((int64_t)gg_div(t55, t56))));
  t58 = ((int64_t)(v29));
  t59 = ((int64_t)(v6));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t60)))));
  t62 = ((int64_t)(v28));
  t63 = ((int64_t)(v7));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t61) + (uint64_t)(t64)))));
  t66 = ((int64_t)(v27));
  t67 = ((int64_t)(v8));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t68)))));
  t69 = ((int64_t)(v25));
  t70 = ((int64_t)(v13));
  t71 = ((t69) < (t70));
  t72 = t71;
  if (!t72) {
  t73 = ((int64_t)(v25));
  t74 = ((int64_t)(v13));
  t75 = ((int64_t)(v12));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t74) + (uint64_t)(t75)))));
  t72 = ((t73) >= (t76));
  }
  t77 = t72;
  if (t77) {
  {
  t78 = v1;
  t79 = ((int64_t)(v30));
  t80 = ((int64_t)(v25));
  t81 = ((int64_t)(((int64_t)UINT64_C(2))));
  t82 = ((int64_t)(((int64_t)gg_div(t80, t81))));
  t83 = ((int64_t)((uint64_t)(t79) + (uint64_t)(t82)));
  t84 = ((float *)t78.data) + gg_index(t83, t78.len);
  t85 = v0;
  t86 = ((int64_t)(v31));
  t87 = ((int64_t)(v25));
  t88 = ((int64_t)(((int64_t)UINT64_C(2))));
  t89 = ((int64_t)(((int64_t)gg_div(t87, t88))));
  t90 = ((int64_t)((uint64_t)(t86) + (uint64_t)(t89)));
  t91 = ((float *)t85.data) + gg_index(t90, t85.len);
  (*t84) = ((float)((*t91)));
  t92 = v1;
  t93 = ((int64_t)(v30));
  t94 = ((int64_t)(v25));
  t95 = ((int64_t)(((int64_t)UINT64_C(2))));
  t96 = ((int64_t)(((int64_t)gg_div(t94, t95))));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t93) + (uint64_t)(t96)))));
  t98 = ((int64_t)(((int64_t)UINT64_C(1))));
  t99 = ((int64_t)((uint64_t)(t97) + (uint64_t)(t98)));
  t100 = ((float *)t92.data) + gg_index(t99, t92.len);
  t101 = v0;
  t102 = ((int64_t)(v31));
  t103 = ((int64_t)(v25));
  t104 = ((int64_t)(((int64_t)UINT64_C(2))));
  t105 = ((int64_t)(((int64_t)gg_div(t103, t104))));
  t106 = ((int64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t105)))));
  t107 = ((int64_t)(((int64_t)UINT64_C(1))));
  t108 = ((int64_t)((uint64_t)(t106) + (uint64_t)(t107)));
  t109 = ((float *)t101.data) + gg_index(t108, t101.len);
  (*t100) = ((float)((*t109)));
  goto gg_done;
  }
  }
  t110 = ((int64_t)(v25));
  t111 = ((int64_t)(v13));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t110) - (uint64_t)(t111)))));
  t112 = ((int64_t)(v20));
  t113 = ((int64_t)(v21));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) + (uint64_t)(t113)))));
  t115 = ((int64_t)(v22));
  t116 = ((int64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t115)))));
  t117 = ((int64_t)(v23));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t116) + (uint64_t)(t117)))));
  t118 = ((int64_t)(v21));
  t119 = ((int64_t)(v20));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t118) + (uint64_t)(t119)))));
  t120 = ((int64_t)(v32));
  t121 = ((int64_t)(((int64_t)UINT64_C(2))));
  t122 = ((int64_t)(((int64_t)gg_div(t120, t121))));
  t123 = ((int64_t)(v33));
  v35 = ((int64_t)(((int64_t)gg_mod(t122, t123))));
  v36 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t124 = ((int64_t)(v24));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((t124) != (t125));
  if (t126) {
  {
  t127 = ((int64_t)(v35));
  t128 = ((int64_t)(((int64_t)UINT64_C(3))));
  t129 = ((int64_t)(((int64_t)gg_mod(t127, t128))));
  t130 = ((int64_t)(((int64_t)UINT64_C(1))));
  t131 = ((t129) == (t130));
  t132 = t131;
  if (t132) {
  t133 = ((int64_t)(v35));
  t134 = ((int64_t)(((int64_t)UINT64_C(3))));
  t135 = ((int64_t)(v21));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t134) * (uint64_t)(t135)))));
  t132 = ((t133) < (t136));
  }
  t137 = t132;
  if (t137) {
  {
  t138 = v2;
  t139 = ((int64_t)(v28));
  t140 = ((int64_t)(v5));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t139) + (uint64_t)(t140)))));
  t142 = gg_function_8(t138, t141, base + 0);
  t143 = ((float)(((float)(t142))));
  t144 = ((float)(v19));
  t145 = ((float)(((float)(v32))));
  t146 = ((float)(((float)(((int64_t)UINT64_C(2))))));
  t147 = ((float)(((t145) / (t146))));
  t148 = apex_powf(t144, t147);
  t149 = ((float)(t148));
  v36 = ((float)(((t143) * (t149))));
  }
  } else {
  t150 = ((int64_t)(v35));
  t151 = ((int64_t)(((int64_t)UINT64_C(3))));
  t152 = ((int64_t)(((int64_t)gg_mod(t150, t151))));
  t153 = ((int64_t)(((int64_t)UINT64_C(2))));
  t154 = ((t152) == (t153));
  t155 = t154;
  if (t155) {
  t156 = ((int64_t)(v35));
  t157 = ((int64_t)(((int64_t)UINT64_C(3))));
  t158 = ((int64_t)(v22));
  t159 = ((int64_t)(((int64_t)((uint64_t)(t157) * (uint64_t)(t158)))));
  t155 = ((t156) < (t159));
  }
  t160 = t155;
  if (t160) {
  {
  t161 = v2;
  t162 = ((int64_t)(v28));
  t163 = ((int64_t)(v5));
  t164 = ((int64_t)(((int64_t)UINT64_C(2))));
  t165 = ((int64_t)(((int64_t)((uint64_t)(t163) * (uint64_t)(t164)))));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t162) + (uint64_t)(t165)))));
  t167 = gg_function_8(t161, t166, base + 0);
  t168 = ((float)(((float)(t167))));
  t169 = ((float)(v19));
  t170 = ((float)(((float)(v32))));
  t171 = ((float)(((float)(((int64_t)UINT64_C(2))))));
  t172 = ((float)(((t170) / (t171))));
  t173 = apex_powf(t169, t172);
  t174 = ((float)(t173));
  v36 = ((float)(((t168) * (t174))));
  }
  } else {
  t175 = ((int64_t)(v35));
  t176 = ((int64_t)(((int64_t)UINT64_C(3))));
  t177 = ((int64_t)(((int64_t)gg_mod(t175, t176))));
  t178 = ((int64_t)(((int64_t)UINT64_C(0))));
  t179 = ((t177) == (t178));
  t180 = t179;
  if (t180) {
  t181 = ((int64_t)(v35));
  t182 = ((int64_t)(((int64_t)UINT64_C(3))));
  t183 = ((int64_t)(v20));
  t184 = ((int64_t)(((int64_t)((uint64_t)(t182) * (uint64_t)(t183)))));
  t180 = ((t181) < (t184));
  }
  t185 = t180;
  if (t185) {
  {
  t186 = v2;
  t187 = ((int64_t)(v28));
  t188 = gg_function_8(t186, t187, base + 0);
  t189 = ((float)(((float)(t188))));
  t190 = ((float)(v19));
  t191 = ((float)(((float)(v32))));
  t192 = ((float)(((float)(((int64_t)UINT64_C(2))))));
  t193 = ((float)(((t191) / (t192))));
  t194 = apex_powf(t190, t193);
  t195 = ((float)(t194));
  v36 = ((float)(((t189) * (t195))));
  }
  } else {
  {
  t196 = v2;
  t197 = ((int64_t)(v28));
  t198 = ((int64_t)(v5));
  t199 = ((int64_t)(((int64_t)UINT64_C(3))));
  t200 = ((int64_t)(((int64_t)((uint64_t)(t198) * (uint64_t)(t199)))));
  t201 = ((int64_t)(((int64_t)((uint64_t)(t197) + (uint64_t)(t200)))));
  t202 = gg_function_8(t196, t201, base + 0);
  t203 = ((float)(((float)(t202))));
  t204 = ((float)(v19));
  t205 = ((float)(((float)(v32))));
  t206 = ((float)(((float)(((int64_t)UINT64_C(2))))));
  t207 = ((float)(((t205) / (t206))));
  t208 = apex_powf(t204, t207);
  t209 = ((float)(t208));
  v36 = ((float)(((t203) * (t209))));
  }
  }
  }
  }
  }
  } else {
  {
  t210 = ((int64_t)(v35));
  t211 = ((int64_t)(v20));
  t212 = ((t210) < (t211));
  if (t212) {
  {
  t213 = v2;
  t214 = ((int64_t)(v28));
  t215 = gg_function_8(t213, t214, base + 0);
  t216 = ((float)(((float)(t215))));
  t217 = ((float)(v19));
  t218 = ((float)(((float)(v32))));
  t219 = ((float)(((float)(((int64_t)UINT64_C(2))))));
  t220 = ((float)(((t218) / (t219))));
  t221 = apex_powf(t217, t220);
  t222 = ((float)(t221));
  v36 = ((float)(((t216) * (t222))));
  }
  } else {
  t223 = ((int64_t)(v35));
  t224 = ((int64_t)(v20));
  t225 = ((t223) >= (t224));
  t226 = t225;
  if (t226) {
  t227 = ((int64_t)(v35));
  t228 = ((int64_t)(v34));
  t226 = ((t227) < (t228));
  }
  t229 = t226;
  if (t229) {
  {
  t230 = v2;
  t231 = ((int64_t)(v28));
  t232 = ((int64_t)(v5));
  t233 = ((int64_t)(((int64_t)((uint64_t)(t231) + (uint64_t)(t232)))));
  t234 = gg_function_8(t230, t233, base + 0);
  t235 = ((float)(((float)(t234))));
  t236 = ((float)(v19));
  t237 = ((float)(((float)(v32))));
  t238 = ((float)(((float)(((int64_t)UINT64_C(2))))));
  t239 = ((float)(((t237) / (t238))));
  t240 = apex_powf(t236, t239);
  t241 = ((float)(t240));
  v36 = ((float)(((t235) * (t241))));
  }
  } else {
  t242 = ((int64_t)(v35));
  t243 = ((int64_t)(v34));
  t244 = ((t242) >= (t243));
  t245 = t244;
  if (t245) {
  t246 = ((int64_t)(v35));
  t247 = ((int64_t)(v34));
  t248 = ((int64_t)(v22));
  t249 = ((int64_t)(((int64_t)((uint64_t)(t247) + (uint64_t)(t248)))));
  t245 = ((t246) < (t249));
  }
  t250 = t245;
  if (t250) {
  {
  t251 = v2;
  t252 = ((int64_t)(v28));
  t253 = ((int64_t)(v5));
  t254 = ((int64_t)(((int64_t)UINT64_C(2))));
  t255 = ((int64_t)(((int64_t)((uint64_t)(t253) * (uint64_t)(t254)))));
  t256 = ((int64_t)(((int64_t)((uint64_t)(t252) + (uint64_t)(t255)))));
  t257 = gg_function_8(t251, t256, base + 0);
  t258 = ((float)(((float)(t257))));
  t259 = ((float)(v19));
  t260 = ((float)(((float)(v32))));
  t261 = ((float)(((float)(((int64_t)UINT64_C(2))))));
  t262 = ((float)(((t260) / (t261))));
  t263 = apex_powf(t259, t262);
  t264 = ((float)(t263));
  v36 = ((float)(((t258) * (t264))));
  }
  } else {
  {
  t265 = v2;
  t266 = ((int64_t)(v28));
  t267 = ((int64_t)(v5));
  t268 = ((int64_t)(((int64_t)UINT64_C(3))));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t267) * (uint64_t)(t268)))));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t266) + (uint64_t)(t269)))));
  t271 = gg_function_8(t265, t270, base + 0);
  t272 = ((float)(((float)(t271))));
  t273 = ((float)(v19));
  t274 = ((float)(((float)(v32))));
  t275 = ((float)(((float)(((int64_t)UINT64_C(2))))));
  t276 = ((float)(((t274) / (t275))));
  t277 = apex_powf(t273, t276);
  t278 = ((float)(t277));
  v36 = ((float)(((t272) * (t278))));
  }
  }
  }
  }
  }
  }
  t279 = ((float)(v36));
  t280 = ((float)(v14));
  t281 = ((float)(v17));
  t282 = ((float)(v18));
  t283 = ((int64_t)(v32));
  t284 = ((float)(v15));
  t285 = ((float)(v16));
  v37 = gg_function_10(t279, t280, t281, t282, t283, t284, t285, base + 0);
  t286 = v0;
  t287 = ((int64_t)(v31));
  t288 = ((int64_t)(v13));
  t289 = ((int64_t)(((int64_t)UINT64_C(2))));
  t290 = ((int64_t)(((int64_t)gg_div(t288, t289))));
  t291 = ((int64_t)((uint64_t)(t287) + (uint64_t)(t290)));
  t292 = ((float *)t286.data) + gg_index(t291, t286.len);
  v38 = ((float)((*t292)));
  t293 = v0;
  t294 = ((int64_t)(v31));
  t295 = ((int64_t)(v13));
  t296 = ((int64_t)(((int64_t)UINT64_C(2))));
  t297 = ((int64_t)(((int64_t)gg_div(t295, t296))));
  t298 = ((int64_t)(((int64_t)((uint64_t)(t294) + (uint64_t)(t297)))));
  t299 = ((int64_t)(v12));
  t300 = ((int64_t)(((int64_t)UINT64_C(2))));
  t301 = ((int64_t)(((int64_t)gg_div(t299, t300))));
  t302 = ((int64_t)((uint64_t)(t298) + (uint64_t)(t301)));
  t303 = ((float *)t293.data) + gg_index(t302, t293.len);
  v39 = ((float)((*t303)));
  t304 = v1;
  t305 = ((int64_t)(v30));
  t306 = ((int64_t)(v13));
  t307 = ((int64_t)(((int64_t)UINT64_C(2))));
  t308 = ((int64_t)(((int64_t)gg_div(t306, t307))));
  t309 = ((int64_t)((uint64_t)(t305) + (uint64_t)(t308)));
  t310 = ((float *)t304.data) + gg_index(t309, t304.len);
  t311 = ((float)(v38));
  /* FG_DIRECT_SLICE_INDEX */
  t312 = ((int64_t)UINT64_C(0));
  t313 = ((float *)(v37).data) + gg_index(t312, ((int64_t)UINT64_C(2)));
  t314 = ((float)((*t313)));
  t315 = ((float)(((t311) * (t314))));
  t316 = ((float)(v39));
  /* FG_DIRECT_SLICE_INDEX */
  t317 = ((int64_t)UINT64_C(1));
  t318 = ((float *)(v37).data) + gg_index(t317, ((int64_t)UINT64_C(2)));
  t319 = ((float)((*t318)));
  t320 = ((float)(((t316) * (t319))));
  (*t310) = ((float)(((t315) - (t320))));
  t321 = v1;
  t322 = ((int64_t)(v30));
  t323 = ((int64_t)(v13));
  t324 = ((int64_t)(((int64_t)UINT64_C(2))));
  t325 = ((int64_t)(((int64_t)gg_div(t323, t324))));
  t326 = ((int64_t)(((int64_t)((uint64_t)(t322) + (uint64_t)(t325)))));
  t327 = ((int64_t)(v12));
  t328 = ((int64_t)(((int64_t)UINT64_C(2))));
  t329 = ((int64_t)(((int64_t)gg_div(t327, t328))));
  t330 = ((int64_t)((uint64_t)(t326) + (uint64_t)(t329)));
  t331 = ((float *)t321.data) + gg_index(t330, t321.len);
  t332 = ((float)(v38));
  /* FG_DIRECT_SLICE_INDEX */
  t333 = ((int64_t)UINT64_C(1));
  t334 = ((float *)(v37).data) + gg_index(t333, ((int64_t)UINT64_C(2)));
  t335 = ((float)((*t334)));
  t336 = ((float)(((t332) * (t335))));
  t337 = ((float)(v39));
  /* FG_DIRECT_SLICE_INDEX */
  t338 = ((int64_t)UINT64_C(0));
  t339 = ((float *)(v37).data) + gg_index(t338, ((int64_t)UINT64_C(2)));
  t340 = ((float)((*t339)));
  t341 = ((float)(((t337) * (t340))));
  (*t331) = ((float)(((t336) + (t341))));
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  float t0;
  float t1;
  int64_t t2;
  float t3;
  float t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  gg_slice t9;
  gg_slice t10;
  gg_slice t11;
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
  float t23;
  float t24;
  float t25;
  float t26;
  float t27;
  float t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  gg_header t34;
  int64_t t35;
  float * t36;
  int64_t t37;
  gg_type_2 v0;
  gg_type_2 v1;
  gg_type_4 v2;
  {
  memset(&t0, 0, sizeof(t0));
  t1 = ((float)(t0));
  for (t2=0; t2<4096; t2++) (v0).data[t2] = t1;
  memset(&t3, 0, sizeof(t3));
  t4 = ((float)(t3));
  for (t5=0; t5<4096; t5++) (v1).data[t5] = t4;
  memset(&t6, 0, sizeof(t6));
  t7 = ((uint8_t)(t6));
  for (t8=0; t8<64; t8++) (v2).data[t8] = t7;
  t9 = gg_view((v0).data, 4096);
  t10 = gg_view((v1).data, 4096);
  t11 = gg_view((v2).data, 64);
  t12 = ((int64_t)(((int64_t)UINT64_C(128))));
  t13 = ((int64_t)(((int64_t)UINT64_C(32))));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(((int64_t)UINT64_C(128))));
  t16 = ((int64_t)(((int64_t)UINT64_C(4096))));
  t17 = ((int64_t)(((int64_t)UINT64_C(4096))));
  t18 = ((int64_t)(((int64_t)UINT64_C(128))));
  t19 = ((int64_t)(((int64_t)UINT64_C(4096))));
  t20 = ((int64_t)(((int64_t)UINT64_C(4096))));
  t21 = ((int64_t)(((int64_t)UINT64_C(128))));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t24 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t25 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t26 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t27 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t28 = ((float)(((float)(((int64_t)UINT64_C(10000))))));
  t29 = ((int64_t)(((int64_t)UINT64_C(16))));
  t30 = ((int64_t)(((int64_t)UINT64_C(24))));
  t31 = ((int64_t)(((int64_t)UINT64_C(24))));
  t32 = ((int64_t)(((int64_t)UINT64_C(24))));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_1(t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, t20, t21, t22, t23, t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, base + 0);
  t34.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t34.len = 0;
  /* FG_DIRECT_SLICE_INDEX */
  t35 = ((int64_t)UINT64_C(0));
  t36 = ((float *)(v1).data) + gg_index(t35, ((int64_t)UINT64_C(4096)));
  gg_float32(gg_ref(&t34, gg_get_stack(base + 0)), (*t36));
  gg_append(gg_ref(&t34, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t34).base, (t34).len));
  }
gg_done:;
  for (t37 = 0; t37 < 1; t37++) gg_stacks[base + t37].top = 0;
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
