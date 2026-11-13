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
typedef struct { void *data; int64_t len; } fg175cns_gg_slice;
typedef struct { unsigned char *base; int64_t len; } fg175cns_gg_header;
typedef struct { unsigned char *base; size_t top, committed; } fg175cns_gg_stack;
typedef struct { fg175cns_gg_header *header; fg175cns_gg_stack *stack; } fg175cns_gg_reference;
#define GG_RESERVATION ((size_t)268435456)
#define GG_STACK_COUNT 8192
static fg175cns_gg_stack fg175cns_gg_stacks[GG_STACK_COUNT];
static int fg175cns_gg_argc;
static char **fg175cns_gg_argv;
static void fg175cns_gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\n", message); exit(1);
}
static fg175cns_gg_stack *fg175cns_gg_get_stack(int64_t id) {
    fg175cns_gg_stack *s;
    if (id < 0 || id >= GG_STACK_COUNT) fg175cns_gg_fail("data stack limit exceeded");
    s = &fg175cns_gg_stacks[id];
    if (!s->base) {
#ifdef _WIN32
        s->base = VirtualAlloc(NULL, GG_RESERVATION + 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        s->base = mmap(NULL, GG_RESERVATION + 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (s->base == MAP_FAILED) s->base = NULL;
#endif
        if (!s->base) fg175cns_gg_fail("cannot reserve data stack");
    }
    return s;
}
static void *fg175cns_gg_allocate(fg175cns_gg_stack *s, size_t size) {
    size_t end, committed;
    void *result;
    if (size > GG_RESERVATION - s->top) fg175cns_gg_fail("data stack reservation exceeded");
    result = s->base + s->top; end = s->top + size;
    if (end > s->committed) {
        committed = (end + 65535) & ~(size_t)65535;
#ifdef _WIN32
        if (!VirtualAlloc(s->base + s->committed, committed - s->committed, MEM_COMMIT, PAGE_READWRITE)) fg175cns_gg_fail("cannot commit data stack");
#else
        if (mprotect(s->base + s->committed, committed - s->committed, PROT_READ | PROT_WRITE)) fg175cns_gg_fail("cannot commit data stack");
#endif
        s->committed = committed;
    }
    s->top = end; return result;
}
static fg175cns_gg_reference fg175cns_gg_ref(fg175cns_gg_header *h, fg175cns_gg_stack *s) { fg175cns_gg_reference r; r.header=h; r.stack=s; return r; }
static fg175cns_gg_slice fg175cns_gg_view(void *p, int64_t n) { fg175cns_gg_slice v; v.data=p; v.len=n; return v; }
static fg175cns_gg_slice fg175cns_gg_byte_view(fg175cns_gg_slice a, size_t width) {
    if (a.len < 0 || (width && (uint64_t)a.len > (uint64_t)INT64_MAX / width)) fg175cns_gg_fail("byte view size overflow");
    return fg175cns_gg_view(a.data, (int64_t)((uint64_t)a.len * width));
}
static fg175cns_gg_slice fg175cns_gg_sub(fg175cns_gg_slice a, int64_t lo, int64_t hi, size_t element) {
    if (lo < 0 || hi < lo || hi > a.len) fg175cns_gg_fail("slice out of bounds");
    return fg175cns_gg_view((unsigned char *)a.data + (size_t)lo * element, hi - lo);
}
static int64_t fg175cns_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg175cns_gg_fail("array index out of bounds"); return i; }
static void fg175cns_gg_append(fg175cns_gg_reference a, const void *p, int64_t n, size_t element) {
    void *to;
    if (n < 0 || (uint64_t)n > GG_RESERVATION / (element ? element : 1)) fg175cns_gg_fail("invalid array size");
    to=fg175cns_gg_allocate(a.stack, (size_t)n * element);
    if (n && element) memmove(to, p, (size_t)n * element);
    a.header->len += n;
}
static void fg175cns_gg_resize(fg175cns_gg_reference a, int64_t n, size_t element, const void *fill) {
    int64_t old=a.header->len, i;
    if (n < 0) fg175cns_gg_fail("negative array length");
    if (n > old) {
        unsigned char *p;
        if (!fill) fg175cns_gg_fail("resize without fill may only shrink");
        if ((uint64_t)(n-old) > GG_RESERVATION / (element ? element : 1)) fg175cns_gg_fail("invalid array size");
        p=fg175cns_gg_allocate(a.stack, (size_t)(n-old)*element);
        for (i=0; i<n-old; i++) if(element) memcpy(p+(size_t)i*element, fill, element);
    } else { a.stack->top = (size_t)(a.header->base-a.stack->base) + (size_t)n*element; }
    a.header->len=n;
}
static int64_t fg175cns_gg_div(int64_t a, int64_t b) { if (!b) fg175cns_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg175cns_gg_fail("division overflow"); return a/b; }
static int64_t fg175cns_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg175cns_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg175cns_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg175cns_gg_fail("division by zero");return a/b; }
static uint64_t fg175cns_gg_umod(uint64_t a,uint64_t b) { if(!b)fg175cns_gg_fail("division by zero");return a%b; }
static int fg175cns_gg_equal(fg175cns_gg_slice a, fg175cns_gg_slice b, size_t element) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,(size_t)a.len*element)); }
static void fg175cns_gg_text(fg175cns_gg_reference out, fg175cns_gg_slice v) { fg175cns_gg_append(out,v.data,v.len,1); }
static void fg175cns_gg_integer(fg175cns_gg_reference out,int64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%lld",(long long)n);fg175cns_gg_append(out,b,k,1); }
static void fg175cns_gg_unsigned(fg175cns_gg_reference out,uint64_t n) { char b[32]; int k=snprintf(b,sizeof b,"%llu",(unsigned long long)n);fg175cns_gg_append(out,b,k,1); }
static double fg175cns_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t ggf_fmt_f64(uint8_t *out,double n);
static int64_t ggf_fmt_f32(uint8_t *out,float n);
static void fg175cns_gg_float(fg175cns_gg_reference out,double n) { uint8_t b[64];int64_t k=ggf_fmt_f64(b,n);fg175cns_gg_append(out,b,k,1); }
static void fg175cns_gg_float32(fg175cns_gg_reference out,float n) { uint8_t b[64];int64_t k=ggf_fmt_f32(b,n);fg175cns_gg_append(out,b,k,1); }
static void fg175cns_gg_bool(fg175cns_gg_reference out,uint8_t n) { fg175cns_gg_append(out,n?"true":"false",n?4:5,1); }
static void fg175cns_gg_write(FILE *file,fg175cns_gg_slice text) { if(text.len && fwrite(text.data,1,(size_t)text.len,file)!=(size_t)text.len)fg175cns_gg_fail("write failed"); }
static char *fg175cns_gg_cstring(fg175cns_gg_slice path) {
    char *s=malloc((size_t)path.len+1); if(!s)fg175cns_gg_fail("out of memory in file shim");
    if(path.len)memcpy(s,path.data,(size_t)path.len);s[path.len]=0;return s;
}
static uint8_t gs_os_read_file(fg175cns_gg_slice path,fg175cns_gg_reference out) {
    char *p=fg175cns_gg_cstring(path);FILE *f=fopen(p,"rb");unsigned char b[16384];size_t n;int ok;free(p);
    if(!f)return 0;while((n=fread(b,1,sizeof b,f))!=0)fg175cns_gg_append(out,b,(int64_t)n,1);ok=!ferror(f);fclose(f);return (uint8_t)ok;
}
static uint8_t gs_os_write_file(fg175cns_gg_slice path,fg175cns_gg_slice data) {
    char *p=fg175cns_gg_cstring(path);FILE *f=fopen(p,"wb");int ok;free(p);if(!f)return 0;
    ok=!data.len || fwrite(data.data,1,(size_t)data.len,f)==(size_t)data.len;if(fclose(f))ok=0;return (uint8_t)ok;
}
static uint8_t gs_os_file_exists(fg175cns_gg_slice path) { char*p=fg175cns_gg_cstring(path);FILE*f=fopen(p,"rb");free(p);if(!f)return 0;fclose(f);return 1; }
static void gs_os_write_stdout(fg175cns_gg_slice s) { fg175cns_gg_write(stdout,s); }
static void gs_os_write_stderr(fg175cns_gg_slice s) { fg175cns_gg_write(stderr,s); }
static void gs_os_flush_stdout(void) { fflush(stdout); }
static int64_t gs_os_arg_count(void) { return fg175cns_gg_argc; }
static void gs_os_arg(int64_t i,fg175cns_gg_reference out) { if(i>=0 && i<fg175cns_gg_argc)fg175cns_gg_append(out,fg175cns_gg_argv[i],(int64_t)strlen(fg175cns_gg_argv[i]),1); }
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


static void fg175cns_gg_quote(fg175cns_gg_reference out,fg175cns_gg_slice text) { int64_t i; unsigned char c; fg175cns_gg_append(out,"\"",1,1); for(i=0;i<text.len;i++){c=((unsigned char*)text.data)[i]; if(c=='\"'||c=='\\'){fg175cns_gg_append(out,"\\",1,1);fg175cns_gg_append(out,&c,1,1);} else if(c=='\n')fg175cns_gg_append(out,"\\n",2,1);else if(c=='\r')fg175cns_gg_append(out,"\\r",2,1);else if(c=='\t')fg175cns_gg_append(out,"\\t",2,1);else fg175cns_gg_append(out,&c,1,1);}fg175cns_gg_append(out,"\"",1,1);}
static uint8_t fg175cns_gg_eq_1(int64_t a, int64_t b);
static uint8_t fg175cns_gg_equal_elements_1(fg175cns_gg_slice a, fg175cns_gg_slice b);
static void fg175cns_gg_render_1(fg175cns_gg_reference out, int64_t value);
static uint8_t fg175cns_gg_eq_2(uint8_t a, uint8_t b);
static uint8_t fg175cns_gg_equal_elements_2(fg175cns_gg_slice a, fg175cns_gg_slice b);
static void fg175cns_gg_render_2(fg175cns_gg_reference out, uint8_t value);
static uint8_t fg175cns_gg_eq_4(uint64_t a, uint64_t b);
static uint8_t fg175cns_gg_equal_elements_4(fg175cns_gg_slice a, fg175cns_gg_slice b);
static void fg175cns_gg_render_4(fg175cns_gg_reference out, uint64_t value);
static uint8_t fg175cns_gg_eq_5(uint8_t a, uint8_t b);
static uint8_t fg175cns_gg_equal_elements_5(fg175cns_gg_slice a, fg175cns_gg_slice b);
static void fg175cns_gg_render_5(fg175cns_gg_reference out, uint8_t value);
static uint8_t fg175cns_gg_equal_elements_1(fg175cns_gg_slice a,fg175cns_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg175cns_gg_eq_1(((int64_t*)a.data)[i],((int64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg175cns_gg_eq_1(int64_t a, int64_t b) {
return a == b;
}
static void fg175cns_gg_render_1(fg175cns_gg_reference out, int64_t value) {
fg175cns_gg_integer(out,value);
}
static uint8_t fg175cns_gg_equal_elements_2(fg175cns_gg_slice a,fg175cns_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg175cns_gg_eq_2(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg175cns_gg_eq_2(uint8_t a, uint8_t b) {
return a == b;
}
static void fg175cns_gg_render_2(fg175cns_gg_reference out, uint8_t value) {
fg175cns_gg_bool(out,value);
}
static uint8_t fg175cns_gg_equal_elements_4(fg175cns_gg_slice a,fg175cns_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg175cns_gg_eq_4(((uint64_t*)a.data)[i],((uint64_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg175cns_gg_eq_4(uint64_t a, uint64_t b) {
return a == b;
}
static void fg175cns_gg_render_4(fg175cns_gg_reference out, uint64_t value) {
fg175cns_gg_unsigned(out,value);
}
static uint8_t fg175cns_gg_equal_elements_5(fg175cns_gg_slice a,fg175cns_gg_slice b) {int64_t i; if(a.len!=b.len)return 0;for(i=0;i<a.len;i++)if(!fg175cns_gg_eq_5(((uint8_t*)a.data)[i],((uint8_t*)b.data)[i]))return 0;return 1;}
static uint8_t fg175cns_gg_eq_5(uint8_t a, uint8_t b) {
return a == b;
}
static void fg175cns_gg_render_5(fg175cns_gg_reference out, uint8_t value) {
fg175cns_gg_unsigned(out,value);
}
static struct {
  unsigned char dummy;
} fg175cns_gg_globals;
static void fg175cns_gg_function_0(int64_t base);
static uint8_t fg175cns_gg_function_1(int64_t p0, int64_t base);
static void fg175cns_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static int64_t fg175cns_gg_function_7(int64_t p0, int64_t base);
static int64_t fg175cns_gg_function_12(int64_t p0, int64_t p1, int64_t base);
static int64_t fg175cns_gg_function_14(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t fg175cns_gg_function_15(int64_t p0, int64_t p1, int64_t base);
static void fg175cns_gg_function_16(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static uint64_t fg175cns_gg_function_17(int64_t p0, int64_t base);
static uint64_t fg175cns_gg_function_18(uint64_t p0, uint64_t p1, int64_t base);
static uint64_t fg175cns_gg_function_19(int64_t base);
static uint64_t fg175cns_gg_function_20(uint64_t p0, uint64_t p1, int64_t base);
static void fg175cns_gg_function_23(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t base);
static void fg175cns_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  fg175cn_fail(t2);
  }
  }
  }
fg175cns_gg_done:;
}
static void fg175cns_gg_function_23(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t base) {
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
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
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
  int64_t t48;
  int64_t t49;
  int64_t t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  uint64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  uint64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  uint64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  uint64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  uint64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  uint64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  uint64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  uint64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  uint64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  uint64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  uint64_t t131;
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
  {
  t0 = ((int64_t)(v14));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v15));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) > (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v16));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) > (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v17));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) > (t13));
  }
  t14 = t11;
  t15 = ((int64_t)(((int64_t)UINT64_C(40))));
  fg175cns_gg_function_3(t14, t15, base + 0);
  t16 = ((int64_t)(v15));
  t17 = ((int64_t)(((int64_t)UINT64_C(2147483647))));
  t18 = ((t16) <= (t17));
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v16));
  t21 = ((int64_t)(((int64_t)UINT64_C(65535))));
  t19 = ((t20) <= (t21));
  }
  t22 = t19;
  t23 = t22;
  if (t23) {
  t24 = ((int64_t)(v17));
  t25 = ((int64_t)(((int64_t)UINT64_C(65535))));
  t23 = ((t24) <= (t25));
  }
  t26 = t23;
  t27 = ((int64_t)(((int64_t)UINT64_C(41))));
  fg175cns_gg_function_3(t26, t27, base + 0);
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((uint64_t)(((uint64_t)(v1))));
  fg175cn_put(t28, t29, t30, t31);
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)UINT64_C(1))));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((uint64_t)(((uint64_t)(v2))));
  fg175cn_put(t32, t33, t34, t35);
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)UINT64_C(1))));
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t39 = ((uint64_t)(((uint64_t)(v3))));
  fg175cn_put(t36, t37, t38, t39);
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)UINT64_C(1))));
  t42 = ((int64_t)(((int64_t)UINT64_C(2))));
  t43 = ((uint64_t)(((uint64_t)(v4))));
  fg175cn_put(t40, t41, t42, t43);
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)UINT64_C(1))));
  t46 = ((int64_t)(((int64_t)UINT64_C(3))));
  t47 = ((uint64_t)(((uint64_t)(v5))));
  fg175cn_put(t44, t45, t46, t47);
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)UINT64_C(1))));
  t50 = ((int64_t)(((int64_t)UINT64_C(4))));
  t51 = ((uint64_t)(((uint64_t)(v6))));
  fg175cn_put(t48, t49, t50, t51);
  t52 = ((int64_t)(v0));
  t53 = ((int64_t)(((int64_t)UINT64_C(1))));
  t54 = ((int64_t)(((int64_t)UINT64_C(5))));
  t55 = ((uint64_t)(((uint64_t)(v7))));
  fg175cn_put(t52, t53, t54, t55);
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(((int64_t)UINT64_C(1))));
  t58 = ((int64_t)(((int64_t)UINT64_C(6))));
  t59 = ((uint64_t)(((uint64_t)(v8))));
  fg175cn_put(t56, t57, t58, t59);
  t60 = ((int64_t)(v0));
  t61 = ((int64_t)(((int64_t)UINT64_C(1))));
  t62 = ((int64_t)(((int64_t)UINT64_C(7))));
  t63 = ((uint64_t)(((uint64_t)(v9))));
  fg175cn_put(t60, t61, t62, t63);
  t64 = ((int64_t)(v0));
  t65 = ((int64_t)(((int64_t)UINT64_C(1))));
  t66 = ((int64_t)(((int64_t)UINT64_C(8))));
  t67 = ((uint64_t)(((uint64_t)(v10))));
  fg175cn_put(t64, t65, t66, t67);
  t68 = ((int64_t)(v0));
  t69 = ((int64_t)(((int64_t)UINT64_C(1))));
  t70 = ((int64_t)(((int64_t)UINT64_C(9))));
  t71 = ((uint64_t)(((uint64_t)(v11))));
  fg175cn_put(t68, t69, t70, t71);
  t72 = ((int64_t)(v0));
  t73 = ((int64_t)(((int64_t)UINT64_C(1))));
  t74 = ((int64_t)(((int64_t)UINT64_C(10))));
  t75 = ((uint64_t)(((uint64_t)(v12))));
  fg175cn_put(t72, t73, t74, t75);
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(((int64_t)UINT64_C(1))));
  t78 = ((int64_t)(((int64_t)UINT64_C(11))));
  t79 = ((uint64_t)(((uint64_t)(v13))));
  fg175cn_put(t76, t77, t78, t79);
  t80 = ((int64_t)(v0));
  t81 = ((int64_t)(((int64_t)UINT64_C(2))));
  t82 = ((int64_t)(((int64_t)UINT64_C(0))));
  t83 = ((uint64_t)(((uint64_t)(v14))));
  fg175cn_put(t80, t81, t82, t83);
  t84 = ((int64_t)(v0));
  t85 = ((int64_t)(((int64_t)UINT64_C(2))));
  t86 = ((int64_t)(((int64_t)UINT64_C(1))));
  t87 = ((uint64_t)(((uint64_t)(v15))));
  fg175cn_put(t84, t85, t86, t87);
  t88 = ((int64_t)(v0));
  t89 = ((int64_t)(((int64_t)UINT64_C(2))));
  t90 = ((int64_t)(((int64_t)UINT64_C(2))));
  t91 = ((uint64_t)(((uint64_t)(v16))));
  fg175cn_put(t88, t89, t90, t91);
  t92 = ((int64_t)(v0));
  t93 = ((int64_t)(((int64_t)UINT64_C(2))));
  t94 = ((int64_t)(((int64_t)UINT64_C(3))));
  t95 = ((uint64_t)(((uint64_t)(v17))));
  fg175cn_put(t92, t93, t94, t95);
  t96 = ((int64_t)(v0));
  t97 = ((int64_t)(((int64_t)UINT64_C(2))));
  t98 = ((int64_t)(((int64_t)UINT64_C(4))));
  t99 = ((uint64_t)(((uint64_t)(v18))));
  fg175cn_put(t96, t97, t98, t99);
  t100 = ((int64_t)(v0));
  t101 = ((int64_t)(((int64_t)UINT64_C(2))));
  t102 = ((int64_t)(((int64_t)UINT64_C(5))));
  t103 = ((uint64_t)(((uint64_t)(v19))));
  fg175cn_put(t100, t101, t102, t103);
  t104 = ((int64_t)(v0));
  t105 = ((int64_t)(((int64_t)UINT64_C(2))));
  t106 = ((int64_t)(((int64_t)UINT64_C(6))));
  t107 = ((uint64_t)(((uint64_t)(v20))));
  fg175cn_put(t104, t105, t106, t107);
  t108 = ((int64_t)(v0));
  t109 = ((int64_t)(((int64_t)UINT64_C(2))));
  t110 = ((int64_t)(((int64_t)UINT64_C(7))));
  t111 = ((uint64_t)(((uint64_t)(v21))));
  fg175cn_put(t108, t109, t110, t111);
  t112 = ((int64_t)(v0));
  t113 = ((int64_t)(((int64_t)UINT64_C(3))));
  t114 = ((int64_t)(((int64_t)UINT64_C(0))));
  t115 = ((uint64_t)(((uint64_t)(v22))));
  fg175cn_put(t112, t113, t114, t115);
  t116 = ((int64_t)(v0));
  t117 = ((int64_t)(((int64_t)UINT64_C(4))));
  t118 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((uint64_t)(((uint64_t)(v15))));
  fg175cn_put(t116, t117, t118, t119);
  t120 = ((int64_t)(v0));
  t121 = ((int64_t)(((int64_t)UINT64_C(4))));
  t122 = ((int64_t)(((int64_t)UINT64_C(1))));
  t123 = ((uint64_t)(((uint64_t)(v16))));
  fg175cn_put(t120, t121, t122, t123);
  t124 = ((int64_t)(v0));
  t125 = ((int64_t)(((int64_t)UINT64_C(4))));
  t126 = ((int64_t)(((int64_t)UINT64_C(2))));
  t127 = ((uint64_t)(((uint64_t)(v17))));
  fg175cn_put(t124, t125, t126, t127);
  t128 = ((int64_t)(v0));
  t129 = ((int64_t)(((int64_t)UINT64_C(4))));
  t130 = ((int64_t)(((int64_t)UINT64_C(3))));
  t131 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(64))))));
  fg175cn_put(t128, t129, t130, t131);
  }
fg175cns_gg_done:;
}
static int64_t fg175cns_gg_function_15(int64_t p0, int64_t p1, int64_t base) {
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
  t4 = fg175cn_ti(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto fg175cns_gg_done;
  }
fg175cns_gg_done:;
  return t0;
}
static uint64_t fg175cns_gg_function_17(int64_t p0, int64_t base) {
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
  fg175cns_gg_function_3(t3, t4, base + 0);
  t0 = ((uint64_t)(((uint64_t)(v0))));
  goto fg175cns_gg_done;
  }
fg175cns_gg_done:;
  return t0;
}
static uint64_t fg175cns_gg_function_20(uint64_t p0, uint64_t p1, int64_t base) {
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
  fg175cns_gg_function_3(t3, t4, base + 0);
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(v1));
  t7 = ((uint64_t)(((uint64_t)fg175cns_gg_udiv(t5, t6))));
  t9 = ((uint64_t)(v0));
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(((uint64_t)fg175cns_gg_umod(t9, t10))));
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
  goto fg175cns_gg_done;
  }
fg175cns_gg_done:;
  return t0;
}
static uint64_t fg175cns_gg_function_19(int64_t base) {
  uint64_t t0;
  {
  t0 = ((uint64_t)(((uint64_t)(((uint64_t)UINT64_C(18446744073709551615))))));
  goto fg175cns_gg_done;
  }
fg175cns_gg_done:;
  return t0;
}
static uint64_t fg175cns_gg_function_18(uint64_t p0, uint64_t p1, int64_t base) {
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
  t6 = fg175cns_gg_function_19(base + 0);
  t7 = ((uint64_t)(t6));
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(((uint64_t)fg175cns_gg_udiv(t7, t8))));
  t4 = ((t5) <= (t9));
  }
  t10 = t4;
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  fg175cns_gg_function_3(t10, t11, base + 0);
  t12 = ((uint64_t)(v0));
  t13 = ((uint64_t)(v1));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  goto fg175cns_gg_done;
  }
fg175cns_gg_done:;
  return t0;
}
static void fg175cns_gg_function_16(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  int64_t t14;
  uint64_t t15;
  uint64_t t16;
  int64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint8_t t23;
  int64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  uint64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  uint64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  uint64_t t72;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  uint64_t v9;
  uint64_t v10;
  uint64_t v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  {
  t0 = ((int64_t)(v5));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v6));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) > (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v7));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) > (t9));
  }
  t10 = t7;
  t11 = ((int64_t)(((int64_t)UINT64_C(30))));
  fg175cns_gg_function_3(t10, t11, base + 0);
  t12 = ((int64_t)(v5));
  t13 = fg175cns_gg_function_17(t12, base + 0);
  t14 = ((int64_t)(v6));
  t15 = fg175cns_gg_function_17(t14, base + 0);
  t16 = fg175cns_gg_function_18(t13, t15, base + 0);
  t17 = ((int64_t)(v7));
  t18 = fg175cns_gg_function_17(t17, base + 0);
  v9 = fg175cns_gg_function_18(t16, t18, base + 0);
  v10 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(64))))));
  t19 = ((uint64_t)(v9));
  t20 = ((uint64_t)(v10));
  v11 = fg175cns_gg_function_20(t19, t20, base + 0);
  t21 = ((uint64_t)(v11));
  t22 = ((uint64_t)(((int64_t)UINT64_C(9223372036854775807))));
  t23 = ((t21) <= (t22));
  t24 = ((int64_t)(((int64_t)UINT64_C(31))));
  fg175cns_gg_function_3(t23, t24, base + 0);
  t25 = ((uint64_t)(v11));
  t26 = ((uint64_t)(((int64_t)UINT64_C(2147483647))));
  t27 = ((t25) <= (t26));
  t28 = ((int64_t)(((int64_t)UINT64_C(32))));
  fg175cns_gg_function_3(t27, t28, base + 0);
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int64_t)(((int64_t)UINT64_C(0))));
  t32 = ((uint64_t)(((uint64_t)(v1))));
  fg175cn_put(t29, t30, t31, t32);
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)UINT64_C(0))));
  t35 = ((int64_t)(((int64_t)UINT64_C(1))));
  t36 = ((uint64_t)(v9));
  fg175cn_put(t33, t34, t35, t36);
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)UINT64_C(0))));
  t39 = ((int64_t)(((int64_t)UINT64_C(2))));
  t40 = ((uint64_t)(v10));
  fg175cn_put(t37, t38, t39, t40);
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(((int64_t)UINT64_C(0))));
  t43 = ((int64_t)(((int64_t)UINT64_C(3))));
  t44 = ((uint64_t)(v11));
  fg175cn_put(t41, t42, t43, t44);
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)UINT64_C(1))));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((uint64_t)(((uint64_t)(v2))));
  fg175cn_put(t45, t46, t47, t48);
  t49 = ((int64_t)(v0));
  t50 = ((int64_t)(((int64_t)UINT64_C(1))));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  t52 = ((uint64_t)(((uint64_t)(v3))));
  fg175cn_put(t49, t50, t51, t52);
  t53 = ((int64_t)(v0));
  t54 = ((int64_t)(((int64_t)UINT64_C(1))));
  t55 = ((int64_t)(((int64_t)UINT64_C(2))));
  t56 = ((uint64_t)(((uint64_t)(v4))));
  fg175cn_put(t53, t54, t55, t56);
  t57 = ((int64_t)(v0));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t59 = ((int64_t)(((int64_t)UINT64_C(3))));
  t60 = ((uint64_t)(((uint64_t)(v5))));
  fg175cn_put(t57, t58, t59, t60);
  t61 = ((int64_t)(v0));
  t62 = ((int64_t)(((int64_t)UINT64_C(1))));
  t63 = ((int64_t)(((int64_t)UINT64_C(4))));
  t64 = ((uint64_t)(((uint64_t)(v6))));
  fg175cn_put(t61, t62, t63, t64);
  t65 = ((int64_t)(v0));
  t66 = ((int64_t)(((int64_t)UINT64_C(1))));
  t67 = ((int64_t)(((int64_t)UINT64_C(5))));
  t68 = ((uint64_t)(((uint64_t)(v7))));
  fg175cn_put(t65, t66, t67, t68);
  t69 = ((int64_t)(v0));
  t70 = ((int64_t)(((int64_t)UINT64_C(2))));
  t71 = ((int64_t)(((int64_t)UINT64_C(0))));
  t72 = ((uint64_t)(((uint64_t)(v8))));
  fg175cn_put(t69, t70, t71, t72);
  }
fg175cns_gg_done:;
}
static int64_t fg175cns_gg_function_14(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
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
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
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
  t2 = fg175cn_tblk(t1);
  v3 = ((int64_t)(t2));
  t3 = ((int64_t)(v0));
  t4 = fg175cn_tsz(t3);
  v4 = ((int64_t)(t4));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v4));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((int64_t)(v3));
  v5 = ((int64_t)(((int64_t)fg175cns_gg_div(t7, t8))));
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = ((t9) >= (t10));
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t13) > (t14));
  }
  t15 = t12;
  t16 = ((int64_t)(((int64_t)UINT64_C(53))));
  fg175cns_gg_function_3(t15, t16, base + 0);
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(((int64_t)fg175cns_gg_mod(t17, t18))));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t21 = ((t19) == (t20));
  t22 = ((int64_t)(((int64_t)UINT64_C(54))));
  fg175cns_gg_function_3(t21, t22, base + 0);
  t23 = ((int64_t)(v5));
  t24 = ((int64_t)(v2));
  t0 = ((int64_t)(((int64_t)fg175cns_gg_div(t23, t24))));
  goto fg175cns_gg_done;
  }
fg175cns_gg_done:;
  return t0;
}
static int64_t fg175cns_gg_function_12(int64_t p0, int64_t p1, int64_t base) {
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
  t4 = fg175cn_ti(t1, t2, t3);
  t0 = ((int64_t)(t4));
  goto fg175cns_gg_done;
  }
fg175cns_gg_done:;
  return t0;
}
static int64_t fg175cns_gg_function_7(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  uint8_t t14;
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
  int64_t t26;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = fg175cn_tblk(t1);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(1))));
  t5 = ((t3) > (t4));
  if (t5) {
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(1))));
  }
  } else {
  {
  v1 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t6) != (t7));
  if (t8) {
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(1))));
  goto fg175cns_gg_done;
  }
  }
  t9 = ((int64_t)(v0));
  t10 = fg175cn_tsz(t9);
  v2 = ((int64_t)(t10));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(1))));
  t13 = ((t11) == (t12));
  t14 = t13;
  if (!t14) {
  t15 = ((int64_t)(v2));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((t15) == (t16));
  }
  t17 = t14;
  t18 = t17;
  if (!t18) {
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t18 = ((t19) == (t20));
  }
  t21 = t18;
  t22 = t21;
  if (!t22) {
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(((int64_t)UINT64_C(8))));
  t22 = ((t23) == (t24));
  }
  t25 = t22;
  t26 = ((int64_t)(((int64_t)UINT64_C(23))));
  fg175cns_gg_function_3(t25, t26, base + 0);
  t0 = ((int64_t)(v2));
  goto fg175cns_gg_done;
  }
fg175cns_gg_done:;
  return t0;
}
static uint8_t fg175cns_gg_function_1(int64_t p0, int64_t base) {
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
  uint8_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  uint8_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  uint8_t t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint8_t t69;
  int64_t t70;
  int64_t t71;
  uint8_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  uint8_t t77;
  uint8_t t78;
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
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint8_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
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
  uint8_t t118;
  uint8_t t119;
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
  uint8_t t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
  int64_t t162;
  int64_t t163;
  uint8_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  uint8_t t174;
  uint8_t t175;
  int64_t t176;
  int64_t t177;
  uint8_t t178;
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
  int64_t t194;
  int64_t t195;
  uint8_t t196;
  uint8_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  uint8_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  uint64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  uint64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  uint64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  uint64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  uint64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  uint8_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  uint8_t t250;
  uint8_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  int64_t t255;
  uint8_t t256;
  uint8_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  uint8_t t262;
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
  int64_t t275;
  uint64_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  uint64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  uint64_t t284;
  int64_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  uint8_t t289;
  int64_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  uint8_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  int64_t t316;
  int64_t t317;
  int64_t t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  int64_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  int64_t t339;
  int64_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  int64_t t345;
  int64_t t346;
  uint64_t t347;
  int64_t t348;
  int64_t t349;
  int64_t t350;
  int64_t t351;
  uint8_t t352;
  int64_t t353;
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
  uint64_t v26;
  uint64_t v27;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = fg175cn_arg(t1, t2);
  v1 = ((int64_t)(t3));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) != (t5));
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg175cns_gg_function_3(t6, t7, base + 0);
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(((int64_t)UINT64_C(5))));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t11 = fg175cn_ti(t8, t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = fg175cn_oc(t13);
  t15 = ((int64_t)(t14));
  t16 = ((t12) != (t15));
  if (t16) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg175cns_gg_done;
  }
  }
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = fg175cn_ti(t17, t18, t19);
  v2 = ((int64_t)(t20));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((int64_t)(((int64_t)UINT64_C(1))));
  t24 = fg175cn_ti(t21, t22, t23);
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
  fg175cns_gg_function_3(t31, t32, base + 0);
  t33 = ((int64_t)(v2));
  t34 = ((int64_t)(((int64_t)UINT64_C(2))));
  t35 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = fg175cn_ti(t33, t34, t35);
  v4 = ((int64_t)(t36));
  t37 = ((int64_t)(v3));
  t38 = ((int64_t)(((int64_t)UINT64_C(2))));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = fg175cn_ti(t37, t38, t39);
  v5 = ((int64_t)(t40));
  t41 = ((int64_t)(v1));
  t42 = ((int64_t)(((int64_t)UINT64_C(2))));
  t43 = ((int64_t)(((int64_t)UINT64_C(0))));
  t44 = fg175cn_ti(t41, t42, t43);
  v6 = ((int64_t)(t44));
  t45 = ((int64_t)(v4));
  t46 = ((int64_t)(v5));
  t47 = ((t45) == (t46));
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(v6));
  t50 = ((int64_t)(v4));
  t48 = ((t49) == (t50));
  }
  t51 = t48;
  t52 = (!(t51));
  if (t52) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg175cns_gg_done;
  }
  }
  t53 = ((int64_t)(v1));
  t54 = ((int64_t)(((int64_t)UINT64_C(8))));
  t55 = ((int64_t)(((int64_t)UINT64_C(0))));
  t56 = fg175cn_ti(t53, t54, t55);
  v7 = ((int64_t)(t56));
  t57 = ((int64_t)(v7));
  t58 = ((int64_t)(((int64_t)UINT64_C(0))));
  t59 = ((t57) < (t58));
  t60 = t59;
  if (!t60) {
  t61 = ((int64_t)(v7));
  t62 = ((int64_t)(((int64_t)UINT64_C(3))));
  t60 = ((t61) > (t62));
  }
  t63 = t60;
  if (t63) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg175cns_gg_done;
  }
  }
  t64 = ((int64_t)(v4));
  v8 = fg175cns_gg_function_7(t64, base + 0);
  t65 = ((int64_t)(v4));
  t66 = fg175cn_tblk(t65);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(((int64_t)UINT64_C(1))));
  t69 = ((t67) > (t68));
  if (t69) {
  {
  t70 = ((int64_t)(v7));
  t71 = ((int64_t)(((int64_t)UINT64_C(3))));
  t72 = ((t70) == (t71));
  if (t72) {
  {
  t73 = ((int64_t)(v2));
  t74 = fg175cn_cont3(t73);
  t75 = ((int64_t)(t74));
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  t77 = ((t75) == (t76));
  t78 = t77;
  if (t78) {
  t79 = ((int64_t)(v2));
  t80 = fg175cn_cont(t79);
  t81 = ((int64_t)(t80));
  t82 = ((int64_t)(((int64_t)UINT64_C(1))));
  t78 = ((t81) == (t82));
  }
  t83 = t78;
  t84 = t83;
  if (t84) {
  t85 = ((int64_t)(v3));
  t86 = fg175cn_cont3(t85);
  t87 = ((int64_t)(t86));
  t88 = ((int64_t)(((int64_t)UINT64_C(1))));
  t84 = ((t87) == (t88));
  }
  t89 = t84;
  t90 = t89;
  if (t90) {
  t91 = ((int64_t)(v3));
  t92 = fg175cn_cont(t91);
  t93 = ((int64_t)(t92));
  t94 = ((int64_t)(((int64_t)UINT64_C(1))));
  t90 = ((t93) == (t94));
  }
  t95 = t90;
  t96 = ((int64_t)(((int64_t)UINT64_C(24))));
  fg175cns_gg_function_3(t95, t96, base + 0);
  }
  } else {
  {
  t97 = ((int64_t)(v2));
  t98 = fg175cn_cont3(t97);
  t99 = ((int64_t)(t98));
  t100 = ((int64_t)(((int64_t)UINT64_C(1))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (t102) {
  t103 = ((int64_t)(v3));
  t104 = fg175cn_cont3(t103);
  t105 = ((int64_t)(t104));
  t106 = ((int64_t)(((int64_t)UINT64_C(1))));
  t102 = ((t105) == (t106));
  }
  t107 = t102;
  t108 = ((int64_t)(((int64_t)UINT64_C(25))));
  fg175cns_gg_function_3(t107, t108, base + 0);
  }
  }
  t109 = ((int64_t)(v4));
  t110 = fg175cn_tblk(t109);
  v9 = ((int64_t)(t110));
  t111 = ((int64_t)(v2));
  t112 = ((int64_t)(((int64_t)UINT64_C(0))));
  t113 = fg175cns_gg_function_12(t111, t112, base + 0);
  t114 = ((int64_t)(t113));
  t115 = ((int64_t)(v9));
  t116 = ((int64_t)(((int64_t)fg175cns_gg_mod(t114, t115))));
  t117 = ((int64_t)(((int64_t)UINT64_C(0))));
  t118 = ((t116) == (t117));
  t119 = t118;
  if (t119) {
  t120 = ((int64_t)(v3));
  t121 = ((int64_t)(((int64_t)UINT64_C(0))));
  t122 = fg175cns_gg_function_12(t120, t121, base + 0);
  t123 = ((int64_t)(t122));
  t124 = ((int64_t)(v9));
  t125 = ((int64_t)(((int64_t)fg175cns_gg_mod(t123, t124))));
  t126 = ((int64_t)(((int64_t)UINT64_C(0))));
  t119 = ((t125) == (t126));
  }
  t127 = t119;
  t128 = ((int64_t)(((int64_t)UINT64_C(26))));
  fg175cns_gg_function_3(t127, t128, base + 0);
  }
  }
  t129 = ((int64_t)(v2));
  t130 = ((int64_t)(((int64_t)UINT64_C(0))));
  v10 = fg175cns_gg_function_12(t129, t130, base + 0);
  t131 = ((int64_t)(v2));
  t132 = ((int64_t)(((int64_t)UINT64_C(1))));
  v11 = fg175cns_gg_function_12(t131, t132, base + 0);
  t133 = ((int64_t)(v2));
  t134 = ((int64_t)(((int64_t)UINT64_C(2))));
  v12 = fg175cns_gg_function_12(t133, t134, base + 0);
  t135 = ((int64_t)(v2));
  t136 = ((int64_t)(((int64_t)UINT64_C(3))));
  v13 = fg175cns_gg_function_12(t135, t136, base + 0);
  t137 = ((int64_t)(v1));
  t138 = ((int64_t)(((int64_t)UINT64_C(0))));
  v14 = fg175cns_gg_function_12(t137, t138, base + 0);
  t139 = ((int64_t)(v1));
  t140 = ((int64_t)(((int64_t)UINT64_C(1))));
  v15 = fg175cns_gg_function_12(t139, t140, base + 0);
  t141 = ((int64_t)(v1));
  t142 = ((int64_t)(((int64_t)UINT64_C(2))));
  v16 = fg175cns_gg_function_12(t141, t142, base + 0);
  t143 = ((int64_t)(v1));
  t144 = ((int64_t)(((int64_t)UINT64_C(3))));
  v17 = fg175cns_gg_function_12(t143, t144, base + 0);
  t145 = ((int64_t)(v0));
  t146 = fg175cn_plan(t145);
  v18 = ((int64_t)(t146));
  t147 = ((int64_t)(v2));
  t148 = fg175cn_cont3(t147);
  v19 = ((int64_t)(t148));
  t149 = ((int64_t)(v3));
  t150 = fg175cn_cont3(t149);
  v20 = ((int64_t)(t150));
  t151 = ((int64_t)(v7));
  t152 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t153 = ((t151) != (t152));
  t154 = t153;
  if (t154) {
  t155 = ((int64_t)(v19));
  t156 = ((int64_t)(((int64_t)UINT64_C(1))));
  t154 = ((t155) == (t156));
  }
  t157 = t154;
  t158 = t157;
  if (t158) {
  t159 = ((int64_t)(v20));
  t160 = ((int64_t)(((int64_t)UINT64_C(1))));
  t158 = ((t159) == (t160));
  }
  t161 = t158;
  if (t161) {
  {
  t162 = ((int64_t)(v17));
  t163 = ((int64_t)(((int64_t)UINT64_C(0))));
  t164 = ((t162) > (t163));
  t165 = ((int64_t)(((int64_t)UINT64_C(50))));
  fg175cns_gg_function_3(t164, t165, base + 0);
  t166 = ((int64_t)(v4));
  t167 = ((int64_t)(v10));
  t168 = ((int64_t)(v8));
  v21 = fg175cns_gg_function_14(t166, t167, t168, base + 0);
  t169 = ((int64_t)(v6));
  t170 = ((int64_t)(v14));
  t171 = ((int64_t)(v8));
  v22 = fg175cns_gg_function_14(t169, t170, t171, base + 0);
  t172 = ((int64_t)(v21));
  t173 = ((int64_t)(((int64_t)UINT64_C(0))));
  t174 = ((t172) > (t173));
  t175 = t174;
  if (t175) {
  t176 = ((int64_t)(v22));
  t177 = ((int64_t)(((int64_t)UINT64_C(0))));
  t175 = ((t176) > (t177));
  }
  t178 = t175;
  t179 = ((int64_t)(((int64_t)UINT64_C(51))));
  fg175cns_gg_function_3(t178, t179, base + 0);
  t180 = ((int64_t)(v2));
  t181 = ((int64_t)(((int64_t)UINT64_C(3))));
  v23 = fg175cns_gg_function_15(t180, t181, base + 0);
  t182 = ((int64_t)(v3));
  t183 = ((int64_t)(((int64_t)UINT64_C(3))));
  v24 = fg175cns_gg_function_15(t182, t183, base + 0);
  t184 = ((int64_t)(v1));
  t185 = ((int64_t)(((int64_t)UINT64_C(3))));
  v25 = fg175cns_gg_function_15(t184, t185, base + 0);
  t186 = ((int64_t)(v23));
  t187 = ((int64_t)(((int64_t)(v8))));
  t188 = ((int64_t)(((int64_t)fg175cns_gg_mod(t186, t187))));
  t189 = ((int64_t)(((int64_t)UINT64_C(0))));
  t190 = ((t188) == (t189));
  t191 = t190;
  if (t191) {
  t192 = ((int64_t)(v24));
  t193 = ((int64_t)(((int64_t)(v8))));
  t194 = ((int64_t)(((int64_t)fg175cns_gg_mod(t192, t193))));
  t195 = ((int64_t)(((int64_t)UINT64_C(0))));
  t191 = ((t194) == (t195));
  }
  t196 = t191;
  t197 = t196;
  if (t197) {
  t198 = ((int64_t)(v25));
  t199 = ((int64_t)(((int64_t)(v8))));
  t200 = ((int64_t)(((int64_t)fg175cns_gg_mod(t198, t199))));
  t201 = ((int64_t)(((int64_t)UINT64_C(0))));
  t197 = ((t200) == (t201));
  }
  t202 = t197;
  t203 = ((int64_t)(((int64_t)UINT64_C(52))));
  fg175cns_gg_function_3(t202, t203, base + 0);
  t204 = ((int64_t)(v18));
  t205 = ((int64_t)(v8));
  t206 = ((int64_t)(v21));
  t207 = ((int64_t)(v11));
  t208 = ((int64_t)(v12));
  t209 = ((int64_t)(v22));
  t210 = ((int64_t)(v15));
  t211 = ((int64_t)(v16));
  t212 = ((int64_t)(v7));
  fg175cns_gg_function_16(t204, t205, t206, t207, t208, t209, t210, t211, t212, base + 0);
  t213 = ((int64_t)(v18));
  t214 = ((int64_t)(((int64_t)UINT64_C(0))));
  t215 = ((int64_t)(((int64_t)UINT64_C(4))));
  t216 = ((uint64_t)(((uint64_t)(v17))));
  fg175cn_put(t213, t214, t215, t216);
  t217 = ((int64_t)(v18));
  t218 = ((int64_t)(((int64_t)UINT64_C(0))));
  t219 = ((int64_t)(((int64_t)UINT64_C(5))));
  t220 = ((int64_t)(v2));
  t221 = ((int64_t)(((int64_t)UINT64_C(3))));
  t222 = fg175cns_gg_function_15(t220, t221, base + 0);
  t223 = ((uint64_t)(((uint64_t)(t222))));
  fg175cn_put(t217, t218, t219, t223);
  t224 = ((int64_t)(v18));
  t225 = ((int64_t)(((int64_t)UINT64_C(0))));
  t226 = ((int64_t)(((int64_t)UINT64_C(6))));
  t227 = ((int64_t)(v3));
  t228 = ((int64_t)(((int64_t)UINT64_C(3))));
  t229 = fg175cns_gg_function_15(t227, t228, base + 0);
  t230 = ((uint64_t)(((uint64_t)(t229))));
  fg175cn_put(t224, t225, t226, t230);
  t231 = ((int64_t)(v18));
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((int64_t)(((int64_t)UINT64_C(7))));
  t234 = ((int64_t)(v1));
  t235 = ((int64_t)(((int64_t)UINT64_C(3))));
  t236 = fg175cns_gg_function_15(t234, t235, base + 0);
  t237 = ((uint64_t)(((uint64_t)(t236))));
  fg175cn_put(t231, t232, t233, t237);
  t238 = ((int64_t)(v18));
  t239 = ((int64_t)(((int64_t)UINT64_C(5))));
  t240 = ((int64_t)(((int64_t)UINT64_C(0))));
  t241 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(1))))));
  fg175cn_put(t238, t239, t240, t241);
  t242 = ((int64_t)(v0));
  t243 = fg175cn_launch(t242);
  t244 = ((int64_t)(t243));
  t245 = ((int64_t)(((int64_t)UINT64_C(0))));
  t246 = ((t244) == (t245));
  t247 = ((int64_t)(((int64_t)UINT64_C(90))));
  fg175cns_gg_function_3(t246, t247, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto fg175cns_gg_done;
  }
  }
  t248 = ((int64_t)(v7));
  t249 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t250 = ((t248) == (t249));
  t251 = t250;
  if (t251) {
  t252 = ((int64_t)(v2));
  t253 = fg175cn_cont(t252);
  t254 = ((int64_t)(t253));
  t255 = ((int64_t)(((int64_t)UINT64_C(1))));
  t251 = ((t254) == (t255));
  }
  t256 = t251;
  t257 = t256;
  if (t257) {
  t258 = ((int64_t)(v3));
  t259 = fg175cn_cont(t258);
  t260 = ((int64_t)(t259));
  t261 = ((int64_t)(((int64_t)UINT64_C(1))));
  t257 = ((t260) == (t261));
  }
  t262 = t257;
  if (t262) {
  {
  t263 = ((int64_t)(v2));
  t264 = ((int64_t)(((int64_t)UINT64_C(9))));
  t265 = ((int64_t)(((int64_t)UINT64_C(0))));
  t266 = fg175cn_ti(t263, t264, t265);
  t267 = ((int64_t)(t266));
  v26 = fg175cns_gg_function_17(t267, base + 0);
  t268 = ((int64_t)(v3));
  t269 = ((int64_t)(((int64_t)UINT64_C(9))));
  t270 = ((int64_t)(((int64_t)UINT64_C(0))));
  t271 = fg175cn_ti(t268, t269, t270);
  t272 = ((int64_t)(t271));
  v27 = fg175cns_gg_function_17(t272, base + 0);
  t273 = ((int64_t)(v18));
  t274 = ((int64_t)(((int64_t)UINT64_C(5))));
  t275 = ((int64_t)(((int64_t)UINT64_C(0))));
  t276 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  fg175cn_put(t273, t274, t275, t276);
  t277 = ((int64_t)(v18));
  t278 = ((int64_t)(((int64_t)UINT64_C(7))));
  t279 = ((int64_t)(((int64_t)UINT64_C(0))));
  t280 = ((uint64_t)(v26));
  fg175cn_put(t277, t278, t279, t280);
  t281 = ((int64_t)(v18));
  t282 = ((int64_t)(((int64_t)UINT64_C(7))));
  t283 = ((int64_t)(((int64_t)UINT64_C(1))));
  t284 = ((uint64_t)(v27));
  fg175cn_put(t281, t282, t283, t284);
  t285 = ((int64_t)(v0));
  t286 = fg175cn_launch(t285);
  t287 = ((int64_t)(t286));
  t288 = ((int64_t)(((int64_t)UINT64_C(0))));
  t289 = ((t287) == (t288));
  t290 = ((int64_t)(((int64_t)UINT64_C(91))));
  fg175cns_gg_function_3(t289, t290, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto fg175cns_gg_done;
  }
  }
  t291 = ((int64_t)(v4));
  t292 = fg175cn_tblk(t291);
  t293 = ((int64_t)(t292));
  t294 = ((int64_t)(((int64_t)UINT64_C(1))));
  t295 = ((t293) <= (t294));
  t296 = ((int64_t)(((int64_t)UINT64_C(60))));
  fg175cns_gg_function_3(t295, t296, base + 0);
  t297 = ((int64_t)(v18));
  t298 = ((int64_t)(v8));
  t299 = ((int64_t)(v10));
  t300 = ((int64_t)(v11));
  t301 = ((int64_t)(v12));
  t302 = ((int64_t)(v13));
  t303 = ((int64_t)(v2));
  t304 = ((int64_t)(((int64_t)UINT64_C(0))));
  t305 = fg175cns_gg_function_15(t303, t304, base + 0);
  t306 = ((int64_t)(v2));
  t307 = ((int64_t)(((int64_t)UINT64_C(1))));
  t308 = fg175cns_gg_function_15(t306, t307, base + 0);
  t309 = ((int64_t)(v2));
  t310 = ((int64_t)(((int64_t)UINT64_C(2))));
  t311 = fg175cns_gg_function_15(t309, t310, base + 0);
  t312 = ((int64_t)(v2));
  t313 = ((int64_t)(((int64_t)UINT64_C(3))));
  t314 = fg175cns_gg_function_15(t312, t313, base + 0);
  t315 = ((int64_t)(v3));
  t316 = ((int64_t)(((int64_t)UINT64_C(0))));
  t317 = fg175cns_gg_function_15(t315, t316, base + 0);
  t318 = ((int64_t)(v3));
  t319 = ((int64_t)(((int64_t)UINT64_C(1))));
  t320 = fg175cns_gg_function_15(t318, t319, base + 0);
  t321 = ((int64_t)(v3));
  t322 = ((int64_t)(((int64_t)UINT64_C(2))));
  t323 = fg175cns_gg_function_15(t321, t322, base + 0);
  t324 = ((int64_t)(v3));
  t325 = ((int64_t)(((int64_t)UINT64_C(3))));
  t326 = fg175cns_gg_function_15(t324, t325, base + 0);
  t327 = ((int64_t)(v14));
  t328 = ((int64_t)(v15));
  t329 = ((int64_t)(v16));
  t330 = ((int64_t)(v17));
  t331 = ((int64_t)(v1));
  t332 = ((int64_t)(((int64_t)UINT64_C(0))));
  t333 = fg175cns_gg_function_15(t331, t332, base + 0);
  t334 = ((int64_t)(v1));
  t335 = ((int64_t)(((int64_t)UINT64_C(1))));
  t336 = fg175cns_gg_function_15(t334, t335, base + 0);
  t337 = ((int64_t)(v1));
  t338 = ((int64_t)(((int64_t)UINT64_C(2))));
  t339 = fg175cns_gg_function_15(t337, t338, base + 0);
  t340 = ((int64_t)(v1));
  t341 = ((int64_t)(((int64_t)UINT64_C(3))));
  t342 = fg175cns_gg_function_15(t340, t341, base + 0);
  t343 = ((int64_t)(v7));
  fg175cns_gg_function_23(t297, t298, t299, t300, t301, t302, t305, t308, t311, t314, t317, t320, t323, t326, t327, t328, t329, t330, t333, t336, t339, t342, t343, base + 0);
  t344 = ((int64_t)(v18));
  t345 = ((int64_t)(((int64_t)UINT64_C(5))));
  t346 = ((int64_t)(((int64_t)UINT64_C(0))));
  t347 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  fg175cn_put(t344, t345, t346, t347);
  t348 = ((int64_t)(v0));
  t349 = fg175cn_launch(t348);
  t350 = ((int64_t)(t349));
  t351 = ((int64_t)(((int64_t)UINT64_C(0))));
  t352 = ((t350) == (t351));
  t353 = ((int64_t)(((int64_t)UINT64_C(92))));
  fg175cns_gg_function_3(t352, t353, base + 0);
  t0 = ((uint8_t)UINT64_C(1));
  goto fg175cns_gg_done;
  }
fg175cns_gg_done:;
  return t0;
}
static void fg175cns_gg_function_0(int64_t base) {
  int64_t t0;
  uint8_t t1;
  {
  t0 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = fg175cns_gg_function_1(t0, base + 0);
  (void)(t1);
  }
fg175cns_gg_done:;
}
static int fg175cns_gg_program(int argc, char **argv) {
  struct { int64_t base; } frame;
  frame.base = 0;
  fg175cns_gg_argc = argc; fg175cns_gg_argv = argv;
  fg175cns_gg_function_0(frame.base + 0);
  return 0;
}
int main(int argc, char **argv) {
#ifdef _MSC_VER
  __try { return fg175cns_gg_program(argc, argv); }
  __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    _resetstkoflw(); fg175cns_gg_fail("native call stack overflow"); return 1;
  }
#else
  return fg175cns_gg_program(argc, argv);
#endif
}
