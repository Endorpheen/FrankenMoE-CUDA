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
static uint8_t gg_eq_4(float a, float b);
static uint8_t gg_equal_elements_4(gg_slice a, gg_slice b);
static void gg_render_4(gg_reference out, float value);
static uint8_t gg_eq_5(double a, double b);
static uint8_t gg_equal_elements_5(gg_slice a, gg_slice b);
static void gg_render_5(gg_reference out, double value);
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
static uint8_t gg_equal_elements_4(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_4(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_4(float a, float b) {
return a == b;
}
static void gg_render_4(gg_reference out, float value) {
gg_float32(out,value);
}
static uint8_t gg_equal_elements_5(gg_slice a,gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!gg_eq_5(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t gg_eq_5(double a, double b) {
return a == b;
}
static void gg_render_5(gg_reference out, double value) {
gg_float(out,value);
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
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
  int64_t v38;
} gg_globals;
static void gg_function_0(int64_t base);
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_2(int64_t p0, int64_t base);
static void gg_function_6(int64_t p0, int64_t p1, int64_t base);
static float gg_function_9(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static void gg_function_12(uint8_t p0, int64_t base);
static void gg_function_13(int64_t p0, int64_t p1, int64_t p2, float p3, int64_t base);
static int64_t gg_function_16(int64_t p0, int64_t base);
static void gg_function_18(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_19(int64_t p0, int64_t base);
static int64_t gg_function_20(int64_t p0, int64_t base);
static void gg_function_22(int64_t p0, int64_t p1, int64_t base);
static void gg_function_23(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t base);
static void gg_function_28(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static int64_t gg_function_30(int64_t p0, int64_t base);
static void gg_function_32(int64_t p0, int64_t base);
static void gg_function_38(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t gg_function_39(int64_t p0, int64_t base);
static void gg_function_41(int64_t p0, int64_t base);
static void gg_function_44(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static int64_t gg_function_47(int64_t p0, int64_t base);
static void gg_function_49(int64_t p0, int64_t base);
static void gg_function_50(int64_t p0, int64_t p1, int64_t base);
static int64_t gg_function_52(int64_t p0, int64_t base);
static void gg_function_54(int64_t p0, int64_t base);
static int64_t gg_function_55(int64_t p0, int64_t base);
static int64_t gg_function_56(int64_t p0, int64_t base);
static int64_t gg_function_57(int64_t p0, int64_t base);
static int64_t gg_function_58(int64_t p0, int64_t base);
static void gg_function_59(int64_t p0, int64_t base);
static int64_t gg_function_60(int64_t p0, int64_t base);
static void gg_function_61(int64_t p0, int64_t base);
static int64_t gg_function_62(int64_t p0, int64_t base);
static void gg_function_63(int64_t p0, int64_t base);
static void gg_function_12(uint8_t p0, int64_t base) {
  uint8_t t0;
  uint8_t t1;
  uint8_t v0;
  v0 = p0;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  if (!(((uint8_t)UINT64_C(0)))) gg_fail("assertion failed");
  }
  }
  }
gg_done:;
}
static void gg_function_13(int64_t p0, int64_t p1, int64_t p2, float p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  float t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  float t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  float v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v0));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = ((float)(v3));
  fg153_am_store_f32(t3, t4, t5);
  goto gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(gg_globals.v1));
  t8 = ((t6) == (t7));
  if (t8) {
  {
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v2));
  t11 = ((float)(v3));
  t12 = fg153_am_f16_bits(t11);
  t13 = ((int64_t)(t12));
  fg153_am_store_u16(t9, t10, t13);
  goto gg_done;
  }
  }
  t14 = ((uint8_t)UINT64_C(0));
  gg_function_12(t14, base + 0);
  }
gg_done:;
}
static float gg_function_9(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  float t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  float t14;
  uint8_t t15;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(gg_globals.v0));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  t6 = fg153_am_load_f32(t4, t5);
  t0 = ((float)(t6));
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(gg_globals.v1));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(v2));
  t12 = fg153_am_load_u16(t10, t11);
  t13 = ((int64_t)(t12));
  t14 = fg153_am_f16_value(t13);
  t0 = ((float)(t14));
  goto gg_done;
  }
  }
  t15 = ((uint8_t)UINT64_C(0));
  gg_function_12(t15, base + 0);
  t0 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_19(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(gg_globals.v0));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(4))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(gg_globals.v1));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(gg_globals.v2));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((int64_t)(gg_globals.v38));
  goto gg_done;
  }
  }
  t10 = ((uint8_t)UINT64_C(0));
  gg_function_12(t10, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_26(int64_t p0, int64_t p1, int64_t base) {
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
static void gg_function_23(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  float t6;
  float t7;
  float t8;
  float t9;
  float t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  float t14;
  float t15;
  float t16;
  float t17;
  float t18;
  uint8_t t19;
  float t20;
  float t21;
  int64_t t22;
  int64_t t23;
  float t24;
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
  float t35;
  float t36;
  float t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  float t46;
  float t47;
  float t48;
  int64_t t49;
  float t50;
  float t51;
  float t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  float t56;
  float t57;
  float t58;
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
  int64_t v0;
  int64_t v1;
  float v2;
  float v3;
  int64_t v4;
  float v5;
  float v6;
  float v7;
  float v8;
  float v9;
  float v10;
  int64_t v11;
  int64_t v12;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v3 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(gg_globals.v37));
  if (!(((t0) < (t1)))) goto label2;
  {
  t2 = ((int64_t)(v0));
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(((int64_t)UINT64_C(4))));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = fg153_am_load_f32(t2, t5);
  v5 = ((float)(t6));
  t7 = ((float)(v5));
  t8 = fg153_am_fabs(t7);
  v6 = ((float)(t8));
  t9 = ((float)(v2));
  t10 = ((float)(v6));
  t11 = ((t9) < (t10));
  if (t11) {
  {
  v2 = ((float)(v6));
  v3 = ((float)(v5));
  }
  }
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  }
  goto label1;
label2:;
  t14 = ((float)(v3));
  t15 = ((float)(((int64_t)UINT64_C(8))));
  t16 = ((float)((-(t15))));
  v7 = ((float)(((t14) / (t16))));
  t17 = ((float)(v7));
  t18 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t19 = ((t17) != (t18));
  if (t19) {
  {
  t20 = ((float)(((float)(((int64_t)UINT64_C(1))))));
  t21 = ((float)(v7));
  v8 = ((float)(((t20) / (t21))));
  }
  } else {
  {
  v8 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  }
  }
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t24 = ((float)(v7));
  t25 = fg153_am_f16_bits(t24);
  t26 = ((int64_t)(t25));
  fg153_am_store_u16(t22, t23, t26);
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t27 = ((int64_t)(v4));
  t28 = ((int64_t)(gg_globals.v37));
  t29 = ((int64_t)(((int64_t)UINT64_C(2))));
  t30 = ((int64_t)(((int64_t)gg_div(t28, t29))));
  if (!(((t27) < (t30)))) goto label5;
  {
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(v4));
  t33 = ((int64_t)(((int64_t)UINT64_C(4))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = fg153_am_load_f32(t31, t34);
  t36 = ((float)(t35));
  t37 = ((float)(v8));
  v9 = ((float)(((t36) * (t37))));
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(gg_globals.v37));
  t41 = ((int64_t)(((int64_t)UINT64_C(2))));
  t42 = ((int64_t)(((int64_t)gg_div(t40, t41))));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t42)))));
  t44 = ((int64_t)(((int64_t)UINT64_C(4))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = fg153_am_load_f32(t38, t45);
  t47 = ((float)(t46));
  t48 = ((float)(v8));
  v10 = ((float)(((t47) * (t48))));
  t49 = ((int64_t)(((int64_t)UINT64_C(15))));
  t50 = ((float)(v9));
  t51 = ((float)(((float)(gg_double_bits(UINT64_C(4620974692658839552))))));
  t52 = ((float)(((t50) + (t51))));
  t53 = fg153_am_trunc_i64(t52);
  t54 = ((int64_t)(t53));
  v11 = gg_function_26(t49, t54, base + 0);
  t55 = ((int64_t)(((int64_t)UINT64_C(15))));
  t56 = ((float)(v10));
  t57 = ((float)(((float)(gg_double_bits(UINT64_C(4620974692658839552))))));
  t58 = ((float)(((t56) + (t57))));
  t59 = fg153_am_trunc_i64(t58);
  t60 = ((int64_t)(t59));
  v12 = gg_function_26(t55, t60, base + 0);
  t61 = ((int64_t)(v1));
  t62 = ((int64_t)(((int64_t)UINT64_C(2))));
  t63 = ((int64_t)(v4));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  t65 = ((int64_t)(v11));
  t66 = ((int64_t)(((int64_t)UINT64_C(255))));
  t67 = ((int64_t)(((t65) & (t66))));
  t68 = ((int64_t)(v12));
  t69 = ((int64_t)(((int64_t)UINT64_C(255))));
  t70 = ((int64_t)(((t68) & (t69))));
  t71 = ((int64_t)(((int64_t)UINT64_C(4))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) << ((uint64_t)(t71) & 63)))));
  t73 = ((int64_t)(((t67) | (t72))));
  fg153_am_store_u8(t61, t64, t73);
  t74 = ((int64_t)(v4));
  t75 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t74) + (uint64_t)(t75)))));
  }
  goto label4;
label5:;
  }
gg_done:;
}
static void gg_function_50(int64_t p0, int64_t p1, int64_t base) {
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
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint8_t t169;
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
  uint8_t t190;
  uint8_t t191;
  int64_t t192;
  int64_t t193;
  uint8_t t194;
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
  float t207;
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
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v29));
  t2 = fg153_am_param(t0, t1);
  v2 = ((int64_t)(t2));
  v3 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(gg_globals.v2));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(gg_globals.v37));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  }
  }
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(gg_globals.v9));
  t10 = fg153_am_param(t8, t9);
  v4 = ((int64_t)(t10));
  t11 = ((int64_t)(v0));
  t12 = ((int64_t)(gg_globals.v10));
  t13 = fg153_am_param(t11, t12);
  v5 = ((int64_t)(t13));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(gg_globals.v11));
  t16 = fg153_am_param(t14, t15);
  v6 = ((int64_t)(t16));
  t17 = ((int64_t)(v4));
  t18 = ((int64_t)(v5));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(v6));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(v7));
  v8 = ((int64_t)(((int64_t)gg_div(t21, t22))));
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(v8));
  t25 = ((int64_t)(v7));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t23) - (uint64_t)(t26)))));
  t27 = ((int64_t)(v9));
  t28 = ((int64_t)(v4));
  t29 = ((int64_t)(v5));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  v10 = ((int64_t)(((int64_t)gg_div(t27, t30))));
  t31 = ((int64_t)(v9));
  t32 = ((int64_t)(v10));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((int64_t)(v5));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t31) - (uint64_t)(t36)))));
  t37 = ((int64_t)(v9));
  t38 = ((int64_t)(v4));
  v11 = ((int64_t)(((int64_t)gg_div(t37, t38))));
  t39 = ((int64_t)(v9));
  t40 = ((int64_t)(v11));
  t41 = ((int64_t)(v4));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) * (uint64_t)(t41)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t39) - (uint64_t)(t42)))));
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(gg_globals.v18));
  t45 = fg153_am_param(t43, t44);
  v13 = ((int64_t)(t45));
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(gg_globals.v19));
  t48 = fg153_am_param(t46, t47);
  v14 = ((int64_t)(t48));
  t49 = ((int64_t)(v10));
  t50 = ((int64_t)(v13));
  t51 = ((int64_t)(v10));
  t52 = ((int64_t)(v13));
  t53 = ((int64_t)(((int64_t)gg_div(t51, t52))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t53)))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t49) - (uint64_t)(t54)))));
  t55 = ((int64_t)(v8));
  t56 = ((int64_t)(v14));
  t57 = ((int64_t)(v8));
  t58 = ((int64_t)(v14));
  t59 = ((int64_t)(((int64_t)gg_div(t57, t58))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t59)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t55) - (uint64_t)(t60)))));
  v17 = ((int64_t)(v11));
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(gg_globals.v8));
  t63 = fg153_am_param(t61, t62);
  v18 = ((int64_t)(t63));
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(gg_globals.v30));
  t66 = fg153_am_param(t64, t65);
  v19 = ((int64_t)(t66));
  t67 = ((int64_t)(v19));
  t68 = ((int64_t)(gg_globals.v3));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  t70 = ((int64_t)(v18));
  t71 = ((int64_t)(v17));
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(gg_globals.v21));
  t74 = fg153_am_param(t72, t73);
  t75 = ((int64_t)(t74));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t71) * (uint64_t)(t75)))));
  t77 = ((int64_t)(v15));
  t78 = ((int64_t)(v0));
  t79 = ((int64_t)(gg_globals.v22));
  t80 = fg153_am_param(t78, t79);
  t81 = ((int64_t)(t80));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t81)))));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t82)))));
  t84 = ((int64_t)(v16));
  t85 = ((int64_t)(v0));
  t86 = ((int64_t)(gg_globals.v23));
  t87 = fg153_am_param(t85, t86);
  t88 = ((int64_t)(t87));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t88)))));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t83) + (uint64_t)(t89)))));
  t91 = fg153_am_load_i32(t70, t90);
  v20 = ((int64_t)(t91));
  }
  } else {
  {
  t92 = ((int64_t)(v18));
  t93 = ((int64_t)(v17));
  t94 = ((int64_t)(v0));
  t95 = ((int64_t)(gg_globals.v21));
  t96 = fg153_am_param(t94, t95);
  t97 = ((int64_t)(t96));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t93) * (uint64_t)(t97)))));
  t99 = ((int64_t)(v15));
  t100 = ((int64_t)(v0));
  t101 = ((int64_t)(gg_globals.v22));
  t102 = fg153_am_param(t100, t101);
  t103 = ((int64_t)(t102));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t103)))));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t98) + (uint64_t)(t104)))));
  t106 = ((int64_t)(v16));
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(gg_globals.v23));
  t109 = fg153_am_param(t107, t108);
  t110 = ((int64_t)(t109));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t106) * (uint64_t)(t110)))));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t105) + (uint64_t)(t111)))));
  t113 = fg153_am_load_i64(t92, t112);
  v20 = ((int64_t)(t113));
  }
  }
  t114 = ((int64_t)(v0));
  t115 = ((int64_t)(gg_globals.v28));
  t116 = fg153_am_param(t114, t115);
  v21 = ((int64_t)(t116));
  t117 = ((int64_t)(v0));
  t118 = ((int64_t)(gg_globals.v6));
  t119 = fg153_am_param(t117, t118);
  v22 = ((int64_t)(t119));
  t120 = ((int64_t)(v0));
  t121 = ((int64_t)(gg_globals.v7));
  t122 = fg153_am_param(t120, t121);
  v23 = ((int64_t)(t122));
  t123 = ((int64_t)(v11));
  t124 = ((int64_t)(v0));
  t125 = ((int64_t)(gg_globals.v14));
  t126 = fg153_am_param(t124, t125);
  t127 = ((int64_t)(t126));
  t128 = ((int64_t)(((int64_t)((uint64_t)(t123) * (uint64_t)(t127)))));
  t129 = ((int64_t)(v10));
  t130 = ((int64_t)(v0));
  t131 = ((int64_t)(gg_globals.v15));
  t132 = fg153_am_param(t130, t131);
  t133 = ((int64_t)(t132));
  t134 = ((int64_t)(((int64_t)((uint64_t)(t129) * (uint64_t)(t133)))));
  t135 = ((int64_t)(((int64_t)((uint64_t)(t128) + (uint64_t)(t134)))));
  t136 = ((int64_t)(v8));
  t137 = ((int64_t)(v0));
  t138 = ((int64_t)(gg_globals.v16));
  t139 = fg153_am_param(t137, t138);
  t140 = ((int64_t)(t139));
  t141 = ((int64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t140)))));
  t142 = ((int64_t)(((int64_t)((uint64_t)(t135) + (uint64_t)(t141)))));
  t143 = ((int64_t)(v12));
  t144 = ((int64_t)(v21));
  t145 = gg_function_19(t144, base + 0);
  t146 = ((int64_t)(t145));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t143) * (uint64_t)(t146)))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t142) + (uint64_t)(t147)))));
  t148 = ((int64_t)(v20));
  t149 = ((int64_t)(v0));
  t150 = ((int64_t)(gg_globals.v25));
  t151 = fg153_am_param(t149, t150);
  t152 = ((int64_t)(t151));
  t153 = ((int64_t)(((int64_t)((uint64_t)(t148) * (uint64_t)(t152)))));
  t154 = ((int64_t)(v10));
  t155 = ((int64_t)(v0));
  t156 = ((int64_t)(gg_globals.v26));
  t157 = fg153_am_param(t155, t156);
  t158 = ((int64_t)(t157));
  t159 = ((int64_t)(((int64_t)((uint64_t)(t154) * (uint64_t)(t158)))));
  t160 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t159)))));
  t161 = ((int64_t)(v8));
  t162 = ((int64_t)(v0));
  t163 = ((int64_t)(gg_globals.v27));
  t164 = fg153_am_param(t162, t163);
  t165 = ((int64_t)(t164));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t161) * (uint64_t)(t165)))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t160) + (uint64_t)(t166)))));
  t167 = ((int64_t)(v2));
  t168 = ((int64_t)(gg_globals.v2));
  t169 = ((t167) == (t168));
  if (t169) {
  {
  t170 = ((int64_t)(v22));
  t171 = ((int64_t)(v24));
  t172 = ((int64_t)(((int64_t)((uint64_t)(t170) + (uint64_t)(t171)))));
  t173 = ((int64_t)(v23));
  t174 = ((int64_t)(v25));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) + (uint64_t)(t174)))));
  t176 = ((int64_t)(v12));
  t177 = ((int64_t)(gg_globals.v37));
  t178 = ((int64_t)(((int64_t)gg_div(t176, t177))));
  t179 = ((int64_t)(gg_globals.v38));
  t180 = ((int64_t)(((int64_t)((uint64_t)(t178) * (uint64_t)(t179)))));
  t181 = ((int64_t)(((int64_t)((uint64_t)(t175) + (uint64_t)(t180)))));
  gg_function_23(t172, t181, base + 0);
  goto gg_done;
  }
  }
  t182 = ((int64_t)(v25));
  t183 = ((int64_t)(v12));
  t184 = ((int64_t)(v2));
  t185 = gg_function_19(t184, base + 0);
  t186 = ((int64_t)(t185));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t183) * (uint64_t)(t186)))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t182) + (uint64_t)(t187)))));
  t188 = ((int64_t)(v21));
  t189 = ((int64_t)(gg_globals.v1));
  t190 = ((t188) == (t189));
  t191 = t190;
  if (t191) {
  t192 = ((int64_t)(v2));
  t193 = ((int64_t)(gg_globals.v1));
  t191 = ((t192) == (t193));
  }
  t194 = t191;
  if (t194) {
  {
  t195 = ((int64_t)(v23));
  t196 = ((int64_t)(v25));
  t197 = ((int64_t)(v22));
  t198 = ((int64_t)(v24));
  t199 = fg153_am_load_u16(t197, t198);
  t200 = ((int64_t)(t199));
  fg153_am_store_u16(t195, t196, t200);
  goto gg_done;
  }
  }
  t201 = ((int64_t)(v2));
  t202 = ((int64_t)(v23));
  t203 = ((int64_t)(v25));
  t204 = ((int64_t)(v21));
  t205 = ((int64_t)(v22));
  t206 = ((int64_t)(v24));
  t207 = gg_function_9(t204, t205, t206, base + 0);
  gg_function_13(t201, t202, t203, t207, base + 0);
  }
gg_done:;
}
static void gg_function_63(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v31));
  t2 = fg153_am_param(t0, t1);
  v1 = ((int64_t)(t2));
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t3 = ((int64_t)(v2));
  t4 = ((int64_t)(v1));
  if (!(((t3) < (t4)))) goto label2;
  {
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(v2));
  gg_function_50(t5, t6, base + 0);
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_62(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  fg153_am_enter_setrows();
  t1 = ((int64_t)(v0));
  gg_function_63(t1, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_44(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  uint8_t t91;
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
  float t114;
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
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  float t145;
  float t146;
  float t147;
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
  float t163;
  float t164;
  float t165;
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
  float t184;
  int64_t t185;
  int64_t t186;
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
  float v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v9));
  t2 = fg153_am_param(t0, t1);
  v5 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(gg_globals.v19));
  t5 = fg153_am_param(t3, t4);
  v6 = ((int64_t)(t5));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(v6));
  v7 = ((int64_t)(((int64_t)gg_div(t6, t7))));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(v7));
  t10 = ((int64_t)(v6));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t8) - (uint64_t)(t11)))));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(gg_globals.v8));
  t14 = fg153_am_param(t12, t13);
  v9 = ((int64_t)(t14));
  t15 = ((int64_t)(v9));
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(gg_globals.v21));
  t19 = fg153_am_param(t17, t18);
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t20)))));
  t22 = ((int64_t)(v7));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(gg_globals.v22));
  t25 = fg153_am_param(t23, t24);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t26)))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t27)))));
  t29 = ((int64_t)(v8));
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(gg_globals.v23));
  t32 = fg153_am_param(t30, t31);
  t33 = ((int64_t)(t32));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t34)))));
  t36 = fg153_am_load_i32(t15, t35);
  v10 = ((int64_t)(t36));
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(gg_globals.v28));
  t39 = fg153_am_param(t37, t38);
  v11 = ((int64_t)(t39));
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(gg_globals.v29));
  t42 = fg153_am_param(t40, t41);
  v12 = ((int64_t)(t42));
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(gg_globals.v6));
  t45 = fg153_am_param(t43, t44);
  v13 = ((int64_t)(t45));
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(gg_globals.v7));
  t48 = fg153_am_param(t46, t47);
  v14 = ((int64_t)(t48));
  t49 = ((int64_t)(v10));
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(gg_globals.v14));
  t52 = fg153_am_param(t50, t51);
  t53 = ((int64_t)(t52));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t53)))));
  t55 = ((int64_t)(v7));
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(gg_globals.v15));
  t58 = fg153_am_param(t56, t57);
  t59 = ((int64_t)(t58));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t59)))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t60)))));
  t62 = ((int64_t)(v8));
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(gg_globals.v16));
  t65 = fg153_am_param(t63, t64);
  t66 = ((int64_t)(t65));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t66)))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t61) + (uint64_t)(t67)))));
  t68 = ((int64_t)(v1));
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(gg_globals.v25));
  t71 = fg153_am_param(t69, t70);
  t72 = ((int64_t)(t71));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t72)))));
  t74 = ((int64_t)(v7));
  t75 = ((int64_t)(v0));
  t76 = ((int64_t)(gg_globals.v26));
  t77 = fg153_am_param(t75, t76);
  t78 = ((int64_t)(t77));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t74) * (uint64_t)(t78)))));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t79)))));
  t81 = ((int64_t)(v8));
  t82 = ((int64_t)(v0));
  t83 = ((int64_t)(gg_globals.v27));
  t84 = fg153_am_param(t82, t83);
  t85 = ((int64_t)(t84));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t85)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t86)))));
  v17 = ((int64_t)(v3));
label1:;
  t87 = ((int64_t)(v17));
  t88 = ((int64_t)(v5));
  if (!(((t87) < (t88)))) goto label2;
  {
  t89 = ((int64_t)(v11));
  t90 = ((int64_t)(gg_globals.v2));
  t91 = ((t89) == (t90));
  if (t91) {
  {
  t92 = ((int64_t)(v17));
  t93 = ((int64_t)(gg_globals.v37));
  v18 = ((int64_t)(((int64_t)gg_div(t92, t93))));
  t94 = ((int64_t)(v17));
  t95 = ((int64_t)(v17));
  t96 = ((int64_t)(gg_globals.v37));
  t97 = ((int64_t)(((int64_t)gg_div(t95, t96))));
  t98 = ((int64_t)(gg_globals.v37));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t97) * (uint64_t)(t98)))));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t94) - (uint64_t)(t99)))));
  t101 = ((int64_t)(((int64_t)UINT64_C(2))));
  v19 = ((int64_t)(((int64_t)gg_div(t100, t101))));
  t102 = ((int64_t)(v17));
  t103 = ((int64_t)(gg_globals.v37));
  t104 = ((int64_t)(((int64_t)gg_div(t102, t103))));
  t105 = ((int64_t)(gg_globals.v37));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t104) * (uint64_t)(t105)))));
  t106 = ((int64_t)(v13));
  t107 = ((int64_t)(v15));
  t108 = ((int64_t)(v18));
  t109 = ((int64_t)(gg_globals.v38));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t107) + (uint64_t)(t110)))));
  t112 = fg153_am_load_u16(t106, t111);
  t113 = ((int64_t)(t112));
  t114 = fg153_am_f16_value(t113);
  v21 = ((float)(t114));
  t115 = ((int64_t)(v13));
  t116 = ((int64_t)(v15));
  t117 = ((int64_t)(v18));
  t118 = ((int64_t)(gg_globals.v38));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) * (uint64_t)(t118)))));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t116) + (uint64_t)(t119)))));
  t121 = ((int64_t)(((int64_t)UINT64_C(2))));
  t122 = ((int64_t)(((int64_t)((uint64_t)(t120) + (uint64_t)(t121)))));
  t123 = ((int64_t)(v19));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t122) + (uint64_t)(t123)))));
  t125 = fg153_am_load_u8(t115, t124);
  v22 = ((int64_t)(t125));
  t126 = ((int64_t)(v22));
  t127 = ((int64_t)(((int64_t)UINT64_C(15))));
  t128 = ((int64_t)(((t126) & (t127))));
  t129 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t128) - (uint64_t)(t129)))));
  t130 = ((int64_t)(v22));
  t131 = ((int64_t)(((int64_t)UINT64_C(4))));
  t132 = ((int64_t)(((t130) >> ((uint64_t)(t131) & 63))));
  t133 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t132) - (uint64_t)(t133)))));
  t134 = ((int64_t)(v12));
  t135 = ((int64_t)(v14));
  t136 = ((int64_t)(v16));
  t137 = ((int64_t)(v20));
  t138 = ((int64_t)(v19));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t137) + (uint64_t)(t138)))));
  t140 = ((int64_t)(v12));
  t141 = gg_function_19(t140, base + 0);
  t142 = ((int64_t)(t141));
  t143 = ((int64_t)(((int64_t)((uint64_t)(t139) * (uint64_t)(t142)))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t136) + (uint64_t)(t143)))));
  t145 = ((float)(((float)(v23))));
  t146 = ((float)(v21));
  t147 = ((float)(((t145) * (t146))));
  gg_function_13(t134, t135, t144, t147, base + 0);
  t148 = ((int64_t)(v12));
  t149 = ((int64_t)(v14));
  t150 = ((int64_t)(v16));
  t151 = ((int64_t)(v20));
  t152 = ((int64_t)(v19));
  t153 = ((int64_t)(((int64_t)((uint64_t)(t151) + (uint64_t)(t152)))));
  t154 = ((int64_t)(gg_globals.v37));
  t155 = ((int64_t)(((int64_t)UINT64_C(2))));
  t156 = ((int64_t)(((int64_t)gg_div(t154, t155))));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t156)))));
  t158 = ((int64_t)(v12));
  t159 = gg_function_19(t158, base + 0);
  t160 = ((int64_t)(t159));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t157) * (uint64_t)(t160)))));
  t162 = ((int64_t)(((int64_t)((uint64_t)(t150) + (uint64_t)(t161)))));
  t163 = ((float)(((float)(v24))));
  t164 = ((float)(v21));
  t165 = ((float)(((t163) * (t164))));
  gg_function_13(t148, t149, t162, t165, base + 0);
  }
  } else {
  {
  t166 = ((int64_t)(v12));
  t167 = ((int64_t)(v14));
  t168 = ((int64_t)(v16));
  t169 = ((int64_t)(v17));
  t170 = ((int64_t)(v12));
  t171 = gg_function_19(t170, base + 0);
  t172 = ((int64_t)(t171));
  t173 = ((int64_t)(((int64_t)((uint64_t)(t169) * (uint64_t)(t172)))));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t168) + (uint64_t)(t173)))));
  t175 = ((int64_t)(v11));
  t176 = ((int64_t)(v13));
  t177 = ((int64_t)(v15));
  t178 = ((int64_t)(v17));
  t179 = ((int64_t)(v11));
  t180 = gg_function_19(t179, base + 0);
  t181 = ((int64_t)(t180));
  t182 = ((int64_t)(((int64_t)((uint64_t)(t178) * (uint64_t)(t181)))));
  t183 = ((int64_t)(((int64_t)((uint64_t)(t177) + (uint64_t)(t182)))));
  t184 = gg_function_9(t175, t176, t183, base + 0);
  gg_function_13(t166, t167, t174, t184, base + 0);
  }
  }
  t185 = ((int64_t)(v17));
  t186 = ((int64_t)(v4));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t185) + (uint64_t)(t186)))));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static void gg_function_61(int64_t p0, int64_t base) {
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
  int64_t t28;
  int64_t t29;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v18));
  t2 = fg153_am_param(t0, t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(gg_globals.v19));
  t5 = fg153_am_param(t3, t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(gg_globals.v34));
  t8 = fg153_am_param(t6, t7);
  v3 = ((int64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(gg_globals.v28));
  t11 = fg153_am_param(t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(gg_globals.v2));
  t14 = ((t12) == (t13));
  if (t14) {
  {
  v4 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  v4 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  }
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t15 = ((int64_t)(v5));
  t16 = ((int64_t)(v3));
  if (!(((t15) < (t16)))) goto label2;
  {
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(v1));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  if (!(((t17) < (t20)))) goto label5;
  {
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t25 = ((int64_t)(v4));
  gg_function_44(t21, t22, t23, t24, t25, base + 0);
  t26 = ((int64_t)(v6));
  t27 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  }
  goto label4;
label5:;
  t28 = ((int64_t)(v5));
  t29 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_60(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  fg153_am_enter_getrows();
  t1 = ((int64_t)(v0));
  gg_function_61(t1, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_38(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint8_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
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
  float t79;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v28));
  t2 = fg153_am_param(t0, t1);
  v4 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(gg_globals.v29));
  t5 = fg153_am_param(t3, t4);
  v5 = ((int64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(gg_globals.v9));
  t8 = fg153_am_param(t6, t7);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(gg_globals.v10));
  t12 = fg153_am_param(t10, t11);
  t13 = ((int64_t)(t12));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t13)))));
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(gg_globals.v10));
  t20 = fg153_am_param(t18, t19);
  t21 = ((int64_t)(t20));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t22)))));
  t24 = ((int64_t)(v2));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))));
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(v6));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  t28 = ((int64_t)(v2));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(gg_globals.v9));
  t31 = fg153_am_param(t29, t30);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t32)))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t33)))));
  t35 = ((int64_t)(v3));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  t36 = ((int64_t)(v4));
  t37 = ((int64_t)(gg_globals.v1));
  t38 = ((t36) == (t37));
  t39 = t38;
  if (t39) {
  t40 = ((int64_t)(v5));
  t41 = ((int64_t)(gg_globals.v1));
  t39 = ((t40) == (t41));
  }
  t42 = t39;
  if (t42) {
  {
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(gg_globals.v7));
  t45 = fg153_am_param(t43, t44);
  t46 = ((int64_t)(t45));
  t47 = ((int64_t)(v8));
  t48 = ((int64_t)(((int64_t)UINT64_C(2))));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(gg_globals.v6));
  t52 = fg153_am_param(t50, t51);
  t53 = ((int64_t)(t52));
  t54 = ((int64_t)(v7));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  t57 = fg153_am_load_u16(t53, t56);
  t58 = ((int64_t)(t57));
  fg153_am_store_u16(t46, t49, t58);
  goto gg_done;
  }
  }
  t59 = ((int64_t)(v5));
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(gg_globals.v7));
  t62 = fg153_am_param(t60, t61);
  t63 = ((int64_t)(t62));
  t64 = ((int64_t)(v8));
  t65 = ((int64_t)(v5));
  t66 = gg_function_19(t65, base + 0);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t67)))));
  t69 = ((int64_t)(v4));
  t70 = ((int64_t)(v0));
  t71 = ((int64_t)(gg_globals.v6));
  t72 = fg153_am_param(t70, t71);
  t73 = ((int64_t)(t72));
  t74 = ((int64_t)(v7));
  t75 = ((int64_t)(v4));
  t76 = gg_function_19(t75, base + 0);
  t77 = ((int64_t)(t76));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t74) * (uint64_t)(t77)))));
  t79 = gg_function_9(t69, t73, t78, base + 0);
  gg_function_13(t59, t63, t68, t79, base + 0);
  }
gg_done:;
}
static void gg_function_59(int64_t p0, int64_t base) {
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
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  uint8_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  uint8_t t63;
  int64_t t64;
  int64_t t65;
  uint8_t t66;
  uint8_t t67;
  int64_t t68;
  int64_t t69;
  uint8_t t70;
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
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v9));
  t2 = fg153_am_param(t0, t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(gg_globals.v10));
  t5 = fg153_am_param(t3, t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(v2));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(gg_globals.v31));
  t10 = fg153_am_param(t8, t9);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(v3));
  v4 = ((int64_t)(((int64_t)gg_div(t11, t12))));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(31))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)UINT64_C(32))));
  v5 = ((int64_t)(((int64_t)gg_div(t15, t16))));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(31))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = ((int64_t)(((int64_t)UINT64_C(32))));
  v6 = ((int64_t)(((int64_t)gg_div(t19, t20))));
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(((int64_t)UINT64_C(7))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  t24 = ((int64_t)(((int64_t)UINT64_C(8))));
  v7 = ((int64_t)(((int64_t)gg_div(t23, t24))));
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t25 = ((int64_t)(v8));
  t26 = ((int64_t)(v5));
  if (!(((t25) < (t26)))) goto label2;
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t27 = ((int64_t)(v9));
  t28 = ((int64_t)(v6));
  if (!(((t27) < (t28)))) goto label5;
  {
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t29 = ((int64_t)(v10));
  t30 = ((int64_t)(v7));
  if (!(((t29) < (t30)))) goto label8;
  {
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label10:;
  t31 = ((int64_t)(v11));
  t32 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t31) < (t32)))) goto label11;
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label13:;
  t33 = ((int64_t)(v12));
  t34 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t33) < (t34)))) goto label14;
  {
  t35 = ((int64_t)(v9));
  t36 = ((int64_t)(((int64_t)UINT64_C(32))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  t38 = ((int64_t)(v12));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t39 = ((int64_t)(v8));
  t40 = ((int64_t)(((int64_t)UINT64_C(32))));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t42 = ((int64_t)(v11));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t42)))));
  v15 = ((int64_t)(((int64_t)UINT64_C(0))));
label16:;
  t43 = ((int64_t)(v15));
  t44 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t43) < (t44)))) goto label17;
  {
  t45 = ((int64_t)(v10));
  t46 = ((int64_t)(((int64_t)UINT64_C(8))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = ((int64_t)(v15));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t49 = ((int64_t)(v16));
  t50 = ((int64_t)(v4));
  t51 = ((t49) < (t50));
  if (t51) {
  {
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
label19:;
  t52 = ((int64_t)(v17));
  t53 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t52) < (t53)))) goto label20;
  {
  t54 = ((int64_t)(v14));
  t55 = ((int64_t)(v17));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t55)))));
  t56 = ((int64_t)(v13));
  t57 = ((int64_t)(v1));
  t58 = ((t56) < (t57));
  t59 = t58;
  if (t59) {
  t60 = ((int64_t)(v18));
  t61 = ((int64_t)(v2));
  t59 = ((t60) < (t61));
  }
  t62 = t59;
  t63 = t62;
  if (t63) {
  t64 = ((int64_t)(v13));
  t65 = ((int64_t)(v2));
  t63 = ((t64) < (t65));
  }
  t66 = t63;
  t67 = t66;
  if (t67) {
  t68 = ((int64_t)(v18));
  t69 = ((int64_t)(v1));
  t67 = ((t68) < (t69));
  }
  t70 = t67;
  if (t70) {
  {
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(v16));
  t73 = ((int64_t)(v18));
  t74 = ((int64_t)(v13));
  gg_function_38(t71, t72, t73, t74, base + 0);
  }
  }
  t75 = ((int64_t)(v17));
  t76 = ((int64_t)(((int64_t)UINT64_C(8))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t76)))));
  }
  goto label19;
label20:;
  }
  }
  t77 = ((int64_t)(v15));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  }
  goto label16;
label17:;
  t79 = ((int64_t)(v12));
  t80 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  }
  goto label13;
label14:;
  t81 = ((int64_t)(v11));
  t82 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t81) + (uint64_t)(t82)))));
  }
  goto label10;
label11:;
  t83 = ((int64_t)(v10));
  t84 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t83) + (uint64_t)(t84)))));
  }
  goto label7;
label8:;
  t85 = ((int64_t)(v9));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t85) + (uint64_t)(t86)))));
  }
  goto label4;
label5:;
  t87 = ((int64_t)(v8));
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t87) + (uint64_t)(t88)))));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_58(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  fg153_am_enter_transpose();
  t1 = ((int64_t)(v0));
  gg_function_59(t1, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_28(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  float t6;
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
  float t30;
  float t31;
  float t32;
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
  float t43;
  float t44;
  float t45;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  float v6;
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
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(gg_globals.v38));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = fg153_am_load_u16(t0, t3);
  t5 = ((int64_t)(t4));
  t6 = fg153_am_f16_value(t5);
  v6 = ((float)(t6));
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(gg_globals.v38));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  t15 = fg153_am_load_u8(t7, t14);
  v7 = ((int64_t)(t15));
  t16 = ((int64_t)(v7));
  t17 = ((int64_t)(((int64_t)UINT64_C(15))));
  t18 = ((int64_t)(((t16) & (t17))));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t18) - (uint64_t)(t19)))));
  t20 = ((int64_t)(v7));
  t21 = ((int64_t)(((int64_t)UINT64_C(4))));
  t22 = ((int64_t)(((t20) >> ((uint64_t)(t21) & 63))));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t22) - (uint64_t)(t23)))));
  t24 = ((int64_t)(v3));
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)(((int64_t)UINT64_C(4))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t28)))));
  t30 = ((float)(((float)(v8))));
  t31 = ((float)(v6));
  t32 = ((float)(((t30) * (t31))));
  fg153_am_store_f32(t24, t29, t32);
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(v4));
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)(v5));
  t37 = ((int64_t)(((int64_t)UINT64_C(2))));
  t38 = ((int64_t)(((int64_t)gg_div(t36, t37))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)UINT64_C(4))));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t41)))));
  t43 = ((float)(((float)(v9))));
  t44 = ((float)(v6));
  t45 = ((float)(((t43) * (t44))));
  fg153_am_store_f32(t33, t42, t45);
  }
gg_done:;
}
static void gg_function_22(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t134;
  uint8_t t135;
  int64_t t136;
  int64_t t137;
  uint8_t t138;
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
  uint8_t t153;
  uint8_t t154;
  int64_t t155;
  int64_t t156;
  uint8_t t157;
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
  uint8_t t176;
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
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v28));
  t2 = fg153_am_param(t0, t1);
  v2 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(gg_globals.v29));
  t5 = fg153_am_param(t3, t4);
  v3 = ((int64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(gg_globals.v9));
  t8 = fg153_am_param(t6, t7);
  v4 = ((int64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(gg_globals.v10));
  t11 = fg153_am_param(t9, t10);
  v5 = ((int64_t)(t11));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(gg_globals.v11));
  t14 = fg153_am_param(t12, t13);
  v6 = ((int64_t)(t14));
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(v6));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(v7));
  v8 = ((int64_t)(((int64_t)gg_div(t19, t20))));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v8));
  t23 = ((int64_t)(v7));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t21) - (uint64_t)(t24)))));
  t25 = ((int64_t)(v9));
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(v5));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  v10 = ((int64_t)(((int64_t)gg_div(t25, t28))));
  t29 = ((int64_t)(v9));
  t30 = ((int64_t)(v10));
  t31 = ((int64_t)(v4));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t29) - (uint64_t)(t34)))));
  t35 = ((int64_t)(v9));
  t36 = ((int64_t)(v4));
  v11 = ((int64_t)(((int64_t)gg_div(t35, t36))));
  t37 = ((int64_t)(v9));
  t38 = ((int64_t)(v11));
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t37) - (uint64_t)(t40)))));
  t41 = ((int64_t)(v12));
  t42 = ((int64_t)(gg_globals.v37));
  t43 = ((int64_t)(((int64_t)gg_div(t41, t42))));
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(gg_globals.v13));
  t46 = fg153_am_param(t44, t45);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t47)))));
  t49 = ((int64_t)(v11));
  t50 = ((int64_t)(v0));
  t51 = ((int64_t)(gg_globals.v14));
  t52 = fg153_am_param(t50, t51);
  t53 = ((int64_t)(t52));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t53)))));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t54)))));
  t56 = ((int64_t)(v10));
  t57 = ((int64_t)(v0));
  t58 = ((int64_t)(gg_globals.v15));
  t59 = fg153_am_param(t57, t58);
  t60 = ((int64_t)(t59));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t60)))));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t61)))));
  t63 = ((int64_t)(v8));
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(gg_globals.v16));
  t66 = fg153_am_param(t64, t65);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t67)))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t68)))));
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(gg_globals.v17));
  t71 = fg153_am_param(t69, t70);
  v14 = ((int64_t)(t71));
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(gg_globals.v18));
  t74 = fg153_am_param(t72, t73);
  v15 = ((int64_t)(t74));
  t75 = ((int64_t)(v0));
  t76 = ((int64_t)(gg_globals.v19));
  t77 = fg153_am_param(t75, t76);
  v16 = ((int64_t)(t77));
  t78 = ((int64_t)(v14));
  t79 = ((int64_t)(v15));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  t81 = ((int64_t)(v16));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  t82 = ((int64_t)(v1));
  t83 = ((int64_t)(v17));
  v18 = ((int64_t)(((int64_t)gg_div(t82, t83))));
  t84 = ((int64_t)(v1));
  t85 = ((int64_t)(v18));
  t86 = ((int64_t)(v17));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t86)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t84) - (uint64_t)(t87)))));
  t88 = ((int64_t)(v19));
  t89 = ((int64_t)(v14));
  t90 = ((int64_t)(v15));
  t91 = ((int64_t)(((int64_t)((uint64_t)(t89) * (uint64_t)(t90)))));
  v20 = ((int64_t)(((int64_t)gg_div(t88, t91))));
  t92 = ((int64_t)(v19));
  t93 = ((int64_t)(v20));
  t94 = ((int64_t)(v14));
  t95 = ((int64_t)(((int64_t)((uint64_t)(t93) * (uint64_t)(t94)))));
  t96 = ((int64_t)(v15));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t95) * (uint64_t)(t96)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t92) - (uint64_t)(t97)))));
  t98 = ((int64_t)(v19));
  t99 = ((int64_t)(v14));
  v21 = ((int64_t)(((int64_t)gg_div(t98, t99))));
  t100 = ((int64_t)(v19));
  t101 = ((int64_t)(v21));
  t102 = ((int64_t)(v14));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) * (uint64_t)(t102)))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t100) - (uint64_t)(t103)))));
  t104 = ((int64_t)(v22));
  t105 = ((int64_t)(gg_globals.v37));
  t106 = ((int64_t)(((int64_t)gg_div(t104, t105))));
  t107 = ((int64_t)(v0));
  t108 = ((int64_t)(gg_globals.v21));
  t109 = fg153_am_param(t107, t108);
  t110 = ((int64_t)(t109));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t106) * (uint64_t)(t110)))));
  t112 = ((int64_t)(v21));
  t113 = ((int64_t)(v0));
  t114 = ((int64_t)(gg_globals.v22));
  t115 = fg153_am_param(t113, t114);
  t116 = ((int64_t)(t115));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t116)))));
  t118 = ((int64_t)(((int64_t)((uint64_t)(t111) + (uint64_t)(t117)))));
  t119 = ((int64_t)(v20));
  t120 = ((int64_t)(v0));
  t121 = ((int64_t)(gg_globals.v23));
  t122 = fg153_am_param(t120, t121);
  t123 = ((int64_t)(t122));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t119) * (uint64_t)(t123)))));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t118) + (uint64_t)(t124)))));
  t126 = ((int64_t)(v18));
  t127 = ((int64_t)(v0));
  t128 = ((int64_t)(gg_globals.v24));
  t129 = fg153_am_param(t127, t128);
  t130 = ((int64_t)(t129));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t126) * (uint64_t)(t130)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t125) + (uint64_t)(t131)))));
  t132 = ((int64_t)(v2));
  t133 = ((int64_t)(gg_globals.v0));
  t134 = ((t132) == (t133));
  t135 = t134;
  if (t135) {
  t136 = ((int64_t)(v3));
  t137 = ((int64_t)(gg_globals.v2));
  t135 = ((t136) == (t137));
  }
  t138 = t135;
  if (t138) {
  {
  t139 = ((int64_t)(v0));
  t140 = ((int64_t)(gg_globals.v6));
  t141 = fg153_am_param(t139, t140);
  t142 = ((int64_t)(t141));
  t143 = ((int64_t)(v13));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t142) + (uint64_t)(t143)))));
  t145 = ((int64_t)(v0));
  t146 = ((int64_t)(gg_globals.v7));
  t147 = fg153_am_param(t145, t146);
  t148 = ((int64_t)(t147));
  t149 = ((int64_t)(v23));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) + (uint64_t)(t149)))));
  gg_function_23(t144, t150, base + 0);
  goto gg_done;
  }
  }
  t151 = ((int64_t)(v2));
  t152 = ((int64_t)(gg_globals.v2));
  t153 = ((t151) == (t152));
  t154 = t153;
  if (t154) {
  t155 = ((int64_t)(v3));
  t156 = ((int64_t)(gg_globals.v0));
  t154 = ((t155) == (t156));
  }
  t157 = t154;
  if (t157) {
  {
  v24 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t158 = ((int64_t)(v24));
  t159 = ((int64_t)(gg_globals.v37));
  t160 = ((int64_t)(((int64_t)UINT64_C(2))));
  t161 = ((int64_t)(((int64_t)gg_div(t159, t160))));
  if (!(((t158) < (t161)))) goto label2;
  {
  t162 = ((int64_t)(v0));
  t163 = ((int64_t)(gg_globals.v6));
  t164 = fg153_am_param(t162, t163);
  t165 = ((int64_t)(t164));
  t166 = ((int64_t)(((int64_t)UINT64_C(0))));
  t167 = ((int64_t)(v24));
  t168 = ((int64_t)(v0));
  t169 = ((int64_t)(gg_globals.v7));
  t170 = fg153_am_param(t168, t169);
  t171 = ((int64_t)(t170));
  t172 = ((int64_t)(v23));
  t173 = ((int64_t)(gg_globals.v37));
  gg_function_28(t165, t166, t167, t171, t172, t173, base + 0);
  t174 = ((int64_t)(v24));
  t175 = ((int64_t)(((int64_t)UINT64_C(1))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t174) + (uint64_t)(t175)))));
  }
  goto label1;
label2:;
  goto gg_done;
  }
  }
  t176 = ((uint8_t)UINT64_C(0));
  gg_function_12(t176, base + 0);
  }
gg_done:;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  fg153_am_enter_copy_blocks();
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(gg_globals.v31));
  t3 = fg153_am_param(t1, t2);
  v1 = ((int64_t)(t3));
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v1));
  if (!(((t4) < (t5)))) goto label2;
  {
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v2));
  gg_function_22(t6, t7, base + 0);
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(gg_globals.v37));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_18(int64_t p0, int64_t p1, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  uint8_t t9;
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
  float t49;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v28));
  t2 = fg153_am_param(t0, t1);
  v2 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(gg_globals.v29));
  t5 = fg153_am_param(t3, t4);
  v3 = ((int64_t)(t5));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(gg_globals.v1));
  t8 = ((t6) == (t7));
  t9 = t8;
  if (t9) {
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(gg_globals.v1));
  t9 = ((t10) == (t11));
  }
  t12 = t9;
  if (t12) {
  {
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(gg_globals.v7));
  t15 = fg153_am_param(t13, t14);
  t16 = ((int64_t)(t15));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(2))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(gg_globals.v6));
  t22 = fg153_am_param(t20, t21);
  t23 = ((int64_t)(t22));
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(((int64_t)UINT64_C(2))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = fg153_am_load_u16(t23, t26);
  t28 = ((int64_t)(t27));
  fg153_am_store_u16(t16, t19, t28);
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v3));
  t30 = ((int64_t)(v0));
  t31 = ((int64_t)(gg_globals.v7));
  t32 = fg153_am_param(t30, t31);
  t33 = ((int64_t)(t32));
  t34 = ((int64_t)(v1));
  t35 = ((int64_t)(v3));
  t36 = gg_function_19(t35, base + 0);
  t37 = ((int64_t)(t36));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t37)))));
  t39 = ((int64_t)(v2));
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(gg_globals.v6));
  t42 = fg153_am_param(t40, t41);
  t43 = ((int64_t)(t42));
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(v2));
  t46 = gg_function_19(t45, base + 0);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t47)))));
  t49 = gg_function_9(t39, t43, t48, base + 0);
  gg_function_13(t29, t33, t38, t49, base + 0);
  }
gg_done:;
}
static int64_t gg_function_56(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  fg153_am_enter_copy_contig();
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(gg_globals.v31));
  t3 = fg153_am_param(t1, t2);
  v1 = ((int64_t)(t3));
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v1));
  if (!(((t4) < (t5)))) goto label2;
  {
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v2));
  gg_function_18(t6, t7, base + 0);
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_6(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t130;
  uint8_t t131;
  int64_t t132;
  int64_t t133;
  uint8_t t134;
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
  float t159;
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
  v0 = p0;
  v1 = p1;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v28));
  t2 = fg153_am_param(t0, t1);
  v2 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(gg_globals.v29));
  t5 = fg153_am_param(t3, t4);
  v3 = ((int64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(gg_globals.v9));
  t8 = fg153_am_param(t6, t7);
  v4 = ((int64_t)(t8));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(gg_globals.v10));
  t11 = fg153_am_param(t9, t10);
  v5 = ((int64_t)(t11));
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(gg_globals.v11));
  t14 = fg153_am_param(t12, t13);
  v6 = ((int64_t)(t14));
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(gg_globals.v17));
  t17 = fg153_am_param(t15, t16);
  v7 = ((int64_t)(t17));
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(gg_globals.v18));
  t20 = fg153_am_param(t18, t19);
  v8 = ((int64_t)(t20));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(gg_globals.v19));
  t23 = fg153_am_param(t21, t22);
  v9 = ((int64_t)(t23));
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(v6));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v10));
  v11 = ((int64_t)(((int64_t)gg_div(t28, t29))));
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(v11));
  t32 = ((int64_t)(v10));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t30) - (uint64_t)(t33)))));
  t34 = ((int64_t)(v12));
  t35 = ((int64_t)(v4));
  t36 = ((int64_t)(v5));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  v13 = ((int64_t)(((int64_t)gg_div(t34, t37))));
  t38 = ((int64_t)(v12));
  t39 = ((int64_t)(v13));
  t40 = ((int64_t)(v4));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t38) - (uint64_t)(t43)))));
  t44 = ((int64_t)(v12));
  t45 = ((int64_t)(v4));
  v14 = ((int64_t)(((int64_t)gg_div(t44, t45))));
  t46 = ((int64_t)(v12));
  t47 = ((int64_t)(v14));
  t48 = ((int64_t)(v4));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t46) - (uint64_t)(t49)))));
  t50 = ((int64_t)(v15));
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(gg_globals.v13));
  t53 = fg153_am_param(t51, t52);
  t54 = ((int64_t)(t53));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t54)))));
  t56 = ((int64_t)(v14));
  t57 = ((int64_t)(v0));
  t58 = ((int64_t)(gg_globals.v14));
  t59 = fg153_am_param(t57, t58);
  t60 = ((int64_t)(t59));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t60)))));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t61)))));
  t63 = ((int64_t)(v13));
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(gg_globals.v15));
  t66 = fg153_am_param(t64, t65);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t67)))));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t68)))));
  t70 = ((int64_t)(v11));
  t71 = ((int64_t)(v0));
  t72 = ((int64_t)(gg_globals.v16));
  t73 = fg153_am_param(t71, t72);
  t74 = ((int64_t)(t73));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t74)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t69) + (uint64_t)(t75)))));
  t76 = ((int64_t)(v7));
  t77 = ((int64_t)(v8));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t76) * (uint64_t)(t77)))));
  t79 = ((int64_t)(v9));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  t80 = ((int64_t)(v1));
  t81 = ((int64_t)(v17));
  v18 = ((int64_t)(((int64_t)gg_div(t80, t81))));
  t82 = ((int64_t)(v1));
  t83 = ((int64_t)(v18));
  t84 = ((int64_t)(v17));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t83) * (uint64_t)(t84)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t82) - (uint64_t)(t85)))));
  t86 = ((int64_t)(v19));
  t87 = ((int64_t)(v7));
  t88 = ((int64_t)(v8));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t87) * (uint64_t)(t88)))));
  v20 = ((int64_t)(((int64_t)gg_div(t86, t89))));
  t90 = ((int64_t)(v19));
  t91 = ((int64_t)(v20));
  t92 = ((int64_t)(v7));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t92)))));
  t94 = ((int64_t)(v8));
  t95 = ((int64_t)(((int64_t)((uint64_t)(t93) * (uint64_t)(t94)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t90) - (uint64_t)(t95)))));
  t96 = ((int64_t)(v19));
  t97 = ((int64_t)(v7));
  v21 = ((int64_t)(((int64_t)gg_div(t96, t97))));
  t98 = ((int64_t)(v19));
  t99 = ((int64_t)(v21));
  t100 = ((int64_t)(v7));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t100)))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t98) - (uint64_t)(t101)))));
  t102 = ((int64_t)(v22));
  t103 = ((int64_t)(v0));
  t104 = ((int64_t)(gg_globals.v21));
  t105 = fg153_am_param(t103, t104);
  t106 = ((int64_t)(t105));
  t107 = ((int64_t)(((int64_t)((uint64_t)(t102) * (uint64_t)(t106)))));
  t108 = ((int64_t)(v21));
  t109 = ((int64_t)(v0));
  t110 = ((int64_t)(gg_globals.v22));
  t111 = fg153_am_param(t109, t110);
  t112 = ((int64_t)(t111));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t112)))));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t107) + (uint64_t)(t113)))));
  t115 = ((int64_t)(v20));
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(gg_globals.v23));
  t118 = fg153_am_param(t116, t117);
  t119 = ((int64_t)(t118));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t115) * (uint64_t)(t119)))));
  t121 = ((int64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t120)))));
  t122 = ((int64_t)(v18));
  t123 = ((int64_t)(v0));
  t124 = ((int64_t)(gg_globals.v24));
  t125 = fg153_am_param(t123, t124);
  t126 = ((int64_t)(t125));
  t127 = ((int64_t)(((int64_t)((uint64_t)(t122) * (uint64_t)(t126)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t121) + (uint64_t)(t127)))));
  t128 = ((int64_t)(v2));
  t129 = ((int64_t)(gg_globals.v1));
  t130 = ((t128) == (t129));
  t131 = t130;
  if (t131) {
  t132 = ((int64_t)(v3));
  t133 = ((int64_t)(gg_globals.v1));
  t131 = ((t132) == (t133));
  }
  t134 = t131;
  if (t134) {
  {
  t135 = ((int64_t)(v0));
  t136 = ((int64_t)(gg_globals.v7));
  t137 = fg153_am_param(t135, t136);
  t138 = ((int64_t)(t137));
  t139 = ((int64_t)(v23));
  t140 = ((int64_t)(v0));
  t141 = ((int64_t)(gg_globals.v6));
  t142 = fg153_am_param(t140, t141);
  t143 = ((int64_t)(t142));
  t144 = ((int64_t)(v16));
  t145 = fg153_am_load_u16(t143, t144);
  t146 = ((int64_t)(t145));
  fg153_am_store_u16(t138, t139, t146);
  goto gg_done;
  }
  }
  t147 = ((int64_t)(v3));
  t148 = ((int64_t)(v0));
  t149 = ((int64_t)(gg_globals.v7));
  t150 = fg153_am_param(t148, t149);
  t151 = ((int64_t)(t150));
  t152 = ((int64_t)(v23));
  t153 = ((int64_t)(v2));
  t154 = ((int64_t)(v0));
  t155 = ((int64_t)(gg_globals.v6));
  t156 = fg153_am_param(t154, t155);
  t157 = ((int64_t)(t156));
  t158 = ((int64_t)(v16));
  t159 = gg_function_9(t153, t157, t158, base + 0);
  gg_function_13(t147, t151, t152, t159, base + 0);
  }
gg_done:;
}
static int64_t gg_function_55(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  fg153_am_enter_copy_scalar();
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(gg_globals.v31));
  t3 = fg153_am_param(t1, t2);
  v1 = ((int64_t)(t3));
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v1));
  if (!(((t4) < (t5)))) goto label2;
  {
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v2));
  gg_function_6(t6, t7, base + 0);
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_54(int64_t p0, int64_t base) {
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
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  float t32;
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
  float t72;
  float t73;
  float t74;
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
  float t93;
  float t94;
  float t95;
  int64_t t96;
  int64_t t97;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v6));
  t2 = fg153_am_param(t0, t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(gg_globals.v7));
  t5 = fg153_am_param(t3, t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(gg_globals.v31));
  t8 = fg153_am_param(t6, t7);
  v3 = ((int64_t)(t8));
  t9 = fg153_am_bid_x();
  v4 = ((int64_t)(t9));
  t10 = fg153_am_tid_x();
  v5 = ((int64_t)(t10));
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(((int64_t)UINT64_C(8))));
  v6 = ((int64_t)(((int64_t)gg_div(t11, t12))));
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(v5));
  t15 = ((int64_t)(((int64_t)UINT64_C(8))));
  t16 = ((int64_t)(((int64_t)gg_div(t14, t15))));
  t17 = ((int64_t)(((int64_t)UINT64_C(8))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t13) - (uint64_t)(t18)))));
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(((int64_t)UINT64_C(8))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(v7));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  t23 = ((int64_t)(v8));
  t24 = ((int64_t)(v3));
  t25 = ((t23) >= (t24));
  if (t25) {
  {
  goto gg_done;
  }
  }
  t26 = ((int64_t)(v1));
  t27 = ((int64_t)(v8));
  t28 = ((int64_t)(gg_globals.v38));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = fg153_am_load_u16(t26, t29);
  t31 = ((int64_t)(t30));
  t32 = fg153_am_f16_value(t31);
  v9 = ((float)(t32));
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t33 = ((int64_t)(v10));
  t34 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t33) < (t34)))) goto label2;
  {
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(v8));
  t37 = ((int64_t)(gg_globals.v38));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)UINT64_C(2))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  t41 = ((int64_t)(((int64_t)UINT64_C(4))));
  t42 = ((int64_t)(v6));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))));
  t45 = ((int64_t)(v10));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = fg153_am_load_u8(t35, t46);
  v11 = ((int64_t)(t47));
  t48 = ((int64_t)(v11));
  t49 = ((int64_t)(((int64_t)UINT64_C(15))));
  t50 = ((int64_t)(((t48) & (t49))));
  t51 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t50) - (uint64_t)(t51)))));
  t52 = ((int64_t)(v11));
  t53 = ((int64_t)(((int64_t)UINT64_C(4))));
  t54 = ((int64_t)(((t52) >> ((uint64_t)(t53) & 63))));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t54) - (uint64_t)(t55)))));
  t56 = ((int64_t)(v2));
  t57 = ((int64_t)(v4));
  t58 = ((int64_t)(((int64_t)UINT64_C(256))));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  t60 = ((int64_t)(((int64_t)UINT64_C(32))));
  t61 = ((int64_t)(v7));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t62)))));
  t64 = ((int64_t)(((int64_t)UINT64_C(4))));
  t65 = ((int64_t)(v6));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t66)))));
  t68 = ((int64_t)(v10));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)UINT64_C(4))));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((float)(((float)(v12))));
  t73 = ((float)(v9));
  t74 = ((float)(((t72) * (t73))));
  fg153_am_store_f32(t56, t71, t74);
  t75 = ((int64_t)(v2));
  t76 = ((int64_t)(v4));
  t77 = ((int64_t)(((int64_t)UINT64_C(256))));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t76) * (uint64_t)(t77)))));
  t79 = ((int64_t)(((int64_t)UINT64_C(32))));
  t80 = ((int64_t)(v7));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t81)))));
  t83 = ((int64_t)(((int64_t)UINT64_C(4))));
  t84 = ((int64_t)(v6));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t83) * (uint64_t)(t84)))));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t85)))));
  t87 = ((int64_t)(((int64_t)UINT64_C(16))));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))));
  t89 = ((int64_t)(v10));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))));
  t91 = ((int64_t)(((int64_t)UINT64_C(4))));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
  t93 = ((float)(((float)(v13))));
  t94 = ((float)(v9));
  t95 = ((float)(((t93) * (t94))));
  fg153_am_store_f32(t75, t92, t95);
  t96 = ((int64_t)(v10));
  t97 = ((int64_t)(((int64_t)UINT64_C(1))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_52(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  fg153_am_enter_q4_dequant();
  t1 = ((int64_t)(v0));
  gg_function_54(t1, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_49(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v31));
  t2 = fg153_am_param(t0, t1);
  v1 = ((int64_t)(t2));
  t3 = fg153_am_gid_x();
  v2 = ((int64_t)(t3));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(v1));
  t6 = ((t4) >= (t5));
  if (t6) {
  {
  goto gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(v2));
  gg_function_50(t7, t8, base + 0);
  }
gg_done:;
}
static int64_t gg_function_47(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  fg153_am_enter_setrows();
  t1 = ((int64_t)(v0));
  gg_function_49(t1, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_41(int64_t p0, int64_t base) {
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
  uint8_t t20;
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v18));
  t2 = fg153_am_param(t0, t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(gg_globals.v19));
  t5 = fg153_am_param(t3, t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(v2));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t8 = fg153_am_bid_y();
  t9 = ((int64_t)(t8));
  t10 = fg153_am_bdim_x();
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t11)))));
  t13 = fg153_am_tid_x();
  t14 = ((int64_t)(t13));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t14)))));
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(gg_globals.v36));
  t17 = fg153_am_param(t15, t16);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t18) != (t19));
  if (t20) {
  {
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(((int64_t)UINT64_C(2))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  }
  }
  t23 = fg153_am_gdim_y();
  t24 = ((int64_t)(t23));
  t25 = fg153_am_bdim_x();
  t26 = ((int64_t)(t25));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t26)))));
  t27 = fg153_am_bid_z();
  v6 = ((int64_t)(t27));
label1:;
  t28 = ((int64_t)(v6));
  t29 = ((int64_t)(v3));
  if (!(((t28) < (t29)))) goto label2;
  {
  t30 = ((int64_t)(v0));
  t31 = fg153_am_bid_x();
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(v6));
  t34 = ((int64_t)(v4));
  t35 = ((int64_t)(v5));
  gg_function_44(t30, t32, t33, t34, t35, base + 0);
  t36 = ((int64_t)(v6));
  t37 = fg153_am_gdim_z();
  t38 = ((int64_t)(t37));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t38)))));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_39(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  fg153_am_enter_getrows();
  t1 = ((int64_t)(v0));
  gg_function_41(t1, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_32(int64_t p0, int64_t base) {
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
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  uint8_t t50;
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
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(gg_globals.v9));
  t2 = fg153_am_param(t0, t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(gg_globals.v10));
  t5 = fg153_am_param(t3, t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(v2));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(gg_globals.v31));
  t10 = fg153_am_param(t8, t9);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(v3));
  v4 = ((int64_t)(((int64_t)gg_div(t11, t12))));
  t13 = fg153_am_bid_y();
  t14 = ((int64_t)(t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(32))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = fg153_am_tid_x();
  t18 = ((int64_t)(t17));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t18)))));
  t19 = fg153_am_bid_x();
  t20 = ((int64_t)(t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(32))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = fg153_am_tid_y();
  t24 = ((int64_t)(t23));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t24)))));
  v7 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t25 = ((int64_t)(v7));
  t26 = ((int64_t)(((int64_t)UINT64_C(8))));
  if (!(((t25) < (t26)))) goto label2;
  {
  t27 = fg153_am_bid_z();
  t28 = ((int64_t)(t27));
  t29 = ((int64_t)(((int64_t)UINT64_C(8))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(v7));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  t32 = ((int64_t)(v8));
  t33 = ((int64_t)(v4));
  t34 = ((t32) >= (t33));
  if (t34) {
  {
  goto gg_done;
  }
  }
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t35 = ((int64_t)(v9));
  t36 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t35) < (t36)))) goto label5;
  {
  t37 = ((int64_t)(v6));
  t38 = ((int64_t)(v9));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t39 = ((int64_t)(v5));
  t40 = ((int64_t)(v1));
  t41 = ((t39) < (t40));
  t42 = t41;
  if (t42) {
  t43 = ((int64_t)(v10));
  t44 = ((int64_t)(v2));
  t42 = ((t43) < (t44));
  }
  t45 = t42;
  t46 = t45;
  if (t46) {
  t47 = ((int64_t)(v5));
  t48 = ((int64_t)(v2));
  t46 = ((t47) < (t48));
  }
  t49 = t46;
  t50 = t49;
  if (t50) {
  t51 = ((int64_t)(v10));
  t52 = ((int64_t)(v1));
  t50 = ((t51) < (t52));
  }
  t53 = t50;
  if (t53) {
  {
  t54 = ((int64_t)(v0));
  t55 = ((int64_t)(v8));
  t56 = ((int64_t)(v10));
  t57 = ((int64_t)(v5));
  gg_function_38(t54, t55, t56, t57, base + 0);
  }
  }
  t58 = ((int64_t)(v9));
  t59 = ((int64_t)(((int64_t)UINT64_C(8))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t59)))));
  }
  goto label4;
label5:;
  t60 = ((int64_t)(v7));
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  }
  goto label1;
label2:;
  }
gg_done:;
}
static int64_t gg_function_30(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t v0;
  v0 = p0;
  {
  fg153_am_enter_transpose();
  t1 = ((int64_t)(v0));
  gg_function_32(t1, base + 0);
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_20(int64_t p0, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  fg153_am_enter_copy_blocks();
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(gg_globals.v31));
  t3 = fg153_am_param(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = fg153_am_gid_x();
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(gg_globals.v37));
  v2 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(v1));
  t9 = ((t7) < (t8));
  if (t9) {
  {
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(v2));
  gg_function_22(t10, t11, base + 0);
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_16(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  fg153_am_enter_copy_contig();
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(gg_globals.v31));
  t3 = fg153_am_param(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = fg153_am_gid_x();
  v2 = ((int64_t)(t4));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v1));
  t7 = ((t5) < (t6));
  if (t7) {
  {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(v2));
  gg_function_18(t8, t9, base + 0);
  }
  }
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
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  fg153_am_enter_copy_scalar();
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(gg_globals.v31));
  t3 = fg153_am_param(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = fg153_am_gid_x();
  v2 = ((int64_t)(t4));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v1));
  t7 = ((t5) < (t6));
  if (t7) {
  {
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(v2));
  gg_function_6(t8, t9, base + 0);
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static int64_t gg_function_1(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v1));
  t0 = gg_function_2(t4, base + 0);
  goto gg_done;
  }
  }
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)(v1));
  t0 = gg_function_16(t8, base + 0);
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)UINT64_C(3))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v1));
  t0 = gg_function_20(t12, base + 0);
  goto gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v1));
  t0 = gg_function_30(t16, base + 0);
  goto gg_done;
  }
  }
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)UINT64_C(5))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t20 = ((int64_t)(v1));
  t0 = gg_function_39(t20, base + 0);
  goto gg_done;
  }
  }
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(6))));
  t23 = ((t21) == (t22));
  if (t23) {
  {
  t24 = ((int64_t)(v1));
  t0 = gg_function_47(t24, base + 0);
  goto gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)UINT64_C(7))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v1));
  t0 = gg_function_52(t28, base + 0);
  goto gg_done;
  }
  }
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(8))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v1));
  t0 = gg_function_55(t32, base + 0);
  goto gg_done;
  }
  }
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(9))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t36 = ((int64_t)(v1));
  t0 = gg_function_56(t36, base + 0);
  goto gg_done;
  }
  }
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(10))));
  t39 = ((t37) == (t38));
  if (t39) {
  {
  t40 = ((int64_t)(v1));
  t0 = gg_function_57(t40, base + 0);
  goto gg_done;
  }
  }
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(11))));
  t43 = ((t41) == (t42));
  if (t43) {
  {
  t44 = ((int64_t)(v1));
  t0 = gg_function_58(t44, base + 0);
  goto gg_done;
  }
  }
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(12))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  t48 = ((int64_t)(v1));
  t0 = gg_function_60(t48, base + 0);
  goto gg_done;
  }
  }
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(13))));
  t51 = ((t49) == (t50));
  if (t51) {
  {
  t52 = ((int64_t)(v1));
  t0 = gg_function_62(t52, base + 0);
  goto gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
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
  {
  t0.base = gg_get_stack(base + 0)->base + gg_get_stack(base + 0)->top; t0.len = 0;
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = gg_function_1(t1, t2, base + 1);
  gg_integer(gg_ref(&t0, gg_get_stack(base + 0)), t3);
  gg_append(gg_ref(&t0, gg_get_stack(base + 0)), "\n", 1, 1);
  gg_write(stdout, gg_view((t0).base, (t0).len));
  }
gg_done:;
  for (t4 = 0; t4 < 1; t4++) gg_stacks[base + t4].top = 0;
}
static int gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  gg_argc = argc; gg_argv = argv;
  gg_globals.v0 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_globals.v1 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_globals.v2 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_globals.v3 = ((int64_t)(((int64_t)UINT64_C(26))));
  gg_globals.v4 = ((int64_t)(((int64_t)UINT64_C(27))));
  gg_globals.v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_globals.v6 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_globals.v7 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_globals.v8 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_globals.v9 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_globals.v10 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_globals.v11 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_globals.v12 = ((int64_t)(((int64_t)UINT64_C(7))));
  gg_globals.v13 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_globals.v14 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_globals.v15 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_globals.v16 = ((int64_t)(((int64_t)UINT64_C(11))));
  gg_globals.v17 = ((int64_t)(((int64_t)UINT64_C(12))));
  gg_globals.v18 = ((int64_t)(((int64_t)UINT64_C(13))));
  gg_globals.v19 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_globals.v20 = ((int64_t)(((int64_t)UINT64_C(15))));
  gg_globals.v21 = ((int64_t)(((int64_t)UINT64_C(16))));
  gg_globals.v22 = ((int64_t)(((int64_t)UINT64_C(17))));
  gg_globals.v23 = ((int64_t)(((int64_t)UINT64_C(18))));
  gg_globals.v24 = ((int64_t)(((int64_t)UINT64_C(19))));
  gg_globals.v25 = ((int64_t)(((int64_t)UINT64_C(20))));
  gg_globals.v26 = ((int64_t)(((int64_t)UINT64_C(21))));
  gg_globals.v27 = ((int64_t)(((int64_t)UINT64_C(22))));
  gg_globals.v28 = ((int64_t)(((int64_t)UINT64_C(23))));
  gg_globals.v29 = ((int64_t)(((int64_t)UINT64_C(24))));
  gg_globals.v30 = ((int64_t)(((int64_t)UINT64_C(25))));
  gg_globals.v31 = ((int64_t)(((int64_t)UINT64_C(26))));
  gg_globals.v32 = ((int64_t)(((int64_t)UINT64_C(27))));
  gg_globals.v33 = ((int64_t)(((int64_t)UINT64_C(28))));
  gg_globals.v34 = ((int64_t)(((int64_t)UINT64_C(29))));
  gg_globals.v35 = ((int64_t)(((int64_t)UINT64_C(30))));
  gg_globals.v36 = ((int64_t)(((int64_t)UINT64_C(31))));
  gg_globals.v37 = ((int64_t)(((int64_t)UINT64_C(32))));
  gg_globals.v38 = ((int64_t)(((int64_t)UINT64_C(18))));
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
