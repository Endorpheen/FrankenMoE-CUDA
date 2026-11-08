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
#if defined(__CUDACC__)
#include <cuda_runtime.h>
#endif
static __host__ __device__ void gg_fail(const char *message) {
#if defined(__CUDA_ARCH__)
    (void) message;
    __trap();
#else
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
#endif
}
static gg_stack *gg_get_stack(int64_t id) {
    gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) gg_fail("data stack limit exceeded");
    s = &gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = (unsigned char *) mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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
static __host__ __device__ gg_slice gg_view(void *p, int64_t n) { gg_slice v; v.data=p; v.len=n; return v; }
static __host__ __device__ gg_slice gg_byte_view(gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) gg_fail("byte view size overflow");
    return gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static __host__ __device__ gg_slice gg_sub(gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) gg_fail("slice out of bounds");
    return gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static __host__ __device__ int64_t gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) gg_fail("array index out of bounds"); return i; }
static void gg_append(gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) gg_fail("invalid array size");
    to=(unsigned char *) gg_allocate(a.stack, (size_t)n * element);
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
        p=(unsigned char *) gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static __host__ __device__ int64_t gg_div(int64_t a, int64_t b) { if (!b) gg_fail("division by zero"); if (a==INT64_MIN && b==-1) gg_fail("division overflow"); return a/b; }
static __host__ __device__ int64_t gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __host__ __device__ uint64_t gg_udiv(uint64_t a,uint64_t b) { if(!b)gg_fail("division by zero");return a/b; }
static __host__ __device__ uint64_t gg_umod(uint64_t a,uint64_t b) { if(!b)gg_fail("division by zero");return a%b; }
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
    char *s=(char *) malloc((size_t)path.len+1); if(!s)gg_fail("out of memory in file shim");
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
typedef struct { uint8_t data[8192]; } gg_type_2;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[1024]; } gg_type_4;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[512]; } gg_type_5;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, uint8_t value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(float a, float b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, float value);
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_type_4 value);
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_type_5 value);
static uint8_t gg_eq_6(int64_t a, int64_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, int64_t value);
static uint8_t gg_eq_7(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_slice value);
static uint8_t gg_eq_8(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_slice value);
static uint8_t gg_eq_9(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, uint8_t value);
static uint8_t gg_eq_11(uint16_t a, uint16_t b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, uint16_t value);
static uint8_t gg_eq_12(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, uint32_t value);
static uint8_t gg_eq_13(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_slice value);
static uint8_t gg_eq_14(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, gg_slice value);
static uint8_t gg_eq_15(int8_t a, int8_t b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, int8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((gg_type_2*)a.data)[i],((gg_type_2*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b) {
return gg_equal_elements_1(gg_view((a).data, 8192),gg_view((b).data, 8192));
}
static void gg_render_2(gg_reference out, gg_type_2 value) {
gg_text(out,gg_view((value).data, 8192));
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(float a, float b) {
return a == b;
}
static void gg_render_3(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_type_4*)a.data)[i],((gg_type_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b) {
return gg_equal_elements_3(gg_view((a).data, 1024),gg_view((b).data, 1024));
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
int64_t i; gg_slice view=gg_view((value).data, 1024);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_type_5*)a.data)[i],((gg_type_5*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_type_5 a, gg_type_5 b) {
return gg_equal_elements_3(gg_view((a).data, 512),gg_view((b).data, 512));
}
static void gg_render_5(gg_reference out, gg_type_5 value) {
int64_t i; gg_slice view=gg_view((value).data, 512);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_7(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_8(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_9(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((uint16_t*)a.data)[i],((uint16_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(uint16_t a, uint16_t b) {
return a == b;
}
static void gg_render_11(gg_reference out, uint16_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_12(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_13(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_slice a, gg_slice b) {
return gg_equal_elements_3(a,b);
}
static void gg_render_14(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_3(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((int8_t*)a.data)[i],((int8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(int8_t a, int8_t b) {
return a == b;
}
static void gg_render_15(gg_reference out, int8_t value) {
gg_integer(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static __host__ __device__ int64_t gg_function_1(gg_slice p0, gg_slice p1, int64_t base);
static __host__ __device__ uint8_t gg_function_3(gg_slice p0, int64_t p1, float p2, int64_t base);
static __host__ __device__ void gg_function_7(gg_slice p0, int64_t p1, uint16_t p2, int64_t base);
static __host__ __device__ int64_t gg_function_8(gg_slice p0, gg_slice p1, int64_t p2, int64_t base);
static __host__ __device__ uint8_t gg_function_9(gg_slice p0, int64_t p1, float p2, int64_t base);
static __host__ __device__ int64_t gg_function_10(float p0, float p1, int64_t p2, gg_slice p3, int64_t p4, int64_t p5, int64_t base);
static __host__ __device__ void gg_function_12(gg_slice p0, int64_t p1, uint32_t p2, int64_t base);
static __host__ __device__ int64_t gg_function_13(int64_t p0, gg_slice p1, int64_t p2, float p3, gg_slice p4, int64_t p5, int64_t base);
static __host__ __device__ int64_t gg_function_14(float p0, float p1, gg_slice p2, int64_t p3, int64_t base);
static __host__ __device__ int64_t gg_function_15(int64_t p0, gg_slice p1, int64_t p2, gg_slice p3, int64_t p4, gg_slice p5, int64_t base);
static __host__ __device__ int64_t gg_function_16(int64_t p0, int64_t base);
static __host__ __device__ float gg_function_17(gg_slice p0, int64_t p1, gg_slice p2, int64_t p3, int64_t base);
static __host__ __device__ float gg_function_18(gg_slice p0, int64_t p1, int64_t base);
static __host__ __device__ uint16_t gg_function_19(gg_slice p0, int64_t p1, int64_t base);
static __host__ __device__ float gg_function_21(gg_slice p0, int64_t p1, int64_t base);
static __host__ __device__ int64_t gg_function_22(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static __host__ __device__ int64_t gg_function_23(gg_slice p0, int64_t p1, int64_t base);
static __host__ __device__ int64_t gg_function_24(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
static __host__ __device__ int64_t gg_function_25(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
static __host__ __device__ float gg_function_26(gg_slice p0, int64_t p1, gg_slice p2, int64_t p3, int64_t base);
static __host__ __device__ float gg_function_27(gg_slice p0, int64_t p1, int64_t base);
static __host__ __device__ uint32_t gg_function_28(gg_slice p0, int64_t p1, int64_t base);
static __host__ __device__ int64_t gg_function_30(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
static __host__ __device__ int64_t gg_function_31(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_slice p6, int64_t p7, int64_t base);
static __host__ __device__ int64_t gg_function_32(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, gg_slice p5, int64_t base);
static __host__ __device__ int64_t gg_function_33(int64_t p0, int64_t p1, int64_t p2, gg_slice p3, int64_t base);
static __host__ __device__ int64_t gg_function_34(int64_t p0, int64_t base);
static __host__ __device__ int64_t gg_function_35(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, gg_slice p7, int64_t base);
static __host__ __device__ int64_t gg_function_36(int64_t p0, int64_t p1, int64_t base);
static __host__ __device__ int64_t gg_function_37(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static __host__ __device__ int64_t gg_function_38(int64_t p0, int64_t p1, int64_t base);
static __host__ __device__ int64_t gg_function_39(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, gg_slice p8, int64_t base);
static __host__ __device__ int64_t gg_function_40(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static __host__ __device__ float gg_function_41(gg_slice p0, int64_t p1, int64_t base);
static __host__ __device__ int64_t gg_function_42(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static __host__ __device__ int64_t gg_function_34(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(12))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(160))));
  }
  } else {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(14))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(224))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_33(int64_t p0, int64_t p1, int64_t p2, gg_slice p3, int64_t base) {
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
  uint8_t t10;
  uint8_t t11;
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
  int64_t t24;
  int64_t t25;
  float * t26;
  int64_t t27;
  float * t28;
  int64_t t29;
  float * t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(105))));
  t2 = ((int64_t)(v0));
  fgc_witness(t1, t2);
  t3 = ((int64_t)(v0));
  v4 = gg_function_34(t3, base + 0);
  t4 = ((int64_t)((v3).len));
  t5 = ((int64_t)(((int64_t)UINT64_C(3))));
  t6 = ((t4) < (t5));
  t7 = t6;
  if (!t7) {
  t8 = ((int64_t)(v4));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) == (t9));
  }
  t10 = t7;
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) <= (t13));
  }
  t14 = t11;
  t15 = t14;
  if (!t15) {
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) <= (t17));
  }
  t18 = t15;
  if (t18) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(((int64_t)UINT64_C(288))));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t24)))));
  /* FG_DIRECT_SLICE_INDEX */
  t25 = ((int64_t)UINT64_C(0));
  t26 = ((float *)(v3).data) + gg_index(t25, (v3).len);
  (*t26) = ((float)(((float)(v5))));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = ((int64_t)UINT64_C(1));
  t28 = ((float *)(v3).data) + gg_index(t27, (v3).len);
  (*t28) = ((float)(((float)(((int64_t)UINT64_C(9472))))));
  /* FG_DIRECT_SLICE_INDEX */
  t29 = ((int64_t)UINT64_C(2));
  t30 = ((float *)(v3).data) + gg_index(t29, (v3).len);
  t31 = ((int64_t)(v5));
  t32 = ((int64_t)(((int64_t)UINT64_C(9472))));
  t33 = ((t31) <= (t32));
  if (t33) {
  {
  (*t30) = ((float)(((float)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  (*t30) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  }
  t34 = ((int64_t)(v5));
  t35 = ((int64_t)(((int64_t)UINT64_C(9472))));
  t36 = ((t34) <= (t35));
  if (t36) {
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
static __host__ __device__ int64_t gg_function_23(gg_slice p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t * t8;
  gg_slice v0;
  int64_t v1;
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
  t4 = ((t5) < (t6));
  }
  if (!(t4)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t7 = v1;
  t8 = ((uint8_t *)(v0).data) + gg_index(t7, (v0).len);
  t0 = ((int64_t)(((int64_t)(((int8_t)((*t8)))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_30(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
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
  gg_slice t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t * t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  gg_slice t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t * t42;
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
  gg_slice v0;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(128))));
  v3 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(128))));
  v4 = ((int64_t)(((int64_t)gg_mod(t3, t4))));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(32))));
  v5 = ((int64_t)(((int64_t)gg_mod(t5, t6))));
  t7 = ((int64_t)(v4));
  t8 = ((int64_t)(((int64_t)UINT64_C(32))));
  v6 = ((int64_t)(((int64_t)gg_div(t7, t8))));
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((int64_t)(((int64_t)gg_mod(t9, t10))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) == (t12));
  if (t13) {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(32))));
  }
  }
  t14 = v0;
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(64))));
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t18)))));
  t20 = ((int64_t)(v5));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t22 = ((int64_t)(v7));
  t23 = ((int64_t)((uint64_t)(t21) + (uint64_t)(t22)));
  t24 = ((uint8_t *)t14.data) + gg_index(t23, t14.len);
  v8 = ((int64_t)(((int64_t)((*t24)))));
  t25 = ((int64_t)(v6));
  t26 = ((int64_t)(((int64_t)UINT64_C(2))));
  t27 = ((t25) < (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v8));
  t29 = ((int64_t)(((int64_t)UINT64_C(15))));
  v9 = ((int64_t)(((t28) & (t29))));
  }
  } else {
  {
  t30 = ((int64_t)(v8));
  t31 = ((int64_t)(((int64_t)UINT64_C(4))));
  v9 = ((int64_t)(((t30) >> ((uint64_t)(t31) & 63))));
  }
  }
  t32 = v0;
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)UINT64_C(128))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
  t36 = ((int64_t)(((int64_t)UINT64_C(32))));
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t38)))));
  t40 = ((int64_t)(v5));
  t41 = ((int64_t)((uint64_t)(t39) + (uint64_t)(t40)));
  t42 = ((uint8_t *)t32.data) + gg_index(t41, t32.len);
  v10 = ((int64_t)(((int64_t)((*t42)))));
  t43 = ((int64_t)(v9));
  t44 = ((int64_t)(v10));
  t45 = ((int64_t)(((int64_t)UINT64_C(2))));
  t46 = ((int64_t)(v6));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = ((int64_t)(((t44) >> ((uint64_t)(t47) & 63))));
  t49 = ((int64_t)(((int64_t)UINT64_C(3))));
  t50 = ((int64_t)(((t48) & (t49))));
  t51 = ((int64_t)(((int64_t)UINT64_C(4))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) << ((uint64_t)(t51) & 63)))));
  t0 = ((int64_t)(((t43) | (t52))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ uint16_t gg_function_19(gg_slice p0, int64_t p1, int64_t base) {
  uint16_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t * t10;
  uint16_t t11;
  gg_slice t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t * t16;
  uint16_t t17;
  int64_t t18;
  uint16_t t19;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t8 = ((int64_t)((v0).len));
  t4 = ((t7) < (t8));
  }
  if (!(t4)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t9 = v1;
  t10 = ((uint8_t *)(v0).data) + gg_index(t9, (v0).len);
  t11 = ((uint16_t)(((uint16_t)((*t10)))));
  t12 = v0;
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)((uint64_t)(t13) + (uint64_t)(t14)));
  t16 = ((uint8_t *)t12.data) + gg_index(t15, t12.len);
  t17 = ((uint16_t)(((uint16_t)((*t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  t19 = ((uint16_t)(((uint16_t)((uint64_t)(t17) << ((uint64_t)(t18) & 15)))));
  t0 = ((uint16_t)(((t11) | (t19))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ uint32_t gg_function_28(gg_slice p0, int64_t p1, int64_t base) {
  uint32_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  gg_slice t9;
  int64_t t10;
  uint16_t t11;
  uint32_t t12;
  gg_slice t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint16_t t17;
  uint32_t t18;
  int64_t t19;
  uint32_t t20;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(3))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))));
  t8 = ((int64_t)((v0).len));
  t4 = ((t7) < (t8));
  }
  if (!(t4)) gg_fail("assertion failed");
  t9 = v0;
  t10 = ((int64_t)(v1));
  t11 = gg_function_19(t9, t10, base + 0);
  t12 = ((uint32_t)(((uint32_t)(t11))));
  t13 = v0;
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(((int64_t)UINT64_C(2))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t17 = gg_function_19(t13, t16, base + 0);
  t18 = ((uint32_t)(((uint32_t)(t17))));
  t19 = ((int64_t)(((int64_t)UINT64_C(16))));
  t20 = ((uint32_t)(((uint32_t)((uint64_t)(t18) << ((uint64_t)(t19) & 31)))));
  t0 = ((uint32_t)(((t12) | (t20))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ float gg_function_27(gg_slice p0, int64_t p1, int64_t base) {
  float t0;
  gg_slice t1;
  int64_t t2;
  uint32_t t3;
  float t4;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  t3 = gg_function_28(t1, t2, base + 0);
  t4 = fgc_from_bits(t3);
  t0 = ((float)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ float gg_function_18(gg_slice p0, int64_t p1, int64_t base) {
  float t0;
  gg_slice t1;
  int64_t t2;
  uint16_t t3;
  float t4;
  gg_slice v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  t3 = gg_function_19(t1, t2, base + 0);
  t4 = fgc_half(t3);
  t0 = ((float)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ float gg_function_26(gg_slice p0, int64_t p1, gg_slice p2, int64_t p3, int64_t base) {
  float t0;
  gg_slice t1;
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
  gg_slice t13;
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
  gg_slice t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  gg_slice t48;
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
  gg_slice t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  float t69;
  float t70;
  float t71;
  float t72;
  float t73;
  float t74;
  int64_t t75;
  int64_t t76;
  float t77;
  float t78;
  gg_slice v0;
  int64_t v1;
  gg_slice v2;
  int64_t v3;
  float v4;
  float v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  float v10;
  int64_t v11;
  float v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)UINT64_C(208))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  v4 = gg_function_18(t1, t4, base + 0);
  v5 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t5 = ((int64_t)(v6));
  t6 = ((int64_t)(((int64_t)UINT64_C(16))));
  if (!(((t5) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v6));
  t8 = ((int64_t)(((int64_t)UINT64_C(2))));
  v7 = ((int64_t)(((int64_t)gg_div(t7, t8))));
  t9 = ((int64_t)(v7));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  v8 = ((int64_t)(((int64_t)gg_div(t9, t10))));
  t11 = ((int64_t)(v7));
  t12 = ((int64_t)(((int64_t)UINT64_C(4))));
  v9 = ((int64_t)(((int64_t)gg_mod(t11, t12))));
  t13 = v2;
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(v8));
  t16 = ((int64_t)(((int64_t)UINT64_C(144))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t17)))));
  t19 = ((int64_t)(v9));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t21)))));
  v10 = gg_function_27(t13, t22, base + 0);
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(v8));
  t25 = ((int64_t)(((int64_t)UINT64_C(144))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t26)))));
  t28 = ((int64_t)(((int64_t)UINT64_C(16))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = ((int64_t)(v9));
  t31 = ((int64_t)(((int64_t)UINT64_C(32))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t32)))));
  t34 = ((int64_t)(v6));
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t36 = ((int64_t)(((int64_t)gg_mod(t34, t35))));
  t37 = ((int64_t)(((int64_t)UINT64_C(16))));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t38)))));
  t39 = v0;
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(((int64_t)UINT64_C(192))));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))));
  t43 = ((int64_t)(v6));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = gg_function_23(t39, t44, base + 0);
  v12 = ((float)(((float)(t45))));
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t46 = ((int64_t)(v14));
  t47 = ((int64_t)(((int64_t)UINT64_C(16))));
  if (!(((t46) < (t47)))) goto label5;
  {
  t48 = v0;
  t49 = ((int64_t)(v1));
  t50 = ((int64_t)(v6));
  t51 = ((int64_t)(((int64_t)UINT64_C(16))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(v14));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = gg_function_30(t48, t49, t54, base + 0);
  t56 = ((int64_t)(t55));
  t57 = ((int64_t)(((int64_t)UINT64_C(32))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t56) - (uint64_t)(t57)))));
  t58 = v13;
  t59 = ((int64_t)(v15));
  t60 = v2;
  t61 = ((int64_t)(v11));
  t62 = ((int64_t)(v14));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) + (uint64_t)(t62)))));
  t64 = gg_function_23(t60, t63, base + 0);
  t65 = ((int64_t)(t64));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t65)))));
  v13 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t66)));
  t67 = v14;
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  v14 = ((int64_t)((uint64_t)(t67) + (uint64_t)(t68)));
  }
  goto label4;
label5:;
  t69 = ((float)(v5));
  t70 = ((float)(v10));
  t71 = ((float)(((float)(v13))));
  t72 = ((float)(v12));
  t73 = ((float)(((t71) * (t72))));
  t74 = ((float)(((t70) * (t73))));
  v5 = ((float)(((t69) + (t74))));
  t75 = v6;
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t75) + (uint64_t)(t76)));
  }
  goto label1;
label2:;
  t77 = ((float)(v4));
  t78 = ((float)(v5));
  t0 = ((float)(((t77) * (t78))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_25(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t * t12;
  int64_t t13;
  int64_t t14;
  gg_slice t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t * t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  gg_slice t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t * t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(4))));
  t5 = ((t3) < (t4));
  if (t5) {
  {
  t6 = v0;
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  t11 = ((int64_t)((uint64_t)(t9) + (uint64_t)(t10)));
  t12 = ((uint8_t *)t6.data) + gg_index(t11, t6.len);
  t13 = ((int64_t)(((int64_t)((*t12)))));
  t14 = ((int64_t)(((int64_t)UINT64_C(63))));
  t0 = ((int64_t)(((t13) & (t14))));
  goto gg_done;
  }
  }
  t15 = v0;
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  t21 = ((uint8_t *)t15.data) + gg_index(t20, t15.len);
  t22 = ((int64_t)(((int64_t)((*t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(4))));
  t24 = ((int64_t)(((t22) >> ((uint64_t)(t23) & 63))));
  t25 = v0;
  t26 = ((int64_t)(v3));
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)((uint64_t)(t26) + (uint64_t)(t27)));
  t29 = ((uint8_t *)t25.data) + gg_index(t28, t25.len);
  t30 = ((int64_t)(((int64_t)((*t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(6))));
  t32 = ((int64_t)(((t30) >> ((uint64_t)(t31) & 63))));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) << ((uint64_t)(t33) & 63)))));
  t0 = ((int64_t)(((t24) | (t34))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_24(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  gg_slice t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t * t10;
  int64_t t11;
  int64_t t12;
  gg_slice t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t * t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  gg_slice t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t * t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t1) + (uint64_t)(t2)))));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(4))));
  t5 = ((t3) < (t4));
  if (t5) {
  {
  t6 = v0;
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)((uint64_t)(t7) + (uint64_t)(t8)));
  t10 = ((uint8_t *)t6.data) + gg_index(t9, t6.len);
  t11 = ((int64_t)(((int64_t)((*t10)))));
  t12 = ((int64_t)(((int64_t)UINT64_C(63))));
  t0 = ((int64_t)(((t11) & (t12))));
  goto gg_done;
  }
  }
  t13 = v0;
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  t18 = ((int64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  t19 = ((uint8_t *)t13.data) + gg_index(t18, t13.len);
  t20 = ((int64_t)(((int64_t)((*t19)))));
  t21 = ((int64_t)(((int64_t)UINT64_C(15))));
  t22 = ((int64_t)(((t20) & (t21))));
  t23 = v0;
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)UINT64_C(4))));
  t28 = ((int64_t)((uint64_t)(t26) - (uint64_t)(t27)));
  t29 = ((uint8_t *)t23.data) + gg_index(t28, t23.len);
  t30 = ((int64_t)(((int64_t)((*t29)))));
  t31 = ((int64_t)(((int64_t)UINT64_C(6))));
  t32 = ((int64_t)(((t30) >> ((uint64_t)(t31) & 63))));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) << ((uint64_t)(t33) & 63)))));
  t0 = ((int64_t)(((t22) | (t34))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_22(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  gg_slice t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t * t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  gg_slice v0;
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
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  v4 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  v5 = ((int64_t)(((int64_t)gg_mod(t3, t4))));
  t5 = v0;
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(16))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t9 = ((int64_t)(((int64_t)UINT64_C(32))));
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t11)))));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)((uint64_t)(t12) + (uint64_t)(t13)));
  t15 = ((uint8_t *)t5.data) + gg_index(t14, t5.len);
  v6 = ((int64_t)(((int64_t)((*t15)))));
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)UINT64_C(15))));
  t0 = ((int64_t)(((t19) & (t20))));
  }
  } else {
  {
  t21 = ((int64_t)(v6));
  t22 = ((int64_t)(((int64_t)UINT64_C(4))));
  t0 = ((int64_t)(((t21) >> ((uint64_t)(t22) & 63))));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ float gg_function_21(gg_slice p0, int64_t p1, int64_t base) {
  /* FG_SHARED_BODY:18 */
  return gg_function_18(p0, p1, base);
}
static __host__ __device__ float gg_function_17(gg_slice p0, int64_t p1, gg_slice p2, int64_t p3, int64_t base) {
  float t0;
  gg_slice t1;
  int64_t t2;
  gg_slice t3;
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
  gg_slice t21;
  int64_t t22;
  gg_slice t23;
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
  gg_slice t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  gg_slice t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  gg_slice t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  gg_slice t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  float t63;
  float t64;
  float t65;
  float t66;
  float t67;
  float t68;
  float t69;
  float t70;
  float t71;
  float t72;
  int64_t t73;
  int64_t t74;
  float t75;
  float t76;
  float t77;
  float t78;
  float t79;
  float t80;
  gg_slice v0;
  int64_t v1;
  gg_slice v2;
  int64_t v3;
  float v4;
  float v5;
  float v6;
  float v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  float v12;
  float v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  float v17;
  float v18;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = v0;
  t2 = ((int64_t)(v1));
  v4 = gg_function_18(t1, t2, base + 0);
  t3 = v0;
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(2))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  v5 = gg_function_18(t3, t6, base + 0);
  v6 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v7 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t7 = ((int64_t)(v8));
  t8 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t7) < (t8)))) goto label2;
  {
  t9 = ((int64_t)(v8));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  v9 = ((int64_t)(((int64_t)gg_div(t9, t10))));
  t11 = ((int64_t)(v8));
  t12 = ((int64_t)(((int64_t)UINT64_C(4))));
  v10 = ((int64_t)(((int64_t)gg_mod(t11, t12))));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(v9));
  t15 = ((int64_t)(((int64_t)UINT64_C(144))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t16)))));
  t18 = ((int64_t)(v10));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t20)))));
  t21 = v2;
  t22 = ((int64_t)(v11));
  v12 = gg_function_18(t21, t22, base + 0);
  t23 = v2;
  t24 = ((int64_t)(v11));
  t25 = ((int64_t)(((int64_t)UINT64_C(2))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  v13 = gg_function_18(t23, t26, base + 0);
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(v9));
  t29 = ((int64_t)(((int64_t)UINT64_C(144))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)UINT64_C(16))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = ((int64_t)(v10));
  t35 = ((int64_t)(((int64_t)UINT64_C(32))));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t36)))));
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t37 = ((int64_t)(v16));
  t38 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t37) < (t38)))) goto label5;
  {
  t39 = v15;
  t40 = v0;
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(v8));
  t43 = ((int64_t)(v16));
  t44 = gg_function_22(t40, t41, t42, t43, base + 0);
  t45 = ((int64_t)(t44));
  t46 = v2;
  t47 = ((int64_t)(v14));
  t48 = ((int64_t)(v16));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t50 = gg_function_23(t46, t49, base + 0);
  t51 = ((int64_t)(t50));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t51)))));
  v15 = ((int64_t)((uint64_t)(t39) + (uint64_t)(t52)));
  t53 = v16;
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)((uint64_t)(t53) + (uint64_t)(t54)));
  }
  goto label4;
label5:;
  t55 = v0;
  t56 = ((int64_t)(v1));
  t57 = ((int64_t)(v8));
  t58 = gg_function_24(t55, t56, t57, base + 0);
  v17 = ((float)(((float)(t58))));
  t59 = v0;
  t60 = ((int64_t)(v1));
  t61 = ((int64_t)(v8));
  t62 = gg_function_25(t59, t60, t61, base + 0);
  v18 = ((float)(((float)(t62))));
  t63 = ((float)(v6));
  t64 = ((float)(v12));
  t65 = ((float)(((float)(v15))));
  t66 = ((float)(v17));
  t67 = ((float)(((t65) * (t66))));
  t68 = ((float)(((t64) * (t67))));
  v6 = ((float)(((t63) + (t68))));
  t69 = ((float)(v7));
  t70 = ((float)(v13));
  t71 = ((float)(v18));
  t72 = ((float)(((t70) * (t71))));
  v7 = ((float)(((t69) + (t72))));
  t73 = v8;
  t74 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t73) + (uint64_t)(t74)));
  }
  goto label1;
label2:;
  t75 = ((float)(v4));
  t76 = ((float)(v6));
  t77 = ((float)(((t75) * (t76))));
  t78 = ((float)(v5));
  t79 = ((float)(v7));
  t80 = ((float)(((t78) * (t79))));
  t0 = ((float)(((t77) - (t80))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_16(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(12))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(144))));
  }
  } else {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(14))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(210))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_15(int64_t p0, gg_slice p1, int64_t p2, gg_slice p3, int64_t p4, gg_slice p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
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
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  float * t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  gg_slice t35;
  int64_t t36;
  gg_slice t37;
  int64_t t38;
  gg_slice t39;
  int64_t t40;
  gg_slice t41;
  int64_t t42;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  gg_slice v3;
  int64_t v4;
  gg_slice v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(102))));
  t2 = ((int64_t)(v0));
  fgc_witness(t1, t2);
  t3 = ((int64_t)((v5).len));
  t4 = ((int64_t)(((int64_t)UINT64_C(1))));
  t5 = ((t3) < (t4));
  if (t5) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  v6 = gg_function_16(t6, base + 0);
  t7 = ((int64_t)(v6));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t11) < (t12));
  }
  t13 = t10;
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
  t18 = ((int64_t)((v1).len));
  t14 = ((t17) > (t18));
  }
  t19 = t14;
  t20 = t19;
  if (!t20) {
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t21) < (t22));
  }
  t23 = t20;
  t24 = t23;
  if (!t24) {
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(((int64_t)UINT64_C(288))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  t28 = ((int64_t)((v3).len));
  t24 = ((t27) > (t28));
  }
  t29 = t24;
  if (t29) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t30 = ((int64_t)UINT64_C(0));
  t31 = ((float *)(v5).data) + gg_index(t30, (v5).len);
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(12))));
  t34 = ((t32) == (t33));
  if (t34) {
  {
  t35 = v1;
  t36 = ((int64_t)(v2));
  t37 = v3;
  t38 = ((int64_t)(v4));
  (*t31) = gg_function_17(t35, t36, t37, t38, base + 0);
  }
  } else {
  {
  t39 = v1;
  t40 = ((int64_t)(v2));
  t41 = v3;
  t42 = ((int64_t)(v4));
  (*t31) = gg_function_26(t39, t40, t41, t42, base + 0);
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_42(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  gg_slice t7;
  int64_t t8;
  gg_slice t9;
  int64_t t10;
  gg_slice t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  gg_slice t18;
  int64_t v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
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
  t1 = ((int64_t)(((int64_t)UINT64_C(109))));
  t2 = ((int64_t)(v0));
  fgc_witness(t1, t2);
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  t6 = ((int64_t)(v5));
  t7 = v1;
  t8 = ((int64_t)(v6));
  t9 = v2;
  t10 = ((int64_t)(v7));
  t11 = v4;
  t0 = gg_function_15(t6, t7, t8, t9, t10, t11, base + 0);
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = ((t12) == (t13));
  if (t14) {
  {
  t15 = ((int64_t)(v5));
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(v7));
  t18 = v4;
  t0 = gg_function_33(t15, t16, t17, t18, base + 0);
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ float gg_function_41(gg_slice p0, int64_t p1, int64_t base) {
  /* FG_SHARED_BODY:27 */
  return gg_function_27(p0, p1, base);
}
static __host__ __device__ int64_t gg_function_40(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
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
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
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
  uint8_t t60;
  gg_slice t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  float t77;
  float t78;
  gg_slice t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  float t87;
  float t88;
  float t89;
  int64_t t90;
  int64_t t91;
  gg_slice t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  float * t96;
  int64_t t97;
  int64_t t98;
  int64_t v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  float v11;
  int64_t v12;
  float v13;
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
  t1 = ((int64_t)(((int64_t)UINT64_C(108))));
  t2 = ((int64_t)(v7));
  fgc_witness(t1, t2);
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) <= (t4));
  t6 = t5;
  if (!t6) {
  t7 = ((int64_t)(v5));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t7) <= (t8));
  }
  t9 = t6;
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v6));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t11) <= (t12));
  }
  t13 = t10;
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t15) < (t16));
  }
  t17 = t14;
  t18 = t17;
  if (!t18) {
  t19 = ((int64_t)(v7));
  t20 = ((int64_t)(v4));
  t18 = ((t19) >= (t20));
  }
  t21 = t18;
  t22 = t21;
  if (!t22) {
  t23 = ((int64_t)(v8));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t23) <= (t24));
  }
  t25 = t22;
  if (t25) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t26 = ((int64_t)(v0));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  }
  t29 = ((int64_t)((v1).len));
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(v6));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(v9));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((t29) < (t34));
  t36 = t35;
  if (!t36) {
  t37 = ((int64_t)((v2).len));
  t38 = ((int64_t)(v6));
  t39 = ((int64_t)(v5));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  t41 = ((int64_t)(((int64_t)UINT64_C(4))));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) * (uint64_t)(t41)))));
  t36 = ((t37) < (t42));
  }
  t43 = t36;
  t44 = t43;
  if (!t44) {
  t45 = ((int64_t)((v3).len));
  t46 = ((int64_t)(v5));
  t47 = ((int64_t)(((int64_t)UINT64_C(1))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) - (uint64_t)(t47)))));
  t49 = ((int64_t)(v8));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t49)))));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t51)))));
  t44 = ((t45) < (t52));
  }
  t53 = t44;
  if (t53) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t54 = ((int64_t)(v10));
  t55 = ((int64_t)(v5));
  if (!(((t54) < (t55)))) goto label2;
  {
  v11 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t56 = ((int64_t)(v12));
  t57 = ((int64_t)(v6));
  if (!(((t56) < (t57)))) goto label5;
  {
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  t60 = ((t58) == (t59));
  if (t60) {
  {
  t61 = v1;
  t62 = ((int64_t)(v7));
  t63 = ((int64_t)(v6));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t65 = ((int64_t)(v12));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
  t67 = ((int64_t)(((int64_t)UINT64_C(2))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  v13 = gg_function_18(t61, t68, base + 0);
  }
  } else {
  {
  t69 = v1;
  t70 = ((int64_t)(v7));
  t71 = ((int64_t)(v6));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))));
  t73 = ((int64_t)(v12));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  t75 = ((int64_t)(((int64_t)UINT64_C(4))));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t74) * (uint64_t)(t75)))));
  v13 = gg_function_27(t69, t76, base + 0);
  }
  }
  t77 = ((float)(v11));
  t78 = ((float)(v13));
  t79 = v2;
  t80 = ((int64_t)(v10));
  t81 = ((int64_t)(v6));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  t83 = ((int64_t)(v12));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  t85 = ((int64_t)(((int64_t)UINT64_C(4))));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))));
  t87 = gg_function_27(t79, t86, base + 0);
  t88 = ((float)(t87));
  t89 = ((float)(((t78) * (t88))));
  v11 = ((float)(((t77) + (t89))));
  t90 = v12;
  t91 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)((uint64_t)(t90) + (uint64_t)(t91)));
  }
  goto label4;
label5:;
  t92 = v3;
  t93 = ((int64_t)(v10));
  t94 = ((int64_t)(v8));
  t95 = ((int64_t)((uint64_t)(t93) * (uint64_t)(t94)));
  t96 = ((float *)t92.data) + gg_index(t95, t92.len);
  (*t96) = ((float)(v11));
  t97 = v10;
  t98 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t97) + (uint64_t)(t98)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_39(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, gg_slice p8, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  float * t10;
  int64_t t11;
  float * t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  float * t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  float * t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  int64_t t51;
  float * t52;
  int64_t t53;
  float * t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  uint8_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
  int64_t t65;
  float * t66;
  int64_t t67;
  float * t68;
  int64_t t69;
  float * t70;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  gg_slice v8;
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
  t1 = ((int64_t)(((int64_t)UINT64_C(107))));
  t2 = ((int64_t)(v0));
  fgc_witness(t1, t2);
  t3 = ((int64_t)(v7));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) < (t4));
  if (t5) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)((v8).len));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((t6) < (t7));
  if (t8) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t9 = ((int64_t)UINT64_C(0));
  t10 = ((float *)(v8).data) + gg_index(t9, (v8).len);
  (*t10) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  /* FG_DIRECT_SLICE_INDEX */
  t11 = ((int64_t)UINT64_C(1));
  t12 = ((float *)(v8).data) + gg_index(t11, (v8).len);
  (*t12) = ((float)(((float)(v1))));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t16 = ((int64_t)UINT64_C(0));
  t17 = ((float *)(v8).data) + gg_index(t16, (v8).len);
  (*t17) = ((float)(((float)(((int64_t)UINT64_C(10))))));
  t0 = ((int64_t)(((int64_t)UINT64_C(10))));
  goto gg_done;
  }
  }
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(12))));
  t20 = ((t18) == (t19));
  t21 = t20;
  if (!t21) {
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)UINT64_C(14))));
  t21 = ((t22) == (t23));
  }
  t24 = t21;
  if (t24) {
  {
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t25) > (t26));
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(((int64_t)UINT64_C(8))));
  t28 = ((t29) <= (t30));
  }
  t31 = t28;
  if (t31) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t32 = ((int64_t)UINT64_C(0));
  t33 = ((float *)(v8).data) + gg_index(t32, (v8).len);
  (*t33) = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t34 = ((int64_t)(v5));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) > (t35));
  t37 = t36;
  if (t37) {
  t38 = ((int64_t)(v5));
  t39 = ((int64_t)(((int64_t)UINT64_C(256))));
  t40 = ((int64_t)(((int64_t)gg_mod(t38, t39))));
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t40) == (t41));
  }
  t42 = t37;
  t43 = t42;
  if (t43) {
  t44 = ((int64_t)(v4));
  t45 = ((int64_t)(((int64_t)UINT64_C(49152))));
  t43 = ((t44) >= (t45));
  }
  t46 = t43;
  t47 = t46;
  if (t47) {
  t48 = ((int64_t)(v3));
  t49 = ((int64_t)(((int64_t)UINT64_C(750))));
  t47 = ((t48) >= (t49));
  }
  t50 = t47;
  if (t50) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t51 = ((int64_t)UINT64_C(0));
  t52 = ((float *)(v8).data) + gg_index(t51, (v8).len);
  (*t52) = ((float)(((float)(((int64_t)UINT64_C(3))))));
  t0 = ((int64_t)(((int64_t)UINT64_C(3))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t53 = ((int64_t)UINT64_C(0));
  t54 = ((float *)(v8).data) + gg_index(t53, (v8).len);
  (*t54) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t55 = ((int64_t)(v0));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t55) == (t56));
  t58 = t57;
  if (!t58) {
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  t58 = ((t59) == (t60));
  }
  t61 = t58;
  if (t61) {
  {
  t62 = ((int64_t)(v1));
  t63 = ((int64_t)(((int64_t)UINT64_C(8))));
  t64 = ((t62) > (t63));
  if (t64) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t65 = ((int64_t)UINT64_C(0));
  t66 = ((float *)(v8).data) + gg_index(t65, (v8).len);
  (*t66) = ((float)(((float)(((int64_t)UINT64_C(2))))));
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t67 = ((int64_t)UINT64_C(0));
  t68 = ((float *)(v8).data) + gg_index(t67, (v8).len);
  (*t68) = ((float)(((float)(((int64_t)UINT64_C(4))))));
  t0 = ((int64_t)(((int64_t)UINT64_C(4))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t69 = ((int64_t)UINT64_C(0));
  t70 = ((float *)(v8).data) + gg_index(t69, (v8).len);
  (*t70) = ((float)(((float)(((int64_t)UINT64_C(4))))));
  t0 = ((int64_t)(((int64_t)UINT64_C(4))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_36(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) gg_fail("assertion failed");
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t4)))));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) - (uint64_t)(t6)))));
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)gg_div(t7, t8))));
  t10 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_37(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)UINT64_C(4))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t8)))));
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(144))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = gg_function_36(t12, t15, base + 0);
  t17 = ((int64_t)(t16));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t17)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_38(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((int64_t)(v0));
  }
  } else {
  {
  t0 = ((int64_t)(v1));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_35(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, gg_slice p7, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  float * t9;
  int64_t t10;
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
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
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
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
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
  uint8_t t96;
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
  uint8_t t107;
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
  uint8_t t122;
  uint8_t t123;
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
  float * t164;
  int64_t t165;
  float * t166;
  int64_t t167;
  float * t168;
  int64_t t169;
  float * t170;
  int64_t t171;
  float * t172;
  int64_t t173;
  float * t174;
  int64_t t175;
  float * t176;
  int64_t t177;
  float * t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  float * t185;
  int64_t t186;
  float * t187;
  int64_t t188;
  float * t189;
  int64_t t190;
  float * t191;
  int64_t t192;
  float * t193;
  int64_t t194;
  float * t195;
  int64_t t196;
  float * t197;
  int64_t t198;
  float * t199;
  int64_t t200;
  float * t201;
  int64_t t202;
  float * t203;
  int64_t t204;
  float * t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  float * t209;
  int64_t t210;
  float * t211;
  int64_t t212;
  float * t213;
  int64_t t214;
  float * t215;
  int64_t t216;
  float * t217;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  gg_slice v7;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(106))));
  t2 = ((int64_t)(v0));
  fgc_witness(t1, t2);
  t3 = ((int64_t)((v7).len));
  t4 = ((int64_t)(((int64_t)UINT64_C(24))));
  t5 = ((t3) < (t4));
  if (t5) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t6 = ((int64_t)(v8));
  t7 = ((int64_t)(((int64_t)UINT64_C(24))));
  if (!(((t6) < (t7)))) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v8;
  t9 = ((float *)(v7).data) + gg_index(t8, (v7).len);
  (*t9) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t10 = v8;
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)((uint64_t)(t10) + (uint64_t)(t11)));
  }
  goto label1;
label2:;
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(12))));
  t14 = ((t12) != (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(14))));
  t15 = ((t16) != (t17));
  }
  t18 = t15;
  if (t18) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) <= (t20));
  t22 = t21;
  if (!t22) {
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t23) <= (t24));
  }
  t25 = t22;
  t26 = t25;
  if (!t26) {
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t27) <= (t28));
  }
  t29 = t26;
  t30 = t29;
  if (!t30) {
  t31 = ((int64_t)(v1));
  t32 = ((int64_t)(((int64_t)UINT64_C(256))));
  t33 = ((int64_t)(((int64_t)gg_mod(t31, t32))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t33) != (t34));
  }
  t35 = t30;
  if (t35) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v9 = ((int64_t)(((int64_t)UINT64_C(128))));
  v10 = ((int64_t)(((int64_t)UINT64_C(256))));
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(12))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(12))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  }
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(12))));
  t44 = ((t42) == (t43));
  if (t44) {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(84))));
  }
  } else {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(76))));
  }
  }
  t45 = ((int64_t)(v2));
  t46 = ((int64_t)(((int64_t)UINT64_C(128))));
  t47 = ((int64_t)(((int64_t)gg_mod(t45, t46))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((t47) != (t48));
  if (t49) {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v14 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(((int64_t)UINT64_C(512))));
  v15 = gg_function_36(t50, t51, base + 0);
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
  v17 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  v18 = ((int64_t)(((int64_t)UINT64_C(8))));
label4:;
  t52 = ((int64_t)(v18));
  t53 = ((int64_t)(((int64_t)UINT64_C(128))));
  t54 = ((t52) <= (t53));
  t55 = t54;
  if (t55) {
  t56 = ((int64_t)(v17));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((t56) > (t57));
  }
  if (!(t55)) goto label5;
  {
  t58 = ((int64_t)(v18));
  t59 = ((int64_t)(v9));
  t60 = ((int64_t)(v13));
  t61 = ((int64_t)(v10));
  t62 = gg_function_37(t58, t59, t60, t61, base + 0);
  t63 = ((int64_t)(t62));
  t64 = ((int64_t)(v4));
  t65 = ((t63) <= (t64));
  if (t65) {
  {
  t66 = ((int64_t)(v3));
  t67 = ((int64_t)(v18));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t69 = ((int64_t)(((int64_t)UINT64_C(1))));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t68) - (uint64_t)(t69)))));
  t71 = ((int64_t)(v18));
  v19 = ((int64_t)(((int64_t)gg_div(t70, t71))));
  t72 = ((int64_t)(v19));
  t73 = ((int64_t)(v17));
  t74 = ((t72) < (t73));
  if (t74) {
  {
  v16 = ((int64_t)(v18));
  v17 = ((int64_t)(v19));
  }
  }
  }
  }
  t75 = v18;
  t76 = ((int64_t)(((int64_t)UINT64_C(8))));
  v18 = ((int64_t)((uint64_t)(t75) + (uint64_t)(t76)));
  }
  goto label4;
label5:;
  t77 = ((int64_t)(v16));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = ((t77) == (t78));
  if (t79) {
  {
  v16 = ((int64_t)(((int64_t)UINT64_C(8))));
  }
  }
  t80 = ((int64_t)(v2));
  t81 = ((int64_t)(v9));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t81)))));
  t83 = ((int64_t)(((int64_t)UINT64_C(1))));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) - (uint64_t)(t83)))));
  t85 = ((int64_t)(v9));
  v20 = ((int64_t)(((int64_t)gg_div(t84, t85))));
  t86 = ((int64_t)(v3));
  t87 = ((int64_t)(v16));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))));
  t89 = ((int64_t)(((int64_t)UINT64_C(1))));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) - (uint64_t)(t89)))));
  t91 = ((int64_t)(v16));
  v21 = ((int64_t)(((int64_t)gg_div(t90, t91))));
  t92 = ((int64_t)(v20));
  t93 = ((int64_t)(v21));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t92) * (uint64_t)(t93)))));
  t94 = ((int64_t)(v6));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = ((t94) > (t95));
  if (t96) {
  {
  v23 = ((int64_t)(v6));
  }
  } else {
  {
  v23 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t97 = ((int64_t)(v22));
  t98 = ((int64_t)(v23));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t97) + (uint64_t)(t98)))));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) - (uint64_t)(t100)))));
  t102 = ((int64_t)(v23));
  v24 = ((int64_t)(((int64_t)gg_div(t101, t102))));
  t103 = ((int64_t)(v23));
  t104 = ((int64_t)(v24));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t104)))));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = ((t105) > (t106));
  if (t107) {
  {
  t108 = ((int64_t)(((int64_t)UINT64_C(100))));
  t109 = ((int64_t)(v22));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = ((int64_t)(v23));
  t112 = ((int64_t)(v24));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t111) * (uint64_t)(t112)))));
  v25 = ((int64_t)(((int64_t)gg_div(t110, t113))));
  }
  } else {
  {
  v25 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t114 = ((int64_t)(v22));
  t115 = ((int64_t)(v1));
  t116 = ((int64_t)(((int64_t)UINT64_C(255))));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = ((int64_t)(((int64_t)UINT64_C(256))));
  t119 = ((int64_t)(((int64_t)gg_div(t117, t118))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t114) * (uint64_t)(t119)))));
  t120 = ((int64_t)(v25));
  t121 = ((int64_t)(((int64_t)UINT64_C(90))));
  t122 = ((t120) >= (t121));
  t123 = t122;
  if (t123) {
  t124 = ((int64_t)(v5));
  t125 = ((int64_t)(((int64_t)UINT64_C(750))));
  t123 = ((t124) >= (t125));
  }
  t126 = t123;
  if (t126) {
  {
  v27 = ((int64_t)(v22));
  }
  } else {
  {
  t127 = ((int64_t)(v23));
  t128 = ((int64_t)(v26));
  v27 = gg_function_38(t127, t128, base + 0);
  }
  }
  t129 = ((int64_t)(v27));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = ((t129) > (t130));
  t132 = t131;
  if (t132) {
  t133 = ((int64_t)(v22));
  t134 = ((int64_t)(v27));
  t135 = ((int64_t)(((int64_t)gg_mod(t133, t134))));
  t136 = ((int64_t)(((int64_t)UINT64_C(0))));
  t132 = ((t135) != (t136));
  }
  t137 = t132;
  if (t137) {
  {
  v28 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v28 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t138 = ((int64_t)(v10));
  t139 = ((int64_t)(((int64_t)UINT64_C(4))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t138) * (uint64_t)(t139)))));
  t140 = ((int64_t)(v16));
  t141 = ((int64_t)(((int64_t)UINT64_C(144))));
  t142 = ((int64_t)(((int64_t)((uint64_t)(t140) * (uint64_t)(t141)))));
  t143 = ((int64_t)(v29));
  v30 = gg_function_36(t142, t143, base + 0);
  v31 = ((int64_t)(((int64_t)UINT64_C(32))));
  v32 = ((int64_t)(((int64_t)UINT64_C(8))));
  t144 = ((int64_t)(v2));
  t145 = ((int64_t)(v31));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t144) + (uint64_t)(t145)))));
  t147 = ((int64_t)(((int64_t)UINT64_C(1))));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t146) - (uint64_t)(t147)))));
  t149 = ((int64_t)(v31));
  v33 = ((int64_t)(((int64_t)gg_div(t148, t149))));
  t150 = ((int64_t)(v3));
  t151 = ((int64_t)(v32));
  t152 = ((int64_t)(((int64_t)((uint64_t)(t150) + (uint64_t)(t151)))));
  t153 = ((int64_t)(((int64_t)UINT64_C(1))));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t152) - (uint64_t)(t153)))));
  t155 = ((int64_t)(v32));
  v34 = ((int64_t)(((int64_t)gg_div(t154, t155))));
  t156 = ((int64_t)(v0));
  v35 = gg_function_34(t156, base + 0);
  t157 = ((int64_t)(v31));
  t158 = ((int64_t)(v35));
  t159 = ((int64_t)(((int64_t)((uint64_t)(t157) * (uint64_t)(t158)))));
  t160 = ((int64_t)(v32));
  t161 = ((int64_t)(((int64_t)UINT64_C(288))));
  t162 = ((int64_t)(((int64_t)((uint64_t)(t160) * (uint64_t)(t161)))));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t159) + (uint64_t)(t162)))));
  /* FG_DIRECT_SLICE_INDEX */
  t163 = ((int64_t)UINT64_C(0));
  t164 = ((float *)(v7).data) + gg_index(t163, (v7).len);
  (*t164) = ((float)(((float)(((int64_t)UINT64_C(1))))));
  /* FG_DIRECT_SLICE_INDEX */
  t165 = ((int64_t)UINT64_C(1));
  t166 = ((float *)(v7).data) + gg_index(t165, (v7).len);
  (*t166) = ((float)(((float)(v16))));
  /* FG_DIRECT_SLICE_INDEX */
  t167 = ((int64_t)UINT64_C(2));
  t168 = ((float *)(v7).data) + gg_index(t167, (v7).len);
  (*t168) = ((float)(((float)(v10))));
  /* FG_DIRECT_SLICE_INDEX */
  t169 = ((int64_t)UINT64_C(3));
  t170 = ((float *)(v7).data) + gg_index(t169, (v7).len);
  (*t170) = ((float)(((float)(v9))));
  /* FG_DIRECT_SLICE_INDEX */
  t171 = ((int64_t)UINT64_C(4));
  t172 = ((float *)(v7).data) + gg_index(t171, (v7).len);
  (*t172) = ((float)(((float)(v11))));
  /* FG_DIRECT_SLICE_INDEX */
  t173 = ((int64_t)UINT64_C(5));
  t174 = ((float *)(v7).data) + gg_index(t173, (v7).len);
  (*t174) = ((float)(((float)(v12))));
  /* FG_DIRECT_SLICE_INDEX */
  t175 = ((int64_t)UINT64_C(6));
  t176 = ((float *)(v7).data) + gg_index(t175, (v7).len);
  (*t176) = ((float)(((float)(v13))));
  /* FG_DIRECT_SLICE_INDEX */
  t177 = ((int64_t)UINT64_C(7));
  t178 = ((float *)(v7).data) + gg_index(t177, (v7).len);
  t179 = ((int64_t)(v16));
  t180 = ((int64_t)(v9));
  t181 = ((int64_t)(v13));
  t182 = ((int64_t)(v10));
  t183 = gg_function_37(t179, t180, t181, t182, base + 0);
  (*t178) = ((float)(((float)(t183))));
  /* FG_DIRECT_SLICE_INDEX */
  t184 = ((int64_t)UINT64_C(8));
  t185 = ((float *)(v7).data) + gg_index(t184, (v7).len);
  (*t185) = ((float)(((float)(v20))));
  /* FG_DIRECT_SLICE_INDEX */
  t186 = ((int64_t)UINT64_C(9));
  t187 = ((float *)(v7).data) + gg_index(t186, (v7).len);
  (*t187) = ((float)(((float)(v21))));
  /* FG_DIRECT_SLICE_INDEX */
  t188 = ((int64_t)UINT64_C(10));
  t189 = ((float *)(v7).data) + gg_index(t188, (v7).len);
  (*t189) = ((float)(((float)(v22))));
  /* FG_DIRECT_SLICE_INDEX */
  t190 = ((int64_t)UINT64_C(11));
  t191 = ((float *)(v7).data) + gg_index(t190, (v7).len);
  (*t191) = ((float)(((float)(v33))));
  /* FG_DIRECT_SLICE_INDEX */
  t192 = ((int64_t)UINT64_C(12));
  t193 = ((float *)(v7).data) + gg_index(t192, (v7).len);
  (*t193) = ((float)(((float)(v34))));
  /* FG_DIRECT_SLICE_INDEX */
  t194 = ((int64_t)UINT64_C(13));
  t195 = ((float *)(v7).data) + gg_index(t194, (v7).len);
  (*t195) = ((float)(((float)(v31))));
  /* FG_DIRECT_SLICE_INDEX */
  t196 = ((int64_t)UINT64_C(14));
  t197 = ((float *)(v7).data) + gg_index(t196, (v7).len);
  (*t197) = ((float)(((float)(v32))));
  /* FG_DIRECT_SLICE_INDEX */
  t198 = ((int64_t)UINT64_C(15));
  t199 = ((float *)(v7).data) + gg_index(t198, (v7).len);
  (*t199) = ((float)(((float)(v36))));
  /* FG_DIRECT_SLICE_INDEX */
  t200 = ((int64_t)UINT64_C(16));
  t201 = ((float *)(v7).data) + gg_index(t200, (v7).len);
  (*t201) = ((float)(((float)(v14))));
  /* FG_DIRECT_SLICE_INDEX */
  t202 = ((int64_t)UINT64_C(17));
  t203 = ((float *)(v7).data) + gg_index(t202, (v7).len);
  (*t203) = ((float)(((float)(v15))));
  /* FG_DIRECT_SLICE_INDEX */
  t204 = ((int64_t)UINT64_C(18));
  t205 = ((float *)(v7).data) + gg_index(t204, (v7).len);
  t206 = ((int64_t)(v15));
  t207 = ((int64_t)(((int64_t)UINT64_C(128))));
  (*t205) = ((float)(((float)(((int64_t)gg_div(t206, t207))))));
  /* FG_DIRECT_SLICE_INDEX */
  t208 = ((int64_t)UINT64_C(19));
  t209 = ((float *)(v7).data) + gg_index(t208, (v7).len);
  (*t209) = ((float)(((float)(v30))));
  /* FG_DIRECT_SLICE_INDEX */
  t210 = ((int64_t)UINT64_C(20));
  t211 = ((float *)(v7).data) + gg_index(t210, (v7).len);
  (*t211) = ((float)(((float)(v27))));
  /* FG_DIRECT_SLICE_INDEX */
  t212 = ((int64_t)UINT64_C(21));
  t213 = ((float *)(v7).data) + gg_index(t212, (v7).len);
  (*t213) = ((float)(((float)(v28))));
  /* FG_DIRECT_SLICE_INDEX */
  t214 = ((int64_t)UINT64_C(22));
  t215 = ((float *)(v7).data) + gg_index(t214, (v7).len);
  (*t215) = ((float)(((float)(((int64_t)UINT64_C(2))))));
  /* FG_DIRECT_SLICE_INDEX */
  t216 = ((int64_t)UINT64_C(23));
  t217 = ((float *)(v7).data) + gg_index(t216, (v7).len);
  (*t217) = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_32(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, gg_slice p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
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
  int64_t t29;
  int64_t t30;
  int64_t t31;
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
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  gg_slice t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t * t55;
  gg_slice t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  uint8_t * t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  gg_slice v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(104))));
  t2 = ((int64_t)(v4));
  fgc_witness(t1, t2);
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) <= (t4));
  t6 = t5;
  if (!t6) {
  t7 = ((int64_t)(v4));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t7) <= (t8));
  }
  t9 = t6;
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v4));
  t12 = ((int64_t)(((int64_t)UINT64_C(8))));
  t10 = ((t11) > (t12));
  }
  t13 = t10;
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)((v5).len));
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)UINT64_C(288))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t14 = ((t15) < (t18));
  }
  t19 = t14;
  if (t19) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(v4));
  if (!(((t20) < (t21)))) goto label2;
  {
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t22 = ((int64_t)(v7));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  if (!(((t22) < (t23)))) goto label5;
  {
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(v7));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = ((int64_t)(v1));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t29 = ((int64_t)(v3));
  t30 = ((int64_t)(v6));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t31)))));
  t33 = ((int64_t)(((int64_t)UINT64_C(144))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t34 = ((int64_t)(v8));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t34) < (t35));
  t37 = t36;
  if (!t37) {
  t38 = ((int64_t)(v8));
  t39 = ((int64_t)(((int64_t)UINT64_C(144))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  t41 = ((int64_t)((v0).len));
  t37 = ((t40) > (t41));
  }
  t42 = t37;
  if (t42) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t43 = ((int64_t)(v9));
  t44 = ((int64_t)(((int64_t)UINT64_C(144))));
  if (!(((t43) < (t44)))) goto label8;
  {
  t45 = v5;
  t46 = ((int64_t)(v6));
  t47 = ((int64_t)(((int64_t)UINT64_C(288))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)(v7));
  t50 = ((int64_t)(((int64_t)UINT64_C(144))));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t51)))));
  t53 = ((int64_t)(v9));
  t54 = ((int64_t)((uint64_t)(t52) + (uint64_t)(t53)));
  t55 = ((uint8_t *)t45.data) + gg_index(t54, t45.len);
  t56 = v0;
  t57 = ((int64_t)(v8));
  t58 = ((int64_t)(v9));
  t59 = ((int64_t)((uint64_t)(t57) + (uint64_t)(t58)));
  t60 = ((uint8_t *)t56.data) + gg_index(t59, t56.len);
  (*t55) = ((uint8_t)((*t60)));
  t61 = v9;
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t61) + (uint64_t)(t62)));
  }
  goto label7;
label8:;
  t63 = v7;
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t63) + (uint64_t)(t64)));
  }
  goto label4;
label5:;
  t65 = v6;
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t65) + (uint64_t)(t66)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_31(int64_t p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_slice p6, int64_t p7, int64_t base) {
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
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
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
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  gg_slice t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t * t52;
  gg_slice t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t * t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  gg_slice t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint8_t * t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_slice v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(103))));
  t2 = ((int64_t)(v0));
  fgc_witness(t1, t2);
  t3 = ((int64_t)(v0));
  v8 = gg_function_16(t3, base + 0);
  t4 = ((int64_t)(v8));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  t7 = t6;
  if (!t7) {
  t8 = ((int64_t)(v5));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) <= (t9));
  }
  t10 = t7;
  t11 = t10;
  if (!t11) {
  t12 = ((int64_t)(v5));
  t13 = ((int64_t)(((int64_t)UINT64_C(32))));
  t11 = ((t12) > (t13));
  }
  t14 = t11;
  t15 = t14;
  if (!t15) {
  t16 = ((int64_t)(v7));
  t17 = ((int64_t)(v8));
  t15 = ((t16) < (t17));
  }
  t18 = t15;
  t19 = t18;
  if (!t19) {
  t20 = ((int64_t)((v6).len));
  t21 = ((int64_t)(v5));
  t22 = ((int64_t)(v7));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t19 = ((t20) < (t23));
  }
  t24 = t19;
  if (t24) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t25 = ((int64_t)(v9));
  t26 = ((int64_t)(v5));
  if (!(((t25) < (t26)))) goto label2;
  {
  t27 = ((int64_t)(v4));
  t28 = ((int64_t)(v9));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = ((int64_t)(v2));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t32 = ((int64_t)(v3));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = ((int64_t)(v8));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t35 = ((int64_t)(v10));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t35) < (t36));
  t38 = t37;
  if (!t38) {
  t39 = ((int64_t)(v10));
  t40 = ((int64_t)(v8));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  t42 = ((int64_t)((v1).len));
  t38 = ((t41) > (t42));
  }
  t43 = t38;
  if (t43) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t44 = ((int64_t)(v11));
  t45 = ((int64_t)(v8));
  if (!(((t44) < (t45)))) goto label5;
  {
  t46 = v6;
  t47 = ((int64_t)(v9));
  t48 = ((int64_t)(v7));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  t50 = ((int64_t)(v11));
  t51 = ((int64_t)((uint64_t)(t49) + (uint64_t)(t50)));
  t52 = ((uint8_t *)t46.data) + gg_index(t51, t46.len);
  t53 = v1;
  t54 = ((int64_t)(v10));
  t55 = ((int64_t)(v11));
  t56 = ((int64_t)((uint64_t)(t54) + (uint64_t)(t55)));
  t57 = ((uint8_t *)t53.data) + gg_index(t56, t53.len);
  (*t52) = ((uint8_t)((*t57)));
  t58 = v11;
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t59)));
  }
  goto label4;
label5:;
label7:;
  t60 = ((int64_t)(v11));
  t61 = ((int64_t)(v7));
  if (!(((t60) < (t61)))) goto label8;
  {
  t62 = v6;
  t63 = ((int64_t)(v9));
  t64 = ((int64_t)(v7));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  t66 = ((int64_t)(v11));
  t67 = ((int64_t)((uint64_t)(t65) + (uint64_t)(t66)));
  t68 = ((uint8_t *)t62.data) + gg_index(t67, t62.len);
  (*t68) = ((uint8_t)(((uint8_t)(((int64_t)UINT64_C(0))))));
  t69 = v11;
  t70 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)((uint64_t)(t69) + (uint64_t)(t70)));
  }
  goto label7;
label8:;
  t71 = v9;
  t72 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t71) + (uint64_t)(t72)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ void gg_function_12(gg_slice p0, int64_t p1, uint32_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t * t9;
  gg_slice t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t * t14;
  uint32_t t15;
  int64_t t16;
  gg_slice t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t * t21;
  uint32_t t22;
  int64_t t23;
  gg_slice t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t * t28;
  uint32_t t29;
  int64_t t30;
  gg_slice v0;
  int64_t v1;
  uint32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(3))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = ((int64_t)((v0).len));
  t3 = ((t6) < (t7));
  }
  if (!(t3)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v1;
  t9 = ((uint8_t *)(v0).data) + gg_index(t8, (v0).len);
  (*t9) = ((uint8_t)(((uint8_t)(v2))));
  t10 = v0;
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = ((int64_t)((uint64_t)(t11) + (uint64_t)(t12)));
  t14 = ((uint8_t *)t10.data) + gg_index(t13, t10.len);
  t15 = ((uint32_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(8))));
  (*t14) = ((uint8_t)(((uint8_t)(((t15) >> ((uint64_t)(t16) & 31))))));
  t17 = v0;
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  t21 = ((uint8_t *)t17.data) + gg_index(t20, t17.len);
  t22 = ((uint32_t)(v2));
  t23 = ((int64_t)(((int64_t)UINT64_C(16))));
  (*t21) = ((uint8_t)(((uint8_t)(((t22) >> ((uint64_t)(t23) & 31))))));
  t24 = v0;
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(3))));
  t27 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  t28 = ((uint8_t *)t24.data) + gg_index(t27, t24.len);
  t29 = ((uint32_t)(v2));
  t30 = ((int64_t)(((int64_t)UINT64_C(24))));
  (*t28) = ((uint8_t)(((uint8_t)(((t29) >> ((uint64_t)(t30) & 31))))));
  }
gg_done:;
}
static __host__ __device__ void gg_function_7(gg_slice p0, int64_t p1, uint16_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t * t9;
  gg_slice t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t * t14;
  uint16_t t15;
  int64_t t16;
  gg_slice v0;
  int64_t v1;
  uint16_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = ((int64_t)((v0).len));
  t3 = ((t6) < (t7));
  }
  if (!(t3)) gg_fail("assertion failed");
  /* FG_DIRECT_SLICE_INDEX */
  t8 = v1;
  t9 = ((uint8_t *)(v0).data) + gg_index(t8, (v0).len);
  (*t9) = ((uint8_t)(((uint8_t)(v2))));
  t10 = v0;
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = ((int64_t)((uint64_t)(t11) + (uint64_t)(t12)));
  t14 = ((uint8_t *)t10.data) + gg_index(t13, t10.len);
  t15 = ((uint16_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(8))));
  (*t14) = ((uint8_t)(((uint8_t)(((t15) >> ((uint64_t)(t16) & 15))))));
  }
gg_done:;
}
static __host__ __device__ int64_t gg_function_10(float p0, float p1, int64_t p2, gg_slice p3, int64_t p4, int64_t p5, int64_t base) {
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
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  float t23;
  float t24;
  uint8_t t25;
  float t26;
  float t27;
  float t28;
  float t29;
  gg_slice t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  float t36;
  uint16_t t37;
  uint16_t t38;
  gg_slice t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  float t47;
  uint16_t t48;
  uint16_t t49;
  int64_t t50;
  int64_t t51;
  uint8_t t52;
  float t53;
  float t54;
  uint8_t t55;
  float t56;
  float t57;
  float t58;
  float t59;
  gg_slice t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  float t66;
  uint32_t t67;
  uint32_t t68;
  float v0;
  float v1;
  int64_t v2;
  gg_slice v3;
  int64_t v4;
  int64_t v5;
  float v6;
  float v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(121))));
  t2 = ((int64_t)(v5));
  fgc_witness(t1, t2);
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) < (t4));
  t6 = t5;
  if (!t6) {
  t7 = ((int64_t)(v4));
  t8 = ((int64_t)(((int64_t)UINT64_C(144))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = ((int64_t)((v3).len));
  t6 = ((t9) > (t10));
  }
  t11 = t6;
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t13) < (t14));
  }
  t15 = t12;
  t16 = t15;
  if (!t16) {
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t16 = ((t17) >= (t18));
  }
  t19 = t16;
  if (t19) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  t23 = ((float)(v0));
  t24 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t25 = ((t23) == (t24));
  if (t25) {
  {
  v6 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t26 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t27 = ((float)(((float)(((int64_t)UINT64_C(127))))));
  t28 = ((float)(v0));
  t29 = ((float)(((t27) / (t28))));
  v6 = ((float)(((t26) / (t29))));
  }
  }
  t30 = v3;
  t31 = ((int64_t)(v4));
  t32 = ((int64_t)(v5));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t34)))));
  t36 = ((float)(v6));
  t37 = fgc_to_half(t36);
  t38 = ((uint16_t)(t37));
  gg_function_7(t30, t35, t38, base + 0);
  t39 = v3;
  t40 = ((int64_t)(v4));
  t41 = ((int64_t)(v5));
  t42 = ((int64_t)(((int64_t)UINT64_C(4))));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)UINT64_C(2))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((float)(v1));
  t48 = fgc_to_half(t47);
  t49 = ((uint16_t)(t48));
  gg_function_7(t39, t46, t49, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t50 = ((int64_t)(v2));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  t52 = ((t50) == (t51));
  if (t52) {
  {
  t53 = ((float)(v0));
  t54 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t55 = ((t53) == (t54));
  if (t55) {
  {
  v7 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t56 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t57 = ((float)(((float)(((int64_t)UINT64_C(127))))));
  t58 = ((float)(v0));
  t59 = ((float)(((t57) / (t58))));
  v7 = ((float)(((t56) / (t59))));
  }
  }
  t60 = v3;
  t61 = ((int64_t)(v4));
  t62 = ((int64_t)(v5));
  t63 = ((int64_t)(((int64_t)UINT64_C(4))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t61) + (uint64_t)(t64)))));
  t66 = ((float)(v7));
  t67 = fgc_bits(t66);
  t68 = ((uint32_t)(t67));
  gg_function_12(t60, t65, t68, base + 0);
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
static __host__ __device__ int64_t gg_function_14(float p0, float p1, gg_slice p2, int64_t p3, int64_t base) {
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
  float t12;
  float t13;
  gg_slice t14;
  int64_t t15;
  float t16;
  uint16_t t17;
  uint16_t t18;
  gg_slice t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  float t23;
  uint16_t t24;
  uint16_t t25;
  float v0;
  float v1;
  gg_slice v2;
  int64_t v3;
  float v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(120))));
  t2 = ((int64_t)(v3));
  fgc_witness(t1, t2);
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) < (t4));
  t6 = t5;
  if (!t6) {
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((int64_t)UINT64_C(36))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = ((int64_t)((v2).len));
  t6 = ((t9) > (t10));
  }
  t11 = t6;
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t12 = ((float)(v0));
  t13 = ((float)(((float)(((int64_t)UINT64_C(127))))));
  v4 = ((float)(((t12) / (t13))));
  t14 = v2;
  t15 = ((int64_t)(v3));
  t16 = ((float)(v4));
  t17 = fgc_to_half(t16);
  t18 = ((uint16_t)(t17));
  gg_function_7(t14, t15, t18, base + 0);
  t19 = v2;
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)UINT64_C(2))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t21)))));
  t23 = ((float)(v1));
  t24 = fgc_to_half(t23);
  t25 = ((uint16_t)(t24));
  gg_function_7(t19, t22, t25, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ uint8_t gg_function_9(gg_slice p0, int64_t p1, float p2, int64_t base) {
  uint8_t t0;
  float t1;
  float t2;
  uint8_t t3;
  float t4;
  float t5;
  int64_t t6;
  float * t7;
  float t8;
  float t9;
  float t10;
  float t11;
  float t12;
  uint8_t t13;
  gg_slice v0;
  int64_t v1;
  float v2;
  float v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((float)(v2));
  t2 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((uint8_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t4 = ((float)(((float)(((int64_t)UINT64_C(127))))));
  t5 = ((float)(v2));
  v3 = ((float)(((t4) / (t5))));
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v1;
  t7 = ((float *)(v0).data) + gg_index(t6, (v0).len);
  t8 = ((float)((*t7)));
  t9 = ((float)(v3));
  t10 = ((float)(((t8) * (t9))));
  t11 = fgc_roundf(t10);
  t12 = ((float)(t11));
  t13 = fgc_f2i8(t12);
  t0 = ((uint8_t)(t13));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ uint8_t gg_function_3(gg_slice p0, int64_t p1, float p2, int64_t base) {
  uint8_t t0;
  float t1;
  float t2;
  uint8_t t3;
  float t4;
  float t5;
  int64_t t6;
  float * t7;
  float t8;
  float t9;
  float t10;
  float t11;
  float t12;
  uint8_t t13;
  gg_slice v0;
  int64_t v1;
  float v2;
  float v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((float)(v2));
  t2 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((uint8_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t4 = ((float)(v2));
  t5 = ((float)(((float)(((int64_t)UINT64_C(127))))));
  v3 = ((float)(((t4) / (t5))));
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v1;
  t7 = ((float *)(v0).data) + gg_index(t6, (v0).len);
  t8 = ((float)((*t7)));
  t9 = ((float)(v3));
  t10 = ((float)(((t8) / (t9))));
  t11 = fgc_roundf(t10);
  t12 = ((float)(t11));
  t13 = fgc_f2i8(t12);
  t0 = ((uint8_t)(t13));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_13(int64_t p0, gg_slice p1, int64_t p2, float p3, gg_slice p4, int64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  int64_t t18;
  uint8_t * t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  gg_slice t23;
  int64_t t24;
  float t25;
  gg_slice t26;
  int64_t t27;
  float t28;
  int64_t v0;
  gg_slice v1;
  int64_t v2;
  float v3;
  gg_slice v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(122))));
  t2 = ((int64_t)(v0));
  fgc_witness(t1, t2);
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) < (t4));
  t6 = t5;
  if (!t6) {
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)((v1).len));
  t6 = ((t7) >= (t8));
  }
  t9 = t6;
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t11) < (t12));
  }
  t13 = t10;
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v5));
  t16 = ((int64_t)((v4).len));
  t14 = ((t15) >= (t16));
  }
  t17 = t14;
  if (t17) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t18 = v5;
  t19 = ((uint8_t *)(v4).data) + gg_index(t18, (v4).len);
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  t23 = v1;
  t24 = ((int64_t)(v2));
  t25 = ((float)(v3));
  (*t19) = gg_function_3(t23, t24, t25, base + 0);
  }
  } else {
  {
  t26 = v1;
  t27 = ((int64_t)(v2));
  t28 = ((float)(v3));
  (*t19) = gg_function_9(t26, t27, t28, base + 0);
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_8(gg_slice p0, gg_slice p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  gg_slice t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  float * t20;
  float t21;
  float t22;
  uint8_t t23;
  float t24;
  float t25;
  float t26;
  float t27;
  uint8_t t28;
  float t29;
  float t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  gg_slice t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t * t43;
  gg_slice t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  float t48;
  int64_t t49;
  int64_t t50;
  float t51;
  float t52;
  int64_t t53;
  gg_slice t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  gg_slice v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  float v5;
  float v6;
  int64_t v7;
  float v8;
  float v9;
  int64_t v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(101))));
  t2 = ((int64_t)(v2));
  fgc_witness(t1, t2);
  t3 = ((int64_t)((v0).len));
  t4 = ((int64_t)(((int64_t)UINT64_C(128))));
  t5 = ((t3) < (t4));
  t6 = t5;
  if (!t6) {
  t7 = ((int64_t)((v1).len));
  t8 = ((int64_t)(((int64_t)UINT64_C(144))));
  t6 = ((t7) < (t8));
  }
  t9 = t6;
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t10) < (t11)))) goto label2;
  {
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(32))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  v5 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v6 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t14 = ((int64_t)(v7));
  t15 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t14) < (t15)))) goto label5;
  {
  t16 = v0;
  t17 = ((int64_t)(v4));
  t18 = ((int64_t)(v7));
  t19 = ((int64_t)((uint64_t)(t17) + (uint64_t)(t18)));
  t20 = ((float *)t16.data) + gg_index(t19, t16.len);
  v8 = ((float)((*t20)));
  t21 = ((float)(v8));
  t22 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t23 = ((t21) < (t22));
  if (t23) {
  {
  t24 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t25 = ((float)(v8));
  v9 = ((float)(((t24) - (t25))));
  }
  } else {
  {
  v9 = ((float)(v8));
  }
  }
  t26 = ((float)(v9));
  t27 = ((float)(v5));
  t28 = ((t26) > (t27));
  if (t28) {
  {
  v5 = ((float)(v9));
  }
  }
  t29 = ((float)(v6));
  t30 = ((float)(v8));
  v6 = ((float)(((t29) + (t30))));
  t31 = v7;
  t32 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t31) + (uint64_t)(t32)));
  }
  goto label4;
label5:;
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t33 = ((int64_t)(v7));
  t34 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t33) < (t34)))) goto label8;
  {
  t35 = v1;
  t36 = ((int64_t)(((int64_t)UINT64_C(16))));
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(((int64_t)UINT64_C(32))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t39)))));
  t41 = ((int64_t)(v7));
  t42 = ((int64_t)((uint64_t)(t40) + (uint64_t)(t41)));
  t43 = ((uint8_t *)t35.data) + gg_index(t42, t35.len);
  t44 = v0;
  t45 = ((int64_t)(v4));
  t46 = ((int64_t)(v7));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  t48 = ((float)(v5));
  (*t43) = gg_function_9(t44, t47, t48, base + 0);
  t49 = v7;
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t49) + (uint64_t)(t50)));
  }
  goto label7;
label8:;
  t51 = ((float)(v5));
  t52 = ((float)(v6));
  t53 = ((int64_t)(v2));
  t54 = v1;
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = ((int64_t)(v3));
  v10 = gg_function_10(t51, t52, t53, t54, t55, t56, base + 0);
  t57 = ((int64_t)(v10));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t59 = ((t57) != (t58));
  if (t59) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t60 = v3;
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t60) + (uint64_t)(t61)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ int64_t gg_function_1(gg_slice p0, gg_slice p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  float * t13;
  float t14;
  float t15;
  uint8_t t16;
  float t17;
  float t18;
  float t19;
  float t20;
  uint8_t t21;
  float t22;
  float t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  gg_slice t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t * t32;
  gg_slice t33;
  int64_t t34;
  float t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  float t40;
  float t41;
  gg_slice t42;
  int64_t t43;
  float t44;
  uint16_t t45;
  uint16_t t46;
  gg_slice t47;
  int64_t t48;
  float t49;
  uint16_t t50;
  uint16_t t51;
  gg_slice v0;
  gg_slice v1;
  float v2;
  float v3;
  int64_t v4;
  float v5;
  float v6;
  float v7;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(100))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  fgc_witness(t1, t2);
  t3 = ((int64_t)((v0).len));
  t4 = ((int64_t)(((int64_t)UINT64_C(32))));
  t5 = ((t3) < (t4));
  t6 = t5;
  if (!t6) {
  t7 = ((int64_t)((v1).len));
  t8 = ((int64_t)(((int64_t)UINT64_C(36))));
  t6 = ((t7) < (t8));
  }
  t9 = t6;
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  v2 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v3 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t10) < (t11)))) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t12 = v4;
  t13 = ((float *)(v0).data) + gg_index(t12, (v0).len);
  v5 = ((float)((*t13)));
  t14 = ((float)(v5));
  t15 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t16 = ((t14) < (t15));
  if (t16) {
  {
  t17 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t18 = ((float)(v5));
  v6 = ((float)(((t17) - (t18))));
  }
  } else {
  {
  v6 = ((float)(v5));
  }
  }
  t19 = ((float)(v6));
  t20 = ((float)(v2));
  t21 = ((t19) > (t20));
  if (t21) {
  {
  v2 = ((float)(v6));
  }
  }
  t22 = ((float)(v3));
  t23 = ((float)(v5));
  v3 = ((float)(((t22) + (t23))));
  t24 = v4;
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t24) + (uint64_t)(t25)));
  }
  goto label1;
label2:;
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t26) < (t27)))) goto label5;
  {
  t28 = v1;
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)((uint64_t)(t29) + (uint64_t)(t30)));
  t32 = ((uint8_t *)t28.data) + gg_index(t31, t28.len);
  t33 = v0;
  t34 = ((int64_t)(v4));
  t35 = ((float)(v2));
  (*t32) = gg_function_3(t33, t34, t35, base + 0);
  t36 = v4;
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t36) + (uint64_t)(t37)));
  }
  goto label4;
label5:;
  t38 = ((int64_t)(((int64_t)UINT64_C(123))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  fgc_witness(t38, t39);
  t40 = ((float)(v2));
  t41 = ((float)(((float)(((int64_t)UINT64_C(127))))));
  v7 = ((float)(((t40) / (t41))));
  t42 = v1;
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = ((float)(v7));
  t45 = fgc_to_half(t44);
  t46 = ((uint16_t)(t45));
  gg_function_7(t42, t43, t46, base + 0);
  t47 = v1;
  t48 = ((int64_t)(((int64_t)UINT64_C(2))));
  t49 = ((float)(v3));
  t50 = fgc_to_half(t49);
  t51 = ((uint16_t)(t50));
  gg_function_7(t47, t48, t51, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  float t12;
  float t13;
  int64_t t14;
  float t15;
  float t16;
  int64_t t17;
  int64_t t18;
  gg_slice t19;
  gg_slice t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  gg_slice t24;
  gg_slice t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  gg_slice t31;
  int64_t t32;
  float t33;
  gg_slice t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  gg_slice t40;
  int64_t t41;
  float t42;
  gg_slice t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  float t48;
  float t49;
  gg_slice t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  float t55;
  float t56;
  int64_t t57;
  gg_slice t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  gg_slice t65;
  int64_t t66;
  gg_slice t67;
  int64_t t68;
  gg_slice t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  gg_slice t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  gg_slice t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  gg_slice t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  gg_slice t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  gg_slice t96;
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
  gg_slice t107;
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
  gg_slice t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  gg_slice t124;
  gg_slice t125;
  gg_slice t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  gg_slice t136;
  gg_slice t137;
  gg_slice t138;
  gg_slice t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  gg_slice t148;
  gg_slice t149;
  gg_slice t150;
  gg_slice t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  gg_header t158;
  int64_t t159;
  gg_type_2 v0;
  gg_type_2 v1;
  gg_type_2 v2;
  gg_type_2 v3;
  gg_type_4 v4;
  gg_type_5 v5;
  int64_t v6;
  {
  memset(&t0, 0, sizeof(t0));
  t1 = ((uint8_t)(t0));
  for (t2=0; t2<8192; t2++) (v0).data[t2] = t1;
  memset(&t3, 0, sizeof(t3));
  t4 = ((uint8_t)(t3));
  for (t5=0; t5<8192; t5++) (v1).data[t5] = t4;
  memset(&t6, 0, sizeof(t6));
  t7 = ((uint8_t)(t6));
  for (t8=0; t8<8192; t8++) (v2).data[t8] = t7;
  memset(&t9, 0, sizeof(t9));
  t10 = ((uint8_t)(t9));
  for (t11=0; t11<8192; t11++) (v3).data[t11] = t10;
  memset(&t12, 0, sizeof(t12));
  t13 = ((float)(t12));
  for (t14=0; t14<1024; t14++) (v4).data[t14] = t13;
  memset(&t15, 0, sizeof(t15));
  t16 = ((float)(t15));
  for (t17=0; t17<512; t17++) (v5).data[t17] = t16;
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = v6;
  t19 = gg_view((v5).data, 512);
  t20 = gg_view((v2).data, 8192);
  t21 = gg_function_1(t19, t20, base + 0);
  t22 = ((int64_t)(t21));
  v6 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t22)));
  t23 = v6;
  t24 = gg_view((v5).data, 512);
  t25 = gg_view((v2).data, 8192);
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = gg_function_8(t24, t25, t26, base + 0);
  t28 = ((int64_t)(t27));
  v6 = ((int64_t)((uint64_t)(t23) + (uint64_t)(t28)));
  t29 = v6;
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = gg_view((v5).data, 512);
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t34 = gg_view((v2).data, 8192);
  t35 = ((int64_t)(((int64_t)UINT64_C(4))));
  t36 = gg_function_13(t30, t31, t32, t33, t34, t35, base + 0);
  t37 = ((int64_t)(t36));
  v6 = ((int64_t)((uint64_t)(t29) + (uint64_t)(t37)));
  t38 = v6;
  t39 = ((int64_t)(((int64_t)UINT64_C(1))));
  t40 = gg_view((v5).data, 512);
  t41 = ((int64_t)(((int64_t)UINT64_C(0))));
  t42 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t43 = gg_view((v2).data, 8192);
  t44 = ((int64_t)(((int64_t)UINT64_C(16))));
  t45 = gg_function_13(t39, t40, t41, t42, t43, t44, base + 0);
  t46 = ((int64_t)(t45));
  v6 = ((int64_t)((uint64_t)(t38) + (uint64_t)(t46)));
  t47 = v6;
  t48 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t49 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t50 = gg_view((v2).data, 8192);
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = gg_function_14(t48, t49, t50, t51, base + 0);
  t53 = ((int64_t)(t52));
  v6 = ((int64_t)((uint64_t)(t47) + (uint64_t)(t53)));
  t54 = v6;
  t55 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t56 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = gg_view((v2).data, 8192);
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = gg_function_10(t55, t56, t57, t58, t59, t60, base + 0);
  t62 = ((int64_t)(t61));
  v6 = ((int64_t)((uint64_t)(t54) + (uint64_t)(t62)));
  t63 = v6;
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = gg_view((v0).data, 8192);
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = gg_view((v2).data, 8192);
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = gg_view((v4).data, 1024);
  t70 = gg_function_15(t64, t65, t66, t67, t68, t69, base + 0);
  t71 = ((int64_t)(t70));
  v6 = ((int64_t)((uint64_t)(t63) + (uint64_t)(t71)));
  t72 = v6;
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = gg_view((v0).data, 8192);
  t75 = ((int64_t)(((int64_t)UINT64_C(1))));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  t79 = gg_view((v2).data, 8192);
  t80 = ((int64_t)(((int64_t)UINT64_C(160))));
  t81 = gg_function_31(t73, t74, t75, t76, t77, t78, t79, t80, base + 0);
  t82 = ((int64_t)(t81));
  v6 = ((int64_t)((uint64_t)(t72) + (uint64_t)(t82)));
  t83 = v6;
  t84 = gg_view((v2).data, 8192);
  t85 = ((int64_t)(((int64_t)UINT64_C(1))));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  t89 = gg_view((v3).data, 8192);
  t90 = gg_function_32(t84, t85, t86, t87, t88, t89, base + 0);
  t91 = ((int64_t)(t90));
  v6 = ((int64_t)((uint64_t)(t83) + (uint64_t)(t91)));
  t92 = v6;
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  t94 = ((int64_t)(((int64_t)UINT64_C(32))));
  t95 = ((int64_t)(((int64_t)UINT64_C(8))));
  t96 = gg_view((v4).data, 1024);
  t97 = gg_function_33(t93, t94, t95, t96, base + 0);
  t98 = ((int64_t)(t97));
  v6 = ((int64_t)((uint64_t)(t92) + (uint64_t)(t98)));
  t99 = v6;
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = ((int64_t)(((int64_t)UINT64_C(256))));
  t102 = ((int64_t)(((int64_t)UINT64_C(128))));
  t103 = ((int64_t)(((int64_t)UINT64_C(8))));
  t104 = ((int64_t)(((int64_t)UINT64_C(49152))));
  t105 = ((int64_t)(((int64_t)UINT64_C(800))));
  t106 = ((int64_t)(((int64_t)UINT64_C(64))));
  t107 = gg_view((v4).data, 1024);
  t108 = gg_function_35(t100, t101, t102, t103, t104, t105, t106, t107, base + 0);
  t109 = ((int64_t)(t108));
  v6 = ((int64_t)((uint64_t)(t99) + (uint64_t)(t109)));
  t110 = v6;
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = ((int64_t)(((int64_t)UINT64_C(1))));
  t113 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((int64_t)(((int64_t)UINT64_C(800))));
  t115 = ((int64_t)(((int64_t)UINT64_C(49152))));
  t116 = ((int64_t)(((int64_t)UINT64_C(256))));
  t117 = ((int64_t)(((int64_t)UINT64_C(8))));
  t118 = ((int64_t)(((int64_t)UINT64_C(64))));
  t119 = gg_view((v4).data, 1024);
  t120 = gg_function_39(t111, t112, t113, t114, t115, t116, t117, t118, t119, base + 0);
  t121 = ((int64_t)(t120));
  v6 = ((int64_t)((uint64_t)(t110) + (uint64_t)(t121)));
  t122 = v6;
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = gg_view((v0).data, 8192);
  t125 = gg_view((v2).data, 8192);
  t126 = gg_view((v4).data, 1024);
  t127 = ((int64_t)(((int64_t)UINT64_C(1))));
  t128 = ((int64_t)(((int64_t)UINT64_C(1))));
  t129 = ((int64_t)(((int64_t)UINT64_C(1))));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = ((int64_t)(((int64_t)UINT64_C(1))));
  t132 = gg_function_40(t123, t124, t125, t126, t127, t128, t129, t130, t131, base + 0);
  t133 = ((int64_t)(t132));
  v6 = ((int64_t)((uint64_t)(t122) + (uint64_t)(t133)));
  t134 = v6;
  t135 = ((int64_t)(((int64_t)UINT64_C(0))));
  t136 = gg_view((v0).data, 8192);
  t137 = gg_view((v2).data, 8192);
  t138 = gg_view((v3).data, 8192);
  t139 = gg_view((v4).data, 1024);
  t140 = ((int64_t)(((int64_t)UINT64_C(0))));
  t141 = ((int64_t)(((int64_t)UINT64_C(0))));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = ((int64_t)(((int64_t)UINT64_C(0))));
  t144 = gg_function_42(t135, t136, t137, t138, t139, t140, t141, t142, t143, base + 0);
  t145 = ((int64_t)(t144));
  v6 = ((int64_t)((uint64_t)(t134) + (uint64_t)(t145)));
  t146 = v6;
  t147 = ((int64_t)(((int64_t)UINT64_C(1))));
  t148 = gg_view((v0).data, 8192);
  t149 = gg_view((v2).data, 8192);
  t150 = gg_view((v3).data, 8192);
  t151 = gg_view((v4).data, 1024);
  t152 = ((int64_t)(((int64_t)UINT64_C(0))));
  t153 = ((int64_t)(((int64_t)UINT64_C(32))));
  t154 = ((int64_t)(((int64_t)UINT64_C(8))));
  t155 = ((int64_t)(((int64_t)UINT64_C(0))));
  t156 = gg_function_42(t147, t148, t149, t150, t151, t152, t153, t154, t155, base + 0);
  t157 = ((int64_t)(t156));
  v6 = ((int64_t)((uint64_t)(t146) + (uint64_t)(t157)));
  t158.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t158.len = 0;
  gg_integer(gg_ref(&t158, gg_get_stack(base + 0)), v6);
  gg_append(gg_ref(&t158, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t158).base, (t158).len));
  }
gg_done:;
  for (t159 = 0; t159 < 1; t159++) gg_stacks[base + t159].top = 0;
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
