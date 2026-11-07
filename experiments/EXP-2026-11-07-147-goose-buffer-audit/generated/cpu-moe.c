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
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { int64_t data[1]; } gg_type_3;
#pragma pack(pop)
typedef struct gg_type_4 gg_type_4;
#pragma pack(push, 1)
struct gg_type_4 {
  int32_t f31;
  int32_t f32;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_4 data[1]; } gg_type_6;
#pragma pack(pop)
typedef gg_type_1 *gg_type_7;
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
static uint8_t gg_eq_8(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_slice value);
static uint8_t gg_eq_9(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, gg_slice value);
static uint8_t gg_eq_10(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, uint8_t value);
static uint8_t gg_eq_12(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_12(gg_slice a, gg_slice b);
static void gg_render_12(gg_reference out, uint8_t value);
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
if(!gg_eq_5(a.f31,b.f31))return 0;
if(!gg_eq_5(a.f32,b.f32))return 0;
return 1;
}
static void gg_render_4(gg_reference out, gg_type_4 value) {
gg_append(out,"\122\157\167\040\173\040",6,1);
gg_render_5(out, value.f31);
gg_append(out,", ",2,1);
gg_render_5(out, value.f32);
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
return a == b;
}
static void gg_render_7(gg_reference out, gg_type_7 value) {
gg_render_1(out,*value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_slice a, gg_slice b) {
return gg_equal_elements_2(a,b);
}
static void gg_render_8(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_2(out, ((int64_t*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_slice a, gg_slice b) {
return gg_equal_elements_4(a,b);
}
static void gg_render_9(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_4(out, ((gg_type_4*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_10(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_12(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_12(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_12(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_12(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(gg_type_7 p0, gg_slice p1, gg_slice p2, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t base) {
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
static void gg_function_1(gg_type_7 p0, gg_slice p1, gg_slice p2, int64_t base) {
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
  int64_t t14;
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
  uint8_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t * t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int32_t t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
  uint8_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t * t117;
  int64_t t118;
  int64_t t119;
  gg_type_4 * t120;
  int64_t t121;
  int64_t * t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int32_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t * t136;
  int64_t t137;
  int64_t t138;
  uint8_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  uint8_t t143;
  uint8_t t144;
  uint8_t t145;
  int64_t t146;
  int64_t t147;
  uint8_t t148;
  uint8_t t149;
  int64_t t150;
  int64_t t151;
  uint8_t t152;
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
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  uint8_t t171;
  uint8_t t172;
  int64_t t173;
  int64_t t174;
  uint8_t t175;
  int64_t t176;
  int64_t t177;
  uint8_t t178;
  int64_t t179;
  int64_t t180;
  uint8_t t181;
  int64_t t182;
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
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  gg_slice t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  gg_type_4 * t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  uint8_t t238;
  uint8_t t239;
  int64_t t240;
  int64_t t241;
  uint8_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
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
  uint8_t t288;
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
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  uint8_t t321;
  int64_t t322;
  int64_t t323;
  int32_t t324;
  int64_t t325;
  int64_t t326;
  gg_type_7 v0;
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
  gg_type_4 v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int64_t v40;
  int64_t v41;
  int64_t v42;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int64_t)(((*(v0))).f2));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(((*(v0))).f1));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) >= (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(((*(v0))).f1));
  t9 = ((int64_t)(((*(v0))).f2));
  t7 = ((t8) < (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)((v1).len));
  t13 = ((int64_t)(((*(v0))).f3));
  t11 = ((t12) >= (t13));
  }
  if (!(t11)) gg_fail("assertion failed");
  t14 = ((int64_t)((v2).len));
  t15 = ((int64_t)(((*(v0))).f3));
  t16 = ((int64_t)(((*(v0))).f4));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(((*(v0))).f5));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  if (!(((t14) >= (t19)))) gg_fail("assertion failed");
  t20 = ((int64_t)(((*(v0))).f0));
  fg_moe_begin(t20);
  t21 = ((int64_t)(((*(v0))).f6));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) != (t22));
  if (t23) {
  {
  v3 = ((int64_t)(((*(v0))).f23));
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(((*(v0))).f12));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(((*(v0))).f13));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t28 = ((int64_t)(((*(v0))).f1));
  t29 = ((int64_t)(((*(v0))).f11));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((*(v0))).f7));
  t32 = ((int64_t)(((int64_t)gg_div(t30, t31))));
  t33 = ((int64_t)(((*(v0))).f2));
  v6 = ((int64_t)(((int64_t)gg_div(t32, t33))));
  t34 = ((int64_t)(((*(v0))).f1));
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  t37 = ((int64_t)(((*(v0))).f11));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int64_t)(((*(v0))).f7));
  t40 = ((int64_t)(((int64_t)gg_div(t38, t39))));
  t41 = ((int64_t)(((*(v0))).f2));
  v7 = ((int64_t)(((int64_t)gg_div(t40, t41))));
  t43 = ((*(v0))).f14;
  t42 = 0;
label0:;
  if (t42 >= t43) goto label2;
  v8 = t42;
  {
  t45 = ((*(v0))).f13;
  t44 = 0;
label3:;
  if (t44 >= t45) goto label5;
  v9 = t44;
  {
  t47 = ((*(v0))).f12;
  t46 = 0;
label6:;
  if (t46 >= t47) goto label8;
  v10 = t46;
  {
  t48 = ((int64_t)(((*(v0))).f26));
  t49 = ((int64_t)(v8));
  t50 = ((int64_t)(((*(v0))).f20));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t51)))));
  t53 = ((int64_t)(v9));
  t54 = ((int64_t)(((*(v0))).f19));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t57 = ((int64_t)(v10));
  t58 = ((int64_t)(((*(v0))).f18));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t59)))));
  t61 = ((int64_t)(v6));
  t62 = ((int64_t)(((*(v0))).f7));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) * (uint64_t)(t62)))));
  t64 = ((int64_t)(((*(v0))).f17));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t65)))));
  t66 = ((int64_t)(((*(v0))).f27));
  t67 = ((int64_t)(v8));
  t68 = ((int64_t)(v5));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t69)))));
  t71 = ((int64_t)(v9));
  t72 = ((int64_t)(v4));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) * (uint64_t)(t72)))));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t73)))));
  t75 = ((int64_t)(v10));
  t76 = ((int64_t)(v3));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t75) * (uint64_t)(t76)))));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t74) + (uint64_t)(t77)))));
  t79 = ((int64_t)(v6));
  t80 = ((int64_t)(((*(v0))).f8));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t81)))));
  t82 = ((int64_t)(((*(v0))).f0));
  t83 = ((int64_t)(v11));
  t84 = ((int64_t)(v12));
  t85 = ((int64_t)(v7));
  t86 = ((int64_t)(v6));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t85) - (uint64_t)(t86)))));
  t88 = ((int64_t)(((*(v0))).f7));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t87) * (uint64_t)(t88)))));
  fg_moe_quant(t82, t83, t84, t89);
  }
label7:;
  t46++; goto label6;
label8:;
  }
label4:;
  t44++; goto label3;
label5:;
  }
label1:;
  t42++; goto label0;
label2:;
  }
  }
  t90 = ((int64_t)(((*(v0))).f1));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((t90) == (t91));
  if (t92) {
  {
  t94 = ((*(v0))).f3;
  t93 = 0;
label9:;
  if (t93 >= t94) goto label11;
  v13 = t93;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t95 = v13;
  t96 = ((int64_t *)(v1).data) + gg_index(t95, (v1).len);
  (*t96) = ((int64_t)(((int64_t)UINT64_C(0))));
  }
label10:;
  t93++; goto label9;
label11:;
  t98 = ((*(v0))).f5;
  t97 = 0;
label12:;
  if (t97 >= t98) goto label14;
  v14 = t97;
  {
  t100 = ((*(v0))).f4;
  t99 = 0;
label15:;
  if (t99 >= t100) goto label17;
  v15 = t99;
  {
  t101 = ((int64_t)(((*(v0))).f0));
  t102 = ((int64_t)(v14));
  t103 = ((int64_t)(v15));
  t104 = fg_moe_read_id(t101, t102, t103);
  v16 = ((int64_t)(((int64_t)(t104))));
  t105 = ((int64_t)(v16));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = ((t105) >= (t106));
  t108 = t107;
  if (t108) {
  t109 = ((int64_t)(v16));
  t110 = ((int64_t)(((*(v0))).f3));
  t108 = ((t109) < (t110));
  }
  if (!(t108)) gg_fail("assertion failed");
  t111 = ((int64_t)(v16));
  t112 = ((int64_t)(((*(v0))).f4));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t111) * (uint64_t)(t112)))));
  t114 = ((int64_t)(((*(v0))).f5));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t113) * (uint64_t)(t114)))));
  /* FG_DIRECT_SLICE_INDEX */
  t116 = v16;
  t117 = ((int64_t *)(v1).data) + gg_index(t116, (v1).len);
  t118 = ((int64_t)((*t117)));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t118)))));
  /* FG_DIRECT_SLICE_INDEX */
  t119 = v17;
  t120 = ((gg_type_4 *)(v2).data) + gg_index(t119, (v2).len);
  memset(&((*t120)), 0, sizeof((*t120)));
  ((*t120)).f31 = ((int32_t)(((int32_t)(v15))));
  ((*t120)).f32 = ((int32_t)(((int32_t)(v14))));
  /* FG_DIRECT_SLICE_INDEX */
  t121 = v16;
  t122 = ((int64_t *)(v1).data) + gg_index(t121, (v1).len);
  t123 = (*t122);
  t124 = ((int64_t)(((int64_t)UINT64_C(1))));
  (*t122) = ((int64_t)((uint64_t)(t123) + (uint64_t)(t124)));
  }
label16:;
  t99++; goto label15;
label17:;
  }
label13:;
  t97++; goto label12;
label14:;
  }
  }
  v18 = ((int64_t)(((*(v0))).f1));
label19:;
  t125 = ((int64_t)(v18));
  t126 = ((int64_t)(((*(v0))).f3));
  if (!(((t125) < (t126)))) goto label20;
  {
  t127 = ((int64_t)(((*(v0))).f0));
  t128 = ((int64_t)(v18));
  t129 = ((int32_t)(((int32_t)(((*(v0))).f2))));
  fg_moe_reset(t127, t128, t129);
  t130 = v18;
  t131 = ((int64_t)(((*(v0))).f2));
  v18 = ((int64_t)((uint64_t)(t130) + (uint64_t)(t131)));
  }
  goto label19;
label20:;
  t132 = ((int64_t)(((*(v0))).f0));
  fg_moe_barrier(t132);
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
label22:;
  t133 = ((int64_t)(v18));
  t134 = ((int64_t)(((*(v0))).f3));
  if (!(((t133) < (t134)))) goto label23;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t135 = v18;
  t136 = ((int64_t *)(v1).data) + gg_index(t135, (v1).len);
  v19 = ((int64_t)((*t136)));
  t137 = ((int64_t)(v19));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((t137) > (t138));
  if (t139) {
  {
  t140 = ((int64_t)(((*(v0))).f0));
  t141 = ((int64_t)(v18));
  t142 = ((int64_t)(v19));
  t143 = fg_moe_try_tiled(t140, t141, t142);
  t144 = t143;
  t145 = (!(t144));
  if (t145) {
  {
  v20 = ((int64_t)(((*(v0))).f10));
  v21 = ((int64_t)(((int64_t)UINT64_C(16))));
  t146 = ((int64_t)(v20));
  t147 = ((int64_t)(((int64_t)UINT64_C(1))));
  t148 = ((t146) == (t147));
  t149 = t148;
  if (!t149) {
  t150 = ((int64_t)(v19));
  t151 = ((int64_t)(((int64_t)UINT64_C(1))));
  t149 = ((t150) == (t151));
  }
  t152 = t149;
  if (t152) {
  {
  v21 = ((int64_t)(((int64_t)UINT64_C(64))));
  }
  }
  t153 = ((int64_t)(v20));
  t154 = ((int64_t)(v21));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t154)))));
  t156 = ((int64_t)(((int64_t)UINT64_C(1))));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) - (uint64_t)(t156)))));
  t158 = ((int64_t)(v21));
  v22 = ((int64_t)(((int64_t)gg_div(t157, t158))));
  t159 = ((int64_t)(v19));
  t160 = ((int64_t)(v21));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t159) + (uint64_t)(t160)))));
  t162 = ((int64_t)(((int64_t)UINT64_C(1))));
  t163 = ((int64_t)(((int64_t)((uint64_t)(t161) - (uint64_t)(t162)))));
  t164 = ((int64_t)(v21));
  v23 = ((int64_t)(((int64_t)gg_div(t163, t164))));
  t165 = ((int64_t)(v22));
  t166 = ((int64_t)(v23));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t165) * (uint64_t)(t166)))));
  t168 = ((int64_t)(((*(v0))).f2));
  t169 = ((int64_t)(((int64_t)UINT64_C(4))));
  t170 = ((int64_t)(((int64_t)((uint64_t)(t168) * (uint64_t)(t169)))));
  t171 = ((t167) < (t170));
  t172 = t171;
  if (!t172) {
  t173 = ((int64_t)(((*(v0))).f29));
  t174 = ((int64_t)(((int64_t)UINT64_C(0))));
  t172 = ((t173) != (t174));
  }
  t175 = t172;
  if (t175) {
  {
  t176 = ((int64_t)(v20));
  t177 = ((int64_t)(v19));
  t178 = ((t176) > (t177));
  if (t178) {
  {
  v22 = ((int64_t)(((*(v0))).f2));
  }
  } else {
  {
  v22 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t179 = ((int64_t)(v20));
  t180 = ((int64_t)(v19));
  t181 = ((t179) > (t180));
  if (t181) {
  {
  v23 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v23 = ((int64_t)(((*(v0))).f2));
  }
  }
  }
  }
  t182 = ((int64_t)(v20));
  t183 = ((int64_t)(v22));
  t184 = ((int64_t)(((int64_t)((uint64_t)(t182) + (uint64_t)(t183)))));
  t185 = ((int64_t)(((int64_t)UINT64_C(1))));
  t186 = ((int64_t)(((int64_t)((uint64_t)(t184) - (uint64_t)(t185)))));
  t187 = ((int64_t)(v22));
  v24 = ((int64_t)(((int64_t)gg_div(t186, t187))));
  t188 = ((int64_t)(v19));
  t189 = ((int64_t)(v23));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t188) + (uint64_t)(t189)))));
  t191 = ((int64_t)(((int64_t)UINT64_C(1))));
  t192 = ((int64_t)(((int64_t)((uint64_t)(t190) - (uint64_t)(t191)))));
  t193 = ((int64_t)(v23));
  v25 = ((int64_t)(((int64_t)gg_div(t192, t193))));
  v26 = ((int64_t)(((*(v0))).f1));
label25:;
  t194 = ((int64_t)(v26));
  t195 = ((int64_t)(v22));
  t196 = ((int64_t)(v23));
  t197 = ((int64_t)(((int64_t)((uint64_t)(t195) * (uint64_t)(t196)))));
  if (!(((t194) < (t197)))) goto label26;
  {
  t198 = ((int64_t)(v24));
  t199 = ((int64_t)(v26));
  t200 = ((int64_t)(v22));
  t201 = ((int64_t)(((int64_t)gg_mod(t199, t200))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t198) * (uint64_t)(t201)))));
  t202 = ((int64_t)(v25));
  t203 = ((int64_t)(v26));
  t204 = ((int64_t)(v22));
  t205 = ((int64_t)(((int64_t)gg_div(t203, t204))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t202) * (uint64_t)(t205)))));
  t206 = ((int64_t)(v27));
  t207 = ((int64_t)(v24));
  t208 = ((int64_t)(((int64_t)((uint64_t)(t206) + (uint64_t)(t207)))));
  t209 = ((int64_t)(v20));
  v29 = gg_function_8(t208, t209, base + 0);
  t210 = ((int64_t)(v28));
  t211 = ((int64_t)(v25));
  t212 = ((int64_t)(((int64_t)((uint64_t)(t210) + (uint64_t)(t211)))));
  t213 = ((int64_t)(v19));
  v30 = gg_function_8(t212, t213, base + 0);
  v31 = ((int64_t)(v28));
label28:;
  t214 = ((int64_t)(v31));
  t215 = ((int64_t)(v30));
  if (!(((t214) < (t215)))) goto label29;
  {
  v32 = ((int64_t)(v27));
label31:;
  t216 = ((int64_t)(v32));
  t217 = ((int64_t)(v29));
  if (!(((t216) < (t217)))) goto label32;
  {
  v33 = ((int64_t)(v31));
label34:;
  t218 = ((int64_t)(v33));
  t219 = ((int64_t)(v31));
  t220 = ((int64_t)(((int64_t)UINT64_C(16))));
  t221 = ((int64_t)(((int64_t)((uint64_t)(t219) + (uint64_t)(t220)))));
  t222 = ((int64_t)(v30));
  t223 = gg_function_8(t221, t222, base + 0);
  t224 = ((int64_t)(t223));
  if (!(((t218) < (t224)))) goto label35;
  {
  t225 = v2;
  t226 = ((int64_t)(v18));
  t227 = ((int64_t)(((*(v0))).f4));
  t228 = ((int64_t)(((int64_t)((uint64_t)(t226) * (uint64_t)(t227)))));
  t229 = ((int64_t)(((*(v0))).f5));
  t230 = ((int64_t)(((int64_t)((uint64_t)(t228) * (uint64_t)(t229)))));
  t231 = ((int64_t)(v33));
  t232 = ((int64_t)((uint64_t)(t230) + (uint64_t)(t231)));
  t233 = ((gg_type_4 *)t225.data) + gg_index(t232, t225.len);
  v34 = (*t233);
  v35 = ((int64_t)(((int64_t)((v34).f31))));
  v36 = ((int64_t)(((int64_t)((v34).f32))));
  t234 = ((int64_t)(v35));
  t235 = ((int64_t)(((*(v0))).f12));
  v37 = ((int64_t)(((int64_t)gg_mod(t234, t235))));
  t236 = ((int64_t)(((*(v0))).f24));
  t237 = ((int64_t)(((int64_t)UINT64_C(0))));
  t238 = ((t236) != (t237));
  t239 = t238;
  if (!t239) {
  t240 = ((int64_t)(((*(v0))).f6));
  t241 = ((int64_t)(((int64_t)UINT64_C(0))));
  t239 = ((t240) != (t241));
  }
  t242 = t239;
  if (t242) {
  {
  t243 = ((int64_t)(((*(v0))).f27));
  t244 = ((int64_t)(v37));
  t245 = ((int64_t)(v36));
  t246 = ((int64_t)(((*(v0))).f12));
  t247 = ((int64_t)(((int64_t)((uint64_t)(t245) * (uint64_t)(t246)))));
  t248 = ((int64_t)(((int64_t)((uint64_t)(t244) + (uint64_t)(t247)))));
  t249 = ((int64_t)(((*(v0))).f23));
  t250 = ((int64_t)(((int64_t)((uint64_t)(t248) * (uint64_t)(t249)))));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t243) + (uint64_t)(t250)))));
  }
  } else {
  {
  t251 = ((int64_t)(((*(v0))).f27));
  t252 = ((int64_t)(v37));
  t253 = ((int64_t)(((*(v0))).f18));
  t254 = ((int64_t)(((int64_t)((uint64_t)(t252) * (uint64_t)(t253)))));
  t255 = ((int64_t)(((int64_t)((uint64_t)(t251) + (uint64_t)(t254)))));
  t256 = ((int64_t)(v36));
  t257 = ((int64_t)(((*(v0))).f19));
  t258 = ((int64_t)(((int64_t)((uint64_t)(t256) * (uint64_t)(t257)))));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t255) + (uint64_t)(t258)))));
  }
  }
  t259 = ((int64_t)(((*(v0))).f28));
  t260 = ((int64_t)(v35));
  t261 = ((int64_t)(((*(v0))).f21));
  t262 = ((int64_t)(((int64_t)((uint64_t)(t260) * (uint64_t)(t261)))));
  t263 = ((int64_t)(((int64_t)((uint64_t)(t259) + (uint64_t)(t262)))));
  t264 = ((int64_t)(v36));
  t265 = ((int64_t)(((*(v0))).f22));
  t266 = ((int64_t)(((int64_t)((uint64_t)(t264) * (uint64_t)(t265)))));
  t267 = ((int64_t)(((int64_t)((uint64_t)(t263) + (uint64_t)(t266)))));
  t268 = ((int64_t)(v32));
  t269 = ((int64_t)(((int64_t)UINT64_C(4))));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t268) * (uint64_t)(t269)))));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t267) + (uint64_t)(t270)))));
  t271 = ((int64_t)(v32));
  t272 = ((int64_t)(((int64_t)UINT64_C(16))));
  t273 = ((int64_t)(((int64_t)((uint64_t)(t271) + (uint64_t)(t272)))));
  t274 = ((int64_t)(v29));
  t275 = gg_function_8(t273, t274, base + 0);
  t276 = ((int64_t)(t275));
  t277 = ((int64_t)(v32));
  v40 = ((int64_t)(((int64_t)((uint64_t)(t276) - (uint64_t)(t277)))));
  t278 = ((int64_t)(((*(v0))).f25));
  t279 = ((int64_t)(v18));
  t280 = ((int64_t)(((*(v0))).f16));
  t281 = ((int64_t)(((int64_t)((uint64_t)(t279) * (uint64_t)(t280)))));
  t282 = ((int64_t)(((int64_t)((uint64_t)(t278) + (uint64_t)(t281)))));
  t283 = ((int64_t)(v32));
  t284 = ((int64_t)(((*(v0))).f15));
  t285 = ((int64_t)(((int64_t)((uint64_t)(t283) * (uint64_t)(t284)))));
  v41 = ((int64_t)(((int64_t)((uint64_t)(t282) + (uint64_t)(t285)))));
  t286 = ((int64_t)(((*(v0))).f15));
  t287 = ((int64_t)(((*(v0))).f30));
  t288 = ((t286) == (t287));
  if (t288) {
  {
  t289 = ((int64_t)(((*(v0))).f0));
  t290 = ((int64_t)(v41));
  t291 = ((int64_t)(v38));
  t292 = ((int64_t)(v39));
  t293 = ((int64_t)(((*(v0))).f9));
  t294 = ((int64_t)(v40));
  fg_moe_matrix(t289, t290, t291, t292, t293, t294);
  }
  } else {
  {
  t296 = v40;
  t295 = 0;
label36:;
  if (t295 >= t296) goto label38;
  v42 = t295;
  {
  t297 = ((int64_t)(((*(v0))).f0));
  t298 = ((int64_t)(v41));
  t299 = ((int64_t)(v42));
  t300 = ((int64_t)(((*(v0))).f15));
  t301 = ((int64_t)(((int64_t)((uint64_t)(t299) * (uint64_t)(t300)))));
  t302 = ((int64_t)(((int64_t)((uint64_t)(t298) + (uint64_t)(t301)))));
  t303 = ((int64_t)(v38));
  t304 = ((int64_t)(v39));
  t305 = ((int64_t)(v42));
  t306 = ((int64_t)(((int64_t)UINT64_C(4))));
  t307 = ((int64_t)(((int64_t)((uint64_t)(t305) * (uint64_t)(t306)))));
  t308 = ((int64_t)(((int64_t)((uint64_t)(t304) + (uint64_t)(t307)))));
  t309 = ((int64_t)(((*(v0))).f9));
  t310 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg_moe_matrix(t297, t302, t303, t308, t309, t310);
  }
label37:;
  t295++; goto label36;
label38:;
  }
  }
  t311 = v33;
  t312 = ((int64_t)(((int64_t)UINT64_C(1))));
  v33 = ((int64_t)((uint64_t)(t311) + (uint64_t)(t312)));
  }
  goto label34;
label35:;
  t313 = v32;
  t314 = ((int64_t)(((int64_t)UINT64_C(16))));
  v32 = ((int64_t)((uint64_t)(t313) + (uint64_t)(t314)));
  }
  goto label31;
label32:;
  t315 = v31;
  t316 = ((int64_t)(((int64_t)UINT64_C(16))));
  v31 = ((int64_t)((uint64_t)(t315) + (uint64_t)(t316)));
  }
  goto label28;
label29:;
  t317 = ((int64_t)(((*(v0))).f2));
  t318 = ((int64_t)(v22));
  t319 = ((int64_t)(v23));
  t320 = ((int64_t)(((int64_t)((uint64_t)(t318) * (uint64_t)(t319)))));
  t321 = ((t317) >= (t320));
  if (t321) {
  {
  goto label26;
  }
  }
  t322 = ((int64_t)(((*(v0))).f0));
  t323 = ((int64_t)(v18));
  t324 = fg_moe_claim(t322, t323);
  v26 = ((int64_t)(((int64_t)(t324))));
  }
  goto label25;
label26:;
  }
  }
  }
  }
  t325 = v18;
  t326 = ((int64_t)(((int64_t)UINT64_C(1))));
  v18 = ((int64_t)((uint64_t)(t325) + (uint64_t)(t326)));
  }
  goto label22;
label23:;
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
  gg_type_4 t34;
  int32_t t35;
  int32_t t36;
  gg_type_4 t37;
  int64_t t38;
  gg_type_7 t39;
  gg_slice t40;
  gg_slice t41;
  gg_header t42;
  int64_t t43;
  int64_t * t44;
  int64_t t45;
  gg_type_1 v0;
  gg_type_3 v1;
  gg_type_6 v2;
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
  (v0).f2 = ((int64_t)(((int64_t)UINT64_C(1))));
  memset(&t31, 0, sizeof(t31));
  t32 = ((int64_t)(t31));
  for (t33=0; t33<1; t33++) (v1).data[t33] = t32;
  memset(&(t34), 0, sizeof(t34));
  memset(&t35, 0, sizeof(t35));
  (t34).f31 = ((int32_t)(t35));
  memset(&t36, 0, sizeof(t36));
  (t34).f32 = ((int32_t)(t36));
  t37 = t34;
  for (t38=0; t38<1; t38++) (v2).data[t38] = t37;
  t39 = &(v0);
  t40 = gg_view((v1).data, 1);
  t41 = gg_view((v2).data, 1);
  gg_function_1(t39, t40, t41, base + 0);
  t42.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t42.len = 0;
  /* FG_DIRECT_SLICE_INDEX */
  t43 = ((int64_t)UINT64_C(0));
  t44 = ((int64_t *)(v1).data) + gg_index(t43, ((int64_t)UINT64_C(1)));
  gg_integer(gg_ref(&t42, gg_get_stack(base + 0)), (*t44));
  gg_append(gg_ref(&t42, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t42).base, (t42).len));
  }
gg_done:;
  for (t45 = 0; t45 < 1; t45++) gg_stacks[base + t45].top = 0;
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
