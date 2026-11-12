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
static uint8_t gg_eq_6(uint32_t a, uint32_t b);
static uint8_t gg_equal_elements_6(gg_slice a, gg_slice b);
static void gg_render_6(gg_reference out, uint32_t value);
static uint8_t gg_eq_7(uint8_t a, uint8_t b);
static uint8_t gg_equal_elements_7(gg_slice a, gg_slice b);
static void gg_render_7(gg_reference out, uint8_t value);
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
static uint8_t gg_equal_elements_6(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_6(((uint32_t*)a.data)[i],((uint32_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_6(uint32_t a, uint32_t b) {
return a == b;
}
static void gg_render_6(gg_reference out, uint32_t value) {
gg_unsigned(out,value);
}
static uint8_t gg_equal_elements_7(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_7(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_7(uint8_t a, uint8_t b) {
return a == b;
}
static void gg_render_7(gg_reference out, uint8_t value) {
gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} gg_globals;
static void gg_function_0(int64_t base);
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int64_t base);
static void gg_function_2(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int64_t base);
static void gg_function_4(uint8_t p0, int64_t p1, int64_t base);
static uint64_t gg_function_6(int64_t p0, int32_t p1, int64_t base);
static uint64_t gg_function_7(int64_t p0, int64_t base);
static void gg_function_8(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int32_t p4, int64_t base);
static void gg_function_17(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int64_t base);
static void gg_function_18(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int64_t base);
static void gg_function_19(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int32_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t base);
static void gg_function_20(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int32_t p20, int32_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t p26, int64_t base);
static void gg_function_21(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int32_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t base);
static void gg_function_22(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int32_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t base);
static void gg_function_23(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int32_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t base);
static void gg_function_4(uint8_t p0, int64_t p1, int64_t base) {
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
  fg175c_fail(t2);
  }
  }
  }
gg_done:;
}
static void gg_function_8(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int32_t p4, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint8_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint32_t t7;
  uint32_t t8;
  int32_t t9;
  int32_t t10;
  uint8_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint32_t t16;
  uint32_t t17;
  int32_t t18;
  int32_t t19;
  uint8_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint32_t t25;
  uint32_t t26;
  int32_t t27;
  int32_t t28;
  uint8_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint8_t t36;
  int64_t t37;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int32_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((int32_t)(v4));
  t1 = ((int32_t)(((int64_t)UINT64_C(1))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  t3 = ((uint64_t)(v2));
  t4 = ((uint64_t)(v3));
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(v1));
  t7 = fg175c_ld_u8(t5, t6);
  t8 = ((uint32_t)(t7));
  fg175c_st_u8(t3, t4, t8);
  }
  } else {
  {
  t9 = ((int32_t)(v4));
  t10 = ((int32_t)(((int64_t)UINT64_C(2))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((uint64_t)(v2));
  t13 = ((uint64_t)(v3));
  t14 = ((uint64_t)(v0));
  t15 = ((uint64_t)(v1));
  t16 = fg175c_ld_u16(t14, t15);
  t17 = ((uint32_t)(t16));
  fg175c_st_u16(t12, t13, t17);
  }
  } else {
  {
  t18 = ((int32_t)(v4));
  t19 = ((int32_t)(((int64_t)UINT64_C(4))));
  t20 = ((t18) == (t19));
  if (t20) {
  {
  t21 = ((uint64_t)(v2));
  t22 = ((uint64_t)(v3));
  t23 = ((uint64_t)(v0));
  t24 = ((uint64_t)(v1));
  t25 = fg175c_ld_u32(t23, t24);
  t26 = ((uint32_t)(t25));
  fg175c_st_u32(t21, t22, t26);
  }
  } else {
  {
  t27 = ((int32_t)(v4));
  t28 = ((int32_t)(((int64_t)UINT64_C(8))));
  t29 = ((t27) == (t28));
  if (t29) {
  {
  t30 = ((uint64_t)(v2));
  t31 = ((uint64_t)(v3));
  t32 = ((uint64_t)(v0));
  t33 = ((uint64_t)(v1));
  t34 = fg175c_ld_u64(t32, t33);
  t35 = ((uint64_t)(t34));
  fg175c_st_u64(t30, t31, t35);
  }
  } else {
  {
  t36 = ((uint8_t)UINT64_C(0));
  t37 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_function_4(t36, t37, base + 0);
  }
  }
  }
  }
  }
  }
  }
  }
  }
gg_done:;
}
static uint64_t gg_function_7(int64_t p0, int64_t base) {
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
  t4 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_function_4(t3, t4, base + 0);
  t0 = ((uint64_t)(((uint64_t)(v0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_20(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int32_t p20, int32_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t p26, int64_t base) {
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
  int64_t t37;
  uint8_t t38;
  uint8_t t39;
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
  uint64_t t65;
  int64_t t66;
  uint64_t t67;
  uint64_t t68;
  int64_t t69;
  uint64_t t70;
  int32_t t71;
  int32_t t72;
  int32_t t73;
  uint8_t t74;
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
  int32_t t91;
  int32_t t92;
  uint8_t t93;
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
  int32_t t110;
  int32_t t111;
  uint8_t t112;
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
  int64_t t127;
  int64_t t128;
  int32_t t129;
  int32_t t130;
  uint8_t t131;
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
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  uint8_t t148;
  int64_t t149;
  uint64_t t150;
  int64_t t151;
  uint64_t t152;
  uint64_t t153;
  int64_t t154;
  uint64_t t155;
  int32_t t156;
  int64_t t157;
  int64_t t158;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
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
  int32_t v20;
  int32_t v21;
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
  v25 = p25;
  v26 = p26;
  {
  fg175c_pdl_sync();
  t0 = ((int64_t)(v26));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v25));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) >= (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v22));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) >= (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v23));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) >= (t13));
  }
  t14 = t11;
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v24));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) >= (t17));
  }
  t18 = t15;
  t19 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_function_4(t18, t19, base + 0);
  v27 = ((int64_t)(v22));
  v28 = ((int64_t)(v23));
  v29 = ((int64_t)(v24));
  v30 = ((int64_t)(v25));
label1:;
  t20 = ((int64_t)(v30));
  t21 = ((int64_t)(v15));
  if (!(((t20) < (t21)))) goto label2;
  {
  t22 = ((int64_t)(v29));
  t23 = ((int64_t)(v19));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((int64_t)(v28));
  t26 = ((int64_t)(v18));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t27)))));
  t29 = ((int64_t)(v27));
  t30 = ((int64_t)(v17));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t31)))));
  t33 = ((int64_t)(v30));
  t34 = ((int64_t)(v16));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t35)))));
  t36 = ((int64_t)(v30));
  t37 = ((int64_t)(v3));
  t38 = ((t36) < (t37));
  t39 = t38;
  if (t39) {
  t40 = ((int64_t)(v27));
  t41 = ((int64_t)(v4));
  t39 = ((t40) < (t41));
  }
  t42 = t39;
  t43 = t42;
  if (t43) {
  t44 = ((int64_t)(v28));
  t45 = ((int64_t)(v5));
  t43 = ((t44) < (t45));
  }
  t46 = t43;
  t47 = t46;
  if (t47) {
  t48 = ((int64_t)(v29));
  t49 = ((int64_t)(v6));
  t47 = ((t48) < (t49));
  }
  t50 = t47;
  if (t50) {
  {
  t51 = ((int64_t)(v29));
  t52 = ((int64_t)(v10));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t54 = ((int64_t)(v28));
  t55 = ((int64_t)(v9));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t56)))));
  t58 = ((int64_t)(v27));
  t59 = ((int64_t)(v8));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t60)))));
  t62 = ((int64_t)(v30));
  t63 = ((int64_t)(v7));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t61) + (uint64_t)(t64)))));
  t65 = ((uint64_t)(v0));
  t66 = ((int64_t)(v32));
  t67 = gg_function_7(t66, base + 0);
  t68 = ((uint64_t)(v2));
  t69 = ((int64_t)(v31));
  t70 = gg_function_7(t69, base + 0);
  t71 = ((int32_t)(v20));
  gg_function_8(t65, t67, t68, t70, t71, base + 0);
  }
  } else {
  {
  v33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((int32_t)(v21));
  t73 = ((int32_t)(((int64_t)UINT64_C(0))));
  t74 = ((t72) == (t73));
  if (t74) {
  {
  t75 = ((int64_t)(v29));
  t76 = ((int64_t)(v14));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t75) * (uint64_t)(t76)))));
  t78 = ((int64_t)(v28));
  t79 = ((int64_t)(v13));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t80)))));
  t82 = ((int64_t)(v27));
  t83 = ((int64_t)(v12));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) * (uint64_t)(t83)))));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t81) + (uint64_t)(t84)))));
  t86 = ((int64_t)(v30));
  t87 = ((int64_t)(v3));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) - (uint64_t)(t87)))));
  t89 = ((int64_t)(v11));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) * (uint64_t)(t89)))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t85) + (uint64_t)(t90)))));
  }
  } else {
  {
  t91 = ((int32_t)(v21));
  t92 = ((int32_t)(((int64_t)UINT64_C(1))));
  t93 = ((t91) == (t92));
  if (t93) {
  {
  t94 = ((int64_t)(v29));
  t95 = ((int64_t)(v14));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t94) * (uint64_t)(t95)))));
  t97 = ((int64_t)(v28));
  t98 = ((int64_t)(v13));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t97) * (uint64_t)(t98)))));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t99)))));
  t101 = ((int64_t)(v27));
  t102 = ((int64_t)(v4));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) - (uint64_t)(t102)))));
  t104 = ((int64_t)(v12));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t104)))));
  t106 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t105)))));
  t107 = ((int64_t)(v30));
  t108 = ((int64_t)(v11));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t107) * (uint64_t)(t108)))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t106) + (uint64_t)(t109)))));
  }
  } else {
  {
  t110 = ((int32_t)(v21));
  t111 = ((int32_t)(((int64_t)UINT64_C(2))));
  t112 = ((t110) == (t111));
  if (t112) {
  {
  t113 = ((int64_t)(v29));
  t114 = ((int64_t)(v14));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t113) * (uint64_t)(t114)))));
  t116 = ((int64_t)(v28));
  t117 = ((int64_t)(v5));
  t118 = ((int64_t)(((int64_t)((uint64_t)(t116) - (uint64_t)(t117)))));
  t119 = ((int64_t)(v13));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t118) * (uint64_t)(t119)))));
  t121 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t120)))));
  t122 = ((int64_t)(v27));
  t123 = ((int64_t)(v12));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t122) * (uint64_t)(t123)))));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t121) + (uint64_t)(t124)))));
  t126 = ((int64_t)(v30));
  t127 = ((int64_t)(v11));
  t128 = ((int64_t)(((int64_t)((uint64_t)(t126) * (uint64_t)(t127)))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t125) + (uint64_t)(t128)))));
  }
  } else {
  {
  t129 = ((int32_t)(v21));
  t130 = ((int32_t)(((int64_t)UINT64_C(3))));
  t131 = ((t129) == (t130));
  if (t131) {
  {
  t132 = ((int64_t)(v29));
  t133 = ((int64_t)(v6));
  t134 = ((int64_t)(((int64_t)((uint64_t)(t132) - (uint64_t)(t133)))));
  t135 = ((int64_t)(v14));
  t136 = ((int64_t)(((int64_t)((uint64_t)(t134) * (uint64_t)(t135)))));
  t137 = ((int64_t)(v28));
  t138 = ((int64_t)(v13));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t137) * (uint64_t)(t138)))));
  t140 = ((int64_t)(((int64_t)((uint64_t)(t136) + (uint64_t)(t139)))));
  t141 = ((int64_t)(v27));
  t142 = ((int64_t)(v12));
  t143 = ((int64_t)(((int64_t)((uint64_t)(t141) * (uint64_t)(t142)))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t140) + (uint64_t)(t143)))));
  t145 = ((int64_t)(v30));
  t146 = ((int64_t)(v11));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t145) * (uint64_t)(t146)))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t144) + (uint64_t)(t147)))));
  }
  } else {
  {
  t148 = ((uint8_t)UINT64_C(0));
  t149 = ((int64_t)(((int64_t)UINT64_C(15))));
  gg_function_4(t148, t149, base + 0);
  }
  }
  }
  }
  }
  }
  }
  }
  t150 = ((uint64_t)(v1));
  t151 = ((int64_t)(v33));
  t152 = gg_function_7(t151, base + 0);
  t153 = ((uint64_t)(v2));
  t154 = ((int64_t)(v31));
  t155 = gg_function_7(t154, base + 0);
  t156 = ((int32_t)(v20));
  gg_function_8(t150, t152, t153, t155, t156, base + 0);
  }
  }
  t157 = v30;
  t158 = ((int64_t)(v26));
  v30 = ((int64_t)((uint64_t)(t157) + (uint64_t)(t158)));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_23(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int32_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
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
  int32_t t20;
  int32_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
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
  int32_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
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
  v25 = p25;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int64_t)(v10));
  t11 = ((int64_t)(v11));
  t12 = ((int64_t)(v12));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)(v14));
  t15 = ((int64_t)(v15));
  t16 = ((int64_t)(v16));
  t17 = ((int64_t)(v17));
  t18 = ((int64_t)(v18));
  t19 = ((int64_t)(v19));
  t20 = ((int32_t)(v20));
  t21 = ((int32_t)(((int64_t)UINT64_C(3))));
  t22 = ((int64_t)(v21));
  t23 = ((int64_t)(v22));
  t24 = ((int64_t)(v23));
  t25 = ((int64_t)(v24));
  t26 = ((int64_t)(v25));
  gg_function_20(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, t20, t21, t22, t23, t24, t25, t26, base + 0);
  }
gg_done:;
}
static void gg_function_22(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int32_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
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
  int32_t t20;
  int32_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
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
  int32_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
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
  v25 = p25;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int64_t)(v10));
  t11 = ((int64_t)(v11));
  t12 = ((int64_t)(v12));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)(v14));
  t15 = ((int64_t)(v15));
  t16 = ((int64_t)(v16));
  t17 = ((int64_t)(v17));
  t18 = ((int64_t)(v18));
  t19 = ((int64_t)(v19));
  t20 = ((int32_t)(v20));
  t21 = ((int32_t)(((int64_t)UINT64_C(2))));
  t22 = ((int64_t)(v21));
  t23 = ((int64_t)(v22));
  t24 = ((int64_t)(v23));
  t25 = ((int64_t)(v24));
  t26 = ((int64_t)(v25));
  gg_function_20(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, t20, t21, t22, t23, t24, t25, t26, base + 0);
  }
gg_done:;
}
static void gg_function_21(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int32_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
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
  int32_t t20;
  int32_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
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
  int32_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
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
  v25 = p25;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int64_t)(v10));
  t11 = ((int64_t)(v11));
  t12 = ((int64_t)(v12));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)(v14));
  t15 = ((int64_t)(v15));
  t16 = ((int64_t)(v16));
  t17 = ((int64_t)(v17));
  t18 = ((int64_t)(v18));
  t19 = ((int64_t)(v19));
  t20 = ((int32_t)(v20));
  t21 = ((int32_t)(((int64_t)UINT64_C(1))));
  t22 = ((int64_t)(v21));
  t23 = ((int64_t)(v22));
  t24 = ((int64_t)(v23));
  t25 = ((int64_t)(v24));
  t26 = ((int64_t)(v25));
  gg_function_20(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, t20, t21, t22, t23, t24, t25, t26, base + 0);
  }
gg_done:;
}
static void gg_function_19(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int32_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
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
  int32_t t20;
  int32_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
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
  int32_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
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
  v25 = p25;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int64_t)(v10));
  t11 = ((int64_t)(v11));
  t12 = ((int64_t)(v12));
  t13 = ((int64_t)(v13));
  t14 = ((int64_t)(v14));
  t15 = ((int64_t)(v15));
  t16 = ((int64_t)(v16));
  t17 = ((int64_t)(v17));
  t18 = ((int64_t)(v18));
  t19 = ((int64_t)(v19));
  t20 = ((int32_t)(v20));
  t21 = ((int32_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(v21));
  t23 = ((int64_t)(v22));
  t24 = ((int64_t)(v23));
  t25 = ((int64_t)(v24));
  t26 = ((int64_t)(v25));
  gg_function_20(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, t20, t21, t22, t23, t24, t25, t26, base + 0);
  }
gg_done:;
}
static uint64_t gg_function_6(int64_t p0, int32_t p1, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t v0;
  int32_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(v1))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t0 = gg_function_7(t3, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_2(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint8_t t2;
  uint8_t t3;
  int32_t t4;
  int32_t t5;
  uint8_t t6;
  uint8_t t7;
  int32_t t8;
  int32_t t9;
  uint8_t t10;
  uint8_t t11;
  int32_t t12;
  int32_t t13;
  uint8_t t14;
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
  int32_t t28;
  int32_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
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
  int32_t t52;
  int32_t t53;
  uint8_t t54;
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
  uint8_t t69;
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
  int32_t t80;
  int32_t t81;
  uint8_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  uint8_t t92;
  int64_t t93;
  int64_t t94;
  int32_t t95;
  int32_t t96;
  int32_t t97;
  uint8_t t98;
  uint64_t t99;
  int64_t t100;
  int32_t t101;
  uint64_t t102;
  uint64_t t103;
  uint64_t t104;
  int32_t t105;
  uint64_t t106;
  int64_t t107;
  int32_t t108;
  uint64_t t109;
  uint64_t t110;
  uint64_t t111;
  int32_t t112;
  int64_t t113;
  int64_t t114;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int32_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int32_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  uint64_t v28;
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
  {
  fg175c_pdl_sync();
  t0 = ((int32_t)(v14));
  t1 = ((int32_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int32_t)(v13));
  t5 = ((int32_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) > (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int32_t)(v11));
  t9 = ((int32_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) >= (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int32_t)(v12));
  t13 = ((int32_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) >= (t13));
  }
  t14 = t11;
  t15 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_function_4(t14, t15, base + 0);
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(v7));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(v8));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t20 = ((int64_t)(((int64_t)(v11))));
  t21 = ((int64_t)(((int64_t)(v14))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)(v12))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))));
  t24 = ((int64_t)(((int64_t)(v13))));
  t25 = ((int64_t)(((int64_t)(v14))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
label1:;
  t26 = ((int64_t)(v16));
  t27 = ((int64_t)(v15));
  if (!(((t26) < (t27)))) goto label2;
  {
  v18 = ((int64_t)(((int64_t)UINT64_C(0))));
  v19 = ((int32_t)(((int64_t)UINT64_C(0))));
  t28 = ((int32_t)(v10));
  t29 = ((int32_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t31 = ((int64_t)(v16));
  t32 = ((int64_t)(v6));
  v20 = ((int64_t)(((int64_t)gg_div(t31, t32))));
  t33 = ((int64_t)(v16));
  t34 = ((int64_t)(v20));
  t35 = ((int64_t)(v6));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t33) - (uint64_t)(t36)))));
  t37 = ((int64_t)(v21));
  t38 = ((int64_t)(v3));
  t39 = ((t37) < (t38));
  if (t39) {
  {
  t40 = ((int64_t)(v20));
  t41 = ((int64_t)(v3));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) * (uint64_t)(t41)))));
  t43 = ((int64_t)(v21));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  v19 = ((int32_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t44 = ((int64_t)(v20));
  t45 = ((int64_t)(v6));
  t46 = ((int64_t)(v3));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) - (uint64_t)(t46)))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t47)))));
  t49 = ((int64_t)(v21));
  t50 = ((int64_t)(v3));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) - (uint64_t)(t50)))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t51)))));
  v19 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  } else {
  {
  t52 = ((int32_t)(v10));
  t53 = ((int32_t)(((int64_t)UINT64_C(1))));
  t54 = ((t52) == (t53));
  if (t54) {
  {
  t55 = ((int64_t)(v6));
  t56 = ((int64_t)(v7));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t57 = ((int64_t)(v6));
  t58 = ((int64_t)(v4));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  t59 = ((int64_t)(v22));
  t60 = ((int64_t)(v23));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t59) - (uint64_t)(t60)))));
  t61 = ((int64_t)(v16));
  t62 = ((int64_t)(v22));
  v25 = ((int64_t)(((int64_t)gg_div(t61, t62))));
  t63 = ((int64_t)(v16));
  t64 = ((int64_t)(v25));
  t65 = ((int64_t)(v22));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t63) - (uint64_t)(t66)))));
  t67 = ((int64_t)(v26));
  t68 = ((int64_t)(v23));
  t69 = ((t67) < (t68));
  if (t69) {
  {
  t70 = ((int64_t)(v25));
  t71 = ((int64_t)(v23));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))));
  t73 = ((int64_t)(v26));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  v19 = ((int32_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t74 = ((int64_t)(v25));
  t75 = ((int64_t)(v24));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t74) * (uint64_t)(t75)))));
  t77 = ((int64_t)(v26));
  t78 = ((int64_t)(v23));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) - (uint64_t)(t78)))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t79)))));
  v19 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  } else {
  {
  t80 = ((int32_t)(v10));
  t81 = ((int32_t)(((int64_t)UINT64_C(2))));
  t82 = ((t80) == (t81));
  if (t82) {
  {
  t83 = ((int64_t)(v6));
  t84 = ((int64_t)(v7));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t83) * (uint64_t)(t84)))));
  t86 = ((int64_t)(v5));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t86)))));
  t87 = ((int64_t)(v16));
  t88 = ((int64_t)(v27));
  t89 = ((t87) < (t88));
  if (t89) {
  {
  v18 = ((int64_t)(v16));
  v19 = ((int32_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  t90 = ((int64_t)(v16));
  t91 = ((int64_t)(v27));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t90) - (uint64_t)(t91)))));
  v19 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  } else {
  {
  t92 = ((uint8_t)UINT64_C(0));
  t93 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_function_4(t92, t93, base + 0);
  }
  }
  }
  }
  }
  }
  t94 = ((int64_t)(v16));
  t95 = ((int32_t)(v9));
  v28 = gg_function_6(t94, t95, base + 0);
  t96 = ((int32_t)(v19));
  t97 = ((int32_t)(((int64_t)UINT64_C(0))));
  t98 = ((t96) == (t97));
  if (t98) {
  {
  t99 = ((uint64_t)(v0));
  t100 = ((int64_t)(v18));
  t101 = ((int32_t)(v9));
  t102 = gg_function_6(t100, t101, base + 0);
  t103 = ((uint64_t)(v2));
  t104 = ((uint64_t)(v28));
  t105 = ((int32_t)(v9));
  gg_function_8(t99, t102, t103, t104, t105, base + 0);
  }
  } else {
  {
  t106 = ((uint64_t)(v1));
  t107 = ((int64_t)(v18));
  t108 = ((int32_t)(v9));
  t109 = gg_function_6(t107, t108, base + 0);
  t110 = ((uint64_t)(v2));
  t111 = ((uint64_t)(v28));
  t112 = ((int32_t)(v9));
  gg_function_8(t106, t109, t110, t111, t112, base + 0);
  }
  }
  t113 = v16;
  t114 = ((int64_t)(v17));
  v16 = ((int64_t)((uint64_t)(t113) + (uint64_t)(t114)));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_18(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int32_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int32_t)(v9));
  t10 = ((int32_t)(((int64_t)UINT64_C(2))));
  t11 = ((int32_t)(v10));
  t12 = ((int32_t)(v11));
  t13 = ((int32_t)(v12));
  t14 = ((int32_t)(v13));
  gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, base + 0);
  }
gg_done:;
}
static void gg_function_17(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int32_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int32_t)(v9));
  t10 = ((int32_t)(((int64_t)UINT64_C(1))));
  t11 = ((int32_t)(v10));
  t12 = ((int32_t)(v11));
  t13 = ((int32_t)(v12));
  t14 = ((int32_t)(v13));
  gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, base + 0);
  }
gg_done:;
}
static void gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int32_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int32_t)(v9));
  t10 = ((int32_t)(((int64_t)UINT64_C(0))));
  t11 = ((int32_t)(v10));
  t12 = ((int32_t)(v11));
  t13 = ((int32_t)(v12));
  t14 = ((int32_t)(v13));
  gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, base + 0);
  }
gg_done:;
}
static void gg_function_0(int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int32_t t23;
  int32_t t24;
  int32_t t25;
  int32_t t26;
  int32_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int32_t t37;
  int32_t t38;
  int32_t t39;
  int32_t t40;
  int32_t t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
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
  int32_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint64_t t68;
  uint64_t t69;
  uint64_t t70;
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
  int32_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  uint64_t t94;
  uint64_t t95;
  uint64_t t96;
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
  int32_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  uint64_t t120;
  uint64_t t121;
  uint64_t t122;
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
  int32_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint64_t v0;
  {
  v0 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v0));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((int32_t)(((int64_t)UINT64_C(1))));
  t10 = ((int32_t)(((int64_t)UINT64_C(0))));
  t11 = ((int32_t)(((int64_t)UINT64_C(0))));
  t12 = ((int32_t)(((int64_t)UINT64_C(1))));
  t13 = ((int32_t)(((int64_t)UINT64_C(1))));
  gg_function_1(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, base + 0);
  t14 = ((uint64_t)(v0));
  t15 = ((uint64_t)(v0));
  t16 = ((uint64_t)(v0));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((int64_t)(((int64_t)UINT64_C(1))));
  t23 = ((int32_t)(((int64_t)UINT64_C(2))));
  t24 = ((int32_t)(((int64_t)UINT64_C(1))));
  t25 = ((int32_t)(((int64_t)UINT64_C(1))));
  t26 = ((int32_t)(((int64_t)UINT64_C(1))));
  t27 = ((int32_t)(((int64_t)UINT64_C(2))));
  gg_function_17(t14, t15, t16, t17, t18, t19, t20, t21, t22, t23, t24, t25, t26, t27, base + 0);
  t28 = ((uint64_t)(v0));
  t29 = ((uint64_t)(v0));
  t30 = ((uint64_t)(v0));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((int64_t)(((int64_t)UINT64_C(0))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((int64_t)(((int64_t)UINT64_C(2))));
  t37 = ((int32_t)(((int64_t)UINT64_C(3))));
  t38 = ((int32_t)(((int64_t)UINT64_C(2))));
  t39 = ((int32_t)(((int64_t)UINT64_C(2))));
  t40 = ((int32_t)(((int64_t)UINT64_C(1))));
  t41 = ((int32_t)(((int64_t)UINT64_C(3))));
  gg_function_18(t28, t29, t30, t31, t32, t33, t34, t35, t36, t37, t38, t39, t40, t41, base + 0);
  t42 = ((uint64_t)(v0));
  t43 = ((uint64_t)(v0));
  t44 = ((uint64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(0))));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = ((int64_t)(((int64_t)UINT64_C(0))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = ((int64_t)(((int64_t)UINT64_C(0))));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((int64_t)(((int64_t)UINT64_C(0))));
  t54 = ((int64_t)(((int64_t)UINT64_C(0))));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((int32_t)(((int64_t)UINT64_C(1))));
  t63 = ((int64_t)(((int64_t)UINT64_C(3))));
  t64 = ((int64_t)(((int64_t)UINT64_C(0))));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((int64_t)(((int64_t)UINT64_C(0))));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_19(t42, t43, t44, t45, t46, t47, t48, t49, t50, t51, t52, t53, t54, t55, t56, t57, t58, t59, t60, t61, t62, t63, t64, t65, t66, t67, base + 0);
  t68 = ((uint64_t)(v0));
  t69 = ((uint64_t)(v0));
  t70 = ((uint64_t)(v0));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  t73 = ((int64_t)(((int64_t)UINT64_C(0))));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((int64_t)(((int64_t)UINT64_C(0))));
  t76 = ((int64_t)(((int64_t)UINT64_C(0))));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((int64_t)(((int64_t)UINT64_C(0))));
  t79 = ((int64_t)(((int64_t)UINT64_C(0))));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = ((int64_t)(((int64_t)UINT64_C(0))));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = ((int64_t)(((int64_t)UINT64_C(0))));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((int32_t)(((int64_t)UINT64_C(1))));
  t89 = ((int64_t)(((int64_t)UINT64_C(4))));
  t90 = ((int64_t)(((int64_t)UINT64_C(0))));
  t91 = ((int64_t)(((int64_t)UINT64_C(0))));
  t92 = ((int64_t)(((int64_t)UINT64_C(0))));
  t93 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_21(t68, t69, t70, t71, t72, t73, t74, t75, t76, t77, t78, t79, t80, t81, t82, t83, t84, t85, t86, t87, t88, t89, t90, t91, t92, t93, base + 0);
  t94 = ((uint64_t)(v0));
  t95 = ((uint64_t)(v0));
  t96 = ((uint64_t)(v0));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((int64_t)(((int64_t)UINT64_C(0))));
  t99 = ((int64_t)(((int64_t)UINT64_C(0))));
  t100 = ((int64_t)(((int64_t)UINT64_C(0))));
  t101 = ((int64_t)(((int64_t)UINT64_C(0))));
  t102 = ((int64_t)(((int64_t)UINT64_C(0))));
  t103 = ((int64_t)(((int64_t)UINT64_C(0))));
  t104 = ((int64_t)(((int64_t)UINT64_C(0))));
  t105 = ((int64_t)(((int64_t)UINT64_C(0))));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t108 = ((int64_t)(((int64_t)UINT64_C(0))));
  t109 = ((int64_t)(((int64_t)UINT64_C(0))));
  t110 = ((int64_t)(((int64_t)UINT64_C(0))));
  t111 = ((int64_t)(((int64_t)UINT64_C(0))));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((int32_t)(((int64_t)UINT64_C(1))));
  t115 = ((int64_t)(((int64_t)UINT64_C(5))));
  t116 = ((int64_t)(((int64_t)UINT64_C(0))));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_22(t94, t95, t96, t97, t98, t99, t100, t101, t102, t103, t104, t105, t106, t107, t108, t109, t110, t111, t112, t113, t114, t115, t116, t117, t118, t119, base + 0);
  t120 = ((uint64_t)(v0));
  t121 = ((uint64_t)(v0));
  t122 = ((uint64_t)(v0));
  t123 = ((int64_t)(((int64_t)UINT64_C(0))));
  t124 = ((int64_t)(((int64_t)UINT64_C(0))));
  t125 = ((int64_t)(((int64_t)UINT64_C(0))));
  t126 = ((int64_t)(((int64_t)UINT64_C(0))));
  t127 = ((int64_t)(((int64_t)UINT64_C(0))));
  t128 = ((int64_t)(((int64_t)UINT64_C(0))));
  t129 = ((int64_t)(((int64_t)UINT64_C(0))));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  t131 = ((int64_t)(((int64_t)UINT64_C(0))));
  t132 = ((int64_t)(((int64_t)UINT64_C(0))));
  t133 = ((int64_t)(((int64_t)UINT64_C(0))));
  t134 = ((int64_t)(((int64_t)UINT64_C(0))));
  t135 = ((int64_t)(((int64_t)UINT64_C(0))));
  t136 = ((int64_t)(((int64_t)UINT64_C(0))));
  t137 = ((int64_t)(((int64_t)UINT64_C(0))));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  t139 = ((int64_t)(((int64_t)UINT64_C(0))));
  t140 = ((int32_t)(((int64_t)UINT64_C(1))));
  t141 = ((int64_t)(((int64_t)UINT64_C(6))));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = ((int64_t)(((int64_t)UINT64_C(0))));
  t144 = ((int64_t)(((int64_t)UINT64_C(0))));
  t145 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_function_23(t120, t121, t122, t123, t124, t125, t126, t127, t128, t129, t130, t131, t132, t133, t134, t135, t136, t137, t138, t139, t140, t141, t142, t143, t144, t145, base + 0);
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
