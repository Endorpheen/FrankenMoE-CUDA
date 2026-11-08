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
static uint8_t gg_eq_4(uint64_t a, uint64_t b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, uint64_t value);
static uint8_t gg_eq_5(int32_t a, int32_t b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, int32_t value);
static uint8_t gg_eq_6(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint8_t value);
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
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(uint64_t a, uint64_t b) {
return a == b;
}
static void gg_render_4(gg_reference out, uint64_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((int32_t*)a.data)[i],((int32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(int32_t a, int32_t b) {
return a == b;
}
static void gg_render_5(gg_reference out, int32_t value) {
gg_integer(out,value);
}
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static uint8_t gg_function_1(int64_t p0, int64_t base);
static void gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_9(int64_t p0, int64_t base);
static int64_t gg_function_11(int64_t p0, int64_t p1, int64_t base);
static void gg_function_13(int64_t p0, int64_t p1, int64_t base);
static uint64_t gg_function_15(int64_t p0, int64_t base);
static uint64_t gg_function_16(uint64_t p0, uint64_t p1, int64_t base);
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
  fg176h_fail(t2);
  }
  }
  }
gg_done:;
}
static uint64_t gg_function_15(int64_t p0, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_function_3(t3, t4, base + 0);
  t0 = ((uint64_t)(((uint64_t)(v0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint64_t gg_function_16(uint64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((uint64_t)(v0));
  }
  } else {
  {
  t0 = ((uint64_t)(v1));
  }
  }
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_11(int64_t p0, int64_t p1, int64_t base) {
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
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = ((int64_t)(v1));
  t4 = fg176h_ti(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_13(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  uint8_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  int64_t t19;
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
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint64_t t45;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v1));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_3(t2, t3, base + 0);
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(4294967295))));
  t6 = ((t4) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_3(t6, t7, base + 0);
  v2 = ((uint64_t)(((uint64_t)(v1))));
  v3 = ((uint64_t)(((int64_t)UINT64_C(0))));
label1:;
  t8 = ((uint64_t)(v3));
  t9 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t10 = ((t8) < (t9));
  t11 = t10;
  if (t11) {
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t13 = ((uint64_t)(v3));
  t14 = ((uint64_t)(((uint64_t)((uint64_t)(t12) << ((uint64_t)(t13) & 63)))));
  t15 = ((uint64_t)(v2));
  t11 = ((t14) < (t15));
  }
  if (!(t11)) goto label2;
  {
  t16 = v3;
  t17 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v3 = ((uint64_t)((uint64_t)(t16) + (uint64_t)(t17)));
  }
  goto label1;
label2:;
  t18 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t19 = ((int64_t)(((int64_t)UINT64_C(32))));
  t20 = ((uint64_t)(((uint64_t)((uint64_t)(t18) << ((uint64_t)(t19) & 63)))));
  t21 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t22 = ((uint64_t)(v3));
  t23 = ((uint64_t)(((uint64_t)((uint64_t)(t21) << ((uint64_t)(t22) & 63)))));
  t24 = ((uint64_t)(v2));
  t25 = ((uint64_t)(((uint64_t)((uint64_t)(t23) - (uint64_t)(t24)))));
  t26 = ((uint64_t)(((uint64_t)((uint64_t)(t20) * (uint64_t)(t25)))));
  t27 = ((uint64_t)(v2));
  t28 = ((uint64_t)(((uint64_t)gg_udiv(t26, t27))));
  t29 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v4 = ((uint64_t)(((uint64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t30 = ((uint64_t)(v4));
  t31 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t32 = ((t30) <= (t31));
  t33 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_function_3(t32, t33, base + 0);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((uint64_t)(v4));
  fg176h_put(t34, t35, t36, t37);
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((int64_t)(((int64_t)UINT64_C(1))));
  t41 = ((uint64_t)(v3));
  fg176h_put(t38, t39, t40, t41);
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = ((int64_t)(((int64_t)UINT64_C(2))));
  t45 = ((uint64_t)(v2));
  fg176h_put(t42, t43, t44, t45);
  }
gg_done:;
}
static int64_t gg_function_8(int64_t p0, int64_t p1, int64_t base) {
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
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = ((int64_t)(v1));
  t4 = fg176h_ti(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_9(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg176h_ti(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = fg176h_tsz(t5);
  t0 = ((int64_t)(t6));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static uint8_t gg_function_1(int64_t p0, int64_t base) {
  uint8_t t0;
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
  uint8_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
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
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
  uint8_t t65;
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
  int64_t t77;
  uint8_t t78;
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
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint8_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint64_t t116;
  uint64_t t117;
  uint64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  uint64_t t122;
  uint64_t t123;
  uint64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint64_t t128;
  uint64_t t129;
  uint64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  uint8_t t138;
  uint8_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  uint8_t t147;
  uint8_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  uint8_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  uint64_t t162;
  uint64_t t163;
  uint64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint64_t t168;
  uint64_t t169;
  uint64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  uint64_t t174;
  uint64_t t175;
  uint64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  uint8_t t184;
  uint8_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  uint8_t t193;
  uint8_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  uint8_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  uint64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  uint64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  uint64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  uint64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  uint64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  uint64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  uint64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  uint64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  uint64_t t248;
  int64_t t249;
  int64_t t250;
  uint8_t t251;
  int64_t t252;
  int64_t t253;
  int32_t t254;
  int32_t t255;
  uint8_t t256;
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
  uint8_t t275;
  uint8_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  uint8_t t284;
  uint8_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  uint8_t t293;
  uint8_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  uint8_t t302;
  uint8_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  uint8_t t311;
  uint8_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  uint8_t t320;
  uint8_t t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  uint8_t t329;
  uint8_t t330;
  uint8_t t331;
  uint8_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  uint8_t t339;
  int64_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  uint64_t t345;
  uint64_t t346;
  int64_t t347;
  uint64_t t348;
  uint64_t t349;
  int64_t t350;
  uint64_t t351;
  uint64_t t352;
  uint64_t t353;
  uint64_t t354;
  uint64_t t355;
  uint64_t t356;
  uint8_t t357;
  int64_t t358;
  int64_t t359;
  int64_t t360;
  int64_t t361;
  uint64_t t362;
  int64_t t363;
  int64_t t364;
  int64_t t365;
  uint64_t t366;
  int64_t t367;
  int64_t t368;
  int64_t t369;
  uint64_t t370;
  int64_t t371;
  int64_t t372;
  int64_t t373;
  uint64_t t374;
  int64_t t375;
  int64_t t376;
  int64_t t377;
  uint64_t t378;
  int64_t t379;
  int64_t t380;
  int64_t t381;
  uint64_t t382;
  int64_t t383;
  int64_t t384;
  int64_t t385;
  uint64_t t386;
  int64_t t387;
  int64_t t388;
  int64_t t389;
  uint64_t t390;
  int64_t t391;
  int64_t t392;
  int64_t t393;
  int64_t t394;
  int64_t t395;
  uint64_t t396;
  uint64_t t397;
  int64_t t398;
  uint64_t t399;
  uint64_t t400;
  int64_t t401;
  uint64_t t402;
  uint64_t t403;
  uint64_t t404;
  uint64_t t405;
  uint64_t t406;
  uint64_t t407;
  uint8_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  uint64_t t413;
  int64_t t414;
  int64_t t415;
  int64_t t416;
  uint64_t t417;
  int64_t t418;
  int64_t t419;
  int64_t t420;
  uint64_t t421;
  int64_t t422;
  int64_t t423;
  int64_t t424;
  uint64_t t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  uint64_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  uint64_t t433;
  int64_t t434;
  int64_t t435;
  int64_t t436;
  uint64_t t437;
  int64_t t438;
  int64_t t439;
  int64_t t440;
  uint64_t t441;
  int64_t t442;
  int64_t t443;
  int64_t t444;
  int64_t t445;
  uint64_t t446;
  int64_t t447;
  int64_t t448;
  int64_t t449;
  int64_t t450;
  uint8_t t451;
  int64_t t452;
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
  uint64_t v19;
  uint64_t v20;
  uint64_t v21;
  uint64_t v22;
  uint64_t v23;
  uint64_t v24;
  uint64_t v25;
  uint64_t v26;
  int32_t v27;
  uint8_t v28;
  int64_t v29;
  int64_t v30;
  int64_t v31;
  uint8_t v32;
  uint8_t v33;
  int64_t v34;
  uint64_t v35;
  uint64_t v36;
  uint64_t v37;
  int64_t v38;
  uint64_t v39;
  uint64_t v40;
  uint64_t v41;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg176h_arg(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(20))));
  gg_function_3(t6, t7, base + 0);
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(5))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg176h_ti(t8, t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg176h_oc(t13);
  t15 = ((int64_t)(t14));
  t16 = ((t12) != (t15));
  if (t16) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg176h_ti(t17, t18, t19);
  v2 = ((int64_t)(t20));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = fg176h_ti(t21, t22, t23);
  v3 = ((int64_t)(t24));
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t25) != (t26));
  t28 = t27;
  if (t28) {
  t29 = ((int64_t)(v3));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t28 = ((t29) != (t30));
  }
  t31 = t28;
  t32 = ((int64_t)(((int64_t)UINT64_C(21))));
  gg_function_3(t31, t32, base + 0);
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg176h_ti(t33, t34, t35);
  t37 = ((int64_t)(t36));
  t38 = ((int64_t)(((int64_t)UINT64_C(8))));
  t39 = fg176h_tc(t38);
  t40 = ((int64_t)(t39));
  t41 = ((t37) != (t40));
  if (t41) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t42 = ((int64_t)(v3));
  t43 = ((int64_t)(((int64_t)UINT64_C(3))));
  t44 = gg_function_8(t42, t43, base + 0);
  t45 = ((int64_t)(t44));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t47 = ((t45) != (t46));
  if (t47) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto gg_done;
  }
  }
  t48 = ((int64_t)(v2));
  t49 = ((int64_t)(((int64_t)UINT64_C(2))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg176h_ti(t48, t49, t50);
  v4 = ((int64_t)(t51));
  t52 = ((int64_t)(v1));
  t53 = ((int64_t)(((int64_t)UINT64_C(2))));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = fg176h_ti(t52, t53, t54);
  v5 = ((int64_t)(t55));
  t56 = ((int64_t)(v2));
  v6 = gg_function_9(t56, base + 0);
  t57 = ((int64_t)(v3));
  v7 = gg_function_9(t57, base + 0);
  t58 = ((int64_t)(v1));
  v8 = gg_function_9(t58, base + 0);
  t59 = ((int64_t)(v2));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = gg_function_11(t59, t60, base + 0);
  t62 = ((int64_t)(t61));
  t63 = ((int64_t)(v6));
  t64 = ((t62) == (t63));
  t65 = t64;
  if (t65) {
  t66 = ((int64_t)(v3));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = gg_function_11(t66, t67, base + 0);
  t69 = ((int64_t)(t68));
  t70 = ((int64_t)(v7));
  t65 = ((t69) == (t70));
  }
  t71 = t65;
  t72 = t71;
  if (t72) {
  t73 = ((int64_t)(v1));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = gg_function_11(t73, t74, base + 0);
  t76 = ((int64_t)(t75));
  t77 = ((int64_t)(v8));
  t72 = ((t76) == (t77));
  }
  t78 = t72;
  t79 = ((int64_t)(((int64_t)UINT64_C(22))));
  gg_function_3(t78, t79, base + 0);
  t80 = ((int64_t)(v2));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  v9 = gg_function_8(t80, t81, base + 0);
  t82 = ((int64_t)(v2));
  t83 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = gg_function_8(t82, t83, base + 0);
  t84 = ((int64_t)(v2));
  t85 = ((int64_t)(((int64_t)UINT64_C(2))));
  v11 = gg_function_8(t84, t85, base + 0);
  t86 = ((int64_t)(v3));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  v12 = gg_function_8(t86, t87, base + 0);
  t88 = ((int64_t)(v3));
  t89 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = gg_function_8(t88, t89, base + 0);
  t90 = ((int64_t)(v3));
  t91 = ((int64_t)(((int64_t)UINT64_C(2))));
  v14 = gg_function_8(t90, t91, base + 0);
  t92 = ((int64_t)(v1));
  t93 = ((int64_t)(((int64_t)UINT64_C(0))));
  v15 = gg_function_8(t92, t93, base + 0);
  t94 = ((int64_t)(v1));
  t95 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = gg_function_8(t94, t95, base + 0);
  t96 = ((int64_t)(v1));
  t97 = ((int64_t)(((int64_t)UINT64_C(2))));
  v17 = gg_function_8(t96, t97, base + 0);
  t98 = ((int64_t)(v14));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = ((t98) > (t99));
  t101 = ((int64_t)(((int64_t)UINT64_C(23))));
  gg_function_3(t100, t101, base + 0);
  t102 = ((int64_t)(v13));
  t103 = ((int64_t)(((int64_t)UINT64_C(4294967295))));
  t104 = ((int64_t)(v14));
  t105 = ((int64_t)(((int64_t)gg_div(t103, t104))));
  t106 = ((t102) <= (t105));
  t107 = ((int64_t)(((int64_t)UINT64_C(24))));
  gg_function_3(t106, t107, base + 0);
  t108 = ((int64_t)(v0));
  t109 = fg176h_plan(t108);
  v18 = ((int64_t)(t109));
  t110 = ((int64_t)(v18));
  t111 = ((int64_t)(v14));
  gg_function_13(t110, t111, base + 0);
  t112 = ((int64_t)(v8));
  v19 = gg_function_15(t112, base + 0);
  t113 = ((int64_t)(v1));
  t114 = ((int64_t)(((int64_t)UINT64_C(1))));
  t115 = gg_function_11(t113, t114, base + 0);
  t116 = gg_function_15(t115, base + 0);
  t117 = ((uint64_t)(t116));
  t118 = ((uint64_t)(v19));
  v20 = ((uint64_t)(((uint64_t)gg_udiv(t117, t118))));
  t119 = ((int64_t)(v1));
  t120 = ((int64_t)(((int64_t)UINT64_C(2))));
  t121 = gg_function_11(t119, t120, base + 0);
  t122 = gg_function_15(t121, base + 0);
  t123 = ((uint64_t)(t122));
  t124 = ((uint64_t)(v19));
  v21 = ((uint64_t)(((uint64_t)gg_udiv(t123, t124))));
  t125 = ((int64_t)(v1));
  t126 = ((int64_t)(((int64_t)UINT64_C(3))));
  t127 = gg_function_11(t125, t126, base + 0);
  t128 = gg_function_15(t127, base + 0);
  t129 = ((uint64_t)(t128));
  t130 = ((uint64_t)(v19));
  v22 = ((uint64_t)(((uint64_t)gg_udiv(t129, t130))));
  t131 = ((int64_t)(v1));
  t132 = ((int64_t)(((int64_t)UINT64_C(1))));
  t133 = gg_function_11(t131, t132, base + 0);
  t134 = ((int64_t)(t133));
  t135 = ((int64_t)(v8));
  t136 = ((int64_t)(((int64_t)gg_mod(t134, t135))));
  t137 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = ((t136) == (t137));
  t139 = t138;
  if (t139) {
  t140 = ((int64_t)(v1));
  t141 = ((int64_t)(((int64_t)UINT64_C(2))));
  t142 = gg_function_11(t140, t141, base + 0);
  t143 = ((int64_t)(t142));
  t144 = ((int64_t)(v8));
  t145 = ((int64_t)(((int64_t)gg_mod(t143, t144))));
  t146 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((t145) == (t146));
  }
  t147 = t139;
  t148 = t147;
  if (t148) {
  t149 = ((int64_t)(v1));
  t150 = ((int64_t)(((int64_t)UINT64_C(3))));
  t151 = gg_function_11(t149, t150, base + 0);
  t152 = ((int64_t)(t151));
  t153 = ((int64_t)(v8));
  t154 = ((int64_t)(((int64_t)gg_mod(t152, t153))));
  t155 = ((int64_t)(((int64_t)UINT64_C(0))));
  t148 = ((t154) == (t155));
  }
  t156 = t148;
  t157 = ((int64_t)(((int64_t)UINT64_C(25))));
  gg_function_3(t156, t157, base + 0);
  t158 = ((int64_t)(v7));
  v23 = gg_function_15(t158, base + 0);
  t159 = ((int64_t)(v3));
  t160 = ((int64_t)(((int64_t)UINT64_C(0))));
  t161 = gg_function_11(t159, t160, base + 0);
  t162 = gg_function_15(t161, base + 0);
  t163 = ((uint64_t)(t162));
  t164 = ((uint64_t)(v23));
  v24 = ((uint64_t)(((uint64_t)gg_udiv(t163, t164))));
  t165 = ((int64_t)(v3));
  t166 = ((int64_t)(((int64_t)UINT64_C(1))));
  t167 = gg_function_11(t165, t166, base + 0);
  t168 = gg_function_15(t167, base + 0);
  t169 = ((uint64_t)(t168));
  t170 = ((uint64_t)(v23));
  v25 = ((uint64_t)(((uint64_t)gg_udiv(t169, t170))));
  t171 = ((int64_t)(v3));
  t172 = ((int64_t)(((int64_t)UINT64_C(2))));
  t173 = gg_function_11(t171, t172, base + 0);
  t174 = gg_function_15(t173, base + 0);
  t175 = ((uint64_t)(t174));
  t176 = ((uint64_t)(v23));
  v26 = ((uint64_t)(((uint64_t)gg_udiv(t175, t176))));
  t177 = ((int64_t)(v3));
  t178 = ((int64_t)(((int64_t)UINT64_C(0))));
  t179 = gg_function_11(t177, t178, base + 0);
  t180 = ((int64_t)(t179));
  t181 = ((int64_t)(v7));
  t182 = ((int64_t)(((int64_t)gg_mod(t180, t181))));
  t183 = ((int64_t)(((int64_t)UINT64_C(0))));
  t184 = ((t182) == (t183));
  t185 = t184;
  if (t185) {
  t186 = ((int64_t)(v3));
  t187 = ((int64_t)(((int64_t)UINT64_C(1))));
  t188 = gg_function_11(t186, t187, base + 0);
  t189 = ((int64_t)(t188));
  t190 = ((int64_t)(v7));
  t191 = ((int64_t)(((int64_t)gg_mod(t189, t190))));
  t192 = ((int64_t)(((int64_t)UINT64_C(0))));
  t185 = ((t191) == (t192));
  }
  t193 = t185;
  t194 = t193;
  if (t194) {
  t195 = ((int64_t)(v3));
  t196 = ((int64_t)(((int64_t)UINT64_C(2))));
  t197 = gg_function_11(t195, t196, base + 0);
  t198 = ((int64_t)(t197));
  t199 = ((int64_t)(v7));
  t200 = ((int64_t)(((int64_t)gg_mod(t198, t199))));
  t201 = ((int64_t)(((int64_t)UINT64_C(0))));
  t194 = ((t200) == (t201));
  }
  t202 = t194;
  t203 = ((int64_t)(((int64_t)UINT64_C(26))));
  gg_function_3(t202, t203, base + 0);
  t204 = ((int64_t)(v18));
  t205 = ((int64_t)(((int64_t)UINT64_C(1))));
  t206 = ((int64_t)(((int64_t)UINT64_C(0))));
  t207 = ((uint64_t)(v20));
  fg176h_put(t204, t205, t206, t207);
  t208 = ((int64_t)(v18));
  t209 = ((int64_t)(((int64_t)UINT64_C(1))));
  t210 = ((int64_t)(((int64_t)UINT64_C(1))));
  t211 = ((uint64_t)(v21));
  fg176h_put(t208, t209, t210, t211);
  t212 = ((int64_t)(v18));
  t213 = ((int64_t)(((int64_t)UINT64_C(1))));
  t214 = ((int64_t)(((int64_t)UINT64_C(2))));
  t215 = ((uint64_t)(v22));
  fg176h_put(t212, t213, t214, t215);
  t216 = ((int64_t)(v18));
  t217 = ((int64_t)(((int64_t)UINT64_C(2))));
  t218 = ((int64_t)(((int64_t)UINT64_C(0))));
  t219 = ((uint64_t)(v24));
  fg176h_put(t216, t217, t218, t219);
  t220 = ((int64_t)(v18));
  t221 = ((int64_t)(((int64_t)UINT64_C(2))));
  t222 = ((int64_t)(((int64_t)UINT64_C(1))));
  t223 = ((uint64_t)(v25));
  fg176h_put(t220, t221, t222, t223);
  t224 = ((int64_t)(v18));
  t225 = ((int64_t)(((int64_t)UINT64_C(2))));
  t226 = ((int64_t)(((int64_t)UINT64_C(2))));
  t227 = ((uint64_t)(v26));
  fg176h_put(t224, t225, t226, t227);
  t228 = ((int64_t)(v18));
  t229 = ((int64_t)(((int64_t)UINT64_C(3))));
  t230 = ((int64_t)(((int64_t)UINT64_C(0))));
  t231 = ((int64_t)(v2));
  t232 = ((int64_t)(((int64_t)UINT64_C(1))));
  t233 = gg_function_11(t231, t232, base + 0);
  t234 = gg_function_15(t233, base + 0);
  fg176h_put(t228, t229, t230, t234);
  t235 = ((int64_t)(v18));
  t236 = ((int64_t)(((int64_t)UINT64_C(3))));
  t237 = ((int64_t)(((int64_t)UINT64_C(1))));
  t238 = ((int64_t)(v2));
  t239 = ((int64_t)(((int64_t)UINT64_C(2))));
  t240 = gg_function_11(t238, t239, base + 0);
  t241 = gg_function_15(t240, base + 0);
  fg176h_put(t235, t236, t237, t241);
  t242 = ((int64_t)(v18));
  t243 = ((int64_t)(((int64_t)UINT64_C(3))));
  t244 = ((int64_t)(((int64_t)UINT64_C(2))));
  t245 = ((int64_t)(v2));
  t246 = ((int64_t)(((int64_t)UINT64_C(3))));
  t247 = gg_function_11(t245, t246, base + 0);
  t248 = gg_function_15(t247, base + 0);
  fg176h_put(t242, t243, t244, t248);
  t249 = ((int64_t)(v4));
  t250 = ((int64_t)(v5));
  t251 = ((t249) == (t250));
  if (t251) {
  {
  t252 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t253 = ((int64_t)(v8));
  v27 = ((int32_t)(((int32_t)(((int64_t)gg_div(t252, t253))))));
  }
  } else {
  {
  v27 = ((int32_t)(((int64_t)UINT64_C(0))));
  }
  }
  v28 = ((uint8_t)UINT64_C(0));
  v29 = ((int64_t)(v9));
  t254 = ((int32_t)(v27));
  t255 = ((int32_t)(((int64_t)UINT64_C(1))));
  t256 = ((t254) > (t255));
  if (t256) {
  {
  t257 = ((int64_t)(v9));
  t258 = ((int64_t)(((int64_t)(v27))));
  v30 = ((int64_t)(((int64_t)gg_div(t257, t258))));
  v29 = ((int64_t)(v30));
  t259 = ((int64_t)(v30));
  t260 = ((int64_t)(((int64_t)UINT64_C(63))));
  t261 = ((int64_t)(((int64_t)((uint64_t)(t259) + (uint64_t)(t260)))));
  t262 = ((int64_t)(((int64_t)UINT64_C(64))));
  v31 = ((int64_t)(((int64_t)gg_div(t261, t262))));
  t263 = ((int64_t)(v31));
  t264 = ((int64_t)(v12));
  t265 = ((int64_t)(((int64_t)((uint64_t)(t263) * (uint64_t)(t264)))));
  t266 = ((int64_t)(v13));
  t267 = ((int64_t)(((int64_t)((uint64_t)(t265) * (uint64_t)(t266)))));
  t268 = ((int64_t)(v14));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t267) * (uint64_t)(t268)))));
  t270 = ((int64_t)(((int64_t)UINT64_C(128))));
  v32 = ((t269) >= (t270));
  t271 = ((int64_t)(v9));
  t272 = ((int64_t)(((int64_t)(v27))));
  t273 = ((int64_t)(((int64_t)gg_mod(t271, t272))));
  t274 = ((int64_t)(((int64_t)UINT64_C(0))));
  t275 = ((t273) == (t274));
  t276 = t275;
  if (t276) {
  t277 = ((int64_t)(v2));
  t278 = ((int64_t)(((int64_t)UINT64_C(1))));
  t279 = gg_function_11(t277, t278, base + 0);
  t280 = ((int64_t)(t279));
  t281 = ((int64_t)(((int64_t)UINT64_C(16))));
  t282 = ((int64_t)(((int64_t)gg_mod(t280, t281))));
  t283 = ((int64_t)(((int64_t)UINT64_C(0))));
  t276 = ((t282) == (t283));
  }
  t284 = t276;
  t285 = t284;
  if (t285) {
  t286 = ((int64_t)(v2));
  t287 = ((int64_t)(((int64_t)UINT64_C(2))));
  t288 = gg_function_11(t286, t287, base + 0);
  t289 = ((int64_t)(t288));
  t290 = ((int64_t)(((int64_t)UINT64_C(16))));
  t291 = ((int64_t)(((int64_t)gg_mod(t289, t290))));
  t292 = ((int64_t)(((int64_t)UINT64_C(0))));
  t285 = ((t291) == (t292));
  }
  t293 = t285;
  t294 = t293;
  if (t294) {
  t295 = ((int64_t)(v2));
  t296 = ((int64_t)(((int64_t)UINT64_C(3))));
  t297 = gg_function_11(t295, t296, base + 0);
  t298 = ((int64_t)(t297));
  t299 = ((int64_t)(((int64_t)UINT64_C(16))));
  t300 = ((int64_t)(((int64_t)gg_mod(t298, t299))));
  t301 = ((int64_t)(((int64_t)UINT64_C(0))));
  t294 = ((t300) == (t301));
  }
  t302 = t294;
  t303 = t302;
  if (t303) {
  t304 = ((int64_t)(v1));
  t305 = ((int64_t)(((int64_t)UINT64_C(1))));
  t306 = gg_function_11(t304, t305, base + 0);
  t307 = ((int64_t)(t306));
  t308 = ((int64_t)(((int64_t)UINT64_C(16))));
  t309 = ((int64_t)(((int64_t)gg_mod(t307, t308))));
  t310 = ((int64_t)(((int64_t)UINT64_C(0))));
  t303 = ((t309) == (t310));
  }
  t311 = t303;
  t312 = t311;
  if (t312) {
  t313 = ((int64_t)(v1));
  t314 = ((int64_t)(((int64_t)UINT64_C(2))));
  t315 = gg_function_11(t313, t314, base + 0);
  t316 = ((int64_t)(t315));
  t317 = ((int64_t)(((int64_t)UINT64_C(16))));
  t318 = ((int64_t)(((int64_t)gg_mod(t316, t317))));
  t319 = ((int64_t)(((int64_t)UINT64_C(0))));
  t312 = ((t318) == (t319));
  }
  t320 = t312;
  t321 = t320;
  if (t321) {
  t322 = ((int64_t)(v1));
  t323 = ((int64_t)(((int64_t)UINT64_C(3))));
  t324 = gg_function_11(t322, t323, base + 0);
  t325 = ((int64_t)(t324));
  t326 = ((int64_t)(((int64_t)UINT64_C(16))));
  t327 = ((int64_t)(((int64_t)gg_mod(t325, t326))));
  t328 = ((int64_t)(((int64_t)UINT64_C(0))));
  t321 = ((t327) == (t328));
  }
  v33 = t321;
  t329 = v32;
  t330 = t329;
  if (t330) {
  t330 = v33;
  }
  t331 = t330;
  t332 = t331;
  if (t332) {
  t333 = ((int64_t)(v0));
  t334 = ((int64_t)(((int64_t)UINT64_C(9))));
  t335 = ((int64_t)(((int64_t)UINT64_C(0))));
  t336 = fg176h_ti(t333, t334, t335);
  t337 = ((int64_t)(t336));
  t338 = ((int64_t)(((int64_t)UINT64_C(1))));
  t332 = ((t337) == (t338));
  }
  v28 = t332;
  }
  }
  t339 = v28;
  if (t339) {
  {
  t340 = ((int64_t)(v29));
  t341 = ((int64_t)(((int64_t)UINT64_C(63))));
  t342 = ((int64_t)(((int64_t)((uint64_t)(t340) + (uint64_t)(t341)))));
  t343 = ((int64_t)(((int64_t)UINT64_C(64))));
  v34 = ((int64_t)(((int64_t)gg_div(t342, t343))));
  t344 = ((int64_t)(v12));
  v35 = gg_function_15(t344, base + 0);
  t345 = ((uint64_t)(((uint64_t)(v34))));
  t346 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(65535))))));
  v36 = gg_function_16(t345, t346, base + 0);
  t347 = ((int64_t)(v13));
  t348 = gg_function_15(t347, base + 0);
  t349 = ((uint64_t)(t348));
  t350 = ((int64_t)(v14));
  t351 = gg_function_15(t350, base + 0);
  t352 = ((uint64_t)(t351));
  t353 = ((uint64_t)(((uint64_t)((uint64_t)(t349) * (uint64_t)(t352)))));
  t354 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(65535))))));
  v37 = gg_function_16(t353, t354, base + 0);
  t355 = ((uint64_t)(v35));
  t356 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2147483647))))));
  t357 = ((t355) <= (t356));
  t358 = ((int64_t)(((int64_t)UINT64_C(30))));
  gg_function_3(t357, t358, base + 0);
  t359 = ((int64_t)(v18));
  t360 = ((int64_t)(((int64_t)UINT64_C(6))));
  t361 = ((int64_t)(((int64_t)UINT64_C(0))));
  t362 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg176h_put(t359, t360, t361, t362);
  t363 = ((int64_t)(v18));
  t364 = ((int64_t)(((int64_t)UINT64_C(7))));
  t365 = ((int64_t)(((int64_t)UINT64_C(0))));
  t366 = ((uint64_t)(((uint64_t)(v29))));
  fg176h_put(t363, t364, t365, t366);
  t367 = ((int64_t)(v18));
  t368 = ((int64_t)(((int64_t)UINT64_C(7))));
  t369 = ((int64_t)(((int64_t)UINT64_C(1))));
  t370 = ((uint64_t)(((uint64_t)(v35))));
  fg176h_put(t367, t368, t369, t370);
  t371 = ((int64_t)(v18));
  t372 = ((int64_t)(((int64_t)UINT64_C(7))));
  t373 = ((int64_t)(((int64_t)UINT64_C(2))));
  t374 = ((uint64_t)(((uint64_t)(v36))));
  fg176h_put(t371, t372, t373, t374);
  t375 = ((int64_t)(v18));
  t376 = ((int64_t)(((int64_t)UINT64_C(7))));
  t377 = ((int64_t)(((int64_t)UINT64_C(3))));
  t378 = ((uint64_t)(((uint64_t)(v37))));
  fg176h_put(t375, t376, t377, t378);
  t379 = ((int64_t)(v18));
  t380 = ((int64_t)(((int64_t)UINT64_C(7))));
  t381 = ((int64_t)(((int64_t)UINT64_C(4))));
  t382 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(64))))));
  fg176h_put(t379, t380, t381, t382);
  t383 = ((int64_t)(v18));
  t384 = ((int64_t)(((int64_t)UINT64_C(7))));
  t385 = ((int64_t)(((int64_t)UINT64_C(5))));
  t386 = ((uint64_t)(((uint64_t)(v13))));
  fg176h_put(t383, t384, t385, t386);
  t387 = ((int64_t)(v18));
  t388 = ((int64_t)(((int64_t)UINT64_C(7))));
  t389 = ((int64_t)(((int64_t)UINT64_C(6))));
  t390 = ((uint64_t)(((uint64_t)(v14))));
  fg176h_put(t387, t388, t389, t390);
  }
  } else {
  {
  t391 = ((int64_t)(v9));
  t392 = ((int64_t)(((int64_t)UINT64_C(63))));
  t393 = ((int64_t)(((int64_t)((uint64_t)(t391) + (uint64_t)(t392)))));
  t394 = ((int64_t)(((int64_t)UINT64_C(64))));
  v38 = ((int64_t)(((int64_t)gg_div(t393, t394))));
  t395 = ((int64_t)(v12));
  v39 = gg_function_15(t395, base + 0);
  t396 = ((uint64_t)(((uint64_t)(v38))));
  t397 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(65535))))));
  v40 = gg_function_16(t396, t397, base + 0);
  t398 = ((int64_t)(v13));
  t399 = gg_function_15(t398, base + 0);
  t400 = ((uint64_t)(t399));
  t401 = ((int64_t)(v14));
  t402 = gg_function_15(t401, base + 0);
  t403 = ((uint64_t)(t402));
  t404 = ((uint64_t)(((uint64_t)((uint64_t)(t400) * (uint64_t)(t403)))));
  t405 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(65535))))));
  v41 = gg_function_16(t404, t405, base + 0);
  t406 = ((uint64_t)(v39));
  t407 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2147483647))))));
  t408 = ((t406) <= (t407));
  t409 = ((int64_t)(((int64_t)UINT64_C(31))));
  gg_function_3(t408, t409, base + 0);
  t410 = ((int64_t)(v18));
  t411 = ((int64_t)(((int64_t)UINT64_C(6))));
  t412 = ((int64_t)(((int64_t)UINT64_C(0))));
  t413 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg176h_put(t410, t411, t412, t413);
  t414 = ((int64_t)(v18));
  t415 = ((int64_t)(((int64_t)UINT64_C(7))));
  t416 = ((int64_t)(((int64_t)UINT64_C(0))));
  t417 = ((uint64_t)(((uint64_t)(v9))));
  fg176h_put(t414, t415, t416, t417);
  t418 = ((int64_t)(v18));
  t419 = ((int64_t)(((int64_t)UINT64_C(7))));
  t420 = ((int64_t)(((int64_t)UINT64_C(1))));
  t421 = ((uint64_t)(((uint64_t)(v39))));
  fg176h_put(t418, t419, t420, t421);
  t422 = ((int64_t)(v18));
  t423 = ((int64_t)(((int64_t)UINT64_C(7))));
  t424 = ((int64_t)(((int64_t)UINT64_C(2))));
  t425 = ((uint64_t)(((uint64_t)(v40))));
  fg176h_put(t422, t423, t424, t425);
  t426 = ((int64_t)(v18));
  t427 = ((int64_t)(((int64_t)UINT64_C(7))));
  t428 = ((int64_t)(((int64_t)UINT64_C(3))));
  t429 = ((uint64_t)(((uint64_t)(v41))));
  fg176h_put(t426, t427, t428, t429);
  t430 = ((int64_t)(v18));
  t431 = ((int64_t)(((int64_t)UINT64_C(7))));
  t432 = ((int64_t)(((int64_t)UINT64_C(4))));
  t433 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(64))))));
  fg176h_put(t430, t431, t432, t433);
  t434 = ((int64_t)(v18));
  t435 = ((int64_t)(((int64_t)UINT64_C(7))));
  t436 = ((int64_t)(((int64_t)UINT64_C(5))));
  t437 = ((uint64_t)(((uint64_t)(v13))));
  fg176h_put(t434, t435, t436, t437);
  t438 = ((int64_t)(v18));
  t439 = ((int64_t)(((int64_t)UINT64_C(7))));
  t440 = ((int64_t)(((int64_t)UINT64_C(6))));
  t441 = ((uint64_t)(((uint64_t)(v14))));
  fg176h_put(t438, t439, t440, t441);
  t442 = ((int64_t)(v18));
  t443 = ((int64_t)(((int64_t)UINT64_C(7))));
  t444 = ((int64_t)(((int64_t)UINT64_C(7))));
  t445 = ((int64_t)(v8));
  t446 = gg_function_15(t445, base + 0);
  fg176h_put(t442, t443, t444, t446);
  }
  }
  t447 = ((int64_t)(v0));
  t448 = fg176h_launch(t447);
  t449 = ((int64_t)(t448));
  t450 = ((int64_t)(((int64_t)UINT64_C(0))));
  t451 = ((t449) == (t450));
  t452 = ((int64_t)(((int64_t)UINT64_C(40))));
  gg_function_3(t451, t452, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(int64_t base) {
  int64_t t0;
  uint8_t t1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = gg_function_1(t0, base + 0);
  (void)(t1);
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
