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
typedef struct { float data[32]; } gg_type_8;
#pragma pack(pop)
static void gg_quote(gg_reference out,gg_slice text) { int64_t i; unsigned char c; gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){gg_append(out,"\\",1,1);gg_append(out,&c,1,1);} else if(c=='\n')gg_append(out,"\\n",2,1);else if(c=='\r')gg_append(out,"\\r",2,1);else if(c=='\t')gg_append(out,"\\t",2,1);else gg_append(out,&c,1,1);}gg_append(out,"\"",1,1);}
static uint8_t gg_eq_1(int64_t a, int64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, int64_t value);
static uint8_t gg_eq_2(int32_t a, int32_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, int32_t value);
static uint8_t gg_eq_3(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_3(gg_slice a, gg_slice b);
static void gg_render_3(gg_reference out, uint64_t value);
static uint8_t gg_eq_4(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint8_t value);
static uint8_t gg_eq_5(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, uint32_t value);
static uint8_t gg_eq_7(float a, float b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, float value);
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b);
static uint8_t gg_equal_elements_8(gg_slice a, gg_slice b);
static void gg_render_8(gg_reference out, gg_type_8 value);
static uint8_t gg_eq_9(double a, double b);
static uint8_t gg_equal_elements_9(gg_slice a, gg_slice b);
static void gg_render_9(gg_reference out, double value);
static uint8_t gg_eq_10(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_10(gg_slice a, gg_slice b);
static void gg_render_10(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_3(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_3(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_3(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_3(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(float a, float b) {
return a == b;
}
static void gg_render_7(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_8(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_8(((gg_type_8*)a.data)[i],((gg_type_8*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_8(gg_type_8 a, gg_type_8 b) {
return gg_equal_elements_7(gg_view((a).data, 32),gg_view((b).data, 32));
}
static void gg_render_8(gg_reference out, gg_type_8 value) {
int64_t i; gg_slice view=gg_view((value).data, 32);gg_append(out,"[",1,1);for(i=0;i<view.len;i++){if(i)gg_append(out,", ",2,1);gg_render_7(out, ((float*)view.data)[i]);}gg_append(out,"]",1,1);
}
static uint8_t gg_equal_elements_9(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_9(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_9(double a, double b) {
return a == b;
}
static void gg_render_9(gg_reference out, double value) {
gg_float(out,value);
}
static uint8_t gg_equal_elements_10(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_10(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_10(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_10(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(int64_t p0, int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int64_t base);
static uint32_t gg_function_3(int64_t p0, uint32_t p1, int64_t p2, int64_t base);
static uint32_t gg_function_4(int64_t p0, uint32_t p1, int64_t p2, int64_t base);
static float gg_function_11(float p0, int32_t p1, int64_t base);
static float gg_function_11(float p0, int32_t p1, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  float t5;
  float t6;
  int32_t t7;
  int32_t t8;
  float t9;
  float t10;
  int32_t t11;
  int32_t t12;
  float v0;
  int32_t v1;
  float v2;
  int32_t v3;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  v3 = ((int32_t)(((int32_t)gg_div(t1, t2))));
label1:;
  t3 = ((int32_t)(v3));
  t4 = ((int32_t)(((int64_t)UINT64_C(0))));
  if (!(((t3) > (t4)))) goto label2;
  {
  t5 = v2;
  t6 = ((float)(v2));
  t7 = ((int32_t)(v3));
  t8 = ((int32_t)(v1));
  t9 = fg174s_shfl_xor(t6, t7, t8);
  t10 = ((float)(t9));
  v2 = ((t5) + (t10));
  t11 = v3;
  t12 = ((int32_t)(((int64_t)UINT64_C(1))));
  v3 = ((t11) >> ((uint64_t)(t12) & 31));
  }
  goto label1;
label2:;
  t0 = ((float)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint32_t gg_function_4(int64_t p0, uint32_t p1, int64_t p2, int64_t base) {
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
  uint32_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((uint32_t)(v1));
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = fg174s_dm(t2, t3, t4);
  t6 = ((uint32_t)(t5));
  t7 = fg174s_umulhi(t1, t6);
  v3 = ((uint32_t)(t7));
  t8 = ((uint64_t)(((uint64_t)(v3))));
  t9 = ((uint64_t)(((uint64_t)(v1))));
  t10 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = fg174s_dm(t11, t12, t13);
  t15 = ((uint64_t)(((uint64_t)(t14))));
  v4 = ((uint32_t)(((uint32_t)(((t10) >> ((uint64_t)(t15) & 63))))));
  t0 = ((uint32_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint32_t gg_function_3(int64_t p0, uint32_t p1, int64_t p2, int64_t base) {
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
  t5 = gg_function_4(t2, t3, t4, base + 0);
  t6 = ((uint32_t)(t5));
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  t10 = fg174s_dm(t7, t8, t9);
  t11 = ((uint32_t)(t10));
  t12 = ((uint32_t)(((uint32_t)((uint64_t)(t6) * (uint64_t)(t11)))));
  t0 = ((uint32_t)(((uint32_t)((uint64_t)(t1) - (uint64_t)(t12)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_1(int64_t p0, int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int64_t base) {
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
  int64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  uint64_t t48;
  uint64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint64_t t63;
  uint64_t t64;
  uint64_t t65;
  uint8_t t66;
  uint64_t t67;
  uint64_t t68;
  uint64_t t69;
  uint64_t t70;
  int32_t t71;
  int32_t t72;
  int32_t t73;
  int32_t t74;
  int32_t t75;
  int32_t t76;
  uint64_t t77;
  uint64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint64_t t83;
  uint64_t t84;
  int64_t t85;
  uint32_t t86;
  int64_t t87;
  uint32_t t88;
  int64_t t89;
  uint32_t t90;
  int64_t t91;
  uint32_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  uint64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  uint64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  uint64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  uint64_t t132;
  int64_t t133;
  int64_t t134;
  float t135;
  uint64_t t136;
  uint64_t t137;
  uint64_t t138;
  uint64_t t139;
  uint64_t t140;
  uint64_t t141;
  uint64_t t142;
  uint64_t t143;
  uint64_t t144;
  uint64_t t145;
  uint64_t t146;
  uint64_t t147;
  uint64_t t148;
  uint64_t t149;
  uint64_t t150;
  uint64_t t151;
  uint64_t t152;
  uint64_t t153;
  uint64_t t154;
  uint64_t t155;
  uint64_t t156;
  uint64_t t157;
  uint64_t t158;
  uint64_t t159;
  uint64_t t160;
  uint64_t t161;
  uint64_t t162;
  uint64_t t163;
  uint64_t t164;
  uint64_t t165;
  uint64_t t166;
  uint64_t t167;
  float t168;
  float t169;
  int64_t t170;
  int32_t t171;
  int32_t t172;
  int32_t t173;
  int32_t t174;
  int32_t t175;
  int32_t t176;
  int32_t t177;
  float * t178;
  uint64_t t179;
  uint64_t t180;
  float t181;
  int32_t t182;
  int32_t t183;
  uint64_t t184;
  uint64_t t185;
  uint64_t t186;
  uint64_t t187;
  uint64_t t188;
  uint64_t t189;
  uint64_t t190;
  uint64_t t191;
  uint64_t t192;
  uint64_t t193;
  uint64_t t194;
  uint64_t t195;
  uint64_t t196;
  uint64_t t197;
  uint64_t t198;
  uint64_t t199;
  uint64_t t200;
  uint64_t t201;
  uint64_t t202;
  uint64_t t203;
  uint64_t t204;
  uint64_t t205;
  uint64_t t206;
  uint64_t t207;
  uint64_t t208;
  uint64_t t209;
  uint64_t t210;
  uint64_t t211;
  uint64_t t212;
  uint64_t t213;
  uint64_t t214;
  uint64_t t215;
  uint64_t t216;
  uint64_t t217;
  uint64_t t218;
  uint64_t t219;
  uint64_t t220;
  uint64_t t221;
  uint64_t t222;
  uint64_t t223;
  uint64_t t224;
  uint64_t t225;
  uint64_t t226;
  uint64_t t227;
  uint64_t t228;
  uint64_t t229;
  uint64_t t230;
  uint64_t t231;
  uint64_t t232;
  uint64_t t233;
  float t234;
  float t235;
  float t236;
  int64_t t237;
  float t238;
  float t239;
  int64_t t240;
  int32_t t241;
  int32_t t242;
  int32_t t243;
  int32_t t244;
  int32_t t245;
  int32_t t246;
  int32_t t247;
  float * t248;
  uint64_t t249;
  uint64_t t250;
  float t251;
  int32_t t252;
  float * t253;
  uint64_t t254;
  uint64_t t255;
  float t256;
  int32_t t257;
  int32_t t258;
  uint8_t t259;
  uint8_t t260;
  uint64_t t261;
  uint64_t t262;
  uint64_t t263;
  uint64_t t264;
  uint64_t t265;
  uint64_t t266;
  float t267;
  float t268;
  float t269;
  int32_t t270;
  int32_t t271;
  float t272;
  int32_t t273;
  float * t274;
  float t275;
  int32_t t276;
  float * t277;
  float t278;
  float t279;
  int32_t t280;
  int32_t t281;
  float t282;
  int32_t t283;
  uint64_t t284;
  uint64_t t285;
  float t286;
  float t287;
  float t288;
  float t289;
  float t290;
  float t291;
  float t292;
  int32_t t293;
  int32_t t294;
  int32_t t295;
  float * t296;
  float t297;
  int32_t t298;
  float * t299;
  float t300;
  int32_t t301;
  float * t302;
  float t303;
  float t304;
  float t305;
  float t306;
  float t307;
  float t308;
  int32_t t309;
  float * t310;
  float t311;
  int32_t t312;
  float * t313;
  float t314;
  float t315;
  int32_t t316;
  int32_t t317;
  float t318;
  int32_t t319;
  int32_t t320;
  int32_t t321;
  uint8_t t322;
  uint64_t t323;
  uint64_t t324;
  float t325;
  float t326;
  float t327;
  uint64_t t328;
  uint64_t t329;
  uint64_t t330;
  uint64_t t331;
  uint64_t t332;
  uint64_t t333;
  int32_t t334;
  int32_t t335;
  int32_t t336;
  int32_t t337;
  int32_t t338;
  int32_t t339;
  float t340;
  uint64_t t341;
  uint64_t t342;
  float t343;
  float t344;
  float t345;
  float t346;
  int32_t t347;
  float * t348;
  float t349;
  float t350;
  int32_t t351;
  float * t352;
  float t353;
  float t354;
  int32_t t355;
  int32_t t356;
  float t357;
  int32_t t358;
  uint64_t t359;
  uint64_t t360;
  float t361;
  float t362;
  float t363;
  float t364;
  float t365;
  int32_t t366;
  int32_t t367;
  int32_t t368;
  int32_t t369;
  int32_t t370;
  int32_t t371;
  int32_t t372;
  float * t373;
  uint64_t t374;
  uint64_t t375;
  float t376;
  float t377;
  float t378;
  float t379;
  int32_t t380;
  float * t381;
  float t382;
  int32_t t383;
  float * t384;
  float t385;
  float t386;
  float t387;
  float t388;
  float t389;
  float t390;
  int32_t t391;
  float * t392;
  float t393;
  int32_t t394;
  float * t395;
  float t396;
  float t397;
  int32_t t398;
  int32_t t399;
  float t400;
  int32_t t401;
  int32_t t402;
  int32_t t403;
  uint8_t t404;
  uint64_t t405;
  uint64_t t406;
  float t407;
  float t408;
  float t409;
  uint64_t t410;
  uint64_t t411;
  uint64_t t412;
  uint64_t t413;
  uint64_t t414;
  uint64_t t415;
  uint8_t t416;
  uint64_t t417;
  uint64_t t418;
  uint64_t t419;
  uint64_t t420;
  uint64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  uint64_t t425;
  uint64_t t426;
  uint8_t t427;
  uint64_t t428;
  uint64_t t429;
  uint64_t t430;
  uint64_t t431;
  uint64_t t432;
  uint64_t t433;
  int32_t t434;
  int32_t t435;
  int32_t t436;
  int32_t t437;
  int32_t t438;
  int32_t t439;
  uint64_t t440;
  uint64_t t441;
  uint64_t t442;
  uint64_t t443;
  uint64_t t444;
  uint64_t t445;
  int32_t t446;
  float * t447;
  float t448;
  int32_t t449;
  int32_t t450;
  uint64_t t451;
  uint64_t t452;
  uint8_t t453;
  uint8_t t454;
  int32_t t455;
  int32_t t456;
  int32_t t457;
  int32_t t458;
  int32_t t459;
  int32_t t460;
  uint64_t t461;
  uint64_t t462;
  uint64_t t463;
  uint64_t t464;
  uint64_t t465;
  uint64_t t466;
  int32_t t467;
  float * t468;
  float t469;
  int32_t t470;
  int32_t t471;
  int64_t v0;
  int32_t v1;
  int32_t v2;
  int32_t v3;
  int32_t v4;
  int32_t v5;
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
  uint8_t v17;
  uint8_t v18;
  uint64_t v19;
  uint64_t v20;
  uint64_t v21;
  uint64_t v22;
  int32_t v23;
  int32_t v24;
  int32_t v25;
  int32_t v26;
  int32_t v27;
  uint64_t v28;
  uint32_t v29;
  uint32_t v30;
  uint64_t v31;
  uint64_t v32;
  uint64_t v33;
  uint64_t v34;
  uint64_t v35;
  uint64_t v36;
  uint64_t v37;
  uint64_t v38;
  uint64_t v39;
  uint64_t v40;
  uint64_t v41;
  uint64_t v42;
  float v43;
  uint64_t v44;
  uint64_t v45;
  uint64_t v46;
  uint64_t v47;
  uint64_t v48;
  gg_type_8 v49;
  int32_t v50;
  uint64_t v51;
  uint64_t v52;
  uint64_t v53;
  uint64_t v54;
  uint64_t v55;
  uint64_t v56;
  uint64_t v57;
  uint64_t v58;
  float v59;
  gg_type_8 v60;
  gg_type_8 v61;
  uint64_t v62;
  float v63;
  float v64;
  float v65;
  float v66;
  float v67;
  float v68;
  uint64_t v69;
  float v70;
  uint64_t v71;
  float v72;
  float v73;
  float v74;
  uint64_t v75;
  float v76;
  uint64_t v77;
  uint64_t v78;
  uint64_t v79;
  uint64_t v80;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg174s_dget(t0, t1, t2);
  v6 = ((uint64_t)(t3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = fg174s_dget(t4, t5, t6);
  v7 = ((uint64_t)(t7));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((int64_t)(((int64_t)UINT64_C(2))));
  t11 = fg174s_dget(t8, t9, t10);
  v8 = ((uint64_t)(t11));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((int64_t)(((int64_t)UINT64_C(3))));
  t15 = fg174s_dget(t12, t13, t14);
  v9 = ((uint64_t)(t15));
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = fg174s_dget(t16, t17, t18);
  v10 = ((uint64_t)(t19));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(5))));
  t23 = fg174s_dget(t20, t21, t22);
  v11 = ((uint64_t)(t23));
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((int64_t)(((int64_t)UINT64_C(6))));
  t27 = fg174s_dget(t24, t25, t26);
  v12 = ((uint64_t)(t27));
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((int64_t)(((int64_t)UINT64_C(7))));
  t31 = fg174s_dget(t28, t29, t30);
  v13 = ((uint64_t)(t31));
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(3))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg174s_dget(t32, t33, t34);
  v14 = ((uint64_t)(t35));
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t39 = fg174s_dget(t36, t37, t38);
  v15 = ((uint64_t)(t39));
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(3))));
  t42 = ((int64_t)(((int64_t)UINT64_C(2))));
  t43 = fg174s_dget(t40, t41, t42);
  v16 = ((uint64_t)(t43));
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(3))));
  t46 = ((int64_t)(((int64_t)UINT64_C(7))));
  t47 = fg174s_dget(t44, t45, t46);
  t48 = ((uint64_t)(t47));
  t49 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  v17 = ((t48) == (t49));
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(((int64_t)UINT64_C(3))));
  t52 = ((int64_t)(((int64_t)UINT64_C(16))));
  t53 = fg174s_dget(t50, t51, t52);
  t54 = ((uint64_t)(t53));
  t55 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  v18 = ((t54) == (t55));
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(((int64_t)UINT64_C(3))));
  t58 = ((int64_t)(((int64_t)UINT64_C(13))));
  t59 = fg174s_dget(t56, t57, t58);
  v19 = ((uint64_t)(t59));
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(3))));
  t62 = ((int64_t)(((int64_t)UINT64_C(12))));
  t63 = fg174s_dget(t60, t61, t62);
  v20 = ((uint64_t)(t63));
  t64 = ((uint64_t)(v19));
  t65 = ((uint64_t)(v14));
  t66 = ((t64) < (t65));
  if (t66) {
  {
  v21 = ((uint64_t)(v19));
  }
  } else {
  {
  v21 = ((uint64_t)(v14));
  }
  }
  t67 = ((uint64_t)(v21));
  t68 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  v22 = ((uint64_t)(((uint64_t)gg_udiv(t67, t68))));
  v23 = ((int32_t)(((int32_t)(v22))));
  t69 = ((uint64_t)(v14));
  t70 = ((uint64_t)(v22));
  v24 = ((int32_t)(((int32_t)(((uint64_t)gg_udiv(t69, t70))))));
  v25 = ((int32_t)(v4));
  t71 = ((int32_t)(v25));
  t72 = ((int32_t)(v23));
  v26 = ((int32_t)(((int32_t)gg_div(t71, t72))));
  t73 = ((int32_t)(v25));
  t74 = ((int32_t)(v26));
  t75 = ((int32_t)(v23));
  t76 = ((int32_t)(((int32_t)((uint64_t)(t74) * (uint64_t)(t75)))));
  v27 = ((int32_t)(((int32_t)((uint64_t)(t73) - (uint64_t)(t76)))));
  t77 = ((uint64_t)(((uint64_t)(v3))));
  t78 = ((uint64_t)(v20));
  t79 = ((uint64_t)(((uint64_t)((uint64_t)(t77) * (uint64_t)(t78)))));
  t80 = ((uint64_t)(((uint64_t)(v5))));
  t81 = ((uint64_t)(((uint64_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  t82 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t83 = ((uint64_t)(((uint64_t)((uint64_t)(t81) * (uint64_t)(t82)))));
  t84 = ((uint64_t)(((uint64_t)(v26))));
  v28 = ((uint64_t)(((uint64_t)((uint64_t)(t83) + (uint64_t)(t84)))));
  v29 = ((uint32_t)(((uint32_t)(((uint64_t)(v1))))));
  v30 = ((uint32_t)(((uint32_t)(((uint64_t)(v2))))));
  t85 = ((int64_t)(v0));
  t86 = ((uint32_t)(v29));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = gg_function_3(t85, t86, t87, base + 0);
  v31 = ((uint64_t)(((uint64_t)(t88))));
  t89 = ((int64_t)(v0));
  t90 = ((uint32_t)(v30));
  t91 = ((int64_t)(((int64_t)UINT64_C(1))));
  t92 = gg_function_4(t89, t90, t91, base + 0);
  v32 = ((uint64_t)(((uint64_t)(t92))));
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(((int64_t)UINT64_C(2))));
  t95 = ((int64_t)(((int64_t)UINT64_C(1))));
  t96 = fg174s_dget(t93, t94, t95);
  v33 = ((uint64_t)(t96));
  t97 = ((int64_t)(v0));
  t98 = ((int64_t)(((int64_t)UINT64_C(2))));
  t99 = ((int64_t)(((int64_t)UINT64_C(2))));
  t100 = fg174s_dget(t97, t98, t99);
  v34 = ((uint64_t)(t100));
  t101 = ((int64_t)(v0));
  t102 = ((int64_t)(((int64_t)UINT64_C(2))));
  t103 = ((int64_t)(((int64_t)UINT64_C(3))));
  t104 = fg174s_dget(t101, t102, t103);
  v35 = ((uint64_t)(t104));
  t105 = ((int64_t)(v0));
  t106 = ((int64_t)(((int64_t)UINT64_C(2))));
  t107 = ((int64_t)(((int64_t)UINT64_C(9))));
  t108 = fg174s_dget(t105, t106, t107);
  v36 = ((uint64_t)(t108));
  t109 = ((int64_t)(v0));
  t110 = ((int64_t)(((int64_t)UINT64_C(2))));
  t111 = ((int64_t)(((int64_t)UINT64_C(10))));
  t112 = fg174s_dget(t109, t110, t111);
  v37 = ((uint64_t)(t112));
  t113 = ((int64_t)(v0));
  t114 = ((int64_t)(((int64_t)UINT64_C(2))));
  t115 = ((int64_t)(((int64_t)UINT64_C(11))));
  t116 = fg174s_dget(t113, t114, t115);
  v38 = ((uint64_t)(t116));
  t117 = ((int64_t)(v0));
  t118 = ((int64_t)(((int64_t)UINT64_C(2))));
  t119 = ((int64_t)(((int64_t)UINT64_C(17))));
  t120 = fg174s_dget(t117, t118, t119);
  v39 = ((uint64_t)(t120));
  t121 = ((int64_t)(v0));
  t122 = ((int64_t)(((int64_t)UINT64_C(2))));
  t123 = ((int64_t)(((int64_t)UINT64_C(18))));
  t124 = fg174s_dget(t121, t122, t123);
  v40 = ((uint64_t)(t124));
  t125 = ((int64_t)(v0));
  t126 = ((int64_t)(((int64_t)UINT64_C(2))));
  t127 = ((int64_t)(((int64_t)UINT64_C(19))));
  t128 = fg174s_dget(t125, t126, t127);
  v41 = ((uint64_t)(t128));
  t129 = ((int64_t)(v0));
  t130 = ((int64_t)(((int64_t)UINT64_C(3))));
  t131 = ((int64_t)(((int64_t)UINT64_C(15))));
  t132 = fg174s_dget(t129, t130, t131);
  v42 = ((uint64_t)(t132));
  t133 = ((int64_t)(v0));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = fg174s_df(t133, t134);
  v43 = ((float)(t135));
  t136 = ((uint64_t)(((uint64_t)(v30))));
  t137 = ((uint64_t)(v15));
  t138 = ((uint64_t)(((uint64_t)((uint64_t)(t136) * (uint64_t)(t137)))));
  t139 = ((uint64_t)(((uint64_t)(v29))));
  t140 = ((uint64_t)(((uint64_t)((uint64_t)(t138) + (uint64_t)(t139)))));
  t141 = ((uint64_t)(v14));
  t142 = ((uint64_t)(v14));
  t143 = ((uint64_t)(((uint64_t)((uint64_t)(t141) * (uint64_t)(t142)))));
  v44 = ((uint64_t)(((uint64_t)((uint64_t)(t140) * (uint64_t)(t143)))));
  t144 = ((uint64_t)(((uint64_t)(v30))));
  t145 = ((uint64_t)(v16));
  t146 = ((uint64_t)(((uint64_t)((uint64_t)(t144) * (uint64_t)(t145)))));
  t147 = ((uint64_t)(v15));
  t148 = ((uint64_t)(((uint64_t)((uint64_t)(t146) * (uint64_t)(t147)))));
  t149 = ((uint64_t)(((uint64_t)(v29))));
  t150 = ((uint64_t)(((uint64_t)((uint64_t)(t148) + (uint64_t)(t149)))));
  t151 = ((uint64_t)(v14));
  v45 = ((uint64_t)(((uint64_t)((uint64_t)(t150) * (uint64_t)(t151)))));
  t152 = ((uint64_t)(v11));
  t153 = ((uint64_t)(v44));
  t154 = ((uint64_t)(v28));
  t155 = ((uint64_t)(v14));
  t156 = ((uint64_t)(((uint64_t)((uint64_t)(t154) * (uint64_t)(t155)))));
  t157 = ((uint64_t)(((uint64_t)((uint64_t)(t153) + (uint64_t)(t156)))));
  t158 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t159 = ((uint64_t)(((uint64_t)((uint64_t)(t157) * (uint64_t)(t158)))));
  v46 = ((uint64_t)(((uint64_t)((uint64_t)(t152) + (uint64_t)(t159)))));
  t160 = ((uint64_t)(v13));
  t161 = ((uint64_t)(v44));
  t162 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t163 = ((uint64_t)(((uint64_t)((uint64_t)(t161) * (uint64_t)(t162)))));
  v47 = ((uint64_t)(((uint64_t)((uint64_t)(t160) + (uint64_t)(t163)))));
  t164 = ((uint64_t)(v12));
  t165 = ((uint64_t)(v45));
  t166 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t167 = ((uint64_t)(((uint64_t)((uint64_t)(t165) * (uint64_t)(t166)))));
  v48 = ((uint64_t)(((uint64_t)((uint64_t)(t164) + (uint64_t)(t167)))));
  memset(&t168, 0, sizeof(t168));
  t169 = ((float)(t168));
  for (t170=0; t170<32; t170++) (v49).data[t170] = t169;
  fg174s_sync();
  v50 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t171 = ((int32_t)(v50));
  t172 = ((int32_t)(v24));
  if (!(((t171) < (t172)))) goto label2;
  {
  t173 = ((int32_t)(v50));
  t174 = ((int32_t)(v23));
  t175 = ((int32_t)(((int32_t)((uint64_t)(t173) * (uint64_t)(t174)))));
  t176 = ((int32_t)(v27));
  v51 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t175) + (uint64_t)(t176)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t177 = v50;
  t178 = ((float *)(v49).data) + gg_index(t177, ((int64_t)UINT64_C(32)));
  t179 = ((uint64_t)(v46));
  t180 = ((uint64_t)(v51));
  t181 = fg174s_load(t179, t180);
  (*t178) = ((float)(t181));
  t182 = v50;
  t183 = ((int32_t)(((int64_t)UINT64_C(1))));
  v50 = ((int32_t)((uint64_t)(t182) + (uint64_t)(t183)));
  }
  goto label1;
label2:;
  v52 = ((uint64_t)(((int64_t)UINT64_C(0))));
label4:;
  t184 = ((uint64_t)(v52));
  t185 = ((uint64_t)(v16));
  if (!(((t184) < (t185)))) goto label5;
  {
  t186 = ((uint64_t)(v32));
  t187 = ((uint64_t)(v35));
  t188 = ((uint64_t)(((uint64_t)((uint64_t)(t186) * (uint64_t)(t187)))));
  t189 = ((uint64_t)(v52));
  t190 = ((uint64_t)(v34));
  t191 = ((uint64_t)(((uint64_t)((uint64_t)(t189) * (uint64_t)(t190)))));
  t192 = ((uint64_t)(((uint64_t)((uint64_t)(t188) + (uint64_t)(t191)))));
  t193 = ((uint64_t)(v31));
  t194 = ((uint64_t)(v33));
  t195 = ((uint64_t)(((uint64_t)((uint64_t)(t193) * (uint64_t)(t194)))));
  v53 = ((uint64_t)(((uint64_t)((uint64_t)(t192) + (uint64_t)(t195)))));
  t196 = ((uint64_t)(v6));
  t197 = ((uint64_t)(v53));
  t198 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t199 = ((uint64_t)(((uint64_t)((uint64_t)(t197) * (uint64_t)(t198)))));
  v54 = ((uint64_t)(((uint64_t)((uint64_t)(t196) + (uint64_t)(t199)))));
  t200 = ((uint64_t)(v7));
  t201 = ((uint64_t)(v53));
  t202 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t203 = ((uint64_t)(((uint64_t)((uint64_t)(t201) * (uint64_t)(t202)))));
  v55 = ((uint64_t)(((uint64_t)((uint64_t)(t200) + (uint64_t)(t203)))));
  t204 = ((uint64_t)(v8));
  t205 = ((uint64_t)(((uint64_t)(v30))));
  t206 = ((uint64_t)(v38));
  t207 = ((uint64_t)(((uint64_t)((uint64_t)(t205) * (uint64_t)(t206)))));
  t208 = ((uint64_t)(v52));
  t209 = ((uint64_t)(v37));
  t210 = ((uint64_t)(((uint64_t)((uint64_t)(t208) * (uint64_t)(t209)))));
  t211 = ((uint64_t)(((uint64_t)((uint64_t)(t207) + (uint64_t)(t210)))));
  t212 = ((uint64_t)(((uint64_t)(v29))));
  t213 = ((uint64_t)(v36));
  t214 = ((uint64_t)(((uint64_t)((uint64_t)(t212) * (uint64_t)(t213)))));
  t215 = ((uint64_t)(((uint64_t)((uint64_t)(t211) + (uint64_t)(t214)))));
  t216 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t217 = ((uint64_t)(((uint64_t)((uint64_t)(t215) * (uint64_t)(t216)))));
  v56 = ((uint64_t)(((uint64_t)((uint64_t)(t204) + (uint64_t)(t217)))));
  t218 = ((uint64_t)(((uint64_t)(v30))));
  t219 = ((uint64_t)(v41));
  t220 = ((uint64_t)(((uint64_t)((uint64_t)(t218) * (uint64_t)(t219)))));
  t221 = ((uint64_t)(v52));
  t222 = ((uint64_t)(v40));
  t223 = ((uint64_t)(((uint64_t)((uint64_t)(t221) * (uint64_t)(t222)))));
  t224 = ((uint64_t)(((uint64_t)((uint64_t)(t220) + (uint64_t)(t223)))));
  t225 = ((uint64_t)(((uint64_t)(v29))));
  t226 = ((uint64_t)(v39));
  t227 = ((uint64_t)(((uint64_t)((uint64_t)(t225) * (uint64_t)(t226)))));
  v57 = ((uint64_t)(((uint64_t)((uint64_t)(t224) + (uint64_t)(t227)))));
  t228 = ((uint64_t)(v10));
  t229 = ((uint64_t)(v57));
  t230 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t231 = ((uint64_t)(((uint64_t)((uint64_t)(t229) * (uint64_t)(t230)))));
  v58 = ((uint64_t)(((uint64_t)((uint64_t)(t228) + (uint64_t)(t231)))));
  t232 = ((uint64_t)(v58));
  t233 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t234 = fg174s_load(t232, t233);
  v59 = ((float)(t234));
  memset(&t235, 0, sizeof(t235));
  t236 = ((float)(t235));
  for (t237=0; t237<32; t237++) (v60).data[t237] = t236;
  memset(&t238, 0, sizeof(t238));
  t239 = ((float)(t238));
  for (t240=0; t240<32; t240++) (v61).data[t240] = t239;
  v50 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t241 = ((int32_t)(v50));
  t242 = ((int32_t)(v24));
  if (!(((t241) < (t242)))) goto label8;
  {
  t243 = ((int32_t)(v50));
  t244 = ((int32_t)(v23));
  t245 = ((int32_t)(((int32_t)((uint64_t)(t243) * (uint64_t)(t244)))));
  t246 = ((int32_t)(v27));
  v62 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t245) + (uint64_t)(t246)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t247 = v50;
  t248 = ((float *)(v60).data) + gg_index(t247, ((int64_t)UINT64_C(32)));
  t249 = ((uint64_t)(v55));
  t250 = ((uint64_t)(v62));
  t251 = fg174s_load(t249, t250);
  (*t248) = ((float)(t251));
  /* FG_DIRECT_SLICE_INDEX */
  t252 = v50;
  t253 = ((float *)(v61).data) + gg_index(t252, ((int64_t)UINT64_C(32)));
  t254 = ((uint64_t)(v54));
  t255 = ((uint64_t)(v62));
  t256 = fg174s_load(t254, t255);
  (*t253) = ((float)(t256));
  t257 = v50;
  t258 = ((int32_t)(((int64_t)UINT64_C(1))));
  v50 = ((int32_t)((uint64_t)(t257) + (uint64_t)(t258)));
  }
  goto label7;
label8:;
  t259 = v18;
  t260 = (!(t259));
  if (t260) {
  {
  t261 = ((uint64_t)(v9));
  t262 = ((uint64_t)(v57));
  t263 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t264 = ((uint64_t)(((uint64_t)((uint64_t)(t262) * (uint64_t)(t263)))));
  t265 = ((uint64_t)(((uint64_t)((uint64_t)(t261) + (uint64_t)(t264)))));
  t266 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t267 = fg174s_load(t265, t266);
  t268 = ((float)(t267));
  t269 = fg174s_expf(t268);
  v63 = ((float)(t269));
  v64 = ((float)(gg_double_bits(UINT64_C(0))));
  v50 = ((int32_t)(((int64_t)UINT64_C(0))));
label10:;
  t270 = ((int32_t)(v50));
  t271 = ((int32_t)(v24));
  if (!(((t270) < (t271)))) goto label11;
  {
  t272 = v64;
  /* FG_DIRECT_SLICE_INDEX */
  t273 = v50;
  t274 = ((float *)(v49).data) + gg_index(t273, ((int64_t)UINT64_C(32)));
  t275 = ((float)((*t274)));
  /* FG_DIRECT_SLICE_INDEX */
  t276 = v50;
  t277 = ((float *)(v60).data) + gg_index(t276, ((int64_t)UINT64_C(32)));
  t278 = ((float)((*t277)));
  t279 = ((float)(((t275) * (t278))));
  v64 = ((t272) + (t279));
  t280 = v50;
  t281 = ((int32_t)(((int64_t)UINT64_C(1))));
  v50 = ((int32_t)((uint64_t)(t280) + (uint64_t)(t281)));
  }
  goto label10;
label11:;
  t282 = ((float)(v64));
  t283 = ((int32_t)(v23));
  v65 = gg_function_11(t282, t283, base + 0);
  t284 = ((uint64_t)(v56));
  t285 = ((uint64_t)(v28));
  t286 = fg174s_load(t284, t285);
  t287 = ((float)(t286));
  t288 = ((float)(v63));
  t289 = ((float)(v65));
  t290 = ((float)(((t288) * (t289))));
  t291 = ((float)(((t287) - (t290))));
  t292 = ((float)(v59));
  v66 = ((float)(((t291) * (t292))));
  v67 = ((float)(gg_double_bits(UINT64_C(0))));
  v50 = ((int32_t)(((int64_t)UINT64_C(0))));
label13:;
  t293 = ((int32_t)(v50));
  t294 = ((int32_t)(v24));
  if (!(((t293) < (t294)))) goto label14;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t295 = v50;
  t296 = ((float *)(v49).data) + gg_index(t295, ((int64_t)UINT64_C(32)));
  t297 = ((float)(v63));
  /* FG_DIRECT_SLICE_INDEX */
  t298 = v50;
  t299 = ((float *)(v49).data) + gg_index(t298, ((int64_t)UINT64_C(32)));
  t300 = ((float)((*t299)));
  /* FG_DIRECT_SLICE_INDEX */
  t301 = v50;
  t302 = ((float *)(v60).data) + gg_index(t301, ((int64_t)UINT64_C(32)));
  t303 = ((float)((*t302)));
  t304 = ((float)(v66));
  t305 = fg174s_mul_rn(t303, t304);
  t306 = ((float)(t305));
  t307 = fg174s_fma(t297, t300, t306);
  (*t296) = ((float)(t307));
  t308 = v67;
  /* FG_DIRECT_SLICE_INDEX */
  t309 = v50;
  t310 = ((float *)(v49).data) + gg_index(t309, ((int64_t)UINT64_C(32)));
  t311 = ((float)((*t310)));
  /* FG_DIRECT_SLICE_INDEX */
  t312 = v50;
  t313 = ((float *)(v61).data) + gg_index(t312, ((int64_t)UINT64_C(32)));
  t314 = ((float)((*t313)));
  t315 = ((float)(((t311) * (t314))));
  v67 = ((t308) + (t315));
  t316 = v50;
  t317 = ((int32_t)(((int64_t)UINT64_C(1))));
  v50 = ((int32_t)((uint64_t)(t316) + (uint64_t)(t317)));
  }
  goto label13;
label14:;
  t318 = ((float)(v67));
  t319 = ((int32_t)(v23));
  v68 = gg_function_11(t318, t319, base + 0);
  t320 = ((int32_t)(v27));
  t321 = ((int32_t)(((int64_t)UINT64_C(0))));
  t322 = ((t320) == (t321));
  if (t322) {
  {
  t323 = ((uint64_t)(v48));
  t324 = ((uint64_t)(v28));
  t325 = ((float)(v68));
  t326 = ((float)(v43));
  t327 = ((float)(((t325) * (t326))));
  fg174s_store(t323, t324, t327);
  }
  }
  }
  } else {
  {
  t328 = ((uint64_t)(v9));
  t329 = ((uint64_t)(v57));
  t330 = ((uint64_t)(v14));
  t331 = ((uint64_t)(((uint64_t)((uint64_t)(t329) * (uint64_t)(t330)))));
  t332 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t333 = ((uint64_t)(((uint64_t)((uint64_t)(t331) * (uint64_t)(t332)))));
  v69 = ((uint64_t)(((uint64_t)((uint64_t)(t328) + (uint64_t)(t333)))));
  v70 = ((float)(gg_double_bits(UINT64_C(0))));
  v50 = ((int32_t)(((int64_t)UINT64_C(0))));
label16:;
  t334 = ((int32_t)(v50));
  t335 = ((int32_t)(v24));
  if (!(((t334) < (t335)))) goto label17;
  {
  t336 = ((int32_t)(v50));
  t337 = ((int32_t)(v23));
  t338 = ((int32_t)(((int32_t)((uint64_t)(t336) * (uint64_t)(t337)))));
  t339 = ((int32_t)(v27));
  v71 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t338) + (uint64_t)(t339)))))));
  t340 = v70;
  t341 = ((uint64_t)(v69));
  t342 = ((uint64_t)(v71));
  t343 = fg174s_load(t341, t342);
  t344 = ((float)(t343));
  t345 = fg174s_expf(t344);
  t346 = ((float)(t345));
  /* FG_DIRECT_SLICE_INDEX */
  t347 = v50;
  t348 = ((float *)(v49).data) + gg_index(t347, ((int64_t)UINT64_C(32)));
  t349 = ((float)((*t348)));
  t350 = ((float)(((t346) * (t349))));
  /* FG_DIRECT_SLICE_INDEX */
  t351 = v50;
  t352 = ((float *)(v60).data) + gg_index(t351, ((int64_t)UINT64_C(32)));
  t353 = ((float)((*t352)));
  t354 = ((float)(((t350) * (t353))));
  v70 = ((t340) + (t354));
  t355 = v50;
  t356 = ((int32_t)(((int64_t)UINT64_C(1))));
  v50 = ((int32_t)((uint64_t)(t355) + (uint64_t)(t356)));
  }
  goto label16;
label17:;
  t357 = ((float)(v70));
  t358 = ((int32_t)(v23));
  v72 = gg_function_11(t357, t358, base + 0);
  t359 = ((uint64_t)(v56));
  t360 = ((uint64_t)(v28));
  t361 = fg174s_load(t359, t360);
  t362 = ((float)(t361));
  t363 = ((float)(v72));
  t364 = ((float)(((t362) - (t363))));
  t365 = ((float)(v59));
  v73 = ((float)(((t364) * (t365))));
  v74 = ((float)(gg_double_bits(UINT64_C(0))));
  v50 = ((int32_t)(((int64_t)UINT64_C(0))));
label19:;
  t366 = ((int32_t)(v50));
  t367 = ((int32_t)(v24));
  if (!(((t366) < (t367)))) goto label20;
  {
  t368 = ((int32_t)(v50));
  t369 = ((int32_t)(v23));
  t370 = ((int32_t)(((int32_t)((uint64_t)(t368) * (uint64_t)(t369)))));
  t371 = ((int32_t)(v27));
  v75 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t370) + (uint64_t)(t371)))))));
  /* FG_DIRECT_SLICE_INDEX */
  t372 = v50;
  t373 = ((float *)(v49).data) + gg_index(t372, ((int64_t)UINT64_C(32)));
  t374 = ((uint64_t)(v69));
  t375 = ((uint64_t)(v75));
  t376 = fg174s_load(t374, t375);
  t377 = ((float)(t376));
  t378 = fg174s_expf(t377);
  t379 = ((float)(t378));
  /* FG_DIRECT_SLICE_INDEX */
  t380 = v50;
  t381 = ((float *)(v49).data) + gg_index(t380, ((int64_t)UINT64_C(32)));
  t382 = ((float)((*t381)));
  /* FG_DIRECT_SLICE_INDEX */
  t383 = v50;
  t384 = ((float *)(v60).data) + gg_index(t383, ((int64_t)UINT64_C(32)));
  t385 = ((float)((*t384)));
  t386 = ((float)(v73));
  t387 = fg174s_mul_rn(t385, t386);
  t388 = ((float)(t387));
  t389 = fg174s_fma(t379, t382, t388);
  (*t373) = ((float)(t389));
  t390 = v74;
  /* FG_DIRECT_SLICE_INDEX */
  t391 = v50;
  t392 = ((float *)(v49).data) + gg_index(t391, ((int64_t)UINT64_C(32)));
  t393 = ((float)((*t392)));
  /* FG_DIRECT_SLICE_INDEX */
  t394 = v50;
  t395 = ((float *)(v61).data) + gg_index(t394, ((int64_t)UINT64_C(32)));
  t396 = ((float)((*t395)));
  t397 = ((float)(((t393) * (t396))));
  v74 = ((t390) + (t397));
  t398 = v50;
  t399 = ((int32_t)(((int64_t)UINT64_C(1))));
  v50 = ((int32_t)((uint64_t)(t398) + (uint64_t)(t399)));
  }
  goto label19;
label20:;
  t400 = ((float)(v74));
  t401 = ((int32_t)(v23));
  v76 = gg_function_11(t400, t401, base + 0);
  t402 = ((int32_t)(v27));
  t403 = ((int32_t)(((int64_t)UINT64_C(0))));
  t404 = ((t402) == (t403));
  if (t404) {
  {
  t405 = ((uint64_t)(v48));
  t406 = ((uint64_t)(v28));
  t407 = ((float)(v76));
  t408 = ((float)(v43));
  t409 = ((float)(((t407) * (t408))));
  fg174s_store(t405, t406, t409);
  }
  }
  }
  }
  t410 = v48;
  t411 = ((uint64_t)(v14));
  t412 = ((uint64_t)(v15));
  t413 = ((uint64_t)(((uint64_t)((uint64_t)(t411) * (uint64_t)(t412)))));
  t414 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t415 = ((uint64_t)(((uint64_t)((uint64_t)(t413) * (uint64_t)(t414)))));
  v48 = ((uint64_t)((uint64_t)(t410) + (uint64_t)(t415)));
  t416 = v17;
  if (t416) {
  {
  t417 = ((uint64_t)(v16));
  t418 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t419 = ((uint64_t)(((uint64_t)((uint64_t)(t417) - (uint64_t)(t418)))));
  t420 = ((uint64_t)(v52));
  v77 = ((uint64_t)(((uint64_t)((uint64_t)(t419) - (uint64_t)(t420)))));
  t421 = ((uint64_t)(v77));
  t422 = ((int64_t)(v0));
  t423 = ((int64_t)(((int64_t)UINT64_C(3))));
  t424 = ((int64_t)(((int64_t)UINT64_C(6))));
  t425 = fg174s_dget(t422, t423, t424);
  t426 = ((uint64_t)(t425));
  t427 = ((t421) < (t426));
  if (t427) {
  {
  t428 = ((uint64_t)(v47));
  t429 = ((uint64_t)(v77));
  t430 = ((uint64_t)(v42));
  t431 = ((uint64_t)(((uint64_t)((uint64_t)(t429) * (uint64_t)(t430)))));
  t432 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t433 = ((uint64_t)(((uint64_t)((uint64_t)(t431) * (uint64_t)(t432)))));
  v78 = ((uint64_t)(((uint64_t)((uint64_t)(t428) + (uint64_t)(t433)))));
  v50 = ((int32_t)(((int64_t)UINT64_C(0))));
label22:;
  t434 = ((int32_t)(v50));
  t435 = ((int32_t)(v24));
  if (!(((t434) < (t435)))) goto label23;
  {
  t436 = ((int32_t)(v50));
  t437 = ((int32_t)(v23));
  t438 = ((int32_t)(((int32_t)((uint64_t)(t436) * (uint64_t)(t437)))));
  t439 = ((int32_t)(v27));
  v79 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t438) + (uint64_t)(t439)))))));
  t440 = ((uint64_t)(v78));
  t441 = ((uint64_t)(v28));
  t442 = ((uint64_t)(v14));
  t443 = ((uint64_t)(((uint64_t)((uint64_t)(t441) * (uint64_t)(t442)))));
  t444 = ((uint64_t)(v79));
  t445 = ((uint64_t)(((uint64_t)((uint64_t)(t443) + (uint64_t)(t444)))));
  /* FG_DIRECT_SLICE_INDEX */
  t446 = v50;
  t447 = ((float *)(v49).data) + gg_index(t446, ((int64_t)UINT64_C(32)));
  t448 = ((float)((*t447)));
  fg174s_store(t440, t445, t448);
  t449 = v50;
  t450 = ((int32_t)(((int64_t)UINT64_C(1))));
  v50 = ((int32_t)((uint64_t)(t449) + (uint64_t)(t450)));
  }
  goto label22;
label23:;
  }
  }
  }
  }
  t451 = v52;
  t452 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v52 = ((uint64_t)((uint64_t)(t451) + (uint64_t)(t452)));
  }
  goto label4;
label5:;
  t453 = v17;
  t454 = (!(t453));
  if (t454) {
  {
  v50 = ((int32_t)(((int64_t)UINT64_C(0))));
label25:;
  t455 = ((int32_t)(v50));
  t456 = ((int32_t)(v24));
  if (!(((t455) < (t456)))) goto label26;
  {
  t457 = ((int32_t)(v50));
  t458 = ((int32_t)(v23));
  t459 = ((int32_t)(((int32_t)((uint64_t)(t457) * (uint64_t)(t458)))));
  t460 = ((int32_t)(v27));
  v80 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t459) + (uint64_t)(t460)))))));
  t461 = ((uint64_t)(v47));
  t462 = ((uint64_t)(v28));
  t463 = ((uint64_t)(v14));
  t464 = ((uint64_t)(((uint64_t)((uint64_t)(t462) * (uint64_t)(t463)))));
  t465 = ((uint64_t)(v80));
  t466 = ((uint64_t)(((uint64_t)((uint64_t)(t464) + (uint64_t)(t465)))));
  /* FG_DIRECT_SLICE_INDEX */
  t467 = v50;
  t468 = ((float *)(v49).data) + gg_index(t467, ((int64_t)UINT64_C(32)));
  t469 = ((float)((*t468)));
  fg174s_store(t461, t466, t469);
  t470 = v50;
  t471 = ((int32_t)(((int64_t)UINT64_C(1))));
  v50 = ((int32_t)((uint64_t)(t470) + (uint64_t)(t471)));
  }
  goto label25;
label26:;
  }
  }
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  int64_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int32_t)(((int64_t)UINT64_C(0))));
  t2 = ((int32_t)(((int64_t)UINT64_C(0))));
  t3 = ((int32_t)(((int64_t)UINT64_C(0))));
  t4 = ((int32_t)(((int64_t)UINT64_C(0))));
  t5 = ((int32_t)(((int64_t)UINT64_C(0))));
  gg_function_1(t0, t1, t2, t3, t4, t5, base + 0);
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
