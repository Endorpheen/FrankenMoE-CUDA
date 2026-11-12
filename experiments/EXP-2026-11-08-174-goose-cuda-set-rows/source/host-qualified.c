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
typedef struct { void *data; int64_t len; } fg176rs_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg176rs_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg176rs_gg_stack;
typedef struct { fg176rs_gg_header *header; fg176rs_gg_stack *stack; } fg176rs_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg176rs_gg_stack fg176rs_gg_stacks[GG_STACK_COUNT];
static int fg176rs_gg_argc;
static char **fg176rs_gg_argv;
static void fg176rs_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg176rs_gg_stack *fg176rs_gg_get_stack(int64_t id) {
    fg176rs_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg176rs_gg_fail("data stack limit exceeded");
    s = &fg176rs_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg176rs_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg176rs_gg_allocate(fg176rs_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg176rs_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg176rs_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg176rs_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg176rs_gg_reference fg176rs_gg_ref(fg176rs_gg_header *h, fg176rs_gg_stack *s) { fg176rs_gg_reference r; r.header=h; r.stack=s; return r; }
static fg176rs_gg_slice fg176rs_gg_view(void *p, int64_t n) { fg176rs_gg_slice v; v.data=p; v.len=n; return v; }
static fg176rs_gg_slice fg176rs_gg_byte_view(fg176rs_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg176rs_gg_fail("byte view size overflow");
    return fg176rs_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg176rs_gg_slice fg176rs_gg_sub(fg176rs_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg176rs_gg_fail("slice out of bounds");
    return fg176rs_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg176rs_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg176rs_gg_fail("array index out of bounds"); return i; }
static void fg176rs_gg_append(fg176rs_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg176rs_gg_fail("invalid array size");
    to=fg176rs_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg176rs_gg_resize(fg176rs_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg176rs_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg176rs_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg176rs_gg_fail("invalid array size");
        p=fg176rs_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg176rs_gg_div(int64_t a, int64_t b) { if (!b) fg176rs_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg176rs_gg_fail("division overflow"); return a/b; }
static int64_t fg176rs_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg176rs_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg176rs_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg176rs_gg_fail("division by zero");return a/b; }
static uint64_t fg176rs_gg_umod(uint64_t a,uint64_t b) { if(!b)fg176rs_gg_fail("division by zero");return a%b; }
static int fg176rs_gg_equal(fg176rs_gg_slice a, fg176rs_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg176rs_gg_text(fg176rs_gg_reference out, fg176rs_gg_slice v) { fg176rs_gg_append(out,v.data,v.len,1); }
static void fg176rs_gg_integer(fg176rs_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg176rs_gg_append(out,b,k,1); }
static void fg176rs_gg_unsigned(fg176rs_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg176rs_gg_append(out,b,k,1); }
static double fg176rs_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg176rs_ggf_fmt_f64(uint8_t *out,double n);
static int64_t fg176rs_ggf_fmt_f32(uint8_t *out,float n);
static void fg176rs_gg_float(fg176rs_gg_reference out,double n) { uint8_t b[64];int64_t k=fg176rs_ggf_fmt_f64(b,n);fg176rs_gg_append(out,b,k,1); }
static void fg176rs_gg_float32(fg176rs_gg_reference out,float n) { uint8_t b[64];int64_t k=fg176rs_ggf_fmt_f32(b,n);fg176rs_gg_append(out,b,k,1); }
static void fg176rs_gg_bool(fg176rs_gg_reference out,uint8_t n) { fg176rs_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg176rs_gg_write(FILE *file,fg176rs_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg176rs_gg_fail("write failed"); }
static char *fg176rs_gg_cstring(fg176rs_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg176rs_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t fg176rs_gs_os_read_file(fg176rs_gg_slice path,fg176rs_gg_reference out) {
    char *p=fg176rs_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg176rs_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t fg176rs_gs_os_write_file(fg176rs_gg_slice path,fg176rs_gg_slice data) {
    char *p=fg176rs_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t fg176rs_gs_os_file_exists(fg176rs_gg_slice path) { char*p=fg176rs_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void fg176rs_gs_os_write_stdout(fg176rs_gg_slice s) { fg176rs_gg_write(stdout,s); }
static void fg176rs_gs_os_write_stderr(fg176rs_gg_slice s) { fg176rs_gg_write(stderr,s); }
static void fg176rs_gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t fg176rs_gs_os_arg_count(void) { return fg176rs_gg_argc; }
static void fg176rs_gs_os_arg(int64_t i,fg176rs_gg_reference out) { if(i>=0 && i<fg176rs_gg_argc)fg176rs_gg_append(out,fg176rs_gg_argv[i],(int64_t)strlen(fg176rs_gg_argv[i]),1); }
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
typedef struct { int n; uint32_t d[36]; } fg176rs_ggf_big;

/* a = v << sh, for v > 0. */
static void fg176rs_ggf_big_set(fg176rs_ggf_big *a, uint64_t v, int sh) {
    int w = sh >> 5, b = sh & 31, i;
    uint64_t lo = v << b, hi = b ? v >> (64 - b) : 0;
    for (i = 0; i < w; i++) a->d[i] = 0;
    a->d[w] = (uint32_t)lo;
    a->d[w + 1] = (uint32_t)(lo >> 32);
    a->d[w + 2] = (uint32_t)hi;
    a->n = w + 3;
    while (!a->d[a->n - 1]) a->n--;
}

static void fg176rs_ggf_big_mul(fg176rs_ggf_big *a, uint32_t m) {
    uint64_t c = 0;
    for (int i = 0; i < a->n; i++) {
        c += (uint64_t)a->d[i] * m;
        a->d[i] = (uint32_t)c;
        c >>= 32;
    }
    if (c) a->d[a->n++] = (uint32_t)c;
}

static void fg176rs_ggf_big_mul_pow10(fg176rs_ggf_big *a, int k) {
    for (; k >= 9; k -= 9) fg176rs_ggf_big_mul(a, 1000000000u);
    for (; k > 0; k--) fg176rs_ggf_big_mul(a, 10);
}

static int fg176rs_ggf_big_cmp(const fg176rs_ggf_big *a, const fg176rs_ggf_big *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

/* t = a + b */
static void fg176rs_ggf_big_add(fg176rs_ggf_big *t, const fg176rs_ggf_big *a, const fg176rs_ggf_big *b) {
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
static void fg176rs_ggf_big_sub(fg176rs_ggf_big *a, const fg176rs_ggf_big *b) {
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
static int fg176rs_ggf_float_digits(uint64_t f, int e, int bits, int emin, char *dig, int *k) {
    fg176rs_ggf_big r, s, mp, mlo, t;
    /* Reading rounds a tie to the even significand, so the interval that
       reads back as f * 2^e includes its ends when f is even. */
    int even = !(f & 1);
    /* Below a power of two, the gap to the next float down is half the gap up. */
    int asym = f == (uint64_t)1 << (bits - 1) && e > emin;
    int up = e > 0 ? e : 0, lg = e - 1, n = 0, c;
    /* The value is r / s, and what reads back as it reaches from (r - *mm) / s
       to (r + mp) / s: half the gap to each neighbour. */
    fg176rs_ggf_big *mm = asym ? &mlo : &mp;
    fg176rs_ggf_big_set(&r, f, up + 1 + asym);
    fg176rs_ggf_big_set(&s, 1, up - e + 1 + asym);
    fg176rs_ggf_big_set(&mp, 1, up + asym);
    fg176rs_ggf_big_set(&mlo, 1, up);
    /* Scale to 10^*k, the least power of ten above the interval: with
       lg = floor(log2 value), ceil(lg log10 2) is it or one short, and
       78913 / 2^18 is log10 2 closely enough to compute that for |lg| < 1650. */
    for (uint64_t g = f; g; g >>= 1) lg++;
    *k = lg > 0 ? ((lg * 78913) >> 18) + 1 : -((-lg * 78913) >> 18);
    if (*k >= 0) fg176rs_ggf_big_mul_pow10(&s, *k);
    else {
        fg176rs_ggf_big_mul_pow10(&r, -*k);
        fg176rs_ggf_big_mul_pow10(&mp, -*k);
        if (asym) fg176rs_ggf_big_mul_pow10(&mlo, -*k);
    }
    fg176rs_ggf_big_add(&t, &r, &mp);
    c = fg176rs_ggf_big_cmp(&t, &s);
    if (even ? c >= 0 : c > 0) {
        fg176rs_ggf_big_mul(&s, 10);
        ++*k;
    }
    /* Each digit d leaves the remainder r: stop where the digits so far
       followed by d (r within *mm) or by d + 1 (r + mp past s) read back. */
    for (;;) {
        int d = 0, low, high;
        fg176rs_ggf_big_mul(&r, 10);
        fg176rs_ggf_big_mul(&mp, 10);
        if (asym) fg176rs_ggf_big_mul(&mlo, 10);
        while (fg176rs_ggf_big_cmp(&r, &s) >= 0) {
            fg176rs_ggf_big_sub(&r, &s);
            d++;
        }
        c = fg176rs_ggf_big_cmp(&r, mm);
        low = even ? c <= 0 : c < 0;
        fg176rs_ggf_big_add(&t, &r, &mp);
        c = fg176rs_ggf_big_cmp(&t, &s);
        high = even ? c >= 0 : c > 0;
        if (!low && !high) {
            dig[n++] = (char)('0' + d);
            continue;
        }
        if (low && high) {
            /* Both read back: the nearer, d + 1 when r is past half of s,
               and of two as near, the even one. */
            fg176rs_ggf_big_add(&t, &r, &r);
            c = fg176rs_ggf_big_cmp(&t, &s);
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
static int64_t fg176rs_ggf_fmt_float(uint8_t *dst, int neg, uint64_t frac, int bexp, int bits,
                            int emin) {
    char dig[20], *p = (char *)dst;
    int k, n, x, i;
    if (neg) *p++ = '-';
    if (!frac && !bexp) {
        memcpy(p, "0.0", 3);
        return p + 3 - (char *)dst;
    }
    n = fg176rs_ggf_float_digits(bexp ? frac | (uint64_t)1 << (bits - 1) : frac,
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

static int64_t fg176rs_ggf_fmt_f64(uint8_t *dst, double v) {
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
    return fg176rs_ggf_fmt_float(dst, (int)(b >> 63), b & (((uint64_t)1 << 52) - 1),
                        (int)(b >> 52) & 0x7FF, 53, -1074);
}

/* The digits are the f32's own, laid out as an f64's are, so 0.1 as an f32
   prints as 0.1, not as the digits of the f64 it widens to. */
static int64_t fg176rs_ggf_fmt_f32(uint8_t *dst, float v) {
    double d = v;
    uint32_t b;
    if (d != d || isinf(d)) return fg176rs_ggf_fmt_f64(dst, d);
    memcpy(&b, &v, sizeof b);
    return fg176rs_ggf_fmt_float(dst, (int)(b >> 31), b & 0x7FFFFF, (int)(b >> 23) & 0xFF, 24, -149);
}


static void fg176rs_gg_quote(fg176rs_gg_reference out,fg176rs_gg_slice text) { int64_t i; unsigned char c; fg176rs_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg176rs_gg_append(out,"\\",1,1);fg176rs_gg_append(out,&c,1,1);} else if(c=='\n')fg176rs_gg_append(out,"\\n",2,1);else if(c=='\r')fg176rs_gg_append(out,"\\r",2,1);else if(c=='\t')fg176rs_gg_append(out,"\\t",2,1);else fg176rs_gg_append(out,&c,1,1);}fg176rs_gg_append(out,"\"",1,1);}
static uint8_t fg176rs_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg176rs_gg_equal_elements_1(fg176rs_gg_slice a, fg176rs_gg_slice b);
static void fg176rs_gg_render_1(fg176rs_gg_reference out, int64_t value);
static uint8_t fg176rs_gg_eq_2(uint8_t a, uint8_t b);
static uint8_t fg176rs_gg_equal_elements_2(fg176rs_gg_slice a, fg176rs_gg_slice b);
static void fg176rs_gg_render_2(fg176rs_gg_reference out, uint8_t value);
static uint8_t fg176rs_gg_eq_4(uint64_t a, uint64_t b);
static uint8_t fg176rs_gg_equal_elements_4(fg176rs_gg_slice a, fg176rs_gg_slice b);
static void fg176rs_gg_render_4(fg176rs_gg_reference out, uint64_t value);
static uint8_t fg176rs_gg_eq_5(uint8_t a, uint8_t b);
static uint8_t fg176rs_gg_equal_elements_5(fg176rs_gg_slice a, fg176rs_gg_slice b);
static void fg176rs_gg_render_5(fg176rs_gg_reference out, uint8_t value);
static uint8_t fg176rs_gg_equal_elements_1(fg176rs_gg_slice a,fg176rs_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg176rs_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg176rs_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg176rs_gg_render_1(fg176rs_gg_reference out, int64_t value) {
fg176rs_gg_integer(out,value);
}
static uint8_t fg176rs_gg_equal_elements_2(fg176rs_gg_slice a,fg176rs_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg176rs_gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg176rs_gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void fg176rs_gg_render_2(fg176rs_gg_reference out, uint8_t value) {
fg176rs_gg_bool(out,value);
}
static uint8_t fg176rs_gg_equal_elements_4(fg176rs_gg_slice a,fg176rs_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg176rs_gg_eq_4(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg176rs_gg_eq_4(uint64_t a, uint64_t b) {
return a == b;
}
static void fg176rs_gg_render_4(fg176rs_gg_reference out, uint64_t value) {
fg176rs_gg_unsigned(out,value);
}
static uint8_t fg176rs_gg_equal_elements_5(fg176rs_gg_slice a,fg176rs_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg176rs_gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg176rs_gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void fg176rs_gg_render_5(fg176rs_gg_reference out, uint8_t value) {
fg176rs_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg176rs_gg_globals;
static void fg176rs_gg_function_0(int64_t base);
static uint8_t fg176rs_gg_function_1(int64_t p0, int64_t base);
static void fg176rs_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static uint64_t fg176rs_gg_function_8(int64_t p0, int64_t p1, int64_t base);
static uint64_t fg176rs_gg_function_9(int64_t p0, int64_t base);
static uint64_t fg176rs_gg_function_10(int64_t p0, int64_t p1, int64_t base);
static uint64_t fg176rs_gg_function_11(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t fg176rs_gg_function_12(int64_t base);
static uint64_t fg176rs_gg_function_13(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t fg176rs_gg_function_14(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t fg176rs_gg_function_15(uint64_t p0, uint64_t p1, int64_t base);
static void fg176rs_gg_function_16(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, int64_t base);
static uint8_t fg176rs_gg_function_17(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base);
static void fg176rs_gg_function_20(int64_t p0, int64_t p1, uint64_t p2, int64_t base);
static void fg176rs_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  fg175s_fail(t2);
  }
  }
  }
fg176rs_gg_done:;
}
static void fg176rs_gg_function_20(int64_t p0, int64_t p1, uint64_t p2, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint8_t t2;
  uint8_t t3;
  uint64_t t4;
  uint64_t t5;
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
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  int64_t v0;
  int64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((uint64_t)(v2));
  t1 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((uint64_t)(v2));
  t5 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t3 = ((t4) <= (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(6))));
  fg176rs_gg_function_3(t6, t7, base + 0);
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
  t28 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t26, t27))));
  t29 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v4 = ((uint64_t)(((uint64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t30 = ((uint64_t)(v4));
  t31 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t32 = ((t30) <= (t31));
  t33 = ((int64_t)(((int64_t)UINT64_C(7))));
  fg176rs_gg_function_3(t32, t33, base + 0);
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(8))));
  t36 = ((int64_t)(v1));
  t37 = ((int64_t)(((int64_t)UINT64_C(3))));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((uint64_t)(v4));
  fg175s_put(t34, t35, t38, t39);
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(8))));
  t42 = ((int64_t)(v1));
  t43 = ((int64_t)(((int64_t)UINT64_C(3))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((uint64_t)(v3));
  fg175s_put(t40, t41, t46, t47);
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(8))));
  t50 = ((int64_t)(v1));
  t51 = ((int64_t)(((int64_t)UINT64_C(3))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(((int64_t)UINT64_C(2))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((uint64_t)(v2));
  fg175s_put(t48, t49, t54, t55);
  }
fg176rs_gg_done:;
}
static uint64_t fg176rs_gg_function_9(int64_t p0, int64_t base) {
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
  t4 = ((int64_t)(((int64_t)UINT64_C(4))));
  fg176rs_gg_function_3(t3, t4, base + 0);
  t0 = ((uint64_t)(((uint64_t)(v0))));
  goto fg176rs_gg_done;
  }
fg176rs_gg_done:;
  return t0;
}
static uint64_t fg176rs_gg_function_12(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto fg176rs_gg_done;
  }
fg176rs_gg_done:;
  return t0;
}
static uint64_t fg176rs_gg_function_15(uint64_t p0, uint64_t p1, int64_t base) {
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
  t2 = fg176rs_gg_function_12(base + 0);
  t3 = ((uint64_t)(t2));
  t4 = ((uint64_t)(v1));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t6 = ((t1) <= (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg176rs_gg_function_3(t6, t7, base + 0);
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  goto fg176rs_gg_done;
  }
fg176rs_gg_done:;
  return t0;
}
static uint8_t fg176rs_gg_function_17(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base) {
  uint8_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint8_t t6;
  uint8_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
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
  t4 = fg176rs_gg_function_15(t2, t3, base + 0);
  t5 = ((uint64_t)(t4));
  t6 = ((t1) < (t5));
  t7 = t6;
  if (t7) {
  t8 = ((uint64_t)(v2));
  t9 = ((uint64_t)(v0));
  t10 = ((uint64_t)(v1));
  t11 = fg176rs_gg_function_15(t9, t10, base + 0);
  t12 = ((uint64_t)(t11));
  t7 = ((t8) < (t12));
  }
  t0 = t7;
  goto fg176rs_gg_done;
  }
fg176rs_gg_done:;
  return t0;
}
static uint64_t fg176rs_gg_function_11(uint64_t p0, uint64_t p1, int64_t base) {
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
  t6 = fg176rs_gg_function_12(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg176rs_gg_function_3(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto fg176rs_gg_done;
  }
fg176rs_gg_done:;
  return t0;
}
static void fg176rs_gg_function_16(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint8_t t2;
  uint64_t t3;
  uint64_t t4;
  uint8_t t5;
  uint64_t t6;
  uint64_t t7;
  uint8_t t8;
  uint64_t t9;
  uint64_t t10;
  uint8_t t11;
  uint64_t t12;
  uint64_t t13;
  uint8_t t14;
  int64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint8_t t24;
  uint64_t t25;
  uint64_t t26;
  uint8_t t27;
  int64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint8_t t37;
  uint64_t t38;
  uint64_t t39;
  uint8_t t40;
  int64_t t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
  uint64_t t46;
  uint64_t t47;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint64_t v5;
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
  uint64_t v17;
  uint64_t v18;
  uint64_t v19;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  v7 = ((uint64_t)(v0));
  v8 = ((uint64_t)(v2));
  v9 = ((uint64_t)(v4));
  v10 = ((uint64_t)(v1));
  v11 = ((uint64_t)(v3));
  v12 = ((uint64_t)(v5));
  t0 = ((uint64_t)(v8));
  t1 = ((uint64_t)(v7));
  t2 = ((t0) < (t1));
  if (t2) {
  {
  v13 = ((uint64_t)(v7));
  v7 = ((uint64_t)(v8));
  v8 = ((uint64_t)(v13));
  v14 = ((uint64_t)(v10));
  v10 = ((uint64_t)(v11));
  v11 = ((uint64_t)(v14));
  }
  }
  t3 = ((uint64_t)(v9));
  t4 = ((uint64_t)(v8));
  t5 = ((t3) < (t4));
  if (t5) {
  {
  v15 = ((uint64_t)(v8));
  v8 = ((uint64_t)(v9));
  v9 = ((uint64_t)(v15));
  v16 = ((uint64_t)(v11));
  v11 = ((uint64_t)(v12));
  v12 = ((uint64_t)(v16));
  }
  }
  t6 = ((uint64_t)(v8));
  t7 = ((uint64_t)(v7));
  t8 = ((t6) < (t7));
  if (t8) {
  {
  v17 = ((uint64_t)(v7));
  v7 = ((uint64_t)(v8));
  v8 = ((uint64_t)(v17));
  v18 = ((uint64_t)(v10));
  v10 = ((uint64_t)(v11));
  v11 = ((uint64_t)(v18));
  }
  }
  v19 = ((uint64_t)(v6));
  t9 = ((uint64_t)(v10));
  t10 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t11 = ((t9) > (t10));
  if (t11) {
  {
  t12 = ((uint64_t)(v7));
  t13 = ((uint64_t)(v19));
  t14 = ((t12) >= (t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(55))));
  fg176rs_gg_function_3(t14, t15, base + 0);
  t16 = ((uint64_t)(v19));
  t17 = ((uint64_t)(v10));
  t18 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t19 = ((uint64_t)(((uint64_t)((uint64_t)(t17) - (uint64_t)(t18)))));
  t20 = ((uint64_t)(v7));
  t21 = fg176rs_gg_function_11(t19, t20, base + 0);
  v19 = fg176rs_gg_function_15(t16, t21, base + 0);
  }
  }
  t22 = ((uint64_t)(v11));
  t23 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t24 = ((t22) > (t23));
  if (t24) {
  {
  t25 = ((uint64_t)(v8));
  t26 = ((uint64_t)(v19));
  t27 = ((t25) >= (t26));
  t28 = ((int64_t)(((int64_t)UINT64_C(55))));
  fg176rs_gg_function_3(t27, t28, base + 0);
  t29 = ((uint64_t)(v19));
  t30 = ((uint64_t)(v11));
  t31 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t32 = ((uint64_t)(((uint64_t)((uint64_t)(t30) - (uint64_t)(t31)))));
  t33 = ((uint64_t)(v8));
  t34 = fg176rs_gg_function_11(t32, t33, base + 0);
  v19 = fg176rs_gg_function_15(t29, t34, base + 0);
  }
  }
  t35 = ((uint64_t)(v12));
  t36 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t37 = ((t35) > (t36));
  if (t37) {
  {
  t38 = ((uint64_t)(v9));
  t39 = ((uint64_t)(v19));
  t40 = ((t38) >= (t39));
  t41 = ((int64_t)(((int64_t)UINT64_C(55))));
  fg176rs_gg_function_3(t40, t41, base + 0);
  t42 = ((uint64_t)(v19));
  t43 = ((uint64_t)(v12));
  t44 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t45 = ((uint64_t)(((uint64_t)((uint64_t)(t43) - (uint64_t)(t44)))));
  t46 = ((uint64_t)(v9));
  t47 = fg176rs_gg_function_11(t45, t46, base + 0);
  v19 = fg176rs_gg_function_15(t42, t47, base + 0);
  }
  }
  }
fg176rs_gg_done:;
}
static uint64_t fg176rs_gg_function_14(uint64_t p0, uint64_t p1, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint8_t t3;
  int64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(3))));
  fg176rs_gg_function_3(t3, t4, base + 0);
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t5) - (uint64_t)(t6)))));
  goto fg176rs_gg_done;
  }
fg176rs_gg_done:;
  return t0;
}
static uint64_t fg176rs_gg_function_13(uint64_t p0, uint64_t p1, int64_t base) {
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
  t4 = ((int64_t)(((int64_t)UINT64_C(5))));
  fg176rs_gg_function_3(t3, t4, base + 0);
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(v1));
  t7 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t5, t6))));
  t9 = ((uint64_t)(v0));
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t9, t10))));
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
  goto fg176rs_gg_done;
  }
fg176rs_gg_done:;
  return t0;
}
static uint64_t fg176rs_gg_function_10(int64_t p0, int64_t p1, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = ((int64_t)(v1));
  t4 = fg175s_ti(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t0 = fg176rs_gg_function_9(t5, base + 0);
  goto fg176rs_gg_done;
  }
fg176rs_gg_done:;
  return t0;
}
static uint64_t fg176rs_gg_function_8(int64_t p0, int64_t p1, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = ((int64_t)(v1));
  t4 = fg175s_ti(t1, t2, t3);
  t5 = ((int64_t)(t4));
  t0 = fg176rs_gg_function_9(t5, base + 0);
  goto fg176rs_gg_done;
  }
fg176rs_gg_done:;
  return t0;
}
static uint8_t fg176rs_gg_function_1(int64_t p0, int64_t base) {
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
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  uint8_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  int64_t t66;
  int64_t t67;
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
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint64_t t95;
  uint64_t t96;
  uint8_t t97;
  uint8_t t98;
  uint64_t t99;
  uint64_t t100;
  uint8_t t101;
  uint8_t t102;
  uint64_t t103;
  uint64_t t104;
  uint8_t t105;
  uint8_t t106;
  uint64_t t107;
  uint64_t t108;
  uint8_t t109;
  uint64_t t110;
  uint64_t t111;
  uint8_t t112;
  uint8_t t113;
  uint64_t t114;
  uint64_t t115;
  uint8_t t116;
  uint8_t t117;
  uint64_t t118;
  uint64_t t119;
  uint8_t t120;
  uint8_t t121;
  uint64_t t122;
  uint64_t t123;
  uint8_t t124;
  int64_t t125;
  uint64_t t126;
  uint64_t t127;
  uint64_t t128;
  uint64_t t129;
  uint8_t t130;
  uint8_t t131;
  uint64_t t132;
  uint64_t t133;
  uint64_t t134;
  uint64_t t135;
  uint8_t t136;
  int64_t t137;
  uint64_t t138;
  uint64_t t139;
  uint8_t t140;
  uint8_t t141;
  uint64_t t142;
  uint64_t t143;
  uint8_t t144;
  uint8_t t145;
  uint64_t t146;
  uint64_t t147;
  uint8_t t148;
  uint8_t t149;
  uint64_t t150;
  uint64_t t151;
  uint8_t t152;
  int64_t t153;
  uint64_t t154;
  uint64_t t155;
  uint8_t t156;
  int64_t t157;
  uint64_t t158;
  uint64_t t159;
  uint8_t t160;
  int64_t t161;
  uint64_t t162;
  uint64_t t163;
  uint8_t t164;
  uint8_t t165;
  uint64_t t166;
  uint64_t t167;
  uint8_t t168;
  int64_t t169;
  uint8_t t170;
  uint64_t t171;
  uint64_t t172;
  uint64_t t173;
  uint64_t t174;
  uint8_t t175;
  int64_t t176;
  uint64_t t177;
  uint64_t t178;
  uint64_t t179;
  uint64_t t180;
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
  uint8_t t211;
  uint8_t t212;
  uint8_t t213;
  uint64_t t214;
  uint64_t t215;
  uint8_t t216;
  int64_t t217;
  uint64_t t218;
  uint64_t t219;
  uint64_t t220;
  uint64_t t221;
  uint8_t t222;
  uint8_t t223;
  uint64_t t224;
  uint64_t t225;
  uint64_t t226;
  uint64_t t227;
  uint8_t t228;
  uint8_t t229;
  uint64_t t230;
  uint64_t t231;
  uint64_t t232;
  uint64_t t233;
  uint8_t t234;
  int64_t t235;
  uint64_t t236;
  uint64_t t237;
  uint8_t t238;
  int64_t t239;
  uint64_t t240;
  uint64_t t241;
  uint64_t t242;
  uint64_t t243;
  uint8_t t244;
  uint8_t t245;
  uint64_t t246;
  uint64_t t247;
  uint64_t t248;
  uint64_t t249;
  uint8_t t250;
  uint8_t t251;
  uint64_t t252;
  uint64_t t253;
  uint64_t t254;
  uint64_t t255;
  uint8_t t256;
  int64_t t257;
  uint64_t t258;
  uint64_t t259;
  uint8_t t260;
  int64_t t261;
  uint64_t t262;
  uint64_t t263;
  uint64_t t264;
  uint64_t t265;
  uint8_t t266;
  uint8_t t267;
  uint64_t t268;
  uint64_t t269;
  uint64_t t270;
  uint64_t t271;
  uint8_t t272;
  uint8_t t273;
  uint64_t t274;
  uint64_t t275;
  uint64_t t276;
  uint64_t t277;
  uint8_t t278;
  int64_t t279;
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
  int64_t t290;
  int64_t t291;
  uint64_t t292;
  uint64_t t293;
  uint8_t t294;
  uint8_t t295;
  uint64_t t296;
  uint64_t t297;
  uint8_t t298;
  uint8_t t299;
  uint64_t t300;
  uint64_t t301;
  uint8_t t302;
  int64_t t303;
  uint64_t t304;
  uint64_t t305;
  uint64_t t306;
  uint64_t t307;
  uint8_t t308;
  uint8_t t309;
  uint64_t t310;
  uint64_t t311;
  uint64_t t312;
  uint64_t t313;
  uint8_t t314;
  uint8_t t315;
  uint64_t t316;
  uint64_t t317;
  uint8_t t318;
  uint64_t t319;
  uint64_t t320;
  uint64_t t321;
  uint8_t t322;
  int64_t t323;
  uint64_t t324;
  uint64_t t325;
  uint64_t t326;
  uint64_t t327;
  uint64_t t328;
  uint64_t t329;
  uint64_t t330;
  uint64_t t331;
  uint64_t t332;
  uint64_t t333;
  uint64_t t334;
  uint64_t t335;
  uint8_t t336;
  uint64_t t337;
  uint64_t t338;
  uint8_t t339;
  uint64_t t340;
  uint64_t t341;
  uint8_t t342;
  uint64_t t343;
  uint64_t t344;
  uint64_t t345;
  uint64_t t346;
  uint64_t t347;
  uint64_t t348;
  uint64_t t349;
  uint64_t t350;
  uint8_t t351;
  uint64_t t352;
  uint64_t t353;
  uint64_t t354;
  uint64_t t355;
  uint8_t t356;
  int64_t t357;
  uint8_t t358;
  uint64_t t359;
  uint64_t t360;
  uint8_t t361;
  int64_t t362;
  uint64_t t363;
  uint64_t t364;
  uint8_t t365;
  int64_t t366;
  uint64_t t367;
  uint64_t t368;
  uint8_t t369;
  uint8_t t370;
  uint64_t t371;
  uint64_t t372;
  uint8_t t373;
  uint8_t t374;
  uint64_t t375;
  uint64_t t376;
  uint8_t t377;
  uint8_t t378;
  uint64_t t379;
  uint64_t t380;
  uint8_t t381;
  uint8_t t382;
  uint64_t t383;
  uint64_t t384;
  uint8_t t385;
  int64_t t386;
  uint64_t t387;
  uint64_t t388;
  uint64_t t389;
  uint64_t t390;
  uint8_t t391;
  int64_t t392;
  uint64_t t393;
  uint64_t t394;
  uint64_t t395;
  uint64_t t396;
  uint64_t t397;
  uint64_t t398;
  uint64_t t399;
  uint64_t t400;
  uint64_t t401;
  uint64_t t402;
  uint64_t t403;
  uint64_t t404;
  uint64_t t405;
  uint64_t t406;
  uint64_t t407;
  uint64_t t408;
  uint64_t t409;
  uint64_t t410;
  uint64_t t411;
  uint64_t t412;
  uint64_t t413;
  uint64_t t414;
  uint64_t t415;
  uint64_t t416;
  uint64_t t417;
  uint64_t t418;
  uint64_t t419;
  uint64_t t420;
  uint64_t t421;
  uint64_t t422;
  uint64_t t423;
  uint64_t t424;
  uint64_t t425;
  uint64_t t426;
  uint64_t t427;
  uint64_t t428;
  uint64_t t429;
  uint64_t t430;
  uint64_t t431;
  uint64_t t432;
  uint8_t t433;
  uint64_t t434;
  uint64_t t435;
  uint64_t t436;
  uint64_t t437;
  uint64_t t438;
  uint64_t t439;
  uint64_t t440;
  uint64_t t441;
  uint64_t t442;
  uint64_t t443;
  uint64_t t444;
  uint64_t t445;
  uint64_t t446;
  uint64_t t447;
  uint64_t t448;
  uint64_t t449;
  uint64_t t450;
  uint64_t t451;
  uint64_t t452;
  uint64_t t453;
  uint64_t t454;
  uint64_t t455;
  uint64_t t456;
  uint64_t t457;
  uint64_t t458;
  uint64_t t459;
  uint64_t t460;
  uint64_t t461;
  uint64_t t462;
  uint64_t t463;
  uint64_t t464;
  uint64_t t465;
  uint64_t t466;
  uint8_t t467;
  uint8_t t468;
  uint64_t t469;
  uint64_t t470;
  uint8_t t471;
  uint8_t t472;
  uint64_t t473;
  uint64_t t474;
  uint8_t t475;
  int64_t t476;
  uint64_t t477;
  uint64_t t478;
  uint64_t t479;
  uint64_t t480;
  uint64_t t481;
  uint64_t t482;
  uint64_t t483;
  uint64_t t484;
  uint64_t t485;
  uint64_t t486;
  uint64_t t487;
  uint64_t t488;
  uint64_t t489;
  uint8_t t490;
  uint8_t t491;
  int64_t t492;
  uint64_t t493;
  uint64_t t494;
  uint64_t t495;
  uint64_t t496;
  uint8_t t497;
  uint8_t t498;
  int64_t t499;
  int64_t t500;
  int64_t t501;
  int64_t t502;
  int64_t t503;
  int64_t t504;
  int64_t t505;
  int64_t t506;
  int64_t t507;
  int64_t t508;
  uint64_t t509;
  uint64_t t510;
  uint8_t t511;
  uint8_t t512;
  uint64_t t513;
  uint64_t t514;
  uint8_t t515;
  int64_t t516;
  int64_t t517;
  int64_t t518;
  int64_t t519;
  int64_t t520;
  int64_t t521;
  uint64_t t522;
  int64_t t523;
  int64_t t524;
  int64_t t525;
  uint64_t t526;
  int64_t t527;
  int64_t t528;
  int64_t t529;
  uint64_t t530;
  int64_t t531;
  int64_t t532;
  int64_t t533;
  uint64_t t534;
  int64_t t535;
  int64_t t536;
  int64_t t537;
  uint64_t t538;
  int64_t t539;
  int64_t t540;
  int64_t t541;
  uint64_t t542;
  int64_t t543;
  int64_t t544;
  int64_t t545;
  uint64_t t546;
  int64_t t547;
  int64_t t548;
  int64_t t549;
  uint64_t t550;
  int64_t t551;
  int64_t t552;
  int64_t t553;
  uint64_t t554;
  int64_t t555;
  int64_t t556;
  int64_t t557;
  uint64_t t558;
  int64_t t559;
  int64_t t560;
  int64_t t561;
  uint64_t t562;
  int64_t t563;
  int64_t t564;
  int64_t t565;
  uint64_t t566;
  int64_t t567;
  int64_t t568;
  int64_t t569;
  uint64_t t570;
  int64_t t571;
  int64_t t572;
  int64_t t573;
  uint64_t t574;
  int64_t t575;
  int64_t t576;
  int64_t t577;
  uint64_t t578;
  int64_t t579;
  int64_t t580;
  int64_t t581;
  uint64_t t582;
  int64_t t583;
  int64_t t584;
  int64_t t585;
  uint64_t t586;
  int64_t t587;
  int64_t t588;
  int64_t t589;
  uint64_t t590;
  int64_t t591;
  int64_t t592;
  int64_t t593;
  uint64_t t594;
  int64_t t595;
  int64_t t596;
  int64_t t597;
  uint64_t t598;
  int64_t t599;
  int64_t t600;
  int64_t t601;
  uint64_t t602;
  int64_t t603;
  int64_t t604;
  int64_t t605;
  uint64_t t606;
  int64_t t607;
  int64_t t608;
  int64_t t609;
  uint64_t t610;
  int64_t t611;
  int64_t t612;
  int64_t t613;
  uint64_t t614;
  int64_t t615;
  int64_t t616;
  int64_t t617;
  uint64_t t618;
  int64_t t619;
  int64_t t620;
  int64_t t621;
  uint64_t t622;
  int64_t t623;
  int64_t t624;
  int64_t t625;
  uint64_t t626;
  int64_t t627;
  int64_t t628;
  int64_t t629;
  uint64_t t630;
  int64_t t631;
  int64_t t632;
  int64_t t633;
  uint64_t t634;
  int64_t t635;
  int64_t t636;
  int64_t t637;
  uint64_t t638;
  int64_t t639;
  int64_t t640;
  int64_t t641;
  uint64_t t642;
  int64_t t643;
  int64_t t644;
  int64_t t645;
  uint64_t t646;
  int64_t t647;
  int64_t t648;
  int64_t t649;
  uint64_t t650;
  int64_t t651;
  int64_t t652;
  int64_t t653;
  uint64_t t654;
  int64_t t655;
  int64_t t656;
  int64_t t657;
  uint64_t t658;
  int64_t t659;
  int64_t t660;
  int64_t t661;
  uint64_t t662;
  uint8_t t663;
  int64_t t664;
  int64_t t665;
  int64_t t666;
  int64_t t667;
  uint8_t t668;
  int64_t t669;
  int64_t t670;
  int64_t t671;
  uint64_t t672;
  int64_t t673;
  int64_t t674;
  int64_t t675;
  uint64_t t676;
  int64_t t677;
  int64_t t678;
  int64_t t679;
  uint64_t t680;
  uint64_t t681;
  uint64_t t682;
  uint64_t t683;
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
  int64_t t703;
  uint64_t t704;
  int64_t t705;
  int64_t t706;
  int64_t t707;
  uint64_t t708;
  int64_t t709;
  int64_t t710;
  int64_t t711;
  uint64_t t712;
  int64_t t713;
  int64_t t714;
  int64_t t715;
  uint64_t t716;
  int64_t t717;
  int64_t t718;
  uint64_t t719;
  int64_t t720;
  int64_t t721;
  uint64_t t722;
  int64_t t723;
  int64_t t724;
  uint64_t t725;
  int64_t t726;
  int64_t t727;
  uint64_t t728;
  int64_t t729;
  int64_t t730;
  uint64_t t731;
  int64_t t732;
  int64_t t733;
  int64_t t734;
  uint64_t t735;
  int64_t t736;
  int64_t t737;
  int64_t t738;
  uint64_t t739;
  int64_t t740;
  int64_t t741;
  int64_t t742;
  uint64_t t743;
  int64_t t744;
  int64_t t745;
  int64_t t746;
  uint64_t t747;
  int64_t t748;
  int64_t t749;
  int64_t t750;
  uint64_t t751;
  int64_t t752;
  int64_t t753;
  int64_t t754;
  int64_t t755;
  uint8_t t756;
  int64_t t757;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  uint8_t v7;
  uint64_t v8;
  uint64_t v9;
  uint64_t v10;
  uint64_t v11;
  uint64_t v12;
  uint64_t v13;
  uint64_t v14;
  uint64_t v15;
  uint64_t v16;
  uint64_t v17;
  uint64_t v18;
  uint64_t v19;
  uint64_t v20;
  uint64_t v21;
  uint64_t v22;
  uint64_t v23;
  uint64_t v24;
  uint64_t v25;
  uint64_t v26;
  uint64_t v27;
  uint64_t v28;
  uint64_t v29;
  uint64_t v30;
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
  uint64_t v43;
  uint64_t v44;
  uint64_t v45;
  uint64_t v46;
  uint64_t v47;
  uint64_t v48;
  uint64_t v49;
  uint64_t v50;
  uint64_t v51;
  uint64_t v52;
  uint64_t v53;
  uint64_t v54;
  uint64_t v55;
  int64_t v56;
  uint64_t v57;
  uint64_t v58;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg175s_arg(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg176rs_gg_function_3(t6, t7, base + 0);
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(5))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg175s_ti(t8, t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg175s_oc(t13);
  t15 = ((int64_t)(t14));
  t16 = ((t12) != (t15));
  if (t16) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg176rs_gg_done;
  }
  }
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg175s_ti(t17, t18, t19);
  v2 = ((int64_t)(t20));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = fg175s_ti(t21, t22, t23);
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
  t32 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg176rs_gg_function_3(t31, t32, base + 0);
  t33 = ((int64_t)(v2));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg175s_ti(t33, t34, t35);
  v4 = ((int64_t)(t36));
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(((int64_t)UINT64_C(2))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = fg175s_ti(t37, t38, t39);
  v5 = ((int64_t)(t40));
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(((int64_t)UINT64_C(2))));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = fg175s_ti(t41, t42, t43);
  v6 = ((int64_t)(t44));
  t45 = ((int64_t)(v4));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t47 = fg175s_tc(t46);
  t48 = ((int64_t)(t47));
  t49 = ((t45) != (t48));
  if (t49) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg176rs_gg_done;
  }
  }
  t50 = ((int64_t)(v5));
  t51 = ((int64_t)(((int64_t)UINT64_C(8))));
  t52 = fg175s_tc(t51);
  t53 = ((int64_t)(t52));
  t54 = ((t50) != (t53));
  t55 = t54;
  if (t55) {
  t56 = ((int64_t)(v5));
  t57 = ((int64_t)(((int64_t)UINT64_C(9))));
  t58 = fg175s_tc(t57);
  t59 = ((int64_t)(t58));
  t55 = ((t56) != (t59));
  }
  t60 = t55;
  if (t60) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg176rs_gg_done;
  }
  }
  v7 = ((uint8_t)UINT64_C(0));
  t61 = ((int64_t)(v6));
  t62 = ((int64_t)(((int64_t)UINT64_C(3))));
  t63 = fg175s_tc(t62);
  t64 = ((int64_t)(t63));
  t65 = ((t61) == (t64));
  if (t65) {
  {
  v7 = ((uint8_t)UINT64_C(1));
  }
  } else {
  {
  t66 = ((int64_t)(v6));
  t67 = ((int64_t)(((int64_t)UINT64_C(1))));
  t68 = fg175s_tc(t67);
  t69 = ((int64_t)(t68));
  t70 = ((t66) != (t69));
  if (t70) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg176rs_gg_done;
  }
  }
  }
  }
  t71 = ((int64_t)(v2));
  t72 = ((int64_t)(((int64_t)UINT64_C(0))));
  v8 = fg176rs_gg_function_8(t71, t72, base + 0);
  t73 = ((int64_t)(v2));
  t74 = ((int64_t)(((int64_t)UINT64_C(1))));
  v9 = fg176rs_gg_function_8(t73, t74, base + 0);
  t75 = ((int64_t)(v2));
  t76 = ((int64_t)(((int64_t)UINT64_C(2))));
  v10 = fg176rs_gg_function_8(t75, t76, base + 0);
  t77 = ((int64_t)(v2));
  t78 = ((int64_t)(((int64_t)UINT64_C(3))));
  v11 = fg176rs_gg_function_8(t77, t78, base + 0);
  t79 = ((int64_t)(v3));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  v12 = fg176rs_gg_function_8(t79, t80, base + 0);
  t81 = ((int64_t)(v3));
  t82 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = fg176rs_gg_function_8(t81, t82, base + 0);
  t83 = ((int64_t)(v3));
  t84 = ((int64_t)(((int64_t)UINT64_C(2))));
  v14 = fg176rs_gg_function_8(t83, t84, base + 0);
  t85 = ((int64_t)(v3));
  t86 = ((int64_t)(((int64_t)UINT64_C(3))));
  v15 = fg176rs_gg_function_8(t85, t86, base + 0);
  t87 = ((int64_t)(v1));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  v16 = fg176rs_gg_function_8(t87, t88, base + 0);
  t89 = ((int64_t)(v1));
  t90 = ((int64_t)(((int64_t)UINT64_C(1))));
  v17 = fg176rs_gg_function_8(t89, t90, base + 0);
  t91 = ((int64_t)(v1));
  t92 = ((int64_t)(((int64_t)UINT64_C(2))));
  v18 = fg176rs_gg_function_8(t91, t92, base + 0);
  t93 = ((int64_t)(v1));
  t94 = ((int64_t)(((int64_t)UINT64_C(3))));
  v19 = fg176rs_gg_function_8(t93, t94, base + 0);
  t95 = ((uint64_t)(v8));
  t96 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t97 = ((t95) == (t96));
  t98 = t97;
  if (!t98) {
  t99 = ((uint64_t)(v9));
  t100 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t98 = ((t99) == (t100));
  }
  t101 = t98;
  t102 = t101;
  if (!t102) {
  t103 = ((uint64_t)(v10));
  t104 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t102 = ((t103) == (t104));
  }
  t105 = t102;
  t106 = t105;
  if (!t106) {
  t107 = ((uint64_t)(v11));
  t108 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t106 = ((t107) == (t108));
  }
  t109 = t106;
  if (t109) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg176rs_gg_done;
  }
  }
  t110 = ((uint64_t)(v12));
  t111 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t112 = ((t110) > (t111));
  t113 = t112;
  if (t113) {
  t114 = ((uint64_t)(v13));
  t115 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t113 = ((t114) > (t115));
  }
  t116 = t113;
  t117 = t116;
  if (t117) {
  t118 = ((uint64_t)(v14));
  t119 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t117 = ((t118) > (t119));
  }
  t120 = t117;
  t121 = t120;
  if (t121) {
  t122 = ((uint64_t)(v15));
  t123 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t121 = ((t122) > (t123));
  }
  t124 = t121;
  t125 = ((int64_t)(((int64_t)UINT64_C(19))));
  fg176rs_gg_function_3(t124, t125, base + 0);
  t126 = ((uint64_t)(v10));
  t127 = ((uint64_t)(v13));
  t128 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t126, t127))));
  t129 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t130 = ((t128) == (t129));
  t131 = t130;
  if (t131) {
  t132 = ((uint64_t)(v11));
  t133 = ((uint64_t)(v14));
  t134 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t132, t133))));
  t135 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t131 = ((t134) == (t135));
  }
  t136 = t131;
  t137 = ((int64_t)(((int64_t)UINT64_C(54))));
  fg176rs_gg_function_3(t136, t137, base + 0);
  t138 = ((uint64_t)(v16));
  t139 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t140 = ((t138) > (t139));
  t141 = t140;
  if (t141) {
  t142 = ((uint64_t)(v17));
  t143 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t141 = ((t142) > (t143));
  }
  t144 = t141;
  t145 = t144;
  if (t145) {
  t146 = ((uint64_t)(v18));
  t147 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t145 = ((t146) > (t147));
  }
  t148 = t145;
  t149 = t148;
  if (t149) {
  t150 = ((uint64_t)(v19));
  t151 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t149 = ((t150) > (t151));
  }
  t152 = t149;
  t153 = ((int64_t)(((int64_t)UINT64_C(18))));
  fg176rs_gg_function_3(t152, t153, base + 0);
  t154 = ((uint64_t)(v12));
  t155 = ((uint64_t)(v9));
  t156 = ((t154) >= (t155));
  t157 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg176rs_gg_function_3(t156, t157, base + 0);
  t158 = ((uint64_t)(v16));
  t159 = ((uint64_t)(v8));
  t160 = ((t158) >= (t159));
  t161 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg176rs_gg_function_3(t160, t161, base + 0);
  t162 = ((uint64_t)(v18));
  t163 = ((uint64_t)(v10));
  t164 = ((t162) >= (t163));
  t165 = t164;
  if (t165) {
  t166 = ((uint64_t)(v19));
  t167 = ((uint64_t)(v11));
  t165 = ((t166) >= (t167));
  }
  t168 = t165;
  t169 = ((int64_t)(((int64_t)UINT64_C(22))));
  fg176rs_gg_function_3(t168, t169, base + 0);
  t170 = v7;
  if (t170) {
  {
  t171 = ((uint64_t)(v8));
  t172 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t173 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t171, t172))));
  t174 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t175 = ((t173) == (t174));
  t176 = ((int64_t)(((int64_t)UINT64_C(23))));
  fg176rs_gg_function_3(t175, t176, base + 0);
  t177 = ((uint64_t)(v16));
  t178 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t179 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t177, t178))));
  t180 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t181 = ((t179) == (t180));
  t182 = ((int64_t)(((int64_t)UINT64_C(24))));
  fg176rs_gg_function_3(t181, t182, base + 0);
  }
  }
  t183 = ((int64_t)(v2));
  t184 = ((int64_t)(((int64_t)UINT64_C(0))));
  v20 = fg176rs_gg_function_10(t183, t184, base + 0);
  t185 = ((int64_t)(v2));
  t186 = ((int64_t)(((int64_t)UINT64_C(1))));
  v21 = fg176rs_gg_function_10(t185, t186, base + 0);
  t187 = ((int64_t)(v2));
  t188 = ((int64_t)(((int64_t)UINT64_C(2))));
  v22 = fg176rs_gg_function_10(t187, t188, base + 0);
  t189 = ((int64_t)(v2));
  t190 = ((int64_t)(((int64_t)UINT64_C(3))));
  v23 = fg176rs_gg_function_10(t189, t190, base + 0);
  t191 = ((int64_t)(v3));
  t192 = ((int64_t)(((int64_t)UINT64_C(0))));
  v24 = fg176rs_gg_function_10(t191, t192, base + 0);
  t193 = ((int64_t)(v3));
  t194 = ((int64_t)(((int64_t)UINT64_C(1))));
  v25 = fg176rs_gg_function_10(t193, t194, base + 0);
  t195 = ((int64_t)(v3));
  t196 = ((int64_t)(((int64_t)UINT64_C(2))));
  v26 = fg176rs_gg_function_10(t195, t196, base + 0);
  t197 = ((int64_t)(v3));
  t198 = ((int64_t)(((int64_t)UINT64_C(3))));
  v27 = fg176rs_gg_function_10(t197, t198, base + 0);
  t199 = ((int64_t)(v1));
  t200 = ((int64_t)(((int64_t)UINT64_C(0))));
  v28 = fg176rs_gg_function_10(t199, t200, base + 0);
  t201 = ((int64_t)(v1));
  t202 = ((int64_t)(((int64_t)UINT64_C(1))));
  v29 = fg176rs_gg_function_10(t201, t202, base + 0);
  t203 = ((int64_t)(v1));
  t204 = ((int64_t)(((int64_t)UINT64_C(2))));
  v30 = fg176rs_gg_function_10(t203, t204, base + 0);
  t205 = ((int64_t)(v1));
  t206 = ((int64_t)(((int64_t)UINT64_C(3))));
  v31 = fg176rs_gg_function_10(t205, t206, base + 0);
  t207 = ((int64_t)(v5));
  t208 = ((int64_t)(((int64_t)UINT64_C(8))));
  t209 = fg175s_tc(t208);
  t210 = ((int64_t)(t209));
  t211 = ((t207) == (t210));
  if (t211) {
  {
  v32 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  }
  } else {
  {
  v32 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(8))))));
  }
  }
  t212 = v7;
  if (t212) {
  {
  v33 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(18))))));
  }
  } else {
  {
  v33 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  }
  }
  t213 = v7;
  if (t213) {
  {
  v34 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32))))));
  }
  } else {
  {
  v34 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t214 = ((uint64_t)(v20));
  t215 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t216 = ((t214) == (t215));
  t217 = ((int64_t)(((int64_t)UINT64_C(30))));
  fg176rs_gg_function_3(t216, t217, base + 0);
  t218 = ((uint64_t)(v21));
  t219 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t220 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t218, t219))));
  t221 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t222 = ((t220) == (t221));
  t223 = t222;
  if (t223) {
  t224 = ((uint64_t)(v22));
  t225 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t226 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t224, t225))));
  t227 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t223 = ((t226) == (t227));
  }
  t228 = t223;
  t229 = t228;
  if (t229) {
  t230 = ((uint64_t)(v23));
  t231 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t232 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t230, t231))));
  t233 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t229 = ((t232) == (t233));
  }
  t234 = t229;
  t235 = ((int64_t)(((int64_t)UINT64_C(31))));
  fg176rs_gg_function_3(t234, t235, base + 0);
  t236 = ((uint64_t)(v24));
  t237 = ((uint64_t)(v32));
  t238 = ((t236) == (t237));
  t239 = ((int64_t)(((int64_t)UINT64_C(32))));
  fg176rs_gg_function_3(t238, t239, base + 0);
  t240 = ((uint64_t)(v25));
  t241 = ((uint64_t)(v32));
  t242 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t240, t241))));
  t243 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t244 = ((t242) == (t243));
  t245 = t244;
  if (t245) {
  t246 = ((uint64_t)(v26));
  t247 = ((uint64_t)(v32));
  t248 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t246, t247))));
  t249 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t245 = ((t248) == (t249));
  }
  t250 = t245;
  t251 = t250;
  if (t251) {
  t252 = ((uint64_t)(v27));
  t253 = ((uint64_t)(v32));
  t254 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t252, t253))));
  t255 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t251 = ((t254) == (t255));
  }
  t256 = t251;
  t257 = ((int64_t)(((int64_t)UINT64_C(33))));
  fg176rs_gg_function_3(t256, t257, base + 0);
  t258 = ((uint64_t)(v28));
  t259 = ((uint64_t)(v33));
  t260 = ((t258) == (t259));
  t261 = ((int64_t)(((int64_t)UINT64_C(34))));
  fg176rs_gg_function_3(t260, t261, base + 0);
  t262 = ((uint64_t)(v29));
  t263 = ((uint64_t)(v33));
  t264 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t262, t263))));
  t265 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t266 = ((t264) == (t265));
  t267 = t266;
  if (t267) {
  t268 = ((uint64_t)(v30));
  t269 = ((uint64_t)(v33));
  t270 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t268, t269))));
  t271 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t267 = ((t270) == (t271));
  }
  t272 = t267;
  t273 = t272;
  if (t273) {
  t274 = ((uint64_t)(v31));
  t275 = ((uint64_t)(v33));
  t276 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t274, t275))));
  t277 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t273 = ((t276) == (t277));
  }
  t278 = t273;
  t279 = ((int64_t)(((int64_t)UINT64_C(35))));
  fg176rs_gg_function_3(t278, t279, base + 0);
  t280 = ((int64_t)(v2));
  t281 = ((int64_t)(((int64_t)UINT64_C(1))));
  t282 = ((int64_t)(((int64_t)UINT64_C(0))));
  t283 = fg175s_ti(t280, t281, t282);
  v35 = ((uint64_t)(((uint64_t)(t283))));
  t284 = ((int64_t)(v3));
  t285 = ((int64_t)(((int64_t)UINT64_C(1))));
  t286 = ((int64_t)(((int64_t)UINT64_C(0))));
  t287 = fg175s_ti(t284, t285, t286);
  v36 = ((uint64_t)(((uint64_t)(t287))));
  t288 = ((int64_t)(v1));
  t289 = ((int64_t)(((int64_t)UINT64_C(1))));
  t290 = ((int64_t)(((int64_t)UINT64_C(0))));
  t291 = fg175s_ti(t288, t289, t290);
  v37 = ((uint64_t)(((uint64_t)(t291))));
  t292 = ((uint64_t)(v35));
  t293 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t294 = ((t292) != (t293));
  t295 = t294;
  if (t295) {
  t296 = ((uint64_t)(v36));
  t297 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t295 = ((t296) != (t297));
  }
  t298 = t295;
  t299 = t298;
  if (t299) {
  t300 = ((uint64_t)(v37));
  t301 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t299 = ((t300) != (t301));
  }
  t302 = t299;
  t303 = ((int64_t)(((int64_t)UINT64_C(36))));
  fg176rs_gg_function_3(t302, t303, base + 0);
  t304 = ((uint64_t)(v35));
  t305 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t306 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t304, t305))));
  t307 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t308 = ((t306) == (t307));
  t309 = t308;
  if (t309) {
  t310 = ((uint64_t)(v36));
  t311 = ((uint64_t)(v32));
  t312 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t310, t311))));
  t313 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t309 = ((t312) == (t313));
  }
  t314 = t309;
  t315 = t314;
  if (t315) {
  t316 = ((uint64_t)(v37));
  t318 = v7;
  if (t318) {
  {
  t317 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  }
  } else {
  {
  t317 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  }
  }
  t319 = ((uint64_t)(t317));
  t320 = ((uint64_t)(((uint64_t)fg176rs_gg_umod(t316, t319))));
  t321 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t315 = ((t320) == (t321));
  }
  t322 = t315;
  t323 = ((int64_t)(((int64_t)UINT64_C(53))));
  fg176rs_gg_function_3(t322, t323, base + 0);
  t324 = ((uint64_t)(v21));
  t325 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v38 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t324, t325))));
  t326 = ((uint64_t)(v22));
  t327 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v39 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t326, t327))));
  t328 = ((uint64_t)(v23));
  t329 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v40 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t328, t329))));
  t330 = ((uint64_t)(v24));
  t331 = ((uint64_t)(v32));
  v41 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t330, t331))));
  t332 = ((uint64_t)(v25));
  t333 = ((uint64_t)(v32));
  v42 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t332, t333))));
  t334 = ((uint64_t)(v26));
  t335 = ((uint64_t)(v32));
  v43 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t334, t335))));
  t336 = v7;
  if (t336) {
  {
  v44 = ((uint64_t)(v29));
  }
  } else {
  {
  t337 = ((uint64_t)(v29));
  t338 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v44 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t337, t338))));
  }
  }
  t339 = v7;
  if (t339) {
  {
  v45 = ((uint64_t)(v30));
  }
  } else {
  {
  t340 = ((uint64_t)(v30));
  t341 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v45 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t340, t341))));
  }
  }
  t342 = v7;
  if (t342) {
  {
  v46 = ((uint64_t)(v31));
  }
  } else {
  {
  t343 = ((uint64_t)(v31));
  t344 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v46 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t343, t344))));
  }
  }
  t345 = ((uint64_t)(v8));
  t346 = ((uint64_t)(v9));
  t347 = fg176rs_gg_function_11(t345, t346, base + 0);
  t348 = ((uint64_t)(v10));
  t349 = fg176rs_gg_function_11(t347, t348, base + 0);
  t350 = ((uint64_t)(v11));
  v47 = fg176rs_gg_function_11(t349, t350, base + 0);
  t351 = v7;
  if (t351) {
  {
  t352 = ((uint64_t)(v47));
  t353 = ((uint64_t)(((int64_t)UINT64_C(32))));
  v48 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t352, t353))));
  }
  } else {
  {
  v48 = ((uint64_t)(v47));
  }
  }
  t354 = ((uint64_t)(v48));
  t355 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t356 = ((t354) > (t355));
  t357 = ((int64_t)(((int64_t)UINT64_C(37))));
  fg176rs_gg_function_3(t356, t357, base + 0);
  t358 = v7;
  if (t358) {
  {
  t359 = ((uint64_t)(v48));
  t360 = ((uint64_t)(((int64_t)UINT64_C(134217728))));
  t361 = ((t359) <= (t360));
  t362 = ((int64_t)(((int64_t)UINT64_C(38))));
  fg176rs_gg_function_3(t361, t362, base + 0);
  }
  } else {
  {
  t363 = ((uint64_t)(v48));
  t364 = ((uint64_t)(((int64_t)UINT64_C(4294967296))));
  t365 = ((t363) <= (t364));
  t366 = ((int64_t)(((int64_t)UINT64_C(39))));
  fg176rs_gg_function_3(t365, t366, base + 0);
  }
  }
  t367 = ((uint64_t)(v8));
  t368 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t369 = ((t367) <= (t368));
  t370 = t369;
  if (t370) {
  t371 = ((uint64_t)(v9));
  t372 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t370 = ((t371) <= (t372));
  }
  t373 = t370;
  t374 = t373;
  if (t374) {
  t375 = ((uint64_t)(v10));
  t376 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t374 = ((t375) <= (t376));
  }
  t377 = t374;
  t378 = t377;
  if (t378) {
  t379 = ((uint64_t)(v13));
  t380 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t378 = ((t379) <= (t380));
  }
  t381 = t378;
  t382 = t381;
  if (t382) {
  t383 = ((uint64_t)(v14));
  t384 = ((uint64_t)(((int64_t)UINT64_C(4294967295))));
  t382 = ((t383) <= (t384));
  }
  t385 = t382;
  t386 = ((int64_t)(((int64_t)UINT64_C(40))));
  fg176rs_gg_function_3(t385, t386, base + 0);
  t387 = ((uint64_t)(v48));
  t388 = ((uint64_t)(((int64_t)UINT64_C(256))));
  v49 = fg176rs_gg_function_13(t387, t388, base + 0);
  t389 = ((uint64_t)(v49));
  t390 = ((uint64_t)(((int64_t)UINT64_C(2147483647))));
  t391 = ((t389) <= (t390));
  t392 = ((int64_t)(((int64_t)UINT64_C(41))));
  fg176rs_gg_function_3(t391, t392, base + 0);
  t393 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t394 = ((uint64_t)(v8));
  t395 = fg176rs_gg_function_11(t393, t394, base + 0);
  t396 = ((uint64_t)(v9));
  t397 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t398 = fg176rs_gg_function_14(t396, t397, base + 0);
  t399 = ((uint64_t)(v21));
  t400 = fg176rs_gg_function_11(t398, t399, base + 0);
  t401 = fg176rs_gg_function_15(t395, t400, base + 0);
  t402 = ((uint64_t)(v10));
  t403 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t404 = fg176rs_gg_function_14(t402, t403, base + 0);
  t405 = ((uint64_t)(v22));
  t406 = fg176rs_gg_function_11(t404, t405, base + 0);
  t407 = ((uint64_t)(v11));
  t408 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t409 = fg176rs_gg_function_14(t407, t408, base + 0);
  t410 = ((uint64_t)(v23));
  t411 = fg176rs_gg_function_11(t409, t410, base + 0);
  t412 = fg176rs_gg_function_15(t406, t411, base + 0);
  v50 = fg176rs_gg_function_15(t401, t412, base + 0);
  t413 = ((uint64_t)(v32));
  t414 = ((uint64_t)(v12));
  t415 = fg176rs_gg_function_11(t413, t414, base + 0);
  t416 = ((uint64_t)(v13));
  t417 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t418 = fg176rs_gg_function_14(t416, t417, base + 0);
  t419 = ((uint64_t)(v25));
  t420 = fg176rs_gg_function_11(t418, t419, base + 0);
  t421 = fg176rs_gg_function_15(t415, t420, base + 0);
  t422 = ((uint64_t)(v14));
  t423 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t424 = fg176rs_gg_function_14(t422, t423, base + 0);
  t425 = ((uint64_t)(v26));
  t426 = fg176rs_gg_function_11(t424, t425, base + 0);
  t427 = ((uint64_t)(v15));
  t428 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t429 = fg176rs_gg_function_14(t427, t428, base + 0);
  t430 = ((uint64_t)(v27));
  t431 = fg176rs_gg_function_11(t429, t430, base + 0);
  t432 = fg176rs_gg_function_15(t426, t431, base + 0);
  v51 = fg176rs_gg_function_15(t421, t432, base + 0);
  t433 = v7;
  if (t433) {
  {
  t434 = ((uint64_t)(v16));
  t435 = ((uint64_t)(((int64_t)UINT64_C(32))));
  t436 = ((uint64_t)(((uint64_t)fg176rs_gg_udiv(t434, t435))));
  t437 = ((uint64_t)(((int64_t)UINT64_C(18))));
  v52 = fg176rs_gg_function_11(t436, t437, base + 0);
  }
  } else {
  {
  t438 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t439 = ((uint64_t)(v16));
  v52 = fg176rs_gg_function_11(t438, t439, base + 0);
  }
  }
  t440 = ((uint64_t)(v52));
  t441 = ((uint64_t)(v17));
  t442 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t443 = fg176rs_gg_function_14(t441, t442, base + 0);
  t444 = ((uint64_t)(v29));
  t445 = fg176rs_gg_function_11(t443, t444, base + 0);
  t446 = fg176rs_gg_function_15(t440, t445, base + 0);
  t447 = ((uint64_t)(v18));
  t448 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t449 = fg176rs_gg_function_14(t447, t448, base + 0);
  t450 = ((uint64_t)(v30));
  t451 = fg176rs_gg_function_11(t449, t450, base + 0);
  t452 = ((uint64_t)(v19));
  t453 = ((uint64_t)(((int64_t)UINT64_C(1))));
  t454 = fg176rs_gg_function_14(t452, t453, base + 0);
  t455 = ((uint64_t)(v31));
  t456 = fg176rs_gg_function_11(t454, t455, base + 0);
  t457 = fg176rs_gg_function_15(t451, t456, base + 0);
  v53 = fg176rs_gg_function_15(t446, t457, base + 0);
  t458 = ((uint64_t)(v29));
  t459 = ((uint64_t)(v17));
  t460 = ((uint64_t)(v30));
  t461 = ((uint64_t)(v18));
  t462 = ((uint64_t)(v31));
  t463 = ((uint64_t)(v19));
  t464 = ((uint64_t)(v52));
  fg176rs_gg_function_16(t458, t459, t460, t461, t462, t463, t464, base + 0);
  t465 = ((uint64_t)(v50));
  t466 = ((uint64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t467 = ((t465) <= (t466));
  t468 = t467;
  if (t468) {
  t469 = ((uint64_t)(v51));
  t470 = ((uint64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t468 = ((t469) <= (t470));
  }
  t471 = t468;
  t472 = t471;
  if (t472) {
  t473 = ((uint64_t)(v53));
  t474 = ((uint64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t472 = ((t473) <= (t474));
  }
  t475 = t472;
  t476 = ((int64_t)(((int64_t)UINT64_C(42))));
  fg176rs_gg_function_3(t475, t476, base + 0);
  t477 = ((uint64_t)(v35));
  t478 = ((uint64_t)(v50));
  t479 = fg176rs_gg_function_15(t477, t478, base + 0);
  (void)(t479);
  t480 = ((uint64_t)(v36));
  t481 = ((uint64_t)(v51));
  t482 = fg176rs_gg_function_15(t480, t481, base + 0);
  (void)(t482);
  t483 = ((uint64_t)(v37));
  t484 = ((uint64_t)(v53));
  t485 = fg176rs_gg_function_15(t483, t484, base + 0);
  (void)(t485);
  t486 = ((uint64_t)(v37));
  t487 = ((uint64_t)(v53));
  t488 = ((uint64_t)(v36));
  t489 = ((uint64_t)(v51));
  t490 = fg176rs_gg_function_17(t486, t487, t488, t489, base + 0);
  t491 = t490;
  if (t491) {
  {
  t492 = ((int64_t)(((int64_t)UINT64_C(43))));
  fg175s_fail(t492);
  }
  }
  t493 = ((uint64_t)(v37));
  t494 = ((uint64_t)(v53));
  t495 = ((uint64_t)(v35));
  t496 = ((uint64_t)(v50));
  t497 = fg176rs_gg_function_17(t493, t494, t495, t496, base + 0);
  t498 = t497;
  if (t498) {
  {
  t499 = ((int64_t)(((int64_t)UINT64_C(44))));
  fg175s_fail(t499);
  }
  }
  t500 = ((int64_t)(v1));
  t501 = ((int64_t)(((int64_t)UINT64_C(6))));
  t502 = ((int64_t)(((int64_t)UINT64_C(0))));
  t503 = fg175s_ti(t500, t501, t502);
  v54 = ((uint64_t)(((uint64_t)(t503))));
  t504 = ((int64_t)(v1));
  t505 = ((int64_t)(((int64_t)UINT64_C(7))));
  t506 = ((int64_t)(((int64_t)UINT64_C(0))));
  t507 = fg175s_ti(t504, t505, t506);
  t508 = ((int64_t)(t507));
  v55 = fg176rs_gg_function_9(t508, base + 0);
  t509 = ((uint64_t)(v55));
  t510 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t511 = ((t509) > (t510));
  t512 = t511;
  if (t512) {
  t513 = ((uint64_t)(v54));
  t514 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t512 = ((t513) == (t514));
  }
  t515 = t512;
  if (t515) {
  {
  t516 = ((int64_t)(((int64_t)UINT64_C(45))));
  fg175s_fail(t516);
  }
  }
  t517 = ((int64_t)(v0));
  t518 = fg175s_plan(t517);
  v56 = ((int64_t)(t518));
  t519 = ((int64_t)(v56));
  t520 = ((int64_t)(((int64_t)UINT64_C(0))));
  t521 = ((int64_t)(((int64_t)UINT64_C(0))));
  t522 = ((uint64_t)(v35));
  fg175s_put(t519, t520, t521, t522);
  t523 = ((int64_t)(v56));
  t524 = ((int64_t)(((int64_t)UINT64_C(0))));
  t525 = ((int64_t)(((int64_t)UINT64_C(1))));
  t526 = ((uint64_t)(v36));
  fg175s_put(t523, t524, t525, t526);
  t527 = ((int64_t)(v56));
  t528 = ((int64_t)(((int64_t)UINT64_C(0))));
  t529 = ((int64_t)(((int64_t)UINT64_C(2))));
  t530 = ((uint64_t)(v37));
  fg175s_put(t527, t528, t529, t530);
  t531 = ((int64_t)(v56));
  t532 = ((int64_t)(((int64_t)UINT64_C(0))));
  t533 = ((int64_t)(((int64_t)UINT64_C(3))));
  t534 = ((uint64_t)(v54));
  fg175s_put(t531, t532, t533, t534);
  t535 = ((int64_t)(v56));
  t536 = ((int64_t)(((int64_t)UINT64_C(0))));
  t537 = ((int64_t)(((int64_t)UINT64_C(4))));
  t538 = ((uint64_t)(v55));
  fg175s_put(t535, t536, t537, t538);
  t539 = ((int64_t)(v56));
  t540 = ((int64_t)(((int64_t)UINT64_C(1))));
  t541 = ((int64_t)(((int64_t)UINT64_C(0))));
  t542 = ((uint64_t)(v8));
  fg175s_put(t539, t540, t541, t542);
  t543 = ((int64_t)(v56));
  t544 = ((int64_t)(((int64_t)UINT64_C(1))));
  t545 = ((int64_t)(((int64_t)UINT64_C(1))));
  t546 = ((uint64_t)(v9));
  fg175s_put(t543, t544, t545, t546);
  t547 = ((int64_t)(v56));
  t548 = ((int64_t)(((int64_t)UINT64_C(1))));
  t549 = ((int64_t)(((int64_t)UINT64_C(2))));
  t550 = ((uint64_t)(v10));
  fg175s_put(t547, t548, t549, t550);
  t551 = ((int64_t)(v56));
  t552 = ((int64_t)(((int64_t)UINT64_C(1))));
  t553 = ((int64_t)(((int64_t)UINT64_C(3))));
  t554 = ((uint64_t)(v11));
  fg175s_put(t551, t552, t553, t554);
  t555 = ((int64_t)(v56));
  t556 = ((int64_t)(((int64_t)UINT64_C(2))));
  t557 = ((int64_t)(((int64_t)UINT64_C(0))));
  t558 = ((uint64_t)(v12));
  fg175s_put(t555, t556, t557, t558);
  t559 = ((int64_t)(v56));
  t560 = ((int64_t)(((int64_t)UINT64_C(2))));
  t561 = ((int64_t)(((int64_t)UINT64_C(1))));
  t562 = ((uint64_t)(v13));
  fg175s_put(t559, t560, t561, t562);
  t563 = ((int64_t)(v56));
  t564 = ((int64_t)(((int64_t)UINT64_C(2))));
  t565 = ((int64_t)(((int64_t)UINT64_C(2))));
  t566 = ((uint64_t)(v14));
  fg175s_put(t563, t564, t565, t566);
  t567 = ((int64_t)(v56));
  t568 = ((int64_t)(((int64_t)UINT64_C(2))));
  t569 = ((int64_t)(((int64_t)UINT64_C(3))));
  t570 = ((uint64_t)(v15));
  fg175s_put(t567, t568, t569, t570);
  t571 = ((int64_t)(v56));
  t572 = ((int64_t)(((int64_t)UINT64_C(3))));
  t573 = ((int64_t)(((int64_t)UINT64_C(0))));
  t574 = ((uint64_t)(v16));
  fg175s_put(t571, t572, t573, t574);
  t575 = ((int64_t)(v56));
  t576 = ((int64_t)(((int64_t)UINT64_C(3))));
  t577 = ((int64_t)(((int64_t)UINT64_C(1))));
  t578 = ((uint64_t)(v17));
  fg175s_put(t575, t576, t577, t578);
  t579 = ((int64_t)(v56));
  t580 = ((int64_t)(((int64_t)UINT64_C(3))));
  t581 = ((int64_t)(((int64_t)UINT64_C(2))));
  t582 = ((uint64_t)(v18));
  fg175s_put(t579, t580, t581, t582);
  t583 = ((int64_t)(v56));
  t584 = ((int64_t)(((int64_t)UINT64_C(3))));
  t585 = ((int64_t)(((int64_t)UINT64_C(3))));
  t586 = ((uint64_t)(v19));
  fg175s_put(t583, t584, t585, t586);
  t587 = ((int64_t)(v56));
  t588 = ((int64_t)(((int64_t)UINT64_C(4))));
  t589 = ((int64_t)(((int64_t)UINT64_C(0))));
  t590 = ((uint64_t)(v38));
  fg175s_put(t587, t588, t589, t590);
  t591 = ((int64_t)(v56));
  t592 = ((int64_t)(((int64_t)UINT64_C(4))));
  t593 = ((int64_t)(((int64_t)UINT64_C(1))));
  t594 = ((uint64_t)(v39));
  fg175s_put(t591, t592, t593, t594);
  t595 = ((int64_t)(v56));
  t596 = ((int64_t)(((int64_t)UINT64_C(4))));
  t597 = ((int64_t)(((int64_t)UINT64_C(2))));
  t598 = ((uint64_t)(v40));
  fg175s_put(t595, t596, t597, t598);
  t599 = ((int64_t)(v56));
  t600 = ((int64_t)(((int64_t)UINT64_C(5))));
  t601 = ((int64_t)(((int64_t)UINT64_C(0))));
  t602 = ((uint64_t)(v41));
  fg175s_put(t599, t600, t601, t602);
  t603 = ((int64_t)(v56));
  t604 = ((int64_t)(((int64_t)UINT64_C(5))));
  t605 = ((int64_t)(((int64_t)UINT64_C(1))));
  t606 = ((uint64_t)(v42));
  fg175s_put(t603, t604, t605, t606);
  t607 = ((int64_t)(v56));
  t608 = ((int64_t)(((int64_t)UINT64_C(5))));
  t609 = ((int64_t)(((int64_t)UINT64_C(2))));
  t610 = ((uint64_t)(v43));
  fg175s_put(t607, t608, t609, t610);
  t611 = ((int64_t)(v56));
  t612 = ((int64_t)(((int64_t)UINT64_C(6))));
  t613 = ((int64_t)(((int64_t)UINT64_C(0))));
  t614 = ((uint64_t)(v44));
  fg175s_put(t611, t612, t613, t614);
  t615 = ((int64_t)(v56));
  t616 = ((int64_t)(((int64_t)UINT64_C(6))));
  t617 = ((int64_t)(((int64_t)UINT64_C(1))));
  t618 = ((uint64_t)(v45));
  fg175s_put(t615, t616, t617, t618);
  t619 = ((int64_t)(v56));
  t620 = ((int64_t)(((int64_t)UINT64_C(6))));
  t621 = ((int64_t)(((int64_t)UINT64_C(2))));
  t622 = ((uint64_t)(v46));
  fg175s_put(t619, t620, t621, t622);
  t623 = ((int64_t)(v56));
  t624 = ((int64_t)(((int64_t)UINT64_C(7))));
  t625 = ((int64_t)(((int64_t)UINT64_C(0))));
  t626 = ((uint64_t)(v48));
  fg175s_put(t623, t624, t625, t626);
  t627 = ((int64_t)(v56));
  t628 = ((int64_t)(((int64_t)UINT64_C(7))));
  t629 = ((int64_t)(((int64_t)UINT64_C(1))));
  t630 = ((uint64_t)(v12));
  fg175s_put(t627, t628, t629, t630);
  t631 = ((int64_t)(v56));
  t632 = ((int64_t)(((int64_t)UINT64_C(7))));
  t633 = ((int64_t)(((int64_t)UINT64_C(2))));
  t634 = ((uint64_t)(v13));
  fg175s_put(t631, t632, t633, t634);
  t635 = ((int64_t)(v56));
  t636 = ((int64_t)(((int64_t)UINT64_C(7))));
  t637 = ((int64_t)(((int64_t)UINT64_C(3))));
  t638 = ((uint64_t)(v14));
  fg175s_put(t635, t636, t637, t638);
  t639 = ((int64_t)(v56));
  t640 = ((int64_t)(((int64_t)UINT64_C(7))));
  t641 = ((int64_t)(((int64_t)UINT64_C(4))));
  t642 = ((uint64_t)(v15));
  fg175s_put(t639, t640, t641, t642);
  t643 = ((int64_t)(v56));
  t644 = ((int64_t)(((int64_t)UINT64_C(7))));
  t645 = ((int64_t)(((int64_t)UINT64_C(5))));
  t646 = ((uint64_t)(v49));
  fg175s_put(t643, t644, t645, t646);
  t647 = ((int64_t)(v56));
  t648 = ((int64_t)(((int64_t)UINT64_C(7))));
  t649 = ((int64_t)(((int64_t)UINT64_C(6))));
  t650 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg175s_put(t647, t648, t649, t650);
  t651 = ((int64_t)(v56));
  t652 = ((int64_t)(((int64_t)UINT64_C(7))));
  t653 = ((int64_t)(((int64_t)UINT64_C(7))));
  t654 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg175s_put(t651, t652, t653, t654);
  t655 = ((int64_t)(v56));
  t656 = ((int64_t)(((int64_t)UINT64_C(7))));
  t657 = ((int64_t)(((int64_t)UINT64_C(8))));
  t658 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(256))))));
  fg175s_put(t655, t656, t657, t658);
  t659 = ((int64_t)(v56));
  t660 = ((int64_t)(((int64_t)UINT64_C(7))));
  t661 = ((int64_t)(((int64_t)UINT64_C(9))));
  t662 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg175s_put(t659, t660, t661, t662);
  t663 = v7;
  if (t663) {
  {
  v57 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  v57 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t664 = ((int64_t)(v5));
  t665 = ((int64_t)(((int64_t)UINT64_C(8))));
  t666 = fg175s_tc(t665);
  t667 = ((int64_t)(t666));
  t668 = ((t664) == (t667));
  if (t668) {
  {
  v58 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  v58 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t669 = ((int64_t)(v56));
  t670 = ((int64_t)(((int64_t)UINT64_C(7))));
  t671 = ((int64_t)(((int64_t)UINT64_C(10))));
  t672 = ((uint64_t)(v57));
  fg175s_put(t669, t670, t671, t672);
  t673 = ((int64_t)(v56));
  t674 = ((int64_t)(((int64_t)UINT64_C(7))));
  t675 = ((int64_t)(((int64_t)UINT64_C(11))));
  t676 = ((uint64_t)(v58));
  fg175s_put(t673, t674, t675, t676);
  t677 = ((int64_t)(v56));
  t678 = ((int64_t)(((int64_t)UINT64_C(7))));
  t679 = ((int64_t)(((int64_t)UINT64_C(12))));
  t680 = ((uint64_t)(v57));
  t681 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t682 = ((uint64_t)(((uint64_t)((uint64_t)(t680) * (uint64_t)(t681)))));
  t683 = ((uint64_t)(v58));
  t684 = ((uint64_t)(((uint64_t)((uint64_t)(t682) + (uint64_t)(t683)))));
  fg175s_put(t677, t678, t679, t684);
  t685 = ((int64_t)(v56));
  t686 = ((int64_t)(((int64_t)UINT64_C(7))));
  t687 = ((int64_t)(((int64_t)UINT64_C(13))));
  t688 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg175s_put(t685, t686, t687, t688);
  t689 = ((int64_t)(v56));
  t690 = ((int64_t)(((int64_t)UINT64_C(7))));
  t691 = ((int64_t)(((int64_t)UINT64_C(14))));
  t692 = ((uint64_t)(v17));
  fg175s_put(t689, t690, t691, t692);
  t693 = ((int64_t)(v56));
  t694 = ((int64_t)(((int64_t)UINT64_C(7))));
  t695 = ((int64_t)(((int64_t)UINT64_C(15))));
  t696 = ((uint64_t)(v53));
  fg175s_put(t693, t694, t695, t696);
  t697 = ((int64_t)(v56));
  t698 = ((int64_t)(((int64_t)UINT64_C(7))));
  t699 = ((int64_t)(((int64_t)UINT64_C(16))));
  t700 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg175s_put(t697, t698, t699, t700);
  t701 = ((int64_t)(v56));
  t702 = ((int64_t)(((int64_t)UINT64_C(7))));
  t703 = ((int64_t)(((int64_t)UINT64_C(17))));
  t704 = ((uint64_t)(v34));
  fg175s_put(t701, t702, t703, t704);
  t705 = ((int64_t)(v56));
  t706 = ((int64_t)(((int64_t)UINT64_C(7))));
  t707 = ((int64_t)(((int64_t)UINT64_C(18))));
  t708 = ((uint64_t)(v33));
  fg175s_put(t705, t706, t707, t708);
  t709 = ((int64_t)(v56));
  t710 = ((int64_t)(((int64_t)UINT64_C(7))));
  t711 = ((int64_t)(((int64_t)UINT64_C(19))));
  t712 = ((uint64_t)(v50));
  fg175s_put(t709, t710, t711, t712);
  t713 = ((int64_t)(v56));
  t714 = ((int64_t)(((int64_t)UINT64_C(7))));
  t715 = ((int64_t)(((int64_t)UINT64_C(20))));
  t716 = ((uint64_t)(v51));
  fg175s_put(t713, t714, t715, t716);
  t717 = ((int64_t)(v56));
  t718 = ((int64_t)(((int64_t)UINT64_C(0))));
  t719 = ((uint64_t)(v8));
  fg176rs_gg_function_20(t717, t718, t719, base + 0);
  t720 = ((int64_t)(v56));
  t721 = ((int64_t)(((int64_t)UINT64_C(1))));
  t722 = ((uint64_t)(v9));
  fg176rs_gg_function_20(t720, t721, t722, base + 0);
  t723 = ((int64_t)(v56));
  t724 = ((int64_t)(((int64_t)UINT64_C(2))));
  t725 = ((uint64_t)(v10));
  fg176rs_gg_function_20(t723, t724, t725, base + 0);
  t726 = ((int64_t)(v56));
  t727 = ((int64_t)(((int64_t)UINT64_C(3))));
  t728 = ((uint64_t)(v13));
  fg176rs_gg_function_20(t726, t727, t728, base + 0);
  t729 = ((int64_t)(v56));
  t730 = ((int64_t)(((int64_t)UINT64_C(4))));
  t731 = ((uint64_t)(v14));
  fg176rs_gg_function_20(t729, t730, t731, base + 0);
  t732 = ((int64_t)(v56));
  t733 = ((int64_t)(((int64_t)UINT64_C(9))));
  t734 = ((int64_t)(((int64_t)UINT64_C(0))));
  t735 = ((uint64_t)(v8));
  fg175s_put(t732, t733, t734, t735);
  t736 = ((int64_t)(v56));
  t737 = ((int64_t)(((int64_t)UINT64_C(9))));
  t738 = ((int64_t)(((int64_t)UINT64_C(1))));
  t739 = ((uint64_t)(v9));
  fg175s_put(t736, t737, t738, t739);
  t740 = ((int64_t)(v56));
  t741 = ((int64_t)(((int64_t)UINT64_C(9))));
  t742 = ((int64_t)(((int64_t)UINT64_C(2))));
  t743 = ((uint64_t)(v10));
  fg175s_put(t740, t741, t742, t743);
  t744 = ((int64_t)(v56));
  t745 = ((int64_t)(((int64_t)UINT64_C(9))));
  t746 = ((int64_t)(((int64_t)UINT64_C(3))));
  t747 = ((uint64_t)(v13));
  fg175s_put(t744, t745, t746, t747);
  t748 = ((int64_t)(v56));
  t749 = ((int64_t)(((int64_t)UINT64_C(9))));
  t750 = ((int64_t)(((int64_t)UINT64_C(4))));
  t751 = ((uint64_t)(v14));
  fg175s_put(t748, t749, t750, t751);
  t752 = ((int64_t)(v0));
  t753 = fg175s_launch(t752);
  t754 = ((int64_t)(t753));
  t755 = ((int64_t)(((int64_t)UINT64_C(0))));
  t756 = ((t754) == (t755));
  t757 = ((int64_t)(((int64_t)UINT64_C(90))));
  fg176rs_gg_function_3(t756, t757, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto fg176rs_gg_done;
  }
fg176rs_gg_done:;
  return t0;
}
static void fg176rs_gg_function_0(int64_t base) {
  int64_t t0;
  uint8_t t1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = fg176rs_gg_function_1(t0, base + 0);
  (void)(t1);
  }
fg176rs_gg_done:;
}
static int fg176rs_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg176rs_gg_argc = argc; fg176rs_gg_argv = argv;
  fg176rs_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg176rs_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg176rs_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg176rs_gg_program(argc, argv);
#endif
}
