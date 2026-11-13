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
  uint64_t f0;
  uint64_t f1;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_1 data[4]; } gg_type_3;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, gg_type_1 value);
static uint8_t gg_eq_2(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint64_t value);
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, gg_type_3 value);
static uint8_t gg_eq_4(int64_t a, int64_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, int64_t value);
static uint8_t gg_eq_5(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, gg_slice value);
static uint8_t gg_eq_6(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint8_t value);
static uint8_t gg_eq_8(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((gg_type_1*)a.data)[i],((gg_type_1*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(gg_type_1 a, gg_type_1 b) {
if(!gg_eq_2(a.f0,b.f0))return 0;
if(!gg_eq_2(a.f1,b.f1))return 0;
return 1;
}
static void gg_render_1(gg_reference out, gg_type_1 value) {
gg_append(out,"\106\162\145\145\102\154\157\143\153\040\173\040",12,1);
gg_render_2(out, value.f0);
gg_append(out,", ",2,1);
gg_render_2(out, value.f1);
gg_append(out," }",2,1);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((gg_type_3*)a.data)[i],((gg_type_3*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b) {
return gg_equal_elements_1(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_3(gg_reference out, gg_type_3 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((gg_type_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_5(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((gg_type_1*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint8_t value) {
gg_bool(out,value);
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
static int64_t gg_function_1(gg_slice p0, int64_t p1, uint64_t p2, uint64_t p3, int64_t base);
static int64_t gg_function_2(gg_slice p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_2(gg_slice p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t t15;
  gg_type_1 * t16;
  gg_slice t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  gg_type_1 * t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  gg_slice v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v1));
  t4 = ((t5) < (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)((v0).len));
  t8 = ((t9) <= (t10));
  }
  if (!(t8)) gg_fail("assertion failed");
  v3 = ((int64_t)(v2));
label1:;
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) - (uint64_t)(t13)))));
  if (!(((t11) < (t14)))) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t15 = v3;
  t16 = ((gg_type_1 *)(v0).data) + gg_index(t15, (v0).len);
  t17 = v0;
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  t20 = ((int64_t)((uint64_t)(t18) + (uint64_t)(t19)));
  t21 = ((gg_type_1 *)t17.data) + gg_index(t20, t17.len);
  (*t16) = (*t21);
  t22 = v3;
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t22) + (uint64_t)(t23)));
  }
  goto label1;
label2:;
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t24) - (uint64_t)(t25)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(gg_slice p0, int64_t p1, uint64_t p2, uint64_t p3, int64_t base) {
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
  gg_type_1 * t10;
  uint64_t t11;
  int64_t t12;
  gg_type_1 * t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint8_t t17;
  int64_t t18;
  gg_type_1 * t19;
  uint64_t t20;
  uint64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  gg_type_1 * t29;
  uint64_t t30;
  int64_t t31;
  gg_type_1 * t32;
  uint64_t t33;
  uint64_t t34;
  gg_slice t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  gg_type_1 * t39;
  uint64_t t40;
  uint8_t t41;
  int64_t t42;
  gg_type_1 * t43;
  uint64_t t44;
  gg_slice t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  gg_type_1 * t49;
  uint64_t t50;
  gg_slice t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  uint64_t t57;
  uint64_t t58;
  int64_t t59;
  gg_type_1 * t60;
  uint64_t t61;
  uint8_t t62;
  int64_t t63;
  gg_type_1 * t64;
  int64_t t65;
  gg_type_1 * t66;
  uint64_t t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
  uint8_t t72;
  gg_slice t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  gg_type_1 * t77;
  uint64_t t78;
  gg_slice t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  gg_type_1 * t83;
  uint64_t t84;
  uint64_t t85;
  int64_t t86;
  gg_type_1 * t87;
  uint64_t t88;
  uint8_t t89;
  gg_slice t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  gg_type_1 * t94;
  uint64_t t95;
  int64_t t96;
  gg_type_1 * t97;
  uint64_t t98;
  gg_slice t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint8_t t106;
  uint8_t t107;
  int64_t t108;
  gg_type_1 * t109;
  uint64_t t110;
  uint64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  gg_type_1 * t117;
  gg_slice t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  gg_type_1 * t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  gg_type_1 * t126;
  int64_t t127;
  int64_t t128;
  gg_slice v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)((v0).len));
  t4 = ((t5) <= (t6));
  }
  if (!(t4)) gg_fail("assertion failed");
  t8 = v1;
  t7 = 0;
label0:;
  if (t7 >= t8) goto label2;
  v4 = t7;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t9 = v4;
  t10 = ((gg_type_1 *)(v0).data) + gg_index(t9, (v0).len);
  t11 = ((uint64_t)(((*t10)).f0));
  /* FG_DIRECT_SLICE_INDEX */
  t12 = v4;
  t13 = ((gg_type_1 *)(v0).data) + gg_index(t12, (v0).len);
  t14 = ((uint64_t)(((*t13)).f1));
  t15 = ((uint64_t)(((uint64_t)((uint64_t)(t11) + (uint64_t)(t14)))));
  t16 = ((uint64_t)(v2));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t18 = v4;
  t19 = ((gg_type_1 *)(v0).data) + gg_index(t18, (v0).len);
  t20 = ((*t19)).f1;
  t21 = ((uint64_t)(v3));
  ((*t19)).f1 = ((uint64_t)((uint64_t)(t20) + (uint64_t)(t21)));
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) - (uint64_t)(t24)))));
  t26 = ((t22) < (t25));
  t27 = t26;
  if (t27) {
  /* FG_DIRECT_SLICE_INDEX */
  t28 = v4;
  t29 = ((gg_type_1 *)(v0).data) + gg_index(t28, (v0).len);
  t30 = ((uint64_t)(((*t29)).f0));
  /* FG_DIRECT_SLICE_INDEX */
  t31 = v4;
  t32 = ((gg_type_1 *)(v0).data) + gg_index(t31, (v0).len);
  t33 = ((uint64_t)(((*t32)).f1));
  t34 = ((uint64_t)(((uint64_t)((uint64_t)(t30) + (uint64_t)(t33)))));
  t35 = v0;
  t36 = ((int64_t)(v4));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  t38 = ((int64_t)((uint64_t)(t36) + (uint64_t)(t37)));
  t39 = ((gg_type_1 *)t35.data) + gg_index(t38, t35.len);
  t40 = ((uint64_t)(((*t39)).f0));
  t27 = ((t34) == (t40));
  }
  t41 = t27;
  if (t41) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t42 = v4;
  t43 = ((gg_type_1 *)(v0).data) + gg_index(t42, (v0).len);
  t44 = ((*t43)).f1;
  t45 = v0;
  t46 = ((int64_t)(v4));
  t47 = ((int64_t)(((int64_t)UINT64_C(1))));
  t48 = ((int64_t)((uint64_t)(t46) + (uint64_t)(t47)));
  t49 = ((gg_type_1 *)t45.data) + gg_index(t48, t45.len);
  t50 = ((uint64_t)(((*t49)).f1));
  ((*t43)).f1 = ((uint64_t)((uint64_t)(t44) + (uint64_t)(t50)));
  t51 = v0;
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(v4));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t54)))));
  t0 = gg_function_2(t51, t52, t55, base + 0);
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
  }
  t56 = ((uint64_t)(v2));
  t57 = ((uint64_t)(v3));
  t58 = ((uint64_t)(((uint64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  /* FG_DIRECT_SLICE_INDEX */
  t59 = v4;
  t60 = ((gg_type_1 *)(v0).data) + gg_index(t59, (v0).len);
  t61 = ((uint64_t)(((*t60)).f0));
  t62 = ((t58) == (t61));
  if (t62) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t63 = v4;
  t64 = ((gg_type_1 *)(v0).data) + gg_index(t63, (v0).len);
  ((*t64)).f0 = ((uint64_t)(v2));
  /* FG_DIRECT_SLICE_INDEX */
  t65 = v4;
  t66 = ((gg_type_1 *)(v0).data) + gg_index(t65, (v0).len);
  t67 = ((*t66)).f1;
  t68 = ((uint64_t)(v3));
  ((*t66)).f1 = ((uint64_t)((uint64_t)(t67) + (uint64_t)(t68)));
  t69 = ((int64_t)(v4));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t71 = ((t69) > (t70));
  t72 = t71;
  if (t72) {
  t73 = v0;
  t74 = ((int64_t)(v4));
  t75 = ((int64_t)(((int64_t)UINT64_C(1))));
  t76 = ((int64_t)((uint64_t)(t74) - (uint64_t)(t75)));
  t77 = ((gg_type_1 *)t73.data) + gg_index(t76, t73.len);
  t78 = ((uint64_t)(((*t77)).f0));
  t79 = v0;
  t80 = ((int64_t)(v4));
  t81 = ((int64_t)(((int64_t)UINT64_C(1))));
  t82 = ((int64_t)((uint64_t)(t80) - (uint64_t)(t81)));
  t83 = ((gg_type_1 *)t79.data) + gg_index(t82, t79.len);
  t84 = ((uint64_t)(((*t83)).f1));
  t85 = ((uint64_t)(((uint64_t)((uint64_t)(t78) + (uint64_t)(t84)))));
  /* FG_DIRECT_SLICE_INDEX */
  t86 = v4;
  t87 = ((gg_type_1 *)(v0).data) + gg_index(t86, (v0).len);
  t88 = ((uint64_t)(((*t87)).f0));
  t72 = ((t85) == (t88));
  }
  t89 = t72;
  if (t89) {
  {
  t90 = v0;
  t91 = ((int64_t)(v4));
  t92 = ((int64_t)(((int64_t)UINT64_C(1))));
  t93 = ((int64_t)((uint64_t)(t91) - (uint64_t)(t92)));
  t94 = ((gg_type_1 *)t90.data) + gg_index(t93, t90.len);
  t95 = ((*t94)).f1;
  /* FG_DIRECT_SLICE_INDEX */
  t96 = v4;
  t97 = ((gg_type_1 *)(v0).data) + gg_index(t96, (v0).len);
  t98 = ((uint64_t)(((*t97)).f1));
  ((*t94)).f1 = ((uint64_t)((uint64_t)(t95) + (uint64_t)(t98)));
  t99 = v0;
  t100 = ((int64_t)(v1));
  t101 = ((int64_t)(v4));
  t0 = gg_function_2(t99, t100, t101, base + 0);
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
  }
  }
label1:;
  t7++; goto label0;
label2:;
  t102 = ((int64_t)(v1));
  t103 = ((int64_t)((v0).len));
  if (!(((t102) < (t103)))) gg_fail("assertion failed");
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t104 = ((int64_t)(v5));
  t105 = ((int64_t)(v1));
  t106 = ((t104) < (t105));
  t107 = t106;
  if (t107) {
  /* FG_DIRECT_SLICE_INDEX */
  t108 = v5;
  t109 = ((gg_type_1 *)(v0).data) + gg_index(t108, (v0).len);
  t110 = ((uint64_t)(((*t109)).f0));
  t111 = ((uint64_t)(v2));
  t107 = ((t110) < (t111));
  }
  if (!(t107)) goto label5;
  {
  t112 = v5;
  t113 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t112) + (uint64_t)(t113)));
  }
  goto label4;
label5:;
  v6 = ((int64_t)(v1));
label7:;
  t114 = ((int64_t)(v6));
  t115 = ((int64_t)(v5));
  if (!(((t114) > (t115)))) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t116 = v6;
  t117 = ((gg_type_1 *)(v0).data) + gg_index(t116, (v0).len);
  t118 = v0;
  t119 = ((int64_t)(v6));
  t120 = ((int64_t)(((int64_t)UINT64_C(1))));
  t121 = ((int64_t)((uint64_t)(t119) - (uint64_t)(t120)));
  t122 = ((gg_type_1 *)t118.data) + gg_index(t121, t118.len);
  (*t117) = (*t122);
  t123 = v6;
  t124 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t123) - (uint64_t)(t124)));
  }
  goto label7;
label8:;
  /* FG_DIRECT_SLICE_INDEX */
  t125 = v5;
  t126 = ((gg_type_1 *)(v0).data) + gg_index(t125, (v0).len);
  memset(&((*t126)), 0, sizeof((*t126)));
  ((*t126)).f0 = ((uint64_t)(v2));
  ((*t126)).f1 = ((uint64_t)(v3));
  t127 = ((int64_t)(v1));
  t128 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t127) + (uint64_t)(t128)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  gg_type_1 t0;
  uint64_t t1;
  uint64_t t2;
  gg_type_1 t3;
  int64_t t4;
  int64_t t5;
  gg_type_1 * t6;
  int64_t t7;
  gg_type_1 * t8;
  gg_slice t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  gg_type_1 * t18;
  uint64_t t19;
  uint64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  gg_type_1 * t24;
  uint64_t t25;
  uint64_t t26;
  gg_type_3 v0;
  int64_t v1;
  {
  memset(&(t0), 0, sizeof(t0));
  memset(&t1, 0, sizeof(t1));
  (t0).f0 = ((uint64_t)(t1));
  memset(&t2, 0, sizeof(t2));
  (t0).f1 = ((uint64_t)(t2));
  t3 = t0;
  for (t4=0; t4<4; t4++) (v0).data[t4] = t3;
  /* FG_DIRECT_SLICE_INDEX */
  t5 = ((int64_t)UINT64_C(0));
  t6 = ((gg_type_1 *)(v0).data) + gg_index(t5, ((int64_t)UINT64_C(4)));
  memset(&((*t6)), 0, sizeof((*t6)));
  ((*t6)).f0 = ((uint64_t)(((int64_t)UINT64_C(0))));
  ((*t6)).f1 = ((uint64_t)(((int64_t)UINT64_C(16))));
  /* FG_DIRECT_SLICE_INDEX */
  t7 = ((int64_t)UINT64_C(1));
  t8 = ((gg_type_1 *)(v0).data) + gg_index(t7, ((int64_t)UINT64_C(4)));
  memset(&((*t8)), 0, sizeof((*t8)));
  ((*t8)).f0 = ((uint64_t)(((int64_t)UINT64_C(32))));
  ((*t8)).f1 = ((uint64_t)(((int64_t)UINT64_C(16))));
  t9 = gg_view((v0).data, 4);
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = ((uint64_t)(((int64_t)UINT64_C(16))));
  t12 = ((uint64_t)(((int64_t)UINT64_C(16))));
  v1 = gg_function_1(t9, t10, t11, t12, base + 0);
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)UINT64_C(1))));
  t15 = ((t13) == (t14));
  t16 = t15;
  if (t16) {
  /* FG_DIRECT_SLICE_INDEX */
  t17 = ((int64_t)UINT64_C(0));
  t18 = ((gg_type_1 *)(v0).data) + gg_index(t17, ((int64_t)UINT64_C(4)));
  t19 = ((uint64_t)(((*t18)).f0));
  t20 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t19) == (t20));
  }
  t21 = t16;
  t22 = t21;
  if (t22) {
  /* FG_DIRECT_SLICE_INDEX */
  t23 = ((int64_t)UINT64_C(0));
  t24 = ((gg_type_1 *)(v0).data) + gg_index(t23, ((int64_t)UINT64_C(4)));
  t25 = ((uint64_t)(((*t24)).f1));
  t26 = ((uint64_t)(((int64_t)UINT64_C(48))));
  t22 = ((t25) == (t26));
  }
  if (!(t22)) gg_fail("assertion failed");
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
