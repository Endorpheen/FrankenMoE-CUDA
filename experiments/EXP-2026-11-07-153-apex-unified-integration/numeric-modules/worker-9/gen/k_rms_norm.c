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
typedef struct { float data[32]; } gg_type_3;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_8;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_8 data[4]; } gg_type_9;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(float a, float b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, float value);
static uint8_t gg_eq_2(gg_type_2 a, gg_type_2 b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, gg_type_2 value);
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, gg_type_3 value);
static uint8_t gg_eq_4(gg_slice a, gg_slice b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, gg_slice value);
static uint8_t gg_eq_5(int64_t a, int64_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, int64_t value);
static uint8_t gg_eq_6(double a, double b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, double value);
static uint8_t gg_eq_7(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, uint8_t value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, gg_type_9 value);
static uint8_t gg_eq_11(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_11(gg_slice a, gg_slice b);
static void gg_render_11(gg_reference out, uint8_t value);
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
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((gg_type_3*)a.data)[i],((gg_type_3*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(gg_type_3 a, gg_type_3 b) {
return gg_equal_elements_1(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_3(gg_reference out, gg_type_3 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((gg_slice*)a.data)[i],((gg_slice*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(gg_slice a, gg_slice b) {
return gg_equal_elements_1(a,b);
}
static void gg_render_4(gg_reference out, gg_slice value) {
int64_t i; gg_slice view=value;gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(double a, double b) {
return a == b;
}
static void gg_render_6(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_7(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
return gg_equal_elements_1(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_1(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((gg_type_9*)a.data)[i],((gg_type_9*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(gg_type_9 a, gg_type_9 b) {
return gg_equal_elements_8(gg_view((a).data, 4),gg_view((b).data, 4));
}
static void gg_render_9(gg_reference out, gg_type_9 value) {
int64_t i; gg_slice view=gg_view((value).data, 4);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_8(out, ((gg_type_8*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_11(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_11(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_11(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_11(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, float p8, int64_t p9, int64_t p10, int64_t p11, gg_slice p12, int64_t base);
static float gg_function_7(float p0, gg_slice p1, int64_t p2, int64_t base);
static float gg_function_8(float p0, int64_t base);
static float gg_function_8(float p0, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  float t3;
  float t4;
  int64_t t5;
  float t6;
  float t7;
  int64_t t8;
  int64_t t9;
  float v0;
  float v1;
  int64_t v2;
  v0 = p0;
  {
  v1 = ((float)(v0));
  v2 = ((int64_t)(((int64_t)UINT64_C(16))));
label1:;
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) goto label2;
  {
  t3 = ((float)(v1));
  t4 = ((float)(v1));
  t5 = ((int64_t)(v2));
  t6 = apex_shfl_xor_f32(t4, t5);
  t7 = ((float)(t6));
  v1 = ((float)(((t3) + (t7))));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  v2 = ((int64_t)(((int64_t)gg_div(t8, t9))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static float gg_function_7(float p0, gg_slice p1, int64_t p2, int64_t base) {
  float t0;
  float t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  int64_t t14;
  float * t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
  int64_t t21;
  float * t22;
  float t23;
  float v0;
  gg_slice v1;
  int64_t v2;
  float v3;
  int64_t v4;
  int64_t v5;
  float v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((float)(v0));
  v3 = gg_function_8(t1, base + 0);
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(((int64_t)UINT64_C(32))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = apex_tid_x();
  t6 = ((int64_t)(t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(32))));
  v4 = ((int64_t)(((int64_t)gg_mod(t6, t7))));
  t8 = apex_tid_x();
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(32))));
  v5 = ((int64_t)(((int64_t)gg_div(t9, t10))));
  t11 = ((int64_t)(v4));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) == (t12));
  if (t13) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t14 = v5;
  t15 = ((float *)(v1).data) + gg_index(t14, (v1).len);
  (*t15) = ((float)(v3));
  }
  }
  apex_sync();
  v6 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(v2));
  t18 = ((int64_t)(((int64_t)UINT64_C(32))));
  t19 = ((int64_t)(((int64_t)gg_div(t17, t18))));
  t20 = ((t16) < (t19));
  if (t20) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t21 = v4;
  t22 = ((float *)(v1).data) + gg_index(t21, (v1).len);
  v6 = ((float)((*t22)));
  }
  }
  t23 = ((float)(v6));
  v3 = gg_function_8(t23, base + 0);
  }
  }
  t0 = ((float)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_1(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, float p8, int64_t p9, int64_t p10, int64_t p11, gg_slice p12, int64_t base) {
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
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  gg_type_8 t34;
  float t35;
  float t36;
  int64_t t37;
  gg_type_8 t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  gg_type_8 * t56;
  gg_slice t57;
  int64_t t58;
  float * t59;
  gg_slice t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  float * t64;
  float t65;
  int64_t t66;
  gg_type_8 * t67;
  gg_slice t68;
  int64_t t69;
  float * t70;
  float t71;
  int64_t t72;
  gg_type_8 * t73;
  gg_slice t74;
  int64_t t75;
  float * t76;
  float t77;
  float t78;
  int64_t t79;
  gg_type_8 * t80;
  gg_slice t81;
  int64_t t82;
  float * t83;
  float t84;
  int64_t t85;
  gg_type_8 * t86;
  gg_slice t87;
  int64_t t88;
  float * t89;
  float t90;
  float t91;
  float t92;
  int64_t t93;
  gg_type_8 * t94;
  gg_slice t95;
  int64_t t96;
  float * t97;
  float t98;
  int64_t t99;
  gg_type_8 * t100;
  gg_slice t101;
  int64_t t102;
  float * t103;
  float t104;
  float t105;
  float t106;
  int64_t t107;
  gg_type_8 * t108;
  gg_slice t109;
  int64_t t110;
  float * t111;
  float t112;
  int64_t t113;
  gg_type_8 * t114;
  gg_slice t115;
  int64_t t116;
  float * t117;
  float t118;
  float t119;
  float t120;
  int64_t t121;
  int64_t t122;
  float t123;
  gg_slice t124;
  int64_t t125;
  float t126;
  float t127;
  float t128;
  float t129;
  float t130;
  float t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  gg_slice t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  float * t151;
  float t152;
  int64_t t153;
  gg_type_8 * t154;
  gg_slice t155;
  int64_t t156;
  float * t157;
  float t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  gg_slice t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  float * t167;
  float t168;
  float t169;
  float t170;
  float t171;
  int64_t t172;
  int64_t t173;
  float t174;
  gg_slice t175;
  int64_t t176;
  float t177;
  float t178;
  float t179;
  float t180;
  float t181;
  float t182;
  int64_t t183;
  int64_t t184;
  gg_slice t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  float * t189;
  float t190;
  gg_slice t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  float * t195;
  float t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  uint8_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  gg_slice v0;
  gg_slice v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  float v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  gg_slice v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  float v20;
  int64_t v21;
  gg_type_9 v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  float v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  float v31;
  float v32;
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
  {
  t0 = apex_gdim_x();
  v13 = ((int64_t)(t0));
  t1 = apex_bid_x();
  v14 = ((int64_t)(t1));
  t2 = apex_tid_x();
  v15 = ((int64_t)(t2));
  t3 = apex_bid_z();
  v16 = ((int64_t)(t3));
label1:;
  t4 = ((int64_t)(v16));
  t5 = ((int64_t)(v4));
  if (!(((t4) < (t5)))) goto label2;
  {
  t6 = apex_bid_y();
  v17 = ((int64_t)(t6));
label4:;
  t7 = ((int64_t)(v17));
  t8 = ((int64_t)(v3));
  if (!(((t7) < (t8)))) goto label5;
  {
  t9 = ((int64_t)(v16));
  t10 = ((int64_t)(v7));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(v17));
  t13 = ((int64_t)(v6));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t14)))));
  t16 = ((int64_t)(v14));
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t18)))));
  t19 = ((int64_t)(v16));
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(v17));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  t24 = ((int64_t)(v13));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t26 = ((int64_t)(v14));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  t28 = ((int64_t)(v2));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  v20 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t29 = ((int64_t)(v9));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t29) != (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v2));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  v21 = ((int64_t)(((int64_t)gg_div(t32, t33))));
  memset(&t35, 0, sizeof(t35));
  t36 = ((float)(t35));
  for (t37=0; t37<4; t37++) (t34).data[t37] = t36;
  t38 = t34;
  for (t39=0; t39<4; t39++) (v22).data[t39] = t38;
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t40 = ((int64_t)(v23));
  t41 = ((int64_t)(v11));
  if (!(((t40) < (t41)))) goto label8;
  {
  t42 = ((int64_t)(v15));
  t43 = ((int64_t)(v23));
  t44 = ((int64_t)(v10));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t45)))));
  t46 = ((int64_t)(v24));
  t47 = ((int64_t)(v21));
  t48 = ((t46) < (t47));
  if (t48) {
  {
  t49 = ((int64_t)(v18));
  t50 = ((int64_t)(v24));
  t51 = ((int64_t)(((int64_t)UINT64_C(4))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t52)))));
  t54 = ((int64_t)UINT64_C(4));
  t53 = 0;
label9:;
  if (t53 >= t54) goto label11;
  v26 = t53;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t55 = v23;
  t56 = ((gg_type_8 *)(v22).data) + gg_index(t55, ((int64_t)UINT64_C(4)));
  t57 = gg_view(((*t56)).data, 4);
  t58 = v26;
  t59 = ((float *)t57.data) + gg_index(t58, t57.len);
  t60 = v0;
  t61 = ((int64_t)(v25));
  t62 = ((int64_t)(v26));
  t63 = ((int64_t)((uint64_t)(t61) + (uint64_t)(t62)));
  t64 = ((float *)t60.data) + gg_index(t63, t60.len);
  (*t59) = ((float)((*t64)));
  }
label10:;
  t53++; goto label9;
label11:;
  t65 = ((float)(v20));
  /* FG_DIRECT_SLICE_INDEX */
  t66 = v23;
  t67 = ((gg_type_8 *)(v22).data) + gg_index(t66, ((int64_t)UINT64_C(4)));
  t68 = gg_view(((*t67)).data, 4);
  t69 = ((int64_t)UINT64_C(0));
  t70 = ((float *)t68.data) + gg_index(t69, t68.len);
  t71 = ((float)((*t70)));
  /* FG_DIRECT_SLICE_INDEX */
  t72 = v23;
  t73 = ((gg_type_8 *)(v22).data) + gg_index(t72, ((int64_t)UINT64_C(4)));
  t74 = gg_view(((*t73)).data, 4);
  t75 = ((int64_t)UINT64_C(0));
  t76 = ((float *)t74.data) + gg_index(t75, t74.len);
  t77 = ((float)((*t76)));
  t78 = ((float)(((t71) * (t77))));
  /* FG_DIRECT_SLICE_INDEX */
  t79 = v23;
  t80 = ((gg_type_8 *)(v22).data) + gg_index(t79, ((int64_t)UINT64_C(4)));
  t81 = gg_view(((*t80)).data, 4);
  t82 = ((int64_t)UINT64_C(1));
  t83 = ((float *)t81.data) + gg_index(t82, t81.len);
  t84 = ((float)((*t83)));
  /* FG_DIRECT_SLICE_INDEX */
  t85 = v23;
  t86 = ((gg_type_8 *)(v22).data) + gg_index(t85, ((int64_t)UINT64_C(4)));
  t87 = gg_view(((*t86)).data, 4);
  t88 = ((int64_t)UINT64_C(1));
  t89 = ((float *)t87.data) + gg_index(t88, t87.len);
  t90 = ((float)((*t89)));
  t91 = ((float)(((t84) * (t90))));
  t92 = ((float)(((t78) + (t91))));
  /* FG_DIRECT_SLICE_INDEX */
  t93 = v23;
  t94 = ((gg_type_8 *)(v22).data) + gg_index(t93, ((int64_t)UINT64_C(4)));
  t95 = gg_view(((*t94)).data, 4);
  t96 = ((int64_t)UINT64_C(2));
  t97 = ((float *)t95.data) + gg_index(t96, t95.len);
  t98 = ((float)((*t97)));
  /* FG_DIRECT_SLICE_INDEX */
  t99 = v23;
  t100 = ((gg_type_8 *)(v22).data) + gg_index(t99, ((int64_t)UINT64_C(4)));
  t101 = gg_view(((*t100)).data, 4);
  t102 = ((int64_t)UINT64_C(2));
  t103 = ((float *)t101.data) + gg_index(t102, t101.len);
  t104 = ((float)((*t103)));
  t105 = ((float)(((t98) * (t104))));
  t106 = ((float)(((t92) + (t105))));
  /* FG_DIRECT_SLICE_INDEX */
  t107 = v23;
  t108 = ((gg_type_8 *)(v22).data) + gg_index(t107, ((int64_t)UINT64_C(4)));
  t109 = gg_view(((*t108)).data, 4);
  t110 = ((int64_t)UINT64_C(3));
  t111 = ((float *)t109.data) + gg_index(t110, t109.len);
  t112 = ((float)((*t111)));
  /* FG_DIRECT_SLICE_INDEX */
  t113 = v23;
  t114 = ((gg_type_8 *)(v22).data) + gg_index(t113, ((int64_t)UINT64_C(4)));
  t115 = gg_view(((*t114)).data, 4);
  t116 = ((int64_t)UINT64_C(3));
  t117 = ((float *)t115.data) + gg_index(t116, t115.len);
  t118 = ((float)((*t117)));
  t119 = ((float)(((t112) * (t118))));
  t120 = ((float)(((t106) + (t119))));
  v20 = ((float)(((t65) + (t120))));
  }
  }
  t121 = v23;
  t122 = ((int64_t)(((int64_t)UINT64_C(1))));
  v23 = ((int64_t)((uint64_t)(t121) + (uint64_t)(t122)));
  }
  goto label7;
label8:;
  t123 = ((float)(v20));
  t124 = v12;
  t125 = ((int64_t)(v10));
  v20 = gg_function_7(t123, t124, t125, base + 0);
  t126 = ((float)(v20));
  t127 = ((float)(((float)(v2))));
  t128 = ((float)(((t126) / (t127))));
  t129 = ((float)(v8));
  t130 = ((float)(((t128) + (t129))));
  t131 = apex_rsqrtf(t130);
  v27 = ((float)(t131));
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t132 = ((int64_t)(v23));
  t133 = ((int64_t)(v11));
  if (!(((t132) < (t133)))) goto label14;
  {
  t134 = ((int64_t)(v15));
  t135 = ((int64_t)(v23));
  t136 = ((int64_t)(v10));
  t137 = ((int64_t)(((int64_t)((uint64_t)(t135) * (uint64_t)(t136)))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t134) + (uint64_t)(t137)))));
  t138 = ((int64_t)(v28));
  t139 = ((int64_t)(v21));
  t140 = ((t138) < (t139));
  if (t140) {
  {
  t142 = ((int64_t)UINT64_C(4));
  t141 = 0;
label15:;
  if (t141 >= t142) goto label17;
  v29 = t141;
  {
  t143 = v1;
  t144 = ((int64_t)(v19));
  t145 = ((int64_t)(v28));
  t146 = ((int64_t)(((int64_t)UINT64_C(4))));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t145) * (uint64_t)(t146)))));
  t148 = ((int64_t)(((int64_t)((uint64_t)(t144) + (uint64_t)(t147)))));
  t149 = ((int64_t)(v29));
  t150 = ((int64_t)((uint64_t)(t148) + (uint64_t)(t149)));
  t151 = ((float *)t143.data) + gg_index(t150, t143.len);
  t152 = ((float)(v27));
  /* FG_DIRECT_SLICE_INDEX */
  t153 = v23;
  t154 = ((gg_type_8 *)(v22).data) + gg_index(t153, ((int64_t)UINT64_C(4)));
  t155 = gg_view(((*t154)).data, 4);
  t156 = v29;
  t157 = ((float *)t155.data) + gg_index(t156, t155.len);
  t158 = ((float)((*t157)));
  (*t151) = ((float)(((t152) * (t158))));
  }
label16:;
  t141++; goto label15;
label17:;
  }
  }
  t159 = v23;
  t160 = ((int64_t)(((int64_t)UINT64_C(1))));
  v23 = ((int64_t)((uint64_t)(t159) + (uint64_t)(t160)));
  }
  goto label13;
label14:;
  }
  } else {
  {
  v30 = ((int64_t)(v15));
label19:;
  t161 = ((int64_t)(v30));
  t162 = ((int64_t)(v2));
  if (!(((t161) < (t162)))) goto label20;
  {
  t163 = v0;
  t164 = ((int64_t)(v18));
  t165 = ((int64_t)(v30));
  t166 = ((int64_t)((uint64_t)(t164) + (uint64_t)(t165)));
  t167 = ((float *)t163.data) + gg_index(t166, t163.len);
  v31 = ((float)((*t167)));
  t168 = ((float)(v20));
  t169 = ((float)(v31));
  t170 = ((float)(v31));
  t171 = ((float)(((t169) * (t170))));
  v20 = ((float)(((t168) + (t171))));
  t172 = v30;
  t173 = ((int64_t)(v10));
  v30 = ((int64_t)((uint64_t)(t172) + (uint64_t)(t173)));
  }
  goto label19;
label20:;
  t174 = ((float)(v20));
  t175 = v12;
  t176 = ((int64_t)(v10));
  v20 = gg_function_7(t174, t175, t176, base + 0);
  t177 = ((float)(v20));
  t178 = ((float)(((float)(v2))));
  t179 = ((float)(((t177) / (t178))));
  t180 = ((float)(v8));
  t181 = ((float)(((t179) + (t180))));
  t182 = apex_rsqrtf(t181);
  v32 = ((float)(t182));
  v30 = ((int64_t)(v15));
label22:;
  t183 = ((int64_t)(v30));
  t184 = ((int64_t)(v2));
  if (!(((t183) < (t184)))) goto label23;
  {
  t185 = v1;
  t186 = ((int64_t)(v19));
  t187 = ((int64_t)(v30));
  t188 = ((int64_t)((uint64_t)(t186) + (uint64_t)(t187)));
  t189 = ((float *)t185.data) + gg_index(t188, t185.len);
  t190 = ((float)(v32));
  t191 = v0;
  t192 = ((int64_t)(v18));
  t193 = ((int64_t)(v30));
  t194 = ((int64_t)((uint64_t)(t192) + (uint64_t)(t193)));
  t195 = ((float *)t191.data) + gg_index(t194, t191.len);
  t196 = ((float)((*t195)));
  (*t189) = ((float)(((t190) * (t196))));
  t197 = v30;
  t198 = ((int64_t)(v10));
  v30 = ((int64_t)((uint64_t)(t197) + (uint64_t)(t198)));
  }
  goto label22;
label23:;
  }
  }
  t199 = ((int64_t)(v10));
  t200 = ((int64_t)(((int64_t)UINT64_C(32))));
  t201 = ((t199) > (t200));
  if (t201) {
  {
  apex_sync();
  }
  }
  t202 = v17;
  t203 = apex_gdim_y();
  t204 = ((int64_t)(t203));
  v17 = ((int64_t)((uint64_t)(t202) + (uint64_t)(t204)));
  }
  goto label4;
label5:;
  t205 = v16;
  t206 = apex_gdim_z();
  t207 = ((int64_t)(t206));
  v16 = ((int64_t)((uint64_t)(t205) + (uint64_t)(t207)));
  }
  goto label1;
label2:;
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
  float t6;
  float t7;
  int64_t t8;
  gg_slice t9;
  gg_slice t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  float t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  gg_slice t21;
  gg_header t22;
  int64_t t23;
  float * t24;
  float t25;
  int64_t t26;
  float * t27;
  float t28;
  int64_t t29;
  gg_type_2 v0;
  gg_type_2 v1;
  gg_type_3 v2;
  {
  memset(&t0, 0, sizeof(t0));
  t1 = ((float)(t0));
  for (t2=0; t2<4096; t2++) (v0).data[t2] = t1;
  memset(&t3, 0, sizeof(t3));
  t4 = ((float)(t3));
  for (t5=0; t5<4096; t5++) (v1).data[t5] = t4;
  memset(&t6, 0, sizeof(t6));
  t7 = ((float)(t6));
  for (t8=0; t8<32; t8++) (v2).data[t8] = t7;
  t9 = gg_view((v0).data, 4096);
  t10 = gg_view((v1).data, 4096);
  t11 = ((int64_t)(((int64_t)UINT64_C(4096))));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = ((int64_t)(((int64_t)UINT64_C(4096))));
  t15 = ((int64_t)(((int64_t)UINT64_C(4096))));
  t16 = ((int64_t)(((int64_t)UINT64_C(4096))));
  t17 = ((float)(((float)(gg_double_bits(UINT64_C(4532020583610935537))))));
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  t19 = ((int64_t)(((int64_t)UINT64_C(1024))));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = gg_view((v2).data, 32);
  gg_function_1(t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, t20, t21, base + 0);
  t22.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t22.len = 0;
  /* FG_DIRECT_SLICE_INDEX */
  t23 = ((int64_t)UINT64_C(0));
  t24 = ((float *)(v0).data) + gg_index(t23, ((int64_t)UINT64_C(4096)));
  t25 = ((float)((*t24)));
  /* FG_DIRECT_SLICE_INDEX */
  t26 = ((int64_t)UINT64_C(0));
  t27 = ((float *)(v1).data) + gg_index(t26, ((int64_t)UINT64_C(4096)));
  t28 = ((float)((*t27)));
  gg_float32(gg_ref(&t22, gg_get_stack(base + 0)), ((t25) + (t28)));
  gg_append(gg_ref(&t22, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t22).base, (t22).len));
  }
gg_done:;
  for (t29 = 0; t29 < 1; t29++) gg_stacks[base + t29].top = 0;
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
