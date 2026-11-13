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


typedef struct gg_type_1 gg_type_1;
#pragma pack(push, 1)
struct gg_type_1 {
  int64_t f0;
  int64_t f1;
  int64_t f2;
  int64_t f3;
  int64_t f4;
  int64_t f5;
  int64_t f6;
  int64_t f7;
  int64_t f8;
  int64_t f9;
  int64_t f10;
  int64_t f11;
  int64_t f12;
  int64_t f13;
  int64_t f14;
  int64_t f15;
  int64_t f16;
  int64_t f17;
  int64_t f18;
  int64_t f19;
  int64_t f20;
  int64_t f21;
  int64_t f22;
  int64_t f23;
  int64_t f24;
  int64_t f25;
  int64_t f26;
  int64_t f27;
  int64_t f28;
  int64_t f29;
  int64_t f30;
  int64_t f31;
  int64_t f32;
  int64_t f33;
  int64_t f34;
  int64_t f35;
  int64_t f36;
  int64_t f37;
  int64_t f38;
  int64_t f39;
  int64_t f40;
  int64_t f41;
  int64_t f42;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int64_t data[1]; } gg_type_3;
#pragma pack(pop)
typedef struct gg_type_4 gg_type_4;
#pragma pack(push, 1)
struct gg_type_4 {
  int32_t f43;
  int32_t f44;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_4 data[1]; } gg_type_6;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int32_t data[1]; } gg_type_7;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { uint8_t data[1]; } gg_type_9;
#pragma pack(pop)
typedef gg_type_1 *gg_type_10;
typedef gg_type_1 *gg_type_17;
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, gg_type_1 value);
static uint8_t gg_eq_2(int64_t a, int64_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, int64_t value);
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, gg_type_3 value);
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_type_4 value);
static uint8_t gg_eq_5(int32_t a, int32_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, int32_t value);
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, gg_type_6 value);
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, gg_type_7 value);
static uint8_t gg_eq_8(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, uint8_t value);
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, gg_type_9 value);
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, gg_type_10 value);
static uint8_t gg_eq_11(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, gg_slice value);
static uint8_t gg_eq_12(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, gg_slice value);
static uint8_t gg_eq_13(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_13(gg_slice a, gg_slice b);
static void gg_render_13(gg_reference out, gg_slice value);
static uint8_t gg_eq_14(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_14(gg_slice a, gg_slice b);
static void gg_render_14(gg_reference out, gg_slice value);
static uint8_t gg_eq_15(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_15(gg_slice a, gg_slice b);
static void gg_render_15(gg_reference out, uint8_t value);
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b);
static uint8_t gg_equal_elements_17(gg_slice a, gg_slice b);
static void gg_render_17(gg_reference out, gg_type_17 value);
static uint8_t gg_eq_18(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_18(gg_slice a, gg_slice b);
static void gg_render_18(gg_reference out, gg_slice value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((gg_type_1*)a.data)[i],((gg_type_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b) {
if(!gg_eq_2(a.f0,b.f0))return 0;
if(!gg_eq_2(a.f1,b.f1))return 0;
if(!gg_eq_2(a.f2,b.f2))return 0;
if(!gg_eq_2(a.f3,b.f3))return 0;
if(!gg_eq_2(a.f4,b.f4))return 0;
if(!gg_eq_2(a.f5,b.f5))return 0;
if(!gg_eq_2(a.f6,b.f6))return 0;
if(!gg_eq_2(a.f7,b.f7))return 0;
if(!gg_eq_2(a.f8,b.f8))return 0;
if(!gg_eq_2(a.f9,b.f9))return 0;
if(!gg_eq_2(a.f10,b.f10))return 0;
if(!gg_eq_2(a.f11,b.f11))return 0;
if(!gg_eq_2(a.f12,b.f12))return 0;
if(!gg_eq_2(a.f13,b.f13))return 0;
if(!gg_eq_2(a.f14,b.f14))return 0;
if(!gg_eq_2(a.f15,b.f15))return 0;
if(!gg_eq_2(a.f16,b.f16))return 0;
if(!gg_eq_2(a.f17,b.f17))return 0;
if(!gg_eq_2(a.f18,b.f18))return 0;
if(!gg_eq_2(a.f19,b.f19))return 0;
if(!gg_eq_2(a.f20,b.f20))return 0;
if(!gg_eq_2(a.f21,b.f21))return 0;
if(!gg_eq_2(a.f22,b.f22))return 0;
if(!gg_eq_2(a.f23,b.f23))return 0;
if(!gg_eq_2(a.f24,b.f24))return 0;
if(!gg_eq_2(a.f25,b.f25))return 0;
if(!gg_eq_2(a.f26,b.f26))return 0;
if(!gg_eq_2(a.f27,b.f27))return 0;
if(!gg_eq_2(a.f28,b.f28))return 0;
if(!gg_eq_2(a.f29,b.f29))return 0;
if(!gg_eq_2(a.f30,b.f30))return 0;
if(!gg_eq_2(a.f31,b.f31))return 0;
if(!gg_eq_2(a.f32,b.f32))return 0;
if(!gg_eq_2(a.f33,b.f33))return 0;
if(!gg_eq_2(a.f34,b.f34))return 0;
if(!gg_eq_2(a.f35,b.f35))return 0;
if(!gg_eq_2(a.f36,b.f36))return 0;
if(!gg_eq_2(a.f37,b.f37))return 0;
if(!gg_eq_2(a.f38,b.f38))return 0;
if(!gg_eq_2(a.f39,b.f39))return 0;
if(!gg_eq_2(a.f40,b.f40))return 0;
if(!gg_eq_2(a.f41,b.f41))return 0;
if(!gg_eq_2(a.f42,b.f42))return 0;
return 1;
}
static void gg_render_1(gg_reference out, gg_type_1 value) {
gg_append(out,"\120\154\141\156\040\173\040",7,1);
gg_render_2(out, value.f0);
gg_append(out,", ",2,1);
gg_render_2(out, value.f1);
gg_append(out,", ",2,1);
gg_render_2(out, value.f2);
gg_append(out,", ",2,1);
gg_render_2(out, value.f3);
gg_append(out,", ",2,1);
gg_render_2(out, value.f4);
gg_append(out,", ",2,1);
gg_render_2(out, value.f5);
gg_append(out,", ",2,1);
gg_render_2(out, value.f6);
gg_append(out,", ",2,1);
gg_render_2(out, value.f7);
gg_append(out,", ",2,1);
gg_render_2(out, value.f8);
gg_append(out,", ",2,1);
gg_render_2(out, value.f9);
gg_append(out,", ",2,1);
gg_render_2(out, value.f10);
gg_append(out,", ",2,1);
gg_render_2(out, value.f11);
gg_append(out,", ",2,1);
gg_render_2(out, value.f12);
gg_append(out,", ",2,1);
gg_render_2(out, value.f13);
gg_append(out,", ",2,1);
gg_render_2(out, value.f14);
gg_append(out,", ",2,1);
gg_render_2(out, value.f15);
gg_append(out,", ",2,1);
gg_render_2(out, value.f16);
gg_append(out,", ",2,1);
gg_render_2(out, value.f17);
gg_append(out,", ",2,1);
gg_render_2(out, value.f18);
gg_append(out,", ",2,1);
gg_render_2(out, value.f19);
gg_append(out,", ",2,1);
gg_render_2(out, value.f20);
gg_append(out,", ",2,1);
gg_render_2(out, value.f21);
gg_append(out,", ",2,1);
gg_render_2(out, value.f22);
gg_append(out,", ",2,1);
gg_render_2(out, value.f23);
gg_append(out,", ",2,1);
gg_render_2(out, value.f24);
gg_append(out,", ",2,1);
gg_render_2(out, value.f25);
gg_append(out,", ",2,1);
gg_render_2(out, value.f26);
gg_append(out,", ",2,1);
gg_render_2(out, value.f27);
gg_append(out,", ",2,1);
gg_render_2(out, value.f28);
gg_append(out,", ",2,1);
gg_render_2(out, value.f29);
gg_append(out,", ",2,1);
gg_render_2(out, value.f30);
gg_append(out,", ",2,1);
gg_render_2(out, value.f31);
gg_append(out,", ",2,1);
gg_render_2(out, value.f32);
gg_append(out,", ",2,1);
gg_render_2(out, value.f33);
gg_append(out,", ",2,1);
gg_render_2(out, value.f34);
gg_append(out,", ",2,1);
gg_render_2(out, value.f35);
gg_append(out,", ",2,1);
gg_render_2(out, value.f36);
gg_append(out,", ",2,1);
gg_render_2(out, value.f37);
gg_append(out,", ",2,1);
gg_render_2(out, value.f38);
gg_append(out,", ",2,1);
gg_render_2(out, value.f39);
gg_append(out,", ",2,1);
gg_render_2(out, value.f40);
gg_append(out,", ",2,1);
gg_render_2(out, value.f41);
gg_append(out,", ",2,1);
gg_render_2(out, value.f42);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((gg_type_3*)a.data)[i],((gg_type_3*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b) {
return gg_equal_elements_2(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_3(gg_reference out, gg_type_3 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_2(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_type_4*)a.data)[i],((gg_type_4*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_type_4 a, gg_type_4 b) {
if(!gg_eq_5(a.f43,b.f43))return 0;
if(!gg_eq_5(a.f44,b.f44))return 0;
return 1;
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
gg_append(out,"\122\157\167\040\173\040",6,1);
gg_render_5(out, value.f43);
gg_append(out,", ",2,1);
gg_render_5(out, value.f44);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((gg_type_6*)a.data)[i],((gg_type_6*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(gg_type_6 a, gg_type_6 b) {
return gg_equal_elements_4(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_6(gg_reference out, gg_type_6 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_4(out, ((gg_type_4*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((gg_type_7*)a.data)[i],((gg_type_7*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(gg_type_7 a, gg_type_7 b) {
return gg_equal_elements_5(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
int64_t i; gg_slice view=gg_view((value).data, 1);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_8(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((gg_type_9*)a.data)[i],((gg_type_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b) {
return gg_equal_elements_8(gg_view((a).data, 1),gg_view((b).data, 1));
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
gg_text(out,gg_view((value).data, 1));
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((gg_type_10*)a.data)[i],((gg_type_10*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(gg_type_10 a, gg_type_10 b) {
return a == b;
}
static void gg_render_10(gg_reference out, gg_type_10 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(gg_slice a, gg_slice b) {
return gg_equal_elements_2(a,b);
}
static void gg_render_11(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_2(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(gg_slice a, gg_slice b) {
return gg_equal_elements_4(a,b);
}
static void gg_render_12(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_4(out, ((gg_type_4*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_13(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_13(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_13(gg_slice a, gg_slice b) {
return gg_equal_elements_5(a,b);
}
static void gg_render_13(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_5(out, ((int32_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_14(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_14(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_14(gg_slice a, gg_slice b) {
return gg_equal_elements_8(a,b);
}
static void gg_render_14(gg_reference out, gg_slice value) {
gg_text(out,value);
}
static uint8_t gg_equal_elements_15(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_15(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_15(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_15(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_17(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_17(((gg_type_17*)a.data)[i],((gg_type_17*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_17(gg_type_17 a, gg_type_17 b) {
return a == b;
}
static void gg_render_17(gg_reference out, gg_type_17 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_18(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_18(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_18(gg_slice a, gg_slice b) {
return gg_equal_elements_4(a,b);
}
static void gg_render_18(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_4(out, ((gg_type_4*)view.data)[i]);}gg_append(out,"]",1,1);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(gg_type_10 p0, int64_t p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, int64_t base);
static void gg_function_2(gg_type_10 p0, int64_t base);
static int64_t gg_function_3(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_4(gg_type_17 p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t base);
static void gg_function_6(gg_type_17 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, int64_t base);
static int64_t gg_function_13(int64_t p0, int64_t p1, int64_t base);
static void gg_function_17(gg_type_17 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_slice p6, int64_t base);
static int64_t gg_function_13(int64_t p0, int64_t p1, int64_t base) {
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
static void gg_function_17(gg_type_17 p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, gg_slice p6, int64_t base) {
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
  gg_slice t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  gg_type_4 * t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
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
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
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
  gg_type_17 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  gg_slice v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  gg_type_4 v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((int64_t)(((*(v0))).f6));
  t1 = ((int64_t)(((*(v0))).f7));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  v8 = ((int64_t)(v4));
label1:;
  t2 = ((int64_t)(v8));
  t3 = ((int64_t)(v5));
  if (!(((t2) < (t3)))) goto label2;
  {
  v9 = ((int64_t)(v2));
label4:;
  t4 = ((int64_t)(v9));
  t5 = ((int64_t)(v3));
  if (!(((t4) < (t5)))) goto label5;
  {
  v10 = ((int64_t)(v8));
label7:;
  t6 = ((int64_t)(v10));
  t7 = ((int64_t)(v8));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t10 = ((int64_t)(v5));
  t11 = gg_function_13(t9, t10, base + 0);
  t12 = ((int64_t)(t11));
  if (!(((t6) < (t12)))) goto label8;
  {
  t13 = v6;
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(v10));
  t18 = ((int64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  t19 = ((gg_type_4 *)t13.data) + gg_index(t18, t13.len);
  v11 = (*t19);
  v12 = ((int64_t)(((int64_t)((v11).f43))));
  v13 = ((int64_t)(((int64_t)((v11).f44))));
  t20 = ((int64_t)(v12));
  t21 = ((int64_t)(((*(v0))).f12));
  v14 = ((int64_t)(((int64_t)gg_mod(t20, t21))));
  t22 = ((int64_t)(((*(v0))).f23));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t22) != (t23));
  t25 = t24;
  if (!t25) {
  t26 = ((int64_t)(((*(v0))).f8));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t26) != (t27));
  }
  t28 = t25;
  if (t28) {
  {
  t29 = ((int64_t)(v14));
  t30 = ((int64_t)(v13));
  t31 = ((int64_t)(((*(v0))).f12));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t32)))));
  t34 = ((int64_t)(((*(v0))).f20));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  }
  } else {
  {
  t35 = ((int64_t)(v14));
  t36 = ((int64_t)(((*(v0))).f16));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  t38 = ((int64_t)(v13));
  t39 = ((int64_t)(((*(v0))).f17));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t40)))));
  }
  }
  t41 = ((int64_t)(v12));
  t42 = ((int64_t)(((*(v0))).f41));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t44 = ((int64_t)(v13));
  t45 = ((int64_t)(((*(v0))).f42));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t46)))));
  t48 = ((int64_t)(v9));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t49)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t50)))));
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(((*(v0))).f40));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t54 = ((int64_t)(v9));
  t55 = ((int64_t)(((*(v0))).f39));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t56)))));
  t57 = ((int64_t)(v9));
  t58 = ((int64_t)(((int64_t)UINT64_C(16))));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t60 = ((int64_t)(v3));
  t61 = gg_function_13(t59, t60, base + 0);
  t62 = ((int64_t)(t61));
  t63 = ((int64_t)(v9));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t62) - (uint64_t)(t63)))));
  t64 = ((int64_t)(((*(v0))).f39));
  t65 = ((int64_t)(((*(v0))).f38));
  t66 = ((t64) == (t65));
  if (t66) {
  {
  t67 = ((int64_t)(((*(v0))).f0));
  t68 = ((int64_t)(v17));
  t69 = ((int64_t)(v15));
  t70 = ((int64_t)(v16));
  t71 = ((int64_t)(((*(v0))).f11));
  t72 = ((int64_t)(v18));
  fg140_matrix(t67, t68, t69, t70, t71, t72);
  }
  } else {
  {
  t74 = v18;
  t73 = 0;
label9:;
  if (t73 >= t74) goto label11;
  v19 = t73;
  {
  t75 = ((int64_t)(((*(v0))).f0));
  t76 = ((int64_t)(v17));
  t77 = ((int64_t)(v19));
  t78 = ((int64_t)(((*(v0))).f39));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t78)))));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t79)))));
  t81 = ((int64_t)(v15));
  t82 = ((int64_t)(v16));
  t83 = ((int64_t)(v19));
  t84 = ((int64_t)(((int64_t)UINT64_C(4))));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t83) * (uint64_t)(t84)))));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t85)))));
  t87 = ((int64_t)(((*(v0))).f11));
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg140_matrix(t75, t80, t81, t86, t87, t88);
  }
label10:;
  t73++; goto label9;
label11:;
  }
  }
  t89 = v10;
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)((uint64_t)(t89) + (uint64_t)(t90)));
  }
  goto label7;
label8:;
  t91 = v9;
  t92 = ((int64_t)(((int64_t)UINT64_C(16))));
  v9 = ((int64_t)((uint64_t)(t91) + (uint64_t)(t92)));
  }
  goto label4;
label5:;
  t93 = v8;
  t94 = ((int64_t)(((int64_t)UINT64_C(16))));
  v8 = ((int64_t)((uint64_t)(t93) + (uint64_t)(t94)));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_3(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
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
  int64_t t15;
  int64_t t16;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  if (!(t4)) gg_fail("assertion failed");
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  t10 = t9;
  if (!t10) {
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)gg_div(t12, t13))));
  t10 = ((t11) <= (t14));
  }
  if (!(t10)) gg_fail("assertion failed");
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_6(gg_type_17 p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
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
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
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
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
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
  int64_t t96;
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
  int64_t t107;
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
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  uint8_t * t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t * t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  gg_slice t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int32_t * t142;
  int64_t t143;
  int64_t t144;
  uint8_t t145;
  uint8_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  uint8_t t151;
  uint8_t t152;
  int64_t t153;
  int32_t * t154;
  int32_t t155;
  int32_t t156;
  uint8_t t157;
  int64_t t158;
  int64_t t159;
  uint8_t t160;
  int64_t t161;
  int32_t * t162;
  int64_t t163;
  int64_t t164;
  uint8_t t165;
  uint8_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t * t173;
  int64_t t174;
  int64_t t175;
  gg_type_4 * t176;
  int64_t t177;
  int64_t * t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int32_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t * t198;
  int64_t t199;
  int64_t t200;
  uint8_t t201;
  int64_t t202;
  int64_t t203;
  uint8_t t204;
  uint8_t t205;
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
  int64_t t218;
  uint8_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  uint8_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  uint8_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  uint8_t t240;
  uint8_t t241;
  int64_t t242;
  int64_t t243;
  uint8_t t244;
  uint8_t t245;
  int64_t t246;
  int64_t t247;
  uint8_t t248;
  uint8_t t249;
  int64_t t250;
  int64_t t251;
  uint8_t t252;
  uint8_t t253;
  int64_t t254;
  int64_t t255;
  uint8_t t256;
  uint8_t t257;
  int64_t t258;
  int64_t t259;
  uint8_t t260;
  uint8_t t261;
  int64_t t262;
  int64_t t263;
  uint8_t t264;
  uint8_t t265;
  int64_t t266;
  int64_t t267;
  uint8_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  uint8_t t273;
  int64_t t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  uint8_t t310;
  int64_t t311;
  int64_t t312;
  int32_t t313;
  int64_t t314;
  int64_t t315;
  uint8_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  uint8_t t320;
  uint8_t t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  uint8_t t325;
  uint8_t t326;
  int64_t t327;
  int64_t t328;
  uint8_t t329;
  uint8_t t330;
  int64_t t331;
  int64_t t332;
  uint8_t t333;
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
  uint8_t t352;
  uint8_t t353;
  int64_t t354;
  int64_t t355;
  uint8_t t356;
  int64_t t357;
  int64_t t358;
  uint8_t t359;
  int64_t t360;
  int64_t t361;
  uint8_t t362;
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
  int64_t t382;
  int64_t t383;
  int64_t t384;
  int64_t t385;
  int64_t t386;
  int64_t t387;
  int64_t t388;
  uint8_t t389;
  gg_type_17 t390;
  int64_t t391;
  int64_t t392;
  int64_t t393;
  int64_t t394;
  int64_t t395;
  int64_t t396;
  int64_t t397;
  int64_t t398;
  int64_t t399;
  int64_t t400;
  int64_t t401;
  int64_t t402;
  int64_t t403;
  gg_slice t404;
  int64_t t405;
  int64_t t406;
  int64_t t407;
  int64_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  int64_t t413;
  int64_t t414;
  int64_t t415;
  int64_t t416;
  int64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  int64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  int64_t t425;
  int64_t t426;
  uint8_t t427;
  int64_t t428;
  int64_t t429;
  int32_t t430;
  int64_t t431;
  int64_t t432;
  gg_type_17 v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
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
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  uint8_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  uint8_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((int64_t)((v1).len));
  t1 = ((int64_t)(((*(v0))).f4));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)((v2).len));
  t5 = ((int64_t)(((*(v0))).f4));
  t6 = ((int64_t)(((*(v0))).f6));
  t7 = gg_function_3(t5, t6, base + 0);
  t8 = ((int64_t)(((*(v0))).f7));
  t9 = gg_function_3(t7, t8, base + 0);
  t10 = ((int64_t)(t9));
  t3 = ((t4) >= (t10));
  }
  if (!(t3)) gg_fail("assertion failed");
  t11 = ((int64_t)(((*(v0))).f21));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) >= (t12));
  t14 = t13;
  if (t14) {
  t15 = ((int64_t)(((*(v0))).f22));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t15) >= (t16));
  }
  t17 = t14;
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(((*(v0))).f31));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t19) >= (t20));
  }
  if (!(t18)) gg_fail("assertion failed");
  t21 = ((int64_t)(((*(v0))).f1));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) != (t22));
  if (t23) {
  {
  t24 = ((int64_t)((v4).len));
  t25 = ((int64_t)(((*(v0))).f5));
  t26 = ((t24) >= (t25));
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)((v6).len));
  t29 = ((int64_t)(((*(v0))).f31));
  t27 = ((t28) >= (t29));
  }
  if (!(t27)) gg_fail("assertion failed");
  }
  }
  t30 = ((int64_t)(((*(v0))).f1));
  t31 = ((int64_t)(((int64_t)UINT64_C(2))));
  t32 = ((t30) == (t31));
  if (t32) {
  {
  t33 = ((int64_t)((v5).len));
  t34 = ((int64_t)(((*(v0))).f5));
  if (!(((t33) >= (t34)))) gg_fail("assertion failed");
  }
  }
  t35 = ((int64_t)(((*(v0))).f0));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg140_witness(t35, t36, t37);
  t38 = ((int64_t)(((*(v0))).f8));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) != (t39));
  if (t40) {
  {
  t41 = ((int64_t)(((*(v0))).f20));
  t42 = ((int64_t)(((*(v0))).f12));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t43 = ((int64_t)(v7));
  t44 = ((int64_t)(((*(v0))).f13));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t45 = ((int64_t)(((*(v0))).f2));
  t46 = ((int64_t)(((*(v0))).f11));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = ((int64_t)(((*(v0))).f9));
  t49 = ((int64_t)(((int64_t)gg_div(t47, t48))));
  t50 = ((int64_t)(((*(v0))).f3));
  v9 = ((int64_t)(((int64_t)gg_div(t49, t50))));
  t51 = ((int64_t)(((*(v0))).f2));
  t52 = ((int64_t)(((int64_t)UINT64_C(1))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) + (uint64_t)(t52)))));
  t54 = ((int64_t)(((*(v0))).f11));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((*(v0))).f9));
  t57 = ((int64_t)(((int64_t)gg_div(t55, t56))));
  t58 = ((int64_t)(((*(v0))).f3));
  v10 = ((int64_t)(((int64_t)gg_div(t57, t58))));
  t60 = ((*(v0))).f14;
  t59 = 0;
label0:;
  if (t59 >= t60) goto label2;
  v11 = t59;
  {
  t62 = ((*(v0))).f13;
  t61 = 0;
label3:;
  if (t61 >= t62) goto label5;
  v12 = t61;
  {
  t64 = ((*(v0))).f12;
  t63 = 0;
label6:;
  if (t63 >= t64) goto label8;
  v13 = t63;
  {
  t65 = ((int64_t)(v11));
  t66 = ((int64_t)(((*(v0))).f18));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  t68 = ((int64_t)(v12));
  t69 = ((int64_t)(((*(v0))).f17));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t69)))));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t70)))));
  t72 = ((int64_t)(v13));
  t73 = ((int64_t)(((*(v0))).f16));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t74)))));
  t76 = ((int64_t)(v9));
  t77 = ((int64_t)(((*(v0))).f9));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t76) * (uint64_t)(t77)))));
  t79 = ((int64_t)(((*(v0))).f15));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t80)))));
  t81 = ((int64_t)(v11));
  t82 = ((int64_t)(v8));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t82)))));
  t84 = ((int64_t)(v12));
  t85 = ((int64_t)(v7));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t83) + (uint64_t)(t86)))));
  t88 = ((int64_t)(v13));
  t89 = ((int64_t)(((*(v0))).f20));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) * (uint64_t)(t89)))));
  t91 = ((int64_t)(((int64_t)((uint64_t)(t87) + (uint64_t)(t90)))));
  t92 = ((int64_t)(v9));
  t93 = ((int64_t)(((*(v0))).f10));
  t94 = ((int64_t)(((int64_t)((uint64_t)(t92) * (uint64_t)(t93)))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t91) + (uint64_t)(t94)))));
  t95 = ((int64_t)(((*(v0))).f0));
  t96 = ((int64_t)(v14));
  t97 = ((int64_t)(v15));
  t98 = ((int64_t)(v10));
  t99 = ((int64_t)(v9));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) - (uint64_t)(t99)))));
  t101 = ((int64_t)(((*(v0))).f9));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t100) * (uint64_t)(t101)))));
  fg140_quantize(t95, t96, t97, t102);
  }
label7:;
  t63++; goto label6;
label8:;
  }
label4:;
  t61++; goto label3;
label5:;
  }
label1:;
  t59++; goto label0;
label2:;
  t103 = ((int64_t)(((*(v0))).f0));
  t104 = ((int64_t)(((int64_t)UINT64_C(1))));
  t105 = ((int64_t)(v10));
  t106 = ((int64_t)(v9));
  t107 = ((int64_t)(((int64_t)((uint64_t)(t105) - (uint64_t)(t106)))));
  t108 = ((int64_t)(((*(v0))).f12));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t107) * (uint64_t)(t108)))));
  t110 = ((int64_t)(((*(v0))).f13));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t109) * (uint64_t)(t110)))));
  t112 = ((int64_t)(((*(v0))).f14));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t111) * (uint64_t)(t112)))));
  fg140_witness(t103, t104, t113);
  }
  }
  t114 = ((int64_t)(((*(v0))).f6));
  t115 = ((int64_t)(((*(v0))).f7));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t114) * (uint64_t)(t115)))));
  t116 = ((int64_t)(((*(v0))).f2));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t118 = ((t116) == (t117));
  if (t118) {
  {
  t119 = ((int64_t)(((*(v0))).f1));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = ((t119) != (t120));
  if (t121) {
  {
  t123 = ((*(v0))).f31;
  t122 = 0;
label9:;
  if (t122 >= t123) goto label11;
  v17 = t122;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t124 = v17;
  t125 = ((uint8_t *)(v6).data) + gg_index(t124, (v6).len);
  (*t125) = ((uint8_t)(((int64_t)UINT64_C(0))));
  }
label10:;
  t122++; goto label9;
label11:;
  }
  }
  t127 = ((*(v0))).f4;
  t126 = 0;
label12:;
  if (t126 >= t127) goto label14;
  v18 = t126;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t128 = v18;
  t129 = ((int64_t *)(v1).data) + gg_index(t128, (v1).len);
  (*t129) = ((int64_t)(((int64_t)UINT64_C(0))));
  }
label13:;
  t126++; goto label12;
label14:;
  t131 = ((*(v0))).f7;
  t130 = 0;
label15:;
  if (t130 >= t131) goto label17;
  v19 = t130;
  {
  t133 = ((*(v0))).f6;
  t132 = 0;
label18:;
  if (t132 >= t133) goto label20;
  v20 = t132;
  {
  t134 = v3;
  t135 = ((int64_t)(v19));
  t136 = ((int64_t)(((*(v0))).f22));
  t137 = ((int64_t)(((int64_t)((uint64_t)(t135) * (uint64_t)(t136)))));
  t138 = ((int64_t)(v20));
  t139 = ((int64_t)(((*(v0))).f21));
  t140 = ((int64_t)(((int64_t)((uint64_t)(t138) * (uint64_t)(t139)))));
  t141 = ((int64_t)((uint64_t)(t137) + (uint64_t)(t140)));
  t142 = ((int32_t *)t134.data) + gg_index(t141, t134.len);
  v21 = ((int64_t)(((int64_t)((*t142)))));
  t143 = ((int64_t)(v21));
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t145 = ((t143) >= (t144));
  t146 = t145;
  if (t146) {
  t147 = ((int64_t)(v21));
  t148 = ((int64_t)(((*(v0))).f5));
  t146 = ((t147) < (t148));
  }
  if (!(t146)) gg_fail("assertion failed");
  t149 = ((int64_t)(((*(v0))).f1));
  t150 = ((int64_t)(((int64_t)UINT64_C(0))));
  t151 = ((t149) == (t150));
  t152 = t151;
  if (!t152) {
  /* FG_DIRECT_SLICE_INDEX */
  t153 = v21;
  t154 = ((int32_t *)(v4).data) + gg_index(t153, (v4).len);
  t155 = ((int32_t)((*t154)));
  t156 = ((int32_t)(((int64_t)UINT64_C(0))));
  t152 = ((t155) != (t156));
  }
  t157 = t152;
  if (t157) {
  {
  t158 = ((int64_t)(((*(v0))).f1));
  t159 = ((int64_t)(((int64_t)UINT64_C(2))));
  t160 = ((t158) == (t159));
  if (t160) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t161 = v21;
  t162 = ((int32_t *)(v5).data) + gg_index(t161, (v5).len);
  v22 = ((int64_t)(((int64_t)((*t162)))));
  }
  } else {
  {
  v22 = ((int64_t)(v21));
  }
  }
  t163 = ((int64_t)(v22));
  t164 = ((int64_t)(((int64_t)UINT64_C(0))));
  t165 = ((t163) >= (t164));
  t166 = t165;
  if (t166) {
  t167 = ((int64_t)(v22));
  t168 = ((int64_t)(((*(v0))).f4));
  t166 = ((t167) < (t168));
  }
  if (!(t166)) gg_fail("assertion failed");
  t169 = ((int64_t)(v22));
  t170 = ((int64_t)(v16));
  t171 = ((int64_t)(((int64_t)((uint64_t)(t169) * (uint64_t)(t170)))));
  /* FG_DIRECT_SLICE_INDEX */
  t172 = v22;
  t173 = ((int64_t *)(v1).data) + gg_index(t172, (v1).len);
  t174 = ((int64_t)((*t173)));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t171) + (uint64_t)(t174)))));
  /* FG_DIRECT_SLICE_INDEX */
  t175 = v23;
  t176 = ((gg_type_4 *)(v2).data) + gg_index(t175, (v2).len);
  memset(&((*t176)), 0, sizeof((*t176)));
  ((*t176)).f43 = ((int32_t)(((int32_t)(v20))));
  ((*t176)).f44 = ((int32_t)(((int32_t)(v19))));
  /* FG_DIRECT_SLICE_INDEX */
  t177 = v22;
  t178 = ((int64_t *)(v1).data) + gg_index(t177, (v1).len);
  t179 = (*t178);
  t180 = ((int64_t)(((int64_t)UINT64_C(1))));
  (*t178) = ((int64_t)((uint64_t)(t179) + (uint64_t)(t180)));
  }
  }
  }
label19:;
  t132++; goto label18;
label20:;
  }
label16:;
  t130++; goto label15;
label17:;
  t181 = ((int64_t)(((*(v0))).f0));
  t182 = ((int64_t)(((int64_t)UINT64_C(2))));
  t183 = ((int64_t)(v16));
  fg140_witness(t181, t182, t183);
  t184 = ((int64_t)(((*(v0))).f0));
  t185 = ((int64_t)(((int64_t)UINT64_C(6))));
  t186 = ((int64_t)(((*(v0))).f36));
  fg140_witness(t184, t185, t186);
  }
  }
  v24 = ((int64_t)(((*(v0))).f2));
label22:;
  t187 = ((int64_t)(v24));
  t188 = ((int64_t)(((*(v0))).f4));
  if (!(((t187) < (t188)))) goto label23;
  {
  t189 = ((int64_t)(((*(v0))).f0));
  t190 = ((int64_t)(v24));
  t191 = ((int32_t)(((int32_t)(((*(v0))).f3))));
  fg140_reset(t189, t190, t191);
  t192 = v24;
  t193 = ((int64_t)(((*(v0))).f3));
  v24 = ((int64_t)((uint64_t)(t192) + (uint64_t)(t193)));
  }
  goto label22;
label23:;
  t194 = ((int64_t)(((*(v0))).f0));
  fg140_barrier(t194);
  v24 = ((int64_t)(((int64_t)UINT64_C(0))));
label25:;
  t195 = ((int64_t)(v24));
  t196 = ((int64_t)(((*(v0))).f4));
  if (!(((t195) < (t196)))) goto label26;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t197 = v24;
  t198 = ((int64_t *)(v1).data) + gg_index(t197, (v1).len);
  v25 = ((int64_t)((*t198)));
  t199 = ((int64_t)(v25));
  t200 = ((int64_t)(((int64_t)UINT64_C(0))));
  t201 = ((t199) > (t200));
  if (t201) {
  {
  t202 = ((int64_t)(((*(v0))).f1));
  t203 = ((int64_t)(((int64_t)UINT64_C(0))));
  t204 = ((t202) == (t203));
  t205 = t204;
  if (t205) {
  t206 = ((int64_t)(((*(v0))).f24));
  t207 = ((int64_t)(((int64_t)UINT64_C(0))));
  t205 = ((t206) != (t207));
  }
  t208 = t205;
  t209 = t208;
  if (t209) {
  t210 = ((int64_t)(((*(v0))).f2));
  t211 = ((int64_t)(((int64_t)UINT64_C(0))));
  t209 = ((t210) == (t211));
  }
  t212 = t209;
  if (t212) {
  {
  t213 = ((int64_t)(((*(v0))).f0));
  t214 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg140_iq_stats(t213, t214);
  t215 = ((int64_t)(((*(v0))).f0));
  t217 = ((int64_t)(v25));
  t218 = ((int64_t)(((int64_t)UINT64_C(4))));
  t219 = ((t217) >= (t218));
  if (t219) {
  {
  t216 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  } else {
  {
  t220 = ((int64_t)(v25));
  t221 = ((int64_t)(((int64_t)UINT64_C(1))));
  t216 = ((int64_t)(((int64_t)((uint64_t)(t220) - (uint64_t)(t221)))));
  }
  }
  t222 = ((int64_t)(t216));
  fg140_iq_stats(t215, t222);
  t223 = ((int64_t)(((*(v0))).f0));
  t224 = ((int64_t)(((int64_t)UINT64_C(8))));
  t226 = ((int64_t)(((*(v0))).f7));
  t227 = ((int64_t)(((int64_t)UINT64_C(4))));
  t228 = ((t226) >= (t227));
  if (t228) {
  {
  t225 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  } else {
  {
  t229 = ((int64_t)(((*(v0))).f7));
  t230 = ((int64_t)(((int64_t)UINT64_C(1))));
  t225 = ((int64_t)(((int64_t)((uint64_t)(t229) - (uint64_t)(t230)))));
  }
  }
  t231 = ((int64_t)(t225));
  t232 = ((int64_t)(((int64_t)((uint64_t)(t224) + (uint64_t)(t231)))));
  fg140_iq_stats(t223, t232);
  }
  }
  t233 = ((int64_t)(((*(v0))).f1));
  t234 = ((int64_t)(((int64_t)UINT64_C(0))));
  t235 = ((t233) == (t234));
  if (t235) {
  {
  t236 = ((int64_t)(((*(v0))).f0));
  t237 = ((int64_t)(v24));
  fg140_ready(t236, t237);
  }
  }
  t238 = ((int64_t)(((*(v0))).f1));
  t239 = ((int64_t)(((int64_t)UINT64_C(0))));
  t240 = ((t238) == (t239));
  t241 = t240;
  if (t241) {
  t242 = ((int64_t)(((*(v0))).f25));
  t243 = ((int64_t)(((int64_t)UINT64_C(0))));
  t241 = ((t242) != (t243));
  }
  t244 = t241;
  t245 = t244;
  if (t245) {
  t246 = ((int64_t)(((*(v0))).f24));
  t247 = ((int64_t)(((int64_t)UINT64_C(0))));
  t245 = ((t246) != (t247));
  }
  t248 = t245;
  t249 = t248;
  if (t249) {
  t250 = ((int64_t)(((*(v0))).f7));
  t251 = ((int64_t)(((int64_t)UINT64_C(2))));
  t249 = ((t250) >= (t251));
  }
  t252 = t249;
  t253 = t252;
  if (t253) {
  t254 = ((int64_t)(((*(v0))).f7));
  t255 = ((int64_t)(((int64_t)UINT64_C(3))));
  t253 = ((t254) <= (t255));
  }
  t256 = t253;
  t257 = t256;
  if (t257) {
  t258 = ((int64_t)(v25));
  t259 = ((int64_t)(((int64_t)UINT64_C(2))));
  t257 = ((t258) >= (t259));
  }
  t260 = t257;
  t261 = t260;
  if (t261) {
  t262 = ((int64_t)(v25));
  t263 = ((int64_t)(((int64_t)UINT64_C(3))));
  t261 = ((t262) <= (t263));
  }
  t264 = t261;
  t265 = t264;
  if (t265) {
  t266 = ((int64_t)(((*(v0))).f26));
  t267 = ((int64_t)(((int64_t)UINT64_C(0))));
  t265 = ((t266) == (t267));
  }
  v26 = t265;
  t268 = v26;
  if (t268) {
  {
  t269 = ((int64_t)(((*(v0))).f0));
  t270 = ((int64_t)(((int64_t)UINT64_C(14))));
  fg140_iq_stats(t269, t270);
  t271 = ((int64_t)(((*(v0))).f2));
  t272 = ((int64_t)(((int64_t)UINT64_C(0))));
  t273 = ((t271) == (t272));
  if (t273) {
  {
  t274 = ((int64_t)(((*(v0))).f0));
  t275 = ((int64_t)(((int64_t)UINT64_C(4))));
  t276 = ((int64_t)(v25));
  t277 = ((int64_t)(((int64_t)((uint64_t)(t275) + (uint64_t)(t276)))));
  t278 = ((int64_t)(((int64_t)UINT64_C(1))));
  t279 = ((int64_t)(((int64_t)((uint64_t)(t277) - (uint64_t)(t278)))));
  fg140_iq_stats(t274, t279);
  }
  }
  t280 = ((int64_t)(((*(v0))).f19));
  t281 = ((int64_t)(((int64_t)UINT64_C(15))));
  t282 = ((int64_t)(((int64_t)((uint64_t)(t280) + (uint64_t)(t281)))));
  t283 = ((int64_t)(((int64_t)UINT64_C(16))));
  v27 = ((int64_t)(((int64_t)gg_div(t282, t283))));
  t284 = ((int64_t)(((*(v0))).f19));
  t285 = ((int64_t)(v27));
  t286 = ((int64_t)(((int64_t)((uint64_t)(t284) + (uint64_t)(t285)))));
  t287 = ((int64_t)(((int64_t)UINT64_C(1))));
  t288 = ((int64_t)(((int64_t)((uint64_t)(t286) - (uint64_t)(t287)))));
  t289 = ((int64_t)(v27));
  v28 = ((int64_t)(((int64_t)gg_div(t288, t289))));
  v29 = ((int64_t)(((*(v0))).f2));
label28:;
  t290 = ((int64_t)(v29));
  t291 = ((int64_t)(v27));
  if (!(((t290) < (t291)))) goto label29;
  {
  t292 = ((int64_t)(v28));
  t293 = ((int64_t)(v29));
  v30 = ((int64_t)(((int64_t)((uint64_t)(t292) * (uint64_t)(t293)))));
  t294 = ((int64_t)(((*(v0))).f0));
  t295 = ((int64_t)(v24));
  t296 = ((int64_t)(v30));
  t297 = ((int64_t)(v30));
  t298 = ((int64_t)(v28));
  t299 = ((int64_t)(((int64_t)((uint64_t)(t297) + (uint64_t)(t298)))));
  t300 = ((int64_t)(((*(v0))).f19));
  t301 = gg_function_13(t299, t300, base + 0);
  t302 = ((int64_t)(((int64_t)UINT64_C(0))));
  t303 = ((int64_t)(v25));
  t304 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg140_chunk(t294, t295, t296, t301, t302, t303, t304);
  t305 = ((int64_t)(((*(v0))).f0));
  t306 = ((int64_t)(((int64_t)UINT64_C(4))));
  t307 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg140_witness(t305, t306, t307);
  t308 = ((int64_t)(((*(v0))).f3));
  t309 = ((int64_t)(v27));
  t310 = ((t308) >= (t309));
  if (t310) {
  {
  goto label29;
  }
  }
  t311 = ((int64_t)(((*(v0))).f0));
  t312 = ((int64_t)(v24));
  t313 = fg140_claim(t311, t312);
  v29 = ((int64_t)(((int64_t)(t313))));
  }
  goto label28;
label29:;
  }
  } else {
  {
  v31 = ((uint8_t)UINT64_C(0));
  t314 = ((int64_t)(((*(v0))).f1));
  t315 = ((int64_t)(((int64_t)UINT64_C(0))));
  t316 = ((t314) == (t315));
  if (t316) {
  {
  t317 = ((int64_t)(((*(v0))).f0));
  t318 = ((int64_t)(v24));
  t319 = ((int64_t)(v25));
  t320 = fg140_tiled(t317, t318, t319);
  v31 = t320;
  t321 = v31;
  if (t321) {
  {
  t322 = ((int64_t)(((*(v0))).f0));
  t323 = ((int64_t)(((int64_t)UINT64_C(5))));
  t324 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg140_witness(t322, t323, t324);
  }
  }
  }
  }
  t325 = v31;
  t326 = (!(t325));
  if (t326) {
  {
  v32 = ((int64_t)(((int64_t)UINT64_C(16))));
  t327 = ((int64_t)(((*(v0))).f19));
  t328 = ((int64_t)(((int64_t)UINT64_C(1))));
  t329 = ((t327) == (t328));
  t330 = t329;
  if (!t330) {
  t331 = ((int64_t)(v25));
  t332 = ((int64_t)(((int64_t)UINT64_C(1))));
  t330 = ((t331) == (t332));
  }
  t333 = t330;
  if (t333) {
  {
  v32 = ((int64_t)(((int64_t)UINT64_C(64))));
  }
  }
  t334 = ((int64_t)(((*(v0))).f19));
  t335 = ((int64_t)(v32));
  t336 = ((int64_t)(((int64_t)((uint64_t)(t334) + (uint64_t)(t335)))));
  t337 = ((int64_t)(((int64_t)UINT64_C(1))));
  t338 = ((int64_t)(((int64_t)((uint64_t)(t336) - (uint64_t)(t337)))));
  t339 = ((int64_t)(v32));
  v33 = ((int64_t)(((int64_t)gg_div(t338, t339))));
  t340 = ((int64_t)(v25));
  t341 = ((int64_t)(v32));
  t342 = ((int64_t)(((int64_t)((uint64_t)(t340) + (uint64_t)(t341)))));
  t343 = ((int64_t)(((int64_t)UINT64_C(1))));
  t344 = ((int64_t)(((int64_t)((uint64_t)(t342) - (uint64_t)(t343)))));
  t345 = ((int64_t)(v32));
  v34 = ((int64_t)(((int64_t)gg_div(t344, t345))));
  t346 = ((int64_t)(v33));
  t347 = ((int64_t)(v34));
  t348 = ((int64_t)(((int64_t)((uint64_t)(t346) * (uint64_t)(t347)))));
  t349 = ((int64_t)(((*(v0))).f3));
  t350 = ((int64_t)(((int64_t)UINT64_C(4))));
  t351 = ((int64_t)(((int64_t)((uint64_t)(t349) * (uint64_t)(t350)))));
  t352 = ((t348) < (t351));
  t353 = t352;
  if (!t353) {
  t354 = ((int64_t)(((*(v0))).f26));
  t355 = ((int64_t)(((int64_t)UINT64_C(0))));
  t353 = ((t354) != (t355));
  }
  t356 = t353;
  if (t356) {
  {
  t357 = ((int64_t)(((*(v0))).f19));
  t358 = ((int64_t)(v25));
  t359 = ((t357) > (t358));
  if (t359) {
  {
  v33 = ((int64_t)(((*(v0))).f3));
  }
  } else {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t360 = ((int64_t)(((*(v0))).f19));
  t361 = ((int64_t)(v25));
  t362 = ((t360) > (t361));
  if (t362) {
  {
  v34 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v34 = ((int64_t)(((*(v0))).f3));
  }
  }
  }
  }
  t363 = ((int64_t)(((*(v0))).f19));
  t364 = ((int64_t)(v33));
  t365 = ((int64_t)(((int64_t)((uint64_t)(t363) + (uint64_t)(t364)))));
  t366 = ((int64_t)(((int64_t)UINT64_C(1))));
  t367 = ((int64_t)(((int64_t)((uint64_t)(t365) - (uint64_t)(t366)))));
  t368 = ((int64_t)(v33));
  v35 = ((int64_t)(((int64_t)gg_div(t367, t368))));
  t369 = ((int64_t)(v25));
  t370 = ((int64_t)(v34));
  t371 = ((int64_t)(((int64_t)((uint64_t)(t369) + (uint64_t)(t370)))));
  t372 = ((int64_t)(((int64_t)UINT64_C(1))));
  t373 = ((int64_t)(((int64_t)((uint64_t)(t371) - (uint64_t)(t372)))));
  t374 = ((int64_t)(v34));
  v36 = ((int64_t)(((int64_t)gg_div(t373, t374))));
  v37 = ((int64_t)(((*(v0))).f2));
label31:;
  t375 = ((int64_t)(v37));
  t376 = ((int64_t)(v33));
  t377 = ((int64_t)(v34));
  t378 = ((int64_t)(((int64_t)((uint64_t)(t376) * (uint64_t)(t377)))));
  if (!(((t375) < (t378)))) goto label32;
  {
  t379 = ((int64_t)(v35));
  t380 = ((int64_t)(v37));
  t381 = ((int64_t)(v33));
  t382 = ((int64_t)(((int64_t)gg_mod(t380, t381))));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t379) * (uint64_t)(t382)))));
  t383 = ((int64_t)(v36));
  t384 = ((int64_t)(v37));
  t385 = ((int64_t)(v33));
  t386 = ((int64_t)(((int64_t)gg_div(t384, t385))));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t383) * (uint64_t)(t386)))));
  t387 = ((int64_t)(((*(v0))).f37));
  t388 = ((int64_t)(((int64_t)UINT64_C(0))));
  t389 = ((t387) != (t388));
  if (t389) {
  {
  t390 = v0;
  t391 = ((int64_t)(v24));
  t392 = ((int64_t)(v38));
  t393 = ((int64_t)(v38));
  t394 = ((int64_t)(v35));
  t395 = ((int64_t)(((int64_t)((uint64_t)(t393) + (uint64_t)(t394)))));
  t396 = ((int64_t)(((*(v0))).f19));
  t397 = gg_function_13(t395, t396, base + 0);
  t398 = ((int64_t)(v39));
  t399 = ((int64_t)(v39));
  t400 = ((int64_t)(v36));
  t401 = ((int64_t)(((int64_t)((uint64_t)(t399) + (uint64_t)(t400)))));
  t402 = ((int64_t)(v25));
  t403 = gg_function_13(t401, t402, base + 0);
  t404 = v2;
  gg_function_17(t390, t391, t392, t397, t398, t403, t404, base + 0);
  }
  } else {
  {
  t405 = ((int64_t)(((*(v0))).f0));
  t406 = ((int64_t)(v24));
  t407 = ((int64_t)(v38));
  t408 = ((int64_t)(v38));
  t409 = ((int64_t)(v35));
  t410 = ((int64_t)(((int64_t)((uint64_t)(t408) + (uint64_t)(t409)))));
  t411 = ((int64_t)(((*(v0))).f19));
  t412 = gg_function_13(t410, t411, base + 0);
  t413 = ((int64_t)(v39));
  t414 = ((int64_t)(v39));
  t415 = ((int64_t)(v36));
  t416 = ((int64_t)(((int64_t)((uint64_t)(t414) + (uint64_t)(t415)))));
  t417 = ((int64_t)(v25));
  t418 = gg_function_13(t416, t417, base + 0);
  t419 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg140_chunk(t405, t406, t407, t412, t413, t418, t419);
  }
  }
  t420 = ((int64_t)(((*(v0))).f0));
  t421 = ((int64_t)(((int64_t)UINT64_C(3))));
  t422 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg140_witness(t420, t421, t422);
  t423 = ((int64_t)(((*(v0))).f3));
  t424 = ((int64_t)(v33));
  t425 = ((int64_t)(v34));
  t426 = ((int64_t)(((int64_t)((uint64_t)(t424) * (uint64_t)(t425)))));
  t427 = ((t423) >= (t426));
  if (t427) {
  {
  goto label32;
  }
  }
  t428 = ((int64_t)(((*(v0))).f0));
  t429 = ((int64_t)(v24));
  t430 = fg140_claim(t428, t429);
  v37 = ((int64_t)(((int64_t)(t430))));
  }
  goto label31;
label32:;
  }
  }
  }
  }
  }
  }
  t431 = v24;
  t432 = ((int64_t)(((int64_t)UINT64_C(1))));
  v24 = ((int64_t)((uint64_t)(t431) + (uint64_t)(t432)));
  }
  goto label25;
label26:;
  }
gg_done:;
}
static int64_t gg_function_5(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t13;
  int64_t t14;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) - (uint64_t)(t11)))));
  t8 = ((t9) <= (t12));
  }
  if (!(t8)) gg_fail("assertion failed");
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_4(gg_type_17 p0, int64_t p1, int64_t p2, int64_t base) {
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
  gg_type_17 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  t9 = ((int64_t)(((t5) & (t8))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t9) == (t10));
  }
  if (!(t4)) gg_fail("assertion failed");
  t11 = ((int64_t)(((*(v0))).f29));
  t12 = ((int64_t)(v1));
  v3 = gg_function_5(t11, t12, base + 0);
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) - (uint64_t)(t15)))));
  v4 = gg_function_5(t13, t16, base + 0);
  t17 = ((int64_t)(v4));
  t18 = v2;
  t19 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t18)))));
  t20 = ((int64_t)(((t17) & (t19))));
  t21 = ((int64_t)(((*(v0))).f29));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t20) - (uint64_t)(t21)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_2(gg_type_10 p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
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
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  uint8_t t58;
  int64_t t59;
  int64_t t60;
  uint8_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint8_t t75;
  uint8_t t76;
  int64_t t77;
  int64_t t78;
  uint8_t t79;
  uint8_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint8_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  gg_type_17 t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  gg_type_17 t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  gg_type_17 t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  gg_type_17 t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  gg_type_17 t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  gg_type_10 v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  {
  t0 = ((int64_t)(((*(v0))).f1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(((*(v0))).f1));
  t5 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((t4) <= (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(((*(v0))).f3));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) > (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(((*(v0))).f2));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) >= (t13));
  }
  t14 = t11;
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(((*(v0))).f2));
  t17 = ((int64_t)(((*(v0))).f3));
  t15 = ((t16) < (t17));
  }
  if (!(t15)) gg_fail("assertion failed");
  t18 = ((int64_t)(((*(v0))).f28));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t18) > (t19));
  t21 = t20;
  if (t21) {
  t22 = ((int64_t)(((*(v0))).f28));
  t23 = ((int64_t)(((*(v0))).f28));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) - (uint64_t)(t24)))));
  t26 = ((int64_t)(((t22) & (t25))));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t26) == (t27));
  }
  if (!(t21)) gg_fail("assertion failed");
  t28 = ((int64_t)(((*(v0))).f28));
  t29 = ((int64_t)(((int64_t)UINT64_C(64))));
  t30 = ((t28) > (t29));
  if (t30) {
  {
  v1 = ((int64_t)(((*(v0))).f28));
  }
  } else {
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(64))));
  }
  }
  t31 = ((int64_t)(((*(v0))).f29));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) >= (t32));
  t34 = t33;
  if (t34) {
  t35 = ((int64_t)(((*(v0))).f29));
  t36 = ((int64_t)(v1));
  t34 = ((t35) < (t36));
  }
  t37 = t34;
  t38 = t37;
  if (t38) {
  t39 = ((int64_t)(((*(v0))).f30));
  t40 = ((int64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t39) >= (t40));
  }
  if (!(t38)) gg_fail("assertion failed");
  t41 = ((int64_t)(((*(v0))).f4));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t41) >= (t42));
  t44 = t43;
  if (t44) {
  t45 = ((int64_t)(((*(v0))).f5));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = ((t45) >= (t46));
  }
  t47 = t44;
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(((*(v0))).f6));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((t49) >= (t50));
  }
  t51 = t48;
  t52 = t51;
  if (t52) {
  t53 = ((int64_t)(((*(v0))).f7));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((t53) >= (t54));
  }
  if (!(t52)) gg_fail("assertion failed");
  t55 = ((int64_t)(((*(v0))).f11));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((t55) > (t56));
  t58 = t57;
  if (t58) {
  t59 = ((int64_t)(((*(v0))).f12));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((t59) > (t60));
  }
  t61 = t58;
  t62 = t61;
  if (t62) {
  t63 = ((int64_t)(((*(v0))).f13));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((t63) >= (t64));
  }
  t65 = t62;
  t66 = t65;
  if (t66) {
  t67 = ((int64_t)(((*(v0))).f14));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((t67) >= (t68));
  }
  t69 = t66;
  t70 = t69;
  if (t70) {
  t71 = ((int64_t)(((*(v0))).f19));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t70 = ((t71) > (t72));
  }
  if (!(t70)) gg_fail("assertion failed");
  t73 = ((int64_t)(((*(v0))).f9));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((t73) > (t74));
  t76 = t75;
  if (t76) {
  t77 = ((int64_t)(((*(v0))).f10));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = ((t77) > (t78));
  }
  t79 = t76;
  t80 = t79;
  if (t80) {
  t81 = ((int64_t)(((*(v0))).f11));
  t82 = ((int64_t)(((*(v0))).f9));
  t83 = ((int64_t)(((int64_t)gg_mod(t81, t82))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = ((t83) == (t84));
  }
  t85 = t80;
  t86 = t85;
  if (t86) {
  t87 = ((int64_t)(((*(v0))).f20));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = ((t87) >= (t88));
  }
  if (!(t86)) gg_fail("assertion failed");
  t89 = ((int64_t)(((*(v0))).f6));
  t90 = ((int64_t)(((*(v0))).f7));
  v2 = gg_function_3(t89, t90, base + 0);
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = ((int64_t)(((*(v0))).f8));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((t91) != (t92));
  if (t93) {
  {
  t94 = v0;
  t95 = ((int64_t)(v3));
  t96 = ((int64_t)(((int64_t)UINT64_C(8))));
  t97 = gg_function_4(t94, t95, t96, base + 0);
  t98 = ((int64_t)(((*(v0))).f20));
  t99 = ((int64_t)(((*(v0))).f12));
  t100 = gg_function_3(t98, t99, base + 0);
  t101 = ((int64_t)(((*(v0))).f13));
  t102 = gg_function_3(t100, t101, base + 0);
  t103 = ((int64_t)(((*(v0))).f14));
  t104 = gg_function_3(t102, t103, base + 0);
  v3 = gg_function_5(t97, t104, base + 0);
  }
  }
  t105 = v0;
  t106 = ((int64_t)(v3));
  t107 = ((int64_t)(((int64_t)UINT64_C(8))));
  t108 = gg_function_4(t105, t106, t107, base + 0);
  ((*(v0))).f32 = ((int64_t)(t108));
  t109 = ((int64_t)(((*(v0))).f32));
  t110 = ((int64_t)(((*(v0))).f4));
  t111 = ((int64_t)(((int64_t)UINT64_C(8))));
  t112 = gg_function_3(t110, t111, base + 0);
  v3 = gg_function_5(t109, t112, base + 0);
  t113 = v0;
  t114 = ((int64_t)(v3));
  t115 = ((int64_t)(((int64_t)UINT64_C(8))));
  t116 = gg_function_4(t113, t114, t115, base + 0);
  ((*(v0))).f33 = ((int64_t)(t116));
  t117 = ((int64_t)(((*(v0))).f33));
  t118 = ((int64_t)(((*(v0))).f4));
  t119 = ((int64_t)(v2));
  t120 = gg_function_3(t118, t119, base + 0);
  t121 = ((int64_t)(((int64_t)UINT64_C(8))));
  t122 = gg_function_3(t120, t121, base + 0);
  v3 = gg_function_5(t117, t122, base + 0);
  t123 = v0;
  t124 = ((int64_t)(v3));
  t125 = ((int64_t)(((*(v0))).f28));
  t126 = gg_function_4(t123, t124, t125, base + 0);
  ((*(v0))).f34 = ((int64_t)(t126));
  t127 = ((int64_t)(((*(v0))).f34));
  t128 = ((int64_t)(((*(v0))).f28));
  t129 = ((int64_t)(((*(v0))).f4));
  t130 = gg_function_3(t128, t129, base + 0);
  v3 = gg_function_5(t127, t130, base + 0);
  ((*(v0))).f35 = ((int64_t)(v3));
  t131 = ((int64_t)(((*(v0))).f1));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t133 = ((t131) == (t132));
  if (t133) {
  {
  t134 = v0;
  t135 = ((int64_t)(v3));
  t136 = ((int64_t)(((int64_t)UINT64_C(64))));
  t137 = gg_function_4(t134, t135, t136, base + 0);
  ((*(v0))).f35 = ((int64_t)(t137));
  t138 = ((int64_t)(((*(v0))).f35));
  t139 = ((int64_t)(((*(v0))).f27));
  v3 = gg_function_5(t138, t139, base + 0);
  }
  }
  ((*(v0))).f36 = ((int64_t)(v3));
  t140 = ((int64_t)(v3));
  t141 = ((int64_t)(((*(v0))).f30));
  if (!(((t140) <= (t141)))) gg_fail("assertion failed");
  }
gg_done:;
}
static void gg_function_1(gg_type_10 p0, int64_t p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  gg_type_10 t3;
  int64_t t4;
  int64_t t5;
  gg_type_17 t6;
  gg_slice t7;
  gg_slice t8;
  gg_slice t9;
  gg_slice t10;
  gg_slice t11;
  gg_slice t12;
  gg_type_10 v0;
  int64_t v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  gg_slice v5;
  gg_slice v6;
  gg_slice v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  t3 = v0;
  gg_function_2(t3, base + 0);
  }
  } else {
  {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  if (!(((t4) == (t5)))) gg_fail("assertion failed");
  t6 = v0;
  t7 = v2;
  t8 = v3;
  t9 = v4;
  t10 = v5;
  t11 = v6;
  t12 = v7;
  gg_function_6(t6, t7, t8, t9, t10, t11, t12, base + 0);
  }
  }
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
  gg_type_4 t46;
  int32_t t47;
  int32_t t48;
  gg_type_4 t49;
  int64_t t50;
  int32_t t51;
  int32_t t52;
  int64_t t53;
  uint8_t t54;
  uint8_t t55;
  int64_t t56;
  gg_type_10 t57;
  int64_t t58;
  gg_slice t59;
  gg_slice t60;
  gg_slice t61;
  gg_slice t62;
  gg_slice t63;
  gg_slice t64;
  gg_header t65;
  int64_t t66;
  gg_type_1 v0;
  gg_type_3 v1;
  gg_type_6 v2;
  gg_type_7 v3;
  gg_type_9 v4;
  {
  memset(&(v0), 0, sizeof(v0));
  memset(&t0, 0, sizeof(t0));
  (v0).f0 = ((int64_t)(t0));
  memset(&t1, 0, sizeof(t1));
  (v0).f1 = ((int64_t)(t1));
  memset(&t2, 0, sizeof(t2));
  (v0).f2 = ((int64_t)(t2));
  memset(&t3, 0, sizeof(t3));
  (v0).f3 = ((int64_t)(t3));
  memset(&t4, 0, sizeof(t4));
  (v0).f4 = ((int64_t)(t4));
  memset(&t5, 0, sizeof(t5));
  (v0).f5 = ((int64_t)(t5));
  memset(&t6, 0, sizeof(t6));
  (v0).f6 = ((int64_t)(t6));
  memset(&t7, 0, sizeof(t7));
  (v0).f7 = ((int64_t)(t7));
  memset(&t8, 0, sizeof(t8));
  (v0).f8 = ((int64_t)(t8));
  memset(&t9, 0, sizeof(t9));
  (v0).f9 = ((int64_t)(t9));
  memset(&t10, 0, sizeof(t10));
  (v0).f10 = ((int64_t)(t10));
  memset(&t11, 0, sizeof(t11));
  (v0).f11 = ((int64_t)(t11));
  memset(&t12, 0, sizeof(t12));
  (v0).f12 = ((int64_t)(t12));
  memset(&t13, 0, sizeof(t13));
  (v0).f13 = ((int64_t)(t13));
  memset(&t14, 0, sizeof(t14));
  (v0).f14 = ((int64_t)(t14));
  memset(&t15, 0, sizeof(t15));
  (v0).f15 = ((int64_t)(t15));
  memset(&t16, 0, sizeof(t16));
  (v0).f16 = ((int64_t)(t16));
  memset(&t17, 0, sizeof(t17));
  (v0).f17 = ((int64_t)(t17));
  memset(&t18, 0, sizeof(t18));
  (v0).f18 = ((int64_t)(t18));
  memset(&t19, 0, sizeof(t19));
  (v0).f19 = ((int64_t)(t19));
  memset(&t20, 0, sizeof(t20));
  (v0).f20 = ((int64_t)(t20));
  memset(&t21, 0, sizeof(t21));
  (v0).f21 = ((int64_t)(t21));
  memset(&t22, 0, sizeof(t22));
  (v0).f22 = ((int64_t)(t22));
  memset(&t23, 0, sizeof(t23));
  (v0).f23 = ((int64_t)(t23));
  memset(&t24, 0, sizeof(t24));
  (v0).f24 = ((int64_t)(t24));
  memset(&t25, 0, sizeof(t25));
  (v0).f25 = ((int64_t)(t25));
  memset(&t26, 0, sizeof(t26));
  (v0).f26 = ((int64_t)(t26));
  memset(&t27, 0, sizeof(t27));
  (v0).f27 = ((int64_t)(t27));
  memset(&t28, 0, sizeof(t28));
  (v0).f28 = ((int64_t)(t28));
  memset(&t29, 0, sizeof(t29));
  (v0).f29 = ((int64_t)(t29));
  memset(&t30, 0, sizeof(t30));
  (v0).f30 = ((int64_t)(t30));
  memset(&t31, 0, sizeof(t31));
  (v0).f31 = ((int64_t)(t31));
  memset(&t32, 0, sizeof(t32));
  (v0).f32 = ((int64_t)(t32));
  memset(&t33, 0, sizeof(t33));
  (v0).f33 = ((int64_t)(t33));
  memset(&t34, 0, sizeof(t34));
  (v0).f34 = ((int64_t)(t34));
  memset(&t35, 0, sizeof(t35));
  (v0).f35 = ((int64_t)(t35));
  memset(&t36, 0, sizeof(t36));
  (v0).f36 = ((int64_t)(t36));
  memset(&t37, 0, sizeof(t37));
  (v0).f37 = ((int64_t)(t37));
  memset(&t38, 0, sizeof(t38));
  (v0).f38 = ((int64_t)(t38));
  memset(&t39, 0, sizeof(t39));
  (v0).f39 = ((int64_t)(t39));
  memset(&t40, 0, sizeof(t40));
  (v0).f40 = ((int64_t)(t40));
  memset(&t41, 0, sizeof(t41));
  (v0).f41 = ((int64_t)(t41));
  memset(&t42, 0, sizeof(t42));
  (v0).f42 = ((int64_t)(t42));
  (v0).f3 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v0).f28 = ((int64_t)(((int64_t)UINT64_C(64))));
  (v0).f9 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v0).f10 = ((int64_t)(((int64_t)UINT64_C(4))));
  (v0).f11 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v0).f12 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v0).f19 = ((int64_t)(((int64_t)UINT64_C(1))));
  (v0).f30 = ((int64_t)(((int64_t)UINT64_C(128))));
  memset(&t43, 0, sizeof(t43));
  t44 = ((int64_t)(t43));
  for (t45=0; t45<1; t45++) (v1).data[t45] = t44;
  memset(&(t46), 0, sizeof(t46));
  memset(&t47, 0, sizeof(t47));
  (t46).f43 = ((int32_t)(t47));
  memset(&t48, 0, sizeof(t48));
  (t46).f44 = ((int32_t)(t48));
  t49 = t46;
  for (t50=0; t50<1; t50++) (v2).data[t50] = t49;
  memset(&t51, 0, sizeof(t51));
  t52 = ((int32_t)(t51));
  for (t53=0; t53<1; t53++) (v3).data[t53] = t52;
  memset(&t54, 0, sizeof(t54));
  t55 = ((uint8_t)(t54));
  for (t56=0; t56<1; t56++) (v4).data[t56] = t55;
  t57 = &(v0);
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = gg_view((v1).data, 1);
  t60 = gg_view((v2).data, 1);
  t61 = gg_view((v3).data, 1);
  t62 = gg_view((v3).data, 1);
  t63 = gg_view((v3).data, 1);
  t64 = gg_view((v4).data, 1);
  gg_function_1(t57, t58, t59, t60, t61, t62, t63, t64, base + 0);
  t65.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t65.len = 0;
  gg_integer(gg_ref(&t65, gg_get_stack(base + 0)), (v0).f36);
  gg_append(gg_ref(&t65, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t65).base, (t65).len));
  }
gg_done:;
  for (t66 = 0; t66 < 1; t66++) gg_stacks[base + t66].top = 0;
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
