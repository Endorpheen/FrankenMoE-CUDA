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
typedef struct { void *data; int64_t len; } fg171rh_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg171rh_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg171rh_gg_stack;
typedef struct { fg171rh_gg_header *header; fg171rh_gg_stack *stack; } fg171rh_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg171rh_gg_stack fg171rh_gg_stacks[GG_STACK_COUNT];
static int fg171rh_gg_argc;
static char **fg171rh_gg_argv;
static void fg171rh_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg171rh_gg_stack *fg171rh_gg_get_stack(int64_t id) {
    fg171rh_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg171rh_gg_fail("data stack limit exceeded");
    s = &fg171rh_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg171rh_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg171rh_gg_allocate(fg171rh_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg171rh_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg171rh_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg171rh_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg171rh_gg_reference fg171rh_gg_ref(fg171rh_gg_header *h, fg171rh_gg_stack *s) { fg171rh_gg_reference r; r.header=h; r.stack=s; return r; }
static fg171rh_gg_slice fg171rh_gg_view(void *p, int64_t n) { fg171rh_gg_slice v; v.data=p; v.len=n; return v; }
static fg171rh_gg_slice fg171rh_gg_byte_view(fg171rh_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg171rh_gg_fail("byte view size overflow");
    return fg171rh_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg171rh_gg_slice fg171rh_gg_sub(fg171rh_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg171rh_gg_fail("slice out of bounds");
    return fg171rh_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg171rh_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg171rh_gg_fail("array index out of bounds"); return i; }
static void fg171rh_gg_append(fg171rh_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg171rh_gg_fail("invalid array size");
    to=fg171rh_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg171rh_gg_resize(fg171rh_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg171rh_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg171rh_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg171rh_gg_fail("invalid array size");
        p=fg171rh_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg171rh_gg_div(int64_t a, int64_t b) { if (!b) fg171rh_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg171rh_gg_fail("division overflow"); return a/b; }
static int64_t fg171rh_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg171rh_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg171rh_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg171rh_gg_fail("division by zero");return a/b; }
static uint64_t fg171rh_gg_umod(uint64_t a,uint64_t b) { if(!b)fg171rh_gg_fail("division by zero");return a%b; }
static int fg171rh_gg_equal(fg171rh_gg_slice a, fg171rh_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg171rh_gg_text(fg171rh_gg_reference out, fg171rh_gg_slice v) { fg171rh_gg_append(out,v.data,v.len,1); }
static void fg171rh_gg_integer(fg171rh_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg171rh_gg_append(out,b,k,1); }
static void fg171rh_gg_unsigned(fg171rh_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg171rh_gg_append(out,b,k,1); }
static double fg171rh_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg171rh_ggf_fmt_f64(uint8_t *out,double n);
static int64_t fg171rh_ggf_fmt_f32(uint8_t *out,float n);
static void fg171rh_gg_float(fg171rh_gg_reference out,double n) { uint8_t b[64];int64_t k=fg171rh_ggf_fmt_f64(b,n);fg171rh_gg_append(out,b,k,1); }
static void fg171rh_gg_float32(fg171rh_gg_reference out,float n) { uint8_t b[64];int64_t k=fg171rh_ggf_fmt_f32(b,n);fg171rh_gg_append(out,b,k,1); }
static void fg171rh_gg_bool(fg171rh_gg_reference out,uint8_t n) { fg171rh_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg171rh_gg_write(FILE *file,fg171rh_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg171rh_gg_fail("write failed"); }
static char *fg171rh_gg_cstring(fg171rh_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg171rh_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t fg171rh_gs_os_read_file(fg171rh_gg_slice path,fg171rh_gg_reference out) {
    char *p=fg171rh_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg171rh_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t fg171rh_gs_os_write_file(fg171rh_gg_slice path,fg171rh_gg_slice data) {
    char *p=fg171rh_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t fg171rh_gs_os_file_exists(fg171rh_gg_slice path) { char*p=fg171rh_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void fg171rh_gs_os_write_stdout(fg171rh_gg_slice s) { fg171rh_gg_write(stdout,s); }
static void fg171rh_gs_os_write_stderr(fg171rh_gg_slice s) { fg171rh_gg_write(stderr,s); }
static void fg171rh_gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t fg171rh_gs_os_arg_count(void) { return fg171rh_gg_argc; }
static void fg171rh_gs_os_arg(int64_t i,fg171rh_gg_reference out) { if(i>=0 && i<fg171rh_gg_argc)fg171rh_gg_append(out,fg171rh_gg_argv[i],(int64_t)strlen(fg171rh_gg_argv[i]),1); }
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
typedef struct { int n; uint32_t d[36]; } fg171rh_ggf_big;

/* a = v << sh, for v > 0. */
static void fg171rh_ggf_big_set(fg171rh_ggf_big *a, uint64_t v, int sh) {
    int w = sh >> 5, b = sh & 31, i;
    uint64_t lo = v << b, hi = b ? v >> (64 - b) : 0;
    for (i = 0; i < w; i++) a->d[i] = 0;
    a->d[w] = (uint32_t)lo;
    a->d[w + 1] = (uint32_t)(lo >> 32);
    a->d[w + 2] = (uint32_t)hi;
    a->n = w + 3;
    while (!a->d[a->n - 1]) a->n--;
}

static void fg171rh_ggf_big_mul(fg171rh_ggf_big *a, uint32_t m) {
    uint64_t c = 0;
    for (int i = 0; i < a->n; i++) {
        c += (uint64_t)a->d[i] * m;
        a->d[i] = (uint32_t)c;
        c >>= 32;
    }
    if (c) a->d[a->n++] = (uint32_t)c;
}

static void fg171rh_ggf_big_mul_pow10(fg171rh_ggf_big *a, int k) {
    for (; k >= 9; k -= 9) fg171rh_ggf_big_mul(a, 1000000000u);
    for (; k > 0; k--) fg171rh_ggf_big_mul(a, 10);
}

static int fg171rh_ggf_big_cmp(const fg171rh_ggf_big *a, const fg171rh_ggf_big *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

/* t = a + b */
static void fg171rh_ggf_big_add(fg171rh_ggf_big *t, const fg171rh_ggf_big *a, const fg171rh_ggf_big *b) {
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
static void fg171rh_ggf_big_sub(fg171rh_ggf_big *a, const fg171rh_ggf_big *b) {
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
static int fg171rh_ggf_float_digits(uint64_t f, int e, int bits, int emin, char *dig, int *k) {
    fg171rh_ggf_big r, s, mp, mlo, t;
    /* Reading rounds a tie to the even significand, so the interval that
       reads back as f * 2^e includes its ends when f is even. */
    int even = !(f & 1);
    /* Below a power of two, the gap to the next float down is half the gap up. */
    int asym = f == (uint64_t)1 << (bits - 1) && e > emin;
    int up = e > 0 ? e : 0, lg = e - 1, n = 0, c;
    /* The value is r / s, and what reads back as it reaches from (r - *mm) / s
       to (r + mp) / s: half the gap to each neighbour. */
    fg171rh_ggf_big *mm = asym ? &mlo : &mp;
    fg171rh_ggf_big_set(&r, f, up + 1 + asym);
    fg171rh_ggf_big_set(&s, 1, up - e + 1 + asym);
    fg171rh_ggf_big_set(&mp, 1, up + asym);
    fg171rh_ggf_big_set(&mlo, 1, up);
    /* Scale to 10^*k, the least power of ten above the interval: with
       lg = floor(log2 value), ceil(lg log10 2) is it or one short, and
       78913 / 2^18 is log10 2 closely enough to compute that for |lg| < 1650. */
    for (uint64_t g = f; g; g >>= 1) lg++;
    *k = lg > 0 ? ((lg * 78913) >> 18) + 1 : -((-lg * 78913) >> 18);
    if (*k >= 0) fg171rh_ggf_big_mul_pow10(&s, *k);
    else {
        fg171rh_ggf_big_mul_pow10(&r, -*k);
        fg171rh_ggf_big_mul_pow10(&mp, -*k);
        if (asym) fg171rh_ggf_big_mul_pow10(&mlo, -*k);
    }
    fg171rh_ggf_big_add(&t, &r, &mp);
    c = fg171rh_ggf_big_cmp(&t, &s);
    if (even ? c >= 0 : c > 0) {
        fg171rh_ggf_big_mul(&s, 10);
        ++*k;
    }
    /* Each digit d leaves the remainder r: stop where the digits so far
       followed by d (r within *mm) or by d + 1 (r + mp past s) read back. */
    for (;;) {
        int d = 0, low, high;
        fg171rh_ggf_big_mul(&r, 10);
        fg171rh_ggf_big_mul(&mp, 10);
        if (asym) fg171rh_ggf_big_mul(&mlo, 10);
        while (fg171rh_ggf_big_cmp(&r, &s) >= 0) {
            fg171rh_ggf_big_sub(&r, &s);
            d++;
        }
        c = fg171rh_ggf_big_cmp(&r, mm);
        low = even ? c <= 0 : c < 0;
        fg171rh_ggf_big_add(&t, &r, &mp);
        c = fg171rh_ggf_big_cmp(&t, &s);
        high = even ? c >= 0 : c > 0;
        if (!low && !high) {
            dig[n++] = (char)('0' + d);
            continue;
        }
        if (low && high) {
            /* Both read back: the nearer, d + 1 when r is past half of s,
               and of two as near, the even one. */
            fg171rh_ggf_big_add(&t, &r, &r);
            c = fg171rh_ggf_big_cmp(&t, &s);
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
static int64_t fg171rh_ggf_fmt_float(uint8_t *dst, int neg, uint64_t frac, int bexp, int bits,
                            int emin) {
    char dig[20], *p = (char *)dst;
    int k, n, x, i;
    if (neg) *p++ = '-';
    if (!frac && !bexp) {
        memcpy(p, "0.0", 3);
        return p + 3 - (char *)dst;
    }
    n = fg171rh_ggf_float_digits(bexp ? frac | (uint64_t)1 << (bits - 1) : frac,
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

static int64_t fg171rh_ggf_fmt_f64(uint8_t *dst, double v) {
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
    return fg171rh_ggf_fmt_float(dst, (int)(b >> 63), b & (((uint64_t)1 << 52) - 1),
                        (int)(b >> 52) & 0x7FF, 53, -1074);
}

/* The digits are the f32's own, laid out as an f64's are, so 0.1 as an f32
   prints as 0.1, not as the digits of the f64 it widens to. */
static int64_t fg171rh_ggf_fmt_f32(uint8_t *dst, float v) {
    double d = v;
    uint32_t b;
    if (d != d || isinf(d)) return fg171rh_ggf_fmt_f64(dst, d);
    memcpy(&b, &v, sizeof b);
    return fg171rh_ggf_fmt_float(dst, (int)(b >> 31), b & 0x7FFFFF, (int)(b >> 23) & 0xFF, 24, -149);
}


static void fg171rh_gg_quote(fg171rh_gg_reference out,fg171rh_gg_slice text) { int64_t i; unsigned char c; fg171rh_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg171rh_gg_append(out,"\\",1,1);fg171rh_gg_append(out,&c,1,1);} else if(c=='\n')fg171rh_gg_append(out,"\\n",2,1);else if(c=='\r')fg171rh_gg_append(out,"\\r",2,1);else if(c=='\t')fg171rh_gg_append(out,"\\t",2,1);else fg171rh_gg_append(out,&c,1,1);}fg171rh_gg_append(out,"\"",1,1);}
static uint8_t fg171rh_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg171rh_gg_equal_elements_1(fg171rh_gg_slice a, fg171rh_gg_slice b);
static void fg171rh_gg_render_1(fg171rh_gg_reference out, int64_t value);
static uint8_t fg171rh_gg_eq_2(uint8_t a, uint8_t b);
static uint8_t fg171rh_gg_equal_elements_2(fg171rh_gg_slice a, fg171rh_gg_slice b);
static void fg171rh_gg_render_2(fg171rh_gg_reference out, uint8_t value);
static uint8_t fg171rh_gg_eq_3(uint64_t a, uint64_t b);
static uint8_t fg171rh_gg_equal_elements_3(fg171rh_gg_slice a, fg171rh_gg_slice b);
static void fg171rh_gg_render_3(fg171rh_gg_reference out, uint64_t value);
static uint8_t fg171rh_gg_eq_5(float a, float b);
static uint8_t fg171rh_gg_equal_elements_5(fg171rh_gg_slice a, fg171rh_gg_slice b);
static void fg171rh_gg_render_5(fg171rh_gg_reference out, float value);
static uint8_t fg171rh_gg_eq_6(double a, double b);
static uint8_t fg171rh_gg_equal_elements_6(fg171rh_gg_slice a, fg171rh_gg_slice b);
static void fg171rh_gg_render_6(fg171rh_gg_reference out, double value);
static uint8_t fg171rh_gg_eq_7(uint8_t a, uint8_t b);
static uint8_t fg171rh_gg_equal_elements_7(fg171rh_gg_slice a, fg171rh_gg_slice b);
static void fg171rh_gg_render_7(fg171rh_gg_reference out, uint8_t value);
static uint8_t fg171rh_gg_equal_elements_1(fg171rh_gg_slice a,fg171rh_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg171rh_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg171rh_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg171rh_gg_render_1(fg171rh_gg_reference out, int64_t value) {
fg171rh_gg_integer(out,value);
}
static uint8_t fg171rh_gg_equal_elements_2(fg171rh_gg_slice a,fg171rh_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg171rh_gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg171rh_gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void fg171rh_gg_render_2(fg171rh_gg_reference out, uint8_t value) {
fg171rh_gg_bool(out,value);
}
static uint8_t fg171rh_gg_equal_elements_3(fg171rh_gg_slice a,fg171rh_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg171rh_gg_eq_3(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg171rh_gg_eq_3(uint64_t a, uint64_t b) {
return a == b;
}
static void fg171rh_gg_render_3(fg171rh_gg_reference out, uint64_t value) {
fg171rh_gg_unsigned(out,value);
}
static uint8_t fg171rh_gg_equal_elements_5(fg171rh_gg_slice a,fg171rh_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg171rh_gg_eq_5(((float*)a.data)[i],((float*)b.data)[i]))return 0;return 1;}
static uint8_t fg171rh_gg_eq_5(float a, float b) {
return a == b;
}
static void fg171rh_gg_render_5(fg171rh_gg_reference out, float value) {
fg171rh_gg_float32(out,value);
}
static uint8_t fg171rh_gg_equal_elements_6(fg171rh_gg_slice a,fg171rh_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg171rh_gg_eq_6(((double*)a.data)[i],((double*)b.data)[i]))return 0;return 1;}
static uint8_t fg171rh_gg_eq_6(double a, double b) {
return a == b;
}
static void fg171rh_gg_render_6(fg171rh_gg_reference out, double value) {
fg171rh_gg_float(out,value);
}
static uint8_t fg171rh_gg_equal_elements_7(fg171rh_gg_slice a,fg171rh_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg171rh_gg_eq_7(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg171rh_gg_eq_7(uint8_t a, uint8_t b) {
return a == b;
}
static void fg171rh_gg_render_7(fg171rh_gg_reference out, uint8_t value) {
fg171rh_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg171rh_gg_globals;
static void fg171rh_gg_function_0(int64_t base);
static void fg171rh_gg_function_1(int64_t p0, int64_t base);
static void fg171rh_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static uint64_t fg171rh_gg_function_9(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static uint64_t fg171rh_gg_function_11(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t fg171rh_gg_function_12(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t fg171rh_gg_function_13(int64_t base);
static uint8_t fg171rh_gg_function_16(float p0, int64_t base);
static uint8_t fg171rh_gg_function_19(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base);
static void fg171rh_gg_function_20(int64_t p0, int64_t base);
static uint64_t fg171rh_gg_function_21(uint64_t p0, uint64_t p1, int64_t base);
static float fg171rh_gg_function_24(float p0, float p1, float p2, float p3, int64_t base);
static float fg171rh_gg_function_26(float p0, int64_t base);
static float fg171rh_gg_function_28(float p0, float p1, int64_t base);
static float fg171rh_gg_function_29(float p0, int64_t base);
static float fg171rh_gg_function_31(float p0, float p1, int64_t base);
static float fg171rh_gg_function_31(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((float)(v0));
  }
  } else {
  {
  t0 = ((float)(v1));
  }
  }
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static void fg171rh_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  fg171r_fail(t2);
  }
  }
  }
fg171rh_gg_done:;
}
static uint8_t fg171rh_gg_function_16(float p0, int64_t base) {
  uint8_t t0;
  float t1;
  float t2;
  uint8_t t3;
  uint8_t t4;
  float t5;
  float t6;
  float t7;
  float t8;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v0));
  t3 = ((t1) == (t2));
  t4 = t3;
  if (t4) {
  t5 = ((float)(v0));
  t6 = ((float)(v0));
  t7 = ((float)(((t5) - (t6))));
  t8 = ((float)(fg171rh_gg_double_bits(UINT64_C(0))));
  t4 = ((t7) == (t8));
  }
  t0 = t4;
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static float fg171rh_gg_function_29(float p0, int64_t base) {
  float t0;
  float t1;
  uint8_t t2;
  int64_t t3;
  float t4;
  float t5;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = fg171rh_gg_function_16(t1, base + 0);
  t3 = ((int64_t)(((int64_t)UINT64_C(49))));
  fg171rh_gg_function_3(t2, t3, base + 0);
  t4 = ((float)(v0));
  t5 = fg171r_hceil(t4);
  t0 = ((float)(t5));
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static float fg171rh_gg_function_24(float p0, float p1, float p2, float p3, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float t8;
  float t9;
  float t10;
  float t11;
  float t12;
  float t13;
  float t14;
  float t15;
  float t16;
  float v0;
  float v1;
  float v2;
  float v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((float)(v2));
  t4 = ((float)(fg171rh_gg_double_bits(UINT64_C(4611686018427387904))));
  t5 = ((float)(((t3) * (t4))));
  t6 = ((float)(fg171rh_gg_double_bits(UINT64_C(4614256656552045841))));
  t7 = ((float)(((t5) * (t6))));
  t8 = ((float)(((t2) / (t7))));
  t9 = fg171r_hlog(t8);
  t10 = ((float)(t9));
  t11 = ((float)(((t1) * (t10))));
  t12 = ((float)(fg171rh_gg_double_bits(UINT64_C(4611686018427387904))));
  t13 = ((float)(v3));
  t14 = fg171r_hlog(t13);
  t15 = ((float)(t14));
  t16 = ((float)(((t12) * (t15))));
  t0 = ((float)(((t11) / (t16))));
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static float fg171rh_gg_function_28(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((float)(v0));
  }
  } else {
  {
  t0 = ((float)(v1));
  }
  }
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static float fg171rh_gg_function_26(float p0, int64_t base) {
  float t0;
  float t1;
  uint8_t t2;
  int64_t t3;
  float t4;
  float t5;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = fg171rh_gg_function_16(t1, base + 0);
  t3 = ((int64_t)(((int64_t)UINT64_C(49))));
  fg171rh_gg_function_3(t2, t3, base + 0);
  t4 = ((float)(v0));
  t5 = fg171r_hfloor(t4);
  t0 = ((float)(t5));
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static uint64_t fg171rh_gg_function_21(uint64_t p0, uint64_t p1, int64_t base) {
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
  t4 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg171rh_gg_function_3(t3, t4, base + 0);
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(v1));
  t7 = ((uint64_t)(((uint64_t)fg171rh_gg_udiv(t5, t6))));
  t9 = ((uint64_t)(v0));
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(((uint64_t)fg171rh_gg_umod(t9, t10))));
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
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static uint64_t fg171rh_gg_function_11(uint64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  uint8_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint8_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
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
  t6 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  t7 = ((uint64_t)(v0));
  t8 = ((uint64_t)(((uint64_t)fg171rh_gg_udiv(t6, t7))));
  t4 = ((t5) <= (t8));
  }
  t9 = t4;
  t10 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg171rh_gg_function_3(t9, t10, base + 0);
  t11 = ((uint64_t)(v0));
  t12 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static uint64_t fg171rh_gg_function_13(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static uint64_t fg171rh_gg_function_12(uint64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint8_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = fg171rh_gg_function_13(base + 0);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(v1));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((t1) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(50))));
  fg171rh_gg_function_3(t6, t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static void fg171rh_gg_function_20(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
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
  uint8_t t47;
  uint8_t t48;
  uint64_t t49;
  uint64_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  uint64_t t57;
  uint64_t t58;
  uint8_t t59;
  int64_t t60;
  uint64_t t61;
  uint64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint64_t t68;
  uint64_t t69;
  uint64_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint64_t t74;
  int64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  uint64_t v5;
  int64_t v6;
  uint64_t v7;
  uint64_t v8;
  v0 = p0;
  {
  v1 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(3))));
  t2 = ((int64_t)(((int64_t)UINT64_C(12))));
  t3 = fg171r_hget(t0, t1, t2);
  t4 = ((uint64_t)(t3));
  t5 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  if (t6) {
  {
  v2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  }
  } else {
  {
  v2 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  }
  }
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t7 = ((int64_t)(v3));
  t8 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t7) < (t8)))) goto label2;
  {
  t9 = ((int64_t)UINT64_C(1));
  v4 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t9)))));
  v5 = fg171rh_gg_function_13(base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t10 = ((int64_t)(v6));
  t11 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t10) < (t11)))) goto label5;
  {
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t13 = ((int64_t)(v6));
  v7 = ((uint64_t)(((uint64_t)((uint64_t)(t12) << ((uint64_t)(t13) & 63)))));
  t14 = ((uint64_t)(v1));
  t15 = ((uint64_t)(v7));
  t16 = ((uint64_t)(((t14) & (t15))));
  t17 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t16) == (t17));
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v0));
  t21 = ((int64_t)(((int64_t)UINT64_C(1))));
  t22 = ((int64_t)(((int64_t)UINT64_C(4))));
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))));
  t25 = fg171r_hget(t20, t21, t24);
  t26 = ((uint64_t)(t25));
  t27 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t19 = ((t26) > (t27));
  }
  t28 = t19;
  if (t28) {
  {
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(2))));
  t31 = ((int64_t)(((int64_t)UINT64_C(4))));
  t32 = ((int64_t)(v6));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = fg171r_hget(t29, t30, t33);
  t35 = ((uint64_t)(t34));
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(3))));
  t39 = ((int64_t)(((int64_t)UINT64_C(12))));
  t40 = fg171r_hget(t37, t38, t39);
  t41 = ((uint64_t)(t40));
  t42 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t43 = ((t41) != (t42));
  if (t43) {
  {
  t36 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  }
  } else {
  {
  t36 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  }
  }
  t44 = ((uint64_t)(t36));
  v8 = ((uint64_t)(((uint64_t)((uint64_t)(t35) * (uint64_t)(t44)))));
  t45 = ((int64_t)(v4));
  t46 = ((int64_t)(((int64_t)UINT64_C(0))));
  t47 = ((t45) < (t46));
  t48 = t47;
  if (!t48) {
  t49 = ((uint64_t)(v8));
  t50 = ((uint64_t)(v5));
  t48 = ((t49) < (t50));
  }
  t51 = t48;
  if (t51) {
  {
  v4 = ((int64_t)(v6));
  v5 = ((uint64_t)(v8));
  }
  }
  }
  }
  t52 = v6;
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t52) + (uint64_t)(t53)));
  }
  goto label4;
label5:;
  t54 = ((int64_t)(v4));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = ((t54) < (t55));
  if (t56) {
  {
  goto fg171rh_gg_done;
  }
  }
  t57 = ((uint64_t)(v5));
  t58 = ((uint64_t)(v2));
  t59 = ((t57) >= (t58));
  t60 = ((int64_t)(((int64_t)UINT64_C(51))));
  fg171rh_gg_function_3(t59, t60, base + 0);
  t61 = ((uint64_t)(v2));
  t62 = ((uint64_t)(v5));
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(((int64_t)UINT64_C(1))));
  t65 = ((int64_t)(((int64_t)UINT64_C(4))));
  t66 = ((int64_t)(v4));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = fg171r_hget(t63, t64, t67);
  t69 = ((uint64_t)(t68));
  t70 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t71 = ((uint64_t)(((uint64_t)((uint64_t)(t69) - (uint64_t)(t70)))));
  t72 = fg171rh_gg_function_11(t62, t71, base + 0);
  v2 = fg171rh_gg_function_12(t61, t72, base + 0);
  t73 = v1;
  t74 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  t75 = ((int64_t)(v4));
  t76 = ((uint64_t)(((uint64_t)((uint64_t)(t74) << ((uint64_t)(t75) & 63)))));
  v1 = ((t73) | (t76));
  t77 = v3;
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  v3 = ((int64_t)((uint64_t)(t77) + (uint64_t)(t78)));
  }
  goto label1;
label2:;
  }
fg171rh_gg_done:;
}
static uint8_t fg171rh_gg_function_19(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base) {
  uint8_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint8_t t5;
  uint8_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v2));
  t3 = ((uint64_t)(v3));
  t4 = ((uint64_t)(((uint64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t5 = ((t1) < (t4));
  t6 = t5;
  if (t6) {
  t7 = ((uint64_t)(v2));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t10 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t6 = ((t7) < (t10));
  }
  t0 = t6;
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static uint64_t fg171rh_gg_function_9(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
  uint64_t t0;
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
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint8_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint64_t t50;
  uint64_t t51;
  uint64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  uint64_t t56;
  uint64_t t57;
  uint64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint64_t t65;
  uint64_t t66;
  uint8_t t67;
  uint8_t t68;
  uint64_t t69;
  uint64_t t70;
  uint8_t t71;
  uint8_t t72;
  uint64_t t73;
  uint64_t t74;
  uint64_t t75;
  uint64_t t76;
  uint64_t t77;
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
  uint8_t t88;
  uint8_t t89;
  uint64_t t90;
  uint64_t t91;
  uint8_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  uint64_t v7;
  uint64_t v8;
  int64_t v9;
  int64_t v10;
  uint64_t v11;
  uint64_t v12;
  int64_t v13;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) != (t2));
  t4 = ((int64_t)(v5));
  fg171rh_gg_function_3(t3, t4, base + 0);
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  v7 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v8 = ((uint64_t)(((uint64_t)(v4))));
label1:;
  t5 = ((int64_t)(v6));
  t6 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t5) < (t6)))) goto label2;
  {
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)UINT64_C(3))));
  t9 = ((int64_t)(v6));
  t10 = fg171r_ti(t7, t8, t9);
  v9 = ((int64_t)(t10));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)UINT64_C(4))));
  t13 = ((int64_t)(v6));
  t14 = fg171r_ti(t11, t12, t13);
  v10 = ((int64_t)(t14));
  t15 = ((int64_t)(v9));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) > (t16));
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(v9));
  t20 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t18 = ((t19) <= (t20));
  }
  t21 = t18;
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(v10));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t23) >= (t24));
  }
  t25 = t22;
  t26 = t25;
  if (t26) {
  t27 = ((uint64_t)(((uint64_t)(v10))));
  t28 = ((uint64_t)(((uint64_t)(v4))));
  t29 = ((uint64_t)(((uint64_t)fg171rh_gg_umod(t27, t28))));
  t30 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t29) == (t30));
  }
  t31 = t26;
  t32 = ((int64_t)(v5));
  fg171rh_gg_function_3(t31, t32, base + 0);
  t33 = ((uint64_t)(((uint64_t)(v10))));
  t34 = ((uint64_t)(((uint64_t)(v4))));
  v11 = ((uint64_t)(((uint64_t)fg171rh_gg_udiv(t33, t34))));
  t35 = ((int64_t)(v0));
  t36 = ((int64_t)(((int64_t)UINT64_C(1))));
  t37 = ((int64_t)(v2));
  t38 = ((int64_t)(((int64_t)UINT64_C(4))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(v6));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  t42 = ((uint64_t)(((uint64_t)(v9))));
  fg171r_hput(t35, t36, t41, t42);
  t43 = ((int64_t)(v0));
  t44 = ((int64_t)(((int64_t)UINT64_C(2))));
  t45 = ((int64_t)(v3));
  t46 = ((int64_t)(((int64_t)UINT64_C(4))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = ((int64_t)(v6));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))));
  t50 = ((uint64_t)(v11));
  fg171r_hput(t43, t44, t49, t50);
  t51 = ((uint64_t)(v8));
  t52 = ((uint64_t)(((uint64_t)(v10))));
  t53 = ((int64_t)(v9));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t53) - (uint64_t)(t54)))))));
  t56 = fg171rh_gg_function_11(t52, t55, base + 0);
  v8 = fg171rh_gg_function_12(t51, t56, base + 0);
  t57 = ((uint64_t)(v7));
  t58 = ((uint64_t)(((uint64_t)(v9))));
  v7 = fg171rh_gg_function_11(t57, t58, base + 0);
  t59 = v6;
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)((uint64_t)(t59) + (uint64_t)(t60)));
  }
  goto label1;
label2:;
  t61 = ((int64_t)(v1));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = fg171r_ti(t61, t62, t63);
  v12 = ((uint64_t)(((uint64_t)(t64))));
  t65 = ((uint64_t)(v12));
  t66 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t67 = ((t65) != (t66));
  t68 = t67;
  if (t68) {
  t69 = ((uint64_t)(v8));
  t70 = ((uint64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t68 = ((t69) <= (t70));
  }
  t71 = t68;
  t72 = t71;
  if (t72) {
  t73 = ((uint64_t)(v12));
  t74 = fg171rh_gg_function_13(base + 0);
  t75 = ((uint64_t)(t74));
  t76 = ((uint64_t)(v8));
  t77 = ((uint64_t)(((uint64_t)((uint64_t)(t75) - (uint64_t)(t76)))));
  t72 = ((t73) <= (t77));
  }
  t78 = t72;
  t79 = ((int64_t)(v5));
  t80 = ((int64_t)(((int64_t)UINT64_C(1))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  fg171rh_gg_function_3(t78, t81, base + 0);
  t82 = ((int64_t)(v1));
  t83 = ((int64_t)(((int64_t)UINT64_C(9))));
  t84 = ((int64_t)(((int64_t)UINT64_C(0))));
  t85 = fg171r_ti(t82, t83, t84);
  v13 = ((int64_t)(t85));
  t86 = ((int64_t)(v13));
  t87 = ((int64_t)(((int64_t)UINT64_C(0))));
  t88 = ((t86) > (t87));
  t89 = t88;
  if (t89) {
  t90 = ((uint64_t)(v8));
  t91 = ((uint64_t)(((uint64_t)(v13))));
  t89 = ((t90) <= (t91));
  }
  t92 = t89;
  t93 = ((int64_t)(v5));
  t94 = ((int64_t)(((int64_t)UINT64_C(2))));
  t95 = ((int64_t)(((int64_t)((uint64_t)(t93) + (uint64_t)(t94)))));
  fg171rh_gg_function_3(t92, t95, base + 0);
  t0 = ((uint64_t)(v7));
  goto fg171rh_gg_done;
  }
fg171rh_gg_done:;
  return t0;
}
static void fg171rh_gg_function_1(int64_t p0, int64_t base) {
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
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
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
  uint8_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint8_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint8_t t73;
  uint8_t t74;
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
  uint8_t t89;
  int64_t t90;
  uint8_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint8_t t95;
  uint8_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  int64_t t110;
  int64_t t111;
  uint64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  uint64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  uint64_t t127;
  uint64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint64_t t134;
  uint64_t t135;
  uint8_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  uint64_t t143;
  uint64_t t144;
  uint64_t t145;
  uint64_t t146;
  uint64_t t147;
  uint8_t t148;
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
  uint8_t t168;
  uint8_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  uint8_t t174;
  uint8_t t175;
  uint64_t t176;
  uint64_t t177;
  uint8_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  uint8_t t182;
  uint8_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  uint8_t t188;
  uint8_t t189;
  int64_t t190;
  int64_t t191;
  uint64_t t192;
  uint64_t t193;
  uint8_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  uint8_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  uint8_t t206;
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
  int64_t t225;
  uint8_t t226;
  uint8_t t227;
  int64_t t228;
  int64_t t229;
  uint8_t t230;
  uint8_t t231;
  int64_t t232;
  int64_t t233;
  uint8_t t234;
  uint8_t t235;
  int64_t t236;
  int64_t t237;
  uint8_t t238;
  int64_t t239;
  uint8_t t240;
  int64_t t241;
  int64_t t242;
  uint8_t t243;
  uint8_t t244;
  int64_t t245;
  int64_t t246;
  uint8_t t247;
  uint8_t t248;
  int64_t t249;
  int64_t t250;
  uint8_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  float t255;
  int64_t t256;
  int64_t t257;
  float t258;
  int64_t t259;
  int64_t t260;
  float t261;
  int64_t t262;
  int64_t t263;
  float t264;
  int64_t t265;
  int64_t t266;
  float t267;
  int64_t t268;
  int64_t t269;
  float t270;
  float t271;
  uint8_t t272;
  uint8_t t273;
  uint8_t t274;
  float t275;
  float t276;
  uint8_t t277;
  int64_t t278;
  float t279;
  uint8_t t280;
  uint8_t t281;
  uint8_t t282;
  float t283;
  float t284;
  uint8_t t285;
  int64_t t286;
  float t287;
  uint8_t t288;
  int64_t t289;
  float t290;
  uint8_t t291;
  int64_t t292;
  float t293;
  uint8_t t294;
  uint8_t t295;
  uint8_t t296;
  float t297;
  float t298;
  uint8_t t299;
  int64_t t300;
  float t301;
  uint8_t t302;
  uint8_t t303;
  uint8_t t304;
  float t305;
  float t306;
  uint8_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  uint8_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  uint8_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  uint8_t t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  uint8_t t336;
  int64_t t337;
  int64_t t338;
  uint8_t t339;
  uint8_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  int64_t t346;
  int64_t t347;
  uint64_t t348;
  uint8_t t349;
  int64_t t350;
  int64_t t351;
  int64_t t352;
  uint64_t t353;
  uint64_t t354;
  uint64_t t355;
  int64_t t356;
  int64_t t357;
  int64_t t358;
  uint64_t t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  uint64_t t363;
  uint64_t t364;
  int64_t t365;
  int64_t t366;
  int64_t t367;
  uint64_t t368;
  uint64_t t369;
  uint64_t t370;
  int64_t t371;
  int64_t t372;
  int64_t t373;
  uint64_t t374;
  uint64_t t375;
  int64_t t376;
  int64_t t377;
  int64_t t378;
  uint64_t t379;
  uint64_t t380;
  uint64_t t381;
  uint64_t t382;
  uint64_t t383;
  uint64_t t384;
  uint8_t t385;
  int64_t t386;
  int64_t t387;
  int64_t t388;
  int64_t t389;
  int64_t t390;
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
  uint8_t t401;
  int64_t t402;
  int64_t t403;
  int64_t t404;
  uint8_t t405;
  int64_t t406;
  int64_t t407;
  int64_t t408;
  uint8_t t409;
  int64_t t410;
  int64_t t411;
  int64_t t412;
  int64_t t413;
  uint64_t t414;
  int64_t t415;
  int64_t t416;
  int64_t t417;
  uint64_t t418;
  int64_t t419;
  int64_t t420;
  int64_t t421;
  uint64_t t422;
  int64_t t423;
  int64_t t424;
  int64_t t425;
  int64_t t426;
  int64_t t427;
  int64_t t428;
  uint8_t t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  int64_t t434;
  int64_t t435;
  int64_t t436;
  uint8_t t437;
  int64_t t438;
  int64_t t439;
  int64_t t440;
  int64_t t441;
  int64_t t442;
  int64_t t443;
  int64_t t444;
  uint8_t t445;
  int64_t t446;
  int64_t t447;
  uint8_t t448;
  uint8_t t449;
  int64_t t450;
  int64_t t451;
  int64_t t452;
  int64_t t453;
  int64_t t454;
  int64_t t455;
  int64_t t456;
  uint64_t t457;
  int64_t t458;
  int64_t t459;
  int64_t t460;
  uint64_t t461;
  uint64_t t462;
  int64_t t463;
  int64_t t464;
  int64_t t465;
  uint64_t t466;
  uint64_t t467;
  uint64_t t468;
  int64_t t469;
  int64_t t470;
  int64_t t471;
  uint64_t t472;
  uint64_t t473;
  int64_t t474;
  int64_t t475;
  int64_t t476;
  uint64_t t477;
  uint64_t t478;
  uint64_t t479;
  uint64_t t480;
  uint64_t t481;
  int64_t t482;
  int64_t t483;
  uint64_t t484;
  uint8_t t485;
  int64_t t486;
  int64_t t487;
  int64_t t488;
  int64_t t489;
  int64_t t490;
  int64_t t491;
  int64_t t492;
  uint8_t t493;
  uint8_t t494;
  int64_t t495;
  int64_t t496;
  uint8_t t497;
  uint8_t t498;
  int64_t t499;
  int64_t t500;
  uint8_t t501;
  uint8_t t502;
  int64_t t503;
  int64_t t504;
  uint8_t t505;
  int64_t t506;
  uint8_t t507;
  int64_t t508;
  int64_t t509;
  uint8_t t510;
  int64_t t511;
  int64_t t512;
  int64_t t513;
  int64_t t514;
  int64_t t515;
  int64_t t516;
  int64_t t517;
  int64_t t518;
  int64_t t519;
  int64_t t520;
  int64_t t521;
  uint8_t t522;
  int64_t t523;
  int64_t t524;
  uint8_t t525;
  int64_t t526;
  int64_t t527;
  int64_t t528;
  int64_t t529;
  int64_t t530;
  int64_t t531;
  int64_t t532;
  int64_t t533;
  int64_t t534;
  int64_t t535;
  int64_t t536;
  int64_t t537;
  int64_t t538;
  uint8_t t539;
  int64_t t540;
  int64_t t541;
  int64_t t542;
  uint64_t t543;
  uint64_t t544;
  uint64_t t545;
  uint64_t t546;
  uint8_t t547;
  uint8_t t548;
  uint8_t t549;
  int64_t t550;
  uint64_t t551;
  uint64_t t552;
  uint64_t t553;
  int64_t t554;
  int64_t t555;
  int64_t t556;
  int64_t t557;
  uint64_t t558;
  uint8_t t559;
  uint8_t t560;
  uint8_t t561;
  int64_t t562;
  int64_t t563;
  int64_t t564;
  uint8_t t565;
  uint64_t t566;
  uint64_t t567;
  uint64_t t568;
  int64_t t569;
  int64_t t570;
  int64_t t571;
  int64_t t572;
  uint64_t t573;
  uint8_t t574;
  uint8_t t575;
  uint8_t t576;
  int64_t t577;
  int64_t t578;
  int64_t t579;
  int64_t t580;
  uint64_t t581;
  int64_t t582;
  int64_t t583;
  int64_t t584;
  uint64_t t585;
  int64_t t586;
  int64_t t587;
  int64_t t588;
  uint64_t t589;
  int64_t t590;
  int64_t t591;
  int64_t t592;
  uint64_t t593;
  int64_t t594;
  int64_t t595;
  int64_t t596;
  uint64_t t597;
  int64_t t598;
  int64_t t599;
  int64_t t600;
  uint64_t t601;
  int64_t t602;
  int64_t t603;
  int64_t t604;
  uint64_t t605;
  int64_t t606;
  int64_t t607;
  int64_t t608;
  uint64_t t609;
  int64_t t610;
  int64_t t611;
  int64_t t612;
  uint64_t t613;
  int64_t t614;
  int64_t t615;
  int64_t t616;
  uint64_t t617;
  int64_t t618;
  int64_t t619;
  uint8_t t620;
  uint64_t t621;
  int64_t t622;
  int64_t t623;
  int64_t t624;
  uint64_t t625;
  int64_t t626;
  int64_t t627;
  uint8_t t628;
  uint64_t t629;
  int64_t t630;
  int64_t t631;
  int64_t t632;
  uint64_t t633;
  int64_t t634;
  int64_t t635;
  int64_t t636;
  uint64_t t637;
  int64_t t638;
  int64_t t639;
  uint8_t t640;
  uint64_t t641;
  int64_t t642;
  int64_t t643;
  int64_t t644;
  uint64_t t645;
  uint8_t t646;
  uint64_t t647;
  int64_t t648;
  int64_t t649;
  int64_t t650;
  uint64_t t651;
  uint8_t t652;
  uint64_t t653;
  int64_t t654;
  int64_t t655;
  int64_t t656;
  int64_t t657;
  uint64_t t658;
  uint64_t t659;
  int64_t t660;
  int64_t t661;
  int64_t t662;
  uint64_t t663;
  uint64_t t664;
  uint64_t t665;
  int64_t t666;
  int64_t t667;
  int64_t t668;
  uint64_t t669;
  uint64_t t670;
  uint64_t t671;
  uint64_t t672;
  uint8_t t673;
  int64_t t674;
  uint64_t t675;
  uint64_t t676;
  uint64_t t677;
  uint64_t t678;
  uint8_t t679;
  int64_t t680;
  int64_t t681;
  int64_t t682;
  int64_t t683;
  uint64_t t684;
  int64_t t685;
  int64_t t686;
  int64_t t687;
  uint64_t t688;
  int64_t t689;
  int64_t t690;
  int64_t t691;
  uint64_t t692;
  int64_t t693;
  int64_t t694;
  int64_t t695;
  uint64_t t696;
  int64_t t697;
  int64_t t698;
  int64_t t699;
  uint64_t t700;
  int64_t t701;
  int64_t t702;
  float t703;
  double t704;
  float t705;
  float t706;
  float t707;
  float t708;
  float t709;
  int64_t t710;
  int64_t t711;
  float t712;
  int64_t t713;
  int64_t t714;
  float t715;
  int64_t t716;
  int64_t t717;
  float t718;
  int64_t t719;
  int64_t t720;
  float t721;
  float t722;
  float t723;
  float t724;
  float t725;
  float t726;
  float t727;
  float t728;
  int64_t t729;
  int64_t t730;
  int64_t t731;
  int64_t t732;
  float t733;
  float t734;
  float t735;
  float t736;
  float t737;
  float t738;
  float t739;
  float t740;
  int64_t t741;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint8_t v5;
  int64_t v6;
  int64_t v7;
  uint8_t v8;
  int64_t v9;
  uint64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  uint8_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  float v20;
  float v21;
  float v22;
  float v23;
  float v24;
  float v25;
  int64_t v26;
  uint64_t v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  uint64_t v31;
  uint64_t v32;
  int64_t v33;
  uint64_t v34;
  uint64_t v35;
  int64_t v36;
  uint64_t v37;
  uint64_t v38;
  v0 = p0;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = fg171r_arg(t0, t1);
  v1 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = ((int64_t)(((int64_t)UINT64_C(2))));
  t5 = fg171r_arg(t3, t4);
  v2 = ((int64_t)(t5));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) >= (t7));
  t9 = t8;
  if (t9) {
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(3))));
  t9 = ((t10) <= (t11));
  }
  t12 = t9;
  t13 = t12;
  if (t13) {
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = fg171r_arg(t14, t15);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t17) != (t18));
  }
  t19 = t13;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t21) != (t22));
  }
  t23 = t20;
  t24 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg171rh_gg_function_3(t23, t24, base + 0);
  t25 = ((int64_t)(v1));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  t27 = ((t25) <= (t26));
  t28 = ((int64_t)(((int64_t)UINT64_C(40))));
  fg171rh_gg_function_3(t27, t28, base + 0);
  t29 = ((int64_t)(v0));
  t30 = fg171r_plan_ptr(t29);
  v3 = ((int64_t)(t30));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(((int64_t)UINT64_C(2))));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = fg171r_ti(t31, t32, t33);
  v4 = ((int64_t)(t34));
  t35 = ((int64_t)(v4));
  t36 = fg171r_f32_type();
  t37 = ((int64_t)(t36));
  t38 = ((t35) == (t37));
  t39 = t38;
  if (!t39) {
  t40 = ((int64_t)(v4));
  t41 = fg171r_f16_type();
  t42 = ((int64_t)(t41));
  t39 = ((t40) == (t42));
  }
  t43 = t39;
  t44 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg171rh_gg_function_3(t43, t44, base + 0);
  t45 = ((int64_t)(v4));
  t46 = fg171r_f16_type();
  t47 = ((int64_t)(t46));
  v5 = ((t45) == (t47));
  t48 = ((int64_t)(v2));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t51 = fg171r_ti(t48, t49, t50);
  t52 = ((int64_t)(t51));
  t54 = v5;
  if (t54) {
  {
  t53 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  t53 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  }
  t55 = ((int64_t)(t53));
  t56 = ((t52) == (t55));
  t57 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg171rh_gg_function_3(t56, t57, base + 0);
  t58 = ((int64_t)(v2));
  t59 = ((int64_t)(((int64_t)UINT64_C(0))));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = fg171r_ti(t58, t59, t60);
  v6 = ((int64_t)(t61));
  t62 = ((int64_t)(v6));
  t63 = ((int64_t)(((int64_t)UINT64_C(0))));
  t64 = ((t62) != (t63));
  t65 = ((int64_t)(((int64_t)UINT64_C(4))));
  fg171rh_gg_function_3(t64, t65, base + 0);
  t66 = ((int64_t)(v6));
  t67 = ((int64_t)(((int64_t)UINT64_C(2))));
  t68 = ((int64_t)(((int64_t)UINT64_C(0))));
  t69 = fg171r_ti(t66, t67, t68);
  v7 = ((int64_t)(t69));
  t70 = ((int64_t)(v7));
  t71 = fg171r_f32_type();
  t72 = ((int64_t)(t71));
  t73 = ((t70) == (t72));
  t74 = t73;
  if (!t74) {
  t75 = ((int64_t)(v7));
  t76 = fg171r_f16_type();
  t77 = ((int64_t)(t76));
  t74 = ((t75) == (t77));
  }
  t78 = t74;
  t79 = ((int64_t)(((int64_t)UINT64_C(5))));
  fg171rh_gg_function_3(t78, t79, base + 0);
  t80 = ((int64_t)(v7));
  t81 = fg171r_f16_type();
  t82 = ((int64_t)(t81));
  v8 = ((t80) == (t82));
  t83 = ((int64_t)(v6));
  t84 = ((int64_t)(((int64_t)UINT64_C(4))));
  t85 = ((int64_t)(((int64_t)UINT64_C(0))));
  t86 = fg171r_ti(t83, t84, t85);
  t87 = ((int64_t)(t86));
  t89 = v8;
  if (t89) {
  {
  t88 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  t88 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  }
  t90 = ((int64_t)(t88));
  t91 = ((t87) == (t90));
  t92 = ((int64_t)(((int64_t)UINT64_C(6))));
  fg171rh_gg_function_3(t91, t92, base + 0);
  t93 = ((int64_t)(v7));
  t94 = ((int64_t)(v4));
  t95 = ((t93) == (t94));
  t96 = t95;
  if (!t96) {
  t97 = ((int64_t)(v7));
  t98 = fg171r_f32_type();
  t99 = ((int64_t)(t98));
  t100 = ((t97) == (t99));
  t101 = t100;
  if (t101) {
  t101 = v5;
  }
  t96 = t101;
  }
  t102 = t96;
  t103 = ((int64_t)(((int64_t)UINT64_C(7))));
  fg171rh_gg_function_3(t102, t103, base + 0);
  t104 = ((int64_t)(v3));
  t105 = ((int64_t)(v6));
  t106 = ((int64_t)(((int64_t)UINT64_C(0))));
  t107 = ((int64_t)(((int64_t)UINT64_C(0))));
  t109 = v8;
  if (t109) {
  {
  t108 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  t108 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  }
  t110 = ((int64_t)(t108));
  t111 = ((int64_t)(((int64_t)UINT64_C(8))));
  t112 = fg171rh_gg_function_9(t104, t105, t106, t107, t110, t111, base + 0);
  (void)(t112);
  t113 = ((int64_t)(v3));
  t114 = ((int64_t)(v2));
  t115 = ((int64_t)(((int64_t)UINT64_C(1))));
  t116 = ((int64_t)(((int64_t)UINT64_C(1))));
  t118 = v5;
  if (t118) {
  {
  t117 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  t117 = ((int64_t)(((int64_t)UINT64_C(4))));
  }
  }
  t119 = ((int64_t)(t117));
  t120 = ((int64_t)(((int64_t)UINT64_C(11))));
  t121 = fg171rh_gg_function_9(t113, t114, t115, t116, t119, t120, base + 0);
  (void)(t121);
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t122 = ((int64_t)(v9));
  t123 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t122) < (t123)))) goto label2;
  {
  t124 = ((int64_t)(v3));
  t125 = ((int64_t)(((int64_t)UINT64_C(1))));
  t126 = ((int64_t)(v9));
  t127 = fg171r_hget(t124, t125, t126);
  t128 = ((uint64_t)(t127));
  t129 = ((int64_t)(v3));
  t130 = ((int64_t)(((int64_t)UINT64_C(1))));
  t131 = ((int64_t)(((int64_t)UINT64_C(4))));
  t132 = ((int64_t)(v9));
  t133 = ((int64_t)(((int64_t)((uint64_t)(t131) + (uint64_t)(t132)))));
  t134 = fg171r_hget(t129, t130, t133);
  t135 = ((uint64_t)(t134));
  t136 = ((t128) == (t135));
  t137 = ((int64_t)(((int64_t)UINT64_C(14))));
  fg171rh_gg_function_3(t136, t137, base + 0);
  t138 = v9;
  t139 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = ((int64_t)((uint64_t)(t138) + (uint64_t)(t139)));
  }
  goto label1;
label2:;
  t140 = ((int64_t)(v3));
  t141 = ((int64_t)(((int64_t)UINT64_C(1))));
  t142 = ((int64_t)(((int64_t)UINT64_C(0))));
  t143 = fg171r_hget(t140, t141, t142);
  v10 = ((uint64_t)(t143));
  t144 = ((uint64_t)(v10));
  t145 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t146 = ((uint64_t)(((uint64_t)fg171rh_gg_umod(t144, t145))));
  t147 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t148 = ((t146) == (t147));
  t149 = ((int64_t)(((int64_t)UINT64_C(15))));
  fg171rh_gg_function_3(t148, t149, base + 0);
  t150 = ((int64_t)(v2));
  t151 = ((int64_t)(((int64_t)UINT64_C(8))));
  t152 = ((int64_t)(((int64_t)UINT64_C(1))));
  t153 = fg171r_ti(t150, t151, t152);
  v11 = ((int64_t)(t153));
  t154 = ((int64_t)(v2));
  t155 = ((int64_t)(((int64_t)UINT64_C(8))));
  t156 = ((int64_t)(((int64_t)UINT64_C(2))));
  t157 = fg171r_ti(t154, t155, t156);
  v12 = ((int64_t)(t157));
  t158 = ((int64_t)(v2));
  t159 = ((int64_t)(((int64_t)UINT64_C(8))));
  t160 = ((int64_t)(((int64_t)UINT64_C(4))));
  t161 = fg171r_ti(t158, t159, t160);
  v13 = ((int64_t)(t161));
  t162 = ((int64_t)(v2));
  t163 = ((int64_t)(((int64_t)UINT64_C(8))));
  t164 = ((int64_t)(((int64_t)UINT64_C(15))));
  t165 = fg171r_ti(t162, t163, t164);
  v14 = ((int64_t)(t165));
  t166 = ((int64_t)(v11));
  t167 = ((int64_t)(((int64_t)UINT64_C(0))));
  t168 = ((t166) > (t167));
  t169 = t168;
  if (t169) {
  t170 = ((int64_t)(v11));
  t171 = ((int64_t)(((int64_t)UINT64_C(2))));
  t172 = ((int64_t)(((int64_t)fg171rh_gg_mod(t170, t171))));
  t173 = ((int64_t)(((int64_t)UINT64_C(0))));
  t169 = ((t172) == (t173));
  }
  t174 = t169;
  t175 = t174;
  if (t175) {
  t176 = ((uint64_t)(((uint64_t)(v11))));
  t177 = ((uint64_t)(v10));
  t175 = ((t176) <= (t177));
  }
  t178 = t175;
  t179 = ((int64_t)(((int64_t)UINT64_C(16))));
  fg171rh_gg_function_3(t178, t179, base + 0);
  t180 = ((int64_t)(v14));
  t181 = ((int64_t)(((int64_t)UINT64_C(0))));
  t182 = ((t180) >= (t181));
  t183 = t182;
  if (t183) {
  t184 = ((int64_t)(v14));
  t185 = ((int64_t)(((int64_t)UINT64_C(2))));
  t186 = ((int64_t)(((int64_t)fg171rh_gg_mod(t184, t185))));
  t187 = ((int64_t)(((int64_t)UINT64_C(0))));
  t183 = ((t186) == (t187));
  }
  t188 = t183;
  t189 = t188;
  if (t189) {
  t190 = ((int64_t)(v14));
  t191 = ((int64_t)(v11));
  t192 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t190) + (uint64_t)(t191)))))));
  t193 = ((uint64_t)(v10));
  t189 = ((t192) <= (t193));
  }
  t194 = t189;
  t195 = ((int64_t)(((int64_t)UINT64_C(17))));
  fg171rh_gg_function_3(t194, t195, base + 0);
  t196 = ((int64_t)(v12));
  t197 = ((int64_t)(((int64_t)UINT64_C(0))));
  t198 = ((t196) >= (t197));
  t199 = ((int64_t)(((int64_t)UINT64_C(18))));
  fg171rh_gg_function_3(t198, t199, base + 0);
  t200 = ((int64_t)(v12));
  t201 = ((int64_t)(((int64_t)UINT64_C(8))));
  t202 = ((int64_t)(((t200) & (t201))));
  t203 = ((int64_t)(((int64_t)UINT64_C(0))));
  v15 = ((t202) != (t203));
  t204 = ((int64_t)(v12));
  t205 = ((int64_t)(((int64_t)UINT64_C(24))));
  t206 = ((t204) == (t205));
  if (t206) {
  {
  t207 = ((int64_t)(((int64_t)UINT64_C(41))));
  fg171r_fail(t207);
  goto fg171rh_gg_done;
  }
  }
  t208 = ((int64_t)(v2));
  t209 = ((int64_t)(((int64_t)UINT64_C(8))));
  t210 = ((int64_t)(((int64_t)UINT64_C(11))));
  t211 = fg171r_ti(t208, t209, t210);
  v16 = ((int64_t)(t211));
  t212 = ((int64_t)(v2));
  t213 = ((int64_t)(((int64_t)UINT64_C(8))));
  t214 = ((int64_t)(((int64_t)UINT64_C(12))));
  t215 = fg171r_ti(t212, t213, t214);
  v17 = ((int64_t)(t215));
  t216 = ((int64_t)(v2));
  t217 = ((int64_t)(((int64_t)UINT64_C(8))));
  t218 = ((int64_t)(((int64_t)UINT64_C(13))));
  t219 = fg171r_ti(t216, t217, t218);
  v18 = ((int64_t)(t219));
  t220 = ((int64_t)(v2));
  t221 = ((int64_t)(((int64_t)UINT64_C(8))));
  t222 = ((int64_t)(((int64_t)UINT64_C(14))));
  t223 = fg171r_ti(t220, t221, t222);
  v19 = ((int64_t)(t223));
  t224 = ((int64_t)(v16));
  t225 = ((int64_t)(((int64_t)UINT64_C(0))));
  t226 = ((t224) >= (t225));
  t227 = t226;
  if (t227) {
  t228 = ((int64_t)(v17));
  t229 = ((int64_t)(((int64_t)UINT64_C(0))));
  t227 = ((t228) >= (t229));
  }
  t230 = t227;
  t231 = t230;
  if (t231) {
  t232 = ((int64_t)(v18));
  t233 = ((int64_t)(((int64_t)UINT64_C(0))));
  t231 = ((t232) >= (t233));
  }
  t234 = t231;
  t235 = t234;
  if (t235) {
  t236 = ((int64_t)(v19));
  t237 = ((int64_t)(((int64_t)UINT64_C(0))));
  t235 = ((t236) >= (t237));
  }
  t238 = t235;
  t239 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg171rh_gg_function_3(t238, t239, base + 0);
  t240 = v15;
  if (t240) {
  {
  t241 = ((int64_t)(v16));
  t242 = ((int64_t)(((int64_t)UINT64_C(0))));
  t243 = ((t241) > (t242));
  t244 = t243;
  if (!t244) {
  t245 = ((int64_t)(v17));
  t246 = ((int64_t)(((int64_t)UINT64_C(0))));
  t244 = ((t245) > (t246));
  }
  t247 = t244;
  t248 = t247;
  if (!t248) {
  t249 = ((int64_t)(v18));
  t250 = ((int64_t)(((int64_t)UINT64_C(0))));
  t248 = ((t249) > (t250));
  }
  t251 = t248;
  t252 = ((int64_t)(((int64_t)UINT64_C(19))));
  fg171rh_gg_function_3(t251, t252, base + 0);
  }
  }
  t253 = ((int64_t)(v2));
  t254 = ((int64_t)(((int64_t)UINT64_C(5))));
  t255 = fg171r_tf(t253, t254);
  v20 = ((float)(t255));
  t256 = ((int64_t)(v2));
  t257 = ((int64_t)(((int64_t)UINT64_C(6))));
  t258 = fg171r_tf(t256, t257);
  v21 = ((float)(t258));
  t259 = ((int64_t)(v2));
  t260 = ((int64_t)(((int64_t)UINT64_C(7))));
  t261 = fg171r_tf(t259, t260);
  v22 = ((float)(t261));
  t262 = ((int64_t)(v2));
  t263 = ((int64_t)(((int64_t)UINT64_C(8))));
  t264 = fg171r_tf(t262, t263);
  v23 = ((float)(t264));
  t265 = ((int64_t)(v2));
  t266 = ((int64_t)(((int64_t)UINT64_C(9))));
  t267 = fg171r_tf(t265, t266);
  v24 = ((float)(t267));
  t268 = ((int64_t)(v2));
  t269 = ((int64_t)(((int64_t)UINT64_C(10))));
  t270 = fg171r_tf(t268, t269);
  v25 = ((float)(t270));
  t271 = ((float)(v20));
  t272 = fg171rh_gg_function_16(t271, base + 0);
  t273 = t272;
  t274 = t273;
  if (t274) {
  t275 = ((float)(v20));
  t276 = ((float)(fg171rh_gg_double_bits(UINT64_C(0))));
  t274 = ((t275) > (t276));
  }
  t277 = t274;
  t278 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg171rh_gg_function_3(t277, t278, base + 0);
  t279 = ((float)(v21));
  t280 = fg171rh_gg_function_16(t279, base + 0);
  t281 = t280;
  t282 = t281;
  if (t282) {
  t283 = ((float)(v21));
  t284 = ((float)(fg171rh_gg_double_bits(UINT64_C(0))));
  t282 = ((t283) > (t284));
  }
  t285 = t282;
  t286 = ((int64_t)(((int64_t)UINT64_C(22))));
  fg171rh_gg_function_3(t285, t286, base + 0);
  t287 = ((float)(v22));
  t288 = fg171rh_gg_function_16(t287, base + 0);
  t289 = ((int64_t)(((int64_t)UINT64_C(23))));
  fg171rh_gg_function_3(t288, t289, base + 0);
  t290 = ((float)(v23));
  t291 = fg171rh_gg_function_16(t290, base + 0);
  t292 = ((int64_t)(((int64_t)UINT64_C(24))));
  fg171rh_gg_function_3(t291, t292, base + 0);
  t293 = ((float)(v24));
  t294 = fg171rh_gg_function_16(t293, base + 0);
  t295 = t294;
  t296 = t295;
  if (t296) {
  t297 = ((float)(v24));
  t298 = ((float)(fg171rh_gg_double_bits(UINT64_C(0))));
  t296 = ((t297) > (t298));
  }
  t299 = t296;
  t300 = ((int64_t)(((int64_t)UINT64_C(25))));
  fg171rh_gg_function_3(t299, t300, base + 0);
  t301 = ((float)(v25));
  t302 = fg171rh_gg_function_16(t301, base + 0);
  t303 = t302;
  t304 = t303;
  if (t304) {
  t305 = ((float)(v25));
  t306 = ((float)(fg171rh_gg_double_bits(UINT64_C(0))));
  t304 = ((t305) > (t306));
  }
  t307 = t304;
  t308 = ((int64_t)(((int64_t)UINT64_C(26))));
  fg171rh_gg_function_3(t307, t308, base + 0);
  t309 = ((int64_t)(v13));
  t310 = ((int64_t)(((int64_t)UINT64_C(0))));
  t311 = ((t309) > (t310));
  t312 = ((int64_t)(((int64_t)UINT64_C(27))));
  fg171rh_gg_function_3(t311, t312, base + 0);
  t313 = ((int64_t)(v2));
  t314 = ((int64_t)(((int64_t)UINT64_C(0))));
  t315 = ((int64_t)(((int64_t)UINT64_C(1))));
  t316 = fg171r_ti(t313, t314, t315);
  v26 = ((int64_t)(t316));
  t317 = ((int64_t)(v26));
  t318 = ((int64_t)(((int64_t)UINT64_C(0))));
  t319 = ((t317) != (t318));
  t320 = ((int64_t)(((int64_t)UINT64_C(28))));
  fg171rh_gg_function_3(t319, t320, base + 0);
  t321 = ((int64_t)(v26));
  t322 = ((int64_t)(((int64_t)UINT64_C(2))));
  t323 = ((int64_t)(((int64_t)UINT64_C(0))));
  t324 = fg171r_ti(t321, t322, t323);
  t325 = ((int64_t)(t324));
  t326 = fg171r_i32_type();
  t327 = ((int64_t)(t326));
  t328 = ((t325) == (t327));
  t329 = ((int64_t)(((int64_t)UINT64_C(29))));
  fg171rh_gg_function_3(t328, t329, base + 0);
  t330 = ((int64_t)(v26));
  t331 = ((int64_t)(((int64_t)UINT64_C(4))));
  t332 = ((int64_t)(((int64_t)UINT64_C(0))));
  t333 = fg171r_ti(t330, t331, t332);
  t334 = ((int64_t)(t333));
  t335 = ((int64_t)(((int64_t)UINT64_C(4))));
  t336 = ((t334) == (t335));
  t337 = ((int64_t)(((int64_t)UINT64_C(30))));
  fg171rh_gg_function_3(t336, t337, base + 0);
  t338 = ((int64_t)(v26));
  t339 = fg171r_contiguous(t338);
  t340 = t339;
  t341 = ((int64_t)(((int64_t)UINT64_C(31))));
  fg171rh_gg_function_3(t340, t341, base + 0);
  t342 = ((int64_t)(v3));
  t343 = ((int64_t)(v26));
  t344 = ((int64_t)(((int64_t)UINT64_C(2))));
  t345 = ((int64_t)(((int64_t)UINT64_C(2))));
  t346 = ((int64_t)(((int64_t)UINT64_C(4))));
  t347 = ((int64_t)(((int64_t)UINT64_C(33))));
  t348 = fg171rh_gg_function_9(t342, t343, t344, t345, t346, t347, base + 0);
  (void)(t348);
  t349 = v15;
  if (t349) {
  {
  t350 = ((int64_t)(v3));
  t351 = ((int64_t)(((int64_t)UINT64_C(1))));
  t352 = ((int64_t)(((int64_t)UINT64_C(2))));
  t353 = fg171r_hget(t350, t351, t352);
  t354 = ((uint64_t)(t353));
  t355 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v27 = fg171rh_gg_function_11(t354, t355, base + 0);
  }
  } else {
  {
  t356 = ((int64_t)(v3));
  t357 = ((int64_t)(((int64_t)UINT64_C(1))));
  t358 = ((int64_t)(((int64_t)UINT64_C(2))));
  t359 = fg171r_hget(t356, t357, t358);
  v27 = ((uint64_t)(t359));
  }
  }
  t360 = ((int64_t)(v3));
  t361 = ((int64_t)(((int64_t)UINT64_C(1))));
  t362 = ((int64_t)(((int64_t)UINT64_C(8))));
  t363 = fg171r_hget(t360, t361, t362);
  t364 = ((uint64_t)(t363));
  t365 = ((int64_t)(v3));
  t366 = ((int64_t)(((int64_t)UINT64_C(1))));
  t367 = ((int64_t)(((int64_t)UINT64_C(9))));
  t368 = fg171r_hget(t365, t366, t367);
  t369 = ((uint64_t)(t368));
  t370 = fg171rh_gg_function_11(t364, t369, base + 0);
  t371 = ((int64_t)(v3));
  t372 = ((int64_t)(((int64_t)UINT64_C(1))));
  t373 = ((int64_t)(((int64_t)UINT64_C(10))));
  t374 = fg171r_hget(t371, t372, t373);
  t375 = ((uint64_t)(t374));
  t376 = ((int64_t)(v3));
  t377 = ((int64_t)(((int64_t)UINT64_C(1))));
  t378 = ((int64_t)(((int64_t)UINT64_C(11))));
  t379 = fg171r_hget(t376, t377, t378);
  t380 = ((uint64_t)(t379));
  t381 = fg171rh_gg_function_11(t375, t380, base + 0);
  t382 = fg171rh_gg_function_11(t370, t381, base + 0);
  t383 = ((uint64_t)(t382));
  t384 = ((uint64_t)(v27));
  t385 = ((t383) >= (t384));
  t386 = ((int64_t)(((int64_t)UINT64_C(32))));
  fg171rh_gg_function_3(t385, t386, base + 0);
  t387 = ((int64_t)(v6));
  t388 = ((int64_t)(((int64_t)UINT64_C(1))));
  t389 = ((int64_t)(((int64_t)UINT64_C(0))));
  t390 = fg171r_ti(t387, t388, t389);
  v28 = ((int64_t)(t390));
  t391 = ((int64_t)(v2));
  t392 = ((int64_t)(((int64_t)UINT64_C(1))));
  t393 = ((int64_t)(((int64_t)UINT64_C(0))));
  t394 = fg171r_ti(t391, t392, t393);
  v29 = ((int64_t)(t394));
  t395 = ((int64_t)(v26));
  t396 = ((int64_t)(((int64_t)UINT64_C(1))));
  t397 = ((int64_t)(((int64_t)UINT64_C(0))));
  t398 = fg171r_ti(t395, t396, t397);
  v30 = ((int64_t)(t398));
  t399 = ((int64_t)(v28));
  t400 = ((int64_t)(((int64_t)UINT64_C(0))));
  t401 = ((t399) != (t400));
  t402 = ((int64_t)(((int64_t)UINT64_C(46))));
  fg171rh_gg_function_3(t401, t402, base + 0);
  t403 = ((int64_t)(v29));
  t404 = ((int64_t)(((int64_t)UINT64_C(0))));
  t405 = ((t403) != (t404));
  t406 = ((int64_t)(((int64_t)UINT64_C(47))));
  fg171rh_gg_function_3(t405, t406, base + 0);
  t407 = ((int64_t)(v30));
  t408 = ((int64_t)(((int64_t)UINT64_C(0))));
  t409 = ((t407) != (t408));
  t410 = ((int64_t)(((int64_t)UINT64_C(48))));
  fg171rh_gg_function_3(t409, t410, base + 0);
  t411 = ((int64_t)(v3));
  t412 = ((int64_t)(((int64_t)UINT64_C(0))));
  t413 = ((int64_t)(((int64_t)UINT64_C(0))));
  t414 = ((uint64_t)(((uint64_t)(v28))));
  fg171r_hput(t411, t412, t413, t414);
  t415 = ((int64_t)(v3));
  t416 = ((int64_t)(((int64_t)UINT64_C(0))));
  t417 = ((int64_t)(((int64_t)UINT64_C(1))));
  t418 = ((uint64_t)(((uint64_t)(v29))));
  fg171r_hput(t415, t416, t417, t418);
  t419 = ((int64_t)(v3));
  t420 = ((int64_t)(((int64_t)UINT64_C(0))));
  t421 = ((int64_t)(((int64_t)UINT64_C(2))));
  t422 = ((uint64_t)(((uint64_t)(v30))));
  fg171r_hput(t419, t420, t421, t422);
  v31 = ((uint64_t)(((int64_t)UINT64_C(0))));
  v32 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t423 = ((int64_t)(v2));
  t424 = ((int64_t)(((int64_t)UINT64_C(0))));
  t425 = ((int64_t)(((int64_t)UINT64_C(2))));
  t426 = fg171r_ti(t423, t424, t425);
  v33 = ((int64_t)(t426));
  t427 = ((int64_t)(v33));
  t428 = ((int64_t)(((int64_t)UINT64_C(0))));
  t429 = ((t427) != (t428));
  if (t429) {
  {
  t430 = ((int64_t)(v33));
  t431 = ((int64_t)(((int64_t)UINT64_C(2))));
  t432 = ((int64_t)(((int64_t)UINT64_C(0))));
  t433 = fg171r_ti(t430, t431, t432);
  t434 = ((int64_t)(t433));
  t435 = fg171r_f32_type();
  t436 = ((int64_t)(t435));
  t437 = ((t434) == (t436));
  t438 = ((int64_t)(((int64_t)UINT64_C(36))));
  fg171rh_gg_function_3(t437, t438, base + 0);
  t439 = ((int64_t)(v33));
  t440 = ((int64_t)(((int64_t)UINT64_C(4))));
  t441 = ((int64_t)(((int64_t)UINT64_C(0))));
  t442 = fg171r_ti(t439, t440, t441);
  t443 = ((int64_t)(t442));
  t444 = ((int64_t)(((int64_t)UINT64_C(4))));
  t445 = ((t443) == (t444));
  t446 = ((int64_t)(((int64_t)UINT64_C(37))));
  fg171rh_gg_function_3(t445, t446, base + 0);
  t447 = ((int64_t)(v33));
  t448 = fg171r_contiguous(t447);
  t449 = t448;
  t450 = ((int64_t)(((int64_t)UINT64_C(38))));
  fg171rh_gg_function_3(t449, t450, base + 0);
  t451 = ((int64_t)(v3));
  t452 = ((int64_t)(v33));
  t453 = ((int64_t)(((int64_t)UINT64_C(3))));
  t454 = ((int64_t)(((int64_t)UINT64_C(3))));
  t455 = ((int64_t)(((int64_t)UINT64_C(4))));
  t456 = ((int64_t)(((int64_t)UINT64_C(42))));
  t457 = fg171rh_gg_function_9(t451, t452, t453, t454, t455, t456, base + 0);
  (void)(t457);
  t458 = ((int64_t)(v3));
  t459 = ((int64_t)(((int64_t)UINT64_C(1))));
  t460 = ((int64_t)(((int64_t)UINT64_C(12))));
  t461 = fg171r_hget(t458, t459, t460);
  t462 = ((uint64_t)(t461));
  t463 = ((int64_t)(v3));
  t464 = ((int64_t)(((int64_t)UINT64_C(1))));
  t465 = ((int64_t)(((int64_t)UINT64_C(13))));
  t466 = fg171r_hget(t463, t464, t465);
  t467 = ((uint64_t)(t466));
  t468 = fg171rh_gg_function_11(t462, t467, base + 0);
  t469 = ((int64_t)(v3));
  t470 = ((int64_t)(((int64_t)UINT64_C(1))));
  t471 = ((int64_t)(((int64_t)UINT64_C(14))));
  t472 = fg171r_hget(t469, t470, t471);
  t473 = ((uint64_t)(t472));
  t474 = ((int64_t)(v3));
  t475 = ((int64_t)(((int64_t)UINT64_C(1))));
  t476 = ((int64_t)(((int64_t)UINT64_C(15))));
  t477 = fg171r_hget(t474, t475, t476);
  t478 = ((uint64_t)(t477));
  t479 = fg171rh_gg_function_11(t473, t478, base + 0);
  t480 = fg171rh_gg_function_11(t468, t479, base + 0);
  t481 = ((uint64_t)(t480));
  t482 = ((int64_t)(v11));
  t483 = ((int64_t)(((int64_t)UINT64_C(2))));
  t484 = ((uint64_t)(((uint64_t)(((int64_t)fg171rh_gg_div(t482, t483))))));
  t485 = ((t481) >= (t484));
  t486 = ((int64_t)(((int64_t)UINT64_C(39))));
  fg171rh_gg_function_3(t485, t486, base + 0);
  t487 = ((int64_t)(v33));
  t488 = ((int64_t)(((int64_t)UINT64_C(1))));
  t489 = ((int64_t)(((int64_t)UINT64_C(0))));
  t490 = fg171r_ti(t487, t488, t489);
  v31 = ((uint64_t)(((uint64_t)(t490))));
  v32 = ((uint64_t)(((int64_t)UINT64_C(1))));
  }
  }
  t491 = ((int64_t)(v12));
  t492 = ((int64_t)(((int64_t)UINT64_C(0))));
  t493 = ((t491) == (t492));
  t494 = t493;
  if (!t494) {
  t495 = ((int64_t)(v12));
  t496 = ((int64_t)(((int64_t)UINT64_C(2))));
  t494 = ((t495) == (t496));
  }
  t497 = t494;
  t498 = t497;
  if (!t498) {
  t499 = ((int64_t)(v12));
  t500 = ((int64_t)(((int64_t)UINT64_C(8))));
  t498 = ((t499) == (t500));
  }
  t501 = t498;
  t502 = t501;
  if (!t502) {
  t503 = ((int64_t)(v12));
  t504 = ((int64_t)(((int64_t)UINT64_C(40))));
  t502 = ((t503) == (t504));
  }
  t505 = t502;
  t506 = ((int64_t)(((int64_t)UINT64_C(52))));
  fg171rh_gg_function_3(t505, t506, base + 0);
  t507 = v15;
  if (t507) {
  {
  t508 = ((int64_t)(v7));
  t509 = ((int64_t)(v4));
  t510 = ((t508) == (t509));
  t511 = ((int64_t)(((int64_t)UINT64_C(53))));
  fg171rh_gg_function_3(t510, t511, base + 0);
  }
  }
  t512 = ((int64_t)(v6));
  t513 = ((int64_t)(((int64_t)UINT64_C(9))));
  t514 = ((int64_t)(((int64_t)UINT64_C(0))));
  t515 = fg171r_ti(t512, t513, t514);
  v34 = ((uint64_t)(((uint64_t)(t515))));
  t516 = ((int64_t)(v2));
  t517 = ((int64_t)(((int64_t)UINT64_C(9))));
  t518 = ((int64_t)(((int64_t)UINT64_C(0))));
  t519 = fg171r_ti(t516, t517, t518);
  v35 = ((uint64_t)(((uint64_t)(t519))));
  t520 = ((int64_t)(v28));
  t521 = ((int64_t)(v29));
  t522 = ((t520) == (t521));
  if (t522) {
  {
  t523 = ((int64_t)(v7));
  t524 = ((int64_t)(v4));
  t525 = ((t523) == (t524));
  t526 = ((int64_t)(((int64_t)UINT64_C(54))));
  fg171rh_gg_function_3(t525, t526, base + 0);
  v36 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t527 = ((int64_t)(v36));
  t528 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t527) < (t528)))) goto label5;
  {
  t529 = ((int64_t)(v6));
  t530 = ((int64_t)(((int64_t)UINT64_C(4))));
  t531 = ((int64_t)(v36));
  t532 = fg171r_ti(t529, t530, t531);
  t533 = ((int64_t)(t532));
  t534 = ((int64_t)(v2));
  t535 = ((int64_t)(((int64_t)UINT64_C(4))));
  t536 = ((int64_t)(v36));
  t537 = fg171r_ti(t534, t535, t536);
  t538 = ((int64_t)(t537));
  t539 = ((t533) == (t538));
  t540 = ((int64_t)(((int64_t)UINT64_C(54))));
  fg171rh_gg_function_3(t539, t540, base + 0);
  t541 = v36;
  t542 = ((int64_t)(((int64_t)UINT64_C(1))));
  v36 = ((int64_t)((uint64_t)(t541) + (uint64_t)(t542)));
  }
  goto label4;
label5:;
  }
  } else {
  {
  t543 = ((uint64_t)(((uint64_t)(v28))));
  t544 = ((uint64_t)(v34));
  t545 = ((uint64_t)(((uint64_t)(v29))));
  t546 = ((uint64_t)(v35));
  t547 = fg171rh_gg_function_19(t543, t544, t545, t546, base + 0);
  t548 = t547;
  t549 = (!(t548));
  t550 = ((int64_t)(((int64_t)UINT64_C(54))));
  fg171rh_gg_function_3(t549, t550, base + 0);
  }
  }
  t551 = ((uint64_t)(((uint64_t)(v29))));
  t552 = ((uint64_t)(v35));
  t553 = ((uint64_t)(((uint64_t)(v30))));
  t554 = ((int64_t)(v26));
  t555 = ((int64_t)(((int64_t)UINT64_C(9))));
  t556 = ((int64_t)(((int64_t)UINT64_C(0))));
  t557 = fg171r_ti(t554, t555, t556);
  t558 = ((uint64_t)(((uint64_t)(t557))));
  t559 = fg171rh_gg_function_19(t551, t552, t553, t558, base + 0);
  t560 = t559;
  t561 = (!(t560));
  t562 = ((int64_t)(((int64_t)UINT64_C(54))));
  fg171rh_gg_function_3(t561, t562, base + 0);
  t563 = ((int64_t)(v33));
  t564 = ((int64_t)(((int64_t)UINT64_C(0))));
  t565 = ((t563) != (t564));
  if (t565) {
  {
  t566 = ((uint64_t)(((uint64_t)(v29))));
  t567 = ((uint64_t)(v35));
  t568 = ((uint64_t)(v31));
  t569 = ((int64_t)(v33));
  t570 = ((int64_t)(((int64_t)UINT64_C(9))));
  t571 = ((int64_t)(((int64_t)UINT64_C(0))));
  t572 = fg171r_ti(t569, t570, t571);
  t573 = ((uint64_t)(((uint64_t)(t572))));
  t574 = fg171rh_gg_function_19(t566, t567, t568, t573, base + 0);
  t575 = t574;
  t576 = (!(t575));
  t577 = ((int64_t)(((int64_t)UINT64_C(54))));
  fg171rh_gg_function_3(t576, t577, base + 0);
  }
  }
  t578 = ((int64_t)(v3));
  t579 = ((int64_t)(((int64_t)UINT64_C(0))));
  t580 = ((int64_t)(((int64_t)UINT64_C(3))));
  t581 = ((uint64_t)(v31));
  fg171r_hput(t578, t579, t580, t581);
  t582 = ((int64_t)(v3));
  t583 = ((int64_t)(((int64_t)UINT64_C(0))));
  t584 = ((int64_t)(((int64_t)UINT64_C(4))));
  t585 = ((uint64_t)(((int64_t)UINT64_C(0))));
  fg171r_hput(t582, t583, t584, t585);
  t586 = ((int64_t)(v3));
  t587 = ((int64_t)(((int64_t)UINT64_C(3))));
  t588 = ((int64_t)(((int64_t)UINT64_C(0))));
  t589 = ((uint64_t)(((uint64_t)(v12))));
  fg171r_hput(t586, t587, t588, t589);
  t590 = ((int64_t)(v3));
  t591 = ((int64_t)(((int64_t)UINT64_C(3))));
  t592 = ((int64_t)(((int64_t)UINT64_C(1))));
  t593 = ((uint64_t)(((uint64_t)(v11))));
  fg171r_hput(t590, t591, t592, t593);
  t594 = ((int64_t)(v3));
  t595 = ((int64_t)(((int64_t)UINT64_C(3))));
  t596 = ((int64_t)(((int64_t)UINT64_C(2))));
  t597 = ((uint64_t)(((uint64_t)(v14))));
  fg171r_hput(t594, t595, t596, t597);
  t598 = ((int64_t)(v3));
  t599 = ((int64_t)(((int64_t)UINT64_C(3))));
  t600 = ((int64_t)(((int64_t)UINT64_C(3))));
  t601 = ((uint64_t)(((uint64_t)(v16))));
  fg171r_hput(t598, t599, t600, t601);
  t602 = ((int64_t)(v3));
  t603 = ((int64_t)(((int64_t)UINT64_C(3))));
  t604 = ((int64_t)(((int64_t)UINT64_C(4))));
  t605 = ((uint64_t)(((uint64_t)(v17))));
  fg171r_hput(t602, t603, t604, t605);
  t606 = ((int64_t)(v3));
  t607 = ((int64_t)(((int64_t)UINT64_C(3))));
  t608 = ((int64_t)(((int64_t)UINT64_C(5))));
  t609 = ((uint64_t)(((uint64_t)(v18))));
  fg171r_hput(t606, t607, t608, t609);
  t610 = ((int64_t)(v3));
  t611 = ((int64_t)(((int64_t)UINT64_C(3))));
  t612 = ((int64_t)(((int64_t)UINT64_C(6))));
  t613 = ((uint64_t)(((uint64_t)(v19))));
  fg171r_hput(t610, t611, t612, t613);
  t614 = ((int64_t)(v3));
  t615 = ((int64_t)(((int64_t)UINT64_C(3))));
  t616 = ((int64_t)(((int64_t)UINT64_C(7))));
  t618 = ((int64_t)(v1));
  t619 = ((int64_t)(((int64_t)UINT64_C(0))));
  t620 = ((t618) == (t619));
  if (t620) {
  {
  t617 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t617 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t621 = ((uint64_t)(t617));
  fg171r_hput(t614, t615, t616, t621);
  t622 = ((int64_t)(v3));
  t623 = ((int64_t)(((int64_t)UINT64_C(3))));
  t624 = ((int64_t)(((int64_t)UINT64_C(8))));
  t626 = ((int64_t)(v28));
  t627 = ((int64_t)(v29));
  t628 = ((t626) == (t627));
  if (t628) {
  {
  t625 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t625 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t629 = ((uint64_t)(t625));
  fg171r_hput(t622, t623, t624, t629);
  t630 = ((int64_t)(v3));
  t631 = ((int64_t)(((int64_t)UINT64_C(3))));
  t632 = ((int64_t)(((int64_t)UINT64_C(9))));
  t633 = ((uint64_t)(v32));
  fg171r_hput(t630, t631, t632, t633);
  t634 = ((int64_t)(v3));
  t635 = ((int64_t)(((int64_t)UINT64_C(3))));
  t636 = ((int64_t)(((int64_t)UINT64_C(10))));
  t638 = ((int64_t)(v12));
  t639 = ((int64_t)(((int64_t)UINT64_C(40))));
  t640 = ((t638) == (t639));
  if (t640) {
  {
  t637 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t637 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t641 = ((uint64_t)(t637));
  fg171r_hput(t634, t635, t636, t641);
  t642 = ((int64_t)(v3));
  t643 = ((int64_t)(((int64_t)UINT64_C(3))));
  t644 = ((int64_t)(((int64_t)UINT64_C(11))));
  t646 = v8;
  if (t646) {
  {
  t645 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t645 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t647 = ((uint64_t)(t645));
  fg171r_hput(t642, t643, t644, t647);
  t648 = ((int64_t)(v3));
  t649 = ((int64_t)(((int64_t)UINT64_C(3))));
  t650 = ((int64_t)(((int64_t)UINT64_C(12))));
  t652 = v5;
  if (t652) {
  {
  t651 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t651 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t653 = ((uint64_t)(t651));
  fg171r_hput(t648, t649, t650, t653);
  t654 = ((int64_t)(v3));
  fg171rh_gg_function_20(t654, base + 0);
  t655 = ((int64_t)(v3));
  t656 = ((int64_t)(((int64_t)UINT64_C(1))));
  t657 = ((int64_t)(((int64_t)UINT64_C(1))));
  t658 = fg171r_hget(t655, t656, t657);
  t659 = ((uint64_t)(t658));
  t660 = ((int64_t)(v3));
  t661 = ((int64_t)(((int64_t)UINT64_C(1))));
  t662 = ((int64_t)(((int64_t)UINT64_C(2))));
  t663 = fg171r_hget(t660, t661, t662);
  t664 = ((uint64_t)(t663));
  t665 = fg171rh_gg_function_11(t659, t664, base + 0);
  t666 = ((int64_t)(v3));
  t667 = ((int64_t)(((int64_t)UINT64_C(1))));
  t668 = ((int64_t)(((int64_t)UINT64_C(3))));
  t669 = fg171r_hget(t666, t667, t668);
  t670 = ((uint64_t)(t669));
  v37 = fg171rh_gg_function_11(t665, t670, base + 0);
  t671 = ((uint64_t)(v37));
  t672 = ((uint64_t)(((int64_t)UINT64_C(2147483647))));
  t673 = ((t671) <= (t672));
  t674 = ((int64_t)(((int64_t)UINT64_C(45))));
  fg171rh_gg_function_3(t673, t674, base + 0);
  t675 = ((uint64_t)(v10));
  t676 = ((uint64_t)(((int64_t)UINT64_C(512))));
  v38 = fg171rh_gg_function_21(t675, t676, base + 0);
  t677 = ((uint64_t)(v38));
  t678 = ((uint64_t)(((int64_t)UINT64_C(65535))));
  t679 = ((t677) <= (t678));
  t680 = ((int64_t)(((int64_t)UINT64_C(45))));
  fg171rh_gg_function_3(t679, t680, base + 0);
  t681 = ((int64_t)(v3));
  t682 = ((int64_t)(((int64_t)UINT64_C(3))));
  t683 = ((int64_t)(((int64_t)UINT64_C(13))));
  t684 = ((uint64_t)(v37));
  fg171r_hput(t681, t682, t683, t684);
  t685 = ((int64_t)(v3));
  t686 = ((int64_t)(((int64_t)UINT64_C(3))));
  t687 = ((int64_t)(((int64_t)UINT64_C(14))));
  t688 = ((uint64_t)(v38));
  fg171r_hput(t685, t686, t687, t688);
  t689 = ((int64_t)(v3));
  t690 = ((int64_t)(((int64_t)UINT64_C(3))));
  t691 = ((int64_t)(((int64_t)UINT64_C(15))));
  t692 = ((uint64_t)(((int64_t)UINT64_C(1))));
  fg171r_hput(t689, t690, t691, t692);
  t693 = ((int64_t)(v3));
  t694 = ((int64_t)(((int64_t)UINT64_C(3))));
  t695 = ((int64_t)(((int64_t)UINT64_C(16))));
  t696 = ((uint64_t)(((int64_t)UINT64_C(256))));
  fg171r_hput(t693, t694, t695, t696);
  t697 = ((int64_t)(v3));
  t698 = ((int64_t)(((int64_t)UINT64_C(3))));
  t699 = ((int64_t)(((int64_t)UINT64_C(17))));
  t700 = ((uint64_t)(((int64_t)UINT64_C(1))));
  fg171r_hput(t697, t698, t699, t700);
  t701 = ((int64_t)(v3));
  t702 = ((int64_t)(((int64_t)UINT64_C(0))));
  t703 = ((float)(v20));
  t704 = fg171rh_gg_double_bits(UINT64_C(4611686018427387904));
  t705 = ((float)((-(t704))));
  t706 = ((float)(((float)(v11))));
  t707 = ((float)(((t705) / (t706))));
  t708 = fg171r_hpow(t703, t707);
  t709 = ((float)(t708));
  fg171r_hfloat(t701, t702, t709);
  t710 = ((int64_t)(v3));
  t711 = ((int64_t)(((int64_t)UINT64_C(1))));
  t712 = ((float)(v21));
  fg171r_hfloat(t710, t711, t712);
  t713 = ((int64_t)(v3));
  t714 = ((int64_t)(((int64_t)UINT64_C(2))));
  t715 = ((float)(v22));
  fg171r_hfloat(t713, t714, t715);
  t716 = ((int64_t)(v3));
  t717 = ((int64_t)(((int64_t)UINT64_C(3))));
  t718 = ((float)(v23));
  fg171r_hfloat(t716, t717, t718);
  t719 = ((int64_t)(v3));
  t720 = ((int64_t)(((int64_t)UINT64_C(4))));
  t721 = ((float)(fg171rh_gg_double_bits(UINT64_C(0))));
  t722 = ((float)(((float)(v11))));
  t723 = ((float)(((float)(v13))));
  t724 = ((float)(v24));
  t725 = ((float)(v20));
  t726 = fg171rh_gg_function_24(t722, t723, t724, t725, base + 0);
  t727 = fg171rh_gg_function_26(t726, base + 0);
  t728 = fg171rh_gg_function_28(t721, t727, base + 0);
  fg171r_hfloat(t719, t720, t728);
  t729 = ((int64_t)(v3));
  t730 = ((int64_t)(((int64_t)UINT64_C(5))));
  t731 = ((int64_t)(v11));
  t732 = ((int64_t)(((int64_t)UINT64_C(1))));
  t733 = ((float)(((float)(((int64_t)((uint64_t)(t731) - (uint64_t)(t732)))))));
  t734 = ((float)(((float)(v11))));
  t735 = ((float)(((float)(v13))));
  t736 = ((float)(v25));
  t737 = ((float)(v20));
  t738 = fg171rh_gg_function_24(t734, t735, t736, t737, base + 0);
  t739 = fg171rh_gg_function_29(t738, base + 0);
  t740 = fg171rh_gg_function_31(t733, t739, base + 0);
  fg171r_hfloat(t729, t730, t740);
  t741 = ((int64_t)(v0));
  fg171r_launch(t741);
  }
fg171rh_gg_done:;
}
static void fg171rh_gg_function_0(int64_t base) {
  int64_t t0;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  fg171rh_gg_function_1(t0, base + 0);
  }
fg171rh_gg_done:;
}
static int fg171rh_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg171rh_gg_argc = argc; fg171rh_gg_argv = argv;
  fg171rh_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg171rh_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg171rh_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg171rh_gg_program(argc, argv);
#endif
}
