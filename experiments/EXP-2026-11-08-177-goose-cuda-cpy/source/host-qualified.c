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
typedef struct { void *data; int64_t len; } fg177cs_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg177cs_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg177cs_gg_stack;
typedef struct { fg177cs_gg_header *header; fg177cs_gg_stack *stack; } fg177cs_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg177cs_gg_stack fg177cs_gg_stacks[GG_STACK_COUNT];
static int fg177cs_gg_argc;
static char **fg177cs_gg_argv;
static void fg177cs_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg177cs_gg_stack *fg177cs_gg_get_stack(int64_t id) {
    fg177cs_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg177cs_gg_fail("data stack limit exceeded");
    s = &fg177cs_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg177cs_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg177cs_gg_allocate(fg177cs_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg177cs_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg177cs_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg177cs_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg177cs_gg_reference fg177cs_gg_ref(fg177cs_gg_header *h, fg177cs_gg_stack *s) { fg177cs_gg_reference r; r.header=h; r.stack=s; return r; }
static fg177cs_gg_slice fg177cs_gg_view(void *p, int64_t n) { fg177cs_gg_slice v; v.data=p; v.len=n; return v; }
static fg177cs_gg_slice fg177cs_gg_byte_view(fg177cs_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg177cs_gg_fail("byte view size overflow");
    return fg177cs_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg177cs_gg_slice fg177cs_gg_sub(fg177cs_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg177cs_gg_fail("slice out of bounds");
    return fg177cs_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg177cs_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg177cs_gg_fail("array index out of bounds"); return i; }
static void fg177cs_gg_append(fg177cs_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg177cs_gg_fail("invalid array size");
    to=fg177cs_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg177cs_gg_resize(fg177cs_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg177cs_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg177cs_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg177cs_gg_fail("invalid array size");
        p=fg177cs_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg177cs_gg_div(int64_t a, int64_t b) { if (!b) fg177cs_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg177cs_gg_fail("division overflow"); return a/b; }
static int64_t fg177cs_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg177cs_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg177cs_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg177cs_gg_fail("division by zero");return a/b; }
static uint64_t fg177cs_gg_umod(uint64_t a,uint64_t b) { if(!b)fg177cs_gg_fail("division by zero");return a%b; }
static int fg177cs_gg_equal(fg177cs_gg_slice a, fg177cs_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg177cs_gg_text(fg177cs_gg_reference out, fg177cs_gg_slice v) { fg177cs_gg_append(out,v.data,v.len,1); }
static void fg177cs_gg_integer(fg177cs_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg177cs_gg_append(out,b,k,1); }
static void fg177cs_gg_unsigned(fg177cs_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg177cs_gg_append(out,b,k,1); }
static double fg177cs_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t ggf_fmt_f64(uint8_t *out,double n);
static int64_t ggf_fmt_f32(uint8_t *out,float n);
static void fg177cs_gg_float(fg177cs_gg_reference out,double n) { uint8_t b[64];int64_t k=ggf_fmt_f64(b,n);fg177cs_gg_append(out,b,k,1); }
static void fg177cs_gg_float32(fg177cs_gg_reference out,float n) { uint8_t b[64];int64_t k=ggf_fmt_f32(b,n);fg177cs_gg_append(out,b,k,1); }
static void fg177cs_gg_bool(fg177cs_gg_reference out,uint8_t n) { fg177cs_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg177cs_gg_write(FILE *file,fg177cs_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg177cs_gg_fail("write failed"); }
static char *fg177cs_gg_cstring(fg177cs_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg177cs_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t gs_os_read_file(fg177cs_gg_slice path,fg177cs_gg_reference out) {
    char *p=fg177cs_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg177cs_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t gs_os_write_file(fg177cs_gg_slice path,fg177cs_gg_slice data) {
    char *p=fg177cs_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t gs_os_file_exists(fg177cs_gg_slice path) { char*p=fg177cs_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void gs_os_write_stdout(fg177cs_gg_slice s) { fg177cs_gg_write(stdout,s); }
static void gs_os_write_stderr(fg177cs_gg_slice s) { fg177cs_gg_write(stderr,s); }
static void gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t gs_os_arg_count(void) { return fg177cs_gg_argc; }
static void gs_os_arg(int64_t i,fg177cs_gg_reference out) { if(i>=0 && i<fg177cs_gg_argc)fg177cs_gg_append(out,fg177cs_gg_argv[i],(int64_t)strlen(fg177cs_gg_argv[i]),1); }
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


static void fg177cs_gg_quote(fg177cs_gg_reference out,fg177cs_gg_slice text) { int64_t i; unsigned char c; fg177cs_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg177cs_gg_append(out,"\\",1,1);fg177cs_gg_append(out,&c,1,1);} else if(c=='\n')fg177cs_gg_append(out,"\\n",2,1);else if(c=='\r')fg177cs_gg_append(out,"\\r",2,1);else if(c=='\t')fg177cs_gg_append(out,"\\t",2,1);else fg177cs_gg_append(out,&c,1,1);}fg177cs_gg_append(out,"\"",1,1);}
static uint8_t fg177cs_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg177cs_gg_equal_elements_1(fg177cs_gg_slice a, fg177cs_gg_slice b);
static void fg177cs_gg_render_1(fg177cs_gg_reference out, int64_t value);
static uint8_t fg177cs_gg_eq_2(uint8_t a, uint8_t b);
static uint8_t fg177cs_gg_equal_elements_2(fg177cs_gg_slice a, fg177cs_gg_slice b);
static void fg177cs_gg_render_2(fg177cs_gg_reference out, uint8_t value);
static uint8_t fg177cs_gg_eq_4(uint64_t a, uint64_t b);
static uint8_t fg177cs_gg_equal_elements_4(fg177cs_gg_slice a, fg177cs_gg_slice b);
static void fg177cs_gg_render_4(fg177cs_gg_reference out, uint64_t value);
static uint8_t fg177cs_gg_eq_5(uint8_t a, uint8_t b);
static uint8_t fg177cs_gg_equal_elements_5(fg177cs_gg_slice a, fg177cs_gg_slice b);
static void fg177cs_gg_render_5(fg177cs_gg_reference out, uint8_t value);
static uint8_t fg177cs_gg_equal_elements_1(fg177cs_gg_slice a,fg177cs_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg177cs_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg177cs_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg177cs_gg_render_1(fg177cs_gg_reference out, int64_t value) {
fg177cs_gg_integer(out,value);
}
static uint8_t fg177cs_gg_equal_elements_2(fg177cs_gg_slice a,fg177cs_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg177cs_gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg177cs_gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void fg177cs_gg_render_2(fg177cs_gg_reference out, uint8_t value) {
fg177cs_gg_bool(out,value);
}
static uint8_t fg177cs_gg_equal_elements_4(fg177cs_gg_slice a,fg177cs_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg177cs_gg_eq_4(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg177cs_gg_eq_4(uint64_t a, uint64_t b) {
return a == b;
}
static void fg177cs_gg_render_4(fg177cs_gg_reference out, uint64_t value) {
fg177cs_gg_unsigned(out,value);
}
static uint8_t fg177cs_gg_equal_elements_5(fg177cs_gg_slice a,fg177cs_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg177cs_gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg177cs_gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void fg177cs_gg_render_5(fg177cs_gg_reference out, uint8_t value) {
fg177cs_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg177cs_gg_globals;
static void fg177cs_gg_function_0(int64_t base);
static uint8_t fg177cs_gg_function_1(int64_t p0, int64_t base);
static void fg177cs_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static uint64_t fg177cs_gg_function_8(int64_t p0, int64_t base);
static int64_t fg177cs_gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t fg177cs_gg_function_11(int64_t p0, int64_t p1, int64_t base);
static int64_t fg177cs_gg_function_12(int64_t p0, int64_t base);
static int64_t fg177cs_gg_function_13(int64_t p0, int64_t p1, int64_t base);
static uint64_t fg177cs_gg_function_14(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t fg177cs_gg_function_15(int64_t base);
static uint64_t fg177cs_gg_function_16(uint64_t p0, uint64_t p1, int64_t base);
static void fg177cs_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  fg177h_fail(t2);
  }
  }
  }
fg177cs_gg_done:;
}
static int64_t fg177cs_gg_function_13(int64_t p0, int64_t p1, int64_t base) {
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
  t4 = fg177h_ti(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto fg177cs_gg_done;
  }
fg177cs_gg_done:;
  return t0;
}
static uint64_t fg177cs_gg_function_8(int64_t p0, int64_t base) {
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
  fg177cs_gg_function_3(t3, t4, base + 0);
  t0 = ((uint64_t)(((uint64_t)(v0))));
  goto fg177cs_gg_done;
  }
fg177cs_gg_done:;
  return t0;
}
static int64_t fg177cs_gg_function_11(int64_t p0, int64_t p1, int64_t base) {
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
  t4 = fg177h_ti(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto fg177cs_gg_done;
  }
fg177cs_gg_done:;
  return t0;
}
static uint64_t fg177cs_gg_function_16(uint64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  int64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint8_t t13;
  uint64_t t14;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(4))));
  fg177cs_gg_function_3(t3, t4, base + 0);
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(v1));
  t7 = ((uint64_t)(((uint64_t)fg177cs_gg_udiv(t5, t6))));
  t9 = ((uint64_t)(v0));
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(((uint64_t)fg177cs_gg_umod(t9, t10))));
  t12 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) != (t12));
  if (t13) {
  {
  t8 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t8 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t14 = ((uint64_t)(t8));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t7) + (uint64_t)(t14)))));
  goto fg177cs_gg_done;
  }
fg177cs_gg_done:;
  return t0;
}
static int64_t fg177cs_gg_function_12(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(5))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg177h_ti(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto fg177cs_gg_done;
  }
fg177cs_gg_done:;
  return t0;
}
static uint64_t fg177cs_gg_function_15(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto fg177cs_gg_done;
  }
fg177cs_gg_done:;
  return t0;
}
static uint64_t fg177cs_gg_function_14(uint64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  uint8_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint8_t t10;
  int64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((uint64_t)(v1));
  t6 = fg177cs_gg_function_15(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)fg177cs_gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg177cs_gg_function_3(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto fg177cs_gg_done;
  }
fg177cs_gg_done:;
  return t0;
}
static int64_t fg177cs_gg_function_10(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint8_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint8_t t43;
  uint64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  uint8_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
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
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint8_t t81;
  uint8_t t82;
  uint64_t t83;
  uint64_t t84;
  uint8_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  uint64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  uint64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  uint64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint64_t t106;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  uint64_t v5;
  int64_t v6;
  int64_t v7;
  uint64_t v8;
  uint64_t v9;
  uint64_t v10;
  uint64_t v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = fg177h_ti(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg177h_ti(t6, t7, t8);
  t10 = ((int64_t)(t9));
  t11 = ((t5) != (t10));
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg177cs_gg_done;
  }
  }
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t12) < (t13)))) goto label2;
  {
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(v3));
  t16 = fg177cs_gg_function_11(t14, t15, base + 0);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(v3));
  t20 = fg177cs_gg_function_11(t18, t19, base + 0);
  t21 = ((int64_t)(t20));
  t22 = ((t17) != (t21));
  if (t22) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg177cs_gg_done;
  }
  }
  t23 = v3;
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t23) + (uint64_t)(t24)));
  }
  goto label1;
label2:;
  t25 = ((int64_t)(v1));
  t26 = fg177cs_gg_function_12(t25, base + 0);
  v4 = fg177cs_gg_function_8(t26, base + 0);
  v5 = ((uint64_t)(v4));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t27 = ((int64_t)(v6));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t27) < (t28)))) goto label5;
  {
  t29 = ((int64_t)(v1));
  t30 = ((int64_t)(v6));
  t31 = fg177cs_gg_function_13(t29, t30, base + 0);
  t32 = fg177cs_gg_function_8(t31, base + 0);
  t33 = ((uint64_t)(t32));
  t34 = ((uint64_t)(v5));
  t35 = ((t33) != (t34));
  t36 = t35;
  if (!t36) {
  t37 = ((int64_t)(v2));
  t38 = ((int64_t)(v6));
  t39 = fg177cs_gg_function_13(t37, t38, base + 0);
  t40 = fg177cs_gg_function_8(t39, base + 0);
  t41 = ((uint64_t)(t40));
  t42 = ((uint64_t)(v5));
  t36 = ((t41) != (t42));
  }
  t43 = t36;
  if (t43) {
  {
  goto label5;
  }
  }
  t44 = ((uint64_t)(v5));
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(v6));
  t47 = fg177cs_gg_function_11(t45, t46, base + 0);
  t48 = fg177cs_gg_function_8(t47, base + 0);
  v5 = fg177cs_gg_function_14(t44, t48, base + 0);
  t49 = v6;
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t49) + (uint64_t)(t50)));
  }
  goto label4;
label5:;
  t51 = ((int64_t)(v6));
  t52 = ((int64_t)(((int64_t)UINT64_C(0))));
  t53 = ((t51) == (t52));
  t54 = t53;
  if (!t54) {
  t55 = ((int64_t)(v6));
  t56 = ((int64_t)(((int64_t)UINT64_C(4))));
  t54 = ((t55) == (t56));
  }
  t57 = t54;
  if (t57) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg177cs_gg_done;
  }
  }
  t58 = ((int64_t)(v6));
  t59 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t59)))));
label7:;
  t60 = ((int64_t)(v7));
  t61 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t60) < (t61)))) goto label8;
  {
  t62 = ((int64_t)(v1));
  t63 = ((int64_t)(v7));
  t64 = fg177cs_gg_function_11(t62, t63, base + 0);
  t65 = ((int64_t)(t64));
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  t67 = ((t65) != (t66));
  if (t67) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg177cs_gg_done;
  }
  }
  t68 = v7;
  t69 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)((uint64_t)(t68) + (uint64_t)(t69)));
  }
  goto label7;
label8:;
  v8 = ((uint64_t)(v5));
  t70 = ((int64_t)(v1));
  t71 = ((int64_t)(v6));
  t72 = fg177cs_gg_function_11(t70, t71, base + 0);
  v9 = fg177cs_gg_function_8(t72, base + 0);
  t73 = ((int64_t)(v1));
  t74 = ((int64_t)(v6));
  t75 = fg177cs_gg_function_13(t73, t74, base + 0);
  v10 = fg177cs_gg_function_8(t75, base + 0);
  t76 = ((int64_t)(v2));
  t77 = ((int64_t)(v6));
  t78 = fg177cs_gg_function_13(t76, t77, base + 0);
  v11 = fg177cs_gg_function_8(t78, base + 0);
  t79 = ((uint64_t)(v10));
  t80 = ((uint64_t)(v8));
  t81 = ((t79) >= (t80));
  t82 = t81;
  if (t82) {
  t83 = ((uint64_t)(v11));
  t84 = ((uint64_t)(v8));
  t82 = ((t83) >= (t84));
  }
  t85 = t82;
  t86 = (!(t85));
  if (t86) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg177cs_gg_done;
  }
  }
  t87 = ((int64_t)(v0));
  t88 = ((int64_t)(((int64_t)UINT64_C(5))));
  t89 = ((int64_t)(((int64_t)UINT64_C(0))));
  t90 = ((uint64_t)(v8));
  fg177h_put(t87, t88, t89, t90);
  t91 = ((int64_t)(v0));
  t92 = ((int64_t)(((int64_t)UINT64_C(5))));
  t93 = ((int64_t)(((int64_t)UINT64_C(1))));
  t94 = ((uint64_t)(v9));
  fg177h_put(t91, t92, t93, t94);
  t95 = ((int64_t)(v0));
  t96 = ((int64_t)(((int64_t)UINT64_C(5))));
  t97 = ((int64_t)(((int64_t)UINT64_C(2))));
  t98 = ((uint64_t)(v10));
  fg177h_put(t95, t96, t97, t98);
  t99 = ((int64_t)(v0));
  t100 = ((int64_t)(((int64_t)UINT64_C(5))));
  t101 = ((int64_t)(((int64_t)UINT64_C(3))));
  t102 = ((uint64_t)(v11));
  fg177h_put(t99, t100, t101, t102);
  t103 = ((int64_t)(v0));
  t104 = ((int64_t)(((int64_t)UINT64_C(5))));
  t105 = ((int64_t)(((int64_t)UINT64_C(4))));
  t106 = ((uint64_t)(((uint64_t)(v6))));
  fg177h_put(t103, t104, t105, t106);
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto fg177cs_gg_done;
  }
fg177cs_gg_done:;
  return t0;
}
static uint8_t fg177cs_gg_function_1(int64_t p0, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
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
  uint8_t t38;
  uint8_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  uint64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  uint64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  uint8_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint8_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint8_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  uint8_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  uint8_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  uint8_t t115;
  uint8_t t116;
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
  uint8_t t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  uint64_t t144;
  uint64_t t145;
  uint64_t t146;
  uint64_t t147;
  uint8_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  uint64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  uint64_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  uint64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  uint64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  uint64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  uint64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  uint64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  uint64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  uint64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  uint64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  uint64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  uint64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  uint64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  uint64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  uint64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  uint64_t t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  uint64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  uint64_t t264;
  uint8_t t265;
  uint8_t t266;
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
  uint8_t t277;
  uint8_t t278;
  uint8_t t279;
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
  uint8_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  uint64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  uint8_t t299;
  int64_t t300;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  uint64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  uint8_t v12;
  uint8_t v13;
  uint64_t v14;
  uint64_t v15;
  uint64_t v16;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg177h_arg(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  t6 = fg177h_arg(t4, t5);
  v2 = ((int64_t)(t6));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) != (t8));
  t10 = t9;
  if (t10) {
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = ((t11) != (t12));
  }
  t13 = t10;
  t14 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg177cs_gg_function_3(t13, t14, base + 0);
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(7))));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = fg177h_ti(t15, t16, t17);
  v3 = ((int64_t)(t18));
  t19 = ((int64_t)(v1));
  t20 = ((int64_t)(((int64_t)UINT64_C(7))));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = fg177h_ti(t19, t20, t21);
  v4 = ((int64_t)(t22));
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(v4));
  t25 = ((t23) != (t24));
  if (t25) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg177cs_gg_done;
  }
  }
  t26 = ((int64_t)(v0));
  t27 = fg177h_plan(t26);
  v5 = ((int64_t)(t27));
  t28 = ((int64_t)(v2));
  t29 = ((int64_t)(((int64_t)UINT64_C(2))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = fg177h_ti(t28, t29, t30);
  t32 = ((int64_t)(t31));
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg177h_ti(t33, t34, t35);
  t37 = ((int64_t)(t36));
  t38 = ((t32) == (t37));
  t39 = t38;
  if (t39) {
  t40 = ((int64_t)(v2));
  t41 = ((int64_t)(((int64_t)UINT64_C(8))));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = fg177h_ti(t40, t41, t42);
  t44 = ((int64_t)(t43));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  t39 = ((t44) == (t45));
  }
  t46 = t39;
  t47 = t46;
  if (t47) {
  t48 = ((int64_t)(v1));
  t49 = ((int64_t)(((int64_t)UINT64_C(8))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg177h_ti(t48, t49, t50);
  t52 = ((int64_t)(t51));
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  t47 = ((t52) == (t53));
  }
  t54 = t47;
  if (t54) {
  {
  t55 = ((int64_t)(v5));
  t56 = ((int64_t)(((int64_t)UINT64_C(0))));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg177h_put(t55, t56, t57, t58);
  t59 = ((int64_t)(v0));
  t60 = ((int64_t)(((int64_t)UINT64_C(9))));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = fg177h_ti(t59, t60, t61);
  t63 = ((int64_t)(t62));
  v6 = fg177cs_gg_function_8(t63, base + 0);
  t64 = ((int64_t)(v5));
  t65 = ((int64_t)(((int64_t)UINT64_C(0))));
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  t67 = ((uint64_t)(v6));
  fg177h_put(t64, t65, t66, t67);
  t68 = ((int64_t)(v0));
  t69 = fg177h_launch(t68);
  t70 = ((int64_t)(t69));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((t70) == (t71));
  t73 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg177cs_gg_function_3(t72, t73, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto fg177cs_gg_done;
  }
  }
  t74 = ((int64_t)(v5));
  t75 = ((int64_t)(v2));
  t76 = ((int64_t)(v1));
  t77 = fg177cs_gg_function_10(t74, t75, t76, base + 0);
  t78 = ((int64_t)(t77));
  t79 = ((int64_t)(((int64_t)UINT64_C(1))));
  t80 = ((t78) == (t79));
  if (t80) {
  {
  t81 = ((int64_t)(v5));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = ((int64_t)(((int64_t)UINT64_C(0))));
  t84 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg177h_put(t81, t82, t83, t84);
  t85 = ((int64_t)(v0));
  t86 = fg177h_launch(t85);
  t87 = ((int64_t)(t86));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((t87) == (t88));
  t90 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg177cs_gg_function_3(t89, t90, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto fg177cs_gg_done;
  }
  }
  t91 = ((int64_t)(v2));
  v7 = fg177cs_gg_function_12(t91, base + 0);
  t92 = ((int64_t)(v1));
  v8 = fg177cs_gg_function_12(t92, base + 0);
  t93 = ((int64_t)(v7));
  t94 = ((int64_t)(((int64_t)UINT64_C(4))));
  t95 = ((t93) != (t94));
  t96 = t95;
  if (!t96) {
  t97 = ((int64_t)(v8));
  t98 = ((int64_t)(((int64_t)UINT64_C(4))));
  t96 = ((t97) != (t98));
  }
  t99 = t96;
  if (t99) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg177cs_gg_done;
  }
  }
  t100 = ((int64_t)(v2));
  t101 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = fg177cs_gg_function_13(t100, t101, base + 0);
  t102 = ((int64_t)(v2));
  t103 = ((int64_t)(((int64_t)UINT64_C(2))));
  v10 = fg177cs_gg_function_13(t102, t103, base + 0);
  t104 = ((int64_t)(v2));
  t105 = ((int64_t)(((int64_t)UINT64_C(3))));
  v11 = fg177cs_gg_function_13(t104, t105, base + 0);
  t106 = ((int64_t)(v9));
  t107 = ((int64_t)(v7));
  t108 = ((t106) == (t107));
  t109 = t108;
  if (t109) {
  t110 = ((int64_t)(v2));
  t111 = ((int64_t)(((int64_t)UINT64_C(3))));
  t112 = fg177cs_gg_function_11(t110, t111, base + 0);
  t113 = ((int64_t)(t112));
  t114 = ((int64_t)(((int64_t)UINT64_C(1))));
  t109 = ((t113) == (t114));
  }
  t115 = t109;
  t116 = t115;
  if (t116) {
  t117 = ((int64_t)(v10));
  t118 = ((int64_t)(v2));
  t119 = ((int64_t)(((int64_t)UINT64_C(0))));
  t120 = fg177cs_gg_function_11(t118, t119, base + 0);
  t121 = ((int64_t)(t120));
  t122 = ((int64_t)(v2));
  t123 = ((int64_t)(((int64_t)UINT64_C(1))));
  t124 = fg177cs_gg_function_11(t122, t123, base + 0);
  t125 = ((int64_t)(t124));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t121) * (uint64_t)(t125)))));
  t127 = ((int64_t)(v7));
  t128 = ((int64_t)(((int64_t)((uint64_t)(t126) * (uint64_t)(t127)))));
  t116 = ((t117) == (t128));
  }
  v12 = t116;
  t129 = ((int64_t)(v2));
  t130 = ((int64_t)(((int64_t)UINT64_C(8))));
  t131 = ((int64_t)(((int64_t)UINT64_C(0))));
  t132 = fg177h_ti(t129, t130, t131);
  t133 = ((int64_t)(t132));
  t134 = ((int64_t)(((int64_t)UINT64_C(1))));
  t135 = ((t133) == (t134));
  t136 = t135;
  if (t136) {
  t137 = ((int64_t)(v1));
  t138 = ((int64_t)(((int64_t)UINT64_C(8))));
  t139 = ((int64_t)(((int64_t)UINT64_C(0))));
  t140 = fg177h_ti(t137, t138, t139);
  t141 = ((int64_t)(t140));
  t142 = ((int64_t)(((int64_t)UINT64_C(1))));
  t136 = ((t141) == (t142));
  }
  v13 = t136;
  v14 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(64))))));
  t143 = ((int64_t)(v3));
  t144 = fg177cs_gg_function_8(t143, base + 0);
  t145 = ((uint64_t)(v14));
  v15 = fg177cs_gg_function_16(t144, t145, base + 0);
  t146 = ((uint64_t)(v15));
  t147 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2147483647))))));
  t148 = ((t146) <= (t147));
  t149 = ((int64_t)(((int64_t)UINT64_C(22))));
  fg177cs_gg_function_3(t148, t149, base + 0);
  t150 = ((int64_t)(v5));
  t151 = ((int64_t)(((int64_t)UINT64_C(0))));
  t152 = ((int64_t)(((int64_t)UINT64_C(0))));
  t153 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  fg177h_put(t150, t151, t152, t153);
  t154 = ((int64_t)(v5));
  t155 = ((int64_t)(((int64_t)UINT64_C(0))));
  t156 = ((int64_t)(((int64_t)UINT64_C(1))));
  t157 = ((uint64_t)(v15));
  fg177h_put(t154, t155, t156, t157);
  t158 = ((int64_t)(v5));
  t159 = ((int64_t)(((int64_t)UINT64_C(0))));
  t160 = ((int64_t)(((int64_t)UINT64_C(2))));
  t161 = ((uint64_t)(v14));
  fg177h_put(t158, t159, t160, t161);
  t162 = ((int64_t)(v5));
  t163 = ((int64_t)(((int64_t)UINT64_C(1))));
  t164 = ((int64_t)(((int64_t)UINT64_C(0))));
  t165 = ((int64_t)(v3));
  t166 = fg177cs_gg_function_8(t165, base + 0);
  fg177h_put(t162, t163, t164, t166);
  t167 = ((int64_t)(v5));
  t168 = ((int64_t)(((int64_t)UINT64_C(1))));
  t169 = ((int64_t)(((int64_t)UINT64_C(1))));
  t170 = ((int64_t)(v2));
  t171 = ((int64_t)(((int64_t)UINT64_C(0))));
  t172 = fg177cs_gg_function_11(t170, t171, base + 0);
  t173 = fg177cs_gg_function_8(t172, base + 0);
  fg177h_put(t167, t168, t169, t173);
  t174 = ((int64_t)(v5));
  t175 = ((int64_t)(((int64_t)UINT64_C(1))));
  t176 = ((int64_t)(((int64_t)UINT64_C(2))));
  t177 = ((int64_t)(v2));
  t178 = ((int64_t)(((int64_t)UINT64_C(1))));
  t179 = fg177cs_gg_function_11(t177, t178, base + 0);
  t180 = fg177cs_gg_function_8(t179, base + 0);
  fg177h_put(t174, t175, t176, t180);
  t181 = ((int64_t)(v5));
  t182 = ((int64_t)(((int64_t)UINT64_C(1))));
  t183 = ((int64_t)(((int64_t)UINT64_C(3))));
  t184 = ((int64_t)(v2));
  t185 = ((int64_t)(((int64_t)UINT64_C(2))));
  t186 = fg177cs_gg_function_11(t184, t185, base + 0);
  t187 = fg177cs_gg_function_8(t186, base + 0);
  fg177h_put(t181, t182, t183, t187);
  t188 = ((int64_t)(v5));
  t189 = ((int64_t)(((int64_t)UINT64_C(1))));
  t190 = ((int64_t)(((int64_t)UINT64_C(4))));
  t191 = ((int64_t)(v2));
  t192 = ((int64_t)(((int64_t)UINT64_C(0))));
  t193 = fg177cs_gg_function_13(t191, t192, base + 0);
  t194 = ((uint64_t)(((uint64_t)(t193))));
  fg177h_put(t188, t189, t190, t194);
  t195 = ((int64_t)(v5));
  t196 = ((int64_t)(((int64_t)UINT64_C(1))));
  t197 = ((int64_t)(((int64_t)UINT64_C(5))));
  t198 = ((int64_t)(v2));
  t199 = ((int64_t)(((int64_t)UINT64_C(1))));
  t200 = fg177cs_gg_function_13(t198, t199, base + 0);
  t201 = ((uint64_t)(((uint64_t)(t200))));
  fg177h_put(t195, t196, t197, t201);
  t202 = ((int64_t)(v5));
  t203 = ((int64_t)(((int64_t)UINT64_C(1))));
  t204 = ((int64_t)(((int64_t)UINT64_C(6))));
  t205 = ((int64_t)(v2));
  t206 = ((int64_t)(((int64_t)UINT64_C(2))));
  t207 = fg177cs_gg_function_13(t205, t206, base + 0);
  t208 = ((uint64_t)(((uint64_t)(t207))));
  fg177h_put(t202, t203, t204, t208);
  t209 = ((int64_t)(v5));
  t210 = ((int64_t)(((int64_t)UINT64_C(1))));
  t211 = ((int64_t)(((int64_t)UINT64_C(7))));
  t212 = ((int64_t)(v2));
  t213 = ((int64_t)(((int64_t)UINT64_C(3))));
  t214 = fg177cs_gg_function_13(t212, t213, base + 0);
  t215 = ((uint64_t)(((uint64_t)(t214))));
  fg177h_put(t209, t210, t211, t215);
  t216 = ((int64_t)(v5));
  t217 = ((int64_t)(((int64_t)UINT64_C(2))));
  t218 = ((int64_t)(((int64_t)UINT64_C(0))));
  t219 = ((int64_t)(v1));
  t220 = ((int64_t)(((int64_t)UINT64_C(0))));
  t221 = fg177cs_gg_function_11(t219, t220, base + 0);
  t222 = fg177cs_gg_function_8(t221, base + 0);
  fg177h_put(t216, t217, t218, t222);
  t223 = ((int64_t)(v5));
  t224 = ((int64_t)(((int64_t)UINT64_C(2))));
  t225 = ((int64_t)(((int64_t)UINT64_C(1))));
  t226 = ((int64_t)(v1));
  t227 = ((int64_t)(((int64_t)UINT64_C(1))));
  t228 = fg177cs_gg_function_11(t226, t227, base + 0);
  t229 = fg177cs_gg_function_8(t228, base + 0);
  fg177h_put(t223, t224, t225, t229);
  t230 = ((int64_t)(v5));
  t231 = ((int64_t)(((int64_t)UINT64_C(2))));
  t232 = ((int64_t)(((int64_t)UINT64_C(2))));
  t233 = ((int64_t)(v1));
  t234 = ((int64_t)(((int64_t)UINT64_C(2))));
  t235 = fg177cs_gg_function_11(t233, t234, base + 0);
  t236 = fg177cs_gg_function_8(t235, base + 0);
  fg177h_put(t230, t231, t232, t236);
  t237 = ((int64_t)(v5));
  t238 = ((int64_t)(((int64_t)UINT64_C(2))));
  t239 = ((int64_t)(((int64_t)UINT64_C(3))));
  t240 = ((int64_t)(v1));
  t241 = ((int64_t)(((int64_t)UINT64_C(0))));
  t242 = fg177cs_gg_function_13(t240, t241, base + 0);
  t243 = ((uint64_t)(((uint64_t)(t242))));
  fg177h_put(t237, t238, t239, t243);
  t244 = ((int64_t)(v5));
  t245 = ((int64_t)(((int64_t)UINT64_C(2))));
  t246 = ((int64_t)(((int64_t)UINT64_C(4))));
  t247 = ((int64_t)(v1));
  t248 = ((int64_t)(((int64_t)UINT64_C(1))));
  t249 = fg177cs_gg_function_13(t247, t248, base + 0);
  t250 = ((uint64_t)(((uint64_t)(t249))));
  fg177h_put(t244, t245, t246, t250);
  t251 = ((int64_t)(v5));
  t252 = ((int64_t)(((int64_t)UINT64_C(2))));
  t253 = ((int64_t)(((int64_t)UINT64_C(5))));
  t254 = ((int64_t)(v1));
  t255 = ((int64_t)(((int64_t)UINT64_C(2))));
  t256 = fg177cs_gg_function_13(t254, t255, base + 0);
  t257 = ((uint64_t)(((uint64_t)(t256))));
  fg177h_put(t251, t252, t253, t257);
  t258 = ((int64_t)(v5));
  t259 = ((int64_t)(((int64_t)UINT64_C(2))));
  t260 = ((int64_t)(((int64_t)UINT64_C(6))));
  t261 = ((int64_t)(v1));
  t262 = ((int64_t)(((int64_t)UINT64_C(3))));
  t263 = fg177cs_gg_function_13(t261, t262, base + 0);
  t264 = ((uint64_t)(((uint64_t)(t263))));
  fg177h_put(t258, t259, t260, t264);
  t265 = v12;
  t266 = t265;
  if (t266) {
  t267 = ((int64_t)(v2));
  t268 = ((int64_t)(((int64_t)UINT64_C(2))));
  t269 = ((int64_t)(((int64_t)UINT64_C(0))));
  t270 = fg177h_ti(t267, t268, t269);
  t271 = ((int64_t)(t270));
  t272 = ((int64_t)(v1));
  t273 = ((int64_t)(((int64_t)UINT64_C(2))));
  t274 = ((int64_t)(((int64_t)UINT64_C(0))));
  t275 = fg177h_ti(t272, t273, t274);
  t276 = ((int64_t)(t275));
  t266 = ((t271) == (t276));
  }
  t277 = t266;
  if (t277) {
  {
  v16 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  }
  } else {
  {
  t278 = v13;
  t279 = t278;
  if (t279) {
  t280 = ((int64_t)(v2));
  t281 = ((int64_t)(((int64_t)UINT64_C(2))));
  t282 = ((int64_t)(((int64_t)UINT64_C(0))));
  t283 = fg177h_ti(t280, t281, t282);
  t284 = ((int64_t)(t283));
  t285 = ((int64_t)(v1));
  t286 = ((int64_t)(((int64_t)UINT64_C(2))));
  t287 = ((int64_t)(((int64_t)UINT64_C(0))));
  t288 = fg177h_ti(t285, t286, t287);
  t289 = ((int64_t)(t288));
  t279 = ((t284) == (t289));
  }
  t290 = t279;
  if (t290) {
  {
  v16 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(3))))));
  }
  } else {
  {
  v16 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  }
  }
  }
  }
  t291 = ((int64_t)(v5));
  t292 = ((int64_t)(((int64_t)UINT64_C(3))));
  t293 = ((int64_t)(((int64_t)UINT64_C(0))));
  t294 = ((uint64_t)(v16));
  fg177h_put(t291, t292, t293, t294);
  t295 = ((int64_t)(v0));
  t296 = fg177h_launch(t295);
  t297 = ((int64_t)(t296));
  t298 = ((int64_t)(((int64_t)UINT64_C(0))));
  t299 = ((t297) == (t298));
  t300 = ((int64_t)(((int64_t)UINT64_C(23))));
  fg177cs_gg_function_3(t299, t300, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto fg177cs_gg_done;
  }
fg177cs_gg_done:;
  return t0;
}
static void fg177cs_gg_function_0(int64_t base) {
  int64_t t0;
  uint8_t t1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = fg177cs_gg_function_1(t0, base + 0);
  (void)(t1);
  }
fg177cs_gg_done:;
}
static int fg177cs_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg177cs_gg_argc = argc; fg177cs_gg_argv = argv;
  fg177cs_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg177cs_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg177cs_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg177cs_gg_program(argc, argv);
#endif
}
