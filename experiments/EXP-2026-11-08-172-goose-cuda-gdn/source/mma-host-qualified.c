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
typedef struct { void *data; int64_t len; } fg174mh_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg174mh_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg174mh_gg_stack;
typedef struct { fg174mh_gg_header *header; fg174mh_gg_stack *stack; } fg174mh_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg174mh_gg_stack fg174mh_gg_stacks[GG_STACK_COUNT];
static int fg174mh_gg_argc;
static char **fg174mh_gg_argv;
static void fg174mh_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg174mh_gg_stack *fg174mh_gg_get_stack(int64_t id) {
    fg174mh_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg174mh_gg_fail("data stack limit exceeded");
    s = &fg174mh_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg174mh_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg174mh_gg_allocate(fg174mh_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg174mh_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg174mh_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg174mh_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg174mh_gg_reference fg174mh_gg_ref(fg174mh_gg_header *h, fg174mh_gg_stack *s) { fg174mh_gg_reference r; r.header=h; r.stack=s; return r; }
static fg174mh_gg_slice fg174mh_gg_view(void *p, int64_t n) { fg174mh_gg_slice v; v.data=p; v.len=n; return v; }
static fg174mh_gg_slice fg174mh_gg_byte_view(fg174mh_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg174mh_gg_fail("byte view size overflow");
    return fg174mh_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg174mh_gg_slice fg174mh_gg_sub(fg174mh_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg174mh_gg_fail("slice out of bounds");
    return fg174mh_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg174mh_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg174mh_gg_fail("array index out of bounds"); return i; }
static void fg174mh_gg_append(fg174mh_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg174mh_gg_fail("invalid array size");
    to=fg174mh_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg174mh_gg_resize(fg174mh_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg174mh_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg174mh_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg174mh_gg_fail("invalid array size");
        p=fg174mh_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg174mh_gg_div(int64_t a, int64_t b) { if (!b) fg174mh_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg174mh_gg_fail("division overflow"); return a/b; }
static int64_t fg174mh_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg174mh_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg174mh_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg174mh_gg_fail("division by zero");return a/b; }
static uint64_t fg174mh_gg_umod(uint64_t a,uint64_t b) { if(!b)fg174mh_gg_fail("division by zero");return a%b; }
static int fg174mh_gg_equal(fg174mh_gg_slice a, fg174mh_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg174mh_gg_text(fg174mh_gg_reference out, fg174mh_gg_slice v) { fg174mh_gg_append(out,v.data,v.len,1); }
static void fg174mh_gg_integer(fg174mh_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg174mh_gg_append(out,b,k,1); }
static void fg174mh_gg_unsigned(fg174mh_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg174mh_gg_append(out,b,k,1); }
static double fg174mh_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg174mh_ggf_fmt_f64(uint8_t *out,double n);
static int64_t fg174mh_ggf_fmt_f32(uint8_t *out,float n);
static void fg174mh_gg_float(fg174mh_gg_reference out,double n) { uint8_t b[64];int64_t k=fg174mh_ggf_fmt_f64(b,n);fg174mh_gg_append(out,b,k,1); }
static void fg174mh_gg_float32(fg174mh_gg_reference out,float n) { uint8_t b[64];int64_t k=fg174mh_ggf_fmt_f32(b,n);fg174mh_gg_append(out,b,k,1); }
static void fg174mh_gg_bool(fg174mh_gg_reference out,uint8_t n) { fg174mh_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg174mh_gg_write(FILE *file,fg174mh_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg174mh_gg_fail("write failed"); }
static char *fg174mh_gg_cstring(fg174mh_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg174mh_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t fg174mh_gs_os_read_file(fg174mh_gg_slice path,fg174mh_gg_reference out) {
    char *p=fg174mh_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg174mh_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t fg174mh_gs_os_write_file(fg174mh_gg_slice path,fg174mh_gg_slice data) {
    char *p=fg174mh_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t fg174mh_gs_os_file_exists(fg174mh_gg_slice path) { char*p=fg174mh_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void fg174mh_gs_os_write_stdout(fg174mh_gg_slice s) { fg174mh_gg_write(stdout,s); }
static void fg174mh_gs_os_write_stderr(fg174mh_gg_slice s) { fg174mh_gg_write(stderr,s); }
static void fg174mh_gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t fg174mh_gs_os_arg_count(void) { return fg174mh_gg_argc; }
static void fg174mh_gs_os_arg(int64_t i,fg174mh_gg_reference out) { if(i>=0 && i<fg174mh_gg_argc)fg174mh_gg_append(out,fg174mh_gg_argv[i],(int64_t)strlen(fg174mh_gg_argv[i]),1); }
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
typedef struct { int n; uint32_t d[36]; } fg174mh_ggf_big;

/* a = v << sh, for v > 0. */
static void fg174mh_ggf_big_set(fg174mh_ggf_big *a, uint64_t v, int sh) {
    int w = sh >> 5, b = sh & 31, i;
    uint64_t lo = v << b, hi = b ? v >> (64 - b) : 0;
    for (i = 0; i < w; i++) a->d[i] = 0;
    a->d[w] = (uint32_t)lo;
    a->d[w + 1] = (uint32_t)(lo >> 32);
    a->d[w + 2] = (uint32_t)hi;
    a->n = w + 3;
    while (!a->d[a->n - 1]) a->n--;
}

static void fg174mh_ggf_big_mul(fg174mh_ggf_big *a, uint32_t m) {
    uint64_t c = 0;
    for (int i = 0; i < a->n; i++) {
        c += (uint64_t)a->d[i] * m;
        a->d[i] = (uint32_t)c;
        c >>= 32;
    }
    if (c) a->d[a->n++] = (uint32_t)c;
}

static void fg174mh_ggf_big_mul_pow10(fg174mh_ggf_big *a, int k) {
    for (; k >= 9; k -= 9) fg174mh_ggf_big_mul(a, 1000000000u);
    for (; k > 0; k--) fg174mh_ggf_big_mul(a, 10);
}

static int fg174mh_ggf_big_cmp(const fg174mh_ggf_big *a, const fg174mh_ggf_big *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

/* t = a + b */
static void fg174mh_ggf_big_add(fg174mh_ggf_big *t, const fg174mh_ggf_big *a, const fg174mh_ggf_big *b) {
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
static void fg174mh_ggf_big_sub(fg174mh_ggf_big *a, const fg174mh_ggf_big *b) {
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
static int fg174mh_ggf_float_digits(uint64_t f, int e, int bits, int emin, char *dig, int *k) {
    fg174mh_ggf_big r, s, mp, mlo, t;
    /* Reading rounds a tie to the even significand, so the interval that
       reads back as f * 2^e includes its ends when f is even. */
    int even = !(f & 1);
    /* Below a power of two, the gap to the next float down is half the gap up. */
    int asym = f == (uint64_t)1 << (bits - 1) && e > emin;
    int up = e > 0 ? e : 0, lg = e - 1, n = 0, c;
    /* The value is r / s, and what reads back as it reaches from (r - *mm) / s
       to (r + mp) / s: half the gap to each neighbour. */
    fg174mh_ggf_big *mm = asym ? &mlo : &mp;
    fg174mh_ggf_big_set(&r, f, up + 1 + asym);
    fg174mh_ggf_big_set(&s, 1, up - e + 1 + asym);
    fg174mh_ggf_big_set(&mp, 1, up + asym);
    fg174mh_ggf_big_set(&mlo, 1, up);
    /* Scale to 10^*k, the least power of ten above the interval: with
       lg = floor(log2 value), ceil(lg log10 2) is it or one short, and
       78913 / 2^18 is log10 2 closely enough to compute that for |lg| < 1650. */
    for (uint64_t g = f; g; g >>= 1) lg++;
    *k = lg > 0 ? ((lg * 78913) >> 18) + 1 : -((-lg * 78913) >> 18);
    if (*k >= 0) fg174mh_ggf_big_mul_pow10(&s, *k);
    else {
        fg174mh_ggf_big_mul_pow10(&r, -*k);
        fg174mh_ggf_big_mul_pow10(&mp, -*k);
        if (asym) fg174mh_ggf_big_mul_pow10(&mlo, -*k);
    }
    fg174mh_ggf_big_add(&t, &r, &mp);
    c = fg174mh_ggf_big_cmp(&t, &s);
    if (even ? c >= 0 : c > 0) {
        fg174mh_ggf_big_mul(&s, 10);
        ++*k;
    }
    /* Each digit d leaves the remainder r: stop where the digits so far
       followed by d (r within *mm) or by d + 1 (r + mp past s) read back. */
    for (;;) {
        int d = 0, low, high;
        fg174mh_ggf_big_mul(&r, 10);
        fg174mh_ggf_big_mul(&mp, 10);
        if (asym) fg174mh_ggf_big_mul(&mlo, 10);
        while (fg174mh_ggf_big_cmp(&r, &s) >= 0) {
            fg174mh_ggf_big_sub(&r, &s);
            d++;
        }
        c = fg174mh_ggf_big_cmp(&r, mm);
        low = even ? c <= 0 : c < 0;
        fg174mh_ggf_big_add(&t, &r, &mp);
        c = fg174mh_ggf_big_cmp(&t, &s);
        high = even ? c >= 0 : c > 0;
        if (!low && !high) {
            dig[n++] = (char)('0' + d);
            continue;
        }
        if (low && high) {
            /* Both read back: the nearer, d + 1 when r is past half of s,
               and of two as near, the even one. */
            fg174mh_ggf_big_add(&t, &r, &r);
            c = fg174mh_ggf_big_cmp(&t, &s);
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
static int64_t fg174mh_ggf_fmt_float(uint8_t *dst, int neg, uint64_t frac, int bexp, int bits,
                            int emin) {
    char dig[20], *p = (char *)dst;
    int k, n, x, i;
    if (neg) *p++ = '-';
    if (!frac && !bexp) {
        memcpy(p, "0.0", 3);
        return p + 3 - (char *)dst;
    }
    n = fg174mh_ggf_float_digits(bexp ? frac | (uint64_t)1 << (bits - 1) : frac,
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

static int64_t fg174mh_ggf_fmt_f64(uint8_t *dst, double v) {
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
    return fg174mh_ggf_fmt_float(dst, (int)(b >> 63), b & (((uint64_t)1 << 52) - 1),
                        (int)(b >> 52) & 0x7FF, 53, -1074);
}

/* The digits are the f32's own, laid out as an f64's are, so 0.1 as an f32
   prints as 0.1, not as the digits of the f64 it widens to. */
static int64_t fg174mh_ggf_fmt_f32(uint8_t *dst, float v) {
    double d = v;
    uint32_t b;
    if (d != d || isinf(d)) return fg174mh_ggf_fmt_f64(dst, d);
    memcpy(&b, &v, sizeof b);
    return fg174mh_ggf_fmt_float(dst, (int)(b >> 31), b & 0x7FFFFF, (int)(b >> 23) & 0xFF, 24, -149);
}


static void fg174mh_gg_quote(fg174mh_gg_reference out,fg174mh_gg_slice text) { int64_t i; unsigned char c; fg174mh_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg174mh_gg_append(out,"\\",1,1);fg174mh_gg_append(out,&c,1,1);} else if(c=='\n')fg174mh_gg_append(out,"\\n",2,1);else if(c=='\r')fg174mh_gg_append(out,"\\r",2,1);else if(c=='\t')fg174mh_gg_append(out,"\\t",2,1);else fg174mh_gg_append(out,&c,1,1);}fg174mh_gg_append(out,"\"",1,1);}
static uint8_t fg174mh_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg174mh_gg_equal_elements_1(fg174mh_gg_slice a, fg174mh_gg_slice b);
static void fg174mh_gg_render_1(fg174mh_gg_reference out, int64_t value);
static uint8_t fg174mh_gg_eq_2(uint8_t a, uint8_t b);
static uint8_t fg174mh_gg_equal_elements_2(fg174mh_gg_slice a, fg174mh_gg_slice b);
static void fg174mh_gg_render_2(fg174mh_gg_reference out, uint8_t value);
static uint8_t fg174mh_gg_eq_4(uint64_t a, uint64_t b);
static uint8_t fg174mh_gg_equal_elements_4(fg174mh_gg_slice a, fg174mh_gg_slice b);
static void fg174mh_gg_render_4(fg174mh_gg_reference out, uint64_t value);
static uint8_t fg174mh_gg_eq_5(uint8_t a, uint8_t b);
static uint8_t fg174mh_gg_equal_elements_5(fg174mh_gg_slice a, fg174mh_gg_slice b);
static void fg174mh_gg_render_5(fg174mh_gg_reference out, uint8_t value);
static uint8_t fg174mh_gg_equal_elements_1(fg174mh_gg_slice a,fg174mh_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg174mh_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg174mh_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg174mh_gg_render_1(fg174mh_gg_reference out, int64_t value) {
fg174mh_gg_integer(out,value);
}
static uint8_t fg174mh_gg_equal_elements_2(fg174mh_gg_slice a,fg174mh_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg174mh_gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg174mh_gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void fg174mh_gg_render_2(fg174mh_gg_reference out, uint8_t value) {
fg174mh_gg_bool(out,value);
}
static uint8_t fg174mh_gg_equal_elements_4(fg174mh_gg_slice a,fg174mh_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg174mh_gg_eq_4(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg174mh_gg_eq_4(uint64_t a, uint64_t b) {
return a == b;
}
static void fg174mh_gg_render_4(fg174mh_gg_reference out, uint64_t value) {
fg174mh_gg_unsigned(out,value);
}
static uint8_t fg174mh_gg_equal_elements_5(fg174mh_gg_slice a,fg174mh_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg174mh_gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg174mh_gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void fg174mh_gg_render_5(fg174mh_gg_reference out, uint8_t value) {
fg174mh_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg174mh_gg_globals;
static void fg174mh_gg_function_0(int64_t base);
static uint8_t fg174mh_gg_function_1(int64_t p0, int64_t base);
static int64_t fg174mh_gg_function_3(int64_t p0, int64_t p1, int64_t base);
static uint8_t fg174mh_gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static uint64_t fg174mh_gg_function_7(int64_t p0, int64_t base);
static uint64_t fg174mh_gg_function_7(int64_t p0, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(3))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(93312))))));
  }
  } else {
  {
  t0 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(38016))))));
  }
  }
  goto fg174mh_gg_done;
  }
fg174mh_gg_done:;
  return t0;
}
static uint8_t fg174mh_gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  uint8_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
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
  uint8_t t21;
  uint64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  uint8_t v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v2));
  v3 = fg174mh_gg_function_7(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(v2));
  t4 = fg174m_memo_get(t2, t3);
  v4 = ((int64_t)(t4));
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t0 = ((uint8_t)UINT64_C(1));
  goto fg174mh_gg_done;
  }
  }
  t8 = ((int64_t)(v4));
  t9 = ((int64_t)(((int64_t)UINT64_C(1))));
  t10 = ((t8) == (t9));
  if (t10) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg174mh_gg_done;
  }
  }
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(v2));
  fg174m_lock(t11, t12);
  v5 = ((uint8_t)UINT64_C(0));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v2));
  t15 = fg174m_memo_get(t13, t14);
  v6 = ((int64_t)(t15));
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(((int64_t)UINT64_C(2))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  v5 = ((uint8_t)UINT64_C(1));
  }
  } else {
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  v5 = ((uint8_t)UINT64_C(0));
  }
  } else {
  {
  t22 = ((uint64_t)(v3));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(((int64_t)UINT64_C(10))));
  t25 = fg174m_query(t23, t24);
  t26 = ((uint64_t)(((uint64_t)(t25))));
  t27 = ((t22) > (t26));
  if (t27) {
  {
  v5 = ((uint8_t)UINT64_C(0));
  }
  } else {
  {
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(v2));
  t30 = ((uint64_t)(v3));
  t31 = fg174m_init(t28, t29, t30);
  v7 = ((int64_t)(t31));
  t32 = ((int64_t)(v7));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((t32) == (t33));
  if (t34) {
  {
  v5 = ((uint8_t)UINT64_C(1));
  }
  } else {
  {
  fg174m_clear_error();
  v5 = ((uint8_t)UINT64_C(0));
  }
  }
  }
  }
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(v2));
  t38 = v5;
  if (t38) {
  {
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  }
  } else {
  {
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  t39 = ((int64_t)(t37));
  fg174m_memo_set(t35, t36, t39);
  }
  }
  }
  t40 = ((int64_t)(v1));
  t41 = ((int64_t)(v2));
  fg174m_unlock(t40, t41);
  t0 = v5;
  goto fg174mh_gg_done;
  }
fg174mh_gg_done:;
  return t0;
}
static int64_t fg174mh_gg_function_3(int64_t p0, int64_t p1, int64_t base) {
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
  uint8_t t10;
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
  uint8_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
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
  uint8_t t53;
  int64_t t54;
  int64_t t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
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
  uint8_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  uint8_t t83;
  uint8_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
  uint8_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint8_t t95;
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
  uint8_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  uint8_t t122;
  uint8_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  uint8_t t127;
  uint8_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  uint8_t t132;
  uint8_t t133;
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
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) < (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(((int64_t)UINT64_C(11))));
  t8 = fg174m_query(t6, t7);
  t9 = ((int64_t)(t8));
  t4 = ((t5) >= (t9));
  }
  t10 = t4;
  if (t10) {
  {
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg174m_fail(t11);
  }
  }
  t12 = ((int64_t)(v0));
  t13 = ((int64_t)(((int64_t)UINT64_C(1))));
  t14 = fg174m_query(t12, t13);
  v2 = ((int64_t)(t14));
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t17 = fg174m_query(t15, t16);
  v3 = ((int64_t)(t17));
  t18 = ((int64_t)(v0));
  t19 = ((int64_t)(((int64_t)UINT64_C(3))));
  t20 = fg174m_query(t18, t19);
  v4 = ((int64_t)(t20));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)UINT64_C(4))));
  t23 = fg174m_query(t21, t22);
  v5 = ((int64_t)(t23));
  t24 = ((int64_t)(v0));
  t25 = ((int64_t)(((int64_t)UINT64_C(5))));
  t26 = fg174m_query(t24, t25);
  v6 = ((int64_t)(t26));
  t27 = ((int64_t)(v0));
  t28 = ((int64_t)(((int64_t)UINT64_C(6))));
  t29 = fg174m_query(t27, t28);
  t30 = ((int64_t)(t29));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((t30) == (t31));
  if (t32) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg174mh_gg_done;
  }
  }
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(((int64_t)UINT64_C(16))));
  t35 = ((t33) != (t34));
  if (t35) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg174mh_gg_done;
  }
  }
  t36 = ((int64_t)(v2));
  t37 = ((int64_t)(((int64_t)UINT64_C(16))));
  t38 = ((t36) != (t37));
  t39 = t38;
  if (t39) {
  t40 = ((int64_t)(v2));
  t41 = ((int64_t)(((int64_t)UINT64_C(32))));
  t39 = ((t40) != (t41));
  }
  t42 = t39;
  t43 = t42;
  if (t43) {
  t44 = ((int64_t)(v2));
  t45 = ((int64_t)(((int64_t)UINT64_C(48))));
  t43 = ((t44) != (t45));
  }
  t46 = t43;
  t47 = t46;
  if (t47) {
  t48 = ((int64_t)(v2));
  t49 = ((int64_t)(((int64_t)UINT64_C(64))));
  t47 = ((t48) != (t49));
  }
  t50 = t47;
  if (t50) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg174mh_gg_done;
  }
  }
  t51 = ((int64_t)(v4));
  t52 = ((int64_t)(((int64_t)UINT64_C(64))));
  t53 = ((t51) < (t52));
  if (t53) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg174mh_gg_done;
  }
  }
  t54 = ((int64_t)(v4));
  t55 = ((int64_t)(((int64_t)UINT64_C(9223372036854775775))));
  t56 = ((t54) > (t55));
  if (t56) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg174mh_gg_done;
  }
  }
  t57 = ((int64_t)(v6));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((t57) <= (t58));
  if (t59) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg174mh_gg_done;
  }
  }
  t60 = ((int64_t)(v5));
  t61 = ((int64_t)(((int64_t)UINT64_C(0))));
  t62 = ((t60) <= (t61));
  if (t62) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg174mh_gg_done;
  }
  }
  t63 = ((int64_t)(v5));
  t64 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t65 = ((int64_t)(v2));
  t66 = ((int64_t)(((int64_t)UINT64_C(4))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)fg174mh_gg_div(t64, t67))));
  t69 = ((t63) > (t68));
  if (t69) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg174mh_gg_done;
  }
  }
  t70 = ((int64_t)(v0));
  t71 = ((int64_t)(((int64_t)UINT64_C(12))));
  t72 = fg174m_query(t70, t71);
  t73 = ((int64_t)(t72));
  t74 = ((int64_t)(((int64_t)UINT64_C(0))));
  t75 = ((t73) == (t74));
  if (t75) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg174mh_gg_done;
  }
  }
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(((int64_t)UINT64_C(8))));
  t78 = fg174m_query(t76, t77);
  v7 = ((int64_t)(t78));
  t79 = ((int64_t)(v7));
  t80 = ((int64_t)(((int64_t)UINT64_C(0))));
  t81 = fg174m_cc(t80);
  t82 = ((int64_t)(t81));
  t83 = ((t79) != (t82));
  t84 = t83;
  if (t84) {
  t85 = ((int64_t)(v7));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = fg174m_cc(t86);
  t88 = ((int64_t)(t87));
  t84 = ((t85) != (t88));
  }
  t89 = t84;
  t90 = t89;
  if (t90) {
  t91 = ((int64_t)(v7));
  t92 = ((int64_t)(((int64_t)UINT64_C(2))));
  t93 = fg174m_cc(t92);
  t94 = ((int64_t)(t93));
  t90 = ((t91) != (t94));
  }
  t95 = t90;
  t96 = t95;
  if (t96) {
  t97 = ((int64_t)(v7));
  t98 = ((int64_t)(((int64_t)UINT64_C(3))));
  t99 = fg174m_cc(t98);
  t100 = ((int64_t)(t99));
  t96 = ((t97) != (t100));
  }
  t101 = t96;
  if (t101) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  goto fg174mh_gg_done;
  }
  }
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(((int64_t)UINT64_C(9))));
  t104 = fg174m_query(t102, t103);
  v8 = ((int64_t)(t104));
  t105 = ((int64_t)(v2));
  t106 = ((int64_t)(v5));
  t107 = ((int64_t)(((int64_t)((uint64_t)(t105) * (uint64_t)(t106)))));
  t108 = ((int64_t)(((int64_t)UINT64_C(2))));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t107) * (uint64_t)(t108)))));
  t110 = ((int64_t)(v8));
  t111 = ((t109) <= (t110));
  if (t111) {
  {
  t112 = ((int64_t)(v2));
  t113 = ((int64_t)(v5));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t113)))));
  t115 = ((int64_t)(((int64_t)UINT64_C(4))));
  t116 = ((int64_t)(((int64_t)((uint64_t)(t114) * (uint64_t)(t115)))));
  t117 = ((int64_t)(v8));
  t118 = ((t116) <= (t117));
  if (t118) {
  {
  t119 = ((int64_t)(v0));
  t120 = ((int64_t)(v1));
  t121 = ((int64_t)(((int64_t)UINT64_C(1))));
  t122 = fg174mh_gg_function_6(t119, t120, t121, base + 0);
  t123 = t122;
  if (t123) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto fg174mh_gg_done;
  }
  }
  t124 = ((int64_t)(v0));
  t125 = ((int64_t)(v1));
  t126 = ((int64_t)(((int64_t)UINT64_C(2))));
  t127 = fg174mh_gg_function_6(t124, t125, t126, base + 0);
  t128 = t127;
  if (t128) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(2))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto fg174mh_gg_done;
  }
  }
  t129 = ((int64_t)(v0));
  t130 = ((int64_t)(v1));
  t131 = ((int64_t)(((int64_t)UINT64_C(3))));
  t132 = fg174mh_gg_function_6(t129, t130, t131, base + 0);
  t133 = t132;
  if (t133) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(3))));
  }
  } else {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  goto fg174mh_gg_done;
  }
fg174mh_gg_done:;
  return t0;
}
static uint8_t fg174mh_gg_function_1(int64_t p0, int64_t base) {
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
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  uint64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  uint64_t v4;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(7))));
  t3 = fg174m_query(t1, t2);
  t4 = ((int64_t)(t3));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg174mh_gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = fg174m_query(t7, t8);
  v1 = ((int64_t)(t9));
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(v1));
  v2 = fg174mh_gg_function_3(t10, t11, base + 0);
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) == (t13));
  if (t14) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg174mh_gg_done;
  }
  }
  t15 = ((int64_t)(v0));
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  t17 = fg174m_query(t15, t16);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = fg174m_query(t19, t20);
  t22 = ((int64_t)(t21));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t22)))));
  t23 = ((uint64_t)(((uint64_t)(v3))));
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)UINT64_C(3))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t24 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t24 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  }
  }
  t28 = ((uint64_t)(t24));
  v4 = ((uint64_t)(((uint64_t)((uint64_t)(t23) * (uint64_t)(t28)))));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(v2));
  t31 = ((int64_t)(v2));
  t32 = fg174mh_gg_function_7(t31, base + 0);
  t33 = ((uint64_t)(v4));
  t34 = fg174m_launch_raw(t29, t30, t32, t33);
  t35 = ((int64_t)(t34));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t35) != (t36));
  if (t37) {
  {
  t38 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg174m_fail(t38);
  }
  }
  t0 = ((uint8_t)UINT64_C(1));
  goto fg174mh_gg_done;
  }
fg174mh_gg_done:;
  return t0;
}
static void fg174mh_gg_function_0(int64_t base) {
  int64_t t0;
  uint8_t t1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = fg174mh_gg_function_1(t0, base + 0);
  (void)(t1);
  }
fg174mh_gg_done:;
}
static int fg174mh_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg174mh_gg_argc = argc; fg174mh_gg_argv = argv;
  fg174mh_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg174mh_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg174mh_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg174mh_gg_program(argc, argv);
#endif
}
