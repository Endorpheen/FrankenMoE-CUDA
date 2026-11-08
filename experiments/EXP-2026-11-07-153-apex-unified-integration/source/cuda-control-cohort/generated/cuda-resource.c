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
static uint8_t gg_eq_1(int64_t a, int64_t b);
static uint8_t gg_equal_elements_1(gg_slice a, gg_slice b);
static void gg_render_1(gg_reference out, int64_t value);
static uint8_t gg_eq_2(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_2(gg_slice a, gg_slice b);
static void gg_render_2(gg_reference out, uint8_t value);
static uint8_t gg_eq_4(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint8_t value);
static uint8_t gg_equal_elements_1(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void gg_render_1(gg_reference out, int64_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_2(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_2(gg_reference out, uint8_t value) {
gg_bool(out,value);
}
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t base);
static int64_t gg_function_10(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_13(int64_t p0, int64_t base);
static int64_t gg_function_16(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_18(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_21(int64_t p0, int64_t base);
static int64_t gg_function_27(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_28(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_34(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_36(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_38(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_45(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_48(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_50(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_52(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_54(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_57(int64_t p0, int64_t base);
static int64_t gg_function_58(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_60(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_61(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_62(int64_t p0, int64_t base);
static int64_t gg_function_67(int64_t p0, int64_t base);
static int64_t gg_function_68(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t gg_function_69(int64_t p0, int64_t base);
static int64_t gg_function_70(int64_t p0, int64_t base);
static int64_t gg_function_18(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = fg14_s_frame_get(t1, t2);
  t0 = ((int64_t)(t3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_16(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = fg14_s_vec_data(t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) == (t7));
  if (t8) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(v1));
  t0 = gg_function_18(t9, t10, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_70(int64_t p0, int64_t base) {
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
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(13))));
  t3 = fg14_s_record_get(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(9))));
  t6 = fg14_s_record_get(t4, t5);
  v2 = ((int64_t)(t6));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(v1));
  t9 = fg14_s_vec_len(t8);
  t10 = ((int64_t)(t9));
  if (!(((t7) < (t10)))) goto label2;
  {
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v3));
  t14 = gg_function_16(t12, t13, base + 0);
  t15 = fg14_s_map_get(t11, t14);
  v4 = ((int64_t)(t15));
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)UINT64_C(2))));
  t22 = fg14_s_record_get(t20, t21);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t23) != (t24));
  }
  t25 = t19;
  if (t25) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t26 = v3;
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t26) + (uint64_t)(t27)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_69(int64_t p0, int64_t base) {
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
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(13))));
  t3 = fg14_s_record_get(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(9))));
  t6 = fg14_s_record_get(t4, t5);
  v2 = ((int64_t)(t6));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(v1));
  t9 = fg14_s_vec_len(t8);
  t10 = ((int64_t)(t9));
  if (!(((t7) < (t10)))) goto label2;
  {
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v3));
  t14 = gg_function_16(t12, t13, base + 0);
  t15 = fg14_s_map_get(t11, t14);
  v4 = ((int64_t)(t15));
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)UINT64_C(4))));
  t22 = fg14_s_record_get(t20, t21);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t23) == (t24));
  }
  t25 = t19;
  if (t25) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
  }
  t26 = v3;
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t26) + (uint64_t)(t27)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_36(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = fg14_s_frame_set(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_34(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
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
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t4)))));
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v0));
  t6 = fg14_s_vec_len(t5);
  v2 = ((int64_t)(t6));
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = fg14_s_vec_resize(t7, t10);
  v3 = ((int64_t)(t11));
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(v0));
  t16 = fg14_s_vec_data(t15);
  v4 = ((int64_t)(t16));
  t17 = ((int64_t)(v4));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(v1));
  t20 = gg_function_36(t17, t18, t19, base + 0);
  (void)(t20);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_68(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(10))));
  t3 = fg14_s_record_get(t1, t2);
  v3 = ((int64_t)(t3));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v1));
  t6 = fg14_s_map_get(t4, t5);
  v4 = ((int64_t)(t6));
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(v2));
  t10 = fg14_s_map_put(t7, t8, t9);
  v5 = ((int64_t)(t10));
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) < (t12));
  if (t13) {
  {
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
  }
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) < (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(11))));
  t19 = fg14_s_record_get(t17, t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(v1));
  t22 = gg_function_34(t20, t21, base + 0);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) < (t24));
  if (t25) {
  {
  t26 = ((int64_t)(v3));
  t27 = ((int64_t)(v1));
  t28 = fg14_s_map_erase(t26, t27);
  (void)(t28);
  t29 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t29)))));
  goto gg_done;
  }
  }
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_21(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
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
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t4 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t4)))));
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(4))));
  t7 = fg14_s_record_get(t5, t6);
  v1 = ((int64_t)(t7));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(6))));
  t10 = fg14_s_record_get(t8, t9);
  v2 = ((int64_t)(t10));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(7))));
  t13 = fg14_s_record_get(t11, t12);
  v3 = ((int64_t)(t13));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
  t16 = fg14_s_record_get(t14, t15);
  v4 = ((int64_t)(t16));
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)(v2));
  t19 = fg14_s_vec_len(t18);
  t20 = ((int64_t)(t19));
  if (!(((t17) < (t20)))) goto label2;
  {
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(v5));
  v6 = gg_function_16(t21, t22, base + 0);
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) != (t24));
  if (t25) {
  {
  t26 = ((int64_t)(v6));
  t27 = fg14_s_event_destroy(t26);
  (void)(t27);
  }
  }
  t28 = v5;
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)((uint64_t)(t28) + (uint64_t)(t29)));
  }
  goto label1;
label2:;
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t32 = fg14_s_record_get(t30, t31);
  v7 = ((int64_t)(t32));
  t33 = ((int64_t)(v7));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) != (t34));
  if (t35) {
  {
  t36 = ((int64_t)(v7));
  t37 = fg14_s_event_destroy(t36);
  (void)(t37);
  }
  }
  t38 = ((int64_t)(v4));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) != (t39));
  if (t40) {
  {
  t41 = ((int64_t)(v4));
  t42 = fg14_s_map_free(t41);
  (void)(t42);
  }
  }
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(((int64_t)UINT64_C(0))));
  t45 = ((t43) != (t44));
  if (t45) {
  {
  t46 = ((int64_t)(v1));
  t47 = fg14_s_vec_free(t46);
  (void)(t47);
  }
  }
  t48 = ((int64_t)(v2));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((t48) != (t49));
  if (t50) {
  {
  t51 = ((int64_t)(v2));
  t52 = fg14_s_vec_free(t51);
  (void)(t52);
  }
  }
  t53 = ((int64_t)(v3));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((t53) != (t54));
  if (t55) {
  {
  t56 = ((int64_t)(v3));
  t57 = fg14_s_vec_free(t56);
  (void)(t57);
  }
  }
  t58 = ((int64_t)(v0));
  t59 = fg14_s_record_free(t58);
  (void)(t59);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_67(int64_t p0, int64_t base) {
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
  uint8_t t18;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(11))));
  t3 = fg14_s_record_get(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(10))));
  t6 = fg14_s_record_get(t4, t5);
  v2 = ((int64_t)(t6));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(v1));
  t9 = fg14_s_vec_len(t8);
  t10 = ((int64_t)(t9));
  if (!(((t7) < (t10)))) goto label2;
  {
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(v3));
  v4 = gg_function_16(t11, t12, base + 0);
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(v4));
  t15 = fg14_s_map_get(t13, t14);
  v5 = ((int64_t)(t15));
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) > (t17));
  if (t18) {
  {
  t19 = ((int64_t)(v5));
  t20 = gg_function_21(t19, base + 0);
  (void)(t20);
  }
  }
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(v4));
  t23 = fg14_s_map_erase(t21, t22);
  (void)(t23);
  t24 = v3;
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t24) + (uint64_t)(t25)));
  }
  goto label1;
label2:;
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = fg14_s_vec_resize(t26, t27);
  t0 = ((int64_t)(t28));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_60(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = fg14_s_record_get(t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(v1));
  t6 = fg14_s_map_get(t4, t5);
  v2 = ((int64_t)(t6));
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_62(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
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
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
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
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  uint8_t t56;
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
  uint8_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint8_t t76;
  uint8_t t77;
  int64_t t78;
  int64_t t79;
  uint8_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  uint8_t t84;
  uint8_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
  uint8_t t90;
  int64_t t91;
  int64_t t92;
  uint8_t t93;
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  uint8_t t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint8_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  uint8_t t117;
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t v0;
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
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(5))));
  t3 = fg14_s_record_get(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v1));
  t8 = fg14_s_t_view_src(t7);
  v2 = ((int64_t)(t8));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  v2 = ((int64_t)(v1));
  }
  }
  t12 = ((int64_t)(v2));
  t13 = fg14_s_t_data(t12);
  v3 = ((int64_t)(t13));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(v2));
  t16 = fg14_s_t_nbytes(t15);
  t17 = ((int64_t)(t16));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t17)))));
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(7))));
  t20 = fg14_s_record_get(t18, t19);
  v5 = ((int64_t)(t20));
  t21 = ((int64_t)(v5));
  t22 = fg14_s_vec_len(t21);
  v6 = ((int64_t)(t22));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t23 = ((int64_t)(v7));
  t24 = ((int64_t)(v6));
  if (!(((t23) < (t24)))) goto label2;
  {
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(v7));
  v8 = gg_function_16(t25, t26, base + 0);
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(v8));
  v9 = gg_function_60(t27, t28, base + 0);
  t29 = ((int64_t)(v8));
  t30 = fg14_s_t_view_src(t29);
  v10 = ((int64_t)(t30));
  t31 = ((int64_t)(v10));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) == (t32));
  if (t33) {
  {
  v10 = ((int64_t)(v8));
  }
  }
  t34 = ((int64_t)(v10));
  t35 = fg14_s_t_data(t34);
  v11 = ((int64_t)(t35));
  t36 = ((int64_t)(v11));
  t37 = ((int64_t)(v10));
  t38 = fg14_s_t_nbytes(t37);
  t39 = ((int64_t)(t38));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t39)))));
  t40 = ((int64_t)(v11));
  t41 = ((int64_t)(v3));
  t42 = ((t40) <= (t41));
  t43 = t42;
  if (t43) {
  t44 = ((int64_t)(v3));
  t45 = ((int64_t)(v12));
  t43 = ((t44) < (t45));
  }
  t46 = t43;
  t47 = (!(t46));
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(v3));
  t50 = ((int64_t)(v11));
  t51 = ((t49) <= (t50));
  t52 = t51;
  if (t52) {
  t53 = ((int64_t)(v11));
  t54 = ((int64_t)(v4));
  t52 = ((t53) < (t54));
  }
  t55 = t52;
  t48 = (!(t55));
  }
  t56 = t48;
  if (t56) {
  {
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t57 = ((int64_t)(v13));
  t58 = ((int64_t)(v6));
  if (!(((t57) < (t58)))) goto label5;
  {
  t59 = ((int64_t)(v5));
  t60 = ((int64_t)(v13));
  v14 = gg_function_16(t59, t60, base + 0);
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(v14));
  v15 = gg_function_60(t61, t62, base + 0);
  t63 = ((int64_t)(v14));
  t64 = fg14_s_t_view_src(t63);
  v16 = ((int64_t)(t64));
  t65 = ((int64_t)(v16));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((t65) == (t66));
  if (t67) {
  {
  v16 = ((int64_t)(v14));
  }
  }
  t68 = ((int64_t)(v16));
  t69 = fg14_s_t_data(t68);
  v17 = ((int64_t)(t69));
  t70 = ((int64_t)(v17));
  t71 = ((int64_t)(v16));
  t72 = fg14_s_t_nbytes(t71);
  t73 = ((int64_t)(t72));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t73)))));
  t74 = ((int64_t)(v9));
  t75 = ((int64_t)(v15));
  t76 = ((t74) != (t75));
  t77 = t76;
  if (t77) {
  t78 = ((int64_t)(v17));
  t79 = ((int64_t)(v3));
  t80 = ((t78) <= (t79));
  t81 = t80;
  if (t81) {
  t82 = ((int64_t)(v3));
  t83 = ((int64_t)(v18));
  t81 = ((t82) < (t83));
  }
  t84 = t81;
  t77 = (!(t84));
  }
  t85 = t77;
  t86 = t85;
  if (t86) {
  t87 = ((int64_t)(v3));
  t88 = ((int64_t)(v17));
  t89 = ((t87) <= (t88));
  t90 = t89;
  if (t90) {
  t91 = ((int64_t)(v17));
  t92 = ((int64_t)(v4));
  t90 = ((t91) < (t92));
  }
  t93 = t90;
  t86 = (!(t93));
  }
  t94 = t86;
  if (t94) {
  {
  t95 = ((int64_t)(v17));
  t96 = ((int64_t)(v12));
  t97 = ((t95) >= (t96));
  t98 = t97;
  if (!t98) {
  t99 = ((int64_t)(v18));
  t100 = ((int64_t)(v11));
  t98 = ((t99) <= (t100));
  }
  t101 = t98;
  t102 = (!(t101));
  if (t102) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  t103 = v13;
  t104 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)((uint64_t)(t103) + (uint64_t)(t104)));
  }
  goto label4;
label5:;
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t105 = ((int64_t)(v19));
  t106 = ((int64_t)(((int64_t)UINT64_C(10))));
  if (!(((t105) < (t106)))) goto label8;
  {
  t107 = ((int64_t)(v8));
  t108 = ((int64_t)(v19));
  t109 = fg14_s_t_src(t107, t108);
  v20 = ((int64_t)(t109));
  t110 = ((int64_t)(v20));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = ((t110) != (t111));
  if (t112) {
  {
  t113 = ((int64_t)(v0));
  t114 = ((int64_t)(v20));
  v21 = gg_function_60(t113, t114, base + 0);
  t115 = ((int64_t)(v21));
  t116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t117 = ((t115) >= (t116));
  t118 = t117;
  if (t118) {
  t119 = ((int64_t)(v21));
  t120 = ((int64_t)(v9));
  t118 = ((t119) != (t120));
  }
  t121 = t118;
  if (t121) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  }
  }
  t122 = v19;
  t123 = ((int64_t)(((int64_t)UINT64_C(1))));
  v19 = ((int64_t)((uint64_t)(t122) + (uint64_t)(t123)));
  }
  goto label7;
label8:;
  }
  }
  t124 = v7;
  t125 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t124) + (uint64_t)(t125)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_61(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = fg14_s_record_get(t1, t2);
  v3 = ((int64_t)(t3));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v1));
  t6 = fg14_s_map_get(t4, t5);
  v4 = ((int64_t)(t6));
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(v2));
  t10 = fg14_s_map_put(t7, t8, t9);
  v5 = ((int64_t)(t10));
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) < (t12));
  if (t13) {
  {
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
  }
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) < (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(7))));
  t19 = fg14_s_record_get(t17, t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(v1));
  t22 = gg_function_34(t20, t21, base + 0);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) < (t24));
  if (t25) {
  {
  t26 = ((int64_t)(v3));
  t27 = ((int64_t)(v1));
  t28 = fg14_s_map_erase(t26, t27);
  (void)(t28);
  t29 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t29)))));
  goto gg_done;
  }
  }
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_58(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t13;
  uint8_t t14;
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
  uint8_t t32;
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
  uint8_t t43;
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
  uint8_t t56;
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
  uint8_t t100;
  uint8_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint8_t t114;
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
  int64_t t126;
  int64_t t127;
  int64_t t128;
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
  int64_t t139;
  int64_t t140;
  int64_t v0;
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
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(1403))));
  t2 = fg14_s_record_new(t1);
  v2 = ((int64_t)(t2));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) <= (t4));
  if (t5) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t6 = fg14_s_event_create();
  v3 = ((int64_t)(t6));
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  t10 = t9;
  if (!t10) {
  t11 = fg14_s_last_error();
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t12) != (t13));
  }
  t14 = t10;
  if (t14) {
  {
  t15 = ((int64_t)(v2));
  t16 = fg14_s_record_free(t15);
  (void)(t16);
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  t18 = ((int64_t)(((int64_t)UINT64_C(19))));
  t19 = fg14_s_check(t17, t18);
  (void)(t19);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t20 = fg14_s_map_new();
  v4 = ((int64_t)(t20));
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t21) == (t22));
  if (t23) {
  {
  t24 = ((int64_t)(v3));
  t25 = fg14_s_event_destroy(t24);
  (void)(t25);
  t26 = ((int64_t)(v2));
  t27 = fg14_s_record_free(t26);
  (void)(t27);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(((int64_t)UINT64_C(8))));
  t29 = fg14_s_vec_new(t28);
  v5 = ((int64_t)(t29));
  t30 = ((int64_t)(v5));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((t30) == (t31));
  if (t32) {
  {
  t33 = ((int64_t)(v4));
  t34 = fg14_s_map_free(t33);
  (void)(t34);
  t35 = ((int64_t)(v3));
  t36 = fg14_s_event_destroy(t35);
  (void)(t36);
  t37 = ((int64_t)(v2));
  t38 = fg14_s_record_free(t37);
  (void)(t38);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t39 = ((int64_t)(((int64_t)UINT64_C(8))));
  t40 = fg14_s_vec_new(t39);
  v6 = ((int64_t)(t40));
  t41 = ((int64_t)(v6));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t41) == (t42));
  if (t43) {
  {
  t44 = ((int64_t)(v5));
  t45 = fg14_s_vec_free(t44);
  (void)(t45);
  t46 = ((int64_t)(v4));
  t47 = fg14_s_map_free(t46);
  (void)(t47);
  t48 = ((int64_t)(v3));
  t49 = fg14_s_event_destroy(t48);
  (void)(t49);
  t50 = ((int64_t)(v2));
  t51 = fg14_s_record_free(t50);
  (void)(t51);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t52 = ((int64_t)(((int64_t)UINT64_C(8))));
  t53 = fg14_s_vec_new(t52);
  v7 = ((int64_t)(t53));
  t54 = ((int64_t)(v7));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = ((t54) == (t55));
  if (t56) {
  {
  t57 = ((int64_t)(v6));
  t58 = fg14_s_vec_free(t57);
  (void)(t58);
  t59 = ((int64_t)(v5));
  t60 = fg14_s_vec_free(t59);
  (void)(t60);
  t61 = ((int64_t)(v4));
  t62 = fg14_s_map_free(t61);
  (void)(t62);
  t63 = ((int64_t)(v3));
  t64 = fg14_s_event_destroy(t63);
  (void)(t64);
  t65 = ((int64_t)(v2));
  t66 = fg14_s_record_free(t65);
  (void)(t66);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t67 = ((int64_t)(v2));
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  t69 = ((int64_t)(v3));
  t70 = fg14_s_record_set(t67, t68, t69);
  v8 = ((int64_t)(t70));
  t71 = ((int64_t)(v2));
  t72 = ((int64_t)(((int64_t)UINT64_C(2))));
  t73 = ((int64_t)(v1));
  t74 = fg14_s_record_set(t71, t72, t73);
  v9 = ((int64_t)(t74));
  t75 = ((int64_t)(v2));
  t76 = ((int64_t)(((int64_t)UINT64_C(3))));
  t77 = ((int64_t)(v4));
  t78 = fg14_s_record_set(t75, t76, t77);
  v10 = ((int64_t)(t78));
  t79 = ((int64_t)(v2));
  t80 = ((int64_t)(((int64_t)UINT64_C(4))));
  t81 = ((int64_t)(v6));
  t82 = fg14_s_record_set(t79, t80, t81);
  v11 = ((int64_t)(t82));
  t83 = ((int64_t)(v2));
  t84 = ((int64_t)(((int64_t)UINT64_C(5))));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = fg14_s_record_set(t83, t84, t85);
  v12 = ((int64_t)(t86));
  t87 = ((int64_t)(v2));
  t88 = ((int64_t)(((int64_t)UINT64_C(6))));
  t89 = ((int64_t)(v7));
  t90 = fg14_s_record_set(t87, t88, t89);
  v13 = ((int64_t)(t90));
  t91 = ((int64_t)(v2));
  t92 = ((int64_t)(((int64_t)UINT64_C(7))));
  t93 = ((int64_t)(v5));
  t94 = fg14_s_record_set(t91, t92, t93);
  v14 = ((int64_t)(t94));
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t95 = ((int64_t)(v15));
  t96 = ((int64_t)(v1));
  if (!(((t95) < (t96)))) goto label2;
  {
  t97 = fg14_s_event_create();
  v16 = ((int64_t)(t97));
  t98 = ((int64_t)(v16));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = ((t98) == (t99));
  t101 = t100;
  if (!t101) {
  t102 = fg14_s_last_error();
  t103 = ((int64_t)(t102));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = ((t103) != (t104));
  }
  t105 = t101;
  if (t105) {
  {
  t106 = ((int64_t)(((int64_t)UINT64_C(1))));
  t107 = ((int64_t)(((int64_t)UINT64_C(20))));
  t108 = fg14_s_check(t106, t107);
  (void)(t108);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t109 = ((int64_t)(v7));
  t110 = ((int64_t)(v16));
  t111 = gg_function_34(t109, t110, base + 0);
  t112 = ((int64_t)(t111));
  t113 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((t112) < (t113));
  if (t114) {
  {
  t115 = ((int64_t)(v16));
  t116 = fg14_s_event_destroy(t115);
  (void)(t116);
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t117 = ((int64_t)(v17));
  t118 = ((int64_t)(v7));
  t119 = fg14_s_vec_len(t118);
  t120 = ((int64_t)(t119));
  if (!(((t117) < (t120)))) goto label5;
  {
  t121 = ((int64_t)(v7));
  t122 = ((int64_t)(v17));
  t123 = gg_function_16(t121, t122, base + 0);
  t124 = fg14_s_event_destroy(t123);
  (void)(t124);
  t125 = v17;
  t126 = ((int64_t)(((int64_t)UINT64_C(1))));
  v17 = ((int64_t)((uint64_t)(t125) + (uint64_t)(t126)));
  }
  goto label4;
label5:;
  t127 = ((int64_t)(v7));
  t128 = fg14_s_vec_free(t127);
  (void)(t128);
  t129 = ((int64_t)(v6));
  t130 = fg14_s_vec_free(t129);
  (void)(t130);
  t131 = ((int64_t)(v5));
  t132 = fg14_s_vec_free(t131);
  (void)(t132);
  t133 = ((int64_t)(v4));
  t134 = fg14_s_map_free(t133);
  (void)(t134);
  t135 = ((int64_t)(v3));
  t136 = fg14_s_event_destroy(t135);
  (void)(t136);
  t137 = ((int64_t)(v2));
  t138 = fg14_s_record_free(t137);
  (void)(t138);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t139 = v15;
  t140 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)((uint64_t)(t139) + (uint64_t)(t140)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_57(int64_t p0, int64_t base) {
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
  uint8_t t18;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(13))));
  t3 = fg14_s_record_get(t1, t2);
  v1 = ((int64_t)(t3));
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v1));
  t6 = fg14_s_vec_len(t5);
  t7 = ((int64_t)(t6));
  if (!(((t4) < (t7)))) goto label2;
  {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(9))));
  t10 = fg14_s_record_get(t8, t9);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v2));
  t14 = gg_function_16(t12, t13, base + 0);
  t15 = fg14_s_map_get(t11, t14);
  v3 = ((int64_t)(t15));
  t16 = ((int64_t)(v3));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) != (t17));
  if (t18) {
  {
  t19 = ((int64_t)(v3));
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = fg14_s_record_set(t19, t20, t21);
  v4 = ((int64_t)(t22));
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(((int64_t)UINT64_C(2))));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = fg14_s_record_set(t23, t24, t25);
  v5 = ((int64_t)(t26));
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(3))));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = fg14_s_record_set(t27, t28, t29);
  v6 = ((int64_t)(t30));
  t31 = ((int64_t)(v3));
  t32 = ((int64_t)(((int64_t)UINT64_C(5))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = fg14_s_record_set(t31, t32, t33);
  v7 = ((int64_t)(t34));
  }
  }
  t35 = v2;
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)((uint64_t)(t35) + (uint64_t)(t36)));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_54(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = fg14_s_record_get(t1, t2);
  v2 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = fg14_s_g_n(t4);
  v3 = ((int64_t)(t5));
  t6 = ((int64_t)(v2));
  t7 = fg14_s_vec_len(t6);
  v4 = ((int64_t)(t7));
  t8 = ((int64_t)(v4));
  t9 = ((int64_t)(v3));
  t10 = ((t8) != (t9));
  if (t10) {
  {
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(v3));
  t13 = fg14_s_vec_resize(t11, t12);
  v5 = ((int64_t)(t13));
  t14 = ((int64_t)(v5));
  t15 = ((int64_t)(((int64_t)UINT64_C(0))));
  t16 = ((t14) < (t15));
  if (t16) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(3))));
  t19 = ((int64_t)(v3));
  t20 = fg14_s_record_set(t17, t18, t19);
  v6 = ((int64_t)(t20));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t21 = ((int64_t)(v7));
  t22 = ((int64_t)(v3));
  if (!(((t21) < (t22)))) goto label2;
  {
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(v7));
  t26 = fg14_s_g_np_capture(t23, t24, t25);
  (void)(t26);
  t27 = v7;
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t27) + (uint64_t)(t28)));
  }
  goto label1;
label2:;
  }
  }
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_52(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
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
  uint8_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t v0;
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
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(9))));
  t3 = fg14_s_record_get(t1, t2);
  v2 = ((int64_t)(t3));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v1));
  t6 = fg14_s_map_get(t4, t5);
  v3 = ((int64_t)(t6));
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) >= (t8));
  if (t9) {
  {
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
  }
  t10 = ((int64_t)(((int64_t)UINT64_C(1402))));
  t11 = fg14_s_record_new(t10);
  v4 = ((int64_t)(t11));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) <= (t13));
  if (t14) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t15 = ((int64_t)(((int64_t)UINT64_C(8))));
  t16 = ((int64_t)(((int64_t)UINT64_C(112))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = fg14_s_vec_new(t17);
  v5 = ((int64_t)(t18));
  t19 = ((int64_t)(v5));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = ((int64_t)(v4));
  t23 = fg14_s_record_free(t22);
  (void)(t23);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = fg14_s_record_set(t24, t25, t26);
  v6 = ((int64_t)(t27));
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(((int64_t)UINT64_C(2))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg14_s_record_set(t28, t29, t30);
  v7 = ((int64_t)(t31));
  t32 = ((int64_t)(v4));
  t33 = ((int64_t)(((int64_t)UINT64_C(3))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = fg14_s_record_set(t32, t33, t34);
  v8 = ((int64_t)(t35));
  t36 = ((int64_t)(v4));
  t37 = ((int64_t)(((int64_t)UINT64_C(4))));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = fg14_s_record_set(t36, t37, t38);
  v9 = ((int64_t)(t39));
  t40 = ((int64_t)(v4));
  t41 = ((int64_t)(((int64_t)UINT64_C(5))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = fg14_s_record_set(t40, t41, t42);
  v10 = ((int64_t)(t43));
  t44 = ((int64_t)(v4));
  t45 = ((int64_t)(((int64_t)UINT64_C(6))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = fg14_s_record_set(t44, t45, t46);
  v11 = ((int64_t)(t47));
  t48 = ((int64_t)(v4));
  t49 = ((int64_t)(((int64_t)UINT64_C(7))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg14_s_record_set(t48, t49, t50);
  v12 = ((int64_t)(t51));
  t52 = ((int64_t)(v4));
  t53 = ((int64_t)(((int64_t)UINT64_C(8))));
  t54 = ((int64_t)(v5));
  t55 = fg14_s_record_set(t52, t53, t54);
  v13 = ((int64_t)(t55));
  t56 = ((int64_t)(v4));
  t57 = ((int64_t)(((int64_t)UINT64_C(9))));
  t58 = ((int64_t)(v1));
  t59 = fg14_s_record_set(t56, t57, t58);
  v14 = ((int64_t)(t59));
  t60 = ((int64_t)(v2));
  t61 = ((int64_t)(v1));
  t62 = ((int64_t)(v4));
  t63 = fg14_s_map_put(t60, t61, t62);
  t64 = ((int64_t)(t63));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((t64) < (t65));
  if (t66) {
  {
  t67 = ((int64_t)(v5));
  t68 = fg14_s_vec_free(t67);
  (void)(t68);
  t69 = ((int64_t)(v4));
  t70 = fg14_s_record_free(t69);
  (void)(t70);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(((int64_t)UINT64_C(13))));
  t73 = fg14_s_record_get(t71, t72);
  t74 = ((int64_t)(t73));
  t75 = ((int64_t)(v1));
  t76 = gg_function_34(t74, t75, base + 0);
  t77 = ((int64_t)(t76));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = ((t77) < (t78));
  if (t79) {
  {
  t80 = ((int64_t)(v2));
  t81 = ((int64_t)(v1));
  t82 = fg14_s_map_erase(t80, t81);
  (void)(t82);
  t83 = ((int64_t)(v5));
  t84 = fg14_s_vec_free(t83);
  (void)(t84);
  t85 = ((int64_t)(v4));
  t86 = fg14_s_record_free(t85);
  (void)(t86);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_28(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(v1));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t4)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_45(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  int64_t v0;
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
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = fg14_s_record_get(t1, t2);
  v3 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  v4 = gg_function_28(t4, t5, base + 0);
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(v4));
  t8 = fg14_s_map_get(t6, t7);
  v5 = ((int64_t)(t8));
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) >= (t10));
  if (t11) {
  {
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(18))));
  t14 = fg14_s_record_get(t12, t13);
  v6 = ((int64_t)(t14));
  t15 = ((int64_t)(v1));
  t16 = fg14_s_info_vmm(t15);
  v7 = ((int64_t)(t16));
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(v7));
  t21 = fg14_s_pool_create(t17, t18, t19, t20);
  v8 = ((int64_t)(t21));
  t22 = ((int64_t)(v8));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t25 = ((int64_t)(((int64_t)UINT64_C(1))));
  t26 = ((int64_t)(((int64_t)UINT64_C(16))));
  t27 = fg14_s_check(t25, t26);
  (void)(t27);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t28 = ((int64_t)(v3));
  t29 = ((int64_t)(v4));
  t30 = ((int64_t)(v8));
  t31 = fg14_s_map_put(t28, t29, t30);
  v9 = ((int64_t)(t31));
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(17))));
  t34 = fg14_s_record_get(t32, t33);
  t35 = ((int64_t)(t34));
  t36 = ((int64_t)(v4));
  v10 = gg_function_34(t35, t36, base + 0);
  t0 = ((int64_t)(v8));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_50(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  v5 = gg_function_45(t1, t2, t3, base + 0);
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(v4));
  t7 = fg14_s_pool_free(t4, t5, t6);
  (void)(t7);
  t8 = fg14_s_last_error();
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(18))));
  t11 = fg14_s_check(t9, t10);
  (void)(t11);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_48(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  v5 = gg_function_45(t1, t2, t3, base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(v4));
  t7 = fg14_s_pool_alloc(t4, t5, t6);
  v7 = ((int64_t)(t7));
  t8 = fg14_s_last_error();
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)UINT64_C(17))));
  t11 = fg14_s_check(t9, t10);
  (void)(t11);
  t0 = ((int64_t)(v7));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_27(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t v0;
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
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = fg14_s_record_get(t1, t2);
  v3 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  v4 = gg_function_28(t4, t5, base + 0);
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(v4));
  t8 = fg14_s_map_get(t6, t7);
  v5 = ((int64_t)(t8));
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) >= (t10));
  if (t11) {
  {
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v1));
  t13 = fg14_s_device_set(t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(10))));
  t16 = fg14_s_check(t14, t15);
  v6 = ((int64_t)(t16));
  t17 = fg14_s_stream_create();
  v7 = ((int64_t)(t17));
  t18 = fg14_s_last_error();
  t19 = ((int64_t)(t18));
  t20 = ((int64_t)(((int64_t)UINT64_C(11))));
  t21 = fg14_s_check(t19, t20);
  (void)(t21);
  t22 = ((int64_t)(v7));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v3));
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(v7));
  t28 = fg14_s_map_put(t25, t26, t27);
  v8 = ((int64_t)(t28));
  t29 = ((int64_t)(v8));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t29) < (t30));
  if (t31) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(14))));
  t34 = fg14_s_record_get(t32, t33);
  v9 = ((int64_t)(t34));
  t35 = ((int64_t)(v9));
  t36 = ((int64_t)(v4));
  v10 = gg_function_34(t35, t36, base + 0);
  t0 = ((int64_t)(v7));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_38(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint8_t t33;
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
  uint8_t t48;
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
  int64_t v0;
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
  int64_t v11;
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
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(5))));
  t3 = fg14_s_record_get(t1, t2);
  v3 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  v4 = gg_function_28(t4, t5, base + 0);
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(v4));
  t8 = fg14_s_map_get(t6, t7);
  v5 = ((int64_t)(t8));
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) >= (t10));
  if (t11) {
  {
  t0 = ((int64_t)(v5));
  goto gg_done;
  }
  }
  t12 = ((int64_t)(v1));
  t13 = fg14_s_device_set(t12);
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(12))));
  t16 = fg14_s_check(t14, t15);
  v6 = ((int64_t)(t16));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(v2));
  v7 = gg_function_27(t17, t18, t19, base + 0);
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(v7));
  t24 = fg14_s_cublas_create(t20, t21, t22, t23);
  v8 = ((int64_t)(t24));
  t25 = ((int64_t)(v8));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t29 = ((int64_t)(((int64_t)UINT64_C(13))));
  t30 = fg14_s_check(t28, t29);
  (void)(t30);
  }
  }
  t31 = ((int64_t)(v8));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  if (t33) {
  {
  t34 = ((int64_t)(v8));
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  t36 = fg14_s_cublas_set_math(t34, t35);
  v9 = ((int64_t)(t36));
  t37 = ((int64_t)(v8));
  t38 = ((int64_t)(v7));
  t39 = fg14_s_cublas_set_stream(t37, t38);
  v10 = ((int64_t)(t39));
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(6))));
  t42 = fg14_s_record_get(t40, t41);
  t43 = ((int64_t)(t42));
  t44 = ((int64_t)(v1));
  t45 = fg14_s_map_get(t43, t44);
  v11 = ((int64_t)(t45));
  t46 = ((int64_t)(v11));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((t46) < (t47));
  if (t48) {
  {
  t49 = ((int64_t)(v1));
  t50 = fg14_s_info_cc(t49);
  v12 = ((int64_t)(t50));
  t51 = ((int64_t)(v12));
  t52 = ((int64_t)(((int64_t)UINT64_C(900))));
  t53 = ((t51) >= (t52));
  if (t53) {
  {
  t54 = ((int64_t)(((int64_t)UINT64_C(32))));
  t55 = ((int64_t)(((int64_t)UINT64_C(1024))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  t57 = ((int64_t)(((int64_t)UINT64_C(1024))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  }
  } else {
  {
  t58 = ((int64_t)(((int64_t)UINT64_C(4))));
  t59 = ((int64_t)(((int64_t)UINT64_C(1024))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(((int64_t)UINT64_C(1024))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  }
  }
  t62 = ((int64_t)(v0));
  t63 = ((int64_t)(((int64_t)UINT64_C(6))));
  t64 = fg14_s_record_get(t62, t63);
  t65 = ((int64_t)(t64));
  t66 = ((int64_t)(v1));
  t67 = ((int64_t)(v11));
  t68 = fg14_s_map_put(t65, t66, t67);
  v13 = ((int64_t)(t68));
  }
  }
  t69 = ((int64_t)(v11));
  t70 = fg14_s_malloc(t69);
  v14 = ((int64_t)(t70));
  t71 = fg14_s_last_error();
  t72 = ((int64_t)(t71));
  t73 = ((int64_t)(((int64_t)UINT64_C(14))));
  t74 = fg14_s_check(t72, t73);
  (void)(t74);
  t75 = ((int64_t)(v8));
  t76 = ((int64_t)(v14));
  t77 = ((int64_t)(v11));
  t78 = fg14_s_cublas_set_workspace(t75, t76, t77);
  t79 = ((int64_t)(t78));
  t80 = ((int64_t)(((int64_t)UINT64_C(15))));
  t81 = fg14_s_check(t79, t80);
  (void)(t81);
  t82 = ((int64_t)(v0));
  t83 = ((int64_t)(((int64_t)UINT64_C(7))));
  t84 = fg14_s_record_get(t82, t83);
  v15 = ((int64_t)(t84));
  t85 = ((int64_t)(v15));
  t86 = ((int64_t)(v4));
  t87 = ((int64_t)(v14));
  t88 = fg14_s_map_put(t85, t86, t87);
  v16 = ((int64_t)(t88));
  t89 = ((int64_t)(v0));
  t90 = ((int64_t)(((int64_t)UINT64_C(16))));
  t91 = fg14_s_record_get(t89, t90);
  t92 = ((int64_t)(t91));
  t93 = ((int64_t)(v4));
  v17 = gg_function_34(t92, t93, base + 0);
  }
  }
  t94 = ((int64_t)(v3));
  t95 = ((int64_t)(v4));
  t96 = ((int64_t)(v8));
  t97 = fg14_s_map_put(t94, t95, t96);
  v18 = ((int64_t)(t97));
  t98 = ((int64_t)(v0));
  t99 = ((int64_t)(((int64_t)UINT64_C(15))));
  t100 = fg14_s_record_get(t98, t99);
  t101 = ((int64_t)(t100));
  t102 = ((int64_t)(v4));
  v19 = gg_function_34(t101, t102, base + 0);
  t0 = ((int64_t)(v8));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_13(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
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
  uint8_t t25;
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
  uint8_t t47;
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
  uint8_t t95;
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
  uint8_t t111;
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
  int64_t t126;
  uint8_t t127;
  int64_t t128;
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
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  uint8_t t143;
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
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  uint8_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t v0;
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
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t4 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t4)))));
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(18))));
  t7 = fg14_s_record_get(t5, t6);
  v1 = ((int64_t)(t7));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(11))));
  t10 = fg14_s_record_get(t8, t9);
  v2 = ((int64_t)(t10));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(((int64_t)UINT64_C(10))));
  t13 = fg14_s_record_get(t11, t12);
  v3 = ((int64_t)(t13));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)(v2));
  t16 = fg14_s_vec_len(t15);
  t17 = ((int64_t)(t16));
  if (!(((t14) < (t17)))) goto label2;
  {
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(v4));
  t21 = gg_function_16(t19, t20, base + 0);
  t22 = fg14_s_map_get(t18, t21);
  v5 = ((int64_t)(t22));
  t23 = ((int64_t)(v5));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((t23) > (t24));
  if (t25) {
  {
  t26 = ((int64_t)(v5));
  t27 = gg_function_21(t26, base + 0);
  (void)(t27);
  }
  }
  t28 = v4;
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t28) + (uint64_t)(t29)));
  }
  goto label1;
label2:;
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(13))));
  t32 = fg14_s_record_get(t30, t31);
  v6 = ((int64_t)(t32));
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(9))));
  t35 = fg14_s_record_get(t33, t34);
  v7 = ((int64_t)(t35));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t36 = ((int64_t)(v4));
  t37 = ((int64_t)(v6));
  t38 = fg14_s_vec_len(t37);
  t39 = ((int64_t)(t38));
  if (!(((t36) < (t39)))) goto label5;
  {
  t40 = ((int64_t)(v7));
  t41 = ((int64_t)(v6));
  t42 = ((int64_t)(v4));
  t43 = gg_function_16(t41, t42, base + 0);
  t44 = fg14_s_map_get(t40, t43);
  v8 = ((int64_t)(t44));
  t45 = ((int64_t)(v8));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((t45) > (t46));
  if (t47) {
  {
  t48 = ((int64_t)(v8));
  t49 = ((int64_t)(((int64_t)UINT64_C(8))));
  t50 = fg14_s_record_get(t48, t49);
  v9 = ((int64_t)(t50));
  t51 = ((int64_t)(v9));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((t51) != (t52));
  if (t53) {
  {
  t54 = ((int64_t)(v9));
  t55 = fg14_s_vec_free(t54);
  (void)(t55);
  }
  }
  t56 = ((int64_t)(v8));
  t57 = fg14_s_record_free(t56);
  (void)(t57);
  }
  }
  t58 = v4;
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t58) + (uint64_t)(t59)));
  }
  goto label4;
label5:;
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(14))));
  t62 = fg14_s_record_get(t60, t61);
  v10 = ((int64_t)(t62));
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(((int64_t)UINT64_C(15))));
  t65 = fg14_s_record_get(t63, t64);
  v11 = ((int64_t)(t65));
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(16))));
  t68 = fg14_s_record_get(t66, t67);
  v12 = ((int64_t)(t68));
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(((int64_t)UINT64_C(17))));
  t71 = fg14_s_record_get(t69, t70);
  v13 = ((int64_t)(t71));
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(((int64_t)UINT64_C(4))));
  t74 = fg14_s_record_get(t72, t73);
  v14 = ((int64_t)(t74));
  t75 = ((int64_t)(v0));
  t76 = ((int64_t)(((int64_t)UINT64_C(5))));
  t77 = fg14_s_record_get(t75, t76);
  v15 = ((int64_t)(t77));
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(((int64_t)UINT64_C(7))));
  t80 = fg14_s_record_get(t78, t79);
  v16 = ((int64_t)(t80));
  t81 = ((int64_t)(v0));
  t82 = ((int64_t)(((int64_t)UINT64_C(8))));
  t83 = fg14_s_record_get(t81, t82);
  v17 = ((int64_t)(t83));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t84 = ((int64_t)(v4));
  t85 = ((int64_t)(v10));
  t86 = fg14_s_vec_len(t85);
  t87 = ((int64_t)(t86));
  if (!(((t84) < (t87)))) goto label8;
  {
  t88 = ((int64_t)(v14));
  t89 = ((int64_t)(v10));
  t90 = ((int64_t)(v4));
  t91 = gg_function_16(t89, t90, base + 0);
  t92 = fg14_s_map_get(t88, t91);
  v18 = ((int64_t)(t92));
  t93 = ((int64_t)(v18));
  t94 = ((int64_t)(((int64_t)UINT64_C(0))));
  t95 = ((t93) > (t94));
  if (t95) {
  {
  t96 = ((int64_t)(v18));
  t97 = fg14_s_stream_destroy(t96);
  (void)(t97);
  }
  }
  t98 = v4;
  t99 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t98) + (uint64_t)(t99)));
  }
  goto label7;
label8:;
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t100 = ((int64_t)(v4));
  t101 = ((int64_t)(v11));
  t102 = fg14_s_vec_len(t101);
  t103 = ((int64_t)(t102));
  if (!(((t100) < (t103)))) goto label11;
  {
  t104 = ((int64_t)(v15));
  t105 = ((int64_t)(v11));
  t106 = ((int64_t)(v4));
  t107 = gg_function_16(t105, t106, base + 0);
  t108 = fg14_s_map_get(t104, t107);
  v19 = ((int64_t)(t108));
  t109 = ((int64_t)(v19));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((t109) > (t110));
  if (t111) {
  {
  t112 = ((int64_t)(v19));
  t113 = fg14_s_cublas_destroy(t112);
  (void)(t113);
  }
  }
  t114 = v4;
  t115 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t114) + (uint64_t)(t115)));
  }
  goto label10;
label11:;
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t116 = ((int64_t)(v4));
  t117 = ((int64_t)(v12));
  t118 = fg14_s_vec_len(t117);
  t119 = ((int64_t)(t118));
  if (!(((t116) < (t119)))) goto label14;
  {
  t120 = ((int64_t)(v16));
  t121 = ((int64_t)(v12));
  t122 = ((int64_t)(v4));
  t123 = gg_function_16(t121, t122, base + 0);
  t124 = fg14_s_map_get(t120, t123);
  v20 = ((int64_t)(t124));
  t125 = ((int64_t)(v20));
  t126 = ((int64_t)(((int64_t)UINT64_C(0))));
  t127 = ((t125) > (t126));
  if (t127) {
  {
  t128 = ((int64_t)(v20));
  t129 = fg14_s_free(t128);
  (void)(t129);
  }
  }
  t130 = v4;
  t131 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t130) + (uint64_t)(t131)));
  }
  goto label13;
label14:;
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t132 = ((int64_t)(v4));
  t133 = ((int64_t)(v13));
  t134 = fg14_s_vec_len(t133);
  t135 = ((int64_t)(t134));
  if (!(((t132) < (t135)))) goto label17;
  {
  t136 = ((int64_t)(v17));
  t137 = ((int64_t)(v13));
  t138 = ((int64_t)(v4));
  t139 = gg_function_16(t137, t138, base + 0);
  t140 = fg14_s_map_get(t136, t139);
  v21 = ((int64_t)(t140));
  t141 = ((int64_t)(v21));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = ((t141) > (t142));
  if (t143) {
  {
  t144 = ((int64_t)(v21));
  t145 = fg14_s_pool_destroy(t144);
  (void)(t145);
  }
  }
  t146 = v4;
  t147 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)((uint64_t)(t146) + (uint64_t)(t147)));
  }
  goto label16;
label17:;
  t148 = ((int64_t)(v7));
  t149 = fg14_s_map_free(t148);
  (void)(t149);
  t150 = ((int64_t)(v3));
  t151 = fg14_s_map_free(t150);
  (void)(t151);
  t152 = ((int64_t)(v14));
  t153 = fg14_s_map_free(t152);
  (void)(t153);
  t154 = ((int64_t)(v15));
  t155 = fg14_s_map_free(t154);
  (void)(t155);
  t156 = ((int64_t)(v0));
  t157 = ((int64_t)(((int64_t)UINT64_C(6))));
  t158 = fg14_s_record_get(t156, t157);
  t159 = ((int64_t)(t158));
  t160 = fg14_s_map_free(t159);
  (void)(t160);
  t161 = ((int64_t)(v16));
  t162 = fg14_s_map_free(t161);
  (void)(t162);
  t163 = ((int64_t)(v17));
  t164 = fg14_s_map_free(t163);
  (void)(t164);
  t165 = ((int64_t)(v6));
  t166 = fg14_s_vec_free(t165);
  (void)(t166);
  t167 = ((int64_t)(v2));
  t168 = fg14_s_vec_free(t167);
  (void)(t168);
  t169 = ((int64_t)(v10));
  t170 = fg14_s_vec_free(t169);
  (void)(t170);
  t171 = ((int64_t)(v11));
  t172 = fg14_s_vec_free(t171);
  (void)(t172);
  t173 = ((int64_t)(v12));
  t174 = fg14_s_vec_free(t173);
  (void)(t174);
  t175 = ((int64_t)(v13));
  t176 = fg14_s_vec_free(t175);
  (void)(t176);
  t177 = ((int64_t)(v1));
  t178 = ((int64_t)(((int64_t)UINT64_C(0))));
  t179 = ((t177) != (t178));
  if (t179) {
  {
  t180 = ((int64_t)(v1));
  t181 = ((int64_t)(((int64_t)UINT64_C(0))));
  t182 = fg14_s_ctx_rec_set(t180, t181);
  (void)(t182);
  }
  }
  t183 = ((int64_t)(v0));
  t184 = fg14_s_record_free(t183);
  (void)(t184);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_2(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
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
  uint8_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  uint8_t t40;
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
  uint8_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  uint8_t t67;
  uint8_t t68;
  int64_t t69;
  int64_t t70;
  uint8_t t71;
  uint8_t t72;
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
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint8_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  uint8_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint8_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
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
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  uint8_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
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
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
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
  int64_t v0;
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
  v0 = p0;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(1401))));
  t2 = fg14_s_record_new(t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((t3) <= (t4));
  if (t5) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  t8 = ((int64_t)(v0));
  t9 = fg14_s_record_set(t6, t7, t8);
  v2 = ((int64_t)(t9));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = fg14_s_record_set(t10, t11, t12);
  v3 = ((int64_t)(t13));
  t14 = fg14_s_map_new();
  v4 = ((int64_t)(t14));
  t15 = fg14_s_map_new();
  v5 = ((int64_t)(t15));
  t16 = fg14_s_map_new();
  v6 = ((int64_t)(t16));
  t17 = fg14_s_map_new();
  v7 = ((int64_t)(t17));
  t18 = fg14_s_map_new();
  v8 = ((int64_t)(t18));
  t19 = fg14_s_map_new();
  v9 = ((int64_t)(t19));
  t20 = fg14_s_map_new();
  v10 = ((int64_t)(t20));
  t21 = ((int64_t)(((int64_t)UINT64_C(8))));
  t22 = fg14_s_vec_new(t21);
  v11 = ((int64_t)(t22));
  t23 = ((int64_t)(((int64_t)UINT64_C(8))));
  t24 = fg14_s_vec_new(t23);
  v12 = ((int64_t)(t24));
  t25 = ((int64_t)(((int64_t)UINT64_C(8))));
  t26 = fg14_s_vec_new(t25);
  v13 = ((int64_t)(t26));
  t27 = ((int64_t)(((int64_t)UINT64_C(8))));
  t28 = fg14_s_vec_new(t27);
  v14 = ((int64_t)(t28));
  t29 = ((int64_t)(((int64_t)UINT64_C(8))));
  t30 = fg14_s_vec_new(t29);
  v15 = ((int64_t)(t30));
  t31 = ((int64_t)(((int64_t)UINT64_C(8))));
  t32 = fg14_s_vec_new(t31);
  v16 = ((int64_t)(t32));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) == (t34));
  t36 = t35;
  if (!t36) {
  t37 = ((int64_t)(v5));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((t37) == (t38));
  }
  t39 = t36;
  t40 = t39;
  if (!t40) {
  t41 = ((int64_t)(v6));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t41) == (t42));
  }
  t43 = t40;
  t44 = t43;
  if (!t44) {
  t45 = ((int64_t)(v7));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = ((t45) == (t46));
  }
  t47 = t44;
  t48 = t47;
  if (!t48) {
  t49 = ((int64_t)(v8));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((t49) == (t50));
  }
  t51 = t48;
  t52 = t51;
  if (!t52) {
  t53 = ((int64_t)(v9));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((t53) == (t54));
  }
  t55 = t52;
  t56 = t55;
  if (!t56) {
  t57 = ((int64_t)(v10));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = ((t57) == (t58));
  }
  t59 = t56;
  t60 = t59;
  if (!t60) {
  t61 = ((int64_t)(v11));
  t62 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((t61) == (t62));
  }
  t63 = t60;
  t64 = t63;
  if (!t64) {
  t65 = ((int64_t)(v12));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((t65) == (t66));
  }
  t67 = t64;
  t68 = t67;
  if (!t68) {
  t69 = ((int64_t)(v13));
  t70 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = ((t69) == (t70));
  }
  t71 = t68;
  t72 = t71;
  if (!t72) {
  t73 = ((int64_t)(v14));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((t73) == (t74));
  }
  t75 = t72;
  t76 = t75;
  if (!t76) {
  t77 = ((int64_t)(v15));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = ((t77) == (t78));
  }
  t79 = t76;
  t80 = t79;
  if (!t80) {
  t81 = ((int64_t)(v16));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = ((t81) == (t82));
  }
  t83 = t80;
  if (t83) {
  {
  t84 = ((int64_t)(v4));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = ((t84) != (t85));
  if (t86) {
  {
  t87 = ((int64_t)(v4));
  t88 = fg14_s_map_free(t87);
  (void)(t88);
  }
  }
  t89 = ((int64_t)(v5));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = ((t89) != (t90));
  if (t91) {
  {
  t92 = ((int64_t)(v5));
  t93 = fg14_s_map_free(t92);
  (void)(t93);
  }
  }
  t94 = ((int64_t)(v6));
  t95 = ((int64_t)(((int64_t)UINT64_C(0))));
  t96 = ((t94) != (t95));
  if (t96) {
  {
  t97 = ((int64_t)(v6));
  t98 = fg14_s_map_free(t97);
  (void)(t98);
  }
  }
  t99 = ((int64_t)(v7));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = ((t99) != (t100));
  if (t101) {
  {
  t102 = ((int64_t)(v7));
  t103 = fg14_s_map_free(t102);
  (void)(t103);
  }
  }
  t104 = ((int64_t)(v8));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((t104) != (t105));
  if (t106) {
  {
  t107 = ((int64_t)(v8));
  t108 = fg14_s_map_free(t107);
  (void)(t108);
  }
  }
  t109 = ((int64_t)(v9));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((t109) != (t110));
  if (t111) {
  {
  t112 = ((int64_t)(v9));
  t113 = fg14_s_map_free(t112);
  (void)(t113);
  }
  }
  t114 = ((int64_t)(v10));
  t115 = ((int64_t)(((int64_t)UINT64_C(0))));
  t116 = ((t114) != (t115));
  if (t116) {
  {
  t117 = ((int64_t)(v10));
  t118 = fg14_s_map_free(t117);
  (void)(t118);
  }
  }
  t119 = ((int64_t)(v11));
  t120 = ((int64_t)(((int64_t)UINT64_C(0))));
  t121 = ((t119) != (t120));
  if (t121) {
  {
  t122 = ((int64_t)(v11));
  t123 = fg14_s_vec_free(t122);
  (void)(t123);
  }
  }
  t124 = ((int64_t)(v12));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((t124) != (t125));
  if (t126) {
  {
  t127 = ((int64_t)(v12));
  t128 = fg14_s_vec_free(t127);
  (void)(t128);
  }
  }
  t129 = ((int64_t)(v13));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = ((t129) != (t130));
  if (t131) {
  {
  t132 = ((int64_t)(v13));
  t133 = fg14_s_vec_free(t132);
  (void)(t133);
  }
  }
  t134 = ((int64_t)(v14));
  t135 = ((int64_t)(((int64_t)UINT64_C(0))));
  t136 = ((t134) != (t135));
  if (t136) {
  {
  t137 = ((int64_t)(v14));
  t138 = fg14_s_vec_free(t137);
  (void)(t138);
  }
  }
  t139 = ((int64_t)(v15));
  t140 = ((int64_t)(((int64_t)UINT64_C(0))));
  t141 = ((t139) != (t140));
  if (t141) {
  {
  t142 = ((int64_t)(v15));
  t143 = fg14_s_vec_free(t142);
  (void)(t143);
  }
  }
  t144 = ((int64_t)(v16));
  t145 = ((int64_t)(((int64_t)UINT64_C(0))));
  t146 = ((t144) != (t145));
  if (t146) {
  {
  t147 = ((int64_t)(v16));
  t148 = fg14_s_vec_free(t147);
  (void)(t148);
  }
  }
  t149 = ((int64_t)(v1));
  t150 = fg14_s_record_free(t149);
  (void)(t150);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t151 = ((int64_t)(v1));
  t152 = ((int64_t)(((int64_t)UINT64_C(4))));
  t153 = ((int64_t)(v4));
  t154 = fg14_s_record_set(t151, t152, t153);
  v17 = ((int64_t)(t154));
  t155 = ((int64_t)(v1));
  t156 = ((int64_t)(((int64_t)UINT64_C(5))));
  t157 = ((int64_t)(v5));
  t158 = fg14_s_record_set(t155, t156, t157);
  v18 = ((int64_t)(t158));
  t159 = ((int64_t)(v1));
  t160 = ((int64_t)(((int64_t)UINT64_C(6))));
  t161 = ((int64_t)(v6));
  t162 = fg14_s_record_set(t159, t160, t161);
  v19 = ((int64_t)(t162));
  t163 = ((int64_t)(v1));
  t164 = ((int64_t)(((int64_t)UINT64_C(7))));
  t165 = ((int64_t)(v7));
  t166 = fg14_s_record_set(t163, t164, t165);
  v20 = ((int64_t)(t166));
  t167 = ((int64_t)(v1));
  t168 = ((int64_t)(((int64_t)UINT64_C(8))));
  t169 = ((int64_t)(v8));
  t170 = fg14_s_record_set(t167, t168, t169);
  v21 = ((int64_t)(t170));
  t171 = ((int64_t)(v1));
  t172 = ((int64_t)(((int64_t)UINT64_C(9))));
  t173 = ((int64_t)(v9));
  t174 = fg14_s_record_set(t171, t172, t173);
  v22 = ((int64_t)(t174));
  t175 = ((int64_t)(v1));
  t176 = ((int64_t)(((int64_t)UINT64_C(10))));
  t177 = ((int64_t)(v10));
  t178 = fg14_s_record_set(t175, t176, t177);
  v23 = ((int64_t)(t178));
  t179 = ((int64_t)(v1));
  t180 = ((int64_t)(((int64_t)UINT64_C(11))));
  t181 = ((int64_t)(v11));
  t182 = fg14_s_record_set(t179, t180, t181);
  v24 = ((int64_t)(t182));
  t183 = ((int64_t)(v1));
  t184 = ((int64_t)(((int64_t)UINT64_C(13))));
  t185 = ((int64_t)(v12));
  t186 = fg14_s_record_set(t183, t184, t185);
  v25 = ((int64_t)(t186));
  t187 = ((int64_t)(v1));
  t188 = ((int64_t)(((int64_t)UINT64_C(14))));
  t189 = ((int64_t)(v13));
  t190 = fg14_s_record_set(t187, t188, t189);
  v26 = ((int64_t)(t190));
  t191 = ((int64_t)(v1));
  t192 = ((int64_t)(((int64_t)UINT64_C(15))));
  t193 = ((int64_t)(v14));
  t194 = fg14_s_record_set(t191, t192, t193);
  v27 = ((int64_t)(t194));
  t195 = ((int64_t)(v1));
  t196 = ((int64_t)(((int64_t)UINT64_C(16))));
  t197 = ((int64_t)(v15));
  t198 = fg14_s_record_set(t195, t196, t197);
  v28 = ((int64_t)(t198));
  t199 = ((int64_t)(v1));
  t200 = ((int64_t)(((int64_t)UINT64_C(17))));
  t201 = ((int64_t)(v16));
  t202 = fg14_s_record_set(t199, t200, t201);
  v29 = ((int64_t)(t202));
  t203 = ((int64_t)(v1));
  t204 = ((int64_t)(((int64_t)UINT64_C(12))));
  t205 = ((int64_t)(((int64_t)UINT64_C(0))));
  t206 = fg14_s_record_set(t203, t204, t205);
  v30 = ((int64_t)(t206));
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_10(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = fg14_s_ctx_rec(t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) != (t7));
  if (t8) {
  {
  t0 = ((int64_t)(v2));
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v1));
  v3 = gg_function_2(t9, base + 0);
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)UINT64_C(18))));
  t15 = ((int64_t)(v0));
  t16 = fg14_s_record_set(t13, t14, t15);
  v4 = ((int64_t)(t16));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(v3));
  t19 = fg14_s_ctx_rec_set(t17, t18);
  (void)(t19);
  t0 = ((int64_t)(v3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint8_t t68;
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
  int64_t t79;
  int64_t t80;
  uint8_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint8_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint8_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint8_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  uint8_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  uint8_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  uint8_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  uint8_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  uint8_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint8_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  uint8_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  uint8_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  uint8_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  uint8_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  uint8_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  uint8_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  uint8_t t198;
  int64_t t199;
  int64_t t200;
  int64_t v0;
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
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t0 = gg_function_2(t4, base + 0);
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(1))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(v2));
  t0 = gg_function_10(t8, t9, base + 0);
  goto gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t13 = ((int64_t)(v1));
  t0 = gg_function_13(t13, base + 0);
  goto gg_done;
  }
  }
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(3))));
  t16 = ((t14) == (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(v2));
  t19 = fg14_s_record_get(t17, t18);
  t0 = ((int64_t)(t19));
  goto gg_done;
  }
  }
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(4))));
  t22 = ((t20) == (t21));
  if (t22) {
  {
  t23 = ((int64_t)(v1));
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(v3));
  t26 = fg14_s_record_set(t23, t24, t25);
  t0 = ((int64_t)(t26));
  goto gg_done;
  }
  }
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(5))));
  t29 = ((t27) == (t28));
  if (t29) {
  {
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(v3));
  t0 = gg_function_27(t30, t31, t32, base + 0);
  goto gg_done;
  }
  }
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(6))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(v2));
  t38 = ((int64_t)(v3));
  t0 = gg_function_38(t36, t37, t38, base + 0);
  goto gg_done;
  }
  }
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)UINT64_C(7))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  t42 = ((int64_t)(v1));
  t43 = ((int64_t)(v2));
  t44 = ((int64_t)(v3));
  t0 = gg_function_45(t42, t43, t44, base + 0);
  goto gg_done;
  }
  }
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(8))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  t48 = ((int64_t)(v1));
  t49 = ((int64_t)(v2));
  t50 = ((int64_t)(v3));
  t51 = ((int64_t)(v4));
  t52 = ((int64_t)(v5));
  t0 = gg_function_48(t48, t49, t50, t51, t52, base + 0);
  goto gg_done;
  }
  }
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(9))));
  t55 = ((t53) == (t54));
  if (t55) {
  {
  t56 = ((int64_t)(v1));
  t57 = ((int64_t)(v2));
  t58 = ((int64_t)(v3));
  t59 = ((int64_t)(v4));
  t60 = ((int64_t)(v5));
  t0 = gg_function_50(t56, t57, t58, t59, t60, base + 0);
  goto gg_done;
  }
  }
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(10))));
  t63 = ((t61) == (t62));
  if (t63) {
  {
  t64 = ((int64_t)(v1));
  t65 = ((int64_t)(v2));
  t0 = gg_function_52(t64, t65, base + 0);
  goto gg_done;
  }
  }
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(11))));
  t68 = ((t66) == (t67));
  if (t68) {
  {
  t69 = ((int64_t)(v1));
  t70 = ((int64_t)(v2));
  t71 = fg14_s_record_get(t69, t70);
  t0 = ((int64_t)(t71));
  goto gg_done;
  }
  }
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(((int64_t)UINT64_C(12))));
  t74 = ((t72) == (t73));
  if (t74) {
  {
  t75 = ((int64_t)(v1));
  t76 = ((int64_t)(v2));
  t77 = ((int64_t)(v3));
  t78 = fg14_s_record_set(t75, t76, t77);
  t0 = ((int64_t)(t78));
  goto gg_done;
  }
  }
  t79 = ((int64_t)(v0));
  t80 = ((int64_t)(((int64_t)UINT64_C(13))));
  t81 = ((t79) == (t80));
  if (t81) {
  {
  t82 = ((int64_t)(v1));
  t83 = ((int64_t)(v2));
  t0 = gg_function_54(t82, t83, base + 0);
  goto gg_done;
  }
  }
  t84 = ((int64_t)(v0));
  t85 = ((int64_t)(((int64_t)UINT64_C(14))));
  t86 = ((t84) == (t85));
  if (t86) {
  {
  t87 = ((int64_t)(v1));
  t0 = gg_function_57(t87, base + 0);
  goto gg_done;
  }
  }
  t88 = ((int64_t)(v0));
  t89 = ((int64_t)(((int64_t)UINT64_C(15))));
  t90 = ((t88) == (t89));
  if (t90) {
  {
  t91 = ((int64_t)(v1));
  t92 = ((int64_t)(v2));
  t0 = gg_function_58(t91, t92, base + 0);
  goto gg_done;
  }
  }
  t93 = ((int64_t)(v0));
  t94 = ((int64_t)(((int64_t)UINT64_C(16))));
  t95 = ((t93) == (t94));
  if (t95) {
  {
  t96 = ((int64_t)(v1));
  t0 = gg_function_21(t96, base + 0);
  goto gg_done;
  }
  }
  t97 = ((int64_t)(v0));
  t98 = ((int64_t)(((int64_t)UINT64_C(17))));
  t99 = ((t97) == (t98));
  if (t99) {
  {
  t100 = ((int64_t)(v1));
  t101 = ((int64_t)(v2));
  t102 = fg14_s_record_get(t100, t101);
  t0 = ((int64_t)(t102));
  goto gg_done;
  }
  }
  t103 = ((int64_t)(v0));
  t104 = ((int64_t)(((int64_t)UINT64_C(18))));
  t105 = ((t103) == (t104));
  if (t105) {
  {
  t106 = ((int64_t)(v1));
  t107 = ((int64_t)(v2));
  t108 = ((int64_t)(v3));
  t109 = fg14_s_record_set(t106, t107, t108);
  t0 = ((int64_t)(t109));
  goto gg_done;
  }
  }
  t110 = ((int64_t)(v0));
  t111 = ((int64_t)(((int64_t)UINT64_C(19))));
  t112 = ((t110) == (t111));
  if (t112) {
  {
  t113 = ((int64_t)(v1));
  t114 = ((int64_t)(v2));
  t0 = gg_function_60(t113, t114, base + 0);
  goto gg_done;
  }
  }
  t115 = ((int64_t)(v0));
  t116 = ((int64_t)(((int64_t)UINT64_C(20))));
  t117 = ((t115) == (t116));
  if (t117) {
  {
  t118 = ((int64_t)(v1));
  t119 = ((int64_t)(v2));
  t120 = ((int64_t)(v3));
  t0 = gg_function_61(t118, t119, t120, base + 0);
  goto gg_done;
  }
  }
  t121 = ((int64_t)(v0));
  t122 = ((int64_t)(((int64_t)UINT64_C(21))));
  t123 = ((t121) == (t122));
  if (t123) {
  {
  t124 = ((int64_t)(v1));
  t125 = ((int64_t)(((int64_t)UINT64_C(4))));
  t126 = fg14_s_record_get(t124, t125);
  t127 = ((int64_t)(t126));
  t128 = fg14_s_vec_len(t127);
  t0 = ((int64_t)(t128));
  goto gg_done;
  }
  }
  t129 = ((int64_t)(v0));
  t130 = ((int64_t)(((int64_t)UINT64_C(22))));
  t131 = ((t129) == (t130));
  if (t131) {
  {
  t132 = ((int64_t)(v1));
  t133 = ((int64_t)(((int64_t)UINT64_C(4))));
  t134 = fg14_s_record_get(t132, t133);
  t135 = ((int64_t)(t134));
  t136 = ((int64_t)(v2));
  t0 = gg_function_16(t135, t136, base + 0);
  goto gg_done;
  }
  }
  t137 = ((int64_t)(v0));
  t138 = ((int64_t)(((int64_t)UINT64_C(23))));
  t139 = ((t137) == (t138));
  if (t139) {
  {
  t140 = ((int64_t)(v1));
  t141 = ((int64_t)(((int64_t)UINT64_C(4))));
  t142 = fg14_s_record_get(t140, t141);
  t143 = ((int64_t)(t142));
  t144 = ((int64_t)(v2));
  t0 = gg_function_34(t143, t144, base + 0);
  goto gg_done;
  }
  }
  t145 = ((int64_t)(v0));
  t146 = ((int64_t)(((int64_t)UINT64_C(24))));
  t147 = ((t145) == (t146));
  if (t147) {
  {
  t148 = ((int64_t)(v1));
  t149 = ((int64_t)(((int64_t)UINT64_C(6))));
  t150 = fg14_s_record_get(t148, t149);
  t151 = ((int64_t)(t150));
  t152 = ((int64_t)(v2));
  t0 = gg_function_16(t151, t152, base + 0);
  goto gg_done;
  }
  }
  t153 = ((int64_t)(v0));
  t154 = ((int64_t)(((int64_t)UINT64_C(25))));
  t155 = ((t153) == (t154));
  if (t155) {
  {
  t156 = ((int64_t)(v1));
  t0 = gg_function_62(t156, base + 0);
  goto gg_done;
  }
  }
  t157 = ((int64_t)(v0));
  t158 = ((int64_t)(((int64_t)UINT64_C(26))));
  t159 = ((t157) == (t158));
  if (t159) {
  {
  t160 = ((int64_t)(v1));
  t0 = gg_function_67(t160, base + 0);
  goto gg_done;
  }
  }
  t161 = ((int64_t)(v0));
  t162 = ((int64_t)(((int64_t)UINT64_C(27))));
  t163 = ((t161) == (t162));
  if (t163) {
  {
  t164 = ((int64_t)(v1));
  t165 = ((int64_t)(((int64_t)UINT64_C(11))));
  t166 = fg14_s_record_get(t164, t165);
  t167 = ((int64_t)(t166));
  t168 = fg14_s_vec_len(t167);
  t0 = ((int64_t)(t168));
  goto gg_done;
  }
  }
  t169 = ((int64_t)(v0));
  t170 = ((int64_t)(((int64_t)UINT64_C(28))));
  t171 = ((t169) == (t170));
  if (t171) {
  {
  t172 = ((int64_t)(v1));
  t173 = ((int64_t)(((int64_t)UINT64_C(11))));
  t174 = fg14_s_record_get(t172, t173);
  t175 = ((int64_t)(t174));
  t176 = ((int64_t)(v2));
  t0 = gg_function_16(t175, t176, base + 0);
  goto gg_done;
  }
  }
  t177 = ((int64_t)(v0));
  t178 = ((int64_t)(((int64_t)UINT64_C(29))));
  t179 = ((t177) == (t178));
  if (t179) {
  {
  t180 = ((int64_t)(v1));
  t181 = ((int64_t)(((int64_t)UINT64_C(10))));
  t182 = fg14_s_record_get(t180, t181);
  t183 = ((int64_t)(t182));
  t184 = ((int64_t)(v2));
  t185 = fg14_s_map_get(t183, t184);
  t0 = ((int64_t)(t185));
  goto gg_done;
  }
  }
  t186 = ((int64_t)(v0));
  t187 = ((int64_t)(((int64_t)UINT64_C(30))));
  t188 = ((t186) == (t187));
  if (t188) {
  {
  t189 = ((int64_t)(v1));
  t190 = ((int64_t)(v2));
  t191 = ((int64_t)(v3));
  t0 = gg_function_68(t189, t190, t191, base + 0);
  goto gg_done;
  }
  }
  t192 = ((int64_t)(v0));
  t193 = ((int64_t)(((int64_t)UINT64_C(31))));
  t194 = ((t192) == (t193));
  if (t194) {
  {
  t195 = ((int64_t)(v1));
  t0 = gg_function_69(t195, base + 0);
  goto gg_done;
  }
  }
  t196 = ((int64_t)(v0));
  t197 = ((int64_t)(((int64_t)UINT64_C(32))));
  t198 = ((t196) == (t197));
  if (t198) {
  {
  t199 = ((int64_t)(v1));
  t0 = gg_function_70(t199, base + 0);
  goto gg_done;
  }
  }
  t200 = ((int64_t)UINT64_C(1));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t200)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  gg_header t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  {
  t0.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = gg_function_1(t1, t2, t3, t4, t5, t6, t7, base + 1);
  gg_integer(gg_ref(&t0, gg_get_stack(base + 0)), t8);
  gg_append(gg_ref(&t0, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t0).base, (t0).len));
  }
gg_done:;
  for (t9 = 0; t9 < 1; t9++) gg_stacks[base + t9].top = 0;
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
